/******************************************************************************
 * $Id$
 *
 * Project:  Mapinfo Image Warper
 * Purpose:  Implementation of one or more GDALTrasformerFunc types, including
 *           the GenImgProj (general image reprojector) transformer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, i3 - information integration and imaging 
 *                          Fort Collin, CO
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
 * $Log$
 * Revision 1.1  2002/12/05 05:43:23  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "gdal_alg.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          InvGeoTransform()                           */
/*                                                                      */
/*      Invert a standard 3x2 "GeoTransform" style matrix with an       */
/*      implicit [1 0 0] final row.                                     */
/************************************************************************/

static int InvGeoTransform( double *gt_in, double *gt_out )

{
    double	det, inv_det;

    /* we assume a 3rd row that is [1 0 0] */

    /* Compute determinate */

    det = gt_in[1] * gt_in[5] - gt_in[2] * gt_in[4];

    if( fabs(det) < 0.000000000000001 )
        return 0;

    inv_det = 1.0 / det;

    /* compute adjoint, and devide by determinate */

    gt_out[1] =  gt_in[5] * inv_det;
    gt_out[4] = -gt_in[4] * inv_det;

    gt_out[2] = -gt_in[2] * inv_det;
    gt_out[5] =  gt_in[1] * inv_det;

    gt_out[0] = ( gt_in[2] * gt_in[3] - gt_in[0] * gt_in[5]) * inv_det;
    gt_out[3] = (-gt_in[1] * gt_in[3] + gt_in[0] * gt_in[4]) * inv_det;

    return 1;
}

/************************************************************************/
/*                      GDALSuggestedWarpOutput()                       */
/************************************************************************/

CPLErr GDALSuggestedWarpOutput( GDALDatasetH hSrcDS, 
                                GDALTransformerFunc pfnTransformer, 
                                void *pTransformArg, 
                                double *padfGeoTransformOut, 
                                int *pnPixels, int *pnLines )

{
/* -------------------------------------------------------------------- */
/*      Setup sample points all around the edge of the input raster.    */
/* -------------------------------------------------------------------- */
    int    nSamplePoints=0, abSuccess[80];
    double adfX[80], adfY[80], adfZ[80], dfRatio;
    int    nInXSize = GDALGetRasterXSize( hSrcDS );
    int    nInYSize = GDALGetRasterYSize( hSrcDS );

    // Take 20 steps 
    for( dfRatio = 0.0; dfRatio <= 1.01; dfRatio += 0.05 )
    {
        
        // Ensure we end exactly at the end.
        if( dfRatio > 0.99 )
            dfRatio = 1.0;

        // Along top 
        adfX[nSamplePoints]   = dfRatio * nInXSize;
        adfY[nSamplePoints]   = 0.0;
        adfZ[nSamplePoints++] = 0.0;

        // Along bottom 
        adfX[nSamplePoints]   = dfRatio * nInXSize;
        adfY[nSamplePoints]   = nInYSize;
        adfZ[nSamplePoints++] = 0.0;

        // Along left
        adfX[nSamplePoints]   = 0.0;
        adfY[nSamplePoints] = dfRatio * nInYSize;
        adfZ[nSamplePoints++] = 0.0;

        // Along right
        adfX[nSamplePoints]   = nInXSize;
        adfY[nSamplePoints] = dfRatio * nInYSize;
        adfZ[nSamplePoints++] = 0.0;
    }

    CPLAssert( nSamplePoints == 80 );

/* -------------------------------------------------------------------- */
/*      Transform them to the output coordinate system.                 */
/* -------------------------------------------------------------------- */
    if( !pfnTransformer( pTransformArg, FALSE, nSamplePoints, 
                         adfX, adfY, adfZ, abSuccess ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "GDALSuggestedWarpOutput() failed because the passed\n"
                  "transformer failed." );
        return CE_Failure;
    }
        
/* -------------------------------------------------------------------- */
/*      Collect the bounds, ignoring any failed points.                 */
/* -------------------------------------------------------------------- */
    double dfMinXOut, dfMinYOut, dfMaxXOut, dfMaxYOut;
    int    bGotInitialPoint = FALSE;
    int    nFailedCount = 0, i;

    for( i = 0; i < nSamplePoints; i++ )
    {
        if( !abSuccess[i] )
        {
            nFailedCount++;
            continue;
        }

        if( !bGotInitialPoint )
        {
            bGotInitialPoint = TRUE;
            dfMinXOut = dfMaxXOut = adfX[i];
            dfMinYOut = dfMaxYOut = adfY[i];
        }
        else
        {
            dfMinXOut = MIN(dfMinXOut,adfX[i]);
            dfMinYOut = MIN(dfMinYOut,adfY[i]);
            dfMaxXOut = MAX(dfMaxXOut,adfX[i]);
            dfMaxYOut = MAX(dfMaxYOut,adfY[i]);
        }
    }

    if( nFailedCount > nSamplePoints - 10 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Too many points (%d out of %d) failed to transform,\n"
                  "unable to compute output bounds.",
                  nFailedCount, nSamplePoints );
        return CE_Failure;
    }

    if( nFailedCount > 0 )
        CPLDebug( "GDAL", 
                  "GDALSuggestedWarpOutput(): %d out of %d points failed to transform.", 
                  nFailedCount, nSamplePoints );

