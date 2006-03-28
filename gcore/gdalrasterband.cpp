/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Base class for format specific band class implementation.  This
 *           base class provides default implementation for many methods.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 * Revision 1.76  2006/03/28 14:49:56  fwarmerdam
 * updated contact info
 *
 * Revision 1.75  2006/02/07 19:07:07  fwarmerdam
 * applied some strategic improved outofmemory checking
 *
 * Revision 1.74  2005/11/01 22:17:17  fwarmerdam
 * fixed GDALSetDefaultRAT (bug 985)
 *
 * Revision 1.73  2005/09/28 00:54:05  fwarmerdam
 * Added some RAT docs.
 *
 * Revision 1.72  2005/09/24 19:02:46  fwarmerdam
 * RAT functions honour GMO_IGNORE_UNIMPLEMENTED
 *
 * Revision 1.71  2005/09/23 20:53:09  fwarmerdam
 * avoid issuing NotSupported errors if GMO_IGNORE_UNIMPLEMENTED is set
 *
 * Revision 1.70  2005/09/23 16:56:16  fwarmerdam
 * Added default RAT methods
 *
 * Revision 1.69  2005/07/25 23:15:10  fwarmerdam
 * Fixed another doc typo.
 *
 * Revision 1.68  2005/07/25 23:14:27  fwarmerdam
 * Fixed typo in BuildOverviews() docs.
 *
 * Revision 1.67  2005/07/11 21:07:33  fwarmerdam
 * fixed a couple papoBlocks related multi-threading issues
 *
 * Revision 1.66  2005/05/23 06:42:57  fwarmerdam
 * Updated for locking of block refs
 *
 * Revision 1.65  2005/05/19 14:45:16  fwarmerdam
 * SetStatistics should now succeed.
 *
 * Revision 1.64  2005/05/16 21:35:16  fwarmerdam
 * added CPLSetDefaultHistogram
 *
 * Revision 1.63  2005/05/13 18:19:37  fwarmerdam
 * added SetDefaultHistogram
 *
 * Revision 1.62  2005/05/11 14:17:57  fwarmerdam
 * include stdcall modifier for GDALGetDefaultHistogram
 *
 * Revision 1.61  2005/05/11 14:04:08  fwarmerdam
 * added getdefaulthistogram
 *
 * Revision 1.60  2005/05/10 04:49:54  fwarmerdam
 * added GetDefaultHistogram
 *
 * Revision 1.59  2005/05/03 21:09:54  fwarmerdam
 * Removed unused variable.
 *
 * Revision 1.58  2005/04/27 16:30:28  fwarmerdam
 * added statistics related methods
 *
 * Revision 1.57  2005/04/04 15:24:48  fwarmerdam
 * Most C entry points now CPL_STDCALL
 *
 * Revision 1.56  2005/03/19 21:50:09  fwarmerdam
 * bug 802: GetBlockRef()-adopt block before reading data into it
 *
 * Revision 1.55  2005/02/17 22:16:12  fwarmerdam
 * changed to use two level block cache
 *
 * Revision 1.54  2005/01/15 16:10:43  fwarmerdam
 * Added set scale/offset methods
 *
 * Revision 1.53  2005/01/04 21:14:01  fwarmerdam
 * added GDAL_FORCE_CACHING config variable
 */

#include "gdal_priv.h"
#include "gdal_rat.h"
#include "cpl_string.h"

#define SUBBLOCK_SIZE 64
#define TO_SUBBLOCK(x) ((x) >> 6)
#define WITHIN_SUBBLOCK(x) ((x) & 0x3f)

CPL_CVSID("$Id$");

/************************************************************************/
/*                           GDALRasterBand()                           */
/************************************************************************/

/*! Constructor. Applications should never create GDALRasterBands directly. */

GDALRasterBand::GDALRasterBand()

{
    poDS = NULL;
    nBand = 0;

    eAccess = GA_ReadOnly;
    nBlockXSize = nBlockYSize = -1;
    eDataType = GDT_Byte;

    nSubBlocksPerRow = nBlocksPerRow = 0;
    nSubBlocksPerColumn = nBlocksPerColumn = 0;

    bSubBlockingActive = FALSE;
    papoBlocks = NULL;

    nBlockReads = 0;
    bForceCachedIO =  CSLTestBoolean( 
        CPLGetConfigOption( "GDAL_FORCE_CACHING", "NO") );
}

/************************************************************************/
/*                          ~GDALRasterBand()                           */
/************************************************************************/

/*! Destructor. Applications should never destroy GDALRasterBands directly,
    instead destroy the GDALDataset. */

GDALRasterBand::~GDALRasterBand()

{
    FlushCache();

    CPLFree( papoBlocks );

    if( nBlockReads > nBlocksPerRow * nBlocksPerColumn
        && nBand == 1 && poDS != NULL )
    {
        CPLDebug( "GDAL", "%d block reads on %d block band 1 of %s.",
                  nBlockReads, nBlocksPerRow * nBlocksPerColumn, 
                  poDS->GetDescription() );
    }
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

/**
 * Read/write a region of image data for this band.
 *
 * This method allows reading a region of a GDALRasterBand into a buffer,
 * or writing data from a buffer into a region of a GDALRasterBand.  It
 * automatically takes care of data type translation if the data type
 * (eBufType) of the buffer is different than that of the GDALRasterBand.
 * The method also takes care of image decimation / replication if the
 * buffer size (nBufXSize x nBufYSize) is different than the size of the
 * region being accessed (nXSize x nYSize).
 *
 * The nPixelSpace and nLineSpace parameters allow reading into or
 * writing from unusually organized buffers.  This is primarily used
 * for buffers containing more than one bands raster data in interleaved
 * format. 
 *
 * Some formats may efficiently implement decimation into a buffer by
 * reading from lower resolution overview images.
 *
 * For highest performance full resolution data access, read and write
 * on "block boundaries" as returned by GetBlockSize(), or use the
 * ReadBlock() and WriteBlock() methods.
 *
 * This method is the same as the C GDALRasterIO() function.
 *
 * @param eRWFlag Either GF_Read to read a region of data, or GT_Write to
 * write a region of data.
 *
 * @param nXOff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param nYOff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param pData The buffer into which the data should be read, or from which
 * it should be written.  This buffer must contain at least nBufXSize *
 * nBufYSize words of type eBufType.  It is organized in left to right,
 * top to bottom pixel order.  Spacing is controlled by the nPixelSpace,
 * and nLineSpace parameters.
 *
 * @param nBufXSize the width of the buffer image into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param nBufYSize the height of the buffer image into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param eBufType the type of the pixel values in the pData data buffer.  The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline.  If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next.  If defaulted the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @return CE_Failure if the access fails, otherwise CE_None.
 */

CPLErr GDALRasterBand::RasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 int nPixelSpace,
                                 int nLineSpace )

{
/* -------------------------------------------------------------------- */
/*      If pixel and line spaceing are defaulted assign reasonable      */
/*      value assuming a packed buffer.                                 */
/* -------------------------------------------------------------------- */
    if( nPixelSpace == 0 )
        nPixelSpace = GDALGetDataTypeSize( eBufType ) / 8;
    
    if( nLineSpace == 0 )
        nLineSpace = nPixelSpace * nBufXSize;
    
/* -------------------------------------------------------------------- */
/*      Do some validation of parameters.                               */
/* -------------------------------------------------------------------- */
    if( nXOff < 0 || nXOff + nXSize > nRasterXSize
        || nYOff < 0 || nYOff + nYSize > nRasterYSize )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Access window out of range in RasterIO().  Requested\n"
                  "(%d,%d) of size %dx%d on raster of %dx%d.",
                  nXOff, nYOff, nXSize, nYSize, nRasterXSize, nRasterYSize );
        return CE_Failure;
    }

    if( eRWFlag != GF_Read && eRWFlag != GF_Write )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "eRWFlag = %d, only GF_Read (0) and GF_Write (1) are legal.",
                  eRWFlag );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Some size values are "noop".  Lets just return to avoid         */
/*      stressing lower level functions.                                */
/* -------------------------------------------------------------------- */
    if( nXSize < 1 || nYSize < 1 || nBufXSize < 1 || nBufYSize < 1 )
    {
        CPLDebug( "GDAL", 
                  "RasterIO() skipped for odd window or buffer size.\n"
                  "  Window = (%d,%d)x%dx%d\n"
                  "  Buffer = %dx%d\n",
                  nXOff, nYOff, nXSize, nYSize, 
                  nBufXSize, nBufYSize );

        return CE_None;
    }
    
/* -------------------------------------------------------------------- */
/*      Call the format specific function.                              */
/* -------------------------------------------------------------------- */
    if( bForceCachedIO )
        return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nPixelSpace, nLineSpace );
    else
        return IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                          pData, nBufXSize, nBufYSize, eBufType,
                          nPixelSpace, nLineSpace ) ;
}

/************************************************************************/
/*                            GDALRasterIO()                            */
/************************************************************************/

/**
 * @see GDALRasterBand::Rasterio()
 */

CPLErr CPL_STDCALL 
GDALRasterIO( GDALRasterBandH hBand, GDALRWFlag eRWFlag,
              int nXOff, int nYOff, int nXSize, int nYSize,
              void * pData, int nBufXSize, int nBufYSize,
              GDALDataType eBufType,
              int nPixelSpace, int nLineSpace )
    
{
    GDALRasterBand      *poBand = (GDALRasterBand *) hBand;

    return( poBand->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                              pData, nBufXSize, nBufYSize, eBufType,
                              nPixelSpace, nLineSpace ) );
}
                     
/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

