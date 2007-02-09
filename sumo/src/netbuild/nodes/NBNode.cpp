/****************************************************************************/
/// @file    NBNode.cpp
/// @author  Daniel Krajzewicz
/// @date    Tue, 20 Nov 2001
/// @version $Id: $
///
// The representation of a single node
/****************************************************************************/
// SUMO, Simulation of Urban MObility; see http://sumo.sourceforge.net/
// copyright : (C) 2001-2007
//  by DLR (http://www.dlr.de/) and ZAIK (http://www.zaik.uni-koeln.de/AFS)
/****************************************************************************/
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
/****************************************************************************/
// ===========================================================================
// compiler pragmas
// ===========================================================================
#ifdef _MSC_VER
#pragma warning(disable: 4786)
#endif


// ===========================================================================
// included modules
// ===========================================================================
#ifdef WIN32
#include <windows_config.h>
#else
#include <config.h>
#endif

#include <string>
#include <map>
#include <cassert>
#include <algorithm>
#include <vector>
#include <deque>
#include <iostream>
#include <set>
#include <cmath>
#include <utils/common/UtilExceptions.h>
#include <utils/common/StringUtils.h>
#include <utils/options/OptionsCont.h>
#include <utils/options/OptionsSubSys.h>
#include <utils/geom/Line2D.h>
#include <utils/geom/GeomHelper.h>
#include <utils/geom/bezier.h>
#include <utils/common/MsgHandler.h>
#include <utils/common/StdDefs.h>
#include <utils/common/ToString.h>
#include <iomanip>
#include "NBNode.h"
#include "NBNodeCont.h"
#include "NBNodeShapeComputer.h"
#include <netbuild/NBEdgeCont.h>
#include <netbuild/NBTypeCont.h>
#include <netbuild/NBJunctionLogicCont.h>
#include <netbuild/NBHelpers.h>
#include <netbuild/NBDistrict.h>
#include <netbuild/NBContHelper.h>
#include <netbuild/NBLogicKeyBuilder.h>
#include <netbuild/NBRequest.h>
#include <netbuild/NBOwnTLDef.h>
#include <netbuild/NBTrafficLightLogicCont.h>

#ifdef _DEBUG
#include <utils/dev/debug_new.h>
#endif // _DEBUG


// ===========================================================================
// used namespaces
// ===========================================================================
using namespace std;


// ===========================================================================
// some constant defnitions
// ===========================================================================
/** Definition how many points an internal lane-geometry should be made of */
#define NO_INTERNAL_POINTS 5
#define DEBUG_OUT cout


// ===========================================================================
// static variable definitions
// ===========================================================================
int NBNode::_noDistricts = 0;
int NBNode::_noUnregulatedJunctions = 0;
int NBNode::_noPriorityJunctions = 0;
int NBNode::_noRightBeforeLeftJunctions = 0;


// ===========================================================================
// method definitions
// ===========================================================================
/* -------------------------------------------------------------------------
 * NBNode::ApproachingDivider-methods
 * ----------------------------------------------------------------------- */
NBNode::ApproachingDivider::ApproachingDivider(
    std::vector<NBEdge*> *approaching, NBEdge *currentOutgoing)
        : _approaching(approaching), _currentOutgoing(currentOutgoing)
{
    // check whether origin lanes have been given
    assert(_approaching!=0);
}


NBNode::ApproachingDivider::~ApproachingDivider()
{}


void
NBNode::ApproachingDivider::execute(SUMOReal src, SUMOReal dest)
{
    assert(_approaching->size()>src);
    // get the origin edge
    NBEdge *incomingEdge = (*_approaching)[(int) src];
    if (incomingEdge->getStep()==NBEdge::LANES2LANES) {
        return;
    }
    vector<size_t> approachingLanes =
        incomingEdge->getConnectionLanes(_currentOutgoing);
    assert(approachingLanes.size()!=0);
    deque<size_t> *approachedLanes = spread(approachingLanes, dest);
    assert(approachedLanes->size()<=_currentOutgoing->getNoLanes());
    // set lanes
    for (size_t i=0; i<approachedLanes->size(); i++) {
        size_t approached = (*approachedLanes)[i];
        assert(approachedLanes->size()>i);
        assert(approachingLanes.size()>i);
        incomingEdge->setConnection(approachingLanes[i], _currentOutgoing,
                                    approached, true);
    }
    delete approachedLanes;
}


deque<size_t> *
NBNode::ApproachingDivider::spread(const vector<size_t> &approachingLanes,
                                   SUMOReal dest) const
{
    deque<size_t> *ret = new deque<size_t>();
    size_t noLanes = approachingLanes.size();
    // when only one lane is approached, we check, whether the SUMOReal-value
    //  is assigned more to the left or right lane
    if (noLanes==1) {
        if ((size_t)(dest+0.5)>(size_t) dest) {
            ret->push_back((size_t) dest+1);
        } else {
            ret->push_back((size_t) dest);
        }
        return ret;
    }

    size_t noOutgoingLanes = _currentOutgoing->getNoLanes();
    //
    ret->push_back((size_t) dest);
    size_t noSet = 1;
    SUMOReal roffset = 1;
    SUMOReal loffset = 1;
    while (noSet<noLanes) {
        // It may be possible, that there are not enough lanes the source
        //  lanes may be divided on
        //  In this case, they remain unset
        //  !!! this is only a hack. It is possible, that this yields in
        //   uncommon divisions
        if (noOutgoingLanes==noSet)
            return ret;

        // as due to the conversion of SUMOReal->uint the numbers will be lower
        //  than they should be, we try to append to the left side first
        //
        // check whether the left boundary of the approached street has
        //  been overridden; if so, move all lanes to the right
        if (((size_t) dest+loffset)>=noOutgoingLanes) {
            loffset -= 1;
            roffset += 1;
            for (size_t i=0; i<ret->size(); i++) {
                (*ret)[i] = (*ret)[i] - 1;
            }
        }
        // append the next lane to the left of all edges
        //  increase the position (destination edge)
        ret->push_back((size_t)(dest+loffset));
        noSet++;
        loffset += 1;

        // as above
        if (noOutgoingLanes==noSet)
            return ret;

        // now we try to append the next lane to the right side, when needed
        if (noSet<noLanes) {
            // check whether the right boundary of the approached street has
            //  been overridden; if so, move all lanes to the right
            if (((size_t) dest-roffset)<0) {
                loffset += 1;
                roffset -= 1;
                for (size_t i=0; i<ret->size(); i++) {
                    (*ret)[i] = (*ret)[i] + 1;
                }
            }
            ret->push_front((size_t)(dest-roffset));
            noSet++;
            roffset += 1;
        }
    }
    return ret;
}




/* -------------------------------------------------------------------------
 * NBNode-methods
 * ----------------------------------------------------------------------- */
NBNode::NBNode(const string &id, const Position2D &position)
        : _id(StringUtils::convertUmlaute(id)), myPosition(position),
        _type(NODETYPE_UNKNOWN), myDistrict(0), _request(0)
{
    _incomingEdges = new EdgeVector();
    _outgoingEdges = new EdgeVector();
}


NBNode::NBNode(const string &id, const Position2D &position,
               BasicNodeType type)
        : _id(StringUtils::convertUmlaute(id)), myPosition(position),
        _type(type), myDistrict(0), _request(0)
{
    _incomingEdges = new EdgeVector();
    _outgoingEdges = new EdgeVector();
}


NBNode::NBNode(const string &id, const Position2D &position, NBDistrict *district)
        : _id(StringUtils::convertUmlaute(id)), myPosition(position),
        _type(NODETYPE_DISTRICT), myDistrict(district), _request(0)
{
    _incomingEdges = new EdgeVector();
    _outgoingEdges = new EdgeVector();
}


NBNode::~NBNode()
{
    delete _incomingEdges;
    delete _outgoingEdges;
    delete _request;
}


Position2D
NBNode::getPosition() const
{
    return myPosition;
}




void
NBNode::addIncomingEdge(NBEdge *edge)
{
    assert(edge!=0);
    if (find(_incomingEdges->begin(), _incomingEdges->end(), edge)
            ==_incomingEdges->end()) {

        _incomingEdges->push_back(edge);
        for (std::vector<NBEdge*>::iterator i=_outgoingEdges->begin(); i!=_outgoingEdges->end(); ++i) {
            if ((*i)->getToNode()==edge->getFromNode()) {
                (*i)->setLaneSpreadFunction(NBEdge::LANESPREAD_RIGHT);
                edge->setLaneSpreadFunction(NBEdge::LANESPREAD_RIGHT);
            }
        }
    }
}


void
NBNode::addOutgoingEdge(NBEdge *edge)
{
    assert(edge!=0);
    if (find(_outgoingEdges->begin(), _outgoingEdges->end(), edge)
            ==_outgoingEdges->end()) {

        _outgoingEdges->push_back(edge);
        for (std::vector<NBEdge*>::iterator i=_incomingEdges->begin(); i!=_incomingEdges->end(); ++i) {
            if ((*i)->getFromNode()==edge->getToNode()) {
                (*i)->setLaneSpreadFunction(NBEdge::LANESPREAD_RIGHT);
                edge->setLaneSpreadFunction(NBEdge::LANESPREAD_RIGHT);
            }
        }
    }
}


const std::string &
NBNode::getID() const
{
    return _id;
}


const EdgeVector &
NBNode::getIncomingEdges() const
{
    return *_incomingEdges;
}


const EdgeVector &
NBNode::getOutgoingEdges() const
{
    return *_outgoingEdges;
}


const EdgeVector &
NBNode::getEdges() const
{
    return _allEdges;
}


void
NBNode::buildList()
{
    copy(_incomingEdges->begin(), _incomingEdges->end(),
         back_inserter(_allEdges));
    copy(_outgoingEdges->begin(), _outgoingEdges->end(),
         back_inserter(_allEdges));
}


bool
NBNode::swapWhenReversed(const vector<NBEdge*>::iterator &i1,
                         const vector<NBEdge*>::iterator &i2)
{
    NBEdge *e1 = *i1;
    NBEdge *e2 = *i2;
    if (e2->getToNode()==this && e2->isTurningDirectionAt(this, e1)) {
        swap(*i1, *i2);
        return true;
    }
    return false;
}

