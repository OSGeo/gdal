/******************************************************************************
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

// Include before others for mingw for VSIStatBufL
#include "cpl_conv.h"

#include "cpl_port.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <algorithm>
#include <limits>

#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"

#if defined(__SSE2__) || defined(_M_X64)
#define HAVE_16_SSE_REG
#define HAVE_SSE2
#include "emmintrin.h"
#endif

CPL_CVSID("$Id$")

static const double kdfDegreesToRadians = M_PI / 180.0;
static const double kdfRadiansToDegrees = 180.0 / M_PI;

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
    bool bAddAlpha;
    bool bZeroForFlat;
    bool bAngleAsAzimuth;
    ColorSelectionMode eColorSelectionMode;
    bool bComputeAtEdges;
    bool bZevenbergenThorne;
    bool bCombined;
    bool bMultiDirectional;
    char** papszCreateOptions;
    int nBand;
};

/************************************************************************/
/*                          ComputeVal()                                */
/************************************************************************/

template<class T>
struct GDALGeneric3x3ProcessingAlg
{
    typedef float (*type) (const T* pafWindow, float fDstNoDataValue, void* pData);
};

template<class T>
struct GDALGeneric3x3ProcessingAlg_multisample
{
    typedef int (*type)  (const T* pafThreeLineWin,
                          int nLine1Off,
                          int nLine2Off,
                          int nLine3Off,
                          int nXSize,
                          void* pData,
                          float* pafOutputBuf);
};

template<class T>
static float ComputeVal( bool bSrcHasNoData, T fSrcNoDataValue,
                         bool bIsSrcNoDataNan,
                         T* afWin, float fDstNoDataValue,
                         typename GDALGeneric3x3ProcessingAlg<T>::type pfnAlg,
                         void* pData,
                         bool bComputeAtEdges );

template<>
float ComputeVal( bool bSrcHasNoData, float fSrcNoDataValue,
                  bool bIsSrcNoDataNan,
                  float* afWin, float fDstNoDataValue,
                  GDALGeneric3x3ProcessingAlg<float>::type pfnAlg,
                  void* pData,
                  bool bComputeAtEdges )
{
    if( bSrcHasNoData &&
        ((!bIsSrcNoDataNan && ARE_REAL_EQUAL(afWin[4], fSrcNoDataValue)) ||
         (bIsSrcNoDataNan && CPLIsNan(afWin[4]))) )
    {
        return fDstNoDataValue;
    }
    else if( bSrcHasNoData )
    {
        for( int k = 0; k < 9; k++ )
        {
            if( (!bIsSrcNoDataNan &&
                 ARE_REAL_EQUAL(afWin[k], fSrcNoDataValue)) ||
                (bIsSrcNoDataNan && CPLIsNan(afWin[k])) )
            {
                if( bComputeAtEdges )
                    afWin[k] = afWin[4];
                else
                    return fDstNoDataValue;
            }
        }
    }

    return pfnAlg(afWin, fDstNoDataValue, pData);
}

template<>
float ComputeVal( bool bSrcHasNoData, GInt32 fSrcNoDataValue,
                  bool /* bIsSrcNoDataNan */,
                  GInt32* afWin, float fDstNoDataValue,
                  GDALGeneric3x3ProcessingAlg<GInt32>::type pfnAlg,
                  void* pData,
                  bool bComputeAtEdges )
{
    if( bSrcHasNoData && afWin[4] == fSrcNoDataValue )
    {
        return fDstNoDataValue;
    }
    else if( bSrcHasNoData )
    {
        for( int k = 0; k < 9; k++ )
        {
            if( afWin[k] == fSrcNoDataValue )
            {
                if( bComputeAtEdges )
                    afWin[k] = afWin[4];
                else
                    return fDstNoDataValue;
            }
        }
    }

    return pfnAlg(afWin, fDstNoDataValue, pData);
}

/************************************************************************/
/*                           INTERPOL()                                 */
/************************************************************************/

template<class T> static T INTERPOL(T a, T b, int bSrcHasNodata, T fSrcNoDataValue);

template<>
float INTERPOL(float a, float b, int bSrcHasNoData, float fSrcNoDataValue)
{
    return ((bSrcHasNoData && (ARE_REAL_EQUAL(a, fSrcNoDataValue) ||
                               ARE_REAL_EQUAL(b, fSrcNoDataValue))) ?
                                            fSrcNoDataValue : 2 * (a) - (b));
}

template<>
GInt32 INTERPOL(GInt32 a, GInt32 b, int bSrcHasNoData, GInt32 fSrcNoDataValue)
{
    if( bSrcHasNoData && ((a == fSrcNoDataValue) || (b == fSrcNoDataValue)) )
        return fSrcNoDataValue;
    int nVal = 2 * a - b;
    if( bSrcHasNoData && fSrcNoDataValue == nVal )
        return nVal + 1;
    return nVal;
}

/************************************************************************/
/*                  GDALGeneric3x3Processing()                          */
/************************************************************************/