/**
 * Read a block of image data efficiently.
 *
 * This method accesses a "natural" block from the raster band without
 * resampling, or data type conversion.  For a more generalized, but
 * potentially less efficient access use RasterIO().
 *
 * This method is the same as the C GDALReadBlock() function.
 *
 * See the GetBlockRef() method for a way of accessing internally cached
 * block oriented data without an extra copy into an application buffer.
 *
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth. 
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @param pImage the buffer into which the data will be read.  The buffer
 * must be large enough to hold GetBlockXSize()*GetBlockYSize() words
 * of type GetRasterDataType().
 *
 * @return CE_None on success or CE_Failure on an error.
 *
 * The following code would efficiently compute a histogram of eight bit
 * raster data.  Note that the final block may be partial ... data beyond
 * the edge of the underlying raster band in these edge blocks is of an
 * undermined value.
 *
<pre>
 CPLErr GetHistogram( GDALRasterBand *poBand, int *panHistogram )

 {
     int        nXBlocks, nYBlocks, nXBlockSize, nYBlockSize;
     int        iXBlock, iYBlock;
     GByte      *pabyData;

     memset( panHistogram, 0, sizeof(int) * 256 );

     CPLAssert( poBand->GetRasterDataType() == GDT_Byte );

     poBand->GetBlockSize( &nXBlockSize, &nYBlockSize );
     nXBlocks = (poBand->GetXSize() + nXBlockSize - 1) / nXBlockSize;
     nYBlocks = (poBand->GetYSize() + nYBlockSize - 1) / nYBlockSize;

     pabyData = (GByte *) CPLMalloc(nXBlockSize * nYBlockSize);

     for( iYBlock = 0; iYBlock < nYBlocks; iYBlock++ )
     {
         for( iXBlock = 0; iXBlock < nXBlocks; iXBlock++ )
         {
             int        nXValid, nYValid;
             
             poBand->ReadBlock( iXBlock, iYBlock, pabyData );

             // Compute the portion of the block that is valid
             // for partial edge blocks.
             if( (iXBlock+1) * nXBlockSize > poBand->GetXSize() )
                 nXValid = poBand->GetXSize() - iXBlock * nXBlockSize;
             else
                 nXValid = nXBlockSize;

             if( (iYBlock+1) * nYBlockSize > poBand->GetYSize() )
                 nYValid = poBand->GetYSize() - iYBlock * nYBlockSize;
             else
                 nYValid = nYBlockSize;

             // Collect the histogram counts.
             for( int iY = 0; iY < nYValid; iY++ )
             {
                 for( int iX = 0; iX < nXValid; iX++ )
                 {
                     panHistogram[pabyData[iX + iY * nXBlockSize]] += 1;
                 }
             }
         }
     }
 }
 
</pre>
 */


CPLErr GDALRasterBand::ReadBlock( int nXBlockOff, int nYBlockOff,
                                   void * pImage )

{
/* -------------------------------------------------------------------- */
/*      Validate arguments.                                             */
/* -------------------------------------------------------------------- */
    CPLAssert( pImage != NULL );
    
    if( nXBlockOff < 0
        || nXBlockOff*nBlockXSize >= nRasterXSize )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nXBlockOff value (%d) in "
                        "GDALRasterBand::ReadBlock()\n",
                  nXBlockOff );

        return( CE_Failure );
    }

    if( nYBlockOff < 0
        || nYBlockOff*nBlockYSize >= nRasterYSize )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nYBlockOff value (%d) in "
                        "GDALRasterBand::ReadBlock()\n",
                  nYBlockOff );

        return( CE_Failure );
    }
    
    if( !InitBlockInfo() )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Invoke underlying implementation method.                        */
/* -------------------------------------------------------------------- */
    return( IReadBlock( nXBlockOff, nYBlockOff, pImage ) );
}

/************************************************************************/
/*                           GDALReadBlock()                            */
/************************************************************************/

/**
 * @see GDALRasterBand::ReadBlock()
 */

CPLErr CPL_STDCALL GDALReadBlock( GDALRasterBandH hBand, int nXOff, int nYOff,
                      void * pData )

{
    GDALRasterBand      *poBand = (GDALRasterBand *) hBand;

    return( poBand->ReadBlock( nXOff, nYOff, pData ) );
}

/************************************************************************/
/*                            IWriteBlock()                             */
/*                                                                      */
/*      Default internal implementation ... to be overriden by          */
/*      subclasses that support writing.                                */
/************************************************************************/

CPLErr GDALRasterBand::IWriteBlock( int, int, void * )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        CPLError( CE_Failure, CPLE_NotSupported,
                  "WriteBlock() not supported for this dataset." );
    
    return( CE_Failure );
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

/**
 * Write a block of image data efficiently.
 *
 * This method accesses a "natural" block from the raster band without
 * resampling, or data type conversion.  For a more generalized, but
 * potentially less efficient access use RasterIO().
 *
 * This method is the same as the C GDALWriteBlock() function.
 *
 * See ReadBlock() for an example of block oriented data access.
 *
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth. 
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @param pImage the buffer from which the data will be written.  The buffer
 * must be large enough to hold GetBlockXSize()*GetBlockYSize() words
 * of type GetRasterDataType().
 *
 * @return CE_None on success or CE_Failure on an error.
 *
 * The following code would efficiently compute a histogram of eight bit
 * raster data.  Note that the final block may be partial ... data beyond
 * the edge of the underlying raster band in these edge blocks is of an
 * undermined value.
 *
 */

CPLErr GDALRasterBand::WriteBlock( int nXBlockOff, int nYBlockOff,
                                   void * pImage )

{
/* -------------------------------------------------------------------- */
/*      Validate arguments.                                             */
/* -------------------------------------------------------------------- */
    CPLAssert( pImage != NULL );
    
    if( nXBlockOff < 0
        || nXBlockOff*nBlockXSize >= GetXSize() )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nXBlockOff value (%d) in "
                        "GDALRasterBand::WriteBlock()\n",
                  nXBlockOff );

        return( CE_Failure );
    }

    if( nYBlockOff < 0
        || nYBlockOff*nBlockYSize >= GetYSize() )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nYBlockOff value (%d) in "
                        "GDALRasterBand::WriteBlock()\n",
                  nYBlockOff );

        return( CE_Failure );
    }

    if( eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Attempt to write to read only dataset in"
                  "GDALRasterBand::WriteBlock().\n" );

        return( CE_Failure );
    }

    if( !InitBlockInfo() )
        return CE_Failure;
    
/* -------------------------------------------------------------------- */
/*      Invoke underlying implementation method.                        */
/* -------------------------------------------------------------------- */
    return( IWriteBlock( nXBlockOff, nYBlockOff, pImage ) );
}

/************************************************************************/
/*                           GDALWriteBlock()                           */
/************************************************************************/

/**
 * @see GDALRasterBand::WriteBlock()
 */

CPLErr CPL_STDCALL GDALWriteBlock( GDALRasterBandH hBand, int nXOff, int nYOff,
                       void * pData )

{
    GDALRasterBand      *poBand = (GDALRasterBand *) hBand;

    return( poBand->WriteBlock( nXOff, nYOff, pData ) );
}


/************************************************************************/
/*                         GetRasterDataType()                          */
/************************************************************************/

/**
 * Fetch the pixel data type for this band.
 *
 * @return the data type of pixels for this band.
 */
  

GDALDataType GDALRasterBand::GetRasterDataType()

{
    return eDataType;
}

/************************************************************************/
/*                       GDALGetRasterDataType()                        */
/************************************************************************/

/**
 * @see GDALRasterBand::GetRasterDataType()
 */

GDALDataType CPL_STDCALL GDALGetRasterDataType( GDALRasterBandH hBand )

{
    return( ((GDALRasterBand *) hBand)->GetRasterDataType() );
}

/************************************************************************/
/*                            GetBlockSize()                            */
/************************************************************************/

/**
 * Fetch the "natural" block size of this band.
 *
 * GDAL contains a concept of the natural block size of rasters so that
 * applications can organized data access efficiently for some file formats.
 * The natural block size is the block size that is most efficient for
 * accessing the format.  For many formats this is simple a whole scanline
 * in which case *pnXSize is set to GetXSize(), and *pnYSize is set to 1.
 *
 * However, for tiled images this will typically be the tile size.
 *
 * Note that the X and Y block sizes don't have to divide the image size
 * evenly, meaning that right and bottom edge blocks may be incomplete.
 * See ReadBlock() for an example of code dealing with these issues.
 *
 * @param pnXSize integer to put the X block size into or NULL.
 *
 * @param pnYSize integer to put the Y block size into or NULL.
 */

void GDALRasterBand::GetBlockSize( int * pnXSize, int *pnYSize )

{
    CPLAssert( nBlockXSize > 0 && nBlockYSize > 0 );
    
    if( pnXSize != NULL )
        *pnXSize = nBlockXSize;
    if( pnYSize != NULL )
        *pnYSize = nBlockYSize;
}

/************************************************************************/
/*                          GDALGetBlockSize()                          */
/************************************************************************/

/**
 * @see GDALRasterBand::GetBlockSize()
 */

void CPL_STDCALL 
GDALGetBlockSize( GDALRasterBandH hBand, int * pnXSize, int * pnYSize )

{
    GDALRasterBand      *poBand = (GDALRasterBand *) hBand;

    poBand->GetBlockSize( pnXSize, pnYSize );
}

/************************************************************************/
/*                           InitBlockInfo()                            */
/************************************************************************/

int GDALRasterBand::InitBlockInfo()

{
    if( papoBlocks != NULL )
        return TRUE;

    CPLAssert( nBlockXSize > 0 && nBlockYSize > 0 );

    nBlocksPerRow = (nRasterXSize+nBlockXSize-1) / nBlockXSize;
    nBlocksPerColumn = (nRasterYSize+nBlockYSize-1) / nBlockYSize;
    
    if( nBlocksPerRow < SUBBLOCK_SIZE/2 )
    {
        bSubBlockingActive = FALSE;
        
        papoBlocks = (GDALRasterBlock **)
            VSICalloc( sizeof(void*), nBlocksPerRow * nBlocksPerColumn );
    }
    else
    {
        bSubBlockingActive = TRUE;

        nSubBlocksPerRow = (nBlocksPerRow + SUBBLOCK_SIZE + 1)/SUBBLOCK_SIZE;
        nSubBlocksPerColumn = (nBlocksPerColumn + SUBBLOCK_SIZE + 1)/SUBBLOCK_SIZE;
        
        papoBlocks = (GDALRasterBlock **)
            VSICalloc( sizeof(void*), nSubBlocksPerRow * nSubBlocksPerColumn );
    }

    if( papoBlocks == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "Out of memory in InitBlockInfo()." );
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                             AdoptBlock()                             */
/*                                                                      */
/*      Add a block to the raster band's block matrix.  If this         */
/*      exceeds our maximum blocks for this layer, flush the oldest     */
/*      block out.                                                      */
/*                                                                      */
/*      This method is protected.                                       */
/************************************************************************/

CPLErr GDALRasterBand::AdoptBlock( int nXBlockOff, int nYBlockOff,
                                   GDALRasterBlock * poBlock )

{
    int         nBlockIndex;
    
    InitBlockInfo();
    
/* -------------------------------------------------------------------- */
/*      Simple case without subblocking.                                */
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        nBlockIndex = nXBlockOff + nYBlockOff * nBlocksPerRow;

        if( papoBlocks[nBlockIndex] == poBlock )
            return( CE_None );

        if( papoBlocks[nBlockIndex] != NULL )
            FlushBlock( nXBlockOff, nYBlockOff );

        papoBlocks[nBlockIndex] = poBlock;
        poBlock->Touch();

        return( CE_None );
    }

/* -------------------------------------------------------------------- */
/*      Identify the subblock in which our target occurs, and create    */
/*      it if necessary.                                                */
/* -------------------------------------------------------------------- */
    int nSubBlock = TO_SUBBLOCK(nXBlockOff) 
        + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;

    if( papoBlocks[nSubBlock] == NULL )
    {
        int nSubGridSize = 
            sizeof(GDALRasterBlock*) * SUBBLOCK_SIZE * SUBBLOCK_SIZE;

        papoBlocks[nSubBlock] = (GDALRasterBlock *) VSIMalloc(nSubGridSize);
        if( papoBlocks[nSubBlock] == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "Out of memory in AdoptBlock()." );
            return CE_Failure;
        }

        memset( papoBlocks[nSubBlock], 0, nSubGridSize );
    }

