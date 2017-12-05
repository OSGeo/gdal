/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALNoDataMaskBand, a class implementing all
 *           a default band mask based on nodata values.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv_templates.hpp"

CPL_CVSID("$Id$")

//! @cond Doxygen_Suppress
/************************************************************************/
/*                        GDALNoDataMaskBand()                          */
/************************************************************************/

GDALNoDataMaskBand::GDALNoDataMaskBand( GDALRasterBand *poParentIn ) :
    dfNoDataValue(poParentIn->GetNoDataValue()),
    poParent(poParentIn)
{
    poDS = NULL;
    nBand = 0;

    nRasterXSize = poParent->GetXSize();
    nRasterYSize = poParent->GetYSize();

    eDataType = GDT_Byte;
    poParent->GetBlockSize( &nBlockXSize, &nBlockYSize );
}

/************************************************************************/
/*                       ~GDALNoDataMaskBand()                          */
/************************************************************************/

GDALNoDataMaskBand::~GDALNoDataMaskBand() {}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALNoDataMaskBand::IReadBlock( int nXBlockOff, int nYBlockOff,
                                       void * pImage )

{
    GDALDataType eWrkDT = GDT_Unknown;

/* -------------------------------------------------------------------- */
/*      Decide on a working type.                                       */
/* -------------------------------------------------------------------- */
    switch( poParent->GetRasterDataType() )
    {
      case GDT_Byte:
        eWrkDT = GDT_Byte;
        break;

      case GDT_UInt16:
      case GDT_UInt32:
        eWrkDT = GDT_UInt32;
        break;

      case GDT_Int16:
      case GDT_Int32:
      case GDT_CInt16:
      case GDT_CInt32:
        eWrkDT = GDT_Int32;
        break;

      case GDT_Float32:
      case GDT_CFloat32:
        eWrkDT = GDT_Float32;
        break;

      case GDT_Float64:
      case GDT_CFloat64:
        eWrkDT = GDT_Float64;
        break;

      default:
        CPLAssert( false );
        eWrkDT = GDT_Float64;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Read the image data.                                            */
/* -------------------------------------------------------------------- */
    // TODO(schwehr): pabySrc would probably be better as a void ptr.
    GByte *pabySrc = static_cast<GByte *>(
        VSI_MALLOC3_VERBOSE( GDALGetDataTypeSizeBytes(eWrkDT),
                             nBlockXSize, nBlockYSize ) );
    if (pabySrc == NULL)
    {
        return CE_Failure;
    }

    int nXSizeRequest = nBlockXSize;
    if (nXBlockOff * nBlockXSize + nBlockXSize > nRasterXSize)
        nXSizeRequest = nRasterXSize - nXBlockOff * nBlockXSize;
    int nYSizeRequest = nBlockYSize;
    if (nYBlockOff * nBlockYSize + nBlockYSize > nRasterYSize)
        nYSizeRequest = nRasterYSize - nYBlockOff * nBlockYSize;

    if (nXSizeRequest != nBlockXSize || nYSizeRequest != nBlockYSize)
    {
        // memset the whole buffer to avoid Valgrind warnings in case RasterIO
        // fetches a partial block.
        memset( pabySrc, 0,
                GDALGetDataTypeSizeBytes(eWrkDT) * nBlockXSize * nBlockYSize );
    }

    CPLErr eErr =
        poParent->RasterIO( GF_Read,
                            nXBlockOff * nBlockXSize,
                            nYBlockOff * nBlockYSize,
                            nXSizeRequest, nYSizeRequest,
                            pabySrc, nXSizeRequest, nYSizeRequest,
                            eWrkDT, 0,
                            nBlockXSize * GDALGetDataTypeSizeBytes(eWrkDT),
                            NULL );
    if( eErr != CE_None )
    {
        CPLFree(pabySrc);
        return eErr;
    }

    const bool bIsNoDataNan = CPLIsNan(dfNoDataValue) != 0;

/* -------------------------------------------------------------------- */
/*      Process different cases.                                        */
/* -------------------------------------------------------------------- */
    switch( eWrkDT )
    {
      case GDT_Byte:
      {
          if( !GDALIsValueInRange<GByte>(dfNoDataValue) )
          {
              memset(pImage, 255, nBlockXSize * nBlockYSize);
          }
          else
          {
            GByte byNoData = static_cast<GByte>( dfNoDataValue );

            for( int i = 0; i < nBlockXSize * nBlockYSize; i++ )
            {
                static_cast<GByte *>(pImage)[i] = pabySrc[i] == byNoData ? 0: 255;
            }
          }
      }
      break;

      case GDT_UInt32:
      {
          if( !GDALIsValueInRange<GUInt32>(dfNoDataValue) )
          {
              memset(pImage, 255, nBlockXSize * nBlockYSize);
          }
          else
          {
            GUInt32 nNoData = static_cast<GUInt32>( dfNoDataValue );

            for( int i = 0; i < nBlockXSize * nBlockYSize; i++ )
            {
                static_cast<GByte *>(pImage)[i] =
                    reinterpret_cast<GUInt32 *>(pabySrc)[i] == nNoData ? 0 : 255;
            }
          }
      }
      break;

      case GDT_Int32:
      {
          if( !GDALIsValueInRange<GInt32>(dfNoDataValue) )
          {
              memset(pImage, 255, nBlockXSize * nBlockYSize);
          }
          else
          {
            GInt32 nNoData = static_cast<GInt32>( dfNoDataValue );

            for( int i = 0; i < nBlockXSize * nBlockYSize; i++ )
            {
                static_cast<GByte *>(pImage)[i] =
                    reinterpret_cast<GInt32 *>(pabySrc)[i] == nNoData ? 0 : 255;
            }
          }
      }
      break;

      case GDT_Float32:
      {
          if( !bIsNoDataNan && !CPLIsInf(dfNoDataValue) &&
              !GDALIsValueInRange<float>(dfNoDataValue) )
          {
              memset(pImage, 255, nBlockXSize * nBlockYSize);
          }
          else
          {
            float fNoData = static_cast<float>( dfNoDataValue );

            for( int i = 0; i < nBlockXSize * nBlockYSize; i++ )
            {
                const float fVal = reinterpret_cast<float *>(pabySrc)[i];
                if( bIsNoDataNan && CPLIsNan(fVal))
                    static_cast<GByte *>(pImage)[i] = 0;
                else if( ARE_REAL_EQUAL(fVal, fNoData) )
                    static_cast<GByte *>(pImage)[i] = 0;
                else
                    static_cast<GByte *>(pImage)[i] = 255;
            }
          }
      }
      break;

      case GDT_Float64:
      {
          for( int i = 0; i < nBlockXSize * nBlockYSize; i++ )
          {
              const double dfVal = reinterpret_cast<double *>(pabySrc)[i];
              if( bIsNoDataNan && CPLIsNan(dfVal))
                  static_cast<GByte *>(pImage)[i] = 0;
              else if( ARE_REAL_EQUAL(dfVal, dfNoDataValue) )
                  static_cast<GByte *>(pImage)[i] = 0;
              else
                  static_cast<GByte *>(pImage)[i] = 255;
          }
      }
      break;

      default:
        CPLAssert( false );
        break;
    }

    CPLFree( pabySrc );

    return CE_None;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALNoDataMaskBand::IRasterIO( GDALRWFlag eRWFlag,
                                      int nXOff, int nYOff,
                                      int nXSize, int nYSize,
                                      void * pData,
                                      int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType,
                                      GSpacing nPixelSpace, GSpacing nLineSpace,
                                      GDALRasterIOExtraArg* psExtraArg )
{
    // Optimization in common use case (#4488).
    // This avoids triggering the block cache on this band, which helps
    // reducing the global block cache consumption.
    if (eRWFlag == GF_Read && eBufType == GDT_Byte &&
        poParent->GetRasterDataType() == GDT_Byte &&
        nXSize == nBufXSize && nYSize == nBufYSize &&
        nPixelSpace == 1 && nLineSpace == nBufXSize)
    {
        const CPLErr eErr =
            poParent->RasterIO( GF_Read, nXOff, nYOff, nXSize, nYSize,
                                pData, nBufXSize, nBufYSize,
                                eBufType,
                                nPixelSpace, nLineSpace, psExtraArg );
        if (eErr != CE_None)
            return eErr;

        GByte* pabyData = static_cast<GByte*>( pData );
        GByte byNoData = static_cast<GByte>( dfNoDataValue );

        for( int i = nBufXSize * nBufYSize - 1; i >= 0; --i )
        {
            pabyData[i] = pabyData[i] == byNoData ? 0 : 255;
        }
        return CE_None;
    }

    return GDALRasterBand::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                      pData, nBufXSize, nBufYSize,
                                      eBufType,
                                      nPixelSpace, nLineSpace, psExtraArg );
}
//! @endcond
