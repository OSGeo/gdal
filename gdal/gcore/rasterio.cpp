/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Contains default implementation of GDALRasterBand::IRasterIO()
 *           and supporting functions of broader utility.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "gdal_vrt.h"
#include "vrtdataset.h"
#include "memdataset.h"
#include "gdalwarper.h"

// Define a list of "C++" compilers that have broken template support or
// broken scoping so we can fall back on the legacy implementation of
// GDALCopyWords
#define NOT_BROKEN_COMPILER \
	(!(defined(_MSC_VER) && _MSC_VER <= 1200) && !defined(__BORLANDC__) && \
	!defined(__SUNPRO_CC))

#if NOT_BROKEN_COMPILER
#include <stdexcept>
#include <limits>

// For now, work around MSVC++ 6.0's broken template support. If this value
// is not defined, the old GDALCopyWords implementation is used.
#define USE_NEW_COPYWORDS 1
#endif


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
                                  GSpacing nPixelSpace, GSpacing nLineSpace,
                                  GDALRasterIOExtraArg* psExtraArg )

{
    int         nBandDataSize = GDALGetDataTypeSize( eDataType ) / 8;
    int         nBufDataSize = GDALGetDataTypeSize( eBufType ) / 8;
    GByte       *pabySrcBlock = NULL;
    GDALRasterBlock *poBlock = NULL;
    int         nLBlockX=-1, nLBlockY=-1, iBufYOff, iBufXOff, iSrcY;
    
    if( eRWFlag == GF_Write && eFlushBlockErr != CE_None )
    {
        CPLError(eFlushBlockErr, CPLE_AppDefined,
                 "An error occured while writing a dirty block");
        CPLErr eErr = eFlushBlockErr;
        eFlushBlockErr = CE_None;
        return eErr;
    }

/* ==================================================================== */
/*      A common case is the data requested with the destination        */
/*      is packed, and the block width is the raster width.             */
/* ==================================================================== */
    if( nPixelSpace == nBufDataSize
        && nLineSpace == nPixelSpace * nXSize
        && nBlockXSize == GetXSize()
        && nBufXSize == nXSize 
        && nBufYSize == nYSize )
    {
//        printf( "IRasterIO(%d,%d,%d,%d) rw=%d case 1\n", 
//                nXOff, nYOff, nXSize, nYSize, 
//                (int) eRWFlag );
        CPLErr eErr = CE_None;

        for( iBufYOff = 0; iBufYOff < nBufYSize; iBufYOff++ )
        {
            int         nSrcByteOffset;
            
            iSrcY = iBufYOff + nYOff;
            
            if( iSrcY < nLBlockY * nBlockYSize
                || iSrcY >= (nLBlockY+1) * nBlockYSize )
            {
                nLBlockY = iSrcY / nBlockYSize;
                int bJustInitialize = 
                    eRWFlag == GF_Write
                    && nXOff == 0 && nXSize == nBlockXSize
                    && nYOff <= nLBlockY * nBlockYSize
                    && nYOff + nYSize >= (nLBlockY+1) * nBlockYSize;

                /* Is this a partial tile at right and/or bottom edges of */
                /* the raster, and that is going to be completely written ? */
                /* If so, don't load it from storage, but zeroized it so that */
                /* the content outsize of the validity area is initialized */
                int bMemZeroBuffer = FALSE;
                if( eRWFlag == GF_Write && !bJustInitialize &&
                    nXOff == 0 && nXSize == nBlockXSize &&
                    nYOff <= nLBlockY * nBlockYSize &&
                    nYOff + nYSize == GetYSize() &&
                    (nLBlockY+1) * nBlockYSize > GetYSize() )
                {
                    bJustInitialize = TRUE;
                    bMemZeroBuffer = TRUE;
                }

                if( poBlock )
                    poBlock->DropLock();

                poBlock = GetLockedBlockRef( 0, nLBlockY, bJustInitialize );
                if( poBlock == NULL )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
            "GetBlockRef failed at X block offset %d, "
                        "Y block offset %d", 0, nLBlockY );
                    eErr = CE_Failure;
                    break;
                }

                if( eRWFlag == GF_Write )
                    poBlock->MarkDirty();
                
                pabySrcBlock = (GByte *) poBlock->GetDataRef();
                if( pabySrcBlock == NULL )
                {
                    poBlock->DropLock();
                    eErr = CE_Failure;
                    break;
                }
                if( bMemZeroBuffer )
                {
                    memset(pabySrcBlock, 0,
                           nBandDataSize * nBlockXSize * nBlockYSize);
                }
            }

            nSrcByteOffset = ((iSrcY-nLBlockY*nBlockYSize)*nBlockXSize + nXOff)
                * nBandDataSize;
            
            if( eDataType == eBufType )
            {
                if( eRWFlag == GF_Read )
                    memcpy( ((GByte *) pData) + (GPtrDiff_t)iBufYOff * nLineSpace,
                            pabySrcBlock + nSrcByteOffset, 
                            (size_t)nLineSpace );
                else
                    memcpy( pabySrcBlock + nSrcByteOffset, 
                            ((GByte *) pData) + (GPtrDiff_t)iBufYOff * nLineSpace,
                            (size_t)nLineSpace );
            }
            else
            {
                /* type to type conversion */
                
                if( eRWFlag == GF_Read )
                    GDALCopyWords( pabySrcBlock + nSrcByteOffset,
                                   eDataType, nBandDataSize,
                                   ((GByte *) pData) + (GPtrDiff_t)iBufYOff * nLineSpace,
                                   eBufType, (int)nPixelSpace, nBufXSize );
                else
                    GDALCopyWords( ((GByte *) pData) + (GPtrDiff_t)iBufYOff * nLineSpace,
                                   eBufType, (int)nPixelSpace,
                                   pabySrcBlock + nSrcByteOffset,
                                   eDataType, nBandDataSize, nBufXSize );
            }

            if( psExtraArg->pfnProgress != NULL &&
                !psExtraArg->pfnProgress(1.0 * (iBufYOff + 1) / nBufYSize, "",
                                         psExtraArg->pProgressData) )
            {
                eErr = CE_Failure;
                break;
            }
        }

        if( poBlock )
            poBlock->DropLock();

        return eErr;
    }
    
/* ==================================================================== */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* ==================================================================== */
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && GetOverviewCount() > 0 && eRWFlag == GF_Read )
    {
        int         nOverview;
        GDALRasterIOExtraArg sExtraArg;
    
        GDALCopyRasterIOExtraArg(&sExtraArg, psExtraArg);

        nOverview =
            GDALBandGetBestOverviewLevel2(this, nXOff, nYOff, nXSize, nYSize,
                                          nBufXSize, nBufYSize, &sExtraArg);
        if (nOverview >= 0)
        {
            GDALRasterBand* poOverviewBand = GetOverview(nOverview);
            if (poOverviewBand == NULL)
                return CE_Failure;

            return poOverviewBand->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                            pData, nBufXSize, nBufYSize, eBufType,
                                            nPixelSpace, nLineSpace, &sExtraArg );
        }
    }
    
    
    if( eRWFlag == GF_Read &&
        nBufXSize < nXSize / 100 && nBufYSize < nYSize / 100 &&
        nPixelSpace == nBufDataSize &&
        nLineSpace == nPixelSpace * nBufXSize &&
        CSLTestBoolean(CPLGetConfigOption("GDAL_NO_COSTLY_OVERVIEW", "NO")) )
    {
        memset(pData, 0, (size_t)(nLineSpace * nBufYSize));
        return CE_None;
    }


/* ==================================================================== */
/*      The second case when we don't need subsample data but likely    */
/*      need data type conversion.                                      */
/* ==================================================================== */
    int         iSrcX;

    if ( /* nPixelSpace == nBufDataSize
            && */ nXSize == nBufXSize
         && nYSize == nBufYSize )    
    {
//        printf( "IRasterIO(%d,%d,%d,%d) rw=%d case 2\n", 
//                nXOff, nYOff, nXSize, nYSize, 
//                (int) eRWFlag );

/* -------------------------------------------------------------------- */
/*      Loop over buffer computing source locations.                    */
/* -------------------------------------------------------------------- */
        int     nLBlockXStart, nXSpanEnd;

        // Calculate starting values out of loop
        nLBlockXStart = nXOff / nBlockXSize;
        nXSpanEnd = nBufXSize + nXOff;

        int nYInc = 0;
        for( iBufYOff = 0, iSrcY = nYOff; iBufYOff < nBufYSize; iBufYOff+=nYInc, iSrcY+=nYInc )
        {
            GPtrDiff_t  iBufOffset;
            GPtrDiff_t  iSrcOffset;
            int     nXSpan;

            iBufOffset = (GPtrDiff_t)iBufYOff * (GPtrDiff_t)nLineSpace;
            nLBlockY = iSrcY / nBlockYSize;
            nLBlockX = nLBlockXStart;
            iSrcX = nXOff;
            while( iSrcX < nXSpanEnd )
            {
                int nXSpanSize;

                nXSpan = (nLBlockX + 1) * nBlockXSize;
                nXSpan = ( ( nXSpan < nXSpanEnd )?nXSpan:nXSpanEnd ) - iSrcX;
                nXSpanSize = nXSpan * (int)nPixelSpace;

                int bJustInitialize = 
                    eRWFlag == GF_Write
                    && nYOff <= nLBlockY * nBlockYSize
                    && nYOff + nYSize >= (nLBlockY+1) * nBlockYSize
                    && nXOff <= nLBlockX * nBlockXSize
                    && nXOff + nXSize >= (nLBlockX+1) * nBlockXSize;

                /* Is this a partial tile at right and/or bottom edges of */
                /* the raster, and that is going to be completely written ? */
                /* If so, don't load it from storage, but zeroized it so that */
                /* the content outsize of the validity area is initialized */
                int bMemZeroBuffer = FALSE;
                if( eRWFlag == GF_Write && !bJustInitialize &&
                    nXOff <= nLBlockX * nBlockXSize &&
                    nYOff <= nLBlockY * nBlockYSize &&
                    (nXOff + nXSize >= (nLBlockX+1) * nBlockXSize ||
                     (nXOff + nXSize == GetXSize() && (nLBlockX+1) * nBlockXSize > GetXSize())) &&
                    (nYOff + nYSize >= (nLBlockY+1) * nBlockYSize ||
                     (nYOff + nYSize == GetYSize() && (nLBlockY+1) * nBlockYSize > GetYSize())) )
                {
                    bJustInitialize = TRUE;
                    bMemZeroBuffer = TRUE;
                }
//                printf( "bJustInitialize = %d (%d,%d,%d,%d)\n", 
//                        bJustInitialize,
//                        nYOff, nYSize, 
//                        nLBlockY, nBlockYSize );
//                bJustInitialize = FALSE;
                

/* -------------------------------------------------------------------- */
/*      Ensure we have the appropriate block loaded.                    */
/* -------------------------------------------------------------------- */
                poBlock = GetLockedBlockRef( nLBlockX, nLBlockY, bJustInitialize );
                if( !poBlock )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
            "GetBlockRef failed at X block offset %d, "
                        "Y block offset %d", nLBlockX, nLBlockY );
                    return( CE_Failure );
                }

                if( eRWFlag == GF_Write )
                    poBlock->MarkDirty();
                
                pabySrcBlock = (GByte *) poBlock->GetDataRef();
                if( pabySrcBlock == NULL )
                {
                    poBlock->DropLock();
                    return CE_Failure;
                }
                if( bMemZeroBuffer )
                {
                    memset(pabySrcBlock, 0,
                           nBandDataSize * nBlockXSize * nBlockYSize);
                }
/* -------------------------------------------------------------------- */
/*      Copy over this chunk of data.                                   */
/* -------------------------------------------------------------------- */
                iSrcOffset = ((GPtrDiff_t)iSrcX - (GPtrDiff_t)nLBlockX*nBlockXSize
                    + ((GPtrDiff_t)(iSrcY) - (GPtrDiff_t)nLBlockY*nBlockYSize) * nBlockXSize)*nBandDataSize;
                /* Fill up as many rows as possible for the loaded block */
                int kmax = MIN(nBlockYSize - (iSrcY % nBlockYSize), nBufYSize - iBufYOff);
                for(int k=0; k<kmax;k++)
                {
                    if( eDataType == eBufType 
                        && nPixelSpace == nBufDataSize )
                    {
                        if( eRWFlag == GF_Read )
                            memcpy( ((GByte *) pData) + iBufOffset + (GPtrDiff_t)k * nLineSpace,
                                    pabySrcBlock + iSrcOffset, nXSpanSize );
                        else
                            memcpy( pabySrcBlock + iSrcOffset, 
                                    ((GByte *) pData) + iBufOffset + (GPtrDiff_t)k * nLineSpace, nXSpanSize );
                    }
                    else
                    {
                        /* type to type conversion */
                        
                        if( eRWFlag == GF_Read )
                            GDALCopyWords( pabySrcBlock + iSrcOffset,
                                        eDataType, nBandDataSize,
                                        ((GByte *) pData) + iBufOffset + (GPtrDiff_t)k * nLineSpace,
                                        eBufType, (int)nPixelSpace, nXSpan );
                        else
                            GDALCopyWords( ((GByte *) pData) + iBufOffset + (GPtrDiff_t)k * nLineSpace,
                                        eBufType, (int)nPixelSpace,
                                        pabySrcBlock + iSrcOffset,
                                        eDataType, nBandDataSize, nXSpan );
                    }
                    
                    iSrcOffset += nBlockXSize * nBandDataSize;
                }

                iBufOffset += nXSpanSize;
                nLBlockX++;
                iSrcX+=nXSpan;

                poBlock->DropLock();
                poBlock = NULL;
            }

            /* Compute the increment to go on a block boundary */
            nYInc = nBlockYSize - (iSrcY % nBlockYSize);

            if( psExtraArg->pfnProgress != NULL &&
                !psExtraArg->pfnProgress(1.0 * MIN(nBufYSize, iBufYOff + nYInc) / nBufYSize, "",
                                         psExtraArg->pProgressData) )
            {
                return CE_Failure;
            }
        }

        return CE_None;
    }

/* ==================================================================== */
/*      Loop reading required source blocks to satisfy output           */
/*      request.  This is the most general implementation.              */
/* ==================================================================== */

    double dfXOff, dfYOff, dfXSize, dfYSize;
    if( psExtraArg->bFloatingPointWindowValidity )
    {
        dfXOff = psExtraArg->dfXOff;
        dfYOff = psExtraArg->dfYOff;
        dfXSize = psExtraArg->dfXSize;
        dfYSize = psExtraArg->dfYSize;
        CPLAssert(dfXOff - nXOff < 1.0);
        CPLAssert(dfYOff - nYOff < 1.0);
        CPLAssert(nXSize - dfXSize < 1.0);
        CPLAssert(nYSize - dfYSize < 1.0);
    }
    else
    {
        dfXOff = nXOff;
        dfYOff = nYOff;
        dfXSize = nXSize;
        dfYSize = nYSize;
    }
/* -------------------------------------------------------------------- */
/*      Compute stepping increment.                                     */
/* -------------------------------------------------------------------- */
    double dfSrcXInc, dfSrcYInc;
    dfSrcXInc = dfXSize / (double) nBufXSize;
    dfSrcYInc = dfYSize / (double) nBufYSize;
    CPLErr eErr = CE_None;

