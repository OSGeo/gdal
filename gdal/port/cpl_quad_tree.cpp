/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implementation of quadtree building and searching functions.
 *           Derived from shapelib and mapserver implementations
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *           Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 1999-2008, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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
 ******************************************************************************
 */

#include "cpl_port.h"
#include "cpl_quad_tree.h"

#include <cstdio>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"

CPL_CVSID("$Id$")

constexpr int MAX_DEFAULT_TREE_DEPTH = 12;
constexpr int MAX_SUBNODES = 4;

typedef struct _QuadTreeNode QuadTreeNode;

struct _QuadTreeNode
{
  /* area covered by this psNode */
  CPLRectObj    rect;

  int           nFeatures;    /* number of shapes stored at this psNode. */

  int           nNumSubNodes; /* number of active subnodes */

  void        **pahFeatures; /* list of shapes stored at this psNode. */
  CPLRectObj   *pasBounds;

  QuadTreeNode *apSubNode[MAX_SUBNODES];
};

struct _CPLQuadTree
{
  QuadTreeNode             *psRoot;
  CPLQuadTreeGetBoundsFunc  pfnGetBounds;
  int                       nFeatures;
  int                       nMaxDepth;
  int                       nBucketCapacity;
  double                    dfSplitRatio;
};

static void CPLQuadTreeAddFeatureInternal(CPLQuadTree *hQuadTree,
                                          void* hFeature,
                                          const CPLRectObj *pRect);
static void CPLQuadTreeNodeDestroy(QuadTreeNode *psNode);

/* -------------------------------------------------------------------- */
/*      If the following is 0.5, psNodes will be split in half.  If it  */
/*      is 0.6 then each apSubNode will contain 60% of the parent       */
/*      psNode, with 20% representing overlap.  This can be help to     */
/*      prevent small objects on a boundary from shifting too high      */
/*      up the hQuadTree.                                               */
/* -------------------------------------------------------------------- */
constexpr double DEFAULT_SPLIT_RATIO = 0.55;

/*
** Returns TRUE if rectangle a is contained in rectangle b
*/
static CPL_INLINE bool CPL_RectContained( const CPLRectObj *a,
                                          const CPLRectObj *b )
{
    return
        a->minx >= b->minx && a->maxx <= b->maxx &&
        a->miny >= b->miny && a->maxy <= b->maxy;
}

/*
** Returns TRUE if rectangles a and b overlap
*/
static CPL_INLINE bool CPL_RectOverlap( const CPLRectObj *a,
                                        const CPLRectObj *b )
{
    if( a->minx > b->maxx ) return false;
    if( a->maxx < b->minx ) return false;
    if( a->miny > b->maxy ) return false;
    if( a->maxy < b->miny ) return false;
    return true;
}

/************************************************************************/
/*                      CPLQuadTreeNodeCreate()                         */
/************************************************************************/

static QuadTreeNode *CPLQuadTreeNodeCreate(const CPLRectObj* pRect)
{
    QuadTreeNode *psNode
        = static_cast<QuadTreeNode *>( CPLMalloc( sizeof(QuadTreeNode) ) );

    psNode->nFeatures = 0;
    psNode->pahFeatures = nullptr;
    psNode->pasBounds = nullptr;

    psNode->nNumSubNodes = 0;

    memcpy(&(psNode->rect), pRect, sizeof(CPLRectObj));

    return psNode;
}

/************************************************************************/
/*                         CPLQuadTreeCreate()                          */
/************************************************************************/

/**
 * Create a new quadtree
 *
 * @param pGlobalBounds a pointer to the global extent of all
 *                      the elements that will be inserted
 * @param pfnGetBounds  a user provided function to get the bounding box of
 *                      the inserted elements. If it is set to NULL, then
 *                      CPLQuadTreeInsertWithBounds() must be used, and
 *                      extra memory will be used to keep features bounds in the
 *                      quad tree.
 *
 * @return a newly allocated quadtree
 */

