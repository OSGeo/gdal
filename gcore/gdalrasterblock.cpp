/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALRasterBlock class and related global 
 *           raster block cache management.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 1998, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.16  2004/04/07 13:15:57  warmerda
 * Ensure that GDALSetCacheMax() keeps flushing till the new limit is
 * honoured. http://bugzilla.remotesensing.org/show_bug.cgi?id=542
 *
 * Revision 1.15  2004/04/06 19:16:16  dron
 * Remove GDALRasterBlock::IsCached() method in favor
 * of GDALRasterBand::IsBlockCached().
 *
 * Revision 1.14  2004/03/19 05:17:42  warmerda
 * Increase default cachesize to 10MB.
 *
 * Revision 1.13  2004/03/15 08:33:38  warmerda
 * Use CPLGetConfigOption() for cache max.
 *
 * Revision 1.12  2003/07/27 11:01:01  dron
 * GDALRasterBlock::IsCached() method added.
 *
 * Revision 1.11  2003/04/25 19:48:16  warmerda
 * added block locking to ensure in-use blocks arent flushed
 *
 * Revision 1.10  2003/02/21 20:07:55  warmerda
 * update header
 *
 * Revision 1.9  2003/01/28 16:51:24  warmerda
 * document cache functions
 *
 * Revision 1.8  2002/07/09 20:33:12  warmerda
 * expand tabs
 *
 * Revision 1.7  2001/09/27 16:33:41  warmerda
 * fixed problems with blocks larger than 2GB/8
 *
 * Revision 1.6  2001/07/18 04:04:30  warmerda
 * added CPL_CVSID
 *
 * Revision 1.5  2001/06/22 21:00:06  warmerda
 * fixed support for caching override by environment variable
 *
 * Revision 1.4  2001/06/22 20:09:13  warmerda
 * added GDAL_CACHEMAX environment variable support
 *
 * Revision 1.3  2000/03/31 13:42:49  warmerda
 * added debugging code
 *
 * Revision 1.2  2000/03/24 00:09:05  warmerda
 * rewrote cache management
 *
 * Revision 1.1  1998/12/31 18:52:58  warmerda
 * New
 *
 */

#include "gdal_priv.h"

CPL_CVSID("$Id$");

static int nTileAgeTicker = 0; 
static int bCacheMaxInitialized = FALSE;
static int nCacheMax = 10 * 1024*1024;
static int nCacheUsed = 0;

static GDALRasterBlock   *poOldest = NULL;    /* tail */
static GDALRasterBlock   *poNewest = NULL;    /* head */


/************************************************************************/
/*                          GDALSetCacheMax()                           */
/************************************************************************/

/**
 * Set maximum cache memory.
 *
 * This function sets the maximum amount of memory that GDAL is permitted
 * to use for GDALRasterBlock caching.
 *
 * @param nNewSize the maximum number of bytes for caching.  Maximum is 2GB.
 */

void GDALSetCacheMax( int nNewSize )

{
    nCacheMax = nNewSize;

/* -------------------------------------------------------------------- */
/*      Flush blocks till we are under the new limit or till we         */
/*      can't seem to flush anymore.                                    */
/* -------------------------------------------------------------------- */
    while( nCacheUsed > nCacheMax )
    {
        int nOldCacheUsed = nCacheUsed;

        GDALFlushCacheBlock();

        if( nCacheUsed == nOldCacheUsed )
            break;
    }
}

/************************************************************************/
/*                          GDALGetCacheMax()                           */
/************************************************************************/

/**
 * Get maximum cache memory.
 *
 * Gets the maximum amount of memory available to the GDALRasterBlock
 * caching system for caching GDAL read/write imagery. 
 *
 * @return maximum in bytes. 
 */

int GDALGetCacheMax()
{
    if( !bCacheMaxInitialized )
    {
        if( CPLGetConfigOption("GDAL_CACHEMAX",NULL) != NULL )
        {
            nCacheMax = atoi(CPLGetConfigOption("GDAL_CACHEMAX","10"));
            if( nCacheMax < 1000 )
                nCacheMax *= 1024 * 1024;
        }
        bCacheMaxInitialized = TRUE;
    }
    
    return nCacheMax;
}

/************************************************************************/
/*                          GDALGetCacheUsed()                          */
/************************************************************************/

/**
 * Get cache memory used.
 *
 * @return the number of bytes of memory currently in use by the 
 * GDALRasterBlock memory caching.
 */

int GDALGetCacheUsed()
{
    return nCacheUsed;
}

/************************************************************************/
/*                        GDALFlushCacheBlock()                         */
/*                                                                      */
/*      The workhorse of cache management!                              */
/************************************************************************/

int GDALFlushCacheBlock()

{
    return GDALRasterBlock::FlushCacheBlock();
}

/************************************************************************/
/*                          FlushCacheBlock()                           */
/************************************************************************/

int GDALRasterBlock::FlushCacheBlock()