void
NBNode::setPriorities()
{
    // reset all priorities
    vector<NBEdge*>::iterator i;
    // check if the junction is not a real junction
    if (_incomingEdges->size()==1&&_outgoingEdges->size()==1) {
        for (i=_allEdges.begin(); i!=_allEdges.end(); i++) {
            (*i)->setJunctionPriority(this, 1);
        }
        return;
    }
    // preset all junction's edge priorities to zero
    for (i=_allEdges.begin(); i!=_allEdges.end(); i++) {
        (*i)->setJunctionPriority(this, 0);
    }
    // compute the priorities on junction when needed
    if (_type!=NODETYPE_RIGHT_BEFORE_LEFT) {
        setPriorityJunctionPriorities();
    }
}


NBNode::BasicNodeType
NBNode::computeType(const NBTypeCont &tc) const
{
    // the type may already be set from the data
    if (_type!=NODETYPE_UNKNOWN) {
        return _type;
    }
    // check whether the junction is not a real junction
    if (_incomingEdges->size()==1) {
        return NODETYPE_PRIORITY_JUNCTION;
    }
    // check whether the junction is a district and has no
    //  special meaning
    if (isDistrictCenter()) {
        return NODETYPE_NOJUNCTION;
    }
    if (isSimpleContinuation()) {
        return NODETYPE_PRIORITY_JUNCTION;
    }
    // choose the uppermost type as default
    BasicNodeType type = NODETYPE_RIGHT_BEFORE_LEFT;
    // determine the type
    for (vector<NBEdge*>::const_iterator i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
        for (vector<NBEdge*>::const_iterator j=i+1; j!=_incomingEdges->end(); j++) {
            bool isOpposite = false;
            if (getOppositeIncoming(*j)==*i&&_incomingEdges->size()>2) {
                isOpposite = true;
            }

            // This usage of defaults is not very well, still we do not have any
            //  methods for the conversion of foreign, sometimes not supplied
            //  road types into an own format
            BasicNodeType tmptype = type;
            if (!isOpposite) {
                tmptype = tc.getJunctionType((*i)->getSpeed(), (*j)->getSpeed());
                if (tmptype<type&&tmptype!=NODETYPE_UNKNOWN&&tmptype!=NODETYPE_NOJUNCTION) {
                    type = tmptype;
                }
            }
        }
    }
    return type;
}


bool
NBNode::isSimpleContinuation() const
{
    // one in, one out->continuation
    if (_incomingEdges->size()==1&&_outgoingEdges->size()==1) {
        // both must have the same number of lanes
        return (*(_incomingEdges->begin()))->getNoLanes()==(*(_outgoingEdges->begin()))->getNoLanes();
    }
    // two in and two out and both in reverse direction
    if (_incomingEdges->size()==2&&_outgoingEdges->size()==2) {
        for (EdgeVector::const_iterator i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
            NBEdge *in = *i;
            EdgeVector::const_iterator opposite = find_if(_outgoingEdges->begin(), _outgoingEdges->end(), NBContHelper::opposite_finder(in, this));
            // must have an opposite edge
            if (opposite==_outgoingEdges->end()) {
                return false;
            }
            // both must have the same number of lanes
            NBContHelper::nextCW(_outgoingEdges, opposite);
            if (in->getNoLanes()!=(*opposite)->getNoLanes()) {
                return false;
            }
        }
        return true;
    }
    // nope
    return false;
}


bool
NBNode::isDistrictCenter() const
{
    return _id.substr(0, 14)=="DistrictCenter";
}


bool
samePriority(NBEdge *e1, NBEdge *e2)
{
    if(e1==e2) {
        return true;
    }
    if (e1->getPriority()!=e2->getPriority()) {
        return false;
    }
    if ((int) e1->getSpeed()!=(int) e2->getSpeed()) {
        return false;
    }
    return (int) e1->getNoLanes()==(int) e2->getNoLanes();
}


void
NBNode::setPriorityJunctionPriorities()
{
    if (_incomingEdges->size()==0||_outgoingEdges->size()==0) {
        return;
    }
    vector<NBEdge*> incoming(*_incomingEdges);
    vector<NBEdge*> outgoing(*_outgoingEdges);
    // what we do want to have is to extract the pair of roads that are
    //  the major roads for this junction
    // let's get the list of incoming edges with the highest priority
    sort(incoming.begin(), incoming.end(), NBContHelper::edge_by_priority_sorter());
    std::vector<NBEdge*> bestIncoming;
    NBEdge *best = incoming[0];
    while (incoming.size()>0&&samePriority(best, incoming[0])) {
        bestIncoming.push_back(*incoming.begin());
        incoming.erase(incoming.begin());
    }
    // now, let's get the list of best outgoing
    assert(outgoing.size()!=0);
    sort(outgoing.begin(), outgoing.end(), NBContHelper::edge_by_priority_sorter());
    std::vector<NBEdge*> bestOutgoing;
    best = outgoing[0];
    while (outgoing.size()>0&&samePriority(best, outgoing[0])) {//->getPriority()==best->getPriority()) {
        bestOutgoing.push_back(*outgoing.begin());
        outgoing.erase(outgoing.begin());
    }
    // now, let's compute for each of the best incoming edges
    //  the incoming which is most opposite
    //  the outgoing which is most opposite
    vector<NBEdge*>::iterator i;
    map<NBEdge*, NBEdge*> counterIncomingEdges;
    map<NBEdge*, NBEdge*> counterOutgoingEdges;
    incoming = *_incomingEdges;
    outgoing = *_outgoingEdges;
    for (i=bestIncoming.begin(); i!=bestIncoming.end(); ++i) {
        sort(incoming.begin(), incoming.end(), NBContHelper::edge_opposite_direction_sorter(*i));
        counterIncomingEdges[*i] = *incoming.begin();
        sort(outgoing.begin(), outgoing.end(), NBContHelper::edge_opposite_direction_sorter(*i));
        counterOutgoingEdges[*i] = *outgoing.begin();
    }
    // ok, let's try
    // 1) there is one best incoming road
    if (bestIncoming.size()==1) {
        // let's mark this road as the best
        NBEdge *best1 = extractAndMarkFirst(bestIncoming);
        if (bestOutgoing.size()!=0) {
            // mark the best outgoing as the continuation
            sort(bestOutgoing.begin(), bestOutgoing.end(), NBContHelper::edge_similar_direction_sorter(best1));
            NBEdge *bo = extractAndMarkFirst(bestOutgoing);
        }
        return;
    }
    // 2b) there are more than one best incoming roads
    //      and the same number of best outgoing roads

    // ok, what we want to do in this case is to determine which incoming
    //  has the best continuation...
    // This means, when several incoming roads have the same priority,
    //  we want a (any) straight connection to be more priorised than a turning
    SUMOReal bestAngle = 0;
    NBEdge *bestFirst = 0;
    NBEdge *bestSecond = 0;
    for (i=bestIncoming.begin(); i!=bestIncoming.end(); ++i) {
        vector<NBEdge*>::iterator j;
        NBEdge *t1 = *i;
        SUMOReal angle1 = 0;
        {
            angle1 = t1->getAngle()+180;
            if (angle1>=360) {
                angle1 -= 360;
            }
        }
        for (j=i+1; j!=bestIncoming.end(); ++j) {
            NBEdge *t2 = *j;
            SUMOReal angle2 = 0;
            {
                angle2 = t2->getAngle()+180;
                if (angle2>=360) {
                    angle2 -= 360;
                }
            }
            SUMOReal angle = MIN2(GeomHelper::getCCWAngleDiff(angle1, angle2), GeomHelper::getCWAngleDiff(angle1, angle2));
            if (angle>bestAngle) {
                bestAngle = angle;
                bestFirst = *i;
                bestSecond = *j;
            }
        }
    }
    bestFirst->setJunctionPriority(this, 1);
    sort(bestOutgoing.begin(), bestOutgoing.end(), NBContHelper::edge_similar_direction_sorter(bestFirst));
    if (bestOutgoing.size()!=0) {
        extractAndMarkFirst(bestOutgoing);
    }
    bestSecond->setJunctionPriority(this, 1);
    sort(bestOutgoing.begin(), bestOutgoing.end(), NBContHelper::edge_similar_direction_sorter(bestSecond));
    if (bestOutgoing.size()!=0) {
        extractAndMarkFirst(bestOutgoing);
    }
}


NBEdge*
NBNode::extractAndMarkFirst(vector<NBEdge*> &s)
{
    if (s.size()==0) {
        return 0;
    }
    NBEdge *ret = s.front();
    s.erase(s.begin());
    ret->setJunctionPriority(this, 1);
    return ret;
}


void
NBNode::rotateIncomingEdges(int norot)
{
    if (_incomingEdges->size()==0) {
        return;
    }
    while (norot>0) {
        NBEdge *e = (*_incomingEdges)[0];
        unsigned int i;
        for (i=0; i<_incomingEdges->size()-1; i++) {
            assert(_incomingEdges!=0&&_incomingEdges->size()>i+1);
            (*_incomingEdges)[i] = (*_incomingEdges)[i+1];
        }
        assert(_incomingEdges!=0&&i<_incomingEdges->size());
        (*_incomingEdges)[i] = e;
        norot--;
    }
}


std::vector<std::string>
NBNode::getInternalNamesList()
{
    std::vector<std::string> ret;
    size_t noInternalLanes = countInternalLanes(true);
    if (noInternalLanes!=0) {
        for (size_t i=0; i<noInternalLanes; i++) {
            ret.push_back(":" + _id + "_" + toString<size_t>(i));
        }
    }
    return ret;
}


