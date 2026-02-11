/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implementation of a set of GDALDerivedPixelFunc(s) to be used
 *           with source raster band of virtual GDAL datasets.
 * Author:   Antonio Valentino <antonio.valentino@tiscali.it>
 *
 ******************************************************************************
 * Copyright (c) 2008-2014,2022 Antonio Valentino <antonio.valentino@tiscali.it>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#include <array>
#include <charconv>
#include <cmath>
#include "gdal.h"
#include "vrtdataset.h"
#include "vrtexpression.h"
#include "vrtreclassifier.h"
#include "cpl_float.h"

#include "geodesic.h"  // from PROJ

#if defined(__x86_64) || defined(_M_X64) || defined(USE_NEON_OPTIMIZATIONS)
#define USE_SSE2
#include "gdalsse_priv.h"

#if !defined(USE_NEON_OPTIMIZATIONS)
#define LIBDIVIDE_SSE2
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Weffc++"
#endif
#include "../../third_party/libdivide/libdivide.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#endif

#endif

#include "gdal_priv_templates.hpp"

#include <algorithm>
#include <cassert>
#include <limits>

namespace gdal
{
MathExpression::~MathExpression() = default;
}

template <typename T>
inline double GetSrcVal(const void *pSource, GDALDataType eSrcType, T ii)
{
    switch (eSrcType)
    {
        case GDT_Unknown:
            return 0;
        case GDT_UInt8:
            return static_cast<const GByte *>(pSource)[ii];
        case GDT_Int8:
            return static_cast<const GInt8 *>(pSource)[ii];
        case GDT_UInt16:
            return static_cast<const GUInt16 *>(pSource)[ii];
        case GDT_Int16:
            return static_cast<const GInt16 *>(pSource)[ii];
        case GDT_UInt32:
            return static_cast<const GUInt32 *>(pSource)[ii];
        case GDT_Int32:
            return static_cast<const GInt32 *>(pSource)[ii];
        // Precision loss currently for int64/uint64
        case GDT_UInt64:
            return static_cast<double>(
                static_cast<const uint64_t *>(pSource)[ii]);
        case GDT_Int64:
            return static_cast<double>(
                static_cast<const int64_t *>(pSource)[ii]);
        case GDT_Float16:
            return static_cast<const GFloat16 *>(pSource)[ii];
        case GDT_Float32:
            return static_cast<const float *>(pSource)[ii];
        case GDT_Float64:
            return static_cast<const double *>(pSource)[ii];
        case GDT_CInt16:
            return static_cast<const GInt16 *>(pSource)[2 * ii];
        case GDT_CInt32:
            return static_cast<const GInt32 *>(pSource)[2 * ii];
        case GDT_CFloat16:
            return static_cast<const GFloat16 *>(pSource)[2 * ii];
        case GDT_CFloat32:
            return static_cast<const float *>(pSource)[2 * ii];
        case GDT_CFloat64:
            return static_cast<const double *>(pSource)[2 * ii];
        case GDT_TypeCount:
            break;
    }
    return 0;
}

static bool IsNoData(double dfVal, double dfNoData)
{
    return dfVal == dfNoData || (std::isnan(dfVal) && std::isnan(dfNoData));
}

static CPLErr FetchDoubleArg(CSLConstList papszArgs, const char *pszName,
                             double *pdfX, double *pdfDefault = nullptr)
{
    const char *pszVal = CSLFetchNameValue(papszArgs, pszName);

    if (pszVal == nullptr)
    {
        if (pdfDefault == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing pixel function argument: %s", pszName);
            return CE_Failure;
        }
        else
        {
            *pdfX = *pdfDefault;
            return CE_None;
        }
    }

    char *pszEnd = nullptr;
    *pdfX = std::strtod(pszVal, &pszEnd);
    if (pszEnd == pszVal)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to parse pixel function argument: %s", pszName);
        return CE_Failure;
    }

    return CE_None;
}

static CPLErr FetchIntegerArg(CSLConstList papszArgs, const char *pszName,
                              int *pnX, int *pnDefault = nullptr)
{
    const char *pszVal = CSLFetchNameValue(papszArgs, pszName);

    if (pszVal == nullptr)
    {
        if (pnDefault == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing pixel function argument: %s", pszName);
            return CE_Failure;
        }
        else
        {
            *pnX = *pnDefault;
            return CE_None;
        }
    }

    char *pszEnd = nullptr;
    const auto ret = std::strtol(pszVal, &pszEnd, 10);
    while (std::isspace(*pszEnd))
    {
        pszEnd++;
    }
    if (*pszEnd != '\0')
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to parse pixel function argument: %s", pszName);
        return CE_Failure;
    }

    if (ret > std::numeric_limits<int>::max())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Pixel function argument %s is above the maximum value of %d",
                 pszName, std::numeric_limits<int>::max());
        return CE_Failure;
    }

    *pnX = static_cast<int>(ret);

    return CE_None;
}

static CPLErr RealPixelFunc(void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize, GDALDataType eSrcType,
                            GDALDataType eBufType, int nPixelSpace,
                            int nLineSpace)
{
    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;

    const int nPixelSpaceSrc = GDALGetDataTypeSizeBytes(eSrcType);
    const size_t nLineSpaceSrc = static_cast<size_t>(nPixelSpaceSrc) * nXSize;

    /* ---- Set pixels ---- */
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        GDALCopyWords(static_cast<GByte *>(papoSources[0]) +
                          nLineSpaceSrc * iLine,
                      eSrcType, nPixelSpaceSrc,
                      static_cast<GByte *>(pData) +
                          static_cast<GSpacing>(nLineSpace) * iLine,
                      eBufType, nPixelSpace, nXSize);
    }

    /* ---- Return success ---- */
    return CE_None;
}  // RealPixelFunc

