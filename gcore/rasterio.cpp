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
    int		nBandDataSize = GDALGetDataTypeSize( eDataType ) / 8;
    GByte	*pabySrcBlock = NULL;
    GDALRasterBlock *poBlock;
    int		nLBlockX=-1, nLBlockY=-1, iBufYOff, iBufXOff, iSrcY;

/* ==================================================================== */
/*      A common case is the data requested with it's inherent data     */
/*      type, the destination is packed, and the block width is the     */
/*      raster width.                                                   */
/* ==================================================================== */
    if( eBufType == eDataType
        && nPixelSpace == GDALGetDataTypeSize(eBufType)/8
        && nLineSpace == nPixelSpace * nXSize
        && nBlockXSize == poDS->GetRasterXSize()
        && nBufXSize == nXSize )
    {
        for( iBufYOff = 0; iBufYOff < nBufYSize; iBufYOff++ )
        {
            int		nSrcByteOffset;
            
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
/*      Loop reading required source blocks to satisfy output           */
/*      request.  This is the most general implementation.              */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Compute stepping increment.                                     */
/* -------------------------------------------------------------------- */
    double	dfSrcX, dfSrcY, dfSrcXInc, dfSrcYInc;
    int		iSrcX;
    
    dfSrcXInc = nXSize / (double) nBufXSize;
    dfSrcYInc = nYSize / (double) nBufYSize;

/* -------------------------------------------------------------------- */
/*      Loop over buffer computing source locations.                    */
/* -------------------------------------------------------------------- */
    for( iBufYOff = 0; iBufYOff < nBufYSize; iBufYOff++ )
    {
        int	iBufOffset, iSrcOffset;
        
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
    int		i;
    GByte	*pabyData = (GByte *) pData;

    switch( nWordSize )
    {
      case 1:
        break;

      case 2:
        CPLAssert( nWordSize >= 2 );
        for( i = 0; i < nWordCount; i++ )
        {
            GByte	byTemp;

            byTemp = pabyData[0];
            pabyData[0] = pabyData[1];
            pabyData[1] = byTemp;

            pabyData += nWordSize;
        }
        break;
        
      case 4:
        CPLAssert( nWordSize >= 4 );
        for( i = 0; i < nWordCount; i++ )
        {
            GByte	byTemp;

            byTemp = pabyData[0];
            pabyData[0] = pabyData[3];
            pabyData[3] = byTemp;

            byTemp = pabyData[1];
            pabyData[1] = pabyData[2];
            pabyData[2] = byTemp;

            pabyData += nWordSize;
        }
        break;

      case 8:
        CPLAssert( nWordSize >= 8 );
        for( i = 0; i < nWordCount; i++ )
        {
            GByte	byTemp;

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

            pabyData += nWordSize;
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
        int	nWordSize = GDALGetDataTypeSize(eSrcType)/8;
        int	i;

        // contiguous blocks.
        if( nWordSize == nSrcPixelOffset && nWordSize == nDstPixelOffset )
        {
            memcpy( pDstData, pSrcData, nSrcPixelOffset * nWordCount );
            return;
        }

        // source or destination is not contiguous
        for( i = 0; i < nWordCount; i++ )
        {
            memcpy( ((GByte *)pSrcData) + i * nSrcPixelOffset,
                    ((GByte *)pDstData) + i * nDstPixelOffset,
                    nWordSize );
        }

        return;
    }

/* ==================================================================== */
/*      General translation case                                        */
/* ==================================================================== */
    for( int iWord = 0; iWord < nWordCount; iWord++ )
    {
        GByte 	*pabySrcWord, *pabyDstWord;
        double	dfPixelValue;

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
              GUInt16	nVal;

              memcpy( &nVal, pabySrcWord, 2 );
              dfPixelValue = nVal;
          }
          break;
          
          case GDT_Int16:
          {
              GInt16	nVal;

              memcpy( &nVal, pabySrcWord, 2 );
              dfPixelValue = nVal;
          }
          break;
          
          case GDT_Int32:
          {
              GInt32	nVal;

              memcpy( &nVal, pabySrcWord, 4 );
              dfPixelValue = nVal;
          }
          break;
          
          case GDT_UInt32:
          {
              GUInt32	nVal;

              memcpy( &nVal, pabySrcWord, 4 );
              dfPixelValue = nVal;
          }
          break;
          
          case GDT_Float32:
          {
              float	fVal;

              memcpy( &fVal, pabySrcWord, 4 );
              dfPixelValue = fVal;
          }
          break;
          
          case GDT_Float64:
          {
              memcpy( &dfPixelValue, pabySrcWord, 4 );
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
              GUInt16	nVal;
              
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
              GInt16	nVal;
              
              if( dfPixelValue < -32768 )
                  nVal = -32768;
              else if( dfPixelValue > 32767 )
                  nVal = 32767;
              else
                  nVal = (GInt16) dfPixelValue;

              memcpy( pabyDstWord, &nVal, 2 );
          }
          break;
          
          case GDT_UInt32:
          {
              GUInt32	nVal;
              
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
              GInt32	nVal;
              
              if( dfPixelValue < -2147483647.0 )
                  nVal = -2147483647;
              else if( dfPixelValue > 2147483647 )
                  nVal = 2147483647;
              else
                  nVal = (GInt32) dfPixelValue;

              memcpy( pabyDstWord, &nVal, 4 );
          }
          break;

          case GDT_Float32:
          {
              float 	fVal;

              fVal = dfPixelValue;

              memcpy( pabyDstWord, &fVal, 4 );
          }
          break;

          case GDT_Float64:
              memcpy( pabyDstWord, &dfPixelValue, 8 );
              break;
              
          default:
            CPLAssert( FALSE );
        }
    } /* next iWord */
}
