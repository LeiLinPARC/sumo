/****************************************************************************/
/// @file    ROMAEdge.cpp
/// @author  Daniel Krajzewicz
/// @author  Jakob Erdmann
/// @author  Christian Roessel
/// @author  Michael Behrisch
/// @date    Sept 2002
/// @version $Id: ROMAEdge.cpp 3359 2014-02-22 09:38:00Z behr_mi $
///
// A basic edge for routing applications
/****************************************************************************/
// SUMO, Simulation of Urban MObility; see http://sumo.sourceforge.net/
// Copyright (C) 2001-2014 DLR (http://www.dlr.de/) and contributors
/****************************************************************************/
//
//   This file is part of SUMO.
//   SUMO is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
/****************************************************************************/


// ===========================================================================
// included modules
// ===========================================================================
#ifdef _MSC_VER
#include <windows_config.h>
#else
#include <config.h>
#endif

#include "ROMAEdge.h"

#ifdef CHECK_MEMORY_LEAKS
#include <foreign/nvwa/debug_new.h>
#endif // CHECK_MEMORY_LEAKS


// ===========================================================================
// method definitions
// ===========================================================================
ROMAEdge::ROMAEdge(const std::string& id, RONode* from, RONode* to, unsigned int index, const int priority)
    : ROEdge(id, from, to, index, priority), myHelpFlow(0.) {
}


ROMAEdge::~ROMAEdge() {
}


void
ROMAEdge::addSuccessor(ROEdge* s, std::string dir) {
    ROEdge::addSuccessor(s, dir);
    if (dir == "l" || dir == "L") {
        myLeftTurns.insert(static_cast<ROMAEdge*>(s));
    }
}


/****************************************************************************/