{
    GDALRasterBlock *poTarget = poOldest;

    while( poTarget != NULL && poTarget->GetLockCount() > 0 ) 
        poTarget = poTarget->poPrevious;

    if( poTarget != NULL )
    {
        poTarget->GetBand()->FlushBlock( poTarget->GetXOff(), 
                                         poTarget->GetYOff() );
        return TRUE;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                          GDALRasterBlock()                           */
/************************************************************************/

GDALRasterBlock::GDALRasterBlock( GDALRasterBand *poBandIn, 
                                  int nXOffIn, int nYOffIn )

{
    poBand = poBandIn;

    poBand->GetBlockSize( &nXSize, &nYSize );
    eType = poBand->GetRasterDataType();
    pData = NULL;
    bDirty = FALSE;
    nLockCount = 0;

    poNext = poPrevious = NULL;

    nXOff = nXOffIn;
    nYOff = nYOffIn;
}

/************************************************************************/
/*                          ~GDALRasterBlock()                          */
/************************************************************************/

GDALRasterBlock::~GDALRasterBlock()

{
    if( pData != NULL )
    {
        int nSizeInBytes;

        VSIFree( pData );

        nSizeInBytes = (nXSize * nYSize * GDALGetDataTypeSize(eType)+7)/8;
        nCacheUsed -= nSizeInBytes;
    }

    if( poOldest == this )
        poOldest = poPrevious;

    if( poNewest == this )
    {
        poNewest = poNext;
    }

    if( poPrevious != NULL )
        poPrevious->poNext = poNext;

    if( poNext != NULL )
        poNext->poPrevious = poPrevious;

    CPLAssert( nLockCount == 0 );

#ifdef ENABLE_DEBUG
    Verify();
#endif

    nAge = -1;
}

/************************************************************************/
/*                               Verify()                               */
/************************************************************************/

void GDALRasterBlock::Verify()

{
    CPLAssert( (poNewest == NULL && poOldest == NULL)
               || (poNewest != NULL && poOldest != NULL) );

    if( poNewest != NULL )
    {
        CPLAssert( poNewest->poPrevious == NULL );
        CPLAssert( poOldest->poNext == NULL );
        

        for( GDALRasterBlock *poBlock = poNewest; 
             poBlock != NULL;
             poBlock = poBlock->poNext )
        {
            if( poBlock->poPrevious )
            {
                CPLAssert( poBlock->poPrevious->poNext == poBlock );
            }

            if( poBlock->poNext )
            {
                CPLAssert( poBlock->poNext->poPrevious == poBlock );
            }
        }
    }
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

CPLErr GDALRasterBlock::Write()

{
    if( !GetDirty() )
        return CE_None;

    if( poBand == NULL )
        return CE_Failure;

    MarkClean();

    return poBand->IWriteBlock( nXOff, nYOff, pData );
}

/************************************************************************/
/*                               Touch()                                */
/************************************************************************/

void GDALRasterBlock::Touch()

{
    nAge = nTileAgeTicker++;

    if( poNewest == this )
        return;

    if( poOldest == this )
        poOldest = this->poPrevious;
    
    if( poPrevious != NULL )
        poPrevious->poNext = poNext;

    if( poNext != NULL )
        poNext->poPrevious = poPrevious;

    poPrevious = NULL;
    poNext = poNewest;

    if( poNewest != NULL )
    {
        CPLAssert( poNewest->poPrevious == NULL );
        poNewest->poPrevious = this;
    }
    poNewest = this;
    
    if( poOldest == NULL )
    {
        CPLAssert( poPrevious == NULL && poNext == NULL );
        poOldest = this;
    }
#ifdef ENABLE_DEBUG
    Verify();
#endif
}

/************************************************************************/
/*                            Internalize()                             */
/************************************************************************/

CPLErr GDALRasterBlock::Internalize()

{
    void        *pNewData;
    int         nSizeInBytes;
    int         nCurCacheMax = GDALGetCacheMax();

    nSizeInBytes = nXSize * nYSize * (GDALGetDataTypeSize(eType) / 8);

    pNewData = VSIMalloc( nSizeInBytes );
    if( pNewData == NULL )
        return( CE_Failure );

    if( pData != NULL )
        memcpy( pNewData, pData, nSizeInBytes );
    
    pData = pNewData;

/* -------------------------------------------------------------------- */
/*      Flush old blocks if we are nearing our memory limit.            */
/* -------------------------------------------------------------------- */
    AddLock(); /* don't flush this block! */

    nCacheUsed += nSizeInBytes;
    while( nCacheUsed > nCurCacheMax )
    {
        int nOldCacheUsed = nCacheUsed;

        GDALFlushCacheBlock();

        if( nCacheUsed == nOldCacheUsed )
            break;
    }

    DropLock();

/* -------------------------------------------------------------------- */
/*      Add this block to the list.                                     */
/* -------------------------------------------------------------------- */
    Touch();

    return( CE_None );
}

/************************************************************************/
/*                             MarkDirty()                              */
/************************************************************************/

void GDALRasterBlock::MarkDirty()

{
    bDirty = TRUE;
}


/************************************************************************/
/*                             MarkClean()                              */
/************************************************************************/

void GDALRasterBlock::MarkClean()

{
    bDirty = FALSE;
}

