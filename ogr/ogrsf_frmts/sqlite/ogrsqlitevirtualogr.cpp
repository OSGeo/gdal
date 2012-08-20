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

/* The layout of fields is :
   0   : RegularField0
   ...
   n-1 : RegularField(n-1)
   n   : OGR_STYLE (may be HIDDEN)
   n+1 : GEOMETRY
*/

/************************************************************************/
/*                          ~OGR2SQLITEModule                           */
/************************************************************************/

OGR2SQLITEModule::~OGR2SQLITEModule()
{
    std::map< std::pair<int,int>, OGRCoordinateTransformation*>::iterator oIter =
        oCachedTransformsMap.begin();
    for(; oIter != oCachedTransformsMap.end(); ++oIter)
        delete oIter->second;
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
    OGRDataSource        *poDS;
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

/************************************************************************/
/*                      OGR2SQLITE_ConnectCreate()                      */
/************************************************************************/

static
int OGR2SQLITE_ConnectCreate(sqlite3* hDB, void *pAux,
                             int argc, const char *const*argv,
                             sqlite3_vtab **ppVTab, char**pzErr)
{
    OGR2SQLITEModule* poModule = (OGR2SQLITEModule*) pAux;
    //CPLDebug("OGR2SQLITE", "Connect/Create");
    int i;

    if( argc < 3 )
        return SQLITE_ERROR;

    OGRLayer* poLayer = poModule->GetDS()->GetLayerByName(argv[2]);
    if( poLayer == NULL )
        return SQLITE_ERROR;

    int bExposeOGR_STYLE = FALSE;
    if( argc >= 5 )
        bExposeOGR_STYLE = atoi(argv[4]);

    OGR2SQLITE_vtab* vtab =
                (OGR2SQLITE_vtab*) CPLCalloc(1, sizeof(OGR2SQLITE_vtab));
    /* We dont need to fill the non-extended fields */
    vtab->poDS = poModule->GetDS();
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
        switch( poFieldDefn->GetType() )
        {
            case OFTInteger: osSQL += "INTEGER"; break;
            case OFTReal   : osSQL += "DOUBLE"; break;
            case OFTBinary : osSQL += "BLOB"; break;
            case OFTString :
            {
                if( poFieldDefn->GetWidth() > 0 )
                    osSQL += CPLSPrintf("VARCHAR(%d)", poFieldDefn->GetWidth());
                else
                    osSQL += "VARCHAR";
                break;
            }
            default:          osSQL += "VARCHAR"; break;
        }
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
    //CPLDebug("OGR2SQLITE", "BestIndex");

    int i;

    OGR2SQLITE_vtab* pMyVTab = (OGR2SQLITE_vtab*) pVTab;
    OGRFeatureDefn* poFDefn = pMyVTab->poLayer->GetLayerDefn();

    int nConstraints = 0;
    for (i = 0; i < pIndex->nConstraint; i++)
    {
        if (pIndex->aConstraint[i].usable &&
            pIndex->aConstraint[i].op != SQLITE_INDEX_CONSTRAINT_MATCH &&
            pIndex->aConstraint[i].iColumn < poFDefn->GetFieldCount() &&
            (pIndex->aConstraint[i].iColumn < 0 ||
             poFDefn->GetFieldDefn(pIndex->aConstraint[i].iColumn)->GetType() != OFTBinary))
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
    //CPLDebug("OGR2SQLITE", "Open(%s, %s)", pMyVTab->poDS->GetName(), pMyVTab->poLayer->GetName());

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
        pCursor->poLayer =
            pCursor->poDupDataSource->GetLayerByName(pMyVTab->poLayer->GetName());
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

    //CPLDebug("OGR2SQLITE", "Close(%s, %s)", pMyVTab->poDS->GetName(), pMyVTab->poLayer->GetName());
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
    //CPLDebug("OGR2SQLITE", "Filter");

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
                        "Unhandled constraint operator : %d", panConstraints[2 * i + 2]);
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
                    "Unhandled constraint data type : %d",sqlite3_value_type (argv[i]));
            return SQLITE_ERROR;
        }
    }

    CPLDebug("OGR2SQLITE", "Attribute filter : %s", osAttributeFilter.c_str());
    if( pMyCursor->poLayer->SetAttributeFilter(
            osAttributeFilter.size() ? osAttributeFilter.c_str() : NULL) != OGRERR_NONE )
    {
        sqlite3_free(pMyCursor->pVTab->zErrMsg);
        pMyCursor->pVTab->zErrMsg = sqlite3_mprintf(
                "Cannot apply attribute filter : %s", osAttributeFilter.c_str());
        return SQLITE_ERROR;
    }

    pMyCursor->poFeature = pMyCursor->poLayer->GetNextFeature();

    return SQLITE_OK;
}

/************************************************************************/
/*                          OGR2SQLITE_Next()                           */
/************************************************************************/

static
int OGR2SQLITE_Next(sqlite3_vtab_cursor* pCursor)
{
    OGR2SQLITE_vtab_cursor* pMyCursor = (OGR2SQLITE_vtab_cursor*) pCursor;
    //CPLDebug("OGR2SQLITE", "Next");

    delete pMyCursor->poFeature;
    pMyCursor->poFeature = pMyCursor->poLayer->GetNextFeature();

    return SQLITE_OK;
}

