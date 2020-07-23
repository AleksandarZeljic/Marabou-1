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
    , _engine( engine )
    , _needToSplit( false )
    , _constraintForSplitting( NULL )
    , _stateId( 0 )
    , _constraintViolationThreshold
      ( GlobalConfiguration::CONSTRAINT_VIOLATION_THRESHOLD )
{
    //SMT_LOG( "Decision level 0 ..." );
    //_context.push();
    //SMT_LOG( "Decision level 0 - DONE" );
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

void SmtCore::decideSplit()
{
    ASSERT( getStackDepth() == static_cast<unsigned>( _context.getLevel() ) );
    ASSERT( _needToSplit );
    SMT_LOG( "Performing a ReLU split" );

    // Maybe the constraint has already become inactive - if so, ignore
    // TODO: Ideally we will not ever reach this point
    if ( !_constraintForSplitting->isActive() )
    {
        _needToSplit = false;
        _constraintToViolationCount[_constraintForSplitting] = 0;
        _constraintForSplitting = NULL;
        return;
    }

    struct timespec start = TimeUtils::sampleMicro();

    ASSERT( _constraintForSplitting->isActive() );
    _needToSplit = false;

    if ( _statistics )
    {
        _statistics->incNumSplits();
        _statistics->incNumVisitedTreeStates();
    }

    // Before storing the state of the engine, we:
    //   1. Obtain the splits.
    //   2. Disable the constraint, so that it is marked as disbaled in the EngineState.
    List<PiecewiseLinearCaseSplit> splits = _constraintForSplitting->getCaseSplits();
    ASSERT( !splits.empty() );
    ASSERT( splits.size() >= 2 ); // Not really necessary, can add code to handle this case.
    _constraintForSplitting->setActiveConstraint( false );

    // Obtain the current state of the engine
    EngineState *stateBeforeSplits = new EngineState;
    stateBeforeSplits->_stateId = _stateId;
    ++_stateId;
    _engine->storeState( *stateBeforeSplits, true );

    StackEntry *stackEntry = new StackEntry;
    // Perform the first split: add bounds and equations
    List<PiecewiseLinearCaseSplit>::iterator split = splits.begin();
    SMT_LOG( "\tApplying new split..." );
    split->dump();
    _engine->applySplit( *split );
    SMT_LOG( "\tApplying new split - DONE" );
    stackEntry->_activeSplit = *split;
    stackEntry->_sourceConstraint= _constraintForSplitting;

    // Trail changes require a context push to denote a new decision level
    SMT_LOG( "New decision level ..." );
    _context.push();
    SMT_LOG( "> New decision level %d", _context.getLevel() );
    SMT_LOG( "New decision ..." );
    TrailEntry te( _constraintForSplitting, stackEntry->_activeSplit.getPhase() );
    _trail.push_back(te);
    SMT_LOG( "Decision push @ %d DONE", _context.getLevel() );

    // Store the remaining splits on the stack, for later
    stackEntry->_engineState = stateBeforeSplits;
    ++split;
    while ( split != splits.end() )
    {
        stackEntry->_alternativeSplits.append( *split );
        ++split;
    }

    _stack.append( stackEntry );
    SMT_LOG( "> New stack depth: %d", getStackDepth() );
    if ( _statistics )
    {
        _statistics->setCurrentStackDepth( getStackDepth() );
        struct timespec end = TimeUtils::sampleMicro();
        _statistics->addTimeSmtCore( TimeUtils::timePassed( start, end ) );
    }
    SMT_LOG( "Performing a ReLU split - DONE");

    //ASSERT( getStackDepth() == static_cast<unsigned>( _context.getLevel() ) );
    ASSERT( checkStackTrailEquivalence() );
}


bool SmtCore::checkStackTrailEquivalence()
{
    std::cout << "Checking STEQ ... ";
    bool result = true;
    // Trail post-condition: TRAIL - STACK equivalence
    // How are things present on the stack?
    List<PiecewiseLinearCaseSplit> stackCaseSplits;
    allSplitsSoFar( stackCaseSplits );

    std::cout << "Collected all splits on stack" <<std::endl;
    List<PiecewiseLinearCaseSplit> trailCaseSplits;
    std::cout << "Trail size: " << _trail.size() <<std::endl;
    for ( TrailEntry trailEntry : _trail )
        trailCaseSplits.append( trailEntry.getPiecewiseLinearCaseSplit() );

    std::cout << "Collected case splits: " << trailCaseSplits.size() <<std::endl;
    std::cout << "Collected all splits on trail" <<std::endl;
   // Equivalence check, assumes the order of stack and trail is identical
    result = result && ( trailCaseSplits.size() == stackCaseSplits.size() );

    if ( !result )
    {
        std::cout << "ASSERTION VIOLATION: ";
        std::cout << "Trail ( " << trailCaseSplits.size();
        std::cout << ")and Stack (" << stackCaseSplits.size();
        std::cout << ") have different number of elements!" << std::endl;

        std::cout << "Trail:" << std::endl;
        for ( auto split : trailCaseSplits )
            split.dump();

        std::cout << "Stack:" << std::endl;
        for ( auto split : stackCaseSplits )
            split.dump();

    }

    std::cout << "Size matches!" <<std::endl;
    PiecewiseLinearCaseSplit tCaseSplit, sCaseSplit;
    int ind = trailCaseSplits.size();
    while ( result && !stackCaseSplits.empty() )
    {
        tCaseSplit = trailCaseSplits.back();
        sCaseSplit = stackCaseSplits.back();
        result = result && ( tCaseSplit == sCaseSplit );
        if ( !result )
        {
            std::cout << "FAILED at position " << ind << "!!!!" <<std::endl;
            std::cout << "Trail case split: ";
            tCaseSplit.dump();

            auto sloc = find_if( stackCaseSplits.begin(),
                                stackCaseSplits.end(),
                                [&](PiecewiseLinearCaseSplit c) { return c == tCaseSplit; } );

            if ( stackCaseSplits.end() == sloc )
                std::cout << "Missing from the stack!";

            std::cout << "Stack case split: ";
            sCaseSplit.dump();
            auto tloc = find_if( trailCaseSplits.begin(),
                                trailCaseSplits.end(),
                                [&](PiecewiseLinearCaseSplit c) { return c == sCaseSplit; } );

            if ( trailCaseSplits.end() == tloc )
                std::cout << "Missing from the stack!";


        }

        --ind;

        trailCaseSplits.popBack();
        stackCaseSplits.popBack();
    }

    if ( result )
        std::cout << "OK" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    return result;
}

unsigned SmtCore::getStackDepth() const
{
    // TODO: This is not an invariant, since the stack treats implications as
    // decisions
    // ASSERT( _stack.size() == static_cast<unsigned>( _context.getLevel() ) );
    return _stack.size();
}

bool SmtCore::popSplit()
{
    if ( _stack.empty() )
        return false;

    if ( _statistics )
        _statistics->incNumPops();

    delete _stack.back()->_engineState;
    delete _stack.back();
    _stack.popBack();
    return true;
}

bool SmtCore::popSplitFromStack( List<PiecewiseLinearCaseSplit> &alternativeSplits )
{
    alternativeSplits.clear();
    alternativeSplits.assign( _stack.back()->_alternativeSplits.begin(), _stack.back()->_alternativeSplits.end() );

    return popSplit();
}



void SmtCore::popDecisionLevel()
{
    SMT_LOG( "Backtracking context ..." );
    _context.pop();
    SMT_LOG( "Backtracking context - %d DONE", _context.getLevel() );
}

void SmtCore::interruptIfCompliantWithDebugSolution()
{
    if ( checkSkewFromDebuggingSolution() )
    {
        SMT_LOG( "Error! Popping from a compliant stack\n" );
        throw MarabouError( MarabouError::DEBUGGING_ERROR );
    }
}

bool SmtCore::backtrackAndContinue()
{
    ASSERT( getStackDepth() == static_cast<unsigned>( _context.getLevel() ) );
    SMT_LOG( "Performing a pop" );

    // TODO: We do not want to reach this point
    if ( _stack.empty() )
        return false;

    struct timespec start = TimeUtils::sampleMicro();

    if ( _statistics )
        _statistics->incNumVisitedTreeStates();

    // Counter for Stack pops to be matched by trail pops
    // Starts at one, because Stack does not distinguish implications
    // (temp feature, to be factored out)
    unsigned popCount = 1;
    List<PiecewiseLinearCaseSplit> alternatives;

    // Remove any entries that have no alternatives

    while ( _stack.back()->_alternativeSplits.empty() )
    {
        interruptIfCompliantWithDebugSolution();

        if ( popSplit() )
            ++popCount;
        else
            return false;
    }

    interruptIfCompliantWithDebugSolution();

    StackEntry *stackEntry = _stack.back();
    SMT_LOG( "\tRestoring engine state..." );
    _engine->restoreState( *(stackEntry->_engineState) );
    SMT_LOG(  "\tRestoring engine state %d - DONE", getStackDepth() );

    // Clear any implications learned using the split we just popped
    stackEntry->_impliedValidSplits.clear();

    while ( popCount-- )
        popDecisionLevel();

    auto split = stackEntry->_alternativeSplits.begin();

    // TODO: Make a distinction between a decision and an implication
    // TODO: Refactor decideSplit to take case split as an argument

    SMT_LOG( "\tApplying new case split to stack/engine ..." );
    _engine->applySplit( *split );
    SMT_LOG( "\tApplying new case split to stack/engine - DONE" );
    stackEntry->_activeSplit = *split;
    stackEntry->_alternativeSplits.erase( split );


    // This is to simulate Stack and ensure STACK/TRAIL size compliance
    _context.push();
    implyCaseSplit( stackEntry->_sourceConstraint, stackEntry->_activeSplit.getPhase() );

    if ( _statistics )
    {
        _statistics->setCurrentStackDepth( getStackDepth() );
        struct timespec end = TimeUtils::sampleMicro();
        _statistics->addTimeSmtCore( TimeUtils::timePassed( start, end ) );
    }

    checkSkewFromDebuggingSolution();

    std::cout << "Stack depth: " << getStackDepth() << std::endl;
    std::cout << "Context depth: " << _context.getLevel() << std::endl;

    ASSERT( getStackDepth() == static_cast<unsigned>( _context.getLevel() ) );
    ASSERT( checkStackTrailEquivalence() );
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

void SmtCore::implyCaseSplit( PiecewiseLinearConstraint *constraint, unsigned phase )
{
    SMT_LOG( "Push implication on trail @s%d ... ", _context.getLevel() );
    TrailEntry te( constraint, phase );
    //te.getPiecewiseLinearCaseSplit().dump();
    _trail.push_back( te );
    SMT_LOG( "Push implication on trail DONE"  );
}

void SmtCore::allSplitsSoFar( List<PiecewiseLinearCaseSplit> &result ) const
{
    result.clear();

    for ( const auto &it : _impliedValidSplitsAtRoot )
        result.append( it );

    for ( const auto &it : _stack )
    {
        result.append( it->_activeSplit );
        for ( const auto &impliedSplit : it->_impliedValidSplits )
            result.append( impliedSplit );
    }

    // auto removed = std::remove_if( result.begin(), result.end(),
    //                                []( PiecewiseLinearCaseSplit plcs )
    //                                { return plcs.getBoundTightenings().empty()
    //                                         && plcs.getEquations().empty(); } );

    // result.erase( removed, result.end() );
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
