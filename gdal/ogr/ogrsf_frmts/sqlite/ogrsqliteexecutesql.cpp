/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Run SQL requests with SQLite SQL engine
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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
#include "ogr_api.h"
#include "ogrsqlitevirtualogr.h"
#include "ogrsqliteexecutesql.h"
#include "cpl_multiproc.h"

#ifdef HAVE_SQLITE_VFS

/************************************************************************/
/*                       OGRSQLiteExecuteSQLLayer                       */
/************************************************************************/

class OGRSQLiteExecuteSQLLayer: public OGRSQLiteSelectLayer
{
        OGR2SQLITEModule *poModule;
        char             *pszTmpDBName;

    public:
        OGRSQLiteExecuteSQLLayer(OGR2SQLITEModule* poModule,
                                 char* pszTmpDBName,
                                 OGRSQLiteDataSource* poDS,
                                 CPLString osSQL,
                                 sqlite3_stmt * hStmt,
                                 int bUseStatementForGetNextFeature,
                                 int bEmptyLayer );
        virtual ~OGRSQLiteExecuteSQLLayer();
};

/************************************************************************/
/*                         OGRSQLiteExecuteSQLLayer()                   */
/************************************************************************/

OGRSQLiteExecuteSQLLayer::OGRSQLiteExecuteSQLLayer(OGR2SQLITEModule* poModule,
                                                   char* pszTmpDBName,
                                                   OGRSQLiteDataSource* poDS,
                                                   CPLString osSQL,
                                                   sqlite3_stmt * hStmt,
                                                   int bUseStatementForGetNextFeature,
                                                   int bEmptyLayer ) :

                               OGRSQLiteSelectLayer(poDS, osSQL, hStmt,
                                                    bUseStatementForGetNextFeature,
                                                    bEmptyLayer)
{
    this->poModule = poModule;
    this->pszTmpDBName = pszTmpDBName;
}

/************************************************************************/
/*                        ~OGRSQLiteExecuteSQLLayer()                   */
/************************************************************************/

OGRSQLiteExecuteSQLLayer::~OGRSQLiteExecuteSQLLayer()
{
    /* This is a bit peculiar: we must "finalize" the OGRLayer, since */
    /* it has objects that depend on the datasource, that we are just */
    /* going to destroy afterwards. The issue here is that we destroy */
    /* our own datasource ! */
    Finalize();

    delete poDS;
    delete poModule;
    VSIUnlink(pszTmpDBName);
    CPLFree(pszTmpDBName);
}

/************************************************************************/
/*                               LayerDesc                              */
/************************************************************************/

class LayerDesc
{
    public:
        LayerDesc() {};

        bool operator < ( const LayerDesc& other ) const
        {
            return osOriginalStr < other.osOriginalStr;
        }

        CPLString osOriginalStr;
        CPLString osSubstitutedName;
        CPLString osDSName;
        CPLString osLayerName;
};

/************************************************************************/
/*                       OGR2SQLITEExtractUnquotedString()              */
/************************************************************************/

static CPLString OGR2SQLITEExtractUnquotedString(const char **ppszSQLCommand)
{
    CPLString osRet;
    const char *pszSQLCommand = *ppszSQLCommand;
    char chQuoteChar = 0;

    if( *pszSQLCommand == '"' || *pszSQLCommand == '\'' )
    {
        chQuoteChar = *pszSQLCommand;
        pszSQLCommand ++;
    }

    while( *pszSQLCommand != '\0' )
    {
        if( *pszSQLCommand == chQuoteChar &&
            pszSQLCommand[1] == chQuoteChar )
        {
            pszSQLCommand ++;
            osRet += chQuoteChar;
        }
        else if( *pszSQLCommand == chQuoteChar )
        {
            pszSQLCommand ++;
            break;
        }
        else if( chQuoteChar == '\0' &&
                (isspace((int)*pszSQLCommand) ||
                 *pszSQLCommand == '.' ||
                 *pszSQLCommand == ',') )
            break;
        else
            osRet += *pszSQLCommand;

        pszSQLCommand ++;
    }

    *ppszSQLCommand = pszSQLCommand;

    return osRet;
}

/************************************************************************/
/*                      OGR2SQLITEExtractLayerDesc()                    */
/************************************************************************/

