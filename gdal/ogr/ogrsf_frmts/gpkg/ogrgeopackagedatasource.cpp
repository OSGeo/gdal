/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements OGRGeoPackageDataSource class
 * Author:   Paul Ramsey <pramsey@boundlessgeo.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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
 * DEALINGS IN THE SOFpszFileNameTWARE.
 ****************************************************************************/

#include "ogr_geopackage.h"
#include "ogrgeopackageutility.h"
#include "ogr_p.h"
#include "swq.h"

/* 1.1.1: A GeoPackage SHALL contain 0x47503130 ("GP10" in ASCII) in the application id */
/* http://opengis.github.io/geopackage/#_file_format */
/* 0x47503130 = 1196437808 */
#define GPKG_APPLICATION_ID 1196437808

/* "GP10" in ASCII bytes */
static const char aGpkgId[4] = {0x47, 0x50, 0x31, 0x30};
static const size_t szGpkgIdPos = 68;

/* Only recent versions of SQLite will let us muck with application_id */
/* via a PRAGMA statement, so we have to write directly into the */
/* file header here. */
/* We do this at the *end* of initialization so that there is */
/* data to write down to a file, and we'll have a writeable file */
/* once we close the SQLite connection */
OGRErr OGRGeoPackageDataSource::SetApplicationId()
{
    CPLAssert( hDB != NULL );
    CPLAssert( pszName != NULL );

    /* Have to flush the file before f***ing with the header */
    CloseDB();

    size_t szWritten = 0;

    /* Open for modification, write to application id area */
    VSILFILE *pfFile = VSIFOpenL( pszName, "rb+" );
    if( pfFile == NULL )
        return OGRERR_FAILURE;
    VSIFSeekL(pfFile, szGpkgIdPos, SEEK_SET);
    szWritten = VSIFWriteL(aGpkgId, 1, 4, pfFile);
    VSIFCloseL(pfFile);

    /* If we didn't write out exactly four bytes, something */
    /* terrible has happened */
    if ( szWritten != 4 )
    {
        return OGRERR_FAILURE;
    }

    /* And re-open the file */
#ifdef HAVE_SQLITE_VFS
    if (!OpenOrCreateDB(SQLITE_OPEN_READWRITE) )
#else
    if (!OpenOrCreateDB(0))
#endif
        return OGRERR_FAILURE;

    return OGRERR_NONE;
}


/* Returns the first row of first column of SQL as integer */
OGRErr OGRGeoPackageDataSource::PragmaCheck(const char * pszPragma, const char * pszExpected, int nRowsExpected)
{
    CPLAssert( pszPragma != NULL );
    CPLAssert( pszExpected != NULL );
    CPLAssert( nRowsExpected >= 0 );
    
    char *pszErrMsg = NULL;
    int nRowCount, nColCount, rc;
    char **papszResult;

    rc = sqlite3_get_table(
        hDB,
        CPLSPrintf("PRAGMA %s", pszPragma),
        &papszResult, &nRowCount, &nColCount, &pszErrMsg );
    
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "unable to execute PRAGMA %s", pszPragma);
        return OGRERR_FAILURE;
    }
    
    if ( nRowCount != nRowsExpected )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "bad result for PRAGMA %s, got %d rows, expected %d", pszPragma, nRowCount, nRowsExpected);
        return OGRERR_FAILURE;        
    }
    
    if ( nRowCount > 0 && ! EQUAL(papszResult[1], pszExpected) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "invalid %s (expected '%s', got '%s')",
                  pszPragma, pszExpected, papszResult[1]);
        return OGRERR_FAILURE;
    }
    
    sqlite3_free_table(papszResult);
    
    return OGRERR_NONE; 
}


OGRSpatialReference* OGRGeoPackageDataSource::GetSpatialRef(int iSrsId)
{
    SQLResult oResult;
    
    /* Should we do something special with undefined SRS ? */
    if( iSrsId == 0 || iSrsId == -1 )
    {
        return NULL;
    }
    
    CPLString oSQL;
    oSQL.Printf("SELECT definition FROM gpkg_spatial_ref_sys WHERE srs_id = %d", iSrsId);
    
    OGRErr err = SQLQuery(hDB, oSQL.c_str(), &oResult);

    if ( err != OGRERR_NONE || oResult.nRowCount != 1 )
    {
        SQLResultFree(&oResult);
        CPLError( CE_Warning, CPLE_AppDefined, "unable to read srs_id '%d' from gpkg_spatial_ref_sys",
                  iSrsId);
        return NULL;
    }
    
    const char *pszWkt = SQLResultGetValue(&oResult, 0, 0);
    if ( ! pszWkt )
    {
        SQLResultFree(&oResult);
        CPLError( CE_Warning, CPLE_AppDefined, "null definition for srs_id '%d' in gpkg_spatial_ref_sys",
                  iSrsId);
        return NULL;
    }
    
    OGRSpatialReference *poSpatialRef = new OGRSpatialReference(pszWkt);
    
    if ( poSpatialRef == NULL )
    {
        SQLResultFree(&oResult);
        CPLError( CE_Warning, CPLE_AppDefined, "unable to parse srs_id '%d' well-known text '%s'",
                  iSrsId, pszWkt);
        return NULL;
    }
    
    SQLResultFree(&oResult);
    return poSpatialRef;
}

const char * OGRGeoPackageDataSource::GetSrsName(const OGRSpatialReference * poSRS)
{
    const OGR_SRSNode *node;
    
    /* Projected coordinate system? */
    if ( (node = poSRS->GetAttrNode("PROJCS")) )
    {
        return node->GetChild(0)->GetValue();
    }
    /* Geographic coordinate system? */
    else if ( (node = poSRS->GetAttrNode("GEOGCS")) )
    {
        return node->GetChild(0)->GetValue();
    }
    /* Something odd! return empty. */
    else
    {
        return "Unnamed SRS";
    }
}

