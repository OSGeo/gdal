/******************************************************************************
 * $Id: shptree.c,v 1.17 2012-01-27 21:09:26 fwarmerdam Exp $
 *
 * Project:  Shapelib
 * Purpose:  Implementation of quadtree building and searching functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * This software is available under the following "MIT Style" license,
 * or at the option of the licensee under the LGPL (see LICENSE.LGPL).  This
 * option is discussed in more detail in shapelib.html.
 *
 * --
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
 * $Log: shptree.c,v $
 * Revision 1.17  2012-01-27 21:09:26  fwarmerdam
 * optimize .qix output (gdal #4472)
 *
 * Revision 1.16  2011-12-11 22:26:46  fwarmerdam
 * upgrade .qix access code to use SAHooks (gdal #3365)
 *
 * Revision 1.15  2011-07-24 05:59:25  fwarmerdam
 * minimize use of CPLError in favor of SAHooks.Error()
 *
 * Revision 1.14  2010-08-27 23:43:27  fwarmerdam
 * add SHPAPI_CALL attribute in code
 *
 * Revision 1.13  2010-06-29 05:50:15  fwarmerdam
 * fix sign of Z/M comparisons in SHPCheckObjectContained (#2223)
 *
 * Revision 1.12  2008-11-12 15:39:50  fwarmerdam
 * improve safety in face of buggy .shp file.
 *
 * Revision 1.11  2007/10/27 03:31:14  fwarmerdam
 * limit default depth of tree to 12 levels (gdal ticket #1594)
 *
 * Revision 1.10  2005/01/03 22:30:13  fwarmerdam
 * added support for saved quadtrees
 *
 * Revision 1.9  2003/01/28 15:53:41  warmerda
 * Avoid build warnings.
 *
 * Revision 1.8  2002/05/07 13:07:45  warmerda
 * use qsort() - patch from Bernhard Herzog
 *
 * Revision 1.7  2002/01/15 14:36:07  warmerda
 * updated email address
 *
 * Revision 1.6  2001/05/23 13:36:52  warmerda
 * added use of SHPAPI_CALL
 *
 * Revision 1.5  1999/11/05 14:12:05  warmerda
 * updated license terms
 *
 * Revision 1.4  1999/06/02 18:24:21  warmerda
 * added trimming code
 *
 * Revision 1.3  1999/06/02 17:56:12  warmerda
 * added quad'' subnode support for trees
 *
 * Revision 1.2  1999/05/18 19:11:11  warmerda
 * Added example searching capability
 *
 * Revision 1.1  1999/05/18 17:49:20  warmerda
 * New
 *
 */

#include "shapefil.h"

#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_CPL
#include "cpl_error.h"
#endif

SHP_CVSID("$Id: shptree.c,v 1.17 2012-01-27 21:09:26 fwarmerdam Exp $")

#ifndef TRUE
#  define TRUE 1
#  define FALSE 0
#endif

static int bBigEndian = 0;


/* -------------------------------------------------------------------- */
/*      If the following is 0.5, nodes will be split in half.  If it    */
/*      is 0.6 then each subnode will contain 60% of the parent         */
/*      node, with 20% representing overlap.  This can be help to       */
/*      prevent small objects on a boundary from shifting too high      */
/*      up the tree.                                                    */
/* -------------------------------------------------------------------- */

#define SHP_SPLIT_RATIO	0.55

/************************************************************************/
/*                             SfRealloc()                              */
/*                                                                      */
/*      A realloc cover function that will access a NULL pointer as     */
/*      a valid input.                                                  */
/************************************************************************/

static void * SfRealloc( void * pMem, int nNewSize )

{
    if( pMem == NULL )
        return( (void *) malloc(nNewSize) );
    else
        return( (void *) realloc(pMem,nNewSize) );
}

/************************************************************************/
/*                          SHPTreeNodeInit()                           */
/*                                                                      */
/*      Initialize a tree node.                                         */
/************************************************************************/

static SHPTreeNode *SHPTreeNodeCreate( double * padfBoundsMin,
                                       double * padfBoundsMax )

{
    SHPTreeNode	*psTreeNode;

    psTreeNode = (SHPTreeNode *) malloc(sizeof(SHPTreeNode));
    if( NULL == psTreeNode )
        return NULL;

    psTreeNode->nShapeCount = 0;
    psTreeNode->panShapeIds = NULL;
    psTreeNode->papsShapeObj = NULL;

    psTreeNode->nSubNodes = 0;

    if( padfBoundsMin != NULL )
        memcpy( psTreeNode->adfBoundsMin, padfBoundsMin, sizeof(double) * 4 );

    if( padfBoundsMax != NULL )
        memcpy( psTreeNode->adfBoundsMax, padfBoundsMax, sizeof(double) * 4 );

    return psTreeNode;
}


/************************************************************************/
/*                           SHPCreateTree()                            */
/************************************************************************/

SHPTree SHPAPI_CALL1(*)
    SHPCreateTree( SHPHandle hSHP, int nDimension, int nMaxDepth,
                   double *padfBoundsMin, double *padfBoundsMax )

