/*********************                                                        */
/*! \file PiecewiseLinearConstraint.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Derek Huang, Duligur Ibeling
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

**/

#ifndef __PiecewiseLinearConstraint_h__
#define __PiecewiseLinearConstraint_h__

#include "context/context.h"
#include "context/cdo.h"
#include "FloatUtils.h"
#include "ITableau.h"
#include "List.h"
#include "Map.h"
#include "MarabouError.h"
#include "PiecewiseLinearCaseSplit.h"
#include "PiecewiseLinearFunctionType.h"
#include "Queue.h"
#include "Tightening.h"

class Equation;
class BoundManager;
class ITableau;
class InputQuery;
class String;

class PiecewiseLinearConstraint : public ITableau::VariableWatcher
{
public:
    enum PhaseStatus : unsigned {
        PHASE_NOT_FIXED = 0,
        RELU_PHASE_ACTIVE = 1,
        RELU_PHASE_INACTIVE = 2,
        ABS_PHASE_POSITIVE = 3,
        ABS_PHASE_NEGATIVE = 4,
    };


    /*
      A possible fix for a violated piecewise linear constraint: a
      variable whose value should be changed.
    */
    struct Fix
    {
    public:
        Fix( unsigned variable, double value )
            : _variable( variable )
            , _value( value )
        {
        }

        bool operator==( const Fix &other ) const
        {
            return
                _variable == other._variable &&
                FloatUtils::areEqual( _value, other._value );
        }

        unsigned _variable;
        double _value;
    };

    PiecewiseLinearConstraint();
    PiecewiseLinearConstraint( const PiecewiseLinearConstraint &original );
    virtual ~PiecewiseLinearConstraint()
    {
        cdoCleanup();
    }

    bool operator<( const PiecewiseLinearConstraint &other ) const
    {
        return _score < other._score;
    }

    /*
      Get the type of this constraint.
    */
    virtual PiecewiseLinearFunctionType getType() const = 0;

    /*
      Return a clone of the constraint. Allocates CDOs for the copy.
    */
    virtual PiecewiseLinearConstraint *duplicateConstraint() const = 0;

    /*
      Restore the state of this constraint from the given one.
      We have this function in order to take advantage of the polymorphically
      correct assignment operator.
    */
    virtual void restoreState( const PiecewiseLinearConstraint *state ) = 0;

    /*
      Register/unregister the constraint with a tableau.
    */
    virtual void registerAsWatcher( ITableau *tableau ) = 0;
    virtual void unregisterAsWatcher( ITableau *tableau ) = 0;

    /*
      The variable watcher notifcation callbacks, about a change in a variable's value or bounds.
    */
    virtual void notifyVariableValue( unsigned /* variable */, double /* value */ ) {}
    virtual void notifyLowerBound( unsigned /* variable */, double /* bound */ ) {}
    virtual void notifyUpperBound( unsigned /* variable */, double /* bound */ ) {}

    /*
      Turn the constraint on/off.
    */
    virtual void setActiveConstraint( bool active )
    {
        if ( nullptr != _constraintActive )
            *_constraintActive = active;
        else
            throw MarabouError( MarabouError::PIECEWISELINEAR_CONSTRAINT_NOT_PROPERLY_INITIALIZED );
    }

    bool isActive() const
    {
        if ( nullptr != _constraintActive )
            return *_constraintActive;

        else
            throw MarabouError( MarabouError::PIECEWISELINEAR_CONSTRAINT_NOT_PROPERLY_INITIALIZED );

        return true;
    }

    /*
      Returns true iff the variable participates in this piecewise
      linear constraint.
    */
    virtual bool participatingVariable( unsigned variable ) const = 0;

    /*
      Get the list of variables participating in this constraint.
    */
    virtual List<unsigned> getParticipatingVariables() const = 0;

    /*
      Returns true iff the assignment satisfies the constraint.
    */
    virtual bool satisfied() const = 0;

    /*
      Returns a list of possible fixes for the violated constraint.
    */
    virtual List<PiecewiseLinearConstraint::Fix> getPossibleFixes() const = 0;

    /*
      Return a list of smart fixes for violated constraint.
    */
    virtual List<PiecewiseLinearConstraint::Fix> getSmartFixes( ITableau *tableau ) const = 0;

    /*
      Returns the list of case splits that this piecewise linear
      constraint breaks into. These splits need to complementary,
      i.e. if the list is {l1, l2, ..., ln-1, ln},
      then ~l1 /\ ~l2 /\ ... /\ ~ln-1 --> ln.
    */
    virtual List<PiecewiseLinearCaseSplit> getCaseSplits() const = 0;

    /*
     * Returns a list of all cases of this constraint
     * TODO: unsigned -> Phase
     */
    virtual List<unsigned> getAllCases() const = 0;

    /*
     * Returns case split corresponding to the given phase/id
     */
    virtual PiecewiseLinearCaseSplit getCaseSplit( unsigned caseId ) const = 0;

    /*
      Check if the constraint's phase has been fixed.
    */
    virtual bool phaseFixed() const = 0;

    /*
      If the constraint's phase has been fixed, get the (valid) case split.
    */
    virtual PiecewiseLinearCaseSplit getValidCaseSplit() const = 0;

    /*
      Dump the current state of the constraint.
    */
    void dump() const;

    /*
      Dump the current state of the constraint.
    */
    virtual void dump( String & ) const {}

    /*
      Preprocessing related functions, to inform that a variable has been eliminated completely
      because it was fixed to some value, or that a variable's index has changed (e.g., x4 is now
      called x2). constraintObsolete() returns true iff and the constraint has become obsolote
      as a result of variable eliminations.
    */
    virtual void eliminateVariable( unsigned variable, double fixedValue ) = 0;
    virtual void updateVariableIndex( unsigned oldIndex, unsigned newIndex ) = 0;
    virtual bool constraintObsolete() const = 0;

