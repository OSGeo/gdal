/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALNoDataValuesMaskBand, a class implementing 
 *           a default band mask based on the NODATA_VALUES metadata item.
 *           A pixel is considered nodata in all bands if and only if all bands
 *           match the corresponding value in the NODATA_VALUES tuple
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot ogr>
 *
 ******************************************************************************
 * Copyright (c) 2008-2009, Even Rouault <even dot rouault at mines-paris dot org>
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
/*                   GDALNoDataValuesMaskBand()                         */
/************************************************************************/

GDALNoDataValuesMaskBand::GDALNoDataValuesMaskBand( GDALDataset* poDS )

{
    const char* pszNoDataValues = poDS->GetMetadataItem("NODATA_VALUES");
    char** papszNoDataValues = CSLTokenizeStringComplex(pszNoDataValues, " ", FALSE, FALSE);

    int i;
    padfNodataValues = (double*)CPLMalloc(sizeof(double) * poDS->GetRasterCount());
    for(i=0;i<poDS->GetRasterCount();i++)
    {
        padfNodataValues[i] = CPLAtof(papszNoDataValues[i]);
    }

    CSLDestroy(papszNoDataValues);

    this->poDS = poDS;
    nBand = 0;

    nRasterXSize = poDS->GetRasterXSize();
    nRasterYSize = poDS->GetRasterYSize();

    eDataType = GDT_Byte;
    poDS->GetRasterBand(1)->GetBlockSize( &nBlockXSize, &nBlockYSize );
}

/************************************************************************/
/*                    ~GDALNoDataValuesMaskBand()                       */
/************************************************************************/

GDALNoDataValuesMaskBand::~GDALNoDataValuesMaskBand()

