/******************************************************************************
 *
 * Project:  Raster Matrix Format
 * Purpose:  Read/write raster files used in GIS "Integratsia"
 *           (also known as "Panorama" GIS).
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2007-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/
#include <algorithm>

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"

#include "rmfdataset.h"

CPL_CVSID("$Id$")

constexpr int RMF_DEFAULT_BLOCKXSIZE = 256;
constexpr int RMF_DEFAULT_BLOCKYSIZE = 256;

static const char RMF_SigRSW[] = { 'R', 'S', 'W', '\0' };
static const char RMF_SigRSW_BE[] = { '\0', 'W', 'S', 'R' };
static const char RMF_SigMTW[] = { 'M', 'T', 'W', '\0' };

static const char RMF_UnitsEmpty[] = "";
static const char RMF_UnitsM[] = "m";
static const char RMF_UnitsCM[] = "cm";
static const char RMF_UnitsDM[] = "dm";
static const char RMF_UnitsMM[] = "mm";

constexpr double RMF_DEFAULT_SCALE = 10000.0;
constexpr double RMF_DEFAULT_RESOLUTION = 100.0;


static char* RMFUnitTypeToStr( GUInt32 iElevationUnit )
{
    switch ( iElevationUnit )
    {
        case 0:
            return CPLStrdup( RMF_UnitsM );
        case 1:
            return CPLStrdup( RMF_UnitsDM );
        case 2:
            return CPLStrdup( RMF_UnitsCM );
        case 3:
            return CPLStrdup( RMF_UnitsMM );
        default:
            return CPLStrdup( RMF_UnitsEmpty );
    }
}

static GUInt32 RMFStrToUnitType( const char* pszUnit, int *pbSuccess = nullptr )
{
    if( pbSuccess != nullptr )
    {
        *pbSuccess = TRUE;
    }
    if( EQUAL(pszUnit, RMF_UnitsM) )
        return 0;
    else if( EQUAL(pszUnit, RMF_UnitsDM) )
        return 1;
    else if( EQUAL(pszUnit, RMF_UnitsCM) )
        return 2;
    else if( EQUAL(pszUnit, RMF_UnitsMM) )
        return 3;
    else
    {
        //There is no 'invalid unit' in RMF format. So meter is default...
        if( pbSuccess != nullptr )
        {
            *pbSuccess = FALSE;
        }
        return 0;
    }
}

/************************************************************************/
/* ==================================================================== */
/*                            RMFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           RMFRasterBand()                            */
/************************************************************************/

RMFRasterBand::RMFRasterBand( RMFDataset *poDSIn, int nBandIn,
                              GDALDataType eType ) :
    nBytesPerPixel(poDSIn->sHeader.nBitDepth / 8),
    nLastTileWidth(poDSIn->GetRasterXSize() % poDSIn->sHeader.nTileWidth),
    nLastTileHeight(poDSIn->GetRasterYSize() % poDSIn->sHeader.nTileHeight),
    nDataSize(GDALGetDataTypeSizeBytes( eType ))
{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = eType;
    nBlockXSize = poDSIn->sHeader.nTileWidth;
    nBlockYSize = poDSIn->sHeader.nTileHeight;
    nBlockSize = nBlockXSize * nBlockYSize;
    nBlockBytes = nBlockSize * nDataSize;

#ifdef DEBUG
    CPLDebug( "RMF",
              "Band %d: tile width is %d, tile height is %d, "
              " last tile width %u, last tile height %u, "
              "bytes per pixel is %d, data type size is %d",
              nBand, nBlockXSize, nBlockYSize,
              nLastTileWidth, nLastTileHeight,
              nBytesPerPixel, nDataSize );
#endif
}

/************************************************************************/
/*                           ~RMFRasterBand()                           */
/************************************************************************/

RMFRasterBand::~RMFRasterBand() {}

/************************************************************************/
/*                              ReadBuffer()                            */
/*                                                                      */
/* Helper function to read specified amount of bytes from the input     */
/* file stream.                                                         */
/************************************************************************/

CPLErr RMFRasterBand::ReadBuffer( GByte *pabyBuf, GUInt32 nBytes ) const
{
    RMFDataset  *poGDS = reinterpret_cast<RMFDataset *>( poDS );

    CPLAssert( pabyBuf != nullptr && poGDS->fp != nullptr );

    const vsi_l_offset nOffset = VSIFTellL( poGDS->fp );

    if( VSIFReadL( pabyBuf, 1, nBytes, poGDS->fp ) < nBytes )
    {
        // XXX
        if( poGDS->eAccess == GA_Update )
        {
            return CE_Failure;
        }
        else
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Can't read at offset %ld from input file.\n%s",
                      static_cast<long>( nOffset ), VSIStrerror( errno ) );
            return CE_Failure;
        }
    }

#ifdef CPL_MSB
    if( poGDS->eRMFType == RMFT_MTW )
    {
        if( poGDS->sHeader.nBitDepth == 16 )
        {
            for( GUInt32 i = 0; i < nBytes; i += 2 )
                CPL_SWAP16PTR( pabyBuf + i );
        }

        else if( poGDS->sHeader.nBitDepth == 32 )
        {
            for( GUInt32 i = 0; i < nBytes; i += 4 )
                CPL_SWAP32PTR( pabyBuf + i );
        }

        else if( poGDS->sHeader.nBitDepth == 64 )
        {
            for( GUInt32 i = 0; i < nBytes; i += 8 )
                CPL_SWAPDOUBLE( pabyBuf + i );
        }
    }
#endif

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RMFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )
{
    RMFDataset  *poGDS = reinterpret_cast<RMFDataset *>( poDS );

    CPLAssert( poGDS != nullptr
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != nullptr );

    memset( pImage, 0, nBlockBytes );

    const GUInt32 nTile = nBlockYOff * poGDS->nXTiles + nBlockXOff;
    if( 2 * nTile + 1 >= poGDS->sHeader.nTileTblSize / sizeof(GUInt32) )
    {
        return CE_Failure;
    }

    GUInt32 nTileBytes = poGDS->paiTiles[2 * nTile + 1];
    const GUInt32 nCurBlockYSize =
        (nLastTileHeight
         && static_cast<GUInt32>( nBlockYOff ) == poGDS->nYTiles - 1 )
        ? nLastTileHeight
        : nBlockYSize;

    vsi_l_offset nTileOffset =
        poGDS->GetFileOffset( poGDS->paiTiles[2 * nTile] );

    if( nTileOffset == 0 )
    {
        return CE_None;
    }

    if( VSIFSeekL( poGDS->fp, nTileOffset, SEEK_SET ) < 0 )
    {
        // XXX: We will not report error here, because file just may be
        // in update state and data for this block will be available later
        if( poGDS->eAccess == GA_Update )
            return CE_None;

        CPLError( CE_Failure, CPLE_FileIO,
                  "Can't seek to offset %ld in input file to read data.\n%s",
                  static_cast<long>( nTileOffset ),
                  VSIStrerror( errno ) );
        return CE_Failure;
    }

    if( poGDS->nBands == 1 &&
        ( poGDS->sHeader.nBitDepth == 8
          || poGDS->sHeader.nBitDepth == 16
          || poGDS->sHeader.nBitDepth == 32
          || poGDS->sHeader.nBitDepth == 64 ) )
    {
        if( nTileBytes > nBlockBytes )
            nTileBytes = nBlockBytes;

/* -------------------------------------------------------------------- */
/*  Decompress buffer, if needed.                                       */
/* -------------------------------------------------------------------- */
        if( poGDS->Decompress )
        {
            GUInt32 nRawBytes = 0;

            if( nLastTileWidth && (GUInt32)nBlockXOff == poGDS->nXTiles - 1 )
                nRawBytes = poGDS->nBands * nLastTileWidth * nDataSize;
            else
                nRawBytes = poGDS->nBands * nBlockXSize * nDataSize;

            if( nLastTileHeight && (GUInt32)nBlockYOff == poGDS->nYTiles - 1 )
                nRawBytes *= nLastTileHeight;
            else
                nRawBytes *= nBlockYSize;

            if( nRawBytes > nTileBytes )
            {
                GByte *pabyTile = reinterpret_cast<GByte *>(
                    VSIMalloc( nTileBytes ) );

                if( !pabyTile )
                {
                    CPLError( CE_Failure, CPLE_FileIO,
                            "Can't allocate tile block of size %lu.\n%s",
                            (unsigned long) nTileBytes, VSIStrerror( errno ) );
                    return CE_Failure;
                }

                if( ReadBuffer( pabyTile, nTileBytes ) == CE_Failure )
                {
                    // XXX: Do not fail here, just return empty block
                    // and continue reading.
                    CPLFree( pabyTile );
                    return CE_None;
                }

                (*poGDS->Decompress)( pabyTile, nTileBytes,
                                      reinterpret_cast<GByte*>( pImage ),
                                      nRawBytes );
                CPLFree( pabyTile );
                // nTileBytes = nRawBytes;
            }
            else
            {
                if( ReadBuffer( reinterpret_cast<GByte *>( pImage ),
                                nTileBytes ) == CE_Failure )
                {
                    // XXX: Do not fail here, just return empty block
                    // and continue reading.
                    return CE_None;
                }
            }
        }
        else
        {
            if( ReadBuffer( reinterpret_cast<GByte *>( pImage ),
                            nTileBytes ) == CE_Failure )
            {
                // XXX: Do not fail here, just return empty block
                // and continue reading.
                return CE_None;
            }
        }
    }
    else if( poGDS->eRMFType == RMFT_RSW )
    {
        const GUInt32 nMaxBlockBytes = nBlockBytes * 4; // 4 bands
        if( nTileBytes > nMaxBlockBytes )
        {
            CPLDebug("RMF",
                     "Only reading %u bytes instead of the %u declared "
                     "in the tile array",
                     nMaxBlockBytes, nTileBytes);
            nTileBytes = nMaxBlockBytes;
        }

        GByte *pabyTile = reinterpret_cast<GByte *>( VSIMalloc( nTileBytes ) );

        if( !pabyTile )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Can't allocate tile block of size %lu.\n%s",
                      (unsigned long) nTileBytes, VSIStrerror( errno ) );
            return CE_Failure;
        }

        if( ReadBuffer( pabyTile, nTileBytes ) == CE_Failure )
        {
            // XXX: Do not fail here, just return empty block
            // and continue reading.
            CPLFree( pabyTile );
            return CE_None;
        }