//    printf( "IRasterIO(%d,%d,%d,%d) rw=%d case 3\n", 
//            nXOff, nYOff, nXSize, nYSize, 
//            (int) eRWFlag );
    if (eRWFlag == GF_Write)
    {
/* -------------------------------------------------------------------- */
/*    Write case                                                        */
/*    Loop over raster window computing source locations in the buffer. */
/* -------------------------------------------------------------------- */
        int iDstX, iDstY;
        GByte* pabyDstBlock = NULL;

        for( iDstY = nYOff; iDstY < nYOff + nYSize; iDstY ++)
        {
            GPtrDiff_t iBufOffset, iDstOffset;
            iBufYOff = (int)((iDstY - nYOff) / dfSrcYInc);

            for( iDstX = nXOff; iDstX < nXOff + nXSize; iDstX ++)
            {
                iBufXOff = (int)((iDstX - nXOff) / dfSrcXInc);
                iBufOffset = (GPtrDiff_t)iBufYOff * (GPtrDiff_t)nLineSpace + iBufXOff * (GPtrDiff_t)nPixelSpace;

                // FIXME: this code likely doesn't work if the dirty block gets flushed
                // to disk before being completely written.
                // In the meantime, bJustInitalize should probably be set to FALSE
                // even if it is not ideal performance wise, and for lossy compression

    /* -------------------------------------------------------------------- */
    /*      Ensure we have the appropriate block loaded.                    */
    /* -------------------------------------------------------------------- */
                if( iDstX < nLBlockX * nBlockXSize
                    || iDstX >= (nLBlockX+1) * nBlockXSize
                    || iDstY < nLBlockY * nBlockYSize
                    || iDstY >= (nLBlockY+1) * nBlockYSize )
                {
                    nLBlockX = iDstX / nBlockXSize;
                    nLBlockY = iDstY / nBlockYSize;

                    int bJustInitialize = 
                           nYOff <= nLBlockY * nBlockYSize
                        && nYOff + nYSize >= (nLBlockY+1) * nBlockYSize
                        && nXOff <= nLBlockX * nBlockXSize
                        && nXOff + nXSize >= (nLBlockX+1) * nBlockXSize;
                    /*int bMemZeroBuffer = FALSE;
                    if( !bJustInitialize &&
                        nXOff <= nLBlockX * nBlockXSize &&
                        nYOff <= nLBlockY * nBlockYSize &&
                        (nXOff + nXSize >= (nLBlockX+1) * nBlockXSize ||
                         (nXOff + nXSize == GetXSize() && (nLBlockX+1) * nBlockXSize > GetXSize())) &&
                        (nYOff + nYSize >= (nLBlockY+1) * nBlockYSize ||
                         (nYOff + nYSize == GetYSize() && (nLBlockY+1) * nBlockYSize > GetYSize())) )
                    {
                        bJustInitialize = TRUE;
                        bMemZeroBuffer = TRUE;
                    }*/
                    if( poBlock != NULL )
                        poBlock->DropLock();

                    poBlock = GetLockedBlockRef( nLBlockX, nLBlockY, 
                                                bJustInitialize );
                    if( poBlock == NULL )
                    {
                        return( CE_Failure );
                    }

                    poBlock->MarkDirty();

                    pabyDstBlock = (GByte *) poBlock->GetDataRef();
                    if( pabyDstBlock == NULL )
                    {
                        poBlock->DropLock();
                        return CE_Failure;
                    }
                    /*if( bMemZeroBuffer )
                    {
                        memset(pabyDstBlock, 0,
                            nBandDataSize * nBlockXSize * nBlockYSize);
                    }*/
                }

    /* -------------------------------------------------------------------- */
    /*      Copy over this pixel of data.                                   */
    /* -------------------------------------------------------------------- */
                iDstOffset = ((GPtrDiff_t)iDstX - (GPtrDiff_t)nLBlockX*nBlockXSize
                    + ((GPtrDiff_t)iDstY - (GPtrDiff_t)nLBlockY*nBlockYSize) * nBlockXSize)*nBandDataSize;

                if( eDataType == eBufType )
                {
                    memcpy( pabyDstBlock + iDstOffset, 
                            ((GByte *) pData) + iBufOffset, nBandDataSize );
                }
                else
                {
                    /* type to type conversion ... ouch, this is expensive way
                    of handling single words */

                    GDALCopyWords( ((GByte *) pData) + iBufOffset, eBufType, 0,
                                pabyDstBlock + iDstOffset, eDataType, 0,
                                1 );
                }
            }

            if( psExtraArg->pfnProgress != NULL &&
                !psExtraArg->pfnProgress(1.0 * (iDstY - nYOff + 1) / nYSize, "",
                                         psExtraArg->pProgressData) )
            {
                eErr = CE_Failure;
                break;
            }
        }
    }
    else
    {
        if( psExtraArg->eResampleAlg != GRIORA_NearestNeighbour )
        {
            if( (psExtraArg->eResampleAlg == GRIORA_Cubic ||
                 psExtraArg->eResampleAlg == GRIORA_CubicSpline ||
                 psExtraArg->eResampleAlg == GRIORA_Bilinear ||
                 psExtraArg->eResampleAlg == GRIORA_Lanczos) &&
                GetColorTable() != NULL )
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Resampling method not supported on paletted band. "
                         "Falling back to nearest neighbour");
            }
            else if( psExtraArg->eResampleAlg == GRIORA_Gauss &&
                     GDALDataTypeIsComplex( eDataType ) )
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Resampling method not supported on complex data type band. "
                         "Falling back to nearest neighbour");
            }
            else
            {
                return RasterIOResampled( eRWFlag,
                                          nXOff, nYOff, nXSize, nYSize,
                                          pData, nBufXSize, nBufYSize,
                                          eBufType,
                                          nPixelSpace, nLineSpace,
                                          psExtraArg );
            }
        }

        double      dfSrcX, dfSrcY;
        int         nLimitBlockY = 0;
        int         bByteCopy = ( eDataType == eBufType && nBandDataSize == 1); 
        int nStartBlockX = -nBlockXSize;

/* -------------------------------------------------------------------- */
/*      Read case                                                       */
/*      Loop over buffer computing source locations.                    */
/* -------------------------------------------------------------------- */
        for( iBufYOff = 0; iBufYOff < nBufYSize; iBufYOff++ )
        {
            GPtrDiff_t iBufOffset, iSrcOffset;

            dfSrcY = (iBufYOff+0.5) * dfSrcYInc + dfYOff;
            dfSrcX = 0.5 * dfSrcXInc + dfXOff;
            iSrcY = (int) dfSrcY;

            iBufOffset = (GPtrDiff_t)iBufYOff * (GPtrDiff_t)nLineSpace;

            if( iSrcY >= nLimitBlockY )
            {
                nLBlockY = iSrcY / nBlockYSize;
                nLimitBlockY = (nLBlockY + 1) * nBlockYSize;
                nStartBlockX = -nBlockXSize; /* make sure a new block is loaded */
            }
            else if( (int)dfSrcX < nStartBlockX )
                nStartBlockX = -nBlockXSize; /* make sure a new block is loaded */

            GPtrDiff_t iSrcOffsetCst = (iSrcY - nLBlockY*nBlockYSize) * (GPtrDiff_t)nBlockXSize;

            for( iBufXOff = 0; iBufXOff < nBufXSize; iBufXOff++, dfSrcX += dfSrcXInc )
            {
                iSrcX = (int) dfSrcX;
                int nDiffX = iSrcX - nStartBlockX;

    /* -------------------------------------------------------------------- */
    /*      Ensure we have the appropriate block loaded.                    */
    /* -------------------------------------------------------------------- */
                if( nDiffX >= nBlockXSize )
                {
                    nLBlockX = iSrcX / nBlockXSize;
                    nStartBlockX = nLBlockX * nBlockXSize;
                    nDiffX = iSrcX - nStartBlockX;

                    if( poBlock != NULL )
                        poBlock->DropLock();

                    poBlock = GetLockedBlockRef( nLBlockX, nLBlockY, 
                                                 FALSE );
                    if( poBlock == NULL )
                    {
                        eErr = CE_Failure;
                        break;
                    }

                    pabySrcBlock = (GByte *) poBlock->GetDataRef();
                    if( pabySrcBlock == NULL )
                    {
                        poBlock->DropLock();
                        eErr = CE_Failure;
                        break;
                    }
                }

    /* -------------------------------------------------------------------- */
    /*      Copy over this pixel of data.                                   */
    /* -------------------------------------------------------------------- */
                iSrcOffset = ((GPtrDiff_t)nDiffX + iSrcOffsetCst)*nBandDataSize;

                if( bByteCopy )
                {
                    ((GByte *) pData)[iBufOffset] = pabySrcBlock[iSrcOffset];
                }
                else if( eDataType == eBufType )
                {
                    memcpy( ((GByte *) pData) + iBufOffset,
                            pabySrcBlock + iSrcOffset, nBandDataSize );
                }
                else
                {
                    /* type to type conversion ... ouch, this is expensive way
                    of handling single words */
                    GDALCopyWords( pabySrcBlock + iSrcOffset, eDataType, 0,
                                ((GByte *) pData) + iBufOffset, eBufType, 0,
                                1 );
                }

                iBufOffset += (int)nPixelSpace;
            }

            if( psExtraArg->pfnProgress != NULL &&
                !psExtraArg->pfnProgress(1.0 * (iBufYOff + 1) / nBufYSize, "",
                                         psExtraArg->pProgressData) )
            {
                eErr = CE_Failure;
                break;
            }
        }
    }

    if( poBlock != NULL )
        poBlock->DropLock();

    return( eErr );
}

/************************************************************************/
/*                         GDALRasterIOTransformer()                    */
/************************************************************************/

typedef struct
{
    double dfXOff, dfYOff;
    double dfXRatioDstToSrc, dfYRatioDstToSrc;
} GDALRasterIOTransformerStruct;

static int GDALRasterIOTransformer( void *pTransformerArg,
                                    CPL_UNUSED int bDstToSrc, int nPointCount,
                                    double *x, double *y, CPL_UNUSED double *z,
                                    int *panSuccess )
{
    CPLAssert(bDstToSrc);
    GDALRasterIOTransformerStruct* psParams = (GDALRasterIOTransformerStruct*) pTransformerArg;
    for(int i = 0; i < nPointCount; i++)
    {
        x[i] = x[i] * psParams->dfXRatioDstToSrc + psParams->dfXOff;
        y[i] = y[i] * psParams->dfYRatioDstToSrc + psParams->dfYOff;
        panSuccess[i] = TRUE;
    }
    return TRUE;
}

/************************************************************************/
/*                          RasterIOResampled()                         */
/************************************************************************/

CPLErr GDALRasterBand::RasterIOResampled( CPL_UNUSED GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpace, GSpacing nLineSpace,
                                  GDALRasterIOExtraArg* psExtraArg )
{
    CPLErr eErr = CE_None;

    // Determine if we use warping resampling or overview resampling
    int bUseWarp;
    if( !GDALDataTypeIsComplex( eDataType ) )
        bUseWarp = FALSE;
    else
        bUseWarp = TRUE;

    double dfXOff, dfYOff, dfXSize, dfYSize;
    if( psExtraArg->bFloatingPointWindowValidity )
    {
        dfXOff = psExtraArg->dfXOff;
        dfYOff = psExtraArg->dfYOff;
        dfXSize = psExtraArg->dfXSize;
        dfYSize = psExtraArg->dfYSize;
        CPLAssert(dfXOff - nXOff < 1.0);
        CPLAssert(dfYOff - nYOff < 1.0);
        CPLAssert(nXSize - dfXSize < 1.0);
        CPLAssert(nYSize - dfYSize < 1.0);
    }
    else
    {
        dfXOff = nXOff;
        dfYOff = nYOff;
        dfXSize = nXSize;
        dfYSize = nYSize;
    }

    double dfXRatioDstToSrc = dfXSize / nBufXSize;
    double dfYRatioDstToSrc = dfYSize / nBufYSize;

    /* Determine the coordinates in the "virtual" output raster to see */
    /* if there are not integers, in which case we will use them as a shift */
    /* so that subwindow extracts give the exact same results as entire raster */
    /* scaling */
    double dfDestXOff = dfXOff / dfXRatioDstToSrc;
    int bHasXOffVirtual = FALSE;
    int nDestXOffVirtual = 0;
    if( fabs(dfDestXOff - (int)(dfDestXOff + 0.5)) < 1e-8 )
    {
        bHasXOffVirtual = TRUE;
        dfXOff = nXOff;
        nDestXOffVirtual = (int)(dfDestXOff + 0.5);
    }

    double dfDestYOff = dfYOff / dfYRatioDstToSrc;
    int bHasYOffVirtual = FALSE;
    int nDestYOffVirtual = 0;
    if( fabs(dfDestYOff - (int)(dfDestYOff + 0.5)) < 1e-8 )
    {
        bHasYOffVirtual = TRUE;
        dfYOff = nYOff;
        nDestYOffVirtual = (int)(dfDestYOff + 0.5);
    }

    /* Create a MEM dataset that wraps the output buffer */
    GDALDataset* poMEMDS = MEMDataset::Create("", nDestXOffVirtual + nBufXSize,
                                              nDestYOffVirtual + nBufYSize, 0,
                                              eBufType, NULL);
    char szBuffer[64];
    int nRet;

    nRet = CPLPrintPointer(szBuffer, (GByte*)pData - nPixelSpace * nDestXOffVirtual
                            - nLineSpace * nDestYOffVirtual, sizeof(szBuffer));
    szBuffer[nRet] = 0;
    char** papszOptions = CSLSetNameValue(NULL, "DATAPOINTER", szBuffer);

    papszOptions = CSLSetNameValue(papszOptions, "PIXELOFFSET",
                                CPLSPrintf(CPL_FRMT_GIB, (GIntBig)nPixelSpace));

    papszOptions = CSLSetNameValue(papszOptions, "LINEOFFSET",
                                CPLSPrintf(CPL_FRMT_GIB, (GIntBig)nLineSpace));

    poMEMDS->AddBand(eBufType, papszOptions);
    CSLDestroy(papszOptions);
    GDALRasterBandH hMEMBand = (GDALRasterBandH)poMEMDS->GetRasterBand(1);

    /* Do the resampling */
    if( bUseWarp )
    {
        VRTDatasetH hVRTDS = NULL;
        GDALRasterBandH hVRTBand = NULL;
        if( GetDataset() == NULL )
        {
            /* Create VRT dataset that wraps the whole dataset */
            hVRTDS = VRTCreate(nRasterXSize, nRasterYSize);
            VRTAddBand( hVRTDS, eDataType, NULL );
            hVRTBand = GDALGetRasterBand(hVRTDS, 1);
            VRTAddSimpleSource( (VRTSourcedRasterBandH)hVRTBand,
                                (GDALRasterBandH)this, 
                                0, 0, 
                                nRasterXSize, nRasterYSize,
                                0, 0,
                                nRasterXSize, nRasterYSize,
                                NULL, VRT_NODATA_UNSET );

            /* Add a mask band if needed */
            if( GetMaskFlags() != GMF_ALL_VALID )
            {
                ((GDALDataset*)hVRTDS)->CreateMaskBand(0);
                VRTSourcedRasterBand* poVRTMaskBand =
                    (VRTSourcedRasterBand*)(((GDALRasterBand*)hVRTBand)->GetMaskBand());
                poVRTMaskBand->
                    AddMaskBandSource( this, 
                                    0, 0, 
                                    nRasterXSize, nRasterYSize,
                                    0, 0,
                                    nRasterXSize, nRasterYSize);
            }
        }

        GDALWarpOptions* psWarpOptions = GDALCreateWarpOptions();
        psWarpOptions->eResampleAlg = (GDALResampleAlg)psExtraArg->eResampleAlg;
        psWarpOptions->hSrcDS = (GDALDatasetH) (hVRTDS ? hVRTDS : GetDataset());
        psWarpOptions->hDstDS = (GDALDatasetH) poMEMDS;
        psWarpOptions->nBandCount = 1;
        int nSrcBandNumber = (hVRTDS ? 1 : nBand);
        int nDstBandNumber = 1;
        psWarpOptions->panSrcBands = &nSrcBandNumber;
        psWarpOptions->panDstBands = &nDstBandNumber;
        psWarpOptions->pfnProgress = psExtraArg->pfnProgress ?
                    psExtraArg->pfnProgress : GDALDummyProgress;
        psWarpOptions->pProgressArg = psExtraArg->pProgressData;
        psWarpOptions->pfnTransformer = GDALRasterIOTransformer;
        GDALRasterIOTransformerStruct sTransformer;
        sTransformer.dfXOff = (bHasXOffVirtual) ? 0 : dfXOff;
        sTransformer.dfYOff = (bHasYOffVirtual) ? 0 : dfYOff;
        sTransformer.dfXRatioDstToSrc = dfXRatioDstToSrc;
        sTransformer.dfYRatioDstToSrc = dfYRatioDstToSrc;
        psWarpOptions->pTransformerArg = &sTransformer;

        GDALWarpOperationH hWarpOperation = GDALCreateWarpOperation(psWarpOptions);
        eErr = GDALChunkAndWarpImage( hWarpOperation,
                                      nDestXOffVirtual, nDestYOffVirtual,
                                      nBufXSize, nBufYSize );
        GDALDestroyWarpOperation( hWarpOperation );

        psWarpOptions->panSrcBands = NULL;
        psWarpOptions->panDstBands = NULL;
        GDALDestroyWarpOptions( psWarpOptions );

        if( hVRTDS )
            GDALClose(hVRTDS);
    }
    else
    {
        const char* pszResampling =
            (psExtraArg->eResampleAlg == GRIORA_Bilinear) ? "BILINEAR" :
            (psExtraArg->eResampleAlg == GRIORA_Cubic) ? "CUBIC" :
            (psExtraArg->eResampleAlg == GRIORA_CubicSpline) ? "CUBICSPLINE" :
            (psExtraArg->eResampleAlg == GRIORA_Lanczos) ? "LANCZOS" :
            (psExtraArg->eResampleAlg == GRIORA_Average) ? "AVERAGE" :
            (psExtraArg->eResampleAlg == GRIORA_Mode) ? "MODE" :
            (psExtraArg->eResampleAlg == GRIORA_Gauss) ? "GAUSS" : "UNKNOWN";

        int nKernelRadius;
        GDALResampleFunction pfnResampleFunc =
                        GDALGetResampleFunction(pszResampling, &nKernelRadius);
        CPLAssert(pfnResampleFunc);
        GDALDataType eWrkDataType = 
            GDALGetOvrWorkDataType(pszResampling, eDataType);
        int bHasNoData = FALSE;
        float fNoDataValue = (float) GetNoDataValue(&bHasNoData);
        if( !bHasNoData )
            fNoDataValue = 0.0f;

        int nDstBlockXSize = nBufXSize;
        int nDstBlockYSize = nBufYSize;
        int nFullResXChunk, nFullResYChunk;
        while(TRUE)
        {
            nFullResXChunk = 3 + (int)(nDstBlockXSize * dfXRatioDstToSrc);
            nFullResYChunk = 3 + (int)(nDstBlockYSize * dfYRatioDstToSrc);
            if( (nDstBlockXSize == 1 && nDstBlockYSize == 1) ||
                ((GIntBig)nFullResXChunk * nFullResYChunk <= 1024 * 1024) )
                break;
            /* When operating on the full width of a raster whose block width is */
            /* the raster width, prefer doing chunks in height */
            if( nFullResXChunk >= nXSize && nXSize == nBlockXSize && nDstBlockYSize > 1 )
                nDstBlockYSize /= 2;
            /* Otherwise cut the maximal dimension */
            else if( nDstBlockXSize > 1 && nFullResXChunk > nFullResYChunk )
                nDstBlockXSize /= 2;
            else
                nDstBlockYSize /= 2;
        }

        int nOvrFactor = MAX( (int)(0.5 + dfXRatioDstToSrc),
                                (int)(0.5 + dfYRatioDstToSrc) );
        if( nOvrFactor == 0 ) nOvrFactor = 1;
        int nFullResXSizeQueried = nFullResXChunk + 2 * nKernelRadius * nOvrFactor;
        int nFullResYSizeQueried = nFullResYChunk + 2 * nKernelRadius * nOvrFactor;

        void * pChunk = 
            VSIMalloc3((GDALGetDataTypeSize(eWrkDataType)/8),
                        nFullResXSizeQueried, nFullResYSizeQueried );
        GByte * pabyChunkNoDataMask = NULL;

        GDALRasterBand* poMaskBand = NULL;
        int nMaskFlags = 0;
        int bUseNoDataMask = FALSE;

        poMaskBand = GetMaskBand();
        nMaskFlags = GetMaskFlags();

        bUseNoDataMask = ((nMaskFlags & GMF_ALL_VALID) == 0);
        if (bUseNoDataMask)
        {
            pabyChunkNoDataMask = (GByte *) 
                (GByte*) VSIMalloc2( nFullResXSizeQueried, nFullResYSizeQueried );
        }
        if( pChunk == NULL || (bUseNoDataMask && pabyChunkNoDataMask == NULL) )
        {
            GDALClose(poMEMDS);
            CPLFree(pChunk);
            CPLFree(pabyChunkNoDataMask);
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "Out of memory in RasterIO()." );
            return CE_Failure;
        }

        int nTotalBlocks = ((nBufXSize + nDstBlockXSize - 1) / nDstBlockXSize) *
                           ((nBufYSize + nDstBlockYSize - 1) / nDstBlockYSize);
        int nBlocksDone = 0;

        int nDstYOff;
        for( nDstYOff = 0; nDstYOff < nBufYSize && eErr == CE_None;
            nDstYOff += nDstBlockYSize )
        {
            int nDstYCount;
            if  (nDstYOff + nDstBlockYSize <= nBufYSize)
                nDstYCount = nDstBlockYSize;
            else
                nDstYCount = nBufYSize - nDstYOff;

            int nChunkYOff = nYOff + (int) (nDstYOff * dfYRatioDstToSrc);
            int nChunkYOff2 = nYOff + 1 + (int) ceil((nDstYOff + nDstYCount) * dfYRatioDstToSrc);
            if( nChunkYOff2 > nRasterYSize )
                nChunkYOff2 = nRasterYSize;
            int nYCount = nChunkYOff2 - nChunkYOff;
            CPLAssert(nYCount <= nFullResYChunk);

            int nChunkYOffQueried = nChunkYOff - nKernelRadius * nOvrFactor;
            int nChunkYSizeQueried = nYCount + 2 * nKernelRadius * nOvrFactor;
            if( nChunkYOffQueried < 0 )
            {
                nChunkYSizeQueried += nChunkYOffQueried;
                nChunkYOffQueried = 0;
            }
            if( nChunkYSizeQueried + nChunkYOffQueried > nRasterYSize )
                nChunkYSizeQueried = nRasterYSize - nChunkYOffQueried;
            CPLAssert(nChunkYSizeQueried <= nFullResYSizeQueried);

            int nDstXOff;
            for( nDstXOff = 0; nDstXOff < nBufXSize && eErr == CE_None;
                nDstXOff += nDstBlockXSize )
            {
                int nDstXCount;
                if  (nDstXOff + nDstBlockXSize <= nBufXSize)
                    nDstXCount = nDstBlockXSize;
                else
                    nDstXCount = nBufXSize - nDstXOff;

                int nChunkXOff = nXOff + (int) (nDstXOff * dfXRatioDstToSrc);
                int nChunkXOff2 = nXOff + 1 + (int) ceil((nDstXOff + nDstXCount) * dfXRatioDstToSrc);
                if( nChunkXOff2 > nRasterXSize )
                    nChunkXOff2 = nRasterXSize;
                int nXCount = nChunkXOff2 - nChunkXOff;
                CPLAssert(nXCount <= nFullResXChunk);

                int nChunkXOffQueried = nChunkXOff - nKernelRadius * nOvrFactor;
                int nChunkXSizeQueried = nXCount + 2 * nKernelRadius * nOvrFactor;
                if( nChunkXOffQueried < 0 )
                {
                    nChunkXSizeQueried += nChunkXOffQueried;
                    nChunkXOffQueried = 0;
                }
                if( nChunkXSizeQueried + nChunkXOffQueried > nRasterXSize )
                    nChunkXSizeQueried = nRasterXSize - nChunkXOffQueried;
                CPLAssert(nChunkXSizeQueried <= nFullResXSizeQueried);

                /* Read the source buffers */
                eErr = RasterIO( GF_Read,
                                nChunkXOffQueried, nChunkYOffQueried,
                                nChunkXSizeQueried, nChunkYSizeQueried, 
                                pChunk,
                                nChunkXSizeQueried, nChunkYSizeQueried,
                                eWrkDataType, 0, 0, NULL );

                int bSkipResample = FALSE;
                int bNoDataMaskFullyOpaque = FALSE;
                if (eErr == CE_None && bUseNoDataMask)
                {
                    eErr = poMaskBand->RasterIO( GF_Read,
                                                 nChunkXOffQueried,
                                                 nChunkYOffQueried,
                                                 nChunkXSizeQueried,
                                                 nChunkYSizeQueried, 
                                                 pabyChunkNoDataMask,
                                                 nChunkXSizeQueried,
                                                 nChunkYSizeQueried,
                                                 GDT_Byte, 0, 0, NULL );

                    /* Optimizations if mask if fully opaque or transparent */
                    int nPixels = nChunkXSizeQueried * nChunkYSizeQueried;
                    GByte bVal = pabyChunkNoDataMask[0];
                    int i = 1;
                    for( ; i < nPixels; i++ )
                    {
                        if( pabyChunkNoDataMask[i] != bVal )
                            break;
                    }
                    if( i == nPixels )
                    {
                        if( bVal == 0 )
                        {
                            for(int j=0;j<nDstYCount;j++)
                            {
                                GDALCopyWords(&fNoDataValue, GDT_Float32, 0,
                                            (GByte*)pData + nLineSpace * (j + nDstYOff) +
                                                        nDstXOff * nPixelSpace,
                                            eBufType, (int)nPixelSpace,
                                            nDstXCount);
                            }
                            bSkipResample = TRUE;
                        }
                        else
                        {
                            bNoDataMaskFullyOpaque = TRUE;
                        }
                    }
                }

                if( !bSkipResample && eErr == CE_None )
                {
                    eErr = pfnResampleFunc( dfXRatioDstToSrc,
                                            dfYRatioDstToSrc,
                                            dfXOff - nXOff, /* == 0 if bHasXOffVirtual */
                                            dfYOff - nYOff, /* == 0 if bHasYOffVirtual */
                                            eWrkDataType,
                                            pChunk,
                                            (bNoDataMaskFullyOpaque) ? NULL : pabyChunkNoDataMask,
                                            nChunkXOffQueried - ((bHasXOffVirtual) ? 0 : nXOff),
                                            nChunkXSizeQueried,
                                            nChunkYOffQueried - ((bHasYOffVirtual) ? 0 : nYOff),
                                            nChunkYSizeQueried,
                                            nDstXOff + nDestXOffVirtual,
                                            nDstXOff + nDestXOffVirtual + nDstXCount,
                                            nDstYOff + nDestYOffVirtual,
                                            nDstYOff + nDestYOffVirtual + nDstYCount,
                                            (GDALRasterBand *) hMEMBand,
                                            pszResampling,
                                            bHasNoData, fNoDataValue,
                                            GetColorTable(),
                                            eDataType );
                }

                nBlocksDone ++;
                if( eErr == CE_None && psExtraArg->pfnProgress != NULL &&
                    !psExtraArg->pfnProgress(1.0 * nBlocksDone / nTotalBlocks, "",
                                             psExtraArg->pProgressData) )
                {
                    eErr = CE_Failure;
                }
            }
        }
    }

    GDALClose(poMEMDS);

    return eErr;
}