    /*
      Get the tightenings entailed by the constraint.
    */
    virtual void getEntailedTightenings( List<Tightening> &tightenings ) const = 0;

    void setStatistics( Statistics *statistics );

    /*
      For preprocessing: get any auxiliary equations that this constraint would
      like to add to the equation pool.
    */
    virtual void addAuxiliaryEquations( InputQuery &/* inputQuery */ ) {}

    /*
      Ask the piecewise linear constraint to contribute a component to the cost
      function. If implemented, this component should be empty when the constraint is
      satisfied or inactive, and should be non-empty otherwise. Minimizing the returned
      equation should then lead to the constraint being "closer to satisfied".
    */
    virtual void getCostFunctionComponent( Map<unsigned, double> &/* cost */ ) const {}

    /*
      Produce string representation of the piecewise linear constraint.
      This representation contains only the information necessary to reproduce it
      but does not account for state or change in state during execution. Additionally
      the first string before a comma has the contraint type identifier
      (ie. "relu", "max", etc)
    */
    virtual String serializeToString() const = 0;

    /*
      Register a bound manager. If a bound manager is registered,
      this piecewise linear constraint will inform the tightener whenever
      it discovers a tighter (entailed) bound.
    */
    void registerBoundManager( BoundManager *boundManager );

    /*
      Return true if and only if this piecewise linear constraint supports
      symbolic bound tightening.
    */
    virtual bool supportsSymbolicBoundTightening() const
    {
        return false;
    }

    /*
      Return true if and only if this piecewise linear constraint supports
      the polarity metric
    */
    virtual bool supportPolarity() const
    {
        return false;
    }

    /*
      Update the preferred direction to take first when splitting on this PLConstraint
    */
    virtual void updateDirection()
    {
    }

    virtual void updateScore()
    {
    }

    /*
      Update _score with score
    */
    void setScore( double score )
    {
        _score = score;
    }

    /*
      Retrieve the current lower and upper bounds
    */
    double getLowerBound( unsigned i ) const
    {
        return _lowerBounds[i];
    }

    double getUpperBound( unsigned i ) const
    {
        return _upperBounds[i];
    }

    /*
       Register context object. Necessary for lazy backtracking features - such
       as _phaseStatus and _activeStatus. Does not require initialization until
       after pre-processing.
     */
    void initializeCDOs( CVC4::context::Context *context )
    {
        ASSERT( nullptr == _context );
        _context = context;

        initializeActiveStatus();
        initializePhaseStatus();
    }

    /*
       Politely clean up allocated CDOs
     */
    void cdoCleanup()
    {
        if ( nullptr != _constraintActive )
            _constraintActive->deleteSelf();

        _constraintActive= nullptr;

        if ( nullptr != _phaseStatus )
            _phaseStatus->deleteSelf();

        _phaseStatus = nullptr;

        _context = nullptr;
    }

    CVC4::context::Context *getContext() const
    {
        return _context;
    }

    /*
      Get the active status object - debugging purposes only
    */
    CVC4::context::CDO<bool> *getActiveStatusCDO() const
    {
        return _constraintActive;
    };

    /*
      Get the current phase status object - debugging purposes only
    */
    CVC4::context::CDO<PhaseStatus> *getPhaseStatusCDO() const
    {
            return _phaseStatus;
    }

    // TODO: Context-dependent Exploration tracking
    // Add CDList<PhaseStatus> infeasibleCases;
    // Add to initializeCDOs and cdoCleanup
    // void markInfeasible( PhaseStatus exploredCase );
    // PhaseStatus nextFeasibleCase();

    /* void markInfeasible( PhaseStatus infeasibleCase ) */
    /* { */
    /*     _infeasibleCases.pushBack( infeasibleCase ); */
    /* } */

    /* PhaseStatus nextFeasibleCase() //O(n^2) - using size to detect infeasible and implications, flagging the last case could also work */
    /* { */
    /*     //if ( _ind) */
    /* } */

protected:
    CVC4::context::Context *_context;
    CVC4::context::CDO<bool> *_constraintActive;


    /* ReluConstraint and AbsoluteValueConstraint use PhaseStatus enumeration.
       MaxConstraint and Disjunction interpret the PhaseStatus value as the case
       number (counts from 1, value 0 is reserved and used as PHASE_NOT_FIXED).
    */
    CVC4::context::CDO<PhaseStatus> *_phaseStatus;

    //CVC4::context::CDList<PhaseStatus> *_infeasibleCases;

    Map<unsigned, double> _assignment;
    Map<unsigned, double> _lowerBounds;
    Map<unsigned, double> _upperBounds;

    /*
      The score denotes priority for splitting. When score is negative, the PL constraint
      is not being considered for splitting.
      We pick the PL constraint with the highest score to branch.
     */
    double _score;

    BoundManager *_boundManager;
    /*
      Statistics collection
    */
    Statistics *_statistics;

    /*
      Initialize _activeStatus CDO.
    */
    void initializeActiveStatus();

    void setPhaseStatus( PhaseStatus phaseStatus );
    PhaseStatus getPhaseStatus() const;

    /*
      Initialize _phaseStatus. 
    */
    void initializePhaseStatus();

    void initializeDuplicatesCDOs( PiecewiseLinearConstraint *clone ) const;

};

#endif // __PiecewiseLinearConstraint_h__

//
// Local Variables:
// compile-command: "make -C ../.. "
// tags-file-name: "../../TAGS"
// c-basic-offset: 4
// End:
//
