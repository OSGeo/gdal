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
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "cpl_string.h"
#include "cpl_hash_set.h"
#include "cpl_csv.h"
#include "cpl_multiproc.h"
#include "ogrsqlitevirtualogr.h"
#include "ogrsqliteutility.h"

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

#ifndef SPATIALITE_412_OR_LATER
static int bSpatialiteGlobalLoaded = FALSE;
#endif

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
        bSpatialiteGlobalLoaded = TRUE;
        spatialite_init(CPLTestBool(CPLGetConfigOption("SPATIALITE_INIT_VERBOSE", "FALSE")));
    }
#endif
    return bSpatialiteGlobalLoaded;
}

/************************************************************************/
/*                          OGRSQLiteDriverUnload()                     */
/************************************************************************/

void OGRSQLiteDriverUnload(GDALDriver*)
{
}

#else // defined(SPATIALITE_412_OR_LATER)

#ifdef SPATIALITE_DLOPEN
static CPLMutex* hMutexLoadSpatialiteSymbols = NULL;
static void * (*pfn_spatialite_alloc_connection) (void) = NULL;
static void (*pfn_spatialite_shutdown) (void) = NULL;
static void (*pfn_spatialite_init_ex) (sqlite3*, const void *, int) = NULL;
static void (*pfn_spatialite_cleanup_ex) (const void *) = NULL;
static const char *(*pfn_spatialite_version) (void) = NULL;
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
      return pfn_spatialite_alloc_connection != NULL;
    bInitializationDone = true;

    const char *pszLibName = CPLGetConfigOption("SPATIALITESO",
                                                SPATIALITE_SONAME);
    CPLPushErrorHandler( CPLQuietErrorHandler );

    /* coverity[tainted_string] */
    pfn_spatialite_alloc_connection = (void* (*)(void))
                    CPLGetSymbol( pszLibName, "spatialite_alloc_connection" );
    CPLPopErrorHandler();

    if( pfn_spatialite_alloc_connection == NULL )
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
    if( pfn_spatialite_shutdown == NULL ||
        pfn_spatialite_init_ex == NULL ||
        pfn_spatialite_cleanup_ex == NULL ||
        pfn_spatialite_version == NULL )
    {
        pfn_spatialite_shutdown = NULL;
        pfn_spatialite_init_ex = NULL;
        pfn_spatialite_cleanup_ex = NULL;
        pfn_spatialite_version = NULL;
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
    if( pfn_spatialite_shutdown != NULL )
        pfn_spatialite_shutdown();
#ifdef SPATIALITE_DLOPEN
    if( hMutexLoadSpatialiteSymbols != NULL )
    {
        CPLDestroyMutex(hMutexLoadSpatialiteSymbols);
        hMutexLoadSpatialiteSymbols = NULL;
    }
#endif
}

/************************************************************************/
/*                          InitNewSpatialite()                         */
/************************************************************************/

bool OGRSQLiteBaseDataSource::InitNewSpatialite()
{
    if( hSpatialiteCtxt == NULL &&
        CPLTestBool(CPLGetConfigOption("SPATIALITE_LOAD", "TRUE")) )
    {
#ifdef SPATIALITE_DLOPEN
        if( !OGRSQLiteLoadSpatialiteSymbols() )
            return false;
#endif
        CPLAssert(hSpatialiteCtxt == NULL);
        hSpatialiteCtxt = pfn_spatialite_alloc_connection();
        if( hSpatialiteCtxt != NULL )
        {
            pfn_spatialite_init_ex(hDB, hSpatialiteCtxt,
                CPLTestBool(CPLGetConfigOption("SPATIALITE_INIT_VERBOSE", "FALSE")));
        }
    }
    return hSpatialiteCtxt != NULL;
}

/************************************************************************/
/*                         FinishNewSpatialite()                        */
/************************************************************************/

void OGRSQLiteBaseDataSource::FinishNewSpatialite()
{
    if( hSpatialiteCtxt != NULL )
    {
        pfn_spatialite_cleanup_ex(hSpatialiteCtxt);
        hSpatialiteCtxt = NULL;
    }
}

#endif // defined(SPATIALITE_412_OR_LATER)

#ifdef HAVE_RASTERLITE2

/************************************************************************/
/*                          InitRasterLite2()                           */
/************************************************************************/

bool OGRSQLiteBaseDataSource::InitRasterLite2()
{
    CPLAssert(m_hRL2Ctxt == NULL);
    m_hRL2Ctxt = rl2_alloc_private();
    if( m_hRL2Ctxt != NULL )
    {
        rl2_init (hDB, m_hRL2Ctxt, 0);
    }
    return m_hRL2Ctxt != NULL;
}

/************************************************************************/
/*                         FinishRasterLite2()                          */
/************************************************************************/

void OGRSQLiteBaseDataSource::FinishRasterLite2()
{
    if( m_hRL2Ctxt != NULL )
    {
        rl2_cleanup_private(m_hRL2Ctxt);
        m_hRL2Ctxt = NULL;
    }
}

#endif // HAVE_RASTERLITE2


/************************************************************************/
/*                          IsSpatialiteLoaded()                        */
/************************************************************************/

int OGRSQLiteDataSource::IsSpatialiteLoaded()
{
#ifdef SPATIALITE_412_OR_LATER
    return hSpatialiteCtxt != NULL;
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

OGRSQLiteBaseDataSource::OGRSQLiteBaseDataSource() :
    m_pszFilename(NULL),
    hDB(NULL),
    bUpdate(FALSE),
    pMyVFS(NULL),
    fpMainFile(NULL),  // Do not close. The VFS layer will do it for us.
#ifdef SPATIALITE_412_OR_LATER
    hSpatialiteCtxt(NULL),
#endif
#ifdef HAVE_RASTERLITE2
    m_hRL2Ctxt(NULL),
#endif
    bUserTransactionActive(FALSE),
    nSoftTransactionLevel(0)
{}

/************************************************************************/
/*                      ~OGRSQLiteBaseDataSource()                      */
/************************************************************************/

OGRSQLiteBaseDataSource::~OGRSQLiteBaseDataSource()

{
#ifdef SPATIALITE_412_OR_LATER
    FinishNewSpatialite();
#endif
#ifdef HAVE_RASTERLITE2
    FinishRasterLite2();
#endif
    CloseDB();
    CPLFree(m_pszFilename);
}

/************************************************************************/
/*                               CloseDB()                              */
/************************************************************************/

void OGRSQLiteBaseDataSource::CloseDB()
{
    if( hDB != NULL )
    {
        sqlite3_close( hDB );
        hDB = NULL;

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
            if( hDB != NULL )
            {
                // Dummy request
                int nRowCount = 0, nColCount = 0;
                char** papszResult = NULL;
                sqlite3_get_table( hDB,
                                    "SELECT name FROM sqlite_master WHERE 0",
                                    &papszResult, &nRowCount, &nColCount,
                                    NULL );
                sqlite3_free_table( papszResult );

                sqlite3_close( hDB );
                hDB = NULL;
            }
        }

    }

    if (pMyVFS)
    {
        sqlite3_vfs_unregister(pMyVFS);
        CPLFree(pMyVFS->pAppData);
        CPLFree(pMyVFS);
        pMyVFS = NULL;
    }
}

/************************************************************************/
/*                        OGRSQLiteDataSource()                         */
/************************************************************************/

OGRSQLiteDataSource::OGRSQLiteDataSource() :
    papoLayers(NULL),
    nLayers(0),
    nKnownSRID(0),
    panSRID(NULL),
    papoSRS(NULL),
    papszOpenOptions(NULL),
    bHaveGeometryColumns(FALSE),
    bIsSpatiaLiteDB(FALSE),
    bSpatialite4Layout(FALSE),
    nUndefinedSRID(-1),  // Will be set to 0 if Spatialite >= 4.0 detected.
    nFileTimestamp(0),
    bLastSQLCommandIsUpdateLayerStatistics(FALSE),
#ifdef HAVE_RASTERLITE2
    m_nSectionId(-1),
    m_pRL2Coverage(NULL),
    m_bRL2MixedResolutions(false),
#endif
    m_bGeoTransformValid(false),
    m_bPromote1BitAs8Bit(false),
    m_poParentDS(NULL)
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
    if( m_pRL2Coverage != NULL )
    {
        rl2_destroy_coverage( m_pRL2Coverage );
    }
