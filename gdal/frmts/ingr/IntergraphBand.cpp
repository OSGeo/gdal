/*****************************************************************************
 *
 * Project:  Intergraph Raster Format support
 * Purpose:  Read/Write Intergraph Raster Format, band support
 * Author:   Ivan Lucena, [lucena_ivan at hotmail.com]
 *
 ******************************************************************************
 * Copyright (c) 2007, Ivan Lucena
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include <cmath>

#include "IntergraphDataset.h"
#include "IntergraphBand.h"
#include "IngrTypes.h"

#include <algorithm>

CPL_CVSID("$Id$")

using std::fill;

//  ----------------------------------------------------------------------------
//                                  IntergraphRasterBand::IntergraphRasterBand()
//  ----------------------------------------------------------------------------

INGR_TileHeader::INGR_TileHeader() :
    ApplicationType(0),
    SubTypeCode(0),
    WordsToFollow(0),
    PacketVersion(0),
    Identifier(0),
    Properties(0),
    DataTypeCode(0),
    TileSize(0),
    Reserved3(0)
{
    fill( Reserved, Reserved + CPL_ARRAYSIZE(Reserved), static_cast<uint16_t>(0) );
    fill( Reserved2, Reserved2 + CPL_ARRAYSIZE(Reserved2), static_cast<uint8_t>(0) );
    First.Start = 0;
    First.Allocated = 0;
    First.Used = 0;
}

IntergraphRasterBand::IntergraphRasterBand( IntergraphDataset *poDSIn,
                                            int nBandIn,
                                            int nBandOffset,
                                            GDALDataType eType ) :
    poColorTable(new GDALColorTable()),
    nDataOffset(0),
    nBlockBufSize(0),
    nBandStart(nBandOffset),
    nRGBIndex(0),
    eFormat(IngrUnknownFrmt),
    bTiled(false),
    nFullBlocksX(0),
    nFullBlocksY(0),
    pabyBlockBuf(nullptr),
    nTiles(0),
    pahTiles(nullptr),
    nRLEOffset(0)
{
    poDS = poDSIn;
    nBand = nBandIn != 0 ? nBandIn : poDSIn->nBands;
    eDataType = eType;

    // --------------------------------------------------------------------
    // Get Header Info
    // --------------------------------------------------------------------
    memcpy(&hHeaderOne, &poDSIn->hHeaderOne, sizeof(hHeaderOne));
    memcpy(&hHeaderTwo, &poDSIn->hHeaderTwo, sizeof(hHeaderTwo));

    // --------------------------------------------------------------------
    // Get the image start from Words to Follow (WTF)
    // --------------------------------------------------------------------
    if( nBandOffset > INT_MAX - (2 + ( 2 * ( hHeaderOne.WordsToFollow + 1 ) )) )
    {
        pabyBlockBuf = nullptr;
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid header values");
        return;
    }

    nDataOffset = nBandOffset + 2 + ( 2 * ( hHeaderOne.WordsToFollow + 1 ) );

    // --------------------------------------------------------------------
    // Get Color Table from Color Table Type (CTV)
    // --------------------------------------------------------------------
    uint32_t nEntries = hHeaderTwo.NumberOfCTEntries;

    if( nEntries > 0 )
    {
        // Not sure what max to use, but 65536 seems far enough
        if( nEntries > 65536 )
            nEntries = 65536;
        switch ( hHeaderTwo.ColorTableType )
        {
        case EnvironVColorTable:
            INGR_GetEnvironVColors( poDSIn->fp, nBandOffset, nEntries, poColorTable );
            if (poColorTable->GetColorEntryCount() == 0)
                return;
            break;
        case IGDSColorTable:
            INGR_GetIGDSColors( poDSIn->fp, nBandOffset, nEntries, poColorTable );
            if (poColorTable->GetColorEntryCount() == 0)
                return;
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

    uint16_t eFormatUntyped = (INGR_Format) hHeaderOne.DataTypeCode;

    bTiled = hHeaderOne.DataTypeCode == TiledRasterData;

    if( bTiled )
    {
        nTiles = INGR_GetTileDirectory( poDSIn->fp,
                                        nDataOffset,
                                        nRasterXSize,
                                        nRasterYSize,
                                        &hTileDir,
                                        &pahTiles );
        if (nTiles == 0)
            return;

        eFormatUntyped = hTileDir.DataTypeCode;

        // ----------------------------------------------------------------
        // Set blocks dimensions based on tiles
        // ----------------------------------------------------------------
        nBlockXSize = hTileDir.TileSize;
        nBlockYSize = hTileDir.TileSize;
    }

    if (nBlockXSize <= 0 || nBlockYSize <= 0)
    {
        pabyBlockBuf = nullptr;
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid block dimensions");
        return;
    }

    // --------------------------------------------------------------------
    // Incomplete tiles have Block Offset greater than:
    // --------------------------------------------------------------------
    nFullBlocksX = nRasterXSize / nBlockXSize;
    nFullBlocksY = nRasterYSize / nBlockYSize;

    // --------------------------------------------------------------------
    // Get the Data Type from Format
    // --------------------------------------------------------------------
    eDataType = INGR_GetDataType( eFormatUntyped );

    // --------------------------------------------------------------------
    // Allocate buffer for a Block of data
    // --------------------------------------------------------------------
    if( nBlockYSize == 0 ||
        nBlockXSize > INT_MAX / nBlockYSize ||
        nBlockXSize > INT_MAX / 4 - 2 ||
        GDALGetDataTypeSize( eDataType ) == 0 ||
        nBlockYSize > INT_MAX / (GDALGetDataTypeSize( eDataType ) / 8) ||
        nBlockXSize > INT_MAX / (nBlockYSize *
                                 (GDALGetDataTypeSizeBytes( eDataType ))) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too big block size / invalid type");
        return;
    }
    eFormat = static_cast<INGR_Format>(eFormatUntyped);

    nBlockBufSize = nBlockXSize * nBlockYSize *
                    (GDALGetDataTypeSize( eDataType ) / 8);

    if (eFormat == RunLengthEncoded)
    {
        pabyBlockBuf = (GByte*) VSIMalloc3(
            nBlockXSize*4+2, nBlockYSize,
            GDALGetDataTypeSizeBytes( eDataType ) );
    }
    else
    {
        pabyBlockBuf = (GByte*) VSIMalloc3(
            nBlockXSize, nBlockYSize,
            GDALGetDataTypeSizeBytes( eDataType ) );
    }

    if (pabyBlockBuf == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot allocate %d bytes", nBlockBufSize);
        return;
    }

    // --------------------------------------------------------------------
    // More Metadata Information
    // --------------------------------------------------------------------
    SetMetadataItem( "FORMAT", INGR_GetFormatName( (uint16_t) eFormat ),
        "IMAGE_STRUCTURE" );

    if( bTiled )
    {
        SetMetadataItem( "TILESSIZE", CPLSPrintf ("%d", hTileDir.TileSize),
            "IMAGE_STRUCTURE" );
    }
    else
    {
        SetMetadataItem( "TILED", "NO", "IMAGE_STRUCTURE" );
    }

    SetMetadataItem( "ORIENTATION",
        INGR_GetOrientation( hHeaderOne.ScanlineOrientation ),
        "IMAGE_STRUCTURE" );

    if( eFormat == PackedBinary ||
        eFormat == RunLengthEncoded ||
        eFormat == CCITTGroup4 )
    {
        SetMetadataItem( "NBITS", "1", "IMAGE_STRUCTURE" );
    }

    nRLEOffset = 0;
}

//  ----------------------------------------------------------------------------
//                                 IntergraphRasterBand::~IntergraphRasterBand()
//  ----------------------------------------------------------------------------

IntergraphRasterBand::~IntergraphRasterBand()
{
    CPLFree( pabyBlockBuf );
    CPLFree( pahTiles );

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

    const double dMinimum = INGR_GetMinMax( eDataType, hHeaderOne.Minimum );
    const double dMaximum = INGR_GetMinMax( eDataType, hHeaderOne.Maximum );

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
    const double dMinimum = INGR_GetMinMax( eDataType, hHeaderOne.Minimum );
    const double dMaximum = INGR_GetMinMax( eDataType, hHeaderOne.Maximum );

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
    if( eFormat == AdaptiveRGB ||
        eFormat == Uncompressed24bit ||
        eFormat == ContinuousTone )
    {
        switch( nRGBIndex )
        {
        case 1:
            return GCI_RedBand;
        case 2:
            return GCI_GreenBand;
        case 3:
            return GCI_BlueBand;
        }
        return GCI_GrayIndex;
    }
    else
    {
        if( poColorTable->GetColorEntryCount() > 0 )
        {
            return GCI_PaletteIndex;
        }

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
        return nullptr;
    }

    return poColorTable;
}

//  ----------------------------------------------------------------------------
//                                         IntergraphRasterBand::SetColorTable()
//  ----------------------------------------------------------------------------

CPLErr IntergraphRasterBand::SetColorTable( GDALColorTable *poColorTableIn )
{
    if( poColorTableIn == nullptr )
    {
        return CE_None;
    }

    delete this->poColorTable;
    this->poColorTable = poColorTableIn->Clone();

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
    // --------------------------------------------------------------------
    // Load Block Buffer
    // --------------------------------------------------------------------
    if (HandleUninstantiatedTile( nBlockXOff, nBlockYOff, pImage ))
        return CE_None;

    uint32_t nBytesRead = LoadBlockBuf( nBlockXOff, nBlockYOff, nBlockBufSize, pabyBlockBuf );

    if( nBytesRead == 0 )
    {
        memset( pImage, 0, nBlockXSize * nBlockYSize *
                    (GDALGetDataTypeSize( eDataType ) / 8) );
        CPLError( CE_Failure, CPLE_FileIO,
            "Can't read (%s) tile with X offset %d and Y offset %d.\n",
            ((IntergraphDataset*)poDS)->pszFilename, nBlockXOff, nBlockYOff );
        return CE_Failure;
    }

    // --------------------------------------------------------------------
    // Reshape blocks if needed
    // --------------------------------------------------------------------

    if( nBlockXOff == nFullBlocksX ||
        nBlockYOff == nFullBlocksY )
    {
        if( !ReshapeBlock( nBlockXOff, nBlockYOff, nBlockBufSize, pabyBlockBuf ) )
            return CE_Failure;
    }

    // --------------------------------------------------------------------
    // Copy block buffer to image
    // --------------------------------------------------------------------

    memcpy( pImage, pabyBlockBuf, nBlockXSize * nBlockYSize *
        (GDALGetDataTypeSize( eDataType ) / 8) );

#ifdef CPL_MSB
    if( eDataType == GDT_Int16 || eDataType == GDT_UInt16)
        GDALSwapWords( pImage, 2, nBlockXSize * nBlockYSize, 2  );
    else if( eDataType == GDT_Int32 || eDataType == GDT_UInt32 || eDataType == GDT_Float32  )
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4  );
    else if (eDataType == GDT_Float64  )
        GDALSwapWords( pImage, 8, nBlockXSize * nBlockYSize, 8  );
#endif

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                        IntergraphRasterBand::HandleUninstantiatedTile()
//  ----------------------------------------------------------------------------

int IntergraphRasterBand::HandleUninstantiatedTile(int nBlockXOff,
                                                   int nBlockYOff,
                                                   void* pImage)
{
    if( bTiled && pahTiles[nBlockXOff + nBlockYOff * nBlocksPerRow].Start == 0 )
    {
        // ------------------------------------------------------------
        // Uninstantieted tile, unique value
        // ------------------------------------------------------------
        int nColor = pahTiles[nBlockXOff + nBlockYOff * nBlocksPerRow].Used;
        switch( GetColorInterpretation() )
        {
            case GCI_RedBand:
                nColor >>= 16; break;
            case GCI_GreenBand:
                nColor >>= 8; break;
            default:
                break;
        }
        memset( pImage, nColor, nBlockXSize * nBlockYSize *
                    (GDALGetDataTypeSize( eDataType ) / 8) );
        return TRUE;
    }
    return FALSE;
}

//  ----------------------------------------------------------------------------
//                                        IntergraphRGBBand::IntergraphRGBBand()
//  ----------------------------------------------------------------------------

IntergraphRGBBand::IntergraphRGBBand( IntergraphDataset *poDSIn,
                                      int nBandIn,
                                      int nBandOffset,
                                      int nRGorB )
    : IntergraphRasterBand( poDSIn, nBandIn, nBandOffset )
{
    if( pabyBlockBuf == nullptr )
        return;

    nRGBIndex = static_cast<uint8_t>(nRGorB);

    // --------------------------------------------------------------------
    // Reallocate buffer for a block of RGB Data
    // --------------------------------------------------------------------

    nBlockBufSize *= 3;
    CPLFree( pabyBlockBuf );
    pabyBlockBuf = static_cast<GByte *>(VSIMalloc( nBlockBufSize ));
    if( pabyBlockBuf == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot allocate %d bytes", nBlockBufSize);
    }
}

//  ----------------------------------------------------------------------------
//                                               IntergraphRGBBand::IReadBlock()
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

    for ( int i = 0, j = ( nRGBIndex - 1 );
          i < ( nBlockXSize * nBlockYSize );
          i++, j += 3 )
    {
        ( (GByte*) pImage )[i] = pabyBlockBuf[j];
    }

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                        IntergraphRLEBand::IntergraphRLEBand()
//  ----------------------------------------------------------------------------

IntergraphRLEBand::IntergraphRLEBand( IntergraphDataset *poDSIn,
                                      int nBandIn,
                                      int nBandOffset,
                                      int nRGorB ) :
    IntergraphRasterBand( poDSIn, nBandIn, nBandOffset ),
    pabyRLEBlock(nullptr),
    nRLESize(0),
    bRLEBlockLoaded(FALSE),
    panRLELineOffset(nullptr)
{
    nRGBIndex = static_cast<uint8_t>(nRGorB);

    if( pabyBlockBuf == nullptr )
        return;

    if( !bTiled )
    {
        // ------------------------------------------------------------
        // Load all rows at once
        // ------------------------------------------------------------

        nFullBlocksX = 1;

        if( eFormat == RunLengthEncodedC || eFormat == RunLengthEncoded )
        {
            nBlockYSize = 1;
            if( nRasterYSize > 1024 * 1024 )
            {
                VSIFSeekL( poDSIn->fp, 0, SEEK_END );
                // At the very least 2 bytes per row (probably more)
                if( VSIFTellL( poDSIn->fp ) / 2 <
                                    static_cast<vsi_l_offset>(nRasterYSize) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "File too short");
                    return;
                }
            }
            panRLELineOffset = (uint32_t *)
                VSI_CALLOC_VERBOSE(sizeof(uint32_t),nRasterYSize);
            if( panRLELineOffset == nullptr )
                return;
            nFullBlocksY = nRasterYSize;
        }
        else
        {
            nBlockYSize  = nRasterYSize;
            nFullBlocksY = 1;
        }

        nRLESize = INGR_GetDataBlockSize( poDSIn->pszFilename,
                                          hHeaderTwo.CatenatedFilePointer,
                                          nDataOffset);

        if( nBlockYSize == 0 || nBlockXSize > INT_MAX / nBlockYSize )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too big block size");
            return;
        }
        nBlockBufSize = nBlockXSize * nBlockYSize;
    }
    else
    {
        // ------------------------------------------------------------
        // Find the biggest tile
        // ------------------------------------------------------------

        for( uint32_t iTiles = 0; iTiles < nTiles; iTiles++)
        {
            nRLESize = MAX( pahTiles[iTiles].Used, nRLESize );
        }
    }

    // ----------------------------------------------------------------
    // Reallocate the decompressed buffer.
    // ----------------------------------------------------------------

    if( eFormat == AdaptiveRGB ||
        eFormat == ContinuousTone )
    {
        if( nBlockBufSize > INT_MAX / 3 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too big block size");
            return;
        }
        nBlockBufSize *= 3;
    }

    CPLFree(pabyBlockBuf);
    pabyBlockBuf = nullptr;
    if( nBlockBufSize > 0 )
        pabyBlockBuf = (GByte*) VSIMalloc( nBlockBufSize );
    if (pabyBlockBuf == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot allocate %d bytes", nBlockBufSize);
    }

    // ----------------------------------------------------------------
    // Create a RLE buffer
    // ----------------------------------------------------------------

    if( nRLESize == 0 )
        pabyRLEBlock = (GByte*) VSIMalloc( 1 );
    else if( nRLESize < INT_MAX )
    {
        if( nRLESize > 100 * 1024 * 1024 )
        {
            IntergraphDataset *poGDS = ( IntergraphDataset * ) poDS;
            VSIFSeekL( poGDS->fp, 0, SEEK_END );
            if( VSIFTellL( poGDS->fp ) < nRLESize )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "File too short");
                pabyRLEBlock = nullptr;
                return;
            }
        }
        pabyRLEBlock = (GByte*) VSIMalloc( nRLESize );
    }
    if (pabyRLEBlock == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot allocate %d bytes", nRLESize);
    }

    // ----------------------------------------------------------------
    // Set a black and white Color Table
    // ----------------------------------------------------------------

    if( eFormat == RunLengthEncoded )
    {
        BlackWhiteCT( true );
    }
}

//  ----------------------------------------------------------------------------
//                                        IntergraphRLEBand::IntergraphRLEBand()
//  ----------------------------------------------------------------------------

IntergraphRLEBand::~IntergraphRLEBand()
{
    CPLFree( pabyRLEBlock );
    CPLFree( panRLELineOffset );
}

//  ----------------------------------------------------------------------------
//                                               IntergraphRLEBand::IReadBlock()
//  ----------------------------------------------------------------------------

CPLErr IntergraphRLEBand::IReadBlock( int nBlockXOff,
                                      int nBlockYOff,
                                      void *pImage )
{
    // --------------------------------------------------------------------
    // Load Block Buffer
    // --------------------------------------------------------------------

    uint32_t nBytesRead;

    if( bTiled || !bRLEBlockLoaded )
    {
        if (HandleUninstantiatedTile( nBlockXOff, nBlockYOff, pImage ))
            return CE_None;

        if (!bTiled)
        {
            // With RLE, we want to load all of the data.
            // So load (0,0) since that's the only offset that will load everything.
            nBytesRead = LoadBlockBuf( 0, 0, nRLESize, pabyRLEBlock );
        }
        else
        {
            nBytesRead = LoadBlockBuf( nBlockXOff, nBlockYOff, nRLESize, pabyRLEBlock );
        }
        bRLEBlockLoaded = TRUE;
    }
    else
        nBytesRead = nRLESize;

    if( nBytesRead == 0 )
    {
        memset( pImage, 0, nBlockXSize * nBlockYSize *
                    (GDALGetDataTypeSize( eDataType ) / 8) );
        CPLError( CE_Failure, CPLE_FileIO,
            "Can't read (%s) tile with X offset %d and Y offset %d.\n%s",
            ((IntergraphDataset*)poDS)->pszFilename, nBlockXOff, nBlockYOff,
            VSIStrerror( errno ) );
        return CE_Failure;
    }

    // ----------------------------------------------------------------
    // Calculate the resulting image dimmention
    // ----------------------------------------------------------------

    int nVirtualXSize = nBlockXSize;
    int nVirtualYSize = nBlockYSize;

    if( nBlockXOff == nFullBlocksX )
    {
        nVirtualXSize = nRasterXSize % nBlockXSize;
    }

    if( nBlockYOff == nFullBlocksY )
    {
        nVirtualYSize = nRasterYSize % nBlockYSize;
    }

    int nExpectedOutputBytes = nVirtualXSize * nVirtualYSize;
    if( eFormat == AdaptiveRGB ||
        eFormat == ContinuousTone )
    {
        nExpectedOutputBytes *= 3;
    }

    // --------------------------------------------------------------------
    // Decode Run Length
    // --------------------------------------------------------------------

    int nOutputBytes;
    if( bTiled && eFormat == RunLengthEncoded )
    {
        nOutputBytes =
            INGR_DecodeRunLengthBitonalTiled( pabyRLEBlock, pabyBlockBuf,
                                              nRLESize, nBlockBufSize, nullptr );
    }
    else if( bTiled || panRLELineOffset == nullptr )
    {
        nOutputBytes = INGR_Decode( eFormat, pabyRLEBlock, pabyBlockBuf,
                                  nRLESize, nBlockBufSize,
                                  nullptr );
    }
    else
    {
        uint32_t nBytesConsumed;

        // If we are missing the offset to this line, process all
        // preceding lines that are not initialized.
        if( nBlockYOff > 0 && panRLELineOffset[nBlockYOff] == 0 )
        {
            int iLine = nBlockYOff - 1;
            // Find the last line that is initialized (or line 0).
            while ((iLine != 0) && (panRLELineOffset[iLine] == 0))
                iLine--;
            for( ; iLine < nBlockYOff; iLine++ )
            {
                // Pass NULL as destination so that no decompression
                // actually takes place.
                if( nRLESize < panRLELineOffset[iLine] ||
                    (uint32_t)INGR_Decode( eFormat,
                             pabyRLEBlock + panRLELineOffset[iLine],
                             nullptr,  nRLESize - panRLELineOffset[iLine], nBlockBufSize,
                             &nBytesConsumed ) < nBlockBufSize )
                {
                    memset( pImage, 0, nBlockXSize * nBlockYSize *
                                (GDALGetDataTypeSize( eDataType ) / 8) );
                    CPLError( CE_Failure, CPLE_AppDefined,
                        "Can't decode line %d", iLine );
                    return CE_Failure;
                }

                if( iLine < nRasterYSize-1 )
                    panRLELineOffset[iLine+1] =
                        panRLELineOffset[iLine] + nBytesConsumed;
            }
        }

        if( nRLESize < panRLELineOffset[nBlockYOff] )
            nOutputBytes = 0;
        else
        {
            // Read the requested line.
            nOutputBytes = INGR_Decode( eFormat,
                         pabyRLEBlock + panRLELineOffset[nBlockYOff],
                         pabyBlockBuf,  nRLESize - panRLELineOffset[nBlockYOff], nBlockBufSize,
                         &nBytesConsumed );
            if( nOutputBytes == nExpectedOutputBytes && nBlockYOff < nRasterYSize-1 )
                panRLELineOffset[nBlockYOff+1] =
                    panRLELineOffset[nBlockYOff] + nBytesConsumed;
        }
    }

    if( nOutputBytes < nExpectedOutputBytes )
    {
        memset( pImage, 0, nBlockXSize * nBlockYSize *
                    (GDALGetDataTypeSize( eDataType ) / 8) );
        CPLError( CE_Failure, CPLE_AppDefined,
            "Can't decode block (%d, %d)", nBlockXOff, nBlockYOff );
        return CE_Failure;
    }

    // --------------------------------------------------------------------
    // Reshape blocks if needed
    // --------------------------------------------------------------------

    if( nBlockXOff == nFullBlocksX ||
        nBlockYOff == nFullBlocksY )
    {
        if( !ReshapeBlock( nBlockXOff, nBlockYOff, nBlockBufSize, pabyBlockBuf ) )
            return CE_Failure;
    }

    // --------------------------------------------------------------------
    // Extract the band of interest from the block buffer (BIL)
    // --------------------------------------------------------------------

    if( eFormat == AdaptiveRGB ||
        eFormat == ContinuousTone )
    {
        GByte *pabyImage = (GByte*) pImage;
        int j = ( nRGBIndex - 1 ) * nVirtualXSize;
        for ( int i = 0; i < nVirtualYSize; i++ )
        {
            memcpy( &pabyImage[i * nBlockXSize], &pabyBlockBuf[j], nBlockXSize );
            j += ( 3 * nBlockXSize );
        }
    }
    else
    {
        memcpy( pImage, pabyBlockBuf, nBlockBufSize );
    }

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                  IntergraphBitmapBand::IntergraphBitmapBand()
//  ----------------------------------------------------------------------------

IntergraphBitmapBand::IntergraphBitmapBand( IntergraphDataset *poDSIn,
                                            int nBandIn,
                                            int nBandOffset,
                                            int nRGorB ) :
    IntergraphRasterBand( poDSIn, nBandIn, nBandOffset, GDT_Byte ),
    pabyBMPBlock(nullptr),
    nBMPSize(0),
    nQuality(0),
    nRGBBand(nRGorB)
{
    if( pabyBlockBuf == nullptr )
        return;

    if( !bTiled )
    {
        // ------------------------------------------------------------
        // Load all rows at once
        // ------------------------------------------------------------

        nBlockYSize = nRasterYSize;
        nBMPSize = INGR_GetDataBlockSize( poDSIn->pszFilename,
                                          hHeaderTwo.CatenatedFilePointer,
                                          nDataOffset );
    }
    else
    {
        // ------------------------------------------------------------
        // Find the biggest tile
        // ------------------------------------------------------------

        for( uint32_t iTiles = 0; iTiles < nTiles; iTiles++)
        {
            nBMPSize = MAX( pahTiles[iTiles].Used, nBMPSize );
        }
    }

    // ----------------------------------------------------------------
    // Create a Bitmap buffer
    // ----------------------------------------------------------------

    if( nBMPSize > INT_MAX )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too large block size: %u bytes", nBMPSize);
        return;
    }
    if( nBMPSize > 10 * 1024 * 1024 )
    {
        VSIFSeekL( poDSIn->fp, 0, SEEK_END );
        if( VSIFTellL( poDSIn->fp ) < nBMPSize )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "File too short");
            return;
        }
    }
    pabyBMPBlock = (GByte*) VSIMalloc( nBMPSize );
    if (pabyBMPBlock == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot allocate %u bytes", nBMPSize);
    }

    // ----------------------------------------------------------------
    // Set a black and white Color Table
    // ----------------------------------------------------------------

    if( eFormat == CCITTGroup4 )
    {
        BlackWhiteCT( true );
    }

    // ----------------------------------------------------------------
    // Read JPEG Quality from Application Data
    // ----------------------------------------------------------------

    if( eFormat == JPEGGRAY ||
        eFormat == JPEGRGB  ||
        eFormat == JPEGCMYK )
    {
        nQuality = INGR_ReadJpegQuality( poDSIn->fp,
                                         hHeaderTwo.ApplicationPacketPointer,
                                         nDataOffset );
    }
}

//  ----------------------------------------------------------------------------
//                                 IntergraphBitmapBand::~IntergraphBitmapBand()
//  ----------------------------------------------------------------------------

IntergraphBitmapBand::~IntergraphBitmapBand()
{
    CPLFree( pabyBMPBlock );
}

//  ----------------------------------------------------------------------------
//                                IntergraphBitmapBand::GetColorInterpretation()
//  ----------------------------------------------------------------------------

GDALColorInterp IntergraphBitmapBand::GetColorInterpretation()
{
    if( eFormat == JPEGRGB)
    {
        switch( nRGBBand )
        {
        case 1:
            return GCI_RedBand;
        case 2:
            return GCI_GreenBand;
        case 3:
            return GCI_BlueBand;
        }
        return GCI_GrayIndex;
    }

    if( poColorTable->GetColorEntryCount() > 0 )
    {
        return GCI_PaletteIndex;
    }

    return GCI_GrayIndex;
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
    if (HandleUninstantiatedTile( nBlockXOff, nBlockYOff, pImage ))
        return CE_None;

    uint32_t nBytesRead = LoadBlockBuf( nBlockXOff, nBlockYOff, nBMPSize, pabyBMPBlock );

    if( nBytesRead == 0 )
    {
        memset( pImage, 0, nBlockXSize * nBlockYSize *
                    (GDALGetDataTypeSize( eDataType ) / 8) );
        CPLError( CE_Failure, CPLE_FileIO,
            "Can't read (%s) tile with X offset %d and Y offset %d.\n%s",
            ((IntergraphDataset*)poDS)->pszFilename, nBlockXOff, nBlockYOff,
            VSIStrerror( errno ) );
        return CE_Failure;
    }

    // ----------------------------------------------------------------
    // Calculate the resulting image dimmention
    // ----------------------------------------------------------------

    int nVirtualXSize = nBlockXSize;
    int nVirtualYSize = nBlockYSize;

    if( nBlockXOff == nFullBlocksX )
    {
        nVirtualXSize = nRasterXSize % nBlockXSize;
    }

    if( nBlockYOff == nFullBlocksY )
    {
        nVirtualYSize = nRasterYSize % nBlockYSize;
    }

    // ----------------------------------------------------------------
    // Create an in memory small tiff file (~400K)
    // ----------------------------------------------------------------

    poGDS->hVirtual = INGR_CreateVirtualFile( poGDS->pszFilename,
                                              eFormat,
                                              nVirtualXSize,
                                              nVirtualYSize,
                                              hTileDir.TileSize,
                                              nQuality,
                                              pabyBMPBlock,
                                              nBytesRead,
                                              nRGBBand );

    if( poGDS->hVirtual.poBand == nullptr )
    {
        memset( pImage, 0, nBlockXSize * nBlockYSize *
                    (GDALGetDataTypeSize( eDataType ) / 8) );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to open virtual file.\n"
                  "Is the GTIFF and JPEG driver available?" );
        return CE_Failure;
    }

    // ----------------------------------------------------------------
    // Read the unique block from the in memory file and release it
    // ----------------------------------------------------------------

    if( poGDS->hVirtual.poBand->RasterIO( GF_Read, 0, 0,
        nVirtualXSize, nVirtualYSize, pImage,
        nVirtualXSize, nVirtualYSize, GDT_Byte, 0, 0, nullptr) != CE_None )
    {
        INGR_ReleaseVirtual( &poGDS->hVirtual );
        return CE_Failure;
    }

    // --------------------------------------------------------------------
    // Reshape blocks if needed
    // --------------------------------------------------------------------

    CPLErr eErr = CE_None;
    if( nBlockXOff == nFullBlocksX ||
        nBlockYOff == nFullBlocksY )
    {
        if( !ReshapeBlock( nBlockXOff, nBlockYOff, nBlockBufSize, (GByte*) pImage ) )
            eErr = CE_Failure;
    }

    INGR_ReleaseVirtual( &poGDS->hVirtual );

    return eErr;
}

//  ----------------------------------------------------------------------------
//                                          IntergraphRasterBand::LoadBlockBuf()
//  ----------------------------------------------------------------------------

int IntergraphRasterBand::LoadBlockBuf( int nBlockXOff,
                                        int nBlockYOff,
                                        int nBlobkBytes,
                                        GByte *pabyBlock ) const
{
    vsi_l_offset nSeekOffset  = 0;
    uint32_t nReadSize    = 0;

    // --------------------------------------------------------------------
    // Read from tiles or read from strip
    // --------------------------------------------------------------------

    if( bTiled )
    {
        uint32_t nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

        if( pahTiles[nBlockId].Start == 0 )
        {
            return 0;
        }

        nSeekOffset   = static_cast<vsi_l_offset>(pahTiles[nBlockId].Start) + nDataOffset;
        nReadSize     = pahTiles[nBlockId].Used;

        CPLAssert( nBlobkBytes >= 0 );
        if( nReadSize > (uint32_t)nBlobkBytes )
        {
            CPLDebug( "INGR",
                      "LoadBlockBuf(%d,%d) - trimmed tile size from %u to %d.",
                      nBlockXOff, nBlockYOff,
                      nReadSize, nBlobkBytes );
            nReadSize = nBlobkBytes;
        }
    }
    else
    {
        nSeekOffset   = nDataOffset + ( static_cast<vsi_l_offset>(nBlockBufSize) * nBlockYOff );
        nReadSize     = nBlobkBytes;
    }

    IntergraphDataset *poGDS = ( IntergraphDataset * ) poDS;

    if( VSIFSeekL( poGDS->fp, nSeekOffset, SEEK_SET ) < 0 )
    {
        return 0;
    }

    uint32_t nRead = static_cast<uint32_t>(VSIFReadL( pabyBlock, 1, nReadSize, poGDS->fp ));
    if( nRead < nReadSize )
        memset( pabyBlock + nRead, 0, nReadSize - nRead );
    return static_cast<int>(nRead);
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

bool IntergraphRasterBand::ReshapeBlock( int nBlockXOff,
                                         int nBlockYOff,
                                         int nBlockBytes,
                                         GByte *pabyBlock )
{
    GByte *pabyTile = (GByte*) VSI_MALLOC_VERBOSE( nBlockBufSize );
    if( pabyTile == nullptr )
        return false;

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

    VSIFree( pabyTile );
    return true;
}

//  ----------------------------------------------------------------------------
//                                           IntergraphRasterBand::IWriteBlock()
//  ----------------------------------------------------------------------------

CPLErr IntergraphRasterBand::IWriteBlock( int nBlockXOff,
                                          int nBlockYOff,
                                          void *pImage )
{
    uint32_t nBlockSize = nBlockBufSize;
    uint32_t nBlockOffset = nBlockBufSize * nBlockYOff;

    IntergraphDataset *poGDS = ( IntergraphDataset * ) poDS;

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
        for( int i = 0, j = ( 3 - nRGBIndex ); i < nBlockXSize; i++, j += 3 )
        {
            pabyBlockBuf[j] = ( ( GByte * ) pImage )[i];
        }
    }
    else if (eFormat == RunLengthEncoded)
    {
        // Series of [OFF, ON,] OFF spans.
        int nLastCount = 0;
        GByte *pInput = ( GByte * ) pImage;
        GInt16 *pOutput = ( GInt16 * ) pabyBlockBuf;

        nBlockOffset = this->nRLEOffset * 2;
        int nRLECount = 0;
        GByte nValue = 0; // Start with OFF spans.

        for(uint32_t i=0; i<nBlockBufSize; i++)
        {
            if (((nValue == 0) && (pInput[i] == 0)) || ((nValue == 1) && pInput[i]))
            {
                nLastCount++;
            }
            else
            {
                // Change of span type.
                while(nLastCount>32767)
                {
                    pOutput[nRLECount++] = CPL_LSBWORD16(32767);
                    pOutput[nRLECount++] = CPL_LSBWORD16(0);
                    nLastCount -= 32767;
                }
                pOutput[nRLECount++] = static_cast<GInt16>(CPL_LSBWORD16(nLastCount));
                nLastCount = 1;
                nValue ^= 1;
            }
        }

        // Output tail end of scanline
        while(nLastCount>32767)
        {
            pOutput[nRLECount++] = CPL_LSBWORD16(32767);
            pOutput[nRLECount++] = CPL_LSBWORD16(0);
            nLastCount -= 32767;
        }

        if (nLastCount != 0)
        {
            pOutput[nRLECount++] = static_cast<GInt16>(CPL_LSBWORD16(nLastCount));
            /*nLastCount = 0;*/
            nValue ^= 1;
        }

        if (nValue == 0)
            pOutput[nRLECount++] = CPL_LSBWORD16(0);

        this->nRLEOffset += nRLECount;
        nBlockSize = nRLECount * 2;
    }
    else
    {
        memcpy( pabyBlockBuf, pImage, nBlockBufSize );
#ifdef CPL_MSB
        if( eDataType == GDT_Int16 || eDataType == GDT_UInt16)
            GDALSwapWords( pabyBlockBuf, 2, nBlockXSize * nBlockYSize, 2  );
        else if( eDataType == GDT_Int32 || eDataType == GDT_UInt32 || eDataType == GDT_Float32  )
            GDALSwapWords( pabyBlockBuf, 4, nBlockXSize * nBlockYSize, 4  );
        else if (eDataType == GDT_Float64  )
            GDALSwapWords( pabyBlockBuf, 8, nBlockXSize * nBlockYSize, 8  );
#endif
    }

    VSIFSeekL( poGDS->fp, nDataOffset + nBlockOffset, SEEK_SET );

    if( ( uint32_t ) VSIFWriteL( pabyBlockBuf, 1, nBlockSize, poGDS->fp ) < nBlockSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
            "Can't write (%s) block with X offset %d and Y offset %d.\n%s",
            poGDS->pszFilename, nBlockXOff, nBlockYOff, VSIStrerror( errno ) );
        return CE_Failure;
    }

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                       IntergraphRasterBand::FlushBandHeader()
//  ----------------------------------------------------------------------------

