/*********************                                                        */
/*! \file Test_SmtCore.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz, Duligur Ibeling, Derek Huang
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** [[ Add lengthier description here ]]

 **/

#include <cxxtest/TestSuite.h>

#include "context/cdlist.h"
#include "context/context.h"
#include "GlobalConfiguration.h"
#include "MockEngine.h"
#include "MockErrno.h"
#include "PiecewiseLinearConstraint.h"
#include "ReluConstraint.h"
#include "SmtCore.h"

#include <string.h>

using namespace CVC4::context;

class MockForSmtCore
{
public:
};

class SmtCoreTestSuite : public CxxTest::TestSuite
{
public:
    MockForSmtCore *mock;
    MockEngine *engine;

    class MockConstraint : public PiecewiseLinearConstraint
    {
    public:
        MockConstraint()
            : setActiveWasCalled( false )
        {
            nextIsActive = true;
        }

        PiecewiseLinearFunctionType getType() const
        {
            return (PiecewiseLinearFunctionType)999;
        }

        PiecewiseLinearConstraint *duplicateConstraint() const
        {
            return NULL;
        }

        void restoreState( const PiecewiseLinearConstraint */* state */ )
        {
        }

        void registerAsWatcher( ITableau * )
        {
        }

        void unregisterAsWatcher( ITableau * )
        {
        }

        bool setActiveWasCalled;
        void setActiveConstraint( bool active )
        {
            TS_ASSERT( active == false );
            setActiveWasCalled = true;
        }

        bool nextIsActive;
        bool isActive() const
        {
            return nextIsActive;
        }

        bool participatingVariable( unsigned ) const
        {
            return true;
        }

        List<unsigned> getParticipatingVariables() const
        {
            return List<unsigned>();
        }


        bool satisfied() const
        {
            return true;
        }

        List<PiecewiseLinearConstraint::Fix> getPossibleFixes() const
        {
            return List<PiecewiseLinearConstraint::Fix>();
        }

        List<PiecewiseLinearConstraint::Fix> getSmartFixes( ITableau * ) const
        {
            return List<PiecewiseLinearConstraint::Fix>();
        }

        List<PiecewiseLinearCaseSplit> nextSplits;
        List<PiecewiseLinearCaseSplit> getCaseSplits() const
        {
            return nextSplits;
        }

        bool phaseFixed() const
        {
            return false;
        }

        PiecewiseLinearCaseSplit getValidCaseSplit() const
        {
            PiecewiseLinearCaseSplit dontCare;
            return dontCare;
        }

		void updateVariableIndex( unsigned, unsigned )
		{
		}

		void eliminateVariable( unsigned, double )
		{
		}

        bool constraintObsolete() const
        {
            return false;
        }

		void preprocessBounds( unsigned, double, Tightening::BoundType )
		{
		}

        void getEntailedTightenings( List<Tightening> & ) const
        {
        }

        void getAuxiliaryEquations( List<Equation> &/* newEquations */ ) const
        {
        }

        String serializeToString() const
        {
            return "";
        }
    };

    void setUp()
    {
        TS_ASSERT( mock = new MockForSmtCore );
        TS_ASSERT( engine = new MockEngine );
    }

    void tearDown()
    {
        TS_ASSERT_THROWS_NOTHING( delete engine );
        TS_ASSERT_THROWS_NOTHING( delete mock );
    }

    void test_need_to_split()
    {
        ReluConstraint constraint1( 1, 2 );
        ReluConstraint constraint2( 3, 4 );

        Context context;
        SmtCore smtCore( engine, context );

        constraint1.initializeContextDependentPhaseStatus( &context );
        constraint2.initializeContextDependentPhaseStatus( &context );

        for ( unsigned i = 0; i < GlobalConfiguration::CONSTRAINT_VIOLATION_THRESHOLD - 1; ++i )
        {
            smtCore.reportViolatedConstraint( &constraint1 );
            TS_ASSERT( !smtCore.needToSplit() );
            smtCore.reportViolatedConstraint( &constraint2 );
            TS_ASSERT( !smtCore.needToSplit() );
        }

        smtCore.reportViolatedConstraint( &constraint2 );
        TS_ASSERT( smtCore.needToSplit() );
    }

    /*
     * Test - Context push and pop functionality:
     *       1. Start with a fresh context
     *       2. Call context.push() 
     *       3. Check that context level is now one.
     *       4. Call context.pop()
     *       5. Check that context level is now zero. 
     */
    void test_context_level()
    {
        Context context;
        // TODO: casting the context from int to unsigned int for now
        int prev_context_level = context.getLevel();
        context.push();
        TS_ASSERT_EQUALS( 1, context.getLevel() );
        TS_ASSERT_EQUALS( prev_context_level + 1, context.getLevel() );

        prev_context_level = context.getLevel();
        context.pop();
        TS_ASSERT_EQUALS( 0, context.getLevel() );
        TS_ASSERT_EQUALS( prev_context_level -1, context.getLevel() );
    }