int OGRGeoPackageDataSource::GetSrsId(const OGRSpatialReference * cpoSRS)
{
    char *pszWKT = NULL;
    char *pszSQL = NULL;
    int nSRSId = UNDEFINED_SRID;
    const char* pszAuthorityName;
    int nAuthorityCode = 0;
    OGRErr err;
    OGRBoolean bCanUseAuthorityCode = FALSE;

    if( cpoSRS == NULL )
        return UNDEFINED_SRID;

    OGRSpatialReference *poSRS = cpoSRS->Clone();

    pszAuthorityName = poSRS->GetAuthorityName(NULL);

    if ( pszAuthorityName == NULL || strlen(pszAuthorityName) == 0 )
    {
        // Try to force identify an EPSG code                                    
        poSRS->AutoIdentifyEPSG();

        pszAuthorityName = poSRS->GetAuthorityName(NULL);
        if (pszAuthorityName != NULL && EQUAL(pszAuthorityName, "EPSG"))
        {
            const char* pszAuthorityCode = poSRS->GetAuthorityCode(NULL);
            if ( pszAuthorityCode != NULL && strlen(pszAuthorityCode) > 0 )
            {
                /* Import 'clean' SRS */
                poSRS->importFromEPSG( atoi(pszAuthorityCode) );

                pszAuthorityName = poSRS->GetAuthorityName(NULL);
            }
        }
    }
    // Check whether the EPSG authority code is already mapped to a
    // SRS ID.                                                         
    if ( pszAuthorityName != NULL && strlen(pszAuthorityName) > 0 )
    {
        // For the root authority name 'EPSG', the authority code
        // should always be integral
        nAuthorityCode = atoi( poSRS->GetAuthorityCode(NULL) );

        pszSQL = sqlite3_mprintf(
                         "SELECT srs_id FROM gpkg_spatial_ref_sys WHERE "
                         "upper(organization) = upper('%q') AND organization_coordsys_id = %d",
                         pszAuthorityName, nAuthorityCode );
        
        nSRSId = SQLGetInteger(hDB, pszSQL, &err);
        sqlite3_free(pszSQL);
        
        // Got a match? Return it!
        if ( OGRERR_NONE == err )
        {
            delete poSRS;
            return nSRSId;
        }
        
        // No match, but maybe we can use the nAuthorityCode as the nSRSId?
        pszSQL = sqlite3_mprintf(
                         "SELECT Count(*) FROM gpkg_spatial_ref_sys WHERE "
                         "srs_id = %d", nAuthorityCode );
        
        // Yep, we can!
        if ( ! SQLGetInteger(hDB, pszSQL, &err) && err == OGRERR_NONE )
            bCanUseAuthorityCode = TRUE;
        sqlite3_free(pszSQL);
    }

    // Translate SRS to WKT.                                           
    if( poSRS->exportToWkt( &pszWKT ) != OGRERR_NONE )
    {
        delete poSRS;
        CPLFree(pszWKT);
        return UNDEFINED_SRID;
    }

    // Reuse the authority code number as SRS_ID if we can
    if ( bCanUseAuthorityCode )
    {
        nSRSId = nAuthorityCode;
    }
    // Otherwise, generate a new SRS_ID number (max + 1)
    else
    {
        // Get the current maximum srid in the srs table.                  
        int nMaxSRSId = SQLGetInteger(hDB, "SELECT MAX(srs_id) FROM gpkg_spatial_ref_sys", &err);
        if ( OGRERR_NONE != err )
        {
            CPLFree(pszWKT);
            delete poSRS;
            return UNDEFINED_SRID;        
        }

        nSRSId = nMaxSRSId + 1;
    }
    
    // Add new SRS row to gpkg_spatial_ref_sys
    if( pszAuthorityName != NULL && nAuthorityCode > 0 )
    {
        pszSQL = sqlite3_mprintf(
                 "INSERT INTO gpkg_spatial_ref_sys "
                 "(srs_name,srs_id,organization,organization_coordsys_id,definition) "
                 "VALUES ('%q', %d, upper('%q'), %d, '%q')",
                 GetSrsName(poSRS), nSRSId, pszAuthorityName, nAuthorityCode, pszWKT
                 );
    }
    else
    {
        pszSQL = sqlite3_mprintf(
                 "INSERT INTO gpkg_spatial_ref_sys "
                 "(srs_name,srs_id,organization,organization_coordsys_id,definition) "
                 "VALUES ('%q', %d, upper('%q'), %d, '%q')",
                 GetSrsName(poSRS), nSRSId, "NONE", nSRSId, pszWKT
                 );
    }

    // Add new row to gpkg_spatial_ref_sys
    err = SQLCommand(hDB, pszSQL);

    // Free everything that was allocated.
    CPLFree(pszWKT);    
    sqlite3_free(pszSQL);
    delete poSRS;
    
    return nSRSId;
}


/************************************************************************/
/*                        OGRGeoPackageDataSource()                     */
/************************************************************************/

OGRGeoPackageDataSource::OGRGeoPackageDataSource()
{
    m_papoLayers = NULL;
    m_nLayers = 0;
    m_bUtf8 = FALSE;
}

/************************************************************************/
/*                       ~OGRGeoPackageDataSource()                     */
/************************************************************************/

