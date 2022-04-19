/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 *
 * Contributor: Alessandro Furieri, a.furieri@lqt.it
 * Portions of this module properly supporting SpatiaLite Table/Geom creation
 * Developed for Faunalia ( http://www.faunalia.it) with funding from
 * Regione Toscana - Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogr_sqlite.h"
#include "ogrsqlitevirtualogr.h"
#include "ogrsqliteutility.h"
#include "ogrsqlitevfs.h"

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_hash_set.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "sqlite3.h"

#include "proj.h"
#include "ogr_proj_p.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdocumentation"
#endif

#if defined(HAVE_SPATIALITE) && !defined(SPATIALITE_DLOPEN)
#include "spatialite.h"
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#undef SQLITE_STATIC
#define SQLITE_STATIC      ((sqlite3_destructor_type)nullptr)

#ifndef SPATIALITE_412_OR_LATER
static bool bSpatialiteGlobalLoaded = false;
#endif

// Keep in sync prototype of those 2 functions between gdalopeninfo.cpp,
// ogrsqlitedatasource.cpp and ogrgeopackagedatasource.cpp
void GDALOpenInfoDeclareFileNotToOpen(const char* pszFilename,
                                       const GByte* pabyHeader,
                                       int nHeaderBytes);
void GDALOpenInfoUnDeclareFileNotToOpen(const char* pszFilename);


CPL_CVSID("$Id$")

/************************************************************************/
/*                      OGRSQLiteInitOldSpatialite()                    */
/************************************************************************/

#ifndef SPATIALITE_412_OR_LATER

#ifdef HAVE_SPATIALITE
static const char *(*pfn_spatialite_version) (void) = spatialite_version;
#endif

static int OGRSQLiteInitOldSpatialite()
{
/* -------------------------------------------------------------------- */
/*      Try loading SpatiaLite.                                         */
/* -------------------------------------------------------------------- */
#ifdef HAVE_SPATIALITE
    if (!bSpatialiteGlobalLoaded && CPLTestBool(CPLGetConfigOption("SPATIALITE_LOAD", "TRUE")))
    {
        bSpatialiteGlobalLoaded = true;
        spatialite_init(CPLTestBool(CPLGetConfigOption("SPATIALITE_INIT_VERBOSE", "FALSE")));
    }
#endif
    return bSpatialiteGlobalLoaded;
}

/************************************************************************/
/*                          InitNewSpatialite()                         */
/************************************************************************/

bool OGRSQLiteBaseDataSource::InitNewSpatialite()
{
    (void)hSpatialiteCtxt;
    return true;
}

/************************************************************************/
/*                         FinishNewSpatialite()                        */
/************************************************************************/

void OGRSQLiteBaseDataSource::FinishNewSpatialite()
{
}

/************************************************************************/
/*                          OGRSQLiteDriverUnload()                     */
/************************************************************************/

void OGRSQLiteDriverUnload(GDALDriver*)
{
}

#else // defined(SPATIALITE_412_OR_LATER)

#ifdef SPATIALITE_DLOPEN
static CPLMutex* hMutexLoadSpatialiteSymbols = nullptr;
static void * (*pfn_spatialite_alloc_connection) (void) = nullptr;
static void (*pfn_spatialite_shutdown) (void) = nullptr;
static void (*pfn_spatialite_init_ex) (sqlite3*, const void *, int) = nullptr;
static void (*pfn_spatialite_cleanup_ex) (const void *) = nullptr;
static const char *(*pfn_spatialite_version) (void) = nullptr;
#else
static void * (*pfn_spatialite_alloc_connection) (void) = spatialite_alloc_connection;
static void (*pfn_spatialite_shutdown) (void) = spatialite_shutdown;
static void (*pfn_spatialite_init_ex) (sqlite3*, const void *, int) = spatialite_init_ex;
static void (*pfn_spatialite_cleanup_ex) (const void *) = spatialite_cleanup_ex;
static const char *(*pfn_spatialite_version) (void) = spatialite_version;
#endif

#ifndef SPATIALITE_SONAME
#define SPATIALITE_SONAME "libspatialite.so"
#endif

#ifdef SPATIALITE_DLOPEN
static bool OGRSQLiteLoadSpatialiteSymbols()
{
    static bool bInitializationDone = false;
    CPLMutexHolderD(&hMutexLoadSpatialiteSymbols);
    if( bInitializationDone )
      return pfn_spatialite_alloc_connection != nullptr;
    bInitializationDone = true;

    const char *pszLibName = CPLGetConfigOption("SPATIALITESO",
                                                SPATIALITE_SONAME);
    CPLPushErrorHandler( CPLQuietErrorHandler );

    /* coverity[tainted_string] */
    pfn_spatialite_alloc_connection = (void* (*)(void))
                    CPLGetSymbol( pszLibName, "spatialite_alloc_connection" );
    CPLPopErrorHandler();

    if( pfn_spatialite_alloc_connection == nullptr )
    {
        CPLDebug("SQLITE", "Cannot find %s in %s", "spatialite_alloc_connection",
                 pszLibName);
        return false;
    }

    pfn_spatialite_shutdown = (void (*)(void))
                    CPLGetSymbol( pszLibName, "spatialite_shutdown" );
    pfn_spatialite_init_ex = (void (*)(sqlite3*, const void *, int))
                    CPLGetSymbol( pszLibName, "spatialite_init_ex" );
    pfn_spatialite_cleanup_ex = (void (*)(const void *))
                    CPLGetSymbol( pszLibName, "spatialite_cleanup_ex" );
    pfn_spatialite_version = (const char* (*)(void))
                    CPLGetSymbol( pszLibName, "spatialite_version" );
    if( pfn_spatialite_shutdown == nullptr ||
        pfn_spatialite_init_ex == nullptr ||
        pfn_spatialite_cleanup_ex == nullptr ||
        pfn_spatialite_version == nullptr )
    {
        pfn_spatialite_shutdown = nullptr;
        pfn_spatialite_init_ex = nullptr;
        pfn_spatialite_cleanup_ex = nullptr;
        pfn_spatialite_version = nullptr;
        return false;
    }
    return true;
}
#endif

/************************************************************************/
/*                          OGRSQLiteDriverUnload()                     */
/************************************************************************/

void OGRSQLiteDriverUnload(GDALDriver*)
{
    if( pfn_spatialite_shutdown != nullptr )
        pfn_spatialite_shutdown();
#ifdef SPATIALITE_DLOPEN
    if( hMutexLoadSpatialiteSymbols != nullptr )
    {
        CPLDestroyMutex(hMutexLoadSpatialiteSymbols);
        hMutexLoadSpatialiteSymbols = nullptr;
    }
#endif
}

/************************************************************************/
/*                          InitNewSpatialite()                         */
/************************************************************************/

bool OGRSQLiteBaseDataSource::InitNewSpatialite()
{
    if( hSpatialiteCtxt == nullptr &&
        CPLTestBool(CPLGetConfigOption("SPATIALITE_LOAD", "TRUE")) )
    {
#ifdef SPATIALITE_DLOPEN
        if( !OGRSQLiteLoadSpatialiteSymbols() )
            return false;
#endif
        CPLAssert(hSpatialiteCtxt == nullptr);
        hSpatialiteCtxt = pfn_spatialite_alloc_connection();
        if( hSpatialiteCtxt != nullptr )
        {
            pfn_spatialite_init_ex(hDB, hSpatialiteCtxt,
                CPLTestBool(CPLGetConfigOption("SPATIALITE_INIT_VERBOSE", "FALSE")));
        }
    }
    return hSpatialiteCtxt != nullptr;
}

/************************************************************************/
/*                         FinishNewSpatialite()                        */
/************************************************************************/

void OGRSQLiteBaseDataSource::FinishNewSpatialite()
{
    if( hSpatialiteCtxt != nullptr )
    {
        pfn_spatialite_cleanup_ex(hSpatialiteCtxt);
        hSpatialiteCtxt = nullptr;
    }
}

#endif // defined(SPATIALITE_412_OR_LATER)

/************************************************************************/
/*                          IsSpatialiteLoaded()                        */
/************************************************************************/

bool OGRSQLiteDataSource::IsSpatialiteLoaded()
{
#ifdef SPATIALITE_412_OR_LATER
    return hSpatialiteCtxt != nullptr;
#else
    return bSpatialiteGlobalLoaded;
#endif
}

/************************************************************************/
/*                     GetSpatialiteVersionNumber()                     */
/************************************************************************/

int OGRSQLiteDataSource::GetSpatialiteVersionNumber()
{
    int v = 0;
#ifdef HAVE_SPATIALITE
    if( IsSpatialiteLoaded() )
    {
        v = (int)(( CPLAtof( pfn_spatialite_version() ) + 0.001 )  * 10.0);
    }
#endif
    return v;
}

/************************************************************************/
/*                       OGRSQLiteBaseDataSource()                      */
/************************************************************************/

OGRSQLiteBaseDataSource::OGRSQLiteBaseDataSource() = default;

/************************************************************************/
/*                      ~OGRSQLiteBaseDataSource()                      */
/************************************************************************/

OGRSQLiteBaseDataSource::~OGRSQLiteBaseDataSource()

{
    CloseDB();

    FinishNewSpatialite();

    if( m_bCallUndeclareFileNotToOpen )
    {
        GDALOpenInfoUnDeclareFileNotToOpen(m_pszFilename);
    }

    if( !m_osFinalFilename.empty() )
    {
        if( !bSuppressOnClose )
        {
            CPLDebug("SQLITE", "Copying temporary file %s onto %s",
                     m_pszFilename, m_osFinalFilename.c_str());
            if( CPLCopyFile(m_osFinalFilename.c_str(), m_pszFilename) != 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Copy temporary file %s onto %s failed",
                         m_pszFilename, m_osFinalFilename.c_str());
            }
        }
        CPLDebug("SQLITE", "Deleting temporary file %s", m_pszFilename);
        if( VSIUnlink(m_pszFilename) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Deleting temporary file %s failed", m_pszFilename);
        }
    }

    CPLFree(m_pszFilename);
}

/************************************************************************/
/*                               CloseDB()                              */
/************************************************************************/

void OGRSQLiteBaseDataSource::CloseDB()
{
    if( hDB != nullptr )
    {
        sqlite3_close( hDB );
        hDB = nullptr;

        // If we opened the DB in read-only mode, there might be spurious
        // -wal and -shm files that we can make disappear by reopening in
        // read-write
        VSIStatBufL sStat;
        if( eAccess == GA_ReadOnly &&
            !(STARTS_WITH(m_pszFilename, "/vsicurl/") ||
              STARTS_WITH(m_pszFilename, "/vsitar/") ||
              STARTS_WITH(m_pszFilename, "/vsizip/")) &&
            VSIStatL( CPLSPrintf("%s-wal", m_pszFilename), &sStat) == 0 )
        {
            CPL_IGNORE_RET_VAL( sqlite3_open( m_pszFilename, &hDB ) );
            if( hDB != nullptr )
            {
#ifdef SQLITE_FCNTL_PERSIST_WAL
                int nPersistentWAL = -1;
                sqlite3_file_control(hDB, "main", SQLITE_FCNTL_PERSIST_WAL, &nPersistentWAL);
                if( nPersistentWAL == 1 )
                {
                    nPersistentWAL = 0;
                    if( sqlite3_file_control(hDB, "main", SQLITE_FCNTL_PERSIST_WAL, &nPersistentWAL) == SQLITE_OK )
                    {
                        CPLDebug("SQLITE", "Disabling persistent WAL succeeded");
                    }
                    else
                    {
                        CPLDebug("SQLITE", "Could not disable persistent WAL");
                    }
                }
#endif

                // Dummy request
                int nRowCount = 0, nColCount = 0;
                char** papszResult = nullptr;
                sqlite3_get_table( hDB,
                                    "SELECT name FROM sqlite_master WHERE 0",
                                    &papszResult, &nRowCount, &nColCount,
                                    nullptr );
                sqlite3_free_table( papszResult );

                sqlite3_close( hDB );
                hDB = nullptr;
#ifdef DEBUG_VERBOSE
                if( VSIStatL( CPLSPrintf("%s-wal", m_pszFilename), &sStat) != 0 )
                {
                    CPLDebug("SQLite", "%s-wal file has been removed", m_pszFilename);
                }
#endif
            }
        }

    }

    if (pMyVFS)
    {
        sqlite3_vfs_unregister(pMyVFS);
        CPLFree(pMyVFS->pAppData);
        CPLFree(pMyVFS);
        pMyVFS = nullptr;
    }
}

/* Returns the first row of first column of SQL as integer */
OGRErr OGRSQLiteBaseDataSource::PragmaCheck(
    const char * pszPragma, const char * pszExpected, int nRowsExpected )
{
    CPLAssert( pszPragma != nullptr );
    CPLAssert( pszExpected != nullptr );
    CPLAssert( nRowsExpected >= 0 );

    char **papszResult = nullptr;
    int nRowCount = 0;
    int nColCount = 0;
    char *pszErrMsg = nullptr;

    int rc = sqlite3_get_table(
        hDB,
        CPLSPrintf("PRAGMA %s", pszPragma),
        &papszResult, &nRowCount, &nColCount, &pszErrMsg );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to execute PRAGMA %s: %s", pszPragma,
                  pszErrMsg ? pszErrMsg : "(null)" );
        sqlite3_free( pszErrMsg );
        return OGRERR_FAILURE;
    }

    if ( nRowCount != nRowsExpected )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "bad result for PRAGMA %s, got %d rows, expected %d",
                  pszPragma, nRowCount, nRowsExpected );
        sqlite3_free_table(papszResult);
        return OGRERR_FAILURE;
    }

    if ( nRowCount > 0 && ! EQUAL(papszResult[1], pszExpected) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "invalid %s (expected '%s', got '%s')",
                  pszPragma, pszExpected, papszResult[1]);
        sqlite3_free_table(papszResult);
        return OGRERR_FAILURE;
    }

    sqlite3_free_table(papszResult);

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OGRSQLiteDataSource()                         */
/************************************************************************/

OGRSQLiteDataSource::OGRSQLiteDataSource()
{
    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                        ~OGRSQLiteDataSource()                        */
/************************************************************************/

OGRSQLiteDataSource::~OGRSQLiteDataSource()

{
#ifdef HAVE_RASTERLITE2
    if( m_pRL2Coverage != nullptr )
    {
        rl2_destroy_coverage( m_pRL2Coverage );
    }
#endif
    for( size_t i=0; i < m_apoOverviewDS.size(); ++i )
    {
        delete m_apoOverviewDS[i];
    }

    if( m_nLayers > 0 || !m_apoInvisibleLayers.empty() )
    {
        // Close any remaining iterator
        for( int i = 0; i < m_nLayers; i++ )
            m_papoLayers[i]->ResetReading();
        for( size_t i = 0; i < m_apoInvisibleLayers.size(); i++ )
            m_apoInvisibleLayers[i]->ResetReading();

        // Create spatial indices in a transaction for faster execution
        if( hDB )
            SoftStartTransaction();
        for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
        {
            if( m_papoLayers[iLayer]->IsTableLayer() )
            {
                OGRSQLiteTableLayer* poLayer =
                    (OGRSQLiteTableLayer*) m_papoLayers[iLayer];
                poLayer->RunDeferredCreationIfNecessary();
                poLayer->CreateSpatialIndexIfNecessary();
            }
        }
        if( hDB )
            SoftCommitTransaction();
    }

    SaveStatistics();

    for( int i = 0; i < m_nLayers; i++ )
        delete m_papoLayers[i];
    for( size_t i = 0; i < m_apoInvisibleLayers.size(); i++ )
        delete m_apoInvisibleLayers[i];

    CPLFree( m_papoLayers );

    for( int i = 0; i < m_nKnownSRID; i++ )
    {
        if( m_papoSRS[i] != nullptr )
            m_papoSRS[i]->Release();
    }
    CPLFree( m_panSRID );
    CPLFree( m_papoSRS );

    CloseDB();
#ifdef HAVE_RASTERLITE2
    FinishRasterLite2();
#endif
}

#ifdef HAVE_RASTERLITE2

/************************************************************************/
/*                          InitRasterLite2()                           */
/************************************************************************/

bool OGRSQLiteDataSource::InitRasterLite2()
{
    CPLAssert(m_hRL2Ctxt == nullptr);
    m_hRL2Ctxt = rl2_alloc_private();
    if( m_hRL2Ctxt != nullptr )
    {
        rl2_init (hDB, m_hRL2Ctxt, 0);
    }
    return m_hRL2Ctxt != nullptr;
}

/************************************************************************/
/*                         FinishRasterLite2()                          */
/************************************************************************/

void OGRSQLiteDataSource::FinishRasterLite2()
{
    if( m_hRL2Ctxt != nullptr )
    {
        rl2_cleanup_private(m_hRL2Ctxt);
        m_hRL2Ctxt = nullptr;
    }
}

#endif // HAVE_RASTERLITE2

/************************************************************************/
/*                              SaveStatistics()                        */
/************************************************************************/

void OGRSQLiteDataSource::SaveStatistics()
{
    if( !m_bIsSpatiaLiteDB || !IsSpatialiteLoaded() ||
        m_bLastSQLCommandIsUpdateLayerStatistics || !GetUpdate() )
        return;

    int nSavedAllLayersCacheData = -1;

    for( int i = 0; i < m_nLayers; i++ )
    {
        if( m_papoLayers[i]->IsTableLayer() )
        {
            OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) m_papoLayers[i];
            int nSaveRet = poLayer->SaveStatistics();
            if( nSaveRet >= 0)
            {
                if( nSavedAllLayersCacheData < 0 )
                    nSavedAllLayersCacheData = nSaveRet;
                else
                    nSavedAllLayersCacheData &= nSaveRet;
            }
        }
    }

    if( hDB && nSavedAllLayersCacheData == TRUE )
    {
        int nReplaceEventId = -1;

        auto oResult = SQLQuery( hDB,
                  "SELECT event_id, table_name, geometry_column, event "
                  "FROM spatialite_history ORDER BY event_id DESC LIMIT 1" );

        if( oResult && oResult->RowCount() == 1 )
        {
            const char* pszEventId = oResult->GetValue(0, 0);
            const char* pszTableName = oResult->GetValue(1, 0);
            const char* pszGeomCol = oResult->GetValue(2, 0);
            const char* pszEvent = oResult->GetValue(3, 0);

            if( pszEventId != nullptr && pszTableName != nullptr &&
                pszGeomCol != nullptr && pszEvent != nullptr &&
                strcmp(pszTableName, "ALL-TABLES") == 0 &&
                strcmp(pszGeomCol, "ALL-GEOMETRY-COLUMNS") == 0 &&
                strcmp(pszEvent, "UpdateLayerStatistics") == 0 )
            {
                nReplaceEventId = atoi(pszEventId);
            }
        }

        const char* pszNow = HasSpatialite4Layout() ?
            "strftime('%Y-%m-%dT%H:%M:%fZ','now')" : "DateTime('now')";
        const char* pszSQL;
        if( nReplaceEventId >= 0 )
        {
            pszSQL = CPLSPrintf("UPDATE spatialite_history SET "
                                          "timestamp = %s "
                                          "WHERE event_id = %d",
                                          pszNow,
                                          nReplaceEventId);
        }
        else
        {
            pszSQL =
                CPLSPrintf( "INSERT INTO spatialite_history (table_name, geometry_column, "
                "event, timestamp, ver_sqlite, ver_splite) VALUES ("
                "'ALL-TABLES', 'ALL-GEOMETRY-COLUMNS', 'UpdateLayerStatistics', "
                "%s, sqlite_version(), spatialite_version())", pszNow);
        }

        SQLCommand( hDB, pszSQL) ;
    }
}

