/*********************                                                        */
/*! \file TrailEntry.h
** \verbatim
** Top contributors (to current version):
**  Aleksandar Zeljic
** This file is part of the Marabou project.
** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
** in the top-level source directory) and their institutional affiliations.
** All rights reserved. See the file COPYING in the top-level source
** directory for licensing information.\endverbatim
**
** TrailEntry represent a case of PiecewiseLinearConstraint asserted on the
** trail. Current implementation consists of a pointer to the
** PiecewiseLinearConstraint and the chosen case represented using PhaseStatus
** enum.
**/

#ifndef __TrailEntry_h__
#define __TrailEntry_h__

#include "PiecewiseLinearConstraint.h"
#include "PiecewiseLinearCaseSplit.h"

/*
  A trail entry consists of the pointer to a PiecewiseLinearConstraint and
  phase designation, whether its a decision and its decision level
*/
class TrailEntry
{
public:
    PiecewiseLinearConstraint *_pwlConstraint;
    PhaseStatus _phase;
    bool _isDecision;
    unsigned _decisionLevel;

    PiecewiseLinearCaseSplit getPiecewiseLinearCaseSplit() const
    {
        return _pwlConstraint->getCaseSplit( _phase );
    }

    inline void markInfeasible()
    {
        _pwlConstraint->markInfeasible( _phase );
    }

    inline bool isFeasible() const
    {
        return _pwlConstraint->isFeasible();
    }

    TrailEntry( PiecewiseLinearConstraint *pwlc, PhaseStatus phase, bool isDecision, unsigned decisionLevel )
        : _pwlConstraint( pwlc )
        , _phase( phase )
        , _isDecision( isDecision )
        , _decisionLevel( decisionLevel )
    {
    }

    ~TrailEntry(){};

    TrailEntry *duplicateTrailEntry() const
    {
      return new TrailEntry( _pwlConstraint, _phase, _isDecision, _decisionLevel );
    }
};

#endif // TrailEntry.h
