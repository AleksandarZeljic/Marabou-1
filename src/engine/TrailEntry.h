/*********************                                                        */
/*! \file SmtCore.h
** \verbatim
** Top contributors (to current version):
**  Aleksandar Zeljic
** This file is part of the Marabou project.
** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
** in the top-level source directory) and their institutional affiliations.
** All rights reserved. See the file COPYING in the top-level source
** directory for licensing information.\endverbatim
**
** [[ Add lengthier description here ]]

**/

#ifndef __TrailEntry_h__
#define __TrailEntry_h__

#include "PiecewiseLinearCaseSplit.h"

/*
  A trail entry consists of the pointer to a PiecewiseLinearConstraint and
  phase designation.
*/
class TrailEntry
{
public:
  PiecewiseLinearConstraint * _pwlConstraint;
  unsigned _phase; /* Enum for fixed case PiecewiseLinearConstraints, case index otherwise (e.g. Max)*/

  PiecewiseLinearCaseSplit getPiecewiseLinearCaseSplit()
  {
      // Assumes that _phase is unique and contained in getCaseSplits
      List<PiecewiseLinearCaseSplit> cases = _pwlConstraint->getCaseSplits();
      unsigned myPhase = _phase;
      auto loc = find_if( cases.begin(), cases.end(), [&](PiecewiseLinearCaseSplit c) { return c.getPhase() == _phase; } );
      return *loc;
      /* for ( auto c : cases) */
      /* { */
      /*     if ( c.getPhase() == _phase ) */
      /*         return c; */
      /* } */
      /* ASSERT( false ); */
  }

 TrailEntry(PiecewiseLinearConstraint * pwlc, unsigned phase)
    : _pwlConstraint(pwlc),
    _phase(phase)
    {
    }

  ~TrailEntry() {};
};

#endif // TrailEntry.h

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