/************************************************************************/
/*                           GDALSwapWords()                            */
/************************************************************************/

/**
 * Byte swap words in-place.
 *
 * This function will byte swap a set of 2, 4 or 8 byte words "in place" in
 * a memory array.  No assumption is made that the words being swapped are
 * word aligned in memory.  Use the CPL_LSB and CPL_MSB macros from cpl_port.h
 * to determine if the current platform is big endian or little endian.  Use
 * The macros like CPL_SWAP32() to byte swap single values without the overhead
 * of a function call. 
 * 
 * @param pData pointer to start of data buffer.
 * @param nWordSize size of words being swapped in bytes. Normally 2, 4 or 8.
 * @param nWordCount the number of words to be swapped in this call. 
 * @param nWordSkip the byte offset from the start of one word to the start of
 * the next. For packed buffers this is the same as nWordSize. 
 */

void CPL_STDCALL GDALSwapWords( void *pData, int nWordSize, int nWordCount,
                                int nWordSkip )

{
    if (nWordCount > 0)
        VALIDATE_POINTER0( pData , "GDALSwapWords" );

    int         i;
    GByte       *pabyData = (GByte *) pData;

    switch( nWordSize )
    {
      case 1:
        break;

      case 2:
        CPLAssert( nWordSkip >= 2 || nWordCount == 1 );
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
        CPLAssert( nWordSkip >= 4 || nWordCount == 1 );
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
        CPLAssert( nWordSkip >= 8 || nWordCount == 1 );
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

#ifdef USE_NEW_COPYWORDS
// Place the new GDALCopyWords helpers in an anonymous namespace
namespace {
/************************************************************************/
/*                          GetDataLimits()                             */
/************************************************************************/
/**
 * Compute the limits of values that can be placed in Tout in terms of
 * Tin. Usually used for output clamping, when the output data type's
 * limits are stable relative to the input type (i.e. no roundoff error).
 *
 * @param tMaxValue the returned maximum value
 * @param tMinValue the returned minimum value
 */

template <class Tin, class Tout>
inline void GetDataLimits(Tin &tMaxValue, Tin &tMinValue)
{
    tMaxValue = std::numeric_limits<Tin>::max();
    tMinValue = std::numeric_limits<Tin>::min();

    // Compute the actual minimum value of Tout in terms of Tin.
    if (std::numeric_limits<Tout>::is_signed && std::numeric_limits<Tout>::is_integer)
    {
        // the minimum value is less than zero
        if (std::numeric_limits<Tout>::digits < std::numeric_limits<Tin>::digits ||
			!std::numeric_limits<Tin>::is_integer)
        {
            // Tout is smaller than Tin, so we need to clamp values in input
            // to the range of Tout's min/max values
            if (std::numeric_limits<Tin>::is_signed)
            {
                tMinValue = static_cast<Tin>(std::numeric_limits<Tout>::min());
            }
            tMaxValue = static_cast<Tin>(std::numeric_limits<Tout>::max());
        }
    }
    else if (std::numeric_limits<Tout>::is_integer)
    {
        // the output is unsigned, so we just need to determine the max
        if (std::numeric_limits<Tout>::digits <= std::numeric_limits<Tin>::digits)
        {
            // Tout is smaller than Tin, so we need to clamp the input values
            // to the range of Tout's max
            tMaxValue = static_cast<Tin>(std::numeric_limits<Tout>::max());
        }
        tMinValue = 0;
    }

}

/************************************************************************/
/*                            ClampValue()                                */
/************************************************************************/
/**
 * Clamp values of type T to a specified range
 *
 * @param tValue the value
 * @param tMax the max value
 * @param tMin the min value
 */
template <class T>
inline T ClampValue(const T tValue, const T tMax, const T tMin)
{
    return tValue > tMax ? tMax :
           tValue < tMin ? tMin : tValue;
}

/************************************************************************/
/*                            CopyWord()                                */
/************************************************************************/
/**
 * Copy a single word, optionally rounding if appropriate (i.e. going
 * from the float to the integer case). Note that this is the function
 * you should specialize if you're adding a new data type.
 *
 * @param tValueIn value of type Tin; the input value to be converted
 * @param tValueOut value of type Tout; the output value
 */

template <class Tin, class Tout>
inline void CopyWord(const Tin tValueIn, Tout &tValueOut)
{
    Tin tMaxVal, tMinVal;
    GetDataLimits<Tin, Tout>(tMaxVal, tMinVal);
    tValueOut = static_cast<Tout>(ClampValue(tValueIn, tMaxVal, tMinVal));
}

template <class Tin>
inline void CopyWord(const Tin tValueIn, float &fValueOut)
{
    fValueOut = (float) tValueIn;
}

template <class Tin>
inline void CopyWord(const Tin tValueIn, double &dfValueOut)
{
    dfValueOut = tValueIn;
}

inline void CopyWord(const double dfValueIn, double &dfValueOut)
{
    dfValueOut = dfValueIn;
}

inline void CopyWord(const float fValueIn, float &fValueOut)
{
    fValueOut = fValueIn;
}

inline void CopyWord(const float fValueIn, double &dfValueOut)
{
    dfValueOut = fValueIn;
}

inline void CopyWord(const double dfValueIn, float &fValueOut)
{
    fValueOut = static_cast<float>(dfValueIn);
}

template <class Tout>
inline void CopyWord(const float fValueIn, Tout &tValueOut)
{
    float fMaxVal, fMinVal;
    GetDataLimits<float, Tout>(fMaxVal, fMinVal);
    tValueOut = static_cast<Tout>(
        ClampValue(fValueIn + 0.5f, fMaxVal, fMinVal));
}

template <class Tout>
inline void CopyWord(const double dfValueIn, Tout &tValueOut)
{
    double dfMaxVal, dfMinVal;
    GetDataLimits<double, Tout>(dfMaxVal, dfMinVal);
    tValueOut = static_cast<Tout>(
        ClampValue(dfValueIn + 0.5, dfMaxVal, dfMinVal));
}

inline void CopyWord(const double dfValueIn, int &nValueOut)
{
    double dfMaxVal, dfMinVal;
    GetDataLimits<double, int>(dfMaxVal, dfMinVal);
    double dfValue = dfValueIn >= 0.0 ? dfValueIn + 0.5 :
        dfValueIn - 0.5;
    nValueOut = static_cast<int>(
        ClampValue(dfValue, dfMaxVal, dfMinVal));
}

inline void CopyWord(const float fValueIn, short &nValueOut)
{
    float fMaxVal, fMinVal;
    GetDataLimits<float, short>(fMaxVal, fMinVal);
    float fValue = fValueIn >= 0.0f ? fValueIn + 0.5f :
        fValueIn - 0.5f;
    nValueOut = static_cast<short>(
        ClampValue(fValue, fMaxVal, fMinVal));
}

inline void CopyWord(const double dfValueIn, short &nValueOut)
{
    double dfMaxVal, dfMinVal;
    GetDataLimits<double, short>(dfMaxVal, dfMinVal);
    double dfValue = dfValueIn > 0.0 ? dfValueIn + 0.5 :
        dfValueIn - 0.5;
    nValueOut = static_cast<short>(
        ClampValue(dfValue, dfMaxVal, dfMinVal));
}

// Roundoff occurs for Float32 -> int32 for max/min. Overload CopyWord
// specifically for this case.
inline void CopyWord(const float fValueIn, int &nValueOut)
{
    if (fValueIn >= static_cast<float>(std::numeric_limits<int>::max()))
    {
        nValueOut = std::numeric_limits<int>::max();
    }
    else if (fValueIn <= static_cast<float>(std::numeric_limits<int>::min()))
    {
        nValueOut = std::numeric_limits<int>::min();
    }
    else
    {
        nValueOut = static_cast<int>(fValueIn > 0.0f ? 
            fValueIn + 0.5f : fValueIn - 0.5f);
    }
}

// Roundoff occurs for Float32 -> uint32 for max. Overload CopyWord
// specifically for this case.
inline void CopyWord(const float fValueIn, unsigned int &nValueOut)
{
    if (fValueIn >= static_cast<float>(std::numeric_limits<unsigned int>::max()))
    {
        nValueOut = std::numeric_limits<unsigned int>::max();
    }
    else if (fValueIn <= static_cast<float>(std::numeric_limits<unsigned int>::min()))
    {
        nValueOut = std::numeric_limits<unsigned int>::min();
    }
    else
    {
        nValueOut = static_cast<unsigned int>(fValueIn + 0.5f);
    }
}

/************************************************************************/
/*                           GDALCopyWordsT()                           */
/************************************************************************/
/**
 * Template function, used to copy data from pSrcData into buffer
 * pDstData, with stride nSrcPixelStride in the source data and
 * stride nDstPixelStride in the destination data. This template can
 * deal with the case where the input data type is real or complex and
 * the output is real.
 *
 * @param pSrcData the source data buffer
 * @param nSrcPixelStride the stride, in the buffer pSrcData for pixels
 *                      of interest.
 * @param pDstData the destination buffer.
 * @param nDstPixelStride the stride in the buffer pDstData for pixels of
 *                      interest.
 * @param nWordCount the total number of pixel words to copy
 *
 * @code
 * // Assume an input buffer of type GUInt16 named pBufferIn 
 * GByte *pBufferOut = new GByte[numBytesOut];
 * GDALCopyWordsT<GUInt16, GByte>(pSrcData, 2, pDstData, 1, numBytesOut);
 * @code
 * @note
 * This is a private function, and should not be exposed outside of rasterio.cpp.
 * External users should call the GDALCopyWords driver function.
 * @note
 */

template <class Tin, class Tout>
static void GDALCopyWordsT(const Tin* const pSrcData, int nSrcPixelStride,
                           Tout* const pDstData, int nDstPixelStride,
                           int nWordCount)
{
    std::ptrdiff_t nDstOffset = 0;

    const char* const pSrcDataPtr = reinterpret_cast<const char*>(pSrcData);
    char* const pDstDataPtr = reinterpret_cast<char*>(pDstData);
    for (std::ptrdiff_t n = 0; n < nWordCount; n++)
    {
        const Tin tValue = *reinterpret_cast<const Tin*>(pSrcDataPtr + (n * nSrcPixelStride));
        Tout* const pOutPixel = reinterpret_cast<Tout*>(pDstDataPtr + nDstOffset);

        CopyWord(tValue, *pOutPixel);

        nDstOffset += nDstPixelStride;
    }
}

/************************************************************************/
/*                   GDALCopyWordsComplexT()                            */
/************************************************************************/
/**
 * Template function, used to copy data from pSrcData into buffer
 * pDstData, with stride nSrcPixelStride in the source data and
 * stride nDstPixelStride in the destination data. Deals with the 
 * complex case, where input is complex and output is complex.
 *
 * @param pSrcData the source data buffer
 * @param nSrcPixelStride the stride, in the buffer pSrcData for pixels
 *                      of interest.
 * @param pDstData the destination buffer.
 * @param nDstPixelStride the stride in the buffer pDstData for pixels of
 *                      interest.
 * @param nWordCount the total number of pixel words to copy
 *
 */
template <class Tin, class Tout>
inline void GDALCopyWordsComplexT(const Tin* const pSrcData, int nSrcPixelStride,
                                  Tout* const pDstData, int nDstPixelStride,
                                  int nWordCount)
{
    std::ptrdiff_t nDstOffset = 0;
    const char* const pSrcDataPtr = reinterpret_cast<const char*>(pSrcData);
    char* const pDstDataPtr = reinterpret_cast<char*>(pDstData);

    // Determine the minimum and maximum value we can have based
    // on the constraints of Tin and Tout.
    Tin tMaxValue, tMinValue;
    GetDataLimits<Tin, Tout>(tMaxValue, tMinValue);

    for (std::ptrdiff_t n = 0; n < nWordCount; n++)
    {
        const Tin* const pPixelIn = reinterpret_cast<const Tin*>(pSrcDataPtr + n * nSrcPixelStride);
        Tout* const pPixelOut = reinterpret_cast<Tout*>(pDstDataPtr + nDstOffset);

        CopyWord(pPixelIn[0], pPixelOut[0]);
        CopyWord(pPixelIn[1], pPixelOut[1]);

        nDstOffset += nDstPixelStride;
    }
}

/************************************************************************/
/*                   GDALCopyWordsComplexOutT()                         */
/************************************************************************/
/**
 * Template function, used to copy data from pSrcData into buffer
 * pDstData, with stride nSrcPixelStride in the source data and
 * stride nDstPixelStride in the destination data. Deals with the 
 * case where the value is real coming in, but complex going out.
 *
 * @param pSrcData the source data buffer
 * @param nSrcPixelStride the stride, in the buffer pSrcData for pixels
 *                      of interest, in bytes.
 * @param pDstData the destination buffer.
 * @param nDstPixelStride the stride in the buffer pDstData for pixels of
 *                      interest, in bytes.
 * @param nWordCount the total number of pixel words to copy
 *
 */
template <class Tin, class Tout>
inline void GDALCopyWordsComplexOutT(const Tin* const pSrcData, int nSrcPixelStride,
                                     Tout* const pDstData, int nDstPixelStride,
                                     int nWordCount)
{
    std::ptrdiff_t nDstOffset = 0;

    const Tout tOutZero = static_cast<Tout>(0);

    const char* const pSrcDataPtr = reinterpret_cast<const char*>(pSrcData);
    char* const pDstDataPtr = reinterpret_cast<char*>(pDstData);

    for (std::ptrdiff_t n = 0; n < nWordCount; n++)
    {
        const Tin tValue = *reinterpret_cast<const Tin* const>(pSrcDataPtr + n * nSrcPixelStride);
        Tout* const pPixelOut = reinterpret_cast<Tout*>(pDstDataPtr + nDstOffset);
        CopyWord(tValue, *pPixelOut);

        pPixelOut[1] = tOutZero;

        nDstOffset += nDstPixelStride;
    }
}

/************************************************************************/
/*                           GDALCopyWordsFromT()                       */
/************************************************************************/
/**
 * Template driver function. Given the input type T, call the appropriate
 * GDALCopyWordsT function template for the desired output type. You should
 * never call this function directly (call GDALCopyWords instead).
 *
 * @param pSrcData source data buffer
 * @param nSrcPixelStride pixel stride in input buffer, in pixel words
 * @param bInComplex input is complex
 * @param pDstData destination data buffer
 * @param eDstType destination data type
 * @param nDstPixelStride pixel stride in output buffer, in pixel words
 * @param nWordCount number of pixel words to be copied
 */
template <class T>
inline void GDALCopyWordsFromT(const T* const pSrcData, int nSrcPixelStride, bool bInComplex,
                               void *pDstData, GDALDataType eDstType, int nDstPixelStride,
                               int nWordCount)
{
    switch (eDstType)
    {
    case GDT_Byte:
        GDALCopyWordsT(pSrcData, nSrcPixelStride,
                       static_cast<unsigned char*>(pDstData), nDstPixelStride,
                       nWordCount);
        break;
    case GDT_UInt16:
        GDALCopyWordsT(pSrcData, nSrcPixelStride,
                       static_cast<unsigned short*>(pDstData), nDstPixelStride,
                       nWordCount);
        break;
    case GDT_Int16:
        GDALCopyWordsT(pSrcData, nSrcPixelStride,
                       static_cast<short*>(pDstData), nDstPixelStride,
                       nWordCount);
        break;
    case GDT_UInt32:
        GDALCopyWordsT(pSrcData, nSrcPixelStride,
                       static_cast<unsigned int*>(pDstData), nDstPixelStride,
                       nWordCount);
        break;
    case GDT_Int32:
        GDALCopyWordsT(pSrcData, nSrcPixelStride,
                       static_cast<int*>(pDstData), nDstPixelStride,
                       nWordCount);
        break;
    case GDT_Float32:
        GDALCopyWordsT(pSrcData, nSrcPixelStride,
                       static_cast<float*>(pDstData), nDstPixelStride,
                       nWordCount);
        break;
    case GDT_Float64:
        GDALCopyWordsT(pSrcData, nSrcPixelStride,
                       static_cast<double*>(pDstData), nDstPixelStride,
                       nWordCount);
        break;
    case GDT_CInt16:
        if (bInComplex)
        {
            GDALCopyWordsComplexT(pSrcData, nSrcPixelStride,
                                  static_cast<short *>(pDstData), nDstPixelStride,
                                  nWordCount);
        }
        else // input is not complex, so we need to promote to a complex buffer
        {
            GDALCopyWordsComplexOutT(pSrcData, nSrcPixelStride,
                                     static_cast<short *>(pDstData), nDstPixelStride,
                                     nWordCount);
        }
        break;
    case GDT_CInt32:
        if (bInComplex)
        {
            GDALCopyWordsComplexT(pSrcData, nSrcPixelStride,
                                  static_cast<int *>(pDstData), nDstPixelStride,
                                  nWordCount);
        }
        else // input is not complex, so we need to promote to a complex buffer
        {
            GDALCopyWordsComplexOutT(pSrcData, nSrcPixelStride,
                                     static_cast<int *>(pDstData), nDstPixelStride,
                                     nWordCount);
        }
        break;
    case GDT_CFloat32:
        if (bInComplex)
        {
            GDALCopyWordsComplexT(pSrcData, nSrcPixelStride,
                                  static_cast<float *>(pDstData), nDstPixelStride,
                                  nWordCount);
        }
        else // input is not complex, so we need to promote to a complex buffer
        {
            GDALCopyWordsComplexOutT(pSrcData, nSrcPixelStride,
                                     static_cast<float *>(pDstData), nDstPixelStride,
                                     nWordCount);
        }
        break;
    case GDT_CFloat64:
        if (bInComplex)
        {
            GDALCopyWordsComplexT(pSrcData, nSrcPixelStride,
                                  static_cast<double *>(pDstData), nDstPixelStride,
                                  nWordCount);
        }
        else // input is not complex, so we need to promote to a complex buffer
        {
            GDALCopyWordsComplexOutT(pSrcData, nSrcPixelStride,
                                     static_cast<double *>(pDstData), nDstPixelStride,
                                     nWordCount);
        }
        break;
    case GDT_Unknown:
    default:
        CPLAssert(FALSE);
    }
}

} // end anonymous namespace
#endif

/************************************************************************/
/*                          GDALReplicateWord()                         */
/************************************************************************/

void GDALReplicateWord(void *pSrcData, GDALDataType eSrcType,
                       void *pDstData, GDALDataType eDstType, int nDstPixelStride,
                       int nWordCount)
{
/* ----------------------------------------------------------------------- */
/* Special case when the source data is always the same value              */
/* (for VRTSourcedRasterBand::IRasterIO and VRTDerivedRasterBand::IRasterIO*/
/*  for example)                                                           */
/* ----------------------------------------------------------------------- */
    /* Let the general translation case do the necessary conversions */
    /* on the first destination element */
    GDALCopyWords(pSrcData, eSrcType, 0,
                  pDstData, eDstType, nDstPixelStride,
                  1 );

    /* Now copy the first element to the nWordCount - 1 following destination */
    /* elements */
    nWordCount--;
    GByte *pabyDstWord = ((GByte *)pDstData) + nDstPixelStride;

    switch (eDstType)
    {
        case GDT_Byte:
        {
            if (nDstPixelStride == 1)
            {
                if (nWordCount > 0)
                    memset(pabyDstWord, *(GByte*)pDstData, nWordCount);
            }
            else
            {
                GByte valSet = *(GByte*)pDstData;
                while(nWordCount--)
                {
                    *pabyDstWord = valSet;
                    pabyDstWord += nDstPixelStride;
                }
            }
            break;
        }

#define CASE_DUPLICATE_SIMPLE(enum_type, c_type) \
        case enum_type:\
        { \
            c_type valSet = *(c_type*)pDstData; \
            while(nWordCount--) \
            { \
                *(c_type*)pabyDstWord = valSet; \
                pabyDstWord += nDstPixelStride; \
            } \
            break; \
        }

        CASE_DUPLICATE_SIMPLE(GDT_UInt16, GUInt16)
        CASE_DUPLICATE_SIMPLE(GDT_Int16,  GInt16)
        CASE_DUPLICATE_SIMPLE(GDT_UInt32, GUInt32)
        CASE_DUPLICATE_SIMPLE(GDT_Int32,  GInt32)
        CASE_DUPLICATE_SIMPLE(GDT_Float32,float)
        CASE_DUPLICATE_SIMPLE(GDT_Float64,double)

#define CASE_DUPLICATE_COMPLEX(enum_type, c_type) \
        case enum_type:\
        { \
            c_type valSet1 = ((c_type*)pDstData)[0]; \
            c_type valSet2 = ((c_type*)pDstData)[1]; \
            while(nWordCount--) \
            { \
                ((c_type*)pabyDstWord)[0] = valSet1; \
                ((c_type*)pabyDstWord)[1] = valSet2; \
                pabyDstWord += nDstPixelStride; \
            } \
            break; \
        }

        CASE_DUPLICATE_COMPLEX(GDT_CInt16, GInt16)
        CASE_DUPLICATE_COMPLEX(GDT_CInt32, GInt32)
        CASE_DUPLICATE_COMPLEX(GDT_CFloat32, float)
        CASE_DUPLICATE_COMPLEX(GDT_CFloat64, double)

        default:
            CPLAssert( FALSE );
    }
}

/************************************************************************/
/*                           GDALCopyWords()                            */
/************************************************************************/

/**
 * Copy pixel words from buffer to buffer.
 *
 * This function is used to copy pixel word values from one memory buffer
 * to another, with support for conversion between data types, and differing
 * step factors.  The data type conversion is done using the normal GDAL 
 * rules.  Values assigned to a lower range integer type are clipped.  For
 * instance assigning GDT_Int16 values to a GDT_Byte buffer will cause values
 * less the 0 to be set to 0, and values larger than 255 to be set to 255. 
 * Assignment from floating point to integer uses default C type casting
 * semantics.   Assignment from non-complex to complex will result in the 
 * imaginary part being set to zero on output.  Assigment from complex to 
 * non-complex will result in the complex portion being lost and the real
 * component being preserved (<i>not magnitidue!</i>). 
 *
 * No assumptions are made about the source or destination words occuring
 * on word boundaries.  It is assumed that all values are in native machine
 * byte order. 
 *
 * @param pSrcData Pointer to source data to be converted.
 * @param eSrcType the source data type (see GDALDataType enum)
 * @param nSrcPixelStride Source pixel stride (i.e. distance between 2 words), in bytes
 * @param pDstData Pointer to buffer where destination data should go
 * @param eDstType the destination data type (see GDALDataType enum)
 * @param nDstPixelStride Destination pixel stride (i.e. distance between 2 words), in bytes
 * @param nWordCount number of words to be copied
 *
 * 
 * @note 
 * When adding a new data type to GDAL, you must do the following to
 * support it properly within the GDALCopyWords function:
 * 1. Add the data type to the switch on eSrcType in GDALCopyWords.
 *    This should invoke the appropriate GDALCopyWordsFromT wrapper.
 * 2. Add the data type to the switch on eDstType in GDALCopyWordsFromT.
 *    This should call the appropriate GDALCopyWordsT template.
 * 3. If appropriate, overload the appropriate CopyWord template in the
 *    above namespace. This will ensure that any conversion issues are
 *    handled (cases like the float -> int32 case, where the min/max)
 *    values are subject to roundoff error.
 */

void CPL_STDCALL
GDALCopyWords( void * pSrcData, GDALDataType eSrcType, int nSrcPixelStride,
               void * pDstData, GDALDataType eDstType, int nDstPixelStride,
               int nWordCount )

{
    // Deal with the case where we're replicating a single word into the
    // provided buffer
    if (nSrcPixelStride == 0 && nWordCount > 1)
    {
        GDALReplicateWord(pSrcData, eSrcType, pDstData, eDstType, nDstPixelStride, nWordCount);
        return;
    }

#ifdef USE_NEW_COPYWORDS

    int nSrcDataTypeSize = GDALGetDataTypeSize(eSrcType) / 8;
    // Let memcpy() handle the case where we're copying a packed buffer
    // of pixels.
    if (eSrcType == eDstType && nSrcPixelStride == nDstPixelStride &&
        nSrcPixelStride == nSrcDataTypeSize)
    {
        memcpy(pDstData, pSrcData, nWordCount * nSrcDataTypeSize);
        return;
    }

    // Handle the more general case -- deals with conversion of data types
    // directly.
    switch (eSrcType)
    {
    case GDT_Byte:
        GDALCopyWordsFromT<unsigned char>(static_cast<unsigned char *>(pSrcData), nSrcPixelStride, false,
                                 pDstData, eDstType, nDstPixelStride,
                                 nWordCount);
        break;
    case GDT_UInt16:
        GDALCopyWordsFromT<unsigned short>(static_cast<unsigned short *>(pSrcData), nSrcPixelStride, false,
                                           pDstData, eDstType, nDstPixelStride,
                                           nWordCount);
        break;
    case GDT_Int16:
        GDALCopyWordsFromT<short>(static_cast<short *>(pSrcData), nSrcPixelStride, false,
                                  pDstData, eDstType, nDstPixelStride,
                                  nWordCount);
        break;
    case GDT_UInt32:
        GDALCopyWordsFromT<unsigned int>(static_cast<unsigned int *>(pSrcData), nSrcPixelStride, false,
                                         pDstData, eDstType, nDstPixelStride,
                                         nWordCount);
        break;
    case GDT_Int32:
        GDALCopyWordsFromT<int>(static_cast<int *>(pSrcData), nSrcPixelStride, false,
                                pDstData, eDstType, nDstPixelStride,
                                nWordCount);
        break;
    case GDT_Float32:
        GDALCopyWordsFromT<float>(static_cast<float *>(pSrcData), nSrcPixelStride, false,
                                  pDstData, eDstType, nDstPixelStride,
                                  nWordCount);
        break;
    case GDT_Float64:
        GDALCopyWordsFromT<double>(static_cast<double *>(pSrcData), nSrcPixelStride, false,
                                   pDstData, eDstType, nDstPixelStride,
                                   nWordCount);
        break;
    case GDT_CInt16:
        GDALCopyWordsFromT<short>(static_cast<short *>(pSrcData), nSrcPixelStride, true,
                                 pDstData, eDstType, nDstPixelStride,
                                 nWordCount);
        break;
    case GDT_CInt32:
        GDALCopyWordsFromT<int>(static_cast<int *>(pSrcData), nSrcPixelStride, true,
                                 pDstData, eDstType, nDstPixelStride,
                                 nWordCount);
        break;
    case GDT_CFloat32:
        GDALCopyWordsFromT<float>(static_cast<float *>(pSrcData), nSrcPixelStride, true,
                                 pDstData, eDstType, nDstPixelStride,
                                 nWordCount);
        break;
    case GDT_CFloat64:
        GDALCopyWordsFromT<double>(static_cast<double *>(pSrcData), nSrcPixelStride, true,
                                 pDstData, eDstType, nDstPixelStride,
                                 nWordCount);
        break;
    case GDT_Unknown:
    default:
        CPLAssert(FALSE);
    }

#else // undefined USE_NEW_COPYWORDS
/* -------------------------------------------------------------------- */
/*      Special case when no data type translation is required.         */
/* -------------------------------------------------------------------- */
    if( eSrcType == eDstType )
    {
        int     nWordSize = GDALGetDataTypeSize(eSrcType)/8;
        int     i;

        // contiguous blocks.
        if( nWordSize == nSrcPixelStride && nWordSize == nDstPixelStride )
        {
            memcpy( pDstData, pSrcData, nSrcPixelStride * nWordCount );
            return;
        }

        GByte *pabySrc = (GByte *) pSrcData;
        GByte *pabyDst = (GByte *) pDstData;

        // Moving single bytes.
        if( nWordSize == 1 )
        {
            if (nWordCount > 100)
            {
/* ==================================================================== */
/*     Optimization for high number of words to transfer and some       */
/*     typical source and destination pixel spacing : we unroll the     */
/*     loop.                                                            */
/* ==================================================================== */
#define ASSIGN(_nSrcPixelStride, _nDstPixelStride, _k) \
                 pabyDst[_nDstPixelStride * _k] = pabySrc[_nSrcPixelStride * _k]
#define ASSIGN_LOOP(_nSrcPixelStride, _nDstPixelStride) \
                for( i = nWordCount / 16 ; i != 0; i-- ) \
                { \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 0); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 1); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 2); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 3); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 4); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 5); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 6); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 7); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 8); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 9); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 10); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 11); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 12); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 13); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 14); \
                    ASSIGN(_nSrcPixelStride, _nDstPixelStride, 15); \
                    pabyDst += _nDstPixelStride * 16; \
                    pabySrc += _nSrcPixelStride * 16; \
                } \
                nWordCount = nWordCount % 16;

                if (nSrcPixelStride == 3 && nDstPixelStride == 1)
                {
                    ASSIGN_LOOP(3, 1)
                }
                else if (nSrcPixelStride == 1 && nDstPixelStride == 3)
                {
                    ASSIGN_LOOP(1, 3)
                }
                else if (nSrcPixelStride == 4 && nDstPixelStride == 1)
                {
                    ASSIGN_LOOP(4, 1)
                }
                else if (nSrcPixelStride == 1 && nDstPixelStride == 4)
                {
                    ASSIGN_LOOP(1, 4)
                }
                else if (nSrcPixelStride == 3 && nDstPixelStride == 4)
                {
                    ASSIGN_LOOP(3, 4)
                }
                else if (nSrcPixelStride == 4 && nDstPixelStride == 3)
                {
                    ASSIGN_LOOP(4, 3)
                }
            }

            for( i = nWordCount; i != 0; i-- )
            {
                *pabyDst = *pabySrc;
                pabyDst += nDstPixelStride;
                pabySrc += nSrcPixelStride;
            }
        }
        else if (nWordSize == 2)
        {
            for( i = nWordCount; i != 0; i-- )
            {
                *(short*)pabyDst = *(short*)pabySrc;
                pabyDst += nDstPixelStride;
                pabySrc += nSrcPixelStride;
            }
        }
        else if (nWordSize == 4)
        {
            for( i = nWordCount; i != 0; i-- )
            {
                *(int*)pabyDst = *(int*)pabySrc;
                pabyDst += nDstPixelStride;
                pabySrc += nSrcPixelStride;
            }
        }
        else if (nWordSize == 8)
        {
            for( i = nWordCount; i != 0; i-- )
            {
                ((int*)pabyDst)[0] = ((int*)pabySrc)[0];
                ((int*)pabyDst)[1] = ((int*)pabySrc)[1];
                pabyDst += nDstPixelStride;
                pabySrc += nSrcPixelStride;
            }
        }
        else if (nWordSize == 16)
        {
            for( i = nWordCount; i != 0; i-- )
            {
                ((int*)pabyDst)[0] = ((int*)pabySrc)[0];
                ((int*)pabyDst)[1] = ((int*)pabySrc)[1];
                ((int*)pabyDst)[2] = ((int*)pabySrc)[2];
                ((int*)pabyDst)[3] = ((int*)pabySrc)[3];
                pabyDst += nDstPixelStride;
                pabySrc += nSrcPixelStride;
            }
        }
        else
        {
            CPLAssert(FALSE);
        }

        return;
    }
    
