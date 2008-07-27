/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implementation of quadtree building and searching functions.
 *           Derived from shapelib and mapserver implementations
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

typedef struct shape_tree_node {
  /* area covered by this node */
  CPLRectObj rect;
  
  /* list of shapes stored at this node. */
  int numfeatures;
  void** features;
  
  int numsubnodes;
  struct shape_tree_node *subnode[MAX_SUBNODES];
} treeNodeObj;

struct _CPLQuadTree {
  int numfeatures;
  int maxdepth;
  treeNodeObj *root;
} ;


/* -------------------------------------------------------------------- */
/*      If the following is 0.5, nodes will be split in half.  If it    */
/*      is 0.6 then each subnode will contain 60% of the parent         */
/*      node, with 20% representing overlap.  This can be help to       */
/*      prevent small objects on a boundary from shifting too high      */
/*      up the tree.                                                    */
/* -------------------------------------------------------------------- */
#define SPLITRATIO  0.55

/*
** Returns TRUE if rectangle a is contained in rectangle b
*/
static CPL_INLINE int CPL_RectContained(CPLRectObj *a, CPLRectObj *b)
{
  if(a->minx >= b->minx && a->maxx <= b->maxx)
    if(a->miny >= b->miny && a->maxy <= b->maxy)
      return(TRUE);
  return(FALSE);  
}

/*
** Returns TRUE if rectangles a and b overlap
*/
static CPL_INLINE int CPL_RectOverlap(CPLRectObj *a, CPLRectObj *b)
{
  if(a->minx > b->maxx) return(FALSE);
  if(a->maxx < b->minx) return(FALSE);
  if(a->miny > b->maxy) return(FALSE);
  if(a->maxy < b->miny) return(FALSE);
  return(TRUE);
}

static int CPL_treeAddFeature(CPLQuadTree *tree, void* feature, CPLRectObj* pRect);

static treeNodeObj *CPL_treeNodeCreate(CPLRectObj* pRect)
{
    treeNodeObj	*node;

    node = (treeNodeObj *) CPLMalloc(sizeof(treeNodeObj));

    node->numfeatures = 0;
    node->features = NULL;

    node->numsubnodes = 0;

    memcpy(&(node->rect), pRect, sizeof(CPLRectObj));

    return node;
}


CPLQuadTree *CPL_CreateTree(int numfeatures, void** features,
                            CPLQuadTreeGetBoundsFunc get_bound_func,
                            CPLRectObj* pGlobalBounds, int maxdepth)
{
  int i;
  CPLQuadTree *tree;
  CPLRectObj bounds;

  CPLAssert(pGlobalBounds);
  CPLAssert(get_bound_func);

  /* -------------------------------------------------------------------- */
  /*      Allocate the tree object                                        */
  /* -------------------------------------------------------------------- */
  tree = (CPLQuadTree *) CPLMalloc(sizeof(CPLQuadTree));
  
  tree->numfeatures = numfeatures;
  tree->maxdepth = maxdepth;

  /* -------------------------------------------------------------------- */
  /*      If no max depth was defined, try to select a reasonable one     */
  /*      that implies approximately 8 shapes per node.                   */
  /* -------------------------------------------------------------------- */
  if( tree->maxdepth == 0 ) {
    int numnodes = 1;
    
    while(numnodes*4 < numfeatures) {
      tree->maxdepth += 1;
      numnodes = numnodes * 2;
    }

    /* NOTE: Due to problems with memory allocation for deep trees,
     * automatically estimated depth is limited up to 12 levels.
     * See Ticket #1594 for detailed discussion.
     */
    if( tree->maxdepth > MAX_DEFAULT_TREE_DEPTH )
    {
        tree->maxdepth = MAX_DEFAULT_TREE_DEPTH;

        CPLDebug( "CPLQuadTree",
                  "Falling back to max number of allowed index tree levels (%d).",
                  MAX_DEFAULT_TREE_DEPTH );
     }
  }

  /* -------------------------------------------------------------------- */
  /*      Allocate the root node.                                         */
  /* -------------------------------------------------------------------- */
  tree->root = CPL_treeNodeCreate(pGlobalBounds);

  for(i=0; i<numfeatures; i++) {
      get_bound_func(features[i], &bounds);
      CPL_treeAddFeature(tree, features[i], &bounds);
  }

  return tree;
}

static void CPL_destroyTreeNode(treeNodeObj *node)
{
  int i;
  
  for(i=0; i<node->numsubnodes; i++ ) {
    if(node->subnode[i]) 
      CPL_destroyTreeNode(node->subnode[i]);
  }
  
  if(node->features)
    CPLFree(node->features);

  CPLFree(node);
}

void CPL_DestroyTree(CPLQuadTree *tree)
{
  CPLAssert(tree);
  CPL_destroyTreeNode(tree->root);
  CPLFree(tree);
}

static void CPL_treeSplitBounds( CPLRectObj *in, CPLRectObj *out1, CPLRectObj *out2)
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
    
    out1->maxx = in->minx + range * SPLITRATIO;
    out2->minx = in->maxx - range * SPLITRATIO;
  }

  /* -------------------------------------------------------------------- */
  /*      Otherwise split in Y direction.                                 */
  /* -------------------------------------------------------------------- */
  else {
    range = in->maxy - in->miny;
    
    out1->maxy = in->miny + range * SPLITRATIO;
    out2->miny = in->maxy - range * SPLITRATIO;
  }
}