/* -------------------------------------------------------------------- */
/*  If buffer was compressed, decompress it first.                      */
/* -------------------------------------------------------------------- */
        if( poGDS->Decompress )
        {
            GUInt32 nRawBytes = 0;

            if( nLastTileWidth && (GUInt32)nBlockXOff == poGDS->nXTiles - 1 )
                nRawBytes = poGDS->nBands * nLastTileWidth * nDataSize;
            else
                nRawBytes = poGDS->nBands * nBlockXSize * nDataSize;

            if( nLastTileHeight && (GUInt32)nBlockYOff == poGDS->nYTiles - 1 )
                nRawBytes *= nLastTileHeight;
            else
                nRawBytes *= nBlockYSize;

            if( nRawBytes > nTileBytes )
            {
                GByte *pabyRawBuf = reinterpret_cast<GByte *>(
                    VSIMalloc( nRawBytes ) );
                if( pabyRawBuf == nullptr )
                {
                    CPLError( CE_Failure, CPLE_FileIO,
                              "Can't allocate a buffer for raw data of "
                              "size %lu.\n%s",
                              static_cast<unsigned long>( nRawBytes ),
                              VSIStrerror( errno ) );

                    VSIFree( pabyTile );
                    return CE_Failure;
                }

                (*poGDS->Decompress)( pabyTile, nTileBytes,
                                      pabyRawBuf, nRawBytes );
                CPLFree( pabyTile );
                pabyTile = pabyRawBuf;
                nTileBytes = nRawBytes;
            }
        }

/* -------------------------------------------------------------------- */
/*  Deinterleave pixels from input buffer.                              */
/* -------------------------------------------------------------------- */
        if( poGDS->sHeader.nBitDepth == 24 || poGDS->sHeader.nBitDepth == 32 )
        {
            GUInt32 nTileSize = nTileBytes / nBytesPerPixel;

            if( nTileSize > nBlockSize )
                nTileSize = nBlockSize;

            for( GUInt32 i = 0; i < nTileSize; i++ )
            {
                // Colour triplets in RMF file organized in reverse order:
                // blue, green, red. When we have 32-bit RMF the forth byte
                // in quadruplet should be discarded as it has no meaning.
                // That is why we always use 3 byte count in the following
                // pabyTemp index.
                reinterpret_cast<GByte *>( pImage )[i] =
                    pabyTile[i * nBytesPerPixel + 3 - nBand];
            }
        }

        else if( poGDS->sHeader.nBitDepth == 16 )
        {
            GUInt32 nTileSize = nTileBytes / nBytesPerPixel;

            if( nTileSize > nBlockSize )
                nTileSize = nBlockSize;

            for( GUInt32 i = 0; i < nTileSize; i++ )
            {
                switch( nBand )
                {
                    case 1:
                        reinterpret_cast<GByte *>( pImage )[i] =
                            static_cast<GByte>((reinterpret_cast<GUInt16 *>(
                                pabyTile )[i] & 0x7c00) >> 7);
                        break;
                    case 2:
                        reinterpret_cast<GByte *>( pImage )[i] =
                            static_cast<GByte>((reinterpret_cast<GUInt16 *>(
                                pabyTile )[i] & 0x03e0) >> 2);
                        break;
                    case 3:
                        reinterpret_cast<GByte *>( pImage )[i] =
                            static_cast<GByte>((reinterpret_cast<GUInt16 *>(
                                pabyTile)[i] & 0x1F) << 3);
                        break;
                    default:
                        break;
                }
            }
        }
        else if( poGDS->sHeader.nBitDepth == 4 )
        {
            GByte *pabyTemp = pabyTile;

            if( nTileBytes != (nBlockSize+1) / 2 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Tile has %d bytes, %d were expected",
                         nTileBytes, (nBlockSize+1) / 2 );
                CPLFree( pabyTile );
                return CE_Failure;
            }

            for( GUInt32 i = 0; i < nBlockSize; i++ )
            {
                // Most significant part of the byte represents leftmost pixel
                if( i & 0x01 )
                    reinterpret_cast<GByte *>( pImage )[i] = *pabyTemp++ & 0x0F;
                else
                    reinterpret_cast<GByte *>( pImage )[i]
                        = (*pabyTemp & 0xF0) >> 4;
            }
        }
        else if( poGDS->sHeader.nBitDepth == 1 )
        {
            GByte *pabyTemp = pabyTile;

            if( nTileBytes != (nBlockSize+7) / 8 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Tile has %d bytes, %d were expected",
                         nTileBytes, (nBlockSize+7) / 8 );
                CPLFree( pabyTile );
                return CE_Failure;
            }

            for( GUInt32 i = 0; i < nBlockSize; i++ )
            {
                switch( i & 0x7 )
                {
                    case 0:
                        reinterpret_cast<GByte *>( pImage )[i] =
                            (*pabyTemp & 0x80) >> 7;
                        break;
                    case 1:
                        reinterpret_cast<GByte *>( pImage )[i] =
                            (*pabyTemp & 0x40) >> 6;
                        break;
                    case 2:
                        reinterpret_cast<GByte *>( pImage )[i] =
                            (*pabyTemp & 0x20) >> 5;
                        break;
                    case 3:
                        reinterpret_cast<GByte *>( pImage )[i] =
                            (*pabyTemp & 0x10) >> 4;
                        break;
                    case 4:
                        reinterpret_cast<GByte *>( pImage )[i] =
                            (*pabyTemp & 0x08) >> 3;
                        break;
                    case 5:
                        reinterpret_cast<GByte *>( pImage )[i] =
                            (*pabyTemp & 0x04) >> 2;
                        break;
                    case 6:
                        reinterpret_cast<GByte *>( pImage )[i] =
                            (*pabyTemp & 0x02) >> 1;
                        break;
                    case 7:
                        reinterpret_cast<GByte *>( pImage )[i] =
                            *pabyTemp++ & 0x01;
                        break;
                    default:
                        break;
                }
            }
        }

        CPLFree( pabyTile );
    }

    if( nLastTileWidth
        && static_cast<GUInt32>( nBlockXOff ) == poGDS->nXTiles - 1 )
    {
        for( GUInt32 iRow = nCurBlockYSize - 1; iRow > 0; iRow-- )
        {
            memmove( reinterpret_cast<GByte *>( pImage )
                     + nBlockXSize * iRow * nDataSize,
                     reinterpret_cast<GByte *>( pImage ) +
                     iRow * nLastTileWidth * nDataSize,
                     nLastTileWidth * nDataSize );
        }
    }

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr RMFRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )
{
    CPLAssert( poDS != nullptr
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != nullptr );

    RMFDataset *poGDS = reinterpret_cast<RMFDataset *>( poDS );
    const GUInt32 nTile = nBlockYOff * poGDS->nXTiles + nBlockXOff;
    GUInt32 nTileBytes = nDataSize * poGDS->nBands;

    vsi_l_offset nTileOffset =
        poGDS->GetFileOffset( poGDS->paiTiles[2 * nTile] );

    if( nTileOffset )
    {
        if( VSIFSeekL( poGDS->fp, nTileOffset, SEEK_SET ) < 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                "Can't seek to offset %ld in output file to write data.\n%s",
                      static_cast<long>( nTileOffset ),
                      VSIStrerror( errno ) );
            return CE_Failure;
        }
    }
    else
    {
        if( VSIFSeekL( poGDS->fp, 0, SEEK_END ) < 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                "Can't seek to offset %ld in output file to write data.\n%s",
                      static_cast<long>( nTileOffset ),
                      VSIStrerror( errno ) );
            return CE_Failure;
        }
        nTileOffset = VSIFTellL( poGDS->fp );
        vsi_l_offset nNewTileOffset = 0;
        poGDS->paiTiles[2 * nTile] =
            poGDS->GetRMFOffset( nTileOffset, &nNewTileOffset );

        if( nTileOffset != nNewTileOffset )
        {   //May be it is better to write some zeros here?
            if( VSIFSeekL( poGDS->fp, nNewTileOffset, SEEK_SET ) < 0 )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Can't seek to offset %ld in output file to "
                          "write data.\n%s",
                          static_cast<long>( nNewTileOffset ),
                          VSIStrerror( errno ) );
                return CE_Failure;
            }
        }
        nTileOffset = nNewTileOffset;
        poGDS->bHeaderDirty = true;
    }

    if( nLastTileWidth
         && static_cast<GUInt32>( nBlockXOff ) == poGDS->nXTiles - 1 )
        nTileBytes *= nLastTileWidth;
    else
        nTileBytes *= nBlockXSize;

    GUInt32 nCurBlockYSize = 0;
    if( nLastTileHeight
         && static_cast<GUInt32>( nBlockYOff ) == poGDS->nYTiles - 1 )
        nCurBlockYSize = nLastTileHeight;
    else
        nCurBlockYSize = nBlockYSize;

    nTileBytes *= nCurBlockYSize;

    GByte *pabyTile = static_cast<GByte *>( VSICalloc( nTileBytes, 1 ) );
    if( !pabyTile )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Can't allocate space for the tile block of size %lu.\n%s",
                  static_cast<unsigned long>( nTileBytes ),
                  VSIStrerror( errno ) );
        return CE_Failure;
    }

    if( nLastTileWidth
        && static_cast<GUInt32>( nBlockXOff ) == poGDS->nXTiles - 1 )
    {
        if( poGDS->nBands == 1 )
        {
            for( GUInt32 iRow = 0; iRow < nCurBlockYSize; iRow++ )
            {
                memcpy( pabyTile + iRow * nLastTileWidth * nDataSize,
                        reinterpret_cast<GByte *>( pImage )
                        + nBlockXSize * iRow * nDataSize, nLastTileWidth * nDataSize );
            }
        }
        else
        {
            if( poGDS->paiTiles[2 * nTile + 1] )
            {
                VSIFReadL( pabyTile, 1, nTileBytes, poGDS->fp );
                VSIFSeekL( poGDS->fp, nTileOffset, SEEK_SET );
            }

            for( GUInt32 iRow = 0; iRow < nCurBlockYSize; iRow++ )
            {
                for( GUInt32 iInPixel = 0, iOutPixel = nBytesPerPixel - nBand;
                      iOutPixel < nLastTileWidth * nDataSize * poGDS->nBands;
                      iInPixel++, iOutPixel += poGDS->nBands )
                    (pabyTile + iRow * nLastTileWidth * nDataSize * poGDS->nBands)[iOutPixel] =
                        (reinterpret_cast<GByte *>( pImage ) + nBlockXSize
                         * iRow * nDataSize)[iInPixel];
            }
        }
    }
    else
    {
        if( poGDS->nBands == 1 )
        {
            memcpy( pabyTile, pImage, nTileBytes );
        }
        else
        {
            if( poGDS->paiTiles[2 * nTile + 1] )
            {
                VSIFReadL( pabyTile, 1, nTileBytes, poGDS->fp );
                VSIFSeekL( poGDS->fp, nTileOffset, SEEK_SET );
            }

            for( GUInt32 iInPixel = 0, iOutPixel = nBytesPerPixel - nBand;
                  iOutPixel < nTileBytes;
                  iInPixel++, iOutPixel += poGDS->nBands )
                pabyTile[iOutPixel] =
                    reinterpret_cast<GByte *>( pImage )[iInPixel];
        }
    }

