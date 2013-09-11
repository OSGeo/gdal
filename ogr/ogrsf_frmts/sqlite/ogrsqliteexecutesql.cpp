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
        char             *pszTmpDBName;

    public:
        OGRSQLiteExecuteSQLLayer(char* pszTmpDBName,
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

OGRSQLiteExecuteSQLLayer::OGRSQLiteExecuteSQLLayer(char* pszTmpDBName,
                                                   OGRSQLiteDataSource* poDS,
                                                   CPLString osSQL,
                                                   sqlite3_stmt * hStmt,
                                                   int bUseStatementForGetNextFeature,
                                                   int bEmptyLayer ) :

                               OGRSQLiteSelectLayer(poDS, osSQL, hStmt,
                                                    bUseStatementForGetNextFeature,
                                                    bEmptyLayer, TRUE)
{
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
    VSIUnlink(pszTmpDBName);
    CPLFree(pszTmpDBName);
}

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
                 *pszSQLCommand == ')' ||
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
            osModifiedSQL += "\"";
            osModifiedSQL += oLayerDesc.osSubstitutedName;
            osModifiedSQL += "\"";
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

static void OGR2SQLITEGetPotentialLayerNamesInternal(const char **ppszSQLCommand,
                                                     std::set<LayerDesc>& oSetLayers,
                                                     std::set<CPLString>& oSetSpatialIndex,
                                                     CPLString& osModifiedSQL,
                                                     int& nNum)
{
    const char *pszSQLCommand = *ppszSQLCommand;
    const char* pszStart = pszSQLCommand;
    char ch;
    int nParenthesisLevel = 0;
    int bLookforFTableName = FALSE;

    while( (ch = *pszSQLCommand) != '\0' )
    {
        if( ch == '(' )
            nParenthesisLevel ++;
        else if( ch == ')' )
        {
            nParenthesisLevel --;
            if( nParenthesisLevel < 0 )
            {
                pszSQLCommand ++;
                break;
            }
        }

        /* Skip literals and strings */
        if( ch == '\'' || ch == '"' )
        {
            char chEscapeChar = ch;
            pszSQLCommand ++;
            while( (ch = *pszSQLCommand) != '\0' )
            {
                if( ch == chEscapeChar && pszSQLCommand[1] == chEscapeChar )
                    pszSQLCommand ++;
                else if( ch == chEscapeChar )
                {
                    pszSQLCommand ++;
                    break;
                }
                pszSQLCommand ++;
            }
        }

        else if( EQUALN(pszSQLCommand, "ogr_layer_", strlen("ogr_layer_"))  )
        {
            while( *pszSQLCommand != '\0' && *pszSQLCommand != '(' )
                pszSQLCommand ++;

            if( *pszSQLCommand != '(' )
                break;

            pszSQLCommand ++;
            nParenthesisLevel ++;

            while( isspace((int)*pszSQLCommand) )
                pszSQLCommand ++;

            OGR2SQLITEAddLayer(pszStart, nNum,
                                pszSQLCommand, oSetLayers, osModifiedSQL);
        }

        else if( bLookforFTableName &&
                 EQUALN(pszSQLCommand, "f_table_name", strlen("f_table_name")) &&
                 (pszSQLCommand[strlen("f_table_name")] == '=' ||
                  isspace((int)pszSQLCommand[strlen("f_table_name")])) )
        {
            pszSQLCommand += strlen("f_table_name");

            while( isspace((int)*pszSQLCommand) )
                pszSQLCommand ++;

            if( *pszSQLCommand == '=' )
            {
                pszSQLCommand ++;

                while( isspace((int)*pszSQLCommand) )
                    pszSQLCommand ++;

                oSetSpatialIndex.insert(OGR2SQLITEExtractUnquotedString(&pszSQLCommand));
            }

            bLookforFTableName = FALSE;
        }

        else if( EQUALN(pszSQLCommand, "FROM", strlen("FROM")) &&
                 isspace(pszSQLCommand[strlen("FROM")]) )
        {
            pszSQLCommand += strlen("FROM") + 1;

            while( isspace((int)*pszSQLCommand) )
                pszSQLCommand ++;

            if( EQUALN(pszSQLCommand, "SpatialIndex", strlen("SpatialIndex")) &&
                isspace((int)pszSQLCommand[strlen("SpatialIndex")]) )
            {
                pszSQLCommand += strlen("SpatialIndex") + 1;

                bLookforFTableName = TRUE;

                continue;
            }

            if( *pszSQLCommand == '(' )
            {
                pszSQLCommand++;

                CPLString osTruncated(pszStart);
                osTruncated.resize(pszSQLCommand - pszStart);
                osModifiedSQL += osTruncated;

                OGR2SQLITEGetPotentialLayerNamesInternal(
                            &pszSQLCommand, oSetLayers, oSetSpatialIndex,
                            osModifiedSQL, nNum);

                pszStart = pszSQLCommand;
            }
            else
                OGR2SQLITEAddLayer(pszStart, nNum,
                                   pszSQLCommand, oSetLayers, osModifiedSQL);

            while( *pszSQLCommand != '\0' )
            {
                if( isspace((int)*pszSQLCommand) )
                {
                    pszSQLCommand ++;
                    while( isspace((int)*pszSQLCommand) )
                        pszSQLCommand ++;

                    if( EQUALN(pszSQLCommand, "AS", 2) )
                    {
                        pszSQLCommand += 2;
                        while( isspace((int)*pszSQLCommand) )
                            pszSQLCommand ++;
                    }

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

                    if( *pszSQLCommand == '(' )
                    {
                        pszSQLCommand++;

                        CPLString osTruncated(pszStart);
                        osTruncated.resize(pszSQLCommand - pszStart);
                        osModifiedSQL += osTruncated;

                        OGR2SQLITEGetPotentialLayerNamesInternal(
                                                    &pszSQLCommand, oSetLayers,
                                                    oSetSpatialIndex,
                                                    osModifiedSQL, nNum);

                        pszStart = pszSQLCommand;
                    }
                    else
                        OGR2SQLITEAddLayer(pszStart, nNum,
                                           pszSQLCommand, oSetLayers,
                                           osModifiedSQL);
                }
                else
                    break;
            }
        }
        else if ( EQUALN(pszSQLCommand, "JOIN", strlen("JOIN")) &&
                  isspace(pszSQLCommand[strlen("JOIN")]) )
        {
            pszSQLCommand += strlen("JOIN") + 1;
            OGR2SQLITEAddLayer(pszStart, nNum, pszSQLCommand,
                               oSetLayers, osModifiedSQL);
        }
        else if( EQUALN(pszSQLCommand, "INTO", strlen("INTO")) &&
                 isspace(pszSQLCommand[strlen("INTO")]) )
        {
            pszSQLCommand += strlen("INTO") + 1;
            OGR2SQLITEAddLayer(pszStart, nNum, pszSQLCommand,
                               oSetLayers, osModifiedSQL);
        }
        else if( EQUALN(pszSQLCommand, "UPDATE", strlen("UPDATE")) &&
                 isspace(pszSQLCommand[strlen("UPDATE")]) )
        {
            pszSQLCommand += strlen("UPDATE") + 1;
            OGR2SQLITEAddLayer(pszStart, nNum, pszSQLCommand,
                               oSetLayers, osModifiedSQL);
        }
        else if ( EQUALN(pszSQLCommand, "DROP TABLE ", strlen("DROP TABLE ")) )
        {
            pszSQLCommand += strlen("DROP TABLE") + 1;
            OGR2SQLITEAddLayer(pszStart, nNum, pszSQLCommand,
                               oSetLayers, osModifiedSQL);
        }
        else
            pszSQLCommand ++;
    }

    CPLString osTruncated(pszStart);
    osTruncated.resize(pszSQLCommand - pszStart);
    osModifiedSQL += osTruncated;

    *ppszSQLCommand = pszSQLCommand;
}

static void OGR2SQLITEGetPotentialLayerNames(const char *pszSQLCommand,
                                             std::set<LayerDesc>& oSetLayers,
                                             std::set<CPLString>& oSetSpatialIndex,
                                             CPLString& osModifiedSQL)
{
    int nNum = 1;
    OGR2SQLITEGetPotentialLayerNamesInternal(&pszSQLCommand, oSetLayers,
                                             oSetSpatialIndex,
                                             osModifiedSQL, nNum);
}

/************************************************************************/
/*               OGR2SQLITE_IgnoreAllFieldsExceptGeometry()             */
/************************************************************************/

static
void OGR2SQLITE_IgnoreAllFieldsExceptGeometry(OGRLayer* poLayer)
{
    char** papszIgnored = NULL;
    papszIgnored = CSLAddString(papszIgnored, "OGR_STYLE");
    OGRFeatureDefn* poFeatureDefn = poLayer->GetLayerDefn();
    for(int i=0; i < poFeatureDefn->GetFieldCount(); i++)
    {
        papszIgnored = CSLAddString(papszIgnored,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef());
    }
    poLayer->SetIgnoredFields((const char**)papszIgnored);
    CSLDestroy(papszIgnored);
}


/************************************************************************/
/*                  OGR2SQLITEDealWithSpatialColumn()                   */
/************************************************************************/
static
int OGR2SQLITEDealWithSpatialColumn(OGRLayer* poLayer,
                                    int iGeomCol,
                                    const LayerDesc& oLayerDesc,
                                    const CPLString& osTableName,
                                    OGRSQLiteDataSource* poSQLiteDS,
                                    sqlite3* hDB,
                                    int bSpatialiteDB,
                                    const std::set<LayerDesc>& oSetLayers,
                                    const std::set<CPLString>& oSetSpatialIndex
                                   )
{
    int rc;

    OGRGeomFieldDefn* poGeomField =
        poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeomCol);
    CPLString osGeomColRaw;
    if( iGeomCol == 0 )
        osGeomColRaw = OGR2SQLITE_GetNameForGeometryColumn(poLayer);
    else
        osGeomColRaw = poGeomField->GetNameRef();
    const char* pszGeomColRaw = osGeomColRaw.c_str();

    CPLString osGeomColEscaped(OGRSQLiteEscape(pszGeomColRaw));
    const char* pszGeomColEscaped = osGeomColEscaped.c_str();

    CPLString osLayerNameEscaped(OGRSQLiteEscape(osTableName));
    const char* pszLayerNameEscaped = osLayerNameEscaped.c_str();

    CPLString osIdxNameRaw(CPLSPrintf("idx_%s_%s",
                    oLayerDesc.osLayerName.c_str(), pszGeomColRaw));
    CPLString osIdxNameEscaped(OGRSQLiteEscapeName(osIdxNameRaw));

    /* Make sure that the SRS is injected in spatial_ref_sys */
    OGRSpatialReference* poSRS = poGeomField->GetSpatialRef();
    if( iGeomCol == 0 && poSRS == NULL )
        poSRS = poLayer->GetSpatialRef();
    int nSRSId = poSQLiteDS->GetUndefinedSRID();
    if( poSRS != NULL )
        nSRSId = poSQLiteDS->FetchSRSId(poSRS);

    CPLString osSQL;
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
#ifdef HAVE_SPATIALITE
    else
    {
        /* We detect the need for creating a spatial index by 2 means : */

        /* 1) if there's an explicit reference to a 'idx_layername_geometrycolumn' */
        /*   table in the SQL --> old/traditionnal way of requesting spatial indices */
        /*   with spatialite. */

        std::set<LayerDesc>::const_iterator oIter2 = oSetLayers.begin();
        for(; oIter2 != oSetLayers.end(); ++oIter2)
        {
            const LayerDesc& oLayerDescIter = *oIter2;
            if( EQUAL(oLayerDescIter.osLayerName, osIdxNameRaw) )
            {
                    bCreateSpatialIndex = TRUE;
                    break;
            }
        }

        /* 2) or if there's a SELECT FROM SpatialIndex WHERE f_table_name = 'layername' */
        if( !bCreateSpatialIndex )
        {
            std::set<CPLString>::const_iterator oIter3 = oSetSpatialIndex.begin();
            for(; oIter3 != oSetSpatialIndex.end(); ++oIter3)
            {
                const CPLString& osNameIter = *oIter3;
                if( EQUAL(osNameIter, oLayerDesc.osLayerName) )
                {
                    bCreateSpatialIndex = TRUE;
                    break;
                }
            }
        }

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
                        "VALUES ('%s',Lower('%s'),%d ,%d ,%d, %d)",
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
#endif // HAVE_SPATIALITE
    rc = sqlite3_exec( hDB, osSQL.c_str(), NULL, NULL, NULL );

#ifdef HAVE_SPATIALITE
/* -------------------------------------------------------------------- */
/*      Should we create a spatial index ?.                             */
/* -------------------------------------------------------------------- */
    if( !bSpatialiteDB || !bCreateSpatialIndex )
        return rc == SQLITE_OK;

    CPLDebug("SQLITE", "Create spatial index %s", osIdxNameRaw.c_str());

    /* ENABLE_VIRTUAL_OGR_SPATIAL_INDEX is not defined */
#ifdef ENABLE_VIRTUAL_OGR_SPATIAL_INDEX
    osSQL.Printf("CREATE VIRTUAL TABLE \"%s\" USING "
                    "VirtualOGRSpatialIndex(%d, '%s', pkid, xmin, xmax, ymin, ymax)",
                    osIdxNameEscaped.c_str(),
                    nExtraDS,
                    OGRSQLiteEscape(oLayerDesc.osLayerName).c_str());

    rc = sqlite3_exec( hDB, osSQL.c_str(), NULL, NULL, NULL );
    if( rc != SQLITE_OK )
    {
        CPLDebug("SQLITE",
                    "Error occured during spatial index creation : %s",
                    sqlite3_errmsg(hDB));
    }
#else //  ENABLE_VIRTUAL_OGR_SPATIAL_INDEX
    rc = sqlite3_exec( hDB, "BEGIN", NULL, NULL, NULL );

    osSQL.Printf("CREATE VIRTUAL TABLE \"%s\" "
                    "USING rtree(pkid, xmin, xmax, ymin, ymax)",
                    osIdxNameEscaped.c_str());

    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB, osSQL.c_str(), NULL, NULL, NULL );

    sqlite3_stmt *hStmt = NULL;
    if( rc == SQLITE_OK )
    {
        const char* pszInsertInto = CPLSPrintf(
            "INSERT INTO \"%s\" (pkid, xmin, xmax, ymin, ymax) "
            "VALUES (?,?,?,?,?)", osIdxNameEscaped.c_str());
        rc = sqlite3_prepare(hDB, pszInsertInto, -1, &hStmt, NULL);
    }

    OGRFeature* poFeature;
    OGREnvelope sEnvelope;
    OGR2SQLITE_IgnoreAllFieldsExceptGeometry(poLayer);
    poLayer->ResetReading();

    while( rc == SQLITE_OK &&
            (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        OGRGeometry* poGeom = poFeature->GetGeometryRef();
        if( poGeom != NULL && !poGeom->IsEmpty() )
        {
            poGeom->getEnvelope(&sEnvelope);
            sqlite3_bind_int64(hStmt, 1,
                                (sqlite3_int64) poFeature->GetFID() );
            sqlite3_bind_double(hStmt, 2, sEnvelope.MinX);
            sqlite3_bind_double(hStmt, 3, sEnvelope.MaxX);
            sqlite3_bind_double(hStmt, 4, sEnvelope.MinY);
            sqlite3_bind_double(hStmt, 5, sEnvelope.MaxY);
            rc = sqlite3_step(hStmt);
            if( rc == SQLITE_OK || rc == SQLITE_DONE )
                rc = sqlite3_reset(hStmt);
        }
        delete poFeature;
    }

    poLayer->SetIgnoredFields(NULL);

    sqlite3_finalize(hStmt);

    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB, "COMMIT", NULL, NULL, NULL );
    else
    {
        CPLDebug("SQLITE",
                    "Error occured during spatial index creation : %s",
                    sqlite3_errmsg(hDB));
        rc = sqlite3_exec( hDB, "ROLLBACK", NULL, NULL, NULL );
    }
#endif //  ENABLE_VIRTUAL_OGR_SPATIAL_INDEX

#endif // HAVE_SPATIALITE

    return rc == SQLITE_OK;
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

    CPLString osOldVal;
    const char* pszOldVal = CPLGetConfigOption("OGR_SQLITE_STATIC_VIRTUAL_OGR", NULL);
    if( pszOldVal != NULL )
    {
        osOldVal = pszOldVal;
        pszOldVal = osOldVal.c_str();
    }

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
        if( !bTried &&
            CSLTestBoolean(CPLGetConfigOption("OGR_SQLITE_DIALECT_USE_SPATIALITE", "YES")) )
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
        CPLSetThreadLocalConfigOption("OGR_SQLITE_STATIC_VIRTUAL_OGR", "NO");
        nRet = poSQLiteDS->Open( pszTmpDBName, TRUE );
        CPLSetThreadLocalConfigOption("OGR_SQLITE_STATIC_VIRTUAL_OGR", pszOldVal);
        if( !nRet )
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
    CPLSetThreadLocalConfigOption("OGR_SQLITE_STATIC_VIRTUAL_OGR", "NO");
    nRet = poSQLiteDS->Create( pszTmpDBName, papszOptions );
    CPLSetThreadLocalConfigOption("OGR_SQLITE_STATIC_VIRTUAL_OGR", pszOldVal);
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
#else // HAVE_SPATIALITE
    if( TRUE )
    {
#endif // HAVE_SPATIALITE
        poSQLiteDS = new OGRSQLiteDataSource();
        CPLSetThreadLocalConfigOption("OGR_SQLITE_STATIC_VIRTUAL_OGR", "NO");
        nRet = poSQLiteDS->Create( pszTmpDBName, NULL );
        CPLSetThreadLocalConfigOption("OGR_SQLITE_STATIC_VIRTUAL_OGR", pszOldVal);
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
    OGR2SQLITEModule* poModule = OGR2SQLITE_Setup(poDS, poSQLiteDS);
    sqlite3* hDB = poSQLiteDS->GetDB();

/* -------------------------------------------------------------------- */
/*      Analysze the statement to determine which tables will be used.  */
/* -------------------------------------------------------------------- */
    std::set<LayerDesc> oSetLayers;
    std::set<CPLString> oSetSpatialIndex;
    CPLString osModifiedSQL;
    OGR2SQLITEGetPotentialLayerNames(pszStatement, oSetLayers,
                                     oSetSpatialIndex, osModifiedSQL);
    std::set<LayerDesc>::iterator oIter = oSetLayers.begin();

    if( strcmp(pszStatement, osModifiedSQL.c_str()) != 0 )
        CPLDebug("OGR", "Modified SQL: %s", osModifiedSQL.c_str());
    pszStatement = osModifiedSQL.c_str(); /* do not use it anymore */

    int bFoundOGRStyle = ( osModifiedSQL.ifind("OGR_STYLE") != std::string::npos );

/* -------------------------------------------------------------------- */
/*      For each of those tables, create a Virtual Table.               */
/* -------------------------------------------------------------------- */
    for(; oIter != oSetLayers.end(); ++oIter)
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
        int nExtraDS;
        if( oLayerDesc.osDSName.size() == 0 )
        {
            poLayer = poDS->GetLayerByName(oLayerDesc.osLayerName);
            /* Might be a false positive (unlikely) */
            if( poLayer == NULL )
                continue;

            osTableName = oLayerDesc.osLayerName;

            nExtraDS = -1;
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

            nExtraDS = OGR2SQLITE_AddExtraDS(poModule, poOtherDS);
        }

        osSQL.Printf("CREATE VIRTUAL TABLE \"%s\" USING VirtualOGR(%d,'%s',%d)",
                OGRSQLiteEscapeName(osTableName).c_str(),
                nExtraDS,
                OGRSQLiteEscape(oLayerDesc.osLayerName).c_str(),
                bFoundOGRStyle);

        char* pszErrMsg = NULL;
        int rc = sqlite3_exec( hDB, osSQL.c_str(),
                               NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create virtual table for layer '%s' : %s",
                     osTableName.c_str(), pszErrMsg);
            sqlite3_free(pszErrMsg);
            continue;
        }

        for(int i=0; i<poLayer->GetLayerDefn()->GetGeomFieldCount(); i++)
        {
            OGR2SQLITEDealWithSpatialColumn(poLayer, i, oLayerDesc,
                                            osTableName, poSQLiteDS, hDB,
                                            bSpatialiteDB, oSetLayers,
                                            oSetSpatialIndex);
        }
    }

