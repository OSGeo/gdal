/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implementation of topologic sorting over a directed acyclic graph
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys dot com>
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

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <vector>

#include <cassert>

namespace gdal
{

// See https://en.wikipedia.org/wiki/Directed_acyclic_graph
template<class T, class V = std::string> class DirectedAcyclicGraph
{
    std::set<T> nodes;
    std::map<T, std::set<T>> incomingNodes; // incomingNodes[j][i] means an edge from i to j
    std::map<T, std::set<T>> outgoingNodes; // outgoingNodes[i][j] means an edge from i to j
    std::map<T, V> names;

public:
    DirectedAcyclicGraph() = default;

    void addNode(const T& i, const V& s) { nodes.insert(i); names[i] = s; }
    void removeNode(const T& i);
    const char* addEdge(const T& i, const T& j);
    const char* removeEdge(const T& i, const T& j);
    bool isTherePathFromTo(const T& i, const T& j) const;
    std::vector<T> findStartingNodes() const;
    std::vector<T> getTopologicalOrdering();
};

template<class T, class V>
void DirectedAcyclicGraph<T, V>::removeNode(const T& i)
{
    nodes.erase(i);
    names.erase(i);

    {
        auto incomingIter = incomingNodes.find(i);
        if( incomingIter != incomingNodes.end() )
        {
            for( const T& j: incomingIter->second )
            {
                auto outgoingIter = outgoingNodes.find(j);
                assert(outgoingIter != outgoingNodes.end());
                auto iterJI = outgoingIter->second.find(i);
                assert(iterJI != outgoingIter->second.end());
                outgoingIter->second.erase(iterJI);
                if( outgoingIter->second.empty() )
                    outgoingNodes.erase(outgoingIter);
            }
            incomingNodes.erase(incomingIter);
        }
    }

    {
        auto outgoingIter = outgoingNodes.find(i);
        if( outgoingIter != outgoingNodes.end() )
        {
            for( const T& j: outgoingIter->second )
            {
                auto incomingIter = incomingNodes.find(j);
                assert(incomingIter != incomingNodes.end());
                auto iterJI = incomingIter->second.find(i);
                assert(iterJI != incomingIter->second.end());
                incomingIter->second.erase(iterJI);
                if( incomingIter->second.empty() )
                    incomingNodes.erase(incomingIter);
            }
            outgoingNodes.erase(outgoingIter);
        }
    }
}

template<class T, class V>
const char* DirectedAcyclicGraph<T, V>::addEdge(const T& i, const T& j)
{
    if( i == j )
    {
        return "self cycle";
    }
    const auto iterI = outgoingNodes.find(i);
    if( iterI != outgoingNodes.end() &&
        iterI->second.find(j) != iterI->second.end() )
    {
        return "already inserted edge";
    }

    if( nodes.find(i) == nodes.end() )
    {
        return "node i unknown";
    }
    if( nodes.find(j) == nodes.end() )
    {
        return "node j unknown";
    }

    if( isTherePathFromTo(j, i) )
    {
        return "can't add edge: this would cause a cycle";
    }

    outgoingNodes[i].insert(j);
    incomingNodes[j].insert(i);
    return nullptr;
}

template<class T, class V>
const char* DirectedAcyclicGraph<T, V>::removeEdge(const T& i, const T& j)
{
    auto iterI = outgoingNodes.find(i);
    if( iterI == outgoingNodes.end() )
        return "no outgoing nodes from i";
    auto iterIJ = iterI->second.find(j);
    if( iterIJ == iterI->second.end() )
        return "no outgoing node from i to j";
    iterI->second.erase(iterIJ);
    if( iterI->second.empty() )
        outgoingNodes.erase(iterI);

    auto iterJ = incomingNodes.find(j);
    assert( iterJ != incomingNodes.end() );
    auto iterJI = iterJ->second.find(i);
    assert( iterJI != iterJ->second.end() );
    iterJ->second.erase(iterJI);
    if( iterJ->second.empty() )
        incomingNodes.erase(iterJ);

    return nullptr;
}

template<class T, class V>
bool DirectedAcyclicGraph<T, V>::isTherePathFromTo(const T& i, const T& j) const
{
    std::set<T> plannedForVisit;
    std::stack<T> toVisit;
    toVisit.push(i);
    plannedForVisit.insert(i);
    while(!toVisit.empty())
    {
        const T n = toVisit.top();
        toVisit.pop();
        if( n == j )
            return true;
        const auto iter = outgoingNodes.find(n);
        if( iter != outgoingNodes.end() )
        {
            for( const T& k: iter->second )
            {
                if( plannedForVisit.find(k) == plannedForVisit.end() )
                {
                    plannedForVisit.insert(k);
                    toVisit.push(k);
                }
            }
        }
    }
    return false;
}

template<class T, class V>
std::vector<T> DirectedAcyclicGraph<T,V>::findStartingNodes() const
{
    std::vector<T> ret;
    for( const auto& i: nodes )
    {
        if( incomingNodes.find(i) == incomingNodes.end() )
            ret.emplace_back(i);
    }
    return ret;
}

// Kahn's algorithm: https://en.wikipedia.org/wiki/Topological_sorting#Kahn's_algorithm
template<class T, class V>
std::vector<T> DirectedAcyclicGraph<T,V>::getTopologicalOrdering()
{
    std::vector<T> ret;
    ret.reserve(nodes.size());

    const auto cmp = [this](const T& a, const T& b) {
        return names.find(a)->second < names.find(b)->second;
    };
    std::set<T, decltype(cmp)> S(cmp);

    const auto sn = findStartingNodes();
    for( const auto& i: sn )
        S.insert(i);

    while( true )
    {
        auto iterFirst = S.begin();
        if( iterFirst == S.end() )
            break;
        const auto n = *iterFirst;
        S.erase(iterFirst);
        ret.emplace_back(n);

        const auto iter = outgoingNodes.find(n);
        if( iter != outgoingNodes.end() )
        {
            // Need to take a copy as we remove edges during iteration
            const auto myOutgoingNodes = iter->second;
            for( const T& m: myOutgoingNodes )
            {
                const char* retRemoveEdge = removeEdge(n, m);
                (void)retRemoveEdge;
                assert(retRemoveEdge == nullptr);
                if( incomingNodes.find(m) == incomingNodes.end() )
                {
                    S.insert(m);
                }
            }
        }
    }

    // Should not happen for a direct acyclic graph
    assert(incomingNodes.empty());
    assert(outgoingNodes.empty());

    return ret;
}

} // namespace gdal
