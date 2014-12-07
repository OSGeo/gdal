/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements GDALGeoPackageDataset class
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
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_geopackage.h"
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
OGRErr GDALGeoPackageDataset::SetApplicationId()
{
    CPLAssert( hDB != NULL );
    CPLAssert( m_pszFilename != NULL );

    /* Have to flush the file before f***ing with the header */
    CloseDB();

    size_t szWritten = 0;

    /* Open for modification, write to application id area */
    VSILFILE *pfFile = VSIFOpenL( m_pszFilename, "rb+" );
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
OGRErr GDALGeoPackageDataset::PragmaCheck(const char * pszPragma, const char * pszExpected, int nRowsExpected)
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


OGRSpatialReference* GDALGeoPackageDataset::GetSpatialRef(int iSrsId)
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

const char * GDALGeoPackageDataset::GetSrsName(const OGRSpatialReference * poSRS)
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

int GDALGeoPackageDataset::GetSrsId(const OGRSpatialReference * cpoSRS)
{
    char *pszWKT = NULL;
    char *pszSQL = NULL;
    int nSRSId = DEFAULT_SRID;
    const char* pszAuthorityName;
    int nAuthorityCode = 0;
    OGRErr err;
    OGRBoolean bCanUseAuthorityCode = FALSE;

    if( cpoSRS == NULL )
        return DEFAULT_SRID;

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
        return DEFAULT_SRID;
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
            return DEFAULT_SRID;        
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
/*                        GDALGeoPackageDataset()                       */
/************************************************************************/

GDALGeoPackageDataset::GDALGeoPackageDataset()
{
    m_bNew = FALSE;
    m_papoLayers = NULL;
    m_nLayers = 0;
    m_bUtf8 = FALSE;
    m_bIdentifierAsCO = FALSE;
    m_bDescriptionAsCO = FALSE;
    m_bHasReadMetadataFromStorage = FALSE;
    m_bMetadataDirty = FALSE;
    m_papszSubDatasets = NULL;
    m_pszProjection = NULL;
    m_bGeoTransformValid = FALSE;
    m_nSRID = UNKNOWN_SRID;
    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;
    m_nZoomLevel = 0;
    m_pabyCachedTiles = NULL;
    for(int i=0;i<4;i++)
    {
        m_asCachedTilesDesc[i].nRow = -1;
        m_asCachedTilesDesc[i].nCol = -1;
        m_asCachedTilesDesc[i].nIdxWithinTileData = -1;
        m_asCachedTilesDesc[i].abBandDirty[0] = FALSE;
        m_asCachedTilesDesc[i].abBandDirty[1] = FALSE;
        m_asCachedTilesDesc[i].abBandDirty[2] = FALSE;
        m_asCachedTilesDesc[i].abBandDirty[3] = FALSE;
    }
    m_nShiftXTiles = 0;
    m_nShiftXPixelsMod = 0;
    m_nShiftYTiles = 0;
    m_nShiftYPixelsMod = 0;
    m_eTF = GPKG_TF_PNG_JPEG;
    m_nZLevel = 6;
    m_nQuality = 75;
    m_bDither = FALSE;
    m_poParentDS = NULL;
    m_nOverviewCount = 0;
    m_papoOverviewDS = NULL;
    m_bZoomOther = FALSE;
    m_bTriedEstablishingCT = FALSE;
    m_pabyHugeColorArray = NULL;
    m_poCT = NULL;
    m_bInWriteTile = FALSE;
}

/************************************************************************/
/*                       ~GDALGeoPackageDataset()                       */
/************************************************************************/

GDALGeoPackageDataset::~GDALGeoPackageDataset()
{
    int i;
    
    SetPamFlags(0);

    if( m_poParentDS == NULL && m_osRasterTable.size() &&
        (!m_bGeoTransformValid || m_nSRID == UNKNOWN_SRID) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Raster table %s not correctly initialized due to missing call "
                 "to SetGeoTransform() and/or SetProjection()",
                 m_osRasterTable.c_str());
    }
    
    FlushCache();
    FlushMetadata();

    if( m_poParentDS != NULL )
        hDB = NULL;
    
    for( i = 0; i < m_nLayers; i++ )
        delete m_papoLayers[i];
    for( i = 0; i < m_nOverviewCount; i++ )
        delete m_papoOverviewDS[i];

    CPLFree( m_papoLayers );
    CPLFree( m_papoOverviewDS );
    CSLDestroy( m_papszSubDatasets );
    CPLFree(m_pszProjection);
    CPLFree(m_pabyCachedTiles);
    delete m_poCT;
    CPLFree(m_pabyHugeColorArray);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int GDALGeoPackageDataset::Open( GDALOpenInfo* poOpenInfo )
{
    int i;
    OGRErr err;

    CPLAssert( m_nLayers == 0 );
    CPLAssert( hDB == NULL );
    
    SetDescription( poOpenInfo->pszFilename );
    CPLString osFilename( poOpenInfo->pszFilename );
    CPLString osSubdatasetTableName;
    if( EQUALN(poOpenInfo->pszFilename, "GPKG:", 5) )
    {
        char** papszTokens = CSLTokenizeString2(poOpenInfo->pszFilename, ":", 0);
        if( CSLCount(papszTokens) != 3 )
        {
            CSLDestroy(papszTokens);
            return FALSE;
        }

        osFilename = papszTokens[1];
        osSubdatasetTableName = papszTokens[2];

        CSLDestroy(papszTokens);
    }

    bUpdate = poOpenInfo->eAccess == GA_Update;
    eAccess = poOpenInfo->eAccess; /* hum annoying duplication */
    m_pszFilename = CPLStrdup( osFilename );

    /* See if we can open the SQLite database */
#ifdef HAVE_SQLITE_VFS
    if (!OpenOrCreateDB((bUpdate) ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY) )
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
        CPLError( CE_Failure, CPLE_AppDefined, "pragma integrity_check on '%s' failed",
                  m_pszFilename);
        return FALSE;
    }
    
    /* Requirement 7: The SQLite PRAGMA foreign_key_check() SQL with no */
    /* parameter value SHALL return an empty result set */
    /* http://opengis.github.io/geopackage/#_file_integrity */
    if ( OGRERR_NONE != PragmaCheck("foreign_key_check", "", 0) ) 
    {
        CPLError( CE_Failure, CPLE_AppDefined, "pragma foreign_key_check on '%s' failed",
                  m_pszFilename);
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
    static std::string aosGpkgTables[] = {
        "gpkg_spatial_ref_sys",
        "gpkg_contents"
    };
    
    for ( i = 0; i < (int)(sizeof(aosGpkgTables) / sizeof(aosGpkgTables[0])); i++ )
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

    CheckUnknownExtensions();

    int bRet = FALSE;
    if( poOpenInfo->nOpenFlags & GDAL_OF_VECTOR )
    {
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
        bRet = TRUE;
    }
    
    if(  poOpenInfo->nOpenFlags & GDAL_OF_RASTER )
    {
        SQLResult oResult;
        std::string osSQL =
            "SELECT c.table_name, c.identifier, c.description, c.srs_id, c.min_x, c.min_y, c.max_x, c.max_y, "
            "tms.min_x, tms.min_y, tms.max_x, tms.max_y FROM gpkg_contents c JOIN gpkg_tile_matrix_set tms ON "
            "c.table_name = tms.table_name WHERE data_type = 'tiles'";
        if( CSLFetchNameValue( poOpenInfo->papszOpenOptions, "TABLE") )
            osSubdatasetTableName = CSLFetchNameValue( poOpenInfo->papszOpenOptions, "TABLE");
        if( osSubdatasetTableName.size() )
        {
            char* pszTmp = sqlite3_mprintf(" AND c.table_name='%q'", osSubdatasetTableName.c_str());
            osSQL += pszTmp;
            sqlite3_free(pszTmp);
            SetPhysicalFilename( osFilename.c_str() );
        }

        err = SQLQuery(hDB, osSQL.c_str(), &oResult);
        if  ( err != OGRERR_NONE )
        {
            SQLResultFree(&oResult);
            return FALSE;
        }

        if( oResult.nRowCount == 1 )
        {
            const char *pszTableName = SQLResultGetValue(&oResult, 0, 0);
            const char* pszIdentifier = SQLResultGetValue(&oResult, 1, 0);
            const char* pszDescription = SQLResultGetValue(&oResult, 2, 0);
            const char* pszSRSId = SQLResultGetValue(&oResult, 3, 0);
            const char* pszMinX = SQLResultGetValue(&oResult, 4, 0);
            const char* pszMinY = SQLResultGetValue(&oResult, 5, 0);
            const char* pszMaxX = SQLResultGetValue(&oResult, 6, 0);
            const char* pszMaxY = SQLResultGetValue(&oResult, 7, 0);
            const char* pszTMSMinX = SQLResultGetValue(&oResult, 8, 0);
            const char* pszTMSMinY = SQLResultGetValue(&oResult, 9, 0);
            const char* pszTMSMaxX = SQLResultGetValue(&oResult, 10, 0);
            const char* pszTMSMaxY = SQLResultGetValue(&oResult, 11, 0);
            if( pszTableName != NULL && pszTMSMinX != NULL && pszTMSMinY != NULL &&
                pszTMSMaxX != NULL && pszTMSMaxY != NULL )
            {
                bRet = OpenRaster( pszTableName, pszIdentifier, pszDescription,
                                   pszSRSId ? atoi(pszSRSId) : 0,
                                   CPLAtof(pszTMSMinX), CPLAtof(pszTMSMinY),
                                   CPLAtof(pszTMSMaxX), CPLAtof(pszTMSMaxY),
                                   pszMinX, pszMinY, pszMaxX, pszMaxY,
                                   poOpenInfo->papszOpenOptions );
            }
        }
        else if( oResult.nRowCount >= 1 )
        {
            bRet = TRUE;
            
            int nSDSCount = 0;
            for ( i = 0; i < oResult.nRowCount; i++ )
            {
                const char *pszTableName = SQLResultGetValue(&oResult, 0, i);
                const char *pszIdentifier = SQLResultGetValue(&oResult, 1, i);
                if( pszTableName != NULL )
                {
                    m_papszSubDatasets = CSLSetNameValue( m_papszSubDatasets,
                                                        CPLSPrintf("SUBDATASET_%d_NAME", nSDSCount+1),
                                                        CPLSPrintf("GPKG:%s:%s", m_pszFilename, pszTableName) );
                    if( pszIdentifier )
                        m_papszSubDatasets = CSLSetNameValue( m_papszSubDatasets,
                                                              CPLSPrintf("SUBDATASET_%d_DESC", nSDSCount+1),
                                                              CPLSPrintf("%s - %s", pszTableName, pszIdentifier) );
                    else
                        m_papszSubDatasets = CSLSetNameValue( m_papszSubDatasets,
                                                              CPLSPrintf("SUBDATASET_%d_DESC", nSDSCount+1),
                                                              pszTableName );
                }
                nSDSCount ++;
            }
        }

        SQLResultFree(&oResult);
    }

    return bRet;
}

/************************************************************************/
/*                         InitRaster()                                 */
/************************************************************************/

int GDALGeoPackageDataset::InitRaster ( GDALGeoPackageDataset* poParentDS,
                                        const char* pszTableName,
                                        double dfMinX,
                                        double dfMinY,
                                        double dfMaxX,
                                        double dfMaxY,
                                        const char* pszContentsMinX,
                                        const char* pszContentsMinY,
                                        const char* pszContentsMaxX,
                                        const char* pszContentsMaxY,
                                        char** papszOpenOptions,
                                        const SQLResult& oResult,
                                        int nIdxInResult )
{
    m_osRasterTable = pszTableName;
    m_dfTMSMinX = dfMinX;
    m_dfTMSMaxY = dfMaxY;

    int nZoomLevel = atoi(SQLResultGetValue(&oResult, 0, nIdxInResult));
    double dfPixelXSize = CPLAtof(SQLResultGetValue(&oResult, 1, nIdxInResult));
    double dfPixelYSize = CPLAtof(SQLResultGetValue(&oResult, 2, nIdxInResult));
    int nTileWidth = atoi(SQLResultGetValue(&oResult, 3, nIdxInResult));
    int nTileHeight = atoi(SQLResultGetValue(&oResult, 4, nIdxInResult));

    /* Use content bounds in priority over tile_matrix_set bounds */
    double dfGDALMinX = dfMinX;
    double dfGDALMinY = dfMinY;
    double dfGDALMaxX = dfMaxX;
    double dfGDALMaxY = dfMaxY;
    pszContentsMinX = CSLFetchNameValueDef(papszOpenOptions, "MINX", pszContentsMinX);
    pszContentsMinY = CSLFetchNameValueDef(papszOpenOptions, "MINY", pszContentsMinY);
    pszContentsMaxX = CSLFetchNameValueDef(papszOpenOptions, "MAXX", pszContentsMaxX);
    pszContentsMaxY = CSLFetchNameValueDef(papszOpenOptions, "MAXY", pszContentsMaxY);
    if( pszContentsMinX != NULL && pszContentsMinY != NULL &&
        pszContentsMaxX != NULL && pszContentsMaxY != NULL )
    {
        dfGDALMinX = CPLAtof(pszContentsMinX);
        dfGDALMinY = CPLAtof(pszContentsMinY);
        dfGDALMaxX = CPLAtof(pszContentsMaxX);
        dfGDALMaxY = CPLAtof(pszContentsMaxY);
    }
    if( dfGDALMinX >= dfGDALMaxX || dfGDALMinY >= dfGDALMaxY )
    {
        return FALSE;
    }

    int nBandCount = atoi(CSLFetchNameValueDef(papszOpenOptions, "BAND_COUNT", "4"));
    if( nBandCount != 1 && nBandCount != 3 && nBandCount != 4 )
        nBandCount = 4;

    return InitRaster(poParentDS, pszTableName, nZoomLevel, nBandCount, dfMinX, dfMaxY,
                      dfPixelXSize, dfPixelYSize, nTileWidth, nTileHeight,
                      dfGDALMinX, dfGDALMinY, dfGDALMaxX, dfGDALMaxY );
}

/************************************************************************/
/*                         InitRaster()                                 */
/************************************************************************/

int GDALGeoPackageDataset::InitRaster ( GDALGeoPackageDataset* poParentDS,
                                        const char* pszTableName,
                                        int nZoomLevel,
                                        int nBandCount,
                                        double dfTMSMinX,
                                        double dfTMSMaxY,
                                        double dfPixelXSize,
                                        double dfPixelYSize,
                                        int nTileWidth,
                                        int nTileHeight,
                                        double dfGDALMinX,
                                        double dfGDALMinY,
                                        double dfGDALMaxX,
                                        double dfGDALMaxY )
{
    m_osRasterTable = pszTableName;
    m_dfTMSMinX = dfTMSMinX;
    m_dfTMSMaxY = dfTMSMaxY;
    m_nZoomLevel = nZoomLevel;

    m_bGeoTransformValid = TRUE;
    m_adfGeoTransform[0] = dfGDALMinX;
    m_adfGeoTransform[1] = dfPixelXSize;
    m_adfGeoTransform[3] = dfGDALMaxY;
    m_adfGeoTransform[5] = -dfPixelYSize;
    double dfRasterXSize = 0.5 + (dfGDALMaxX - dfGDALMinX) / dfPixelXSize;
    double dfRasterYSize = 0.5 + (dfGDALMaxY - dfGDALMinY) / dfPixelYSize;
    if( dfRasterXSize > INT_MAX || dfRasterYSize > INT_MAX )
        return FALSE;
    nRasterXSize = (int)dfRasterXSize;
    nRasterYSize = (int)dfRasterYSize;

    // Compute shift between GDAL origin and TileMatrixSet origin
    int nShiftXPixels = (int)floor(0.5 + (m_adfGeoTransform[0] - m_dfTMSMinX) /  m_adfGeoTransform[1]);
    m_nShiftXTiles = (int)floor(1.0 * nShiftXPixels / nTileWidth);
    m_nShiftXPixelsMod = ((nShiftXPixels % nTileWidth) + nTileWidth) % nTileWidth;
    int nShiftYPixels = (int)floor(0.5 + (m_adfGeoTransform[3] - m_dfTMSMaxY) /  m_adfGeoTransform[5]);
    m_nShiftYTiles = (int)floor(1.0 * nShiftYPixels / nTileHeight);
    m_nShiftYPixelsMod = ((nShiftYPixels % nTileHeight) + nTileHeight) % nTileHeight;

    m_pabyCachedTiles = (GByte*) VSIMalloc3(4 * 4, nTileWidth, nTileHeight);
    if( m_pabyCachedTiles == NULL )
    {
        return FALSE;
    }

    for(int i = 1; i <= nBandCount; i ++)
        SetBand( i, new GDALGeoPackageRasterBand(this, i, nTileWidth, nTileHeight) );

    GDALPamDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    GDALPamDataset::SetMetadataItem("ZOOM_LEVEL", CPLSPrintf("%d", m_nZoomLevel));

    if( poParentDS )
    {
        m_poParentDS = poParentDS;
        bUpdate = poParentDS->bUpdate;
        eAccess = poParentDS->eAccess;
        hDB = poParentDS->hDB;
        m_eTF = poParentDS->m_eTF;
        m_nQuality = poParentDS->m_nQuality;
        m_nZLevel = poParentDS->m_nZLevel;
        m_bDither = poParentDS->m_bDither;
        m_nSRID = poParentDS->m_nSRID;
        m_osWHERE = poParentDS->m_osWHERE;
        SetDescription(CPLSPrintf("%s - zoom_level=%d",
                                  poParentDS->GetDescription(), m_nZoomLevel));
    }

    return TRUE;
}

/************************************************************************/
/*                         GetTileFormat()                              */
/************************************************************************/

static GPKGTileFormat GetTileFormat(const char* pszTF )
{
    GPKGTileFormat eTF = GPKG_TF_PNG_JPEG;
    if( pszTF )
    {
        if( EQUAL(pszTF, "PNG_JPEG") )
            eTF = GPKG_TF_PNG_JPEG;
        else if( EQUAL(pszTF, "PNG") )
            eTF = GPKG_TF_PNG;
        else if( EQUAL(pszTF, "PNG8") )
            eTF = GPKG_TF_PNG8;
        else if( EQUAL(pszTF, "JPEG") )
            eTF = GPKG_TF_JPEG;
        else if( EQUAL(pszTF, "WEBP") )
            eTF = GPKG_TF_WEBP;
    }
    return eTF;
}

/************************************************************************/
/*                         OpenRaster()                                 */
/************************************************************************/

int GDALGeoPackageDataset::OpenRaster( const char* pszTableName,
                                       const char* pszIdentifier,
                                       const char* pszDescription,
                                       int nSRSId,
                                       double dfMinX,
                                       double dfMinY,
                                       double dfMaxX,
                                       double dfMaxY,
                                       const char* pszContentsMinX,
                                       const char* pszContentsMinY,
                                       const char* pszContentsMaxX,
                                       const char* pszContentsMaxY,
                                       char** papszOpenOptions )
{
    OGRErr err;
    SQLResult oResult;

    if( dfMinX >= dfMaxX || dfMinY >= dfMaxY )
        return FALSE;

    m_nSRID = nSRSId;
    if( nSRSId > 0 )
    {
        OGRSpatialReference* poSRS = GetSpatialRef( nSRSId );
        if( poSRS )
        {
            poSRS->exportToWkt(&m_pszProjection);
            delete poSRS;
        }
    }

    /* The NOT NULL are just in case the tables would have been built without */
    /* the mandatory constraints */
    char* pszQuotedTableName = sqlite3_mprintf("'%q'", pszTableName);
    CPLString osQuotedTableName(pszQuotedTableName);
    sqlite3_free(pszQuotedTableName);
    char* pszSQL = sqlite3_mprintf(
            "SELECT zoom_level, pixel_x_size, pixel_y_size, tile_width, tile_height FROM gpkg_tile_matrix tm "
            "WHERE table_name = %s AND pixel_x_size > 0 "
            "AND pixel_y_size > 0 AND tile_width > 0 AND tile_height > 0",
            osQuotedTableName.c_str());
    CPLString osSQL(pszSQL);
    const char* pszZoomLevel =  CSLFetchNameValue(papszOpenOptions, "ZOOM_LEVEL");
    if( pszZoomLevel )
    {
        if( bUpdate )
            osSQL += CPLSPrintf(" AND zoom_level <= %d", atoi(pszZoomLevel));
        else
        {
            osSQL += CPLSPrintf(" AND (zoom_level = %d OR (zoom_level < %d AND EXISTS(SELECT 1 FROM %s WHERE zoom_level = tm.zoom_level LIMIT 1)))",
                                atoi(pszZoomLevel), atoi(pszZoomLevel), osQuotedTableName.c_str());
        }
    }
    // In read-only mode, only lists non empty zoom levels
    else if( !bUpdate )
    {
        osSQL += CPLSPrintf(" AND EXISTS(SELECT 1 FROM %s WHERE zoom_level = tm.zoom_level LIMIT 1)",
                            osQuotedTableName.c_str());
    }
    else if( pszZoomLevel == NULL )
    {
        osSQL += CPLSPrintf(" AND zoom_level <= (SELECT MAX(zoom_level) FROM %s)",
                            osQuotedTableName.c_str());
    }
    osSQL += " ORDER BY zoom_level DESC";

    err = SQLQuery(hDB, osSQL.c_str(), &oResult);
    if( err != OGRERR_NONE || oResult.nRowCount == 0 )
    {
        if( err == OGRERR_NONE && oResult.nRowCount == 0 &&
            pszContentsMinX != NULL && pszContentsMinY != NULL &&
            pszContentsMaxX != NULL && pszContentsMaxY != NULL )
        {
            SQLResultFree(&oResult);
            osSQL = pszSQL;
            osSQL += " ORDER BY zoom_level DESC LIMIT 1";
            err = SQLQuery(hDB, osSQL.c_str(), &oResult);
        }
        if( err != OGRERR_NONE || oResult.nRowCount == 0 )
        {
            SQLResultFree(&oResult);
            sqlite3_free(pszSQL);
            return FALSE;
        }
    }
    sqlite3_free(pszSQL);

    // If USE_TILE_EXTENT=YES, then query the tile table to find which tiles
    // actually exist.
    CPLString osContentsMinX, osContentsMinY, osContentsMaxX, osContentsMaxY;
    if( CSLTestBoolean(CSLFetchNameValueDef(papszOpenOptions, "USE_TILE_EXTENT", "NO")) )
    {
        pszSQL = sqlite3_mprintf(
            "SELECT MIN(tile_column), MIN(tile_row), MAX(tile_column), MAX(tile_row) FROM '%q' WHERE zoom_level = %d",
            pszTableName, atoi(SQLResultGetValue(&oResult, 0, 0)));
        SQLResult oResult2;
        err = SQLQuery(hDB, pszSQL, &oResult2);
        sqlite3_free(pszSQL);
        if  ( err != OGRERR_NONE || oResult2.nRowCount == 0 )
        {
            SQLResultFree(&oResult);
            SQLResultFree(&oResult2);
            return FALSE;
        }
        double dfPixelXSize = CPLAtof(SQLResultGetValue(&oResult, 1, 0));
        double dfPixelYSize = CPLAtof(SQLResultGetValue(&oResult, 2, 0));
        int nTileWidth = atoi(SQLResultGetValue(&oResult, 3, 0));
        int nTileHeight = atoi(SQLResultGetValue(&oResult, 4, 0));
        osContentsMinX = CPLSPrintf("%.18g", dfMinX + dfPixelXSize * nTileWidth * atoi(SQLResultGetValue(&oResult2, 0, 0)));
        osContentsMaxY = CPLSPrintf("%.18g", dfMaxY - dfPixelYSize * nTileHeight * atoi(SQLResultGetValue(&oResult2, 1, 0)));
        osContentsMaxX = CPLSPrintf("%.18g", dfMinX + dfPixelXSize * nTileWidth * (1 + atoi(SQLResultGetValue(&oResult2, 2, 0))));
        osContentsMinY = CPLSPrintf("%.18g", dfMaxY - dfPixelYSize * nTileHeight * (1 + atoi(SQLResultGetValue(&oResult2, 3, 0))));
        pszContentsMinX = osContentsMinX.c_str();
        pszContentsMinY = osContentsMinY.c_str();
        pszContentsMaxX = osContentsMaxX.c_str();
        pszContentsMaxY = osContentsMaxY.c_str();
        SQLResultFree(&oResult2);
    }
    
    if(! InitRaster ( NULL, pszTableName, dfMinX, dfMinY, dfMaxX, dfMaxY,
                 pszContentsMinX, pszContentsMinY, pszContentsMaxX, pszContentsMaxY,
                 papszOpenOptions, oResult, 0) )
    {
        SQLResultFree(&oResult);
        return FALSE;
    }

    CheckUnknownExtensions(TRUE);

    // Do this after CheckUnknownExtensions() so that m_eTF is set to GPKG_TF_WEBP
    // if the table already registers the gpkg_webp extension
    const char* pszTF = CSLFetchNameValue(papszOpenOptions, "DRIVER");
    if( pszTF )
    {
        if( !bUpdate )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "DRIVER open option ignored in read-only mode");
        }
        else
        {
            GPKGTileFormat eTF = GetTileFormat(pszTF);
            if( eTF == GPKG_TF_WEBP && m_eTF != eTF )
            {
                if( !RegisterWebPExtension() )
                    return FALSE;
            }
            m_eTF = eTF;
        }
    }

    ParseCompressionOptions(papszOpenOptions);

    m_osWHERE = CSLFetchNameValueDef(papszOpenOptions, "WHERE", "");

    // Set metadata
    if( pszIdentifier && pszIdentifier[0] )
        GDALPamDataset::SetMetadataItem("IDENTIFIER", pszIdentifier);
    if( pszDescription && pszDescription[0] )
        GDALPamDataset::SetMetadataItem("DESCRIPTION", pszDescription);

    // Add overviews
    for( int i = 1; i < oResult.nRowCount; i++ )
    {
        GDALGeoPackageDataset* poOvrDS = new GDALGeoPackageDataset();
        poOvrDS->InitRaster ( this, pszTableName, dfMinX, dfMinY, dfMaxX, dfMaxY,
                 pszContentsMinX, pszContentsMinY, pszContentsMaxX, pszContentsMaxY,
                 papszOpenOptions, oResult, i);

        m_papoOverviewDS = (GDALGeoPackageDataset**) CPLRealloc(m_papoOverviewDS,
                        sizeof(GDALGeoPackageDataset*) * (m_nOverviewCount+1));
        m_papoOverviewDS[m_nOverviewCount ++] = poOvrDS;

        int nTileWidth, nTileHeight;
        poOvrDS->GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);
        if( poOvrDS->GetRasterXSize() < nTileWidth &&
            poOvrDS->GetRasterYSize() < nTileHeight )
        {
            break;
        }
    }

    SQLResultFree(&oResult);

    return TRUE;
}