/* -------------------------------------------------------------------- */
/*      Compute the distance in "georeferenced" units from the top      */
/*      corner of the transformed input image to the bottom left        */
/*      corner of the transformed input.  Use this distance to          */
/*      compute an approximate pixel size in the output                 */
/*      georeferenced coordinates.                                      */
/* -------------------------------------------------------------------- */
    double dfDiagonalDist, dfDeltaX, dfDeltaY;
    
    dfDeltaX = adfX[nSamplePoints-1] - adfX[0];
    dfDeltaY = adfY[nSamplePoints-1] - adfY[0];

    dfDiagonalDist = sqrt( dfDeltaX * dfDeltaX + dfDeltaY * dfDeltaY );
    
/* -------------------------------------------------------------------- */
/*      Compute a pixel size from this.                                 */
/* -------------------------------------------------------------------- */
    double dfPixelSize;

    dfPixelSize = dfDiagonalDist 
        / sqrt(((double)nInXSize)*nInXSize + ((double)nInYSize)*nInYSize);

    *pnPixels = (int) ((dfMaxXOut - dfMinXOut) / dfPixelSize) + 1;
    *pnLines = (int) ((dfMaxYOut - dfMinYOut) / dfPixelSize) + 1;

/* -------------------------------------------------------------------- */
/*      Set the output geotransform.                                    */
/* -------------------------------------------------------------------- */
    padfGeoTransformOut[0] = dfMinXOut;
    padfGeoTransformOut[1] = dfPixelSize;
    padfGeoTransformOut[2] = 0.0;
    padfGeoTransformOut[3] = dfMaxYOut;
    padfGeoTransformOut[4] = 0.0;
    padfGeoTransformOut[5] = - dfPixelSize;
    
    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*			 GDALGenImgProjTransformer                      */
/* ==================================================================== */
/************************************************************************/

typedef struct {

    double   adfSrcGeoTransform[6];
    double   adfSrcInvGeoTransform[6];

    void     *pReprojectArg;

    double   adfDstGeoTransform[6];
    double   adfDstInvGeoTransform[6];
    
} GDALGenImgProjTransformInfo;

/************************************************************************/
/*                  GDALCreateGenImgProjTransformer()                   */
/************************************************************************/

void *
GDALCreateGenImgProjTransformer( GDALDataset *poSrcDS, const char *pszSrcWKT,
                                 GDALDataset *poDstDS, const char *pszDstWKT,
                                 int bGCPUseOK, double dfGCPErrorThreshold )