OGRGeoPackageDataSource::~OGRGeoPackageDataSource()
{
    for( int i = 0; i < m_nLayers; i++ )
        delete m_papoLayers[i];

    CPLFree( m_papoLayers );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGeoPackageDataSource::Open(const char * pszFilename, int bUpdateIn )
{
    int i;
    OGRErr err;

    CPLAssert( m_nLayers == 0 );
    CPLAssert( hDB == NULL );

    bUpdate = bUpdateIn;
    pszName = CPLStrdup( pszFilename );

    /* See if we can open the SQLite database */
#ifdef HAVE_SQLITE_VFS
    if (!OpenOrCreateDB((bUpdateIn) ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY) )
#else
    if (!OpenOrCreateDB(0))
#endif
        return FALSE;

    /* Requirement 6: The SQLite PRAGMA integrity_check SQL command SHALL return “ok” */
    /* http://opengis.github.io/geopackage/#_file_integrity */
    /* Disable integrity check by default, since it is expensive on big files */
    if( CSLTestBoolean(CPLGetConfigOption("OGR_GPKG_INTEGRITY_CHECK", "NO")) &&
        OGRERR_NONE != PragmaCheck("integrity_check", "ok", 1) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "pragma integrity_check on '%s' failed", pszFilename);
        return FALSE;
    }
    
    /* Requirement 7: The SQLite PRAGMA foreign_key_check() SQL with no */
    /* parameter value SHALL return an empty result set */
    /* http://opengis.github.io/geopackage/#_file_integrity */
    if ( OGRERR_NONE != PragmaCheck("foreign_key_check", "", 0) ) 
    {
        CPLError( CE_Failure, CPLE_AppDefined, "pragma foreign_key_check on '%s' failed", pszFilename);
        return FALSE; 
    }

    /* OGR UTF-8 capability, we'll advertise UTF-8 support if we have it */
    if ( OGRERR_NONE == PragmaCheck("encoding", "UTF-8", 1) ) 
    {
        m_bUtf8 = TRUE;
    }
    else
    {
        m_bUtf8 = FALSE;
    }

    /* Check for requirement metadata tables */
    /* Requirement 10: gpkg_spatial_ref_sys must exist */
    /* Requirement 13: gpkg_contents must exist */
    /* Requirement 21: gpkg_geometry_columns must exist */
    static std::string aosGpkgTables[] = {
        "gpkg_geometry_columns",
        "gpkg_spatial_ref_sys",
        "gpkg_contents"
    };
    
    for ( i = 0; i < 3; i++ )
    {
        SQLResult oResult;
        char *pszSQL = sqlite3_mprintf("pragma table_info('%q')", aosGpkgTables[i].c_str());
        err = SQLQuery(hDB, pszSQL, &oResult);
        sqlite3_free(pszSQL);
        
        if  ( err != OGRERR_NONE )
            return FALSE;
            
        if ( oResult.nRowCount <= 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "required GeoPackage table '%s' is missing", aosGpkgTables[i].c_str());
            SQLResultFree(&oResult);
            return FALSE;
        }
        
        SQLResultFree(&oResult);
    }

    /* Load layer definitions for all tables in gpkg_contents & gpkg_geometry_columns */
    /* and non-spatial tables as well */
    SQLResult oResult;
    std::string osSQL =
        "SELECT c.table_name, c.identifier, 1 as is_spatial, c.min_x, c.min_y, c.max_x, c.max_y "
        "  FROM gpkg_geometry_columns g JOIN gpkg_contents c ON (g.table_name = c.table_name)"
        "  WHERE c.data_type = 'features' ";

    if (HasGDALAspatialExtension()) {
        osSQL +=
            "UNION ALL "
            "SELECT table_name, identifier, 0 as is_spatial, 0 AS xmin, 0 AS ymin, 0 AS xmax, 0 AS ymax "
            "  FROM gpkg_contents"
            "  WHERE data_type = 'aspatial' ";
    }

    err = SQLQuery(hDB, osSQL.c_str(), &oResult);
    if  ( err != OGRERR_NONE )
    {
        SQLResultFree(&oResult);
        return FALSE;
    }

    if ( oResult.nRowCount > 0 )
    {
        m_papoLayers = (OGRGeoPackageTableLayer**)CPLMalloc(sizeof(OGRGeoPackageTableLayer*) * oResult.nRowCount);

        for ( i = 0; i < oResult.nRowCount; i++ )
        {
            const char *pszTableName = SQLResultGetValue(&oResult, 0, i);
            if ( ! pszTableName )
            {
                CPLError(CE_Warning, CPLE_AppDefined, "unable to read table name for layer(%d)", i);            
                continue;
            }
            int bIsSpatial = SQLResultGetValueAsInteger(&oResult, 2, i);
            OGRGeoPackageTableLayer *poLayer = new OGRGeoPackageTableLayer(this, pszTableName);
            if( OGRERR_NONE != poLayer->ReadTableDefinition(bIsSpatial) )
            {
                delete poLayer;
                CPLError(CE_Warning, CPLE_AppDefined, "unable to read table definition for '%s'", pszTableName);            
                continue;
            }
            m_papoLayers[m_nLayers++] = poLayer;
        }
    }
    
    SQLResultFree(&oResult);

    return TRUE;
}

/************************************************************************/
/*                                Create()                              */
/************************************************************************/

