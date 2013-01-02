/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  SQLite Virtual Table module using OGR layers
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
 
#include "ogrsqlitevirtualogr.h"
#include "ogr_api.h"
#include "swq.h"
#include <map>
#include <vector>

#ifdef HAVE_SQLITE_VFS

#define VIRTUAL_OGR_DYNAMIC_EXTENSION_ENABLED
//#define DEBUG_OGR2SQLITE

#if defined(SPATIALITE_AMALGAMATION)
#include "ogrsqlite3ext.h"
#else
#include "sqlite3ext.h"
#endif

/* Declaration of sqlite3_api structure */
SQLITE_EXTENSION_INIT1

/* The layout of fields is :
   0   : RegularField0
   ...
   n-1 : RegularField(n-1)
   n   : OGR_STYLE (may be HIDDEN)
   n+1 : GEOMETRY
*/

#define COMPILATION_ALLOWED
#include "ogrsqlitesqlfunctions.cpp" /* yes the .cpp file, to make it work on Windows with load_extension('gdalXX.dll') */
#undef COMPILATION_ALLOWED

/************************************************************************/
/*                           OGR2SQLITEModule                           */
/************************************************************************/

class OGR2SQLITEModule
{
#ifdef DEBUG
    void* pDummy; /* to track memory leaks */
#endif
    sqlite3* hDB; /* *NOT* to be freed */

    OGRDataSource* poDS; /* *NOT* to be freed */
    std::vector<OGRDataSource*> apoExtraDS; /* each datasource to be freed */

    OGRSQLiteDataSource* poSQLiteDS;  /* *NOT* to be freed, might be NULL */

    std::map< CPLString, OGRLayer* > oMapVTableToOGRLayer;

    void* hHandleSQLFunctions;

  public:
                                 OGR2SQLITEModule();
                                ~OGR2SQLITEModule();

    int                          Setup(OGRDataSource* poDS,
                                       OGRSQLiteDataSource* poSQLiteDS);
    int                          Setup(sqlite3* hDB);

    OGRDataSource*               GetDS() { return poDS; }

    int                          AddExtraDS(OGRDataSource* poDS);
    OGRDataSource               *GetExtraDS(int nIndex);

    int                          FetchSRSId(OGRSpatialReference* poSRS);

    void                         RegisterVTable(const char* pszVTableName, OGRLayer* poLayer);
    void                         UnregisterVTable(const char* pszVTableName);
    OGRLayer*                    GetLayerForVTable(const char* pszVTableName);

    void                         SetHandleSQLFunctions(void* hHandleSQLFunctionsIn);
};

/************************************************************************/
/*                        OGR2SQLITEModule()                            */
/************************************************************************/

OGR2SQLITEModule::OGR2SQLITEModule() :
    hDB(NULL), poDS(NULL), poSQLiteDS(NULL), hHandleSQLFunctions(NULL)
{
#ifdef DEBUG
    pDummy = CPLMalloc(1);
#endif
}

/************************************************************************/
/*                          ~OGR2SQLITEModule                           */
/************************************************************************/

OGR2SQLITEModule::~OGR2SQLITEModule()
{
#ifdef DEBUG
    CPLFree(pDummy);
#endif

    for(int i=0;i<(int)apoExtraDS.size();i++)
        delete apoExtraDS[i];

    OGRSQLiteUnregisterSQLFunctions(hHandleSQLFunctions);
}

/************************************************************************/
/*                        SetHandleSQLFunctions()                       */
/************************************************************************/

void OGR2SQLITEModule::SetHandleSQLFunctions(void* hHandleSQLFunctionsIn)
{
    CPLAssert(hHandleSQLFunctions == NULL);
    hHandleSQLFunctions = hHandleSQLFunctionsIn;
}

/************************************************************************/
/*                            AddExtraDS()                              */
/************************************************************************/

int OGR2SQLITEModule::AddExtraDS(OGRDataSource* poDS)
{
    int nRet = (int)apoExtraDS.size();
    apoExtraDS.push_back(poDS);
    return nRet;
}

/************************************************************************/
/*                            GetExtraDS()                              */
/************************************************************************/

OGRDataSource* OGR2SQLITEModule::GetExtraDS(int nIndex)
{
    if( nIndex < 0 || nIndex >= (int)apoExtraDS.size() )
        return NULL;
    return apoExtraDS[nIndex];
}

/************************************************************************/
/*                                Setup()                               */
/************************************************************************/

int OGR2SQLITEModule::Setup(OGRDataSource* poDSIn,
                            OGRSQLiteDataSource* poSQLiteDSIn)
{
    CPLAssert(poDS == NULL);
    CPLAssert(poSQLiteDS == NULL);
    poDS = poDSIn;
    poSQLiteDS = poSQLiteDSIn;
    return Setup(poSQLiteDS->GetDB());
}

/************************************************************************/
/*                            FetchSRSId()                              */
/************************************************************************/

int OGR2SQLITEModule::FetchSRSId(OGRSpatialReference* poSRS)
{
    int nSRSId;

    if( poSQLiteDS != NULL )
    {
        nSRSId = poSQLiteDS->GetUndefinedSRID();
        if( poSRS != NULL )
            nSRSId = poSQLiteDS->FetchSRSId(poSRS);
    }
    else
    {
        nSRSId = -1;
        if( poSRS != NULL )
        {
            const char* pszAuthorityName = poSRS->GetAuthorityName(NULL);
            if (pszAuthorityName != NULL && EQUAL(pszAuthorityName, "EPSG"))
            {
                const char* pszAuthorityCode = poSRS->GetAuthorityCode(NULL);
                if ( pszAuthorityCode != NULL && strlen(pszAuthorityCode) > 0 )
                {
                    nSRSId = atoi(pszAuthorityCode);
                }
            }
        }
    }

    return nSRSId;
}

/************************************************************************/
/*                          RegisterVTable()                            */
/************************************************************************/

void OGR2SQLITEModule::RegisterVTable(const char* pszVTableName,
                                      OGRLayer* poLayer)
{
    oMapVTableToOGRLayer[pszVTableName] = poLayer;
}

/************************************************************************/
/*                          UnregisterVTable()                          */
/************************************************************************/

void OGR2SQLITEModule::UnregisterVTable(const char* pszVTableName)
{
    oMapVTableToOGRLayer[pszVTableName] = NULL;
}

/************************************************************************/
/*                          GetLayerForVTable()                         */
/************************************************************************/

OGRLayer* OGR2SQLITEModule::GetLayerForVTable(const char* pszVTableName)
{
    std::map<CPLString, OGRLayer*>::iterator oIter =
        oMapVTableToOGRLayer.find(pszVTableName);
    if( oIter == oMapVTableToOGRLayer.end() )
        return NULL;

    OGRLayer* poLayer = oIter->second;
    if( poLayer == NULL )
    {
        /* If the associate layer is null, then try to "ping" the virtual */
        /* table since we know that we have managed to create it before */
        if( sqlite3_exec(hDB,
                     CPLSPrintf("PRAGMA table_info(\"%s\")",
                                OGRSQLiteEscapeName(pszVTableName).c_str()),
                     NULL, NULL, NULL) == SQLITE_OK )
        {
            poLayer = oMapVTableToOGRLayer[pszVTableName];
        }
    }

    return poLayer;
}

/* See http://www.sqlite.org/vtab.html for the documentation on how to
   implement a new module for the Virtual Table mechanism. */

/************************************************************************/
/*                            OGR2SQLITE_vtab                           */
/************************************************************************/

typedef struct
{
    /* Mandatory fields by sqlite3: don't change or reorder them ! */
    const sqlite3_module *pModule;
    int                   nRef;
    char                 *zErrMsg;

    /* Extension fields */
    char                 *pszVTableName;
    OGR2SQLITEModule     *poModule;
    OGRDataSource        *poDS;
    int                   bCloseDS;
    OGRLayer             *poLayer;
    int                   nMyRef;
} OGR2SQLITE_vtab;

/************************************************************************/
/*                          OGR2SQLITE_vtab_cursor                      */
/************************************************************************/

typedef struct
{
    /* Mandatory fields by sqlite3: don't change or reorder them ! */
    OGR2SQLITE_vtab *pVTab;

    /* Extension fields */
    OGRDataSource *poDupDataSource;
    OGRLayer      *poLayer;
    OGRFeature    *poFeature;

    /* nFeatureCount >= 0 if the layer has a feast feature count capability. */
    /* In which case nNextWishedIndex and nCurFeatureIndex */
    /* will be used to avoid useless GetNextFeature() */
    /* Helps in SELECT COUNT(*) FROM xxxx scenarios */
    int            nFeatureCount;
    int            nNextWishedIndex;
    int            nCurFeatureIndex;

    GByte         *pabyGeomBLOB;
    int            nGeomBLOBLen;
} OGR2SQLITE_vtab_cursor;


/************************************************************************/
/*                  OGR2SQLITE_GetNameForGeometryColumn()               */
/************************************************************************/

CPLString OGR2SQLITE_GetNameForGeometryColumn(OGRLayer* poLayer)
{
    if( poLayer->GetGeometryColumn() != NULL &&
        !EQUAL(poLayer->GetGeometryColumn(), "") )
    {
        return poLayer->GetGeometryColumn();
    }
    else
    {
        CPLString osGeomCol("GEOMETRY");
        int bTry = 2;
        while( poLayer->GetLayerDefn()->GetFieldIndex(osGeomCol) >= 0 )
        {
            osGeomCol.Printf("GEOMETRY%d", bTry++);
        }
        return osGeomCol;
    }
}

#ifdef VIRTUAL_OGR_DYNAMIC_EXTENSION_ENABLED

/************************************************************************/
/*                     OGR2SQLITEDetectSuspiciousUsage()                */
/************************************************************************/

