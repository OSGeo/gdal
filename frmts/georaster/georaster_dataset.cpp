/******************************************************************************
 * $Id: $
 *
 * Name:     georaster_dataset.cpp
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Implement GeoRasterDataset Methods
 * Author:   Ivan Lucena [ivan.lucena@pmldnet.com]
 *
 ******************************************************************************
 * Copyright (c) 2008, Ivan Lucena
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

#include "georaster_priv.h"

CPL_C_START
void CPL_DLL GDALRegister_GEOR(void);
CPL_C_END

bool    OWIsNumeric( const char *pszText );

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
    poDriver            = (GDALDriver *) GDALGetDriverByName( "GEORASTER" );
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

    //  -------------------------------------------------------------------
    //  Parse arguments
    //  -------------------------------------------------------------------

    char** papszParam = GeoRasterWrapper::ParseIdentificator( 
        poOpenInfo->pszFilename );

    int nArgc = CSLCount( papszParam );

    //  -------------------------------------------------------------------
    //  Check mandatory arguments
    //  -------------------------------------------------------------------

    if ( nArgc < 2 ||
         nArgc > 6 ||
         EQUAL( papszParam[0], "" ) ||
         EQUAL( papszParam[1], "" ) )
    {
        CPLError( CE_Warning, CPLE_IllegalArg,
        "Invalid georaster identification\n"
        "Usage:\n"
        "    {georaster/geor}:<user>{,/}<pwd>{,@}[db],[table],[column],[where]\n"
        "    {georaster/geor}:<user>{,/}<pwd>{,@}[db],<rdt>:<rid>\n"
        "    user   - user's login\n"
        "    pwd    - user's password\n"
        "    db     - connection string ( default is $ORACLE_SID )\n"
        "    table  - name of a georaster table\n"
        "    column - name of a georaster column\n"
        "    where  - simple where clause\n"
        "    rdt    - raster data table name\n"
        "    rid    - georaster numeric identification\n"
        "Examples:\n"
        "    geor:scott/tiger@demodb,table,column,id=1\n"
        "    geor:scott/tiger@server.company.com:1521/survey,table,column,id=1\n"
        "    \"georaster:scott,tiger,demodb,table,column,city='london'\"\n"
        "    georaster:scott,tiger,,rdt_10$,10\n" );
        CSLDestroy( papszParam );
        return false;
    }
    CSLDestroy( papszParam );
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

    GeoRasterWrapper* poGRW = GeoRasterWrapper::Open(poOpenInfo->pszFilename);

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

    poGRD->eAccess       = poOpenInfo->eAccess;
    poGRD->poGeoRaster   = poGRW;

    //  -------------------------------------------------------------------
    //  Assign GeoRaster information
    //  -------------------------------------------------------------------

    if( poGRW->nRasterRows && poGRW->nRasterColumns )
    {
        poGRD->nRasterXSize  = poGRW->nRasterColumns;
        poGRD->nRasterYSize  = poGRW->nRasterRows;
        poGRD->nBands        = poGRW->nRasterBands;
        poGRD->poGeoRaster   = poGRW;
        
        if( poGRW->bIsReferenced )
        {
            poGRD->bGeoTransform = poGRW->GetImageExtent( poGRD->adfGeoTransform );
        }
    }
    else
    {
        poGRD->SetSubdatasets( poGRW );

        if( CSLCount( poGRD->papszSubdatasets ) == 0 &&
            poGRD->eAccess == GA_ReadOnly )
        {
            delete poGRD;
            poGRD = NULL;
        }

        return (GDALDataset*) poGRD;
    }

    //  -------------------------------------------------------------------
    //  Load mask band
    //  -------------------------------------------------------------------

    poGRW->bHasBitmapMask = EQUAL( "TRUE", CPLGetXMLValue( poGRW->phMetadata,
                          "layerInfo.objectLayer.bitmapMask", "FALSE" ) );

    if( poGRW->bHasBitmapMask )
    {
        poGRD->poMaskBand = 
                new GeoRasterRasterBand( poGRD, 0, DEFAULT_BMP_MASK );
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

    if( EQUAL( poGRW->szInterleaving, "BSQ" ) )
    {
        poGRD->SetMetadataItem( "INTERLEAVE", "BAND", "IMAGE_STRUCTURE" );
    }
    else if( EQUAL( poGRW->szInterleaving, "BIP" ) )
    {
        poGRD->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    }
    else if( EQUAL( poGRW->szInterleaving, "BIL" ) )
    {
        poGRD->SetMetadataItem( "INTERLEAVE", "LINE", "IMAGE_STRUCTURE" );
    }

    poGRD->SetMetadataItem( "COMPRESSION", CPLGetXMLValue( poGRW->phMetadata,
        "rasterInfo.compression.type", "NONE" ), "IMAGE_STRUCTURE" );

    if( EQUALN( poGRW->pszCompressionType, "JPEG", 4 ) )
    {
        poGRD->SetMetadataItem( "COMPRESS_QUALITY", 
            CPLGetXMLValue( poGRW->phMetadata,
            "rasterInfo.compression.quality", "0" ), "IMAGE_STRUCTURE" );
    }

    if( EQUAL( poGRW->pszCellDepth, "1BIT" ) )
    {
        poGRD->SetMetadataItem( "NBITS", "1", "IMAGE_STRUCTURE" );
    }

    if( EQUAL( poGRW->pszCellDepth, "2BIT" ) )
    {
        poGRD->SetMetadataItem( "NBITS", "2", "IMAGE_STRUCTURE" );
    }

    if( EQUAL( poGRW->pszCellDepth, "4BIT" ) )
    {
        poGRD->SetMetadataItem( "NBITS", "4", "IMAGE_STRUCTURE" );
    }

    //  -------------------------------------------------------------------
    //  Set Metadata on "ORACLE" domain
    //  -------------------------------------------------------------------

    char* pszDoc = CPLSerializeXMLTree( poGRW->phMetadata );

    poGRD->SetMetadataItem( "TABLE_NAME", CPLSPrintf( "%s%s",
                            poGRW->pszSchema, poGRW->pszTable),    "ORACLE" );
    poGRD->SetMetadataItem( "COLUMN_NAME",    poGRW->pszColumn,    "ORACLE" );
    poGRD->SetMetadataItem( "RDT_TABLE_NAME", poGRW->pszDataTable, "ORACLE" );
    poGRD->SetMetadataItem( "RASTER_ID", CPLSPrintf( "%d", 
                            poGRW->nRasterId ),                    "ORACLE" );
    poGRD->SetMetadataItem( "METADATA",       pszDoc,              "ORACLE" );

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
    //  Is it trying to overwrite an existing GeoRaster?
    //  -------------------------------------------------------------------

    bool bOverwrite = ( poGRW->nRasterRows && poGRW->nRasterColumns );

    //  -------------------------------------------------------------------
    //  Set basic information and default values
    //  -------------------------------------------------------------------

    poGRW->nRasterColumns   = nXSize;
    poGRW->nRasterRows      = nYSize;
    poGRW->nRasterBands     = nBands;
    poGRW->pszCellDepth     = CPLStrdup( pszCellDepth );
    poGRW->nColumnBlockSize = 256;
    poGRW->nRowBlockSize    = 256;
    poGRW->nBandBlockSize   = 1;

    if( bOverwrite )
    {
        poGRW->PrepareToOverwrite();
    }

    //  -------------------------------------------------------------------
    //  Check the create options to use in initialization
    //  -------------------------------------------------------------------

    const char* pszFetched  = "";
    char* pszDescription    = NULL;
    char* pszInsert         = NULL;
    const char* pszCompress = "";
    int   nQuality          = 75;

    if( poGRW->pszTable )
    {
        pszFetched = CSLFetchNameValue( papszOptions, "DESCRIPTION" );

        if( pszFetched )
        {
            pszDescription  = CPLStrdup( pszFetched );
        }
    }

    if( ! poGRW->pszTable )
    {
        poGRW->pszTable     = CPLStrdup( "GDAL_IMPORT" );
        poGRW->pszDataTable = CPLStrdup( "GDAL_RDT" );
    }

    if( ! poGRW->pszColumn )
    {
        poGRW->pszColumn    = CPLStrdup( "RASTER" );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "INSERT" );

    if( pszFetched )
    {
        pszInsert           = CPLStrdup( pszFetched );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "BLOCKXSIZE" );

    if( pszFetched )
    {
        poGRW->nColumnBlockSize = atoi( pszFetched );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "BLOCKYSIZE" );

    if( pszFetched )
    {
        poGRW->nRowBlockSize    = atoi( pszFetched );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "NBITS" );

    if( pszFetched != NULL )
    {
        poGRW->pszCellDepth = CPLStrdup( 
            CPLSPrintf( "%dBIT", atoi( pszFetched ) ) );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "COMPRESS" );

    if( pszFetched != NULL &&
      ( EQUALN( pszFetched, "JPEG", 4 ) ||
        EQUAL( pszFetched, "DEFLATE" ) ) )
    {
        poGRW->pszCompressionType = CPLStrdup( pszFetched );
        pszCompress = CPLStrdup( pszFetched );
    }
    else
    {
        poGRW->pszCompressionType = CPLStrdup( "NONE" );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "QUALITY" );

    if( pszFetched )
    {
        poGRW->nCompressQuality = atoi( pszFetched );
        nQuality = poGRW->nCompressQuality;
    }

    pszFetched = CSLFetchNameValue( papszOptions, "INTERLEAVE" );

    if( pszFetched )
    {
        if( EQUAL( pszFetched, "BAND" ) ||  EQUAL( pszFetched, "BSQ" ) )
        {
            strcpy( poGRW->szInterleaving, "BSQ" );
        }
        if( EQUAL( pszFetched, "LINE" ) ||  EQUAL( pszFetched, "BIL" ) )
        {
            strcpy( poGRW->szInterleaving, "BIL" );
        }
        if( EQUAL( pszFetched, "PIXEL" ) ||  EQUAL( pszFetched, "BIP" ) )
        {
            strcpy( poGRW->szInterleaving, "BIP" );
        }
    }
    else
    {
        if( EQUAL( poGRW->pszCompressionType, "NONE" ) == false )
        {
            strcpy( poGRW->szInterleaving, "BIP" );
        }
    }

    pszFetched = CSLFetchNameValue( papszOptions, "BLOCKBSIZE" );

    if( pszFetched )
    {
        poGRW->nBandBlockSize   = atoi( pszFetched );
    }
    else
    {
        if( EQUAL( poGRW->pszCompressionType, "NONE" ) == false &&
          ( nBands == 3 || nBands == 4 ) )
        {
            poGRW->nBandBlockSize = nBands;
        }
    }

    //  -------------------------------------------------------------------
    //  Validate options
    //  -------------------------------------------------------------------

    if( pszDescription && bOverwrite )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, 
            "Cannot use DESCRIPTION on a existing GeoRaster" );
        delete poGRD;
        return NULL;
    }

    if( pszInsert && bOverwrite )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, 
            "Cannot use INSERT on a existing GeoRaster" );
        delete poGRD;
        return NULL;
    }

    if( EQUALN( poGRW->pszCompressionType, "JPEG", 4 ) )
    {
        if( ! eType == GDT_Byte )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, 
                "For (COMPRESS=%s) data type must be Byte. ",
                poGRW->pszCompressionType );
            delete poGRD;
            return NULL;
        }

        if( poGRW->nBandBlockSize != 1 && 
          ( poGRW->nBandBlockSize != poGRW->nRasterBands ) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, 
                "For (COMPRESS=%s) BLOCKBSIZE must be equal to 1 or to "
                "the exact number of bands (%d).",
                poGRW->pszCompressionType,
                poGRW->nRasterBands );
            delete poGRD;
            return NULL;
        }

        if( poGRW->nBandBlockSize != 1 && 
            EQUAL( poGRW->szInterleaving, "BIP" ) == false )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, 
                "For (COMPRESS=%s) and (INTERLEAVE=%s) BLOCKBSIZE must be 1 ",
                poGRW->pszCompressionType,
                poGRW->szInterleaving );
            delete poGRD;
            return NULL;
        }

        if( ( poGRW->nColumnBlockSize * 
              poGRW->nRowBlockSize *
              poGRW->nBandBlockSize *
              ( GDALGetDataTypeSize( eType ) / 8 ) ) > ( 50 * 1024 * 1024 ) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, 
                "For (COMPRESS=%s) each data block must not exceed 50Mb. "
                "Consider reducing BLOCK{X,Y,B}XSIZE.",
                poGRW->pszCompressionType );
            delete poGRD;
            return NULL;
        }
    }

    if( EQUALN( poGRW->pszCompressionType, "DEFLATE", 4 ) )
    {
        if( poGRW->nBandBlockSize != 1 && 
            EQUAL( poGRW->szInterleaving, "BIP" ) == false )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, 
                "(COMPRESS=%s) and BLOCKBSIZE > 1 must select "
                "INTERLEAVE as PIXEL (BIP).",
                poGRW->pszCompressionType );
            delete poGRD;
            return NULL;
        }

        if( ( poGRW->nColumnBlockSize * 
              poGRW->nRowBlockSize *
              poGRW->nBandBlockSize *
              ( GDALGetDataTypeSize( eType ) / 8 ) ) > ( 1024 * 1024 * 1024 ) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, 
                "For (COMPRESS=%s) each data block must not exceed 1Gb. "
                "Consider reducing BLOCK{X,Y,B}XSIZE.",
                poGRW->pszCompressionType );
            delete poGRD;
            return NULL;
        }
    }

    //  -------------------------------------------------------------------
    //  Create a SDO_GEORASTER object on the server
    //  -------------------------------------------------------------------

    bool bSucced = poGRW->Create( pszDescription, pszInsert, bOverwrite );

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
        poGRW->pszDataTable,
        poGRW->nRasterId ) );

    //  -------------------------------------------------------------------
    //  Load the new Dataset
    //  -------------------------------------------------------------------

    delete poGRD;

    poGRD = (GeoRasterDataset*) GDALOpen( szStringId, GA_Update );

    //  -------------------------------------------------------------------
    //  Load aditional options
    //  -------------------------------------------------------------------

    pszFetched = CSLFetchNameValue( papszOptions, "SRID" );

    if( pszFetched )
    {
        poGRD->bForcedSRID = true; /* ignore others methods */
        poGRD->poGeoRaster->SetGeoReference( atoi( pszFetched ) );
    }

    if( ! EQUAL( pszCompress, "" ) )
    {
        poGRD->poGeoRaster->SetCompression( pszCompress, nQuality );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "COORDLOCATION" );

    if( pszFetched )
    {
        if( EQUAL( pszFetched, "CENTER" ) )
        {
            poGRD->poGeoRaster->eForceCoordLocation = MCL_CENTER;
        }
        else if( EQUAL( pszFetched, "UPPERLEFT" ) )
        {
            poGRD->poGeoRaster->eForceCoordLocation = MCL_UPPERLEFT;
        }
        else 
        {
            CPLError( CE_Warning, CPLE_IllegalArg, 
                "Incorrect COORDLOCATION (%s)", pszFetched );
        }
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

    GeoRasterDataset *poGRD;

    poGRD = (GeoRasterDataset *) GeoRasterDataset::Create( pszFilename,
        poSrcDS->GetRasterXSize(),
        poSrcDS->GetRasterYSize(),
        poSrcDS->GetRasterCount(),
        eType, papszOptions );

    if( poGRD == NULL )
    {
        return NULL;
    }

    //  -----------------------------------------------------------
    //  Copy information to the dataset
    //  -----------------------------------------------------------

    double adfTransform[6];

    poSrcDS->GetGeoTransform( adfTransform );

    poGRD->SetGeoTransform( adfTransform );

    if( ! poGRD->bForcedSRID ) /* forced by create option SRID */
    {
        poGRD->SetProjection( poSrcDS->GetProjectionRef() );
    }

    // --------------------------------------------------------------------
    //      Copy information to the raster bands
    // --------------------------------------------------------------------

    int    bHasNoDataValue = FALSE;
    double dfNoDataValue = 0.0;
    double dfMin, dfMax, dfStdDev, dfMean;
    int    iBand = 0;

    for( iBand = 1; iBand <= poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand* poSrcBand = poSrcDS->GetRasterBand( iBand );
        GeoRasterRasterBand* poDstBand = (GeoRasterRasterBand*) 
                                    poGRD->GetRasterBand( iBand );
        GDALColorTable* poColorTable = poSrcBand->GetColorTable(); 

        if( poColorTable )
        {
            poDstBand->SetColorTable( poColorTable );
        }

        if( poDstBand->GetStatistics( false, false, &dfMin, &dfMax,
            &dfStdDev, &dfMean ) == CE_None )
        {
            poDstBand->SetStatistics( dfMin, dfMax, dfStdDev, dfMean );
        }

        const GDALRasterAttributeTable *poRAT = poSrcBand->GetDefaultRAT();

        if( poRAT != NULL )
        {
            poDstBand->SetDefaultRAT( poRAT );
        }

        dfNoDataValue = poSrcBand->GetNoDataValue( &bHasNoDataValue );

        if( bHasNoDataValue )
        {
            poDstBand->SetNoDataValue( dfNoDataValue );
        }
    }

    // --------------------------------------------------------------------
    //  Copy actual imagery.
    // --------------------------------------------------------------------

    int nXSize = poGRD->GetRasterXSize();
    int nYSize = poGRD->GetRasterYSize();

    int nBlockXSize = 0;
    int nBlockYSize = 0;

    poGRD->GetRasterBand( 1 )->GetBlockSize( &nBlockXSize, &nBlockYSize );

    void *pData = VSIMalloc( nBlockXSize * nBlockYSize *
        GDALGetDataTypeSize( eType ) / 8 );

    if( pData == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
            "GeoRaster::CreateCopy : Out of memory " );
        delete poGRD;
        return NULL;
    }

    int iYOffset = 0;
    int iXOffset = 0;
    int iXBlock  = 0;
    int iYBlock  = 0;
    int nBlockCols = 0;
    int nBlockRows = 0;
    CPLErr eErr = CE_None;

    poGRD->poGeoRaster->SetOrdelyAccess( true );

    int nPixelSize = GDALGetDataTypeSize( 
        poSrcDS->GetRasterBand(1)->GetRasterDataType() ) / 8;

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
                GDALRasterBand *poDstBand = poGRD->GetRasterBand( iBand );

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
            ( iYOffset + nBlockRows ) / (double) nYSize, NULL, pProgressData ) ) )
        {
            eErr = CE_Failure;
            CPLError( CE_Failure, CPLE_UserInterrupt,
                "User terminated CreateCopy()" );
        }
    }

    CPLFree( pData );

    // --------------------------------------------------------------------
    //      Finalize
    // --------------------------------------------------------------------

    poGRD->FlushCache();

    if( pfnProgress )
    {
        printf( "Ouput dataset: (georaster:%s,%s,%s,%s,%d) on %s%s,%s\n",
            poGRD->poGeoRaster->poConnection->GetUser(),
            poGRD->poGeoRaster->poConnection->GetPassword(),
            poGRD->poGeoRaster->poConnection->GetServer(),
            poGRD->poGeoRaster->pszDataTable,
            poGRD->poGeoRaster->nRasterId,
            poGRD->poGeoRaster->pszSchema,
            poGRD->poGeoRaster->pszTable,
            poGRD->poGeoRaster->pszColumn );
    }

    return poGRD;
}

