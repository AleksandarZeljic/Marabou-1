/*********************                                                        */
/*! \file BoundManager.h
 ** \verbatim
 ** Top contributors (to current version):
 **  Aleksandar Zeljic, Haoze Wu,
 ** This file is part of the Marabou project.
 ** Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** BoundManager class is a context-dependent implementation of a centralized
 ** variable registry and their bounds. The intent it so use a single
 ** BoundManager object between multiple bound tightener classes, which enables
 ** those classes to care only about bounds and forget about book-keeping.
 **
 ** BoundManager provides a method to obtain a new variable with:
 ** registerNewVariable().
 **
 ** The bound values and tighten flags are stored using context-dependent objects,
 ** which backtrack automatically with the central _context object.
 **
 ** There are two sets of methods to set bounds:
 **   * set*Bounds     - local method used to update bounds
 **   * tighten*Bounds - shared method to update bounds, propagates the new bounds
 **                        to the _tableau (if registered) to keep the assignment and
 **                      basic/non-basic variables updated accordingly.
 **
 ** As soon as bounds become inconsistent, i.e. lowerBound > upperBound, an
 ** InfeasableQueryException is thrown. In the long run, we want the exception
 ** replaced by a flag, and switch to the conflict analysis mode instead.
 **
 ** It is assumed that variables are not introduced on the fly, and as such
 ** interaction with context-dependent features is not implemented.
 **/

#ifndef __BoundManager_h__
#define __BoundManager_h__

#include "IBoundManager.h"
#include "ITableau.h"
#include "List.h"
#include "Tightening.h"
#include "Vector.h"
#include "context/cdo.h"
#include "context/cdhashmap.h"
#include "context/context.h"

class ITableau;
class BoundManager : public IBoundManager
{
public:
    BoundManager( CVC4::context::Context &ctx );
    ~BoundManager();

    /*
       Initialize BoundManager and register numberOfVariables of variables
     */
    void initialize( unsigned numberOfVariables );

    /*
       Registers a new variable, grows the BoundManager size and bound vectors,
       initializes new bounds to +/-inf, and returns the index of the new
       variable.
     */
    unsigned registerNewVariable();

    /*
       Returns number of registered variables
     */
    unsigned getNumberOfVariables() const;

    /*
       Communicates bounds to the bound Manager and informs _tableau of the
       changes, so that any necessary updates can be performed.
     */
    bool tightenLowerBound( unsigned variable, double value );
    bool tightenUpperBound( unsigned variable, double value );

    /*
       Silently sets bounds to the assigned value, checks bound consistency.
     */
    bool setLowerBound( unsigned variable, double value );
    bool setUpperBound( unsigned variable, double value );

    /*
       Return current bound value.
     */
    double getLowerBound( unsigned variable ) const;
    double getUpperBound( unsigned variable ) const;

    /*
      Get pointers to latest bounds used for access by tableau and tighteners
     */
    const double * getLowerBounds() const;
    const double * getUpperBounds() const;

    /*
       Store and restore local bounds after context advances/backtracks.
     */
    void storeLocalBounds();
    void restoreLocalBounds();
    void clearLocalBoundsHashMap();

    /*
       Obtain a list of all the bound updates since the last call to
       getTightenings.
     */
    void getTightenings( List<Tightening> &tightenings );

    /*
      Returns true if the bounds of all variables are valid. Returns false in a conflict state.
    */
    bool consistentBounds() const;

    /*
      Returns true if the bounds for the variable is valid, used to
      detect a conflict state.
    */
    bool consistentBounds( unsigned variable ) const;
    /*
       Register Tableau reference for callbacks from tighten*Bound methods.
     */
    void registerTableau( ITableau *tableau );

private:
    CVC4::context::Context &_context; // SmtCore's context that synchronizes the global search
    CVC4::context::Context _myContext; // Local context, slightly shifted w.r.t to global context to allow backtracking local updates.
    unsigned _size;
    unsigned _allocated;
    ITableau *_tableau; // Used only by callbacks

    CVC4::context::CDO<bool> _consistentBounds;
    Tightening _firstInconsistentTightening;

    double * _lowerBounds;
    double * _upperBounds;
    CVC4::context::CDHashMap<unsigned,bool> _copyUpper;
    CVC4::context::CDHashMap<unsigned,bool> _copyLower;


    Vector<CVC4::context::CDO<double> *> _storedLowerBounds;
    Vector<CVC4::context::CDO<double> *> _storedUpperBounds;

    Vector<CVC4::context::CDO<bool> *> _tightenedLower;
    Vector<CVC4::context::CDO<bool> *> _tightenedUpper;

    /*
       Record first tightening that violates bounds
     */
    void recordInconsistentBound( unsigned variable, double value, Tightening::BoundType type );

    void allocateLocalBounds( unsigned size );

};

#endif // __BoundManager_h__