int OGRGeoPackageDataSource::Create( const char * pszFilename,
                                     CPL_UNUSED char **papszOptions )
{
    CPLString osCommand;
    const char *pszSpatialRefSysRecord;

    pszName = CPLStrdup(pszFilename);
    bUpdate = TRUE;

    /* The OGRGeoPackageDriver has already confirmed that the pszFilename */
    /* is not already in use, so try to create the file */
#ifdef HAVE_SQLITE_VFS
    if (!OpenOrCreateDB(SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
#else
    if (!OpenOrCreateDB(0))
#endif
        return FALSE;

    /* OGR UTF-8 support. If we set the UTF-8 Pragma early on, it */
    /* will be written into the main file and supported henceforth */
    SQLCommand(hDB, "PRAGMA encoding = \"UTF-8\"");

    /* Requirement 2: A GeoPackage SHALL contain 0x47503130 ("GP10" in ASCII) in the application id */
    /* http://opengis.github.io/geopackage/#_file_format */
    const char *pszPragma = CPLSPrintf("PRAGMA application_id = %d", GPKG_APPLICATION_ID);
    
    if ( OGRERR_NONE != SQLCommand(hDB, pszPragma) )
        return FALSE;
        
    /* Requirement 10: A GeoPackage SHALL include a gpkg_spatial_ref_sys table */
    /* http://opengis.github.io/geopackage/#spatial_ref_sys */
    const char *pszSpatialRefSys = 
        "CREATE TABLE gpkg_spatial_ref_sys ("
        "srs_name TEXT NOT NULL,"
        "srs_id INTEGER NOT NULL PRIMARY KEY,"
        "organization TEXT NOT NULL,"
        "organization_coordsys_id INTEGER NOT NULL,"
        "definition  TEXT NOT NULL,"
        "description TEXT"
        ")";
        
    if ( OGRERR_NONE != SQLCommand(hDB, pszSpatialRefSys) )
        return FALSE;

    /* Requirement 11: The gpkg_spatial_ref_sys table in a GeoPackage SHALL */
    /* contain a record for EPSG:4326, the geodetic WGS84 SRS */
    /* http://opengis.github.io/geopackage/#spatial_ref_sys */
    pszSpatialRefSysRecord = 
        "INSERT INTO gpkg_spatial_ref_sys ("
        "srs_name, srs_id, organization, organization_coordsys_id, definition, description"
        ") VALUES ("
        "'WGS 84 geodetic', 4326, 'EPSG', 4326, '"
        "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]]"
        "', 'longitude/latitude coordinates in decimal degrees on the WGS 84 spheroid'"
        ")";  
          
    if ( OGRERR_NONE != SQLCommand(hDB, pszSpatialRefSysRecord) )
        return FALSE;

    /* Requirement 11: The gpkg_spatial_ref_sys table in a GeoPackage SHALL */
    /* contain a record with an srs_id of -1, an organization of “NONE”, */
    /* an organization_coordsys_id of -1, and definition “undefined” */
    /* for undefined Cartesian coordinate reference systems */
    /* http://opengis.github.io/geopackage/#spatial_ref_sys */
    pszSpatialRefSysRecord = 
        "INSERT INTO gpkg_spatial_ref_sys ("
        "srs_name, srs_id, organization, organization_coordsys_id, definition, description"
        ") VALUES ("
        "'Undefined cartesian SRS', -1, 'NONE', -1, 'undefined', 'undefined cartesian coordinate reference system'"
        ")"; 
           
    if ( OGRERR_NONE != SQLCommand(hDB, pszSpatialRefSysRecord) )
        return FALSE;

    /* Requirement 11: The gpkg_spatial_ref_sys table in a GeoPackage SHALL */
    /* contain a record with an srs_id of 0, an organization of “NONE”, */
    /* an organization_coordsys_id of 0, and definition “undefined” */
    /* for undefined geographic coordinate reference systems */
    /* http://opengis.github.io/geopackage/#spatial_ref_sys */
    pszSpatialRefSysRecord = 
        "INSERT INTO gpkg_spatial_ref_sys ("
        "srs_name, srs_id, organization, organization_coordsys_id, definition, description"
        ") VALUES ("
        "'Undefined geographic SRS', 0, 'NONE', 0, 'undefined', 'undefined geographic coordinate reference system'"
        ")"; 
           
    if ( OGRERR_NONE != SQLCommand(hDB, pszSpatialRefSysRecord) )
        return FALSE;
    
    /* Requirement 13: A GeoPackage file SHALL include a gpkg_contents table */
    /* http://opengis.github.io/geopackage/#_contents */
    const char *pszContents =
        "CREATE TABLE gpkg_contents ("
        "table_name TEXT NOT NULL PRIMARY KEY,"
        "data_type TEXT NOT NULL,"
        "identifier TEXT UNIQUE,"
        "description TEXT DEFAULT '',"
        "last_change DATETIME NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ',CURRENT_TIMESTAMP)),"
        "min_x DOUBLE, min_y DOUBLE,"
        "max_x DOUBLE, max_y DOUBLE,"
        "srs_id INTEGER,"
        "CONSTRAINT fk_gc_r_srs_id FOREIGN KEY (srs_id) REFERENCES gpkg_spatial_ref_sys(srs_id)"
        ")";
        
    if ( OGRERR_NONE != SQLCommand(hDB, pszContents) )
        return FALSE;

    /* Requirement 21: A GeoPackage with a gpkg_contents table row with a “features” */
    /* data_type SHALL contain a gpkg_geometry_columns table or updateable view */
    /* http://opengis.github.io/geopackage/#_geometry_columns */
    const char *pszGeometryColumns =        
        "CREATE TABLE gpkg_geometry_columns ("
        "table_name TEXT NOT NULL,"
        "column_name TEXT NOT NULL,"
        "geometry_type_name TEXT NOT NULL,"
        "srs_id INTEGER NOT NULL,"
        "z TINYINT NOT NULL,"
        "m TINYINT NOT NULL,"
        "CONSTRAINT pk_geom_cols PRIMARY KEY (table_name, column_name),"
        "CONSTRAINT uk_gc_table_name UNIQUE (table_name),"
        "CONSTRAINT fk_gc_tn FOREIGN KEY (table_name) REFERENCES gpkg_contents(table_name),"
        "CONSTRAINT fk_gc_srs FOREIGN KEY (srs_id) REFERENCES gpkg_spatial_ref_sys (srs_id)"
        ")";
        
    if ( OGRERR_NONE != SQLCommand(hDB, pszGeometryColumns) )
        return FALSE;

    /* Requirement 2: A GeoPackage SHALL contain 0x47503130 ("GP10" in ASCII) */
    /* in the application id field of the SQLite database header */
    /* We have to do this after there's some content so the database file */
    /* is not zero length */
    SetApplicationId();


    return TRUE;
}


/************************************************************************/
/*                              AddColumn()                             */
/************************************************************************/

OGRErr OGRGeoPackageDataSource::AddColumn(const char *pszTableName, const char *pszColumnName, const char *pszColumnType)
{
    char *pszSQL;
    
    pszSQL = sqlite3_mprintf("ALTER TABLE \"%s\" ADD COLUMN \"%s\" %s", 
                             pszTableName, pszColumnName, pszColumnType);

    OGRErr err = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    
    return err;
}


/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer* OGRGeoPackageDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= m_nLayers )
        return NULL;
    else
        return m_papoLayers[iLayer];
}


/************************************************************************/
/*                          ICreateLayer()                              */
/* Options:                                                             */
/*   FID = primary key name                                             */
/*   OVERWRITE = YES|NO, overwrite existing layer?                      */
/*   SPATIAL_INDEX = YES|NO, TBD                                        */
/************************************************************************/

