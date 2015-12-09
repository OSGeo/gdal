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
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
 * TRI - Terrain Ruggedness Index is as described in Wilson et al. (2007)
 * this is based on the method of Valentine et al. (2004)
 *
 * TPI - Topographic Position Index follows the description in
 * Wilson et al. (2007), following Weiss (2001).  The radius is fixed
 * at 1 cell width/height
 *
 * Roughness - follows the definition in Wilson et al. (2007), which follows
 * Dartnell (2000).
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

#include "cpl_vsi.h"
#include <stdlib.h>
#include <math.h>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils_priv.h"

CPL_CVSID("$Id$");

#define INTERPOL(a,b) ((bSrcHasNoData && (ARE_REAL_EQUAL(a, fSrcNoDataValue) || ARE_REAL_EQUAL(b, fSrcNoDataValue))) ? fSrcNoDataValue : 2 * (a) - (b))

typedef enum
{
    COLOR_SELECTION_INTERPOLATE,
    COLOR_SELECTION_NEAREST_ENTRY,
    COLOR_SELECTION_EXACT_ENTRY
} ColorSelectionMode;

struct GDALDEMProcessingOptions
{
    /*! output format. The default is GeoTIFF(GTiff). Use the short format name. */
    char *pszFormat;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress;
    
    /*! pointer to the progress data variable */
    void *pProgressData;

    double z;
    double scale;
    double az;
    double alt;
    int slopeFormat; 
    int bAddAlpha;
    int bZeroForFlat;
    int bAngleAsAzimuth ;
    ColorSelectionMode eColorSelectionMode;
    int bComputeAtEdges;
    int bZevenbergenThorne;
    int bCombined;
    char** papszCreateOptions;
    int nBand;
};

/************************************************************************/
/*                          ComputeVal()                                */
/************************************************************************/

typedef float (*GDALGeneric3x3ProcessingAlg) (float* pafWindow, float fDstNoDataValue, void* pData);

static float ComputeVal(int bSrcHasNoData, float fSrcNoDataValue,
                        int bIsSrcNoDataNan,
                        float* afWin, float fDstNoDataValue,
                        GDALGeneric3x3ProcessingAlg pfnAlg,
                        void* pData,
                        int bComputeAtEdges)
{
    if (bSrcHasNoData &&
            ((!bIsSrcNoDataNan && ARE_REAL_EQUAL(afWin[4], fSrcNoDataValue)) ||
             (bIsSrcNoDataNan && CPLIsNan(afWin[4]))))
    {
        return fDstNoDataValue;
    }
    else if (bSrcHasNoData)
    {
        int k;
        for(k=0;k<9;k++)
        {
            if ((!bIsSrcNoDataNan && ARE_REAL_EQUAL(afWin[k], fSrcNoDataValue)) ||
                (bIsSrcNoDataNan && CPLIsNan(afWin[k])))
            {
                if (bComputeAtEdges)
                    afWin[k] = afWin[4];
                else
                    return fDstNoDataValue;
            }
        }
    }

    return pfnAlg(afWin, fDstNoDataValue, pData);
}

/************************************************************************/
/*                  GDALGeneric3x3Processing()                          */
/************************************************************************/

