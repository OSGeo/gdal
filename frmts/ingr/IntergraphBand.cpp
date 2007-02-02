/*****************************************************************************
 * $Id: $
 *
 * Project:  Intergraph Raster Format support
 * Purpose:  Read/Write Intergraph Raster Format, band support
 * Author:   Ivan Lucena, ivan@ilucena.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Ivan Lucena
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files ( the "Software" ),
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
 *****************************************************************************
 *
 * $Log: $
 *
 */

#include "gdal_priv.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "ogr_spatialref.h"
#include "gdal_pam.h"
#include "gdal_alg.h"
#include "math.h"

#include "IntergraphDataset.h"
#include "IntergraphBand.h"
#include "IngrTypes.h"

//  ----------------------------------------------------------------------------
//                                  IntergraphRasterBand::IntergraphRasterBand()
//  ----------------------------------------------------------------------------

IntergraphRasterBand::IntergraphRasterBand( IntergraphDataset *poDS, 
                                            int nBand,
                                            int nBandOffset,
                                            int nRGorB,
                                            GDALDataType eType )
{
    this->poColorTable  = new GDALColorTable();

    this->poDS          = poDS;
    this->eDataType     = eType;
    this->nRGBIndex     = nRGorB;
    this->nBand         = nBand != 0 ? nBand : poDS->nBands;
    this->nBandStart    = nBandOffset;
    this->nTiles        = 0;
    this->pabyScanLine  = NULL;
    this->pahTiles      = NULL;

    // -------------------------------------------------------------------- 
    // Get Header Info
    // -------------------------------------------------------------------- 

    if( poDS->eAccess == GA_ReadOnly )
    {
        VSIFSeekL( poDS->fp, nBandStart, SEEK_SET );
        VSIFReadL( &hHeaderOne, 1, SIZEOF_HDR1,   poDS->fp );
        VSIFReadL( &hHeaderTwo, 1, SIZEOF_HDR2_A, poDS->fp );
    }
    else
    {
        memcpy( &hHeaderOne, &poDS->hHeaderOne, SIZEOF_HDR1 );
        memcpy( &hHeaderTwo, &poDS->hHeaderTwo, SIZEOF_HDR2_A );
    }

    // -------------------------------------------------------------------- 
    // Get the image start from Words to Follow ( WTF )
    // -------------------------------------------------------------------- 

    nDataOffset = nBandOffset + 2 + ( 2 * ( hHeaderOne.WordsToFollow + 1 ) );

    // -------------------------------------------------------------------- 
    // Get Color Tabel from Color Table Type (CTV)
    // -------------------------------------------------------------------- 

    IngrEnvVTable  hEnvrTable;
    IngrColorTable hIGDSTable;

    uint32 nEntries = hHeaderTwo.NumberOfCTEntries;

    if( ( nEntries > 0 ) && ( hHeaderTwo.ColorTableType == EnvironVColorTable ) )
    {
        // ----------------------------------------------------------------
        // Get EnvironV color table starting at block 3
        // ----------------------------------------------------------------

        hEnvrTable.Entry = (vlt_slot*) CPLCalloc( nEntries, sizeof( vlt_slot ) );

        VSIFSeekL( poDS->fp, nBandOffset + ( 2 * SIZEOF_HDR1 ), SEEK_SET );
        VSIFReadL( hEnvrTable.Entry, nEntries, sizeof( vlt_slot ), poDS->fp );

        GetEnvironColorTable( poColorTable, &hEnvrTable, nEntries );
        CPLFree( hEnvrTable.Entry );
    }
    else if( ( nEntries > 0 ) && ( hHeaderTwo.ColorTableType == IGDSColorTable ) )
    {
        // ----------------------------------------------------------------
        // Get IGDS (fixed size) starting in the middle of block 2 + 1.5 blocks
        // ----------------------------------------------------------------

        VSIFSeekL( poDS->fp, nBandOffset + ( 1.5 * SIZEOF_HDR1 ), SEEK_SET );
        VSIFReadL( hIGDSTable.Entry, 256, sizeof( igds_slot ), poDS->fp );

        GetIGDSColorTable( poColorTable, &hIGDSTable );
    }

    // -------------------------------------------------------------------- 
    // Set Dimension
    // -------------------------------------------------------------------- 

    nRasterXSize  = hHeaderOne.PixelsPerLine;
    nRasterYSize  = hHeaderOne.NumberOfLines;
    
    nBlockYSize   = 1;
    nBlockXSize   = poDS->GetRasterXSize();

    // -------------------------------------------------------------------- 
    // Get tile directory
    // -------------------------------------------------------------------- 

    eFormat = (IngrFormatType) hHeaderOne.DataTypeCode;

    if( hHeaderOne.DataTypeCode == TiledRasterData )
    {
        // ----------------------------------------------------------------
        // Reads tile header and the first tile info
        // ----------------------------------------------------------------

        VSIFSeekL( poDS->fp, nDataOffset, SEEK_SET );
        VSIFReadL( &hTileDir, 1, SIZEOF_TDIR, poDS->fp );

        eFormat = (IngrFormatType) hTileDir.SubTypeCode;

        // ----------------------------------------------------------------
        // Calculate the number of tiles
        // ----------------------------------------------------------------

        nTiles = ceil( (float) nRasterXSize / hTileDir.TileSize ) *
                 ceil( (float) nRasterYSize / hTileDir.TileSize );

        // ----------------------------------------------------------------
        // Load the tile table ( first tile info is already read )
        // ----------------------------------------------------------------

        pahTiles  = (IngrOneTile*) CPLCalloc( nTiles, SIZEOF_TILE );
        pahTiles[0].Start      = hTileDir.First.Start;
        pahTiles[0].Allocated  = hTileDir.First.Allocated;
        pahTiles[0].Used       = hTileDir.First.Used;

        if( nTiles > 1 )
        {
            VSIFReadL( &pahTiles[1], nTiles - 1, SIZEOF_TILE, poDS->fp );
        }

        // ----------------------------------------------------------------
        // Set blocks dimensions based on tiles
        // ----------------------------------------------------------------

        nBlockXSize = hTileDir.TileSize;
        nBlockYSize = hTileDir.TileSize;

        // ----------------------------------------------------------------
        // Number of bytes in a record based on tiles
        // ----------------------------------------------------------------

        nRecordBytes = hTileDir.TileSize 
                     * hTileDir.TileSize 
                     * GDALGetDataTypeSize( eDataType ) / 8;
    }
    else
    {
        // ----------------------------------------------------------------
        // Number of bytes in a record
        // ----------------------------------------------------------------

        nRecordBytes = nRasterXSize * GDALGetDataTypeSize( eDataType ) / 8;
    }
        
    // -------------------------------------------------------------------- 
    // Bands interleaved by pixels
    // -------------------------------------------------------------------- 

    if( nRGBIndex > 0 )
    {
        nRecordBytes *= 3;
    }

    // -------------------------------------------------------------------- 
    // Allocate buffer for a ScanLine
    // -------------------------------------------------------------------- 

    pabyScanLine = (GByte*) CPLMalloc( nRecordBytes );

}