size_t
NBNode::countInternalLanes(bool includeSplits)
{
    size_t lno = 0;
    EdgeVector::iterator i;
    for (i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
        size_t noLanesEdge = (*i)->getNoLanes();
        for (size_t j=0; j<noLanesEdge; j++) {
            const EdgeLaneVector &elv = (*i)->getEdgeLanesFromLane(j);
            for (EdgeLaneVector::const_iterator k=elv.begin(); k!=elv.end(); k++) {
                if ((*k).edge==0) {
                    continue;
                }
                lno++;
                // add internal splits if any
                if (includeSplits) {
                    std::pair<SUMOReal, std::vector<size_t> > cross = getCrossingPosition(*i, j, (*k).edge, (*k).lane);
                    if (cross.first>=0) {
                        lno++;
                    }
                }
            }
        }
    }
    return lno;
}


void
NBNode::writeXMLInternalLinks(ostream &into)
{
    size_t noInternalNoSplits = countInternalLanes(false);
    if (noInternalNoSplits==0) {
        return;
    }
    string id = ":" + _id;
    size_t lno = 0;
    size_t splitNo = 0;
    EdgeVector::iterator i;
    for (i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
        size_t noLanesEdge = (*i)->getNoLanes();
        for (size_t j=0; j<noLanesEdge; j++) {
            const EdgeLaneVector &elv = (*i)->getEdgeLanesFromLane(j);
            for (EdgeLaneVector::const_iterator k=elv.begin(); k!=elv.end(); k++) {
                if ((*k).edge==0) {
                    continue;
                }
                // compute the maximum speed allowed
                //  see !!! for an explanation (with a_lat_mean ~0.3)
                SUMOReal vmax = (SUMOReal) 0.3 * (SUMOReal) 9.80778 *
                                GeomHelper::distance(
                                    (*i)->getLaneShape(j).getEnd(),
                                    (*k).edge->getLaneShape((*k).lane).getBegin())
                                / (SUMOReal) 2.0 / (SUMOReal) PI;
                vmax = MIN2(vmax, (((*i)->getSpeed()+(*k).edge->getSpeed())/(SUMOReal) 2.0));
                vmax = ((*i)->getSpeed()+(*k).edge->getSpeed())/(SUMOReal) 2.0;
                //
                string id = ":" + _id + "_" + toString<size_t>(lno);
                Position2D end = (*k).edge->getLaneShape((*k).lane).getBegin();
                Position2D beg = (*i)->getLaneShape(j).getEnd();
                Position2DVector shape = computeInternalLaneShape(*i, j, (*k).edge, (*k).lane);
                SUMOReal length = MAX2(shape.length(), (SUMOReal) .1);

                // get internal splits if any
                std::pair<SUMOReal, std::vector<size_t> > cross = getCrossingPosition(*i, j, (*k).edge, (*k).lane);
                if (cross.first>=0) {
                    std::pair<Position2DVector, Position2DVector> split = shape.splitAt(cross.first);

                    into << "   <edge id=\"" << id
                    << "\" function=\"internal\">" << endl;
                    into << "      <lanes>" << endl;
                    into << "         <lane id=\"" << id << "_0\" depart=\"0\" "
                    << "maxspeed=\"" << vmax << "\" length=\""
                    << toString<SUMOReal>(cross.first) << "\" "
                    << ">"
                    << split.first
                    << "</lane>" << endl;
                    into << "      </lanes>" << endl;
                    into << "      <cedge id=\":" << _id << "_" << toString<size_t>(splitNo+noInternalNoSplits)
                    << "\">" << id << "_0" << "</cedge>" << endl;
                    into << "   </edge>" << endl << endl;
                    lno++;

                    string id = ":" + _id + "_" + toString<size_t>(splitNo+noInternalNoSplits);
                    into << "   <edge id=\"" << id
                    << "\" function=\"internal\">" << endl;
                    into << "      <lanes>" << endl;
                    into << "         <lane id=\"" << id << "_0\" depart=\"0\" "
                    << "maxspeed=\"" << vmax << "\" length=\""
                    << toString<SUMOReal>(length-cross.first) << "\" "
                    << ">"
                    << split.second
                    << "</lane>" << endl;
                    into << "      </lanes>" << endl;
                    into << "      <cedge id=\"" << (*k).edge->getID()
                    << "\">" << id << "_0" << "</cedge>" << endl;
                    into << "   </edge>" << endl << endl;
                    splitNo++;
                } else {
                    into << "   <edge id=\"" << id
                    << "\" function=\"internal\">" << endl;
                    into << "      <lanes>" << endl;
                    into << "         <lane id=\"" << id << "_0\" depart=\"0\" "
                    << "maxspeed=\"" << vmax << "\" length=\""
                    << toString<SUMOReal>(length) << "\" "
                    << ">"
                    << shape
                    << "</lane>" << endl;
                    into << "      </lanes>" << endl;
                    into << "      <cedge id=\"" << (*k).edge->getID()
                    << "\">" << id << "_0" << "</cedge>" << endl;
                    into << "   </edge>" << endl << endl;
                    lno++;
                }
            }
        }
    }
}


Position2DVector
NBNode::computeInternalLaneShape(NBEdge *fromE, size_t fromL,
                                 NBEdge *toE, size_t toL)
{
    if (fromL>=fromE->getNoLanes()) {
        MsgHandler::getErrorInstance()->inform("Connection '" + fromE->getID() + "_" + toString(fromL) + "->" + toE->getID() + "_" + toString(toL) + "' starts at a not existing lane.");
        throw ProcessError();
    }
    if (toL>=toE->getNoLanes()) {
        MsgHandler::getErrorInstance()->inform("Connection '" + fromE->getID() + "_" + toString(fromL) + "->" + toE->getID() + "_" + toString(toL) + "' yields in a not existing lane.");
        throw ProcessError();
    }
    bool noSpline = false;
    Position2DVector ret;
    Position2DVector init;
    Position2D beg = fromE->getLaneShape(fromL).getEnd();
    Position2D end = toE->getLaneShape(toL).getBegin();
    Position2D intersection;
    size_t noInitialPoints = 0;
    if (beg==end) {
        noSpline = true;
    } else {
        if (fromE->getTurnDestination()==toE) {
            // turnarounds:
            //  - end of incoming lane
            //  - position between incoming/outgoing end/begin shifted by the distace orthogonally
            //  - begin of outgoing lane
            noInitialPoints = 3;
            init.push_back(beg);
            Line2D straightConn(fromE->getLaneShape(fromL)[-1],toE->getLaneShape(toL)[0]);
            Position2D straightCenter = straightConn.getPositionAtDistance((SUMOReal) straightConn.length() / (SUMOReal) 2.);
            Position2D center = straightCenter;//.add(straightCenter);
            Line2D cross(straightConn);
            cross.sub(cross.p1().x(), cross.p1().y());
            cross.rotateDegAtP1(90);
            center.sub(cross.p2());
            init.push_back(center);
            init.push_back(end);
        } else {
            //
            SUMOReal angle1 = fromE->getLaneShape(fromL).getEndLine().atan2DegreeAngle();
            SUMOReal angle2 = toE->getLaneShape(toL).getBegLine().atan2DegreeAngle();
            SUMOReal angle = MIN2(GeomHelper::getCCWAngleDiff(angle1, angle2), GeomHelper::getCWAngleDiff(angle1, angle2));
            if (angle<45) {
                // very low angle: almost straight
                noInitialPoints = 4;
                init.push_back(beg);
                Line2D begL = fromE->getLaneShape(fromL).getEndLine();
                begL.extrapolateSecondBy(100);
                Line2D endL = toE->getLaneShape(toL).getBegLine();
                endL.extrapolateFirstBy(100);
                SUMOReal distance = GeomHelper::distance(beg, end);
                if (distance>10) {
                    {
                        SUMOReal off1 = fromE->getLaneShape(fromL).getEndLine().length() + (SUMOReal) 5. * (SUMOReal) fromE->getNoLanes();
                        off1 = MIN2(off1, (SUMOReal)(fromE->getLaneShape(fromL).getEndLine().length()+distance/2.));
                        Position2D tmp = begL.getPositionAtDistance(off1);
                        init.push_back(tmp);
                    }
                    {
                        SUMOReal off1 = (SUMOReal) 100. - (SUMOReal) 5. * (SUMOReal) toE->getNoLanes();
                        off1 = MAX2(off1, (SUMOReal)(100.-distance/2.));
                        Position2D tmp = endL.getPositionAtDistance(off1);
                        init.push_back(tmp);
                    }
                } else {
                    noSpline = true;
                }
                init.push_back(end);
            } else {
                // turning
                //  - end of incoming lane
                //  - intersection of the extrapolated lanes
                //  - begin of outgoing lane
                // attention: if there is no intersection, use a straight line
                noInitialPoints = 3;
                init.push_back(beg);
                Line2D begL = fromE->getLaneShape(fromL).getEndLine();
                begL.extrapolateSecondBy(100);
                Line2D endL = toE->getLaneShape(toL).getBegLine();
                endL.extrapolateFirstBy(100);
                if (!begL.intersects(endL)) {
                    noSpline = true;
                } else {
                    init.push_back(begL.intersectsAt(endL));
                }
                init.push_back(end);
            }
        }
    }
    //
    if (noSpline) {
        ret.push_back(fromE->getLaneShape(fromL).getEnd());
        ret.push_back(toE->getLaneShape(toL).getBegin());
        return ret;
    }

    size_t i;
    SUMOReal *def = new SUMOReal[1+noInitialPoints*3];
    for (i=0; i<init.size(); ++i) {
        // starts at index 1
        def[i*3+1] = init[i].x();
        def[i*3+2] = 0;
        def[i*3+3] = init[i].y();
    }
    SUMOReal ret_buf[NO_INTERNAL_POINTS*3+1];
    bezier(noInitialPoints, def, NO_INTERNAL_POINTS, ret_buf);
    delete[] def;
    Position2D prev;

    for (i=0; i<NO_INTERNAL_POINTS; i++) {
        Position2D current(ret_buf[i*3+1], ret_buf[i*3+3]);
        if (prev!=current) {
            ret.push_back(current);
        }
        prev = current;
    }
    return ret;
}


