/******************************************************************************
 * $Id: $
 *
 * Name:     georaster_dataset.cpp
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Implement GeoRasterDataset Methods
 * Author:   Ivan Lucena [ivan.lucena at oracle.com]
 *
 ******************************************************************************
 * Copyright (c) 2008, Ivan Lucena
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_error.h"

#include "ogr_spatialref.h"

#include "gdal.h"
#include "gdal_priv.h"
#include "georaster_priv.h"

CPL_C_START
void CPL_DLL GDALRegister_GEOR(void);
CPL_C_END

//  ---------------------------------------------------------------------------
//                                                           GeoRasterDataset()
//  ---------------------------------------------------------------------------

GeoRasterDataset::GeoRasterDataset()
{
    bGeoTransform       = false;
    bForcedSRID         = false;
    poGeoRaster         = NULL;
    papszSubdatasets    = NULL;
    adfGeoTransform[0]  = 0.0;
    adfGeoTransform[1]  = 1.0;
    adfGeoTransform[2]  = 0.0;
    adfGeoTransform[3]  = 0.0;
    adfGeoTransform[4]  = 0.0;
    adfGeoTransform[5]  = 1.0;
    pszProjection       = NULL;
    nGCPCount           = 0;
    pasGCPList          = NULL;
    poMaskBand          = NULL;
    bApplyNoDataArray   = false;
}

//  ---------------------------------------------------------------------------
//                                                          ~GeoRasterDataset()
//  ---------------------------------------------------------------------------

GeoRasterDataset::~GeoRasterDataset()
{
    FlushCache();

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    delete poGeoRaster;

    if( poMaskBand )
    {
        delete poMaskBand;
    }
    
    CPLFree( pszProjection );
    CSLDestroy( papszSubdatasets );
}

//  ---------------------------------------------------------------------------
//                                                                   Identify()
//  ---------------------------------------------------------------------------

int GeoRasterDataset::Identify( GDALOpenInfo* poOpenInfo )
{
    //  -------------------------------------------------------------------
    //  Verify georaster prefix
    //  -------------------------------------------------------------------

    char* pszFilename = poOpenInfo->pszFilename;

    if( EQUALN( pszFilename, "georaster:", 10 ) == false &&
        EQUALN( pszFilename, "geor:", 5 )       == false )
    {
        return false;
    }

    return true;
}

//  ---------------------------------------------------------------------------
//                                                                       Open()
//  ---------------------------------------------------------------------------

GDALDataset* GeoRasterDataset::Open( GDALOpenInfo* poOpenInfo )
{
    //  -------------------------------------------------------------------
    //  It shouldn't have an open file pointer
    //  -------------------------------------------------------------------

    if( poOpenInfo->fp != NULL )
    {
        return NULL;
    }

    //  -------------------------------------------------------------------
    //  Check identification string and usage
    //  -------------------------------------------------------------------

    if( ! Identify( poOpenInfo ) )
    {
        return NULL;
    }

    //  -------------------------------------------------------------------
    //  Create a GeoRaster wrapper object
    //  -------------------------------------------------------------------

    GeoRasterWrapper* poGRW = GeoRasterWrapper::Open(
            poOpenInfo->pszFilename,
            poOpenInfo->eAccess == GA_Update );

    if( ! poGRW )
    {
        return NULL;
    }

    //  -------------------------------------------------------------------
    //  Create a corresponding GDALDataset
    //  -------------------------------------------------------------------

    GeoRasterDataset *poGRD;

    poGRD = new GeoRasterDataset();

    if( ! poGRD )
    {
        return NULL;
    }

    poGRD->eAccess     = poOpenInfo->eAccess;
    poGRD->poGeoRaster = poGRW;

    //  -------------------------------------------------------------------
    //  List Subdatasets
    //  -------------------------------------------------------------------

    if( ! poGRW->bUniqueFound )
    {
        if( poGRD->eAccess == GA_ReadOnly )
        {
            poGRD->SetSubdatasets( poGRW );

            if( CSLCount( poGRD->papszSubdatasets ) == 0 )
            {
                delete poGRD;
                poGRD = NULL;
            }
        }
        return (GDALDataset*) poGRD;
    }

    //  -------------------------------------------------------------------
    //  Assign GeoRaster information
    //  -------------------------------------------------------------------

    poGRD->poGeoRaster   = poGRW;
    poGRD->nRasterXSize  = poGRW->nRasterColumns;
    poGRD->nRasterYSize  = poGRW->nRasterRows;
    poGRD->nBands        = poGRW->nRasterBands;

    if( poGRW->bIsReferenced )
    {
        poGRD->adfGeoTransform[1] = poGRW->dfXCoefficient[0];
        poGRD->adfGeoTransform[2] = poGRW->dfXCoefficient[1];
        poGRD->adfGeoTransform[0] = poGRW->dfXCoefficient[2];
        poGRD->adfGeoTransform[4] = poGRW->dfYCoefficient[0];
        poGRD->adfGeoTransform[5] = poGRW->dfYCoefficient[1];
        poGRD->adfGeoTransform[3] = poGRW->dfYCoefficient[2];
    }

    //  -------------------------------------------------------------------
    //  Copy RPC values to RPC metadata domain
    //  -------------------------------------------------------------------

    if( poGRW->phRPC )
    {
        char **papszRPC_MD = RPCInfoToMD( poGRW->phRPC );
        char **papszSanitazed = NULL;

        int i = 0;
        int n = CSLCount( papszRPC_MD );

        for( i = 0; i < n; i++ )
        {
            if ( EQUALN( papszRPC_MD[i], "MIN_LAT", 7 )  ||
                 EQUALN( papszRPC_MD[i], "MIN_LONG", 8 ) ||
                 EQUALN( papszRPC_MD[i], "MAX_LAT", 7 )  ||
                 EQUALN( papszRPC_MD[i], "MAX_LONG", 8 ) )
            {
                continue;
            }
            papszSanitazed = CSLAddString( papszSanitazed, papszRPC_MD[i] );
        }

        poGRD->SetMetadata( papszSanitazed, "RPC" );

        CSLDestroy( papszRPC_MD );
        CSLDestroy( papszSanitazed );
    }

    //  -------------------------------------------------------------------
    //  Load mask band
    //  -------------------------------------------------------------------

    poGRW->bHasBitmapMask = EQUAL( "TRUE", CPLGetXMLValue( poGRW->phMetadata,
                          "layerInfo.objectLayer.bitmapMask", "FALSE" ) );

    if( poGRW->bHasBitmapMask )
    {
        poGRD->poMaskBand = new GeoRasterRasterBand( poGRD, 0, DEFAULT_BMP_MASK );
    }
    
    //  -------------------------------------------------------------------
    //  Check for filter Nodata environment variable, default is YES
    //  -------------------------------------------------------------------

    const char *pszGEOR_FILTER_NODATA =
        CPLGetConfigOption( "GEOR_FILTER_NODATA_VALUES", "NO" );

    if( ! EQUAL(pszGEOR_FILTER_NODATA, "NO") )
    {
        poGRD->bApplyNoDataArray = true;
    }
    //  -------------------------------------------------------------------
    //  Create bands
    //  -------------------------------------------------------------------

    int i = 0;
    int nBand = 0;

    for( i = 0; i < poGRD->nBands; i++ )
    {
        nBand = i + 1;
        poGRD->SetBand( nBand, new GeoRasterRasterBand( poGRD, nBand, 0 ) );
    }

    //  -------------------------------------------------------------------
    //  Set IMAGE_STRUCTURE metadata information
    //  -------------------------------------------------------------------

    if( poGRW->nBandBlockSize == 1 )
    {
        poGRD->SetMetadataItem( "INTERLEAVE", "BAND", "IMAGE_STRUCTURE" );
    }
    else
    {
        if( EQUAL( poGRW->sInterleaving.c_str(), "BSQ" ) )
        {
            poGRD->SetMetadataItem( "INTERLEAVE", "BAND", "IMAGE_STRUCTURE" );
        }
        else if( EQUAL( poGRW->sInterleaving.c_str(), "BIP" ) )
        {
            poGRD->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
        }
        else if( EQUAL( poGRW->sInterleaving.c_str(), "BIL" ) )
        {
            poGRD->SetMetadataItem( "INTERLEAVE", "LINE", "IMAGE_STRUCTURE" );
        }
    }

    poGRD->SetMetadataItem( "COMPRESSION", CPLGetXMLValue( poGRW->phMetadata,
        "rasterInfo.compression.type", "NONE" ), "IMAGE_STRUCTURE" );

    if( EQUALN( poGRW->sCompressionType.c_str(), "JPEG", 4 ) )
    {
        poGRD->SetMetadataItem( "COMPRESS_QUALITY",
            CPLGetXMLValue( poGRW->phMetadata,
            "rasterInfo.compression.quality", "0" ), "IMAGE_STRUCTURE" );
    }

    if( EQUAL( poGRW->sCellDepth.c_str(), "1BIT" ) )
    {
        poGRD->SetMetadataItem( "NBITS", "1", "IMAGE_STRUCTURE" );
    }

    if( EQUAL( poGRW->sCellDepth.c_str(), "2BIT" ) )
    {
        poGRD->SetMetadataItem( "NBITS", "2", "IMAGE_STRUCTURE" );
    }

    if( EQUAL( poGRW->sCellDepth.c_str(), "4BIT" ) )
    {
        poGRD->SetMetadataItem( "NBITS", "4", "IMAGE_STRUCTURE" );
    }

    //  -------------------------------------------------------------------
    //  Set Metadata on "ORACLE" domain
    //  -------------------------------------------------------------------

    char* pszDoc = CPLSerializeXMLTree( poGRW->phMetadata );

    poGRD->SetMetadataItem( "TABLE_NAME", CPLSPrintf( "%s%s",
        poGRW->sSchema.c_str(),
        poGRW->sTable.c_str()), "ORACLE" );

    poGRD->SetMetadataItem( "COLUMN_NAME",
        poGRW->sColumn.c_str(), "ORACLE" );

    poGRD->SetMetadataItem( "RDT_TABLE_NAME",
        poGRW->sDataTable.c_str(), "ORACLE" );

    poGRD->SetMetadataItem( "RASTER_ID", CPLSPrintf( "%d",
        poGRW->nRasterId ), "ORACLE" );

    poGRD->SetMetadataItem( "SRID", CPLSPrintf( "%d",
        poGRW->nSRID ), "ORACLE" );

    poGRD->SetMetadataItem( "WKT", poGRW->sWKText.c_str(), "ORACLE" );

    poGRD->SetMetadataItem( "METADATA", pszDoc, "ORACLE" );

    CPLFree( pszDoc );

    //  -------------------------------------------------------------------
    //  Return a GDALDataset
    //  -------------------------------------------------------------------

    return (GDALDataset*) poGRD;
}

//  ---------------------------------------------------------------------------
//                                                                     Create()
//  ---------------------------------------------------------------------------

GDALDataset *GeoRasterDataset::Create( const char *pszFilename,
                                       int nXSize,
                                       int nYSize,
                                       int nBands, 
                                       GDALDataType eType,
                                       char **papszOptions )
{
    //  -------------------------------------------------------------------
    //  Check for supported Data types
    //  -------------------------------------------------------------------

    const char* pszCellDepth = OWSetDataType( eType );

    if( EQUAL( pszCellDepth, "Unknown" ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Attempt to create GeoRaster with unsupported data type (%s)",
            GDALGetDataTypeName( eType ) );
        return NULL;
    }

    //  -------------------------------------------------------------------
    //  Open the Dataset
    //  -------------------------------------------------------------------

    GeoRasterDataset* poGRD = NULL;

    poGRD = (GeoRasterDataset*) GDALOpen( pszFilename, GA_Update );

    if( ! poGRD )
    {
        return NULL;
    }

    //  -------------------------------------------------------------------
    //  Get the GeoRaster
    //  -------------------------------------------------------------------

    GeoRasterWrapper* poGRW = poGRD->poGeoRaster;

    if( ! poGRW )
    {
        delete poGRD;
        return NULL;
    }

    //  -------------------------------------------------------------------
    //  Set basic information and default values
    //  -------------------------------------------------------------------

    poGRW->nRasterColumns   = nXSize;
    poGRW->nRasterRows      = nYSize;
    poGRW->nRasterBands     = nBands;
    poGRW->sCellDepth       = pszCellDepth;
    poGRW->nRowBlockSize    = DEFAULT_BLOCK_ROWS;
    poGRW->nColumnBlockSize = DEFAULT_BLOCK_COLUMNS;
    poGRW->nBandBlockSize   = 1;

    if( poGRW->bUniqueFound )
    {
        poGRW->PrepareToOverwrite();
    }

    //  -------------------------------------------------------------------
    //  Check the create options to use in initialization
    //  -------------------------------------------------------------------

    const char* pszFetched  = "";
    char* pszDescription    = NULL;
    char* pszInsert         = NULL;
    int   nQuality          = -1;

    if( ! poGRW->sTable.empty() )
    {
        pszFetched = CSLFetchNameValue( papszOptions, "DESCRIPTION" );

        if( pszFetched )
        {
            pszDescription  = CPLStrdup( pszFetched );
        }
    }

    if( poGRW->sTable.empty() )
    {
        poGRW->sTable = "GDAL_IMPORT";
        poGRW->sDataTable = "GDAL_RDT";
    }

    if( poGRW->sColumn.empty() )
    {
        poGRW->sColumn = "RASTER";
    }

    pszFetched = CSLFetchNameValue( papszOptions, "INSERT" );

    if( pszFetched )
    {
        pszInsert = CPLStrdup( pszFetched );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "BLOCKXSIZE" );

    if( pszFetched )
    {
        poGRW->nColumnBlockSize = atoi( pszFetched );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "BLOCKYSIZE" );

    if( pszFetched )
    {
        poGRW->nRowBlockSize = atoi( pszFetched );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "NBITS" );

    if( pszFetched != NULL )
    {
        poGRW->sCellDepth = CPLSPrintf( "%dBIT", atoi( pszFetched ) );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "COMPRESS" );

    if( pszFetched != NULL &&
        ( EQUALN( pszFetched, "JPEG", 4 ) ||
          EQUAL( pszFetched, "DEFLATE" ) ) )
    {
        poGRW->sCompressionType = pszFetched;
    }
    else
    {
        poGRW->sCompressionType = "NONE";
    }

    pszFetched = CSLFetchNameValue( papszOptions, "QUALITY" );

    if( pszFetched )
    {
        poGRW->nCompressQuality = atoi( pszFetched );
        nQuality = poGRW->nCompressQuality;
    }

    pszFetched = CSLFetchNameValue( papszOptions, "INTERLEAVE" );

    bool bInterleve_ind = false;

    if( pszFetched )
    {
        bInterleve_ind = true;

        if( EQUAL( pszFetched, "BAND" ) ||  EQUAL( pszFetched, "BSQ" ) )
        {
            poGRW->sInterleaving = "BSQ";
        }
        if( EQUAL( pszFetched, "LINE" ) ||  EQUAL( pszFetched, "BIL" ) )
        {
            poGRW->sInterleaving = "BIL";
        }
        if( EQUAL( pszFetched, "PIXEL" ) ||  EQUAL( pszFetched, "BIP" ) )
        {
            poGRW->sInterleaving = "BIP";
        }
    }
    else
    {
        if( EQUAL( poGRW->sCompressionType.c_str(), "NONE" ) == false )
        {
            poGRW->sInterleaving = "BIP";
        }
    }

    pszFetched = CSLFetchNameValue( papszOptions, "BLOCKBSIZE" );

    if( pszFetched )
    {
        poGRW->nBandBlockSize = atoi( pszFetched );
    }
    else
    {
        if( ! EQUAL( poGRW->sCompressionType.c_str(), "NONE" ) &&
          ( nBands == 3 || nBands == 4 ) )
        {
            poGRW->nBandBlockSize = nBands;
        }
    }

    if( bInterleve_ind == false && 
      ( poGRW->nBandBlockSize == 3 || poGRW->nBandBlockSize == 4 ) ) 
    {
      poGRW->sInterleaving = "BIP";
    }

    if( EQUALN( poGRW->sCompressionType.c_str(), "JPEG", 4 ) )
    {
        if( ! EQUAL( poGRW->sInterleaving.c_str(), "BIP" ) )
        {
            CPLError( CE_Warning, CPLE_IllegalArg, 
                "compress=JPEG assumes interleave=BIP" );
            poGRW->sInterleaving = "BIP";
        }
    }

    pszFetched = CSLFetchNameValue( papszOptions, "BLOCKING" );

    if( pszFetched )
    {
        if( EQUAL( pszFetched, "NO" ) )
        {
            poGRW->bBlocking = false;
        }

        if( EQUAL( pszFetched, "OPTIMALPADDING" ) )
        {
            if( poGRW->poConnection->GetVersion() < 11 )
            {
                CPLError( CE_Warning, CPLE_IllegalArg, 
                    "BLOCKING=OPTIMALPADDING not supported on Oracle older than 11g" );
            }
            else
            {
                poGRW->bAutoBlocking = true;
                poGRW->bBlocking = true;
            }
        }
    }

    //  -------------------------------------------------------------------
    //  Validate options
    //  -------------------------------------------------------------------

    if( pszDescription && poGRW->bUniqueFound )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, 
            "Option (DESCRIPTION) cannot be used on a existing GeoRaster." );
        delete poGRD;
        return NULL;
    }

    if( pszInsert && poGRW->bUniqueFound )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, 
            "Option (INSERT) cannot be used on a existing GeoRaster." );
        delete poGRD;
        return NULL;
    }

    /* Compression JPEG-B is deprecated. It should be able to read but to
     * to create new GeoRaster on databases with that compression option.
     *
     * TODO: Remove that options on next release.
     */
    if( EQUAL( poGRW->sCompressionType.c_str(), "JPEG-B" ) )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
            "Option (COMPRESS=%s) is deprecated and cannot be used.",
            poGRW->sCompressionType.c_str() );
        delete poGRD;
        return NULL;
    }

    if( EQUAL( poGRW->sCompressionType.c_str(), "JPEG-F" ) )
    {
        /* JPEG-F can only compress byte data type
         */
        if( eType != GDT_Byte )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, 
                "Option (COMPRESS=%s) can only be used with Byte data type.",
                poGRW->sCompressionType.c_str() );
            delete poGRD;
            return NULL;
        }

        /* JPEG-F can compress one band per block or 3 for RGB
         * or 4 for RGBA.
         */
        if( ( poGRW->nBandBlockSize != 1 &&
              poGRW->nBandBlockSize != 3 &&
              poGRW->nBandBlockSize != 4 ) ||
          ( ( poGRW->nBandBlockSize != 1 &&
            ( poGRW->nBandBlockSize != poGRW->nRasterBands ) ) ) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                "Option (COMPRESS=%s) requires BLOCKBSIZE to be 1 (for any "
                "number of bands), 3 (for 3 bands RGB) and 4 (for 4 bands RGBA).",
                poGRW->sCompressionType.c_str() );
            delete poGRD;
            return NULL;
        }

        /* There is a limite on how big a compressed block can be.
         */
        if( ( poGRW->nColumnBlockSize * 
              poGRW->nRowBlockSize *
              poGRW->nBandBlockSize *
              ( GDALGetDataTypeSize( eType ) / 8 ) ) > ( 50 * 1024 * 1024 ) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, 
                "Option (COMPRESS=%s) each data block must not exceed 50Mb. "
                "Consider reducing BLOCK{X,Y,B}XSIZE.",
                poGRW->sCompressionType.c_str() );
            delete poGRD;
            return NULL;
        }
    }

    if( EQUAL( poGRW->sCompressionType.c_str(), "DEFLATE" ) )
    {
        if( ( poGRW->nColumnBlockSize * 
              poGRW->nRowBlockSize *
              poGRW->nBandBlockSize *
              ( GDALGetDataTypeSize( eType ) / 8 ) ) > ( 1024 * 1024 * 1024 ) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, 
                "For (COMPRESS=%s) each data block must not exceed 1Gb. "
                "Consider reducing BLOCK{X,Y,B}XSIZE.",
                poGRW->sCompressionType.c_str() );
            delete poGRD;
            return NULL;
        }
    }

    pszFetched = CSLFetchNameValue( papszOptions, "OBJECTTABLE" );

    if( pszFetched )
    {
        int nVersion = poGRW->poConnection->GetVersion();
        if( nVersion <= 11 )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, 
                "Driver create-option OBJECTTABLE not "
                "supported on Oracle %d", nVersion );
            delete poGRD;
            return NULL;
        }
    }

    poGRD->poGeoRaster->bCreateObjectTable = (bool)
        CSLFetchBoolean( papszOptions, "OBJECTTABLE", FALSE );

    //  -------------------------------------------------------------------
    //  Create a SDO_GEORASTER object on the server
    //  -------------------------------------------------------------------

    bool bSucced = poGRW->Create( pszDescription, pszInsert, poGRW->bUniqueFound );

    CPLFree( pszInsert );
    CPLFree( pszDescription );

    if( ! bSucced )
    {
        delete poGRD;
        return NULL;
    }
    
    //  -------------------------------------------------------------------
    //  Prepare an identification string
    //  -------------------------------------------------------------------

    char szStringId[OWTEXT];

    strcpy( szStringId, CPLSPrintf( "georaster:%s,%s,%s,%s,%d",
        poGRW->poConnection->GetUser(),
        poGRW->poConnection->GetPassword(),
        poGRW->poConnection->GetServer(),
        poGRW->sDataTable.c_str(),
        poGRW->nRasterId ) );

    delete poGRD;

    poGRD = (GeoRasterDataset*) GDALOpen( szStringId, GA_Update );

    if( ! poGRD )
    {
        return NULL;
    }

    //  -------------------------------------------------------------------
    //  Load aditional options
    //  -------------------------------------------------------------------

    pszFetched = CSLFetchNameValue( papszOptions, "VATNAME" );

    if( pszFetched )
    {
        poGRW->sValueAttributeTab = pszFetched;
    }

    pszFetched = CSLFetchNameValue( papszOptions, "SRID" );

    if( pszFetched )
    {
        poGRD->bForcedSRID = true;
        poGRD->poGeoRaster->SetGeoReference( atoi( pszFetched ) );
    }

    poGRD->poGeoRaster->bGenSpatialIndex = (bool)
        CSLFetchBoolean( papszOptions, "SPATIALEXTENT", TRUE );

    pszFetched = CSLFetchNameValue( papszOptions, "EXTENTSRID" );

    if( pszFetched )
    {
        poGRD->poGeoRaster->nExtentSRID = atoi( pszFetched );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "COORDLOCATION" );

    if( pszFetched )
    {
        if( EQUAL( pszFetched, "CENTER" ) )
        {
            poGRD->poGeoRaster->eModelCoordLocation = MCL_CENTER;
        }
        else if( EQUAL( pszFetched, "UPPERLEFT" ) )
        {
            poGRD->poGeoRaster->eModelCoordLocation = MCL_UPPERLEFT;
        }
        else 
        {
            CPLError( CE_Warning, CPLE_IllegalArg, 
                "Incorrect COORDLOCATION (%s)", pszFetched );
        }
    }

    if ( nQuality > 0 )
    {
        poGRD->poGeoRaster->nCompressQuality = nQuality;
    }

    pszFetched = CSLFetchNameValue( papszOptions, "GENPYRAMID" );

    if( pszFetched != NULL )
    {
        if (!(EQUAL(pszFetched, "NN") ||
              EQUAL(pszFetched, "BILINEAR") ||
              EQUAL(pszFetched, "BIQUADRATIC") ||
              EQUAL(pszFetched, "CUBIC") ||
              EQUAL(pszFetched, "AVERAGE4") ||
              EQUAL(pszFetched, "AVERAGE16")))
        {
            CPLError( CE_Warning, CPLE_IllegalArg, "Wrong resample method for pyramid (%s)", pszFetched);
        }

        poGRD->poGeoRaster->bGenPyramid = true;
        poGRD->poGeoRaster->sPyramidResampling = pszFetched;
    }

    pszFetched = CSLFetchNameValue( papszOptions, "GENPYRLEVELS" );

    if( pszFetched != NULL )
    {
        poGRD->poGeoRaster->bGenPyramid = true;
        poGRD->poGeoRaster->nPyramidLevels = atoi(pszFetched);
    }

    //  -------------------------------------------------------------------
    //  Return a new Dataset
    //  -------------------------------------------------------------------

    return (GDALDataset*) poGRD;
}