#ifdef CPL_MSB
    if ( poGDS->eRMFType == RMFT_MTW )
    {
        if( poGDS->sHeader.nBitDepth == 16 )
        {
            for( GUInt32 i = 0; i < nTileBytes; i += 2 )
                CPL_SWAP16PTR( pabyTile + i );
        }
        else if( poGDS->sHeader.nBitDepth == 32 )
        {
            for( GUInt32 i = 0; i < nTileBytes; i += 4 )
                CPL_SWAP32PTR( pabyTile + i );
        }
        else if( poGDS->sHeader.nBitDepth == 64 )
        {
            for( GUInt32 i = 0; i < nTileBytes; i += 8 )
                CPL_SWAPDOUBLE( pabyTile + i );
        }
    }
#endif

    if( VSIFWriteL( pabyTile, 1, nTileBytes, poGDS->fp ) < nTileBytes )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Can't write block with X offset %d and Y offset %d.\n%s",
                  nBlockXOff, nBlockYOff, VSIStrerror( errno ) );
        VSIFree( pabyTile );
        return CE_Failure;
    }

    poGDS->paiTiles[2 * nTile + 1] = nTileBytes;
    VSIFree( pabyTile );

    poGDS->bHeaderDirty = true;

    return CE_None;
}

/************************************************************************/
/*                          GetNoDataValue()                            */
/************************************************************************/

double RMFRasterBand::GetNoDataValue( int *pbSuccess )

{
    RMFDataset *poGDS = reinterpret_cast<RMFDataset *>( poDS );

    if( pbSuccess )
        *pbSuccess = TRUE;

    return poGDS->sHeader.dfNoData;
}