std::pair<SUMOReal, std::vector<size_t> >
NBNode::getCrossingPosition(NBEdge *fromE, size_t fromL, NBEdge *toE, size_t toL)
{
    std::pair<SUMOReal, std::vector<size_t> > ret(-1, std::vector<size_t>());
    NBMMLDirection dir = getMMLDirection(fromE, toE);
    switch (dir) {
    case MMLDIR_LEFT:
    case MMLDIR_PARTLEFT:
    case MMLDIR_TURN: {
        Position2DVector thisShape = computeInternalLaneShape(fromE, fromL, toE, toL);
        size_t index = 0;
        for (EdgeVector::iterator i2=_incomingEdges->begin(); i2!=_incomingEdges->end(); i2++) {
            size_t noLanesEdge = (*i2)->getNoLanes();
            for (size_t j2=0; j2<noLanesEdge; j2++) {
                const EdgeLaneVector &elv = (*i2)->getEdgeLanesFromLane(j2);
                for (EdgeLaneVector::const_iterator k2=elv.begin(); k2!=elv.end(); k2++) {
                    if ((*k2).edge==0) {
                        continue;
                    }
                    if (fromE!=(*i2)&&_request->forbids(*i2, (*k2).edge, fromE, toE, true)) {
                        // compute the crossing point
                        ret.second.push_back(index);
                        Position2DVector otherShape = computeInternalLaneShape(*i2, j2, (*k2).edge, (*k2).lane);
                        if (thisShape.intersects(otherShape)) {
                            DoubleVector dv = thisShape.intersectsAtLengths(otherShape);
                            SUMOReal minDV = dv[0];
                            if (minDV<thisShape.length()-.1&&minDV>.1) { // !!!?
                                assert(minDV>=0);
                                if (ret.first<0||ret.first>minDV) {
                                    ret.first = minDV;
                                }
                            }
                        }
                    }
                    index++;
                }
            }
        }
    }
    break;
    default:
        break;
    }
    return ret;
}


std::string
NBNode::getCrossingNames_dividedBySpace(NBEdge *fromE, size_t fromL,
                                        NBEdge *toE, size_t toL)
{
    std::string ret;
    NBMMLDirection dir = getMMLDirection(fromE, toE);
    switch (dir) {
    case MMLDIR_LEFT:
    case MMLDIR_PARTLEFT:
    case MMLDIR_TURN: {
        Position2DVector thisShape = computeInternalLaneShape(fromE, fromL, toE, toL);
        size_t index = 0;
        for (EdgeVector::iterator i2=_incomingEdges->begin(); i2!=_incomingEdges->end(); i2++) {
            size_t noLanesEdge = (*i2)->getNoLanes();
            for (size_t j2=0; j2<noLanesEdge; j2++) {
                const EdgeLaneVector &elv = (*i2)->getEdgeLanesFromLane(j2);
                for (EdgeLaneVector::const_iterator k2=elv.begin(); k2!=elv.end(); k2++) {
                    if ((*k2).edge==0) {
                        continue;
                    }
                    NBEdge *e = fromE->getToNode()->getOppositeIncoming(fromE);
                    if (e!=*i2) {
                        index++;
                        continue;
                    }
                    NBMMLDirection dir2 = getMMLDirection(*i2, (*k2).edge);
                    bool left = dir2==MMLDIR_LEFT || dir2==MMLDIR_PARTLEFT || dir2==MMLDIR_TURN;
                    left = false;
                    if (!left&&fromE!=(*i2)&&_request->forbids(*i2, (*k2).edge, fromE, toE, true)) {
                        if (ret.length()!=0) {
                            ret += " ";
                        }
                        ret += (":" + _id + "_" + toString(index) + "_0");
                    }
                    index++;
                }
            }
        }
    }
    break;
    default:
        break;
    }
    return ret;
}


std::string
NBNode::getCrossingSourcesNames_dividedBySpace(NBEdge *fromE, size_t fromL,
        NBEdge *toE, size_t toL)
{
    std::string ret;
    std::vector<std::string> tmp;
    NBMMLDirection dir = getMMLDirection(fromE, toE);
    switch (dir) {
    case MMLDIR_LEFT:
    case MMLDIR_PARTLEFT:
    case MMLDIR_TURN: {
        Position2DVector thisShape = computeInternalLaneShape(fromE, fromL, toE, toL);
        size_t index = 0;
        for (EdgeVector::iterator i2=_incomingEdges->begin(); i2!=_incomingEdges->end(); i2++) {
            size_t noLanesEdge = (*i2)->getNoLanes();
            for (size_t j2=0; j2<noLanesEdge; j2++) {
                const EdgeLaneVector &elv = (*i2)->getEdgeLanesFromLane(j2);
                for (EdgeLaneVector::const_iterator k2=elv.begin(); k2!=elv.end(); k2++) {
                    if ((*k2).edge==0) {
                        continue;
                    }
                    NBEdge *e = fromE->getToNode()->getOppositeIncoming(fromE);
                    if (e!=*i2) {
                        index++;
                        continue;
                    }
                    NBMMLDirection dir2 = getMMLDirection(*i2, (*k2).edge);
                    bool left = dir2==MMLDIR_LEFT || dir2==MMLDIR_PARTLEFT || dir2==MMLDIR_TURN;
                    left = false;
                    if (!left&&fromE!=(*i2)&&_request->forbids(*i2, (*k2).edge, fromE, toE, true)) {
                        string nid = (*i2)->getID() + "_" + toString(j2);
                        if (find(tmp.begin(), tmp.end(), nid)==tmp.end()) {
                            tmp.push_back(nid);
                        }
                    }
                    index++;
                }
            }
        }
    }
    break;
    default:
        break;
    }
    for (vector<string>::iterator i=tmp.begin(); i!=tmp.end(); ++i) {
        if (ret.length()>0) {
            ret = ret + " ";
        }
        ret = ret + *i;
    }
    return ret;
}


void
NBNode::writeXMLInternalSuccInfos(ostream &into)
{
    size_t noInternalNoSplits = countInternalLanes(false);
    if (noInternalNoSplits==0) {
        return;
    }
    size_t lno = 0;
    size_t splitNo = 0;
    for (EdgeVector::iterator i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
        size_t noLanesEdge = (*i)->getNoLanes();
        for (size_t j=0; j<noLanesEdge; j++) {
            const EdgeLaneVector &elv = (*i)->getEdgeLanesFromLane(j);
            for (EdgeLaneVector::const_iterator k=elv.begin(); k!=elv.end(); k++) {
                if ((*k).edge==0) {
                    continue;
                }
                string id = ":" + _id + "_" + toString<size_t>(lno);
                string sid = ":" + _id + "_" + toString<size_t>(splitNo+noInternalNoSplits);
                std::pair<SUMOReal, std::vector<size_t> > cross = getCrossingPosition(*i, j, (*k).edge, (*k).lane);

                // get internal splits if any
                into << "   <succ edge=\"" << id << "\" "
                << "lane=\"" << id << "_"
                << 0 << "\" junction=\"" << _id << "\">"
                << endl;
                if (cross.first>=0) {
                    into << "      <succlane lane=\""
                    //<< sid << "_" << 0 ()
                    << (*k).edge->getID() << "_" << (*k).lane
                    << "\" tl=\"" << "" << "\" linkno=\""
                    << "" << "\" yield=\"1\" dir=\"s\" state=\"M\"" // !!! yield or not depends on whether it is tls controlled or not
                    << " via=\"" << sid << "_" << 0 << "\"";
                } else {
                    into << "      <succlane lane=\""
                    << (*k).edge->getID() << "_" << (*k).lane
                    << "\" tl=\"" << "" << "\" linkno=\""
                    << "" << "\" yield=\"0\" dir=\"s\" state=\"M\"";
                }
                into << " int_end=\"x\"/>" << endl;
                into << "   </succ>" << endl;

                if (cross.first>=0) {
                    into << "   <succ edge=\"" << sid << "\" "
                    << "lane=\"" << sid << "_" << 0
                    << "\" junction=\"" << sid << "\">"
                    << endl;
                    into << "      <succlane lane=\""
                    << (*k).edge->getID() << "_" << (*k).lane
                    << "\" tl=\"" << "" << "\" linkno=\""
                    << "0" << "\" yield=\"0\" dir=\"s\" state=\"M\"";
                    into << " int_end=\"x\"/>" << endl;
                    into << "   </succ>" << endl;
                    splitNo++;
                }
                lno++;
            }
        }
    }
}


void
NBNode::writeXMLInternalNodes(ostream &into)
{
    size_t noInternalNoSplits = countInternalLanes(false);
    if (noInternalNoSplits==0) {
        return;
    }
    size_t lno = 0;
    size_t splitNo = 0;
    for (EdgeVector::iterator i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
        size_t noLanesEdge = (*i)->getNoLanes();
        for (size_t j=0; j<noLanesEdge; j++) {
            const EdgeLaneVector &elv = (*i)->getEdgeLanesFromLane(j);
            for (EdgeLaneVector::const_iterator k=elv.begin(); k!=elv.end(); k++) {
                if ((*k).edge==0) {
                    continue;
                }


                std::pair<SUMOReal, std::vector<size_t> > cross = getCrossingPosition(*i, j, (*k).edge, (*k).lane);
                if (cross.first<=0) {
                    lno++;
                    continue;
                }

                // write the attributes
                string sid = ":" + _id + "_" + toString<size_t>(splitNo+noInternalNoSplits) + "_0";
                string iid = ":" + _id + "_" + toString<size_t>(lno) + "_0";
                Position2DVector shape = computeInternalLaneShape(*i, j, (*k).edge, (*k).lane);
                Position2D pos = shape.positionAtLengthPosition(cross.first);
                into << "   <junction id=\"" << sid << '\"';
                into << " type=\"" << "internal\"";
                into << " x=\"" << setprecision(2) << pos.x()
                << "\" y=\"" << setprecision(2) << pos.y() << "\"";
                into <<  ">" << endl;
                // write the incoming and the internal lanes
                string furtherIncoming = getCrossingSourcesNames_dividedBySpace(*i, j, (*k).edge, (*k).lane);
                if (furtherIncoming.length()!=0) {
                    into << "      <inclanes>" << iid << " " << furtherIncoming << "</inclanes>" << endl;
                } else {
                    into << "      <inclanes>" << iid << "</inclanes>" << endl;
                }
                into << "      <intlanes>"
                << getCrossingNames_dividedBySpace(*i, j, (*k).edge, (*k).lane)
                << "</intlanes>" << endl;
                into << "      <shape></shape>" << endl;
                // close writing
                into << "   </junction>" << endl << endl;
                splitNo++;
                lno++;
            }
        }
    }
}