OGRLayer* OGRGeoPackageDataSource::ICreateLayer( const char * pszLayerName,
                                      OGRSpatialReference * poSpatialRef,
                                      OGRwkbGeometryType eGType,
                                      char **papszOptions )
{
    int iLayer;
    OGRErr err;

/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "New layer %s cannot be created.\n",
                  pszName, pszLayerName );

        return NULL;
    }

    /* Read GEOMETRY_COLUMN option */
    const char* pszGeomColumnName = CSLFetchNameValue(papszOptions, "GEOMETRY_COLUMN");
    if (pszGeomColumnName == NULL)
        pszGeomColumnName = "geom";
    
    /* Read FID option */
    const char* pszFIDColumnName = CSLFetchNameValue(papszOptions, "FID");
    if (pszFIDColumnName == NULL)
        pszFIDColumnName = "fid";

    if ( strspn(pszFIDColumnName, "`~!@#$%^&*()+-={}|[]\\:\";'<>?,./") > 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The primary key (%s) name may not contain special characters or spaces", 
                 pszFIDColumnName);
        return NULL;
    }

    /* Avoiding gpkg prefixes is not an official requirement, but seems wise */
    if (strncmp(pszLayerName, "gpkg", 4) == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The layer name may not begin with 'gpkg' as it is a reserved geopackage prefix");
        return NULL;
    }

    /* Pre-emptively try and avoid sqlite3 syntax errors due to  */
    /* illegal characters */
    if ( strspn(pszLayerName, "`~!@#$%^&*()+-={}|[]\\:\";'<>?,./") > 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The layer name may not contain special characters or spaces");
        return NULL;
    }

    /* Check for any existing layers that already use this name */
    for( iLayer = 0; iLayer < m_nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName, m_papoLayers[iLayer]->GetName()) )
        {
            const char *pszOverwrite = CSLFetchNameValue(papszOptions,"OVERWRITE");
            if( pszOverwrite != NULL && CSLTestBoolean(pszOverwrite) )
            {
                DeleteLayer( iLayer );
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          pszLayerName );
                return NULL;
            }
        }
    }

    /* Read our SRS_ID from the OGRSpatialReference */
    int nSRSId = UNDEFINED_SRID;
    if( poSpatialRef != NULL )
        nSRSId = GetSrsId( poSpatialRef );

    int bIsSpatial = (eGType != wkbNone);

    /* Requirement 25: The geometry_type_name value in a gpkg_geometry_columns */
    /* row SHALL be one of the uppercase geometry type names specified in */
    /* Geometry Types (Normative). */
    const char *pszGeometryType = OGRToOGCGeomType(eGType);
    
    /* Create the table! */
    char *pszSQL = NULL;
    if ( bIsSpatial )
    {
        pszSQL = sqlite3_mprintf(
            "CREATE TABLE \"%s\" ( "
            "\"%s\" INTEGER PRIMARY KEY AUTOINCREMENT, "
            "\"%s\" %s )",
             pszLayerName, pszFIDColumnName, pszGeomColumnName, pszGeometryType);
    }
    else
    {
        pszSQL = sqlite3_mprintf(
            "CREATE TABLE \"%s\" ( "
            "\"%s\" INTEGER PRIMARY KEY AUTOINCREMENT )",
             pszLayerName, pszFIDColumnName);
    }
    
    err = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if ( OGRERR_NONE != err )
        return NULL;

    /* Only spatial tables need to be registered in the metadata (hmmm) */
    if ( bIsSpatial )
    {
        /* Requirement 27: The z value in a gpkg_geometry_columns table row */
        /* SHALL be one of 0 (none), 1 (mandatory), or 2 (optional) */
        int bGeometryTypeHasZ = (wkb25DBit & eGType) != 0;

        /* Update gpkg_geometry_columns with the table info */
        pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_geometry_columns "
            "(table_name,column_name,geometry_type_name,srs_id,z,m)"
            " VALUES "
            "('%q','%q','%q',%d,%d,%d)",
            pszLayerName,pszGeomColumnName,pszGeometryType,
            nSRSId,bGeometryTypeHasZ,0);
    
        err = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if ( err != OGRERR_NONE )
            return NULL;
    }

    /* Update gpkg_contents with the table info */
    char *pszSRSId = NULL;
    if ( !bIsSpatial )
    {
        err = CreateGDALAspatialExtension();
        if ( err != OGRERR_NONE )
            return NULL;
    }
    else
    {
        pszSRSId = sqlite3_mprintf("%d", nSRSId);
    }

    pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_contents "
        "(table_name,data_type,identifier,last_change,srs_id)"
        " VALUES "
        "('%q','%q','%q',strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ',CURRENT_TIMESTAMP),%Q)",
        pszLayerName, (bIsSpatial ? "features": "aspatial"), pszLayerName, pszSRSId);

    err = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if (pszSRSId) {
        sqlite3_free(pszSRSId);
    }
    if ( err != OGRERR_NONE )
        return NULL;

    /* The database is now all set up, so create a blank layer and read in the */
    /* info from the database. */
    OGRGeoPackageTableLayer *poLayer = new OGRGeoPackageTableLayer(this, pszLayerName);
    
    if( OGRERR_NONE != poLayer->ReadTableDefinition(eGType != wkbNone) )
    {
        delete poLayer;
        return NULL;
    }

    /* Should we create a spatial index ? */
    const char *pszSI = CSLFetchNameValue( papszOptions, "SPATIAL_INDEX" );
    int bCreateSpatialIndex = ( pszSI == NULL || CSLTestBoolean(pszSI) );
    if( eGType != wkbNone && bCreateSpatialIndex )
    {
        poLayer->SetDeferedSpatialIndexCreation(TRUE);
    }

    m_papoLayers = (OGRGeoPackageTableLayer**)CPLRealloc(m_papoLayers,  sizeof(OGRGeoPackageTableLayer*) * (m_nLayers+1));
    m_papoLayers[m_nLayers++] = poLayer;
    return poLayer;
}


/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

int OGRGeoPackageDataSource::DeleteLayer( int iLayer )
{
    char *pszSQL;

    if( !bUpdate || iLayer < 0 || iLayer >= m_nLayers )
        return OGRERR_FAILURE;

    CPLString osLayerName = m_papoLayers[iLayer]->GetLayerDefn()->GetName();

    CPLDebug( "GPKG", "DeleteLayer(%s)", osLayerName.c_str() );

    m_papoLayers[iLayer]->DropSpatialIndex();

    /* Delete the layer object and remove the gap in the layers list */
    delete m_papoLayers[iLayer];
    memmove( m_papoLayers + iLayer, m_papoLayers + iLayer + 1,
             sizeof(void *) * (m_nLayers - iLayer - 1) );
    m_nLayers--;

    if (osLayerName.size() == 0)
        return OGRERR_NONE;

    pszSQL = sqlite3_mprintf(
            "DROP TABLE \"%s\"",
             osLayerName.c_str());
    
    SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf(
            "DELETE FROM gpkg_geometry_columns WHERE table_name = '%q'",
             osLayerName.c_str());
    
    SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    
    pszSQL = sqlite3_mprintf(
             "DELETE FROM gpkg_contents WHERE table_name = '%q'",
              osLayerName.c_str());

    SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);

    return OGRERR_NONE;
}



/************************************************************************/
/*                       TestCapability()                               */
/************************************************************************/

int OGRGeoPackageDataSource::TestCapability( const char * pszCap )
{
    if ( EQUAL(pszCap,ODsCCreateLayer) ||
         EQUAL(pszCap,ODsCDeleteLayer) )
    {
         return bUpdate;
    }
    return FALSE;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

static const char* apszFuncsWithSideEffects[] =
{
    "CreateSpatialIndex",
    "DisableSpatialIndex",
};

OGRLayer * OGRGeoPackageDataSource::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )

