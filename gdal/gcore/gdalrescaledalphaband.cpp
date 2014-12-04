/******************************************************************************
 * $Id$
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

#include "gdal_priv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        GDALRescaledAlphaBand()                       */
/************************************************************************/

GDALRescaledAlphaBand::GDALRescaledAlphaBand( GDALRasterBand *poParent )

{
    CPLAssert(poParent->GetRasterDataType() == GDT_UInt16);

    poDS = NULL;
    nBand = 0;

    nRasterXSize = poParent->GetXSize();
    nRasterYSize = poParent->GetYSize();

    eDataType = GDT_Byte;
    poParent->GetBlockSize( &nBlockXSize, &nBlockYSize );

    this->poParent = poParent;

    pTemp = NULL;
}

/************************************************************************/
/*                      ~GDALRescaledAlphaBand()                        */
/************************************************************************/

GDALRescaledAlphaBand::~GDALRescaledAlphaBand()

{
    VSIFree(pTemp);
}

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

    return IRasterIO(GF_Read, nXBlockOff * nBlockXSize, nYBlockOff * nBlockYSize,
                     nXSizeRequest, nYSizeRequest, pImage,
                     nXSizeRequest, nYSizeRequest, GDT_Byte,
                     1, nBlockXSize, &sExtraArg);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALRescaledAlphaBand::IRasterIO( GDALRWFlag eRWFlag,
                                      int nXOff, int nYOff, int nXSize, int nYSize,
                                      void * pData, int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType,
                                      GSpacing nPixelSpace,
                                      GSpacing nLineSpace,
                                      GDALRasterIOExtraArg* psExtraArg )
{
    /* Optimization in common use case */
    /* This avoids triggering the block cache on this band, which helps */
    /* reducing the global block cache consumption */
    if (eRWFlag == GF_Read && eBufType == GDT_Byte &&
        nXSize == nBufXSize && nYSize == nBufYSize &&
        nPixelSpace == 1)
    {
        if( pTemp == NULL )
        {
            pTemp = VSIMalloc2( sizeof(GUInt16), nRasterXSize );
            if (pTemp == NULL)
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                        "GDALRescaledAlphaBand::IReadBlock: Out of memory for buffer." );
                return CE_Failure;
            }
        }
        for(int j = 0; j < nBufYSize; j++ )
        {
            CPLErr eErr = poParent->RasterIO( GF_Read, nXOff, nYOff + j, nXSize, 1,
                                              pTemp, nBufXSize, 1,
                                              GDT_UInt16,
                                              0, 0, NULL );
            if (eErr != CE_None)
                return eErr;

            GByte* pabyImage = ((GByte*) pData) + j * nLineSpace;
            GUInt16* pSrc = (GUInt16 *)pTemp;

            for( int i = 0; i < nBufXSize; i++ )
            {
                /* In case the dynamics was actually 0-255 and not 0-65535 as */
                /* expected, we want to make sure non-zero alpha will still be non-zero */
                if( pSrc[i] > 0 && pSrc[i] < 257 )
                    pabyImage[i] = 1;
                else
                    pabyImage[i] = (pSrc[i] * 255) / 65535;
            }
        }
        return CE_None;
    }

    return GDALRasterBand::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                      pData, nBufXSize, nBufYSize,
                                      eBufType,
                                      nPixelSpace, nLineSpace, psExtraArg );
}
