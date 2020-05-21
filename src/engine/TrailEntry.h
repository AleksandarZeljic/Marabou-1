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
  PiecewiseLinearConstraint * _pwlconstraint;
  unsigned _phase; /* Enum for fixed case PiecewiseLinearConstraints, case index otherwise (e.g. Max)*/

 TrailEntry(PiecewiseLinearConstraint * pwlc, unsigned phase)
    : _pwlconstraint(pwlc),
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
