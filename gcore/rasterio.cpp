/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Contains default implementation of GDALRasterBand::IRasterIO()
 *           and supporting functions of broader utility.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.16  2002/07/09 20:33:12  warmerda
 * expand tabs
 *
 * Revision 1.15  2002/05/31 22:18:50  warmerda
 * Ensure that GDALCopyWords() rounds off (nearest) rather than rounding
 * down copying from float to integer outputs, and uses floor() when assigning
 * to signed integer output to ensure consistent rounding behaviour across 0.
 *
 * Revision 1.14  2001/07/18 04:04:31  warmerda
 * added CPL_CVSID
 *
 * Revision 1.13  2000/08/16 15:50:52  warmerda
 * fixed some bugs with floating (datasetless) bands
 *
 * Revision 1.12  2000/07/13 13:08:53  warmerda
 * fixed GDALSwapWords with skip value different from word size
 *
 * Revision 1.11  2000/06/05 17:24:05  warmerda
 * added real complex support
 *
 * Revision 1.10  2000/05/15 14:33:49  warmerda
 * don't crash on read failure
 *
 * Revision 1.9  2000/04/04 15:25:13  warmerda
 * Fixed embarrasing bug in GDALCopyWords() for some cases.
 *
 * Revision 1.8  2000/03/06 18:57:07  warmerda
 * Fixed bug in 1:1 special case code.
 *
 * Revision 1.7  2000/03/06 02:22:13  warmerda
 * added overview support
 *
 * Revision 1.6  1999/11/23 18:44:10  warmerda
 * Fixed GDALCopyWords!
 *
 * Revision 1.5  1999/07/23 19:36:09  warmerda
 * added support for data type translation and a swapping function
 *
 * Revision 1.4  1999/01/11 15:38:38  warmerda
 * Added optimized case for simple 1:1 copies.
 *
 * Revision 1.3  1999/01/02 21:14:01  warmerda
 * Added write support
 *
 * Revision 1.2  1998/12/31 18:54:25  warmerda
 * Implement initial GDALRasterBlock support, and block cache
 *
 * Revision 1.1  1998/12/06 22:15:42  warmerda
 * New
 */

#include "gdal_priv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                             IRasterIO()                              */
/*                                                                      */
/*      Default internal implementation of RasterIO() ... utilizes      */
/*      the Block access methods to satisfy the request.  This would    */
/*      normally only be overridden by formats with overviews.          */
/************************************************************************/

CPLErr GDALRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  int nPixelSpace, int nLineSpace )

{
    int         nBandDataSize = GDALGetDataTypeSize( eDataType ) / 8;
    GByte       *pabySrcBlock = NULL;
    GDALRasterBlock *poBlock;
    int         nLBlockX=-1, nLBlockY=-1, iBufYOff, iBufXOff, iSrcY;

/* ==================================================================== */
/*      A common case is the data requested with it's inherent data     */
/*      type, the destination is packed, and the block width is the     */
/*      raster width.                                                   */
/* ==================================================================== */
    if( eBufType == eDataType
        && nPixelSpace == GDALGetDataTypeSize(eBufType)/8
        && nLineSpace == nPixelSpace * nXSize
        && nBlockXSize == GetXSize()
        && nBufXSize == nXSize 
        && nBufYSize == nYSize )
    {
        for( iBufYOff = 0; iBufYOff < nBufYSize; iBufYOff++ )
        {
            int         nSrcByteOffset;
            
            iSrcY = iBufYOff + nYOff;
            
            if( iSrcY < nLBlockY * nBlockYSize
                || iSrcY >= (nLBlockY+1) * nBlockYSize )
            {
                nLBlockY = iSrcY / nBlockYSize;

                poBlock = GetBlockRef( 0, nLBlockY );
                if( poBlock == NULL )
                {
                    return( CE_Failure );
                }

                if( eRWFlag == GF_Write )
                    poBlock->MarkDirty();
                
                pabySrcBlock = (GByte *) poBlock->GetDataRef();
            }

            nSrcByteOffset = ((iSrcY-nLBlockY*nBlockYSize)*nBlockXSize + nXOff)
                * nPixelSpace;
            
            if( eRWFlag == GF_Write )
                memcpy( pabySrcBlock + nSrcByteOffset, 
                        ((GByte *) pData) + iBufYOff * nLineSpace,
                        nLineSpace );
            else
                memcpy( ((GByte *) pData) + iBufYOff * nLineSpace,
                        pabySrcBlock + nSrcByteOffset, 
                        nLineSpace );
        }

        return CE_None;
    }
    
/* ==================================================================== */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* ==================================================================== */
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && GetOverviewCount() > 0 && eRWFlag == GF_Read )
    {
        if( OverviewRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                              pData, nBufXSize, nBufYSize, 
                              eBufType, nPixelSpace, nLineSpace ) == CE_None )
            return CE_None;
    }
    
