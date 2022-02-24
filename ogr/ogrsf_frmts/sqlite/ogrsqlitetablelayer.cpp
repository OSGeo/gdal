/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteTableLayer class, access to an existing table.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
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
#include "ogrsqliteutility.h"

#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "sqlite3.h"

static const char UNSUPPORTED_OP_READ_ONLY[] =
  "%s : unsupported operation on a read-only datasource.";

CPL_CVSID("$Id$")

/************************************************************************/
/*                        OGRSQLiteTableLayer()                         */
/************************************************************************/

OGRSQLiteTableLayer::OGRSQLiteTableLayer( OGRSQLiteDataSource *poDSIn ) :
    OGRSQLiteLayer(poDSIn),
    m_bSpatialite2D(poDSIn->GetSpatialiteVersionNumber() < 24),
    m_bHasCheckedTriggers(!CPLTestBool(
        CPLGetConfigOption("OGR_SQLITE_DISABLE_INSERT_TRIGGERS", "YES")))
{
}

/************************************************************************/
/*                        ~OGRSQLiteTableLayer()                        */
/************************************************************************/

OGRSQLiteTableLayer::~OGRSQLiteTableLayer()

{
    ClearStatement();
    ClearInsertStmt();

    const int nGeomFieldCount =
        m_poFeatureDefn ? m_poFeatureDefn->GetGeomFieldCount() : 0;
    for( int i = 0; i < nGeomFieldCount; i++ )
    {
        OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
            m_poFeatureDefn->myGetGeomFieldDefn(i);
        // Restore temporarily disabled triggers.
        for( int j = 0;
             j < static_cast<int>(poGeomFieldDefn->m_aosDisabledTriggers.size());
             j++ )
        {
            CPLDebug("SQLite", "Restoring trigger %s",
                     poGeomFieldDefn->m_aosDisabledTriggers[j].first.c_str());
            // This may fail since CreateSpatialIndex() reinstalls triggers, so
            // don't check result.
            CPL_IGNORE_RET_VAL(
                sqlite3_exec(
                    m_poDS->GetDB(),
                    poGeomFieldDefn->m_aosDisabledTriggers[j].second.c_str(),
                    nullptr, nullptr, nullptr ));
        }
    }

    CPLFree(m_pszTableName);
    CPLFree(m_pszEscapedTableName);
    CPLFree(m_pszCreationGeomFormat);
}

/************************************************************************/
/*                     CreateSpatialIndexIfNecessary()                  */
/************************************************************************/

void OGRSQLiteTableLayer::CreateSpatialIndexIfNecessary()
{
    if( m_bDeferredSpatialIndexCreation )
    {
        for(int iGeomCol = 0; iGeomCol < m_poFeatureDefn->GetGeomFieldCount(); iGeomCol ++)
            CreateSpatialIndex(iGeomCol);
        m_bDeferredSpatialIndexCreation = false;
    }
}

/************************************************************************/
/*                          ClearInsertStmt()                           */
/************************************************************************/

