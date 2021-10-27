/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implementation of a set of GDALDerivedPixelFunc(s) to be used
 *           with source raster band of virtual GDAL datasets.
 * Author:   Antonio Valentino <antonio.valentino@tiscali.it>
 *
 ******************************************************************************
 * Copyright (c) 2008-2014 Antonio Valentino <antonio.valentino@tiscali.it>
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

static CPLErr RealPixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace );

static CPLErr ImagPixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace );

static CPLErr ComplexPixelFunc( void **papoSources, int nSources, void *pData,
                                int nXSize, int nYSize,
                                GDALDataType eSrcType, GDALDataType eBufType,
                                int nPixelSpace, int nLineSpace );

static CPLErr ModulePixelFunc( void **papoSources, int nSources, void *pData,
                               int nXSize, int nYSize,
                               GDALDataType eSrcType, GDALDataType eBufType,
                               int nPixelSpace, int nLineSpace );

static CPLErr PhasePixelFunc( void **papoSources, int nSources, void *pData,
                              int nXSize, int nYSize,
                              GDALDataType eSrcType, GDALDataType eBufType,
                              int nPixelSpace, int nLineSpace );

static CPLErr ConjPixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace );

static CPLErr SumPixelFunc( void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize,
                            GDALDataType eSrcType, GDALDataType eBufType,
                            int nPixelSpace, int nLineSpace );

static CPLErr DiffPixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace );

static CPLErr MulPixelFunc( void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize,
                            GDALDataType eSrcType, GDALDataType eBufType,
                            int nPixelSpace, int nLineSpace );

static CPLErr CMulPixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace );

static CPLErr InvPixelFunc( void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize,
                            GDALDataType eSrcType, GDALDataType eBufType,
                            int nPixelSpace, int nLineSpace );

static CPLErr IntensityPixelFunc( void **papoSources, int nSources, void *pData,
                                  int nXSize, int nYSize,
                                  GDALDataType eSrcType, GDALDataType eBufType,
                                  int nPixelSpace, int nLineSpace );

static CPLErr SqrtPixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace );

static CPLErr Log10PixelFunc( void **papoSources, int nSources, void *pData,
                              int nXSize, int nYSize,
                              GDALDataType eSrcType, GDALDataType eBufType,
                              int nPixelSpace, int nLineSpace );

static CPLErr DBPixelFunc( void **papoSources, int nSources, void *pData,
                           int nXSize, int nYSize,
                           GDALDataType eSrcType, GDALDataType eBufType,
                           int nPixelSpace, int nLineSpace );

static CPLErr dB2AmpPixelFunc( void **papoSources, int nSources, void *pData,
                               int nXSize, int nYSize,
                               GDALDataType eSrcType, GDALDataType eBufType,
                               int nPixelSpace, int nLineSpace );

static CPLErr dB2PowPixelFunc( void **papoSources, int nSources, void *pData,
                               int nXSize, int nYSize,
                               GDALDataType eSrcType, GDALDataType eBufType,
                               int nPixelSpace, int nLineSpace );

static CPLErr PowPixelFuncHelper( void **papoSources, int nSources, void *pData,
                                  int nXSize, int nYSize,
                                  GDALDataType eSrcType, GDALDataType eBufType,
                                  int nPixelSpace, int nLineSpace,
                                  double base, double fact );

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