void
writeinternal(EdgeVector *_incomingEdges, ostream &into, const std::string &id)
{
    size_t l = 0;
    for (EdgeVector::iterator i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
        size_t noLanesEdge = (*i)->getNoLanes();
        for (size_t j=0; j<noLanesEdge; j++) {
            const EdgeLaneVector &elv = (*i)->getEdgeLanesFromLane(j);
            for (EdgeLaneVector::const_iterator k=elv.begin(); k!=elv.end(); k++) {
                if ((*k).edge==0) {
                    continue;
                }
                if (l!=0) {
                    into << ' ';
                }
                into << ':' << id << '_' << l << "_0";
                l++;
            }
        }
    }
}


void
NBNode::writeXML(ostream &into)
{
    // write the attributes
    into << "   <junction id=\"" << _id << '\"';
    if (_incomingEdges->size()!=0&&_outgoingEdges->size()!=0) {
        //into << " key=\"" << _key << '\"';
        switch (_type) {
        case NODETYPE_NOJUNCTION:
            into << " type=\"" << "none\"";
            break;
        case NODETYPE_PRIORITY_JUNCTION:
        case NODETYPE_TRAFFIC_LIGHT:
            into << " type=\"" << "priority\"";
            break;
        case NODETYPE_RIGHT_BEFORE_LEFT:
            into << " type=\"" << "right_before_left\"";
            break;
        case NODETYPE_DISTRICT:
            into << " type=\"" << "district\"";
            break;
        default:
            throw exception();
            break;
        }
    } else {
        into << " type=\"DEAD_END\"";
    }
    into << " x=\"" << setprecision(2) << myPosition.x()
    << "\" y=\"" << setprecision(2) << myPosition.y() << "\"";
    into <<  ">" << endl;
    // write the incoming lanes
    EdgeVector::iterator i;
    into << "      <inclanes>";
    for (i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
        size_t noLanes = (*i)->getNoLanes();
        string id = (*i)->getID();
        for (size_t j=0; j<noLanes; j++) {
            into << id << '_' << j;
            if (i!=_incomingEdges->end()-1 || j<noLanes-1) {
                into << ' ';
            }
        }
    }
    into << "</inclanes>" << endl;
    // write the internal lanes
    if (OptionsSubSys::getOptions().getBool("add-internal-links")) {
        into << "      <intlanes>";
        writeinternal(_incomingEdges, into, _id);
        into << "</intlanes>" << endl;
    }

    // write the shape
    into << "      <shape>" << myPoly << "</shape>" << endl;
    // close writing
    into << "   </junction>" << endl << endl;
}


void
NBNode::computeLogic(const NBEdgeCont &ec, NBJunctionLogicCont &jc,
                     OptionsCont &)
{
    if (_incomingEdges->size()==0||_outgoingEdges->size()==0) {
        return;
    }
    // build the request
    _request = new NBRequest(ec, this,
                             static_cast<const EdgeVector * const>(&_allEdges),
                             static_cast<const EdgeVector * const>(_incomingEdges),
                             static_cast<const EdgeVector * const>(_outgoingEdges),
                             _blockedConnections);

    // compute the logic if necessary or split the junction
    if (_type!=NODETYPE_NOJUNCTION&&_type!=NODETYPE_DISTRICT) {
        _request->buildBitfieldLogic(jc, _id);
    }
}


void
NBNode::setType(BasicNodeType type)
{
    switch (type) {
    case NODETYPE_NOJUNCTION:
        _noUnregulatedJunctions++;
        break;
    case NODETYPE_PRIORITY_JUNCTION:
    case NODETYPE_TRAFFIC_LIGHT:
        _noPriorityJunctions++;
        break;
    case NODETYPE_RIGHT_BEFORE_LEFT:
        _noRightBeforeLeftJunctions++;
        break;
    case NODETYPE_DISTRICT:
        _noRightBeforeLeftJunctions++;
        break;
    default:
        throw exception();
    }
    _type = type;
}


NBNode::BasicNodeType
NBNode::getType() const
{
    return _type;
}


void
NBNode::reportBuild()
{
    WRITE_MESSAGE("Unregulated Junctions       : " + toString<int>(_noUnregulatedJunctions));
    WRITE_MESSAGE("Priority Junctions          : " + toString<int>(_noPriorityJunctions));
    WRITE_MESSAGE("Right Before Left Junctions : " + toString<int>(_noRightBeforeLeftJunctions));
}


void
NBNode::sortNodesEdges(const NBTypeCont &tc, std::ofstream *strm)
{
    // sort the edges
    buildList();
    sort(_allEdges.begin(), _allEdges.end(), NBContHelper::edge_by_junction_angle_sorter(this));
    sort(_incomingEdges->begin(), _incomingEdges->end(), NBContHelper::edge_by_junction_angle_sorter(this));
    sort(_outgoingEdges->begin(), _outgoingEdges->end(), NBContHelper::edge_by_junction_angle_sorter(this));
    if (_allEdges.size()==0) {
        return;
    }
    vector<NBEdge*>::iterator i;
    for (i=_allEdges.begin();
            i!=_allEdges.end()-1&&i!=_allEdges.end(); i++) {
        swapWhenReversed(i, i+1);
    }
    if (_allEdges.size()>1 && i!=_allEdges.end()) {
        swapWhenReversed(_allEdges.end()-1, _allEdges.begin());
    }
#ifdef _DEBUG
    if (OptionsSubSys::getOptions().getInt("netbuild.debug")>0) {
        DEBUG_OUT << "Node '" << _id << "': ";
        const EdgeVector &ev = getEdges();
        for (EdgeVector::const_iterator i=ev.begin(); i!=ev.end(); ++i) {
            DEBUG_OUT << (*i)->getID() << ", ";
        }
        DEBUG_OUT << endl;
    }
#endif
    NBNode::BasicNodeType type = computeType(tc);
    // write if wished
    if (strm!=0) {
        string col;
        switch (type) {
        case NODETYPE_NOJUNCTION:
            col = ".5,.5,.5";
            break;
        case NODETYPE_PRIORITY_JUNCTION:
            col = "0,1,0";
            break;
        case NODETYPE_RIGHT_BEFORE_LEFT:
            col = "0,0,1";
            break;
        case NODETYPE_DISTRICT:
            col = "1,0,0";
            break;
        case NODETYPE_TRAFFIC_LIGHT:
            col = "1,1,0";
            break;
        }
        (*strm) << "   <poi id=\"type_" << _id
        << "\" type=\"node_type\" color=\"" << col << "\""
        << " x=\"" << getPosition().x() << "\" y=\"" << getPosition().y() << "\"/>"
        << endl;
    }
    setType(type);
    setPriorities();
}


void
NBNode::computeNodeShape(ofstream *out)
{
    if (_incomingEdges->size()==0&&_outgoingEdges->size()==0) {
        return;
    }
    NBNodeShapeComputer computer(*this, out);
    myPoly = computer.compute();
}


SUMOReal
NBNode::chooseLaneOffset(DoubleVector &chk)
{
    return DoubleVectorHelper::minValue(chk);
}


SUMOReal
NBNode::chooseLaneOffset2(DoubleVector &chk)
{
    DoubleVectorHelper::remove_larger_than(chk, 100);
    SUMOReal max = DoubleVectorHelper::maxValue(chk);
    return 100-max+100;
}

SUMOReal
NBNode::getOffset(Position2DVector on, Position2DVector cross) const
{
    if (on.intersects(cross)) {
        DoubleVector posses = on.intersectsAtLengths(cross);
        assert(posses.size()>0);
        // heuristic
        SUMOReal val = DoubleVectorHelper::maxValue(posses);
        if (val<50) {
            return val;
        }
        on.extrapolate(10);
        cross.extrapolate(10);
        if (on.intersects(cross)) {
            DoubleVector posses = on.intersectsAtLengths(cross);
            assert(posses.size()>0);
            return DoubleVectorHelper::minValue(posses) - 10;
        }
    }
    on.extrapolate(10);
    cross.extrapolate(10);
    if (on.intersects(cross)) {
        DoubleVector posses = on.intersectsAtLengths(cross);
        assert(posses.size()>0);
        return DoubleVectorHelper::maxValue(posses) - 10;
    }
    return -1;
}


void
NBNode::computeLanes2Lanes()
{
    // special case:
    //  one in, one out, the outgoing has one lane more
    if (_incomingEdges->size()==1&&_outgoingEdges->size()==1&&(*_incomingEdges)[0]->getNoLanes()==(*_outgoingEdges)[0]->getNoLanes()-1&&(*_incomingEdges)[0]!=(*_outgoingEdges)[0]) {
        NBEdge *incoming = (*_incomingEdges)[0];
        NBEdge *outgoing = (*_outgoingEdges)[0];
        for (int i=0; i<(int) incoming->getNoLanes(); ++i) {
            incoming->setConnection(i, outgoing, i+1, false);
        }
        incoming->setConnection(0, outgoing, 0, false);
        return;
    }

    // go through this node's outgoing edges
    //  for every outgoing edge, compute the distribution of the node's
    //  incoming edges on this edge when approaching this edge
    // the incoming edges' steps will then also be marked as LANE2LANE_RECHECK...
    vector<NBEdge*>::reverse_iterator i;
    for (i=_outgoingEdges->rbegin(); i!=_outgoingEdges->rend(); i++) {
        NBEdge *currentOutgoing = *i;
        // get the information about edges that do approach this edge
        vector<NBEdge*> *approaching = getApproaching(currentOutgoing);
        if (approaching->size()!=0) {
            ApproachingDivider divider(approaching, currentOutgoing);
            Bresenham::compute(&divider, (SUMOReal) approaching->size(),
                               (SUMOReal) currentOutgoing->getNoLanes());
        }
        delete approaching;
    }
    // ... but we may have the case that there are no outgoing edges
    //  In this case, we have to mark the incoming edges as being in state
    //   LANE2LANE( not RECHECK) by hand
    if (_outgoingEdges->size()==0) {
        for (i=_incomingEdges->rbegin(); i!=_incomingEdges->rend(); i++) {
            (*i)->markAsInLane2LaneState();
        }
    }
    // ... the next extension is to allow vehicles move from a lane to different
    //  next lanes. We take a look at each combination of from-lane and to-lane
    //  and if a lane beside the to-lane is free we will also add a connection
    //  to it as long as it is not approached by any other lane from the same edge
    if (_outgoingEdges->size()==1&&_incomingEdges->size()==1) {
        vector<NBEdge*>::iterator i2;
        for (i2=_incomingEdges->begin(); i2!=_incomingEdges->end(); i2++) {
            NBEdge *currentIncoming = *i2;
            currentIncoming->addAdditionalConnections();
        }
    }
}