void OGRSQLiteTableLayer::ClearInsertStmt()
{
    if( m_hInsertStmt != nullptr )
    {
        sqlite3_finalize( m_hInsertStmt );
        m_hInsertStmt = nullptr;
    }
    m_osLastInsertStmt = "";
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRSQLiteTableLayer::Initialize( const char *m_pszTableNameIn,
                                        bool bIsTable,
                                        bool bIsVirtualShapeIn,
                                        bool bDeferredCreationIn )
{
    SetDescription( m_pszTableNameIn );

    m_bIsTable = bIsTable;
    m_bIsVirtualShape = bIsVirtualShapeIn;
    m_pszTableName = CPLStrdup(m_pszTableNameIn);
    m_bDeferredCreation = bDeferredCreationIn;
    m_pszEscapedTableName = CPLStrdup(SQLEscapeLiteral(m_pszTableName));

    if( strchr(m_pszTableName, '(') != nullptr &&
        m_pszTableName[strlen(m_pszTableName)-1] == ')' )
    {
        char* pszErrMsg = nullptr;
        int nRowCount = 0, nColCount = 0;
        char** papszResult = nullptr;
        const char* pszSQL = CPLSPrintf("SELECT * FROM sqlite_master WHERE name = '%s'",
                                        m_pszEscapedTableName);
        int rc = sqlite3_get_table( m_poDS->GetDB(),
                                pszSQL,
                                &papszResult, &nRowCount,
                                &nColCount, &pszErrMsg );
        int bFound = ( rc == SQLITE_OK && nRowCount == 1 );
        sqlite3_free_table(papszResult);
        sqlite3_free( pszErrMsg );

        if( !bFound )
        {
            char* pszGeomCol = CPLStrdup(strchr(m_pszTableName, '(')+1);
            pszGeomCol[strlen(pszGeomCol)-1] = 0;
            *strchr(m_pszTableName, '(') = 0;
            CPLFree(m_pszEscapedTableName);
            m_pszEscapedTableName = CPLStrdup(SQLEscapeLiteral(m_pszTableName));
            EstablishFeatureDefn(pszGeomCol);
            CPLFree(pszGeomCol);
            if( m_poFeatureDefn == nullptr || m_poFeatureDefn->GetGeomFieldCount() == 0 )
                return CE_Failure;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                            GetGeomFormat()                           */
/************************************************************************/

static OGRSQLiteGeomFormat GetGeomFormat( const char* pszGeomFormat )
{
    OGRSQLiteGeomFormat eGeomFormat = OSGF_None;
    if( pszGeomFormat )
    {
        if ( EQUAL(pszGeomFormat, "WKT") )
            eGeomFormat = OSGF_WKT;
        else if ( EQUAL(pszGeomFormat,"WKB") )
            eGeomFormat = OSGF_WKB;
        else if ( EQUAL(pszGeomFormat,"FGF") )
            eGeomFormat = OSGF_FGF;
        else if( EQUAL(pszGeomFormat,"SpatiaLite") )
            eGeomFormat = OSGF_SpatiaLite;
    }
    return eGeomFormat;
}

/************************************************************************/
/*                         SetCreationParameters()                      */
/************************************************************************/

void OGRSQLiteTableLayer::SetCreationParameters( const char *pszFIDColumnName,
                                                 OGRwkbGeometryType eGeomType,
                                                 const char *pszGeomFormat,
                                                 const char *pszGeometryName,
                                                 OGRSpatialReference *poSRS,
                                                 int nSRSId )

{
    m_pszFIDColumn = CPLStrdup(pszFIDColumnName);
    m_poFeatureDefn = new OGRSQLiteFeatureDefn(m_pszTableName);
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();
    m_pszCreationGeomFormat = (pszGeomFormat) ? CPLStrdup(pszGeomFormat) : nullptr;
    if( eGeomType != wkbNone )
    {
        if( nSRSId == UNINITIALIZED_SRID )
            nSRSId = m_poDS->GetUndefinedSRID();
        OGRSQLiteGeomFormat eGeomFormat = GetGeomFormat(pszGeomFormat);
        auto poGeomFieldDefn =
            cpl::make_unique<OGRSQLiteGeomFieldDefn>(pszGeometryName, -1);
        poGeomFieldDefn->SetType(eGeomType);
        poGeomFieldDefn->m_nSRSId = nSRSId;
        poGeomFieldDefn->m_eGeomFormat = eGeomFormat;
        poGeomFieldDefn->SetSpatialRef(poSRS);
        m_poFeatureDefn->AddGeomFieldDefn(std::move(poGeomFieldDefn));
    }
}

/************************************************************************/
/*                               GetName()                              */
/************************************************************************/

const char* OGRSQLiteTableLayer::GetName()
{
    return GetDescription();
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **OGRSQLiteTableLayer::GetMetadata( const char *pszDomain )

{
    // Update GetMetadataItem() optimization that skips calling GetMetadata()
    // when key != OLMD_FID64 if we add more metadata items.

    GetLayerDefn();
    if( !m_bHasTriedDetectingFID64 && m_pszFIDColumn != nullptr )
    {
        m_bHasTriedDetectingFID64 = true;

/* -------------------------------------------------------------------- */
/*      Find if the FID holds 64bit values                              */
/* -------------------------------------------------------------------- */

        // Normally the fid should be AUTOINCREMENT, so check sqlite_sequence
        OGRErr err = OGRERR_NONE;
        char* pszSQL = sqlite3_mprintf(
            "SELECT seq FROM sqlite_sequence WHERE name = '%q'",
            m_pszTableName);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        GIntBig nMaxId = SQLGetInteger64( m_poDS->GetDB(), pszSQL, &err);
        CPLPopErrorHandler();
        sqlite3_free(pszSQL);
        if( err != OGRERR_NONE )
        {
            CPLErrorReset();

            // In case of error, fallback to taking the MAX of the FID
            pszSQL = sqlite3_mprintf("SELECT MAX(\"%w\") FROM \"%w\"",
                                        m_pszFIDColumn,
                                        m_pszTableName);

            nMaxId = SQLGetInteger64( m_poDS->GetDB(), pszSQL, nullptr);
            sqlite3_free(pszSQL);
        }
        if( nMaxId > INT_MAX )
            OGRLayer::SetMetadataItem(OLMD_FID64, "YES");
    }

    return OGRSQLiteLayer::GetMetadata(pszDomain);
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *OGRSQLiteTableLayer::GetMetadataItem( const char * pszName,
                                                  const char * pszDomain )
{
    if( !((pszDomain == nullptr || EQUAL(pszDomain, "")) && EQUAL(pszName, OLMD_FID64)) )
        return nullptr;
    return CSLFetchNameValue( GetMetadata(pszDomain), pszName );
}

/************************************************************************/
/*                         EstablishFeatureDefn()                       */
/************************************************************************/

CPLErr OGRSQLiteTableLayer::EstablishFeatureDefn(const char* pszGeomCol)
{
    sqlite3 *hDB = m_poDS->GetDB();

/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */
    bool bHasRowId = m_bIsTable;

    // SELECT .. FROM ... LIMIT ... is broken on VirtualShape tables with spatialite 5.0.1 and sqlite 3.38.0
    const char *pszSQLConst =
        CPLSPrintf(m_bIsVirtualShape ?
                        "SELECT %s* FROM '%s'" :
                        "SELECT %s* FROM '%s' LIMIT 1",
                   m_bIsTable ? "_rowid_, " : "",
                   m_pszEscapedTableName);

    sqlite3_stmt *hColStmt = nullptr;
    int rc = sqlite3_prepare_v2( hDB, pszSQLConst, -1, &hColStmt, nullptr );
    if( rc != SQLITE_OK )
    {
        const char* pszErrMsg = sqlite3_errmsg(hDB);
        if( m_bIsTable && pszErrMsg && strstr(pszErrMsg, "_rowid_") != nullptr )
        {
            // This is likely a table WITHOUT ROWID
            bHasRowId = false;
            sqlite3_finalize( hColStmt );
            hColStmt = nullptr;
            pszSQLConst = CPLSPrintf(
                "SELECT * FROM '%s' LIMIT 1", m_pszEscapedTableName);
            rc = sqlite3_prepare_v2( hDB, pszSQLConst, -1, &hColStmt, nullptr );
        }
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to query table %s for column definitions : %s.",
                      m_pszTableName, sqlite3_errmsg(hDB) );

            return CE_Failure;
        }
    }

    rc = sqlite3_step( hColStmt );
    if ( rc != SQLITE_DONE && rc != SQLITE_ROW )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "In Initialize(): sqlite3_step(%s):\n  %s",
                  pszSQLConst, sqlite3_errmsg(hDB) );
        sqlite3_finalize( hColStmt );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      What should we use as FID?  If there is a primary key           */
/*      integer field, then this will be used as the _rowid_, and we    */
/*      will pick up the real column name here.                         */
/*                                                                      */
/*      Note that the select _rowid_ will return the real column        */
/*      name if the rowid corresponds to another primary key            */
/*      column.                                                         */
/* -------------------------------------------------------------------- */
    if( bHasRowId )
    {
        CPLFree( m_pszFIDColumn );
        m_pszFIDColumn = CPLStrdup(SQLUnescape(sqlite3_column_name( hColStmt, 0 )));
    }

/* -------------------------------------------------------------------- */
/*      Collect the rest of the fields.                                 */
/* -------------------------------------------------------------------- */
    if( pszGeomCol )
    {
        std::set<CPLString> aosGeomCols;
        aosGeomCols.insert(pszGeomCol);
        std::set<CPLString> aosIgnoredCols(m_poDS->GetGeomColsForTable(m_pszTableName));
        aosIgnoredCols.erase(pszGeomCol);
        BuildFeatureDefn( GetDescription(),false, hColStmt, &aosGeomCols, aosIgnoredCols);
    }
    else
    {
        std::set<CPLString> aosIgnoredCols;
        const std::set<CPLString>& aosGeomCols(m_poDS->GetGeomColsForTable(m_pszTableName));
        BuildFeatureDefn( GetDescription(), false, hColStmt,
                          (m_bIsVirtualShape) ? nullptr : &aosGeomCols, aosIgnoredCols );
    }
    sqlite3_finalize( hColStmt );

/* -------------------------------------------------------------------- */
/*      Set the properties of the geometry column.                      */
/* -------------------------------------------------------------------- */
    bool bHasSpatialiteCol = false;
    for(int i=0; i<m_poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
            m_poFeatureDefn->myGetGeomFieldDefn(i);
        poGeomFieldDefn->m_nSRSId = m_poDS->GetUndefinedSRID();

        if( m_poDS->IsSpatialiteDB() )
        {
            if( m_poDS->HasSpatialite4Layout() )
            {
                pszSQLConst = CPLSPrintf("SELECT srid, geometry_type, coord_dimension, spatial_index_enabled FROM geometry_columns WHERE lower(f_table_name) = lower('%s') AND lower(f_geometry_column) = lower('%s')",
                                    m_pszEscapedTableName,
                                    SQLEscapeLiteral(poGeomFieldDefn->GetNameRef()).c_str());
            }
            else
            {
                pszSQLConst = CPLSPrintf("SELECT srid, type, coord_dimension, spatial_index_enabled FROM geometry_columns WHERE lower(f_table_name) = lower('%s') AND lower(f_geometry_column) = lower('%s')",
                                    m_pszEscapedTableName,
                                    SQLEscapeLiteral(poGeomFieldDefn->GetNameRef()).c_str());
            }
        }
        else
        {
            pszSQLConst = CPLSPrintf("SELECT srid, geometry_type, coord_dimension, geometry_format FROM geometry_columns WHERE lower(f_table_name) = lower('%s') AND lower(f_geometry_column) = lower('%s')",
                                m_pszEscapedTableName,
                                SQLEscapeLiteral(poGeomFieldDefn->GetNameRef()).c_str());
        }
        char* pszErrMsg = nullptr;
        int nRowCount = 0, nColCount = 0;
        char** papszResult = nullptr;
        rc = sqlite3_get_table( hDB,
                                pszSQLConst,
                                &papszResult, &nRowCount,
                                &nColCount, &pszErrMsg );
        OGRwkbGeometryType eGeomType = wkbUnknown;
        OGRSQLiteGeomFormat eGeomFormat = OSGF_None;
        if( rc == SQLITE_OK && nRowCount == 1 )
        {
            char **papszRow = papszResult + nColCount;
            if( papszRow[1] == nullptr || papszRow[2] == nullptr )
            {
                CPLDebug("SQLite", "Did not get expected col value");
                sqlite3_free_table(papszResult);
                continue;
            }
            if( papszRow[0] != nullptr )
                poGeomFieldDefn->m_nSRSId = atoi(papszRow[0]);
            if( m_poDS->IsSpatialiteDB() )
            {
                if( papszRow[3] != nullptr )
                    poGeomFieldDefn->m_bHasSpatialIndex = atoi(papszRow[3]) != 0;
                if( m_poDS->HasSpatialite4Layout() )
                {
                    int nGeomType = atoi(papszRow[1]);

                    if( nGeomType >= 0 && nGeomType <= 7 ) /* XY */
                        eGeomType = (OGRwkbGeometryType) nGeomType;
                    else if( nGeomType >= 1000 && nGeomType <= 1007 ) /* XYZ */
                        eGeomType = wkbSetZ(wkbFlatten(nGeomType));
                    else if( nGeomType >= 2000 && nGeomType <= 2007 ) /* XYM */
                        eGeomType = wkbSetM(wkbFlatten(nGeomType));
                    else if( nGeomType >= 3000 && nGeomType <= 3007 ) /* XYZM */
                        eGeomType = wkbSetM(wkbSetZ(wkbFlatten(nGeomType)));
                }
                else
                {
                    eGeomType = OGRFromOGCGeomType(papszRow[1]);

                    if( strcmp ( papszRow[2], "XYZ" ) == 0 ||
                        strcmp ( papszRow[2], "3" ) == 0) // SpatiaLite's own 3D geometries
                        eGeomType = wkbSetZ(eGeomType);
                    else if( strcmp ( papszRow[2], "XYM" ) == 0 )
                        eGeomType = wkbSetM(eGeomType);
                    else if( strcmp ( papszRow[2], "XYZM" ) == 0 ) // M coordinate declared
                        eGeomType = wkbSetM(wkbSetZ(eGeomType));
                }
                eGeomFormat = OSGF_SpatiaLite;
            }
            else
            {
                eGeomType = (OGRwkbGeometryType) atoi(papszRow[1]);
                if( atoi(papszRow[2]) > 2 )
                    eGeomType = wkbSetZ(eGeomType);
                eGeomFormat = GetGeomFormat( papszRow[3] );
            }
        }
        sqlite3_free_table(papszResult);
        sqlite3_free( pszErrMsg );

        poGeomFieldDefn->m_eGeomFormat = eGeomFormat;
        poGeomFieldDefn->SetType( eGeomType );
        poGeomFieldDefn->SetSpatialRef(m_poDS->FetchSRS(poGeomFieldDefn->m_nSRSId));

        // cppcheck-suppress knownConditionTrueFalse
        if( eGeomFormat == OSGF_SpatiaLite )
            bHasSpatialiteCol = true;
    }

    if ( bHasSpatialiteCol &&
        m_poDS->IsSpatialiteLoaded() &&
        m_poDS->GetSpatialiteVersionNumber() < 24 && m_poDS->GetUpdate() )
    {
        // we need to test version required by Spatialite TRIGGERs
        // hColStmt = NULL;
        pszSQLConst = CPLSPrintf( "SELECT sql FROM sqlite_master WHERE type = 'trigger' AND tbl_name = '%s' AND sql LIKE '%%RTreeAlign%%'",
            m_pszEscapedTableName );

        int nRowTriggerCount, nColTriggerCount;
        char **papszTriggerResult, *pszErrMsg;

        /* rc = */ sqlite3_get_table( hDB, pszSQLConst, &papszTriggerResult,
            &nRowTriggerCount, &nColTriggerCount, &pszErrMsg );
        if( nRowTriggerCount >= 1 )
        {
        // obsolete library version not supporting new triggers
        // enforcing ReadOnly mode
            CPLDebug("SQLITE", "Enforcing ReadOnly mode : obsolete library version not supporting new triggers");
            m_poDS->DisableUpdate();
        }

        sqlite3_free_table( papszTriggerResult );
    }
/* -------------------------------------------------------------------- */
/*      Check if there are default values and nullable status           */
/* -------------------------------------------------------------------- */

    char **papszResult = nullptr;
    int nRowCount = 0;
    int nColCount = 0;
    char *pszErrMsg = nullptr;
    /*  #|name|type|notnull|default|pk */
    char* pszSQL = sqlite3_mprintf("PRAGMA table_info('%q')", m_pszTableName);
    rc = sqlite3_get_table( hDB, pszSQL, &papszResult, &nRowCount,
                            &nColCount, &pszErrMsg );
    sqlite3_free( pszSQL );
    if( rc != SQLITE_OK )
    {
        sqlite3_free( pszErrMsg );
    }
    else
    {
        if( nColCount == 6 )
        {
            const std::set<std::string> uniqueFieldsUC(
                SQLGetUniqueFieldUCConstraints(hDB, m_pszTableName));
            for(int i=0;i<nRowCount;i++)
            {
                const char* pszName = papszResult[(i+1)*6+1];
                const char* pszNotNull = papszResult[(i+1)*6+3];
                const char* pszDefault = papszResult[(i+1)*6+4];
                const int idx = pszName != nullptr ?
                                    m_poFeatureDefn->GetFieldIndex(pszName) : -1;
                if( pszDefault != nullptr )
                {
                    if( idx >= 0 )
                    {
                        OGRFieldDefn* poFieldDefn =  m_poFeatureDefn->GetFieldDefn(idx);
                        if( poFieldDefn->GetType() == OFTString &&
                            !EQUAL(pszDefault, "NULL") &&
                            !STARTS_WITH_CI(pszDefault, "CURRENT_") &&
                            pszDefault[0] != '(' &&
                            pszDefault[0] != '\'' &&
                            CPLGetValueType(pszDefault) == CPL_VALUE_STRING )
                        {
                            CPLString osDefault("'");
                            char* pszTmp = CPLEscapeString(pszDefault, -1, CPLES_SQL);
                            osDefault += pszTmp;
                            CPLFree(pszTmp);
                            osDefault += "'";
                            poFieldDefn->SetDefault(osDefault);
                        }
                        else if( (poFieldDefn->GetType() == OFTDate || poFieldDefn->GetType() == OFTDateTime) &&
                             !EQUAL(pszDefault, "NULL") &&
                             !STARTS_WITH_CI(pszDefault, "CURRENT_") &&
                             pszDefault[0] != '(' &&
                             pszDefault[0] != '\'' &&
                             !(pszDefault[0] >= '0' && pszDefault[0] <= '9') &&
                            CPLGetValueType(pszDefault) == CPL_VALUE_STRING )
                        {
                            CPLString osDefault("(");
                            osDefault += pszDefault;
                            osDefault += ")";
                            poFieldDefn->SetDefault(osDefault);
                        }
                        else
                            poFieldDefn->SetDefault(pszDefault);
                    }
                }
                if( pszName != nullptr && pszNotNull != nullptr &&
                    EQUAL(pszNotNull, "1") )
                {
                    if( idx >= 0 )
                        m_poFeatureDefn->GetFieldDefn(idx)->SetNullable(0);
                    else
                    {
                        const int geomFieldIdx = m_poFeatureDefn->GetGeomFieldIndex(pszName);
                        if( geomFieldIdx >= 0 )
                            m_poFeatureDefn->GetGeomFieldDefn(geomFieldIdx)->SetNullable(0);
                    }
                }
                if( idx >= 0 &&
                     uniqueFieldsUC.find( CPLString( pszName ).toupper() ) != uniqueFieldsUC.end() )
                {
                    m_poFeatureDefn->GetFieldDefn(idx)->SetUnique(TRUE);
                }

            }
        }
        sqlite3_free_table(papszResult);
    }

    nRowCount = 0;
    nColCount = 0;
    papszResult = nullptr;

    pszSQL = sqlite3_mprintf("SELECT sql FROM sqlite_master WHERE type='table' AND name='%q'", m_pszTableName);
    rc = sqlite3_get_table( hDB, pszSQL, &papszResult, &nRowCount,
                            &nColCount, nullptr );
    sqlite3_free( pszSQL );
    if( rc == SQLITE_OK && nRowCount == 1 && nColCount == 1 )
    {
        const char* pszCreateTableSQL = papszResult[1];
        const char* pszLastParenthesis = strrchr(pszCreateTableSQL, ')');
        if( pszLastParenthesis )
        {
            m_bStrict = CPLString(pszLastParenthesis+1).ifind("STRICT") != std::string::npos;
#ifdef DEBUG_VERBOSE
            if( m_bStrict )
                CPLDebug("SQLite", "Table %s has STRICT mode", m_pszTableName);
#endif
        }

        const int nFieldCount = m_poFeatureDefn->GetFieldCount();
        for( int i = 0; i < nFieldCount; ++i )
        {
            auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
            if( poFieldDefn->GetType() == OFTInteger )
            {
                // In strict mode, the number of allowed types is severely
                // restricted, so interpret INTEGER as Int64 by default, unless
                // a check constraint makes it clear it is Int32
                if( m_bStrict )
                    poFieldDefn->SetType(OFTInteger64);
                if( strstr(pszCreateTableSQL,
                        ("CHECK (\"" +
                         CPLString(poFieldDefn->GetNameRef()).replaceAll('"', "\"\"") +
                         "\" BETWEEN -2147483648 AND 2147483647)").c_str()) )
                {
                    poFieldDefn->SetType(OFTInteger);
                }
                else if( strstr(pszCreateTableSQL,
                        ("CHECK (\"" +
                         CPLString(poFieldDefn->GetNameRef()).replaceAll('"', "\"\"") +
                         "\" BETWEEN -9223372036854775808 AND 9223372036854775807)").c_str()) )
                {
                    poFieldDefn->SetType(OFTInteger64);
                }
            }
            else if( poFieldDefn->GetType() == OFTString )
            {
                if( strstr(pszCreateTableSQL,
                        ("CHECK (\"" +
                         CPLString(poFieldDefn->GetNameRef()).replaceAll('"', "\"\"") +
                         "\" LIKE '[0-9][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T[0-2][0-9]:[0-5][0-9]:[0-6][0-9]*')").c_str()) )
                {
                    poFieldDefn->SetType(OFTDateTime);
                }
                else if( strstr(pszCreateTableSQL,
                        ("CHECK (\"" +
                         CPLString(poFieldDefn->GetNameRef()).replaceAll('"', "\"\"") +
                         "\" LIKE '[0-9][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T')").c_str()) )
                {
                    poFieldDefn->SetType(OFTDate);
                }
                else if( strstr(pszCreateTableSQL,
                        ("CHECK (\"" +
                         CPLString(poFieldDefn->GetNameRef()).replaceAll('"', "\"\"") +
                         "\" LIKE '[0-2][0-9]:[0-5][0-9]:[0-6][0-9]*')").c_str()) )
                {
                    poFieldDefn->SetType(OFTTime);
                }
            }
        }
    }
    sqlite3_free_table(papszResult);

    return CE_None;
}

/************************************************************************/
/*                         RecomputeOrdinals()                          */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::RecomputeOrdinals()
{
    sqlite3 *hDB = m_poDS->GetDB();
    sqlite3_stmt *hColStmt = nullptr;
/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */

    const char *pszSQL =
        CPLSPrintf("SELECT %s* FROM '%s' LIMIT 1",
                   m_pszFIDColumn != nullptr ? "_rowid_, " : "",
                   m_pszEscapedTableName);

    int rc = sqlite3_prepare_v2( hDB, pszSQL, -1, &hColStmt, nullptr );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to query table %s for column definitions : %s.",
                  m_pszTableName, sqlite3_errmsg(hDB) );

        return OGRERR_FAILURE;
    }

    rc = sqlite3_step( hColStmt );
    if ( rc != SQLITE_DONE && rc != SQLITE_ROW )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "In Initialize(): sqlite3_step(%s):\n  %s",
                  pszSQL, sqlite3_errmsg(hDB) );
        sqlite3_finalize( hColStmt );
        return OGRERR_FAILURE;
    }

    int nRawColumns = sqlite3_column_count( hColStmt );

    CPLFree(m_panFieldOrdinals);
    m_panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * m_poFeatureDefn->GetFieldCount() );
    int nCountFieldOrdinals = 0;
    int nCountGeomFieldOrdinals = 0;
    m_iFIDCol = -1;

    for( int iCol = 0; iCol < nRawColumns; iCol++ )
    {
        CPLString osName =
            SQLUnescape(sqlite3_column_name( hColStmt, iCol ));
        int nIdx = m_poFeatureDefn->GetFieldIndex(osName);
        if( m_pszFIDColumn != nullptr && strcmp(osName, m_pszFIDColumn) == 0 )
        {
            if( m_iFIDCol < 0 )
            {
                m_iFIDCol = iCol;
                if( nIdx >= 0 ) /* in case it has also been created as a regular field */
                    nCountFieldOrdinals++;
            }
            continue;
        }
        if( nIdx >= 0 )
        {
            m_panFieldOrdinals[nIdx] = iCol;
            nCountFieldOrdinals++;
        }
        else
        {
            nIdx = m_poFeatureDefn->GetGeomFieldIndex(osName);
            if( nIdx >= 0 )
            {
                OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                    m_poFeatureDefn->myGetGeomFieldDefn(nIdx);
                poGeomFieldDefn->m_iCol = iCol;
                nCountGeomFieldOrdinals++;
            }
        }
    }
    (void)nCountFieldOrdinals;
    CPLAssert(nCountFieldOrdinals == m_poFeatureDefn->GetFieldCount() );
    (void)nCountGeomFieldOrdinals;
    CPLAssert(nCountGeomFieldOrdinals == m_poFeatureDefn->GetGeomFieldCount() );
    CPLAssert(m_pszFIDColumn == nullptr || m_iFIDCol >= 0 );

    sqlite3_finalize( hColStmt );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn* OGRSQLiteTableLayer::GetLayerDefn()
{
    if (m_poFeatureDefn)
        return m_poFeatureDefn;

    EstablishFeatureDefn(nullptr);

    if (m_poFeatureDefn == nullptr)
    {
        m_bLayerDefnError = true;

        m_poFeatureDefn = new OGRSQLiteFeatureDefn( GetDescription() );
        m_poFeatureDefn->Reference();
    }
    else
        LoadStatistics();

    return m_poFeatureDefn;
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::ResetStatement()

{
    CPLString osSQL;

    if( m_bDeferredCreation ) RunDeferredCreationIfNecessary();

    ClearStatement();

    m_iNextShapeId = 0;

    osSQL.Printf( "SELECT %s* FROM '%s' %s",
                  m_pszFIDColumn != nullptr ? "_rowid_, " : "",
                  m_pszEscapedTableName,
                  m_osWHERE.c_str() );
#ifdef DEBUG_VERBOSE
    CPLDebug("SQLite", "%s", osSQL.c_str());
#endif

    const int rc = sqlite3_prepare_v2( m_poDS->GetDB(), osSQL, -1, &m_hStmt, nullptr );
    if( rc == SQLITE_OK )
    {
        return OGRERR_NONE;
    }

    CPLError( CE_Failure, CPLE_AppDefined,
              "In ResetStatement(): sqlite3_prepare_v2(%s):\n  %s",
              osSQL.c_str(), sqlite3_errmsg(m_poDS->GetDB()) );
    m_hStmt = nullptr;
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSQLiteTableLayer::GetNextFeature()

{
    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return nullptr;

    if (HasLayerDefnError())
        return nullptr;

    OGRFeature* poFeature = OGRSQLiteLayer::GetNextFeature();
    if( poFeature && m_iFIDAsRegularColumnIndex >= 0 )
    {
        poFeature->SetField(m_iFIDAsRegularColumnIndex, poFeature->GetFID());
    }
    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRSQLiteTableLayer::GetFeature( GIntBig nFeatureId )

{
    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return nullptr;

    if (HasLayerDefnError())
        return nullptr;

/* -------------------------------------------------------------------- */
/*      If we don't have an explicit FID column, just read through      */
/*      the result set iteratively to find our target.                  */
/* -------------------------------------------------------------------- */
    if( m_pszFIDColumn == nullptr )
        return OGRSQLiteLayer::GetFeature( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Setup explicit query statement to fetch the record we want.     */
/* -------------------------------------------------------------------- */
    CPLString osSQL;

    ClearStatement();

    m_iNextShapeId = nFeatureId;

    osSQL.Printf( "SELECT _rowid_, * FROM '%s' WHERE \"%s\" = " CPL_FRMT_GIB,
                  m_pszEscapedTableName,
                  SQLEscapeLiteral(m_pszFIDColumn).c_str(), nFeatureId );

    CPLDebug( "OGR_SQLITE", "exec(%s)", osSQL.c_str() );

    const int rc = sqlite3_prepare_v2( m_poDS->GetDB(), osSQL,
                                    static_cast<int>(osSQL.size()),
                                    &m_hStmt, nullptr );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "In GetFeature(): sqlite3_prepare_v2(%s):\n  %s",
                  osSQL.c_str(), sqlite3_errmsg(m_poDS->GetDB()) );

        return nullptr;
    }
/* -------------------------------------------------------------------- */
/*      Get the feature if possible.                                    */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = GetNextRawFeature();

    ResetReading();

    return poFeature;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : nullptr;

    if( pszQuery == nullptr )
        m_osQuery = "";
    else
        m_osQuery = pszQuery;

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRSQLiteTableLayer::SetSpatialFilter(  OGRGeometry * poGeomIn )
{
    SetSpatialFilter(0,poGeomIn);
}

void OGRSQLiteTableLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    if( iGeomField == 0 )
    {
        m_iGeomFieldFilter = 0;
    }
    else
    {
        if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
            return;
        }

        m_iGeomFieldFilter = iGeomField;
    }

    if( InstallFilter( poGeomIn ) )
    {
        BuildWhere();

        ResetReading();
    }
}

/************************************************************************/
/*                        CheckSpatialIndexTable()                      */
/************************************************************************/

bool OGRSQLiteTableLayer::CheckSpatialIndexTable(int iGeomCol)
{
    GetLayerDefn();
    if( iGeomCol < 0 || iGeomCol >= m_poFeatureDefn->GetGeomFieldCount() )
        return false;
    OGRSQLiteGeomFieldDefn* poGeomFieldDefn = m_poFeatureDefn->myGetGeomFieldDefn(iGeomCol);
    if (HasSpatialIndex(iGeomCol) && !poGeomFieldDefn->m_bHasCheckedSpatialIndexTable)
    {
        poGeomFieldDefn->m_bHasCheckedSpatialIndexTable = true;
        char **papszResult = nullptr;
        int nRowCount = 0;
        int nColCount = 0;
        char *pszErrMsg = nullptr;

        CPLString osSQL;

        /* This will ensure that RTree support is available */
        osSQL.Printf("SELECT pkid FROM 'idx_%s_%s' WHERE xmax > 0 AND xmin < 0 AND ymax > 0 AND ymin < 0",
                     m_pszEscapedTableName, SQLEscapeLiteral(poGeomFieldDefn->GetNameRef()).c_str());

        int  rc = sqlite3_get_table( m_poDS->GetDB(), osSQL.c_str(),
                                    &papszResult, &nRowCount,
                                    &nColCount, &pszErrMsg );

        if( rc != SQLITE_OK )
        {
            CPLDebug("SQLITE", "Count not find or use idx_%s_%s layer (%s). Disabling spatial index",
                        m_pszEscapedTableName, poGeomFieldDefn->GetNameRef(), pszErrMsg);
            sqlite3_free( pszErrMsg );
            poGeomFieldDefn->m_bHasSpatialIndex = false;
        }
        else
        {
            sqlite3_free_table(papszResult);
        }
    }

    return poGeomFieldDefn->m_bHasSpatialIndex;
}

/************************************************************************/
/*                         HasFastSpatialFilter()                       */
/************************************************************************/

bool OGRSQLiteTableLayer::HasFastSpatialFilter(int iGeomCol)
{
    OGRPolygon oFakePoly;
    const char* pszWKT = "POLYGON((0 0,0 1,1 1,1 0,0 0))";
    oFakePoly.importFromWkt(&pszWKT);
    CPLString    osSpatialWhere = GetSpatialWhere(iGeomCol, &oFakePoly);
    return osSpatialWhere.find("ROWID") == 0;
}

/************************************************************************/
/*                           GetSpatialWhere()                          */
/************************************************************************/

CPLString OGRSQLiteTableLayer::GetSpatialWhere(int iGeomCol,
                                               OGRGeometry* poFilterGeom)
{
    if( !m_poDS->IsSpatialiteDB() ||
        iGeomCol < 0 || iGeomCol >= GetLayerDefn()->GetGeomFieldCount() )
        return "";

    OGRSQLiteGeomFieldDefn* poGeomFieldDefn = m_poFeatureDefn->myGetGeomFieldDefn(iGeomCol);
    if( poFilterGeom != nullptr && CheckSpatialIndexTable(iGeomCol) )
    {
        return FormatSpatialFilterFromRTree(poFilterGeom, "ROWID",
            m_pszEscapedTableName,
            SQLEscapeLiteral(poGeomFieldDefn->GetNameRef()).c_str());
    }

    if( poFilterGeom != nullptr &&
        m_poDS->IsSpatialiteLoaded() && !poGeomFieldDefn->m_bHasSpatialIndex )
    {
        return FormatSpatialFilterFromMBR(poFilterGeom,
            SQLEscapeName(poGeomFieldDefn->GetNameRef()).c_str());
    }

    return "";
}

/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRSQLiteTableLayer::BuildWhere()

{
    m_osWHERE = "";

    CPLString osSpatialWHERE = GetSpatialWhere(m_iGeomFieldFilter,
                                               m_poFilterGeom);
    if (!osSpatialWHERE.empty())
    {
        m_osWHERE = "WHERE ";
        m_osWHERE += osSpatialWHERE;
    }

    if( !m_osQuery.empty() )
    {
        if( m_osWHERE.empty() )
        {
            m_osWHERE = "WHERE ";
            m_osWHERE += m_osQuery;
        }
        else
        {
            m_osWHERE += " AND (";
            m_osWHERE += m_osQuery;
            m_osWHERE += ")";
        }
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteTableLayer::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap,OLCFastFeatureCount))
        return m_poFilterGeom == nullptr || HasSpatialIndex(0);

    else if (EQUAL(pszCap,OLCFastSpatialFilter))
        return HasSpatialIndex(0);

    else if( EQUAL(pszCap,OLCFastGetExtent) )
    {
        return GetLayerDefn()->GetGeomFieldCount() >= 1 &&
               myGetLayerDefn()->myGetGeomFieldDefn(0)->m_bCachedExtentIsValid;
    }

    else if( EQUAL(pszCap,OLCRandomRead) )
        return m_pszFIDColumn != nullptr;

    else if( EQUAL(pszCap,OLCSequentialWrite)
             || EQUAL(pszCap,OLCRandomWrite) )
    {
        return m_poDS->GetUpdate();
    }

    else if( EQUAL(pszCap,OLCDeleteFeature) )
    {
        return m_poDS->GetUpdate() && m_pszFIDColumn != nullptr;
    }

    else if( EQUAL(pszCap,OLCCreateField) ||
             EQUAL(pszCap,OLCCreateGeomField) )
        return m_poDS->GetUpdate();

    else if( EQUAL(pszCap,OLCDeleteField) )
        return m_poDS->GetUpdate();

    else if( EQUAL(pszCap,OLCAlterFieldDefn) )
        return m_poDS->GetUpdate();

    else if( EQUAL(pszCap,OLCReorderFields) )
        return m_poDS->GetUpdate();

    else if( EQUAL(pszCap,OLCCurveGeometries) )
        return m_poDS->TestCapability(ODsCCurveGeometries);

    else if( EQUAL(pszCap,OLCMeasuredGeometries) )
        return m_poDS->TestCapability(ODsCMeasuredGeometries);
    else
        return OGRSQLiteLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRSQLiteTableLayer::GetFeatureCount( int bForce )

{
    if (HasLayerDefnError())
        return 0;

    if( !TestCapability(OLCFastFeatureCount) )
        return OGRSQLiteLayer::GetFeatureCount( bForce );

    if (m_nFeatureCount >= 0 && m_poFilterGeom == nullptr &&
        m_osQuery.empty() )
    {
        return m_nFeatureCount;
    }

/* -------------------------------------------------------------------- */
/*      Form count SQL.                                                 */
/* -------------------------------------------------------------------- */
    const char *pszSQL = nullptr;

    if (m_poFilterGeom != nullptr && CheckSpatialIndexTable(m_iGeomFieldFilter) &&
        m_osQuery.empty())
    {
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );
        const char* pszGeomCol = m_poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter)->GetNameRef();
        pszSQL = CPLSPrintf("SELECT count(*) FROM 'idx_%s_%s' WHERE "
                            "xmax >= %.12f AND xmin <= %.12f AND ymax >= %.12f AND ymin <= %.12f",
                            m_pszEscapedTableName, SQLEscapeLiteral(pszGeomCol).c_str(),
                            sEnvelope.MinX - 1e-11,
                            sEnvelope.MaxX + 1e-11,
                            sEnvelope.MinY - 1e-11,
                            sEnvelope.MaxY + 1e-11);
    }
    else
    {
        pszSQL = CPLSPrintf( "SELECT count(*) FROM '%s' %s",
                            m_pszEscapedTableName, m_osWHERE.c_str() );
    }

    CPLDebug("SQLITE", "Running %s", pszSQL);