static CPLErr ImagPixelFunc(void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize, GDALDataType eSrcType,
                            GDALDataType eBufType, int nPixelSpace,
                            int nLineSpace)
{
    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;

    if (GDALDataTypeIsComplex(eSrcType))
    {
        const GDALDataType eSrcBaseType = GDALGetNonComplexDataType(eSrcType);
        const int nPixelSpaceSrc = GDALGetDataTypeSizeBytes(eSrcType);
        const size_t nLineSpaceSrc =
            static_cast<size_t>(nPixelSpaceSrc) * nXSize;

        const void *const pImag = static_cast<GByte *>(papoSources[0]) +
                                  GDALGetDataTypeSizeBytes(eSrcType) / 2;

        /* ---- Set pixels ---- */
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            GDALCopyWords(static_cast<const GByte *>(pImag) +
                              nLineSpaceSrc * iLine,
                          eSrcBaseType, nPixelSpaceSrc,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine,
                          eBufType, nPixelSpace, nXSize);
        }
    }
    else
    {
        const double dfImag = 0;

        /* ---- Set pixels ---- */
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            // Always copy from the same location.
            GDALCopyWords(&dfImag, eSrcType, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine,
                          eBufType, nPixelSpace, nXSize);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // ImagPixelFunc

static CPLErr ComplexPixelFunc(void **papoSources, int nSources, void *pData,
                               int nXSize, int nYSize, GDALDataType eSrcType,
                               GDALDataType eBufType, int nPixelSpace,
                               int nLineSpace)
{
    /* ---- Init ---- */
    if (nSources != 2)
        return CE_Failure;

    const void *const pReal = papoSources[0];
    const void *const pImag = papoSources[1];

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            // Source raster pixels may be obtained with GetSrcVal macro.
            const double adfPixVal[2] = {
                GetSrcVal(pReal, eSrcType, ii),  // re
                GetSrcVal(pImag, eSrcType, ii)   // im
            };

            GDALCopyWords(adfPixVal, GDT_CFloat64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // ComplexPixelFunc

typedef enum
{
    GAT_amplitude,
    GAT_intensity,
    GAT_dB
} PolarAmplitudeType;

static const char pszPolarPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='amplitude_type' description='Amplitude Type' "
    "type='string-select' default='AMPLITUDE'>"
    "       <Value>INTENSITY</Value>"
    "       <Value>dB</Value>"
    "       <Value>AMPLITUDE</Value>"
    "   </Argument>"
    "</PixelFunctionArgumentsList>";

static CPLErr PolarPixelFunc(void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize, GDALDataType eSrcType,
                             GDALDataType eBufType, int nPixelSpace,
                             int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (nSources != 2)
        return CE_Failure;

    const char pszName[] = "amplitude_type";
    const char *pszVal = CSLFetchNameValue(papszArgs, pszName);
    PolarAmplitudeType amplitudeType = GAT_amplitude;
    if (pszVal != nullptr)
    {
        if (strcmp(pszVal, "INTENSITY") == 0)
            amplitudeType = GAT_intensity;
        else if (strcmp(pszVal, "dB") == 0)
            amplitudeType = GAT_dB;
        else if (strcmp(pszVal, "AMPLITUDE") != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid value for pixel function argument '%s': %s",
                     pszName, pszVal);
            return CE_Failure;
        }
    }

    const void *const pAmp = papoSources[0];
    const void *const pPhase = papoSources[1];

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            // Source raster pixels may be obtained with GetSrcVal macro.
            double dfAmp = GetSrcVal(pAmp, eSrcType, ii);
            switch (amplitudeType)
            {
                case GAT_intensity:
                    // clip to zero
                    dfAmp = dfAmp <= 0 ? 0 : std::sqrt(dfAmp);
                    break;
                case GAT_dB:
                    dfAmp = dfAmp <= 0
                                ? -std::numeric_limits<double>::infinity()
                                : pow(10, dfAmp / 20.);
                    break;
                case GAT_amplitude:
                    break;
            }
            const double dfPhase = GetSrcVal(pPhase, eSrcType, ii);
            const double adfPixVal[2] = {
                dfAmp * std::cos(dfPhase),  // re
                dfAmp * std::sin(dfPhase)   // im
            };

            GDALCopyWords(adfPixVal, GDT_CFloat64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // PolarPixelFunc

static CPLErr ModulePixelFunc(void **papoSources, int nSources, void *pData,
                              int nXSize, int nYSize, GDALDataType eSrcType,
                              GDALDataType eBufType, int nPixelSpace,
                              int nLineSpace)
{
    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;

    if (GDALDataTypeIsComplex(eSrcType))
    {
        const void *pReal = papoSources[0];
        const void *pImag = static_cast<GByte *>(papoSources[0]) +
                            GDALGetDataTypeSizeBytes(eSrcType) / 2;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal = GetSrcVal(pReal, eSrcType, ii);
                const double dfImag = GetSrcVal(pImag, eSrcType, ii);

                const double dfPixVal = sqrt(dfReal * dfReal + dfImag * dfImag);

                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfPixVal =
                    fabs(GetSrcVal(papoSources[0], eSrcType, ii));

                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // ModulePixelFunc

static CPLErr PhasePixelFunc(void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize, GDALDataType eSrcType,
                             GDALDataType eBufType, int nPixelSpace,
                             int nLineSpace)
{
    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;

    if (GDALDataTypeIsComplex(eSrcType))
    {
        const void *const pReal = papoSources[0];
        const void *const pImag = static_cast<GByte *>(papoSources[0]) +
                                  GDALGetDataTypeSizeBytes(eSrcType) / 2;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal = GetSrcVal(pReal, eSrcType, ii);
                const double dfImag = GetSrcVal(pImag, eSrcType, ii);

                const double dfPixVal = atan2(dfImag, dfReal);

                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }
    else if (GDALDataTypeIsInteger(eSrcType) && !GDALDataTypeIsSigned(eSrcType))
    {
        constexpr double dfZero = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            GDALCopyWords(&dfZero, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine,
                          eBufType, nPixelSpace, nXSize);
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                const void *const pReal = papoSources[0];

                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal = GetSrcVal(pReal, eSrcType, ii);
                const double dfPixVal = (dfReal < 0) ? M_PI : 0.0;

                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // PhasePixelFunc

static CPLErr ConjPixelFunc(void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize, GDALDataType eSrcType,
                            GDALDataType eBufType, int nPixelSpace,
                            int nLineSpace)
{
    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;

    if (GDALDataTypeIsComplex(eSrcType) && GDALDataTypeIsComplex(eBufType))
    {
        const int nOffset = GDALGetDataTypeSizeBytes(eSrcType) / 2;
        const void *const pReal = papoSources[0];
        const void *const pImag =
            static_cast<GByte *>(papoSources[0]) + nOffset;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double adfPixVal[2] = {
                    +GetSrcVal(pReal, eSrcType, ii),  // re
                    -GetSrcVal(pImag, eSrcType, ii)   // im
                };

                GDALCopyWords(adfPixVal, GDT_CFloat64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        // No complex data type.
        return RealPixelFunc(papoSources, nSources, pData, nXSize, nYSize,
                             eSrcType, eBufType, nPixelSpace, nLineSpace);
    }

    /* ---- Return success ---- */
    return CE_None;
}  // ConjPixelFunc

static constexpr char pszRoundPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='digits' description='Digits' type='integer' "
    "default='0' />"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "</PixelFunctionArgumentsList>";

static CPLErr RoundPixelFunc(void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize, GDALDataType eSrcType,
                             GDALDataType eBufType, int nPixelSpace,
                             int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (nSources != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "round: input must be a single band");
        return CE_Failure;
    }

    if (GDALDataTypeIsComplex(eSrcType))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "round: complex data types not supported");
        return CE_Failure;
    }

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    int nDigits{0};
    if (FetchIntegerArg(papszArgs, "digits", &nDigits, &nDigits) != CE_None)
        return CE_Failure;

    const double dfScaleVal = std::pow(10, nDigits);
    const double dfInvScaleVal = 1. / dfScaleVal;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            // Source raster pixels may be obtained with GetSrcVal macro.
            const double dfSrcVal = GetSrcVal(papoSources[0], eSrcType, ii);

            const double dfDstVal =
                bHasNoData && IsNoData(dfSrcVal, dfNoData)
                    ? dfNoData
                    : std::round(dfSrcVal * dfScaleVal) * dfInvScaleVal;

            GDALCopyWords(&dfDstVal, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // RoundPixelFunc

#ifdef USE_SSE2

/************************************************************************/
/*                      OptimizedSumToFloat_SSE2()                      */
/************************************************************************/

template <typename Tsrc>
static void OptimizedSumToFloat_SSE2(double dfK, void *pOutBuffer,
                                     int nLineSpace, int nXSize, int nYSize,
                                     int nSources,
                                     const void *const *papoSources)
{
    const XMMReg4Float cst = XMMReg4Float::Set1(static_cast<float>(dfK));

    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        float *CPL_RESTRICT const pDest = reinterpret_cast<float *>(
            static_cast<GByte *>(pOutBuffer) +
            static_cast<GSpacing>(nLineSpace) * iLine);
        const size_t iOffsetLine = static_cast<size_t>(iLine) * nXSize;

        constexpr int VALUES_PER_REG = 4;
        constexpr int UNROLLING = 4 * VALUES_PER_REG;
        int iCol = 0;
        for (; iCol < nXSize - (UNROLLING - 1); iCol += UNROLLING)
        {
            XMMReg4Float d0(cst);
            XMMReg4Float d1(cst);
            XMMReg4Float d2(cst);
            XMMReg4Float d3(cst);
            for (int iSrc = 0; iSrc < nSources; ++iSrc)
            {
                XMMReg4Float t0, t1, t2, t3;
                XMMReg4Float::Load16Val(
                    static_cast<const Tsrc * CPL_RESTRICT>(papoSources[iSrc]) +
                        iOffsetLine + iCol,
                    t0, t1, t2, t3);
                d0 += t0;
                d1 += t1;
                d2 += t2;
                d3 += t3;
            }
            d0.Store4Val(pDest + iCol + VALUES_PER_REG * 0);
            d1.Store4Val(pDest + iCol + VALUES_PER_REG * 1);
            d2.Store4Val(pDest + iCol + VALUES_PER_REG * 2);
            d3.Store4Val(pDest + iCol + VALUES_PER_REG * 3);
        }

        for (; iCol < nXSize; iCol++)
        {
            float d = static_cast<float>(dfK);
            for (int iSrc = 0; iSrc < nSources; ++iSrc)
            {
                d += static_cast<const Tsrc * CPL_RESTRICT>(
                    papoSources[iSrc])[iOffsetLine + iCol];
            }
            pDest[iCol] = d;
        }
    }
}

/************************************************************************/
/*                     OptimizedSumToDouble_SSE2()                      */
/************************************************************************/

template <typename Tsrc>
static void OptimizedSumToDouble_SSE2(double dfK, void *pOutBuffer,
                                      int nLineSpace, int nXSize, int nYSize,
                                      int nSources,
                                      const void *const *papoSources)
{
    const XMMReg4Double cst = XMMReg4Double::Set1(dfK);

    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        double *CPL_RESTRICT const pDest = reinterpret_cast<double *>(
            static_cast<GByte *>(pOutBuffer) +
            static_cast<GSpacing>(nLineSpace) * iLine);
        const size_t iOffsetLine = static_cast<size_t>(iLine) * nXSize;

        constexpr int VALUES_PER_REG = 4;
        constexpr int UNROLLING = 2 * VALUES_PER_REG;
        int iCol = 0;
        for (; iCol < nXSize - (UNROLLING - 1); iCol += UNROLLING)
        {
            XMMReg4Double d0(cst);
            XMMReg4Double d1(cst);
            for (int iSrc = 0; iSrc < nSources; ++iSrc)
            {
                XMMReg4Double t0, t1;
                XMMReg4Double::Load8Val(
                    static_cast<const Tsrc * CPL_RESTRICT>(papoSources[iSrc]) +
                        iOffsetLine + iCol,
                    t0, t1);
                d0 += t0;
                d1 += t1;
            }
            d0.Store4Val(pDest + iCol + VALUES_PER_REG * 0);
            d1.Store4Val(pDest + iCol + VALUES_PER_REG * 1);
        }

        for (; iCol < nXSize; iCol++)
        {
            double d = dfK;
            for (int iSrc = 0; iSrc < nSources; ++iSrc)
            {
                d += static_cast<const Tsrc * CPL_RESTRICT>(
                    papoSources[iSrc])[iOffsetLine + iCol];
            }
            pDest[iCol] = d;
        }
    }
}

/************************************************************************/
/*                     OptimizedSumSameType_SSE2()                      */
/************************************************************************/

template <typename T, typename Tsigned, typename Tacc, class SSEWrapper>
static void OptimizedSumSameType_SSE2(double dfK, void *pOutBuffer,
                                      int nLineSpace, int nXSize, int nYSize,
                                      int nSources,
                                      const void *const *papoSources)
{
    static_assert(std::numeric_limits<T>::is_integer);
    static_assert(!std::numeric_limits<T>::is_signed);
    static_assert(std::numeric_limits<Tsigned>::is_integer);
    static_assert(std::numeric_limits<Tsigned>::is_signed);
    static_assert(sizeof(T) == sizeof(Tsigned));
    const T nK = static_cast<T>(dfK);
    Tsigned nKSigned;
    memcpy(&nKSigned, &nK, sizeof(T));
    const __m128i valInit = SSEWrapper::Set1(nKSigned);
    constexpr int VALUES_PER_REG =
        static_cast<int>(sizeof(valInit) / sizeof(T));
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        T *CPL_RESTRICT const pDest =
            reinterpret_cast<T *>(static_cast<GByte *>(pOutBuffer) +
                                  static_cast<GSpacing>(nLineSpace) * iLine);
        const size_t iOffsetLine = static_cast<size_t>(iLine) * nXSize;
        int iCol = 0;
        for (; iCol < nXSize - (4 * VALUES_PER_REG - 1);
             iCol += 4 * VALUES_PER_REG)
        {
            __m128i reg0 = valInit;
            __m128i reg1 = valInit;
            __m128i reg2 = valInit;
            __m128i reg3 = valInit;
            for (int iSrc = 0; iSrc < nSources; ++iSrc)
            {
                reg0 = SSEWrapper::AddSaturate(
                    reg0,
                    _mm_loadu_si128(reinterpret_cast<const __m128i *>(
                        static_cast<const T * CPL_RESTRICT>(papoSources[iSrc]) +
                        iOffsetLine + iCol)));
                reg1 = SSEWrapper::AddSaturate(
                    reg1,
                    _mm_loadu_si128(reinterpret_cast<const __m128i *>(
                        static_cast<const T * CPL_RESTRICT>(papoSources[iSrc]) +
                        iOffsetLine + iCol + VALUES_PER_REG)));
                reg2 = SSEWrapper::AddSaturate(
                    reg2,
                    _mm_loadu_si128(reinterpret_cast<const __m128i *>(
                        static_cast<const T * CPL_RESTRICT>(papoSources[iSrc]) +
                        iOffsetLine + iCol + 2 * VALUES_PER_REG)));
                reg3 = SSEWrapper::AddSaturate(
                    reg3,
                    _mm_loadu_si128(reinterpret_cast<const __m128i *>(
                        static_cast<const T * CPL_RESTRICT>(papoSources[iSrc]) +
                        iOffsetLine + iCol + 3 * VALUES_PER_REG)));
            }
            _mm_storeu_si128(reinterpret_cast<__m128i *>(pDest + iCol), reg0);
            _mm_storeu_si128(
                reinterpret_cast<__m128i *>(pDest + iCol + VALUES_PER_REG),
                reg1);
            _mm_storeu_si128(
                reinterpret_cast<__m128i *>(pDest + iCol + 2 * VALUES_PER_REG),
                reg2);
            _mm_storeu_si128(
                reinterpret_cast<__m128i *>(pDest + iCol + 3 * VALUES_PER_REG),
                reg3);
        }
        for (; iCol < nXSize; ++iCol)
        {
            Tacc nAcc = nK;
            for (int iSrc = 0; iSrc < nSources; ++iSrc)
            {
                nAcc = std::min<Tacc>(
                    nAcc + static_cast<const T * CPL_RESTRICT>(
                               papoSources[iSrc])[iOffsetLine + iCol],
                    std::numeric_limits<T>::max());
            }
            pDest[iCol] = static_cast<T>(nAcc);
        }
    }
}
#endif  // USE_SSE2

/************************************************************************/
/*                      OptimizedSumPackedOutput()                      */
/************************************************************************/

template <typename Tsrc, typename Tdest>
static void OptimizedSumPackedOutput(double dfK, void *pOutBuffer,
                                     int nLineSpace, int nXSize, int nYSize,
                                     int nSources,
                                     const void *const *papoSources)
{
#ifdef USE_SSE2
    if constexpr (std::is_same_v<Tdest, float> && !std::is_same_v<Tsrc, double>)
    {
        OptimizedSumToFloat_SSE2<Tsrc>(dfK, pOutBuffer, nLineSpace, nXSize,
                                       nYSize, nSources, papoSources);
    }
    else if constexpr (std::is_same_v<Tdest, double>)
    {
        OptimizedSumToDouble_SSE2<Tsrc>(dfK, pOutBuffer, nLineSpace, nXSize,
                                        nYSize, nSources, papoSources);
    }
    else
#endif  // USE_SSE2
    {
        const Tdest nCst = static_cast<Tdest>(dfK);
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            Tdest *CPL_RESTRICT const pDest = reinterpret_cast<Tdest *>(
                static_cast<GByte *>(pOutBuffer) +
                static_cast<GSpacing>(nLineSpace) * iLine);
            const size_t iOffsetLine = static_cast<size_t>(iLine) * nXSize;

#define LOAD_SRCVAL(iSrc_, j_)                                                 \
    static_cast<Tdest>(static_cast<const Tsrc * CPL_RESTRICT>(                 \
        papoSources[(iSrc_)])[iOffsetLine + iCol + (j_)])

            constexpr int UNROLLING = 4;
            int iCol = 0;
            for (; iCol < nXSize - (UNROLLING - 1); iCol += UNROLLING)
            {
                Tdest d[4] = {nCst, nCst, nCst, nCst};
                for (int iSrc = 0; iSrc < nSources; ++iSrc)
                {
                    d[0] += LOAD_SRCVAL(iSrc, 0);
                    d[1] += LOAD_SRCVAL(iSrc, 1);
                    d[2] += LOAD_SRCVAL(iSrc, 2);
                    d[3] += LOAD_SRCVAL(iSrc, 3);
                }
                pDest[iCol + 0] = d[0];
                pDest[iCol + 1] = d[1];
                pDest[iCol + 2] = d[2];
                pDest[iCol + 3] = d[3];
            }
            for (; iCol < nXSize; iCol++)
            {
                Tdest d0 = nCst;
                for (int iSrc = 0; iSrc < nSources; ++iSrc)
                {
                    d0 += LOAD_SRCVAL(iSrc, 0);
                }
                pDest[iCol] = d0;
            }
#undef LOAD_SRCVAL
        }
    }
}

/************************************************************************/
/*                      OptimizedSumPackedOutput()                      */
/************************************************************************/

template <typename Tdest>
static bool OptimizedSumPackedOutput(GDALDataType eSrcType, double dfK,
                                     void *pOutBuffer, int nLineSpace,
                                     int nXSize, int nYSize, int nSources,
                                     const void *const *papoSources)
{
    switch (eSrcType)
    {
        case GDT_UInt8:
            OptimizedSumPackedOutput<uint8_t, Tdest>(dfK, pOutBuffer,
                                                     nLineSpace, nXSize, nYSize,
                                                     nSources, papoSources);
            return true;

        case GDT_UInt16:
            OptimizedSumPackedOutput<uint16_t, Tdest>(
                dfK, pOutBuffer, nLineSpace, nXSize, nYSize, nSources,
                papoSources);
            return true;

        case GDT_Int16:
            OptimizedSumPackedOutput<int16_t, Tdest>(dfK, pOutBuffer,
                                                     nLineSpace, nXSize, nYSize,
                                                     nSources, papoSources);
            return true;

        case GDT_Int32:
            OptimizedSumPackedOutput<int32_t, Tdest>(dfK, pOutBuffer,
                                                     nLineSpace, nXSize, nYSize,
                                                     nSources, papoSources);
            return true;

        case GDT_Float32:
            OptimizedSumPackedOutput<float, Tdest>(dfK, pOutBuffer, nLineSpace,
                                                   nXSize, nYSize, nSources,
                                                   papoSources);
            return true;

        case GDT_Float64:
            OptimizedSumPackedOutput<double, Tdest>(dfK, pOutBuffer, nLineSpace,
                                                    nXSize, nYSize, nSources,
                                                    papoSources);
            return true;

        default:
            break;
    }
    return false;
}

/************************************************************************/
/*                   OptimizedSumThroughLargerType()                    */
/************************************************************************/

namespace
{
template <typename Tsrc, typename Tdest, typename Enable = void>
struct TintermediateS
{
    using type = double;
};

template <typename Tsrc, typename Tdest>
struct TintermediateS<
    Tsrc, Tdest,
    std::enable_if_t<
        (std::is_same_v<Tsrc, uint8_t> || std::is_same_v<Tsrc, int16_t> ||
         std::is_same_v<Tsrc, uint16_t>) &&
            (std::is_same_v<Tdest, uint8_t> || std::is_same_v<Tdest, int16_t> ||
             std::is_same_v<Tdest, uint16_t>),
        bool>>
{
    using type = int32_t;
};

}  // namespace

template <typename Tsrc, typename Tdest>
static bool OptimizedSumThroughLargerType(double dfK, void *pOutBuffer,
                                          int nPixelSpace, int nLineSpace,
                                          int nXSize, int nYSize, int nSources,
                                          const void *const *papoSources)
{
    using Tintermediate = typename TintermediateS<Tsrc, Tdest>::type;
    const Tintermediate k = static_cast<Tintermediate>(dfK);

    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        GByte *CPL_RESTRICT pDest = static_cast<GByte *>(pOutBuffer) +
                                    static_cast<GSpacing>(nLineSpace) * iLine;

        constexpr int UNROLLING = 4;
        int iCol = 0;
        for (; iCol < nXSize - (UNROLLING - 1);
             iCol += UNROLLING, ii += UNROLLING)
        {
            Tintermediate aSum[4] = {k, k, k, k};

            for (int iSrc = 0; iSrc < nSources; ++iSrc)
            {
                aSum[0] += static_cast<const Tsrc *>(papoSources[iSrc])[ii + 0];
                aSum[1] += static_cast<const Tsrc *>(papoSources[iSrc])[ii + 1];
                aSum[2] += static_cast<const Tsrc *>(papoSources[iSrc])[ii + 2];
                aSum[3] += static_cast<const Tsrc *>(papoSources[iSrc])[ii + 3];
            }

            GDALCopyWord(aSum[0], *reinterpret_cast<Tdest *>(pDest));
            pDest += nPixelSpace;
            GDALCopyWord(aSum[1], *reinterpret_cast<Tdest *>(pDest));
            pDest += nPixelSpace;
            GDALCopyWord(aSum[2], *reinterpret_cast<Tdest *>(pDest));
            pDest += nPixelSpace;
            GDALCopyWord(aSum[3], *reinterpret_cast<Tdest *>(pDest));
            pDest += nPixelSpace;
        }
        for (; iCol < nXSize; ++iCol, ++ii, pDest += nPixelSpace)
        {
            Tintermediate sum = k;
            for (int iSrc = 0; iSrc < nSources; ++iSrc)
            {
                sum += static_cast<const Tsrc *>(papoSources[iSrc])[ii];
            }

            auto pDst = reinterpret_cast<Tdest *>(pDest);
            GDALCopyWord(sum, *pDst);
        }
    }
    return true;
}

/************************************************************************/
/*                   OptimizedSumThroughLargerType()                    */
/************************************************************************/

template <typename Tsrc>
static bool OptimizedSumThroughLargerType(GDALDataType eBufType, double dfK,
                                          void *pOutBuffer, int nPixelSpace,
                                          int nLineSpace, int nXSize,
                                          int nYSize, int nSources,
                                          const void *const *papoSources)
{
    switch (eBufType)
    {
        case GDT_UInt8:
            return OptimizedSumThroughLargerType<Tsrc, uint8_t>(
                dfK, pOutBuffer, nPixelSpace, nLineSpace, nXSize, nYSize,
                nSources, papoSources);

        case GDT_UInt16:
            return OptimizedSumThroughLargerType<Tsrc, uint16_t>(
                dfK, pOutBuffer, nPixelSpace, nLineSpace, nXSize, nYSize,
                nSources, papoSources);

        case GDT_Int16:
            return OptimizedSumThroughLargerType<Tsrc, int16_t>(
                dfK, pOutBuffer, nPixelSpace, nLineSpace, nXSize, nYSize,
                nSources, papoSources);

        case GDT_Int32:
            return OptimizedSumThroughLargerType<Tsrc, int32_t>(
                dfK, pOutBuffer, nPixelSpace, nLineSpace, nXSize, nYSize,
                nSources, papoSources);

        // Float32 and Float64 already covered by OptimizedSum() for packed case
        default:
            break;
    }
    return false;
}

/************************************************************************/
/*                   OptimizedSumThroughLargerType()                    */
/************************************************************************/

static bool OptimizedSumThroughLargerType(GDALDataType eSrcType,
                                          GDALDataType eBufType, double dfK,
                                          void *pOutBuffer, int nPixelSpace,
                                          int nLineSpace, int nXSize,
                                          int nYSize, int nSources,
                                          const void *const *papoSources)
{
    switch (eSrcType)
    {
        case GDT_UInt8:
            return OptimizedSumThroughLargerType<uint8_t>(
                eBufType, dfK, pOutBuffer, nPixelSpace, nLineSpace, nXSize,
                nYSize, nSources, papoSources);

        case GDT_UInt16:
            return OptimizedSumThroughLargerType<uint16_t>(
                eBufType, dfK, pOutBuffer, nPixelSpace, nLineSpace, nXSize,
                nYSize, nSources, papoSources);

        case GDT_Int16:
            return OptimizedSumThroughLargerType<int16_t>(
                eBufType, dfK, pOutBuffer, nPixelSpace, nLineSpace, nXSize,
                nYSize, nSources, papoSources);

        case GDT_Int32:
            return OptimizedSumThroughLargerType<int32_t>(
                eBufType, dfK, pOutBuffer, nPixelSpace, nLineSpace, nXSize,
                nYSize, nSources, papoSources);

        case GDT_Float32:
            return OptimizedSumThroughLargerType<float>(
                eBufType, dfK, pOutBuffer, nPixelSpace, nLineSpace, nXSize,
                nYSize, nSources, papoSources);

        case GDT_Float64:
            return OptimizedSumThroughLargerType<double>(
                eBufType, dfK, pOutBuffer, nPixelSpace, nLineSpace, nXSize,
                nYSize, nSources, papoSources);

        default:
            break;
    }

    return false;
}

/************************************************************************/
/*                            SumPixelFunc()                            */
/************************************************************************/

static const char pszSumPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='k' description='Optional constant term' type='double' "
    "default='0.0' />"
    "   <Argument name='propagateNoData' description='Whether the output value "
    "should be NoData as as soon as one source is NoData' type='boolean' "
    "default='false' />"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "</PixelFunctionArgumentsList>";

static CPLErr SumPixelFunc(void **papoSources, int nSources, void *pData,
                           int nXSize, int nYSize, GDALDataType eSrcType,
                           GDALDataType eBufType, int nPixelSpace,
                           int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (nSources < 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "sum requires at least one source");
        return CE_Failure;
    }

    double dfK = 0.0;
    if (FetchDoubleArg(papszArgs, "k", &dfK, &dfK) != CE_None)
        return CE_Failure;

    double dfNoData{0};
    bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    const bool bPropagateNoData = CPLTestBool(
        CSLFetchNameValueDef(papszArgs, "propagateNoData", "false"));

    if (dfNoData == 0 && !bPropagateNoData)
        bHasNoData = false;

    /* ---- Set pixels ---- */
    if (GDALDataTypeIsComplex(eSrcType))
    {
        const int nOffset = GDALGetDataTypeSizeBytes(eSrcType) / 2;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                double adfSum[2] = {dfK, 0.0};

                for (int iSrc = 0; iSrc < nSources; ++iSrc)
                {
                    const void *const pReal = papoSources[iSrc];
                    const void *const pImag =
                        static_cast<const GByte *>(pReal) + nOffset;

                    // Source raster pixels may be obtained with GetSrcVal
                    // macro.
                    adfSum[0] += GetSrcVal(pReal, eSrcType, ii);
                    adfSum[1] += GetSrcVal(pImag, eSrcType, ii);
                }

                GDALCopyWords(adfSum, GDT_CFloat64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        bool bGeneralCase = true;
        if (dfNoData == 0 && !bPropagateNoData)
        {
#ifdef USE_SSE2
            if (eBufType == GDT_UInt8 && nPixelSpace == sizeof(uint8_t) &&
                eSrcType == GDT_UInt8 &&
                dfK >= std::numeric_limits<uint8_t>::min() &&
                dfK <= std::numeric_limits<uint8_t>::max() &&
                static_cast<int>(dfK) == dfK)
            {
                bGeneralCase = false;

                struct SSEWrapper
                {
                    inline static __m128i Set1(int8_t x)
                    {
                        return _mm_set1_epi8(x);
                    }

                    inline static __m128i AddSaturate(__m128i x, __m128i y)
                    {
                        return _mm_adds_epu8(x, y);
                    }
                };

                OptimizedSumSameType_SSE2<uint8_t, int8_t, uint32_t,
                                          SSEWrapper>(dfK, pData, nLineSpace,
                                                      nXSize, nYSize, nSources,
                                                      papoSources);
            }
            else if (eBufType == GDT_UInt16 &&
                     nPixelSpace == sizeof(uint16_t) &&
                     eSrcType == GDT_UInt16 &&
                     dfK >= std::numeric_limits<uint16_t>::min() &&
                     dfK <= std::numeric_limits<uint16_t>::max() &&
                     static_cast<int>(dfK) == dfK)
            {
                bGeneralCase = false;

                struct SSEWrapper
                {
                    inline static __m128i Set1(int16_t x)
                    {
                        return _mm_set1_epi16(x);
                    }

                    inline static __m128i AddSaturate(__m128i x, __m128i y)
                    {
                        return _mm_adds_epu16(x, y);
                    }
                };

                OptimizedSumSameType_SSE2<uint16_t, int16_t, uint32_t,
                                          SSEWrapper>(dfK, pData, nLineSpace,
                                                      nXSize, nYSize, nSources,
                                                      papoSources);
            }
            else
#endif
                if (eBufType == GDT_Float32 && nPixelSpace == sizeof(float))
            {
                bGeneralCase = !OptimizedSumPackedOutput<float>(
                    eSrcType, dfK, pData, nLineSpace, nXSize, nYSize, nSources,
                    papoSources);
            }
            else if (eBufType == GDT_Float64 && nPixelSpace == sizeof(double))
            {
                bGeneralCase = !OptimizedSumPackedOutput<double>(
                    eSrcType, dfK, pData, nLineSpace, nXSize, nYSize, nSources,
                    papoSources);
            }
            else if (
                dfK >= 0 && dfK <= INT_MAX && eBufType == GDT_Int32 &&
                nPixelSpace == sizeof(int32_t) && eSrcType == GDT_UInt8 &&
                // Limitation to avoid overflow of int32 if all source values are at the max of their data type
                nSources <=
                    (INT_MAX - dfK) / std::numeric_limits<uint8_t>::max())
            {
                bGeneralCase = false;
                OptimizedSumPackedOutput<uint8_t, int32_t>(
                    dfK, pData, nLineSpace, nXSize, nYSize, nSources,
                    papoSources);
            }

            if (bGeneralCase && dfK >= 0 && dfK <= INT_MAX &&
                nSources <=
                    (INT_MAX - dfK) / std::numeric_limits<uint16_t>::max())
            {
                bGeneralCase = !OptimizedSumThroughLargerType(
                    eSrcType, eBufType, dfK, pData, nPixelSpace, nLineSpace,
                    nXSize, nYSize, nSources, papoSources);
            }
        }

        if (bGeneralCase)
        {
            size_t ii = 0;
            for (int iLine = 0; iLine < nYSize; ++iLine)
            {
                for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
                {
                    double dfSum = dfK;
                    for (int iSrc = 0; iSrc < nSources; ++iSrc)
                    {
                        const double dfVal =
                            GetSrcVal(papoSources[iSrc], eSrcType, ii);

                        if (bHasNoData && IsNoData(dfVal, dfNoData))
                        {
                            if (bPropagateNoData)
                            {
                                dfSum = dfNoData;
                                break;
                            }
                        }
                        else
                        {
                            dfSum += dfVal;
                        }
                    }

                    GDALCopyWords(&dfSum, GDT_Float64, 0,
                                  static_cast<GByte *>(pData) +
                                      static_cast<GSpacing>(nLineSpace) *
                                          iLine +
                                      iCol * nPixelSpace,
                                  eBufType, nPixelSpace, 1);
                }
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
} /* SumPixelFunc */

static const char pszDiffPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "</PixelFunctionArgumentsList>";

static CPLErr DiffPixelFunc(void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize, GDALDataType eSrcType,
                            GDALDataType eBufType, int nPixelSpace,
                            int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (nSources != 2)
        return CE_Failure;

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    if (GDALDataTypeIsComplex(eSrcType))
    {
        const int nOffset = GDALGetDataTypeSizeBytes(eSrcType) / 2;
        const void *const pReal0 = papoSources[0];
        const void *const pImag0 =
            static_cast<GByte *>(papoSources[0]) + nOffset;
        const void *const pReal1 = papoSources[1];
        const void *const pImag1 =
            static_cast<GByte *>(papoSources[1]) + nOffset;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                double adfPixVal[2] = {GetSrcVal(pReal0, eSrcType, ii) -
                                           GetSrcVal(pReal1, eSrcType, ii),
                                       GetSrcVal(pImag0, eSrcType, ii) -
                                           GetSrcVal(pImag1, eSrcType, ii)};

                GDALCopyWords(adfPixVal, GDT_CFloat64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                const double dfA = GetSrcVal(papoSources[0], eSrcType, ii);
                const double dfB = GetSrcVal(papoSources[1], eSrcType, ii);

                const double dfPixVal =
                    bHasNoData &&
                            (IsNoData(dfA, dfNoData) || IsNoData(dfB, dfNoData))
                        ? dfNoData
                        : dfA - dfB;

                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // DiffPixelFunc

static const char pszMulPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='k' description='Optional constant factor' "
    "type='double' default='1.0' />"
    "   <Argument name='propagateNoData' description='Whether the output value "
    "should be NoData as as soon as one source is NoData' type='boolean' "
    "default='false' />"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "</PixelFunctionArgumentsList>";

static CPLErr MulPixelFunc(void **papoSources, int nSources, void *pData,
                           int nXSize, int nYSize, GDALDataType eSrcType,
                           GDALDataType eBufType, int nPixelSpace,
                           int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (nSources < 2 && CSLFetchNameValue(papszArgs, "k") == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "mul requires at least two sources or a specified constant k");
        return CE_Failure;
    }

    double dfK = 1.0;
    if (FetchDoubleArg(papszArgs, "k", &dfK, &dfK) != CE_None)
        return CE_Failure;

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    const bool bPropagateNoData = CPLTestBool(
        CSLFetchNameValueDef(papszArgs, "propagateNoData", "false"));

    /* ---- Set pixels ---- */
    if (GDALDataTypeIsComplex(eSrcType))
    {
        const int nOffset = GDALGetDataTypeSizeBytes(eSrcType) / 2;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                double adfPixVal[2] = {dfK, 0.0};

                for (int iSrc = 0; iSrc < nSources; ++iSrc)
                {
                    const void *const pReal = papoSources[iSrc];
                    const void *const pImag =
                        static_cast<const GByte *>(pReal) + nOffset;

                    const double dfOldR = adfPixVal[0];
                    const double dfOldI = adfPixVal[1];

                    // Source raster pixels may be obtained with GetSrcVal
                    // macro.
                    const double dfNewR = GetSrcVal(pReal, eSrcType, ii);
                    const double dfNewI = GetSrcVal(pImag, eSrcType, ii);

                    adfPixVal[0] = dfOldR * dfNewR - dfOldI * dfNewI;
                    adfPixVal[1] = dfOldR * dfNewI + dfOldI * dfNewR;
                }

                GDALCopyWords(adfPixVal, GDT_CFloat64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                double dfPixVal = dfK;  // Not complex.

                for (int iSrc = 0; iSrc < nSources; ++iSrc)
                {
                    const double dfVal =
                        GetSrcVal(papoSources[iSrc], eSrcType, ii);

                    if (bHasNoData && IsNoData(dfVal, dfNoData))
                    {
                        if (bPropagateNoData)
                        {
                            dfPixVal = dfNoData;
                            break;
                        }
                    }
                    else
                    {
                        dfPixVal *= dfVal;
                    }
                }

                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // MulPixelFunc

static const char pszDivPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   "
    "<Argument type='builtin' value='NoData' optional='true' />"
    "</PixelFunctionArgumentsList>";

static CPLErr DivPixelFunc(void **papoSources, int nSources, void *pData,
                           int nXSize, int nYSize, GDALDataType eSrcType,
                           GDALDataType eBufType, int nPixelSpace,
                           int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (nSources != 2)
        return CE_Failure;

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    /* ---- Set pixels ---- */
    if (GDALDataTypeIsComplex(eSrcType))
    {
        const int nOffset = GDALGetDataTypeSizeBytes(eSrcType) / 2;
        const void *const pReal0 = papoSources[0];
        const void *const pImag0 =
            static_cast<GByte *>(papoSources[0]) + nOffset;
        const void *const pReal1 = papoSources[1];
        const void *const pImag1 =
            static_cast<GByte *>(papoSources[1]) + nOffset;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal0 = GetSrcVal(pReal0, eSrcType, ii);
                const double dfReal1 = GetSrcVal(pReal1, eSrcType, ii);
                const double dfImag0 = GetSrcVal(pImag0, eSrcType, ii);
                const double dfImag1 = GetSrcVal(pImag1, eSrcType, ii);
                const double dfAux = dfReal1 * dfReal1 + dfImag1 * dfImag1;

                const double adfPixVal[2] = {
                    dfAux == 0
                        ? std::numeric_limits<double>::infinity()
                        : dfReal0 * dfReal1 / dfAux + dfImag0 * dfImag1 / dfAux,
                    dfAux == 0 ? std::numeric_limits<double>::infinity()
                               : dfReal1 / dfAux * dfImag0 -
                                     dfReal0 * dfImag1 / dfAux};

                GDALCopyWords(adfPixVal, GDT_CFloat64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                const double dfNum = GetSrcVal(papoSources[0], eSrcType, ii);
                const double dfDenom = GetSrcVal(papoSources[1], eSrcType, ii);

                double dfPixVal = dfNoData;
                if (!bHasNoData || (!IsNoData(dfNum, dfNoData) &&
                                    !IsNoData(dfDenom, dfNoData)))
                {
                    // coverity[divide_by_zero]
                    dfPixVal =
                        dfDenom == 0
                            ? std::numeric_limits<double>::infinity()
                            : dfNum /
#ifdef __COVERITY__
                                  (dfDenom + std::numeric_limits<double>::min())
#else
                                  dfDenom
#endif
                        ;
                }

                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // DivPixelFunc

static CPLErr CMulPixelFunc(void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize, GDALDataType eSrcType,
                            GDALDataType eBufType, int nPixelSpace,
                            int nLineSpace)
{
    /* ---- Init ---- */
    if (nSources != 2)
        return CE_Failure;

    /* ---- Set pixels ---- */
    if (GDALDataTypeIsComplex(eSrcType))
    {
        const int nOffset = GDALGetDataTypeSizeBytes(eSrcType) / 2;
        const void *const pReal0 = papoSources[0];
        const void *const pImag0 =
            static_cast<GByte *>(papoSources[0]) + nOffset;
        const void *const pReal1 = papoSources[1];
        const void *const pImag1 =
            static_cast<GByte *>(papoSources[1]) + nOffset;

        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal0 = GetSrcVal(pReal0, eSrcType, ii);
                const double dfReal1 = GetSrcVal(pReal1, eSrcType, ii);
                const double dfImag0 = GetSrcVal(pImag0, eSrcType, ii);
                const double dfImag1 = GetSrcVal(pImag1, eSrcType, ii);
                const double adfPixVal[2] = {
                    dfReal0 * dfReal1 + dfImag0 * dfImag1,
                    dfReal1 * dfImag0 - dfReal0 * dfImag1};

                GDALCopyWords(adfPixVal, GDT_CFloat64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                // Not complex.
                const double adfPixVal[2] = {
                    GetSrcVal(papoSources[0], eSrcType, ii) *
                        GetSrcVal(papoSources[1], eSrcType, ii),
                    0.0};

                GDALCopyWords(adfPixVal, GDT_CFloat64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // CMulPixelFunc

static const char pszInvPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='k' description='Optional constant factor' "
    "type='double' default='1.0' />"
    "   "
    "<Argument type='builtin' value='NoData' optional='true' />"
    "</PixelFunctionArgumentsList>";

static CPLErr InvPixelFunc(void **papoSources, int nSources, void *pData,
                           int nXSize, int nYSize, GDALDataType eSrcType,
                           GDALDataType eBufType, int nPixelSpace,
                           int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;

    double dfK = 1.0;
    if (FetchDoubleArg(papszArgs, "k", &dfK, &dfK) != CE_None)
        return CE_Failure;

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    /* ---- Set pixels ---- */
    if (GDALDataTypeIsComplex(eSrcType))
    {
        const int nOffset = GDALGetDataTypeSizeBytes(eSrcType) / 2;
        const void *const pReal = papoSources[0];
        const void *const pImag =
            static_cast<GByte *>(papoSources[0]) + nOffset;

        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal = GetSrcVal(pReal, eSrcType, ii);
                const double dfImag = GetSrcVal(pImag, eSrcType, ii);
                const double dfAux = dfReal * dfReal + dfImag * dfImag;
                const double adfPixVal[2] = {
                    dfAux == 0 ? std::numeric_limits<double>::infinity()
                               : dfK * dfReal / dfAux,
                    dfAux == 0 ? std::numeric_limits<double>::infinity()
                               : -dfK * dfImag / dfAux};

                GDALCopyWords(adfPixVal, GDT_CFloat64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                // Not complex.
                const double dfVal = GetSrcVal(papoSources[0], eSrcType, ii);
                double dfPixVal = dfNoData;

                if (!bHasNoData || !IsNoData(dfVal, dfNoData))
                {
                    dfPixVal =
                        dfVal == 0
                            ? std::numeric_limits<double>::infinity()
                            : dfK /
#ifdef __COVERITY__
                                  (dfVal + std::numeric_limits<double>::min())
#else
                                  dfVal
#endif
                        ;
                }

                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // InvPixelFunc

static CPLErr IntensityPixelFunc(void **papoSources, int nSources, void *pData,
                                 int nXSize, int nYSize, GDALDataType eSrcType,
                                 GDALDataType eBufType, int nPixelSpace,
                                 int nLineSpace)
{
    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;

    if (GDALDataTypeIsComplex(eSrcType))
    {
        const int nOffset = GDALGetDataTypeSizeBytes(eSrcType) / 2;
        const void *const pReal = papoSources[0];
        const void *const pImag =
            static_cast<GByte *>(papoSources[0]) + nOffset;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal = GetSrcVal(pReal, eSrcType, ii);
                const double dfImag = GetSrcVal(pImag, eSrcType, ii);

                const double dfPixVal = dfReal * dfReal + dfImag * dfImag;

                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                double dfPixVal = GetSrcVal(papoSources[0], eSrcType, ii);
                dfPixVal *= dfPixVal;

                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // IntensityPixelFunc

static const char pszSqrtPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument type='builtin' value='NoData' optional='true'/>"
    "</PixelFunctionArgumentsList>";

static CPLErr SqrtPixelFunc(void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize, GDALDataType eSrcType,
                            GDALDataType eBufType, int nPixelSpace,
                            int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;
    if (GDALDataTypeIsComplex(eSrcType))
        return CE_Failure;

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            // Source raster pixels may be obtained with GetSrcVal macro.
            double dfPixVal = GetSrcVal(papoSources[0], eSrcType, ii);

            if (bHasNoData && IsNoData(dfPixVal, dfNoData))
            {
                dfPixVal = dfNoData;
            }
            else
            {
                dfPixVal = std::sqrt(dfPixVal);
            }

            GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // SqrtPixelFunc

static CPLErr Log10PixelFuncHelper(void **papoSources, int nSources,
                                   void *pData, int nXSize, int nYSize,
                                   GDALDataType eSrcType, GDALDataType eBufType,
                                   int nPixelSpace, int nLineSpace,
                                   CSLConstList papszArgs, double fact)
{
    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    if (GDALDataTypeIsComplex(eSrcType))
    {
        // Complex input datatype.
        const int nOffset = GDALGetDataTypeSizeBytes(eSrcType) / 2;
        const void *const pReal = papoSources[0];
        const void *const pImag =
            static_cast<GByte *>(papoSources[0]) + nOffset;

        /* We should compute fact * log10( sqrt( dfReal * dfReal + dfImag *
         * dfImag ) ) */
        /* Given that log10(sqrt(x)) = 0.5 * log10(x) */
        /* we can remove the sqrt() by multiplying fact by 0.5 */
        fact *= 0.5;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal = GetSrcVal(pReal, eSrcType, ii);
                const double dfImag = GetSrcVal(pImag, eSrcType, ii);

                const double dfPixVal =
                    fact * log10(dfReal * dfReal + dfImag * dfImag);

                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for (int iLine = 0; iLine < nYSize; ++iLine)
        {
            for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
            {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfSrcVal = GetSrcVal(papoSources[0], eSrcType, ii);
                const double dfPixVal =
                    bHasNoData && IsNoData(dfSrcVal, dfNoData)
                        ? dfNoData
                        : fact * std::log10(std::abs(dfSrcVal));

                GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                              static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine +
                                  iCol * nPixelSpace,
                              eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // Log10PixelFuncHelper

static const char pszLog10PixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument type='builtin' value='NoData' optional='true'/>"
    "</PixelFunctionArgumentsList>";

static CPLErr Log10PixelFunc(void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize, GDALDataType eSrcType,
                             GDALDataType eBufType, int nPixelSpace,
                             int nLineSpace, CSLConstList papszArgs)
{
    return Log10PixelFuncHelper(papoSources, nSources, pData, nXSize, nYSize,
                                eSrcType, eBufType, nPixelSpace, nLineSpace,
                                papszArgs, 1.0);
}  // Log10PixelFunc

static const char pszDBPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='fact' description='Factor' type='double' "
    "default='20.0' />"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "</PixelFunctionArgumentsList>";

static CPLErr DBPixelFunc(void **papoSources, int nSources, void *pData,
                          int nXSize, int nYSize, GDALDataType eSrcType,
                          GDALDataType eBufType, int nPixelSpace,
                          int nLineSpace, CSLConstList papszArgs)
{
    double dfFact = 20.;
    if (FetchDoubleArg(papszArgs, "fact", &dfFact, &dfFact) != CE_None)
        return CE_Failure;

    return Log10PixelFuncHelper(papoSources, nSources, pData, nXSize, nYSize,
                                eSrcType, eBufType, nPixelSpace, nLineSpace,
                                papszArgs, dfFact);
}  // DBPixelFunc

static CPLErr ExpPixelFuncHelper(void **papoSources, int nSources, void *pData,
                                 int nXSize, int nYSize, GDALDataType eSrcType,
                                 GDALDataType eBufType, int nPixelSpace,
                                 int nLineSpace, CSLConstList papszArgs,
                                 double base, double fact)
{
    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;
    if (GDALDataTypeIsComplex(eSrcType))
        return CE_Failure;

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            // Source raster pixels may be obtained with GetSrcVal macro.
            const double dfVal = GetSrcVal(papoSources[0], eSrcType, ii);
            const double dfPixVal = bHasNoData && IsNoData(dfVal, dfNoData)
                                        ? dfNoData
                                        : pow(base, dfVal * fact);

            GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // ExpPixelFuncHelper

static const char pszExpPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='base' description='Base' type='double' "
    "default='2.7182818284590452353602874713526624' />"
    "   <Argument name='fact' description='Factor' type='double' default='1' />"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "</PixelFunctionArgumentsList>";

static CPLErr ExpPixelFunc(void **papoSources, int nSources, void *pData,
                           int nXSize, int nYSize, GDALDataType eSrcType,
                           GDALDataType eBufType, int nPixelSpace,
                           int nLineSpace, CSLConstList papszArgs)
{
    double dfBase = 2.7182818284590452353602874713526624;
    double dfFact = 1.;

    if (FetchDoubleArg(papszArgs, "base", &dfBase, &dfBase) != CE_None)
        return CE_Failure;

    if (FetchDoubleArg(papszArgs, "fact", &dfFact, &dfFact) != CE_None)
        return CE_Failure;

    return ExpPixelFuncHelper(papoSources, nSources, pData, nXSize, nYSize,
                              eSrcType, eBufType, nPixelSpace, nLineSpace,
                              papszArgs, dfBase, dfFact);
}  // ExpPixelFunc

static CPLErr dB2AmpPixelFunc(void **papoSources, int nSources, void *pData,
                              int nXSize, int nYSize, GDALDataType eSrcType,
                              GDALDataType eBufType, int nPixelSpace,
                              int nLineSpace)
{
    return ExpPixelFuncHelper(papoSources, nSources, pData, nXSize, nYSize,
                              eSrcType, eBufType, nPixelSpace, nLineSpace,
                              nullptr, 10.0, 1. / 20);
}  // dB2AmpPixelFunc

static CPLErr dB2PowPixelFunc(void **papoSources, int nSources, void *pData,
                              int nXSize, int nYSize, GDALDataType eSrcType,
                              GDALDataType eBufType, int nPixelSpace,
                              int nLineSpace)
{
    return ExpPixelFuncHelper(papoSources, nSources, pData, nXSize, nYSize,
                              eSrcType, eBufType, nPixelSpace, nLineSpace,
                              nullptr, 10.0, 1. / 10);
}  // dB2PowPixelFunc

static const char pszPowPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='power' description='Exponent' type='double' "
    "mandatory='1' />"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "</PixelFunctionArgumentsList>";

static CPLErr PowPixelFunc(void **papoSources, int nSources, void *pData,
                           int nXSize, int nYSize, GDALDataType eSrcType,
                           GDALDataType eBufType, int nPixelSpace,
                           int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;
    if (GDALDataTypeIsComplex(eSrcType))
        return CE_Failure;

    double power;
    if (FetchDoubleArg(papszArgs, "power", &power) != CE_None)
        return CE_Failure;

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            const double dfVal = GetSrcVal(papoSources[0], eSrcType, ii);

            const double dfPixVal = bHasNoData && IsNoData(dfVal, dfNoData)
                                        ? dfNoData
                                        : std::pow(dfVal, power);

            GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}

// Given nt intervals spaced by dt and beginning at t0, return the index of
// the lower bound of the interval that should be used to
// interpolate/extrapolate a value for t.
static std::size_t intervalLeft(double t0, double dt, std::size_t nt, double t)
{
    if (t < t0)
    {
        return 0;
    }

    std::size_t n = static_cast<std::size_t>((t - t0) / dt);

    if (n >= nt - 1)
    {
        return nt - 2;
    }

    return n;
}

static double InterpolateLinear(double dfX0, double dfX1, double dfY0,
                                double dfY1, double dfX)
{
    return dfY0 + (dfX - dfX0) * (dfY1 - dfY0) / (dfX1 - dfX0);
}

static double InterpolateExponential(double dfX0, double dfX1, double dfY0,
                                     double dfY1, double dfX)
{
    const double r = std::log(dfY1 / dfY0) / (dfX1 - dfX0);
    return dfY0 * std::exp(r * (dfX - dfX0));
}

static const char pszInterpolatePixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='t0' description='t0' type='double' mandatory='1' />"
    "   <Argument name='dt' description='dt' type='double' mandatory='1' />"
    "   <Argument name='t' description='t' type='double' mandatory='1' />"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "</PixelFunctionArgumentsList>";

template <decltype(InterpolateLinear) InterpolationFunction>
CPLErr InterpolatePixelFunc(void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize, GDALDataType eSrcType,
                            GDALDataType eBufType, int nPixelSpace,
                            int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (GDALDataTypeIsComplex(eSrcType))
        return CE_Failure;

    double dfT0;
    if (FetchDoubleArg(papszArgs, "t0", &dfT0) == CE_Failure)
        return CE_Failure;

    double dfT;
    if (FetchDoubleArg(papszArgs, "t", &dfT) == CE_Failure)
        return CE_Failure;

    double dfDt;
    if (FetchDoubleArg(papszArgs, "dt", &dfDt) == CE_Failure)
        return CE_Failure;

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    if (nSources < 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "At least two sources required for interpolation.");
        return CE_Failure;
    }

    if (dfT == 0 || !std::isfinite(dfT))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "dt must be finite and non-zero");
        return CE_Failure;
    }

    const auto i0 = intervalLeft(dfT0, dfDt, nSources, dfT);
    const auto i1 = i0 + 1;
    const double dfX0 = dfT0 + static_cast<double>(i0) * dfDt;
    const double dfX1 = dfT0 + static_cast<double>(i0 + 1) * dfDt;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            const double dfY0 = GetSrcVal(papoSources[i0], eSrcType, ii);
            const double dfY1 = GetSrcVal(papoSources[i1], eSrcType, ii);

            double dfPixVal = dfNoData;
            if (dfT == dfX0)
                dfPixVal = dfY0;
            else if (dfT == dfX1)
                dfPixVal = dfY1;
            else if (!bHasNoData ||
                     (!IsNoData(dfY0, dfNoData) && !IsNoData(dfY1, dfNoData)))
                dfPixVal = InterpolationFunction(dfX0, dfX1, dfY0, dfY1, dfT);

            GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}

static const char pszReplaceNoDataPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument type='builtin' value='NoData' />"
    "   <Argument name='to' type='double' description='New NoData value to be "
    "replaced' default='nan' />"
    "</PixelFunctionArgumentsList>";

static CPLErr ReplaceNoDataPixelFunc(void **papoSources, int nSources,
                                     void *pData, int nXSize, int nYSize,
                                     GDALDataType eSrcType,
                                     GDALDataType eBufType, int nPixelSpace,
                                     int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;
    if (GDALDataTypeIsComplex(eSrcType))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "replace_nodata cannot convert complex data types");
        return CE_Failure;
    }

    double dfOldNoData, dfNewNoData = NAN;
    if (FetchDoubleArg(papszArgs, "NoData", &dfOldNoData) != CE_None)
        return CE_Failure;
    if (FetchDoubleArg(papszArgs, "to", &dfNewNoData, &dfNewNoData) != CE_None)
        return CE_Failure;

    if (!GDALDataTypeIsFloating(eBufType) && std::isnan(dfNewNoData))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Using nan requires a floating point type output buffer");
        return CE_Failure;
    }

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            double dfPixVal = GetSrcVal(papoSources[0], eSrcType, ii);
            if (dfPixVal == dfOldNoData || std::isnan(dfPixVal))
                dfPixVal = dfNewNoData;

            GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}

static const char pszScalePixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument type='builtin' value='offset' />"
    "   <Argument type='builtin' value='scale' />"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "</PixelFunctionArgumentsList>";

static CPLErr ScalePixelFunc(void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize, GDALDataType eSrcType,
                             GDALDataType eBufType, int nPixelSpace,
                             int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (nSources != 1)
        return CE_Failure;
    if (GDALDataTypeIsComplex(eSrcType))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "scale cannot by applied to complex data types");
        return CE_Failure;
    }

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    double dfScale, dfOffset;
    if (FetchDoubleArg(papszArgs, "scale", &dfScale) != CE_None)
        return CE_Failure;
    if (FetchDoubleArg(papszArgs, "offset", &dfOffset) != CE_None)
        return CE_Failure;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            const double dfVal = GetSrcVal(papoSources[0], eSrcType, ii);

            const double dfPixVal = bHasNoData && IsNoData(dfVal, dfNoData)
                                        ? dfNoData
                                        : dfVal * dfScale + dfOffset;

            GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}

static const char pszNormDiffPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "</PixelFunctionArgumentsList>";

static CPLErr NormDiffPixelFunc(void **papoSources, int nSources, void *pData,
                                int nXSize, int nYSize, GDALDataType eSrcType,
                                GDALDataType eBufType, int nPixelSpace,
                                int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (nSources != 2)
        return CE_Failure;

    if (GDALDataTypeIsComplex(eSrcType))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "norm_diff cannot by applied to complex data types");
        return CE_Failure;
    }

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            const double dfLeftVal = GetSrcVal(papoSources[0], eSrcType, ii);
            const double dfRightVal = GetSrcVal(papoSources[1], eSrcType, ii);

            double dfPixVal = dfNoData;

            if (!bHasNoData || (!IsNoData(dfLeftVal, dfNoData) &&
                                !IsNoData(dfRightVal, dfNoData)))
            {
                const double dfDenom = (dfLeftVal + dfRightVal);
                // coverity[divide_by_zero]
                dfPixVal =
                    dfDenom == 0
                        ? std::numeric_limits<double>::infinity()
                        : (dfLeftVal - dfRightVal) /
#ifdef __COVERITY__
                              (dfDenom + std::numeric_limits<double>::min())
#else
                              dfDenom
#endif
                    ;
            }

            GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // NormDiffPixelFunc

/************************************************************************/
/*                     pszMinMaxFuncMetadataNodata                      */
/************************************************************************/

static const char pszArgMinMaxFuncMetadataNodata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "   <Argument name='propagateNoData' description='Whether the output value "
    "should be NoData as as soon as one source is NoData' type='boolean' "
    "default='false' />"
    "</PixelFunctionArgumentsList>";

static const char pszMinMaxFuncMetadataNodata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='k' description='Optional constant term' type='double' "
    "default='nan' />"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "   <Argument name='propagateNoData' description='Whether the output value "
    "should be NoData as as soon as one source is NoData' type='boolean' "
    "default='false' />"
    "</PixelFunctionArgumentsList>";

namespace
{
struct ReturnIndex;
struct ReturnValue;
}  // namespace

template <class Comparator, class ReturnType = ReturnValue>
static CPLErr MinOrMaxPixelFunc(double dfK, void **papoSources, int nSources,
                                void *pData, int nXSize, int nYSize,
                                GDALDataType eSrcType, GDALDataType eBufType,
                                int nPixelSpace, int nLineSpace,
                                CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (GDALDataTypeIsComplex(eSrcType))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Complex data type not supported for min/max().");
        return CE_Failure;
    }

    double dfNoData = std::numeric_limits<double>::quiet_NaN();
    if (FetchDoubleArg(papszArgs, "NoData", &dfNoData, &dfNoData) != CE_None)
        return CE_Failure;
    const bool bPropagateNoData = CPLTestBool(
        CSLFetchNameValueDef(papszArgs, "propagateNoData", "false"));

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            double dfRes = std::numeric_limits<double>::quiet_NaN();
            double dfResSrc = std::numeric_limits<double>::quiet_NaN();

            for (int iSrc = 0; iSrc < nSources; ++iSrc)
            {
                const double dfVal = GetSrcVal(papoSources[iSrc], eSrcType, ii);

                if (std::isnan(dfVal) || dfVal == dfNoData)
                {
                    if (bPropagateNoData)
                    {
                        dfRes = dfNoData;
                        if constexpr (std::is_same_v<ReturnType, ReturnIndex>)
                        {
                            dfResSrc = std::numeric_limits<double>::quiet_NaN();
                        }
                        break;
                    }
                }
                else if (Comparator::compare(dfVal, dfRes))
                {
                    dfRes = dfVal;
                    if constexpr (std::is_same_v<ReturnType, ReturnIndex>)
                    {
                        dfResSrc = iSrc;
                    }
                }
            }

            if constexpr (std::is_same_v<ReturnType, ReturnIndex>)
            {
                static_cast<void>(dfK);  // Placate gcc 9.4
                dfRes = std::isnan(dfResSrc) ? dfNoData : dfResSrc + 1;
            }
            else
            {
                if (std::isnan(dfRes))
                {
                    dfRes = dfNoData;
                }

                if (IsNoData(dfRes, dfNoData))
                {
                    if (!bPropagateNoData && !std::isnan(dfK))
                    {
                        dfRes = dfK;
                    }
                }
                else if (!std::isnan(dfK) && Comparator::compare(dfK, dfRes))
                {
                    dfRes = dfK;
                }
            }

            GDALCopyWords(&dfRes, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
} /* MinOrMaxPixelFunc */

#ifdef USE_SSE2

template <class T, class SSEWrapper>
static void OptimizedMinOrMaxSSE2(const void *const *papoSources, int nSources,
                                  void *pData, int nXSize, int nYSize,
                                  int nLineSpace)
{
    assert(nSources >= 1);
    constexpr int VALUES_PER_REG =
        static_cast<int>(sizeof(typename SSEWrapper::Vec) / sizeof(T));
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        T *CPL_RESTRICT pDest =
            reinterpret_cast<T *>(static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine);
        const size_t iOffsetLine = static_cast<size_t>(iLine) * nXSize;
        int iCol = 0;
        for (; iCol < nXSize - (2 * VALUES_PER_REG - 1);
             iCol += 2 * VALUES_PER_REG)
        {
            auto reg0 = SSEWrapper::LoadU(
                static_cast<const T * CPL_RESTRICT>(papoSources[0]) +
                iOffsetLine + iCol);
            auto reg1 = SSEWrapper::LoadU(
                static_cast<const T * CPL_RESTRICT>(papoSources[0]) +
                iOffsetLine + iCol + VALUES_PER_REG);
            for (int iSrc = 1; iSrc < nSources; ++iSrc)
            {
                reg0 = SSEWrapper::MinOrMax(
                    reg0, SSEWrapper::LoadU(static_cast<const T * CPL_RESTRICT>(
                                                papoSources[iSrc]) +
                                            iOffsetLine + iCol));
                reg1 = SSEWrapper::MinOrMax(
                    reg1,
                    SSEWrapper::LoadU(
                        static_cast<const T * CPL_RESTRICT>(papoSources[iSrc]) +
                        iOffsetLine + iCol + VALUES_PER_REG));
            }
            SSEWrapper::StoreU(pDest + iCol, reg0);
            SSEWrapper::StoreU(pDest + iCol + VALUES_PER_REG, reg1);
        }
        for (; iCol < nXSize; ++iCol)
        {
            T v = static_cast<const T * CPL_RESTRICT>(
                papoSources[0])[iOffsetLine + iCol];
            for (int iSrc = 1; iSrc < nSources; ++iSrc)
            {
                v = SSEWrapper::MinOrMax(
                    v, static_cast<const T * CPL_RESTRICT>(
                           papoSources[iSrc])[iOffsetLine + iCol]);
            }
            pDest[iCol] = v;
        }
    }
}

// clang-format off
namespace
{
struct SSEWrapperMinByte
{
    using T = uint8_t;
    typedef __m128i Vec;

    static inline Vec LoadU(const T *x) { return _mm_loadu_si128(reinterpret_cast<const Vec*>(x)); }
    static inline void StoreU(T *x, Vec y) { _mm_storeu_si128(reinterpret_cast<Vec*>(x), y); }
    static inline Vec MinOrMax(Vec x, Vec y) { return _mm_min_epu8(x, y); }
    static inline T MinOrMax(T x, T y) { return std::min(x, y); }
};

struct SSEWrapperMaxByte
{
    using T = uint8_t;
    typedef __m128i Vec;

    static inline Vec LoadU(const T *x) { return _mm_loadu_si128(reinterpret_cast<const Vec*>(x)); }
    static inline void StoreU(T *x, Vec y) { _mm_storeu_si128(reinterpret_cast<Vec*>(x), y); }
    static inline Vec MinOrMax(Vec x, Vec y) { return _mm_max_epu8(x, y); }
    static inline T MinOrMax(T x, T y) { return std::max(x, y); }
};

struct SSEWrapperMinUInt16
{
    using T = uint16_t;
    typedef __m128i Vec;

    static inline Vec LoadU(const T *x) { return _mm_loadu_si128(reinterpret_cast<const Vec*>(x)); }
    static inline void StoreU(T *x, Vec y) { _mm_storeu_si128(reinterpret_cast<Vec*>(x), y); }
#if defined(__SSE4_1__) || defined(USE_NEON_OPTIMIZATIONS)
    static inline Vec MinOrMax(Vec x, Vec y) { return _mm_min_epu16(x, y); }
#else
    static inline Vec MinOrMax(Vec x, Vec y) { return
        _mm_add_epi16(
           _mm_min_epi16(
             _mm_add_epi16(x, _mm_set1_epi16(-32768)),
             _mm_add_epi16(y, _mm_set1_epi16(-32768))),
           _mm_set1_epi16(-32768)); }
#endif
    static inline T MinOrMax(T x, T y) { return std::min(x, y); }
};

struct SSEWrapperMaxUInt16
{
    using T = uint16_t;
    typedef __m128i Vec;

    static inline Vec LoadU(const T *x) { return _mm_loadu_si128(reinterpret_cast<const Vec*>(x)); }
    static inline void StoreU(T *x, Vec y) { _mm_storeu_si128(reinterpret_cast<Vec*>(x), y); }
#if defined(__SSE4_1__) || defined(USE_NEON_OPTIMIZATIONS)
    static inline Vec MinOrMax(Vec x, Vec y) { return _mm_max_epu16(x, y); }
#else
    static inline Vec MinOrMax(Vec x, Vec y) { return
        _mm_add_epi16(
           _mm_max_epi16(
             _mm_add_epi16(x, _mm_set1_epi16(-32768)),
             _mm_add_epi16(y, _mm_set1_epi16(-32768))),
           _mm_set1_epi16(-32768)); }
#endif
    static inline T MinOrMax(T x, T y) { return std::max(x, y); }
};

struct SSEWrapperMinInt16
{
    using T = int16_t;
    typedef __m128i Vec;

    static inline Vec LoadU(const T *x) { return _mm_loadu_si128(reinterpret_cast<const Vec*>(x)); }
    static inline void StoreU(T *x, Vec y) { _mm_storeu_si128(reinterpret_cast<Vec*>(x), y); }
    static inline Vec MinOrMax(Vec x, Vec y) { return _mm_min_epi16(x, y); }
    static inline T MinOrMax(T x, T y) { return std::min(x, y); }
};

struct SSEWrapperMaxInt16
{
    using T = int16_t;
    typedef __m128i Vec;

    static inline Vec LoadU(const T *x) { return _mm_loadu_si128(reinterpret_cast<const Vec*>(x)); }
    static inline void StoreU(T *x, Vec y) { _mm_storeu_si128(reinterpret_cast<Vec*>(x), y); }
    static inline Vec MinOrMax(Vec x, Vec y) { return _mm_max_epi16(x, y); }
    static inline T MinOrMax(T x, T y) { return std::max(x, y); }
};

struct SSEWrapperMinFloat
{
    using T = float;
    typedef __m128 Vec;

    static inline Vec LoadU(const T *x) { return _mm_loadu_ps(x); }
    static inline void StoreU(T *x, Vec y) { _mm_storeu_ps(x, y); }
    static inline Vec MinOrMax(Vec x, Vec y) { return _mm_min_ps(x, y); }
    static inline T MinOrMax(T x, T y) { return std::min(x, y); }
};

struct SSEWrapperMaxFloat
{
    using T = float;
    typedef __m128 Vec;

    static inline Vec LoadU(const T *x) { return _mm_loadu_ps(x); }
    static inline void StoreU(T *x, Vec y) { _mm_storeu_ps(x, y); }
    static inline Vec MinOrMax(Vec x, Vec y) { return _mm_max_ps(x, y); }
    static inline T MinOrMax(T x, T y) { return std::max(x, y); }
};

struct SSEWrapperMinDouble
{
    using T = double;
    typedef __m128d Vec;

    static inline Vec LoadU(const T *x) { return _mm_loadu_pd(x); }
    static inline void StoreU(T *x, Vec y) { _mm_storeu_pd(x, y); }
    static inline Vec MinOrMax(Vec x, Vec y) { return _mm_min_pd(x, y); }
    static inline T MinOrMax(T x, T y) { return std::min(x, y); }
};

struct SSEWrapperMaxDouble
{
    using T = double;
    typedef __m128d Vec;

    static inline Vec LoadU(const T *x) { return _mm_loadu_pd(x); }
    static inline void StoreU(T *x, Vec y) { _mm_storeu_pd(x, y); }
    static inline Vec MinOrMax(Vec x, Vec y) { return _mm_max_pd(x, y); }
    static inline T MinOrMax(T x, T y) { return std::max(x, y); }
};

}  // namespace

// clang-format on

#endif  // USE_SSE2

template <typename ReturnType>
static CPLErr MinPixelFunc(void **papoSources, int nSources, void *pData,
                           int nXSize, int nYSize, GDALDataType eSrcType,
                           GDALDataType eBufType, int nPixelSpace,
                           int nLineSpace, CSLConstList papszArgs)
{
    struct Comparator
    {
        static bool compare(double x, double resVal)
        {
            // Written this way to deal with resVal being NaN
            return !(x >= resVal);
        }
    };

    double dfK = std::numeric_limits<double>::quiet_NaN();
    if constexpr (std::is_same_v<ReturnType, ReturnValue>)
    {
        if (FetchDoubleArg(papszArgs, "k", &dfK, &dfK) != CE_None)
            return CE_Failure;

#ifdef USE_SSE2
        const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
        if (std::isnan(dfK) && nSources > 0 && !bHasNoData &&
            eSrcType == eBufType &&
            nPixelSpace == GDALGetDataTypeSizeBytes(eSrcType))
        {
            if (eSrcType == GDT_UInt8)
            {
                OptimizedMinOrMaxSSE2<uint8_t, SSEWrapperMinByte>(
                    papoSources, nSources, pData, nXSize, nYSize, nLineSpace);
                return CE_None;
            }
            else if (eSrcType == GDT_UInt16)
            {
                OptimizedMinOrMaxSSE2<uint16_t, SSEWrapperMinUInt16>(
                    papoSources, nSources, pData, nXSize, nYSize, nLineSpace);
                return CE_None;
            }
            else if (eSrcType == GDT_Int16)
            {
                OptimizedMinOrMaxSSE2<int16_t, SSEWrapperMinInt16>(
                    papoSources, nSources, pData, nXSize, nYSize, nLineSpace);
                return CE_None;
            }
            else if (eSrcType == GDT_Float32)
            {
                OptimizedMinOrMaxSSE2<float, SSEWrapperMinFloat>(
                    papoSources, nSources, pData, nXSize, nYSize, nLineSpace);
                return CE_None;
            }
            else if (eSrcType == GDT_Float64)
            {
                OptimizedMinOrMaxSSE2<double, SSEWrapperMinDouble>(
                    papoSources, nSources, pData, nXSize, nYSize, nLineSpace);
                return CE_None;
            }
        }
#endif
    }

    return MinOrMaxPixelFunc<Comparator, ReturnType>(
        dfK, papoSources, nSources, pData, nXSize, nYSize, eSrcType, eBufType,
        nPixelSpace, nLineSpace, papszArgs);
}

template <typename ReturnType>
static CPLErr MaxPixelFunc(void **papoSources, int nSources, void *pData,
                           int nXSize, int nYSize, GDALDataType eSrcType,
                           GDALDataType eBufType, int nPixelSpace,
                           int nLineSpace, CSLConstList papszArgs)
{
    struct Comparator
    {
        static bool compare(double x, double resVal)
        {
            // Written this way to deal with resVal being NaN
            return !(x <= resVal);
        }
    };

    double dfK = std::numeric_limits<double>::quiet_NaN();
    if constexpr (std::is_same_v<ReturnType, ReturnValue>)
    {
        if (FetchDoubleArg(papszArgs, "k", &dfK, &dfK) != CE_None)
            return CE_Failure;

#ifdef USE_SSE2
        const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
        if (std::isnan(dfK) && nSources > 0 && !bHasNoData &&
            eSrcType == eBufType &&
            nPixelSpace == GDALGetDataTypeSizeBytes(eSrcType))
        {
            if (eSrcType == GDT_UInt8)
            {
                OptimizedMinOrMaxSSE2<uint8_t, SSEWrapperMaxByte>(
                    papoSources, nSources, pData, nXSize, nYSize, nLineSpace);
                return CE_None;
            }
            else if (eSrcType == GDT_UInt16)
            {
                OptimizedMinOrMaxSSE2<uint16_t, SSEWrapperMaxUInt16>(
                    papoSources, nSources, pData, nXSize, nYSize, nLineSpace);
                return CE_None;
            }
            else if (eSrcType == GDT_Int16)
            {
                OptimizedMinOrMaxSSE2<int16_t, SSEWrapperMaxInt16>(
                    papoSources, nSources, pData, nXSize, nYSize, nLineSpace);
                return CE_None;
            }
            else if (eSrcType == GDT_Float32)
            {
                OptimizedMinOrMaxSSE2<float, SSEWrapperMaxFloat>(
                    papoSources, nSources, pData, nXSize, nYSize, nLineSpace);
                return CE_None;
            }
            else if (eSrcType == GDT_Float64)
            {
                OptimizedMinOrMaxSSE2<double, SSEWrapperMaxDouble>(
                    papoSources, nSources, pData, nXSize, nYSize, nLineSpace);
                return CE_None;
            }
        }
#endif
    }

    return MinOrMaxPixelFunc<Comparator, ReturnType>(
        dfK, papoSources, nSources, pData, nXSize, nYSize, eSrcType, eBufType,
        nPixelSpace, nLineSpace, papszArgs);
}

static const char pszExprPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "   <Argument name='propagateNoData' description='Whether the output value "
    "should be NoData as as soon as one source is NoData' type='boolean' "
    "default='false' />"
    "   <Argument name='expression' "
    "             description='Expression to be evaluated' "
    "             type='string'></Argument>"
    "   <Argument name='dialect' "
    "             description='Expression dialect' "
    "             type='string-select'"
    "             default='muparser'>"
    "       <Value>exprtk</Value>"
    "       <Value>muparser</Value>"
    "   </Argument>"
    "   <Argument type='builtin' value='source_names' />"
    "   <Argument type='builtin' value='xoff' />"
    "   <Argument type='builtin' value='yoff' />"
    "   <Argument type='builtin' value='geotransform' />"
    "</PixelFunctionArgumentsList>";

static CPLErr ExprPixelFunc(void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize, GDALDataType eSrcType,
                            GDALDataType eBufType, int nPixelSpace,
                            int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    if (GDALDataTypeIsComplex(eSrcType))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "expression cannot by applied to complex data types");
        return CE_Failure;
    }

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    const bool bPropagateNoData = CPLTestBool(
        CSLFetchNameValueDef(papszArgs, "propagateNoData", "false"));

    const char *pszExpression = CSLFetchNameValue(papszArgs, "expression");
    if (!pszExpression)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing 'expression' pixel function argument");
        return CE_Failure;
    }

    const char *pszSourceNames = CSLFetchNameValue(papszArgs, "source_names");
    const CPLStringList aosSourceNames(
        CSLTokenizeString2(pszSourceNames, "|", 0));
    if (aosSourceNames.size() != nSources)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The source_names variable passed to ExprPixelFunc() has %d "
                 "values, whereas %d were expected. An invalid variable name "
                 "has likely been used",
                 aosSourceNames.size(), nSources);
        return CE_Failure;
    }

    std::vector<double> adfValuesForPixel(nSources);

    const char *pszDialect = CSLFetchNameValue(papszArgs, "dialect");
    if (!pszDialect)
    {
        pszDialect = "muparser";
    }

    auto poExpression = gdal::MathExpression::Create(pszExpression, pszDialect);

    // cppcheck-suppress knownConditionTrueFalse
    if (!poExpression)
    {
        return CE_Failure;
    }

    int nXOff = 0;
    int nYOff = 0;
    GDALGeoTransform gt;
    double dfCenterX = 0;
    double dfCenterY = 0;

    bool includeCenterCoords = false;
    if (strstr(pszExpression, "_CENTER_X_") ||
        strstr(pszExpression, "_CENTER_Y_"))
    {
        includeCenterCoords = true;

        const char *pszXOff = CSLFetchNameValue(papszArgs, "xoff");
        nXOff = std::atoi(pszXOff);

        const char *pszYOff = CSLFetchNameValue(papszArgs, "yoff");
        nYOff = std::atoi(pszYOff);

        const char *pszGT = CSLFetchNameValue(papszArgs, "geotransform");
        if (pszGT == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "To use _CENTER_X_ or _CENTER_Y_ in an expression, "
                     "VRTDataset must have a <GeoTransform> element.");
            return CE_Failure;
        }

        if (!gt.Init(pszGT))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid GeoTransform argument");
            return CE_Failure;
        }
    }

    {
        int iSource = 0;
        for (const auto &osName : aosSourceNames)
        {
            poExpression->RegisterVariable(osName,
                                           &adfValuesForPixel[iSource++]);
        }
    }

    if (includeCenterCoords)
    {
        poExpression->RegisterVariable("_CENTER_X_", &dfCenterX);
        poExpression->RegisterVariable("_CENTER_Y_", &dfCenterY);
    }

    if (bHasNoData)
    {
        poExpression->RegisterVariable("NODATA", &dfNoData);
    }

    if (strstr(pszExpression, "BANDS"))
    {
        poExpression->RegisterVector("BANDS", &adfValuesForPixel);
    }

    std::unique_ptr<double, VSIFreeReleaser> padfResults(
        static_cast<double *>(VSI_MALLOC2_VERBOSE(nXSize, sizeof(double))));
    if (!padfResults)
        return CE_Failure;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            double &dfResult = padfResults.get()[iCol];
            bool resultIsNoData = false;

            for (int iSrc = 0; iSrc < nSources; iSrc++)
            {
                // cppcheck-suppress unreadVariable
                double dfVal = GetSrcVal(papoSources[iSrc], eSrcType, ii);

                if (bHasNoData && bPropagateNoData && IsNoData(dfVal, dfNoData))
                {
                    resultIsNoData = true;
                }

                adfValuesForPixel[iSrc] = dfVal;
            }

            if (includeCenterCoords)
            {
                // Add 0.5 to pixel / line to move from pixel corner to cell center
                gt.Apply(static_cast<double>(iCol + nXOff) + 0.5,
                         static_cast<double>(iLine + nYOff) + 0.5, &dfCenterX,
                         &dfCenterY);
            }

            if (resultIsNoData)
            {
                dfResult = dfNoData;
            }
            else
            {
                if (auto eErr = poExpression->Evaluate(); eErr != CE_None)
                {
                    return CE_Failure;
                }

                dfResult = poExpression->Results()[0];
            }
        }

        GDALCopyWords(padfResults.get(), GDT_Float64, sizeof(double),
                      static_cast<GByte *>(pData) +
                          static_cast<GSpacing>(nLineSpace) * iLine,
                      eBufType, nPixelSpace, nXSize);
    }

    /* ---- Return success ---- */
    return CE_None;
}  // ExprPixelFunc

static constexpr char pszAreaPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument type='builtin' value='crs' />"
    "   <Argument type='builtin' value='xoff' />"
    "   <Argument type='builtin' value='yoff' />"
    "   <Argument type='builtin' value='geotransform' />"
    "</PixelFunctionArgumentsList>";

static CPLErr AreaPixelFunc(void ** /*papoSources*/, int nSources, void *pData,
                            int nXSize, int nYSize, GDALDataType /* eSrcType */,
                            GDALDataType eBufType, int nPixelSpace,
                            int nLineSpace, CSLConstList papszArgs)
{
    if (nSources)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "area: unexpected source band(s)");
        return CE_Failure;
    }

    const char *pszGT = CSLFetchNameValue(papszArgs, "geotransform");
    if (pszGT == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "area: VRTDataset has no <GeoTransform>");
        return CE_Failure;
    }

    GDALGeoTransform gt;
    if (!gt.Init(pszGT))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "area: Invalid GeoTransform argument");
        return CE_Failure;
    }

    const char *pszXOff = CSLFetchNameValue(papszArgs, "xoff");
    const int nXOff = std::atoi(pszXOff);

    const char *pszYOff = CSLFetchNameValue(papszArgs, "yoff");
    const int nYOff = std::atoi(pszYOff);

    const char *pszCrsPtr = CSLFetchNameValue(papszArgs, "crs");

    if (!pszCrsPtr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "area: VRTDataset has no <SRS>");
        return CE_Failure;
    }

    std::uintptr_t nCrsPtr = 0;
    if (auto [end, ec] =
            std::from_chars(pszCrsPtr, pszCrsPtr + strlen(pszCrsPtr), nCrsPtr);
        ec != std::errc())
    {
        // Since "crs" is populated by GDAL, this should never happen.
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to read CRS");
        return CE_Failure;
    }

    const OGRSpatialReference *poCRS =
        reinterpret_cast<const OGRSpatialReference *>(nCrsPtr);

    if (!poCRS)
    {
        // can't get here, but cppcheck doesn't know that
        return CE_Failure;
    }

    const OGRSpatialReference *poGeographicCRS = nullptr;
    std::unique_ptr<OGRSpatialReference> poGeographicCRSHolder;
    std::unique_ptr<OGRCoordinateTransformation> poTransform;

    if (!poCRS->IsGeographic())
    {
        poGeographicCRSHolder = std::make_unique<OGRSpatialReference>();
        if (poGeographicCRSHolder->CopyGeogCSFrom(poCRS) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot reproject geometry to geographic CRS");
            return CE_Failure;
        }
        poGeographicCRSHolder->SetAxisMappingStrategy(
            OAMS_TRADITIONAL_GIS_ORDER);

        poTransform.reset(OGRCreateCoordinateTransformation(
            poCRS, poGeographicCRSHolder.get()));

        if (!poTransform)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot reproject geometry to geographic CRS");
            return CE_Failure;
        }

        poGeographicCRS = poGeographicCRSHolder.get();
    }
    else
    {
        poGeographicCRS = poCRS;
    }

    geod_geodesic g{};
    OGRErr eErr = OGRERR_NONE;
    double dfSemiMajor = poGeographicCRS->GetSemiMajor(&eErr);
    if (eErr != OGRERR_NONE)
        return CE_Failure;
    const double dfInvFlattening = poGeographicCRS->GetInvFlattening(&eErr);
    if (eErr != OGRERR_NONE)
        return CE_Failure;
    geod_init(&g, dfSemiMajor,
              dfInvFlattening != 0 ? 1.0 / dfInvFlattening : 0.0);

    std::array<double, 5> adfLon{};
    std::array<double, 5> adfLat{};

    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            gt.Apply(static_cast<double>(iCol + nXOff),
                     static_cast<double>(iLine + nYOff), &adfLon[0],
                     &adfLat[0]);
            gt.Apply(static_cast<double>(iCol + nXOff + 1),
                     static_cast<double>(iLine + nYOff), &adfLon[1],
                     &adfLat[1]);
            gt.Apply(static_cast<double>(iCol + nXOff + 1),
                     static_cast<double>(iLine + nYOff + 1), &adfLon[2],
                     &adfLat[2]);
            gt.Apply(static_cast<double>(iCol + nXOff),
                     static_cast<double>(iLine + nYOff + 1), &adfLon[3],
                     &adfLat[3]);
            adfLon[4] = adfLon[0];
            adfLat[4] = adfLat[0];

            if (poTransform &&
                !poTransform->Transform(adfLon.size(), adfLon.data(),
                                        adfLat.data(), nullptr))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to reproject cell corners to geographic CRS");
                return CE_Failure;
            }

            double dfArea = -1.0;
            geod_polygonarea(&g, adfLat.data(), adfLon.data(),
                             static_cast<int>(adfLat.size()), &dfArea, nullptr);
            dfArea = std::fabs(dfArea);

            GDALCopyWords(&dfArea, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    return CE_None;
}  // AreaPixelFunc

static const char pszReclassifyPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='mapping' "
    "             description='Lookup table for mapping, in format "
    "from=to,from=to' "
    "             type='string'></Argument>"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "</PixelFunctionArgumentsList>";

static CPLErr ReclassifyPixelFunc(void **papoSources, int nSources, void *pData,
                                  int nXSize, int nYSize, GDALDataType eSrcType,
                                  GDALDataType eBufType, int nPixelSpace,
                                  int nLineSpace, CSLConstList papszArgs)
{
    if (GDALDataTypeIsComplex(eSrcType))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "reclassify cannot by applied to complex data types");
        return CE_Failure;
    }

    if (nSources != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "reclassify only be applied to a single source at a time");
        return CE_Failure;
    }
    std::optional<double> noDataValue{};

    const char *pszNoData = CSLFetchNameValue(papszArgs, "NoData");
    if (pszNoData != nullptr)
    {
        noDataValue = CPLAtof(pszNoData);
    }

    const char *pszMappings = CSLFetchNameValue(papszArgs, "mapping");
    if (pszMappings == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "reclassify must be called with 'mapping' argument");
        return CE_Failure;
    }

    gdal::Reclassifier oReclassifier;
    if (auto eErr = oReclassifier.Init(pszMappings, noDataValue, eBufType);
        eErr != CE_None)
    {
        return eErr;
    }

    std::unique_ptr<double, VSIFreeReleaser> padfResults(
        static_cast<double *>(VSI_MALLOC2_VERBOSE(nXSize, sizeof(double))));
    if (!padfResults)
        return CE_Failure;

    size_t ii = 0;
    bool bSuccess = false;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            double srcVal = GetSrcVal(papoSources[0], eSrcType, ii);
            padfResults.get()[iCol] =
                oReclassifier.Reclassify(srcVal, bSuccess);
            if (!bSuccess)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Encountered value %g with no specified mapping",
                         srcVal);
                return CE_Failure;
            }
        }

        GDALCopyWords(padfResults.get(), GDT_Float64, sizeof(double),
                      static_cast<GByte *>(pData) +
                          static_cast<GSpacing>(nLineSpace) * iLine,
                      eBufType, nPixelSpace, nXSize);
    }

    return CE_None;
}  // ReclassifyPixelFunc