{
    CPLFree(padfNodataValues);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALNoDataValuesMaskBand::IReadBlock( int nXBlockOff, int nYBlockOff,
                                         void * pImage )

{
    int iBand;
    GDALDataType eWrkDT;
  
/* -------------------------------------------------------------------- */
/*      Decide on a working type.                                       */
/* -------------------------------------------------------------------- */
    switch( poDS->GetRasterBand(1)->GetRasterDataType() ) 
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
        CPLAssert( FALSE );
        eWrkDT = GDT_Float64;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Read the image data.                                            */
/* -------------------------------------------------------------------- */
    GByte *pabySrc;
    CPLErr eErr;

    int nBands = poDS->GetRasterCount();
    pabySrc = (GByte *) VSIMalloc3( nBands * GDALGetDataTypeSize(eWrkDT)/8, nBlockXSize, nBlockYSize );
    if (pabySrc == NULL)
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "GDALNoDataValuesMaskBand::IReadBlock: Out of memory for buffer." );
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
        /* memset the whole buffer to avoid Valgrind warnings in case we can't */
        /* fetch a full block */
        memset(pabySrc, 0, nBands * GDALGetDataTypeSize(eWrkDT)/8 * nBlockXSize * nBlockYSize );
    }

    int nBlockOffsetPixels = nBlockXSize * nBlockYSize;
    int nBandOffsetByte = (GDALGetDataTypeSize(eWrkDT)/8) * nBlockXSize * nBlockYSize;
    for(iBand=0;iBand<nBands;iBand++)
    {
        eErr = poDS->GetRasterBand(iBand + 1)->RasterIO(
                                   GF_Read,
                                   nXBlockOff * nBlockXSize, nYBlockOff * nBlockYSize,
                                   nXSizeRequest, nYSizeRequest,
                                   pabySrc + iBand * nBandOffsetByte, nXSizeRequest, nYSizeRequest,
                                   eWrkDT, 0, nBlockXSize * (GDALGetDataTypeSize(eWrkDT)/8),
                                   NULL);
        if( eErr != CE_None )
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Process different cases.                                        */
/* -------------------------------------------------------------------- */
    int i;
    switch( eWrkDT )
    {
      case GDT_Byte:
      {
          GByte* pabyNoData = (GByte*) CPLMalloc(nBands * sizeof(GByte));
          for(iBand=0;iBand<nBands;iBand++)
          {
              pabyNoData[iBand] = (GByte)padfNodataValues[iBand];
          }

          for( i = nBlockXSize * nBlockYSize - 1; i >= 0; i-- )
          {
              int nCountNoData = 0;
              for(iBand=0;iBand<nBands;iBand++)
              {
                  if( pabySrc[i + iBand * nBlockOffsetPixels] == pabyNoData[iBand] )
                      nCountNoData ++;
              }
              if (nCountNoData == nBands)
                  ((GByte *) pImage)[i] = 0;
              else
                  ((GByte *) pImage)[i] = 255;
          }

          CPLFree(pabyNoData);
      }
      break;

      case GDT_UInt32:
      {
          GUInt32* panNoData = (GUInt32*) CPLMalloc(nBands * sizeof(GUInt32));
          for(iBand=0;iBand<nBands;iBand++)
          {
              panNoData[iBand] = (GUInt32)padfNodataValues[iBand];
          }

          for( i = nBlockXSize * nBlockYSize - 1; i >= 0; i-- )
          {
              int nCountNoData = 0;
              for(iBand=0;iBand<nBands;iBand++)
              {
                  if( ((GUInt32 *)pabySrc)[i + iBand * nBlockOffsetPixels] == panNoData[iBand] )
                      nCountNoData ++;
              }
              if (nCountNoData == nBands)
                  ((GByte *) pImage)[i] = 0;
              else
                  ((GByte *) pImage)[i] = 255;
          }

          CPLFree(panNoData);
      }
      break;

      case GDT_Int32:
      {
          GInt32* panNoData = (GInt32*) CPLMalloc(nBands * sizeof(GInt32));
          for(iBand=0;iBand<nBands;iBand++)
          {
              panNoData[iBand] = (GInt32)padfNodataValues[iBand];
          }

          for( i = nBlockXSize * nBlockYSize - 1; i >= 0; i-- )
          {
              int nCountNoData = 0;
              for(iBand=0;iBand<nBands;iBand++)
              {
                  if( ((GInt32 *)pabySrc)[i + iBand * nBlockOffsetPixels] == panNoData[iBand] )
                      nCountNoData ++;
              }
              if (nCountNoData == nBands)
                  ((GByte *) pImage)[i] = 0;
              else
                  ((GByte *) pImage)[i] = 255;
          }

          CPLFree(panNoData);
      }
      break;

      case GDT_Float32:
      {
          float* pafNoData = (float*) CPLMalloc(nBands * sizeof(float));
          for(iBand=0;iBand<nBands;iBand++)
          {
              pafNoData[iBand] = (float)padfNodataValues[iBand];
          }

          for( i = nBlockXSize * nBlockYSize - 1; i >= 0; i-- )
          {
              int nCountNoData = 0;
              for(iBand=0;iBand<nBands;iBand++)
              {
                  if( ((float *)pabySrc)[i + iBand * nBlockOffsetPixels] == pafNoData[iBand] )
                      nCountNoData ++;
              }
              if (nCountNoData == nBands)
                  ((GByte *) pImage)[i] = 0;
              else
                  ((GByte *) pImage)[i] = 255;
          }

          CPLFree(pafNoData);
      }
      break;

      case GDT_Float64:
      {
          double* padfNoData = (double*) CPLMalloc(nBands * sizeof(double));
          for(iBand=0;iBand<nBands;iBand++)
          {
              padfNoData[iBand] = (double)padfNodataValues[iBand];
          }

          for( i = nBlockXSize * nBlockYSize - 1; i >= 0; i-- )
          {
              int nCountNoData = 0;
              for(iBand=0;iBand<nBands;iBand++)
              {
                  if( ((double *)pabySrc)[i + iBand * nBlockOffsetPixels] == padfNoData[iBand] )
                      nCountNoData ++;
              }
              if (nCountNoData == nBands)
                  ((GByte *) pImage)[i] = 0;
              else
                  ((GByte *) pImage)[i] = 255;
          }

          CPLFree(padfNoData);
      }
      break;

      default:
        CPLAssert( FALSE );
        break;
    }

    CPLFree( pabySrc );

    return CE_None;
}