/* ==================================================================== */
/*      Loop reading required source blocks to satisfy output           */
/*      request.  This is the most general implementation.              */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Compute stepping increment.                                     */
/* -------------------------------------------------------------------- */
    double      dfSrcX, dfSrcY, dfSrcXInc, dfSrcYInc;
    int         iSrcX;
    
    dfSrcXInc = nXSize / (double) nBufXSize;
    dfSrcYInc = nYSize / (double) nBufYSize;

/* -------------------------------------------------------------------- */
/*      Loop over buffer computing source locations.                    */
/* -------------------------------------------------------------------- */
    for( iBufYOff = 0; iBufYOff < nBufYSize; iBufYOff++ )
    {
        int     iBufOffset, iSrcOffset;
        
        dfSrcY = (iBufYOff+0.5) * dfSrcYInc + nYOff;
        iSrcY = (int) dfSrcY;

        iBufOffset = iBufYOff * nLineSpace;
        
        for( iBufXOff = 0; iBufXOff < nBufXSize; iBufXOff++ )
        {
            dfSrcX = (iBufXOff+0.5) * dfSrcXInc + nXOff;
            
            iSrcX = (int) dfSrcX;

/* -------------------------------------------------------------------- */
/*      Ensure we have the appropriate block loaded.                    */
/* -------------------------------------------------------------------- */
            if( iSrcX < nLBlockX * nBlockXSize
                || iSrcX >= (nLBlockX+1) * nBlockXSize
                || iSrcY < nLBlockY * nBlockYSize
                || iSrcY >= (nLBlockY+1) * nBlockYSize )
            {
                nLBlockX = iSrcX / nBlockXSize;
                nLBlockY = iSrcY / nBlockYSize;

                poBlock = GetBlockRef( nLBlockX, nLBlockY );
                if( poBlock == NULL )
                {
                    return( CE_Failure );
                }

                if( eRWFlag == GF_Write )
                    poBlock->MarkDirty();
                
                pabySrcBlock = (GByte *) poBlock->GetDataRef();
                if( pabySrcBlock == NULL )
                    return CE_Failure;
            }

/* -------------------------------------------------------------------- */
/*      Copy over this pixel of data.                                   */
/* -------------------------------------------------------------------- */
            iSrcOffset = (iSrcX - nLBlockX*nBlockXSize
                + (iSrcY - nLBlockY*nBlockYSize) * nBlockXSize)*nBandDataSize;

            if( eDataType == eBufType )
            {
                if( eRWFlag == GF_Read )
                    memcpy( ((GByte *) pData) + iBufOffset,
                            pabySrcBlock + iSrcOffset, nBandDataSize );
                else
                    memcpy( pabySrcBlock + iSrcOffset, 
                            ((GByte *) pData) + iBufOffset, nBandDataSize );
            }
            else
            {
                /* type to type conversion ... ouch, this is expensive way
                   of handling single words */
                
                if( eRWFlag == GF_Read )
                    GDALCopyWords( pabySrcBlock + iSrcOffset, eDataType, 0,
                                   ((GByte *) pData) + iBufOffset, eBufType, 0,
                                   1 );
                else
                    GDALCopyWords( ((GByte *) pData) + iBufOffset, eBufType, 0,
                                   pabySrcBlock + iSrcOffset, eDataType, 0,
                                   1 );
            }

            iBufOffset += nPixelSpace;
        }
    }

    return( CE_None );
}

/************************************************************************/
/*                           GDALSwapWords()                            */
/************************************************************************/

void GDALSwapWords( void *pData, int nWordSize, int nWordCount,
                    int nWordSkip )