/* -------------------------------------------------------------------- */
/*      Check within subblock.                                          */
/* -------------------------------------------------------------------- */
    GDALRasterBlock **papoSubBlockGrid = 
        (GDALRasterBlock **) papoBlocks[nSubBlock];

    int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
        + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;

    if( papoSubBlockGrid[nBlockInSubBlock] == poBlock )
        return CE_None;

    if( papoSubBlockGrid[nBlockInSubBlock] != NULL )
        FlushBlock( nXBlockOff, nYBlockOff );

    papoSubBlockGrid[nBlockInSubBlock] = poBlock;
    poBlock->Touch();

    return CE_None;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

/**
 * Flush raster data cache.
 *
 * This call will recover memory used to cache data blocks for this raster
 * band, and ensure that new requests are referred to the underlying driver.
 *
 * This method is the same as the C function GDALFlushRasterCache().
 *
 * @return CE_None on success.
 */

CPLErr GDALRasterBand::FlushCache()

{
/* -------------------------------------------------------------------- */
/*      Flush all blocks in memory ... this case is without subblocking.*/
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        for( int iY = 0; iY < nBlocksPerColumn; iY++ )
        {
            for( int iX = 0; iX < nBlocksPerRow; iX++ )
            {
                if( papoBlocks[iX + iY*nBlocksPerRow] != NULL )
                {
                    CPLErr    eErr;

                    eErr = FlushBlock( iX, iY );

                    if( eErr != CE_None )
                        return eErr;
                }
            }
        }
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      With subblocking.  We can short circuit missing subblocks.      */
/* -------------------------------------------------------------------- */
    int iSBX, iSBY;

    for( iSBY = 0; iSBY < nSubBlocksPerColumn; iSBY++ )
    {
        for( iSBX = 0; iSBX < nSubBlocksPerRow; iSBX++ )
        {
            int nSubBlock = iSBX + iSBY * nSubBlocksPerRow;
        
            GDALRasterBlock **papoSubBlockGrid = 
                (GDALRasterBlock **) papoBlocks[nSubBlock];

            if( papoSubBlockGrid == NULL )
                continue;

            for( int iY = 0; iY < SUBBLOCK_SIZE; iY++ )
            {
                for( int iX = 0; iX < SUBBLOCK_SIZE; iX++ )
                {
                    if( papoSubBlockGrid[iX + iY * SUBBLOCK_SIZE] != NULL )
                    {
                        CPLErr eErr;

                        eErr = FlushBlock( iX + iSBX * SUBBLOCK_SIZE, 
                                           iY + iSBY * SUBBLOCK_SIZE );
                        if( eErr != CE_None )
                            return eErr;
                    }
                }
            }

            // We might as well get rid of this grid chunk since we know 
            // it is now empty.
            papoBlocks[nSubBlock] = NULL;
            CPLFree( papoSubBlockGrid );
        }
    }

    return( CE_None );
}

/************************************************************************/
/*                        GDALFlushRasterCache()                        */
/************************************************************************/

/**
 * @see GDALRasterBand::FlushCache()
 */

CPLErr CPL_STDCALL GDALFlushRasterCache( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->FlushCache();
}

/************************************************************************/
/*                             FlushBlock()                             */
/*                                                                      */
/*      Flush a block out of the block cache.  If it has been           */
/*      modified write it to disk.  If no specific tile is              */
/*      indicated, write the oldest tile.                               */
/*                                                                      */
/*      Protected method.                                               */
/************************************************************************/

CPLErr GDALRasterBand::FlushBlock( int nXBlockOff, int nYBlockOff )

{
    int             nBlockIndex;
    GDALRasterBlock *poBlock;

    if( !papoBlocks )
        return CE_None;
    
/* -------------------------------------------------------------------- */
/*      Validate the request                                            */
/* -------------------------------------------------------------------- */
    if( nXBlockOff < 0 || nXBlockOff >= nBlocksPerRow )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nBlockXOff value (%d) in "
                        "GDALRasterBand::GetBlockRef()\n",
                  nXBlockOff );

        return( CE_Failure );
    }

    if( nYBlockOff < 0 || nYBlockOff >= nBlocksPerColumn )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nBlockYOff value (%d) in "
                        "GDALRasterBand::GetBlockRef()\n",
                  nYBlockOff );

        return( CE_Failure );
    }

/* -------------------------------------------------------------------- */
/*      Simple case for single level caches.                            */
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        nBlockIndex = nXBlockOff + nYBlockOff * nBlocksPerRow;

        GDALRasterBlock::SafeLockBlock( papoBlocks + nBlockIndex );

        poBlock = papoBlocks[nBlockIndex];
        papoBlocks[nBlockIndex] = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Identify our subblock.                                          */
/* -------------------------------------------------------------------- */
    else
    {
        int nSubBlock = TO_SUBBLOCK(nXBlockOff) 
            + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;
        
        if( papoBlocks[nSubBlock] == NULL )
            return CE_None;
        
/* -------------------------------------------------------------------- */
/*      Check within subblock.                                          */
/* -------------------------------------------------------------------- */
        GDALRasterBlock **papoSubBlockGrid = 
            (GDALRasterBlock **) papoBlocks[nSubBlock];
        
        int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
            + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;
        
        GDALRasterBlock::SafeLockBlock( papoSubBlockGrid + nBlockInSubBlock );

        poBlock = papoSubBlockGrid[nBlockInSubBlock];
        papoSubBlockGrid[nBlockInSubBlock] = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Is the target block dirty?  If so we need to write it.          */
/* -------------------------------------------------------------------- */
    if( poBlock == NULL )
        return CE_None;

    poBlock->Detach();

    if( poBlock->GetDirty() )
        poBlock->Write();

/* -------------------------------------------------------------------- */
/*      Deallocate the block;                                           */
/* -------------------------------------------------------------------- */
    poBlock->DropLock();
    delete poBlock;

    return( CE_None );
}

/************************************************************************/
/*                        TryGetLockedBlockRef()                        */
/************************************************************************/

/**
 * Try fetching block ref. 
 *
 * This method will returned the requested block (locked) if it is already
 * in the block cache for the layer.  If not, NULL is returned.  
 * 
 * If a non-NULL value is returned, then a lock for the block will have been
 * acquired on behalf of the caller.  It is absolutely imperative that the
 * caller release this lock (with GDALRasterBlock::DropLock()) or else
 * severe problems may result.
 *
 * @param nBlockXOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth. 
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the top most block, 1 the next block and so forth.
 * 
 * @return NULL if block not available, or locked block pointer. 
 */

GDALRasterBlock *GDALRasterBand::TryGetLockedBlockRef( int nXBlockOff, 
                                                       int nYBlockOff )

{
    int             nBlockIndex;
    
    InitBlockInfo();
    
/* -------------------------------------------------------------------- */
/*      Validate the request                                            */
/* -------------------------------------------------------------------- */
    if( nXBlockOff < 0 || nXBlockOff >= nBlocksPerRow )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nBlockXOff value (%d) in "
                        "GDALRasterBand::GetBlockRef()\n",
                  nXBlockOff );

        return( NULL );
    }

    if( nYBlockOff < 0 || nYBlockOff >= nBlocksPerColumn )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nBlockYOff value (%d) in "
                        "GDALRasterBand::GetBlockRef()\n",
                  nYBlockOff );

        return( NULL );
    }

/* -------------------------------------------------------------------- */
/*      Simple case for single level caches.                            */
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        nBlockIndex = nXBlockOff + nYBlockOff * nBlocksPerRow;
        
        GDALRasterBlock::SafeLockBlock( papoBlocks + nBlockIndex );

        return papoBlocks[nBlockIndex];
    }

/* -------------------------------------------------------------------- */
/*      Identify our subblock.                                          */
/* -------------------------------------------------------------------- */
    int nSubBlock = TO_SUBBLOCK(nXBlockOff) 
        + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;

    if( papoBlocks[nSubBlock] == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Check within subblock.                                          */
/* -------------------------------------------------------------------- */
    GDALRasterBlock **papoSubBlockGrid = 
        (GDALRasterBlock **) papoBlocks[nSubBlock];

    int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
        + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;

    GDALRasterBlock::SafeLockBlock( papoSubBlockGrid + nBlockInSubBlock );

    return papoSubBlockGrid[nBlockInSubBlock];
}

/************************************************************************/
/*                         GetLockedBlockRef()                          */
/************************************************************************/

/**
 * Fetch a pointer to an internally cached raster block.
 *
 * This method will returned the requested block (locked) if it is already
 * in the block cache for the layer.  If not, the block will be read from 
 * the driver, and placed in the layer block cached, then returned.  If an
 * error occurs reading the block from the driver, a NULL value will be
 * returned.
 * 
 * If a non-NULL value is returned, then a lock for the block will have been
 * acquired on behalf of the caller.  It is absolutely imperative that the
 * caller release this lock (with GDALRasterBlock::DropLock()) or else
 * severe problems may result.
 *
 * Note that calling GetBlockRef() on a previously uncached band will
 * enable caching.
 * 
 * @param nBlockXOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth. 
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the top most block, 1 the next block and so forth.
 * 
 * @param bJustInitialize If TRUE the block will be allocated and initialized,
 * but not actually read from the source.  This is useful when it will just
 * be completely set and written back. 
 *
 * @return pointer to the block object, or NULL on failure.
 */

GDALRasterBlock * GDALRasterBand::GetLockedBlockRef( int nXBlockOff,
                                                     int nYBlockOff,
                                                     int bJustInitialize )