CPLErr RMFRasterBand::SetNoDataValue( double dfNoData )
{
    RMFDataset *poGDS = reinterpret_cast<RMFDataset *>( poDS );

    poGDS->sHeader.dfNoData = dfNoData;
    poGDS->bHeaderDirty = true;

    return CE_None;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *RMFRasterBand::GetUnitType()

{
    RMFDataset *poGDS = reinterpret_cast<RMFDataset *>( poDS );

    return (const char *)poGDS->pszUnitType;
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr RMFRasterBand::SetUnitType( const char *pszNewValue )

{
    RMFDataset *poGDS = reinterpret_cast<RMFDataset *>( poDS );
    int         bSuccess = FALSE;
    int         iNewUnit = RMFStrToUnitType(pszNewValue, &bSuccess);

    if( bSuccess )
    {
        CPLFree(poGDS->pszUnitType);
        poGDS->pszUnitType = CPLStrdup( pszNewValue );
        poGDS->sHeader.iElevationUnit = iNewUnit;
        poGDS->bHeaderDirty = true;
        return CE_None;
    }
    else
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "RMF driver does not support '%s' elevation units. "
                  "Possible values are: m, dm, cm, mm.",
                  pszNewValue );
        return CE_Failure;
    }
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *RMFRasterBand::GetColorTable()
{
    RMFDataset *poGDS = reinterpret_cast<RMFDataset *>( poDS );

    return poGDS->poColorTable;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr RMFRasterBand::SetColorTable( GDALColorTable *poColorTable )
{
    RMFDataset *poGDS = reinterpret_cast<RMFDataset *>( poDS );

    if( poColorTable )
    {
        if( poGDS->eRMFType == RMFT_RSW && poGDS->nBands == 1 )
        {
            if( !poGDS->pabyColorTable )
                return CE_Failure;

            GDALColorEntry  oEntry;
            for( GUInt32 i = 0; i < poGDS->nColorTableSize; i++ )
            {
                poColorTable->GetColorEntryAsRGB( i, &oEntry );
                poGDS->pabyColorTable[i * 4] = (GByte) oEntry.c1;     // Red
                poGDS->pabyColorTable[i * 4 + 1] = (GByte) oEntry.c2; // Green
                poGDS->pabyColorTable[i * 4 + 2] = (GByte) oEntry.c3; // Blue
                poGDS->pabyColorTable[i * 4 + 3] = 0;
            }

            poGDS->bHeaderDirty = true;
        }
        return CE_None;
    }

    return CE_Failure;
}

int RMFRasterBand::GetOverviewCount()
{
    RMFDataset *poGDS = reinterpret_cast<RMFDataset *>( poDS );
    if( poGDS->poOvrDatasets.empty() )
        return GDALRasterBand::GetOverviewCount();
    else
        return static_cast<int>( poGDS->poOvrDatasets.size() );
}

GDALRasterBand* RMFRasterBand::GetOverview(int i)
{
    RMFDataset *poGDS = reinterpret_cast<RMFDataset *>( poDS );
    size_t      n = static_cast<size_t>( i );
    if( poGDS->poOvrDatasets.empty() )
        return GDALRasterBand::GetOverview(i);
    else
        return poGDS->poOvrDatasets[n]->GetRasterBand(nBand);
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp RMFRasterBand::GetColorInterpretation()
{
    RMFDataset *poGDS = reinterpret_cast<RMFDataset *>( poDS );

    if( poGDS->nBands == 3 )
    {
        if( nBand == 1 )
            return GCI_RedBand;
        else if( nBand == 2 )
            return GCI_GreenBand;
        else if( nBand == 3 )
            return GCI_BlueBand;

        return GCI_Undefined;
    }

    if( poGDS->eRMFType == RMFT_RSW )
        return GCI_PaletteIndex;

    return GCI_Undefined;
}

/************************************************************************/
/* ==================================================================== */
/*                              RMFDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           RMFDataset()                               */
/************************************************************************/

RMFDataset::RMFDataset() :
    eRMFType(RMFT_RSW),
    nXTiles(0),
    nYTiles(0),
    paiTiles(nullptr),
    nColorTableSize(0),
    pabyColorTable(nullptr),
    poColorTable(nullptr),
    pszProjection(CPLStrdup( "" )),
    pszUnitType(CPLStrdup( RMF_UnitsEmpty )),
    bBigEndian(false),
    bHeaderDirty(false),
    pszFilename(nullptr),
    fp(nullptr),
    Decompress(nullptr),
    nHeaderOffset(0),
    poParentDS(nullptr)
{
    nBands = 0;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    memset( &sHeader, 0, sizeof(sHeader) );
    memset( &sExtHeader, 0, sizeof(sExtHeader) );
}

/************************************************************************/
/*                            ~RMFDataset()                             */
/************************************************************************/

RMFDataset::~RMFDataset()
{
    RMFDataset::FlushCache();

    CPLFree( paiTiles );
    CPLFree( pszProjection );
    CPLFree( pszUnitType );
    CPLFree( pabyColorTable );
    if( poColorTable != nullptr )
        delete poColorTable;

    for( size_t n = 0; n != poOvrDatasets.size(); ++n )
    {
        GDALClose( poOvrDatasets[n] );
    }

    if( fp != nullptr && poParentDS == nullptr )
    {
        VSIFCloseL( fp );
    }
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr RMFDataset::GetGeoTransform( double * padfTransform )
{
    memcpy( padfTransform, adfGeoTransform, sizeof(adfGeoTransform[0]) * 6 );

    if( sHeader.iGeorefFlag )
        return CE_None;

    return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr RMFDataset::SetGeoTransform( double * padfTransform )
{
    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );
    sHeader.dfPixelSize = adfGeoTransform[1];
    if( sHeader.dfPixelSize != 0.0 )
        sHeader.dfResolution = sHeader.dfScale / sHeader.dfPixelSize;
    sHeader.dfLLX = adfGeoTransform[0];
    sHeader.dfLLY = adfGeoTransform[3] - nRasterYSize * sHeader.dfPixelSize;
    sHeader.iGeorefFlag = 1;

    bHeaderDirty = true;

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *RMFDataset::GetProjectionRef()
{
    if( pszProjection )
        return pszProjection;

    return "";
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr RMFDataset::SetProjection( const char * pszNewProjection )

{
    CPLFree( pszProjection );
    pszProjection = CPLStrdup( (pszNewProjection) ? pszNewProjection : "" );

    bHeaderDirty = true;

    return CE_None;
}

/************************************************************************/
/*                           WriteHeader()                              */
/************************************************************************/

CPLErr RMFDataset::WriteHeader()
{
/* -------------------------------------------------------------------- */
/*  Setup projection.                                                   */
/* -------------------------------------------------------------------- */
    if( pszProjection && !EQUAL( pszProjection, "" ) )
    {
        OGRSpatialReference oSRS;
        if( oSRS.importFromWkt( pszProjection ) == OGRERR_NONE )
        {
            long iProjection = 0;
            long iDatum = 0;
            long iEllips = 0;
            long iZone = 0;
            double adfPrjParams[7] = {};

            oSRS.exportToPanorama( &iProjection, &iDatum, &iEllips, &iZone,
                                   adfPrjParams );
            sHeader.iProjection = static_cast<int>(iProjection);
            sHeader.dfStdP1 = adfPrjParams[0];
            sHeader.dfStdP2 = adfPrjParams[1];
            sHeader.dfCenterLat = adfPrjParams[2];
            sHeader.dfCenterLong = adfPrjParams[3];

            sExtHeader.nEllipsoid = static_cast<int>(iEllips);
            sExtHeader.nDatum = static_cast<int>(iDatum);
            sExtHeader.nZone = static_cast<int>(iZone);
        }
    }

#define RMF_WRITE_LONG( ptr, value, offset )            \
do {                                                    \
    GInt32  iLong = CPL_LSBWORD32( value );             \
    memcpy( (ptr) + (offset), &iLong, 4 );              \
} while( false );

#define RMF_WRITE_ULONG( ptr,value, offset )            \
do {                                                    \
    GUInt32 iULong = CPL_LSBWORD32( value );            \
    memcpy( (ptr) + (offset), &iULong, 4 );             \
} while( false );

#define RMF_WRITE_DOUBLE( ptr,value, offset )           \
do {                                                    \
    double  dfDouble = (value);                         \
    CPL_LSBPTR64( &dfDouble );                          \
    memcpy( (ptr) + (offset), &dfDouble, 8 );           \
} while( false );

    vsi_l_offset    iCurrentFileSize( GetLastOffset() );
    sHeader.nFileSize0 = GetRMFOffset( iCurrentFileSize, &iCurrentFileSize );
    sHeader.nSize = sHeader.nFileSize0 - GetRMFOffset( nHeaderOffset, nullptr );
/* -------------------------------------------------------------------- */
/*  Write out the main header.                                          */
/* -------------------------------------------------------------------- */
    {
        GByte abyHeader[RMF_HEADER_SIZE] = {};

        memcpy( abyHeader, sHeader.bySignature, RMF_SIGNATURE_SIZE );
        RMF_WRITE_ULONG( abyHeader, sHeader.iVersion, 4 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nSize, 8 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nOvrOffset, 12 );
        RMF_WRITE_ULONG( abyHeader, sHeader.iUserID, 16 );
        memcpy( abyHeader + 20, sHeader.byName, RMF_NAME_SIZE );
        RMF_WRITE_ULONG( abyHeader, sHeader.nBitDepth, 52 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nHeight, 56 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nWidth, 60 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nXTiles, 64 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nYTiles, 68 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nTileHeight, 72 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nTileWidth, 76 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nLastTileHeight, 80 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nLastTileWidth, 84 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nROIOffset, 88 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nROISize, 92 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nClrTblOffset, 96 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nClrTblSize, 100 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nTileTblOffset, 104 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nTileTblSize, 108 );
        RMF_WRITE_LONG( abyHeader, sHeader.iMapType, 124 );
        RMF_WRITE_LONG( abyHeader, sHeader.iProjection, 128 );
        RMF_WRITE_DOUBLE( abyHeader, sHeader.dfScale, 136 );
        RMF_WRITE_DOUBLE( abyHeader, sHeader.dfResolution, 144 );
        RMF_WRITE_DOUBLE( abyHeader, sHeader.dfPixelSize, 152 );
        RMF_WRITE_DOUBLE( abyHeader, sHeader.dfLLY, 160 );
        RMF_WRITE_DOUBLE( abyHeader, sHeader.dfLLX, 168 );
        RMF_WRITE_DOUBLE( abyHeader, sHeader.dfStdP1, 176 );
        RMF_WRITE_DOUBLE( abyHeader, sHeader.dfStdP2, 184 );
        RMF_WRITE_DOUBLE( abyHeader, sHeader.dfCenterLong, 192 );
        RMF_WRITE_DOUBLE( abyHeader, sHeader.dfCenterLat, 200 );
        *(abyHeader + 208) = sHeader.iCompression;
        *(abyHeader + 209) = sHeader.iMaskType;
        *(abyHeader + 210) = sHeader.iMaskStep;
        *(abyHeader + 211) = sHeader.iFrameFlag;
        RMF_WRITE_ULONG( abyHeader, sHeader.nFlagsTblOffset, 212 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nFlagsTblSize, 216 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nFileSize0, 220 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nFileSize1, 224 );
        *(abyHeader + 228) = sHeader.iUnknown;
        *(abyHeader + 244) = sHeader.iGeorefFlag;
        *(abyHeader + 245) = sHeader.iInverse;
        memcpy( abyHeader + 248, sHeader.abyInvisibleColors,
                sizeof(sHeader.abyInvisibleColors) );
        RMF_WRITE_DOUBLE( abyHeader, sHeader.adfElevMinMax[0], 280 );
        RMF_WRITE_DOUBLE( abyHeader, sHeader.adfElevMinMax[1], 288 );
        RMF_WRITE_DOUBLE( abyHeader, sHeader.dfNoData, 296 );
        RMF_WRITE_ULONG( abyHeader, sHeader.iElevationUnit, 304 );
        *(abyHeader + 308) = sHeader.iElevationType;
        RMF_WRITE_ULONG( abyHeader, sHeader.nExtHdrOffset, 312 );
        RMF_WRITE_ULONG( abyHeader, sHeader.nExtHdrSize, 316 );

        VSIFSeekL( fp, nHeaderOffset, SEEK_SET );
        VSIFWriteL( abyHeader, 1, sizeof(abyHeader), fp );
    }

/* -------------------------------------------------------------------- */
/*  Write out the extended header.                                      */
/* -------------------------------------------------------------------- */

    if( sHeader.nExtHdrOffset && sHeader.nExtHdrSize )
    {
        GByte *pabyExtHeader = reinterpret_cast<GByte *>(
            CPLCalloc( sHeader.nExtHdrSize, 1 ) );

        RMF_WRITE_LONG( pabyExtHeader, sExtHeader.nEllipsoid, 24 );
        RMF_WRITE_LONG( pabyExtHeader, sExtHeader.nDatum, 32 );
        RMF_WRITE_LONG( pabyExtHeader, sExtHeader.nZone, 36 );

        VSIFSeekL( fp, GetFileOffset( sHeader.nExtHdrOffset ), SEEK_SET );
        VSIFWriteL( pabyExtHeader, 1, sHeader.nExtHdrSize, fp );

        CPLFree( pabyExtHeader );
    }

#undef RMF_WRITE_DOUBLE
#undef RMF_WRITE_ULONG
#undef RMF_WRITE_LONG

/* -------------------------------------------------------------------- */
/*  Write out the color table.                                          */
/* -------------------------------------------------------------------- */

    if( sHeader.nClrTblOffset && sHeader.nClrTblSize )
    {
        VSIFSeekL( fp, GetFileOffset( sHeader.nClrTblOffset ), SEEK_SET );
        VSIFWriteL( pabyColorTable, 1, sHeader.nClrTblSize, fp );
    }

/* -------------------------------------------------------------------- */
/*  Write out the block table, swap if needed.                          */
/* -------------------------------------------------------------------- */

    VSIFSeekL( fp, GetFileOffset( sHeader.nTileTblOffset ), SEEK_SET );

#ifdef CPL_MSB
    GUInt32 *paiTilesSwapped = reinterpret_cast<GUInt32 *>(
        CPLMalloc( sHeader.nTileTblSize ) );
    if( !paiTilesSwapped )
        return CE_Failure;

    memcpy( paiTilesSwapped, paiTiles, sHeader.nTileTblSize );
    for( GUInt32 i = 0; i < sHeader.nTileTblSize / sizeof(GUInt32); i++ )
        CPL_SWAP32PTR( paiTilesSwapped + i );
    VSIFWriteL( paiTilesSwapped, 1, sHeader.nTileTblSize, fp );

    CPLFree( paiTilesSwapped );
#else
    VSIFWriteL( paiTiles, 1, sHeader.nTileTblSize, fp );
#endif

    bHeaderDirty = false;

    return CE_None;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void RMFDataset::FlushCache()

{
    GDALDataset::FlushCache();

    if( !bHeaderDirty )
        return;

    if( eRMFType == RMFT_MTW )
    {
        GDALRasterBand *poBand = GetRasterBand(1);

        if( poBand )
        {
            poBand->ComputeRasterMinMax( FALSE, sHeader.adfElevMinMax );
            bHeaderDirty = true;
        }
    }
    WriteHeader();
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int RMFDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->pabyHeader == nullptr)
        return FALSE;

    if( memcmp(poOpenInfo->pabyHeader, RMF_SigRSW, sizeof(RMF_SigRSW)) != 0
        && memcmp(poOpenInfo->pabyHeader, RMF_SigRSW_BE,
                  sizeof(RMF_SigRSW_BE)) != 0
        && memcmp(poOpenInfo->pabyHeader, RMF_SigMTW, sizeof(RMF_SigMTW)) != 0 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RMFDataset::Open( GDALOpenInfo * poOpenInfo )
{
    GDALDataset* poDS = Open( poOpenInfo, nullptr, 0 );
    if( poDS == nullptr )
    {
        return nullptr;
    }

    RMFDataset* poCurrentLayer = dynamic_cast<RMFDataset*>( poDS );
    RMFDataset* poParent = poCurrentLayer;
    const int   nMaxPossibleOvCount = 64;

    for( int iOv = 0; iOv < nMaxPossibleOvCount && poCurrentLayer != nullptr; ++iOv )
    {
        poCurrentLayer = poCurrentLayer->OpenOverview( poParent, poOpenInfo );
        if( poCurrentLayer == nullptr )
            break;
        poParent->poOvrDatasets.push_back( poCurrentLayer );
    }

    return poDS;
}

GDALDataset *RMFDataset::Open(GDALOpenInfo * poOpenInfo,
                              RMFDataset* poParentDS,
                              vsi_l_offset nNextHeaderOffset )
{
    if( !Identify(poOpenInfo) || 
        (poParentDS == nullptr && poOpenInfo->fpL == nullptr) )
        return nullptr;

/* -------------------------------------------------------------------- */
/*  Create a corresponding GDALDataset.                                 */
/* -------------------------------------------------------------------- */
    RMFDataset *poDS = new RMFDataset();

    if( poParentDS == nullptr )
    {
        poDS->fp = poOpenInfo->fpL;
        poOpenInfo->fpL = nullptr;
        poDS->nHeaderOffset = 0;
        poDS->poParentDS = nullptr;
    }
    else
    {
        poDS->fp = poParentDS->fp;
        poDS->poParentDS = poParentDS;
        poDS->nHeaderOffset = nNextHeaderOffset;
    }
    poDS->eAccess = poOpenInfo->eAccess;

#define RMF_READ_SHORT(ptr, value, offset)                              \
do {                                                                    \
    if( poDS->bBigEndian )                                              \
    {                                                                   \
        (value) = CPL_MSBWORD16(*(GInt16*)((ptr) + (offset)));          \
    }                                                                   \
    else                                                                \
    {                                                                   \
        (value) = CPL_LSBWORD16(*(GInt16*)((ptr) + (offset)));          \
    }                                                                   \
} while( false );

#define RMF_READ_ULONG(ptr, value, offset)                              \
do {                                                                    \
    if( poDS->bBigEndian )                                              \
    {                                                                   \
        (value) = CPL_MSBWORD32(*(GUInt32*)((ptr) + (offset)));         \
    }                                                                   \
    else                                                                \
    {                                                                   \
        (value) = CPL_LSBWORD32(*(GUInt32*)((ptr) + (offset)));         \
    }                                                                   \
} while( false );

#define RMF_READ_LONG(ptr, value, offset)                               \
do {                                                                    \
    if( poDS->bBigEndian )                                              \
    {                                                                   \
        (value) = CPL_MSBWORD32(*(GInt32*)((ptr) + (offset)));          \
    }                                                                   \
    else                                                                \
    {                                                                   \
        (value) = CPL_LSBWORD32(*(GInt32*)((ptr) + (offset)));          \
    }                                                                   \
} while( false );

#define RMF_READ_DOUBLE(ptr, value, offset)                             \
do {                                                                    \
    (value) = *reinterpret_cast<double*>((ptr) + (offset));             \
    if( poDS->bBigEndian )                                              \
    {                                                                   \
        CPL_MSBPTR64(&(value));                                         \
    }                                                                   \
    else                                                                \
    {                                                                   \
        CPL_LSBPTR64(&(value));                                         \
    }                                                                   \
} while( false );

/* -------------------------------------------------------------------- */
/*  Read the main header.                                               */
/* -------------------------------------------------------------------- */

    {
        GByte abyHeader[RMF_HEADER_SIZE] = {};

        VSIFSeekL( poDS->fp, nNextHeaderOffset, SEEK_SET );
        if( VSIFReadL( abyHeader, 1, sizeof(abyHeader),
                       poDS->fp ) != sizeof(abyHeader) )
        {
            delete poDS;
            return nullptr;
        }

        if( memcmp(abyHeader, RMF_SigMTW, sizeof(RMF_SigMTW)) == 0 )
        {
            poDS->eRMFType = RMFT_MTW;
        }
        else if( memcmp(abyHeader, RMF_SigRSW_BE, sizeof(RMF_SigRSW_BE)) == 0 )
        {
            poDS->eRMFType = RMFT_RSW;
            poDS->bBigEndian = true;
        }
        else
        {
            poDS->eRMFType = RMFT_RSW;
        }

        memcpy( poDS->sHeader.bySignature, abyHeader, RMF_SIGNATURE_SIZE );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.iVersion, 4 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nSize, 8 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nOvrOffset, 12 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.iUserID, 16 );
        memcpy( poDS->sHeader.byName, abyHeader + 20,
                sizeof(poDS->sHeader.byName) );
        poDS->sHeader.byName[sizeof(poDS->sHeader.byName) - 1] = '\0';
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nBitDepth, 52 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nHeight, 56 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nWidth, 60 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nXTiles, 64 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nYTiles, 68 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nTileHeight, 72 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nTileWidth, 76 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nLastTileHeight, 80 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nLastTileWidth, 84 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nROIOffset, 88 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nROISize, 92 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nClrTblOffset, 96 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nClrTblSize, 100 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nTileTblOffset, 104 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nTileTblSize, 108 );
        RMF_READ_LONG( abyHeader, poDS->sHeader.iMapType, 124 );
        RMF_READ_LONG( abyHeader, poDS->sHeader.iProjection, 128 );
        RMF_READ_DOUBLE( abyHeader, poDS->sHeader.dfScale, 136 );
        RMF_READ_DOUBLE( abyHeader, poDS->sHeader.dfResolution, 144 );
        RMF_READ_DOUBLE( abyHeader, poDS->sHeader.dfPixelSize, 152 );
        RMF_READ_DOUBLE( abyHeader, poDS->sHeader.dfLLY, 160 );
        RMF_READ_DOUBLE( abyHeader, poDS->sHeader.dfLLX, 168 );
        RMF_READ_DOUBLE( abyHeader, poDS->sHeader.dfStdP1, 176 );
        RMF_READ_DOUBLE( abyHeader, poDS->sHeader.dfStdP2, 184 );
        RMF_READ_DOUBLE( abyHeader, poDS->sHeader.dfCenterLong, 192 );
        RMF_READ_DOUBLE( abyHeader, poDS->sHeader.dfCenterLat, 200 );
        poDS->sHeader.iCompression = *(abyHeader + 208);
        poDS->sHeader.iMaskType = *(abyHeader + 209);
        poDS->sHeader.iMaskStep = *(abyHeader + 210);
        poDS->sHeader.iFrameFlag = *(abyHeader + 211);
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nFlagsTblOffset, 212 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nFlagsTblSize, 216 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nFileSize0, 220 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nFileSize1, 224 );
        poDS->sHeader.iUnknown = *(abyHeader + 228);
        poDS->sHeader.iGeorefFlag = *(abyHeader + 244);
        poDS->sHeader.iInverse = *(abyHeader + 245);
        memcpy( poDS->sHeader.abyInvisibleColors,
                abyHeader + 248, sizeof(poDS->sHeader.abyInvisibleColors) );
        RMF_READ_DOUBLE( abyHeader, poDS->sHeader.adfElevMinMax[0], 280 );
        RMF_READ_DOUBLE( abyHeader, poDS->sHeader.adfElevMinMax[1], 288 );
        RMF_READ_DOUBLE(abyHeader, poDS->sHeader.dfNoData, 296);

        RMF_READ_ULONG( abyHeader, poDS->sHeader.iElevationUnit, 304 );
        poDS->sHeader.iElevationType = *(abyHeader + 308);
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nExtHdrOffset, 312 );
        RMF_READ_ULONG( abyHeader, poDS->sHeader.nExtHdrSize, 316 );
    }

    if( poParentDS != nullptr )
    {
        if( 0 != memcmp( poDS->sHeader.bySignature,
                         poParentDS->sHeader.bySignature,
                         RMF_SIGNATURE_SIZE ) )
        {
            CPLError( CE_Warning, CPLE_IllegalArg,
                      "Invalid subheader signature." );
            delete poDS;
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*  Read the extended header.                                           */
/* -------------------------------------------------------------------- */

    if( poDS->sHeader.nExtHdrOffset && poDS->sHeader.nExtHdrSize )
    {
        if( poDS->sHeader.nExtHdrSize > 1000000 )
        {
            delete poDS;
            return nullptr;
        }
        GByte *pabyExtHeader = reinterpret_cast<GByte *>(
            VSICalloc( poDS->sHeader.nExtHdrSize, 1 ) );
        if( pabyExtHeader == nullptr )
        {
            delete poDS;
            return nullptr;
        }

        VSIFSeekL( poDS->fp, poDS->GetFileOffset( poDS->sHeader.nExtHdrOffset ),
                   SEEK_SET );
        VSIFReadL( pabyExtHeader, 1, poDS->sHeader.nExtHdrSize, poDS->fp );

        if( poDS->sHeader.nExtHdrSize >= 36 + 4 )
        {
            RMF_READ_LONG( pabyExtHeader, poDS->sExtHeader.nEllipsoid, 24 );
            RMF_READ_LONG( pabyExtHeader, poDS->sExtHeader.nDatum, 32 );
            RMF_READ_LONG( pabyExtHeader, poDS->sExtHeader.nZone, 36 );
        }

        CPLFree( pabyExtHeader );
    }

#undef RMF_READ_DOUBLE
#undef RMF_READ_LONG
#undef RMF_READ_ULONG

    CPLDebug( "RMF", "Version %d", poDS->sHeader.iVersion );

#ifdef DEBUG

    CPLDebug( "RMF", "%s image has width %d, height %d, bit depth %d, "
              "compression scheme %d, %s, nodata %f",
              (poDS->eRMFType == RMFT_MTW) ? "MTW" : "RSW",
              poDS->sHeader.nWidth, poDS->sHeader.nHeight,
              poDS->sHeader.nBitDepth, poDS->sHeader.iCompression,
              poDS->bBigEndian ? "big endian" : "little endian",
              poDS->sHeader.dfNoData );
    CPLDebug( "RMF", "Size %d, offset to overview %#lx, user ID %d, "
              "ROI offset %#lx, ROI size %d",
              poDS->sHeader.nSize,
              static_cast<unsigned long>( poDS->sHeader.nOvrOffset ),
              poDS->sHeader.iUserID,
              static_cast<unsigned long>( poDS->sHeader.nROIOffset ),
              poDS->sHeader.nROISize );
    CPLDebug( "RMF", "Map type %d, projection %d, scale %f, resolution %f, ",
              poDS->sHeader.iMapType, poDS->sHeader.iProjection,
              poDS->sHeader.dfScale, poDS->sHeader.dfResolution );
    CPLDebug( "RMF", "Georeferencing: pixel size %f, LLX %f, LLY %f",
              poDS->sHeader.dfPixelSize,
              poDS->sHeader.dfLLX, poDS->sHeader.dfLLY );
    if( poDS->sHeader.nROIOffset && poDS->sHeader.nROISize )
    {
        GInt32 nValue = 0;

        CPLDebug( "RMF", "ROI coordinates:" );
        /* coverity[tainted_data] */
        for( GUInt32 i = 0; i < poDS->sHeader.nROISize; i += sizeof(nValue) )
        {
            if( VSIFSeekL( poDS->fp,
                           poDS->GetFileOffset( poDS->sHeader.nROIOffset + i ),
                           SEEK_SET ) != 0 ||
                VSIFReadL( &nValue, 1, sizeof(nValue),
                           poDS->fp ) != sizeof(nValue) )
            {
                CPLDebug("RMF", "Cannot read ROI at index %u", i);
                break;
                //delete poDS;
                //return nullptr;
            }

            CPLDebug( "RMF", "%d", nValue );
        }
    }
#endif
    if( poDS->sHeader.nWidth >= INT_MAX ||
        poDS->sHeader.nHeight >= INT_MAX ||
        !GDALCheckDatasetDimensions(poDS->sHeader.nWidth, poDS->sHeader.nHeight) )
    {
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*  Read array of blocks offsets/sizes.                                 */
/* -------------------------------------------------------------------- */

    // To avoid useless excessive memory allocation
    if( poDS->sHeader.nTileTblSize > 1000000 )
    {
        VSIFSeekL( poDS->fp, 0, SEEK_END );
        vsi_l_offset nFileSize = VSIFTellL( poDS->fp );
        if( nFileSize < poDS->sHeader.nTileTblSize )
        {
            delete poDS;
            return nullptr;
        }
    }

    if( VSIFSeekL( poDS->fp,
                   poDS->GetFileOffset( poDS->sHeader.nTileTblOffset ),
                   SEEK_SET ) < 0 )
    {
        delete poDS;
        return nullptr;
    }

    poDS->paiTiles = reinterpret_cast<GUInt32 *>(
        VSIMalloc( poDS->sHeader.nTileTblSize ) );
    if( !poDS->paiTiles )
    {
        delete poDS;
        return nullptr;
    }

    if( VSIFReadL( poDS->paiTiles, 1, poDS->sHeader.nTileTblSize,
                   poDS->fp ) < poDS->sHeader.nTileTblSize )
    {
        CPLDebug( "RMF", "Can't read tiles offsets/sizes table." );
        delete poDS;
        return nullptr;
    }

#ifdef CPL_MSB
    if( !poDS->bBigEndian )
    {
        for( GUInt32 i = 0;
             i < poDS->sHeader.nTileTblSize / sizeof(GUInt32);
             i++ )
            CPL_SWAP32PTR( poDS->paiTiles + i );
    }
#else
    if( poDS->bBigEndian )
    {
        for( GUInt32 i = 0;
             i < poDS->sHeader.nTileTblSize / sizeof(GUInt32);
             i++ )
            CPL_SWAP32PTR( poDS->paiTiles + i );
    }
#endif

#ifdef DEBUG
    CPLDebug( "RMF", "List of block offsets/sizes:" );

    for( GUInt32 i = 0;
         i < poDS->sHeader.nTileTblSize / sizeof(GUInt32);
         i += 2 )
    {
        CPLDebug( "RMF", "    %u / %u",
                  poDS->paiTiles[i], poDS->paiTiles[i + 1] );
    }
#endif

/* -------------------------------------------------------------------- */
/*  Set up essential image parameters.                                  */
/* -------------------------------------------------------------------- */
    GDALDataType eType = GDT_Byte;

    poDS->nRasterXSize = poDS->sHeader.nWidth;
    poDS->nRasterYSize = poDS->sHeader.nHeight;

    if( poDS->eRMFType == RMFT_RSW )
    {
        switch( poDS->sHeader.nBitDepth )
        {
            case 32:
            case 24:
            case 16:
                poDS->nBands = 3;
                break;
            case 1:
            case 4:
            case 8:
                if( poParentDS != nullptr && poParentDS->poColorTable != nullptr )
                {
                    poDS->poColorTable = poParentDS->poColorTable->Clone();
                }
                else
                {
                    // Allocate memory for colour table and read it
                    poDS->nColorTableSize = 1 << poDS->sHeader.nBitDepth;
                    GUInt32 nExpectedColorTableBytes = poDS->nColorTableSize * 4;
                    if(nExpectedColorTableBytes > poDS->sHeader.nClrTblSize )
                    {
                        // We could probably test for strict equality in
                        // the above test ???
                        CPLDebug( "RMF",
                                  "Wrong color table size. "
                                  "Expected %u, got %u.",
                                  nExpectedColorTableBytes,
                                  poDS->sHeader.nClrTblSize );
                        delete poDS;
                        return nullptr;
                    }
                    poDS->pabyColorTable = reinterpret_cast<GByte *>(
                        VSIMalloc( nExpectedColorTableBytes ) );
                    if( poDS->pabyColorTable == nullptr )
                    {
                        CPLDebug( "RMF", "Can't allocate color table." );
                        delete poDS;
                        return nullptr;
                    }
                    if( VSIFSeekL( poDS->fp,
                                   poDS->GetFileOffset( poDS->sHeader.nClrTblOffset ),
                                   SEEK_SET ) < 0 )
                    {
                        CPLDebug( "RMF",
                                  "Can't seek to color table location." );
                        delete poDS;
                        return nullptr;
                    }
                    if( VSIFReadL( poDS->pabyColorTable, 1,
                                   nExpectedColorTableBytes, poDS->fp )
                        < nExpectedColorTableBytes )
                    {
                        CPLDebug( "RMF", "Can't read color table." );
                        delete poDS;
                        return nullptr;
                    }

                    poDS->poColorTable = new GDALColorTable();
                    for( GUInt32 i = 0; i < poDS->nColorTableSize; i++ )
                    {
                        const GDALColorEntry oEntry = {
                            poDS->pabyColorTable[i * 4],     // Red
                            poDS->pabyColorTable[i * 4 + 1], // Green
                            poDS->pabyColorTable[i * 4 + 2], // Blue
                            255                              // Alpha
                        };

                        poDS->poColorTable->SetColorEntry( i, &oEntry );
                    }
                }
                poDS->nBands = 1;
                break;
            default:
                break;
        }
        eType = GDT_Byte;
    }
    else
    {
        poDS->nBands = 1;
        if( poDS->sHeader.nBitDepth == 8 )
            eType = GDT_Byte;
        else if( poDS->sHeader.nBitDepth == 16 )
            eType = GDT_Int16;
        else if( poDS->sHeader.nBitDepth == 32 )
            eType = GDT_Int32;
        else if( poDS->sHeader.nBitDepth == 64 )
            eType = GDT_Float64;
    }

    if( poDS->sHeader.nTileWidth == 0 || poDS->sHeader.nTileWidth > INT_MAX ||
        poDS->sHeader.nTileHeight == 0 || poDS->sHeader.nTileHeight > INT_MAX )
    {
        CPLDebug("RMF", "Invalid tile dimension : %u x %u",
                 poDS->sHeader.nTileWidth, poDS->sHeader.nTileHeight);
        delete poDS;
        return nullptr;
    }

    const int nDataSize = GDALGetDataTypeSizeBytes( eType );
    const int nBlockXSize = static_cast<int>(poDS->sHeader.nTileWidth);
    const int nBlockYSize = static_cast<int>(poDS->sHeader.nTileHeight);
    if( nDataSize == 0 ||
        nBlockXSize > INT_MAX / nBlockYSize ||
        nBlockYSize > INT_MAX / nDataSize ||
        nBlockXSize > INT_MAX / (nBlockYSize * nDataSize) )
    {
        CPLDebug ("RMF", "Too big raster / tile dimension");
        delete poDS;
        return nullptr;
    }

    poDS->nXTiles = DIV_ROUND_UP( poDS->nRasterXSize, nBlockXSize );
    poDS->nYTiles = DIV_ROUND_UP( poDS->nRasterYSize, nBlockYSize );

#ifdef DEBUG
    CPLDebug( "RMF", "Image is %d tiles wide, %d tiles long",
              poDS->nXTiles, poDS->nYTiles );
#endif

/* -------------------------------------------------------------------- */
/*  Choose compression scheme.                                          */
/*  XXX: The DEM compression method seems to be only applicable         */
/*  to Int32 data.                                                      */
/* -------------------------------------------------------------------- */
    if( poDS->sHeader.iCompression == RMF_COMPRESSION_LZW )
        poDS->Decompress = &LZWDecompress;
    else if( poDS->sHeader.iCompression == RMF_COMPRESSION_DEM
             && eType == GDT_Int32 )
        poDS->Decompress = &DEMDecompress;
    else    // No compression
        poDS->Decompress = nullptr;

/* -------------------------------------------------------------------- */
/*  Create band information objects.                                    */
/* -------------------------------------------------------------------- */
    for( int iBand = 1; iBand <= poDS->nBands; iBand++ )
        poDS->SetBand( iBand, new RMFRasterBand( poDS, iBand, eType ) );

/* -------------------------------------------------------------------- */
/*  Set up projection.                                                  */
/*                                                                      */
/*  XXX: If projection value is not specified, but image still have     */
/*  georeferencing information, assume Gauss-Kruger projection.         */
/* -------------------------------------------------------------------- */
    if( poDS->sHeader.iProjection > 0 ||
        (poDS->sHeader.dfPixelSize != 0.0 &&
         poDS->sHeader.dfLLX != 0.0 &&
         poDS->sHeader.dfLLY != 0.0) )
    {
        OGRSpatialReference oSRS;
        GInt32 nProj =
            (poDS->sHeader.iProjection) ? poDS->sHeader.iProjection : 1;
        double padfPrjParams[8] = {
            poDS->sHeader.dfStdP1,
            poDS->sHeader.dfStdP2,
            poDS->sHeader.dfCenterLat,
            poDS->sHeader.dfCenterLong,
            1.0,
            0.0,
            0.0,
            0.0
          };

        // XXX: Compute zone number for Gauss-Kruger (Transverse Mercator)
        // projection if it is not specified.
        if( nProj == 1L && poDS->sHeader.dfCenterLong == 0.0 )
        {
            if( poDS->sExtHeader.nZone == 0 )
            {
                double centerXCoord = poDS->sHeader.dfLLX +
                    (poDS->nRasterXSize * poDS->sHeader.dfPixelSize / 2.0);
                padfPrjParams[7] =
                    floor((centerXCoord - 500000.0 ) / 1000000.0);
            }
            else
            {
                padfPrjParams[7] = poDS->sExtHeader.nZone;
            }
        }

        oSRS.importFromPanorama( nProj, poDS->sExtHeader.nDatum,
                                 poDS->sExtHeader.nEllipsoid, padfPrjParams );
        if( poDS->pszProjection )
            CPLFree( poDS->pszProjection );
        oSRS.exportToWkt( &poDS->pszProjection );
    }

/* -------------------------------------------------------------------- */
/*  Set up georeferencing.                                              */
/* -------------------------------------------------------------------- */
    if( (poDS->eRMFType == RMFT_RSW && poDS->sHeader.iGeorefFlag) ||
        (poDS->eRMFType == RMFT_MTW && poDS->sHeader.dfPixelSize != 0.0) )
    {
        poDS->adfGeoTransform[0] = poDS->sHeader.dfLLX;
        poDS->adfGeoTransform[3] = poDS->sHeader.dfLLY
            + poDS->nRasterYSize * poDS->sHeader.dfPixelSize;
        poDS->adfGeoTransform[1] = poDS->sHeader.dfPixelSize;
        poDS->adfGeoTransform[5] = - poDS->sHeader.dfPixelSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[4] = 0.0;
    }

/* -------------------------------------------------------------------- */
/*  Set units.                                                          */
/* -------------------------------------------------------------------- */

    if( poDS->eRMFType == RMFT_MTW )
    {
        CPLFree(poDS->pszUnitType);
        poDS->pszUnitType = RMFUnitTypeToStr(poDS->sHeader.iElevationUnit);
    }

/* -------------------------------------------------------------------- */
/*  Report some other dataset related information.                      */
/* -------------------------------------------------------------------- */

    if( poDS->eRMFType == RMFT_MTW )
    {
        char szTemp[256] = {};

        snprintf(szTemp, sizeof(szTemp), "%g", poDS->sHeader.adfElevMinMax[0]);
        poDS->SetMetadataItem( "ELEVATION_MINIMUM", szTemp );

        snprintf(szTemp, sizeof(szTemp), "%g", poDS->sHeader.adfElevMinMax[1]);
        poDS->SetMetadataItem( "ELEVATION_MAXIMUM", szTemp );

        poDS->SetMetadataItem( "ELEVATION_UNITS", poDS->pszUnitType );

        snprintf(szTemp, sizeof(szTemp), "%d", poDS->sHeader.iElevationType);
        poDS->SetMetadataItem( "ELEVATION_TYPE", szTemp );
    }

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    if( nNextHeaderOffset == 0 && poParentDS == nullptr )
    {
        poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/
GDALDataset *RMFDataset::Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char **papszParmList )
{
    return Create( pszFilename, nXSize, nYSize, nBands,
                   eType, papszParmList, nullptr, 1.0 );
}

GDALDataset *RMFDataset::Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char **papszParmList,
                                 RMFDataset* poParentDS, double dfOvFactor )

{
    if( nBands != 1 && nBands != 3 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "RMF driver doesn't support %d bands. Must be 1 or 3.",
                  nBands );

        return nullptr;
    }

    if( nBands == 1
        && eType != GDT_Byte
        && eType != GDT_Int16
        && eType != GDT_Int32
        && eType != GDT_Float64 )
    {
         CPLError(
             CE_Failure, CPLE_AppDefined,
             "Attempt to create RMF dataset with an illegal data type (%s), "
             "only Byte, Int16, Int32 and Float64 types supported "
             "by the format for single-band images.",
             GDALGetDataTypeName(eType) );

        return nullptr;
    }

    if( nBands == 3 && eType != GDT_Byte )
    {
         CPLError(
             CE_Failure, CPLE_AppDefined,
             "Attempt to create RMF dataset with an illegal data type (%s), "
             "only Byte type supported by the format for three-band images.",
             GDALGetDataTypeName(eType) );

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*  Create the dataset.                                                 */
/* -------------------------------------------------------------------- */
    RMFDataset *poDS = new RMFDataset();

    GUInt32 nBlockXSize =
        ( nXSize < RMF_DEFAULT_BLOCKXSIZE ) ? nXSize : RMF_DEFAULT_BLOCKXSIZE;
    GUInt32 nBlockYSize =
        ( nYSize < RMF_DEFAULT_BLOCKYSIZE ) ? nYSize : RMF_DEFAULT_BLOCKYSIZE;
    double dfScale;
    double dfResolution;
    double dfPixelSize;
    if( poParentDS == nullptr )
    {
        poDS->fp = VSIFOpenL( pszFilename, "w+b" );
        if( poDS->fp == nullptr )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, "Unable to create file %s.",
                      pszFilename );
            delete poDS;
            return nullptr;
        }

        dfScale = RMF_DEFAULT_SCALE;
        dfResolution = RMF_DEFAULT_RESOLUTION;
        dfPixelSize = 1;

        if( CPLFetchBool( papszParmList, "MTW", false) )
            poDS->eRMFType = RMFT_MTW;
        else
            poDS->eRMFType = RMFT_RSW;

        GUInt32 iVersion = RMF_VERSION;
        const char *pszRMFHUGE = CSLFetchNameValue(papszParmList, "RMFHUGE");

        if( pszRMFHUGE == nullptr )
            pszRMFHUGE = "NO";// Keep old behavior by default

        if( EQUAL(pszRMFHUGE,"NO") )
        {
            iVersion = RMF_VERSION;
        }
        else if( EQUAL(pszRMFHUGE,"YES") )
        {
            iVersion = RMF_VERSION_HUGE;
        }
        else if( EQUAL(pszRMFHUGE,"IF_SAFER") )
        {
            const double dfImageSize =
                static_cast<double>(nXSize) *
                static_cast<double>(nYSize) *
                static_cast<double>(nBands) *
                static_cast<double>(GDALGetDataTypeSizeBytes(eType));
            if( dfImageSize > 3.0*1024.0*1024.0*1024.0 )
            {
                iVersion = RMF_VERSION_HUGE;
            }
            else
            {
                iVersion = RMF_VERSION;
            }
        }

        const char *pszValue = CSLFetchNameValue(papszParmList,"BLOCKXSIZE");
        if( pszValue != nullptr )
            nBlockXSize = atoi( pszValue );
        if( static_cast<int>(nBlockXSize) <= 0 )
            nBlockXSize = RMF_DEFAULT_BLOCKXSIZE;

        pszValue = CSLFetchNameValue(papszParmList,"BLOCKYSIZE");
        if( pszValue != nullptr )
            nBlockYSize = atoi( pszValue );
        if( static_cast<int>(nBlockYSize) <= 0 )
            nBlockYSize = RMF_DEFAULT_BLOCKXSIZE;
        poDS->pszFilename = pszFilename;

        if( poDS->eRMFType == RMFT_MTW )
            memcpy( poDS->sHeader.bySignature, RMF_SigMTW, RMF_SIGNATURE_SIZE );
        else
            memcpy( poDS->sHeader.bySignature, RMF_SigRSW, RMF_SIGNATURE_SIZE );
        poDS->sHeader.iVersion = iVersion;
        poDS->sHeader.nOvrOffset = 0x00;
    }
    else
    {
        poDS->fp = poParentDS->fp;
        memcpy( poDS->sHeader.bySignature, poParentDS->sHeader.bySignature,
                RMF_SIGNATURE_SIZE );
        poDS->sHeader.iVersion = poParentDS->sHeader.iVersion;
        poDS->eRMFType = poParentDS->eRMFType;
        nBlockXSize = poParentDS->sHeader.nTileWidth;
        nBlockYSize = poParentDS->sHeader.nTileHeight;
        dfScale = poParentDS->sHeader.dfScale;
        dfResolution = poParentDS->sHeader.dfResolution/dfOvFactor;
        dfPixelSize = poParentDS->sHeader.dfPixelSize*dfOvFactor;

        poDS->nHeaderOffset = poParentDS->GetLastOffset();
        poParentDS->sHeader.nOvrOffset =
               poDS->GetRMFOffset( poDS->nHeaderOffset, &poDS->nHeaderOffset );
        poParentDS->bHeaderDirty = true;
        VSIFSeekL( poDS->fp, poDS->nHeaderOffset, SEEK_SET );
        poDS->poParentDS = poParentDS;
        CPLDebug( "RMF",
                  "Create overview subfile at " CPL_FRMT_GUIB
                  " with size %dx%d, parent overview offset %d",
                  poDS->nHeaderOffset, nXSize, nYSize,
                  poParentDS->sHeader.nOvrOffset );
    }
/* -------------------------------------------------------------------- */
/*  Fill the RMFHeader                                                  */
/* -------------------------------------------------------------------- */
    CPLDebug( "RMF", "Version %d", poDS->sHeader.iVersion );

    poDS->sHeader.iUserID = 0x00;
    memset( poDS->sHeader.byName, 0, sizeof(poDS->sHeader.byName) );
    poDS->sHeader.nBitDepth = GDALGetDataTypeSizeBits( eType ) * nBands;
    poDS->sHeader.nHeight = nYSize;
    poDS->sHeader.nWidth = nXSize;
    poDS->sHeader.nTileWidth = nBlockXSize;
    poDS->sHeader.nTileHeight = nBlockYSize;

    poDS->nXTiles = poDS->sHeader.nXTiles =
        ( nXSize + poDS->sHeader.nTileWidth - 1 ) / poDS->sHeader.nTileWidth;
    poDS->nYTiles = poDS->sHeader.nYTiles =
        ( nYSize + poDS->sHeader.nTileHeight - 1 ) / poDS->sHeader.nTileHeight;
    poDS->sHeader.nLastTileHeight = nYSize % poDS->sHeader.nTileHeight;
    if( !poDS->sHeader.nLastTileHeight )
        poDS->sHeader.nLastTileHeight = poDS->sHeader.nTileHeight;
    poDS->sHeader.nLastTileWidth = nXSize % poDS->sHeader.nTileWidth;
    if( !poDS->sHeader.nLastTileWidth )
        poDS->sHeader.nLastTileWidth = poDS->sHeader.nTileWidth;

    poDS->sHeader.nROIOffset = 0x00;
    poDS->sHeader.nROISize = 0x00;

    vsi_l_offset nCurPtr = poDS->nHeaderOffset + RMF_HEADER_SIZE;

    // Extended header
    poDS->sHeader.nExtHdrOffset = poDS->GetRMFOffset( nCurPtr, &nCurPtr );
    poDS->sHeader.nExtHdrSize = RMF_EXT_HEADER_SIZE;
    nCurPtr += poDS->sHeader.nExtHdrSize;

    // Color table
    if( poDS->eRMFType == RMFT_RSW && nBands == 1 )
    {
        if( poDS->sHeader.nBitDepth > 8 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Cannot create color table of RSW with nBitDepth = %d. "
                      "Retry with MTW ?",
                      poDS->sHeader.nBitDepth );
            delete poDS;
            return nullptr;
        }

        poDS->sHeader.nClrTblOffset = poDS->GetRMFOffset( nCurPtr, &nCurPtr );
        poDS->nColorTableSize = 1 << poDS->sHeader.nBitDepth;
        poDS->sHeader.nClrTblSize = poDS->nColorTableSize * 4;
        poDS->pabyColorTable = reinterpret_cast<GByte *>(
            VSI_MALLOC_VERBOSE( poDS->sHeader.nClrTblSize ) );
        if( poDS->pabyColorTable == nullptr )
        {
            delete poDS;
            return nullptr;
        }
        for( GUInt32 i = 0; i < poDS->nColorTableSize; i++ )
        {
            poDS->pabyColorTable[i * 4] =
                poDS->pabyColorTable[i * 4 + 1] =
                poDS->pabyColorTable[i * 4 + 2] = (GByte) i;
                poDS->pabyColorTable[i * 4 + 3] = 0;
        }
        nCurPtr += poDS->sHeader.nClrTblSize;
    }
    else
    {
        poDS->sHeader.nClrTblOffset = 0x00;
        poDS->sHeader.nClrTblSize = 0x00;
    }

    // Blocks table
    poDS->sHeader.nTileTblOffset = poDS->GetRMFOffset( nCurPtr, &nCurPtr );
    poDS->sHeader.nTileTblSize =
        poDS->sHeader.nXTiles * poDS->sHeader.nYTiles * 4 * 2;
    poDS->paiTiles = reinterpret_cast<GUInt32 *>(
        CPLCalloc( poDS->sHeader.nTileTblSize, 1 ) );
    // nCurPtr += poDS->sHeader.nTileTblSize;
    const GUInt32 nTileSize =
        poDS->sHeader.nTileWidth * poDS->sHeader.nTileHeight
        * GDALGetDataTypeSizeBytes( eType );
    poDS->sHeader.nSize =
        poDS->paiTiles[poDS->sHeader.nTileTblSize / 4 - 2] + nTileSize;

    // Elevation units
    poDS->sHeader.iElevationUnit = RMFStrToUnitType(poDS->pszUnitType);

    poDS->sHeader.iMapType = -1;
    poDS->sHeader.iProjection = -1;
    poDS->sHeader.dfScale = dfScale;
    poDS->sHeader.dfResolution = dfResolution;
    poDS->sHeader.dfPixelSize = dfPixelSize;
    poDS->sHeader.iCompression = 0;
    poDS->sHeader.iMaskType = 0;
    poDS->sHeader.iMaskStep = 0;
    poDS->sHeader.iFrameFlag = 0;
    poDS->sHeader.nFlagsTblOffset = 0x00;
    poDS->sHeader.nFlagsTblSize = 0x00;
    poDS->sHeader.nFileSize0 = 0x00;
    poDS->sHeader.nFileSize1 = 0x00;
    poDS->sHeader.iUnknown = 0;
    poDS->sHeader.iGeorefFlag = 0;
    poDS->sHeader.iInverse = 0;
    memset( poDS->sHeader.abyInvisibleColors, 0,
            sizeof(poDS->sHeader.abyInvisibleColors) );
    poDS->sHeader.adfElevMinMax[0] = 0.0;
    poDS->sHeader.adfElevMinMax[1] = 0.0;
    poDS->sHeader.dfNoData = 0.0;
    poDS->sHeader.iElevationType = 0;

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->nBands = nBands;

    poDS->WriteHeader();

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 1; iBand <= poDS->nBands; iBand++ )
        poDS->SetBand( iBand, new RMFRasterBand( poDS, iBand, eType ) );

    return reinterpret_cast<GDALDataset *>( poDS );
}

//GIS Panorama 11 was introduced new format for huge files (greater than 3 Gb)
vsi_l_offset RMFDataset::GetFileOffset( GUInt32 iRMFOffset ) const
{
    if( sHeader.iVersion >= RMF_VERSION_HUGE )
    {
        return ((vsi_l_offset)iRMFOffset) * RMF_HUGE_OFFSET_FACTOR;
    }

    return (vsi_l_offset)iRMFOffset;
}

GUInt32 RMFDataset::GetRMFOffset( vsi_l_offset nFileOffset,
                                  vsi_l_offset* pnNewFileOffset ) const
{
    if( sHeader.iVersion >= RMF_VERSION_HUGE )
    {
        //Round offset to next RMF_HUGE_OFFSET_FACTOR
        const GUInt32 iRMFOffset = static_cast<GUInt32>(
            (nFileOffset + (RMF_HUGE_OFFSET_FACTOR-1) ) /
            RMF_HUGE_OFFSET_FACTOR );
        if( pnNewFileOffset != nullptr )
        {
            *pnNewFileOffset = GetFileOffset( iRMFOffset );
        }
        return iRMFOffset;
    }

    if( pnNewFileOffset != nullptr )
    {
        *pnNewFileOffset = nFileOffset;
    }
    return static_cast<GUInt32>(nFileOffset);
}

RMFDataset* RMFDataset::OpenOverview(RMFDataset* poParent, GDALOpenInfo* poOpenInfo)
{
    if( sHeader.nOvrOffset == 0 )
    {
        return nullptr;
    }

    if( poParent == nullptr )
    {
        return nullptr;
    }

    vsi_l_offset nSubOffset = GetFileOffset(sHeader.nOvrOffset);

    CPLDebug( "RMF",
              "Try to open overview subfile at " CPL_FRMT_GUIB " for '%s'",
              nSubOffset, poOpenInfo->pszFilename );

    if( !poParent->poOvrDatasets.empty() )
    {
        if( poParent->GetFileOffset( poParent->sHeader.nOvrOffset ) ==
            nSubOffset )
        {
            CPLError( CE_Warning, CPLE_IllegalArg,
                      "Recursive subdataset list is detected. "
                      "Overview open failed." );
            return nullptr;
        }

        for( size_t n = 0; n != poParent->poOvrDatasets.size() - 1; ++n )
        {
            RMFDataset* poOvr( poParent->poOvrDatasets[n] );

            if( poOvr == nullptr )
                continue;
            if( poOvr->GetFileOffset( poOvr->sHeader.nOvrOffset ) ==
                nSubOffset )
            {
                CPLError( CE_Warning, CPLE_IllegalArg,
                          "Recursive subdataset list is detected. "
                          "Overview open failed." );
                return nullptr;
            }
        }
    }

    size_t nHeaderSize( RMF_HEADER_SIZE );
    GByte * pabyNewHeader;
    pabyNewHeader = static_cast<GByte *>( CPLRealloc(poOpenInfo->pabyHeader,
                                                     nHeaderSize + 1) );
    if( pabyNewHeader == nullptr )
    {
        CPLError( CE_Warning, CPLE_OutOfMemory,
                  "Can't allocate buffer for overview header" );
        return nullptr;
    }

    poOpenInfo->pabyHeader = pabyNewHeader;
    memset( poOpenInfo->pabyHeader, 0, nHeaderSize + 1 );
    VSIFSeekL( fp, nSubOffset, SEEK_SET );
    poOpenInfo->nHeaderBytes = static_cast<int>( VSIFReadL( poOpenInfo->pabyHeader,
                                                 1, nHeaderSize, fp ) );

    RMFDataset* poSub = (RMFDataset*)Open( poOpenInfo, poParent, nSubOffset );

    if( poSub == nullptr )
    {
        return nullptr;
    }

    return poSub;
}

CPLErr RMFDataset::IBuildOverviews( const char* pszResampling,
                                    int nOverviews, int* panOverviewList,
                                    int nBandsIn, int* panBandList,
                                    GDALProgressFunc pfnProgress,
                                    void* pProgressData )
{
    bool bUseGenericHandling = false;

    if( GetAccess() != GA_Update )
    {
        CPLDebug( "RMF",
                  "File open for read-only accessing, "
                  "creating overviews externally." );

        bUseGenericHandling = true;
    }

    if( bUseGenericHandling )
    {
        if( !poOvrDatasets.empty() )
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Cannot add external overviews when there are already "
                "internal overviews" );
            return CE_Failure;
        }

        return GDALDataset::IBuildOverviews(
            pszResampling, nOverviews, panOverviewList,
            nBandsIn, panBandList, pfnProgress, pProgressData );
    }

    if( nBandsIn != GetRasterCount() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews in RMF is only "
                  "supported when operating on all bands.  "
                  "Operation failed." );
        return CE_Failure;
    }

    if( nOverviews == 0 )
    {
        if( poOvrDatasets.empty() )
        {
            return GDALDataset::IBuildOverviews(
                pszResampling, nOverviews, panOverviewList,
                nBandsIn, panBandList, pfnProgress, pProgressData );
        }
        return CleanOverviews();
    }

    // First destroy old overviews
    if( CE_None != CleanOverviews() )
    {
        return CE_Failure;
    }

    CPLDebug( "RMF", "Build overviews on dataset %d x %d size",
              GetRasterXSize(), GetRasterYSize() );

    GDALDataType    eMainType = GetRasterBand(1)->GetRasterDataType();
    RMFDataset*     poParent = this;
    double          prevOvLevel = 1.0;
    for( int n = 0; n != nOverviews; ++n )
    {
        int nOvLevel = panOverviewList[n];
        const int nOXSize = (GetRasterXSize() + nOvLevel - 1) / nOvLevel;
        const int nOYSize = (GetRasterYSize() + nOvLevel - 1) / nOvLevel;
        CPLDebug( "RMF", "\tCreate overview #%d size %d x %d",
                  nOvLevel, nOXSize, nOYSize );

        RMFDataset*    poOvrDataset;
        poOvrDataset = static_cast<RMFDataset*>(
                    RMFDataset::Create( nullptr, nOXSize, nOYSize,
                                        GetRasterCount(), eMainType,
                                        nullptr, poParent, nOvLevel / prevOvLevel ) );

        if( poOvrDataset == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Can't create overview dataset #%d size %d x %d",
                      nOvLevel, nOXSize, nOYSize );
            return CE_Failure;
        }

        prevOvLevel = nOvLevel;
        poParent = poOvrDataset;
        poOvrDatasets.push_back( poOvrDataset );
    }

    GDALRasterBand ***papapoOverviewBands =
        static_cast<GDALRasterBand ***>(CPLCalloc(sizeof(void*),nBandsIn));
    GDALRasterBand **papoBandList =
        static_cast<GDALRasterBand **>(CPLCalloc(sizeof(void*),nBandsIn));

    for( int iBand = 0; iBand < nBandsIn; ++iBand )
    {
        GDALRasterBand* poBand = GetRasterBand( panBandList[iBand] );

        papoBandList[iBand] = poBand;
        papapoOverviewBands[iBand] =
            static_cast<GDALRasterBand **>( CPLCalloc(
                sizeof(void*), poBand->GetOverviewCount()) );

        for( int i = 0; i < nOverviews; ++i )
        {
            papapoOverviewBands[iBand][i] = poBand->GetOverview( i );
        }
    }
