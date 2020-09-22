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
    ASSERT( nullptr == _context );
    _context = context;

    initializeActiveStatus();
    initializePhaseStatus();
}

void PiecewiseLinearConstraint::cdoCleanup()
{
    if ( nullptr != _constraintActive )
        _constraintActive->deleteSelf();

    _constraintActive= nullptr;

    if ( nullptr != _phaseStatus )
        _phaseStatus->deleteSelf();

    _phaseStatus = nullptr;

    _context = nullptr;
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


PhaseStatus PiecewiseLinearConstraint::getPhaseStatus() const
{
    ASSERT( nullptr != _phaseStatus );
    return *_phaseStatus;
}

void PiecewiseLinearConstraint::setPhaseStatus( PhaseStatus phaseStatus )
{
    ASSERT( nullptr != _phaseStatus );
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
    }
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