/* ==================================================================== */
/*      General translation case                                        */
/* ==================================================================== */
    for( int iWord = 0; iWord < nWordCount; iWord++ )
    {
        void   *pSrcWord, *pDstWord;
        double  dfPixelValue=0.0, dfPixelValueI=0.0;

        pSrcWord = static_cast<GByte *>(pSrcData) + iWord * nSrcPixelStride;
        pDstWord = static_cast<GByte *>(pDstData) + iWord * nDstPixelStride;

/* -------------------------------------------------------------------- */
/*      Fetch source value based on data type.                          */
/* -------------------------------------------------------------------- */
        switch( eSrcType )
        {
          case GDT_Byte:
          {
              GByte byVal = *static_cast<GByte *>(pSrcWord);
              switch( eDstType )
              {
                case GDT_UInt16:
                  *static_cast<GUInt16 *>(pDstWord) = byVal;
                  continue;
                case GDT_Int16:
                  *static_cast<GInt16 *>(pDstWord) = byVal;
                  continue;
                case GDT_UInt32:
                  *static_cast<GUInt32 *>(pDstWord) = byVal;
                  continue;
                case GDT_Int32:
                  *static_cast<GInt32 *>(pDstWord) = byVal;
                  continue;
                case GDT_CInt16:
                {
                    GInt16 *panDstWord = static_cast<GInt16 *>(pDstWord);
                    panDstWord[0] = byVal;
                    panDstWord[1] = 0;
                    continue;
                }
                case GDT_CInt32:
                {
                    GInt32 *panDstWord = static_cast<GInt32 *>(pDstWord);
                    panDstWord[0] = byVal;
                    panDstWord[1] = 0;
                    continue;
                }
                default:
                  break;
              }
              dfPixelValue = byVal;
          }
          break;

          case GDT_UInt16:
          {
              GUInt16 nVal = *static_cast<GUInt16 *>(pSrcWord);
              switch( eDstType )
              {
                case GDT_Byte:
                {
                    GByte byVal;
                    if( nVal > 255 )
                        byVal = 255;
                    else
                        byVal = static_cast<GByte>(nVal);
                    *static_cast<GByte *>(pDstWord) = byVal;
                    continue;
                }
                case GDT_Int16:
                  if( nVal > 32767 )
                      nVal = 32767;
                  *static_cast<GInt16 *>(pDstWord) = nVal;
                  continue;
                case GDT_UInt32:
                  *static_cast<GUInt32 *>(pDstWord) = nVal;
                  continue;
                case GDT_Int32:
                  *static_cast<GInt32 *>(pDstWord) = nVal;
                  continue;
                case GDT_CInt16:
                {
                    GInt16 *panDstWord = static_cast<GInt16 *>(pDstWord);
                    if( nVal > 32767 )
                        nVal = 32767;
                    panDstWord[0] = nVal;
                    panDstWord[1] = 0;
                    continue;
                }
                case GDT_CInt32:
                {
                    GInt32 *panDstWord = static_cast<GInt32 *>(pDstWord);
                    panDstWord[0] = nVal;
                    panDstWord[1] = 0;
                    continue;
                }
                default:
                  break;
              }
              dfPixelValue = nVal;
          }
          break;
          
          case GDT_Int16:
          {
              GInt16 nVal = *static_cast<GInt16 *>(pSrcWord);
              switch( eDstType )
              {
                case GDT_Byte:
                {
                    GByte byVal;
                    if( nVal > 255 )
                        byVal = 255;
                    else if (nVal < 0)
                        byVal = 0;
                    else
                        byVal = static_cast<GByte>(nVal);
                    *static_cast<GByte *>(pDstWord) = byVal;
                    continue;
                }
                case GDT_UInt16:
                  if( nVal < 0 )
                      nVal = 0;
                  *static_cast<GUInt16 *>(pDstWord) = nVal;
                  continue;
                case GDT_UInt32:
                  if( nVal < 0 )
                      nVal = 0;
                  *static_cast<GUInt32 *>(pDstWord) = nVal;
                  continue;
                case GDT_Int32:
                  *static_cast<GInt32 *>(pDstWord) = nVal;
                  continue;
                case GDT_CInt16:
                {
                    GInt16 *panDstWord = static_cast<GInt16 *>(pDstWord);
                    panDstWord[0] = nVal;
                    panDstWord[1] = 0;
                    continue;
                }
                case GDT_CInt32:
                {
                    GInt32 *panDstWord = static_cast<GInt32 *>(pDstWord);
                    panDstWord[0] = nVal;
                    panDstWord[1] = 0;
                    continue;
                }
                default:
                  break;
              }
              dfPixelValue = nVal;
          }
          break;
          
          case GDT_Int32:
          {
              GInt32 nVal = *static_cast<GInt32 *>(pSrcWord);
              switch( eDstType )
              {
                case GDT_Byte:
                {
                    GByte byVal;
                    if( nVal > 255 )
                        byVal = 255;
                    else if (nVal < 0)
                        byVal = 0;
                    else
                        byVal = nVal;
                    *static_cast<GByte *>(pDstWord) = byVal;
                    continue;
                }
                case GDT_UInt16:
                  if( nVal > 65535 )
                      nVal = 65535;
                  else if( nVal < 0 )
                      nVal = 0;
                  *static_cast<GUInt16 *>(pDstWord) = nVal;
                  continue;
                case GDT_Int16:
                  if( nVal > 32767 )
                      nVal = 32767;
                  else if( nVal < -32768)
                      nVal = -32768;
                  *static_cast<GInt16 *>(pDstWord) = nVal;
                  continue;
                case GDT_UInt32:
                  if( nVal < 0 )
                      nVal = 0;
                  *static_cast<GUInt32 *>(pDstWord) = nVal;
                  continue;
                case GDT_CInt16:
                {
                    GInt16 *panDstWord = static_cast<GInt16 *>(pDstWord);
                    if( nVal > 32767 )
                        nVal = 32767;
                    else if( nVal < -32768)
                        nVal = -32768;
                    panDstWord[0] = nVal;
                    panDstWord[1] = 0;
                    continue;
                }
                case GDT_CInt32:
                {
                    GInt32 *panDstWord = static_cast<GInt32 *>(pDstWord);
                    panDstWord[0] = nVal;
                    panDstWord[1] = 0;
                    continue;
                }
                default:
                  break;
              }
              dfPixelValue = nVal;
          }
          break;
          
          case GDT_UInt32:
          {
              GUInt32 nVal = *static_cast<GUInt32 *>(pSrcWord);
              switch( eDstType )
              {
                case GDT_Byte:
                {
                    GByte byVal;
                    if( nVal > 255 )
                        byVal = 255;
                    else
                        byVal = nVal;
                    *static_cast<GByte *>(pDstWord) = byVal;
                    continue;
                }
                case GDT_UInt16:
                  if( nVal > 65535 )
                      nVal = 65535;
                  *static_cast<GUInt16 *>(pDstWord) = nVal;
                  continue;
                case GDT_Int16:
                  if( nVal > 32767 )
                      nVal = 32767;
                  *static_cast<GInt16 *>(pDstWord) = nVal;
                  continue;
                case GDT_Int32:
                  if( nVal > 2147483647UL )
                      nVal = 2147483647UL;
                  *static_cast<GInt32 *>(pDstWord) = nVal;
                  continue;
                case GDT_CInt16:
                {
                    GInt16 *panDstWord = static_cast<GInt16 *>(pDstWord);
                    if( nVal > 32767 )
                        nVal = 32767;
                    panDstWord[0] = nVal;
                    panDstWord[1] = 0;
                    continue;
                }
                case GDT_CInt32:
                {
                    GInt32 *panDstWord = static_cast<GInt32 *>(pDstWord);
                    if( nVal > 2147483647UL )
                        nVal = 2147483647UL;
                    panDstWord[0] = nVal;
                    panDstWord[1] = 0;
                    continue;
                }
                default:
                  break;
              }
              dfPixelValue = nVal;
          }
          break;
          
          case GDT_CInt16:
          {
              GInt16 *panSrcWord = static_cast<GInt16 *>(pSrcWord);
              GInt16 nVal = panSrcWord[0];
              switch( eDstType )
              {
                case GDT_Byte:
                {
                    GByte byVal;
                    if( nVal > 255 )
                        byVal = 255;
                    else if (nVal < 0)
                        byVal = 0;
                    else
                        byVal = static_cast<GByte>(nVal);
                    *static_cast<GByte *>(pDstWord) = byVal;
                    continue;
                }
                case GDT_Int16:
                  *static_cast<GInt16 *>(pDstWord) = nVal;
                  continue;
                case GDT_UInt16:
                  if( nVal < 0 )
                      nVal = 0;
                  *static_cast<GUInt16 *>(pDstWord) = nVal;
                  continue;
                case GDT_UInt32:
                  if( nVal < 0 )
                      nVal = 0;
                  *static_cast<GUInt32 *>(pDstWord) = nVal;
                  continue;
                case GDT_Int32:
                  *static_cast<GInt32 *>(pDstWord) = nVal;
                  continue;
                case GDT_CInt32:
                {
                    GInt32 *panDstWord = static_cast<GInt32 *>(pDstWord);
                    panDstWord[0] = panSrcWord[0];
                    panDstWord[1] = panSrcWord[1];
                    continue;
                }
                default:
                  break;
              }
              dfPixelValue = panSrcWord[0];
              dfPixelValueI = panSrcWord[1];
          }
          break;
          
          case GDT_CInt32:
          {
              GInt32 *panSrcWord = static_cast<GInt32 *>(pSrcWord);
              GInt32 nVal = panSrcWord[0];
              switch( eDstType )
              {
                case GDT_Byte:
                {
                    GByte byVal;
                    if( nVal > 255 )
                        byVal = 255;
                    else if (nVal < 0)
                        byVal = 0;
                    else
                        byVal = nVal;
                    *static_cast<GByte *>(pDstWord) = byVal;
                    continue;
                }
                case GDT_Int16:
                  if( nVal > 32767 )
                      nVal = 32767;
                  else if( nVal < -32768)
                      nVal = -32768;
                  *static_cast<GInt16 *>(pDstWord) = nVal;
                  continue;
                case GDT_UInt16:
                  if( nVal > 65535 )
                      nVal = 65535;
                  else if( nVal < 0 )
                      nVal = 0;
                  *static_cast<GUInt16 *>(pDstWord) = nVal;
                  continue;
                case GDT_UInt32:
                  if( nVal < 0 )
                      nVal = 0;
                  *static_cast<GUInt32 *>(pDstWord) = nVal;
                  continue;
                case GDT_Int32:
                  *static_cast<GInt32 *>(pDstWord) = nVal;
                  continue;
                case GDT_CInt16:
                {
                    GInt16 *panDstWord = static_cast<GInt16 *>(pDstWord);
                    if( nVal > 32767 )
                        nVal = 32767;
                    else if( nVal < -32768)
                        nVal = -32768;
                    panDstWord[0] = nVal;
                    nVal = panSrcWord[1];
                    if( nVal > 32767 )
                        nVal = 32767;
                    else if( nVal < -32768)
                        nVal = -32768;
                    panDstWord[1] = nVal;
                    continue;
                }
                default:
                  break;
              }
              dfPixelValue = panSrcWord[0];
              dfPixelValueI = panSrcWord[1];
          }
          break;

          case GDT_Float32:
          {
              float fVal = *static_cast<float *>(pSrcWord);
              dfPixelValue = fVal;
          }
          break;
          
          case GDT_Float64:
          {
              dfPixelValue = *static_cast<double *>(pSrcWord);
          }
          break;

          case GDT_CFloat32:
          {
              float *pafSrcWord = static_cast<float *>(pSrcWord);
              dfPixelValue = pafSrcWord[0];
              dfPixelValueI = pafSrcWord[1];
          }
          break;
          
          case GDT_CFloat64:
          {
              double *padfSrcWord = static_cast<double *>(pSrcWord);
              dfPixelValue = padfSrcWord[0];
              dfPixelValueI = padfSrcWord[1];
          }
          break;

          default:
            CPLAssert( FALSE );
        }
        
/* -------------------------------------------------------------------- */
/*      Set the destination pixel, doing range clipping as needed.      */
/* -------------------------------------------------------------------- */
        switch( eDstType )
        {
          case GDT_Byte:
          {
              GByte *pabyDstWord = static_cast<GByte *>(pDstWord);

              dfPixelValue += (float) 0.5;

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

              *static_cast<GUInt16 *>(pDstWord) = nVal;
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

              *static_cast<GInt16 *>(pDstWord) = nVal;
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

              *static_cast<GUInt32 *>(pDstWord) = nVal;
          }
          break;
          
          case GDT_Int32:
          {
              GInt32    nVal;
              
              dfPixelValue += 0.5;

              if( dfPixelValue < -2147483648.0 )
                  nVal = INT_MIN;
              else if( dfPixelValue > 2147483647 )
                  nVal = 2147483647;
              else
                  nVal = (GInt32) floor(dfPixelValue);

              *static_cast<GInt32 *>(pDstWord) = nVal;
          }
          break;

          case GDT_Float32:
          {
              *static_cast<float *>(pDstWord) = static_cast<float>(dfPixelValue);
          }
          break;

          case GDT_Float64:
            *static_cast<double *>(pDstWord) = dfPixelValue;
            break;
              
          case GDT_CInt16:
          {
              GInt16    nVal;
              GInt16 *panDstWord = static_cast<GInt16 *>(pDstWord);
              
              dfPixelValue += 0.5;
              dfPixelValueI += 0.5;

              if( dfPixelValue < -32768 )
                  nVal = -32768;
              else if( dfPixelValue > 32767 )
                  nVal = 32767;
              else
                  nVal = (GInt16) floor(dfPixelValue);
              panDstWord[0] = nVal;

              if( dfPixelValueI < -32768 )
                  nVal = -32768;
              else if( dfPixelValueI > 32767 )
                  nVal = 32767;
              else
                  nVal = (GInt16) floor(dfPixelValueI);
              panDstWord[1] = nVal;
          }
          break;
          
          case GDT_CInt32:
          {
              GInt32    nVal;
              GInt32 *panDstWord = static_cast<GInt32 *>(pDstWord);
              
              dfPixelValue += 0.5;
              dfPixelValueI += 0.5;

              if( dfPixelValue < -2147483648.0 )
                  nVal = INT_MIN;
              else if( dfPixelValue > 2147483647 )
                  nVal = 2147483647;
              else
                  nVal = (GInt32) floor(dfPixelValue);

              panDstWord[0] = nVal;

              if( dfPixelValueI < -2147483648.0 )
                  nVal = INT_MIN;
              else if( dfPixelValueI > 2147483647 )
                  nVal = 2147483647;
              else
                  nVal = (GInt32) floor(dfPixelValueI);

              panDstWord[1] = nVal;
          }
          break;

          case GDT_CFloat32:
          {
              float *pafDstWord = static_cast<float *>(pDstWord);
              pafDstWord[0] = static_cast<float>(dfPixelValue);
              pafDstWord[1] = static_cast<float>(dfPixelValueI);
          }
          break;

          case GDT_CFloat64:
          {
              double *padfDstWord = static_cast<double *>(pDstWord);
              padfDstWord[0] = dfPixelValue;
              padfDstWord[1] = dfPixelValueI;
          }
          break;
              
          default:
            CPLAssert( FALSE );
        }
    } /* next iWord */
