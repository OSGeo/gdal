/*****************************************************************************
 * $Id: $
 *
 * Project:  Intergraph Raster Format support
 * Purpose:  Read/Write Intergraph Raster Format, dataset support
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
//                                        IntergraphDataset::IntergraphDataset()
//  ----------------------------------------------------------------------------

IntergraphDataset::IntergraphDataset()
{
    pszFilename = NULL;
    fp = NULL;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

}

//  ----------------------------------------------------------------------------
//                                       IntergraphDataset::~IntergraphDataset()
//  ----------------------------------------------------------------------------

IntergraphDataset::~IntergraphDataset()
{
    FlushCache();

    CPLFree( pszFilename );

    if( fp != NULL )
    {
        VSIFCloseL( fp );
    }
}

//  ----------------------------------------------------------------------------
//                                                     IntergraphDataset::Open()
//  ----------------------------------------------------------------------------

GDALDataset *IntergraphDataset::Open( GDALOpenInfo *poOpenInfo )
{
    if( poOpenInfo->fp == NULL || poOpenInfo->nHeaderBytes < 1024 )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Assign Header Information
    // -------------------------------------------------------------------- 

    IngrHeaderOne  *pHeaderOne = ( IngrHeaderOne* ) poOpenInfo->pabyHeader;
    IngrHeaderTwoA *pHeaderTwo = ( IngrHeaderTwoA* ) pHeaderOne + 1;

    // -------------------------------------------------------------------- 
    // Check Header Type (HTC) Version
    // -------------------------------------------------------------------- 

    if( pHeaderOne->HeaderType.Version != INGR_HEADER_VERSION )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Check Header Type (HTC) 2D / 3D Flag
    // -------------------------------------------------------------------- 

    if( ( pHeaderOne->HeaderType.Is2Dor3D != INGR_HEADER_2D ) && 
        ( pHeaderOne->HeaderType.Is2Dor3D != INGR_HEADER_3D ) )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Check Header Type (HTC) Type Flag
    // -------------------------------------------------------------------- 

    if( pHeaderOne->HeaderType.Type != INGR_HEADER_TYPE )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Check Words To Follow (WTC) Minimum Value
    // -------------------------------------------------------------------- 

    if( pHeaderOne->WordsToFollow < 254 )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Check Words To Follow (WTC) Integrity
    // -------------------------------------------------------------------- 

    float fHeaderBlocks = ( pHeaderOne->WordsToFollow + 2 ) / 256;

    if( ( fHeaderBlocks - (int) fHeaderBlocks ) != 0.0 )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Check Grid File Version (VER)
    // -------------------------------------------------------------------- 

    if( pHeaderOne->GridFileVersion != 1 &&
        pHeaderOne->GridFileVersion != 2 &&
        pHeaderOne->GridFileVersion != 3 )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Get Data Type Code (DTC) => Format Type
    // -------------------------------------------------------------------- 

    IngrFormatType eFormatType = (IngrFormatType) pHeaderOne->DataTypeCode;

    if( pHeaderOne->DataTypeCode == TiledRasterData )
    {
        IngrTileDir hTileDir;

        int nOffset = 2 + ( 2 * ( pHeaderOne->WordsToFollow + 1 ) );

        // ----------------------------------------------------------------
        // Get Format Type from the tile directory
        // ----------------------------------------------------------------

        if( (VSIFSeek( poOpenInfo->fp, nOffset, SEEK_SET ) != 0 )  ||
            (VSIFRead( &hTileDir, 1, sizeof( IngrTileDir ), poOpenInfo->fp ) ) );
        {
            if( hTileDir.ApplicationType     == 1 &&
                hTileDir.SubTypeCode         == 7 &&
              ( hTileDir.WordsToFollow % 4 ) == 0 &&
                hTileDir.PacketVersion       == 1 &&
                hTileDir.Identifier          == 1 )
            {
                eFormatType = (IngrFormatType) hTileDir.DataTypeCode;
            }
        }
    }

    // -------------------------------------------------------------------- 
    // Check supported Data Type Code (DTC) => Format Type
    // -------------------------------------------------------------------- 

    if( eFormatType != ByteInteger &&
        eFormatType != WordIntegers &&
        eFormatType != Integers32Bit &&
        eFormatType != FloatingPoint32Bit &&
        eFormatType != FloatingPoint64Bit &&
        eFormatType != RunLengthEncoded &&
        eFormatType != RunLengthEncodedC &&
        eFormatType != CCITTGroup4 &&
        eFormatType != AdaptiveRGB &&
        eFormatType != Uncompressed24bit &&
        eFormatType != AdaptiveGrayScale &&
        eFormatType != ContinuousTone &&
        eFormatType != JPEGGRAY &&
        eFormatType != JPEGRGB && 
        eFormatType != JPEGCYMK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
            "Intergraph Raster Format %d ( \"%s\" ) not supported",
            pHeaderOne->DataTypeCode, GetFormatNameFromCode( pHeaderOne->DataTypeCode ) );
        return NULL;
    }

    // -----------------------------------------------------------------
    // Create a corresponding GDALDataset
    // -----------------------------------------------------------------

    IntergraphDataset *poDS;

    poDS = new IntergraphDataset();
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->pszFilename = CPLStrdup( poOpenInfo->pszFilename );

    if( poOpenInfo->eAccess == GA_ReadOnly  )
    {
        poDS->fp = VSIFOpenL( poDS->pszFilename, "rb" );
    } 
    else 
    {
        poDS->fp = VSIFOpenL( poDS->pszFilename, "r+b" );
    }

    if( poDS->fp == NULL )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Get X Size from Pixels Per Line (PPL)
    // -------------------------------------------------------------------- 

    poDS->nRasterXSize = pHeaderOne->PixelsPerLine;

    // -------------------------------------------------------------------- 
    // Get Y Size from Number of Lines (NOL)
    // -------------------------------------------------------------------- 

    poDS->nRasterYSize = pHeaderOne->NumberOfLines;

    // -------------------------------------------------------------------- 
    // Get Geo Transformation from Homogeneous Transformation Matrix (TRN)
    // -------------------------------------------------------------------- 

    GetTransforMatrix( pHeaderOne->TransformationMatrix, 
        poDS->adfGeoTransform );

    // -------------------------------------------------------------------- 
    // Get Data type for Format
    // -------------------------------------------------------------------- 

    GDALDataType eType = GetDataTypeForFormat( eFormatType );

    // -------------------------------------------------------------------- 
    // Create Band Information
    // -------------------------------------------------------------------- 

    int nBands = 0;
    int nBandOffset = 0;

    do
    {
        VSIFSeekL( poDS->fp, nBandOffset, SEEK_SET );
        VSIFReadL( &poDS->hHeaderOne, 1, SIZEOF_HDR1,   poDS->fp );
        VSIFReadL( &poDS->hHeaderTwo, 1, SIZEOF_HDR2_A, poDS->fp );

        if( eFormatType == Uncompressed24bit )
        {
            nBands++;
            poDS->SetBand( nBands, 
                new IntergraphRasterBand( poDS, nBands, nBandOffset, 3, GDT_Byte ) );

            nBands++;
            poDS->SetBand( nBands, 
                new IntergraphRasterBand( poDS, nBands, nBandOffset, 2, GDT_Byte ) );

            nBands++;
            poDS->SetBand( nBands, 
                new IntergraphRasterBand( poDS, nBands, nBandOffset, 1, GDT_Byte ) );
        }
        else
        {
            eType = GetDataTypeForFormat( poDS->hHeaderOne.DataTypeCode );

            nBands++;
            poDS->SetBand( nBands, 
                new IntergraphRasterBand( poDS, nBands, nBandOffset, 0, eType ) );
        }

        // ----------------------------------------------------------------
        // Get next band offset from Catenated File Pointer (CFP)
        // ----------------------------------------------------------------

        nBandOffset = poDS->hHeaderTwo.CatenatedFilePointer;
    }
    while( nBandOffset != 0 );

    poDS->nBands = nBands;

    // -------------------------------------------------------------------- 
    // Initialize any PAM information                                 
    // -------------------------------------------------------------------- 

    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return ( poDS );
}

//  ----------------------------------------------------------------------------
//                                                   IntergraphDataset::Create()
//  ----------------------------------------------------------------------------

GDALDataset *IntergraphDataset::Create( const char *pszFilename,
                                        int nXSize, 
                                        int nYSize, 
                                        int nBands, 
                                        GDALDataType eType,
                                        char **papszOptions )
{
    if( eType != GDT_Byte &&
        eType != GDT_Int16 && 
        eType != GDT_Int32 && 
        eType != GDT_UInt16 && 
        eType != GDT_UInt32 && 
        eType != GDT_Float32&& 
        eType != GDT_Float64 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Data type not supported (%s)",
            GDALGetDataTypeName( eType ) );
        return NULL;
    }

    // -------------------------------------------------------------------- 
    //  Get Format option
    // -------------------------------------------------------------------- 

    const char *pszCompression = CSLFetchNameValue( papszOptions, "FORMAT" );

    if( pszCompression == NULL )
    {
        pszCompression = CPLStrdup( "None" );
    }

    if( EQUAL( pszCompression, GetFormatNameFromCode( PackedBinary ) )      == FALSE &&
        EQUAL( pszCompression, GetFormatNameFromCode( RunLengthEncoded ) )  == FALSE &&
        EQUAL( pszCompression, GetFormatNameFromCode( CCITTGroup4 ) )       == FALSE &&
        EQUAL( pszCompression, GetFormatNameFromCode( AdaptiveRGB ) )       == FALSE &&
        EQUAL( pszCompression, GetFormatNameFromCode( AdaptiveGrayScale ) ) == FALSE &&
        EQUAL( pszCompression, GetFormatNameFromCode( ContinuousTone ) )    == FALSE &&
        EQUAL( pszCompression, GetFormatNameFromCode( JPEGGRAY ) )          == FALSE &&
        EQUAL( pszCompression, GetFormatNameFromCode( JPEGRGB ) )           == FALSE &&
        EQUAL( pszCompression, GetFormatNameFromCode( JPEGCYMK ) )          == FALSE &&
        EQUAL( pszCompression, "None" ) == FALSE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
            "Compression option (%s) not supported", pszCompression );
        return NULL;
    }

    // -------------------------------------------------------------------- 
    //  Fill headers with minimun information
    // -------------------------------------------------------------------- 

    IngrHeaderOne  hHdr1;
    IngrHeaderTwoA hHdr2;
    IngrColorTable hCTab;

    hHdr1.HeaderType.Version    = INGR_HEADER_VERSION;
    hHdr1.HeaderType.Type       = INGR_HEADER_TYPE;
    hHdr1.HeaderType.Is2Dor3D   = INGR_HEADER_2D;
    hHdr1.DataTypeCode          = GetFormatCodeFromName( eType, pszCompression );
    hHdr1.WordsToFollow         = ( ( SIZEOF_HDR1 * 3 ) / 2 ) - 2;
    hHdr1.ApplicationType       = GenericRasterImageFile;
    hHdr1.XViewOrigin           = 0.0;
    hHdr1.YViewOrigin           = 0.0;
    hHdr1.ZViewOrigin           = 0.0;
    hHdr1.XViewExtent           = 0.0;
    hHdr1.YViewExtent           = 0.0;
    hHdr1.ZViewExtent           = 0.0;
    for( int i = 0; i < 15; i++ )
    {
        hHdr1.TransformationMatrix[i]   = 0.0;
    }
    hHdr1.TransformationMatrix[15]      = 1.0;
    hHdr1.PixelsPerLine         = nXSize;
    hHdr1.NumberOfLines         = nYSize;
    hHdr1.DeviceResolution      = 1.0;
    hHdr1.ScanlineOrientation   = UpperLeftHorizontal;
    hHdr1.ScannableFlag         = NoLineHeader;
    hHdr1.RotationAngle         = 0.0;
    hHdr1.SkewAngle             = 0.0;
    hHdr1.DataTypeModifier      = 0;
    hHdr1.DesignFileName[0]     = '\0';
    hHdr1.DataBaseFileName[0]   = '\0';
    hHdr1.ParentGridFileName[0] = '\0';
    hHdr1.FileDescription[0]    = '\0';
    hHdr1.Minimum               = SetMinMax( eType, 0.0 );
    hHdr1.Maximum               = SetMinMax( eType, 0.0 );
    hHdr1.GridFileVersion       = 3;
    hHdr1.Reserved[0]           = 0;
    hHdr1.Reserved[1]           = 0;
    hHdr1.Reserved[2]           = 0;
    hHdr2.Gain                  = 0;
    hHdr2.OffsetThreshold       = 0;
    hHdr2.View1                 = 0;
    hHdr2.View2                 = 0;
    hHdr2.ViewNumber            = 0;
    hHdr2.Reserved2             = 0;
    hHdr2.Reserved3             = 0;
    hHdr2.AspectRatio           = nXSize / nYSize;
    hHdr2.CatenatedFilePointer  = 0;
    hHdr2.ColorTableType        = NoColorTable;
    hHdr2.NumberOfCTEntries     = 0;
    hHdr2.Reserved8             = 0;
    for( int i = 0; i < 110; i++ )
    {
        hHdr2.Reserved[i]       = 0;
    }
    hHdr2.ApplicationPacketLength   = 0;
    hHdr2.ApplicationPacketPointer  = 0;
    for( int i = 0; i < 256; i++ )
    {
        hCTab.Entry[i].v_red   = ( uint8 ) 0;
        hCTab.Entry[i].v_green = ( uint8 ) 0;
        hCTab.Entry[i].v_blue  = ( uint8 ) 0;
    }

    // -------------------------------------------------------------------- 
    //  RGB Composite assumption
    // -------------------------------------------------------------------- 

    if(  EQUAL( pszCompression, "None" ) && 
         eType == GDT_Byte  &&
         nBands == 3 )
    {
        hHdr1.DataTypeCode = Uncompressed24bit;
    }

    // -------------------------------------------------------------------- 
    //  Create output file with minimum header info
    // -------------------------------------------------------------------- 

    FILE *fp = VSIFOpenL( pszFilename, "wb+" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
            "Attempt to create file %s' failed.\n", pszFilename );
        return NULL;
    }

    VSIFWriteL( &hHdr1, 1, SIZEOF_HDR1, fp );
    VSIFWriteL( &hHdr2, 1, SIZEOF_HDR2_A, fp );
    VSIFWriteL( &hCTab, 1, SIZEOF_CTAB, fp );
    VSIFCloseL( fp );

    // -------------------------------------------------------------------- 
    //  Returns a new IntergraphDataset from the created file
    // -------------------------------------------------------------------- 

    return ( IntergraphDataset * ) GDALOpen( pszFilename, GA_Update );
}

//  ----------------------------------------------------------------------------
//                                               IntergraphDataset::CreateCopy()
//  ----------------------------------------------------------------------------

GDALDataset *IntergraphDataset::CreateCopy( const char *pszFilename, 
                                            GDALDataset *poSrcDS,
                                            int bStrict,
                                            char **papszOptions,
                                            GDALProgressFunc pfnProgress, 
                                            void *pProgressData )
{
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Query GDAL Data Type 
    // -------------------------------------------------------------------- 

    GDALDataType eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();

    // -------------------------------------------------------------------- 
    // Create IntergraphDataset
    // -------------------------------------------------------------------- 

    IntergraphDataset *poDstDS;

    poDstDS = (IntergraphDataset*) IntergraphDataset::Create( pszFilename, 
        poSrcDS->GetRasterXSize(), 
        poSrcDS->GetRasterYSize(), 
        poSrcDS->GetRasterCount(), 
        eType, 
        papszOptions );

    if( poDstDS == NULL )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    //  Get compression option
    // -------------------------------------------------------------------- 

    const char *pszCompression = CSLFetchNameValue( papszOptions, "FORMAT" );

    if( pszCompression == NULL )
    {
        pszCompression = CPLStrdup( "None" );
    }

    // -------------------------------------------------------------------- 
    // Copy Transformation Matrix to the dataset
    // -------------------------------------------------------------------- 

    double adfGeoTransform[6];

    poDstDS->SetProjection( poSrcDS->GetProjectionRef() );
    poSrcDS->GetGeoTransform( adfGeoTransform );
    poDstDS->SetGeoTransform( adfGeoTransform );

    // -------------------------------------------------------------------- 
    // Copy information to the raster band
    // -------------------------------------------------------------------- 

    GDALRasterBand *poSrcBand;
    GDALRasterBand *poDstBand;
    double dfMin;
    double dfMax;
    double dfMean;
    double dfStdDev = -1;

    if( poDstDS->hHeaderOne.DataTypeCode == Uncompressed24bit )
    {
        poDstDS->SetBand( 1, new IntergraphRasterBand( poDstDS, 1, 0, 3, GDT_Byte ) );
        poDstDS->SetBand( 2, new IntergraphRasterBand( poDstDS, 2, 0, 2, GDT_Byte ) );
        poDstDS->SetBand( 3, new IntergraphRasterBand( poDstDS, 3, 0, 1, GDT_Byte ) );
        poDstDS->nBands = 3;
    }
    else
    {
        for( int i = 1; i <= poSrcDS->GetRasterCount(); i++ )
        {
            poSrcBand = poSrcDS->GetRasterBand(i);
            eType = poSrcDS->GetRasterBand(i)->GetRasterDataType();

            poDstBand = new IntergraphRasterBand( poDstDS, i, 0, 0, eType );
            poDstDS->SetBand( i, poDstBand );

            poDstBand->SetCategoryNames( poSrcBand->GetCategoryNames() );
            poDstBand->SetColorTable( poSrcBand->GetColorTable() );
            poSrcBand->GetStatistics( false, true, &dfMin, &dfMax, &dfMean, &dfStdDev );
            poDstBand->SetStatistics( dfMin, dfMax, dfMean, dfStdDev );
        }
    }

    // -------------------------------------------------------------------- 
    // Copy image data
    // -------------------------------------------------------------------- 

    int nXSize = poDstDS->GetRasterXSize();
    int nYSize = poDstDS->GetRasterYSize();

    int nBlockXSize;
    int nBlockYSize;

    CPLErr eErr = CE_None;

    for( int iBand = 1; iBand <= poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poDstBand = poDstDS->GetRasterBand( iBand );
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand );

        // ------------------------------------------------------------
        // Copy Untiled / Uncompressed
        // ------------------------------------------------------------

        int    iYOffset, iXOffset;
        void *pData;

        poSrcBand->GetBlockSize( &nBlockXSize, &nBlockYSize );

        pData = CPLMalloc( nBlockXSize * nBlockYSize * GDALGetDataTypeSize( eType ) / 8 );

        for( iYOffset = 0; iYOffset < nYSize; iYOffset += nBlockYSize )
        {
            for( iXOffset = 0; iXOffset < nXSize; iXOffset += nBlockXSize )
            {
                eErr = poSrcBand->RasterIO( GF_Read, 
                    iXOffset, iYOffset, 
                    nBlockXSize, nBlockYSize,
                    pData, nBlockXSize, nBlockYSize,
                    eType, 0, 0 );
                if( eErr != CE_None )
                {
                    return NULL;
                }
                eErr = poDstBand->RasterIO( GF_Write, 
                    iXOffset, iYOffset, 
                    nBlockXSize, nBlockYSize,
                    pData, nBlockXSize, nBlockYSize,
                    eType, 0, 0 );
                if( eErr != CE_None )
                {
                    return NULL;
                }
            }
            if( ( eErr == CE_None ) && ( ! pfnProgress( 
                ( iYOffset + 1 ) / ( double ) nYSize, NULL, pProgressData ) ) )
            {
                eErr = CE_Failure;
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated CreateCopy()" );
            }
        }
        CPLFree( pData );
    }

    // -------------------------------------------------------------------- 
    // Finalize
    // -------------------------------------------------------------------- 

    poDstDS->FlushCache();

    return poDstDS;
}

//  ----------------------------------------------------------------------------
//                                          IntergraphDataset::GetGeoTransform()
//  ----------------------------------------------------------------------------

CPLErr  IntergraphDataset::GetGeoTransform( double *padfTransform )
{
    GetTransforMatrix( hHeaderOne.TransformationMatrix, padfTransform );

    if( adfGeoTransform[0] == 0.0 &&
        adfGeoTransform[1] == 0.0 &&
        adfGeoTransform[2] == 0.0 &&
        adfGeoTransform[3] == 0.0 &&
        adfGeoTransform[4] == 0.0 &&
        adfGeoTransform[5] == 0.0 )
    {
        adfGeoTransform[0] = 0.0;
        adfGeoTransform[1] = 1.0;
        adfGeoTransform[2] = 0.0; 
        adfGeoTransform[3] = nRasterYSize;
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = -1.0;
    }

    if( GDALPamDataset::GetGeoTransform( padfTransform ) != CE_None )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof( double ) * 6 );
    }

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                          IntergraphDataset::SetGeoTransform()
//  ----------------------------------------------------------------------------

CPLErr IntergraphDataset::SetGeoTransform( double *padfTransform )
{
    if( GDALPamDataset::SetGeoTransform( padfTransform ) != CE_None )
    {
        memcpy( adfGeoTransform, padfTransform, sizeof( double ) * 6 );
    }

    SetTransforMatrix( hHeaderOne.TransformationMatrix, padfTransform );

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                            IntergraphDataset::SetProjection()
//  ----------------------------------------------------------------------------

CPLErr IntergraphDataset::SetProjection( const char *pszProjString )
{   
    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                                           GDALRegister_INGR()
//  ----------------------------------------------------------------------------

void GDALRegister_INGR()
{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "INGR" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "INGR" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Intergraph Raster" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_IntergraphRaster.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
            "Byte Int16 Int32 Float32 Float64" );
        poDriver->pfnOpen = IntergraphDataset::Open;
        poDriver->pfnCreate    = IntergraphDataset::Create;
        poDriver->pfnCreateCopy = IntergraphDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