{
    GDALGenImgProjTransformInfo *psInfo;

/* -------------------------------------------------------------------- */
/*      Initialize the transform info.                                  */
/* -------------------------------------------------------------------- */
    psInfo = (GDALGenImgProjTransformInfo *) 
        CPLCalloc(sizeof(GDALGenImgProjTransformInfo),1);

/* -------------------------------------------------------------------- */
/*      Get forward and inverse geotransform for the source image.      */
/* -------------------------------------------------------------------- */
    poSrcDS->GetGeoTransform( psInfo->adfSrcGeoTransform );
    InvGeoTransform( psInfo->adfSrcGeoTransform, 
                     psInfo->adfSrcInvGeoTransform );

/* -------------------------------------------------------------------- */
/*      Setup reprojection.                                             */
/* -------------------------------------------------------------------- */
    if( pszSrcWKT != NULL && pszDstWKT != NULL
        && !EQUAL(pszSrcWKT,pszDstWKT) )
    {
        psInfo->pReprojectArg = 
            GDALCreateReprojectionTransformer( pszSrcWKT, pszDstWKT );
    }
        
/* -------------------------------------------------------------------- */
/*      Get forward and inverse geotransform for destination image.     */
/*      If we have no destination use a unit transform.                 */
/* -------------------------------------------------------------------- */
    if( poDstDS )
    {
        poDstDS->GetGeoTransform( psInfo->adfDstGeoTransform );
        InvGeoTransform( psInfo->adfDstGeoTransform, 
                         psInfo->adfDstInvGeoTransform );
    }
    else
    {
        psInfo->adfDstGeoTransform[0] = 0.0;
        psInfo->adfDstGeoTransform[0] = 1.0;
        psInfo->adfDstGeoTransform[0] = 0.0;
        psInfo->adfDstGeoTransform[0] = 0.0;
        psInfo->adfDstGeoTransform[0] = 0.0;
        psInfo->adfDstGeoTransform[0] = 1.0;
        memcpy( psInfo->adfDstInvGeoTransform, psInfo->adfDstGeoTransform,
                sizeof(double) * 6 );
    }
    
    return psInfo;
}
    

/************************************************************************/
/*                  GDALDestroyGenImgProjTransformer()                  */
/************************************************************************/

void GDALDestroyGenImgProjTransformer( void *hTransformArg )

{
    GDALGenImgProjTransformInfo *psInfo = 
        (GDALGenImgProjTransformInfo *) hTransformArg;

    if( psInfo->pReprojectArg != NULL )
        GDALDestroyReprojectionTransformer( psInfo->pReprojectArg );

    CPLFree( psInfo );
}

/************************************************************************/
/*                      GDALGenImgProjTransform()                       */
/************************************************************************/

int GDALGenImgProjTransform( void *pTransformArg, int bDstToSrc, 
                             int nPointCount, 
                             double *padfX, double *padfY, double *padfZ,
                             int *panSuccess )
{
    GDALGenImgProjTransformInfo *psInfo = 
        (GDALGenImgProjTransformInfo *) pTransformArg;
    int   i;
    double *padfGeoTransform;

/* -------------------------------------------------------------------- */
/*      Convert from src (dst) pixel/line to src (dst)                  */
/*      georeferenced coordinates.                                      */
/* -------------------------------------------------------------------- */
    if( bDstToSrc )
        padfGeoTransform = psInfo->adfDstGeoTransform;
    else
        padfGeoTransform = psInfo->adfSrcGeoTransform;

    for( i = 0; i < nPointCount; i++ )
    {
        double dfNewX, dfNewY;
        
        dfNewX = padfGeoTransform[0]
            + padfX[i] * padfGeoTransform[1]
            + padfY[i] * padfGeoTransform[2];
        dfNewY = padfGeoTransform[3]
            + padfX[i] * padfGeoTransform[4]
            + padfY[i] * padfGeoTransform[5];
        
        padfX[i] = dfNewX;
        padfY[i] = dfNewY;
    }

/* -------------------------------------------------------------------- */
/*      Reproject if needed.                                            */
/* -------------------------------------------------------------------- */
    if( psInfo->pReprojectArg )
    {
        if( !GDALReprojectionTransform( psInfo->pReprojectArg, bDstToSrc, 
                                        nPointCount, padfX, padfY, padfZ,
                                        panSuccess ) )
            return FALSE;
    }
    else
    {
        for( i = 0; i < nPointCount; i++ )
            panSuccess[i] = 1;
    }

/* -------------------------------------------------------------------- */
/*      Convert dst (src) georef coordinates back to pixel/line.        */
/* -------------------------------------------------------------------- */
    if( bDstToSrc )
        padfGeoTransform = psInfo->adfSrcInvGeoTransform;
    else
        padfGeoTransform = psInfo->adfDstInvGeoTransform;

    for( i = 0; i < nPointCount; i++ )
    {
        double dfNewX, dfNewY;
        
        dfNewX = padfGeoTransform[0]
            + padfX[i] * padfGeoTransform[1]
            + padfY[i] * padfGeoTransform[2];
        dfNewY = padfGeoTransform[3]
            + padfX[i] * padfGeoTransform[4]
            + padfY[i] * padfGeoTransform[5];
        
        padfX[i] = dfNewX;
        padfY[i] = dfNewY;
    }

    return TRUE;
}


