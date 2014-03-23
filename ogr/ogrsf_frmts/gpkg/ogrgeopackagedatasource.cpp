/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements OGRGeoPackageDataSource class
 * Author:   Paul Ramsey <pramsey@boundlessgeo.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
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


/* Cannnot count on the "PRAGMA application_id" command existing */
/* it is a very recent addition to SQLite. */
bool OGRGeoPackageDataSource::CheckApplicationId(const char * pszFileName)
{
    CPLAssert( m_poDb == NULL );
    
    /* "GP10" in ASCII bytes */
    static char aGpkgId[4] = {0x47, 0x50, 0x31, 0x30};
    static size_t szGpkgIdPos = 68;
    char aFileId[4];

    VSILFILE *fp = VSIFOpenL( pszFileName, "rb" );

    /* Should never happen (always called after existence check) but just in case */
    if ( ! fp ) return FALSE;

    /* application_id is 4 bytes at offset 68 in the header */
    VSIFSeekL(fp, szGpkgIdPos, SEEK_SET);
    VSIFReadL(aFileId, 4, 1, fp);

    VSIFCloseL(fp);
    
    for ( int i = 0; i < 4; i++ )
    {
        if ( aFileId[i] != aGpkgId[i] )
            return FALSE;
    }
    return TRUE;
}

/* Only recent versions of SQLite will let us muck with application_id */
/* via a PRAGMA statement, so we have to write directly into the */
/* file header here. */
/* We do this at the *end* of initialization so that there is */
/* data to write down to a file, and we'll have a writeable file */
/* once we close the SQLite connection */
OGRErr OGRGeoPackageDataSource::SetApplicationId()
{
    CPLAssert( m_poDb != NULL );
    CPLAssert( m_pszFileName != NULL );

    /* Have to flush the file before f***ing with the header */
    sqlite3_close(m_poDb);

    /* "GP10" */
    static char aGpkgId[4] = {0x47, 0x50, 0x31, 0x30};
    static size_t szGpkgIdPos = 68;
    size_t szWritten = 0;

    /* Open for modification, write to application id area */
    VSILFILE *pfFile = VSIFOpenL( m_pszFileName, "rb+" );
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
    if ( sqlite3_open(m_pszFileName, &m_poDb) != SQLITE_OK )
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
        m_poDb,
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
    
    CPLString oSQL;
    oSQL.Printf("SELECT definition FROM gpkg_spatial_ref_sys WHERE srs_id = %d", iSrsId);
    
    OGRErr err = SQLQuery(m_poDb, oSQL.c_str(), &oResult);

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

    poSRS->morphFromESRI();
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
        
        nSRSId = SQLGetInteger(m_poDb, pszSQL, &err);
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
        if ( ! SQLGetInteger(m_poDb, pszSQL, &err) && err == OGRERR_NONE )
            bCanUseAuthorityCode = TRUE;
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
        int nMaxSRSId = SQLGetInteger(m_poDb, "SELECT MAX(srs_id) FROM gpkg_spatial_ref_sys", &err);
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
                 "VALUES ('%s', %d, upper('%s'), %d, '%q')",
                 GetSrsName(poSRS), nSRSId, pszAuthorityName, nAuthorityCode, pszWKT
                 );
    }
    else
    {
        pszSQL = sqlite3_mprintf(
                 "INSERT INTO gpkg_spatial_ref_sys "
                 "(srs_name,srs_id,organization,organization_coordsys_id,definition) "
                 "VALUES ('%s', %d, upper('%s'), %d, '%q')",
                 GetSrsName(poSRS), nSRSId, "NONE", nSRSId, pszWKT
                 );
    }

    // Add new row to gpkg_spatial_ref_sys
    err = SQLCommand(m_poDb, pszSQL);

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
    m_pszFileName = NULL;
    m_papoLayers = NULL;
    m_nLayers = 0;
    m_bUtf8 = FALSE;
    m_poDb = NULL;
    m_bUpdate = FALSE;
}