/* -------------------------------------------------------------------- */
/*      Execute.                                                        */
/* -------------------------------------------------------------------- */
    OGRErr eErr = OGRERR_NONE;
    GIntBig nResult = SQLGetInteger64( m_poDS->GetDB(), pszSQL, &eErr);
    if( eErr == OGRERR_FAILURE )
    {
        nResult = -1;
    }
    else
    {
        if( m_poFilterGeom == nullptr && m_osQuery.empty() )
        {
            m_nFeatureCount = nResult;
            if( m_poDS->GetUpdate() )
                ForceStatisticsToBeFlushed();
        }
    }

    return nResult;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    return GetExtent(0, psExtent, bForce);
}

OGRErr OGRSQLiteTableLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    if (HasLayerDefnError())
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      If this layer has a none geometry type, then we can             */
/*      reasonably assume there are not extents available.              */
/* -------------------------------------------------------------------- */
    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

    OGRSQLiteGeomFieldDefn* poGeomFieldDefn = m_poFeatureDefn->myGetGeomFieldDefn(iGeomField);
    if (poGeomFieldDefn->m_bCachedExtentIsValid)
    {
        *psExtent = poGeomFieldDefn->m_oCachedExtent;
        return OGRERR_NONE;
    }

    if (CheckSpatialIndexTable(iGeomField) &&
        !CPLTestBool(CPLGetConfigOption("OGR_SQLITE_EXACT_EXTENT", "NO")))
    {
        const char* pszSQL =
            CPLSPrintf(
                "SELECT MIN(xmin), MIN(ymin), "
                "MAX(xmax), MAX(ymax) FROM 'idx_%s_%s'",
                m_pszEscapedTableName,
                SQLEscapeLiteral(poGeomFieldDefn->GetNameRef()).c_str());

        CPLDebug("SQLITE", "Running %s", pszSQL);

/* -------------------------------------------------------------------- */
/*      Execute.                                                        */
/* -------------------------------------------------------------------- */
        char **papszResult = nullptr;
        char *pszErrMsg;
        int nRowCount = 0;
        int nColCount = 0;

        if( sqlite3_get_table( m_poDS->GetDB(), pszSQL, &papszResult,
                               &nRowCount, &nColCount, &pszErrMsg ) != SQLITE_OK )
            return OGRSQLiteLayer::GetExtent(psExtent, bForce);

        OGRErr eErr = OGRERR_FAILURE;

        if( nRowCount == 1 && nColCount == 4 &&
            papszResult[4+0] != nullptr &&
            papszResult[4+1] != nullptr &&
            papszResult[4+2] != nullptr &&
            papszResult[4+3] != nullptr)
        {
            psExtent->MinX = CPLAtof(papszResult[4+0]);
            psExtent->MinY = CPLAtof(papszResult[4+1]);
            psExtent->MaxX = CPLAtof(papszResult[4+2]);
            psExtent->MaxY = CPLAtof(papszResult[4+3]);
            eErr = OGRERR_NONE;

            if( m_poFilterGeom == nullptr && m_osQuery.empty() )
            {
                poGeomFieldDefn->m_bCachedExtentIsValid = true;
                if( m_poDS->GetUpdate() )
                    ForceStatisticsToBeFlushed();
                poGeomFieldDefn->m_oCachedExtent = *psExtent;
            }
        }

        sqlite3_free_table( papszResult );

        if (eErr == OGRERR_NONE)
            return eErr;
    }

    OGRErr eErr;
    if( iGeomField == 0 )
        eErr = OGRSQLiteLayer::GetExtent(psExtent, bForce);
    else
        eErr = OGRSQLiteLayer::GetExtent(iGeomField, psExtent, bForce);
    if( eErr == OGRERR_NONE && m_poFilterGeom == nullptr && m_osQuery.empty() )
    {
        poGeomFieldDefn->m_bCachedExtentIsValid = true;
        ForceStatisticsToBeFlushed();
        poGeomFieldDefn->m_oCachedExtent = *psExtent;
    }
    return eErr;
}

/************************************************************************/
/*                  OGRSQLiteFieldDefnToSQliteFieldDefn()               */
/************************************************************************/

CPLString OGRSQLiteFieldDefnToSQliteFieldDefn( OGRFieldDefn* poFieldDefn,
                                               bool bSQLiteDialectInternalUse,
                                               bool bStrict )
{
    if( bStrict )
    {
        switch( poFieldDefn->GetType() )
        {
            case OFTInteger:
                return "INTEGER CHECK (\"" +
                        CPLString(poFieldDefn->GetNameRef()).replaceAll('"', "\"\"") +
                        "\" BETWEEN -2147483648 AND 2147483647)";
            case OFTInteger64:
                return "INTEGER CHECK (\"" +
                        CPLString(poFieldDefn->GetNameRef()).replaceAll('"', "\"\"") +
                        "\" BETWEEN -9223372036854775808 AND 9223372036854775807)";
            case OFTReal:
                return "REAL";
            case OFTBinary :
                return "BLOB";
            case OFTDateTime:
                return "TEXT CHECK (\"" +
                        CPLString(poFieldDefn->GetNameRef()).replaceAll('"', "\"\"") +
                        "\" LIKE '[0-9][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T[0-2][0-9]:[0-5][0-9]:[0-6][0-9]*')";
            case OFTDate:
                return "TEXT CHECK (\"" +
                        CPLString(poFieldDefn->GetNameRef()).replaceAll('"', "\"\"") +
                        "\" LIKE '[0-9][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T')";
            case OFTTime:
                return "TEXT CHECK (\"" +
                        CPLString(poFieldDefn->GetNameRef()).replaceAll('"', "\"\"") +
                        "\" LIKE '[0-2][0-9]:[0-5][0-9]:[0-6][0-9]*')";
            default:
                return "TEXT";
        }
    }

    switch( poFieldDefn->GetType() )
    {
        case OFTInteger:
            if (poFieldDefn->GetSubType() == OFSTBoolean)
                return "INTEGER_BOOLEAN";
            else if (poFieldDefn->GetSubType() == OFSTInt16 )
                return "INTEGER_INT16";
            else
                return "INTEGER";
            break;
        case OFTInteger64:
            return "BIGINT";
        case OFTReal:
            if (bSQLiteDialectInternalUse && poFieldDefn->GetSubType() == OFSTFloat32)
                return "FLOAT_FLOAT32";
            else
                return "FLOAT";
            break;
        case OFTBinary : return "BLOB"; break;
        case OFTString :
        {
            if( poFieldDefn->GetWidth() > 0 )
                return CPLSPrintf("VARCHAR(%d)", poFieldDefn->GetWidth());
            else
                return "VARCHAR";
            break;
        }
        case OFTDateTime: return "TIMESTAMP"; break;
        case OFTDate    : return "DATE"; break;
        case OFTTime    : return "TIME"; break;
        case OFTIntegerList:
            return "JSONINTEGERLIST";
            break;
        case OFTInteger64List:
            return "JSONINTEGER64LIST";
            break;
        case OFTRealList:
            return "JSONREALLIST";
            break;
        case OFTStringList:
            return "JSONSTRINGLIST";
            break;
        default         : return "VARCHAR"; break;
    }
}

/************************************************************************/
/*                    FieldDefnToSQliteFieldDefn()                      */
/************************************************************************/

CPLString OGRSQLiteTableLayer::FieldDefnToSQliteFieldDefn( OGRFieldDefn* poFieldDefn )
{
    CPLString osRet = OGRSQLiteFieldDefnToSQliteFieldDefn(poFieldDefn, false, m_bStrict);
    if( !m_bStrict && poFieldDefn->GetType() == OFTString &&
        CSLFindString(m_papszCompressedColumns, poFieldDefn->GetNameRef()) >= 0 )
        osRet += "_deflate";

    return osRet;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::CreateField( OGRFieldDefn *poFieldIn,
                                         CPL_UNUSED int bApproxOK )
{
    OGRFieldDefn        oField( poFieldIn );

    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    if (!m_poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "CreateField");
        return OGRERR_FAILURE;
    }

    if( m_pszFIDColumn != nullptr &&
        EQUAL( oField.GetNameRef(), m_pszFIDColumn ) &&
        oField.GetType() != OFTInteger &&
        oField.GetType() != OFTInteger64 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Wrong field type for %s",
                 oField.GetNameRef());
        return OGRERR_FAILURE;
    }

    ClearInsertStmt();

    if( m_poDS->IsSpatialiteDB() && EQUAL( oField.GetNameRef(), "ROWID") &&
        !(m_pszFIDColumn != nullptr && EQUAL( oField.GetNameRef(), m_pszFIDColumn )) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "In a Spatialite DB, a 'ROWID' column that is not the integer "
                 "primary key can corrupt spatial index. "
                 "See https://www.gaia-gis.it/fossil/libspatialite/wiki?name=Shadowed+ROWID+issues");
    }

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into SQLite            */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( m_bLaunderColumnNames )
    {
        char    *pszSafeName = m_poDS->LaunderName( oField.GetNameRef() );

        oField.SetName( pszSafeName );
        CPLFree( pszSafeName );
    }

    if( (oField.GetType() == OFTTime || oField.GetType() == OFTDate ||
         oField.GetType() == OFTDateTime) &&
        !(CPLTestBool(
            CPLGetConfigOption("OGR_SQLITE_ENABLE_DATETIME", "YES"))) )
    {
        oField.SetType(OFTString);
    }

    if( !m_bDeferredCreation )
    {
        CPLString osCommand;

        CPLString osFieldType(FieldDefnToSQliteFieldDefn(&oField));
        osCommand.Printf("ALTER TABLE '%s' ADD COLUMN '%s' %s",
                        m_pszEscapedTableName,
                        SQLEscapeLiteral(oField.GetNameRef()).c_str(),
                        osFieldType.c_str());
        if( !oField.IsNullable() )
        {
            osCommand += " NOT NULL";
        }
        if( oField.IsUnique() )
        {
            osCommand += " UNIQUE";
        }
        if( oField.GetDefault() != nullptr && !oField.IsDefaultDriverSpecific() )
        {
            osCommand += " DEFAULT ";
            osCommand += oField.GetDefault();
        }
        else if( !oField.IsNullable() )
        {
            // This is kind of dumb, but SQLite mandates a DEFAULT value
            // when adding a NOT NULL column in an ALTER TABLE ADD COLUMN
            // statement, which defeats the purpose of NOT NULL,
            // whereas it doesn't in CREATE TABLE
            osCommand += " DEFAULT ''";
        }

    #ifdef DEBUG
        CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
    #endif

        if( SQLCommand( m_poDS->GetDB(), osCommand ) != OGRERR_NONE )
            return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Add the field to the OGRFeatureDefn.                            */
/* -------------------------------------------------------------------- */
    m_poFeatureDefn->AddFieldDefn( &oField );

    if( m_pszFIDColumn != nullptr &&
        EQUAL( oField.GetNameRef(), m_pszFIDColumn ) )
    {
        m_iFIDAsRegularColumnIndex = m_poFeatureDefn->GetFieldCount() - 1;
    }

    if( !m_bDeferredCreation )
        RecomputeOrdinals();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::CreateGeomField( OGRGeomFieldDefn *poGeomFieldIn,
                                             CPL_UNUSED int bApproxOK )
{
    OGRwkbGeometryType eType = poGeomFieldIn->GetType();
    if( eType == wkbNone )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create geometry field of type wkbNone");
        return OGRERR_FAILURE;
    }
    if ( m_poDS->IsSpatialiteDB() )
    {
        // We need to catch this right now as AddGeometryColumn does not
        // return an error
        OGRwkbGeometryType eFType = wkbFlatten(eType);
        if( eFType > wkbGeometryCollection )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "Cannot create geometry field of type %s",
                    OGRToOGCGeomType(eType));
            return OGRERR_FAILURE;
        }
    }

    auto poGeomField =
        cpl::make_unique<OGRSQLiteGeomFieldDefn>( poGeomFieldIn->GetNameRef(), -1 );
    if( EQUAL(poGeomField->GetNameRef(), "") )
    {
        if( m_poFeatureDefn->GetGeomFieldCount() == 0 )
            poGeomField->SetName( "GEOMETRY" );
        else
            poGeomField->SetName(
                CPLSPrintf("GEOMETRY%d", m_poFeatureDefn->GetGeomFieldCount()+1) );
    }
    auto l_poSRS = poGeomFieldIn->GetSpatialRef();
    if( l_poSRS )
    {
        l_poSRS = l_poSRS->Clone();
        l_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        poGeomField->SetSpatialRef(l_poSRS);
        l_poSRS->Release();
    }

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into Postgres          */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( m_bLaunderColumnNames )
    {
        char    *pszSafeName = m_poDS->LaunderName( poGeomField->GetNameRef() );

        poGeomField->SetName( pszSafeName );
        CPLFree( pszSafeName );
    }

    OGRSpatialReference* poSRS = poGeomField->GetSpatialRef();
    int nSRSId = -1;
    if( poSRS != nullptr )
        nSRSId = m_poDS->FetchSRSId( poSRS );

    poGeomField->SetType(eType);
    poGeomField->SetNullable( poGeomFieldIn->IsNullable() );
    poGeomField->m_nSRSId = nSRSId;
    if( m_poDS->IsSpatialiteDB() )
        poGeomField->m_eGeomFormat = OSGF_SpatiaLite;
    else if( m_pszCreationGeomFormat )
        poGeomField->m_eGeomFormat = GetGeomFormat(m_pszCreationGeomFormat);
    else
        poGeomField->m_eGeomFormat = OSGF_WKB ;

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    if( !m_bDeferredCreation )
    {
        if( RunAddGeometryColumn(poGeomField.get(), true) != OGRERR_NONE )
        {
            return OGRERR_FAILURE;
        }
    }

    m_poFeatureDefn->AddGeomFieldDefn( std::move(poGeomField) );

    if( !m_bDeferredCreation )
        RecomputeOrdinals();

    return OGRERR_NONE;
}

