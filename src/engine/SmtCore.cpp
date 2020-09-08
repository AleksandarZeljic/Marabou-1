/*********************                                                        */
/*! \file SmtCore.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Parth Shah, Duligur Ibeling
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#include "Debug.h"
#include "DivideStrategy.h"
#include "EngineState.h"
#include "FloatUtils.h"
#include "GlobalConfiguration.h"
#include "IEngine.h"
#include "MStringf.h"
#include "MarabouError.h"
#include "ReluConstraint.h"
#include "SmtCore.h"

using namespace CVC4::context;

SmtCore::SmtCore( IEngine *engine, Context &ctx )
    : _statistics( NULL )
    , _context( ctx )
    , _trail( &_context )
    , _decisions( &_context)
    , _engine( engine )
    , _needToSplit( false )
    , _constraintForSplitting( NULL )
    , _stateId( 0 )
    , _constraintViolationThreshold
      ( GlobalConfiguration::CONSTRAINT_VIOLATION_THRESHOLD )
{
}

SmtCore::~SmtCore()
{
    //_context.popto( 0 );
    freeMemory();
}

void SmtCore::freeMemory()
{

    for ( const auto &stackEntry : _stack )
    {
        delete stackEntry->_engineState;
        delete stackEntry;
    }

    _stack.clear();
}

void SmtCore::reportViolatedConstraint( PiecewiseLinearConstraint *constraint )
{
    ASSERT( !constraint->phaseFixed() );

    if ( !_constraintToViolationCount.exists( constraint ) )
        _constraintToViolationCount[constraint] = 0;

    ++_constraintToViolationCount[constraint];

    if ( _constraintToViolationCount[constraint] >=
         _constraintViolationThreshold )
    {
        _needToSplit = true;
        if ( GlobalConfiguration::SPLITTING_HEURISTICS ==
             DivideStrategy::ReLUViolation || !pickSplitPLConstraint() )
            // If pickSplitConstraint failed to pick one, use the native
            // relu-violation based splitting heuristic.
            _constraintForSplitting = constraint;
        ASSERT( !_constraintForSplitting->phaseFixed() );
    }
}

unsigned SmtCore::getViolationCounts( PiecewiseLinearConstraint *constraint ) const
{
    if ( !_constraintToViolationCount.exists( constraint ) )
        return 0;

    return _constraintToViolationCount[constraint];
}

bool SmtCore::needToSplit() const
{
    return _needToSplit;
}

void SmtCore::pushDecision( PiecewiseLinearConstraint *constraint,  unsigned decision, List<unsigned> &alternativeSplits )
{
    ASSERT ( (int)( _decisions.size() ) == (int)( _context.getLevel() ) );
    SMT_LOG( "New decision level ..." );

    _context.push();

    TrailEntry te( constraint, decision, alternativeSplits );
    _trail.push_back(te);

    _decisions.push_back( &_trail.back() );

    _engine->applySplit( constraint->getCaseSplit( decision ) );

    SMT_LOG( "Decision push @ %d DONE", _context.getLevel() );
    ASSERT ( (int)( _decisions.size() ) == (int)( _context.getLevel() ) );
    ASSERT( getDecisionLevel() == _decisions.size() )
}

void SmtCore::pushImplication( PiecewiseLinearConstraint *constraint, unsigned phase )
{
    SMT_LOG( "Push implication on trail @s%d ... ", _context.getLevel() );

    TrailEntry te( constraint, phase );

    _trail.push_back(te);

    _engine->applySplit( constraint->getCaseSplit( phase ) );

    SMT_LOG( "Push implication on trail DONE"  );
}

void SmtCore::decide()
{
    ASSERT( _needToSplit );
    SMT_LOG( "Performing a ReLU split" );

    // Maybe the constraint has already become inactive - if so, ignore
    // TODO: Ideally we will not ever reach this point
    // This should be a list of all constraints above the threshold
    // If one is no longer active, we should proceed with the next one
    if ( !_constraintForSplitting->isActive() )
        {
            _needToSplit = false;
            _constraintToViolationCount[_constraintForSplitting] = 0;
            _constraintForSplitting = nullptr;
            return;
    }

    ASSERT( _constraintForSplitting->isActive() );
    _needToSplit = false;
    _constraintForSplitting->setActiveConstraint( false );

    List<unsigned> cases = _constraintForSplitting->getAllCases();
    decideSplit( _constraintForSplitting, cases );
}

void SmtCore::decideSplit( PiecewiseLinearConstraint * constraint, List<unsigned> &cases )
{
    struct timespec start = TimeUtils::sampleMicro();

    if ( _statistics )
    {
        _statistics->incNumSplits();
        _statistics->incNumVisitedTreeStates();
    }

    ASSERT( !cases.empty() );
    ASSERT( cases.size() >= 2 ); 

    // TODO: DecisionMakingLogic
    unsigned decision = cases.front();
    cases.erase( decision );

    pushDecision( constraint, decision, cases );

    if ( _statistics )
    {
        _statistics->setCurrentDecisionLevel( _context.getLevel() );
        struct timespec end = TimeUtils::sampleMicro();
        _statistics->addTimeSmtCore( TimeUtils::timePassed( start, end ) );
    }
    SMT_LOG( "Performing a ReLU split - DONE");
}


// bool SmtCore::checkStackTrailEquivalence()
// {
//     std::cout << "Checking STEQ ... ";
//     bool result = true;
//     // Trail post-condition: TRAIL - STACK equivalence
//     // How are things present on the stack?
//     List<PiecewiseLinearCaseSplit> stackCaseSplits;
//     allSplitsSoFar( stackCaseSplits );

//     std::cout << "Collected all splits on stack" <<std::endl;
//     List<PiecewiseLinearCaseSplit> trailCaseSplits;
//     std::cout << "Trail size: " << _trail.size() <<std::endl;
//     for ( TrailEntry trailEntry : _trail )
//         trailCaseSplits.append( trailEntry.getPiecewiseLinearCaseSplit() );

//     std::cout << "Collected case splits: " << trailCaseSplits.size() <<std::endl;
//     std::cout << "Collected all splits on trail" <<std::endl;
//    // Equivalence check, assumes the order of stack and trail is identical
//     result = result && ( trailCaseSplits.size() == stackCaseSplits.size() );

//     if ( !result )
//     {
//         std::cout << "ASSERTION VIOLATION: ";
//         std::cout << "Trail ( " << trailCaseSplits.size();
//         std::cout << ")and Stack (" << stackCaseSplits.size();
//         std::cout << ") have different number of elements!" << std::endl;

//         std::cout << "Trail:" << std::endl;
//         for ( auto split : trailCaseSplits )
//             split.dump();

//         std::cout << "Stack:" << std::endl;
//         for ( auto split : stackCaseSplits )
//             split.dump();

//     }

//     std::cout << "Size matches!" <<std::endl;
//     PiecewiseLinearCaseSplit tCaseSplit, sCaseSplit;
//     int ind = trailCaseSplits.size();
//     while ( result && !stackCaseSplits.empty() )
//     {
//         tCaseSplit = trailCaseSplits.back();
//         sCaseSplit = stackCaseSplits.back();
//         result = result && ( tCaseSplit == sCaseSplit );
//         if ( !result )
//         {
//             std::cout << "FAILED at position " << ind << "!!!!" <<std::endl;
//             std::cout << "Trail case split: ";
//             tCaseSplit.dump();

//             auto sloc = find_if( stackCaseSplits.begin(),
//                                 stackCaseSplits.end(),
//                                 [&](PiecewiseLinearCaseSplit c) { return c == tCaseSplit; } );

//             if ( stackCaseSplits.end() == sloc )
//                 std::cout << "Missing from the stack!" << std::endl;

//             std::cout << "Stack case split: ";
//             sCaseSplit.dump();
//             auto tloc = find_if( trailCaseSplits.begin(),
//                                 trailCaseSplits.end(),
//                                 [&](PiecewiseLinearCaseSplit c) { return c == sCaseSplit; } );

//             if ( trailCaseSplits.end() == tloc )
//                 std::cout << "Missing from the trail!" << std::endl;

//         }

//         --ind;

//         trailCaseSplits.popBack();
//         stackCaseSplits.popBack();
//     }

//     if ( result )
//         std::cout << "OK" << std::endl;
//     std::cout << "--------------------------------------------------------" << std::endl;
//     return result;
// }



// bool SmtCore::popSplit()
// {
//     if ( _stack.empty() )
//         return false;

//     if ( _statistics )
//         _statistics->incNumPops();

//     delete _stack.back()->_engineState;
//     delete _stack.back();
//     _stack.popBack();
//     return true;
// }

// bool SmtCore::popSplitFromStack( List<PiecewiseLinearCaseSplit> &alternativeSplits )
// {
//     alternativeSplits.clear();
//     alternativeSplits.assign( _stack.back()->_alternativeSplits.begin(), _stack.back()->_alternativeSplits.end() );

//     return popSplit();
// }

inline unsigned SmtCore::getDecisionLevel() const
{
    ASSERT ( (int)( _decisions.size() ) == (int)( _context.getLevel() ) );
    return _context.getLevel();
}

//TODO _decision bookkeeping
bool SmtCore::popDecisionLevel( TrailEntry *lastDecision )
{
    if ( _decisions.empty() )
        return false;

    SMT_LOG( "Backtracking context ..." );

    *lastDecision = *_decisions.back();
    _context.pop();
    SMT_LOG( "Backtracking context - %d DONE", _context.getLevel() );
    return true;
}

void SmtCore::interruptIfCompliantWithDebugSolution()
{
    if ( checkSkewFromDebuggingSolution() )
    {
        SMT_LOG( "Error! Popping from a compliant stack\n" );
        throw MarabouError( MarabouError::DEBUGGING_ERROR );
    }
}


PiecewiseLinearCaseSplit SmtCore::getDecision( unsigned decisionLevel )
{
    ASSERT( decisionLevel <= getDecisionLevel() );
    ASSERT( decisionLevel > 0 );
    return _decisions[decisionLevel - 1]->getPiecewiseLinearCaseSplit() ;
}

bool SmtCore::backtrackAndContinue()
{
    SMT_LOG( "Performing a pop" );

    if ( getDecisionLevel() == 0 )
        return false;

    struct timespec start = TimeUtils::sampleMicro();

    if ( _statistics )
        _statistics->incNumVisitedTreeStates();

    TrailEntry lastDecision( NULL, 0 );

    popDecisionLevel( &lastDecision );

    while ( lastDecision._alternativeSplits.empty() )
    {
        interruptIfCompliantWithDebugSolution();

        if ( !popDecisionLevel( &lastDecision ) )
            return false;
    }

    interruptIfCompliantWithDebugSolution();

    ASSERT( lastDecision._alternativeSplits.size() > 0 );
    if ( 1u == lastDecision._alternativeSplits.size() )
        pushImplication( lastDecision._pwlConstraint,  lastDecision._alternativeSplits.front() );
    else
        decideSplit( lastDecision._pwlConstraint, lastDecision._alternativeSplits);

    if ( _statistics )
    {
        _statistics->setCurrentDecisionLevel( getDecisionLevel() );
        struct timespec end = TimeUtils::sampleMicro();
        _statistics->addTimeSmtCore( TimeUtils::timePassed( start, end ) );
    }

    checkSkewFromDebuggingSolution();
    return true;
}

void SmtCore::resetReportedViolations()
{
    _constraintToViolationCount.clear();
    _needToSplit = false;
}

void SmtCore::implyValidSplit( PiecewiseLinearCaseSplit &validSplit )
{
    SMT_LOG( "Push implication on stack @t%d ...", getStackDepth() );
    // validSplit.dump()
    if ( _stack.empty() )
        _impliedValidSplitsAtRoot.append( validSplit );
    else
        _stack.back()->_impliedValidSplits.append( validSplit );

    SMT_LOG( "Push implication on stack DONE" );

    checkSkewFromDebuggingSolution();
}



void SmtCore::allSplitsSoFar( List<PiecewiseLinearCaseSplit> &result ) const
{
    result.clear();

    for ( auto trailEntry : _trail )
        result.append( trailEntry.getPiecewiseLinearCaseSplit() );
}

void SmtCore::setStatistics( Statistics *statistics )
{
    _statistics = statistics;
}

void SmtCore::storeDebuggingSolution( const Map<unsigned, double> &debuggingSolution )
{
    _debuggingSolution = debuggingSolution;
}

// Return true if stack is currently compliant, false otherwise
// If there is no stored solution, return false --- incompliant.
bool SmtCore::checkSkewFromDebuggingSolution()
{
    if ( _debuggingSolution.empty() )
        return false;

    String error;

    // First check that the valid splits implied at the root level are okay
    for ( const auto &split : _impliedValidSplitsAtRoot )
    {
        if ( !splitAllowsStoredSolution( split, error ) )
        {
            printf( "Error with one of the splits implied at root level:\n\t%s\n", error.ascii() );
            throw MarabouError( MarabouError::DEBUGGING_ERROR );
        }
    }

    // Now go over the stack from oldest to newest and check that each level is compliant
    for ( const auto &stackEntry : _stack )
    {
        // If the active split is non-compliant but there are alternatives, that's fine
        if ( !splitAllowsStoredSolution( stackEntry->_activeSplit, error ) )
        {
            if ( stackEntry->_alternativeSplits.empty() )
            {
                printf( "Error! Have a split that is non-compliant with the stored solution, "
                        "without alternatives:\n\t%s\n", error.ascii() );
                throw MarabouError( MarabouError::DEBUGGING_ERROR );
            }

            // Active split is non-compliant but this is fine, because there are alternatives. We're done.
            return false;
        }

        // Did we learn any valid splits that are non-compliant?
        for ( auto const &split : stackEntry->_impliedValidSplits )
        {
            if ( !splitAllowsStoredSolution( split, error ) )
            {
                printf( "Error with one of the splits implied at this stack level:\n\t%s\n",
                        error.ascii() );
                throw MarabouError( MarabouError::DEBUGGING_ERROR );
            }
        }
    }

    // No problems were detected, the stack is compliant with the stored solution
    return true;
}

bool SmtCore::splitAllowsStoredSolution( const PiecewiseLinearCaseSplit &split, String &error ) const
{
    // False if the split prevents one of the values in the stored solution, true otherwise.
    error = "";
    if ( _debuggingSolution.empty() )
        return true;

    for ( const auto bound : split.getBoundTightenings() )
    {
        unsigned variable = bound._variable;

        // If the possible solution doesn't care about this variable,
        // ignore it
        if ( !_debuggingSolution.exists( variable ) )
            continue;

        // Otherwise, check that the bound is consistent with the solution
        double solutionValue = _debuggingSolution[variable];
        double boundValue = bound._value;

        if ( ( bound._type == Tightening::LB ) && FloatUtils::gt( boundValue, solutionValue ) )
        {
            error = Stringf( "Variable %u: new LB is %.5lf, which contradicts possible solution %.5lf",
                             variable,
                             boundValue,
                             solutionValue );
            return false;
        }
        else if ( ( bound._type == Tightening::UB ) && FloatUtils::lt( boundValue, solutionValue ) )
        {
            error = Stringf( "Variable %u: new UB is %.5lf, which contradicts possible solution %.5lf",
                             variable,
                             boundValue,
                             solutionValue );
            return false;
        }
    }

    return true;
}

void SmtCore::setConstraintViolationThreshold( unsigned threshold )
{
    _constraintViolationThreshold = threshold;
}

PiecewiseLinearConstraint *SmtCore::chooseViolatedConstraintForFixing( List<PiecewiseLinearConstraint *> &_violatedPlConstraints ) const
{
    ASSERT( !_violatedPlConstraints.empty() );

    if ( !GlobalConfiguration::USE_LEAST_FIX )
        return *_violatedPlConstraints.begin();

    PiecewiseLinearConstraint *candidate;

    // Apply the least fix heuristic
    auto it = _violatedPlConstraints.begin();

    candidate = *it;
    unsigned minFixes = getViolationCounts( candidate );

    PiecewiseLinearConstraint *contender;
    unsigned contenderFixes;
    while ( it != _violatedPlConstraints.end() )
    {
        contender = *it;
        contenderFixes = getViolationCounts( contender );
        if ( contenderFixes < minFixes )
        {
            minFixes = contenderFixes;
            candidate = contender;
        }

        ++it;
    }

    return candidate;
}

bool SmtCore::pickSplitPLConstraint()
{
    if ( _needToSplit )
        _constraintForSplitting = _engine->pickSplitPLConstraint();
    return _constraintForSplitting != NULL;
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