static int OGR2SQLITEDetectSuspiciousUsage(sqlite3* hDB,
                                           const char* pszVirtualTableName,
                                           char**pzErr)
{
    char **papszResult = NULL;
    int nRowCount = 0, nColCount = 0;
    int i;

    std::vector<CPLString> aosDatabaseNames;

    /* Collect database names */
    sqlite3_get_table( hDB, "PRAGMA database_list",
                       &papszResult, &nRowCount, &nColCount, NULL );

    for(i = 1; i <= nRowCount; i++)
    {
        const char* pszUnescapedName = papszResult[i * nColCount + 1];
        aosDatabaseNames.push_back(
            CPLSPrintf("\"%s\".sqlite_master",
                       OGRSQLiteEscapeName(pszUnescapedName).c_str()));
    }

    /* Add special database (just in case, not sure it is really needed) */
    aosDatabaseNames.push_back("sqlite_temp_master");

    sqlite3_free_table(papszResult);
    papszResult = NULL;

    /* Check the triggers of each database */
    for(i = 0; i < (int)aosDatabaseNames.size(); i++ )
    {
        nRowCount = 0; nColCount = 0;

        const char* pszSQL;

        pszSQL = CPLSPrintf("SELECT name, sql FROM %s "
                            "WHERE type = 'trigger' AND ("
                            "sql LIKE '%%%s%%' OR "
                            "sql LIKE '%%\"%s\"%%' OR "
                            "sql LIKE '%%ogr_layer_%%' OR "
                            "sql LIKE '%%ogr_geocode%%' OR "
                            "sql LIKE '%%ogr_datasource_load_layers%%' OR "
                            "sql LIKE '%%ogr_GetConfigOption%%' OR "
                            "sql LIKE '%%ogr_SetConfigOption%%' )",
                            aosDatabaseNames[i].c_str(),
                            pszVirtualTableName,
                            OGRSQLiteEscapeName(pszVirtualTableName).c_str());

        sqlite3_get_table( hDB, pszSQL, &papszResult, &nRowCount, &nColCount,
                           NULL );

        sqlite3_free_table(papszResult);
        papszResult = NULL;

        if( nRowCount > 0 )
        {
            if( !CSLTestBoolean(CPLGetConfigOption("ALLOW_VIRTUAL_OGR_FROM_TRIGGER", "NO")) )
            {
                *pzErr = sqlite3_mprintf(
                    "A trigger might reference VirtualOGR table '%s'.\n"
                    "This is suspicious practice that could be used to steal data without your consent.\n"
                    "Disabling access to it unless you define the ALLOW_VIRTUAL_OGR_FROM_TRIGGER "
                    "configuration option to YES.",
                    pszVirtualTableName);
                return TRUE;
            }
        }
    }

    return FALSE;
}

#endif // VIRTUAL_OGR_DYNAMIC_EXTENSION_ENABLED

/************************************************************************/
/*                      OGR2SQLITE_ConnectCreate()                      */
/************************************************************************/

static
int OGR2SQLITE_DisconnectDestroy(sqlite3_vtab *pVTab);

static
int OGR2SQLITE_ConnectCreate(sqlite3* hDB, void *pAux,
                             int argc, const char *const*argv,
                             sqlite3_vtab **ppVTab, char**pzErr)
{
    OGR2SQLITEModule* poModule = (OGR2SQLITEModule*) pAux;
    OGRLayer* poLayer = NULL;
    OGRDataSource* poDS = NULL;
    int bExposeOGR_STYLE = FALSE;
    int bCloseDS = FALSE;
    int i;

#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "ConnectCreate(%s)", argv[2]);
#endif

    /*for(i=0;i<argc;i++)
        printf("[%d] %s\n", i, argv[i]);*/

/* -------------------------------------------------------------------- */
/*      If called from ogrexecutesql.cpp                                */
/* -------------------------------------------------------------------- */
    poDS = poModule->GetDS();
    if( poDS != NULL && argc == 6 &&
        CPLGetValueType(argv[3]) == CPL_VALUE_INTEGER )
    {
        if( argc != 6 )
        {
            *pzErr = sqlite3_mprintf(
                "Expected syntax: CREATE VIRTUAL TABLE xxx USING "
                "VirtualOGR(ds_idx, layer_name, expose_ogr_style)");
            return SQLITE_ERROR;
        }

        int nDSIndex = atoi(argv[3]);
        if( nDSIndex >= 0 )
        {
            poDS = poModule->GetExtraDS(nDSIndex);
            if( poDS == NULL )
            {
                *pzErr = sqlite3_mprintf("Invalid dataset index : %d", nDSIndex);
                return SQLITE_ERROR;
            }
        }
        CPLString osLayerName(OGRSQLiteParamsUnquote(argv[4]));

        poLayer = poDS->GetLayerByName(osLayerName);
        if( poLayer == NULL )
        {
            *pzErr = sqlite3_mprintf( "Cannot find layer '%s' in '%s'",
                                      osLayerName.c_str(), poDS->GetName() );
            return SQLITE_ERROR;
        }

        bExposeOGR_STYLE = atoi(OGRSQLiteParamsUnquote(argv[5]));
    }
#ifdef VIRTUAL_OGR_DYNAMIC_EXTENSION_ENABLED
/* -------------------------------------------------------------------- */
/*      If called from outside (OGR loaded as a sqlite3 extension)      */
/* -------------------------------------------------------------------- */
    else
    {
        if( argc < 4 || argc > 7 )
        {
            *pzErr = sqlite3_mprintf(
                "Expected syntax: CREATE VIRTUAL TABLE xxx USING "
                "VirtualOGR(datasource_name[, update_mode, [layer_name[, expose_ogr_style]]])");
            return SQLITE_ERROR;
        }

        if( OGR2SQLITEDetectSuspiciousUsage(hDB, argv[2], pzErr) )
        {
            return SQLITE_ERROR;
        }

        CPLString osDSName(OGRSQLiteParamsUnquote(argv[3]));
        CPLString osUpdate(OGRSQLiteParamsUnquote((argc >= 5) ? argv[4] : "0"));

        if( !EQUAL(osUpdate, "1") && !EQUAL(osUpdate, "0") )
        {
            *pzErr = sqlite3_mprintf(
                "update_mode parameter should be 0 or 1");
            return SQLITE_ERROR;
        }

        int bUpdate = atoi(osUpdate);

        poDS = (OGRDataSource* )OGROpenShared(osDSName, bUpdate, NULL);
        if( poDS == NULL )
        {
            *pzErr = sqlite3_mprintf( "Cannot open datasource '%s'", osDSName.c_str() );
            return SQLITE_ERROR;
        }

        CPLString osLayerName;
        if( argc >= 6 )
        {
            osLayerName = OGRSQLiteParamsUnquote(argv[5]);
            poLayer = poDS->GetLayerByName(osLayerName);
        }
        else
        {
            if( poDS->GetLayerCount() == 0 )
            {
                *pzErr = sqlite3_mprintf( "Datasource '%s' has no layers",
                                          osDSName.c_str() );
                poDS->Release();
                return SQLITE_ERROR;
            }

            if( poDS->GetLayerCount() > 1 )
            {
                *pzErr = sqlite3_mprintf( "Datasource '%s' has more than one layers, and none was explicitely selected.",
                                          osDSName.c_str() );
                poDS->Release();
                return SQLITE_ERROR;
            }

            poLayer = poDS->GetLayer(0);
        }

        if( poLayer == NULL )
        {
            *pzErr = sqlite3_mprintf( "Cannot find layer '%s' in '%s'", osLayerName.c_str(), osDSName.c_str() );
            poDS->Release();
            return SQLITE_ERROR;
        }

        if( argc == 7 )
        {
            bExposeOGR_STYLE = atoi(OGRSQLiteParamsUnquote(argv[6]));
        }
        
        bCloseDS = TRUE;
    }
#endif // VIRTUAL_OGR_DYNAMIC_EXTENSION_ENABLED
    OGR2SQLITE_vtab* vtab =
                (OGR2SQLITE_vtab*) CPLCalloc(1, sizeof(OGR2SQLITE_vtab));
    /* We dont need to fill the non-extended fields */
    vtab->pszVTableName = CPLStrdup(OGRSQLiteEscapeName(argv[2]));
    vtab->poModule = poModule;
    vtab->poDS = poDS;
    vtab->bCloseDS = bCloseDS;
    vtab->poLayer = poLayer;
    vtab->nMyRef = 0;

    poModule->RegisterVTable(vtab->pszVTableName, poLayer);

    *ppVTab = (sqlite3_vtab*) vtab;

    CPLString osSQL;
    osSQL = "CREATE TABLE ";
    osSQL += "\"";
    osSQL += OGRSQLiteEscapeName(argv[2]);
    osSQL += "\"";
    osSQL += "(";

    int bAddComma = FALSE;

    OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
    for(i=0;i<poFDefn->GetFieldCount();i++)
    {
        if( bAddComma )
            osSQL += ",";
        bAddComma = TRUE;

        OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(i);

        osSQL += "\"";
        osSQL += OGRSQLiteEscapeName(poFieldDefn->GetNameRef());
        osSQL += "\"";
        osSQL += " ";
        osSQL += OGRSQLiteFieldDefnToSQliteFieldDefn(poFieldDefn);
    }

    if( bAddComma )
        osSQL += ",";
    bAddComma = TRUE;
    osSQL += "OGR_STYLE VARCHAR";
    if( !bExposeOGR_STYLE )
     osSQL += " HIDDEN";

    if( poFDefn->GetGeomType() != wkbNone )
    {
        if( bAddComma )
            osSQL += ",";
        bAddComma = TRUE;

        osSQL += OGRSQLiteEscapeName(OGR2SQLITE_GetNameForGeometryColumn(poLayer));
        osSQL += " BLOB";
    }

    osSQL += ")";

    CPLDebug("OGR2SQLITE", "sqlite3_declare_vtab(%s)", osSQL.c_str());
    if (sqlite3_declare_vtab (hDB, osSQL.c_str()) != SQLITE_OK)
    {
        *pzErr = sqlite3_mprintf("CREATE VIRTUAL: invalid SQL statement : %s",
                                 osSQL.c_str());
        OGR2SQLITE_DisconnectDestroy((sqlite3_vtab*) vtab);
        return SQLITE_ERROR;
    }

    return SQLITE_OK;
}

