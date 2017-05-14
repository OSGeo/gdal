/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements Geolocation array based transformer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "gdal_alg.h"

#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$");

CPL_C_START
CPLXMLNode *GDALSerializeGeoLocTransformer( void *pTransformArg );
void *GDALDeserializeGeoLocTransformer( CPLXMLNode *psTree );
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                         GDALGeoLocTransformer                        */
/* ==================================================================== */
/************************************************************************/

typedef struct {
    GDALTransformerInfo sTI;

    bool        bReversed;

    // Map from target georef coordinates back to geolocation array
    // pixel line coordinates.  Built only if needed.
    int         nBackMapWidth;
    int         nBackMapHeight;
    double      adfBackMapGeoTransform[6];  // Maps georef to pixel/line.
    float       *pafBackMapX;
    float       *pafBackMapY;

    // Geolocation bands.
    GDALDatasetH     hDS_X;
    GDALRasterBandH  hBand_X;
    GDALDatasetH     hDS_Y;
    GDALRasterBandH  hBand_Y;

    // Located geolocation data.
    int              nGeoLocXSize;
    int              nGeoLocYSize;
    double           *padfGeoLocX;
    double           *padfGeoLocY;

    int              bHasNoData;
    double           dfNoDataX;

    // Geolocation <-> base image mapping.
    double           dfPIXEL_OFFSET;
    double           dfPIXEL_STEP;
    double           dfLINE_OFFSET;
    double           dfLINE_STEP;

    char **          papszGeolocationInfo;

} GDALGeoLocTransformInfo;

/************************************************************************/
/*                         GeoLocLoadFullData()                         */
/************************************************************************/

static bool GeoLocLoadFullData( GDALGeoLocTransformInfo *psTransform )

{
    const int nXSize_XBand = GDALGetRasterXSize( psTransform->hDS_X );
    const int nYSize_XBand = GDALGetRasterYSize( psTransform->hDS_X );
    const int nXSize_YBand = GDALGetRasterXSize( psTransform->hDS_Y );
    const int nYSize_YBand = GDALGetRasterYSize( psTransform->hDS_Y );

    const int nXSize = nXSize_XBand;
    // TODO(schwehr): This could use an explanation.
    int nYSize = 0;
    if( nYSize_XBand == 1 && nYSize_YBand == 1 )
    {
        nYSize = nXSize_YBand;
    }
    else
    {
        nYSize = nYSize_XBand;
    }

    psTransform->nGeoLocXSize = nXSize;
    psTransform->nGeoLocYSize = nYSize;

    psTransform->padfGeoLocY = static_cast<double *>(
        VSI_MALLOC3_VERBOSE(sizeof(double), nXSize, nYSize));
    psTransform->padfGeoLocX = static_cast<double *>(
        VSI_MALLOC3_VERBOSE(sizeof(double), nXSize, nYSize));

    if( psTransform->padfGeoLocX == NULL ||
        psTransform->padfGeoLocY == NULL )
    {
        return false;
    }

    if( nYSize_XBand == 1 && nYSize_YBand == 1 )
    {
        // Case of regular grid.
        // The XBAND contains the x coordinates for all lines.
        // The YBAND contains the y coordinates for all columns.

        double* padfTempX = static_cast<double *>(
            VSI_MALLOC2_VERBOSE(nXSize, sizeof(double)));
        double* padfTempY = static_cast<double *>(
            VSI_MALLOC2_VERBOSE(nYSize, sizeof(double)));
        if( padfTempX == NULL || padfTempY == NULL )
        {
            CPLFree(padfTempX);
            CPLFree(padfTempY);
            return false;
        }

        CPLErr eErr =
            GDALRasterIO( psTransform->hBand_X, GF_Read,
                          0, 0, nXSize, 1,
                          padfTempX, nXSize, 1,
                          GDT_Float64, 0, 0 );

        for( int j = 0; j < nYSize; j++ )
        {
            memcpy( psTransform->padfGeoLocX + j * nXSize,
                    padfTempX,
                    nXSize * sizeof(double) );
        }

        if( eErr == CE_None )
        {
            eErr = GDALRasterIO( psTransform->hBand_Y, GF_Read,
                                 0, 0, nYSize, 1,
                                 padfTempY, nYSize, 1,
                                 GDT_Float64, 0, 0 );

            for( int j = 0; j < nYSize; j++ )
            {
                for( int i = 0; i < nXSize; i++ )
                {
                    psTransform->padfGeoLocY[j * nXSize + i] = padfTempY[j];
                }
            }
        }

        CPLFree(padfTempX);
        CPLFree(padfTempY);

        if( eErr != CE_None )
            return false;
    }
    else
    {
        if( GDALRasterIO( psTransform->hBand_X, GF_Read,
                          0, 0, nXSize, nYSize,
                          psTransform->padfGeoLocX, nXSize, nYSize,
                          GDT_Float64, 0, 0 ) != CE_None
            || GDALRasterIO( psTransform->hBand_Y, GF_Read,
                             0, 0, nXSize, nYSize,
                             psTransform->padfGeoLocY, nXSize, nYSize,
                             GDT_Float64, 0, 0 ) != CE_None )
            return false;
    }

    psTransform->dfNoDataX =
        GDALGetRasterNoDataValue( psTransform->hBand_X,
                                  &(psTransform->bHasNoData) );

    return true;
}

