/******************************************************************************
 * $Id$
 *
 * Project:  GDAL DEM Utilities
 * Purpose:  
 * Author:   Matthew Perry, perrygeo at gmail.com
 *           Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2006, 2009 Matthew Perry 
 * Copyright (c) 2009 Even Rouault
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


#include <stdlib.h>
#include <math.h>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal.h"


CPL_CVSID("$Id$");

#ifndef M_PI
# define M_PI  3.1415926535897932384626433832795
#endif


/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( " Usage: \n"
            " - To generate a shaded relief map from any GDAL-supported elevation raster : \n\n"
            "     gdaldem hillshade input_dem output_hillshade \n"
            "                 [-z ZFactor (default=1)] [-s scale* (default=1)] \n"
            "                 [-az Azimuth (default=315)] [-alt Altitude (default=45)]\n"
            "                 [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-quiet]\n"
            "\n"
            " - To generates a slope map from any GDAL-supported elevation raster :\n\n"
            "     gdaldem slope input_dem output_slope_map \n"
            "                 [-p use percent slope (default=degrees)] [-s scale* (default=1)]\n"
            "                 [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-quiet]\n"
            "\n"
            " - To generate an aspect map from any GDAL-supported elevation raster\n"
            "   Outputs a 32-bit float tiff with pixel values from 0-360 indicating azimuth :\n\n"
            "     gdaldem aspect input_dem output_aspect_map \n"
            "                 [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-quiet]\n"
            "\n"
            " - To generate a color relief map from any GDAL-supported elevation raster\n"
            "     gdaldem color-relief input_dem color_text_file output_color_relief_map\n"
            "                 [-alpha]\n"
            "                 [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-quiet]\n"
            "     where color_text_file contains lines of the format \"elevation_value red green blue\"\n"
            "\n"
            " Notes : \n"
            "   Scale is the ratio of vertical units to horizontal\n"
            "    for Feet:Latlong use scale=370400, for Meters:LatLong use scale=111120 \n\n");
    exit( 1 );
}

/************************************************************************/
/*                         GDALHillshade()                              */
/************************************************************************/

