/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Base class for format specific band class implementation.  This
 *           base class provides default implementation for many methods.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 **********************************************************************
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
 * $Log$
 * Revision 1.6  1999/10/21 13:24:37  warmerda
 * Fixed some build breaking variable name differences.
 *
 * Revision 1.5  1999/10/01 14:44:02  warmerda
 * added documentation
 *
 * Revision 1.4  1998/12/31 18:54:25  warmerda
 * Implement initial GDALRasterBlock support, and block cache
 *
 * Revision 1.3  1998/12/06 22:17:09  warmerda
 * Fill out rasterio support.
 *
 * Revision 1.2  1998/12/06 02:52:08  warmerda
 * Added new methods, and C cover functions.
 *
 * Revision 1.1  1998/12/03 18:32:01  warmerda
 * New
 */

#include "gdal_priv.h"

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

    nBlocksPerRow = 0;
    nBlocksPerColumn = 0;

    nLoadedBlocks = 0;
    nMaxLoadableBlocks = 0;

    papoBlocks = NULL;
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
 * @param nBufXSize the width of the buffer into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param nBufYSize the height of the buffer into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param eBufType the type of the pixel values in the pData data buffer.  The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline.  If defaulted
 * the size of the datatype eBufType is used.
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
/*      Do some argument checking.                                      */
/* -------------------------------------------------------------------- */
    /* notdef: to fill in later */

    
/* -------------------------------------------------------------------- */
/*      Call the format specific function.                              */
/* -------------------------------------------------------------------- */
    return( IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                       pData, nBufXSize, nBufYSize, eBufType,
                       nPixelSpace, nLineSpace ) );
}

/************************************************************************/
/*                            GDALRasterIO()                            */
/************************************************************************/

CPLErr GDALRasterIO( GDALRasterBandH hBand, GDALRWFlag eRWFlag,
                     int nXOff, int nYOff,
                     int nXSize, int nYSize,
                     void * pData,
                     int nBufXSize, int nBufYSize,
                     GDALDataType eBufType,
                     int nPixelSpace,
                     int nLineSpace )

{
    GDALRasterBand	*poBand = (GDALRasterBand *) hBand;

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
     int	nXBlocks, nYBlocks, nXBlockSize, nYBlockSize;
     int	iXBlock, iYBlock;

     memset( panHistogram, 0, sizeof(int) * 256 );

     CPLAssert( poBand->GetRasterDataType() == GDT_Byte );

     poBand->GetBlockSize( &nXBlockSize, &nYBlockSize );
     nXBlocks = (poBand->GetXSize() + nXBlockSize - 1) / nXBlockSize;
     nYBlocks = (poBand->GetYSize() + nYBlockSize - 1) / nYBlockSize;

     pabyData = (GByte *) CPLMalloc(nXBlockSize * nYBlockSize);

     for( iYBlock = 0; iYBlock < nYBlocks; iYBlock++ )
     {
         for( iXBlock = 0; iXBlock < nxBlocks; iXBlock++ )
         {
             int	nXValid, nYValid;
             
             poBand->ReadBlock( iXBlock, iYBlock, pabyData );

             // Compute the portion of the block that is valid
             // for partial edge blocks.
             if( iXBlock * nXBlockSize > poBand->GetXSize() )
                 nXValid = poBand->GetXSize() - iXBlock * nXBlockSize;
             else
                 nXValid = nXBlockSize;

             if( iYBlock * nYBlockSize > poBand->GetYSize() )
                 nYValid = poBand->GetXSize() - iYBlock * nYBlockSize;
             else
                 nYValid = nYBlockSize;

             // Collect the histogram counts.
             for( int iY = 0; iY < nXValid; iY++ )
             {
                 for( int iX = 0; iX < nXValid; iX++ )
                 {
                     pabyHistogram[pabyData[iX + iY * nXBlockSize]] += 1;
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
        || nXBlockOff*nBlockXSize >= poDS->GetRasterXSize() )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nXBlockOff value (%d) in "
                  	"GDALRasterBand::ReadBlock()\n",
                  nXBlockOff );

        return( CE_Failure );
    }

    if( nYBlockOff < 0
        || nYBlockOff*nBlockYSize >= poDS->GetRasterYSize() )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nYBlockOff value (%d) in "
                  	"GDALRasterBand::ReadBlock()\n",
                  nYBlockOff );

        return( CE_Failure );
    }
    
