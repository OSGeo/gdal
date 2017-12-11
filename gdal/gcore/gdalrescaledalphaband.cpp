/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALRescaledAlphaBand, a class implementing
 *           a band mask based from a non-GDT_Byte alpha band
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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
#include "gdal_priv.h"

#include <cstddef>

#include "cpl_error.h"
#include "cpl_vsi.h"
#include "gdal.h"

CPL_CVSID("$Id$")

//! @cond Doxygen_Suppress
/************************************************************************/
/*                        GDALRescaledAlphaBand()                       */
/************************************************************************/

GDALRescaledAlphaBand::GDALRescaledAlphaBand( GDALRasterBand *poParentIn ) :
    poParent(poParentIn),
    pTemp(nullptr)
{
    CPLAssert(poParent->GetRasterDataType() == GDT_UInt16);

    // Defined in GDALRasterBand.
    poDS = nullptr;
    nBand = 0;

    nRasterXSize = poParent->GetXSize();
    nRasterYSize = poParent->GetYSize();

    eDataType = GDT_Byte;
    poParent->GetBlockSize( &nBlockXSize, &nBlockYSize );
}

/************************************************************************/
/*                      ~GDALRescaledAlphaBand()                        */
/************************************************************************/

GDALRescaledAlphaBand::~GDALRescaledAlphaBand() { VSIFree(pTemp); }

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALRescaledAlphaBand::IReadBlock( int nXBlockOff, int nYBlockOff,
                                          void * pImage )
{
    int nXSizeRequest = nBlockXSize;
    if (nXBlockOff * nBlockXSize + nBlockXSize > nRasterXSize)
        nXSizeRequest = nRasterXSize - nXBlockOff * nBlockXSize;
    int nYSizeRequest = nBlockYSize;
    if (nYBlockOff * nBlockYSize + nBlockYSize > nRasterYSize)
        nYSizeRequest = nRasterYSize - nYBlockOff * nBlockYSize;

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);

    return IRasterIO(
        GF_Read, nXBlockOff * nBlockXSize, nYBlockOff * nBlockYSize,
        nXSizeRequest, nYSizeRequest, pImage,
        nXSizeRequest, nYSizeRequest, GDT_Byte,
        1, nBlockXSize, &sExtraArg );
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALRescaledAlphaBand::IRasterIO(
    GDALRWFlag eRWFlag,
    int nXOff, int nYOff, int nXSize, int nYSize,
    void * pData, int nBufXSize, int nBufYSize,
    GDALDataType eBufType,
    GSpacing nPixelSpace,
    GSpacing nLineSpace,
    GDALRasterIOExtraArg* psExtraArg )
{
    // Optimization in common use case.
    // This avoids triggering the block cache on this band, which helps
    // reducing the global block cache consumption.
    if( eRWFlag == GF_Read && eBufType == GDT_Byte &&
        nXSize == nBufXSize && nYSize == nBufYSize &&
        nPixelSpace == 1 )
    {
        if( pTemp == nullptr )
        {
            pTemp = VSI_MALLOC2_VERBOSE( sizeof(GUInt16), nRasterXSize );
            if( pTemp == nullptr )
            {
                return CE_Failure;
            }
        }
        for( int j = 0; j < nBufYSize; j++ )
        {
            const CPLErr eErr =
                poParent->RasterIO( GF_Read, nXOff, nYOff + j, nXSize, 1,
                                    pTemp, nBufXSize, 1,
                                    GDT_UInt16,
                                    0, 0, nullptr );
            if( eErr != CE_None )
                return eErr;

            GByte* pabyImage = static_cast<GByte*>(pData) + j * nLineSpace;
            GUInt16* pSrc = static_cast<GUInt16 *>(pTemp);

            for( int i = 0; i < nBufXSize; i++ )
            {
                // In case the dynamics was actually 0-255 and not 0-65535 as
                // expected, we want to make sure non-zero alpha will still
                // be non-zero.
                if( pSrc[i] > 0 && pSrc[i] < 257 )
                    pabyImage[i] = 1;
                else
                    pabyImage[i] = static_cast<GByte>((pSrc[i] * 255) / 65535);
            }
        }
        return CE_None;
    }

    return GDALRasterBand::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                      pData, nBufXSize, nBufYSize,
                                      eBufType,
                                      nPixelSpace, nLineSpace, psExtraArg );
}
//! @endcond