#ifdef DEBUG
    for( int iBand = 0; iBand < nBandsIn; ++iBand )
    {
        CPLDebug( "RMF",
                  "Try to create overview for #%d size %d x %d",
                  iBand + 1,
                  papoBandList[iBand]->GetXSize(),
                  papoBandList[iBand]->GetYSize() );
        for( int i = 0; i < nOverviews; ++i )
        {
            CPLDebug( "RMF",
                      "\t%d x %d",
                      papapoOverviewBands[iBand][i]->GetXSize(),
                      papapoOverviewBands[iBand][i]->GetYSize() );
        }
    }
#endif //DEBUG
    CPLErr  res;
    res = GDALRegenerateOverviewsMultiBand( nBandsIn, papoBandList,
                                            nOverviews, papapoOverviewBands,
                                            pszResampling, pfnProgress,
                                            pProgressData );

    for( int iBand = 0; iBand < nBandsIn; ++iBand )
    {
        CPLFree(papapoOverviewBands[iBand]);
    }

    CPLFree(papapoOverviewBands);
    CPLFree(papoBandList);

    return res;
}

vsi_l_offset RMFDataset::GetLastOffset() const
{
    vsi_l_offset    nLastTileOff = 0;
    GUInt32         nTiles( sHeader.nTileTblSize/sizeof(GUInt32) );

    for( GUInt32 n = 0; n < nTiles; n += 2 )
    {
        vsi_l_offset nTileOffset = GetFileOffset( paiTiles[n] );
        GUInt32      nTileBytes = paiTiles[n + 1];
        nLastTileOff = std::max( nLastTileOff, nTileOffset + nTileBytes );
        CPLDebug( "RMF",
                  "Tile #%d off " CPL_FRMT_GUIB " size %d",
                  n, nTileOffset, nTileBytes );
    }

    nLastTileOff = std::max( nLastTileOff,
                             GetFileOffset( sHeader.nROIOffset ) +
                             sHeader.nROISize );
    nLastTileOff = std::max( nLastTileOff,
                             GetFileOffset( sHeader.nClrTblOffset ) +
                             sHeader.nClrTblSize );
    nLastTileOff = std::max( nLastTileOff,
                             GetFileOffset( sHeader.nTileTblOffset ) +
                             sHeader.nTileTblSize );
    nLastTileOff = std::max( nLastTileOff,
                             GetFileOffset( sHeader.nFlagsTblOffset ) +
                             sHeader.nFlagsTblSize );
    nLastTileOff = std::max( nLastTileOff,
                             GetFileOffset( sHeader.nExtHdrOffset ) +
                             sHeader.nExtHdrSize );
    return nLastTileOff;
}

