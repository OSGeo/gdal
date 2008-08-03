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
    poGeoRaster         = NULL;
    papszSubdatasets    = NULL;
    adfGeoTransform[0]  = 0.0;
    adfGeoTransform[1]  = 1.0;
    adfGeoTransform[2]  = 0.0;
    adfGeoTransform[3]  = 0.0;
    adfGeoTransform[4]  = 0.0;
    adfGeoTransform[5]  = 1.0;
    pszSpatialRef       = NULL;
    poDriver            = (GDALDriver *) GDALGetDriverByName( "GEORASTER" );
}

//  ---------------------------------------------------------------------------
//                                                          ~GeoRasterDataset()
//  ---------------------------------------------------------------------------

GeoRasterDataset::~GeoRasterDataset()
{
    ObjFree_nt( poGeoRaster );
    CSLFree_nt( papszSubdatasets );
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

    char **papszParam = CSLTokenizeString2( 
                            strstr( pszFilename, ":" ) + 1, 
                            ID_SEPARATORS, 
                            CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS );

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
        "Invalid georaster identification\n\n"
        "Usage:\n\n"
        "    georaster:<user>,<pwd>,[db],[table],[column],[where]\n"
        "    georaster:<user>,<pwd>,[db],<rdt>:<rid>\n\n"
        "    user   = user's login\n"
        "    pwd    = user's password\n"
        "    db     = connection identifier (database name)\n"
        "    table  = name of a georaster table\n"
        "    column = name of a georaster column\n"
        "    where  = simple where clause\n"
        "    rdt    = raster data table name\n"
        "    rid    = numeric identification of a georaster\n\n"
        "Note: Allowed separator characters are \",/@:\"\n"
        "      Short name \"geor:\" is also allowed.\n\n"
        "Example: geor:scott,tiger,demodb,table,column,id=1\n"
        "        \"georaster:scott/tiger@demodb,table,column,gain>10\"\n"
        "        \"georaster:scott/tiger@demodb,table,column,city=london\"\n"
        "         georaster:scott,tiger,,rdt_10$,10\n"
        "         geor:scott/tiger,,rdt_10$,10\n\n" );

        CPLFree_nt( papszParam);
        return false;
    }

    return true;
}