CPLQuadTree *CPLQuadTreeCreate( const CPLRectObj* pGlobalBounds,
                                CPLQuadTreeGetBoundsFunc pfnGetBounds )
{
    CPLAssert(pGlobalBounds);

    /* -------------------------------------------------------------------- */
    /*      Allocate the hQuadTree object                                   */
    /* -------------------------------------------------------------------- */
    CPLQuadTree *hQuadTree
        = static_cast<CPLQuadTree *>( CPLMalloc(sizeof(CPLQuadTree)) );

    hQuadTree->nFeatures = 0;
    hQuadTree->pfnGetBounds = pfnGetBounds;
    hQuadTree->nMaxDepth = 0;
    hQuadTree->nBucketCapacity = 8;

    hQuadTree->dfSplitRatio = DEFAULT_SPLIT_RATIO;

    /* -------------------------------------------------------------------- */
    /*      Allocate the psRoot psNode.                                     */
    /* -------------------------------------------------------------------- */
    hQuadTree->psRoot = CPLQuadTreeNodeCreate(pGlobalBounds);

    return hQuadTree;
}

/************************************************************************/
/*                 CPLQuadTreeGetAdvisedMaxDepth()                      */
/************************************************************************/

/**
 * Returns the optimal depth of a quadtree to hold nExpectedFeatures
 *
 * @param nExpectedFeatures the expected maximum number of elements to be
 * inserted.
 *
 * @return the optimal depth of a quadtree to hold nExpectedFeatures
 */

int CPLQuadTreeGetAdvisedMaxDepth(int nExpectedFeatures)
{
/* -------------------------------------------------------------------- */
/*      Try to select a reasonable one                                  */
/*      that implies approximately 8 shapes per node.                   */
/* -------------------------------------------------------------------- */
    int nMaxDepth = 0;
    int nMaxNodeCount = 1;

    while( nMaxNodeCount < nExpectedFeatures / 4 )
    {
        nMaxDepth += 1;
        nMaxNodeCount = nMaxNodeCount * 2;
    }

    CPLDebug( "CPLQuadTree",
              "Estimated spatial index tree depth: %d",
              nMaxDepth );

    /* NOTE: Due to problems with memory allocation for deep trees,
        * automatically estimated depth is limited up to 12 levels.
        * See Ticket #1594 for detailed discussion.
        */
    if( nMaxDepth > MAX_DEFAULT_TREE_DEPTH )
    {
        nMaxDepth = MAX_DEFAULT_TREE_DEPTH;

        CPLDebug( "CPLQuadTree",
                  "Falling back to max number of allowed index tree "
                  "levels (%d).",
                  MAX_DEFAULT_TREE_DEPTH );
    }

    return nMaxDepth;
}

/************************************************************************/
/*                     CPLQuadTreeSetMaxDepth()                         */
/************************************************************************/

/**
 * Set the maximum depth of a quadtree. By default, quad trees have
 * no maximum depth, but a maximum bucket capacity.
 *
 * @param hQuadTree the quad tree
 * @param nMaxDepth the maximum depth allowed
 */

void CPLQuadTreeSetMaxDepth(CPLQuadTree *hQuadTree, int nMaxDepth)
{
    hQuadTree->nMaxDepth = nMaxDepth;
}

/************************************************************************/
/*                   CPLQuadTreeSetBucketCapacity()                     */
/************************************************************************/

/**
 * Set the maximum capacity of a node of a quadtree. The default value is 8.
 * Note that the maximum capacity will only be honoured if the features
 * inserted have a point geometry. Otherwise it may be exceeded.
 *
 * @param hQuadTree the quad tree
 * @param nBucketCapacity the maximum capacity of a node of a quadtree
 */

void CPLQuadTreeSetBucketCapacity(CPLQuadTree *hQuadTree, int nBucketCapacity)
{
    if( nBucketCapacity > 0 )
        hQuadTree->nBucketCapacity = nBucketCapacity;
}

/************************************************************************/
/*                        CPLQuadTreeInsert()                           */
/************************************************************************/

/**
 * Insert a feature into a quadtree
 *
 * @param hQuadTree the quad tree
 * @param hFeature the feature to insert
 */

void CPLQuadTreeInsert(CPLQuadTree * hQuadTree, void* hFeature)
{
    if( hQuadTree->pfnGetBounds == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "hQuadTree->pfnGetBounds == NULL");
        return;
    }
    hQuadTree->nFeatures++;
    CPLRectObj bounds;
    hQuadTree->pfnGetBounds(hFeature, &bounds);
    CPLQuadTreeAddFeatureInternal(hQuadTree, hFeature, &bounds);
}

/************************************************************************/
/*                        CPLQuadTreeInsertWithBounds()                 */
/************************************************************************/

