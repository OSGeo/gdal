/******************************************************************************
 * $Id$
 *
 * Project:  ESRI ArcSDE Raster reader
 * Purpose:  Dataset implementaion for ESRI ArcSDE Rasters
 * Author:   Howard Butler, hobu@hobu.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Howard Butler <hobu@hobu.net>
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

#include "sdedataset.h"











/************************************************************************/
/*                          GetRastercount()                            */
/************************************************************************/

int SDEDataset::GetRasterCount( void )

{
    return nBands;
}    

/************************************************************************/
/*                          GetRasterXSize()                            */
/************************************************************************/

int SDEDataset::GetRasterXSize( void )

{
    return nRasterXSize;
}  

/************************************************************************/
/*                          GetRasterYSize()                            */
/************************************************************************/

int SDEDataset::GetRasterYSize( void )

{
    return nRasterYSize;
}


/************************************************************************/
/*                          ComputeRasterInfo()                         */
/************************************************************************/
CPLErr SDEDataset::ComputeRasterInfo() {
    long nSDEErr;
    SE_RASTERINFO raster;
    SE_RASBANDINFO *bands;
    SE_RASTERATTR attributes;
    
    nSDEErr = SE_rasterinfo_create(&raster);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasterinfo_create" );
        return CE_Fatal;
    }
    
    long nRasterColumnId = 0;

    nSDEErr = SE_rascolinfo_get_id(hRasterColumn, &nRasterColumnId);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rascolinfo_get_id" );
        return CE_Fatal;
    }        

    nSDEErr = SE_raster_get_info_by_id(*hConnection, nRasterColumnId, 1, raster);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rascolinfo_get_id" );
        return CE_Fatal;
    }
    nSDEErr = SE_raster_get_bands(*hConnection, raster, &bands, &nBands);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_raster_get_bands" );
        return CE_Fatal;
    }
    
    SE_RASBANDINFO band;
    
    // grab our other stuff from the first band and hope for the best
    band = bands[0];
    
    
    nSDEErr = SE_rasbandinfo_get_band_size(band, &nRasterXSize, &nRasterYSize);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasbandinfo_get_band_size" );
        return CE_Fatal;
    }
    
    SE_ENVELOPE extent;
    nSDEErr = SE_rasbandinfo_get_extent(band, &extent);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasbandinfo_get_extent" );
        return CE_Fatal;
    }
    dfMinX = extent.minx;
    dfMinY = extent.miny;
    dfMaxX = extent.maxx;
    dfMaxY = extent.maxy;
    
    
    nSDEErr = SE_rasterattr_create(&attributes, false);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasterattr_create" );
        return CE_Fatal;
    }


    SE_QUERYINFO query;
    char ** paszTables = (char**) CPLMalloc((SE_MAX_TABLE_LEN+1)*sizeof(char*));

    nSDEErr = SE_queryinfo_create(&query);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_queryinfo_create" );
        return CE_Fatal;
    }
        
    nSDEErr = SE_queryinfo_set_tables(query, 1, (const char**) &pszLayerName, NULL);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_queryinfo_set_tables" );
        return CE_Fatal;
    }

    nSDEErr = SE_queryinfo_set_where_clause(query, (const char*) "");
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_queryinfo_set_where" );
        return CE_Fatal;
    }

    nSDEErr = SE_queryinfo_set_columns(query, 1, (const char**) &pszColumnName);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_queryinfo_set_where" );
        return CE_Fatal;
    }

    SE_STREAM stream;
    nSDEErr = SE_stream_create(*hConnection, &stream);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_create" );
        return CE_Fatal;
    }

    nSDEErr = SE_stream_query_with_info(stream, query);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_query_with_info" );
        return CE_Fatal;
    }

    nSDEErr = SE_stream_execute (stream);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_execute" );
        return CE_Fatal;
    }
    nSDEErr = SE_stream_fetch (stream);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_fetch" );
        return CE_Fatal;
    }

    nSDEErr = SE_stream_get_raster (stream, 1, attributes);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_stream_fetch" );
        return CE_Fatal;
    }

    eDataType = GDT_UInt16;
    long pixel_type;
    nSDEErr = SE_rasterattr_get_pixel_type (attributes, &pixel_type);
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rasterattr_get_pixel_type" );
        return CE_Fatal;
    }
    CPLDebug("SDERASTER", "pixel type from sde: %d %d", pixel_type,SE_PIXEL_TYPE_GET_DEPTH(pixel_type));
    
    eDataType = MorphESRIRasterType(pixel_type);
    
    CPLDebug("SDERASTER", "pixel type from GDAL: %d", eDataType);
    
    for (int i=0; i < nBands; i++) {
        SetBand( i+1, new SDERasterBand( this, i+1, bands[i] ));
    }

    
    SE_queryinfo_free(query);
    SE_stream_free(stream);
    
    SE_rasterinfo_free(raster);
    SE_rasterband_free_info_list(nBands, bands);

    return CE_None;
}