#endif // defined USE_NEW_COPYWORDS
}

/************************************************************************/
/*                            GDALCopyBits()                            */
/************************************************************************/

/**
 * Bitwise word copying.
 *
 * A function for moving sets of partial bytes around.  Loosely
 * speaking this is a bitswise analog to GDALCopyWords().
 *
 * It copies nStepCount "words" where each word is nBitCount bits long. 
 * The nSrcStep and nDstStep are the number of bits from the start of one
 * word to the next (same as nBitCount if they are packed).  The nSrcOffset
 * and nDstOffset are the offset into the source and destination buffers
 * to start at, also measured in bits. 
 *
 * All bit offsets are assumed to start from the high order bit in a byte
 * (ie. most significant bit first).  Currently this function is not very
 * optimized, but it may be improved for some common cases in the future 
 * as needed. 
 *
 * @param pabySrcData the source data buffer.
 * @param nSrcOffset the offset (in bits) in pabySrcData to the start of the 
 * first word to copy.
 * @param nSrcStep the offset in bits from the start one source word to the 
 * start of the next. 
 * @param pabyDstData the destination data buffer.
 * @param nDstOffset the offset (in bits) in pabyDstData to the start of the 
 * first word to copy over.
 * @param nDstStep the offset in bits from the start one word to the 
 * start of the next. 
 * @param nBitCount the number of bits in a word to be copied.
 * @param nStepCount the number of words to copy.
 */