    /*
     * Test - Trail push and pop functionality:
     *       1. Start with a fresh context and trail
     *       2. Call context.push(), and push 0 to the trail
     *       3. Call context.push() again, and push 1, 2 to the trail
     *       4. Check that the trail size is 3
     *       5. Call context.pop()
     *       6. Check that the trail size is 1
     *       7. Check that the last element in the trail is 0 
     *       8. Call context.pop()
     *       9. Check that the trail size is 0
     */ 
    void test_trail_push_pop()
    {
        Context context;
        CDList<int> trail( &context );

        context.push();
        trail.push_back( 0 );

        context.push();
        trail.push_back( 1 );
        trail.push_back( 2 );

        TS_ASSERT_EQUALS( trail.size(), 3U );

        context.pop();
        TS_ASSERT_EQUALS( trail.size(), 1U );
        TS_ASSERT_EQUALS( trail.back(), 0 );

        context.pop();
        TS_ASSERT_EQUALS( trail.size(), 0U );

        // Test CDList of PiecewiseLinearCaseSplits


        // Split 1
        PiecewiseLinearCaseSplit split1;
        Tightening bound1( 1, 3.0, Tightening::LB );
        Tightening bound2( 1, 5.0, Tightening::UB );

        Equation equation1( Equation::EQ );
        equation1.addAddend( 1, 0 );
        equation1.addAddend( 2, 1 );
        equation1.addAddend( -1, 2 );
        equation1.setScalar( 11 );

        split1.storeBoundTightening( bound1 );
        split1.storeBoundTightening( bound2 );
        split1.addEquation( equation1 );
        split1.setPhase(0u);

        // Split 2
        PiecewiseLinearCaseSplit split2;
        Tightening bound3( 2, 13.0, Tightening::UB );
        Tightening bound4( 3, 25.0, Tightening::UB );

        Equation equation2( Equation::EQ );
        equation2.addAddend( -3, 0 );
        equation2.addAddend( 3, 1 );
        equation2.setScalar( -5 );

        split2.storeBoundTightening( bound3 );
        split2.storeBoundTightening( bound4 );
        split2.addEquation( equation2 );
        split2.setPhase(1u);

        // Split 3
        PiecewiseLinearCaseSplit split3;
        Tightening bound5( 14, 2.3, Tightening::LB );

        split3.storeBoundTightening( bound5 );
        split3.addEquation( equation1 );
        split3.addEquation( equation2 );
        split3.setPhase(2u);

        CDList<PiecewiseLinearCaseSplit> trailPWCaseSplits( &context );
        context.push();
        trailPWCaseSplits.push_back( split1 );
        // TS_ASSERT_EQUALS( & split1, & (trailPWCaseSplits.back()) )

        context.push();
        trailPWCaseSplits.push_back( split2 );
        trailPWCaseSplits.push_back( split3 );

        TS_ASSERT_EQUALS( trailPWCaseSplits.size(), 3U );

        context.pop();
        TS_ASSERT_EQUALS( trailPWCaseSplits.size(), 1U );
        TS_ASSERT_EQUALS( trailPWCaseSplits.back(), split1 );

        context.pop();
        TS_ASSERT_EQUALS( trailPWCaseSplits.size(), 0U );
    }


