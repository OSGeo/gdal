/******************************************************************************
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
 * gdalrasterband.cpp
 *
 * The GDALRasterBand class.
 *
 * Note that the GDALRasterBand class is normally just used as a base class
 * for format specific band classes. 
 * 
 * $Log$
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
 *
 */

#include "gdal_priv.h"

/************************************************************************/
/*                           GDALRasterBand()                           */
/************************************************************************/

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

GDALRasterBand::~GDALRasterBand()

{
    FlushCache();
    
    CPLFree( papoBlocks );
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

CPLErr GDALRasterBand::RasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff,
                                 int nXSize, int nYSize,
                                 void * pData,
                                 int nBufXSize, int nBufYSize,
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

GDALRasterBlock * GDALRasterBand::GetBlockRef( int nBlockXOff,
                                               int nBlockYOff )

{
    int		nBlockIndex;

    InitBlockInfo();
    
/* -------------------------------------------------------------------- */
/*      Validate the request                                            */
/* -------------------------------------------------------------------- */
    if( nBlockXOff < 0 || nBlockXOff >= nBlocksPerRow )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nBlockXOff value (%d) in "
                  	"GDALRasterBand::GetBlockRef()\n",
                  nBlockXOff );

        return( NULL );
    }

    if( nBlockYOff < 0 || nBlockYOff >= nBlocksPerColumn )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nBlockYOff value (%d) in "
                  	"GDALRasterBand::GetBlockRef()\n",
                  nBlockYOff );

        return( NULL );
    }

/* -------------------------------------------------------------------- */
/*      If the block isn't already in the cache, we will need to        */
/*      create it, read into it, and adopt it.  Adopting it may         */
/*      flush an old tile from the cache.                               */
/* -------------------------------------------------------------------- */
    nBlockIndex = nBlockXOff + nBlockYOff * nBlocksPerRow;
    
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

        if( IReadBlock(nBlockXOff,nBlockYOff,poBlock->GetDataRef()) != CE_None)
        {
            delete poBlock;
            return( NULL );
        }

        AdoptBlock( nBlockXOff, nBlockYOff, poBlock );
    }

/* -------------------------------------------------------------------- */
/*      Every read access updates the last touched time.                */
/* -------------------------------------------------------------------- */
    if( papoBlocks[nBlockIndex] != NULL )
        papoBlocks[nBlockIndex]->Touch();

    return( papoBlocks[nBlockIndex] );
}
