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

#include "gnm_priv.h"
#include <vector>
#include <queue>
#include <set>

// Types declarations.

typedef std::vector<GNMGFID> GNMVECTOR, *LPGNMVECTOR;
typedef std::pair<GNMGFID,GNMGFID> EDGEVERTEXPAIR;
typedef std::vector< EDGEVERTEXPAIR > GNMPATH;

struct GNMStdEdge
{
    GNMGFID nSrcVertexFID;
    GNMGFID nTgtVertexFID;
    bool bIsBidir;
    double dfDirCost;
    double dfInvCost;
    bool bIsBloked;
};

struct GNMStdVertex
{
    GNMVECTOR anOutEdgeFIDs;
    bool bIsBloked;
};

/**
 * The simple graph class, which holds the appropriate for
 * calculations graph in memory (based on STL containers) and provides
 * several basic algorithms of this graph's analysis. See the methods of
 * this class for details. The common thing for all analysis methods is that
 * all of them return the results in the array of GFIDs form. Use the
 * GNMGraph class to receive the results in OGRLayer form.
 * NOTE: GNMGraph holds the whole graph in memory, so it can consume
 * a lot of memory if operating huge networks.
 *
 * @since GDAL 2.1
 */

class CPL_DLL GNMGraph
{
public:
    GNMGraph();
    virtual ~GNMGraph();

    // GNMGraph

    /**
     * @brief Add a vertex to the graph
     *
     * NOTE: if there are vertices with these ID's already - nothing will be
     * added.
     *
     * @param nFID - vertex identificator
     */
    virtual void AddVertex(GNMGFID nFID);

    /**
     * @brief Delete vertex from the graph
     * @param nFID Vertex identificator
     */
    virtual void DeleteVertex(GNMGFID nFID);

    /**
     * @brief Add an edge to the graph
     * @param nConFID Edge identificator
     * @param nSrcFID Source vertex identificator
     * @param nTgtFID Target vertex identificator
     * @param bIsBidir Is bidirectional
     * @param dfCost Cost
     * @param dfInvCost Inverse cost
     */
    virtual void AddEdge(GNMGFID nConFID, GNMGFID nSrcFID, GNMGFID nTgtFID,
                         bool bIsBidir, double dfCost, double dfInvCost);

    /**
     * @brief Delete edge from graph
     * @param nConFID Edge identificator
     */
    virtual void DeleteEdge(GNMGFID nConFID);

    /**
     * @brief Change edge properties
     * @param nConFID Edge identificator
     * @param dfCost Cost
     * @param dfInvCost Inverse cost
     */
    virtual void ChangeEdge(GNMGFID nFID, double dfCost, double dfInvCost);

    /**
     * @brief Change the block state of edge or vertex
     * @param nFID Identificator
     * @param bIsBlock Block or unblock
     */
    virtual void ChangeBlockState (GNMGFID nFID, bool bBlock);

    /**
     * @brief Check if vertex is blocked
     * @param nFID Vertex identificator
     * @return true if blocked, false if not blocked.
     */
    virtual bool CheckVertexBlocked(GNMGFID nFID) const;

    /**
     * @brief Change all vertice and edges block state.
     *
     * This is mainly use for unblock all vertices and edges.
     *
     * @param bIsBlock Block or unblock
     */
    virtual void ChangeAllBlockState (bool bBlock = false);

    /**
     * @brief An implementation of Dijkstra shortest path algorithm.
     *
     * Returns the best path between nStartFID and nEndFID features. Method
     * uses @see DijkstraShortestPathTree and after that searches in
     * the resulting tree the path from end to start point.
     *
     * @param nStartFID Start identificator
     * @param nEndFID End identificator
     * @return an array of best path included identificator of vertices and
     * edges
     */
    virtual GNMPATH DijkstraShortestPath(GNMGFID nStartFID, GNMGFID nEndFID);

    /**
     * @brief An implementation of KShortest paths algorithm.
     *
     * Calculates several best paths between two points. Method takes in account
     * the blocking state of features, i.e. the blocked features are the barriers
     * during the routing process.
     *
     * @param nStartFID Vertex identificator from which to start paths calculating.
     * @param nEndFID Vertex identificator to which the path will be calculated.
     * @param nK How much best paths try to find between start and end points.
     * @return an array of best paths. Each path is an array of pairs, where the
     * first in a pair is a vertex identificator and the second is an edge
     * identificator leading to this vertex. The elements in a path array are
     * sorted by the path segments order, i.e. the first is the pair (nStartFID,
     * -1) and the last is (nEndFID, <some edge>).
     * If there is no any path between start and end vertex the returned array
     * of paths will be empty. Also the actual amount of paths in the returned
     * array can be less or equal than the nK parameter.
     */
    virtual std::vector<GNMPATH> KShortestPaths(GNMGFID nStartFID,
                                                GNMGFID nEndFID, size_t nK);
    
    /**
     * @brief Search connected components of the network 
     * 
     * Returns the resource distribution in the network. Method search starting 
     * from the features identificators from input array. Uses the recursive 
     * Breadth-first search algorithm to find the connected to the given vector 
     * of GFIDs components. Method takes in account the blocking state of 
     * features, i.e. the blocked features are the barriers during the routing 
     * process.
     *  
     * @param anEmittersIDs - array of emmiters identificators
     * @return an array of connected identificators
     */
    virtual GNMPATH ConnectedComponents(const GNMVECTOR &anEmittersIDs);

    virtual void Clear();
protected:
    /**
     * @brief Method to create best paht tree.
     *
     * Calculates and builds the best path tree with the Dijkstra
     * algorithm starting from the nFID. Method takes in account the blocking
     * state of features, i.e. the blocked features are the barriers during the
     * routing process.
     *
     * @param nFID - Vertex identificator from which to start tree building.
     * @param mnPathTree - means < vertex id, edge id >
     * @return std::map where the first is vertex identificator and the second
     * is the edge identificator, which is the best way to the current vertex.
     * The identificator to the start vertex is -1. If the vertex is isolated
     * the returned map will be empty.
     */
    virtual void DijkstraShortestPathTree(GNMGFID nFID,
                                  const std::map<GNMGFID, GNMStdEdge> &mstEdges,
                                        std::map<GNMGFID, GNMGFID> &mnPathTree);
    virtual GNMPATH DijkstraShortestPath(GNMGFID nStartFID, GNMGFID nEndFID,
                                 const std::map<GNMGFID, GNMStdEdge> &mstEdges);

    virtual const LPGNMVECTOR GetOutEdges(GNMGFID nFID) const;
    virtual GNMGFID GetOppositVertex(GNMGFID nEdgeFID, GNMGFID nVertexFID) const;
    virtual void TraceTargets(std::queue<GNMGFID> &vertexQueue, 
                                std::set<GNMGFID> &markedVertIds, 
                                GNMPATH &connectedIds);
protected:
    std::map<GNMGFID, GNMStdVertex> m_mstVertices;
    std::map<GNMGFID, GNMStdEdge>   m_mstEdges;
};
