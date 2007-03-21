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
 *****************************************************************************/

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
                                            GDALDataType eType )
{
    this->poColorTable  = new GDALColorTable();

    this->poDS          = poDS;
    this->nBand         = nBand != 0 ? nBand : poDS->nBands;
    this->nTiles        = 0;
    this->eDataType     = eType;
    this->pabyBlockBuf  = NULL;
    this->pahTiles      = NULL;
    this->nRGBIndex     = 0;
    this->nBandStart    = nBandOffset;

    // -------------------------------------------------------------------- 
    // Get Header Info
    // -------------------------------------------------------------------- 

    if( poDS->eAccess == GA_ReadOnly )
    {
        VSIFSeekL( poDS->fp, nBandOffset, SEEK_SET );
        VSIFReadL( &hHeaderOne, 1, SIZEOF_HDR1,   poDS->fp );
        VSIFReadL( &hHeaderTwo, 1, SIZEOF_HDR2_A, poDS->fp );

        this->eDataType = INGR_GetDataType( hHeaderOne.DataTypeCode );
    }
    else
    {
        memcpy( &hHeaderOne, &poDS->hHeaderOne, SIZEOF_HDR1 );
        memcpy( &hHeaderTwo, &poDS->hHeaderTwo, SIZEOF_HDR2_A );
    }


    // -------------------------------------------------------------------- 
    // Get the image start from Words to Follow (WTF)
    // -------------------------------------------------------------------- 

    nDataOffset = nBandOffset + 2 + ( 2 * ( hHeaderOne.WordsToFollow + 1 ) );

    // -------------------------------------------------------------------- 
    // Get Color Tabel from Color Table Type (CTV)
    // -------------------------------------------------------------------- 

    INGR_ColorTableVar hEnvrTable;
    INGR_ColorTable256 hIGDSTable;

    uint32 nEntries = hHeaderTwo.NumberOfCTEntries;

    if( ( nEntries > 0 ) && ( hHeaderTwo.ColorTableType == EnvironVColorTable ) )
    {
        // ----------------------------------------------------------------
        // Get EnvironV color table starting at block 3
        // ----------------------------------------------------------------

        hEnvrTable.Entry = (vlt_slot*) CPLCalloc( nEntries, sizeof( vlt_slot ) );

        VSIFSeekL( poDS->fp, nBandOffset + ( 2 * SIZEOF_HDR1 ), SEEK_SET );
        VSIFReadL( hEnvrTable.Entry, nEntries, sizeof( vlt_slot ), poDS->fp );

        INGR_GetEnvironVColors( poColorTable, &hEnvrTable, nEntries );
        CPLFree( hEnvrTable.Entry );
    }
    else if( ( nEntries > 0 ) && ( hHeaderTwo.ColorTableType == IGDSColorTable ) )
    {
        // ----------------------------------------------------------------
        // Get IGDS (fixed size) starting in the middle of block 2 + 1.5 blocks
        // ----------------------------------------------------------------

        VSIFSeekL( poDS->fp, nBandOffset + ( 1.5 * SIZEOF_HDR1 ), SEEK_SET );
        VSIFReadL( hIGDSTable.Entry, 256, sizeof( igds_slot ), poDS->fp );

        INGR_GetIGDSColors( poColorTable, &hIGDSTable );
    }

    // -------------------------------------------------------------------- 
    // Set Dimension
    // -------------------------------------------------------------------- 

    nRasterXSize  = hHeaderOne.PixelsPerLine;
    nRasterYSize  = hHeaderOne.NumberOfLines;
    
    nBlockXSize   = nRasterXSize;
    nBlockYSize   = 6;

    // -------------------------------------------------------------------- 
    // Get tile directory
    // -------------------------------------------------------------------- 

    eFormat = hHeaderOne.DataTypeCode;

    if( hHeaderOne.DataTypeCode == TiledRasterData )
    {
        // ----------------------------------------------------------------
        // Reads tile header and the first tile info
        // ----------------------------------------------------------------

        VSIFSeekL( poDS->fp, nDataOffset, SEEK_SET );
        VSIFReadL( &hTileDir, 1, SIZEOF_TDIR, poDS->fp );

        eFormat = hTileDir.DataTypeCode;

        // ----------------------------------------------------------------
        // Calculate the number of tiles
        // ----------------------------------------------------------------

        int nTilesPerCol = ceil( (float) nRasterXSize / hTileDir.TileSize );
        int nTilesPerRow = ceil( (float) nRasterYSize / hTileDir.TileSize );

        nTiles = nTilesPerCol * nTilesPerRow;

        // ----------------------------------------------------------------
        // Load the tile table (first tile info is already read)
        // ----------------------------------------------------------------

        pahTiles  = (INGR_TileItem*) CPLCalloc( nTiles, SIZEOF_TILE );
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

        SetMetadataItem( "INGR_TILESSIZE", CPLSPrintf ("%d", hTileDir.TileSize) );
    }
    else
    {
        SetMetadataItem( "INGR_TILED", "NO" ); 
    }

    // -------------------------------------------------------------------- 
    // Get the Data Type from Format
    // -------------------------------------------------------------------- 

    this->eDataType = INGR_GetDataType( eFormat );

    // -------------------------------------------------------------------- 
    // More Metadata Information
    // -------------------------------------------------------------------- 

    SetMetadataItem( "INGR_FORMAT", INGR_GetFormatName( eFormat ) );

    if( hHeaderOne.RotationAngle != 0.0 )
        SetMetadataItem( "INGR_ROTATION", 
            CPLSPrintf ( "%f", hHeaderOne.RotationAngle ) );
     
    SetMetadataItem( "INGR_ORIENTATION", IngrOrientation[hHeaderOne.ScanlineOrientation] );

    if( hHeaderOne.ScannableFlag == HasLineHeader )
        SetMetadataItem( "INGR_SCANFLAG", "YES" );
    else
        SetMetadataItem( "INGR_SCANFLAG", "NO" );

    // -------------------------------------------------------------------- 
    // Number of bytes in a record
    // -------------------------------------------------------------------- 

    nBlockBufSize = nBlockXSize * nBlockYSize * 
                    GDALGetDataTypeSize( eDataType ) / 8;
        
    // -------------------------------------------------------------------- 
    // Allocate buffer for a Block of data
    // -------------------------------------------------------------------- 

    pabyBlockBuf = (GByte*) CPLMalloc( nBlockBufSize );

}