template<class T>
static
CPLErr GDALGeneric3x3Processing(
    GDALRasterBandH hSrcBand,
    GDALRasterBandH hDstBand,
    typename GDALGeneric3x3ProcessingAlg<T>::type pfnAlg,
    typename GDALGeneric3x3ProcessingAlg_multisample<T>::type pfnAlg_multisample,
    void *pData,
    bool bComputeAtEdges,
    GDALProgressFunc pfnProgress,
    void *pProgressData )
{
    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Initialize progress counter.                                    */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

    const int nXSize = GDALGetRasterBandXSize(hSrcBand);
    const int nYSize = GDALGetRasterBandYSize(hSrcBand);

    // 1 line destination buffer.
    float *pafOutputBuf = static_cast<float *>(
        VSI_MALLOC2_VERBOSE(sizeof(float), nXSize));
    // 3 line rotating source buffer.
    T *pafThreeLineWin  = static_cast<T *>(
        VSI_MALLOC2_VERBOSE(3 * sizeof(T), nXSize + 1));
    if( pafOutputBuf == NULL || pafThreeLineWin == NULL )
    {
        VSIFree(pafOutputBuf);
        VSIFree(pafThreeLineWin);
        return CE_Failure;
    }

    GDALDataType eReadDT;
    int bSrcHasNoData = FALSE;
    const double dfNoDataValue =
        GDALGetRasterNoDataValue(hSrcBand, &bSrcHasNoData);

    int bIsSrcNoDataNan = FALSE;
    T fSrcNoDataValue = 0;
    if( std::numeric_limits<T>::is_integer )
    {
        eReadDT = GDT_Int32;
        if( bSrcHasNoData )
        {
            GDALDataType eSrcDT = GDALGetRasterDataType( hSrcBand );
            CPLAssert( eSrcDT == GDT_Byte ||
                       eSrcDT == GDT_UInt16 ||
                       eSrcDT == GDT_Int16 );
            const int nMinVal =
                (eSrcDT == GDT_Byte ) ? 0 : (eSrcDT == GDT_UInt16) ? 0 : -32768;
            const int nMaxVal =
                (eSrcDT == GDT_Byte )
                ? 255
                : (eSrcDT == GDT_UInt16) ? 65535 : 32767;

            if( fabs(dfNoDataValue - floor(dfNoDataValue + 0.5)) < 1e-2 &&
                dfNoDataValue >= nMinVal && dfNoDataValue <= nMaxVal )
            {
                fSrcNoDataValue = static_cast<T>(floor(dfNoDataValue + 0.5));
            }
            else
            {
                bSrcHasNoData = FALSE;
            }
        }
    }
    else
    {
        eReadDT = GDT_Float32;
        fSrcNoDataValue = static_cast<T>(dfNoDataValue);
        bIsSrcNoDataNan = bSrcHasNoData && CPLIsNan(dfNoDataValue);
    }

    int bDstHasNoData = FALSE;
    float fDstNoDataValue =
        static_cast<float>(GDALGetRasterNoDataValue(hDstBand, &bDstHasNoData));
    if( !bDstHasNoData )
        fDstNoDataValue = 0.0;

    int nLine1Off = 0;
    int nLine2Off = nXSize;
    int nLine3Off = 2*nXSize;

    // Move a 3x3 pafWindow over each cell
    // (where the cell in question is #4)
    //
    //      0 1 2
    //      3 4 5
    //      6 7 8

    /* Preload the first 2 lines */

    bool abLineHasNoDataValue[3] = {
        CPL_TO_BOOL(bSrcHasNoData),
        CPL_TO_BOOL(bSrcHasNoData),
        CPL_TO_BOOL(bSrcHasNoData)
    };

    // Create an extra scope for VC12 to ignore i.
    {
      for( int i = 0; i < 2 && i < nYSize; i++ )
      {
        if( GDALRasterIO( hSrcBand,
                          GF_Read,
                          0, i,
                          nXSize, 1,
                          pafThreeLineWin + i * nXSize,
                          nXSize, 1,
                          eReadDT,
                          0, 0) != CE_None )
        {
            CPLFree(pafOutputBuf);
            CPLFree(pafThreeLineWin);

            return CE_Failure;
        }
        if( std::numeric_limits<T>::is_integer && bSrcHasNoData )
        {
            abLineHasNoDataValue[i] = false;
            for( int iX = 0; iX < nXSize; iX++ )
            {
                if( pafThreeLineWin[i * nXSize + iX] == fSrcNoDataValue )
                {
                    abLineHasNoDataValue[i] = true;
                    break;
                }
            }
        }
      }
    }  // End extra scope for VC12

    CPLErr eErr = CE_None;
    if( bComputeAtEdges && nXSize >= 2 && nYSize >= 2 )
    {
        for( int j = 0; j < nXSize; j++ )
        {
            int jmin = (j == 0) ? j : j - 1;
            int jmax = (j == nXSize - 1) ? j : j + 1;

            T afWin[9] = {
                INTERPOL(pafThreeLineWin[jmin], pafThreeLineWin[nXSize + jmin],
                         bSrcHasNoData, fSrcNoDataValue),
                INTERPOL(pafThreeLineWin[j],    pafThreeLineWin[nXSize + j],
                         bSrcHasNoData, fSrcNoDataValue),
                INTERPOL(pafThreeLineWin[jmax], pafThreeLineWin[nXSize + jmax],
                         bSrcHasNoData, fSrcNoDataValue),
                pafThreeLineWin[jmin],
                pafThreeLineWin[j],
                pafThreeLineWin[jmax],
                pafThreeLineWin[nXSize + jmin],
                pafThreeLineWin[nXSize + j],
                pafThreeLineWin[nXSize + jmax]
            };
            pafOutputBuf[j] = ComputeVal(
                CPL_TO_BOOL(bSrcHasNoData),
                fSrcNoDataValue,
                CPL_TO_BOOL(bIsSrcNoDataNan),
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
        for( int j = 0; j < nXSize; j++ )
        {
            pafOutputBuf[j] = fDstNoDataValue;
        }
        eErr = GDALRasterIO(hDstBand, GF_Write,
                    0, 0, nXSize, 1,
                    pafOutputBuf, nXSize, 1, GDT_Float32, 0, 0);

        if( eErr == CE_None && nYSize > 1 )
        {
            eErr = GDALRasterIO(hDstBand, GF_Write,
                        0, nYSize - 1, nXSize, 1,
                        pafOutputBuf, nXSize, 1, GDT_Float32, 0, 0);
        }
    }
    if( eErr != CE_None )
    {
        CPLFree(pafOutputBuf);
        CPLFree(pafThreeLineWin);

        return eErr;
    }

    int i = 1;  // Used after for.
    for( ; i < nYSize-1; i++ )
    {
        /* Read third line of the line buffer */
        eErr = GDALRasterIO(   hSrcBand,
                        GF_Read,
                        0, i+1,
                        nXSize, 1,
                        pafThreeLineWin + nLine3Off,
                        nXSize, 1,
                        eReadDT,
                        0, 0);
        if( eErr != CE_None )
        {
            CPLFree(pafOutputBuf);
            CPLFree(pafThreeLineWin);

            return eErr;
        }

        // In case none of the 3 lines have nodata values, then no need to
        // check it in ComputeVal()
        bool bOneOfThreeLinesHasNoData = CPL_TO_BOOL(bSrcHasNoData);
        if( std::numeric_limits<T>::is_integer && bSrcHasNoData )
        {
            bool bLastLineHasNoDataValue = false;
            int iX = 0;
            for( ; iX + 3 < nXSize; iX +=4 )
            {
                if( pafThreeLineWin[nLine3Off + iX] == fSrcNoDataValue ||
                    pafThreeLineWin[nLine3Off + iX + 1] == fSrcNoDataValue ||
                    pafThreeLineWin[nLine3Off + iX + 2] == fSrcNoDataValue ||
                    pafThreeLineWin[nLine3Off + iX + 3] == fSrcNoDataValue )
                {
                    bLastLineHasNoDataValue = true;
                    break;
                }
            }
            if( !bLastLineHasNoDataValue )
            {
                for( ; iX < nXSize; iX++ )
                {
                    if( pafThreeLineWin[nLine3Off + iX] == fSrcNoDataValue )
                    {
                        bLastLineHasNoDataValue = true;
                    }
                }
            }
            abLineHasNoDataValue[nLine3Off / nXSize] = bLastLineHasNoDataValue;

            bOneOfThreeLinesHasNoData = abLineHasNoDataValue[0] ||
                                abLineHasNoDataValue[1] ||
                                abLineHasNoDataValue[2];
        }

        if( bComputeAtEdges && nXSize >= 2 )
        {
            int j = 0;
            T afWin[9] = {
                INTERPOL(pafThreeLineWin[nLine1Off + j],
                         pafThreeLineWin[nLine1Off + j+1],
                         bSrcHasNoData, fSrcNoDataValue),
                pafThreeLineWin[nLine1Off + j],
                pafThreeLineWin[nLine1Off + j+1],
                INTERPOL(pafThreeLineWin[nLine2Off + j],
                         pafThreeLineWin[nLine2Off + j+1],
                         bSrcHasNoData, fSrcNoDataValue),
                pafThreeLineWin[nLine2Off + j],
                pafThreeLineWin[nLine2Off + j+1],
                INTERPOL(pafThreeLineWin[nLine3Off + j],
                         pafThreeLineWin[nLine3Off + j+1],
                         bSrcHasNoData, fSrcNoDataValue),
                pafThreeLineWin[nLine3Off + j],
                pafThreeLineWin[nLine3Off + j+1]
            };

            pafOutputBuf[j] =
                ComputeVal(
                    CPL_TO_BOOL(bOneOfThreeLinesHasNoData),
                    fSrcNoDataValue,
                    CPL_TO_BOOL(bIsSrcNoDataNan),
                    afWin, fDstNoDataValue,
                    pfnAlg, pData, bComputeAtEdges);
        }
        else
        {
            // Exclude the edges
            pafOutputBuf[0] = fDstNoDataValue;
        }

        int j = 1;
        if( pfnAlg_multisample && !bOneOfThreeLinesHasNoData )
        {
            j = pfnAlg_multisample(pafThreeLineWin,
                                   nLine1Off,
                                   nLine2Off,
                                   nLine3Off,
                                   nXSize,
                                   pData,
                                   pafOutputBuf);
        }

        for( ; j < nXSize - 1; j++ )
        {
            T afWin[9] = {
                pafThreeLineWin[nLine1Off + j-1],
                pafThreeLineWin[nLine1Off + j],
                pafThreeLineWin[nLine1Off + j+1],
                pafThreeLineWin[nLine2Off + j-1],
                pafThreeLineWin[nLine2Off + j],
                pafThreeLineWin[nLine2Off + j+1],
                pafThreeLineWin[nLine3Off + j-1],
                pafThreeLineWin[nLine3Off + j],
                pafThreeLineWin[nLine3Off + j+1]
            };

            pafOutputBuf[j] =
                ComputeVal(
                    CPL_TO_BOOL(bOneOfThreeLinesHasNoData),
                    fSrcNoDataValue,
                    CPL_TO_BOOL(bIsSrcNoDataNan),
                    afWin, fDstNoDataValue,
                    pfnAlg, pData, bComputeAtEdges);
        }

        if( bComputeAtEdges && nXSize >= 2 )
        {
            j = nXSize - 1;

            T afWin[9] = {
                pafThreeLineWin[nLine1Off + j-1],
                pafThreeLineWin[nLine1Off + j],
                INTERPOL(pafThreeLineWin[nLine1Off + j],
                         pafThreeLineWin[nLine1Off + j-1],
                         bSrcHasNoData, fSrcNoDataValue),
                pafThreeLineWin[nLine2Off + j-1],
                pafThreeLineWin[nLine2Off + j],
                INTERPOL(pafThreeLineWin[nLine2Off + j],
                         pafThreeLineWin[nLine2Off + j-1],
                         bSrcHasNoData, fSrcNoDataValue),
                pafThreeLineWin[nLine3Off + j-1],
                pafThreeLineWin[nLine3Off + j],
                INTERPOL(pafThreeLineWin[nLine3Off + j],
                         pafThreeLineWin[nLine3Off + j-1],
                         bSrcHasNoData, fSrcNoDataValue)
            };

            pafOutputBuf[j] =
                ComputeVal(
                    CPL_TO_BOOL(bOneOfThreeLinesHasNoData),
                    fSrcNoDataValue,
                    CPL_TO_BOOL(bIsSrcNoDataNan),
                    afWin, fDstNoDataValue,
                    pfnAlg, pData, bComputeAtEdges);
        }
        else
        {
            // Exclude the edges
            if( nXSize > 1 )
                pafOutputBuf[nXSize - 1] = fDstNoDataValue;
        }

        /* -----------------------------------------
         * Write Line to Raster
         */
        eErr = GDALRasterIO(hDstBand, GF_Write, 0, i, nXSize, 1,
                            pafOutputBuf, nXSize, 1, GDT_Float32, 0, 0);
        if( eErr != CE_None )
        {
            CPLFree(pafOutputBuf);
            CPLFree(pafThreeLineWin);

            return eErr;
        }

        if( !pfnProgress( 1.0 * (i+1) / nYSize, NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;

            CPLFree(pafOutputBuf);
            CPLFree(pafThreeLineWin);

            return eErr;
        }

        const int nTemp = nLine1Off;
        nLine1Off = nLine2Off;
        nLine2Off = nLine3Off;
        nLine3Off = nTemp;
    }

    if( bComputeAtEdges && nXSize >= 2 && nYSize >= 2 )
    {
        for( int j = 0; j < nXSize; j++ )
        {
            int jmin = (j == 0) ? j : j - 1;
            int jmax = (j == nXSize - 1) ? j : j + 1;

            T afWin[9] = {
                pafThreeLineWin[nLine1Off + jmin],
                pafThreeLineWin[nLine1Off + j],
                pafThreeLineWin[nLine1Off + jmax],
                pafThreeLineWin[nLine2Off + jmin],
                pafThreeLineWin[nLine2Off + j],
                pafThreeLineWin[nLine2Off + jmax],
                INTERPOL(pafThreeLineWin[nLine2Off + jmin],
                         pafThreeLineWin[nLine1Off + jmin],
                         bSrcHasNoData, fSrcNoDataValue),
                INTERPOL(pafThreeLineWin[nLine2Off + j],
                         pafThreeLineWin[nLine1Off + j],
                         bSrcHasNoData, fSrcNoDataValue),
                INTERPOL(pafThreeLineWin[nLine2Off + jmax],
                         pafThreeLineWin[nLine1Off + jmax],
                         bSrcHasNoData, fSrcNoDataValue),
            };

            pafOutputBuf[j] = ComputeVal(
                CPL_TO_BOOL(bSrcHasNoData),
                fSrcNoDataValue,
                CPL_TO_BOOL(bIsSrcNoDataNan),
                afWin, fDstNoDataValue,
                pfnAlg, pData, bComputeAtEdges);
        }
        eErr = GDALRasterIO(hDstBand, GF_Write,
                            0, i, nXSize, 1,
                            pafOutputBuf, nXSize, 1, GDT_Float32, 0, 0);
        if( eErr != CE_None )
        {
            CPLFree(pafOutputBuf);
            CPLFree(pafThreeLineWin);

            return eErr;
        }
    }

    pfnProgress( 1.0, NULL, pProgressData );
    eErr = CE_None;

    CPLFree(pafOutputBuf);
    CPLFree(pafThreeLineWin);

    return eErr;
}

/************************************************************************/
/*                            GradientAlg                               */
/************************************************************************/

typedef enum
{
    HORN,
    ZEVENBERGEN_THORNE
} GradientAlg;

template<class T, GradientAlg alg> struct Gradient
{
    static void inline calc(const T* afWin, double inv_ewres, double inv_nsres,
                            double&x, double&y);
};

template<class T> struct Gradient<T, HORN>
{
    static void calc(const T* afWin, double inv_ewres, double inv_nsres,
                     double&x, double&y)
    {
        x = ((afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
             (afWin[2] + afWin[5] + afWin[5] + afWin[8])) * inv_ewres;

        y = ((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
             (afWin[0] + afWin[1] + afWin[1] + afWin[2])) * inv_nsres;
    }
};

template<class T> struct Gradient<T, ZEVENBERGEN_THORNE>
{
    static void calc(const T* afWin, double inv_ewres, double inv_nsres,
                     double&x, double&y)
    {
        x = (afWin[3] - afWin[5]) * inv_ewres;
        y = (afWin[7] - afWin[1]) * inv_nsres;
    }
};

/************************************************************************/
/*                         GDALHillshade()                              */
/************************************************************************/

typedef struct
{
    double inv_nsres;
    double inv_ewres;
    double sin_altRadians;
    double cos_alt_mul_z;
    double azRadians;
    double cos_az_mul_cos_alt_mul_z;
    double sin_az_mul_cos_alt_mul_z;
    double square_z;
    double sin_altRadians_mul_254;
    double cos_az_mul_cos_alt_mul_z_mul_254;
    double sin_az_mul_cos_alt_mul_z_mul_254;

    double square_z_mul_square_inv_res;
    double cos_az_mul_cos_alt_mul_z_mul_254_mul_inv_res;
    double sin_az_mul_cos_alt_mul_z_mul_254_mul_inv_res;
} GDALHillshadeAlgData;

/* Unoptimized formulas are :
    x = psData->z*((afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
        (afWin[2] + afWin[5] + afWin[5] + afWin[8])) /
        (8.0 * psData->ewres * psData->scale);

    y = psData->z*((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
        (afWin[0] + afWin[1] + afWin[1] + afWin[2])) /
        (8.0 * psData->nsres * psData->scale);

    slope = atan(sqrt(x*x + y*y));

    aspect = atan2(y,x);

    cang = sin(alt) * cos(slope) +
           cos(alt) * sin(slope) *
           cos(az - M_PI/2 - aspect);

We can avoid a lot of trigonometric computations:

    since cos(atan(x)) = 1 / sqrt(1+x^2)
      ==> cos(slope) = 1 / sqrt(1+ x*x+y*y)

      and sin(atan(x)) = x / sqrt(1+x^2)
      ==> sin(slope) = sqrt(x*x + y*y) / sqrt(1+ x*x+y*y)

      and cos(az - M_PI/2 - aspect)
        = cos(-az + M_PI/2 + aspect)
        = cos(M_PI/2 - (az - aspect))
        = sin(az - aspect)
        = -sin(aspect-az)

==> cang = (sin(alt) - cos(alt) * sqrt(x*x + y*y)  * sin(aspect-as)) /
           sqrt(1+ x*x+y*y)

    But:
    sin(aspect - az) = sin(aspect)*cos(az) - cos(aspect)*sin(az))

and as sin(aspect)=sin(atan2(y,x)) = y / sqrt(xx_plus_yy)
   and cos(aspect)=cos(atan2(y,x)) = x / sqrt(xx_plus_yy)

    sin(aspect - az) = (y * cos(az) - x * sin(az)) / sqrt(xx_plus_yy)

so we get a final formula with just one transcendental function
(reciprocal of square root):

    cang = (psData->sin_altRadians -
           (y * psData->cos_az_mul_cos_alt_mul_z -
            x * psData->sin_az_mul_cos_alt_mul_z)) /
           sqrt(1 + psData->square_z * xx_plus_yy);
*/

#ifdef HAVE_SSE2
inline double ApproxADivByInvSqrtB( double a, double b )
{
    __m128d regB = _mm_load_sd( &b );
    __m128d regB_half = _mm_mul_sd( regB, _mm_set1_pd( 0.5 ) );
    // Compute rough approximation of 1 / sqrt(b) with _mm_rsqrt_ss
    regB = _mm_cvtss_sd( regB, _mm_rsqrt_ss(
                            _mm_cvtsd_ss( _mm_setzero_ps(), regB ) ) );
    // And perform one step of Newton-Raphson approximation to improve it
    // approx_inv_sqrt_x = approx_inv_sqrt_x*(1.5 -
    //                            0.5*x*approx_inv_sqrt_x*approx_inv_sqrt_x);
    regB = _mm_mul_sd(regB, _mm_sub_sd( _mm_set1_pd( 1.5 ),
                                        _mm_mul_sd(regB_half,
                                                   _mm_mul_sd(regB, regB)) ) );
    double dOut;
    _mm_store_sd( &dOut, regB );
    return a * dOut;
}
#else
inline double ApproxADivByInvSqrtB( double a, double b )
{
    return a / sqrt(b);
}
#endif

template<class T, GradientAlg alg>
static
float GDALHillshadeAlg (const T* afWin, float /*fDstNoDataValue*/, void* pData)
{
    GDALHillshadeAlgData* psData = (GDALHillshadeAlgData*)pData;

    // First Slope ...
    double x, y;
    Gradient<T, alg>::calc(afWin, psData->inv_ewres, psData->inv_nsres, x, y);

    const double xx_plus_yy = x * x + y * y;

    // ... then the shade value
    const double cang_mul_254 =
        ApproxADivByInvSqrtB(
            psData->sin_altRadians_mul_254 -
            (y * psData->cos_az_mul_cos_alt_mul_z_mul_254 -
             x * psData->sin_az_mul_cos_alt_mul_z_mul_254),
            1 + psData->square_z * xx_plus_yy);

    const double cang = cang_mul_254 <= 0.0 ? 1.0 : 1.0 + cang_mul_254;

    return static_cast<float>(cang);
}

template<class T>
static
float GDALHillshadeAlg_same_res (const T* afWin, float /*fDstNoDataValue*/, void* pData)
{
    GDALHillshadeAlgData* psData = (GDALHillshadeAlgData*)pData;

    // First Slope ...
    /*x = (afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
        (afWin[2] + afWin[5] + afWin[5] + afWin[8]);

    y = (afWin[0] + afWin[1] + afWin[1] + afWin[2]) -
        (afWin[6] + afWin[7] + afWin[7] + afWin[8]);*/

    T accX = afWin[0] - afWin[8];
    const T six_minus_two = afWin[6] - afWin[2];
    T accY = accX;
    const T three_minus_five = afWin[3] - afWin[5];
    const T one_minus_seven = afWin[1] - afWin[7];
    accX += three_minus_five;
    accY += one_minus_seven;
    accX += three_minus_five;
    accY += one_minus_seven;
    accX += six_minus_two;
    accY -= six_minus_two;
    const double x = accX;
    const double y = accY;

    const double xx_plus_yy = x * x + y * y;

    // ... then the shade value
    const double cang_mul_254 =
        ApproxADivByInvSqrtB(
            psData->sin_altRadians_mul_254 +
            (x * psData->sin_az_mul_cos_alt_mul_z_mul_254_mul_inv_res +
             y * psData->cos_az_mul_cos_alt_mul_z_mul_254_mul_inv_res),
            1 + psData->square_z_mul_square_inv_res * xx_plus_yy);

    const double cang = cang_mul_254 <= 0.0 ? 1.0 : 1.0 + cang_mul_254;

    return static_cast<float>(cang);
}

#ifdef HAVE_16_SSE_REG
template<class T>
static
int GDALHillshadeAlg_same_res_multisample( const T* pafThreeLineWin,
                                           int nLine1Off,
                                           int nLine2Off,
                                           int nLine3Off,
                                           int nXSize,
                                           void* pData,
                                           float* pafOutputBuf )
{
    // Only valid for T == int

    GDALHillshadeAlgData* psData = (GDALHillshadeAlgData*)pData;
    const __m128d reg_fact_x = _mm_load1_pd(
                      &(psData->sin_az_mul_cos_alt_mul_z_mul_254_mul_inv_res));
    const __m128d reg_fact_y = _mm_load1_pd (
                      &(psData->cos_az_mul_cos_alt_mul_z_mul_254_mul_inv_res));
    const __m128d reg_constant_num = _mm_load1_pd(
                      &(psData->sin_altRadians_mul_254));
    const __m128d reg_constant_denom = _mm_load1_pd(
                      &(psData->square_z_mul_square_inv_res));
    const __m128d reg_half = _mm_set1_pd(0.5);
    const __m128d reg_one = _mm_add_pd(reg_half, reg_half);
    const __m128 reg_one_float = _mm_set1_ps(1);

    int j = 1;  // Used after for.
    for( ; j < nXSize - 4; j+= 4 )
    {
        const T* firstLine  = pafThreeLineWin + nLine1Off + j-1;
        const T* secondLine = pafThreeLineWin + nLine2Off + j-1;
        const T* thirdLine  = pafThreeLineWin + nLine3Off + j-1;

        __m128i firstLine0 = _mm_loadu_si128( (__m128i const*)firstLine );
        __m128i firstLine1 = _mm_loadu_si128( (__m128i const*)(firstLine + 1) );
        __m128i firstLine2 = _mm_loadu_si128( (__m128i const*)(firstLine + 2) );
        __m128i thirdLine0 = _mm_loadu_si128( (__m128i const*)thirdLine );
        __m128i thirdLine1 = _mm_loadu_si128( (__m128i const*)(thirdLine + 1) );
        __m128i thirdLine2 = _mm_loadu_si128( (__m128i const*)(thirdLine + 2) );
        __m128i accX = _mm_sub_epi32( firstLine0, thirdLine2);
        const __m128i six_minus_two = _mm_sub_epi32( thirdLine0, firstLine2 );
        __m128i accY = accX;
        const __m128i three_minus_five = _mm_sub_epi32(
                          _mm_loadu_si128( (__m128i const*)secondLine ),
                          _mm_loadu_si128( (__m128i const*)(secondLine+2) ) );
        const __m128i one_minus_seven = _mm_sub_epi32( firstLine1, thirdLine1 );
        accX = _mm_add_epi32(accX, three_minus_five);
        accY = _mm_add_epi32(accY, one_minus_seven);
        accX = _mm_add_epi32(accX, three_minus_five);
        accY = _mm_add_epi32(accY, one_minus_seven);
        accX = _mm_add_epi32(accX, six_minus_two);
        accY = _mm_sub_epi32(accY, six_minus_two);

        __m128d reg_x0 = _mm_cvtepi32_pd(accX);
        __m128d reg_x1 = _mm_cvtepi32_pd(_mm_srli_si128(accX, 8));
        __m128d reg_y0 = _mm_cvtepi32_pd(accY);
        __m128d reg_y1 = _mm_cvtepi32_pd(_mm_srli_si128(accY, 8));
        __m128d reg_xx_plus_yy0 = _mm_add_pd( _mm_mul_pd(reg_x0, reg_x0),
                                              _mm_mul_pd(reg_y0, reg_y0) );
        __m128d reg_xx_plus_yy1 = _mm_add_pd( _mm_mul_pd(reg_x1, reg_x1),
                                              _mm_mul_pd(reg_y1, reg_y1) );

        __m128d reg_numerator0 = _mm_add_pd(reg_constant_num,
                  _mm_add_pd( _mm_mul_pd(reg_fact_x, reg_x0),
                              _mm_mul_pd(reg_fact_y, reg_y0) ) );
        __m128d reg_numerator1 = _mm_add_pd(reg_constant_num,
                  _mm_add_pd( _mm_mul_pd(reg_fact_x, reg_x1),
                              _mm_mul_pd(reg_fact_y, reg_y1) ) );
        __m128d reg_denominator0 = _mm_add_pd(reg_one,
                              _mm_mul_pd(reg_constant_denom, reg_xx_plus_yy0));
        __m128d reg_denominator1 = _mm_add_pd(reg_one,
                              _mm_mul_pd(reg_constant_denom, reg_xx_plus_yy1));

        __m128d regB0 = reg_denominator0;
        __m128d regB1 = reg_denominator1;
        __m128d regB0_half = _mm_mul_pd( regB0, reg_half );
        __m128d regB1_half = _mm_mul_pd( regB1, reg_half );
        // Compute rough approximation of 1 / sqrt(b) with _mm_rsqrt_ps
        regB0 = _mm_cvtps_pd( _mm_rsqrt_ps( _mm_cvtpd_ps( regB0 ) ) );
        regB1 = _mm_cvtps_pd( _mm_rsqrt_ps( _mm_cvtpd_ps( regB1 ) ) );
        // And perform one step of Newton-Raphson approximation to improve it
        // approx_inv_sqrt_x = approx_inv_sqrt_x*(1.5 -
        //                            0.5*x*approx_inv_sqrt_x*approx_inv_sqrt_x);
        const __m128d reg_one_and_a_half = _mm_add_pd(reg_one, reg_half);
        regB0 = _mm_mul_pd(regB0, _mm_sub_pd( reg_one_and_a_half,
                                             _mm_mul_pd(regB0_half,
                                                  _mm_mul_pd(regB0, regB0)) ) );
        regB1 = _mm_mul_pd(regB1, _mm_sub_pd( reg_one_and_a_half,
                                            _mm_mul_pd(regB1_half,
                                                  _mm_mul_pd(regB1, regB1)) ) );
        reg_numerator0 = _mm_mul_pd(reg_numerator0, regB0);
        reg_numerator1 = _mm_mul_pd(reg_numerator1, regB1);

        __m128 res = _mm_castsi128_ps(
          _mm_unpacklo_epi64 (_mm_castps_si128(_mm_cvtpd_ps(reg_numerator0)),
                              _mm_castps_si128(_mm_cvtpd_ps(reg_numerator1))));
        res = _mm_add_ps(res, reg_one_float);
        res = _mm_max_ps(res, reg_one_float);

        _mm_storeu_ps( pafOutputBuf + j, res);
    }
    return j;
}
#endif

static const double INV_SQUARE_OF_HALF_PI = 1.0 / ((M_PI*M_PI)/4);

template<class T, GradientAlg alg>
static
float GDALHillshadeCombinedAlg (const T* afWin, float /*fDstNoDataValue*/, void* pData)
{
    GDALHillshadeAlgData* psData = (GDALHillshadeAlgData*)pData;

    // First Slope ...
    double x, y;
    Gradient<T, alg>::calc(afWin, psData->inv_ewres, psData->inv_nsres, x, y);

    const double xx_plus_yy = x * x + y * y;

    const double slope = xx_plus_yy * psData->square_z;

    // ... then the shade value
    double cang =
        acos(
            ApproxADivByInvSqrtB(
                psData->sin_altRadians -
                (y * psData->cos_az_mul_cos_alt_mul_z -
                 x * psData->sin_az_mul_cos_alt_mul_z),
                1 + slope));

    // combined shading
    cang = 1 - cang * atan(sqrt(slope)) * INV_SQUARE_OF_HALF_PI;

    const float fcang =
        cang <= 0.0
        ? 1.0f
        : static_cast<float>(1.0 + (254.0 * cang));

    return fcang;
}

static
void* GDALCreateHillshadeData( double* adfGeoTransform,
                               double z,
                               double scale,
                               double alt,
                               double az,
                               bool bZevenbergenThorne )
{
    GDALHillshadeAlgData* pData = static_cast<GDALHillshadeAlgData *>(
        CPLCalloc(1, sizeof(GDALHillshadeAlgData)));

    pData->inv_nsres = 1.0 / adfGeoTransform[5];
    pData->inv_ewres = 1.0 / adfGeoTransform[1];
    pData->sin_altRadians = sin(alt * kdfDegreesToRadians);
    pData->azRadians = az * kdfDegreesToRadians;
    const double z_scaled = z / ((bZevenbergenThorne ? 2 : 8) * scale);
    pData->cos_alt_mul_z =
        cos(alt * kdfDegreesToRadians) * z_scaled;
    pData->cos_az_mul_cos_alt_mul_z =
        cos(pData->azRadians) * pData->cos_alt_mul_z;
    pData->sin_az_mul_cos_alt_mul_z =
        sin(pData->azRadians) * pData->cos_alt_mul_z;
    pData->square_z = z_scaled * z_scaled;

    pData->sin_altRadians_mul_254 = 254.0 *
                                    pData->sin_altRadians;
    pData->cos_az_mul_cos_alt_mul_z_mul_254 = 254.0 *
        pData->cos_az_mul_cos_alt_mul_z;
    pData->sin_az_mul_cos_alt_mul_z_mul_254 = 254.0 *
        pData->sin_az_mul_cos_alt_mul_z;

    if( adfGeoTransform[1] == -adfGeoTransform[5] )
    {
        pData->square_z_mul_square_inv_res =
          pData->square_z * pData->inv_ewres * pData->inv_ewres;
        pData->cos_az_mul_cos_alt_mul_z_mul_254_mul_inv_res =
          pData->cos_az_mul_cos_alt_mul_z_mul_254 * -pData->inv_ewres;
        pData->sin_az_mul_cos_alt_mul_z_mul_254_mul_inv_res =
          pData->sin_az_mul_cos_alt_mul_z_mul_254 * pData->inv_ewres;
    }

    return pData;
}

/************************************************************************/
/*                   GDALHillshadeMultiDirectional()                    */
/************************************************************************/

typedef struct
{
    double inv_nsres;
    double inv_ewres;
    double square_z;
    double sin_altRadians_mul_127;
    double sin_altRadians_mul_254;

    double cos_alt_mul_z_mul_127;
    double cos225_az_mul_cos_alt_mul_z_mul_127;

} GDALHillshadeMultiDirectionalAlgData;

template<class T, GradientAlg alg>
static
float GDALHillshadeMultiDirectionalAlg (const T* afWin,
                                        float /*fDstNoDataValue*/,
                                        void* pData)
{
    const GDALHillshadeMultiDirectionalAlgData* psData =
                            (const GDALHillshadeMultiDirectionalAlgData*)pData;

    // First Slope ...
    double x, y;
    Gradient<T, alg>::calc(afWin, psData->inv_ewres, psData->inv_nsres, x, y);

    // See http://pubs.usgs.gov/of/1992/of92-422/of92-422.pdf
    // W225 = sin^2(aspect - 225) = 0.5 * (1 - 2 * sin(aspect) * cos(aspect))
    // W270 = sin^2(aspect - 270) = cos^2(aspect)
    // W315 = sin^2(aspect - 315) = 0.5 * (1 + 2 * sin(aspect) * cos(aspect))
    // W360 = sin^2(aspect - 360) = sin^2(aspect)
    // hillshade=  0.5 * (W225 * hillshade(az=225) +
    //                    W270 * hillshade(az=270) +
    //                    W315 * hillshade(az=315) +
    //                    W360 * hillshade(az=360))

    const double xx = x * x;
    const double yy = y * y;
    const double xx_plus_yy = xx + yy;
    if( xx_plus_yy == 0.0 )
        return static_cast<float>(1.0 + psData->sin_altRadians_mul_254);

    // ... then the shade value from different azimuth
    double val225_mul_127 = psData->sin_altRadians_mul_127 +
                            (x-y) * psData->cos225_az_mul_cos_alt_mul_z_mul_127;
    val225_mul_127 = ( val225_mul_127 <= 0.0) ? 0.0 : val225_mul_127;
    double val270_mul_127 = psData->sin_altRadians_mul_127 -
                            x * psData->cos_alt_mul_z_mul_127;
    val270_mul_127 = ( val270_mul_127 <= 0.0) ? 0.0 : val270_mul_127;
    double val315_mul_127 = psData->sin_altRadians_mul_127 +
                            (x+y) * psData->cos225_az_mul_cos_alt_mul_z_mul_127;
    val315_mul_127 = ( val315_mul_127 <= 0.0) ? 0.0 : val315_mul_127;
    double val360_mul_127 = psData->sin_altRadians_mul_127 -
                            y * psData->cos_alt_mul_z_mul_127;
    val360_mul_127 = ( val360_mul_127 <= 0.0) ? 0.0 : val360_mul_127;

    // ... then the weighted shading
    const double weight_225 = 0.5 * xx_plus_yy - x * y;
    const double weight_270 = xx;
    const double weight_315 = xx_plus_yy - weight_225;
    const double weight_360 = yy;
    const double cang_mul_127 = ApproxADivByInvSqrtB(
                  (weight_225 * val225_mul_127 +
                   weight_270 * val270_mul_127 +
                   weight_315 * val315_mul_127 +
                   weight_360 * val360_mul_127) / xx_plus_yy,
            1 + psData->square_z * xx_plus_yy);

    const double cang = 1.0 + cang_mul_127;

    return static_cast<float>(cang);
}

static
void* GDALCreateHillshadeMultiDirectionalData( double* adfGeoTransform,
                                               double z,
                                               double scale,
                                               double alt,
                                               bool bZevenbergenThorne )
{
    GDALHillshadeMultiDirectionalAlgData* pData =
      static_cast<GDALHillshadeMultiDirectionalAlgData *>(
          CPLCalloc(1, sizeof(GDALHillshadeMultiDirectionalAlgData)));

    pData->inv_nsres = 1.0 / adfGeoTransform[5];
    pData->inv_ewres = 1.0 / adfGeoTransform[1];
    const double z_scaled = z / ((bZevenbergenThorne ? 2 : 8) * scale);
    const double cos_alt_mul_z =
        cos(alt * kdfDegreesToRadians) * z_scaled;
    pData->square_z = z_scaled * z_scaled;

    pData->sin_altRadians_mul_127 = 127.0 * sin(alt * kdfDegreesToRadians);
    pData->sin_altRadians_mul_254 = 254.0 * sin(alt * kdfDegreesToRadians);
    pData->cos_alt_mul_z_mul_127 = 127.0 * cos_alt_mul_z;
    pData->cos225_az_mul_cos_alt_mul_z_mul_127 = 127.0 *
        cos(225 * kdfDegreesToRadians) * cos_alt_mul_z;

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

template<class T>
static
float GDALSlopeHornAlg( const T* afWin, float /*fDstNoDataValue*/, void* pData )
{
    GDALSlopeAlgData* psData = (GDALSlopeAlgData*)pData;

    const double dx = ((afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
          (afWin[2] + afWin[5] + afWin[5] + afWin[8]))/psData->ewres;

    const double dy = ((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
          (afWin[0] + afWin[1] + afWin[1] + afWin[2]))/psData->nsres;

    const double key = (dx * dx + dy * dy);

    if( psData->slopeFormat == 1 )
        return static_cast<float>(
            atan(sqrt(key) / (8*psData->scale)) * kdfRadiansToDegrees);

    return static_cast<float>(100*(sqrt(key) / (8*psData->scale)));
}

template<class T>
static
float GDALSlopeZevenbergenThorneAlg( const T* afWin,
                                     float /*fDstNoDataValue*/,
                                     void* pData )
{
    GDALSlopeAlgData* psData = (GDALSlopeAlgData*)pData;

    const double dx = (afWin[3] - afWin[5]) / psData->ewres;
    const double dy = (afWin[7] - afWin[1]) / psData->nsres;
    const double key = dx * dx + dy * dy;

    if( psData->slopeFormat == 1 )
        return static_cast<float>(
            atan(sqrt(key) / (2*psData->scale)) * kdfRadiansToDegrees);

    return static_cast<float>(100*(sqrt(key) / (2*psData->scale)));
}

static
void* GDALCreateSlopeData( double* adfGeoTransform,
                           double scale,
                           int slopeFormat )
{
    GDALSlopeAlgData* pData =
        static_cast<GDALSlopeAlgData*>(CPLMalloc(sizeof(GDALSlopeAlgData)));

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
    bool bAngleAsAzimuth;
} GDALAspectAlgData;

template<class T>
static
float GDALAspectAlg( const T* afWin, float fDstNoDataValue, void* pData )
{
    GDALAspectAlgData* psData = (GDALAspectAlgData*)pData;

    const double dx = ((afWin[2] + afWin[5] + afWin[5] + afWin[8]) -
          (afWin[0] + afWin[3] + afWin[3] + afWin[6]));

    const double dy = ((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
          (afWin[0] + afWin[1] + afWin[1] + afWin[2]));

    float aspect = static_cast<float>(atan2(dy,-dx) / kdfDegreesToRadians);

    if( dx == 0 && dy == 0 )
    {
        /* Flat area */
        aspect = fDstNoDataValue;
    }
    else if( psData->bAngleAsAzimuth )
    {
        if( aspect > 90.0f )
            aspect = 450.0f - aspect;
        else
            aspect = 90.0f - aspect;
    }
    else
    {
        if( aspect < 0 )
            aspect += 360.0f;
    }

    if( aspect == 360.0f )
        aspect = 0.0;

    return aspect;
}

template<class T>
static
float GDALAspectZevenbergenThorneAlg( const T* afWin, float fDstNoDataValue,
                                      void* pData )
{
    GDALAspectAlgData* psData = (GDALAspectAlgData*)pData;

    const double dx = afWin[5] - afWin[3];
    const double dy = afWin[7] - afWin[1];
    float aspect = static_cast<float>(atan2(dy,-dx) / kdfDegreesToRadians);
    if( dx == 0 && dy == 0 )
    {
        /* Flat area */
        aspect = fDstNoDataValue;
    }
    else if( psData->bAngleAsAzimuth )
    {
        if( aspect > 90.0f )
            aspect = 450.0f - aspect;
        else
            aspect = 90.0f - aspect;
    }
    else
    {
        if( aspect < 0 )
            aspect += 360.0f;
    }

    if( aspect == 360.0f )
        aspect = 0.0;

    return aspect;
}

static
void *GDALCreateAspectData( bool bAngleAsAzimuth )
{
    GDALAspectAlgData* pData =
        static_cast<GDALAspectAlgData *>(CPLMalloc(sizeof(GDALAspectAlgData)));

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

static int GDALColorReliefSortColors(const ColorAssociation &pA,
                                     const ColorAssociation &pB)
{
    /* Sort NaN in first position */
    return (CPLIsNan(pA.dfVal) && !CPLIsNan(pB.dfVal)) || pA.dfVal < pB.dfVal;
}

static void GDALColorReliefProcessColors(ColorAssociation **ppasColorAssociation,
                                         int *pnColorAssociation, int bSrcHasNoData,
                                         double dfSrcNoDataValue)
{
    ColorAssociation *pasColorAssociation = *ppasColorAssociation;
    int nColorAssociation = *pnColorAssociation;

    std::stable_sort(pasColorAssociation, pasColorAssociation + nColorAssociation,
                     GDALColorReliefSortColors);

    ColorAssociation *pPrevious =
            (nColorAssociation > 0) ? &pasColorAssociation[0] : NULL;
    int nAdded = 0;
    int nRepeatedEntryIndex = 0;
    for( int i = 1; i < nColorAssociation; ++i )
    {
        ColorAssociation *pCurrent = &pasColorAssociation[i];

        // NaN comparison is always false, so it handles itself
        if( bSrcHasNoData && pCurrent->dfVal == dfSrcNoDataValue )
        {
            // Check if there is enough distance between the nodata value and
            // its predecessor.
            const double dfNewValue =
                pCurrent->dfVal - std::abs(pCurrent->dfVal) * DBL_EPSILON;
            if( dfNewValue > pPrevious->dfVal )
            {
                // add one just below the nodata value
                ++nAdded;
                ColorAssociation sPrevious = *pPrevious;
                pasColorAssociation = static_cast<ColorAssociation *>(
                    CPLRealloc(pasColorAssociation,
                               (nColorAssociation + nAdded) *
                               sizeof(ColorAssociation)));
                pCurrent = &pasColorAssociation[i];
                pPrevious = NULL;
                pasColorAssociation[nColorAssociation + nAdded - 1] = sPrevious;
                pasColorAssociation[nColorAssociation + nAdded - 1].dfVal = dfNewValue;
            }
        }
        else if( bSrcHasNoData && pPrevious->dfVal == dfSrcNoDataValue )
        {
            // Check if there is enough distance between the nodata value and
            // its successor.
            const double dfNewValue =
                pPrevious->dfVal + std::abs(pPrevious->dfVal) * DBL_EPSILON;
            if( dfNewValue < pCurrent->dfVal )
            {
                // add one just above the nodata value
                ++nAdded;
                ColorAssociation sCurrent = *pCurrent;
                pasColorAssociation = static_cast<ColorAssociation *>(
                    CPLRealloc(pasColorAssociation,
                               (nColorAssociation + nAdded) *
                               sizeof(ColorAssociation)));
                pCurrent = &pasColorAssociation[i];
                pPrevious = NULL;
                pasColorAssociation[nColorAssociation + nAdded - 1] = sCurrent;
                pasColorAssociation[nColorAssociation + nAdded - 1].dfVal = dfNewValue;
            }
        }
        else if( nRepeatedEntryIndex == 0 &&
                 pCurrent->dfVal == pPrevious->dfVal )
        {
            // second of a series of equivalent entries
            nRepeatedEntryIndex = i;
        }
        else if( nRepeatedEntryIndex != 0 &&
                 pCurrent->dfVal != pPrevious->dfVal )
        {
            // Get the distance between the predecessor and successor of the
            // equivalent entries.
            double dfTotalDist = 0.0;
            double dfLeftDist = 0.0;
            if( nRepeatedEntryIndex >= 2 )
            {
                ColorAssociation *pLower =
                    &pasColorAssociation[nRepeatedEntryIndex - 2];
                dfTotalDist = pCurrent->dfVal - pLower->dfVal;
                dfLeftDist = pPrevious->dfVal - pLower->dfVal;
            }
            else
            {
                dfTotalDist = pCurrent->dfVal - pPrevious->dfVal;
            }

            // check if this distance is enough
            const int nEquivalentCount = i - nRepeatedEntryIndex + 1;
            if( dfTotalDist >
                std::abs(pPrevious->dfVal) * nEquivalentCount * DBL_EPSILON )
            {
                // balance the alterations
                double dfMultiplier =
                    0.5 - nEquivalentCount * dfLeftDist / dfTotalDist;
                for( int j = nRepeatedEntryIndex - 1; j < i; ++j )
                {
                    pasColorAssociation[j].dfVal +=
                        (std::abs(pPrevious->dfVal) * dfMultiplier) *
                        DBL_EPSILON;
                    dfMultiplier += 1.0;
                }
            }
            else
            {
                // Fallback to the old behaviour: keep equivalent entries as
                // they are.
            }

            nRepeatedEntryIndex = 0;
        }
        pPrevious = pCurrent;
    }

    if( nAdded )
    {
        std::stable_sort(pasColorAssociation,
                         pasColorAssociation + nColorAssociation + nAdded,
                         GDALColorReliefSortColors);
        *ppasColorAssociation = pasColorAssociation;
        *pnColorAssociation = nColorAssociation + nAdded;
    }
}

static bool GDALColorReliefGetRGBA( ColorAssociation* pasColorAssociation,
                                    int nColorAssociation,
                                    double dfVal,
                                    ColorSelectionMode eColorSelectionMode,
                                    int* pnR,
                                    int* pnG,
                                    int* pnB,
                                    int* pnA)
{
    int lower = 0;

    // Special case for NaN
    if( CPLIsNan(pasColorAssociation[0].dfVal) )
    {
        if( CPLIsNan(dfVal) )
        {
            *pnR = pasColorAssociation[0].nR;
            *pnG = pasColorAssociation[0].nG;
            *pnB = pasColorAssociation[0].nB;
            *pnA = pasColorAssociation[0].nA;
            return true;
        }
        else
        {
            lower = 1;
        }
    }

    // Find the index of the first element in the LUT input array that
    // is not smaller than the dfVal value.
    int i = 0;
    int upper = nColorAssociation - 1;
    while( true )
    {
        const int mid = (lower + upper) / 2;
        if( upper - lower <= 1 )
        {
            if( dfVal <= pasColorAssociation[lower].dfVal )
                i = lower;
            else if( dfVal <= pasColorAssociation[upper].dfVal )
                i = upper;
            else
                i = upper + 1;
            break;
        }
        else if( pasColorAssociation[mid].dfVal >= dfVal )
        {
            upper = mid;
        }
        else
        {
            lower = mid;
        }
    }

    if( i == 0 )
    {
        if( eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY &&
            pasColorAssociation[0].dfVal != dfVal )
        {
            *pnR = 0;
            *pnG = 0;
            *pnB = 0;
            *pnA = 0;
            return false;
        }
        else
        {
            *pnR = pasColorAssociation[0].nR;
            *pnG = pasColorAssociation[0].nG;
            *pnB = pasColorAssociation[0].nB;
            *pnA = pasColorAssociation[0].nA;
            return true;
        }
    }
    else if( i == nColorAssociation )
    {
        if( eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY &&
            pasColorAssociation[i-1].dfVal != dfVal )
        {
            *pnR = 0;
            *pnG = 0;
            *pnB = 0;
            *pnA = 0;
            return false;
        }
        else
        {
            *pnR = pasColorAssociation[i-1].nR;
            *pnG = pasColorAssociation[i-1].nG;
            *pnB = pasColorAssociation[i-1].nB;
            *pnA = pasColorAssociation[i-1].nA;
            return true;
        }
    }
    else
    {
        if( eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY &&
            pasColorAssociation[i-1].dfVal != dfVal )
        {
            *pnR = 0;
            *pnG = 0;
            *pnB = 0;
            *pnA = 0;
            return false;
        }

        if( eColorSelectionMode == COLOR_SELECTION_NEAREST_ENTRY &&
            pasColorAssociation[i-1].dfVal != dfVal )
        {
            int index = i;
            if( dfVal - pasColorAssociation[i-1].dfVal <
                pasColorAssociation[i].dfVal - dfVal )
            {
                --index;
            }

            *pnR = pasColorAssociation[index].nR;
            *pnG = pasColorAssociation[index].nG;
            *pnB = pasColorAssociation[index].nB;
            *pnA = pasColorAssociation[index].nA;
            return true;
        }

        if( pasColorAssociation[i-1].dfVal == dfVal )
        {
            *pnR = pasColorAssociation[i-1].nR;
            *pnG = pasColorAssociation[i-1].nG;
            *pnB = pasColorAssociation[i-1].nB;
            *pnA = pasColorAssociation[i-1].nA;
            return true;
        }

        if( CPLIsNan(pasColorAssociation[i-1].dfVal) )
        {
            *pnR = pasColorAssociation[i].nR;
            *pnG = pasColorAssociation[i].nG;
            *pnB = pasColorAssociation[i].nB;
            *pnA = pasColorAssociation[i].nA;
            return true;
        }

        const double dfRatio =
            (dfVal - pasColorAssociation[i-1].dfVal) /
            (pasColorAssociation[i].dfVal - pasColorAssociation[i-1].dfVal);
        *pnR = static_cast<int>(
            0.45 + pasColorAssociation[i-1].nR + dfRatio *
            (pasColorAssociation[i].nR - pasColorAssociation[i-1].nR));
        if( *pnR < 0 ) *pnR = 0;
        else if( *pnR > 255 ) *pnR = 255;
        *pnG = static_cast<int>(
            0.45 + pasColorAssociation[i-1].nG + dfRatio *
            (pasColorAssociation[i].nG - pasColorAssociation[i-1].nG));
        if( *pnG < 0 ) *pnG = 0;
        else if( *pnG > 255 ) *pnG = 255;
        *pnB = static_cast<int>(
            0.45 + pasColorAssociation[i-1].nB + dfRatio *
            (pasColorAssociation[i].nB - pasColorAssociation[i-1].nB));
        if( *pnB < 0 ) *pnB = 0;
        else if( *pnB > 255 ) *pnB = 255;
        *pnA = static_cast<int>(
            0.45 + pasColorAssociation[i-1].nA + dfRatio *
            (pasColorAssociation[i].nA - pasColorAssociation[i-1].nA));
        if( *pnA < 0 ) *pnA = 0;
        else if( *pnA > 255 ) *pnA = 255;

        return true;
    }
}

/* dfPct : percentage between 0 and 1 */
static double GDALColorReliefGetAbsoluteValFromPct( GDALRasterBandH hSrcBand,
                                                    double dfPct )
{
    int bSuccessMin = FALSE;
    int bSuccessMax = FALSE;
    double dfMin = GDALGetRasterMinimum(hSrcBand, &bSuccessMin);
    double dfMax = GDALGetRasterMaximum(hSrcBand, &bSuccessMax);
    if( !bSuccessMin || !bSuccessMax )
    {
        double dfMean = 0.0;
        double dfStdDev = 0.0;
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
bool GDALColorReliefFindNamedColor( const char *pszColorName,
                                    int *pnR, int *pnG, int *pnB )
{
    *pnR = 0;
    *pnG = 0;
    *pnB = 0;
    for( unsigned int i = 0;
         i < sizeof(namedColors) / sizeof(namedColors[0]);
         i++ )
    {
        if( EQUAL(pszColorName, namedColors[i].name) )
        {
            *pnR = static_cast<int>(255.0 * namedColors[i].r);
            *pnG = static_cast<int>(255.0 * namedColors[i].g);
            *pnB = static_cast<int>(255.0 * namedColors[i].b);
            return true;
        }
    }
    return false;
}

static
ColorAssociation* GDALColorReliefParseColorFile( GDALRasterBandH hSrcBand,
                                                 const char* pszColorFilename,
                                                 int* pnColors )
{
    VSILFILE* fpColorFile = VSIFOpenL(pszColorFilename, "rt");
    if( fpColorFile == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find %s", pszColorFilename);
        *pnColors = 0;
        return NULL;
    }

    ColorAssociation* pasColorAssociation = NULL;
    int nColorAssociation = 0;

    int bSrcHasNoData = FALSE;
    double dfSrcNoDataValue = GDALGetRasterNoDataValue(hSrcBand, &bSrcHasNoData);

    const char* pszLine = NULL;
    bool bIsGMT_CPT = false;
    while( (pszLine = CPLReadLineL(fpColorFile)) != NULL )
    {
        if( pszLine[0] == '#' && strstr(pszLine, "COLOR_MODEL") )
        {
            if( strstr(pszLine, "COLOR_MODEL = RGB") == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Only COLOR_MODEL = RGB is supported");
                CPLFree(pasColorAssociation);
                *pnColors = 0;
                return NULL;
            }
            bIsGMT_CPT = true;
        }

        char** papszFields = CSLTokenizeStringComplex(pszLine, " ,\t:",
                                                      FALSE, FALSE );
        /* Skip comment lines */
        const int nTokens = CSLCount(papszFields);
        if( nTokens >= 1 && (papszFields[0][0] == '#' ||
                             papszFields[0][0] == '/') )
        {
            CSLDestroy(papszFields);
            continue;
        }

        if( bIsGMT_CPT && nTokens == 8 )
        {
            pasColorAssociation = static_cast<ColorAssociation *>(
                CPLRealloc(pasColorAssociation,
                           (nColorAssociation + 2) * sizeof(ColorAssociation)));

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
        else if( bIsGMT_CPT && nTokens == 4 )
        {
            // The first token might be B (background), F (foreground) or N
            // (nodata) Just interested in N.
            if( EQUAL(papszFields[0], "N") && bSrcHasNoData )
            {
                pasColorAssociation = static_cast<ColorAssociation *>(
                    CPLRealloc(pasColorAssociation,
                               (nColorAssociation + 1) *
                               sizeof(ColorAssociation)));

                pasColorAssociation[nColorAssociation].dfVal = dfSrcNoDataValue;
                pasColorAssociation[nColorAssociation].nR = atoi(papszFields[1]);
                pasColorAssociation[nColorAssociation].nG = atoi(papszFields[2]);
                pasColorAssociation[nColorAssociation].nB = atoi(papszFields[3]);
                pasColorAssociation[nColorAssociation].nA = 255;
                nColorAssociation++;
            }
        }
        else if( !bIsGMT_CPT && nTokens >= 2 )
        {
            pasColorAssociation = static_cast<ColorAssociation *>(
                CPLRealloc(pasColorAssociation,
                           (nColorAssociation + 1) * sizeof(ColorAssociation)));
            if( EQUAL(papszFields[0], "nv") && bSrcHasNoData )
            {
                pasColorAssociation[nColorAssociation].dfVal = dfSrcNoDataValue;
            }
            else if( strlen(papszFields[0]) > 1 &&
                     papszFields[0][strlen(papszFields[0])-1] == '%' )
            {
                const double dfPct = CPLAtof(papszFields[0]) / 100.0;
                if( dfPct < 0.0 || dfPct > 1.0 )
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
            {
                pasColorAssociation[nColorAssociation].dfVal = CPLAtof(papszFields[0]);
            }

            if( nTokens >= 4 )
            {
                pasColorAssociation[nColorAssociation].nR = atoi(papszFields[1]);
                pasColorAssociation[nColorAssociation].nG = atoi(papszFields[2]);
                pasColorAssociation[nColorAssociation].nB = atoi(papszFields[3]);
                pasColorAssociation[nColorAssociation].nA =
                        (CSLCount(papszFields) >= 5 ) ? atoi(papszFields[4]) : 255;
            }
            else
            {
                int nR = 0;
                int nG = 0;
                int nB = 0;
                if( !GDALColorReliefFindNamedColor(papszFields[1],
                                                   &nR, &nG, &nB) )
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

            nColorAssociation++;
        }
        CSLDestroy(papszFields);
    }
    VSIFCloseL(fpColorFile);

    if( nColorAssociation == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No color association found in %s", pszColorFilename);
        *pnColors = 0;
        return NULL;
    }

    GDALColorReliefProcessColors(&pasColorAssociation, &nColorAssociation,
                                 bSrcHasNoData, dfSrcNoDataValue);

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
    const GDALDataType eDT = GDALGetRasterDataType(hSrcBand);
    GByte* pabyPrecomputed = NULL;
    const int nIndexOffset = (eDT == GDT_Int16) ? 32768 : 0;
    *pnIndexOffset = nIndexOffset;
    const int nXSize = GDALGetRasterBandXSize(hSrcBand);
    const int nYSize = GDALGetRasterBandXSize(hSrcBand);
    if( eDT == GDT_Byte ||
        ((eDT == GDT_Int16 || eDT == GDT_UInt16) && nXSize * nYSize > 65536) )
    {
        const int iMax = (eDT == GDT_Byte) ? 256: 65536;
        pabyPrecomputed = static_cast<GByte *>(VSI_MALLOC2_VERBOSE(4, iMax));
        if( pabyPrecomputed )
        {
            for( int i = 0; i < iMax; i++ )
            {
                int nR = 0;
                int nG = 0;
                int nB = 0;
                int nA = 0;
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
                        GDALColorReliefDataset(
                            GDALDatasetH hSrcDS,
                            GDALRasterBandH hSrcBand,
                            const char* pszColorFilename,
                            ColorSelectionMode eColorSelectionMode,
                            int bAlpha);
                       ~GDALColorReliefDataset();

    bool        InitOK() const
        { return pafSourceBuf != NULL || panSourceBuf != NULL; }

    CPLErr      GetGeoTransform( double * padfGeoTransform ) override;
    const char *GetProjectionRef() override;
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

    virtual CPLErr          IReadBlock( int, int, void * ) override;
    virtual GDALColorInterp GetColorInterpretation() override;
};

GDALColorReliefDataset::GDALColorReliefDataset(
    GDALDatasetH hSrcDSIn,
    GDALRasterBandH hSrcBandIn,
    const char* pszColorFilename,
    ColorSelectionMode eColorSelectionModeIn,
    int bAlpha ) :
    hSrcDS(hSrcDSIn),
    hSrcBand(hSrcBandIn),
    nColorAssociation(0),
    pasColorAssociation(NULL),
    eColorSelectionMode(eColorSelectionModeIn),
    pabyPrecomputed(NULL),
    nIndexOffset(0),
    pafSourceBuf(NULL),
    panSourceBuf(NULL),
    nCurBlockXOff(-1),
    nCurBlockYOff(-1)
{
    pasColorAssociation =
            GDALColorReliefParseColorFile(hSrcBand, pszColorFilename,
                                          &nColorAssociation);

    nRasterXSize = GDALGetRasterXSize(hSrcDS);
    nRasterYSize = GDALGetRasterYSize(hSrcDS);

    int nBlockXSize = 0;
    int nBlockYSize = 0;
    GDALGetBlockSize( hSrcBand, &nBlockXSize, &nBlockYSize);

    pabyPrecomputed =
        GDALColorReliefPrecompute(hSrcBand,
                                  pasColorAssociation,
                                  nColorAssociation,
                                  eColorSelectionMode,
                                  &nIndexOffset);

    for( int i = 0; i < ((bAlpha) ? 4 : 3); i++ )
    {
        SetBand(i + 1, new GDALColorReliefRasterBand(this, i+1));
    }

    if( pabyPrecomputed )
        panSourceBuf = static_cast<int *>(
            VSI_MALLOC3_VERBOSE(sizeof(int), nBlockXSize, nBlockYSize));
    else
        pafSourceBuf = static_cast<float *>(
            VSI_MALLOC3_VERBOSE(sizeof(float), nBlockXSize, nBlockYSize));
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
    const int nReqXSize =
        (nBlockXOff + 1) * nBlockXSize >= nRasterXSize
        ? nRasterXSize - nBlockXOff * nBlockXSize
        : nBlockXSize;

    const int nReqYSize =
        (nBlockYOff + 1) * nBlockYSize >= nRasterYSize
        ? nRasterYSize - nBlockYOff * nBlockYSize
        : nBlockYSize;

    if( poGDS->nCurBlockXOff != nBlockXOff ||
        poGDS->nCurBlockYOff != nBlockYOff )
    {
        poGDS->nCurBlockXOff = nBlockXOff;
        poGDS->nCurBlockYOff = nBlockYOff;

        const CPLErr eErr =
            GDALRasterIO( poGDS->hSrcBand,
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
        if( eErr != CE_None )
        {
            memset(pImage, 0, nBlockXSize * nBlockYSize);
            return eErr;
        }
    }

    int j = 0;
    if( poGDS->panSourceBuf )
    {
        for( int y = 0; y < nReqYSize; y++ )
        {
            for( int x = 0; x < nReqXSize; x++ )
            {
                const int nIndex = poGDS->panSourceBuf[j] + poGDS->nIndexOffset;
                ((GByte*)pImage)[y * nBlockXSize + x] =
                    poGDS->pabyPrecomputed[4*nIndex + nBand-1];
                j++;
            }
        }
    }
    else
    {
        int anComponents[4] = { 0, 0, 0, 0 };
        for( int y = 0; y < nReqYSize; y++ )
        {
            for( int x = 0; x < nReqXSize; x++ )
            {
                GDALColorReliefGetRGBA  (poGDS->pasColorAssociation,
                                        poGDS->nColorAssociation,
                                        poGDS->pafSourceBuf[j],
                                        poGDS->eColorSelectionMode,
                                        &anComponents[0],
                                        &anComponents[1],
                                        &anComponents[2],
                                        &anComponents[3]);
                ((GByte*)pImage)[y * nBlockXSize + x] =
                    (GByte) anComponents[nBand-1];
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
CPLErr GDALColorRelief( GDALRasterBandH hSrcBand,
                        GDALRasterBandH hDstBand1,
                        GDALRasterBandH hDstBand2,
                        GDALRasterBandH hDstBand3,
                        GDALRasterBandH hDstBand4,
                        const char* pszColorFilename,
                        ColorSelectionMode eColorSelectionMode,
                        GDALProgressFunc pfnProgress,
                        void * pProgressData )
{
    if( hSrcBand == NULL || hDstBand1 == NULL || hDstBand2 == NULL ||
        hDstBand3 == NULL )
        return CE_Failure;

    int nColorAssociation = 0;
    ColorAssociation* pasColorAssociation =
        GDALColorReliefParseColorFile(hSrcBand, pszColorFilename,
                                      &nColorAssociation);
    if( pasColorAssociation == NULL )
        return CE_Failure;

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

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

    const int nXSize = GDALGetRasterBandXSize(hSrcBand);
    const int nYSize = GDALGetRasterBandYSize(hSrcBand);

    float* pafSourceBuf = NULL;
    int* panSourceBuf = NULL;
    if( pabyPrecomputed )
        panSourceBuf = static_cast<int *>(
            VSI_MALLOC2_VERBOSE(sizeof(int), nXSize));
    else
        pafSourceBuf = static_cast<float *>(
            VSI_MALLOC2_VERBOSE(sizeof(float), nXSize));
    GByte* pabyDestBuf1 = static_cast<GByte *>(VSI_MALLOC2_VERBOSE(4, nXSize));
    GByte* pabyDestBuf2 =  pabyDestBuf1 + nXSize;
    GByte* pabyDestBuf3 =  pabyDestBuf2 + nXSize;
    GByte* pabyDestBuf4 =  pabyDestBuf3 + nXSize;

    if( (pabyPrecomputed != NULL && panSourceBuf == NULL) ||
        (pabyPrecomputed == NULL && pafSourceBuf == NULL) ||
        pabyDestBuf1 == NULL )
    {
        VSIFree(pabyPrecomputed);
        CPLFree(pafSourceBuf);
        CPLFree(panSourceBuf);
        CPLFree(pabyDestBuf1);
        CPLFree(pasColorAssociation);

        return CE_Failure;
    }

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        VSIFree(pabyPrecomputed);
        CPLFree(pafSourceBuf);
        CPLFree(panSourceBuf);
        CPLFree(pabyDestBuf1);
        CPLFree(pasColorAssociation);

        return CE_Failure;
    }

    int nR = 0;
    int nG = 0;
    int nB = 0;
    int nA = 0;

    for( int i = 0; i < nYSize; i++ )
    {
        /* Read source buffer */
        CPLErr eErr = GDALRasterIO( hSrcBand,
                                    GF_Read,
                                    0, i,
                                    nXSize, 1,
                                    panSourceBuf
                                    ? (void*) panSourceBuf
                                    : (void*) pafSourceBuf,
                                    nXSize, 1,
                                    panSourceBuf ? GDT_Int32 : GDT_Float32,
                                    0, 0);
        if( eErr != CE_None )
        {
            VSIFree(pabyPrecomputed);
            CPLFree(pafSourceBuf);
            CPLFree(panSourceBuf);
            CPLFree(pabyDestBuf1);
            CPLFree(pasColorAssociation);
            return eErr;
        }

        if( pabyPrecomputed )
        {
            for( int j = 0; j < nXSize; j++ )
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
            for( int j = 0; j < nXSize; j++ )
            {
                GDALColorReliefGetRGBA  (pasColorAssociation,
                                         nColorAssociation,
                                         pafSourceBuf[j],
                                         eColorSelectionMode,
                                         &nR,
                                         &nG,
                                         &nB,
                                         &nA);
                pabyDestBuf1[j] = static_cast<GByte>(nR);
                pabyDestBuf2[j] = static_cast<GByte>(nG);
                pabyDestBuf3[j] = static_cast<GByte>(nB);
                pabyDestBuf4[j] = static_cast<GByte>(nA);
            }
        }

        /* -----------------------------------------
         * Write Line to Raster
         */
        eErr = GDALRasterIO(hDstBand1,
                            GF_Write,
                            0, i, nXSize,
                            1, pabyDestBuf1, nXSize, 1, GDT_Byte, 0, 0);
        if( eErr != CE_None )
        {
            VSIFree(pabyPrecomputed);
            CPLFree(pafSourceBuf);
            CPLFree(panSourceBuf);
            CPLFree(pabyDestBuf1);
            CPLFree(pasColorAssociation);

            return eErr;
        }

        eErr = GDALRasterIO(hDstBand2,
                            GF_Write,
                            0, i, nXSize,
                            1, pabyDestBuf2, nXSize, 1, GDT_Byte, 0, 0);
        if( eErr != CE_None )
        {
            VSIFree(pabyPrecomputed);
            CPLFree(pafSourceBuf);
            CPLFree(panSourceBuf);
            CPLFree(pabyDestBuf1);
            CPLFree(pasColorAssociation);

            return eErr;
        }

        eErr = GDALRasterIO(hDstBand3,
                            GF_Write,
                            0, i, nXSize,
                            1, pabyDestBuf3, nXSize, 1, GDT_Byte, 0, 0);
        if( eErr != CE_None )
        {
            VSIFree(pabyPrecomputed);
            CPLFree(pafSourceBuf);
            CPLFree(panSourceBuf);
            CPLFree(pabyDestBuf1);
            CPLFree(pasColorAssociation);

            return eErr;
        }

        if( hDstBand4 )
        {
            eErr = GDALRasterIO(hDstBand4,
                        GF_Write,
                        0, i, nXSize,
                        1, pabyDestBuf4, nXSize, 1, GDT_Byte, 0, 0);
            if( eErr != CE_None )
            {
                VSIFree(pabyPrecomputed);
                CPLFree(pafSourceBuf);
                CPLFree(panSourceBuf);
                CPLFree(pabyDestBuf1);
                CPLFree(pasColorAssociation);

                return eErr;
            }
        }

        if( !pfnProgress( 1.0 * (i+1) / nYSize, NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );

            VSIFree(pabyPrecomputed);
            CPLFree(pafSourceBuf);
            CPLFree(panSourceBuf);
            CPLFree(pabyDestBuf1);
            CPLFree(pasColorAssociation);

            return CE_Failure;
        }
    }

    pfnProgress( 1.0, NULL, pProgressData );

    VSIFree(pabyPrecomputed);
    CPLFree(pafSourceBuf);
    CPLFree(panSourceBuf);
    CPLFree(pabyDestBuf1);
    CPLFree(pasColorAssociation);

    return CE_None;
}

/************************************************************************/
/*                     GDALGenerateVRTColorRelief()                     */
/************************************************************************/

static
CPLErr GDALGenerateVRTColorRelief( const char* pszDstFilename,
                                   GDALDatasetH hSrcDataset,
                                   GDALRasterBandH hSrcBand,
                                   const char* pszColorFilename,
                                   ColorSelectionMode eColorSelectionMode,
                                   bool bAddAlpha )
{
    int nColorAssociation = 0;
    ColorAssociation* pasColorAssociation =
            GDALColorReliefParseColorFile(hSrcBand, pszColorFilename,
                                          &nColorAssociation);
    if( pasColorAssociation == NULL )
        return CE_Failure;

    VSILFILE* fp = VSIFOpenL(pszDstFilename, "wt");
    if( fp == NULL )
    {
        CPLFree(pasColorAssociation);
        return CE_Failure;
    }

    const int nXSize = GDALGetRasterBandXSize(hSrcBand);
    const int nYSize = GDALGetRasterBandYSize(hSrcBand);

    bool bOK =
        VSIFPrintfL(fp,
                    "<VRTDataset rasterXSize=\"%d\" rasterYSize=\"%d\">\n",
                    nXSize, nYSize) > 0;
    const char* pszProjectionRef = GDALGetProjectionRef(hSrcDataset);
    if( pszProjectionRef && pszProjectionRef[0] != '\0' )
    {
        char* pszEscapedString = CPLEscapeString(pszProjectionRef, -1, CPLES_XML);
        bOK &= VSIFPrintfL(fp, "  <SRS>%s</SRS>\n", pszEscapedString) > 0;
        VSIFree(pszEscapedString);
    }
    double adfGT[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    if( GDALGetGeoTransform(hSrcDataset, adfGT) == CE_None )
    {
        bOK &= VSIFPrintfL(fp,
                           "  <GeoTransform> %.16g, %.16g, %.16g, "
                           "%.16g, %.16g, %.16g</GeoTransform>\n",
                           adfGT[0], adfGT[1], adfGT[2],
                           adfGT[3], adfGT[4], adfGT[5]) > 0;
    }

    int nBlockXSize = 0;
    int nBlockYSize = 0;
    GDALGetBlockSize(hSrcBand, &nBlockXSize, &nBlockYSize);

    int bRelativeToVRT = FALSE;
    CPLString osPath = CPLGetPath(pszDstFilename);
    char* pszSourceFilename = CPLStrdup(
        CPLExtractRelativePath( osPath.c_str(), GDALGetDescription(hSrcDataset),
                                &bRelativeToVRT ));

    const int nBands = 3 + (bAddAlpha ? 1 : 0);

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        bOK &= VSIFPrintfL(
            fp,
            "  <VRTRasterBand dataType=\"Byte\" band=\"%d\">\n", iBand + 1) > 0;
        bOK &= VSIFPrintfL(
            fp, "    <ColorInterp>%s</ColorInterp>\n",
            GDALGetColorInterpretationName((GDALColorInterp)(GCI_RedBand + iBand))) > 0;
        bOK &= VSIFPrintfL(
            fp, "    <ComplexSource>\n") > 0;
        bOK &= VSIFPrintfL(
            fp,
            "      <SourceFilename relativeToVRT=\"%d\">%s</SourceFilename>\n",
            bRelativeToVRT, pszSourceFilename) > 0;
        bOK &= VSIFPrintfL(
            fp, "      <SourceBand>%d</SourceBand>\n",
            GDALGetBandNumber(hSrcBand)) > 0;
        bOK &= VSIFPrintfL(
            fp, "      <SourceProperties RasterXSize=\"%d\" "
            "RasterYSize=\"%d\" DataType=\"%s\" "
            "BlockXSize=\"%d\" BlockYSize=\"%d\"/>\n",
            nXSize, nYSize,
            GDALGetDataTypeName(GDALGetRasterDataType(hSrcBand)),
            nBlockXSize, nBlockYSize) > 0;
        bOK &= VSIFPrintfL(
            fp,
            "      "
            "<SrcRect xOff=\"0\" yOff=\"0\" xSize=\"%d\" ySize=\"%d\"/>\n",
            nXSize, nYSize) > 0;
        bOK &= VSIFPrintfL(
            fp,
            "      "
            "<DstRect xOff=\"0\" yOff=\"0\" xSize=\"%d\" ySize=\"%d\"/>\n",
            nXSize, nYSize) > 0;

        bOK &= VSIFPrintfL(fp, "      <LUT>") > 0;

        for( int iColor = 0; iColor < nColorAssociation; iColor++ )
        {
            if( eColorSelectionMode == COLOR_SELECTION_NEAREST_ENTRY )
            {
                if( iColor > 1 )
                    bOK &= VSIFPrintfL(fp, ",") > 0;
            }
            else if( iColor > 0 )
                bOK &= VSIFPrintfL(fp, ",") > 0;

            const double dfVal = pasColorAssociation[iColor].dfVal;

            if( eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY )
            {
                bOK &= VSIFPrintfL(fp, "%.18g:0,",
                                   dfVal - fabs(dfVal) * DBL_EPSILON) > 0;
            }
            else if( iColor > 0 &&
                     eColorSelectionMode == COLOR_SELECTION_NEAREST_ENTRY )
            {
                const double dfMidVal =
                    (dfVal + pasColorAssociation[iColor-1].dfVal) / 2.0;
                bOK &= VSIFPrintfL(
                    fp, "%.18g:%d",
                    dfMidVal - fabs(dfMidVal) * DBL_EPSILON,
                    (iBand == 0) ? pasColorAssociation[iColor-1].nR :
                    (iBand == 1) ? pasColorAssociation[iColor-1].nG :
                    (iBand == 2) ? pasColorAssociation[iColor-1].nB :
                    pasColorAssociation[iColor-1].nA) > 0;
                bOK &= VSIFPrintfL(
                    fp, ",%.18g:%d", dfMidVal,
                    (iBand == 0) ? pasColorAssociation[iColor].nR :
                    (iBand == 1) ? pasColorAssociation[iColor].nG :
                    (iBand == 2) ? pasColorAssociation[iColor].nB :
                    pasColorAssociation[iColor].nA) > 0;
            }

            if( eColorSelectionMode != COLOR_SELECTION_NEAREST_ENTRY )
            {
                if( dfVal != static_cast<double>(static_cast<int>(dfVal)) )
                    bOK &= VSIFPrintfL(fp, "%.18g", dfVal) > 0;
                else
                    bOK &= VSIFPrintfL(fp, "%d", static_cast<int>(dfVal)) > 0;
                bOK &= VSIFPrintfL(fp, ":%d",
                            (iBand == 0) ? pasColorAssociation[iColor].nR :
                            (iBand == 1) ? pasColorAssociation[iColor].nG :
                            (iBand == 2) ? pasColorAssociation[iColor].nB :
                                           pasColorAssociation[iColor].nA) > 0;
            }

            if( eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY )
            {
                bOK &= VSIFPrintfL(fp, ",%.18g:0",
                                   dfVal + fabs(dfVal) * DBL_EPSILON) > 0;
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

template<class T> static T MyAbs(T x);

template<> float MyAbs( float x ) { return fabsf(x); }
template<> int MyAbs( int x ) { return x >= 0 ? x : -x; }

template<class T>
static
float GDALTRIAlg( const T* afWin,
                  float /*fDstNoDataValue*/,
                  void* /*pData*/ )
{
    // Terrain Ruggedness is average difference in height
    return (MyAbs(afWin[0]-afWin[4]) +
            MyAbs(afWin[1]-afWin[4]) +
            MyAbs(afWin[2]-afWin[4]) +
            MyAbs(afWin[3]-afWin[4]) +
            MyAbs(afWin[5]-afWin[4]) +
            MyAbs(afWin[6]-afWin[4]) +
            MyAbs(afWin[7]-afWin[4]) +
            MyAbs(afWin[8]-afWin[4])) * 0.125f;
}

/************************************************************************/
/*                         GDALTPIAlg()                                 */
/************************************************************************/

template<class T>
static
float GDALTPIAlg( const T* afWin,
                  float /*fDstNoDataValue*/,
                  void* /*pData*/ )
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
              afWin[8]) * 0.125f );
}

/************************************************************************/
/*                     GDALRoughnessAlg()                               */
/************************************************************************/

template<class T>
static
float GDALRoughnessAlg( const T* afWin, float /*fDstNoDataValue*/,
                        void* /*pData*/ )
{
    // Roughness is the largest difference
    //  between any two cells

    T pafRoughnessMin = afWin[0];
    T pafRoughnessMax = afWin[0];

    for( int k = 1; k < 9; k++ )
    {
        if( afWin[k] > pafRoughnessMax )
        {
            pafRoughnessMax=afWin[k];
        }
        if( afWin[k] < pafRoughnessMin )
        {
            pafRoughnessMin=afWin[k];
        }
    }
    return static_cast<float>(pafRoughnessMax - pafRoughnessMin);
}

/************************************************************************/
/* ==================================================================== */
/*                       GDALGeneric3x3Dataset                        */
/* ==================================================================== */
/************************************************************************/

template<class T>
class GDALGeneric3x3RasterBand;

template<class T>
class GDALGeneric3x3Dataset : public GDALDataset
{
    friend class GDALGeneric3x3RasterBand<T>;

    typename GDALGeneric3x3ProcessingAlg<T>::type pfnAlg;
    void*              pAlgData;
    GDALDatasetH       hSrcDS;
    GDALRasterBandH    hSrcBand;
    T*                 apafSourceBuf[3];
    int                bDstHasNoData;
    double             dfDstNoDataValue;
    int                nCurLine;
    bool               bComputeAtEdges;

  public:
                        GDALGeneric3x3Dataset(
                            GDALDatasetH hSrcDS,
                            GDALRasterBandH hSrcBand,
                            GDALDataType eDstDataType,
                            int bDstHasNoData,
                            double dfDstNoDataValue,
                            typename GDALGeneric3x3ProcessingAlg<T>::type pfnAlg,
                            void* pAlgData,
                            bool bComputeAtEdges );
                       ~GDALGeneric3x3Dataset();

    bool                InitOK() const { return apafSourceBuf[0] != NULL &&
                                                apafSourceBuf[1] != NULL &&
                                                apafSourceBuf[2] != NULL; }

    CPLErr      GetGeoTransform( double * padfGeoTransform ) override;
    const char *GetProjectionRef() override;
};

/************************************************************************/
/* ==================================================================== */
/*                    GDALGeneric3x3RasterBand                       */
/* ==================================================================== */
/************************************************************************/

template<class T>
class GDALGeneric3x3RasterBand : public GDALRasterBand
{
    friend class GDALGeneric3x3Dataset<T>;
    int bSrcHasNoData;
    T fSrcNoDataValue;
    int bIsSrcNoDataNan;
    GDALDataType eReadDT;

    void                    InitWithNoData( void* pImage );

  public:
                 GDALGeneric3x3RasterBand( GDALGeneric3x3Dataset<T> *poDS,
                                           GDALDataType eDstDataType );

    virtual CPLErr          IReadBlock( int, int, void * ) override;
    virtual double          GetNoDataValue( int* pbHasNoData ) override;
};

template<class T>
GDALGeneric3x3Dataset<T>::GDALGeneric3x3Dataset(
    GDALDatasetH hSrcDSIn,
    GDALRasterBandH hSrcBandIn,
    GDALDataType eDstDataType,
    int bDstHasNoDataIn,
    double dfDstNoDataValueIn,
    typename GDALGeneric3x3ProcessingAlg<T>::type pfnAlgIn,
    void* pAlgDataIn,
    bool bComputeAtEdgesIn ) :
    pfnAlg(pfnAlgIn),
    pAlgData(pAlgDataIn),
    hSrcDS(hSrcDSIn),
    hSrcBand(hSrcBandIn),
    bDstHasNoData(bDstHasNoDataIn),
    dfDstNoDataValue(dfDstNoDataValueIn),
    nCurLine(-1),
    bComputeAtEdges(bComputeAtEdgesIn)
{
    CPLAssert(eDstDataType == GDT_Byte || eDstDataType == GDT_Float32);

    nRasterXSize = GDALGetRasterXSize(hSrcDS);
    nRasterYSize = GDALGetRasterYSize(hSrcDS);

    SetBand(1, new GDALGeneric3x3RasterBand<T>(this, eDstDataType));

    apafSourceBuf[0] =
        static_cast<T *>(VSI_MALLOC2_VERBOSE(sizeof(T), nRasterXSize));
    apafSourceBuf[1] =
        static_cast<T *>(VSI_MALLOC2_VERBOSE(sizeof(T), nRasterXSize));
    apafSourceBuf[2] =
        static_cast<T *>(VSI_MALLOC2_VERBOSE(sizeof(T), nRasterXSize));
}

template<class T>
GDALGeneric3x3Dataset<T>::~GDALGeneric3x3Dataset()
{
    CPLFree(apafSourceBuf[0]);
    CPLFree(apafSourceBuf[1]);
    CPLFree(apafSourceBuf[2]);
}

template<class T>
CPLErr GDALGeneric3x3Dataset<T>::GetGeoTransform( double * padfGeoTransform )
{
    return GDALGetGeoTransform(hSrcDS, padfGeoTransform);
}

template<class T>
const char *GDALGeneric3x3Dataset<T>::GetProjectionRef()
{
    return GDALGetProjectionRef(hSrcDS);
}

template<class T>
GDALGeneric3x3RasterBand<T>::GDALGeneric3x3RasterBand(
    GDALGeneric3x3Dataset<T> *poDSIn,
    GDALDataType eDstDataType ) :
    bSrcHasNoData(FALSE),
    fSrcNoDataValue(0),
    bIsSrcNoDataNan(FALSE),
    eReadDT(GDT_Unknown)
{
    poDS = poDSIn;
    nBand = 1;
    eDataType = eDstDataType;
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    const double dfNoDataValue = GDALGetRasterNoDataValue(poDSIn->hSrcBand,
                                                          &bSrcHasNoData);
    if( std::numeric_limits<T>::is_integer )
    {
        eReadDT = GDT_Int32;
        if( bSrcHasNoData )
        {
            GDALDataType eSrcDT = GDALGetRasterDataType(poDSIn->hSrcBand);
            CPLAssert( eSrcDT == GDT_Byte ||
                       eSrcDT == GDT_UInt16 ||
                       eSrcDT == GDT_Int16 );
            const int nMinVal =
                (eSrcDT == GDT_Byte ) ? 0 : (eSrcDT == GDT_UInt16) ? 0 : -32768;
            const int nMaxVal =
                (eSrcDT == GDT_Byte )
                ? 255
                : (eSrcDT == GDT_UInt16) ? 65535 : 32767;

            if( fabs(dfNoDataValue - floor(dfNoDataValue + 0.5)) < 1e-2 &&
                dfNoDataValue >= nMinVal && dfNoDataValue <= nMaxVal )
            {
                fSrcNoDataValue = static_cast<T>(floor(dfNoDataValue + 0.5));
            }
            else
            {
                bSrcHasNoData = FALSE;
            }
        }
    }
    else
    {
        eReadDT = GDT_Float32;
        fSrcNoDataValue = static_cast<T>(dfNoDataValue);
        bIsSrcNoDataNan = bSrcHasNoData && CPLIsNan(dfNoDataValue);
    }
}

template<class T>
void GDALGeneric3x3RasterBand<T>::InitWithNoData(void* pImage)
{
    GDALGeneric3x3Dataset<T> * poGDS = (GDALGeneric3x3Dataset<T> *) poDS;
    if( eDataType == GDT_Byte )
    {
        for( int j = 0; j < nBlockXSize; j++ )
            ((GByte*)pImage)[j] = (GByte) poGDS->dfDstNoDataValue;
    }
    else
    {
        for( int j = 0; j < nBlockXSize; j++ )
            ((float*)pImage)[j] = static_cast<float>(poGDS->dfDstNoDataValue);
    }
}

template<class T>
CPLErr GDALGeneric3x3RasterBand<T>::IReadBlock( int /*nBlockXOff*/,
                                                int nBlockYOff,
                                                void *pImage )
{
    GDALGeneric3x3Dataset<T> * poGDS = (GDALGeneric3x3Dataset<T> *) poDS;

    if( poGDS->bComputeAtEdges && nRasterXSize >= 2 && nRasterYSize >= 2 )
    {
        if( nBlockYOff == 0 )
        {
            for( int i = 0; i < 2; i++ )
            {
                CPLErr eErr = GDALRasterIO( poGDS->hSrcBand,
                                    GF_Read,
                                    0, i, nBlockXSize, 1,
                                    poGDS->apafSourceBuf[i+1],
                                    nBlockXSize, 1,
                                    eReadDT,
                                    0, 0);
                if( eErr != CE_None )
                {
                    InitWithNoData(pImage);
                    return eErr;
                }
            }
            poGDS->nCurLine = 0;

            for( int j = 0; j < nRasterXSize; j++ )
            {
                int jmin = (j == 0) ? j : j - 1;
                int jmax = (j == nRasterXSize - 1) ? j : j + 1;

                T afWin[9] = {
                    INTERPOL(poGDS->apafSourceBuf[1][jmin],
                             poGDS->apafSourceBuf[2][jmin],
                             bSrcHasNoData, fSrcNoDataValue),
                    INTERPOL(poGDS->apafSourceBuf[1][j],
                             poGDS->apafSourceBuf[2][j],
                             bSrcHasNoData, fSrcNoDataValue),
                    INTERPOL(poGDS->apafSourceBuf[1][jmax],
                             poGDS->apafSourceBuf[2][jmax],
                             bSrcHasNoData, fSrcNoDataValue),
                    poGDS->apafSourceBuf[1][jmin],
                    poGDS->apafSourceBuf[1][j],
                    poGDS->apafSourceBuf[1][jmax],
                    poGDS->apafSourceBuf[2][jmin],
                    poGDS->apafSourceBuf[2][j],
                    poGDS->apafSourceBuf[2][jmax]
                };

                const float fVal =
                    ComputeVal(
                        CPL_TO_BOOL(bSrcHasNoData),
                        fSrcNoDataValue,
                        CPL_TO_BOOL(bIsSrcNoDataNan),
                        afWin,
                        static_cast<float>(poGDS->dfDstNoDataValue),
                        poGDS->pfnAlg,
                        poGDS->pAlgData,
                        poGDS->bComputeAtEdges);

                if( eDataType == GDT_Byte )
                    ((GByte*)pImage)[j] = (GByte) (fVal + 0.5);
                else
                    ((float*)pImage)[j] = fVal;
            }

            return CE_None;
        }
        else if( nBlockYOff == nRasterYSize - 1 )
        {
            if( poGDS->nCurLine != nRasterYSize - 2 )
            {
                for( int i = 0; i < 2; i++ )
                {
                    CPLErr eErr = GDALRasterIO(
                        poGDS->hSrcBand,
                        GF_Read,
                        0, nRasterYSize - 2 + i, nBlockXSize, 1,
                        poGDS->apafSourceBuf[i+1],
                        nBlockXSize, 1,
                        eReadDT,
                        0, 0);
                    if( eErr != CE_None )
                    {
                        InitWithNoData(pImage);
                        return eErr;
                    }
                }
            }

            for( int j = 0; j < nRasterXSize; j++ )
            {
                int jmin = (j == 0) ? j : j - 1;
                int jmax = (j == nRasterXSize - 1) ? j : j + 1;

                T afWin[9] = {
                    poGDS->apafSourceBuf[1][jmin],
                    poGDS->apafSourceBuf[1][j],
                    poGDS->apafSourceBuf[1][jmax],
                    poGDS->apafSourceBuf[2][jmin],
                    poGDS->apafSourceBuf[2][j],
                    poGDS->apafSourceBuf[2][jmax],
                    INTERPOL(poGDS->apafSourceBuf[2][jmin],
                             poGDS->apafSourceBuf[1][jmin],
                             bSrcHasNoData, fSrcNoDataValue),
                    INTERPOL(poGDS->apafSourceBuf[2][j],
                             poGDS->apafSourceBuf[1][j],
                             bSrcHasNoData, fSrcNoDataValue),
                    INTERPOL(poGDS->apafSourceBuf[2][jmax],
                             poGDS->apafSourceBuf[1][jmax],
                             bSrcHasNoData, fSrcNoDataValue)
                };

                const float fVal =
                    ComputeVal(
                        CPL_TO_BOOL(bSrcHasNoData),
                        fSrcNoDataValue,
                        CPL_TO_BOOL(bIsSrcNoDataNan),
                        afWin,
                        static_cast<float>(poGDS->dfDstNoDataValue),
                        poGDS->pfnAlg,
                        poGDS->pAlgData,
                        poGDS->bComputeAtEdges);

                if( eDataType == GDT_Byte )
                    ((GByte*)pImage)[j] = (GByte) (fVal + 0.5);
                else
                    ((float*)pImage)[j] = fVal;
            }

            return CE_None;
        }
    }
    else if( nBlockYOff == 0 || nBlockYOff == nRasterYSize - 1 )
    {
        InitWithNoData(pImage);
        return CE_None;
    }

    if( poGDS->nCurLine != nBlockYOff )
    {
        if( poGDS->nCurLine + 1 == nBlockYOff )
        {
            T* pafTmp = poGDS->apafSourceBuf[0];
            poGDS->apafSourceBuf[0] = poGDS->apafSourceBuf[1];
            poGDS->apafSourceBuf[1] = poGDS->apafSourceBuf[2];
            poGDS->apafSourceBuf[2] = pafTmp;

            CPLErr eErr = GDALRasterIO( poGDS->hSrcBand,
                                    GF_Read,
                                    0, nBlockYOff + 1, nBlockXSize, 1,
                                    poGDS->apafSourceBuf[2],
                                    nBlockXSize, 1,
                                    eReadDT,
                                    0, 0);

            if( eErr != CE_None )
            {
                InitWithNoData(pImage);
                return eErr;
            }
        }
        else
        {
            for( int i = 0; i < 3; i++ )
            {
                const CPLErr eErr =
                    GDALRasterIO(poGDS->hSrcBand,
                                 GF_Read,
                                 0, nBlockYOff + i - 1, nBlockXSize, 1,
                                 poGDS->apafSourceBuf[i],
                                 nBlockXSize, 1,
                                 eReadDT,
                                 0, 0);
                if( eErr != CE_None )
                {
                    InitWithNoData(pImage);
                    return eErr;
                }
            }
        }

        poGDS->nCurLine = nBlockYOff;
    }

    if( poGDS->bComputeAtEdges && nRasterXSize >= 2 )
    {
        int j = 0;
        T afWin[9] = {
            INTERPOL(poGDS->apafSourceBuf[0][j], poGDS->apafSourceBuf[0][j+1],
                     bSrcHasNoData, fSrcNoDataValue),
            poGDS->apafSourceBuf[0][j],
            poGDS->apafSourceBuf[0][j+1],
            INTERPOL(poGDS->apafSourceBuf[1][j], poGDS->apafSourceBuf[1][j+1],
                     bSrcHasNoData, fSrcNoDataValue),
            poGDS->apafSourceBuf[1][j],
            poGDS->apafSourceBuf[1][j+1],
            INTERPOL(poGDS->apafSourceBuf[2][j], poGDS->apafSourceBuf[2][j+1],
                     bSrcHasNoData, fSrcNoDataValue),
            poGDS->apafSourceBuf[2][j],
            poGDS->apafSourceBuf[2][j+1]
        };

        {
            const float fVal =
                ComputeVal(
                    CPL_TO_BOOL(bSrcHasNoData),
                    fSrcNoDataValue,
                    CPL_TO_BOOL(bIsSrcNoDataNan),
                    afWin,
                    static_cast<float>(poGDS->dfDstNoDataValue),
                    poGDS->pfnAlg,
                    poGDS->pAlgData,
                    poGDS->bComputeAtEdges);

            if( eDataType == GDT_Byte )
                ((GByte*)pImage)[j] = (GByte) (fVal + 0.5);
            else
                ((float*)pImage)[j] = fVal;
        }

        j = nRasterXSize - 1;

        afWin[0] = poGDS->apafSourceBuf[0][j-1];
        afWin[1] = poGDS->apafSourceBuf[0][j];
        afWin[2] = INTERPOL(poGDS->apafSourceBuf[0][j], poGDS->apafSourceBuf[0][j-1], bSrcHasNoData, fSrcNoDataValue);
        afWin[3] = poGDS->apafSourceBuf[1][j-1];
        afWin[4] = poGDS->apafSourceBuf[1][j];
        afWin[5] = INTERPOL(poGDS->apafSourceBuf[1][j], poGDS->apafSourceBuf[1][j-1], bSrcHasNoData, fSrcNoDataValue);
        afWin[6] = poGDS->apafSourceBuf[2][j-1];
        afWin[7] = poGDS->apafSourceBuf[2][j];
        afWin[8] = INTERPOL(poGDS->apafSourceBuf[2][j], poGDS->apafSourceBuf[2][j-1], bSrcHasNoData, fSrcNoDataValue);

        const float fVal =
            ComputeVal(
                CPL_TO_BOOL(bSrcHasNoData),
                fSrcNoDataValue,
                CPL_TO_BOOL(bIsSrcNoDataNan),
                afWin,
                static_cast<float>(poGDS->dfDstNoDataValue),
                poGDS->pfnAlg,
                poGDS->pAlgData,
                poGDS->bComputeAtEdges);

        if( eDataType == GDT_Byte )
            ((GByte*)pImage)[j] = (GByte) (fVal + 0.5);
        else
            ((float*)pImage)[j] = fVal;
    }
    else
    {
        if( eDataType == GDT_Byte )
        {
            ((GByte*)pImage)[0] = (GByte) poGDS->dfDstNoDataValue;
            if( nBlockXSize > 1 )
                ((GByte*)pImage)[nBlockXSize - 1] = (GByte) poGDS->dfDstNoDataValue;
        }
        else
        {
            ((float*)pImage)[0] = static_cast<float>(poGDS->dfDstNoDataValue);
            if( nBlockXSize > 1 )
                ((float*)pImage)[nBlockXSize - 1] =
                    static_cast<float>(poGDS->dfDstNoDataValue);
        }
    }

    for( int j = 1; j < nBlockXSize - 1; j++ )
    {
        T afWin[9] = {
            poGDS->apafSourceBuf[0][j-1],
            poGDS->apafSourceBuf[0][j],
            poGDS->apafSourceBuf[0][j+1],
            poGDS->apafSourceBuf[1][j-1],
            poGDS->apafSourceBuf[1][j],
            poGDS->apafSourceBuf[1][j+1],
            poGDS->apafSourceBuf[2][j-1],
            poGDS->apafSourceBuf[2][j],
            poGDS->apafSourceBuf[2][j+1]
        };

        const float fVal =
            ComputeVal(
                CPL_TO_BOOL(bSrcHasNoData),
                fSrcNoDataValue,
                CPL_TO_BOOL(bIsSrcNoDataNan),
                afWin,
                static_cast<float>(poGDS->dfDstNoDataValue),
                poGDS->pfnAlg,
                poGDS->pAlgData,
                poGDS->bComputeAtEdges);

        if( eDataType == GDT_Byte )
            ((GByte*)pImage)[j] = (GByte) (fVal + 0.5);
        else
            ((float*)pImage)[j] = fVal;
    }

    return CE_None;
}

template<class T>
double GDALGeneric3x3RasterBand<T>::GetNoDataValue( int* pbHasNoData )
{
    GDALGeneric3x3Dataset<T> * poGDS = (GDALGeneric3x3Dataset<T> *) poDS;
    if( pbHasNoData )
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
    if( EQUAL(pszProcessing, "shade") || EQUAL(pszProcessing, "hillshade") )
    {
        return HILL_SHADE;
    }
    else if( EQUAL(pszProcessing, "slope") )
    {
        return SLOPE;
    }
    else if( EQUAL(pszProcessing, "aspect") )
    {
        return ASPECT;
    }
    else if( EQUAL(pszProcessing, "color-relief") )
    {
        return COLOR_RELIEF;
    }
    else if( EQUAL(pszProcessing, "TRI") )
    {
        return TRI;
    }
    else if( EQUAL(pszProcessing, "TPI") )
    {
        return TPI;
    }
    else if( EQUAL(pszProcessing, "roughness") )
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

GDALDatasetH GDALDEMProcessing( const char *pszDest,
                                GDALDatasetH hSrcDataset,
                                const char* pszProcessing,
                                const char* pszColorFilename,
                                const GDALDEMProcessingOptions *psOptionsIn,
                                int *pbUsageError )
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

    if( psOptionsIn->bCombined && eUtilityMode != HILL_SHADE )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "-combined can only be used with hillshade");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }

    if( psOptionsIn->bMultiDirectional && eUtilityMode != HILL_SHADE )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "-multidirectional can only be used with hillshade");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }

    GDALDEMProcessingOptions* psOptionsToFree = NULL;
    const GDALDEMProcessingOptions* psOptions = psOptionsIn;
    if( !psOptions )
    {
        psOptionsToFree = GDALDEMProcessingOptionsNew(NULL, NULL);
        psOptions = psOptionsToFree;
    }

    double adfGeoTransform[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

    const int nXSize = GDALGetRasterXSize(hSrcDataset);
    const int nYSize = GDALGetRasterYSize(hSrcDataset);

    if( psOptions->nBand <= 0 ||
        psOptions->nBand > GDALGetRasterCount(hSrcDataset) )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Unable to fetch band #%d", psOptions->nBand );
        GDALDEMProcessingOptionsFree(psOptionsToFree);
        return NULL;
    }
    GDALRasterBandH hSrcBand =
        GDALGetRasterBand( hSrcDataset, psOptions->nBand );

    GDALGetGeoTransform(hSrcDataset, adfGeoTransform);

    GDALDriverH hDriver = GDALGetDriverByName(psOptions->pszFormat);
    if( hDriver == NULL
        || (GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) == NULL &&
            GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY, NULL ) == NULL))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Output driver `%s' not recognised to have output support.",
                 psOptions->pszFormat );
        fprintf( stderr, "The following format drivers are configured\n"
                 "and support output:\n" );

        for( int iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
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

    double dfDstNoDataValue = 0.0;
    bool bDstHasNoData = false;
    void* pData = NULL;
    GDALGeneric3x3ProcessingAlg<float>::type pfnAlgFloat = NULL;
    GDALGeneric3x3ProcessingAlg<GInt32>::type pfnAlgInt32 = NULL;
    GDALGeneric3x3ProcessingAlg_multisample<GInt32>::type pfnAlgInt32_multisample = NULL;

    if( eUtilityMode == HILL_SHADE && psOptions->bMultiDirectional )
    {
        dfDstNoDataValue = 0;
        bDstHasNoData = true;
        pData = GDALCreateHillshadeMultiDirectionalData(adfGeoTransform,
                                                        psOptions->z,
                                                        psOptions->scale,
                                                        psOptions->alt,
                                                        psOptions->bZevenbergenThorne);
        if( psOptions->bZevenbergenThorne )
        {
            pfnAlgFloat = GDALHillshadeMultiDirectionalAlg<float, ZEVENBERGEN_THORNE>;
            pfnAlgInt32 = GDALHillshadeMultiDirectionalAlg<GInt32, ZEVENBERGEN_THORNE>;
        }
        else
        {
            pfnAlgFloat = GDALHillshadeMultiDirectionalAlg<float, HORN>;
            pfnAlgInt32 = GDALHillshadeMultiDirectionalAlg<GInt32, HORN>;
        }
    }
    else if( eUtilityMode == HILL_SHADE )
    {
        dfDstNoDataValue = 0;
        bDstHasNoData = true;
        pData = GDALCreateHillshadeData(adfGeoTransform,
                                        psOptions->z,
                                        psOptions->scale,
                                        psOptions->alt,
                                        psOptions->az,
                                        psOptions->bZevenbergenThorne);
        if( psOptions->bZevenbergenThorne )
        {
            if( !psOptions->bCombined )
            {
                pfnAlgFloat = GDALHillshadeAlg<float, ZEVENBERGEN_THORNE>;
                pfnAlgInt32 = GDALHillshadeAlg<GInt32, ZEVENBERGEN_THORNE>;
            }
            else
            {
                pfnAlgFloat = GDALHillshadeCombinedAlg<float, ZEVENBERGEN_THORNE>;
                pfnAlgInt32 = GDALHillshadeCombinedAlg<GInt32, ZEVENBERGEN_THORNE>;
            }
        }
        else
        {
            if( !psOptions->bCombined )
            {
                if( adfGeoTransform[1] == -adfGeoTransform[5] )
                {
                    pfnAlgFloat = GDALHillshadeAlg_same_res<float>;
                    pfnAlgInt32 = GDALHillshadeAlg_same_res<GInt32>;
#ifdef HAVE_16_SSE_REG
                    pfnAlgInt32_multisample =
                                GDALHillshadeAlg_same_res_multisample<GInt32>;
#endif
                }
                else
                {
                    pfnAlgFloat = GDALHillshadeAlg<float, HORN>;
                    pfnAlgInt32 = GDALHillshadeAlg<GInt32, HORN>;
                }
            }
            else
            {
                pfnAlgFloat = GDALHillshadeCombinedAlg<float, HORN>;
                pfnAlgInt32 = GDALHillshadeCombinedAlg<GInt32, HORN>;
            }
        }
    }
    else if( eUtilityMode == SLOPE )
    {
        dfDstNoDataValue = -9999;
        bDstHasNoData = true;

        pData = GDALCreateSlopeData(adfGeoTransform, psOptions->scale, psOptions->slopeFormat);
        if( psOptions->bZevenbergenThorne )
        {
            pfnAlgFloat = GDALSlopeZevenbergenThorneAlg<float>;
            pfnAlgInt32 = GDALSlopeZevenbergenThorneAlg<GInt32>;
        }
        else
        {
            pfnAlgFloat = GDALSlopeHornAlg<float>;
            pfnAlgInt32 = GDALSlopeHornAlg<GInt32>;
        }
    }

    else if( eUtilityMode == ASPECT )
    {
        if( !psOptions->bZeroForFlat )
        {
            dfDstNoDataValue = -9999;
            bDstHasNoData = true;
        }

        pData = GDALCreateAspectData(psOptions->bAngleAsAzimuth);
        if( psOptions->bZevenbergenThorne )
        {
            pfnAlgFloat = GDALAspectZevenbergenThorneAlg<float>;
            pfnAlgInt32 = GDALAspectZevenbergenThorneAlg<GInt32>;
        }
        else
        {
            pfnAlgFloat = GDALAspectAlg<float>;
            pfnAlgInt32 = GDALAspectAlg<GInt32>;
        }
    }
    else if( eUtilityMode == TRI )
    {
        dfDstNoDataValue = -9999;
        bDstHasNoData = true;
        pfnAlgFloat = GDALTRIAlg<float>;
        pfnAlgInt32 = GDALTRIAlg<GInt32>;
    }
    else if( eUtilityMode == TPI )
    {
        dfDstNoDataValue = -9999;
        bDstHasNoData = true;
        pfnAlgFloat = GDALTPIAlg<float>;
        pfnAlgInt32 = GDALTPIAlg<GInt32>;
    }
    else if( eUtilityMode == ROUGHNESS )
    {
        dfDstNoDataValue = -9999;
        bDstHasNoData = true;
        pfnAlgFloat = GDALRoughnessAlg<float>;
        pfnAlgInt32 = GDALRoughnessAlg<GInt32>;
    }

    const GDALDataType eDstDataType =
        (eUtilityMode == HILL_SHADE ||
         eUtilityMode == COLOR_RELIEF)
        ? GDT_Byte
        : GDT_Float32;

    if( EQUAL(psOptions->pszFormat, "VRT") )
    {
        if( eUtilityMode == COLOR_RELIEF )
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
    bool bForceUseIntermediateDataset = false;

    GDALProgressFunc pfnProgress = psOptions->pfnProgress;
    void* pProgressData = psOptions->pProgressData;

    if( EQUAL(psOptions->pszFormat, "GTiff") )
    {
        if( !EQUAL(CSLFetchNameValueDef(psOptions->papszCreateOptions, "COMPRESS", "NONE"), "NONE") &&
            CPLTestBool(CSLFetchNameValueDef(psOptions->papszCreateOptions, "TILED", "NO")) )
        {
            bForceUseIntermediateDataset = true;
        }
        else if( strcmp(pszDest, "/vsistdout/") == 0 )
        {
            bForceUseIntermediateDataset = true;
            pfnProgress = GDALDummyProgress;
            pProgressData = NULL;
        }
#ifdef S_ISFIFO
        else
        {
            VSIStatBufL sStat;
            if( VSIStatExL(pszDest, &sStat,
                           VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0 &&
                S_ISFIFO(sStat.st_mode) )
            {
                bForceUseIntermediateDataset = true;
            }
        }
#endif
    }

    const GDALDataType eSrcDT = GDALGetRasterDataType(hSrcBand);

    if( GDALGetMetadataItem( hDriver, GDAL_DCAP_RASTER, NULL) != NULL &&
        ((bForceUseIntermediateDataset ||
          GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) == NULL) &&
         GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY, NULL ) != NULL) )
    {
        GDALDatasetH hIntermediateDataset = NULL;

        if( eUtilityMode == COLOR_RELIEF )
        {
            GDALColorReliefDataset* poDS =
                new GDALColorReliefDataset(hSrcDataset,
                                           hSrcBand,
                                           pszColorFilename,
                                           psOptions->eColorSelectionMode,
                                           psOptions->bAddAlpha);
            if( !(poDS->InitOK()) )
            {
                delete poDS;
                CPLFree(pData);
                GDALDEMProcessingOptionsFree(psOptionsToFree);
                return NULL;
            }
            hIntermediateDataset = (GDALDatasetH)poDS;
        }
        else
        {
            if( eSrcDT == GDT_Byte ||
                eSrcDT == GDT_Int16 ||
                eSrcDT == GDT_UInt16 )
            {
                GDALGeneric3x3Dataset<GInt32>* poDS =
                    new GDALGeneric3x3Dataset<GInt32>(hSrcDataset, hSrcBand,
                                            eDstDataType,
                                            bDstHasNoData,
                                            dfDstNoDataValue,
                                            pfnAlgInt32,
                                            pData,
                                            psOptions->bComputeAtEdges);

                if( !(poDS->InitOK()) )
                {
                    delete poDS;
                    CPLFree(pData);
                    GDALDEMProcessingOptionsFree(psOptionsToFree);
                    return NULL;
                }
                hIntermediateDataset = (GDALDatasetH)poDS;
            }
            else
            {
                GDALGeneric3x3Dataset<float>* poDS =
                    new GDALGeneric3x3Dataset<float>(hSrcDataset, hSrcBand,
                                            eDstDataType,
                                            bDstHasNoData,
                                            dfDstNoDataValue,
                                            pfnAlgFloat,
                                            pData,
                                            psOptions->bComputeAtEdges);

                if( !(poDS->InitOK()) )
                {
                    delete poDS;
                    CPLFree(pData);
                    GDALDEMProcessingOptionsFree(psOptionsToFree);
                    return NULL;
                }
                hIntermediateDataset = (GDALDatasetH)poDS;
            }
        }

        GDALDatasetH hOutDS = GDALCreateCopy(
            hDriver, pszDest, hIntermediateDataset,
            TRUE, psOptions->papszCreateOptions,
            pfnProgress, pProgressData );

        GDALClose(hIntermediateDataset);

        CPLFree(pData);

        GDALDEMProcessingOptionsFree(psOptionsToFree);
        return hOutDS;
    }

    const int nDstBands =
        eUtilityMode == COLOR_RELIEF
        ? ((psOptions->bAddAlpha) ? 4 : 3)
        : 1;

    GDALDatasetH hDstDataset =
        GDALCreate( hDriver,
                    pszDest,
                    nXSize,
                    nYSize,
                    nDstBands,
                    eDstDataType,
                    psOptions->papszCreateOptions );

    if( hDstDataset == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to create dataset %s", pszDest );
        GDALDEMProcessingOptionsFree(psOptionsToFree);
        CPLFree(pData);
        return NULL;
    }

    GDALRasterBandH hDstBand = GDALGetRasterBand( hDstDataset, 1 );

    GDALSetGeoTransform(hDstDataset, adfGeoTransform);
    GDALSetProjection(hDstDataset, GDALGetProjectionRef(hSrcDataset));

    if( eUtilityMode == COLOR_RELIEF )
    {
        GDALColorRelief (hSrcBand,
                         GDALGetRasterBand(hDstDataset, 1),
                         GDALGetRasterBand(hDstDataset, 2),
                         GDALGetRasterBand(hDstDataset, 3),
                         psOptions->bAddAlpha ? GDALGetRasterBand(hDstDataset, 4) : NULL,
                         pszColorFilename,
                         psOptions->eColorSelectionMode,
                         pfnProgress, pProgressData);
    }
    else
    {
        if( bDstHasNoData )
            GDALSetRasterNoDataValue(hDstBand, dfDstNoDataValue);

        if( eSrcDT == GDT_Byte || eSrcDT == GDT_Int16 || eSrcDT == GDT_UInt16 )
        {
            GDALGeneric3x3Processing<GInt32>(hSrcBand, hDstBand,
                                             pfnAlgInt32,
                                             pfnAlgInt32_multisample,
                                             pData,
                                             psOptions->bComputeAtEdges,
                                             pfnProgress, pProgressData);
        }
        else
        {
            GDALGeneric3x3Processing<float>(hSrcBand, hDstBand,
                                            pfnAlgFloat,
                                            NULL,
                                            pData,
                                            psOptions->bComputeAtEdges,
                                            pfnProgress, pProgressData);
        }
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
 *                  The accepted options are the ones of the <a href="gdaldem.html">gdaldem</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be NULL),
 *                           otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALDEMProcessingOptionsForBinaryNew() prior to this function. Will be
 *                           filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALDEMProcessingOptions struct. Must be freed with GDALDEMProcessingOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALDEMProcessingOptions *GDALDEMProcessingOptionsNew(
    char** papszArgv,
    GDALDEMProcessingOptionsForBinary* psOptionsForBinary )
{
    GDALDEMProcessingOptions *psOptions =
        static_cast<GDALDEMProcessingOptions *>(
            CPLCalloc(1, sizeof(GDALDEMProcessingOptions)));
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
    psOptions->bAddAlpha = false;
    psOptions->bZeroForFlat = false;
    psOptions->bAngleAsAzimuth = true;
    psOptions->eColorSelectionMode = COLOR_SELECTION_INTERPOLATE;
    psOptions->bComputeAtEdges = false;
    psOptions->bZevenbergenThorne = false;
    psOptions->bCombined = false;
    psOptions->bMultiDirectional = false;
    psOptions->nBand = 1;
    psOptions->papszCreateOptions = NULL;
    bool bAzimuthSpecified = false;

/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    int argc = CSLCount(papszArgv);
    for( int i = 0; papszArgv != NULL && i < argc; i++ )
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

        if( i < argc-1 && (EQUAL(papszArgv[i],"-of") || EQUAL(papszArgv[i],"-f")) )
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

        else if( (EQUAL(papszArgv[i], "--z") || EQUAL(papszArgv[i], "-z")) &&
                 i + 1 < argc )
        {
            ++i;
            if( !ArgIsNumeric(papszArgv[i]) )
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Numeric value expected for -z");
                GDALDEMProcessingOptionsFree(psOptions);
                return NULL;
            }
            psOptions->z = CPLAtof(papszArgv[i]);
        }
        else if( EQUAL(papszArgv[i], "-p") )
        {
            psOptions->slopeFormat = 0;
        }
        else if( EQUAL(papszArgv[i], "-alg") && i+1<argc )
        {
            i++;
            if( EQUAL(papszArgv[i], "ZevenbergenThorne") )
            {
                psOptions->bZevenbergenThorne = true;
            }
            else if( !EQUAL(papszArgv[i], "Horn") )
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Numeric value expected for %s", papszArgv[i-1]);
                GDALDEMProcessingOptionsFree(psOptions);
                return NULL;
            }
        }
        else if( EQUAL(papszArgv[i], "-trigonometric") )
        {
            psOptions->bAngleAsAzimuth = false;
        }
        else if( EQUAL(papszArgv[i], "-zero_for_flat") )
        {
            psOptions->bZeroForFlat = true;
        }
        else if( EQUAL(papszArgv[i], "-exact_color_entry") )
        {
            psOptions->eColorSelectionMode = COLOR_SELECTION_EXACT_ENTRY;
        }
        else if( EQUAL(papszArgv[i], "-nearest_color_entry") )
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
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Numeric value expected for %s", papszArgv[i-1]);
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
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Numeric value expected for %s", papszArgv[i-1]);
                GDALDEMProcessingOptionsFree(psOptions);
                return NULL;
            }
            bAzimuthSpecified = true;
            psOptions->az = CPLAtof(papszArgv[i]);
        }
        else if(
            (EQUAL(papszArgv[i], "--alt") ||
             EQUAL(papszArgv[i], "-alt") ||
             EQUAL(papszArgv[i], "--altitude") ||
             EQUAL(papszArgv[i], "-altitude")) && i+1<argc
          )
        {
            ++i;
            if( !ArgIsNumeric(papszArgv[i]) )
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Numeric value expected for %s", papszArgv[i-1]);
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
            psOptions->bCombined = true;
        }
        else if( EQUAL(papszArgv[i], "-multidirectional") ||
                 EQUAL(papszArgv[i], "--multidirectional") )
        {
            psOptions->bMultiDirectional = true;
        }
        else if(
                 EQUAL(papszArgv[i], "-alpha"))
        {
            psOptions->bAddAlpha = true;
        }
        else if(
                 EQUAL(papszArgv[i], "-compute_edges"))
        {
            psOptions->bComputeAtEdges = true;
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
            psOptions->papszCreateOptions =
                CSLAddString( psOptions->papszCreateOptions, papszArgv[++i] );
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
        else if( psOptionsForBinary &&
                 eUtilityMode == COLOR_RELIEF &&
                 psOptionsForBinary->pszColorFilename == NULL )
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

    if( psOptions->bMultiDirectional &&
        psOptions->bCombined)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "-multidirectional and -combined cannot be used together");
        GDALDEMProcessingOptionsFree(psOptions);
        return NULL;
    }

    if( psOptions->bMultiDirectional && bAzimuthSpecified )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "-multidirectional and -az cannot be used together");
        GDALDEMProcessingOptionsFree(psOptions);
        return NULL;
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

void GDALDEMProcessingOptionsFree( GDALDEMProcessingOptions *psOptions )
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
                                          GDALProgressFunc pfnProgress,
                                          void *pProgressData )
{
    psOptions->pfnProgress = pfnProgress;
    psOptions->pProgressData = pProgressData;
}