//  ---------------------------------------------------------------------------
//                                                            GetGeoTransform()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::GetGeoTransform( double *padfTransform )
{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    
    if( bGeoTransform )
    {
        return CE_None;
    }

    if( ! poGeoRaster->bIsReferenced )
    {
        return CE_Failure;
    }

    if( ! poGeoRaster->GetImageExtent( adfGeoTransform ) )
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
    if( ! poGeoRaster->bIsReferenced )
    {
        return "";
    }

    if( poGeoRaster->nSRID == UNKNOWN_CRS )
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

    if( oSRS.importFromEPSG( poGeoRaster->nSRID ) == OGRERR_NONE )
    {
        /*
         * Ignores the WKT from Oracle and use the one from GDAL's
         * EPSG tables. That would ensure that other drivers/software
         * will recognizize the parameters.
         */

        if( oSRS.exportToWkt( &pszProjection ) == OGRERR_NONE )
        {
            return pszProjection;
        }

        return "";
    }

    // --------------------------------------------------------------------
    // Get the WKT from the server
    // --------------------------------------------------------------------

    char* pszWKText = CPLStrdup(
        poGeoRaster->GetWKText( poGeoRaster->nSRID ) );

    if( ! ( oSRS.importFromWkt( &pszWKText ) == OGRERR_NONE && oSRS.GetRoot() ) )
    {
        return "";
    }

    // ----------------------------------------------------------------
    // Decorate with EPSG Authority codes
    // ----------------------------------------------------------------

    oSRS.SetAuthority( oSRS.GetRoot()->GetValue(), "EPSG", poGeoRaster->nSRID );

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
            //?? One ot two parameters?
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
//                                                                  IRasterIO()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::IRasterIO( GDALRWFlag eRWFlag,
                                    int nXOff, int nYOff, int nXSize, int nYSize,
                                    void *pData, int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType,
                                    int nBandCount, int *panBandMap,
                                    int nPixelSpace, int nLineSpace, int nBandSpace )

