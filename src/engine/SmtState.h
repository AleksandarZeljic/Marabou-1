/*********************                                                        */
/*! \file SmtState.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Duligur Ibeling
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#ifndef __SmtState_h__
#define __SmtState_h__

#include "List.h"
#include "TrailEntry.h"

class SmtState
{
public:
    /*
      The trail.
    */
    List<TrailEntry *> _trail;

    /*
      A unique ID allocated to every state that is stored, for
      debugging purposes.
    */
    unsigned _stateId;
};

#endif // __SmtState_h__