struct MeanKernel
{
    static constexpr const char *pszName = "mean";

    double dfMean = 0;
    int nValidSources = 0;

    void Reset()
    {
        dfMean = 0;
        nValidSources = 0;
    }

    static CPLErr ProcessArguments(CSLConstList)
    {
        return CE_None;
    }

    void ProcessPixel(double dfVal)
    {
        ++nValidSources;

        if (CPL_UNLIKELY(std::isinf(dfVal)))
        {
            if (nValidSources == 1)
            {
                dfMean = dfVal;
            }
            else if (dfVal == -dfMean)
            {
                dfMean = std::numeric_limits<double>::quiet_NaN();
            }
        }
        else if (CPL_UNLIKELY(std::isinf(dfMean)))
        {
            if (!std::isfinite(dfVal))
            {
                dfMean = std::numeric_limits<double>::quiet_NaN();
            }
        }
        else
        {
            const double delta = dfVal - dfMean;
            if (CPL_UNLIKELY(std::isinf(delta)))
                dfMean += dfVal / nValidSources - dfMean / nValidSources;
            else
                dfMean += delta / nValidSources;
        }
    }

    bool HasValue() const
    {
        return nValidSources > 0;
    }

    double GetValue() const
    {
        return dfMean;
    }
};

struct GeoMeanKernel
{
    static constexpr const char *pszName = "geometric_mean";