/************************************************************************/
/*                         GetProjectionRef()                           */
/************************************************************************/

const char* GDALGeoPackageDataset::GetProjectionRef()
{
    return (m_pszProjection) ? m_pszProjection : "";
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr GDALGeoPackageDataset::SetProjection( const char* pszProjection )
{
    if( nBands == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetProjection() not supported on a dataset with 0 band");
        return CE_Failure;
    }
    if( m_nSRID != UNKNOWN_SRID )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot modify projection once set");
        return CE_Failure;
    }
    OGRSpatialReference oSRS;
    if( oSRS.SetFromUserInput(pszProjection) != OGRERR_NONE )
        return CE_Failure;

    m_nSRID = GetSrsId( &oSRS );

    if( m_bGeoTransformValid )
        return FinalizeRasterRegistration();
    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GDALGeoPackageDataset::GetGeoTransform( double* padfGeoTransform )
{
    memcpy(padfGeoTransform, m_adfGeoTransform, 6 * sizeof(double));
    if( !m_bGeoTransformValid )
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GDALGeoPackageDataset::SetGeoTransform( double* padfGeoTransform )
{
    if( nBands == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetGeoTransform() not supported on a dataset with 0 band");
        return CE_Failure;
    }
    if( m_bGeoTransformValid )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot modify geotransform once set");
        return CE_Failure;
    }
    if( padfGeoTransform[2] != 0.0 || padfGeoTransform[4] != 0 ||
        padfGeoTransform[5] > 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only north-up non rotated geotransform supported");
        return CE_Failure;
    }
    memcpy(m_adfGeoTransform, padfGeoTransform, 6 * sizeof(double));
    m_bGeoTransformValid = TRUE;
    
    if( m_nSRID != UNKNOWN_SRID )
        return FinalizeRasterRegistration();
    return CE_None;
}

/************************************************************************/
/*                      FinalizeRasterRegistration()                    */
/************************************************************************/

CPLErr GDALGeoPackageDataset::FinalizeRasterRegistration()
{
    OGRErr eErr;
    char* pszSQL;

    m_dfTMSMinX = m_adfGeoTransform[0];
    m_dfTMSMaxY = m_adfGeoTransform[3];

    double dfGDALMinX = m_adfGeoTransform[0];
    double dfGDALMinY = m_adfGeoTransform[3] + nRasterYSize * m_adfGeoTransform[5];
    double dfGDALMaxX = m_adfGeoTransform[0] + nRasterXSize * m_adfGeoTransform[1];
    double dfGDALMaxY = m_adfGeoTransform[3];

    pszSQL = sqlite3_mprintf("INSERT INTO gpkg_contents "
        "(table_name,data_type,identifier,description,min_x,min_y,max_x,max_y,srs_id) VALUES "
        "('%q','tiles','%q','%q',%.18g,%.18g,%.18g,%.18g,%d)",
        m_osRasterTable.c_str(),
        m_osIdentifier.c_str(),
        m_osDescription.c_str(),
        dfGDALMinX, dfGDALMinY, dfGDALMaxX, dfGDALMaxY,
        m_nSRID);
    eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if ( eErr != OGRERR_NONE )
        return CE_Failure;

    m_nZoomLevel = 0;
    int nTileWidth, nTileHeight;
    GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);
    while( (nRasterXSize >> m_nZoomLevel) > nTileWidth ||
           (nRasterYSize >> m_nZoomLevel) > nTileHeight )
        m_nZoomLevel ++;
    
    double dfPixelXSizeZoomLevel0 = m_adfGeoTransform[1] * (1 << m_nZoomLevel);
    double dfPixelYSizeZoomLevel0 = fabs(m_adfGeoTransform[5]) * (1 << m_nZoomLevel);
    int nTileXCountZoomLevel0 = ((nRasterXSize >> m_nZoomLevel) + nTileWidth - 1) / nTileWidth;
    int nTileYCountZoomLevel0 = ((nRasterYSize >> m_nZoomLevel) + nTileHeight - 1) / nTileHeight;
    double dfTMSMinX = m_adfGeoTransform[0];
    double dfTMSMaxX = dfTMSMinX + nTileXCountZoomLevel0 * nTileWidth * dfPixelXSizeZoomLevel0;
    double dfTMSMaxY = m_adfGeoTransform[3];
    double dfTMSMinY = dfTMSMaxY - nTileYCountZoomLevel0 * nTileHeight * dfPixelYSizeZoomLevel0;

    pszSQL = sqlite3_mprintf("INSERT INTO gpkg_tile_matrix_set "
            "(table_name,srs_id,min_x,min_y,max_x,max_y) VALUES "
            "('%q',%d,%.18g,%.18g,%.18g,%.18g)",
            m_osRasterTable.c_str(), m_nSRID,
            dfTMSMinX,dfTMSMinY,dfTMSMaxX,dfTMSMaxY);
    eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if ( eErr != OGRERR_NONE )
        return CE_Failure;

    m_papoOverviewDS = (GDALGeoPackageDataset**) CPLCalloc(sizeof(GDALGeoPackageDataset*),
                                                           m_nZoomLevel);

    for(int i=0; i<=m_nZoomLevel; i++)
    {
        double dfPixelXSizeZoomLevel = m_adfGeoTransform[1] * (1 << (m_nZoomLevel-i));
        double dfPixelYSizeZoomLevel = fabs(m_adfGeoTransform[5]) * (1 << (m_nZoomLevel-i));
        int nTileXCountZoomLevel = ((nRasterXSize >> (m_nZoomLevel-i)) + nTileWidth - 1) / nTileWidth;
        int nTileYCountZoomLevel = ((nRasterYSize >> (m_nZoomLevel-i)) + nTileHeight - 1) / nTileHeight;
        pszSQL = sqlite3_mprintf("INSERT INTO gpkg_tile_matrix "
                "(table_name,zoom_level,matrix_width,matrix_height,tile_width,tile_height,pixel_x_size,pixel_y_size) VALUES "
                "('%q',%d,%d,%d,%d,%d,%.18g,%.18g)",
                m_osRasterTable.c_str(),i,nTileXCountZoomLevel,nTileYCountZoomLevel,
                nTileWidth,nTileHeight,dfPixelXSizeZoomLevel,dfPixelYSizeZoomLevel);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if ( eErr != OGRERR_NONE )
            return CE_Failure;

        if( i < m_nZoomLevel )
        {
            GDALGeoPackageDataset* poOvrDS = new GDALGeoPackageDataset();
            poOvrDS->InitRaster ( this, m_osRasterTable, i, nBands,
                                  m_dfTMSMinX, m_dfTMSMaxY,
                                  dfPixelXSizeZoomLevel, dfPixelYSizeZoomLevel,
                                  nTileWidth, nTileHeight,
                                  dfGDALMinX, dfGDALMinY,
                                  dfGDALMaxX, dfGDALMaxY );

            m_papoOverviewDS[m_nZoomLevel-1-i] = poOvrDS;
        }
    }
    m_nOverviewCount = m_nZoomLevel;

    return CE_None;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void GDALGeoPackageDataset::FlushCache()

{
    // Short circuit GDALPamDataset to avoid serialization to .aux.xml
    GDALDataset::FlushCache();
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

/* See TIFF_OvLevelAdjust */
static int GPKG_OvLevelAdjust( int nOvLevel, int nXSize )

{
    int nOXSize = (nXSize + nOvLevel - 1) / nOvLevel;
    
    return (int) (0.5 + nXSize / (double) nOXSize);
}

static int GetFloorPowerOfTwo(int n)
{
    int p2 = 1;
    while( (n = n >> 1) > 0 )
    {
        p2 <<= 1;
    }
    return p2;
}

CPLErr GDALGeoPackageDataset::IBuildOverviews( 
                        const char * pszResampling, 
                        int nOverviews, int * panOverviewList,
                        int nBandsIn, int * panBandList,
                        GDALProgressFunc pfnProgress, void * pProgressData )
{
    if( GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Overview building not supported on a database opened in read-only mode");
        return CE_Failure;
    }
    if( m_poParentDS != NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Overview building not supported on overview dataset");
        return CE_Failure;
    }
    
    if( nOverviews == 0 )
    {
        for(int i=0;i<m_nOverviewCount;i++)
            m_papoOverviewDS[i]->FlushCache();
        char* pszSQL = sqlite3_mprintf("DELETE FROM '%q' WHERE zoom_level < %d",
                                       m_osRasterTable.c_str(),
                                       m_nZoomLevel);
        char* pszErrMsg = NULL;
        int ret = sqlite3_exec(hDB, pszSQL, NULL, NULL, &pszErrMsg);
        sqlite3_free(pszSQL);
        if( ret != SQLITE_OK )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failure: %s",
                     pszErrMsg ? pszErrMsg : "");
            sqlite3_free(pszErrMsg);
            return CE_Failure;
        }
        return CE_None;
    }
    
    if( nBandsIn != nBands )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews in GPKG only"
                  "supported when operating on all bands." );
        return CE_Failure;
    }

    if( m_nOverviewCount == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Image too small to support overviews");
        return CE_Failure;
    }

    FlushCache();
    for(int i=0;i<nOverviews;i++)
    {
        if( panOverviewList[i] < 2 )
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Overview factor must be >= 2");
            return CE_Failure;
        }

        int bFound = FALSE;
        int jCandidate = -1;
        int nMaxOvFactor = 0;
        for(int j=0;j<m_nOverviewCount;j++)
        {
            int    nOvFactor;

            GDALDataset* poODS = m_papoOverviewDS[j];

            nOvFactor = (int) 
                (0.5 + GetRasterXSize() / (double) poODS->GetRasterXSize());
            nMaxOvFactor = nOvFactor;

            if( nOvFactor == panOverviewList[i] 
                || nOvFactor == GPKG_OvLevelAdjust( panOverviewList[i],
                                                    GetRasterXSize() ) )
            {
                bFound = TRUE;
                break;
            }

            if( jCandidate < 0 && nOvFactor > panOverviewList[i] )
                jCandidate = j;
        }

        if( !bFound )
        {
            /* Mostly for debug */
            if( !CSLTestBoolean(CPLGetConfigOption("ALLOW_GPKG_ZOOM_OTHER_EXTENSION", "YES")) )
            {
                CPLString osOvrList;
                for(int j=0;j<m_nOverviewCount;j++)
                {
                    int    nOvFactor;

                    GDALDataset* poODS = m_papoOverviewDS[j];

                    /* Compute overview factor */
                    nOvFactor = (int) 
                        (0.5 + GetRasterXSize() / (double) poODS->GetRasterXSize());
                    int nODSXSize = (int)(0.5 + GetRasterXSize() / (double) nOvFactor);
                    if( nODSXSize != poODS->GetRasterXSize() )
                    {
                        int nOvFactorPowerOfTwo = GetFloorPowerOfTwo(nOvFactor);
                        nODSXSize = (int)(0.5 + GetRasterXSize() / (double) nOvFactorPowerOfTwo);
                        if( nODSXSize == poODS->GetRasterXSize() )
                            nOvFactor = nOvFactorPowerOfTwo;
                        else
                        {
                            nOvFactorPowerOfTwo <<= 1;
                            nODSXSize = (int)(0.5 + GetRasterXSize() / (double) nOvFactorPowerOfTwo);
                            if( nODSXSize == poODS->GetRasterXSize() )
                                nOvFactor = nOvFactorPowerOfTwo;
                        }
                    }
                    if( j != 0 )
                        osOvrList += " ";
                    osOvrList += CPLSPrintf("%d", nOvFactor);
                }
                CPLError(CE_Failure, CPLE_NotSupported,
                        "Only overviews %s can be computed", osOvrList.c_str());
                return CE_Failure;
            }
            else
            {
                int nOvFactor = panOverviewList[i];
                if( jCandidate < 0 )
                    jCandidate = m_nOverviewCount;

                int nOvXSize = GetRasterXSize() / nOvFactor;
                int nOvYSize = GetRasterYSize() / nOvFactor;
                if( nOvXSize < 8 || nOvYSize < 8)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Too big overview factor : %d. Would result in a %dx%d overview",
                             nOvFactor, nOvXSize, nOvYSize);
                    return CE_Failure;
                }
                if( !(jCandidate == m_nOverviewCount && nOvFactor == 2 * nMaxOvFactor) &&
                    !m_bZoomOther )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Use of overview factor %d cause gpkg_zoom_other extension to be needed",
                            nOvFactor);
                    RegisterZoomOtherExtension();
                    m_bZoomOther = TRUE;
                }

                SQLCommand(hDB, "BEGIN");

                CPLAssert(jCandidate > 0);
                int nNewZoomLevel = m_papoOverviewDS[jCandidate-1]->m_nZoomLevel;

                char* pszSQL;
                OGRErr eErr;
                for(int k=0;k<=jCandidate;k++)
                {
                    pszSQL = sqlite3_mprintf("UPDATE gpkg_tile_matrix SET zoom_level = %d "
                        "WHERE table_name = '%q' AND zoom_level = %d",
                        m_nZoomLevel - k + 1,
                        m_osRasterTable.c_str(),
                        m_nZoomLevel - k);
                    eErr = SQLCommand(hDB, pszSQL);
                    sqlite3_free(pszSQL);
                    if ( eErr != OGRERR_NONE )
                        return CE_Failure;

                    pszSQL = sqlite3_mprintf("UPDATE '%q' SET zoom_level = %d "
                        "WHERE zoom_level = %d",
                        m_osRasterTable.c_str(),
                        m_nZoomLevel - k + 1,
                        m_nZoomLevel - k);
                    eErr = SQLCommand(hDB, pszSQL);
                    sqlite3_free(pszSQL);
                    if ( eErr != OGRERR_NONE )
                        return CE_Failure;
                }

                double dfGDALMinX = m_adfGeoTransform[0];
                double dfGDALMinY = m_adfGeoTransform[3] + nRasterYSize * m_adfGeoTransform[5];
                double dfGDALMaxX = m_adfGeoTransform[0] + nRasterXSize * m_adfGeoTransform[1];
                double dfGDALMaxY = m_adfGeoTransform[3];
                double dfPixelXSizeZoomLevel = m_adfGeoTransform[1] * nOvFactor;
                double dfPixelYSizeZoomLevel = fabs(m_adfGeoTransform[5]) * nOvFactor;
                int nTileWidth, nTileHeight;
                GetRasterBand(1)->GetBlockSize(&nTileWidth, &nTileHeight);
                int nTileXCountZoomLevel = (nOvXSize + nTileWidth - 1) / nTileWidth;
                int nTileYCountZoomLevel = (nOvYSize + nTileHeight - 1) / nTileHeight;
                pszSQL = sqlite3_mprintf("INSERT INTO gpkg_tile_matrix "
                        "(table_name,zoom_level,matrix_width,matrix_height,tile_width,tile_height,pixel_x_size,pixel_y_size) VALUES "
                        "('%q',%d,%d,%d,%d,%d,%.18g,%.18g)",
                        m_osRasterTable.c_str(),nNewZoomLevel,nTileXCountZoomLevel,nTileYCountZoomLevel,
                        nTileWidth,nTileHeight,dfPixelXSizeZoomLevel,dfPixelYSizeZoomLevel);
                eErr = SQLCommand(hDB, pszSQL);
                sqlite3_free(pszSQL);
                if ( eErr != OGRERR_NONE )
                    return CE_Failure;
                
                SQLCommand(hDB, "COMMIT");

                m_nZoomLevel ++; /* this change our zoom level as well as previous overviews */
                for(int k=0;k<jCandidate;k++)
                    m_papoOverviewDS[k]->m_nZoomLevel ++;

                GDALGeoPackageDataset* poOvrDS = new GDALGeoPackageDataset();
                poOvrDS->InitRaster ( this, m_osRasterTable,
                                      nNewZoomLevel, nBands,
                                      m_dfTMSMinX, m_dfTMSMaxY,
                                      dfPixelXSizeZoomLevel, dfPixelYSizeZoomLevel,
                                      nTileWidth, nTileHeight,
                                      dfGDALMinX, dfGDALMinY,
                                      dfGDALMaxX, dfGDALMaxY );
                m_papoOverviewDS = (GDALGeoPackageDataset**) CPLRealloc(
                    m_papoOverviewDS, sizeof(GDALGeoPackageDataset*) * (m_nOverviewCount+1));

                if( jCandidate < m_nOverviewCount )
                {
                    memmove(m_papoOverviewDS + jCandidate + 1,
                            m_papoOverviewDS + jCandidate,
                            sizeof(GDALGeoPackageDataset*) * (m_nOverviewCount-jCandidate));
                }
                m_papoOverviewDS[jCandidate] = poOvrDS;
                m_nOverviewCount ++;
            }
        }
    }
    
    GDALRasterBand*** papapoOverviewBands = (GDALRasterBand ***) CPLCalloc(sizeof(void*),nBands);
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        papapoOverviewBands[iBand] = (GDALRasterBand **) CPLCalloc(sizeof(void*),nOverviews);
        int iCurOverview = 0;
        for(int i=0;i<nOverviews;i++)
        {
            int   j;
            for( j = 0; j < m_nOverviewCount; j++ )
            {
                int    nOvFactor;
                GDALDataset* poODS = m_papoOverviewDS[j];

                nOvFactor = (int) 
                    (0.5 + GetRasterXSize() / (double) poODS->GetRasterXSize());

                if( nOvFactor == panOverviewList[i] 
                    || nOvFactor == GPKG_OvLevelAdjust( panOverviewList[i],
                                                        GetRasterXSize() ) )
                {
                    papapoOverviewBands[iBand][iCurOverview] = poODS->GetRasterBand(iBand+1);
                    iCurOverview++ ;
                    break;
                }
            }
            CPLAssert(j < m_nOverviewCount);
        }
        CPLAssert(iCurOverview == nOverviews);
    }

    CPLErr eErr = GDALRegenerateOverviewsMultiBand(nBands, papoBands,
                                     nOverviews, papapoOverviewBands,
                                     pszResampling, pfnProgress, pProgressData );

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        CPLFree(papapoOverviewBands[iBand]);
    }
    CPLFree(papapoOverviewBands);

    return eErr;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GDALGeoPackageDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "SUBDATASETS", NULL);
}

