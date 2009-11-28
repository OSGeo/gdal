/******************************************************************************
 * $Id$
 *
 * Project:  GDAL DEM Utilities
 * Purpose:  
 * Authors:  Matthew Perry, perrygeo at gmail.com
 *           Even Rouault, even dot rouault at mines dash paris dot org
 *           Howard Butler, hobu.inc at gmail.com
 *           Chris Yesson, chris dot yesson at ioz dot ac dot uk
 *
 ******************************************************************************
 * Copyright (c) 2006, 2009 Matthew Perry 
 * Copyright (c) 2009 Even Rouault
 * Portions derived from GRASS 4.1 (public domain) See 
 * http://trac.osgeo.org/gdal/ticket/2975 for more information regarding 
 * history of this code
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
 ****************************************************************************
 *
 * Slope and aspect calculations based on original method for GRASS GIS 4.1
 * by Michael Shapiro, U.S.Army Construction Engineering Research Laboratory
 *    Olga Waupotitsch, U.S.Army Construction Engineering Research Laboratory
 *    Marjorie Larson, U.S.Army Construction Engineering Research Laboratory
 * as found in GRASS's r.slope.aspect module.
 *
 * Horn's formula is used to find the first order derivatives in x and y directions
 * for slope and aspect calculations: Horn, B. K. P. (1981).
 * "Hill Shading and the Reflectance Map", Proceedings of the IEEE, 69(1):14-47. 
 *
 * Other reference :
 * Burrough, P.A. and McDonell, R.A., 1998. Principles of Geographical Information
 * Systems. p. 190.
 *
 * Shaded relief based on original method for GRASS GIS 4.1 by Jim Westervelt,
 * U.S. Army Construction Engineering Research Laboratory
 * as found in GRASS's r.shaded.relief (formerly shade.rel.sh) module.
 * ref: "r.mapcalc: An Algebra for GIS and Image Processing",
 * by Michael Shapiro and Jim Westervelt, U.S. Army Construction Engineering
 * Research Laboratory (March/1991)
 *
 * Color table of named colors and lookup code derived from src/libes/gis/named_colr.c
 * of GRASS 4.1
 *
 * TRI - Terrain Ruggedness Index is as descibed in Wilson et al (2007)
 * this is based on the method of Valentine et al. (2004)  
 * 
 * TPI - Topographic Position Index follows the description in Wilson et al (2007), following Weiss (2001)
 * The radius is fixed at 1 cell width/height
 * 
 * Roughness - follows the definition in Wilson et al. (2007), which follows Dartnell (2000)
 *
 * References for TRI/TPI/Roughness:
 * Dartnell, P. 2000. Applying Remote Sensing Techniques to map Seafloor 
 *  Geology/Habitat Relationships. Masters Thesis, San Francisco State 
 *  University, pp. 108.
 * Valentine, P. C., S. J. Fuller, L. A. Scully. 2004. Terrain Ruggedness 
 *  Analysis and Distribution of Boulder Ridges in the Stellwagen Bank National
 *  Marine Sanctuary Region (poster). Galway, Ireland: 5th International 
 *  Symposium on Marine Geological and Biological Habitat Mapping (GeoHAB), 
 *  May 2004.
 * Weiss, A. D. 2001. Topographic Positions and Landforms Analysis (poster), 
 *  ESRI International User Conference, July 2001. San Diego, CA: ESRI.
 * Wilson, M. F. J.; O'Connell, B.; Brown, C.; Guinan, J. C. & Grehan, A. J. 
 *  Multiscale terrain analysis of multibeam bathymetry data for habitat mapping
 *  on the continental slope Marine Geodesy, 2007, 30, 3-35
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
            "                 [-trigonometric] [-zero_for_flat]\n"
            "                 [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-quiet]\n"
            "\n"
            " - To generate a color relief map from any GDAL-supported elevation raster\n"
            "     gdaldem color-relief input_dem color_text_file output_color_relief_map\n"
            "                 [-alpha] [-exact_color_entry | -nearest_color_entry]\n"
            "                 [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-quiet]\n"
            "     where color_text_file contains lines of the format \"elevation_value red green blue\"\n"
            "\n"
            " - To generate a Terrain Ruggedness Index (TRI) map from any GDAL-supported elevation raster\n"
            "     gdaldem TRI input_dem output_TRI_map\n"
            "                 [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-quiet]\n"
            "\n"
            " - To generate a Topographic Position Index (TPI) map from any GDAL-supported elevation raster\n"
            "     gdaldem TPI input_dem output_TPI_map\n"
            "                 [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-quiet]\n"
            "\n"
            " - To generate a roughness map from any GDAL-supported elevation raster\n"
            "     gdaldem roughness input_dem output_roughness_map\n"
            "                 [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-quiet]\n"
            "\n"
            " Notes : \n"
            "   Scale is the ratio of vertical units to horizontal\n"
            "    for Feet:Latlong use scale=370400, for Meters:LatLong use scale=111120 \n\n");
    exit( 1 );
}

/************************************************************************/
/*                  GDALGeneric3x3Processing()                          */
/************************************************************************/