/************************************************************************/
/*                        OGR2SQLITE_BestIndex()                        */
/************************************************************************/

static
int OGR2SQLITE_BestIndex(sqlite3_vtab *pVTab, sqlite3_index_info* pIndex)
{
    int i;
    OGR2SQLITE_vtab* pMyVTab = (OGR2SQLITE_vtab*) pVTab;
    OGRFeatureDefn* poFDefn = pMyVTab->poLayer->GetLayerDefn();

#ifdef DEBUG_OGR2SQLITE
    CPLString osQueryPatternUsable, osQueryPatternNotUsable;
    for (i = 0; i < pIndex->nConstraint; i++)
    {
        int iCol = pIndex->aConstraint[i].iColumn;
        const char* pszFieldName;
        if( iCol == -1 )
            pszFieldName = "FID";
        else if( iCol >= 0 && iCol < poFDefn->GetFieldCount() )
            pszFieldName = poFDefn->GetFieldDefn(iCol)->GetNameRef();
        else
            pszFieldName = "unkown_field";

        const char* pszOp;
        switch(pIndex->aConstraint[i].op)
        {
            case SQLITE_INDEX_CONSTRAINT_EQ: pszOp = " = "; break;
            case SQLITE_INDEX_CONSTRAINT_GT: pszOp = " > "; break;
            case SQLITE_INDEX_CONSTRAINT_LE: pszOp = " <= "; break;
            case SQLITE_INDEX_CONSTRAINT_LT: pszOp = " < "; break;
            case SQLITE_INDEX_CONSTRAINT_GE: pszOp = " >= "; break;
            case SQLITE_INDEX_CONSTRAINT_MATCH: pszOp = " MATCH "; break;
            default: pszOp = " (unknown op) "; break;
        }

        if (pIndex->aConstraint[i].usable)
        {
            if (osQueryPatternUsable.size()) osQueryPatternUsable += " AND ";
            osQueryPatternUsable += pszFieldName;
            osQueryPatternUsable += pszOp;
            osQueryPatternUsable += "?";
        }
        else
        {
            if (osQueryPatternNotUsable.size()) osQueryPatternNotUsable += " AND ";
            osQueryPatternNotUsable += pszFieldName;
            osQueryPatternNotUsable += pszOp;
            osQueryPatternNotUsable += "?";
        }
    }
    CPLDebug("OGR2SQLITE", "BestIndex, usable ( %s ), not usable ( %s )",
             osQueryPatternUsable.c_str(), osQueryPatternNotUsable.c_str());
#endif

    int nConstraints = 0;
    for (i = 0; i < pIndex->nConstraint; i++)
    {
        int iCol = pIndex->aConstraint[i].iColumn;
        if (pIndex->aConstraint[i].usable &&
            pIndex->aConstraint[i].op != SQLITE_INDEX_CONSTRAINT_MATCH &&
            iCol < poFDefn->GetFieldCount() &&
            (iCol < 0 || poFDefn->GetFieldDefn(iCol)->GetType() != OFTBinary))
        {
            pIndex->aConstraintUsage[i].argvIndex = nConstraints + 1;
            pIndex->aConstraintUsage[i].omit = TRUE;

            nConstraints ++;
        }
        else
        {
            pIndex->aConstraintUsage[i].argvIndex = 0;
            pIndex->aConstraintUsage[i].omit = FALSE;
        }
    }

    int* panConstraints = NULL;

    if( nConstraints )
    {
        panConstraints = (int*)
                    sqlite3_malloc( sizeof(int) * (1 + 2 * nConstraints) );
        panConstraints[0] = nConstraints;

        nConstraints = 0;

        for (i = 0; i < pIndex->nConstraint; i++)
        {
            if (pIndex->aConstraintUsage[i].omit)
            {
                panConstraints[2 * nConstraints + 1] =
                                            pIndex->aConstraint[i].iColumn;
                panConstraints[2 * nConstraints + 2] =
                                            pIndex->aConstraint[i].op;

                nConstraints ++;
            }
        }
    }

    pIndex->orderByConsumed = FALSE;
    pIndex->idxNum = 0;

    if (nConstraints != 0)
    {
        pIndex->idxStr = (char *) panConstraints;
        pIndex->needToFreeIdxStr = TRUE;
    }
    else
    {
        pIndex->idxStr = NULL;
        pIndex->needToFreeIdxStr = FALSE;
    }

    return SQLITE_OK;
}

/************************************************************************/
/*                      OGR2SQLITE_DisconnectDestroy()                  */
/************************************************************************/

static
int OGR2SQLITE_DisconnectDestroy(sqlite3_vtab *pVTab)
{
    OGR2SQLITE_vtab* pMyVTab = (OGR2SQLITE_vtab*) pVTab;

#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "DisconnectDestroy(%s)",pMyVTab->pszVTableName);
#endif

    sqlite3_free(pMyVTab->zErrMsg);
    if( pMyVTab->bCloseDS )
        pMyVTab->poDS->Release();
    pMyVTab->poModule->UnregisterVTable(pMyVTab->pszVTableName);
    CPLFree(pMyVTab->pszVTableName);
    CPLFree(pMyVTab);

    return SQLITE_OK;
}

/************************************************************************/
/*                           OGR2SQLITE_Open()                          */
/************************************************************************/

static
int OGR2SQLITE_Open(sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor)
{
    OGR2SQLITE_vtab* pMyVTab = (OGR2SQLITE_vtab*) pVTab;
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Open(%s, %s)",
             pMyVTab->poDS->GetName(), pMyVTab->poLayer->GetName());
#endif

    OGRDataSource* poDupDataSource = NULL;
    OGRLayer* poLayer = NULL;

    if( pMyVTab->nMyRef == 0 )
    {
        poLayer = pMyVTab->poLayer;
    }
    else
    {
        poDupDataSource =
            (OGRDataSource*) OGROpen(pMyVTab->poDS->GetName(), FALSE, NULL);
        if( poDupDataSource == NULL )
            return SQLITE_ERROR;
        poLayer = poDupDataSource->GetLayerByName(
                                                pMyVTab->poLayer->GetName());
        if( poLayer == NULL )
        {
            delete poDupDataSource;
            return SQLITE_ERROR;
        }
        if( !poLayer->GetLayerDefn()->
                IsSame(pMyVTab->poLayer->GetLayerDefn()) )
        {
            delete poDupDataSource;
            return SQLITE_ERROR;
        }
    }
    pMyVTab->nMyRef ++;

    OGR2SQLITE_vtab_cursor* pCursor = (OGR2SQLITE_vtab_cursor*)
                                CPLCalloc(1, sizeof(OGR2SQLITE_vtab_cursor));
    /* We dont need to fill the non-extended fields */
    *ppCursor = (sqlite3_vtab_cursor *)pCursor;

    pCursor->poDupDataSource = poDupDataSource;
    pCursor->poLayer = poLayer;
    pCursor->poLayer->ResetReading();
    pCursor->poFeature = NULL;
    pCursor->nNextWishedIndex = 0;
    pCursor->nCurFeatureIndex = -1;
    pCursor->nFeatureCount = -1;

    pCursor->pabyGeomBLOB = NULL;
    pCursor->nGeomBLOBLen = -1;

    return SQLITE_OK;
}

/************************************************************************/
/*                           OGR2SQLITE_Close()                         */
/************************************************************************/

static
int OGR2SQLITE_Close(sqlite3_vtab_cursor* pCursor)
{
    OGR2SQLITE_vtab_cursor* pMyCursor = (OGR2SQLITE_vtab_cursor*) pCursor;
    OGR2SQLITE_vtab* pMyVTab = pMyCursor->pVTab;
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Close(%s, %s)",
             pMyVTab->poDS->GetName(), pMyVTab->poLayer->GetName());
#endif
    pMyVTab->nMyRef --;

    delete pMyCursor->poFeature;
    delete pMyCursor->poDupDataSource;

    CPLFree(pMyCursor->pabyGeomBLOB);

    CPLFree(pCursor);

    return SQLITE_OK;
}

/************************************************************************/
/*                          OGR2SQLITE_Filter()                         */
/************************************************************************/