/************************************************************************/
/*                        RunAddGeometryColumn()                        */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::RunAddGeometryColumn( const OGRSQLiteGeomFieldDefn *poGeomFieldDefn,
                                                  bool bAddColumnsForNonSpatialite )
{
    OGRwkbGeometryType eType = poGeomFieldDefn->GetType();
    const char* pszGeomCol = poGeomFieldDefn->GetNameRef();
    int nSRSId = poGeomFieldDefn->m_nSRSId;

    const int nCoordDim = eType == wkbFlatten(eType) ? 2 : 3;

    if( bAddColumnsForNonSpatialite && !m_poDS->IsSpatialiteDB() )
    {
        CPLString osCommand = CPLSPrintf("ALTER TABLE '%s' ADD COLUMN ",
                                         m_pszEscapedTableName );
        if( poGeomFieldDefn->m_eGeomFormat == OSGF_WKT )
        {
            osCommand += CPLSPrintf(" '%s' VARCHAR",
                SQLEscapeLiteral(poGeomFieldDefn->GetNameRef()).c_str() );
        }
        else
        {
            osCommand += CPLSPrintf(" '%s' BLOB",
                SQLEscapeLiteral(poGeomFieldDefn->GetNameRef()).c_str() );
        }
        if( !poGeomFieldDefn->IsNullable() )
            osCommand += " NOT NULL DEFAULT ''";

#ifdef DEBUG
        CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
#endif

        if( SQLCommand( m_poDS->GetDB(), osCommand ) != OGRERR_NONE )
            return OGRERR_FAILURE;
    }

    CPLString osCommand;

    if ( m_poDS->IsSpatialiteDB() )
    {
        /*
        / SpatiaLite full support: calling AddGeometryColumn()
        /
        / IMPORTANT NOTICE: on SpatiaLite any attempt aimed
        / to directly INSERT a row into GEOMETRY_COLUMNS
        / [by-passing AddGeometryColumn() as absolutely required]
        / will severely [and irremediably] corrupt the DB !!!
        */
        const char *pszType = OGRToOGCGeomType(eType);
        if (pszType[0] == '\0')
            pszType = "GEOMETRY";

        /*
        / SpatiaLite v.2.4.0 (or any subsequent) is required
        / to support 2.5D: if an obsolete version of the library
        / is found we'll unconditionally activate 2D casting mode
        */
        int iSpatialiteVersion = m_poDS->GetSpatialiteVersionNumber();
        const char* pszCoordDim = "2";
        if ( iSpatialiteVersion < 24 && nCoordDim == 3 )
        {
            CPLDebug("SQLITE", "Spatialite < 2.4.0 --> 2.5D geometry not supported. Casting to 2D");
        }
        else if( OGR_GT_HasM( eType ) )
        {
            pszCoordDim = ( OGR_GT_HasZ( eType ) ) ? "'XYZM'" : "'XYM'";
        }
        else if( OGR_GT_HasZ( eType ) )
        {
            pszCoordDim = "3";
        }
        osCommand.Printf( "SELECT AddGeometryColumn("
                        "'%s', '%s', %d, '%s', %s",
                        m_pszEscapedTableName,
                        SQLEscapeLiteral(pszGeomCol).c_str(), nSRSId,
                        pszType, pszCoordDim );
        if( iSpatialiteVersion >= 30 && !poGeomFieldDefn->IsNullable() )
            osCommand += ", 1";
        osCommand += ")";
    }
    else
    {
        const char* pszGeomFormat =
            (poGeomFieldDefn->m_eGeomFormat == OSGF_WKT ) ? "WKT" :
            (poGeomFieldDefn->m_eGeomFormat == OSGF_WKB ) ? "WKB" :
            (poGeomFieldDefn->m_eGeomFormat == OSGF_FGF ) ? "FGF" :
                                                            "Spatialite";
        if( nSRSId > 0 )
        {
            osCommand.Printf(
                "INSERT INTO geometry_columns "
                "(f_table_name, f_geometry_column, geometry_format, "
                "geometry_type, coord_dimension, srid) VALUES "
                "('%s','%s','%s', %d, %d, %d)",
                m_pszEscapedTableName,
                SQLEscapeLiteral(pszGeomCol).c_str(), pszGeomFormat,
                (int) wkbFlatten(eType), nCoordDim, nSRSId );
        }
        else
        {
            osCommand.Printf(
                "INSERT INTO geometry_columns "
                "(f_table_name, f_geometry_column, geometry_format, "
                "geometry_type, coord_dimension) VALUES "
                "('%s','%s','%s', %d, %d)",
                m_pszEscapedTableName,
                SQLEscapeLiteral(pszGeomCol).c_str(), pszGeomFormat,
                (int) wkbFlatten(eType), nCoordDim );
        }
    }

#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
#endif

    return SQLCommand( m_poDS->GetDB(), osCommand );
}

/************************************************************************/
/*                     InitFieldListForRecrerate()                      */
/************************************************************************/

void OGRSQLiteTableLayer::InitFieldListForRecrerate(char* & pszNewFieldList,
                                                    char* & pszFieldListForSelect,
                                                    size_t& nBufLenOut,
                                                    int nExtraSpace)
{
    size_t nFieldListLen = 100 + 2 * nExtraSpace;

    for( int iField = 0; iField < m_poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn* poFieldDefn = m_poFeatureDefn->GetFieldDefn(iField);
        nFieldListLen +=
            2 * strlen(poFieldDefn->GetNameRef()) + 70;
        nFieldListLen += strlen(" UNIQUE");
        if( poFieldDefn->GetDefault() != nullptr )
            nFieldListLen += 10 + strlen( poFieldDefn->GetDefault() );
    }

    nFieldListLen += 50 + (m_pszFIDColumn ? 2 * strlen(m_pszFIDColumn) : strlen("OGC_FID"));
    for( int iField = 0; iField < m_poFeatureDefn->GetGeomFieldCount(); iField++ )
    {
        nFieldListLen += 70 + 2 * strlen(m_poFeatureDefn->GetGeomFieldDefn(iField)->GetNameRef());
    }

    nBufLenOut = nFieldListLen;
    pszFieldListForSelect = (char *) CPLCalloc(1,nFieldListLen);
    pszNewFieldList = (char *) CPLCalloc(1,nFieldListLen);

/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
    snprintf( pszFieldListForSelect, nFieldListLen, "\"%s\"", m_pszFIDColumn ? SQLEscapeName(m_pszFIDColumn).c_str() : "OGC_FID" );
    snprintf( pszNewFieldList, nFieldListLen, "\"%s\" INTEGER PRIMARY KEY",m_pszFIDColumn ? SQLEscapeName(m_pszFIDColumn).c_str() : "OGC_FID" );

    for( int iField = 0; iField < m_poFeatureDefn->GetGeomFieldCount(); iField++ )
    {
        OGRSQLiteGeomFieldDefn* poGeomFieldDefn = m_poFeatureDefn->myGetGeomFieldDefn(iField);
        strcat( pszFieldListForSelect, "," );
        strcat( pszNewFieldList, "," );

        strcat( pszFieldListForSelect, "\"");
        strcat( pszFieldListForSelect, SQLEscapeName(poGeomFieldDefn->GetNameRef()) );
        strcat( pszFieldListForSelect, "\"");

        strcat( pszNewFieldList, "\"");
        strcat( pszNewFieldList, SQLEscapeName(poGeomFieldDefn->GetNameRef()) );
        strcat( pszNewFieldList, "\"");

        if ( poGeomFieldDefn->m_eGeomFormat == OSGF_WKT )
            strcat( pszNewFieldList, " VARCHAR" );
        else
            strcat( pszNewFieldList, " BLOB" );
        if( !poGeomFieldDefn->IsNullable() )
            strcat( pszNewFieldList, " NOT NULL" );
    }
}

/************************************************************************/
/*                         AddColumnDef()                               */
/************************************************************************/

void OGRSQLiteTableLayer::AddColumnDef(char* pszNewFieldList, size_t nBufLen,
                                       OGRFieldDefn* poFldDefn)
{
    snprintf( pszNewFieldList+strlen(pszNewFieldList), nBufLen-strlen(pszNewFieldList),
             ", '%s' %s", SQLEscapeLiteral(poFldDefn->GetNameRef()).c_str(),
             FieldDefnToSQliteFieldDefn(poFldDefn).c_str() );
    if( !poFldDefn->IsNullable() )
        snprintf( pszNewFieldList+strlen(pszNewFieldList),
                 nBufLen-strlen(pszNewFieldList), " NOT NULL" );
    if( poFldDefn->IsUnique() )
        snprintf( pszNewFieldList+strlen(pszNewFieldList),
                 nBufLen-strlen(pszNewFieldList), " UNIQUE" );
    if( poFldDefn->GetDefault() != nullptr && !poFldDefn->IsDefaultDriverSpecific() )
    {
        snprintf( pszNewFieldList+strlen(pszNewFieldList),
                 nBufLen-strlen(pszNewFieldList), " DEFAULT %s",
                 poFldDefn->GetDefault() );
    }
}

/************************************************************************/
/*                           RecreateTable()                            */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::RecreateTable(const char* pszFieldListForSelect,
                                          const char* pszNewFieldList,
                                          const char* pszGenericErrorMessage)
{
/* -------------------------------------------------------------------- */
/*      Do this all in a transaction.                                   */
/* -------------------------------------------------------------------- */
    m_poDS->SoftStartTransaction();

/* -------------------------------------------------------------------- */
/*      Save existing related triggers and index                        */
/* -------------------------------------------------------------------- */
    char *pszErrMsg = nullptr;
    sqlite3 *hDB = m_poDS->GetDB();
    CPLString osSQL;

    osSQL.Printf( "SELECT sql FROM sqlite_master WHERE type IN ('trigger','index') AND tbl_name='%s'",
                   m_pszEscapedTableName );

    int nRowTriggerIndexCount, nColTriggerIndexCount;
    char **papszTriggerIndexResult = nullptr;
    int rc =
        sqlite3_get_table( hDB, osSQL.c_str(), &papszTriggerIndexResult,
                           &nRowTriggerIndexCount, &nColTriggerIndexCount,
                           &pszErrMsg );

/* -------------------------------------------------------------------- */
/*      Make a backup of the table.                                     */
/* -------------------------------------------------------------------- */

    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB,
                       CPLSPrintf( "CREATE TABLE t1_back(%s)%s",
                                   pszNewFieldList,
                                   m_bStrict ? " STRICT": "" ),
                       nullptr, nullptr, &pszErrMsg );

    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB,
                           CPLSPrintf( "INSERT INTO t1_back SELECT %s FROM '%s'",
                                       pszFieldListForSelect,
                                       m_pszEscapedTableName ),
                           nullptr, nullptr, &pszErrMsg );

/* -------------------------------------------------------------------- */
/*      Drop the original table                                         */
/* -------------------------------------------------------------------- */
    if( rc == SQLITE_OK )
        rc = sqlite3_exec( hDB,
                           CPLSPrintf( "DROP TABLE '%s'",
                                       m_pszEscapedTableName ),
                           nullptr, nullptr, &pszErrMsg );

/* -------------------------------------------------------------------- */
/*      Rename backup table as new table                                */
/* -------------------------------------------------------------------- */
    if( rc == SQLITE_OK )
    {
        const char *pszCmd =
            CPLSPrintf( "ALTER TABLE t1_back RENAME TO '%s'",
                        m_pszEscapedTableName);
        rc = sqlite3_exec( hDB, pszCmd,
                           nullptr, nullptr, &pszErrMsg );
    }

/* -------------------------------------------------------------------- */
/*      Recreate existing related tables, triggers and index            */
/* -------------------------------------------------------------------- */

    if( rc == SQLITE_OK )
    {
        for( int i = 1;
             i <= nRowTriggerIndexCount &&
             nColTriggerIndexCount == 1 &&
             rc == SQLITE_OK;
             i++)
        {
            if (papszTriggerIndexResult[i] != nullptr && papszTriggerIndexResult[i][0] != '\0')
                rc = sqlite3_exec( hDB,
                            papszTriggerIndexResult[i],
                            nullptr, nullptr, &pszErrMsg );
        }
    }

/* -------------------------------------------------------------------- */
/*      COMMIT on success or ROLLBACK on failure.                       */
/* -------------------------------------------------------------------- */

    sqlite3_free_table( papszTriggerIndexResult );

    if( rc == SQLITE_OK )
    {
        m_poDS->SoftCommitTransaction();

        return OGRERR_NONE;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s:\n %s",
                  pszGenericErrorMessage,
                  pszErrMsg );
        sqlite3_free( pszErrMsg );

        m_poDS->SoftRollbackTransaction();

        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                             DeleteField()                            */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::DeleteField( int iFieldToDelete )
{
    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    if (!m_poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "DeleteField");
        return OGRERR_FAILURE;
    }

    if (iFieldToDelete < 0 || iFieldToDelete >= m_poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    ResetReading();

    if( m_poDS->SoftStartTransaction() != OGRERR_NONE )
        return OGRERR_FAILURE;

    // ALTER TABLE ... DROP COLUMN ... was first implemented in 3.35.0 but
    // there was bug fixes related to it until 3.35.5
#if SQLITE_VERSION_NUMBER >= 3035005L
    const char* pszFieldName =
        m_poFeatureDefn->GetFieldDefn(iFieldToDelete)->GetNameRef();
    OGRErr eErr = SQLCommand( m_poDS->GetDB(),
                       CPLString().Printf("ALTER TABLE \"%s\" DROP COLUMN \"%s\"",
                          SQLEscapeName(m_pszTableName).c_str(),
                          SQLEscapeName(pszFieldName).c_str()).c_str() );
#else
/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
    char *pszNewFieldList = nullptr;
    char *pszFieldListForSelect = nullptr;
    size_t nBufLen = 0;

    InitFieldListForRecrerate(pszNewFieldList, pszFieldListForSelect, nBufLen);

    for( int iField = 0; iField < m_poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFldDefn = m_poFeatureDefn->GetFieldDefn(iField);

        if (iField == iFieldToDelete)
            continue;

        snprintf( pszFieldListForSelect+strlen(pszFieldListForSelect),
                  nBufLen-strlen(pszFieldListForSelect),
                 ", \"%s\"", SQLEscapeName(poFldDefn->GetNameRef()).c_str() );

        AddColumnDef(pszNewFieldList, nBufLen, poFldDefn);
    }

/* -------------------------------------------------------------------- */
/*      Recreate table.                                                 */
/* -------------------------------------------------------------------- */
    CPLString osErrorMsg;
    osErrorMsg.Printf("Failed to remove field %s from table %s",
                  m_poFeatureDefn->GetFieldDefn(iFieldToDelete)->GetNameRef(),
                  m_poFeatureDefn->GetName());

    OGRErr eErr = RecreateTable(pszFieldListForSelect,
                                pszNewFieldList,
                                osErrorMsg.c_str());

    CPLFree( pszFieldListForSelect );
    CPLFree( pszNewFieldList );
#endif

/* -------------------------------------------------------------------- */
/*      Check foreign key integrity if enforcement of foreign keys      */
/*      constraint is enabled.                                          */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE &&
        SQLGetInteger(m_poDS->GetDB(), "PRAGMA foreign_keys", nullptr) )
    {
        CPLDebug("SQLite", "Running PRAGMA foreign_key_check");
        eErr = m_poDS->PragmaCheck("foreign_key_check", "", 0);
    }