#endif
    for( size_t i=0; i < m_apoOverviewDS.size(); ++i )
    {
        delete m_apoOverviewDS[i];
    }

    if( nLayers > 0 || !apoInvisibleLayers.empty() )
    {
        // Close any remaining iterator
        for( int i = 0; i < nLayers; i++ )
            papoLayers[i]->ResetReading();
        for( size_t i = 0; i < apoInvisibleLayers.size(); i++ )
            apoInvisibleLayers[i]->ResetReading();

        // Create spatial indices in a transaction for faster execution
        if( hDB )
            SoftStartTransaction();
        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            if( papoLayers[iLayer]->IsTableLayer() )
            {
                OGRSQLiteTableLayer* poLayer =
                    (OGRSQLiteTableLayer*) papoLayers[iLayer];
                poLayer->RunDeferredCreationIfNecessary();
                poLayer->CreateSpatialIndexIfNecessary();
            }
        }
        if( hDB )
            SoftCommitTransaction();
    }

    SaveStatistics();

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    for( size_t i = 0; i < apoInvisibleLayers.size(); i++ )
        delete apoInvisibleLayers[i];

    CPLFree( papoLayers );

    for( int i = 0; i < nKnownSRID; i++ )
    {
        if( papoSRS[i] != NULL )
            papoSRS[i]->Release();
    }
    CPLFree( panSRID );
    CPLFree( papoSRS );
    CSLDestroy( papszOpenOptions );
}

/************************************************************************/
/*                              SaveStatistics()                        */
/************************************************************************/

void OGRSQLiteDataSource::SaveStatistics()
{
    if( !bIsSpatiaLiteDB || !IsSpatialiteLoaded() ||
        bLastSQLCommandIsUpdateLayerStatistics || !bUpdate )
        return;

    int nSavedAllLayersCacheData = -1;

    for( int i = 0; i < nLayers; i++ )
    {
        if( papoLayers[i]->IsTableLayer() )
        {
            OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) papoLayers[i];
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
        SQLResult oResult;
        int nReplaceEventId = -1;

        CPL_IGNORE_RET_VAL( SQLQuery( hDB,
                  "SELECT event_id, table_name, geometry_column, event "
                  "FROM spatialite_history ORDER BY event_id DESC LIMIT 1",
                  &oResult ) );

        if( oResult.nRowCount == 1 )
        {
            const char* pszEventId = SQLResultGetValue(&oResult, 0, 0);
            const char* pszTableName = SQLResultGetValue(&oResult, 1, 0);
            const char* pszGeomCol = SQLResultGetValue(&oResult, 2, 0);
            const char* pszEvent = SQLResultGetValue(&oResult, 3, 0);

            if( pszEventId != NULL && pszTableName != NULL &&
                pszGeomCol != NULL && pszEvent != NULL &&
                strcmp(pszTableName, "ALL-TABLES") == 0 &&
                strcmp(pszGeomCol, "ALL-GEOMETRY-COLUMNS") == 0 &&
                strcmp(pszEvent, "UpdateLayerStatistics") == 0 )
            {
                nReplaceEventId = atoi(pszEventId);
            }
        }
        SQLResultFree(&oResult);

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
    const char* pszSqliteSync = CPLGetConfigOption("OGR_SQLITE_SYNCHRONOUS", NULL);
    if (pszSqliteSync != NULL)
    {
        const char* pszSQL = NULL;
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

        return pszSQL != NULL && SQLCommand(hDB, pszSQL) == OGRERR_NONE;
    }
    return true;
}

/************************************************************************/
/*                              SetCacheSize()                          */
/************************************************************************/

bool OGRSQLiteBaseDataSource::SetCacheSize()
{
    const char* pszSqliteCacheMB = CPLGetConfigOption("OGR_SQLITE_CACHE", NULL);
    if (pszSqliteCacheMB != NULL)
    {
        const GIntBig iSqliteCacheBytes = 
            static_cast<GIntBig>(atoi( pszSqliteCacheMB )) * 1024 * 1024;

        /* querying the current PageSize */
        int iSqlitePageSize = SQLGetInteger(hDB, "PRAGMA page_size", NULL);
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
static const int DMA_SIGNATURE = 0x444D4139;

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

int OGRSQLiteBaseDataSource::OpenOrCreateDB(int flagsIn, int bRegisterOGR2SQLiteExtensions)
{
#ifdef USE_SQLITE_DEBUG_MEMALLOC
    if( CPLTestBool(CPLGetConfigOption("USE_SQLITE_DEBUG_MEMALLOC", "NO")) )
        sqlite3_config(SQLITE_CONFIG_MALLOC, &sDebugMemAlloc);
#endif

    if( bRegisterOGR2SQLiteExtensions )
        OGR2SQLITE_Register();

    // No mutex since OGR objects are not supposed to be used concurrently
    // from multiple threads.
    int flags = flagsIn | SQLITE_OPEN_NOMUTEX;
#ifdef SQLITE_OPEN_URI
    // This code enables support for named memory databases in SQLite.
    // SQLITE_USE_URI is checked only to enable backward compatibility, in
    // case we accidentally hijacked some other format.
    if( STARTS_WITH(m_pszFilename, "file:") &&
        CPLTestBool(CPLGetConfigOption("SQLITE_USE_URI", "YES")) )
    {
        flags |= SQLITE_OPEN_URI;
    }
#endif

    int rc = SQLITE_OK;

    const bool bUseOGRVFS =
        CPLTestBool(CPLGetConfigOption("SQLITE_USE_OGR_VFS", "NO"));
    if (bUseOGRVFS || STARTS_WITH(m_pszFilename, "/vsi"))
    {
        pMyVFS = OGRSQLiteCreateVFS(OGRSQLiteBaseDataSourceNotifyFileOpened, this);
        sqlite3_vfs_register(pMyVFS, 0);
        rc = sqlite3_open_v2( m_pszFilename, &hDB, flags, pMyVFS->zName );
    }
    else
    {
        rc = sqlite3_open_v2( m_pszFilename, &hDB, flags, NULL );
    }

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "sqlite3_open(%s) failed: %s",
                  m_pszFilename, sqlite3_errmsg( hDB ) );
        return FALSE;
    }

    if( (flagsIn & SQLITE_OPEN_CREATE) == 0 )
    {
        if( CPLTestBool(CPLGetConfigOption("OGR_VFK_DB_READ", "NO")) )
        {
            if( SQLGetInteger( hDB,
                               "SELECT 1 FROM sqlite_master "
                               "WHERE type = 'table' AND name = 'vfk_tables'",
                               NULL ) )
                return FALSE;  /* DB is valid VFK datasource */
        }

        int nRowCount = 0, nColCount = 0;
        char** papszResult = NULL;
        char* pszErrMsg = NULL;
        rc = sqlite3_get_table( hDB,
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
            if( fp != NULL )
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

    const char* pszSqlitePragma =
                            CPLGetConfigOption("OGR_SQLITE_PRAGMA", NULL);
    CPLString osJournalMode =
                        CPLGetConfigOption("OGR_SQLITE_JOURNAL", "");

    bool bPageSizeFound = false;
    if (pszSqlitePragma != NULL)
    {
        char** papszTokens = CSLTokenizeString2( pszSqlitePragma, ",",
                                                 CSLT_HONOURSTRINGS );
        for(int i=0; papszTokens[i] != NULL; i++ )
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
                    break;
                }
            }

            const char* pszSQL = CPLSPrintf("PRAGMA %s", papszTokens[i]);

            CPL_IGNORE_RET_VAL(
                sqlite3_exec( hDB, pszSQL, NULL, NULL, NULL ) );
        }
        CSLDestroy(papszTokens);
    }

    if( !bPageSizeFound && (flagsIn & SQLITE_OPEN_CREATE) != 0 )
    {
        // Since sqlite 3.12 the default page_size is now 4096. But we
        // can use that even with older versions.
        CPL_IGNORE_RET_VAL(
            sqlite3_exec( hDB, "PRAGMA page_size = 4096", NULL, NULL, NULL ) );
    }

    // journal_mode = WAL must be done *AFTER* changing page size.
    if (!osJournalMode.empty())
    {
        const char* pszSQL = CPLSPrintf("PRAGMA journal_mode = %s",
                                        osJournalMode.c_str());

        CPL_IGNORE_RET_VAL(
            sqlite3_exec( hDB, pszSQL, NULL, NULL, NULL ) );
    }

    SetCacheSize();
    SetSynchronous();

    return TRUE;
}

