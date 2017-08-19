/******************************************************************************
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements OGRGeoPackageTableLayer class
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
#include "ogrgeopackageutility.h"
#include "ogrsqliteutility.h"
#include "cpl_time.h"
#include "ogr_p.h"

CPL_CVSID("$Id$")

static const char UNSUPPORTED_OP_READ_ONLY[] =
  "%s : unsupported operation on a read-only datasource.";

//----------------------------------------------------------------------
// SaveExtent()
//
// Write the current contents of the layer envelope down to the
// gpkg_contents metadata table.
//
OGRErr OGRGeoPackageTableLayer::SaveExtent()
{
    if ( !m_poDS->GetUpdate() || ! m_bExtentChanged || ! m_poExtent )
        return OGRERR_NONE;

    sqlite3* poDb = m_poDS->GetDB();

    if ( ! poDb ) return OGRERR_FAILURE;

    char *pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_contents SET "
                "min_x = %g, min_y = %g, "
                "max_x = %g, max_y = %g "
                "WHERE lower(table_name) = lower('%q') AND "
                "Lower(data_type) = 'features'",
                m_poExtent->MinX, m_poExtent->MinY,
                m_poExtent->MaxX, m_poExtent->MaxY,
                m_pszTableName);

    OGRErr err = SQLCommand(poDb, pszSQL);
    sqlite3_free(pszSQL);
    m_bExtentChanged = false;

    return err;
}

//----------------------------------------------------------------------
// SaveTimestamp()
//
// Update the last_change column of the gpkg_contents metadata table.
//
OGRErr OGRGeoPackageTableLayer::SaveTimestamp()
{
    if ( !m_poDS->GetUpdate() || !m_bContentChanged )
        return OGRERR_NONE;

    m_bContentChanged = false;

    OGRErr err = m_poDS->UpdateGpkgContentsLastChange(m_pszTableName);

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if( m_bIsTable && err == OGRERR_NONE && m_poDS->m_bHasGPKGOGRContents )
    {
        CPLString osFeatureCount;
        if( m_nTotalFeatureCount >= 0 )
        {
            osFeatureCount.Printf(CPL_FRMT_GIB, m_nTotalFeatureCount);
        }
        else
        {
            osFeatureCount = "NULL";
        }
        char* pszSQL = sqlite3_mprintf(
                    "UPDATE gpkg_ogr_contents SET "
                    "feature_count = %s "
                    "WHERE lower(table_name) = lower('%q')",
                    osFeatureCount.c_str(),
                    m_pszTableName);
        err = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }
#endif

    return err;
}

//----------------------------------------------------------------------
// UpdateExtent()
//
// Expand the layer envelope if necessary to reflect the bounds
// of new features being added to the layer.
//
OGRErr OGRGeoPackageTableLayer::UpdateExtent( const OGREnvelope *poExtent )
{
    if ( ! m_poExtent )
    {
        m_poExtent = new OGREnvelope( *poExtent );
    }
    m_poExtent->Merge( *poExtent );
    m_bExtentChanged = true;
    return OGRERR_NONE;
}

//----------------------------------------------------------------------
// BuildColumns()
//
// Save a list of columns (fid, geometry, attributes) suitable
// for use in a SELECT query that retrieves all fields.
//
OGRErr OGRGeoPackageTableLayer::BuildColumns()
{
    CPLFree(panFieldOrdinals);
    panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * m_poFeatureDefn->GetFieldCount() );

    /* Always start with a primary key */
    CPLString soColumns = "m.";
    soColumns += m_pszFidColumn ?
        "\"" + SQLEscapeName(m_pszFidColumn) + "\"" : "_rowid_";
    iFIDCol = 0;

    /* Add a geometry column if there is one (just one) */
    if ( m_poFeatureDefn->GetGeomFieldCount() )
    {
        soColumns += ", m.\"";
        soColumns += SQLEscapeName(m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef());
        soColumns += "\"";
        iGeomCol = 1;
    }

    /* Add all the attribute columns */
    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++ )
    {
        soColumns += ", m.\"";
        soColumns += SQLEscapeName(m_poFeatureDefn->GetFieldDefn(i)->GetNameRef());
        soColumns += "\"";
        panFieldOrdinals[i] = 1 + (iGeomCol >= 0) + i;
    }

    m_soColumns = soColumns;
    return OGRERR_NONE;
}

//----------------------------------------------------------------------
// IsGeomFieldSet()
//
// Utility method to determine if there is a non-Null geometry
// in an OGRGeometry.
//
bool OGRGeoPackageTableLayer::IsGeomFieldSet( OGRFeature *poFeature )
{
    return
        poFeature->GetDefnRef()->GetGeomFieldCount() &&
        poFeature->GetGeomFieldRef(0);
}

#define MY_CPLAssert CPLAssert

OGRErr OGRGeoPackageTableLayer::FeatureBindParameters( OGRFeature *poFeature,
                                                       sqlite3_stmt *poStmt,
                                                       int *pnColCount,
                                                       bool bAddFID,
                                                       bool bBindUnsetFields )
{
    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    int nColCount = 1;
    int err = SQLITE_OK;
    if( bAddFID )
    {
        err = sqlite3_bind_int64(poStmt, nColCount++, poFeature->GetFID());
        MY_CPLAssert( err == SQLITE_OK );
    }

    /* Bind data values to the statement, here bind the blob for geometry */
    if ( err == SQLITE_OK && poFeatureDefn->GetGeomFieldCount() )
    {
        // Non-NULL geometry.
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(0);
        if ( poGeom )
        {
            GByte *pabyWkb = NULL;
            size_t szWkb = 0;
            pabyWkb = GPkgGeometryFromOGR(poGeom, m_iSrs, &szWkb);
            err = sqlite3_bind_blob(poStmt, nColCount++, pabyWkb,
                                    static_cast<int>(szWkb), CPLFree);
            MY_CPLAssert( err == SQLITE_OK );

            CreateGeometryExtensionIfNecessary(poGeom);
        }
        /* NULL geometry */
        else
        {
            err = sqlite3_bind_null(poStmt, nColCount++);
            MY_CPLAssert( err == SQLITE_OK );
        }
    }

    /* Bind the attributes using appropriate SQLite data types */
    for( int i = 0;
         err == SQLITE_OK && i < poFeatureDefn->GetFieldCount();
         i++ )
    {
        if( i == m_iFIDAsRegularColumnIndex )
            continue;
        if( !poFeature->IsFieldSet(i) )
        {
            if( bBindUnsetFields )
            {
                err = sqlite3_bind_null(poStmt, nColCount++);
                MY_CPLAssert( err == SQLITE_OK );
            }
            continue;
        }

        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(i);

        if( !poFeature->IsFieldNull(i) )
        {
            switch(SQLiteFieldFromOGR(poFieldDefn->GetType()))
            {
                case SQLITE_INTEGER:
                {
                    err = sqlite3_bind_int64(poStmt, nColCount++, poFeature->GetFieldAsInteger64(i));
                    MY_CPLAssert( err == SQLITE_OK );
                    break;
                }
                case SQLITE_FLOAT:
                {
                    err = sqlite3_bind_double(poStmt, nColCount++, poFeature->GetFieldAsDouble(i));
                    MY_CPLAssert( err == SQLITE_OK );
                    break;
                }
                case SQLITE_BLOB:
                {
                    int szBlob = 0;
                    GByte *pabyBlob = poFeature->GetFieldAsBinary(i, &szBlob);
                    err = sqlite3_bind_blob(poStmt, nColCount++, pabyBlob, szBlob, NULL);
                    MY_CPLAssert( err == SQLITE_OK );
                    break;
                }
                default:
                {
                    const char *pszVal = poFeature->GetFieldAsString(i);
                    int nValLengthBytes = (int)strlen(pszVal);
                    char szVal[32];
                    int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZFlag;
                    CPLString osTemp;
                    if( poFieldDefn->GetType() == OFTDate )
                    {
                        poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond, &nTZFlag);
                        snprintf(szVal, sizeof(szVal), "%04d-%02d-%02d", nYear, nMonth, nDay);
                        pszVal = szVal;
                        nValLengthBytes = (int)strlen(pszVal);
                    }
                    else if( poFieldDefn->GetType() == OFTDateTime )
                    {
                        float fSecond = 0.0f;
                        poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay,
                                                      &nHour, &nMinute,
                                                      &fSecond, &nTZFlag);
                        if( nTZFlag == 0 || nTZFlag == 100 )
                        {
                            if( OGR_GET_MS(fSecond) )
                                snprintf(szVal, sizeof(szVal), "%04d-%02d-%02dT%02d:%02d:%06.3fZ",
                                     nYear, nMonth, nDay, nHour, nMinute, fSecond);
                            else
                                snprintf(szVal, sizeof(szVal), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                                     nYear, nMonth, nDay, nHour, nMinute, (int)fSecond);
                            pszVal = szVal;
                            nValLengthBytes = (int)strlen(pszVal);
                        }
                    }
                    else if( poFieldDefn->GetType() == OFTString &&
                             poFieldDefn->GetWidth() > 0 )
                    {
                        if( !CPLIsUTF8(pszVal, -1) )
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Value of field '%s' is not a valid UTF-8 string.%s",
                                     poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                                     m_bTruncateFields ? " Value will be laundered." : "");
                            if( m_bTruncateFields )
                            {
                                char* pszTemp = CPLForceToASCII(pszVal, -1, '_');
                                osTemp = pszTemp;
                                pszVal = osTemp.c_str();
                                CPLFree(pszTemp);
                            }
                        }

                        if( CPLStrlenUTF8(pszVal) > poFieldDefn->GetWidth() )
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Value of field '%s' has %d characters, whereas maximum allowed is %d.%s",
                                     poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                                     CPLStrlenUTF8(pszVal),
                                     poFieldDefn->GetWidth(),
                                     m_bTruncateFields ? " Value will be truncated." : "");
                            if( m_bTruncateFields )
                            {
                                int k = 0;
                                nValLengthBytes = 0;
                                while (pszVal[nValLengthBytes])
                                {
                                    if ((pszVal[nValLengthBytes] & 0xc0) != 0x80)
                                    {
                                        k++;
                                        // Stop at the start of the character just beyond the maximum accepted
                                        if( k > poFieldDefn->GetWidth() )
                                            break;
                                    }
                                    nValLengthBytes++;
                                }
                            }
                        }
                    }
                    err = sqlite3_bind_text(poStmt, nColCount++, pszVal, nValLengthBytes, SQLITE_TRANSIENT);
                    MY_CPLAssert( err == SQLITE_OK );
                    break;
                }
            }
        }
        else
        {
            err = sqlite3_bind_null(poStmt, nColCount++);
            MY_CPLAssert( err == SQLITE_OK );
        }
    }

    if( pnColCount != NULL )
        *pnColCount = nColCount;
    return (err == SQLITE_OK) ? OGRERR_NONE : OGRERR_FAILURE;
}

//----------------------------------------------------------------------
// FeatureBindUpdateParameters()
//
// Selectively bind the values of an OGRFeature to a prepared
// statement, prior to execution. Carefully binds exactly the
// same parameters that have been set up by FeatureGenerateUpdateSQL()
// as bindable.
//
OGRErr OGRGeoPackageTableLayer::FeatureBindUpdateParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt )
{

    int nColCount = 0;
    const OGRErr err =
        FeatureBindParameters( poFeature, poStmt, &nColCount, false, false );
    if ( err != OGRERR_NONE )
        return err;

    // Bind the FID to the "WHERE" clause.
    const int sqlite_err =
        sqlite3_bind_int64(poStmt, nColCount, poFeature->GetFID());
    if ( sqlite_err != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "failed to bind FID '" CPL_FRMT_GIB "' to statement", poFeature->GetFID());
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

//----------------------------------------------------------------------
// FeatureBindInsertParameters()
//
// Selectively bind the values of an OGRFeature to a prepared
// statement, prior to execution. Carefully binds exactly the
// same parameters that have been set up by FeatureGenerateInsertSQL()
// as bindable.
//
OGRErr OGRGeoPackageTableLayer::FeatureBindInsertParameters( OGRFeature *poFeature,
                                                             sqlite3_stmt *poStmt,
                                                             bool bAddFID,
                                                             bool bBindUnsetFields )
{
    return FeatureBindParameters( poFeature, poStmt, NULL, bAddFID, bBindUnsetFields );
}

//----------------------------------------------------------------------
// FeatureGenerateInsertSQL()
//
// Build a SQL INSERT statement that references all the columns in
// the OGRFeatureDefn, then prepare it for repeated use in a prepared
// statement. All statements start off with geometry (if it exists)
// then reference each column in the order it appears in the OGRFeatureDefn.
// FeatureBindParameters operates on the expectation of this
// column ordering.
//
CPLString OGRGeoPackageTableLayer::FeatureGenerateInsertSQL( OGRFeature *poFeature,
                                                             bool bAddFID,
                                                             bool bBindUnsetFields )
{
    bool bNeedComma = false;
    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    if( poFeatureDefn->GetFieldCount() == ((m_iFIDAsRegularColumnIndex >= 0) ? 1 : 0) &&
        poFeatureDefn->GetGeomFieldCount() == 0 &&
        !bAddFID )
        return CPLSPrintf("INSERT INTO \"%s\" DEFAULT VALUES",
                          SQLEscapeName(m_pszTableName).c_str());

    /* Set up our SQL string basics */
    CPLString osSQLFront;
    osSQLFront.Printf("INSERT INTO \"%s\" ( ",
                      SQLEscapeName(m_pszTableName).c_str());

    CPLString osSQLBack;
    osSQLBack = ") VALUES (";

    CPLString osSQLColumn;

    if( bAddFID )
    {
        osSQLColumn.Printf("\"%s\"", SQLEscapeName(GetFIDColumn()).c_str());
        osSQLFront += osSQLColumn;
        osSQLBack += "?";
        bNeedComma = true;
    }

    if ( poFeatureDefn->GetGeomFieldCount() )
    {
        if( bNeedComma )
        {
            osSQLFront += ", ";
            osSQLBack += ", ";
        }

        osSQLColumn.Printf("\"%s\"", SQLEscapeName(poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef()).c_str());
        osSQLFront += osSQLColumn;
        osSQLBack += "?";
        bNeedComma = true;
    }

    /* Add attribute column names (except FID) to the SQL */
    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( i == m_iFIDAsRegularColumnIndex )
            continue;
        if( !bBindUnsetFields && !poFeature->IsFieldSet(i) )
            continue;

        if( !bNeedComma )
        {
            bNeedComma = true;
        }
        else
        {
            osSQLFront += ", ";
            osSQLBack += ", ";
        }

        osSQLColumn.Printf("\"%s\"",
                           SQLEscapeName(poFeatureDefn->GetFieldDefn(i)->GetNameRef()).c_str());
        osSQLFront += osSQLColumn;
        osSQLBack += "?";
    }

    osSQLBack += ")";

    if( !bNeedComma )
        return CPLSPrintf("INSERT INTO \"%s\" DEFAULT VALUES",
                          SQLEscapeName(m_pszTableName).c_str());

    return osSQLFront + osSQLBack;
}

//----------------------------------------------------------------------
// FeatureGenerateUpdateSQL()
//
// Build a SQL UPDATE statement that references all the columns in
// the OGRFeatureDefn, then prepare it for repeated use in a prepared
// statement. All statements start off with geometry (if it exists)
// then reference each column in the order it appears in the OGRFeatureDefn.
// FeatureBindParameters operates on the expectation of this
// column ordering.

//
CPLString OGRGeoPackageTableLayer::FeatureGenerateUpdateSQL( OGRFeature *poFeature )
{
    bool bNeedComma = false;
    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    /* Set up our SQL string basics */
    CPLString osUpdate;
    osUpdate.Printf("UPDATE \"%s\" SET ",
                    SQLEscapeName(m_pszTableName).c_str());

    CPLString osSQLColumn;

    if ( poFeatureDefn->GetGeomFieldCount() > 0 )
    {
        osSQLColumn.Printf("\"%s\"",
                           SQLEscapeName(poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef()).c_str());
        osUpdate += osSQLColumn;
        osUpdate += "=?";
        bNeedComma = true;
    }

    /* Add attribute column names (except FID) to the SQL */
    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( i == m_iFIDAsRegularColumnIndex )
            continue;
        if( !poFeature->IsFieldSet(i) )
            continue;
        if( !bNeedComma )
            bNeedComma = true;
        else
            osUpdate += ", ";

        osSQLColumn.Printf("\"%s\"",
                           SQLEscapeName(poFeatureDefn->GetFieldDefn(i)->GetNameRef()).c_str());
        osUpdate += osSQLColumn;
        osUpdate += "=?";
    }
    if( !bNeedComma )
        return CPLString();

    CPLString osWhere;
    osWhere.Printf(" WHERE \"%s\" = ?",
                   SQLEscapeName(m_pszFidColumn).c_str());

    return osUpdate + osWhere;
}