/* -------------------------------------------------------------------- */
/*      Finish                                                          */
/* -------------------------------------------------------------------- */

    if( eErr == OGRERR_NONE)
    {
        eErr = m_poDS->SoftCommitTransaction();
        if( eErr == OGRERR_NONE)
        {
            eErr = m_poFeatureDefn->DeleteFieldDefn( iFieldToDelete );

            RecomputeOrdinals();

            ResetReading();
        }
    }
    else
    {
        m_poDS->SoftRollbackTransaction();
    }

    return eErr;
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::AlterFieldDefn( int iFieldToAlter, OGRFieldDefn* poNewFieldDefn, int nFlagsIn )
{
    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    if (!m_poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "AlterFieldDefn");
        return OGRERR_FAILURE;
    }

    if (iFieldToAlter < 0 || iFieldToAlter >= m_poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    ClearInsertStmt();
    ResetReading();

/* -------------------------------------------------------------------- */
/*      Check that the new column name is not a duplicate.              */
/* -------------------------------------------------------------------- */

    OGRFieldDefn* poFieldDefnToAlter = m_poFeatureDefn->GetFieldDefn(iFieldToAlter);
    const CPLString osOldColName( poFieldDefnToAlter->GetNameRef() );
    const CPLString osNewColName( (nFlagsIn & ALTER_NAME_FLAG) ?
                                  CPLString(poNewFieldDefn->GetNameRef()) :
                                  osOldColName );

    const bool bRenameCol = osOldColName != osNewColName;
    if( bRenameCol )
    {
        if( (m_pszFIDColumn &&
             strcmp(poNewFieldDefn->GetNameRef(), m_pszFIDColumn) == 0) ||
            (GetGeomType() != wkbNone &&
             strcmp(poNewFieldDefn->GetNameRef(),
                    m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef()) == 0) ||
            m_poFeatureDefn->GetFieldIndex(poNewFieldDefn->GetNameRef()) >= 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Field name %s is already used for another field",
                      poNewFieldDefn->GetNameRef());
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Build the modified field definition from the flags.             */
/* -------------------------------------------------------------------- */
    OGRFieldDefn oTmpFieldDefn(poFieldDefnToAlter);
    int nActualFlags = 0;
    if( bRenameCol )
    {
        nActualFlags |= ALTER_NAME_FLAG;
        oTmpFieldDefn.SetName(poNewFieldDefn->GetNameRef());
    }
    if( (nFlagsIn & ALTER_TYPE_FLAG) != 0 &&
        (poFieldDefnToAlter->GetType() != poNewFieldDefn->GetType() ||
         poFieldDefnToAlter->GetSubType() != poNewFieldDefn->GetSubType()) )
    {
        nActualFlags |= ALTER_TYPE_FLAG;
        oTmpFieldDefn.SetSubType(OFSTNone);
        oTmpFieldDefn.SetType(poNewFieldDefn->GetType());
        oTmpFieldDefn.SetSubType(poNewFieldDefn->GetSubType());
    }
    if ( (nFlagsIn & ALTER_WIDTH_PRECISION_FLAG) != 0 &&
         (poFieldDefnToAlter->GetWidth() != poNewFieldDefn->GetWidth() ||
          poFieldDefnToAlter->GetPrecision() != poNewFieldDefn->GetPrecision()) )
    {
        nActualFlags |= ALTER_WIDTH_PRECISION_FLAG;
        oTmpFieldDefn.SetWidth(poNewFieldDefn->GetWidth());
        oTmpFieldDefn.SetPrecision(poNewFieldDefn->GetPrecision());
    }
    if( (nFlagsIn & ALTER_NULLABLE_FLAG) != 0 &&
        poFieldDefnToAlter->IsNullable() != poNewFieldDefn->IsNullable() )
    {
        nActualFlags |= ALTER_NULLABLE_FLAG;
        oTmpFieldDefn.SetNullable(poNewFieldDefn->IsNullable());
    }
    if( (nFlagsIn & ALTER_DEFAULT_FLAG) != 0 &&
        !( (poFieldDefnToAlter->GetDefault() == nullptr && poNewFieldDefn->GetDefault() == nullptr) ||
           (poFieldDefnToAlter->GetDefault() != nullptr && poNewFieldDefn->GetDefault() != nullptr &&
            strcmp(poFieldDefnToAlter->GetDefault(), poNewFieldDefn->GetDefault()) == 0) ) )
    {
        nActualFlags |= ALTER_DEFAULT_FLAG;
        oTmpFieldDefn.SetDefault(poNewFieldDefn->GetDefault());
    }
    if( (nFlagsIn & ALTER_UNIQUE_FLAG) != 0 &&
        poFieldDefnToAlter->IsUnique() != poNewFieldDefn->IsUnique() )
    {
        nActualFlags |= ALTER_UNIQUE_FLAG;
        oTmpFieldDefn.SetUnique( poNewFieldDefn->IsUnique());
    }

    // ALTER TABLE ... RENAME COLUMN ... was first implemented in 3.25.0 but
    // 3.26.0 was required so that foreign key constraints are updated as well
#if SQLITE_VERSION_NUMBER >= 3026000L
    if( nActualFlags == ALTER_NAME_FLAG )
    {
        CPLDebug("SQLite", "Running ALTER TABLE RENAME COLUMN");
        OGRErr eErr = SQLCommand( m_poDS->GetDB(),
                   CPLString().Printf("ALTER TABLE \"%s\" RENAME COLUMN \"%s\" TO \"%s\"",
                      SQLEscapeName(m_pszTableName).c_str(),
                      SQLEscapeName(osOldColName).c_str(),
                      SQLEscapeName(osNewColName).c_str()).c_str() );

        if (eErr != OGRERR_NONE)
            return eErr;
    }
    else
#endif
    {
/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
        char *pszNewFieldList = nullptr;
        char *pszFieldListForSelect = nullptr;
        size_t nBufLen = 0;

        InitFieldListForRecrerate(pszNewFieldList, pszFieldListForSelect,
                                  nBufLen,
                                  static_cast<int>(strlen(poNewFieldDefn->GetNameRef())) +
                                  50 +
                                  (poNewFieldDefn->GetDefault() ? static_cast<int>(strlen(poNewFieldDefn->GetDefault())) : 0)
                                  );

        for( int iField = 0; iField < m_poFeatureDefn->GetFieldCount(); iField++ )
        {
            OGRFieldDefn *poFldDefn = m_poFeatureDefn->GetFieldDefn(iField);

            snprintf( pszFieldListForSelect+strlen(pszFieldListForSelect),
                     nBufLen-strlen(pszFieldListForSelect),
                     ", \"%s\"", SQLEscapeName(poFldDefn->GetNameRef()).c_str() );

            if (iField == iFieldToAlter)
            {
                snprintf( pszNewFieldList+strlen(pszNewFieldList),
                          nBufLen-strlen(pszNewFieldList),
                        ", '%s' %s",
                        SQLEscapeLiteral(oTmpFieldDefn.GetNameRef()).c_str(),
                        FieldDefnToSQliteFieldDefn(&oTmpFieldDefn).c_str() );
                if ( (nFlagsIn & ALTER_NAME_FLAG) &&
                     oTmpFieldDefn.GetType() == OFTString &&
                     CSLFindString(m_papszCompressedColumns, poFldDefn->GetNameRef()) >= 0 )
                {
                    snprintf( pszNewFieldList+strlen(pszNewFieldList),
                             nBufLen-strlen(pszNewFieldList), "_deflate");
                }
                if( !oTmpFieldDefn.IsNullable() )
                    snprintf( pszNewFieldList+strlen(pszNewFieldList),
                              nBufLen-strlen(pszNewFieldList)," NOT NULL" );
                if( oTmpFieldDefn.IsUnique() )
                    snprintf( pszNewFieldList+strlen(pszNewFieldList),
                              nBufLen-strlen(pszNewFieldList)," UNIQUE" );
                if( oTmpFieldDefn.GetDefault() )
                {
                    snprintf( pszNewFieldList+strlen(pszNewFieldList),
                             nBufLen-strlen(pszNewFieldList)," DEFAULT %s",
                             oTmpFieldDefn.GetDefault());
                }
            }
            else
            {
                AddColumnDef(pszNewFieldList, nBufLen, poFldDefn);
            }
        }

/* -------------------------------------------------------------------- */
/*      Recreate table.                                                 */
/* -------------------------------------------------------------------- */
        CPLString osErrorMsg;
        osErrorMsg.Printf("Failed to alter field %s from table %s",
                      m_poFeatureDefn->GetFieldDefn(iFieldToAlter)->GetNameRef(),
                      m_poFeatureDefn->GetName());

        OGRErr eErr = RecreateTable(pszFieldListForSelect,
                                    pszNewFieldList,
                                    osErrorMsg.c_str());

        CPLFree( pszFieldListForSelect );
        CPLFree( pszNewFieldList );

        if (eErr != OGRERR_NONE)
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Finish                                                          */
/* -------------------------------------------------------------------- */

    OGRFieldDefn* poFieldDefn = m_poFeatureDefn->GetFieldDefn(iFieldToAlter);

    if (nActualFlags & ALTER_TYPE_FLAG)
    {
        int iIdx = 0;
        if( poNewFieldDefn->GetType() != OFTString &&
            (iIdx = CSLFindString(m_papszCompressedColumns,
                                  poFieldDefn->GetNameRef())) >= 0 )
        {
            m_papszCompressedColumns = CSLRemoveStrings(m_papszCompressedColumns,
                                                      iIdx, 1, nullptr);
        }
        poFieldDefn->SetSubType(OFSTNone);
        poFieldDefn->SetType(poNewFieldDefn->GetType());
        poFieldDefn->SetSubType(poNewFieldDefn->GetSubType());
    }
    if (nActualFlags & ALTER_NAME_FLAG)
    {
        const int iIdx = CSLFindString(m_papszCompressedColumns,
                                       poFieldDefn->GetNameRef());
        if( iIdx >= 0 )
        {
            CPLFree(m_papszCompressedColumns[iIdx]);
            m_papszCompressedColumns[iIdx] =
                CPLStrdup(poNewFieldDefn->GetNameRef());
        }
        poFieldDefn->SetName(poNewFieldDefn->GetNameRef());
    }
    if (nActualFlags & ALTER_WIDTH_PRECISION_FLAG)
    {
        poFieldDefn->SetWidth(poNewFieldDefn->GetWidth());
        poFieldDefn->SetPrecision(poNewFieldDefn->GetPrecision());
    }
    if (nActualFlags & ALTER_NULLABLE_FLAG)
        poFieldDefn->SetNullable(poNewFieldDefn->IsNullable());
    if (nActualFlags & ALTER_DEFAULT_FLAG)
        poFieldDefn->SetDefault(poNewFieldDefn->GetDefault());

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ReorderFields()                            */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::ReorderFields( int* panMap )
{
    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    if (!m_poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "ReorderFields");
        return OGRERR_FAILURE;
    }

    if (m_poFeatureDefn->GetFieldCount() == 0)
        return OGRERR_NONE;

    OGRErr eErr = OGRCheckPermutation(panMap, m_poFeatureDefn->GetFieldCount());
    if (eErr != OGRERR_NONE)
        return eErr;

    ClearInsertStmt();
    ResetReading();

/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
    char *pszNewFieldList = nullptr;
    char *pszFieldListForSelect = nullptr;
    size_t nBufLen = 0;

    InitFieldListForRecrerate(pszNewFieldList, pszFieldListForSelect, nBufLen);

    for( int iField = 0; iField < m_poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFldDefn = m_poFeatureDefn->GetFieldDefn(panMap[iField]);

        snprintf( pszFieldListForSelect+strlen(pszFieldListForSelect),
                  nBufLen - strlen(pszFieldListForSelect),
                 ", \"%s\"", SQLEscapeName(poFldDefn->GetNameRef()).c_str() );

        AddColumnDef(pszNewFieldList, nBufLen, poFldDefn);
    }

/* -------------------------------------------------------------------- */
/*      Recreate table.                                                 */
/* -------------------------------------------------------------------- */
    CPLString osErrorMsg;
    osErrorMsg.Printf("Failed to reorder fields from table %s",
                  m_poFeatureDefn->GetName());

    eErr = RecreateTable(pszFieldListForSelect,
                                pszNewFieldList,
                                osErrorMsg.c_str());

    CPLFree( pszFieldListForSelect );
    CPLFree( pszNewFieldList );

    if (eErr != OGRERR_NONE)
        return eErr;

/* -------------------------------------------------------------------- */
/*      Finish                                                          */
/* -------------------------------------------------------------------- */

    eErr = m_poFeatureDefn->ReorderFieldDefns( panMap );

    RecomputeOrdinals();

    return eErr;
}

/************************************************************************/
/*                             BindValues()                             */
/************************************************************************/

/* the bBindNullValues is set to TRUE by SetFeature() for UPDATE statements, */
/* and to FALSE by CreateFeature() for INSERT statements; */

OGRErr OGRSQLiteTableLayer::BindValues( OGRFeature *poFeature,
                                        sqlite3_stmt* m_hStmtIn,
                                        bool bBindUnsetAsNull )
{
    sqlite3 *hDB = m_poDS->GetDB();

/* -------------------------------------------------------------------- */
/*      Bind the geometry                                               */
/* -------------------------------------------------------------------- */
    int nBindField = 1;
    int nFieldCount = m_poFeatureDefn->GetGeomFieldCount();
    for( int iField = 0; iField < nFieldCount; iField++ )
    {
        OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
            m_poFeatureDefn->myGetGeomFieldDefn(iField);
        OGRSQLiteGeomFormat eGeomFormat = poGeomFieldDefn->m_eGeomFormat;
        if( eGeomFormat == OSGF_FGF )
            continue;
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(iField);
        int rc = SQLITE_OK;
        if ( poGeom != nullptr )
        {
            if ( eGeomFormat == OSGF_WKT )
            {
                char *pszWKT = nullptr;
                poGeom->exportToWkt( &pszWKT );
                rc = sqlite3_bind_text( m_hStmtIn, nBindField++, pszWKT, -1, CPLFree );
            }
            else if( eGeomFormat == OSGF_WKB )
            {
                const size_t nWKBLen = poGeom->WkbSize();
                if( nWKBLen > static_cast<size_t>(std::numeric_limits<int>::max()) )
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "Too large geometry");
                    return OGRERR_FAILURE;
                }
                GByte *pabyWKB = (GByte *) VSI_MALLOC_VERBOSE(nWKBLen);
                if( pabyWKB )
                {
                    poGeom->exportToWkb( wkbNDR, pabyWKB );
                    rc = sqlite3_bind_blob( m_hStmtIn, nBindField++, pabyWKB, static_cast<int>(nWKBLen), CPLFree );
                }
                else
                {
                    return OGRERR_FAILURE;
                }
            }
            else if ( eGeomFormat == OSGF_SpatiaLite )
            {
                int nBLOBLen = 0;
                GByte *pabySLBLOB = nullptr;

                const int nSRSId = poGeomFieldDefn->m_nSRSId;
                CPL_IGNORE_RET_VAL(
                    ExportSpatiaLiteGeometry(
                        poGeom, nSRSId, wkbNDR,
                        m_bSpatialite2D, m_bUseComprGeom, &pabySLBLOB, &nBLOBLen ));
                rc = sqlite3_bind_blob( m_hStmtIn, nBindField++, pabySLBLOB,
                                        nBLOBLen, CPLFree );
            }
            else
            {
                rc = SQLITE_OK;
                CPL_IGNORE_RET_VAL(rc);
                CPLAssert(false);
            }
        }
        else
        {
            rc = sqlite3_bind_null( m_hStmtIn, nBindField++ );
        }

        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "sqlite3_bind_blob/text() failed:\n  %s",
                      sqlite3_errmsg(hDB) );
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Bind field values.                                              */
/* -------------------------------------------------------------------- */
    nFieldCount = m_poFeatureDefn->GetFieldCount();
    for( int iField = 0; iField < nFieldCount; iField++ )
    {
        if( iField == m_iFIDAsRegularColumnIndex )
            continue;
        if( !bBindUnsetAsNull && !poFeature->IsFieldSet(iField) )
            continue;

        int rc = SQLITE_OK;

        if( (bBindUnsetAsNull && !poFeature->IsFieldSet(iField)) ||
            poFeature->IsFieldNull( iField ) )
        {
            rc = sqlite3_bind_null( m_hStmtIn, nBindField++ );
        }
        else
        {
            OGRFieldDefn* poFieldDefn = m_poFeatureDefn->GetFieldDefn(iField);
            switch( poFieldDefn->GetType() )
            {
                case OFTInteger:
                {
                    int nFieldVal = poFeature->GetFieldAsInteger( iField );
                    rc = sqlite3_bind_int(m_hStmtIn, nBindField++, nFieldVal);
                    break;
                }

                case OFTInteger64:
                {
                    GIntBig nFieldVal = poFeature->GetFieldAsInteger64( iField );
                    rc = sqlite3_bind_int64(m_hStmtIn, nBindField++, nFieldVal);
                    break;
                }

                case OFTReal:
                {
                    double dfFieldVal = poFeature->GetFieldAsDouble( iField );
                    rc = sqlite3_bind_double(m_hStmtIn, nBindField++, dfFieldVal);
                    break;
                }

                case OFTBinary:
                {
                    int nDataLength = 0;
                    GByte* pabyData =
                        poFeature->GetFieldAsBinary( iField, &nDataLength );
                    rc = sqlite3_bind_blob(m_hStmtIn, nBindField++,
                                        pabyData, nDataLength, SQLITE_TRANSIENT);
                    break;
                }

                case OFTDateTime:
                {
                    char* pszStr = OGRGetXMLDateTime(poFeature->GetRawFieldRef(iField));
                    rc = sqlite3_bind_text(m_hStmtIn, nBindField++,
                                           pszStr, -1, SQLITE_TRANSIENT);
                    CPLFree(pszStr);
                    break;
                }

                case OFTDate:
                {
                    int nYear = 0;
                    int nMonth = 0;
                    int nDay = 0;
                    int nHour = 0;
                    int nMinute = 0;
                    int nSecond = 0;
                    int nTZ = 0;
                    poFeature->GetFieldAsDateTime(iField, &nYear, &nMonth, &nDay,
                                                &nHour, &nMinute, &nSecond, &nTZ);
                    char szBuffer[64];
                    snprintf(szBuffer, sizeof(szBuffer), "%04d-%02d-%02d", nYear, nMonth, nDay);
                    rc = sqlite3_bind_text(m_hStmtIn, nBindField++,
                                           szBuffer, -1, SQLITE_TRANSIENT);
                    break;
                }

                case OFTTime:
                {
                    int nYear = 0;
                    int nMonth = 0;
                    int nDay = 0;
                    int nHour = 0;
                    int nMinute = 0;
                    int nTZ = 0;
                    float fSecond = 0.0f;
                    poFeature->GetFieldAsDateTime(iField, &nYear, &nMonth, &nDay,
                                                &nHour, &nMinute, &fSecond, &nTZ );
                    char szBuffer[64];
                    if( OGR_GET_MS(fSecond) != 0 )
                        snprintf(szBuffer, sizeof(szBuffer), "%02d:%02d:%06.3f", nHour, nMinute, fSecond);
                    else
                        snprintf(szBuffer, sizeof(szBuffer), "%02d:%02d:%02d", nHour, nMinute, (int)fSecond);
                    rc = sqlite3_bind_text(m_hStmtIn, nBindField++,
                                           szBuffer, -1, SQLITE_TRANSIENT);
                    break;
                }

                case OFTStringList:
                case OFTIntegerList:
                case OFTInteger64List:
                case OFTRealList:
                {
                    char* pszJSon = poFeature->GetFieldAsSerializedJSon(iField);
                    rc = sqlite3_bind_text(m_hStmtIn, nBindField++,
                                               pszJSon, -1, SQLITE_TRANSIENT);
                    CPLFree(pszJSon);
                    break;
                }

                default:
                {
                    const char *pszRawValue =
                        poFeature->GetFieldAsString( iField );
                    if( CSLFindString(m_papszCompressedColumns,
                                      m_poFeatureDefn->GetFieldDefn(iField)->GetNameRef()) >= 0 )
                    {
                        size_t nBytesOut = 0;
                        void* pOut = CPLZLibDeflate( pszRawValue,
                                                     strlen(pszRawValue), -1,
                                                     nullptr, 0,
                                                     &nBytesOut );
                        if( pOut != nullptr )
                        {
                            rc = sqlite3_bind_blob(m_hStmtIn, nBindField++,
                                                   pOut,
                                                   static_cast<int>(nBytesOut),
                                                   CPLFree);
                        }
                        else
                            rc = SQLITE_ERROR;
                    }
                    else
                    {
                        rc = sqlite3_bind_text(m_hStmtIn, nBindField++,
                                               pszRawValue, -1, SQLITE_TRANSIENT);
                    }
                    break;
                }
            }
        }

        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "sqlite3_bind_() for column %s failed:\n  %s",
                      m_poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                      sqlite3_errmsg(hDB) );
            return OGRERR_FAILURE;
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             ISetFeature()                             */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::ISetFeature( OGRFeature *poFeature )

{
    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    if( m_pszFIDColumn == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "SetFeature() without any FID column." );
        return OGRERR_FAILURE;
    }

    if( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "SetFeature() with unset FID fails." );
        return OGRERR_FAILURE;
    }

    if (!m_poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "SetFeature");
        return OGRERR_FAILURE;
    }

    /* In case the FID column has also been created as a regular field */
    if( m_iFIDAsRegularColumnIndex >= 0 )
    {
        if( !poFeature->IsFieldSetAndNotNull( m_iFIDAsRegularColumnIndex ) ||
            poFeature->GetFieldAsInteger64(m_iFIDAsRegularColumnIndex) != poFeature->GetFID() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Inconsistent values of FID and field of same name");
            return OGRERR_FAILURE;
        }
    }

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    sqlite3 *hDB = m_poDS->GetDB();
    int            bNeedComma = false;