/************************************************************************/
/*                        CheckMetadataDomain()                         */
/************************************************************************/

const char* GDALGeoPackageDataset::CheckMetadataDomain( const char* pszDomain )
{
    if( pszDomain != NULL && EQUAL(pszDomain, "GEOPACKAGE") &&
        m_osRasterTable.size() == 0 )
    {
        CPLError(CE_Warning, CPLE_IllegalArg,
                 "Using GEOPACKAGE for a non-raster geopackage is not supported. "
                 "Using default domain instead");
        return NULL;
    }
    return pszDomain;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GDALGeoPackageDataset::GetMetadata( const char *pszDomain )

{
    pszDomain = CheckMetadataDomain(pszDomain);
    if( pszDomain != NULL && EQUAL(pszDomain,"SUBDATASETS") )
        return m_papszSubDatasets;

    if( m_bHasReadMetadataFromStorage )
        return GDALPamDataset::GetMetadata( pszDomain );

    m_bHasReadMetadataFromStorage = TRUE;

    OGRErr err;
    int nCount = SQLGetInteger(hDB,
                  "SELECT COUNT(*) FROM sqlite_master WHERE name IN "
                  "('gpkg_metadata', 'gpkg_metadata_reference') "
                  "AND type IN ('table', 'view')", &err);
    if ( err != OGRERR_NONE || nCount != 2 )
        return GDALPamDataset::GetMetadata( pszDomain );

    char* pszSQL;
    if( m_osRasterTable.size() )
    {
        pszSQL = sqlite3_mprintf(
            "SELECT md.metadata, md.md_standard_uri, md.mime_type, mdr.reference_scope FROM gpkg_metadata md "
            "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE mdr.reference_scope = 'geopackage' OR "
            "(mdr.reference_scope = 'table' AND mdr.table_name = '%q') ORDER BY md.id",
            m_osRasterTable.c_str());
    }
    else
    {
        pszSQL = sqlite3_mprintf(
            "SELECT md.metadata, md.md_standard_uri, md.mime_type, mdr.reference_scope FROM gpkg_metadata md "
            "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE mdr.reference_scope = 'geopackage' ORDER BY md.id");
    }

    SQLResult oResult;
    err = SQLQuery(hDB, pszSQL, &oResult);
    sqlite3_free(pszSQL);
    if  ( err != OGRERR_NONE )
    {
        SQLResultFree(&oResult);
        return GDALPamDataset::GetMetadata( pszDomain );
    }

    char** papszMetadata = CSLDuplicate(GDALPamDataset::GetMetadata());

    /* GDAL metadata */
    for(int i=0;i<oResult.nRowCount;i++)
    {
        const char *pszMetadata = SQLResultGetValue(&oResult, 0, i);
        const char* pszMDStandardURI = SQLResultGetValue(&oResult, 1, i);
        const char* pszMimeType = SQLResultGetValue(&oResult, 2, i);
        const char* pszReferenceScope = SQLResultGetValue(&oResult, 3, i);
        int bIsGPKGScope = EQUAL(pszReferenceScope, "geopackage");
        if( pszMetadata == NULL )
            continue;
        if( pszMDStandardURI != NULL && EQUAL(pszMDStandardURI, "http://gdal.org") &&
            pszMimeType != NULL && EQUAL(pszMimeType, "text/xml") )
        {
            CPLXMLNode* psXMLNode = CPLParseXMLString(pszMetadata);
            if( psXMLNode )
            {
                GDALMultiDomainMetadata oLocalMDMD;
                oLocalMDMD.XMLInit(psXMLNode, FALSE);
                if( m_osRasterTable.size() && bIsGPKGScope )
                {
                    oMDMD.SetMetadata( oLocalMDMD.GetMetadata(), "GEOPACKAGE" );
                }
                else
                {
                    papszMetadata = CSLMerge(papszMetadata, oLocalMDMD.GetMetadata());
                    char** papszDomainList = oLocalMDMD.GetDomainList();
                    char** papszIter = papszDomainList;
                    while( papszIter && *papszIter )
                    {
                        if( !EQUAL(*papszIter, "") && !EQUAL(*papszIter, "IMAGE_STRUCTURE") )
                            oMDMD.SetMetadata(oLocalMDMD.GetMetadata(*papszIter), *papszIter);
                        papszIter ++;
                    }
                }
                CPLDestroyXMLNode(psXMLNode);
            }
        }
    }

    GDALPamDataset::SetMetadata(papszMetadata);
    CSLDestroy(papszMetadata);
    papszMetadata = NULL;

    /* Add non-GDAL metadata now */
    int nNonGDALMDILocal = 1;
    int nNonGDALMDIGeopackage = 1;
    for(int i=0;i<oResult.nRowCount;i++)
    {
        const char *pszMetadata = SQLResultGetValue(&oResult, 0, i);
        const char* pszMDStandardURI = SQLResultGetValue(&oResult, 1, i);
        const char* pszMimeType = SQLResultGetValue(&oResult, 2, i);
        const char* pszReferenceScope = SQLResultGetValue(&oResult, 3, i);
        int bIsGPKGScope = EQUAL(pszReferenceScope, "geopackage");
        if( pszMetadata == NULL )
            continue;
        if( pszMDStandardURI != NULL && EQUAL(pszMDStandardURI, "http://gdal.org") &&
            pszMimeType != NULL && EQUAL(pszMimeType, "text/xml") )
            continue;

        if( m_osRasterTable.size() && bIsGPKGScope )
        {
            oMDMD.SetMetadataItem( CPLSPrintf("GPKG_METADATA_ITEM_%d", nNonGDALMDIGeopackage),
                                   pszMetadata,
                                   "GEOPACKAGE" );
            nNonGDALMDIGeopackage ++;
        }
        else
        {
            oMDMD.SetMetadataItem( CPLSPrintf("GPKG_METADATA_ITEM_%d", nNonGDALMDILocal),
                                   pszMetadata );
            nNonGDALMDILocal ++;
        }
    }

    SQLResultFree(&oResult);

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                            WriteMetadata()                           */
/************************************************************************/

void GDALGeoPackageDataset::WriteMetadata(CPLXMLNode* psXMLNode, /* will be destroyed by the method /*/
                                          const char* pszTableName)
{
    int bIsEmpty = (psXMLNode->psChild == NULL);
    char *pszXML = NULL;
    if( bIsEmpty )
        CPLDestroyXMLNode(psXMLNode);
    else
    {
        CPLXMLNode* psMasterXMLNode = CPLCreateXMLNode( NULL, CXT_Element,
                                                        "GDALMultiDomainMetadata" );
        psMasterXMLNode->psChild = psXMLNode;
        pszXML = CPLSerializeXMLTree(psMasterXMLNode);
        CPLDestroyXMLNode(psMasterXMLNode);
    }
    psXMLNode = NULL;

    char* pszSQL;
    if( pszTableName && pszTableName[0] != '\0' )
    {
        pszSQL = sqlite3_mprintf(
            "SELECT md.id FROM gpkg_metadata md "
            "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE md.md_scope = 'dataset' AND md.md_standard_uri='http://gdal.org' "
            "AND md.mime_type='text/xml' AND mdr.reference_scope = 'table' AND mdr.table_name = '%q'",
            pszTableName);
    }
    else
    {
        pszSQL = sqlite3_mprintf(
            "SELECT md.id FROM gpkg_metadata md "
            "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE md.md_scope = 'dataset' AND md.md_standard_uri='http://gdal.org' "
            "AND md.mime_type='text/xml' AND mdr.reference_scope = 'geopackage'");
    }
    OGRErr err;
    int mdId = SQLGetInteger(hDB, pszSQL, &err);
    if( err != OGRERR_NONE )
        mdId = -1;
    sqlite3_free(pszSQL);

    if( bIsEmpty )
    {
        if( mdId >= 0 )
        {
            SQLCommand(hDB,
                       CPLSPrintf("DELETE FROM gpkg_metadata_reference WHERE md_file_id = %d", mdId));
            SQLCommand(hDB,
                       CPLSPrintf("DELETE FROM gpkg_metadata WHERE id = %d", mdId));
        }
    }
    else
    {
        if( mdId >= 0 )
        {
            pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_metadata SET metadata = '%q' WHERE id = %d",
                pszXML, mdId);
        }
        else
        {
            pszSQL = sqlite3_mprintf(
                "INSERT INTO gpkg_metadata (md_scope, md_standard_uri, mime_type, metadata) VALUES "
                "('dataset','http://gdal.org','text/xml','%q')",
                pszXML);
        }
        SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);

        CPLFree(pszXML);

        if( mdId < 0 )
        {
            const sqlite_int64 nFID = sqlite3_last_insert_rowid( hDB );
            if( pszTableName != NULL && pszTableName[0] != '\0' )
            {
                pszSQL = sqlite3_mprintf(
                    "INSERT INTO gpkg_metadata_reference (reference_scope, table_name, timestamp, md_file_id) VALUES "
                    "('table', '%q', strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ','now'), %d)",
                    pszTableName, (int)nFID);
            }
            else
            {
                pszSQL = sqlite3_mprintf(
                    "INSERT INTO gpkg_metadata_reference (reference_scope, timestamp, md_file_id) VALUES "
                    "('geopackage', strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ','now'), %d)",
                    (int)nFID);
            }
        }
        else
        {
            pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_metadata_reference SET timestamp = strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ','now') WHERE md_file_id = %d",
                mdId);
        }
        SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }
}