vector<NBEdge*> *
NBNode::getApproaching(NBEdge *currentOutgoing)
{
    // get the position of the node to get the approaching nodes of
    vector<NBEdge*>::const_iterator i = find(_allEdges.begin(),
                                        _allEdges.end(), currentOutgoing);
    // get the first possible approaching edge
    NBContHelper::nextCW(&_allEdges, i);
    // go through the list of edges clockwise and add the edges
    vector<NBEdge*> *approaching = new vector<NBEdge*>();
    for (; *i!=currentOutgoing;) {
        // check only incoming edges
        if ((*i)->getToNode()==this) {
            vector<size_t> connLanes =
                (*i)->getConnectionLanes(currentOutgoing);
            if (connLanes.size()!=0) {
                approaching->push_back(*i);
            }
        }
        NBContHelper::nextCW(&_allEdges, i);
    }
    return approaching;
}


void
NBNode::setTurningDefinition(NBNode *from, NBNode *to)
{
    EdgeVector::iterator i;
    // get the source edge
    NBEdge *src = 0;
    for (i=_incomingEdges->begin(); src==0 && i!=_incomingEdges->end(); i++) {
        if ((*i)->getFromNode()==from) {
            src = (*i);
        }
    }
    // get the destination edge
    NBEdge *dest = 0;
    for (i=_outgoingEdges->begin(); dest==0 && i!=_outgoingEdges->end(); i++) {
        if ((*i)->getToNode()==to) {
            dest = (*i);
        }
    }
    // check both
    if (src==0) {
        // maybe it was removed due to something
        if (OptionsSubSys::getOptions().isSet("edges-min-speed")
                ||
                OptionsSubSys::getOptions().isSet("keep-edges")) {

            WRITE_WARNING("Could not set connection from node '" + from->getID() + "' to node '" + getID() + "'.");
        } else {
            MsgHandler::getErrorInstance()->inform("There is no edge from node '" + from->getID() + "' to node '" + getID() + "'.");
        }
        return;
    }
    if (dest==0) {
        if (OptionsSubSys::getOptions().isSet("edges-min-speed")
                ||
                OptionsSubSys::getOptions().isSet("keep-edges")) {

            WRITE_WARNING("Could not set connection from node '" + getID() + "' to node '" + to->getID() + "'.");
        } else {
            MsgHandler::getErrorInstance()->inform("There is no edge from node '" + getID() + "' to node '" + to->getID() + "'.");
        }
        return;
    }
    // both edges found
    //  set them into the edge
    src->addEdge2EdgeConnection(dest);
}


void
NBNode::resetby(SUMOReal xoffset, SUMOReal yoffset)
{
    myPosition.add(xoffset, yoffset);
    myPoly.resetBy(xoffset, yoffset);
}


void
NBNode::reshiftPosition(SUMOReal xoff, SUMOReal yoff, SUMOReal rot)
{
    myPosition.reshiftRotate(xoff, yoff, rot);
    myPoly.reshiftRotate(xoff, yoff, rot);
}


void
NBNode::replaceOutgoing(NBEdge *which, NBEdge *by, size_t laneOff)
{
    size_t i;
    // replace the edge in the list of outgoing nodes
    for (i=0; i<_outgoingEdges->size(); i++) {
        if ((*_outgoingEdges)[i]==which) {
            (*_outgoingEdges)[i] = by;
        }
    }
    // replace the edge in connections of incoming edges
    for (i=0; i<_incomingEdges->size(); i++) {
        (*_incomingEdges)[i]->replaceInConnections(which, by, laneOff);
    }
    // replace within the connetion prohibition dependencies
    replaceInConnectionProhibitions(which, by, 0, laneOff);
}


void
NBNode::replaceOutgoing(const EdgeVector &which, NBEdge *by)
{
    // replace edges
    size_t laneOff = 0;
    for (EdgeVector::const_iterator i=which.begin(); i!=which.end(); i++) {
        replaceOutgoing(*i, by, laneOff);
        laneOff += (*i)->getNoLanes();
    }
    // removed SUMOReal occurences
    removeDoubleEdges();
    // check whether this node belongs to a district and the edges
    //  must here be also remapped
    if (myDistrict!=0) {
        myDistrict->replaceOutgoing(which, by);
    }
}


void
NBNode::replaceIncoming(NBEdge *which, NBEdge *by, size_t laneOff)
{
    // replace the edge in the list of incoming nodes
    for (size_t i=0; i<_incomingEdges->size(); i++) {
        if ((*_incomingEdges)[i]==which) {
            (*_incomingEdges)[i] = by;
        }
    }
    // replace within the connetion prohibition dependencies
    replaceInConnectionProhibitions(which, by, laneOff, 0);
}


void
NBNode::replaceIncoming(const EdgeVector &which, NBEdge *by)
{
    // replace edges
    size_t laneOff = 0;
    for (EdgeVector::const_iterator i=which.begin(); i!=which.end(); i++) {
        replaceIncoming(*i, by, laneOff);
        laneOff += (*i)->getNoLanes();
    }
    // removed SUMOReal occurences
    removeDoubleEdges();
    // check whether this node belongs to a district and the edges
    //  must here be also remapped
    if (myDistrict!=0) {
        myDistrict->replaceIncoming(which, by);
    }
}



void
NBNode::replaceInConnectionProhibitions(NBEdge *which, NBEdge *by,
                                        size_t whichLaneOff, size_t byLaneOff)
{
    // replace in keys
    NBConnectionProhibits::iterator j = _blockedConnections.begin();
    while (j!=_blockedConnections.end()) {
        bool changed = false;
        NBConnection c = (*j).first;
        if (c.replaceFrom(which, whichLaneOff, by, byLaneOff)) {
            changed = true;
        }
        if (c.replaceTo(which, whichLaneOff, by, byLaneOff)) {
            changed = true;
        }
        if (changed) {
            _blockedConnections[c] = (*j).second;
            _blockedConnections.erase(j);
            j = _blockedConnections.begin();
        } else {
            j++;
        }
    }
    // replace in values
    for (j=_blockedConnections.begin(); j!=_blockedConnections.end(); j++) {
        NBConnectionVector &prohibiting = (*j).second;
        for (NBConnectionVector::iterator k=prohibiting.begin(); k!=prohibiting.end(); k++) {
            NBConnection &sprohibiting = *k;
            sprohibiting.replaceFrom(which, whichLaneOff, by, byLaneOff);
            sprohibiting.replaceTo(which, whichLaneOff, by, byLaneOff);
        }
    }
}



void
NBNode::removeDoubleEdges()
{
    size_t i, j;
    // check incoming
    for (i=0; _incomingEdges->size()>0&&i<_incomingEdges->size()-1; i++) {
        j = i + 1;
        while (j<_incomingEdges->size()) {
            if ((*_incomingEdges)[i]==(*_incomingEdges)[j]) {
                _incomingEdges->erase(_incomingEdges->begin()+j);
            } else {
                j++;
            }
        }
    }
    // check outgoing
    for (i=0; _outgoingEdges->size()>0&&i<_outgoingEdges->size()-1; i++) {
        j = i + 1;
        while (j<_outgoingEdges->size()) {
            if ((*_outgoingEdges)[i]==(*_outgoingEdges)[j]) {
                _outgoingEdges->erase(_outgoingEdges->begin()+j);
            } else {
                j++;
            }
        }
    }
}


bool
NBNode::hasOutgoing(NBEdge *e) const
{
    return find(_outgoingEdges->begin(), _outgoingEdges->end(), e)
           !=
           _outgoingEdges->end();
}


bool
NBNode::hasIncoming(NBEdge *e) const
{
    return find(_incomingEdges->begin(), _incomingEdges->end(), e)
           !=
           _incomingEdges->end();
}


NBEdge *
NBNode::getOppositeIncoming(NBEdge *e) const
{
    EdgeVector edges(*_incomingEdges);
    if (find(edges.begin(), edges.end(), e)!=edges.end()) {
        edges.erase(find(edges.begin(), edges.end(), e));
    }
    if (e->getToNode()==this) {
        sort(edges.begin(), edges.end(), NBContHelper::edge_opposite_direction_sorter(e));
    } else {
        sort(edges.begin(), edges.end(), NBContHelper::edge_similar_direction_sorter(e));
    }
    return edges[0];
}


void
NBNode::addSortedLinkFoes(const NBConnection &mayDrive,
                          const NBConnection &mustStop)
{
    if (mayDrive.getFrom()==0 ||
            mayDrive.getTo()==0 ||
            mustStop.getFrom()==0 ||
            mustStop.getTo()==0) {

        WRITE_WARNING("Something went wrong during the building of a connection...");
        return; // !!! mark to recompute connections
    }
    NBConnectionVector conn = _blockedConnections[mustStop];
    conn.push_back(mayDrive);
    _blockedConnections[mustStop] = conn;
}


NBEdge *
NBNode::getPossiblySplittedIncoming(const std::string &edgeid)
{
    size_t size = edgeid.length();
    for (EdgeVector::iterator i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
        string id = (*i)->getID();
        if (id.substr(0, size)==edgeid) {
            return *i;
        }
    }
    return 0;
}