static
CPLErr GDALGeneric3x3Processing  ( GDALRasterBandH hSrcBand,
                                   GDALRasterBandH hDstBand,
                                   GDALGeneric3x3ProcessingAlg pfnAlg,
                                   void* pData,
                                   int bComputeAtEdges,
                                   GDALProgressFunc pfnProgress,
                                   void * pProgressData)
{
    CPLErr eErr;
    float *pafThreeLineWin; /* 3 line rotating source buffer */
    float *pafOutputBuf;     /* 1 line destination buffer */
    int i, j;

    int bSrcHasNoData, bDstHasNoData;
    float fSrcNoDataValue = 0.0, fDstNoDataValue = 0.0;

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
    pafThreeLineWin  = (float *) CPLMalloc(3*sizeof(float)*(nXSize+1));

    fSrcNoDataValue = (float) GDALGetRasterNoDataValue(hSrcBand, &bSrcHasNoData);
    fDstNoDataValue = (float) GDALGetRasterNoDataValue(hDstBand, &bDstHasNoData);
    if (!bDstHasNoData)
        fDstNoDataValue = 0.0;
    int bIsSrcNoDataNan = bSrcHasNoData && CPLIsNan(fSrcNoDataValue);
    
    int nLine1Off = 0*nXSize;
    int nLine2Off = 1*nXSize;
    int nLine3Off = 2*nXSize;

    // Move a 3x3 pafWindow over each cell 
    // (where the cell in question is #4)
    // 
    //      0 1 2
    //      3 4 5
    //      6 7 8

    /* Preload the first 2 lines */
    for ( i = 0; i < 2 && i < nYSize; i++)
    {
        if( GDALRasterIO(   hSrcBand,
                        GF_Read,
                        0, i,
                        nXSize, 1,
                        pafThreeLineWin + i * nXSize,
                        nXSize, 1,
                        GDT_Float32,
                        0, 0) != CE_None )
        {
            eErr = CE_Failure;
            goto end;
        }
    }
    
    if (bComputeAtEdges && nXSize >= 2 && nYSize >= 2)
    {
        for (j = 0; j < nXSize; j++)
        {
            float afWin[9];
            int jmin = (j == 0) ? j : j - 1;
            int jmax = (j == nXSize - 1) ? j : j + 1;

            afWin[0] = INTERPOL(pafThreeLineWin[jmin], pafThreeLineWin[nXSize + jmin]);
            afWin[1] = INTERPOL(pafThreeLineWin[j],    pafThreeLineWin[nXSize + j]);
            afWin[2] = INTERPOL(pafThreeLineWin[jmax], pafThreeLineWin[nXSize + jmax]);
            afWin[3] = pafThreeLineWin[jmin];
            afWin[4] = pafThreeLineWin[j];
            afWin[5] = pafThreeLineWin[jmax];
            afWin[6] = pafThreeLineWin[nXSize + jmin];
            afWin[7] = pafThreeLineWin[nXSize + j];
            afWin[8] = pafThreeLineWin[nXSize + jmax];

            pafOutputBuf[j] = ComputeVal(bSrcHasNoData, fSrcNoDataValue,
                                         bIsSrcNoDataNan,
                                         afWin, fDstNoDataValue,
                                         pfnAlg, pData, bComputeAtEdges);
        }
        eErr = GDALRasterIO(hDstBand, GF_Write,
                    0, 0, nXSize, 1,
                    pafOutputBuf, nXSize, 1, GDT_Float32, 0, 0);
    }
    else
    {
        // Exclude the edges
        for (j = 0; j < nXSize; j++)
        {
            pafOutputBuf[j] = fDstNoDataValue;
        }
        eErr = GDALRasterIO(hDstBand, GF_Write,
                    0, 0, nXSize, 1,
                    pafOutputBuf, nXSize, 1, GDT_Float32, 0, 0);
    
        if (eErr == CE_None && nYSize > 1)
        {
            eErr = GDALRasterIO(hDstBand, GF_Write,
                        0, nYSize - 1, nXSize, 1,
                        pafOutputBuf, nXSize, 1, GDT_Float32, 0, 0);
        }
    }
    if( eErr != CE_None )
        goto end;


    for ( i = 1; i < nYSize-1; i++)
    {
        /* Read third line of the line buffer */
        eErr = GDALRasterIO(   hSrcBand,
                        GF_Read,
                        0, i+1,
                        nXSize, 1,
                        pafThreeLineWin + nLine3Off,
                        nXSize, 1,
                        GDT_Float32,
                        0, 0);
        if (eErr != CE_None)
            goto end;

        if (bComputeAtEdges && nXSize >= 2)
        {
            float afWin[9];

            j = 0;
            afWin[0] = INTERPOL(pafThreeLineWin[nLine1Off + j], pafThreeLineWin[nLine1Off + j+1]);
            afWin[1] = pafThreeLineWin[nLine1Off + j];
            afWin[2] = pafThreeLineWin[nLine1Off + j+1];
            afWin[3] = INTERPOL(pafThreeLineWin[nLine2Off + j], pafThreeLineWin[nLine2Off + j+1]);
            afWin[4] = pafThreeLineWin[nLine2Off + j];
            afWin[5] = pafThreeLineWin[nLine2Off + j+1];
            afWin[6] = INTERPOL(pafThreeLineWin[nLine3Off + j], pafThreeLineWin[nLine3Off + j+1]);
            afWin[7] = pafThreeLineWin[nLine3Off + j];
            afWin[8] = pafThreeLineWin[nLine3Off + j+1];

            pafOutputBuf[j] = ComputeVal(bSrcHasNoData, fSrcNoDataValue,
                                         bIsSrcNoDataNan,
                                         afWin, fDstNoDataValue,
                                         pfnAlg, pData, bComputeAtEdges);
            j = nXSize - 1;

            afWin[0] = pafThreeLineWin[nLine1Off + j-1];
            afWin[1] = pafThreeLineWin[nLine1Off + j];
            afWin[2] = INTERPOL(pafThreeLineWin[nLine1Off + j], pafThreeLineWin[nLine1Off + j-1]);
            afWin[3] = pafThreeLineWin[nLine2Off + j-1];
            afWin[4] = pafThreeLineWin[nLine2Off + j];
            afWin[5] = INTERPOL(pafThreeLineWin[nLine2Off + j], pafThreeLineWin[nLine2Off + j-1]);
            afWin[6] = pafThreeLineWin[nLine3Off + j-1];
            afWin[7] = pafThreeLineWin[nLine3Off + j];
            afWin[8] = INTERPOL(pafThreeLineWin[nLine3Off + j], pafThreeLineWin[nLine3Off + j-1]);

            pafOutputBuf[j] = ComputeVal(bSrcHasNoData, fSrcNoDataValue,
                                         bIsSrcNoDataNan,
                                         afWin, fDstNoDataValue,
                                         pfnAlg, pData, bComputeAtEdges);
        }
        else
        {
            // Exclude the edges
            pafOutputBuf[0] = fDstNoDataValue;
            if (nXSize > 1)
                pafOutputBuf[nXSize - 1] = fDstNoDataValue;
        }

        for (j = 1; j < nXSize - 1; j++)
        {
            float afWin[9];
            afWin[0] = pafThreeLineWin[nLine1Off + j-1];
            afWin[1] = pafThreeLineWin[nLine1Off + j];
            afWin[2] = pafThreeLineWin[nLine1Off + j+1];
            afWin[3] = pafThreeLineWin[nLine2Off + j-1];
            afWin[4] = pafThreeLineWin[nLine2Off + j];
            afWin[5] = pafThreeLineWin[nLine2Off + j+1];
            afWin[6] = pafThreeLineWin[nLine3Off + j-1];
            afWin[7] = pafThreeLineWin[nLine3Off + j];
            afWin[8] = pafThreeLineWin[nLine3Off + j+1];

            pafOutputBuf[j] = ComputeVal(bSrcHasNoData, fSrcNoDataValue,
                                         bIsSrcNoDataNan,
                                         afWin, fDstNoDataValue,
                                         pfnAlg, pData, bComputeAtEdges);
        }

        /* -----------------------------------------
         * Write Line to Raster
         */
        eErr = GDALRasterIO(hDstBand, GF_Write, 0, i, nXSize, 1,
                     pafOutputBuf, nXSize, 1, GDT_Float32, 0, 0);
        if (eErr != CE_None)
            goto end;

        if( !pfnProgress( 1.0 * (i+1) / nYSize, NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
            goto end;
        }
        
        int nTemp = nLine1Off;
        nLine1Off = nLine2Off;
        nLine2Off = nLine3Off;
        nLine3Off = nTemp;
    }

    if (bComputeAtEdges && nXSize >= 2 && nYSize >= 2)
    {
        for (j = 0; j < nXSize; j++)
        {
            float afWin[9];
            int jmin = (j == 0) ? j : j - 1;
            int jmax = (j == nXSize - 1) ? j : j + 1;

            afWin[0] = pafThreeLineWin[nLine1Off + jmin];
            afWin[1] = pafThreeLineWin[nLine1Off + j];
            afWin[2] = pafThreeLineWin[nLine1Off + jmax];
            afWin[3] = pafThreeLineWin[nLine2Off + jmin];
            afWin[4] = pafThreeLineWin[nLine2Off + j];
            afWin[5] = pafThreeLineWin[nLine2Off + jmax];
            afWin[6] = INTERPOL(pafThreeLineWin[nLine2Off + jmin], pafThreeLineWin[nLine1Off + jmin]);
            afWin[7] = INTERPOL(pafThreeLineWin[nLine2Off + j],    pafThreeLineWin[nLine1Off + j]);
            afWin[8] = INTERPOL(pafThreeLineWin[nLine2Off + jmax], pafThreeLineWin[nLine1Off + jmax]);

            pafOutputBuf[j] = ComputeVal(bSrcHasNoData, fSrcNoDataValue,
                                         bIsSrcNoDataNan,
                                         afWin, fDstNoDataValue,
                                         pfnAlg, pData, bComputeAtEdges);
        }
        eErr = GDALRasterIO(hDstBand, GF_Write,
                     0, i, nXSize, 1,
                     pafOutputBuf, nXSize, 1, GDT_Float32, 0, 0);
        if( eErr != CE_None )
            goto end;
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
    double sin_altRadians;
    double cos_altRadians_mul_z_scale_factor;
    double azRadians;
    double square_z_scale_factor;
    double square_M_PI_2;
} GDALHillshadeAlgData;

/* Unoptimized formulas are :
    x = psData->z*((afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
        (afWin[2] + afWin[5] + afWin[5] + afWin[8])) /
        (8.0 * psData->ewres * psData->scale);

    y = psData->z*((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
        (afWin[0] + afWin[1] + afWin[1] + afWin[2])) /
        (8.0 * psData->nsres * psData->scale);

    slope = M_PI / 2 - atan(sqrt(x*x + y*y));

    aspect = atan2(y,x);

    cang = sin(alt * degreesToRadians) * sin(slope) +
           cos(alt * degreesToRadians) * cos(slope) *
           cos(az * degreesToRadians - M_PI/2 - aspect);
*/

static
float GDALHillshadeAlg (float* afWin, CPL_UNUSED float fDstNoDataValue, void* pData)
{
    GDALHillshadeAlgData* psData = (GDALHillshadeAlgData*)pData;
    double x, y, aspect, xx_plus_yy, cang;

    // First Slope ...
    x = ((afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
        (afWin[2] + afWin[5] + afWin[5] + afWin[8])) / psData->ewres;

    y = ((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
        (afWin[0] + afWin[1] + afWin[1] + afWin[2])) / psData->nsres;

    xx_plus_yy = x * x + y * y;

    // ... then aspect...
    aspect = atan2(y,x);

    // ... then the shade value
    cang = (psData->sin_altRadians -
           psData->cos_altRadians_mul_z_scale_factor * sqrt(xx_plus_yy) *
           sin(aspect - psData->azRadians)) /
           sqrt(1 + psData->square_z_scale_factor * xx_plus_yy);

    if (cang <= 0.0)
        cang = 1.0;
    else
        cang = 1.0 + (254.0 * cang);

    return (float) cang;
}

static
float GDALHillshadeCombinedAlg (float* afWin, CPL_UNUSED float fDstNoDataValue, void* pData)
{
    GDALHillshadeAlgData* psData = (GDALHillshadeAlgData*)pData;
    double x, y, aspect, xx_plus_yy, cang;

    // First Slope ...
    x = ((afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
        (afWin[2] + afWin[5] + afWin[5] + afWin[8])) / psData->ewres;

    y = ((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
        (afWin[0] + afWin[1] + afWin[1] + afWin[2])) / psData->nsres;

    xx_plus_yy = x * x + y * y;

    // ... then aspect...
    aspect = atan2(y,x);
    double slope = xx_plus_yy * psData->square_z_scale_factor;

    // ... then the shade value
    cang = acos((psData->sin_altRadians -
           psData->cos_altRadians_mul_z_scale_factor * sqrt(xx_plus_yy) *
           sin(aspect - psData->azRadians)) /
           sqrt(1 + slope));

    // combined shading
    cang = 1 - cang * atan(sqrt(slope)) / psData->square_M_PI_2;

    if (cang <= 0.0)
        cang = 1.0;
    else
        cang = 1.0 + (254.0 * cang);

    return (float) cang;
}

static
float GDALHillshadeZevenbergenThorneAlg (float* afWin, CPL_UNUSED float fDstNoDataValue, void* pData)
{
    GDALHillshadeAlgData* psData = (GDALHillshadeAlgData*)pData;
    double x, y, aspect, xx_plus_yy, cang;

    // First Slope ...
    x = (afWin[3] - afWin[5]) / psData->ewres;

    y = (afWin[7] - afWin[1]) / psData->nsres;

    xx_plus_yy = x * x + y * y;

    // ... then aspect...
    aspect = atan2(y,x);

    // ... then the shade value
    cang = (psData->sin_altRadians -
           psData->cos_altRadians_mul_z_scale_factor * sqrt(xx_plus_yy) *
           sin(aspect - psData->azRadians)) /
           sqrt(1 + psData->square_z_scale_factor * xx_plus_yy);

    if (cang <= 0.0)
        cang = 1.0;
    else
        cang = 1.0 + (254.0 * cang);

    return (float) cang;
}

static
float GDALHillshadeZevenbergenThorneCombinedAlg (float* afWin, CPL_UNUSED float fDstNoDataValue, void* pData)
{
    GDALHillshadeAlgData* psData = (GDALHillshadeAlgData*)pData;
    double x, y, aspect, xx_plus_yy, cang;

    // First Slope ...
    x = (afWin[3] - afWin[5]) / psData->ewres;

    y = (afWin[7] - afWin[1]) / psData->nsres;

    xx_plus_yy = x * x + y * y;

    // ... then aspect...
    aspect = atan2(y,x);
    double slope = xx_plus_yy * psData->square_z_scale_factor;

    // ... then the shade value
    cang = acos((psData->sin_altRadians -
           psData->cos_altRadians_mul_z_scale_factor * sqrt(xx_plus_yy) *
           sin(aspect - psData->azRadians)) /
           sqrt(1 + slope));

    // combined shading
    cang = 1 - cang * atan(sqrt(slope)) / psData->square_M_PI_2;

    if (cang <= 0.0) 
        cang = 1.0;
    else
        cang = 1.0 + (254.0 * cang);
        
    return (float) cang;
}

static
void*  GDALCreateHillshadeData(double* adfGeoTransform,
                               double z,
                               double scale,
                               double alt,
                               double az,
                               int bZevenbergenThorne)
{
    GDALHillshadeAlgData* pData =
        (GDALHillshadeAlgData*)CPLMalloc(sizeof(GDALHillshadeAlgData));
        
    const double degreesToRadians = M_PI / 180.0;
    pData->nsres = adfGeoTransform[5];
    pData->ewres = adfGeoTransform[1];
    pData->sin_altRadians = sin(alt * degreesToRadians);
    pData->azRadians = az * degreesToRadians;
    double z_scale_factor = z / (((bZevenbergenThorne) ? 2 : 8) * scale);
    pData->cos_altRadians_mul_z_scale_factor =
        cos(alt * degreesToRadians) * z_scale_factor;
    pData->square_z_scale_factor = z_scale_factor * z_scale_factor;
    pData->square_M_PI_2 = (M_PI*M_PI)/4;
    return pData;
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

static
float GDALSlopeHornAlg (float* afWin, CPL_UNUSED float fDstNoDataValue, void* pData)
{
    const double radiansToDegrees = 180.0 / M_PI;
    GDALSlopeAlgData* psData = (GDALSlopeAlgData*)pData;
    double dx, dy, key;

    dx = ((afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
          (afWin[2] + afWin[5] + afWin[5] + afWin[8]))/psData->ewres;

    dy = ((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
          (afWin[0] + afWin[1] + afWin[1] + afWin[2]))/psData->nsres;

    key = (dx * dx + dy * dy);

    if (psData->slopeFormat == 1)
        return (float) (atan(sqrt(key) / (8*psData->scale)) * radiansToDegrees);
    else
        return (float) (100*(sqrt(key) / (8*psData->scale)));
}

static
float GDALSlopeZevenbergenThorneAlg (float* afWin, CPL_UNUSED float fDstNoDataValue, void* pData)
{
    const double radiansToDegrees = 180.0 / M_PI;
    GDALSlopeAlgData* psData = (GDALSlopeAlgData*)pData;
    double dx, dy, key;

    dx = (afWin[3] - afWin[5])/psData->ewres;

    dy = (afWin[7] - afWin[1])/psData->nsres;

    key = (dx * dx + dy * dy);

    if (psData->slopeFormat == 1)
        return (float) (atan(sqrt(key) / (2*psData->scale)) * radiansToDegrees);
    else
        return (float) (100*(sqrt(key) / (2*psData->scale)));
}

static
void*  GDALCreateSlopeData(double* adfGeoTransform,
                           double scale,
                           int slopeFormat)
{
    GDALSlopeAlgData* pData =
        (GDALSlopeAlgData*)CPLMalloc(sizeof(GDALSlopeAlgData));
        
    pData->nsres = adfGeoTransform[5];
    pData->ewres = adfGeoTransform[1];
    pData->scale = scale;
    pData->slopeFormat = slopeFormat;
    return pData;
}

/************************************************************************/
/*                         GDALAspect()                                 */
/************************************************************************/

typedef struct
{
    int bAngleAsAzimuth;
} GDALAspectAlgData;

static
float GDALAspectAlg (float* afWin, float fDstNoDataValue, void* pData)
{
    const double degreesToRadians = M_PI / 180.0;
    GDALAspectAlgData* psData = (GDALAspectAlgData*)pData;
    double dx, dy;
    float aspect;
    
    dx = ((afWin[2] + afWin[5] + afWin[5] + afWin[8]) -
          (afWin[0] + afWin[3] + afWin[3] + afWin[6]));

    dy = ((afWin[6] + afWin[7] + afWin[7] + afWin[8]) - 
          (afWin[0] + afWin[1] + afWin[1] + afWin[2]));

    aspect = (float) (atan2(dy,-dx) / degreesToRadians);

    if (dx == 0 && dy == 0)
    {
        /* Flat area */
        aspect = fDstNoDataValue;
    } 
    else if ( psData->bAngleAsAzimuth )
    {
        if (aspect > 90.0) 
            aspect = 450.0f - aspect;
        else
            aspect = 90.0f - aspect;
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

static
float GDALAspectZevenbergenThorneAlg (float* afWin, float fDstNoDataValue, void* pData)
{
    const double degreesToRadians = M_PI / 180.0;
    GDALAspectAlgData* psData = (GDALAspectAlgData*)pData;
    double dx, dy;
    float aspect;
    
    dx = (afWin[5] - afWin[3]);

    dy = (afWin[7] - afWin[1]);

    aspect = (float) (atan2(dy,-dx) / degreesToRadians);

    if (dx == 0 && dy == 0)
    {
        /* Flat area */
        aspect = fDstNoDataValue;
    } 
    else if ( psData->bAngleAsAzimuth )
    {
        if (aspect > 90.0) 
            aspect = 450.0f - aspect;
        else
            aspect = 90.0f - aspect;
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

static
void*  GDALCreateAspectData(int bAngleAsAzimuth)
{
    GDALAspectAlgData* pData =
        (GDALAspectAlgData*)CPLMalloc(sizeof(GDALAspectAlgData));
        
    pData->bAngleAsAzimuth = bAngleAsAzimuth;
    return pData;
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
    /* Sort NaN in first position */
    if( CPLIsNan(pC1->dfVal) )
        return -1;
    return (pC1->dfVal < pC2->dfVal) ? -1 :
           (pC1->dfVal == pC2->dfVal) ? 0 : 1;
}

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

    // Special case for NaN
    if( CPLIsNan(pasColorAssociation[0].dfVal) )
    {
        if( CPLIsNan(dfVal) )
        {
            *pnR = pasColorAssociation[0].nR;
            *pnG = pasColorAssociation[0].nG;
            *pnB = pasColorAssociation[0].nB;
            *pnA = pasColorAssociation[0].nA;
            return TRUE;
        }
        else
            lower = 1;
    }

    /* Find the index of the first element in the LUT input array that */
    /* is not smaller than the dfVal value. */
    while( true )
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
    VSILFILE* fpColorFile = VSIFOpenL(pszColorFilename, "rt");
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
    int bIsGMT_CPT = FALSE;
    while ((pszLine = CPLReadLineL(fpColorFile)) != NULL)
    {
        if (pszLine[0] == '#' && strstr(pszLine, "COLOR_MODEL"))
        {
            if (strstr(pszLine, "COLOR_MODEL = RGB") == NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Only COLOR_MODEL = RGB is supported");
                CPLFree(pasColorAssociation);
                *pnColors = 0;
                return NULL;
            }
            bIsGMT_CPT = TRUE;
        }

        char** papszFields = CSLTokenizeStringComplex(pszLine, " ,\t:", 
                                                      FALSE, FALSE );
        /* Skip comment lines */
        int nTokens = CSLCount(papszFields);
        if (nTokens >= 1 && (papszFields[0][0] == '#' ||
                             papszFields[0][0] == '/'))
        {
            CSLDestroy(papszFields);
            continue;
        }

        if (bIsGMT_CPT && nTokens == 8)
        {
            pasColorAssociation =
                    (ColorAssociation*)CPLRealloc(pasColorAssociation,
                           (nColorAssociation + 2) * sizeof(ColorAssociation));

            pasColorAssociation[nColorAssociation].dfVal = CPLAtof(papszFields[0]);
            pasColorAssociation[nColorAssociation].nR = atoi(papszFields[1]);
            pasColorAssociation[nColorAssociation].nG = atoi(papszFields[2]);
            pasColorAssociation[nColorAssociation].nB = atoi(papszFields[3]);
            pasColorAssociation[nColorAssociation].nA = 255;
            nColorAssociation++;

            pasColorAssociation[nColorAssociation].dfVal = CPLAtof(papszFields[4]);
            pasColorAssociation[nColorAssociation].nR = atoi(papszFields[5]);
            pasColorAssociation[nColorAssociation].nG = atoi(papszFields[6]);
            pasColorAssociation[nColorAssociation].nB = atoi(papszFields[7]);
            pasColorAssociation[nColorAssociation].nA = 255;
            nColorAssociation++;
        }
        else if (bIsGMT_CPT && nTokens == 4)
        {
            /* The first token might be B (background), F (foreground) or N (nodata) */
            /* Just interested in N */
            if (EQUAL(papszFields[0], "N") && bSrcHasNoData)
            {
                 pasColorAssociation =
                    (ColorAssociation*)CPLRealloc(pasColorAssociation,
                           (nColorAssociation + 1) * sizeof(ColorAssociation));

                pasColorAssociation[nColorAssociation].dfVal = dfSrcNoDataValue;
                pasColorAssociation[nColorAssociation].nR = atoi(papszFields[1]);
                pasColorAssociation[nColorAssociation].nG = atoi(papszFields[2]);
                pasColorAssociation[nColorAssociation].nB = atoi(papszFields[3]);
                pasColorAssociation[nColorAssociation].nA = 255;
                nColorAssociation++;
            }
        }
        else if (!bIsGMT_CPT && nTokens >= 2)
        {
            pasColorAssociation =
                    (ColorAssociation*)CPLRealloc(pasColorAssociation,
                           (nColorAssociation + 1) * sizeof(ColorAssociation));
            if (EQUAL(papszFields[0], "nv") && bSrcHasNoData)
                pasColorAssociation[nColorAssociation].dfVal = dfSrcNoDataValue;
            else if (strlen(papszFields[0]) > 1 && papszFields[0][strlen(papszFields[0])-1] == '%')
            {
                double dfPct = CPLAtof(papszFields[0]) / 100.;
                if (dfPct < 0.0 || dfPct > 1.0)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Wrong value for a percentage : %s", papszFields[0]);
                    CSLDestroy(papszFields);
                    VSIFCloseL(fpColorFile);
                    CPLFree(pasColorAssociation);
                    *pnColors = 0;
                    return NULL;
                }
                pasColorAssociation[nColorAssociation].dfVal =
                        GDALColorReliefGetAbsoluteValFromPct(hSrcBand, dfPct);
            }
            else
                pasColorAssociation[nColorAssociation].dfVal = CPLAtof(papszFields[0]);

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
                    VSIFCloseL(fpColorFile);
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
    VSIFCloseL(fpColorFile);

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

static
GByte* GDALColorReliefPrecompute(GDALRasterBandH hSrcBand,
                                 ColorAssociation* pasColorAssociation,
                                 int nColorAssociation,
                                 ColorSelectionMode eColorSelectionMode,
                                 int* pnIndexOffset)
{
    GDALDataType eDT = GDALGetRasterDataType(hSrcBand);
    GByte* pabyPrecomputed = NULL;
    int nIndexOffset = (eDT == GDT_Int16) ? 32768 : 0;
    *pnIndexOffset = nIndexOffset;
    int nXSize = GDALGetRasterBandXSize(hSrcBand);
    int nYSize = GDALGetRasterBandXSize(hSrcBand);
    if (eDT == GDT_Byte ||
        ((eDT == GDT_Int16 || eDT == GDT_UInt16) && nXSize * nYSize > 65536))
    {
        int iMax = (eDT == GDT_Byte) ? 256: 65536;
        pabyPrecomputed = (GByte*) VSIMalloc(4 * iMax);
        if (pabyPrecomputed)
        {
            int i;
            for(i=0;i<iMax;i++)
            {
                int nR, nG, nB, nA;
                GDALColorReliefGetRGBA  (pasColorAssociation,
                                         nColorAssociation,
                                         i - nIndexOffset,
                                         eColorSelectionMode,
                                         &nR, &nG, &nB, &nA);
                pabyPrecomputed[4 * i] = (GByte) nR;
                pabyPrecomputed[4 * i + 1] = (GByte) nG;
                pabyPrecomputed[4 * i + 2] = (GByte) nB;
                pabyPrecomputed[4 * i + 3] = (GByte) nA;
            }
        }
    }
    return pabyPrecomputed;
}

/************************************************************************/
/* ==================================================================== */
/*                       GDALColorReliefDataset                        */
/* ==================================================================== */
/************************************************************************/

class GDALColorReliefRasterBand;

class GDALColorReliefDataset : public GDALDataset
{
    friend class GDALColorReliefRasterBand;

    GDALDatasetH       hSrcDS;
    GDALRasterBandH    hSrcBand;
    int                nColorAssociation;
    ColorAssociation*  pasColorAssociation;
    ColorSelectionMode eColorSelectionMode;
    GByte*             pabyPrecomputed;
    int                nIndexOffset;
    float*             pafSourceBuf;
    int*               panSourceBuf;
    int                nCurBlockXOff;
    int                nCurBlockYOff;

  public:
                        GDALColorReliefDataset(GDALDatasetH hSrcDS,
                                            GDALRasterBandH hSrcBand,
                                            const char* pszColorFilename,
                                            ColorSelectionMode eColorSelectionMode,
                                            int bAlpha);
                       ~GDALColorReliefDataset();

    CPLErr      GetGeoTransform( double * padfGeoTransform );
    const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                    GDALColorReliefRasterBand                       */
/* ==================================================================== */
/************************************************************************/

class GDALColorReliefRasterBand : public GDALRasterBand
{
    friend class GDALColorReliefDataset;

    
  public:
                 GDALColorReliefRasterBand( GDALColorReliefDataset *, int );
    
    virtual CPLErr          IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
};

GDALColorReliefDataset::GDALColorReliefDataset(
                                     GDALDatasetH hSrcDSIn,
                                     GDALRasterBandH hSrcBandIn,
                                     const char* pszColorFilename,
                                     ColorSelectionMode eColorSelectionModeIn,
                                     int bAlpha)
{
    hSrcDS = hSrcDSIn;
    hSrcBand = hSrcBandIn;
    nColorAssociation = 0;
    pasColorAssociation =
            GDALColorReliefParseColorFile(hSrcBand, pszColorFilename,
                                          &nColorAssociation);
    eColorSelectionMode = eColorSelectionModeIn;
    
    nRasterXSize = GDALGetRasterXSize(hSrcDS);
    nRasterYSize = GDALGetRasterYSize(hSrcDS);
    
    int nBlockXSize, nBlockYSize;
    GDALGetBlockSize( hSrcBand, &nBlockXSize, &nBlockYSize);
    
    nIndexOffset = 0;
    pabyPrecomputed =
        GDALColorReliefPrecompute(hSrcBand,
                                  pasColorAssociation,
                                  nColorAssociation,
                                  eColorSelectionMode,
                                  &nIndexOffset);
    
    int i;
    for(i=0;i<((bAlpha) ? 4 : 3);i++)
    {
        SetBand(i + 1, new GDALColorReliefRasterBand(this, i+1));
    }
    
    pafSourceBuf = NULL;
    panSourceBuf = NULL;
    if (pabyPrecomputed)
        panSourceBuf = (int *) CPLMalloc(sizeof(int)*nBlockXSize*nBlockYSize);
    else
        pafSourceBuf = (float *) CPLMalloc(sizeof(float)*nBlockXSize*nBlockYSize);
    nCurBlockXOff = -1;
    nCurBlockYOff = -1;
}

GDALColorReliefDataset::~GDALColorReliefDataset()
{
    CPLFree(pasColorAssociation);
    CPLFree(pabyPrecomputed);
    CPLFree(panSourceBuf);
    CPLFree(pafSourceBuf);
}

CPLErr GDALColorReliefDataset::GetGeoTransform( double * padfGeoTransform )
{
    return GDALGetGeoTransform(hSrcDS, padfGeoTransform);
}

const char *GDALColorReliefDataset::GetProjectionRef()
{
    return GDALGetProjectionRef(hSrcDS);
}

GDALColorReliefRasterBand::GDALColorReliefRasterBand(
                                    GDALColorReliefDataset * poDSIn, int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = GDT_Byte;
    GDALGetBlockSize( poDSIn->hSrcBand, &nBlockXSize, &nBlockYSize);
}

CPLErr GDALColorReliefRasterBand::IReadBlock( int nBlockXOff,
                                              int nBlockYOff,
                                              void *pImage )
{
    GDALColorReliefDataset * poGDS = (GDALColorReliefDataset *) poDS;
    int nReqXSize, nReqYSize;

    if ((nBlockXOff + 1) * nBlockXSize >= nRasterXSize)
        nReqXSize = nRasterXSize - nBlockXOff * nBlockXSize;
    else
        nReqXSize = nBlockXSize;
        
    if ((nBlockYOff + 1) * nBlockYSize >= nRasterYSize)
        nReqYSize = nRasterYSize - nBlockYOff * nBlockYSize;
    else
        nReqYSize = nBlockYSize;

    if ( poGDS->nCurBlockXOff != nBlockXOff ||
         poGDS->nCurBlockYOff != nBlockYOff )
    {
        poGDS->nCurBlockXOff = nBlockXOff;
        poGDS->nCurBlockYOff = nBlockYOff;
        
        CPLErr eErr = GDALRasterIO( poGDS->hSrcBand,
                            GF_Read,
                            nBlockXOff * nBlockXSize,
                            nBlockYOff * nBlockYSize,
                            nReqXSize, nReqYSize,
                            (poGDS->panSourceBuf) ?
                                (void*) poGDS->panSourceBuf :
                                (void* )poGDS->pafSourceBuf,
                            nReqXSize, nReqYSize,
                            (poGDS->panSourceBuf) ? GDT_Int32 : GDT_Float32,
                            0, 0);
        if (eErr != CE_None)
        {
            memset(pImage, 0, nBlockXSize * nBlockYSize);
            return eErr;
        }
    }

    int x, y, j = 0;
    if (poGDS->panSourceBuf)
    {
        for( y = 0; y < nReqYSize; y++ )
        {
            for( x = 0; x < nReqXSize; x++ )
            {
                int nIndex = poGDS->panSourceBuf[j] + poGDS->nIndexOffset;
                ((GByte*)pImage)[y * nBlockXSize + x] = poGDS->pabyPrecomputed[4*nIndex + nBand-1];
                j++;
            }
        }
    }
    else
    {
        int anComponents[4];
        for( y = 0; y < nReqYSize; y++ )
        {
            for( x = 0; x < nReqXSize; x++ )
            {
                GDALColorReliefGetRGBA  (poGDS->pasColorAssociation,
                                        poGDS->nColorAssociation,
                                        poGDS->pafSourceBuf[j],
                                        poGDS->eColorSelectionMode,
                                        &anComponents[0],
                                        &anComponents[1],
                                        &anComponents[2],
                                        &anComponents[3]);
                ((GByte*)pImage)[y * nBlockXSize + x] = (GByte) anComponents[nBand-1];
                j++;
            }
        }
    }
    
    return CE_None;
}

GDALColorInterp GDALColorReliefRasterBand::GetColorInterpretation()
{
    return (GDALColorInterp)(GCI_RedBand + nBand - 1);
}


static
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
    
    if (hSrcBand == NULL || hDstBand1 == NULL || hDstBand2 == NULL ||
        hDstBand3 == NULL)
        return CE_Failure;

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
        
    int nR = 0, nG = 0, nB = 0, nA = 0;
    
/* -------------------------------------------------------------------- */
/*      Precompute the map from values to RGBA quadruplets              */
/*      for GDT_Byte, GDT_Int16 or GDT_UInt16                           */
/* -------------------------------------------------------------------- */
    int nIndexOffset = 0;
    GByte* pabyPrecomputed =
        GDALColorReliefPrecompute(hSrcBand,
                                  pasColorAssociation,
                                  nColorAssociation,
                                  eColorSelectionMode,
                                  &nIndexOffset);

/* -------------------------------------------------------------------- */
/*      Initialize progress counter.                                    */
/* -------------------------------------------------------------------- */

    float* pafSourceBuf = NULL;
    int* panSourceBuf = NULL;
    if (pabyPrecomputed)
        panSourceBuf = (int *) CPLMalloc(sizeof(int)*nXSize);
    else
        pafSourceBuf = (float *) CPLMalloc(sizeof(float)*nXSize);
    GByte* pabyDestBuf1  = (GByte*) CPLMalloc( 4 * nXSize );
    GByte* pabyDestBuf2  =  pabyDestBuf1 + nXSize;
    GByte* pabyDestBuf3  =  pabyDestBuf2 + nXSize;
    GByte* pabyDestBuf4  =  pabyDestBuf3 + nXSize;
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
        eErr = GDALRasterIO(   hSrcBand,
                        GF_Read,
                        0, i,
                        nXSize, 1,
                        (panSourceBuf) ? (void*) panSourceBuf : (void* )pafSourceBuf,
                        nXSize, 1,
                        (panSourceBuf) ? GDT_Int32 : GDT_Float32,
                        0, 0);
        if (eErr != CE_None)
            goto end;

        if (panSourceBuf)
        {
            for ( j = 0; j < nXSize; j++)
            {
                int nIndex = panSourceBuf[j] + nIndexOffset;
                pabyDestBuf1[j] = pabyPrecomputed[4 * nIndex];
                pabyDestBuf2[j] = pabyPrecomputed[4 * nIndex + 1];
                pabyDestBuf3[j] = pabyPrecomputed[4 * nIndex + 2];
                pabyDestBuf4[j] = pabyPrecomputed[4 * nIndex + 3];
            }
        }
        else
        {
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
                pabyDestBuf1[j] = (GByte) nR;
                pabyDestBuf2[j] = (GByte) nG;
                pabyDestBuf3[j] = (GByte) nB;
                pabyDestBuf4[j] = (GByte) nA;
            }
        }
        
        /* -----------------------------------------
         * Write Line to Raster
         */
        eErr = GDALRasterIO(hDstBand1,
                      GF_Write,
                      0, i, nXSize,
                      1, pabyDestBuf1, nXSize, 1, GDT_Byte, 0, 0);
        if (eErr != CE_None)
            goto end;

        eErr = GDALRasterIO(hDstBand2,
                      GF_Write,
                      0, i, nXSize,
                      1, pabyDestBuf2, nXSize, 1, GDT_Byte, 0, 0);
        if (eErr != CE_None)
            goto end;
            
        eErr = GDALRasterIO(hDstBand3,
                      GF_Write,
                      0, i, nXSize,
                      1, pabyDestBuf3, nXSize, 1, GDT_Byte, 0, 0);
        if (eErr != CE_None)
            goto end;
            
        if (hDstBand4)
        {
            eErr = GDALRasterIO(hDstBand4,
                        GF_Write,
                        0, i, nXSize,
                        1, pabyDestBuf4, nXSize, 1, GDT_Byte, 0, 0);
            if (eErr != CE_None)
                goto end;
        }

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
    VSIFree(pabyPrecomputed);
    CPLFree(pafSourceBuf);
    CPLFree(panSourceBuf);
    CPLFree(pabyDestBuf1);
    CPLFree(pasColorAssociation);

    return eErr;
}

/************************************************************************/
/*                     GDALGenerateVRTColorRelief()                     */
/************************************************************************/

static
CPLErr GDALGenerateVRTColorRelief(const char* pszDstFilename,
                               GDALDatasetH hSrcDataset,
                               GDALRasterBandH hSrcBand,
                               const char* pszColorFilename,
                               ColorSelectionMode eColorSelectionMode,
                               int bAddAlpha)
{

    int nColorAssociation = 0;
    ColorAssociation* pasColorAssociation =
            GDALColorReliefParseColorFile(hSrcBand, pszColorFilename,
                                          &nColorAssociation);
    if (pasColorAssociation == NULL)
        return CE_Failure;

    int nXSize = GDALGetRasterBandXSize(hSrcBand);
    int nYSize = GDALGetRasterBandYSize(hSrcBand);

    VSILFILE* fp = VSIFOpenL(pszDstFilename, "wt");
    if (fp == NULL)
    {
        CPLFree(pasColorAssociation);
        return CE_Failure;
    }

    bool bOK = VSIFPrintfL(fp, "<VRTDataset rasterXSize=\"%d\" rasterYSize=\"%d\">\n", nXSize, nYSize) > 0;
    const char* pszProjectionRef = GDALGetProjectionRef(hSrcDataset);
    if (pszProjectionRef && pszProjectionRef[0] != '\0')
    {
        char* pszEscapedString = CPLEscapeString(pszProjectionRef, -1, CPLES_XML);
        bOK &= VSIFPrintfL(fp, "  <SRS>%s</SRS>\n", pszEscapedString) > 0;
        VSIFree(pszEscapedString);
    }
    double adfGT[6];
    if (GDALGetGeoTransform(hSrcDataset, adfGT) == CE_None)
    {
        bOK &= VSIFPrintfL(fp, "  <GeoTransform> %.16g, %.16g, %.16g, "
                        "%.16g, %.16g, %.16g</GeoTransform>\n",
                        adfGT[0], adfGT[1], adfGT[2], adfGT[3], adfGT[4], adfGT[5]) > 0;
    }
    int nBands = 3 + (bAddAlpha ? 1 : 0);
    int iBand;

    int nBlockXSize, nBlockYSize;
    GDALGetBlockSize(hSrcBand, &nBlockXSize, &nBlockYSize);
    
    int bRelativeToVRT;
    CPLString osPath = CPLGetPath(pszDstFilename);
    char* pszSourceFilename = CPLStrdup(
        CPLExtractRelativePath( osPath.c_str(), GDALGetDescription(hSrcDataset), 
                                &bRelativeToVRT ));

    for(iBand = 0; iBand < nBands; iBand++)
    {
        bOK &= VSIFPrintfL(fp, "  <VRTRasterBand dataType=\"Byte\" band=\"%d\">\n", iBand + 1) > 0;
        bOK &= VSIFPrintfL(fp, "    <ColorInterp>%s</ColorInterp>\n",
                    GDALGetColorInterpretationName((GDALColorInterp)(GCI_RedBand + iBand))) > 0;
        bOK &= VSIFPrintfL(fp, "    <ComplexSource>\n") > 0;
        bOK &= VSIFPrintfL(fp, "      <SourceFilename relativeToVRT=\"%d\">%s</SourceFilename>\n",
                        bRelativeToVRT, pszSourceFilename) > 0;
        bOK &= VSIFPrintfL(fp, "      <SourceBand>%d</SourceBand>\n", GDALGetBandNumber(hSrcBand)) > 0;
        bOK &= VSIFPrintfL(fp, "      <SourceProperties RasterXSize=\"%d\" "
                        "RasterYSize=\"%d\" DataType=\"%s\" "
                        "BlockXSize=\"%d\" BlockYSize=\"%d\"/>\n",
                        nXSize, nYSize,
                        GDALGetDataTypeName(GDALGetRasterDataType(hSrcBand)),
                        nBlockXSize, nBlockYSize) > 0;
        bOK &= VSIFPrintfL(fp, "      <SrcRect xOff=\"0\" yOff=\"0\" xSize=\"%d\" ySize=\"%d\"/>\n",
                        nXSize, nYSize) > 0;
        bOK &= VSIFPrintfL(fp, "      <DstRect xOff=\"0\" yOff=\"0\" xSize=\"%d\" ySize=\"%d\"/>\n",
                        nXSize, nYSize) > 0;

        bOK &= VSIFPrintfL(fp, "      <LUT>") > 0;
        int iColor;
#define EPSILON 1e-8
        for(iColor=0;iColor<nColorAssociation;iColor++)
        {
            if (eColorSelectionMode == COLOR_SELECTION_NEAREST_ENTRY)
            {
                if (iColor > 1)
                    bOK &= VSIFPrintfL(fp, ",") > 0;
            }
            else if (iColor > 0)
                bOK &= VSIFPrintfL(fp, ",") > 0;

            double dfVal = pasColorAssociation[iColor].dfVal;

            if (eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY)
            {
                bOK &= VSIFPrintfL(fp, "%.12g:0,", dfVal - EPSILON) > 0;
            }
            else if (iColor > 0 &&
                     eColorSelectionMode == COLOR_SELECTION_NEAREST_ENTRY)
            {
                double dfMidVal = (dfVal + pasColorAssociation[iColor-1].dfVal) / 2;
                bOK &= VSIFPrintfL(fp, "%.12g:%d", dfMidVal - EPSILON,
                        (iBand == 0) ? pasColorAssociation[iColor-1].nR :
                        (iBand == 1) ? pasColorAssociation[iColor-1].nG :
                        (iBand == 2) ? pasColorAssociation[iColor-1].nB :
                                       pasColorAssociation[iColor-1].nA) > 0;
                bOK &= VSIFPrintfL(fp, ",%.12g:%d", dfMidVal ,
                        (iBand == 0) ? pasColorAssociation[iColor].nR :
                        (iBand == 1) ? pasColorAssociation[iColor].nG :
                        (iBand == 2) ? pasColorAssociation[iColor].nB :
                                       pasColorAssociation[iColor].nA) > 0;

            }

            if (eColorSelectionMode != COLOR_SELECTION_NEAREST_ENTRY)
            {
                if (dfVal != (double)(int)dfVal)
                    bOK &= VSIFPrintfL(fp, "%.12g", dfVal) > 0;
                else
                    bOK &= VSIFPrintfL(fp, "%d", (int)dfVal) > 0;
                bOK &= VSIFPrintfL(fp, ":%d",
                            (iBand == 0) ? pasColorAssociation[iColor].nR :
                            (iBand == 1) ? pasColorAssociation[iColor].nG :
                            (iBand == 2) ? pasColorAssociation[iColor].nB :
                                           pasColorAssociation[iColor].nA) > 0;
            }

            if (eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY)
            {
                bOK &= VSIFPrintfL(fp, ",%.12g:0", dfVal + EPSILON) > 0;
            }

        }
        bOK &= VSIFPrintfL(fp, "</LUT>\n") > 0;

        bOK &= VSIFPrintfL(fp, "    </ComplexSource>\n") > 0;
        bOK &= VSIFPrintfL(fp, "  </VRTRasterBand>\n") > 0;
    }

    CPLFree(pszSourceFilename);

    bOK &= VSIFPrintfL(fp, "</VRTDataset>\n") > 0;

    VSIFCloseL(fp);

    CPLFree(pasColorAssociation);

    return (bOK) ? CE_None : CE_Failure;
}


/************************************************************************/
/*                         GDALTRIAlg()                                 */
/************************************************************************/

static
float GDALTRIAlg (float* afWin,
                  CPL_UNUSED float fDstNoDataValue,
                  CPL_UNUSED void* pData)
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


/************************************************************************/
/*                         GDALTPIAlg()                                 */
/************************************************************************/

static
float GDALTPIAlg (float* afWin,
                  CPL_UNUSED float fDstNoDataValue,
                  CPL_UNUSED void* pData)
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

/************************************************************************/
/*                     GDALRoughnessAlg()                               */
/************************************************************************/

static
float GDALRoughnessAlg (float* afWin, CPL_UNUSED float fDstNoDataValue, CPL_UNUSED void* pData)
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

/************************************************************************/
/* ==================================================================== */
/*                       GDALGeneric3x3Dataset                        */
/* ==================================================================== */
/************************************************************************/

class GDALGeneric3x3RasterBand;

class GDALGeneric3x3Dataset : public GDALDataset
{
    friend class GDALGeneric3x3RasterBand;

    GDALGeneric3x3ProcessingAlg pfnAlg;
    void*              pAlgData;
    GDALDatasetH       hSrcDS;
    GDALRasterBandH    hSrcBand;
    float*             apafSourceBuf[3];
    int                bDstHasNoData;
    double             dfDstNoDataValue;
    int                nCurLine;
    int                bComputeAtEdges;

  public:
                        GDALGeneric3x3Dataset(GDALDatasetH hSrcDS,
                                              GDALRasterBandH hSrcBand,
                                              GDALDataType eDstDataType,
                                              int bDstHasNoData,
                                              double dfDstNoDataValue,
                                              GDALGeneric3x3ProcessingAlg pfnAlg,
                                              void* pAlgData,
                                              int bComputeAtEdges);
                       ~GDALGeneric3x3Dataset();

    CPLErr      GetGeoTransform( double * padfGeoTransform );
    const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                    GDALGeneric3x3RasterBand                       */
/* ==================================================================== */
/************************************************************************/

class GDALGeneric3x3RasterBand : public GDALRasterBand
{
    friend class GDALGeneric3x3Dataset;
    int bSrcHasNoData;
    float fSrcNoDataValue;
    int bIsSrcNoDataNan;
    
    void                    InitWidthNoData(void* pImage);
    
  public:
                 GDALGeneric3x3RasterBand( GDALGeneric3x3Dataset *poDS,
                                           GDALDataType eDstDataType );
    
    virtual CPLErr          IReadBlock( int, int, void * );
    virtual double          GetNoDataValue( int* pbHasNoData );
};

GDALGeneric3x3Dataset::GDALGeneric3x3Dataset(
                                     GDALDatasetH hSrcDSIn,
                                     GDALRasterBandH hSrcBandIn,
                                     GDALDataType eDstDataType,
                                     int bDstHasNoDataIn,
                                     double dfDstNoDataValueIn,
                                     GDALGeneric3x3ProcessingAlg pfnAlgIn,
                                     void* pAlgDataIn,
                                     int bComputeAtEdgesIn)
{
    hSrcDS = hSrcDSIn;
    hSrcBand = hSrcBandIn;
    pfnAlg = pfnAlgIn;
    pAlgData = pAlgDataIn;
    bDstHasNoData = bDstHasNoDataIn;
    dfDstNoDataValue = dfDstNoDataValueIn;
    bComputeAtEdges = bComputeAtEdgesIn;
    
    CPLAssert(eDstDataType == GDT_Byte || eDstDataType == GDT_Float32);

    nRasterXSize = GDALGetRasterXSize(hSrcDS);
    nRasterYSize = GDALGetRasterYSize(hSrcDS);
    
    SetBand(1, new GDALGeneric3x3RasterBand(this, eDstDataType));
    
    apafSourceBuf[0] = (float *) CPLMalloc(sizeof(float)*nRasterXSize);
    apafSourceBuf[1] = (float *) CPLMalloc(sizeof(float)*nRasterXSize);
    apafSourceBuf[2] = (float *) CPLMalloc(sizeof(float)*nRasterXSize);

    nCurLine = -1;
}

GDALGeneric3x3Dataset::~GDALGeneric3x3Dataset()
{
    CPLFree(apafSourceBuf[0]);
    CPLFree(apafSourceBuf[1]);
    CPLFree(apafSourceBuf[2]);
}

CPLErr GDALGeneric3x3Dataset::GetGeoTransform( double * padfGeoTransform )
{
    return GDALGetGeoTransform(hSrcDS, padfGeoTransform);
}

const char *GDALGeneric3x3Dataset::GetProjectionRef()
{
    return GDALGetProjectionRef(hSrcDS);
}

GDALGeneric3x3RasterBand::GDALGeneric3x3RasterBand(GDALGeneric3x3Dataset *poDSIn,
                                                   GDALDataType eDstDataType)
{
    poDS = poDSIn;
    this->nBand = 1;
    eDataType = eDstDataType;
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    bSrcHasNoData = FALSE;
    fSrcNoDataValue = (float)GDALGetRasterNoDataValue(poDSIn->hSrcBand,
                                                      &bSrcHasNoData);
    bIsSrcNoDataNan = bSrcHasNoData && CPLIsNan(fSrcNoDataValue);
}

void   GDALGeneric3x3RasterBand::InitWidthNoData(void* pImage)
{
    int j;
    GDALGeneric3x3Dataset * poGDS = (GDALGeneric3x3Dataset *) poDS;
    if (eDataType == GDT_Byte)
    {
        for(j=0;j<nBlockXSize;j++)
            ((GByte*)pImage)[j] = (GByte) poGDS->dfDstNoDataValue;
    }
    else
    {
        for(j=0;j<nBlockXSize;j++)
            ((float*)pImage)[j] = (float) poGDS->dfDstNoDataValue;
    }
}

CPLErr GDALGeneric3x3RasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                             int nBlockYOff,
                                             void *pImage )
{
    int i, j;
    float fVal;
    GDALGeneric3x3Dataset * poGDS = (GDALGeneric3x3Dataset *) poDS;

    if (poGDS->bComputeAtEdges && nRasterXSize >= 2 && nRasterYSize >= 2)
    {
        if (nBlockYOff == 0)
        {
            for(i=0;i<2;i++)
            {
                CPLErr eErr = GDALRasterIO( poGDS->hSrcBand,
                                    GF_Read,
                                    0, i, nBlockXSize, 1,
                                    poGDS->apafSourceBuf[i+1],
                                    nBlockXSize, 1,
                                    GDT_Float32,
                                    0, 0);
                if (eErr != CE_None)
                {
                    InitWidthNoData(pImage);
                    return eErr;
                }
            }
            poGDS->nCurLine = 0;

            for (j = 0; j < nRasterXSize; j++)
            {
                float afWin[9];
                int jmin = (j == 0) ? j : j - 1;
                int jmax = (j == nRasterXSize - 1) ? j : j + 1;

                afWin[0] = INTERPOL(poGDS->apafSourceBuf[1][jmin], poGDS->apafSourceBuf[2][jmin]);
                afWin[1] = INTERPOL(poGDS->apafSourceBuf[1][j],    poGDS->apafSourceBuf[2][j]);
                afWin[2] = INTERPOL(poGDS->apafSourceBuf[1][jmax], poGDS->apafSourceBuf[2][jmax]);
                afWin[3] = poGDS->apafSourceBuf[1][jmin];
                afWin[4] = poGDS->apafSourceBuf[1][j];
                afWin[5] = poGDS->apafSourceBuf[1][jmax];
                afWin[6] = poGDS->apafSourceBuf[2][jmin];
                afWin[7] = poGDS->apafSourceBuf[2][j];
                afWin[8] = poGDS->apafSourceBuf[2][jmax];

                fVal = ComputeVal(bSrcHasNoData, fSrcNoDataValue,
                                  bIsSrcNoDataNan,
                                    afWin, (float) poGDS->dfDstNoDataValue,
                                    poGDS->pfnAlg,
                                    poGDS->pAlgData,
                                    poGDS->bComputeAtEdges);

                if (eDataType == GDT_Byte)
                    ((GByte*)pImage)[j] = (GByte) (fVal + 0.5);
                else
                    ((float*)pImage)[j] = fVal;
            }

            return CE_None;
        }
        else if (nBlockYOff == nRasterYSize - 1)
        {
            if (poGDS->nCurLine != nRasterYSize - 2)
            {
                for(i=0;i<2;i++)
                {
                    CPLErr eErr = GDALRasterIO( poGDS->hSrcBand,
                                        GF_Read,
                                        0, nRasterYSize - 2 + i, nBlockXSize, 1,
                                        poGDS->apafSourceBuf[i+1],
                                        nBlockXSize, 1,
                                        GDT_Float32,
                                        0, 0);
                    if (eErr != CE_None)
                    {
                        InitWidthNoData(pImage);
                        return eErr;
                    }
                }
            }

            for (j = 0; j < nRasterXSize; j++)
            {
                float afWin[9];
                int jmin = (j == 0) ? j : j - 1;
                int jmax = (j == nRasterXSize - 1) ? j : j + 1;

                afWin[0] = poGDS->apafSourceBuf[1][jmin];
                afWin[1] = poGDS->apafSourceBuf[1][j];
                afWin[2] = poGDS->apafSourceBuf[1][jmax];
                afWin[3] = poGDS->apafSourceBuf[2][jmin];
                afWin[4] = poGDS->apafSourceBuf[2][j];
                afWin[5] = poGDS->apafSourceBuf[2][jmax];
                afWin[6] = INTERPOL(poGDS->apafSourceBuf[2][jmin], poGDS->apafSourceBuf[1][jmin]);
                afWin[7] = INTERPOL(poGDS->apafSourceBuf[2][j],    poGDS->apafSourceBuf[1][j]);
                afWin[8] = INTERPOL(poGDS->apafSourceBuf[2][jmax], poGDS->apafSourceBuf[1][jmax]);

                fVal = ComputeVal(bSrcHasNoData, fSrcNoDataValue,
                                  bIsSrcNoDataNan,
                                    afWin, (float) poGDS->dfDstNoDataValue,
                                    poGDS->pfnAlg,
                                    poGDS->pAlgData,
                                    poGDS->bComputeAtEdges);

                if (eDataType == GDT_Byte)
                    ((GByte*)pImage)[j] = (GByte) (fVal + 0.5);
                else
                    ((float*)pImage)[j] = fVal;
            }

            return CE_None;
        }
    }
    else if ( nBlockYOff == 0 || nBlockYOff == nRasterYSize - 1)
    {
        InitWidthNoData(pImage);
        return CE_None;
    }

    if ( poGDS->nCurLine != nBlockYOff )
    {
        if (poGDS->nCurLine + 1 == nBlockYOff)
        {
            float* pafTmp =  poGDS->apafSourceBuf[0];
            poGDS->apafSourceBuf[0] = poGDS->apafSourceBuf[1];
            poGDS->apafSourceBuf[1] = poGDS->apafSourceBuf[2];
            poGDS->apafSourceBuf[2] = pafTmp;

            CPLErr eErr = GDALRasterIO( poGDS->hSrcBand,
                                    GF_Read,
                                    0, nBlockYOff + 1, nBlockXSize, 1,
                                    poGDS->apafSourceBuf[2],
                                    nBlockXSize, 1,
                                    GDT_Float32,
                                    0, 0);

            if (eErr != CE_None)
            {
                InitWidthNoData(pImage);
                return eErr;
            }
        }
        else
        {
            for(i=0;i<3;i++)
            {
                CPLErr eErr = GDALRasterIO( poGDS->hSrcBand,
                                    GF_Read,
                                    0, nBlockYOff + i - 1, nBlockXSize, 1,
                                    poGDS->apafSourceBuf[i],
                                    nBlockXSize, 1,
                                    GDT_Float32,
                                    0, 0);
                if (eErr != CE_None)
                {
                    InitWidthNoData(pImage);
                    return eErr;
                }
            }
        }

        poGDS->nCurLine = nBlockYOff;
    }

    if (poGDS->bComputeAtEdges && nRasterXSize >= 2)
    {
        float afWin[9];

        j = 0;
        afWin[0] = INTERPOL(poGDS->apafSourceBuf[0][j], poGDS->apafSourceBuf[0][j+1]);
        afWin[1] = poGDS->apafSourceBuf[0][j];
        afWin[2] = poGDS->apafSourceBuf[0][j+1];
        afWin[3] = INTERPOL(poGDS->apafSourceBuf[1][j], poGDS->apafSourceBuf[1][j+1]);
        afWin[4] = poGDS->apafSourceBuf[1][j];
        afWin[5] = poGDS->apafSourceBuf[1][j+1];
        afWin[6] = INTERPOL(poGDS->apafSourceBuf[2][j], poGDS->apafSourceBuf[2][j+1]);
        afWin[7] = poGDS->apafSourceBuf[2][j];
        afWin[8] = poGDS->apafSourceBuf[2][j+1];

        fVal = ComputeVal(bSrcHasNoData, fSrcNoDataValue,
                          bIsSrcNoDataNan,
                                    afWin, (float) poGDS->dfDstNoDataValue,
                                    poGDS->pfnAlg,
                                    poGDS->pAlgData,
                                    poGDS->bComputeAtEdges);

        if (eDataType == GDT_Byte)
            ((GByte*)pImage)[j] = (GByte) (fVal + 0.5);
        else
            ((float*)pImage)[j] = fVal;

        j = nRasterXSize - 1;

        afWin[0] = poGDS->apafSourceBuf[0][j-1];
        afWin[1] = poGDS->apafSourceBuf[0][j];
        afWin[2] = INTERPOL(poGDS->apafSourceBuf[0][j], poGDS->apafSourceBuf[0][j-1]);
        afWin[3] = poGDS->apafSourceBuf[1][j-1];
        afWin[4] = poGDS->apafSourceBuf[1][j];
        afWin[5] = INTERPOL(poGDS->apafSourceBuf[1][j], poGDS->apafSourceBuf[1][j-1]);
        afWin[6] = poGDS->apafSourceBuf[2][j-1];
        afWin[7] = poGDS->apafSourceBuf[2][j];
        afWin[8] = INTERPOL(poGDS->apafSourceBuf[2][j], poGDS->apafSourceBuf[2][j-1]);

        fVal = ComputeVal(bSrcHasNoData, fSrcNoDataValue,
                          bIsSrcNoDataNan,
                                    afWin, (float) poGDS->dfDstNoDataValue,
                                    poGDS->pfnAlg,
                                    poGDS->pAlgData,
                                    poGDS->bComputeAtEdges);

        if (eDataType == GDT_Byte)
            ((GByte*)pImage)[j] = (GByte) (fVal + 0.5);
        else
            ((float*)pImage)[j] = fVal;
    }
    else
    {
        if (eDataType == GDT_Byte)
        {
            ((GByte*)pImage)[0] = (GByte) poGDS->dfDstNoDataValue;
            if (nBlockXSize > 1)
                ((GByte*)pImage)[nBlockXSize - 1] = (GByte) poGDS->dfDstNoDataValue;
        }
        else
        {
            ((float*)pImage)[0] = (float) poGDS->dfDstNoDataValue;
            if (nBlockXSize > 1)
                ((float*)pImage)[nBlockXSize - 1] = (float) poGDS->dfDstNoDataValue;
        }
    }


    for(j=1;j<nBlockXSize - 1;j++)
    {
        float afWin[9];
        afWin[0] = poGDS->apafSourceBuf[0][j-1];
        afWin[1] = poGDS->apafSourceBuf[0][j];
        afWin[2] = poGDS->apafSourceBuf[0][j+1];
        afWin[3] = poGDS->apafSourceBuf[1][j-1];
        afWin[4] = poGDS->apafSourceBuf[1][j];
        afWin[5] = poGDS->apafSourceBuf[1][j+1];
        afWin[6] = poGDS->apafSourceBuf[2][j-1];
        afWin[7] = poGDS->apafSourceBuf[2][j];
        afWin[8] = poGDS->apafSourceBuf[2][j+1];

        fVal = ComputeVal(bSrcHasNoData, fSrcNoDataValue,
                          bIsSrcNoDataNan,
                                afWin, (float) poGDS->dfDstNoDataValue,
                                poGDS->pfnAlg,
                                poGDS->pAlgData,
                                poGDS->bComputeAtEdges);

        if (eDataType == GDT_Byte)
            ((GByte*)pImage)[j] = (GByte) (fVal + 0.5);
        else
            ((float*)pImage)[j] = fVal;

    }

    return CE_None;
}

double GDALGeneric3x3RasterBand::GetNoDataValue( int* pbHasNoData )
{
    GDALGeneric3x3Dataset * poGDS = (GDALGeneric3x3Dataset *) poDS;
    if (pbHasNoData)
        *pbHasNoData = poGDS->bDstHasNoData;
    return poGDS->dfDstNoDataValue;
}

/************************************************************************/
/*                            ArgIsNumeric()                            */
/************************************************************************/

static int ArgIsNumeric( const char *pszArg )

{
    return CPLGetValueType(pszArg) != CPL_VALUE_STRING;
}

/************************************************************************/
/*                            GetAlgorithm()                            */
/************************************************************************/

typedef enum
{
    INVALID,
    HILL_SHADE,
    SLOPE,
    ASPECT,
    COLOR_RELIEF,
    TRI,
    TPI,
    ROUGHNESS
} Algorithm;

static Algorithm GetAlgorithm(const char* pszProcessing)
{
    if ( EQUAL(pszProcessing, "shade") || EQUAL(pszProcessing, "hillshade") )
    {
        return HILL_SHADE;
    }
    else if ( EQUAL(pszProcessing, "slope") )
    {
        return SLOPE;
    }
    else if ( EQUAL(pszProcessing, "aspect") )
    {
        return ASPECT;
    }
    else if ( EQUAL(pszProcessing, "color-relief") )
    {
        return COLOR_RELIEF;
    }
    else if ( EQUAL(pszProcessing, "TRI") )
    {
        return TRI;
    }
    else if ( EQUAL(pszProcessing, "TPI") )
    {
        return TPI;
    }
    else if ( EQUAL(pszProcessing, "roughness") )
    {
        return ROUGHNESS;
    }
    else
    {
        return INVALID;
    }
}

/************************************************************************/
/*                            GDALDEMProcessing()                       */
/************************************************************************/

/**
 * Apply a DEM processing.
 *
 * This is the equivalent of the <a href="gdaldem.html">gdaldem</a> utility.
 *
 * GDALDEMProcessingOptions* must be allocated and freed with
 * GDALDEMProcessingOptionsNew() and GDALDEMProcessingOptionsFree()
 * respectively.
 *
 * @param pszDest the destination dataset path.
 * @param hSrcDataset the source dataset handle.
 * @param pszProcessing the processing to apply (one of "hillshade", "slope",
 * "aspect", "color-relief", "TRI", "TPI", "Roughness")
 * @param pszColorFilename color file (mandatory for "color-relief" processing,
 * should be NULL otherwise)
 * @param psOptionsIn the options struct returned by
 * GDALDEMProcessingOptionsNew() or NULL.
 * @param pbUsageError the pointer to int variable to determine any usage
 * error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using
 * GDALClose()) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALDEMProcessing(const char *pszDest,
                               GDALDatasetH hSrcDataset,
                               const char* pszProcessing,
                               const char* pszColorFilename,
                               const GDALDEMProcessingOptions *psOptionsIn,
                               int *pbUsageError)
{
    if( hSrcDataset == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No source dataset specified.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }
    if( pszDest == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No target dataset specified.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }
    if( pszProcessing == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No target dataset specified.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }

    Algorithm eUtilityMode = GetAlgorithm(pszProcessing);
    if( eUtilityMode == INVALID )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Invalid processing");
        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }

    if( eUtilityMode == COLOR_RELIEF && pszColorFilename == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "pszColorFilename == NULL.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }
    else if( eUtilityMode != COLOR_RELIEF && pszColorFilename != NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "pszColorFilename != NULL.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }


    GDALDEMProcessingOptions* psOptionsToFree = NULL;
    const GDALDEMProcessingOptions* psOptions;
    if( psOptionsIn )
        psOptions = psOptionsIn;
    else
    {
        psOptionsToFree = GDALDEMProcessingOptionsNew(NULL, NULL);
        psOptions = psOptionsToFree;
    }

    double  adfGeoTransform[6];

    GDALDatasetH hDstDataset = NULL;
    GDALRasterBandH hSrcBand = NULL;
    GDALRasterBandH hDstBand = NULL;
    GDALDriverH hDriver = NULL;

    int nXSize = GDALGetRasterXSize(hSrcDataset);
    int nYSize = GDALGetRasterYSize(hSrcDataset);

    if( psOptions->nBand <= 0 || psOptions->nBand > GDALGetRasterCount(hSrcDataset) )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Unable to fetch band #%d", psOptions->nBand );
        GDALDEMProcessingOptionsFree(psOptionsToFree);
        return NULL;
    }
    hSrcBand = GDALGetRasterBand( hSrcDataset, psOptions->nBand );

    GDALGetGeoTransform(hSrcDataset, adfGeoTransform);

    hDriver = GDALGetDriverByName(psOptions->pszFormat);
    if( hDriver == NULL 
        || (GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) == NULL &&
            GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY, NULL ) == NULL))
    {
        int	iDr;

        CPLError(CE_Failure, CPLE_AppDefined, "Output driver `%s' not recognised to have output support.", 
                 psOptions->pszFormat );
        fprintf( stderr, "The following format drivers are configured\n"
                "and support output:\n" );

        for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
        {
            hDriver = GDALGetDriver(iDr);

            if( GDALGetMetadataItem( hDriver, GDAL_DCAP_RASTER, NULL) != NULL &&
                (GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) != NULL ||
                 GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY, NULL ) != NULL) )
            {
                fprintf( stderr, "  %s: %s\n",
                        GDALGetDriverShortName( hDriver  ),
                        GDALGetDriverLongName( hDriver ) );
            }
        }
        GDALDEMProcessingOptionsFree(psOptionsToFree);
        return NULL;
    }

    double dfDstNoDataValue = 0;
    int bDstHasNoData = FALSE;
    void* pData = NULL;
    GDALGeneric3x3ProcessingAlg pfnAlg = NULL;

    if (eUtilityMode == HILL_SHADE)
    {
        dfDstNoDataValue = 0;
        bDstHasNoData = TRUE;
        pData = GDALCreateHillshadeData   (adfGeoTransform,
                                           psOptions->z,
                                           psOptions->scale,
                                           psOptions->alt,
                                           psOptions->az,
                                           psOptions->bZevenbergenThorne);
        if (psOptions->bZevenbergenThorne)
        {
            if(!psOptions->bCombined)
                pfnAlg = GDALHillshadeZevenbergenThorneAlg;
            else
                pfnAlg = GDALHillshadeZevenbergenThorneCombinedAlg;
        }
        else
        {
            if(!psOptions->bCombined)
                pfnAlg = GDALHillshadeAlg;
            else
                pfnAlg = GDALHillshadeCombinedAlg;
        }
    }
    else if (eUtilityMode == SLOPE)
    {
        dfDstNoDataValue = -9999;
        bDstHasNoData = TRUE;

        pData = GDALCreateSlopeData(adfGeoTransform, psOptions->scale, psOptions->slopeFormat);
        if (psOptions->bZevenbergenThorne)
            pfnAlg = GDALSlopeZevenbergenThorneAlg;
        else
            pfnAlg = GDALSlopeHornAlg;
    }

    else if (eUtilityMode == ASPECT)
    {
        if (!psOptions->bZeroForFlat)
        {
            dfDstNoDataValue = -9999;
            bDstHasNoData = TRUE;
        }

        pData = GDALCreateAspectData(psOptions->bAngleAsAzimuth);
        if (psOptions->bZevenbergenThorne)
            pfnAlg = GDALAspectZevenbergenThorneAlg;
        else
            pfnAlg = GDALAspectAlg;
    }
    else if (eUtilityMode == TRI)
    {
        dfDstNoDataValue = -9999;
        bDstHasNoData = TRUE;
        pfnAlg = GDALTRIAlg;
    }
    else if (eUtilityMode == TPI)
    {
        dfDstNoDataValue = -9999;
        bDstHasNoData = TRUE;
        pfnAlg = GDALTPIAlg;
    }
    else if (eUtilityMode == ROUGHNESS)
    {
        dfDstNoDataValue = -9999;
        bDstHasNoData = TRUE;
        pfnAlg = GDALRoughnessAlg;
    }
    
    GDALDataType eDstDataType = (eUtilityMode == HILL_SHADE ||
                                 eUtilityMode == COLOR_RELIEF) ? GDT_Byte :
                                                               GDT_Float32;

    if( EQUAL(psOptions->pszFormat, "VRT") )
    {
        if (eUtilityMode == COLOR_RELIEF)
        {
            GDALGenerateVRTColorRelief(pszDest,
                                       hSrcDataset,
                                       hSrcBand,
                                       pszColorFilename,
                                       psOptions->eColorSelectionMode,
                                       psOptions->bAddAlpha);

            CPLFree(pData);

            GDALDEMProcessingOptionsFree(psOptionsToFree);
            return GDALOpen(pszDest, GA_Update);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "VRT driver can only be used with color-relief utility.");
            GDALDEMProcessingOptionsFree(psOptionsToFree);
            CPLFree(pData);
            return NULL;
        }
    }
    
    // We might actually want to always go through the intermediate dataset
    int bForceUseIntermediateDataset = FALSE;
    
    GDALProgressFunc pfnProgress = psOptions->pfnProgress;
    void* pProgressData = psOptions->pProgressData;
    
    if( EQUAL(psOptions->pszFormat, "GTiff") )
    {
        if( !EQUAL(CSLFetchNameValueDef(psOptions->papszCreateOptions, "COMPRESS", "NONE"), "NONE") &&
            CSLTestBoolean(CSLFetchNameValueDef(psOptions->papszCreateOptions, "TILED", "NO")) )
        {
            bForceUseIntermediateDataset = TRUE;
        }
        else if( strcmp(pszDest, "/vsistdout/") == 0 )
        {
            bForceUseIntermediateDataset = TRUE;
            pfnProgress = GDALDummyProgress;
            pProgressData = NULL;
        }
#ifdef S_ISFIFO
        else
        {
            VSIStatBufL sStat;
            if( VSIStatExL(pszDest, &sStat, VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0 &&
                S_ISFIFO(sStat.st_mode) )
            {
                bForceUseIntermediateDataset = TRUE;
            }
        }
#endif
    }

    if( GDALGetMetadataItem( hDriver, GDAL_DCAP_RASTER, NULL) != NULL &&
        ((bForceUseIntermediateDataset || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) == NULL) &&
         GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY, NULL ) != NULL) )
    {
        GDALDatasetH hIntermediateDataset;
        
        if (eUtilityMode == COLOR_RELIEF)
            hIntermediateDataset = (GDALDatasetH)
                new GDALColorReliefDataset (hSrcDataset,
                                            hSrcBand,
                                            pszColorFilename,
                                            psOptions->eColorSelectionMode,
                                            psOptions->bAddAlpha);
        else
            hIntermediateDataset = (GDALDatasetH)
                new GDALGeneric3x3Dataset(hSrcDataset, hSrcBand,
                                          eDstDataType,
                                          bDstHasNoData,
                                          dfDstNoDataValue,
                                          pfnAlg,
                                          pData,
                                          psOptions->bComputeAtEdges);

        GDALDatasetH hOutDS = GDALCreateCopy(
                                 hDriver, pszDest, hIntermediateDataset, 
                                 TRUE, psOptions->papszCreateOptions, 
                                 pfnProgress, pProgressData );

        GDALClose(hIntermediateDataset);

        CPLFree(pData);

        GDALDEMProcessingOptionsFree(psOptionsToFree);
        return hOutDS;
    }

    int nDstBands;
    if (eUtilityMode == COLOR_RELIEF)
        nDstBands = (psOptions->bAddAlpha) ? 4 : 3;
    else
        nDstBands = 1;

    hDstDataset = GDALCreate(   hDriver,
                                pszDest,
                                nXSize,
                                nYSize,
                                nDstBands,
                                eDstDataType,
                                psOptions->papszCreateOptions);

    if( hDstDataset == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to create dataset %s", pszDest );
        GDALDEMProcessingOptionsFree(psOptionsToFree);
        CPLFree(pData);
        return NULL;
    }
    
    hDstBand = GDALGetRasterBand( hDstDataset, 1 );

    GDALSetGeoTransform(hDstDataset, adfGeoTransform);
    GDALSetProjection(hDstDataset, GDALGetProjectionRef(hSrcDataset));
    
    if (eUtilityMode == COLOR_RELIEF)
    {
        GDALColorRelief (hSrcBand, 
                         GDALGetRasterBand(hDstDataset, 1),
                         GDALGetRasterBand(hDstDataset, 2),
                         GDALGetRasterBand(hDstDataset, 3),
                         (psOptions->bAddAlpha) ? GDALGetRasterBand(hDstDataset, 4) : NULL,
                         pszColorFilename,
                         psOptions->eColorSelectionMode,
                         pfnProgress, pProgressData);
    }
    else
    {
        if (bDstHasNoData)
            GDALSetRasterNoDataValue(hDstBand, dfDstNoDataValue);

        GDALGeneric3x3Processing(hSrcBand, hDstBand,
                                 pfnAlg, pData,
                                 psOptions->bComputeAtEdges,
                                 pfnProgress, pProgressData);

    }

    CPLFree(pData);

    GDALDEMProcessingOptionsFree(psOptionsToFree);
    return hDstDataset;
}

/************************************************************************/
/*                           GDALDEMProcessingOptionsNew()              */
/************************************************************************/

/**
 * Allocates a GDALDEMProcessingOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="gdal_translate.html">gdal_translate</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be NULL),
 *                           otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALDEMProcessingOptionsForBinaryNew() prior to this function. Will be
 *                           filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALDEMProcessingOptions struct. Must be freed with GDALDEMProcessingOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALDEMProcessingOptions *GDALDEMProcessingOptionsNew(char** papszArgv,
                                                      GDALDEMProcessingOptionsForBinary* psOptionsForBinary)
{
    GDALDEMProcessingOptions *psOptions = (GDALDEMProcessingOptions *) CPLCalloc( 1, sizeof(GDALDEMProcessingOptions) );
    Algorithm eUtilityMode = INVALID;

    psOptions->pszFormat = CPLStrdup("GTiff");
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->pProgressData = NULL;
    psOptions->z = 1.0;
    psOptions->scale = 1.0;
    psOptions->az = 315.0;
    psOptions->alt = 45.0;
    // 0 = 'percent' or 1 = 'degrees'
    psOptions->slopeFormat = 1; 
    psOptions->bAddAlpha = FALSE;
    psOptions->bZeroForFlat = FALSE;
    psOptions->bAngleAsAzimuth = TRUE;
    psOptions->eColorSelectionMode = COLOR_SELECTION_INTERPOLATE;
    psOptions->bComputeAtEdges = FALSE;
    psOptions->bZevenbergenThorne = FALSE;
    psOptions->bCombined = FALSE;
    psOptions->nBand = 1;
    psOptions->papszCreateOptions = NULL;

/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    int argc = CSLCount(papszArgv);
    for( int i = 0; i < argc; i++ )
    {
        if( i == 0 && psOptionsForBinary )
        {
            eUtilityMode = GetAlgorithm(papszArgv[0]);
            if(eUtilityMode == INVALID )
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Invalid utility mode");
                GDALDEMProcessingOptionsFree(psOptions);
                return NULL;
            }
            psOptionsForBinary->pszProcessing = CPLStrdup(papszArgv[0]);
            continue;
        }

        if( EQUAL(papszArgv[i],"-of") && i < argc-1 )
        {
            ++i;
            CPLFree(psOptions->pszFormat);
            psOptions->pszFormat = CPLStrdup(papszArgv[i]);
            if( psOptionsForBinary )
            {
                psOptionsForBinary->bFormatExplicitlySet = TRUE;
            }
        }

        else if( EQUAL(papszArgv[i],"-q") || EQUAL(papszArgv[i],"-quiet") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bQuiet = TRUE;
        }

        else if( (EQUAL(papszArgv[i], "--z") || EQUAL(papszArgv[i], "-z")) && i+1<argc)
        {
            ++i;
            if( !ArgIsNumeric(papszArgv[i]) )
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Numeric value expected for -z");
                GDALDEMProcessingOptionsFree(psOptions);
                return NULL;
            }
            psOptions->z = CPLAtof(papszArgv[i]);
        }
        else if ( EQUAL(papszArgv[i], "-p") )
        {
            psOptions->slopeFormat = 0;
        }
        else if ( EQUAL(papszArgv[i], "-alg") && i+1<argc)
        {
            i ++;
            if (EQUAL(papszArgv[i], "ZevenbergenThorne"))
                psOptions->bZevenbergenThorne = TRUE;
            else if (!EQUAL(papszArgv[i], "Horn"))
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Numeric value expected for %s", papszArgv[i-1]);
                GDALDEMProcessingOptionsFree(psOptions);
                return NULL;
            }
        }
        else if ( EQUAL(papszArgv[i], "-trigonometric"))
        {
            psOptions->bAngleAsAzimuth = FALSE;
        }
        else if ( EQUAL(papszArgv[i], "-zero_for_flat"))
        {
            psOptions->bZeroForFlat = TRUE;
        }
        else if ( EQUAL(papszArgv[i], "-exact_color_entry"))
        {
            psOptions->eColorSelectionMode = COLOR_SELECTION_EXACT_ENTRY;
        }
        else if ( EQUAL(papszArgv[i], "-nearest_color_entry"))
        {
            psOptions->eColorSelectionMode = COLOR_SELECTION_NEAREST_ENTRY;
        }
        else if( 
            (EQUAL(papszArgv[i], "--s") || 
             EQUAL(papszArgv[i], "-s") ||
             EQUAL(papszArgv[i], "--scale") ||
             EQUAL(papszArgv[i], "-scale")) && i+1<argc
          )
        {
            ++i;
            if( !ArgIsNumeric(papszArgv[i]) )
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Numeric value expected for %s", papszArgv[i-1]);
                GDALDEMProcessingOptionsFree(psOptions);
                return NULL;
            }
            psOptions->scale = CPLAtof(papszArgv[i]);
        }
        else if( 
            (EQUAL(papszArgv[i], "--az") || 
             EQUAL(papszArgv[i], "-az") ||
             EQUAL(papszArgv[i], "--azimuth") ||
             EQUAL(papszArgv[i], "-azimuth")) && i+1<argc
          )
        {
            ++i;
            if( !ArgIsNumeric(papszArgv[i]) )
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Numeric value expected for %s", papszArgv[i-1]);
                GDALDEMProcessingOptionsFree(psOptions);
                return NULL;
            }
            psOptions->az = CPLAtof(papszArgv[i]);
        }
        else if( eUtilityMode == HILL_SHADE &&
            (EQUAL(papszArgv[i], "--alt") || 
             EQUAL(papszArgv[i], "-alt") ||
             EQUAL(papszArgv[i], "--alt") ||
             EQUAL(papszArgv[i], "-alt")) && i+1<argc
          )
        {
            ++i;
            if( !ArgIsNumeric(papszArgv[i]) )
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Numeric value expected for %s", papszArgv[i-1]);
                GDALDEMProcessingOptionsFree(psOptions);
                return NULL;
            }
            psOptions->alt = CPLAtof(papszArgv[i]);
        }
        else if( 
            (EQUAL(papszArgv[i], "-combined") || 
             EQUAL(papszArgv[i], "--combined"))
          )
        {
            psOptions->bCombined = TRUE;
        }
        else if( 
                 EQUAL(papszArgv[i], "-alpha"))
        {
            psOptions->bAddAlpha = TRUE;
        }
        else if( 
                 EQUAL(papszArgv[i], "-compute_edges"))
        {
            psOptions->bComputeAtEdges = TRUE;
        }
        else if( i + 1 < argc &&
            (EQUAL(papszArgv[i], "--b") || 
             EQUAL(papszArgv[i], "-b"))
          )
        {
            psOptions->nBand = atoi(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i],"-co") && i+1<argc )
        {
            psOptions->papszCreateOptions = CSLAddString( psOptions->papszCreateOptions, papszArgv[++i] );
        }
        else if( papszArgv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[i]);
            GDALDEMProcessingOptionsFree(psOptions);
            return NULL;
        }
        else if( psOptionsForBinary && psOptionsForBinary->pszSrcFilename == NULL )
        {
            psOptionsForBinary->pszSrcFilename = CPLStrdup(papszArgv[i]);
        }
        else if( psOptionsForBinary && eUtilityMode == COLOR_RELIEF && psOptionsForBinary->pszColorFilename == NULL )
        {
            psOptionsForBinary->pszColorFilename = CPLStrdup(papszArgv[i]);
        }
        else if( psOptionsForBinary && psOptionsForBinary->pszDstFilename == NULL )
        {
            psOptionsForBinary->pszDstFilename = CPLStrdup(papszArgv[i]);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many command options '%s'", papszArgv[i]);
            GDALDEMProcessingOptionsFree(psOptions);
            return NULL;
        }
    }

    if( psOptionsForBinary )
    {
        psOptionsForBinary->pszFormat = CPLStrdup(psOptions->pszFormat);
    }

    return psOptions;
}

/************************************************************************/
/*                       GDALDEMProcessingOptionsFree()                 */
/************************************************************************/

/**
 * Frees the GDALDEMProcessingOptions struct.
 *
 * @param psOptions the options struct for GDALDEMProcessing().
 *
 * @since GDAL 2.1
 */

void GDALDEMProcessingOptionsFree(GDALDEMProcessingOptions *psOptions)
{
    if( psOptions )
    {
        CPLFree(psOptions->pszFormat);
        CSLDestroy(psOptions->papszCreateOptions);

        CPLFree(psOptions);
    }
}

/************************************************************************/
/*                 GDALDEMProcessingOptionsSetProgress()                */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALDEMProcessing().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 2.1
 */

void GDALDEMProcessingOptionsSetProgress( GDALDEMProcessingOptions *psOptions,
                                      GDALProgressFunc pfnProgress, void *pProgressData )
{
    psOptions->pfnProgress = pfnProgress;
    psOptions->pProgressData = pProgressData;
}