/************************************************************************/
/*                              SetSynchronous()                        */
/************************************************************************/

bool OGRSQLiteBaseDataSource::SetSynchronous()
{
    const char* pszSqliteSync = CPLGetConfigOption("OGR_SQLITE_SYNCHRONOUS", nullptr);
    if (pszSqliteSync != nullptr)
    {
        const char* pszSQL = nullptr;
        if (EQUAL(pszSqliteSync, "OFF") || EQUAL(pszSqliteSync, "0") ||
            EQUAL(pszSqliteSync, "FALSE"))
            pszSQL = "PRAGMA synchronous = OFF";
        else if (EQUAL(pszSqliteSync, "NORMAL") || EQUAL(pszSqliteSync, "1"))
            pszSQL =  "PRAGMA synchronous = NORMAL";
        else if (EQUAL(pszSqliteSync, "ON") || EQUAL(pszSqliteSync, "FULL") ||
            EQUAL(pszSqliteSync, "2") || EQUAL(pszSqliteSync, "TRUE"))
            pszSQL = "PRAGMA synchronous = FULL";
        else
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Unrecognized value for OGR_SQLITE_SYNCHRONOUS : %s",
                      pszSqliteSync);

        return pszSQL != nullptr && SQLCommand(hDB, pszSQL) == OGRERR_NONE;
    }
    return true;
}

/************************************************************************/
/*                              LoadExtensions()                        */
/************************************************************************/

void OGRSQLiteBaseDataSource::LoadExtensions()
{
    const char* pszExtensions = CPLGetConfigOption("OGR_SQLITE_LOAD_EXTENSIONS", nullptr);
    if (pszExtensions != nullptr)
    {
#ifdef OGR_SQLITE_ALLOW_LOAD_EXTENSIONS
        // Allow sqlite3_load_extension() (C API only)
#ifdef SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION
        int oldMode = 0;
        if( sqlite3_db_config(hDB, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, -1, &oldMode) != SQLITE_OK )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot get initial value for SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION");
            return;
        }
        CPLDebugOnly("SQLite",
                     "Initial mode for SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION = %d",
                     oldMode);
        int newMode = 0;
        if( oldMode != 1 &&
            (sqlite3_db_config(hDB, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, 1, &newMode) != SQLITE_OK ||
             newMode != 1) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION failed");
            return;
        }
#endif
        const CPLStringList aosExtensions(CSLTokenizeString2( pszExtensions, ",", 0 ));
        bool bRestoreOldMode = true;
        for( int i = 0; i < aosExtensions.size(); i++ )
        {
            if( EQUAL(aosExtensions[i], "ENABLE_SQL_LOAD_EXTENSION") )
            {
                if( sqlite3_enable_load_extension(hDB, 1) == SQLITE_OK )
                {
                    bRestoreOldMode = false;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "sqlite3_enable_load_extension() failed");
                }
            }
            else
            {
                char* pszErrMsg = nullptr;
                if( sqlite3_load_extension(hDB, aosExtensions[i], nullptr, &pszErrMsg) != SQLITE_OK )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot load extension %s: %s",
                             aosExtensions[i],
                             pszErrMsg ? pszErrMsg : "unknown reason");
                }
                sqlite3_free(pszErrMsg);
            }
        }
        CPL_IGNORE_RET_VAL(bRestoreOldMode);
#ifdef SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION
        if( bRestoreOldMode && oldMode != 1 )
        {
            CPL_IGNORE_RET_VAL(sqlite3_db_config(
                hDB, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, oldMode, nullptr));
        }
#endif
#else
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The OGR_SQLITE_LOAD_EXTENSIONS was specified at run time, "
                 "but GDAL has been built without OGR_SQLITE_ALLOW_LOAD_EXTENSIONS. "
                 "So extensions won't be loaded");
#endif
    }
}

/************************************************************************/
/*                              SetCacheSize()                          */
/************************************************************************/

bool OGRSQLiteBaseDataSource::SetCacheSize()
{
    const char* pszSqliteCacheMB = CPLGetConfigOption("OGR_SQLITE_CACHE", nullptr);
    if (pszSqliteCacheMB != nullptr)
    {
        const GIntBig iSqliteCacheBytes =
            static_cast<GIntBig>(atoi( pszSqliteCacheMB )) * 1024 * 1024;

        /* querying the current PageSize */
        int iSqlitePageSize = SQLGetInteger(hDB, "PRAGMA page_size", nullptr);
        if( iSqlitePageSize <= 0 )
            return false;
        /* computing the CacheSize as #Pages */
        const int iSqliteCachePages =
                static_cast<int>(iSqliteCacheBytes / iSqlitePageSize);
        if( iSqliteCachePages <= 0)
            return false;

        return SQLCommand( hDB, CPLSPrintf( "PRAGMA cache_size = %d",
                                        iSqliteCachePages ) ) == OGRERR_NONE;
    }
    return true;
}

/************************************************************************/
/*               OGRSQLiteBaseDataSourceNotifyFileOpened()              */
/************************************************************************/

static
void OGRSQLiteBaseDataSourceNotifyFileOpened (void* pfnUserData,
                                              const char* pszFilename,
                                              VSILFILE* fp)
{
    ((OGRSQLiteBaseDataSource*)pfnUserData)->NotifyFileOpened(pszFilename, fp);
}

/************************************************************************/
/*                          NotifyFileOpened()                          */
/************************************************************************/

void OGRSQLiteBaseDataSource::NotifyFileOpened(const char* pszFilename,
                                           VSILFILE* fp)
{
    if (strcmp(pszFilename, m_pszFilename) == 0)
    {
        fpMainFile = fp;
    }
}

#ifdef USE_SQLITE_DEBUG_MEMALLOC

/* DMA9 */
constexpr int DMA_SIGNATURE = 0x444D4139;

static void* OGRSQLiteDMA_Malloc(int size)
{
    int* ret = (int*) CPLMalloc(size + 8);
    ret[0] = size;
    ret[1] = DMA_SIGNATURE;
    return ret + 2;
}

static void* OGRSQLiteDMA_Realloc(void* old_ptr, int size)
{
    CPLAssert(((int*)old_ptr)[-1] == DMA_SIGNATURE);
    int* ret = (int*) CPLRealloc(old_ptr ? (int*)old_ptr - 2 : NULL, size + 8);
    ret[0] = size;
    ret[1] = DMA_SIGNATURE;
    return ret + 2;
}

static void OGRSQLiteDMA_Free(void* ptr)
{
    if( ptr )
    {
        CPLAssert(((int*)ptr)[-1] == DMA_SIGNATURE);
        ((int*)ptr)[-1] = 0;
        CPLFree((int*)ptr - 2);
    }
}

static int OGRSQLiteDMA_Size (void* ptr)
{
    if( ptr )
    {
        CPLAssert(((int*)ptr)[-1] == DMA_SIGNATURE);
        return ((int*)ptr)[-2];
    }
    else
        return 0;
}

static int OGRSQLiteDMA_Roundup( int size )
{
    return (size + 7) & (~7);
}

static int OGRSQLiteDMA_Init(void *)
{
    return SQLITE_OK;
}

static void OGRSQLiteDMA_Shutdown(void *)
{
}

const struct sqlite3_mem_methods sDebugMemAlloc =
{
    OGRSQLiteDMA_Malloc,
    OGRSQLiteDMA_Free,
    OGRSQLiteDMA_Realloc,
    OGRSQLiteDMA_Size,
    OGRSQLiteDMA_Roundup,
    OGRSQLiteDMA_Init,
    OGRSQLiteDMA_Shutdown,
    NULL
};

#endif // USE_SQLITE_DEBUG_MEMALLOC

/************************************************************************/
/*                            OpenOrCreateDB()                          */
/************************************************************************/

int OGRSQLiteBaseDataSource::OpenOrCreateDB(int flagsIn, bool bRegisterOGR2SQLiteExtensions)
{
#ifdef USE_SQLITE_DEBUG_MEMALLOC
    if( CPLTestBool(CPLGetConfigOption("USE_SQLITE_DEBUG_MEMALLOC", "NO")) )
        sqlite3_config(SQLITE_CONFIG_MALLOC, &sDebugMemAlloc);
#endif

    if( bRegisterOGR2SQLiteExtensions )
        OGR2SQLITE_Register();

    const bool bUseOGRVFS =
        CPLTestBool(CPLGetConfigOption("SQLITE_USE_OGR_VFS", "NO"));

#ifdef SQLITE_OPEN_URI
    if ( m_osFilenameForSQLiteOpen.empty() &&
          (flagsIn & SQLITE_OPEN_READWRITE) == 0 &&
          !(bUseOGRVFS || STARTS_WITH(m_pszFilename, "/vsi")) &&
          !STARTS_WITH(m_pszFilename, "file:") &&
          CPLTestBool(CSLFetchNameValueDef(papszOpenOptions, "NOLOCK", "NO")) )
    {
        m_osFilenameForSQLiteOpen = "file:";

        // Apply rules from "3.1. The URI Path" of https://www.sqlite.org/uri.html
        CPLString osFilenameForURI(m_pszFilename);
        osFilenameForURI.replaceAll('?', "%3f");
        osFilenameForURI.replaceAll('#', "%23");
#ifdef _WIN32
        osFilenameForURI.replaceAll('\\', '/');
#endif
        osFilenameForURI.replaceAll("//", '/');
#ifdef _WIN32
        if( osFilenameForURI.size() > 3 && osFilenameForURI[1] == ':' &&
            osFilenameForURI[2] == '/' )
        {
            osFilenameForURI = '/' + osFilenameForURI;
        }
#endif

        m_osFilenameForSQLiteOpen += osFilenameForURI;
        m_osFilenameForSQLiteOpen += "?nolock=1";
    }
#endif
    if( m_osFilenameForSQLiteOpen.empty() )
    {
        m_osFilenameForSQLiteOpen = m_pszFilename;
    }

    // No mutex since OGR objects are not supposed to be used concurrently
    // from multiple threads.
    int flags = flagsIn | SQLITE_OPEN_NOMUTEX;
#ifdef SQLITE_OPEN_URI
    // This code enables support for named memory databases in SQLite.
    // SQLITE_USE_URI is checked only to enable backward compatibility, in
    // case we accidentally hijacked some other format.
    if( STARTS_WITH(m_osFilenameForSQLiteOpen.c_str(), "file:") &&
        CPLTestBool(CPLGetConfigOption("SQLITE_USE_URI", "YES")) )
    {
        flags |= SQLITE_OPEN_URI;
    }
#endif

    bool bPageSizeFound = false;

    const char* pszSqlitePragma =
                            CPLGetConfigOption("OGR_SQLITE_PRAGMA", nullptr);
    CPLString osJournalMode =
                        CPLGetConfigOption("OGR_SQLITE_JOURNAL", "");

    for( int iterOpen = 0; iterOpen < 2 ; iterOpen++ )
    {
        int rc;
        if (bUseOGRVFS || STARTS_WITH(m_pszFilename, "/vsi"))
        {
            pMyVFS = OGRSQLiteCreateVFS(OGRSQLiteBaseDataSourceNotifyFileOpened, this);
            sqlite3_vfs_register(pMyVFS, 0);
            rc = sqlite3_open_v2( m_osFilenameForSQLiteOpen.c_str(), &hDB, flags, pMyVFS->zName );
        }
        else
        {
            rc = sqlite3_open_v2( m_osFilenameForSQLiteOpen.c_str(), &hDB, flags, nullptr );
        }

        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "sqlite3_open(%s) failed: %s",
                      m_pszFilename, sqlite3_errmsg( hDB ) );
            return FALSE;
        }

#ifdef SQLITE_DBCONFIG_DEFENSIVE
        // SQLite builds on recent MacOS enable defensive mode by default, which
        // causes issues in the VDV driver (when updating a deleted database),
        // or in the GPKG driver (when modifying a CREATE TABLE DDL with writable_schema=ON)
        // So disable it.
        int bDefensiveOldValue = 0;
        if( sqlite3_db_config(hDB, SQLITE_DBCONFIG_DEFENSIVE, -1, &bDefensiveOldValue) == SQLITE_OK &&
            bDefensiveOldValue == 1 )
        {
            if( sqlite3_db_config(hDB, SQLITE_DBCONFIG_DEFENSIVE, 0, nullptr) == SQLITE_OK )
            {
                CPLDebug("SQLITE", "Disabling defensive mode succeeded");
            }
            else
            {
                CPLDebug("SQLITE", "Could not disable defensive mode");
            }
        }
#endif

#ifdef SQLITE_FCNTL_PERSIST_WAL
        int nPersistentWAL = -1;
        sqlite3_file_control(hDB, "main", SQLITE_FCNTL_PERSIST_WAL, &nPersistentWAL);
        if( nPersistentWAL == 1 )
        {
            nPersistentWAL = 0;
            if( sqlite3_file_control(hDB, "main", SQLITE_FCNTL_PERSIST_WAL, &nPersistentWAL) == SQLITE_OK )
            {
                CPLDebug("SQLITE", "Disabling persistent WAL succeeded");
            }
            else
            {
                CPLDebug("SQLITE", "Could not disable persistent WAL");
            }
        }
#endif

        if (pszSqlitePragma != nullptr)
        {
            char** papszTokens = CSLTokenizeString2( pszSqlitePragma, ",",
                                                     CSLT_HONOURSTRINGS );
            for(int i=0; papszTokens[i] != nullptr; i++ )
            {
                if( STARTS_WITH_CI(papszTokens[i], "PAGE_SIZE") )
                    bPageSizeFound = true;
                if( STARTS_WITH_CI(papszTokens[i], "JOURNAL_MODE") )
                {
                    const char* pszEqual = strchr(papszTokens[i], '=');
                    if( pszEqual )
                    {
                        osJournalMode = pszEqual + 1;
                        osJournalMode.Trim();
                        // Only apply journal_mode after changing page_size
                        continue;
                    }
                }

                const char* pszSQL = CPLSPrintf("PRAGMA %s", papszTokens[i]);

                CPL_IGNORE_RET_VAL(
                    sqlite3_exec( hDB, pszSQL, nullptr, nullptr, nullptr ) );
            }
            CSLDestroy(papszTokens);
        }

        const char* pszVal = CPLGetConfigOption("SQLITE_BUSY_TIMEOUT", "5000");
        if ( pszVal != nullptr ) {
            sqlite3_busy_timeout(hDB, atoi(pszVal));
        }