/************************************************************************/
/*                          GetInternalHandle()                         */
/************************************************************************/

/* Used by MBTILES driver */
void *OGRSQLiteBaseDataSource::GetInternalHandle( const char * pszKey )
{
    if( pszKey != NULL && EQUAL(pszKey, "SQLITE_HANDLE") )
        return hDB;
    return NULL;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRSQLiteDataSource::Create( const char * pszNameIn, char **papszOptions )
{
    CPLString osCommand;

    m_pszFilename = CPLStrdup( pszNameIn );

/* -------------------------------------------------------------------- */
/*      Check that spatialite extensions are loaded if required to      */
/*      create a spatialite database                                    */
/* -------------------------------------------------------------------- */
    bool bSpatialite = CPLFetchBool( papszOptions, "SPATIALITE", false );
    int bMetadata = CPLFetchBool( papszOptions, "METADATA", true );

    if( bSpatialite )
    {
#ifdef HAVE_SPATIALITE
#ifndef SPATIALITE_412_OR_LATER
        OGRSQLiteInitOldSpatialite();
        if (!IsSpatialiteLoaded())
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                    "Creating a Spatialite database, but Spatialite extensions are not loaded." );
            return FALSE;
        }
#endif
#else
        CPLError( CE_Failure, CPLE_NotSupported,
            "OGR was built without libspatialite support\n"
            "... sorry, creating/writing any SpatiaLite DB is unsupported\n" );

        return FALSE;
#endif
    }

    bIsSpatiaLiteDB = bSpatialite;

/* -------------------------------------------------------------------- */
/*      Create the database file.                                       */
/* -------------------------------------------------------------------- */
    if (!OpenOrCreateDB(SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, TRUE))
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Create the SpatiaLite metadata tables.                          */
/* -------------------------------------------------------------------- */
    if ( bSpatialite )
    {
#ifdef SPATIALITE_412_OR_LATER
        if (!InitNewSpatialite())
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                    "Creating a Spatialite database, but Spatialite extensions are not loaded." );
            return FALSE;
        }
#endif
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
        if( pszVal != NULL && !CPLTestBool(pszVal) &&
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
            return FALSE;
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
            return FALSE;
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
            return FALSE;
    }

    GDALOpenInfo oOpenInfo(m_pszFilename, GDAL_OF_VECTOR | GDAL_OF_UPDATE);
    return Open(&oOpenInfo);
}

/************************************************************************/
/*                           InitWithEPSG()                             */
/************************************************************************/