//  ---------------------------------------------------------------------------
//                                                           GeoRasterDataset()
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

    GeoRasterWrapper* poGRW = GeoRasterWrapper::Open( poOpenInfo->pszFilename, 
                                           poOpenInfo->eAccess );

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

    if( poGRW->phMetadata != NULL )
    {
        poGRD->nRasterXSize  = poGRW->nRasterColumns;
        poGRD->nRasterYSize  = poGRW->nRasterRows;
        poGRD->nBands        = poGRW->nRasterBands;
        poGRD->poGeoRaster   = poGRW;
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
    //  Create bands
    //  -------------------------------------------------------------------

    int i = 0;
    int nBand = 0;

    for( i = 0; i < poGRD->nBands; i++ )
    {
        nBand = i + 1;
        poGRD->SetBand( nBand, new GeoRasterRasterBand( poGRD, nBand ) );
    }

    //  -------------------------------------------------------------------
    //  Set GDAL's metadata information
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
    //  Check for overwriting
    //  -------------------------------------------------------------------

    const char* pszFetched      = "";
    bool  bOverwrite            = false;

    pszFetched = CSLFetchNameValue( papszOptions, "OVERWRITE" );

    if( pszFetched )
    {
        bOverwrite  = EQUAL( pszFetched, "YES" ) || EQUAL( pszFetched, "ON" );
    }

    if( poGRW->phMetadata != NULL )
    {
        if( ! bOverwrite )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                "Use OVERWRITE option to update an existing GeoRaster" );
            delete poGRD;
            return NULL;
        }

        return (GeoRasterDataset *) poGRD;
    }

    //  -------------------------------------------------------------------
    //  Set basic information and default values
    //  -------------------------------------------------------------------

    poGRW->eType                 = eType;

    poGRW->nRasterColumns        = nXSize;
    poGRW->nRasterRows           = nYSize;
    poGRW->nRasterBands          = nBands;

    //  -------------------------------------------------------------------
    //  Create GeoRaster Table (if necessary)
    //  -------------------------------------------------------------------

    char* pszDescription        = NULL;

    if( ! poGRW->pszTable )
    {
        pszFetched = CSLFetchNameValue( papszOptions, "DESCRIPTION" );

        if( pszFetched )
        {
            pszDescription      = CPLStrdup( pszFetched );
        }
    }

    if( ! poGRW->pszTable )
    {
        poGRW->pszTable          = CPLStrdup( "GDAL_IMPORT" );
    }

    if( ! poGRW->pszColumn )
    {
        poGRW->pszColumn         = CPLStrdup( "RASTER" );
    }

    if( ! poGRW->CreateTable( pszDescription ) )
    {
        CPLFree_nt( pszDescription );
        return NULL;
    }

    CPLFree_nt( pszDescription );

    //  -------------------------------------------------------------------
    //  Create a blank GeoRaster (on a new row on the GeoRaster table)
    //  -------------------------------------------------------------------

    char* pszInsert             = NULL;

    pszFetched = CSLFetchNameValue( papszOptions, "INSERT" );

    if( pszFetched )
    {
        pszInsert               = CPLStrdup( pszFetched );
    }

    if( ! poGRW->CreateBlank( pszInsert ) )
    {
        CPLFree_nt( pszInsert );
        return NULL;
    }

    CPLFree_nt( pszInsert );

    //  -------------------------------------------------------------------
    //  Create a Raster Data Table (if needed)
    //  -------------------------------------------------------------------

    if( ! poGRW->CreateDataTable() )
    {
        return NULL;
    }

    //  -------------------------------------------------------------------
    //  Change GeoRaster Format (set storage parameters)
    //  -------------------------------------------------------------------

    char* pszBlocking       = NULL;
    char* pszInterleav      = NULL;
    char* pszPyramid        = NULL;
    char* pszCompression    = NULL;

    int   nBlockXSize       = 0;
    int   nBlockYSize       = 0;
    int   nBlockBSize       = 0;

    pszFetched = CSLFetchNameValue( papszOptions, "BLOCKXSIZE" );

    if( pszFetched )
    {
        nBlockXSize  = atoi( pszFetched );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "BLOCKYSIZE" );

    if( pszFetched )
    {
        nBlockYSize  = atoi( pszFetched );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "BLOCKBSIZE" );

    if( pszFetched )
    {
        nBlockBSize  = atoi( pszFetched );
    }

    if( nBlockXSize || nBlockYSize )
    {
        if( nBlockYSize == 0 )
        {
            nBlockYSize = nBlockXSize;
        }
 
        if( nBlockXSize == 0 )
        {
            nBlockXSize = nBlockYSize;
        }
 
        if( nBlockBSize == 0 )
        {
            nBlockBSize = nBands == 3 ? 1 : nBands;
        }
 
        if( nBands > 1 )
        {
            pszBlocking = CPLStrdup( 
                CPLSPrintf( "(%d,%d)", nBlockYSize, nBlockXSize ) );
        }
        else
        {
            pszBlocking = CPLStrdup( 
                CPLSPrintf( "(%d,%d,%d)", nBlockYSize,
                                          nBlockXSize,
                                          nBlockBSize ) );
        }
    }

    pszFetched = CSLFetchNameValue( papszOptions, "INTERLEAVE" );

    if( pszFetched )
    {
        pszInterleav = CPLStrdup( pszFetched );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "COMPRESSION" );

    if( pszFetched )
    {
        pszCompression = CPLStrdup( pszFetched );
    }

    pszFetched = CSLFetchNameValue( papszOptions, "PYRAMID" );

    if( pszFetched )
    {
        pszPyramid = CPLStrdup( pszFetched );
    }

    if( ! poGRW->ChangeFormat( pszBlocking,
                              pszInterleav,
                              pszCellDepth,
                              pszPyramid,
                              pszCompression ) )
    {
        CPLFree_nt( pszBlocking );
        CPLFree_nt( pszInterleav );
        CPLFree_nt( pszPyramid );
        CPLFree_nt( pszCompression );
        return NULL;
    }

    CPLFree_nt( pszBlocking );
    CPLFree_nt( pszInterleav );
    CPLFree_nt( pszPyramid );
    CPLFree_nt( pszCompression );

    //  ----------------------------------------------------------------- --
    //  Load aditional options
    //  -------------------------------------------------------------------

    pszFetched = CSLFetchNameValue( papszOptions, "SRID" );

    if( pszFetched )
    {
        poGRW->nSRID = atoi( pszFetched );
        poGRW->SetGeoReference();
    }

    //  -------------------------------------------------------------------
    //  Pepare a identification string
    //  -------------------------------------------------------------------

    char szStringId[OWTEXT];

    strcpy( szStringId, CPLSPrintf( "georaster:%s,%s,%s,%s,%d",
        poGRW->poConnection->GetUser(),
        poGRW->poConnection->GetPassword(),
        poGRW->poConnection->GetServer(),
        poGRW->pszDataTable,
        poGRW->nRasterId ) );


    //  -------------------------------------------------------------------
    //  Finalize Dataset
    //  -------------------------------------------------------------------

    delete poGRD;

    //  -------------------------------------------------------------------
    //  Return a new Dataset
    //  -------------------------------------------------------------------

    return (GeoRasterDataset*) GDALOpen( szStringId, GA_Update );
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

    poGRD->SetProjection( poSrcDS->GetProjectionRef() );
    poSrcDS->GetGeoTransform( adfTransform );

    poGRD->SetGeoTransform( adfTransform );

    // --------------------------------------------------------------------
    //      Copy information to the raster bands
    // --------------------------------------------------------------------

    int    bHasNoDataValue = FALSE;
    double dfNoDataValue = 0.0;
    double dfMin, dfMax, dfStdDev, dfMean;
    int    iBand = 0;

    for( iBand = 1; iBand <= poGRD->nBands; iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand );
        GDALRasterBand *poDstBand = poGRD->GetRasterBand( iBand );

        poSrcBand = poSrcDS->GetRasterBand( iBand );
        poDstBand = (GeoRasterRasterBand*) poGRD->GetRasterBand( iBand );

        if( ((GeoRasterRasterBand*)poDstBand)->poColorTable->GetColorEntryCount() == 0 )
        {
            poDstBand->SetColorTable( poSrcBand->GetColorTable() );
        }

        if( poDstBand->GetStatistics( false, false, &dfMin, &dfMax, 
            &dfStdDev, &dfMean ) == CE_None )
        {
            poDstBand->SetStatistics( dfMin, dfMax, dfStdDev, dfMean );
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

    for( int iBand = 1; iBand <= poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand );
        GDALRasterBand *poDstBand = poGRD->GetRasterBand( iBand );

        CPLErr eErr = CE_None;

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
                    nBlockCols, nBlockRows, eType, 0, 0 );

                if( eErr != CE_None )
                {
                    return NULL;
                }

                eErr = poDstBand->RasterIO( GF_Write,
                    iXOffset, iYOffset, 
                    nBlockCols, nBlockRows, pData, 
                    nBlockCols, nBlockRows, eType, 0, 0 );

                if( eErr != CE_None )
                {
                    return NULL;
                }

                if( ( eErr == CE_None ) && ( ! pfnProgress( 
                    ( iYOffset + 1 ) / (double) nYSize, NULL, pProgressData ) ) )
                {
                    eErr = CE_Failure;
                    CPLError( CE_Failure, CPLE_UserInterrupt, 
                        "User terminated CreateCopy()" );
                }
            }
        }
    }

    CPLFree( pData );

    // --------------------------------------------------------------------
    //      Finalize
    // --------------------------------------------------------------------

    poGRD->FlushCache();

    return poGRD;
}

