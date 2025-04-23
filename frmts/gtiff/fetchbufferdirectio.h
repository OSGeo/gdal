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
 * SPDX-License-Identifier: MIT
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