void IntergraphRasterBand::FlushBandHeader( void )
{
    if( nRGBIndex > 1 )
    {
        return;
    }

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

    IntergraphDataset *poGDS = ( IntergraphDataset* ) poDS;
    VSIFSeekL( poGDS->fp, nBandStart, SEEK_SET );

    GByte abyBuf[MAX(SIZEOF_HDR1,SIZEOF_CTAB)];

    INGR_HeaderOneMemToDisk( &hHeaderOne, abyBuf );

    VSIFWriteL( abyBuf, 1, SIZEOF_HDR1, poGDS->fp );

    INGR_HeaderTwoAMemToDisk( &hHeaderTwo, abyBuf );

    VSIFWriteL( abyBuf, 1, SIZEOF_HDR2_A, poGDS->fp );

    unsigned int n = 0;

    for( unsigned int i = 0; i < 256; i++ )
    {
        STRC2BUF( abyBuf, n, hCTab.Entry[i].v_red );
        STRC2BUF( abyBuf, n, hCTab.Entry[i].v_green );
        STRC2BUF( abyBuf, n, hCTab.Entry[i].v_blue );
    }

    VSIFWriteL( abyBuf, 1, SIZEOF_CTAB, poGDS->fp );
}

//  ----------------------------------------------------------------------------
//                                          IntergraphRasterBand::BlackWhiteCT()
//  ----------------------------------------------------------------------------

void IntergraphRasterBand::BlackWhiteCT( bool bReverse )
{
    GDALColorEntry oBlack;
    GDALColorEntry oWhite;

    oWhite.c1 = (short) 255;
    oWhite.c2 = (short) 255;
    oWhite.c3 = (short) 255;
    oWhite.c4 = (short) 255;

    oBlack.c1 = (short) 0;
    oBlack.c2 = (short) 0;
    oBlack.c3 = (short) 0;
    oBlack.c4 = (short) 255;

    if( bReverse )
    {
        poColorTable->SetColorEntry( 0, &oWhite );
        poColorTable->SetColorEntry( 1, &oBlack );
    }
    else
    {
        poColorTable->SetColorEntry( 0, &oBlack );
        poColorTable->SetColorEntry( 1, &oWhite );
    }
}