int OGRSQLiteDataSource::InitWithEPSG()
{
    CPLString osCommand;

    if ( bIsSpatiaLiteDB )
    {
        /*
        / if v.2.4.0 (or any subsequent) InitWithEPSG make no sense at all
        / because the EPSG dataset is already self-initialized at DB creation
        */
        int iSpatialiteVersion = GetSpatialiteVersionNumber();
        if ( iSpatialiteVersion >= 24 )
            return TRUE;
    }

    if( SoftStartTransaction() != OGRERR_NONE )
        return FALSE;

    int rc = SQLITE_OK;
    for( int i = 0; i < 2 && rc == SQLITE_OK; i++ )
    {
        const char* pszFilename = (i == 0) ? "gcs.csv" : "pcs.csv";
        FILE *fp = VSIFOpen(CSVFilename(pszFilename), "rt");
        if( fp == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                "Unable to open EPSG support file %s.\n"
                "Try setting the GDAL_DATA environment variable to point to the\n"
                "directory containing EPSG csv files.",
                pszFilename );

            continue;
        }

        OGRSpatialReference oSRS;
        CSLDestroy(CSVReadParseLine( fp ));

        char **papszTokens = NULL;
        while ( (papszTokens = CSVReadParseLine( fp )) != NULL && rc == SQLITE_OK)
        {
            int nSRSId = atoi(papszTokens[0]);
            CSLDestroy(papszTokens);

            CPLPushErrorHandler(CPLQuietErrorHandler);
            oSRS.importFromEPSG(nSRSId);
            CPLPopErrorHandler();

            if (bIsSpatiaLiteDB)
            {
                char    *pszProj4 = NULL;

                CPLPushErrorHandler(CPLQuietErrorHandler);
                OGRErr eErr = oSRS.exportToProj4( &pszProj4 );
                CPLPopErrorHandler();

                char    *pszWKT = NULL;
                if( oSRS.exportToWkt( &pszWKT ) != OGRERR_NONE )
                {
                    CPLFree(pszWKT);
                    pszWKT = NULL;
                }

                if( eErr == OGRERR_NONE )
                {
                    const char  *pszProjCS = oSRS.GetAttrValue("PROJCS");
                    if (pszProjCS == NULL)
                        pszProjCS = oSRS.GetAttrValue("GEOGCS");

                    const char* pszSRTEXTColName = GetSRTEXTColName();
                    if (pszSRTEXTColName != NULL)
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

                    sqlite3_stmt *hInsertStmt = NULL;
                    rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hInsertStmt, NULL );

                    if ( pszProjCS )
                    {
                        if( rc == SQLITE_OK)
                            rc = sqlite3_bind_text( hInsertStmt, 1, pszProjCS, -1, SQLITE_STATIC );
                        if( rc == SQLITE_OK)
                            rc = sqlite3_bind_text( hInsertStmt, 2, pszProj4, -1, SQLITE_STATIC );
                        if (pszSRTEXTColName != NULL)
                        {
                        /* the SPATIAL_REF_SYS table supports a SRS_WKT column */
                            if( rc == SQLITE_OK && pszWKT != NULL)
                                rc = sqlite3_bind_text( hInsertStmt, 3, pszWKT, -1, SQLITE_STATIC );
                        }
                    }
                    else
                    {
                        if( rc == SQLITE_OK)
                            rc = sqlite3_bind_text( hInsertStmt, 1, pszProj4, -1, SQLITE_STATIC );
                        if (pszSRTEXTColName != NULL)
                        {
                        /* the SPATIAL_REF_SYS table supports a SRS_WKT column */
                            if( rc == SQLITE_OK && pszWKT != NULL)
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
                char    *pszWKT = NULL;
                if( oSRS.exportToWkt( &pszWKT ) == OGRERR_NONE )
                {
                    osCommand.Printf(
                        "INSERT INTO spatial_ref_sys "
                        "(srid, auth_name, auth_srid, srtext) "
                        "VALUES (%d, 'EPSG', '%d', ?)",
                        nSRSId, nSRSId );

                    sqlite3_stmt *hInsertStmt = NULL;
                    rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hInsertStmt, NULL );

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
        VSIFClose(fp);
    }

    if( rc == SQLITE_OK )
    {
        if( SoftCommitTransaction() != OGRERR_NONE )
            return FALSE;
        return TRUE;
    }
    else
    {
        SoftRollbackTransaction();
        return FALSE;
    }
}

/************************************************************************/
/*                        ReloadLayers()                                */
/************************************************************************/

void OGRSQLiteDataSource::ReloadLayers()
{
    for(int i=0;i<nLayers;i++)
        delete papoLayers[i];
    CPLFree(papoLayers);
    papoLayers = NULL;
    nLayers = 0;

    GDALOpenInfo oOpenInfo(m_pszFilename,
                           GDAL_OF_VECTOR | (bUpdate ? GDAL_OF_UPDATE: 0));
    Open(&oOpenInfo);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRSQLiteDataSource::Open( GDALOpenInfo* poOpenInfo)

{
    const char * pszNewName = poOpenInfo->pszFilename;
    CPLAssert( nLayers == 0 );
    bUpdate = poOpenInfo->eAccess == GA_Update;
    nOpenFlags = poOpenInfo->nOpenFlags;
    SetDescription(pszNewName);

    if (m_pszFilename == NULL)
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
                return FALSE;
            }
            m_pszFilename = CPLStrdup( SQLUnescape( papszTokens[1] ) );
            CSLDestroy(papszTokens);
        }
        else
#endif
        {
            m_pszFilename = CPLStrdup( pszNewName );
        }
    }
    SetPhysicalFilename(m_pszFilename);

    VSIStatBufL sStat;
    if( VSIStatL( m_pszFilename, &sStat ) == 0 )
    {
        nFileTimestamp = sStat.st_mtime;
    }

    if( poOpenInfo->papszOpenOptions )
    {
        CSLDestroy(papszOpenOptions);
        papszOpenOptions = CSLDuplicate(poOpenInfo->papszOpenOptions);
    }

    bool bListVectorLayers = (nOpenFlags & GDAL_OF_VECTOR) != 0;

    bool bListAllTables = bListVectorLayers &&
        CPLTestBool(CSLFetchNameValueDef(
            papszOpenOptions, "LIST_ALL_TABLES",
            CPLGetConfigOption("SQLITE_LIST_ALL_TABLES", "NO")));

    // Don't list by default: there might be some security implications
    // if a user is provided with a file and doesn't know that there are
    // virtual OGR tables in it.
    bool bListVirtualOGRLayers = bListVectorLayers &&
        CPLTestBool(CSLFetchNameValueDef(
            papszOpenOptions, "LIST_VIRTUAL_OGR",
            CPLGetConfigOption("OGR_SQLITE_LIST_VIRTUAL_OGR", "NO")));

/* -------------------------------------------------------------------- */
/*      Try to open the sqlite database properly now.                   */
/* -------------------------------------------------------------------- */
    if (hDB == NULL)
    {
#ifndef SPATIALITE_412_OR_LATER
        OGRSQLiteInitOldSpatialite();
#endif

#ifdef ENABLE_SQL_SQLITE_FORMAT
        if( poOpenInfo->pabyHeader &&
            (STARTS_WITH((const char*)poOpenInfo->pabyHeader, "-- SQL SQLITE") ||
             STARTS_WITH((const char*)poOpenInfo->pabyHeader, "-- SQL RASTERLITE") ||
             STARTS_WITH((const char*)poOpenInfo->pabyHeader, "-- SQL MBTILES")) &&
            poOpenInfo->fpL != NULL )
        {
            if( sqlite3_open_v2( ":memory:", &hDB, SQLITE_OPEN_READWRITE, NULL )
                    != SQLITE_OK )
            {
                return FALSE;
            }

#ifdef SPATIALITE_412_OR_LATER
            // We need it here for ST_MinX() and the like
            InitNewSpatialite();
#endif

            // Ingest the lines of the dump
            VSIFSeekL( poOpenInfo->fpL, 0, SEEK_SET );
            const char* pszLine;
            while( (pszLine = CPLReadLineL( poOpenInfo->fpL )) != NULL )
            {
                if( STARTS_WITH(pszLine, "--") )
                    continue;

                // Blacklist a few words tat might have security implications
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
                        bOK = TRUE;
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
                            bOK = TRUE;
                        }
                        CSLDestroy(papszTokens);
                    }

                    if( !bOK )
                    {
                        CPLError(CE_Failure, CPLE_NotSupported,
                                "Rejected statement: %s", pszLine);
                        return FALSE;
                    }
                }
                char* pszErrMsg = NULL;
                if( sqlite3_exec( hDB, pszLine, NULL, NULL, &pszErrMsg ) != SQLITE_OK )
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
        else
#endif
        if (!OpenOrCreateDB((bUpdate) ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY, TRUE) )
            return FALSE;

#ifdef SPATIALITE_412_OR_LATER
        InitNewSpatialite();
#endif
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

/* -------------------------------------------------------------------- */
/*      If we have a GEOMETRY_COLUMNS tables, initialize on the basis   */
/*      of that.                                                        */
/* -------------------------------------------------------------------- */
    CPLHashSet* hSet = CPLHashSetNew(CPLHashSetHashStr, CPLHashSetEqualStr, CPLFree);

    char **papszResult = NULL;
    char *pszErrMsg = NULL;
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

        bHaveGeometryColumns = TRUE;

        for ( int iRow = 0; bListVectorLayers && iRow < nRowCount; iRow++ )
        {
            char **papszRow = papszResult + iRow * 6 + 6;
            const char* pszTableName = papszRow[0];
            const char* pszGeomCol = papszRow[1];

            if( pszTableName == NULL || pszGeomCol == NULL )
                continue;

            aoMapTableToSetOfGeomCols[pszTableName].insert(CPLString(pszGeomCol).tolower());
        }

        for( int iRow = 0; bListVectorLayers && iRow < nRowCount; iRow++ )
        {
            char **papszRow = papszResult + iRow * 6 + 6;
            const char* pszTableName = papszRow[0];

            if (pszTableName == NULL)
                continue;

            if( GDALDataset::GetLayerByName(pszTableName) == NULL )
                OpenTable( pszTableName );

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
                    if( pszName == NULL || pszSQL == NULL )
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
                return FALSE;
        }

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we can deal with SpatiaLite database.                 */
/* -------------------------------------------------------------------- */
    sqlite3_free( pszErrMsg );
    rc = sqlite3_get_table( hDB,
                            "SELECT f_table_name, f_geometry_column, "
                            "type, coord_dimension, srid, "
                            "spatial_index_enabled FROM geometry_columns "
                            "LIMIT 10000",
                            &papszResult, &nRowCount,
                            &nColCount, &pszErrMsg );
    if (rc != SQLITE_OK )
    {
        /* Test with SpatiaLite 4.0 schema */
        sqlite3_free( pszErrMsg );
        rc = sqlite3_get_table( hDB,
                                "SELECT f_table_name, f_geometry_column, "
                                "geometry_type, coord_dimension, srid, "
                                "spatial_index_enabled FROM geometry_columns "
                                "LIMIT 10000",
                                &papszResult, &nRowCount,
                                &nColCount, &pszErrMsg );
        if ( rc == SQLITE_OK )
        {
            bSpatialite4Layout = TRUE;
            nUndefinedSRID = 0;
        }
    }

    if ( rc == SQLITE_OK )
    {
        bIsSpatiaLiteDB = TRUE;
        bHaveGeometryColumns = TRUE;

        int iSpatialiteVersion = -1;

        /* Only enables write-mode if linked against SpatiaLite */
        if( IsSpatialiteLoaded() )
        {
            iSpatialiteVersion = GetSpatialiteVersionNumber();
        }
        else if( bUpdate )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "SpatiaLite%s DB found, "
                     "but updating tables disabled because no linking against spatialite library !",
                     (bSpatialite4Layout) ? " v4" : "");
            sqlite3_free_table(papszResult);
            CPLHashSetDestroy(hSet);
            return FALSE;
        }

        if (bSpatialite4Layout && bUpdate && iSpatialiteVersion > 0 && iSpatialiteVersion < 40)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "SpatiaLite v4 DB found, "
                     "but updating tables disabled because runtime spatialite library is v%.1f !",
                     iSpatialiteVersion / 10.0);
            sqlite3_free_table(papszResult);
            CPLHashSetDestroy(hSet);
            return FALSE;
        }
        else
        {
            CPLDebug("SQLITE", "SpatiaLite%s DB found !",
                     (bSpatialite4Layout) ? " v4" : "");
        }

        // List RasterLite2 coverages, so as to avoid listing corresponding
        // technical tables
        std::set<CPLString> aoSetTablesToIgnore;
        if( bSpatialite4Layout )
        {
            char** papszResults2 = NULL;
            int nRowCount2 = 0, nColCount2 = 0;
            rc = sqlite3_get_table( hDB,
                                "SELECT name FROM sqlite_master WHERE "
                                "type = 'table' AND name = 'raster_coverages'",
                                &papszResults2, &nRowCount2,
                                &nColCount2, NULL );
            sqlite3_free_table(papszResults2);
            if( rc == SQLITE_OK && nRowCount2 == 1 )
            {
                papszResults2 = NULL;
                nRowCount2 = 0;
                nColCount2 = 0;
                rc = sqlite3_get_table( hDB,
                                "SELECT coverage_name FROM raster_coverages "
                                "LIMIT 10000",
                                &papszResults2, &nRowCount2,
                                &nColCount2, NULL );
                if( rc == SQLITE_OK )
                {
                    for(int i=0;i<nRowCount2;++i)
                    {
                        const char * const* papszRow = papszResults2 + i*1 + 1;
                        if( papszRow[0] != NULL )
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

            if( pszTableName == NULL || pszGeomCol == NULL )
                continue;
            if( !bListAllTables &&
                aoSetTablesToIgnore.find(pszTableName) !=
                                                aoSetTablesToIgnore.end() )
            {
                continue;
            }

            aoMapTableToSetOfGeomCols[pszTableName].insert(CPLString(pszGeomCol).tolower());
        }

        for ( int iRow = 0; bListVectorLayers && iRow < nRowCount; iRow++ )
        {
            char **papszRow = papszResult + iRow * 6 + 6;
            const char* pszTableName = papszRow[0];

            if (pszTableName == NULL )
                continue;
            if( !bListAllTables &&
                aoSetTablesToIgnore.find(pszTableName) !=
                                                aoSetTablesToIgnore.end() )
            {
                continue;
            }

            if( GDALDataset::GetLayerByName(pszTableName) == NULL )
                OpenTable( pszTableName);
            if (bListAllTables)
                CPLHashSetInsert(hSet, CPLStrdup(pszTableName));
        }

        sqlite3_free_table(papszResult);
        papszResult = NULL;

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
                if( pszName == NULL || pszSQL == NULL )
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
        papszResult = NULL;

/* -------------------------------------------------------------------- */
/*      Detect spatial views                                            */
/* -------------------------------------------------------------------- */

        rc = sqlite3_get_table( hDB,
                                "SELECT view_name, view_geometry, view_rowid, "
                                "f_table_name, f_geometry_column "
                                "FROM views_geometry_columns "
                                "LIMIT 10000",
                                &papszResult, &nRowCount,
                                &nColCount, NULL );
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

                if (pszViewName == NULL ||
                    pszViewGeometry == NULL ||
                    pszViewRowid == NULL ||
                    pszTableName == NULL ||
                    pszGeometryColumn == NULL)
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
                return FALSE;
        }

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise our final resort is to return all tables and views    */
/*      as non-spatial tables.                                          */
/* -------------------------------------------------------------------- */
    sqlite3_free( pszErrMsg );

all_tables:
    rc = sqlite3_get_table( hDB,
                            "SELECT name FROM sqlite_master "
                            "WHERE type IN ('table','view') "
                            "UNION ALL "
                            "SELECT name FROM sqlite_temp_master "
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
        return FALSE;
    }

    for( int iRow = 0; iRow < nRowCount; iRow++ )
    {
        const char* pszTableName = papszResult[iRow+1];
        if( pszTableName != NULL && CPLHashSetLookup(hSet, pszTableName) == NULL )
            OpenTable( pszTableName );
    }

    sqlite3_free_table(papszResult);
    CPLHashSetDestroy(hSet);

    if( nOpenFlags & GDAL_OF_RASTER )
    {
        bool bRet = OpenRaster();
        if( !bRet && !(nOpenFlags & GDAL_OF_VECTOR))
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                          OpenVirtualTable()                          */
/************************************************************************/

int OGRSQLiteDataSource::OpenVirtualTable(const char* pszName, const char* pszSQL)
{
    int nSRID = nUndefinedSRID;
    const char* pszVirtualShape = strstr(pszSQL, "VirtualShape");
    if (pszVirtualShape != NULL)
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

    if (OpenTable(pszName, pszVirtualShape != NULL))
    {
        OGRSQLiteLayer* poLayer = papoLayers[nLayers-1];
        if( poLayer->GetLayerDefn()->GetGeomFieldCount() == 1 )
        {
            OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                    poLayer->myGetLayerDefn()->myGetGeomFieldDefn(0);
            poGeomFieldDefn->eGeomFormat = OSGF_SpatiaLite;
            if( nSRID > 0 )
            {
                poGeomFieldDefn->nSRSId = nSRID;
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
        return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRSQLiteDataSource::OpenTable( const char *pszTableName,
                                    int bIsVirtualShapeIn )

{
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRSQLiteTableLayer *poLayer = new OGRSQLiteTableLayer( this );
    if( poLayer->Initialize( pszTableName, bIsVirtualShapeIn, FALSE) != CE_None )
    {
        delete poLayer;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRSQLiteLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRSQLiteLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;

    return TRUE;
}

/************************************************************************/
/*                             OpenView()                               */
/************************************************************************/

int OGRSQLiteDataSource::OpenView( const char *pszViewName,
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
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRSQLiteLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRSQLiteLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return bUpdate;
    else if( EQUAL(pszCap,ODsCDeleteLayer) )
        return bUpdate;
    else if( EQUAL(pszCap,ODsCCurveGeometries) )
        return !bIsSpatiaLiteDB;
    else if( EQUAL(pszCap,ODsCMeasuredGeometries) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCCreateGeomFieldAfterCreateLayer) )
        return bUpdate;
    else if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return bUpdate;
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
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/

OGRLayer *OGRSQLiteDataSource::GetLayerByName( const char* pszLayerName )

{
    OGRLayer* poLayer = GDALDataset::GetLayerByName(pszLayerName);
    if( poLayer != NULL )
        return poLayer;

    for( size_t i=0; i<apoInvisibleLayers.size(); ++i)
    {
        if( EQUAL(apoInvisibleLayers[i]->GetName(), pszLayerName) )
            return apoInvisibleLayers[i];
    }

    if( !OpenTable(pszLayerName) )
        return NULL;

    poLayer = papoLayers[nLayers-1];
    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    poLayer->GetLayerDefn();
    CPLPopErrorHandler();
    if( CPLGetLastErrorType() != 0 )
    {
        CPLErrorReset();
        delete poLayer;
        nLayers --;
        return NULL;
    }

    return poLayer;
}

/************************************************************************/
/*                    GetLayerByNameNotVisible()                        */
/************************************************************************/

OGRLayer *OGRSQLiteDataSource::GetLayerByNameNotVisible( const char* pszLayerName )

{
    {
        OGRLayer* poLayer = GDALDataset::GetLayerByName(pszLayerName);
        if( poLayer != NULL )
            return poLayer;
    }

    for( size_t i=0; i<apoInvisibleLayers.size(); ++i)
    {
        if( EQUAL(apoInvisibleLayers[i]->GetName(), pszLayerName) )
            return apoInvisibleLayers[i];
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRSQLiteTableLayer *poLayer = new OGRSQLiteTableLayer( this );
    if( poLayer->Initialize( pszLayerName, FALSE, FALSE) != CE_None )
    {
        delete poLayer;
        return NULL;
    }
    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    poLayer->GetLayerDefn();
    CPLPopErrorHandler();
    if( CPLGetLastErrorType() != 0 )
    {
        CPLErrorReset();
        delete poLayer;
        return NULL;
    }
    apoInvisibleLayers.push_back(poLayer);

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

void OGRSQLiteDataSource::FlushCache()
{
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( papoLayers[iLayer]->IsTableLayer() )
        {
            OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) papoLayers[iLayer];
            poLayer->RunDeferredCreationIfNecessary();
            poLayer->CreateSpatialIndexIfNecessary();
        }
    }
    GDALDataset::FlushCache();
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
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( papoLayers[iLayer]->IsTableLayer() )
        {
            OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) papoLayers[iLayer];
            poLayer->RunDeferredCreationIfNecessary();
            poLayer->CreateSpatialIndexIfNecessary();
        }
    }

    if( pszDialect != NULL && EQUAL(pszDialect,"OGRSQL") )
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
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Special case GetVSILFILE() command (used by GDAL MBTiles driver)*/
/* -------------------------------------------------------------------- */
    if (strcmp(pszSQLCommand, "GetVSILFILE()") == 0)
    {
        if (fpMainFile == NULL)
            return NULL;

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
        int bNeedRefresh = -1;
        for( int i = 0; i < nLayers; i++ )
        {
            if( papoLayers[i]->IsTableLayer() )
            {
                OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) papoLayers[i];
                if ( !(poLayer->AreStatisticsValid()) ||
                     poLayer->DoStatisticsNeedToBeFlushed())
                {
                    bNeedRefresh = FALSE;
                    break;
                }
                else if( bNeedRefresh < 0 )
                    bNeedRefresh = TRUE;
            }
        }
        if( bNeedRefresh == TRUE )
        {
            for( int i = 0; i < nLayers; i++ )
            {
                if( papoLayers[i]->IsTableLayer() )
                {
                    OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) papoLayers[i];
                    poLayer->ForceStatisticsToBeFlushed();
                }
            }
        }
    }
    else if( !STARTS_WITH_CI(pszSQLCommand, "SELECT ") && !EQUAL(pszSQLCommand, "BEGIN")
        && !EQUAL(pszSQLCommand, "COMMIT")
        && !STARTS_WITH_CI(pszSQLCommand, "CREATE TABLE ") )
    {
        for(int i = 0; i < nLayers; i++)
            papoLayers[i]->InvalidateCachedFeatureCountAndExtent();
    }

    bLastSQLCommandIsUpdateLayerStatistics =
        EQUAL(pszSQLCommand, "SELECT UpdateLayerStatistics()");

/* -------------------------------------------------------------------- */
/*      Prepare statement.                                              */
/* -------------------------------------------------------------------- */
    sqlite3_stmt *hSQLStmt = NULL;

    CPLString osSQLCommand = pszSQLCommand;

    /* This will speed-up layer creation */
    /* ORDER BY are costly to evaluate and are not necessary to establish */
    /* the layer definition. */
    int bUseStatementForGetNextFeature = TRUE;
    int bEmptyLayer = FALSE;

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
            bUseStatementForGetNextFeature = FALSE;
        }
    }

    int rc = sqlite3_prepare_v2( GetDB(), osSQLCommand.c_str(),
                              static_cast<int>(osSQLCommand.size()),
                              &hSQLStmt, NULL );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "In ExecuteSQL(): sqlite3_prepare_v2(%s):\n  %s",
                  osSQLCommand.c_str(), sqlite3_errmsg(GetDB()) );

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
                  osSQLCommand.c_str(), sqlite3_errmsg(GetDB()) );

            sqlite3_finalize( hSQLStmt );
            return NULL;
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
            return NULL;
        }

        if( !STARTS_WITH_CI(pszSQLCommand, "SELECT ") )
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
    OGRSQLiteSelectLayer *poLayer = NULL;

    CPLString osSQL = pszSQLCommand;
    poLayer = new OGRSQLiteSelectLayer( this, osSQL, hSQLStmt,
                                        bUseStatementForGetNextFeature, bEmptyLayer, TRUE );

    if( poSpatialFilter != NULL && poLayer->GetLayerDefn()->GetGeomFieldCount() > 0 )
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
    char *pszLayerName = NULL;
    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "New layer %s cannot be created.\n",
                  m_pszFilename, pszLayerNameIn );

        return NULL;
    }

    if ( bIsSpatiaLiteDB && eType != wkbNone )
    {
        // We need to catch this right now as AddGeometryColumn does not
        // return an error
        OGRwkbGeometryType eFType = wkbFlatten(eType);
        if( eFType > wkbGeometryCollection )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "Cannot create geometry field of type %s",
                    OGRToOGCGeomType(eType));
            return NULL;
        }
    }

    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( papoLayers[iLayer]->IsTableLayer() )
        {
            OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) papoLayers[iLayer];
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
    if( pszGeomFormat == NULL )
    {
        if ( !bIsSpatiaLiteDB )
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
        return NULL;
    }

    CPLString osGeometryName;
    const char* pszGeometryNameIn = CSLFetchNameValue( papszOptions, "GEOMETRY_NAME" );
    if( pszGeometryNameIn == NULL )
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

    if (bIsSpatiaLiteDB && !EQUAL(pszGeomFormat, "SpatiaLite") )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "FORMAT=%s not supported on a SpatiaLite enabled database.",
                  pszGeomFormat );
        CPLFree( pszLayerName );
        return NULL;
    }

    // Should not happen since a spatialite DB should be opened in
    // read-only mode if libspatialite is not loaded.
    if (bIsSpatiaLiteDB && !IsSpatialiteLoaded())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Creating layers on a SpatiaLite enabled database, "
                  "without Spatialite extensions loaded, is not supported." );
        CPLFree( pszLayerName );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?  If so, should we blow it        */
