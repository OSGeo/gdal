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
 *****************************************************************************/

#include <cmath>
#include "gdal.h"
#include "vrtdataset.h"

#include <limits>

CPL_CVSID("$Id$")

template<typename T> inline double GetSrcVal(const void* pSource, GDALDataType eSrcType, T ii)
{
    switch( eSrcType )
    {
        case GDT_Unknown: return 0;
        case GDT_Byte: return static_cast<const GByte*>(pSource)[ii];
        case GDT_UInt16: return static_cast<const GUInt16*>(pSource)[ii];
        case GDT_Int16: return static_cast<const GInt16*>(pSource)[ii];
        case GDT_UInt32: return static_cast<const GUInt32*>(pSource)[ii];
        case GDT_Int32: return static_cast<const GInt32*>(pSource)[ii];
        // Precision loss currently for int64/uint64
        case GDT_UInt64: return static_cast<double>(static_cast<const uint64_t*>(pSource)[ii]);
        case GDT_Int64: return static_cast<double>(static_cast<const int64_t*>(pSource)[ii]);
        case GDT_Float32: return static_cast<const float*>(pSource)[ii];
        case GDT_Float64: return static_cast<const double*>(pSource)[ii];
        case GDT_CInt16: return static_cast<const GInt16*>(pSource)[2 * ii];
        case GDT_CInt32: return static_cast<const GInt32*>(pSource)[2 * ii];
        case GDT_CFloat32: return static_cast<const float*>(pSource)[2 * ii];
        case GDT_CFloat64: return static_cast<const double*>(pSource)[2 * ii];
        case GDT_TypeCount: break;
    }
    return 0;
}

static CPLErr FetchDoubleArg(CSLConstList papszArgs, const char *pszName,
                             double* pdfX, double* pdfDefault = nullptr)
{
    const char* pszVal = CSLFetchNameValue(papszArgs, pszName);

    if ( pszVal == nullptr )
    {
        if ( pdfDefault == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing pixel function argument: %s", pszName);
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
    if ( pszEnd == pszVal )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to parse pixel function argument: %s", pszName);
        return CE_Failure;
    }

    return CE_None;
}

static CPLErr RealPixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace )
{
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;

    const int nPixelSpaceSrc = GDALGetDataTypeSizeBytes( eSrcType );
    const size_t nLineSpaceSrc = static_cast<size_t>(nPixelSpaceSrc) * nXSize;

    /* ---- Set pixels ---- */
    for( int iLine = 0; iLine < nYSize; ++iLine ) {
        GDALCopyWords(
            static_cast<GByte *>(papoSources[0]) + nLineSpaceSrc * iLine,
            eSrcType, nPixelSpaceSrc,
            static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine,
            eBufType, nPixelSpace, nXSize );
    }

    /* ---- Return success ---- */
    return CE_None;
}  // RealPixelFunc