/************************************************************************/
/* ==================================================================== */
/*			 GDALReprojectionTransformer                    */
/* ==================================================================== */
/************************************************************************/

typedef struct {
    OGRCoordinateTransformation *poForwardTransform;
    OGRCoordinateTransformation *poReverseTransform;
} GDALReprojectionTransformInfo;

/************************************************************************/
/*                 GDALCreateReprojectionTransformer()                  */
/************************************************************************/

void *GDALCreateReprojectionTransformer( const char *pszSrcWKT, 
                                         const char *pszDstWKT )

{
    OGRSpatialReference oSrcSRS, oDstSRS;
    OGRCoordinateTransformation *poForwardTransform;

/* -------------------------------------------------------------------- */
/*      Ingest the SRS definitions.                                     */
/* -------------------------------------------------------------------- */
    if( oSrcSRS.importFromWkt( (char **) &pszSrcWKT ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to import coordinate system `%s'.", 
                  pszSrcWKT );
        return NULL;
    }
    if( oDstSRS.importFromWkt( (char **) &pszDstWKT ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to import coordinate system `%s'.", 
                  pszSrcWKT );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Build the forward coordinate transformation.                    */
/* -------------------------------------------------------------------- */
    poForwardTransform = OGRCreateCoordinateTransformation(&oSrcSRS,&oDstSRS);

    if( poForwardTransform == NULL )
        // OGRCreateCoordinateTransformation() will report errors on its own.
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a structure to hold the transform info, and also         */
/*      build reverse transform.  We assume that if the forward         */
/*      transform can be created, then so can the reverse one.          */
/* -------------------------------------------------------------------- */
    GDALReprojectionTransformInfo *psInfo;

    psInfo = (GDALReprojectionTransformInfo *) 
        CPLCalloc(sizeof(GDALReprojectionTransformInfo),1);

    psInfo->poForwardTransform = poForwardTransform;
    psInfo->poReverseTransform = 
        OGRCreateCoordinateTransformation(&oDstSRS,&oSrcSRS);

    return psInfo;
}

/************************************************************************/
/*                  GDALDestroyReprojectionTransform()                  */
/************************************************************************/

void GDALDestroyReprojectionTransform( void *pTransformAlg )

{
    GDALReprojectionTransformInfo *psInfo = 
        (GDALReprojectionTransformInfo *) pTransformAlg;		

    if( psInfo->poForwardTransform )
        delete psInfo->poForwardTransform;

    if( psInfo->poReverseTransform )
    delete psInfo->poReverseTransform;

    CPLFree( psInfo );
}

/************************************************************************/
/*                     GDALReprojectionTransform()                      */
/************************************************************************/

int GDALReprojectionTransform( void *pTransformArg, int bDstToSrc, 
                                int nPointCount, 
                                double *padfX, double *padfY, double *padfZ,
                                int *panSuccess )

{
    GDALReprojectionTransformInfo *psInfo = 
        (GDALReprojectionTransformInfo *) pTransformArg;		
    int bSuccess;

    if( bDstToSrc )
        bSuccess = psInfo->poReverseTransform->Transform( 
            nPointCount, padfX, padfY, padfZ );
    else
        bSuccess = psInfo->poForwardTransform->Transform( 
            nPointCount, padfX, padfY, padfZ );

    if( bSuccess )
        memset( panSuccess, 1, sizeof(int) * nPointCount );
    else
        memset( panSuccess, 0, sizeof(int) * nPointCount );

    return bSuccess;
}