{
    GDALRasterBlock *poBlock;

/* -------------------------------------------------------------------- */
/*      Try and fetch from cache.                                       */
/* -------------------------------------------------------------------- */
    poBlock = TryGetLockedBlockRef( nXBlockOff, nYBlockOff );

/* -------------------------------------------------------------------- */
/*      If we didn't find it in our memory cache, instantiate a         */
/*      block (potentially load from disk) and "adopt" it into the      */
/*      cache.                                                          */
/* -------------------------------------------------------------------- */
    if( poBlock == NULL )
    {
        poBlock = new GDALRasterBlock( this, nXBlockOff, nYBlockOff );

        poBlock->AddLock();

        /* allocate data space */
        if( poBlock->Internalize() != CE_None )
        {
            delete poBlock;
            return( NULL );
        }

        AdoptBlock( nXBlockOff, nYBlockOff, poBlock );

        if( !bJustInitialize
         && IReadBlock(nXBlockOff,nYBlockOff,poBlock->GetDataRef()) != CE_None)
        {
            poBlock->DropLock();
            FlushBlock( nXBlockOff, nYBlockOff );
            CPLError( CE_Failure, CPLE_AppDefined,
		      "IReadBlock failed at X offset %d, Y offset %d",
		      nXBlockOff, nYBlockOff );
	    return( NULL );
        }

        if( !bJustInitialize )
        {
            nBlockReads++;
            if( nBlockReads == nBlocksPerRow * nBlocksPerColumn + 1 
                && nBand == 1 && poDS != NULL )
            {
                CPLDebug( "GDAL", "Potential thrashing on band %d of %s.",
                          nBand, poDS->GetDescription() );
            }
        }
    }

    return poBlock;
}

/************************************************************************/
/*                               Fill()                                 */
/************************************************************************/

/** 
 * Fill this band with a constant value. GDAL makes no guarantees
 * about what values pixels in newly created files are set to, so this
 * method can be used to clear a band to a specified "default" value.
 * The fill value is passed in as a double but this will be converted
 * to the underlying type before writing to the file. An optional
 * second argument allows the imaginary component of a complex
 * constant value to be specified.
 * 
 * @param dfRealvalue Real component of fill value
 * @param dfImaginaryValue Imaginary component of fill value, defaults to zero
 * 
 * @return CE_Failure if the write fails, otherwise CE_None
 */
CPLErr GDALRasterBand::Fill(double dfRealValue, double dfImaginaryValue) {

    // General approach is to construct a source block of the file's
    // native type containing the appropriate value and then copy this
    // to each block in the image via the the RasterBlock cache. Using
    // the cache means we avoid file I/O if it's not necessary, at the
    // expense of some extra memcpy's (since we write to the
    // RasterBlock cache, which is then at some point written to the
    // underlying file, rather than simply directly to the underlying
    // file.)

    // Check we can write to the file
    if( eAccess == GA_ReadOnly ) {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Attempt to write to read only dataset in"
                 "GDALRasterBand::Fill().\n" );
        return CE_Failure;
    }

    // Make sure block parameters are set
    InitBlockInfo();

    // Allocate the source block
    int blockSize = nBlockXSize * nBlockYSize;
    int elementSize = GDALGetDataTypeSize(eDataType) / 8;
    int blockByteSize = blockSize * elementSize;
    unsigned char* srcBlock = (unsigned char*) VSIMalloc(blockByteSize);
    if (srcBlock == NULL) {
	CPLError(CE_Failure, CPLE_OutOfMemory,
                 "GDALRasterBand::Fill(): Out of memory "
		 "allocating %d bytes.\n", blockByteSize);
        return CE_Failure;
    }
    
    // Initialize the first element of the block, doing type conversion
    double complexSrc[2] = { dfRealValue, dfImaginaryValue };
    GDALCopyWords(complexSrc, GDT_CFloat64, 0, srcBlock, eDataType, 0, 1);

    // Copy first element to the rest of the block
    for (unsigned char* blockPtr = srcBlock + elementSize; 
	 blockPtr < srcBlock + blockByteSize; blockPtr += elementSize) {
	memcpy(blockPtr, srcBlock, elementSize);
    }

    // Write block to block cache
    for (int j = 0; j < nBlocksPerColumn; ++j) {
	for (int i = 0; i < nBlocksPerRow; ++i) {
	    GDALRasterBlock* destBlock = GetLockedBlockRef(i, j, TRUE);
	    if (destBlock == NULL) {
		CPLError(CE_Failure, CPLE_OutOfMemory,
			 "GDALRasterBand::Fill(): Error "
			 "while retrieving cache block.\n");
		return CE_Failure;
	    }
	    memcpy(destBlock->GetDataRef(), srcBlock, blockByteSize);
	    destBlock->MarkDirty();
            destBlock->DropLock();
	}
    }

    // Free up the source block
    VSIFree(srcBlock);

    return CE_None;
}


/************************************************************************/
/*                         GDALFillRaster()                             */
/************************************************************************/

/** 
 * Fill this band with a constant value. Set \a dfImaginaryValue to
 * zero non-complex rasters.
 * 
 * @param dfRealvalue Real component of fill value
 * @param dfImaginaryValue Imaginary component of fill value
 * 
 * @see GDALRasterBand::Fill()
 * 
 * @return CE_Failure if the write fails, otherwise CE_None
 */
CPLErr CPL_STDCALL GDALFillRaster(GDALRasterBandH hBand, double dfRealValue, 
		      double dfImaginaryValue) {
    return ((GDALRasterBand*) hBand)->Fill(dfRealValue, dfImaginaryValue);
}

/************************************************************************/
/*                             GetAccess()                              */
/************************************************************************/

/**
 * Find out if we have update permission for this band.
 *
 * This method is the same as the C function GDALGetRasterAccess().
 *
 * @return Either GA_Update or GA_ReadOnly.
 */

GDALAccess GDALRasterBand::GetAccess()

{
    return eAccess;
}

/************************************************************************/
/*                        GDALGetRasterAccess()                         */
/************************************************************************/

/**
 * @see GDALRasterBand::GetAccess()
 */