/************************************************************************/
/*                            FlushMetadata()                           */
/************************************************************************/

CPLErr GDALGeoPackageDataset::FlushMetadata()
{
    if( !m_bMetadataDirty || m_poParentDS != NULL )
        return CE_None;
    m_bMetadataDirty = FALSE;

    if( m_osRasterTable.size() )
    {
        const char* pszIdentifier = GetMetadataItem("IDENTIFIER");
        const char* pszDescription = GetMetadataItem("DESCRIPTION");
        if( !m_bIdentifierAsCO && pszIdentifier != NULL &&
            pszIdentifier != m_osIdentifier )
        {
            m_osIdentifier = pszIdentifier;
            char* pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_contents SET identifier = '%q' WHERE table_name = '%q'",
                pszIdentifier, m_osRasterTable.c_str());
            SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
        if( !m_bDescriptionAsCO && pszDescription != NULL &&
            pszDescription != m_osDescription )
        {
            m_osDescription = pszDescription;
            char* pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_contents SET description = '%q' WHERE table_name = '%q'",
                pszDescription, m_osRasterTable.c_str());
            SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
    }

    char** papszMDDup = NULL;
    for( char** papszIter = GetMetadata(); papszIter && *papszIter; ++papszIter )
    {
        if( EQUALN(*papszIter, "IDENTIFIER=", strlen("IDENTIFIER=")) )
            continue;
        if( EQUALN(*papszIter, "DESCRIPTION=", strlen("DESCRIPTION=")) )
            continue;
        if( EQUALN(*papszIter, "ZOOM_LEVEL=", strlen("ZOOM_LEVEL=")) )
            continue;
        if( EQUALN(*papszIter, "GPKG_METADATA_ITEM_", strlen("GPKG_METADATA_ITEM_")) )
            continue;
        papszMDDup = CSLInsertString(papszMDDup, -1, *papszIter);
    }

    CPLXMLNode* psXMLNode;
    {
        GDALMultiDomainMetadata oLocalMDMD;
        char** papszDomainList = oMDMD.GetDomainList();
        char** papszIter = papszDomainList;
        oLocalMDMD.SetMetadata(papszMDDup);
        while( papszIter && *papszIter )
        {
            if( !EQUAL(*papszIter, "") &&  !EQUAL(*papszIter, "IMAGE_STRUCTURE") )
                oLocalMDMD.SetMetadata(oMDMD.GetMetadata(*papszIter), *papszIter);
            papszIter ++;
        }
        psXMLNode = oLocalMDMD.Serialize();
    }

    CSLDestroy(papszMDDup);
    papszMDDup = NULL;

    if( psXMLNode != NULL )
    {
        WriteMetadata(psXMLNode, m_osRasterTable.c_str() );
    }

    if( m_osRasterTable.size() )
    {
        char** papszGeopackageMD = GetMetadata("GEOPACKAGE");

        char** papszMDDup = NULL;
        for( char** papszIter = papszGeopackageMD; papszIter && *papszIter; ++papszIter )
        {
            papszMDDup = CSLInsertString(papszMDDup, -1, *papszIter);
        }

        GDALMultiDomainMetadata oLocalMDMD;
        oLocalMDMD.SetMetadata(papszMDDup);
        CSLDestroy(papszMDDup);
        papszMDDup = NULL;
        psXMLNode = oLocalMDMD.Serialize();
        if( psXMLNode != NULL )
        {
            WriteMetadata(psXMLNode, NULL);
        }
    }

    return CE_None;
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GDALGeoPackageDataset::GetMetadataItem( const char * pszName,
                                                    const char * pszDomain )
{
    pszDomain = CheckMetadataDomain(pszDomain);
    return CSLFetchNameValue( GetMetadata(pszDomain), pszName );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GDALGeoPackageDataset::SetMetadata( char ** papszMetadata, const char * pszDomain )
{
    pszDomain = CheckMetadataDomain(pszDomain);
    m_bMetadataDirty = TRUE;
    GetMetadata(); /* force loading from storage if needed */
    return GDALPamDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GDALGeoPackageDataset::SetMetadataItem( const char * pszName,
                                               const char * pszValue,
                                               const char * pszDomain )
{
    pszDomain = CheckMetadataDomain(pszDomain);
    m_bMetadataDirty = TRUE;
    GetMetadata(); /* force loading from storage if needed */
    return GDALPamDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                                Create()                              */
/************************************************************************/

int GDALGeoPackageDataset::Create( const char * pszFilename,
                                   int bFileExists,
                                   int nXSize,
                                   int nYSize,
                                   int nBands,
                                   char **papszOptions )
{
    CPLString osCommand;
    const char *pszSpatialRefSysRecord;

    m_pszFilename = CPLStrdup(pszFilename);
    m_bNew = TRUE;
    bUpdate = TRUE;
    eAccess = GA_Update; /* hum annoying duplication */

#ifdef HAVE_SQLITE_VFS
    if (!OpenOrCreateDB(bFileExists ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
#else
    if (!OpenOrCreateDB(0))
#endif
        return FALSE;

    /* OGR UTF-8 support. If we set the UTF-8 Pragma early on, it */
    /* will be written into the main file and supported henceforth */
    SQLCommand(hDB, "PRAGMA encoding = \"UTF-8\"");

    if( !bFileExists )
    {
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

        /* From C.5. gpkg_tile_matrix_set Table 28. gpkg_tile_matrix_set Table Creation SQL  */
        const char *pszTileMatrixSet =
            "CREATE TABLE gpkg_tile_matrix_set ("
            "table_name TEXT NOT NULL PRIMARY KEY,"
            "srs_id INTEGER NOT NULL,"
            "min_x DOUBLE NOT NULL,"
            "min_y DOUBLE NOT NULL,"
            "max_x DOUBLE NOT NULL,"
            "max_y DOUBLE NOT NULL,"
            "CONSTRAINT fk_gtms_table_name FOREIGN KEY (table_name) REFERENCES gpkg_contents(table_name),"
            "CONSTRAINT fk_gtms_srs FOREIGN KEY (srs_id) REFERENCES gpkg_spatial_ref_sys (srs_id)"
            ")";
            
        if ( OGRERR_NONE != SQLCommand(hDB, pszTileMatrixSet) )
            return FALSE;
        
        /* From C.6. gpkg_tile_matrix Table 29. gpkg_tile_matrix Table Creation SQL */
        const char *pszTileMatrix =
            "CREATE TABLE gpkg_tile_matrix ("
            "table_name TEXT NOT NULL,"
            "zoom_level INTEGER NOT NULL,"
            "matrix_width INTEGER NOT NULL,"
            "matrix_height INTEGER NOT NULL,"
            "tile_width INTEGER NOT NULL,"
            "tile_height INTEGER NOT NULL,"
            "pixel_x_size DOUBLE NOT NULL,"
            "pixel_y_size DOUBLE NOT NULL,"
            "CONSTRAINT pk_ttm PRIMARY KEY (table_name, zoom_level),"
            "CONSTRAINT fk_tmm_table_name FOREIGN KEY (table_name) REFERENCES gpkg_contents(table_name)"
            ")";
            
        if ( OGRERR_NONE != SQLCommand(hDB, pszTileMatrix) )
            return FALSE;

        /* From D.1. gpkg_tile_matrix Table 39. gpkg_tile_matrix Trigger Definition SQL */
        const char* pszTileMatrixTrigger =
        "CREATE TRIGGER 'gpkg_tile_matrix_zoom_level_insert' "
        "BEFORE INSERT ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' violates constraint: zoom_level cannot be less than 0') "
        "WHERE (NEW.zoom_level < 0); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_zoom_level_update' "
        "BEFORE UPDATE of zoom_level ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' violates constraint: zoom_level cannot be less than 0') "
        "WHERE (NEW.zoom_level < 0); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_matrix_width_insert' "
        "BEFORE INSERT ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' violates constraint: matrix_width cannot be less than 1') "
        "WHERE (NEW.matrix_width < 1); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_matrix_width_update' "
        "BEFORE UPDATE OF matrix_width ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' violates constraint: matrix_width cannot be less than 1') "
        "WHERE (NEW.matrix_width < 1); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_matrix_height_insert' "
        "BEFORE INSERT ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' violates constraint: matrix_height cannot be less than 1') "
        "WHERE (NEW.matrix_height < 1); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_matrix_height_update' "
        "BEFORE UPDATE OF matrix_height ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' violates constraint: matrix_height cannot be less than 1') "
        "WHERE (NEW.matrix_height < 1); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_pixel_x_size_insert' "
        "BEFORE INSERT ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' violates constraint: pixel_x_size must be greater than 0') "
        "WHERE NOT (NEW.pixel_x_size > 0); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_pixel_x_size_update' "
        "BEFORE UPDATE OF pixel_x_size ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' violates constraint: pixel_x_size must be greater than 0') "
        "WHERE NOT (NEW.pixel_x_size > 0); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_pixel_y_size_insert' "
        "BEFORE INSERT ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''gpkg_tile_matrix'' violates constraint: pixel_y_size must be greater than 0') "
        "WHERE NOT (NEW.pixel_y_size > 0); "
        "END; "
        "CREATE TRIGGER 'gpkg_tile_matrix_pixel_y_size_update' "
        "BEFORE UPDATE OF pixel_y_size ON 'gpkg_tile_matrix' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''gpkg_tile_matrix'' violates constraint: pixel_y_size must be greater than 0') "
        "WHERE NOT (NEW.pixel_y_size > 0); "
        "END;";
        if ( OGRERR_NONE != SQLCommand(hDB, pszTileMatrixTrigger) )
            return FALSE;
        
        /* From C.10. gpkg_metadata Table 35. gpkg_metadata Table Definition SQL  */
        const char* pszMetadata =
            "CREATE TABLE gpkg_metadata ("
            "id INTEGER CONSTRAINT m_pk PRIMARY KEY ASC NOT NULL UNIQUE,"
            "md_scope TEXT NOT NULL DEFAULT 'dataset',"
            "md_standard_uri TEXT NOT NULL,"
            "mime_type TEXT NOT NULL DEFAULT 'text/xml',"
            "metadata TEXT NOT NULL"
            ")";
            
        if ( OGRERR_NONE != SQLCommand(hDB, pszMetadata) )
            return FALSE;

        /* From D.2. metadata Table 40. metadata Trigger Definition SQL  */
        const char* pszMetadataTriggers =
        "CREATE TRIGGER 'gpkg_metadata_md_scope_insert' "
        "BEFORE INSERT ON 'gpkg_metadata' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata violates "
        "constraint: md_scope must be one of undefined | fieldSession | "
        "collectionSession | series | dataset | featureType | feature | "
        "attributeType | attribute | tile | model | catalogue | schema | "
        "taxonomy software | service | collectionHardware | "
        "nonGeographicDataset | dimensionGroup') "
        "WHERE NOT(NEW.md_scope IN "
        "('undefined','fieldSession','collectionSession','series','dataset', "
        "'featureType','feature','attributeType','attribute','tile','model', "
        "'catalogue','schema','taxonomy','software','service', "
        "'collectionHardware','nonGeographicDataset','dimensionGroup')); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_md_scope_update' "
        "BEFORE UPDATE OF 'md_scope' ON 'gpkg_metadata' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata violates "
        "constraint: md_scope must be one of undefined | fieldSession | "
        "collectionSession | series | dataset | featureType | feature | "
        "attributeType | attribute | tile | model | catalogue | schema | "
        "taxonomy software | service | collectionHardware | "
        "nonGeographicDataset | dimensionGroup') "
        "WHERE NOT(NEW.md_scope IN "
        "('undefined','fieldSession','collectionSession','series','dataset', "
        "'featureType','feature','attributeType','attribute','tile','model', "
        "'catalogue','schema','taxonomy','software','service', "
        "'collectionHardware','nonGeographicDataset','dimensionGroup')); "
        "END";
        if ( OGRERR_NONE != SQLCommand(hDB, pszMetadataTriggers) )
            return FALSE;

        /* From C.11. gpkg_metadata_reference Table 36. gpkg_metadata_reference Table Definition SQL */
        const char* pszMetadataReference =
            "CREATE TABLE gpkg_metadata_reference ("
            "reference_scope TEXT NOT NULL,"
            "table_name TEXT,"
            "column_name TEXT,"
            "row_id_value INTEGER,"
            "timestamp DATETIME NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),"
            "md_file_id INTEGER NOT NULL,"
            "md_parent_id INTEGER,"
            "CONSTRAINT crmr_mfi_fk FOREIGN KEY (md_file_id) REFERENCES gpkg_metadata(id),"
            "CONSTRAINT crmr_mpi_fk FOREIGN KEY (md_parent_id) REFERENCES gpkg_metadata(id)"
            ")";
            
        if ( OGRERR_NONE != SQLCommand(hDB, pszMetadataReference) )
            return FALSE;

        /* From D.3. metadata_reference Table 41. gpkg_metadata_reference Trigger Definition SQL   */
        const char* pszMetadataReferenceTriggers =
            "CREATE TRIGGER 'gpkg_metadata_reference_reference_scope_insert' "
            "BEFORE INSERT ON 'gpkg_metadata_reference' "
            "FOR EACH ROW BEGIN "
            "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
            "violates constraint: reference_scope must be one of \"geopackage\", "
            "table\", \"column\", \"row\", \"row/col\"') "
            "WHERE NOT NEW.reference_scope IN "
            "('geopackage','table','column','row','row/col'); "
            "END; "
            "CREATE TRIGGER 'gpkg_metadata_reference_reference_scope_update' "
            "BEFORE UPDATE OF 'reference_scope' ON 'gpkg_metadata_reference' "
            "FOR EACH ROW BEGIN "
            "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
            "violates constraint: referrence_scope must be one of \"geopackage\", "
            "\"table\", \"column\", \"row\", \"row/col\"') "
            "WHERE NOT NEW.reference_scope IN "
            "('geopackage','table','column','row','row/col'); "
            "END; "
            "CREATE TRIGGER 'gpkg_metadata_reference_column_name_insert' "
            "BEFORE INSERT ON 'gpkg_metadata_reference' "
            "FOR EACH ROW BEGIN "
            "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
            "violates constraint: column name must be NULL when reference_scope "
            "is \"geopackage\", \"table\" or \"row\"') "
            "WHERE (NEW.reference_scope IN ('geopackage','table','row') "
            "AND NEW.column_name IS NOT NULL); "
            "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
            "violates constraint: column name must be defined for the specified "
            "table when reference_scope is \"column\" or \"row/col\"') "
            "WHERE (NEW.reference_scope IN ('column','row/col') "
            "AND NOT NEW.table_name IN ( "
            "SELECT name FROM SQLITE_MASTER WHERE type = 'table' "
            "AND name = NEW.table_name "
            "AND sql LIKE ('%' || NEW.column_name || '%'))); "
            "END; "
            "CREATE TRIGGER 'gpkg_metadata_reference_column_name_update' "
            "BEFORE UPDATE OF column_name ON 'gpkg_metadata_reference' "
            "FOR EACH ROW BEGIN "
            "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
            "violates constraint: column name must be NULL when reference_scope "
            "is \"geopackage\", \"table\" or \"row\"') "
            "WHERE (NEW.reference_scope IN ('geopackage','table','row') "
            "AND NEW.column_nameIS NOT NULL); "
            "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
            "violates constraint: column name must be defined for the specified "
            "table when reference_scope is \"column\" or \"row/col\"') "
            "WHERE (NEW.reference_scope IN ('column','row/col') "
            "AND NOT NEW.table_name IN ( "
            "SELECT name FROM SQLITE_MASTER WHERE type = 'table' "
            "AND name = NEW.table_name "
            "AND sql LIKE ('%' || NEW.column_name || '%'))); "
            "END; "
            "CREATE TRIGGER 'gpkg_metadata_reference_row_id_value_insert' "
            "BEFORE INSERT ON 'gpkg_metadata_reference' "
            "FOR EACH ROW BEGIN "
            "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
            "violates constraint: row_id_value must be NULL when reference_scope "
            "is \"geopackage\", \"table\" or \"column\"') "
            "WHERE NEW.reference_scope IN ('geopackage','table','column') "
            "AND NEW.row_id_value IS NOT NULL; "
            "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
            "violates constraint: row_id_value must exist in specified table when "
            "reference_scope is \"row\" or \"row/col\"') "
            "WHERE NEW.reference_scope IN ('row','row/col') "
            "AND NOT EXISTS (SELECT rowid "
            "FROM (SELECT NEW.table_name AS table_name) WHERE rowid = "
            "NEW.row_id_value); "
            "END; "
            "CREATE TRIGGER 'gpkg_metadata_reference_row_id_value_update' "
            "BEFORE UPDATE OF 'row_id_value' ON 'gpkg_metadata_reference' "
            "FOR EACH ROW BEGIN "
            "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
            "violates constraint: row_id_value must be NULL when reference_scope "
            "is \"geopackage\", \"table\" or \"column\"') "
            "WHERE NEW.reference_scope IN ('geopackage','table','column') "
            "AND NEW.row_id_value IS NOT NULL; "
            "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
            "violates constraint: row_id_value must exist in specified table when "
            "reference_scope is \"row\" or \"row/col\"') "
            "WHERE NEW.reference_scope IN ('row','row/col') "
            "AND NOT EXISTS (SELECT rowid "
            "FROM (SELECT NEW.table_name AS table_name) WHERE rowid = "
            "NEW.row_id_value); "
            "END; "
            "CREATE TRIGGER 'gpkg_metadata_reference_timestamp_insert' "
            "BEFORE INSERT ON 'gpkg_metadata_reference' "
            "FOR EACH ROW BEGIN "
            "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
            "violates constraint: timestamp must be a valid time in ISO 8601 "
            "\"yyyy-mm-ddThh:mm:ss.cccZ\" form') "
            "WHERE NOT (NEW.timestamp GLOB "
            "'[1-2][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T[0-2][0-9]:[0-5][0-9]:[0-5][0-9].[0-9][0-9][0-9]Z' "
            "AND strftime('%s',NEW.timestamp) NOT NULL); "
            "END; "
            "CREATE TRIGGER 'gpkg_metadata_reference_timestamp_update' "
            "BEFORE UPDATE OF 'timestamp' ON 'gpkg_metadata_reference' "
            "FOR EACH ROW BEGIN "
            "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
            "violates constraint: timestamp must be a valid time in ISO 8601 "
            "\"yyyy-mm-ddThh:mm:ss.cccZ\" form') "
            "WHERE NOT (NEW.timestamp GLOB "
            "'[1-2][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T[0-2][0-9]:[0-5][0-9]:[0-5][0-9].[0-9][0-9][0-9]Z' "
            "AND strftime('%s',NEW.timestamp) NOT NULL); "
            "END";
        if ( OGRERR_NONE != SQLCommand(hDB, pszMetadataReferenceTriggers) )
            return FALSE;

    }
    
    if( nBands != 0 )
    {
        const char* pszTableName = CPLGetBasename(m_pszFilename);
        m_osRasterTable = CSLFetchNameValueDef(papszOptions, "RASTER_TABLE", pszTableName);
        m_bIdentifierAsCO = CSLFetchNameValue(papszOptions, "RASTER_IDENTIFIER" ) != NULL;
        m_osIdentifier = CSLFetchNameValueDef(papszOptions, "RASTER_IDENTIFIER", m_osRasterTable);
        m_bDescriptionAsCO = CSLFetchNameValue(papszOptions, "RASTER_DESCRIPTION" ) != NULL;
        m_osDescription = CSLFetchNameValueDef(papszOptions, "RASTER_DESCRIPTION", "");

        /* From C.7. sample_tile_pyramid (Informative) Table 31. EXAMPLE: tiles table Create Table SQL (Informative) */
        char* pszSQL = sqlite3_mprintf("CREATE TABLE '%q' ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "zoom_level INTEGER NOT NULL,"
          "tile_column INTEGER NOT NULL,"
          "tile_row INTEGER NOT NULL,"
          "tile_data BLOB NOT NULL,"
          "UNIQUE (zoom_level, tile_column, tile_row)"
        ")", m_osRasterTable.c_str());
        OGRErr eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
        if ( OGRERR_NONE != eErr )
            return FALSE;

        /* From D.5. sample_tile_pyramid Table 43. tiles table Trigger Definition SQL  */
        char* pszSQLTriggers = sqlite3_mprintf("CREATE TRIGGER '%q_zoom_insert' "
        "BEFORE INSERT ON '%q' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''%q'' violates constraint: zoom_level not specified for table in gpkg_tile_matrix') "
        "WHERE NOT (NEW.zoom_level IN (SELECT zoom_level FROM gpkg_tile_matrix WHERE table_name = '%q')) ; "
        "END; "
        "CREATE TRIGGER '%q_zoom_update' "
        "BEFORE UPDATE OF zoom_level ON '%q' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''%q'' violates constraint: zoom_level not specified for table in gpkg_tile_matrix') "
        "WHERE NOT (NEW.zoom_level IN (SELECT zoom_level FROM gpkg_tile_matrix WHERE table_name = '%q')) ; "
        "END; "
        "CREATE TRIGGER '%q_tile_column_insert' "
        "BEFORE INSERT ON '%q' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''%q'' violates constraint: tile_column cannot be < 0') "
        "WHERE (NEW.tile_column < 0) ; "
        "SELECT RAISE(ABORT, 'insert on table ''%q'' violates constraint: tile_column must by < matrix_width specified for table and zoom level in gpkg_tile_matrix') "
        "WHERE NOT (NEW.tile_column < (SELECT matrix_width FROM gpkg_tile_matrix WHERE table_name = '%q' AND zoom_level = NEW.zoom_level)); "
        "END; "
        "CREATE TRIGGER '%q_tile_column_update' "
        "BEFORE UPDATE OF tile_column ON '%q' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''%q'' violates constraint: tile_column cannot be < 0') "
        "WHERE (NEW.tile_column < 0) ; "
        "SELECT RAISE(ABORT, 'update on table ''%q'' violates constraint: tile_column must by < matrix_width specified for table and zoom level in gpkg_tile_matrix') "
        "WHERE NOT (NEW.tile_column < (SELECT matrix_width FROM gpkg_tile_matrix WHERE table_name = '%q' AND zoom_level = NEW.zoom_level)); "
        "END; "
        "CREATE TRIGGER '%q_tile_row_insert' "
        "BEFORE INSERT ON '%q' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table ''%q'' violates constraint: tile_row cannot be < 0') "
        "WHERE (NEW.tile_row < 0) ; "
        "SELECT RAISE(ABORT, 'insert on table ''%q'' violates constraint: tile_row must by < matrix_height specified for table and zoom level in gpkg_tile_matrix') "
        "WHERE NOT (NEW.tile_row < (SELECT matrix_height FROM gpkg_tile_matrix WHERE table_name = '%q' AND zoom_level = NEW.zoom_level)); "
        "END; "
        "CREATE TRIGGER '%q_tile_row_update' "
        "BEFORE UPDATE OF tile_row ON '%q' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table ''%q'' violates constraint: tile_row cannot be < 0') "
        "WHERE (NEW.tile_row < 0) ; "
        "SELECT RAISE(ABORT, 'update on table ''%q'' violates constraint: tile_row must by < matrix_height specified for table and zoom level in gpkg_tile_matrix') "
        "WHERE NOT (NEW.tile_row < (SELECT matrix_height FROM gpkg_tile_matrix WHERE table_name = '%q' AND zoom_level = NEW.zoom_level)); "
        "END; ",
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str(),
        m_osRasterTable.c_str()
        );
        eErr = SQLCommand(hDB, pszSQLTriggers);
        sqlite3_free(pszSQLTriggers);
        if ( OGRERR_NONE != eErr )
            return FALSE;

        nRasterXSize = nXSize;
        nRasterYSize = nYSize;

        const char* pszTileSize = CSLFetchNameValueDef(papszOptions, "BLOCKSIZE", "256");
        const char* pszTileWidth = CSLFetchNameValueDef(papszOptions, "BLOCKXSIZE", pszTileSize);
        const char* pszTileHeight = CSLFetchNameValueDef(papszOptions, "BLOCKYSIZE", pszTileSize);
        int nTileWidth = atoi(pszTileWidth);
        int nTileHeight = atoi(pszTileHeight);
        if( nTileWidth < 8 || nTileWidth > 4096 || nTileHeight < 8 || nTileHeight > 4096 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid block dimensions: %dx%d",
                     nTileWidth, nTileHeight);
            return FALSE;
        }

        m_pabyCachedTiles = (GByte*) VSIMalloc3(4 * 4, nTileWidth, nTileHeight);
        if( m_pabyCachedTiles == NULL )
        {
            return FALSE;
        }

        for(int i = 1; i <= nBands; i ++)
            SetBand( i, new GDALGeoPackageRasterBand(this, i, nTileWidth, nTileHeight) );

        GDALPamDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
        GDALPamDataset::SetMetadataItem("IDENTIFIER", m_osIdentifier);
        if( m_osDescription.size() )
            GDALPamDataset::SetMetadataItem("DESCRIPTION", m_osDescription);

        const char* pszTF = CSLFetchNameValue(papszOptions, "DRIVER");
        if( pszTF )
            m_eTF = GetTileFormat(pszTF);

        ParseCompressionOptions(papszOptions);

        if( m_eTF == GPKG_TF_WEBP )
        {
            if( !RegisterWebPExtension() )
                return FALSE;
        }
    }

    /* Requirement 2: A GeoPackage SHALL contain 0x47503130 ("GP10" in ASCII) */
    /* in the application id field of the SQLite database header */
    /* We have to do this after there's some content so the database file */
    /* is not zero length */
    SetApplicationId();


    return TRUE;
}

/************************************************************************/
/*                        ParseCompressionOptions()                     */
/************************************************************************/

void GDALGeoPackageDataset::ParseCompressionOptions(char** papszOptions)
{
    const char* pszZLevel = CSLFetchNameValue(papszOptions, "ZLEVEL");
    if( pszZLevel )
        m_nZLevel = atoi(pszZLevel);

    const char* pszQuality = CSLFetchNameValue(papszOptions, "QUALITY");
    if( pszQuality )
        m_nQuality = atoi(pszQuality);

    const char* pszDither = CSLFetchNameValue(papszOptions, "DITHER");
    if( pszDither )
        m_bDither = CSLTestBoolean(pszDither);
}

/************************************************************************/
/*                          RegisterWebPExtension()                     */
/************************************************************************/

int GDALGeoPackageDataset::RegisterWebPExtension()
{
    CreateExtensionsTableIfNecessary();

    char* pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_extensions "
        "(table_name, column_name, extension_name, definition, scope) "
        "VALUES "
        "('%q', 'tile_data', 'gpkg_webp', 'GeoPackage 1.0 Specification Annex P', 'read-write')",
        m_osRasterTable.c_str());
    OGRErr eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if ( OGRERR_NONE != eErr )
        return FALSE;
    return TRUE;
}

/************************************************************************/
/*                       RegisterZoomOtherExtension()                   */
/************************************************************************/

int GDALGeoPackageDataset::RegisterZoomOtherExtension()
{
    CreateExtensionsTableIfNecessary();

    char* pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_extensions "
        "(table_name, extension_name, definition, scope) "
        "VALUES "
        "('%q', 'gpkg_zoom_other', 'GeoPackage 1.0 Specification Annex O', 'read-write')",
        m_osRasterTable.c_str());
    OGRErr eErr = SQLCommand(hDB, pszSQL);
    sqlite3_free(pszSQL);
    if ( OGRERR_NONE != eErr )
        return FALSE;
    return TRUE;
}

/************************************************************************/
/*                              AddColumn()                             */
/************************************************************************/

OGRErr GDALGeoPackageDataset::AddColumn(const char *pszTableName, const char *pszColumnName, const char *pszColumnType)
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

OGRLayer* GDALGeoPackageDataset::GetLayer( int iLayer )

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

OGRLayer* GDALGeoPackageDataset::ICreateLayer( const char * pszLayerName,
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
                  m_pszFilename, pszLayerName );

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
    int nSRSId = DEFAULT_SRID;
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
        int bGeometryTypeHasZ = wkbHasZ(eGType);

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

    if( OGR_GT_IsNonLinear( eGType ) )
        poLayer->CreateGeometryExtensionIfNecessary(eGType);

    m_papoLayers = (OGRGeoPackageTableLayer**)CPLRealloc(m_papoLayers,  sizeof(OGRGeoPackageTableLayer*) * (m_nLayers+1));
    m_papoLayers[m_nLayers++] = poLayer;
    return poLayer;
}


/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

int GDALGeoPackageDataset::DeleteLayer( int iLayer )
{
    char *pszSQL;

    if( !bUpdate || iLayer < 0 || iLayer >= m_nLayers )
        return OGRERR_FAILURE;

    CPLString osLayerName = m_papoLayers[iLayer]->GetLayerDefn()->GetName();

    CPLDebug( "GPKG", "DeleteLayer(%s)", osLayerName.c_str() );

    if( m_papoLayers[iLayer]->HasSpatialIndex() )
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

int GDALGeoPackageDataset::TestCapability( const char * pszCap )
{
    if ( EQUAL(pszCap,ODsCCreateLayer) ||
         EQUAL(pszCap,ODsCDeleteLayer) )
    {
         return bUpdate;
    }
    else if( EQUAL(pszCap,ODsCCurveGeometries) )
        return TRUE;
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

OGRLayer * GDALGeoPackageDataset::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )

{
    m_bHasReadMetadataFromStorage = FALSE;
    FlushMetadata();
    for( int i = 0; i < m_nLayers; i++ )
    {
        m_papoLayers[i]->CreateSpatialIndexIfNecessary();
    }

    if( pszDialect != NULL && EQUAL(pszDialect,"OGRSQL") )
        return GDALDataset::ExecuteSQL( pszSQLCommand, 
                                          poSpatialFilter, 
                                          pszDialect );
    else if( pszDialect != NULL && EQUAL(pszDialect,"INDIRECT_SQLITE") )
        return GDALDataset::ExecuteSQL( pszSQLCommand, 
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

void GDALGeoPackageDataset::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                         HasExtensionsTable()                         */
/************************************************************************/

int GDALGeoPackageDataset::HasExtensionsTable()
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
/*                    CheckUnknownExtensions()                          */
/************************************************************************/

void GDALGeoPackageDataset::CheckUnknownExtensions(int bCheckRasterTable)
{
    if( !HasExtensionsTable() )
        return;

    char* pszSQL;
    if( !bCheckRasterTable)
        pszSQL = sqlite3_mprintf(
            "SELECT extension_name, definition, scope FROM gpkg_extensions WHERE table_name IS NULL AND extension_name != 'gdal_aspatial'");
    else
        pszSQL = sqlite3_mprintf(
            "SELECT extension_name, definition, scope FROM gpkg_extensions WHERE table_name = '%q'",
            m_osRasterTable.c_str());

    SQLResult oResultTable;
    OGRErr err = SQLQuery(GetDB(), pszSQL, &oResultTable);
    sqlite3_free(pszSQL);
    if ( err == OGRERR_NONE && oResultTable.nRowCount > 0 )
    {
        for(int i=0; i<oResultTable.nRowCount;i++)
        {
            const char* pszExtName = SQLResultGetValue(&oResultTable, 0, i);
            const char* pszDefinition = SQLResultGetValue(&oResultTable, 1, i);
            const char* pszScope = SQLResultGetValue(&oResultTable, 2, i);
            if( pszExtName == NULL ) pszExtName = "(null)";
            if( pszDefinition == NULL ) pszDefinition = "(null)";
            if( pszScope == NULL ) pszScope = "(null)";

            if( EQUAL(pszExtName, "gpkg_webp") )
            {
                if( GDALGetDriverByName("WEBP") == NULL )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Table %s contains WEBP tiles, but GDAL configured "
                             "without WEBP support. Data will be missing",
                             m_osRasterTable.c_str());
                }
                m_eTF = GPKG_TF_WEBP;
                continue;
            }
            if( EQUAL(pszExtName, "gpkg_zoom_other") )
            {
                m_bZoomOther = TRUE;
                continue;
            }

            if( GetUpdate() && EQUAL(pszScope, "write-only") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Database relies on the '%s' (%s) extension that should "
                         "be implemented for safe write-support, but is not currently. "
                         "Update of that database are strongly discouraged to avoid corruption.",
                         pszExtName, pszDefinition);
            }
            else if( GetUpdate() && EQUAL(pszScope, "read-write") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Database relies on the '%s' (%s) extension that should "
                         "be implemented in order to read/write it safely, but is not currently. "
                         "Some data may be missing while reading that database, and updates are strongly discouraged.",
                         pszExtName, pszDefinition);
            }
            else if( EQUAL(pszScope, "read-write") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Database relies on the '%s' (%s) extension that should "
                         "be implemented in order to read it safely, but is not currently. "
                         "Some data may be missing while reading that database.",
                         pszExtName, pszDefinition);
            }
        }
    }
    SQLResultFree(&oResultTable);
}