/*      away?                                                           */
/* -------------------------------------------------------------------- */
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,papoLayers[iLayer]->GetLayerDefn()->GetName()) )
        {
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != NULL
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
                return NULL;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to get the SRS Id of this spatial reference system,         */
/*      adding to the srs table if needed.                              */
/* -------------------------------------------------------------------- */
    int nSRSId = nUndefinedSRID;
    const char* pszSRID = CSLFetchNameValue(papszOptions, "SRID");

    if( pszSRID != NULL )
    {
        nSRSId = atoi(pszSRID);
        if( nSRSId > 0 )
        {
            OGRSpatialReference* poSRSFetched = FetchSRS( nSRSId );
            if( poSRSFetched == NULL )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "SRID %d will be used, but no matching SRS is defined in spatial_ref_sys",
                         nSRSId);
            }
        }
    }
    else if( poSRS != NULL )
        nSRSId = FetchSRSId( poSRS );

    bool bImmediateSpatialIndexCreation = false;
    bool bDeferredSpatialIndexCreation = false;

    const char* pszSI = CSLFetchNameValue( papszOptions, "SPATIAL_INDEX" );
    if( bHaveGeometryColumns && eType != wkbNone )
    {
        if ( pszSI != NULL && CPLTestBool(pszSI) &&
             (bIsSpatiaLiteDB || EQUAL(pszGeomFormat, "SpatiaLite")) && !IsSpatialiteLoaded() )
        {
            CPLError( CE_Warning, CPLE_OpenFailed,
                    "Cannot create a spatial index when Spatialite extensions are not loaded." );
        }

#ifdef HAVE_SPATIALITE
        /* Only if linked against SpatiaLite and the datasource was created as a SpatiaLite DB */
        if ( bIsSpatiaLiteDB && IsSpatialiteLoaded() )
#else
        if ( 0 )
#endif
        {
            if( pszSI != NULL && EQUAL(pszSI, "IMMEDIATE") )
            {
                bImmediateSpatialIndexCreation = true;
            }
            else if( pszSI == NULL || CPLTestBool(pszSI) )
            {
                bDeferredSpatialIndexCreation = true;
            }
        }
    }
    else if( bHaveGeometryColumns )
    {
#ifdef HAVE_SPATIALITE
        if( bIsSpatiaLiteDB && IsSpatialiteLoaded() && (pszSI == NULL || CPLTestBool(pszSI)) )
            bDeferredSpatialIndexCreation = true;
#endif
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRSQLiteTableLayer *poLayer = new OGRSQLiteTableLayer( this );

    poLayer->Initialize( pszLayerName, FALSE, TRUE ) ;
    poLayer->SetCreationParameters( osFIDColumnName, eType, pszGeomFormat,
                                    osGeometryName, poSRS, nSRSId );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRSQLiteLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRSQLiteLayer *) * (nLayers+1) );

    papoLayers[nLayers++] = poLayer;

    poLayer->InitFeatureCount();
    poLayer->SetLaunderFlag( CPLFetchBool(papszOptions, "LAUNDER", true) );
    if( CPLFetchBool(papszOptions, "COMPRESS_GEOM", false) )
        poLayer->SetUseCompressGeom( TRUE );
    if( bImmediateSpatialIndexCreation )
        poLayer->CreateSpatialIndex(0);
    else if( bDeferredSpatialIndexCreation )
        poLayer->SetDeferredSpatialIndexCreation( TRUE );
    poLayer->SetCompressedColumns( CSLFetchNameValue(papszOptions,"COMPRESS_COLUMNS") );

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
    if( !bUpdate )
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

    for( ; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,papoLayers[iLayer]->GetLayerDefn()->GetName()) )
            break;
    }

    if( iLayer == nLayers )
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
    if( iLayer < 0 || iLayer >= nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer %d not in legal range of 0 to %d.",
                  iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }

    CPLString osLayerName = GetLayer(iLayer)->GetName();
    CPLString osGeometryColumn = GetLayer(iLayer)->GetGeometryColumn();

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLDebug( "OGR_SQLITE", "DeleteLayer(%s)", osLayerName.c_str() );

    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof(void *) * (nLayers - iLayer - 1) );
    nLayers--;