//----------------------------------------------------------------------
// ReadTableDefinition()
//
// Initialization routine. Read all the metadata about a table,
// starting from just the table name. Reads information from GPKG
// metadata tables and from SQLite table metadata. Uses it to
// populate OGRSpatialReference information and OGRFeatureDefn objects,
// among others.
//
OGRErr OGRGeoPackageTableLayer::ReadTableDefinition(bool bIsSpatial, bool bIsGpkgTable)
{
    OGRErr err;
    SQLResult oResultTable;
    bool bReadExtent = false;
    sqlite3* poDb = m_poDS->GetDB();
    OGREnvelope oExtent;
    CPLString osGeomColumnName;
    CPLString osGeomColsType;
    bool bHasZ = false;
    bool bHasM = false;

    // Is it a table or a view ?
    {
        SQLResult oResult;
        char* pszSQL = sqlite3_mprintf(
            "SELECT type FROM sqlite_master WHERE lower(name) = lower('%q') AND type "
            "IN ('view', 'table')",
            m_pszTableName);
        err = SQLQuery(poDb, pszSQL, &oResult);
        sqlite3_free(pszSQL);
        if ( err == OGRERR_NONE && oResult.nRowCount == 1 )
        {
            m_bIsTable = EQUAL(SQLResultGetValue(&oResult, 0, 0),
                               "table");
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Table or view '%s' does not exist",
                      m_pszTableName );
            SQLResultFree(&oResult);
            return OGRERR_FAILURE;
        }
        SQLResultFree(&oResult);
    }

    if( bIsGpkgTable )
    {
        /* Check that the table name is registered in gpkg_contents */
        char* pszSQL = sqlite3_mprintf(
            "SELECT table_name, data_type, identifier, "
            "description, min_x, min_y, max_x, max_y "
            "FROM gpkg_contents "
            "WHERE (lower(table_name) = lower('%q'))"
#ifdef WORKAROUND_SQLITE3_BUGS
            " OR 0"
#endif
            " LIMIT 2"
            , m_pszTableName);

        SQLResult oResultContents;
        err = SQLQuery(poDb, pszSQL, &oResultContents);
        sqlite3_free(pszSQL);

        /* gpkg_contents query has to work */
        /* gpkg_contents.table_name is supposed to be unique */
        if ( err != OGRERR_NONE || oResultContents.nRowCount != 1 )
        {
            if ( err != OGRERR_NONE )
                CPLError( CE_Failure, CPLE_AppDefined, "%s", oResultContents.pszErrMsg ? oResultContents.pszErrMsg : "" );
            else /* if ( oResultContents.nRowCount != 1 ) */
                CPLError( CE_Failure, CPLE_AppDefined,
                          "layer '%s' is not registered in gpkg_contents",
                          m_pszTableName );

            SQLResultFree(&oResultContents);
            return OGRERR_FAILURE;
        }

        const char* pszIdentifier = SQLResultGetValue(&oResultContents, 2, 0);
        if( pszIdentifier && strcmp(pszIdentifier, m_pszTableName) != 0 )
            OGRLayer::SetMetadataItem("IDENTIFIER", pszIdentifier);
        const char* pszDescription = SQLResultGetValue(&oResultContents, 3, 0);
        if( pszDescription && pszDescription[0] )
            OGRLayer::SetMetadataItem("DESCRIPTION", pszDescription);

#ifdef ENABLE_GPKG_OGR_CONTENTS
        if( m_poDS->m_bHasGPKGOGRContents )
        {
            pszSQL = sqlite3_mprintf(
                "SELECT feature_count "
                "FROM gpkg_ogr_contents "
                "WHERE lower(table_name) = lower('%q')"
#ifdef WORKAROUND_SQLITE3_BUGS
                " OR 0"
#endif
                " LIMIT 2"
                , m_pszTableName);
            SQLResult oResultFeatureCount;
            err = SQLQuery(poDb, pszSQL, &oResultFeatureCount);
            sqlite3_free(pszSQL);
            if( err == OGRERR_NONE && oResultFeatureCount.nRowCount == 1 )
            {
                const char* pszFeatureCount =
                                SQLResultGetValue(&oResultFeatureCount, 0, 0);
                if( pszFeatureCount )
                {
                    m_nTotalFeatureCount = CPLAtoGIntBig(pszFeatureCount);
                }
            }
            SQLResultFree(&oResultFeatureCount);

            // Check if the triggers are there. 
            pszSQL = sqlite3_mprintf(
                "SELECT COUNT(*) FROM sqlite_master WHERE type = 'trigger' "
                "AND name IN ('trigger_insert_feature_count_%q', "
                "'trigger_delete_feature_count_%q')",
                m_pszTableName, m_pszTableName);
            if( SQLGetInteger(poDb, pszSQL, NULL) == 2 )
            {
                m_bOGRFeatureCountTriggersEnabled = true;
            }
            else if( m_bIsTable )
            {
                CPLDebug("GPKG", "Insert/delete feature_count triggers "
                         "missing on %s", m_pszTableName);
            }
            sqlite3_free(pszSQL);
        }
#endif

        if( bIsSpatial )
        {
            const char *pszMinX = SQLResultGetValue(&oResultContents, 4, 0);
            const char *pszMinY = SQLResultGetValue(&oResultContents, 5, 0);
            const char *pszMaxX = SQLResultGetValue(&oResultContents, 6, 0);
            const char *pszMaxY = SQLResultGetValue(&oResultContents, 7, 0);

            /* All the extrema have to be non-NULL for this to make sense */
            if ( pszMinX && pszMinY && pszMaxX && pszMaxY )
            {
                oExtent.MinX = CPLAtof(pszMinX);
                oExtent.MinY = CPLAtof(pszMinY);
                oExtent.MaxX = CPLAtof(pszMaxX);
                oExtent.MaxY = CPLAtof(pszMaxY);
                bReadExtent = oExtent.MinX <= oExtent.MaxX &&
                              oExtent.MinY <= oExtent.MaxY;
            }

            /* Done with info from gpkg_contents now */
            SQLResultFree(&oResultContents);

            /* Check that the table name is registered in gpkg_geometry_columns */
            pszSQL = sqlite3_mprintf(
                        "SELECT table_name, column_name, "
                        "geometry_type_name, srs_id, z, m "
                        "FROM gpkg_geometry_columns "
                        "WHERE lower(table_name) = lower('%q')"
#ifdef WORKAROUND_SQLITE3_BUGS
                        " OR 0"
#endif
                        " LIMIT 2"
                        ,m_pszTableName);

            SQLResult oResultGeomCols;
            err = SQLQuery(poDb, pszSQL, &oResultGeomCols);
            sqlite3_free(pszSQL);

            /* gpkg_geometry_columns query has to work */
            /* gpkg_geometry_columns.table_name is supposed to be unique */
            if ( err != OGRERR_NONE || oResultGeomCols.nRowCount != 1 )
            {
                if ( err != OGRERR_NONE )
                    CPLError( CE_Failure, CPLE_AppDefined, "%s", oResultGeomCols.pszErrMsg ? oResultGeomCols.pszErrMsg : "" );
                else /* if ( oResultContents.nRowCount != 1 ) */
                    CPLError( CE_Failure, CPLE_AppDefined, "layer '%s' is not registered in gpkg_geometry_columns", m_pszTableName );

                SQLResultFree(&oResultGeomCols);
                return OGRERR_FAILURE;
            }

            const char* pszGeomColName = SQLResultGetValue(&oResultGeomCols, 1, 0);
            if( pszGeomColName != NULL )
                osGeomColumnName = pszGeomColName;
            const char* pszGeomColsType = SQLResultGetValue(&oResultGeomCols, 2, 0);
            if( pszGeomColsType != NULL )
                osGeomColsType = pszGeomColsType;
            m_iSrs = SQLResultGetValueAsInteger(&oResultGeomCols, 3, 0);
            bHasZ = CPL_TO_BOOL(SQLResultGetValueAsInteger(&oResultGeomCols, 4, 0));
            bHasM = CPL_TO_BOOL(SQLResultGetValueAsInteger(&oResultGeomCols, 5, 0));

            SQLResultFree(&oResultGeomCols);
        }
        else
            SQLResultFree(&oResultContents);
    }

    /* Use the "PRAGMA TABLE_INFO()" call to get table definition */
    /*  #|name|type|notnull|default|pk */
    /*  0|id|integer|0||1 */
    /*  1|name|varchar|0||0 */
    char* pszSQL = sqlite3_mprintf("pragma table_info('%q')", m_pszTableName);
    err = SQLQuery(poDb, pszSQL, &oResultTable);
    sqlite3_free(pszSQL);

    if ( err != OGRERR_NONE || oResultTable.nRowCount == 0 )
    {
        if( oResultTable.pszErrMsg != NULL )
            CPLError( CE_Failure, CPLE_AppDefined, "%s", oResultTable.pszErrMsg );
        else
            CPLError( CE_Failure, CPLE_AppDefined, "Cannot find table %s", m_pszTableName );

        SQLResultFree(&oResultTable);
        return OGRERR_FAILURE;
    }

    /* Populate feature definition from table description */

    // First pass to determine if we have a single PKID column
    int nCountPKIDColumns = 0;
    for ( int iRecord = 0; iRecord < oResultTable.nRowCount; iRecord++ )
    {
        int nPKIDIndex = SQLResultGetValueAsInteger(&oResultTable, 5, iRecord);
        if( nPKIDIndex > 0 )
            nCountPKIDColumns ++;
    }
    if( nCountPKIDColumns > 1 )
    {
        CPLDebug("GPKG", "For table %s, multiple columns make "
                         "the primary key. Ignoring them",
                 m_pszTableName);
    }

    for ( int iRecord = 0; iRecord < oResultTable.nRowCount; iRecord++ )
    {
        const char *pszName = SQLResultGetValue(&oResultTable, 1, iRecord);
        const char *pszType = SQLResultGetValue(&oResultTable, 2, iRecord);
        int bNotNull = SQLResultGetValueAsInteger(&oResultTable, 3, iRecord);
        const char* pszDefault = SQLResultGetValue(&oResultTable, 4, iRecord);
        int nPKIDIndex = SQLResultGetValueAsInteger(&oResultTable, 5, iRecord);
        OGRFieldSubType eSubType;
        int nMaxWidth = 0;
        OGRFieldType oType = GPkgFieldToOGR(pszType, eSubType, nMaxWidth);

        /* Not a standard field type... */
        if ( !EQUAL(pszType, "") && !EQUAL(pszName, "OGC_FID") &&
            ((oType > OFTMaxType && !osGeomColsType.empty() ) ||
             EQUAL(osGeomColumnName, pszName)) )
        {
            /* Maybe it's a geometry type? */
            OGRwkbGeometryType oGeomType;
            if( oType > OFTMaxType )
                oGeomType = GPkgGeometryTypeToWKB(pszType, bHasZ, bHasM);
            else
                oGeomType = wkbUnknown;
            if ( oGeomType != wkbNone )
            {
                OGRwkbGeometryType oGeomTypeGeomCols = GPkgGeometryTypeToWKB(osGeomColsType.c_str(), bHasZ, bHasM);
                /* Enforce consistency between table and metadata */
                if( wkbFlatten(oGeomType) == wkbUnknown )
                    oGeomType = oGeomTypeGeomCols;
                if ( oGeomType != oGeomTypeGeomCols )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "geometry column type in '%s.%s' is not consistent with type in gpkg_geometry_columns",
                             m_pszTableName, pszName);
                }

                if ( m_poFeatureDefn->GetGeomFieldCount() == 0 )
                {
                    OGRGeomFieldDefn oGeomField(pszName, oGeomType);
                    if( bNotNull )
                        oGeomField.SetNullable(FALSE);
                    m_poFeatureDefn->AddGeomFieldDefn(&oGeomField);

                    /* Read the SRS */
                    OGRSpatialReference *poSRS = m_poDS->GetSpatialRef(m_iSrs);
                    if ( poSRS )
                    {
                        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
                        poSRS->Dereference();
                    }
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "table '%s' has multiple geometry fields? not legal in gpkg",
                             m_pszTableName);
                    SQLResultFree(&oResultTable);
                    return OGRERR_FAILURE;
                }
            }
            else
            {
                // CPLError( CE_Failure, CPLE_AppDefined, "invalid field type '%s'", pszType );
                // SQLResultFree(&oResultTable);
                CPLError(CE_Warning, CPLE_AppDefined,
                         "geometry column '%s' of type '%s' ignored", pszName, pszType);
            }
        }
        else
        {
            if( oType > OFTMaxType )
            {
                CPLDebug("GPKG",
                         "For table %s, unrecognized type name %s for "
                         "column %s. Using string type",
                         m_pszTableName, pszType, pszName);
                oType = OFTString;
            }

            /* Is this the FID column? */
            if ( nPKIDIndex > 0 && nCountPKIDColumns == 1 &&
                      (oType == OFTInteger || oType == OFTInteger64) )
            {
                m_pszFidColumn = CPLStrdup(pszName);
            }
            else
            {
                OGRFieldDefn oField(pszName, oType);
                oField.SetSubType(eSubType);
                oField.SetWidth(nMaxWidth);
                if( bNotNull )
                    oField.SetNullable(FALSE);
                if( pszDefault != NULL )
                {
                    int nYear = 0;
                    int nMonth = 0;
                    int nDay = 0;
                    int nHour = 0;
                    int nMinute = 0;
                    float fSecond = 0.0f;
                    if( oField.GetType() == OFTString &&
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
                        oField.SetDefault(osDefault);
                    }
                    else if( oType == OFTDateTime &&
                             sscanf(pszDefault, "'%d-%d-%dT%d:%d:%fZ'", &nYear, &nMonth, &nDay,
                                        &nHour, &nMinute, &fSecond) == 6 )
                    {
                        if( strchr(pszDefault, '.') == NULL )
                            oField.SetDefault(CPLSPrintf("'%04d/%02d/%02d %02d:%02d:%02d'",
                                                      nYear, nMonth, nDay, nHour, nMinute, (int)(fSecond+0.5)));
                        else
                            oField.SetDefault(CPLSPrintf("'%04d/%02d/%02d %02d:%02d:%06.3f'",
                                                            nYear, nMonth, nDay, nHour, nMinute, fSecond));
                    }
                    else if( (oField.GetType() == OFTDate || oField.GetType() == OFTDateTime) &&
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
                        if( EQUAL(osDefault, "(strftime('%Y-%m-%dT%H:%M:%fZ','now'))") )
                            oField.SetDefault("CURRENT_TIMESTAMP");
                        else
                            oField.SetDefault(osDefault);
                    }
                    else
                    {
                        oField.SetDefault(pszDefault);
                    }
                }
                m_poFeatureDefn->AddFieldDefn(&oField);
            }
        }
    }

    /* Wait, we didn't find a FID? Some operations will not be possible */
    if ( m_bIsTable && m_pszFidColumn == NULL )
    {
        CPLDebug("GPKG",
                 "no integer primary key defined for table '%s'",
                 m_pszTableName);
    }

    if ( bReadExtent )
    {
        m_poExtent = new OGREnvelope(oExtent);
    }

    SQLResultFree(&oResultTable);

    /* Update the columns string */
    BuildColumns();

    CheckUnknownExtensions();

    return OGRERR_NONE;
}

/************************************************************************/
/*                      OGRGeoPackageTableLayer()                       */
/************************************************************************/

OGRGeoPackageTableLayer::OGRGeoPackageTableLayer(
                    GDALGeoPackageDataset *poDS,
                    const char * pszTableName) :
    OGRGeoPackageLayer(poDS),
    m_pszTableName(CPLStrdup(pszTableName)),
    m_bIsTable(true), // sensible init for creation mode
    m_iSrs(0),
    m_poExtent(NULL),
#ifdef ENABLE_GPKG_OGR_CONTENTS
    m_nTotalFeatureCount(-1),
    m_bOGRFeatureCountTriggersEnabled(false),
    m_bAddOGRFeatureCountTriggers(false),
    m_bFeatureCountTriggersDeletedInTransaction(false),
#endif
    m_soColumns(""),
    m_soFilter(""),
    m_bExtentChanged(false),
    m_bContentChanged(false),
    m_poUpdateStatement(NULL),
    m_bInsertStatementWithFID(false),
    m_poInsertStatement(NULL),
    m_bDeferredSpatialIndexCreation(false),
    m_bHasSpatialIndex(-1),
    m_bDropRTreeTable(false),
    m_bPreservePrecision(true),
    m_bTruncateFields(false),
    m_bDeferredCreation(false),
    m_iFIDAsRegularColumnIndex(-1),
    m_bHasReadMetadataFromStorage(false),
    m_bHasTriedDetectingFID64(false),
    m_eASPatialVariant(GPKG_ATTRIBUTES)
{
    memset(m_abHasGeometryExtension, 0, sizeof(m_abHasGeometryExtension));

    m_poFeatureDefn = new OGRFeatureDefn( m_pszTableName );
    SetDescription( m_poFeatureDefn->GetName() );
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();
}

/************************************************************************/
/*                      ~OGRGeoPackageTableLayer()                      */
/************************************************************************/

OGRGeoPackageTableLayer::~OGRGeoPackageTableLayer()
{
    SyncToDisk();

    if( m_bDropRTreeTable )
    {
        ResetReading();

        char* pszSQL =
            sqlite3_mprintf("DROP TABLE \"%w\"", m_osRTreeName.c_str());
        SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        m_bDropRTreeTable = false;
    }

    /* Clean up resources in memory */
    if ( m_pszTableName )
        CPLFree( m_pszTableName );

    if ( m_poExtent )
        delete m_poExtent;

    if ( m_poUpdateStatement )
        sqlite3_finalize(m_poUpdateStatement);

    if ( m_poInsertStatement )
        sqlite3_finalize(m_poInsertStatement);
}

/************************************************************************/
/*                        PostInit()                                    */
/************************************************************************/