static
int OGR2SQLITE_Filter(sqlite3_vtab_cursor* pCursor,
                      int idxNum, const char *idxStr,
                      int argc, sqlite3_value **argv)
{
    OGR2SQLITE_vtab_cursor* pMyCursor = (OGR2SQLITE_vtab_cursor*) pCursor;
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Filter");
#endif

    int* panConstraints = (int*) idxStr;
    int nConstraints = panConstraints ? panConstraints[0] : 0;

    if( nConstraints != argc )
        return SQLITE_ERROR;

    CPLString osAttributeFilter;

    OGRFeatureDefn* poFDefn = pMyCursor->poLayer->GetLayerDefn();

    int i;
    for (i = 0; i < argc; i++)
    {
        int nCol = panConstraints[2 * i + 1];
        OGRFieldDefn* poFieldDefn = NULL;
        if( nCol >= 0 )
        {
            poFieldDefn = poFDefn->GetFieldDefn(nCol);
            if( poFieldDefn == NULL )
                return SQLITE_ERROR;
        }

        if( i != 0 )
            osAttributeFilter += " AND ";

        if( poFieldDefn != NULL )
        {
            const char* pszFieldName = poFieldDefn->GetNameRef();
            char ch;
            int bNeedsQuoting = swq_is_reserved_keyword(pszFieldName);
            for(int j = 0; !bNeedsQuoting &&
                           (ch = pszFieldName[j]) != '\0'; j++ )
            {
                if (!(isalnum((int)ch) || ch == '_'))
                    bNeedsQuoting = FALSE;
            }

            if( bNeedsQuoting )
            {
                /* FIXME: we would need some virtual method */
                OGRSFDriver* poDriver = pMyCursor->pVTab->poDS->GetDriver();
                char chQuote;

                if (poDriver && (
                    EQUAL(poDriver->GetName(), "PostgreSQL") ||
                    EQUAL(poDriver->GetName(), "SQLite") ||
                    EQUAL(poDriver->GetName(), "FileGDB" )) )
                    chQuote = '"';
                else
                    chQuote = '\'';

                osAttributeFilter += chQuote;
                if( chQuote == '"' )
                    osAttributeFilter += OGRSQLiteEscapeName(pszFieldName);
                else
                    osAttributeFilter += OGRSQLiteEscape(pszFieldName);
                osAttributeFilter += chQuote;
            }
            else
            {
                osAttributeFilter += pszFieldName;
            }
        }
        else
            osAttributeFilter += "FID";

        switch(panConstraints[2 * i + 2])
        {
            case SQLITE_INDEX_CONSTRAINT_EQ: osAttributeFilter += " = "; break;
            case SQLITE_INDEX_CONSTRAINT_GT: osAttributeFilter += " > "; break;
            case SQLITE_INDEX_CONSTRAINT_LE: osAttributeFilter += " <= "; break;
            case SQLITE_INDEX_CONSTRAINT_LT: osAttributeFilter += " < "; break;
            case SQLITE_INDEX_CONSTRAINT_GE: osAttributeFilter += " >= "; break;
            default:
            {
                sqlite3_free(pMyCursor->pVTab->zErrMsg);
                pMyCursor->pVTab->zErrMsg = sqlite3_mprintf(
                                        "Unhandled constraint operator : %d",
                                        panConstraints[2 * i + 2]);
                return SQLITE_ERROR;
            }
        }

        if (sqlite3_value_type (argv[i]) == SQLITE_INTEGER)
        {
            osAttributeFilter +=
                CPLSPrintf(CPL_FRMT_GIB, sqlite3_value_int64 (argv[i]));
        }
        else if (sqlite3_value_type (argv[i]) == SQLITE_FLOAT)
        {
            osAttributeFilter +=
                CPLSPrintf("%.18g", sqlite3_value_double (argv[i]));
        }
        else if (sqlite3_value_type (argv[i]) == SQLITE_TEXT)
        {
            osAttributeFilter += "'";
            osAttributeFilter += OGRSQLiteEscape((const char*) sqlite3_value_text (argv[i]));
            osAttributeFilter += "'";
        }
        else
        {
            sqlite3_free(pMyCursor->pVTab->zErrMsg);
            pMyCursor->pVTab->zErrMsg = sqlite3_mprintf(
                                    "Unhandled constraint data type : %d",
                                    sqlite3_value_type (argv[i]));
            return SQLITE_ERROR;
        }
    }

#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Attribute filter : %s",
             osAttributeFilter.c_str());
#endif

    if( pMyCursor->poLayer->SetAttributeFilter( osAttributeFilter.size() ?
                            osAttributeFilter.c_str() : NULL) != OGRERR_NONE )
    {
        sqlite3_free(pMyCursor->pVTab->zErrMsg);
        pMyCursor->pVTab->zErrMsg = sqlite3_mprintf(
                "Cannot apply attribute filter : %s",
                osAttributeFilter.c_str());
        return SQLITE_ERROR;
    }

    if( pMyCursor->poLayer->TestCapability(OLCFastFeatureCount) )
    {
        pMyCursor->nFeatureCount = pMyCursor->poLayer->GetFeatureCount();
        pMyCursor->poLayer->ResetReading();
    }
    else
        pMyCursor->nFeatureCount = -1;

    if( pMyCursor->nFeatureCount < 0 )
    {
        pMyCursor->poFeature = pMyCursor->poLayer->GetNextFeature();
#ifdef DEBUG_OGR2SQLITE
        CPLDebug("OGR2SQLITE", "GetNextFeature() --> %d",
            pMyCursor->poFeature ? (int)pMyCursor->poFeature->GetFID() : -1);
#endif
    }

    pMyCursor->nNextWishedIndex = 0;
    pMyCursor->nCurFeatureIndex = -1;

    return SQLITE_OK;
}

/************************************************************************/
/*                          OGR2SQLITE_Next()                           */
/************************************************************************/

static
int OGR2SQLITE_Next(sqlite3_vtab_cursor* pCursor)
{
    OGR2SQLITE_vtab_cursor* pMyCursor = (OGR2SQLITE_vtab_cursor*) pCursor;
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Next");
#endif

    pMyCursor->nNextWishedIndex ++;
    if( pMyCursor->nFeatureCount < 0 )
    {
        delete pMyCursor->poFeature;
        pMyCursor->poFeature = pMyCursor->poLayer->GetNextFeature();

        CPLFree(pMyCursor->pabyGeomBLOB);
        pMyCursor->pabyGeomBLOB = NULL;
        pMyCursor->nGeomBLOBLen = -1;

#ifdef DEBUG_OGR2SQLITE
        CPLDebug("OGR2SQLITE", "GetNextFeature() --> %d",
            pMyCursor->poFeature ? (int)pMyCursor->poFeature->GetFID() : -1);
#endif
    }
    return SQLITE_OK;
}

/************************************************************************/
/*                          OGR2SQLITE_Eof()                            */
/************************************************************************/

static
int OGR2SQLITE_Eof(sqlite3_vtab_cursor* pCursor)
{
    OGR2SQLITE_vtab_cursor* pMyCursor = (OGR2SQLITE_vtab_cursor*) pCursor;
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Eof");
#endif

    if( pMyCursor->nFeatureCount < 0 )
    {
        return (pMyCursor->poFeature == NULL);
    }
    else
    {
        return ( pMyCursor->nNextWishedIndex >= pMyCursor->nFeatureCount );
    }
}

/************************************************************************/
/*                      OGR2SQLITE_GoToWishedIndex()                    */
/************************************************************************/

static void OGR2SQLITE_GoToWishedIndex(OGR2SQLITE_vtab_cursor* pMyCursor)
{
    if( pMyCursor->nFeatureCount >= 0 )
    {
        if( pMyCursor->nCurFeatureIndex < pMyCursor->nNextWishedIndex )
        {
            do
            {
                pMyCursor->nCurFeatureIndex ++;

                delete pMyCursor->poFeature;
                pMyCursor->poFeature = pMyCursor->poLayer->GetNextFeature();
#ifdef DEBUG_OGR2SQLITE
                CPLDebug("OGR2SQLITE", "GetNextFeature() --> %d",
                    pMyCursor->poFeature ? (int)pMyCursor->poFeature->GetFID() : -1);
#endif
            }
            while( pMyCursor->nCurFeatureIndex < pMyCursor->nNextWishedIndex );

            CPLFree(pMyCursor->pabyGeomBLOB);
            pMyCursor->pabyGeomBLOB = NULL;
            pMyCursor->nGeomBLOBLen = -1;
        }
    }
}

/************************************************************************/
/*                         OGR2SQLITE_Column()                          */
/************************************************************************/

static
int OGR2SQLITE_Column(sqlite3_vtab_cursor* pCursor,
                      sqlite3_context* pContext, int nCol)
{
    OGR2SQLITE_vtab_cursor* pMyCursor = (OGR2SQLITE_vtab_cursor*) pCursor;
    OGRFeature* poFeature;
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Column %d", nCol);
#endif

    OGR2SQLITE_GoToWishedIndex(pMyCursor);

    poFeature = pMyCursor->poFeature;
    if( poFeature == NULL)
        return SQLITE_ERROR;

    OGRFeatureDefn* poFDefn = pMyCursor->poLayer->GetLayerDefn();
    int nFieldCount = poFDefn->GetFieldCount();

    if( nCol == nFieldCount )
    {
        sqlite3_result_text(pContext,
                            poFeature->GetStyleString(),
                            -1, SQLITE_TRANSIENT);
        return SQLITE_OK;
    }
    else if( nCol == (nFieldCount + 1) &&
             poFDefn->GetGeomType() != wkbNone )
    {
        if( pMyCursor->nGeomBLOBLen < 0 )
        {
            OGRGeometry* poGeom = poFeature->GetGeometryRef();
            if( poGeom == NULL )
            {
                pMyCursor->nGeomBLOBLen = 0;
            }
            else
            {
                CPLAssert(pMyCursor->pabyGeomBLOB == NULL);

                OGRSpatialReference* poSRS = poGeom->getSpatialReference();
                int nSRSId = pMyCursor->pVTab->poModule->FetchSRSId(poSRS);

                if( OGRSQLiteLayer::ExportSpatiaLiteGeometry(
                        poGeom, nSRSId, wkbNDR, FALSE, FALSE, FALSE,
                        &pMyCursor->pabyGeomBLOB,
                        &pMyCursor->nGeomBLOBLen ) != CE_None )
                {
                    pMyCursor->nGeomBLOBLen = 0;
                }
            }
        }

        if( pMyCursor->nGeomBLOBLen == 0 )
        {
            sqlite3_result_null(pContext);
        }
        else
        {
            GByte *pabyGeomBLOBDup = (GByte*)
                                CPLMalloc(pMyCursor->nGeomBLOBLen);
            memcpy(pabyGeomBLOBDup,
                   pMyCursor->pabyGeomBLOB, pMyCursor->nGeomBLOBLen);
            sqlite3_result_blob(pContext, pabyGeomBLOBDup,
                                pMyCursor->nGeomBLOBLen, CPLFree);
        }

        return SQLITE_OK;
    }
    else if( nCol < 0 || nCol >= nFieldCount )
    {
        return SQLITE_ERROR;
    }
    else if( !poFeature->IsFieldSet(nCol) )
    {
        sqlite3_result_null(pContext);
        return SQLITE_OK;
    }

    switch( poFDefn->GetFieldDefn(nCol)->GetType() )
    {
        case OFTInteger:
            sqlite3_result_int(pContext,
                               poFeature->GetFieldAsInteger(nCol));
            break;

        case OFTReal:
            sqlite3_result_double(pContext,
                                  poFeature->GetFieldAsDouble(nCol));
            break;

        case OFTBinary:
        {
            int nSize;
            GByte* pBlob = poFeature->GetFieldAsBinary(nCol, &nSize);
            sqlite3_result_blob(pContext, pBlob, nSize, SQLITE_TRANSIENT);
            break;
        }

        case OFTDateTime:
        {
            int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZ;
            poFeature->GetFieldAsDateTime(nCol, &nYear, &nMonth, &nDay,
                                          &nHour, &nMinute, &nSecond, &nTZ);
            char szBuffer[64];
            sprintf(szBuffer, "%04d-%02d-%02dT%02d:%02d:%02d",
                    nYear, nMonth, nDay, nHour, nMinute, nSecond);
            sqlite3_result_text(pContext,
                                szBuffer,
                                -1, SQLITE_TRANSIENT);
            break;
        }

        case OFTDate:
        {
            int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZ;
            poFeature->GetFieldAsDateTime(nCol, &nYear, &nMonth, &nDay,
                                          &nHour, &nMinute, &nSecond, &nTZ);
            char szBuffer[64];
            sprintf(szBuffer, "%04d-%02d-%02dT", nYear, nMonth, nDay);
            sqlite3_result_text(pContext,
                                szBuffer,
                                -1, SQLITE_TRANSIENT);
            break;
        }

        case OFTTime:
        {
            int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZ;
            poFeature->GetFieldAsDateTime(nCol, &nYear, &nMonth, &nDay,
                                          &nHour, &nMinute, &nSecond, &nTZ);
            char szBuffer[64];
            sprintf(szBuffer, "%02d:%02d:%02d", nHour, nMinute, nSecond);
            sqlite3_result_text(pContext,
                                szBuffer,
                                -1, SQLITE_TRANSIENT);
            break;
        }

        default:
            sqlite3_result_text(pContext,
                                poFeature->GetFieldAsString(nCol),
                                -1, SQLITE_TRANSIENT);
            break;
    }

    return SQLITE_OK;
}