#ifdef SQLITE_OPEN_URI
        if( iterOpen == 0 && m_osFilenameForSQLiteOpen != m_pszFilename &&
            m_osFilenameForSQLiteOpen.find("?nolock=1") != std::string::npos )
        {
            int nRowCount = 0, nColCount = 0;
            char** papszResult = nullptr;
            rc = sqlite3_get_table( hDB,
                            "PRAGMA journal_mode",
                            &papszResult, &nRowCount, &nColCount,
                            nullptr );
            bool bWal = false;
            // rc == SQLITE_CANTOPEN seems to be what we get when issuing the
            // above in nolock mode on a wal enabled file
            if( rc != SQLITE_OK || (nRowCount == 1 && nColCount == 1 &&
                papszResult[1] && EQUAL(papszResult[1], "wal")) )
            {
                bWal = true;
            }
            sqlite3_free_table(papszResult);
            if( bWal )
            {
                flags &= ~SQLITE_OPEN_URI;
                sqlite3_close(hDB);
                hDB = nullptr;
                CPLDebug("SQLite", "Cannot open %s in nolock mode because it is presumably in -wal mode", m_pszFilename);
                m_osFilenameForSQLiteOpen = m_pszFilename;
                continue;
            }
        }
#endif
        break;
    }

    if( (flagsIn & SQLITE_OPEN_CREATE) == 0 )
    {
        if( CPLTestBool(CPLGetConfigOption("OGR_VFK_DB_READ", "NO")) )
        {
            if( SQLGetInteger( hDB,
                               "SELECT 1 FROM sqlite_master "
                               "WHERE type = 'table' AND name = 'vfk_tables'",
                               nullptr ) )
                return FALSE;  /* DB is valid VFK datasource */
        }

        int nRowCount = 0, nColCount = 0;
        char** papszResult = nullptr;
        char* pszErrMsg = nullptr;
        int rc = sqlite3_get_table( hDB,
                        "SELECT 1 FROM sqlite_master "
                        "WHERE (type = 'trigger' OR type = 'view') AND ("
                        "sql LIKE '%%ogr_geocode%%' OR "
                        "sql LIKE '%%ogr_datasource_load_layers%%' OR "
                        "sql LIKE '%%ogr_GetConfigOption%%' OR "
                        "sql LIKE '%%ogr_SetConfigOption%%' ) "
                        "LIMIT 1",
                        &papszResult, &nRowCount, &nColCount,
                        &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            bool bIsWAL = false;
            VSILFILE* fp = VSIFOpenL(m_pszFilename, "rb");
            if( fp != nullptr )
            {
                GByte byVal = 0;
                VSIFSeekL(fp, 18, SEEK_SET);
                VSIFReadL(&byVal, 1, 1, fp);
                bIsWAL = byVal == 2;
                VSIFCloseL(fp);
            }
            if( bIsWAL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "%s: this file is a WAL-enabled database. "
                    "It cannot be opened "
                    "because it is presumably read-only or in a "
                    "read-only directory.",
                    pszErrMsg);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s", pszErrMsg);
            }
            sqlite3_free( pszErrMsg );
            return FALSE;
        }

        sqlite3_free_table(papszResult);

        if( nRowCount > 0 )
        {
            if( !CPLTestBool(CPLGetConfigOption(
                "ALLOW_OGR_SQL_FUNCTIONS_FROM_TRIGGER_AND_VIEW", "NO")) )
            {
                CPLError( CE_Failure, CPLE_OpenFailed, "%s",
                    "A trigger and/or view calls a OGR extension SQL "
                    "function that could be used to "
                    "steal data, or use network bandwidth, without your consent.\n"
                    "The database will not be opened unless the "
                    "ALLOW_OGR_SQL_FUNCTIONS_FROM_TRIGGER_AND_VIEW "
                    "configuration option to YES.");
                return FALSE;
            }
        }
    }

    if( m_osFilenameForSQLiteOpen != m_pszFilename &&
            m_osFilenameForSQLiteOpen.find("?nolock=1") != std::string::npos )
    {
        m_bNoLock = true;
        CPLDebug("SQLite", "%s open in nolock mode", m_pszFilename);
    }

    if( !bPageSizeFound && (flagsIn & SQLITE_OPEN_CREATE) != 0 )
    {
        // Since sqlite 3.12 the default page_size is now 4096. But we
        // can use that even with older versions.
        CPL_IGNORE_RET_VAL(
            sqlite3_exec( hDB, "PRAGMA page_size = 4096", nullptr, nullptr, nullptr ) );
    }

    // journal_mode = WAL must be done *AFTER* changing page size.
    if (!osJournalMode.empty())
    {
        const char* pszSQL = CPLSPrintf("PRAGMA journal_mode = %s",
                                        osJournalMode.c_str());

        CPL_IGNORE_RET_VAL(
            sqlite3_exec( hDB, pszSQL, nullptr, nullptr, nullptr ) );
    }

    SetCacheSize();
    SetSynchronous();
    LoadExtensions();

    return TRUE;
}

/************************************************************************/
/*                          GetInternalHandle()                         */
/************************************************************************/

/* Used by MBTILES driver */
void *OGRSQLiteBaseDataSource::GetInternalHandle( const char * pszKey )
{
    if( pszKey != nullptr && EQUAL(pszKey, "SQLITE_HANDLE") )
        return hDB;
    return nullptr;
}


/************************************************************************/
/*                               Create()                               */
/************************************************************************/

bool OGRSQLiteDataSource::Create( const char * pszNameIn, char **papszOptions )
{
    CPLString osCommand;

    const bool bUseTempFile =
        CPLTestBool(CPLGetConfigOption("CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", "NO")) &&
        (VSIHasOptimizedReadMultiRange(pszNameIn) != FALSE ||
         EQUAL(CPLGetConfigOption("CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", ""), "FORCED"));

    if( bUseTempFile )
    {
        m_osFinalFilename = pszNameIn;
        m_pszFilename = CPLStrdup(CPLGenerateTempFilename(CPLGetFilename(pszNameIn)));
        CPLDebug("SQLITE", "Creating temporary file %s", m_pszFilename);
    }
    else
    {
        m_pszFilename = CPLStrdup( pszNameIn );
    }

/* -------------------------------------------------------------------- */
/*      Check that spatialite extensions are loaded if required to      */
/*      create a spatialite database                                    */
/* -------------------------------------------------------------------- */
    const bool bSpatialite = CPLFetchBool( papszOptions, "SPATIALITE", false );
    const bool bMetadata = CPLFetchBool( papszOptions, "METADATA", true );

    if( bSpatialite )
    {
#ifdef HAVE_SPATIALITE
#ifndef SPATIALITE_412_OR_LATER
        OGRSQLiteInitOldSpatialite();
        if (!IsSpatialiteLoaded())
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                    "Creating a Spatialite database, but Spatialite extensions are not loaded." );
            return false;
        }
#endif
#else
        CPLError( CE_Failure, CPLE_NotSupported,
            "OGR was built without libspatialite support\n"
            "... sorry, creating/writing any SpatiaLite DB is unsupported\n" );

        return false;
#endif
    }

    m_bIsSpatiaLiteDB = bSpatialite;

/* -------------------------------------------------------------------- */
/*      Create the database file.                                       */
/* -------------------------------------------------------------------- */
    if (!OpenOrCreateDB(SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, true))
        return false;

/* -------------------------------------------------------------------- */
/*      Create the SpatiaLite metadata tables.                          */
/* -------------------------------------------------------------------- */
    if ( bSpatialite )
    {
        if (!InitNewSpatialite())
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                    "Creating a Spatialite database, but Spatialite extensions are not loaded." );
            return false;
        }
#ifdef HAVE_RASTERLITE2
        InitRasterLite2();
#endif

        /*
        / SpatiaLite full support: calling InitSpatialMetadata()
        /
        / IMPORTANT NOTICE: on SpatiaLite any attempt aimed
        / to directly CREATE "geometry_columns" and "spatial_ref_sys"
        / [by-passing InitSpatialMetadata() as absolutely required]
        / will severely [and irremediably] corrupt the DB !!!
        */

        const char* pszVal = CSLFetchNameValue( papszOptions, "INIT_WITH_EPSG" );
        const int nSpatialiteVersionNumber = GetSpatialiteVersionNumber();
        if( pszVal != nullptr && !CPLTestBool(pszVal) &&
            nSpatialiteVersionNumber >= 40 )
        {
            if( nSpatialiteVersionNumber >= 41 )
                osCommand =  "SELECT InitSpatialMetadata(1, 'NONE')";
            else
                osCommand =  "SELECT InitSpatialMetadata('NONE')";
        }
        else
        {
            /* Since spatialite 4.1, InitSpatialMetadata() is no longer run */
            /* into a transaction, which makes population of spatial_ref_sys */
            /* from EPSG awfully slow. We have to use InitSpatialMetadata(1) */
            /* to run within a transaction */
            if( nSpatialiteVersionNumber >= 41 )
                osCommand =  "SELECT InitSpatialMetadata(1)";
            else
                osCommand =  "SELECT InitSpatialMetadata()";
        }
        if( SQLCommand( hDB, osCommand ) != OGRERR_NONE )
        {
            return false;
        }
    }

/* -------------------------------------------------------------------- */
/*  Create the geometry_columns and spatial_ref_sys metadata tables.    */
/* -------------------------------------------------------------------- */
    else if( bMetadata )
    {
        if( SQLCommand( hDB,
            "CREATE TABLE geometry_columns ("
            "     f_table_name VARCHAR, "
            "     f_geometry_column VARCHAR, "
            "     geometry_type INTEGER, "
            "     coord_dimension INTEGER, "
            "     srid INTEGER,"
            "     geometry_format VARCHAR )"
            ";"
            "CREATE TABLE spatial_ref_sys        ("
            "     srid INTEGER UNIQUE,"
            "     auth_name TEXT,"
            "     auth_srid TEXT,"
            "     srtext TEXT)") != OGRERR_NONE )
        {
            return false;
        }
    }

/* -------------------------------------------------------------------- */
/*      Optionally initialize the content of the spatial_ref_sys table  */
/*      with the EPSG database                                          */
/* -------------------------------------------------------------------- */
    if ( (bSpatialite || bMetadata) &&
         CPLFetchBool( papszOptions, "INIT_WITH_EPSG", false ) )
    {
        if (!InitWithEPSG())
            return false;
    }

    GDALOpenInfo oOpenInfo(m_pszFilename, GDAL_OF_VECTOR | GDAL_OF_UPDATE);
    return Open(&oOpenInfo);
}

/************************************************************************/
/*                           InitWithEPSG()                             */
/************************************************************************/

bool OGRSQLiteDataSource::InitWithEPSG()
{
    CPLString osCommand;

    if ( m_bIsSpatiaLiteDB )
    {
        /*
        / if v.2.4.0 (or any subsequent) InitWithEPSG make no sense at all
        / because the EPSG dataset is already self-initialized at DB creation
        */
        int iSpatialiteVersion = GetSpatialiteVersionNumber();
        if ( iSpatialiteVersion >= 24 )
            return true;
    }

    if( SoftStartTransaction() != OGRERR_NONE )
        return false;

    OGRSpatialReference oSRS;
    int rc = SQLITE_OK;
    for( int i = 0; i < 2 && rc == SQLITE_OK; i++ )
    {
        PROJ_STRING_LIST crsCodeList =
            proj_get_codes_from_database(
                OSRGetProjTLSContext(), "EPSG",
                i == 0 ? PJ_TYPE_GEOGRAPHIC_2D_CRS : PJ_TYPE_PROJECTED_CRS,
                true);
        for( auto iterCode = crsCodeList; iterCode && *iterCode; ++iterCode )
        {
            int nSRSId = atoi(*iterCode);

            CPLPushErrorHandler(CPLQuietErrorHandler);
            oSRS.importFromEPSG(nSRSId);
            CPLPopErrorHandler();

            if (m_bIsSpatiaLiteDB)
            {
                char    *pszProj4 = nullptr;

                CPLPushErrorHandler(CPLQuietErrorHandler);
                OGRErr eErr = oSRS.exportToProj4( &pszProj4 );

                char    *pszWKT = nullptr;
                if( eErr == OGRERR_NONE &&
                    oSRS.exportToWkt( &pszWKT ) != OGRERR_NONE )
                {
                    CPLFree(pszWKT);
                    pszWKT = nullptr;
                    eErr = OGRERR_FAILURE;
                }
                CPLPopErrorHandler();

                if( eErr == OGRERR_NONE )
                {
                    const char  *pszProjCS = oSRS.GetAttrValue("PROJCS");
                    if (pszProjCS == nullptr)
                        pszProjCS = oSRS.GetAttrValue("GEOGCS");

                    const char* pszSRTEXTColName = GetSRTEXTColName();
                    if (pszSRTEXTColName != nullptr)
                    {
                    /* the SPATIAL_REF_SYS table supports a SRS_WKT column */
                        if ( pszProjCS )
                            osCommand.Printf(
                                "INSERT INTO spatial_ref_sys "
                                "(srid, auth_name, auth_srid, ref_sys_name, proj4text, %s) "
                                "VALUES (%d, 'EPSG', '%d', ?, ?, ?)",
                                pszSRTEXTColName, nSRSId, nSRSId);
                        else
                            osCommand.Printf(
                                "INSERT INTO spatial_ref_sys "
                                "(srid, auth_name, auth_srid, proj4text, %s) "
                                "VALUES (%d, 'EPSG', '%d', ?, ?)",
                                pszSRTEXTColName, nSRSId, nSRSId);
                    }
                    else
                    {
                    /* the SPATIAL_REF_SYS table does not support a SRS_WKT column */
                        if ( pszProjCS )
                            osCommand.Printf(
                                "INSERT INTO spatial_ref_sys "
                                "(srid, auth_name, auth_srid, ref_sys_name, proj4text) "
                                "VALUES (%d, 'EPSG', '%d', ?, ?)",
                                nSRSId, nSRSId);
                        else
                            osCommand.Printf(
                                "INSERT INTO spatial_ref_sys "
                                "(srid, auth_name, auth_srid, proj4text) "
                                "VALUES (%d, 'EPSG', '%d', ?)",
                                nSRSId, nSRSId);
                    }

                    sqlite3_stmt *hInsertStmt = nullptr;
                    rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hInsertStmt, nullptr );

                    if ( pszProjCS )
                    {
                        if( rc == SQLITE_OK)
                            rc = sqlite3_bind_text( hInsertStmt, 1, pszProjCS, -1, SQLITE_STATIC );
                        if( rc == SQLITE_OK)
                            rc = sqlite3_bind_text( hInsertStmt, 2, pszProj4, -1, SQLITE_STATIC );
                        if (pszSRTEXTColName != nullptr)
                        {
                        /* the SPATIAL_REF_SYS table supports a SRS_WKT column */
                            if( rc == SQLITE_OK && pszWKT != nullptr)
                                rc = sqlite3_bind_text( hInsertStmt, 3, pszWKT, -1, SQLITE_STATIC );
                        }
                    }
                    else
                    {
                        if( rc == SQLITE_OK)
                            rc = sqlite3_bind_text( hInsertStmt, 1, pszProj4, -1, SQLITE_STATIC );
                        if (pszSRTEXTColName != nullptr)
                        {
                        /* the SPATIAL_REF_SYS table supports a SRS_WKT column */
                            if( rc == SQLITE_OK && pszWKT != nullptr)
                                rc = sqlite3_bind_text( hInsertStmt, 2, pszWKT, -1, SQLITE_STATIC );
                        }
                    }

                    if( rc == SQLITE_OK)
                        rc = sqlite3_step( hInsertStmt );

                    if( rc != SQLITE_OK && rc != SQLITE_DONE )
                    {
                        CPLError( CE_Failure, CPLE_AppDefined,
                                    "Cannot insert %s into spatial_ref_sys : %s",
                                    pszProj4,
                                    sqlite3_errmsg(hDB) );

                        sqlite3_finalize( hInsertStmt );
                        CPLFree(pszProj4);
                        CPLFree(pszWKT);
                        break;
                    }
                    rc = SQLITE_OK;

                    sqlite3_finalize( hInsertStmt );
                }

                CPLFree(pszProj4);
                CPLFree(pszWKT);
            }
            else
            {
                char    *pszWKT = nullptr;
                CPLPushErrorHandler(CPLQuietErrorHandler);
                bool bSuccess = ( oSRS.exportToWkt( &pszWKT ) == OGRERR_NONE );
                CPLPopErrorHandler();
                if( bSuccess )
                {
                    osCommand.Printf(
                        "INSERT INTO spatial_ref_sys "
                        "(srid, auth_name, auth_srid, srtext) "
                        "VALUES (%d, 'EPSG', '%d', ?)",
                        nSRSId, nSRSId );

                    sqlite3_stmt *hInsertStmt = nullptr;
                    rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hInsertStmt, nullptr );

                    if( rc == SQLITE_OK)
                        rc = sqlite3_bind_text( hInsertStmt, 1, pszWKT, -1, SQLITE_STATIC );

                    if( rc == SQLITE_OK)
                        rc = sqlite3_step( hInsertStmt );

                    if( rc != SQLITE_OK && rc != SQLITE_DONE )
                    {
                        CPLError( CE_Failure, CPLE_AppDefined,
                                    "Cannot insert %s into spatial_ref_sys : %s",
                                    pszWKT,
                                    sqlite3_errmsg(hDB) );

                        sqlite3_finalize( hInsertStmt );
                        CPLFree(pszWKT);
                        break;
                    }
                    rc = SQLITE_OK;

                    sqlite3_finalize( hInsertStmt );
                }

                CPLFree(pszWKT);
            }
        }

        proj_string_list_destroy(crsCodeList);
    }

    if( rc == SQLITE_OK )
    {
        if( SoftCommitTransaction() != OGRERR_NONE )
            return false;
        return true;
    }
    else
    {
        SoftRollbackTransaction();
        return false;
    }
}