{
    SHPTree	*psTree;

    if( padfBoundsMin == NULL && hSHP == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Allocate the tree object                                        */
/* -------------------------------------------------------------------- */
    psTree = (SHPTree *) malloc(sizeof(SHPTree));
    if( NULL == psTree )
    {
        return NULL;
    }

    psTree->hSHP = hSHP;
    psTree->nMaxDepth = nMaxDepth;
    psTree->nDimension = nDimension;
    psTree->nTotalCount = 0;

/* -------------------------------------------------------------------- */
/*      If no max depth was defined, try to select a reasonable one     */
/*      that implies approximately 8 shapes per node.                   */
/* -------------------------------------------------------------------- */
    if( psTree->nMaxDepth == 0 && hSHP != NULL )
    {
        int	nMaxNodeCount = 1;
        int	nShapeCount;

        SHPGetInfo( hSHP, &nShapeCount, NULL, NULL, NULL );
        while( nMaxNodeCount*4 < nShapeCount )
        {
            psTree->nMaxDepth += 1;
            nMaxNodeCount = nMaxNodeCount * 2;
        }

#ifdef USE_CPL
        CPLDebug( "Shape",
                  "Estimated spatial index tree depth: %d",
                  psTree->nMaxDepth );
#endif

        /* NOTE: Due to problems with memory allocation for deep trees,
         * automatically estimated depth is limited up to 12 levels.
         * See Ticket #1594 for detailed discussion.
         */
        if( psTree->nMaxDepth > MAX_DEFAULT_TREE_DEPTH )
        {
            psTree->nMaxDepth = MAX_DEFAULT_TREE_DEPTH;

#ifdef USE_CPL
            CPLDebug( "Shape",
                      "Falling back to max number of allowed index tree levels (%d).",
                      MAX_DEFAULT_TREE_DEPTH );
#endif
        }
    }

/* -------------------------------------------------------------------- */
/*      Allocate the root node.                                         */
/* -------------------------------------------------------------------- */
    psTree->psRoot = SHPTreeNodeCreate( padfBoundsMin, padfBoundsMax );
    if( NULL == psTree->psRoot )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Assign the bounds to the root node.  If none are passed in,     */
/*      use the bounds of the provided file otherwise the create        */
/*      function will have already set the bounds.                      */
/* -------------------------------------------------------------------- */
    assert( NULL != psTree );
    assert( NULL != psTree->psRoot );
	
    if( padfBoundsMin == NULL )
    {
        SHPGetInfo( hSHP, NULL, NULL,
                    psTree->psRoot->adfBoundsMin, 
                    psTree->psRoot->adfBoundsMax );
    }

/* -------------------------------------------------------------------- */
/*      If we have a file, insert all it's shapes into the tree.        */
/* -------------------------------------------------------------------- */
    if( hSHP != NULL )
    {
        int	iShape, nShapeCount;
        
        SHPGetInfo( hSHP, &nShapeCount, NULL, NULL, NULL );

        for( iShape = 0; iShape < nShapeCount; iShape++ )
        {
            SHPObject	*psShape;
            
            psShape = SHPReadObject( hSHP, iShape );
            if( psShape != NULL )
            {
                SHPTreeAddShapeId( psTree, psShape );
                SHPDestroyObject( psShape );
            }
        }
    }        

    return psTree;
}

/************************************************************************/
/*                         SHPDestroyTreeNode()                         */
/************************************************************************/

static void SHPDestroyTreeNode( SHPTreeNode * psTreeNode )

{
    int		i;
    
	assert( NULL != psTreeNode );

    for( i = 0; i < psTreeNode->nSubNodes; i++ )
    {
        if( psTreeNode->apsSubNode[i] != NULL )
            SHPDestroyTreeNode( psTreeNode->apsSubNode[i] );
    }
    
    if( psTreeNode->panShapeIds != NULL )
        free( psTreeNode->panShapeIds );

    if( psTreeNode->papsShapeObj != NULL )
    {
        for( i = 0; i < psTreeNode->nShapeCount; i++ )
        {
            if( psTreeNode->papsShapeObj[i] != NULL )
                SHPDestroyObject( psTreeNode->papsShapeObj[i] );
        }

        free( psTreeNode->papsShapeObj );
    }

    free( psTreeNode );
}

/************************************************************************/
/*                           SHPDestroyTree()                           */
/************************************************************************/

void SHPAPI_CALL
SHPDestroyTree( SHPTree * psTree )

{
    SHPDestroyTreeNode( psTree->psRoot );
    free( psTree );
}

/************************************************************************/
/*                       SHPCheckBoundsOverlap()                        */
/*                                                                      */
/*      Do the given boxes overlap at all?                              */
/************************************************************************/

int SHPAPI_CALL
SHPCheckBoundsOverlap( double * padfBox1Min, double * padfBox1Max,
                       double * padfBox2Min, double * padfBox2Max,
                       int nDimension )

{
    int		iDim;

    for( iDim = 0; iDim < nDimension; iDim++ )
    {
        if( padfBox2Max[iDim] < padfBox1Min[iDim] )
            return FALSE;
        
        if( padfBox1Max[iDim] < padfBox2Min[iDim] )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                      SHPCheckObjectContained()                       */
/*                                                                      */
/*      Does the given shape fit within the indicated extents?          */
/************************************************************************/

static int SHPCheckObjectContained( SHPObject * psObject, int nDimension,
                           double * padfBoundsMin, double * padfBoundsMax )

{
    if( psObject->dfXMin < padfBoundsMin[0]
        || psObject->dfXMax > padfBoundsMax[0] )
        return FALSE;
    
    if( psObject->dfYMin < padfBoundsMin[1]
        || psObject->dfYMax > padfBoundsMax[1] )
        return FALSE;

    if( nDimension == 2 )
        return TRUE;
    
    if( psObject->dfZMin < padfBoundsMin[2]
        || psObject->dfZMax > padfBoundsMax[2] )
        return FALSE;
        
    if( nDimension == 3 )
        return TRUE;

    if( psObject->dfMMin < padfBoundsMin[3]
        || psObject->dfMMax > padfBoundsMax[3] )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                         SHPTreeSplitBounds()                         */
/*                                                                      */
/*      Split a region into two subregion evenly, cutting along the     */
/*      longest dimension.                                              */
/************************************************************************/

void SHPAPI_CALL
SHPTreeSplitBounds( double *padfBoundsMinIn, double *padfBoundsMaxIn,
                    double *padfBoundsMin1, double * padfBoundsMax1,
                    double *padfBoundsMin2, double * padfBoundsMax2 )

{
/* -------------------------------------------------------------------- */
/*      The output bounds will be very similar to the input bounds,     */
/*      so just copy over to start.                                     */
/* -------------------------------------------------------------------- */
    memcpy( padfBoundsMin1, padfBoundsMinIn, sizeof(double) * 4 );
    memcpy( padfBoundsMax1, padfBoundsMaxIn, sizeof(double) * 4 );
    memcpy( padfBoundsMin2, padfBoundsMinIn, sizeof(double) * 4 );
    memcpy( padfBoundsMax2, padfBoundsMaxIn, sizeof(double) * 4 );
    
/* -------------------------------------------------------------------- */
/*      Split in X direction.                                           */
/* -------------------------------------------------------------------- */
    if( (padfBoundsMaxIn[0] - padfBoundsMinIn[0])
        			> (padfBoundsMaxIn[1] - padfBoundsMinIn[1]) )
    {
        double	dfRange = padfBoundsMaxIn[0] - padfBoundsMinIn[0];

        padfBoundsMax1[0] = padfBoundsMinIn[0] + dfRange * SHP_SPLIT_RATIO;
        padfBoundsMin2[0] = padfBoundsMaxIn[0] - dfRange * SHP_SPLIT_RATIO;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise split in Y direction.                                 */
/* -------------------------------------------------------------------- */
    else
    {
        double	dfRange = padfBoundsMaxIn[1] - padfBoundsMinIn[1];

        padfBoundsMax1[1] = padfBoundsMinIn[1] + dfRange * SHP_SPLIT_RATIO;
        padfBoundsMin2[1] = padfBoundsMaxIn[1] - dfRange * SHP_SPLIT_RATIO;
    }
}

/************************************************************************/
/*                       SHPTreeNodeAddShapeId()                        */
/************************************************************************/

static int
SHPTreeNodeAddShapeId( SHPTreeNode * psTreeNode, SHPObject * psObject,
                       int nMaxDepth, int nDimension )

{
    int		i;
    
/* -------------------------------------------------------------------- */
/*      If there are subnodes, then consider wiether this object        */
/*      will fit in them.                                               */
/* -------------------------------------------------------------------- */
    if( nMaxDepth > 1 && psTreeNode->nSubNodes > 0 )
    {
        for( i = 0; i < psTreeNode->nSubNodes; i++ )
        {
            if( SHPCheckObjectContained(psObject, nDimension,
                                      psTreeNode->apsSubNode[i]->adfBoundsMin,
                                      psTreeNode->apsSubNode[i]->adfBoundsMax))
            {
                return SHPTreeNodeAddShapeId( psTreeNode->apsSubNode[i],
                                              psObject, nMaxDepth-1,
                                              nDimension );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise, consider creating four subnodes if could fit into    */
/*      them, and adding to the appropriate subnode.                    */
/* -------------------------------------------------------------------- */
#if MAX_SUBNODE == 4
    else if( nMaxDepth > 1 && psTreeNode->nSubNodes == 0 )
    {
        double	adfBoundsMinH1[4], adfBoundsMaxH1[4];
        double	adfBoundsMinH2[4], adfBoundsMaxH2[4];
        double	adfBoundsMin1[4], adfBoundsMax1[4];
        double	adfBoundsMin2[4], adfBoundsMax2[4];
        double	adfBoundsMin3[4], adfBoundsMax3[4];
        double	adfBoundsMin4[4], adfBoundsMax4[4];

        SHPTreeSplitBounds( psTreeNode->adfBoundsMin,
                            psTreeNode->adfBoundsMax,
                            adfBoundsMinH1, adfBoundsMaxH1,
                            adfBoundsMinH2, adfBoundsMaxH2 );

        SHPTreeSplitBounds( adfBoundsMinH1, adfBoundsMaxH1,
                            adfBoundsMin1, adfBoundsMax1,
                            adfBoundsMin2, adfBoundsMax2 );

        SHPTreeSplitBounds( adfBoundsMinH2, adfBoundsMaxH2,
                            adfBoundsMin3, adfBoundsMax3,
                            adfBoundsMin4, adfBoundsMax4 );

        if( SHPCheckObjectContained(psObject, nDimension,
                                    adfBoundsMin1, adfBoundsMax1)
            || SHPCheckObjectContained(psObject, nDimension,
                                    adfBoundsMin2, adfBoundsMax2)
            || SHPCheckObjectContained(psObject, nDimension,
                                    adfBoundsMin3, adfBoundsMax3)
            || SHPCheckObjectContained(psObject, nDimension,
                                    adfBoundsMin4, adfBoundsMax4) )
        {
            psTreeNode->nSubNodes = 4;
            psTreeNode->apsSubNode[0] = SHPTreeNodeCreate( adfBoundsMin1,
                                                           adfBoundsMax1 );
            psTreeNode->apsSubNode[1] = SHPTreeNodeCreate( adfBoundsMin2,
                                                           adfBoundsMax2 );
            psTreeNode->apsSubNode[2] = SHPTreeNodeCreate( adfBoundsMin3,
                                                           adfBoundsMax3 );
            psTreeNode->apsSubNode[3] = SHPTreeNodeCreate( adfBoundsMin4,
                                                           adfBoundsMax4 );

            /* recurse back on this node now that it has subnodes */
            return( SHPTreeNodeAddShapeId( psTreeNode, psObject,
                                           nMaxDepth, nDimension ) );
        }
    }
#endif /* MAX_SUBNODE == 4 */

/* -------------------------------------------------------------------- */
/*      Otherwise, consider creating two subnodes if could fit into     */
/*      them, and adding to the appropriate subnode.                    */
/* -------------------------------------------------------------------- */
#if MAX_SUBNODE == 2
    else if( nMaxDepth > 1 && psTreeNode->nSubNodes == 0 )
    {
        double	adfBoundsMin1[4], adfBoundsMax1[4];
        double	adfBoundsMin2[4], adfBoundsMax2[4];

        SHPTreeSplitBounds( psTreeNode->adfBoundsMin, psTreeNode->adfBoundsMax,
                            adfBoundsMin1, adfBoundsMax1,
                            adfBoundsMin2, adfBoundsMax2 );

        if( SHPCheckObjectContained(psObject, nDimension,
                                 adfBoundsMin1, adfBoundsMax1))
        {
            psTreeNode->nSubNodes = 2;
            psTreeNode->apsSubNode[0] = SHPTreeNodeCreate( adfBoundsMin1,
                                                           adfBoundsMax1 );
            psTreeNode->apsSubNode[1] = SHPTreeNodeCreate( adfBoundsMin2,
                                                           adfBoundsMax2 );

            return( SHPTreeNodeAddShapeId( psTreeNode->apsSubNode[0], psObject,
                                           nMaxDepth - 1, nDimension ) );
        }
        else if( SHPCheckObjectContained(psObject, nDimension,
                                         adfBoundsMin2, adfBoundsMax2) )
        {
            psTreeNode->nSubNodes = 2;
            psTreeNode->apsSubNode[0] = SHPTreeNodeCreate( adfBoundsMin1,
                                                           adfBoundsMax1 );
            psTreeNode->apsSubNode[1] = SHPTreeNodeCreate( adfBoundsMin2,
                                                           adfBoundsMax2 );

            return( SHPTreeNodeAddShapeId( psTreeNode->apsSubNode[1], psObject,
                                           nMaxDepth - 1, nDimension ) );
        }
    }
#endif /* MAX_SUBNODE == 2 */

/* -------------------------------------------------------------------- */
/*      If none of that worked, just add it to this nodes list.         */
/* -------------------------------------------------------------------- */
    psTreeNode->nShapeCount++;

    psTreeNode->panShapeIds = (int *) 
        SfRealloc( psTreeNode->panShapeIds,
                   sizeof(int) * psTreeNode->nShapeCount );
    psTreeNode->panShapeIds[psTreeNode->nShapeCount-1] = psObject->nShapeId;

    if( psTreeNode->papsShapeObj != NULL )
    {
        psTreeNode->papsShapeObj = (SHPObject **)
            SfRealloc( psTreeNode->papsShapeObj,
                       sizeof(void *) * psTreeNode->nShapeCount );
        psTreeNode->papsShapeObj[psTreeNode->nShapeCount-1] = NULL;
    }

    return TRUE;
}

/************************************************************************/
/*                         SHPTreeAddShapeId()                          */
/*                                                                      */
/*      Add a shape to the tree, but don't keep a pointer to the        */
/*      object data, just keep the shapeid.                             */
/************************************************************************/

int SHPAPI_CALL
SHPTreeAddShapeId( SHPTree * psTree, SHPObject * psObject )

{
    psTree->nTotalCount++;

    return( SHPTreeNodeAddShapeId( psTree->psRoot, psObject,
                                   psTree->nMaxDepth, psTree->nDimension ) );
}

/************************************************************************/
/*                      SHPTreeCollectShapesIds()                       */
/*                                                                      */
/*      Work function implementing SHPTreeFindLikelyShapes() on a       */
/*      tree node by tree node basis.                                   */
/************************************************************************/

void SHPAPI_CALL
SHPTreeCollectShapeIds( SHPTree *hTree, SHPTreeNode * psTreeNode,
                        double * padfBoundsMin, double * padfBoundsMax,
                        int * pnShapeCount, int * pnMaxShapes,
                        int ** ppanShapeList )

{
    int		i;
    
/* -------------------------------------------------------------------- */
/*      Does this node overlap the area of interest at all?  If not,    */
/*      return without adding to the list at all.                       */
/* -------------------------------------------------------------------- */
    if( !SHPCheckBoundsOverlap( psTreeNode->adfBoundsMin,
                                psTreeNode->adfBoundsMax,
                                padfBoundsMin,
                                padfBoundsMax,
                                hTree->nDimension ) )
        return;

/* -------------------------------------------------------------------- */
/*      Grow the list to hold the shapes on this node.                  */
/* -------------------------------------------------------------------- */
    if( *pnShapeCount + psTreeNode->nShapeCount > *pnMaxShapes )
    {
        *pnMaxShapes = (*pnShapeCount + psTreeNode->nShapeCount) * 2 + 20;
        *ppanShapeList = (int *)
            SfRealloc(*ppanShapeList,sizeof(int) * *pnMaxShapes);
    }

/* -------------------------------------------------------------------- */
/*      Add the local nodes shapeids to the list.                       */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psTreeNode->nShapeCount; i++ )
    {
        (*ppanShapeList)[(*pnShapeCount)++] = psTreeNode->panShapeIds[i];
    }
    
/* -------------------------------------------------------------------- */
/*      Recurse to subnodes if they exist.                              */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psTreeNode->nSubNodes; i++ )
    {
        if( psTreeNode->apsSubNode[i] != NULL )
            SHPTreeCollectShapeIds( hTree, psTreeNode->apsSubNode[i],
                                    padfBoundsMin, padfBoundsMax,
                                    pnShapeCount, pnMaxShapes,
                                    ppanShapeList );
    }
}

/************************************************************************/
/*                      SHPTreeFindLikelyShapes()                       */
/*                                                                      */
/*      Find all shapes within tree nodes for which the tree node       */
/*      bounding box overlaps the search box.  The return value is      */
/*      an array of shapeids terminated by a -1.  The shapeids will     */
/*      be in order, as hopefully this will result in faster (more      */
/*      sequential) reading from the file.                              */
/************************************************************************/

/* helper for qsort */
static int
compare_ints( const void * a, const void * b)
{
    return (*(int*)a) - (*(int*)b);
}

int SHPAPI_CALL1(*)
SHPTreeFindLikelyShapes( SHPTree * hTree,
                         double * padfBoundsMin, double * padfBoundsMax,
                         int * pnShapeCount )

{
    int	*panShapeList=NULL, nMaxShapes = 0;

/* -------------------------------------------------------------------- */
/*      Perform the search by recursive descent.                        */
/* -------------------------------------------------------------------- */
    *pnShapeCount = 0;

    SHPTreeCollectShapeIds( hTree, hTree->psRoot,
                            padfBoundsMin, padfBoundsMax,
                            pnShapeCount, &nMaxShapes,
                            &panShapeList );

/* -------------------------------------------------------------------- */
/*      Sort the id array                                               */
/* -------------------------------------------------------------------- */

    qsort(panShapeList, *pnShapeCount, sizeof(int), compare_ints);

    return panShapeList;
}

/************************************************************************/
/*                          SHPTreeNodeTrim()                           */
/*                                                                      */
/*      This is the recurve version of SHPTreeTrimExtraNodes() that     */
/*      walks the tree cleaning it up.                                  */
/************************************************************************/

static int SHPTreeNodeTrim( SHPTreeNode * psTreeNode )

{
    int		i;

/* -------------------------------------------------------------------- */
/*      Trim subtrees, and free subnodes that come back empty.          */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psTreeNode->nSubNodes; i++ )
    {
        if( SHPTreeNodeTrim( psTreeNode->apsSubNode[i] ) )
        {
            SHPDestroyTreeNode( psTreeNode->apsSubNode[i] );

            psTreeNode->apsSubNode[i] =
                psTreeNode->apsSubNode[psTreeNode->nSubNodes-1];

            psTreeNode->nSubNodes--;

            i--; /* process the new occupant of this subnode entry */
        }
    }

/* -------------------------------------------------------------------- */
/*      If the current node has 1 subnode and no shapes, promote that   */
/*      subnode to the current node position.                           */
/* -------------------------------------------------------------------- */
    if( psTreeNode->nSubNodes == 1 && psTreeNode->nShapeCount == 0)
    {
        SHPTreeNode* psSubNode = psTreeNode->apsSubNode[0];

        memcpy(psTreeNode->adfBoundsMin, psSubNode->adfBoundsMin,
               sizeof(psSubNode->adfBoundsMin));
        memcpy(psTreeNode->adfBoundsMax, psSubNode->adfBoundsMax,
               sizeof(psSubNode->adfBoundsMax));
        psTreeNode->nShapeCount = psSubNode->nShapeCount;
        assert(psTreeNode->panShapeIds == NULL);
        psTreeNode->panShapeIds = psSubNode->panShapeIds;
        assert(psTreeNode->papsShapeObj == NULL);
        psTreeNode->papsShapeObj = psSubNode->papsShapeObj;
        psTreeNode->nSubNodes = psSubNode->nSubNodes;
        for( i = 0; i < psSubNode->nSubNodes; i++ )
            psTreeNode->apsSubNode[i] = psSubNode->apsSubNode[i];
        free(psSubNode);
    }

/* -------------------------------------------------------------------- */
/*      We should be trimmed if we have no subnodes, and no shapes.     */
/* -------------------------------------------------------------------- */
    return( psTreeNode->nSubNodes == 0 && psTreeNode->nShapeCount == 0 );
}

/************************************************************************/
/*                       SHPTreeTrimExtraNodes()                        */
/*                                                                      */
/*      Trim empty nodes from the tree.  Note that we never trim an     */
/*      empty root node.                                                */
/************************************************************************/

void SHPAPI_CALL
SHPTreeTrimExtraNodes( SHPTree * hTree )

{
    SHPTreeNodeTrim( hTree->psRoot );
}

/************************************************************************/
/*                              SwapWord()                              */
/*                                                                      */
/*      Swap a 2, 4 or 8 byte word.                                     */
/************************************************************************/

static void SwapWord( int length, void * wordP )

{
    int		i;
    unsigned char	temp;

    for( i=0; i < length/2; i++ )
    {
	temp = ((unsigned char *) wordP)[i];
	((unsigned char *)wordP)[i] = ((unsigned char *) wordP)[length-i-1];
	((unsigned char *) wordP)[length-i-1] = temp;
    }
}


struct SHPDiskTreeInfo
{
    SAHooks sHooks;
    SAFile  fpQIX;
};

/************************************************************************/
/*                         SHPOpenDiskTree()                            */
/************************************************************************/

SHPTreeDiskHandle SHPOpenDiskTree( const char* pszQIXFilename,
                                   SAHooks *psHooks )
{
    SHPTreeDiskHandle hDiskTree;

    hDiskTree = (SHPTreeDiskHandle) calloc(sizeof(struct SHPDiskTreeInfo),1);

    if (psHooks == NULL)
        SASetupDefaultHooks( &(hDiskTree->sHooks) );
    else
        memcpy( &(hDiskTree->sHooks), psHooks, sizeof(SAHooks) );

    hDiskTree->fpQIX = hDiskTree->sHooks.FOpen(pszQIXFilename, "rb");
    if (hDiskTree->fpQIX == NULL)
    {
        free(hDiskTree);
        return NULL;
    }

    return hDiskTree;
}

/***********************************************************************/
/*                         SHPCloseDiskTree()                           */
/************************************************************************/

void SHPCloseDiskTree( SHPTreeDiskHandle hDiskTree )
{
    if (hDiskTree == NULL)
        return;

    hDiskTree->sHooks.FClose(hDiskTree->fpQIX);
    free(hDiskTree);
}

/************************************************************************/
/*                       SHPSearchDiskTreeNode()                        */
/************************************************************************/

static int
SHPSearchDiskTreeNode( SHPTreeDiskHandle hDiskTree, double *padfBoundsMin, double *padfBoundsMax,
                       int **ppanResultBuffer, int *pnBufferMax, 
                       int *pnResultCount, int bNeedSwap, int nRecLevel )

{
    unsigned int i;
    unsigned int offset;
    unsigned int numshapes, numsubnodes;
    double adfNodeBoundsMin[2], adfNodeBoundsMax[2];
    int nFReadAcc;

/* -------------------------------------------------------------------- */
/*      Read and unswap first part of node info.                        */
/* -------------------------------------------------------------------- */
    nFReadAcc = (int)hDiskTree->sHooks.FRead( &offset, 4, 1, hDiskTree->fpQIX );
    if ( bNeedSwap ) SwapWord ( 4, &offset );

    nFReadAcc += (int)hDiskTree->sHooks.FRead( adfNodeBoundsMin, sizeof(double), 2, hDiskTree->fpQIX );
    nFReadAcc += (int)hDiskTree->sHooks.FRead( adfNodeBoundsMax, sizeof(double), 2, hDiskTree->fpQIX );
    if ( bNeedSwap )
    {
        SwapWord( 8, adfNodeBoundsMin + 0 );
        SwapWord( 8, adfNodeBoundsMin + 1 );
        SwapWord( 8, adfNodeBoundsMax + 0 );
        SwapWord( 8, adfNodeBoundsMax + 1 );
    }
      
    nFReadAcc += (int)hDiskTree->sHooks.FRead( &numshapes, 4, 1, hDiskTree->fpQIX );
    if ( bNeedSwap ) SwapWord ( 4, &numshapes );

    /* Check that we could read all previous values */
    if( nFReadAcc != 1 + 2 + 2 + 1 )
    {
        hDiskTree->sHooks.Error("I/O error");
        return FALSE;
    }

    /* Sanity checks to avoid int overflows in later computation */
    if( offset > INT_MAX - sizeof(int) )
    {
        hDiskTree->sHooks.Error("Invalid value for offset");
        return FALSE;
    }

    if( numshapes > (INT_MAX - offset - sizeof(int)) / sizeof(int) ||
        numshapes > INT_MAX / sizeof(int) - *pnResultCount )
    {
        hDiskTree->sHooks.Error("Invalid value for numshapes");
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If we don't overlap this node at all, we can just fseek()       */
/*      pass this node info and all subnodes.                           */
/* -------------------------------------------------------------------- */
    if( !SHPCheckBoundsOverlap( adfNodeBoundsMin, adfNodeBoundsMax, 
                                padfBoundsMin, padfBoundsMax, 2 ) )
    {
        offset += numshapes*sizeof(int) + sizeof(int);
        hDiskTree->sHooks.FSeek(hDiskTree->fpQIX, offset, SEEK_CUR);
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Add all the shapeids at this node to our list.                  */
/* -------------------------------------------------------------------- */
    if(numshapes > 0) 
    {
        if( *pnResultCount + numshapes > *pnBufferMax )
        {
            int* pNewBuffer;

            *pnBufferMax = (*pnResultCount + numshapes + 100) * 5 / 4;

            if( *pnBufferMax > INT_MAX / sizeof(int) )
                *pnBufferMax = *pnResultCount + numshapes;

            pNewBuffer = (int *)
                SfRealloc( *ppanResultBuffer, *pnBufferMax * sizeof(int) );

            if( pNewBuffer == NULL )
            {
                hDiskTree->sHooks.Error("Out of memory error");
                return FALSE;
            }

            *ppanResultBuffer = pNewBuffer;
        }

        if( hDiskTree->sHooks.FRead( *ppanResultBuffer + *pnResultCount,
                    sizeof(int), numshapes, hDiskTree->fpQIX ) != numshapes )
        {
            hDiskTree->sHooks.Error("I/O error");
            return FALSE;
        }

        if (bNeedSwap )
        {
            for( i=0; i<numshapes; i++ )
                SwapWord( 4, *ppanResultBuffer + *pnResultCount + i );
        }

        *pnResultCount += numshapes; 
    } 

/* -------------------------------------------------------------------- */
/*      Process the subnodes.                                           */
/* -------------------------------------------------------------------- */
    if( hDiskTree->sHooks.FRead( &numsubnodes, 4, 1, hDiskTree->fpQIX ) != 1 )
    {
        hDiskTree->sHooks.Error("I/O error");
        return FALSE;
    }
    if ( bNeedSwap  ) SwapWord ( 4, &numsubnodes );
    if( numsubnodes > 0 && nRecLevel == 32 )
    {
        hDiskTree->sHooks.Error("Shape tree is too deep");
        return FALSE;
    }

    for(i=0; i<numsubnodes; i++)
    {
        if( !SHPSearchDiskTreeNode( hDiskTree, padfBoundsMin, padfBoundsMax, 
                                    ppanResultBuffer, pnBufferMax, 
                                    pnResultCount, bNeedSwap, nRecLevel + 1 ) )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                          SHPTreeReadLibc()                           */
/************************************************************************/

static
SAOffset SHPTreeReadLibc( void *p, SAOffset size, SAOffset nmemb, SAFile file )

{
    return (SAOffset) fread( p, (size_t) size, (size_t) nmemb,
                                 (FILE *) file );
}

/************************************************************************/
/*                          SHPTreeSeekLibc()                           */
/************************************************************************/

static
SAOffset SHPTreeSeekLibc( SAFile file, SAOffset offset, int whence )

{
    return (SAOffset) fseek( (FILE *) file, (long) offset, whence );
}

/************************************************************************/
/*                         SHPSearchDiskTree()                          */
/************************************************************************/

int SHPAPI_CALL1(*) 
SHPSearchDiskTree( FILE *fp, 
                   double *padfBoundsMin, double *padfBoundsMax,
                   int *pnShapeCount )
{
    struct SHPDiskTreeInfo sDiskTree;
    memset(&sDiskTree.sHooks, 0, sizeof(sDiskTree.sHooks));

    /* We do not use SASetupDefaultHooks() because the FILE* */
    /* is a libc FILE* */
    sDiskTree.sHooks.FSeek = SHPTreeSeekLibc;
    sDiskTree.sHooks.FRead = SHPTreeReadLibc;

    sDiskTree.fpQIX = (SAFile)fp;

    return SHPSearchDiskTreeEx( &sDiskTree, padfBoundsMin, padfBoundsMax,
                                 pnShapeCount );
}

/***********************************************************************/
/*                       SHPSearchDiskTreeEx()                         */
/************************************************************************/

int* SHPSearchDiskTreeEx( SHPTreeDiskHandle hDiskTree, 
                     double *padfBoundsMin, double *padfBoundsMax,
                     int *pnShapeCount )

{
    int i, bNeedSwap, nBufferMax = 0;
    unsigned char abyBuf[16];
    int *panResultBuffer = NULL;

    *pnShapeCount = 0;

/* -------------------------------------------------------------------- */
/*	Establish the byte order on this machine.	  	        */
/* -------------------------------------------------------------------- */
    i = 1;
    if( *((unsigned char *) &i) == 1 )
        bBigEndian = FALSE;
    else
        bBigEndian = TRUE;

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    hDiskTree->sHooks.FSeek( hDiskTree->fpQIX, 0, SEEK_SET );
    hDiskTree->sHooks.FRead( abyBuf, 16, 1, hDiskTree->fpQIX );

    if( memcmp( abyBuf, "SQT", 3 ) != 0 )
        return NULL;

    if( (abyBuf[3] == 2 && bBigEndian)
        || (abyBuf[3] == 1 && !bBigEndian) )
        bNeedSwap = FALSE;
    else
        bNeedSwap = TRUE;

/* -------------------------------------------------------------------- */
/*      Search through root node and it's decendents.                   */
/* -------------------------------------------------------------------- */
    if( !SHPSearchDiskTreeNode( hDiskTree, padfBoundsMin, padfBoundsMax, 
                                &panResultBuffer, &nBufferMax, 
                                pnShapeCount, bNeedSwap, 0 ) )
    {
        if( panResultBuffer != NULL )
            free( panResultBuffer );
        *pnShapeCount = 0;
        return NULL;
    }
/* -------------------------------------------------------------------- */
/*      Sort the id array                                               */
/* -------------------------------------------------------------------- */
    qsort(panResultBuffer, *pnShapeCount, sizeof(int), compare_ints);

    /* To distinguish between empty intersection from error case */
    if( panResultBuffer == NULL )
        panResultBuffer = (int*) calloc(1, sizeof(int));
    
    return panResultBuffer;
}

/************************************************************************/
/*                        SHPGetSubNodeOffset()                         */
/*                                                                      */
/*      Determine how big all the subnodes of this node (and their      */
/*      children) will be.  This will allow disk based searchers to     */
/*      seek past them all efficiently.                                 */
/************************************************************************/

static int SHPGetSubNodeOffset( SHPTreeNode *node) 
{
    int i;
    long offset=0;

    for(i=0; i<node->nSubNodes; i++ ) 
    {
        if(node->apsSubNode[i]) 
        {
            offset += 4*sizeof(double) 
                + (node->apsSubNode[i]->nShapeCount+3)*sizeof(int);
            offset += SHPGetSubNodeOffset(node->apsSubNode[i]);
        }
    }

    return(offset);
}

/************************************************************************/
/*                          SHPWriteTreeNode()                          */
/************************************************************************/

static void SHPWriteTreeNode( SAFile fp, SHPTreeNode *node, SAHooks* psHooks) 
{
    int i,j;
    int offset;
    unsigned char *pabyRec = NULL;
    assert( NULL != node );

    offset = SHPGetSubNodeOffset(node);
  
    pabyRec = (unsigned char *) 
        malloc(sizeof(double) * 4
               + (3 * sizeof(int)) + (node->nShapeCount * sizeof(int)) );
    if( NULL == pabyRec )
    {
#ifdef USE_CPL
        CPLError( CE_Fatal, CPLE_OutOfMemory, "Memory allocation failure");
#endif
        assert( 0 );
        return;
    }

    memcpy( pabyRec, &offset, 4);

    /* minx, miny, maxx, maxy */
    memcpy( pabyRec+ 4, node->adfBoundsMin+0, sizeof(double) );
    memcpy( pabyRec+12, node->adfBoundsMin+1, sizeof(double) );
    memcpy( pabyRec+20, node->adfBoundsMax+0, sizeof(double) );
    memcpy( pabyRec+28, node->adfBoundsMax+1, sizeof(double) );

    memcpy( pabyRec+36, &node->nShapeCount, 4);
    j = node->nShapeCount * sizeof(int);
    memcpy( pabyRec+40, node->panShapeIds, j);
    memcpy( pabyRec+j+40, &node->nSubNodes, 4);

    psHooks->FWrite( pabyRec, 44+j, 1, fp );
    free (pabyRec);
  
    for(i=0; i<node->nSubNodes; i++ ) 
    {
        if(node->apsSubNode[i])
            SHPWriteTreeNode( fp, node->apsSubNode[i], psHooks);
    }
}

/************************************************************************/
/*                            SHPWriteTree()                            */
/************************************************************************/

int SHPAPI_CALL SHPWriteTree(SHPTree *tree, const char *filename )
{
    SAHooks sHooks;

    SASetupDefaultHooks( &sHooks );

    return SHPWriteTreeLL(tree, filename, &sHooks);
}

/************************************************************************/
/*                           SHPWriteTreeLL()                           */
/************************************************************************/

int SHPWriteTreeLL(SHPTree *tree, const char *filename, SAHooks* psHooks )
{
    char		signature[4] = "SQT";
    int		        i;
    char		abyBuf[32];
    SAFile              fp;

    SAHooks sHooks;
    if (psHooks == NULL)
    {
        SASetupDefaultHooks( &sHooks );
        psHooks = &sHooks;
    }
  
/* -------------------------------------------------------------------- */
/*      Open the output file.                                           */
/* -------------------------------------------------------------------- */
    fp = psHooks->FOpen(filename, "wb");
    if( fp == NULL ) 
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*	Establish the byte order on this machine.	  	        */
/* -------------------------------------------------------------------- */
    i = 1;
    if( *((unsigned char *) &i) == 1 )
        bBigEndian = FALSE;
    else
        bBigEndian = TRUE;
  
/* -------------------------------------------------------------------- */
/*      Write the header.                                               */
/* -------------------------------------------------------------------- */
    memcpy( abyBuf+0, signature, 3 );
    
    if( bBigEndian )
        abyBuf[3] = 2; /* New MSB */
    else
        abyBuf[3] = 1; /* New LSB */

    abyBuf[4] = 1; /* version */
    abyBuf[5] = 0; /* next 3 reserved */
    abyBuf[6] = 0;
    abyBuf[7] = 0;

    psHooks->FWrite( abyBuf, 8, 1, fp );

    psHooks->FWrite( &(tree->nTotalCount), 4, 1, fp );

    /* write maxdepth */

    psHooks->FWrite( &(tree->nMaxDepth), 4, 1, fp );

/* -------------------------------------------------------------------- */
/*      Write all the nodes "in order".                                 */
/* -------------------------------------------------------------------- */

    SHPWriteTreeNode( fp, tree->psRoot, psHooks );  
    
    psHooks->FClose( fp );

    return TRUE;
}