    double dfProduct = 1;
    int nValidSources = 0;

    void Reset()
    {
        dfProduct = 1;
        nValidSources = 0;
    }

    static CPLErr ProcessArguments(CSLConstList)
    {
        return CE_None;
    }

    void ProcessPixel(double dfVal)
    {
        dfProduct *= dfVal;
        nValidSources++;
    }

    bool HasValue() const
    {
        return nValidSources > 0;
    }

    double GetValue() const
    {
        return std::pow(dfProduct, 1.0 / nValidSources);
    }
};

struct HarmonicMeanKernel
{
    static constexpr const char *pszName = "harmonic_mean";

    double dfDenom = 0;
    int nValidSources = 0;
    bool bValueIsZero = false;
    bool bPropagateZero = false;

    void Reset()
    {
        dfDenom = 0;
        nValidSources = 0;
        bValueIsZero = false;
    }

    void ProcessPixel(double dfVal)
    {
        if (dfVal == 0)
        {
            bValueIsZero = true;
        }
        else
        {
            dfDenom += 1 / dfVal;
        }
        nValidSources++;
    }

    CPLErr ProcessArguments(CSLConstList papszArgs)
    {
        bPropagateZero =
            CPLTestBool(CSLFetchNameValueDef(papszArgs, "propagateZero", "0"));
        return CE_None;
    }

