/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef FETCHBUFFERDIRECTIO_H_INCLUDED
#define FETCHBUFFERDIRECTIO_H_INCLUDED

#include "cpl_error.h"
#include "cpl_vsi.h"
#include "gdal.h"

/************************************************************************/
/*                        FetchBufferDirectIO                           */
/************************************************************************/

class FetchBufferDirectIO final
{
    VSILFILE *fp;
    GByte *pTempBuffer;
    size_t nTempBufferSize;

  public:
    FetchBufferDirectIO(VSILFILE *fpIn, GByte *pTempBufferIn,
                        size_t nTempBufferSizeIn)
        : fp(fpIn), pTempBuffer(pTempBufferIn),
          nTempBufferSize(nTempBufferSizeIn)
    {
    }

    const GByte *FetchBytes(vsi_l_offset nOffset, int nPixels, int nDTSize,
                            bool bIsByteSwapped, bool bIsComplex, int nBlockId)
    {
        if (!FetchBytes(pTempBuffer, nOffset, nPixels, nDTSize, bIsByteSwapped,
                        bIsComplex, nBlockId))
        {
            return nullptr;
        }
        return pTempBuffer;
    }

    bool FetchBytes(GByte *pabyDstBuffer, vsi_l_offset nOffset, int nPixels,
                    int nDTSize, bool bIsByteSwapped, bool bIsComplex,
                    int nBlockId)
    {
        vsi_l_offset nSeekForward = 0;
        if (nOffset <= VSIFTellL(fp) ||
            (nSeekForward = nOffset - VSIFTellL(fp)) > nTempBufferSize)
        {
            if (VSIFSeekL(fp, nOffset, SEEK_SET) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot seek to block %d",
                         nBlockId);
                return false;
            }
        }
        else
        {
            while (nSeekForward > 0)
            {
                vsi_l_offset nToRead = nSeekForward;
                if (nToRead > nTempBufferSize)
                    nToRead = nTempBufferSize;
                if (VSIFReadL(pTempBuffer, static_cast<size_t>(nToRead), 1,
                              fp) != 1)
                {
                    CPLError(CE_Failure, CPLE_FileIO, "Cannot seek to block %d",
                             nBlockId);
                    return false;
                }
                nSeekForward -= nToRead;
            }
        }
        if (VSIFReadL(pabyDstBuffer, static_cast<size_t>(nPixels) * nDTSize, 1,
                      fp) != 1)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Missing data for block %d",
                     nBlockId);
            return false;
        }

        if (bIsByteSwapped)
        {
            if (bIsComplex)
                GDALSwapWords(pabyDstBuffer, nDTSize / 2, 2 * nPixels,
                              nDTSize / 2);
            else
                GDALSwapWords(pabyDstBuffer, nDTSize, nPixels, nDTSize);
        }
        return true;
    }

    static const bool bMinimizeIO = true;
};

#endif  // FETCHBUFFERDIRECTIO_H_INCLUDED