{
    for( int i = 0; i < m_nLayers; i++ )
    {
        m_papoLayers[i]->CreateSpatialIndexIfNecessary();
    }

    if( pszDialect != NULL && EQUAL(pszDialect,"OGRSQL") )
        return OGRDataSource::ExecuteSQL( pszSQLCommand, 
                                          poSpatialFilter, 
                                          pszDialect );
    else if( pszDialect != NULL && EQUAL(pszDialect,"INDIRECT_SQLITE") )
        return OGRDataSource::ExecuteSQL( pszSQLCommand, 
                                          poSpatialFilter, 
                                          "SQLITE" );

/* -------------------------------------------------------------------- */
/*      Prepare statement.                                              */
/* -------------------------------------------------------------------- */
    int rc;
    sqlite3_stmt *hSQLStmt = NULL;

    CPLString osSQLCommand = pszSQLCommand;

    /* This will speed-up layer creation */
    /* ORDER BY are costly to evaluate and are not necessary to establish */
    /* the layer definition. */
    int bUseStatementForGetNextFeature = TRUE;
    int bEmptyLayer = FALSE;

    if( osSQLCommand.ifind("SELECT ") == 0 &&
        osSQLCommand.ifind(" UNION ") == std::string::npos &&
        osSQLCommand.ifind(" INTERSECT ") == std::string::npos &&
        osSQLCommand.ifind(" EXCEPT ") == std::string::npos )
    {
        size_t nOrderByPos = osSQLCommand.ifind(" ORDER BY ");
        if( nOrderByPos != std::string::npos )
        {
            osSQLCommand.resize(nOrderByPos);
            bUseStatementForGetNextFeature = FALSE;
        }
    }

    rc = sqlite3_prepare( hDB, osSQLCommand.c_str(), osSQLCommand.size(),
                          &hSQLStmt, NULL );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                "In ExecuteSQL(): sqlite3_prepare(%s):\n  %s", 
                pszSQLCommand, sqlite3_errmsg(hDB) );

        if( hSQLStmt != NULL )
        {
            sqlite3_finalize( hSQLStmt );
        }

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we get a resultset?                                          */
/* -------------------------------------------------------------------- */
    rc = sqlite3_step( hSQLStmt );
    if( rc != SQLITE_ROW )
    {
        if ( rc != SQLITE_DONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                  "In ExecuteSQL(): sqlite3_step(%s):\n  %s", 
                  pszSQLCommand, sqlite3_errmsg(hDB) );

            sqlite3_finalize( hSQLStmt );
            return NULL;
        }
        
        if( EQUAL(pszSQLCommand, "VACUUM") )
        {
            sqlite3_finalize( hSQLStmt );
            /* VACUUM rewrites the DB, so we need to reset the application id */
            SetApplicationId();
            return NULL;
        }
        
        if( EQUALN(pszSQLCommand, "ALTER TABLE ", strlen("ALTER TABLE ")) )
        {
            char **papszTokens = CSLTokenizeString( pszSQLCommand );
            /* ALTER TABLE src_table RENAME TO dst_table */
            if( CSLCount(papszTokens) == 6 && EQUAL(papszTokens[3], "RENAME") &&
                EQUAL(papszTokens[4], "TO") )
            {
                const char* pszSrcTableName = papszTokens[2];
                const char* pszDstTableName = papszTokens[5];
                OGRGeoPackageTableLayer* poSrcLayer = (OGRGeoPackageTableLayer*)GetLayerByName(pszSrcTableName);
                if( poSrcLayer )
                {
                    poSrcLayer->RenameTo( pszDstTableName );
                }
            }
            CSLDestroy(papszTokens);
        }

        if( !EQUALN(pszSQLCommand, "SELECT ", 7) )
        {
            sqlite3_finalize( hSQLStmt );
            return NULL;
        }

        bUseStatementForGetNextFeature = FALSE;
        bEmptyLayer = TRUE;
    }
    