/* -------------------------------------------------------------------- */
/*      Remove from the database.                                       */
/* -------------------------------------------------------------------- */
    CPLString osEscapedLayerName = SQLEscapeLiteral(osLayerName);
    const char* pszEscapedLayerName = osEscapedLayerName.c_str();
    const char* pszGeometryColumn = osGeometryColumn.size() ? osGeometryColumn.c_str() : NULL;

    if( SQLCommand( hDB,
                    CPLSPrintf( "DROP TABLE '%s'", pszEscapedLayerName ) )
                                                            != OGRERR_NONE )
    {
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Drop from geometry_columns table.                               */
/* -------------------------------------------------------------------- */
    if( bHaveGeometryColumns )
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
        if( bIsSpatiaLiteDB && pszGeometryColumn )
        {
            osCommand.Printf( "DROP TABLE 'idx_%s_%s'", pszEscapedLayerName,
                              SQLEscapeLiteral(pszGeometryColumn).c_str());
            CPL_IGNORE_RET_VAL(sqlite3_exec( hDB, osCommand, NULL, NULL, NULL ));

            osCommand.Printf( "DROP TABLE 'idx_%s_%s_node'", pszEscapedLayerName,
                              SQLEscapeLiteral(pszGeometryColumn).c_str());
            CPL_IGNORE_RET_VAL(sqlite3_exec( hDB, osCommand, NULL, NULL, NULL ));

            osCommand.Printf( "DROP TABLE 'idx_%s_%s_parent'", pszEscapedLayerName,
                              SQLEscapeLiteral(pszGeometryColumn).c_str());
            CPL_IGNORE_RET_VAL(sqlite3_exec( hDB, osCommand, NULL, NULL, NULL ));

            osCommand.Printf( "DROP TABLE 'idx_%s_%s_rowid'", pszEscapedLayerName,
                              SQLEscapeLiteral(pszGeometryColumn).c_str());
            CPL_IGNORE_RET_VAL(sqlite3_exec( hDB, osCommand, NULL, NULL, NULL ));
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

    bUserTransactionActive = TRUE;
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

    bUserTransactionActive = FALSE;
    CPLAssert( nSoftTransactionLevel == 1 );
    return SoftCommitTransaction();
}

OGRErr OGRSQLiteDataSource::CommitTransaction()

{
    if( nSoftTransactionLevel == 1 )
    {
        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            if( papoLayers[iLayer]->IsTableLayer() )
            {
                OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) papoLayers[iLayer];
                poLayer->RunDeferredCreationIfNecessary();
                //poLayer->CreateSpatialIndexIfNecessary();
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

    bUserTransactionActive = FALSE;
    CPLAssert( nSoftTransactionLevel == 1 );
    return SoftRollbackTransaction();
}

OGRErr OGRSQLiteDataSource::RollbackTransaction()

{
    if( nSoftTransactionLevel == 1 )
    {
        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            if( papoLayers[iLayer]->IsTableLayer() )
            {
                OGRSQLiteTableLayer* poLayer = (OGRSQLiteTableLayer*) papoLayers[iLayer];
                poLayer->RunDeferredCreationIfNecessary();
                poLayer->CreateSpatialIndexIfNecessary();
            }
        }

        for(int i = 0; i < nLayers; i++)
        {
            papoLayers[i]->InvalidateCachedFeatureCountAndExtent();
            papoLayers[i]->ResetReading();
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
    if( !bIsSpatiaLiteDB || bSpatialite4Layout )
        return "srtext";

    // Testing for SRS_WKT column presence.
    bool bHasSrsWkt = false;
    char **papszResult = NULL;
    int nRowCount = 0;
    int nColCount = 0;
    char *pszErrMsg = NULL;
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

    return bHasSrsWkt ? "srs_wkt" : NULL;
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
    panSRID = (int *) CPLRealloc(panSRID,sizeof(int) * (nKnownSRID+1) );
    papoSRS = (OGRSpatialReference **)
        CPLRealloc(papoSRS, sizeof(void*) * (nKnownSRID + 1) );
    panSRID[nKnownSRID] = nId;
    papoSRS[nKnownSRID] = poSRS;
    nKnownSRID++;
}

/************************************************************************/
/*                             FetchSRSId()                             */
/*                                                                      */
/*      Fetch the id corresponding to an SRS, and if not found, add     */
/*      it to the table.                                                */
/************************************************************************/

int OGRSQLiteDataSource::FetchSRSId( OGRSpatialReference * poSRS )

{
    int nSRSId = nUndefinedSRID;
    if( poSRS == NULL )
        return nSRSId;

/* -------------------------------------------------------------------- */
/*      First, we look through our SRID cache, is it there?             */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nKnownSRID; i++ )
    {
        if( papoSRS[i] == poSRS )
            return panSRID[i];
    }
    for( int i = 0; i < nKnownSRID; i++ )
    {
        if( papoSRS[i] != NULL && papoSRS[i]->IsSame(poSRS) )
            return panSRID[i];
    }

/* -------------------------------------------------------------------- */
/*      Build a copy since we may call AutoIdentifyEPSG()               */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS(*poSRS);
    poSRS = NULL;

    const char *pszAuthorityName = oSRS.GetAuthorityName(NULL);
    const char *pszAuthorityCode = NULL;

    if( pszAuthorityName == NULL || strlen(pszAuthorityName) == 0 )
    {
/* -------------------------------------------------------------------- */
/*      Try to identify an EPSG code                                    */
/* -------------------------------------------------------------------- */
        oSRS.AutoIdentifyEPSG();

        pszAuthorityName = oSRS.GetAuthorityName(NULL);
        if (pszAuthorityName != NULL && EQUAL(pszAuthorityName, "EPSG"))
        {
            pszAuthorityCode = oSRS.GetAuthorityCode(NULL);
            if ( pszAuthorityCode != NULL && strlen(pszAuthorityCode) > 0 )
            {
                /* Import 'clean' SRS */
                oSRS.importFromEPSG( atoi(pszAuthorityCode) );

                pszAuthorityName = oSRS.GetAuthorityName(NULL);
                pszAuthorityCode = oSRS.GetAuthorityCode(NULL);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Check whether the EPSG authority code is already mapped to a    */
/*      SRS ID.                                                         */
/* -------------------------------------------------------------------- */
    char *pszErrMsg = NULL;
    CPLString osCommand;
    char **papszResult = NULL;
    int nRowCount = 0;
    int nColCount = 0;

    if( pszAuthorityName != NULL && strlen(pszAuthorityName) > 0 )
    {
        pszAuthorityCode = oSRS.GetAuthorityCode(NULL);

        if ( pszAuthorityCode != NULL && strlen(pszAuthorityCode) > 0 )
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
                    /* If it's in upper case, look for lower case */
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
                nSRSId = (papszResult[1] != NULL) ? atoi(papszResult[1]) : nUndefinedSRID;
                sqlite3_free_table(papszResult);

                if( nSRSId != nUndefinedSRID)
                    AddSRIDToCache(nSRSId, new OGRSpatialReference(oSRS));

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
    char *pszWKT = NULL;

    if( oSRS.exportToWkt( &pszWKT ) != OGRERR_NONE )
    {
        CPLFree(pszWKT);
        return nUndefinedSRID;
    }

    osWKT = pszWKT;
    CPLFree( pszWKT );
    pszWKT = NULL;

    const char* pszSRTEXTColName = GetSRTEXTColName();

    if ( pszSRTEXTColName != NULL )
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
        char    *pszProj4 = NULL;

        if( oSRS.exportToProj4( &pszProj4 ) != OGRERR_NONE )
        {
            CPLFree(pszProj4);
            return nUndefinedSRID;
        }

        osProj4 = pszProj4;
        CPLFree( pszProj4 );
        pszProj4 = NULL;

/* -------------------------------------------------------------------- */
/*      Try to find based on the PROJ.4 match.                          */
/* -------------------------------------------------------------------- */
        osCommand.Printf(
            "SELECT srid FROM spatial_ref_sys WHERE proj4text = ? LIMIT 2");
    }

    sqlite3_stmt *hSelectStmt = NULL;
    int rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hSelectStmt, NULL );

    if( rc == SQLITE_OK)
        rc = sqlite3_bind_text( hSelectStmt, 1,
                                ( pszSRTEXTColName != NULL ) ? osWKT.c_str() : osProj4.c_str(),
                                -1, SQLITE_STATIC );

    if( rc == SQLITE_OK)
        rc = sqlite3_step( hSelectStmt );

    if (rc == SQLITE_ROW)
    {
        if (sqlite3_column_type( hSelectStmt, 0 ) == SQLITE_INTEGER)
            nSRSId = sqlite3_column_int( hSelectStmt, 0 );
        else
            nSRSId = nUndefinedSRID;

        sqlite3_finalize( hSelectStmt );

        if( nSRSId != nUndefinedSRID)
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
        return nUndefinedSRID;
    }

    sqlite3_finalize( hSelectStmt );

/* -------------------------------------------------------------------- */
/*      Translate SRS to PROJ.4 string (if not already done)            */
/* -------------------------------------------------------------------- */
    if( osProj4.empty() )
    {
        char* pszProj4 = NULL;
        if( oSRS.exportToProj4( &pszProj4 ) == OGRERR_NONE )
        {
            osProj4 = pszProj4;
        }
        CPLFree( pszProj4 );
        pszProj4 = NULL;
    }

/* -------------------------------------------------------------------- */
/*      If we have an authority code try to assign SRS ID the same      */
/*      as that code.                                                   */
/* -------------------------------------------------------------------- */
    if ( pszAuthorityCode != NULL && strlen(pszAuthorityCode) > 0 )
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
                nSRSId = nUndefinedSRID;
                if( bIsSpatiaLiteDB )
                    pszAuthorityName = NULL;
            }
        }
        sqlite3_free_table(papszResult);
    }

/* -------------------------------------------------------------------- */
/*      Otherwise get the current maximum srid in the srs table.        */
/* -------------------------------------------------------------------- */
    if ( nSRSId == nUndefinedSRID )
    {
        rc = sqlite3_get_table( hDB, "SELECT MAX(srid) FROM spatial_ref_sys",
                                &papszResult, &nRowCount, &nColCount,
                                &pszErrMsg );

        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "SELECT of the maximum SRS ID failed: %s", pszErrMsg );
            sqlite3_free( pszErrMsg );
            return nUndefinedSRID;
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

    const char* apszToInsert[] = { NULL, NULL, NULL, NULL, NULL, NULL };

    if ( !bIsSpatiaLiteDB )
    {
        if( pszAuthorityName != NULL )
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
        if( pszSRTEXTColName != NULL )
            osSRTEXTColNameWithCommaBefore.Printf(", %s", pszSRTEXTColName);

        const char  *pszProjCS = oSRS.GetAttrValue("PROJCS");
        if (pszProjCS == NULL)
            pszProjCS = oSRS.GetAttrValue("GEOGCS");

        if( pszAuthorityName != NULL )
        {
            if ( pszProjCS )
            {
                osCommand.Printf(
                    "INSERT INTO spatial_ref_sys "
                    "(srid, auth_name, auth_srid, ref_sys_name, proj4text%s) "
                    "VALUES (%d, ?, ?, ?, ?%s)",
                    (pszSRTEXTColName != NULL) ? osSRTEXTColNameWithCommaBefore.c_str() : "",
                    nSRSId,
                    (pszSRTEXTColName != NULL) ? ", ?" : "");
                apszToInsert[0] = pszAuthorityName;
                apszToInsert[1] = pszAuthorityCode;
                apszToInsert[2] = pszProjCS;
                apszToInsert[3] = osProj4.c_str();
                apszToInsert[4] = (pszSRTEXTColName != NULL) ? osWKT.c_str() : NULL;
            }
            else
            {
                osCommand.Printf(
                    "INSERT INTO spatial_ref_sys "
                    "(srid, auth_name, auth_srid, proj4text%s) "
                    "VALUES (%d, ?, ?, ?%s)",
                    (pszSRTEXTColName != NULL) ? osSRTEXTColNameWithCommaBefore.c_str() : "",
                    nSRSId,
                    (pszSRTEXTColName != NULL) ? ", ?" : "");
                apszToInsert[0] = pszAuthorityName;
                apszToInsert[1] = pszAuthorityCode;
                apszToInsert[2] = osProj4.c_str();
                apszToInsert[3] = (pszSRTEXTColName != NULL) ? osWKT.c_str() : NULL;
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
                    (pszSRTEXTColName != NULL) ? osSRTEXTColNameWithCommaBefore.c_str() : "",
                    nSRSId, nSRSId,
                    (pszSRTEXTColName != NULL) ? ", ?" : "");
                apszToInsert[0] = pszProjCS;
                apszToInsert[1] = osProj4.c_str();
                apszToInsert[2] = (pszSRTEXTColName != NULL) ? osWKT.c_str() : NULL;
            }
            else
            {
                osCommand.Printf(
                    "INSERT INTO spatial_ref_sys "
                    "(srid, auth_name, auth_srid, proj4text%s) VALUES (%d, 'OGR', %d, ?%s)",
                    (pszSRTEXTColName != NULL) ? osSRTEXTColNameWithCommaBefore.c_str() : "",
                    nSRSId, nSRSId,
                    (pszSRTEXTColName != NULL) ? ", ?" : "");
                apszToInsert[0] = osProj4.c_str();
                apszToInsert[1] = (pszSRTEXTColName != NULL) ? osWKT.c_str() : NULL;
            }
        }
    }

    sqlite3_stmt *hInsertStmt = NULL;
    rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hInsertStmt, NULL );

    for( int i = 0; apszToInsert[i] != NULL; i++ )
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

    if( nSRSId != nUndefinedSRID)
        AddSRIDToCache(nSRSId, new OGRSpatialReference(oSRS));

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
        return NULL;