static CPLErr ImagPixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace )
{
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;

    if( GDALDataTypeIsComplex( eSrcType ) )
    {
        const GDALDataType eSrcBaseType = GDALGetNonComplexDataType( eSrcType );
        const int nPixelSpaceSrc = GDALGetDataTypeSizeBytes( eSrcType );
        const size_t nLineSpaceSrc = static_cast<size_t>(nPixelSpaceSrc) * nXSize;

        const void * const pImag = static_cast<GByte *>(papoSources[0])
            + GDALGetDataTypeSizeBytes( eSrcType ) / 2;

        /* ---- Set pixels ---- */
        for( int iLine = 0; iLine < nYSize; ++iLine )
        {
            GDALCopyWords(
                static_cast<const GByte *>(pImag) + nLineSpaceSrc * iLine,
                eSrcBaseType, nPixelSpaceSrc,
                static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine,
                eBufType, nPixelSpace, nXSize );
        }
    }
    else
    {
        const double dfImag = 0;

        /* ---- Set pixels ---- */
        for( int iLine = 0; iLine < nYSize; ++iLine )
        {
            // Always copy from the same location.
            GDALCopyWords(
                &dfImag, eSrcType, 0,
                static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine,
                eBufType, nPixelSpace, nXSize);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // ImagPixelFunc

static CPLErr ComplexPixelFunc( void **papoSources, int nSources, void *pData,
                                int nXSize, int nYSize,
                                GDALDataType eSrcType, GDALDataType eBufType,
                                int nPixelSpace, int nLineSpace )
{
    /* ---- Init ---- */
    if( nSources != 2 ) return CE_Failure;

    const void * const pReal = papoSources[0];
    const void * const pImag = papoSources[1];

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for( int iLine = 0; iLine < nYSize; ++iLine ) {
        for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
            // Source raster pixels may be obtained with GetSrcVal macro.
            const double adfPixVal[2] = {
                GetSrcVal(pReal, eSrcType, ii),  // re
                GetSrcVal(pImag, eSrcType, ii)  // im
            };

            GDALCopyWords(adfPixVal, GDT_CFloat64, 0,
                          static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                          iCol * nPixelSpace, eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // ComplexPixelFunc

typedef enum {
    GAT_amplitude,
    GAT_intensity,
    GAT_dB
} PolarAmplitudeType;

static const char pszPolarPixelFuncMetadata[] =
"<PixelFunctionArgumentsList>"
"   <Argument name='amplitude_type' description='Amplitude Type' type='string-select' default='AMPLITUDE'>"
"       <Value>INTENSITY</Value>"
"       <Value>dB</Value>"
"       <Value>AMPLITUDE</Value>"
"   </Argument>"
"</PixelFunctionArgumentsList>";

static CPLErr PolarPixelFunc( void **papoSources, int nSources, void *pData,
                              int nXSize, int nYSize,
                              GDALDataType eSrcType, GDALDataType eBufType,
                              int nPixelSpace, int nLineSpace,
                              CSLConstList papszArgs )
{
    /* ---- Init ---- */
    if( nSources != 2 ) return CE_Failure;

    const char pszName[] = "amplitude_type";
    const char* pszVal = CSLFetchNameValue(papszArgs, pszName);
    PolarAmplitudeType amplitudeType = GAT_amplitude;
    if ( pszVal != nullptr )
    {
        if ( strcmp( pszVal, "INTENSITY" ) == 0 )
            amplitudeType = GAT_intensity;
        else if ( strcmp( pszVal, "dB" ) == 0 )
            amplitudeType = GAT_dB;
        else if ( strcmp( pszVal, "AMPLITUDE" ) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid value for pixel function argument '%s': %s",
                     pszName, pszVal);
            return CE_Failure;
        }
    }

    const void * const pAmp = papoSources[0];
    const void * const pPhase = papoSources[1];

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for( int iLine = 0; iLine < nYSize; ++iLine ) {
        for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
            // Source raster pixels may be obtained with GetSrcVal macro.
            double dfAmp = GetSrcVal(pAmp, eSrcType, ii);
            switch ( amplitudeType )
            {
                case GAT_intensity:
                    // clip to zero
                    dfAmp = dfAmp <= 0 ? 0 : std::sqrt( dfAmp );
                    break;
                case GAT_dB:
                    dfAmp = dfAmp <= 0 ?
                                -std::numeric_limits<double>::infinity() :
                                pow(10, dfAmp / 20.);
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
                          static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                          iCol * nPixelSpace, eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // PolarPixelFunc

static CPLErr ModulePixelFunc( void **papoSources, int nSources, void *pData,
                               int nXSize, int nYSize,
                               GDALDataType eSrcType, GDALDataType eBufType,
                               int nPixelSpace, int nLineSpace )
{
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;

    if( GDALDataTypeIsComplex( eSrcType ) )
    {
        const void *pReal = papoSources[0];
        const void *pImag = static_cast<GByte *>(papoSources[0])
                    + GDALGetDataTypeSizeBytes( eSrcType ) / 2;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal = GetSrcVal(pReal, eSrcType, ii);
                const double dfImag = GetSrcVal(pImag, eSrcType, ii);

                const double dfPixVal =
                    sqrt( dfReal * dfReal + dfImag * dfImag );

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfPixVal =
                    fabs(GetSrcVal(papoSources[0], eSrcType, ii));

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // ModulePixelFunc

static CPLErr PhasePixelFunc( void **papoSources, int nSources, void *pData,
                              int nXSize, int nYSize,
                              GDALDataType eSrcType, GDALDataType eBufType,
                              int nPixelSpace, int nLineSpace )
{
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;

    if( GDALDataTypeIsComplex( eSrcType ) )
    {
        const void * const pReal = papoSources[0];
        const void * const pImag = static_cast<GByte *>(papoSources[0])
            + GDALGetDataTypeSizeBytes( eSrcType ) / 2;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal = GetSrcVal(pReal, eSrcType, ii);
                const double dfImag = GetSrcVal(pImag, eSrcType, ii);

                const double dfPixVal = atan2(dfImag, dfReal);

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }
    else if( GDALDataTypeIsInteger( eSrcType ) &&
             !GDALDataTypeIsSigned( eSrcType ) )
    {
        constexpr double dfZero = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            GDALCopyWords(
                &dfZero, GDT_Float64, 0,
                static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine,
                eBufType, nPixelSpace, nXSize );
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                const void * const pReal = papoSources[0];

                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal = GetSrcVal(pReal, eSrcType, ii);
                const double dfPixVal = (dfReal < 0) ? M_PI : 0.0;

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // PhasePixelFunc

static CPLErr ConjPixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace )
{
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;

    if( GDALDataTypeIsComplex( eSrcType ) && GDALDataTypeIsComplex( eBufType ) )
    {
        const int nOffset = GDALGetDataTypeSizeBytes( eSrcType ) / 2;
        const void * const pReal = papoSources[0];
        const void * const pImag =
            static_cast<GByte *>(papoSources[0]) + nOffset;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double adfPixVal[2] = {
                    +GetSrcVal(pReal, eSrcType, ii),  // re
                    -GetSrcVal(pImag, eSrcType, ii)  // im
                };

                GDALCopyWords(
                    adfPixVal, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
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

static const char pszSumPixelFuncMetadata[] =
"<PixelFunctionArgumentsList>"
"   <Argument name='k' description='Optional constant term' type='double' default='0.0' />"
"</PixelFunctionArgumentsList>";

static CPLErr SumPixelFunc(void **papoSources, int nSources, void *pData,
                    int nXSize, int nYSize,
                    GDALDataType eSrcType, GDALDataType eBufType,
                    int nPixelSpace, int nLineSpace, CSLConstList papszArgs )
{
    /* ---- Init ---- */
    if( nSources < 2 ) return CE_Failure;

    double dfK = 0.0;
    if ( FetchDoubleArg(papszArgs, "k", &dfK, &dfK ) != CE_None )
        return CE_Failure;

    /* ---- Set pixels ---- */
    if( GDALDataTypeIsComplex( eSrcType ) )
    {
        const int nOffset = GDALGetDataTypeSizeBytes( eSrcType ) / 2;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                double adfSum[2] = { dfK, 0.0 };

                for( int iSrc = 0; iSrc < nSources; ++iSrc ) {
                    const void * const pReal = papoSources[iSrc];
                    const void * const pImag =
                        static_cast<const GByte *>(pReal) + nOffset;

                    // Source raster pixels may be obtained with GetSrcVal macro.
                    adfSum[0] += GetSrcVal(pReal, eSrcType, ii);
                    adfSum[1] += GetSrcVal(pImag, eSrcType, ii);
                }

                GDALCopyWords(
                    adfSum, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                double dfSum = dfK;  // Not complex.

                for( int iSrc = 0; iSrc < nSources; ++iSrc ) {
                    // Source raster pixels may be obtained with GetSrcVal macro.
                    dfSum += GetSrcVal(papoSources[iSrc], eSrcType, ii);
                }

                GDALCopyWords(
                    &dfSum, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
} /* SumPixelFunc */

static CPLErr DiffPixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace)
{
    /* ---- Init ---- */
    if( nSources != 2 ) return CE_Failure;

    if( GDALDataTypeIsComplex( eSrcType ) )
    {
        const int nOffset = GDALGetDataTypeSizeBytes( eSrcType ) / 2;
        const void * const pReal0 = papoSources[0];
        const void * const pImag0 =
            static_cast<GByte *>(papoSources[0]) + nOffset;
        const void * const pReal1 = papoSources[1];
        const void * const pImag1 =
            static_cast<GByte *>(papoSources[1]) + nOffset;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                double adfPixVal[2] = {
                    GetSrcVal(pReal0, eSrcType, ii)
                    - GetSrcVal(pReal1, eSrcType, ii),
                    GetSrcVal(pImag0, eSrcType, ii)
                    - GetSrcVal(pImag1, eSrcType, ii)
                };

                GDALCopyWords(
                    adfPixVal, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                // Not complex.
                const double dfPixVal = GetSrcVal(papoSources[0], eSrcType, ii)
                    - GetSrcVal(papoSources[1], eSrcType, ii);

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // DiffPixelFunc

static const char pszMulPixelFuncMetadata[] =
"<PixelFunctionArgumentsList>"
"   <Argument name='k' description='Optional constant factor' type='double' default='1.0' />"
"</PixelFunctionArgumentsList>";

static CPLErr MulPixelFunc( void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize,
                            GDALDataType eSrcType, GDALDataType eBufType,
                            int nPixelSpace, int nLineSpace,
                            CSLConstList papszArgs )
{
    /* ---- Init ---- */
    if( nSources < 2 ) return CE_Failure;

    double dfK = 1.0;
    if ( FetchDoubleArg(papszArgs, "k", &dfK, &dfK ) != CE_None )
        return CE_Failure;

    /* ---- Set pixels ---- */
    if( GDALDataTypeIsComplex( eSrcType ) )
    {
        const int nOffset = GDALGetDataTypeSizeBytes( eSrcType ) / 2;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                double adfPixVal[2] = { dfK, 0.0 };

                for( int iSrc = 0; iSrc < nSources; ++iSrc ) {
                    const void * const pReal = papoSources[iSrc];
                    const void * const pImag =
                        static_cast<const GByte *>(pReal) + nOffset;

                    const double dfOldR = adfPixVal[0];
                    const double dfOldI = adfPixVal[1];

                    // Source raster pixels may be obtained with GetSrcVal macro.
                    const double dfNewR = GetSrcVal(pReal, eSrcType, ii);
                    const double dfNewI = GetSrcVal(pImag, eSrcType, ii);

                    adfPixVal[0] = dfOldR * dfNewR - dfOldI * dfNewI;
                    adfPixVal[1] = dfOldR * dfNewI + dfOldI * dfNewR;
                }

                GDALCopyWords(
                    adfPixVal, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                double dfPixVal = dfK;  // Not complex.

                for( int iSrc = 0; iSrc < nSources; ++iSrc ) {
                    // Source raster pixels may be obtained with GetSrcVal macro.
                    dfPixVal *= GetSrcVal(papoSources[iSrc], eSrcType, ii);
                }

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // MulPixelFunc

static CPLErr DivPixelFunc( void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize,
                            GDALDataType eSrcType, GDALDataType eBufType,
                            int nPixelSpace, int nLineSpace )
{
    /* ---- Init ---- */
    if( nSources != 2 ) return CE_Failure;

    /* ---- Set pixels ---- */
    if( GDALDataTypeIsComplex( eSrcType ) )
    {
        const int nOffset = GDALGetDataTypeSizeBytes( eSrcType ) / 2;
        const void * const pReal0 = papoSources[0];
        const void * const pImag0 =
            static_cast<GByte *>(papoSources[0]) + nOffset;
        const void * const pReal1 = papoSources[1];
        const void * const pImag1 =
            static_cast<GByte *>(papoSources[1]) + nOffset;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal0 = GetSrcVal(pReal0, eSrcType, ii);
                const double dfReal1 = GetSrcVal(pReal1, eSrcType, ii);
                const double dfImag0 = GetSrcVal(pImag0, eSrcType, ii);
                const double dfImag1 = GetSrcVal(pImag1, eSrcType, ii);
                const double dfAux = dfReal1 * dfReal1 + dfImag1 * dfImag1;

                const double adfPixVal[2] = {
                    dfAux == 0 ? std::numeric_limits<double>::infinity() :
                        dfReal0 * dfReal1 / dfAux + dfImag0 * dfImag1 / dfAux,
                    dfAux == 0 ? std::numeric_limits<double>::infinity() :
                        dfReal1 / dfAux * dfImag0 - dfReal0 * dfImag1 / dfAux
                };

                GDALCopyWords(
                    adfPixVal, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                const double dfVal = GetSrcVal(papoSources[1], eSrcType, ii);
                double dfPixVal =
                        dfVal == 0 ? std::numeric_limits<double>::infinity() :
                        GetSrcVal(papoSources[0], eSrcType, ii) /  dfVal;

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // DivPixelFunc

static CPLErr CMulPixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace )
{
    /* ---- Init ---- */
    if( nSources != 2 ) return CE_Failure;

    /* ---- Set pixels ---- */
    if( GDALDataTypeIsComplex( eSrcType ) )
    {
        const int nOffset = GDALGetDataTypeSizeBytes( eSrcType ) / 2;
        const void * const pReal0 = papoSources[0];
        const void * const pImag0 =
            static_cast<GByte *>(papoSources[0]) + nOffset;
        const void * const pReal1 = papoSources[1];
        const void * const pImag1 =
            static_cast<GByte *>(papoSources[1]) + nOffset;

        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal0 = GetSrcVal(pReal0, eSrcType, ii);
                const double dfReal1 = GetSrcVal(pReal1, eSrcType, ii);
                const double dfImag0 = GetSrcVal(pImag0, eSrcType, ii);
                const double dfImag1 = GetSrcVal(pImag1, eSrcType, ii);
                const double adfPixVal[2] = {
                    dfReal0 * dfReal1 + dfImag0 * dfImag1,
                    dfReal1 * dfImag0 - dfReal0 * dfImag1
                };

                GDALCopyWords(
                    adfPixVal, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }
    else
    {
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                // Not complex.
                const double adfPixVal[2] = {
                    GetSrcVal(papoSources[0], eSrcType, ii)
                    * GetSrcVal(papoSources[1], eSrcType, ii),
                    0.0
                };

                GDALCopyWords(
                    adfPixVal, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // CMulPixelFunc

static const char pszInvPixelFuncMetadata[] =
"<PixelFunctionArgumentsList>"
"   <Argument name='k' description='Optional constant factor' type='double' default='1.0' />"
"</PixelFunctionArgumentsList>";

static CPLErr InvPixelFunc( void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize,
                            GDALDataType eSrcType, GDALDataType eBufType,
                            int nPixelSpace, int nLineSpace,
                            CSLConstList papszArgs )
{
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;

    double dfK = 1.0;
    if ( FetchDoubleArg(papszArgs, "k", &dfK, &dfK ) != CE_None )
        return CE_Failure;

    /* ---- Set pixels ---- */
    if( GDALDataTypeIsComplex( eSrcType ) )
    {
        const int nOffset = GDALGetDataTypeSizeBytes( eSrcType ) / 2;
        const void * const pReal = papoSources[0];
        const void * const pImag =
            static_cast<GByte *>(papoSources[0]) + nOffset;

        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal = GetSrcVal(pReal, eSrcType, ii);
                const double dfImag = GetSrcVal(pImag, eSrcType, ii);
                const double dfAux = dfReal * dfReal + dfImag * dfImag;
                const double adfPixVal[2] = {
                    dfAux == 0 ? std::numeric_limits<double>::infinity() : dfK * dfReal / dfAux,
                    dfAux == 0 ? std::numeric_limits<double>::infinity() : - dfK * dfImag / dfAux };

                GDALCopyWords(
                    adfPixVal, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                // Not complex.
                const double dfVal = GetSrcVal(papoSources[0], eSrcType, ii);
                const double dfPixVal =
                    dfVal == 0 ? std::numeric_limits<double>::infinity() :
                    dfK / dfVal;

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // InvPixelFunc

static CPLErr IntensityPixelFunc( void **papoSources, int nSources, void *pData,
                                  int nXSize, int nYSize,
                                  GDALDataType eSrcType, GDALDataType eBufType,
                                  int nPixelSpace, int nLineSpace )
{
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;

    if( GDALDataTypeIsComplex( eSrcType ) )
    {
        const int nOffset = GDALGetDataTypeSizeBytes( eSrcType ) / 2;
        const void * const pReal = papoSources[0];
        const void * const pImag =
            static_cast<GByte *>(papoSources[0]) + nOffset;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal = GetSrcVal(pReal, eSrcType, ii);
                const double dfImag = GetSrcVal(pImag, eSrcType, ii);

                const double dfPixVal = dfReal * dfReal + dfImag * dfImag;

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                double dfPixVal = GetSrcVal(papoSources[0], eSrcType, ii);
                dfPixVal *= dfPixVal;

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // IntensityPixelFunc

static CPLErr SqrtPixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace )
{
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;
    if( GDALDataTypeIsComplex( eSrcType ) ) return CE_Failure;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for( int iLine = 0; iLine < nYSize; ++iLine ) {
        for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
            // Source raster pixels may be obtained with GetSrcVal macro.
            const double dfPixVal =
                sqrt( GetSrcVal(papoSources[0], eSrcType, ii) );

            GDALCopyWords(
                &dfPixVal, GDT_Float64, 0,
                static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                iCol * nPixelSpace, eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // SqrtPixelFunc

static CPLErr Log10PixelFuncHelper( void **papoSources, int nSources,
                                    void *pData,
                                    int nXSize, int nYSize,
                                    GDALDataType eSrcType,
                                    GDALDataType eBufType,
                                    int nPixelSpace, int nLineSpace,
                                    double fact )
{
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;

    if( GDALDataTypeIsComplex( eSrcType ) )
    {
        // Complex input datatype.
        const int nOffset = GDALGetDataTypeSizeBytes( eSrcType ) / 2;
        const void * const pReal = papoSources[0];
        const void * const pImag =
            static_cast<GByte *>(papoSources[0]) + nOffset;

        /* We should compute fact * log10( sqrt( dfReal * dfReal + dfImag * dfImag ) ) */
        /* Given that log10(sqrt(x)) = 0.5 * log10(x) */
        /* we can remove the sqrt() by multiplying fact by 0.5 */
        fact *= 0.5;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal = GetSrcVal(pReal, eSrcType, ii);
                const double dfImag = GetSrcVal(pImag, eSrcType, ii);

                const double dfPixVal =
                    fact * log10( dfReal * dfReal + dfImag * dfImag );

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfPixVal =
                    fact * log10( fabs(
                        GetSrcVal(papoSources[0], eSrcType, ii) ) );

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // Log10PixelFuncHelper

static CPLErr Log10PixelFunc( void **papoSources, int nSources, void *pData,
                              int nXSize, int nYSize,
                              GDALDataType eSrcType, GDALDataType eBufType,
                              int nPixelSpace, int nLineSpace )
{
    return Log10PixelFuncHelper(papoSources, nSources, pData,
                                nXSize, nYSize, eSrcType, eBufType,
                                nPixelSpace, nLineSpace, 1.0);
} // Log10PixelFunc

static const char pszDBPixelFuncMetadata[] =
"<PixelFunctionArgumentsList>"
"   <Argument name='fact' description='Factor' type='double' default='20.0' />"
"</PixelFunctionArgumentsList>";

static CPLErr DBPixelFunc( void **papoSources, int nSources, void *pData,
                           int nXSize, int nYSize,
                           GDALDataType eSrcType, GDALDataType eBufType,
                           int nPixelSpace, int nLineSpace,
                           CSLConstList papszArgs)
{
    double dfFact = 20.;
    if ( FetchDoubleArg(papszArgs, "fact", &dfFact, &dfFact ) != CE_None )
        return CE_Failure;

    return Log10PixelFuncHelper(papoSources, nSources, pData,
                                nXSize, nYSize, eSrcType, eBufType,
                                nPixelSpace, nLineSpace, dfFact);
} // DBPixelFunc

static CPLErr ExpPixelFuncHelper( void **papoSources, int nSources, void *pData,
                                  int nXSize, int nYSize,
                                  GDALDataType eSrcType, GDALDataType eBufType,
                                  int nPixelSpace, int nLineSpace,
                                  double base, double fact )
{
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;
    if( GDALDataTypeIsComplex( eSrcType ) ) return CE_Failure;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for( int iLine = 0; iLine < nYSize; ++iLine ) {
        for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
            // Source raster pixels may be obtained with GetSrcVal macro.
            const double dfPixVal =
                pow(base, GetSrcVal(papoSources[0], eSrcType, ii) * fact);

            GDALCopyWords(
                &dfPixVal, GDT_Float64, 0,
                static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                iCol * nPixelSpace, eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // ExpPixelFuncHelper

static const char pszExpPixelFuncMetadata[] =
"<PixelFunctionArgumentsList>"
"   <Argument name='base' description='Base' type='double' default='2.7182818284590452353602874713526624' />"
"   <Argument name='fact' description='Factor' type='double' default='1' />"
"</PixelFunctionArgumentsList>";

static CPLErr ExpPixelFunc( void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize,
                            GDALDataType eSrcType, GDALDataType eBufType,
                            int nPixelSpace, int nLineSpace,
                            CSLConstList papszArgs )
{
    double dfBase = 2.7182818284590452353602874713526624;
    double dfFact = 1.;

    if ( FetchDoubleArg(papszArgs, "base", &dfBase, &dfBase ) != CE_None )
        return CE_Failure;

    if ( FetchDoubleArg(papszArgs, "fact", &dfFact, &dfFact ) != CE_None )
        return CE_Failure;

    return ExpPixelFuncHelper(papoSources, nSources, pData,
                              nXSize, nYSize, eSrcType, eBufType,
                              nPixelSpace, nLineSpace, dfBase, dfFact);
}  // ExpPixelFunc

static CPLErr dB2AmpPixelFunc( void **papoSources, int nSources, void *pData,
                               int nXSize, int nYSize,
                               GDALDataType eSrcType, GDALDataType eBufType,
                               int nPixelSpace, int nLineSpace )
{
    return ExpPixelFuncHelper(papoSources, nSources, pData,
                              nXSize, nYSize, eSrcType, eBufType,
                              nPixelSpace, nLineSpace, 10.0, 1./20);
}  // dB2AmpPixelFunc

static CPLErr dB2PowPixelFunc( void **papoSources, int nSources, void *pData,
                               int nXSize, int nYSize,
                               GDALDataType eSrcType, GDALDataType eBufType,
                               int nPixelSpace, int nLineSpace )
{
    return ExpPixelFuncHelper(papoSources, nSources, pData,
                              nXSize, nYSize, eSrcType, eBufType,
                              nPixelSpace, nLineSpace, 10.0, 1./10);
}  // dB2PowPixelFunc

static const char pszPowPixelFuncMetadata[] =
"<PixelFunctionArgumentsList>"
"   <Argument name='power' description='Exponent' type='double' mandatory='1' />"
"</PixelFunctionArgumentsList>";

static CPLErr PowPixelFunc( void **papoSources, int nSources, void *pData,
                               int nXSize, int nYSize,
                               GDALDataType eSrcType, GDALDataType eBufType,
                               int nPixelSpace, int nLineSpace, CSLConstList papszArgs ) {
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;
    if( GDALDataTypeIsComplex( eSrcType ) ) return CE_Failure;

    double power;
    if ( FetchDoubleArg(papszArgs, "power", &power) != CE_None ) return CE_Failure;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for( int iLine = 0; iLine < nYSize; ++iLine ) {
        for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
            const double dfPixVal = std::pow(
                    GetSrcVal(papoSources[0], eSrcType, ii),
                    power);

            GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;

}

// Given nt intervals spaced by dt and beginning at t0, return the the index of
// the lower bound of the interval that should be used to interpolate/extrapolate
// a value for t.
static std::size_t intervalLeft(double t0, double dt, std::size_t nt, double t)
{
    if (t < t0) {
        return 0;
    }

    std::size_t n = static_cast<std::size_t>((t - t0) / dt);

    if (n >= nt - 1) {
        return nt - 2;
    }

    return n;
}

static double InterpolateLinear(double dfX0, double dfX1, double dfY0, double dfY1, double dfX) {
    return dfY0 + (dfX - dfX0) * (dfY1 - dfY0) / (dfX1 - dfX0);
}

static double InterpolateExponential(double dfX0, double dfX1, double dfY0, double dfY1, double dfX) {
    const double r = std::log(dfY1 / dfY0) / (dfX1 - dfX0);
    return dfY0*std::exp(r * (dfX - dfX0));
}

static const char pszInterpolatePixelFuncMetadata[] =
"<PixelFunctionArgumentsList>"
"   <Argument name='t0' description='t0' type='double' mandatory='1' />"
"   <Argument name='dt' description='dt' type='double' mandatory='1' />"
"   <Argument name='t' description='t' type='double' mandatory='1' />"
"</PixelFunctionArgumentsList>";

template<decltype(InterpolateLinear) InterpolationFunction>
CPLErr InterpolatePixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace, CSLConstList papszArgs ) {
    /* ---- Init ---- */
    if( GDALDataTypeIsComplex( eSrcType ) ) return CE_Failure;

    double dfT0;
    if (FetchDoubleArg(papszArgs, "t0", &dfT0) == CE_Failure ) return CE_Failure;

    double dfT;
    if (FetchDoubleArg(papszArgs, "t", &dfT) == CE_Failure ) return CE_Failure;

    double dfDt;
    if (FetchDoubleArg(papszArgs, "dt", &dfDt) == CE_Failure ) return CE_Failure;

    if( nSources < 2 ) {
        CPLError(CE_Failure, CPLE_AppDefined, "At least two sources required for interpolation.");
        return CE_Failure;
    }

    if (dfT == 0 || !std::isfinite(dfT) ) {
        CPLError(CE_Failure, CPLE_AppDefined, "dt must be finite and non-zero");
        return CE_Failure;
    }

    const auto i0 = intervalLeft(dfT0, dfDt, nSources, dfT);
    const auto i1 = i0 + 1;
    dfT0 = dfT0 + static_cast<double>(i0) * dfDt;
    double dfX1 = dfT0 + dfDt;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for( int iLine = 0; iLine < nYSize; ++iLine ) {
        for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
            const double dfY0 = GetSrcVal(papoSources[i0], eSrcType, ii);
            const double dfY1 = GetSrcVal(papoSources[i1], eSrcType, ii);

            const double dfPixVal = InterpolationFunction(dfT0, dfX1, dfY0, dfY1, dfT);

            GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}

static const char pszReplaceNoDataPixelFuncMetadata[] =
"<PixelFunctionArgumentsList>"
"   <Argument type='builtin' value='NoData' />"
"   <Argument name='to' type='double' description='New NoData value to be replaced' default='nan' />"
"</PixelFunctionArgumentsList>";

static CPLErr ReplaceNoDataPixelFunc( void **papoSources, int nSources, void *pData,
                               int nXSize, int nYSize,
                               GDALDataType eSrcType, GDALDataType eBufType,
                               int nPixelSpace, int nLineSpace, CSLConstList papszArgs ) {
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;
    if (GDALDataTypeIsComplex(eSrcType))
    {
        CPLError(
          CE_Failure, CPLE_AppDefined, "replace_nodata cannot convert complex data types");
        return CE_Failure;
    }

    double dfOldNoData, dfNewNoData = NAN;
    if ( FetchDoubleArg(papszArgs, "NoData", &dfOldNoData) != CE_None ) return CE_Failure;
    if ( FetchDoubleArg(papszArgs, "to", &dfNewNoData, &dfNewNoData) != CE_None ) return CE_Failure;

    if (!GDALDataTypeIsFloating(eBufType) && std::isnan(dfNewNoData))
    {
        CPLError(
          CE_Failure, CPLE_AppDefined, "Using nan requires a floating point type output buffer");
        return CE_Failure;
    }

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for( int iLine = 0; iLine < nYSize; ++iLine )
    {
        for( int iCol = 0; iCol < nXSize; ++iCol, ++ii )
        {
            double dfPixVal = GetSrcVal(papoSources[0], eSrcType, ii);
            if (dfPixVal == dfOldNoData || std::isnan(dfPixVal)) dfPixVal = dfNewNoData;

            GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}

static const char pszScalePixelFuncMetadata[] =
"<PixelFunctionArgumentsList>"
"   <Argument type='builtin' value='offset' />"
"   <Argument type='builtin' value='scale' />"
"</PixelFunctionArgumentsList>";

static CPLErr ScalePixelFunc( void **papoSources, int nSources, void *pData,
                               int nXSize, int nYSize,
                               GDALDataType eSrcType, GDALDataType eBufType,
                               int nPixelSpace, int nLineSpace, CSLConstList papszArgs ) {
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;
    if (GDALDataTypeIsComplex(eSrcType))
    {
        CPLError(
          CE_Failure, CPLE_AppDefined, "scale cannot by applied to complex data types");
        return CE_Failure;
    }

    double dfScale, dfOffset;
    if ( FetchDoubleArg(papszArgs, "scale", &dfScale) != CE_None ) return CE_Failure;
    if ( FetchDoubleArg(papszArgs, "offset", &dfOffset) != CE_None ) return CE_Failure;

    /* ---- Set pixels ---- */
    size_t ii = 0;
    for( int iLine = 0; iLine < nYSize; ++iLine )
    {
        for( int iCol = 0; iCol < nXSize; ++iCol, ++ii )
        {
            const double dfPixVal = GetSrcVal(papoSources[0], eSrcType, ii) * dfScale + dfOffset;

            GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}


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
 * - "div": divide one rasted band by another (b1 / b2).
 *          Note: no check is performed on zero division
 * - "cmul": multiply the first band for the complex conjugate of the second
 * - "inv": inverse (1./x). Note: no check is performed on zero division
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
 *          The function also accepts two optional parameters: ``base`` and ``fact``
 *          that allow to compute the generalized formula: ``base ^ ( fact * x)``.
 *          Note: this function is the recommended one to perform conversion
 *          form logarithmic scale (dB): `` 10. ^ (x / 20.)``, in this case
 *          ``base = 10.`` and ``fact = 1./20``
 * - "dB2amp": perform scale conversion from logarithmic to linear
 *             (amplitude) (i.e. 10 ^ ( x / 20 ) ) of a single raster
 *             band (real only).
 *             Deprecated in GDAL v3.5. Please use the ``exp`` pixel function with
 *             ``base = 10.`` and ``fact = 0.05`` i.e. ``1./20``
 * - "dB2pow": perform scale conversion from logarithmic to linear
 *             (power) (i.e. 10 ^ ( x / 10 ) ) of a single raster
 *             band (real only)
 *             Deprecated in GDAL v3.5. Please use the ``exp`` pixel function with
 *             ``base = 10.`` and ``fact = 0.1`` i.e. ``1./10``
 * - "pow": raise a single raster band to a constant power
 * - "interpolate_linear": interpolate values between two raster bands
 *                         using linear interpolation
 * - "interpolate_exp": interpolate values between two raster bands using
 *                      exponential interpolation
 * - "scale": Apply the RasterBand metadata values of "offset" and "scale"
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
    GDALAddDerivedBandPixelFuncWithArgs("polar", PolarPixelFunc, pszPolarPixelFuncMetadata);
    GDALAddDerivedBandPixelFunc("mod", ModulePixelFunc);
    GDALAddDerivedBandPixelFunc("phase", PhasePixelFunc);
    GDALAddDerivedBandPixelFunc("conj", ConjPixelFunc);
    GDALAddDerivedBandPixelFuncWithArgs("sum", SumPixelFunc, pszSumPixelFuncMetadata);
    GDALAddDerivedBandPixelFunc("diff", DiffPixelFunc);
    GDALAddDerivedBandPixelFuncWithArgs("mul", MulPixelFunc, pszMulPixelFuncMetadata);
    GDALAddDerivedBandPixelFunc("div", DivPixelFunc);
    GDALAddDerivedBandPixelFunc("cmul", CMulPixelFunc);
    GDALAddDerivedBandPixelFuncWithArgs("inv", InvPixelFunc, pszInvPixelFuncMetadata);
    GDALAddDerivedBandPixelFunc("intensity", IntensityPixelFunc);
    GDALAddDerivedBandPixelFunc("sqrt", SqrtPixelFunc);
    GDALAddDerivedBandPixelFunc("log10", Log10PixelFunc);
    GDALAddDerivedBandPixelFuncWithArgs("dB", DBPixelFunc, pszDBPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("exp", ExpPixelFunc, pszExpPixelFuncMetadata);
    GDALAddDerivedBandPixelFunc("dB2amp", dB2AmpPixelFunc);  // deprecated in v3.5
    GDALAddDerivedBandPixelFunc("dB2pow", dB2PowPixelFunc);  // deprecated in v3.5
    GDALAddDerivedBandPixelFuncWithArgs("pow", PowPixelFunc, pszPowPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("interpolate_linear",
        InterpolatePixelFunc<InterpolateLinear>, pszInterpolatePixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("interpolate_exp",
        InterpolatePixelFunc<InterpolateExponential>, pszInterpolatePixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("replace_nodata",
        ReplaceNoDataPixelFunc, pszReplaceNoDataPixelFuncMetadata);
    GDALAddDerivedBandPixelFuncWithArgs("scale", ScalePixelFunc, pszScalePixelFuncMetadata);

    return CE_None;
}