//  ---------------------------------------------------------------------------
//                                                                 CreateCopy()
//  ---------------------------------------------------------------------------

GDALDataset *GeoRasterDataset::CreateCopy( const char* pszFilename,
                                           GDALDataset* poSrcDS,
                                           int bStrict,
                                           char** papszOptions,
                                           GDALProgressFunc pfnProgress,
                                           void* pProgressData )
{
    (void) bStrict;

    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
        "GeoRaster driver does not support source dataset with zero band.\n");
        return NULL;
    }

    GDALRasterBand* poBand = poSrcDS->GetRasterBand( 1 );
    GDALDataType    eType  = poBand->GetRasterDataType();

    //  -----------------------------------------------------------
    //  Create a GeoRaster on the server or select one to overwrite
    //  -----------------------------------------------------------

    GeoRasterDataset *poDstDS;

    poDstDS = (GeoRasterDataset *) GeoRasterDataset::Create( pszFilename,
        poSrcDS->GetRasterXSize(),
        poSrcDS->GetRasterYSize(),
        poSrcDS->GetRasterCount(),
        eType, papszOptions );

    if( poDstDS == NULL )
    {
        return NULL;
    }

    //  -----------------------------------------------------------
    //  Copy information to the dataset
    //  -----------------------------------------------------------

    double adfTransform[6];

    if ( poSrcDS->GetGeoTransform( adfTransform ) == CE_None )
    {
        if ( ! ( adfTransform[0] == 0.0 && 
                 adfTransform[1] == 1.0 &&
                 adfTransform[2] == 0.0 &&
                 adfTransform[3] == 0.0 &&
                 adfTransform[4] == 0.0 &&
                 adfTransform[5] == 1.0 ) ) 
        {
            poDstDS->SetGeoTransform( adfTransform );
        }
    }

    if( ! poDstDS->bForcedSRID ) /* forced by create option SRID */
    {
        poDstDS->SetProjection( poSrcDS->GetProjectionRef() );
    }

    // --------------------------------------------------------------------
    //      Copy RPC 
    // --------------------------------------------------------------------

    char **papszRPCMetadata = GDALGetMetadata( poSrcDS, "RPC" );

    if ( papszRPCMetadata != NULL )
    {
        poDstDS->poGeoRaster->phRPC = (GDALRPCInfo*) VSIMalloc( sizeof(GDALRPCInfo) );
        GDALExtractRPCInfo( papszRPCMetadata, poDstDS->poGeoRaster->phRPC );
    }

    // --------------------------------------------------------------------
    //      Copy information to the raster bands
    // --------------------------------------------------------------------

    int    bHasNoDataValue = FALSE;
    double dfNoDataValue = 0.0;
    double dfMin = 0.0, dfMax = 0.0, dfStdDev = 0.0, dfMean = 0.0;
    double dfMedian = 0.0, dfMode = 0.0;
    int    iBand = 0;

    for( iBand = 1; iBand <= poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand*      poSrcBand = poSrcDS->GetRasterBand( iBand );
        GeoRasterRasterBand* poDstBand = (GeoRasterRasterBand*) 
                                         poDstDS->GetRasterBand( iBand );

        // ----------------------------------------------------------------
        //  Copy Color Table
        // ----------------------------------------------------------------

        GDALColorTable* poColorTable = poSrcBand->GetColorTable(); 

        if( poColorTable )
        {
            poDstBand->SetColorTable( poColorTable );
        }

        // ----------------------------------------------------------------
        //  Copy statitics information, without median and mode
        // ----------------------------------------------------------------

        if( poSrcBand->GetStatistics( false, false, &dfMin, &dfMax,
            &dfMean, &dfStdDev ) == CE_None )
        {
            poDstBand->SetStatistics( dfMin, dfMax, dfMean, dfStdDev );

            /* That will not be recorded in the GeoRaster metadata since it
             * doesn't have median and mode, so those values are only useful
             * at runtime.
             */
        }

        // ----------------------------------------------------------------
        //  Copy statitics metadata information, including median and mode
        // ----------------------------------------------------------------

        const char *pszMin     = poSrcBand->GetMetadataItem( "STATISTICS_MINIMUM" );
        const char *pszMax     = poSrcBand->GetMetadataItem( "STATISTICS_MAXIMUM" );
        const char *pszMean    = poSrcBand->GetMetadataItem( "STATISTICS_MEAN" );
        const char *pszMedian  = poSrcBand->GetMetadataItem( "STATISTICS_MEDIAN" );
        const char *pszMode    = poSrcBand->GetMetadataItem( "STATISTICS_MODE" );
        const char *pszStdDev  = poSrcBand->GetMetadataItem( "STATISTICS_STDDEV" );
        const char *pszSkipFX  = poSrcBand->GetMetadataItem( "STATISTICS_SKIPFACTORX" );
        const char *pszSkipFY  = poSrcBand->GetMetadataItem( "STATISTICS_SKIPFACTORY" );

        if ( pszMin    != NULL && pszMax  != NULL && pszMean   != NULL &&
             pszMedian != NULL && pszMode != NULL && pszStdDev != NULL )
        {
            dfMin        = CPLScanDouble( pszMin, MAX_DOUBLE_STR_REP );
            dfMax        = CPLScanDouble( pszMax, MAX_DOUBLE_STR_REP );
            dfMean       = CPLScanDouble( pszMean, MAX_DOUBLE_STR_REP );
            dfMedian     = CPLScanDouble( pszMedian, MAX_DOUBLE_STR_REP );
            dfMode       = CPLScanDouble( pszMode, MAX_DOUBLE_STR_REP );

            if ( ! ( ( dfMin    > dfMax ) ||
                     ( dfMean   > dfMax ) || ( dfMean   < dfMin ) ||
                     ( dfMedian > dfMax ) || ( dfMedian < dfMin ) ||
                     ( dfMode   > dfMax ) || ( dfMode   < dfMin ) ) )
            {
                if ( ! pszSkipFX )
                {
                    pszSkipFX = pszSkipFY != NULL ? pszSkipFY : "1";
                }

                poDstBand->poGeoRaster->SetStatistics( iBand,
                                                       pszMin, pszMax, pszMean, 
                                                       pszMedian, pszMode,
                                                       pszStdDev, pszSkipFX );
            }
        }

        // ----------------------------------------------------------------
        //  Copy Raster Attribute Table (RAT)
        // ----------------------------------------------------------------

        GDALRasterAttributeTableH poRAT = GDALGetDefaultRAT( poSrcBand );

        if( poRAT != NULL )
        {
            poDstBand->SetDefaultRAT( (GDALRasterAttributeTable*) poRAT );
        }

        // ----------------------------------------------------------------
        //  Copy NoData Value
        // ----------------------------------------------------------------

        dfNoDataValue = poSrcBand->GetNoDataValue( &bHasNoDataValue );

        if( bHasNoDataValue )
        {
            poDstBand->SetNoDataValue( dfNoDataValue );
        }
    }

    // --------------------------------------------------------------------
    //  Copy actual imagery.
    // --------------------------------------------------------------------

    int nXSize = poDstDS->GetRasterXSize();
    int nYSize = poDstDS->GetRasterYSize();

    int nBlockXSize = 0;
    int nBlockYSize = 0;

    poDstDS->GetRasterBand( 1 )->GetBlockSize( &nBlockXSize, &nBlockYSize );

    void *pData = VSIMalloc( nBlockXSize * nBlockYSize *
        GDALGetDataTypeSize( eType ) / 8 );

    if( pData == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
            "GeoRaster::CreateCopy : Out of memory " );
        delete poDstDS;
        return NULL;
    }

    int iYOffset = 0;
    int iXOffset = 0;
    int iXBlock  = 0;
    int iYBlock  = 0;
    int nBlockCols = 0;
    int nBlockRows = 0;
    CPLErr eErr = CE_None;

    int nPixelSize = GDALGetDataTypeSize( 
        poSrcDS->GetRasterBand(1)->GetRasterDataType() ) / 8;

    if( poDstDS->poGeoRaster->nBandBlockSize == 1)
    {
        // ----------------------------------------------------------------
        //  Band order
        // ----------------------------------------------------------------

        int nBandCount = poSrcDS->GetRasterCount();

        for( iBand = 1; iBand <= nBandCount; iBand++ )
        {
            GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand );
            GDALRasterBand *poDstBand = poDstDS->GetRasterBand( iBand );

            for( iYOffset = 0, iYBlock = 0;
                 iYOffset < nYSize;
                 iYOffset += nBlockYSize, iYBlock++ )
            {

                for( iXOffset = 0, iXBlock = 0;
                     iXOffset < nXSize;
                     iXOffset += nBlockXSize, iXBlock++ )
                {

                    nBlockCols = MIN( nBlockXSize, nXSize - iXOffset );
                    nBlockRows = MIN( nBlockYSize, nYSize - iYOffset );

                    eErr = poSrcBand->RasterIO( GF_Read,
                        iXOffset, iYOffset,
                        nBlockCols, nBlockRows, pData,
                        nBlockCols, nBlockRows, eType,
                        nPixelSize,
                        nPixelSize * nBlockXSize );

                    if( eErr != CE_None )
                    {
                        return NULL;
                    }

                    eErr = poDstBand->WriteBlock( iXBlock, iYBlock, pData );

                    if( eErr != CE_None )
                    {
                        return NULL;
                    }
                }

                if( ( eErr == CE_None ) && ( ! pfnProgress(
                      ( ( iBand - 1) / (float) nBandCount ) +
                      ( iYOffset + nBlockRows ) / (float) (nYSize * nBandCount),
                      NULL, pProgressData ) ) )
                {
                    eErr = CE_Failure;
                    CPLError( CE_Failure, CPLE_UserInterrupt,
                        "User terminated CreateCopy()" );
                }
            }
        }
    }
    else
    {
        // ----------------------------------------------------------------
        //  Block order
        // ----------------------------------------------------------------

        poDstDS->poGeoRaster->SetWriteOnly( true );

        for( iYOffset = 0, iYBlock = 0;
             iYOffset < nYSize;
             iYOffset += nBlockYSize, iYBlock++ )
        {
            for( iXOffset = 0, iXBlock = 0;
                 iXOffset < nXSize;
                 iXOffset += nBlockXSize, iXBlock++ )
            {
                nBlockCols = MIN( nBlockXSize, nXSize - iXOffset );
                nBlockRows = MIN( nBlockYSize, nYSize - iYOffset );

                for( iBand = 1;
                     iBand <= poSrcDS->GetRasterCount();
                     iBand++ )
                {
                    GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand );
                    GDALRasterBand *poDstBand = poDstDS->GetRasterBand( iBand );

                    eErr = poSrcBand->RasterIO( GF_Read,
                        iXOffset, iYOffset,
                        nBlockCols, nBlockRows, pData,
                        nBlockCols, nBlockRows, eType,
                        nPixelSize,
                        nPixelSize * nBlockXSize );

                    if( eErr != CE_None )
                    {
                        return NULL;
                    }

                    eErr = poDstBand->WriteBlock( iXBlock, iYBlock, pData );

                    if( eErr != CE_None )
                    {
                        return NULL;
                    }
                }

            }

            if( ( eErr == CE_None ) && ( ! pfnProgress(
                ( iYOffset + nBlockRows ) / (double) nYSize, NULL,
                    pProgressData ) ) )
            {
                eErr = CE_Failure;
                CPLError( CE_Failure, CPLE_UserInterrupt,
                    "User terminated CreateCopy()" );
            }
        }
    }

    CPLFree( pData );

    // --------------------------------------------------------------------
    //      Finalize
    // --------------------------------------------------------------------

    poDstDS->FlushCache();

    if( pfnProgress )
    {
        printf( "Ouput dataset: (georaster:%s/%s@%s,%s,%d) on %s%s,%s\n",
            poDstDS->poGeoRaster->poConnection->GetUser(),
            poDstDS->poGeoRaster->poConnection->GetPassword(),
            poDstDS->poGeoRaster->poConnection->GetServer(),
            poDstDS->poGeoRaster->sDataTable.c_str(),
            poDstDS->poGeoRaster->nRasterId,
            poDstDS->poGeoRaster->sSchema.c_str(),
            poDstDS->poGeoRaster->sTable.c_str(),
            poDstDS->poGeoRaster->sColumn.c_str() );
    }

    return poDstDS;
}