void OGRGeoPackageTableLayer::PostInit()
{
#ifdef SQLITE_HAS_COLUMN_METADATA
    if( !m_bIsTable )
    {
        /* Detect if the view columns have the FID and geom columns of a */
        /* table that has itself a spatial index */
        sqlite3_stmt* hStmt = NULL;
        char* pszSQL = sqlite3_mprintf("SELECT * FROM \"%w\"", m_pszTableName);
        CPL_IGNORE_RET_VAL(sqlite3_prepare_v2(m_poDS->GetDB(),
                                              pszSQL, -1, &hStmt, NULL));
        sqlite3_free(pszSQL);
        if( hStmt )
        {
            if( sqlite3_step(hStmt) == SQLITE_ROW )
            {
                OGRGeoPackageTableLayer* poLayerGeom = NULL;
                const int nRawColumns = sqlite3_column_count( hStmt );
                for( int iCol = 0; iCol < nRawColumns; iCol++ )
                {
                    CPLString osColName(SQLUnescape(
                                        sqlite3_column_name( hStmt, iCol )));
                    const char* pszTableName =
                        sqlite3_column_table_name( hStmt, iCol );
                    const char* pszOriginName =
                        sqlite3_column_origin_name( hStmt, iCol );
                    if( EQUAL(osColName, "OGC_FID") &&
                        (pszOriginName == NULL ||
                         osColName != pszOriginName) )
                    {
                        // in the case we have a OGC_FID column, and that
                        // is not the name of the original column, then
                        // interpret this as an explicit intent to be a
                        // PKID.
                        // We cannot just take the FID of a source table as
                        // a FID because of potential joins that would result
                        // in multiple records with same source FID.
                        m_pszFidColumn = CPLStrdup(osColName);
                        m_poFeatureDefn->DeleteFieldDefn(
                            m_poFeatureDefn->GetFieldIndex(osColName));
                    }
                    else if( pszTableName != NULL && pszOriginName != NULL )
                    {
                        OGRGeoPackageTableLayer* poLayer =
                            dynamic_cast<OGRGeoPackageTableLayer*>(
                            m_poDS->GetLayerByName(pszTableName));
                        if( poLayer != NULL &&
                            osColName == GetGeometryColumn() &&
                            strcmp(pszOriginName,
                                   poLayer->GetGeometryColumn()) == 0 )
                        {
                            poLayerGeom = poLayer;
                        }
                    }
                }

                if( poLayerGeom != NULL && poLayerGeom->HasSpatialIndex() )
                {
                    for( int iCol = 0; iCol < nRawColumns; iCol++ )
                    {
                        CPLString osColName(SQLUnescape(
                                            sqlite3_column_name( hStmt, iCol )));
                        const char* pszTableName =
                            sqlite3_column_table_name( hStmt, iCol );
                        const char* pszOriginName =
                            sqlite3_column_origin_name( hStmt, iCol );
                        if( pszTableName != NULL && pszOriginName != NULL )
                        {
                            OGRGeoPackageTableLayer* poLayer =
                                dynamic_cast<OGRGeoPackageTableLayer*>(
                                m_poDS->GetLayerByName(pszTableName));
                            if( poLayer != NULL && poLayer == poLayerGeom &&
                                 strcmp(pszOriginName,
                                        poLayer->GetFIDColumn()) == 0 )
                            {
                                m_bHasSpatialIndex = true;
                                m_osRTreeName = poLayerGeom->m_osRTreeName;
                                m_osFIDForRTree = osColName;
                                break;
                            }
                        }
                    }
                }
            }
            sqlite3_finalize(hStmt);
        }

        /* Update the columns string */
        BuildColumns();
    }
#endif
}

/************************************************************************/
/*                      CheckUpdatableTable()                           */
/************************************************************************/

bool OGRGeoPackageTableLayer::CheckUpdatableTable(const char* pszOperation)
{
    if( !m_poDS->GetUpdate() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  pszOperation);
        return false;
    }
/* -------------------------------------------------------------------- */
/*      Check that is a table and not a view                            */
/* -------------------------------------------------------------------- */
    if( !m_bIsTable )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Layer %s is not a table",
                 m_pszTableName);
        return false;
    }
    return true;
}

/************************************************************************/
/*                      CreateField()                                   */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::CreateField( OGRFieldDefn *poField,
                                             CPL_UNUSED int bApproxOK )
{
    if( !CheckUpdatableTable("CreateField") )
        return OGRERR_FAILURE;

    OGRFieldDefn oFieldDefn(poField);
    int nMaxWidth = 0;
    if( m_bPreservePrecision && poField->GetType() == OFTString )
        nMaxWidth = poField->GetWidth();
    else
        oFieldDefn.SetWidth(0);
    oFieldDefn.SetPrecision(0);

    if( m_pszFidColumn != NULL &&
        EQUAL( oFieldDefn.GetNameRef(), m_pszFidColumn ) &&
        oFieldDefn.GetType() != OFTInteger &&
        oFieldDefn.GetType() != OFTInteger64 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Wrong field type for %s",
                 oFieldDefn.GetNameRef());
        return OGRERR_FAILURE;
    }

    if( !m_bDeferredCreation )
    {
        CPLString osCommand;

        osCommand.Printf("ALTER TABLE \"%s\" ADD COLUMN \"%s\" %s",
                          SQLEscapeName(m_pszTableName).c_str(),
                          SQLEscapeName(poField->GetNameRef()).c_str(),
                          GPkgFieldFromOGR(poField->GetType(),
                                           poField->GetSubType(),
                                           nMaxWidth));
        if(  !poField->IsNullable() )
            osCommand += " NOT NULL";
        if( poField->GetDefault() != NULL && !poField->IsDefaultDriverSpecific() )
        {
            osCommand += " DEFAULT ";
            int nYear = 0;
            int nMonth = 0;
            int nDay = 0;
            int nHour = 0;
            int nMinute = 0;
            float fSecond = 0.0f;
            if( poField->GetType() == OFTDateTime &&
                sscanf(poField->GetDefault(), "'%d/%d/%d %d:%d:%f'",
                       &nYear, &nMonth, &nDay,
                       &nHour, &nMinute, &fSecond) == 6 )
            {
                if( strchr(poField->GetDefault(), '.') == NULL )
                    osCommand += CPLSPrintf("'%04d-%02d-%02dT%02d:%02d:%02dZ'",
                                        nYear, nMonth, nDay, nHour, nMinute, (int)(fSecond+0.5));
                else
                    osCommand += CPLSPrintf("'%04d-%02d-%02dT%02d:%02d:%06.3fZ'",
                                            nYear, nMonth, nDay, nHour, nMinute, fSecond);
            }
            else
            {
                osCommand += poField->GetDefault();
            }
        }
        else if( !poField->IsNullable() )
        {
            // This is kind of dumb, but SQLite mandates a DEFAULT value
            // when adding a NOT NULL column in an ALTER TABLE ADD COLUMN
            // statement, which defeats the purpose of NOT NULL,
            // whereas it doesn't in CREATE TABLE
            osCommand += " DEFAULT ''";
        }

        OGRErr err = SQLCommand(m_poDS->GetDB(), osCommand.c_str());

        if ( err != OGRERR_NONE )
            return err;
    }

    m_poFeatureDefn->AddFieldDefn( &oFieldDefn );

    if( m_pszFidColumn != NULL &&
        EQUAL( oFieldDefn.GetNameRef(), m_pszFidColumn ) )
    {
        m_iFIDAsRegularColumnIndex = m_poFeatureDefn->GetFieldCount() - 1;
    }

    if( !m_bDeferredCreation )
    {
        ResetReading();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::CreateGeomField( OGRGeomFieldDefn *poGeomFieldIn,
                                                 int /* bApproxOK */ )
{
    if( !CheckUpdatableTable("CreateGeomField") )
        return OGRERR_FAILURE;

    if( m_poFeatureDefn->GetGeomFieldCount() == 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create more than on geometry field in GeoPackage");
        return OGRERR_FAILURE;
    }

    OGRwkbGeometryType eType = poGeomFieldIn->GetType();
    if( eType == wkbNone )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create geometry field of type wkbNone");
        return OGRERR_FAILURE;
    }

    OGRGeomFieldDefn oGeomField(poGeomFieldIn);
    if( EQUAL(oGeomField.GetNameRef(), "") )
    {
        oGeomField.SetName( "geom" );
    }

    OGRSpatialReference* poSRS = oGeomField.GetSpatialRef();
    if( poSRS != NULL )
        m_iSrs = m_poDS->GetSrsId(*poSRS);

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    if( !m_bDeferredCreation )
    {
        char *pszSQL = sqlite3_mprintf(
            "ALTER TABLE \"%w\" ADD COLUMN \"%w\" %s%s"
            ";"
            "UPDATE gpkg_contents SET data_type = 'features' "
            "WHERE lower(table_name) = lower('%q')",
            m_pszTableName, oGeomField.GetNameRef(),
            m_poDS->GetGeometryTypeString(oGeomField.GetType()),
            !oGeomField.IsNullable() ? " NOT NULL DEFAULT ''" : "",
            m_pszTableName);
        CPLString osSQL(pszSQL);
        sqlite3_free(pszSQL);

        if( m_poDS->HasExtensionsTable() )
        {
            // Suppress gdal_aspatial extension if this was the last
            // aspatial layer.
            bool bHasASpatialLayers = false;
            for(int i=0;i<m_poDS->GetLayerCount();i++)
            {
                if( m_poDS->GetLayer(i) != this &&
                    m_poDS->GetLayer(i)->GetLayerDefn()->GetGeomFieldCount() == 0 )
                    bHasASpatialLayers = true;
            }
            if( !bHasASpatialLayers )
            {
                osSQL +=
                    ";"
                    "DELETE FROM gpkg_extensions WHERE "
                    "extension_name = 'gdal_aspatial' "
                    "AND table_name IS NULL "
                    "AND column_name IS NULL";
            }
        }
        OGRErr err = SQLCommand(m_poDS->GetDB(), osSQL);
        if ( err != OGRERR_NONE )
            return err;
    }

    m_poFeatureDefn->AddGeomFieldDefn( &oGeomField );

    if( !m_bDeferredCreation )
    {
        OGRErr err = RegisterGeometryColumn();
        if ( err != OGRERR_NONE )
            return err;

        ResetReading();
    }

    return OGRERR_NONE;
}

#ifdef ENABLE_GPKG_OGR_CONTENTS

/************************************************************************/
/*                      DisableFeatureCount()                           */
/************************************************************************/

void OGRGeoPackageTableLayer::DisableFeatureCount(bool bInMemoryOnly)
{
    m_nTotalFeatureCount = -1;
    if( !bInMemoryOnly && m_poDS->m_bHasGPKGOGRContents )
    {
        char* pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_ogr_contents SET feature_count = NULL WHERE "
            "lower(table_name )= lower('%q')",
            m_pszTableName);
        SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }
}

/************************************************************************/
/*                      CreateTriggers()                                */
/************************************************************************/

void OGRGeoPackageTableLayer::CreateTriggers(const char* pszTableName)
{
    if( m_bAddOGRFeatureCountTriggers )
    {
        if( pszTableName == NULL )
            pszTableName = m_pszTableName;

        m_bOGRFeatureCountTriggersEnabled = true;
        m_bAddOGRFeatureCountTriggers = false;
        m_bFeatureCountTriggersDeletedInTransaction = false;

        CPLDebug("GPKG", "Creating insert/delete feature_count triggers");
        char* pszSQL = sqlite3_mprintf(
            "CREATE TRIGGER \"trigger_insert_feature_count_%w\" "
            "AFTER INSERT ON \"%w\" "
            "BEGIN UPDATE gpkg_ogr_contents SET feature_count = "
            "feature_count + 1 WHERE lower(table_name) = lower('%q'); END;",
            pszTableName, pszTableName, pszTableName);
        SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);

        pszSQL = sqlite3_mprintf(
            "CREATE TRIGGER \"trigger_delete_feature_count_%w\" "
            "AFTER DELETE ON \"%w\" "
            "BEGIN UPDATE gpkg_ogr_contents SET feature_count = "
            "feature_count - 1 WHERE lower(table_name) = lower('%q'); END;",
            pszTableName, pszTableName, pszTableName);
        SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }
}

/************************************************************************/
/*                      DisableTriggers()                               */
/************************************************************************/

void OGRGeoPackageTableLayer::DisableTriggers(bool bNullifyFeatureCount)
{
    if( m_bOGRFeatureCountTriggersEnabled )
    {
        m_bOGRFeatureCountTriggersEnabled = false;
        m_bAddOGRFeatureCountTriggers = true;
        m_bFeatureCountTriggersDeletedInTransaction =
            m_poDS->IsInTransaction();

        CPLDebug("GPKG", "Deleting insert/delete feature_count triggers");

        char* pszSQL = sqlite3_mprintf(
            "DROP TRIGGER \"trigger_insert_feature_count_%w\"",
            m_pszTableName);
        SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);

        pszSQL = sqlite3_mprintf(
            "DROP TRIGGER \"trigger_delete_feature_count_%w\"",
            m_pszTableName);
        SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);

        if( m_poDS->m_bHasGPKGOGRContents && bNullifyFeatureCount )
        {
            pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_ogr_contents SET feature_count = NULL WHERE "
                "lower(table_name )= lower('%q')",
                m_pszTableName);
            SQLCommand(m_poDS->GetDB(), pszSQL);
            sqlite3_free(pszSQL);
        }
    }
}

#endif // #ifdef ENABLE_GPKG_OGR_CONTENTS

/************************************************************************/
/*                      CheckGeometryType()                             */
/************************************************************************/

void OGRGeoPackageTableLayer::CheckGeometryType( OGRFeature *poFeature )
{
    OGRwkbGeometryType eLayerGeomType = wkbFlatten(GetGeomType());
    if( eLayerGeomType != wkbNone && eLayerGeomType != wkbUnknown )
    {
        OGRGeometry* poGeom = poFeature->GetGeometryRef();
        if( poGeom != NULL )
        {
            OGRwkbGeometryType eGeomType =
                wkbFlatten(poGeom->getGeometryType());
            if( !OGR_GT_IsSubClassOf(eGeomType, eLayerGeomType) &&
                m_eSetBadGeomTypeWarned.find(eGeomType) ==
                                        m_eSetBadGeomTypeWarned.end() )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "A geometry of type %s is inserted into layer %s "
                         "of geometry type %s, which is not allowed. "
                         "This warning will no longer be emitted for this "
                         "combination of layer and feature geometry type.",
                         OGRToOGCGeomType(eGeomType),
                         GetName(),
                         OGRToOGCGeomType(eLayerGeomType));
                m_eSetBadGeomTypeWarned.insert(eGeomType);
            }
        }
    }
}

/************************************************************************/
/*                      ICreateFeature()                                 */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::ICreateFeature( OGRFeature *poFeature )
{
    if( !m_poDS->GetUpdate() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "CreateFeature");
        return OGRERR_FAILURE;
    }

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if( m_bOGRFeatureCountTriggersEnabled )
    {
        DisableTriggers();
    }
#endif

    CheckGeometryType(poFeature);

    /* Substitute default values for null Date/DateTime fields as the standard */
    /* format of SQLite is not the one mandated by GeoPackage */
    poFeature->FillUnsetWithDefault(FALSE, NULL);
    bool bHasDefaultValue = false;
    const int nFieldCount = m_poFeatureDefn->GetFieldCount();
    for( int iField = 0; iField < nFieldCount; iField++ )
    {
        if( poFeature->IsFieldSet( iField ) )
            continue;
        const char* pszDefault = poFeature->GetFieldDefnRef(iField)->GetDefault();
        if( pszDefault != NULL )
        {
            bHasDefaultValue = true;
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

    /* If there's a unset field with a default value, then we must create */
    /* a specific INSERT statement to avoid unset fields to be bound to NULL */
    if( m_poInsertStatement && (bHasDefaultValue || m_bInsertStatementWithFID != (poFeature->GetFID() != OGRNullFID)) )
    {
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = NULL;
    }

    if ( ! m_poInsertStatement )
    {
        /* Construct a SQL INSERT statement from the OGRFeature */
        /* Only work with fields that are set */
        /* Do not stick values into SQL, use placeholder and bind values later */
        m_bInsertStatementWithFID = poFeature->GetFID() != OGRNullFID;
        CPLString osCommand = FeatureGenerateInsertSQL(poFeature, m_bInsertStatementWithFID, !bHasDefaultValue);

        /* Prepare the SQL into a statement */
        sqlite3 *poDb = m_poDS->GetDB();
        int err = sqlite3_prepare_v2(poDb, osCommand, -1, &m_poInsertStatement, NULL);
        if ( err != SQLITE_OK )
        {
            m_poInsertStatement = NULL;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "failed to prepare SQL: %s", osCommand.c_str());
            return OGRERR_FAILURE;
        }
    }

    /* Bind values onto the statement now */
    OGRErr errOgr = FeatureBindInsertParameters(poFeature, m_poInsertStatement,
                                                m_bInsertStatementWithFID,
                                                !bHasDefaultValue);
    if ( errOgr != OGRERR_NONE )
    {
        sqlite3_reset(m_poInsertStatement);
        sqlite3_clear_bindings(m_poInsertStatement);
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = NULL;
        return errOgr;
    }

    /* From here execute the statement and check errors */
    int err = sqlite3_step(m_poInsertStatement);
    if ( ! (err == SQLITE_OK || err == SQLITE_DONE) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "failed to execute insert : %s",
                  sqlite3_errmsg(m_poDS->GetDB()) ? sqlite3_errmsg(m_poDS->GetDB()) : "");
        sqlite3_reset(m_poInsertStatement);
        sqlite3_clear_bindings(m_poInsertStatement);
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = NULL;
        return OGRERR_FAILURE;
    }

    sqlite3_reset(m_poInsertStatement);
    sqlite3_clear_bindings(m_poInsertStatement);

    if( bHasDefaultValue )
    {
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = NULL;
    }

    /* Update the layer extents with this new object */
    if( IsGeomFieldSet(poFeature) )
    {
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(0);
        if( !poGeom->IsEmpty() )
        {
            OGREnvelope oEnv;
            poGeom->getEnvelope(&oEnv);
            UpdateExtent(&oEnv);
        }
    }

    /* Read the latest FID value */
    GIntBig nFID = sqlite3_last_insert_rowid(m_poDS->GetDB());
    if( nFID || poFeature->GetFID() == 0 )
    {
        poFeature->SetFID(nFID);
        if( m_iFIDAsRegularColumnIndex >= 0 )
            poFeature->SetField( m_iFIDAsRegularColumnIndex, nFID );
    }
    else
    {
        poFeature->SetFID(OGRNullFID);
    }

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if( m_nTotalFeatureCount >= 0 )
        m_nTotalFeatureCount++;