/* -------------------------------------------------------------------- */
/*      Reload, so that virtual tables are recognized                   */
/* -------------------------------------------------------------------- */
    poSQLiteDS->ReloadLayers();

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

    poLayer = new OGRSQLiteExecuteSQLLayer( pszTmpDBName,
                                            poSQLiteDS, pszStatement, hSQLStmt,
                                            bUseStatementForGetNextFeature, bEmptyLayer );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( 0, poSpatialFilter );

    return poLayer;
}

/************************************************************************/
/*                   OGRSQLiteGetReferencedLayers()                     */
/************************************************************************/

std::set<LayerDesc> OGRSQLiteGetReferencedLayers(const char* pszStatement)
{
/* -------------------------------------------------------------------- */
/*      Analysze the statement to determine which tables will be used.  */
/* -------------------------------------------------------------------- */
    std::set<LayerDesc> oSetLayers;
    std::set<CPLString> oSetSpatialIndex;
    CPLString osModifiedSQL;
    OGR2SQLITEGetPotentialLayerNames(pszStatement, oSetLayers,
                                     oSetSpatialIndex, osModifiedSQL);

    return oSetLayers;
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

/************************************************************************/
/*                   OGRSQLiteGetReferencedLayers()                     */
/************************************************************************/

std::set<LayerDesc> OGRSQLiteGetReferencedLayers(const char* pszStatement)
{
     std::set<LayerDesc> oSetLayers;
     return oSetLayers;
}

#endif // HAVE_SQLITE_VFS