typedef float (*GDALGeneric3x3ProcessingAlg) (float* pafWindow, double dfDstNoDataValue, void* pData);

CPLErr GDALGeneric3x3Processing  ( GDALRasterBandH hSrcBand,
                                   GDALRasterBandH hDstBand,
                                   GDALGeneric3x3ProcessingAlg pfnAlg,
                                   void* pData,
                                   GDALProgressFunc pfnProgress,
                                   void * pProgressData)
{
    CPLErr eErr;
    int bContainsNull;
    float *pafThreeLineWin; /* 3 line rotating source buffer */
    float *pafOutputBuf;     /* 1 line destination buffer */
    int i;

    int bSrcHasNoData, bDstHasNoData;
    double dfSrcNoDataValue = 0.0, dfDstNoDataValue = 0.0;

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

    pafOutputBuf = (float *) CPLMalloc(sizeof(float)*nXSize);
    pafThreeLineWin  = (float *) CPLMalloc(3*sizeof(float)*nXSize);

    dfSrcNoDataValue = GDALGetRasterNoDataValue(hSrcBand, &bSrcHasNoData);
    dfDstNoDataValue = GDALGetRasterNoDataValue(hDstBand, &bDstHasNoData);
    if (!bDstHasNoData)
        dfDstNoDataValue = 0.0;

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
                pafOutputBuf[j] = dfDstNoDataValue;
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
                    pafOutputBuf[j] = dfDstNoDataValue;
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
                    pafOutputBuf[j] = dfDstNoDataValue;
                    continue;
                } else {
                    // We have a valid 3x3 window.
                    pafOutputBuf[j] = pfnAlg(afWin, dfDstNoDataValue, pData);
                }
            }
        }

        /* -----------------------------------------
         * Write Line to Raster
         */
        GDALRasterIO(hDstBand,
                           GF_Write,
                           0, i, nXSize,
                           1, pafOutputBuf, nXSize, 1, GDT_Float32, 0, 0);

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
    CPLFree(pafOutputBuf);
    CPLFree(pafThreeLineWin);

    return eErr;
}


/************************************************************************/
/*                         GDALHillshade()                              */
/************************************************************************/

typedef struct
{
    double nsres;
    double ewres;
    double scale;
    double z;
    double altRadians;
    double azRadians;
} GDALHillshadeAlgData;