/* -------------------------------------------------------------------- */
/*      Special case for some functions which must be run               */
/*      only once                                                       */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand,"SELECT ",7) )
    {
        unsigned int i;
        for(i=0;i<sizeof(apszFuncsWithSideEffects)/
                  sizeof(apszFuncsWithSideEffects[0]);i++)
        {
            if( EQUALN(apszFuncsWithSideEffects[i], pszSQLCommand + 7,
                       strlen(apszFuncsWithSideEffects[i])) )
            {
                if (sqlite3_column_count( hSQLStmt ) == 1 &&
                    sqlite3_column_type( hSQLStmt, 0 ) == SQLITE_INTEGER )
                {
                    int ret = sqlite3_column_int( hSQLStmt, 0 );

                    sqlite3_finalize( hSQLStmt );

                    return new OGRSQLiteSingleFeatureLayer
                                        ( apszFuncsWithSideEffects[i], ret );
                }
            }
        }
    }
    else if( EQUALN(pszSQLCommand,"PRAGMA ",7) )
    {
        if (sqlite3_column_count( hSQLStmt ) == 1 &&
            sqlite3_column_type( hSQLStmt, 0 ) == SQLITE_INTEGER )
        {
            int ret = sqlite3_column_int( hSQLStmt, 0 );

            sqlite3_finalize( hSQLStmt );

            return new OGRSQLiteSingleFeatureLayer
                                ( pszSQLCommand + 7, ret );
        }
        else if (sqlite3_column_count( hSQLStmt ) == 1 &&
                 sqlite3_column_type( hSQLStmt, 0 ) == SQLITE_TEXT )
        {
            const char* pszRet = (const char*) sqlite3_column_text( hSQLStmt, 0 );

            OGRLayer* poRet = new OGRSQLiteSingleFeatureLayer
                                ( pszSQLCommand + 7, pszRet );

            sqlite3_finalize( hSQLStmt );

            return poRet;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create layer.                                                   */
/* -------------------------------------------------------------------- */
    OGRLayer *poLayer = NULL;

    CPLString osSQL = pszSQLCommand;
    poLayer = new OGRGeoPackageSelectLayer( this, osSQL, hSQLStmt,
                                        bUseStatementForGetNextFeature, bEmptyLayer );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( 0, poSpatialFilter );

    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRGeoPackageDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                         HasExtensionsTable()                         */
/************************************************************************/

int OGRGeoPackageDataSource::HasExtensionsTable()
{
    SQLResult oResultTable;
    OGRErr err = SQLQuery(hDB,
        "SELECT * FROM sqlite_master WHERE name = 'gpkg_extensions' "
        "AND type IN ('table', 'view')", &oResultTable);
    int bHasExtensionsTable = ( err == OGRERR_NONE && oResultTable.nRowCount == 1 );
    SQLResultFree(&oResultTable);
    return bHasExtensionsTable;
}

/************************************************************************/
/*                         HasGDALAspatialExtension()                       */
/************************************************************************/

int OGRGeoPackageDataSource::HasGDALAspatialExtension()
{
    if (!HasExtensionsTable())
        return 0;

    SQLResult oResultTable;
    OGRErr err = SQLQuery(hDB,
        "SELECT * FROM gpkg_extensions "
        "WHERE extension_name = 'gdal_aspatial' "
        "AND table_name IS NULL "
        "AND column_name IS NULL", &oResultTable);
    int bHasExtension = ( err == OGRERR_NONE && oResultTable.nRowCount == 1 );
    SQLResultFree(&oResultTable);
    return bHasExtension;
}

/************************************************************************/
/*                  CreateGDALAspatialExtension()                       */
/************************************************************************/

OGRErr OGRGeoPackageDataSource::CreateGDALAspatialExtension()
{
    CreateExtensionsTableIfNecessary();

    const char* pszCreateAspatialExtension =
        "INSERT OR REPLACE INTO gpkg_extensions "
        "(table_name, column_name, extension_name, definition, scope) "
        "VALUES "
        "(NULL, NULL, 'gdal_aspatial', 'http://gdal.org/geopackage_aspatial.html', 'read-write')";

    return SQLCommand(hDB, pszCreateAspatialExtension);
}

/************************************************************************/
/*                  CreateExtensionsTableIfNecessary()                  */
/************************************************************************/

OGRErr OGRGeoPackageDataSource::CreateExtensionsTableIfNecessary()
{
    /* Check if the table gpkg_extensions exists */
    if( HasExtensionsTable() )
        return OGRERR_NONE;

    /* Requirement 79 : Every extension of a GeoPackage SHALL be registered */
    /* in a corresponding row in the gpkg_extensions table. The absence of a */
    /* gpkg_extensions table or the absence of rows in gpkg_extnsions table */
    /* SHALL both indicate the absence of extensions to a GeoPackage. */
    const char* pszCreateGpkgExtensions = 
        "CREATE TABLE gpkg_extensions ("
        "table_name TEXT,"
        "column_name TEXT,"
        "extension_name TEXT NOT NULL,"
        "definition TEXT NOT NULL,"
        "scope TEXT NOT NULL,"
        "CONSTRAINT ge_tce UNIQUE (table_name, column_name, extension_name)"
        ")";

    return SQLCommand(hDB, pszCreateGpkgExtensions);
}

/************************************************************************/
/*                     OGRGeoPackageGetHeader()                         */
/************************************************************************/

static int OGRGeoPackageGetHeader(sqlite3_context* pContext,
                                  CPL_UNUSED int argc,
                                  sqlite3_value** argv,
                                  GPkgHeader* psHeader,
                                  int bNeedExtent)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        sqlite3_result_null(pContext);
        return FALSE;
    }
    int nBLOBLen = sqlite3_value_bytes (argv[0]);
    const GByte* pabyBLOB = (const GByte *) sqlite3_value_blob (argv[0]);
    if( nBLOBLen < 4 ||
        GPkgHeaderFromWKB(pabyBLOB, psHeader) != OGRERR_NONE )
    {
        sqlite3_result_null(pContext);
        return FALSE;
    }
    if( psHeader->iDims == 0 && bNeedExtent )
    {
        OGRGeometry *poGeom = GPkgGeometryToOGR(pabyBLOB, nBLOBLen, NULL);
        if( poGeom == NULL || poGeom->IsEmpty() )
        {
            sqlite3_result_null(pContext);
            delete poGeom;
            return FALSE;
        }
        OGREnvelope sEnvelope;
        poGeom->getEnvelope(&sEnvelope);
        psHeader->MinX = sEnvelope.MinX;
        psHeader->MaxX = sEnvelope.MaxX;
        psHeader->MinY = sEnvelope.MinY;
        psHeader->MaxY = sEnvelope.MaxY;
        delete poGeom;
    }
    return TRUE;
}

/************************************************************************/
/*                      OGRGeoPackageSTMinX()                           */
/************************************************************************/

static
void OGRGeoPackageSTMinX(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, TRUE) )
        return;
    sqlite3_result_double( pContext, sHeader.MinX );
}

/************************************************************************/
/*                      OGRGeoPackageSTMinY()                           */
/************************************************************************/

static
void OGRGeoPackageSTMinY(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, TRUE) )
        return;
    sqlite3_result_double( pContext, sHeader.MinY );
}

/************************************************************************/
/*                      OGRGeoPackageSTMaxX()                           */
/************************************************************************/

static
void OGRGeoPackageSTMaxX(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, TRUE) )
        return;
    sqlite3_result_double( pContext, sHeader.MaxX );
}

/************************************************************************/
/*                      OGRGeoPackageSTMaxY()                           */
/************************************************************************/

static
void OGRGeoPackageSTMaxY(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, TRUE) )
        return;
    sqlite3_result_double( pContext, sHeader.MaxY );
}

/************************************************************************/
/*                     OGRGeoPackageSTIsEmpty()                         */
/************************************************************************/

static
void OGRGeoPackageSTIsEmpty(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, FALSE) )
        return;
    sqlite3_result_int( pContext, sHeader.bEmpty );
}

/************************************************************************/
/*                    OGRGeoPackageSTGeometryType()                     */
/************************************************************************/

static
void OGRGeoPackageSTGeometryType(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, FALSE) )
        return;

    int nBLOBLen = sqlite3_value_bytes (argv[0]);
    const GByte* pabyBLOB = (const GByte *) sqlite3_value_blob (argv[0]);
    OGRBoolean bIs3D;
    OGRwkbGeometryType eGeometryType;
    if( nBLOBLen <= (int)sHeader.szHeader )
    {
        sqlite3_result_null( pContext );
        return;
    }
    OGRErr err = OGRReadWKBGeometryType( (GByte*)pabyBLOB + sHeader.szHeader, &eGeometryType, &bIs3D );
    if( err != OGRERR_NONE )
        sqlite3_result_null( pContext );
    else
        sqlite3_result_text( pContext, OGRToOGCGeomType(eGeometryType), -1, SQLITE_TRANSIENT );
}

/************************************************************************/
/*                    OGRGeoPackageGPKGIsAssignable()                   */
/************************************************************************/

static
void OGRGeoPackageGPKGIsAssignable(sqlite3_context* pContext,
                                   CPL_UNUSED int argc,
                                   sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type (argv[1]) != SQLITE_TEXT )
    {
        sqlite3_result_int( pContext, 0 );
        return;
    }

    const char* pszExpected = (const char*)sqlite3_value_text(argv[0]);
    const char* pszActual = (const char*)sqlite3_value_text(argv[1]);

    if( EQUAL(pszExpected, pszActual) ||
        EQUAL(pszExpected, "GEOMETRY") ||
        (EQUAL(pszExpected, "GEOMETRYCOLLECTION") &&
         (EQUAL(pszActual, "MULTIPOINT") ||
          EQUAL(pszActual, "MULTILINESTRING") ||
          EQUAL(pszActual, "MULTIPOLYGON"))) )
    {
        sqlite3_result_int( pContext, 1 );
        return;
    }
    
    sqlite3_result_int( pContext, 0 );
}