{
    int         i;
    GByte       *pabyData = (GByte *) pData;

    switch( nWordSize )
    {
      case 1:
        break;

      case 2:
        CPLAssert( nWordSize >= 2 );
        for( i = 0; i < nWordCount; i++ )
        {
            GByte       byTemp;

            byTemp = pabyData[0];
            pabyData[0] = pabyData[1];
            pabyData[1] = byTemp;

            pabyData += nWordSkip;
        }
        break;
        
      case 4:
        CPLAssert( nWordSize >= 4 );
        for( i = 0; i < nWordCount; i++ )
        {
            GByte       byTemp;

            byTemp = pabyData[0];
            pabyData[0] = pabyData[3];
            pabyData[3] = byTemp;

            byTemp = pabyData[1];
            pabyData[1] = pabyData[2];
            pabyData[2] = byTemp;

            pabyData += nWordSkip;
        }
        break;

      case 8:
        CPLAssert( nWordSize >= 8 );
        for( i = 0; i < nWordCount; i++ )
        {
            GByte       byTemp;

            byTemp = pabyData[0];
            pabyData[0] = pabyData[7];
            pabyData[7] = byTemp;

            byTemp = pabyData[1];
            pabyData[1] = pabyData[6];
            pabyData[6] = byTemp;

            byTemp = pabyData[2];
            pabyData[2] = pabyData[5];
            pabyData[5] = byTemp;

            byTemp = pabyData[3];
            pabyData[3] = pabyData[4];
            pabyData[4] = byTemp;

            pabyData += nWordSkip;
        }
        break;

      default:
        CPLAssert( FALSE );
    }
}

/************************************************************************/
/*                           GDALCopyWords()                            */
/************************************************************************/

void 
GDALCopyWords( void * pSrcData, GDALDataType eSrcType, int nSrcPixelOffset,
               void * pDstData, GDALDataType eDstType, int nDstPixelOffset,
               int nWordCount )