/************************************************************************/
/*                         OGR2SQLITE_Rowid()                           */
/************************************************************************/

static
int OGR2SQLITE_Rowid(sqlite3_vtab_cursor* pCursor, sqlite3_int64 *pRowid)
{
    OGR2SQLITE_vtab_cursor* pMyCursor = (OGR2SQLITE_vtab_cursor*) pCursor;
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Rowid");
#endif

    OGR2SQLITE_GoToWishedIndex(pMyCursor);

    if( pMyCursor->poFeature == NULL)
        return SQLITE_ERROR;

    *pRowid = pMyCursor->poFeature->GetFID();

    return SQLITE_OK;
}

/************************************************************************/
/*                         OGR2SQLITE_Rename()                          */
/************************************************************************/

static
int OGR2SQLITE_Rename(sqlite3_vtab *pVtab, const char *zNew)
{
    //CPLDebug("OGR2SQLITE", "Rename");
    return SQLITE_ERROR;
}

#if 0
/************************************************************************/
/*                        OGR2SQLITE_FindFunction()                     */
/************************************************************************/

static
int OGR2SQLITE_FindFunction(sqlite3_vtab *pVtab,
                            int nArg,
                            const char *zName,
                            void (**pxFunc)(sqlite3_context*,int,sqlite3_value**),
                            void **ppArg)
{
    CPLDebug("OGR2SQLITE", "FindFunction %s", zName);

    return 0;
}
#endif

/************************************************************************/
/*                     OGR2SQLITE_FeatureFromArgs()                     */
/************************************************************************/

static OGRFeature* OGR2SQLITE_FeatureFromArgs(OGRLayer* poLayer,
                                              int argc,
                                              sqlite3_value **argv)
{
    OGRFeatureDefn* poLayerDefn = poLayer->GetLayerDefn();
    int nFieldCount = poLayerDefn->GetFieldCount();
    int bHasGeomField = (poLayerDefn->GetGeomType() != wkbNone);
    if( argc != 2 + nFieldCount + 1 + bHasGeomField)
    {
        CPLDebug("OGR2SQLITE", "Did not get expect argument count : %d, %d", argc,
                    2 + nFieldCount + 1 + bHasGeomField);
        return NULL;
    }

    OGRFeature* poFeature = new OGRFeature(poLayerDefn);
    int i;
    for(i = 0; i < nFieldCount; i++)
    {
        switch( sqlite3_value_type(argv[2 + i]) )
        {
            case SQLITE_INTEGER:
                poFeature->SetField(i, sqlite3_value_int(argv[2 + i]));
                break;
            case SQLITE_FLOAT:
                poFeature->SetField(i, sqlite3_value_double(argv[2 + i]));
                break;
            case SQLITE_TEXT:
            {
                const char* pszValue = (const char*) sqlite3_value_text(argv[2 + i]);
                switch( poLayerDefn->GetFieldDefn(i)->GetType() )
                {
                    case OFTDate:
                    case OFTTime:
                    case OFTDateTime:
                    {
                        if( !OGRSQLITEStringToDateTimeField( poFeature, i, pszValue ) )
                            poFeature->SetField(i, pszValue);
                        break;
                    }

                    default:
                        poFeature->SetField(i, pszValue);
                        break;
                }
                break;
            }
            case SQLITE_BLOB:
            {
                GByte* paby = (GByte *) sqlite3_value_blob (argv[2 + i]);
                int nLen = sqlite3_value_bytes (argv[2 + i]);
                poFeature->SetField(i, nLen, paby);
                break;
            }
            default:
                break;
        }
    }

    int nStyleIdx = 2 + nFieldCount;
    if( sqlite3_value_type(argv[nStyleIdx]) == SQLITE_TEXT )
    {
        poFeature->SetStyleString((const char*) sqlite3_value_text(argv[nStyleIdx]));
    }

    if( bHasGeomField )
    {
        int nGeomFieldIdx = 2 + nFieldCount + 1;
        if( sqlite3_value_type(argv[nGeomFieldIdx]) == SQLITE_BLOB )
        {
            GByte* pabyBlob = (GByte *) sqlite3_value_blob (argv[nGeomFieldIdx]);
            int nLen = sqlite3_value_bytes (argv[nGeomFieldIdx]);
            OGRGeometry* poGeom = NULL;
            if( OGRSQLiteLayer::ImportSpatiaLiteGeometry(
                            pabyBlob, nLen, &poGeom ) == CE_None )
            {
                poFeature->SetGeometryDirectly(poGeom);
            }
        }
    }

    if( sqlite3_value_type(argv[1]) == SQLITE_INTEGER )
        poFeature->SetFID( sqlite3_value_int(argv[1]) );

    return poFeature;
}

/************************************************************************/
/*                            OGR2SQLITE_Update()                       */
/************************************************************************/

static
int OGR2SQLITE_Update(sqlite3_vtab *pVTab,
                      int argc,
                      sqlite3_value **argv,
                      sqlite_int64 *pRowid)
{
    CPLDebug("OGR2SQLITE", "OGR2SQLITE_Update");

    OGR2SQLITE_vtab* pMyVTab = (OGR2SQLITE_vtab*) pVTab;
    OGRLayer* poLayer = pMyVTab->poLayer;

    if( argc == 1 )
    {
         /* DELETE */

        OGRErr eErr = poLayer->DeleteFeature(sqlite3_value_int64(argv[0]));

        return ( eErr == OGRERR_NONE ) ? SQLITE_OK : SQLITE_ERROR;
    }
    else if( argc > 1 && sqlite3_value_type(argv[0]) == SQLITE_NULL )
    {
         /* INSERT */

        OGRFeature* poFeature = OGR2SQLITE_FeatureFromArgs(poLayer, argc, argv);
        if( poFeature == NULL )
            return SQLITE_ERROR;

        OGRErr eErr = poLayer->CreateFeature(poFeature);
        if( eErr == OGRERR_NONE )
            *pRowid = poFeature->GetFID();

        delete poFeature;

        return ( eErr == OGRERR_NONE ) ? SQLITE_OK : SQLITE_ERROR;
    }
    else if( argc > 1 && sqlite3_value_type(argv[0]) == SQLITE_INTEGER &&
             sqlite3_value_type(argv[1]) == SQLITE_INTEGER &&
             sqlite3_value_int64(argv[0]) == sqlite3_value_int64(argv[1]) )
    {
        /* UPDATE */

        OGRFeature* poFeature = OGR2SQLITE_FeatureFromArgs(poLayer, argc, argv);
        if( poFeature == NULL )
            return SQLITE_ERROR;

        OGRErr eErr = poLayer->SetFeature(poFeature);

        delete poFeature;

        return ( eErr == OGRERR_NONE ) ? SQLITE_OK : SQLITE_ERROR;
    }

    // UPDATE table SET rowid=rowid+1 WHERE ... unsupported

    return SQLITE_ERROR;
}

/************************************************************************/
/*                        sOGR2SQLITEModule                             */
/************************************************************************/

static const struct sqlite3_module sOGR2SQLITEModule =
{
    1, /* iVersion */
    OGR2SQLITE_ConnectCreate, /* xCreate */
    OGR2SQLITE_ConnectCreate, /* xConnect */
    OGR2SQLITE_BestIndex,
    OGR2SQLITE_DisconnectDestroy, /* xDisconnect */
    OGR2SQLITE_DisconnectDestroy, /* xDestroy */
    OGR2SQLITE_Open,
    OGR2SQLITE_Close,
    OGR2SQLITE_Filter,
    OGR2SQLITE_Next,
    OGR2SQLITE_Eof,
    OGR2SQLITE_Column,
    OGR2SQLITE_Rowid,
    OGR2SQLITE_Update,
    NULL, /* xBegin */
    NULL, /* xSync */
    NULL, /* xCommit */
    NULL, /* xFindFunctionRollback */
    NULL, /* xFindFunction */  // OGR2SQLITE_FindFunction;
    OGR2SQLITE_Rename
};

/************************************************************************/
/*                           OGR2SQLITE_GetLayer()                      */
/************************************************************************/