    /*
     *  Test - Context and Perform split are in sync:
     *   1. Context level advances with decideSplit
     *   2. TODO: Context level decreases with popSplit
     *   3. TODO: perfromImplication does not affect context level.
     *   N. TODO: Additionally, trail and asserted ReLUs are in sync
     */
    void test_context_perform_interaction()
    {
        Context context;
        SmtCore smtCore( engine, context );

        MockConstraint constraint;

        // Split 1
        PiecewiseLinearCaseSplit split1;
        Tightening bound1( 1, 3.0, Tightening::LB );
        Tightening bound2( 1, 5.0, Tightening::UB );

        Equation equation1( Equation::EQ );
        equation1.addAddend( 1, 0 );
        equation1.addAddend( 2, 1 );
        equation1.addAddend( -1, 2 );
        equation1.setScalar( 11 );

        split1.storeBoundTightening( bound1 );
        split1.storeBoundTightening( bound2 );
        split1.addEquation( equation1 );
        split1.setPhase(0u);

        // Split 2
        PiecewiseLinearCaseSplit split2;
        Tightening bound3( 2, 13.0, Tightening::UB );
        Tightening bound4( 3, 25.0, Tightening::UB );

        Equation equation2( Equation::EQ );
        equation2.addAddend( -3, 0 );
        equation2.addAddend( 3, 1 );
        equation2.setScalar( -5 );

        split2.storeBoundTightening( bound3 );
        split2.storeBoundTightening( bound4 );
        split2.addEquation( equation2 );
        split2.setPhase(1u);

        // Split 3
        PiecewiseLinearCaseSplit split3;
        Tightening bound5( 14, 2.3, Tightening::LB );

        split3.storeBoundTightening( bound5 );
        split3.addEquation( equation1 );
        split3.addEquation( equation2 );

        // Store the splits
        constraint.nextSplits.append( split1 );
        constraint.nextSplits.append( split2 );
        constraint.nextSplits.append( split3 );
        split3.setPhase(2u);

        for ( unsigned i = 0; i < GlobalConfiguration::CONSTRAINT_VIOLATION_THRESHOLD; ++i )
            smtCore.reportViolatedConstraint( &constraint );

        engine->lastStoredState = NULL;
        engine->lastRestoredState = NULL;

        unsigned prev_context_level = context.getLevel();

        // Assert preconditions
        TS_ASSERT( smtCore.needToSplit() );
        TS_ASSERT_EQUALS( 0U, prev_context_level );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        TS_ASSERT( !constraint.setActiveWasCalled );

        // Call decideSplit
        TS_ASSERT_THROWS_NOTHING( smtCore.decideSplit() );

        // Check postconditions
        TS_ASSERT( constraint.setActiveWasCalled );
        TS_ASSERT( !smtCore.needToSplit() );

        // Check that depth has increased, and exactly one step, both for the trail and stack frames
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 1U );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), static_cast<unsigned>( context.getLevel() ) );
        TS_ASSERT_EQUALS( prev_context_level + 1, static_cast<unsigned>( context.getLevel() ) );

        // Check that Split1 was performed and tableau state was stored
        TS_ASSERT_EQUALS( engine->lastLowerBounds.size(), 1U );
        TS_ASSERT_EQUALS( engine->lastLowerBounds.begin()->_variable, 1U );
        TS_ASSERT_EQUALS( engine->lastLowerBounds.begin()->_bound, 3.0 );

        TS_ASSERT_EQUALS( engine->lastUpperBounds.size(), 1U );
        TS_ASSERT_EQUALS( engine->lastUpperBounds.begin()->_variable, 1U );
        TS_ASSERT_EQUALS( engine->lastUpperBounds.begin()->_bound, 5.0 );

        TS_ASSERT_EQUALS( engine->lastEquations.size(), 1U );
        Equation equation4 = equation1;
        TS_ASSERT_EQUALS( *engine->lastEquations.begin(), equation4 );

        TS_ASSERT( engine->lastStoredState );
        TS_ASSERT( !engine->lastRestoredState );

        EngineState *originalState = engine->lastStoredState;
        engine->lastStoredState = NULL;
        engine->lastLowerBounds.clear();
        engine->lastUpperBounds.clear();
        engine->lastEquations.clear();

        // Pop Split1, check that the tableau was restored and that
        // a Split2 was performed
        TS_ASSERT_THROWS_NOTHING( smtCore.backtrackAndContinue() );
        // TODO: Enable after popSplit/decideSplit refactor
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), context.getLevel())
        // TS_ASSERT_THROWS_NOTHING( smtCore.decideSplit() );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 1U );
        TS_ASSERT_EQUALS( static_cast<unsigned>( context.getLevel() ), 1u );

        TS_ASSERT_EQUALS( engine->lastRestoredState, originalState );
        TS_ASSERT( !engine->lastStoredState );
        engine->lastRestoredState = NULL;

        TS_ASSERT( engine->lastLowerBounds.empty() );

        TS_ASSERT_EQUALS( engine->lastUpperBounds.size(), 2U );
        auto it = engine->lastUpperBounds.begin();
        TS_ASSERT_EQUALS( it->_variable, 2U );
        TS_ASSERT_EQUALS( it->_bound, 13.0 );
        ++it;
        TS_ASSERT_EQUALS( it->_variable, 3U );
        TS_ASSERT_EQUALS( it->_bound, 25.0 );

        TS_ASSERT_EQUALS( engine->lastEquations.size(), 1U );
        Equation equation5 = equation2;
        TS_ASSERT_EQUALS( *engine->lastEquations.begin(), equation5 );

        engine->lastRestoredState = NULL;
        engine->lastLowerBounds.clear();
        engine->lastUpperBounds.clear();
        engine->lastEquations.clear();

        // Pop Split2, check that the tableau was restored and that
        // a Split3 was performed
        TS_ASSERT( smtCore.backtrackAndContinue() );
        // TODO: Enable after popSplit/decideSplit refactor
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), context.getLevel())
        // TS_ASSERT_THROWS_NOTHING( smtCore.performImplication() );
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 1U );
        TS_ASSERT( smtCore.getStackDepth() >= static_cast<unsigned>( context.getLevel() ) );


        TS_ASSERT_EQUALS( engine->lastRestoredState, originalState );
        TS_ASSERT( !engine->lastStoredState );
        engine->lastRestoredState = NULL;

        TS_ASSERT_EQUALS( engine->lastLowerBounds.size(), 1U );
        it = engine->lastLowerBounds.begin();
        TS_ASSERT_EQUALS( it->_variable, 14U );
        TS_ASSERT_EQUALS( it->_bound, 2.3 );

        TS_ASSERT( engine->lastUpperBounds.empty() );

        TS_ASSERT_EQUALS( engine->lastEquations.size(), 2U );
        auto equation = engine->lastEquations.begin();
        Equation equation6 = equation1;
        TS_ASSERT_EQUALS( *equation, equation6 );
        ++equation;
        Equation equation7 = equation2;
        TS_ASSERT_EQUALS( *equation, equation7 );

        engine->lastRestoredState = NULL;
        engine->lastLowerBounds.clear();
        engine->lastUpperBounds.clear();
        engine->lastEquations.clear();

        // Final pop
        // Potentially context.pop() and smtCore.popSplit have different semantics
        TS_ASSERT( !smtCore.backtrackAndContinue() );
        TS_ASSERT( !engine->lastRestoredState );
        TS_ASSERT_EQUALS( static_cast<unsigned>( context.getLevel() ), 0U );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        // Stack and trail depth differ at depth zero
        //TS_ASSERT( smtCore.getStackDepth() >= static_cast<unsigned>( context.getLevel() ) );
    }

    /*
     * Test _trail contents:
     *  1. Decide PWL1, check that it is at _context/_trail level 1.
     *  2. Decide PWL2, check that it is at level 2. 
     *  3. Backtrack level 2, check that only PWL1 is on the trail.
     *  4. Propagate PWL2, check that it is asserted at on level 1 and trail consists of PWL1, PWL2
     */
    void test_trail_perform_split()
    {
        Context context;
        SmtCore smtCore( engine, context );

        MockConstraint constraint1;

        // Split 1_1
        PiecewiseLinearCaseSplit split1_1;
        Tightening bound1_1( 1, 3.0, Tightening::LB );
        Tightening bound1_2( 1, 5.0, Tightening::UB );

        Equation equation1_1( Equation::EQ );
        equation1_1.addAddend( 1, 0 );
        equation1_1.addAddend( 2, 1 );
        equation1_1.addAddend( -1, 2 );
        equation1_1.setScalar( 11 );

        split1_1.storeBoundTightening( bound1_1 );
        split1_1.storeBoundTightening( bound1_2 );
        split1_1.addEquation( equation1_1 );
        split1_1.setPhase(0u);

        // Split 1_2
        PiecewiseLinearCaseSplit split1_2;
        Tightening bound1_3( 2, 13.0, Tightening::UB );
        Tightening bound1_4( 3, 25.0, Tightening::UB );

        Equation equation1_2( Equation::EQ );
        equation1_2.addAddend( -3, 0 );
        equation1_2.addAddend( 3, 1 );
        equation1_2.setScalar( -5 );

        split1_2.storeBoundTightening( bound1_3 );
        split1_2.storeBoundTightening( bound1_4 );
        split1_2.addEquation( equation1_2 );
        split1_2.setPhase(1u);

        // Split 1_3
        PiecewiseLinearCaseSplit split1_3;
        Tightening bound1_5( 14, 2.3, Tightening::LB );

        split1_3.storeBoundTightening( bound1_5 );
        split1_3.addEquation( equation1_1 );
        split1_3.addEquation( equation1_2 );
        split1_3.setPhase(2u);

        // Store the splits
        constraint1.nextSplits.append( split1_1 );
        constraint1.nextSplits.append( split1_2 );
        constraint1.nextSplits.append( split1_3 );

        MockConstraint constraint2;

        // Split2_1
        PiecewiseLinearCaseSplit split2_1;
        Tightening bound2_1( 1, -3.0, Tightening::LB );
        Tightening bound2_2( 1, 15.0, Tightening::UB );

        Equation equation2_1( Equation::EQ );
        equation2_1.addAddend( 1.5, 0 );
        equation2_1.addAddend( 2.3, 1 );
        equation2_1.addAddend( -1.1, 2 );
        equation2_1.setScalar( 15 );

        split2_1.storeBoundTightening( bound2_1 );
        split2_1.storeBoundTightening( bound2_2 );
        split2_1.addEquation( equation2_1 );
        split2_1.setPhase(0u);

        // Split2_2
        PiecewiseLinearCaseSplit split2_2;
        Tightening bound2_3( 2, 31.0, Tightening::UB );
        Tightening bound2_4( 3, 52.0, Tightening::UB );

        Equation equation2_2( Equation::EQ );
        equation2_2.addAddend( -3.5, 0 );
        equation2_2.addAddend( 3.5, 1 );
        equation2_2.setScalar( -5.5 );

        split2_2.storeBoundTightening( bound2_3 );
        split2_2.storeBoundTightening( bound2_4 );
        split2_2.addEquation( equation2_2 );
        split2_2.setPhase(1u);

        // split2_3
        PiecewiseLinearCaseSplit split2_3;
        Tightening bound2_5( 14, 2.3, Tightening::LB );

        split2_3.storeBoundTightening( bound2_5 );
        split2_3.addEquation( equation2_1 );
        split2_3.addEquation( equation2_2 );
        split2_3.setPhase(2u);

        // Store the splits
        constraint2.nextSplits.append( split2_1 );
        constraint2.nextSplits.append( split2_2 );
        constraint2.nextSplits.append( split2_3 );

        for ( unsigned i = 0; i < GlobalConfiguration::CONSTRAINT_VIOLATION_THRESHOLD; ++i )
            smtCore.reportViolatedConstraint( &constraint1 );

        engine->lastStoredState = NULL;
        engine->lastRestoredState = NULL;

        unsigned prev_context_level = context.getLevel();

        TS_ASSERT( smtCore.needToSplit() );
        TS_ASSERT_EQUALS( 0U, prev_context_level );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        TS_ASSERT( !constraint1.setActiveWasCalled );
        TS_ASSERT_THROWS_NOTHING( smtCore.decideSplit() );

        List<PiecewiseLinearCaseSplit> allSplitsOnStackSoFar;
        smtCore.allSplitsSoFar( allSplitsOnStackSoFar );

        auto trail = smtCore.trailBegin();
        auto endtrail = smtCore.trailEnd();
        auto splits = allSplitsOnStackSoFar.begin();
        auto endsplits = allSplitsOnStackSoFar.end();
        for ( ; trail != endtrail && splits != endsplits; ++trail, ++splits )
        {
            // TS_ASSERT_EQUALS( *trail , *splits );
        }
        TS_ASSERT_EQUALS( trail, endtrail );
        TS_ASSERT_EQUALS( splits, endsplits );

        TS_ASSERT( constraint1.setActiveWasCalled );
        TS_ASSERT( !smtCore.needToSplit() );


        // Pop Split1, check that the tableau was restored and that
        // a Split2 was performed
        TS_ASSERT_THROWS_NOTHING( smtCore.backtrackAndContinue() );
        // TODO: Enable after popSplit/decideSplit refactor
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), context.getLevel())
        // TS_ASSERT_THROWS_NOTHING( smtCore.decideSplit() );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 1U );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), static_cast<unsigned>( context.getLevel() ) );

        // Pop Split2, check that the tableau was restored and that
        // a Split3 was performed
        TS_ASSERT( smtCore.backtrackAndContinue() );
        // TODO: Enable after popSplit/decideSplit refactor
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), context.getLevel())
        // TS_ASSERT_THROWS_NOTHING( smtCore.performImplication() );
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 1U );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), static_cast<unsigned>( context.getLevel() ) );

        // Final pop
        // Potentially context.pop() and smtCore.popSplit have different semantics
        TS_ASSERT( !smtCore.backtrackAndContinue() );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        //TS_ASSERT_EQUALS( smtCore.getStackDepth(), static_cast<unsigned>( context.getLevel() ) );
    }

    /*
     *  Test - Context and Perform split are in sync:
     *   1. Context level advances with performSplit 
     *   2. TODO: Context level decreases with popSplit
     *   3. TODO: perfromImplciation does not effct context level.
     *   N. TODO: Additionally, trail and asserted ReLUs are in sync
     */
    void test_context_perform_split()
    {
        Context context;
        SmtCore smtCore( engine, context );

        MockConstraint constraint;

        // Split 1
        PiecewiseLinearCaseSplit split1;
        Tightening bound1( 1, 3.0, Tightening::LB );
        Tightening bound2( 1, 5.0, Tightening::UB );

        Equation equation1( Equation::EQ );
        equation1.addAddend( 1, 0 );
        equation1.addAddend( 2, 1 );
        equation1.addAddend( -1, 2 );
        equation1.setScalar( 11 );

        split1.storeBoundTightening( bound1 );
        split1.storeBoundTightening( bound2 );
        split1.addEquation( equation1 );

        // Split 2
        PiecewiseLinearCaseSplit split2;
        Tightening bound3( 2, 13.0, Tightening::UB );
        Tightening bound4( 3, 25.0, Tightening::UB );

        Equation equation2( Equation::EQ );
        equation2.addAddend( -3, 0 );
        equation2.addAddend( 3, 1 );
        equation2.setScalar( -5 );

        split2.storeBoundTightening( bound3 );
        split2.storeBoundTightening( bound4 );
        split2.addEquation( equation2 );

        // Split 3
        PiecewiseLinearCaseSplit split3;
        Tightening bound5( 14, 2.3, Tightening::LB );

        split3.storeBoundTightening( bound5 );
        split3.addEquation( equation1 );
        split3.addEquation( equation2 );

        // Store the splits
        constraint.nextSplits.append( split1 );
        constraint.nextSplits.append( split2 );
        constraint.nextSplits.append( split3 );

        for ( unsigned i = 0; i < GlobalConfiguration::CONSTRAINT_VIOLATION_THRESHOLD; ++i )
            smtCore.reportViolatedConstraint( &constraint );

        engine->lastStoredState = NULL;
        engine->lastRestoredState = NULL;

        unsigned prev_context_level = context.getLevel();

        TS_ASSERT( smtCore.needToSplit() );
        TS_ASSERT_EQUALS( 0U, prev_context_level );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        TS_ASSERT( !constraint.setActiveWasCalled );
        TS_ASSERT_THROWS_NOTHING( smtCore.decideSplit() );
        TS_ASSERT( constraint.setActiveWasCalled );
        TS_ASSERT( !smtCore.needToSplit() );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 1U );

        // Check that Split1 was performed and tableau state was stored
        TS_ASSERT_EQUALS( engine->lastLowerBounds.size(), 1U );
        TS_ASSERT_EQUALS( engine->lastLowerBounds.begin()->_variable, 1U );
        TS_ASSERT_EQUALS( engine->lastLowerBounds.begin()->_bound, 3.0 );

        TS_ASSERT_EQUALS( engine->lastUpperBounds.size(), 1U );
        TS_ASSERT_EQUALS( engine->lastUpperBounds.begin()->_variable, 1U );
        TS_ASSERT_EQUALS( engine->lastUpperBounds.begin()->_bound, 5.0 );

        TS_ASSERT_EQUALS( engine->lastEquations.size(), 1U );
        Equation equation4 = equation1;
        TS_ASSERT_EQUALS( *engine->lastEquations.begin(), equation4 );

        TS_ASSERT( engine->lastStoredState );
        TS_ASSERT( !engine->lastRestoredState );

        EngineState *originalState = engine->lastStoredState;
        engine->lastStoredState = NULL;
        engine->lastLowerBounds.clear();
        engine->lastUpperBounds.clear();
        engine->lastEquations.clear();

        // Pop Split1, check that the tableau was restored and that
        // a Split2 was performed
        TS_ASSERT( smtCore.backtrackAndContinue() );
        // TODO: Enable after popSplit/performSplit refactor
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), context.getLevel())
        // TS_ASSERT_THROWS_NOTHING( smtCore.performSplit() );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 1U );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), static_cast<unsigned>( context.getLevel() ) );

        TS_ASSERT_EQUALS( engine->lastRestoredState, originalState );
        TS_ASSERT( !engine->lastStoredState );
        engine->lastRestoredState = NULL;

        TS_ASSERT( engine->lastLowerBounds.empty() );

        TS_ASSERT_EQUALS( engine->lastUpperBounds.size(), 2U );
        auto it = engine->lastUpperBounds.begin();
        TS_ASSERT_EQUALS( it->_variable, 2U );
        TS_ASSERT_EQUALS( it->_bound, 13.0 );
        ++it;
        TS_ASSERT_EQUALS( it->_variable, 3U );
        TS_ASSERT_EQUALS( it->_bound, 25.0 );

        TS_ASSERT_EQUALS( engine->lastEquations.size(), 1U );
        Equation equation5 = equation2;
        TS_ASSERT_EQUALS( *engine->lastEquations.begin(), equation5 );

        engine->lastRestoredState = NULL;
        engine->lastLowerBounds.clear();
        engine->lastUpperBounds.clear();
        engine->lastEquations.clear();

        // Pop Split2, check that the tableau was restored and that
        // a Split3 was performed
        TS_ASSERT( smtCore.backtrackAndContinue() );
        // TODO: Enable after popSplit/performSplit refactor
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), context.getLevel())
        // TS_ASSERT_THROWS_NOTHING( smtCore.performImplication() );
        // TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 1U );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), static_cast<unsigned>( context.getLevel() ) );


        TS_ASSERT_EQUALS( engine->lastRestoredState, originalState );
        TS_ASSERT( !engine->lastStoredState );
        engine->lastRestoredState = NULL;

        TS_ASSERT_EQUALS( engine->lastLowerBounds.size(), 1U );
        it = engine->lastLowerBounds.begin();
        TS_ASSERT_EQUALS( it->_variable, 14U );
        TS_ASSERT_EQUALS( it->_bound, 2.3 );

        TS_ASSERT( engine->lastUpperBounds.empty() );

        TS_ASSERT_EQUALS( engine->lastEquations.size(), 2U );
        auto equation = engine->lastEquations.begin();
        Equation equation6 = equation1;
        TS_ASSERT_EQUALS( *equation, equation6 );
        ++equation;
        Equation equation7 = equation2;
        TS_ASSERT_EQUALS( *equation, equation7 );

        engine->lastRestoredState = NULL;
        engine->lastLowerBounds.clear();
        engine->lastUpperBounds.clear();
        engine->lastEquations.clear();

        // Final pop
        // Potentially context.pop() and smtCore.popSplit have different semantics
        TS_ASSERT( !smtCore.backtrackAndContinue() );
        TS_ASSERT( !engine->lastRestoredState );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), static_cast<unsigned>( context.getLevel() ) );
    }

    void test_perform_split()
    {
        Context context;
        SmtCore smtCore( engine, context );
        
        MockConstraint constraint;

        // Split 1
        PiecewiseLinearCaseSplit split1;
        Tightening bound1( 1, 3.0, Tightening::LB );
        Tightening bound2( 1, 5.0, Tightening::UB );

        Equation equation1( Equation::EQ );
        equation1.addAddend( 1, 0 );
        equation1.addAddend( 2, 1 );
        equation1.addAddend( -1, 2 );
        equation1.setScalar( 11 );

        split1.storeBoundTightening( bound1 );
        split1.storeBoundTightening( bound2 );
        split1.addEquation( equation1 );

        // Split 2
        PiecewiseLinearCaseSplit split2;
        Tightening bound3( 2, 13.0, Tightening::UB );
        Tightening bound4( 3, 25.0, Tightening::UB );

        Equation equation2( Equation::EQ );
        equation2.addAddend( -3, 0 );
        equation2.addAddend( 3, 1 );
        equation2.setScalar( -5 );

        split2.storeBoundTightening( bound3 );
        split2.storeBoundTightening( bound4 );
        split2.addEquation( equation2 );

        // Split 3
        PiecewiseLinearCaseSplit split3;
        Tightening bound5( 14, 2.3, Tightening::LB );

        split3.storeBoundTightening( bound5 );
        split3.addEquation( equation1 );
        split3.addEquation( equation2 );

        // Store the splits
        constraint.nextSplits.append( split1 );
        constraint.nextSplits.append( split2 );
        constraint.nextSplits.append( split3 );

        for ( unsigned i = 0; i < GlobalConfiguration::CONSTRAINT_VIOLATION_THRESHOLD; ++i )
            smtCore.reportViolatedConstraint( &constraint );

        engine->lastStoredState = NULL;
        engine->lastRestoredState = NULL;

        TS_ASSERT( smtCore.needToSplit() );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
        TS_ASSERT( !constraint.setActiveWasCalled );
        TS_ASSERT_THROWS_NOTHING( smtCore.decideSplit() );
        TS_ASSERT( constraint.setActiveWasCalled );
        TS_ASSERT( !smtCore.needToSplit() );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 1U );

        // Check that Split1 was performed and tableau state was stored
        TS_ASSERT_EQUALS( engine->lastLowerBounds.size(), 1U );
        TS_ASSERT_EQUALS( engine->lastLowerBounds.begin()->_variable, 1U );
        TS_ASSERT_EQUALS( engine->lastLowerBounds.begin()->_bound, 3.0 );

        TS_ASSERT_EQUALS( engine->lastUpperBounds.size(), 1U );
        TS_ASSERT_EQUALS( engine->lastUpperBounds.begin()->_variable, 1U );
        TS_ASSERT_EQUALS( engine->lastUpperBounds.begin()->_bound, 5.0 );

        TS_ASSERT_EQUALS( engine->lastEquations.size(), 1U );
        Equation equation4 = equation1;
        TS_ASSERT_EQUALS( *engine->lastEquations.begin(), equation4 );

        TS_ASSERT( engine->lastStoredState );
        TS_ASSERT( !engine->lastRestoredState );

        EngineState *originalState = engine->lastStoredState;
        engine->lastStoredState = NULL;
        engine->lastLowerBounds.clear();
        engine->lastUpperBounds.clear();
        engine->lastEquations.clear();

        // Pop Split1, check that the tableau was restored and that
        // a Split2 was performed
        TS_ASSERT( smtCore.backtrackAndContinue() );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 1U );

        TS_ASSERT_EQUALS( engine->lastRestoredState, originalState );
        TS_ASSERT( !engine->lastStoredState );
        engine->lastRestoredState = NULL;

        TS_ASSERT( engine->lastLowerBounds.empty() );

        TS_ASSERT_EQUALS( engine->lastUpperBounds.size(), 2U );
        auto it = engine->lastUpperBounds.begin();
        TS_ASSERT_EQUALS( it->_variable, 2U );
        TS_ASSERT_EQUALS( it->_bound, 13.0 );
        ++it;
        TS_ASSERT_EQUALS( it->_variable, 3U );
        TS_ASSERT_EQUALS( it->_bound, 25.0 );

        TS_ASSERT_EQUALS( engine->lastEquations.size(), 1U );
        Equation equation5 = equation2;
        TS_ASSERT_EQUALS( *engine->lastEquations.begin(), equation5 );

        engine->lastRestoredState = NULL;
        engine->lastLowerBounds.clear();
        engine->lastUpperBounds.clear();
        engine->lastEquations.clear();

        // Pop Split2, check that the tableau was restored and that
        // a Split3 was performed
        TS_ASSERT( smtCore.backtrackAndContinue() );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 1U );

        TS_ASSERT_EQUALS( engine->lastRestoredState, originalState );
        TS_ASSERT( !engine->lastStoredState );
        engine->lastRestoredState = NULL;

        TS_ASSERT_EQUALS( engine->lastLowerBounds.size(), 1U );
        it = engine->lastLowerBounds.begin();
        TS_ASSERT_EQUALS( it->_variable, 14U );
        TS_ASSERT_EQUALS( it->_bound, 2.3 );

        TS_ASSERT( engine->lastUpperBounds.empty() );

        TS_ASSERT_EQUALS( engine->lastEquations.size(), 2U );
        auto equation = engine->lastEquations.begin();
        Equation equation6 = equation1;
        TS_ASSERT_EQUALS( *equation, equation6 );
        ++equation;
        Equation equation7 = equation2;
        TS_ASSERT_EQUALS( *equation, equation7 );

        engine->lastRestoredState = NULL;
        engine->lastLowerBounds.clear();
        engine->lastUpperBounds.clear();
        engine->lastEquations.clear();

        // Final pop
        TS_ASSERT( !smtCore.backtrackAndContinue() );
        TS_ASSERT( !engine->lastRestoredState );
        TS_ASSERT_EQUALS( smtCore.getStackDepth(), 0U );
    }

    void test_perform_split__inactive_constraint()
    {
        Context context;
        SmtCore smtCore( engine, context );

        MockConstraint constraint;

        // Split 1
        PiecewiseLinearCaseSplit split1;
        Tightening bound1( 1, 3.0, Tightening::LB );
        Tightening bound2( 1, 5.0, Tightening::UB );

        Equation equation1( Equation::EQ );
        equation1.addAddend( 1, 0 );
        equation1.addAddend( 2, 1 );
        equation1.addAddend( -1, 2 );
        equation1.setScalar( 11 );

        split1.storeBoundTightening( bound1 );
        split1.storeBoundTightening( bound2 );
        split1.addEquation( equation1 );

        // Split 2
        PiecewiseLinearCaseSplit split2;
        Tightening bound3( 2, 13.0, Tightening::UB );
        Tightening bound4( 3, 25.0, Tightening::UB );

        Equation equation2( Equation::EQ );
        equation2.addAddend( -3, 0 );
        equation2.addAddend( 3, 1 );
        equation2.setScalar( -5 );

        split2.storeBoundTightening( bound3 );
        split2.storeBoundTightening( bound4 );
        split2.addEquation( equation2 );

        // Split 3
        PiecewiseLinearCaseSplit split3;
        Tightening bound5( 14, 2.3, Tightening::LB );

        split3.storeBoundTightening( bound5 );
        split3.addEquation( equation1 );
        split3.addEquation( equation2 );

        // Store the splits
        constraint.nextSplits.append( split1 );
        constraint.nextSplits.append( split2 );
        constraint.nextSplits.append( split3 );

        for ( unsigned i = 0; i < GlobalConfiguration::CONSTRAINT_VIOLATION_THRESHOLD; ++i )
            smtCore.reportViolatedConstraint( &constraint );

        constraint.nextIsActive = false;

        TS_ASSERT( smtCore.needToSplit() );
        TS_ASSERT_THROWS_NOTHING( smtCore.decideSplit() );
        TS_ASSERT( !smtCore.needToSplit() );

        // Check that no split was performed

        TS_ASSERT( engine->lastLowerBounds.empty() );
        TS_ASSERT( engine->lastUpperBounds.empty() );
        TS_ASSERT( engine->lastEquations.empty() );
        TS_ASSERT( !engine->lastStoredState );
    }

    void test_all_splits_so_far()
    {
        Context context;
        SmtCore smtCore( engine, context );

        MockConstraint constraint;

        // Split 1
        PiecewiseLinearCaseSplit split1;
        Tightening bound1( 1, 3.0, Tightening::LB );
        Tightening bound2( 1, 5.0, Tightening::UB );

        Equation equation1( Equation::EQ );
        equation1.addAddend( 1, 0 );
        equation1.addAddend( 2, 1 );
        equation1.addAddend( -1, 2 );
        equation1.setScalar( 11 );

        split1.storeBoundTightening( bound1 );
        split1.storeBoundTightening( bound2 );
        split1.addEquation( equation1 );

        // Split 2
        PiecewiseLinearCaseSplit split2;
        Tightening bound3( 2, 13.0, Tightening::UB );
        Tightening bound4( 3, 25.0, Tightening::UB );

        Equation equation2( Equation::EQ );
        equation2.addAddend( -3, 0 );
        equation2.addAddend( 3, 1 );
        equation2.setScalar( -5 );

        split2.storeBoundTightening( bound3 );
        split2.storeBoundTightening( bound4 );
        split2.addEquation( equation2 );

        // Store the splits
        constraint.nextSplits.append( split1 );
        constraint.nextSplits.append( split2 );

        for ( unsigned i = 0; i < GlobalConfiguration::CONSTRAINT_VIOLATION_THRESHOLD; ++i )
            smtCore.reportViolatedConstraint( &constraint );

        constraint.nextIsActive = true;

        TS_ASSERT( smtCore.needToSplit() );
        TS_ASSERT_THROWS_NOTHING( smtCore.decideSplit() );
        TS_ASSERT( !smtCore.needToSplit() );

        // Register a valid split

        // Split 3
        MockConstraint impliedConstraint;
        PiecewiseLinearCaseSplit split3;
        Tightening bound5( 14, 2.3, Tightening::LB );

        split3.storeBoundTightening( bound5 );
        // TS_ASSERT_THROWS_NOTHING( smtCore.implyCaseSplit( split3 ) );

        // Do another real split

        MockConstraint constraint2;

        // Split 4
        PiecewiseLinearCaseSplit split4;
        Tightening bound6( 7, 3.0, Tightening::LB );
        split4.storeBoundTightening( bound6 );

        PiecewiseLinearCaseSplit split5;
        Tightening bound7( 8, 13.0, Tightening::UB );
        split5.storeBoundTightening( bound7 );

        constraint2.nextSplits.append( split4 );
        constraint2.nextSplits.append( split5 );

        for ( unsigned i = 0; i < GlobalConfiguration::CONSTRAINT_VIOLATION_THRESHOLD; ++i )
            smtCore.reportViolatedConstraint( &constraint2 );

        constraint2.nextIsActive = true;

        TS_ASSERT( smtCore.needToSplit() );
        TS_ASSERT_THROWS_NOTHING( smtCore.decideSplit() );
        TS_ASSERT( !smtCore.needToSplit() );

        // Check that everything is received in the correct order
        List<PiecewiseLinearCaseSplit> allSplitsSoFar;
        TS_ASSERT_THROWS_NOTHING( smtCore.allSplitsSoFar( allSplitsSoFar ) );

        TS_ASSERT_EQUALS( allSplitsSoFar.size(), 2U ); // TODO: restore to 3, when the implication is fixed

        auto it = allSplitsSoFar.begin();
        TS_ASSERT_EQUALS( *it, split1 );

        //++it;
        //TS_ASSERT_EQUALS( *it, split3 );

        ++it;
        TS_ASSERT_EQUALS( *it, split4 );
    }

    void test_todo()
    {
        // Reason: the inefficiency in resizing the tableau mutliple times
        TS_TRACE( "add support for adding multiple equations at once, not one-by-one" );
    }
};

//
// Local Variables:
// compile-command: "make -C ../../.. "
// tags-file-name: "../../../TAGS"
// c-basic-offset: 4
// End:
//