/************************************************************************/
/*                        ReloadLayers()                                */
/************************************************************************/

void OGRSQLiteDataSource::ReloadLayers()
{
    for(int i=0;i<m_nLayers;i++)
        delete m_papoLayers[i];
    CPLFree(m_papoLayers);
    m_papoLayers = nullptr;
    m_nLayers = 0;

    GDALOpenInfo oOpenInfo(m_pszFilename,
                           GDAL_OF_VECTOR | (GetUpdate() ? GDAL_OF_UPDATE: 0));
    Open(&oOpenInfo);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

bool OGRSQLiteDataSource::Open( GDALOpenInfo* poOpenInfo)

{
    const char * pszNewName = poOpenInfo->pszFilename;
    CPLAssert( m_nLayers == 0 );
    eAccess = poOpenInfo->eAccess;
    nOpenFlags = poOpenInfo->nOpenFlags;
    SetDescription(pszNewName);

    if (m_pszFilename == nullptr)
    {
#ifdef HAVE_RASTERLITE2
        if( STARTS_WITH_CI(pszNewName, "RASTERLITE2:") &&
            (nOpenFlags & GDAL_OF_RASTER) != 0 )
        {
            char** papszTokens =
                CSLTokenizeString2( pszNewName, ":", CSLT_HONOURSTRINGS );
            if( CSLCount(papszTokens) < 2 )
            {
                CSLDestroy(papszTokens);
                return false;
            }
            m_pszFilename = CPLStrdup( SQLUnescape( papszTokens[1] ) );
            CSLDestroy(papszTokens);
        }
        else
#endif
        if( STARTS_WITH_CI(pszNewName, "SQLITE:") )
        {
            m_pszFilename = CPLStrdup( pszNewName + strlen("SQLITE:") );
        }
        else
        {
            m_pszFilename = CPLStrdup( pszNewName );
            if( poOpenInfo->pabyHeader &&
                STARTS_WITH((const char*)poOpenInfo->pabyHeader, "SQLite format 3") )
            {
                m_bCallUndeclareFileNotToOpen = true;
                GDALOpenInfoDeclareFileNotToOpen(m_pszFilename,
                                            poOpenInfo->pabyHeader,
                                            poOpenInfo->nHeaderBytes);
            }
        }
    }
    SetPhysicalFilename(m_pszFilename);

    VSIStatBufL sStat;
    if( VSIStatL( m_pszFilename, &sStat ) == 0 )
    {
        m_nFileTimestamp = sStat.st_mtime;
    }

    if( poOpenInfo->papszOpenOptions )
    {
        CSLDestroy(papszOpenOptions);
        papszOpenOptions = CSLDuplicate(poOpenInfo->papszOpenOptions);
    }

    const bool bListVectorLayers = (nOpenFlags & GDAL_OF_VECTOR) != 0;

    const bool bListAllTables = bListVectorLayers &&
        CPLTestBool(CSLFetchNameValueDef(
            papszOpenOptions, "LIST_ALL_TABLES",
            CPLGetConfigOption("SQLITE_LIST_ALL_TABLES", "NO")));

    // Don't list by default: there might be some security implications
    // if a user is provided with a file and doesn't know that there are
    // virtual OGR tables in it.
    const bool bListVirtualOGRLayers = bListVectorLayers &&
        CPLTestBool(CSLFetchNameValueDef(
            papszOpenOptions, "LIST_VIRTUAL_OGR",
            CPLGetConfigOption("OGR_SQLITE_LIST_VIRTUAL_OGR", "NO")));

/* -------------------------------------------------------------------- */
/*      Try to open the sqlite database properly now.                   */
/* -------------------------------------------------------------------- */
    if (hDB == nullptr)
    {
#ifndef SPATIALITE_412_OR_LATER
        OGRSQLiteInitOldSpatialite();
#endif

#ifdef ENABLE_SQL_SQLITE_FORMAT
        // SQLite -wal locking appears to be extremely fragile. In particular
        // if we have a file descriptor opened on the file while sqlite3_open
        // is called, then it will mis-behave (a process opening in update mode
        // the file and closing it will remove the -wal file !)
        // So make sure that the GDALOpenInfo object goes out of scope before
        // going on.
        {
          GDALOpenInfo oOpenInfo(m_pszFilename, GA_ReadOnly);
          if( oOpenInfo.pabyHeader &&
            (STARTS_WITH((const char*)oOpenInfo.pabyHeader, "-- SQL SQLITE") ||
             STARTS_WITH((const char*)oOpenInfo.pabyHeader, "-- SQL RASTERLITE") ||
             STARTS_WITH((const char*)oOpenInfo.pabyHeader, "-- SQL MBTILES")) &&
            oOpenInfo.fpL != nullptr )
          {
            if( sqlite3_open_v2( ":memory:", &hDB, SQLITE_OPEN_READWRITE, nullptr )
                    != SQLITE_OK )
            {
                return false;
            }

            // We need it here for ST_MinX() and the like
            InitNewSpatialite();

            // Ingest the lines of the dump
            VSIFSeekL( oOpenInfo.fpL, 0, SEEK_SET );
            const char* pszLine;
            while( (pszLine = CPLReadLineL( oOpenInfo.fpL )) != nullptr )
            {
                if( STARTS_WITH(pszLine, "--") )
                    continue;

                // Reject a few words tat might have security implications
                // Basically we just want to allow CREATE TABLE and INSERT INTO
                if( CPLString(pszLine).ifind("ATTACH") != std::string::npos ||
                    CPLString(pszLine).ifind("DETACH") != std::string::npos ||
                    CPLString(pszLine).ifind("PRAGMA") != std::string::npos ||
                    CPLString(pszLine).ifind("SELECT") != std::string::npos ||
                    CPLString(pszLine).ifind("UPDATE") != std::string::npos ||
                    CPLString(pszLine).ifind("REPLACE") != std::string::npos ||
                    CPLString(pszLine).ifind("DELETE") != std::string::npos ||
                    CPLString(pszLine).ifind("DROP") != std::string::npos ||
                    CPLString(pszLine).ifind("ALTER") != std::string::npos||
                    CPLString(pszLine).ifind("VIRTUAL") != std::string::npos )
                {
                    bool bOK = false;
                    if( EQUAL(pszLine,
                              "CREATE VIRTUAL TABLE SpatialIndex "
                              "USING VirtualSpatialIndex();") )
                    {
                        bOK = true;
                    }
                    // Accept creation of spatial index
                    else if( STARTS_WITH_CI(pszLine, "CREATE VIRTUAL TABLE ") )
                    {
                        const char* pszStr = pszLine +
                                            strlen("CREATE VIRTUAL TABLE ");
                        if( *pszStr == '"' )
                            pszStr ++;
                        while( (*pszStr >= 'a' && *pszStr <= 'z') ||
                               (*pszStr >= 'A' && *pszStr <= 'Z') ||
                               *pszStr == '_' )
                        {
                            pszStr ++;
                        }
                        if( *pszStr == '"' )
                            pszStr ++;
                        if( EQUAL(pszStr,
                            " USING rtree(pkid, xmin, xmax, ymin, ymax);") )
                        {
                            bOK = true;
                        }
                    }
                    // Accept INSERT INTO idx_byte_metadata_geometry SELECT rowid, ST_MinX(geometry), ST_MaxX(geometry), ST_MinY(geometry), ST_MaxY(geometry) FROM byte_metadata;
                    else if( STARTS_WITH_CI(pszLine, "INSERT INTO idx_") &&
                        CPLString(pszLine).ifind("SELECT") != std::string::npos )
                    {
                        char** papszTokens = CSLTokenizeString2( pszLine, " (),,", 0 );
                        if( CSLCount(papszTokens) == 15 &&
                            EQUAL(papszTokens[3], "SELECT") &&
                            EQUAL(papszTokens[5], "ST_MinX") &&
                            EQUAL(papszTokens[7], "ST_MaxX") &&
                            EQUAL(papszTokens[9], "ST_MinY") &&
                            EQUAL(papszTokens[11], "ST_MaxY") &&
                            EQUAL(papszTokens[13], "FROM") )
                        {
                            bOK = true;
                        }
                        CSLDestroy(papszTokens);
                    }

                    if( !bOK )
                    {
                        CPLError(CE_Failure, CPLE_NotSupported,
                                "Rejected statement: %s", pszLine);
                        return false;
                    }
                }
                char* pszErrMsg = nullptr;
                if( sqlite3_exec( hDB, pszLine, nullptr, nullptr, &pszErrMsg ) != SQLITE_OK )
                {
                    if( pszErrMsg )
                    {
                        CPLDebug("SQLITE", "Error %s at line %s",
                                 pszErrMsg, pszLine);
                    }
                }
                sqlite3_free(pszErrMsg);
            }
          }
        }
        if( hDB == nullptr )
#endif
        {
            if (poOpenInfo->fpL )
            {
                // See above comment about -wal locking for the importance of
                // closing that file, prior to calling sqlite3_open()
                VSIFCloseL(poOpenInfo->fpL);
                poOpenInfo->fpL = nullptr;
            }
            if (!OpenOrCreateDB(GetUpdate() ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY, true) )
            {
                poOpenInfo->fpL = VSIFOpenL(poOpenInfo->pszFilename,
                            poOpenInfo->eAccess == GA_Update ? "rb+" : "rb");
                return false;
            }
        }

        InitNewSpatialite();

#ifdef HAVE_RASTERLITE2
        InitRasterLite2();
#endif
    }

#ifdef HAVE_RASTERLITE2
    if( STARTS_WITH_CI(pszNewName, "RASTERLITE2:") &&
        (nOpenFlags & GDAL_OF_RASTER) != 0 )
    {
        return OpenRasterSubDataset( pszNewName );
    }
#endif

    const char* pszPreludeStatements = CSLFetchNameValue(papszOpenOptions, "PRELUDE_STATEMENTS");
    if( pszPreludeStatements )
    {
        if( SQLCommand(hDB, pszPreludeStatements) != OGRERR_NONE )
            return false;
    }

/* -------------------------------------------------------------------- */
/*      If we have a GEOMETRY_COLUMNS tables, initialize on the basis   */
/*      of that.                                                        */
/* -------------------------------------------------------------------- */
    CPLHashSet* hSet = CPLHashSetNew(CPLHashSetHashStr, CPLHashSetEqualStr, CPLFree);

    char **papszResult = nullptr;
    char *pszErrMsg = nullptr;
    int nRowCount = 0;
    int nColCount = 0;
    int rc = sqlite3_get_table(
        hDB,
        "SELECT f_table_name, f_geometry_column, geometry_type, "
        "coord_dimension, geometry_format, srid"
        " FROM geometry_columns "
        "LIMIT 10000",
        &papszResult, &nRowCount, &nColCount, &pszErrMsg );

    if( rc == SQLITE_OK )
    {
        CPLDebug("SQLITE", "OGR style SQLite DB found !");

        m_bHaveGeometryColumns = true;

        for ( int iRow = 0; bListVectorLayers && iRow < nRowCount; iRow++ )
        {
            char **papszRow = papszResult + iRow * 6 + 6;
            const char* pszTableName = papszRow[0];
            const char* pszGeomCol = papszRow[1];

            if( pszTableName == nullptr || pszGeomCol == nullptr )
                continue;

            m_aoMapTableToSetOfGeomCols[pszTableName].insert(CPLString(pszGeomCol).tolower());
        }

        for( int iRow = 0; bListVectorLayers && iRow < nRowCount; iRow++ )
        {
            char **papszRow = papszResult + iRow * 6 + 6;
            const char* pszTableName = papszRow[0];

            if (pszTableName == nullptr)
                continue;

            if( GDALDataset::GetLayerByName(pszTableName) == nullptr )
                OpenTable( pszTableName, true, false );

            if (bListAllTables)
                CPLHashSetInsert(hSet, CPLStrdup(pszTableName));
        }

        sqlite3_free_table(papszResult);

/* -------------------------------------------------------------------- */
/*      Detect VirtualOGR layers                                        */
/* -------------------------------------------------------------------- */
        if( bListVirtualOGRLayers )
        {
            rc = sqlite3_get_table( hDB,
                                "SELECT name, sql FROM sqlite_master "
                                "WHERE sql LIKE 'CREATE VIRTUAL TABLE %' "
                                "LIMIT 10000",
                                &papszResult, &nRowCount,
                                &nColCount, &pszErrMsg );

            if ( rc == SQLITE_OK )
            {
                for( int iRow = 0; iRow < nRowCount; iRow++ )
                {
                    char **papszRow = papszResult + iRow * 2 + 2;
                    const char *pszName = papszRow[0];
                    const char *pszSQL = papszRow[1];
                    if( pszName == nullptr || pszSQL == nullptr )
                        continue;

                    if( strstr(pszSQL, "VirtualOGR") )
                    {
                        OpenVirtualTable(pszName, pszSQL);

                        if (bListAllTables)
                            CPLHashSetInsert(hSet, CPLStrdup(pszName));
                    }
                }
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Unable to fetch list of tables: %s",
                        pszErrMsg );
                sqlite3_free( pszErrMsg );
            }

            sqlite3_free_table(papszResult);
        }

        if (bListAllTables)
            goto all_tables;

        CPLHashSetDestroy(hSet);

        if( nOpenFlags & GDAL_OF_RASTER )
        {
            bool bRet = OpenRaster();
            if( !bRet && !(nOpenFlags & GDAL_OF_VECTOR))
                return false;
        }

        return true;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we can deal with SpatiaLite database.                 */
/* -------------------------------------------------------------------- */
    sqlite3_free( pszErrMsg );
    rc = sqlite3_get_table( hDB,
                            "SELECT sm.name, gc.f_geometry_column, "
                            "gc.type, gc.coord_dimension, gc.srid, "
                            "gc.spatial_index_enabled FROM geometry_columns gc "
                            "JOIN sqlite_master sm ON "
                            "LOWER(gc.f_table_name)=LOWER(sm.name) "
                            "LIMIT 10000",
                            &papszResult, &nRowCount,
                            &nColCount, &pszErrMsg );
    if (rc != SQLITE_OK )
    {
        /* Test with SpatiaLite 4.0 schema */
        sqlite3_free( pszErrMsg );
        rc = sqlite3_get_table( hDB,
                                "SELECT sm.name, gc.f_geometry_column, "
                                "gc.geometry_type, gc.coord_dimension, gc.srid, "
                                "gc.spatial_index_enabled FROM geometry_columns gc "
                                "JOIN sqlite_master sm ON "
                                "LOWER(gc.f_table_name)=LOWER(sm.name) "
                                "LIMIT 10000",
                                &papszResult, &nRowCount,
                                &nColCount, &pszErrMsg );
        if ( rc == SQLITE_OK )
        {
            m_bSpatialite4Layout = true;
            m_nUndefinedSRID = 0;
        }
    }

    if ( rc == SQLITE_OK )
    {
        m_bIsSpatiaLiteDB = true;
        m_bHaveGeometryColumns = true;

        int iSpatialiteVersion = -1;

        /* Only enables write-mode if linked against SpatiaLite */
        if( IsSpatialiteLoaded() )
        {
            iSpatialiteVersion = GetSpatialiteVersionNumber();
        }
        else if( GetUpdate() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "SpatiaLite%s DB found, "
                     "but updating tables disabled because no linking against spatialite library !",
                     (m_bSpatialite4Layout) ? " v4" : "");
            sqlite3_free_table(papszResult);
            CPLHashSetDestroy(hSet);
            return false;
        }

        if (m_bSpatialite4Layout && GetUpdate() && iSpatialiteVersion > 0 && iSpatialiteVersion < 40)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "SpatiaLite v4 DB found, "
                     "but updating tables disabled because runtime spatialite library is v%.1f !",
                     iSpatialiteVersion / 10.0);
            sqlite3_free_table(papszResult);
            CPLHashSetDestroy(hSet);
            return false;
        }
        else
        {
            CPLDebug("SQLITE", "SpatiaLite%s DB found !",
                     (m_bSpatialite4Layout) ? " v4" : "");
        }

        // List RasterLite2 coverages, so as to avoid listing corresponding
        // technical tables
        std::set<CPLString> aoSetTablesToIgnore;
        if( m_bSpatialite4Layout )
        {
            char** papszResults2 = nullptr;
            int nRowCount2 = 0, nColCount2 = 0;
            rc = sqlite3_get_table( hDB,
                                "SELECT name FROM sqlite_master WHERE "
                                "type = 'table' AND name = 'raster_coverages'",
                                &papszResults2, &nRowCount2,
                                &nColCount2, nullptr );
            sqlite3_free_table(papszResults2);
            if( rc == SQLITE_OK && nRowCount2 == 1 )
            {
                papszResults2 = nullptr;
                nRowCount2 = 0;
                nColCount2 = 0;
                rc = sqlite3_get_table( hDB,
                                "SELECT coverage_name FROM raster_coverages "
                                "LIMIT 10000",
                                &papszResults2, &nRowCount2,
                                &nColCount2, nullptr );
                if( rc == SQLITE_OK )
                {
                    for(int i=0;i<nRowCount2;++i)
                    {
                        const char * const* papszRow = papszResults2 + i*1 + 1;
                        if( papszRow[0] != nullptr )
                        {
                            aoSetTablesToIgnore.insert(
                                    CPLString(papszRow[0]) + "_sections" );
                            aoSetTablesToIgnore.insert(
                                    CPLString(papszRow[0]) + "_tiles" );
                        }
                    }
                }
                sqlite3_free_table(papszResults2);
            }
        }

        for ( int iRow = 0; bListVectorLayers && iRow < nRowCount; iRow++ )
        {
            char **papszRow = papszResult + iRow * 6 + 6;
            const char* pszTableName = papszRow[0];
            const char* pszGeomCol = papszRow[1];

            if( pszTableName == nullptr || pszGeomCol == nullptr )
                continue;
            if( !bListAllTables &&
                aoSetTablesToIgnore.find(pszTableName) !=
                                                aoSetTablesToIgnore.end() )
            {
                continue;
            }

            m_aoMapTableToSetOfGeomCols[pszTableName].insert(CPLString(pszGeomCol).tolower());
        }

        for ( int iRow = 0; bListVectorLayers && iRow < nRowCount; iRow++ )
        {
            char **papszRow = papszResult + iRow * 6 + 6;
            const char* pszTableName = papszRow[0];

            if (pszTableName == nullptr )
                continue;
            if( !bListAllTables &&
                aoSetTablesToIgnore.find(pszTableName) !=
                                                aoSetTablesToIgnore.end() )
            {
                continue;
            }

            if( GDALDataset::GetLayerByName(pszTableName) == nullptr )
                OpenTable( pszTableName, true, false);
            if (bListAllTables)
                CPLHashSetInsert(hSet, CPLStrdup(pszTableName));
        }

        sqlite3_free_table(papszResult);
        papszResult = nullptr;