void GDALCopyBits( const GByte *pabySrcData, int nSrcOffset, int nSrcStep, 
                   GByte *pabyDstData, int nDstOffset, int nDstStep,
                   int nBitCount, int nStepCount )

{
    VALIDATE_POINTER0( pabySrcData, "GDALCopyBits" );

    int iStep;
    int iBit;

    for( iStep = 0; iStep < nStepCount; iStep++ )
    {
        for( iBit = 0; iBit < nBitCount; iBit++ )
        {
            if( pabySrcData[nSrcOffset>>3] 
                & (0x80 >>(nSrcOffset & 7)) )
                pabyDstData[nDstOffset>>3] |= (0x80 >> (nDstOffset & 7));
            else
                pabyDstData[nDstOffset>>3] &= ~(0x80 >> (nDstOffset & 7));


            nSrcOffset++;
            nDstOffset++;
        } 

        nSrcOffset += (nSrcStep - nBitCount);
        nDstOffset += (nDstStep - nBitCount);
    }
}

/************************************************************************/
/*                    GDALGetBestOverviewLevel()                        */
/*                                                                      */
/* Returns the best overview level to satisfy the query or -1 if none   */
/* Also updates nXOff, nYOff, nXSize, nYSize and psExtraArg when        */
/* returning a valid overview level                                     */
/************************************************************************/

int GDALBandGetBestOverviewLevel(GDALRasterBand* poBand,
                                 int &nXOff, int &nYOff,
                                 int &nXSize, int &nYSize,
                                 int nBufXSize, int nBufYSize )
{
    return GDALBandGetBestOverviewLevel2(poBand, nXOff, nYOff, nXSize, nYSize,
                                        nBufXSize, nBufYSize, NULL);
}

int GDALBandGetBestOverviewLevel2(GDALRasterBand* poBand,
                                  int &nXOff, int &nYOff,
                                  int &nXSize, int &nYSize,
                                  int nBufXSize, int nBufYSize,
                                  GDALRasterIOExtraArg* psExtraArg )
{
    double dfDesiredResolution;
/* -------------------------------------------------------------------- */
/*      Compute the desired resolution.  The resolution is              */
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
    int nOverviewCount = poBand->GetOverviewCount();
    GDALRasterBand* poBestOverview = NULL;
    double dfBestResolution = 0;
    int nBestOverviewLevel = -1;
    
    for( int iOverview = 0; iOverview < nOverviewCount; iOverview++ )
    {
        GDALRasterBand  *poOverview = poBand->GetOverview( iOverview );
        if (poOverview == NULL)
            continue;

        double          dfResolution;

        // What resolution is this?
        if( (poBand->GetXSize() / (double) poOverview->GetXSize())
            < (poBand->GetYSize() / (double) poOverview->GetYSize()) )
            dfResolution = 
                poBand->GetXSize() / (double) poOverview->GetXSize();
        else
            dfResolution = 
                poBand->GetYSize() / (double) poOverview->GetYSize();

        // Is it nearly the requested resolution and better (lower) than
        // the current best resolution?
        if( dfResolution >= dfDesiredResolution * 1.2 
            || dfResolution <= dfBestResolution )
            continue;

        // Ignore AVERAGE_BIT2GRAYSCALE overviews for RasterIO purposes.
        const char *pszResampling = 
            poOverview->GetMetadataItem( "RESAMPLING" );

        if( pszResampling != NULL && EQUALN(pszResampling,"AVERAGE_BIT2",12))
            continue;

        // OK, this is our new best overview.
        poBestOverview = poOverview;
        nBestOverviewLevel = iOverview;
        dfBestResolution = dfResolution;
    }

/* -------------------------------------------------------------------- */
/*      If we didn't find an overview that helps us, just return        */
/*      indicating failure and the full resolution image will be used.  */
/* -------------------------------------------------------------------- */
    if( nBestOverviewLevel < 0 )
        return -1;

/* -------------------------------------------------------------------- */
/*      Recompute the source window in terms of the selected            */
/*      overview.                                                       */
/* -------------------------------------------------------------------- */
    int         nOXOff, nOYOff, nOXSize, nOYSize;
    double      dfXRes, dfYRes;
    
    dfXRes = poBand->GetXSize() / (double) poBestOverview->GetXSize();
    dfYRes = poBand->GetYSize() / (double) poBestOverview->GetYSize();

    nOXOff = MIN(poBestOverview->GetXSize()-1,(int) (nXOff/dfXRes+0.5));
    nOYOff = MIN(poBestOverview->GetYSize()-1,(int) (nYOff/dfYRes+0.5));
    nOXSize = MAX(1,(int) (nXSize/dfXRes + 0.5));
    nOYSize = MAX(1,(int) (nYSize/dfYRes + 0.5));
    if( nOXOff + nOXSize > poBestOverview->GetXSize() )
        nOXSize = poBestOverview->GetXSize() - nOXOff;
    if( nOYOff + nOYSize > poBestOverview->GetYSize() )
        nOYSize = poBestOverview->GetYSize() - nOYOff;
        
    nXOff = nOXOff;
    nYOff = nOYOff;
    nXSize = nOXSize;
    nYSize = nOYSize;
    
    if( psExtraArg && psExtraArg->bFloatingPointWindowValidity )
    {
        psExtraArg->dfXOff /= dfXRes;
        psExtraArg->dfXSize /= dfXRes;
        psExtraArg->dfYOff /= dfYRes;
        psExtraArg->dfYSize /= dfYRes;
    }
    
    return nBestOverviewLevel;
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
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GDALRasterIOExtraArg* psExtraArg )


{
    int         nOverview;
    GDALRasterIOExtraArg sExtraArg;
    
    GDALCopyRasterIOExtraArg(&sExtraArg, psExtraArg);

    nOverview =
        GDALBandGetBestOverviewLevel2(this, nXOff, nYOff, nXSize, nYSize,
                                      nBufXSize, nBufYSize, &sExtraArg);
    if (nOverview < 0)
        return CE_Failure;
        
/* -------------------------------------------------------------------- */
/*      Recast the call in terms of the new raster layer.               */
/* -------------------------------------------------------------------- */
    GDALRasterBand* poOverviewBand = GetOverview(nOverview);
    if (poOverviewBand == NULL)
        return CE_Failure;

    return poOverviewBand->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nPixelSpace, nLineSpace, &sExtraArg );
}

/************************************************************************/
/*                        GetBestOverviewLevel()                        */
/*                                                                      */
/* Returns the best overview level to satisfy the query or -1 if none   */
/* Also updates nXOff, nYOff, nXSize, nYSize when returning a valid     */
/* overview level                                                       */
/************************************************************************/

static
int GDALDatasetGetBestOverviewLevel(GDALDataset* poDS,
                                    int &nXOff, int &nYOff,
                                    int &nXSize, int &nYSize,
                                    int nBufXSize, int nBufYSize,
                                    int nBandCount, int *panBandMap)
{
    int iBand, iOverview;
    int nOverviewCount = 0;
    GDALRasterBand *poFirstBand = NULL;
    
    if (nBandCount == 0)
        return -1;
    
/* -------------------------------------------------------------------- */
/* Check that all bands have the same number of overviews and           */
/* that they have all the same size and block dimensions                */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBand *poBand = poDS->GetRasterBand( panBandMap[iBand] );
        if (iBand == 0)
        {
            poFirstBand = poBand;
            nOverviewCount = poBand->GetOverviewCount();
        }
        else if (nOverviewCount != poBand->GetOverviewCount())
        {
            CPLDebug( "GDAL", 
                      "GDALDataset::GetBestOverviewLevel() ... "
                      "mismatched overview count, use std method." );
            return -1;
        }
        else
        {
            for(iOverview = 0; iOverview < nOverviewCount; iOverview++)
            {
                GDALRasterBand* poOvrBand =
                    poBand->GetOverview(iOverview);
                GDALRasterBand* poOvrFirstBand =
                    poFirstBand->GetOverview(iOverview);
                if ( poOvrBand == NULL || poOvrFirstBand == NULL)
                    continue;

                if ( poOvrFirstBand->GetXSize() != poOvrBand->GetXSize() ||
                     poOvrFirstBand->GetYSize() != poOvrBand->GetYSize() )
                {
                    CPLDebug( "GDAL", 
                              "GDALDataset::GetBestOverviewLevel() ... "
                              "mismatched overview sizes, use std method." );
                    return -1;
                }
                int nBlockXSizeFirst=0, nBlockYSizeFirst=0;
                int nBlockXSizeCurrent=0, nBlockYSizeCurrent=0;
                poOvrFirstBand->GetBlockSize(&nBlockXSizeFirst, &nBlockYSizeFirst);
                poOvrBand->GetBlockSize(&nBlockXSizeCurrent, &nBlockYSizeCurrent);
                if (nBlockXSizeFirst != nBlockXSizeCurrent ||
                    nBlockYSizeFirst != nBlockYSizeCurrent)
                {
                    CPLDebug( "GDAL", 
                          "GDALDataset::GetBestOverviewLevel() ... "
                          "mismatched block sizes, use std method." );
                    return -1;
                }
            }
        }
    }
 
    return GDALBandGetBestOverviewLevel2(poFirstBand,
                                        nXOff, nYOff, nXSize, nYSize,
                                        nBufXSize, nBufYSize, NULL);
}

/************************************************************************/
/*                         BlockBasedRasterIO()                         */
/*                                                                      */
/*      This convenience function implements a dataset level            */
/*      RasterIO() interface based on calling down to fetch blocks,     */
/*      much like the GDALRasterBand::IRasterIO(), but it handles       */
/*      all bands at once, so that a format driver that handles a       */
/*      request for different bands of the same block efficiently       */
/*      (ie. without re-reading interleaved data) will efficiently.     */
/*                                                                      */
/*      This method is intended to be called by an overridden           */
/*      IRasterIO() method in the driver specific GDALDataset           */
/*      derived class.                                                  */
/*                                                                      */
/*      Default internal implementation of RasterIO() ... utilizes      */
/*      the Block access methods to satisfy the request.  This would    */
/*      normally only be overridden by formats with overviews.          */
/*                                                                      */
/*      To keep things relatively simple, this method does not          */
/*      currently take advantage of some special cases addressed in     */
/*      GDALRasterBand::IRasterIO(), so it is likely best to only       */
/*      call it when you know it will help.  That is in cases where     */
/*      data is at 1:1 to the buffer, and you know the driver is        */
/*      implementing interleaved IO efficiently on a block by block     */
/*      basis. Overviews will be used when possible.                    */
/************************************************************************/

CPLErr 
GDALDataset::BlockBasedRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 int nBandCount, int *panBandMap,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GSpacing nBandSpace,
                                 GDALRasterIOExtraArg* psExtraArg )
    
{
    GByte      **papabySrcBlock = NULL;
    GDALRasterBlock *poBlock = NULL;
    GDALRasterBlock **papoBlocks = NULL;
    int         nLBlockX=-1, nLBlockY=-1, iBufYOff, iBufXOff, iSrcY, iBand;
    int         nBlockXSize=1, nBlockYSize=1;
    CPLErr      eErr = CE_None;
    GDALDataType eDataType = GDT_Byte;

    CPLAssert( NULL != pData );

/* -------------------------------------------------------------------- */
/*      Ensure that all bands share a common block size and data type.  */
/* -------------------------------------------------------------------- */

    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBand *poBand = GetRasterBand( panBandMap[iBand] );
        int nThisBlockXSize, nThisBlockYSize;

        if( iBand == 0 )
        {
            poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
            eDataType = poBand->GetRasterDataType();
        }
        else
        {
            poBand->GetBlockSize( &nThisBlockXSize, &nThisBlockYSize );
            if( nThisBlockXSize != nBlockXSize 
                || nThisBlockYSize != nBlockYSize )
            {
                CPLDebug( "GDAL", 
                          "GDALDataset::BlockBasedRasterIO() ... "
                          "mismatched block sizes, use std method." );
                return GDALDataset::IRasterIO( eRWFlag, 
                                               nXOff, nYOff, nXSize, nYSize, 
                                               pData, nBufXSize, nBufYSize, 
                                               eBufType, 
                                               nBandCount, panBandMap,
                                               nPixelSpace, nLineSpace, 
                                               nBandSpace, psExtraArg );
            }

            if( eDataType != poBand->GetRasterDataType() 
                && (nXSize != nBufXSize || nYSize != nBufYSize) )
            {
                CPLDebug( "GDAL", 
                          "GDALDataset::BlockBasedRasterIO() ... "
                          "mismatched band data types, use std method." );
                return GDALDataset::IRasterIO( eRWFlag, 
                                               nXOff, nYOff, nXSize, nYSize, 
                                               pData, nBufXSize, nBufYSize, 
                                               eBufType, 
                                               nBandCount, panBandMap,
                                               nPixelSpace, nLineSpace, 
                                               nBandSpace, psExtraArg );
            }
        }
    }

