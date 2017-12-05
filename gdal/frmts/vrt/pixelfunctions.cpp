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

static CPLErr RealPixelFunc( void **papoSources, int nSources, void *pData,
                             int nXSize, int nYSize,
                             GDALDataType eSrcType, GDALDataType eBufType,
                             int nPixelSpace, int nLineSpace )
{
    /* ---- Init ---- */
    if( nSources != 1 ) return CE_Failure;

    const int nPixelSpaceSrc = GDALGetDataTypeSizeBytes( eSrcType );
    const int nLineSpaceSrc = nPixelSpaceSrc * nXSize;

    /* ---- Set pixels ---- */
    for( int iLine = 0; iLine < nYSize; ++iLine ) {
        GDALCopyWords(
            static_cast<GByte *>(papoSources[0]) + nLineSpaceSrc * iLine,
            eSrcType, nPixelSpaceSrc,
            static_cast<GByte *>(pData) + nLineSpace * iLine,
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
        const int nLineSpaceSrc = nPixelSpaceSrc * nXSize;

        const void * const pImag = static_cast<GByte *>(papoSources[0])
            + GDALGetDataTypeSizeBytes( eSrcType ) / 2;

        /* ---- Set pixels ---- */
        for( int iLine = 0; iLine < nYSize; ++iLine )
        {
            GDALCopyWords(
                static_cast<const GByte *>(pImag) + nLineSpaceSrc * iLine,
                eSrcBaseType, nPixelSpaceSrc,
                static_cast<GByte *>(pData) + nLineSpace * iLine,
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
                static_cast<GByte *>(pData) + nLineSpace * iLine,
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
    for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
        for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
            // Source raster pixels may be obtained with SRCVAL macro.
            const double adfPixVal[2] = {
                SRCVAL(pReal, eSrcType, ii),  // re
                SRCVAL(pImag, eSrcType, ii)  // im
            };

            GDALCopyWords(adfPixVal, GDT_CFloat64, 0,
                          static_cast<GByte *>(pData) + nLineSpace * iLine +
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
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                const double dfReal = SRCVAL(pReal, eSrcType, ii);
                const double dfImag = SRCVAL(pImag, eSrcType, ii);

                const double dfPixVal =
                    sqrt( dfReal * dfReal + dfImag * dfImag );

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                const double dfPixVal =
                    fabs(SRCVAL(papoSources[0], eSrcType, ii));

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
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
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                const double dfReal = SRCVAL(pReal, eSrcType, ii);
                const double dfImag = SRCVAL(pImag, eSrcType, ii);

                const double dfPixVal = atan2(dfImag, dfReal);

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                const void * const pReal = papoSources[0];

                // Source raster pixels may be obtained with SRCVAL macro.
                const double dfReal = SRCVAL(pReal, eSrcType, ii);
                const double dfPixVal = (dfReal < 0) ? M_PI : 0.0;

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
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
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                const double adfPixVal[2] = {
                    +SRCVAL(pReal, eSrcType, ii),  // re
                    -SRCVAL(pImag, eSrcType, ii)  // im
                };

                GDALCopyWords(
                    adfPixVal, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
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
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                double adfSum[2] = { 0.0, 0.0 };

                for( int iSrc = 0; iSrc < nSources; ++iSrc ) {
                    const void * const pReal = papoSources[iSrc];
                    const void * const pImag =
                        static_cast<const GByte *>(pReal) + nOffset;

                    // Source raster pixels may be obtained with SRCVAL macro.
                    adfSum[0] += SRCVAL(pReal, eSrcType, ii);
                    adfSum[1] += SRCVAL(pImag, eSrcType, ii);
                }

                GDALCopyWords(
                    adfSum, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                double dfSum = 0;  // Not complex.

                for( int iSrc = 0; iSrc < nSources; ++iSrc ) {
                    // Source raster pixels may be obtained with SRCVAL macro.
                    dfSum += SRCVAL(papoSources[iSrc], eSrcType, ii);
                }

                GDALCopyWords(
                    &dfSum, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
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
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                double adfPixVal[2] = {
                    SRCVAL(pReal0, eSrcType, ii)
                    - SRCVAL(pReal1, eSrcType, ii),
                    SRCVAL(pImag0, eSrcType, ii)
                    - SRCVAL(pImag1, eSrcType, ii)
                };

                GDALCopyWords(
                    adfPixVal, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                // Not complex.
                const double dfPixVal = SRCVAL(papoSources[0], eSrcType, ii)
                    - SRCVAL(papoSources[1], eSrcType, ii);

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
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
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                double adfPixVal[2] = { 1.0, 0.0 };

                for( int iSrc = 0; iSrc < nSources; ++iSrc ) {
                    const void * const pReal = papoSources[iSrc];
                    const void * const pImag =
                        static_cast<const GByte *>(pReal) + nOffset;

                    const double dfOldR = adfPixVal[0];
                    const double dfOldI = adfPixVal[1];

                    // Source raster pixels may be obtained with SRCVAL macro.
                    const double dfNewR = SRCVAL(pReal, eSrcType, ii);
                    const double dfNewI = SRCVAL(pImag, eSrcType, ii);

                    adfPixVal[0] = dfOldR * dfNewR - dfOldI * dfNewI;
                    adfPixVal[1] = dfOldR * dfNewI + dfOldI * dfNewR;
                }

                GDALCopyWords(
                    adfPixVal, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                double dfPixVal = 1.0;  // Not complex.

                for( int iSrc = 0; iSrc < nSources; ++iSrc ) {
                    // Source raster pixels may be obtained with SRCVAL macro.
                    dfPixVal *= SRCVAL(papoSources[iSrc], eSrcType, ii);
                }

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
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

        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                const double dfReal0 = SRCVAL(pReal0, eSrcType, ii);
                const double dfReal1 = SRCVAL(pReal1, eSrcType, ii);
                const double dfImag0 = SRCVAL(pImag0, eSrcType, ii);
                const double dfImag1 = SRCVAL(pImag1, eSrcType, ii);
                const double adfPixVal[2] = {
                    dfReal0 * dfReal1 + dfImag0 * dfImag1,
                    dfReal1 * dfImag0 - dfReal0 * dfImag1
                };

                GDALCopyWords(
                    adfPixVal, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }
    else
    {
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                // Not complex.
                const double adfPixVal[2] = {
                    SRCVAL(papoSources[0], eSrcType, ii)
                    * SRCVAL(papoSources[1], eSrcType, ii),
                    0.0
                };

                GDALCopyWords(
                    adfPixVal, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
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

        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                const double dfReal = SRCVAL(pReal, eSrcType, ii);
                const double dfImag = SRCVAL(pImag, eSrcType, ii);
                const double dfAux = dfReal * dfReal + dfImag * dfImag;
                const double adfPixVal[2] = { dfReal / dfAux, -dfImag / dfAux };

                GDALCopyWords(
                    adfPixVal, GDT_CFloat64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                // Not complex.
                const double dfPixVal =
                    1.0 / SRCVAL(papoSources[0], eSrcType, ii);

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
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
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                const double dfReal = SRCVAL(pReal, eSrcType, ii);
                const double dfImag = SRCVAL(pImag, eSrcType, ii);

                const double dfPixVal = dfReal * dfReal + dfImag * dfImag;

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1);
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                double dfPixVal = SRCVAL(papoSources[0], eSrcType, ii);
                dfPixVal *= dfPixVal;

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
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
    for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
        for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
            // Source raster pixels may be obtained with SRCVAL macro.
            const double dfPixVal =
                sqrt( SRCVAL(papoSources[0], eSrcType, ii) );

            GDALCopyWords(
                &dfPixVal, GDT_Float64, 0,
                static_cast<GByte *>(pData) + nLineSpace * iLine +
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
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                const double dfReal = SRCVAL(pReal, eSrcType, ii);
                const double dfImag = SRCVAL(pImag, eSrcType, ii);

                const double dfPixVal =
                    fact * log10( sqrt( dfReal * dfReal + dfImag * dfImag ) );

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
                    iCol * nPixelSpace, eBufType, nPixelSpace, 1 );
            }
        }
    }
    else
    {
        /* ---- Set pixels ---- */
        for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
            for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
                // Source raster pixels may be obtained with SRCVAL macro.
                const double dfPixVal =
                    fact * log10( fabs(
                        SRCVAL(papoSources[0], eSrcType, ii) ) );

                GDALCopyWords(
                    &dfPixVal, GDT_Float64, 0,
                    static_cast<GByte *>(pData) + nLineSpace * iLine +
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
    for( int iLine = 0, ii = 0; iLine < nYSize; ++iLine ) {
        for( int iCol = 0; iCol < nXSize; ++iCol, ++ii ) {
            // Source raster pixels may be obtained with SRCVAL macro.
            const double dfPixVal =
                pow(base, SRCVAL(papoSources[0], eSrcType, ii) / fact);

            GDALCopyWords(
                &dfPixVal, GDT_Float64, 0,
                static_cast<GByte *>(pData) + nLineSpace * iLine +
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

    return CE_None;
}