/* -------------------------------------------------------------------- */
/*      Detect VirtualShape, VirtualXL and VirtualOGR layers            */
/* -------------------------------------------------------------------- */
        rc = sqlite3_get_table( hDB,
                            "SELECT name, sql FROM sqlite_master "
                            "WHERE sql LIKE 'CREATE VIRTUAL TABLE %' "
                            "LIMIT 10000",
                            &papszResult, &nRowCount,
                            &nColCount, &pszErrMsg );

        if ( rc == SQLITE_OK )
        {
            for( int iRow = 0; bListVectorLayers && iRow < nRowCount; iRow++ )
            {
                char **papszRow = papszResult + iRow * 2 + 2;
                const char *pszName = papszRow[0];
                const char *pszSQL = papszRow[1];
                if( pszName == nullptr || pszSQL == nullptr )
                    continue;

                if( (IsSpatialiteLoaded() &&
                        (strstr(pszSQL, "VirtualShape") || strstr(pszSQL, "VirtualXL"))) ||
                    (bListVirtualOGRLayers && strstr(pszSQL, "VirtualOGR")) )
                {
                    OpenVirtualTable(pszName, pszSQL);

                    if (bListAllTables)
                        CPLHashSetInsert(hSet, CPLStrdup(pszName));
                }
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to fetch list of tables: %s",
                    pszErrMsg );
            sqlite3_free( pszErrMsg );
        }

        sqlite3_free_table(papszResult);
        papszResult = nullptr;

/* -------------------------------------------------------------------- */
/*      Detect spatial views                                            */
/* -------------------------------------------------------------------- */

        rc = sqlite3_get_table( hDB,
                                "SELECT view_name, view_geometry, view_rowid, "
                                "f_table_name, f_geometry_column "
                                "FROM views_geometry_columns "
                                "LIMIT 10000",
                                &papszResult, &nRowCount,
                                &nColCount, nullptr );
        if ( rc == SQLITE_OK )
        {
            for( int iRow = 0; bListVectorLayers && iRow < nRowCount; iRow++ )
            {
                char **papszRow = papszResult + iRow * 5 + 5;
                const char* pszViewName = papszRow[0];
                const char* pszViewGeometry = papszRow[1];
                const char* pszViewRowid = papszRow[2];
                const char* pszTableName = papszRow[3];
                const char* pszGeometryColumn = papszRow[4];

                if (pszViewName == nullptr ||
                    pszViewGeometry == nullptr ||
                    pszViewRowid == nullptr ||
                    pszTableName == nullptr ||
                    pszGeometryColumn == nullptr)
                    continue;

                OpenView( pszViewName, pszViewGeometry, pszViewRowid,
                          pszTableName, pszGeometryColumn );

                if (bListAllTables)
                    CPLHashSetInsert(hSet, CPLStrdup(pszViewName));
            }
            sqlite3_free_table(papszResult);
        }

        if (bListAllTables)
            goto all_tables;

        CPLHashSetDestroy(hSet);

        if( nOpenFlags & GDAL_OF_RASTER )
        {
            bool bRet = OpenRaster();
            if( !bRet && !(nOpenFlags & GDAL_OF_VECTOR))
                return false;
        }

        return true;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise our final resort is to return all tables and views    */
/*      as non-spatial tables.                                          */
/* -------------------------------------------------------------------- */
    sqlite3_free( pszErrMsg );

all_tables:
    rc = sqlite3_get_table( hDB,
                            "SELECT name, type FROM sqlite_master "
                            "WHERE type IN ('table','view') "
                            "UNION ALL "
                            "SELECT name, type FROM sqlite_temp_master "
                            "WHERE type IN ('table','view') "
                            "ORDER BY 1 "
                            "LIMIT 10000",
                            &papszResult, &nRowCount,
                            &nColCount, &pszErrMsg );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to fetch list of tables: %s",
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
        CPLHashSetDestroy(hSet);
        return false;
    }

    for( int iRow = 0; iRow < nRowCount; iRow++ )
    {
        const char* pszTableName = papszResult[2*(iRow+1)+0];
        const char* pszType = papszResult[2*(iRow+1)+1];
        if( pszTableName != nullptr && CPLHashSetLookup(hSet, pszTableName) == nullptr )
        {
            const bool bIsTable = pszType != nullptr && strcmp(pszType, "table") == 0;
            OpenTable( pszTableName, bIsTable, false );
        }
    }

    sqlite3_free_table(papszResult);
    CPLHashSetDestroy(hSet);

    if( nOpenFlags & GDAL_OF_RASTER )
    {
        bool bRet = OpenRaster();
        if( !bRet && !(nOpenFlags & GDAL_OF_VECTOR))
            return false;
    }

    return true;
}

/************************************************************************/
/*                          OpenVirtualTable()                          */
/************************************************************************/

bool OGRSQLiteDataSource::OpenVirtualTable(const char* pszName, const char* pszSQL)
{
    int nSRID = m_nUndefinedSRID;
    const char* pszVirtualShape = strstr(pszSQL, "VirtualShape");
    if (pszVirtualShape != nullptr)
    {
        const char* pszParenthesis = strchr(pszVirtualShape, '(');
        if (pszParenthesis)
        {
            /* CREATE VIRTUAL TABLE table_name VirtualShape(shapename, codepage, srid) */
            /* Extract 3rd parameter */
            char** papszTokens = CSLTokenizeString2( pszParenthesis + 1, ",", CSLT_HONOURSTRINGS );
            if (CSLCount(papszTokens) == 3)
            {
                nSRID = atoi(papszTokens[2]);
            }
            CSLDestroy(papszTokens);
        }
    }

    if (OpenTable(pszName, true, pszVirtualShape != nullptr))
    {
        OGRSQLiteLayer* poLayer = m_papoLayers[m_nLayers-1];
        if( poLayer->GetLayerDefn()->GetGeomFieldCount() == 1 )
        {
            OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                    poLayer->myGetLayerDefn()->myGetGeomFieldDefn(0);
            poGeomFieldDefn->m_eGeomFormat = OSGF_SpatiaLite;
            if( nSRID > 0 )
            {
                poGeomFieldDefn->m_nSRSId = nSRID;
                poGeomFieldDefn->SetSpatialRef( FetchSRS( nSRID ) );
            }
        }

        OGRFeature* poFeature = poLayer->GetNextFeature();
        if (poFeature)
        {
            OGRGeometry* poGeom = poFeature->GetGeometryRef();
            if (poGeom)
                poLayer->GetLayerDefn()->SetGeomType(poGeom->getGeometryType());
            delete poFeature;
        }
        poLayer->ResetReading();
        return true;
    }

    return false;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

bool OGRSQLiteDataSource::OpenTable( const char *pszTableName,
                                    bool bIsTable,
                                    bool bIsVirtualShape )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRSQLiteTableLayer *poLayer = new OGRSQLiteTableLayer( this );
    if( poLayer->Initialize( pszTableName, bIsTable, bIsVirtualShape, false) != CE_None )
    {
        delete poLayer;
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    m_papoLayers = (OGRSQLiteLayer **)
        CPLRealloc( m_papoLayers,  sizeof(OGRSQLiteLayer *) * (m_nLayers+1) );
    m_papoLayers[m_nLayers++] = poLayer;

    return true;
}

/************************************************************************/
/*                             OpenView()                               */
/************************************************************************/

bool OGRSQLiteDataSource::OpenView( const char *pszViewName,
                                   const char *pszViewGeometry,
                                   const char *pszViewRowid,
                                   const char *pszTableName,
                                   const char *pszGeometryColumn)

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRSQLiteViewLayer *poLayer = new OGRSQLiteViewLayer( this );

    if( poLayer->Initialize( pszViewName, pszViewGeometry,
                             pszViewRowid, pszTableName, pszGeometryColumn ) != CE_None )
    {
        delete poLayer;
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    m_papoLayers = (OGRSQLiteLayer **)
        CPLRealloc( m_papoLayers,  sizeof(OGRSQLiteLayer *) * (m_nLayers+1) );
    m_papoLayers[m_nLayers++] = poLayer;

    return true;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return GetUpdate();
    else if( EQUAL(pszCap,ODsCDeleteLayer) )
        return GetUpdate();
    else if( EQUAL(pszCap,ODsCCurveGeometries) )
        return !m_bIsSpatiaLiteDB;
    else if( EQUAL(pszCap,ODsCMeasuredGeometries) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCCreateGeomFieldAfterCreateLayer) )
        return GetUpdate();
    else if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return GetUpdate();
    else
        return OGRSQLiteBaseDataSource::TestCapability(pszCap);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteBaseDataSource::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap,ODsCTransactions) )
        return TRUE;
    else
        return GDALPamDataset::TestCapability(pszCap);
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRSQLiteDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= m_nLayers )
        return nullptr;
    else
        return m_papoLayers[iLayer];
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/

OGRLayer *OGRSQLiteDataSource::GetLayerByName( const char* pszLayerName )

{
    OGRLayer* poLayer = GDALDataset::GetLayerByName(pszLayerName);
    if( poLayer != nullptr )
        return poLayer;

    for( size_t i=0; i<m_apoInvisibleLayers.size(); ++i)
    {
        if( EQUAL(m_apoInvisibleLayers[i]->GetName(), pszLayerName) )
            return m_apoInvisibleLayers[i];
    }

    std::string osName(pszLayerName);
    bool bIsTable = true;
    for(int i = 0; i < 2; i++ )
    {
        char* pszSQL = sqlite3_mprintf("SELECT type FROM sqlite_master "
                                       "WHERE type IN ('table', 'view') AND "
                                       "lower(name) = lower('%q')",
                                       osName.c_str());
        int nRowCount = 0;
        char** papszResult = nullptr;
        CPL_IGNORE_RET_VAL(
            sqlite3_get_table( hDB, pszSQL, &papszResult, &nRowCount, nullptr, nullptr ));
        if( papszResult && nRowCount == 1 && papszResult[1] )
            bIsTable = strcmp(papszResult[1], "table") == 0;
        sqlite3_free_table(papszResult);
        sqlite3_free(pszSQL);
        if( i == 0 && nRowCount == 0 )
        {
            const auto nParenthesis = osName.find('(');
            if( nParenthesis != std::string::npos && osName.back() == ')' )
            {
                osName.resize(nParenthesis);
                continue;
            }
        }
        break;
    }

    if( !OpenTable(pszLayerName, bIsTable, false) )
        return nullptr;

    poLayer = m_papoLayers[m_nLayers-1];
    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    poLayer->GetLayerDefn();
    CPLPopErrorHandler();
    if( CPLGetLastErrorType() != 0 )
    {
        CPLErrorReset();
        delete poLayer;
        m_nLayers --;
        return nullptr;
    }

    return poLayer;
}

/************************************************************************/
/*                    IsLayerPrivate()                                  */
/************************************************************************/

bool OGRSQLiteDataSource::IsLayerPrivate( int iLayer ) const
{
    if( iLayer < 0 || iLayer >= m_nLayers )
        return false;

    const std::string osName( m_papoLayers[iLayer]->GetName() );
    const CPLString osLCName(CPLString(osName).tolower());
    for ( const char* systemTableName : {
        "spatialindex",
        "geom_cols_ref_sys",
        "geometry_columns",
        "geometry_columns_auth",
        "views_geometry_column",
        "virts_geometry_column",
        "spatial_ref_sys",
        "spatial_ref_sys_all",
        "spatial_ref_sys_aux",
        "sqlite_sequence",
        "tableprefix_metadata",
        "tableprefix_rasters",
        "layer_params",
        "layer_statistics",
        "layer_sub_classes",
        "layer_table_layout",
        "pattern_bitmaps",
        "symbol_bitmaps",
        "project_defs",
        "raster_pyramids",
        "sqlite_stat1",
        "sqlite_stat2",
        "spatialite_history",
        "geometry_columns_field_infos",
        "geometry_columns_statistics",
        "geometry_columns_time",
        "sql_statements_log",
        "vector_layers",
        "vector_layers_auth",
        "vector_layers_field_infos",
        "vector_layers_statistics",
        "views_geometry_columns_auth",
        "views_geometry_columns_field_infos",
        "views_geometry_columns_statistics",
        "virts_geometry_columns_auth",
        "virts_geometry_columns_field_infos",
        "virts_geometry_columns_statistics",
        "virts_layer_statistics",
        "views_layer_statistics",
        "elementarygeometries" } )
    {
        if ( osLCName == systemTableName )
            return true;
    }

    return false;
}

/************************************************************************/
/*                    GetLayerByNameNotVisible()                        */
/************************************************************************/

OGRLayer *OGRSQLiteDataSource::GetLayerByNameNotVisible( const char* pszLayerName )

