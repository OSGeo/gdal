/******************************************************************************
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Thin Plate Spline transformer (GDAL wrapper portion)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "thinplatespline.h"

#include <stdlib.h>
#include <string.h>
#include <map>
#include <utility>

#include "cpl_atomic_ops.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

CPL_C_START
CPLXMLNode *GDALSerializeTPSTransformer( void *pTransformArg );
void *GDALDeserializeTPSTransformer( CPLXMLNode *psTree );
CPL_C_END

typedef struct
{
    GDALTransformerInfo  sTI;

    VizGeorefSpline2D   *poForward;
    VizGeorefSpline2D   *poReverse;
    bool                 bForwardSolved;
    bool                 bReverseSolved;

    bool      bReversed;

    int       nGCPCount;
    GDAL_GCP *pasGCPList;

    volatile int nRefCount;

} TPSTransformInfo;

/************************************************************************/
/*                   GDALCreateSimilarTPSTransformer()                  */
/************************************************************************/

static
void* GDALCreateSimilarTPSTransformer( void *hTransformArg,
                                       double dfRatioX, double dfRatioY )
{
    VALIDATE_POINTER1( hTransformArg, "GDALCreateSimilarTPSTransformer", nullptr );

    TPSTransformInfo *psInfo = static_cast<TPSTransformInfo *>(hTransformArg);

    if( dfRatioX == 1.0 && dfRatioY == 1.0 )
    {
        // We can just use a ref count, since using the source transformation
        // is thread-safe.
        CPLAtomicInc(&(psInfo->nRefCount));
    }
    else
    {
        GDAL_GCP *pasGCPList = GDALDuplicateGCPs( psInfo->nGCPCount,
                                                  psInfo->pasGCPList );
        for( int i = 0; i < psInfo->nGCPCount; i++ )
        {
            pasGCPList[i].dfGCPPixel /= dfRatioX;
            pasGCPList[i].dfGCPLine /= dfRatioY;
        }
        psInfo = static_cast<TPSTransformInfo *>(
            GDALCreateTPSTransformer( psInfo->nGCPCount, pasGCPList,
                                      psInfo->bReversed ));
        GDALDeinitGCPs( psInfo->nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

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
 * It is suitable for for many applications not well modeled by polynomial
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
    return GDALCreateTPSTransformerInt(nGCPCount, pasGCPList, bReversed, nullptr);
}

static void GDALTPSComputeForwardInThread( void *pData )
{
    TPSTransformInfo *psInfo = static_cast<TPSTransformInfo *>(pData);
    psInfo->bForwardSolved = psInfo->poForward->solve() != 0;
}

void *GDALCreateTPSTransformerInt( int nGCPCount, const GDAL_GCP *pasGCPList,
                                   int bReversed, char** papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Allocate transform info.                                        */
/* -------------------------------------------------------------------- */
    TPSTransformInfo *psInfo = static_cast<TPSTransformInfo *>(
        CPLCalloc(sizeof(TPSTransformInfo), 1));

    psInfo->pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPList );
    psInfo->nGCPCount = nGCPCount;

    psInfo->bReversed = CPL_TO_BOOL(bReversed);
    psInfo->poForward = new VizGeorefSpline2D( 2 );
    psInfo->poReverse = new VizGeorefSpline2D( 2 );

    memcpy( psInfo->sTI.abySignature,
            GDAL_GTI2_SIGNATURE,
            strlen(GDAL_GTI2_SIGNATURE) );
    psInfo->sTI.pszClassName = "GDALTPSTransformer";
    psInfo->sTI.pfnTransform = GDALTPSTransform;
    psInfo->sTI.pfnCleanup = GDALDestroyTPSTransformer;
    psInfo->sTI.pfnSerialize = GDALSerializeTPSTransformer;
    psInfo->sTI.pfnCreateSimilar = GDALCreateSimilarTPSTransformer;

/* -------------------------------------------------------------------- */
/*      Attach (non-redundant) points to the transformation.            */
/* -------------------------------------------------------------------- */
    std::map< std::pair<double, double>, int > oMapPixelLineToIdx;
    std::map< std::pair<double, double>, int > oMapXYToIdx;
    for( int iGCP = 0; iGCP < nGCPCount; iGCP++ )
    {
        const double afPL[2] = {
            pasGCPList[iGCP].dfGCPPixel,
            pasGCPList[iGCP].dfGCPLine };
        const double afXY[2] =
            { pasGCPList[iGCP].dfGCPX, pasGCPList[iGCP].dfGCPY };

        std::map< std::pair<double, double>, int >::iterator oIter(
            oMapPixelLineToIdx.find(std::pair<double, double>(afPL[0],
                                                              afPL[1])));
        if( oIter != oMapPixelLineToIdx.end() )
        {
            if( afXY[0] == pasGCPList[oIter->second].dfGCPX &&
                afXY[1] == pasGCPList[oIter->second].dfGCPY )
            {
                continue;
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "GCP %d and %d have same (pixel,line)=(%f,%f), "
                         "but different (X,Y): (%f,%f) vs (%f,%f)",
                         iGCP + 1, oIter->second,
                         afPL[0], afPL[1],
                         afXY[0], afXY[1],
                         pasGCPList[oIter->second].dfGCPX,
                         pasGCPList[oIter->second].dfGCPY);
            }
        }
        else
        {
            oMapPixelLineToIdx[std::pair<double, double>(afPL[0], afPL[1])] =
                iGCP;
        }

        oIter = oMapXYToIdx.find(std::pair<double, double>(afXY[0], afXY[1]));
        if( oIter != oMapXYToIdx.end() )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "GCP %d and %d have same (x,y)=(%f,%f), "
                     "but different (pixel,line): (%f,%f) vs (%f,%f)",
                     iGCP + 1, oIter->second,
                     afXY[0], afXY[1],
                     afPL[0], afPL[1],
                     pasGCPList[oIter->second].dfGCPPixel,
                     pasGCPList[oIter->second].dfGCPLine);
        }
        else
        {
            oMapXYToIdx[std::pair<double, double>(afXY[0], afXY[1])] = iGCP;
        }

        bool bOK = true;
        if( bReversed )
        {
            bOK &= psInfo->poReverse->add_point( afPL[0], afPL[1], afXY );
            bOK &= psInfo->poForward->add_point( afXY[0], afXY[1], afPL );
        }
        else
        {
            bOK &= psInfo->poForward->add_point( afPL[0], afPL[1], afXY );
            bOK &= psInfo->poReverse->add_point( afXY[0], afXY[1], afPL );
        }
        if( !bOK )
        {
            GDALDestroyTPSTransformer(psInfo);
            return nullptr;
        }
    }

    psInfo->nRefCount = 1;

    int nThreads = 1;
    if( nGCPCount > 100 )
    {
        const char* pszWarpThreads =
            CSLFetchNameValue(papszOptions, "NUM_THREADS");
        if( pszWarpThreads == nullptr )
            pszWarpThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "1");
        if( EQUAL(pszWarpThreads, "ALL_CPUS") )
            nThreads = CPLGetNumCPUs();
        else
            nThreads = atoi(pszWarpThreads);
    }

    if( nThreads > 1 )
    {
        // Compute direct and reverse transforms in parallel.
        CPLJoinableThread* hThread =
            CPLCreateJoinableThread(GDALTPSComputeForwardInThread, psInfo);
        psInfo->bReverseSolved = psInfo->poReverse->solve() != 0;
        if( hThread != nullptr )
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
        GDALDestroyTPSTransformer(psInfo);
        return nullptr;
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
    if( pTransformArg == nullptr )
        return;

    TPSTransformInfo *psInfo = static_cast<TPSTransformInfo *>(pTransformArg);

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
                      double *x, double *y,
                      CPL_UNUSED double *z,
                      int *panSuccess )
{
    VALIDATE_POINTER1( pTransformArg, "GDALTPSTransform", 0 );

    TPSTransformInfo *psInfo = static_cast<TPSTransformInfo *>(pTransformArg);

    for( int i = 0; i < nPointCount; i++ )
    {
        double xy_out[2] = { 0.0, 0.0 };

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
    VALIDATE_POINTER1( pTransformArg, "GDALSerializeTPSTransformer", nullptr );

    TPSTransformInfo *psInfo = static_cast<TPSTransformInfo *>(pTransformArg);

    CPLXMLNode *psTree = CPLCreateXMLNode(nullptr, CXT_Element, "TPSTransformer");

/* -------------------------------------------------------------------- */
/*      Serialize bReversed.                                            */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue(
        psTree, "Reversed",
        CPLString().Printf( "%d", static_cast<int>(psInfo->bReversed) ) );

/* -------------------------------------------------------------------- */
/*      Attach GCP List.                                                */
/* -------------------------------------------------------------------- */
    if( psInfo->nGCPCount > 0 )
    {
        GDALSerializeGCPListToXML( psTree,
                                   psInfo->pasGCPList,
                                   psInfo->nGCPCount,
                                   nullptr );
    }

    return psTree;
}

/************************************************************************/
/*                   GDALDeserializeTPSTransformer()                    */
/************************************************************************/

void *GDALDeserializeTPSTransformer( CPLXMLNode *psTree )

{
    /* -------------------------------------------------------------------- */
    /*      Check for GCPs.                                                 */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psGCPList = CPLGetXMLNode( psTree, "GCPList" );
    GDAL_GCP *pasGCPList = nullptr;
    int nGCPCount = 0;

    if( psGCPList != nullptr )
    {
        GDALDeserializeGCPListFromXML( psGCPList,
                                       &pasGCPList,
                                       &nGCPCount,
                                       nullptr );
    }

/* -------------------------------------------------------------------- */
/*      Get other flags.                                                */
/* -------------------------------------------------------------------- */
    const int bReversed = atoi(CPLGetXMLValue(psTree, "Reversed", "0"));

/* -------------------------------------------------------------------- */
/*      Generate transformation.                                        */
/* -------------------------------------------------------------------- */
    void *pResult =
        GDALCreateTPSTransformer( nGCPCount, pasGCPList, bReversed );

/* -------------------------------------------------------------------- */
/*      Cleanup GCP copy.                                               */
/* -------------------------------------------------------------------- */
    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );

    return pResult;
}