NBEdge *
NBNode::getPossiblySplittedOutgoing(const std::string &edgeid)
{
    size_t size = edgeid.length();
    for (EdgeVector::iterator i=_outgoingEdges->begin(); i!=_outgoingEdges->end(); i++) {
        string id = (*i)->getID();
        if (id.substr(0, size)==edgeid) {
            return *i;
        }
    }
    return 0;
}


size_t
NBNode::eraseDummies(NBDistrictCont &dc, NBEdgeCont &ec, NBTrafficLightLogicCont &tc)
{
    size_t ret = 0;
    if (_outgoingEdges==0||_incomingEdges==0) {
        return ret;
    }
    size_t pos = 0;
    EdgeVector::const_iterator j=_incomingEdges->begin();
    while (j!=_incomingEdges->end()) {
        // skip edges which are only incoming and not outgoing
        if (find(_outgoingEdges->begin(), _outgoingEdges->end(), *j)==_outgoingEdges->end()) {
            j++;
            pos++;
            continue;
        }
        // an edge with both its origin and destination being the current
        //  node should be removed
        NBEdge *dummy = *j;
        WRITE_WARNING(" Removing dummy edge '" + dummy->getID() + "'");
        // get the list of incoming edges connected to the dummy
        EdgeVector incomingConnected;
        EdgeVector::const_iterator i;
        for (i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
            if ((*i)->isConnectedTo(dummy)&&*i!=dummy) {
                incomingConnected.push_back(*i);
            }
        }
        // get the list of outgoing edges connected to the dummy
        EdgeVector outgoingConnected;
        for (i=_outgoingEdges->begin(); i!=_outgoingEdges->end(); i++) {
            if (dummy->isConnectedTo(*i)&&*i!=dummy) {
                outgoingConnected.push_back(*i);
            }
        }
        // let the dummy remap its connections
        dummy->remapConnections(incomingConnected);
        remapRemoved(tc, dummy, incomingConnected, outgoingConnected);
        // delete the dummy
        ec.erase(dc, dummy);
        j = _incomingEdges->begin() + pos;
        ret++;
    }
    return ret;
}


void
NBNode::removeOutgoing(NBEdge *edge)
{
    EdgeVector::iterator i =
        find(_outgoingEdges->begin(), _outgoingEdges->end(), edge);
    if (i!=_outgoingEdges->end()) {
        _outgoingEdges->erase(i);
    }
}


void
NBNode::removeIncoming(NBEdge *edge)
{
    EdgeVector::iterator i =
        find(_incomingEdges->begin(), _incomingEdges->end(), edge);
    if (i!=_incomingEdges->end()) {
        _incomingEdges->erase(i);
    }
}




Position2D
NBNode::getEmptyDir() const
{
    Position2D pos(0, 0);
    EdgeVector::const_iterator i;
    for (i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
        NBNode *conn = (*i)->getFromNode();
        Position2D toAdd = conn->getPosition();
        toAdd.sub(myPosition);
        toAdd.mul((SUMOReal) 1.0/sqrt(toAdd.x()*toAdd.x()+toAdd.y()*toAdd.y()));
        pos.add(toAdd);
    }
    for (i=_outgoingEdges->begin(); i!=_outgoingEdges->end(); i++) {
        NBNode *conn = (*i)->getToNode();
        Position2D toAdd = conn->getPosition();
        toAdd.sub(myPosition);
        toAdd.mul((SUMOReal) 1.0/sqrt(toAdd.x()*toAdd.x()+toAdd.y()*toAdd.y()));
        pos.add(toAdd);
    }
    pos.mul((SUMOReal) -1.0/(_incomingEdges->size()+_outgoingEdges->size()));
    if (pos.x()==0&&pos.y()==0) {
        pos = Position2D(1, 0);
    }
    pos.norm();
    return pos;
}



void
NBNode::invalidateIncomingConnections()
{
    for (EdgeVector::const_iterator i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
        (*i)->invalidateConnections();
    }
}


void
NBNode::invalidateOutgoingConnections()
{
    for (EdgeVector::const_iterator i=_outgoingEdges->begin(); i!=_outgoingEdges->end(); i++) {
        (*i)->invalidateConnections();
    }
}


bool
NBNode::mustBrake(NBEdge *from, NBEdge *to, int toLane) const
{
    assert(_request!=0);
    // check whether it is participant to a traffic light
    //  - controlled links are set by the traffic lights, not the normal
    //    right-of-way rules
    //  - uncontrolled participants (spip lanes etc.) should always break
    if (myTrafficLights.size()!=0) {
        // ok, we have a traffic light, return true by now, it will be later
        //  controlled by the tls
        return true;
    }
    // vehicles which do not have a following lane must always decelerate to the end
    if (to==0) {
        return true;
    }
    // check whether any other connection on this node prohibits this connection
    bool try1 = _request->mustBrake(from, to);
    if (!try1||toLane==-1) {
        return try1;
    }
    if (from->getSpeed()<70./3.6) {
        return try1;
    }
    // on highways (on-ramps, in fact):
    // check whether any other connection uses the same destination edge
    for (EdgeVector::const_iterator i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
        if ((*i)==from) {
            continue;
        }
        size_t noLanesEdge1 = (*i)->getNoLanes();
        for (size_t j1=0; j1<noLanesEdge1; j1++) {
            const EdgeLaneVector &el1 = (*i)->getEdgeLanesFromLane(j1);
            for (EdgeLaneVector::const_iterator i2=el1.begin(); i2!=el1.end(); i2++) {
                if ((*i2).edge==to&&((*i2).lane==-1||(*i2).lane==toLane)) {
                    return true;
                }
            }
        }
    }
    return false;
}



bool
NBNode::isLeftMover(NBEdge *from, NBEdge *to) const
{
    // when the junction has only one incoming edge, there are no
    //  problems caused by left blockings
    if (_incomingEdges->size()==1) {
        return false;
    }
    // ok, per definition, a left-moving stream is a stream
    //  that crosses the stream which moves to the other direction.
    //  So, if that direction does not exist, there is not left-moving stream
    //  !!!??
    if (from->getTurnDestination()==0) {
        return false;
    }

    // now again some heuristics...
    //  how to compute whether an edge is going to the left in the meaning,
    //  that it crosses the opposite straight direction?!
    vector<NBEdge*> incoming(*_incomingEdges);
    sort(incoming.begin(), incoming.end(),
         NBContHelper::edge_opposite_direction_sorter(from));
    NBEdge *opposite = *(incoming.begin());
    assert(opposite!=from);
    EdgeVector::const_iterator i =
        find(_allEdges.begin(), _allEdges.end(), from);
    NBContHelper::nextCW(&_allEdges, i);
    while (true) {
        if ((*i)==opposite) {
            return false;
        }
        if ((*i)==to) {
            return true;
        }
        NBContHelper::nextCW(&_allEdges, i);
    }
    return false;
}


bool
NBNode::forbids(NBEdge *possProhibitorFrom, NBEdge *possProhibitorTo,
                NBEdge *possProhibitedFrom, NBEdge *possProhibitedTo,
                bool regardNonSignalisedLowerPriority) const
{
    return _request->forbids(possProhibitorFrom, possProhibitorTo,
                             possProhibitedFrom, possProhibitedTo,
                             regardNonSignalisedLowerPriority);
}


bool
NBNode::foes(NBEdge *from1, NBEdge *to1, NBEdge *from2, NBEdge *to2) const
{
    return _request->foes(from1, to1, from2, to2);
}


void
NBNode::remapRemoved(NBTrafficLightLogicCont &tc,
                     NBEdge *removed, const EdgeVector &incoming,
                     const EdgeVector &outgoing)
{
    assert(find(incoming.begin(), incoming.end(), removed)==incoming.end());
    bool changed = true;
    while (changed) {
        changed = false;
        NBConnectionProhibits blockedConnectionsTmp = _blockedConnections;
        NBConnectionProhibits blockedConnectionsNew;
        // remap in connections
        for (NBConnectionProhibits::iterator i=blockedConnectionsTmp.begin(); i!=blockedConnectionsTmp.end(); i++) {
            const NBConnection &blocker = (*i).first;
            const NBConnectionVector &blocked = (*i).second;
            // check the blocked connections first
            // check whether any of the blocked must be changed
            bool blockedChanged = false;
            NBConnectionVector newBlocked;
            NBConnectionVector::const_iterator j;
            for (j=blocked.begin(); j!=blocked.end(); j++) {
                const NBConnection &sblocked = *j;
                if (sblocked.getFrom()==removed||sblocked.getTo()==removed) {
                    blockedChanged = true;
                }
            }
            // adapt changes if so
            for (j=blocked.begin(); blockedChanged&&j!=blocked.end(); j++) {
                const NBConnection &sblocked = *j;
                if (sblocked.getFrom()==removed&&sblocked.getTo()==removed) {
                    /*                    for(EdgeVector::const_iterator k=incoming.begin(); k!=incoming.end(); k++) {
                    !!!                        newBlocked.push_back(NBConnection(*k, *k));
                                        }*/
                } else if (sblocked.getFrom()==removed) {
                    assert(sblocked.getTo()!=removed);
                    for (EdgeVector::const_iterator k=incoming.begin(); k!=incoming.end(); k++) {
                        newBlocked.push_back(NBConnection(*k, sblocked.getTo()));
                    }
                } else if (sblocked.getTo()==removed) {
                    assert(sblocked.getFrom()!=removed);
                    for (EdgeVector::const_iterator k=outgoing.begin(); k!=outgoing.end(); k++) {
                        newBlocked.push_back(NBConnection(sblocked.getFrom(), *k));
                    }
                } else {
                    newBlocked.push_back(NBConnection(sblocked.getFrom(), sblocked.getTo()));
                }
            }
            if (blockedChanged) {
                blockedConnectionsNew[blocker] = newBlocked;
                changed = true;
            }
            // if the blocked were kept
            else {
                if (blocker.getFrom()==removed&&blocker.getTo()==removed) {
                    changed = true;
                    /*                    for(EdgeVector::const_iterator k=incoming.begin(); k!=incoming.end(); k++) {
                    !!!                        blockedConnectionsNew[NBConnection(*k, *k)] = blocked;
                                        }*/
                } else if (blocker.getFrom()==removed) {
                    assert(blocker.getTo()!=removed);
                    changed = true;
                    for (EdgeVector::const_iterator k=incoming.begin(); k!=incoming.end(); k++) {
                        blockedConnectionsNew[NBConnection(*k, blocker.getTo())] = blocked;
                    }
                } else if (blocker.getTo()==removed) {
                    assert(blocker.getFrom()!=removed);
                    changed = true;
                    for (EdgeVector::const_iterator k=outgoing.begin(); k!=outgoing.end(); k++) {
                        blockedConnectionsNew[NBConnection(blocker.getFrom(), *k)] = blocked;
                    }
                } else {
                    blockedConnectionsNew[blocker] = blocked;
                }
            }
        }
        _blockedConnections = blockedConnectionsNew;
    }
    // remap in traffic lights
    tc.remapRemoved(removed, incoming, outgoing);
}