GDALAccess CPL_STDCALL GDALGetRasterAccess( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->GetAccess();
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

/**
 * Fetch the list of category names for this raster.
 *
 * The return list is a "StringList" in the sense of the CPL functions.
 * That is a NULL terminated array of strings.  Raster values without 
 * associated names will have an empty string in the returned list.  The
 * first entry in the list is for raster values of zero, and so on. 
 *
 * The returned stringlist should not be altered or freed by the application.
 * It may change on the next GDAL call, so please copy it if it is needed
 * for any period of time. 
 * 
 * @return list of names, or NULL if none.
 */

char **GDALRasterBand::GetCategoryNames()

{
    return NULL;
}

/************************************************************************/
/*                     GDALGetRasterCategoryNames()                     */
/************************************************************************/

/**
 * @see GDALRasterBand::GetCategoryNames()
 */

char ** CPL_STDCALL GDALGetRasterCategoryNames( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->GetCategoryNames();
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

/**
 * Set the category names for this band.
 *
 * See the GetCategoryNames() method for more on the interpretation of
 * category names. 
 *
 * This method is the same as the C function GDALSetRasterCategoryNames().
 *
 * @param papszNames the NULL terminated StringList of category names.  May
 * be NULL to just clear the existing list. 
 *
 * @return CE_None on success of CE_Failure on failure.  If unsupported
 * by the driver CE_Failure is returned, but no error message is reported.
 */

CPLErr GDALRasterBand::SetCategoryNames( char ** )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SetCategoryNames() not supported for this dataset." );
    
    return CE_Failure;
}

/************************************************************************/
/*                        GDALSetCategoryNames()                        */
/************************************************************************/

/**
 * @see GDALRasterBand::SetCategoryNames()
 */

CPLErr CPL_STDCALL 
GDALSetRasterCategoryNames( GDALRasterBandH hBand, char ** papszNames )

{
    return ((GDALRasterBand *) hBand)->SetCategoryNames( papszNames );
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

/**
 * Fetch the no data value for this band.
 * 
 * If there is no out of data value, an out of range value will generally
 * be returned.  The no data value for a band is generally a special marker
 * value used to mark pixels that are not valid data.  Such pixels should
 * generally not be displayed, nor contribute to analysis operations.
 *
 * This method is the same as the C function GDALGetRasterNoDataValue().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if a value
 * is actually associated with this layer.  May be NULL (default).
 *
 * @return the nodata value for this band.
 */

double GDALRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = FALSE;
    
    return -1e10;
}

/************************************************************************/
/*                      GDALGetRasterNoDataValue()                      */
/************************************************************************/

/**
 * @see GDALRasterBand::GetNoDataValue()
 */

double CPL_STDCALL 
GDALGetRasterNoDataValue( GDALRasterBandH hBand, int *pbSuccess )

{
    return ((GDALRasterBand *) hBand)->GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

/**
 * Set the no data value for this band. 
 *
 * To clear the nodata value, just set it with an "out of range" value.
 * Complex band no data values must have an imagery component of zero.
 *
 * This method is the same as the C function GDALSetRasterNoDataValue().
 *
 * @param dfNoData the value to set.
 *
 * @return CE_None on success, or CE_Failure on failure.  If unsupported
 * by the driver, CE_Failure is returned by no error message will have
 * been emitted.
 */

CPLErr GDALRasterBand::SetNoDataValue( double )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SetNoDataValue() not supported for this dataset." );

    return CE_Failure;
}

/************************************************************************/
/*                         GDALSetRasterNoDataValue()                   */
/************************************************************************/

/**
 * @see GDALRasterBand::SetNoDataValue()
 */

CPLErr CPL_STDCALL 
GDALSetRasterNoDataValue( GDALRasterBandH hBand, double dfValue )

{
    return ((GDALRasterBand *) hBand)->SetNoDataValue( dfValue );
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

/**
 * Fetch the maximum value for this band.
 * 
 * For file formats that don't know this intrinsically, the maximum supported
 * value for the data type will generally be returned.  
 *
 * This method is the same as the C function GDALGetRasterMaximum().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if the
 * returned value is a tight maximum or not.  May be NULL (default).
 *
 * @return the maximum raster value (excluding no data pixels)
 */

double GDALRasterBand::GetMaximum( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = FALSE;

    switch( eDataType )
    {
      case GDT_Byte:
        return 255;

      case GDT_UInt16:
        return 65535;

      case GDT_Int16:
      case GDT_CInt16:
        return 32767;

      case GDT_Int32:
      case GDT_CInt32:
        return 2147483647.0;

      case GDT_UInt32:
        return 4294967295.0;

      case GDT_Float32:
      case GDT_CFloat32:
        return 4294967295.0; /* not actually accurate */

      case GDT_Float64:
      case GDT_CFloat64:
        return 4294967295.0; /* not actually accurate */

      default:
        return 4294967295.0; /* not actually accurate */
    }
}

/************************************************************************/
/*                        GDALGetRasterMaximum()                        */
/************************************************************************/

/**
 * @see GDALRasterBand::GetMaximum()
 */

double CPL_STDCALL 
GDALGetRasterMaximum( GDALRasterBandH hBand, int *pbSuccess )

{
    return ((GDALRasterBand *) hBand)->GetMaximum( pbSuccess );
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

/**
 * Fetch the minimum value for this band.
 * 
 * For file formats that don't know this intrinsically, the minimum supported
 * value for the data type will generally be returned.  
 *
 * This method is the same as the C function GDALGetRasterMinimum().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if the
 * returned value is a tight minimum or not.  May be NULL (default).
 *
 * @return the minimum raster value (excluding no data pixels)
 */

double GDALRasterBand::GetMinimum( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = FALSE;

    switch( eDataType )
    {
      case GDT_Byte:
        return 0;

      case GDT_UInt16:
        return 0;

      case GDT_Int16:
        return -32768;

      case GDT_Int32:
        return -2147483648.0;

      case GDT_UInt32:
        return 0;

      case GDT_Float32:
        return -4294967295.0; /* not actually accurate */

      case GDT_Float64:
        return -4294967295.0; /* not actually accurate */

      default:
        return -4294967295.0; /* not actually accurate */
    }
}

/************************************************************************/
/*                        GDALGetRasterMinimum()                        */
/************************************************************************/

/**
 * @see GDALRasterBand::GetMinimum()
 */

double CPL_STDCALL 
GDALGetRasterMinimum( GDALRasterBandH hBand, int *pbSuccess )

{
    return ((GDALRasterBand *) hBand)->GetMinimum( pbSuccess );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

/**
 * How should this band be interpreted as color?
 *
 * CV_Undefined is returned when the format doesn't know anything
 * about the color interpretation. 
 *
 * This method is the same as the C function 
 * GDALGetRasterColorInterpretation().
 *
 * @return color interpretation value for band.
 */

GDALColorInterp GDALRasterBand::GetColorInterpretation()

{
    return GCI_Undefined;
}

/************************************************************************/
/*                  GDALGetRasterColorInterpretation()                  */
/************************************************************************/

/**
 * @see GDALRasterBand::GetColorInterpretation()
 */

GDALColorInterp CPL_STDCALL 
GDALGetRasterColorInterpretation( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->GetColorInterpretation();
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

/**
 * Set color interpretation of a band.
 *
 * @param eColorInterp the new color interpretation to apply to this band.
 * 
 * @return CE_None on success or CE_Failure if method is unsupported by format.
 */

CPLErr GDALRasterBand::SetColorInterpretation( GDALColorInterp eColorInterp)

{
    (void) eColorInterp;
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SetColorInterpretation() not supported for this dataset." );
    return CE_Failure;
}

/************************************************************************/
/*                  GDALSetRasterColorInterpretation()                  */
/************************************************************************/

/**
 * @see GDALRasterBand::SetColorInterpretation()
 */

CPLErr CPL_STDCALL 
GDALSetRasterColorInterpretation( GDALRasterBandH hBand,
                                  GDALColorInterp eColorInterp )

{
    return ((GDALRasterBand *) hBand)->SetColorInterpretation(eColorInterp);
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

/**
 * Fetch the color table associated with band.
 *
 * If there is no associated color table, the return result is NULL.  The
 * returned color table remains owned by the GDALRasterBand, and can't
 * be depended on for long, nor should it ever be modified by the caller.
 *
 * This method is the same as the C function GDALGetRasterColorTable().
 *
 * @return internal color table, or NULL.
 */

GDALColorTable *GDALRasterBand::GetColorTable()

{
    return NULL;
}

/************************************************************************/
/*                      GDALGetRasterColorTable()                       */
/************************************************************************/

/**
 * @see GDALRasterBand::GetColorTable()
 */

GDALColorTableH CPL_STDCALL GDALGetRasterColorTable( GDALRasterBandH hBand )

{
    return (GDALColorTableH) ((GDALRasterBand *) hBand)->GetColorTable();
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

/**
 * Set the raster color table. 
 * 
 * The driver will make a copy of all desired data in the colortable.  It
 * remains owned by the caller after the call.
 *
 * This method is the same as the C function GDALSetRasterColorTable().
 *
 * @param poCT the color table to apply.
 *
 * @return CE_None on success, or CE_Failure on failure.  If the action is
 * unsupported by the driver, a value of CE_Failure is returned, but no
 * error is issued.
 */

CPLErr GDALRasterBand::SetColorTable( GDALColorTable * poCT )

{
    (void) poCT;
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SetColorTable() not supported for this dataset." );
    return CE_Failure;
}

/************************************************************************/
/*                      GDALSetRasterColorTable()                       */
/************************************************************************/

/**
 * @see GDALRasterBand::SetColorTable()
 */

CPLErr CPL_STDCALL 
GDALSetRasterColorTable( GDALRasterBandH hBand, GDALColorTableH hCT )

{
    return ((GDALRasterBand *) hBand)->SetColorTable( (GDALColorTable *) hCT );
}

/************************************************************************/
/*                       HasArbitraryOverviews()                        */
/************************************************************************/

/**
 * Check for arbitrary overviews.
 *
 * This returns TRUE if the underlying datastore can compute arbitrary 
 * overviews efficiently, such as is the case with OGDI over a network. 
 * Datastores with arbitrary overviews don't generally have any fixed
 * overviews, but the RasterIO() method can be used in downsampling mode
 * to get overview data efficiently.
 *
 * This method is the same as the C function GDALHasArbitraryOverviews(),
 *
 * @return TRUE if arbitrary overviews available (efficiently), otherwise
 * FALSE. 
 */

int GDALRasterBand::HasArbitraryOverviews()

{
    return FALSE;
}

/************************************************************************/
/*                     GDALHasArbitraryOverviews()                      */
/************************************************************************/

/**
 * @see GDALRasterBand::HasArbitraryOverviews()
 */

int CPL_STDCALL GDALHasArbitraryOverviews( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->HasArbitraryOverviews();
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

/**
 * Return the number of overview layers available.
 *
 * This method is the same as the C function GDALGetOverviewCount();
 *
 * @return overview count, zero if none.
 */

int GDALRasterBand::GetOverviewCount()

{
    if( poDS != NULL && poDS->oOvManager.IsInitialized() )
        return poDS->oOvManager.GetOverviewCount( nBand );
    else
        return 0;
}

/************************************************************************/
/*                        GDALGetOverviewCount()                        */
/************************************************************************/

/**
 * @see GDALRasterBand::GetOverviewCount()
 */

int CPL_STDCALL GDALGetOverviewCount( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->GetOverviewCount();
}


/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

/**
 * Fetch overview raster band object.
 *
 * This method is the same as the C function GDALGetOverview().
 * 
 * @param i overview index between 0 and GetOverviewCount()-1.
 * 
 * @return overview GDALRasterBand.
 */

GDALRasterBand * GDALRasterBand::GetOverview( int i )

{
    if( poDS != NULL && poDS->oOvManager.IsInitialized() )
        return poDS->oOvManager.GetOverview( nBand, i );
    else
        return NULL;
}

/************************************************************************/
/*                          GDALGetOverview()                           */
/************************************************************************/

/**
 * @see GDALRasterBand::GetOverview()
 */

GDALRasterBandH CPL_STDCALL GDALGetOverview( GDALRasterBandH hBand, int i )

{
    return (GDALRasterBandH) ((GDALRasterBand *) hBand)->GetOverview(i);
}

/************************************************************************/
/*                           BuildOverviews()                           */
/************************************************************************/

/**
 * Build raster overview(s)
 *
 * If the operation is unsupported for the indicated dataset, then 
 * CE_Failure is returned, and CPLGetLastErrorNo() will return 
 * CPLE_NotSupported.
 *
 * WARNING:  It is not possible to build overviews for a single band in
 * TIFF format, and thus this method does not work for TIFF format, or any
 * formats that use the default overview building in TIFF format.  Instead
 * it is necessary to build overviews on the dataset as a whole using
 * GDALDataset::BuildOverviews().  That makes this method pretty useless
 * from a practical point of view. 
 *
 * @param pszResampling one of "NEAREST", "AVERAGE" or "MODE" controlling
 * the downsampling method applied.
 * @param nOverviews number of overviews to build. 
 * @param panOverviewList the list of overview decimation factors to build. 
 * @param pfnProgress a function to call to report progress, or NULL.
 * @param pProgressData application data to pass to the progress function.
 *
 * @return CE_None on success or CE_Failure if the operation doesn't work. 
 */

CPLErr GDALRasterBand::BuildOverviews( const char * pszResampling, 
                                       int nOverviews, 
                                       int * panOverviewList, 
                                       GDALProgressFunc pfnProgress, 
                                       void * pProgressData )

{
    (void) pszResampling;
    (void) nOverviews;
    (void) panOverviewList;
    (void) pfnProgress;
    (void) pProgressData;

    CPLError( CE_Failure, CPLE_NotSupported,
              "BuildOverviews() not supported for this dataset." );
    
    return( CE_Failure );
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

/**
 * Fetch the raster value offset.
 *
 * This value (in combination with the GetScale() value) is used to
 * transform raw pixel values into the units returned by GetUnits().  
 * For example this might be used to store elevations in GUInt16 bands
 * with a precision of 0.1, and starting from -100. 
 * 
 * Units value = (raw value * scale) + offset
 * 
 * For file formats that don't know this intrinsically a value of zero
 * is returned. 
 *
 * This method is the same as the C function GDALGetRasterOffset().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if the
 * returned value is meaningful or not.  May be NULL (default).
 *
 * @return the raster offset.
 */

double GDALRasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = FALSE;

    return 0.0;
}

/************************************************************************/
/*                        GDALGetRasterOffset()                         */
/************************************************************************/

double CPL_STDCALL GDALGetRasterOffset( GDALRasterBandH hBand, int *pbSuccess )

{
    return ((GDALRasterBand *) hBand)->GetOffset( pbSuccess );
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

/**
 * Set scaling offset.
 *
 * Very few formats implement this method.   When not implemented it will
 * issue a CPLE_NotSupported error and return CE_Failure. 
 * 
 * @param dfNewOffset the new offset.
 *
 * @return CE_None or success or CE_Failure on failure. 
 */

CPLErr GDALRasterBand::SetOffset( double dfNewOffset )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SetOffset() not supported on this raster band." );
    
    return CE_Failure;
}

/************************************************************************/
/*                        GDALSetRasterOffset()                         */
/************************************************************************/

CPLErr CPL_STDCALL 
GDALSetRasterOffset( GDALRasterBandH hBand, double dfNewOffset )

{
    return ((GDALRasterBand *) hBand)->SetOffset( dfNewOffset );
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

/**
 * Fetch the raster value scale.
 *
 * This value (in combination with the GetOffset() value) is used to
 * transform raw pixel values into the units returned by GetUnits().  
 * For example this might be used to store elevations in GUInt16 bands
 * with a precision of 0.1, and starting from -100. 
 * 
 * Units value = (raw value * scale) + offset
 * 
 * For file formats that don't know this intrinsically a value of one
 * is returned. 
 *
 * This method is the same as the C function GDALGetRasterScale().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if the
 * returned value is meaningful or not.  May be NULL (default).
 *
 * @return the raster scale.
 */

double GDALRasterBand::GetScale( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = FALSE;

    return 1.0;
}

/************************************************************************/
/*                         GDALGetRasterScale()                         */
/************************************************************************/

double CPL_STDCALL GDALGetRasterScale( GDALRasterBandH hBand, int *pbSuccess )

{
    return ((GDALRasterBand *) hBand)->GetScale( pbSuccess );
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

/**
 * Set scaling ratio.
 *
 * Very few formats implement this method.   When not implemented it will
 * issue a CPLE_NotSupported error and return CE_Failure. 
 * 
 * @param dfNewScale the new scale.
 *
 * @return CE_None or success or CE_Failure on failure. 
 */

CPLErr GDALRasterBand::SetScale( double dfNewScale )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SetScale() not supported on this raster band." );
    
    return CE_Failure;
}

/************************************************************************/
/*                        GDALSetRasterScale()                          */
/************************************************************************/

CPLErr CPL_STDCALL 
GDALSetRasterScale( GDALRasterBandH hBand, double dfNewOffset )

{
    return ((GDALRasterBand *) hBand)->SetScale( dfNewOffset );
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

/**
 * Return raster unit type.
 *
 * Return a name for the units of this raster's values.  For instance, it
 * might be "m" for an elevation model in meters, or "ft" for feet.  If no 
 * units are available, a value of "" will be returned.  The returned string 
 * should not be modified, nor freed by the calling application.
 *
 * This method is the same as the C function GDALGetRasterUnitType(). 
 *
 * @return unit name string.
 */

const char *GDALRasterBand::GetUnitType()

{
    return "";
}

/************************************************************************/
/*                       GDALGetRasterUnitType()                        */
/************************************************************************/

/**
 * @see GDALRasterBand::GetUnitType()
 */

const char * CPL_STDCALL GDALGetRasterUnitType( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->GetUnitType();
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

/**
 * Set unit type.
 *
 * Set the unit type for a raster band.  Values should be one of
 * "" (the default indicating it is unknown), "m" indicating meters, 
 * or "ft" indicating feet, though other nonstandard values are allowed.
 *
 * @param pszNewValue the new unit type value.
 *
 * @return CE_None on success or CE_Failure if not succuessful, or 
 * unsupported.
 */

CPLErr GDALRasterBand::SetUnitType( const char *pszNewValue )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "SetUnitType() not supported on this raster band." );
    return CE_Failure;
}

/************************************************************************/
/*                              GetXSize()                              */
/************************************************************************/

/**
 * Fetch XSize of raster. 
 *
 * This method is the same as the C function GDALGetRasterBandXSize(). 
 *
 * @return the width in pixels of this band.
 */

int GDALRasterBand::GetXSize()

{
    return nRasterXSize;
}

/************************************************************************/
/*                       GDALGetRasterBandXSize()                       */
/************************************************************************/

/**
 * @see GDALRasterBand::GetXSize()
 */

int CPL_STDCALL GDALGetRasterBandXSize( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->GetXSize();
}

/************************************************************************/
/*                              GetYSize()                              */
/************************************************************************/

/**
 * Fetch YSize of raster. 
 *
 * This method is the same as the C function GDALGetRasterBandYSize(). 
 *
 * @return the height in pixels of this band.
 */

int GDALRasterBand::GetYSize()

{
    return nRasterYSize;
}

/************************************************************************/
/*                       GDALGetRasterBandYSize()                       */
/************************************************************************/

/**
 * @see GDALRasterBand::GetYSize()
 */

int CPL_STDCALL GDALGetRasterBandYSize( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->GetYSize();
}

/************************************************************************/
/*                              GetBand()                               */
/************************************************************************/

/**
 * Fetch the band number.
 *
 * This method returns the band that this GDALRasterBand objects represents
 * within it's dataset.  This method may return a value of 0 to indicate
 * GDALRasterBand objects without an apparently relationship to a dataset,
 * such as GDALRasterBands serving as overviews.
 *
 * This method is the same as the C function GDALGetBandNumber().
 *
 * @return band number (1+) or 0 if the band number isn't known.
 */

int GDALRasterBand::GetBand()

{
    return nBand;
}

/************************************************************************/
/*                         GDALGetBandNumber()                          */
/************************************************************************/

/**
 * @see GDALRasterBand::GetBand()
 */

int CPL_STDCALL GDALGetBandNumber( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->GetBand();
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

/**
 * Fetch the owning dataset handle.
 *
 * Note that some GDALRasterBands are not considered to be a part of a dataset,
 * such as overviews or other "freestanding" bands. 
 *
 * There is currently no C analog to this method.
 *
 * @return the pointer to the GDALDataset to which this band belongs, or
 * NULL if this cannot be determined.
 */

GDALDataset *GDALRasterBand::GetDataset()

{
    return poDS;
}

/************************************************************************/
/*                         GDALGetBandDataset()                         */
/************************************************************************/

/**
 * @see GDALRasterBand::GetDataset()
 */

GDALDatasetH CPL_STDCALL GDALGetBandDataset( GDALRasterBandH hBand )

{
    return (GDALDatasetH) ((GDALRasterBand *) hBand)->GetDataset();
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

/**
 * Compute raster histogram. 
 *
 * Note that the bucket size is (dfMax-dfMin) / nBuckets.  
 *
 * For example to compute a simple 256 entry histogram of eight bit data, 
 * the following would be suitable.  The unusual bounds are to ensure that
 * bucket boundaries don't fall right on integer values causing possible errors
 * due to rounding after scaling. 
<pre>
    int anHistogram[256];

    poBand->GetHistogram( -0.5, 255.5, 256, anHistogram, FALSE, FALSE, 
                          GDALDummyProgress, NULL );
</pre>
 *
 * Note that setting bApproxOK will generally result in a subsampling of the
 * file, and will utilize overviews if available.  It should generally 
 * produce a representative histogram for the data that is suitable for use
 * in generating histogram based luts for instance.  Generally bApproxOK is
 * much faster than an exactly computed histogram.
 *
 * @param dfMin the lower bound of the histogram.
 * @param dfMax the upper bound of the histogram.
 * @param nBuckets the number of buckets in panHistogram.
 * @param panHistogram array into which the histogram totals are placed.
 * @param bIncludeOutOfRange if TRUE values below the histogram range will
 * mapped into panHistogram[0], and values above will be mapped into 
 * panHistogram[nBuckets-1] otherwise out of range values are discarded.
 * @param bApproxOK TRUE if an approximate, or incomplete histogram OK.
 * @param pfnProgress function to report progress to completion. 
 * @param pProgressData application data to pass to pfnProgress. 
 *
 * @return CE_None on success, or CE_Failure if something goes wrong. 
 */

CPLErr GDALRasterBand::GetHistogram( double dfMin, double dfMax, 
                                     int nBuckets, int *panHistogram, 
                                     int bIncludeOutOfRange, int bApproxOK,
                                     GDALProgressFunc pfnProgress, 
                                     void *pProgressData )

{
    CPLAssert( pfnProgress != NULL );

/* -------------------------------------------------------------------- */
/*      If we have overviews, use them for the histogram.               */
/* -------------------------------------------------------------------- */
    if( bApproxOK && GetOverviewCount() > 0 )
    {
        double dfBestPixels = GetXSize() * GetYSize();
        GDALRasterBand *poBestOverview = NULL;
        
        for( int i = 0; i < GetOverviewCount(); i++ )
        {
            GDALRasterBand *poOverview = GetOverview(i);
            double         dfPixels;

            dfPixels = poOverview->GetXSize() * poOverview->GetYSize();
            if( dfPixels < dfBestPixels )
            {
                dfBestPixels = dfPixels;
                poBestOverview = poOverview;
            }
            
            if( poBestOverview != NULL )
                return poBestOverview->
                    GetHistogram( dfMin, dfMax, nBuckets, panHistogram, 
                                  bIncludeOutOfRange, bApproxOK, 
                                  pfnProgress, pProgressData );
        }
    }

/* -------------------------------------------------------------------- */
/*      Figure out the ratio of blocks we will read to get an           */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
    int         nSampleRate;
    double      dfScale;

    InitBlockInfo();
    
    if( bApproxOK )
        nSampleRate = 
            (int) MAX(1,sqrt((double) nBlocksPerRow * nBlocksPerColumn));
    else
        nSampleRate = 1;
    
    dfScale = nBuckets / (dfMax - dfMin);

/* -------------------------------------------------------------------- */
/*      Read the blocks, and add to histogram.                          */
/* -------------------------------------------------------------------- */
    memset( panHistogram, 0, sizeof(int) * nBuckets );
    for( int iSampleBlock = 0; 
         iSampleBlock < nBlocksPerRow * nBlocksPerColumn;
         iSampleBlock += nSampleRate )
    {
        double dfValue = 0.0, dfReal, dfImag;
        int  iXBlock, iYBlock, nXCheck, nYCheck;
        GDALRasterBlock *poBlock;

        if( !pfnProgress(iSampleBlock/((double)nBlocksPerRow*nBlocksPerColumn),
                         NULL, pProgressData ) )
            return CE_Failure;

        iYBlock = iSampleBlock / nBlocksPerRow;
        iXBlock = iSampleBlock - nBlocksPerRow * iYBlock;
        
        poBlock = GetLockedBlockRef( iXBlock, iYBlock );
        if( poBlock == NULL )
            return CE_Failure;
        
        if( (iXBlock+1) * nBlockXSize > GetXSize() )
            nXCheck = GetXSize() - iXBlock * nBlockXSize;
        else
            nXCheck = nBlockXSize;

        if( (iYBlock+1) * nBlockYSize > GetYSize() )
            nYCheck = GetYSize() - iYBlock * nBlockYSize;
        else
            nYCheck = nBlockYSize;

        /* this is a special case for a common situation */
        if( poBlock->GetDataType() == GDT_Byte
            && dfScale == 1.0 && (dfMin >= -0.5 && dfMin <= 0.5)
            && nYCheck == nBlockYSize && nXCheck == nBlockXSize
            && nBuckets == 256 )
        {
            int    nPixels = nXCheck * nYCheck;
            GByte  *pabyData = (GByte *) poBlock->GetDataRef();
            
            for( int i = 0; i < nPixels; i++ )
                panHistogram[pabyData[i]]++;

            poBlock->DropLock();
            continue; /* to next sample block */
        }

        /* this isn't the fastest way to do this, but is easier for now */
        for( int iY = 0; iY < nYCheck; iY++ )
        {
            for( int iX = 0; iX < nXCheck; iX++ )
            {
                int    iOffset = iX + iY * nBlockXSize;
                int    nIndex;

                switch( poBlock->GetDataType() )
                {
                  case GDT_Byte:
                    dfValue = ((GByte *) poBlock->GetDataRef())[iOffset];
                    break;

                  case GDT_UInt16:
                    dfValue = ((GUInt16 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Int16:
                    dfValue = ((GInt16 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_UInt32:
                    dfValue = ((GUInt32 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Int32:
                    dfValue = ((GInt32 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Float32:
                    dfValue = ((float *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Float64:
                    dfValue = ((double *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_CInt16:
                    dfReal = ((GInt16 *) poBlock->GetDataRef())[iOffset*2];
                    dfImag = ((GInt16 *) poBlock->GetDataRef())[iOffset*2+1];
                    dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                    break;
                  case GDT_CInt32:
                    dfReal = ((GInt32 *) poBlock->GetDataRef())[iOffset*2];
                    dfImag = ((GInt32 *) poBlock->GetDataRef())[iOffset*2+1];
                    dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                    break;
                  case GDT_CFloat32:
                    dfReal = ((float *) poBlock->GetDataRef())[iOffset*2];
                    dfImag = ((float *) poBlock->GetDataRef())[iOffset*2+1];
                    dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                    break;
                  case GDT_CFloat64:
                    dfReal = ((double *) poBlock->GetDataRef())[iOffset*2];
                    dfImag = ((double *) poBlock->GetDataRef())[iOffset*2+1];
                    dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                    break;
                  default:
                    CPLAssert( FALSE );
                    return CE_Failure;
                }
                
                nIndex = (int) floor((dfValue - dfMin) * dfScale);

                if( nIndex < 0 )
                {
                    if( bIncludeOutOfRange )
                        panHistogram[0]++;
                }
                else if( nIndex >= nBuckets )
                {
                    if( bIncludeOutOfRange )
                        panHistogram[nBuckets-1]++;
                }
                else
                {
                    panHistogram[nIndex]++;
                }
            }

        }

        poBlock->DropLock();
    }

    pfnProgress( 1.0, NULL, pProgressData );

    return CE_None;
}

/************************************************************************/
/*                       GDALGetRasterHistogram()                       */
/************************************************************************/

/**
 * @see GDALRasterBand::GetHistogram()
 */

CPLErr CPL_STDCALL 
GDALGetRasterHistogram( GDALRasterBandH hBand, 
                        double dfMin, double dfMax, 
                        int nBuckets, int *panHistogram, 
                        int bIncludeOutOfRange, int bApproxOK,
                        GDALProgressFunc pfnProgress, 
                        void *pProgressData )
    
{
    return ((GDALRasterBand *) hBand)->
        GetHistogram( dfMin, dfMax, nBuckets, panHistogram, 
                      bIncludeOutOfRange, bApproxOK,
                      pfnProgress, pProgressData );
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

/**
 * Fetch default raster histogram. 
 *
 * Note that the bucket size is (dfMax-dfMin) / nBuckets.  
 *
 * For example to compute a simple 256 entry histogram of eight bit data, 
 * the following would be suitable.  The unusual bounds are to ensure that
 * bucket boundaries don't fall right on integer values causing possible errors
 * due to rounding after scaling. 
<pre>
    int anHistogram[256];

    poBand->GetHistogram( -0.5, 255.5, 256, anHistogram, FALSE, FALSE, 
                          GDALDummyProgress, NULL );
</pre>
 *
 * Note that setting bApproxOK will generally result in a subsampling of the
 * file, and will utilize overviews if available.  It should generally 
 * produce a representative histogram for the data that is suitable for use
 * in generating histogram based luts for instance.  Generally bApproxOK is
 * much faster than an exactly computed histogram.
 *
 * @param dfMin the lower bound of the histogram.
 * @param dfMax the upper bound of the histogram.
 * @param nBuckets the number of buckets in panHistogram.
 * @param panHistogram array into which the histogram totals are placed.
 * @param bIncludeOutOfRange if TRUE values below the histogram range will
 * mapped into panHistogram[0], and values above will be mapped into 
 * panHistogram[nBuckets-1] otherwise out of range values are discarded.
 * @param bApproxOK TRUE if an approximate, or incomplete histogram OK.
 * @param pfnProgress function to report progress to completion. 
 * @param pProgressData application data to pass to pfnProgress. 
 *
 * @return CE_None on success, CE_Failure if something goes wrong, or 
 * CE_Warning if no default histogram is available.
 */

CPLErr 
    GDALRasterBand::GetDefaultHistogram( double *pdfMin, double *pdfMax, 
                                         int *pnBuckets, int **ppanHistogram, 
                                         int bForce,
                                         GDALProgressFunc pfnProgress, 
                                         void *pProgressData )

{
    if( !bForce )
        return CE_Warning;

    *pnBuckets = 256;

    if( GetRasterDataType() == GDT_Byte )
    {
        *pdfMin = -0.5;
        *pdfMax = 255.5;
    }
    else
    {
        CPLErr eErr;
        double dfHalfBucket;

        eErr = GetStatistics( TRUE, TRUE, pdfMin, pdfMax, NULL, NULL );
        dfHalfBucket = (*pdfMax - *pdfMin) / (2 * *pnBuckets);
        *pdfMin -= dfHalfBucket;
        *pdfMax += dfHalfBucket;

        if( eErr != CE_None )
            return eErr;
    }

    *ppanHistogram = (int *) VSICalloc(sizeof(int),*pnBuckets);
    if( *ppanHistogram == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "Out of memory in InitBlockInfo()." );
        return CE_Failure;
    }

    return GetHistogram( *pdfMin, *pdfMax, *pnBuckets, *ppanHistogram, 
                         TRUE, FALSE, pfnProgress, pProgressData );
}

/************************************************************************/
/*                      GDALGetDefaultHistogram()                       */
/************************************************************************/

CPLErr CPL_STDCALL GDALGetDefaultHistogram( GDALRasterBandH hBand, 
                                double *pdfMin, double *pdfMax, 
                                int *pnBuckets, int **ppanHistogram, 
                                int bForce,
                                GDALProgressFunc pfnProgress, 
                                void *pProgressData )

{
    return ((GDALRasterBand *) hBand)->GetDefaultHistogram( 
        pdfMin, pdfMax, pnBuckets, ppanHistogram, bForce, 
        pfnProgress, pProgressData );
}

/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

/**
 * Advise driver of upcoming read requests.
 *
 * Some GDAL drivers operate more efficiently if they know in advance what 
 * set of upcoming read requests will be made.  The AdviseRead() method allows
 * an application to notify the driver of the region of interest, 
 * and at what resolution the region will be read.  
 *
 * Many drivers just ignore the AdviseRead() call, but it can dramatically
 * accelerate access via some drivers.  
 *
 * @param nXOff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param nYOff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param nBufXSize the width of the buffer image into which the desired region
 * is to be read, or from which it is to be written.
 *
 * @param nBufYSize the height of the buffer image into which the desired
 * region is to be read, or from which it is to be written.
 *
 * @param eBufType the type of the pixel values in the pData data buffer.  The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param papszOptions a list of name=value strings with special control 
 * options.  Normally this is NULL.
 *
 * @return CE_Failure if the request is invalid and CE_None if it works or
 * is ignored. 
 */

CPLErr GDALRasterBand::AdviseRead( 
    int nXOff, int nYOff, int nXSize, int nYSize,
    int nBufXSize, int nBufYSize, GDALDataType eDT, char **papszOptions )

{
    return CE_None;
}

/************************************************************************/
/*                        GDALRasterAdviseRead()                        */
/************************************************************************/

CPLErr CPL_STDCALL 
GDALRasterAdviseRead( GDALRasterBandH hRB, 
                      int nXOff, int nYOff, int nXSize, int nYSize,
                      int nBufXSize, int nBufYSize, 
                      GDALDataType eDT, char **papszOptions )
    
{
    return ((GDALRasterBand *) hRB)->AdviseRead( nXOff, nYOff, nXSize, nYSize, 
                                                 nBufXSize, nBufYSize, eDT, 
                                                 papszOptions );
}

/************************************************************************/
/*                           GetStatistics()                            */
/************************************************************************/

/**
 * Fetch image statistics. 
 *
 * Returns the minimum, maximum, mean and standard deviation of all
 * pixel values in this band.  If approximate statistics are sufficient,
 * the bApproxOK flag can be set to true in which case overviews, or a
 * subset of image tiles may be used in computing the statistics.  
 *
 * If bForce is FALSE results will only be returned if it can be done 
 * quickly (ie. without scanning the data).  If bForce is FALSE and 
 * results cannot be returned efficiently, the method will return CE_Warning
 * but no warning will have been issued.   This is a non-standard use of
 * the CE_Warning return value to indicate "nothing done". 
 *
 * Note that file formats using PAM (Persistent Auxilary Metadata) services
 * will generally cache statistics in the .pam file allowing fast fetch
 * after the first request. 
 *
 * @param bApproxOK If TRUE statistics may be computed based on overviews
 * or a subset of all tiles. 
 * 
 * @param bForce If FALSE statistics will only be returned if it can
 * be done without rescanning the image. 
 *
 * @param pdfMin Location into which to load image minimum (may be NULL).
 *
 * @param pdfMax Location into which to load image maximum (may be NULL).-
 *
 * @param pdfMean Location into which to load image mean (may be NULL).
 *
 * @param pdfStdDev Location into which to load image standard deviation 
 * (may be NULL).
 *
 * @return CE_None on success, CE_Warning if no values returned, 
 * CE_Failure if an error occurs.
 */

CPLErr GDALRasterBand::GetStatistics( int bApproxOK, int bForce,
                                      double *pdfMin, double *pdfMax, 
                                      double *pdfMean, double *pdfStdDev )

{
    double       dfMin=0.0, dfMax=0.0;

/* -------------------------------------------------------------------- */
/*      Do we already have metadata items for the requested values?     */
/* -------------------------------------------------------------------- */
    if( (pdfMin == NULL || GetMetadataItem("STATISTICS_MINIMUM") != NULL)
     && (pdfMax == NULL || GetMetadataItem("STATISTICS_MAXIMUM") != NULL)
     && (pdfMean == NULL || GetMetadataItem("STATISTICS_MEAN") != NULL)
     && (pdfStdDev == NULL || GetMetadataItem("STATISTICS_STDDEV") != NULL) )
    {
        if( pdfMin != NULL )
            *pdfMin = atof(GetMetadataItem("STATISTICS_MINIMUM"));
        if( pdfMax != NULL )
            *pdfMax = atof(GetMetadataItem("STATISTICS_MAXIMUM"));
        if( pdfMean != NULL )
            *pdfMean = atof(GetMetadataItem("STATISTICS_MEAN"));
        if( pdfStdDev != NULL )
            *pdfStdDev = atof(GetMetadataItem("STATISTICS_STDDEV"));

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Does the driver already know the min/max?                       */
/* -------------------------------------------------------------------- */
    if( bApproxOK && pdfMean == NULL && pdfStdDev == NULL )
    {
        int          bSuccessMin, bSuccessMax;

        dfMin = GetMinimum( &bSuccessMin );
        dfMax = GetMaximum( &bSuccessMax );

        if( bSuccessMin && bSuccessMax )
        {
            if( pdfMin != NULL )
                *pdfMin = dfMin;
            if( pdfMax != NULL )
                *pdfMax = dfMax;
            return CE_None;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      If we have overview bands, use them for min/max.                */
/* -------------------------------------------------------------------- */
    if( bApproxOK )
    {
        GDALRasterBand *poBand;

        poBand = (GDALRasterBand *) GDALGetRasterSampleOverview( this, 2500 );

        if( poBand != this )
            return poBand->GetStatistics( bApproxOK, bForce, pdfMin, pdfMax, 
                                          pdfMean, pdfStdDev );
    }
    

    if( !bForce )
        return CE_Warning;

/* -------------------------------------------------------------------- */
/*      Figure out the ratio of blocks we will read to get an           */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
    int         nBlockXSize, nBlockYSize;
    int         nBlocksPerRow, nBlocksPerColumn;
    int         nSampleRate, nSampleCount = 0;
    int         bGotNoDataValue, bFirstValue = TRUE;
    double      dfNoDataValue, dfSum=0.0, dfSum2=0.0;

    dfNoDataValue = GetNoDataValue( &bGotNoDataValue );

    GetBlockSize( &nBlockXSize, &nBlockYSize );
    nBlocksPerRow = (GetXSize() + nBlockXSize - 1) / nBlockXSize;
    nBlocksPerColumn = (GetYSize() + nBlockYSize - 1) / nBlockYSize;

    if( bApproxOK )
        nSampleRate = 
            (int) MAX(1,sqrt((double) nBlocksPerRow * nBlocksPerColumn));
    else
        nSampleRate = 1;
    
    for( int iSampleBlock = 0; 
         iSampleBlock < nBlocksPerRow * nBlocksPerColumn;
         iSampleBlock += nSampleRate )
    {
        double dfValue = 0.0;
        int  iXBlock, iYBlock, nXCheck, nYCheck;
        GDALRasterBlock *poBlock;

        iYBlock = iSampleBlock / nBlocksPerRow;
        iXBlock = iSampleBlock - nBlocksPerRow * iYBlock;
        
        poBlock = GetLockedBlockRef( iXBlock, iYBlock );
        if( poBlock == NULL )
            continue;
        
        if( (iXBlock+1) * nBlockXSize > GetXSize() )
            nXCheck = GetXSize() - iXBlock * nBlockXSize;
        else
            nXCheck = nBlockXSize;

        if( (iYBlock+1) * nBlockYSize > GetYSize() )
            nYCheck = GetYSize() - iYBlock * nBlockYSize;
        else
            nYCheck = nBlockYSize;

        /* this isn't the fastest way to do this, but is easier for now */
        for( int iY = 0; iY < nYCheck; iY++ )
        {
            for( int iX = 0; iX < nXCheck; iX++ )
            {
                int    iOffset = iX + iY * nBlockXSize;

                switch( poBlock->GetDataType() )
                {
                  case GDT_Byte:
                    dfValue = ((GByte *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_UInt16:
                    dfValue = ((GUInt16 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Int16:
                    dfValue = ((GInt16 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_UInt32:
                    dfValue = ((GUInt32 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Int32:
                    dfValue = ((GInt32 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Float32:
                    dfValue = ((float *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Float64:
                    dfValue = ((double *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_CInt16:
                    dfValue = ((GInt16 *) poBlock->GetDataRef())[iOffset*2];
                    break;
                  case GDT_CInt32:
                    dfValue = ((GInt32 *) poBlock->GetDataRef())[iOffset*2];
                    break;
                  case GDT_CFloat32:
                    dfValue = ((float *) poBlock->GetDataRef())[iOffset*2];
                    break;
                  case GDT_CFloat64:
                    dfValue = ((double *) poBlock->GetDataRef())[iOffset*2];
                    break;
                  default:
                    CPLAssert( FALSE );
                }
                
                if( bGotNoDataValue && dfValue == dfNoDataValue )
                    continue;

                if( bFirstValue )
                {
                    dfMin = dfMax = dfValue;
                    bFirstValue = FALSE;
                }
                else
                {
                    dfMin = MIN(dfMin,dfValue);
                    dfMax = MAX(dfMax,dfValue);
                }

                dfSum += dfValue;
                dfSum2 += dfValue * dfValue;

                nSampleCount++;
            }
        }

        poBlock->DropLock();
    }

/* -------------------------------------------------------------------- */
/*      Save computed information.                                      */
/* -------------------------------------------------------------------- */
    double dfMean = dfSum / nSampleCount;
    double dfStdDev = sqrt((dfSum2 / nSampleCount) - (dfMean * dfMean));

    if( nSampleCount > 1 )
        SetStatistics( dfMin, dfMax, dfMean, dfStdDev );

/* -------------------------------------------------------------------- */
/*      Record results.                                                 */
/* -------------------------------------------------------------------- */
    if( pdfMin != NULL )
        *pdfMin = dfMin;
    if( pdfMax != NULL )
        *pdfMax = dfMax;

    if( pdfMean != NULL )
        *pdfMean = dfMean;

    if( pdfStdDev != NULL )
        *pdfStdDev = dfStdDev;

    if( nSampleCount > 0 )
        return CE_None;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to compute statistics, no valid pixels found in sampling." );
        return CE_Failure;
    }
}

/************************************************************************/
/*                      GDALGetRasterStatistics()                       */
/************************************************************************/

CPLErr CPL_STDCALL GDALGetRasterStatistics( 
        GDALRasterBandH hBand, int bApproxOK, int bForce, 
        double *pdfMin, double *pdfMax, double *pdfMean, double *pdfStdDev )

{
    return ((GDALRasterBand *) hBand)->GetStatistics( 
        bApproxOK, bForce, pdfMin, pdfMax, pdfMean, pdfStdDev );
}

/************************************************************************/
/*                           SetStatistics()                            */
/************************************************************************/

CPLErr GDALRasterBand::SetStatistics( double dfMin, double dfMax, 
                                      double dfMean, double dfStdDev )

{
    char szValue[128];

    sprintf( szValue, "%.14g", dfMin );
    SetMetadataItem( "STATISTICS_MINIMUM", szValue );

    sprintf( szValue, "%.14g", dfMax );
    SetMetadataItem( "STATISTICS_MAXIMUM", szValue );

    sprintf( szValue, "%.14g", dfMean );
    SetMetadataItem( "STATISTICS_MEAN", szValue );

    sprintf( szValue, "%.14g", dfStdDev );
    SetMetadataItem( "STATISTICS_STDDEV", szValue );

    return CE_None;
}

/************************************************************************/
/*                        SetDefaultHistogram()                         */
/************************************************************************/

CPLErr GDALRasterBand::SetDefaultHistogram( double dfMin, double dfMax, 
                                            int nBuckets, int *panHistogram )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SetDefaultHistogram() not implemented for this format." );

    return CE_Failure;
}

/************************************************************************/
/*                      GDALSetDefaultHistogram()                       */
/************************************************************************/

CPLErr CPL_STDCALL GDALSetDefaultHistogram( GDALRasterBandH hBand, 
                                            double dfMin, double dfMax, 
                                            int nBuckets, int *panHistogram )

{
    return ((GDALRasterBand *) hBand)->SetDefaultHistogram(
        dfMin, dfMax, nBuckets, panHistogram );
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

/**
 * Fetch default Raster Attribute Table.
 *
 * A RAT will be returned if there is a default one associated with the
 * band, otherwise NULL is returned.  The returned RAT is owned by the
 * band and should not be deleted, or altered by the application. 
 * 
 * @return NULL, or a pointer to an internal RAT owned by the band.
 */

const GDALRasterAttributeTable *GDALRasterBand::GetDefaultRAT()

{
    return NULL;
}

/************************************************************************/
/*                         GDALGetDefaultRAT()                          */
/************************************************************************/

GDALRasterAttributeTableH CPL_STDCALL GDALGetDefaultRAT( GDALRasterBandH hBand)

{
    return (GDALRasterAttributeTableH) 
        ((GDALRasterBand *) hBand)->GetDefaultRAT();
}

/************************************************************************/
/*                           SetDefaultRAT()                            */
/************************************************************************/

/**
 * Set default Raster Attribute Table.
 *
 * Associates a default RAT with the band.  If not implemented for the
 * format a CPLE_NotSupported error will be issued.  If successful a copy
 * of the RAT is made, the original remains owned by the caller.
 *
 * @param poRAT the RAT to assign to the band.
 *
 * @return CE_None on success or CE_Failure if unsupported or otherwise 
 * failing.
 */

CPLErr GDALRasterBand::SetDefaultRAT( const GDALRasterAttributeTable *poRAT )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SetDefaultRAT() not implemented for this format." );

    return CE_Failure;
}

/************************************************************************/
/*                         GDALSetDefaultRAT()                          */
/************************************************************************/

CPLErr CPL_STDCALL GDALSetDefaultRAT( GDALRasterBandH hBand,
                                      GDALRasterAttributeTableH hRAT )

{
    return ((GDALRasterBand *) hBand)->SetDefaultRAT( 
        (GDALRasterAttributeTable *) hRAT );
}