/* -------------------------------------------------------------------- */
/*      Form the UPDATE command.                                        */
/* -------------------------------------------------------------------- */
    CPLString osCommand = CPLSPrintf( "UPDATE '%s' SET ", m_pszEscapedTableName );

/* -------------------------------------------------------------------- */
/*      Add geometry field name.                                        */
/* -------------------------------------------------------------------- */
    int nFieldCount = m_poFeatureDefn->GetGeomFieldCount();
    for( int iField = 0; iField < nFieldCount; iField++ )
    {
        OGRSQLiteGeomFormat eGeomFormat =
            m_poFeatureDefn->myGetGeomFieldDefn(iField)->m_eGeomFormat;
        if( eGeomFormat == OSGF_FGF )
            continue;
        if( bNeedComma )
            osCommand += ",";

        osCommand += "\"";
        osCommand += SQLEscapeName( m_poFeatureDefn->GetGeomFieldDefn(iField)->GetNameRef());
        osCommand += "\" = ?";

        bNeedComma = true;
    }

/* -------------------------------------------------------------------- */
/*      Add field names.                                                */
/* -------------------------------------------------------------------- */
    nFieldCount = m_poFeatureDefn->GetFieldCount();
    for( int iField = 0; iField < nFieldCount; iField++ )
    {
        if( iField == m_iFIDAsRegularColumnIndex )
            continue;
        if( !poFeature->IsFieldSet(iField) )
            continue;
        if( bNeedComma )
            osCommand += ",";

        osCommand += "\"";
        osCommand += SQLEscapeName(m_poFeatureDefn->GetFieldDefn(iField)->GetNameRef());
        osCommand += "\" = ?";

        bNeedComma = true;
    }

    if (!bNeedComma)
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Merge final command.                                            */
/* -------------------------------------------------------------------- */
    osCommand += " WHERE \"";
    osCommand += SQLEscapeName(m_pszFIDColumn);
    osCommand += CPLSPrintf("\" = " CPL_FRMT_GIB, poFeature->GetFID());

/* -------------------------------------------------------------------- */
/*      Prepare the statement.                                          */
/* -------------------------------------------------------------------- */
#ifdef DEBUG_VERBOSE
    CPLDebug( "OGR_SQLITE", "prepare_v2(%s)", osCommand.c_str() );
#endif

    sqlite3_stmt *hUpdateStmt = nullptr;
    int rc = sqlite3_prepare_v2( hDB, osCommand, -1, &hUpdateStmt, nullptr );

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "In SetFeature(): sqlite3_prepare_v2(%s):\n  %s",
                  osCommand.c_str(), sqlite3_errmsg(hDB) );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Bind values.                                                   */
/* -------------------------------------------------------------------- */
    OGRErr eErr = BindValues( poFeature, hUpdateStmt, false );
    if (eErr != OGRERR_NONE)
    {
        sqlite3_finalize( hUpdateStmt );
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Execute the update.                                             */
/* -------------------------------------------------------------------- */
    rc = sqlite3_step( hUpdateStmt );

    if( rc != SQLITE_OK && rc != SQLITE_DONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_step() failed:\n  %s",
                  sqlite3_errmsg(hDB) );

        sqlite3_finalize( hUpdateStmt );
        return OGRERR_FAILURE;
    }

    sqlite3_finalize( hUpdateStmt );

    eErr = (sqlite3_changes(hDB) > 0) ? OGRERR_NONE : OGRERR_NON_EXISTING_FEATURE;
    if( eErr == OGRERR_NONE )
    {
        nFieldCount = m_poFeatureDefn->GetGeomFieldCount();
        for( int iField = 0; iField < nFieldCount; iField++ )
        {
            OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                m_poFeatureDefn->myGetGeomFieldDefn(iField);
            OGRGeometry *poGeom = poFeature->GetGeomFieldRef(iField);
            if( poGeomFieldDefn->m_bCachedExtentIsValid &&
                poGeom != nullptr && !poGeom->IsEmpty() )
            {
                OGREnvelope sGeomEnvelope;
                poGeom->getEnvelope(&sGeomEnvelope);
                poGeomFieldDefn->m_oCachedExtent.Merge(sGeomEnvelope);
            }
        }
        ForceStatisticsToBeFlushed();
    }

    return eErr;
}

/************************************************************************/
/*                          AreTriggersSimilar                          */
/************************************************************************/

static int AreTriggersSimilar(const char* pszExpectedTrigger,
                              const char* pszTriggerSQL)
{
    int i = 0;  // Used after for.
    for( ; pszTriggerSQL[i] != '\0' && pszExpectedTrigger[i] != '\0'; i++ )
    {
        if( pszTriggerSQL[i] == pszExpectedTrigger[i] )
            continue;
        if( pszTriggerSQL[i] == '\n' && pszExpectedTrigger[i] == ' ' )
            continue;
        if( pszTriggerSQL[i] == ' ' && pszExpectedTrigger[i] == '\n' )
            continue;
        return FALSE;
    }
    return pszTriggerSQL[i] == '\0' && pszExpectedTrigger[i] == '\0';
}

/************************************************************************/
/*                          ICreateFeature()                            */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::ICreateFeature( OGRFeature *poFeature )