/************************************************************************/
/*                         HasGDALAspatialExtension()                       */
/************************************************************************/

int GDALGeoPackageDataset::HasGDALAspatialExtension()
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

OGRErr GDALGeoPackageDataset::CreateGDALAspatialExtension()
{
    CreateExtensionsTableIfNecessary();

    if( HasGDALAspatialExtension() )
        return OGRERR_NONE;

    const char* pszCreateAspatialExtension =
        "INSERT INTO gpkg_extensions "
        "(table_name, column_name, extension_name, definition, scope) "
        "VALUES "
        "(NULL, NULL, 'gdal_aspatial', 'http://gdal.org/geopackage_aspatial.html', 'read-write')";

    return SQLCommand(hDB, pszCreateAspatialExtension);
}

/************************************************************************/
/*                  CreateExtensionsTableIfNecessary()                  */
/************************************************************************/

OGRErr GDALGeoPackageDataset::CreateExtensionsTableIfNecessary()
{
    /* Check if the table gpkg_extensions exists */
    if( HasExtensionsTable() )
        return OGRERR_NONE;

    /* Requirement 79 : Every extension of a GeoPackage SHALL be registered */
    /* in a corresponding row in the gpkg_extensions table. The absence of a */
    /* gpkg_extensions table or the absence of rows in gpkg_extensions table */
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
    OGRErr err = OGRReadWKBGeometryType( (GByte*)pabyBLOB + sHeader.szHeader,
                                         wkbVariantIso, &eGeometryType, &bIs3D );
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
    int bIsAssignable = OGR_GT_IsSubClassOf( OGRFromOGCGeomType(pszActual),
                                             OGRFromOGCGeomType(pszExpected) );
    sqlite3_result_int( pContext, bIsAssignable );
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
    GDALGeoPackageDataset* poDS = (GDALGeoPackageDataset* )sqlite3_user_data(pContext);
    
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
    GDALGeoPackageDataset* poDS = (GDALGeoPackageDataset* )sqlite3_user_data(pContext);
    
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
/*                      GPKG_GDAL_GetMemFileFromBlob()                  */
/************************************************************************/

static CPLString GPKG_GDAL_GetMemFileFromBlob(sqlite3_value** argv)
{
    int nBytes = sqlite3_value_bytes (argv[0]);
    const GByte* pabyBLOB = (const GByte *) sqlite3_value_blob (argv[0]);
    CPLString osMemFileName;
    osMemFileName.Printf("/vsimem/GPKG_GDAL_GetMemFileFromBlob_%p", argv);
    VSILFILE * fp = VSIFileFromMemBuffer( osMemFileName.c_str(), (GByte*)pabyBLOB,
                                          nBytes, FALSE);
    VSIFCloseL(fp);
    return osMemFileName;
}

/************************************************************************/
/*                       GPKG_GDAL_GetMimeType()                        */
/************************************************************************/

static
void GPKG_GDAL_GetMimeType(sqlite3_context* pContext,
                           CPL_UNUSED int argc,
                           sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        sqlite3_result_null (pContext);
        return;
    }

    CPLString osMemFileName(GPKG_GDAL_GetMemFileFromBlob(argv));
    GDALDriver* poDriver = (GDALDriver*)GDALIdentifyDriver(osMemFileName, NULL);
    if( poDriver != NULL )
    {
        const char* pszRes;
        if( EQUAL(poDriver->GetDescription(), "PNG") )
            pszRes = "image/png";
        else if( EQUAL(poDriver->GetDescription(), "JPEG") )
            pszRes = "image/jpeg";
        else if( EQUAL(poDriver->GetDescription(), "WEBP") )
            pszRes = "image/x-webp";
        else
            pszRes = CPLSPrintf("gdal/%s", poDriver->GetDescription());
        sqlite3_result_text( pContext, pszRes, -1, SQLITE_TRANSIENT );
    }
    else
        sqlite3_result_null (pContext);
    VSIUnlink(osMemFileName);
}

