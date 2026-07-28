/******************************************************************************
 *
 * Project:  GDAL DEM Utilities
 * Purpose:
 * Authors:  Matthew Perry, perrygeo at gmail.com
 *           Even Rouault, even dot rouault at spatialys.com
 *           Howard Butler, hobu.inc at gmail.com
 *           Chris Yesson, chris dot yesson at ioz dot ac dot uk
 *
 ******************************************************************************
 * Copyright (c) 2006, 2009 Matthew Perry
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
 * Portions derived from GRASS 4.1 (public domain) See
 * http://trac.osgeo.org/gdal/ticket/2975 for more information regarding
 * history of this code
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************
 *
 * Slope and aspect calculations based on original method for GRASS GIS 4.1
 * by Michael Shapiro, U.S.Army Construction Engineering Research Laboratory
 *    Olga Waupotitsch, U.S.Army Construction Engineering Research Laboratory
 *    Marjorie Larson, U.S.Army Construction Engineering Research Laboratory
 * as found in GRASS's r.slope.aspect module.
 *
 * Horn's formula is used to find the first order derivatives in x and y
 *directions for slope and aspect calculations: Horn, B. K. P. (1981). "Hill
 *Shading and the Reflectance Map", Proceedings of the IEEE, 69(1):14-47.
 *
 * Other reference :
 * Burrough, P.A. and McDonell, R.A., 1998. Principles of Geographical
 *Information Systems. p. 190.
 *
 * Shaded relief based on original method for GRASS GIS 4.1 by Jim Westervelt,
 * U.S. Army Construction Engineering Research Laboratory
 * as found in GRASS's r.shaded.relief (formerly shade.rel.sh) module.
 * ref: "r.mapcalc: An Algebra for GIS and Image Processing",
 * by Michael Shapiro and Jim Westervelt, U.S. Army Construction Engineering
 * Research Laboratory (March/1991)
 *
 * Color table of named colors and lookup code derived from
 *src/libes/gis/named_colr.c of GRASS 4.1
 *
 * TRI -
 * For bathymetric use cases, implements
 * Terrain Ruggedness Index is as described in Wilson et al. (2007)
 * this is based on the method of Valentine et al. (2004)
 *
 * For terrestrial use cases, implements
 * Riley, S.J., De Gloria, S.D., Elliot, R. (1999): A Terrain Ruggedness
 * that Quantifies Topographic Heterogeneity. Intermountain Journal of Science,
 *Vol.5, No.1-4, pp.23-27
 *
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
#include "commonutils.h"
#include "gdalargumentparser.h"

#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "cpl_error.h"
#include "cpl_float.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "vrtdataset.h"

#if defined(__x86_64__) || defined(_M_X64)
#define HAVE_16_SSE_REG
#include "emmintrin.h"
#include "gdalsse_priv.h"
#elif defined(USE_NEON_OPTIMIZATIONS)
#define HAVE_16_SSE_REG
#define USE_SSE2
#include "include_sse2neon.h"
#include "gdalsse_priv.h"
#endif

constexpr float kfDegToRad = static_cast<float>(M_PI / 180.0);
constexpr float kfRadToDeg = static_cast<float>(180.0 / M_PI);

typedef enum
{
    COLOR_SELECTION_INTERPOLATE,
    COLOR_SELECTION_NEAREST_ENTRY,
    COLOR_SELECTION_EXACT_ENTRY
} ColorSelectionMode;

namespace gdal::GDALDEM
{
enum class GradientAlg
{
    HORN,
    ZEVENBERGEN_THORNE,
};

enum class TRIAlg
{
    WILSON,
    RILEY,
};
}  // namespace gdal::GDALDEM

using namespace gdal::GDALDEM;

struct GDALDEMProcessingOptions
{
    /*! output format. Use the short format name. */
    std::string osFormat{};

    /*! the progress function to use */
    GDALProgressFunc pfnProgress = nullptr;

    /*! pointer to the progress data variable */
    void *pProgressData = nullptr;

    double z = 1.0;
    double globalScale = std::numeric_limits<
        double>::quiet_NaN();  // when set, copied to xscale and yscale
    double xscale = std::numeric_limits<double>::quiet_NaN();
    double yscale = std::numeric_limits<double>::quiet_NaN();
    double az = 315.0;
    double alt = 45.0;
    bool bSlopeFormatUseDegrees =
        true;  // false = 'percent' or true = 'degrees'
    bool bAddAlpha = false;
    bool bZeroForFlat = false;
    bool bAngleAsAzimuth = true;
    ColorSelectionMode eColorSelectionMode = COLOR_SELECTION_INTERPOLATE;
    bool bComputeAtEdges = false;
    bool bGradientAlgSpecified = false;
    GradientAlg eGradientAlg = GradientAlg::HORN;
    bool bTRIAlgSpecified = false;
    TRIAlg eTRIAlg = TRIAlg::RILEY;
    bool bCombined = false;
    bool bIgor = false;
    bool bMultiDirectional = false;
    CPLStringList aosCreationOptions{};
    int nBand = 1;
};

/************************************************************************/
/*                         AlgorithmParameters                          */
/************************************************************************/

struct AlgorithmParameters
{
    AlgorithmParameters() = default;
    virtual ~AlgorithmParameters();
    AlgorithmParameters(const AlgorithmParameters &) = default;
    AlgorithmParameters &operator=(const AlgorithmParameters &) = delete;
    AlgorithmParameters(AlgorithmParameters &&) = delete;
    AlgorithmParameters &operator=(AlgorithmParameters &&) = delete;

    virtual std::unique_ptr<AlgorithmParameters>
    CreateScaledParameters(double dfXRatio, double dfYRatio) = 0;
};

AlgorithmParameters::~AlgorithmParameters() = default;

/************************************************************************/
/*                             ComputeVal()                             */
/************************************************************************/

template <class T> struct GDALGeneric3x3ProcessingAlg
{
    typedef float (*type)(const T *pafWindow, float fDstNoDataValue,
                          const AlgorithmParameters *pData);
};

template <class T> struct GDALGeneric3x3ProcessingAlg_multisample
{
    typedef int (*type)(const T *pafFirstLine, const T *pafSecondLine,
                        const T *pafThirdLine, int nXSize,
                        const AlgorithmParameters *pData, float *pafOutputBuf);
};

template <class T>
static float ComputeVal(bool bSrcHasNoData, T fSrcNoDataValue,
                        bool bIsSrcNoDataNan, T *afWin, float fDstNoDataValue,
                        typename GDALGeneric3x3ProcessingAlg<T>::type pfnAlg,
                        const AlgorithmParameters *pData, bool bComputeAtEdges);