{
    sqlite3 *hDB = m_poDS->GetDB();
    CPLString      osCommand;
    bool           bNeedComma = false;

    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    if (!m_poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "CreateFeature");
        return OGRERR_FAILURE;
    }

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    // For speed-up, disable Spatialite triggers that :
    // * check the geometry type
    // * update the last_insert columns in geometry_columns_time and the spatial index
    // We do that only if there's no spatial index currently active
    // We'll check ourselves the first constraint and update last_insert
    // at layer closing
    if( !m_bHasCheckedTriggers &&
        m_poDS->HasSpatialite4Layout() && m_poFeatureDefn->GetGeomFieldCount() > 0 )
    {
        m_bHasCheckedTriggers = true;

        char* pszErrMsg = nullptr;

        // Backup INSERT ON triggers
        int nRowCount = 0, nColCount = 0;
        char **papszResult = nullptr;
        char* pszSQL3 = sqlite3_mprintf("SELECT name, sql FROM sqlite_master WHERE "
            "tbl_name = '%q' AND type = 'trigger' AND (name LIKE 'ggi_%%' OR name LIKE 'tmi_%%')",
            m_pszTableName);
        sqlite3_get_table( m_poDS->GetDB(),
                           pszSQL3,
                           &papszResult,
                           &nRowCount, &nColCount, &pszErrMsg );
        sqlite3_free(pszSQL3);

        if( pszErrMsg )
            sqlite3_free( pszErrMsg );
        pszErrMsg = nullptr;

        for(int j=0;j<m_poFeatureDefn->GetGeomFieldCount();j++)
        {
            OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                                        m_poFeatureDefn->myGetGeomFieldDefn(j);
            if( !((m_bDeferredSpatialIndexCreation || !poGeomFieldDefn->m_bHasSpatialIndex)) )
                continue;
            const char* pszGeomCol = poGeomFieldDefn->GetNameRef();

            for(int i=0;i<nRowCount;i++)
            {
                const char* pszTriggerName = papszResult[2*(i+1)+0];
                const char* pszTriggerSQL = papszResult[2*(i+1)+1];
                if( pszTriggerName!= nullptr && pszTriggerSQL != nullptr &&
                    CPLString(pszTriggerName).tolower().find(CPLString(pszGeomCol).tolower()) != std::string::npos )
                {
                    const char* pszExpectedTrigger = nullptr;
                    if( STARTS_WITH(pszTriggerName, "ggi_") )
                    {
                        pszExpectedTrigger = CPLSPrintf(
                        "CREATE TRIGGER \"ggi_%s_%s\" BEFORE INSERT ON \"%s\" "
                        "FOR EACH ROW BEGIN "
                        "SELECT RAISE(ROLLBACK, '%s.%s violates Geometry constraint [geom-type or SRID not allowed]') "
                        "WHERE (SELECT geometry_type FROM geometry_columns "
                        "WHERE Lower(f_table_name) = Lower('%s') AND Lower(f_geometry_column) = Lower('%s') "
                        "AND GeometryConstraints(NEW.\"%s\", geometry_type, srid) = 1) IS NULL; "
                        "END",
                        m_pszTableName, pszGeomCol, m_pszTableName,
                        m_pszTableName, pszGeomCol,
                        m_pszTableName, pszGeomCol,
                        pszGeomCol);
                    }
                    else if( STARTS_WITH(pszTriggerName, "tmi_") )
                    {
                        pszExpectedTrigger = CPLSPrintf(
                        "CREATE TRIGGER \"tmi_%s_%s\" AFTER INSERT ON \"%s\" "
                        "FOR EACH ROW BEGIN "
                        "UPDATE geometry_columns_time SET last_insert = strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ', 'now') "
                        "WHERE Lower(f_table_name) = Lower('%s') AND Lower(f_geometry_column) = Lower('%s'); "
                        "END",
                        m_pszTableName, pszGeomCol, m_pszTableName,
                        m_pszTableName, pszGeomCol);
                    }
                    /* Cannot happen due to the tests that lead to that code path */
                    /* that check there's no spatial index active */
                    /* A further potential optimization would be to rebuild the spatial index */
                    /* afterwards... */
                    /*else if( STARTS_WITH(pszTriggerName, "gii_") )
                    {
                        pszExpectedTrigger = CPLSPrintf(
                        "CREATE TRIGGER \"gii_%s_%s\" AFTER INSERT ON \"%s\" "
                        "FOR EACH ROW BEGIN "
                        "UPDATE geometry_columns_time SET last_insert = strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ', 'now') "
                        "WHERE Lower(f_table_name) = Lower('%s') AND Lower(f_geometry_column) = Lower('%s'); "
                        "DELETE FROM \"idx_%s_%s\" WHERE pkid=NEW.ROWID; "
                        "SELECT RTreeAlign('idx_%s_%s', NEW.ROWID, NEW.\"%s\"); "
                        "END",
                        m_pszTableName, pszGeomCol, m_pszTableName,
                        m_pszTableName, pszGeomCol,
                        m_pszTableName, pszGeomCol,
                        m_pszTableName, pszGeomCol, pszGeomCol);
                    }*/

                    if( pszExpectedTrigger != nullptr && AreTriggersSimilar(pszExpectedTrigger, pszTriggerSQL) )
                    {
                        // And drop them
                        pszSQL3 = sqlite3_mprintf("DROP TRIGGER %s", pszTriggerName);
                        int rc = sqlite3_exec( m_poDS->GetDB(), pszSQL3, nullptr, nullptr, &pszErrMsg );
                        if( rc != SQLITE_OK )
                            CPLDebug("SQLITE", "Error %s", pszErrMsg ? pszErrMsg : "");
                        else
                        {
                            CPLDebug("SQLite", "Dropping trigger %s", pszTriggerName);
                            poGeomFieldDefn->m_aosDisabledTriggers.push_back(std::pair<CPLString,CPLString>(pszTriggerName, pszTriggerSQL));
                        }
                        sqlite3_free(pszSQL3);
                        if( pszErrMsg )
                            sqlite3_free( pszErrMsg );
                        pszErrMsg = nullptr;
                    }
                    else
                    {
                        CPLDebug("SQLite", "Cannot drop %s trigger. Doesn't match expected definition",
                                pszTriggerName);
                    }
                }
            }
        }

        sqlite3_free_table( papszResult );
    }

    ResetReading();

    for(int j=0;j<m_poFeatureDefn->GetGeomFieldCount();j++)
    {
        OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                                        m_poFeatureDefn->myGetGeomFieldDefn(j);
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(j);
        if( !poGeomFieldDefn->m_aosDisabledTriggers.empty()  && poGeom != nullptr )
        {
            OGRwkbGeometryType eGeomType = poGeomFieldDefn->GetType();
            if( eGeomType != wkbUnknown && poGeom->getGeometryType() != eGeomType )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Cannot insert feature with geometry of type %s%s in column %s. Type %s%s expected",
                          OGRToOGCGeomType(poGeom->getGeometryType()),
                          (wkbFlatten(poGeom->getGeometryType()) != poGeom->getGeometryType()) ? "Z" :"",
                          poGeomFieldDefn->GetNameRef(),
                          OGRToOGCGeomType(eGeomType),
                          (wkbFlatten(eGeomType) != eGeomType) ? "Z": "" );
                return OGRERR_FAILURE;
            }
        }
    }

    int bReuseStmt = false;

    /* If there's a unset field with a default value, then we must create */
    /* a specific INSERT statement to avoid unset fields to be bound to NULL */
    bool bHasDefaultValue = false;
    int nFieldCount = m_poFeatureDefn->GetFieldCount();
    for( int iField = 0; iField < nFieldCount; iField++ )
    {
        if( !poFeature->IsFieldSet( iField ) &&
            poFeature->GetFieldDefnRef(iField)->GetDefault() != nullptr )
        {
            bHasDefaultValue = true;
            break;
        }
    }

    /* In case the FID column has also been created as a regular field */
    if( m_iFIDAsRegularColumnIndex >= 0 )
    {
        if( poFeature->GetFID() == OGRNullFID )
        {
            if( poFeature->IsFieldSetAndNotNull( m_iFIDAsRegularColumnIndex ) )
            {
                poFeature->SetFID(
                    poFeature->GetFieldAsInteger64(m_iFIDAsRegularColumnIndex));
            }
        }
        else
        {
            if( !poFeature->IsFieldSetAndNotNull( m_iFIDAsRegularColumnIndex ) ||
                poFeature->GetFieldAsInteger64(m_iFIDAsRegularColumnIndex) != poFeature->GetFID() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Inconsistent values of FID and field of same name");
                return OGRERR_FAILURE;
            }
        }
    }

    int bTemporaryStatement = (poFeature->GetFID() != OGRNullFID || bHasDefaultValue);
    if( m_hInsertStmt == nullptr || bTemporaryStatement )
    {
        CPLString      osValues;

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
        osCommand += CPLSPrintf( "INSERT INTO '%s' (", m_pszEscapedTableName );

/* -------------------------------------------------------------------- */
/*      Add FID if we have a cleartext FID column.                      */
/* -------------------------------------------------------------------- */
        if( m_pszFIDColumn != nullptr
            && poFeature->GetFID() != OGRNullFID )
        {
            osCommand += "\"";
            osCommand += SQLEscapeName(m_pszFIDColumn);
            osCommand += "\"";

            osValues += CPLSPrintf( CPL_FRMT_GIB, poFeature->GetFID() );
            bNeedComma = true;
        }

/* -------------------------------------------------------------------- */
/*      Add geometry.                                                   */
/* -------------------------------------------------------------------- */
        nFieldCount = m_poFeatureDefn->GetGeomFieldCount();
        for( int iField = 0; iField < nFieldCount; iField++ )
        {
            OGRSQLiteGeomFormat eGeomFormat =
                m_poFeatureDefn->myGetGeomFieldDefn(iField)->m_eGeomFormat;
            if( eGeomFormat == OSGF_FGF )
                continue;
            if( bHasDefaultValue && poFeature->GetGeomFieldRef(iField) == nullptr )
                continue;
            if( bNeedComma )
            {
                osCommand += ",";
                osValues += ",";
            }

            osCommand += "\"";
            osCommand += SQLEscapeName(m_poFeatureDefn->GetGeomFieldDefn(iField)->GetNameRef());
            osCommand += "\"";

            osValues += "?";

            bNeedComma = true;
        }

/* -------------------------------------------------------------------- */
/*      Add field values.                                               */
/* -------------------------------------------------------------------- */
        nFieldCount = m_poFeatureDefn->GetFieldCount();
        for( int iField = 0; iField < nFieldCount; iField++ )
        {
            if( iField == m_iFIDAsRegularColumnIndex )
                continue;
            if( bHasDefaultValue && !poFeature->IsFieldSet( iField ) )
                continue;

            if( bNeedComma )
            {
                osCommand += ",";
                osValues += ",";
            }

            osCommand += "\"";
            osCommand += SQLEscapeName(m_poFeatureDefn->GetFieldDefn(iField)->GetNameRef());
            osCommand += "\"";

            osValues += "?";

            bNeedComma = true;
        }

/* -------------------------------------------------------------------- */
/*      Merge final command.                                            */
/* -------------------------------------------------------------------- */
        osCommand += ") VALUES (";
        osCommand += osValues;
        osCommand += ")";

        if (bNeedComma == false)
            osCommand = CPLSPrintf( "INSERT INTO '%s' DEFAULT VALUES", m_pszEscapedTableName );
    }
    else
    {
        bReuseStmt = true;
    }

/* -------------------------------------------------------------------- */
/*      Prepare the statement.                                          */
/* -------------------------------------------------------------------- */
    if( !bReuseStmt && (m_hInsertStmt == nullptr || osCommand != m_osLastInsertStmt) )
    {
    #ifdef DEBUG
        CPLDebug( "OGR_SQLITE", "prepare_v2(%s)", osCommand.c_str() );
    #endif

        ClearInsertStmt();
        if( poFeature->GetFID() == OGRNullFID )
            m_osLastInsertStmt = osCommand;

        const int rc = sqlite3_prepare_v2( hDB, osCommand, -1, &m_hInsertStmt, nullptr );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "In CreateFeature(): sqlite3_prepare_v2(%s):\n  %s",
                    osCommand.c_str(), sqlite3_errmsg(hDB) );

            ClearInsertStmt();
            return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Bind values.                                                   */
/* -------------------------------------------------------------------- */
    OGRErr eErr = BindValues( poFeature, m_hInsertStmt, !bHasDefaultValue );
    if (eErr != OGRERR_NONE)
    {
        sqlite3_reset( m_hInsertStmt );
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    const int rc = sqlite3_step( m_hInsertStmt );

    if( rc != SQLITE_OK && rc != SQLITE_DONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_step() failed:\n  %s (%d)",
                  sqlite3_errmsg(hDB), rc );
        sqlite3_reset( m_hInsertStmt );
        ClearInsertStmt();
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Capture the FID/rowid.                                          */
/* -------------------------------------------------------------------- */
    const sqlite_int64 nFID = sqlite3_last_insert_rowid( hDB );
    if(nFID > 0)
    {
        poFeature->SetFID( nFID );
        if( m_iFIDAsRegularColumnIndex >= 0 )
            poFeature->SetField( m_iFIDAsRegularColumnIndex, nFID );
    }

    sqlite3_reset( m_hInsertStmt );

    if( bTemporaryStatement )
        ClearInsertStmt();

    nFieldCount = m_poFeatureDefn->GetGeomFieldCount();
    for( int iField = 0; iField < nFieldCount; iField++ )
    {
        OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
            m_poFeatureDefn->myGetGeomFieldDefn(iField);
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(iField);

        if( (poGeomFieldDefn->m_bCachedExtentIsValid || m_nFeatureCount == 0) &&
            poGeom != nullptr && !poGeom->IsEmpty() )
        {
            OGREnvelope sGeomEnvelope;
            poGeom->getEnvelope(&sGeomEnvelope);
            poGeomFieldDefn->m_oCachedExtent.Merge(sGeomEnvelope);
            poGeomFieldDefn->m_bCachedExtentIsValid = true;
            ForceStatisticsToBeFlushed();
        }
    }

    if( m_nFeatureCount >= 0 )
    {
        ForceStatisticsToBeFlushed();
        m_nFeatureCount ++;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::DeleteFeature( GIntBig nFID )

{
    CPLString      osSQL;

    if (HasLayerDefnError())
        return OGRERR_FAILURE;

    if( m_pszFIDColumn == nullptr )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't delete feature on a layer without FID column.");
        return OGRERR_FAILURE;
    }

    if (!m_poDS->GetUpdate())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "DeleteFeature");
        return OGRERR_FAILURE;
    }

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    ResetReading();

    osSQL.Printf( "DELETE FROM '%s' WHERE \"%s\" = " CPL_FRMT_GIB,
                  m_pszEscapedTableName,
                  SQLEscapeName(m_pszFIDColumn).c_str(), nFID );

    CPLDebug( "OGR_SQLITE", "exec(%s)", osSQL.c_str() );

    if( SQLCommand( m_poDS->GetDB(), osSQL ) != OGRERR_NONE )
        return OGRERR_FAILURE;

    OGRErr eErr = (sqlite3_changes(m_poDS->GetDB()) > 0) ? OGRERR_NONE : OGRERR_NON_EXISTING_FEATURE;
    if( eErr == OGRERR_NONE )
    {
        int nFieldCount = m_poFeatureDefn->GetGeomFieldCount();
        for( int iField = 0; iField < nFieldCount; iField++ )
        {
            OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                m_poFeatureDefn->myGetGeomFieldDefn(iField);
            poGeomFieldDefn->m_bCachedExtentIsValid = false;
        }
        m_nFeatureCount --;
        ForceStatisticsToBeFlushed();
    }

    return eErr;
}

/************************************************************************/
/*                         CreateSpatialIndex()                         */
/************************************************************************/