//  ---------------------------------------------------------------------------
//                                                                  IRasterIO()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::IRasterIO( GDALRWFlag eRWFlag,
                                    int nXOff, int nYOff, int nXSize, int nYSize,
                                    void *pData, int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType,
                                    int nBandCount, int *panBandMap,
                                    int nPixelSpace, int nLineSpace, int nBandSpace )

{
    if( poGeoRaster->nBandBlockSize > 1 )
    {
        return GDALDataset::BlockBasedRasterIO( eRWFlag,
            nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nBandCount, panBandMap, nPixelSpace,
            nLineSpace, nBandSpace );
    }
    else
    {
        return GDALDataset::IRasterIO( eRWFlag,
            nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nBandCount, panBandMap,
            nPixelSpace, nLineSpace, nBandSpace );
    }
}

//  ---------------------------------------------------------------------------
//                                                                 FlushCache()
//  ---------------------------------------------------------------------------

void GeoRasterDataset::FlushCache()
{
    GDALDataset::FlushCache();
}

//  ---------------------------------------------------------------------------
//                                                            GetGeoTransform()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::GetGeoTransform( double *padfTransform )
{
    if( poGeoRaster->phRPC )
    {
        return CE_Failure;
    }

    if( poGeoRaster->nSRID == 0 )
    {
        return CE_Failure;
    }
    
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );

    bGeoTransform = true;

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                           GetProjectionRef()
//  ---------------------------------------------------------------------------