/* -------------------------------------------------------------------- */
/*      Invoke underlying implementation method.                        */
/* -------------------------------------------------------------------- */
    return( IReadBlock( nXBlockOff, nYBlockOff, pImage ) );
}

/************************************************************************/
/*                           GDALReadBlock()                            */
/************************************************************************/

CPLErr GDALReadBlock( GDALRasterBandH hBand, int nXOff, int nYOff,
                      void * pData )

{
    GDALRasterBand	*poBand = (GDALRasterBand *) hBand;

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
        || nXBlockOff*nBlockXSize >= poDS->GetRasterXSize() )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nXBlockOff value (%d) in "
                  	"GDALRasterBand::WriteBlock()\n",
                  nXBlockOff );

        return( CE_Failure );
    }

    if( nYBlockOff < 0
        || nYBlockOff*nBlockYSize >= poDS->GetRasterYSize() )
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
    
/* -------------------------------------------------------------------- */
/*      Invoke underlying implementation method.                        */
/* -------------------------------------------------------------------- */
    return( IWriteBlock( nXBlockOff, nYBlockOff, pImage ) );
}

/************************************************************************/
/*                           GDALWriteBlock()                           */
/************************************************************************/

CPLErr GDALWriteBlock( GDALRasterBandH hBand, int nXOff, int nYOff,
                       void * pData )