float GDALHillshadeAlg (float* afWin, double dfDstNoDataValue, void* pData)
{
    GDALHillshadeAlgData* psData = (GDALHillshadeAlgData*)pData;
    double x, y, aspect, slope, cang;
    
    // First Slope ...
    x = psData->z*((afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
        (afWin[2] + afWin[5] + afWin[5] + afWin[8])) /
        (8.0 * psData->ewres * psData->scale);

    y = psData->z*((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
        (afWin[0] + afWin[1] + afWin[1] + afWin[2])) /
        (8.0 * psData->nsres * psData->scale);

    slope = M_PI / 2 - atan(sqrt(x*x + y*y));

    // ... then aspect...
    aspect = atan2(x,y);

    // ... then the shade value
    cang = sin(psData->altRadians) * sin(slope) +
           cos(psData->altRadians) * cos(slope) *
           cos(psData->azRadians-M_PI/2 - aspect);

    if (cang <= 0.0) 
        cang = 1.0;
    else
        cang = 1.0 + (254.0 * cang);
        
    return cang;
}

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
    const double degreesToRadians = M_PI / 180.0;
    
    GDALHillshadeAlgData sData;
    sData.nsres = adfGeoTransform[5];
    sData.ewres = adfGeoTransform[1];
    sData.scale = scale;
    sData.z = z;
    sData.altRadians = alt * degreesToRadians;
    sData.azRadians = az * degreesToRadians;

    return GDALGeneric3x3Processing(hSrcBand, hDstBand,
                                    GDALHillshadeAlg, &sData,
                                    pfnProgress, pProgressData);
}


/************************************************************************/
/*                         GDALSlope()                                  */
/************************************************************************/

typedef struct
{
    double nsres;
    double ewres;
    double scale;
    int    slopeFormat;
} GDALSlopeAlgData;

float GDALSlopeAlg (float* afWin, double dfDstNoDataValue, void* pData)
{
    const double radiansToDegrees = 180.0 / M_PI;
    GDALSlopeAlgData* psData = (GDALSlopeAlgData*)pData;
    double dx, dy, key;
    
    dx = ((afWin[0] + afWin[3] + afWin[3] + afWin[6]) - 
          (afWin[2] + afWin[5] + afWin[5] + afWin[8]))/(8*psData->ewres*psData->scale);

    dy = ((afWin[6] + afWin[7] + afWin[7] + afWin[8]) - 
          (afWin[0] + afWin[1] + afWin[1] + afWin[2]))/(8*psData->nsres*psData->scale);

    key = dx * dx + dy * dy;

    if (psData->slopeFormat == 1) 
        return atan(sqrt(key)) * radiansToDegrees;
    else
        return 100*sqrt(key);
}

CPLErr GDALSlope  ( GDALRasterBandH hSrcBand,
                    GDALRasterBandH hDstBand,
                    double* adfGeoTransform,
                    double scale,
                    int slopeFormat,
                    GDALProgressFunc pfnProgress,
                    void * pProgressData)
{
    GDALSlopeAlgData sData;
    sData.nsres = adfGeoTransform[5];
    sData.ewres = adfGeoTransform[1];
    sData.scale = scale;
    sData.slopeFormat = slopeFormat;

    return GDALGeneric3x3Processing(hSrcBand, hDstBand,
                                    GDALSlopeAlg, &sData,
                                    pfnProgress, pProgressData);
}

/************************************************************************/
/*                         GDALAspect()                                 */
/************************************************************************/

typedef struct
{
    int bAngleAsAzimuth;
} GDALAspectAlgData;

float GDALAspectAlg (float* afWin, double dfDstNoDataValue, void* pData)
{
    const double degreesToRadians = M_PI / 180.0;
    GDALAspectAlgData* psData = (GDALAspectAlgData*)pData;
    double dx, dy;
    float aspect;
    
    dx = ((afWin[2] + afWin[5] + afWin[5] + afWin[8]) -
          (afWin[0] + afWin[3] + afWin[3] + afWin[6]));

    dy = ((afWin[6] + afWin[7] + afWin[7] + afWin[8]) - 
          (afWin[0] + afWin[1] + afWin[1] + afWin[2]));

    aspect = atan2(dy/8.0,-1.0*dx/8.0) / degreesToRadians;

    if (dx == 0 && dy == 0)
    {
        /* Flat area */
        aspect = dfDstNoDataValue;
    } 
    else if ( psData->bAngleAsAzimuth )
    {
        if (aspect > 90.0) 
            aspect = 450.0 - aspect;
        else
            aspect = 90.0 - aspect;
    }
    else
    {
        if (aspect < 0)
            aspect += 360.0;
    }

    if (aspect == 360.0) 
        aspect = 0.0;

    return aspect;
}

CPLErr GDALAspect  (GDALRasterBandH hSrcBand,
                    GDALRasterBandH hDstBand,
                    int bAngleAsAzimuth,
                    GDALProgressFunc pfnProgress,
                    void * pProgressData)
{
    GDALAspectAlgData sData;
    sData.bAngleAsAzimuth = bAngleAsAzimuth;

    return GDALGeneric3x3Processing(hSrcBand, hDstBand,
                                    GDALAspectAlg, &sData,
                                    pfnProgress, pProgressData);
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

typedef enum
{
    COLOR_SELECTION_INTERPOLATE,
    COLOR_SELECTION_NEAREST_ENTRY,
    COLOR_SELECTION_EXACT_ENTRY
} ColorSelectionMode;

static int GDALColorReliefGetRGBA (ColorAssociation* pasColorAssociation,
                                   int nColorAssociation,
                                   double dfVal,
                                   ColorSelectionMode eColorSelectionMode,
                                   int* pnR,
                                   int* pnG,
                                   int* pnB,
                                   int* pnA)
{
    int i;
    int lower = 0;
    int upper = nColorAssociation - 1;
    int mid;

    /* Find the index of the first element in the LUT input array that */
    /* is not smaller than the dfVal value. */
    while(TRUE)
    {
        mid = (lower + upper) / 2;
        if (upper - lower <= 1)
        {
            if (dfVal < pasColorAssociation[lower].dfVal)
                i = lower;
            else if (dfVal < pasColorAssociation[upper].dfVal)
                i = upper;
            else
                i = upper + 1;
            break;
        }
        else if (pasColorAssociation[mid].dfVal >= dfVal)
        {
            upper = mid;
        }
        else
        {
            lower = mid;
        }
    }

    if (i == 0)
    {
        if (eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY &&
            pasColorAssociation[0].dfVal != dfVal)
        {
            *pnR = 0;
            *pnG = 0;
            *pnB = 0;
            *pnA = 0;
            return FALSE;
        }
        else
        {
            *pnR = pasColorAssociation[0].nR;
            *pnG = pasColorAssociation[0].nG;
            *pnB = pasColorAssociation[0].nB;
            *pnA = pasColorAssociation[0].nA;
            return TRUE;
        }
    }
    else if (i == nColorAssociation)
    {
        if (eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY &&
            pasColorAssociation[i-1].dfVal != dfVal)
        {
            *pnR = 0;
            *pnG = 0;
            *pnB = 0;
            *pnA = 0;
            return FALSE;
        }
        else
        {
            *pnR = pasColorAssociation[i-1].nR;
            *pnG = pasColorAssociation[i-1].nG;
            *pnB = pasColorAssociation[i-1].nB;
            *pnA = pasColorAssociation[i-1].nA;
            return TRUE;
        }
    }
    else
    {
        if (eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY &&
            pasColorAssociation[i-1].dfVal != dfVal)
        {
            *pnR = 0;
            *pnG = 0;
            *pnB = 0;
            *pnA = 0;
            return FALSE;
        }
        
        if (eColorSelectionMode == COLOR_SELECTION_NEAREST_ENTRY &&
            pasColorAssociation[i-1].dfVal != dfVal)
        {
            int index;
            if (dfVal - pasColorAssociation[i-1].dfVal <
                pasColorAssociation[i].dfVal - dfVal)
                index = i -1;
            else
                index = i;

            *pnR = pasColorAssociation[index].nR;
            *pnG = pasColorAssociation[index].nG;
            *pnB = pasColorAssociation[index].nB;
            *pnA = pasColorAssociation[index].nA;
            return TRUE;
        }
        
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
        
        return TRUE;
    }
}

/* dfPct : percentage between 0 and 1 */
static double GDALColorReliefGetAbsoluteValFromPct(GDALRasterBandH hSrcBand,
                                                   double dfPct)
{
    double dfMin, dfMax;
    int bSuccessMin, bSuccessMax;
    dfMin = GDALGetRasterMinimum(hSrcBand, &bSuccessMin);
    dfMax = GDALGetRasterMaximum(hSrcBand, &bSuccessMax);
    if (!bSuccessMin || !bSuccessMax)
    {
        double dfMean, dfStdDev;
        fprintf(stderr, "Computing source raster statistics...\n");
        GDALComputeRasterStatistics(hSrcBand, FALSE, &dfMin, &dfMax,
                                    &dfMean, &dfStdDev, NULL, NULL);
    }
    return dfMin + dfPct * (dfMax - dfMin);
}

typedef struct
{
    const char *name;
    float r, g, b;
} NamedColor;

static const NamedColor namedColors[] = {
    { "white",  1.00, 1.00, 1.00 },
    { "black",  0.00, 0.00, 0.00 },
    { "red",    1.00, 0.00, 0.00 },
    { "green",  0.00, 1.00, 0.00 },
    { "blue",   0.00, 0.00, 1.00 },
    { "yellow", 1.00, 1.00, 0.00 },
    { "magenta",1.00, 0.00, 1.00 },
    { "cyan",   0.00, 1.00, 1.00 },
    { "aqua",   0.00, 0.75, 0.75 },
    { "grey",   0.75, 0.75, 0.75 },
    { "gray",   0.75, 0.75, 0.75 },
    { "orange", 1.00, 0.50, 0.00 },
    { "brown",  0.75, 0.50, 0.25 },
    { "purple", 0.50, 0.00, 1.00 },
    { "violet", 0.50, 0.00, 1.00 },
    { "indigo", 0.00, 0.50, 1.00 },
};

static
int GDALColorReliefFindNamedColor(const char *pszColorName, int *pnR, int *pnG, int *pnB)
{
    unsigned int i;

    *pnR = *pnG = *pnB = 0;
    for (i = 0; i < sizeof(namedColors) / sizeof(namedColors[0]); i++)
    {
        if (EQUAL(pszColorName, namedColors[i].name))
        {
            *pnR = (int)(255. * namedColors[i].r);
            *pnG = (int)(255. * namedColors[i].g);
            *pnB = (int)(255. * namedColors[i].b);
            return TRUE;
        }
    }
    return FALSE;
}

static
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
        char** papszFields = CSLTokenizeStringComplex(pszLine, " ,\t:", 
                                                      FALSE, FALSE );
        /* Skip comment lines */
        int nTokens = CSLCount(papszFields);
        if (nTokens >= 2 &&
            papszFields[0][0] != '#' &&
            papszFields[0][0] != '/')
        {
            pasColorAssociation =
                    (ColorAssociation*)CPLRealloc(pasColorAssociation,
                           (nColorAssociation + 1) * sizeof(ColorAssociation));
            if (EQUAL(papszFields[0], "nv") && bSrcHasNoData)
                pasColorAssociation[nColorAssociation].dfVal = dfSrcNoDataValue;
            else if (strlen(papszFields[0]) > 1 && papszFields[0][strlen(papszFields[0])-1] == '%')
            {
                double dfPct = atof(papszFields[0]) / 100.;
                if (dfPct < 0.0 || dfPct > 1.0)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Wrong value for a percentage : %s", papszFields[0]);
                    CSLDestroy(papszFields);
                    VSIFClose(fpColorFile);
                    CPLFree(pasColorAssociation);
                    *pnColors = 0;
                    return NULL;
                }
                pasColorAssociation[nColorAssociation].dfVal =
                        GDALColorReliefGetAbsoluteValFromPct(hSrcBand, dfPct);
            }
            else
                pasColorAssociation[nColorAssociation].dfVal = atof(papszFields[0]);

            if (nTokens >= 4)
            {
                pasColorAssociation[nColorAssociation].nR = atoi(papszFields[1]);
                pasColorAssociation[nColorAssociation].nG = atoi(papszFields[2]);
                pasColorAssociation[nColorAssociation].nB = atoi(papszFields[3]);
                pasColorAssociation[nColorAssociation].nA =
                        (CSLCount(papszFields) >= 5 ) ? atoi(papszFields[4]) : 255;
            }
            else
            {
                int nR, nG, nB;
                if (!GDALColorReliefFindNamedColor(papszFields[1], &nR, &nG, &nB))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unknown color : %s", papszFields[1]);
                    CSLDestroy(papszFields);
                    VSIFClose(fpColorFile);
                    CPLFree(pasColorAssociation);
                    *pnColors = 0;
                    return NULL;
                }
                pasColorAssociation[nColorAssociation].nR = nR;
                pasColorAssociation[nColorAssociation].nG = nG;
                pasColorAssociation[nColorAssociation].nB = nB;
                            pasColorAssociation[nColorAssociation].nA =
                    (CSLCount(papszFields) >= 3 ) ? atoi(papszFields[2]) : 255;
            }

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
                        ColorSelectionMode eColorSelectionMode,
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
    int i, j;

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        eErr = CE_Failure;
        goto end;
    }

    for ( i = 0; i < nYSize; i++)
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

        for ( j = 0; j < nXSize; j++)
        {
            GDALColorReliefGetRGBA  (pasColorAssociation,
                                     nColorAssociation,
                                     pafSourceBuf[j],
                                     eColorSelectionMode,
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
/*                         GDALTRI()                                  */
/************************************************************************/

float GDALTRIAlg (float* afWin, double dfDstNoDataValue, void* pData)
{
    // Terrain Ruggedness is average difference in height
    return (fabs(afWin[0]-afWin[4]) +
            fabs(afWin[1]-afWin[4]) +
            fabs(afWin[2]-afWin[4]) +
            fabs(afWin[3]-afWin[4]) +
            fabs(afWin[5]-afWin[4]) +
            fabs(afWin[6]-afWin[4]) +
            fabs(afWin[7]-afWin[4]) +
            fabs(afWin[8]-afWin[4]))/8;
}

CPLErr GDALTRI  ( GDALRasterBandH hSrcBand,
                  GDALRasterBandH hDstBand,
                  GDALProgressFunc pfnProgress,
                  void * pProgressData)
{
    return GDALGeneric3x3Processing(hSrcBand, hDstBand,
                                    GDALTRIAlg, NULL,
                                    pfnProgress, pProgressData);
}

/************************************************************************/
/*                         GDALTPI()                                  */
/************************************************************************/

float GDALTPIAlg (float* afWin, double dfDstNoDataValue, void* pData)
{
    // Terrain Position is the difference between
    // The central cell and the mean of the surrounding cells
    return afWin[4] - 
            ((afWin[0]+
              afWin[1]+
              afWin[2]+
              afWin[3]+
              afWin[5]+
              afWin[6]+
              afWin[7]+
              afWin[8])/8);
}

CPLErr GDALTPI  ( GDALRasterBandH hSrcBand,
                  GDALRasterBandH hDstBand,
                  GDALProgressFunc pfnProgress,
                  void * pProgressData)
{
    return GDALGeneric3x3Processing(hSrcBand, hDstBand,
                                    GDALTPIAlg, NULL,
                                    pfnProgress, pProgressData);
}

/************************************************************************/
/*                      GDALRoughness()                                */
/************************************************************************/

float GDALRoughnessAlg (float* afWin, double dfDstNoDataValue, void* pData)
{
    // Roughness is the largest difference
    //  between any two cells

    float pafRoughnessMin = afWin[0];
    float pafRoughnessMax = afWin[0];

    for ( int k = 1; k < 9; k++)
    {
        if (afWin[k] > pafRoughnessMax)
        {
            pafRoughnessMax=afWin[k];
        }
        if (afWin[k] < pafRoughnessMin)
        {
            pafRoughnessMin=afWin[k];
        }
    }
    return pafRoughnessMax - pafRoughnessMin;
}

CPLErr GDALRoughness  ( GDALRasterBandH hSrcBand,
                        GDALRasterBandH hDstBand,
                        GDALProgressFunc pfnProgress,
                        void * pProgressData)
{
    return GDALGeneric3x3Processing(hSrcBand, hDstBand,
                                    GDALRoughnessAlg, NULL,
                                    pfnProgress, pProgressData);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/


enum
{
    HILL_SHADE,
    SLOPE,
    ASPECT,
    COLOR_RELIEF,
    TRI,
    TPI,
    ROUGHNESS
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
    int bZeroForFlat = FALSE;
    int bAngleAsAzimuth = TRUE;
    ColorSelectionMode eColorSelectionMode = COLOR_SELECTION_INTERPOLATE;
    
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
    else if ( EQUAL(argv[1], "TRI") )
    {
        eUtilityMode = TRI;
    }
    else if ( EQUAL(argv[1], "TPI") )
    {
        eUtilityMode = TPI;
    }
    else if ( EQUAL(argv[1], "roughness") )
    {
        eUtilityMode = ROUGHNESS;
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
        else if ( eUtilityMode == ASPECT && EQUAL(argv[i], "-trigonometric"))
        {
            bAngleAsAzimuth = FALSE;
        }
        else if ( eUtilityMode == ASPECT && EQUAL(argv[i], "-zero_for_flat"))
        {
            bZeroForFlat = TRUE;
        }
        else if ( eUtilityMode == COLOR_RELIEF && EQUAL(argv[i], "-exact_color_entry"))
        {
            eColorSelectionMode = COLOR_SELECTION_EXACT_ENTRY;
        }
        else if ( eUtilityMode == COLOR_RELIEF && EQUAL(argv[i], "-nearest_color_entry"))
        {
            eColorSelectionMode = COLOR_SELECTION_NEAREST_ENTRY;
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
        if (!bZeroForFlat)
            GDALSetRasterNoDataValue(hDstBand, -9999.0);

        GDALAspect( hSrcBand, 
                    hDstBand, 
                    bAngleAsAzimuth,
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
                         eColorSelectionMode,
                         pfnProgress, NULL);
    }
    else if (eUtilityMode == TRI)
    {
        GDALSetRasterNoDataValue(hDstBand, -9999);

        GDALTRI(  hSrcBand, 
                  hDstBand, 
                  pfnProgress, NULL);
    }
    else if (eUtilityMode == TPI)
    {
        GDALSetRasterNoDataValue(hDstBand, -9999);

        GDALTPI(  hSrcBand, 
                  hDstBand, 
                  pfnProgress, NULL);
    }
    else if (eUtilityMode == ROUGHNESS)
    {
        GDALSetRasterNoDataValue(hDstBand, -9999);

        GDALRoughness(  hSrcBand, 
                        hDstBand, 
                        pfnProgress, NULL);
    }

    GDALClose(hSrcDataset);
    GDALClose(hDstDataset);

    GDALDestroyDriverManager();
    CSLDestroy( argv );
    CSLDestroy( papszCreateOptions );

    return 0;
}