/**
 * Insert a feature into a quadtree
 *
 * @param hQuadTree the quad tree
 * @param hFeature the feature to insert
 * @param psBounds bounds of the feature
 */
void CPLQuadTreeInsertWithBounds(CPLQuadTree *hQuadTree,
                                 void* hFeature,
                                 const CPLRectObj* psBounds)
{
    hQuadTree->nFeatures++;
    CPLQuadTreeAddFeatureInternal(hQuadTree, hFeature, psBounds);
}

/************************************************************************/
/*                            CPLQuadTreeRemove()                       */
/************************************************************************/

static bool CPLQuadTreeRemoveInternal(QuadTreeNode* psNode,
                                      void* hFeature,
                                      const CPLRectObj* psBounds)
{
    bool bRemoved = false;

    for( int i = 0; i < psNode->nFeatures; i++ )
    {
        if( psNode->pahFeatures[i] == hFeature )
        {
            if( i < psNode->nFeatures - 1 )
            {
                memmove(psNode->pahFeatures + i,
                        psNode->pahFeatures + i + 1,
                        (psNode->nFeatures - 1 - i) * sizeof(void*));
                if( psNode->pasBounds )
                {
                    memmove(psNode->pasBounds + i,
                            psNode->pasBounds + i + 1,
                            (psNode->nFeatures - 1 - i) * sizeof(CPLRectObj));
                }
            }
            bRemoved = true;
            psNode->nFeatures --;
            break;
        }
    }
    if( psNode->nFeatures == 0 && psNode->pahFeatures != nullptr )
    {
        CPLFree(psNode->pahFeatures);
        CPLFree(psNode->pasBounds);
        psNode->pahFeatures = nullptr;
        psNode->pasBounds = nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Recurse to subnodes if they exist.                              */
    /* -------------------------------------------------------------------- */
    for( int i = 0; i < psNode->nNumSubNodes; i++ )
    {
        if( psNode->apSubNode[i] &&
            CPL_RectOverlap(&(psNode->apSubNode[i]->rect), psBounds) )
        {
            bRemoved |=
                CPLQuadTreeRemoveInternal( psNode->apSubNode[i], hFeature, psBounds );

            if( psNode->apSubNode[i]->nFeatures == 0 &&
                psNode->apSubNode[i]->nNumSubNodes == 0 )
            {
                CPLQuadTreeNodeDestroy(psNode->apSubNode[i]);
                if( i < psNode->nNumSubNodes - 1 )
                {
                    memmove(psNode->apSubNode + i,
                            psNode->apSubNode + i + 1,
                            (psNode->nNumSubNodes - 1 - i) * sizeof(QuadTreeNode*));
                }
                i --;
                psNode->nNumSubNodes --;
            }
        }
    }

    return bRemoved;
}

/**
 * Remove a feature from a quadtree.
 *
 * Currently the quadtree is not re-balanced.
 *
 * @param hQuadTree the quad tree
 * @param hFeature the feature to remove
 * @param psBounds bounds of the feature (or NULL if pfnGetBounds has been filled)
 */
void CPLQuadTreeRemove(CPLQuadTree *hQuadTree, void* hFeature,
                       const CPLRectObj* psBounds)
{
    if( psBounds == nullptr && hQuadTree->pfnGetBounds == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "hQuadTree->pfnGetBounds == NULL");
        return;
    }
    CPLRectObj bounds; // keep variable in this outer scope
    if( psBounds == nullptr )
    {
        hQuadTree->pfnGetBounds(hFeature, &bounds);
        psBounds = &bounds;
    }
    if( CPLQuadTreeRemoveInternal(hQuadTree->psRoot, hFeature, psBounds) )
    {
        hQuadTree->nFeatures--;
    }
}

/************************************************************************/
/*                    CPLQuadTreeNodeDestroy()                          */
/************************************************************************/

static void CPLQuadTreeNodeDestroy(QuadTreeNode *psNode)
{
    for( int i = 0; i < psNode->nNumSubNodes; i++ )
    {
        if( psNode->apSubNode[i] )
            CPLQuadTreeNodeDestroy(psNode->apSubNode[i]);
    }

    if( psNode->pahFeatures )
    {
        CPLFree(psNode->pahFeatures);
        CPLFree(psNode->pasBounds);
    }

    CPLFree(psNode);
}

/************************************************************************/
/*                       CPLQuadTreeDestroy()                           */
/************************************************************************/