const char* GeoRasterDataset::GetProjectionRef( void )
{
    if( poGeoRaster->phRPC )
    {
        return "";
    }

    if( ! poGeoRaster->bIsReferenced )
    {
        return "";
    }

    if( poGeoRaster->nSRID == UNKNOWN_CRS || poGeoRaster->nSRID == 0 )
    {
        return "";
    }

    if( pszProjection )
    {
        return pszProjection;
    }

    OGRSpatialReference oSRS;

    // --------------------------------------------------------------------
    // Check if the SRID is a valid EPSG code
    // --------------------------------------------------------------------

    CPLPushErrorHandler( CPLQuietErrorHandler );

    if( oSRS.importFromEPSG( poGeoRaster->nSRID ) == OGRERR_NONE )
    {
        /*
         * Ignores the WKT from Oracle and use the one from GDAL's
         * EPSG tables. That would ensure that other drivers/software
         * will recognizize the parameters.
         */

        if( oSRS.exportToWkt( &pszProjection ) == OGRERR_NONE )
        {
            CPLPopErrorHandler();

            return pszProjection;
        }
    }

    CPLPopErrorHandler();

    // --------------------------------------------------------------------
    // Try to interpreter the WKT text
    // --------------------------------------------------------------------

    char* pszWKText = CPLStrdup( poGeoRaster->sWKText );

    if( ! ( oSRS.importFromWkt( &pszWKText ) == OGRERR_NONE && oSRS.GetRoot() ) )
    {
        return "";
    }

    // ----------------------------------------------------------------
    // Decorate with ORACLE Authority codes
    // ----------------------------------------------------------------

    oSRS.SetAuthority(oSRS.GetRoot()->GetValue(), "ORACLE", poGeoRaster->nSRID);

    int nSpher = OWParseEPSG( oSRS.GetAttrValue("GEOGCS|DATUM|SPHEROID") );

    if( nSpher > 0 )
    {
        oSRS.SetAuthority( "GEOGCS|DATUM|SPHEROID", "EPSG", nSpher );
    }

    int nDatum = OWParseEPSG( oSRS.GetAttrValue("GEOGCS|DATUM") );

    if( nDatum > 0 )
    {
        oSRS.SetAuthority( "GEOGCS|DATUM", "EPSG", nDatum );
    }

    // ----------------------------------------------------------------
    // Checks for Projection info
    // ----------------------------------------------------------------

    const char *pszProjName = oSRS.GetAttrValue( "PROJECTION" );

    if( pszProjName )
    {
        int nProj = OWParseEPSG( pszProjName );

        // ----------------------------------------------------------------
        // Decorate with EPSG Authority
        // ----------------------------------------------------------------

        if( nProj > 0 )
        {
            oSRS.SetAuthority( "PROJECTION", "EPSG", nProj );
        }

        // ----------------------------------------------------------------
        // Translate projection names to GDAL's standards
        // ----------------------------------------------------------------

        if ( EQUAL( pszProjName, "Transverse Mercator" ) )
        {
            oSRS.SetProjection( SRS_PT_TRANSVERSE_MERCATOR );
        }
        else if ( EQUAL( pszProjName, "Albers Conical Equal Area" ) )
        {
            oSRS.SetProjection( SRS_PT_ALBERS_CONIC_EQUAL_AREA );
        }
        else if ( EQUAL( pszProjName, "Azimuthal Equidistant" ) )
        {
            oSRS.SetProjection( SRS_PT_AZIMUTHAL_EQUIDISTANT );
        }
        else if ( EQUAL( pszProjName, "Miller Cylindrical" ) )
        {
            oSRS.SetProjection( SRS_PT_MILLER_CYLINDRICAL );
        }
        else if ( EQUAL( pszProjName, "Hotine Oblique Mercator" ) )
        {
            oSRS.SetProjection( SRS_PT_HOTINE_OBLIQUE_MERCATOR );
        }
        else if ( EQUAL( pszProjName, "Wagner IV" ) )
        {
            oSRS.SetProjection( SRS_PT_WAGNER_IV );
        }
        else if ( EQUAL( pszProjName, "Wagner VII" ) )
        {
            oSRS.SetProjection( SRS_PT_WAGNER_VII );
        }
        else if ( EQUAL( pszProjName, "Eckert IV" ) )
        {
            oSRS.SetProjection( SRS_PT_ECKERT_IV );
        }
        else if ( EQUAL( pszProjName, "Eckert VI" ) )
        {
            oSRS.SetProjection( SRS_PT_ECKERT_VI );
        }
        else if ( EQUAL( pszProjName, "New Zealand Map Grid" ) )
        {
            oSRS.SetProjection( SRS_PT_NEW_ZEALAND_MAP_GRID );
        }
        else if ( EQUAL( pszProjName, "Lambert Conformal Conic" ) )
        {
            oSRS.SetProjection( SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP );
        }
        else if ( EQUAL( pszProjName, "Lambert Azimuthal Equal Area" ) )
        {
            oSRS.SetProjection( SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA );
        }
        else if ( EQUAL( pszProjName, "Van der Grinten" ) )
        {
            oSRS.SetProjection( SRS_PT_VANDERGRINTEN );
        }
        else if ( EQUAL(
            pszProjName, "Lambert Conformal Conic (Belgium 1972)" ) )
        {
            oSRS.SetProjection( SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM );
        }
        else if ( EQUAL( pszProjName, "Cylindrical Equal Area" ) )
        {
            oSRS.SetProjection( SRS_PT_CYLINDRICAL_EQUAL_AREA );
        }
        else if ( EQUAL( pszProjName, "Interrupted Goode Homolosine" ) )
        {
            oSRS.SetProjection( SRS_PT_GOODE_HOMOLOSINE );
        }
    }

    oSRS.exportToWkt( &pszProjection );

    return pszProjection;
}