/************************************************************************/
/*                       GPKG_GDAL_GetBandCount()                       */
/************************************************************************/

static
void GPKG_GDAL_GetBandCount(sqlite3_context* pContext,
                           CPL_UNUSED int argc,
                           sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        sqlite3_result_null (pContext);
        return;
    }

    CPLString osMemFileName(GPKG_GDAL_GetMemFileFromBlob(argv));
    GDALDataset* poDS = (GDALDataset*) GDALOpenEx(osMemFileName,
                                                  GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                                                  NULL, NULL, NULL);
    if( poDS != NULL )
    {
        sqlite3_result_int( pContext, poDS->GetRasterCount() );
        GDALClose( poDS );
    }
    else
        sqlite3_result_null (pContext);
    VSIUnlink(osMemFileName);
}

/************************************************************************/
/*                       GPKG_GDAL_HasColorTable()                      */
/************************************************************************/

static
void GPKG_GDAL_HasColorTable(sqlite3_context* pContext,
                             CPL_UNUSED int argc,
                             sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        sqlite3_result_null (pContext);
        return;
    }

    CPLString osMemFileName(GPKG_GDAL_GetMemFileFromBlob(argv));
    GDALDataset* poDS = (GDALDataset*) GDALOpenEx(osMemFileName,
                                                  GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                                                  NULL, NULL, NULL);
    if( poDS != NULL )
    {
        sqlite3_result_int( pContext,
                            poDS->GetRasterCount() == 1 &&
                            poDS->GetRasterBand(1)->GetColorTable() != NULL );
        GDALClose( poDS );
    }
    else
        sqlite3_result_null (pContext);
    VSIUnlink(osMemFileName);
}

