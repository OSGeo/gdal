/******************************************************************************
 * $Id$
 *
 * Project:  Raster Matrix Format
 * Purpose:  Read/write raster files used in GIS "Integratsia"
 *           (also known as "Panorama" GIS).
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Andrey Kiselev <dron@ak4719.spb.edu>
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

#include "ogr_spatialref.h"
#include "cpl_string.h"

#include "rmfdataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_RMF(void);
CPL_C_END

#define RMF_DEFAULT_BLOCKXSIZE 256
#define RMF_DEFAULT_BLOCKYSIZE 256

/************************************************************************/
/* ==================================================================== */
/*                            RMFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           RMFRasterBand()                            */
/************************************************************************/

RMFRasterBand::RMFRasterBand( RMFDataset *poDS, int nBand,
                              GDALDataType eType )
{
    this->poDS = poDS;
    this->nBand = nBand;

    eDataType = eType;
    nBytesPerPixel = poDS->sHeader.nBitDepth / 8;
    nDataSize = GDALGetDataTypeSize( eDataType ) / 8;
    nBlockXSize = poDS->sHeader.nTileWidth;
    nBlockYSize = poDS->sHeader.nTileHeight;
    nBlockSize = nBlockXSize * nBlockYSize;
    nBlockBytes = nBlockSize * nDataSize;
    nLastTileXBytes =
        (poDS->GetRasterXSize() % poDS->sHeader.nTileWidth) * nDataSize;
    nLastTileHeight = poDS->GetRasterYSize() % poDS->sHeader.nTileHeight;

#if DEBUG
    CPLDebug( "RMF",
              "Band %d: tile width is %d, tile height is %d, "
              " last tile width %d, last tile height %d, "
              "bytes per pixel is %d, data type size is %d",
              nBand, nBlockXSize, nBlockYSize,
              poDS->sHeader.nLastTileWidth, poDS->sHeader.nLastTileHeight,
              nBytesPerPixel, nDataSize );
#endif
}

/************************************************************************/
/*                           ~RMFRasterBand()                           */
/************************************************************************/

RMFRasterBand::~RMFRasterBand()
{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RMFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )
{
    RMFDataset  *poGDS = (RMFDataset *) poDS;
    GUInt32     nTile = nBlockYOff * poGDS->nXTiles + nBlockXOff;
    GUInt32     nTileBytes = poGDS->paiTiles[2 * nTile + 1];
    GUInt32     i, nCurBlockYSize;

    memset( pImage, 0, nBlockBytes );

    if ( poGDS->sHeader.nLastTileHeight
         && (GUInt32) nBlockYOff == poGDS->nYTiles - 1 )
        nCurBlockYSize = poGDS->sHeader.nLastTileHeight;
    else
        nCurBlockYSize = nBlockYSize;

    if ( VSIFSeekL( poGDS->fp, poGDS->paiTiles[2 * nTile], SEEK_SET ) < 0 )
    {
        // XXX: We will not report error here, because file just may be
	// in update state and data for this block will be available later
        if( poGDS->eAccess == GA_Update )
            return CE_None;
        else
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Can't seek to offset %ld in input file to read data.",
                      poGDS->paiTiles[2 * nTile] );
            return CE_Failure;
        }
    }

    if ( poGDS->nBands == 1 &&
         ( poGDS->sHeader.nBitDepth == 8
           || poGDS->sHeader.nBitDepth == 16
           || poGDS->sHeader.nBitDepth == 32
           || poGDS->sHeader.nBitDepth == 64 ) )
    {
        if ( nTileBytes > nBlockBytes )
            nTileBytes = nBlockBytes;
    
        if ( VSIFReadL( pImage, 1, nTileBytes, poGDS->fp ) < nTileBytes )
        {
            // XXX
            if( poGDS->eAccess == GA_Update )
                return CE_None;
            else
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Can't read from offset %ld in input file.",
                          poGDS->paiTiles[2 * nTile] );
                // XXX: Do not fail here, just return empty block and continue
                // reading.
                return CE_None;
            }
        }

#ifdef CPL_MSB
        if ( poGDS->eRMFType == RMFT_MTW )
        {
            if ( poGDS->sHeader.nBitDepth == 16 )
            {
                for ( i = 0; i < nTileBytes; i += 2 )
                    CPL_SWAP16PTR( (GByte*)pImage + i );
            }

            else if ( poGDS->sHeader.nBitDepth == 32 )
            {
                for ( i = 0; i < nTileBytes; i += 4 )
                    CPL_SWAP32PTR( (GByte*)pImage + i );
            }

            else if ( poGDS->sHeader.nBitDepth == 64 )
            {
                for ( i = 0; i < nTileBytes; i += 8 )
                    CPL_SWAPDOUBLE( (GByte*)pImage + i );
            }
        }
#endif

    }

    else if ( poGDS->eRMFType == RMFT_RSW )
    {
        GByte   *pabyTile = (GByte *) CPLMalloc( nTileBytes );

        if ( VSIFReadL( pabyTile, 1, nTileBytes, poGDS->fp ) < nTileBytes )
        {
            // XXX
            if( poGDS->eAccess == GA_Update )
            {
                CPLFree( pabyTile );
                return CE_None;
            }
            else
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Can't read from offset %ld in input file.",
                          poGDS->paiTiles[2 * nTile] );
                // XXX: Do not fail here, just return empty block and continue
                // reading.
                CPLFree( pabyTile );
                return CE_None;
            }
        }

