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

void PiecewiseLinearConstraint::initializeActiveStatus()
{
    ASSERT( nullptr != _context );
    ASSERT( nullptr == _constraintActive );
    _constraintActive = new (true) CVC4::context::CDO<bool>( _context );
    *_constraintActive = true;
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