static CPLErr FetchDoubleArg(CSLConstList papszArgs, const char *pszName, double* pdfX)
{
    const char* pszVal = CSLFetchNameValue(papszArgs, pszName);

    if ( pszVal == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing pixel function argument: %s", pszName);
        return CE_Failure;
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
}  // MakeComplexPixelFunc

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

static CPLErr SumPixelFunc(void **papoSources, int nSources, void *pData,
                    int nXSize, int nYSize,
                    GDALDataType eSrcType, GDALDataType eBufType,
                    int nPixelSpace, int nLineSpace)
{
    /* ---- Init ---- */
    if( nSources < 2 ) return CE_Failure;

    /* ---- Set pixels ---- */
    if( GDALDataTypeIsComplex( eSrcType ) )
    {
        const int nOffset = GDALGetDataTypeSizeBytes( eSrcType ) / 2;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                double adfSum[2] = { 0.0, 0.0 };

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
                double dfSum = 0;  // Not complex.

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

static CPLErr MulPixelFunc( void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize,
                            GDALDataType eSrcType, GDALDataType eBufType,
                            int nPixelSpace, int nLineSpace )
{
    /* ---- Init ---- */
    if( nSources < 2 ) return CE_Failure;

    /* ---- Set pixels ---- */
    if( GDALDataTypeIsComplex( eSrcType ) )
    {
        const int nOffset = GDALGetDataTypeSizeBytes( eSrcType ) / 2;

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                double adfPixVal[2] = { 1.0, 0.0 };

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
                double dfPixVal = 1.0;  // Not complex.

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

static CPLErr InvPixelFunc( void **papoSources, int nSources, void *pData,
                            int nXSize, int nYSize,
                            GDALDataType eSrcType, GDALDataType eBufType,
                            int nPixelSpace, int nLineSpace )
{
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;

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
                    dfAux == 0 ? std::numeric_limits<double>::infinity() : dfReal / dfAux,
                    dfAux == 0 ? std::numeric_limits<double>::infinity() : -dfImag / dfAux };

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
                    1.0 / dfVal;

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

        /* ---- Set pixels ---- */
        size_t ii = 0;
        for( int iLine = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with GetSrcVal macro.
                const double dfReal = GetSrcVal(pReal, eSrcType, ii);
                const double dfImag = GetSrcVal(pImag, eSrcType, ii);

                const double dfPixVal =
                    fact * log10( sqrt( dfReal * dfReal + dfImag * dfImag ) );

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

static CPLErr DBPixelFunc( void **papoSources, int nSources, void *pData,
                           int nXSize, int nYSize,
                           GDALDataType eSrcType, GDALDataType eBufType,
                           int nPixelSpace, int nLineSpace )
{
    return Log10PixelFuncHelper(papoSources, nSources, pData,
                                nXSize, nYSize, eSrcType, eBufType,
                                nPixelSpace, nLineSpace, 20.0);
} // DBPixelFunc

static CPLErr PowPixelFuncHelper( void **papoSources, int nSources, void *pData,
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
                pow(base, GetSrcVal(papoSources[0], eSrcType, ii) / fact);

            GDALCopyWords(
                &dfPixVal, GDT_Float64, 0,
                static_cast<GByte *>(pData) + static_cast<GSpacing>(nLineSpace) * iLine +
                iCol * nPixelSpace, eBufType, nPixelSpace, 1);
        }
    }

    /* ---- Return success ---- */
    return CE_None;
}  // PowPixelFuncHelper

static CPLErr dB2AmpPixelFunc( void **papoSources, int nSources, void *pData,
                               int nXSize, int nYSize,
                               GDALDataType eSrcType, GDALDataType eBufType,
                               int nPixelSpace, int nLineSpace )
{
    return PowPixelFuncHelper(papoSources, nSources, pData,
                              nXSize, nYSize, eSrcType, eBufType,
                              nPixelSpace, nLineSpace, 10.0, 20.0);
}  // dB2AmpPixelFunc

static CPLErr dB2PowPixelFunc( void **papoSources, int nSources, void *pData,
                               int nXSize, int nYSize,
                               GDALDataType eSrcType, GDALDataType eBufType,
                               int nPixelSpace, int nLineSpace )
{
    return PowPixelFuncHelper(papoSources, nSources, pData,
                              nXSize, nYSize, eSrcType, eBufType,
                              nPixelSpace, nLineSpace, 10.0, 10.0);
}  // dB2PowPixelFunc

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
 * - "mod": extract module from a single raster band (real or complex)
 * - "phase": extract phase from a single raster band [-PI,PI] (0 or PI for
              non-complex)
 * - "conj": computes the complex conjugate of a single raster band (just a
 *           copy if the input is non-complex)
 * - "sum": sum 2 or more raster bands
 * - "diff": computes the difference between 2 raster bands (b1 - b2)
 * - "mul": multiply 2 or more raster bands
 * - "cmul": multiply the first band for the complex conjugate of the second
 * - "inv": inverse (1./x). Note: no check is performed on zero division
 * - "intensity": computes the intensity Re(x*conj(x)) of a single raster band
 *                (real or complex)
 * - "sqrt": perform the square root of a single raster band (real only)
 * - "log10": compute the logarithm (base 10) of the abs of a single raster
 *            band (real or complex): log10( abs( x ) )
 * - "dB": perform conversion to dB of the abs of a single raster
 *         band (real or complex): 20. * log10( abs( x ) )
 * - "dB2amp": perform scale conversion from logarithmic to linear
 *             (amplitude) (i.e. 10 ^ ( x / 20 ) ) of a single raster
 *                 band (real only)
 * - "dB2pow": perform scale conversion from logarithmic to linear
 *             (power) (i.e. 10 ^ ( x / 10 ) ) of a single raster
 *             band (real only)
 * - "pow": raise a single raster band to a constant power
 * - "interpolate_linear": interpolate values between two raster bands
 *                         using linear interpolation
 * - "interpolate_exp": interpolate values between two raster bands using
 *                      exponential interpolation
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
    GDALAddDerivedBandPixelFunc("mod", ModulePixelFunc);
    GDALAddDerivedBandPixelFunc("phase", PhasePixelFunc);
    GDALAddDerivedBandPixelFunc("conj", ConjPixelFunc);
    GDALAddDerivedBandPixelFunc("sum", SumPixelFunc);
    GDALAddDerivedBandPixelFunc("diff", DiffPixelFunc);
    GDALAddDerivedBandPixelFunc("mul", MulPixelFunc);
    GDALAddDerivedBandPixelFunc("cmul", CMulPixelFunc);
    GDALAddDerivedBandPixelFunc("inv", InvPixelFunc);
    GDALAddDerivedBandPixelFunc("intensity", IntensityPixelFunc);
    GDALAddDerivedBandPixelFunc("sqrt", SqrtPixelFunc);
    GDALAddDerivedBandPixelFunc("log10", Log10PixelFunc);
    GDALAddDerivedBandPixelFunc("dB", DBPixelFunc);
    GDALAddDerivedBandPixelFunc("dB2amp", dB2AmpPixelFunc);
    GDALAddDerivedBandPixelFunc("dB2pow", dB2PowPixelFunc);
    GDALAddDerivedBandPixelFuncWithArgs("pow", PowPixelFunc, nullptr);
    GDALAddDerivedBandPixelFuncWithArgs("interpolate_linear", InterpolatePixelFunc<InterpolateLinear>, nullptr);
    GDALAddDerivedBandPixelFuncWithArgs("interpolate_exp", InterpolatePixelFunc<InterpolateExponential>, nullptr);

    return CE_None;
}