/* -------------------------------------------------------------------- */
/*  If buffer was compressed, decompress it first.                      */
/* -------------------------------------------------------------------- */
        GUInt32 nRawBytes;
        if ( nLastTileXBytes && (GUInt32) nBlockXOff == poGDS->nXTiles - 1 )
        {
            nRawBytes = nLastTileXBytes;
            if (nLastTileHeight && (GUInt32) nBlockYOff == poGDS->nYTiles - 1)
                nRawBytes *= nLastTileHeight;
            else
                nRawBytes *= nBlockYSize;
        }
        else
        {
            nRawBytes = poGDS->nBands * nBlockXSize * nDataSize;
            if (nLastTileHeight && (GUInt32) nBlockYOff == poGDS->nYTiles - 1)
                nRawBytes *= nLastTileHeight;
            else
                nRawBytes *= nBlockYSize;
        }

        if (poGDS->Decompress && nRawBytes > nTileBytes)
        {
            GByte *pszRawBuf = (GByte *)CPLMalloc(nRawBytes);

            (*poGDS->Decompress)(pabyTile, nTileBytes, pszRawBuf, nRawBytes);
            CPLFree(pabyTile);
            pabyTile = pszRawBuf;
            nTileBytes = nRawBytes;
        }

/* -------------------------------------------------------------------- */
/*  Deinterleave pixels from input buffer.                              */
/* -------------------------------------------------------------------- */
        if ( poGDS->sHeader.nBitDepth == 24 || poGDS->sHeader.nBitDepth == 32 )
        {
            GUInt32 nTileSize = nTileBytes / nBytesPerPixel;

            if ( nTileSize > nBlockSize )
                nTileSize = nBlockSize;

            for ( i = 0; i < nTileSize; i++ )
            {
                // Colour triplets in RMF file organized in reverse order:
                // blue, green, red. When we have 32-bit RMF the forth byte
                // in quadriplet should be discarded as it has no meaning.
                // That is why we always use 3 byte count in the following
                // pabyTemp index.
                ((GByte *) pImage)[i] =
                    pabyTile[i * nBytesPerPixel + 3 - nBand];
            }
        }

        else if ( poGDS->sHeader.nBitDepth == 16 )
        {
            GUInt32 nTileSize = nTileBytes / nBytesPerPixel;

            if ( nTileSize > nBlockSize )
                nTileSize = nBlockSize;

            for ( i = 0; i < nTileSize; i++ )
            {
                switch ( nBand )
                {
                    case 1:
                        ((GByte *) pImage)[i] =
                            (GByte)((((GUInt16*)pabyTile)[i] & 0x7c00) >> 7);
                        break;
                    case 2:
                        ((GByte *) pImage)[i] =
                            (GByte)((((GUInt16*)pabyTile)[i] & 0x03e0) >> 2);
                        break;
                    case 3:
                        ((GByte *) pImage)[i] =
                            (GByte)(((GUInt16*)pabyTile)[i] & 0x1F) << 3;
                        break;
                    default:
                        break;
                }
            }
        }

        else if ( poGDS->sHeader.nBitDepth == 4 )
        {
            GByte *pabyTemp = pabyTile;

            for ( i = 0; i < nBlockSize; i++ )
            {
                // Most significant part of the byte represents leftmost pixel
                if ( i & 0x01 )
                    ((GByte *) pImage)[i] = *pabyTemp++ & 0x0F;
                else
                    ((GByte *) pImage)[i] = (*pabyTemp & 0xF0) >> 4;
            }
        }

        else if ( poGDS->sHeader.nBitDepth == 1 )
        {
            GByte *pabyTemp = pabyTile;

            for ( i = 0; i < nBlockSize; i++ )
            {
                switch ( i & 0x7 )
                {
                    case 0:
                        ((GByte *) pImage)[i] = (*pabyTemp & 0x80) >> 7;
                        break;
                    case 1:
                        ((GByte *) pImage)[i] = (*pabyTemp & 0x40) >> 6;
                        break;
                    case 2:
                        ((GByte *) pImage)[i] = (*pabyTemp & 0x20) >> 5;
                        break;
                    case 3:
                        ((GByte *) pImage)[i] = (*pabyTemp & 0x10) >> 4;
                        break;
                    case 4:
                        ((GByte *) pImage)[i] = (*pabyTemp & 0x08) >> 3;
                        break;
                    case 5:
                        ((GByte *) pImage)[i] = (*pabyTemp & 0x04) >> 2;
                        break;
                    case 6:
                        ((GByte *) pImage)[i] = (*pabyTemp & 0x02) >> 1;
                        break;
                    case 7:
                        ((GByte *) pImage)[i] = *pabyTemp++ & 0x01;
                        break;
                    default:
                        break;
                }
            }
        }

        CPLFree( pabyTile );
    }

    if ( nLastTileXBytes
         && (GUInt32) nBlockXOff == poGDS->nXTiles - 1 )
    {
        GUInt32 iRow;

        for ( iRow = nCurBlockYSize - 1; iRow > 0; iRow-- )
        {
            memmove( (GByte *)pImage + nBlockXSize * iRow * nDataSize,
                     (GByte *)pImage + iRow * nLastTileXBytes,
                     nLastTileXBytes );
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
    RMFDataset  *poGDS = (RMFDataset *)poDS;
    GUInt32     nTile = nBlockYOff * poGDS->nXTiles + nBlockXOff;
    GUInt32     nTileBytes = nDataSize * poGDS->nBands;
    GUInt32     iInPixel, iOutPixel, nCurBlockYSize;
    GByte       *pabyTile;

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

    if ( poGDS->paiTiles[2 * nTile] )
    {
        if ( VSIFSeekL( poGDS->fp, poGDS->paiTiles[2 * nTile], SEEK_SET ) < 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                "Can't seek to offset %ld in output file to write data.\n%s",
                poGDS->paiTiles[2 * nTile], VSIStrerror( errno ) );
            return CE_Failure;
        }
    }
    else
    {
        if ( VSIFSeekL( poGDS->fp, 0, SEEK_END ) < 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                "Can't seek to offset %ld in output file to write data.\n%s",
                poGDS->paiTiles[2 * nTile], VSIStrerror( errno ) );
            return CE_Failure;
        }
        poGDS->paiTiles[2 * nTile] = VSIFTellL( poGDS->fp );

        poGDS->bHeaderDirty = TRUE;
    }

    if ( nLastTileXBytes
         && (GUInt32) nBlockXOff == poGDS->nXTiles - 1 )
        nTileBytes *= poGDS->sHeader.nLastTileWidth;
    else
        nTileBytes *= nBlockXSize;

    if ( poGDS->sHeader.nLastTileHeight
         && (GUInt32) nBlockYOff == poGDS->nYTiles - 1 )
        nCurBlockYSize = poGDS->sHeader.nLastTileHeight;
    else
        nCurBlockYSize = nBlockYSize;

    nTileBytes *= nCurBlockYSize;
        
    pabyTile = (GByte *) CPLMalloc( nTileBytes );
    if ( !pabyTile )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Can't allocate space for the tile buffer.\n%s",
                  VSIStrerror( errno ) );
        return CE_Failure;
    }

    if ( nLastTileXBytes
         && (GUInt32) nBlockXOff == poGDS->nXTiles - 1 )
    {
        GUInt32 iRow;

        if ( poGDS->nBands == 1 )
        {
            for ( iRow = 0; iRow < nCurBlockYSize; iRow++ )
            {
                memcpy( pabyTile + iRow * nLastTileXBytes,
                         (GByte*)pImage + nBlockXSize * iRow * nDataSize,
                         nLastTileXBytes );
            }
        }
        else
        {
            memset( pabyTile, 0, nTileBytes );
            if ( poGDS->paiTiles[2 * nTile + 1] )
            {
                VSIFReadL( pabyTile, 1, nTileBytes, poGDS->fp );
                VSIFSeekL( poGDS->fp, poGDS->paiTiles[2 * nTile], SEEK_SET );
            }

            for ( iRow = 0; iRow < nCurBlockYSize; iRow++ )
            {
                for ( iInPixel = 0, iOutPixel = nBytesPerPixel - nBand;
                      iOutPixel < nLastTileXBytes * poGDS->nBands;
                      iInPixel++, iOutPixel += poGDS->nBands )
                    (pabyTile + iRow * nLastTileXBytes * poGDS->nBands)[iOutPixel] =
                        ((GByte *) pImage + nBlockXSize * iRow * nDataSize)[iInPixel];
            }
        }
    }
    else
    {
        if ( poGDS->nBands == 1 )
            memcpy( pabyTile, pImage, nTileBytes );
        else
        {
            memset( pabyTile, 0, nTileBytes );
            if ( poGDS->paiTiles[2 * nTile + 1] )
            {
                VSIFReadL( pabyTile, 1, nTileBytes, poGDS->fp );
                VSIFSeekL( poGDS->fp, poGDS->paiTiles[2 * nTile], SEEK_SET );
            }

            for ( iInPixel = 0, iOutPixel = nBytesPerPixel - nBand;
                  iOutPixel < nTileBytes;
                  iInPixel++, iOutPixel += poGDS->nBands )
                pabyTile[iOutPixel] = ((GByte *) pImage)[iInPixel];

        }
    }
    
