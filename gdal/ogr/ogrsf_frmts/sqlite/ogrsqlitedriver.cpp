/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 *
 * Contributor: Alessandro Furieri, a.furieri@lqt.it
 * Portions of this module properly supporting SpatiaLite DB creation
 * Developed for Faunalia ( http://www.faunalia.it) with funding from 
 * Regione Toscana - Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_sqlite.h"
#include "cpl_conv.h"

#ifdef HAVE_SPATIALITE
#include "spatialite.h"
#endif

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRSQLiteDriverUnload()                     */
/************************************************************************/

static void OGRSQLiteDriverUnload(CPL_UNUSED GDALDriver* poDriver)
{
#ifdef SPATIALITE_412_OR_LATER
    spatialite_shutdown();
#endif
}

/************************************************************************/
/*                     OGRSQLiteDriverIdentify()                        */
/************************************************************************/

static int OGRSQLiteDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    int nLen = (int) strlen(poOpenInfo->pszFilename);
    if (EQUALN(poOpenInfo->pszFilename, "VirtualShape:", strlen( "VirtualShape:" )) &&
        nLen > 4 && EQUAL(poOpenInfo->pszFilename + nLen - 4, ".SHP"))
    {
        return TRUE;
    }

    if( EQUAL(poOpenInfo->pszFilename, ":memory:") )
        return TRUE;
/* -------------------------------------------------------------------- */
/*      Verify that the target is a real file, and has an               */
/*      appropriate magic string at the beginning.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 16 )
        return FALSE;

    return( strncmp( (const char*)poOpenInfo->pabyHeader, "SQLite format 3", 15 ) == 0 );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRSQLiteDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRSQLiteDriverIdentify(poOpenInfo) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Check VirtualShape:xxx.shp syntax                               */
/* -------------------------------------------------------------------- */
    int nLen = (int) strlen(poOpenInfo->pszFilename);
    if (EQUALN(poOpenInfo->pszFilename, "VirtualShape:", strlen( "VirtualShape:" )) &&
        nLen > 4 && EQUAL(poOpenInfo->pszFilename + nLen - 4, ".SHP"))
    {
        OGRSQLiteDataSource     *poDS;

        poDS = new OGRSQLiteDataSource();

        char** papszOptions = CSLAddString(NULL, "SPATIALITE=YES");
        int nRet = poDS->Create( ":memory:", papszOptions );
        poDS->SetName(poOpenInfo->pszFilename);
        CSLDestroy(papszOptions);
        if (!nRet)
        {
            delete poDS;
            return NULL;
        }

        char* pszSQLiteFilename = CPLStrdup(poOpenInfo->pszFilename + strlen( "VirtualShape:" ));
        GDALDataset* poSQLiteDS = (GDALDataset*) GDALOpenEx(pszSQLiteFilename,
                                            GDAL_OF_VECTOR, NULL, NULL, NULL);
        if (poSQLiteDS == NULL)
        {
            CPLFree(pszSQLiteFilename);
            delete poDS;
            return NULL;
        }
        delete poSQLiteDS;

        char* pszLastDot = strrchr(pszSQLiteFilename, '.');
        if (pszLastDot)
            *pszLastDot = '\0';

        const char* pszTableName = CPLGetBasename(pszSQLiteFilename);

        char* pszSQL = CPLStrdup(CPLSPrintf("CREATE VIRTUAL TABLE %s USING VirtualShape(%s, CP1252, -1)",
                                            pszTableName, pszSQLiteFilename));
        poDS->ExecuteSQL(pszSQL, NULL, NULL);
        CPLFree(pszSQL);
        CPLFree(pszSQLiteFilename);
        return poDS;
    }

/* -------------------------------------------------------------------- */
/*      We think this is really an SQLite database, go ahead and try    */
/*      and open it.                                                    */
/* -------------------------------------------------------------------- */
    OGRSQLiteDataSource     *poDS;

    poDS = new OGRSQLiteDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRSQLiteDriverCreate( const char * pszName,
                                           CPL_UNUSED int nBands,
                                           CPL_UNUSED int nXSize,
                                           CPL_UNUSED int nYSize,
                                           CPL_UNUSED GDALDataType eDT,
                                           char **papszOptions )
{
/* -------------------------------------------------------------------- */
/*      First, ensure there isn't any such file yet.                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszName, &sStatBuf ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "It seems a file system object called '%s' already exists.",
                  pszName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create datasource.                                       */
/* -------------------------------------------------------------------- */
    OGRSQLiteDataSource     *poDS;

    poDS = new OGRSQLiteDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                             Delete()                                 */
/************************************************************************/

CPLErr OGRSQLiteDriverDelete( const char *pszName )
{
    if (VSIUnlink( pszName ) == 0)
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                         RegisterOGRSQLite()                          */
/************************************************************************/

void RegisterOGRSQLite()

{
    if (! GDAL_CHECK_VERSION("SQLite driver"))
        return;
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "SQLite" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "SQLite" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "SQLite / Spatialite" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_sqlite.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "sqlite db" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
#ifdef HAVE_SPATIALITE
"  <Option name='SPATIALITE' type='boolean' description='Whether to create a Spatialite database' default='NO'/>"
#endif
"  <Option name='METADATA' type='boolean' description='Whether to create the geometry_columns and spatial_ref_sys tables' default='YES'/>"
"  <Option name='INIT_WITH_EPSG' type='boolean' description='Whether to insert the content of the EPSG CSV files into the spatial_ref_sys table ' default='NO'/>"
"</CreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='FORMAT' type='string-select' description='Format of geometry columns'>"
"    <Value>WKB</Value>"
"    <Value>WKT</Value>"
#ifdef HAVE_SPATIALITE
"    <Value>SPATIALITE</Value>"
#endif
"  </Option>"
"  <Option name='LAUNDER' type='boolean' description='Whether layer and field names will be laundered' default='YES'/>"
#ifdef HAVE_SPATIALITE
"  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to create a spatial index for Spatialite databases' default='YES'/>"
"  <Option name='COMPRESS_GEOM' type='boolean' description='Whether to use compressed format of Spatialite geometries' default='NO'/>"
#endif
"  <Option name='SRID' type='int' description='Forced SRID of the layer'/>"
"  <Option name='COMPRESS_COLUMNS' type='string' description='=column_name1[,column_name2, ...].  list of (String) columns that must be compressed with ZLib DEFLATE algorithm'/>"
"  <Option name='OVERWRITE' type='boolean' description='Whether to overwrite an existing table with the layer name to be created' default='NO'/>"
"</LayerCreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGRSQLiteDriverOpen;
        poDriver->pfnIdentify = OGRSQLiteDriverIdentify;
        poDriver->pfnCreate = OGRSQLiteDriverCreate;
        poDriver->pfnDelete = OGRSQLiteDriverDelete;
        poDriver->pfnUnloadDriver = OGRSQLiteDriverUnload;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