/************************************************************************/
/*                       ~OGRGeoPackageDataSource()                     */
/************************************************************************/

OGRGeoPackageDataSource::~OGRGeoPackageDataSource()
{
    for( int i = 0; i < m_nLayers; i++ )
        delete m_papoLayers[i];
        
    if ( m_poDb )
        sqlite3_close(m_poDb);

    CPLFree( m_papoLayers );
    CPLFree( m_pszFileName );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGeoPackageDataSource::Open(const char * pszFilename, int bUpdate )
{
    int i;
    OGRErr err;

    CPLAssert( m_nLayers == 0 );
    CPLAssert( m_poDb == NULL );
    CPLAssert( m_pszFileName == NULL );

    m_bUpdate = bUpdate;

    /* Requirement 3: File name has to end in "gpkg" */
    /* http://opengis.github.io/geopackage/#_file_extension_name */
    int nLen = strlen(pszFilename);
    if(! (nLen >= 5 && EQUAL(pszFilename + nLen - 5, ".gpkg")) )
        return FALSE;

    /* Check that the filename exists and is a file */
    VSIStatBuf stat;
    if( CPLStat( pszFilename, &stat ) != 0 || !VSI_ISREG(stat.st_mode) )
        return FALSE;

    /* Requirement 2: A GeoPackage SHALL contain 0x47503130 ("GP10" in ASCII) */
    /* in the application id */
    /* http://opengis.github.io/geopackage/#_file_format */
    if ( ! CheckApplicationId(pszFilename) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "bad application_id on '%s'", pszFilename);
        return FALSE;
    }

    /* See if we can open the SQLite database */
    int rc = sqlite3_open( pszFilename, &m_poDb );
    if ( rc != SQLITE_OK )
    {
        m_poDb = NULL;
        CPLError( CE_Failure, CPLE_OpenFailed, "sqlite3_open(%s) failed: %s",
                  pszFilename, sqlite3_errmsg( m_poDb ) );
        return FALSE;
    }
    
    /* Filename is good, store it for future reference */
    m_pszFileName = CPLStrdup( pszFilename );

    /* Requirement 6: The SQLite PRAGMA integrity_check SQL command SHALL return “ok” */
    /* http://opengis.github.io/geopackage/#_file_integrity */
    if ( OGRERR_NONE != PragmaCheck("integrity_check", "ok", 1) )
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
        char *pszSQL = sqlite3_mprintf("pragma table_info('%s')", aosGpkgTables[i].c_str());
        err = SQLQuery(m_poDb, pszSQL, &oResult);
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
    SQLResult oResult;
    std::string osSQL = 
        "SELECT c.table_name, c.identifier, c.min_x, c.min_y, c.max_x, c.max_y "
        "FROM gpkg_geometry_columns g JOIN gpkg_contents c ON (g.table_name = c.table_name)"
        "WHERE c.data_type = 'features'";
        
    err = SQLQuery(m_poDb, osSQL.c_str(), &oResult);
    if  ( err != OGRERR_NONE )
    {
        SQLResultFree(&oResult);
        return FALSE;
    }

    if ( oResult.nRowCount > 0 )
    {
        m_papoLayers = (OGRLayer**)CPLMalloc(sizeof(OGRGeoPackageLayer*) * oResult.nRowCount);

        for ( i = 0; i < oResult.nRowCount; i++ )
        {
            const char *pszTableName = SQLResultGetValue(&oResult, 0, i);
            if ( ! pszTableName )
            {
                CPLError(CE_Warning, CPLE_AppDefined, "unable to read table name for layer(%d)", i);            
                continue;
            }
            OGRGeoPackageLayer *poLayer = new OGRGeoPackageLayer(this, pszTableName);
            if( OGRERR_NONE != poLayer->ReadTableDefinition() )
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
/*                          GetDatabaseHandle()                         */
/************************************************************************/

sqlite3* OGRGeoPackageDataSource::GetDatabaseHandle()
{
    return m_poDb;
}

/************************************************************************/
/*                                Create()                              */
/************************************************************************/

int OGRGeoPackageDataSource::Create( const char * pszFilename, char **papszOptions )
{
    CPLString osCommand;
    const char *pszSpatialRefSysRecord;

	/* The OGRGeoPackageDriver has already confirmed that the pszFilename */
	/* is not already in use, so try to create the file */
    int rc = sqlite3_open( pszFilename, &m_poDb );
    if ( rc != SQLITE_OK )
    {
        m_poDb = NULL;
        CPLError( CE_Failure, CPLE_OpenFailed, "sqlite3_open(%s) failed: %s",
                  pszFilename, sqlite3_errmsg( m_poDb ) );
        return FALSE;
    }

    m_pszFileName = CPLStrdup(pszFilename);
    m_bUpdate = TRUE;

    /* OGR UTF-8 support. If we set the UTF-8 Pragma early on, it */
    /* will be written into the main file and supported henceforth */
    SQLCommand(m_poDb, "PRAGMA encoding = \"UTF-8\"");

    /* Requirement 2: A GeoPackage SHALL contain 0x47503130 ("GP10" in ASCII) in the application id */
    /* http://opengis.github.io/geopackage/#_file_format */
    const char *pszPragma = CPLSPrintf("PRAGMA application_id = %d", GPKG_APPLICATION_ID);
    
    if ( OGRERR_NONE != SQLCommand(m_poDb, pszPragma) )
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
        
    if ( OGRERR_NONE != SQLCommand(m_poDb, pszSpatialRefSys) )
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
          
    if ( OGRERR_NONE != SQLCommand(m_poDb, pszSpatialRefSysRecord) )
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
           
    if ( OGRERR_NONE != SQLCommand(m_poDb, pszSpatialRefSysRecord) )
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
           
    if ( OGRERR_NONE != SQLCommand(m_poDb, pszSpatialRefSysRecord) )
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
        
    if ( OGRERR_NONE != SQLCommand(m_poDb, pszContents) )
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
        
    if ( OGRERR_NONE != SQLCommand(m_poDb, pszGeometryColumns) )
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
    
    pszSQL = sqlite3_mprintf("ALTER TABLE %s ADD COLUMN %s %s", 
                             pszTableName, pszColumnName, pszColumnType);

    OGRErr err = SQLCommand(m_poDb, pszSQL);
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
/*                           CreateLayer()                              */
/* Options:                                                             */
/*   FID = primary key name                                             */
/*   OVERWRITE = YES|NO, overwrite existing layer?                      */
/*   SPATIAL_INDEX = YES|NO, TBD                                        */
/************************************************************************/

OGRLayer* OGRGeoPackageDataSource::CreateLayer( const char * pszLayerName,
                                      OGRSpatialReference * poSpatialRef,
                                      OGRwkbGeometryType eGType,
                                      char **papszOptions )
{
    int iLayer;
    OGRErr err;
    
    if( !m_bUpdate )
        return NULL;

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
        
    /* Requirement 25: The geometry_type_name value in a gpkg_geometry_columns */
    /* row SHALL be one of the uppercase geometry type names specified in */
    /* Geometry Types (Normative). */
    const char *pszGeometryType = OGRToOGCGeomType(eGType);
    
    /* Create the table! */
    char *pszSQL = NULL;
    if ( eGType != wkbNone )
    {
        pszSQL = sqlite3_mprintf(
            "CREATE TABLE %s ( "
            "%s INTEGER PRIMARY KEY AUTOINCREMENT, "
            "%s %s )",
             pszLayerName, pszFIDColumnName, pszGeomColumnName, pszGeometryType);
    }
    else
    {
        pszSQL = sqlite3_mprintf(
            "CREATE TABLE %s ( "
            "%s INTEGER PRIMARY KEY AUTOINCREMENT )",
             pszLayerName, pszFIDColumnName);
    }
    
    err = SQLCommand(m_poDb, pszSQL);
    sqlite3_free(pszSQL);
    if ( OGRERR_NONE != err )
        return NULL;

    /* Only spatial tables need to be registered in the metadata (hmmm) */
    if ( eGType != wkbNone )
    {
        /* Requirement 27: The z value in a gpkg_geometry_columns table row */
        /* SHALL be one of 0 (none), 1 (mandatory), or 2 (optional) */
        int bGeometryTypeHasZ = wkb25DBit & eGType;

        /* Update gpkg_geometry_columns with the table info */
        pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_geometry_columns "
            "(table_name,column_name,geometry_type_name,srs_id,z,m)"
            " VALUES "
            "('%q','%q','%q',%d,%d,%d)",
            pszLayerName,pszGeomColumnName,pszGeometryType,
            nSRSId,bGeometryTypeHasZ,0);
    
        err = SQLCommand(m_poDb, pszSQL);
        sqlite3_free(pszSQL);
        if ( err != OGRERR_NONE )
            return NULL;

        /* Update gpkg_contents with the table info */
        pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_contents "
            "(table_name,data_type,identifier,last_change,srs_id)"
            " VALUES "
            "('%q','features','%q',strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ',CURRENT_TIMESTAMP),%d)",
            pszLayerName, pszLayerName, nSRSId);
    
        err = SQLCommand(m_poDb, pszSQL);
        sqlite3_free(pszSQL);
        if ( err != OGRERR_NONE )
            return NULL;

    }

    /* This is where spatial index logic will go in the future */
    const char *pszSI = CSLFetchNameValue( papszOptions, "SPATIAL_INDEX" );
    int bCreateSpatialIndex = ( pszSI == NULL || CSLTestBoolean(pszSI) );
    if( eGType != wkbNone && bCreateSpatialIndex )
    {
        /* This is where spatial index logic will go in the future */
    }
    
    /* The database is now all set up, so create a blank layer and read in the */
    /* info from the database. */
    OGRGeoPackageLayer *poLayer = new OGRGeoPackageLayer(this, pszLayerName);
    
    if( OGRERR_NONE != poLayer->ReadTableDefinition() )
    {
        delete poLayer;
        return NULL;
    }

    m_papoLayers = (OGRLayer**)CPLRealloc(m_papoLayers,  sizeof(OGRGeoPackageLayer*) * (m_nLayers+1));
    m_papoLayers[m_nLayers++] = poLayer;
    return poLayer;
}


/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

int OGRGeoPackageDataSource::DeleteLayer( int iLayer )
{
    char *pszSQL;

    if( !m_bUpdate || iLayer < 0 || iLayer >= m_nLayers )
        return OGRERR_FAILURE;

    CPLString osLayerName = m_papoLayers[iLayer]->GetLayerDefn()->GetName();

    CPLDebug( "GPKG", "DeleteLayer(%s)", osLayerName.c_str() );

    /* Delete the layer object and remove the gap in the layers list */
    delete m_papoLayers[iLayer];
    memmove( m_papoLayers + iLayer, m_papoLayers + iLayer + 1,
             sizeof(void *) * (m_nLayers - iLayer - 1) );
    m_nLayers--;

    if (osLayerName.size() == 0)
        return OGRERR_NONE;

    pszSQL = sqlite3_mprintf(
            "DROP TABLE %s",
             osLayerName.c_str());
    
    SQLCommand(m_poDb, pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf(
            "DELETE FROM gpkg_geometry_columns WHERE table_name = '%s'",
             osLayerName.c_str());
    
    SQLCommand(m_poDb, pszSQL);
    sqlite3_free(pszSQL);
    
    pszSQL = sqlite3_mprintf(
             "DELETE FROM gpkg_contents WHERE table_name = '%s'",
              osLayerName.c_str());

    SQLCommand(m_poDb, pszSQL);
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
         return m_bUpdate;
    }
    return FALSE;
}