#endif

    m_bContentChanged = true;

    /* All done! */
    return OGRERR_NONE;
}

/************************************************************************/
/*                          ISetFeature()                                */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::ISetFeature( OGRFeature *poFeature )
{
    if( !m_poDS->GetUpdate() || m_pszFidColumn == NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "SetFeature");
        return OGRERR_FAILURE;
    }

    /* No FID? We can't set, we have to create */
    if ( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FID required on features given to SetFeature()." );
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

    CheckGeometryType(poFeature);

    /* Old version of SQLite have issues with some of the spatial index triggers */
#if SQLITE_VERSION_NUMBER < 3007008
    if( HasSpatialIndex() )
    {
        if ( ! m_poUpdateStatement )
        {
            /* Construct a SQL INSERT statement from the OGRFeature */
            /* Only work with fields that are set */
            /* Do not stick values into SQL, use placeholder and bind values later */
            CPLString osCommand = FeatureGenerateInsertSQL(poFeature, true, true);

            /* Prepare the SQL into a statement */
            int err = sqlite3_prepare_v2(m_poDS->GetDB(), osCommand, -1, &m_poUpdateStatement, NULL);
            if ( err != SQLITE_OK )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "failed to prepare SQL: %s", osCommand.c_str());
                return OGRERR_FAILURE;
            }
        }

        sqlite3_stmt* hBackupStmt = m_poUpdateStatement;
        m_poUpdateStatement = NULL;

#ifdef ENABLE_GPKG_OGR_CONTENTS
        GIntBig nTotalFeatureCountBackup = m_nTotalFeatureCount;
#endif
        OGRErr errOgr = DeleteFeature( poFeature->GetFID() );

        m_poUpdateStatement = hBackupStmt;

        if ( errOgr != OGRERR_NONE )
            return errOgr;

        /* Bind values onto the statement now */
        errOgr = FeatureBindInsertParameters(poFeature, m_poUpdateStatement, true, true);
        if ( errOgr != OGRERR_NONE )
            return errOgr;
#ifdef ENABLE_GPKG_OGR_CONTENTS
        m_nTotalFeatureCount = nTotalFeatureCountBackup;
#endif
    }
    else
#endif
    {
        if ( ! m_poUpdateStatement )
        {
            /* Construct a SQL UPDATE statement from the OGRFeature */
            /* Only work with fields that are set */
            /* Do not stick values into SQL, use placeholder and bind values later */
            CPLString osCommand = FeatureGenerateUpdateSQL(poFeature);
            if( osCommand.empty() )
                return OGRERR_NONE;

            /* Prepare the SQL into a statement */
            int err = sqlite3_prepare_v2(m_poDS->GetDB(), osCommand, -1, &m_poUpdateStatement, NULL);
            if ( err != SQLITE_OK )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "failed to prepare SQL: %s", osCommand.c_str());
                return OGRERR_FAILURE;
            }
        }

        /* Bind values onto the statement now */
        OGRErr errOgr = FeatureBindUpdateParameters(poFeature, m_poUpdateStatement);
        if ( errOgr != OGRERR_NONE )
        {
            sqlite3_reset(m_poUpdateStatement);
            sqlite3_clear_bindings(m_poUpdateStatement);
            return errOgr;
        }
    }

    /* From here execute the statement and check errors */
    int err = sqlite3_step(m_poUpdateStatement);
    if ( ! (err == SQLITE_OK || err == SQLITE_DONE) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "failed to execute update : %s",
                  sqlite3_errmsg( m_poDS->GetDB() ) );
        sqlite3_reset(m_poUpdateStatement);
        sqlite3_clear_bindings(m_poUpdateStatement);
        return OGRERR_FAILURE;
    }

    sqlite3_reset(m_poUpdateStatement);
    sqlite3_clear_bindings(m_poUpdateStatement);

    /* Only update the envelope if we changed something */
    OGRErr eErr = (sqlite3_changes(m_poDS->GetDB()) > 0) ? OGRERR_NONE : OGRERR_NON_EXISTING_FEATURE;
    if (eErr == OGRERR_NONE)
    {
        /* Update the layer extents with this new object */
        if( IsGeomFieldSet(poFeature) )
        {
            OGRGeometry* poGeom = poFeature->GetGeomFieldRef(0);
            if( !poGeom->IsEmpty() )
            {
                OGREnvelope oEnv;
                poGeom->getEnvelope(&oEnv);
                UpdateExtent(&oEnv);
            }
        }

        m_bContentChanged = true;
    }

    /* All done! */
    return eErr;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : NULL;

    if( pszQuery == NULL )
        osQuery = "";
    else
        osQuery = pszQuery;

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                      ResetReading()                                  */
/************************************************************************/

void OGRGeoPackageTableLayer::ResetReading()
{
    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return;

    OGRGeoPackageLayer::ResetReading();

    if ( m_poInsertStatement )
    {
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = NULL;
    }

    if ( m_poUpdateStatement )
    {
        sqlite3_finalize(m_poUpdateStatement);
        m_poUpdateStatement = NULL;
    }

    BuildColumns();
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::ResetStatement()

{
    ClearStatement();

    /* There is no active query statement set up, */
    /* so job #1 is to prepare the statement. */
    /* Append the attribute filter, if there is one */
    CPLString soSQL;
    if ( !m_soFilter.empty() )
    {
        soSQL.Printf("SELECT %s FROM \"%s\" m WHERE %s",
                     m_soColumns.c_str(),
                     SQLEscapeName(m_pszTableName).c_str(),
                     m_soFilter.c_str());

        if ( m_poFilterGeom != NULL && m_pszAttrQueryString == NULL &&
            HasSpatialIndex() )
        {
            OGREnvelope  sEnvelope;

            m_poFilterGeom->getEnvelope( &sEnvelope );

            bool bUseSpatialIndex = true;
            if( m_poExtent &&
                sEnvelope.MinX <= m_poExtent->MinX &&
                sEnvelope.MinY <= m_poExtent->MinY &&
                sEnvelope.MaxX >= m_poExtent->MaxX &&
                sEnvelope.MaxY >= m_poExtent->MaxY )
            {
                // Selecting from spatial filter on whole extent can be rather
                // slow. So use function based filtering, just in case the
                // advertized global extent might be wrong. Otherwise we might
                // just discard completely the spatial filter.
                bUseSpatialIndex = false;
            }

            if( bUseSpatialIndex &&
                !CPLIsInf(sEnvelope.MinX) && !CPLIsInf(sEnvelope.MinY) &&
                !CPLIsInf(sEnvelope.MaxX) && !CPLIsInf(sEnvelope.MaxY) )
            {
                soSQL.Printf("SELECT %s FROM \"%s\" m "
                             "JOIN \"%s\" r "
                             "ON m.\"%s\" = r.id WHERE "
                             "r.maxx >= %.12f AND r.minx <= %.12f AND "
                             "r.maxy >= %.12f AND r.miny <= %.12f",
                             m_soColumns.c_str(),
                             SQLEscapeName(m_pszTableName).c_str(),
                             SQLEscapeName(m_osRTreeName).c_str(),
                             SQLEscapeName(m_osFIDForRTree).c_str(),
                             sEnvelope.MinX - 1e-11, sEnvelope.MaxX + 1e-11,
                             sEnvelope.MinY - 1e-11, sEnvelope.MaxY + 1e-11);
            }
        }
    }
    else
        soSQL.Printf("SELECT %s FROM \"%s\" m",
                     m_soColumns.c_str(),
                     SQLEscapeName(m_pszTableName).c_str());

    CPLDebug("GPKG", "ResetStatement(%s)", soSQL.c_str());

    int err = sqlite3_prepare_v2(
        m_poDS->GetDB(), soSQL.c_str(), -1, &m_poQueryStatement, NULL);
    if ( err != SQLITE_OK )
    {
        m_poQueryStatement = NULL;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "failed to prepare SQL: %s", soSQL.c_str());
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature* OGRGeoPackageTableLayer::GetNextFeature()
{
    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return NULL;

    CreateSpatialIndexIfNecessary();

    OGRFeature* poFeature = OGRGeoPackageLayer::GetNextFeature();
    if( poFeature && m_iFIDAsRegularColumnIndex >= 0 )
    {
        poFeature->SetField(m_iFIDAsRegularColumnIndex, poFeature->GetFID());
    }
    return poFeature;
}

/************************************************************************/
/*                        GetFeature()                                  */
/************************************************************************/

OGRFeature* OGRGeoPackageTableLayer::GetFeature(GIntBig nFID)
{
    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return NULL;

    CreateSpatialIndexIfNecessary();

    if( m_pszFidColumn == NULL )
        return OGRLayer::GetFeature(nFID);

    /* No filters apply, just use the FID */
    CPLString soSQL;
    soSQL.Printf("SELECT %s FROM \"%s\" m "
                 "WHERE \"%s\" = " CPL_FRMT_GIB,
                 m_soColumns.c_str(),
                 SQLEscapeName(m_pszTableName).c_str(),
                 SQLEscapeName(m_pszFidColumn).c_str(),
                 nFID);

    sqlite3_stmt* poStmt = NULL;
    int err = sqlite3_prepare_v2(
        m_poDS->GetDB(), soSQL.c_str(), -1, &poStmt, NULL);
    if ( err != SQLITE_OK )
    {
        sqlite3_finalize(poStmt);
        CPLError( CE_Failure, CPLE_AppDefined,
                  "failed to prepare SQL: %s", soSQL.c_str());
        return NULL;
    }

    /* Should be only one or zero results */
    err = sqlite3_step(poStmt);

    /* Aha, got one */
    if ( err == SQLITE_ROW )
    {
        OGRFeature* poFeature = TranslateFeature(poStmt);
        sqlite3_finalize(poStmt);
        if( m_iFIDAsRegularColumnIndex >= 0 )
        {
            poFeature->SetField(m_iFIDAsRegularColumnIndex, poFeature->GetFID());
        }
        return poFeature;
    }
    sqlite3_finalize(poStmt);

    /* Error out on all other return codes */
    return NULL;
}

/************************************************************************/
/*                        DeleteFeature()                               */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::DeleteFeature(GIntBig nFID)
{
    if ( !m_poDS->GetUpdate() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "DeleteFeature");
        return OGRERR_FAILURE;
    }
    if( m_pszFidColumn == NULL )
    {
        return OGRERR_FAILURE;
    }

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if( m_bOGRFeatureCountTriggersEnabled )
    {
        DisableTriggers();
    }
#endif

    /* Clear out any existing query */
    ResetReading();

    /* No filters apply, just use the FID */
    CPLString soSQL;
    soSQL.Printf("DELETE FROM \"%s\" WHERE \"%s\" = " CPL_FRMT_GIB,
                 SQLEscapeName(m_pszTableName).c_str(),
                 SQLEscapeName(m_pszFidColumn).c_str(), nFID);

    OGRErr eErr = SQLCommand(m_poDS->GetDB(), soSQL.c_str());
    if( eErr == OGRERR_NONE )
    {
        eErr = (sqlite3_changes(m_poDS->GetDB()) > 0) ? OGRERR_NONE : OGRERR_NON_EXISTING_FEATURE;

        if( eErr == OGRERR_NONE )
        {
#ifdef ENABLE_GPKG_OGR_CONTENTS
            if( m_nTotalFeatureCount >= 0 )
                m_nTotalFeatureCount--;
#endif

            m_bContentChanged = true;
        }
    }
    return eErr;
}

/************************************************************************/
/*                        SyncToDisk()                                  */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::SyncToDisk()
{
    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

#ifdef ENABLE_GPKG_OGR_CONTENTS
    CreateTriggers();
#endif

    if( !m_bDropRTreeTable )
    {
        CreateSpatialIndexIfNecessary();
    }

    /* Save metadata back to the database */
    SaveExtent();
    SaveTimestamp();

    return OGRERR_NONE;
}

/************************************************************************/
/*                        StartTransaction()                            */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::StartTransaction()
{
    return m_poDS->StartTransaction();
}

/************************************************************************/
/*                        CommitTransaction()                           */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::CommitTransaction()
{
    return m_poDS->CommitTransaction();
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::RollbackTransaction()
{
    return m_poDS->RollbackTransaction();
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig OGRGeoPackageTableLayer::GetFeatureCount( int /*bForce*/ )
{
#ifdef ENABLE_GPKG_OGR_CONTENTS
    if( m_poFilterGeom == NULL && m_pszAttrQueryString == NULL )
    {
        if( m_nTotalFeatureCount >= 0 )
        {
            return m_nTotalFeatureCount;
        }

        if( m_poDS->m_bHasGPKGOGRContents )
        {
            char* pszSQL = sqlite3_mprintf(
                "SELECT feature_count FROM gpkg_ogr_contents WHERE "
                "lower(table_name) = lower('%q') LIMIT 2",
                m_pszTableName);
            SQLResult oResult;
            OGRErr err = SQLQuery( m_poDS->GetDB(), pszSQL, &oResult);
            sqlite3_free(pszSQL);
            if( err == OGRERR_NONE && oResult.nRowCount == 1 )
            {
                const char* pszFeatureCount =
                                            SQLResultGetValue(&oResult, 0, 0);
                if( pszFeatureCount )
                {
                    m_nTotalFeatureCount = CPLAtoGIntBig(pszFeatureCount);
                }
            }
            SQLResultFree( &oResult );
            if( m_nTotalFeatureCount >= 0 )
            {
                return m_nTotalFeatureCount;
            }
        }
    }
#endif

    if( m_poFilterGeom != NULL && !m_bFilterIsEnvelope )
        return OGRGeoPackageLayer::GetFeatureCount();

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return 0;

    /* Ignore bForce, because we always do a full count on the database */
    OGRErr err;
    CPLString soSQL;
    if ( m_bIsTable && m_poFilterGeom != NULL && m_pszAttrQueryString == NULL &&
        HasSpatialIndex() )
    {
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );

        if( !CPLIsInf(sEnvelope.MinX) && !CPLIsInf(sEnvelope.MinY) &&
            !CPLIsInf(sEnvelope.MaxX) && !CPLIsInf(sEnvelope.MaxY) )
        {
            soSQL.Printf("SELECT COUNT(*) FROM \"%s\" WHERE "
                         "maxx >= %.12f AND minx <= %.12f AND "
                         "maxy >= %.12f AND miny <= %.12f",
                         SQLEscapeName(m_osRTreeName).c_str(),
                         sEnvelope.MinX - 1e-11, sEnvelope.MaxX + 1e-11,
                         sEnvelope.MinY - 1e-11, sEnvelope.MaxY + 1e-11);
        }
    }

    if( soSQL.empty() )
    {
        if ( !m_soFilter.empty() )
            soSQL.Printf("SELECT Count(*) FROM \"%s\" WHERE %s",
                         SQLEscapeName(m_pszTableName).c_str(),
                         m_soFilter.c_str());
        else
            soSQL.Printf("SELECT Count(*) FROM \"%s\"",
                         SQLEscapeName(m_pszTableName).c_str());
    }

    /* Just run the query directly and get back integer */
    GIntBig iFeatureCount = SQLGetInteger64(m_poDS->GetDB(), soSQL.c_str(), &err);

    /* Generic implementation uses -1 for error condition, so we will too */
    if ( err == OGRERR_NONE )
    {
#ifdef ENABLE_GPKG_OGR_CONTENTS
        if( m_bIsTable && m_poFilterGeom == NULL && m_pszAttrQueryString == NULL )
        {
            m_nTotalFeatureCount = iFeatureCount;

            if( m_poDS->GetUpdate() && m_poDS->m_bHasGPKGOGRContents )
            {
                const char* pszCount = CPLSPrintf(CPL_FRMT_GIB,
                                                  m_nTotalFeatureCount);
                char* pszSQL = sqlite3_mprintf(
                    "UPDATE gpkg_ogr_contents SET feature_count = %s WHERE "
                    "lower(table_name )= lower('%q')", pszCount, m_pszTableName);
                SQLCommand(m_poDS->GetDB(), pszSQL);
                sqlite3_free(pszSQL);
            }
        }
#endif
        return iFeatureCount;
    }
    else
        return -1;
}

/************************************************************************/
/*                        GetExtent()                                   */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    /* Extent already calculated! We're done. */
    if ( m_poExtent != NULL )
    {
        if ( psExtent )
        {
            *psExtent = *m_poExtent;
        }
        return OGRERR_NONE;
    }

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    /* User is OK with expensive calculation, fall back to */
    /* default implementation (scan all features) and save */
    /* the result for later */
    if ( bForce && m_poFeatureDefn->GetGeomFieldCount() )
    {
        const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
        char* pszSQL = sqlite3_mprintf(
            "SELECT MIN(ST_MinX(\"%w\")), MIN(ST_MinY(\"%w\")), "
            "MAX(ST_MaxX(\"%w\")), MAX(ST_MaxY(\"%w\")) FROM \"%w\" WHERE "
            "\"%w\" IS NOT NULL AND NOT ST_IsEmpty(\"%w\")",
            pszC, pszC, pszC, pszC, m_pszTableName, pszC, pszC);
        SQLResult oResult;
        OGRErr err = SQLQuery( m_poDS->GetDB(), pszSQL, &oResult);
        sqlite3_free(pszSQL);
        delete m_poExtent;
        m_poExtent = NULL;
        if( err == OGRERR_NONE && oResult.nRowCount == 1 &&
            SQLResultGetValue(&oResult, 0, 0) != NULL )
        {
            psExtent->MinX = CPLAtof(SQLResultGetValue(&oResult, 0, 0));
            psExtent->MinY = CPLAtof(SQLResultGetValue(&oResult, 1, 0));
            psExtent->MaxX = CPLAtof(SQLResultGetValue(&oResult, 2, 0));
            psExtent->MaxY = CPLAtof(SQLResultGetValue(&oResult, 3, 0));
            m_poExtent = new OGREnvelope( *psExtent );
            m_bExtentChanged = true;
            SaveExtent();
        }
        else
        {
            pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_contents SET "
                "min_x = NULL, min_y = NULL, "
                "max_x = NULL, max_y = NULL "
                "WHERE lower(table_name) = lower('%q') AND "
                "Lower(data_type) = 'features'",
                m_pszTableName);
            SQLCommand( m_poDS->GetDB(), pszSQL);
            sqlite3_free(pszSQL);
            m_bExtentChanged = false;
            err = OGRERR_FAILURE; // we didn't get an extent
        }
        SQLResultFree(&oResult);
        return err;
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                      RecomputeExtent()                               */
/************************************************************************/

void OGRGeoPackageTableLayer::RecomputeExtent()
{
    m_bExtentChanged = true;
    delete m_poExtent;
    m_poExtent = NULL;
    OGREnvelope sExtent;
    GetExtent(&sExtent, true);
}

/************************************************************************/
/*                      TestCapability()                                */
/************************************************************************/

int OGRGeoPackageTableLayer::TestCapability ( const char * pszCap )
{
    if ( EQUAL(pszCap, OLCSequentialWrite) )
    {
        return m_poDS->GetUpdate();
    }
    else if ( EQUAL(pszCap, OLCCreateField) ||
              EQUAL(pszCap, OLCDeleteField) ||
              EQUAL(pszCap, OLCAlterFieldDefn) ||
              EQUAL(pszCap, OLCReorderFields) )
    {
        return m_poDS->GetUpdate() && m_bIsTable;
    }
    else if ( EQUAL(pszCap, OLCDeleteFeature) ||
              EQUAL(pszCap, OLCRandomWrite) )
    {
        return m_poDS->GetUpdate() && m_pszFidColumn != NULL;
    }
    else if ( EQUAL(pszCap, OLCRandomRead) )
    {
        return m_pszFidColumn != NULL;
    }
    else if ( EQUAL(pszCap, OLCTransactions) )
    {
        return TRUE;
    }
#ifdef ENABLE_GPKG_OGR_CONTENTS
    else if ( EQUAL(pszCap, OLCFastFeatureCount) )
    {
        return  m_poFilterGeom == NULL && m_pszAttrQueryString == NULL &&
                m_nTotalFeatureCount >= 0;
    }
#endif
    else if ( EQUAL(pszCap, OLCFastSpatialFilter) )
    {
        return HasSpatialIndex() || m_bDeferredSpatialIndexCreation;
    }
    else if ( EQUAL(pszCap, OLCFastGetExtent) )
    {
        return ( m_poExtent != NULL );
    }
    else if( EQUAL(pszCap,OLCCurveGeometries) )
        return TRUE;
    else if( EQUAL(pszCap,OLCMeasuredGeometries) )
        return TRUE;
    else
    {
        return OGRGeoPackageLayer::TestCapability(pszCap);
    }
}

/************************************************************************/
/*                     CreateSpatialIndexIfNecessary()                  */
/************************************************************************/

void OGRGeoPackageTableLayer::CreateSpatialIndexIfNecessary()
{
    if( m_bDeferredSpatialIndexCreation )
    {
        CreateSpatialIndex();
    }
}

/************************************************************************/
/*                       CreateSpatialIndex()                           */
/************************************************************************/

typedef struct
{
    GIntBig nId;
    double  dfMinX;
    double  dfMinY;
    double  dfMaxX;
    double  dfMaxY;
} GPKGRTreeEntry;

bool OGRGeoPackageTableLayer::CreateSpatialIndex(const char* pszTableName)
{
    OGRErr err;

    if( !CheckUpdatableTable("CreateSpatialIndex") )
        return false;

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return false;

    m_bDeferredSpatialIndexCreation = false;

    if( m_pszFidColumn == NULL )
        return false;

    if( HasSpatialIndex() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Spatial index already existing");
        return false;
    }

    if( m_poFeatureDefn->GetGeomFieldCount() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No geometry column");
        return false;
    }
    if( m_poDS->CreateExtensionsTableIfNecessary() != OGRERR_NONE )
        return false;

    const char* pszT = (pszTableName) ? pszTableName : m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    const char* pszI = GetFIDColumn();

    m_osRTreeName = "rtree_";
    m_osRTreeName += pszT;
    m_osRTreeName += "_";
    m_osRTreeName += pszC;
    m_osFIDForRTree = m_pszFidColumn;

    m_poDS->SoftStartTransaction();

    char* pszSQL;
    /* Create virtual table */
    if( !m_bDropRTreeTable )
    {
        pszSQL = sqlite3_mprintf(
                    "CREATE VIRTUAL TABLE \"%w\" USING rtree(id, minx, maxx, miny, maxy)",
                    m_osRTreeName.c_str() );
        err = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        if( err != OGRERR_NONE )
        {
            m_poDS->SoftRollbackTransaction();
            return false;
        }
    }
    m_bDropRTreeTable = false;

    /* Populate the RTree */
#ifdef NO_PROGRESSIVE_RTREE_INSERTION
    pszSQL = sqlite3_mprintf(
        "INSERT INTO \"%w\" "
        "SELECT \"%w\", ST_MinX(\"%w\"), ST_MaxX(\"%w\"), "
        "ST_MinY(\"%w\"), ST_MaxY(\"%w\") FROM \"%w\" "
        "WHERE \"%w\" NOT NULL AND NOT ST_IsEmpty(\"%w\")",
        m_osRTreeName.c_str(), pszI, pszC, pszC, pszC, pszC, pszT, pszC, pszC );
    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if( err != OGRERR_NONE )
    {
        m_poDS->SoftRollbackTransaction();
        return false;
    }
#else
    pszSQL = sqlite3_mprintf(
        "SELECT \"%w\", ST_MinX(\"%w\"), ST_MaxX(\"%w\"), "
        "ST_MinY(\"%w\"), ST_MaxY(\"%w\") FROM \"%w\" "
        "WHERE \"%w\" NOT NULL AND NOT ST_IsEmpty(\"%w\")",
            pszI, pszC, pszC, pszC, pszC, pszT, pszC, pszC );
    sqlite3_stmt* hIterStmt = NULL;
    if ( sqlite3_prepare_v2(m_poDS->GetDB(), pszSQL, -1, &hIterStmt, NULL)
                                                            != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "failed to prepare SQL: %s", pszSQL);
        sqlite3_free(pszSQL);
        m_poDS->SoftRollbackTransaction();
        return false;
    }
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf(
        "INSERT INTO \"%w\" VALUES (?,?,?,?,?)",
        m_osRTreeName.c_str());
    sqlite3_stmt* hInsertStmt = NULL;
    if ( sqlite3_prepare_v2(m_poDS->GetDB(), pszSQL, -1, &hInsertStmt, NULL)
                                                            != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "failed to prepare SQL: %s", pszSQL);
        sqlite3_free(pszSQL);
        sqlite3_finalize(hIterStmt);
        m_poDS->SoftRollbackTransaction();
        return false;
    }
    sqlite3_free(pszSQL);

    // Insert entries in RTree by chuncks of 100000
    std::vector<GPKGRTreeEntry> aoEntries;
    GUIntBig nEntryCount = 0;
    const size_t nChunkSize = 100000;
    while( true )
    {
        int sqlite_err = sqlite3_step(hIterStmt);
        bool bFinished = false;
        if( sqlite_err == SQLITE_ROW )
        {
            GPKGRTreeEntry sEntry;
            sEntry.nId = sqlite3_column_int64(hIterStmt, 0);
            sEntry.dfMinX = sqlite3_column_double(hIterStmt, 1);
            sEntry.dfMaxX = sqlite3_column_double(hIterStmt, 2);
            sEntry.dfMinY = sqlite3_column_double(hIterStmt, 3);
            sEntry.dfMaxY = sqlite3_column_double(hIterStmt, 4);
            aoEntries.push_back(sEntry);
        }
        else if( sqlite_err == SQLITE_DONE )
        {
            bFinished = true;
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "failed to iterate over features while inserting in "
                      "RTree: %s",
                      sqlite3_errmsg( m_poDS->GetDB() ) );
            sqlite3_finalize(hIterStmt);
            sqlite3_finalize(hInsertStmt);
            m_poDS->SoftRollbackTransaction();
            return false;
        }

        if( aoEntries.size() == nChunkSize || bFinished )
        {
            for( size_t i = 0; i < aoEntries.size(); ++i )
            {
                sqlite3_reset(hInsertStmt);

                sqlite3_bind_int64(hInsertStmt,1,aoEntries[i].nId);
                sqlite3_bind_double(hInsertStmt,2,aoEntries[i].dfMinX);
                sqlite3_bind_double(hInsertStmt,3,aoEntries[i].dfMaxX);
                sqlite3_bind_double(hInsertStmt,4,aoEntries[i].dfMinY);
                sqlite3_bind_double(hInsertStmt,5,aoEntries[i].dfMaxY);
                sqlite_err = sqlite3_step(hInsertStmt);
                if ( sqlite_err != SQLITE_OK && sqlite_err != SQLITE_DONE )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "failed to execute insertion in RTree : %s",
                              sqlite3_errmsg( m_poDS->GetDB() ) );
                    sqlite3_finalize(hIterStmt);
                    sqlite3_finalize(hInsertStmt);
                    m_poDS->SoftRollbackTransaction();
                    return false;
                }
            }

            nEntryCount += aoEntries.size();
            CPLDebug("GPKG", CPL_FRMT_GUIB " rows inserted into %s",
                     nEntryCount, m_osRTreeName.c_str());

            aoEntries.clear();
            if( bFinished )
                break;
        }
    }

    sqlite3_finalize(hIterStmt);
    sqlite3_finalize(hInsertStmt);