//  ----------------------------------------------------------------------------
//                                 IntergraphRasterBand::~IntergraphRasterBand()
//  ----------------------------------------------------------------------------

IntergraphRasterBand::~IntergraphRasterBand()
{
    CPLFree( pabyScanLine );

    if( pahTiles )
    {
        CPLFree( pahTiles );
    }

    if( poColorTable )
    {
        delete poColorTable;
    }
}

//  ----------------------------------------------------------------------------
//                                            IntergraphRasterBand::GetMinimum()
//  ----------------------------------------------------------------------------

double IntergraphRasterBand::GetMinimum( int *pbSuccess )
{
    if( pbSuccess )
    {
        *pbSuccess = TRUE;
    }

    return GetMinMax( eDataType, hHeaderOne.Minimum ); 
}

//  ----------------------------------------------------------------------------
//                                            IntergraphRasterBand::GetMaximum()
//  ----------------------------------------------------------------------------

double IntergraphRasterBand::GetMaximum( int *pbSuccess )
{
    if( pbSuccess )
    {
        *pbSuccess = TRUE;
    }

    return GetMinMax( eDataType, hHeaderOne.Maximum ); 
}

//  ----------------------------------------------------------------------------
//                                IntergraphRasterBand::GetColorInterpretation()
//  ----------------------------------------------------------------------------