    bool HasValue() const
    {
        return dfDenom > 0 && (bPropagateZero || !bValueIsZero);
    }

    double GetValue() const
    {
        if (bPropagateZero && bValueIsZero)
        {
            return 0;
        }
        return static_cast<double>(nValidSources) / dfDenom;
    }
};

struct MedianKernel
{
    static constexpr const char *pszName = "median";

    mutable std::vector<double> values{};

    void Reset()
    {
        values.clear();
    }

    static CPLErr ProcessArguments(CSLConstList)
    {
        return CE_None;
    }

    void ProcessPixel(double dfVal)
    {
        if (!std::isnan(dfVal))
        {
            values.push_back(dfVal);
        }
    }

    bool HasValue() const
    {
        return !values.empty();
    }

    double GetValue() const
    {
        std::sort(values.begin(), values.end());
        if (values.size() % 2 == 0)
        {
            return 0.5 *
                   (values[values.size() / 2 - 1] + values[values.size() / 2]);
        }

        return values[values.size() / 2];
    }
};

struct QuantileKernel
{
    static constexpr const char *pszName = "quantile";

    mutable std::vector<double> values{};
    double q = 0.5;

    void Reset()
    {
        values.clear();
        // q intentionally preserved (set via ProcessArguments)
    }

    CPLErr ProcessArguments(CSLConstList papszArgs)
    {
        const char *pszQ = CSLFetchNameValue(papszArgs, "q");
        if (pszQ)
        {
            char *end;
            const double dq = CPLStrtod(pszQ, &end);
            while (isspace(*end))
            {
                end++;
            }
            if (*end != '\0' || dq < 0.0 || dq > 1.0 || std::isnan(dq))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "quantile: q must be between 0 and 1");
                return CE_Failure;
            }
            q = dq;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "quantile: q must be specified");
            return CE_Failure;
        }
        return CE_None;
    }

    void ProcessPixel(double dfVal)
    {
        if (!std::isnan(dfVal))
        {
            values.push_back(dfVal);
        }
    }

    bool HasValue() const
    {
        return !values.empty();
    }

    double GetValue() const
    {
        if (values.empty())
        {
            return std::numeric_limits<double>::quiet_NaN();
        }

        std::sort(values.begin(), values.end());
        const double loc = q * static_cast<double>(values.size() - 1);

        // Use formula from NumPy docs with default linear interpolation
        // g: fractional component of loc
        // j: integral component of loc
        double j;
        const double g = std::modf(loc, &j);

        if (static_cast<size_t>(j) + 1 == values.size())
        {
            return values[static_cast<size_t>(j)];
        }

        return (1 - g) * values[static_cast<size_t>(j)] +
               g * values[static_cast<size_t>(j) + 1];
    }
};