static
OGRLayer* OGR2SQLITE_GetLayer(const char* pszFuncName,
                              sqlite3_context* pContext,
                              int argc, sqlite3_value** argv)
{
    if( argc != 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s: %s(): %s",
                 "VirtualOGR",
                 pszFuncName,
                 "Invalid number of arguments");
        sqlite3_result_null (pContext);
        return NULL;
    }

    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s: %s(): %s",
                 "VirtualOGR",
                 pszFuncName,
                 "Invalid argument type");
        sqlite3_result_null (pContext);
        return NULL;
    }

    const char* pszVTableName = (const char*)sqlite3_value_text(argv[0]);

    OGR2SQLITEModule* poModule =
                    (OGR2SQLITEModule*) sqlite3_user_data(pContext);

    OGRLayer* poLayer = poModule->GetLayerForVTable(OGRSQLiteParamsUnquote(pszVTableName));
    if( poLayer == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s: %s(): %s",
                 "VirtualOGR",
                 pszFuncName,
                 "Unknown virtual table");
        sqlite3_result_null (pContext);
        return NULL;
    }

    return poLayer;
}

/************************************************************************/
/*                       OGR2SQLITE_ogr_layer_Extent()                  */
/************************************************************************/

static
void OGR2SQLITE_ogr_layer_Extent(sqlite3_context* pContext,
                                 int argc, sqlite3_value** argv)
{
    OGRLayer* poLayer = OGR2SQLITE_GetLayer("ogr_layer_Extent",
                                            pContext, argc, argv);
    if( poLayer == NULL )
        return;

    OGR2SQLITEModule* poModule =
                    (OGR2SQLITEModule*) sqlite3_user_data(pContext);

    if( poLayer->GetGeomType() == wkbNone )
    {
        sqlite3_result_null (pContext);
        return;
    }

    OGREnvelope sExtent;
    if( poLayer->GetExtent(&sExtent) != OGRERR_NONE )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s: %s(): %s",
                 "VirtualOGR",
                 "ogr_layer_Extent",
                 "Cannot fetch layer extent");
        sqlite3_result_null (pContext);
        return;
    }

    OGRPolygon oPoly;
    OGRLinearRing* poRing = new OGRLinearRing();
    oPoly.addRingDirectly(poRing);
    poRing->addPoint(sExtent.MinX, sExtent.MinY);
    poRing->addPoint(sExtent.MaxX, sExtent.MinY);
    poRing->addPoint(sExtent.MaxX, sExtent.MaxY);
    poRing->addPoint(sExtent.MinX, sExtent.MaxY);
    poRing->addPoint(sExtent.MinX, sExtent.MinY);

    GByte* pabySLBLOB = NULL;
    int nBLOBLen = 0;
    int nSRID = poModule->FetchSRSId(poLayer->GetSpatialRef());
    if( OGRSQLiteLayer::ExportSpatiaLiteGeometry(
                    &oPoly, nSRID, wkbNDR, FALSE,
                    FALSE, FALSE, &pabySLBLOB, &nBLOBLen ) == CE_None )
    {
        sqlite3_result_blob(pContext, pabySLBLOB, nBLOBLen, CPLFree);
    }
    else
    {
        sqlite3_result_null (pContext);
    }
}

/************************************************************************/
/*                       OGR2SQLITE_ogr_layer_SRID()                    */
/************************************************************************/

static
void OGR2SQLITE_ogr_layer_SRID(sqlite3_context* pContext,
                                 int argc, sqlite3_value** argv)
{
    OGRLayer* poLayer = OGR2SQLITE_GetLayer("OGR2SQLITE_ogr_layer_SRID",
                                            pContext, argc, argv);
    if( poLayer == NULL )
        return;

    OGR2SQLITEModule* poModule =
                    (OGR2SQLITEModule*) sqlite3_user_data(pContext);

    if( poLayer->GetGeomType() == wkbNone )
    {
        sqlite3_result_null (pContext);
        return;
    }

    int nSRID = poModule->FetchSRSId(poLayer->GetSpatialRef());
    sqlite3_result_int(pContext, nSRID);
}

/************************************************************************/
/*                 OGR2SQLITE_ogr_layer_GeometryType()                  */
/************************************************************************/

static
void OGR2SQLITE_ogr_layer_GeometryType(sqlite3_context* pContext,
                                 int argc, sqlite3_value** argv)
{
    OGRLayer* poLayer = OGR2SQLITE_GetLayer("OGR2SQLITE_ogr_layer_GeometryType",
                                            pContext, argc, argv);
    if( poLayer == NULL )
        return;

    OGRwkbGeometryType eType = poLayer->GetGeomType();

    if( eType == wkbNone )
    {
        sqlite3_result_null (pContext);
        return;
    }

    const char* psz2DName = OGRToOGCGeomType(eType);
    if( eType & wkb25DBit )
        sqlite3_result_text( pContext, CPLSPrintf("%s Z", psz2DName), -1, SQLITE_TRANSIENT );
    else
        sqlite3_result_text( pContext, psz2DName, -1, SQLITE_TRANSIENT );
}

/************************************************************************/
/*                OGR2SQLITE_ogr_layer_FeatureCount()                   */
/************************************************************************/

static
void OGR2SQLITE_ogr_layer_FeatureCount(sqlite3_context* pContext,
                                       int argc, sqlite3_value** argv)
{
    OGRLayer* poLayer = OGR2SQLITE_GetLayer("OGR2SQLITE_ogr_layer_FeatureCount",
                                            pContext, argc, argv);
    if( poLayer == NULL )
        return;

    sqlite3_result_int64( pContext, poLayer->GetFeatureCount() );
}

/************************************************************************/
/*                      OGR2SQLITEDestroyModule()                       */
/************************************************************************/

static void OGR2SQLITEDestroyModule(void* pData)
{
    CPLDebug("OGR", "Unloading VirtualOGR module");
    delete (OGR2SQLITEModule*) pData;
}

/* ENABLE_VIRTUAL_OGR_SPATIAL_INDEX is not defined */
#ifdef ENABLE_VIRTUAL_OGR_SPATIAL_INDEX

/************************************************************************/
/*                    OGR2SQLITESpatialIndex_vtab                       */
/************************************************************************/

typedef struct
{
    /* Mandatory fields by sqlite3: don't change or reorder them ! */
    const sqlite3_module *pModule;
    int                   nRef;
    char                 *zErrMsg;

    /* Extension fields */
    char                 *pszVTableName;
    OGR2SQLITEModule     *poModule;
    OGRDataSource        *poDS;
    int                   bCloseDS;
    OGRLayer             *poLayer;
    int                   nMyRef;
} OGR2SQLITESpatialIndex_vtab;

/************************************************************************/
/*                  OGR2SQLITESpatialIndex_vtab_cursor                  */
/************************************************************************/

typedef struct
{
    /* Mandatory fields by sqlite3: don't change or reorder them ! */
    OGR2SQLITESpatialIndex_vtab *pVTab;

    /* Extension fields */
    OGRDataSource *poDupDataSource;
    OGRLayer      *poLayer;
    OGRFeature    *poFeature;
    int            bHasSetBounds;
    double         dfMinX, dfMinY, dfMaxX, dfMaxY;
} OGR2SQLITESpatialIndex_vtab_cursor;

/************************************************************************/
/*                   OGR2SQLITESpatialIndex_ConnectCreate()             */
/************************************************************************/

static
int OGR2SQLITESpatialIndex_ConnectCreate(sqlite3* hDB, void *pAux,
                             int argc, const char *const*argv,
                             sqlite3_vtab **ppVTab, char**pzErr)
{
    OGR2SQLITEModule* poModule = (OGR2SQLITEModule*) pAux;
    OGRLayer* poLayer = NULL;
    OGRDataSource* poDS = NULL;
    int bCloseDS = FALSE;
    int i;

#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "ConnectCreate(%s)", argv[2]);
#endif

    /*for(i=0;i<argc;i++)
        printf("[%d] %s\n", i, argv[i]);*/

/* -------------------------------------------------------------------- */
/*      If called from ogrexecutesql.cpp                                */
/* -------------------------------------------------------------------- */
    poDS = poModule->GetDS();
    if( poDS == NULL )
        return SQLITE_ERROR;

    if( argc != 10 )
    {
        *pzErr = sqlite3_mprintf(
            "Expected syntax: CREATE VIRTUAL TABLE xxx USING "
            "VirtualOGRSpatialIndex(ds_idx, layer_name, pkid, xmin, xmax, ymin, ymax)");
        return SQLITE_ERROR;
    }

    int nDSIndex = atoi(argv[3]);
    if( nDSIndex >= 0 )
    {
        poDS = poModule->GetExtraDS(nDSIndex);
        if( poDS == NULL )
        {
            *pzErr = sqlite3_mprintf("Invalid dataset index : %d", nDSIndex);
            return SQLITE_ERROR;
        }
    }

    poDS = (OGRDataSource*) OGROpen( poDS->GetName(), FALSE, NULL);
    if( poDS == NULL )
    {
        return SQLITE_ERROR;
    }
    bCloseDS = TRUE;

    CPLString osLayerName(OGRSQLiteParamsUnquote(argv[4]));

    poLayer = poDS->GetLayerByName(osLayerName);
    if( poLayer == NULL )
    {
        *pzErr = sqlite3_mprintf( "Cannot find layer '%s' in '%s'",
                                    osLayerName.c_str(), poDS->GetName() );
        return SQLITE_ERROR;
    }

    OGR2SQLITESpatialIndex_vtab* vtab =
                (OGR2SQLITESpatialIndex_vtab*) CPLCalloc(1, sizeof(OGR2SQLITESpatialIndex_vtab));
    /* We dont need to fill the non-extended fields */
    vtab->pszVTableName = CPLStrdup(OGRSQLiteEscapeName(argv[2]));
    vtab->poModule = poModule;
    vtab->poDS = poDS;
    vtab->bCloseDS = bCloseDS;
    vtab->poLayer = poLayer;
    vtab->nMyRef = 0;

    *ppVTab = (sqlite3_vtab*) vtab;

    CPLString osSQL;
    osSQL = "CREATE TABLE ";
    osSQL += "\"";
    osSQL += OGRSQLiteEscapeName(argv[2]);
    osSQL += "\"";
    osSQL += "(";

    int bAddComma = FALSE;

    for(i=0;i<5;i++)
    {
        if( bAddComma )
            osSQL += ",";
        bAddComma = TRUE;

        osSQL += "\"";
        osSQL += OGRSQLiteEscapeName(OGRSQLiteParamsUnquote(argv[5+i]));
        osSQL += "\"";
        osSQL += " ";
        osSQL += (i == 0) ? "INTEGER" : "FLOAT";
    }

    osSQL += ")";

    CPLDebug("OGR2SQLITE", "sqlite3_declare_vtab(%s)", osSQL.c_str());
    if (sqlite3_declare_vtab (hDB, osSQL.c_str()) != SQLITE_OK)
    {
        *pzErr = sqlite3_mprintf("CREATE VIRTUAL: invalid SQL statement : %s",
                                 osSQL.c_str());
        return SQLITE_ERROR;
    }

    return SQLITE_OK;
}