//  ----------------------------------------------------------------------------
//                                  IntergraphBitmapBand::IntergraphBitmapBand()
//  ----------------------------------------------------------------------------

IntergraphBitmapBand::IntergraphBitmapBand( IntergraphDataset *poDS, 
                                            int nBand,
                                            int nBandOffset )
    : IntergraphRasterBand( poDS, nBand, nBandOffset )
{
    //TODO: Initialize directory of lines
}

//  ----------------------------------------------------------------------------
//                                 IntergraphRasterBand::~IntergraphRasterBand()
//  ----------------------------------------------------------------------------

IntergraphRasterBand::~IntergraphRasterBand()
{
    if( pabyBlockBuf )
    {
        CPLFree( pabyBlockBuf );
    }

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

    double dMinimum = INGR_GetMinMax( eDataType, hHeaderOne.Minimum ); 
    double dMaximum = INGR_GetMinMax( eDataType, hHeaderOne.Maximum ); 

    if( pbSuccess )
    {
        *pbSuccess = dMinimum == dMaximum ? FALSE : TRUE;
    }

    return dMinimum;
}

//  ----------------------------------------------------------------------------
//                                            IntergraphRasterBand::GetMaximum()
//  ----------------------------------------------------------------------------

double IntergraphRasterBand::GetMaximum( int *pbSuccess )
{
    double dMinimum = INGR_GetMinMax( eDataType, hHeaderOne.Minimum ); 
    double dMaximum = INGR_GetMinMax( eDataType, hHeaderOne.Maximum ); 

    if( pbSuccess )
    {
        *pbSuccess = dMinimum == dMaximum ? FALSE : TRUE;
    }

    return dMaximum;
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
    else
    {
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

    hHeaderOne.Minimum = INGR_SetMinMax( eDataType, dfMin );
    hHeaderOne.Maximum = INGR_SetMinMax( eDataType, dfMax );
    
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

    // --------------------------------------------------------------------
    // Load Block Buffer
    // --------------------------------------------------------------------

    if( LoadBlockBuf( nBlockXOff, nBlockYOff ) != CE_None )
    {
        memset( pImage, 0, nBlockBufSize );
        CPLError( CE_Failure, CPLE_FileIO, 
            "Can't read (%s) tile with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror( errno ) );
        return CE_Failure;
    }

    // --------------------------------------------------------------------
    // Reshape blocks if needed
    // --------------------------------------------------------------------

    if( hHeaderOne.DataTypeCode == TiledRasterData && 
        nBlockBufSize > nBytesRead )
    {
        ReshapeBlock( nBlockXOff, nBlockYOff, pabyBlockBuf );
    }

    // --------------------------------------------------------------------
    // Copy block buffer to image
    // --------------------------------------------------------------------

    memcpy( pImage, pabyBlockBuf, nBlockXSize * nBlockYSize * 
        GDALGetDataTypeSize( eDataType ) / 8 );

#ifdef CPL_MSB    
    if( eDataType == GDT_Float32  )
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4  );
#endif

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                        IntergraphRGBBand::IntergraphRGBBand()
//  ----------------------------------------------------------------------------

IntergraphRGBBand::IntergraphRGBBand( IntergraphDataset *poDS, 
                                      int nBand,
                                      int nBandOffset,
                                      int nRGorB )
    : IntergraphRasterBand( poDS, nBand, nBandOffset )
{
    nRGBIndex     = nRGorB;
    nBlockBufSize *= 3;
       
    // -------------------------------------------------------------------- 
    // Reallocate buffer for a block of RGB Data
    // -------------------------------------------------------------------- 

    CPLFree( pabyBlockBuf );

    pabyBlockBuf = (GByte*) CPLMalloc( nBlockBufSize );
}

//  ----------------------------------------------------------------------------
//                                   IntergraphRGBBand::GetColorInterpretation()
//  ----------------------------------------------------------------------------

GDALColorInterp IntergraphRGBBand::GetColorInterpretation()
{               
    switch( nRGBIndex )
    {
    case 1: 
        return GCI_RedBand;
    case 2: 
        return GCI_BlueBand;
    case 3: 
        return GCI_GreenBand;
    }

    return GCI_GrayIndex;
}

//  ----------------------------------------------------------------------------
//                                            IntergraphRasterBand::IReadBlock()
//  ----------------------------------------------------------------------------

CPLErr IntergraphRGBBand::IReadBlock( int nBlockXOff, 
                                      int nBlockYOff,
                                      void *pImage )
{
    if( IntergraphRasterBand::IReadBlock( nBlockXOff, 
                                          nBlockYOff, 
                                          pImage ) != CE_None )
    {
        return CE_Failure;
    }

    // --------------------------------------------------------------------
    // Extract the band of interest from the block buffer
    // --------------------------------------------------------------------

    unsigned int i, j;

    for ( i = 0, j = ( 3 - this->nRGBIndex ); 
          i < ( nBlockXSize * nBlockYSize ); 
          i++, j += 3 )
    {
        ( (GByte*) pImage )[i] = pabyBlockBuf[j];
    }

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                            IntergraphBitmapBand::IReadBlock()
//  ----------------------------------------------------------------------------

CPLErr IntergraphBitmapBand::IReadBlock( int nBlockXOff, 
                                         int nBlockYOff,
                                         void *pImage )
{
    IntergraphDataset *poGDS = ( IntergraphDataset * ) poDS;

    //TODO: What is the nBlockBufSize for untiled CCITT?

    // --------------------------------------------------------------------
    // Load Block Buffer
    // --------------------------------------------------------------------
#ifdef UnderContruction
    if( LoadBlockBuf( nBlockXOff, nBlockYOff ) != CE_None )
    {
        memset( pImage, 0, nBlockBufSize );
        CPLError( CE_Failure, CPLE_FileIO, 
            "Can't read (%s) tile with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror( errno ) );
        return CE_Failure;
    }

    memset( pImage, 0, nBlockBufSize );
#endif 
    //TODO: Decoded

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                      IntergraphJPEGBand::IntergraphJPEGBand()
//  ----------------------------------------------------------------------------

IntergraphJPEGBand::IntergraphJPEGBand( IntergraphDataset *poDS, 
                                        int nBand,
                                        int nBandOffset )
    : IntergraphRasterBand( poDS, nBand, nBandOffset )
{
    this->nQLevel = 1;
    this->nQuality = 100;
}

//  ----------------------------------------------------------------------------
//                                            IntergraphBitmapBand::IReadBlock()
//  ----------------------------------------------------------------------------

CPLErr IntergraphJPEGBand::IReadBlock( int nBlockXOff, 
                                       int nBlockYOff,
                                       void *pImage )
{
    IntergraphDataset *poGDS = ( IntergraphDataset * ) poDS;
#ifdef UnderContruction
    uint32 nSeekOffset = 0;

    int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

    if( pahTiles != NULL)
    {
        nSeekOffset = pahTiles[nBlockId].Start + nDataOffset;
    }
    else
    {
        nSeekOffset = nDataOffset + ( nBlockBufSize * nBlockYOff );
    }

    if( nBlockId > 1 )
        return CE_None; /////DEBUG

    const static char *pszFilename = /** "/vsimem/ **/ "abbreviated_table_only.jpg";

    FILE *fpImage = VSIFOpenL( pszFilename, "wb" );

    struct jpeg_compress_struct sCInfo;
    struct jpeg_error_mgr sJErr;
    
    sCInfo.err = jpeg_std_error( &sJErr );
    jpeg_create_compress( &sCInfo );
    
    sCInfo.image_width = hTileDir.TileSize;
    sCInfo.image_height = hTileDir.TileSize;

    switch( this->eFormat )
    {
    case JPEGGRAY:
        sCInfo.input_components = 1;
        sCInfo.in_color_space = JCS_GRAYSCALE;
        break;
    case JPEGRGB: 
        sCInfo.input_components = 3;
        sCInfo.in_color_space = JCS_RGB;
        break;
    case JPEGCYMK:
        sCInfo.input_components = 4;
        sCInfo.in_color_space = JCS_CMYK;
    }

    jpeg_set_defaults( &sCInfo );

    jpeg_set_quality( &sCInfo, nQuality, TRUE );

    jpeg_vsiio_dest( &sCInfo, fpImage );
    
    jpeg_write_tables( &sCInfo );

    jpeg_
    VSIFCloseL( fpImage );

    jpeg_destroy_compress( &sCInfo );

// decompression

    fpImage = VSIFOpenL( pszFilename, "rb" );

    struct jpeg_decompress_struct sDInfo;

    jpeg_create_decompress( &sDInfo );

    sDInfo.err = jpeg_std_error( &sJErr );

    jpeg_vsiio_src( &sDInfo, fpImage );

    jpeg_read_header( &sDInfo, FALSE );

    VSIFSeekL( poGDS->fp, nSeekOffset, SEEK_SET );

    jpeg_vsiio_src( &sDInfo, poGDS->fp );

    jpeg_read_header( &sDInfo, TRUE );

    jpeg_start_decompress( &sDInfo );

    jpeg_finish_decompress( &sDInfo );

    jpeg_destroy_decompress( &sDInfo );

    VSIFCloseL( fpImage );
#endif
    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                          IntergraphRasterBand::LoadBlockBuf()
//  ----------------------------------------------------------------------------

CPLErr IntergraphRasterBand::LoadBlockBuf( int nBlockXOff, int nBlockYOff )
{
    IntergraphDataset *poGDS = ( IntergraphDataset * ) poDS;

    uint32 nSeekOffset = 0;
    uint32 nReadSize = 0;

    // --------------------------------------------------------------------
    // Read from tiles or read from strip
    // --------------------------------------------------------------------

    if( hHeaderOne.DataTypeCode == TiledRasterData )
    {
        int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;
        
        if( pahTiles[nBlockId].Start == 0 ) 
        {
            // ------------------------------------------------------------
            // Uninstantieted tile, unique value
            // ------------------------------------------------------------

            memset( pabyBlockBuf, pahTiles[nBlockId].Used, nBlockBufSize );
            return CE_None;
        }

        nSeekOffset = pahTiles[nBlockId].Start + nDataOffset;
        nReadSize = pahTiles[nBlockId].Used;
    }
    else
    {
        nSeekOffset = nDataOffset + ( nBlockBufSize * nBlockYOff );
        nReadSize = nBlockBufSize;
    }

    if( VSIFSeekL( poGDS->fp, nSeekOffset, SEEK_SET ) < 0 )
    {
        return CE_Failure;
    }

    nBytesRead = VSIFReadL( pabyBlockBuf, 1, nReadSize, poGDS->fp );

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                   IntergraphRasterBand::ReshapeBlock()
//  ----------------------------------------------------------------------------

/**
 *  Complete Tile with zeroes to fill up a Block
 * 
 *         ###    ##000   ######    ###00
 *         ### => ##000 , 000000 or ###00
 *                ##000   000000    00000
 ***/

void IntergraphRasterBand::ReshapeBlock( int nBlockXOff, 
                                         int nBlockYOff,
                                         GByte *pabyBlock )
{
    int nTileId    = nBlockXOff + nBlockYOff * nBlocksPerRow;
    int nTileBytes = pahTiles[nTileId].Used;

    GByte *pabyTile = (GByte*) CPLCalloc( 1, nBlockBufSize );

    memcpy( pabyTile, pabyBlock, nBlockBufSize );
    memset( pabyBlock, 0, nBlockBufSize );

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
            VSIFSeekL( poGDS->fp, nDataOffset + ( nBlockBufSize * nBlockYOff ), SEEK_SET );
            VSIFReadL( pabyBlockBuf, 1, nBlockBufSize, poGDS->fp );
        }
        int i, j;
        for( i = 0, j = ( 3 - nRGBIndex ); i < nBlockXSize; i++, j += 3 )
        {
            pabyBlockBuf[j] = ( ( GByte * ) pImage )[i];
        }
    }
    else
    {
        memcpy( pabyBlockBuf, pImage, nBlockBufSize );
    }
#ifdef CPL_MSB    
    if( eDataType == GDT_Float32  )
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4  );
#endif

    VSIFSeekL( poGDS->fp, nDataOffset + ( nBlockBufSize * nBlockYOff ), SEEK_SET );

    if( ( int ) VSIFWriteL( pabyBlockBuf, 1, nBlockBufSize, poGDS->fp ) < nBlockBufSize )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
            "Can't write (%s) block with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror( errno ) );
        return CE_Failure;
    }

    return CE_None;
}

void IntergraphRasterBand::FlushBandHeader( void )
{
    if( nRGBIndex > 1 )
    {
        return;
    }

    IntergraphDataset *poGDS = ( IntergraphDataset* ) poDS;

    INGR_ColorTable256 hCTab;

    if( poColorTable->GetColorEntryCount() > 0 )
    {
        hHeaderTwo.ColorTableType = IGDSColorTable;
        hHeaderTwo.NumberOfCTEntries = poColorTable->GetColorEntryCount();
        INGR_SetIGDSColors( poColorTable, &hCTab );
    }

    if( nBand > poDS->GetRasterCount() )
    {
        hHeaderTwo.CatenatedFilePointer = nBand *
            ( ( 3 * SIZEOF_HDR1 ) + ( nBlockBufSize * nRasterYSize ) );
    }

    VSIFSeekL( poGDS->fp, nBandStart, SEEK_SET );
    VSIFWriteL( &hHeaderOne, 1, SIZEOF_HDR1,   poGDS->fp );
    VSIFWriteL( &hHeaderTwo, 1, SIZEOF_HDR2_A, poGDS->fp );
    VSIFWriteL( &hCTab,      1, SIZEOF_CTAB,   poGDS->fp );
}