/* -------------------------------------------------------------------- */
/*      First, we look through our SRID cache, is it there?             */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nKnownSRID; i++ )
    {
        if( panSRID[i] == nId )
            return papoSRS[i];
    }

/* -------------------------------------------------------------------- */
/*      Try looking up in spatial_ref_sys table.                        */
/* -------------------------------------------------------------------- */
    char *pszErrMsg = NULL;
    char **papszResult = NULL;
    int nRowCount = 0;
    int nColCount = 0;
    OGRSpatialReference *poSRS = NULL;

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
            return NULL;
        }

        char** papszRow = papszResult + nColCount;
        if (papszRow[0] != NULL)
        {
            CPLString osWKT = papszRow[0];

/* -------------------------------------------------------------------- */
/*      Translate into a spatial reference.                             */
/* -------------------------------------------------------------------- */
            char *pszWKT = (char *) osWKT.c_str();

            poSRS = new OGRSpatialReference();
            if( poSRS->importFromWkt( &pszWKT ) != OGRERR_NONE )
            {
                delete poSRS;
                poSRS = NULL;
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
        pszErrMsg = NULL;

        const char* pszSRTEXTColName = GetSRTEXTColName();
        CPLString osSRTEXTColNameWithCommaBefore;
        if( pszSRTEXTColName != NULL )
            osSRTEXTColNameWithCommaBefore.Printf(", %s", pszSRTEXTColName);

        osCommand.Printf(
            "SELECT proj4text, auth_name, auth_srid%s FROM spatial_ref_sys "
            "WHERE srid = %d LIMIT 2",
            (pszSRTEXTColName != NULL) ? osSRTEXTColNameWithCommaBefore.c_str() : "", nId );
        rc = sqlite3_get_table( hDB, osCommand,
                                &papszResult, &nRowCount,
                                &nColCount, &pszErrMsg );
        if ( rc == SQLITE_OK )
        {
            if( nRowCount < 1 )
            {
                sqlite3_free_table(papszResult);
                return NULL;
            }

/* -------------------------------------------------------------------- */
/*      Translate into a spatial reference.                             */
/* -------------------------------------------------------------------- */
            char** papszRow = papszResult + nColCount;

            const char* pszProj4Text = papszRow[0];
            const char* pszAuthName = papszRow[1];
            int nAuthSRID = (papszRow[2] != NULL) ? atoi(papszRow[2]) : 0;
            char* pszWKT = (pszSRTEXTColName != NULL) ? (char*) papszRow[3] : NULL;

            poSRS = new OGRSpatialReference();

            /* Try first from EPSG code */
            if (pszAuthName != NULL &&
                EQUAL(pszAuthName, "EPSG") &&
                poSRS->importFromEPSG( nAuthSRID ) == OGRERR_NONE)
            {
                /* Do nothing */
            }
            /* Then from WKT string */
            else if( pszWKT != NULL &&
                     poSRS->importFromWkt( &pszWKT ) == OGRERR_NONE )
            {
                /* Do nothing */
            }
            /* Finally from Proj4 string */
            else if( pszProj4Text != NULL &&
                     poSRS->importFromProj4( pszProj4Text ) == OGRERR_NONE )
            {
                /* Do nothing */
            }
            else
            {
                delete poSRS;
                poSRS = NULL;
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
            return NULL;
        }
    }

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
        return NULL;
}

/************************************************************************/
/*                         SetEnvelopeForSQL()                          */
/************************************************************************/

void OGRSQLiteBaseDataSource::SetEnvelopeForSQL(const CPLString& osSQL,
                                            const OGREnvelope& oEnvelope)
{
    oMapSQLEnvelope[osSQL] = oEnvelope;
}