int OGRSQLiteTableLayer::CreateSpatialIndex(int iGeomCol)
{
    CPLString osCommand;

    if( m_bDeferredCreation ) RunDeferredCreationIfNecessary();

    if( iGeomCol < 0 || iGeomCol >= m_poFeatureDefn->GetGeomFieldCount() )
        return FALSE;

    osCommand.Printf("SELECT CreateSpatialIndex('%s', '%s')",
                     m_pszEscapedTableName,
                     SQLEscapeLiteral(m_poFeatureDefn->GetGeomFieldDefn(iGeomCol)->GetNameRef()).c_str());

    char* pszErrMsg = nullptr;
    sqlite3 *hDB = m_poDS->GetDB();
#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
#endif
    int rc = sqlite3_exec( hDB, osCommand, nullptr, nullptr, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Unable to create spatial index:\n%s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    m_poFeatureDefn->myGetGeomFieldDefn(iGeomCol)->m_bHasSpatialIndex = true;
    return TRUE;
}

/************************************************************************/
/*                      RunDeferredCreationIfNecessary()                */
/************************************************************************/

OGRErr OGRSQLiteTableLayer::RunDeferredCreationIfNecessary()
{
    if( !m_bDeferredCreation )
        return OGRERR_NONE;
    m_bDeferredCreation = false;

    CPLString osCommand;

    osCommand.Printf( "CREATE TABLE '%s' ( \"%s\" INTEGER PRIMARY KEY AUTOINCREMENT",
                      m_pszEscapedTableName,
                      SQLEscapeName(m_pszFIDColumn).c_str() );

    if ( !m_poDS->IsSpatialiteDB() )
    {
        for( int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                m_poFeatureDefn->myGetGeomFieldDefn(i);

            if( poGeomFieldDefn->m_eGeomFormat == OSGF_WKT )
            {
                osCommand += CPLSPrintf(", '%s' VARCHAR",
                    SQLEscapeLiteral(poGeomFieldDefn->GetNameRef()).c_str() );
            }
            else
            {
                osCommand += CPLSPrintf(", '%s' BLOB",
                    SQLEscapeLiteral(poGeomFieldDefn->GetNameRef()).c_str() );
            }
            if( !poGeomFieldDefn->IsNullable() )
            {
                osCommand += " NOT NULL";
            }
        }
    }

    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn* poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        if( i == m_iFIDAsRegularColumnIndex )
            continue;
        CPLString osFieldType(FieldDefnToSQliteFieldDefn(poFieldDefn));
        osCommand += CPLSPrintf(", '%s' %s",
                        SQLEscapeLiteral(poFieldDefn->GetNameRef()).c_str(),
                        osFieldType.c_str());
        if( !poFieldDefn->IsNullable() )
        {
            osCommand += " NOT NULL";
        }
        if( poFieldDefn->IsUnique() )
        {
            osCommand += " UNIQUE";
        }
        const char* pszDefault = poFieldDefn->GetDefault();
        if( pszDefault != nullptr &&
            (!poFieldDefn->IsDefaultDriverSpecific() ||
             (pszDefault[0] == '(' && pszDefault[strlen(pszDefault)-1] == ')' &&
             (STARTS_WITH_CI(pszDefault+1, "strftime") ||
              STARTS_WITH_CI(pszDefault+1, " strftime")))) )
        {
            osCommand += " DEFAULT ";
            osCommand += poFieldDefn->GetDefault();
        }
    }
    osCommand += ")";
    if( m_bStrict )
        osCommand += " STRICT";

#ifdef DEBUG
    CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
#endif

    if( SQLCommand( m_poDS->GetDB(), osCommand ) != OGRERR_NONE )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Eventually we should be adding this table to a table of         */
/*      "geometric layers", capturing the WKT projection, and           */
/*      perhaps some other housekeeping.                                */
/* -------------------------------------------------------------------- */
    if( m_poDS->HasGeometryColumns() )
    {
        /* Sometimes there is an old cruft entry in the geometry_columns
        * table if things were not properly cleaned up before.  We make
        * an effort to clean out such cruft.
        */
        osCommand.Printf(
            "DELETE FROM geometry_columns WHERE f_table_name = '%s'",
            m_pszEscapedTableName );

#ifdef DEBUG
        CPLDebug( "OGR_SQLITE", "exec(%s)", osCommand.c_str() );
#endif
        if( SQLCommand( m_poDS->GetDB(), osCommand ) != OGRERR_NONE )
            return OGRERR_FAILURE;

        for( int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            OGRSQLiteGeomFieldDefn* poGeomFieldDefn =
                m_poFeatureDefn->myGetGeomFieldDefn(i);
            if( RunAddGeometryColumn(poGeomFieldDefn, false) != OGRERR_NONE )
                return OGRERR_FAILURE;
        }
    }

    if (RecomputeOrdinals() != OGRERR_NONE )
        return OGRERR_FAILURE;

    if( m_poDS->IsSpatialiteDB() && m_poDS->GetLayerCount() == 1)
    {
        /* To create the layer_statistics and spatialite_history tables */
        if( SQLCommand( m_poDS->GetDB(), "SELECT UpdateLayerStatistics()" )
                                                            != OGRERR_NONE )
            return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           HasSpatialIndex()                          */
/************************************************************************/

bool OGRSQLiteTableLayer::HasSpatialIndex(int iGeomCol)
{
    GetLayerDefn();
    if( iGeomCol < 0 || iGeomCol >= m_poFeatureDefn->GetGeomFieldCount() )
        return false;
    OGRSQLiteGeomFieldDefn* poGeomFieldDefn = m_poFeatureDefn->myGetGeomFieldDefn(iGeomCol);

    CreateSpatialIndexIfNecessary();

    return poGeomFieldDefn->m_bHasSpatialIndex;
}

/************************************************************************/
/*                          InitFeatureCount()                          */
/************************************************************************/

void OGRSQLiteTableLayer::InitFeatureCount()
{
    m_nFeatureCount = 0;
    ForceStatisticsToBeFlushed();
}

/************************************************************************/
/*                 InvalidateCachedFeatureCountAndExtent()              */
/************************************************************************/

void OGRSQLiteTableLayer::InvalidateCachedFeatureCountAndExtent()
{
    m_nFeatureCount = -1;
    for(int iGeomCol=0;iGeomCol<GetLayerDefn()->GetGeomFieldCount();iGeomCol++)
        m_poFeatureDefn->myGetGeomFieldDefn(iGeomCol)->m_bCachedExtentIsValid = false;
    ForceStatisticsToBeFlushed();
}

/************************************************************************/
/*                     DoStatisticsNeedToBeFlushed()                    */
/************************************************************************/

bool OGRSQLiteTableLayer::DoStatisticsNeedToBeFlushed()
{
    return m_bStatisticsNeedsToBeFlushed &&
           m_poDS->IsSpatialiteDB() &&
           m_poDS->IsSpatialiteLoaded();
}

/************************************************************************/
/*                     ForceStatisticsToBeFlushed()                     */
/************************************************************************/

void OGRSQLiteTableLayer::ForceStatisticsToBeFlushed()
{
    m_bStatisticsNeedsToBeFlushed = true;
}

/************************************************************************/
/*                         AreStatisticsValid()                         */
/************************************************************************/

bool OGRSQLiteTableLayer::AreStatisticsValid()
{
    return m_nFeatureCount >= 0;
}

/************************************************************************/
/*                     LoadStatisticsSpatialite4DB()                    */
/************************************************************************/

void OGRSQLiteTableLayer::LoadStatisticsSpatialite4DB()
{
    for(int iCol = 0; iCol < GetLayerDefn()->GetGeomFieldCount(); iCol++ )
    {
        OGRSQLiteGeomFieldDefn* poGeomFieldDefn = m_poFeatureDefn->myGetGeomFieldDefn(iCol);
        const char* pszGeomCol = poGeomFieldDefn->GetNameRef();

        CPLString osSQL;
        CPLString osLastEvtDate;
        osSQL.Printf("SELECT MAX(last_insert, last_update, last_delete) FROM geometry_columns_time WHERE "
                    "(f_table_name = lower('%s') AND f_geometry_column = lower('%s'))"
#ifdef WORKAROUND_SQLITE3_BUGS
                    " OR 0"
#endif
                    ,m_pszEscapedTableName, SQLEscapeLiteral(pszGeomCol).c_str());

        sqlite3 *hDB = m_poDS->GetDB();
        int nRowCount = 0;
        int nColCount = 0;
        char **papszResult = nullptr;

        sqlite3_get_table( hDB, osSQL.c_str(), &papszResult,
                        &nRowCount, &nColCount, nullptr );

        /* Make it a Unix timestamp */
        int nYear = 0;
        int nMonth = 0;
        int nDay = 0;
        char chSep = 0;
        int nHour = 0;
        int nMinute = 0;
        float fSecond = 0.0f;
        if( nRowCount == 1 && nColCount == 1 && papszResult[1] != nullptr &&
            sscanf( papszResult[1], "%04d-%02d-%02d%c%02d:%02d:%f",
                    &nYear, &nMonth, &nDay, &chSep, &nHour, &nMinute, &fSecond ) == 7 )
        {
            osLastEvtDate = papszResult[1];
        }

        sqlite3_free_table( papszResult );
        papszResult = nullptr;

        if( osLastEvtDate.empty() )
            return;

        osSQL.Printf("SELECT last_verified, row_count, extent_min_x, extent_min_y, "
                    "extent_max_x, extent_max_y FROM geometry_columns_statistics WHERE "
                    "(f_table_name = lower('%s') AND f_geometry_column = lower('%s'))"
#ifdef WORKAROUND_SQLITE3_BUGS
                    " OR 0"
#endif
                    ,m_pszEscapedTableName, SQLEscapeLiteral(pszGeomCol).c_str());

        nRowCount = 0;
        nColCount = 0;
        sqlite3_get_table( hDB, osSQL.c_str(), &papszResult,
                        &nRowCount, &nColCount, nullptr );

        if( nRowCount == 1 && nColCount == 6 && papszResult[6] != nullptr &&
            sscanf( papszResult[6], "%04d-%02d-%02d%c%02d:%02d:%f",
                    &nYear, &nMonth, &nDay, &chSep, &nHour, &nMinute, &fSecond ) == 7 )
        {
            CPLString osLastVerified(papszResult[6]);

            /* Check that the information in geometry_columns_statistics is more */
            /* recent than geometry_columns_time */
            if( osLastVerified.compare(osLastEvtDate) > 0 )
            {
                char **papszRow = papszResult + 6;
                const char* pszRowCount = papszRow[1];
                const char* pszMinX = papszRow[2];
                const char* pszMinY = papszRow[3];
                const char* pszMaxX = papszRow[4];
                const char* pszMaxY = papszRow[5];

                CPLDebug("SQLITE",  "Loading statistics for %s,%s", m_pszTableName,
                         pszGeomCol);

                if( pszRowCount != nullptr )
                {
                    m_nFeatureCount = CPLAtoGIntBig( pszRowCount );
                    if( m_nFeatureCount == 0)
                    {
                        m_nFeatureCount = -1;
                        pszMinX = nullptr;
                    }
                    else
                    {
                        CPLDebug("SQLITE", "Layer %s feature count : " CPL_FRMT_GIB,
                                    m_pszTableName, m_nFeatureCount);
                    }
                }

                if( pszMinX != nullptr && pszMinY != nullptr &&
                    pszMaxX != nullptr && pszMaxY != nullptr )
                {
                    poGeomFieldDefn->m_bCachedExtentIsValid = true;
                    poGeomFieldDefn->m_oCachedExtent.MinX = CPLAtof(pszMinX);
                    poGeomFieldDefn->m_oCachedExtent.MinY = CPLAtof(pszMinY);
                    poGeomFieldDefn->m_oCachedExtent.MaxX = CPLAtof(pszMaxX);
                    poGeomFieldDefn->m_oCachedExtent.MaxY = CPLAtof(pszMaxY);
                    CPLDebug("SQLITE", "Layer %s extent : %s,%s,%s,%s",
                                m_pszTableName, pszMinX,pszMinY,pszMaxX,pszMaxY);
                }
            }
            else
            {
                CPLDebug("SQLite", "Statistics in %s is not up-to-date",
                         m_pszTableName);
            }
        }

        sqlite3_free_table( papszResult );
        papszResult = nullptr;
    }
}

/************************************************************************/
/*                          LoadStatistics()                            */
/************************************************************************/

void OGRSQLiteTableLayer::LoadStatistics()
{
    if( !m_poDS->IsSpatialiteDB() || !m_poDS->IsSpatialiteLoaded() )
        return;

    if( m_poDS->HasSpatialite4Layout() )
    {
        LoadStatisticsSpatialite4DB();
        return;
    }

    if( GetLayerDefn()->GetGeomFieldCount() != 1 )
        return;
    const char* pszGeomCol = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();

    GIntBig nFileTimestamp = m_poDS->GetFileTimestamp();
    if( nFileTimestamp == 0 )
        return;

    /* Find the most recent event in the 'spatialite_history' that is */
    /* a UpdateLayerStatistics event on all tables and geometry columns */
    CPLString osSQL;
    osSQL.Printf("SELECT MAX(timestamp) FROM spatialite_history WHERE "
                 "((table_name = '%s' AND geometry_column = '%s') OR "
                 "(table_name = 'ALL-TABLES' AND geometry_column = 'ALL-GEOMETRY-COLUMNS')) AND "
                 "event = 'UpdateLayerStatistics'",
                 m_pszEscapedTableName, SQLEscapeLiteral(pszGeomCol).c_str());

    sqlite3 *hDB = m_poDS->GetDB();
    int nRowCount = 0, nColCount = 0;
    char **papszResult = nullptr, *pszErrMsg = nullptr;

    sqlite3_get_table( hDB, osSQL.c_str(), &papszResult,
                       &nRowCount, &nColCount, &pszErrMsg );

    /* Make it a Unix timestamp */
    int nYear, nMonth, nDay, nHour, nMinute, nSecond;
    struct tm brokendown;
    GIntBig nTS = -1;
    if( nRowCount >= 1 && nColCount == 1 && papszResult[1] != nullptr &&
        sscanf( papszResult[1], "%04d-%02d-%02d %02d:%02d:%02d",
                &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond ) == 6 )
    {
        brokendown.tm_year = nYear - 1900;
        brokendown.tm_mon = nMonth - 1;
        brokendown.tm_mday = nDay;
        brokendown.tm_hour = nHour;
        brokendown.tm_min = nMinute;
        brokendown.tm_sec = nSecond;
        nTS = CPLYMDHMSToUnixTime(&brokendown);
    }

    /* If it is equal to the modified timestamp of the DB (as a file) */
    /* then we can safely use the data from the layer_statistics, since */
    /* it will be up-to-date */
    // cppcheck-suppress knownConditionTrueFalse
    if( nFileTimestamp == nTS || nFileTimestamp == nTS + 1 )
    {
        osSQL.Printf("SELECT row_count, extent_min_x, extent_min_y, extent_max_x, extent_max_y "
                        "FROM layer_statistics WHERE table_name = '%s' AND geometry_column = '%s'",
                        m_pszEscapedTableName, SQLEscapeLiteral(pszGeomCol).c_str());

        sqlite3_free_table( papszResult );
        papszResult = nullptr;

        sqlite3_get_table( hDB, osSQL.c_str(), &papszResult,
                            &nRowCount, &nColCount, &pszErrMsg );

        if( nRowCount == 1 )
        {
            char **papszRow = papszResult + 5;
            const char* pszRowCount = papszRow[0];
            const char* pszMinX = papszRow[1];
            const char* pszMinY = papszRow[2];
            const char* pszMaxX = papszRow[3];
            const char* pszMaxY = papszRow[4];

            CPLDebug("SQLITE", "File timestamp matches layer statistics timestamp. "
                        "Loading statistics for %s", m_pszTableName);

            if( pszRowCount != nullptr )
            {
                m_nFeatureCount = CPLAtoGIntBig( pszRowCount );
                CPLDebug("SQLITE", "Layer %s feature count : " CPL_FRMT_GIB,
                            m_pszTableName, m_nFeatureCount);
            }

            if( pszMinX != nullptr && pszMinY != nullptr &&
                pszMaxX != nullptr && pszMaxY != nullptr )
            {
                OGRSQLiteGeomFieldDefn* poGeomFieldDefn = m_poFeatureDefn->myGetGeomFieldDefn(0);
                poGeomFieldDefn->m_bCachedExtentIsValid = true;
                poGeomFieldDefn->m_oCachedExtent.MinX = CPLAtof(pszMinX);
                poGeomFieldDefn->m_oCachedExtent.MinY = CPLAtof(pszMinY);
                poGeomFieldDefn->m_oCachedExtent.MaxX = CPLAtof(pszMaxX);
                poGeomFieldDefn->m_oCachedExtent.MaxY = CPLAtof(pszMaxY);
                CPLDebug("SQLITE", "Layer %s extent : %s,%s,%s,%s",
                            m_pszTableName, pszMinX,pszMinY,pszMaxX,pszMaxY);
            }
        }
    }

    if( pszErrMsg )
        sqlite3_free( pszErrMsg );

    sqlite3_free_table( papszResult );
}

/************************************************************************/
/*                          SaveStatistics()                            */
/************************************************************************/

int OGRSQLiteTableLayer::SaveStatistics()
{
    if( !m_bStatisticsNeedsToBeFlushed || !m_poDS->IsSpatialiteDB()  ||
        !m_poDS->IsSpatialiteLoaded() || !m_poDS->GetUpdate() )
        return -1;
    if( GetLayerDefn()->GetGeomFieldCount() != 1 )
        return -1;
    OGRSQLiteGeomFieldDefn* poGeomFieldDefn = m_poFeatureDefn->myGetGeomFieldDefn(0);
    const char* pszGeomCol = poGeomFieldDefn->GetNameRef();

    CPLString osSQL;
    sqlite3 *hDB = m_poDS->GetDB();
    char* pszErrMsg = nullptr;

    // Update geometry_columns_time.
    if( !poGeomFieldDefn->m_aosDisabledTriggers.empty() )
    {
        char* pszSQL3 = sqlite3_mprintf(
            "UPDATE geometry_columns_time "
            "SET last_insert = strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ', 'now') "
            "WHERE Lower(f_table_name) = Lower('%q') AND "
            "Lower(f_geometry_column) = Lower('%q')",
            m_pszTableName, poGeomFieldDefn->GetNameRef());
        if( sqlite3_exec( m_poDS->GetDB(), pszSQL3, nullptr, nullptr, &pszErrMsg) != SQLITE_OK )
        {
            CPLDebug("SQLITE", "%s: error %s",
                     pszSQL3, pszErrMsg ? pszErrMsg : "unknown");
            sqlite3_free( pszErrMsg );
            pszErrMsg = nullptr;
        }
        sqlite3_free( pszSQL3 );
    }

    const char* pszStatTableName =
        m_poDS->HasSpatialite4Layout() ? "geometry_columns_statistics":
                                       "layer_statistics";
    if( SQLGetInteger( m_poDS->GetDB(),
            CPLSPrintf("SELECT 1 FROM sqlite_master WHERE type IN "
                       "('view', 'table') AND name = '%s'",
                       pszStatTableName), nullptr ) == 0 )
    {
        return TRUE;
    }
    const char* pszFTableName =
        m_poDS->HasSpatialite4Layout() ? "f_table_name" : "table_name";
    const char* pszFGeometryColumn =
        m_poDS->HasSpatialite4Layout() ? "f_geometry_column" : "geometry_column";
    CPLString osTableName(m_pszTableName);
    CPLString osGeomCol(pszGeomCol);
    const char* pszNowValue = "";
    if( m_poDS->HasSpatialite4Layout() )
    {
        osTableName = osTableName.tolower();
        osGeomCol = osGeomCol.tolower();
        pszNowValue = ", strftime('%Y-%m-%dT%H:%M:%fZ','now')";
    }

    if( m_nFeatureCount >= 0 )
    {
        /* Update or add entry in the layer_statistics table */
        if( poGeomFieldDefn->m_bCachedExtentIsValid )
        {
            osSQL.Printf("INSERT OR REPLACE INTO %s (%s"
                            "%s, %s, row_count, extent_min_x, "
                            "extent_min_y, extent_max_x, extent_max_y%s) VALUES ("
                            "%s'%s', '%s', " CPL_FRMT_GIB ", ?, ?, ?, ?%s)",
                            pszStatTableName,
                            m_poDS->HasSpatialite4Layout() ? "" : "raster_layer, ",
                            pszFTableName,
                            pszFGeometryColumn,
                            m_poDS->HasSpatialite4Layout() ? ", last_verified": "",
                            m_poDS->HasSpatialite4Layout() ? "" : "0 ,",
                            SQLEscapeLiteral(osTableName).c_str(),
                            SQLEscapeLiteral(osGeomCol).c_str(),
                            m_nFeatureCount,
                            pszNowValue
                        );

            sqlite3_stmt *m_hStmtInsert = nullptr;
            int rc = sqlite3_prepare_v2( hDB, osSQL, -1, &m_hStmtInsert, nullptr );
            if( rc == SQLITE_OK )
                rc = sqlite3_bind_double(m_hStmtInsert, 1,
                                         poGeomFieldDefn->m_oCachedExtent.MinX);
            if( rc == SQLITE_OK )
                rc = sqlite3_bind_double(m_hStmtInsert, 2,
                                         poGeomFieldDefn->m_oCachedExtent.MinY);
            if( rc == SQLITE_OK )
                rc = sqlite3_bind_double(m_hStmtInsert, 3,
                                         poGeomFieldDefn->m_oCachedExtent.MaxX);
            if( rc == SQLITE_OK )
                rc = sqlite3_bind_double(m_hStmtInsert, 4,
                                         poGeomFieldDefn->m_oCachedExtent.MaxY);
            if( rc == SQLITE_OK )
                rc = sqlite3_step( m_hStmtInsert );
            if ( rc != SQLITE_DONE )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "In Initialize(): sqlite3_step(%s):\n  %s",
                          osSQL.c_str(), sqlite3_errmsg(hDB) );
            }
            sqlite3_finalize( m_hStmtInsert );
            return rc == SQLITE_DONE;
        }
        else
        {
            osSQL.Printf("INSERT OR REPLACE INTO %s (%s"
                            "%s, %s, row_count, extent_min_x, "
                            "extent_min_y, extent_max_x, extent_max_y%s) VALUES ("
                            "%s'%s', '%s', " CPL_FRMT_GIB ", NULL, NULL, NULL, NULL%s)",
                            pszStatTableName,
                            m_poDS->HasSpatialite4Layout() ? "" : "raster_layer, ",
                            pszFTableName,
                            pszFGeometryColumn,
                            m_poDS->HasSpatialite4Layout() ? ", last_verified": "",
                            m_poDS->HasSpatialite4Layout() ? "" : "0 ,",
                            SQLEscapeLiteral(osTableName).c_str(),
                            SQLEscapeLiteral(osGeomCol).c_str(),
                            m_nFeatureCount,
                            pszNowValue
                        );
            return SQLCommand( hDB, osSQL) == OGRERR_NONE;
        }
    }
    else
    {
        /* Remove any existing entry in layer_statistics if for some reason */
        /* we know that it will out-of-sync */
        osSQL.Printf("DELETE FROM %s WHERE "
                     "%s = '%s' AND %s = '%s'",
                     pszStatTableName,
                     pszFTableName,
                     SQLEscapeLiteral(osTableName).c_str(),
                     pszFGeometryColumn,
                     SQLEscapeLiteral(osGeomCol).c_str());
        return SQLCommand( hDB, osSQL) == OGRERR_NONE;
    }
}

/************************************************************************/
/*                      SetCompressedColumns()                          */
/************************************************************************/

void OGRSQLiteTableLayer::SetCompressedColumns( const char* pszCompressedColumns )
{
    m_papszCompressedColumns = CSLTokenizeString2( pszCompressedColumns, ",",
                                                   CSLT_HONOURSTRINGS );
}