static
LayerDesc OGR2SQLITEExtractLayerDesc(const char **ppszSQLCommand)
{
    CPLString osStr;
    const char *pszSQLCommand = *ppszSQLCommand;
    LayerDesc oLayerDesc;

    while( isspace((int)*pszSQLCommand) )
        pszSQLCommand ++;

    const char* pszOriginalStrStart = pszSQLCommand;
    oLayerDesc.osOriginalStr = pszSQLCommand;

    osStr = OGR2SQLITEExtractUnquotedString(&pszSQLCommand);

    if( *pszSQLCommand == '.' )
    {
        oLayerDesc.osDSName = osStr;
        pszSQLCommand ++;
        oLayerDesc.osLayerName =
                            OGR2SQLITEExtractUnquotedString(&pszSQLCommand);
    }
    else
    {
        oLayerDesc.osLayerName = osStr;
    }
    
    oLayerDesc.osOriginalStr.resize(pszSQLCommand - pszOriginalStrStart);

    *ppszSQLCommand = pszSQLCommand;

    return oLayerDesc;
}

/************************************************************************/
/*                           OGR2SQLITEAddLayer()                       */
/************************************************************************/

static void OGR2SQLITEAddLayer( const char*& pszStart, int& nNum,
                                const char*& pszSQLCommand,
                                std::set<LayerDesc>& oSet,
                                CPLString& osModifiedSQL )
{
    CPLString osTruncated(pszStart);
    osTruncated.resize(pszSQLCommand - pszStart);
    osModifiedSQL += osTruncated;
    pszStart = pszSQLCommand;
    LayerDesc oLayerDesc = OGR2SQLITEExtractLayerDesc(&pszSQLCommand);
    int bInsert = TRUE;
    if( oLayerDesc.osDSName.size() == 0 )
    {
        osTruncated = pszStart;
        osTruncated.resize(pszSQLCommand - pszStart);
        osModifiedSQL += osTruncated; 
    }
    else
    {
        std::set<LayerDesc>::iterator oIter = oSet.find(oLayerDesc);
        if( oIter == oSet.end() )
        {
            oLayerDesc.osSubstitutedName = CPLString().Printf("_OGR_%d", nNum ++);
            osModifiedSQL += oLayerDesc.osSubstitutedName;
        }
        else
        {
            osModifiedSQL += (*oIter).osSubstitutedName;
            bInsert = FALSE;
        }
    }
    if( bInsert )
    {
        oSet.insert(oLayerDesc);
    }
    pszStart = pszSQLCommand;
}

/************************************************************************/
/*                         StartsAsSQLITEKeyWord()                      */
/************************************************************************/

static const char* apszKeywords[] =  {
    "WHERE", "GROUP", "ORDER", "JOIN", "UNION", "INTERSECT", "EXCEPT", "LIMIT"
};