void
NBNode::addTrafficLight(NBTrafficLightDefinition *tld)
{
    myTrafficLights.insert(tld);
}


NBMMLDirection
NBNode::getMMLDirection(NBEdge *incoming, NBEdge *outgoing) const
{
    if (outgoing==0) {
        return MMLDIR_NODIR;
    }
    if (incoming->isTurningDirectionAt(this, outgoing)) {
        return MMLDIR_TURN;
    }
    SUMOReal angle =
        NBHelpers::normRelAngle(
            incoming->getAngle(), outgoing->getAngle());
    // ok, should be a straight connection
    if (abs(angle)<45) {
        return MMLDIR_STRAIGHT;
    }

    NBMMLDirection tmp;
    // check for left and right, first
    if (angle>0) {
        // check whether any othe edge outgoes further to right
        EdgeVector::const_iterator i =
            find(_allEdges.begin(), _allEdges.end(), outgoing);
        NBContHelper::nextCW(&_allEdges, i);
        while ((*i)!=incoming) {
            if ((*i)->getFromNode()==this) {
                return MMLDIR_PARTRIGHT;
            }
            NBContHelper::nextCW(&_allEdges, i);
        }
        return MMLDIR_RIGHT;
    } else {
        // check whether any othe edge outgoes further to right
        EdgeVector::const_iterator i =
            find(_allEdges.begin(), _allEdges.end(), outgoing);
        NBContHelper::nextCCW(&_allEdges, i);
        while ((*i)!=incoming) {
            if ((*i)->getFromNode()==this&&!incoming->isTurningDirectionAt(this, *i)) {
                return MMLDIR_PARTLEFT;
            }
            NBContHelper::nextCCW(&_allEdges, i);
        }
        return MMLDIR_LEFT;
    }
    return tmp;
}


char
NBNode::stateCode(NBEdge *incoming, NBEdge *outgoing, int fromlane)
{
    if (outgoing==0) {
        return 'O'; // always off
    }
    if (_type==NODETYPE_RIGHT_BEFORE_LEFT) {
        return '='; // all the same
    }
    if (mustBrake(incoming, outgoing, fromlane)) {
        return 'm'; // minor road
    }
    // traffic lights are not regardedm here
    return 'M';
}


bool
NBNode::checkIsRemovable() const
{
    // check whether this node is included in a traffic light
    if (myTrafficLights.size()!=0) {
        return false;
    }
    EdgeVector::const_iterator i;
    // one in, one out -> just a geometry ...
    if (_outgoingEdges->size()==1&&_incomingEdges->size()==1) {
        // ... if types match ...
        if (!(*_incomingEdges)[0]->expandableBy((*_outgoingEdges)[0])) {
            return false;
        }
        //
        return (*_incomingEdges)[0]->getFromNode()!=(*_outgoingEdges)[0]->getToNode();
    }
    // two in, two out -> may be something else
    if (_outgoingEdges->size()==2&&_incomingEdges->size()==2) {
        // check whether the origin nodes of the incoming edges differ
        std::set<NBNode*> origSet;
        for (i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
            origSet.insert((*i)->getFromNode());
        }
        if (origSet.size()<2) {
            return false;
        }
        // check whether this node is an intermediate node of
        //  a two-directional street
        for (i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
            // try to find the opposite direction
            NBNode *origin = (*i)->getFromNode();
            // find the back direction of the current edge
            EdgeVector::const_iterator j =
                find_if(_outgoingEdges->begin(), _outgoingEdges->end(),
                        NBContHelper::edge_with_destination_finder(origin));
            // check whether the back direction exists
            if (j!=_outgoingEdges->end()) {
                // check whether the edge from the backdirection (must be
                //  the counter-clockwise one) may be joined with the current
                NBContHelper::nextCCW(_outgoingEdges, j);
                // check whether the types allow joining
                if (!(*i)->expandableBy(*j)) {
                    return false;
                }
            } else {
                // ok, at least one outgoing edge is not an opposite
                //  of an incoming one
                return false;
            }
        }
        return true;
    }
    // ok, a real node
    return false;
}


std::vector<std::pair<NBEdge*, NBEdge*> >
NBNode::getEdgesToJoin() const
{
    assert(checkIsRemovable());
    std::vector<std::pair<NBEdge*, NBEdge*> > ret;
    // one in, one out-case
    if (_outgoingEdges->size()==1&&_incomingEdges->size()==1) {
        ret.push_back(
            std::pair<NBEdge*, NBEdge*>(
                (*_incomingEdges)[0], (*_outgoingEdges)[0]));
        return ret;
    }
    // two in, two out-case
    for (EdgeVector::const_iterator i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
        NBNode *origin = (*i)->getFromNode();
        EdgeVector::const_iterator j =
            find_if(_outgoingEdges->begin(), _outgoingEdges->end(),
                    NBContHelper::edge_with_destination_finder(origin));
        NBContHelper::nextCCW(_outgoingEdges, j);
        ret.push_back(std::pair<NBEdge*, NBEdge*>(*i, *j));
    }
    return ret;
}


const Position2DVector &
NBNode::getShape() const
{
    return myPoly;
}

std::string
NBNode::getInternalLaneID(NBEdge *from, size_t fromlane,
                          NBEdge *to, size_t tolane) const
{
    size_t l = 0;
    for (EdgeVector::const_iterator i=_incomingEdges->begin(); i!=_incomingEdges->end(); i++) {
        size_t noLanesEdge = (*i)->getNoLanes();
        for (size_t j=0; j<noLanesEdge; j++) {
            const EdgeLaneVector &elv = (*i)->getEdgeLanesFromLane(j);
            for (EdgeLaneVector::const_iterator k=elv.begin(); k!=elv.end(); k++) {
                if ((*k).edge==0) {
                    continue;
                }
                if ((from==*i)&&(j==fromlane)&&((*k).edge==to)&&((*k).lane==tolane)) {
                    return ":" + _id + "_" + toString<size_t>(l);
                }
                l++;
            }
        }
    }
    throw 1;
}


bool
NBNode::isTLControlled() const
{
    return myTrafficLights.size()!=0;
}


SUMOReal
NBNode::getMaxEdgeWidth() const
{
    EdgeVector::const_iterator i=_allEdges.begin();
    assert(i!=_allEdges.end());
    SUMOReal ret = (*i)->width();
    ++i;
    for (; i!=_allEdges.end(); i++) {
        ret = ret > (*i)->width()
              ? ret
              : (*i)->width();
    }
    return ret;
}


NBEdge *
NBNode::getConnectionTo(NBNode *n) const
{
    vector<NBEdge*>::iterator i;
    for (i=_outgoingEdges->begin(); i!=_outgoingEdges->end(); i++) {
        if ((*i)->getToNode()==n) {
            return (*i);
        }
    }
    return 0;
}


bool
NBNode::isNearDistrict() const
{
    EdgeVector edges;
    copy(getIncomingEdges().begin(), getIncomingEdges().end(),
         back_inserter(edges));
    copy(getOutgoingEdges().begin(), getOutgoingEdges().end(),
         back_inserter(edges));
    bool districtFound = false;
    if (edges.size()>8) {
        return false;
    }
    bool amSelfDistrict = false;
    for (EdgeVector::const_iterator j=edges.begin(); !amSelfDistrict&&j!=edges.end()&&!districtFound; ++j) {
        NBEdge *t = *j;
        if (t->getBasicType()==NBEdge::EDGEFUNCTION_SOURCE||t->getBasicType()==NBEdge::EDGEFUNCTION_SINK) {
            // is self a district
            return false;
            continue;
        }
        NBNode *other = 0;
        if (t->getToNode()==this) {
            other = t->getFromNode();
        } else {
            other = t->getToNode();
        }
        EdgeVector edges2;
        copy(other->getIncomingEdges().begin(), other->getIncomingEdges().end(),
             back_inserter(edges2));
        copy(other->getOutgoingEdges().begin(), other->getOutgoingEdges().end(),
             back_inserter(edges2));
        for (EdgeVector::const_iterator k=edges2.begin(); k!=edges2.end()&&!districtFound; ++k) {
            if ((*k)->getBasicType()==NBEdge::EDGEFUNCTION_SOURCE||(*k)->getBasicType()==NBEdge::EDGEFUNCTION_SINK) {
                return true;
            }
        }
    }
    return false;
}


bool
NBNode::isDistrict() const
{
    if (_type==NODETYPE_DISTRICT) {
        return true;
    }
    EdgeVector edges;
    copy(getIncomingEdges().begin(), getIncomingEdges().end(),
         back_inserter(edges));
    copy(getOutgoingEdges().begin(), getOutgoingEdges().end(),
         back_inserter(edges));
    bool districtFound = false;
    bool amSelfDistrict = false;
    for (EdgeVector::const_iterator j=edges.begin(); !amSelfDistrict&&j!=edges.end()&&!districtFound; ++j) {
        NBEdge *t = *j;
        if (t->getBasicType()==NBEdge::EDGEFUNCTION_SOURCE||t->getBasicType()==NBEdge::EDGEFUNCTION_SINK) {
            // is self a district
            return true;
        }
    }
    return false;
}



/****************************************************************************/