{
    {
        OGRLayer* poLayer = GDALDataset::GetLayerByName(pszLayerName);
        if( poLayer != nullptr )
            return poLayer;
    }

    for( size_t i=0; i<m_apoInvisibleLayers.size(); ++i)
    {
        if( EQUAL(m_apoInvisibleLayers[i]->GetName(), pszLayerName) )
            return m_apoInvisibleLayers[i];
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRSQLiteTableLayer *poLayer = new OGRSQLiteTableLayer( this );
    if( poLayer->Initialize( pszLayerName, true, false, false) != CE_None )
    {
        delete poLayer;
        return nullptr;
    }
    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    poLayer->GetLayerDefn();
    CPLPopErrorHandler();
    if( CPLGetLastErrorType() != 0 )
    {
        CPLErrorReset();
        delete poLayer;
        return nullptr;
    }
    m_apoInvisibleLayers.push_back(poLayer);

    return poLayer;
}

/************************************************************************/
/*                   GetLayerWithGetSpatialWhereByName()                */
/************************************************************************/

std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>
    OGRSQLiteDataSource::GetLayerWithGetSpatialWhereByName( const char* pszName )
{
    OGRSQLiteLayer* poRet = (OGRSQLiteLayer*) GetLayerByName(pszName);
    return std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*>(poRet, poRet);
}

/************************************************************************/
/*                              FlushCache()                            */
/************************************************************************/

void OGRSQLiteDataSource::FlushCache(bool bAtClosing)
{
    for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
    {
        if( m_papoLayers[iLayer]->IsTableLayer() )
        {
            OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) m_papoLayers[iLayer];
            poLayer->RunDeferredCreationIfNecessary();
            poLayer->CreateSpatialIndexIfNecessary();
        }
    }
    GDALDataset::FlushCache(bAtClosing);
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

static const char* const apszFuncsWithSideEffects[] =
{
    "InitSpatialMetaData",
    "AddGeometryColumn",
    "RecoverGeometryColumn",
    "DiscardGeometryColumn",
    "CreateSpatialIndex",
    "CreateMbrCache",
    "DisableSpatialIndex",
    "UpdateLayerStatistics",

    "ogr_datasource_load_layers"
};

OGRLayer * OGRSQLiteDataSource::ExecuteSQL( const char *pszSQLCommand,
                                          OGRGeometry *poSpatialFilter,
                                          const char *pszDialect )

{
    for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
    {
        if( m_papoLayers[iLayer]->IsTableLayer() )
        {
            OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) m_papoLayers[iLayer];
            poLayer->RunDeferredCreationIfNecessary();
            poLayer->CreateSpatialIndexIfNecessary();
        }
    }

    if( pszDialect != nullptr && EQUAL(pszDialect,"OGRSQL") )
        return GDALDataset::ExecuteSQL( pszSQLCommand,
                                          poSpatialFilter,
                                          pszDialect );

/* -------------------------------------------------------------------- */
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "DELLAYER:") )
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while( *pszLayerName == ' ' )
            pszLayerName++;

        DeleteLayer( pszLayerName );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Special case GetVSILFILE() command (used by GDAL MBTiles driver)*/
/* -------------------------------------------------------------------- */
    if (strcmp(pszSQLCommand, "GetVSILFILE()") == 0)
    {
        if (fpMainFile == nullptr)
            return nullptr;

        char szVal[64];
        int nRet = CPLPrintPointer( szVal, fpMainFile, sizeof(szVal)-1 );
        szVal[nRet] = '\0';
        return new OGRSQLiteSingleFeatureLayer( "VSILFILE", szVal );
    }

/* -------------------------------------------------------------------- */
/*      Special case for SQLITE_HAS_COLUMN_METADATA()                   */
/* -------------------------------------------------------------------- */
    if (strcmp(pszSQLCommand, "SQLITE_HAS_COLUMN_METADATA()") == 0)
    {
#ifdef SQLITE_HAS_COLUMN_METADATA
        return new OGRSQLiteSingleFeatureLayer( "SQLITE_HAS_COLUMN_METADATA", TRUE );
#else
        return new OGRSQLiteSingleFeatureLayer( "SQLITE_HAS_COLUMN_METADATA", FALSE );
#endif
    }

/* -------------------------------------------------------------------- */
/*      In case, this is not a SELECT, invalidate cached feature        */
/*      count and extent to be on the safe side.                        */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszSQLCommand, "VACUUM") )
    {
        int nNeedRefresh = -1;
        for( int i = 0; i < m_nLayers; i++ )
        {
            if( m_papoLayers[i]->IsTableLayer() )
            {
                OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) m_papoLayers[i];
                if ( !(poLayer->AreStatisticsValid()) ||
                     poLayer->DoStatisticsNeedToBeFlushed())
                {
                    nNeedRefresh = FALSE;
                    break;
                }
                else if( nNeedRefresh < 0 )
                    nNeedRefresh = TRUE;
            }
        }
        if( nNeedRefresh == TRUE )
        {
            for( int i = 0; i < m_nLayers; i++ )
            {
                if( m_papoLayers[i]->IsTableLayer() )
                {
                    OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) m_papoLayers[i];
                    poLayer->ForceStatisticsToBeFlushed();
                }
            }
        }
    }
    else if( !STARTS_WITH_CI(pszSQLCommand, "SELECT ") && !EQUAL(pszSQLCommand, "BEGIN")
        && !EQUAL(pszSQLCommand, "COMMIT")
        && !STARTS_WITH_CI(pszSQLCommand, "CREATE TABLE ")
        && !STARTS_WITH_CI(pszSQLCommand, "PRAGMA ") )
    {
        for(int i = 0; i < m_nLayers; i++)
            m_papoLayers[i]->InvalidateCachedFeatureCountAndExtent();
    }

    m_bLastSQLCommandIsUpdateLayerStatistics =
        EQUAL(pszSQLCommand, "SELECT UpdateLayerStatistics()");

/* -------------------------------------------------------------------- */
/*      Prepare statement.                                              */
/* -------------------------------------------------------------------- */
    sqlite3_stmt *hSQLStmt = nullptr;

    CPLString osSQLCommand = pszSQLCommand;

    /* This will speed-up layer creation */
    /* ORDER BY are costly to evaluate and are not necessary to establish */
    /* the layer definition. */
    bool bUseStatementForGetNextFeature = true;
    bool bEmptyLayer = false;

    if( osSQLCommand.ifind("SELECT ") == 0 &&
        CPLString(osSQLCommand.substr(1)).ifind("SELECT ") == std::string::npos &&
        osSQLCommand.ifind(" UNION ") == std::string::npos &&
        osSQLCommand.ifind(" INTERSECT ") == std::string::npos &&
        osSQLCommand.ifind(" EXCEPT ") == std::string::npos )
    {
        size_t nOrderByPos = osSQLCommand.ifind(" ORDER BY ");
        if( nOrderByPos != std::string::npos )
        {
            osSQLCommand.resize(nOrderByPos);
            bUseStatementForGetNextFeature = false;
        }
    }

    int rc = sqlite3_prepare_v2( GetDB(), osSQLCommand.c_str(),
                              static_cast<int>(osSQLCommand.size()),
                              &hSQLStmt, nullptr );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "In ExecuteSQL(): sqlite3_prepare_v2(%s):\n  %s",
                  osSQLCommand.c_str(), sqlite3_errmsg(GetDB()) );

        if( hSQLStmt != nullptr )
        {
            sqlite3_finalize( hSQLStmt );
        }

        return nullptr;
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
                  osSQLCommand.c_str(), sqlite3_errmsg(GetDB()) );

            sqlite3_finalize( hSQLStmt );
            return nullptr;
        }

        if( STARTS_WITH_CI(pszSQLCommand, "CREATE ") )
        {
            char **papszTokens = CSLTokenizeString( pszSQLCommand );
            if ( CSLCount(papszTokens) >= 4 &&
                 EQUAL(papszTokens[1], "VIRTUAL") &&
                 EQUAL(papszTokens[2], "TABLE") )
            {
                OpenVirtualTable(papszTokens[3], pszSQLCommand);
            }
            CSLDestroy(papszTokens);

            sqlite3_finalize( hSQLStmt );
            return nullptr;
        }

        if( !STARTS_WITH_CI(pszSQLCommand, "SELECT ") )
        {
            sqlite3_finalize( hSQLStmt );
            return nullptr;
        }

        bUseStatementForGetNextFeature = false;
        bEmptyLayer = true;
    }

/* -------------------------------------------------------------------- */
/*      Special case for some functions which must be run               */
/*      only once                                                       */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "SELECT ") )
    {
        for( unsigned int i=0;
             i < sizeof(apszFuncsWithSideEffects) /
                 sizeof(apszFuncsWithSideEffects[0]);
             i++ )
        {
            if( EQUALN(apszFuncsWithSideEffects[i], pszSQLCommand + 7,
                       strlen(apszFuncsWithSideEffects[i])) )
            {
                if (sqlite3_column_count( hSQLStmt ) == 1 &&
                    sqlite3_column_type( hSQLStmt, 0 ) == SQLITE_INTEGER )
                {
                    const int ret = sqlite3_column_int( hSQLStmt, 0 );

                    sqlite3_finalize( hSQLStmt );

                    return new OGRSQLiteSingleFeatureLayer
                                        ( apszFuncsWithSideEffects[i], ret );
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create layer.                                                   */
/* -------------------------------------------------------------------- */

    CPLString osSQL = pszSQLCommand;
    OGRSQLiteSelectLayer* poLayer = new OGRSQLiteSelectLayer(
        this, osSQL, hSQLStmt,
        bUseStatementForGetNextFeature, bEmptyLayer, true );

    if( poSpatialFilter != nullptr && poLayer->GetLayerDefn()->GetGeomFieldCount() > 0 )
        poLayer->SetSpatialFilter( 0, poSpatialFilter );

    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRSQLiteDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRSQLiteDataSource::ICreateLayer( const char * pszLayerNameIn,
                                  OGRSpatialReference *poSRS,
                                  OGRwkbGeometryType eType,
                                  char ** papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    char *pszLayerName = nullptr;
    if( !GetUpdate() )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "New layer %s cannot be created.\n",
                  m_pszFilename, pszLayerNameIn );

        return nullptr;
    }

    if ( m_bIsSpatiaLiteDB && eType != wkbNone )
    {
        // We need to catch this right now as AddGeometryColumn does not
        // return an error
        OGRwkbGeometryType eFType = wkbFlatten(eType);
        if( eFType > wkbGeometryCollection )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "Cannot create geometry field of type %s",
                    OGRToOGCGeomType(eType));
            return nullptr;
        }
    }

    for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
    {
        if( m_papoLayers[iLayer]->IsTableLayer() )
        {
            OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) m_papoLayers[iLayer];
            poLayer->RunDeferredCreationIfNecessary();
        }
    }

    CPLString osFIDColumnName;
    const char* pszFIDColumnNameIn = CSLFetchNameValueDef(papszOptions, "FID", "OGC_FID");
    if( CPLFetchBool(papszOptions, "LAUNDER", true) )
    {
        char* pszFIDColumnName = LaunderName(pszFIDColumnNameIn);
        osFIDColumnName = pszFIDColumnName;
        CPLFree(pszFIDColumnName);
    }
    else
        osFIDColumnName = pszFIDColumnNameIn;

    if( CPLFetchBool(papszOptions, "LAUNDER", true) )
        pszLayerName = LaunderName( pszLayerNameIn );
    else
        pszLayerName = CPLStrdup( pszLayerNameIn );

    const char *pszGeomFormat = CSLFetchNameValue( papszOptions, "FORMAT" );
    if( pszGeomFormat == nullptr )
    {
        if ( !m_bIsSpatiaLiteDB )
            pszGeomFormat = "WKB";
        else
            pszGeomFormat = "SpatiaLite";
    }

    if( !EQUAL(pszGeomFormat,"WKT")
        && !EQUAL(pszGeomFormat,"WKB")
        && !EQUAL(pszGeomFormat, "SpatiaLite") )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "FORMAT=%s not recognised or supported.",
                  pszGeomFormat );
        CPLFree( pszLayerName );
        return nullptr;
    }

    CPLString osGeometryName;
    const char* pszGeometryNameIn = CSLFetchNameValue( papszOptions, "GEOMETRY_NAME" );
    if( pszGeometryNameIn == nullptr )
    {
        osGeometryName = ( EQUAL(pszGeomFormat,"WKT") ) ? "WKT_GEOMETRY" : "GEOMETRY";
    }
    else
    {
        if( CPLFetchBool(papszOptions,"LAUNDER", true) )
        {
            char* pszGeometryName = LaunderName(pszGeometryNameIn);
            osGeometryName = pszGeometryName;
            CPLFree(pszGeometryName);
        }
        else
            osGeometryName = pszGeometryNameIn;
    }

    if (m_bIsSpatiaLiteDB && !EQUAL(pszGeomFormat, "SpatiaLite") )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "FORMAT=%s not supported on a SpatiaLite enabled database.",
                  pszGeomFormat );
        CPLFree( pszLayerName );
        return nullptr;
    }

    // Should not happen since a spatialite DB should be opened in
    // read-only mode if libspatialite is not loaded.
    if (m_bIsSpatiaLiteDB && !IsSpatialiteLoaded())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Creating layers on a SpatiaLite enabled database, "
                  "without Spatialite extensions loaded, is not supported." );
        CPLFree( pszLayerName );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */
    for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,m_papoLayers[iLayer]->GetLayerDefn()->GetName()) )
        {
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != nullptr
                && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),"NO") )
            {
                DeleteLayer( pszLayerName );
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          pszLayerName );
                CPLFree( pszLayerName );
                return nullptr;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to get the SRS Id of this spatial reference system,         */
/*      adding to the srs table if needed.                              */
/* -------------------------------------------------------------------- */
    int nSRSId = m_nUndefinedSRID;
    const char* pszSRID = CSLFetchNameValue(papszOptions, "SRID");

    if( pszSRID != nullptr )
    {
        nSRSId = atoi(pszSRID);
        if( nSRSId > 0 )
        {
            OGRSpatialReference* poSRSFetched = FetchSRS( nSRSId );
            if( poSRSFetched == nullptr )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "SRID %d will be used, but no matching SRS is defined in spatial_ref_sys",
                         nSRSId);
            }
        }
    }
    else if( poSRS != nullptr )
        nSRSId = FetchSRSId( poSRS );

    bool bImmediateSpatialIndexCreation = false;
    bool bDeferredSpatialIndexCreation = false;

    const char* pszSI = CSLFetchNameValue( papszOptions, "SPATIAL_INDEX" );
    if( m_bHaveGeometryColumns && eType != wkbNone )
    {
        if ( pszSI != nullptr && CPLTestBool(pszSI) &&
             (m_bIsSpatiaLiteDB || EQUAL(pszGeomFormat, "SpatiaLite")) && !IsSpatialiteLoaded() )
        {
            CPLError( CE_Warning, CPLE_OpenFailed,
                    "Cannot create a spatial index when Spatialite extensions are not loaded." );
        }

#ifdef HAVE_SPATIALITE
        /* Only if linked against SpatiaLite and the datasource was created as a SpatiaLite DB */
        if ( m_bIsSpatiaLiteDB && IsSpatialiteLoaded() )
#else
        if ( 0 )
#endif
        {
            if( pszSI != nullptr && EQUAL(pszSI, "IMMEDIATE") )
            {
                bImmediateSpatialIndexCreation = true;
            }
            else if( pszSI == nullptr || CPLTestBool(pszSI) )
            {
                bDeferredSpatialIndexCreation = true;
            }
        }
    }
    else if( m_bHaveGeometryColumns )
    {
#ifdef HAVE_SPATIALITE
        if( m_bIsSpatiaLiteDB && IsSpatialiteLoaded() && (pszSI == nullptr || CPLTestBool(pszSI)) )
            bDeferredSpatialIndexCreation = true;
#endif
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRSQLiteTableLayer *poLayer = new OGRSQLiteTableLayer( this );

    poLayer->Initialize( pszLayerName, true, false, true ) ;
    OGRSpatialReference* poSRSClone = poSRS;
    if( poSRSClone )
    {
        poSRSClone = poSRSClone->Clone();
        poSRSClone->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    poLayer->SetCreationParameters( osFIDColumnName, eType, pszGeomFormat,
                                    osGeometryName, poSRSClone, nSRSId );
    if( poSRSClone )
        poSRSClone->Release();

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    m_papoLayers = (OGRSQLiteLayer **)
        CPLRealloc( m_papoLayers,  sizeof(OGRSQLiteLayer *) * (m_nLayers+1) );

    m_papoLayers[m_nLayers++] = poLayer;

    poLayer->InitFeatureCount();
    poLayer->SetLaunderFlag( CPLFetchBool(papszOptions, "LAUNDER", true) );
    if( CPLFetchBool(papszOptions, "COMPRESS_GEOM", false) )
        poLayer->SetUseCompressGeom( true );
    if( bImmediateSpatialIndexCreation )
        poLayer->CreateSpatialIndex(0);
    else if( bDeferredSpatialIndexCreation )
        poLayer->SetDeferredSpatialIndexCreation( true );
    poLayer->SetCompressedColumns( CSLFetchNameValue(papszOptions,"COMPRESS_COLUMNS") );
    poLayer->SetStrictFlag( CPLFetchBool(papszOptions, "STRICT", false) );

    CPLFree( pszLayerName );

    return poLayer;
}

/************************************************************************/
/*                            LaunderName()                             */
/************************************************************************/

char *OGRSQLiteDataSource::LaunderName( const char *pszSrcName )

{
    char *pszSafeName = CPLStrdup( pszSrcName );
    for( int i = 0; pszSafeName[i] != '\0'; i++ )
    {
        pszSafeName[i] = (char) tolower( pszSafeName[i] );
        if( pszSafeName[i] == '\'' || pszSafeName[i] == '-' || pszSafeName[i] == '#' )
            pszSafeName[i] = '_';
    }

    return pszSafeName;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

void OGRSQLiteDataSource::DeleteLayer( const char *pszLayerName )

{
/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !GetUpdate() )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "Layer %s cannot be deleted.\n",
                  m_pszFilename, pszLayerName );

        return;
    }