/**
 * Destroy a quadtree
 *
 * @param hQuadTree the quad tree to destroy
 */

void CPLQuadTreeDestroy(CPLQuadTree *hQuadTree)
{
    CPLAssert(hQuadTree);
    CPLQuadTreeNodeDestroy(hQuadTree->psRoot);
    CPLFree(hQuadTree);
}

/************************************************************************/
/*                     CPLQuadTreeSplitBounds()                         */
/************************************************************************/

static void CPLQuadTreeSplitBounds( double dfSplitRatio,
                                      const CPLRectObj *in,
                                      CPLRectObj *out1,
                                      CPLRectObj *out2)
{
    /* -------------------------------------------------------------------- */
    /*      The output bounds will be very similar to the input bounds,     */
    /*      so just copy over to start.                                     */
    /* -------------------------------------------------------------------- */
    memcpy(out1, in, sizeof(CPLRectObj));
    memcpy(out2, in, sizeof(CPLRectObj));

    /* -------------------------------------------------------------------- */
    /*      Split in X direction.                                           */
    /* -------------------------------------------------------------------- */
    if( (in->maxx - in->minx) > (in->maxy - in->miny) )
    {
        const double range = in->maxx - in->minx;

        out1->maxx = in->minx + range * dfSplitRatio;
        out2->minx = in->maxx - range * dfSplitRatio;
    }

    /* -------------------------------------------------------------------- */
    /*      Otherwise split in Y direction.                                 */
    /* -------------------------------------------------------------------- */
    else
    {
        const double range = in->maxy - in->miny;

        out1->maxy = in->miny + range * dfSplitRatio;
        out2->miny = in->maxy - range * dfSplitRatio;
    }
}

/************************************************************************/
/*                  CPLQuadTreeNodeAddFeatureAlg1()                     */
/************************************************************************/

