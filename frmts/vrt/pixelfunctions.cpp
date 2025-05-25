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

#include <cmath>
#include "gdal.h"
#include "vrtdataset.h"
#include "vrtexpression.h"
#include "vrtreclassifier.h"
#include "cpl_float.h"

#if defined(__x86_64) || defined(_M_X64) || defined(USE_NEON_OPTIMIZATIONS)
#define USE_SSE2
#include "gdalsse_priv.h"
#endif

#include "gdal_priv_templates.hpp"

#include <limits>

template <typename T>
inline double GetSrcVal(const void *pSource, GDALDataType eSrcType, T ii)
{
    switch (eSrcType)
    {
        case GDT_Unknown:
            return 0;
        case GDT_Byte:
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

#ifdef USE_SSE2

/************************************************************************/
/*                        OptimizedSumToFloat_SSE2()                    */
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
/*                       OptimizedSumToDouble_SSE2()                    */
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

#endif  // USE_SSE2

/************************************************************************/
/*                       OptimizedSumPackedOutput()                     */
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
/*                       OptimizedSumPackedOutput()                     */
/************************************************************************/

template <typename Tdest>
static bool OptimizedSumPackedOutput(GDALDataType eSrcType, double dfK,
                                     void *pOutBuffer, int nLineSpace,
                                     int nXSize, int nYSize, int nSources,
                                     const void *const *papoSources)
{
    switch (eSrcType)
    {
        case GDT_Byte:
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
/*                    OptimizedSumThroughLargerType()                   */
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
         std::is_same_v<Tsrc, uint16_t>)&&(std::is_same_v<Tdest, uint8_t> ||
                                           std::is_same_v<Tdest, int16_t> ||
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
/*                     OptimizedSumThroughLargerType()                  */
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
        case GDT_Byte:
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
/*                    OptimizedSumThroughLargerType()                   */
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
        case GDT_Byte:
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
                nPixelSpace == sizeof(int32_t) && eSrcType == GDT_Byte &&
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
                    dfPixVal = dfDenom == 0
                                   ? std::numeric_limits<double>::infinity()
                                   : dfNum / dfDenom;
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
                    dfPixVal = dfVal == 0
                                   ? std::numeric_limits<double>::infinity()
                                   : dfK / dfVal;
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
                dfPixVal = dfDenom == 0
                               ? std::numeric_limits<double>::infinity()
                               : (dfLeftVal - dfRightVal) / dfDenom;
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
/*                   pszMinMaxFuncMetadataNodata                        */
/************************************************************************/

static const char pszMinMaxFuncMetadataNodata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument type='builtin' value='NoData' optional='true' />"
    "   <Argument name='propagateNoData' description='Whether the output value "
    "should be NoData as as soon as one source is NoData' type='boolean' "
    "default='false' />"
    "</PixelFunctionArgumentsList>";

template <class Comparator>
static CPLErr MinOrMaxPixelFunc(void **papoSources, int nSources, void *pData,
                                int nXSize, int nYSize, GDALDataType eSrcType,
                                GDALDataType eBufType, int nPixelSpace,
                                int nLineSpace, CSLConstList papszArgs)
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

            for (int iSrc = 0; iSrc < nSources; ++iSrc)
            {
                const double dfVal = GetSrcVal(papoSources[iSrc], eSrcType, ii);

                if (std::isnan(dfVal) || dfVal == dfNoData)
                {
                    if (bPropagateNoData)
                    {
                        dfRes = dfNoData;
                        break;
                    }
                }
                else if (Comparator::compare(dfVal, dfRes))
                {
                    dfRes = dfVal;
                }
            }

            if (!bPropagateNoData && std::isnan(dfRes))
            {
                dfRes = dfNoData;
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

    return MinOrMaxPixelFunc<Comparator>(papoSources, nSources, pData, nXSize,
                                         nYSize, eSrcType, eBufType,
                                         nPixelSpace, nLineSpace, papszArgs);
}

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

    return MinOrMaxPixelFunc<Comparator>(papoSources, nSources, pData, nXSize,
                                         nYSize, eSrcType, eBufType,
                                         nPixelSpace, nLineSpace, papszArgs);
}

static const char pszExprPixelFuncMetadata[] =
    "<PixelFunctionArgumentsList>"
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

    std::unique_ptr<gdal::MathExpression> poExpression;

    const char *pszExpression = CSLFetchNameValue(papszArgs, "expression");

    const char *pszSourceNames = CSLFetchNameValue(papszArgs, "source_names");
    const CPLStringList aosSourceNames(
        CSLTokenizeString2(pszSourceNames, "|", 0));

    std::vector<double> adfValuesForPixel(nSources);

    const char *pszDialect = CSLFetchNameValue(papszArgs, "dialect");
    if (!pszDialect)
    {
        pszDialect = "muparser";
    }

    poExpression = gdal::MathExpression::Create(pszExpression, pszDialect);

    // cppcheck-suppress knownConditionTrueFalse
    if (!poExpression)
    {
        return CE_Failure;
    }

    {
        int iSource = 0;
        for (const auto &osName : aosSourceNames)
        {
            poExpression->RegisterVariable(osName,
                                           &adfValuesForPixel[iSource++]);
        }
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
            for (int iSrc = 0; iSrc < nSources; iSrc++)
            {
                // cppcheck-suppress unreadVariable
                adfValuesForPixel[iSrc] =
                    GetSrcVal(papoSources[iSrc], eSrcType, ii);
            }

            if (auto eErr = poExpression->Evaluate(); eErr != CE_None)
            {
                return CE_Failure;
            }
            else
            {
                padfResults.get()[iCol] = poExpression->Results()[0];
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

    double dfSum = 0;
    int nValidSources = 0;

    void Reset()
    {
        dfSum = 0;
        nValidSources = 0;
    }

    static CPLErr ProcessArguments(CSLConstList)
    {
        return CE_None;
    }

    void ProcessPixel(double dfVal)
    {
        dfSum += dfVal;
        nValidSources++;
    }

    bool HasValue() const
    {
        return nValidSources > 0;
    }

    double GetValue() const
    {
        return dfSum / nValidSources;
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

template <typename T>
static CPLErr BasicPixelFunc(void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize, GDALDataType eSrcType,
                             GDALDataType eBufType, int nPixelSpace,
                             int nLineSpace, CSLConstList papszArgs)
{
    /* ---- Init ---- */
    T oKernel;

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
/*                     GDALRegisterDefaultPixelFunc()                   */
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
    GDALAddDerivedBandPixelFuncWithArgs("min", MinPixelFunc,
                                        pszMinMaxFuncMetadataNodata);
    GDALAddDerivedBandPixelFuncWithArgs("max", MaxPixelFunc,
                                        pszMinMaxFuncMetadataNodata);
    GDALAddDerivedBandPixelFuncWithArgs("expression", ExprPixelFunc,
                                        pszExprPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("reclassify", ReclassifyPixelFunc,
                                        pszReclassifyPixelFuncMetadata);
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
    GDALAddDerivedBandPixelFuncWithArgs("mode", BasicPixelFunc<ModeKernel>,
                                        pszBasicPixelFuncMetadata);
    return CE_None;
}
