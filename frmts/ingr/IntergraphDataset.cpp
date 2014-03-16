/*****************************************************************************
 * $Id$
 *
 * Project:  Intergraph Raster Format support
 * Purpose:  Read/Write Intergraph Raster Format, dataset support
 * Author:   Ivan Lucena, [lucena_ivan at hotmail.com]
 *
 ******************************************************************************
 * Copyright (c) 2007, Ivan Lucena
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

    hVirtual.poDS = NULL;
    hVirtual.poBand = NULL;
    hVirtual.pszFileName = NULL;

    memset(&hHeaderOne, 0, sizeof(hHeaderOne));
    memset(&hHeaderTwo, 0, sizeof(hHeaderTwo));
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
    if( poOpenInfo->nHeaderBytes < 1024 )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Assign Header Information
    // -------------------------------------------------------------------- 

    INGR_HeaderOne hHeaderOne;

    INGR_HeaderOneDiskToMem( &hHeaderOne, (GByte*) poOpenInfo->pabyHeader);

    // -------------------------------------------------------------------- 
    // Check Header Type (HTC) Version
    // -------------------------------------------------------------------- 

    if( hHeaderOne.HeaderType.Version != INGR_HEADER_VERSION )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Check Header Type (HTC) 2D / 3D Flag
    // -------------------------------------------------------------------- 

    if( ( hHeaderOne.HeaderType.Is2Dor3D != INGR_HEADER_2D ) && 
        ( hHeaderOne.HeaderType.Is2Dor3D != INGR_HEADER_3D ) )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Check Header Type (HTC) Type Flag
    // -------------------------------------------------------------------- 

    if( hHeaderOne.HeaderType.Type != INGR_HEADER_TYPE )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Check Grid File Version (VER)
    // -------------------------------------------------------------------- 

    if( hHeaderOne.GridFileVersion != 1 &&
        hHeaderOne.GridFileVersion != 2 &&
        hHeaderOne.GridFileVersion != 3 )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Check Words To Follow (WTC) Minimum Value
    // -------------------------------------------------------------------- 

    if( hHeaderOne.WordsToFollow < 254 )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Check Words To Follow (WTC) Integrity
    // -------------------------------------------------------------------- 

    float fHeaderBlocks = (float) ( hHeaderOne.WordsToFollow + 2 ) / 256;

    if( ( fHeaderBlocks - (int) fHeaderBlocks ) != 0.0 )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Get Data Type Code (DTC) => Format Type
    // -------------------------------------------------------------------- 

    INGR_Format eFormat = (INGR_Format) hHeaderOne.DataTypeCode;

    // -------------------------------------------------------------------- 
    // We need to scan around the file, so we open it now. 
    // -------------------------------------------------------------------- 

    VSILFILE   *fp;

    if( poOpenInfo->eAccess == GA_ReadOnly  )
    {
        fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    } 
    else 
    {
        fp = VSIFOpenL( poOpenInfo->pszFilename, "r+b" );
    }

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "%s", VSIStrerror( errno ) );
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Get Format Type from the tile directory
    // -------------------------------------------------------------------- 

    if( hHeaderOne.DataTypeCode == TiledRasterData )
    {
        INGR_TileHeader hTileDir;

        int nOffset = 2 + ( 2 * ( hHeaderOne.WordsToFollow + 1 ) );

        GByte abyBuffer[SIZEOF_TDIR];

        if( (VSIFSeekL( fp, nOffset, SEEK_SET ) == -1 )  ||
            (VSIFReadL( abyBuffer, 1, SIZEOF_TDIR, fp ) == 0) )
        {
            VSIFCloseL( fp );
            CPLError( CE_Failure, CPLE_AppDefined, 
                "Error reading tiles header" );
            return NULL;
        }

        INGR_TileHeaderDiskToMem( &hTileDir, abyBuffer );

        if( !
          ( hTileDir.ApplicationType     == 1 &&
            hTileDir.SubTypeCode         == 7 &&
            ( hTileDir.WordsToFollow % 4 ) == 0 &&
            hTileDir.PacketVersion       == 1 &&
            hTileDir.Identifier          == 1 ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                "Cannot recognize tiles header info");
            VSIFCloseL( fp );
            return NULL;
        }
            
        eFormat = (INGR_Format) hTileDir.DataTypeCode;
    }

    // -------------------------------------------------------------------- 
    // Check Scannable Flag
    // -------------------------------------------------------------------- 
/*
    if (hHeaderOne.ScannableFlag == HasLineHeader)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
            "Intergraph Raster Scannable Line Header not supported yet" );
        VSIFCloseL( fp );
        return NULL;
    }
*/
    // -------------------------------------------------------------------- 
    // Check supported Format Type
    // -------------------------------------------------------------------- 

    if( eFormat != ByteInteger &&
        eFormat != WordIntegers &&
        eFormat != Integers32Bit &&
        eFormat != FloatingPoint32Bit &&
        eFormat != FloatingPoint64Bit &&
        eFormat != RunLengthEncoded &&
        eFormat != RunLengthEncodedC &&
        eFormat != CCITTGroup4 &&
        eFormat != AdaptiveRGB &&
        eFormat != Uncompressed24bit &&
        eFormat != AdaptiveGrayScale &&
        eFormat != ContinuousTone &&
        eFormat != JPEGGRAY &&
        eFormat != JPEGRGB && 
        eFormat != JPEGCYMK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
            "Intergraph Raster Format %d ( \"%s\" ) not supported",
            hHeaderOne.DataTypeCode, INGR_GetFormatName( (uint16) eFormat ) );
        VSIFCloseL( fp );
        return NULL;
    }

    // -----------------------------------------------------------------
    // Create a corresponding GDALDataset
    // -----------------------------------------------------------------

    IntergraphDataset *poDS;

    poDS = new IntergraphDataset();
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->pszFilename = CPLStrdup( poOpenInfo->pszFilename );
    poDS->fp = fp;

    // -------------------------------------------------------------------- 
    // Get X Size from Pixels Per Line (PPL)
    // -------------------------------------------------------------------- 

    poDS->nRasterXSize = hHeaderOne.PixelsPerLine;

    // -------------------------------------------------------------------- 
    // Get Y Size from Number of Lines (NOL)
    // -------------------------------------------------------------------- 

    poDS->nRasterYSize = hHeaderOne.NumberOfLines;

    if (poDS->nRasterXSize <= 0 || poDS->nRasterYSize <= 0)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid dimensions : %d x %d",
                  poDS->nRasterXSize, poDS->nRasterYSize);
        delete poDS;
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Get Geo Transformation from Homogeneous Transformation Matrix (TRN)
    // -------------------------------------------------------------------- 

    INGR_GetTransMatrix( &hHeaderOne, poDS->adfGeoTransform );

    // -------------------------------------------------------------------- 
    // Set Metadata Information
    // -------------------------------------------------------------------- 

    poDS->SetMetadataItem( "VERSION", 
        CPLSPrintf ( "%d", hHeaderOne.GridFileVersion ), "IMAGE_STRUCTURE" );
    poDS->SetMetadataItem( "RESOLUTION",
        CPLSPrintf ( "%d", (hHeaderOne.DeviceResolution < 0)?-hHeaderOne.DeviceResolution:1) );

    // -------------------------------------------------------------------- 
    // Create Band Information
    // -------------------------------------------------------------------- 

    int nBands = 0;
    int nBandOffset = 0;

    GByte abyBuf[MAX(SIZEOF_HDR1,SIZEOF_HDR2_A)];

    do
    {
        VSIFSeekL( poDS->fp, nBandOffset, SEEK_SET );

        VSIFReadL( abyBuf, 1, SIZEOF_HDR1, poDS->fp );

        INGR_HeaderOneDiskToMem( &poDS->hHeaderOne, abyBuf );

        VSIFReadL( abyBuf, 1, SIZEOF_HDR2_A, poDS->fp );

        INGR_HeaderTwoADiskToMem( &poDS->hHeaderTwo, abyBuf );

        switch( eFormat )
        {
        case JPEGRGB:
        case JPEGCYMK:
        {
            IntergraphBitmapBand* poBand;
            nBands++;
            poDS->SetBand( nBands, 
                poBand = new IntergraphBitmapBand( poDS, nBands, nBandOffset, 1 ));
            if (poBand->pabyBMPBlock == NULL)
            {
                delete poDS;
                return NULL;
            }
            nBands++;
            poDS->SetBand( nBands, 
                poBand = new IntergraphBitmapBand( poDS, nBands, nBandOffset, 2 ));
            if (poBand->pabyBMPBlock == NULL)
            {
                delete poDS;
                return NULL;
            }
            nBands++;
            poDS->SetBand( nBands, 
                poBand = new IntergraphBitmapBand( poDS, nBands, nBandOffset, 3 ));
            if (poBand->pabyBMPBlock == NULL)
            {
                delete poDS;
                return NULL;
            }
            break;
        }
        case JPEGGRAY:
        case CCITTGroup4:
        {
            IntergraphBitmapBand* poBand;
            nBands++;
            poDS->SetBand( nBands, 
                poBand = new IntergraphBitmapBand( poDS, nBands, nBandOffset ));
            if (poBand->pabyBMPBlock == NULL)
            {
                delete poDS;
                return NULL;
            }
            break;
        }
        case RunLengthEncoded:
        case RunLengthEncodedC:
        case AdaptiveGrayScale:
        {
            IntergraphRLEBand* poBand;
            nBands++;
            poDS->SetBand( nBands, 
                poBand = new IntergraphRLEBand( poDS, nBands, nBandOffset ));
            if (poBand->pabyBlockBuf == NULL || poBand->pabyRLEBlock == NULL)
            {
                delete poDS;
                return NULL;
            }
            break;
        }
        case AdaptiveRGB:
        case ContinuousTone:
        {
            IntergraphRLEBand* poBand;
            nBands++;
            poDS->SetBand( nBands, 
                poBand = new IntergraphRLEBand( poDS, nBands, nBandOffset, 1 ));
            if (poBand->pabyBlockBuf == NULL || poBand->pabyRLEBlock == NULL)
            {
                delete poDS;
                return NULL;
            }
            nBands++;
            poDS->SetBand( nBands, 
                poBand = new IntergraphRLEBand( poDS, nBands, nBandOffset, 2 ));
            if (poBand->pabyBlockBuf == NULL || poBand->pabyRLEBlock == NULL)
            {
                delete poDS;
                return NULL;
            }
            nBands++;
            poDS->SetBand( nBands, 
                poBand = new IntergraphRLEBand( poDS, nBands, nBandOffset, 3 ));
            if (poBand->pabyBlockBuf == NULL || poBand->pabyRLEBlock == NULL)
            {
                delete poDS;
                return NULL;
            }
            break;
        }
        case Uncompressed24bit:
        {
            IntergraphRGBBand* poBand;
            nBands++;
            poDS->SetBand( nBands, 
                poBand = new IntergraphRGBBand( poDS, nBands, nBandOffset, 1 ));
            if (poBand->pabyBlockBuf == NULL)
            {
                delete poDS;
                return NULL;
            }
            nBands++;
            poDS->SetBand( nBands, 
                poBand = new IntergraphRGBBand( poDS, nBands, nBandOffset, 2 ));
            if (poBand->pabyBlockBuf == NULL)
            {
                delete poDS;
                return NULL;
            }
            nBands++;
            poDS->SetBand( nBands, 
                poBand = new IntergraphRGBBand( poDS, nBands, nBandOffset, 3 ));
            if (poBand->pabyBlockBuf == NULL)
            {
                delete poDS;
                return NULL;
            }
            break;
        }
        default:
        {
            IntergraphRasterBand* poBand;
            nBands++;
            poDS->SetBand( nBands, 
                poBand = new IntergraphRasterBand( poDS, nBands, nBandOffset ));
            if (poBand->pabyBlockBuf == NULL)
            {
                delete poDS;
                return NULL;
            }
        }
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

    /* -------------------------------------------------------------------- */
    /*      Check for external overviews.                                   */
    /* -------------------------------------------------------------------- */

    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

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
    int nDeviceResolution = 1;
    const char *pszValue;
    const char *pszCompression = NULL;

    pszValue = CSLFetchNameValue(papszOptions, "RESOLUTION");
    if( pszValue != NULL )
        nDeviceResolution = -atoi( pszValue );

    char *pszExtension = CPLStrlwr(CPLStrdup(CPLGetExtension(pszFilename)));
    if ( EQUAL( pszExtension, "rle" ) )
        pszCompression = INGR_GetFormatName(RunLengthEncoded);
    CPLFree(pszExtension);
	
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
    //  Fill headers with minimun information
    // -------------------------------------------------------------------- 

    INGR_HeaderOne  hHdr1;
    INGR_HeaderTwoA hHdr2;
    INGR_ColorTable256 hCTab;
    int             i;
    
    memset(&hHdr1, 0, SIZEOF_HDR1);
    memset(&hHdr2, 0, SIZEOF_HDR2_A);
    memset(&hCTab, 0, SIZEOF_CTAB);

    hHdr1.HeaderType.Version    = INGR_HEADER_VERSION;
    hHdr1.HeaderType.Type       = INGR_HEADER_TYPE;
    hHdr1.HeaderType.Is2Dor3D   = INGR_HEADER_2D;
    hHdr1.DataTypeCode          = (uint16) INGR_GetFormat( eType, (pszCompression!=NULL)?pszCompression:"None" );
    hHdr1.WordsToFollow         = ( ( SIZEOF_HDR1 * 3 ) / 2 ) - 2;
    hHdr1.ApplicationType       = GenericRasterImageFile;
    hHdr1.XViewOrigin           = 0.0;
    hHdr1.YViewOrigin           = 0.0;
    hHdr1.ZViewOrigin           = 0.0;
    hHdr1.XViewExtent           = 0.0;
    hHdr1.YViewExtent           = 0.0;
    hHdr1.ZViewExtent           = 0.0;
    for( i = 0; i < 15; i++ )
        hHdr1.TransformationMatrix[i]   = 0.0;
    hHdr1.TransformationMatrix[15]      = 1.0;
    hHdr1.PixelsPerLine         = nXSize;
    hHdr1.NumberOfLines         = nYSize;
    hHdr1.DeviceResolution      = nDeviceResolution;
    hHdr1.ScanlineOrientation   = UpperLeftHorizontal;
    hHdr1.ScannableFlag         = NoLineHeader;
    hHdr1.RotationAngle         = 0.0;
    hHdr1.SkewAngle             = 0.0;
    hHdr1.DataTypeModifier      = 0;
    hHdr1.DesignFileName[0]     = '\0';
    hHdr1.DataBaseFileName[0]   = '\0';
    hHdr1.ParentGridFileName[0] = '\0';
    hHdr1.FileDescription[0]    = '\0';
    hHdr1.Minimum               = INGR_SetMinMax( eType, 0.0 );
    hHdr1.Maximum               = INGR_SetMinMax( eType, 0.0 );
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
    for( i = 0; i < 110; i++ )
        hHdr2.Reserved[i]       = 0;
    hHdr2.ApplicationPacketLength   = 0;
    hHdr2.ApplicationPacketPointer  = 0;

    // -------------------------------------------------------------------- 
    //  RGB Composite assumption
    // -------------------------------------------------------------------- 

    if( eType  == GDT_Byte  &&
        nBands == 3 )
    {
        hHdr1.DataTypeCode = Uncompressed24bit;
    }

    // -------------------------------------------------------------------- 
    //  Create output file with minimum header info
    // -------------------------------------------------------------------- 

    VSILFILE *fp = VSIFOpenL( pszFilename, "wb+" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
            "Attempt to create file %s' failed.\n", pszFilename );
        return NULL;
    }

    GByte abyBuf[MAX(SIZEOF_HDR1,SIZEOF_CTAB)];

    INGR_HeaderOneMemToDisk( &hHdr1, abyBuf );

    VSIFWriteL( abyBuf, 1, SIZEOF_HDR1, fp );

    INGR_HeaderTwoAMemToDisk( &hHdr2, abyBuf );

    VSIFWriteL( abyBuf, 1, SIZEOF_HDR2_A, fp );

    unsigned int n = 0;

    for( i = 0; i < 256; i++ )
    {
        STRC2BUF( abyBuf, n, hCTab.Entry[i].v_red );
        STRC2BUF( abyBuf, n, hCTab.Entry[i].v_green );
        STRC2BUF( abyBuf, n, hCTab.Entry[i].v_blue );
    }

    VSIFWriteL( abyBuf, 1, SIZEOF_CTAB, fp );

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
    (void) bStrict;

    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Intergraph driver does not support source dataset with zero band.\n");
        return NULL;
    }

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        return NULL;
    }

    // -------------------------------------------------------------------- 
    // Query GDAL Data Type 
    // -------------------------------------------------------------------- 

    GDALDataType eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();

    // -------------------------------------------------------------------- 
    // Copy metadata
    // -------------------------------------------------------------------- 

    char **papszCreateOptions = CSLDuplicate( papszOptions );
    const char  *pszValue;
	
    pszValue = CSLFetchNameValue(papszCreateOptions, "RESOLUTION");
    if( pszValue == NULL )
    {
        const char *value = poSrcDS->GetMetadataItem("RESOLUTION");
        if (value)
        {
            papszCreateOptions = CSLSetNameValue( papszCreateOptions, "RESOLUTION", 
                value );
        }
    }

    // -------------------------------------------------------------------- 
    // Create IntergraphDataset
    // -------------------------------------------------------------------- 

    IntergraphDataset *poDstDS;

    poDstDS = (IntergraphDataset*) IntergraphDataset::Create( pszFilename, 
        poSrcDS->GetRasterXSize(), 
        poSrcDS->GetRasterYSize(), 
        poSrcDS->GetRasterCount(), 
        eType, 
        papszCreateOptions );

    CSLDestroy( papszCreateOptions );

    if( poDstDS == NULL )
    {
        return NULL;
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
    
    for( int i = 1; i <= poDstDS->nBands; i++)
    {
        delete poDstDS->GetRasterBand(i);
    }
    poDstDS->nBands = 0;

    if( poDstDS->hHeaderOne.DataTypeCode == Uncompressed24bit )
    {
        poDstDS->SetBand( 1, new IntergraphRGBBand( poDstDS, 1, 0, 3 ) );
        poDstDS->SetBand( 2, new IntergraphRGBBand( poDstDS, 2, 0, 2 ) );
        poDstDS->SetBand( 3, new IntergraphRGBBand( poDstDS, 3, 0, 1 ) );
        poDstDS->nBands = 3;
    }
    else
    {
        for( int i = 1; i <= poSrcDS->GetRasterCount(); i++ )
        {
            poSrcBand = poSrcDS->GetRasterBand(i);
            eType = poSrcDS->GetRasterBand(i)->GetRasterDataType();

            poDstBand = new IntergraphRasterBand( poDstDS, i, 0, eType );
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

        int   iYOffset, iXOffset;
        void *pData;

        poSrcBand->GetBlockSize( &nBlockXSize, &nBlockYSize );

        nBlockXSize = nXSize;
        nBlockYSize = 1;

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

    INGR_SetTransMatrix( hHeaderOne.TransformationMatrix, padfTransform );

    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                            IntergraphDataset::SetProjection()
//  ----------------------------------------------------------------------------

CPLErr IntergraphDataset::SetProjection( const char *pszProjString )
{   
    (void) pszProjString;

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
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
            "Byte Int16 Int32 Float32 Float64" );
        poDriver->pfnOpen = IntergraphDataset::Open;
        poDriver->pfnCreate    = IntergraphDataset::Create;
        poDriver->pfnCreateCopy = IntergraphDataset::CreateCopy;
        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