#endif

    CPLString osSQL;

    /* Register the table in gpkg_extensions */
    pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_extensions "
        "(table_name,column_name,extension_name,definition,scope) "
        "VALUES ('%q', '%q', 'gpkg_rtree_index', "
        "'GeoPackage 1.0 Specification Annex L', 'write-only')",
        pszT, pszC );
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    /* Define Triggers to Maintain Spatial Index Values */

    /* Conditions: Insertion of non-empty geometry
       Actions   : Insert record into rtree */
    pszSQL = sqlite3_mprintf(
                   "CREATE TRIGGER \"%w_insert\" AFTER INSERT ON \"%w\" "
                   "WHEN (new.\"%w\" NOT NULL AND NOT ST_IsEmpty(NEW.\"%w\")) "
                   "BEGIN "
                   "INSERT OR REPLACE INTO \"%w\" VALUES ("
                   "NEW.\"%w\","
                   "ST_MinX(NEW.\"%w\"), ST_MaxX(NEW.\"%w\"),"
                   "ST_MinY(NEW.\"%w\"), ST_MaxY(NEW.\"%w\")"
                   "); "
                   "END",
                   m_osRTreeName.c_str(), pszT,
                   pszC, pszC,
                   m_osRTreeName.c_str(),
                   pszI,
                   pszC, pszC,
                   pszC, pszC);
    osSQL += ";";
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    /* Conditions: Update of geometry column to non-empty geometry
               No row ID change
       Actions   : Update record in rtree */
    pszSQL = sqlite3_mprintf(
                   "CREATE TRIGGER \"%w_update1\" AFTER UPDATE OF \"%w\" ON \"%w\" "
                   "WHEN OLD.\"%w\" = NEW.\"%w\" AND "
                   "(NEW.\"%w\" NOTNULL AND NOT ST_IsEmpty(NEW.\"%w\")) "
                   "BEGIN "
                   "INSERT OR REPLACE INTO \"%w\" VALUES ("
                   "NEW.\"%w\","
                   "ST_MinX(NEW.\"%w\"), ST_MaxX(NEW.\"%w\"),"
                   "ST_MinY(NEW.\"%w\"), ST_MaxY(NEW.\"%w\")"
                   "); "
                   "END",
                   m_osRTreeName.c_str(), pszC, pszT,
                   pszI, pszI,
                   pszC, pszC,
                   m_osRTreeName.c_str(),
                   pszI,
                   pszC, pszC,
                   pszC, pszC);
    osSQL += ";";
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    /* Conditions: Update of geometry column to empty geometry
               No row ID change
       Actions   : Remove record from rtree */
    pszSQL = sqlite3_mprintf(
                   "CREATE TRIGGER \"%w_update2\" AFTER UPDATE OF \"%w\" ON \"%w\" "
                   "WHEN OLD.\"%w\" = NEW.\"%w\" AND "
                   "(NEW.\"%w\" ISNULL OR ST_IsEmpty(NEW.\"%w\")) "
                   "BEGIN "
                   "DELETE FROM \"%w\" WHERE id = OLD.\"%w\"; "
                   "END",
                   m_osRTreeName.c_str(), pszC, pszT,
                   pszI, pszI,
                   pszC, pszC,
                   m_osRTreeName.c_str(), pszI);
    osSQL += ";";
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    /* Conditions: Update of any column
                    Row ID change
                    Non-empty geometry
        Actions   : Remove record from rtree for old <i>
                    Insert record into rtree for new <i> */
    pszSQL = sqlite3_mprintf(
                   "CREATE TRIGGER \"%w_update3\" AFTER UPDATE OF \"%w\" ON \"%w\" "
                   "WHEN OLD.\"%w\" != NEW.\"%w\" AND "
                   "(NEW.\"%w\" NOTNULL AND NOT ST_IsEmpty(NEW.\"%w\")) "
                   "BEGIN "
                   "DELETE FROM \"%w\" WHERE id = OLD.\"%w\"; "
                   "INSERT OR REPLACE INTO \"%w\" VALUES ("
                   "NEW.\"%w\","
                   "ST_MinX(NEW.\"%w\"), ST_MaxX(NEW.\"%w\"),"
                   "ST_MinY(NEW.\"%w\"), ST_MaxY(NEW.\"%w\")"
                   "); "
                   "END",
                   m_osRTreeName.c_str(), pszC, pszT,
                   pszI, pszI,
                   pszC, pszC,
                   m_osRTreeName.c_str(), pszI,
                   m_osRTreeName.c_str(),
                   pszI,
                   pszC, pszC,
                   pszC, pszC);
    osSQL += ";";
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    /* Conditions: Update of any column
                    Row ID change
                    Empty geometry
        Actions   : Remove record from rtree for old and new <i> */
    pszSQL = sqlite3_mprintf(
                   "CREATE TRIGGER \"%w_update4\" AFTER UPDATE ON \"%w\" "
                   "WHEN OLD.\"%w\" != NEW.\"%w\" AND "
                   "(NEW.\"%w\" ISNULL OR ST_IsEmpty(NEW.\"%w\")) "
                   "BEGIN "
                   "DELETE FROM \"%w\" WHERE id IN (OLD.\"%w\", NEW.\"%w\"); "
                   "END",
                   m_osRTreeName.c_str(), pszT,
                   pszI, pszI,
                   pszC, pszC,
                   m_osRTreeName.c_str(), pszI, pszI);
    osSQL += ";";
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    /* Conditions: Row deleted
        Actions   : Remove record from rtree for old <i> */
    pszSQL = sqlite3_mprintf(
                   "CREATE TRIGGER \"%w_delete\" AFTER DELETE ON \"%w\" "
                   "WHEN old.\"%w\" NOT NULL "
                   "BEGIN "
                   "DELETE FROM \"%w\" WHERE id = OLD.\"%w\"; "
                   "END",
                   m_osRTreeName.c_str(), pszT,
                   pszC,
                   m_osRTreeName.c_str(), pszI);
    osSQL += ";";
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    err = SQLCommand(m_poDS->GetDB(), osSQL);
    if( err != OGRERR_NONE )
    {
        m_poDS->SoftRollbackTransaction();
        return false;
    }

    m_poDS->SoftCommitTransaction();

    m_bHasSpatialIndex = true;

    return true;
}

/************************************************************************/
/*                    CheckUnknownExtensions()                          */
/************************************************************************/