/************************************************************************/
/*                     OGRGeoPackageSTSRID()                            */
/************************************************************************/

static
void OGRGeoPackageSTSRID(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    GPkgHeader sHeader;
    if( !OGRGeoPackageGetHeader(pContext, argc, argv, &sHeader, FALSE) )
        return;
    sqlite3_result_int( pContext, sHeader.iSrsId );
}

/************************************************************************/
/*                  OGRGeoPackageCreateSpatialIndex()                   */
/************************************************************************/

static
void OGRGeoPackageCreateSpatialIndex(sqlite3_context* pContext,
                                     CPL_UNUSED int argc,
                                     sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type (argv[1]) != SQLITE_TEXT )
    {
        sqlite3_result_int( pContext, 0 );
        return;
    }

    const char* pszTableName = (const char*)sqlite3_value_text(argv[0]);
    const char* pszGeomName = (const char*)sqlite3_value_text(argv[1]);
    OGRGeoPackageDataSource* poDS = (OGRGeoPackageDataSource* )sqlite3_user_data(pContext);
    
    OGRGeoPackageTableLayer* poLyr = (OGRGeoPackageTableLayer*)poDS->GetLayerByName(pszTableName);
    if( poLyr == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer name");
        sqlite3_result_int( pContext, 0 );
        return;
    }
    if( !EQUAL(poLyr->GetGeometryColumn(), pszGeomName) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown geometry column name");
        sqlite3_result_int( pContext, 0 );
        return;
    }

    sqlite3_result_int( pContext, poLyr->CreateSpatialIndex() );
}

/************************************************************************/
/*                  OGRGeoPackageDisableSpatialIndex()                  */
/************************************************************************/

static
void OGRGeoPackageDisableSpatialIndex(sqlite3_context* pContext,
                                      CPL_UNUSED int argc,
                                      sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type (argv[1]) != SQLITE_TEXT )
    {
        sqlite3_result_int( pContext, 0 );
        return;
    }

    const char* pszTableName = (const char*)sqlite3_value_text(argv[0]);
    const char* pszGeomName = (const char*)sqlite3_value_text(argv[1]);
    OGRGeoPackageDataSource* poDS = (OGRGeoPackageDataSource* )sqlite3_user_data(pContext);
    
    OGRGeoPackageTableLayer* poLyr = (OGRGeoPackageTableLayer*)poDS->GetLayerByName(pszTableName);
    if( poLyr == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer name");
        sqlite3_result_int( pContext, 0 );
        return;
    }
    if( !EQUAL(poLyr->GetGeometryColumn(), pszGeomName) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown geometry column name");
        sqlite3_result_int( pContext, 0 );
        return;
    }

    sqlite3_result_int( pContext, poLyr->DropSpatialIndex(TRUE) );
}

/************************************************************************/
/*                       GPKG_hstore_get_value()                        */
/************************************************************************/

static
void GPKG_hstore_get_value(sqlite3_context* pContext,
                           CPL_UNUSED int argc,
                           sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type (argv[1]) != SQLITE_TEXT )
    {
        sqlite3_result_null (pContext);
        return;
    }

    const char* pszHStore = (const char*)sqlite3_value_text(argv[0]);
    const char* pszSearchedKey = (const char*)sqlite3_value_text(argv[1]);
    char* pszValue = OGRHStoreGetValue(pszHStore, pszSearchedKey);
    if( pszValue != NULL )
        sqlite3_result_text( pContext, pszValue, -1, CPLFree );
    else
        sqlite3_result_null( pContext );
}

/************************************************************************/
/*                         OpenOrCreateDB()                             */
/************************************************************************/

int OGRGeoPackageDataSource::OpenOrCreateDB(int flags)
{
    int bSuccess = OGRSQLiteBaseDataSource::OpenOrCreateDB(flags, FALSE);
    if( !bSuccess )
        return FALSE;

#ifdef SPATIALITE_412_OR_LATER
    InitNewSpatialite();
#endif

    /* Used by RTree Spatial Index Extension */
    sqlite3_create_function(hDB, "ST_MinX", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTMinX, NULL, NULL);
    sqlite3_create_function(hDB, "ST_MinY", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTMinY, NULL, NULL);
    sqlite3_create_function(hDB, "ST_MaxX", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTMaxX, NULL, NULL);
    sqlite3_create_function(hDB, "ST_MaxY", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTMaxY, NULL, NULL);
    sqlite3_create_function(hDB, "ST_IsEmpty", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTIsEmpty, NULL, NULL);

    /* Used by Geometry Type Triggers Extension */
    sqlite3_create_function(hDB, "ST_GeometryType", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTGeometryType, NULL, NULL);
    sqlite3_create_function(hDB, "GPKG_IsAssignable", 2, SQLITE_ANY, NULL,
                            OGRGeoPackageGPKGIsAssignable, NULL, NULL);

    /* Used by Geometry SRS ID Triggers Extension */
    sqlite3_create_function(hDB, "ST_SRID", 1, SQLITE_ANY, NULL,
                            OGRGeoPackageSTSRID, NULL, NULL);

    /* Spatialite-like functions */
    sqlite3_create_function(hDB, "CreateSpatialIndex", 2, SQLITE_ANY, this,
                            OGRGeoPackageCreateSpatialIndex, NULL, NULL);
    sqlite3_create_function(hDB, "DisableSpatialIndex", 2, SQLITE_ANY, this,
                            OGRGeoPackageDisableSpatialIndex, NULL, NULL);

    // HSTORE functions
    sqlite3_create_function(hDB, "hstore_get_value", 2, SQLITE_ANY, NULL,
                            GPKG_hstore_get_value, NULL, NULL);

    return TRUE;
}

/************************************************************************/
/*                   GetLayerWithGetSpatialWhereByName()                */
/************************************************************************/

std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>
    OGRGeoPackageDataSource::GetLayerWithGetSpatialWhereByName( const char* pszName )
{
    OGRGeoPackageLayer* poRet = (OGRGeoPackageLayer*) GetLayerByName(pszName);
    return std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>(poRet, poRet);
}