struct ModeKernel
{
    static constexpr const char *pszName = "mode";

    std::map<double, size_t> counts{};
    std::size_t nanCount{0};
    double dfMax = std::numeric_limits<double>::quiet_NaN();
    decltype(counts.begin()) oMax = counts.end();

    void Reset()
    {
        nanCount = 0;
        counts.clear();
        oMax = counts.end();
    }

    static CPLErr ProcessArguments(CSLConstList)
    {
        return CE_None;
    }

    void ProcessPixel(double dfVal)
    {
        if (std::isnan(dfVal))
        {
            nanCount += 1;
            return;
        }

        // if dfVal is NaN, try_emplace will return an entry for a different key!
        auto [it, inserted] = counts.try_emplace(dfVal, 0);

        it->second += 1;

        if (oMax == counts.end() || it->second > oMax->second)
        {
            oMax = it;
        }
    }

    bool HasValue() const
    {
        return nanCount > 0 || oMax != counts.end();
    }

    double GetValue() const
    {
        double ret = std::numeric_limits<double>::quiet_NaN();
        if (oMax != counts.end())
        {
            const size_t nCount = oMax->second;
            if (nCount > nanCount)
                ret = oMax->first;
        }
        return ret;
    }
};

static const char pszBasicPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "   <Argument name='propagateNoData' description='Whether the output value "
    "should be NoData as as soon as one source is NoData' type='boolean' "
    "default='false' />"
    "</PixelFunctionArgumentsList>";