CPLErr RMFDataset::CleanOverviews()
{
    if( sHeader.nOvrOffset == 0 )
    {
        return CE_None;
    }

    if( GetAccess() != GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "File open for read-only accessing, "
                  "overviews cleanup failed." );
        return CE_Failure;
    }

    if( poParentDS != nullptr )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Overviews cleanup for non-root dataset is not possible." );
        return CE_Failure;
    }

    for( size_t n = 0; n != poOvrDatasets.size(); ++n )
    {
        GDALClose( poOvrDatasets[n] );
    }
    poOvrDatasets.clear();

    vsi_l_offset    nLastTileOff = GetLastOffset();

    if( 0 != VSIFSeekL( fp, 0, SEEK_END ) )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to end of file, "
                  "overviews cleanup failed." );
    }

    vsi_l_offset    nFileSize = VSIFTellL( fp );
    if( nFileSize < nLastTileOff )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Invalid file offset, "
                  "overviews cleanup failed." );
        return CE_Failure;
    }

    CPLDebug( "RMF", "Truncate to " CPL_FRMT_GUIB, nLastTileOff );
    CPLDebug( "RMF", "File size:  " CPL_FRMT_GUIB, nFileSize );

    if( 0 != VSIFTruncateL( fp, nLastTileOff ) )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to truncate file, "
                  "overviews cleanup failed." );
        return CE_Failure;
    }

    sHeader.nOvrOffset = 0;
    bHeaderDirty = true;

    return CE_None;
}

/************************************************************************/
/*                        GDALRegister_RMF()                            */
/************************************************************************/

void GDALRegister_RMF()

{
    if( GDALGetDriverByName( "RMF" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "RMF" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Raster Matrix Format" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_rmf.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "rsw" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 Int32 Float64" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='MTW' type='boolean' description='Create MTW DEM matrix'/>"
"   <Option name='BLOCKXSIZE' type='int' description='Tile Width'/>"
"   <Option name='BLOCKYSIZE' type='int' description='Tile Height'/>"
"   <Option name='RMFHUGE' type='string-select' description='Creation of huge RMF file (Supported by GIS Panorama since v11)'>"
"     <Value>NO</Value>"
"     <Value>YES</Value>"
"     <Value>IF_SAFER</Value>"
"   </Option>"
"</CreationOptionList>" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnIdentify = RMFDataset::Identify;
    poDriver->pfnOpen = RMFDataset::Open;
    poDriver->pfnCreate = RMFDataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