GDALDataType SDEDataset::MorphESRIRasterType(int gtype) {
    
    switch (gtype) {
        case SE_PIXEL_TYPE_1BIT:
            return GDT_Byte;
        case SE_PIXEL_TYPE_4BIT:
            return GDT_Byte;
        case SE_PIXEL_TYPE_8BIT_U:
            return GDT_Byte;
        case SE_PIXEL_TYPE_8BIT_S:
            return GDT_Byte;
        case SE_PIXEL_TYPE_16BIT_U:
            return GDT_UInt16;
        case SE_PIXEL_TYPE_16BIT_S:
            return GDT_Int16;
        case SE_PIXEL_TYPE_32BIT_U:
            return GDT_UInt32;
        case SE_PIXEL_TYPE_32BIT_S:
            return GDT_Int32;
        case SE_PIXEL_TYPE_32BIT_REAL:
            return GDT_Float32;
        case SE_PIXEL_TYPE_64BIT_REAL:
            return GDT_Float64;
        default:
            return GDT_UInt16;
        }
}
/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr SDEDataset::GetGeoTransform( double * padfTransform )

{
    
    if (dfMinX == 0.0 && dfMinY == 0.0 && dfMaxX == 0.0 && dfMaxY == 0.0)
        return CE_Fatal;
 
    padfTransform[0] = dfMinX;
    padfTransform[3] = dfMaxY;
    padfTransform[1] = (dfMaxX - dfMinX) / GetRasterXSize();
    padfTransform[2] = 0.0;
        
    padfTransform[4] = 0.0;
    padfTransform[5] = -1 * (dfMaxY - dfMinY) / GetRasterYSize();
    
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *SDEDataset::GetProjectionRef()

{
    long nSDEErr;
    SE_COORDREF coordref;
    nSDEErr = SE_coordref_create(&coordref);

    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_coordref_create" );
        return FALSE;
    }
    
    if (!hRasterColumn){
        CPLError ( CE_Failure, CPLE_AppDefined,
                   "Raster Column not defined");        
        return ("");   
    }
    
    nSDEErr = SE_rascolinfo_get_coordref(hRasterColumn, coordref);

    if (nSDEErr == SE_NO_COORDREF) {
        return ("");
    }
    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_rascolinfo_get_coordref" );
      //  return FALSE;
    }    
    
    char szWKT[SE_MAX_SPATIALREF_SRTEXT_LEN];
    nSDEErr = SE_coordref_get_description(coordref, szWKT);
    if (nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_coordref_get_description");
    }
    SE_coordref_free(coordref);

    OGRSpatialReference *poSRS;
    poSRS = new OGRSpatialReference(szWKT);
    poSRS->morphFromESRI();
    char* pszWKT;
    poSRS->exportToWkt(&pszWKT);
    poSRS->Release();
    return CPLStrdup(pszWKT);
}

/************************************************************************/
/*                                SDEDataset()                          */
/************************************************************************/

SDEDataset::SDEDataset( SE_CONNECTION* connection )

{
    hConnection         = connection;
    nSubDataCount       = 0;
    pszLayerName        = NULL;
    pszColumnName       = NULL;
    paohSDERasterColumns  = NULL;
    hRasterColumn       = NULL;
    nBands              = 0;
    nRasterXSize        = 0;
    nRasterYSize        = 0;
    
    dfMinX              = 0.0;
    dfMinY              = 0.0;
    dfMaxX              = 0.0;
    dfMaxY              = 0.0;
    SE_rascolinfo_create(&hRasterColumn);

}



/************************************************************************/
/*                            ~SDEDataset()                             */
/************************************************************************/

SDEDataset::~SDEDataset()