template <>
float ComputeVal(bool bSrcHasNoData, float fSrcNoDataValue,
                 bool bIsSrcNoDataNan, float *afWin, float fDstNoDataValue,
                 GDALGeneric3x3ProcessingAlg<float>::type pfnAlg,
                 const AlgorithmParameters *pData, bool bComputeAtEdges)
{
    if (bSrcHasNoData &&
        ((!bIsSrcNoDataNan && ARE_REAL_EQUAL(afWin[4], fSrcNoDataValue)) ||
         (bIsSrcNoDataNan && std::isnan(afWin[4]))))
    {
        return fDstNoDataValue;
    }
    else if (bSrcHasNoData)
    {
        for (int k = 0; k < 9; k++)
        {
            if ((!bIsSrcNoDataNan &&
                 ARE_REAL_EQUAL(afWin[k], fSrcNoDataValue)) ||
                (bIsSrcNoDataNan && std::isnan(afWin[k])))
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

template <>
float ComputeVal(bool bSrcHasNoData, GInt32 fSrcNoDataValue,
                 bool /* bIsSrcNoDataNan */, GInt32 *afWin,
                 float fDstNoDataValue,
                 GDALGeneric3x3ProcessingAlg<GInt32>::type pfnAlg,
                 const AlgorithmParameters *pData, bool bComputeAtEdges)
{
    if (bSrcHasNoData && afWin[4] == fSrcNoDataValue)
    {
        return fDstNoDataValue;
    }
    else if (bSrcHasNoData)
    {
        for (int k = 0; k < 9; k++)
        {
            if (afWin[k] == fSrcNoDataValue)
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
/*                              INTERPOL()                              */
/************************************************************************/

template <class T>
static T INTERPOL(T a, T b, int bSrcHasNodata, T fSrcNoDataValue);

template <>
float INTERPOL(float a, float b, int bSrcHasNoData, float fSrcNoDataValue)
{
    if (bSrcHasNoData && (ARE_REAL_EQUAL(a, fSrcNoDataValue) ||
                          ARE_REAL_EQUAL(b, fSrcNoDataValue)))
        return fSrcNoDataValue;
    const float fVal = 2 * a - b;
    if (bSrcHasNoData && ARE_REAL_EQUAL(fVal, fSrcNoDataValue))
        return fSrcNoDataValue *
               (1 + 3 * std::numeric_limits<float>::epsilon());
    return fVal;
}

template <>
GInt32 INTERPOL(GInt32 a, GInt32 b, int bSrcHasNoData, GInt32 fSrcNoDataValue)
{
    if (bSrcHasNoData && ((a == fSrcNoDataValue) || (b == fSrcNoDataValue)))
        return fSrcNoDataValue;
    const int nVal = static_cast<int>(
        std::clamp<int64_t>(2 * static_cast<int64_t>(a) - b, INT_MIN, INT_MAX));
    if (bSrcHasNoData && fSrcNoDataValue == nVal)
        return nVal == INT_MAX ? INT_MAX - 1 : nVal + 1;
    return nVal;
}

/************************************************************************/
/*                      GDALGeneric3x3Processing()                      */
/************************************************************************/

template <class T>
static CPLErr GDALGeneric3x3Processing(
    GDALRasterBandH hSrcBand, GDALRasterBandH hDstBand,
    typename GDALGeneric3x3ProcessingAlg<T>::type pfnAlg,
    typename GDALGeneric3x3ProcessingAlg_multisample<T>::type
        pfnAlg_multisample,
    std::unique_ptr<AlgorithmParameters> pData, bool bComputeAtEdges,
    GDALProgressFunc pfnProgress, void *pProgressData)
{
    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    /* -------------------------------------------------------------------- */
    /*      Initialize progress counter.                                    */
    /* -------------------------------------------------------------------- */
    if (!pfnProgress(0.0, nullptr, pProgressData))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return CE_Failure;
    }

    const int nXSize = GDALGetRasterBandXSize(hSrcBand);
    const int nYSize = GDALGetRasterBandYSize(hSrcBand);

    // 1 line destination buffer.
    float *pafOutputBuf =
        static_cast<float *>(VSI_MALLOC2_VERBOSE(sizeof(float), nXSize));
    // 3 line rotating source buffer.
    T *pafThreeLineWin =
        static_cast<T *>(VSI_MALLOC2_VERBOSE(3 * sizeof(T), nXSize));
    if (pafOutputBuf == nullptr || pafThreeLineWin == nullptr)
    {
        VSIFree(pafOutputBuf);
        VSIFree(pafThreeLineWin);
        return CE_Failure;
    }

    GDALDataType eReadDT;
    int bSrcHasNoData = FALSE;
    const double dfNoDataValue =
        GDALGetRasterNoDataValue(hSrcBand, &bSrcHasNoData);

    bool bIsSrcNoDataNan = false;
    T fSrcNoDataValue = 0;
    if constexpr (std::numeric_limits<T>::is_integer)
    {
        eReadDT = GDT_Int32;
        if (bSrcHasNoData)
        {
            GDALDataType eSrcDT = GDALGetRasterDataType(hSrcBand);
            CPLAssert(eSrcDT == GDT_UInt8 || eSrcDT == GDT_UInt16 ||
                      eSrcDT == GDT_Int16);
            const int nMinVal = (eSrcDT == GDT_UInt8)    ? 0
                                : (eSrcDT == GDT_UInt16) ? 0
                                                         : -32768;
            const int nMaxVal = (eSrcDT == GDT_UInt8)    ? 255
                                : (eSrcDT == GDT_UInt16) ? 65535
                                                         : 32767;

            if (fabs(dfNoDataValue - floor(dfNoDataValue + 0.5)) < 1e-2 &&
                dfNoDataValue >= nMinVal && dfNoDataValue <= nMaxVal)
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
        bIsSrcNoDataNan = bSrcHasNoData && std::isnan(dfNoDataValue);
    }

    int bDstHasNoData = FALSE;
    float fDstNoDataValue =
        static_cast<float>(GDALGetRasterNoDataValue(hDstBand, &bDstHasNoData));
    if (!bDstHasNoData)
        fDstNoDataValue = 0.0;

    int nLine1Off = 0;
    int nLine2Off = nXSize;
    int nLine3Off = 2 * nXSize;

    // Move a 3x3 pafWindow over each cell
    // (where the cell in question is #4)
    //
    //      0 1 2
    //      3 4 5
    //      6 7 8

    /* Preload the first 2 lines */

    bool abLineHasNoDataValue[3] = {CPL_TO_BOOL(bSrcHasNoData),
                                    CPL_TO_BOOL(bSrcHasNoData),
                                    CPL_TO_BOOL(bSrcHasNoData)};

    for (int i = 0; i < 2 && i < nYSize; i++)
    {
        if (GDALRasterIO(hSrcBand, GF_Read, 0, i, nXSize, 1,
                         pafThreeLineWin + i * nXSize, nXSize, 1, eReadDT, 0,
                         0) != CE_None)
        {
            CPLFree(pafOutputBuf);
            CPLFree(pafThreeLineWin);

            return CE_Failure;
        }
        if (bSrcHasNoData)
        {
            abLineHasNoDataValue[i] = false;
            if constexpr (std::numeric_limits<T>::is_integer)
            {
                for (int iX = 0; iX < nXSize; iX++)
                {
                    if (pafThreeLineWin[i * nXSize + iX] == fSrcNoDataValue)
                    {
                        abLineHasNoDataValue[i] = true;
                        break;
                    }
                }
            }
            else
            {
                for (int iX = 0; iX < nXSize; iX++)
                {
                    if (pafThreeLineWin[i * nXSize + iX] == fSrcNoDataValue ||
                        std::isnan(pafThreeLineWin[i * nXSize + iX]))
                    {
                        abLineHasNoDataValue[i] = true;
                        break;
                    }
                }
            }
        }
    }

    CPLErr eErr = CE_None;
    if (bComputeAtEdges && nXSize >= 2 && nYSize >= 2)
    {
        for (int j = 0; j < nXSize; j++)
        {
            int jmin = (j == 0) ? j : j - 1;
            int jmax = (j == nXSize - 1) ? j : j + 1;

            T afWin[9] = {
                INTERPOL(pafThreeLineWin[jmin], pafThreeLineWin[nXSize + jmin],
                         bSrcHasNoData, fSrcNoDataValue),
                INTERPOL(pafThreeLineWin[j], pafThreeLineWin[nXSize + j],
                         bSrcHasNoData, fSrcNoDataValue),
                INTERPOL(pafThreeLineWin[jmax], pafThreeLineWin[nXSize + jmax],
                         bSrcHasNoData, fSrcNoDataValue),
                pafThreeLineWin[jmin],
                pafThreeLineWin[j],
                pafThreeLineWin[jmax],
                pafThreeLineWin[nXSize + jmin],
                pafThreeLineWin[nXSize + j],
                pafThreeLineWin[nXSize + jmax]};
            pafOutputBuf[j] = ComputeVal(
                CPL_TO_BOOL(bSrcHasNoData), fSrcNoDataValue, bIsSrcNoDataNan,
                afWin, fDstNoDataValue, pfnAlg, pData.get(), bComputeAtEdges);
        }
        eErr = GDALRasterIO(hDstBand, GF_Write, 0, 0, nXSize, 1, pafOutputBuf,
                            nXSize, 1, GDT_Float32, 0, 0);
    }
    else
    {
        // Exclude the edges
        for (int j = 0; j < nXSize; j++)
        {
            pafOutputBuf[j] = fDstNoDataValue;
        }
        eErr = GDALRasterIO(hDstBand, GF_Write, 0, 0, nXSize, 1, pafOutputBuf,
                            nXSize, 1, GDT_Float32, 0, 0);

        if (eErr == CE_None && nYSize > 1)
        {
            eErr = GDALRasterIO(hDstBand, GF_Write, 0, nYSize - 1, nXSize, 1,
                                pafOutputBuf, nXSize, 1, GDT_Float32, 0, 0);
        }
    }
    if (eErr != CE_None)
    {
        CPLFree(pafOutputBuf);
        CPLFree(pafThreeLineWin);

        return eErr;
    }

    int i = 1;  // Used after for.
    for (; i < nYSize - 1; i++)
    {
        /* Read third line of the line buffer */
        eErr =
            GDALRasterIO(hSrcBand, GF_Read, 0, i + 1, nXSize, 1,
                         pafThreeLineWin + nLine3Off, nXSize, 1, eReadDT, 0, 0);
        if (eErr != CE_None)
        {
            CPLFree(pafOutputBuf);
            CPLFree(pafThreeLineWin);

            return eErr;
        }

        // In case none of the 3 lines have nodata values, then no need to
        // check it in ComputeVal()
        bool bOneOfThreeLinesHasNoData = CPL_TO_BOOL(bSrcHasNoData);
        if (bSrcHasNoData)
        {
            if constexpr (std::numeric_limits<T>::is_integer)
            {
                bool bLastLineHasNoDataValue = false;
                int iX = 0;
                for (; iX + 3 < nXSize; iX += 4)
                {
                    if (pafThreeLineWin[nLine3Off + iX] == fSrcNoDataValue ||
                        pafThreeLineWin[nLine3Off + iX + 1] ==
                            fSrcNoDataValue ||
                        pafThreeLineWin[nLine3Off + iX + 2] ==
                            fSrcNoDataValue ||
                        pafThreeLineWin[nLine3Off + iX + 3] == fSrcNoDataValue)
                    {
                        bLastLineHasNoDataValue = true;
                        break;
                    }
                }
                if (!bLastLineHasNoDataValue)
                {
                    for (; iX < nXSize; iX++)
                    {
                        if (pafThreeLineWin[nLine3Off + iX] == fSrcNoDataValue)
                        {
                            bLastLineHasNoDataValue = true;
                        }
                    }
                }
                abLineHasNoDataValue[nLine3Off / nXSize] =
                    bLastLineHasNoDataValue;

                bOneOfThreeLinesHasNoData = abLineHasNoDataValue[0] ||
                                            abLineHasNoDataValue[1] ||
                                            abLineHasNoDataValue[2];
            }
            else
            {
                bool bLastLineHasNoDataValue = false;
                int iX = 0;
                for (; iX + 3 < nXSize; iX += 4)
                {
                    if (pafThreeLineWin[nLine3Off + iX] == fSrcNoDataValue ||
                        std::isnan(pafThreeLineWin[nLine3Off + iX]) ||
                        pafThreeLineWin[nLine3Off + iX + 1] ==
                            fSrcNoDataValue ||
                        std::isnan(pafThreeLineWin[nLine3Off + iX + 1]) ||
                        pafThreeLineWin[nLine3Off + iX + 2] ==
                            fSrcNoDataValue ||
                        std::isnan(pafThreeLineWin[nLine3Off + iX + 2]) ||
                        pafThreeLineWin[nLine3Off + iX + 3] ==
                            fSrcNoDataValue ||
                        std::isnan(pafThreeLineWin[nLine3Off + iX + 3]))
                    {
                        bLastLineHasNoDataValue = true;
                        break;
                    }
                }
                if (!bLastLineHasNoDataValue)
                {
                    for (; iX < nXSize; iX++)
                    {
                        if (pafThreeLineWin[nLine3Off + iX] ==
                                fSrcNoDataValue ||
                            std::isnan(pafThreeLineWin[nLine3Off + iX]))
                        {
                            bLastLineHasNoDataValue = true;
                        }
                    }
                }
                abLineHasNoDataValue[nLine3Off / nXSize] =
                    bLastLineHasNoDataValue;

                bOneOfThreeLinesHasNoData = abLineHasNoDataValue[0] ||
                                            abLineHasNoDataValue[1] ||
                                            abLineHasNoDataValue[2];
            }
        }

        if (bComputeAtEdges && nXSize >= 2)
        {
            int j = 0;
            T afWin[9] = {INTERPOL(pafThreeLineWin[nLine1Off + j],
                                   pafThreeLineWin[nLine1Off + j + 1],
                                   bSrcHasNoData, fSrcNoDataValue),
                          pafThreeLineWin[nLine1Off + j],
                          pafThreeLineWin[nLine1Off + j + 1],
                          INTERPOL(pafThreeLineWin[nLine2Off + j],
                                   pafThreeLineWin[nLine2Off + j + 1],
                                   bSrcHasNoData, fSrcNoDataValue),
                          pafThreeLineWin[nLine2Off + j],
                          pafThreeLineWin[nLine2Off + j + 1],
                          INTERPOL(pafThreeLineWin[nLine3Off + j],
                                   pafThreeLineWin[nLine3Off + j + 1],
                                   bSrcHasNoData, fSrcNoDataValue),
                          pafThreeLineWin[nLine3Off + j],
                          pafThreeLineWin[nLine3Off + j + 1]};

            pafOutputBuf[j] = ComputeVal(
                bOneOfThreeLinesHasNoData, fSrcNoDataValue, bIsSrcNoDataNan,
                afWin, fDstNoDataValue, pfnAlg, pData.get(), bComputeAtEdges);
        }
        else
        {
            // Exclude the edges
            pafOutputBuf[0] = fDstNoDataValue;
        }

        int j = 1;
        if (pfnAlg_multisample && !bOneOfThreeLinesHasNoData)
        {
            j = pfnAlg_multisample(
                pafThreeLineWin + nLine1Off, pafThreeLineWin + nLine2Off,
                pafThreeLineWin + nLine3Off, nXSize, pData.get(), pafOutputBuf);
        }

        for (; j < nXSize - 1; j++)
        {
            T afWin[9] = {pafThreeLineWin[nLine1Off + j - 1],
                          pafThreeLineWin[nLine1Off + j],
                          pafThreeLineWin[nLine1Off + j + 1],
                          pafThreeLineWin[nLine2Off + j - 1],
                          pafThreeLineWin[nLine2Off + j],
                          pafThreeLineWin[nLine2Off + j + 1],
                          pafThreeLineWin[nLine3Off + j - 1],
                          pafThreeLineWin[nLine3Off + j],
                          pafThreeLineWin[nLine3Off + j + 1]};

            pafOutputBuf[j] = ComputeVal(
                bOneOfThreeLinesHasNoData, fSrcNoDataValue, bIsSrcNoDataNan,
                afWin, fDstNoDataValue, pfnAlg, pData.get(), bComputeAtEdges);
        }

        if (bComputeAtEdges && nXSize >= 2)
        {
            j = nXSize - 1;

            T afWin[9] = {pafThreeLineWin[nLine1Off + j - 1],
                          pafThreeLineWin[nLine1Off + j],
                          INTERPOL(pafThreeLineWin[nLine1Off + j],
                                   pafThreeLineWin[nLine1Off + j - 1],
                                   bSrcHasNoData, fSrcNoDataValue),
                          pafThreeLineWin[nLine2Off + j - 1],
                          pafThreeLineWin[nLine2Off + j],
                          INTERPOL(pafThreeLineWin[nLine2Off + j],
                                   pafThreeLineWin[nLine2Off + j - 1],
                                   bSrcHasNoData, fSrcNoDataValue),
                          pafThreeLineWin[nLine3Off + j - 1],
                          pafThreeLineWin[nLine3Off + j],
                          INTERPOL(pafThreeLineWin[nLine3Off + j],
                                   pafThreeLineWin[nLine3Off + j - 1],
                                   bSrcHasNoData, fSrcNoDataValue)};

            pafOutputBuf[j] = ComputeVal(
                bOneOfThreeLinesHasNoData, fSrcNoDataValue, bIsSrcNoDataNan,
                afWin, fDstNoDataValue, pfnAlg, pData.get(), bComputeAtEdges);
        }
        else
        {
            // Exclude the edges
            if (nXSize > 1)
                pafOutputBuf[nXSize - 1] = fDstNoDataValue;
        }

        /* -----------------------------------------
         * Write Line to Raster
         */
        eErr = GDALRasterIO(hDstBand, GF_Write, 0, i, nXSize, 1, pafOutputBuf,
                            nXSize, 1, GDT_Float32, 0, 0);
        if (eErr != CE_None)
        {
            CPLFree(pafOutputBuf);
            CPLFree(pafThreeLineWin);

            return eErr;
        }

        if (!pfnProgress(1.0 * (i + 1) / nYSize, nullptr, pProgressData))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
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

    if (bComputeAtEdges && nXSize >= 2 && nYSize >= 2)
    {
        for (int j = 0; j < nXSize; j++)
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
                         pafThreeLineWin[nLine1Off + jmin], bSrcHasNoData,
                         fSrcNoDataValue),
                INTERPOL(pafThreeLineWin[nLine2Off + j],
                         pafThreeLineWin[nLine1Off + j], bSrcHasNoData,
                         fSrcNoDataValue),
                INTERPOL(pafThreeLineWin[nLine2Off + jmax],
                         pafThreeLineWin[nLine1Off + jmax], bSrcHasNoData,
                         fSrcNoDataValue),
            };

            pafOutputBuf[j] = ComputeVal(
                CPL_TO_BOOL(bSrcHasNoData), fSrcNoDataValue, bIsSrcNoDataNan,
                afWin, fDstNoDataValue, pfnAlg, pData.get(), bComputeAtEdges);
        }
        eErr = GDALRasterIO(hDstBand, GF_Write, 0, i, nXSize, 1, pafOutputBuf,
                            nXSize, 1, GDT_Float32, 0, 0);
        if (eErr != CE_None)
        {
            CPLFree(pafOutputBuf);
            CPLFree(pafThreeLineWin);

            return eErr;
        }
    }

    pfnProgress(1.0, nullptr, pProgressData);
    eErr = CE_None;

    CPLFree(pafOutputBuf);
    CPLFree(pafThreeLineWin);

    return eErr;
}

/************************************************************************/
/*                             GradientAlg                              */
/************************************************************************/

template <class T, GradientAlg alg> struct Gradient
{
    static void inline calc(const T *afWin, float inv_ewres, float inv_nsres,
                            float &x, float &y);
};

template <class T> struct Gradient<T, GradientAlg::HORN>
{
    static void calc(const T *afWin, float inv_ewres, float inv_nsres, float &x,
                     float &y)
    {
        x = float((afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
                  (afWin[2] + afWin[5] + afWin[5] + afWin[8])) *
            inv_ewres;

        y = float((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
                  (afWin[0] + afWin[1] + afWin[1] + afWin[2])) *
            inv_nsres;
    }
};

template <class T> struct Gradient<T, GradientAlg::ZEVENBERGEN_THORNE>
{
    static void calc(const T *afWin, float inv_ewres, float inv_nsres, float &x,
                     float &y)
    {
        x = float(afWin[3] - afWin[5]) * inv_ewres;
        y = float(afWin[7] - afWin[1]) * inv_nsres;
    }
};

/************************************************************************/
/*                           GDALHillshade()                            */
/************************************************************************/

struct GDALHillshadeAlgData final : public AlgorithmParameters
{
    float inv_nsres_yscale = 0;
    float inv_ewres_xscale = 0;
    float sin_altRadians = 0;
    float cos_alt_mul_z = 0;
    float azRadians = 0;
    float cos_az_mul_cos_alt_mul_z = 0;
    float sin_az_mul_cos_alt_mul_z = 0;
    float square_z = 0;
    float sin_altRadians_mul_254 = 0;
    float cos_az_mul_cos_alt_mul_z_mul_254 = 0;
    float sin_az_mul_cos_alt_mul_z_mul_254 = 0;

    float square_z_mul_square_inv_res = 0;
    float cos_az_mul_cos_alt_mul_z_mul_254_mul_inv_res = 0;
    float sin_az_mul_cos_alt_mul_z_mul_254_mul_inv_res = 0;
    float z_factor = 0;

    std::unique_ptr<AlgorithmParameters>
    CreateScaledParameters(double dfXRatio, double dfYRatio) override;
};

std::unique_ptr<AlgorithmParameters>
GDALHillshadeAlgData::CreateScaledParameters(double dfXRatio, double dfYRatio)
{
    auto newData = std::make_unique<GDALHillshadeAlgData>(*this);
    const float fXRatio = static_cast<float>(dfXRatio);
    const float fYRatio = static_cast<float>(dfYRatio);
    newData->inv_ewres_xscale /= fXRatio;
    newData->inv_nsres_yscale /= fYRatio;

    newData->square_z_mul_square_inv_res /= fXRatio * fXRatio;
    newData->cos_az_mul_cos_alt_mul_z_mul_254_mul_inv_res /= fXRatio;
    newData->sin_az_mul_cos_alt_mul_z_mul_254_mul_inv_res /= fXRatio;

    return newData;
}

/* Unoptimized formulas are :
    x = psData->z*((afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
        (afWin[2] + afWin[5] + afWin[5] + afWin[8])) /
        (8.0 * psData->ewres * psData->xscale);

    y = psData->z*((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
        (afWin[0] + afWin[1] + afWin[1] + afWin[2])) /
        (8.0 * psData->nsres * psData->yscale);

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

==> cang = (sin(alt) - cos(alt) * sqrt(x*x + y*y)  * sin(aspect-az)) /
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
inline float ApproxADivByInvSqrtB(float a, float b)
{
    __m128 regB = _mm_load_ss(&b);
    __m128 regB_half = _mm_mul_ss(regB, _mm_set1_ps(0.5f));
    // Compute rough approximation of 1 / sqrt(b) with _mm_rsqrt_ss
    regB = _mm_rsqrt_ss(regB);
    // And perform one step of Newton-Raphson approximation to improve it
    // approx_inv_sqrt_x = approx_inv_sqrt_x*(1.5 -
    //                            0.5*x*approx_inv_sqrt_x*approx_inv_sqrt_x);
    regB = _mm_mul_ss(
        regB, _mm_sub_ss(_mm_set1_ps(1.5f),
                         _mm_mul_ss(regB_half, _mm_mul_ss(regB, regB))));
    float fOut;
    _mm_store_ss(&fOut, regB);
    return a * fOut;
}
#else
inline float ApproxADivByInvSqrtB(float a, float b)
{
    return a / std::sqrt(b);
}
#endif

static float NormalizeAngle(float angle, float normalizer)
{
    angle = std::fmod(angle, normalizer);
    if (angle < 0)
        angle = normalizer + angle;

    return angle;
}

static float DifferenceBetweenAngles(float angle1, float angle2,
                                     float normalizer)
{
    float diff =
        NormalizeAngle(angle1, normalizer) - NormalizeAngle(angle2, normalizer);
    diff = std::abs(diff);
    if (diff > normalizer * 0.5f)
        diff = normalizer - diff;
    return diff;
}

template <class T, GradientAlg alg>
static float GDALHillshadeIgorAlg(const T *afWin, float /*fDstNoDataValue*/,
                                  const AlgorithmParameters *pData)
{
    const GDALHillshadeAlgData *psData =
        static_cast<const GDALHillshadeAlgData *>(pData);

    float slopeDegrees;
    if (alg == GradientAlg::HORN)
    {
        const auto dx =
            static_cast<float>((afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
                               (afWin[2] + afWin[5] + afWin[5] + afWin[8])) *
            psData->inv_ewres_xscale;

        const auto dy =
            static_cast<float>((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
                               (afWin[0] + afWin[1] + afWin[1] + afWin[2])) *
            psData->inv_nsres_yscale;

        const auto key = (dx * dx + dy * dy);
        slopeDegrees =
            std::atan(std::sqrt(key) * psData->z_factor) * kfRadToDeg;
    }
    else  // ZEVENBERGEN_THORNE
    {
        const auto dx =
            static_cast<float>(afWin[3] - afWin[5]) * psData->inv_ewres_xscale;
        const auto dy =
            static_cast<float>(afWin[7] - afWin[1]) * psData->inv_nsres_yscale;
        const auto key = dx * dx + dy * dy;

        slopeDegrees =
            std::atan(std::sqrt(key) * psData->z_factor) * kfRadToDeg;
    }

    float aspect;
    if (alg == GradientAlg::HORN)
    {
        const auto dx =
            static_cast<float>((afWin[2] + afWin[5] + afWin[5] + afWin[8]) -
                               (afWin[0] + afWin[3] + afWin[3] + afWin[6]));

        const auto dy2 =
            static_cast<float>((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
                               (afWin[0] + afWin[1] + afWin[1] + afWin[2]));

        aspect = std::atan2(dy2, -dx);
    }
    else  // ZEVENBERGEN_THORNE
    {
        const auto dx = static_cast<float>(afWin[5] - afWin[3]);
        const auto dy = static_cast<float>(afWin[7] - afWin[1]);
        aspect = std::atan2(dy, -dx);
    }

    const auto slopeStrength = slopeDegrees * (1.0f / 90.0f);

    constexpr float PIf = static_cast<float>(M_PI);
    const auto aspectDiff = DifferenceBetweenAngles(
        aspect, PIf * (3.0f / 2.0f) - psData->azRadians, PIf * 2.0f);

    const auto aspectStrength = 1.0f - aspectDiff * (1.0f / PIf);

    const auto shadowness = 1.0f - slopeStrength * aspectStrength;

    return 255.0f * shadowness;
}

template <class T, GradientAlg alg>
static float GDALHillshadeAlg(const T *afWin, float /*fDstNoDataValue*/,
                              const AlgorithmParameters *pData)
{
    const GDALHillshadeAlgData *psData =
        static_cast<const GDALHillshadeAlgData *>(pData);

    // First Slope ...
    float x, y;
    Gradient<T, alg>::calc(afWin, psData->inv_ewres_xscale,
                           psData->inv_nsres_yscale, x, y);

    const auto xx_plus_yy = x * x + y * y;

    // ... then the shade value
    const auto cang_mul_254 =
        ApproxADivByInvSqrtB(psData->sin_altRadians_mul_254 -
                                 (y * psData->cos_az_mul_cos_alt_mul_z_mul_254 -
                                  x * psData->sin_az_mul_cos_alt_mul_z_mul_254),
                             1.0f + psData->square_z * xx_plus_yy);

    const auto cang = cang_mul_254 <= 0.0f ? 1.0f : 1.0f + cang_mul_254;

    return cang;
}

template <class T>
static float GDALHillshadeAlg_same_res(const T *afWin,
                                       float /*fDstNoDataValue*/,
                                       const AlgorithmParameters *pData)
{
    const GDALHillshadeAlgData *psData =
        static_cast<const GDALHillshadeAlgData *>(pData);

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
    const auto x = static_cast<float>(accX);
    const auto y = static_cast<float>(accY);

    const auto xx_plus_yy = x * x + y * y;

    // ... then the shade value
    const auto cang_mul_254 = ApproxADivByInvSqrtB(
        psData->sin_altRadians_mul_254 +
            (x * psData->sin_az_mul_cos_alt_mul_z_mul_254_mul_inv_res +
             y * psData->cos_az_mul_cos_alt_mul_z_mul_254_mul_inv_res),
        1.0f + psData->square_z_mul_square_inv_res * xx_plus_yy);

    const auto cang = cang_mul_254 <= 0.0f ? 1.0f : 1.0f + cang_mul_254;

    return cang;
}

#if defined(HAVE_16_SSE_REG)
template <class T, class REG_T, class REG_FLOAT>
static int GDALHillshadeAlg_same_res_multisample(
    const T *pafFirstLine, const T *pafSecondLine, const T *pafThirdLine,
    int nXSize, const AlgorithmParameters *pData, float *pafOutputBuf)
{
    const GDALHillshadeAlgData *psData =
        static_cast<const GDALHillshadeAlgData *>(pData);
    const auto reg_fact_x =
        REG_FLOAT::Set1(psData->sin_az_mul_cos_alt_mul_z_mul_254_mul_inv_res);
    const auto reg_fact_y =
        REG_FLOAT::Set1(psData->cos_az_mul_cos_alt_mul_z_mul_254_mul_inv_res);
    const auto reg_constant_num =
        REG_FLOAT::Set1(psData->sin_altRadians_mul_254);
    const auto reg_constant_denom =
        REG_FLOAT::Set1(psData->square_z_mul_square_inv_res);
    const auto reg_half = REG_FLOAT::Set1(0.5f);
    const auto reg_one = reg_half + reg_half;
    const auto reg_one_float = REG_FLOAT::Set1(1.0f);

    int j = 1;  // Used after for.
    constexpr int N_VAL_PER_REG =
        static_cast<int>(sizeof(REG_FLOAT) / sizeof(float));
    for (; j < nXSize - N_VAL_PER_REG; j += N_VAL_PER_REG)
    {
        const T *firstLine = pafFirstLine + j - 1;
        const T *secondLine = pafSecondLine + j - 1;
        const T *thirdLine = pafThirdLine + j - 1;

        const auto firstLine0 = REG_T::LoadAllVal(firstLine);
        const auto firstLine1 = REG_T::LoadAllVal(firstLine + 1);
        const auto firstLine2 = REG_T::LoadAllVal(firstLine + 2);
        const auto thirdLine0 = REG_T::LoadAllVal(thirdLine);
        const auto thirdLine1 = REG_T::LoadAllVal(thirdLine + 1);
        const auto thirdLine2 = REG_T::LoadAllVal(thirdLine + 2);
        auto accX = firstLine0 - thirdLine2;
        const auto six_minus_two = thirdLine0 - firstLine2;
        auto accY = accX;
        const auto three_minus_five =
            REG_T::LoadAllVal(secondLine) - REG_T::LoadAllVal(secondLine + 2);
        const auto one_minus_seven = firstLine1 - thirdLine1;
        accX += three_minus_five;
        accY += one_minus_seven;
        accX += three_minus_five;
        accY += one_minus_seven;
        accX += six_minus_two;
        accY -= six_minus_two;

        const auto reg_x = accX.cast_to_float();
        const auto reg_y = accY.cast_to_float();
        const auto reg_xx_plus_yy = reg_x * reg_x + reg_y * reg_y;
        const auto reg_numerator =
            reg_constant_num + reg_fact_x * reg_x + reg_fact_y * reg_y;
        const auto reg_denominator =
            reg_one + reg_constant_denom * reg_xx_plus_yy;
        const auto num_div_sqrt_denom =
            reg_numerator * reg_denominator.approx_inv_sqrt(reg_one, reg_half);

        auto res =
            REG_FLOAT::Max(reg_one_float, num_div_sqrt_denom + reg_one_float);
        res.StoreAllVal(pafOutputBuf + j);
    }
    return j;
}
#endif

template <class T, GradientAlg alg>
static float GDALHillshadeCombinedAlg(const T *afWin, float /*fDstNoDataValue*/,
                                      const AlgorithmParameters *pData)
{
    const GDALHillshadeAlgData *psData =
        static_cast<const GDALHillshadeAlgData *>(pData);

    // First Slope ...
    float x, y;
    Gradient<T, alg>::calc(afWin, psData->inv_ewres_xscale,
                           psData->inv_nsres_yscale, x, y);

    const auto xx_plus_yy = x * x + y * y;

    const auto slope = xx_plus_yy * psData->square_z;

    // ... then the shade value
    auto cang = std::acos(ApproxADivByInvSqrtB(
        psData->sin_altRadians - (y * psData->cos_az_mul_cos_alt_mul_z -
                                  x * psData->sin_az_mul_cos_alt_mul_z),
        1.0f + slope));

    // combined shading
    constexpr float INV_SQUARE_OF_HALF_PI =
        static_cast<float>(1.0 / ((M_PI * M_PI) / 4));

    cang = 1.0f - cang * std::atan(std::sqrt(slope)) * INV_SQUARE_OF_HALF_PI;

    const float fcang = cang <= 0.0f ? 1.0f : 1.0f + 254.0f * cang;

    return fcang;
}

static std::unique_ptr<AlgorithmParameters>
GDALCreateHillshadeData(const double *adfGeoTransform, double z, double xscale,
                        double yscale, double alt, double az, GradientAlg eAlg)
{
    auto pData = std::make_unique<GDALHillshadeAlgData>();

    pData->inv_nsres_yscale =
        static_cast<float>(1.0 / (adfGeoTransform[5] * yscale));
    pData->inv_ewres_xscale =
        static_cast<float>(1.0 / (adfGeoTransform[1] * xscale));
    pData->sin_altRadians = std::sin(static_cast<float>(alt) * kfDegToRad);
    pData->azRadians = static_cast<float>(az) * kfDegToRad;
    pData->z_factor = static_cast<float>(
        z / (eAlg == GradientAlg::ZEVENBERGEN_THORNE ? 2 : 8));
    pData->cos_alt_mul_z =
        std::cos(static_cast<float>(alt) * kfDegToRad) * pData->z_factor;
    pData->cos_az_mul_cos_alt_mul_z =
        std::cos(pData->azRadians) * pData->cos_alt_mul_z;
    pData->sin_az_mul_cos_alt_mul_z =
        std::sin(pData->azRadians) * pData->cos_alt_mul_z;
    pData->square_z = pData->z_factor * pData->z_factor;

    pData->sin_altRadians_mul_254 = 254.0f * pData->sin_altRadians;
    pData->cos_az_mul_cos_alt_mul_z_mul_254 =
        254.0f * pData->cos_az_mul_cos_alt_mul_z;
    pData->sin_az_mul_cos_alt_mul_z_mul_254 =
        254.0f * pData->sin_az_mul_cos_alt_mul_z;

    if (adfGeoTransform[1] == -adfGeoTransform[5] && xscale == yscale)
    {
        pData->square_z_mul_square_inv_res =
            pData->square_z * pData->inv_ewres_xscale * pData->inv_ewres_xscale;
        pData->cos_az_mul_cos_alt_mul_z_mul_254_mul_inv_res =
            pData->cos_az_mul_cos_alt_mul_z_mul_254 * -pData->inv_ewres_xscale;
        pData->sin_az_mul_cos_alt_mul_z_mul_254_mul_inv_res =
            pData->sin_az_mul_cos_alt_mul_z_mul_254 * pData->inv_ewres_xscale;
    }

    return pData;
}

/************************************************************************/
/*                   GDALHillshadeMultiDirectional()                    */
/************************************************************************/

struct GDALHillshadeMultiDirectionalAlgData final : public AlgorithmParameters
{
    float inv_nsres_yscale = 0;
    float inv_ewres_xscale = 0;
    float square_z = 0;
    float sin_altRadians_mul_127 = 0;
    float sin_altRadians_mul_254 = 0;

    float cos_alt_mul_z_mul_127 = 0;
    float cos225_az_mul_cos_alt_mul_z_mul_127 = 0;

    std::unique_ptr<AlgorithmParameters>
    CreateScaledParameters(double dfXRatio, double dfYRatio) override;
};

std::unique_ptr<AlgorithmParameters>
GDALHillshadeMultiDirectionalAlgData::CreateScaledParameters(double dfXRatio,
                                                             double dfYRatio)
{
    auto newData =
        std::make_unique<GDALHillshadeMultiDirectionalAlgData>(*this);
    newData->inv_ewres_xscale /= static_cast<float>(dfXRatio);
    newData->inv_nsres_yscale /= static_cast<float>(dfYRatio);
    return newData;
}

template <class T, GradientAlg alg>
static float GDALHillshadeMultiDirectionalAlg(const T *afWin,
                                              float /*fDstNoDataValue*/,
                                              const AlgorithmParameters *pData)
{
    const GDALHillshadeMultiDirectionalAlgData *psData =
        static_cast<const GDALHillshadeMultiDirectionalAlgData *>(pData);

    // First Slope ...
    float x, y;
    Gradient<T, alg>::calc(afWin, psData->inv_ewres_xscale,
                           psData->inv_nsres_yscale, x, y);

    // See http://pubs.usgs.gov/of/1992/of92-422/of92-422.pdf
    // W225 = sin^2(aspect - 225) = 0.5 * (1 - 2 * sin(aspect) * cos(aspect))
    // W270 = sin^2(aspect - 270) = cos^2(aspect)
    // W315 = sin^2(aspect - 315) = 0.5 * (1 + 2 * sin(aspect) * cos(aspect))
    // W360 = sin^2(aspect - 360) = sin^2(aspect)
    // hillshade=  0.5 * (W225 * hillshade(az=225) +
    //                    W270 * hillshade(az=270) +
    //                    W315 * hillshade(az=315) +
    //                    W360 * hillshade(az=360))

    const auto xx = x * x;
    const auto yy = y * y;
    const auto xx_plus_yy = xx + yy;
    if (xx_plus_yy == 0.0f)
        return 1.0f + psData->sin_altRadians_mul_254;

    // ... then the shade value from different azimuth
    auto val225_mul_127 = psData->sin_altRadians_mul_127 +
                          (x - y) * psData->cos225_az_mul_cos_alt_mul_z_mul_127;
    val225_mul_127 = (val225_mul_127 <= 0.0f) ? 0.0f : val225_mul_127;
    auto val270_mul_127 =
        psData->sin_altRadians_mul_127 - x * psData->cos_alt_mul_z_mul_127;
    val270_mul_127 = (val270_mul_127 <= 0.0f) ? 0.0f : val270_mul_127;
    auto val315_mul_127 = psData->sin_altRadians_mul_127 +
                          (x + y) * psData->cos225_az_mul_cos_alt_mul_z_mul_127;
    val315_mul_127 = (val315_mul_127 <= 0.0f) ? 0.0f : val315_mul_127;
    auto val360_mul_127 =
        psData->sin_altRadians_mul_127 - y * psData->cos_alt_mul_z_mul_127;
    val360_mul_127 = (val360_mul_127 <= 0.0f) ? 0.0f : val360_mul_127;

    // ... then the weighted shading
    const auto weight_225 = 0.5f * xx_plus_yy - x * y;
    const auto weight_270 = xx;
    const auto weight_315 = xx_plus_yy - weight_225;
    const auto weight_360 = yy;
    const auto cang_mul_127 = ApproxADivByInvSqrtB(
        (weight_225 * val225_mul_127 + weight_270 * val270_mul_127 +
         weight_315 * val315_mul_127 + weight_360 * val360_mul_127) /
            xx_plus_yy,
        1.0f + psData->square_z * xx_plus_yy);

    const auto cang = 1.0f + cang_mul_127;

    return cang;
}

static std::unique_ptr<AlgorithmParameters>
GDALCreateHillshadeMultiDirectionalData(const double *adfGeoTransform, double z,
                                        double xscale, double yscale,
                                        double alt, GradientAlg eAlg)
{
    auto pData = std::make_unique<GDALHillshadeMultiDirectionalAlgData>();

    pData->inv_nsres_yscale =
        static_cast<float>(1.0 / (adfGeoTransform[5] * yscale));
    pData->inv_ewres_xscale =
        static_cast<float>(1.0 / (adfGeoTransform[1] * xscale));
    const float z_factor = static_cast<float>(
        z / (eAlg == GradientAlg::ZEVENBERGEN_THORNE ? 2 : 8));
    const float cos_alt_mul_z =
        std::cos(static_cast<float>(alt) * kfDegToRad) * z_factor;
    pData->square_z = z_factor * z_factor;

    pData->sin_altRadians_mul_127 =
        127.0f * std::sin(static_cast<float>(alt) * kfDegToRad);
    pData->sin_altRadians_mul_254 =
        254.0f * std::sin(static_cast<float>(alt) * kfDegToRad);
    pData->cos_alt_mul_z_mul_127 = 127.0f * cos_alt_mul_z;
    pData->cos225_az_mul_cos_alt_mul_z_mul_127 =
        127.0f * std::cos(225.0f * kfDegToRad) * cos_alt_mul_z;

    return pData;
}

/************************************************************************/
/*                             GDALSlope()                              */
/************************************************************************/

struct GDALSlopeAlgData final : public AlgorithmParameters
{
    float inv_nsres_yscale = 0;
    float inv_ewres_xscale = 0;
    int slopeFormat = 0;

    std::unique_ptr<AlgorithmParameters>
    CreateScaledParameters(double dfXRatio, double dfYRatio) override;
};

std::unique_ptr<AlgorithmParameters>
GDALSlopeAlgData::CreateScaledParameters(double dfXRatio, double dfYRatio)
{
    auto newData = std::make_unique<GDALSlopeAlgData>(*this);
    newData->inv_nsres_yscale /= static_cast<float>(dfXRatio);
    newData->inv_ewres_xscale /= static_cast<float>(dfYRatio);
    return newData;
}

template <class T>
static float GDALSlopeHornAlg(const T *afWin, float /*fDstNoDataValue*/,
                              const AlgorithmParameters *pData)
{
    const GDALSlopeAlgData *psData =
        static_cast<const GDALSlopeAlgData *>(pData);

    const auto dx =
        static_cast<float>((afWin[0] + afWin[3] + afWin[3] + afWin[6]) -
                           (afWin[2] + afWin[5] + afWin[5] + afWin[8])) *
        psData->inv_ewres_xscale;

    const auto dy =
        static_cast<float>((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
                           (afWin[0] + afWin[1] + afWin[1] + afWin[2])) *
        psData->inv_nsres_yscale;

    const auto key = dx * dx + dy * dy;

    if (psData->slopeFormat == 1)
        return std::atan(std::sqrt(key) * (1.0f / 8.0f)) * kfRadToDeg;

    return (100.0f / 8.0f) * std::sqrt(key);
}

template <class T>
static float GDALSlopeZevenbergenThorneAlg(const T *afWin,
                                           float /*fDstNoDataValue*/,
                                           const AlgorithmParameters *pData)
{
    const GDALSlopeAlgData *psData =
        static_cast<const GDALSlopeAlgData *>(pData);

    const auto dx =
        static_cast<float>(afWin[3] - afWin[5]) * psData->inv_ewres_xscale;
    const auto dy =
        static_cast<float>(afWin[7] - afWin[1]) * psData->inv_nsres_yscale;
    const auto key = dx * dx + dy * dy;

    if (psData->slopeFormat == 1)
        return std::atan(std::sqrt(key) * 0.5f) * kfRadToDeg;

    return (100.0f / 2.0f) * std::sqrt(key);
}

static std::unique_ptr<AlgorithmParameters>
GDALCreateSlopeData(double *adfGeoTransform, double xscale, double yscale,
                    int slopeFormat)
{
    auto pData = std::make_unique<GDALSlopeAlgData>();
    pData->inv_nsres_yscale =
        1.0f / static_cast<float>(adfGeoTransform[5] * yscale);
    pData->inv_ewres_xscale =
        1.0f / static_cast<float>(adfGeoTransform[1] * xscale);
    pData->slopeFormat = slopeFormat;
    return pData;
}

/************************************************************************/
/*                             GDALAspect()                             */
/************************************************************************/

struct GDALAspectAlgData final : public AlgorithmParameters
{
    bool bAngleAsAzimuth = false;

    std::unique_ptr<AlgorithmParameters>
    CreateScaledParameters(double, double) override;
};

std::unique_ptr<AlgorithmParameters>
GDALAspectAlgData::CreateScaledParameters(double, double)
{
    return std::make_unique<GDALAspectAlgData>(*this);
}

template <class T>
static float GDALAspectAlg(const T *afWin, float fDstNoDataValue,
                           const AlgorithmParameters *pData)
{
    const GDALAspectAlgData *psData =
        static_cast<const GDALAspectAlgData *>(pData);

    const auto dx =
        static_cast<float>((afWin[2] + afWin[5] + afWin[5] + afWin[8]) -
                           (afWin[0] + afWin[3] + afWin[3] + afWin[6]));

    const auto dy =
        static_cast<float>((afWin[6] + afWin[7] + afWin[7] + afWin[8]) -
                           (afWin[0] + afWin[1] + afWin[1] + afWin[2]));

    auto aspect = std::atan2(dy, -dx) * kfRadToDeg;

    if (dx == 0 && dy == 0)
    {
        /* Flat area */
        aspect = fDstNoDataValue;
    }
    else if (psData->bAngleAsAzimuth)
    {
        if (aspect > 90.0f)
            aspect = 450.0f - aspect;
        else
            aspect = 90.0f - aspect;
    }
    else
    {
        if (aspect < 0)
            aspect += 360.0f;
    }

    if (aspect == 360.0f)
        aspect = 0.0;

    return aspect;
}

template <class T>
static float GDALAspectZevenbergenThorneAlg(const T *afWin,
                                            float fDstNoDataValue,
                                            const AlgorithmParameters *pData)
{
    const GDALAspectAlgData *psData =
        static_cast<const GDALAspectAlgData *>(pData);

    const auto dx = static_cast<float>(afWin[5] - afWin[3]);
    const auto dy = static_cast<float>(afWin[7] - afWin[1]);
    float aspect = std::atan2(dy, -dx) * kfRadToDeg;
    if (dx == 0 && dy == 0)
    {
        /* Flat area */
        aspect = fDstNoDataValue;
    }
    else if (psData->bAngleAsAzimuth)
    {
        if (aspect > 90.0f)
            aspect = 450.0f - aspect;
        else
            aspect = 90.0f - aspect;
    }
    else
    {
        if (aspect < 0)
            aspect += 360.0f;
    }

    if (aspect == 360.0f)
        aspect = 0.0;

    return aspect;
}

static std::unique_ptr<AlgorithmParameters>
GDALCreateAspectData(bool bAngleAsAzimuth)
{
    auto pData = std::make_unique<GDALAspectAlgData>();
    pData->bAngleAsAzimuth = bAngleAsAzimuth;
    return pData;
}

/************************************************************************/
/*                          GDALColorRelief()                           */
/************************************************************************/

static int GDALColorReliefSortColors(const GDALColorAssociation &pA,
                                     const GDALColorAssociation &pB)
{
    /* Sort NaN in first position */
    return (std::isnan(pA.dfVal) && !std::isnan(pB.dfVal)) ||
           pA.dfVal < pB.dfVal;
}

static void GDALColorReliefProcessColors(
    std::vector<GDALColorAssociation> &asColorAssociation, int bSrcHasNoData,
    double dfSrcNoDataValue, ColorSelectionMode eColorSelectionMode)
{
    std::stable_sort(asColorAssociation.begin(), asColorAssociation.end(),
                     GDALColorReliefSortColors);

    size_t nRepeatedEntryIndex = 0;
    const size_t nInitialSize = asColorAssociation.size();
    for (size_t i = 1; i < nInitialSize; ++i)
    {
        const GDALColorAssociation *pPrevious = &asColorAssociation[i - 1];
        const GDALColorAssociation *pCurrent = &asColorAssociation[i];

        // NaN comparison is always false, so it handles itself
        if (eColorSelectionMode != COLOR_SELECTION_EXACT_ENTRY &&
            bSrcHasNoData && pCurrent->dfVal == dfSrcNoDataValue)
        {
            // Check if there is enough distance between the nodata value and
            // its predecessor.
            const double dfNewValue = std::nextafter(
                pCurrent->dfVal, -std::numeric_limits<double>::infinity());
            if (dfNewValue > pPrevious->dfVal)
            {
                // add one just below the nodata value
                GDALColorAssociation sNew = *pPrevious;
                sNew.dfVal = dfNewValue;
                asColorAssociation.push_back(std::move(sNew));
            }
        }
        else if (eColorSelectionMode != COLOR_SELECTION_EXACT_ENTRY &&
                 bSrcHasNoData && pPrevious->dfVal == dfSrcNoDataValue)
        {
            // Check if there is enough distance between the nodata value and
            // its successor.
            const double dfNewValue = std::nextafter(
                pPrevious->dfVal, std::numeric_limits<double>::infinity());
            if (dfNewValue < pCurrent->dfVal)
            {
                // add one just above the nodata value
                GDALColorAssociation sNew = *pCurrent;
                sNew.dfVal = dfNewValue;
                asColorAssociation.push_back(std::move(sNew));
            }
        }
        else if (nRepeatedEntryIndex == 0 &&
                 pCurrent->dfVal == pPrevious->dfVal)
        {
            // second of a series of equivalent entries
            nRepeatedEntryIndex = i;
        }
        else if (nRepeatedEntryIndex != 0 &&
                 pCurrent->dfVal != pPrevious->dfVal)
        {
            // Get the distance between the predecessor and successor of the
            // equivalent entries.
            double dfTotalDist = 0.0;
            double dfLeftDist = 0.0;
            if (nRepeatedEntryIndex >= 2)
            {
                const GDALColorAssociation *pLower =
                    &asColorAssociation[nRepeatedEntryIndex - 2];
                dfTotalDist = pCurrent->dfVal - pLower->dfVal;
                dfLeftDist = pPrevious->dfVal - pLower->dfVal;
            }
            else
            {
                dfTotalDist = pCurrent->dfVal - pPrevious->dfVal;
            }

            // check if this distance is enough
            const size_t nEquivalentCount = i - nRepeatedEntryIndex + 1;
            if (dfTotalDist >
                std::abs(pPrevious->dfVal) * nEquivalentCount * DBL_EPSILON)
            {
                // balance the alterations
                double dfMultiplier =
                    0.5 - double(nEquivalentCount) * dfLeftDist / dfTotalDist;
                for (auto j = nRepeatedEntryIndex - 1; j < i; ++j)
                {
                    asColorAssociation[j].dfVal +=
                        (std::abs(pPrevious->dfVal) * dfMultiplier) *
                        DBL_EPSILON;
                    dfMultiplier += 1.0;
                }
            }
            else
            {
                // Fallback to the old behavior: keep equivalent entries as
                // they are.
            }

            nRepeatedEntryIndex = 0;
        }
    }

    if (nInitialSize != asColorAssociation.size())
    {
        std::stable_sort(asColorAssociation.begin(), asColorAssociation.end(),
                         GDALColorReliefSortColors);
    }
}

static bool GDALColorReliefGetRGBA(
    const std::vector<GDALColorAssociation> &asColorAssociation, double dfVal,
    ColorSelectionMode eColorSelectionMode, int *pnR, int *pnG, int *pnB,
    int *pnA)
{
    CPLAssert(!asColorAssociation.empty());

    size_t lower = 0;

    // Special case for NaN
    if (std::isnan(asColorAssociation[0].dfVal))
    {
        if (std::isnan(dfVal))
        {
            *pnR = asColorAssociation[0].nR;
            *pnG = asColorAssociation[0].nG;
            *pnB = asColorAssociation[0].nB;
            *pnA = asColorAssociation[0].nA;
            return true;
        }
        else
        {
            lower = 1;
        }
    }

    // Find the index of the first element in the LUT input array that
    // is not smaller than the dfVal value.
    size_t i = 0;
    size_t upper = asColorAssociation.size() - 1;
    while (true)
    {
        const size_t mid = (lower + upper) / 2;
        if (upper - lower <= 1)
        {
            if (dfVal <= asColorAssociation[lower].dfVal)
                i = lower;
            else if (dfVal <= asColorAssociation[upper].dfVal)
                i = upper;
            else
                i = upper + 1;
            break;
        }
        else if (asColorAssociation[mid].dfVal >= dfVal)
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
            asColorAssociation[0].dfVal != dfVal)
        {
            *pnR = 0;
            *pnG = 0;
            *pnB = 0;
            *pnA = 0;
            return false;
        }
        else
        {
            *pnR = asColorAssociation[0].nR;
            *pnG = asColorAssociation[0].nG;
            *pnB = asColorAssociation[0].nB;
            *pnA = asColorAssociation[0].nA;
            return true;
        }
    }
    else if (i == asColorAssociation.size())
    {
        if (eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY &&
            asColorAssociation[i - 1].dfVal != dfVal)
        {
            *pnR = 0;
            *pnG = 0;
            *pnB = 0;
            *pnA = 0;
            return false;
        }
        else
        {
            *pnR = asColorAssociation[i - 1].nR;
            *pnG = asColorAssociation[i - 1].nG;
            *pnB = asColorAssociation[i - 1].nB;
            *pnA = asColorAssociation[i - 1].nA;
            return true;
        }
    }
    else
    {
        if (asColorAssociation[i - 1].dfVal == dfVal)
        {
            *pnR = asColorAssociation[i - 1].nR;
            *pnG = asColorAssociation[i - 1].nG;
            *pnB = asColorAssociation[i - 1].nB;
            *pnA = asColorAssociation[i - 1].nA;
            return true;
        }

        if (asColorAssociation[i].dfVal == dfVal)
        {
            *pnR = asColorAssociation[i].nR;
            *pnG = asColorAssociation[i].nG;
            *pnB = asColorAssociation[i].nB;
            *pnA = asColorAssociation[i].nA;
            return true;
        }

        if (eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY)
        {
            *pnR = 0;
            *pnG = 0;
            *pnB = 0;
            *pnA = 0;
            return false;
        }

        if (eColorSelectionMode == COLOR_SELECTION_NEAREST_ENTRY &&
            asColorAssociation[i - 1].dfVal != dfVal)
        {
            const size_t index = (dfVal - asColorAssociation[i - 1].dfVal <
                                  asColorAssociation[i].dfVal - dfVal)
                                     ? i - 1
                                     : i;
            *pnR = asColorAssociation[index].nR;
            *pnG = asColorAssociation[index].nG;
            *pnB = asColorAssociation[index].nB;
            *pnA = asColorAssociation[index].nA;
            return true;
        }

        if (std::isnan(asColorAssociation[i - 1].dfVal))
        {
            *pnR = asColorAssociation[i].nR;
            *pnG = asColorAssociation[i].nG;
            *pnB = asColorAssociation[i].nB;
            *pnA = asColorAssociation[i].nA;
            return true;
        }

        const double dfRatio =
            (dfVal - asColorAssociation[i - 1].dfVal) /
            (asColorAssociation[i].dfVal - asColorAssociation[i - 1].dfVal);
        const auto LinearInterpolation = [dfRatio](int nValBefore, int nVal)
        {
            return std::clamp(static_cast<int>(0.5 + nValBefore +
                                               dfRatio * (nVal - nValBefore)),
                              0, 255);
        };

        *pnR = LinearInterpolation(asColorAssociation[i - 1].nR,
                                   asColorAssociation[i].nR);
        *pnG = LinearInterpolation(asColorAssociation[i - 1].nG,
                                   asColorAssociation[i].nG);
        *pnB = LinearInterpolation(asColorAssociation[i - 1].nB,
                                   asColorAssociation[i].nB);
        *pnA = LinearInterpolation(asColorAssociation[i - 1].nA,
                                   asColorAssociation[i].nA);

        return true;
    }
}

static std::vector<GDALColorAssociation>
GDALColorReliefParseColorFile(GDALRasterBandH hSrcBand,
                              const char *pszColorFilename,
                              ColorSelectionMode eColorSelectionMode)
{
    std::vector<GDALColorAssociation> asColorAssociation = GDALLoadTextColorMap(
        pszColorFilename, GDALRasterBand::FromHandle(hSrcBand));
    if (asColorAssociation.empty())
    {
        return {};
    }

    int bSrcHasNoData = FALSE;
    const double dfSrcNoDataValue =
        GDALGetRasterNoDataValue(hSrcBand, &bSrcHasNoData);

    GDALColorReliefProcessColors(asColorAssociation, bSrcHasNoData,
                                 dfSrcNoDataValue, eColorSelectionMode);

    return asColorAssociation;
}

static GByte *GDALColorReliefPrecompute(
    GDALRasterBandH hSrcBand,
    const std::vector<GDALColorAssociation> &asColorAssociation,
    ColorSelectionMode eColorSelectionMode, int *pnIndexOffset)
{
    const GDALDataType eDT = GDALGetRasterDataType(hSrcBand);
    GByte *pabyPrecomputed = nullptr;
    const int nIndexOffset = (eDT == GDT_Int16) ? 32768 : 0;
    *pnIndexOffset = nIndexOffset;
    const int nXSize = GDALGetRasterBandXSize(hSrcBand);
    const int nYSize = GDALGetRasterBandYSize(hSrcBand);
    if (eDT == GDT_UInt8 || ((eDT == GDT_Int16 || eDT == GDT_UInt16) &&
                             static_cast<GIntBig>(nXSize) * nYSize > 65536))
    {
        const int iMax = (eDT == GDT_UInt8) ? 256 : 65536;
        pabyPrecomputed = static_cast<GByte *>(VSI_MALLOC2_VERBOSE(4, iMax));
        if (pabyPrecomputed)
        {
            for (int i = 0; i < iMax; i++)
            {
                int nR = 0;
                int nG = 0;
                int nB = 0;
                int nA = 0;
                GDALColorReliefGetRGBA(asColorAssociation, i - nIndexOffset,
                                       eColorSelectionMode, &nR, &nG, &nB, &nA);
                pabyPrecomputed[4 * i] = static_cast<GByte>(nR);
                pabyPrecomputed[4 * i + 1] = static_cast<GByte>(nG);
                pabyPrecomputed[4 * i + 2] = static_cast<GByte>(nB);
                pabyPrecomputed[4 * i + 3] = static_cast<GByte>(nA);
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

class GDALColorReliefDataset final : public GDALDataset
{
    friend class GDALColorReliefRasterBand;

    GDALDatasetH hSrcDS;
    GDALRasterBandH hSrcBand;
    std::vector<GDALColorAssociation> asColorAssociation{};
    ColorSelectionMode eColorSelectionMode;
    GByte *pabyPrecomputed;
    int nIndexOffset;
    float *pafSourceBuf;
    int *panSourceBuf;
    int nCurBlockXOff;
    int nCurBlockYOff;

    CPL_DISALLOW_COPY_ASSIGN(GDALColorReliefDataset)

  public:
    GDALColorReliefDataset(GDALDatasetH hSrcDS, GDALRasterBandH hSrcBand,
                           const char *pszColorFilename,
                           ColorSelectionMode eColorSelectionMode, int bAlpha);
    ~GDALColorReliefDataset() override;

    bool InitOK() const
    {
        return !asColorAssociation.empty() &&
               (pafSourceBuf != nullptr || panSourceBuf != nullptr);
    }

    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    const OGRSpatialReference *GetSpatialRef() const override;
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
    GDALColorReliefRasterBand(GDALColorReliefDataset *, int);

    CPLErr IReadBlock(int, int, void *) override;
    GDALColorInterp GetColorInterpretation() override;
};

GDALColorReliefDataset::GDALColorReliefDataset(
    GDALDatasetH hSrcDSIn, GDALRasterBandH hSrcBandIn,
    const char *pszColorFilename, ColorSelectionMode eColorSelectionModeIn,
    int bAlpha)
    : hSrcDS(hSrcDSIn), hSrcBand(hSrcBandIn),
      eColorSelectionMode(eColorSelectionModeIn), pabyPrecomputed(nullptr),
      nIndexOffset(0), pafSourceBuf(nullptr), panSourceBuf(nullptr),
      nCurBlockXOff(-1), nCurBlockYOff(-1)
{
    asColorAssociation = GDALColorReliefParseColorFile(
        hSrcBand, pszColorFilename, eColorSelectionMode);

    nRasterXSize = GDALGetRasterXSize(hSrcDS);
    nRasterYSize = GDALGetRasterYSize(hSrcDS);

    int nBlockXSize = 0;
    int nBlockYSize = 0;
    GDALGetBlockSize(hSrcBand, &nBlockXSize, &nBlockYSize);

    pabyPrecomputed = GDALColorReliefPrecompute(
        hSrcBand, asColorAssociation, eColorSelectionMode, &nIndexOffset);

    for (int i = 0; i < ((bAlpha) ? 4 : 3); i++)
    {
        SetBand(i + 1, new GDALColorReliefRasterBand(this, i + 1));
    }

    if (pabyPrecomputed)
        panSourceBuf = static_cast<int *>(
            VSI_MALLOC3_VERBOSE(sizeof(int), nBlockXSize, nBlockYSize));
    else
        pafSourceBuf = static_cast<float *>(
            VSI_MALLOC3_VERBOSE(sizeof(float), nBlockXSize, nBlockYSize));
}

GDALColorReliefDataset::~GDALColorReliefDataset()
{
    CPLFree(pabyPrecomputed);
    CPLFree(panSourceBuf);
    CPLFree(pafSourceBuf);
}

CPLErr GDALColorReliefDataset::GetGeoTransform(GDALGeoTransform &gt) const
{
    return GDALDataset::FromHandle(hSrcDS)->GetGeoTransform(gt);
}

const OGRSpatialReference *GDALColorReliefDataset::GetSpatialRef() const
{
    return GDALDataset::FromHandle(hSrcDS)->GetSpatialRef();
}

GDALColorReliefRasterBand::GDALColorReliefRasterBand(
    GDALColorReliefDataset *poDSIn, int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = GDT_UInt8;
    GDALGetBlockSize(poDSIn->hSrcBand, &nBlockXSize, &nBlockYSize);
}

CPLErr GDALColorReliefRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                             void *pImage)
{
    GDALColorReliefDataset *poGDS =
        cpl::down_cast<GDALColorReliefDataset *>(poDS);
    const int nReqXSize = (nBlockXOff + 1) * nBlockXSize >= nRasterXSize
                              ? nRasterXSize - nBlockXOff * nBlockXSize
                              : nBlockXSize;

    const int nReqYSize = (nBlockYOff + 1) * nBlockYSize >= nRasterYSize
                              ? nRasterYSize - nBlockYOff * nBlockYSize
                              : nBlockYSize;

    if (poGDS->nCurBlockXOff != nBlockXOff ||
        poGDS->nCurBlockYOff != nBlockYOff)
    {
        poGDS->nCurBlockXOff = nBlockXOff;
        poGDS->nCurBlockYOff = nBlockYOff;

        const CPLErr eErr = GDALRasterIO(
            poGDS->hSrcBand, GF_Read, nBlockXOff * nBlockXSize,
            nBlockYOff * nBlockYSize, nReqXSize, nReqYSize,
            (poGDS->panSourceBuf) ? static_cast<void *>(poGDS->panSourceBuf)
                                  : static_cast<void *>(poGDS->pafSourceBuf),
            nReqXSize, nReqYSize,
            (poGDS->panSourceBuf) ? GDT_Int32 : GDT_Float32, 0, 0);
        if (eErr != CE_None)
        {
            memset(pImage, 0, static_cast<size_t>(nBlockXSize) * nBlockYSize);
            return eErr;
        }
    }

    int j = 0;
    if (poGDS->panSourceBuf)
    {
        for (int y = 0; y < nReqYSize; y++)
        {
            for (int x = 0; x < nReqXSize; x++)
            {
                const int nIndex = poGDS->panSourceBuf[j] + poGDS->nIndexOffset;
                static_cast<GByte *>(pImage)[y * nBlockXSize + x] =
                    poGDS->pabyPrecomputed[4 * nIndex + nBand - 1];
                j++;
            }
        }
    }
    else
    {
        int anComponents[4] = {0, 0, 0, 0};
        for (int y = 0; y < nReqYSize; y++)
        {
            for (int x = 0; x < nReqXSize; x++)
            {
                GDALColorReliefGetRGBA(
                    poGDS->asColorAssociation, double(poGDS->pafSourceBuf[j]),
                    poGDS->eColorSelectionMode, &anComponents[0],
                    &anComponents[1], &anComponents[2], &anComponents[3]);
                static_cast<GByte *>(pImage)[y * nBlockXSize + x] =
                    static_cast<GByte>(anComponents[nBand - 1]);
                j++;
            }
        }
    }

    return CE_None;
}

GDALColorInterp GDALColorReliefRasterBand::GetColorInterpretation()
{
    return static_cast<GDALColorInterp>(GCI_RedBand + nBand - 1);
}

static CPLErr
GDALColorRelief(GDALRasterBandH hSrcBand, GDALRasterBandH hDstBand1,
                GDALRasterBandH hDstBand2, GDALRasterBandH hDstBand3,
                GDALRasterBandH hDstBand4, const char *pszColorFilename,
                ColorSelectionMode eColorSelectionMode,
                GDALProgressFunc pfnProgress, void *pProgressData)
{
    if (hSrcBand == nullptr || hDstBand1 == nullptr || hDstBand2 == nullptr ||
        hDstBand3 == nullptr)
        return CE_Failure;

    const auto asColorAssociation = GDALColorReliefParseColorFile(
        hSrcBand, pszColorFilename, eColorSelectionMode);
    if (asColorAssociation.empty())
        return CE_Failure;

    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    /* -------------------------------------------------------------------- */
    /*      Precompute the map from values to RGBA quadruplets              */
    /*      for GDT_UInt8, GDT_Int16 or GDT_UInt16                           */
    /* -------------------------------------------------------------------- */
    int nIndexOffset = 0;
    std::unique_ptr<GByte, VSIFreeReleaser> pabyPrecomputed(
        GDALColorReliefPrecompute(hSrcBand, asColorAssociation,
                                  eColorSelectionMode, &nIndexOffset));

    /* -------------------------------------------------------------------- */
    /*      Initialize progress counter.                                    */
    /* -------------------------------------------------------------------- */

    const int nXSize = GDALGetRasterBandXSize(hSrcBand);
    const int nYSize = GDALGetRasterBandYSize(hSrcBand);

    std::unique_ptr<float, VSIFreeReleaser> pafSourceBuf;
    std::unique_ptr<int, VSIFreeReleaser> panSourceBuf;
    if (pabyPrecomputed)
        panSourceBuf.reset(
            static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nXSize)));
    else
        pafSourceBuf.reset(
            static_cast<float *>(VSI_MALLOC2_VERBOSE(sizeof(float), nXSize)));
    std::unique_ptr<GByte, VSIFreeReleaser> pabyDestBuf(
        static_cast<GByte *>(VSI_MALLOC2_VERBOSE(4, nXSize)));
    GByte *pabyDestBuf1 = pabyDestBuf.get();
    GByte *pabyDestBuf2 = pabyDestBuf1 ? pabyDestBuf1 + nXSize : nullptr;
    GByte *pabyDestBuf3 = pabyDestBuf2 ? pabyDestBuf2 + nXSize : nullptr;
    GByte *pabyDestBuf4 = pabyDestBuf3 ? pabyDestBuf3 + nXSize : nullptr;

    if ((pabyPrecomputed != nullptr && panSourceBuf == nullptr) ||
        (pabyPrecomputed == nullptr && pafSourceBuf == nullptr) ||
        pabyDestBuf1 == nullptr)
    {
        return CE_Failure;
    }

    if (!pfnProgress(0.0, nullptr, pProgressData))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return CE_Failure;
    }

    int nR = 0;
    int nG = 0;
    int nB = 0;
    int nA = 0;

    for (int i = 0; i < nYSize; i++)
    {
        /* Read source buffer */
        CPLErr eErr = GDALRasterIO(
            hSrcBand, GF_Read, 0, i, nXSize, 1,
            panSourceBuf ? static_cast<void *>(panSourceBuf.get())
                         : static_cast<void *>(pafSourceBuf.get()),
            nXSize, 1, panSourceBuf ? GDT_Int32 : GDT_Float32, 0, 0);
        if (eErr != CE_None)
        {
            return eErr;
        }

        if (pabyPrecomputed)
        {
            const auto pabyPrecomputedRaw = pabyPrecomputed.get();
            const auto panSourceBufRaw = panSourceBuf.get();
            for (int j = 0; j < nXSize; j++)
            {
                int nIndex = panSourceBufRaw[j] + nIndexOffset;
                pabyDestBuf1[j] = pabyPrecomputedRaw[4 * nIndex];
                pabyDestBuf2[j] = pabyPrecomputedRaw[4 * nIndex + 1];
                pabyDestBuf3[j] = pabyPrecomputedRaw[4 * nIndex + 2];
                pabyDestBuf4[j] = pabyPrecomputedRaw[4 * nIndex + 3];
            }
        }
        else
        {
            const auto pafSourceBufRaw = pafSourceBuf.get();
            for (int j = 0; j < nXSize; j++)
            {
                GDALColorReliefGetRGBA(asColorAssociation,
                                       double(pafSourceBufRaw[j]),
                                       eColorSelectionMode, &nR, &nG, &nB, &nA);
                pabyDestBuf1[j] = static_cast<GByte>(nR);
                pabyDestBuf2[j] = static_cast<GByte>(nG);
                pabyDestBuf3[j] = static_cast<GByte>(nB);
                pabyDestBuf4[j] = static_cast<GByte>(nA);
            }
        }

        /* -----------------------------------------
         * Write Line to Raster
         */
        eErr = GDALRasterIO(hDstBand1, GF_Write, 0, i, nXSize, 1, pabyDestBuf1,
                            nXSize, 1, GDT_UInt8, 0, 0);
        if (eErr == CE_None)
        {
            eErr = GDALRasterIO(hDstBand2, GF_Write, 0, i, nXSize, 1,
                                pabyDestBuf2, nXSize, 1, GDT_UInt8, 0, 0);
        }
        if (eErr == CE_None)
        {
            eErr = GDALRasterIO(hDstBand3, GF_Write, 0, i, nXSize, 1,
                                pabyDestBuf3, nXSize, 1, GDT_UInt8, 0, 0);
        }
        if (eErr == CE_None && hDstBand4)
        {
            eErr = GDALRasterIO(hDstBand4, GF_Write, 0, i, nXSize, 1,
                                pabyDestBuf4, nXSize, 1, GDT_UInt8, 0, 0);
        }

        if (eErr == CE_None &&
            !pfnProgress(1.0 * (i + 1) / nYSize, nullptr, pProgressData))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            eErr = CE_Failure;
        }
        if (eErr != CE_None)
        {
            return eErr;
        }
    }

    pfnProgress(1.0, nullptr, pProgressData);

    return CE_None;
}

/************************************************************************/
/*                     GDALGenerateVRTColorRelief()                     */
/************************************************************************/

static std::unique_ptr<GDALDataset> GDALGenerateVRTColorRelief(
    const char *pszDest, GDALDatasetH hSrcDataset, GDALRasterBandH hSrcBand,
    const char *pszColorFilename, ColorSelectionMode eColorSelectionMode,
    bool bAddAlpha)
{
    const auto asColorAssociation = GDALColorReliefParseColorFile(
        hSrcBand, pszColorFilename, eColorSelectionMode);
    if (asColorAssociation.empty())
        return nullptr;

    GDALDataset *poSrcDS = GDALDataset::FromHandle(hSrcDataset);
    const int nXSize = GDALGetRasterBandXSize(hSrcBand);
    const int nYSize = GDALGetRasterBandYSize(hSrcBand);

    int nBlockXSize = 0;
    int nBlockYSize = 0;
    GDALGetBlockSize(hSrcBand, &nBlockXSize, &nBlockYSize);

    auto poVRTDS =
        std::make_unique<VRTDataset>(nXSize, nYSize, nBlockXSize, nBlockYSize);
    poVRTDS->SetDescription(pszDest);
    poVRTDS->SetSpatialRef(poSrcDS->GetSpatialRef());
    GDALGeoTransform gt;
    if (poSrcDS->GetGeoTransform(gt) == CE_None)
    {
        poVRTDS->SetGeoTransform(gt);
    }

    const int nBands = 3 + (bAddAlpha ? 1 : 0);

    for (int iBand = 0; iBand < nBands; iBand++)
    {
        poVRTDS->AddBand(GDT_Byte, nullptr);
        auto poVRTBand = cpl::down_cast<VRTSourcedRasterBand *>(
            poVRTDS->GetRasterBand(iBand + 1));
        poVRTBand->SetColorInterpretation(
            static_cast<GDALColorInterp>(GCI_RedBand + iBand));

        auto poComplexSource = std::make_unique<VRTComplexSource>();
        poVRTBand->ConfigureSource(poComplexSource.get(),
                                   GDALRasterBand::FromHandle(hSrcBand), FALSE,
                                   0, 0, nXSize, nYSize, 0, 0, nXSize, nYSize);

        std::vector<double> adfInputLUT;
        std::vector<double> adfOutputLUT;

        for (size_t iColor = 0; iColor < asColorAssociation.size(); iColor++)
        {
            const double dfVal = asColorAssociation[iColor].dfVal;
            if (iColor > 0 &&
                eColorSelectionMode == COLOR_SELECTION_NEAREST_ENTRY &&
                dfVal !=
                    std::nextafter(asColorAssociation[iColor - 1].dfVal,
                                   std::numeric_limits<double>::infinity()))
            {
                const double dfMidVal =
                    (dfVal + asColorAssociation[iColor - 1].dfVal) / 2.0;
                adfInputLUT.push_back(std::nextafter(
                    dfMidVal, -std::numeric_limits<double>::infinity()));
                adfOutputLUT.push_back(
                    (iBand == 0)   ? asColorAssociation[iColor - 1].nR
                    : (iBand == 1) ? asColorAssociation[iColor - 1].nG
                    : (iBand == 2) ? asColorAssociation[iColor - 1].nB
                                   : asColorAssociation[iColor - 1].nA);
                adfInputLUT.push_back(dfMidVal);
                adfOutputLUT.push_back(
                    (iBand == 0)   ? asColorAssociation[iColor].nR
                    : (iBand == 1) ? asColorAssociation[iColor].nG
                    : (iBand == 2) ? asColorAssociation[iColor].nB
                                   : asColorAssociation[iColor].nA);
            }
            else
            {
                if (eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY)
                {
                    adfInputLUT.push_back(std::nextafter(
                        dfVal, -std::numeric_limits<double>::infinity()));
                    adfOutputLUT.push_back(0);
                }
                adfInputLUT.push_back(dfVal);
                adfOutputLUT.push_back(
                    (iBand == 0)   ? asColorAssociation[iColor].nR
                    : (iBand == 1) ? asColorAssociation[iColor].nG
                    : (iBand == 2) ? asColorAssociation[iColor].nB
                                   : asColorAssociation[iColor].nA);
            }

            if (eColorSelectionMode == COLOR_SELECTION_EXACT_ENTRY)
            {
                adfInputLUT.push_back(std::nextafter(
                    dfVal, std::numeric_limits<double>::infinity()));
                adfOutputLUT.push_back(0);
            }
        }

        poComplexSource->SetLUT(adfInputLUT, adfOutputLUT);

        poVRTBand->AddSource(std::move(poComplexSource));
    }

    return poVRTDS;
}

/************************************************************************/
/*                             GDALTRIAlg()                             */
/************************************************************************/

// Implements Wilson et al. (2007), for bathymetric use cases
template <class T>
static float GDALTRIAlgWilson(const T *afWin, float /*fDstNoDataValue*/,
                              const AlgorithmParameters * /*pData*/)
{
    // Terrain Ruggedness is average difference in height
    return (std::abs(afWin[0] - afWin[4]) + std::abs(afWin[1] - afWin[4]) +
            std::abs(afWin[2] - afWin[4]) + std::abs(afWin[3] - afWin[4]) +
            std::abs(afWin[5] - afWin[4]) + std::abs(afWin[6] - afWin[4]) +
            std::abs(afWin[7] - afWin[4]) + std::abs(afWin[8] - afWin[4])) *
           0.125f;
}

// Implements Riley, S.J., De Gloria, S.D., Elliot, R. (1999): A Terrain
// Ruggedness that Quantifies Topographic Heterogeneity. Intermountain Journal
// of Science, Vol.5, No.1-4, pp.23-27 for terrestrial use cases
template <class T>
static float GDALTRIAlgRiley(const T *afWin, float /*fDstNoDataValue*/,
                             const AlgorithmParameters * /*pData*/)
{
    const auto square = [](double x) { return x * x; };

    return static_cast<float>(std::sqrt(square(double(afWin[0] - afWin[4])) +
                                        square(double(afWin[1] - afWin[4])) +
                                        square(double(afWin[2] - afWin[4])) +
                                        square(double(afWin[3] - afWin[4])) +
                                        square(double(afWin[5] - afWin[4])) +
                                        square(double(afWin[6] - afWin[4])) +
                                        square(double(afWin[7] - afWin[4])) +
                                        square(double(afWin[8] - afWin[4]))));
}

/************************************************************************/
/*                             GDALTPIAlg()                             */
/************************************************************************/

template <class T>
static float GDALTPIAlg(const T *afWin, float /*fDstNoDataValue*/,
                        const AlgorithmParameters * /*pData*/)
{
    // Terrain Position is the difference between
    // The central cell and the mean of the surrounding cells
    return afWin[4] - ((afWin[0] + afWin[1] + afWin[2] + afWin[3] + afWin[5] +
                        afWin[6] + afWin[7] + afWin[8]) *
                       0.125f);
}

/************************************************************************/
/*                          GDALRoughnessAlg()                          */
/************************************************************************/

template <class T>
static float GDALRoughnessAlg(const T *afWin, float /*fDstNoDataValue*/,
                              const AlgorithmParameters * /*pData*/)
{
    // Roughness is the largest difference between any two cells

    T fRoughnessMin = afWin[0];
    T fRoughnessMax = afWin[0];

    for (int k = 1; k < 9; k++)
    {
        if (afWin[k] > fRoughnessMax)
        {
            fRoughnessMax = afWin[k];
        }
        if (afWin[k] < fRoughnessMin)
        {
            fRoughnessMin = afWin[k];
        }
    }
    return static_cast<float>(fRoughnessMax - fRoughnessMin);
}

/************************************************************************/
/* ==================================================================== */
/*                       GDALGeneric3x3Dataset                        */
/* ==================================================================== */
/************************************************************************/

template <class T> class GDALGeneric3x3RasterBand;

template <class T> class GDALGeneric3x3Dataset final : public GDALDataset
{
    friend class GDALGeneric3x3RasterBand<T>;

    const typename GDALGeneric3x3ProcessingAlg<T>::type pfnAlg;
    const typename GDALGeneric3x3ProcessingAlg_multisample<T>::type
        pfnAlg_multisample;
    std::unique_ptr<AlgorithmParameters> pAlgData;
    GDALDatasetH hSrcDS = nullptr;
    GDALRasterBandH hSrcBand = nullptr;
    std::array<T *, 3> apafSourceBuf = {nullptr, nullptr, nullptr};
    std::array<bool, 3> abLineHasNoDataValue = {false, false, false};
    std::unique_ptr<float, VSIFreeReleaser> pafOutputBuf{};
    int bDstHasNoData = false;
    double dfDstNoDataValue = 0;
    int nCurLine = -1;
    const bool bComputeAtEdges;
    const bool bTakeReference;

    using GDALDatasetRefCountedPtr =
        std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser>;

    std::vector<GDALDatasetRefCountedPtr> m_apoOverviewDS{};

    CPL_DISALLOW_COPY_ASSIGN(GDALGeneric3x3Dataset)

  public:
    GDALGeneric3x3Dataset(
        GDALDatasetH hSrcDS, GDALRasterBandH hSrcBand,
        GDALDataType eDstDataType, int bDstHasNoData, double dfDstNoDataValue,
        typename GDALGeneric3x3ProcessingAlg<T>::type pfnAlg,
        typename GDALGeneric3x3ProcessingAlg_multisample<T>::type
            pfnAlg_multisample,
        std::unique_ptr<AlgorithmParameters> pAlgData, bool bComputeAtEdges,
        bool bTakeReferenceIn);
    ~GDALGeneric3x3Dataset() override;

    bool InitOK() const
    {
        return apafSourceBuf[0] != nullptr && apafSourceBuf[1] != nullptr &&
               apafSourceBuf[2] != nullptr;
    }

    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    const OGRSpatialReference *GetSpatialRef() const override;
};

/************************************************************************/
/* ==================================================================== */
/*                    GDALGeneric3x3RasterBand                       */
/* ==================================================================== */
/************************************************************************/

template <class T> class GDALGeneric3x3RasterBand final : public GDALRasterBand
{
    friend class GDALGeneric3x3Dataset<T>;
    int bSrcHasNoData = false;
    T fSrcNoDataValue = 0;
    bool bIsSrcNoDataNan = false;
    GDALDataType eReadDT = GDT_Unknown;

    void InitWithNoData(void *pImage);

  public:
    GDALGeneric3x3RasterBand(GDALGeneric3x3Dataset<T> *poDSIn,
                             GDALDataType eDstDataType);

    CPLErr IReadBlock(int, int, void *) override;
    double GetNoDataValue(int *pbHasNoData) override;

    int GetOverviewCount() override
    {
        auto poGDS = cpl::down_cast<GDALGeneric3x3Dataset<T> *>(poDS);
        return static_cast<int>(poGDS->m_apoOverviewDS.size());
    }

    GDALRasterBand *GetOverview(int idx) override
    {
        auto poGDS = cpl::down_cast<GDALGeneric3x3Dataset<T> *>(poDS);
        return idx >= 0 && idx < GetOverviewCount()
                   ? poGDS->m_apoOverviewDS[idx]->GetRasterBand(1)
                   : nullptr;
    }
};

template <class T>
GDALGeneric3x3Dataset<T>::GDALGeneric3x3Dataset(
    GDALDatasetH hSrcDSIn, GDALRasterBandH hSrcBandIn,
    GDALDataType eDstDataType, int bDstHasNoDataIn, double dfDstNoDataValueIn,
    typename GDALGeneric3x3ProcessingAlg<T>::type pfnAlgIn,
    typename GDALGeneric3x3ProcessingAlg_multisample<T>::type
        pfnAlg_multisampleIn,
    std::unique_ptr<AlgorithmParameters> pAlgDataIn, bool bComputeAtEdgesIn,
    bool bTakeReferenceIn)
    : pfnAlg(pfnAlgIn), pfnAlg_multisample(pfnAlg_multisampleIn),
      pAlgData(std::move(pAlgDataIn)), hSrcDS(hSrcDSIn), hSrcBand(hSrcBandIn),
      bDstHasNoData(bDstHasNoDataIn), dfDstNoDataValue(dfDstNoDataValueIn),
      bComputeAtEdges(bComputeAtEdgesIn), bTakeReference(bTakeReferenceIn)
{
    CPLAssert(eDstDataType == GDT_UInt8 || eDstDataType == GDT_Float32);

    if (bTakeReference)
        GDALReferenceDataset(hSrcDS);

    nRasterXSize = GDALGetRasterXSize(hSrcDS);
    nRasterYSize = GDALGetRasterYSize(hSrcDS);

    SetBand(1, new GDALGeneric3x3RasterBand<T>(this, eDstDataType));

    apafSourceBuf[0] =
        static_cast<T *>(VSI_MALLOC2_VERBOSE(sizeof(T), nRasterXSize));
    apafSourceBuf[1] =
        static_cast<T *>(VSI_MALLOC2_VERBOSE(sizeof(T), nRasterXSize));
    apafSourceBuf[2] =
        static_cast<T *>(VSI_MALLOC2_VERBOSE(sizeof(T), nRasterXSize));
    if (pfnAlg_multisample && eDstDataType == GDT_UInt8)
    {
        pafOutputBuf.reset(static_cast<float *>(
            VSI_MALLOC2_VERBOSE(sizeof(float), nRasterXSize)));
    }

    const int nOvrCount = GDALGetOverviewCount(hSrcBandIn);
    for (int i = 0;
         i < nOvrCount && m_apoOverviewDS.size() == static_cast<size_t>(i); ++i)
    {
        auto hOvrBand = GDALGetOverview(hSrcBandIn, i);
        auto hOvrDS = GDALGetBandDataset(hOvrBand);
        if (hOvrDS && hOvrDS != hSrcDSIn)
        {
            auto poOvrDS = std::make_unique<GDALGeneric3x3Dataset>(
                hOvrDS, hOvrBand, eDstDataType, bDstHasNoData, dfDstNoDataValue,
                pfnAlg, pfnAlg_multisampleIn,
                pAlgData ? pAlgData->CreateScaledParameters(
                               static_cast<double>(nRasterXSize) /
                                   GDALGetRasterXSize(hOvrDS),
                               static_cast<double>(nRasterYSize) /
                                   GDALGetRasterYSize(hOvrDS))
                         : nullptr,
                bComputeAtEdges, false);
            if (poOvrDS->InitOK())
            {
                m_apoOverviewDS.emplace_back(poOvrDS.release());
            }
        }
    }
}

template <class T> GDALGeneric3x3Dataset<T>::~GDALGeneric3x3Dataset()
{
    if (bTakeReference)
        GDALReleaseDataset(hSrcDS);

    CPLFree(apafSourceBuf[0]);
    CPLFree(apafSourceBuf[1]);
    CPLFree(apafSourceBuf[2]);
}

template <class T>
CPLErr GDALGeneric3x3Dataset<T>::GetGeoTransform(GDALGeoTransform &gt) const
{
    return GDALDataset::FromHandle(hSrcDS)->GetGeoTransform(gt);
}

template <class T>
const OGRSpatialReference *GDALGeneric3x3Dataset<T>::GetSpatialRef() const
{
    return GDALDataset::FromHandle(hSrcDS)->GetSpatialRef();
}

template <class T>
GDALGeneric3x3RasterBand<T>::GDALGeneric3x3RasterBand(
    GDALGeneric3x3Dataset<T> *poDSIn, GDALDataType eDstDataType)
{
    poDS = poDSIn;
    nBand = 1;
    eDataType = eDstDataType;
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    const double dfNoDataValue =
        GDALGetRasterNoDataValue(poDSIn->hSrcBand, &bSrcHasNoData);
    if (std::numeric_limits<T>::is_integer)
    {
        eReadDT = GDT_Int32;
        if (bSrcHasNoData)
        {
            GDALDataType eSrcDT = GDALGetRasterDataType(poDSIn->hSrcBand);
            CPLAssert(eSrcDT == GDT_UInt8 || eSrcDT == GDT_UInt16 ||
                      eSrcDT == GDT_Int16);
            const int nMinVal = (eSrcDT == GDT_UInt8)    ? 0
                                : (eSrcDT == GDT_UInt16) ? 0
                                                         : -32768;
            const int nMaxVal = (eSrcDT == GDT_UInt8)    ? 255
                                : (eSrcDT == GDT_UInt16) ? 65535
                                                         : 32767;

            if (fabs(dfNoDataValue - floor(dfNoDataValue + 0.5)) < 1e-2 &&
                dfNoDataValue >= nMinVal && dfNoDataValue <= nMaxVal)
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
        bIsSrcNoDataNan = bSrcHasNoData && std::isnan(dfNoDataValue);
    }
}

template <class T>
void GDALGeneric3x3RasterBand<T>::InitWithNoData(void *pImage)
{
    auto poGDS = cpl::down_cast<GDALGeneric3x3Dataset<T> *>(poDS);
    if (eDataType == GDT_UInt8)
    {
        for (int j = 0; j < nBlockXSize; j++)
            static_cast<GByte *>(pImage)[j] =
                static_cast<GByte>(poGDS->dfDstNoDataValue);
    }
    else
    {
        for (int j = 0; j < nBlockXSize; j++)
            static_cast<float *>(pImage)[j] =
                static_cast<float>(poGDS->dfDstNoDataValue);
    }
}

template <class T>
CPLErr GDALGeneric3x3RasterBand<T>::IReadBlock(int /*nBlockXOff*/,
                                               int nBlockYOff, void *pImage)
{
    auto poGDS = cpl::down_cast<GDALGeneric3x3Dataset<T> *>(poDS);

    const auto UpdateLineNoDataFlag = [this, poGDS](int iLine)
    {
        if (bSrcHasNoData)
        {
            poGDS->abLineHasNoDataValue[iLine] = false;
            for (int i = 0; i < nRasterXSize; ++i)
            {
                if constexpr (std::numeric_limits<T>::is_integer)
                {
                    if (poGDS->apafSourceBuf[iLine][i] == fSrcNoDataValue)
                    {
                        poGDS->abLineHasNoDataValue[iLine] = true;
                        break;
                    }
                }
                else
                {
                    if (poGDS->apafSourceBuf[iLine][i] == fSrcNoDataValue ||
                        std::isnan(poGDS->apafSourceBuf[iLine][i]))
                    {
                        poGDS->abLineHasNoDataValue[iLine] = true;
                        break;
                    }
                }
            }
        }
    };

    if (poGDS->bComputeAtEdges && nRasterXSize >= 2 && nRasterYSize >= 2)
    {
        if (nBlockYOff == 0)
        {
            for (int i = 0; i < 2; i++)
            {
                CPLErr eErr = GDALRasterIO(
                    poGDS->hSrcBand, GF_Read, 0, i, nBlockXSize, 1,
                    poGDS->apafSourceBuf[i + 1], nBlockXSize, 1, eReadDT, 0, 0);
                if (eErr != CE_None)
                {
                    InitWithNoData(pImage);
                    return eErr;
                }
                UpdateLineNoDataFlag(i + 1);
            }
            poGDS->nCurLine = 0;

            for (int j = 0; j < nRasterXSize; j++)
            {
                int jmin = (j == 0) ? j : j - 1;
                int jmax = (j == nRasterXSize - 1) ? j : j + 1;

                T afWin[9] = {INTERPOL(poGDS->apafSourceBuf[1][jmin],
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
                              poGDS->apafSourceBuf[2][jmax]};

                const float fVal = ComputeVal(
                    CPL_TO_BOOL(bSrcHasNoData), fSrcNoDataValue,
                    bIsSrcNoDataNan, afWin,
                    static_cast<float>(poGDS->dfDstNoDataValue), poGDS->pfnAlg,
                    poGDS->pAlgData.get(), poGDS->bComputeAtEdges);

                if (eDataType == GDT_UInt8)
                    static_cast<GByte *>(pImage)[j] =
                        static_cast<GByte>(fVal + 0.5f);
                else
                    static_cast<float *>(pImage)[j] = fVal;
            }

            return CE_None;
        }
        else if (nBlockYOff == nRasterYSize - 1)
        {
            if (poGDS->nCurLine != nRasterYSize - 2)
            {
                for (int i = 0; i < 2; i++)
                {
                    CPLErr eErr = GDALRasterIO(
                        poGDS->hSrcBand, GF_Read, 0, nRasterYSize - 2 + i,
                        nBlockXSize, 1, poGDS->apafSourceBuf[i + 1],
                        nBlockXSize, 1, eReadDT, 0, 0);
                    if (eErr != CE_None)
                    {
                        InitWithNoData(pImage);
                        return eErr;
                    }
                    UpdateLineNoDataFlag(i + 1);
                }
            }

            for (int j = 0; j < nRasterXSize; j++)
            {
                int jmin = (j == 0) ? j : j - 1;
                int jmax = (j == nRasterXSize - 1) ? j : j + 1;

                T afWin[9] = {poGDS->apafSourceBuf[1][jmin],
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
                                       bSrcHasNoData, fSrcNoDataValue)};

                const float fVal = ComputeVal(
                    CPL_TO_BOOL(bSrcHasNoData), fSrcNoDataValue,
                    bIsSrcNoDataNan, afWin,
                    static_cast<float>(poGDS->dfDstNoDataValue), poGDS->pfnAlg,
                    poGDS->pAlgData.get(), poGDS->bComputeAtEdges);

                if (eDataType == GDT_UInt8)
                    static_cast<GByte *>(pImage)[j] =
                        static_cast<GByte>(fVal + 0.5f);
                else
                    static_cast<float *>(pImage)[j] = fVal;
            }

            return CE_None;
        }
    }
    else if (nBlockYOff == 0 || nBlockYOff == nRasterYSize - 1)
    {
        InitWithNoData(pImage);
        return CE_None;
    }

    if (poGDS->nCurLine != nBlockYOff)
    {
        if (poGDS->nCurLine + 1 == nBlockYOff)
        {
            T *pafTmp = poGDS->apafSourceBuf[0];
            poGDS->apafSourceBuf[0] = poGDS->apafSourceBuf[1];
            poGDS->apafSourceBuf[1] = poGDS->apafSourceBuf[2];
            poGDS->apafSourceBuf[2] = pafTmp;

            CPLErr eErr = GDALRasterIO(
                poGDS->hSrcBand, GF_Read, 0, nBlockYOff + 1, nBlockXSize, 1,
                poGDS->apafSourceBuf[2], nBlockXSize, 1, eReadDT, 0, 0);

            if (eErr != CE_None)
            {
                InitWithNoData(pImage);
                return eErr;
            }

            UpdateLineNoDataFlag(2);
        }
        else
        {
            for (int i = 0; i < 3; i++)
            {
                const CPLErr eErr = GDALRasterIO(
                    poGDS->hSrcBand, GF_Read, 0, nBlockYOff + i - 1,
                    nBlockXSize, 1, poGDS->apafSourceBuf[i], nBlockXSize, 1,
                    eReadDT, 0, 0);
                if (eErr != CE_None)
                {
                    InitWithNoData(pImage);
                    return eErr;
                }

                UpdateLineNoDataFlag(i);
            }
        }

        poGDS->nCurLine = nBlockYOff;
    }

    if (poGDS->bComputeAtEdges && nRasterXSize >= 2)
    {
        int j = 0;
        T afWin[9] = {
            INTERPOL(poGDS->apafSourceBuf[0][j], poGDS->apafSourceBuf[0][j + 1],
                     bSrcHasNoData, fSrcNoDataValue),
            poGDS->apafSourceBuf[0][j],
            poGDS->apafSourceBuf[0][j + 1],
            INTERPOL(poGDS->apafSourceBuf[1][j], poGDS->apafSourceBuf[1][j + 1],
                     bSrcHasNoData, fSrcNoDataValue),
            poGDS->apafSourceBuf[1][j],
            poGDS->apafSourceBuf[1][j + 1],
            INTERPOL(poGDS->apafSourceBuf[2][j], poGDS->apafSourceBuf[2][j + 1],
                     bSrcHasNoData, fSrcNoDataValue),
            poGDS->apafSourceBuf[2][j],
            poGDS->apafSourceBuf[2][j + 1]};

        {
            const float fVal = ComputeVal(
                CPL_TO_BOOL(bSrcHasNoData), fSrcNoDataValue, bIsSrcNoDataNan,
                afWin, static_cast<float>(poGDS->dfDstNoDataValue),
                poGDS->pfnAlg, poGDS->pAlgData.get(), poGDS->bComputeAtEdges);

            if (eDataType == GDT_UInt8)
                static_cast<GByte *>(pImage)[j] =
                    static_cast<GByte>(fVal + 0.5f);
            else
                static_cast<float *>(pImage)[j] = fVal;
        }

        j = nRasterXSize - 1;

        afWin[0] = poGDS->apafSourceBuf[0][j - 1];
        afWin[1] = poGDS->apafSourceBuf[0][j];
        afWin[2] =
            INTERPOL(poGDS->apafSourceBuf[0][j], poGDS->apafSourceBuf[0][j - 1],
                     bSrcHasNoData, fSrcNoDataValue);
        afWin[3] = poGDS->apafSourceBuf[1][j - 1];
        afWin[4] = poGDS->apafSourceBuf[1][j];
        afWin[5] =
            INTERPOL(poGDS->apafSourceBuf[1][j], poGDS->apafSourceBuf[1][j - 1],
                     bSrcHasNoData, fSrcNoDataValue);
        afWin[6] = poGDS->apafSourceBuf[2][j - 1];
        afWin[7] = poGDS->apafSourceBuf[2][j];
        afWin[8] =
            INTERPOL(poGDS->apafSourceBuf[2][j], poGDS->apafSourceBuf[2][j - 1],
                     bSrcHasNoData, fSrcNoDataValue);

        const float fVal = ComputeVal(
            CPL_TO_BOOL(bSrcHasNoData), fSrcNoDataValue, bIsSrcNoDataNan, afWin,
            static_cast<float>(poGDS->dfDstNoDataValue), poGDS->pfnAlg,
            poGDS->pAlgData.get(), poGDS->bComputeAtEdges);

        if (eDataType == GDT_UInt8)
            static_cast<GByte *>(pImage)[j] = static_cast<GByte>(fVal + 0.5f);
        else
            static_cast<float *>(pImage)[j] = fVal;
    }
    else
    {
        if (eDataType == GDT_UInt8)
        {
            static_cast<GByte *>(pImage)[0] =
                static_cast<GByte>(poGDS->dfDstNoDataValue);
            if (nBlockXSize > 1)
                static_cast<GByte *>(pImage)[nBlockXSize - 1] =
                    static_cast<GByte>(poGDS->dfDstNoDataValue);
        }
        else
        {
            static_cast<float *>(pImage)[0] =
                static_cast<float>(poGDS->dfDstNoDataValue);
            if (nBlockXSize > 1)
                static_cast<float *>(pImage)[nBlockXSize - 1] =
                    static_cast<float>(poGDS->dfDstNoDataValue);
        }
    }

    int j = 1;
    if (poGDS->pfnAlg_multisample &&
        (eDataType == GDT_Float32 || poGDS->pafOutputBuf) &&
        !poGDS->abLineHasNoDataValue[0] && !poGDS->abLineHasNoDataValue[1] &&
        !poGDS->abLineHasNoDataValue[2])
    {
        j = poGDS->pfnAlg_multisample(
            poGDS->apafSourceBuf[0], poGDS->apafSourceBuf[1],
            poGDS->apafSourceBuf[2], nRasterXSize, poGDS->pAlgData.get(),
            poGDS->pafOutputBuf ? poGDS->pafOutputBuf.get()
                                : static_cast<float *>(pImage));

        if (poGDS->pafOutputBuf)
        {
            GDALCopyWords64(poGDS->pafOutputBuf.get() + 1, GDT_Float32,
                            static_cast<int>(sizeof(float)),
                            static_cast<GByte *>(pImage) + 1, GDT_UInt8, 1,
                            j - 1);
        }
    }

    for (; j < nBlockXSize - 1; j++)
    {
        T afWin[9] = {
            poGDS->apafSourceBuf[0][j - 1], poGDS->apafSourceBuf[0][j],
            poGDS->apafSourceBuf[0][j + 1], poGDS->apafSourceBuf[1][j - 1],
            poGDS->apafSourceBuf[1][j],     poGDS->apafSourceBuf[1][j + 1],
            poGDS->apafSourceBuf[2][j - 1], poGDS->apafSourceBuf[2][j],
            poGDS->apafSourceBuf[2][j + 1]};

        const float fVal = ComputeVal(
            CPL_TO_BOOL(bSrcHasNoData), fSrcNoDataValue, bIsSrcNoDataNan, afWin,
            static_cast<float>(poGDS->dfDstNoDataValue), poGDS->pfnAlg,
            poGDS->pAlgData.get(), poGDS->bComputeAtEdges);

        if (eDataType == GDT_UInt8)
            static_cast<GByte *>(pImage)[j] = static_cast<GByte>(fVal + 0.5f);
        else
            static_cast<float *>(pImage)[j] = fVal;
    }

    return CE_None;
}

template <class T>
double GDALGeneric3x3RasterBand<T>::GetNoDataValue(int *pbHasNoData)
{
    auto poGDS = cpl::down_cast<GDALGeneric3x3Dataset<T> *>(poDS);
    if (pbHasNoData)
        *pbHasNoData = poGDS->bDstHasNoData;
    return poGDS->dfDstNoDataValue;
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

static Algorithm GetAlgorithm(const char *pszProcessing)
{
    if (EQUAL(pszProcessing, "shade") || EQUAL(pszProcessing, "hillshade"))
    {
        return HILL_SHADE;
    }
    else if (EQUAL(pszProcessing, "slope"))
    {
        return SLOPE;
    }
    else if (EQUAL(pszProcessing, "aspect"))
    {
        return ASPECT;
    }
    else if (EQUAL(pszProcessing, "color-relief"))
    {
        return COLOR_RELIEF;
    }
    else if (EQUAL(pszProcessing, "TRI"))
    {
        return TRI;
    }
    else if (EQUAL(pszProcessing, "TPI"))
    {
        return TPI;
    }
    else if (EQUAL(pszProcessing, "roughness"))
    {
        return ROUGHNESS;
    }
    else
    {
        return INVALID;
    }
}

/************************************************************************/
/*                     GDALDEMAppOptionsGetParser()                     */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser> GDALDEMAppOptionsGetParser(
    GDALDEMProcessingOptions *psOptions,
    GDALDEMProcessingOptionsForBinary *psOptionsForBinary)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "gdaldem", /* bForBinary=*/psOptionsForBinary != nullptr);

    argParser->add_description(_("Tools to analyze and visualize DEMs."));

    argParser->add_epilog(_("For more details, consult "
                            "https://gdal.org/programs/gdaldem.html"));

    // Common options (for all modes)
    auto addCommonOptions =
        [psOptions, psOptionsForBinary](GDALArgumentParser *subParser)
    {
        subParser->add_output_format_argument(psOptions->osFormat);

        subParser->add_argument("-compute_edges")
            .flag()
            .store_into(psOptions->bComputeAtEdges)
            .help(_(
                "Do the computation at raster edges and near nodata values."));

        auto &bandArg = subParser->add_argument("-b")
                            .metavar("<value>")
                            .scan<'i', int>()
                            .store_into(psOptions->nBand)
                            .help(_("Select an input band."));

        subParser->add_hidden_alias_for(bandArg, "--b");

        subParser->add_creation_options_argument(psOptions->aosCreationOptions);

        if (psOptionsForBinary)
        {
            subParser->add_quiet_argument(&psOptionsForBinary->bQuiet);
        }
    };

    // Hillshade

    auto subCommandHillshade = argParser->add_subparser(
        "hillshade", /* bForBinary=*/psOptionsForBinary != nullptr);
    subCommandHillshade->add_description(_("Compute hillshade."));

    if (psOptionsForBinary)
    {
        subCommandHillshade->add_argument("input_dem")
            .store_into(psOptionsForBinary->osSrcFilename)
            .help(_("The input DEM raster to be processed."));

        subCommandHillshade->add_argument("output_hillshade")
            .store_into(psOptionsForBinary->osDstFilename)
            .help(_("The output raster to be produced."));
    }

    subCommandHillshade->add_argument("-alg")
        .metavar("<Horn|ZevenbergenThorne>")
        .action(
            [psOptions](const std::string &s)
            {
                if (EQUAL(s.c_str(), "ZevenbergenThorne"))
                {
                    psOptions->bGradientAlgSpecified = true;
                    psOptions->eGradientAlg = GradientAlg::ZEVENBERGEN_THORNE;
                }
                else if (EQUAL(s.c_str(), "Horn"))
                {
                    psOptions->bGradientAlgSpecified = true;
                    psOptions->eGradientAlg = GradientAlg::HORN;
                }
                else
                {
                    throw std::invalid_argument(
                        CPLSPrintf("Invalid value for -alg: %s.", s.c_str()));
                }
            })
        .help(_("Choose between ZevenbergenThorne or Horn algorithms."));

    subCommandHillshade->add_argument("-z")
        .metavar("<factor>")
        .scan<'g', double>()
        .store_into(psOptions->z)
        .help(_("Vertical exaggeration."));

    auto &scaleHillshadeArg =
        subCommandHillshade->add_argument("-s")
            .metavar("<scale>")
            .scan<'g', double>()
            .store_into(psOptions->globalScale)
            .help(_("Ratio of vertical units to horizontal units."));

    subCommandHillshade->add_hidden_alias_for(scaleHillshadeArg, "--s");
    subCommandHillshade->add_hidden_alias_for(scaleHillshadeArg, "-scale");
    subCommandHillshade->add_hidden_alias_for(scaleHillshadeArg, "--scale");

    auto &xscaleHillshadeArg =
        subCommandHillshade->add_argument("-xscale")
            .metavar("<scale>")
            .scan<'g', double>()
            .store_into(psOptions->xscale)
            .help(_("Ratio of vertical units to horizontal X axis units."));
    subCommandHillshade->add_hidden_alias_for(xscaleHillshadeArg, "--xscale");

    auto &yscaleHillshadeArg =
        subCommandHillshade->add_argument("-yscale")
            .metavar("<scale>")
            .scan<'g', double>()
            .store_into(psOptions->yscale)
            .help(_("Ratio of vertical units to horizontal Y axis units."));
    subCommandHillshade->add_hidden_alias_for(yscaleHillshadeArg, "--yscale");

    auto &azimuthHillshadeArg =
        subCommandHillshade->add_argument("-az")
            .metavar("<azimuth>")
            .scan<'g', double>()
            .store_into(psOptions->az)
            .help(_("Azimuth of the light, in degrees."));

    subCommandHillshade->add_hidden_alias_for(azimuthHillshadeArg, "--az");
    subCommandHillshade->add_hidden_alias_for(azimuthHillshadeArg, "-azimuth");
    subCommandHillshade->add_hidden_alias_for(azimuthHillshadeArg, "--azimuth");

    auto &altitudeHillshadeArg =
        subCommandHillshade->add_argument("-alt")
            .metavar("<altitude>")
            .scan<'g', double>()
            .store_into(psOptions->alt)
            .help(_("Altitude of the light, in degrees."));

    subCommandHillshade->add_hidden_alias_for(altitudeHillshadeArg, "--alt");
    subCommandHillshade->add_hidden_alias_for(altitudeHillshadeArg,
                                              "--altitude");
    subCommandHillshade->add_hidden_alias_for(altitudeHillshadeArg,
                                              "-altitude");

    auto &shadingGroup = subCommandHillshade->add_mutually_exclusive_group();

    auto &combinedHillshadeArg = shadingGroup.add_argument("-combined")
                                     .flag()
                                     .store_into(psOptions->bCombined)
                                     .help(_("Use combined shading."));

    subCommandHillshade->add_hidden_alias_for(combinedHillshadeArg,
                                              "--combined");

    auto &multidirectionalHillshadeArg =
        shadingGroup.add_argument("-multidirectional")
            .flag()
            .store_into(psOptions->bMultiDirectional)
            .help(_("Use multidirectional shading."));

    subCommandHillshade->add_hidden_alias_for(multidirectionalHillshadeArg,
                                              "--multidirectional");

    auto &igorHillshadeArg =
        shadingGroup.add_argument("-igor")
            .flag()
            .store_into(psOptions->bIgor)
            .help(_("Shading which tries to minimize effects on other map "
                    "features beneath."));

    subCommandHillshade->add_hidden_alias_for(igorHillshadeArg, "--igor");

    addCommonOptions(subCommandHillshade);

    // Slope

    auto subCommandSlope = argParser->add_subparser(
        "slope", /* bForBinary=*/psOptionsForBinary != nullptr);

    subCommandSlope->add_description(_("Compute slope."));

    if (psOptionsForBinary)
    {
        subCommandSlope->add_argument("input_dem")
            .store_into(psOptionsForBinary->osSrcFilename)
            .help(_("The input DEM raster to be processed."));

        subCommandSlope->add_argument("output_slope_map")
            .store_into(psOptionsForBinary->osDstFilename)
            .help(_("The output raster to be produced."));
    }

    subCommandSlope->add_argument("-alg")
        .metavar("Horn|ZevenbergenThorne")
        .action(
            [psOptions](const std::string &s)
            {
                if (EQUAL(s.c_str(), "ZevenbergenThorne"))
                {
                    psOptions->bGradientAlgSpecified = true;
                    psOptions->eGradientAlg = GradientAlg::ZEVENBERGEN_THORNE;
                }
                else if (EQUAL(s.c_str(), "Horn"))
                {
                    psOptions->bGradientAlgSpecified = true;
                    psOptions->eGradientAlg = GradientAlg::HORN;
                }
                else
                {
                    throw std::invalid_argument(
                        CPLSPrintf("Invalid value for -alg: %s.", s.c_str()));
                }
            })
        .help(_("Choose between ZevenbergenThorne or Horn algorithms."));

    subCommandSlope->add_inverted_logic_flag(
        "-p", &psOptions->bSlopeFormatUseDegrees,
        _("Express slope as a percentage."));

    subCommandSlope->add_argument("-s")
        .metavar("<scale>")
        .scan<'g', double>()
        .store_into(psOptions->globalScale)
        .help(_("Ratio of vertical units to horizontal."));

    auto &xscaleSlopeArg =
        subCommandSlope->add_argument("-xscale")
            .metavar("<scale>")
            .scan<'g', double>()
            .store_into(psOptions->xscale)
            .help(_("Ratio of vertical units to horizontal X axis units."));
    subCommandSlope->add_hidden_alias_for(xscaleSlopeArg, "--xscale");

    auto &yscaleSlopeArg =
        subCommandSlope->add_argument("-yscale")
            .metavar("<scale>")
            .scan<'g', double>()
            .store_into(psOptions->yscale)
            .help(_("Ratio of vertical units to horizontal Y axis units."));
    subCommandSlope->add_hidden_alias_for(yscaleSlopeArg, "--yscale");

    addCommonOptions(subCommandSlope);

    // Aspect

    auto subCommandAspect = argParser->add_subparser(
        "aspect", /* bForBinary=*/psOptionsForBinary != nullptr);

    subCommandAspect->add_description(_("Compute aspect."));

    if (psOptionsForBinary)
    {
        subCommandAspect->add_argument("input_dem")
            .store_into(psOptionsForBinary->osSrcFilename)
            .help(_("The input DEM raster to be processed."));

        subCommandAspect->add_argument("output_aspect_map")
            .store_into(psOptionsForBinary->osDstFilename)
            .help(_("The output raster to be produced."));
    }

    subCommandAspect->add_argument("-alg")
        .metavar("Horn|ZevenbergenThorne")
        .action(
            [psOptions](const std::string &s)
            {
                if (EQUAL(s.c_str(), "ZevenbergenThorne"))
                {
                    psOptions->bGradientAlgSpecified = true;
                    psOptions->eGradientAlg = GradientAlg::ZEVENBERGEN_THORNE;
                }
                else if (EQUAL(s.c_str(), "Horn"))
                {
                    psOptions->bGradientAlgSpecified = true;
                    psOptions->eGradientAlg = GradientAlg::HORN;
                }
                else
                {
                    throw std::invalid_argument(
                        CPLSPrintf("Invalid value for -alg: %s.", s.c_str()));
                }
            })
        .help(_("Choose between ZevenbergenThorne or Horn algorithms."));

    subCommandAspect->add_inverted_logic_flag(
        "-trigonometric", &psOptions->bAngleAsAzimuth,
        _("Express aspect in trigonometric format."));

    subCommandAspect->add_argument("-zero_for_flat")
        .flag()
        .store_into(psOptions->bZeroForFlat)
        .help(_("Return 0 for flat areas with slope=0, instead of -9999."));

    addCommonOptions(subCommandAspect);

    // Color-relief

    auto subCommandColorRelief = argParser->add_subparser(
        "color-relief", /* bForBinary=*/psOptionsForBinary != nullptr);

    subCommandColorRelief->add_description(
        _("Color relief computed from the elevation and a text-based color "
          "configuration file."));

    if (psOptionsForBinary)
    {
        subCommandColorRelief->add_argument("input_dem")
            .store_into(psOptionsForBinary->osSrcFilename)
            .help(_("The input DEM raster to be processed."));

        subCommandColorRelief->add_argument("color_text_file")
            .store_into(psOptionsForBinary->osColorFilename)
            .help(_("Text-based color configuration file."));

        subCommandColorRelief->add_argument("output_color_relief_map")
            .store_into(psOptionsForBinary->osDstFilename)
            .help(_("The output raster to be produced."));
    }

    subCommandColorRelief->add_argument("-alpha")
        .flag()
        .store_into(psOptions->bAddAlpha)
        .help(_("Add an alpha channel to the output raster."));

    subCommandColorRelief->add_argument("-exact_color_entry")
        .nargs(0)
        .action(
            [psOptions](const auto &)
            { psOptions->eColorSelectionMode = COLOR_SELECTION_EXACT_ENTRY; })
        .help(
            _("Use strict matching when searching in the configuration file."));

    subCommandColorRelief->add_argument("-nearest_color_entry")
        .nargs(0)
        .action(
            [psOptions](const auto &)
            { psOptions->eColorSelectionMode = COLOR_SELECTION_NEAREST_ENTRY; })
        .help(_("Use the RGBA corresponding to the closest entry in the "
                "configuration file."));

    addCommonOptions(subCommandColorRelief);

    // TRI

    auto subCommandTRI = argParser->add_subparser(
        "TRI", /* bForBinary=*/psOptionsForBinary != nullptr);

    subCommandTRI->add_description(_("Compute the Terrain Ruggedness Index."));

    if (psOptionsForBinary)
    {

        subCommandTRI->add_argument("input_dem")
            .store_into(psOptionsForBinary->osSrcFilename)
            .help(_("The input DEM raster to be processed."));

        subCommandTRI->add_argument("output_TRI_map")
            .store_into(psOptionsForBinary->osDstFilename)
            .help(_("The output raster to be produced."));
    }

    subCommandTRI->add_argument("-alg")
        .metavar("Wilson|Riley")
        .action(
            [psOptions](const std::string &s)
            {
                if (EQUAL(s.c_str(), "Wilson"))
                {
                    psOptions->bTRIAlgSpecified = true;
                    psOptions->eTRIAlg = TRIAlg::WILSON;
                }
                else if (EQUAL(s.c_str(), "Riley"))
                {
                    psOptions->bTRIAlgSpecified = true;
                    psOptions->eTRIAlg = TRIAlg::RILEY;
                }
                else
                {
                    throw std::invalid_argument(
                        CPLSPrintf("Invalid value for -alg: %s.", s.c_str()));
                }
            })
        .help(_("Choose between Wilson or Riley algorithms."));

    addCommonOptions(subCommandTRI);

    // TPI

    auto subCommandTPI = argParser->add_subparser(
        "TPI", /* bForBinary=*/psOptionsForBinary != nullptr);

    subCommandTPI->add_description(
        _("Compute the Topographic Position Index."));

    if (psOptionsForBinary)
    {
        subCommandTPI->add_argument("input_dem")
            .store_into(psOptionsForBinary->osSrcFilename)
            .help(_("The input DEM raster to be processed."));

        subCommandTPI->add_argument("output_TPI_map")
            .store_into(psOptionsForBinary->osDstFilename)
            .help(_("The output raster to be produced."));
    }

    addCommonOptions(subCommandTPI);

    // Roughness

    auto subCommandRoughness = argParser->add_subparser(
        "roughness", /* bForBinary=*/psOptionsForBinary != nullptr);

    subCommandRoughness->add_description(
        _("Compute the roughness of the DEM."));

    if (psOptionsForBinary)
    {
        subCommandRoughness->add_argument("input_dem")
            .store_into(psOptionsForBinary->osSrcFilename)
            .help(_("The input DEM raster to be processed."));

        subCommandRoughness->add_argument("output_roughness_map")
            .store_into(psOptionsForBinary->osDstFilename)
            .help(_("The output raster to be produced."));
    }

    addCommonOptions(subCommandRoughness);

    return argParser;
}

/************************************************************************/
/*                      GDALDEMAppGetParserUsage()                      */
/************************************************************************/

std::string GDALDEMAppGetParserUsage(const std::string &osProcessingMode)
{
    try
    {
        GDALDEMProcessingOptions sOptions;
        GDALDEMProcessingOptionsForBinary sOptionsForBinary;
        auto argParser =
            GDALDEMAppOptionsGetParser(&sOptions, &sOptionsForBinary);
        if (!osProcessingMode.empty())
        {
            const auto subParser = argParser->get_subparser(osProcessingMode);
            if (subParser)
            {
                return subParser->usage();
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid processing mode: %s",
                         osProcessingMode.c_str());
            }
        }
        return argParser->usage();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                 err.what());
        return std::string();
    }
}

/************************************************************************/
/*                         GDALDEMProcessing()                          */
/************************************************************************/

/**
 * Apply a DEM processing.
 *
 * This is the equivalent of the <a href="/programs/gdaldem.html">gdaldem</a>
 * utility.
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
 * @param pbUsageError pointer to a integer output variable to store if any
 * usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using
 * GDALClose()) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALDEMProcessing(const char *pszDest, GDALDatasetH hSrcDataset,
                               const char *pszProcessing,
                               const char *pszColorFilename,
                               const GDALDEMProcessingOptions *psOptionsIn,
                               int *pbUsageError)
{
    if (hSrcDataset == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No source dataset specified.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if (pszDest == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No target dataset specified.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if (pszProcessing == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No target dataset specified.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    Algorithm eUtilityMode = GetAlgorithm(pszProcessing);
    if (eUtilityMode == INVALID)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Invalid processing mode: %s",
                 pszProcessing);
        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    if (eUtilityMode == COLOR_RELIEF &&
        (pszColorFilename == nullptr || strlen(pszColorFilename) == 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "pszColorFilename == NULL.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    else if (eUtilityMode != COLOR_RELIEF && pszColorFilename != nullptr &&
             strlen(pszColorFilename) > 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "pszColorFilename != NULL.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    if (psOptionsIn && psOptionsIn->bCombined && eUtilityMode != HILL_SHADE)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "-combined can only be used with hillshade");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    if (psOptionsIn && psOptionsIn->bIgor && eUtilityMode != HILL_SHADE)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "-igor can only be used with hillshade");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    if (psOptionsIn && psOptionsIn->bMultiDirectional &&
        eUtilityMode != HILL_SHADE)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "-multidirectional can only be used with hillshade");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    std::unique_ptr<GDALDEMProcessingOptions> psOptionsToFree;
    if (psOptionsIn)
    {
        psOptionsToFree =
            std::make_unique<GDALDEMProcessingOptions>(*psOptionsIn);
    }
    else
    {
        psOptionsToFree.reset(GDALDEMProcessingOptionsNew(nullptr, nullptr));
    }

    GDALDEMProcessingOptions *psOptions = psOptionsToFree.get();

    double adfGeoTransform[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    // Keep that variable in that scope.
    // A reference on that dataset will be taken by GDALGeneric3x3Dataset
    // (and released at its destruction) if we go to that code path, and
    // GDALWarp() (actually the VRTWarpedDataset) will itself take a reference
    // on hSrcDataset.
    std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser> poTmpSrcDS;

    if (GDALGetGeoTransform(hSrcDataset, adfGeoTransform) == CE_None &&
        // For following modes, detect non north-up datasets and autowarp
        // them so they are north-up oriented.
        (((eUtilityMode == ASPECT || eUtilityMode == TRI ||
           eUtilityMode == TPI) &&
          (adfGeoTransform[2] != 0 || adfGeoTransform[4] != 0 ||
           adfGeoTransform[5] > 0)) ||
         // For following modes, detect rotated datasets and autowarp
         // them so they are north-up oriented. (south-up are fine for those)
         ((eUtilityMode == SLOPE || eUtilityMode == HILL_SHADE) &&
          (adfGeoTransform[2] != 0 || adfGeoTransform[4] != 0))))
    {
        const char *const apszWarpOptions[] = {"-of", "VRT", nullptr};
        GDALWarpAppOptions *psWarpOptions = GDALWarpAppOptionsNew(
            const_cast<char **>(apszWarpOptions), nullptr);
        poTmpSrcDS.reset(GDALDataset::FromHandle(
            GDALWarp("", nullptr, 1, &hSrcDataset, psWarpOptions, nullptr)));
        GDALWarpAppOptionsFree(psWarpOptions);
        if (!poTmpSrcDS)
            return nullptr;
        hSrcDataset = GDALDataset::ToHandle(poTmpSrcDS.get());
        GDALGetGeoTransform(hSrcDataset, adfGeoTransform);
    }

    const int nXSize = GDALGetRasterXSize(hSrcDataset);
    const int nYSize = GDALGetRasterYSize(hSrcDataset);

    if (psOptions->nBand <= 0 ||
        psOptions->nBand > GDALGetRasterCount(hSrcDataset))
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Unable to fetch band #%d",
                 psOptions->nBand);

        return nullptr;
    }

    if (std::isnan(psOptions->xscale))
    {
        psOptions->xscale = 1;
        psOptions->yscale = 1;

        double zunit = 1;

        auto poSrcDS = GDALDataset::FromHandle(hSrcDataset);

        const char *pszUnits =
            poSrcDS->GetRasterBand(psOptions->nBand)->GetUnitType();
        if (EQUAL(pszUnits, "m") || EQUAL(pszUnits, "metre") ||
            EQUAL(pszUnits, "meter"))
        {
        }
        else if (EQUAL(pszUnits, "ft") || EQUAL(pszUnits, "foot") ||
                 EQUAL(pszUnits, "foot (International)") ||
                 EQUAL(pszUnits, "feet"))
        {
            zunit = CPLAtof(SRS_UL_FOOT_CONV);
        }
        else if (EQUAL(pszUnits, "us-ft") || EQUAL(pszUnits, "Foot_US") ||
                 EQUAL(pszUnits, "US survey foot"))
        {
            zunit = CPLAtof(SRS_UL_US_FOOT_CONV);
        }
        else if (!EQUAL(pszUnits, ""))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unknown band unit '%s'. Assuming metre", pszUnits);
        }

        const auto poSrcSRS = poSrcDS->GetSpatialRef();
        if (poSrcSRS && poSrcSRS->IsGeographic())
        {
            GDALGeoTransform gt;
            if (poSrcDS->GetGeoTransform(gt) == CE_None)
            {
                const double dfAngUnits = poSrcSRS->GetAngularUnits();
                // Rough conversion of angular units to linear units.
                psOptions->yscale =
                    dfAngUnits * poSrcSRS->GetSemiMajor() / zunit;
                // Take the center latitude to compute the xscale.
                const double dfMeanLat =
                    (gt.yorig + nYSize * gt.yscale / 2) * dfAngUnits;
                if (std::fabs(dfMeanLat) / M_PI * 180 > 80)
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Automatic computation of xscale at high latitudes may "
                        "lead to incorrect results. The source dataset should "
                        "likely be reprojected to a polar projection");
                }
                psOptions->xscale = psOptions->yscale * cos(dfMeanLat);
            }
        }
        else if (poSrcSRS && poSrcSRS->IsProjected())
        {
            psOptions->xscale = poSrcSRS->GetLinearUnits() / zunit;
            psOptions->yscale = psOptions->xscale;
        }
        CPLDebug("GDAL", "Using xscale=%f and yscale=%f", psOptions->xscale,
                 psOptions->yscale);
    }

    if (psOptions->bGradientAlgSpecified &&
        !(eUtilityMode == HILL_SHADE || eUtilityMode == SLOPE ||
          eUtilityMode == ASPECT))
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "This value of -alg is only valid for hillshade, slope or "
                 "aspect processing");

        return nullptr;
    }

    if (psOptions->bTRIAlgSpecified && !(eUtilityMode == TRI))
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "This value of -alg is only valid for TRI processing");

        return nullptr;
    }

    GDALRasterBandH hSrcBand = GDALGetRasterBand(hSrcDataset, psOptions->nBand);

    CPLString osFormat;
    if (psOptions->osFormat.empty())
    {
        osFormat = GetOutputDriverForRaster(pszDest);
        if (osFormat.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not identify driver for output %s", pszDest);
            return nullptr;
        }
    }
    else
    {
        osFormat = psOptions->osFormat;
    }

    GDALDriverH hDriver = nullptr;
    if (!EQUAL(osFormat.c_str(), "stream"))
    {
        hDriver = GDALGetDriverByName(osFormat);
        if (hDriver == nullptr ||
            (GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE, nullptr) ==
                 nullptr &&
             GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATECOPY, nullptr) ==
                 nullptr))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Output driver `%s' does not support writing.",
                     osFormat.c_str());
            fprintf(stderr, "The following format drivers are enabled\n"
                            "and support writing:\n");

            for (int iDr = 0; iDr < GDALGetDriverCount(); iDr++)
            {
                hDriver = GDALGetDriver(iDr);

                if (GDALGetMetadataItem(hDriver, GDAL_DCAP_RASTER, nullptr) !=
                        nullptr &&
                    (GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE, nullptr) !=
                         nullptr ||
                     GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATECOPY,
                                         nullptr) != nullptr))
                {
                    fprintf(stderr, "  %s: %s\n",
                            GDALGetDriverShortName(hDriver),
                            GDALGetDriverLongName(hDriver));
                }
            }

            return nullptr;
        }
    }

    double dfDstNoDataValue = 0.0;
    bool bDstHasNoData = false;
    std::unique_ptr<AlgorithmParameters> pData;
    GDALGeneric3x3ProcessingAlg<float>::type pfnAlgFloat = nullptr;
    GDALGeneric3x3ProcessingAlg<GInt32>::type pfnAlgInt32 = nullptr;
    GDALGeneric3x3ProcessingAlg_multisample<float>::type
        pfnAlgFloat_multisample = nullptr;
    GDALGeneric3x3ProcessingAlg_multisample<GInt32>::type
        pfnAlgInt32_multisample = nullptr;

    if (eUtilityMode == HILL_SHADE && psOptions->bMultiDirectional)
    {
        dfDstNoDataValue = 0;
        bDstHasNoData = true;
        pData = GDALCreateHillshadeMultiDirectionalData(
            adfGeoTransform, psOptions->z, psOptions->xscale, psOptions->yscale,
            psOptions->alt, psOptions->eGradientAlg);
        if (psOptions->eGradientAlg == GradientAlg::ZEVENBERGEN_THORNE)
        {
            pfnAlgFloat = GDALHillshadeMultiDirectionalAlg<
                float, GradientAlg::ZEVENBERGEN_THORNE>;
            pfnAlgInt32 = GDALHillshadeMultiDirectionalAlg<
                GInt32, GradientAlg::ZEVENBERGEN_THORNE>;
        }
        else
        {
            pfnAlgFloat =
                GDALHillshadeMultiDirectionalAlg<float, GradientAlg::HORN>;
            pfnAlgInt32 =
                GDALHillshadeMultiDirectionalAlg<GInt32, GradientAlg::HORN>;
        }
    }
    else if (eUtilityMode == HILL_SHADE)
    {
        dfDstNoDataValue = 0;
        bDstHasNoData = true;
        pData = GDALCreateHillshadeData(
            adfGeoTransform, psOptions->z, psOptions->xscale, psOptions->yscale,
            psOptions->alt, psOptions->az, psOptions->eGradientAlg);
        if (psOptions->eGradientAlg == GradientAlg::ZEVENBERGEN_THORNE)
        {
            if (psOptions->bCombined)
            {
                pfnAlgFloat =
                    GDALHillshadeCombinedAlg<float,
                                             GradientAlg::ZEVENBERGEN_THORNE>;
                pfnAlgInt32 =
                    GDALHillshadeCombinedAlg<GInt32,
                                             GradientAlg::ZEVENBERGEN_THORNE>;
            }
            else if (psOptions->bIgor)
            {
                pfnAlgFloat =
                    GDALHillshadeIgorAlg<float,
                                         GradientAlg::ZEVENBERGEN_THORNE>;
                pfnAlgInt32 =
                    GDALHillshadeIgorAlg<GInt32,
                                         GradientAlg::ZEVENBERGEN_THORNE>;
            }
            else
            {
                pfnAlgFloat =
                    GDALHillshadeAlg<float, GradientAlg::ZEVENBERGEN_THORNE>;
                pfnAlgInt32 =
                    GDALHillshadeAlg<GInt32, GradientAlg::ZEVENBERGEN_THORNE>;
            }
        }
        else
        {
            if (psOptions->bCombined)
            {
                pfnAlgFloat =
                    GDALHillshadeCombinedAlg<float, GradientAlg::HORN>;
                pfnAlgInt32 =
                    GDALHillshadeCombinedAlg<GInt32, GradientAlg::HORN>;
            }
            else if (psOptions->bIgor)
            {
                pfnAlgFloat = GDALHillshadeIgorAlg<float, GradientAlg::HORN>;
                pfnAlgInt32 = GDALHillshadeIgorAlg<GInt32, GradientAlg::HORN>;
            }
            else
            {
                if (adfGeoTransform[1] == -adfGeoTransform[5] &&
                    psOptions->xscale == psOptions->yscale)
                {
                    pfnAlgFloat = GDALHillshadeAlg_same_res<float>;
                    pfnAlgInt32 = GDALHillshadeAlg_same_res<GInt32>;
#if defined(HAVE_16_SSE_REG) && defined(__AVX2__)
                    pfnAlgFloat_multisample =
                        GDALHillshadeAlg_same_res_multisample<
                            float, XMMReg8Float, XMMReg8Float>;
                    pfnAlgInt32_multisample =
                        GDALHillshadeAlg_same_res_multisample<
                            GInt32, XMMReg8Int, XMMReg8Float>;
#elif defined(HAVE_16_SSE_REG)
                    pfnAlgFloat_multisample =
                        GDALHillshadeAlg_same_res_multisample<
                            float, XMMReg4Float, XMMReg4Float>;
                    pfnAlgInt32_multisample =
                        GDALHillshadeAlg_same_res_multisample<
                            GInt32, XMMReg4Int, XMMReg4Float>;
#endif
                }
                else
                {
                    pfnAlgFloat = GDALHillshadeAlg<float, GradientAlg::HORN>;
                    pfnAlgInt32 = GDALHillshadeAlg<GInt32, GradientAlg::HORN>;
                }
            }
        }
    }
    else if (eUtilityMode == SLOPE)
    {
        dfDstNoDataValue = -9999;
        bDstHasNoData = true;

        pData = GDALCreateSlopeData(adfGeoTransform, psOptions->xscale,
                                    psOptions->yscale,
                                    psOptions->bSlopeFormatUseDegrees);
        if (psOptions->eGradientAlg == GradientAlg::ZEVENBERGEN_THORNE)
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

    else if (eUtilityMode == ASPECT)
    {
        if (!psOptions->bZeroForFlat)
        {
            dfDstNoDataValue = -9999;
            bDstHasNoData = true;
        }

        pData = GDALCreateAspectData(psOptions->bAngleAsAzimuth);
        if (psOptions->eGradientAlg == GradientAlg::ZEVENBERGEN_THORNE)
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
    else if (eUtilityMode == TRI)
    {
        dfDstNoDataValue = -9999;
        bDstHasNoData = true;
        if (psOptions->eTRIAlg == TRIAlg::WILSON)
        {
            pfnAlgFloat = GDALTRIAlgWilson<float>;
            pfnAlgInt32 = GDALTRIAlgWilson<GInt32>;
        }
        else
        {
            pfnAlgFloat = GDALTRIAlgRiley<float>;
            pfnAlgInt32 = GDALTRIAlgRiley<GInt32>;
        }
    }
    else if (eUtilityMode == TPI)
    {
        dfDstNoDataValue = -9999;
        bDstHasNoData = true;
        pfnAlgFloat = GDALTPIAlg<float>;
        pfnAlgInt32 = GDALTPIAlg<GInt32>;
    }
    else if (eUtilityMode == ROUGHNESS)
    {
        dfDstNoDataValue = -9999;
        bDstHasNoData = true;
        pfnAlgFloat = GDALRoughnessAlg<float>;
        pfnAlgInt32 = GDALRoughnessAlg<GInt32>;
    }

    const GDALDataType eDstDataType =
        (eUtilityMode == HILL_SHADE || eUtilityMode == COLOR_RELIEF)
            ? GDT_UInt8
            : GDT_Float32;

    if (EQUAL(osFormat, "VRT"))
    {
        if (eUtilityMode == COLOR_RELIEF)
        {
            auto poDS = GDALGenerateVRTColorRelief(
                pszDest, hSrcDataset, hSrcBand, pszColorFilename,
                psOptions->eColorSelectionMode, psOptions->bAddAlpha);
            if (poDS && pszDest[0] != 0)
            {
                poDS.reset();
                poDS.reset(GDALDataset::Open(
                    pszDest,
                    GDAL_OF_UPDATE | GDAL_OF_VERBOSE_ERROR | GDAL_OF_RASTER));
            }
            return GDALDataset::ToHandle(poDS.release());
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "VRT driver can only be used with color-relief utility.");

            return nullptr;
        }
    }

    // We might actually want to always go through the intermediate dataset
    bool bForceUseIntermediateDataset = false;

    GDALProgressFunc pfnProgress = psOptions->pfnProgress;
    void *pProgressData = psOptions->pProgressData;

    if (EQUAL(osFormat, "GTiff"))
    {
        if (!EQUAL(psOptions->aosCreationOptions.FetchNameValueDef("COMPRESS",
                                                                   "NONE"),
                   "NONE") &&
            CPLTestBool(
                psOptions->aosCreationOptions.FetchNameValueDef("TILED", "NO")))
        {
            bForceUseIntermediateDataset = true;
        }
        else if (strcmp(pszDest, "/vsistdout/") == 0)
        {
            bForceUseIntermediateDataset = true;
            pfnProgress = GDALDummyProgress;
            pProgressData = nullptr;
        }
#ifdef S_ISFIFO
        else
        {
            VSIStatBufL sStat;
            if (VSIStatExL(pszDest, &sStat,
                           VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0 &&
                S_ISFIFO(sStat.st_mode))
            {
                bForceUseIntermediateDataset = true;
            }
        }
#endif
    }

    const GDALDataType eSrcDT = GDALGetRasterDataType(hSrcBand);

    if (hDriver == nullptr ||
        (GDALGetMetadataItem(hDriver, GDAL_DCAP_RASTER, nullptr) != nullptr &&
         ((bForceUseIntermediateDataset ||
           GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE, nullptr) ==
               nullptr) &&
          GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATECOPY, nullptr) !=
              nullptr)))
    {
        GDALDatasetH hIntermediateDataset = nullptr;

        if (eUtilityMode == COLOR_RELIEF)
        {
            auto poDS = std::make_unique<GDALColorReliefDataset>(
                hSrcDataset, hSrcBand, pszColorFilename,
                psOptions->eColorSelectionMode, psOptions->bAddAlpha);
            if (!(poDS->InitOK()))
            {
                return nullptr;
            }
            hIntermediateDataset = GDALDataset::ToHandle(poDS.release());
        }
        else
        {
            if (eSrcDT == GDT_UInt8 || eSrcDT == GDT_Int16 ||
                eSrcDT == GDT_UInt16)
            {
                auto poDS = std::make_unique<GDALGeneric3x3Dataset<GInt32>>(
                    hSrcDataset, hSrcBand, eDstDataType, bDstHasNoData,
                    dfDstNoDataValue, pfnAlgInt32, pfnAlgInt32_multisample,
                    std::move(pData), psOptions->bComputeAtEdges, true);

                if (!(poDS->InitOK()))
                {
                    return nullptr;
                }
                hIntermediateDataset = GDALDataset::ToHandle(poDS.release());
            }
            else
            {
                auto poDS = std::make_unique<GDALGeneric3x3Dataset<float>>(
                    hSrcDataset, hSrcBand, eDstDataType, bDstHasNoData,
                    dfDstNoDataValue, pfnAlgFloat, pfnAlgFloat_multisample,
                    std::move(pData), psOptions->bComputeAtEdges, true);

                if (!(poDS->InitOK()))
                {
                    return nullptr;
                }
                hIntermediateDataset = GDALDataset::ToHandle(poDS.release());
            }
        }

        if (!hDriver)
        {
            return hIntermediateDataset;
        }

        GDALDatasetH hOutDS = GDALCreateCopy(
            hDriver, pszDest, hIntermediateDataset, TRUE,
            psOptions->aosCreationOptions.List(), pfnProgress, pProgressData);

        GDALClose(hIntermediateDataset);

        return hOutDS;
    }

    const int nDstBands =
        eUtilityMode == COLOR_RELIEF ? ((psOptions->bAddAlpha) ? 4 : 3) : 1;

    GDALDatasetH hDstDataset =
        GDALCreate(hDriver, pszDest, nXSize, nYSize, nDstBands, eDstDataType,
                   psOptions->aosCreationOptions.List());

    if (hDstDataset == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unable to create dataset %s",
                 pszDest);
        return nullptr;
    }

    GDALRasterBandH hDstBand = GDALGetRasterBand(hDstDataset, 1);

    GDALSetGeoTransform(hDstDataset, adfGeoTransform);
    GDALSetProjection(hDstDataset, GDALGetProjectionRef(hSrcDataset));

    if (eUtilityMode == COLOR_RELIEF)
    {
        GDALColorRelief(hSrcBand, GDALGetRasterBand(hDstDataset, 1),
                        GDALGetRasterBand(hDstDataset, 2),
                        GDALGetRasterBand(hDstDataset, 3),
                        psOptions->bAddAlpha ? GDALGetRasterBand(hDstDataset, 4)
                                             : nullptr,
                        pszColorFilename, psOptions->eColorSelectionMode,
                        pfnProgress, pProgressData);
    }
    else
    {
        if (bDstHasNoData)
            GDALSetRasterNoDataValue(hDstBand, dfDstNoDataValue);

        if (eSrcDT == GDT_UInt8 || eSrcDT == GDT_Int16 || eSrcDT == GDT_UInt16)
        {
            GDALGeneric3x3Processing<GInt32>(
                hSrcBand, hDstBand, pfnAlgInt32, pfnAlgInt32_multisample,
                std::move(pData), psOptions->bComputeAtEdges, pfnProgress,
                pProgressData);
        }
        else
        {
            GDALGeneric3x3Processing<float>(
                hSrcBand, hDstBand, pfnAlgFloat, pfnAlgFloat_multisample,
                std::move(pData), psOptions->bComputeAtEdges, pfnProgress,
                pProgressData);
        }
    }

    return hDstDataset;
}

/************************************************************************/
/*                    GDALDEMProcessingOptionsNew()                     */
/************************************************************************/

/**
 * Allocates a GDALDEMProcessingOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/gdaldem.html">gdaldem</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be
 * NULL), otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALDEMProcessingOptionsForBinaryNew() prior to
 * this function. Will be filled with potentially present filename, open
 * options,...
 * @return pointer to the allocated GDALDEMProcessingOptions struct. Must be
 * freed with GDALDEMProcessingOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALDEMProcessingOptions *GDALDEMProcessingOptionsNew(
    char **papszArgv, GDALDEMProcessingOptionsForBinary *psOptionsForBinary)
{

    auto psOptions = std::make_unique<GDALDEMProcessingOptions>();
    /* -------------------------------------------------------------------- */
    /*      Handle command line arguments.                                  */
    /* -------------------------------------------------------------------- */
    CPLStringList aosArgv;

    if (papszArgv)
    {
        const int nArgc = CSLCount(papszArgv);
        for (int i = 0; i < nArgc; i++)
            aosArgv.AddString(papszArgv[i]);
    }

    // Ugly hack: papszArgv may not contain the processing mode if coming from SWIG
    const bool bNoProcessingMode(aosArgv.size() > 0 && aosArgv[0][0] == '-');

    auto argParser =
        GDALDEMAppOptionsGetParser(psOptions.get(), psOptionsForBinary);

    auto tryHandleArgv = [&](const CPLStringList &args)
    {
        argParser->parse_args_without_binary_name(args);
        // Validate the parsed options

        if (psOptions->nBand <= 0)
        {
            throw std::invalid_argument("Invalid value for -b");
        }

        if (psOptions->z <= 0)
        {
            throw std::invalid_argument("Invalid value for -z");
        }

        if (psOptions->globalScale <= 0)
        {
            throw std::invalid_argument("Invalid value for -s");
        }

        if (psOptions->xscale <= 0)
        {
            throw std::invalid_argument("Invalid value for -xscale");
        }

        if (psOptions->yscale <= 0)
        {
            throw std::invalid_argument("Invalid value for -yscale");
        }

        if (psOptions->alt <= 0)
        {
            throw std::invalid_argument("Invalid value for -alt");
        }

        if (psOptions->bMultiDirectional && argParser->is_used_globally("-az"))
        {
            throw std::invalid_argument(
                "-multidirectional and -az cannot be used together");
        }

        if (psOptions->bIgor && argParser->is_used_globally("-alt"))
        {
            throw std::invalid_argument(
                "-igor and -alt cannot be used together");
        }

        if (psOptionsForBinary && aosArgv.size() > 1)
        {
            psOptionsForBinary->osProcessing = aosArgv[0];
        }
    };

    try
    {

        // Ugly hack: papszArgv may not contain the processing mode if coming from
        // SWIG we have not other option than to check them all
        const static std::list<std::string> processingModes{
            {"hillshade", "slope", "aspect", "color-relief", "TRI", "TPI",
             "roughness"}};

        if (bNoProcessingMode)
        {
            try
            {
                tryHandleArgv(aosArgv);
            }
            catch (std::exception &)
            {
                bool bSuccess = false;
                for (const auto &processingMode : processingModes)
                {
                    CPLStringList aosArgvTmp{aosArgv};
                    CPL_IGNORE_RET_VAL(aosArgv);
                    aosArgvTmp.InsertString(0, processingMode.c_str());
                    try
                    {
                        tryHandleArgv(aosArgvTmp);
                        bSuccess = true;
                        break;
                    }
                    catch (std::runtime_error &)
                    {
                        continue;
                    }
                    catch (std::invalid_argument &)
                    {
                        continue;
                    }
                    catch (std::logic_error &)
                    {
                        continue;
                    }
                }

                if (!bSuccess)
                {
                    throw std::invalid_argument(
                        "Argument(s) are not valid with any processing mode.");
                }
            }
        }
        else
        {
            tryHandleArgv(aosArgv);
        }
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                 err.what());
        return nullptr;
    }

    if (!std::isnan(psOptions->globalScale))
    {
        if (!std::isnan(psOptions->xscale) || !std::isnan(psOptions->yscale))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "-scale and -xscale/-yscale are mutually exclusive.");
            return nullptr;
        }
        psOptions->xscale = psOptions->globalScale;
        psOptions->yscale = psOptions->globalScale;
    }
    else if ((!std::isnan(psOptions->xscale)) ^
             (!std::isnan(psOptions->yscale)))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "When one of -xscale or -yscale is specified, both must be "
                 "specified.");
        return nullptr;
    }

    return psOptions.release();
}

/************************************************************************/
/*                    GDALDEMProcessingOptionsFree()                    */
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
    delete psOptions;
}

/************************************************************************/
/*                GDALDEMProcessingOptionsSetProgress()                 */
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

void GDALDEMProcessingOptionsSetProgress(GDALDEMProcessingOptions *psOptions,
                                         GDALProgressFunc pfnProgress,
                                         void *pProgressData)
{
    psOptions->pfnProgress = pfnProgress;
    psOptions->pProgressData = pProgressData;
}