//  ---------------------------------------------------------------------------
//                                                            SetGeoTransform()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::SetGeoTransform( double *padfTransform )
{
    memcpy( adfGeoTransform, padfTransform, sizeof( double ) * 6 );

    poGeoRaster->dfXCoefficient[0] = adfGeoTransform[1];
    poGeoRaster->dfXCoefficient[1] = adfGeoTransform[2];
    poGeoRaster->dfXCoefficient[2] = adfGeoTransform[0];
    poGeoRaster->dfYCoefficient[0] = adfGeoTransform[4];
    poGeoRaster->dfYCoefficient[1] = adfGeoTransform[5];
    poGeoRaster->dfYCoefficient[2] = adfGeoTransform[3];

    bGeoTransform = true;

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                              SetProjection()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::SetProjection( const char *pszProjString )
{
    OGRSpatialReference oSRS;

    char* pszWKT = CPLStrdup( pszProjString );

    OGRErr eOGRErr = oSRS.importFromWkt( &pszWKT );

    if( eOGRErr != OGRERR_NONE )
    {
        poGeoRaster->SetGeoReference( DEFAULT_CRS );

        return CE_Failure;
    }

    // --------------------------------------------------------------------
    // Try to extract EPGS authority code
    // --------------------------------------------------------------------

    const char *pszAuthName = NULL, *pszAuthCode = NULL;

    if( oSRS.IsGeographic() )
    {
        pszAuthName = oSRS.GetAuthorityName( "GEOGCS" );
        pszAuthCode = oSRS.GetAuthorityCode( "GEOGCS" );
    }
    else if( oSRS.IsProjected() )
    {
        pszAuthName = oSRS.GetAuthorityName( "PROJCS" );
        pszAuthCode = oSRS.GetAuthorityCode( "PROJCS" );
    }

    if( pszAuthName != NULL && pszAuthCode != NULL )
    {
        if( EQUAL( pszAuthName, "ORACLE" ) || 
            EQUAL( pszAuthName, "EPSG" ) )
        {
            poGeoRaster->SetGeoReference( atoi( pszAuthCode ) );
            return CE_None;
        }
    }

    // ----------------------------------------------------------------
    // Convert SRS into old style format (SF-SQL 1.0)
    // ----------------------------------------------------------------

    OGRSpatialReference *poSRS2 = oSRS.Clone();
    
    poSRS2->StripCTParms();

    double dfAngularUnits = poSRS2->GetAngularUnits( NULL );
    
    if( fabs(dfAngularUnits - 0.0174532925199433) < 0.0000000000000010 )
    {
        /* match the precision used on Oracle for that particular value */

        poSRS2->SetAngularUnits( "Decimal Degree", 0.0174532925199433 );
    }

    char* pszCloneWKT = NULL;

    if( poSRS2->exportToWkt( &pszCloneWKT ) != OGRERR_NONE )
    {
        delete poSRS2;
        return CE_Failure;
    }
    
    const char *pszProjName = poSRS2->GetAttrValue( "PROJECTION" );

    if( pszProjName )
    {
        // ----------------------------------------------------------------
        // Translate projection names to Oracle's standards
        // ----------------------------------------------------------------

        if ( EQUAL( pszProjName, SRS_PT_TRANSVERSE_MERCATOR ) )
        {
            poSRS2->SetProjection( "Transverse Mercator" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_ALBERS_CONIC_EQUAL_AREA ) )
        {
            poSRS2->SetProjection( "Albers Conical Equal Area" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_AZIMUTHAL_EQUIDISTANT ) )
        {
            poSRS2->SetProjection( "Azimuthal Equidistant" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_MILLER_CYLINDRICAL ) )
        {
            poSRS2->SetProjection( "Miller Cylindrical" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_HOTINE_OBLIQUE_MERCATOR ) )
        {
            poSRS2->SetProjection( "Hotine Oblique Mercator" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_WAGNER_IV ) )
        {
            poSRS2->SetProjection( "Wagner IV" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_WAGNER_VII ) )
        {
            poSRS2->SetProjection( "Wagner VII" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_ECKERT_IV ) )
        {
            poSRS2->SetProjection( "Eckert IV" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_ECKERT_VI ) )
        {
            poSRS2->SetProjection( "Eckert VI" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_NEW_ZEALAND_MAP_GRID ) )
        {
            poSRS2->SetProjection( "New Zealand Map Grid" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP ) )
        {
            poSRS2->SetProjection( "Lambert Conformal Conic" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA ) )
        {
            poSRS2->SetProjection( "Lambert Azimuthal Equal Area" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_VANDERGRINTEN ) )
        {
            poSRS2->SetProjection( "Van der Grinten" );
        }
        else if ( EQUAL(
            pszProjName, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM ) )
        {
            poSRS2->SetProjection( "Lambert Conformal Conic (Belgium 1972)" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_CYLINDRICAL_EQUAL_AREA ) )
        {
            poSRS2->SetProjection( "Cylindrical Equal Area" );
        }
        else if ( EQUAL( pszProjName, SRS_PT_GOODE_HOMOLOSINE ) )
        {
            poSRS2->SetProjection( "Interrupted Goode Homolosine" );
        }
        
        // ----------------------------------------------------------------
        // Translate projection's parameters to Oracle's standards
        // ----------------------------------------------------------------

        char* pszStart = NULL;
        
        CPLFree( pszCloneWKT );       

        if( poSRS2->exportToWkt( &pszCloneWKT ) != OGRERR_NONE )
        {
            delete poSRS2;
            return CE_Failure;
        }
        
        if( ( pszStart = strstr(pszCloneWKT, SRS_PP_AZIMUTH) ) )
        {
            strncpy( pszStart, "Azimuth", strlen(SRS_PP_AZIMUTH) );
        }

        if( ( pszStart = strstr(pszCloneWKT, SRS_PP_CENTRAL_MERIDIAN) ) )
        {
            strncpy( pszStart, "Central_Meridian", 
                                        strlen(SRS_PP_CENTRAL_MERIDIAN) );
        }

        if( ( pszStart = strstr(pszCloneWKT, SRS_PP_FALSE_EASTING) ) )
        {
            strncpy( pszStart, "False_Easting", strlen(SRS_PP_FALSE_EASTING) );
        }

        if( ( pszStart = strstr(pszCloneWKT, SRS_PP_FALSE_NORTHING) ) )
        {
            strncpy( pszStart, "False_Northing", 
                                        strlen(SRS_PP_FALSE_NORTHING) );
        }

        if( ( pszStart = strstr(pszCloneWKT, SRS_PP_LATITUDE_OF_CENTER) ) )
        {
            strncpy( pszStart, "Latitude_Of_Center", 
                                        strlen(SRS_PP_LATITUDE_OF_CENTER) );
        }
                
        if( ( pszStart = strstr(pszCloneWKT, SRS_PP_LATITUDE_OF_ORIGIN) ) )
        {
            strncpy( pszStart, "Latitude_Of_Origin", 
                                        strlen(SRS_PP_LATITUDE_OF_ORIGIN) );
        }
                
        if( ( pszStart = strstr(pszCloneWKT, SRS_PP_LONGITUDE_OF_CENTER) ) )
        {
            strncpy( pszStart, "Longitude_Of_Center", 
                                        strlen(SRS_PP_LONGITUDE_OF_CENTER) );
        }
                
        if( ( pszStart = strstr(pszCloneWKT, SRS_PP_PSEUDO_STD_PARALLEL_1) ) )
        {
            strncpy( pszStart, "Pseudo_Standard_Parallel_1", 
                                        strlen(SRS_PP_PSEUDO_STD_PARALLEL_1) );
        }
                
        if( ( pszStart = strstr(pszCloneWKT, SRS_PP_SCALE_FACTOR) ) )
        {
            strncpy( pszStart, "Scale_Factor", strlen(SRS_PP_SCALE_FACTOR) );
        }
                
        if( ( pszStart = strstr(pszCloneWKT, SRS_PP_STANDARD_PARALLEL_1) ) )
        {
            strncpy( pszStart, "Standard_Parallel_1", 
                                        strlen(SRS_PP_STANDARD_PARALLEL_1) );
        }
                
        if( ( pszStart = strstr(pszCloneWKT, SRS_PP_STANDARD_PARALLEL_2) ) )
        {
            strncpy( pszStart, "Standard_Parallel_2", 
                                        strlen(SRS_PP_STANDARD_PARALLEL_2) );
        }                
                
        if( ( pszStart = strstr(pszCloneWKT, SRS_PP_STANDARD_PARALLEL_2) ) )
        {
            strncpy( pszStart, "Standard_Parallel_2", 
                                        strlen(SRS_PP_STANDARD_PARALLEL_2) );
        }                
        
        // ----------------------------------------------------------------
        // Fix Unit name
        // ----------------------------------------------------------------
        
        if( ( pszStart = strstr(pszCloneWKT, "metre") ) )
        {
            strncpy( pszStart, SRS_UL_METER, strlen(SRS_UL_METER) );
        }
    }

    // --------------------------------------------------------------------
    // Tries to find a SRID compatible with the WKT
    // --------------------------------------------------------------------

    OWConnection* poConnection  = poGeoRaster->poConnection;
    OWStatement* poStmt = NULL;
    
    int nNewSRID = 0;    
   
    char *pszFuncName = "FIND_GEOG_CRS";
  
    if( poSRS2->IsProjected() )
    {
        pszFuncName = "FIND_PROJ_CRS";
    }
    
    poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  LIST SDO_SRID_LIST;"
        "BEGIN\n"
        "  SELECT SDO_CS.%s('%s', null) into LIST FROM DUAL;\n"
        "  IF LIST.COUNT() > 0 then\n"
        "    SELECT LIST(1) into :out from dual;\n"
        "  ELSE\n"
        "    SELECT 0 into :out from dual;\n"
        "  END IF;\n"
        "END;",
            pszFuncName,
            pszCloneWKT ) );
        
    poStmt->BindName( ":out", &nNewSRID );

    CPLPushErrorHandler( CPLQuietErrorHandler );

    if( poStmt->Execute() )
    {
        CPLPopErrorHandler();

        if ( nNewSRID > 0 )
        {
            poGeoRaster->SetGeoReference( nNewSRID );
            CPLFree( pszCloneWKT );       
            return CE_None;
        }
    }   

    // --------------------------------------------------------------------
    // Search by simplified WKT or insert it as a user defined SRS
    // --------------------------------------------------------------------
    
    int nCounter = 0;

    poStmt = poConnection->CreateStatement( CPLSPrintf(
        "SELECT COUNT(*) FROM MDSYS.CS_SRS WHERE WKTEXT = '%s'", pszCloneWKT));
    
    poStmt->Define( &nCounter );
            
    CPLPushErrorHandler( CPLQuietErrorHandler );

    if( poStmt->Execute() && nCounter > 0 )
    {    
        poStmt = poConnection->CreateStatement( CPLSPrintf(
            "SELECT SRID FROM MDSYS.CS_SRS WHERE WKTEXT = '%s'", pszCloneWKT));

        poStmt->Define( &nNewSRID );

        if( poStmt->Execute() )
        {
            CPLPopErrorHandler();
            
            poGeoRaster->SetGeoReference( nNewSRID );
            CPLFree( pszCloneWKT );
            return CE_None;
        }    
    }

    CPLPopErrorHandler();
    
    poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  MAX_SRID NUMBER := 0;\n"
        "BEGIN\n"
        "  SELECT MAX(SRID) INTO MAX_SRID FROM MDSYS.CS_SRS;\n"
        "  MAX_SRID := MAX_SRID + 1;\n"
        "  INSERT INTO MDSYS.CS_SRS (SRID, WKTEXT, CS_NAME)\n"
        "        VALUES (MAX_SRID, '%s', '%s');\n"
        "  SELECT MAX_SRID INTO :out FROM DUAL;\n"
        "END;",
            pszCloneWKT,
            oSRS.GetRoot()->GetChild(0)->GetValue() ) );

    poStmt->BindName( ":out", &nNewSRID );

    CPLErr eError = CE_None;

    CPLPushErrorHandler( CPLQuietErrorHandler );

    if( poStmt->Execute() )
    {
        CPLPopErrorHandler();
            
        poGeoRaster->SetGeoReference( nNewSRID );
    }
    else
    {
        CPLPopErrorHandler();
            
        poGeoRaster->SetGeoReference( UNKNOWN_CRS );

        CPLError( CE_Warning, CPLE_UserInterrupt,
            "Insufficient privileges to insert reference system to "
            "table MDSYS.CS_SRS." );
        
        eError = CE_Warning;
    }

    CPLFree( pszCloneWKT );
    
    return eError;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GeoRasterDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "SUBDATASETS", NULL);
}

