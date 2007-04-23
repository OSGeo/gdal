/*****************************************************************************
 * $Id$
 *
 * Project:  Intergraph Raster Format support
 * Purpose:  Read/Write Intergraph Raster Format, band support
 * Author:   Ivan Lucena, ivan.lucena@pmldnet.com
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
    this->nBytesRead    = 0;
    this->bTiled        = FALSE;
    this->bVirtualTile  = FALSE;

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

    if( nEntries > 0 )
    {
        switch ( hHeaderTwo.ColorTableType )
        {
        case EnvironVColorTable:
            INGR_GetEnvironVColors( poDS->fp, nBandOffset, nEntries, poColorTable );
            break;
        case IGDSColorTable:
            INGR_GetIGDSColors( poDS->fp, nBandOffset, nEntries, poColorTable );
            break;
        default:
            CPLDebug( "INGR", "Wrong Color table type (%d), number of colors (%d)", 
                hHeaderTwo.ColorTableType, nEntries );
        }
    }

    // -------------------------------------------------------------------- 
    // Set Dimension
    // -------------------------------------------------------------------- 

    nRasterXSize  = hHeaderOne.PixelsPerLine;
    nRasterYSize  = hHeaderOne.NumberOfLines;
    
    nBlockXSize   = nRasterXSize;
    nBlockYSize   = 1;

    // -------------------------------------------------------------------- 
    // Get tile directory
    // -------------------------------------------------------------------- 

    this->eFormat = (INGR_Format) hHeaderOne.DataTypeCode;

    this->bTiled = (hHeaderOne.DataTypeCode == TiledRasterData);

    if( bTiled )
    {
        uint32 nTiles = INGR_GetTileDirectory( poDS->fp, 
                                               nDataOffset, 
                                               nRasterXSize, 
                                               nRasterYSize,
                                               &hTileDir, 
                                               &pahTiles );

        eFormat = (INGR_Format) hTileDir.DataTypeCode;

        // ----------------------------------------------------------------
        // Set blocks dimensions based on tiles
        // ----------------------------------------------------------------

        nBlockXSize = MIN( hTileDir.TileSize, (uint32) nRasterXSize );
        nBlockYSize = MIN( hTileDir.TileSize, (uint32) nRasterYSize );
    }

    // -------------------------------------------------------------------- 
    // Get the Data Type from Format
    // -------------------------------------------------------------------- 

    this->eDataType = INGR_GetDataType( eFormat );

    // -------------------------------------------------------------------- 
    // More Metadata Information
    // -------------------------------------------------------------------- 

    SetMetadataItem( "INGR_FORMAT", INGR_GetFormatName( eFormat ) );

    if( bTiled )
    {
        SetMetadataItem( "INGR_TILESSIZE", CPLSPrintf ("%d", hTileDir.TileSize) );
    }
    else
    {
        SetMetadataItem( "INGR_TILED", "NO" ); 
    }

    if( hHeaderOne.RotationAngle != 0.0 )
        SetMetadataItem( "INGR_ROTATION", 
            CPLSPrintf ( "%f", hHeaderOne.RotationAngle ) );
     
    SetMetadataItem( "INGR_ORIENTATION", INGR_GetOrientation( hHeaderOne.ScanlineOrientation ) );

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

    this->poColorTable = poColorTable;

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

    if( bTiled && nBlockBufSize > nBytesRead )
    {
        ReshapeBlock( nBlockXOff, nBlockYOff, nBlockBufSize, pabyBlockBuf );
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

    int i, j;

    for ( i = 0, j = ( 3 - this->nRGBIndex ); 
          i < ( nBlockXSize * nBlockYSize ); 
          i++, j += 3 )
    {
        ( (GByte*) pImage )[i] = pabyBlockBuf[j];
    }

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                  IntergraphBitmapBand::IntergraphBitmapBand()
//  ----------------------------------------------------------------------------

IntergraphBitmapBand::IntergraphBitmapBand( IntergraphDataset *poDS, 
                                           int nBand,
                                           int nBandOffset )
    : IntergraphRasterBand( poDS, nBand, nBandOffset, GDT_Byte )
{
    if( ! this->bTiled )
    {
        // ------------------------------------------------------------
        // Load all rows at once
        // ------------------------------------------------------------

        nBlockYSize         = nRasterYSize;
        this->bVirtualTile  = TRUE;
    }

    // ----------------------------------------------------------------
    // Black and White Color Table
    // ----------------------------------------------------------------

	if( eFormat == CCITTGroup4 )
	{
		GDALColorEntry oEntry;

		oEntry.c1 = (short) 255;
		oEntry.c2 = (short) 255;
		oEntry.c3 = (short) 255;
		oEntry.c4 = (short) 255;

		poColorTable->SetColorEntry( 0, &oEntry );

		oEntry.c1 = (short) 0;
		oEntry.c2 = (short) 0;
		oEntry.c3 = (short) 0;
		oEntry.c4 = (short) 255;

		poColorTable->SetColorEntry( 1, &oEntry );
	}

    // ----------------------------------------------------------------
    // Read JPEG Quality from Application Data
    // ----------------------------------------------------------------

	if( eFormat == JPEGGRAY ||
		eFormat == JPEGRGB  ||
		eFormat == JPEGCYMK )
	{
        nQuality = INGR_ReadJpegQuality( poDS->fp, 
            hHeaderTwo.ApplicationPacketPointer,
            nDataOffset );
	}
}

//  ----------------------------------------------------------------------------
//                                            IntergraphBitmapBand::IReadBlock()
//  ----------------------------------------------------------------------------

CPLErr IntergraphBitmapBand::IReadBlock( int nBlockXOff, 
                                         int nBlockYOff,
                                         void *pImage )
{
    IntergraphDataset *poGDS = ( IntergraphDataset * ) poDS;

    // ----------------------------------------------------------------
	// Load the block of a tile or a whole image
    // ----------------------------------------------------------------

    if( LoadBlockBuf( nBlockXOff, nBlockYOff ) != CE_None )
    {
        memset( pImage, 0, nBlockBufSize );
        CPLError( CE_Failure, CPLE_FileIO, 
            "Can't read (%s) tile with X offset %d and Y offset %d.\n%s", 
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror( errno ) );
        return CE_Failure;
    }

    // ----------------------------------------------------------------
	// Calcule the resulting image dimmention
    // ----------------------------------------------------------------

    int nTiffXSize = nBlockXSize;
    int nTiffYSize = nBlockYSize; 

    if( bTiled && 
	  ( nBlockXOff + 1 == nBlocksPerRow ) &&	// left block 
	  ( nRasterXSize > nTiffYSize ) )			// smaller than a tile
    {
        nTiffXSize = nRasterXSize % nBlockXSize;
    }

    if( bTiled && 
	  ( nBlockYOff + 1 == nBlocksPerColumn ) &&	// bottom block 
	  ( nRasterYSize > nTiffYSize ) )			// smaller than a tile
    {
        nTiffYSize = nRasterYSize % nBlockYSize;
    }

    // ----------------------------------------------------------------
	// Create a in memory a smal (~400K) tiff file
    // ----------------------------------------------------------------

    poGDS->hTiffMem = INGR_CreateTiff( poGDS->pszFilename, eFormat,
                                       nTiffXSize, nTiffYSize, nQuality,
                                       pabyBlockBuf, nBytesRead );

    if( poGDS->hTiffMem.poDS == NULL )
    {
        memset( pImage, 0, nBlockBufSize );
        CPLError( CE_Failure, CPLE_AppDefined, 
			"Unable to open TIFF virtual file.\n"
			"Is the GTIFF driver available?" );
        return CE_Failure;
    }

    // ----------------------------------------------------------------
	// Read the unique block of the in memory tiff and release it
    // ----------------------------------------------------------------

    poGDS->hTiffMem.poBand->ReadBlock( 0, 0, pImage );

	INGR_ReleaseTiff( &poGDS->hTiffMem );

	// --------------------------------------------------------------------
    // Reshape blocks if needed
    // --------------------------------------------------------------------

    if( nTiffXSize != nBlockXSize && nTiffXSize != nBlockXSize )
    {
        ReshapeBlock( nBlockXOff, nBlockYOff, nBlockBufSize, (GByte*) pImage );
    }

    return CE_None;
} 


//  ----------------------------------------------------------------------------
//                                          IntergraphRasterBand::LoadBlockBuf()
//  ----------------------------------------------------------------------------

CPLErr IntergraphRasterBand::LoadBlockBuf( int nBlockXOff, int nBlockYOff )
{
    IntergraphDataset *poGDS = ( IntergraphDataset * ) poDS;

    uint32 nSeekOffset   = 0;
    uint32 nReadSize     = 0;

    // --------------------------------------------------------------------
    // Read from tiles or read from strip
    // --------------------------------------------------------------------

    if( bTiled )
    {
        int nBlockId  = nBlockXOff + nBlockYOff * nBlocksPerRow;

        if( pahTiles[nBlockId].Start == 0 ) 
        {
            // ------------------------------------------------------------
            // Uninstantieted tile, unique value
            // ------------------------------------------------------------

            memset( pabyBlockBuf, pahTiles[nBlockId].Used, nBlockBufSize );
            return CE_None;
        }

        nSeekOffset   = pahTiles[nBlockId].Start + nDataOffset;
        nReadSize     = pahTiles[nBlockId].Used;
    }
    else if( bVirtualTile )
    {
        if( hHeaderTwo.CatenatedFilePointer == 0 )
        {
            VSIStatBuf  sStat;
            CPLStat( poGDS->pszFilename, &sStat );
            nReadSize = sStat.st_size - nDataOffset;
        }
        else
        {
            nReadSize = hHeaderTwo.CatenatedFilePointer - nDataOffset;
        }
        if( nBlockBufSize < nReadSize )
        {
            CPLFree( pabyBlockBuf );
            pabyBlockBuf  = (GByte*) CPLMalloc( nReadSize );
        }
        nSeekOffset   = nDataOffset;
    }
    else
    {
        nSeekOffset   = nDataOffset + ( nBlockBufSize * nBlockYOff );
        nReadSize     = nBlockBufSize;
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
                                         int nBlockBytes,
                                         GByte *pabyBlock )
{
    GByte *pabyTile = (GByte*) CPLCalloc( 1, nBlockBufSize );

    memcpy( pabyTile, pabyBlock, nBlockBytes );
    memset( pabyBlock, 0, nBlockBytes );

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

    if( ( uint32 ) VSIFWriteL( pabyBlockBuf, 1, nBlockBufSize, poGDS->fp ) < nBlockBufSize )
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
