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

#define VIRTUAL_OGR_DYNAMIC_EXTENSION_ENABLED
//#define DEBUG_OGR2SQLITE

#if defined(SPATIALITE_AMALGAMATION)
#include "ogrsqlite3ext.h"
#else
#include "sqlite3ext.h"
#endif

/* Declaration of sqlite3_api structure */
SQLITE_EXTENSION_INIT1

#ifdef HAVE_SQLITE_VFS

/* The layout of fields is :
   0   : RegularField0
   ...
   n-1 : RegularField(n-1)
   n   : OGR_STYLE (may be HIDDEN)
   n+1 : GEOMETRY
*/

/************************************************************************/
/*                        OGR2SQLITEModule()                            */
/************************************************************************/

OGR2SQLITEModule::OGR2SQLITEModule(OGRDataSource* poDS) : poDS(poDS), poSQLiteDS(NULL)
{
}

/************************************************************************/
/*                          ~OGR2SQLITEModule                           */
/************************************************************************/

OGR2SQLITEModule::~OGR2SQLITEModule()
{
    std::map< std::pair<int,int>, OGRCoordinateTransformation*>::iterator oIter =
        oCachedTransformsMap.begin();
    for(; oIter != oCachedTransformsMap.end(); ++oIter)
        delete oIter->second;
        
    for(int i=0;i<(int)apoExtraDS.size();i++)
        delete apoExtraDS[i];
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
    if( nIndex < 0 || nIndex > (int)apoExtraDS.size() )
        return NULL;
    return apoExtraDS[nIndex];
}

/************************************************************************/
/*                            SetSQLiteDS()                             */
/************************************************************************/