/* ==================================================================== */
/*      In this special case at full resolution we step through in      */
/*      blocks, turning the request over to the per-band                */
/*      IRasterIO(), but ensuring that all bands of one block are       */
/*      called before proceeding to the next.                           */
/* ==================================================================== */

    if( nXSize == nBufXSize && nYSize == nBufYSize )    
    {
        GDALRasterIOExtraArg sDummyExtraArg;
        INIT_RASTERIO_EXTRA_ARG(sDummyExtraArg);

        int nChunkYSize, nChunkXSize, nChunkXOff, nChunkYOff;

        for( iBufYOff = 0; iBufYOff < nBufYSize; iBufYOff += nChunkYSize )
        {
            nChunkYSize = nBlockYSize;
            nChunkYOff = iBufYOff + nYOff;
            nChunkYSize = nBlockYSize - (nChunkYOff % nBlockYSize);
            if( nChunkYSize == 0 )
                nChunkYSize = nBlockYSize;
            if( nChunkYOff + nChunkYSize > nYOff + nYSize )
                nChunkYSize = (nYOff + nYSize) - nChunkYOff;

            for( iBufXOff = 0; iBufXOff < nBufXSize; iBufXOff += nChunkXSize )
            {
                nChunkXSize = nBlockXSize;
                nChunkXOff = iBufXOff + nXOff;
                nChunkXSize = nBlockXSize - (nChunkXOff % nBlockXSize);
                if( nChunkXSize == 0 )
                    nChunkXSize = nBlockXSize;
                if( nChunkXOff + nChunkXSize > nXOff + nXSize )
                    nChunkXSize = (nXOff + nXSize) - nChunkXOff;

                GByte *pabyChunkData;

                pabyChunkData = ((GByte *) pData) 
                    + iBufXOff * nPixelSpace 
                    + (GPtrDiff_t)iBufYOff * nLineSpace;

                for( iBand = 0; iBand < nBandCount; iBand++ )
                {
                    GDALRasterBand *poBand = GetRasterBand(panBandMap[iBand]);
                    
                    eErr = 
                        poBand->GDALRasterBand::IRasterIO( 
                            eRWFlag, nChunkXOff, nChunkYOff, 
                            nChunkXSize, nChunkYSize, 
                            pabyChunkData + (GPtrDiff_t)iBand * nBandSpace, 
                            nChunkXSize, nChunkYSize, eBufType, 
                            nPixelSpace, nLineSpace, &sDummyExtraArg );
                    if( eErr != CE_None )
                        return eErr;
                }
            }

            if( psExtraArg->pfnProgress != NULL &&
                !psExtraArg->pfnProgress(1.0 * MAX(nBufYSize,iBufYOff + nChunkYSize) / nBufYSize, "", psExtraArg->pProgressData) )
            {
                return CE_Failure;
            }
        }

        return CE_None;
    }
    
    /* Below code is not compatible with that case. It would need a complete */
    /* separate code like done in GDALRasterBand::IRasterIO. */
    if (eRWFlag == GF_Write && (nBufXSize < nXSize || nBufYSize < nYSize))
    {
        return GDALDataset::IRasterIO( eRWFlag, 
                                       nXOff, nYOff, nXSize, nYSize, 
                                       pData, nBufXSize, nBufYSize, 
                                       eBufType, 
                                       nBandCount, panBandMap,
                                       nPixelSpace, nLineSpace, 
                                       nBandSpace, psExtraArg );
    }

    /* We could have a smarter implementation, but that will do for now */
    if( psExtraArg->eResampleAlg != GRIORA_NearestNeighbour &&
        (nBufXSize != nXSize || nBufYSize != nYSize) )
    {
        return GDALDataset::IRasterIO( eRWFlag, 
                                       nXOff, nYOff, nXSize, nYSize, 
                                       pData, nBufXSize, nBufYSize, 
                                       eBufType, 
                                       nBandCount, panBandMap,
                                       nPixelSpace, nLineSpace, 
                                       nBandSpace, psExtraArg );
    }

/* ==================================================================== */
/*      Loop reading required source blocks to satisfy output           */
/*      request.  This is the most general implementation.              */
/* ==================================================================== */

    int         nBandDataSize = GDALGetDataTypeSize( eDataType ) / 8;

    papabySrcBlock = (GByte **) CPLCalloc(sizeof(GByte*),nBandCount);
    papoBlocks = (GDALRasterBlock **) CPLCalloc(sizeof(void*),nBandCount);
    
/* -------------------------------------------------------------------- */
/*      Select an overview level if appropriate.                        */
/* -------------------------------------------------------------------- */
    int nOverviewLevel = GDALDatasetGetBestOverviewLevel (this,
                                               nXOff, nYOff, nXSize, nYSize,
                                               nBufXSize, nBufYSize,
                                               nBandCount, panBandMap);
    if (nOverviewLevel >= 0)
    {
        GetRasterBand(panBandMap[0])->GetOverview(nOverviewLevel)->
                                GetBlockSize( &nBlockXSize, &nBlockYSize );
    }

/* -------------------------------------------------------------------- */
/*      Compute stepping increment.                                     */
/* -------------------------------------------------------------------- */
    double      dfSrcX, dfSrcY, dfSrcXInc, dfSrcYInc;
    
    dfSrcXInc = nXSize / (double) nBufXSize;
    dfSrcYInc = nYSize / (double) nBufYSize;

/* -------------------------------------------------------------------- */
/*      Loop over buffer computing source locations.                    */
/* -------------------------------------------------------------------- */
    for( iBufYOff = 0; iBufYOff < nBufYSize; iBufYOff++ )
    {
        GPtrDiff_t  iBufOffset;
        int         iSrcOffset;
        
        dfSrcY = (iBufYOff+0.5) * dfSrcYInc + nYOff;
        iSrcY = (int) dfSrcY;

        iBufOffset = (GPtrDiff_t)iBufYOff * (GPtrDiff_t)nLineSpace;
        
        for( iBufXOff = 0; iBufXOff < nBufXSize; iBufXOff++ )
        {
            int iSrcX;

            dfSrcX = (iBufXOff+0.5) * dfSrcXInc + nXOff;
            
            iSrcX = (int) dfSrcX;

            // FIXME: this code likely doesn't work if the dirty block gets flushed
            // to disk before being completely written.
            // In the meantime, bJustInitalize should probably be set to FALSE
            // even if it is not ideal performance wise, and for lossy compression

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

                int bJustInitialize = 
                    eRWFlag == GF_Write
                    && nYOff <= nLBlockY * nBlockYSize
                    && nYOff + nYSize >= (nLBlockY+1) * nBlockYSize
                    && nXOff <= nLBlockX * nBlockXSize
                    && nXOff + nXSize >= (nLBlockX+1) * nBlockXSize;
                /*int bMemZeroBuffer = FALSE;
                if( eRWFlag == GF_Write && !bJustInitialize &&
                    nXOff <= nLBlockX * nBlockXSize &&
                    nYOff <= nLBlockY * nBlockYSize &&
                    (nXOff + nXSize >= (nLBlockX+1) * nBlockXSize ||
                     (nXOff + nXSize == GetRasterXSize() && (nLBlockX+1) * nBlockXSize > GetRasterXSize())) &&
                    (nYOff + nYSize >= (nLBlockY+1) * nBlockYSize ||
                     (nYOff + nYSize == GetRasterYSize() && (nLBlockY+1) * nBlockYSize > GetRasterYSize())) )
                {
                    bJustInitialize = TRUE;
                    bMemZeroBuffer = TRUE;
                }*/
                for( iBand = 0; iBand < nBandCount; iBand++ )
                {
                    GDALRasterBand *poBand = GetRasterBand( panBandMap[iBand]);
                    if (nOverviewLevel >= 0)
                        poBand = poBand->GetOverview(nOverviewLevel);
                    poBlock = poBand->GetLockedBlockRef( nLBlockX, nLBlockY, 
                                                         bJustInitialize );
                    if( poBlock == NULL )
                    {
                        eErr = CE_Failure;
                        goto CleanupAndReturn;
                    }

                    if( eRWFlag == GF_Write )
                        poBlock->MarkDirty();

                    if( papoBlocks[iBand] != NULL )
                        papoBlocks[iBand]->DropLock();
                
                    papoBlocks[iBand] = poBlock;

                    papabySrcBlock[iBand] = (GByte *) poBlock->GetDataRef();
                    if( papabySrcBlock[iBand] == NULL )
                    {
                        eErr = CE_Failure; 
                        goto CleanupAndReturn;
                    }
                    /*if( bMemZeroBuffer )
                    {
                        memset(papabySrcBlock[iBand], 0,
                            nBandDataSize * nBlockXSize * nBlockYSize);
                    }*/
                }
            }

/* -------------------------------------------------------------------- */
/*      Copy over this pixel of data.                                   */
/* -------------------------------------------------------------------- */
            iSrcOffset = ((GPtrDiff_t)iSrcX - (GPtrDiff_t)nLBlockX*nBlockXSize
                + ((GPtrDiff_t)iSrcY - (GPtrDiff_t)nLBlockY*nBlockYSize) * nBlockXSize)*nBandDataSize;

            for( iBand = 0; iBand < nBandCount; iBand++ )
            {
                GByte *pabySrcBlock = papabySrcBlock[iBand];
                GPtrDiff_t iBandBufOffset = iBufOffset + (GPtrDiff_t)iBand * (GPtrDiff_t)nBandSpace;
                
                if( eDataType == eBufType )
                {
                    if( eRWFlag == GF_Read )
                        memcpy( ((GByte *) pData) + iBandBufOffset,
                                pabySrcBlock + iSrcOffset, nBandDataSize );
                else
                    memcpy( pabySrcBlock + iSrcOffset, 
                            ((GByte *)pData) + iBandBufOffset, nBandDataSize );
                }
                else
                {
                    /* type to type conversion ... ouch, this is expensive way
                       of handling single words */
                    
                    if( eRWFlag == GF_Read )
                        GDALCopyWords( pabySrcBlock + iSrcOffset, eDataType, 0,
                                       ((GByte *) pData) + iBandBufOffset, 
                                       eBufType, 0, 1 );
                    else
                        GDALCopyWords( ((GByte *) pData) + iBandBufOffset, 
                                       eBufType, 0,
                                       pabySrcBlock + iSrcOffset, eDataType, 0,
                                       1 );
                }
            }

            iBufOffset += (int)nPixelSpace;
        }
    }

/* -------------------------------------------------------------------- */
/*      CleanupAndReturn.                                               */
/* -------------------------------------------------------------------- */
  CleanupAndReturn:
    CPLFree( papabySrcBlock );
    if( papoBlocks != NULL )
    {
        for( iBand = 0; iBand < nBandCount; iBand++ )
        {
            if( papoBlocks[iBand] != NULL )
                papoBlocks[iBand]->DropLock();
        }
        CPLFree( papoBlocks );
    }

    return( CE_None );
}

/************************************************************************/
/*                  GDALCopyWholeRasterGetSwathSize()                   */
/************************************************************************/

static void GDALCopyWholeRasterGetSwathSize(GDALRasterBand *poSrcPrototypeBand,
                                            GDALRasterBand *poDstPrototypeBand,
                                            int nBandCount,
                                            int bDstIsCompressed, int bInterleave,
                                            int* pnSwathCols, int *pnSwathLines)
{
    GDALDataType eDT = poDstPrototypeBand->GetRasterDataType();
    int nSrcBlockXSize, nSrcBlockYSize;
    int nBlockXSize, nBlockYSize;

    int nXSize = poSrcPrototypeBand->GetXSize();
    int nYSize = poSrcPrototypeBand->GetYSize();

    poSrcPrototypeBand->GetBlockSize( &nSrcBlockXSize, &nSrcBlockYSize );
    poDstPrototypeBand->GetBlockSize( &nBlockXSize, &nBlockYSize );

    int nMaxBlockXSize = MAX(nBlockXSize, nSrcBlockXSize);
    int nMaxBlockYSize = MAX(nBlockYSize, nSrcBlockYSize);

    int nPixelSize = (GDALGetDataTypeSize(eDT) / 8);
    if( bInterleave)
        nPixelSize *= nBandCount;

    // aim for one row of blocks.  Do not settle for less.
    int nSwathCols  = nXSize;
    int nSwathLines = nBlockYSize;

/* -------------------------------------------------------------------- */
/*      What will our swath size be?                                    */
/* -------------------------------------------------------------------- */
    /* When writing interleaved data in a compressed format, we want to be sure */
    /* that each block will only be written once, so the swath size must not be */
    /* greater than the block cache. */
    const char* pszSwathSize = CPLGetConfigOption("GDAL_SWATH_SIZE", NULL);
    int nTargetSwathSize;
    if( pszSwathSize != NULL )
        nTargetSwathSize = atoi(pszSwathSize);
    else
    {
        /* As a default, take one 1/4 of the cache size */
        nTargetSwathSize = (int)MIN(INT_MAX, GDALGetCacheMax64() / 4);

        /* but if the minimum idal swath buf size is less, then go for it to */
        /* avoid unnecessarily abusing RAM usage */
        GIntBig nIdealSwathBufSize = (GIntBig)nSwathCols * nSwathLines * nPixelSize;
        if( nTargetSwathSize > nIdealSwathBufSize )
            nTargetSwathSize = (int)nIdealSwathBufSize;
    }

    if (nTargetSwathSize < 1000000)
        nTargetSwathSize = 1000000;

    /* But let's check that  */
    if (bDstIsCompressed && bInterleave && nTargetSwathSize > GDALGetCacheMax64())
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "When translating into a compressed interleave format, the block cache size (" CPL_FRMT_GIB ") "
                 "should be at least the size of the swath (%d) (GDAL_SWATH_SIZE config. option)", GDALGetCacheMax64(), nTargetSwathSize);
    }

#define IS_MULTIPLE_OF(x,y) ((y)%(x) == 0)
#define ROUND_TO(x,y) (((x)/(y))*(y))

    /* if both input and output datasets are tiled, that the tile dimensions */
    /* are "compatible", try to stick  to a swath dimension that is a multiple */
    /* of input and output block dimensions */
    if (nBlockXSize != nXSize && nSrcBlockXSize != nXSize &&
        IS_MULTIPLE_OF(nBlockXSize, nMaxBlockXSize) &&
        IS_MULTIPLE_OF(nSrcBlockXSize, nMaxBlockXSize) &&
        IS_MULTIPLE_OF(nBlockYSize, nMaxBlockYSize) &&
        IS_MULTIPLE_OF(nSrcBlockYSize, nMaxBlockYSize))
    {
        if (((GIntBig)nMaxBlockXSize) * nMaxBlockYSize * nPixelSize <=
                                                    (GIntBig)nTargetSwathSize)
        {
            nSwathCols = nTargetSwathSize / (nMaxBlockYSize * nPixelSize);
            nSwathCols = ROUND_TO(nSwathCols, nMaxBlockXSize);
            if (nSwathCols == 0)
                nSwathCols = nMaxBlockXSize;
            if (nSwathCols > nXSize)
                nSwathCols = nXSize;
            nSwathLines = nMaxBlockYSize;

            if (((GIntBig)nSwathCols) * nSwathLines * nPixelSize >
                                                    (GIntBig)nTargetSwathSize)
            {
                nSwathCols  = nXSize;
                nSwathLines = nBlockYSize;
            }
        }
    }

    int nMemoryPerCol = nSwathCols * nPixelSize;

    /* Do the computation on a big int since for example when translating */
    /* the JPL WMS layer, we overflow 32 bits*/
    GIntBig nSwathBufSize = (GIntBig)nMemoryPerCol * nSwathLines;
    if (nSwathBufSize > (GIntBig)nTargetSwathSize)
    {
        nSwathLines = nTargetSwathSize / nMemoryPerCol;
        if (nSwathLines == 0)
            nSwathLines = 1;

        CPLDebug( "GDAL",
              "GDALCopyWholeRasterGetSwathSize(): adjusting to %d line swath "
              "since requirement (%d * %d bytes) exceed target swath size (%d bytes) (GDAL_SWATH_SIZE config. option)",
              nSwathLines, nBlockYSize, nMemoryPerCol, nTargetSwathSize);
    }
    // If we are processing single scans, try to handle several at once.
    // If we are handling swaths already, only grow the swath if a row
    // of blocks is substantially less than our target buffer size.
    else if( nSwathLines == 1
        || nMemoryPerCol * nSwathLines < nTargetSwathSize / 10 )
    {
        nSwathLines = MIN(nYSize,MAX(1,nTargetSwathSize/nMemoryPerCol));

        /* If possible try to align to source and target block height */
        if ((nSwathLines % nMaxBlockYSize) != 0 && nSwathLines > nMaxBlockYSize &&
            IS_MULTIPLE_OF(nBlockYSize, nMaxBlockYSize) &&
            IS_MULTIPLE_OF(nSrcBlockYSize, nMaxBlockYSize))
            nSwathLines = ROUND_TO(nSwathLines, nMaxBlockYSize);
    }


    if (bDstIsCompressed)
    {
        if (nSwathLines < nBlockYSize)
        {
            nSwathLines = nBlockYSize;

            /* Number of pixels that can be read/write simultaneously */
            nSwathCols = nTargetSwathSize / (nSwathLines * nPixelSize);
            nSwathCols = ROUND_TO(nSwathCols, nBlockXSize);
            if (nSwathCols == 0)
                nSwathCols = nBlockXSize;
            if (nSwathCols > nXSize)
                nSwathCols = nXSize;

            CPLDebug( "GDAL",
              "GDALCopyWholeRasterGetSwathSize(): because of compression and too high block,\n"
              "use partial width at one time");
        }
        else if ((nSwathLines % nBlockYSize) != 0)
        {
            /* Round on a multiple of nBlockYSize */
            nSwathLines = ROUND_TO(nSwathLines, nBlockYSize);
            CPLDebug( "GDAL",
              "GDALCopyWholeRasterGetSwathSize(): because of compression, \n"
              "round nSwathLines to block height : %d", nSwathLines);
        }
    }

    *pnSwathCols = nSwathCols;
    *pnSwathLines = nSwathLines;
}