void OGRGeoPackageTableLayer::CheckUnknownExtensions()
{
    if( !m_poDS->HasExtensionsTable() )
        return;

    const char* pszT = m_pszTableName;

    /* We have only the SQL functions needed by the 3 following extensions */
    /* anything else will likely cause troubles */
    char* pszSQL = NULL;

    if( m_poFeatureDefn->GetGeomFieldCount() == 0 )
    {
        pszSQL = sqlite3_mprintf(
                    "SELECT extension_name, definition, scope "
                    "FROM gpkg_extensions WHERE (lower(table_name)=lower('%q') "
                    "AND extension_name IS NOT NULL "
                    "AND definition IS NOT NULL "
                    "AND scope IS NOT NULL) "
#ifdef WORKAROUND_SQLITE3_BUGS
                    "OR 0 "
#endif
                    "LIMIT 1000" // to avoid denial of service
                    ,pszT );
    }
    else
    {
        pszSQL = sqlite3_mprintf(
                    "SELECT extension_name, definition, scope "
                    "FROM gpkg_extensions WHERE (lower(table_name)=lower('%q') "
                    "AND extension_name IS NOT NULL "
                    "AND definition IS NOT NULL "
                    "AND scope IS NOT NULL "
                    "AND lower(column_name)=lower('%q') AND extension_name NOT IN ('gpkg_geom_CIRCULARSTRING', "
                    "'gpkg_geom_COMPOUNDCURVE', 'gpkg_geom_CURVEPOLYGON', 'gpkg_geom_MULTICURVE', "
                    "'gpkg_geom_MULTISURFACE', 'gpkg_geom_CURVE', 'gpkg_geom_SURFACE', "
                    "'gpkg_geom_POLYHEDRALSURFACE', 'gpkg_geom_TIN', 'gpkg_geom_TRIANGLE', "
                    "'gpkg_rtree_index', 'gpkg_geometry_type_trigger', 'gpkg_srs_id_trigger')) "
#ifdef WORKAROUND_SQLITE3_BUGS
                    "OR 0"
#endif
                    "LIMIT 1000" // to avoid denial of service
                    ,pszT,
                    m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef() );
    }
    SQLResult oResultTable;
    OGRErr err = SQLQuery(m_poDS->GetDB(), pszSQL, &oResultTable);
    sqlite3_free(pszSQL);
    if ( err == OGRERR_NONE && oResultTable.nRowCount > 0 )
    {
        for(int i=0; i<oResultTable.nRowCount;i++)
        {
            const char* pszExtName = SQLResultGetValue(&oResultTable, 0, i);
            const char* pszDefinition = SQLResultGetValue(&oResultTable, 1, i);
            const char* pszScope = SQLResultGetValue(&oResultTable, 2, i);
            if( m_poDS->GetUpdate() && EQUAL(pszScope, "write-only") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Layer %s relies on the '%s' (%s) extension that should "
                         "be implemented for safe write-support, but is not currently. "
                         "Update of that layer are strongly discouraged to avoid corruption.",
                         GetName(), pszExtName, pszDefinition);
            }
            else if( m_poDS->GetUpdate() && EQUAL(pszScope, "read-write") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Layer %s relies on the '%s' (%s) extension that should "
                         "be implemented in order to read/write it safely, but is not currently. "
                         "Some data may be missing while reading that layer, and updates are strongly discouraged.",
                         GetName(), pszExtName, pszDefinition);
            }
            else if( EQUAL(pszScope, "read-write") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Layer %s relies on the '%s' (%s) extension that should "
                         "be implemented in order to read it safely, but is not currently. "
                         "Some data may be missing while reading that layer.",
                         GetName(), pszExtName, pszDefinition);
            }
        }
    }
    SQLResultFree(&oResultTable);
}

/************************************************************************/
/*                     CreateGeometryExtensionIfNecessary()             */
/************************************************************************/

bool OGRGeoPackageTableLayer::CreateGeometryExtensionIfNecessary(
                                                    const OGRGeometry* poGeom)
{
    bool bRet = true;
    if( poGeom != NULL )
    {
        OGRwkbGeometryType eGType = wkbFlatten(poGeom->getGeometryType());
        if( eGType >= wkbGeometryCollection )
        {
            if( eGType > wkbGeometryCollection )
                CreateGeometryExtensionIfNecessary(eGType);
            const OGRGeometryCollection* poGC =
                            dynamic_cast<const OGRGeometryCollection*>(poGeom);
            if( poGC != NULL )
            {
                const int nSubGeoms = poGC->getNumGeometries();
                for( int i = 0; i < nSubGeoms; i++ )
                {
                    bRet &=
                    CreateGeometryExtensionIfNecessary(poGC->getGeometryRef(i));
                }
            }
        }
    }
    return bRet;
}

/************************************************************************/
/*                     CreateGeometryExtensionIfNecessary()             */
/************************************************************************/

bool OGRGeoPackageTableLayer::CreateGeometryExtensionIfNecessary(OGRwkbGeometryType eGType)
{
    eGType = wkbFlatten(eGType);
    CPLAssert(eGType > wkbGeometryCollection && eGType <= wkbTriangle);
    if( m_abHasGeometryExtension[eGType] )
        return true;

    if( m_poDS->CreateExtensionsTableIfNecessary() != OGRERR_NONE )
        return false;

    const char* pszT = m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    const char *pszGeometryType = m_poDS->GetGeometryTypeString(eGType);

    // Check first if the extension isn't registered
    char* pszSQL = sqlite3_mprintf(
        "SELECT 1 FROM gpkg_extensions WHERE lower(table_name) = lower('%q') AND "
        "lower(column_name) = lower('%q') AND extension_name = 'gpkg_geom_%s'",
         pszT, pszC, pszGeometryType);
    const bool bExists = SQLGetInteger(m_poDS->GetDB(), pszSQL, NULL) == 1;
    sqlite3_free(pszSQL);

    if( !bExists )
    {
        if( eGType == wkbPolyhedralSurface ||
            eGType == wkbTIN || eGType == wkbTriangle )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Registering non-standard gpkg_geom_%s extension",
                     pszGeometryType);
        }

        /* Register the table in gpkg_extensions */
        pszSQL = sqlite3_mprintf(
                    "INSERT INTO gpkg_extensions "
                    "(table_name,column_name,extension_name,definition,scope) "
                    "VALUES ('%q', '%q', 'gpkg_geom_%s', 'GeoPackage 1.0 Specification Annex J', 'read-write')",
                    pszT, pszC, pszGeometryType);
        OGRErr err = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        if ( err != OGRERR_NONE )
            return false;
    }

    m_abHasGeometryExtension[eGType] = true;
    return true;
}

/************************************************************************/
/*                        HasSpatialIndex()                             */
/************************************************************************/

bool OGRGeoPackageTableLayer::HasSpatialIndex()
{
    if( m_bHasSpatialIndex >= 0 )
        return CPL_TO_BOOL(m_bHasSpatialIndex);
    m_bHasSpatialIndex = false;

    if( m_pszFidColumn == NULL ||
        m_poFeatureDefn->GetGeomFieldCount() == 0 ||
        !m_poDS->HasExtensionsTable() )
        return false;

    const char* pszT = m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();

    /* Check into gpkg_extensions */
    char* pszSQL = sqlite3_mprintf(
                 "SELECT * FROM gpkg_extensions WHERE (lower(table_name)=lower('%q') "
                 "AND lower(column_name)=lower('%q') AND extension_name='gpkg_rtree_index')"
#ifdef WORKAROUND_SQLITE3_BUGS
                " OR 0"
#endif
                " LIMIT 2"
                 ,pszT, pszC );
    SQLResult oResultTable;
    OGRErr err = SQLQuery(m_poDS->GetDB(), pszSQL, &oResultTable);
    sqlite3_free(pszSQL);
    if ( err == OGRERR_NONE && oResultTable.nRowCount == 1 )
    {
        m_bHasSpatialIndex = true;
        m_osRTreeName = "rtree_";
        m_osRTreeName += pszT;
        m_osRTreeName += "_";
        m_osRTreeName += pszC;
        m_osFIDForRTree = m_pszFidColumn;
    }
    SQLResultFree(&oResultTable);

    return CPL_TO_BOOL(m_bHasSpatialIndex);
}

/************************************************************************/
/*                        DropSpatialIndex()                            */
/************************************************************************/

bool OGRGeoPackageTableLayer::DropSpatialIndex(bool bCalledFromSQLFunction)
{
    if( !CheckUpdatableTable("DropSpatialIndex") )
        return false;

    if( !HasSpatialIndex() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Spatial index not existing");
        return false;
    }

    const char* pszT = m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    char* pszSQL = sqlite3_mprintf(
        "DELETE FROM gpkg_extensions WHERE lower(table_name)=lower('%q') "
        "AND lower(column_name)=lower('%q') AND extension_name='gpkg_rtree_index'",
        pszT, pszC );
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    if( bCalledFromSQLFunction )
    {
        /* We cannot drop a table from a SQLite function call, so we just */
        /* remove the content and memorize that we will have to delete the */
        /* table later */
        m_bDropRTreeTable = true;
        pszSQL = sqlite3_mprintf("DELETE FROM \"%w\"", m_osRTreeName.c_str());
    }
    else
    {
        pszSQL = sqlite3_mprintf("DROP TABLE \"%w\"", m_osRTreeName.c_str());
    }
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("DROP TRIGGER \"%w_insert\"",
                             m_osRTreeName.c_str());
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("DROP TRIGGER \"%w_update1\"",
                             m_osRTreeName.c_str());
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("DROP TRIGGER \"%w_update2\"",
                             m_osRTreeName.c_str());
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("DROP TRIGGER \"%w_update3\"",
                             m_osRTreeName.c_str());
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("DROP TRIGGER \"%w_update4\"",
                             m_osRTreeName.c_str());
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("DROP TRIGGER \"%w_delete\"",
                             m_osRTreeName.c_str());
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    m_bHasSpatialIndex = false;
    return true;
}

/************************************************************************/
/*                          RenameTo()                                  */
/************************************************************************/

void OGRGeoPackageTableLayer::RenameTo(const char* pszDstTableName)
{
    ResetReading();
    SyncToDisk();

    char* pszSQL = sqlite3_mprintf(
        "SELECT 1 FROM sqlite_master WHERE lower(name) = lower('%q') "
        "AND type IN ('table', 'view')",
         pszDstTableName);
    const bool bAlreadyExists =
            SQLGetInteger(m_poDS->GetDB(), pszSQL, NULL) == 1;
    sqlite3_free(pszSQL);
    if( bAlreadyExists )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Table %s already exists",
                 pszDstTableName);
        return;
    }

    if( m_poDS->SoftStartTransaction() != OGRERR_NONE )
        return;

    const bool bHasSpatialIndex = HasSpatialIndex();
    if( bHasSpatialIndex )
    {
        DropSpatialIndex();
    }

#ifdef ENABLE_GPKG_OGR_CONTENTS
    DisableTriggers(false);
#endif

    pszSQL = sqlite3_mprintf(
        "UPDATE gpkg_geometry_columns SET table_name = '%q' WHERE "
        "lower(table_name )= lower('%q')",
        pszDstTableName, m_pszTableName);
    OGRErr eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    if( eErr == OGRERR_NONE )
    {
        // Rename the identifier if it defaulted to the table name
        pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_contents SET identifier = '%q' WHERE "
                "lower(table_name) = lower('%q') AND identifier = '%q'",
                pszDstTableName, m_pszTableName, m_pszTableName);
        eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }

    if( eErr == OGRERR_NONE )
    {
        pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_contents SET table_name = '%q' WHERE "
                "lower(table_name )= lower('%q')",
                pszDstTableName, m_pszTableName);
        eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }

    if( eErr == OGRERR_NONE && m_poDS->HasExtensionsTable() )
    {
        pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_extensions SET table_name = '%q' WHERE "
            "lower(table_name )= lower('%q')",
            pszDstTableName, m_pszTableName);
        eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }

    if( eErr == OGRERR_NONE && m_poDS->HasMetadataTables() )
    {
        pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_metadata_reference SET table_name = '%q' WHERE "
            "lower(table_name )= lower('%q')",
            pszDstTableName, m_pszTableName);
        eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }

    if( eErr == OGRERR_NONE && m_poDS->HasDataColumnsTable() )
    {
        pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_data_columns SET table_name = '%q' WHERE "
            "lower(table_name )= lower('%q')",
            pszDstTableName, m_pszTableName);
        eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if( eErr == OGRERR_NONE && m_poDS->m_bHasGPKGOGRContents )
    {
        pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_ogr_contents SET table_name = '%q' WHERE "
            "lower(table_name )= lower('%q')",
            pszDstTableName, m_pszTableName);
        eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }
#endif

    if( eErr == OGRERR_NONE )
    {
        pszSQL = sqlite3_mprintf(
                "ALTER TABLE \"%w\" RENAME TO \"%w\"",
                m_pszTableName, pszDstTableName );
        eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }

    // Check foreign key integrity
    if ( eErr == OGRERR_NONE )
    {
        eErr = m_poDS->PragmaCheck("foreign_key_check", "", 0);
    }

    if( eErr == OGRERR_NONE)
    {
        if( bHasSpatialIndex )
        {
            CreateSpatialIndex(pszDstTableName);
        }

#ifdef ENABLE_GPKG_OGR_CONTENTS
        CreateTriggers(pszDstTableName);
#endif

        eErr = m_poDS->SoftCommitTransaction();
        if( eErr == OGRERR_NONE)
        {
            CPLFree(m_pszTableName);
            m_pszTableName = CPLStrdup(pszDstTableName);
        }
    }
    else
    {
        m_poDS->SoftRollbackTransaction();
    }
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRGeoPackageTableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( InstallFilter( poGeomIn ) )
    {
        BuildWhere();

        ResetReading();
    }
}

/************************************************************************/
/*                        HasFastSpatialFilter()                        */
/************************************************************************/

int OGRGeoPackageTableLayer::HasFastSpatialFilter( int iGeomColIn )
{
    if( iGeomColIn < 0 || iGeomColIn >= m_poFeatureDefn->GetGeomFieldCount() )
        return FALSE;
    return HasSpatialIndex();
}

/************************************************************************/
/*                           GetSpatialWhere()                          */
/************************************************************************/

CPLString OGRGeoPackageTableLayer::GetSpatialWhere(int iGeomColIn,
                                               OGRGeometry* poFilterGeom)
{
    CPLString osSpatialWHERE;

    if( iGeomColIn < 0 || iGeomColIn >= m_poFeatureDefn->GetGeomFieldCount() )
        return osSpatialWHERE;

    if( poFilterGeom != NULL )
    {
        OGREnvelope  sEnvelope;

        poFilterGeom->getEnvelope( &sEnvelope );

        if( CPLIsInf(sEnvelope.MinX) && sEnvelope.MinX < 0 &&
            CPLIsInf(sEnvelope.MinY) && sEnvelope.MinY < 0 &&
            CPLIsInf(sEnvelope.MaxX) && sEnvelope.MaxX > 0 &&
            CPLIsInf(sEnvelope.MaxY) && sEnvelope.MaxY > 0 )
        {
            return CPLString();
        }

        bool bUseSpatialIndex = true;
        if( m_poExtent &&
            sEnvelope.MinX <= m_poExtent->MinX &&
            sEnvelope.MinY <= m_poExtent->MinY &&
            sEnvelope.MaxX >= m_poExtent->MaxX &&
            sEnvelope.MaxY >= m_poExtent->MaxY )
        {
            // Selecting from spatial filter on whole extent can be rather
            // slow. So use function based filtering, just in case the
            // advertized global extent might be wrong. Otherwise we might
            // just discard completely the spatial filter.
            bUseSpatialIndex = false;
        }

        if( bUseSpatialIndex && HasSpatialIndex() )
        {
            osSpatialWHERE.Printf(
                "\"%s\" IN ( SELECT id FROM \"%s\" WHERE "
                "maxx >= %.12f AND minx <= %.12f AND "
                "maxy >= %.12f AND miny <= %.12f)",
                SQLEscapeName(m_osFIDForRTree).c_str(),
                SQLEscapeName(m_osRTreeName).c_str(),
                sEnvelope.MinX - 1e-11, sEnvelope.MaxX + 1e-11,
                sEnvelope.MinY - 1e-11, sEnvelope.MaxY + 1e-11);
        }
        else
        {
            const char* pszC =
                m_poFeatureDefn->GetGeomFieldDefn(iGeomColIn)->GetNameRef();
            /* A bit inefficient but still faster than OGR filtering */
            osSpatialWHERE.Printf(
                "(ST_MaxX(\"%s\") >= %.12f AND ST_MinX(\"%s\") <= %.12f AND "
                "ST_MaxY(\"%s\") >= %.12f AND ST_MinY(\"%s\") <= %.12f)",
                SQLEscapeName(pszC).c_str(), sEnvelope.MinX - 1e-11,
                SQLEscapeName(pszC).c_str(), sEnvelope.MaxX + 1e-11,
                SQLEscapeName(pszC).c_str(), sEnvelope.MinY - 1e-11,
                SQLEscapeName(pszC).c_str(), sEnvelope.MaxY + 1e-11);
        }
    }

    return osSpatialWHERE;
}
/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRGeoPackageTableLayer::BuildWhere()

{
    m_soFilter = "";

    CPLString osSpatialWHERE = GetSpatialWhere(m_iGeomFieldFilter,
                                               m_poFilterGeom);
    if (!osSpatialWHERE.empty())
    {
        m_soFilter += osSpatialWHERE;
    }

    if( !osQuery.empty() )
    {
        if( m_soFilter.empty() )
        {
            m_soFilter += osQuery;
        }
        else
        {
            m_soFilter += " AND (";
            m_soFilter += osQuery;
            m_soFilter += ")";
        }
    }
    CPLDebug("GPKG", "Filter: %s", m_soFilter.c_str());
}

/************************************************************************/
/*                        SetCreationParameters()                       */
/************************************************************************/

void OGRGeoPackageTableLayer::SetCreationParameters( OGRwkbGeometryType eGType,
                                                     const char* pszGeomColumnName,
                                                     int bGeomNullable,
                                                     OGRSpatialReference* poSRS,
                                                     const char* pszFIDColumnName,
                                                     const char* pszIdentifier,
                                                     const char* pszDescription )
{
    m_bDeferredCreation = true;
    m_bHasTriedDetectingFID64 = true;
    m_pszFidColumn = CPLStrdup(pszFIDColumnName);

    if( eGType != wkbNone )
    {
        OGRGeomFieldDefn oGeomFieldDefn(pszGeomColumnName, eGType);
        if( poSRS )
            m_iSrs = m_poDS->GetSrsId(*poSRS);
        oGeomFieldDefn.SetSpatialRef(poSRS);
        oGeomFieldDefn.SetNullable(bGeomNullable);
        m_poFeatureDefn->AddGeomFieldDefn(&oGeomFieldDefn);
    }
    if( pszIdentifier )
    {
        m_osIdentifierLCO = pszIdentifier;
        OGRLayer::SetMetadataItem("IDENTIFIER", pszIdentifier);
    }
    if( pszDescription )
    {
        m_osDescriptionLCO = pszDescription;
        OGRLayer::SetMetadataItem("DESCRIPTION", pszDescription);
    }
}