/************************************************************************/
/*                          OGR2SQLITE_Eof()                            */
/************************************************************************/

static
int OGR2SQLITE_Eof(sqlite3_vtab_cursor* pCursor)
{
    OGR2SQLITE_vtab_cursor* pMyCursor = (OGR2SQLITE_vtab_cursor*) pCursor;
    //CPLDebug("OGR2SQLITE", "Eof");

    return (pMyCursor->poFeature == NULL);
}

/************************************************************************/
/*                         OGR2SQLITE_Column()                          */
/************************************************************************/

static
int OGR2SQLITE_Column(sqlite3_vtab_cursor* pCursor,
                      sqlite3_context* pContext, int nCol)
{
    OGR2SQLITE_vtab_cursor* pMyCursor = (OGR2SQLITE_vtab_cursor*) pCursor;
    OGRFeature* poFeature = pMyCursor->poFeature;
    //CPLDebug("OGR2SQLITE", "Column %d", nCol);

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
    else if( nCol == (poFDefn->GetFieldCount() + 1) && poFDefn->GetGeomType() != wkbNone )
    {
        OGRGeometry* poGeom = poFeature->GetGeometryRef();
        if( poGeom == NULL )
            sqlite3_result_null(pContext);
        else
        {
            int     nBLOBLen;
            GByte   *pabySLBLOB;

            int nSRSId = -1;
            OGRSpatialReference* poSRS = poGeom->getSpatialReference();
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
            sqlite3_result_blob(pContext, pBlob, nSize, SQLITE_STATIC);
            break;
        }

        case OFTString:
            sqlite3_result_text(pContext,
                                poFeature->GetFieldAsString(nCol),
                                -1, SQLITE_STATIC);
            break;

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
    //CPLDebug("OGR2SQLITE", "Rowid");

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
    int nFieldCount = poLayer->GetLayerDefn()->GetFieldCount();
    int bHasGeomField = (poLayer->GetLayerDefn()->GetGeomType() != wkbNone);
    if( argc != 2 + nFieldCount + 1 + bHasGeomField)
    {
        CPLDebug("OGR2SQLITE", "Did not get expect argument count : %d, %d", argc,
                    2 + nFieldCount + 1 + bHasGeomField);
        return NULL;
    }

    OGRFeature* poFeature = new OGRFeature(poLayer->GetLayerDefn());
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
                poFeature->SetField(i, (const char*) sqlite3_value_text(argv[2 + i]));
                break;
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
/*                        OGR2SQLITEModule()                            */
/************************************************************************/

OGR2SQLITEModule::OGR2SQLITEModule(OGRDataSource* poDS) : poDS(poDS)
{
    sOGR2SQLITEModule.iVersion = 1;
    sOGR2SQLITEModule.xCreate = OGR2SQLITE_ConnectCreate;
    sOGR2SQLITEModule.xConnect = OGR2SQLITE_ConnectCreate;
    sOGR2SQLITEModule.xBestIndex = OGR2SQLITE_BestIndex;
    sOGR2SQLITEModule.xDisconnect = OGR2SQLITE_DisconnectDestroy;
    sOGR2SQLITEModule.xDestroy = OGR2SQLITE_DisconnectDestroy;
    sOGR2SQLITEModule.xOpen = OGR2SQLITE_Open;
    sOGR2SQLITEModule.xClose = OGR2SQLITE_Close;
    sOGR2SQLITEModule.xFilter = OGR2SQLITE_Filter;
    sOGR2SQLITEModule.xNext = OGR2SQLITE_Next;
    sOGR2SQLITEModule.xEof = OGR2SQLITE_Eof;
    sOGR2SQLITEModule.xColumn = OGR2SQLITE_Column;
    sOGR2SQLITEModule.xRowid = OGR2SQLITE_Rowid;
    sOGR2SQLITEModule.xUpdate = OGR2SQLITE_Update;
    sOGR2SQLITEModule.xBegin = NULL;
    sOGR2SQLITEModule.xSync = NULL;
    sOGR2SQLITEModule.xCommit = NULL;
    sOGR2SQLITEModule.xRollback = NULL;
    sOGR2SQLITEModule.xFindFunction = NULL; // OGR2SQLITE_FindFunction;
    sOGR2SQLITEModule.xRename = OGR2SQLITE_Rename;
}

/************************************************************************/
/*                              SetToDB()                               */
/************************************************************************/

void OGR2SQLITEModule::SetToDB(sqlite3* hDB)
{
    sqlite3_create_module(hDB, "OGR2SQLITE", &sOGR2SQLITEModule, this);
    sqlite3_create_function(hDB, "ogr_version", 0, SQLITE_ANY, NULL,
                            OGR2SQLITE_ogr_version, NULL, NULL);
    sqlite3_create_function(hDB, "Transform", 3, SQLITE_ANY, this,
                            OGR2SQLITE_Transform, NULL, NULL);
}
