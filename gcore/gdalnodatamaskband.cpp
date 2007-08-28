/******************************************************************************
 * $Id: gdalrasterband.cpp 11880 2007-08-13 15:02:33Z mloskot $
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALNoDataMaskBand, a class implementing all
 *           a default band mask based on nodata values.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam
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

/************************************************************************/
/*                        GDALNoDataMaskBand()                          */
/************************************************************************/

GDALNoDataMaskBand::GDALNoDataMaskBand( GDALRasterBand *poParent )

{
    poDS = NULL;
    nBand = 0;

    nRasterXSize = poParent->GetXSize();
    nRasterYSize = poParent->GetYSize();

    eDataType = GDT_Byte;
    poParent->GetBlockSize( &nBlockXSize, &nBlockYSize );

    this->poParent = poParent;
    dfNoDataValue = poParent->GetNoDataValue();
}

/************************************************************************/
/*                       ~GDALNoDataMaskBand()                          */
/************************************************************************/

GDALNoDataMaskBand::~GDALNoDataMaskBand()

{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALNoDataMaskBand::IReadBlock( int nXBlockOff, int nYBlockOff,
                                         void * pImage )

{
    GDALDataType eWrkDT;
  
/* -------------------------------------------------------------------- */
/*      Decide on a working type.                                       */
/* -------------------------------------------------------------------- */
    switch( poParent->GetRasterDataType() ) 
    {
      case GDT_Byte:
        eWrkDT = GDT_Byte;
        break;

      case GDT_UInt16:
      case GDT_Int16:
      case GDT_Int32:
      case GDT_UInt32:
        eWrkDT = GDT_UInt32;
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
        CPLAssert( FALSE );
        eWrkDT = GDT_Float64;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Read the image data.                                            */
/* -------------------------------------------------------------------- */
    GByte *pabySrc;
    CPLErr eErr;

    pabySrc = (GByte *)
        CPLMalloc( (GDALGetDataTypeSize(eWrkDT)/8)*nBlockXSize*nBlockYSize );

    eErr = poParent->ReadBlock( nXBlockOff, nYBlockOff, pabySrc );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Process different cases.                                        */
/* -------------------------------------------------------------------- */
    int i;
    switch( eWrkDT )
    {
      case GDT_Byte:
      {
          GByte byNoData = (GByte) dfNoDataValue;

          for( i = nBlockXSize * nBlockYSize - 1; i >= 0; i-- )
          {
              if( pabySrc[i] == byNoData )
                  ((GByte *) pImage)[i] = 0;
              else
                  ((GByte *) pImage)[i] = 255;
          }
      }
      break;

      case GDT_UInt32:
      {
          GUInt32 nNoData = (GUInt32) dfNoDataValue;

          for( i = nBlockXSize * nBlockYSize - 1; i >= 0; i-- )
          {
              if( ((GUInt32 *)pabySrc)[i] == nNoData )
                  ((GByte *) pImage)[i] = 0;
              else
                  ((GByte *) pImage)[i] = 255;
          }
      }
      break;

      case GDT_Float32:
      {
          float fNoData = (float) dfNoDataValue;

          for( i = nBlockXSize * nBlockYSize - 1; i >= 0; i-- )
          {
              if( ((float *)pabySrc)[i] == fNoData )
                  ((GByte *) pImage)[i] = 0;
              else
                  ((GByte *) pImage)[i] = 255;
          }
      }
      break;

      case GDT_Float64:
      {
          for( i = nBlockXSize * nBlockYSize - 1; i >= 0; i-- )
          {
              if( ((double *)pabySrc)[i] == dfNoDataValue )
                  ((GByte *) pImage)[i] = 0;
              else
                  ((GByte *) pImage)[i] = 255;
          }
      }
      break;

      default:
        CPLAssert( FALSE );
        break;
    }

    CPLFree( pabySrc );

    return CE_None;
}
