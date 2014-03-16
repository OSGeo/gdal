/******************************************************************************
 * $Id$
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Thin Plate Spline transformer (GDAL wrapper portion)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "thinplatespline.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"
#include "gdal_priv.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_atomic_ops.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

CPL_C_START
CPLXMLNode *GDALSerializeTPSTransformer( void *pTransformArg );
void *GDALDeserializeTPSTransformer( CPLXMLNode *psTree );
CPL_C_END

typedef struct
{
    GDALTransformerInfo  sTI;

    VizGeorefSpline2D   *poForward;
    VizGeorefSpline2D   *poReverse;
    int                  bForwardSolved;
    int                  bReverseSolved;

    int       bReversed;

    int       nGCPCount;
    GDAL_GCP *pasGCPList;
    
    volatile int nRefCount;
    
} TPSTransformInfo;

/************************************************************************/
/*                       GDALCloneTPSTransformer()                      */
/************************************************************************/

void* GDALCloneTPSTransformer( void *hTransformArg )
{
    VALIDATE_POINTER1( hTransformArg, "GDALCloneTPSTransformer", NULL );

    TPSTransformInfo *psInfo = 
        (TPSTransformInfo *) hTransformArg;

    /* We can just use a ref count, since using the source transformation */
    /* is thread-safe */
    CPLAtomicInc(&(psInfo->nRefCount));

    return psInfo;
}

/************************************************************************/
/*                      GDALCreateTPSTransformer()                      */
/************************************************************************/

/**
 * Create Thin Plate Spline transformer from GCPs.
 *
 * The thin plate spline transformer produces exact transformation
 * at all control points and smoothly varying transformations between
 * control points with greatest influence from local control points. 
 * It is suitable for for many applications not well modelled by polynomial
 * transformations. 
 *
 * Creating the TPS transformer involves solving systems of linear equations
 * related to the number of control points involved.  This solution is
 * computed within this function call.  It can be quite an expensive operation
 * for large numbers of GCPs.  For instance, for reference, it takes on the 
 * order of 10s for 400 GCPs on a 2GHz Athlon processor. 
 *
 * TPS Transformers are serializable. 
 *
 * The GDAL Thin Plate Spline transformer is based on code provided by
 * Gilad Ronnen on behalf of VIZRT Inc (http://www.visrt.com).  Incorporation 
 * of the algorithm into GDAL was supported by the Centro di Ecologia Alpina 
 * (http://www.cealp.it). 
 *
 * @param nGCPCount the number of GCPs in pasGCPList.
 * @param pasGCPList an array of GCPs to be used as input.
 * @param bReversed set it to TRUE to compute the reversed transformation.
 * 
 * @return the transform argument or NULL if creation fails. 
 */

void *GDALCreateTPSTransformer( int nGCPCount, const GDAL_GCP *pasGCPList, 
                                int bReversed )
{
    return GDALCreateTPSTransformerInt(nGCPCount, pasGCPList, bReversed, NULL);
}

static void GDALTPSComputeForwardInThread(void* pData)
{
    TPSTransformInfo *psInfo = (TPSTransformInfo *)pData;
    psInfo->bForwardSolved = psInfo->poForward->solve() != 0;
}

void *GDALCreateTPSTransformerInt( int nGCPCount, const GDAL_GCP *pasGCPList, 
                                   int bReversed, char** papszOptions )

{
    TPSTransformInfo *psInfo;
    int    iGCP;

/* -------------------------------------------------------------------- */
/*      Allocate transform info.                                        */
/* -------------------------------------------------------------------- */
    psInfo = (TPSTransformInfo *) CPLCalloc(sizeof(TPSTransformInfo),1);

    psInfo->pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPList );
    psInfo->nGCPCount = nGCPCount;

    psInfo->bReversed = bReversed;
    psInfo->poForward = new VizGeorefSpline2D( 2 );
    psInfo->poReverse = new VizGeorefSpline2D( 2 );

    strcpy( psInfo->sTI.szSignature, "GTI" );
    psInfo->sTI.pszClassName = "GDALTPSTransformer";
    psInfo->sTI.pfnTransform = GDALTPSTransform;
    psInfo->sTI.pfnCleanup = GDALDestroyTPSTransformer;
    psInfo->sTI.pfnSerialize = GDALSerializeTPSTransformer;