static int StartsAsSQLITEKeyWord(const char* pszStr)
{
    int i;
    for(i=0;i<(int)(sizeof(apszKeywords) / sizeof(char*));i++)
    {
        if( EQUALN(pszStr, apszKeywords[i], strlen(apszKeywords[i])) )
            return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                     OGR2SQLITEGetPotentialLayerNames()               */
/************************************************************************/

static void OGR2SQLITEGetPotentialLayerNames(const char *pszSQLCommand,
                                             std::set<LayerDesc>& oSet,
                                             CPLString& osModifiedSQL)
{
    const char* pszStart = pszSQLCommand;
    int nNum = 1;
    char ch;

    while( (ch = *pszSQLCommand) != '\0' )
    {
        /* Skip literals and strings */
        if( ch == '\'' || ch == '"' )
        {
            pszSQLCommand ++;
            while( *pszSQLCommand != '\0' )
            {
                if( ch == '\'' && ch == '\'' )
                    pszSQLCommand ++;
                else if( ch == '\'' )
                {
                    pszSQLCommand ++;
                    break;
                }
                pszSQLCommand ++;
            }
        }
        else if( EQUALN(pszSQLCommand, "FROM ", strlen("FROM ")) )
        {
            pszSQLCommand += strlen("FROM ");
            OGR2SQLITEAddLayer(pszStart, nNum, pszSQLCommand, oSet, osModifiedSQL);

            while( *pszSQLCommand != '\0' )
            {
                if( isspace((int)*pszSQLCommand) )
                {
                    pszSQLCommand ++;
                    while( isspace((int)*pszSQLCommand) )
                        pszSQLCommand ++;
                    /* Skip alias */
                    if( *pszSQLCommand != '\0' &&
                        *pszSQLCommand != ',' )
                    {
                        if ( StartsAsSQLITEKeyWord(pszSQLCommand) )
                            break;
                        OGR2SQLITEExtractUnquotedString(&pszSQLCommand);
                    }
                }
                else if (*pszSQLCommand == ',' )
                {
                    pszSQLCommand ++;
                    while( isspace((int)*pszSQLCommand) )
                        pszSQLCommand ++;
                    OGR2SQLITEAddLayer(pszStart, nNum, pszSQLCommand, oSet, osModifiedSQL);
                }
                else
                    break;
            }
        }
        else if ( EQUALN(pszSQLCommand, "JOIN ", strlen("JOIN ")) )
        {
            pszSQLCommand += strlen("JOIN ");
            OGR2SQLITEAddLayer(pszStart, nNum, pszSQLCommand, oSet, osModifiedSQL);
        }
        else if( EQUALN(pszSQLCommand, "INSERT INTO ", strlen("INSERT INTO ")) )
        {
            pszSQLCommand += strlen("INSERT INTO ");
            OGR2SQLITEAddLayer(pszStart, nNum, pszSQLCommand, oSet, osModifiedSQL);
        }
        else if( EQUALN(pszSQLCommand, "UPDATE ", strlen("UPDATE ")) )
        {
            pszSQLCommand += strlen("UPDATE ");
            OGR2SQLITEAddLayer(pszStart, nNum, pszSQLCommand, oSet, osModifiedSQL);
        }
        else if ( EQUALN(pszSQLCommand, "DROP TABLE ", strlen("DROP TABLE ")) )
        {
            pszSQLCommand += strlen("DROP TABLE ");
            OGR2SQLITEAddLayer(pszStart, nNum, pszSQLCommand, oSet, osModifiedSQL);
        }
        else
            pszSQLCommand ++;
    }
    
    CPLString osTruncated(pszStart);
    osTruncated.resize(pszSQLCommand - pszStart);
    osModifiedSQL += osTruncated;
}

/************************************************************************/
/*                          OGRSQLiteExecuteSQL()                       */
/************************************************************************/

OGRLayer * OGRSQLiteExecuteSQL( OGRDataSource* poDS,
                                const char *pszStatement,
                                OGRGeometry *poSpatialFilter,
                                const char *pszDialect )
{
    char* pszTmpDBName = (char*) CPLMalloc(256);
    sprintf(pszTmpDBName, "/vsimem/ogr2sqlite/temp_%p.db", pszTmpDBName);

    OGRSQLiteDataSource* poSQLiteDS = NULL;
    int nRet;
    int bSpatialiteDB = FALSE;
    
    OGR2SQLITE_Register();

/* -------------------------------------------------------------------- */
/*      Create in-memory sqlite/spatialite DB                           */
/* -------------------------------------------------------------------- */

#ifdef HAVE_SPATIALITE

/* -------------------------------------------------------------------- */
/*      Creating an empty spatialite DB (with spatial_ref_sys populated */
/*      has a non-neglectable cost. So at the first attempt, let's make */
/*      one and cache it for later use.                                 */
/* -------------------------------------------------------------------- */
#if 1
    static vsi_l_offset nEmptyDBSize = 0;
    static GByte* pabyEmptyDB = NULL;
    {
        static void* hMutex = NULL;
        CPLMutexHolder oMutexHolder(&hMutex);
        static int bTried = FALSE;
        if( !bTried )
        {
            bTried = TRUE;
            char* pszCachedFilename = (char*) CPLMalloc(256);
            sprintf(pszCachedFilename, "/vsimem/ogr2sqlite/reference_%p.db",pszCachedFilename);
            char** papszOptions = CSLAddString(NULL, "SPATIALITE=YES");
            OGRSQLiteDataSource* poCachedDS = new OGRSQLiteDataSource();
            nRet = poCachedDS->Create( pszCachedFilename, papszOptions );
            CSLDestroy(papszOptions);
            papszOptions = NULL;
            delete poCachedDS;
            if( nRet )
                /* Note: the reference file keeps the ownership of the data, so that */
                /* it gets released with VSICleanupFileManager() */
                pabyEmptyDB = VSIGetMemFileBuffer( pszCachedFilename, &nEmptyDBSize, FALSE );
            CPLFree( pszCachedFilename );
        }
    }

    /* The following configuration option is usefull mostly for debugging/testing */
    if( pabyEmptyDB != NULL && CSLTestBoolean(CPLGetConfigOption("OGR_SQLITE_DIALECT_USE_SPATIALITE", "YES")) )
    {
        GByte* pabyEmptyDBClone = (GByte*)VSIMalloc(nEmptyDBSize);
        if( pabyEmptyDBClone == NULL )
        {
            CPLFree(pszTmpDBName);
            return NULL;
        }
        memcpy(pabyEmptyDBClone, pabyEmptyDB, nEmptyDBSize);
        VSIFCloseL(VSIFileFromMemBuffer( pszTmpDBName, pabyEmptyDBClone, nEmptyDBSize, TRUE ));

        poSQLiteDS = new OGRSQLiteDataSource();
        if( !poSQLiteDS->Open( pszTmpDBName, TRUE ) )
        {
            /* should not happen really ! */
            delete poSQLiteDS;
            VSIUnlink(pszTmpDBName);
            CPLFree(pszTmpDBName);
            return NULL;
        }
        bSpatialiteDB = TRUE;
    }
#else
    /* No caching version */
    poSQLiteDS = new OGRSQLiteDataSource();
    char** papszOptions = CSLAddString(NULL, "SPATIALITE=YES");
    nRet = poSQLiteDS->Create( pszTmpDBName, papszOptions );
    CSLDestroy(papszOptions);
    papszOptions = NULL;
    if( nRet )
    {
        bSpatialiteDB = TRUE;
    }
#endif

    else
    {
        delete poSQLiteDS;
        poSQLiteDS = NULL;
#else
    if( TRUE )
    {
#endif
        poSQLiteDS = new OGRSQLiteDataSource();
        nRet = poSQLiteDS->Create( pszTmpDBName, NULL );
        if( !nRet )
        {
            delete poSQLiteDS;
            VSIUnlink(pszTmpDBName);
            CPLFree(pszTmpDBName);
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Attach the Virtual Table OGR2SQLITE module to it.               */
/* -------------------------------------------------------------------- */
    OGR2SQLITEModule* poModule = new OGR2SQLITEModule(poDS);
    poModule->SetToDB(poSQLiteDS->GetDB());

/* -------------------------------------------------------------------- */
/*      Analysze the statement to determine which tables will be used.  */
/* -------------------------------------------------------------------- */
    std::set<LayerDesc> oSetNames;
    CPLString osModifiedSQL;
    OGR2SQLITEGetPotentialLayerNames(pszStatement, oSetNames, osModifiedSQL);
    std::set<LayerDesc>::iterator oIter = oSetNames.begin();

    if( strcmp(pszStatement, osModifiedSQL.c_str()) != 0 )
        CPLDebug("OGR", "Modified SQL: %s", osModifiedSQL.c_str());
    pszStatement = osModifiedSQL.c_str(); /* do not use it anymore */

    int bFoundOGRStyle = ( osModifiedSQL.ifind("OGR_STYLE") != std::string::npos );

/* -------------------------------------------------------------------- */
/*      For each of those tables, create a Virtual Table.               */
/* -------------------------------------------------------------------- */
    for(; oIter != oSetNames.end(); ++oIter)
    {
        const LayerDesc& oLayerDesc = *oIter;
        /*CPLDebug("OGR", "Layer desc : %s, %s, %s, %s",
                 oLayerDesc.osOriginalStr.c_str(),
                 oLayerDesc.osSubstitutedName.c_str(),
                 oLayerDesc.osDSName.c_str(),
                 oLayerDesc.osLayerName.c_str());*/

        CPLString osSQL;
        OGRLayer* poLayer = NULL;
        CPLString osTableName;
        if( oLayerDesc.osDSName.size() == 0 )
        {
            poLayer = poDS->GetLayerByName(oLayerDesc.osLayerName);
            /* Might be a false positive (unlikely) */
            if( poLayer == NULL )
                continue;
                
            osTableName = poLayer->GetName();

            osSQL.Printf("CREATE VIRTUAL TABLE \"%s\" USING VirtualOGR(-1,'%s',%d)",
                         OGRSQLiteEscapeName(osTableName).c_str(),
                         OGRSQLiteEscape(osTableName).c_str(),
                         bFoundOGRStyle);
        }
        else
        {
            OGRDataSource* poOtherDS = (OGRDataSource* )
                OGROpen(oLayerDesc.osDSName, FALSE, NULL);
            if( poOtherDS == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot open datasource '%s'",
                         oLayerDesc.osDSName.c_str() );
                delete poSQLiteDS;
                VSIUnlink(pszTmpDBName);
                CPLFree(pszTmpDBName);
                return NULL;
            }
            
            poLayer = poOtherDS->GetLayerByName(oLayerDesc.osLayerName);
            if( poLayer == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find layer '%s' in '%s'",
                         oLayerDesc.osLayerName.c_str(),
                         oLayerDesc.osDSName.c_str() );
                delete poOtherDS;
                delete poSQLiteDS;
                VSIUnlink(pszTmpDBName);
                CPLFree(pszTmpDBName);
                return NULL;
            }
            
            osTableName = oLayerDesc.osSubstitutedName;
            
            int nExtraDS = poModule->AddExtraDS(poOtherDS);

            osSQL.Printf("CREATE VIRTUAL TABLE \"%s\" USING VirtualOGR(%d,'%s',%d)",
                         OGRSQLiteEscapeName(osTableName).c_str(),
                         nExtraDS,
                         OGRSQLiteEscape(oLayerDesc.osLayerName).c_str(),
                         bFoundOGRStyle);
        }

        char* pszErrMsg = NULL;
        int rc = sqlite3_exec( poSQLiteDS->GetDB(), osSQL.c_str(),
                               NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create virtual table for layer '%s' : %s",
                     osTableName.c_str(), pszErrMsg);
            sqlite3_free(pszErrMsg);
            continue;
        }

        if( poLayer->GetGeomType() == wkbNone )
            continue;

        CPLString osGeomColRaw(OGR2SQLITE_GetNameForGeometryColumn(poLayer));
        const char* pszGeomColRaw = osGeomColRaw.c_str();

        CPLString osGeomColEscaped(OGRSQLiteEscape(pszGeomColRaw));
        const char* pszGeomColEscaped = osGeomColEscaped.c_str();

        CPLString osLayerNameEscaped(OGRSQLiteEscape(osTableName));
        const char* pszLayerNameEscaped = osLayerNameEscaped.c_str();

        /* Make sure that the SRS is injected in spatial_ref_sys */
        OGRSpatialReference* poSRS = poLayer->GetSpatialRef();
        int nSRSId = poSQLiteDS->GetUndefinedSRID();
        if( poSRS != NULL )
            nSRSId = poSQLiteDS->FetchSRSId(poSRS);

        int bCreateSpatialIndex = FALSE;
        if( !bSpatialiteDB )
        {
            osSQL.Printf("INSERT INTO geometry_columns (f_table_name, "
                        "f_geometry_column, geometry_format, geometry_type, "
                        "coord_dimension, srid) "
                        "VALUES ('%s','%s','SpatiaLite',%d,%d,%d)",
                        pszLayerNameEscaped,
                        pszGeomColEscaped,
                         (int) wkbFlatten(poLayer->GetGeomType()),
                        ( poLayer->GetGeomType() & wkb25DBit ) ? 3 : 2,
                        nSRSId);
        }
        else
        {
            bCreateSpatialIndex = poSpatialFilter != NULL;
            
            if( poSQLiteDS->HasSpatialite4Layout() )
            {
                int nGeomType = poLayer->GetGeomType();
                int nCoordDimension = 2;
                if( nGeomType & wkb25DBit )
                {
                    nGeomType += 1000;
                    nCoordDimension = 3;
                }

                osSQL.Printf("INSERT INTO geometry_columns (f_table_name, "
                            "f_geometry_column, geometry_type, coord_dimension, "
                            "srid, spatial_index_enabled) "
                            "VALUES ('%s','%s',%d ,%d ,%d, %d)",
                            pszLayerNameEscaped,
                            pszGeomColEscaped, nGeomType,
                            nCoordDimension,
                            nSRSId, bCreateSpatialIndex );
            }
            else
            {
                const char *pszGeometryType = OGRToOGCGeomType(poLayer->GetGeomType());
                if (pszGeometryType[0] == '\0')
                    pszGeometryType = "GEOMETRY";

                osSQL.Printf("INSERT INTO geometry_columns (f_table_name, "
                            "f_geometry_column, type, coord_dimension, "
                            "srid, spatial_index_enabled) "
                            "VALUES ('%s','%s','%s','%s',%d, %d)",
                            pszLayerNameEscaped,
                            pszGeomColEscaped, pszGeometryType,
                            ( poLayer->GetGeomType() & wkb25DBit ) ? "XYZ" : "XY",
                            nSRSId, bCreateSpatialIndex );
            }
        }

        sqlite3_exec( poSQLiteDS->GetDB(), osSQL.c_str(), NULL, NULL, NULL );

/* -------------------------------------------------------------------- */
/*      Should we create a spatial index ?.                             */
/* -------------------------------------------------------------------- */
        if( !bSpatialiteDB || !bCreateSpatialIndex )
            continue;

        CPLString osIdxName(OGRSQLiteEscapeName(
                CPLSPrintf("idx_%s_%s",osTableName.c_str(), pszGeomColRaw)));

        osSQL.Printf("CREATE VIRTUAL TABLE \"%s\" USING rtree(pkid, xmin, xmax, ymin, ymax)",
                     osIdxName.c_str());

        sqlite3_exec( poSQLiteDS->GetDB(), osSQL.c_str(), NULL, NULL, NULL );

        CPLString osGeomColNameEscaped(OGRSQLiteEscape(pszGeomColRaw));
        const char* pszGeomColNameEscaped = osGeomColNameEscaped.c_str();

        osSQL.Printf("INSERT INTO \"%s\" (pkid, xmin, xmax, ymin, ymax) "
                     "SELECT ROWID, MbrMinX(\"%s\"), MbrMaxX(\"%s\"), MbrMinY(\"%s\"), "
                     "MbrMaxY(\"%s\") FROM \"%s\"",
                     osIdxName.c_str(), pszGeomColNameEscaped, pszGeomColNameEscaped,
                     pszGeomColNameEscaped, pszGeomColNameEscaped, pszLayerNameEscaped);

        sqlite3_exec( poSQLiteDS->GetDB(), osSQL.c_str(), NULL, NULL, NULL );
    }

    delete poSQLiteDS;

/* -------------------------------------------------------------------- */
/*      Re-open so that virtual tables are recognized                   */
/* -------------------------------------------------------------------- */
    poSQLiteDS = new OGRSQLiteDataSource();
    if( !poSQLiteDS->Open(pszTmpDBName, TRUE) )
    {
        delete poSQLiteDS;
        VSIUnlink(pszTmpDBName);
        CPLFree(pszTmpDBName);
        return NULL;
    }
    sqlite3* hDB = poSQLiteDS->GetDB();
    poModule->SetToDB(hDB);
    poModule->SetSQLiteDS(poSQLiteDS);

/* -------------------------------------------------------------------- */
/*      Prepare the statement.                                          */
/* -------------------------------------------------------------------- */
    /* This will speed-up layer creation */
    /* ORDER BY are costly to evaluate and are not necessary to establish */
    /* the layer definition. */
    int bUseStatementForGetNextFeature = TRUE;
    int bEmptyLayer = FALSE;

    sqlite3_stmt *hSQLStmt = NULL;
    int rc = sqlite3_prepare( hDB,
                              pszStatement, strlen(pszStatement),
                              &hSQLStmt, NULL );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "In ExecuteSQL(): sqlite3_prepare(%s):\n  %s",
                pszStatement, sqlite3_errmsg(hDB) );

        if( hSQLStmt != NULL )
        {
            sqlite3_finalize( hSQLStmt );
        }

        delete poSQLiteDS;
        VSIUnlink(pszTmpDBName);
        CPLFree(pszTmpDBName);

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
                  pszStatement, sqlite3_errmsg(hDB) );

            sqlite3_finalize( hSQLStmt );

            delete poSQLiteDS;
            VSIUnlink(pszTmpDBName);
            CPLFree(pszTmpDBName);

            return NULL;
        }

        if( !EQUALN(pszStatement, "SELECT ", 7) )
        {

            sqlite3_finalize( hSQLStmt );

            delete poSQLiteDS;
            VSIUnlink(pszTmpDBName);
            CPLFree(pszTmpDBName);

            return NULL;
        }

        bUseStatementForGetNextFeature = FALSE;
        bEmptyLayer = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Create layer.                                                   */
/* -------------------------------------------------------------------- */
    OGRSQLiteSelectLayer *poLayer = NULL;

    poLayer = new OGRSQLiteExecuteSQLLayer( poModule, pszTmpDBName,
                                            poSQLiteDS, pszStatement, hSQLStmt,
                                            bUseStatementForGetNextFeature, bEmptyLayer );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );

    return poLayer;
}

#else // HAVE_SQLITE_VFS

/************************************************************************/
/*                          OGRSQLiteExecuteSQL()                       */
/************************************************************************/

OGRLayer * OGRSQLiteExecuteSQL( OGRDataSource* poDS,
                                const char *pszStatement,
                                OGRGeometry *poSpatialFilter,
                                const char *pszDialect )
{
    CPLError(CE_Failure, CPLE_NotSupported,
                "The SQLite version is to old to support the SQLite SQL dialect");
    return NULL;
}

#endif // HAVE_SQLITE_VFS