/************************************************************************/
/*                      RegisterGeometryColumn()                        */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::RegisterGeometryColumn()
{
    OGRwkbGeometryType eGType = GetGeomType();
    const char *pszGeometryType = m_poDS->GetGeometryTypeString(eGType);
    /* Requirement 27: The z value in a gpkg_geometry_columns table row */
    /* SHALL be one of 0 (none), 1 (mandatory), or 2 (optional) */
    bool bGeometryTypeHasZ = CPL_TO_BOOL(wkbHasZ(eGType));
    bool bGeometryTypeHasM = CPL_TO_BOOL(wkbHasM(eGType));

    /* Update gpkg_geometry_columns with the table info */
    char* pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_geometry_columns "
        "(table_name,column_name,geometry_type_name,srs_id,z,m)"
        " VALUES "
        "('%q','%q','%q',%d,%d,%d)",
        GetName(),GetGeometryColumn(),pszGeometryType,
        m_iSrs,static_cast<int>(bGeometryTypeHasZ),
        static_cast<int>(bGeometryTypeHasM));

    OGRErr err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if ( err != OGRERR_NONE )
        return OGRERR_FAILURE;

    if( wkbFlatten(eGType) > wkbGeometryCollection )
    {
        CreateGeometryExtensionIfNecessary(eGType);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        GetColumnsOfCreateTable()                     */
/************************************************************************/

CPLString OGRGeoPackageTableLayer::GetColumnsOfCreateTable(const std::vector<OGRFieldDefn*>& apoFields)
{
    CPLString osSQL;

    char *pszSQL = NULL;
    bool bNeedComma = false;
    if( m_pszFidColumn != NULL )
    {
        pszSQL = sqlite3_mprintf("\"%w\" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL",
                                m_pszFidColumn);
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
        bNeedComma = true;
    }

    const OGRwkbGeometryType eGType = GetGeomType();
    if( eGType != wkbNone )
    {
        if( bNeedComma )
        {
            osSQL += ", ";
        }
        bNeedComma = true;

        /* Requirement 25: The geometry_type_name value in a gpkg_geometry_columns */
        /* row SHALL be one of the uppercase geometry type names specified in */
        /* Geometry Types (Normative). */
        const char *pszGeometryType = m_poDS->GetGeometryTypeString(eGType);

        pszSQL = sqlite3_mprintf("\"%w\" %s",
                                 GetGeometryColumn(), pszGeometryType);
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
        if( !m_poFeatureDefn->GetGeomFieldDefn(0)->IsNullable() )
        {
            osSQL += " NOT NULL";
        }
    }

    for(size_t i = 0; i < apoFields.size(); i++ )
    {
        if( bNeedComma )
        {
            osSQL += ", ";
        }
        bNeedComma = true;

        OGRFieldDefn* poFieldDefn = apoFields[i];
        pszSQL = sqlite3_mprintf("\"%w\" %s",
                                 poFieldDefn->GetNameRef(),
                                 GPkgFieldFromOGR(poFieldDefn->GetType(),
                                                  poFieldDefn->GetSubType(),
                                                  poFieldDefn->GetWidth()));
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
        if( !poFieldDefn->IsNullable() )
        {
            osSQL += " NOT NULL";
        }
        const char* pszDefault = poFieldDefn->GetDefault();
        if( pszDefault != NULL &&
            (!poFieldDefn->IsDefaultDriverSpecific() ||
             (pszDefault[0] == '(' && pszDefault[strlen(pszDefault)-1] == ')' &&
             (STARTS_WITH_CI(pszDefault+1, "strftime") ||
              STARTS_WITH_CI(pszDefault+1, " strftime")))) )
        {
            osSQL += " DEFAULT ";
            OGRField sField;
            if( poFieldDefn->GetType() == OFTDateTime &&
                OGRParseDate(pszDefault, &sField, 0) )
            {
                char* pszXML = OGRGetXMLDateTime(&sField);
                osSQL += pszXML;
                CPLFree(pszXML);
            }
            /* Make sure CURRENT_TIMESTAMP is translated into appropriate format for GeoPackage */
            else if( poFieldDefn->GetType() == OFTDateTime &&
                     EQUAL(pszDefault, "CURRENT_TIMESTAMP") )
            {
                osSQL += "(strftime('%Y-%m-%dT%H:%M:%fZ','now'))";
            }
            else
            {
                osSQL += poFieldDefn->GetDefault();
            }
        }
    }

    return osSQL;
}

/************************************************************************/
/*                      RunDeferredCreationIfNecessary()                */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::RunDeferredCreationIfNecessary()
{
    if( !m_bDeferredCreation )
        return OGRERR_NONE;
    m_bDeferredCreation = false;

    const char* pszLayerName = m_poFeatureDefn->GetName();

    /* Create the table! */
    CPLString osCommand;

    char* pszSQL = sqlite3_mprintf("CREATE TABLE \"%w\" ( ", pszLayerName);
    osCommand += pszSQL;
    sqlite3_free(pszSQL);

    std::vector<OGRFieldDefn*> apoFields;
    for(int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++ )
    {
        if( i == m_iFIDAsRegularColumnIndex )
            continue;
        apoFields.push_back( m_poFeatureDefn->GetFieldDefn(i) );
    }

    osCommand += GetColumnsOfCreateTable(apoFields);

    osCommand += ")";

#ifdef DEBUG
    CPLDebug( "GPKG", "exec(%s)", osCommand.c_str() );
#endif
    OGRErr err = SQLCommand(m_poDS->GetDB(), osCommand.c_str());
    if ( OGRERR_NONE != err )
        return OGRERR_FAILURE;

    /* Update gpkg_contents with the table info */
    const OGRwkbGeometryType eGType = GetGeomType();
    const bool bIsSpatial = (eGType != wkbNone);
    if ( bIsSpatial )
        err = RegisterGeometryColumn();
    else if( m_eASPatialVariant == OGR_ASPATIAL )
        err = m_poDS->CreateGDALAspatialExtension();

    if ( err != OGRERR_NONE )
        return OGRERR_FAILURE;

    if( bIsSpatial ||
        m_eASPatialVariant == OGR_ASPATIAL ||
        m_eASPatialVariant == GPKG_ATTRIBUTES )
    {
        const char* pszIdentifier = GetMetadataItem("IDENTIFIER");
        if( pszIdentifier == NULL )
            pszIdentifier = pszLayerName;
        const char* pszDescription = GetMetadataItem("DESCRIPTION");
        if( pszDescription == NULL )
            pszDescription = "";
        const char* pszCurrentDate = CPLGetConfigOption("OGR_CURRENT_DATE", NULL);
        CPLString osInsertGpkgContentsFormatting("INSERT INTO gpkg_contents "
                 "(table_name,data_type,identifier,description,last_change,srs_id) VALUES "
                "('%q','%q','%q','%q',");
        osInsertGpkgContentsFormatting += ( pszCurrentDate ) ? "'%q'" : "%s";
        osInsertGpkgContentsFormatting += ",%d)";

        pszSQL = sqlite3_mprintf(
            osInsertGpkgContentsFormatting.c_str(),
            pszLayerName, (bIsSpatial ? "features":
                          (m_eASPatialVariant == GPKG_ATTRIBUTES) ? "attributes" :
                          "aspatial"),
            pszIdentifier, pszDescription,
            pszCurrentDate ? pszCurrentDate : "strftime('%Y-%m-%dT%H:%M:%fZ','now')",
            m_iSrs);

        err = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        if ( err != OGRERR_NONE )
            return OGRERR_FAILURE;

#ifdef ENABLE_GPKG_OGR_CONTENTS
        if( m_poDS->m_bHasGPKGOGRContents )
        {
            pszSQL = sqlite3_mprintf(
                "DELETE FROM gpkg_ogr_contents WHERE lower(table_name) = lower('%q')",
                pszLayerName);
            SQLCommand(m_poDS->GetDB(), pszSQL);
            sqlite3_free(pszSQL);

            pszSQL = sqlite3_mprintf(
                "INSERT INTO gpkg_ogr_contents (table_name, feature_count) "
                "VALUES ('%q', NULL)",
                pszLayerName);
            err = SQLCommand(m_poDS->GetDB(), pszSQL);
            sqlite3_free(pszSQL);
            if ( err == OGRERR_NONE )
            {
                m_nTotalFeatureCount = 0;
                m_bAddOGRFeatureCountTriggers = true;
            }
        }
#endif
    }

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **OGRGeoPackageTableLayer::GetMetadata( const char *pszDomain )

{
    if( !m_bHasTriedDetectingFID64 && m_pszFidColumn != NULL )
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
                                        m_pszFidColumn,
                                        m_pszTableName);

            nMaxId = SQLGetInteger64( m_poDS->GetDB(), pszSQL, NULL);
            sqlite3_free(pszSQL);
        }
        if( nMaxId > INT_MAX )
            OGRLayer::SetMetadataItem(OLMD_FID64, "YES");
    }

    if( m_bHasReadMetadataFromStorage )
        return OGRLayer::GetMetadata( pszDomain );

    m_bHasReadMetadataFromStorage = true;

    if( !m_poDS->HasMetadataTables() )
        return OGRLayer::GetMetadata( pszDomain );

    char* pszSQL = sqlite3_mprintf(
        "SELECT md.metadata, md.md_standard_uri, md.mime_type, "
        "mdr.reference_scope FROM gpkg_metadata md "
        "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
        "WHERE md.metadata IS NOT NULL AND "
        "md.md_standard_uri IS NOT NULL AND "
        "md.mime_type IS NOT NULL AND "
        "lower(mdr.table_name) = lower('%q') ORDER BY md.id "
        "LIMIT 1000", // to avoid denial of service
        m_pszTableName);

    SQLResult oResult;
    OGRErr err = SQLQuery(m_poDS->GetDB(), pszSQL, &oResult);
    sqlite3_free(pszSQL);
    if  ( err != OGRERR_NONE )
    {
        SQLResultFree(&oResult);
        return OGRLayer::GetMetadata( pszDomain );
    }

    char** papszMetadata = CSLDuplicate(OGRLayer::GetMetadata());

    /* GDAL metadata */
    for(int i=0;i<oResult.nRowCount;i++)
    {
        const char *pszMetadata = SQLResultGetValue(&oResult, 0, i);
        const char* pszMDStandardURI = SQLResultGetValue(&oResult, 1, i);
        const char* pszMimeType = SQLResultGetValue(&oResult, 2, i);
        //const char* pszReferenceScope = SQLResultGetValue(&oResult, 3, i);
        //int bIsGPKGScope = EQUAL(pszReferenceScope, "geopackage");
        if( EQUAL(pszMDStandardURI, "http://gdal.org") &&
            EQUAL(pszMimeType, "text/xml") )
        {
            CPLXMLNode* psXMLNode = CPLParseXMLString(pszMetadata);
            if( psXMLNode )
            {
                GDALMultiDomainMetadata oLocalMDMD;
                oLocalMDMD.XMLInit(psXMLNode, FALSE);

                papszMetadata = CSLMerge(papszMetadata, oLocalMDMD.GetMetadata());
                char** papszDomainList = oLocalMDMD.GetDomainList();
                char** papszIter = papszDomainList;
                while( papszIter && *papszIter )
                {
                    if( !EQUAL(*papszIter, "") )
                        oMDMD.SetMetadata(oLocalMDMD.GetMetadata(*papszIter), *papszIter);
                    papszIter ++;
                }

                CPLDestroyXMLNode(psXMLNode);
            }
        }
    }

    OGRLayer::SetMetadata(papszMetadata);
    CSLDestroy(papszMetadata);
    papszMetadata = NULL;

    /* Add non-GDAL metadata now */
    int nNonGDALMDILocal = 1;
    for(int i=0;i<oResult.nRowCount;i++)
    {
        const char *pszMetadata = SQLResultGetValue(&oResult, 0, i);
        const char* pszMDStandardURI = SQLResultGetValue(&oResult, 1, i);
        const char* pszMimeType = SQLResultGetValue(&oResult, 2, i);
        //const char* pszReferenceScope = SQLResultGetValue(&oResult, 3, i);
        //int bIsGPKGScope = EQUAL(pszReferenceScope, "geopackage");
        if( EQUAL(pszMDStandardURI, "http://gdal.org") &&
            EQUAL(pszMimeType, "text/xml") )
            continue;

        /*if( strcmp( pszMDStandardURI, "http://www.isotc211.org/2005/gmd" ) == 0 &&
            strcmp( pszMimeType, "text/xml" ) == 0 )
        {
            char* apszMD[2];
            apszMD[0] = (char*)pszMetadata;
            apszMD[1] = NULL;
            oMDMD.SetMetadata(apszMD, "xml:MD_Metadata");
        }
        else*/
        {
            oMDMD.SetMetadataItem( CPLSPrintf("GPKG_METADATA_ITEM_%d", nNonGDALMDILocal),
                                    pszMetadata );
            nNonGDALMDILocal ++;
        }
    }

    SQLResultFree(&oResult);

    return OGRLayer::GetMetadata(pszDomain);
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *OGRGeoPackageTableLayer::GetMetadataItem( const char * pszName,
                                                    const char * pszDomain )
{
    return CSLFetchNameValue( GetMetadata(pszDomain), pszName );
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **OGRGeoPackageTableLayer::GetMetadataDomainList()
{
    GetMetadata();
    return OGRLayer::GetMetadataDomainList();
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr OGRGeoPackageTableLayer::SetMetadata( char ** papszMetadata, const char * pszDomain )
{
    GetMetadata(); /* force loading from storage if needed */
    CPLErr eErr = OGRLayer::SetMetadata(papszMetadata, pszDomain);
    m_poDS->SetMetadataDirty();
    if( pszDomain == NULL || EQUAL(pszDomain, "") )
    {
        if( !m_osIdentifierLCO.empty() )
            OGRLayer::SetMetadataItem("IDENTIFIER", m_osIdentifierLCO);
        if( !m_osDescriptionLCO.empty() )
            OGRLayer::SetMetadataItem("DESCRIPTION", m_osDescriptionLCO);
    }
    return eErr;
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr OGRGeoPackageTableLayer::SetMetadataItem( const char * pszName,
                                                 const char * pszValue,
                                                 const char * pszDomain )
{
    GetMetadata(); /* force loading from storage if needed */
    if( !m_osIdentifierLCO.empty() && EQUAL(pszName, "IDENTIFIER") &&
        (pszDomain == NULL || EQUAL(pszDomain, "")) )
        return CE_None;
    if( !m_osDescriptionLCO.empty() && EQUAL(pszName, "DESCRIPTION") &&
        (pszDomain == NULL || EQUAL(pszDomain, "")) )
        return CE_None;
    m_poDS->SetMetadataDirty();
    return OGRLayer::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                          RecreateTable()                             */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::RecreateTable(const CPLString& osColumnsForCreate,
                                              const CPLString& osFieldListForSelect)
{
/* -------------------------------------------------------------------- */
/*      Save existing related triggers and index                        */
/* -------------------------------------------------------------------- */
    sqlite3 *hDB = m_poDS->GetDB();

    char* pszSQL = sqlite3_mprintf(
        "SELECT sql FROM sqlite_master WHERE type IN ('trigger','index') "
        "AND lower(tbl_name)=lower('%q') LIMIT 10000",
        m_pszTableName );
    SQLResult oTriggers;
    OGRErr eErr = SQLQuery(hDB, pszSQL, &oTriggers);
    sqlite3_free(pszSQL);

/* -------------------------------------------------------------------- */
/*      Make a temporary table with new content.                        */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE )
    {
        pszSQL = sqlite3_mprintf("CREATE TABLE \"%w_ogr_tmp\" (%s)",
                                m_pszTableName, osColumnsForCreate.c_str());
        eErr = SQLCommand( hDB, pszSQL );
        sqlite3_free(pszSQL);
    }

    if( eErr == OGRERR_NONE )
    {
        pszSQL = sqlite3_mprintf(
                "INSERT INTO \"%w_ogr_tmp\" SELECT %s FROM \"%w\"",
                m_pszTableName,
                osFieldListForSelect.c_str(),
                m_pszTableName);
        eErr = SQLCommand( hDB, pszSQL );
        sqlite3_free(pszSQL);
    }

/* -------------------------------------------------------------------- */
/*      Drop the original table                                         */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE )
    {
        pszSQL = sqlite3_mprintf( "DROP TABLE \"%w\"", m_pszTableName );
        eErr = SQLCommand( hDB, pszSQL );
        sqlite3_free(pszSQL);
    }

/* -------------------------------------------------------------------- */
/*      Rename temporary table as new table                             */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE )
    {
        pszSQL = sqlite3_mprintf( "ALTER TABLE \"%w_ogr_tmp\" RENAME TO \"%w\"",
                                  m_pszTableName, m_pszTableName );
        eErr = SQLCommand( hDB, pszSQL );
        sqlite3_free(pszSQL);
    }

/* -------------------------------------------------------------------- */
/*      Recreate existing related tables, triggers and index            */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < oTriggers.nRowCount && eErr == OGRERR_NONE; i++)
    {
        const char* pszSQLTriggerIdx = SQLResultGetValue( &oTriggers, 0, i );
        if (pszSQLTriggerIdx != NULL && *pszSQLTriggerIdx != '\0')
        {
            eErr = SQLCommand( hDB, pszSQLTriggerIdx );
        }
    }

    SQLResultFree( &oTriggers );

    return eErr;
}

/************************************************************************/
/*                          BuildSelectFieldList()                      */
/************************************************************************/