/* -------------------------------------------------------------------- */
/*      Try to find layer.                                              */
/* -------------------------------------------------------------------- */
    int iLayer = 0;  // Used after for.

    for( ; iLayer < m_nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,m_papoLayers[iLayer]->GetLayerDefn()->GetName()) )
            break;
    }

    if( iLayer == m_nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to delete layer '%s', but this layer is not known to OGR.",
                  pszLayerName );
        return;
    }

    DeleteLayer(iLayer);
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRSQLiteDataSource::DeleteLayer(int iLayer)
{
    if( iLayer < 0 || iLayer >= m_nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer %d not in legal range of 0 to %d.",
                  iLayer, m_nLayers-1 );
        return OGRERR_FAILURE;
    }

    CPLString osLayerName = GetLayer(iLayer)->GetName();
    CPLString osGeometryColumn = GetLayer(iLayer)->GetGeometryColumn();

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLDebug( "OGR_SQLITE", "DeleteLayer(%s)", osLayerName.c_str() );

    delete m_papoLayers[iLayer];
    memmove( m_papoLayers + iLayer, m_papoLayers + iLayer + 1,
             sizeof(void *) * (m_nLayers - iLayer - 1) );
    m_nLayers--;

/* -------------------------------------------------------------------- */
/*      Remove from the database.                                       */
/* -------------------------------------------------------------------- */
    CPLString osEscapedLayerName = SQLEscapeLiteral(osLayerName);
    const char* pszEscapedLayerName = osEscapedLayerName.c_str();
    const char* pszGeometryColumn = osGeometryColumn.size() ? osGeometryColumn.c_str() : nullptr;

    if( SQLCommand( hDB,
                    CPLSPrintf( "DROP TABLE '%s'", pszEscapedLayerName ) )
                                                            != OGRERR_NONE )
    {
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Drop from geometry_columns table.                               */
/* -------------------------------------------------------------------- */
    if( m_bHaveGeometryColumns )
    {
        CPLString osCommand;

        osCommand.Printf(
            "DELETE FROM geometry_columns WHERE f_table_name = '%s'",
            pszEscapedLayerName );

        if( SQLCommand( hDB, osCommand ) != OGRERR_NONE )
        {
            return OGRERR_FAILURE;
        }

/* -------------------------------------------------------------------- */
/*      Drop spatialite spatial index tables                            */
/* -------------------------------------------------------------------- */
        if( m_bIsSpatiaLiteDB && pszGeometryColumn )
        {
            osCommand.Printf( "DROP TABLE 'idx_%s_%s'", pszEscapedLayerName,
                              SQLEscapeLiteral(pszGeometryColumn).c_str());
            CPL_IGNORE_RET_VAL(sqlite3_exec( hDB, osCommand, nullptr, nullptr, nullptr ));

            osCommand.Printf( "DROP TABLE 'idx_%s_%s_node'", pszEscapedLayerName,
                              SQLEscapeLiteral(pszGeometryColumn).c_str());
            CPL_IGNORE_RET_VAL(sqlite3_exec( hDB, osCommand, nullptr, nullptr, nullptr ));

            osCommand.Printf( "DROP TABLE 'idx_%s_%s_parent'", pszEscapedLayerName,
                              SQLEscapeLiteral(pszGeometryColumn).c_str());
            CPL_IGNORE_RET_VAL(sqlite3_exec( hDB, osCommand, nullptr, nullptr, nullptr ));

            osCommand.Printf( "DROP TABLE 'idx_%s_%s_rowid'", pszEscapedLayerName,
                              SQLEscapeLiteral(pszGeometryColumn).c_str());
            CPL_IGNORE_RET_VAL(sqlite3_exec( hDB, osCommand, nullptr, nullptr, nullptr ));
        }
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                         StartTransaction()                           */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRSQLiteBaseDataSource::StartTransaction(CPL_UNUSED int bForce)
{
    if( bUserTransactionActive || nSoftTransactionLevel != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Transaction already established");
        return OGRERR_FAILURE;
    }

    OGRErr eErr = SoftStartTransaction();
    if( eErr != OGRERR_NONE )
        return eErr;

    bUserTransactionActive = true;
    return OGRERR_NONE;
}

/************************************************************************/
/*                         CommitTransaction()                          */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRSQLiteBaseDataSource::CommitTransaction()
{
    if( !bUserTransactionActive )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Transaction not established");
        return OGRERR_FAILURE;
    }

    bUserTransactionActive = false;
    CPLAssert( nSoftTransactionLevel == 1 );
    return SoftCommitTransaction();
}

OGRErr OGRSQLiteDataSource::CommitTransaction()

{
    if( nSoftTransactionLevel == 1 )
    {
        for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
        {
            if( m_papoLayers[iLayer]->IsTableLayer() )
            {
                OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) m_papoLayers[iLayer];
                poLayer->RunDeferredCreationIfNecessary();
            }
        }
    }

    return OGRSQLiteBaseDataSource::CommitTransaction();
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/*                                                                      */
/* Should only be called by user code. Not driver internals.            */
/************************************************************************/

OGRErr OGRSQLiteBaseDataSource::RollbackTransaction()
{
    if( !bUserTransactionActive )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Transaction not established");
        return OGRERR_FAILURE;
    }

    bUserTransactionActive = false;
    CPLAssert( nSoftTransactionLevel == 1 );
    return SoftRollbackTransaction();
}

OGRErr OGRSQLiteDataSource::RollbackTransaction()

{
    if( nSoftTransactionLevel == 1 )
    {
        for( int iLayer = 0; iLayer < m_nLayers; iLayer++ )
        {
            if( m_papoLayers[iLayer]->IsTableLayer() )
            {
                OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) m_papoLayers[iLayer];
                poLayer->RunDeferredCreationIfNecessary();
            }
        }

        for(int i = 0; i < m_nLayers; i++)
        {
            m_papoLayers[i]->InvalidateCachedFeatureCountAndExtent();
            m_papoLayers[i]->ResetReading();
        }
    }

    return OGRSQLiteBaseDataSource::RollbackTransaction();
}

/************************************************************************/
/*                        SoftStartTransaction()                        */
/*                                                                      */
/*      Create a transaction scope.  If we already have a               */
/*      transaction active this isn't a real transaction, but just      */
/*      an increment to the scope count.                                */
/************************************************************************/

OGRErr OGRSQLiteBaseDataSource::SoftStartTransaction()

{
    nSoftTransactionLevel++;

    OGRErr eErr = OGRERR_NONE;
    if( nSoftTransactionLevel == 1 )
    {
        eErr = DoTransactionCommand("BEGIN");
    }

    //CPLDebug("SQLite", "%p->SoftStartTransaction() : %d",
    //         this, nSoftTransactionLevel);

    return eErr;
}

/************************************************************************/
/*                     SoftCommitTransaction()                          */
/*                                                                      */
/*      Commit the current transaction if we are at the outer           */
/*      scope.                                                          */
/************************************************************************/

OGRErr OGRSQLiteBaseDataSource::SoftCommitTransaction()

{
    //CPLDebug("SQLite", "%p->SoftCommitTransaction() : %d",
    //         this, nSoftTransactionLevel);

    if( nSoftTransactionLevel <= 0 )
    {
        CPLAssert(false);
        return OGRERR_FAILURE;
    }

    OGRErr eErr = OGRERR_NONE;
    nSoftTransactionLevel--;
    if( nSoftTransactionLevel == 0 )
    {
        eErr = DoTransactionCommand("COMMIT");
    }

    return eErr;
}

/************************************************************************/
/*                  SoftRollbackTransaction()                           */
/*                                                                      */
/*      Do a rollback of the current transaction if we are at the 1st   */
/*      level                                                           */
/************************************************************************/

OGRErr OGRSQLiteBaseDataSource::SoftRollbackTransaction()

{
    //CPLDebug("SQLite", "%p->SoftRollbackTransaction() : %d",
    //         this, nSoftTransactionLevel);

    if( nSoftTransactionLevel <= 0 )
    {
        CPLAssert(false);
        return OGRERR_FAILURE;
    }

    OGRErr eErr = OGRERR_NONE;
    nSoftTransactionLevel--;
    if( nSoftTransactionLevel == 0 )
    {
        eErr = DoTransactionCommand("ROLLBACK");
    }

    return eErr;
}

/************************************************************************/
/*                          DoTransactionCommand()                      */
/************************************************************************/

OGRErr OGRSQLiteBaseDataSource::DoTransactionCommand(const char* pszCommand)

{
#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "%s Transaction", pszCommand );
#endif

    return SQLCommand( hDB, pszCommand );
}

/************************************************************************/
/*                          GetSRTEXTColName()                        */
/************************************************************************/

const char* OGRSQLiteDataSource::GetSRTEXTColName()
{
    if( !m_bIsSpatiaLiteDB || m_bSpatialite4Layout )
        return "srtext";

    // Testing for SRS_WKT column presence.
    bool bHasSrsWkt = false;
    char **papszResult = nullptr;
    int nRowCount = 0;
    int nColCount = 0;
    char *pszErrMsg = nullptr;
    const int rc =
        sqlite3_get_table( hDB, "PRAGMA table_info(spatial_ref_sys)",
                           &papszResult, &nRowCount, &nColCount,
                           &pszErrMsg );

    if( rc == SQLITE_OK )
    {
        for( int iRow = 1; iRow <= nRowCount; iRow++ )
        {
            if (EQUAL("srs_wkt",
                        papszResult[(iRow * nColCount) + 1]))
                bHasSrsWkt = true;
        }
        sqlite3_free_table(papszResult);
    }
    else
    {
        sqlite3_free( pszErrMsg );
    }

    return bHasSrsWkt ? "srs_wkt" : nullptr;
}

/************************************************************************/
/*                         AddSRIDToCache()                             */
/*                                                                      */
/*      Note: this will not add a reference on the poSRS object. Make   */
/*      sure it is freshly created, or add a reference yourself if not. */
/************************************************************************/

void OGRSQLiteDataSource::AddSRIDToCache(int nId, OGRSpatialReference * poSRS )
{
/* -------------------------------------------------------------------- */
/*      Add to the cache.                                               */
/* -------------------------------------------------------------------- */
    m_panSRID = (int *) CPLRealloc(m_panSRID,sizeof(int) * (m_nKnownSRID+1) );
    m_papoSRS = (OGRSpatialReference **)
        CPLRealloc(m_papoSRS, sizeof(void*) * (m_nKnownSRID + 1) );
    m_panSRID[m_nKnownSRID] = nId;
    m_papoSRS[m_nKnownSRID] = poSRS;
    m_nKnownSRID++;
}

/************************************************************************/
/*                             FetchSRSId()                             */
/*                                                                      */
/*      Fetch the id corresponding to an SRS, and if not found, add     */
/*      it to the table.                                                */
/************************************************************************/

int OGRSQLiteDataSource::FetchSRSId( const OGRSpatialReference * poSRS )

{
    int nSRSId = m_nUndefinedSRID;
    if( poSRS == nullptr )
        return nSRSId;

/* -------------------------------------------------------------------- */
/*      First, we look through our SRID cache, is it there?             */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < m_nKnownSRID; i++ )
    {
        if( m_papoSRS[i] == poSRS )
            return m_panSRID[i];
    }
    for( int i = 0; i < m_nKnownSRID; i++ )
    {
        if( m_papoSRS[i] != nullptr && m_papoSRS[i]->IsSame(poSRS) )
            return m_panSRID[i];
    }

/* -------------------------------------------------------------------- */
/*      Build a copy since we may call AutoIdentifyEPSG()               */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS(*poSRS);
    poSRS = nullptr;

    const char *pszAuthorityName = oSRS.GetAuthorityName(nullptr);
    const char *pszAuthorityCode = nullptr;

    if( pszAuthorityName == nullptr || strlen(pszAuthorityName) == 0 )
    {
/* -------------------------------------------------------------------- */
/*      Try to identify an EPSG code                                    */
/* -------------------------------------------------------------------- */
        oSRS.AutoIdentifyEPSG();

        pszAuthorityName = oSRS.GetAuthorityName(nullptr);
        if (pszAuthorityName != nullptr && EQUAL(pszAuthorityName, "EPSG"))
        {
            pszAuthorityCode = oSRS.GetAuthorityCode(nullptr);
            if ( pszAuthorityCode != nullptr && strlen(pszAuthorityCode) > 0 )
            {
                /* Import 'clean' SRS */
                oSRS.importFromEPSG( atoi(pszAuthorityCode) );

                pszAuthorityName = oSRS.GetAuthorityName(nullptr);
                pszAuthorityCode = oSRS.GetAuthorityCode(nullptr);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Check whether the EPSG authority code is already mapped to a    */
/*      SRS ID.                                                         */
/* -------------------------------------------------------------------- */
    char *pszErrMsg = nullptr;
    CPLString osCommand;
    char **papszResult = nullptr;
    int nRowCount = 0;
    int nColCount = 0;

    if( pszAuthorityName != nullptr && strlen(pszAuthorityName) > 0 )
    {
        pszAuthorityCode = oSRS.GetAuthorityCode(nullptr);

        if ( pszAuthorityCode != nullptr && strlen(pszAuthorityCode) > 0 )
        {
            // XXX: We are using case insensitive comparison for "auth_name"
            // values, because there are variety of options exist. By default
            // the driver uses 'EPSG' in upper case, but SpatiaLite extension
            // uses 'epsg' in lower case.
            osCommand.Printf( "SELECT srid FROM spatial_ref_sys WHERE "
                              "auth_name = '%s' COLLATE NOCASE AND auth_srid = '%s' "
                              "LIMIT 2",
                              pszAuthorityName, pszAuthorityCode );

            int rc = sqlite3_get_table( hDB, osCommand, &papszResult,
                                        &nRowCount, &nColCount, &pszErrMsg );
            if( rc != SQLITE_OK )
            {
                /* Retry without COLLATE NOCASE which may not be understood by older sqlite3 */
                sqlite3_free( pszErrMsg );

                osCommand.Printf( "SELECT srid FROM spatial_ref_sys WHERE "
                                  "auth_name = '%s' AND auth_srid = '%s'",
                                  pszAuthorityName, pszAuthorityCode );

                rc = sqlite3_get_table( hDB, osCommand, &papszResult,
                                        &nRowCount, &nColCount, &pszErrMsg );

                /* Retry in lower case for SpatiaLite */
                if( rc != SQLITE_OK )
                {
                    sqlite3_free( pszErrMsg );
                }
                else if ( nRowCount == 0 &&
                          strcmp(pszAuthorityName, "EPSG") == 0)
                {
                    /* If it is in upper case, look for lower case */
                    sqlite3_free_table(papszResult);

                    osCommand.Printf( "SELECT srid FROM spatial_ref_sys WHERE "
                                      "auth_name = 'epsg' AND auth_srid = '%s' "
                                      "LIMIT 2",
                                      pszAuthorityCode );

                    rc = sqlite3_get_table( hDB, osCommand, &papszResult,
                                            &nRowCount, &nColCount, &pszErrMsg );

                    if( rc != SQLITE_OK )
                    {
                        sqlite3_free( pszErrMsg );
                    }
                }
            }

            if( rc == SQLITE_OK && nRowCount == 1 )
            {
                nSRSId = (papszResult[1] != nullptr) ? atoi(papszResult[1]) : m_nUndefinedSRID;
                sqlite3_free_table(papszResult);

                if( nSRSId != m_nUndefinedSRID)
                {
                    auto poCachedSRS = new OGRSpatialReference(oSRS);
                    poCachedSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    AddSRIDToCache(nSRSId, poCachedSRS);
                }

                return nSRSId;
            }
            sqlite3_free_table(papszResult);
        }
    }

/* -------------------------------------------------------------------- */
/*      Search for existing record using either WKT definition or       */
/*      PROJ.4 string (SpatiaLite variant).                             */
/* -------------------------------------------------------------------- */
    CPLString osWKT;
    CPLString osProj4;

/* -------------------------------------------------------------------- */
/*      Translate SRS to WKT.                                           */
/* -------------------------------------------------------------------- */
    char *pszWKT = nullptr;

    if( oSRS.exportToWkt( &pszWKT ) != OGRERR_NONE )
    {
        CPLFree(pszWKT);
        return m_nUndefinedSRID;
    }

    osWKT = pszWKT;
    CPLFree( pszWKT );
    pszWKT = nullptr;

    const char* pszSRTEXTColName = GetSRTEXTColName();

    if ( pszSRTEXTColName != nullptr )
    {
/* -------------------------------------------------------------------- */
/*      Try to find based on the WKT match.                             */
/* -------------------------------------------------------------------- */
        osCommand.Printf( "SELECT srid FROM spatial_ref_sys WHERE \"%s\" = ? "
                          "LIMIT 2",
                          SQLEscapeName(pszSRTEXTColName).c_str());
    }

/* -------------------------------------------------------------------- */
/*      Handle SpatiaLite (< 4) flavor of the spatial_ref_sys.         */
/* -------------------------------------------------------------------- */
    else
    {
/* -------------------------------------------------------------------- */
/*      Translate SRS to PROJ.4 string.                                 */
/* -------------------------------------------------------------------- */
        char    *pszProj4 = nullptr;

        if( oSRS.exportToProj4( &pszProj4 ) != OGRERR_NONE )
        {
            CPLFree(pszProj4);
            return m_nUndefinedSRID;
        }

        osProj4 = pszProj4;
        CPLFree( pszProj4 );
        pszProj4 = nullptr;

/* -------------------------------------------------------------------- */
/*      Try to find based on the PROJ.4 match.                          */
/* -------------------------------------------------------------------- */
        osCommand.Printf(
            "SELECT srid FROM spatial_ref_sys WHERE proj4text = ? LIMIT 2");
    }

    sqlite3_stmt *hSelectStmt = nullptr;
    int rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hSelectStmt, nullptr );

    if( rc == SQLITE_OK)
        rc = sqlite3_bind_text( hSelectStmt, 1,
                                ( pszSRTEXTColName != nullptr ) ? osWKT.c_str() : osProj4.c_str(),
                                -1, SQLITE_STATIC );

    if( rc == SQLITE_OK)
        rc = sqlite3_step( hSelectStmt );

    if (rc == SQLITE_ROW)
    {
        if (sqlite3_column_type( hSelectStmt, 0 ) == SQLITE_INTEGER)
            nSRSId = sqlite3_column_int( hSelectStmt, 0 );
        else
            nSRSId = m_nUndefinedSRID;

        sqlite3_finalize( hSelectStmt );

        if( nSRSId != m_nUndefinedSRID)
            AddSRIDToCache(nSRSId, new OGRSpatialReference(oSRS));

        return nSRSId;
    }

/* -------------------------------------------------------------------- */
/*      If the command actually failed, then the metadata table is      */
/*      likely missing, so we give up.                                  */
/* -------------------------------------------------------------------- */
    if (rc != SQLITE_DONE && rc != SQLITE_ROW)
    {
        sqlite3_finalize( hSelectStmt );
        return m_nUndefinedSRID;
    }

    sqlite3_finalize( hSelectStmt );

/* -------------------------------------------------------------------- */
/*      Translate SRS to PROJ.4 string (if not already done)            */
/* -------------------------------------------------------------------- */
    if( osProj4.empty() )
    {
        char* pszProj4 = nullptr;
        if( oSRS.exportToProj4( &pszProj4 ) == OGRERR_NONE )
        {
            osProj4 = pszProj4;
        }
        CPLFree( pszProj4 );
        pszProj4 = nullptr;
    }

/* -------------------------------------------------------------------- */
/*      If we have an authority code try to assign SRS ID the same      */
/*      as that code.                                                   */
/* -------------------------------------------------------------------- */
    if ( pszAuthorityCode != nullptr && strlen(pszAuthorityCode) > 0 )
    {
        osCommand.Printf( "SELECT * FROM spatial_ref_sys WHERE auth_srid='%s' "
                          "LIMIT 2",
                          SQLEscapeLiteral(pszAuthorityCode).c_str() );
        rc = sqlite3_get_table( hDB, osCommand, &papszResult,
                                &nRowCount, &nColCount, &pszErrMsg );

        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "exec(SELECT '%s' FROM spatial_ref_sys) failed: %s",
                      pszAuthorityCode, pszErrMsg );
            sqlite3_free( pszErrMsg );
        }

/* -------------------------------------------------------------------- */
/*      If there is no SRS ID with such auth_srid, use it as SRS ID.    */
/* -------------------------------------------------------------------- */
        if ( nRowCount < 1 )
        {
            nSRSId = atoi(pszAuthorityCode);
            /* The authority code might be non numeric, e.g. IGNF:LAMB93 */
            /* in which case we might fallback to the fake OGR authority */
            /* for spatialite, since its auth_srid is INTEGER */
            if( nSRSId == 0 )
            {
                nSRSId = m_nUndefinedSRID;
                if( m_bIsSpatiaLiteDB )
                    pszAuthorityName = nullptr;
            }
        }
        sqlite3_free_table(papszResult);
    }