void OGR2SQLITEModule::SetSQLiteDS(OGRSQLiteDataSource* poSQLiteDS)
{
    this->poSQLiteDS = poSQLiteDS;
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
/*                          GetTransform()                              */
/************************************************************************/

OGRCoordinateTransformation* OGR2SQLITEModule::GetTransform(int nSrcSRSId,
                                                            int nDstSRSId)
{
    std::map< std::pair<int,int>, OGRCoordinateTransformation*>::iterator oIter =
        oCachedTransformsMap.find(std::pair<int,int>(nSrcSRSId, nDstSRSId));
    if( oIter == oCachedTransformsMap.end() )
    {
        OGRCoordinateTransformation* poCT = NULL;
        OGRSpatialReference oSrcSRS, oDstSRS;
        if (oSrcSRS.importFromEPSG(nSrcSRSId) == OGRERR_NONE &&
            oDstSRS.importFromEPSG(nDstSRSId) == OGRERR_NONE )
        {
            poCT = OGRCreateCoordinateTransformation( &oSrcSRS, &oDstSRS );
        }
        oCachedTransformsMap[std::pair<int,int>(nSrcSRSId, nDstSRSId)] = poCT;
        return poCT;
    }
    else
        return oIter->second;
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
                            "WHERE type = 'trigger' AND (sql LIKE '%%%s%%' OR sql LIKE '%%\"%s\"%%')",
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
int OGR2SQLITE_ConnectCreate(sqlite3* hDB, void *pAux,
                             int argc, const char *const*argv,
                             sqlite3_vtab **ppVTab, char**pzErr)
{
    OGR2SQLITEModule* poModule = (OGR2SQLITEModule*) pAux;
    OGRLayer* poLayer = NULL;
    OGRDataSource* poDS = NULL;
    int bExposeOGR_STYLE = FALSE;
    int bCloseDS = FALSE;

    //CPLDebug("OGR2SQLITE", "Connect/Create");
    int i;
    /*for(i=0;i<argc;i++)
        printf("[%d] %s\n", i, argv[i]);*/

/* -------------------------------------------------------------------- */
/*      If called from ogrexecutesql.cpp                                */
/* -------------------------------------------------------------------- */
    poDS = poModule->GetDS();
    if( poDS != NULL )
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

        poDS = (OGRDataSource* )OGROpen(osDSName, bUpdate, NULL);
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
                delete poDS;
                return SQLITE_ERROR;
            }

            if( poDS->GetLayerCount() > 1 )
            {
                *pzErr = sqlite3_mprintf( "Datasource '%s' has more than one layers, and none was explicitely selected.",
                                          osDSName.c_str() );
                delete poDS;
                return SQLITE_ERROR;
            }

            poLayer = poDS->GetLayer(0);
        }

        if( poLayer == NULL )
        {
            *pzErr = sqlite3_mprintf( "Cannot find layer '%s' in '%s'", osLayerName.c_str(), osDSName.c_str() );
            delete poDS;
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
#ifdef DEBUG_OGR2SQLITE
    CPLDebug("OGR2SQLITE", "BestIndex");
#endif

    int i;

    OGR2SQLITE_vtab* pMyVTab = (OGR2SQLITE_vtab*) pVTab;
    OGRFeatureDefn* poFDefn = pMyVTab->poLayer->GetLayerDefn();

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
    //CPLDebug("OGR2SQLITE", "DisconnectDestroy");

    OGR2SQLITE_vtab* pMyVTab = (OGR2SQLITE_vtab*) pVTab;
    sqlite3_free(pMyVTab->zErrMsg);
    if( pMyVTab->bCloseDS )
        delete pMyVTab->poDS;
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

    OGR2SQLITE_vtab_cursor* pCursor = (OGR2SQLITE_vtab_cursor*)
                                CPLCalloc(1, sizeof(OGR2SQLITE_vtab_cursor));
    /* We dont need to fill the non-extended fields */
    *ppCursor = (sqlite3_vtab_cursor *)pCursor;

    if( pMyVTab->nMyRef == 0 )
    {
        pCursor->poLayer = pMyVTab->poLayer;
    }
    else
    {
        pCursor->poDupDataSource =
            (OGRDataSource*) OGROpen(pMyVTab->poDS->GetName(), FALSE, NULL);
        if( pCursor->poDupDataSource == NULL )
            return SQLITE_ERROR;
        pCursor->poLayer = pCursor->poDupDataSource->GetLayerByName(
                                                pMyVTab->poLayer->GetName());
        if( pCursor->poLayer == NULL )
        {
            delete pCursor->poDupDataSource;
            pCursor->poDupDataSource = NULL;
            return SQLITE_ERROR;
        }
        if( !pCursor->poLayer->GetLayerDefn()->
                IsSame(pMyVTab->poLayer->GetLayerDefn()) )
        {
            pCursor->poLayer = NULL;
            delete pCursor->poDupDataSource;
            pCursor->poDupDataSource = NULL;
            return SQLITE_ERROR;
        }
    }
    pMyVTab->nMyRef ++;

    pCursor->poLayer->ResetReading();
    pCursor->poFeature = NULL;
    pCursor->nNextWishedIndex = 0;
    pCursor->nCurFeatureIndex = -1;
    pCursor->nFeatureCount = -1;

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
            osAttributeFilter += poFieldDefn->GetNameRef();
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
            osAttributeFilter += (const char*) sqlite3_value_text (argv[i]);
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

    CPLDebug("OGR2SQLITE", "Attribute filter : %s",
             osAttributeFilter.c_str());
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
        while( pMyCursor->nCurFeatureIndex < pMyCursor->nNextWishedIndex )
        {
            pMyCursor->nCurFeatureIndex ++;

            delete pMyCursor->poFeature;
            pMyCursor->poFeature = pMyCursor->poLayer->GetNextFeature();
#ifdef DEBUG_OGR2SQLITE
            CPLDebug("OGR2SQLITE", "GetNextFeature() --> %d",
                pMyCursor->poFeature ? (int)pMyCursor->poFeature->GetFID() : -1);
#endif
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

    if( nCol == poFDefn->GetFieldCount() )
    {
        sqlite3_result_text(pContext,
                            poFeature->GetStyleString(),
                            -1, SQLITE_TRANSIENT);
        return SQLITE_OK;
    }
    else if( nCol == (poFDefn->GetFieldCount() + 1) &&
             poFDefn->GetGeomType() != wkbNone )
    {
        OGRGeometry* poGeom = poFeature->GetGeometryRef();
        if( poGeom == NULL )
            sqlite3_result_null(pContext);
        else
        {
            int     nBLOBLen;
            GByte   *pabySLBLOB;

            OGRSpatialReference* poSRS = poGeom->getSpatialReference();
            int nSRSId = pMyCursor->pVTab->poModule->FetchSRSId(poSRS);

            if( OGRSQLiteLayer::ExportSpatiaLiteGeometry(
                    poGeom, nSRSId, wkbNDR, FALSE, FALSE, FALSE,
                    &pabySLBLOB, &nBLOBLen ) == CE_None )
            {
                sqlite3_result_blob(pContext, pabySLBLOB, nBLOBLen, CPLFree);
            }
            else
            {
                sqlite3_result_null(pContext);
            }
        }
        return SQLITE_OK;
    }
    else if( nCol < 0 || nCol >= poFDefn->GetFieldCount() )
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
/*                        OGR2SQLITE_ogr_version()                     */
/************************************************************************/

static
void OGR2SQLITE_ogr_version(sqlite3_context* pContext,
                            int argc, sqlite3_value** argv)
{
    sqlite3_result_text( pContext, GDAL_RELEASE_NAME, -1, SQLITE_STATIC );
}

/************************************************************************/
/*                          OGR2SQLITE_Transform()                      */
/************************************************************************/

static
void OGR2SQLITE_Transform(sqlite3_context* pContext,
                          int argc, sqlite3_value** argv)
{
    if( argc != 3 )
    {
        sqlite3_result_null (pContext);
        return;
    }

    if( sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        sqlite3_result_null (pContext);
        return;
    }

    if( sqlite3_value_type (argv[1]) != SQLITE_INTEGER )
    {
        sqlite3_result_null (pContext);
        return;
    }

    if( sqlite3_value_type (argv[2]) != SQLITE_INTEGER )
    {
        sqlite3_result_null (pContext);
        return;
    }

    int nSrcSRSId = sqlite3_value_int(argv[1]);
    int nDstSRSId = sqlite3_value_int(argv[2]);

    OGR2SQLITEModule* poModule =
                    (OGR2SQLITEModule*) sqlite3_user_data(pContext);
    OGRCoordinateTransformation* poCT =
                    poModule->GetTransform(nSrcSRSId, nDstSRSId);
    if( poCT == NULL )
    {
        sqlite3_result_null (pContext);
        return;
    }

    GByte* pabySLBLOB = (GByte *) sqlite3_value_blob (argv[0]);
    int nBLOBLen = sqlite3_value_bytes (argv[0]);
    OGRGeometry* poGeom = NULL;
    if( OGRSQLiteLayer::ImportSpatiaLiteGeometry(
                    pabySLBLOB, nBLOBLen, &poGeom ) == CE_None &&
        poGeom->transform(poCT) == OGRERR_NONE &&
        OGRSQLiteLayer::ExportSpatiaLiteGeometry(
                    poGeom, nDstSRSId, wkbNDR, FALSE,
                    FALSE, FALSE, &pabySLBLOB, &nBLOBLen ) == CE_None )
    {
        sqlite3_result_blob(pContext, pabySLBLOB, nBLOBLen, CPLFree);
    }
    else
    {
        sqlite3_result_null (pContext);
    }
    delete poGeom;
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
/*                      OGR2SQLITEDestroyModule()                       */
/************************************************************************/

static void OGR2SQLITEDestroyModule(void* pData)
{
    CPLDebug("OGR", "Unloading VirtualOGR module");
    delete (OGR2SQLITEModule*) pData;
}

/************************************************************************/
/*                      OGR2SQLITESetupModule()                         */
/************************************************************************/

static int OGR2SQLITESetupModule(sqlite3* hDB, OGR2SQLITEModule* poModule)
{
    int rc;

    if( poModule != NULL )
        rc = sqlite3_create_module(hDB, "VirtualOGR", &sOGR2SQLITEModule, poModule);
    else
    {
        poModule = new OGR2SQLITEModule(NULL);
        rc = sqlite3_create_module_v2(hDB, "VirtualOGR", &sOGR2SQLITEModule, poModule,
                                      OGR2SQLITEDestroyModule);
    }
    if( rc != SQLITE_OK )
        return rc;

    rc= sqlite3_create_function(hDB, "ogr_version", 0, SQLITE_ANY, NULL,
                                OGR2SQLITE_ogr_version, NULL, NULL);
    if( rc != SQLITE_OK )
        return rc;

    // Custom and undocumented function, not sure I'll keep it.
    rc = sqlite3_create_function(hDB, "Transform3", 3, SQLITE_ANY, poModule,
                                 OGR2SQLITE_Transform, NULL, NULL);
    if( rc != SQLITE_OK )
        return rc;

    return rc;
}

/************************************************************************/
/*                              SetToDB()                               */
/************************************************************************/

int OGR2SQLITEModule::SetToDB(sqlite3* hDB)
{
    return OGR2SQLITESetupModule(hDB, this) == SQLITE_OK;
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

    int rc = OGR2SQLITESetupModule(hDB, NULL);
    
    if( rc == SQLITE_OK )
        CPLDebug("OGR", "OGR SQLite extension loaded");

    return rc;
}

#endif // VIRTUAL_OGR_DYNAMIC_EXTENSION_ENABLED

/************************************************************************/
/*                        OGR2SQLITE_static_register()                  */
/************************************************************************/

CPL_C_START
int CPL_DLL OGR2SQLITE_static_register (sqlite3 * db, char **pzErrMsg,
                                        const sqlite3_api_routines * pApi);
CPL_C_END

/* We just set the sqlite3_api structure with the pApi */
/* The registration of the module will be done later by OGR2SQLITESetupModule */
/* since we need a specific context. */
int OGR2SQLITE_static_register (sqlite3 * db, char **pzErrMsg,
                                const sqlite3_api_routines * pApi)
{
    SQLITE_EXTENSION_INIT2 (pApi);

    *pzErrMsg = NULL;

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