/************************************************************************/
/*                       GeoLocGenerateBackMap()                        */
/************************************************************************/

static bool GeoLocGenerateBackMap( GDALGeoLocTransformInfo *psTransform )

{
    const int nXSize = psTransform->nGeoLocXSize;
    const int nYSize = psTransform->nGeoLocYSize;
    const int nMaxIter = 3;

/* -------------------------------------------------------------------- */
/*      Scan forward map for lat/long extents.                          */
/* -------------------------------------------------------------------- */
    double dfMinX = 0.0;
    double dfMaxX = 0.0;
    double dfMinY = 0.0;
    double dfMaxY = 0.0;
    bool bInit = false;

    for( int i = nXSize * nYSize - 1; i >= 0; i-- )
    {
        if( !psTransform->bHasNoData ||
            psTransform->padfGeoLocX[i] != psTransform->dfNoDataX )
        {
            if( bInit )
            {
                dfMinX = std::min(dfMinX, psTransform->padfGeoLocX[i]);
                dfMaxX = std::max(dfMaxX, psTransform->padfGeoLocX[i]);
                dfMinY = std::min(dfMinY, psTransform->padfGeoLocY[i]);
                dfMaxY = std::max(dfMaxY, psTransform->padfGeoLocY[i]);
            }
            else
            {
                bInit = true;
                dfMinX = psTransform->padfGeoLocX[i];
                dfMaxX = psTransform->padfGeoLocX[i];
                dfMinY = psTransform->padfGeoLocY[i];
                dfMaxY = psTransform->padfGeoLocY[i];
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Decide on resolution for backmap.  We aim for slightly          */
/*      higher resolution than the source but we can't easily           */
/*      establish how much dead space there is in the backmap, so it    */
/*      is approximate.                                                 */
/* -------------------------------------------------------------------- */
    const double dfTargetPixels = (nXSize * nYSize * 1.3);
    const double dfPixelSize = sqrt((dfMaxX - dfMinX) * (dfMaxY - dfMinY)
                              / dfTargetPixels);

    const int nBMYSize = psTransform->nBackMapHeight =
        static_cast<int>((dfMaxY - dfMinY) / dfPixelSize + 1);
    const int nBMXSize = psTransform->nBackMapWidth =
        static_cast<int>((dfMaxX - dfMinX) / dfPixelSize + 1);

    if( nBMXSize > INT_MAX / nBMYSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Int overflow : %d x %d",
                 nBMXSize, nBMYSize);
        return false;
    }

    dfMinX -= dfPixelSize / 2.0;
    dfMaxY += dfPixelSize / 2.0;

    psTransform->adfBackMapGeoTransform[0] = dfMinX;
    psTransform->adfBackMapGeoTransform[1] = dfPixelSize;
    psTransform->adfBackMapGeoTransform[2] = 0.0;
    psTransform->adfBackMapGeoTransform[3] = dfMaxY;
    psTransform->adfBackMapGeoTransform[4] = 0.0;
    psTransform->adfBackMapGeoTransform[5] = -dfPixelSize;

/* -------------------------------------------------------------------- */
/*      Allocate backmap, and initialize to nodata value (-1.0).        */
/* -------------------------------------------------------------------- */
    GByte *pabyValidFlag = static_cast<GByte *>(
        VSI_CALLOC_VERBOSE(nBMXSize, nBMYSize));

    psTransform->pafBackMapX = static_cast<float *>(
        VSI_MALLOC3_VERBOSE(nBMXSize, nBMYSize, sizeof(float)));
    psTransform->pafBackMapY = static_cast<float *>(
        VSI_MALLOC3_VERBOSE(nBMXSize, nBMYSize, sizeof(float)));

    if( pabyValidFlag == NULL ||
        psTransform->pafBackMapX == NULL ||
        psTransform->pafBackMapY == NULL )
    {
        CPLFree( pabyValidFlag );
        return false;
    }

    for( int i = nBMXSize * nBMYSize - 1; i >= 0; i-- )
    {
        psTransform->pafBackMapX[i] = -1.0;
        psTransform->pafBackMapY[i] = -1.0;
    }

/* -------------------------------------------------------------------- */
/*      Run through the whole geoloc array forward projecting and       */
/*      pushing into the backmap.                                       */
/*      Initialize to the nMaxIter+1 value so we can spot genuinely     */
/*      valid pixels in the hole-filling loop.                          */
/* -------------------------------------------------------------------- */

    for( int iY = 0; iY < nYSize; iY++ )
    {
        for( int iX = 0; iX < nXSize; iX++ )
        {
            if( psTransform->bHasNoData &&
                psTransform->padfGeoLocX[iX + iY * nXSize]
                == psTransform->dfNoDataX )
                continue;

            const int i = iX + iY * nXSize;

            const int iBMX = static_cast<int>(
                (psTransform->padfGeoLocX[i] - dfMinX) / dfPixelSize);
            const int iBMY = static_cast<int>(
                (dfMaxY - psTransform->padfGeoLocY[i]) / dfPixelSize);

            if( iBMX < 0 || iBMY < 0 || iBMX >= nBMXSize || iBMY >= nBMYSize )
                continue;

            psTransform->pafBackMapX[iBMX + iBMY * nBMXSize] =
                static_cast<float>(
                    iX * psTransform->dfPIXEL_STEP +
                    psTransform->dfPIXEL_OFFSET);
            psTransform->pafBackMapY[iBMX + iBMY * nBMXSize] =
                static_cast<float>(
                    iY * psTransform->dfLINE_STEP +
                    psTransform->dfLINE_OFFSET);

            pabyValidFlag[iBMX + iBMY * nBMXSize] =
                static_cast<GByte>(nMaxIter+1);
        }
    }

/* -------------------------------------------------------------------- */
/*      Now, loop over the backmap trying to fill in holes with         */
/*      nearby values.                                                  */
/* -------------------------------------------------------------------- */
    for( int iIter = 0; iIter < nMaxIter; iIter++ )
    {
        int nNumValid = 0;
        for( int iBMY = 0; iBMY < nBMYSize; iBMY++ )
        {
            for( int iBMX = 0; iBMX < nBMXSize; iBMX++ )
            {
                // If this point is already set, ignore it.
                if( pabyValidFlag[iBMX + iBMY*nBMXSize] )
                {
                    nNumValid++;
                    continue;
                }

                int nCount = 0;
                double dfXSum = 0.0;
                double dfYSum = 0.0;
                const int nMarkedAsGood = nMaxIter - iIter;

                // Left?
                if( iBMX > 0 &&
                    pabyValidFlag[iBMX-1+iBMY*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum += psTransform->pafBackMapX[iBMX-1+iBMY*nBMXSize];
                    dfYSum += psTransform->pafBackMapY[iBMX-1+iBMY*nBMXSize];
                    nCount++;
                }
                // Right?
                if( iBMX + 1 < nBMXSize &&
                    pabyValidFlag[iBMX+1+iBMY*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum += psTransform->pafBackMapX[iBMX+1+iBMY*nBMXSize];
                    dfYSum += psTransform->pafBackMapY[iBMX+1+iBMY*nBMXSize];
                    nCount++;
                }
                // Top?
                if( iBMY > 0 &&
                    pabyValidFlag[iBMX+(iBMY-1)*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum += psTransform->pafBackMapX[iBMX+(iBMY-1)*nBMXSize];
                    dfYSum += psTransform->pafBackMapY[iBMX+(iBMY-1)*nBMXSize];
                    nCount++;
                }
                // Bottom?
                if( iBMY + 1 < nBMYSize &&
                    pabyValidFlag[iBMX+(iBMY+1)*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum += psTransform->pafBackMapX[iBMX+(iBMY+1)*nBMXSize];
                    dfYSum += psTransform->pafBackMapY[iBMX+(iBMY+1)*nBMXSize];
                    nCount++;
                }
                // Top-left?
                if( iBMX > 0 && iBMY > 0 &&
                    pabyValidFlag[iBMX-1+(iBMY-1)*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum +=
                        psTransform->pafBackMapX[iBMX-1+(iBMY-1)*nBMXSize];
                    dfYSum +=
                        psTransform->pafBackMapY[iBMX-1+(iBMY-1)*nBMXSize];
                    nCount++;
                }
                // Top-right?
                if( iBMX + 1 < nBMXSize && iBMY > 0 &&
                    pabyValidFlag[iBMX+1+(iBMY-1)*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum +=
                        psTransform->pafBackMapX[iBMX+1+(iBMY-1)*nBMXSize];
                    dfYSum +=
                        psTransform->pafBackMapY[iBMX+1+(iBMY-1)*nBMXSize];
                    nCount++;
                }
                // Bottom-left?
                if( iBMX > 0 && iBMY + 1 < nBMYSize &&
                    pabyValidFlag[iBMX-1+(iBMY+1)*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum +=
                        psTransform->pafBackMapX[iBMX-1+(iBMY+1)*nBMXSize];
                    dfYSum +=
                        psTransform->pafBackMapY[iBMX-1+(iBMY+1)*nBMXSize];
                    nCount++;
                }
                // Bottom-right?
                if( iBMX + 1 < nBMXSize && iBMY + 1 < nBMYSize &&
                    pabyValidFlag[iBMX+1+(iBMY+1)*nBMXSize] > nMarkedAsGood )
                {
                    dfXSum +=
                        psTransform->pafBackMapX[iBMX+1+(iBMY+1)*nBMXSize];
                    dfYSum +=
                        psTransform->pafBackMapY[iBMX+1+(iBMY+1)*nBMXSize];
                    nCount++;
                }

                if( nCount > 0 )
                {
                    psTransform->pafBackMapX[iBMX + iBMY * nBMXSize] =
                        static_cast<float>(dfXSum/nCount);
                    psTransform->pafBackMapY[iBMX + iBMY * nBMXSize] =
                        static_cast<float>(dfYSum/nCount);
                    // Genuinely valid points will have value iMaxIter + 1.
                    // On each iteration mark newly valid points with a
                    // descending value so that it will not be used on the
                    // current iteration only on subsequent ones.
                    pabyValidFlag[iBMX+iBMY*nBMXSize] =
                        static_cast<GByte>(nMaxIter - iIter);
                }
            }
        }
        if( nNumValid == nBMXSize * nBMYSize )
            break;
    }

    CPLFree( pabyValidFlag );

    return true;
}

/************************************************************************/
/*                       GDALGeoLocRescale()                            */
/************************************************************************/

static void GDALGeoLocRescale( char**& papszMD, const char* pszItem,
                               double dfRatio, double dfDefaultVal )
{
    const double dfVal =
        dfRatio *
        CPLAtofM(CSLFetchNameValueDef(papszMD, pszItem,
                                      CPLSPrintf("%.18g", dfDefaultVal)));

    papszMD = CSLSetNameValue(papszMD, pszItem, CPLSPrintf("%.18g", dfVal));
}

/************************************************************************/
/*                 GDALCreateSimilarGeoLocTransformer()                 */
/************************************************************************/

static
void* GDALCreateSimilarGeoLocTransformer( void *hTransformArg,
                                          double dfRatioX, double dfRatioY )
{
    VALIDATE_POINTER1(hTransformArg, "GDALCreateSimilarGeoLocTransformer",
                      NULL);

    GDALGeoLocTransformInfo *psInfo =
        static_cast<GDALGeoLocTransformInfo *>(hTransformArg);

    char** papszGeolocationInfo = CSLDuplicate(psInfo->papszGeolocationInfo);

    if( dfRatioX != 1.0 || dfRatioY != 1.0 )
    {
        GDALGeoLocRescale(papszGeolocationInfo, "PIXEL_OFFSET", dfRatioX, 0.0);
        GDALGeoLocRescale(papszGeolocationInfo, "LINE_OFFSET", dfRatioY, 0.0);
        GDALGeoLocRescale(
            papszGeolocationInfo, "PIXEL_STEP", 1.0 / dfRatioX, 1.0);
        GDALGeoLocRescale(papszGeolocationInfo,
            "LINE_STEP", 1.0 / dfRatioY, 1.0);
    }

    psInfo = static_cast<GDALGeoLocTransformInfo*>(
        GDALCreateGeoLocTransformer(
            NULL, papszGeolocationInfo, psInfo->bReversed));

    CSLDestroy(papszGeolocationInfo);

    return psInfo;
}

/************************************************************************/
/*                    GDALCreateGeoLocTransformer()                     */
/************************************************************************/

/** Create GeoLocation transformer */
void *GDALCreateGeoLocTransformer( GDALDatasetH hBaseDS,
                                   char **papszGeolocationInfo,
                                   int bReversed )

{
    if( CSLFetchNameValue(papszGeolocationInfo, "PIXEL_OFFSET") == NULL
        || CSLFetchNameValue(papszGeolocationInfo, "LINE_OFFSET") == NULL
        || CSLFetchNameValue(papszGeolocationInfo, "PIXEL_STEP") == NULL
        || CSLFetchNameValue(papszGeolocationInfo, "LINE_STEP") == NULL
        || CSLFetchNameValue(papszGeolocationInfo, "X_BAND") == NULL
        || CSLFetchNameValue(papszGeolocationInfo, "Y_BAND") == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Missing some geolocation fields in "
                  "GDALCreateGeoLocTransformer()" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize core info.                                           */
/* -------------------------------------------------------------------- */
    GDALGeoLocTransformInfo *psTransform =
        static_cast<GDALGeoLocTransformInfo *>(
            CPLCalloc(sizeof(GDALGeoLocTransformInfo), 1));

    psTransform->bReversed = CPL_TO_BOOL(bReversed);

    memcpy( psTransform->sTI.abySignature,
            GDAL_GTI2_SIGNATURE,
            strlen(GDAL_GTI2_SIGNATURE) );
    psTransform->sTI.pszClassName = "GDALGeoLocTransformer";
    psTransform->sTI.pfnTransform = GDALGeoLocTransform;
    psTransform->sTI.pfnCleanup = GDALDestroyGeoLocTransformer;
    psTransform->sTI.pfnSerialize = GDALSerializeGeoLocTransformer;
    psTransform->sTI.pfnCreateSimilar = GDALCreateSimilarGeoLocTransformer;

    psTransform->papszGeolocationInfo = CSLDuplicate( papszGeolocationInfo );

/* -------------------------------------------------------------------- */
/*      Pull geolocation info from the options/metadata.                */
/* -------------------------------------------------------------------- */
    psTransform->dfPIXEL_OFFSET =
        CPLAtof(CSLFetchNameValue( papszGeolocationInfo, "PIXEL_OFFSET" ));
    psTransform->dfLINE_OFFSET =
        CPLAtof(CSLFetchNameValue( papszGeolocationInfo, "LINE_OFFSET" ));
    psTransform->dfPIXEL_STEP =
        CPLAtof(CSLFetchNameValue( papszGeolocationInfo, "PIXEL_STEP" ));
    psTransform->dfLINE_STEP =
        CPLAtof(CSLFetchNameValue( papszGeolocationInfo, "LINE_STEP" ));

/* -------------------------------------------------------------------- */
/*      Establish access to geolocation dataset(s).                     */
/* -------------------------------------------------------------------- */
    const char *pszDSName = CSLFetchNameValue( papszGeolocationInfo,
                                               "X_DATASET" );
    if( pszDSName != NULL )
    {
        CPLConfigOptionSetter oSetter("CPL_ALLOW_VSISTDIN", "NO", true);
        psTransform->hDS_X = GDALOpenShared( pszDSName, GA_ReadOnly );
    }
    else
    {
        psTransform->hDS_X = hBaseDS;
        if( hBaseDS )
        {
            GDALReferenceDataset( psTransform->hDS_X );
            psTransform->papszGeolocationInfo =
                CSLSetNameValue( psTransform->papszGeolocationInfo,
                                 "X_DATASET",
                                 GDALGetDescription( hBaseDS ) );
        }
    }

    pszDSName = CSLFetchNameValue( papszGeolocationInfo, "Y_DATASET" );
    if( pszDSName != NULL )
    {
        CPLConfigOptionSetter oSetter("CPL_ALLOW_VSISTDIN", "NO", true);
        psTransform->hDS_Y = GDALOpenShared( pszDSName, GA_ReadOnly );
    }
    else
    {
        psTransform->hDS_Y = hBaseDS;
        if( hBaseDS )
        {
            GDALReferenceDataset( psTransform->hDS_Y );
            psTransform->papszGeolocationInfo =
                CSLSetNameValue( psTransform->papszGeolocationInfo,
                                 "Y_DATASET",
                                 GDALGetDescription( hBaseDS ) );
        }
    }

    if( psTransform->hDS_X == NULL ||
        psTransform->hDS_Y == NULL )
    {
        GDALDestroyGeoLocTransformer( psTransform );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get the band handles.                                           */
/* -------------------------------------------------------------------- */
    const int nXBand =
        std::max(1, atoi(CSLFetchNameValue( papszGeolocationInfo, "X_BAND" )));
    psTransform->hBand_X = GDALGetRasterBand( psTransform->hDS_X, nXBand );

    const int nYBand =
        std::max(1, atoi(CSLFetchNameValue( papszGeolocationInfo, "Y_BAND" )));
    psTransform->hBand_Y = GDALGetRasterBand( psTransform->hDS_Y, nYBand );

    if( psTransform->hBand_X == NULL ||
        psTransform->hBand_Y == NULL )
    {
        GDALDestroyGeoLocTransformer( psTransform );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*     Check that X and Y bands have the same dimensions                */
/* -------------------------------------------------------------------- */
    const int nXSize_XBand = GDALGetRasterXSize( psTransform->hDS_X );
    const int nYSize_XBand = GDALGetRasterYSize( psTransform->hDS_X );
    const int nXSize_YBand = GDALGetRasterXSize( psTransform->hDS_Y );
    const int nYSize_YBand = GDALGetRasterYSize( psTransform->hDS_Y );
    if( nYSize_XBand == 1 || nYSize_YBand == 1 )
    {
        if( nYSize_XBand != 1 || nYSize_YBand != 1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "X_BAND and Y_BAND should have both nYSize == 1");
            GDALDestroyGeoLocTransformer( psTransform );
            return NULL;
        }
    }
    else if( nXSize_XBand != nXSize_YBand ||
             nYSize_XBand != nYSize_YBand )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "X_BAND and Y_BAND do not have the same dimensions");
        GDALDestroyGeoLocTransformer( psTransform );
        return NULL;
    }

    if( nXSize_XBand > INT_MAX / nYSize_XBand )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Int overflow : %d x %d",
                 nXSize_XBand, nYSize_XBand);
        GDALDestroyGeoLocTransformer( psTransform );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Load the geolocation array.                                     */
/* -------------------------------------------------------------------- */
    if( !GeoLocLoadFullData( psTransform )
        || !GeoLocGenerateBackMap( psTransform ) )
    {
        GDALDestroyGeoLocTransformer( psTransform );
        return NULL;
    }

    return psTransform;
}

/************************************************************************/
/*                    GDALDestroyGeoLocTransformer()                    */
/************************************************************************/

/** Destroy GeoLocation transformer */
void GDALDestroyGeoLocTransformer( void *pTransformAlg )

{
    if( pTransformAlg == NULL )
        return;

    GDALGeoLocTransformInfo *psTransform =
        static_cast<GDALGeoLocTransformInfo *>(pTransformAlg);

    CPLFree( psTransform->pafBackMapX );
    CPLFree( psTransform->pafBackMapY );
    CSLDestroy( psTransform->papszGeolocationInfo );
    CPLFree( psTransform->padfGeoLocX );
    CPLFree( psTransform->padfGeoLocY );

    if( psTransform->hDS_X != NULL
        && GDALDereferenceDataset( psTransform->hDS_X ) == 0 )
            GDALClose( psTransform->hDS_X );

    if( psTransform->hDS_Y != NULL
        && GDALDereferenceDataset( psTransform->hDS_Y ) == 0 )
            GDALClose( psTransform->hDS_Y );

    CPLFree( pTransformAlg );
}

/************************************************************************/
/*                        GDALGeoLocTransform()                         */
/************************************************************************/

/** Use GeoLocation transformer */
int GDALGeoLocTransform( void *pTransformArg,
                         int bDstToSrc,
                         int nPointCount,
                         double *padfX, double *padfY,
                         CPL_UNUSED double *padfZ,
                         int *panSuccess )
{
    GDALGeoLocTransformInfo *psTransform =
        static_cast<GDALGeoLocTransformInfo *>(pTransformArg);

    if( psTransform->bReversed )
        bDstToSrc = !bDstToSrc;

/* -------------------------------------------------------------------- */
/*      Do original pixel line to target geox/geoy.                     */
/* -------------------------------------------------------------------- */
    if( !bDstToSrc )
    {
        const int nXSize = psTransform->nGeoLocXSize;

        for( int i = 0; i < nPointCount; i++ )
        {
            if( padfX[i] == HUGE_VAL || padfY[i] == HUGE_VAL )
            {
                panSuccess[i] = FALSE;
                continue;
            }

            const double dfGeoLocPixel =
                (padfX[i] - psTransform->dfPIXEL_OFFSET)
                / psTransform->dfPIXEL_STEP;
            const double dfGeoLocLine =
                (padfY[i] - psTransform->dfLINE_OFFSET)
                / psTransform->dfLINE_STEP;

            int iX = std::max(0, static_cast<int>(dfGeoLocPixel));
            iX = std::min(iX, psTransform->nGeoLocXSize-1);
            int iY = std::max(0, static_cast<int>(dfGeoLocLine));
            iY = std::min(iY, psTransform->nGeoLocYSize-1);

            double *padfGLX = psTransform->padfGeoLocX + iX + iY * nXSize;
            double *padfGLY = psTransform->padfGeoLocY + iX + iY * nXSize;

            if( psTransform->bHasNoData &&
                padfGLX[0] == psTransform->dfNoDataX )
            {
                panSuccess[i] = FALSE;
                padfX[i] = HUGE_VAL;
                padfY[i] = HUGE_VAL;
                continue;
            }

            // This assumes infinite extension beyond borders of available
            // data based on closest grid square.

            if( iX + 1 < psTransform->nGeoLocXSize &&
                iY + 1 < psTransform->nGeoLocYSize &&
                (!psTransform->bHasNoData ||
                    (padfGLX[1] != psTransform->dfNoDataX &&
                     padfGLX[nXSize] != psTransform->dfNoDataX &&
                     padfGLX[nXSize + 1] != psTransform->dfNoDataX) ))
            {
                padfX[i] =
                    (1 - (dfGeoLocLine -iY))
                    * (padfGLX[0] +
                       (dfGeoLocPixel-iX) * (padfGLX[1] - padfGLX[0]))
                    + (dfGeoLocLine -iY)
                    * (padfGLX[nXSize] + (dfGeoLocPixel-iX) *
                       (padfGLX[nXSize+1] - padfGLX[nXSize]));
                padfY[i] =
                    (1 - (dfGeoLocLine -iY))
                    * (padfGLY[0] +
                       (dfGeoLocPixel-iX) * (padfGLY[1] - padfGLY[0]))
                    + (dfGeoLocLine -iY)
                    * (padfGLY[nXSize] + (dfGeoLocPixel-iX) *
                       (padfGLY[nXSize+1] - padfGLY[nXSize]));
            }
            else if( iX + 1 < psTransform->nGeoLocXSize &&
                     (!psTransform->bHasNoData ||
                        padfGLX[1] != psTransform->dfNoDataX) )
            {
                padfX[i] =
                    padfGLX[0] + (dfGeoLocPixel-iX) * (padfGLX[1] - padfGLX[0]);
                padfY[i] =
                    padfGLY[0] + (dfGeoLocPixel-iX) * (padfGLY[1] - padfGLY[0]);
            }
            else if( iY + 1 < psTransform->nGeoLocYSize &&
                     (!psTransform->bHasNoData ||
                        padfGLX[nXSize] != psTransform->dfNoDataX) )
            {
                padfX[i] = padfGLX[0]
                    + (dfGeoLocLine -iY) * (padfGLX[nXSize] - padfGLX[0]);
                padfY[i] = padfGLY[0]
                    + (dfGeoLocLine -iY) * (padfGLY[nXSize] - padfGLY[0]);
            }
            else
            {
                padfX[i] = padfGLX[0];
                padfY[i] = padfGLY[0];
            }

            panSuccess[i] = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      geox/geoy to pixel/line using backmap.                          */
/* -------------------------------------------------------------------- */
    else
    {
        for( int i = 0; i < nPointCount; i++ )
        {
            if( padfX[i] == HUGE_VAL || padfY[i] == HUGE_VAL )
            {
                panSuccess[i] = FALSE;
                continue;
            }

            const double dfBMX =
                ((padfX[i] - psTransform->adfBackMapGeoTransform[0])
                 / psTransform->adfBackMapGeoTransform[1]);
            const double dfBMY =
                ((padfY[i] - psTransform->adfBackMapGeoTransform[3])
                 / psTransform->adfBackMapGeoTransform[5]);

            const int iBMX = static_cast<int>(dfBMX);
            const int iBMY = static_cast<int>(dfBMY);

            const int iBM = iBMX + iBMY * psTransform->nBackMapWidth;

            if( iBMX < 0 || iBMY < 0
                || iBMX >= psTransform->nBackMapWidth
                || iBMY >= psTransform->nBackMapHeight
                || psTransform->pafBackMapX[iBM] < 0 )
            {
                panSuccess[i] = FALSE;
                padfX[i] = HUGE_VAL;
                padfY[i] = HUGE_VAL;
                continue;
            }

            float* pafBMX = psTransform->pafBackMapX + iBM;
            float* pafBMY = psTransform->pafBackMapY + iBM;

            if( iBMX + 1 < psTransform->nBackMapWidth &&
                iBMY + 1 < psTransform->nBackMapHeight &&
                pafBMX[1] >=0 && pafBMX[psTransform->nBackMapWidth] >= 0 &&
                pafBMX[psTransform->nBackMapWidth+1] >= 0)
            {
                padfX[i] =
                    (1-(dfBMY - iBMY))
                    * (pafBMX[0] + (dfBMX - iBMX) * (pafBMX[1] - pafBMX[0]))
                    + (dfBMY - iBMY)
                    * (pafBMX[psTransform->nBackMapWidth] +
                       (dfBMX - iBMX) * (pafBMX[psTransform->nBackMapWidth+1] -
                                         pafBMX[psTransform->nBackMapWidth]));
                padfY[i] =
                    (1-(dfBMY - iBMY))
                    * (pafBMY[0] + (dfBMX - iBMX) * (pafBMY[1] - pafBMY[0]))
                    + (dfBMY - iBMY)
                    * (pafBMY[psTransform->nBackMapWidth] +
                       (dfBMX - iBMX) * (pafBMY[psTransform->nBackMapWidth+1] -
                                         pafBMY[psTransform->nBackMapWidth]));
            }
            else if( iBMX + 1 < psTransform->nBackMapWidth &&
                     pafBMX[1] >=0)
            {
                padfX[i] = pafBMX[0] +
                            (dfBMX - iBMX) * (pafBMX[1] - pafBMX[0]);
                padfY[i] = pafBMY[0] +
                            (dfBMX - iBMX) * (pafBMY[1] - pafBMY[0]);
            }
            else if( iBMY + 1 < psTransform->nBackMapHeight &&
                     pafBMX[psTransform->nBackMapWidth] >= 0 )
            {
                padfX[i] =
                    pafBMX[0] +
                    (dfBMY - iBMY) * (pafBMX[psTransform->nBackMapWidth] -
                                      pafBMX[0]);
                padfY[i] =
                    pafBMY[0] +
                    (dfBMY - iBMY) * (pafBMY[psTransform->nBackMapWidth] -
                                      pafBMY[0]);
            }
            else
            {
                padfX[i] = pafBMX[0];
                padfY[i] = pafBMY[0];
            }
            panSuccess[i] = TRUE;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                   GDALSerializeGeoLocTransformer()                   */
/************************************************************************/

CPLXMLNode *GDALSerializeGeoLocTransformer( void *pTransformArg )

{
    VALIDATE_POINTER1( pTransformArg, "GDALSerializeGeoLocTransformer", NULL );

    GDALGeoLocTransformInfo *psInfo =
        static_cast<GDALGeoLocTransformInfo *>(pTransformArg);

    CPLXMLNode *psTree =
        CPLCreateXMLNode( NULL, CXT_Element, "GeoLocTransformer" );

/* -------------------------------------------------------------------- */
/*      Serialize bReversed.                                            */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue(
        psTree, "Reversed",
        CPLString().Printf( "%d", static_cast<int>(psInfo->bReversed) ) );

/* -------------------------------------------------------------------- */
/*      geoloc metadata.                                                */
/* -------------------------------------------------------------------- */
    char **papszMD = psInfo->papszGeolocationInfo;
    CPLXMLNode *psMD= CPLCreateXMLNode( psTree, CXT_Element, "Metadata" );

    for( int i = 0; papszMD != NULL && papszMD[i] != NULL; i++ )
    {
        char *pszKey = NULL;
        const char *pszRawValue = CPLParseNameValue( papszMD[i], &pszKey );

        CPLXMLNode *psMDI = CPLCreateXMLNode( psMD, CXT_Element, "MDI" );
        CPLSetXMLValue( psMDI, "#key", pszKey );
        CPLCreateXMLNode( psMDI, CXT_Text, pszRawValue );

        CPLFree( pszKey );
    }

    return psTree;
}

/************************************************************************/
/*                   GDALDeserializeGeoLocTransformer()                 */
/************************************************************************/

void *GDALDeserializeGeoLocTransformer( CPLXMLNode *psTree )

{
/* -------------------------------------------------------------------- */
/*      Collect metadata.                                               */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psMetadata = CPLGetXMLNode( psTree, "Metadata" );

    if( psMetadata == NULL ||
        psMetadata->eType != CXT_Element
        || !EQUAL(psMetadata->pszValue, "Metadata") )
        return NULL;

    char **papszMD = NULL;

    for( CPLXMLNode *psMDI = psMetadata->psChild;
         psMDI != NULL;
         psMDI = psMDI->psNext )
    {
        if( !EQUAL(psMDI->pszValue, "MDI")
            || psMDI->eType != CXT_Element
            || psMDI->psChild == NULL
            || psMDI->psChild->psNext == NULL
            || psMDI->psChild->eType != CXT_Attribute
            || psMDI->psChild->psChild == NULL )
            continue;

        papszMD =
            CSLSetNameValue( papszMD,
                             psMDI->psChild->psChild->pszValue,
                             psMDI->psChild->psNext->pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Get other flags.                                                */
/* -------------------------------------------------------------------- */
    const int bReversed = atoi(CPLGetXMLValue(psTree, "Reversed", "0"));

/* -------------------------------------------------------------------- */
/*      Generate transformation.                                        */
/* -------------------------------------------------------------------- */
    void *pResult = GDALCreateGeoLocTransformer( NULL, papszMD, bReversed );

/* -------------------------------------------------------------------- */
/*      Cleanup GCP copy.                                               */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszMD );

    return pResult;
}