GDALColorInterp IntergraphRasterBand::GetColorInterpretation()
{               
    if( poColorTable->GetColorEntryCount() > 0 )
    {
        return GCI_PaletteIndex;
    }

    switch( nRGBIndex )
    {
    case 1: 
        return GCI_RedBand;
    case 2: 
        return GCI_BlueBand;
    case 3: 
        return GCI_GreenBand;
    default:
        return GCI_GrayIndex;
    }
}

//  ----------------------------------------------------------------------------
//                                         IntergraphRasterBand::GetColorTable()
//  ----------------------------------------------------------------------------

GDALColorTable *IntergraphRasterBand::GetColorTable()
{               
    if( poColorTable->GetColorEntryCount() == 0 )
    {
        return NULL;
    }
    else
    {
        return poColorTable;
    }
}

//  ----------------------------------------------------------------------------
//                                         IntergraphRasterBand::SetColorTable()
//  ----------------------------------------------------------------------------

CPLErr IntergraphRasterBand::SetColorTable( GDALColorTable *poColorTable )
{      
    if( poColorTable == NULL )
    {
        return CE_None;
    }

    if( poColorTable->GetColorEntryCount() == 0 )
    {
        return CE_None;
    }

    poColorTable = poColorTable;

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                         IntergraphRasterBand::SetStatistics()
//  ----------------------------------------------------------------------------

CPLErr IntergraphRasterBand::SetStatistics( double dfMin, 
                                            double dfMax, 
                                            double dfMean, 
                                            double dfStdDev )
{      
    IntergraphDataset *poGDS = ( IntergraphDataset * ) poDS;

    hHeaderOne.Minimum = SetMinMax( eDataType, dfMin );
    hHeaderOne.Maximum = SetMinMax( eDataType, dfMax );
    
    return GDALRasterBand::SetStatistics( dfMin, dfMax, dfMean, dfStdDev );
}

//  ----------------------------------------------------------------------------
//                                            IntergraphRasterBand::IReadBlock()
//  ----------------------------------------------------------------------------

CPLErr IntergraphRasterBand::IReadBlock( int nBlockXOff, 
                                         int nBlockYOff,
                                         void *pImage )
{
    IntergraphDataset *poGDS = ( IntergraphDataset * ) poDS;

    uint32 nTileBytes = 0;

    // --------------------------------------------------------------------
    // Load Scan Line with block or tile raw data
    // --------------------------------------------------------------------

    if( hHeaderOne.DataTypeCode == TiledRasterData )
    {
        // ----------------------------------------------------------------
        // Read block from tile
        // ----------------------------------------------------------------

        nTileBytes = ReadTiledBlock( nBlockXOff, 
                                     nBlockYOff, (GByte*) pabyScanLine );
        if ( nTileBytes < 0 )
        {
            memset( pImage, 0, nRecordBytes );
            CPLError( CE_Failure, CPLE_FileIO, 
                "Can't read (%s) tile with X offset %d and Y offset %d.\n%s", 
                poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror( errno ) );
            return CE_Failure;
        }
    }
    else
    {
        // ----------------------------------------------------------------
        // Read data block
        // ----------------------------------------------------------------

        if(( VSIFSeekL( poGDS->fp, 
                        nDataOffset + ( nRecordBytes * nBlockYOff ), 
                        SEEK_SET ) < 0 ) ||
           ( VSIFReadL( pabyScanLine, 1, nRecordBytes, 
                        poGDS->fp ) < nRecordBytes ))
        {
            memset( pImage, 0, nRecordBytes );
            CPLError( CE_Failure, CPLE_FileIO, 
                "Can't read (%s) block with X offset %d and Y offset %d.\n%s", 
                poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror( errno ) );
            return CE_Failure;
        }
    }

    // --------------------------------------------------------------------
    // Uncompress the raw data
    // --------------------------------------------------------------------

    //TODO: Select and call Decoders...

    // --------------------------------------------------------------------
    // Reshape blocks if needed
    // --------------------------------------------------------------------

    if( hHeaderOne.DataTypeCode == TiledRasterData && 
        nRecordBytes > nTileBytes )
    {
        ReshapeBlock(nBlockXOff, nBlockYOff, pabyScanLine );
    }

    // --------------------------------------------------------------------
    // Extract the band of interest from RGB buffer
    // --------------------------------------------------------------------

    if( nRGBIndex > 0 ) 
    {
        int i, j;
        for (i = 0, j = (3 - nRGBIndex); i < nBlockXSize; i++, j += 3)
        {
            ((GByte *) pImage)[i] = pabyScanLine[j];
        }
    }
    else
    {
        memcpy( pImage, pabyScanLine, nRecordBytes );
    }

#ifdef CPL_MSB    
    if( eDataType == GDT_Float32  )
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4  );
#endif

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                              IntergraphRasterBand::ReadTile()
//  ----------------------------------------------------------------------------

int IntergraphRasterBand::ReadTiledBlock( int nBlockXOff,                                          
                                          int nBlockYOff,                                          
                                          GByte *pabyImage )
{
    IntergraphDataset *poGDS = ( IntergraphDataset * ) poDS;

    int nTileId = nBlockXOff + nBlockYOff * nBlocksPerRow;

    // --------------------------------------------------------------------
    // Check for Un-instantiated
    // --------------------------------------------------------------------

    if( pahTiles[nTileId].Start == 0 ) 
    {
        // ----------------------------------------------------------------
        // All bytes are equal to the value on 'Used'  (pag. 64)
        // ----------------------------------------------------------------

        memset( pabyImage, pahTiles[nTileId].Used, nRecordBytes );

        return nRecordBytes;
    }

    // --------------------------------------------------------------------
    // Read tile buffer
    // --------------------------------------------------------------------

    if( ( VSIFSeekL( poGDS->fp, nDataOffset + pahTiles[nTileId].Start, 
                     SEEK_SET ) < 0 ) ||
        ( VSIFReadL( pabyImage, 1, pahTiles[nTileId].Used, 
                     poGDS->fp ) < pahTiles[nTileId].Used ) )
    {
        return -1;
    }

    return pahTiles[nTileId].Used;
}

/**
 *  Complete Tile with zeroes to fill up a Block
 * 
 *      ##########      ###00000000   ###########    #######0000
 *      ##########      ###00000000   ###########    #######0000
 *      0000000000  =>  ###00000000 , 00000000000 or #######0000
 *      0000000000      ###00000000   00000000000    #######0000
 *      0000000000      ###00000000   00000000000    00000000000
 ***/

//  ----------------------------------------------------------------------------
//                                   IntergraphRasterBand::ReshapeBlock()
//  ----------------------------------------------------------------------------

void IntergraphRasterBand::ReshapeBlock( int nBlockXOff, 
                                         int nBlockYOff,
                                         GByte *pabyBlock )
{
    int nTileId    = nBlockXOff + nBlockYOff * nBlocksPerRow;
    int nTileBytes = pahTiles[nTileId].Used;

    GByte *pabyTile = (GByte*) CPLCalloc( 1, nRecordBytes );

    memcpy( pabyTile, pabyBlock, nRecordBytes );
    memset( pabyBlock, 0, nRecordBytes );

    int nColSize   = nBlockXSize;
    int nRowSize   = nBlockYSize;
    int nCellBytes = GDALGetDataTypeSize( eDataType ) / 8;

    if( nBlockXOff + 1 == nBlocksPerRow )
    {
        nColSize = nRasterXSize % nBlockXSize;
    }
    if( nBlockYOff + 1 == nBlocksPerColumn )
    {
        nRowSize = nRasterYSize % nBlockYSize;
    }
    if( nRGBIndex > 0 )
    {
        nCellBytes = nCellBytes * 3;
    }
    for( int iRow = 0; iRow < nRowSize; iRow++ )
    {
        memcpy( pabyBlock + ( iRow * nCellBytes * nBlockXSize ), 
                pabyTile  + ( iRow * nCellBytes * nColSize ), 
                nCellBytes * nColSize);
    }

    CPLFree( pabyTile );
}

//  ----------------------------------------------------------------------------
//                                           IntergraphRasterBand::IWriteBlock()
//  ----------------------------------------------------------------------------

CPLErr IntergraphRasterBand::IWriteBlock( int nBlockXOff, 
                                          int nBlockYOff,
                                          void *pImage )
{
    IntergraphDataset *poGDS = ( IntergraphDataset * ) poDS;

#ifdef CPL_MSB    
    if( eDataType == GDT_Float32  )
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4  );
#endif

    if( ( nBlockXOff == 0 ) && ( nBlockYOff == 0 ) )
    {
        FlushBandHeader();
    }

    if( nRGBIndex > 0 ) 
    {
        if( nBand > 1 ) 
        {
            VSIFSeekL( poGDS->fp, nDataOffset + ( nRecordBytes * nBlockYOff ), SEEK_SET );
            VSIFReadL( pabyScanLine, 1, nRecordBytes, poGDS->fp );
        }
        int i, j;
        for( i = 0, j = ( 3 - nRGBIndex ); i < nBlockXSize; i++, j += 3 )
        {
            pabyScanLine[j] = ( ( GByte * ) pImage )[i];
        }
    }
    else
    {
        memcpy( pabyScanLine, pImage, nRecordBytes );
    }
#ifdef CPL_MSB    
    if( eDataType == GDT_Float32  )
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4  );
#endif

    VSIFSeekL( poGDS->fp, nDataOffset + ( nRecordBytes * nBlockYOff ), SEEK_SET );

    if( ( int ) VSIFWriteL( pabyScanLine, 1, nRecordBytes, poGDS->fp ) < nRecordBytes )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
            "Can't write (%s) block with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror( errno ) );
        return CE_Failure;
    }

    return CE_None;
}

void IntergraphRasterBand::FlushBandHeader()
{
    if( nRGBIndex > 1 )
    {
        return;
    }

    IntergraphDataset *poGDS = ( IntergraphDataset* ) poDS;

    IngrColorTable hCTab;

    if( poColorTable->GetColorEntryCount() > 0 )
    {
        hHeaderTwo.ColorTableType = IGDSColorTable;
        hHeaderTwo.NumberOfCTEntries = poColorTable->GetColorEntryCount();
        SetIGDSColorTable( poColorTable, &hCTab );
    }

    if( nBand > poDS->GetRasterCount() )
    {
        hHeaderTwo.CatenatedFilePointer = nBand *
            ( ( 3 * SIZEOF_HDR1 ) + ( nRecordBytes * nRasterYSize ) );
    }

    VSIFSeekL( poGDS->fp, nBandStart, SEEK_SET );
    VSIFWriteL( &hHeaderOne, 1, SIZEOF_HDR1,   poGDS->fp );
    VSIFWriteL( &hHeaderTwo, 1, SIZEOF_HDR2_A, poGDS->fp );
    VSIFWriteL( &hCTab,      1, SIZEOF_CTAB,   poGDS->fp );
}