/* -------------------------------------------------------------------- */
/*      Attach all the points to the transformation.                    */
/* -------------------------------------------------------------------- */
    for( iGCP = 0; iGCP < nGCPCount; iGCP++ )
    {
        double    afPL[2], afXY[2];

        afPL[0] = pasGCPList[iGCP].dfGCPPixel;
        afPL[1] = pasGCPList[iGCP].dfGCPLine;
        afXY[0] = pasGCPList[iGCP].dfGCPX;
        afXY[1] = pasGCPList[iGCP].dfGCPY;

        if( bReversed )
        {
            psInfo->poReverse->add_point( afPL[0], afPL[1], afXY );
            psInfo->poForward->add_point( afXY[0], afXY[1], afPL );
        }
        else
        {
            psInfo->poForward->add_point( afPL[0], afPL[1], afXY );
            psInfo->poReverse->add_point( afXY[0], afXY[1], afPL );
        }
    }

    psInfo->nRefCount = 1;

    int nThreads = 1;
    if( nGCPCount > 100 )
    {
        const char* pszWarpThreads = CSLFetchNameValue(papszOptions, "NUM_THREADS");
        if (pszWarpThreads == NULL)
            pszWarpThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "1");
        if (EQUAL(pszWarpThreads, "ALL_CPUS"))
            nThreads = CPLGetNumCPUs();
        else
            nThreads = atoi(pszWarpThreads);
    }

    if( nThreads > 1 )
    {
        /* Compute direct and reverse transforms in parallel */
        void* hThread = CPLCreateJoinableThread(GDALTPSComputeForwardInThread, psInfo);
        psInfo->bReverseSolved = psInfo->poReverse->solve() != 0;
        if( hThread != NULL )
            CPLJoinThread(hThread);
        else
            psInfo->bForwardSolved = psInfo->poForward->solve() != 0;
    }
    else
    {
        psInfo->bForwardSolved = psInfo->poForward->solve() != 0;
        psInfo->bReverseSolved = psInfo->poReverse->solve() != 0;
    }

    if( !psInfo->bForwardSolved || !psInfo->bReverseSolved )
    {
        delete psInfo;
        return NULL;
    }

    return psInfo;
}

/************************************************************************/
/*                     GDALDestroyTPSTransformer()                      */
/************************************************************************/

/**
 * Destroy TPS transformer.
 *
 * This function is used to destroy information about a GCP based
 * polynomial transformation created with GDALCreateTPSTransformer(). 
 *
 * @param pTransformArg the transform arg previously returned by 
 * GDALCreateTPSTransformer(). 
 */

void GDALDestroyTPSTransformer( void *pTransformArg )

{
    VALIDATE_POINTER0( pTransformArg, "GDALDestroyTPSTransformer" );

    TPSTransformInfo *psInfo = (TPSTransformInfo *) pTransformArg;

    if( CPLAtomicDec(&(psInfo->nRefCount)) == 0 )
    {
        delete psInfo->poForward;
        delete psInfo->poReverse;

        GDALDeinitGCPs( psInfo->nGCPCount, psInfo->pasGCPList );
        CPLFree( psInfo->pasGCPList );
        
        CPLFree( pTransformArg );
    }
}

/************************************************************************/
/*                          GDALTPSTransform()                          */
/************************************************************************/