{
    if( nBandCount > 1 )
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

    CPLDebug("GEOR", " SetProjection(%s)", pszWKT);

    OGRErr eOGRErr = oSRS.importFromWkt( &pszWKT );

    if( eOGRErr != OGRERR_NONE )
    {
        CPLDebug("GEOR", "Not recongnized");

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
        if( EQUAL( pszAuthName, "Oracle" ) || 
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
        poSRS2->SetAngularUnits( "Decimal Degree", 0.0174532925199433 );
    }

    char* pszCloneWKT = NULL;

    if( poSRS2->exportToWkt( &pszCloneWKT ) != OGRERR_NONE )
    {
        delete poSRS2;
        return CE_Failure;
    }

    //TODO: Try to find a correspondent WKT on the server

    CPLFree( pszCloneWKT );

    delete poSRS2;

    return CE_Failure;
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

//  ----------------------------------------------------------------------------
//                                                                      Delete()
//  ----------------------------------------------------------------------------

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

    if( ! poGRD->poGeoRaster->pszWhere )
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
    if( eAccess == GA_Update )
    {
        return;
    }

    OWConnection* poConnection  = poGRW->poConnection;
    OWStatement*  poStmt        = NULL;

    const char* papszToken[12];
    papszToken[0] = poConnection->GetUser();
    papszToken[1] = poConnection->GetPassword();
    papszToken[2] = poConnection->GetServer();
    papszToken[3] = papszToken[4] = papszToken[5] = 
                    papszToken[6] = papszToken[7] = 
                    papszToken[8] = papszToken[9] = 
                    papszToken[10] = papszToken[11] = "";

    char szTable[OWNAME]      = "";
    char szColumn[OWNAME]     = "";
    char szDataTable[OWNAME]  = "";
    char szRasterId[OWNAME]   = "";
    char szSchema[OWNAME]     = "";

    char szRows[OWNAME]       = "";
    char szColumns[OWNAME]    = "";
    char szBands[OWNAME]      = "";
    char szCellDepth[OWNAME]  = "";

    bool bDimAndDataType      = false;

    //  -----------------------------------------------------------
    //  Resolve who is the schema owner
    //  -----------------------------------------------------------

    if( poGRW->pszSchema != NULL && strlen( poGRW->pszSchema ) > 0 )
    {
        strcpy( szSchema, poGRW->pszSchema );
    }

    //  -----------------------------------------------------------
    //  List all the GeoRaster Tables of that User/Database
    //  -----------------------------------------------------------

    if( poGRW->pszTable  == NULL &&
        poGRW->pszColumn == NULL )
    {
        poStmt = poConnection->CreateStatement(
            "SELECT DISTINCT TABLE_NAME FROM ALL_SDO_GEOR_SYSDATA\n"
            "  WHERE OWNER = UPPER(:1)\n"
            "  ORDER  BY TABLE_NAME ASC" );

        poStmt->Bind( poGRW->pszOwner );
        poStmt->Define( szTable );

        papszToken[3] = szSchema;
        papszToken[4] = szTable;
        papszToken[7] = "Table:";
        papszToken[8] = szSchema;
        papszToken[9] = szTable;
    }

    //  -----------------------------------------------------------
    //  List all the GeoRaster Columns of that Table
    //  -----------------------------------------------------------

    if( poGRW->pszTable  != NULL && 
        poGRW->pszColumn == NULL )
    {
        poStmt = poConnection->CreateStatement(
            "SELECT DISTINCT COLUMN_NAME FROM ALL_SDO_GEOR_SYSDATA\n"
            "  WHERE OWNER = UPPER(:1) AND TABLE_NAME = UPPER(:2)\n"
            "  ORDER  BY COLUMN_NAME ASC" );

        poStmt->Bind( poGRW->pszOwner );
        poStmt->Bind( poGRW->pszTable );
        poStmt->Define( szColumn );

        papszToken[3] = szSchema;
        papszToken[4] = poGRW->pszTable;
        papszToken[5] = ",";
        papszToken[6] = szColumn;
        papszToken[7] = "Column:";
        papszToken[8] = szColumn;
    }

    //  -----------------------------------------------------------
    //  List all the rows that contains GeoRaster on Table/Column/Where
    //  -----------------------------------------------------------

    if( poGRW->pszTable  != NULL && 
        poGRW->pszColumn != NULL )
    {
        bDimAndDataType = true;

        CPLString osAndWhere = "";

        if( poGRW->pszWhere )
        {
            osAndWhere = CPLSPrintf( "AND %s", poGRW->pszWhere );
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
"'/georasterMetadata/rasterInfo/cellDepth','%s')\n"
            "  FROM   %s%s T\n"
            "  WHERE  %s IS NOT NULL %s\n"
            "  ORDER  BY T.%s.RASTERDATATABLE ASC,\n"
            "            T.%s.RASTERID ASC",
            poGRW->pszColumn, poGRW->pszColumn,
            poGRW->pszColumn, OW_XMLNS,
            poGRW->pszColumn, OW_XMLNS,
            poGRW->pszColumn, OW_XMLNS,
            poGRW->pszColumn, OW_XMLNS,
            poGRW->pszSchema, poGRW->pszTable,
            poGRW->pszColumn, osAndWhere.c_str(),
            poGRW->pszColumn, poGRW->pszColumn ) );

        poStmt->Define( szDataTable );
        poStmt->Define( szRasterId );
        poStmt->Define( szRows );
        poStmt->Define( szColumns );
        poStmt->Define( szBands );
        poStmt->Define( szCellDepth );

        papszToken[3] = szSchema;
        papszToken[4] = szDataTable;
        papszToken[5] = ",";
        papszToken[6] = szRasterId;
        papszToken[7] = "DataTable:";
        papszToken[8] = szSchema;
        papszToken[9] = szDataTable;
        papszToken[10] = "RasterId:";
        papszToken[11] = szRasterId;
    }

    if( poStmt->Execute() == false )
    {
        return;
    }

    //  -----------------------------------------------------------
    //  Format subdataset list
    //  -----------------------------------------------------------

    int nCount = 1;

    CPLString osName;
    CPLString osValue;
    CPLString osDimen = "";
    CPLString osDType = "";
    
    while( poStmt->Fetch() )
    {
        osName  = CPLSPrintf( "SUBDATASET_%d_NAME", nCount );
        osValue = CPLSPrintf( "geor:%s/%s@%s,%s%s%s%s",
            papszToken[0], papszToken[1], papszToken[2], papszToken[3],
            papszToken[4], papszToken[5], papszToken[6] );

        papszSubdatasets = CSLSetNameValue( papszSubdatasets,
            osName.c_str(), osValue.c_str() );

        if( bDimAndDataType )
        {
            if( EQUAL( szBands, "" ) )
            {
                osDimen = CPLSPrintf( "[%sx%s] ", szRows, szColumns );
            }
            else
            {
                osDimen = CPLSPrintf( "[%sx%sx%s] ", szRows, szColumns, szBands );
            }
            osDType = CPLSPrintf( " (%s = %s)", szCellDepth,
                    GDALGetDataTypeName( OWGetDataType( szCellDepth ) ) );
        }

        osName  = CPLSPrintf( "SUBDATASET_%d_DESC", nCount );
        osValue = CPLSPrintf( "%s%s%s%s %s%s%s",
            osDimen.c_str(),
            papszToken[7], papszToken[8], papszToken[9], papszToken[10],
            papszToken[11],
            osDType.c_str());

        papszSubdatasets = CSLSetNameValue( papszSubdatasets,
            osName.c_str(), osValue.c_str() );

        nCount++;
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

    char szMethod[OWNAME];

    //  -----------------------------------------------------------
    //  Pyramids applies to the whole dataset not to a specific band
    //  -----------------------------------------------------------

    if( nBands < GetRasterCount())
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid GeoRaster Pyramids band selection" );
        return CE_Failure;
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
    //  Re-sampling method: NN, BILINEAR, AVERAGE4, AVERAGE16 and CUBIC
    //  -----------------------------------------------------------

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
    //  There is no progress report callback from PL/SQL statement
    //  -----------------------------------------------------------

    pfnProgress( 10.0 / 100.0 , NULL, pProgressData );

    //  -----------------------------------------------------------
    //  Generate Pyramids based on SDO_GEOR.generatePyramids()
    //  -----------------------------------------------------------

    if( ! this->poGeoRaster->GeneratePyramid( nOverviews, szMethod ) )
    {
        return CE_Failure;
    }

    pfnProgress( 100 / 100.0 , NULL, pProgressData );

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                             CreateMaskBand()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::CreateMaskBand( int nFlags )
{
    if( ! poGeoRaster->InitializePyramidLevel( DEFAULT_BMP_MASK,
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
    GeoRasterDriver* poDriver;

    if (! GDAL_CHECK_VERSION("GeoRaster driver"))
        return;

    if( GDALGetDriverByName( "GeoRaster" ) == NULL )
    {
        poDriver = new GeoRasterDriver();

        poDriver->SetDescription(  "GeoRaster" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Oracle Spatial GeoRaster" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_georaster.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                                   "Byte UInt16 Int16 UInt32 Int32 Float32 "
                                   "Float64 CFloat32 CFloat64" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='DESCRIPTION' type='string' description='Table Description' "
                                           "default='(RASTER MDSYS.SDO_GEORASTER)'/>"
"  <Option name='INSERT'      type='string' description='Column Values' "
                                           "default='(SDO_GEOR.INIT())'/>"
"  <Option name='BLOCKXSIZE'  type='int'    description='Column Block Size' "
                                           "default='256'/>"
"  <Option name='BLOCKYSIZE'  type='int'    description='Row Block Size' "
                                           "default='256'/>"
"  <Option name='BLOCKBSIZE'  type='int'    description='Band Block Size' "
                                           "default='1'/>"
"  <Option name='SRID'        type='int'    description='Overwrite EPSG code' "
                                           "default='0'/>"
"  <Option name='NBITS'       type='int'    description='BITS for sub-byte "
                                           "data types (1,2,4) bits'/>"
"  <Option name='INTERLEAVE'  type='string-select' default='BAND'>"
"       <Value>BAND</Value>"
"       <Value>PIXEL</Value>"
"       <Value>LINE</Value>"
"       <Value>BSQ</Value>"
"       <Value>BIP</Value>"
"       <Value>BIL</Value>"
"   </Option>"
"  <Option name='COMPRESS'    type='string-select' default='NONE'>"
"       <Value>NONE</Value>"
"       <Value>JPEG-B</Value>"
"       <Value>JPEG-F</Value>"
"       <Value>DEFLATE</Value>"
"  </Option>"
"  <Option name='COORDLOCATION'    type='string-select' default='CENTER'>"
"       <Value>CENTER</Value>"
"       <Value>UPPERLEFT</Value>"
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