//  ---------------------------------------------------------------------------
//                                                                GetMetadata()
//  ---------------------------------------------------------------------------

char **GeoRasterDataset::GetMetadata( const char *pszDomain )
{
    if( pszDomain != NULL && EQUALN( pszDomain, "SUBDATASETS", 11 ) )
        return papszSubdatasets;
    else
        return GDALDataset::GetMetadata( pszDomain );
}

//  ---------------------------------------------------------------------------
//                                                                     Delete()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::Delete( const char* pszFilename )
{
    (void) pszFilename;
/***
    GeoRasterDataset* poGRD = NULL;

    poGRD = (GeoRasterDataset*) GDALOpen( pszFilename, GA_Update );

    if( ! poGRD )
    {
        return CE_Failure;
    }

    if( ! poGRD->poGeoRaster->Delete() )
    {
        return CE_Failure;
    }
***/
    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                             SetSubdatasets()
//  ---------------------------------------------------------------------------

void GeoRasterDataset::SetSubdatasets( GeoRasterWrapper* poGRW )
{
    OWConnection* poConnection  = poGRW->poConnection;
    OWStatement* poStmt = NULL;

    //  -----------------------------------------------------------
    //  List all the GeoRaster Tables of that User/Database
    //  -----------------------------------------------------------

    if( poGRW->sTable.empty() &&
        poGRW->sColumn.empty() )
    {
        poStmt = poConnection->CreateStatement( 
            "SELECT   DISTINCT TABLE_NAME, OWNER FROM ALL_SDO_GEOR_SYSDATA\n"
            "  ORDER  BY TABLE_NAME ASC" );
        
        char szTable[OWNAME];
        char szOwner[OWNAME];

        poStmt->Define( szTable );
        poStmt->Define( szOwner );

        if( poStmt->Execute() )
        {
            int nCount = 1;

            do
            {
                papszSubdatasets = CSLSetNameValue( papszSubdatasets,
                    CPLSPrintf( "SUBDATASET_%d_NAME", nCount ),
                    CPLSPrintf( "geor:%s/%s@%s,%s.%s",
                        poConnection->GetUser(), poConnection->GetPassword(),
                        poConnection->GetServer(), szOwner, szTable ) );

                papszSubdatasets = CSLSetNameValue( papszSubdatasets,
                    CPLSPrintf( "SUBDATASET_%d_DESC", nCount ),
                    CPLSPrintf( "%s.Table=%s", szOwner, szTable ) );

                nCount++;
            }
            while( poStmt->Fetch() );
        }

        return;
    }

    //  -----------------------------------------------------------
    //  List all the GeoRaster Columns of that Table
    //  -----------------------------------------------------------

    if( ! poGRW->sTable.empty() &&
          poGRW->sColumn.empty() )
    {
        poStmt = poConnection->CreateStatement( CPLSPrintf(
            "SELECT   DISTINCT COLUMN_NAME, OWNER FROM ALL_SDO_GEOR_SYSDATA\n"
            "  WHERE  TABLE_NAME = UPPER('%s')\n"
            "  ORDER  BY COLUMN_NAME ASC",
                poGRW->sTable.c_str() ) );

        char szColumn[OWNAME];
        char szOwner[OWNAME];

        poStmt->Define( szColumn );
        poStmt->Define( szOwner );
        
        if( poStmt->Execute() )
        {
            int nCount = 1;

            do
            {
                papszSubdatasets = CSLSetNameValue( papszSubdatasets,
                    CPLSPrintf( "SUBDATASET_%d_NAME", nCount ),
                    CPLSPrintf( "geor:%s/%s@%s,%s.%s,%s",
                        poConnection->GetUser(), poConnection->GetPassword(),
                        poConnection->GetServer(), szOwner,
                        poGRW->sTable.c_str(), szColumn ) );

                papszSubdatasets = CSLSetNameValue( papszSubdatasets,
                    CPLSPrintf( "SUBDATASET_%d_DESC", nCount ),
                    CPLSPrintf( "Table=%s.%s Column=%s", szOwner,
                        poGRW->sTable.c_str(), szColumn ) );

                nCount++;
            }
            while( poStmt->Fetch() );
        }
        
        return;
    }

    //  -----------------------------------------------------------
    //  List all the rows that contains GeoRaster on Table/Column/Where
    //  -----------------------------------------------------------

    CPLString osAndWhere = "";

    if( ! poGRW->sWhere.empty() )
    {
        osAndWhere = CPLSPrintf( "AND %s", poGRW->sWhere.c_str() );
    }

    poStmt = poConnection->CreateStatement( CPLSPrintf(
        "SELECT T.%s.RASTERDATATABLE, T.%s.RASTERID, \n"
        "  extractValue(t.%s.metadata, "
"'/georasterMetadata/rasterInfo/dimensionSize[@type=\"ROW\"]/size','%s'),\n"
        "  extractValue(t.%s.metadata, "
"'/georasterMetadata/rasterInfo/dimensionSize[@type=\"COLUMN\"]/size','%s'),\n"
        "  extractValue(t.%s.metadata, "
"'/georasterMetadata/rasterInfo/dimensionSize[@type=\"BAND\"]/size','%s'),\n"
        "  extractValue(t.%s.metadata, "
"'/georasterMetadata/rasterInfo/cellDepth','%s'),\n"
        "  extractValue(t.%s.metadata, "
"'/georasterMetadata/spatialReferenceInfo/SRID','%s')\n"
        "  FROM   %s%s T\n"
        "  WHERE  %s IS NOT NULL %s\n"
        "  ORDER  BY T.%s.RASTERDATATABLE ASC,\n"
        "            T.%s.RASTERID ASC",
        poGRW->sColumn.c_str(), poGRW->sColumn.c_str(),
        poGRW->sColumn.c_str(), OW_XMLNS,
        poGRW->sColumn.c_str(), OW_XMLNS,
        poGRW->sColumn.c_str(), OW_XMLNS,
        poGRW->sColumn.c_str(), OW_XMLNS,
        poGRW->sColumn.c_str(), OW_XMLNS,
        poGRW->sSchema.c_str(), poGRW->sTable.c_str(),
        poGRW->sColumn.c_str(), osAndWhere.c_str(),
        poGRW->sColumn.c_str(), poGRW->sColumn.c_str() ) );

    char szDataTable[OWNAME];
    char szRasterId[OWNAME];
    char szRows[OWNAME];
    char szColumns[OWNAME];
    char szBands[OWNAME];
    char szCellDepth[OWNAME];
    char szSRID[OWNAME];

    poStmt->Define( szDataTable );
    poStmt->Define( szRasterId );
    poStmt->Define( szRows );
    poStmt->Define( szColumns );
    poStmt->Define( szBands );
    poStmt->Define( szCellDepth );
    poStmt->Define( szSRID );

    if( poStmt->Execute() )
    {
        int nCount = 1;

        do
        {
            papszSubdatasets = CSLSetNameValue( papszSubdatasets,
                CPLSPrintf( "SUBDATASET_%d_NAME", nCount ),
                CPLSPrintf( "geor:%s/%s@%s,%s,%s",
                    poConnection->GetUser(), poConnection->GetPassword(),
                    poConnection->GetServer(), szDataTable, szRasterId ) );

            const char* pszXBands = "";

            if( ! EQUAL( szBands, "" ) )
            {
                pszXBands = CPLSPrintf( "x%s", szBands );
            }

            papszSubdatasets = CSLSetNameValue( papszSubdatasets,
                CPLSPrintf( "SUBDATASET_%d_DESC", nCount ),
                CPLSPrintf( "[%sx%s%s] CellDepth=%s SRID=%s",
                    szRows, szColumns, pszXBands,
                    szCellDepth, szSRID ) );

            nCount++;
        }
        while( poStmt->Fetch() );
    }
}

//  ---------------------------------------------------------------------------
//                                                                    SetGCPs()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::SetGCPs( int, const GDAL_GCP *, const char * )
{
    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                           GetGCPProjection()
//  ---------------------------------------------------------------------------

const char* GeoRasterDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return pszProjection;
    else
        return "";
}

//  ---------------------------------------------------------------------------
//                                                            IBuildOverviews()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::IBuildOverviews( const char* pszResampling,
                                          int nOverviews,
                                          int* panOverviewList,
                                          int nListBands,
                                          int* panBandList,
                                          GDALProgressFunc pfnProgress,
                                          void* pProgressData )
{
    (void) panBandList;
    (void) nListBands;

    //  ---------------------------------------------------------------
    //  Can't update on read-only access mode
    //  ---------------------------------------------------------------

    if( GetAccess() != GA_Update )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Can't build overviews/pyramids on read-only access." );
        return CE_Failure;
    }

    //  ---------------------------------------------------------------
    //  Uses internal sdo_generatePyramid at PL/SQL?
    //  ---------------------------------------------------------------

    bool bInternal = true;

    const char *pszGEOR_INTERNAL_PYR = CPLGetConfigOption( "GEOR_INTERNAL_PYR",
        "YES" );

    if( EQUAL(pszGEOR_INTERNAL_PYR, "NO") )
    {
        bInternal = false;
    }
        
    //  -----------------------------------------------------------
    //  Pyramids applies to the whole dataset not to a specific band
    //  -----------------------------------------------------------

    if( nBands < GetRasterCount())
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid GeoRaster Pyramids band selection" );
        return CE_Failure;
    }

    //  ---------------------------------------------------------------
    //  Initialize progress reporting
    //  ---------------------------------------------------------------

    if( ! pfnProgress( 0.1, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

    //  ---------------------------------------------------------------
    //  Clear existing overviews
    //  ---------------------------------------------------------------

    if( nOverviews == 0 )
    {
        poGeoRaster->DeletePyramid();
        return CE_None;
    }

    //  -----------------------------------------------------------
    //  Pyramids levels can not be treated individually
    //  -----------------------------------------------------------

    if( nOverviews > 0 )
    {
        int i;
        for( i = 1; i < nOverviews; i++ )
        {
            //  -----------------------------------------------------------
            //  Power of 2, starting on 2, e.g. 2, 4, 8, 16, 32, 64, 128
            //  -----------------------------------------------------------

            if( panOverviewList[0] != 2 ||
              ( panOverviewList[i] != panOverviewList[i-1] * 2 ) )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid GeoRaster Pyramids levels." );        
                return CE_Failure;
            }
        }
    }

    //  -----------------------------------------------------------
    //  Re-sampling method: 
    //    NN, BILINEAR, AVERAGE4, AVERAGE16 and CUBIC
    //  -----------------------------------------------------------

    char szMethod[OWNAME];

    if( EQUAL( pszResampling, "NEAREST" ) )
    {
        strcpy( szMethod, "NN" );
    }
    else if( EQUALN( pszResampling, "AVERAGE", 7 ) )
    {
        strcpy( szMethod, "AVERAGE4" );
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Invalid resampling method" );
        return CE_Failure;
    }

    //  -----------------------------------------------------------
    //  Generate pyramids on poGeoRaster
    //  -----------------------------------------------------------

    if( ! poGeoRaster->GeneratePyramid( nOverviews, szMethod, bInternal ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Error generating pyramid" );
        return CE_Failure;
    }

    //  -----------------------------------------------------------
    //  If Pyramid was done internally on the server exit here
    //  -----------------------------------------------------------
    
    if( bInternal )
    {
        pfnProgress( 1 , NULL, pProgressData );
        return CE_None;
    }

    //  -----------------------------------------------------------
    //  Load the pyramids data using GDAL methods
    //  -----------------------------------------------------------

    CPLErr eErr = CE_None;

    int i = 0;

    for( i = 0; i < nBands; i++ )
    {
        GeoRasterRasterBand* poBand = (GeoRasterRasterBand*) papoBands[i];

        //  -------------------------------------------------------
        //  Clean up previous overviews
        //  -------------------------------------------------------

        int j = 0;

        if( poBand->nOverviewCount && poBand->papoOverviews )
        {
            for( j = 0; j < poBand->nOverviewCount; j++ )
            {
                delete poBand->papoOverviews[j];
            }
            CPLFree( poBand->papoOverviews );
        }

        //  -------------------------------------------------------
        //  Create new band's overviews list
        //  -------------------------------------------------------

        poBand->nOverviewCount = poGeoRaster->nPyramidMaxLevel;
        poBand->papoOverviews  = (GeoRasterRasterBand**) VSIMalloc(
                sizeof(GeoRasterRasterBand*) * poBand->nOverviewCount );

        for( j = 0; j < poBand->nOverviewCount; j++ )
        {
          poBand->papoOverviews[j] = new GeoRasterRasterBand(
                (GeoRasterDataset*) this, ( i + 1 ), ( j + 1 ) );
        }
    }

    //  -----------------------------------------------------------
    //  Load band's overviews
    //  -----------------------------------------------------------

    for( i = 0; i < nBands; i++ )
    {
        GeoRasterRasterBand* poBand = (GeoRasterRasterBand*) papoBands[i];

        void *pScaledProgressData = GDALCreateScaledProgress( 
            i / (double) nBands, ( i + 1) / (double) nBands, 
            pfnProgress, pProgressData );

        eErr = GDALRegenerateOverviews(
            (GDALRasterBandH) poBand,
            poBand->nOverviewCount,
            (GDALRasterBandH*) poBand->papoOverviews,
            pszResampling,
            GDALScaledProgress,
            pScaledProgressData );

        GDALDestroyScaledProgress( pScaledProgressData );
    }

    return eErr;
}

//  ---------------------------------------------------------------------------
//                                                             CreateMaskBand()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::CreateMaskBand( int nFlags )
{
    (void) nFlags;

    if( ! poGeoRaster->InitializeMask( DEFAULT_BMP_MASK,
            poGeoRaster->nRowBlockSize,
            poGeoRaster->nColumnBlockSize,
            poGeoRaster->nTotalRowBlocks,
            poGeoRaster->nTotalColumnBlocks,
            poGeoRaster->nTotalBandBlocks ) )
    {
        return CE_Failure;
    }
    
    poGeoRaster->bHasBitmapMask = true;

    return CE_None;
}

/*****************************************************************************/
/*                          GDALRegister_GEOR                                */
/*****************************************************************************/

void CPL_DLL GDALRegister_GEOR()
{
    GDALDriver* poDriver;

    if (! GDAL_CHECK_VERSION("GeoRaster driver"))
        return;

    if( GDALGetDriverByName( "GeoRaster" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription(  "GeoRaster" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Oracle Spatial GeoRaster" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_georaster.html" );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                                   "Byte UInt16 Int16 UInt32 Int32 Float32 "
                                   "Float64 CFloat32 CFloat64" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='DESCRIPTION' type='string' description='Table Description'/>"
"  <Option name='INSERT'      type='string' description='Column Values'/>"
"  <Option name='BLOCKXSIZE'  type='int'    description='Column Block Size' "
                                           "default='512'/>"
"  <Option name='BLOCKYSIZE'  type='int'    description='Row Block Size' "
                                           "default='512'/>"
"  <Option name='BLOCKBSIZE'  type='int'    description='Band Block Size'/>"
"  <Option name='BLOCKING'    type='string-select' default='YES'>"
"       <Value>YES</Value>"
"       <Value>NO</Value>"
"       <Value>OPTIMALPADDING</Value>"
"  </Option>"
"  <Option name='SRID'        type='int'    description='Overwrite EPSG code'/>"
"  <Option name='GENPYRAMID'  type='string-select' "
" description='Generate Pyramid, inform resampling method'>"
"       <Value>NN</Value>"
"       <Value>BILINEAR</Value>"
"       <Value>BIQUADRATIC</Value>"
"       <Value>CUBIC</Value>"
"       <Value>AVERAGE4</Value>"
"       <Value>AVERAGE16</Value>"
"  </Option>"
"  <Option name='GENPYRLEVELS'  type='int'  description='Number of pyramid level to generate'/>"
"  <Option name='OBJECTTABLE' type='boolean' "
                                           "description='Create RDT as object table'/>"
"  <Option name='SPATIALEXTENT' type='boolean' "
                                           "description='Generate Spatial Extent' "
                                           "default='TRUE'/>"
"  <Option name='EXTENTSRID'  type='int'    description='Spatial ExtentSRID code'/>"
"  <Option name='COORDLOCATION'    type='string-select' default='CENTER'>"
"       <Value>CENTER</Value>"
"       <Value>UPPERLEFT</Value>"
"  </Option>"
"  <Option name='VATNAME'     type='string' description='Value Attribute Table Name'/>"
"  <Option name='NBITS'       type='int'    description='BITS for sub-byte "
                                           "data types (1,2,4) bits'/>"
"  <Option name='INTERLEAVE'  type='string-select'>"
"       <Value>BSQ</Value>"
"       <Value>BIP</Value>"
"       <Value>BIL</Value>"
"   </Option>"
"  <Option name='COMPRESS'    type='string-select'>"
"       <Value>NONE</Value>"
"       <Value>JPEG-B</Value>"
"       <Value>JPEG-F</Value>"
"       <Value>DEFLATE</Value>"
"  </Option>"
"  <Option name='QUALITY'     type='int'    description='JPEG quality 0..100' "
                                           "default='75'/>"
"</CreationOptionList>" );

        poDriver->pfnOpen       = GeoRasterDataset::Open;
        poDriver->pfnCreate     = GeoRasterDataset::Create;
        poDriver->pfnCreateCopy = GeoRasterDataset::CreateCopy;
        poDriver->pfnIdentify   = GeoRasterDataset::Identify;
        poDriver->pfnDelete     = GeoRasterDataset::Delete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
