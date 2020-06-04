/*********************                                                        */
/*! \file PiecewiseLinearCaseSplit.h
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

#ifndef __PiecewiseLinearCaseSplit_h__
#define __PiecewiseLinearCaseSplit_h__

#include "Equation.h"
#include "IEngine.h"
#include "MString.h"
#include "Pair.h"
#include "Tightening.h"
#include "PiecewiseLinearConstraint.h"


// TODO: Add all the other phases into this enumeration?
enum PWLCPhaseStatus {
                      PHASE_NOT_FIXED = 0,
                      RELU_PHASE_ACTIVE = 1,
                      RELU_PHASE_INACTIVE = 2,
                      ABS_BOTH_POSITIVE = 4,
                      ABS_BOTH_NEGATIVE = 5,
                      ABS_POSITIVE_NEGATIVE = 6,
                      ABS_NEGATIVE_POSITIVE = 7,
};

class PiecewiseLinearCaseSplit
{
public:
    /*
      Store information regarding a bound tightening.
    */
    void storeBoundTightening( const Tightening &tightening );
    const List<Tightening> & getBoundTightenings() const;

    /*
      Store information regarding a new equation to be added.
    */
    void addEquation( const Equation &equation );
  	const List<Equation> & getEquations() const;

    /*
      Dump the case split - for debugging purposes.
    */
    void dump() const;
    void dump( String &output ) const;

    /*
      Equality operator.
    */
    bool operator==( const PiecewiseLinearCaseSplit &other ) const;

    /*
      Change the index of a variable that appears in this case split
    */
    void updateVariableIndex( unsigned oldIndex, unsigned newIndex );

    unsigned getPhase() { return _phase; }
    void setPhase(unsigned phase) { _phase = phase;}

private:
    /*
      Bound tightening information.
    */
    List<Tightening> _bounds;

    /*
      The equation that needs to be added.
    */
    List<Equation> _equations;

    /*
     * PhaseStatus
     */
    unsigned _phase;
};

#endif // __PiecewiseLinearCaseSplit_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