{
    GDALRasterBand	*poBand = (GDALRasterBand *) hBand;

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

GDALDataType GDALGetRasterDataType( GDALRasterBandH hBand )

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

void GDALGetBlockSize( GDALRasterBandH hBand, int * pnXSize, int * pnYSize )

{
    GDALRasterBand	*poBand = (GDALRasterBand *) hBand;

    poBand->GetBlockSize( pnXSize, pnYSize );
}

/************************************************************************/
/*                           InitBlockInfo()                            */
/************************************************************************/

void GDALRasterBand::InitBlockInfo()

{
    if( papoBlocks != NULL )
        return;

    CPLAssert( nBlockXSize > 0 && nBlockYSize > 0 );
    
    nBlocksPerRow = (poDS->GetRasterXSize()+nBlockXSize-1) / nBlockXSize;
    nBlocksPerColumn = (poDS->GetRasterYSize()+nBlockYSize-1) / nBlockYSize;
    
    papoBlocks = (GDALRasterBlock **)
        CPLCalloc( sizeof(void*), nBlocksPerRow * nBlocksPerColumn );

/* -------------------------------------------------------------------- */
/*      Don't override caching info if the subclass has already set     */
/*      it.  Eventually I imagine the application should be mostly      */
/*      in control of this.                                             */
/* -------------------------------------------------------------------- */
    if( nMaxLoadableBlocks < 1 )
    {
        nMaxLoadableBlocks = nBlocksPerRow;
    }
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

CPLErr GDALRasterBand::AdoptBlock( int nBlockXOff, int nBlockYOff,
                                   GDALRasterBlock * poBlock )

{
    int		nBlockIndex;
    
    InitBlockInfo();
    
    CPLAssert( nBlockXOff >= 0 && nBlockXOff < nBlocksPerRow );
    CPLAssert( nBlockYOff >= 0 && nBlockYOff < nBlocksPerColumn );

    nBlockIndex = nBlockXOff + nBlockYOff * nBlocksPerRow;
    if( papoBlocks[nBlockIndex] == poBlock )
        return( CE_None );

    if( papoBlocks[nBlockIndex] != NULL )
        FlushBlock( nBlockXOff, nBlockYOff );

    papoBlocks[nBlockIndex] = poBlock;
    poBlock->Touch();

    nLoadedBlocks++;
    if( nLoadedBlocks > nMaxLoadableBlocks )
        FlushBlock();

    return( CE_None );
}

/************************************************************************/
/*                             FlushCache()                             */
/*                                                                      */
/*      Clear all cached blocks out of this bands cache.                */
/************************************************************************/

CPLErr GDALRasterBand::FlushCache()

{
    CPLErr	eErr = CE_None;

    while( nLoadedBlocks > 0 && eErr == CE_None )
    {
        eErr = FlushBlock();
    }
    
    return( eErr );
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

CPLErr GDALRasterBand::FlushBlock( int nBlockXOff, int nBlockYOff )

{
    int		nBlockIndex;
    GDALRasterBlock *poBlock;
    CPLErr	eErr = CE_None;
        
    InitBlockInfo();
    
/* -------------------------------------------------------------------- */
/*      Select a block if none indicated.                               */
/*                                                                      */
/*      Currently we scan all possible blocks, but for efficiency in    */
/*      cases with many blocks we should eventually modify the          */
/*      GDALRasterBand to keep a linked list of blocks by age.          */
/* -------------------------------------------------------------------- */
    if( nBlockXOff == -1 || nBlockYOff == -1 )
    {
        int		i, nBlocks;
        int		nOldestAge = 0x7fffffff;
        int		nOldestBlock = -1;

        nBlocks = nBlocksPerRow * nBlocksPerColumn;

        for( i = 0; i < nBlocks; i++ )
        {
            if( papoBlocks[i] != NULL
                && papoBlocks[i]->GetAge() < nOldestAge )
            {
                nOldestAge = papoBlocks[i]->GetAge();
                nOldestBlock = i;
            }
        }

        if( nOldestBlock == -1 )
            return( CE_None );

        nBlockXOff = nOldestBlock % nBlocksPerRow;
        nBlockYOff = (nOldestBlock - nBlockXOff) / nBlocksPerRow;
    }

/* -------------------------------------------------------------------- */
/*      Validate                                                        */
/* -------------------------------------------------------------------- */
    CPLAssert( nBlockXOff >= 0 && nBlockXOff < nBlocksPerRow );
    CPLAssert( nBlockYOff >= 0 && nBlockYOff < nBlocksPerColumn );

    nBlockIndex = nBlockXOff + nBlockYOff * nBlocksPerRow;
    poBlock = papoBlocks[nBlockIndex];
    if( poBlock == NULL )
        return( CE_None );

/* -------------------------------------------------------------------- */
/*      Remove, and update count.                                       */
/* -------------------------------------------------------------------- */
    papoBlocks[nBlockIndex] = NULL;
    nLoadedBlocks--;

    CPLAssert( nLoadedBlocks >= 0 );
    
/* -------------------------------------------------------------------- */
/*      Is the target block dirty?  If so we need to write it.          */
/* -------------------------------------------------------------------- */
    if( poBlock->GetDirty() )
    {
        eErr = IWriteBlock( nBlockXOff, nBlockYOff, poBlock->GetDataRef() );
        poBlock->MarkClean();
    }

/* -------------------------------------------------------------------- */
/*      Deallocate the block;                                           */
/* -------------------------------------------------------------------- */
    delete poBlock;

    return( eErr );
}


/************************************************************************/
/*                            GetBlockRef()                             */
/************************************************************************/

/**
 * Fetch a pointer to an internally cached raster block.
 *
 * Note that calling GetBlockRef() on a previously uncached band will
 * enable caching.
 * 
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth. 
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @return pointer to the block object, or NULL on failure.
 */

GDALRasterBlock * GDALRasterBand::GetBlockRef( int nXBlockOff,
                                               int nYBlockOff )

{
    int		nBlockIndex;

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
/*      If the block isn't already in the cache, we will need to        */
/*      create it, read into it, and adopt it.  Adopting it may         */
/*      flush an old tile from the cache.                               */
/* -------------------------------------------------------------------- */
    nBlockIndex = nXBlockOff + nYBlockOff * nBlocksPerRow;
    
    if( papoBlocks[nBlockIndex] == NULL )
    {
        GDALRasterBlock	*poBlock;
        
        poBlock = new GDALRasterBlock( nBlockXSize, nBlockYSize,
                                       eDataType, NULL );

        /* allocate data space */
        if( poBlock->Internalize() != CE_None )
        {
            delete poBlock;

            return( NULL );
        }

        if( IReadBlock(nXBlockOff,nYBlockOff,poBlock->GetDataRef()) != CE_None)
        {
            delete poBlock;
            return( NULL );
        }

        AdoptBlock( nXBlockOff, nYBlockOff, poBlock );
    }

/* -------------------------------------------------------------------- */
/*      Every read access updates the last touched time.                */
/* -------------------------------------------------------------------- */
    if( papoBlocks[nBlockIndex] != NULL )
        papoBlocks[nBlockIndex]->Touch();

    return( papoBlocks[nBlockIndex] );
}

/************************************************************************/
/*                             GetAccess()                              */
/************************************************************************/

/**
 * Find out if we have update permission for this band.
 *
 * @return Either GA_Update or GA_ReadOnly.
 */

GDALAccess GDALRasterBand::GetAccess()

{
    return eAccess;
}