{
/* -------------------------------------------------------------------- */
/*      Special case when no data type translation is required.         */
/* -------------------------------------------------------------------- */
    if( eSrcType == eDstType )
    {
        int     nWordSize = GDALGetDataTypeSize(eSrcType)/8;
        int     i;

        // contiguous blocks.
        if( nWordSize == nSrcPixelOffset && nWordSize == nDstPixelOffset )
        {
            memcpy( pDstData, pSrcData, nSrcPixelOffset * nWordCount );
            return;
        }

        // source or destination is not contiguous
        for( i = 0; i < nWordCount; i++ )
        {
            memcpy( ((GByte *)pDstData) + i * nDstPixelOffset,
                    ((GByte *)pSrcData) + i * nSrcPixelOffset,
                    nWordSize );
        }

        return;
    }

/* ==================================================================== */
/*      General translation case                                        */
/* ==================================================================== */
    for( int iWord = 0; iWord < nWordCount; iWord++ )
    {
        GByte   *pabySrcWord, *pabyDstWord;
        double  dfPixelValue, dfPixelValueI=0.0;

        pabySrcWord = ((GByte *) pSrcData) + iWord * nSrcPixelOffset;

/* -------------------------------------------------------------------- */
/*      Fetch source value based on data type.                          */
/* -------------------------------------------------------------------- */
        switch( eSrcType )
        {
          case GDT_Byte:
            dfPixelValue = *pabySrcWord;
            break;

          case GDT_UInt16:
          {
              GUInt16   nVal;

              memcpy( &nVal, pabySrcWord, 2 );
              dfPixelValue = nVal;
          }
          break;
          
          case GDT_Int16:
          {
              GInt16    nVal;

              memcpy( &nVal, pabySrcWord, 2 );
              dfPixelValue = nVal;
          }
          break;
          
          case GDT_Int32:
          {
              GInt32    nVal;

              memcpy( &nVal, pabySrcWord, 4 );
              dfPixelValue = nVal;
          }
          break;
          
          case GDT_UInt32:
          {
              GUInt32   nVal;

              memcpy( &nVal, pabySrcWord, 4 );
              dfPixelValue = nVal;
          }
          break;
          
          case GDT_Float32:
          {
              float     fVal;

              memcpy( &fVal, pabySrcWord, 4 );
              dfPixelValue = fVal;
          }
          break;
          
          case GDT_Float64:
          {
              memcpy( &dfPixelValue, pabySrcWord, 8 );
          }
          break;

          case GDT_CInt16:
          {
              GInt16    nVal;

              memcpy( &nVal, pabySrcWord, 2 );
              dfPixelValue = nVal;
              memcpy( &nVal, pabySrcWord+2, 2 );
              dfPixelValueI = nVal;
          }
          break;
          
          case GDT_CInt32:
          {
              GInt32    nVal;

              memcpy( &nVal, pabySrcWord, 4 );
              dfPixelValue = nVal;
              memcpy( &nVal, pabySrcWord+4, 4 );
              dfPixelValueI = nVal;
          }
          break;
          
          case GDT_CFloat32:
          {
              float     fVal;

              memcpy( &fVal, pabySrcWord, 4 );
              dfPixelValue = fVal;
              memcpy( &fVal, pabySrcWord+4, 4 );
              dfPixelValueI = fVal;
          }
          break;
          
          case GDT_CFloat64:
          {
              memcpy( &dfPixelValue, pabySrcWord, 8 );
              memcpy( &dfPixelValueI, pabySrcWord+8, 8 );
          }
          break;

          default:
            CPLAssert( FALSE );
        }
        
/* -------------------------------------------------------------------- */
/*      Set the destination pixel, doing range clipping as needed.      */
/* -------------------------------------------------------------------- */
        pabyDstWord = ((GByte *) pDstData) + iWord * nDstPixelOffset;
        switch( eDstType )
        {
          case GDT_Byte:
          {
              dfPixelValue += 0.5;

              if( dfPixelValue < 0.0 )
                  *pabyDstWord = 0;
              else if( dfPixelValue > 255.0 )
                  *pabyDstWord = 255;
              else
                  *pabyDstWord = (GByte) dfPixelValue;
          }
          break;

          case GDT_UInt16:
          {
              GUInt16   nVal;
              
              dfPixelValue += 0.5;

              if( dfPixelValue < 0.0 )
                  nVal = 0;
              else if( dfPixelValue > 65535.0 )
                  nVal = 65535;
              else
                  nVal = (GUInt16) dfPixelValue;

              memcpy( pabyDstWord, &nVal, 2 );
          }
          break;

          case GDT_Int16:
          {
              GInt16    nVal;
              
              dfPixelValue += 0.5;

              if( dfPixelValue < -32768 )
                  nVal = -32768;
              else if( dfPixelValue > 32767 )
                  nVal = 32767;
              else
                  nVal = (GInt16) floor(dfPixelValue);

              memcpy( pabyDstWord, &nVal, 2 );
          }
          break;
          
          case GDT_UInt32:
          {
              GUInt32   nVal;
              
              dfPixelValue += 0.5;

              if( dfPixelValue < 0 )
                  nVal = 0;
              else if( dfPixelValue > 4294967295U )
                  nVal = 4294967295U;
              else
                  nVal = (GInt32) dfPixelValue;

              memcpy( pabyDstWord, &nVal, 4 );
          }
          break;
          
          case GDT_Int32:
          {
              GInt32    nVal;
              
              dfPixelValue += 0.5;

              if( dfPixelValue < -2147483647.0 )
                  nVal = -2147483647;
              else if( dfPixelValue > 2147483647 )
                  nVal = 2147483647;
              else
                  nVal = (GInt32) floor(dfPixelValue);

              memcpy( pabyDstWord, &nVal, 4 );
          }
          break;

          case GDT_Float32:
          {
              float     fVal;

              fVal = dfPixelValue;

              memcpy( pabyDstWord, &fVal, 4 );
          }
          break;

          case GDT_Float64:
              memcpy( pabyDstWord, &dfPixelValue, 8 );
              break;
              
          case GDT_CInt16:
          {
              GInt16    nVal;
              
              dfPixelValue += 0.5;
              dfPixelValueI += 0.5;

              if( dfPixelValue < -32768 )
                  nVal = -32768;
              else if( dfPixelValue > 32767 )
                  nVal = 32767;
              else
                  nVal = (GInt16) floor(dfPixelValue);
              memcpy( pabyDstWord, &nVal, 2 );

              if( dfPixelValueI < -32768 )
                  nVal = -32768;
              else if( dfPixelValueI > 32767 )
                  nVal = 32767;
              else
                  nVal = (GInt16) floor(dfPixelValueI);
              memcpy( pabyDstWord+2, &nVal, 2 );
          }
          break;
          
          case GDT_CInt32:
          {
              GInt32    nVal;
              
              dfPixelValue += 0.5;
              dfPixelValueI += 0.5;

              if( dfPixelValue < -2147483647.0 )
                  nVal = -2147483647;
              else if( dfPixelValue > 2147483647 )
                  nVal = 2147483647;
              else
                  nVal = (GInt32) floor(dfPixelValue);

              memcpy( pabyDstWord, &nVal, 4 );

              if( dfPixelValueI < -2147483647.0 )
                  nVal = -2147483647;
              else if( dfPixelValueI > 2147483647 )
                  nVal = 2147483647;
              else
                  nVal = (GInt32) floor(dfPixelValueI);

              memcpy( pabyDstWord+4, &nVal, 4 );
          }
          break;

          case GDT_CFloat32:
          {
              float     fVal;

              fVal = dfPixelValue;
              memcpy( pabyDstWord, &fVal, 4 );
              fVal = dfPixelValueI;
              memcpy( pabyDstWord+4, &fVal, 4 );
          }
          break;

          case GDT_CFloat64:
              memcpy( pabyDstWord, &dfPixelValue, 8 );
              memcpy( pabyDstWord+8, &dfPixelValueI, 8 );
              break;
              
          default:
            CPLAssert( FALSE );
        }
    } /* next iWord */
}

