/*********************                                                        */
/*! \file PiecewiseLinearConstraint.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#include "PiecewiseLinearConstraint.h"
#include "Statistics.h"

PiecewiseLinearConstraint::PiecewiseLinearConstraint()
    : _context( nullptr )
    , _constraintActive( nullptr )
    , _phaseStatus( nullptr )
    , _infeasibleCases( nullptr )
    , _numCases( 0 )
    , _score( -1 )
    , _boundManager( nullptr )
    , _statistics( nullptr )
{
}

PiecewiseLinearConstraint::PiecewiseLinearConstraint( unsigned numCases )
    : _context( nullptr )
    , _constraintActive( nullptr )
    , _phaseStatus( nullptr )
    , _infeasibleCases( nullptr )
    , _numCases( numCases )
    , _score( -1 )
    , _boundManager( nullptr )
    , _statistics( nullptr )
{
}

void PiecewiseLinearConstraint::setStatistics( Statistics *statistics )
{
    _statistics = statistics;
}

void PiecewiseLinearConstraint::registerBoundManager( BoundManager *boundManager)
{
    _boundManager = boundManager;
}

void PiecewiseLinearConstraint::initializeCDOs( CVC4::context::Context *context )
{
    if ( _context == NULL )
    {
        _context = context;
        initializeActiveStatus();
        initializePhaseStatus();
        initializeInfeasibleCases();
    }
    else
    {
        _context = context;
        reinitializeActiveStatus();
        reinitializePhaseStatus();
        reinitializeInfeasibleCases();
    }
}

void PiecewiseLinearConstraint::cdoCleanup()
{
    if ( nullptr != _constraintActive )
        _constraintActive->deleteSelf();

    _constraintActive= nullptr;

    if ( nullptr != _phaseStatus )
        _phaseStatus->deleteSelf();

    _phaseStatus = nullptr;

    if ( nullptr != _infeasibleCases )
        _infeasibleCases->deleteSelf();

    _infeasibleCases = nullptr;

    _context = nullptr;
}

void PiecewiseLinearConstraint::initializeInfeasibleCases()
{
    ASSERT( nullptr != _context );
    ASSERT( nullptr == _infeasibleCases );
    _infeasibleCases = new (true) CVC4::context::CDList<PhaseStatus>( _context );
}

void PiecewiseLinearConstraint::initializeActiveStatus()
{
    ASSERT( nullptr != _context );
    ASSERT( nullptr == _constraintActive );
    _constraintActive = new (true) CVC4::context::CDO<bool>( _context, true );
}

void PiecewiseLinearConstraint::initializePhaseStatus()
{
    ASSERT( nullptr != _context );
    ASSERT( nullptr == _phaseStatus );
    _phaseStatus = new (true) CVC4::context::CDO<PhaseStatus>( _context, PHASE_NOT_FIXED );
}

void PiecewiseLinearConstraint::reinitializeInfeasibleCases()
{
    ASSERT( nullptr != _context );
    ASSERT( nullptr != _infeasibleCases );
    auto infeasibleCases = new (true) CVC4::context::CDList<PhaseStatus>( _context );
    for ( auto infeasibleCase = _infeasibleCases->begin(); infeasibleCase != _infeasibleCases->end(); ++infeasibleCase )
        infeasibleCases->push_back( *infeasibleCase );
    _infeasibleCases->deleteSelf();
    _infeasibleCases = infeasibleCases;
}

void PiecewiseLinearConstraint::reinitializeActiveStatus()
{
    ASSERT( nullptr != _context );
    ASSERT( nullptr != _constraintActive );
    bool constraintActive = *_constraintActive;
    _constraintActive->deleteSelf();
    _constraintActive = new (true) CVC4::context::CDO<bool>( _context, constraintActive );
}

void PiecewiseLinearConstraint::reinitializePhaseStatus()
{
    ASSERT( nullptr != _context );
    ASSERT( nullptr != _phaseStatus );
    PhaseStatus phaseStatus = *_phaseStatus;
    _phaseStatus->deleteSelf();
    _phaseStatus = new (true) CVC4::context::CDO<PhaseStatus>( _context, phaseStatus );
}


PhaseStatus PiecewiseLinearConstraint::getPhaseStatus() const
{
    ASSERT( nullptr != _phaseStatus );
    return *_phaseStatus;
}

void PiecewiseLinearConstraint::setPhaseStatus( PhaseStatus phaseStatus )
{
    ASSERT( nullptr != _phaseStatus );
    //ASSERT( PHASE_NOT_FIXED == getPhaseStatus() );
    *_phaseStatus = phaseStatus;
}

void PiecewiseLinearConstraint::initializeDuplicatesCDOs( PiecewiseLinearConstraint *clone ) const
{
    if ( nullptr != clone->_context)
    {
        ASSERT( nullptr != clone->_constraintActive );
        clone->_constraintActive = nullptr;
        clone->initializeActiveStatus();
        clone->setActiveConstraint( this->isActive() );

        ASSERT( nullptr != clone->_phaseStatus );
        clone->_phaseStatus = nullptr;
        clone->initializePhaseStatus();
        clone->setPhaseStatus( this->getPhaseStatus() );

        ASSERT( nullptr != clone->_infeasibleCases );
        clone->_infeasibleCases = nullptr;
        clone->initializeInfeasibleCases();
        // Does not copy contents
    }
}

void PiecewiseLinearConstraint::markInfeasible( PhaseStatus infeasibleCase )
{
    _infeasibleCases->push_back( infeasibleCase );
}

PhaseStatus PiecewiseLinearConstraint::nextFeasibleCase()
{
    List<PhaseStatus> allCases = getAllCases();

    if ( phaseFixed() )
        return getPhaseStatus();

    if ( _infeasibleCases->size() == allCases.size() )
        return PHASE_NOT_FIXED;

    for ( PhaseStatus thisCase : allCases)
    {
        auto loc = std::find( _infeasibleCases->begin(), _infeasibleCases->end(), thisCase );
        // Case not found, therefore it is feasible
        if ( _infeasibleCases->end() == loc )
            return thisCase;
    }

    //UNREACHABLE
    ASSERT( false );
    return PHASE_NOT_FIXED;
}

unsigned PiecewiseLinearConstraint::numFeasibleCases()
{
    return _numCases - _infeasibleCases->size();
}

bool PiecewiseLinearConstraint::isFeasible()
{
    return numFeasibleCases() >  0u;
}

bool PiecewiseLinearConstraint::isImplication()
{
    return 1u == numFeasibleCases();
}

void PiecewiseLinearConstraint::dump() const
{
    String output;
    dump( output );
    printf( "%s", output.ascii() );
}

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