/**
 * Transforms point based on GCP derived polynomial model.
 *
 * This function matches the GDALTransformerFunc signature, and can be
 * used to transform one or more points from pixel/line coordinates to
 * georeferenced coordinates (SrcToDst) or vice versa (DstToSrc).
 *
 * @param pTransformArg return value from GDALCreateTPSTransformer(). 
 * @param bDstToSrc TRUE if transformation is from the destination 
 * (georeferenced) coordinates to pixel/line or FALSE when transforming
 * from pixel/line to georeferenced coordinates.
 * @param nPointCount the number of values in the x, y and z arrays.
 * @param x array containing the X values to be transformed.
 * @param y array containing the Y values to be transformed.
 * @param z array containing the Z values to be transformed.
 * @param panSuccess array in which a flag indicating success (TRUE) or
 * failure (FALSE) of the transformation are placed.
 *
 * @return TRUE.
 */

int GDALTPSTransform( void *pTransformArg, int bDstToSrc, 
                      int nPointCount, 
                      double *x, double *y, double *z, 
                      int *panSuccess )

{
    VALIDATE_POINTER1( pTransformArg, "GDALTPSTransform", 0 );

    int    i;
    TPSTransformInfo *psInfo = (TPSTransformInfo *) pTransformArg;

    for( i = 0; i < nPointCount; i++ )
    {
        double xy_out[2];

        if( bDstToSrc )
        {
            psInfo->poReverse->get_point( x[i], y[i], xy_out );
            x[i] = xy_out[0];
            y[i] = xy_out[1];
        }
        else
        {
            psInfo->poForward->get_point( x[i], y[i], xy_out );
            x[i] = xy_out[0];
            y[i] = xy_out[1];
        }
        panSuccess[i] = TRUE;
    }

    return TRUE;
}

/************************************************************************/
/*                    GDALSerializeTPSTransformer()                     */
/************************************************************************/

CPLXMLNode *GDALSerializeTPSTransformer( void *pTransformArg )

{
    VALIDATE_POINTER1( pTransformArg, "GDALSerializeTPSTransformer", NULL );

    CPLXMLNode *psTree;
    TPSTransformInfo *psInfo = static_cast<TPSTransformInfo *>(pTransformArg);

    psTree = CPLCreateXMLNode( NULL, CXT_Element, "TPSTransformer" );

/* -------------------------------------------------------------------- */
/*      Serialize bReversed.                                            */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue( 
        psTree, "Reversed", 
        CPLString().Printf( "%d", psInfo->bReversed ) );
                                 
/* -------------------------------------------------------------------- */
/*	Attach GCP List. 						*/
/* -------------------------------------------------------------------- */
    if( psInfo->nGCPCount > 0 )
    {
        GDALSerializeGCPListToXML( psTree,
                                   psInfo->pasGCPList,
                                   psInfo->nGCPCount,
                                   NULL );
    }

    return psTree;
}

/************************************************************************/
/*                   GDALDeserializeTPSTransformer()                    */
/************************************************************************/

void *GDALDeserializeTPSTransformer( CPLXMLNode *psTree )

{
    GDAL_GCP *pasGCPList = 0;
    int nGCPCount = 0;
    void *pResult;
    int bReversed;

    /* -------------------------------------------------------------------- */
    /*      Check for GCPs.                                                 */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psGCPList = CPLGetXMLNode( psTree, "GCPList" );

    if( psGCPList != NULL )
    {
        GDALDeserializeGCPListFromXML( psGCPList,
                                       &pasGCPList,
                                       &nGCPCount,
                                       NULL );
    }

/* -------------------------------------------------------------------- */
/*      Get other flags.                                                */
/* -------------------------------------------------------------------- */
    bReversed = atoi(CPLGetXMLValue(psTree,"Reversed","0"));

/* -------------------------------------------------------------------- */
/*      Generate transformation.                                        */
/* -------------------------------------------------------------------- */
    pResult = GDALCreateTPSTransformer( nGCPCount, pasGCPList, bReversed );
    
/* -------------------------------------------------------------------- */
/*      Cleanup GCP copy.                                               */
/* -------------------------------------------------------------------- */
    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );

    return pResult;
}