/************************************************************************/
/*                         OpenOrCreateDB()                             */
/************************************************************************/

int GDALGeoPackageDataset::OpenOrCreateDB(int flags)
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
    
    // Debug functions
    if( CSLTestBoolean(CPLGetConfigOption("GPKG_DEBUG", "FALSE")) )
    {
        sqlite3_create_function(hDB, "GDAL_GetMimeType", 1, SQLITE_ANY, NULL,
                                GPKG_GDAL_GetMimeType, NULL, NULL);
        sqlite3_create_function(hDB, "GDAL_GetBandCount", 1, SQLITE_ANY, NULL,
                                GPKG_GDAL_GetBandCount, NULL, NULL);
        sqlite3_create_function(hDB, "GDAL_HasColorTable", 1, SQLITE_ANY, NULL,
                                GPKG_GDAL_HasColorTable, NULL, NULL);
    }

    return TRUE;
}

/************************************************************************/
/*                   GetLayerWithGetSpatialWhereByName()                */
/************************************************************************/

std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>
    GDALGeoPackageDataset::GetLayerWithGetSpatialWhereByName( const char* pszName )
{
    OGRGeoPackageLayer* poRet = (OGRGeoPackageLayer*) GetLayerByName(pszName);
    return std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>(poRet, poRet);
}