/* -------------------------------------------------------------------- */
/*      Otherwise get the current maximum srid in the srs table.        */
/* -------------------------------------------------------------------- */
    if ( nSRSId == m_nUndefinedSRID )
    {
        rc = sqlite3_get_table( hDB, "SELECT MAX(srid) FROM spatial_ref_sys",
                                &papszResult, &nRowCount, &nColCount,
                                &pszErrMsg );

        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "SELECT of the maximum SRS ID failed: %s", pszErrMsg );
            sqlite3_free( pszErrMsg );
            return m_nUndefinedSRID;
        }

        if ( nRowCount < 1 || !papszResult[1] )
            nSRSId = 50000;
        else
            nSRSId = atoi(papszResult[1]) + 1;  // Insert as the next SRS ID
        sqlite3_free_table(papszResult);
    }

/* -------------------------------------------------------------------- */
/*      Try adding the SRS to the SRS table.                            */
/* -------------------------------------------------------------------- */

    const char* apszToInsert[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    if ( !m_bIsSpatiaLiteDB )
    {
        if( pszAuthorityName != nullptr )
        {
            osCommand.Printf(
                "INSERT INTO spatial_ref_sys (srid,srtext,auth_name,auth_srid) "
                "                     VALUES (%d, ?, ?, ?)",
                nSRSId );
            apszToInsert[0] = osWKT.c_str();
            apszToInsert[1] = pszAuthorityName;
            apszToInsert[2] = pszAuthorityCode;
        }
        else
        {
            osCommand.Printf(
                "INSERT INTO spatial_ref_sys (srid,srtext) "
                "                     VALUES (%d, ?)",
                nSRSId );
            apszToInsert[0] = osWKT.c_str();
        }
    }
    else
    {
        CPLString osSRTEXTColNameWithCommaBefore;
        if( pszSRTEXTColName != nullptr )
            osSRTEXTColNameWithCommaBefore.Printf(", %s", pszSRTEXTColName);

        const char  *pszProjCS = oSRS.GetAttrValue("PROJCS");
        if (pszProjCS == nullptr)
            pszProjCS = oSRS.GetAttrValue("GEOGCS");

        if( pszAuthorityName != nullptr )
        {
            if ( pszProjCS )
            {
                osCommand.Printf(
                    "INSERT INTO spatial_ref_sys "
                    "(srid, auth_name, auth_srid, ref_sys_name, proj4text%s) "
                    "VALUES (%d, ?, ?, ?, ?%s)",
                    (pszSRTEXTColName != nullptr) ? osSRTEXTColNameWithCommaBefore.c_str() : "",
                    nSRSId,
                    (pszSRTEXTColName != nullptr) ? ", ?" : "");
                apszToInsert[0] = pszAuthorityName;
                apszToInsert[1] = pszAuthorityCode;
                apszToInsert[2] = pszProjCS;
                apszToInsert[3] = osProj4.c_str();
                apszToInsert[4] = (pszSRTEXTColName != nullptr) ? osWKT.c_str() : nullptr;
            }
            else
            {
                osCommand.Printf(
                    "INSERT INTO spatial_ref_sys "
                    "(srid, auth_name, auth_srid, proj4text%s) "
                    "VALUES (%d, ?, ?, ?%s)",
                    (pszSRTEXTColName != nullptr) ? osSRTEXTColNameWithCommaBefore.c_str() : "",
                    nSRSId,
                    (pszSRTEXTColName != nullptr) ? ", ?" : "");
                apszToInsert[0] = pszAuthorityName;
                apszToInsert[1] = pszAuthorityCode;
                apszToInsert[2] = osProj4.c_str();
                apszToInsert[3] = (pszSRTEXTColName != nullptr) ? osWKT.c_str() : nullptr;
            }
        }
        else
        {
            /* SpatiaLite spatial_ref_sys auth_name and auth_srid columns must be NOT NULL */
            /* so insert within a fake OGR "authority" */
            if ( pszProjCS )
            {
                osCommand.Printf(
                    "INSERT INTO spatial_ref_sys "
                    "(srid, auth_name, auth_srid, ref_sys_name, proj4text%s) VALUES (%d, 'OGR', %d, ?, ?%s)",
                    (pszSRTEXTColName != nullptr) ? osSRTEXTColNameWithCommaBefore.c_str() : "",
                    nSRSId, nSRSId,
                    (pszSRTEXTColName != nullptr) ? ", ?" : "");
                apszToInsert[0] = pszProjCS;
                apszToInsert[1] = osProj4.c_str();
                apszToInsert[2] = (pszSRTEXTColName != nullptr) ? osWKT.c_str() : nullptr;
            }
            else
            {
                osCommand.Printf(
                    "INSERT INTO spatial_ref_sys "
                    "(srid, auth_name, auth_srid, proj4text%s) VALUES (%d, 'OGR', %d, ?%s)",
                    (pszSRTEXTColName != nullptr) ? osSRTEXTColNameWithCommaBefore.c_str() : "",
                    nSRSId, nSRSId,
                    (pszSRTEXTColName != nullptr) ? ", ?" : "");
                apszToInsert[0] = osProj4.c_str();
                apszToInsert[1] = (pszSRTEXTColName != nullptr) ? osWKT.c_str() : nullptr;
            }
        }
    }

    sqlite3_stmt *hInsertStmt = nullptr;
    rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hInsertStmt, nullptr );

    for( int i = 0; apszToInsert[i] != nullptr; i++ )
    {
        if( rc == SQLITE_OK)
            rc = sqlite3_bind_text( hInsertStmt, i+1, apszToInsert[i], -1, SQLITE_STATIC );
    }

    if( rc == SQLITE_OK)
        rc = sqlite3_step( hInsertStmt );

    if( rc != SQLITE_OK && rc != SQLITE_DONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to insert SRID (%s): %s",
                  osCommand.c_str(), sqlite3_errmsg(hDB) );

        sqlite3_finalize( hInsertStmt );
        return FALSE;
    }

    sqlite3_finalize( hInsertStmt );

    if( nSRSId != m_nUndefinedSRID)
    {
        auto poCachedSRS = new OGRSpatialReference(oSRS);
        poCachedSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        AddSRIDToCache(nSRSId, poCachedSRS);
    }

    return nSRSId;
}

/************************************************************************/
/*                              FetchSRS()                              */
/*                                                                      */
/*      Return a SRS corresponding to a particular id.  Note that       */
/*      reference counting should be honoured on the returned           */
/*      OGRSpatialReference, as handles may be cached.                  */
/************************************************************************/

OGRSpatialReference *OGRSQLiteDataSource::FetchSRS( int nId )

{
    if( nId <= 0 )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      First, we look through our SRID cache, is it there?             */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < m_nKnownSRID; i++ )
    {
        if( m_panSRID[i] == nId )
            return m_papoSRS[i];
    }

/* -------------------------------------------------------------------- */
/*      Try looking up in spatial_ref_sys table.                        */
/* -------------------------------------------------------------------- */
    char *pszErrMsg = nullptr;
    char **papszResult = nullptr;
    int nRowCount = 0;
    int nColCount = 0;
    OGRSpatialReference *poSRS = nullptr;

    CPLString osCommand;
    osCommand.Printf( "SELECT srtext FROM spatial_ref_sys WHERE srid = %d "
                      "LIMIT 2",
                      nId );
    int rc =
        sqlite3_get_table(
            hDB, osCommand,
            &papszResult, &nRowCount, &nColCount, &pszErrMsg );

    if ( rc == SQLITE_OK )
    {
        if( nRowCount < 1 )
        {
            sqlite3_free_table(papszResult);
            return nullptr;
        }

        char** papszRow = papszResult + nColCount;
        if (papszRow[0] != nullptr)
        {
            CPLString osWKT = papszRow[0];

/* -------------------------------------------------------------------- */
/*      Translate into a spatial reference.                             */
/* -------------------------------------------------------------------- */
            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if( poSRS->importFromWkt( osWKT.c_str() ) != OGRERR_NONE )
            {
                delete poSRS;
                poSRS = nullptr;
            }
        }

        sqlite3_free_table(papszResult);
    }

/* -------------------------------------------------------------------- */
/*      Next try SpatiaLite flavor. SpatiaLite uses PROJ.4 strings     */
/*      in 'proj4text' column instead of WKT in 'srtext'. Note: recent  */
/*      versions of spatialite have a srs_wkt column too                */
/* -------------------------------------------------------------------- */
    else
    {
        sqlite3_free( pszErrMsg );
        pszErrMsg = nullptr;

        const char* pszSRTEXTColName = GetSRTEXTColName();
        CPLString osSRTEXTColNameWithCommaBefore;
        if( pszSRTEXTColName != nullptr )
            osSRTEXTColNameWithCommaBefore.Printf(", %s", pszSRTEXTColName);

        osCommand.Printf(
            "SELECT proj4text, auth_name, auth_srid%s FROM spatial_ref_sys "
            "WHERE srid = %d LIMIT 2",
            (pszSRTEXTColName != nullptr) ? osSRTEXTColNameWithCommaBefore.c_str() : "", nId );
        rc = sqlite3_get_table( hDB, osCommand,
                                &papszResult, &nRowCount,
                                &nColCount, &pszErrMsg );
        if ( rc == SQLITE_OK )
        {
            if( nRowCount < 1 )
            {
                sqlite3_free_table(papszResult);
                return nullptr;
            }

/* -------------------------------------------------------------------- */
/*      Translate into a spatial reference.                             */
/* -------------------------------------------------------------------- */
            char** papszRow = papszResult + nColCount;

            const char* pszProj4Text = papszRow[0];
            const char* pszAuthName = papszRow[1];
            int nAuthSRID = (papszRow[2] != nullptr) ? atoi(papszRow[2]) : 0;
            const char* pszWKT = (pszSRTEXTColName != nullptr) ? papszRow[3] : nullptr;

            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

            /* Try first from EPSG code */
            if (pszAuthName != nullptr &&
                EQUAL(pszAuthName, "EPSG") &&
                poSRS->importFromEPSG( nAuthSRID ) == OGRERR_NONE)
            {
                /* Do nothing */
            }
            /* Then from WKT string */
            else if( pszWKT != nullptr &&
                     poSRS->importFromWkt( pszWKT ) == OGRERR_NONE )
            {
                /* Do nothing */
            }
            /* Finally from Proj4 string */
            else if( pszProj4Text != nullptr &&
                     poSRS->importFromProj4( pszProj4Text ) == OGRERR_NONE )
            {
                /* Do nothing */
            }
            else
            {
                delete poSRS;
                poSRS = nullptr;
            }

            sqlite3_free_table(papszResult);
        }

/* -------------------------------------------------------------------- */
/*      No success, report an error.                                    */
/* -------------------------------------------------------------------- */
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s: %s", osCommand.c_str(), pszErrMsg );
            sqlite3_free( pszErrMsg );
            return nullptr;
        }
    }

    if( poSRS )
        poSRS->StripTOWGS84IfKnownDatumAndAllowed();

/* -------------------------------------------------------------------- */
/*      Add to the cache.                                               */
/* -------------------------------------------------------------------- */
    AddSRIDToCache(nId, poSRS);

    return poSRS;
}

/************************************************************************/
/*                              SetName()                               */
/************************************************************************/

void OGRSQLiteDataSource::SetName(const char* pszNameIn)
{
    CPLFree(m_pszFilename);
    m_pszFilename = CPLStrdup(pszNameIn);
}

/************************************************************************/
/*                       GetEnvelopeFromSQL()                           */
/************************************************************************/

const OGREnvelope* OGRSQLiteBaseDataSource::GetEnvelopeFromSQL(const CPLString& osSQL)
{
    std::map<CPLString, OGREnvelope>::iterator oIter = oMapSQLEnvelope.find(osSQL);
    if (oIter != oMapSQLEnvelope.end())
        return &oIter->second;
    else
        return nullptr;
}

/************************************************************************/
/*                         SetEnvelopeForSQL()                          */
/************************************************************************/

void OGRSQLiteBaseDataSource::SetEnvelopeForSQL(const CPLString& osSQL,
                                            const OGREnvelope& oEnvelope)
{
    oMapSQLEnvelope[osSQL] = oEnvelope;
}

/************************************************************************/
/*                         AbortSQL()                                   */
/************************************************************************/

OGRErr OGRSQLiteBaseDataSource::AbortSQL()
{
    sqlite3_interrupt( hDB );
    return OGRERR_NONE;
}