{
//    if (paohSDERasterColumns != NULL)
//        SE_rastercolumn_free_info_list(nSubDataCount,
//                                   paohSDERasterColumns);
    if (hRasterColumn)
        SE_rascolinfo_free(hRasterColumn);
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SDEDataset::Open( GDALOpenInfo * poOpenInfo )

{
    


/* -------------------------------------------------------------------- */
/*      If we aren't prefixed with SDE: then ignore this datasource.    */
/* -------------------------------------------------------------------- */
    if( !EQUALN(poOpenInfo->pszFilename,"SDE:",4) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Parse arguments on comma.  We expect (layer is optional):       */
/*        SDE:server,instance,database,username,password,layer          */
/* -------------------------------------------------------------------- */
    char **papszTokens = CSLTokenizeStringComplex( poOpenInfo->pszFilename+4, ",",
                                                   TRUE, TRUE );
    CPLDebug( "SDERASTER", "Open(\"%s\") revealed %d tokens.", poOpenInfo->pszFilename,
              CSLCount( papszTokens ) );


    if( CSLCount( papszTokens ) < 5 || CSLCount( papszTokens ) > 6 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "SDE connect string had wrong number of arguments.\n"
                  "Expected 'SDE:server,instance,database,username,password,layer'\n"
                  "The layer name value is optional.\n"
                  "Got '%s'", 
                  poOpenInfo->pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try to establish connection.                                    */
/* -------------------------------------------------------------------- */
    int         nSDEErr;
    SE_ERROR    sSDEErrorInfo;

    SE_CONNECTION connection = NULL;
        
    nSDEErr = SE_connection_create( papszTokens[0], 
                                    papszTokens[1], 
                                    papszTokens[2], 
                                    papszTokens[3],
                                    papszTokens[4],
                                    &sSDEErrorInfo, &connection );

    if( nSDEErr != SE_SUCCESS )
    {
        IssueSDEError( nSDEErr, "SE_connection_create" );
        return FALSE;
    }


/* -------------------------------------------------------------------- */
/*      Set unprotected concurrency policy, suitable for single         */
/*      threaded access.                                                */
/* -------------------------------------------------------------------- */
    nSDEErr = SE_connection_set_concurrency( connection,
                                             SE_UNPROTECTED_POLICY);

    if( nSDEErr != SE_SUCCESS) {
        IssueSDEError( nSDEErr, NULL );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */

    SDEDataset *poDS;

    poDS = new SDEDataset(&connection);

/* -------------------------------------------------------------------- */
/*      If we were given a layer name, use that directly, otherwise     */
/*      query for subdatasets.                                          */
/* -------------------------------------------------------------------- */


    if (CSLCount( papszTokens ) == 6 ) {
//
        poDS->pszLayerName = CPLStrdup( papszTokens[5] );
//        
//        // FIXME this needs to be a configuration option or allow it to
//        // come in via the arguments
        poDS->pszColumnName = CPLStrdup( "RASTER" );
        CPLDebug( "SDERASTER", "'%s' raster layer specified... "\
                               "using it directly with '%s' as the raster column name.", 
                  poDS->pszLayerName,
                  poDS->pszColumnName);
        nSDEErr = SE_rastercolumn_get_info_by_name(*(poDS->hConnection), 
                                                    poDS->pszLayerName, 
                                                    poDS->pszColumnName, 
                                                    poDS->hRasterColumn);
        if( nSDEErr != SE_SUCCESS )
        {
            IssueSDEError( nSDEErr, "SE_rastercolumn_get_info_by_name" );
            return FALSE;
        }
        poDS->ComputeRasterInfo();
    } else {
//
//      SE_RASCOLINFO* columns;
//          nSDEErr = SE_rastercolumn_get_info_list(connection, 
//                                                  &(poDS->paohSDERasterColumns), 
//                                                  &(poDS->nSubDataCount));
//        if( nSDEErr != SE_SUCCESS )
//        {
//            IssueSDEError( nSDEErr, "SE_rascolinfo_get_info_list" );
//            return FALSE;
//        }
//
//        CPLDebug( "SDERASTER", "No layername specified, %d subdatasets available.", 
//                  poDS->nSubDataCount);
//                  
//
//        for (int i = 0; i < poDS->nSubDataCount; i++) {
//
//              char         szTableName[SE_QUALIFIED_TABLE_NAME+1];
//              char         szColumnName[SE_MAX_COLUMN_LEN+1];
//            nSDEErr = SE_rascolinfo_get_raster_column (poDS->paohSDERasterColumns[i], 
//                                                       szTableName, 
//                                                       szColumnName); 
//            CPLDebug("SDERASTER", "Layer '%s' with column '%s' found.", szTableName, szColumnName);
//
//            if( nSDEErr != SE_SUCCESS )
//            {
//                IssueSDEError( nSDEErr, "SE_rascolinfo_get_raster_column" );
//                return FALSE;
//            }
//        }
//
    return FALSE;
    }
    
    return( poDS );
}

/************************************************************************/
/*                          GDALRegister_SDE()                          */
/************************************************************************/

void GDALRegister_SDE()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "SDE" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "SDE" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "ESRI ArcSDE" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#SDE" );
       // poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "mem" );

        poDriver->pfnOpen = SDEDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