static int CPL_treeNodeAddFeature( treeNodeObj *node, void* feature, CPLRectObj* pRect, int maxdepth)
{
  int i;
    
  /* -------------------------------------------------------------------- */
  /*      If there are subnodes, then consider whether this object        */
  /*      will fit in them.                                               */
  /* -------------------------------------------------------------------- */
  if( maxdepth > 1 && node->numsubnodes > 0 ) {
    for(i=0; i<node->numsubnodes; i++ ) {
      if( CPL_RectContained(pRect, &node->subnode[i]->rect)) {
	return CPL_treeNodeAddFeature( node->subnode[i], feature, pRect, maxdepth-1);
      }
    }
  }

  /* -------------------------------------------------------------------- */
  /*      Otherwise, consider creating four subnodes if could fit into    */
  /*      them, and adding to the appropriate subnode.                    */
  /* -------------------------------------------------------------------- */
  else if( maxdepth > 1 && node->numsubnodes == 0 ) {
    CPLRectObj half1, half2, quad1, quad2, quad3, quad4;

    CPL_treeSplitBounds(&node->rect, &half1, &half2);
    CPL_treeSplitBounds(&half1, &quad1, &quad2);
    CPL_treeSplitBounds(&half2, &quad3, &quad4);
  
    if(CPL_RectContained(pRect, &quad1) ||
       CPL_RectContained(pRect, &quad2) ||
       CPL_RectContained(pRect, &quad3) ||
       CPL_RectContained(pRect, &quad4)) {
      node->numsubnodes = 4;
      node->subnode[0] = CPL_treeNodeCreate(&quad1);
      node->subnode[1] = CPL_treeNodeCreate(&quad2);
      node->subnode[2] = CPL_treeNodeCreate(&quad3);
      node->subnode[3] = CPL_treeNodeCreate(&quad4);
      
      /* recurse back on this node now that it has subnodes */
      return(CPL_treeNodeAddFeature(node, feature, pRect, maxdepth));
    }
  }

/* -------------------------------------------------------------------- */
/*      If none of that worked, just add it to this nodes list.         */
/* -------------------------------------------------------------------- */
    node->numfeatures++;

    node->features = (void**) CPLRealloc( node->features, sizeof(void*) * node->numfeatures );
    node->features[node->numfeatures-1] = feature;

    return TRUE;
}

static int CPL_treeAddFeature(CPLQuadTree *tree, void* feature, CPLRectObj *pRect)
{
  return(CPL_treeNodeAddFeature(tree->root, feature, pRect, tree->maxdepth));
}

static void CPL_treeCollectShapeIds(treeNodeObj *node, CPLRectObj* pAoi,
                                    int* pnFeatureCount, int* pnMaxFeatures, void*** pppFeatureList)
{
  int i;

  /* -------------------------------------------------------------------- */
  /*      Does this node overlap the area of interest at all?  If not,    */
  /*      return without adding to the list at all.                       */
  /* -------------------------------------------------------------------- */
  if(!CPL_RectOverlap(&node->rect, pAoi))
    return;

/* -------------------------------------------------------------------- */
/*      Grow the list to hold the shapes on this node.                  */
/* -------------------------------------------------------------------- */
    if( *pnFeatureCount + node->numfeatures > *pnMaxFeatures )
    {
        *pnMaxFeatures = (*pnFeatureCount + node->numfeatures) * 2 + 20;
        *pppFeatureList = (void**)
            CPLRealloc(*pppFeatureList,sizeof(void*) * *pnMaxFeatures);
    }

  /* -------------------------------------------------------------------- */
  /*      Add the local nodes shapefeatures to the list.                       */
  /* -------------------------------------------------------------------- */
  for(i=0; i<node->numfeatures; i++)
    (*pppFeatureList)[(*pnFeatureCount)++] = node->features[i];
  
  /* -------------------------------------------------------------------- */
  /*      Recurse to subnodes if they exist.                              */
  /* -------------------------------------------------------------------- */
  for(i=0; i<node->numsubnodes; i++) {
    if(node->subnode[i])
      CPL_treeCollectShapeIds(node->subnode[i], pAoi, pnFeatureCount, pnMaxFeatures, pppFeatureList);
  }
}

void** CPL_SearchTree(CPLQuadTree *tree, CPLRectObj* pAoi, int* pnFeatureCount)
{
  void** ppFeatureList = NULL;
  int nMaxFeatures = 0;

  CPLAssert(tree);
  CPLAssert(pAoi);
  CPLAssert(pnFeatureCount);

  *pnFeatureCount = 0;
  CPL_treeCollectShapeIds(tree->root, pAoi, pnFeatureCount, &nMaxFeatures, &ppFeatureList);

  return(ppFeatureList);
}

static int CPL_treeNodeTrim( treeNodeObj *node )
{
    int	i;

    /* -------------------------------------------------------------------- */
    /*      Trim subtrees, and free subnodes that come back empty.          */
    /* -------------------------------------------------------------------- */
    for(i=0; i<node->numsubnodes; i++ ) {
      if(CPL_treeNodeTrim(node->subnode[i])) {
	CPL_destroyTreeNode(node->subnode[i]);
	node->subnode[i] = node->subnode[node->numsubnodes-1];
	node->numsubnodes--;
	i--; /* process the new occupant of this subnode entry */
      }
    }

    if( node->numsubnodes == 1 && node->numfeatures == 0 ) {
      node = node->subnode[0];
    }
/* if I only have 1 subnode promote that subnode to my positon */ 

    /* -------------------------------------------------------------------- */
    /*      We should be trimmed if we have no subnodes, and no shapes.     */
    /* -------------------------------------------------------------------- */

    return(node->numsubnodes == 0 && node->numfeatures == 0);
}

void CPL_TreeTrim(CPLQuadTree *tree)
{
  CPLAssert(tree);
  CPL_treeNodeTrim(tree->root);
}