CPLString OGRGeoPackageTableLayer::BuildSelectFieldList(const std::vector<OGRFieldDefn*>& apoFields)
{
    CPLString osFieldListForSelect;

    char *pszSQL = NULL;
    bool bNeedComma = false;

    if( m_pszFidColumn != NULL )
    {
        pszSQL = sqlite3_mprintf("\"%w\"", m_pszFidColumn);
        osFieldListForSelect += pszSQL;
        sqlite3_free(pszSQL);
        bNeedComma = true;
    }

    if( GetGeomType() != wkbNone )
    {
        if( bNeedComma )
        {
            osFieldListForSelect += ", ";
        }
        bNeedComma = true;

        pszSQL = sqlite3_mprintf("\"%w\"", GetGeometryColumn());
        osFieldListForSelect += pszSQL;
        sqlite3_free(pszSQL);
    }

    for( size_t iField = 0; iField < apoFields.size(); iField++ )
    {
        if( bNeedComma )
        {
            osFieldListForSelect += ", ";
        }
        bNeedComma = true;

        OGRFieldDefn *poFieldDefn = apoFields[iField];
        pszSQL = sqlite3_mprintf("\"%w\"", poFieldDefn->GetNameRef());
        osFieldListForSelect += pszSQL;
        sqlite3_free(pszSQL);
    }

    return osFieldListForSelect;
}

/************************************************************************/
/*                             DeleteField()                            */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::DeleteField( int iFieldToDelete )
{
    if( !CheckUpdatableTable("DeleteField") )
        return OGRERR_FAILURE;

    if (iFieldToDelete < 0 || iFieldToDelete >= m_poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    ResetReading();
    RunDeferredCreationIfNecessary();
    CreateSpatialIndexIfNecessary();

/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
    std::vector<OGRFieldDefn*> apoFields;
    for( int iField = 0; iField < m_poFeatureDefn->GetFieldCount(); iField++ )
    {
        if (iField == iFieldToDelete)
            continue;

        OGRFieldDefn *poFieldDefn = m_poFeatureDefn->GetFieldDefn(iField);
        apoFields.push_back(poFieldDefn);
    }

    CPLString osFieldListForSelect( BuildSelectFieldList(apoFields) );
    CPLString osColumnsForCreate( GetColumnsOfCreateTable(apoFields) );

/* -------------------------------------------------------------------- */
/*      Drop any iterator since we change the DB structure              */
/* -------------------------------------------------------------------- */
    m_poDS->ResetReadingAllLayers();

/* -------------------------------------------------------------------- */
/*      Recreate table in a transaction                                 */
/* -------------------------------------------------------------------- */
    if( m_poDS->SoftStartTransaction() != OGRERR_NONE )
        return OGRERR_FAILURE;

    OGRErr eErr = RecreateTable(osColumnsForCreate, osFieldListForSelect);

/* -------------------------------------------------------------------- */
/*      Update gpkg_extensions if needed.                               */
/* -------------------------------------------------------------------- */
    if( m_poDS->HasExtensionsTable() )
    {
        char* pszSQL = sqlite3_mprintf(
            "DELETE FROM gpkg_extensions WHERE lower(table_name) = lower('%q') AND "
            "lower(column_name) = lower('%q')",
            m_pszTableName,
            m_poFeatureDefn->GetFieldDefn(iFieldToDelete)->GetNameRef() );
        eErr = SQLCommand( m_poDS->GetDB(), pszSQL );
        sqlite3_free(pszSQL);
    }

/* -------------------------------------------------------------------- */
/*      Update gpkg_data_columns if needed.                             */
/* -------------------------------------------------------------------- */
    if( m_poDS->HasDataColumnsTable() )
    {
        char* pszSQL = sqlite3_mprintf(
            "DELETE FROM gpkg_data_columns WHERE lower(table_name) = lower('%q') AND "
            "lower(column_name) = lower('%q')",
            m_pszTableName,
            m_poFeatureDefn->GetFieldDefn(iFieldToDelete)->GetNameRef() );
        eErr = SQLCommand( m_poDS->GetDB(), pszSQL );
        sqlite3_free(pszSQL);
    }

/* -------------------------------------------------------------------- */
/*      Check foreign key integrity.                                    */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE )
    {
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

OGRErr OGRGeoPackageTableLayer::AlterFieldDefn( int iFieldToAlter,
                                                OGRFieldDefn* poNewFieldDefn,
                                                int nFlagsIn )
{
    if( !CheckUpdatableTable("AlterFieldDefn") )
        return OGRERR_FAILURE;

    if (iFieldToAlter < 0 || iFieldToAlter >= m_poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Deferred actions, reset state.                                   */
/* -------------------------------------------------------------------- */
    ResetReading();
    RunDeferredCreationIfNecessary();
    CreateSpatialIndexIfNecessary();

/* -------------------------------------------------------------------- */
/*      Check that the new column name is not a duplicate.              */
/* -------------------------------------------------------------------- */

    const CPLString osOldColName(
            m_poFeatureDefn->GetFieldDefn(iFieldToAlter)->GetNameRef() );
    const CPLString osNewColName( (nFlagsIn & ALTER_NAME_FLAG) ?
                                  CPLString(poNewFieldDefn->GetNameRef()) :
                                  osOldColName );

    const bool bRenameCol =
        (nFlagsIn & ALTER_NAME_FLAG) &&
        strcmp(poNewFieldDefn->GetNameRef(), osOldColName) != 0;
    if( bRenameCol )
    {
        if( (m_pszFidColumn &&
             strcmp(poNewFieldDefn->GetNameRef(), m_pszFidColumn) == 0) ||
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
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
    OGRFieldDefn oTmpFieldDefn(m_poFeatureDefn->GetFieldDefn(iFieldToAlter));
    if( (nFlagsIn & ALTER_NAME_FLAG) )
        oTmpFieldDefn.SetName(poNewFieldDefn->GetNameRef());
    if( (nFlagsIn & ALTER_TYPE_FLAG) )
    {
        oTmpFieldDefn.SetSubType(OFSTNone);
        oTmpFieldDefn.SetType(poNewFieldDefn->GetType());
        oTmpFieldDefn.SetSubType(poNewFieldDefn->GetSubType());
    }
    if (nFlagsIn & ALTER_WIDTH_PRECISION_FLAG)
    {
        oTmpFieldDefn.SetWidth(poNewFieldDefn->GetWidth());
        oTmpFieldDefn.SetPrecision(poNewFieldDefn->GetPrecision());
    }
    if( (nFlagsIn & ALTER_NULLABLE_FLAG) )
    {
        oTmpFieldDefn.SetNullable(poNewFieldDefn->IsNullable());
    }
    if( (nFlagsIn & ALTER_DEFAULT_FLAG) )
    {
        oTmpFieldDefn.SetDefault(poNewFieldDefn->GetDefault());
    }
    std::vector<OGRFieldDefn*> apoFields;
    std::vector<OGRFieldDefn*> apoFieldsOld;
    for( int iField = 0; iField < m_poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFieldDefn;
        if (iField == iFieldToAlter)
        {
            poFieldDefn = &oTmpFieldDefn;
        }
        else
        {
            poFieldDefn = m_poFeatureDefn->GetFieldDefn(iField);
        }
        apoFields.push_back(poFieldDefn);
        apoFieldsOld.push_back(m_poFeatureDefn->GetFieldDefn(iField));
    }

    const CPLString osColumnsForCreate( GetColumnsOfCreateTable(apoFields) );

/* -------------------------------------------------------------------- */
/*      Drop any iterator since we change the DB structure              */
/* -------------------------------------------------------------------- */
    m_poDS->ResetReadingAllLayers();

    const bool bUseFastMethod = ( m_poDS->nSoftTransactionLevel == 0 );

    if( m_poDS->SoftStartTransaction() != OGRERR_NONE )
        return OGRERR_FAILURE;

    sqlite3 *hDB = m_poDS->GetDB();
    SQLResult oTriggers;
    SQLResultInit(&oTriggers);
    OGRErr eErr = OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Drop triggers and index that look like to be related to the     */
/*      column if renaming. We re-install some indexes afterwards.      */
/* -------------------------------------------------------------------- */
    if( bRenameCol )
    {
        char* pszSQL = sqlite3_mprintf(
            "SELECT name, type, sql FROM sqlite_master WHERE "
            "type IN ('trigger','index') "
            "AND lower(tbl_name)=lower('%q') AND sql LIKE '%%%q%%' LIMIT 10000",
            m_pszTableName,
            SQLEscapeName(osOldColName).c_str() );
        eErr = SQLQuery(hDB, pszSQL, &oTriggers);
        sqlite3_free(pszSQL);

        for( int i = 0; i < oTriggers.nRowCount && eErr == OGRERR_NONE ; i++)
        {
            pszSQL = sqlite3_mprintf("DROP %s \"%w\"",
                                     SQLResultGetValue(&oTriggers, 1, i),
                                     SQLResultGetValue(&oTriggers, 0, i));
            eErr = SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
    }

    if( !bUseFastMethod )
    {
/* -------------------------------------------------------------------- */
/*      If we are within a transaction, we cannot use the method       */
/*      that consists in altering the database in a raw way.            */
/* -------------------------------------------------------------------- */
        const CPLString osFieldListForSelect( BuildSelectFieldList(apoFieldsOld) );

        if( eErr == OGRERR_NONE )
        {
            eErr = RecreateTable(osColumnsForCreate, osFieldListForSelect);
        }
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Rewrite schema in a transaction by altering the database        */
/*      schema in a rather raw way, as described at bottom of           */
/*      https://www.sqlite.org/lang_altertable.html                     */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Collect schema version number.                                  */
/* -------------------------------------------------------------------- */
        int nSchemaVersion = SQLGetInteger(hDB,
                                        "PRAGMA schema_version",
                                        &eErr);

/* -------------------------------------------------------------------- */
/*      Turn on writable schema.                                        */
/* -------------------------------------------------------------------- */
        if( eErr == OGRERR_NONE )
        {
            eErr = m_poDS->PragmaCheck( "writable_schema=ON", "", 0 );
        }

/* -------------------------------------------------------------------- */
/*      Rewrite CREATE TABLE statement.                                 */
/* -------------------------------------------------------------------- */
        if( eErr == OGRERR_NONE )
        {
            char* psSQLCreateTable = sqlite3_mprintf("CREATE TABLE \"%w\" (%s)",
                                    m_pszTableName,
                                    osColumnsForCreate.c_str());
            char* pszSQL = sqlite3_mprintf(
                "UPDATE sqlite_master SET sql='%q' WHERE type='table' AND name='%q'",
                psSQLCreateTable, m_pszTableName);
            eErr = SQLCommand( hDB, pszSQL );
            sqlite3_free(psSQLCreateTable);
            sqlite3_free(pszSQL);
        }

/* -------------------------------------------------------------------- */
/*      Increment schema number.                                        */
/* -------------------------------------------------------------------- */
        if( eErr == OGRERR_NONE )
        {
            char* pszSQL = sqlite3_mprintf(
                "PRAGMA schema_version = %d", nSchemaVersion + 1);
            eErr = SQLCommand( hDB, pszSQL );
            sqlite3_free(pszSQL);
        }

/* -------------------------------------------------------------------- */
/*      Turn off writable schema.                                        */
/* -------------------------------------------------------------------- */
        if( eErr == OGRERR_NONE )
        {
            eErr = m_poDS->PragmaCheck( "writable_schema=OFF", "", 0 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Update gpkg_extensions if needed.                               */
/* -------------------------------------------------------------------- */
    if( bRenameCol && eErr == OGRERR_NONE && m_poDS->HasExtensionsTable() )
    {
        char* pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_extensions SET column_name = '%q' WHERE "
            "lower(table_name) = lower('%q') AND lower(column_name) = lower('%q')",
            poNewFieldDefn->GetNameRef(),
            m_pszTableName,
            osOldColName.c_str() );
        eErr = SQLCommand( hDB, pszSQL );
        sqlite3_free(pszSQL);
    }

/* -------------------------------------------------------------------- */
/*      Update gpkg_data_columns if needed.                             */
/* -------------------------------------------------------------------- */
    if( bRenameCol && eErr == OGRERR_NONE && m_poDS->HasDataColumnsTable() )
    {
        char* pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_data_columns SET column_name = '%q' WHERE "
            "lower(table_name) = lower('%q') AND lower(column_name) = lower('%q')",
            poNewFieldDefn->GetNameRef(),
            m_pszTableName,
            osOldColName.c_str() );
        eErr = SQLCommand( hDB, pszSQL );
        sqlite3_free(pszSQL);
    }

/* -------------------------------------------------------------------- */
/*      Run integrity check.                                            */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE )
    {
        eErr = m_poDS->PragmaCheck("integrity_check", "ok", 1);
    }

/* -------------------------------------------------------------------- */
/*      Finish                                                          */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE)
    {
        eErr = m_poDS->SoftCommitTransaction();

        // We need to force database reopening due to schema change
        if( eErr == OGRERR_NONE && bUseFastMethod && !m_poDS->ReOpenDB() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot reopen database");
            eErr = OGRERR_FAILURE;
        }
        hDB = m_poDS->GetDB();

/* -------------------------------------------------------------------- */
/*      Recreate indices.                                               */
/* -------------------------------------------------------------------- */
        for( int i = 0; i < oTriggers.nRowCount && eErr == OGRERR_NONE ; i++)
        {
            if( EQUAL(SQLResultGetValue(&oTriggers, 1, i), "index") )
            {
                CPLString osSQL( SQLResultGetValue(&oTriggers, 2, i) );
                // CREATE INDEX idx_name ON table_name(column_name)
                char** papszTokens = SQLTokenize( osSQL );
                if( CSLCount(papszTokens) == 8 &&
                    EQUAL(papszTokens[0], "CREATE") &&
                    EQUAL(papszTokens[1], "INDEX") &&
                    EQUAL(papszTokens[3], "ON") &&
                    EQUAL(papszTokens[5], "(") &&
                    EQUAL(papszTokens[7], ")") )
                {
                    osSQL = "CREATE INDEX ";
                    osSQL += papszTokens[2];
                    osSQL += " ON ";
                    osSQL += papszTokens[4];
                    osSQL += "(\"";
                    osSQL += SQLEscapeName(osNewColName);
                    osSQL += "\")";
                    eErr = SQLCommand(hDB, osSQL);
                }
                CSLDestroy(papszTokens);
            }
        }

        if( eErr == OGRERR_NONE )
        {
            OGRFieldDefn* poFieldDefn =
                m_poFeatureDefn->GetFieldDefn(iFieldToAlter);

            if (nFlagsIn & ALTER_TYPE_FLAG)
            {
                poFieldDefn->SetSubType(OFSTNone);
                poFieldDefn->SetType(poNewFieldDefn->GetType());
                poFieldDefn->SetSubType(poNewFieldDefn->GetSubType());
            }
            if (nFlagsIn & ALTER_NAME_FLAG)
            {
                poFieldDefn->SetName(poNewFieldDefn->GetNameRef());
            }
            if (nFlagsIn & ALTER_WIDTH_PRECISION_FLAG)
            {
                poFieldDefn->SetWidth(poNewFieldDefn->GetWidth());
                poFieldDefn->SetPrecision(poNewFieldDefn->GetPrecision());
            }
            if (nFlagsIn & ALTER_NULLABLE_FLAG)
                poFieldDefn->SetNullable(poNewFieldDefn->IsNullable());
            if (nFlagsIn & ALTER_DEFAULT_FLAG)
                poFieldDefn->SetDefault(poNewFieldDefn->GetDefault());

            ResetReading();
        }
    }
    else
    {
        m_poDS->SoftRollbackTransaction();
    }

    SQLResultFree(&oTriggers);

    return eErr;
}

/************************************************************************/
/*                           ReorderFields()                            */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::ReorderFields( int* panMap )
{
    if( !CheckUpdatableTable("ReorderFields") )
        return OGRERR_FAILURE;

    if (m_poFeatureDefn->GetFieldCount() == 0)
        return OGRERR_NONE;

    OGRErr eErr = OGRCheckPermutation(panMap, m_poFeatureDefn->GetFieldCount());
    if (eErr != OGRERR_NONE)
        return eErr;

/* -------------------------------------------------------------------- */
/*      Deferred actions, reset state.                                   */
/* -------------------------------------------------------------------- */
    ResetReading();
    RunDeferredCreationIfNecessary();
    CreateSpatialIndexIfNecessary();

/* -------------------------------------------------------------------- */
/*      Drop any iterator since we change the DB structure              */
/* -------------------------------------------------------------------- */
    m_poDS->ResetReadingAllLayers();

/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
    std::vector<OGRFieldDefn*> apoFields;
    for( int iField = 0; iField < m_poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn* poFieldDefn = m_poFeatureDefn->GetFieldDefn(panMap[iField]);
        apoFields.push_back(poFieldDefn);
    }

    const CPLString osFieldListForSelect( BuildSelectFieldList(apoFields) );
    const CPLString osColumnsForCreate( GetColumnsOfCreateTable(apoFields) );

/* -------------------------------------------------------------------- */
/*      Recreate table in a transaction                                 */
/* -------------------------------------------------------------------- */
    if( m_poDS->SoftStartTransaction() != OGRERR_NONE )
        return OGRERR_FAILURE;

    eErr = RecreateTable(osColumnsForCreate, osFieldListForSelect);

/* -------------------------------------------------------------------- */
/*      Finish                                                          */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE)
    {
        eErr = m_poDS->SoftCommitTransaction();

        if( eErr == OGRERR_NONE )
            eErr = m_poFeatureDefn->ReorderFieldDefns( panMap );

        ResetReading();
    }
    else
    {
        m_poDS->SoftRollbackTransaction();
    }

    return eErr;
}