static void CPLQuadTreeNodeAddFeatureAlg1( CPLQuadTree* hQuadTree,
                                           QuadTreeNode *psNode,
                                           void* hFeature,
                                           const CPLRectObj* pRect)
{
    if( psNode->nNumSubNodes == 0 )
    {
        // If we have reached the max bucket capacity, try to insert
        // in a subnode if possible.
        if( psNode->nFeatures >= hQuadTree->nBucketCapacity )
        {
            CPLRectObj half1 = { 0.0, 0.0, 0.0, 0.0 };
            CPLRectObj half2 = { 0.0, 0.0, 0.0, 0.0 };
            CPLRectObj quad1 = { 0.0, 0.0, 0.0, 0.0 };
            CPLRectObj quad2 = { 0.0, 0.0, 0.0, 0.0 };
            CPLRectObj quad3 = { 0.0, 0.0, 0.0, 0.0 };
            CPLRectObj quad4 = { 0.0, 0.0, 0.0, 0.0 };

            CPLQuadTreeSplitBounds( hQuadTree->dfSplitRatio, &psNode->rect,
                                    &half1, &half2);
            CPLQuadTreeSplitBounds( hQuadTree->dfSplitRatio, &half1,
                                    &quad1, &quad2);
            CPLQuadTreeSplitBounds( hQuadTree->dfSplitRatio, &half2,
                                    &quad3, &quad4);

            if( memcmp(&psNode->rect, &quad1, sizeof(CPLRectObj)) != 0 &&
                memcmp(&psNode->rect, &quad2, sizeof(CPLRectObj)) != 0 &&
                memcmp(&psNode->rect, &quad3, sizeof(CPLRectObj)) != 0 &&
                memcmp(&psNode->rect, &quad4, sizeof(CPLRectObj)) != 0 &&
                (CPL_RectContained(pRect, &quad1) ||
                CPL_RectContained(pRect, &quad2) ||
                CPL_RectContained(pRect, &quad3) ||
                CPL_RectContained(pRect, &quad4)) )
            {
                psNode->nNumSubNodes = 4;
                psNode->apSubNode[0] = CPLQuadTreeNodeCreate(&quad1);
                psNode->apSubNode[1] = CPLQuadTreeNodeCreate(&quad2);
                psNode->apSubNode[2] = CPLQuadTreeNodeCreate(&quad3);
                psNode->apSubNode[3] = CPLQuadTreeNodeCreate(&quad4);

                const int oldNumFeatures = psNode->nFeatures;
                void** oldFeatures = psNode->pahFeatures;
                CPLRectObj* pasOldBounds = psNode->pasBounds;
                psNode->nFeatures = 0;
                psNode->pahFeatures = nullptr;
                psNode->pasBounds = nullptr;

                // Redispatch existing pahFeatures in apSubNodes.
                for( int i = 0; i < oldNumFeatures; i++ )
                {
                    if( hQuadTree->pfnGetBounds == nullptr )
                        CPLQuadTreeNodeAddFeatureAlg1( hQuadTree, psNode,
                                                       oldFeatures[i],
                                                       &pasOldBounds[i]);
                    else
                    {
                        CPLRectObj bounds;
                        hQuadTree->pfnGetBounds(oldFeatures[i], &bounds);
                        CPLQuadTreeNodeAddFeatureAlg1( hQuadTree, psNode,
                                                       oldFeatures[i],
                                                       &bounds );
                    }
                }

                CPLFree(oldFeatures);
                CPLFree(pasOldBounds);

                /* recurse back on this psNode now that it has apSubNodes */
                CPLQuadTreeNodeAddFeatureAlg1( hQuadTree, psNode, hFeature,
                                               pRect );
                return;
            }
        }
    }
    else
    {
    /* -------------------------------------------------------------------- */
    /*      If there are apSubNodes, then consider whether this object      */
    /*      will fit in them.                                               */
    /* -------------------------------------------------------------------- */
        for( int i = 0; i < psNode->nNumSubNodes; i++ )
        {
            if( CPL_RectContained(pRect, &psNode->apSubNode[i]->rect))
            {
                CPLQuadTreeNodeAddFeatureAlg1( hQuadTree, psNode->apSubNode[i],
                                               hFeature, pRect );
                return;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If none of that worked, just add it to this psNodes list.         */
/* -------------------------------------------------------------------- */
    psNode->nFeatures++;

    if( psNode->nFeatures == 1 )
    {
        CPLAssert( psNode->pahFeatures == nullptr );
        psNode->pahFeatures = static_cast<void **>(
            CPLMalloc( hQuadTree->nBucketCapacity * sizeof(void*) ) );
        if( hQuadTree->pfnGetBounds == nullptr )
            psNode->pasBounds = static_cast<CPLRectObj *>(
                CPLMalloc( hQuadTree->nBucketCapacity * sizeof(CPLRectObj) ) );
    }
    else if( psNode->nFeatures > hQuadTree->nBucketCapacity )
    {
        psNode->pahFeatures = static_cast<void **>(
            CPLRealloc( psNode->pahFeatures,
                        sizeof(void*) * psNode->nFeatures ) );
        if( hQuadTree->pfnGetBounds == nullptr )
            psNode->pasBounds = static_cast<CPLRectObj *>(
                CPLRealloc( psNode->pasBounds,
                            sizeof(CPLRectObj) * psNode->nFeatures ) );
    }
    psNode->pahFeatures[psNode->nFeatures-1] = hFeature;
    if( hQuadTree->pfnGetBounds == nullptr )
        psNode->pasBounds[psNode->nFeatures-1] = *pRect;

    return;
}

/************************************************************************/
/*                  CPLQuadTreeNodeAddFeatureAlg2()                     */
/************************************************************************/

static void CPLQuadTreeNodeAddFeatureAlg2( CPLQuadTree *hQuadTree,
                                           QuadTreeNode *psNode,
                                           void* hFeature,
                                           const CPLRectObj* pRect,
                                           int nMaxDepth)
{
  /* -------------------------------------------------------------------- */
  /*      If there are apSubNodes, then consider whether this object      */
  /*      will fit in them.                                               */
  /* -------------------------------------------------------------------- */
    if( nMaxDepth > 1 && psNode->nNumSubNodes > 0 )
    {
        for( int i = 0; i < psNode->nNumSubNodes; i++ )
        {
            if( CPL_RectContained(pRect, &psNode->apSubNode[i]->rect))
            {
                CPLQuadTreeNodeAddFeatureAlg2( hQuadTree, psNode->apSubNode[i],
                                               hFeature, pRect, nMaxDepth-1);
                return;
            }
        }
    }

  /* -------------------------------------------------------------------- */
  /*      Otherwise, consider creating four apSubNodes if could fit into  */
  /*      them, and adding to the appropriate apSubNode.                  */
  /* -------------------------------------------------------------------- */
    else if( nMaxDepth > 1 && psNode->nNumSubNodes == 0 )
    {
        CPLRectObj half1, half2, quad1, quad2, quad3, quad4;

        CPLQuadTreeSplitBounds(hQuadTree->dfSplitRatio, &psNode->rect,
                               &half1, &half2);
        CPLQuadTreeSplitBounds(hQuadTree->dfSplitRatio, &half1, &quad1, &quad2);
        CPLQuadTreeSplitBounds(hQuadTree->dfSplitRatio, &half2, &quad3, &quad4);

        if( memcmp(&psNode->rect, &quad1, sizeof(CPLRectObj)) != 0 &&
            memcmp(&psNode->rect, &quad2, sizeof(CPLRectObj)) != 0 &&
            memcmp(&psNode->rect, &quad3, sizeof(CPLRectObj)) != 0 &&
            memcmp(&psNode->rect, &quad4, sizeof(CPLRectObj)) != 0 &&
            (CPL_RectContained(pRect, &quad1) ||
             CPL_RectContained(pRect, &quad2) ||
             CPL_RectContained(pRect, &quad3) ||
             CPL_RectContained(pRect, &quad4)) )
        {
            psNode->nNumSubNodes = 4;
            psNode->apSubNode[0] = CPLQuadTreeNodeCreate(&quad1);
            psNode->apSubNode[1] = CPLQuadTreeNodeCreate(&quad2);
            psNode->apSubNode[2] = CPLQuadTreeNodeCreate(&quad3);
            psNode->apSubNode[3] = CPLQuadTreeNodeCreate(&quad4);

            /* recurse back on this psNode now that it has apSubNodes */
            CPLQuadTreeNodeAddFeatureAlg2( hQuadTree, psNode, hFeature,
                                           pRect, nMaxDepth);
            return;
        }
    }

/* -------------------------------------------------------------------- */
/*      If none of that worked, just add it to this psNodes list.       */
/* -------------------------------------------------------------------- */
    psNode->nFeatures++;

    psNode->pahFeatures =
        static_cast<void **>( CPLRealloc( psNode->pahFeatures,
                                          sizeof(void*) * psNode->nFeatures ) );
    if( hQuadTree->pfnGetBounds == nullptr )
    {
        psNode->pasBounds =
          static_cast<CPLRectObj*>(
              CPLRealloc( psNode->pasBounds,
                          sizeof(CPLRectObj) * psNode->nFeatures ) );
    }
    psNode->pahFeatures[psNode->nFeatures-1] = hFeature;
    if( hQuadTree->pfnGetBounds == nullptr )
    {
        psNode->pasBounds[psNode->nFeatures-1] = *pRect;
    }
}

/************************************************************************/
/*                  CPLQuadTreeAddFeatureInternal()                     */
/************************************************************************/

static void CPLQuadTreeAddFeatureInternal(CPLQuadTree *hQuadTree,
                                          void* hFeature,
                                          const CPLRectObj *pRect)
{
    if( hQuadTree->nMaxDepth == 0 )
    {
        CPLQuadTreeNodeAddFeatureAlg1(hQuadTree, hQuadTree->psRoot,
                                     hFeature, pRect);
    }
    else
    {
        CPLQuadTreeNodeAddFeatureAlg2(hQuadTree, hQuadTree->psRoot,
                                     hFeature, pRect, hQuadTree->nMaxDepth);
    }
}

/************************************************************************/
/*                     CPLQuadTreeCollectFeatures()                     */
/************************************************************************/

static void CPLQuadTreeCollectFeatures(const CPLQuadTree *hQuadTree,
                                       const QuadTreeNode *psNode,
                                       const CPLRectObj* pAoi,
                                       int* pnFeatureCount,
                                       int* pnMaxFeatures,
                                       void*** pppFeatureList)
{
    /* -------------------------------------------------------------------- */
    /*      Does this psNode overlap the area of interest at all?  If not,  */
    /*      return without adding to the list at all.                       */
    /* -------------------------------------------------------------------- */
    if( !CPL_RectOverlap(&psNode->rect, pAoi) )
        return;

    /* -------------------------------------------------------------------- */
    /*      Grow the list to hold the features on this psNode.              */
    /* -------------------------------------------------------------------- */
    if( *pnFeatureCount + psNode->nFeatures > *pnMaxFeatures )
    {
        // TODO(schwehr): Symbolic constant.
        *pnMaxFeatures = (*pnFeatureCount + psNode->nFeatures) * 2 + 20;
        *pppFeatureList = static_cast<void **>(
            CPLRealloc(*pppFeatureList, sizeof(void*) * *pnMaxFeatures) );
    }

    /* -------------------------------------------------------------------- */
    /*      Add the local features to the list.                             */
    /* -------------------------------------------------------------------- */
    for( int i = 0; i < psNode->nFeatures; i++ )
    {
        if( hQuadTree->pfnGetBounds == nullptr )
        {
            if( CPL_RectOverlap(&psNode->pasBounds[i], pAoi) )
                (*pppFeatureList)[(*pnFeatureCount)++] = psNode->pahFeatures[i];
        }
        else
        {
            CPLRectObj bounds;
            hQuadTree->pfnGetBounds(psNode->pahFeatures[i], &bounds);
            if( CPL_RectOverlap(&bounds, pAoi) )
                (*pppFeatureList)[(*pnFeatureCount)++] = psNode->pahFeatures[i];
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Recurse to subnodes if they exist.                              */
    /* -------------------------------------------------------------------- */
    for( int i = 0; i < psNode->nNumSubNodes; i++ )
    {
        if( psNode->apSubNode[i] )
            CPLQuadTreeCollectFeatures( hQuadTree, psNode->apSubNode[i], pAoi,
                                        pnFeatureCount, pnMaxFeatures,
                                        pppFeatureList );
    }
}

/************************************************************************/
/*                         CPLQuadTreeSearch()                          */
/************************************************************************/

/**
 * Returns all the elements inserted whose bounding box intersects the
 * provided area of interest
 *
 * @param hQuadTree the quad tree
 * @param pAoi the pointer to the area of interest
 * @param pnFeatureCount the user data provided to the function.
 *
 * @return an array of features that must be freed with CPLFree
 */

void** CPLQuadTreeSearch( const CPLQuadTree *hQuadTree,
                          const CPLRectObj* pAoi,
                          int* pnFeatureCount )
{
    CPLAssert(hQuadTree);
    CPLAssert(pAoi);

    int nFeatureCount = 0;
    if( pnFeatureCount == nullptr )
        pnFeatureCount = &nFeatureCount;

    *pnFeatureCount = 0;

    int nMaxFeatures = 0;
    void** ppFeatureList = nullptr;
    CPLQuadTreeCollectFeatures(hQuadTree, hQuadTree->psRoot, pAoi,
                               pnFeatureCount, &nMaxFeatures, &ppFeatureList);

    return ppFeatureList;
}

/************************************************************************/
/*                    CPLQuadTreeNodeForeach()                          */
/************************************************************************/

static bool CPLQuadTreeNodeForeach(const QuadTreeNode *psNode,
                                  CPLQuadTreeForeachFunc pfnForeach,
                                  void* pUserData)
{
    for( int i = 0; i < psNode->nNumSubNodes; i++ )
    {
        if( !CPLQuadTreeNodeForeach(psNode->apSubNode[i], pfnForeach,
                                    pUserData) )
            return false;
    }

    for( int i = 0; i < psNode->nFeatures; i++ )
    {
        if( pfnForeach(psNode->pahFeatures[i], pUserData) == FALSE )
            return false;
    }

    return true;
}

/************************************************************************/
/*                       CPLQuadTreeForeach()                           */
/************************************************************************/

/**
 * Walk through the quadtree and runs the provided function on all the
 * elements
 *
 * This function is provided with the user_data argument of pfnForeach.
 * It must return TRUE to go on the walk through the hash set, or FALSE to
 * make it stop.
 *
 * Note : the structure of the quadtree must *NOT* be modified during the
 * walk.
 *
 * @param hQuadTree the quad tree
 * @param pfnForeach the function called on each element.
 * @param pUserData the user data provided to the function.
 */

void CPLQuadTreeForeach( const CPLQuadTree *hQuadTree,
                         CPLQuadTreeForeachFunc pfnForeach,
                         void* pUserData )
{
    CPLAssert(hQuadTree);
    CPLAssert(pfnForeach);
    CPLQuadTreeNodeForeach(hQuadTree->psRoot, pfnForeach, pUserData);
}

/************************************************************************/
/*                       CPLQuadTreeDumpNode()                          */
/************************************************************************/

static void CPLQuadTreeDumpNode( const QuadTreeNode *psNode,
                                 int nIndentLevel,
                                 CPLQuadTreeDumpFeatureFunc pfnDumpFeatureFunc,
                                 void* pUserData )
{
    if( psNode->nNumSubNodes )
    {
        for( int count = nIndentLevel; --count >= 0; )
        {
            printf("  "); /*ok*/
        }
        printf("SubhQuadTrees :\n"); /*ok*/
        for( int i = 0; i < psNode->nNumSubNodes; i++ )
        {
            for( int count = nIndentLevel+1; --count >= 0; )
            {
                printf("  "); /*ok*/
            }
            printf("SubhQuadTree %d :\n", i+1); /*ok*/
            CPLQuadTreeDumpNode(psNode->apSubNode[i], nIndentLevel + 2,
                                pfnDumpFeatureFunc, pUserData);
        }
    }
    if( psNode->nFeatures )
    {
        for( int count = nIndentLevel; --count >= 0; )
            printf("  "); /*ok*/
        printf("Leaves (%d):\n", psNode->nFeatures); /*ok*/
        for( int i = 0; i < psNode->nFeatures; i++ )
        {
            if( pfnDumpFeatureFunc )
            {
                pfnDumpFeatureFunc(psNode->pahFeatures[i], nIndentLevel + 2,
                                   pUserData);
            }
            else
            {
                for( int count = nIndentLevel + 1; --count >= 0; )
                {
                    printf("  "); /*ok*/
                }
                printf("%p\n", psNode->pahFeatures[i]); /*ok*/
            }
        }
    }
}

/************************************************************************/
/*                         CPLQuadTreeDump()                            */
/************************************************************************/

/** Dump quad tree */
void CPLQuadTreeDump( const CPLQuadTree *hQuadTree,
                      CPLQuadTreeDumpFeatureFunc pfnDumpFeatureFunc,
                      void* pUserData )
{
    CPLQuadTreeDumpNode(hQuadTree->psRoot, 0, pfnDumpFeatureFunc, pUserData);
}

/************************************************************************/
/*                  CPLQuadTreeGetStatsNode()                           */
/************************************************************************/

static
void CPLQuadTreeGetStatsNode( const QuadTreeNode *psNode,
                              int nDepthLevel,
                              int* pnNodeCount,
                              int* pnMaxDepth,
                              int* pnMaxBucketCapacity )
{
    (*pnNodeCount) ++;
    if( nDepthLevel > *pnMaxDepth )
        *pnMaxDepth = nDepthLevel;
    if( psNode->nFeatures > *pnMaxBucketCapacity )
        *pnMaxBucketCapacity = psNode->nFeatures;

    for( int i = 0; i < psNode->nNumSubNodes; i++ )
    {
        CPLQuadTreeGetStatsNode(psNode->apSubNode[i], nDepthLevel + 1,
                                pnNodeCount, pnMaxDepth, pnMaxBucketCapacity);
    }
}

/************************************************************************/
/*                    CPLQuadTreeGetStats()                             */
/************************************************************************/

/** Get stats */
void CPLQuadTreeGetStats( const CPLQuadTree *hQuadTree,
                          int* pnFeatureCount,
                          int* pnNodeCount,
                          int* pnMaxDepth,
                          int* pnMaxBucketCapacity )
{
    CPLAssert(hQuadTree);

    int nFeatureCount = 0;
    if( pnFeatureCount == nullptr )
        pnFeatureCount = &nFeatureCount;
    int nNodeCount = 0;
    if( pnNodeCount == nullptr )
        pnNodeCount = &nNodeCount;
    int nMaxDepth = 0;
    if( pnMaxDepth == nullptr )
        pnMaxDepth = &nMaxDepth;
    int nMaxBucketCapacity = 0;
    if( pnMaxBucketCapacity == nullptr )
        pnMaxBucketCapacity = &nMaxBucketCapacity;

    *pnFeatureCount = hQuadTree->nFeatures;
    *pnNodeCount = 0;
    *pnMaxDepth = 1;
    *pnMaxBucketCapacity = 0;

    CPLQuadTreeGetStatsNode( hQuadTree->psRoot, 0, pnNodeCount, pnMaxDepth,
                             pnMaxBucketCapacity );

    // TODO(schwehr): If any of the pointers were set to local vars,
    // do they need to be reset to a nullptr?
}
