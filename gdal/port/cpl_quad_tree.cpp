/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implementation of quadtree building and searching functions.
 *           Derived from shapelib and mapserver implementations
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *           Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 1999-2008, Frank Warmerdam
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
 *
 */

#include "cpl_conv.h"
#include "cpl_quad_tree.h"

CPL_CVSID("$Id$");

#define MAX_DEFAULT_TREE_DEPTH 12
#define MAX_SUBNODES 4

typedef struct _QuadTreeNode QuadTreeNode;

struct _QuadTreeNode
{
  /* area covered by this psNode */
  CPLRectObj    rect;

  /* list of shapes stored at this psNode. */
  int           nFeatures;
  void        **pahFeatures;

  int           nNumSubNodes;
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

/* -------------------------------------------------------------------- */
/*      If the following is 0.5, psNodes will be split in half.  If it    */
/*      is 0.6 then each apSubNode will contain 60% of the parent         */
/*      psNode, with 20% representing overlap.  This can be help to       */
/*      prevent small objects on a boundary from shifting too high      */
/*      up the hQuadTree.                                                    */
/* -------------------------------------------------------------------- */
#define DEFAULT_SPLIT_RATIO  0.55

/*
** Returns TRUE if rectangle a is contained in rectangle b
*/
static CPL_INLINE int CPL_RectContained(const CPLRectObj *a, const CPLRectObj *b)
{
  if(a->minx >= b->minx && a->maxx <= b->maxx)
    if(a->miny >= b->miny && a->maxy <= b->maxy)
      return(TRUE);
  return(FALSE);  
}

/*
** Returns TRUE if rectangles a and b overlap
*/
static CPL_INLINE int CPL_RectOverlap(const CPLRectObj *a, const CPLRectObj *b)
{
  if(a->minx > b->maxx) return(FALSE);
  if(a->maxx < b->minx) return(FALSE);
  if(a->miny > b->maxy) return(FALSE);
  if(a->maxy < b->miny) return(FALSE);
  return(TRUE);
}

/************************************************************************/
/*                      CPLQuadTreeNodeCreate()                         */
/************************************************************************/

static QuadTreeNode *CPLQuadTreeNodeCreate(const CPLRectObj* pRect)
{
    QuadTreeNode	*psNode;

    psNode = (QuadTreeNode *) CPLMalloc(sizeof(QuadTreeNode));

    psNode->nFeatures = 0;
    psNode->pahFeatures = NULL;

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
 *                      the inserted elements
 *
 * @return a newly allocated quadtree
 */

CPLQuadTree *CPLQuadTreeCreate(const CPLRectObj* pGlobalBounds, CPLQuadTreeGetBoundsFunc pfnGetBounds)
{
    CPLQuadTree *hQuadTree;

    CPLAssert(pGlobalBounds);

    /* -------------------------------------------------------------------- */
    /*      Allocate the hQuadTree object                                        */
    /* -------------------------------------------------------------------- */
    hQuadTree = (CPLQuadTree *) CPLMalloc(sizeof(CPLQuadTree));

    hQuadTree->nFeatures = 0;
    hQuadTree->pfnGetBounds = pfnGetBounds;
    hQuadTree->nMaxDepth = 0;
    hQuadTree->nBucketCapacity = 8;

    hQuadTree->dfSplitRatio = DEFAULT_SPLIT_RATIO;

    /* -------------------------------------------------------------------- */
    /*      Allocate the psRoot psNode.                                         */
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
 * @param nExpectedFeatures the expected maximum number of elements to be inserted
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

    while( nMaxNodeCount*4 < nExpectedFeatures )
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
                    "Falling back to max number of allowed index tree levels (%d).",
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
 * @param nBucketCapacity the maximum capactiy of a node of a quadtree
 */

void CPLQuadTreeSetBucketCapacity(CPLQuadTree *hQuadTree, int nBucketCapacity)
{
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
    CPLRectObj bounds;
    hQuadTree->nFeatures ++;
    hQuadTree->pfnGetBounds(hFeature, &bounds);
    CPLQuadTreeAddFeatureInternal(hQuadTree, hFeature, &bounds);
}

/************************************************************************/
/*                    CPLQuadTreeNodeDestroy()                          */
/************************************************************************/

static void CPLQuadTreeNodeDestroy(QuadTreeNode *psNode)
{
    int i;

    for(i=0; i<psNode->nNumSubNodes; i++ )
    {
        if(psNode->apSubNode[i]) 
            CPLQuadTreeNodeDestroy(psNode->apSubNode[i]);
    }

    if(psNode->pahFeatures)
        CPLFree(psNode->pahFeatures);

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
  double range;

  /* -------------------------------------------------------------------- */
  /*      The output bounds will be very similar to the input bounds,     */
  /*      so just copy over to start.                                     */
  /* -------------------------------------------------------------------- */
  memcpy(out1, in, sizeof(CPLRectObj));
  memcpy(out2, in, sizeof(CPLRectObj));
  
  /* -------------------------------------------------------------------- */
  /*      Split in X direction.                                           */
  /* -------------------------------------------------------------------- */
  if((in->maxx - in->minx) > (in->maxy - in->miny)) {
    range = in->maxx - in->minx;
    
    out1->maxx = in->minx + range * dfSplitRatio;
    out2->minx = in->maxx - range * dfSplitRatio;
  }

  /* -------------------------------------------------------------------- */
  /*      Otherwise split in Y direction.                                 */
  /* -------------------------------------------------------------------- */
  else {
    range = in->maxy - in->miny;
    
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
    int i;
    if (psNode->nNumSubNodes == 0)
    {
        /* If we have reached the max bucket capacity, try to insert */
        /* in a subnode if possible */
        if (psNode->nFeatures >= hQuadTree->nBucketCapacity)
        {
            CPLRectObj half1, half2, quad1, quad2, quad3, quad4;

            CPLQuadTreeSplitBounds(hQuadTree->dfSplitRatio, &psNode->rect, &half1, &half2);
            CPLQuadTreeSplitBounds(hQuadTree->dfSplitRatio, &half1, &quad1, &quad2);
            CPLQuadTreeSplitBounds(hQuadTree->dfSplitRatio, &half2, &quad3, &quad4);

            if (CPL_RectContained(pRect, &quad1) ||
                CPL_RectContained(pRect, &quad2) ||
                CPL_RectContained(pRect, &quad3) ||
                CPL_RectContained(pRect, &quad4))
            {
                psNode->nNumSubNodes = 4;
                psNode->apSubNode[0] = CPLQuadTreeNodeCreate(&quad1);
                psNode->apSubNode[1] = CPLQuadTreeNodeCreate(&quad2);
                psNode->apSubNode[2] = CPLQuadTreeNodeCreate(&quad3);
                psNode->apSubNode[3] = CPLQuadTreeNodeCreate(&quad4);

                int oldNumFeatures = psNode->nFeatures;
                void** oldFeatures = psNode->pahFeatures;
                psNode->nFeatures = 0;
                psNode->pahFeatures = NULL;

                /* redispatch existing pahFeatures in apSubNodes */
                int i;
                for(i=0;i<oldNumFeatures;i++)
                {
                    CPLRectObj hFeatureBound;
                    hQuadTree->pfnGetBounds(oldFeatures[i], &hFeatureBound);
                    CPLQuadTreeNodeAddFeatureAlg1(hQuadTree, psNode, oldFeatures[i], &hFeatureBound);
                }

                CPLFree(oldFeatures);

                /* recurse back on this psNode now that it has apSubNodes */
                CPLQuadTreeNodeAddFeatureAlg1(hQuadTree, psNode, hFeature, pRect);
                return;
            }
        }
    }
    else
    {
    /* -------------------------------------------------------------------- */
    /*      If there are apSubNodes, then consider whether this object        */
    /*      will fit in them.                                               */
    /* -------------------------------------------------------------------- */
        for(i=0; i<psNode->nNumSubNodes; i++ )
        {
            if( CPL_RectContained(pRect, &psNode->apSubNode[i]->rect))
            {
                CPLQuadTreeNodeAddFeatureAlg1( hQuadTree, psNode->apSubNode[i], hFeature, pRect);
                return;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If none of that worked, just add it to this psNodes list.         */
/* -------------------------------------------------------------------- */
    psNode->nFeatures++;

    psNode->pahFeatures = (void**) CPLRealloc( psNode->pahFeatures, sizeof(void*) * psNode->nFeatures );
    psNode->pahFeatures[psNode->nFeatures-1] = hFeature;

    return ;
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
    int i;

  /* -------------------------------------------------------------------- */
  /*      If there are apSubNodes, then consider whether this object        */
  /*      will fit in them.                                               */
  /* -------------------------------------------------------------------- */
    if( nMaxDepth > 1 && psNode->nNumSubNodes > 0 )
    {
        for(i=0; i<psNode->nNumSubNodes; i++ )
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
  /*      Otherwise, consider creating four apSubNodes if could fit into    */
  /*      them, and adding to the appropriate apSubNode.                    */
  /* -------------------------------------------------------------------- */
    else if( nMaxDepth > 1 && psNode->nNumSubNodes == 0 )
    {
        CPLRectObj half1, half2, quad1, quad2, quad3, quad4;

        CPLQuadTreeSplitBounds(hQuadTree->dfSplitRatio, &psNode->rect, &half1, &half2);
        CPLQuadTreeSplitBounds(hQuadTree->dfSplitRatio, &half1, &quad1, &quad2);
        CPLQuadTreeSplitBounds(hQuadTree->dfSplitRatio, &half2, &quad3, &quad4);

        if(CPL_RectContained(pRect, &quad1) ||
        CPL_RectContained(pRect, &quad2) ||
        CPL_RectContained(pRect, &quad3) ||
        CPL_RectContained(pRect, &quad4))
        {
            psNode->nNumSubNodes = 4;
            psNode->apSubNode[0] = CPLQuadTreeNodeCreate(&quad1);
            psNode->apSubNode[1] = CPLQuadTreeNodeCreate(&quad2);
            psNode->apSubNode[2] = CPLQuadTreeNodeCreate(&quad3);
            psNode->apSubNode[3] = CPLQuadTreeNodeCreate(&quad4);

            /* recurse back on this psNode now that it has apSubNodes */
            CPLQuadTreeNodeAddFeatureAlg2(hQuadTree, psNode, hFeature, pRect, nMaxDepth);
            return;
        }
    }

/* -------------------------------------------------------------------- */
/*      If none of that worked, just add it to this psNodes list.         */
/* -------------------------------------------------------------------- */
    psNode->nFeatures++;

    psNode->pahFeatures =
            (void**) CPLRealloc( psNode->pahFeatures,
                                 sizeof(void*) * psNode->nFeatures );
    psNode->pahFeatures[psNode->nFeatures-1] = hFeature;
}


/************************************************************************/
/*                  CPLQuadTreeAddFeatureInternal()                     */
/************************************************************************/

static void CPLQuadTreeAddFeatureInternal(CPLQuadTree *hQuadTree,
                                          void* hFeature,
                                          const CPLRectObj *pRect)
{
    if (hQuadTree->nMaxDepth == 0)
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
  int i;

  /* -------------------------------------------------------------------- */
  /*      Does this psNode overlap the area of interest at all?  If not,    */
  /*      return without adding to the list at all.                       */
  /* -------------------------------------------------------------------- */
  if(!CPL_RectOverlap(&psNode->rect, pAoi))
     return;

/* -------------------------------------------------------------------- */
/*      Grow the list to hold the features on this psNode.              */
/* -------------------------------------------------------------------- */
    if( *pnFeatureCount + psNode->nFeatures > *pnMaxFeatures )
    {
        *pnMaxFeatures = (*pnFeatureCount + psNode->nFeatures) * 2 + 20;
        *pppFeatureList = (void**)
            CPLRealloc(*pppFeatureList,sizeof(void*) * *pnMaxFeatures);
    }

  /* -------------------------------------------------------------------- */
  /*      Add the local features to the list.                             */
  /* -------------------------------------------------------------------- */
  for(i=0; i<psNode->nFeatures; i++)
  {
      CPLRectObj bounds;
      hQuadTree->pfnGetBounds(psNode->pahFeatures[i], &bounds);
      if (CPL_RectOverlap(&bounds, pAoi))
            (*pppFeatureList)[(*pnFeatureCount)++] = psNode->pahFeatures[i];
  }
  
  /* -------------------------------------------------------------------- */
  /*      Recurse to subnodes if they exist.                              */
  /* -------------------------------------------------------------------- */
  for(i=0; i<psNode->nNumSubNodes; i++)
  {
      if(psNode->apSubNode[i])
        CPLQuadTreeCollectFeatures(hQuadTree, psNode->apSubNode[i], pAoi,
                                   pnFeatureCount, pnMaxFeatures, pppFeatureList);
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

void** CPLQuadTreeSearch(const CPLQuadTree *hQuadTree,
                         const CPLRectObj* pAoi,
                         int* pnFeatureCount)
{
  void** ppFeatureList = NULL;
  int nMaxFeatures = 0;
  int nFeatureCount = 0;

  CPLAssert(hQuadTree);
  CPLAssert(pAoi);

  if (pnFeatureCount == NULL)
      pnFeatureCount = &nFeatureCount;

  *pnFeatureCount = 0;
  CPLQuadTreeCollectFeatures(hQuadTree, hQuadTree->psRoot, pAoi,
                            pnFeatureCount, &nMaxFeatures, &ppFeatureList);

  return(ppFeatureList);
}

/************************************************************************/
/*                    CPLQuadTreeNodeForeach()                          */
/************************************************************************/

static int CPLQuadTreeNodeForeach(const QuadTreeNode *psNode,
                                  CPLQuadTreeForeachFunc pfnForeach,
                                  void* pUserData)
{
    int i;
    for(i=0; i<psNode->nNumSubNodes; i++ )
    {
        if (CPLQuadTreeNodeForeach(psNode->apSubNode[i], pfnForeach, pUserData) == FALSE)
            return FALSE;
    }

    for(i=0; i<psNode->nFeatures; i++)
    {
        if (pfnForeach(psNode->pahFeatures[i], pUserData) == FALSE)
            return FALSE;
    }

    return TRUE;
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

void  CPLQuadTreeForeach(const CPLQuadTree *hQuadTree,
                         CPLQuadTreeForeachFunc pfnForeach,
                         void* pUserData)
{
    CPLAssert(hQuadTree);
    CPLAssert(pfnForeach);
    CPLQuadTreeNodeForeach(hQuadTree->psRoot, pfnForeach, pUserData);
}

/************************************************************************/
/*                       CPLQuadTreeDumpNode()                          */
/************************************************************************/

static void CPLQuadTreeDumpNode(const QuadTreeNode *psNode,
                                int nIndentLevel,
                                CPLQuadTreeDumpFeatureFunc pfnDumpFeatureFunc,
                                void* pUserData)
{
    int i;
    int count;
    if (psNode->nNumSubNodes)
    {
        for(count=nIndentLevel;--count>=0;)
            printf("  ");
        printf("SubhQuadTrees :\n");
        for(i=0; i<psNode->nNumSubNodes; i++ )
        {
            for(count=nIndentLevel+1;--count>=0;)
                printf("  ");
            printf("SubhQuadTree %d :\n", i+1);
            CPLQuadTreeDumpNode(psNode->apSubNode[i], nIndentLevel + 2,
                                pfnDumpFeatureFunc, pUserData);
        }
    }
    if (psNode->nFeatures)
    {
        for(count=nIndentLevel;--count>=0;)
            printf("  ");
        printf("Leaves (%d):\n", psNode->nFeatures);
        for(i=0; i<psNode->nFeatures; i++)
        {
            if (pfnDumpFeatureFunc)
                pfnDumpFeatureFunc(psNode->pahFeatures[i], nIndentLevel + 2,
                                   pUserData);
            else
            {
                for(count=nIndentLevel + 1;--count>=0;)
                    printf("  ");
                printf("%p\n", psNode->pahFeatures[i]);
            }
        }
    }
}

/************************************************************************/
/*                         CPLQuadTreeDump()                            */
/************************************************************************/

void CPLQuadTreeDump(const CPLQuadTree *hQuadTree,
                     CPLQuadTreeDumpFeatureFunc pfnDumpFeatureFunc,
                     void* pUserData)
{
    CPLQuadTreeDumpNode(hQuadTree->psRoot, 0, pfnDumpFeatureFunc, pUserData);
}

/************************************************************************/
/*                  CPLQuadTreeGetStatsNode()                           */
/************************************************************************/

static
void CPLQuadTreeGetStatsNode(const QuadTreeNode *psNode,
                             int nDepthLevel,
                             int* pnNodeCount,
                             int* pnMaxDepth,
                             int* pnMaxBucketCapacity)
{
    int i;
    (*pnNodeCount) ++;
    if (nDepthLevel > *pnMaxDepth)
        *pnMaxDepth = nDepthLevel;
    if (psNode->nFeatures > *pnMaxBucketCapacity)
        *pnMaxBucketCapacity = psNode->nFeatures;
    for(i=0; i<psNode->nNumSubNodes; i++ )
    {
        CPLQuadTreeGetStatsNode(psNode->apSubNode[i], nDepthLevel + 1,
                                pnNodeCount, pnMaxDepth, pnMaxBucketCapacity);
    }
}


/************************************************************************/
/*                    CPLQuadTreeGetStats()                             */
/************************************************************************/

void CPLQuadTreeGetStats(const CPLQuadTree *hQuadTree,
                         int* pnFeatureCount,
                         int* pnNodeCount,
                         int* pnMaxDepth,
                         int* pnMaxBucketCapacity)
{
    int nFeatureCount, nNodeCount, nMaxDepth, nMaxBucketCapacity;
    CPLAssert(hQuadTree);
    if (pnFeatureCount == NULL)
        pnFeatureCount = &nFeatureCount;
    if (pnNodeCount == NULL)
        pnNodeCount = &nNodeCount;
    if (pnMaxDepth == NULL)
        pnMaxDepth = &nMaxDepth;
    if (pnMaxBucketCapacity == NULL)
        pnMaxBucketCapacity = &nMaxBucketCapacity;

    *pnFeatureCount = hQuadTree->nFeatures;
    *pnNodeCount = 0;
    *pnMaxDepth = 1;
    *pnMaxBucketCapacity = 0;

    CPLQuadTreeGetStatsNode(hQuadTree->psRoot, 0, pnNodeCount, pnMaxDepth, pnMaxBucketCapacity);
}