/************************************************************************/
/*                      OGR2SQLITESpatialIndex_BestIndex()              */
/************************************************************************/

static
int OGR2SQLITESpatialIndex_BestIndex(sqlite3_vtab *pVTab, sqlite3_index_info* pIndex)
{
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "BestIndex");
#endif

    int i;

    int bMinX = FALSE, bMinY = FALSE, bMaxX = FALSE, bMaxY = FALSE;

    for (i = 0; i < pIndex->nConstraint; i++)
    {
        int iCol = pIndex->aConstraint[i].iColumn;
        /* MinX */
        if( !bMinX && iCol == 1 && pIndex->aConstraint[i].usable &&
            (pIndex->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_LE ||
                pIndex->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_LT) )
            bMinX = TRUE;
        /* MaxX */
        else if( !bMaxX && iCol == 2 && pIndex->aConstraint[i].usable &&
            (pIndex->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_GE ||
                pIndex->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_GT) )
            bMaxX = TRUE;
        /* MinY */
        else if( !bMinY && iCol == 3 && pIndex->aConstraint[i].usable &&
            (pIndex->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_LE ||
                pIndex->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_LT) )
            bMinY = TRUE;
        /* MaxY */
        else if( !bMaxY && iCol == 4 && pIndex->aConstraint[i].usable &&
            (pIndex->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_GE ||
                pIndex->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_GT) )
            bMaxY = TRUE;
        else
            break;
    }

    if( bMinX && bMinY && bMaxX && bMaxY )
    {
        CPLAssert( pIndex->nConstraint == 4 );

        int nConstraints = 0;
        for (i = 0; i < pIndex->nConstraint; i++)
        {
            pIndex->aConstraintUsage[i].argvIndex = nConstraints + 1;
            pIndex->aConstraintUsage[i].omit = TRUE;

            nConstraints ++;
        }

        int* panConstraints = (int*)
                    sqlite3_malloc( sizeof(int) * (1 + 2 * nConstraints) );
        panConstraints[0] = nConstraints;

        nConstraints = 0;

        for (i = 0; i < pIndex->nConstraint; i++)
        {
            if (pIndex->aConstraintUsage[i].omit)
            {
                panConstraints[2 * nConstraints + 1] =
                                            pIndex->aConstraint[i].iColumn;
                panConstraints[2 * nConstraints + 2] =
                                            pIndex->aConstraint[i].op;

                nConstraints ++;
            }
        }

        pIndex->idxStr = (char *) panConstraints;
        pIndex->needToFreeIdxStr = TRUE;

        pIndex->orderByConsumed = FALSE;
        pIndex->idxNum = 0;

        return SQLITE_OK;
    }
    else
    {
        CPLDebug("OGR2SQLITE", "OGR2SQLITESpatialIndex_BestIndex: unhandled request");
        return SQLITE_ERROR;
/*
        for (i = 0; i < pIndex->nConstraint; i++)
        {
            pIndex->aConstraintUsage[i].argvIndex = 0;
            pIndex->aConstraintUsage[i].omit = FALSE;
        }

        pIndex->idxStr = NULL;
        pIndex->needToFreeIdxStr = FALSE;
*/
    }
}

/************************************************************************/
/*                  OGR2SQLITESpatialIndex_DisconnectDestroy()          */
/************************************************************************/

static
int OGR2SQLITESpatialIndex_DisconnectDestroy(sqlite3_vtab *pVTab)
{
    OGR2SQLITESpatialIndex_vtab* pMyVTab = (OGR2SQLITESpatialIndex_vtab*) pVTab;

#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "DisconnectDestroy(%s)",pMyVTab->pszVTableName);
#endif

    sqlite3_free(pMyVTab->zErrMsg);
    if( pMyVTab->bCloseDS )
        delete pMyVTab->poDS;
    CPLFree(pMyVTab->pszVTableName);
    CPLFree(pMyVTab);

    return SQLITE_OK;
}

/************************************************************************/
/*                    OGR2SQLITESpatialIndex_Open()                     */
/************************************************************************/

static
int OGR2SQLITESpatialIndex_Open(sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor)
{
    OGR2SQLITESpatialIndex_vtab* pMyVTab = (OGR2SQLITESpatialIndex_vtab*) pVTab;
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Open(%s, %s)",
             pMyVTab->poDS->GetName(), pMyVTab->poLayer->GetName());
#endif

    OGRDataSource* poDupDataSource = NULL;
    OGRLayer* poLayer = NULL;

    if( pMyVTab->nMyRef == 0 )
    {
        poLayer = pMyVTab->poLayer;
    }
    else
    {
        poDupDataSource =
            (OGRDataSource*) OGROpen(pMyVTab->poDS->GetName(), FALSE, NULL);
        if( poDupDataSource == NULL )
            return SQLITE_ERROR;
        poLayer = poDupDataSource->GetLayerByName(
                                                pMyVTab->poLayer->GetName());
        if( poLayer == NULL )
        {
            delete poDupDataSource;
            return SQLITE_ERROR;
        }
        if( !poLayer->GetLayerDefn()->
                IsSame(pMyVTab->poLayer->GetLayerDefn()) )
        {
            delete poDupDataSource;
            return SQLITE_ERROR;
        }
    }
    pMyVTab->nMyRef ++;

    OGR2SQLITESpatialIndex_vtab_cursor* pCursor = (OGR2SQLITESpatialIndex_vtab_cursor*)
                                CPLCalloc(1, sizeof(OGR2SQLITESpatialIndex_vtab_cursor));
    /* We dont need to fill the non-extended fields */
    *ppCursor = (sqlite3_vtab_cursor *)pCursor;

    pCursor->poDupDataSource = poDupDataSource;
    pCursor->poLayer = poLayer;
    pCursor->poLayer->ResetReading();
    pCursor->poFeature = NULL;

    return SQLITE_OK;
}

/************************************************************************/
/*                      OGR2SQLITESpatialIndex_Close()                  */
/************************************************************************/

static
int OGR2SQLITESpatialIndex_Close(sqlite3_vtab_cursor* pCursor)
{
    OGR2SQLITESpatialIndex_vtab_cursor* pMyCursor = (OGR2SQLITESpatialIndex_vtab_cursor*) pCursor;
    OGR2SQLITESpatialIndex_vtab* pMyVTab = pMyCursor->pVTab;
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Close(%s, %s)",
             pMyVTab->poDS->GetName(), pMyVTab->poLayer->GetName());
#endif
    pMyVTab->nMyRef --;

    delete pMyCursor->poFeature;
    delete pMyCursor->poDupDataSource;

    CPLFree(pCursor);

    return SQLITE_OK;
}

/************************************************************************/
/*                     OGR2SQLITESpatialIndex_Filter()                  */
/************************************************************************/

static
int OGR2SQLITESpatialIndex_Filter(sqlite3_vtab_cursor* pCursor,
                      int idxNum, const char *idxStr,
                      int argc, sqlite3_value **argv)
{
    OGR2SQLITESpatialIndex_vtab_cursor* pMyCursor = (OGR2SQLITESpatialIndex_vtab_cursor*) pCursor;
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Filter");
#endif

    int* panConstraints = (int*) idxStr;
    int nConstraints = panConstraints ? panConstraints[0] : 0;

    if( nConstraints != argc )
        return SQLITE_ERROR;

    int i;
    double dfMinX = 0, dfMaxX = 0, dfMinY = 0, dfMaxY = 0;
    for (i = 0; i < argc; i++)
    {
        int nCol = panConstraints[2 * i + 1];
        if( nCol < 0 )
            return SQLITE_ERROR;

        double dfVal;
        if (sqlite3_value_type (argv[i]) == SQLITE_INTEGER)
            dfVal = sqlite3_value_int64 (argv[i]);
        else if (sqlite3_value_type (argv[i]) == SQLITE_FLOAT)
            dfVal = sqlite3_value_double (argv[i]);
        else
            return SQLITE_ERROR;

        if( nCol == 1 )
            dfMaxX = dfVal;
        else if( nCol == 2 )
            dfMinX = dfVal;
        else if( nCol == 3 )
            dfMaxY = dfVal;
        else if( nCol == 4 )
            dfMinY = dfVal;
        else
            return SQLITE_ERROR;
    }

#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Spatial filter : %.18g, %.18g, %.18g, %.18g",
              dfMinX, dfMinY, dfMaxX, dfMaxY);
#endif

    pMyCursor->poLayer->SetSpatialFilterRect(dfMinX, dfMinY, dfMaxX, dfMaxY);
    pMyCursor->poLayer->ResetReading();

    pMyCursor->poFeature = pMyCursor->poLayer->GetNextFeature();
    pMyCursor->bHasSetBounds = FALSE;

    return SQLITE_OK;
}

/************************************************************************/
/*                    OGR2SQLITESpatialIndex_Next()                     */
/************************************************************************/

static
int OGR2SQLITESpatialIndex_Next(sqlite3_vtab_cursor* pCursor)
{
    OGR2SQLITESpatialIndex_vtab_cursor* pMyCursor = (OGR2SQLITESpatialIndex_vtab_cursor*) pCursor;
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Next");
#endif

    delete pMyCursor->poFeature;
    pMyCursor->poFeature = pMyCursor->poLayer->GetNextFeature();
    pMyCursor->bHasSetBounds = FALSE;

    return SQLITE_OK;
}

/************************************************************************/
/*                      OGR2SQLITESpatialIndex_Eof()                    */
/************************************************************************/