/************************************************************************/
/*                     GDALDatasetCopyWholeRaster()                     */
/************************************************************************/

/**
 * \brief Copy all dataset raster data.
 *
 * This function copies the complete raster contents of one dataset to 
 * another similarly configured dataset.  The source and destination 
 * dataset must have the same number of bands, and the same width
 * and height.  The bands do not have to have the same data type. 
 *
 * This function is primarily intended to support implementation of 
 * driver specific CreateCopy() functions.  It implements efficient copying,
 * in particular "chunking" the copy in substantial blocks and, if appropriate,
 * performing the transfer in a pixel interleaved fashion.
 *
 * Currently the only papszOptions value supported are : "INTERLEAVE=PIXEL"
 * to force pixel interleaved operation and "COMPRESSED=YES" to force alignment
 * on target dataset block sizes to achieve best compression.  More options may be supported in
 * the future.  
 *
 * @param hSrcDS the source dataset
 * @param hDstDS the destination dataset
 * @param papszOptions transfer hints in "StringList" Name=Value format.
 * @param pfnProgress progress reporting function.
 * @param pProgressData callback data for progress function.
 *
 * @return CE_None on success, or CE_Failure on failure. 
 */

CPLErr CPL_STDCALL GDALDatasetCopyWholeRaster(
    GDALDatasetH hSrcDS, GDALDatasetH hDstDS, char **papszOptions, 
    GDALProgressFunc pfnProgress, void *pProgressData )

{
    VALIDATE_POINTER1( hSrcDS, "GDALDatasetCopyWholeRaster", CE_Failure );
    VALIDATE_POINTER1( hDstDS, "GDALDatasetCopyWholeRaster", CE_Failure );

    GDALDataset *poSrcDS = (GDALDataset *) hSrcDS;
    GDALDataset *poDstDS = (GDALDataset *) hDstDS;
    CPLErr eErr = CE_None;

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Confirm the datasets match in size and band counts.             */
/* -------------------------------------------------------------------- */
    int nXSize = poDstDS->GetRasterXSize(), 
        nYSize = poDstDS->GetRasterYSize(),
        nBandCount = poDstDS->GetRasterCount();

    if( poSrcDS->GetRasterXSize() != nXSize 
        || poSrcDS->GetRasterYSize() != nYSize
        || poSrcDS->GetRasterCount() != nBandCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Input and output dataset sizes or band counts do not\n"
                  "match in GDALDatasetCopyWholeRaster()" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Report preliminary (0) progress.                                */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, 
                  "User terminated CreateCopy()" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Get our prototype band, and assume the others are similarly     */
/*      configured.                                                     */
/* -------------------------------------------------------------------- */
    if( nBandCount == 0 )
        return CE_None;

    GDALRasterBand *poSrcPrototypeBand = poSrcDS->GetRasterBand(1);
    GDALRasterBand *poDstPrototypeBand = poDstDS->GetRasterBand(1);
    GDALDataType eDT = poDstPrototypeBand->GetRasterDataType();

/* -------------------------------------------------------------------- */
/*      Do we want to try and do the operation in a pixel               */
/*      interleaved fashion?                                            */
/* -------------------------------------------------------------------- */
    int bInterleave = FALSE;
    const char *pszInterleave = NULL;
    
    pszInterleave = poSrcDS->GetMetadataItem( "INTERLEAVE", "IMAGE_STRUCTURE");
    if( pszInterleave != NULL 
        && (EQUAL(pszInterleave,"PIXEL") || EQUAL(pszInterleave,"LINE")) )
        bInterleave = TRUE;

    pszInterleave = poDstDS->GetMetadataItem( "INTERLEAVE", "IMAGE_STRUCTURE");
    if( pszInterleave != NULL 
        && (EQUAL(pszInterleave,"PIXEL") || EQUAL(pszInterleave,"LINE")) )
        bInterleave = TRUE;

    pszInterleave = CSLFetchNameValue( papszOptions, "INTERLEAVE" );
    if( pszInterleave != NULL 
        && (EQUAL(pszInterleave,"PIXEL") || EQUAL(pszInterleave,"LINE")) )
        bInterleave = TRUE;

    /* If the destination is compressed, we must try to write blocks just once, to save */
    /* disk space (GTiff case for example), and to avoid data loss (JPEG compression for example) */
    int bDstIsCompressed = FALSE;
    const char* pszDstCompressed= CSLFetchNameValue( papszOptions, "COMPRESSED" );
    if (pszDstCompressed != NULL && CSLTestBoolean(pszDstCompressed))
        bDstIsCompressed = TRUE;

/* -------------------------------------------------------------------- */
/*      What will our swath size be?                                    */
/* -------------------------------------------------------------------- */

    int nSwathCols, nSwathLines;
    GDALCopyWholeRasterGetSwathSize(poSrcPrototypeBand,
                                    poDstPrototypeBand,
                                    nBandCount,
                                    bDstIsCompressed, bInterleave,
                                    &nSwathCols, &nSwathLines);

    int nPixelSize = (GDALGetDataTypeSize(eDT) / 8);
    if( bInterleave)
        nPixelSize *= nBandCount;

    void *pSwathBuf = VSIMalloc3(nSwathCols, nSwathLines, nPixelSize );
    if( pSwathBuf == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                "Failed to allocate %d*%d*%d byte swath buffer in\n"
                "GDALDatasetCopyWholeRaster()",
                nSwathCols, nSwathLines, nPixelSize );
        return CE_Failure;
    }

    CPLDebug( "GDAL", 
            "GDALDatasetCopyWholeRaster(): %d*%d swaths, bInterleave=%d", 
            nSwathCols, nSwathLines, bInterleave );

    if( nSwathCols == nXSize && poSrcDS->GetDriver() != NULL &&
        EQUAL(poSrcDS->GetDriver()->GetDescription(), "ECW") )
    {
        poSrcDS->AdviseRead(0, 0, nXSize, nYSize, nXSize, nYSize, eDT, nBandCount, NULL, NULL);
    }

/* ==================================================================== */
/*      Band oriented (uninterleaved) case.                             */
/* ==================================================================== */
    if( !bInterleave )
    {
        int iBand, iX, iY;

        GDALRasterIOExtraArg sExtraArg;
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);

        int nTotalBlocks = nBandCount *
                           ((nYSize + nSwathLines - 1) / nSwathLines) *
                           ((nXSize + nSwathCols - 1) / nSwathCols);
        int nBlocksDone = 0;

        for( iBand = 0; iBand < nBandCount && eErr == CE_None; iBand++ )
        {
            int nBand = iBand+1;

            for( iY = 0; iY < nYSize && eErr == CE_None; iY += nSwathLines )
            {
                int nThisLines = nSwathLines;

                if( iY + nThisLines > nYSize )
                    nThisLines = nYSize - iY;

                for( iX = 0; iX < nXSize && eErr == CE_None; iX += nSwathCols )
                {
                    int nThisCols = nSwathCols;

                    if( iX + nThisCols > nXSize )
                        nThisCols = nXSize - iX;

                    sExtraArg.pfnProgress = GDALScaledProgress;
                    sExtraArg.pProgressData = 
                        GDALCreateScaledProgress( nBlocksDone / (double)nTotalBlocks,
                                                (nBlocksDone + 0.5) / (double)nTotalBlocks,
                                                pfnProgress,
                                                pProgressData );

                    eErr = poSrcDS->RasterIO( GF_Read, 
                                            iX, iY, nThisCols, nThisLines,
                                            pSwathBuf, nThisCols, nThisLines, 
                                            eDT, 1, &nBand, 
                                            0, 0, 0, &sExtraArg );

                    GDALDestroyScaledProgress( sExtraArg.pProgressData );

                    if( eErr == CE_None )
                        eErr = poDstDS->RasterIO( GF_Write, 
                                                iX, iY, nThisCols, nThisLines,
                                                pSwathBuf, nThisCols, nThisLines, 
                                                eDT, 1, &nBand,
                                                0, 0, 0, NULL );
                    nBlocksDone ++;
                    if( eErr == CE_None 
                        && !pfnProgress( nBlocksDone / (double)nTotalBlocks,
                                        NULL, pProgressData ) )
                    {
                        eErr = CE_Failure;
                        CPLError( CE_Failure, CPLE_UserInterrupt, 
                                "User terminated CreateCopy()" );
                    }
                }
            }
        }
    }

/* ==================================================================== */
/*      Pixel interleaved case.                                         */
/* ==================================================================== */
    else /* if( bInterleave ) */
    {
        int iY, iX;

        GDALRasterIOExtraArg sExtraArg;
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);

        int nTotalBlocks = ((nYSize + nSwathLines - 1) / nSwathLines) *
                           ((nXSize + nSwathCols - 1) / nSwathCols);
        int nBlocksDone = 0;

        for( iY = 0; iY < nYSize && eErr == CE_None; iY += nSwathLines )
        {
            int nThisLines = nSwathLines;

            if( iY + nThisLines > nYSize )
                nThisLines = nYSize - iY;

            for( iX = 0; iX < nXSize && eErr == CE_None; iX += nSwathCols )
            {
                int nThisCols = nSwathCols;

                if( iX + nThisCols > nXSize )
                    nThisCols = nXSize - iX;

                sExtraArg.pfnProgress = GDALScaledProgress;
                sExtraArg.pProgressData = 
                    GDALCreateScaledProgress( nBlocksDone / (double)nTotalBlocks,
                                            (nBlocksDone + 0.5) / (double)nTotalBlocks,
                                            pfnProgress,
                                            pProgressData );

                eErr = poSrcDS->RasterIO( GF_Read, 
                                        iX, iY, nThisCols, nThisLines,
                                        pSwathBuf, nThisCols, nThisLines, 
                                        eDT, nBandCount, NULL, 
                                        0, 0, 0, &sExtraArg );

                GDALDestroyScaledProgress( sExtraArg.pProgressData );

                if( eErr == CE_None )
                    eErr = poDstDS->RasterIO( GF_Write, 
                                            iX, iY, nThisCols, nThisLines,
                                            pSwathBuf, nThisCols, nThisLines, 
                                            eDT, nBandCount, NULL, 
                                            0, 0, 0, NULL );

                nBlocksDone ++;
                if( eErr == CE_None 
                    && !pfnProgress( nBlocksDone / (double)nTotalBlocks,
                                    NULL, pProgressData ) )
                {
                    eErr = CE_Failure;
                    CPLError( CE_Failure, CPLE_UserInterrupt, 
                            "User terminated CreateCopy()" );
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( pSwathBuf );

    return eErr;
}


/************************************************************************/
/*                     GDALRasterBandCopyWholeRaster()                  */
/************************************************************************/

/**
 * \brief Copy all raster band raster data.
 *
 * This function copies the complete raster contents of one band to
 * another similarly configured band.  The source and destination
 * bands must have the same width and height.  The bands do not have
 * to have the same data type.
 *
 * It implements efficient copying, in particular "chunking" the copy in
 * substantial blocks.
 *
 * Currently the only papszOptions value supported is : "COMPRESSED=YES" to
 * force alignment on target dataset block sizes to achieve best compression.
 * More options may be supported in the future.
 *
 * @param hSrcBand the source band
 * @param hDstBand the destination band
 * @param papszOptions transfer hints in "StringList" Name=Value format.
 * @param pfnProgress progress reporting function.
 * @param pProgressData callback data for progress function.
 *
 * @return CE_None on success, or CE_Failure on failure.
 */

CPLErr CPL_STDCALL GDALRasterBandCopyWholeRaster(
    GDALRasterBandH hSrcBand, GDALRasterBandH hDstBand, char **papszOptions,
    GDALProgressFunc pfnProgress, void *pProgressData )

{
    VALIDATE_POINTER1( hSrcBand, "GDALRasterBandCopyWholeRaster", CE_Failure );
    VALIDATE_POINTER1( hDstBand, "GDALRasterBandCopyWholeRaster", CE_Failure );

    GDALRasterBand *poSrcBand = (GDALRasterBand *) hSrcBand;
    GDALRasterBand *poDstBand = (GDALRasterBand *) hDstBand;
    CPLErr eErr = CE_None;

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      Confirm the datasets match in size and band counts.             */
/* -------------------------------------------------------------------- */
    int nXSize = poSrcBand->GetXSize(),
        nYSize = poSrcBand->GetYSize();

    if( poDstBand->GetXSize() != nXSize
        || poDstBand->GetYSize() != nYSize )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Input and output band sizes do not\n"
                  "match in GDALRasterBandCopyWholeRaster()" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Report preliminary (0) progress.                                */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt,
                  "User terminated CreateCopy()" );
        return CE_Failure;
    }

    GDALDataType eDT = poDstBand->GetRasterDataType();

    /* If the destination is compressed, we must try to write blocks just once, to save */
    /* disk space (GTiff case for example), and to avoid data loss (JPEG compression for example) */
    int bDstIsCompressed = FALSE;
    const char* pszDstCompressed= CSLFetchNameValue( papszOptions, "COMPRESSED" );
    if (pszDstCompressed != NULL && CSLTestBoolean(pszDstCompressed))
        bDstIsCompressed = TRUE;

/* -------------------------------------------------------------------- */
/*      What will our swath size be?                                    */
/* -------------------------------------------------------------------- */

    int nSwathCols, nSwathLines;
    GDALCopyWholeRasterGetSwathSize(poSrcBand,
                                    poDstBand,
                                    1,
                                    bDstIsCompressed, FALSE,
                                    &nSwathCols, &nSwathLines);

    int nPixelSize = (GDALGetDataTypeSize(eDT) / 8);

    void *pSwathBuf = VSIMalloc3(nSwathCols, nSwathLines, nPixelSize );
    if( pSwathBuf == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                "Failed to allocate %d*%d*%d byte swath buffer in\n"
                "GDALRasterBandCopyWholeRaster()",
                nSwathCols, nSwathLines, nPixelSize );
        return CE_Failure;
    }

    CPLDebug( "GDAL",
            "GDALRasterBandCopyWholeRaster(): %d*%d swaths",
            nSwathCols, nSwathLines );

/* ==================================================================== */
/*      Band oriented (uninterleaved) case.                             */
/* ==================================================================== */

    int iX, iY;

    for( iY = 0; iY < nYSize && eErr == CE_None; iY += nSwathLines )
    {
        int nThisLines = nSwathLines;

        if( iY + nThisLines > nYSize )
            nThisLines = nYSize - iY;

        for( iX = 0; iX < nXSize && eErr == CE_None; iX += nSwathCols )
        {
            int nThisCols = nSwathCols;

            if( iX + nThisCols > nXSize )
                nThisCols = nXSize - iX;

            eErr = poSrcBand->RasterIO( GF_Read,
                                    iX, iY, nThisCols, nThisLines,
                                    pSwathBuf, nThisCols, nThisLines,
                                    eDT, 0, 0, NULL );

            if( eErr == CE_None )
                eErr = poDstBand->RasterIO( GF_Write,
                                        iX, iY, nThisCols, nThisLines,
                                        pSwathBuf, nThisCols, nThisLines,
                                        eDT, 0, 0, NULL );

            if( eErr == CE_None
                && !pfnProgress(
                    (iY+nThisLines) / (float) (nYSize),
                    NULL, pProgressData ) )
            {
                eErr = CE_Failure;
                CPLError( CE_Failure, CPLE_UserInterrupt,
                        "User terminated CreateCopy()" );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( pSwathBuf );

    return eErr;
}


/************************************************************************/
/*                      GDALCopyRasterIOExtraArg ()                     */
/************************************************************************/

void GDALCopyRasterIOExtraArg(GDALRasterIOExtraArg* psDestArg,
                              GDALRasterIOExtraArg* psSrcArg)
{
    INIT_RASTERIO_EXTRA_ARG(*psDestArg);
    if( psSrcArg )
    {
        psDestArg->eResampleAlg = psSrcArg->eResampleAlg;
        psDestArg->pfnProgress = psSrcArg->pfnProgress;
        psDestArg->pProgressData = psSrcArg->pProgressData;
        psDestArg->bFloatingPointWindowValidity = psSrcArg->bFloatingPointWindowValidity;
        if( psSrcArg->bFloatingPointWindowValidity )
        {
            psDestArg->dfXOff = psSrcArg->dfXOff;
            psDestArg->dfYOff = psSrcArg->dfYOff;
            psDestArg->dfXSize = psSrcArg->dfXSize;
            psDestArg->dfYSize = psSrcArg->dfYSize;
        }
    }
}