//  ---------------------------------------------------------------------------
//                                                            GetGeoTransform()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::GetGeoTransform( double *padfTransform )
{
    if( bGeoTransform )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
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

    bGeoTransform = true;

    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                           GetProjectionRef()
//  ---------------------------------------------------------------------------

const char* GeoRasterDataset::GetProjectionRef( void )
{

    if( ! poGeoRaster->bIsReferenced )
    {
        return NULL;
    }

    if( pszSpatialRef )
    {
        return pszSpatialRef;
    }

    OGRSpatialReference oSRS;

    // --------------------------------------------------------------------
    // Try to interpreter the EPSG code
    // --------------------------------------------------------------------

    if( oSRS.importFromEPSG( poGeoRaster->nSRID ) == OGRERR_NONE )
    {
        oSRS.exportToWkt( &pszSpatialRef );
        return pszSpatialRef;
    }

    // --------------------------------------------------------------------
    // Try to interpreter the WKT text
    // --------------------------------------------------------------------

    char* pszWKText = poGeoRaster->GetWKText( poGeoRaster->nSRID );

    if( oSRS.importFromWkt( &pszWKText ) != OGRERR_NONE || 
        oSRS.GetRoot() == NULL )
    {
        CPLFree_nt( pszWKText );
        return FALSE;
    }

    CPLFree_nt( pszWKText );

    // --------------------------------------------------------------------
    // Try to extract EPGS authority codes
    // --------------------------------------------------------------------

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

    int nProjc = OWParseEPSG( oSRS.GetAttrValue("PROJECTION") );
    if( nProjc > 0 )
    {
        oSRS.SetAuthority( "PROJECTION", "EPSG", nProjc );
    }

    oSRS.SetAuthority( oSRS.GetRoot()->GetValue(), "EPSG", poGeoRaster->nSRID );
    oSRS.exportToWkt( &pszSpatialRef );

    return pszSpatialRef;
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
    poGeoRaster->Flush();

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

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                              SetProjection()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::SetProjection( const char *pszProjString )
{
    OGRSpatialReference oSRS;

    char*  pszWKT  = CPLStrdup( pszProjString );

    OGRErr eOGRErr = oSRS.importFromWkt( &pszWKT );

    if( eOGRErr != OGRERR_NONE )
    {
        return CE_Failure;
    }

    // --------------------------------------------------------------------
    // Try to extract EPGS authority code
    // --------------------------------------------------------------------

    const char* pszSRID = NULL;

    if( oSRS.IsProjected() )
    {
        pszSRID = oSRS.GetAuthorityCode( "PROJCS" );
    }
    else if( oSRS.IsGeographic() )
    {
        pszSRID = oSRS.GetAuthorityCode( "GEOGCS" );
    }
    else
    {
        pszSRID = "0";
    }

    if( pszSRID )
    {
        poGeoRaster->nSRID = atoi( pszSRID );
    }

    return CE_None;
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

    char* papszToken[10];
    papszToken[0] = poConnection->GetUser();
    papszToken[1] = poConnection->GetPassword();
    papszToken[2] = poConnection->GetServer();
    papszToken[3] = papszToken[4] = papszToken[5] = 
                    papszToken[6] = papszToken[7] = 
                    papszToken[8] = papszToken[9] = "";

    //  -----------------------------------------------------------
    //  List all the GeoRaster Tables of that User/Database
    //  -----------------------------------------------------------

    char szTable[OWNAME]      = "";
    char szColumn[OWNAME]     = "";
    char szDataTable[OWNAME]  = "";
    char szRasterId[OWNAME]   = "";

    if( poGRW->pszTable  == NULL && 
        poGRW->pszColumn == NULL )
    {
        poStmt = poConnection->CreateStatement(
            "SELECT DISTINCT TABLE_NAME FROM USER_SDO_GEOR_SYSDATA" );

        poStmt->Define( szTable );

        papszToken[3] = papszToken[7] = szTable;
        papszToken[6] = "Table:";
    }

    //  -----------------------------------------------------------
    //  List all the GeoRaster Columns of that Table
    //  -----------------------------------------------------------

    if( poGRW->pszTable  != NULL && 
        poGRW->pszColumn == NULL )
    {
        poStmt = poConnection->CreateStatement(
            "SELECT DISTINCT COLUMN_NAME FROM USER_SDO_GEOR_SYSDATA\n"
            "WHERE  TABLE_NAME = UPPER(:1)" );

        poStmt->Bind( poGRW->pszTable );
        poStmt->Define( szColumn );

        papszToken[3] = poGRW->pszTable;
        papszToken[4] = ",";
        papszToken[5] = papszToken[7] = szColumn;
        papszToken[6] = "Column:";
    }

    //  -----------------------------------------------------------
    //  List all the rows that contains GeoRaster on Table/Column/Where
    //  -----------------------------------------------------------

    if( poGRW->pszTable  != NULL && 
        poGRW->pszColumn != NULL )
    {
        if( poGRW->pszWhere == NULL )
        {
            poStmt = poConnection->CreateStatement( CPLSPrintf(
                "SELECT T.%s.RASTERDATATABLE, T.%s.RASTERID FROM %s T",
                poGRW->pszColumn, poGRW->pszColumn, poGRW->pszTable ) );
        }
        else
        {
            poStmt = poConnection->CreateStatement( CPLSPrintf(
                "SELECT T.%s.RASTERDATATABLE, T.%s.RASTERID FROM %s T\n"
                "WHERE  %s",
                poGRW->pszColumn, poGRW->pszColumn, poGRW->pszTable,
                poGRW->pszWhere ) );
        }

        poStmt->Define( szDataTable );
        poStmt->Define( szRasterId );

        papszToken[3] = papszToken[7] = szDataTable;
        papszToken[4] = ",";
        papszToken[5] = papszToken[9] = szRasterId;
        papszToken[6] = "DataTable:";
        papszToken[8] = "RasterId:";
    }

    if( poStmt->Execute() == false )
    {
        return;
    }

    //  -----------------------------------------------------------
    //  Format subdataset list
    //  -----------------------------------------------------------

    int nCount = 1;

    while( poStmt->Fetch() )
    {
        papszSubdatasets = CSLSetNameValue( papszSubdatasets, 
            CPLSPrintf( "SUBDATASET_%d_NAME", nCount ),
            CPLSPrintf( "georaster:%s,%s,%s,%s%s%s",
            papszToken[0], papszToken[1], papszToken[2], papszToken[3], 
            papszToken[4], papszToken[5] ) );

        papszSubdatasets = CSLSetNameValue( papszSubdatasets, 
            CPLSPrintf( "SUBDATASET_%d_DESC", nCount ),
            CPLSPrintf( "%s%s %s%s", 
            papszToken[6], papszToken[7], papszToken[8], papszToken[9] ) );

        nCount++;
    }
}

/*****************************************************************************/
/*                          GDALRegister_GEOR                                */
/*****************************************************************************/

void CPL_DLL GDALRegister_GEOR()
{
    GeoRasterDriver* poDriver;

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
"  <Option name='BLOCKXSIZE'   type='int'     description='Tile Width'/>"
"  <Option name='BLOCKYSIZE'   type='int'     description='Tile Height'/>"
"  <Option name='BLOCKBSIZE'   type='int'     description='Tile Bands'/>"
"  <Option name='INTERLEAVE'   type='string'  description='{BAND,PIXEL,LINE}'/>"
"  <Option name='PYRAMID'      type='integer' description=''/>"
"  <Option name='INDEX'        type='boolean' description=''/>"
"  <Option name='INSERT'       type='string'  description="
"'<column1, column2,...> VALUES (<value1, value2,...> [*=GeoRaster])'/>"
"</CreationOptionList>" );
        poDriver->pfnOpen       = GeoRasterDataset::Open;
        poDriver->pfnCreate     = GeoRasterDataset::Create;
        poDriver->pfnCreateCopy = GeoRasterDataset::CreateCopy;
        poDriver->pfnIdentify   = GeoRasterDataset::Identify;
        poDriver->pfnDelete     = GeoRasterDataset::Delete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