static const char pszQuantilePixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='q' type='float' description='Quantile in [0,1]' />"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "   <Argument name='propagateNoData' type='boolean' default='false' />"
    "</PixelFunctionArgumentsList>";

#if defined(USE_SSE2) && !defined(USE_NEON_OPTIMIZATIONS)
inline __m128i packus_epi32(__m128i low, __m128i high)
{
#if __SSE4_1__
    return _mm_packus_epi32(low, high);  // Pack uint32 to uint16
#else
    low = _mm_add_epi32(low, _mm_set1_epi32(-32768));
    high = _mm_add_epi32(high, _mm_set1_epi32(-32768));
    return _mm_sub_epi16(_mm_packs_epi32(low, high), _mm_set1_epi16(-32768));
#endif
}
#endif

#ifdef USE_SSE2

template <class T, class SSEWrapper>
static void OptimizedMeanFloatSSE2(const void *const *papoSources, int nSources,
                                   void *pData, int nXSize, int nYSize,
                                   int nLineSpace)
{
    assert(nSources >= 1);
    constexpr int VALUES_PER_REG =
        static_cast<int>(sizeof(typename SSEWrapper::Vec) / sizeof(T));
    const T invSources = static_cast<T>(1.0) / static_cast<T>(nSources);
    const auto invSourcesSSE = SSEWrapper::Set1(invSources);
    const auto signMaskSSE = SSEWrapper::Set1(static_cast<T>(-0.0));
    const auto infSSE = SSEWrapper::Set1(std::numeric_limits<T>::infinity());
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        T *CPL_RESTRICT pDest =
            reinterpret_cast<T *>(static_cast<GByte *>(pData) +
                                  static_cast<GSpacing>(nLineSpace) * iLine);
        const size_t iOffsetLine = static_cast<size_t>(iLine) * nXSize;
        int iCol = 0;
        for (; iCol < nXSize - (2 * VALUES_PER_REG - 1);
             iCol += 2 * VALUES_PER_REG)
        {
            auto reg0 = SSEWrapper::LoadU(
                static_cast<const T * CPL_RESTRICT>(papoSources[0]) +
                iOffsetLine + iCol);
            auto reg1 = SSEWrapper::LoadU(
                static_cast<const T * CPL_RESTRICT>(papoSources[0]) +
                iOffsetLine + iCol + VALUES_PER_REG);
            for (int iSrc = 1; iSrc < nSources; ++iSrc)
            {
                const auto inputVal0 = SSEWrapper::LoadU(
                    static_cast<const T * CPL_RESTRICT>(papoSources[iSrc]) +
                    iOffsetLine + iCol);
                const auto inputVal1 = SSEWrapper::LoadU(
                    static_cast<const T * CPL_RESTRICT>(papoSources[iSrc]) +
                    iOffsetLine + iCol + VALUES_PER_REG);
                reg0 = SSEWrapper::Add(reg0, inputVal0);
                reg1 = SSEWrapper::Add(reg1, inputVal1);
            }
            reg0 = SSEWrapper::Mul(reg0, invSourcesSSE);
            reg1 = SSEWrapper::Mul(reg1, invSourcesSSE);

            // Detect infinity that could happen when summing huge
            // values
            if (SSEWrapper::MoveMask(SSEWrapper::Or(
                    SSEWrapper::CmpEq(SSEWrapper::AndNot(signMaskSSE, reg0),
                                      infSSE),
                    SSEWrapper::CmpEq(SSEWrapper::AndNot(signMaskSSE, reg1),
                                      infSSE))))
            {
                break;
            }

            SSEWrapper::StoreU(pDest + iCol, reg0);
            SSEWrapper::StoreU(pDest + iCol + VALUES_PER_REG, reg1);
        }

        // Use numerically stable mean computation
        for (; iCol < nXSize; ++iCol)
        {
            T mean = static_cast<const T * CPL_RESTRICT>(
                papoSources[0])[iOffsetLine + iCol];
            if (nSources >= 2)
            {
                T new_val = static_cast<const T * CPL_RESTRICT>(
                    papoSources[1])[iOffsetLine + iCol];
                if (CPL_UNLIKELY(std::isinf(new_val)))
                {
                    if (new_val == -mean)
                    {
                        pDest[iCol] = std::numeric_limits<T>::quiet_NaN();
                        continue;
                    }
                }
                else if (CPL_UNLIKELY(std::isinf(mean)))
                {
                    if (!std::isfinite(new_val))
                    {
                        pDest[iCol] = std::numeric_limits<T>::quiet_NaN();
                        continue;
                    }
                }
                else
                {
                    const T delta = new_val - mean;
                    if (CPL_UNLIKELY(std::isinf(delta)))
                        mean += new_val * static_cast<T>(0.5) -
                                mean * static_cast<T>(0.5);
                    else
                        mean += delta * static_cast<T>(0.5);
                }

                for (int iSrc = 2; iSrc < nSources; ++iSrc)
                {
                    new_val = static_cast<const T * CPL_RESTRICT>(
                        papoSources[iSrc])[iOffsetLine + iCol];
                    if (CPL_UNLIKELY(std::isinf(new_val)))
                    {
                        if (new_val == -mean)
                        {
                            mean = std::numeric_limits<T>::quiet_NaN();
                            break;
                        }
                    }
                    else if (CPL_UNLIKELY(std::isinf(mean)))
                    {
                        if (!std::isfinite(new_val))
                        {
                            mean = std::numeric_limits<T>::quiet_NaN();
                            break;
                        }
                    }
                    else
                    {
                        const T delta = new_val - mean;
                        if (CPL_UNLIKELY(std::isinf(delta)))
                            mean += new_val / static_cast<T>(iSrc + 1) -
                                    mean / static_cast<T>(iSrc + 1);
                        else
                            mean += delta / static_cast<T>(iSrc + 1);
                    }
                }
            }
            pDest[iCol] = mean;
        }
    }
}

// clang-format off
namespace
{
#ifdef __AVX2__
struct SSEWrapperFloat
{
    typedef __m256 Vec;

    static inline Vec Set1(float x) { return _mm256_set1_ps(x); }
    static inline Vec LoadU(const float *x) { return _mm256_loadu_ps(x); }
    static inline void StoreU(float *x, Vec y) { _mm256_storeu_ps(x, y); }
    static inline Vec Add(Vec x, Vec y) { return _mm256_add_ps(x, y); }
    static inline Vec Mul(Vec x, Vec y) { return _mm256_mul_ps(x, y); }
    static inline Vec Or(Vec x, Vec y) { return _mm256_or_ps(x, y); }
    static inline Vec AndNot(Vec x, Vec y) { return _mm256_andnot_ps(x, y); }
    static inline Vec CmpEq(Vec x, Vec y) { return _mm256_cmp_ps(x, y, _CMP_EQ_OQ); }
    static inline int MoveMask(Vec x) { return _mm256_movemask_ps(x); }
};

struct SSEWrapperDouble
{
    typedef __m256d Vec;

    static inline Vec Set1(double x) { return _mm256_set1_pd(x); }
    static inline Vec LoadU(const double *x) { return _mm256_loadu_pd(x); }
    static inline void StoreU(double *x, Vec y) { _mm256_storeu_pd(x, y); }
    static inline Vec Add(Vec x, Vec y) { return _mm256_add_pd(x, y); }
    static inline Vec Mul(Vec x, Vec y) { return _mm256_mul_pd(x, y); }
    static inline Vec Or(Vec x, Vec y) { return _mm256_or_pd(x, y); }
    static inline Vec AndNot(Vec x, Vec y) { return _mm256_andnot_pd(x, y); }
    static inline Vec CmpEq(Vec x, Vec y) { return _mm256_cmp_pd(x, y, _CMP_EQ_OQ); }
    static inline int MoveMask(Vec x) { return _mm256_movemask_pd(x); }
};

#else

struct SSEWrapperFloat
{
    typedef __m128 Vec;

    static inline Vec Set1(float x) { return _mm_set1_ps(x); }
    static inline Vec LoadU(const float *x) { return _mm_loadu_ps(x); }
    static inline void StoreU(float *x, Vec y) { _mm_storeu_ps(x, y); }
    static inline Vec Add(Vec x, Vec y) { return _mm_add_ps(x, y); }
    static inline Vec Mul(Vec x, Vec y) { return _mm_mul_ps(x, y); }
    static inline Vec Or(Vec x, Vec y) { return _mm_or_ps(x, y); }
    static inline Vec AndNot(Vec x, Vec y) { return _mm_andnot_ps(x, y); }
    static inline Vec CmpEq(Vec x, Vec y) { return _mm_cmpeq_ps(x, y); }
    static inline int MoveMask(Vec x) { return _mm_movemask_ps(x); }
};

struct SSEWrapperDouble
{
    typedef __m128d Vec;

    static inline Vec Set1(double x) { return _mm_set1_pd(x); }
    static inline Vec LoadU(const double *x) { return _mm_loadu_pd(x); }
    static inline void StoreU(double *x, Vec y) { _mm_storeu_pd(x, y); }
    static inline Vec Add(Vec x, Vec y) { return _mm_add_pd(x, y); }
    static inline Vec Mul(Vec x, Vec y) { return _mm_mul_pd(x, y); }
    static inline Vec Or(Vec x, Vec y) { return _mm_or_pd(x, y); }
    static inline Vec AndNot(Vec x, Vec y) { return _mm_andnot_pd(x, y); }
    static inline Vec CmpEq(Vec x, Vec y) { return _mm_cmpeq_pd(x, y); }
    static inline int MoveMask(Vec x) { return _mm_movemask_pd(x); }
};
#endif
}  // namespace

// clang-format on

#endif  // USE_SSE2

template <typename Kernel>
static CPLErr BasicPixelFunc(void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize, GDALDataType eSrcType,
                             GDALDataType eBufType, int nPixelSpace,
                             int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    Kernel oKernel;

    if (GDALDataTypeIsComplex(eSrcType))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Complex data types not supported by %s", oKernel.pszName);
        return CE_Failure;
    }

    double dfNoData{0};
    const bool bHasNoData = CSLFindName(papszArgs, "NoData") != -1;
    if (bHasNoData && FetchDoubleArg(papszArgs, "NoData", &dfNoData) != CE_None)
        return CE_Failure;

    const bool bPropagateNoData = CPLTestBool(
        CSLFetchNameValueDef(papszArgs, "propagateNoData", "false"));

    if (oKernel.ProcessArguments(papszArgs) == CE_Failure)
    {
        return CE_Failure;
    }

