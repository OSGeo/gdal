/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Compute simple checksum for a region of image data.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam
 * Copyright (c) 2007-2008, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "gdal_alg.h"

#include <cmath>
#include <cstddef>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "gdal.h"


CPL_CVSID("$Id$")

/************************************************************************/
/*                         GDALChecksumImage()                          */
/************************************************************************/

/**
 * Compute checksum for image region.
 *
 * Computes a 16bit (0-65535) checksum from a region of raster data on a GDAL
 * supported band.   Floating point data is converted to 32bit integer
 * so decimal portions of such raster data will not affect the checksum.
 * Real and Imaginary components of complex bands influence the result.
 *
 * @param hBand the raster band to read from.
 * @param nXOff pixel offset of window to read.
 * @param nYOff line offset of window to read.
 * @param nXSize pixel size of window to read.
 * @param nYSize line size of window to read.
 *
 * @return Checksum value.
 */

int CPL_STDCALL
GDALChecksumImage( GDALRasterBandH hBand,
                   int nXOff, int nYOff, int nXSize, int nYSize )

{
    VALIDATE_POINTER1( hBand, "GDALChecksumImage", 0 );

    const static int anPrimes[11] =
        { 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43 };

    int nChecksum = 0;
    int iPrime = 0;
    const GDALDataType eDataType = GDALGetRasterDataType(hBand);
    const bool bComplex = CPL_TO_BOOL(GDALDataTypeIsComplex(eDataType));

    if( eDataType == GDT_Float32 || eDataType == GDT_Float64 ||
        eDataType == GDT_CFloat32 || eDataType == GDT_CFloat64 )
    {
        const GDALDataType eDstDataType = bComplex ? GDT_CFloat64 : GDT_Float64;

        double* padfLineData = static_cast<double *>(
            VSI_MALLOC2_VERBOSE(nXSize,
                                GDALGetDataTypeSizeBytes(eDstDataType)));
        if( padfLineData == nullptr )
        {
            return 0;
        }

        for( int iLine = nYOff; iLine < nYOff + nYSize; iLine++ )
        {
            if( GDALRasterIO( hBand, GF_Read, nXOff, iLine, nXSize, 1,
                              padfLineData, nXSize, 1,
                              eDstDataType, 0, 0 ) != CE_None )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Checksum value couldn't be computed due to "
                         "I/O read error.");
                break;
            }
            const int nCount = bComplex ? nXSize * 2 : nXSize;

            for( int i = 0; i < nCount; i++ )
            {
                double dfVal = padfLineData[i];
                int nVal;
                if( CPLIsNan(dfVal) || CPLIsInf(dfVal) )
                {
                    // Most compilers seem to cast NaN or Inf to 0x80000000.
                    // but VC7 is an exception. So we force the result
                    // of such a cast.
                    nVal = 0x80000000;
                }
                else
                {
                    // Standard behavior of GDALCopyWords when converting
                    // from floating point to Int32.
                    dfVal += 0.5;

                    if( dfVal < -2147483647.0 )
                        nVal = -2147483647;
                    else if( dfVal > 2147483647 )
                        nVal = 2147483647;
                    else
                        nVal = static_cast<GInt32>(floor(dfVal));
                }

                nChecksum += nVal % anPrimes[iPrime++];
                if( iPrime > 10 )
                    iPrime = 0;

                nChecksum &= 0xffff;
            }
        }

        CPLFree(padfLineData);
    }
    else
    {
        const GDALDataType eDstDataType = bComplex ? GDT_CInt32 : GDT_Int32;

        int *panLineData = static_cast<GInt32 *>(
            VSI_MALLOC2_VERBOSE(nXSize,
                                GDALGetDataTypeSizeBytes(eDstDataType)));
        if( panLineData == nullptr )
        {
            return 0;
        }

        for( int iLine = nYOff; iLine < nYOff + nYSize; iLine++ )
        {
            if( GDALRasterIO( hBand, GF_Read, nXOff, iLine, nXSize, 1,
                              panLineData, nXSize, 1, eDstDataType,
                              0, 0 ) != CE_None )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Checksum value could not be computed due to I/O "
                         "read error.");
                break;
            }
            const int nCount = bComplex ? nXSize * 2 : nXSize;

            for( int i = 0; i < nCount; i++ )
            {
                nChecksum += panLineData[i] % anPrimes[iPrime++];
                if( iPrime > 10 )
                    iPrime = 0;

                nChecksum &= 0xffff;
            }
        }

        CPLFree( panLineData );
    }

    return nChecksum;
}