static
int OGR2SQLITESpatialIndex_Eof(sqlite3_vtab_cursor* pCursor)
{
    OGR2SQLITESpatialIndex_vtab_cursor* pMyCursor = (OGR2SQLITESpatialIndex_vtab_cursor*) pCursor;
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Eof");
#endif

    return (pMyCursor->poFeature == NULL);
}

/************************************************************************/
/*                    OGR2SQLITESpatialIndex_Column()                   */
/************************************************************************/

static
int OGR2SQLITESpatialIndex_Column(sqlite3_vtab_cursor* pCursor,
                      sqlite3_context* pContext, int nCol)
{
    OGR2SQLITESpatialIndex_vtab_cursor* pMyCursor = (OGR2SQLITESpatialIndex_vtab_cursor*) pCursor;
    OGRFeature* poFeature;
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Column %d", nCol);
#endif

    poFeature = pMyCursor->poFeature;
    if( poFeature == NULL)
        return SQLITE_ERROR;

    if( nCol == 0 )
    {
        CPLDebug("OGR2SQLITE", "--> FID = %ld", poFeature->GetFID());
        sqlite3_result_int64(pContext, poFeature->GetFID());
        return SQLITE_OK;
    }

    if( !pMyCursor->bHasSetBounds )
    {
        OGRGeometry* poGeom = poFeature->GetGeometryRef();
        if( poGeom != NULL && !poGeom->IsEmpty() )
        {
            OGREnvelope sEnvelope;
            poGeom->getEnvelope(&sEnvelope);
            pMyCursor->bHasSetBounds = TRUE;
            pMyCursor->dfMinX = sEnvelope.MinX;
            pMyCursor->dfMinY = sEnvelope.MinY;
            pMyCursor->dfMaxX = sEnvelope.MaxX;
            pMyCursor->dfMaxY = sEnvelope.MaxY;
        }
    }
    if( !pMyCursor->bHasSetBounds )
    {
        sqlite3_result_null(pContext);
        return SQLITE_OK;
    }

    if( nCol == 1 )
    {
        sqlite3_result_double(pContext, pMyCursor->dfMinX);
        return SQLITE_OK;
    }
    if( nCol == 2 )
    {
        sqlite3_result_double(pContext, pMyCursor->dfMaxX);
        return SQLITE_OK;
    }
    if( nCol == 3 )
    {
        sqlite3_result_double(pContext, pMyCursor->dfMinY);
        return SQLITE_OK;
    }
    if( nCol == 4 )
    {
        sqlite3_result_double(pContext, pMyCursor->dfMaxY);
        return SQLITE_OK;
    }

    return SQLITE_ERROR;
}

/************************************************************************/
/*                    OGR2SQLITESpatialIndex_Rowid()                    */
/************************************************************************/

static
int OGR2SQLITESpatialIndex_Rowid(sqlite3_vtab_cursor* pCursor, sqlite3_int64 *pRowid)
{
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "Rowid");
#endif

    return SQLITE_ERROR;
}

/************************************************************************/
/*                   OGR2SQLITESpatialIndex_Rename()                    */
/************************************************************************/

static
int OGR2SQLITESpatialIndex_Rename(sqlite3_vtab *pVtab, const char *zNew)
{
    //CPLDebug("OGR2SQLITE", "Rename");
    return SQLITE_ERROR;
}

/************************************************************************/
/*                       sOGR2SQLITESpatialIndex                        */
/************************************************************************/

static const struct sqlite3_module sOGR2SQLITESpatialIndex =
{
    1, /* iVersion */
    OGR2SQLITESpatialIndex_ConnectCreate, /* xCreate */
    OGR2SQLITESpatialIndex_ConnectCreate, /* xConnect */
    OGR2SQLITESpatialIndex_BestIndex,
    OGR2SQLITESpatialIndex_DisconnectDestroy, /* xDisconnect */
    OGR2SQLITESpatialIndex_DisconnectDestroy, /* xDestroy */
    OGR2SQLITESpatialIndex_Open,
    OGR2SQLITESpatialIndex_Close,
    OGR2SQLITESpatialIndex_Filter,
    OGR2SQLITESpatialIndex_Next,
    OGR2SQLITESpatialIndex_Eof,
    OGR2SQLITESpatialIndex_Column,
    OGR2SQLITESpatialIndex_Rowid,
    NULL, /* xUpdate */
    NULL, /* xBegin */
    NULL, /* xSync */
    NULL, /* xCommit */
    NULL, /* xFindFunctionRollback */
    NULL, /* xFindFunction */
    OGR2SQLITESpatialIndex_Rename
};
#endif // ENABLE_VIRTUAL_OGR_SPATIAL_INDEX

/************************************************************************/
/*                              Setup()                                 */
/************************************************************************/

int OGR2SQLITEModule::Setup(sqlite3* hDB)
{
    int rc;

    this->hDB = hDB;

    rc = sqlite3_create_module_v2(hDB, "VirtualOGR", &sOGR2SQLITEModule, this,
                                  OGR2SQLITEDestroyModule);
    if( rc != SQLITE_OK )
        return FALSE;

#ifdef ENABLE_VIRTUAL_OGR_SPATIAL_INDEX
    rc = sqlite3_create_module(hDB, "VirtualOGRSpatialIndex",
                                &sOGR2SQLITESpatialIndex, this);
    if( rc != SQLITE_OK )
        return FALSE;
#endif // ENABLE_VIRTUAL_OGR_SPATIAL_INDEX

    rc= sqlite3_create_function(hDB, "ogr_layer_Extent", 1, SQLITE_ANY, this,
                                OGR2SQLITE_ogr_layer_Extent, NULL, NULL);
    if( rc != SQLITE_OK )
        return FALSE;

    rc= sqlite3_create_function(hDB, "ogr_layer_SRID", 1, SQLITE_ANY, this,
                                OGR2SQLITE_ogr_layer_SRID, NULL, NULL);
    if( rc != SQLITE_OK )
        return FALSE;

    rc= sqlite3_create_function(hDB, "ogr_layer_GeometryType", 1, SQLITE_ANY, this,
                                OGR2SQLITE_ogr_layer_GeometryType, NULL, NULL);
    if( rc != SQLITE_OK )
        return FALSE;

    rc= sqlite3_create_function(hDB, "ogr_layer_FeatureCount", 1, SQLITE_ANY, this,
                                OGR2SQLITE_ogr_layer_FeatureCount, NULL, NULL);
    if( rc != SQLITE_OK )
        return FALSE;

    SetHandleSQLFunctions(OGRSQLiteRegisterSQLFunctions(hDB));

    return TRUE;
}

/************************************************************************/
/*                        OGR2SQLITE_Setup()                            */
/************************************************************************/

OGR2SQLITEModule* OGR2SQLITE_Setup(OGRDataSource* poDS,
                                   OGRSQLiteDataSource* poSQLiteDS)
{
    OGR2SQLITEModule* poModule = new OGR2SQLITEModule();
    poModule->Setup(poDS, poSQLiteDS);
    return poModule;
}

/************************************************************************/
/*                       OGR2SQLITE_AddExtraDS()                        */
/************************************************************************/

int OGR2SQLITE_AddExtraDS(OGR2SQLITEModule* poModule, OGRDataSource* poDS)
{
    return poModule->AddExtraDS(poDS);
}

#ifdef VIRTUAL_OGR_DYNAMIC_EXTENSION_ENABLED

/************************************************************************/
/*                        sqlite3_extension_init()                      */
/************************************************************************/

CPL_C_START
int CPL_DLL sqlite3_extension_init (sqlite3 * hDB, char **pzErrMsg,
                                    const sqlite3_api_routines * pApi);
CPL_C_END

/* Entry point for dynamically loaded extension (typically called by load_extension()) */
int sqlite3_extension_init (sqlite3 * hDB, char **pzErrMsg,
                            const sqlite3_api_routines * pApi)
{
    CPLDebug("OGR", "OGR SQLite extension loading...");

    SQLITE_EXTENSION_INIT2(pApi);

    *pzErrMsg = NULL;

    OGRRegisterAll();

    OGR2SQLITEModule* poModule = new OGR2SQLITEModule();
    if( poModule->Setup(hDB) )
    {
        CPLDebug("OGR", "OGR SQLite extension loaded");
        return SQLITE_OK;
    }
    else
        return SQLITE_ERROR;
}

#endif // VIRTUAL_OGR_DYNAMIC_EXTENSION_ENABLED

/************************************************************************/
/*                        OGR2SQLITE_static_register()                  */
/************************************************************************/

CPL_C_START
int CPL_DLL OGR2SQLITE_static_register (sqlite3 * hDB, char **pzErrMsg,
                                        const sqlite3_api_routines * pApi);
CPL_C_END

int OGR2SQLITE_static_register (sqlite3 * hDB, char **pzErrMsg,
                                const sqlite3_api_routines * pApi)
{
    SQLITE_EXTENSION_INIT2 (pApi);

    *pzErrMsg = NULL;

    /* Can happen if sqlite is compiled with SQLITE_OMIT_LOAD_EXTENSION (with sqlite 3.6.10 for example) */
    if( pApi->create_module == NULL )
        return SQLITE_ERROR;

    /* The config option is turned off by ogrsqliteexecutesql.cpp that needs */
    /* to create a custom module */
    if( CSLTestBoolean(CPLGetConfigOption("OGR_SQLITE_STATIC_VIRTUAL_OGR", "YES")) )
    {
        OGR2SQLITEModule* poModule = new OGR2SQLITEModule();
        return poModule->Setup(hDB) ? SQLITE_OK : SQLITE_ERROR;
    }

    return SQLITE_OK;
}

/************************************************************************/
/*                           OGR2SQLITE_Register()                      */
/************************************************************************/

/* We call this function so that each time a db is created, */
/* OGR2SQLITE_static_register is called, to initialize the sqlite3_api */
/* structure with the right pointers. */

void OGR2SQLITE_Register()
{
    sqlite3_auto_extension ((void (*)(void)) OGR2SQLITE_static_register);
}

#endif // HAVE_SQLITE_VFS