CPLErr GDALHillshade  ( GDALRasterBandH hSrcBand,
                        GDALRasterBandH hDstBand,
                        double* adfGeoTransform,
                        double z,
                        double scale,
                        double alt,
                        double az,
                        GDALProgressFunc pfnProgress,
                        void * pProgressData)
{
    CPLErr eErr;
    int bContainsNull;
    const double degreesToRadians = M_PI / 180.0;
    float *pafThreeLineWin; /* 3 line rotating source buffer */
    float *pafShadeBuf;     /* 1 line destination buffer */
    int i;

    double x, y, aspect, slope, cang;
    const double altRadians = alt * degreesToRadians;
    const double azRadians = az * degreesToRadians;

    int bSrcHasNoData;
    double dfSrcNoDataValue = 0.0;

    int nXSize = GDALGetRasterBandXSize(hSrcBand);
    int nYSize = GDALGetRasterBandYSize(hSrcBand);

    double   nsres = adfGeoTransform[5];
    double   ewres = adfGeoTransform[1];

    if (pfnProgress == NULL)
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Initialize progress counter.                                    */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

    pafShadeBuf = (float *) CPLMalloc(sizeof(float)*nXSize);
    pafThreeLineWin  = (float *) CPLMalloc(3*sizeof(float)*nXSize);

    dfSrcNoDataValue = GDALGetRasterNoDataValue(hSrcBand, &bSrcHasNoData);

    // Move a 3x3 pafWindow over each cell 
    // (where the cell in question is #4)
    // 
    //      0 1 2
    //      3 4 5
    //      6 7 8

    /* Preload the first 2 lines */
    for ( i = 0; i < 2 && i < nYSize; i++)
    {
        GDALRasterIO(   hSrcBand,
                        GF_Read,
                        0, i,
                        nXSize, 1,
                        pafThreeLineWin + i * nXSize,
                        nXSize, 1,
                        GDT_Float32,
                        0, 0);
    }

    for ( i = 0; i < nYSize; i++)
    {
        // Exclude the edges
        if (i == 0 || i == nYSize-1)
        {
            memset(pafShadeBuf, 0, sizeof(float)*nXSize);
        }
        else
        {
            /* Read third line of the line buffer */
            GDALRasterIO(   hSrcBand,
                            GF_Read,
                            0, i+1,
                            nXSize, 1,
                            pafThreeLineWin + ((i+1) % 3) * nXSize,
                            nXSize, 1,
                            GDT_Float32,
                            0, 0);

            int nLine1Off = (i-1) % 3;
            int nLine2Off = (i) % 3;
            int nLine3Off = (i+1) % 3;

            for (int j = 0; j < nXSize; j++)
            {
                bContainsNull = FALSE;

                // Exclude the edges
                if ( j == 0 || j == nXSize-1 )
                {
                    // We are at the edge so write nullValue and move on
                    pafShadeBuf[j] = 0;
                    continue;
                }

                float afWin[9];
                afWin[0] = pafThreeLineWin[nLine1Off*nXSize + j-1];
                afWin[1] = pafThreeLineWin[nLine1Off*nXSize + j];
                afWin[2] = pafThreeLineWin[nLine1Off*nXSize + j+1];
                afWin[3] = pafThreeLineWin[nLine2Off*nXSize + j-1];
                afWin[4] = pafThreeLineWin[nLine2Off*nXSize + j];
                afWin[5] = pafThreeLineWin[nLine2Off*nXSize + j+1];
                afWin[6] = pafThreeLineWin[nLine3Off*nXSize + j-1];
                afWin[7] = pafThreeLineWin[nLine3Off*nXSize + j];
                afWin[8] = pafThreeLineWin[nLine3Off*nXSize + j+1];

                // Check if afWin has null value
                if (bSrcHasNoData)
                {
                    for ( int n = 0; n <= 8; n++)
                    {
                        if(afWin[n] == dfSrcNoDataValue)
                        {
                            bContainsNull = TRUE;
                            break;
                        }
                    }
                }

                if (bContainsNull) {
                    // We have nulls so write nullValue and move on
                    pafShadeBuf[j] = 0;
                    continue;
                } else {
                    // We have a valid 3x3 window.

                    /* ---------------------------------------
                    * Compute Hillshade
                    */

                    // First Slope ...
                    x = z*((afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
                        (afWin[2] + afWin[5] + afWin[5] + afWin[8])) /
                        (8.0 * ewres * scale);

                    y = z*((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
                        (afWin[0] + afWin[1] + afWin[1] + afWin[2])) /
                        (8.0 * nsres * scale);

                    slope = M_PI / 2 - atan(sqrt(x*x + y*y));

                    // ... then aspect...
                    aspect = atan2(x,y);

                    // ... then the shade value
                    cang = sin(altRadians) * sin(slope) +
                           cos(altRadians) * cos(slope) *
                           cos(azRadians-M_PI/2 - aspect);

                    if (cang <= 0.0) 
                        cang = 1.0;
                    else
                        cang = 1.0 + (254.0 * cang);

                    pafShadeBuf[j] = cang;
                }
            }
        }

        /* -----------------------------------------
         * Write Line to Raster
         */
        GDALRasterIO(hDstBand,
                           GF_Write,
                           0, i, nXSize,
                           1, pafShadeBuf, nXSize, 1, GDT_Float32, 0, 0);

        if( !pfnProgress( 1.0 * (i+1) / nYSize, NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
            goto end;
        }
    }

    pfnProgress( 1.0, NULL, pProgressData );
    eErr = CE_None;

end:
    CPLFree(pafShadeBuf);
    CPLFree(pafThreeLineWin);

    return eErr;
}


/************************************************************************/
/*                         GDALSlope()                                  */
/************************************************************************/

CPLErr GDALSlope  ( GDALRasterBandH hSrcBand,
                    GDALRasterBandH hDstBand,
                    double* adfGeoTransform,
                    double scale,
                    int slopeFormat,
                    GDALProgressFunc pfnProgress,
                    void * pProgressData)
{
    CPLErr eErr;
    const double radiansToDegrees = 180.0 / M_PI;
    int bContainsNull;
    float *pafThreeLineWin; /* 3 line rotating source buffer */
    float *pafSlopeBuf;     /* 1 line destination buffer */
    int i;

    double dx, dy, key;

    int bSrcHasNoData;
    double dfSrcNoDataValue = 0.0, dfDstNoDataValue;

    int nXSize = GDALGetRasterBandXSize(hSrcBand);
    int nYSize = GDALGetRasterBandYSize(hSrcBand);

    double   nsres = adfGeoTransform[5];
    double   ewres = adfGeoTransform[1];

    if (pfnProgress == NULL)
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Initialize progress counter.                                    */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

    pafSlopeBuf = (float *) CPLMalloc(sizeof(float)*nXSize);
    pafThreeLineWin  = (float *) CPLMalloc(3*sizeof(float)*nXSize);

    dfSrcNoDataValue = GDALGetRasterNoDataValue(hSrcBand, &bSrcHasNoData);
    dfDstNoDataValue = GDALGetRasterNoDataValue(hDstBand, NULL);

    // Move a 3x3 pafWindow over each cell 
    // (where the cell in question is #4)
    // 
    //      0 1 2
    //      3 4 5
    //      6 7 8

    /* Preload the first 2 lines */
    for ( i = 0; i < 2 && i < nYSize; i++)
    {
        GDALRasterIO(   hSrcBand,
                        GF_Read,
                        0, i,
                        nXSize, 1,
                        pafThreeLineWin + i * nXSize,
                        nXSize, 1,
                        GDT_Float32,
                        0, 0);
    }

    for ( i = 0; i < nYSize; i++)
    {
        // Exclude the edges
        if (i == 0 || i == nYSize-1)
        {
            for (int j = 0; j < nXSize; j++)
            {
                pafSlopeBuf[j] = dfDstNoDataValue;
            }
        }
        else
        {
            /* Read third line of the line buffer */
            GDALRasterIO(   hSrcBand,
                            GF_Read,
                            0, i+1,
                            nXSize, 1,
                            pafThreeLineWin + ((i+1) % 3) * nXSize,
                            nXSize, 1,
                            GDT_Float32,
                            0, 0);

            int nLine1Off = (i-1) % 3;
            int nLine2Off = (i) % 3;
            int nLine3Off = (i+1) % 3;

            for (int j = 0; j < nXSize; j++)
            {
                bContainsNull = FALSE;

                // Exclude the edges
                if ( j == 0 || j == nXSize-1 )
                {
                    // We are at the edge so write nullValue and move on
                    pafSlopeBuf[j] = dfDstNoDataValue;
                    continue;
                }

                float afWin[9];
                afWin[0] = pafThreeLineWin[nLine1Off*nXSize + j-1];
                afWin[1] = pafThreeLineWin[nLine1Off*nXSize + j];
                afWin[2] = pafThreeLineWin[nLine1Off*nXSize + j+1];
                afWin[3] = pafThreeLineWin[nLine2Off*nXSize + j-1];
                afWin[4] = pafThreeLineWin[nLine2Off*nXSize + j];
                afWin[5] = pafThreeLineWin[nLine2Off*nXSize + j+1];
                afWin[6] = pafThreeLineWin[nLine3Off*nXSize + j-1];
                afWin[7] = pafThreeLineWin[nLine3Off*nXSize + j];
                afWin[8] = pafThreeLineWin[nLine3Off*nXSize + j+1];

                // Check if afWin has null value
                if (bSrcHasNoData)
                {
                    for ( int n = 0; n <= 8; n++)
                    {
                        if(afWin[n] == dfSrcNoDataValue)
                        {
                            bContainsNull = TRUE;
                            break;
                        }
                    }
                }

                if (bContainsNull) {
                    // We have nulls so write nullValue and move on
                    pafSlopeBuf[j] = dfDstNoDataValue;
                    continue;
                } else {
                    // We have a valid 3x3 window.

                    dx = ((afWin[0] + afWin[3] + afWin[3] + afWin[6]) - 
                          (afWin[2] + afWin[5] + afWin[5] + afWin[8]))/(8*ewres*scale);

                    dy = ((afWin[6] + afWin[7] + afWin[7] + afWin[8]) - 
                          (afWin[0] + afWin[1] + afWin[1] + afWin[2]))/(8*nsres*scale);

                    key = dx * dx + dy * dy;

                    if (slopeFormat == 1) 
                        pafSlopeBuf[j] = atan(sqrt(key)) * radiansToDegrees;
                    else
                        pafSlopeBuf[j] = 100*sqrt(key);
                }
            }
        }

        /* -----------------------------------------
         * Write Line to Raster
         */
        GDALRasterIO(hDstBand,
                           GF_Write,
                           0, i, nXSize,
                           1, pafSlopeBuf, nXSize, 1, GDT_Float32, 0, 0);

        if( !pfnProgress( 1.0 * (i+1) / nYSize, NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
            goto end;
        }
    }

    pfnProgress( 1.0, NULL, pProgressData );
    eErr = CE_None;

end:
    CPLFree(pafSlopeBuf);
    CPLFree(pafThreeLineWin);

    return eErr;
}

/************************************************************************/
/*                         GDALAspect()                                 */
/************************************************************************/

CPLErr GDALAspect  (GDALRasterBandH hSrcBand,
                    GDALRasterBandH hDstBand,
                    GDALProgressFunc pfnProgress,
                    void * pProgressData)
{
    CPLErr eErr;
    const double degreesToRadians = M_PI / 180.0;
    int bContainsNull;
    float *pafThreeLineWin; /* 3 line rotating source buffer */
    float *pafAspectBuf;     /* 1 line destination buffer */
    int i;
    
    float aspect;
    double dx, dy;

    int bSrcHasNoData;
    double dfSrcNoDataValue = 0.0, dfDstNoDataValue;

    int nXSize = GDALGetRasterBandXSize(hSrcBand);
    int nYSize = GDALGetRasterBandYSize(hSrcBand);

    if (pfnProgress == NULL)
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Initialize progress counter.                                    */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

    pafAspectBuf = (float *) CPLMalloc(sizeof(float)*nXSize);
    pafThreeLineWin  = (float *) CPLMalloc(3*sizeof(float)*nXSize);

    dfSrcNoDataValue = GDALGetRasterNoDataValue(hSrcBand, &bSrcHasNoData);
    dfDstNoDataValue = GDALGetRasterNoDataValue(hDstBand, NULL);

    // Move a 3x3 pafWindow over each cell 
    // (where the cell in question is #4)
    // 
    //      0 1 2
    //      3 4 5
    //      6 7 8

    /* Preload the first 2 lines */
    for ( i = 0; i < 2 && i < nYSize; i++)
    {
        GDALRasterIO(   hSrcBand,
                        GF_Read,
                        0, i,
                        nXSize, 1,
                        pafThreeLineWin + i * nXSize,
                        nXSize, 1,
                        GDT_Float32,
                        0, 0);
    }

    for ( i = 0; i < nYSize; i++)
    {
        // Exclude the edges
        if (i == 0 || i == nYSize-1)
        {
            for (int j = 0; j < nXSize; j++)
            {
                pafAspectBuf[j] = dfDstNoDataValue;
            }
        }
        else
        {
            /* Read third line of the line buffer */
            GDALRasterIO(   hSrcBand,
                            GF_Read,
                            0, i+1,
                            nXSize, 1,
                            pafThreeLineWin + ((i+1) % 3) * nXSize,
                            nXSize, 1,
                            GDT_Float32,
                            0, 0);

            int nLine1Off = (i-1) % 3;
            int nLine2Off = (i) % 3;
            int nLine3Off = (i+1) % 3;

            for (int j = 0; j < nXSize; j++)
            {
                bContainsNull = FALSE;

                // Exclude the edges
                if ( j == 0 || j == nXSize-1 )
                {
                    // We are at the edge so write nullValue and move on
                    pafAspectBuf[j] = dfDstNoDataValue;
                    continue;
                }

                float afWin[9];
                afWin[0] = pafThreeLineWin[nLine1Off*nXSize + j-1];
                afWin[1] = pafThreeLineWin[nLine1Off*nXSize + j];
                afWin[2] = pafThreeLineWin[nLine1Off*nXSize + j+1];
                afWin[3] = pafThreeLineWin[nLine2Off*nXSize + j-1];
                afWin[4] = pafThreeLineWin[nLine2Off*nXSize + j];
                afWin[5] = pafThreeLineWin[nLine2Off*nXSize + j+1];
                afWin[6] = pafThreeLineWin[nLine3Off*nXSize + j-1];
                afWin[7] = pafThreeLineWin[nLine3Off*nXSize + j];
                afWin[8] = pafThreeLineWin[nLine3Off*nXSize + j+1];

                // Check if afWin has null value
                if (bSrcHasNoData)
                {
                    for ( int n = 0; n <= 8; n++)
                    {
                        if(afWin[n] == dfSrcNoDataValue)
                        {
                            bContainsNull = TRUE;
                            break;
                        }
                    }
                }

                if (bContainsNull) {
                    // We have nulls so write nullValue and move on
                    pafAspectBuf[j] = dfDstNoDataValue;
                    continue;
                } else {
                    // We have a valid 3x3 window.

                    dx = ((afWin[2] + afWin[5] + afWin[5] + afWin[8]) -
                          (afWin[0] + afWin[3] + afWin[3] + afWin[6]));

                    dy = ((afWin[6] + afWin[7] + afWin[7] + afWin[8]) - 
                          (afWin[0] + afWin[1] + afWin[1] + afWin[2]));

                    aspect = atan2(dy/8.0,-1.0*dx/8.0) / degreesToRadians;

                    if (dx == 0)
                    {
                        if (dy > 0) 
                            aspect = 0.0;
                        else if (dy < 0)
                            aspect = 180.0;
                        else
                            aspect = dfDstNoDataValue;
                    } 
                    else 
                    {
                        if (aspect > 90.0) 
                            aspect = 450.0 - aspect;
                        else
                            aspect = 90.0 - aspect;
                    }

                    if (aspect == 360.0) 
                        aspect = 0.0;
       
                    pafAspectBuf[j] = aspect;

                }
            }
        }

        /* -----------------------------------------
         * Write Line to Raster
         */
        GDALRasterIO(hDstBand,
                           GF_Write,
                           0, i, nXSize,
                           1, pafAspectBuf, nXSize, 1, GDT_Float32, 0, 0);

        if( !pfnProgress( 1.0 * (i+1) / nYSize, NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
            goto end;
        }
    }
    pfnProgress( 1.0, NULL, pProgressData );
    eErr = CE_None;

end:
    CPLFree(pafAspectBuf);
    CPLFree(pafThreeLineWin);

    return eErr;
}

/************************************************************************/
/*                      GDALColorRelief()                               */
/************************************************************************/

typedef struct
{
    double dfVal;
    int nR;
    int nG;
    int nB;
    int nA;
} ColorAssociation;

static int GDALColorReliefSortColors(const void* pA, const void* pB)
{
    ColorAssociation* pC1 = (ColorAssociation*)pA;
    ColorAssociation* pC2 = (ColorAssociation*)pB;
    return (pC1->dfVal < pC2->dfVal) ? -1 :
           (pC1->dfVal == pC2->dfVal) ? 0 : 1;
}

static void GDALColorReliefInterpolateVal(ColorAssociation* pasColorAssociation,
                                          int nColorAssociation,
                                          double dfVal,
                                          int* pnR,
                                          int* pnG,
                                          int* pnB,
                                          int* pnA)
{
    int i;

    if (dfVal <= pasColorAssociation[0].dfVal)
    {
        *pnR = pasColorAssociation[0].nR;
        *pnG = pasColorAssociation[0].nG;
        *pnB = pasColorAssociation[0].nB;
        *pnA = pasColorAssociation[0].nA;
        return;
    }

    for(i=1;i<nColorAssociation;i++)
    {
        if (dfVal > pasColorAssociation[i-1].dfVal &&
            dfVal < pasColorAssociation[i].dfVal)
        {
            double dfRatio = (dfVal - pasColorAssociation[i-1].dfVal) /
               (pasColorAssociation[i].dfVal - pasColorAssociation[i-1].dfVal);
            *pnR = (int)(0.45 + pasColorAssociation[i-1].nR + dfRatio *
                    (pasColorAssociation[i].nR - pasColorAssociation[i-1].nR));
            if (*pnR < 0) *pnR = 0;
            else if (*pnR > 255) *pnR = 255;
            *pnG = (int)(0.45 + pasColorAssociation[i-1].nG + dfRatio *
                    (pasColorAssociation[i].nG - pasColorAssociation[i-1].nG));
            if (*pnG < 0) *pnG = 0;
            else if (*pnG > 255) *pnG = 255;
            *pnB = (int)(0.45 + pasColorAssociation[i-1].nB + dfRatio *
                    (pasColorAssociation[i].nB - pasColorAssociation[i-1].nB));
            if (*pnB < 0) *pnB = 0;
            else if (*pnB > 255) *pnB = 255;
            *pnA = (int)(0.45 + pasColorAssociation[i-1].nA + dfRatio *
                    (pasColorAssociation[i].nA - pasColorAssociation[i-1].nA));
            if (*pnA < 0) *pnA = 0;
            else if (*pnA > 255) *pnA = 255;

            return;
        }
        else if (i == nColorAssociation - 1 ||
                 dfVal == pasColorAssociation[i].dfVal)
        {
            *pnR = pasColorAssociation[i].nR;
            *pnG = pasColorAssociation[i].nG;
            *pnB = pasColorAssociation[i].nB;
            *pnA = pasColorAssociation[i].nA;
            return;
        }
    }
}

ColorAssociation* GDALColorReliefParseColorFile(GDALRasterBandH hSrcBand,
                                                const char* pszColorFilename,
                                                int* pnColors)
{
    FILE* fpColorFile = VSIFOpen(pszColorFilename, "rt");
    if (fpColorFile == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s", pszColorFilename);
        *pnColors = 0;
        return NULL;
    }

    ColorAssociation* pasColorAssociation = NULL;
    int nColorAssociation = 0;

    int bSrcHasNoData = FALSE;
    double dfSrcNoDataValue = GDALGetRasterNoDataValue(hSrcBand, &bSrcHasNoData);

    const char* pszLine;
    while ((pszLine = CPLReadLine(fpColorFile)) != NULL)
    {
        char** papszFields = CSLTokenizeStringComplex(pszLine, " ,\t", 
                                                      FALSE, FALSE );
        /* Skip comment lines */
        if (CSLCount(papszFields) >= 4 &&
            papszFields[0][0] != '#' &&
            papszFields[0][0] != '/')
        {
            pasColorAssociation =
                    (ColorAssociation*)CPLRealloc(pasColorAssociation,
                           (nColorAssociation + 1) * sizeof(ColorAssociation));
            if (EQUAL(papszFields[0], "nodata") && bSrcHasNoData)
                pasColorAssociation[nColorAssociation].dfVal = dfSrcNoDataValue;
            else if (EQUAL(papszFields[0], "min"))
            {
                int bSuccess;
                double dfVal = GDALGetRasterMinimum(hSrcBand, &bSuccess);
                if (!bSuccess)
                {
                    double dfMin, dfMax, dfMean, dfStdDev;
                    fprintf(stderr, "Computing source raster statistics...\n");
                    GDALComputeRasterStatistics(hSrcBand, FALSE, &dfMin, &dfMax,
                                                &dfMean, &dfStdDev, NULL, NULL);
                    dfVal = dfMin;
                }
                pasColorAssociation[nColorAssociation].dfVal = dfVal;
            }
            else if (EQUAL(papszFields[0], "max"))
            {
                int bSuccess;
                double dfVal = GDALGetRasterMaximum(hSrcBand, &bSuccess);
                if (!bSuccess)
                {
                    double dfMin, dfMax, dfMean, dfStdDev;
                    fprintf(stderr, "Computing source raster statistics...\n");
                    GDALComputeRasterStatistics(hSrcBand, FALSE, &dfMin, &dfMax,
                                                &dfMean, &dfStdDev, NULL, NULL);
                    dfVal = dfMax;
                }
                pasColorAssociation[nColorAssociation].dfVal = dfVal;
            }
            else
                pasColorAssociation[nColorAssociation].dfVal = atof(papszFields[0]);
            pasColorAssociation[nColorAssociation].nR = atoi(papszFields[1]);
            pasColorAssociation[nColorAssociation].nG = atoi(papszFields[2]);
            pasColorAssociation[nColorAssociation].nB = atoi(papszFields[3]);
            pasColorAssociation[nColorAssociation].nA =
                    (CSLCount(papszFields) >= 5 ) ? atoi(papszFields[4]) : 255;
            nColorAssociation ++;
        }
        CSLDestroy(papszFields);
    }
    VSIFClose(fpColorFile);

    if (nColorAssociation == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No color association found in %s", pszColorFilename);
        *pnColors = 0;
        return NULL;
    }

    qsort(pasColorAssociation, nColorAssociation,
          sizeof(ColorAssociation), GDALColorReliefSortColors);

    *pnColors = nColorAssociation;
    return pasColorAssociation;
}

CPLErr GDALColorRelief (GDALRasterBandH hSrcBand,
                        GDALRasterBandH hDstBand1,
                        GDALRasterBandH hDstBand2,
                        GDALRasterBandH hDstBand3,
                        GDALRasterBandH hDstBand4,
                        const char* pszColorFilename,
                        GDALProgressFunc pfnProgress,
                        void * pProgressData)
{
    CPLErr eErr;

    int nColorAssociation = 0;
    ColorAssociation* pasColorAssociation =
            GDALColorReliefParseColorFile(hSrcBand, pszColorFilename,
                                          &nColorAssociation);
    if (pasColorAssociation == NULL)
        return CE_Failure;

    int nXSize = GDALGetRasterBandXSize(hSrcBand);
    int nYSize = GDALGetRasterBandYSize(hSrcBand);

    if (pfnProgress == NULL)
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Initialize progress counter.                                    */
/* -------------------------------------------------------------------- */

    float* pafSourceBuf = (float *) CPLMalloc(sizeof(float)*nXSize);
    GByte* pabyDestBuf1  = (GByte*) CPLMalloc(nXSize);
    GByte* pabyDestBuf2  = (GByte*) CPLMalloc(nXSize);
    GByte* pabyDestBuf3  = (GByte*) CPLMalloc(nXSize);
    GByte* pabyDestBuf4  = (GByte*) CPLMalloc(nXSize);
    int nR = 0, nG = 0, nB = 0, nA = 0;

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        eErr = CE_Failure;
        goto end;
    }

    for ( int i = 0; i < nYSize; i++)
    {
        /* Read source buffer */
        GDALRasterIO(   hSrcBand,
                        GF_Read,
                        0, i,
                        nXSize, 1,
                        pafSourceBuf,
                        nXSize, 1,
                        GDT_Float32,
                        0, 0);

        for (int j = 0; j < nXSize; j++)
        {
            GDALColorReliefInterpolateVal  (pasColorAssociation,
                                            nColorAssociation,
                                            pafSourceBuf[j],
                                            &nR,
                                            &nG,
                                            &nB,
                                            &nA);
            pabyDestBuf1[j] = nR;
            pabyDestBuf2[j] = nG;
            pabyDestBuf3[j] = nB;
            pabyDestBuf4[j] = nA;
        }

        /* -----------------------------------------
         * Write Line to Raster
         */
        GDALRasterIO(hDstBand1,
                      GF_Write,
                      0, i, nXSize,
                      1, pabyDestBuf1, nXSize, 1, GDT_Byte, 0, 0);
        GDALRasterIO(hDstBand2,
                      GF_Write,
                      0, i, nXSize,
                      1, pabyDestBuf2, nXSize, 1, GDT_Byte, 0, 0);
        GDALRasterIO(hDstBand3,
                      GF_Write,
                      0, i, nXSize,
                      1, pabyDestBuf3, nXSize, 1, GDT_Byte, 0, 0);
        if (hDstBand4)
            GDALRasterIO(hDstBand4,
                        GF_Write,
                        0, i, nXSize,
                        1, pabyDestBuf4, nXSize, 1, GDT_Byte, 0, 0);

        if( !pfnProgress( 1.0 * (i+1) / nYSize, NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
            goto end;
        }
    }
    pfnProgress( 1.0, NULL, pProgressData );
    eErr = CE_None;

end:
    CPLFree(pafSourceBuf);
    CPLFree(pabyDestBuf1);
    CPLFree(pabyDestBuf2);
    CPLFree(pabyDestBuf3);
    CPLFree(pabyDestBuf4);
    CPLFree(pasColorAssociation);

    return eErr;
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/


enum
{
    HILL_SHADE,
    SLOPE,
    ASPECT,
    COLOR_RELIEF
};


int main( int argc, char ** argv )

{
    int eUtilityMode;
    double z = 1.0;
    double scale = 1.0;
    double az = 315.0;
    double alt = 45.0;
    // 0 = 'percent' or 1 = 'degrees'
    int slopeFormat = 1; 
    int bAddAlpha = FALSE;
    
    int nBand = 1;
    double  adfGeoTransform[6];

    const char *pszSrcFilename = NULL;
    const char *pszDstFilename = NULL;
    const char *pszColorFilename = NULL;
    const char *pszFormat = "GTiff";
    char **papszCreateOptions = NULL;
    
    GDALDatasetH hSrcDataset = NULL;
    GDALDatasetH hDstDataset = NULL;
    GDALRasterBandH hSrcBand = NULL;
    GDALRasterBandH hDstBand = NULL;
    GDALDriverH hDriver = NULL;

    GDALProgressFunc pfnProgress = GDALTermProgress;
    
    int nXSize = 0;
    int nYSize = 0;
    
    /* Check that we are running against at least GDAL 1.4 */
    /* Note to developers : if we use newer API, please change the requirement */
    if (atoi(GDALVersionInfo("VERSION_NUM")) < 1400)
    {
        fprintf(stderr, "At least, GDAL >= 1.4.0 is required for this version of %s, "
                "which was compiled against GDAL %s\n", argv[0], GDAL_RELEASE_NAME);
        exit(1);
    }

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 2 )
    {
        fprintf(stderr, "Not enough arguments\n");
        Usage();
        exit( 1 );
    }

    if( EQUAL(argv[1], "--utility_version") || EQUAL(argv[1], "--utility-version") )
    {
        printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
        return 0;
    }
    else if ( EQUAL(argv[1], "shade") || EQUAL(argv[1], "hillshade") )
    {
        eUtilityMode = HILL_SHADE;
    }
    else if ( EQUAL(argv[1], "slope") )
    {
        eUtilityMode = SLOPE;
    }
    else if ( EQUAL(argv[1], "aspect") )
    {
        eUtilityMode = ASPECT;
    }
    else if ( EQUAL(argv[1], "color-relief") )
    {
        eUtilityMode = COLOR_RELIEF;
    }
    else
    {
        fprintf(stderr, "Missing valid sub-utility mention\n");
        Usage();
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for(int i = 2; i < argc; i++ )
    {
        if( eUtilityMode == HILL_SHADE && i + 1 < argc &&
            (EQUAL(argv[i], "--z") || EQUAL(argv[i], "-z")))
        {
            z = atof(argv[++i]);
        }
        else if ( eUtilityMode == SLOPE && EQUAL(argv[i], "-p"))
        {
            slopeFormat = 0;
        }
        else if( i + 1 < argc &&
            (EQUAL(argv[i], "--s") || 
             EQUAL(argv[i], "-s") ||
             EQUAL(argv[i], "--scale") ||
             EQUAL(argv[i], "-scale"))
          )
        {
            scale = atof(argv[++i]);
        }
        else if( eUtilityMode == HILL_SHADE && i + 1 < argc &&
            (EQUAL(argv[i], "--az") || 
             EQUAL(argv[i], "-az") ||
             EQUAL(argv[i], "--azimuth") ||
             EQUAL(argv[i], "-azimuth"))
          )
        {
            az = atof(argv[++i]);
        }
        else if( eUtilityMode == HILL_SHADE && i + 1 < argc &&
            (EQUAL(argv[i], "--alt") || 
             EQUAL(argv[i], "-alt") ||
             EQUAL(argv[i], "--alt") ||
             EQUAL(argv[i], "-alt"))
          )
        {
            alt = atof(argv[++i]);
        }
        else if( eUtilityMode == COLOR_RELIEF &&
                 EQUAL(argv[i], "-alpha"))
        {
            bAddAlpha = TRUE;
        }
        else if( i + 1 < argc &&
            (EQUAL(argv[i], "--b") || 
             EQUAL(argv[i], "-b"))
          )
        {
            nBand = atoi(argv[++i]);
        }
        else if ( EQUAL(argv[i], "-quiet") )
        {
            pfnProgress = GDALDummyProgress;
        }
        else if( EQUAL(argv[i],"-co") && i < argc-1 )
        {
            papszCreateOptions = CSLAddString( papszCreateOptions, argv[++i] );
        }
        else if( EQUAL(argv[i],"-of") && i < argc-1 )
        {
            pszFormat = argv[++i];
        }
        else if( argv[i][0] == '-' )
        {
            fprintf( stderr, "Option %s incomplete, or not recognised.\n\n", 
                    argv[i] );
            Usage();
            GDALDestroyDriverManager();
            exit( 2 );
        }
        else if( pszSrcFilename == NULL )
        {
            pszSrcFilename = argv[i];
        }
        else if( eUtilityMode == COLOR_RELIEF && pszColorFilename == NULL )
        {
            pszColorFilename = argv[i];
        }
        else if( pszDstFilename == NULL )
        {
            pszDstFilename = argv[i];
        }
        else
            Usage();
    }

    if( pszSrcFilename == NULL )
    {
        fprintf( stderr, "Missing source.\n\n" );
        Usage();
    }
    if ( eUtilityMode == COLOR_RELIEF && pszColorFilename == NULL )
    {
        fprintf( stderr, "Missing color file.\n\n" );
        Usage();
    }
    if( pszDstFilename == NULL )
    {
        fprintf( stderr, "Missing destination.\n\n" );
        Usage();
    }

    GDALAllRegister();

    // Open Dataset and get raster band
    hSrcDataset = GDALOpen( pszSrcFilename, GA_ReadOnly );
    
    if( hSrcDataset == NULL )
    {
        fprintf( stderr,
                 "GDALOpen failed - %d\n%s\n",
                 CPLGetLastErrorNo(), CPLGetLastErrorMsg() );
        GDALDestroyDriverManager();
        exit( 1 );
    }

    nXSize = GDALGetRasterXSize(hSrcDataset);
    nYSize = GDALGetRasterYSize(hSrcDataset);

    if( nBand <= 0 || nBand > GDALGetRasterCount(hSrcDataset) )
    {
        fprintf( stderr,
                 "Unable to fetch band #%d\n", nBand );
        GDALDestroyDriverManager();
        exit( 1 );
    }
    hSrcBand = GDALGetRasterBand( hSrcDataset, nBand );

    GDALGetGeoTransform(hSrcDataset, adfGeoTransform);
    hDriver = GDALGetDriverByName(pszFormat);
    if( hDriver == NULL 
        || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) == NULL )
    {
        int	iDr;

        fprintf( stderr, "Output driver `%s' not recognised or does not support\n", 
                 pszFormat );
        fprintf( stderr, "direct output file creation.  The following format drivers are configured\n"
                "and support direct output:\n" );

        for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
        {
            GDALDriverH hDriver = GDALGetDriver(iDr);

            if( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL) != NULL )
            {
                printf( "  %s: %s\n",
                        GDALGetDriverShortName( hDriver  ),
                        GDALGetDriverLongName( hDriver ) );
            }
        }
        GDALDestroyDriverManager();
        exit( 1 );
    }

    int nDstBands;
    if (eUtilityMode == COLOR_RELIEF)
        nDstBands = (bAddAlpha) ? 4 : 3;
    else
        nDstBands = 1;

    hDstDataset = GDALCreate(   hDriver,
                                pszDstFilename,
                                nXSize,
                                nYSize,
                                nDstBands,
                                (eUtilityMode == HILL_SHADE ||
                                 eUtilityMode == COLOR_RELIEF) ? GDT_Byte :
                                                               GDT_Float32,
                                papszCreateOptions);

    if( hDstDataset == NULL )
    {
        fprintf( stderr,
                 "Unable to create dataset %s %d\n%s\n", pszDstFilename,
                 CPLGetLastErrorNo(), CPLGetLastErrorMsg() );
        GDALDestroyDriverManager();
        exit( 1 );
    }
    
    hDstBand = GDALGetRasterBand( hDstDataset, 1 );

    GDALSetGeoTransform(hDstDataset, adfGeoTransform);
    GDALSetProjection(hDstDataset, GDALGetProjectionRef(hSrcDataset));
    
    if (eUtilityMode == HILL_SHADE)
    {
        GDALSetRasterNoDataValue(hDstBand, 0);

        GDALHillshade(  hSrcBand, 
                        hDstBand, 
                        adfGeoTransform,
                        z,
                        scale,
                        alt,
                        az,
                        pfnProgress, NULL);
    }
    else if (eUtilityMode == SLOPE)
    {
        GDALSetRasterNoDataValue(hDstBand, -9999);

        GDALSlope(  hSrcBand, 
                    hDstBand, 
                    adfGeoTransform,
                    scale,
                    slopeFormat,
                    pfnProgress, NULL);
    }

    else if (eUtilityMode == ASPECT)
    {
        GDALSetRasterNoDataValue(hDstBand, -9999.0);

        GDALAspect( hSrcBand, 
                    hDstBand, 
                    pfnProgress, NULL);
    }
    else if (eUtilityMode == COLOR_RELIEF)
    {
        GDALColorRelief (hSrcBand, 
                         GDALGetRasterBand(hDstDataset, 1),
                         GDALGetRasterBand(hDstDataset, 2),
                         GDALGetRasterBand(hDstDataset, 3),
                         (bAddAlpha) ? GDALGetRasterBand(hDstDataset, 4) : NULL,
                         pszColorFilename,
                         pfnProgress, NULL);
    }

    GDALClose(hSrcDataset);
    GDALClose(hDstDataset);

    GDALDestroyDriverManager();
    CSLDestroy( argv );
    CSLDestroy( papszCreateOptions );

    return 0;
}

