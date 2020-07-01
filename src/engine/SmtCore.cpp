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
    //log( "Decision level 0 ..." );
    //_context.push();
    //log( "Decision level 0 - DONE" );
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
        if ( GlobalConfiguration::SPLITTING_HEURISTICS == DivideStrategy::ReLUViolation )
            _constraintForSplitting = constraint;
        else
            pickSplitPLConstraint();
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

void SmtCore::performSplit()
{
    ASSERT( getStackDepth() == static_cast<unsigned>( _context.getLevel() ) );
    ASSERT( _needToSplit );
    log( "Performing a ReLU split" );

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
    log( "\tApplying new split..." );
    split->dump();
    _engine->applySplit( *split );
    log( "\tApplying new split - DONE" );
    stackEntry->_activeSplit = *split;
    stackEntry->_sourceConstraint= _constraintForSplitting;

    // Trail changes require a context push to denote a new decision level
    log( "New decision level ..." );
    _context.push();
    log( Stringf("> New decision level %d", _context.getLevel() ) );
    log( "New decision ..." );
    TrailEntry te( _constraintForSplitting, stackEntry->_activeSplit.getPhase() );
    _trail.push_back(te);
    log( Stringf( "Decision push @ %d DONE", _context.getLevel() ) );

    // Store the remaining splits on the stack, for later
    stackEntry->_engineState = stateBeforeSplits;
    ++split;
    while ( split != splits.end() )
    {
        stackEntry->_alternativeSplits.append( *split );
        ++split;
    }

    _stack.append( stackEntry );
    log( Stringf( "> New stack depth: %d", getStackDepth() ) );
    if ( _statistics )
    {
        _statistics->setCurrentStackDepth( getStackDepth() );
        struct timespec end = TimeUtils::sampleMicro();
        _statistics->addTimeSmtCore( TimeUtils::timePassed( start, end ) );
    }
    log( "Performing a ReLU split - DONE");

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

    List<PiecewiseLinearCaseSplit> trailCaseSplits;
    for ( TrailEntry trailEntry : _trail )
        trailCaseSplits.append( trailEntry.getPiecewiseLinearCaseSplit() );

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
    PiecewiseLinearCaseSplit tCaseSplit, sCaseSplit;
    while ( result && !stackCaseSplits.empty() )
    {
        tCaseSplit = trailCaseSplits.back();
        sCaseSplit = stackCaseSplits.back();

        result = result && ( tCaseSplit == sCaseSplit );

        trailCaseSplits.popBack();
        stackCaseSplits.popBack();
    }
    if ( result )
        std::cout << "OK";
    else
        std::cout << "FAILED!!!!";

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

    ASSERT( getStackDepth() == static_cast<unsigned>( _context.getLevel() ) );
    log( "Performing a pop" );

    // TODO: We do not want to reach this point
    if ( _stack.empty() )
        return false;

    struct timespec start = TimeUtils::sampleMicro();

    if ( _statistics )
    {
        _statistics->incNumPops();
        // A pop always sends us to a state that we haven't seen before - whether
        // from a sibling split, or from a lower level of the tree.
        _statistics->incNumVisitedTreeStates();
    }

    // Remove any entries that have no alternatives
    String error;
    unsigned popCount = 1;
    while ( _stack.back()->_alternativeSplits.empty() )
    {
        if ( checkSkewFromDebuggingSolution() )
        {
            // Pops should not occur from a compliant stack!
            printf( "Error! Popping from a compliant stack\n" );
            throw MarabouError( MarabouError::DEBUGGING_ERROR );
        }

        delete _stack.back()->_engineState;
        delete _stack.back();
        _stack.popBack();
        ASSERT( _context.getLevel() > 0 );
        log( "Backtracking context ..." );
        ++popCount;
        log( Stringf( "Backtracking context - %d DONE", _context.getLevel() ) );
        if ( _stack.empty() )
            return false;
    }

    if ( checkSkewFromDebuggingSolution() )
    {
        // Pops should not occur from a compliant stack!
        printf( "Error! Popping from a compliant stack\n" );
        throw MarabouError( MarabouError::DEBUGGING_ERROR );
    }

    StackEntry *stackEntry = _stack.back();

    while ( popCount-- )
    {
        _context.pop();
    }

    _context.push(); //This is just to simulate Stack
    // Restore the state of the engine
    log( "\tRestoring engine state..." );
    _engine->restoreState( *(stackEntry->_engineState) );
    log( Stringf("\tRestoring engine state %d - DONE", getStackDepth() ) );

    // Apply the new split and erase it from the list
    // TODO: Rename split to implication?
    auto split = stackEntry->_alternativeSplits.begin();

    // Erase any valid splits that were learned using the split we just popped
    stackEntry->_impliedValidSplits.clear();

    // Pop ends here; And this is also a pop loop in fact;
    // While  ( stackEntry -> noAlternatives )
    //     popSplit();
    // decideSplit( getNextAlternative() ) 
    // {
    // _context.push();
    // propagateSplit( );
    // }

    // implySplit( caseSplit );

    // Stack:
    // D1 (D1' D1'' D1''') -> I1 I2 I3
    // D2 -> I4 I5 ...


    // Trail:
    // * D1 (-> C1)
    //   I1
    //  ...
    // * D2
    // ....
    
    log( "\tApplying new split..." );
    split->dump();
    _engine->applySplit( *split );
    log( "\tApplying new split - DONE" );

    stackEntry->_activeSplit = *split;
    stackEntry->_alternativeSplits.erase( split );

    // Check whether this is an implication and if it is pop
    // if ( stackEntry->_alternativeSplits.empty() )
    //    _context.pop();

    // To keep generality in mind, if this is not an implication we need to
    // _context.push();

    // TODO: Trail bookkeeping - where each level begins
    // _context.push();
    // TODO: Assert negated Literal
    log( "Trail push... " );
    TrailEntry te( stackEntry->_sourceConstraint, stackEntry->_activeSplit.getPhase() );
    te.getPiecewiseLinearCaseSplit().dump();
    _trail.push_back( te );
    log( Stringf( "\"Decision\" push @ %d DONE", _context.getLevel() ) );

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

void SmtCore::recordImpliedValidSplit( PiecewiseLinearCaseSplit &validSplit )
{
    if ( _stack.empty() )
        _impliedValidSplitsAtRoot.append( validSplit );
    else
        _stack.back()->_impliedValidSplits.append( validSplit );
    log( "Pushing implication ..." );
    //_trail.push_back( validSplit );
    log( Stringf( "Implication push @ %d DONE", _context.getLevel() ) );
    //ASSERT( & validSplit != & (_trail.back()));

      checkSkewFromDebuggingSolution();
}

void SmtCore::recordImpliedValidCaseSplit( PiecewiseLinearConstraint *constraint, unsigned phase )
{
    log( "Implication... " );
    TrailEntry te( constraint, phase );
    _trail.push_back( te );
    log( Stringf( "Implication @ %d DONE", _context.getLevel() ) );
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

void SmtCore::log( const String &message )
{
    if ( GlobalConfiguration::SMT_CORE_LOGGING )
        printf( "SmtCore: %s\n", message.ascii() );
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

void SmtCore::pickSplitPLConstraint()
{
    if ( _needToSplit && !_constraintForSplitting )
    {
        _constraintForSplitting = _engine->pickSplitPLConstraint();
    }
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