#ifdef CPL_MSB
    if ( poGDS->eRMFType == RMFT_MTW )
    {
        GUInt32 i;

        if ( poGDS->sHeader.nBitDepth == 16 )
        {
            for ( i = 0; i < nTileBytes; i += 2 )
                CPL_SWAP16PTR( pabyTile + i );
        }

        else if ( poGDS->sHeader.nBitDepth == 32 )
        {
            for ( i = 0; i < nTileBytes; i += 4 )
                CPL_SWAP32PTR( pabyTile + i );
        }

        else if ( poGDS->sHeader.nBitDepth == 64 )
        {
            for ( i = 0; i < nTileBytes; i += 8 )
                CPL_SWAPDOUBLE( pabyTile + i );
        }
    }
#endif

    if ( VSIFWriteL( pabyTile, 1, nTileBytes, poGDS->fp ) < nTileBytes )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Can't write block with X offset %d and Y offset %d.\n%s",
                  nBlockXOff, nBlockYOff, VSIStrerror( errno ) );
        CPLFree( pabyTile );
        return CE_Failure;
    }
    
    poGDS->paiTiles[2 * nTile + 1] = nTileBytes;
    CPLFree( pabyTile );

    poGDS->bHeaderDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *RMFRasterBand::GetColorTable()
{
    RMFDataset   *poGDS = (RMFDataset *) poDS;

    return poGDS->poColorTable;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr RMFRasterBand::SetColorTable( GDALColorTable *poColorTable )
{
    RMFDataset  *poGDS = (RMFDataset *) poDS;

    if ( poColorTable )
    {
        if ( poGDS->eRMFType == RMFT_RSW && poGDS->nBands == 1 )
        {
            GDALColorEntry  oEntry;
            GUInt32         i;

            if ( !poGDS->pabyColorTable )
                return CE_Failure;

            for( i = 0; i < poGDS->nColorTableSize; i++ )
            {
                poColorTable->GetColorEntryAsRGB( i, &oEntry );
                poGDS->pabyColorTable[i * 4] = (GByte) oEntry.c1;     // Red
                poGDS->pabyColorTable[i * 4 + 1] = (GByte) oEntry.c2; // Green
                poGDS->pabyColorTable[i * 4 + 2] = (GByte) oEntry.c3; // Blue
                poGDS->pabyColorTable[i * 4 + 3] = 0;
            }

            poGDS->bHeaderDirty = TRUE;
        }
    }
    else
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp RMFRasterBand::GetColorInterpretation()
{
    RMFDataset      *poGDS = (RMFDataset *) poDS;

    if( poGDS->nBands == 3 )
    {
        if( nBand == 1 )
            return GCI_RedBand;
        else if( nBand == 2 )
            return GCI_GreenBand;
        else if( nBand == 3 )
            return GCI_BlueBand;
        else
            return GCI_Undefined;
    }
    else
    {
        if ( poGDS->eRMFType == RMFT_RSW )
            return GCI_PaletteIndex;
        else
            return GCI_Undefined;
    }
}

/************************************************************************/
/* ==================================================================== */
/*                              RMFDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           RMFDataset()                               */
/************************************************************************/

RMFDataset::RMFDataset()
{
    pszFilename = NULL;
    fp = NULL;
    nBands = 0;
    nXTiles = 0;
    nYTiles = 0;
    paiTiles = NULL;
    pszProjection = CPLStrdup( "" );
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    pabyColorTable = NULL;
    poColorTable = NULL;
    memset( abyHeader, 0, RMF_HEADER_SIZE ); 
    memset( &sHeader, 0, sizeof(sHeader) );

    bHeaderDirty = FALSE;
}

/************************************************************************/
/*                            ~RMFDataset()                             */
/************************************************************************/

RMFDataset::~RMFDataset()
{
    FlushCache();

    if ( paiTiles )
        CPLFree( paiTiles );
    if ( pszProjection )
        CPLFree( pszProjection );
    if ( pabyColorTable )
        CPLFree( pabyColorTable );
    if ( poColorTable != NULL )
        delete poColorTable;
    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr RMFDataset::GetGeoTransform( double * padfTransform )
{
    memcpy( padfTransform, adfGeoTransform, sizeof(adfGeoTransform[0]) * 6 );

    if( sHeader.iGeorefFlag )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr RMFDataset::SetGeoTransform( double * padfTransform )
{
    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );
    sHeader.dfPixelSize = adfGeoTransform[1];
    if ( sHeader.dfPixelSize != 0.0 )
        sHeader.dfResolution = sHeader.dfScale / sHeader.dfPixelSize;
    sHeader.dfLLX = adfGeoTransform[0];
    sHeader.dfLLY = adfGeoTransform[3] - nRasterYSize * sHeader.dfPixelSize;
    sHeader.iGeorefFlag = 1;

    bHeaderDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *RMFDataset::GetProjectionRef()
{
    if( pszProjection )
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr RMFDataset::SetProjection( const char * pszNewProjection )

{
    if ( pszProjection )
        CPLFree( pszProjection );
    pszProjection = CPLStrdup( (pszNewProjection) ? pszNewProjection : "" );

    bHeaderDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                           WriteHeader()                              */
/************************************************************************/

#define RMF_WRITE_LONG(value, offset)               \
{                                                   \
    GInt32  iLong = CPL_LSBWORD32( value );         \
    memcpy( abyHeader + (offset), &iLong, 4 );      \
}

#define RMF_WRITE_ULONG(value, offset)              \
{                                                   \
    GUInt32 iULong = CPL_LSBWORD32( value );        \
    memcpy( abyHeader + (offset), &iULong, 4 );     \
}

#define RMF_WRITE_DOUBLE(value, offset)             \
{                                                   \
    double  dfDouble = (value);                     \
    CPL_LSBPTR64( &dfDouble );                      \
    memcpy( abyHeader + (offset), &dfDouble, 8 );   \
}

CPLErr RMFDataset::WriteHeader()
{
/* -------------------------------------------------------------------- */
/*  Setup projection.                                                   */
/* -------------------------------------------------------------------- */
    if( pszProjection && !EQUAL( pszProjection, "" ) )
    {
        OGRSpatialReference oSRS;
        long            iDatum, iEllips, iZone;
        char            *pszProj =  pszProjection;

        if ( oSRS.importFromWkt( &pszProj ) == OGRERR_NONE )
        {
            oSRS.exportToPanorama((long *)&sHeader.iProjection,
                                  &iDatum, &iEllips, &iZone,
                                  &sHeader.dfStdP1, &sHeader.dfStdP2,
                                  &sHeader.dfCenterLat, &sHeader.dfCenterLong);
        }
    }

/* -------------------------------------------------------------------- */
/*  Write out the file header.                                          */
/* -------------------------------------------------------------------- */
    memcpy( abyHeader, sHeader.szSignature, RMF_SIGNATURE_SIZE );
    RMF_WRITE_ULONG( sHeader.iVersion, 4 );
    // Длина
    RMF_WRITE_ULONG( sHeader.nOvrOffset, 12 );
    RMF_WRITE_ULONG( sHeader.iUserID, 16 );
    memcpy( abyHeader + 20, sHeader.byName, RMF_NAME_SIZE );
    RMF_WRITE_ULONG( sHeader.nBitDepth, 52 );
    RMF_WRITE_ULONG( sHeader.nHeight, 56 );
    RMF_WRITE_ULONG( sHeader.nWidth, 60 );
    RMF_WRITE_ULONG( sHeader.nXTiles, 64 );
    RMF_WRITE_ULONG( sHeader.nYTiles, 68 );
    RMF_WRITE_ULONG( sHeader.nTileHeight, 72 );
    RMF_WRITE_ULONG( sHeader.nTileWidth, 76 );
    RMF_WRITE_ULONG( sHeader.nLastTileHeight, 80 );
    RMF_WRITE_ULONG( sHeader.nLastTileWidth, 84 );
    RMF_WRITE_ULONG( sHeader.nROIOffset, 88 );
    RMF_WRITE_ULONG( sHeader.nROISize, 92 );
    RMF_WRITE_ULONG( sHeader.nClrTblOffset, 96 );
    RMF_WRITE_ULONG( sHeader.nClrTblSize, 100 );
    RMF_WRITE_ULONG( sHeader.nTileTblOffset, 104 );
    RMF_WRITE_ULONG( sHeader.nTileTblSize, 108 );
    RMF_WRITE_LONG( sHeader.iMapType, 124 );
    RMF_WRITE_LONG( sHeader.iProjection, 128 );
    RMF_WRITE_DOUBLE( sHeader.dfScale, 136 );
    RMF_WRITE_DOUBLE( sHeader.dfResolution, 144 );
    RMF_WRITE_DOUBLE( sHeader.dfPixelSize, 152 );
    RMF_WRITE_DOUBLE( sHeader.dfLLX, 160 );
    RMF_WRITE_DOUBLE( sHeader.dfLLY, 168 );
    RMF_WRITE_DOUBLE( sHeader.dfStdP1, 176 );
    RMF_WRITE_DOUBLE( sHeader.dfStdP2, 184 );
    RMF_WRITE_DOUBLE( sHeader.dfCenterLong, 192 );
    RMF_WRITE_DOUBLE( sHeader.dfCenterLat, 200 );
    *(abyHeader + 208) = sHeader.iCompression;
    *(abyHeader + 209) = sHeader.iMaskType;
    *(abyHeader + 210) = sHeader.iMaskStep;
    *(abyHeader + 211) = sHeader.iFrameFlag;
    RMF_WRITE_ULONG( sHeader.nFlagsTblOffset, 212 );
    RMF_WRITE_ULONG( sHeader.nFlagsTblSize, 216 );
    RMF_WRITE_ULONG( sHeader.nFileSize0, 220 );
    RMF_WRITE_ULONG( sHeader.nFileSize1, 224 );
    *(abyHeader + 228) = sHeader.iUnknown;
    *(abyHeader + 244) = sHeader.iGeorefFlag;
    *(abyHeader + 245) = sHeader.iInverse;
    memcpy( abyHeader + 248, sHeader.abyInvisibleColors,
            RMF_INVISIBLE_COLORS_SIZE );
    RMF_WRITE_DOUBLE( sHeader.dfElevMinMax[0], 280 );
    RMF_WRITE_DOUBLE( sHeader.dfElevMinMax[1], 288 );
    RMF_WRITE_DOUBLE( sHeader.dfNoData, 296 );
    RMF_WRITE_ULONG( sHeader.iElevationUnit, 304 );
    RMF_WRITE_ULONG( sHeader.iElevationType, 308 );

    VSIFSeekL( fp, 0, SEEK_SET );
    VSIFWriteL( abyHeader, 1, RMF_HEADER_SIZE, fp );

/* -------------------------------------------------------------------- */
/*  Write out the color table.                                          */
/* -------------------------------------------------------------------- */
    if ( sHeader.nClrTblOffset && sHeader.nClrTblSize )
    {
        VSIFSeekL( fp, sHeader.nClrTblOffset, SEEK_SET );
        VSIFWriteL( pabyColorTable, 1, sHeader.nClrTblSize, fp );
    }

/* -------------------------------------------------------------------- */
/*  Write out the block table.                                          */
/* -------------------------------------------------------------------- */
    VSIFWriteL( paiTiles, 1, sHeader.nTileTblSize, fp );

    bHeaderDirty = FALSE;

    return CE_None;
}

#undef RMF_WRITE_DOUBLE
#undef RMF_WRITE_ULONG
#undef RMF_WRITE_LONG

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void RMFDataset::FlushCache()

{
    GDALDataset::FlushCache();

    if ( !bHeaderDirty )
        return;

    if ( eRMFType == RMFT_MTW )
        GDALComputeRasterMinMax( GetRasterBand(1), FALSE,
                                 sHeader.dfElevMinMax );
    WriteHeader();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RMFDataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( poOpenInfo->fp == NULL )
        return NULL;

    if( memcmp(poOpenInfo->pabyHeader, "RSW\0", 4) != 0
        && memcmp(poOpenInfo->pabyHeader, "MTW\0", 4) != 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*  Create a corresponding GDALDataset.                                 */
/* -------------------------------------------------------------------- */
    RMFDataset      *poDS;

    poDS = new RMFDataset();

    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    else
        poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "r+b" );
    if ( !poDS->fp )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*  Read the file header.                                               */
/* -------------------------------------------------------------------- */
#define RMF_READ_ULONG(value, offset)                               \
    (value) = CPL_LSBWORD32(*(GUInt32*)(poDS->abyHeader + (offset)))

#define RMF_READ_LONG(value, offset)                                \
    (value) = CPL_LSBWORD32(*(GInt32*)(poDS->abyHeader + (offset)))

#define RMF_READ_DOUBLE(value, offset)                              \
{                                                                   \
    (value) = *(double*)(poDS->abyHeader + (offset));               \
    CPL_LSBPTR64(&(value));                                         \
}

    VSIFSeekL( poDS->fp, 0, SEEK_SET );
    VSIFReadL( poDS->abyHeader, 1, RMF_HEADER_SIZE, poDS->fp );

    memcpy( poDS->sHeader.szSignature, poDS->abyHeader, RMF_SIGNATURE_SIZE );
    poDS->sHeader.szSignature[3] = '\0'; // Paranoid
    RMF_READ_ULONG( poDS->sHeader.iVersion, 4 );
    RMF_READ_ULONG( poDS->sHeader.nSize, 8 );
    RMF_READ_ULONG( poDS->sHeader.nOvrOffset, 12 );
    RMF_READ_ULONG( poDS->sHeader.iUserID, 16 );
    memcpy( poDS->sHeader.byName, poDS->abyHeader + 20, RMF_NAME_SIZE );
    poDS->sHeader.byName[31] = '\0';
    RMF_READ_ULONG( poDS->sHeader.nBitDepth, 52 );
    RMF_READ_ULONG( poDS->sHeader.nHeight, 56 );
    RMF_READ_ULONG( poDS->sHeader.nWidth, 60 );
    RMF_READ_ULONG( poDS->sHeader.nXTiles, 64 );
    RMF_READ_ULONG( poDS->sHeader.nYTiles, 68 );
    RMF_READ_ULONG( poDS->sHeader.nTileHeight, 72 );
    RMF_READ_ULONG( poDS->sHeader.nTileWidth, 76 );
    RMF_READ_ULONG( poDS->sHeader.nLastTileHeight, 80 );
    RMF_READ_ULONG( poDS->sHeader.nLastTileWidth, 84 );
    RMF_READ_ULONG( poDS->sHeader.nROIOffset, 88 );
    RMF_READ_ULONG( poDS->sHeader.nROISize, 92 );
    RMF_READ_ULONG( poDS->sHeader.nClrTblOffset, 96 );
    RMF_READ_ULONG( poDS->sHeader.nClrTblSize, 100 );
    RMF_READ_ULONG( poDS->sHeader.nTileTblOffset, 104 );
    RMF_READ_ULONG( poDS->sHeader.nTileTblSize, 108 );
    RMF_READ_LONG( poDS->sHeader.iMapType, 124 );
    RMF_READ_LONG( poDS->sHeader.iProjection, 128 );
    RMF_READ_DOUBLE( poDS->sHeader.dfScale, 136 );
    RMF_READ_DOUBLE( poDS->sHeader.dfResolution, 144 );
    RMF_READ_DOUBLE( poDS->sHeader.dfPixelSize, 152 );
    RMF_READ_DOUBLE( poDS->sHeader.dfLLX, 160 );
    RMF_READ_DOUBLE( poDS->sHeader.dfLLY, 168 );
    RMF_READ_DOUBLE( poDS->sHeader.dfStdP1, 176 );
    RMF_READ_DOUBLE( poDS->sHeader.dfStdP2, 184 );
    RMF_READ_DOUBLE( poDS->sHeader.dfCenterLong, 192 );
    RMF_READ_DOUBLE( poDS->sHeader.dfCenterLat, 200 );
    poDS->sHeader.iCompression = *(poDS->abyHeader + 208);
    poDS->sHeader.iMaskType = *(poDS->abyHeader + 209);
    poDS->sHeader.iMaskStep = *(poDS->abyHeader + 210);
    poDS->sHeader.iFrameFlag = *(poDS->abyHeader + 211);
    RMF_READ_ULONG( poDS->sHeader.nFlagsTblOffset, 212 );
    RMF_READ_ULONG( poDS->sHeader.nFlagsTblSize, 216 );
    RMF_READ_ULONG( poDS->sHeader.nFileSize0, 220 );
    RMF_READ_ULONG( poDS->sHeader.nFileSize1, 224 );
    poDS->sHeader.iUnknown = *(poDS->abyHeader + 228);
    poDS->sHeader.iGeorefFlag = *(poDS->abyHeader + 244);
    poDS->sHeader.iInverse = *(poDS->abyHeader + 245);
    memcpy( poDS->sHeader.abyInvisibleColors,
            poDS->abyHeader + 248, RMF_INVISIBLE_COLORS_SIZE );
    RMF_READ_DOUBLE( poDS->sHeader.dfElevMinMax[0], 280 );
    RMF_READ_DOUBLE( poDS->sHeader.dfElevMinMax[1], 288 );
    RMF_READ_DOUBLE( poDS->sHeader.dfNoData, 296 );
    RMF_READ_ULONG( poDS->sHeader.iElevationUnit, 304 );
    RMF_READ_ULONG( poDS->sHeader.iElevationType, 308 );

#undef RMF_READ_DOUBLE
#undef RMF_READ_LONG
#undef RMF_READ_ULONG

#ifdef DEBUG
    CPLDebug( "RMF", "%s image has width %d, height %d, bit depth %d, "
              "compression scheme %d",
              poDS->sHeader.szSignature, poDS->sHeader.nWidth,
              poDS->sHeader.nHeight, poDS->sHeader.nBitDepth,
              poDS->sHeader.iCompression );
    CPLDebug( "RMF", "Size %d, offset to overview 0x%x, user ID %d, "
              "ROI offset 0x%x, ROI size %d",
              poDS->sHeader.nSize, poDS->sHeader.nOvrOffset,
              poDS->sHeader.iUserID, poDS->sHeader.nROIOffset,
              poDS->sHeader.nROISize );
    CPLDebug( "RMF", "Map type %d, projection %d, scale %f, resolution %f, ",
              poDS->sHeader.iMapType, poDS->sHeader.iProjection,
              poDS->sHeader.dfScale, poDS->sHeader.dfResolution );
    CPLDebug( "RMF", "Georeferencing: pixel size %f, LLX %f, LLY %f",
              poDS->sHeader.dfPixelSize,
              poDS->sHeader.dfLLX, poDS->sHeader.dfLLY );
#endif

/* -------------------------------------------------------------------- */
/*  Read array of blocks offsets/sizes.                                 */
/* -------------------------------------------------------------------- */
    GUInt32 i;

    if ( VSIFSeekL( poDS->fp, poDS->sHeader.nTileTblOffset, SEEK_SET ) < 0)
    {
        delete poDS;
        return NULL;
    }

    poDS->paiTiles = (GUInt32 *)CPLMalloc( poDS->sHeader.nTileTblSize );
    if ( !poDS->paiTiles )
    {
        delete poDS;
        return NULL;
    }

    if ( VSIFReadL( poDS->paiTiles, 1,
                    poDS->sHeader.nTileTblSize, poDS->fp ) < 0 )
    {
        CPLDebug( "RMF", "Can't read tiles offsets/sizes table." );
        delete poDS;
        return NULL;
    }

#if DEBUG
    CPLDebug( "RMF", "List of block offsets/sizes:" );

    for ( i = 0; i < poDS->sHeader.nTileTblSize / 4; i += 2 )
        CPLDebug( "RMF", "    %d / %d",
                  poDS->paiTiles[i], poDS->paiTiles[i + 1] );
#endif

/* -------------------------------------------------------------------- */
/*  Set up essential image parameters.                                  */
/* -------------------------------------------------------------------- */
    GDALDataType eType = GDT_Byte;

    poDS->eRMFType =
        ( EQUAL( poDS->sHeader.szSignature, "MTW" ) ) ? RMFT_MTW : RMFT_RSW;
    poDS->nRasterXSize = poDS->sHeader.nWidth;
    poDS->nRasterYSize = poDS->sHeader.nHeight;

    if ( poDS->eRMFType == RMFT_RSW )
    {
        switch ( poDS->sHeader.nBitDepth )
        {
            case 32:
            case 24:
            case 16:
                poDS->nBands = 3;
                break;
            case 1:
            case 4:
            case 8:
                {
                    // Allocate memory for colour table and read it
                    poDS->nColorTableSize = 1 << poDS->sHeader.nBitDepth;
                    if ( poDS->nColorTableSize * 4 > poDS->sHeader.nClrTblSize )
                    {
                        CPLDebug( "RMF",
                                  "Wrong color table size. Expected %d, got %d.",
                                  poDS->nColorTableSize * 4,
                                  poDS->sHeader.nClrTblSize );
                        delete poDS;
                        return NULL;
                    }
                    poDS->pabyColorTable =
                        (GByte *)CPLMalloc( poDS->sHeader.nClrTblSize );
                    if ( VSIFSeekL( poDS->fp, poDS->sHeader.nClrTblOffset,
                                    SEEK_SET ) < 0 )
                    {
                        CPLDebug( "RMF", "Can't seek to color table location." );
                        delete poDS;
                        return NULL;
                    }
                    if ( VSIFReadL( poDS->pabyColorTable, 1,
                                    poDS->sHeader.nClrTblSize, poDS->fp )
                         < poDS->sHeader.nClrTblSize )
                    {
                        CPLDebug( "RMF", "Can't read color table." );
                        delete poDS;
                        return NULL;
                    }

                    GDALColorEntry oEntry;
                    poDS->poColorTable = new GDALColorTable();
                    for( i = 0; i < poDS->nColorTableSize; i++ )
                    {
                        oEntry.c1 = poDS->pabyColorTable[i * 4];     // Red
                        oEntry.c2 = poDS->pabyColorTable[i * 4 + 1]; // Green
                        oEntry.c3 = poDS->pabyColorTable[i * 4 + 2]; // Blue
                        oEntry.c4 = 255;                             // Alpha

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
        if ( poDS->sHeader.nBitDepth == 8 )
            eType = GDT_Byte;
        else if ( poDS->sHeader.nBitDepth == 16 )
            eType = GDT_Int16;
        else if ( poDS->sHeader.nBitDepth == 32 )
            eType = GDT_Int32;
        else if ( poDS->sHeader.nBitDepth == 64 )
            eType = GDT_Float64;
    }

    poDS->nXTiles = ( poDS->nRasterXSize + poDS->sHeader.nTileWidth - 1 ) /
        poDS->sHeader.nTileWidth;
    poDS->nYTiles = ( poDS->nRasterYSize + poDS->sHeader.nTileHeight - 1 ) /
        poDS->sHeader.nTileHeight;

#if DEBUG
    CPLDebug( "RMF", "Image is %d tiles wide, %d tiles long",
              poDS->nXTiles, poDS->nYTiles );
#endif

/* -------------------------------------------------------------------- */
/*  Choose compression scheme.                                          */
/* -------------------------------------------------------------------- */
    if (poDS->sHeader.iCompression == 1)
        poDS->Decompress = &LZWDecompress;
    else
        poDS->Decompress = NULL;

/* -------------------------------------------------------------------- */
/*  Create band information objects.                                    */
/* -------------------------------------------------------------------- */
    int iBand;

    for( iBand = 1; iBand <= poDS->nBands; iBand++ )
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
        GInt32  nProj =
            (poDS->sHeader.iProjection) ? poDS->sHeader.iProjection : 1L;

        // XXX: Ellipsoid and datum are not specified in RMF file, but they
        // are always Krassovsky/Pulkovo, 1942.
        oSRS.importFromPanorama( nProj, 1, 1, 0,
                                 poDS->sHeader.dfStdP1, poDS->sHeader.dfStdP2,
                                 poDS->sHeader.dfCenterLat,
                                 poDS->sHeader.dfCenterLong );
        if ( poDS->pszProjection )
            CPLFree( poDS->pszProjection );
        oSRS.exportToWkt( &poDS->pszProjection );
    }

/* -------------------------------------------------------------------- */
/*  Set up georeferencing.                                              */
/* -------------------------------------------------------------------- */
    if ( (poDS->eRMFType == RMFT_RSW && poDS->sHeader.iGeorefFlag) ||
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

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *RMFDataset::Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char **papszParmList )

{
    if ( nBands != 1 && nBands != 3 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "RMF driver doesn't support %d bands. Must be 1 or 3.\n",
                  nBands );

        return NULL;
    }

    if ( nBands == 1
         && eType != GDT_Byte
         && eType != GDT_Int16
         && eType != GDT_Int32
         && eType != GDT_Float64 )
    {
         CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create RMF dataset with an illegal data type (%s),\n"
              "only Byte, Int16, Int32 and Float64 types supported "
              "by the format for single-band images.\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

    if ( nBands == 3 && eType != GDT_Byte )
    {
         CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create RMF dataset with an illegal data type (%s),\n"
              "only Byte type supported by the format for three-band images.\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*  Create the dataset.                                                 */
/* -------------------------------------------------------------------- */
    RMFDataset      *poDS;

    poDS = new RMFDataset();

    poDS->fp = VSIFOpenL( pszFilename, "wb+" );
    if( poDS->fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "Unable to create file %s.\n",
                  pszFilename );
        return NULL;
    }

    poDS->pszFilename = pszFilename;

/* -------------------------------------------------------------------- */
/*  Fill the RMFHeader                                                  */
/* -------------------------------------------------------------------- */
    GUInt32     nTileSize, nCurPtr = 0;
    GUInt32     nBlockXSize =
        ( nXSize < RMF_DEFAULT_BLOCKXSIZE ) ? nXSize : RMF_DEFAULT_BLOCKXSIZE;
    GUInt32     nBlockYSize =
        ( nYSize < RMF_DEFAULT_BLOCKYSIZE ) ? nYSize : RMF_DEFAULT_BLOCKYSIZE;
    const char  *pszValue;

    if ( CSLFetchBoolean( papszParmList, "MTW", FALSE) )
        poDS->eRMFType = RMFT_MTW;
    else
        poDS->eRMFType = RMFT_RSW;
    if ( poDS->eRMFType == RMFT_MTW )
        memcpy( poDS->sHeader.szSignature, "MTW\0", RMF_SIGNATURE_SIZE );
    else
        memcpy( poDS->sHeader.szSignature, "RSW\0", RMF_SIGNATURE_SIZE );
    poDS->sHeader.iVersion = 0x0200;
    poDS->sHeader.nOvrOffset = 0x00;
    poDS->sHeader.iUserID = 0x00;
    memset( poDS->sHeader.byName, 0, RMF_NAME_SIZE );
    poDS->sHeader.nBitDepth = GDALGetDataTypeSize( eType ) * nBands;
    poDS->sHeader.nHeight = nYSize;
    poDS->sHeader.nWidth = nXSize;

    pszValue = CSLFetchNameValue(papszParmList,"BLOCKXSIZE");
    if( pszValue != NULL )
        nBlockXSize = atoi( pszValue );

    pszValue = CSLFetchNameValue(papszParmList,"BLOCKYSIZE");
    if( pszValue != NULL )
        nBlockYSize = atoi( pszValue );

    poDS->sHeader.nTileWidth = nBlockXSize;
    poDS->sHeader.nTileHeight = nBlockYSize;

    poDS->nXTiles = poDS->sHeader.nXTiles =
        ( nXSize + poDS->sHeader.nTileWidth - 1 ) / poDS->sHeader.nTileWidth;
    poDS->nYTiles = poDS->sHeader.nYTiles =
        ( nYSize + poDS->sHeader.nTileHeight - 1 ) / poDS->sHeader.nTileHeight;
    poDS->sHeader.nLastTileHeight = nYSize % poDS->sHeader.nTileHeight;
    if ( !poDS->sHeader.nLastTileHeight )
        poDS->sHeader.nLastTileHeight = poDS->sHeader.nTileHeight;
    poDS->sHeader.nLastTileWidth = nXSize % poDS->sHeader.nTileWidth;
    if ( !poDS->sHeader.nLastTileWidth )
        poDS->sHeader.nLastTileWidth = poDS->sHeader.nTileWidth;
    
    poDS->sHeader.nROIOffset = 0x00;
    poDS->sHeader.nROISize = 0x00;
    nCurPtr += RMF_HEADER_SIZE;

    // Color table
    if ( poDS->eRMFType == RMFT_RSW && nBands == 1 )
    {
        GUInt32 i;

        poDS->sHeader.nClrTblOffset = nCurPtr;
        poDS->nColorTableSize = 1 << poDS->sHeader.nBitDepth;
        poDS->sHeader.nClrTblSize = poDS->nColorTableSize * 4;
        poDS->pabyColorTable = (GByte *) CPLMalloc( poDS->sHeader.nClrTblSize );
        for ( i = 0; i < poDS->nColorTableSize; i++ )
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
    poDS->sHeader.nTileTblOffset = nCurPtr;
    poDS->sHeader.nTileTblSize =
        poDS->sHeader.nXTiles * poDS->sHeader.nYTiles * 4 * 2;
    poDS->paiTiles = (GUInt32 *)CPLMalloc( poDS->sHeader.nTileTblSize );
    memset( poDS->paiTiles, 0, poDS->sHeader.nTileTblSize );
    nCurPtr += poDS->sHeader.nTileTblSize;
    nTileSize = poDS->sHeader.nTileWidth * poDS->sHeader.nTileHeight
        * GDALGetDataTypeSize( eType ) / 8;
    poDS->sHeader.nSize =
        poDS->paiTiles[poDS->sHeader.nTileTblSize / 4 - 2] + nTileSize;

    poDS->sHeader.iMapType = -1;
    poDS->sHeader.iProjection = -1;
    poDS->sHeader.dfScale = 10000.0;
    poDS->sHeader.dfResolution = 100.0;
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
    memset( poDS->sHeader.abyInvisibleColors, 0, RMF_INVISIBLE_COLORS_SIZE );
    poDS->sHeader.dfElevMinMax[0] = 0.0;
    poDS->sHeader.dfElevMinMax[1] = 0.0;
    poDS->sHeader.dfNoData = 0.0;
    poDS->sHeader.iElevationUnit = 0;
    poDS->sHeader.iElevationType = 0;

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->nBands = nBands;

    poDS->WriteHeader();

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int         iBand;

    for( iBand = 1; iBand <= poDS->nBands; iBand++ )
        poDS->SetBand( iBand, new RMFRasterBand( poDS, iBand, eType ) );

    return (GDALDataset *) poDS;
}

/************************************************************************/
/*                        GDALRegister_RMF()                            */
/************************************************************************/

void GDALRegister_RMF()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "RMF" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "RMF" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Raster Matrix Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_rmf.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "rsw" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='MTW' type='boolean' description='Create MTW DEM matrix'/>"
"   <Option name='BLOCKXSIZE' type='int' description='Tile Width'/>"
"   <Option name='BLOCKYSIZE' type='int' description='Tile Height'/>"
"</CreationOptionList>" );

        poDriver->pfnOpen = RMFDataset::Open;
        poDriver->pfnCreate = RMFDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