/************************************************************************/
/*                          OverviewRasterIO()                          */
/*                                                                      */
/*      Special work function to utilize available overviews to         */
/*      more efficiently satisfy downsampled requests.  It will         */
/*      return CE_Failure if there are no appropriate overviews         */
/*      available but it doesn't emit any error messages.               */
/************************************************************************/

CPLErr GDALRasterBand::OverviewRasterIO( GDALRWFlag eRWFlag,
                                int nXOff, int nYOff, int nXSize, int nYSize,
                                void * pData, int nBufXSize, int nBufYSize,
                                GDALDataType eBufType,
                                int nPixelSpace, int nLineSpace )


{
    GDALRasterBand      *poBestOverview = NULL;
    int                 nOverviewCount = GetOverviewCount();
    double              dfDesiredResolution, dfBestResolution = 1.0;

/* -------------------------------------------------------------------- */
/*      Find the Compute the desired resolution.  The resolution is     */
/*      based on the least reduced axis, and represents the number      */
/*      of source pixels to one destination pixel.                      */
/* -------------------------------------------------------------------- */
    if( (nXSize / (double) nBufXSize) < (nYSize / (double) nBufYSize ) 
        || nBufYSize == 1 )
        dfDesiredResolution = nXSize / (double) nBufXSize;
    else
        dfDesiredResolution = nYSize / (double) nBufYSize;

/* -------------------------------------------------------------------- */
/*      Find the overview level that largest resolution value (most     */
/*      downsampled) that is still less than (or only a little more)    */
/*      downsampled than the request.                                   */
/* -------------------------------------------------------------------- */
    for( int iOverview = 0; iOverview < nOverviewCount; iOverview++ )
    {
        GDALRasterBand  *poOverview = GetOverview( iOverview );
        double          dfResolution;

        if( (GetXSize() / (double) poOverview->GetXSize())
            < (GetYSize() / (double) poOverview->GetYSize()) )
            dfResolution = 
                GetXSize() / (double) poOverview->GetXSize();
        else
            dfResolution = 
                GetYSize() / (double) poOverview->GetYSize();

        if( dfResolution < dfDesiredResolution * 1.2 
            && dfResolution > dfBestResolution )
        {
            poBestOverview = poOverview;
            dfBestResolution = dfResolution;
        }
    }

/* -------------------------------------------------------------------- */
/*      If we didn't find an overview that helps us, just return        */
/*      indicating failure and the full resolution image will be used.  */
/* -------------------------------------------------------------------- */
    if( poBestOverview == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Recompute the source window in terms of the selected            */
/*      overview.                                                       */
/* -------------------------------------------------------------------- */
    int         nOXOff, nOYOff, nOXSize, nOYSize;
    double      dfXRes, dfYRes;
    
    dfXRes = GetXSize() / (double) poBestOverview->GetXSize();
    dfYRes = GetYSize() / (double) poBestOverview->GetYSize();

    nOXOff = MIN(poBestOverview->GetXSize()-1,(int) (nXOff/dfXRes+0.5));
    nOYOff = MIN(poBestOverview->GetYSize()-1,(int) (nYOff/dfYRes+0.5));
    nOXSize = MAX(1,(int) (nXSize/dfXRes + 0.5));
    nOYSize = MAX(1,(int) (nYSize/dfYRes + 0.5));
    if( nOXOff + nOXSize > poBestOverview->GetXSize() )
        nOXSize = poBestOverview->GetXSize() - nOXOff;
    if( nOYOff + nOYSize > poBestOverview->GetYSize() )
        nOYSize = poBestOverview->GetYSize() - nOYOff;

/* -------------------------------------------------------------------- */
/*      Recast the call in terms of the new raster layer.               */
/* -------------------------------------------------------------------- */
    return poBestOverview->RasterIO( eRWFlag, nOXOff, nOYOff, nOXSize, nOYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nPixelSpace, nLineSpace );
}

