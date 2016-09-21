/******************************************************************************
 * $Id$
 *
 * Project:  GDAL/OGR Geography Network support (Geographic Network Model)
 * Purpose:  GNM graph implementation.
 * Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
 *           Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014, Mikhail Gusev
 * Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gnmgraph.h"
#include <algorithm>
#include <limits>
#include <set>

GNMGraph::GNMGraph()
{
}

GNMGraph::~GNMGraph()
{

}

void GNMGraph::AddVertex(GNMGFID nFID)
{
    if(m_mstVertices.find(nFID) != m_mstVertices.end())
        return;

    GNMStdVertex stVertex;
    stVertex.bIsBloked = false;
    m_mstVertices[nFID] = stVertex;
}

void GNMGraph::DeleteVertex(GNMGFID nFID)
{
    m_mstVertices.erase(nFID);

    // remove all edges with this vertex
    std::vector<GNMGFID> aoIdsToErase;
    for(std::map<GNMGFID,GNMStdEdge>::iterator it = m_mstEdges.begin();
        it != m_mstEdges.end(); ++it)
    {
        if(it->second.nSrcVertexFID == nFID || it->second.nTgtVertexFID == nFID)
            aoIdsToErase.push_back(it->first);
    }
    for(size_t i=0;i<aoIdsToErase.size();i++)
        m_mstEdges.erase(aoIdsToErase[i]);
}

void GNMGraph::AddEdge(GNMGFID nConFID, GNMGFID nSrcFID, GNMGFID nTgtFID,
                       bool bIsBidir, double dfCost, double dfInvCost)
{
    // We do not add edge if an edge with the same id already exist
    // because each edge must have only one source and one target vertex.
    std::map<GNMGFID,GNMStdEdge>::iterator it = m_mstEdges.find(nConFID);
    if (it != m_mstEdges.end())
    {
        CPLError( CE_Failure, CPLE_AppDefined, "The edge already exist." );
        return;
    }

    AddVertex(nSrcFID);
    AddVertex(nTgtFID);

    std::map<GNMGFID, GNMStdVertex>::iterator itSrs = m_mstVertices.find(nSrcFID);
    std::map<GNMGFID, GNMStdVertex>::iterator itTgt = m_mstVertices.find(nTgtFID);

    // Insert edge to the array of edges.
    GNMStdEdge stEdge;
    stEdge.nSrcVertexFID = nSrcFID;
    stEdge.nTgtVertexFID = nTgtFID;
    stEdge.bIsBidir = bIsBidir;
    stEdge.dfDirCost = dfCost;
    stEdge.dfInvCost = dfInvCost;
    stEdge.bIsBloked = false;

    m_mstEdges[nConFID] = stEdge;

    if (bIsBidir)
    {
        itSrs->second.anOutEdgeFIDs.push_back(nConFID);
        itTgt->second.anOutEdgeFIDs.push_back(nConFID);
    }
    else
    {
        itSrs->second.anOutEdgeFIDs.push_back(nConFID);
    }
}

void GNMGraph::DeleteEdge(GNMGFID nConFID)
{
    m_mstEdges.erase(nConFID);

    // remove edge from all vertices anOutEdgeFIDs
    for(std::map<GNMGFID, GNMStdVertex>::iterator it = m_mstVertices.begin();
        it != m_mstVertices.end(); ++it)
    {
        it->second.anOutEdgeFIDs.erase(
                    std::remove( it->second.anOutEdgeFIDs.begin(),
                                 it->second.anOutEdgeFIDs.end(), nConFID),
                    it->second.anOutEdgeFIDs.end());
    }
}

void GNMGraph::ChangeEdge(GNMGFID nFID, double dfCost, double dfInvCost)
{
    std::map<GNMGFID, GNMStdEdge>::iterator it = m_mstEdges.find(nFID);
    if (it != m_mstEdges.end())
    {
        it->second.dfDirCost = dfCost;
        it->second.dfInvCost = dfInvCost;
    }
}

void GNMGraph::ChangeBlockState(GNMGFID nFID, bool bBlock)
{
    // check vertices
    std::map<GNMGFID, GNMStdVertex>::iterator itv = m_mstVertices.find(nFID);
    if(itv != m_mstVertices.end())
    {
        itv->second.bIsBloked = bBlock;
        return;
    }

    // check edges
    std::map<GNMGFID, GNMStdEdge>::iterator ite = m_mstEdges.find(nFID);
    if (ite != m_mstEdges.end())
    {
        ite->second.bIsBloked = bBlock;
    }
}

bool GNMGraph::CheckVertexBlocked(GNMGFID nFID) const
{
    std::map<GNMGFID, GNMStdVertex>::const_iterator it = m_mstVertices.find(nFID);
    if (it != m_mstVertices.end())
        return it->second.bIsBloked;
    return false;
}

void GNMGraph::ChangeAllBlockState(bool bBlock)
{
    for(std::map<GNMGFID, GNMStdVertex>::iterator itv = m_mstVertices.begin();
        itv != m_mstVertices.end(); ++itv)
    {
        itv->second.bIsBloked = bBlock;
    }

    for(std::map<GNMGFID, GNMStdEdge>::iterator ite = m_mstEdges.begin();
        ite != m_mstEdges.end(); ++ite)
    {
        ite->second.bIsBloked = bBlock;
    }
}

GNMPATH GNMGraph::DijkstraShortestPath( GNMGFID nStartFID, GNMGFID nEndFID,
                                 const std::map<GNMGFID, GNMStdEdge> &mstEdges)
{
    std::map<GNMGFID, GNMGFID> mnShortestTree;
    DijkstraShortestPathTree(nStartFID, mstEdges, mnShortestTree);

    // We search for a path in the resulting tree, starting from end point to
    // start point.

    GNMPATH aoShortestPath;
    GNMGFID nNextVertexId = nEndFID;
    std::map<GNMGFID, GNMGFID>::iterator it;
    EDGEVERTEXPAIR buf;

    while (true)
    {
        it = mnShortestTree.find(nNextVertexId);

        if (it == mnShortestTree.end())
        {
            // We haven't found the start vertex - there is no path between
            // to given vertices in a shortest-path tree.
            break;
        }
        else if (it->first == nStartFID)
        {
            // We've reached the start vertex and return an array.
            aoShortestPath.push_back( std::make_pair(nNextVertexId, -1) );

            // Revert array because the first vertex is now the last in path.
            int size = static_cast<int>(aoShortestPath.size());
            for (int i = 0; i < size / 2; ++i)
            {
                buf = aoShortestPath[i];
                aoShortestPath[i] = aoShortestPath[size - i - 1];
                aoShortestPath[size - i - 1] = buf;
            }
            return aoShortestPath;
        }
        else
        {
            // There is only one edge which leads to this vertex, because we
            // analyse a tree. We add this edge with its target vertex into
            // final array.
            aoShortestPath.push_back(std::make_pair(nNextVertexId, it->second));

            // An edge has only two vertexes, so we get the opposite one to the
            // current vertex in order to continue search backwards.
            nNextVertexId = GetOppositVertex(it->second, it->first);
        }
    }

    // return empty array
    GNMPATH oRet;
    return oRet;
}

GNMPATH GNMGraph::DijkstraShortestPath( GNMGFID nStartFID, GNMGFID nEndFID)
{
    return DijkstraShortestPath(nStartFID, nEndFID, m_mstEdges);
}

std::vector<GNMPATH> GNMGraph::KShortestPaths(GNMGFID nStartFID, GNMGFID nEndFID,
                                              size_t nK)
{
    // Resulting array with paths.
    // A will be sorted by the path costs' descending.
    std::vector<GNMPATH> A;

    if (nK <= 0)
        return A; // return empty array if K is incorrect.

    // Temporary array for storing paths-candidates.
    // B will be automatically sorted by the cost descending order. We
    // need multimap because there can be physically different paths but
    // with the same costs.
    std::multimap<double, GNMPATH> B;

    // Firstly get the very shortest path.
    // Note, that it is important to obtain the path from DijkstraShortestPath()
    // as vector, rather than the map, because we need the correct order of the
    // path segments in the Yen's algorithm iterations.
    GNMPATH aoFirstPath = DijkstraShortestPath(nStartFID, nEndFID);
    if (aoFirstPath.empty())
        return A; // return empty array if there is no path between points.

    A.push_back(aoFirstPath);

    size_t i, k, l;
    GNMPATH::iterator itAk, tempIt, itR;
    std::vector<GNMPATH>::iterator itA;
    std::map<GNMGFID, double>::iterator itDel;
    GNMPATH aoRootPath, aoRootPathOther, aoSpurPath;
    GNMGFID nSpurNode, nVertexToDel, nEdgeToDel;
    double dfSumCost;

    std::map<GNMGFID, GNMStdEdge> mstEdges = m_mstEdges;

    for (k = 0; k < nK - 1; ++k) // -1 because we have already found one
    {
        std::map<GNMGFID, double> mDeletedEdges; // for infinity costs assignement
        itAk = A[k].begin();

        for (i = 0; i < A[k].size() - 1; ++i) // avoid end node
        {
            // Get the current node.
            nSpurNode = A[k][i].first;

            // Get the root path from the 0 to the current node.

            // Equivalent to A[k][i]
            // because we will use std::vector::assign, which assigns [..)
            // range, not [..]
            ++itAk;

            aoRootPath.assign(A[k].begin(), itAk);

            // Remove old incidence edges of all other best paths.
            // i.e. if the spur vertex can be reached in already found best
            // paths we must remove the following edge after the end of root
            // path from the graph in order not to take in account these already
            // seen best paths.
            // i.e. it ensures that the spur path will be different.
            for (itA = A.begin(); itA != A.end(); ++itA)
            {
                // check if the number of node exceed the number of last node in
                // the path array (i.e. if one of the A paths has less amount of
                // segments than the current candidate path)
                if (i >= itA->size())
                    continue;

                // + 1, because we will use std::vector::assign, which assigns
                // [..) range, not [..]
                aoRootPathOther.assign(itA->begin(), itA->begin() + i + 1);

                // Get the edge which follows the spur node for current path
                // and delete it.
                //
                // NOTE: we do not delete edges due to performance reasons,
                // because the deletion of edge and all its GFIDs in vertex
                // records is slower than the infinity cost assignment.

                // also check if node number exceed the number of the last node
                // in root array.
                if ((aoRootPath == aoRootPathOther) &&
                        (i < aoRootPathOther.size()))
                {
                    tempIt = itA->begin() + i + 1;
                    mDeletedEdges.insert(std::make_pair(tempIt->second,
                                           mstEdges[tempIt->second].dfDirCost));
                    mstEdges[tempIt->second].dfDirCost
                                      = std::numeric_limits<double>::infinity();
                }
            }

            // Remove root path nodes from the graph. If we do not delete them
            // the path will be found backwards and some parts of the path will
            // duplicate the parts of old paths.
            // Note: we "delete" all the incidence to the root nodes edges, so
            // to restore them in a common way.

            // end()-1, because we should not remove the spur node
            for (itR = aoRootPath.begin(); itR != aoRootPath.end() - 1; ++itR)
            {
                nVertexToDel = itR->first;
                for (l = 0; l < m_mstVertices[nVertexToDel].anOutEdgeFIDs.size();
                     ++l)
                {
                    nEdgeToDel = m_mstVertices[nVertexToDel].anOutEdgeFIDs[l];
                    mDeletedEdges.insert(std::make_pair(nEdgeToDel,
                                               mstEdges[nEdgeToDel].dfDirCost));
                    mstEdges[nEdgeToDel].dfDirCost
                                      = std::numeric_limits<double>::infinity();
                }
            }

            // Find the new best path in the modified graph.
            aoSpurPath = DijkstraShortestPath(nSpurNode, nEndFID, mstEdges);

            // Firstly, restore deleted edges in order to calculate the summary
            // cost of the path correctly later, because the costs will be
            // gathered from the initial graph.
            // We must do it here, after each edge removing, because the later
            // Dijkstra searches must consider these edges.
            for (itDel = mDeletedEdges.begin(); itDel != mDeletedEdges.end();
                 ++itDel)
            {
                mstEdges[itDel->first].dfDirCost = itDel->second;
            }

            mDeletedEdges.clear();

            // If the part of a new best path has been found we form a full one
            // and add it to the candidates array.
            if (!aoSpurPath.empty())
            {
                // + 1 so not to consider the first node in the found path,
                // which is already the last node in the root path
                aoRootPath.insert( aoRootPath.end(), aoSpurPath.begin() + 1,
                                   aoSpurPath.end());
                // Calculate the summary cost of the path.
                // TODO: get the summary cost from the Dejkstra method?
                dfSumCost = 0.0;
                for (itR = aoRootPath.begin(); itR != aoRootPath.end(); ++itR)
                {
                    // TODO: check: Note, that here the current cost can not be
                    // infinity, because every time we assign infinity costs for
                    // edges of old paths, we anyway have the alternative edges
                    // with non-infinity costs.
                    dfSumCost += mstEdges[itR->second].dfDirCost;
                }

                B.insert(std::make_pair(dfSumCost, aoRootPath));
            }
        }

        if (B.empty())
            break;

        // The best path is the first, because the map is sorted accordingly.
        // Note, that here we won't clear the path candidates array and select
        // the best path from all of the rest paths, even from those which were
        // found on previous iterations. That's why we need k iterations at all.
        // Note, that if there were two paths with the same costs and it is the
        // LAST iteration the first occurred path will be added, rather than
        // random.
        A.push_back(B.begin()->second);

        // Sometimes B contains fully duplicate paths. Such duplicates have been
        // formed during the search of alternative for almost the same paths
        // which were already in A.
        // We allowed to add them into B so here we must delete all duplicates.
        while (!B.empty() && B.begin()->second == A.back())
        {
            B.erase(B.begin());
        }
    }

    return A;
}

GNMPATH GNMGraph::ConnectedComponents(const GNMVECTOR &anEmittersIDs)
{
    GNMPATH anConnectedIDs;

    if(anEmittersIDs.empty())
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "Emitters list is empty." );
        return anConnectedIDs;
    }
    std::set<GNMGFID> anMarkedVertIDs;

    std::queue<GNMGFID> anStartQueue;
    GNMVECTOR::const_iterator it;
    for (it = anEmittersIDs.begin(); it != anEmittersIDs.end(); ++it)
    {
        anStartQueue.push(*it);
    }

    // Begin the iterations of the Breadth-first search.
    TraceTargets(anStartQueue, anMarkedVertIDs, anConnectedIDs);

    return anConnectedIDs;
}

void GNMGraph::Clear()
{
    m_mstVertices.clear();
    m_mstEdges.clear();
}

void GNMGraph::DijkstraShortestPathTree(GNMGFID nFID,
                                   const std::map<GNMGFID, GNMStdEdge> &mstEdges,
                                         std::map<GNMGFID, GNMGFID> &mnPathTree)
{
    // Initialize all vertexes in graph with infinity mark.
    double dfInfinity = std::numeric_limits<double>::infinity();

    std::map<GNMGFID, double> mMarks;
    std::map<GNMGFID, GNMStdVertex>::iterator itv;
    for (itv = m_mstVertices.begin(); itv != m_mstVertices.end(); ++itv)
    {
        mMarks[itv->first] = dfInfinity;
    }

    mMarks[nFID] = 0.0;
    mnPathTree[nFID] = -1;

    // Initialize all vertexes as unseen (there are no seen vertexes).
    std::set<GNMGFID> snSeen;

    // We use multimap to maintain the ascending order of costs and because
    // there can be different vertexes with the equal cost.
    std::multimap<double,GNMGFID> to_see;
    std::multimap<double,GNMGFID>::iterator it;
    to_see.insert(std::pair<double,GNMGFID>(0.0, nFID));
    LPGNMCONSTVECTOR panOutcomeEdgeId;

    size_t i;
    GNMGFID nCurrenVertId, nCurrentEdgeId, nTargetVertId;
    double dfCurrentEdgeCost, dfCurrentVertMark, dfNewVertexMark;
    std::map<GNMGFID, GNMStdEdge>::const_iterator ite;

    // Continue iterations while there are some vertexes to see.
    while (!to_see.empty())
    {
        // We must see vertexes with minimal costs at first.
        // In multimap the first cost is the minimal.
        it = to_see.begin();

        nCurrenVertId = it->second;
        dfCurrentVertMark = it->first;
        snSeen.insert(it->second);
        to_see.erase(it);

        // For all neighbours for the current vertex.
        panOutcomeEdgeId = GetOutEdges(nCurrenVertId);
        if(NULL == panOutcomeEdgeId)
            continue;

        for (i = 0; i < panOutcomeEdgeId->size(); ++i)
        {
            nCurrentEdgeId = panOutcomeEdgeId->operator[](i);

            ite = mstEdges.find(nCurrentEdgeId);
            if(ite == mstEdges.end() || ite->second.bIsBloked)
                continue;

            // We go in any edge from source to target so we take only
            // direct cost (even if an edge is bi-directed).
            dfCurrentEdgeCost = ite->second.dfDirCost;

            // While we see outcome edges of current vertex id we definitely
            // know that target vertex id will be target for current edge id.
            nTargetVertId = GetOppositVertex(nCurrentEdgeId, nCurrenVertId);

            // Calculate a new mark assuming the full path cost (mark of the
            // current vertex) to this vertex.
            dfNewVertexMark = dfCurrentVertMark + dfCurrentEdgeCost;

            // Update mark of the vertex if needed.
            if (snSeen.find(nTargetVertId) == snSeen.end() &&
                    dfNewVertexMark < mMarks[nTargetVertId] &&
                    !CheckVertexBlocked(nTargetVertId))
            {
                mMarks[nTargetVertId] = dfNewVertexMark;
                mnPathTree[nTargetVertId] = nCurrentEdgeId;

                // The vertex with minimal cost will be inserted to the
                // beginning.
                to_see.insert(std::pair<double,GNMGFID>(dfNewVertexMark,
                                                        nTargetVertId));
            }
        }
    }
}

LPGNMCONSTVECTOR GNMGraph::GetOutEdges(GNMGFID nFID) const
{
    std::map<GNMGFID,GNMStdVertex>::const_iterator it = m_mstVertices.find(nFID);
    if (it != m_mstVertices.end())
        return &it->second.anOutEdgeFIDs;
    return NULL;
}

GNMGFID GNMGraph::GetOppositVertex(GNMGFID nEdgeFID, GNMGFID nVertexFID) const
{
    std::map<GNMGFID, GNMStdEdge>::const_iterator it = m_mstEdges.find(nEdgeFID);
    if (it != m_mstEdges.end())
    {
        if (nVertexFID == it->second.nSrcVertexFID)
        {
            return it->second.nTgtVertexFID;
        }
        else if (nVertexFID == it->second.nTgtVertexFID)
        {
            return it->second.nSrcVertexFID;
        }
    }
    return -1;
}

void GNMGraph::TraceTargets(std::queue<GNMGFID> &vertexQueue,
                            std::set<GNMGFID> &markedVertIds,
                            GNMPATH &connectedIds)
{
    GNMCONSTVECTOR::const_iterator it;
    std::queue<GNMGFID> neighbours_queue;

    // See all given vertexes except thouse that have been already seen.
    while (!vertexQueue.empty())
    {
        GNMGFID nCurVertID = vertexQueue.front();

        // There may be duplicate unmarked vertexes in a current queue. Check it.
        if (markedVertIds.find(nCurVertID) == markedVertIds.end())
        {
            markedVertIds.insert(nCurVertID);

            // See all outcome edges, add them to connected and than see the target
            // vertex of each edge. Add it to the queue, which will be recursively
            // seen the same way on the next iteration.
            LPGNMCONSTVECTOR panOutcomeEdgeIDs = GetOutEdges(nCurVertID);
            if(NULL != panOutcomeEdgeIDs)
            {
                for (it = panOutcomeEdgeIDs->begin(); it != panOutcomeEdgeIDs->end(); ++it)
                {
                    GNMGFID nCurEdgeID = *it;

                    // ISSUE: think about to return a sequence of vertexes and edges
                    // (which is more universal), as now we are going to return only
                    // sequence of edges.
                    connectedIds.push_back( std::make_pair(nCurVertID, nCurEdgeID) );

                    // Get the only target vertex of this edge. If edge is bidirected
                    // get not that vertex that with nCurVertID.
                    GNMGFID nTargetVertID;
                    /*
                    std::vector<GNMGFID> target_vert_ids = _getTgtVert(cur_edge_id);
                    std::vector<GNMGFID>::iterator itt;
                    for (itt = target_vert_ids.begin(); itt != target_vert_ids.end(); ++itt)
                    {
                        if ((*itt) != cur_vert_id)
                        {
                            target_vert_id = *itt;
                            break;
                        }
                    }
                    */
                    nTargetVertID = GetOppositVertex(nCurEdgeID, nCurVertID);

                    // Avoid marked or blocked vertexes.
                    if ((markedVertIds.find(nTargetVertID) == markedVertIds.end())
                    && (!CheckVertexBlocked(nTargetVertID)))
                        neighbours_queue.push(nTargetVertID);
                }
            }
        }

        vertexQueue.pop();
    }

    if (!neighbours_queue.empty())
        TraceTargets(neighbours_queue, markedVertIds, connectedIds);
}