#if defined(USE_SSE2) && !defined(USE_NEON_OPTIMIZATIONS)
    if constexpr (std::is_same_v<Kernel, MeanKernel>)
    {
        if (!bHasNoData && eSrcType == GDT_UInt8 && eBufType == GDT_UInt8 &&
            nPixelSpace == 1 &&
            // We use signed int16 to accumulate
            nSources <= std::numeric_limits<int16_t>::max() /
                            std::numeric_limits<uint8_t>::max())
        {
            using T = uint8_t;
            constexpr int VALUES_PER_REG = 16;
            if (nSources == 2)
            {
                for (int iLine = 0; iLine < nYSize; ++iLine)
                {
                    T *CPL_RESTRICT pDest = reinterpret_cast<T *>(
                        static_cast<GByte *>(pData) +
                        static_cast<GSpacing>(nLineSpace) * iLine);
                    const size_t iOffsetLine =
                        static_cast<size_t>(iLine) * nXSize;
                    int iCol = 0;
                    for (; iCol < nXSize - (VALUES_PER_REG - 1);
                         iCol += VALUES_PER_REG)
                    {
                        const __m128i inputVal0 =
                            _mm_loadu_si128(reinterpret_cast<const __m128i *>(
                                static_cast<const T * CPL_RESTRICT>(
                                    papoSources[0]) +
                                iOffsetLine + iCol));
                        const __m128i inputVal1 =
                            _mm_loadu_si128(reinterpret_cast<const __m128i *>(
                                static_cast<const T * CPL_RESTRICT>(
                                    papoSources[1]) +
                                iOffsetLine + iCol));
                        _mm_storeu_si128(
                            reinterpret_cast<__m128i *>(pDest + iCol),
                            _mm_avg_epu8(inputVal0, inputVal1));
                    }
                    for (; iCol < nXSize; ++iCol)
                    {
                        uint32_t acc = 1 +
                                       static_cast<const T * CPL_RESTRICT>(
                                           papoSources[0])[iOffsetLine + iCol] +
                                       static_cast<const T * CPL_RESTRICT>(
                                           papoSources[1])[iOffsetLine + iCol];
                        pDest[iCol] = static_cast<T>(acc / 2);
                    }
                }
            }
            else
            {
                libdivide::divider<uint16_t> fast_d(
                    static_cast<uint16_t>(nSources));
                const auto halfConstant =
                    _mm_set1_epi16(static_cast<int16_t>(nSources / 2));
                for (int iLine = 0; iLine < nYSize; ++iLine)
                {
                    T *CPL_RESTRICT pDest =
                        static_cast<GByte *>(pData) +
                        static_cast<GSpacing>(nLineSpace) * iLine;
                    const size_t iOffsetLine =
                        static_cast<size_t>(iLine) * nXSize;
                    int iCol = 0;
                    for (; iCol < nXSize - (VALUES_PER_REG - 1);
                         iCol += VALUES_PER_REG)
                    {
                        __m128i reg0 = halfConstant;
                        __m128i reg1 = halfConstant;
                        for (int iSrc = 0; iSrc < nSources; ++iSrc)
                        {
                            const __m128i inputVal = _mm_loadu_si128(
                                reinterpret_cast<const __m128i *>(
                                    static_cast<const T * CPL_RESTRICT>(
                                        papoSources[iSrc]) +
                                    iOffsetLine + iCol));
                            reg0 = _mm_add_epi16(
                                reg0, _mm_unpacklo_epi8(inputVal,
                                                        _mm_setzero_si128()));
                            reg1 = _mm_add_epi16(
                                reg1, _mm_unpackhi_epi8(inputVal,
                                                        _mm_setzero_si128()));
                        }
                        reg0 /= fast_d;
                        reg1 /= fast_d;
                        _mm_storeu_si128(
                            reinterpret_cast<__m128i *>(pDest + iCol),
                            _mm_packus_epi16(reg0, reg1));
                    }
                    for (; iCol < nXSize; ++iCol)
                    {
                        uint32_t acc = nSources / 2;
                        for (int iSrc = 0; iSrc < nSources; ++iSrc)
                        {
                            acc += static_cast<const T * CPL_RESTRICT>(
                                papoSources[iSrc])[iOffsetLine + iCol];
                        }
                        pDest[iCol] = static_cast<T>(acc / nSources);
                    }
                }
            }
            return CE_None;
        }

        if (!bHasNoData && eSrcType == GDT_UInt8 && eBufType == GDT_UInt8 &&
            nPixelSpace == 1 &&
            // We use signed int32 to accumulate
            nSources <= std::numeric_limits<int32_t>::max() /
                            std::numeric_limits<uint8_t>::max())
        {
            using T = uint8_t;
            constexpr int VALUES_PER_REG = 16;
            libdivide::divider<uint32_t> fast_d(nSources);
            const auto halfConstant = _mm_set1_epi32(nSources / 2);
            for (int iLine = 0; iLine < nYSize; ++iLine)
            {
                T *CPL_RESTRICT pDest =
                    static_cast<GByte *>(pData) +
                    static_cast<GSpacing>(nLineSpace) * iLine;
                const size_t iOffsetLine = static_cast<size_t>(iLine) * nXSize;
                int iCol = 0;
                for (; iCol < nXSize - (VALUES_PER_REG - 1);
                     iCol += VALUES_PER_REG)
                {
                    __m128i reg0 = halfConstant;
                    __m128i reg1 = halfConstant;
                    __m128i reg2 = halfConstant;
                    __m128i reg3 = halfConstant;
                    for (int iSrc = 0; iSrc < nSources; ++iSrc)
                    {
                        const __m128i inputVal =
                            _mm_loadu_si128(reinterpret_cast<const __m128i *>(
                                static_cast<const T * CPL_RESTRICT>(
                                    papoSources[iSrc]) +
                                iOffsetLine + iCol));
                        const __m128i low =
                            _mm_unpacklo_epi8(inputVal, _mm_setzero_si128());
                        const __m128i high =
                            _mm_unpackhi_epi8(inputVal, _mm_setzero_si128());
                        reg0 = _mm_add_epi32(
                            reg0, _mm_unpacklo_epi16(low, _mm_setzero_si128()));
                        reg1 = _mm_add_epi32(
                            reg1, _mm_unpackhi_epi16(low, _mm_setzero_si128()));
                        reg2 = _mm_add_epi32(
                            reg2,
                            _mm_unpacklo_epi16(high, _mm_setzero_si128()));
                        reg3 = _mm_add_epi32(
                            reg3,
                            _mm_unpackhi_epi16(high, _mm_setzero_si128()));
                    }
                    reg0 /= fast_d;
                    reg1 /= fast_d;
                    reg2 /= fast_d;
                    reg3 /= fast_d;
                    _mm_storeu_si128(
                        reinterpret_cast<__m128i *>(pDest + iCol),
                        _mm_packus_epi16(packus_epi32(reg0, reg1),
                                         packus_epi32(reg2, reg3)));
                }
                for (; iCol < nXSize; ++iCol)
                {
                    uint32_t acc = nSources / 2;
                    for (int iSrc = 0; iSrc < nSources; ++iSrc)
                    {
                        acc += static_cast<const T * CPL_RESTRICT>(
                            papoSources[iSrc])[iOffsetLine + iCol];
                    }
                    pDest[iCol] = static_cast<T>(acc / nSources);
                }
            }
            return CE_None;
        }

        if (!bHasNoData && eSrcType == GDT_UInt16 && eBufType == GDT_UInt16 &&
            nPixelSpace == 2 &&
            nSources <= std::numeric_limits<int32_t>::max() /
                            std::numeric_limits<uint16_t>::max())
        {
            libdivide::divider<uint32_t> fast_d(nSources);
            using T = uint16_t;
            const auto halfConstant = _mm_set1_epi32(nSources / 2);
            constexpr int VALUES_PER_REG = 8;
            for (int iLine = 0; iLine < nYSize; ++iLine)
            {
                T *CPL_RESTRICT pDest = reinterpret_cast<T *>(
                    static_cast<GByte *>(pData) +
                    static_cast<GSpacing>(nLineSpace) * iLine);
                const size_t iOffsetLine = static_cast<size_t>(iLine) * nXSize;
                int iCol = 0;
                for (; iCol < nXSize - (VALUES_PER_REG - 1);
                     iCol += VALUES_PER_REG)
                {
                    __m128i reg0 = halfConstant;
                    __m128i reg1 = halfConstant;
                    for (int iSrc = 0; iSrc < nSources; ++iSrc)
                    {
                        const __m128i inputVal =
                            _mm_loadu_si128(reinterpret_cast<const __m128i *>(
                                static_cast<const T * CPL_RESTRICT>(
                                    papoSources[iSrc]) +
                                iOffsetLine + iCol));
                        reg0 = _mm_add_epi32(
                            reg0,
                            _mm_unpacklo_epi16(inputVal, _mm_setzero_si128()));
                        reg1 = _mm_add_epi32(
                            reg1,
                            _mm_unpackhi_epi16(inputVal, _mm_setzero_si128()));
                    }
                    reg0 /= fast_d;
                    reg1 /= fast_d;
                    _mm_storeu_si128(reinterpret_cast<__m128i *>(pDest + iCol),
                                     packus_epi32(reg0, reg1));
                }
                for (; iCol < nXSize; ++iCol)
                {
                    uint32_t acc = nSources / 2;
                    for (int iSrc = 0; iSrc < nSources; ++iSrc)
                    {
                        acc += static_cast<const T * CPL_RESTRICT>(
                            papoSources[iSrc])[iOffsetLine + iCol];
                    }
                    pDest[iCol] = static_cast<T>(acc / nSources);
                }
            }
            return CE_None;
        }

        if (!bHasNoData && eSrcType == GDT_Int16 && eBufType == GDT_Int16 &&
            nPixelSpace == 2 &&
            nSources <= std::numeric_limits<int32_t>::max() /
                            std::numeric_limits<uint16_t>::max())
        {
            libdivide::divider<uint32_t> fast_d(nSources);
            using T = int16_t;
            const auto halfConstant = _mm_set1_epi32(nSources / 2);
            const auto shift = _mm_set1_epi16(std::numeric_limits<T>::min());
            constexpr int VALUES_PER_REG = 8;
            for (int iLine = 0; iLine < nYSize; ++iLine)
            {
                T *CPL_RESTRICT pDest = reinterpret_cast<T *>(
                    static_cast<GByte *>(pData) +
                    static_cast<GSpacing>(nLineSpace) * iLine);
                const size_t iOffsetLine = static_cast<size_t>(iLine) * nXSize;
                int iCol = 0;
                for (; iCol < nXSize - (VALUES_PER_REG - 1);
                     iCol += VALUES_PER_REG)
                {
                    __m128i reg0 = halfConstant;
                    __m128i reg1 = halfConstant;
                    for (int iSrc = 0; iSrc < nSources; ++iSrc)
                    {
                        // Shift input values by 32768 to get unsigned values
                        const __m128i inputVal = _mm_add_epi16(
                            _mm_loadu_si128(reinterpret_cast<const __m128i *>(
                                static_cast<const T * CPL_RESTRICT>(
                                    papoSources[iSrc]) +
                                iOffsetLine + iCol)),
                            shift);
                        reg0 = _mm_add_epi32(
                            reg0,
                            _mm_unpacklo_epi16(inputVal, _mm_setzero_si128()));
                        reg1 = _mm_add_epi32(
                            reg1,
                            _mm_unpackhi_epi16(inputVal, _mm_setzero_si128()));
                    }
                    reg0 /= fast_d;
                    reg1 /= fast_d;
                    _mm_storeu_si128(
                        reinterpret_cast<__m128i *>(pDest + iCol),
                        _mm_add_epi16(packus_epi32(reg0, reg1), shift));
                }
                for (; iCol < nXSize; ++iCol)
                {
                    int32_t acc = (-std::numeric_limits<T>::min()) * nSources +
                                  nSources / 2;
                    for (int iSrc = 0; iSrc < nSources; ++iSrc)
                    {
                        acc += static_cast<const T * CPL_RESTRICT>(
                            papoSources[iSrc])[iOffsetLine + iCol];
                    }
                    pDest[iCol] = static_cast<T>(acc / nSources +
                                                 std::numeric_limits<T>::min());
                }
            }
            return CE_None;
        }
    }
#endif  // defined(USE_SSE2) && !defined(USE_NEON_OPTIMIZATIONS)

#if defined(USE_SSE2)
    if constexpr (std::is_same_v<Kernel, MeanKernel>)
    {
        if (!bHasNoData && eSrcType == GDT_Float32 && eBufType == GDT_Float32 &&
            nPixelSpace == 4 && nSources > 0)
        {
            OptimizedMeanFloatSSE2<float, SSEWrapperFloat>(
                papoSources, nSources, pData, nXSize, nYSize, nLineSpace);
            return CE_None;
        }

        if (!bHasNoData && eSrcType == GDT_Float64 && eBufType == GDT_Float64 &&
            nPixelSpace == 8 && nSources > 0)
        {
            OptimizedMeanFloatSSE2<double, SSEWrapperDouble>(
                papoSources, nSources, pData, nXSize, nYSize, nLineSpace);
            return CE_None;
        }
    }
#endif  // USE_SSE2

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for (int iLine = 0; iLine < nYSize; ++iLine)
    {
        for (int iCol = 0; iCol < nXSize; ++iCol, ++ii)
        {
            oKernel.Reset();
            bool bWriteNoData = false;

            for (int iSrc = 0; iSrc < nSources; ++iSrc)
            {
                const double dfVal = GetSrcVal(papoSources[iSrc], eSrcType, ii);

                if (bHasNoData && IsNoData(dfVal, dfNoData))
                {
                    if (bPropagateNoData)
                    {
                        bWriteNoData = true;
                        break;
                    }
                }
                else
                {
                    oKernel.ProcessPixel(dfVal);
                }
            }

            double dfPixVal{dfNoData};
            if (!bWriteNoData && oKernel.HasValue())
            {
                dfPixVal = oKernel.GetValue();
            }

            GDALCopyWords(&dfPixVal, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GSpacing>(nLineSpace) * iLine +
                              iCol * nPixelSpace,
                          eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // BasicPixelFunc

/************************************************************************/
/*                    GDALRegisterDefaultPixelFunc()                    */
/************************************************************************/

/**
 * This adds a default set of pixel functions to the global list of
 * available pixel functions for derived bands:
 *
 * - "real": extract real part from a single raster band (just a copy if the
 *           input is non-complex)
 * - "imag": extract imaginary part from a single raster band (0 for
 *           non-complex)
 * - "complex": make a complex band merging two bands used as real and
 *              imag values
 * - "polar": make a complex band using input bands for amplitude and
 *            phase values (b1 * exp( j * b2 ))
 * - "mod": extract module from a single raster band (real or complex)
 * - "phase": extract phase from a single raster band [-PI,PI] (0 or PI for
              non-complex)
 * - "conj": computes the complex conjugate of a single raster band (just a
 *           copy if the input is non-complex)
 * - "sum": sum 2 or more raster bands
 * - "diff": computes the difference between 2 raster bands (b1 - b2)
 * - "mul": multiply 2 or more raster bands
 * - "div": divide one raster band by another (b1 / b2).
 * - "min": minimum value of 2 or more raster bands
 * - "max": maximum value of 2 or more raster bands
 * - "norm_diff": computes the normalized difference between two raster bands:
 *                ``(b1 - b2)/(b1 + b2)``.
 * - "cmul": multiply the first band for the complex conjugate of the second
 * - "inv": inverse (1./x).
 * - "intensity": computes the intensity Re(x*conj(x)) of a single raster band
 *                (real or complex)
 * - "sqrt": perform the square root of a single raster band (real only)
 * - "log10": compute the logarithm (base 10) of the abs of a single raster
 *            band (real or complex): log10( abs( x ) )
 * - "dB": perform conversion to dB of the abs of a single raster
 *         band (real or complex): 20. * log10( abs( x ) ).
 *         Note: the optional fact parameter can be set to 10. to get the
 *         alternative formula: 10. * log10( abs( x ) )
 * - "exp": computes the exponential of each element in the input band ``x``
 *          (of real values): ``e ^ x``.
 *          The function also accepts two optional parameters: ``base`` and
 ``fact``
 *          that allow to compute the generalized formula: ``base ^ ( fact *
 x)``.
 *          Note: this function is the recommended one to perform conversion
 *          form logarithmic scale (dB): `` 10. ^ (x / 20.)``, in this case
 *          ``base = 10.`` and ``fact = 1./20``
 * - "dB2amp": perform scale conversion from logarithmic to linear
 *             (amplitude) (i.e. 10 ^ ( x / 20 ) ) of a single raster
 *             band (real only).
 *             Deprecated in GDAL v3.5. Please use the ``exp`` pixel function
 with
 *             ``base = 10.`` and ``fact = 0.05`` i.e. ``1./20``
 * - "dB2pow": perform scale conversion from logarithmic to linear
 *             (power) (i.e. 10 ^ ( x / 10 ) ) of a single raster
 *             band (real only)
 *             Deprecated in GDAL v3.5. Please use the ``exp`` pixel function
 with
 *             ``base = 10.`` and ``fact = 0.1`` i.e. ``1./10``
 * - "pow": raise a single raster band to a constant power
 * - "interpolate_linear": interpolate values between two raster bands
 *                         using linear interpolation
 * - "interpolate_exp": interpolate values between two raster bands using
 *                      exponential interpolation
 * - "scale": Apply the RasterBand metadata values of "offset" and "scale"
 * - "reclassify": Reclassify values matching ranges in a table
 * - "nan": Convert incoming NoData values to IEEE 754 nan
 *
 * @see GDALAddDerivedBandPixelFunc
 *
 * @return CE_None
 */
CPLErr GDALRegisterDefaultPixelFunc()
{
    GDALAddDerivedBandPixelFunc("real", RealPixelFunc);
    GDALAddDerivedBandPixelFunc("imag", ImagPixelFunc);
    GDALAddDerivedBandPixelFunc("complex", ComplexPixelFunc);
    GDALAddDerivedBandPixelFuncWithArgs("polar", PolarPixelFunc,
                                        pszPolarPixelFuncMetadata);
    GDALAddDerivedBandPixelFunc("mod", ModulePixelFunc);
    GDALAddDerivedBandPixelFunc("phase", PhasePixelFunc);
    GDALAddDerivedBandPixelFunc("conj", ConjPixelFunc);
    GDALAddDerivedBandPixelFuncWithArgs("sum", SumPixelFunc,
                                        pszSumPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("diff", DiffPixelFunc,
                                        pszDiffPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("mul", MulPixelFunc,
                                        pszMulPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("div", DivPixelFunc,
                                        pszDivPixelFuncMetadata);
    GDALAddDerivedBandPixelFunc("cmul", CMulPixelFunc);
    GDALAddDerivedBandPixelFuncWithArgs("inv", InvPixelFunc,
                                        pszInvPixelFuncMetadata);
    GDALAddDerivedBandPixelFunc("intensity", IntensityPixelFunc);
    GDALAddDerivedBandPixelFuncWithArgs("sqrt", SqrtPixelFunc,
                                        pszSqrtPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("log10", Log10PixelFunc,
                                        pszLog10PixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("dB", DBPixelFunc,
                                        pszDBPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("exp", ExpPixelFunc,
                                        pszExpPixelFuncMetadata);
    GDALAddDerivedBandPixelFunc("dB2amp",
                                dB2AmpPixelFunc);  // deprecated in v3.5
    GDALAddDerivedBandPixelFunc("dB2pow",
                                dB2PowPixelFunc);  // deprecated in v3.5
    GDALAddDerivedBandPixelFuncWithArgs("pow", PowPixelFunc,
                                        pszPowPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("interpolate_linear",
                                        InterpolatePixelFunc<InterpolateLinear>,
                                        pszInterpolatePixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs(
        "interpolate_exp", InterpolatePixelFunc<InterpolateExponential>,
        pszInterpolatePixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("replace_nodata",
                                        ReplaceNoDataPixelFunc,
                                        pszReplaceNoDataPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("scale", ScalePixelFunc,
                                        pszScalePixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("norm_diff", NormDiffPixelFunc,
                                        pszNormDiffPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("min", MinPixelFunc<ReturnValue>,
                                        pszMinMaxFuncMetadataNodata);
    GDALAddDerivedBandPixelFuncWithArgs("argmin", MinPixelFunc<ReturnIndex>,
                                        pszArgMinMaxFuncMetadataNodata);
    GDALAddDerivedBandPixelFuncWithArgs("max", MaxPixelFunc<ReturnValue>,
                                        pszMinMaxFuncMetadataNodata);
    GDALAddDerivedBandPixelFuncWithArgs("argmax", MaxPixelFunc<ReturnIndex>,
                                        pszArgMinMaxFuncMetadataNodata);
    GDALAddDerivedBandPixelFuncWithArgs("expression", ExprPixelFunc,
                                        pszExprPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("reclassify", ReclassifyPixelFunc,
                                        pszReclassifyPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("round", RoundPixelFunc,
                                        pszRoundPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("mean", BasicPixelFunc<MeanKernel>,
                                        pszBasicPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("geometric_mean",
                                        BasicPixelFunc<GeoMeanKernel>,
                                        pszBasicPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("harmonic_mean",
                                        BasicPixelFunc<HarmonicMeanKernel>,
                                        pszBasicPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("median", BasicPixelFunc<MedianKernel>,
                                        pszBasicPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("quantile",
                                        BasicPixelFunc<QuantileKernel>,
                                        pszQuantilePixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("mode", BasicPixelFunc<ModeKernel>,
                                        pszBasicPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("area", AreaPixelFunc,
                                        pszAreaPixelFuncMetadata);
    return CE_None;
}
