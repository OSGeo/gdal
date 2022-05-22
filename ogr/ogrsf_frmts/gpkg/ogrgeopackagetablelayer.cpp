/******************************************************************************
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements OGRGeoPackageTableLayer class
 * Author:   Paul Ramsey <pramsey@boundlessgeo.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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

#include <algorithm>
#include <cmath>

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
                "min_x = %.18g, min_y = %.18g, "
                "max_x = %.18g, max_y = %.18g "
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
    int iCurCol = 0;

    /* Always start with a primary key */
    CPLString soColumns;
    if( m_bIsTable || m_pszFidColumn != nullptr )
    {
        soColumns += "m.";
        soColumns += m_pszFidColumn ?
            "\"" + SQLEscapeName(m_pszFidColumn) + "\"" : "_rowid_";
        iFIDCol = iCurCol;
        iCurCol ++;
    }

    /* Add a geometry column if there is one (just one) */
    if ( m_poFeatureDefn->GetGeomFieldCount() )
    {
        if( !soColumns.empty() )
            soColumns += ", ";
        soColumns += "m.\"";
        soColumns += SQLEscapeName(m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef());
        soColumns += "\"";
        iGeomCol = iCurCol;
        iCurCol ++;
    }

    /* Add all the attribute columns */
    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !soColumns.empty() )
            soColumns += ", ";
        soColumns += "m.\"";
        soColumns += SQLEscapeName(m_poFeatureDefn->GetFieldDefn(i)->GetNameRef());
        soColumns += "\"";
        panFieldOrdinals[i] = iCurCol;
        iCurCol ++;
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
            size_t szWkb = 0;
            GByte* pabyWkb = GPkgGeometryFromOGR(poGeom, m_iSrs, &szWkb);
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
                    err = sqlite3_bind_blob(poStmt, nColCount++, pabyBlob, szBlob, nullptr);
                    MY_CPLAssert( err == SQLITE_OK );
                    break;
                }
                default:
                {
                    const char *pszVal = poFeature->GetFieldAsString(i);
                    int nValLengthBytes = (int)strlen(pszVal);
                    char szVal[32];
                    CPLString osTemp;
                    if( poFieldDefn->GetType() == OFTDate )
                    {
                        int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZFlag;
                        poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond, &nTZFlag);
                        snprintf(szVal, sizeof(szVal), "%04d-%02d-%02d", nYear, nMonth, nDay);
                        pszVal = szVal;
                        nValLengthBytes = (int)strlen(pszVal);
                    }
                    else if( poFieldDefn->GetType() == OFTDateTime )
                    {
                        OGRField sField(*(poFeature->GetRawFieldRef(i)));

                        if( !m_poDS->m_bDateTimeWithTZ &&
                                (sField.Date.TZFlag == 0 || sField.Date.TZFlag == 1) )
                        {
                            sField.Date.TZFlag = 100;
                        }
                        else if( !m_poDS->m_bDateTimeWithTZ && sField.Date.TZFlag != 100 )
                        {
                            struct tm brokendowntime;
                            brokendowntime.tm_year = sField.Date.Year - 1900;
                            brokendowntime.tm_mon = sField.Date.Month -1;
                            brokendowntime.tm_mday = sField.Date.Day;
                            brokendowntime.tm_hour = sField.Date.Hour;
                            brokendowntime.tm_min = sField.Date.Minute;
                            brokendowntime.tm_sec = 0;
                            GIntBig nDT = CPLYMDHMSToUnixTime(&brokendowntime);
                            const int TZOffset = std::abs(sField.Date.TZFlag - 100) * 15;
                            nDT -= TZOffset * 60;
                            CPLUnixTimeToYMDHMS(nDT, &brokendowntime);
                            sField.Date.Year = static_cast<GInt16>(brokendowntime.tm_year + 1900);
                            sField.Date.Month = static_cast<GByte>(brokendowntime.tm_mon + 1);
                            sField.Date.Day = static_cast<GByte>(brokendowntime.tm_mday);
                            sField.Date.Hour = static_cast<GByte>(brokendowntime.tm_hour);
                            sField.Date.Minute = static_cast<GByte>(brokendowntime.tm_min);
                            sField.Date.TZFlag = 100;
                        }

                        constexpr bool bAlwaysMillisecond = true;
                        char* pszXMLDateTime = OGRGetXMLDateTime(&sField, bAlwaysMillisecond);
                        osTemp = pszXMLDateTime;
                        pszVal = osTemp.c_str();
                        nValLengthBytes = static_cast<int>(osTemp.size());
                        CPLFree(pszXMLDateTime);
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

    if( pnColCount != nullptr )
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
    return FeatureBindParameters( poFeature, poStmt, nullptr, bAddFID, bBindUnsetFields );
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

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn* OGRGeoPackageTableLayer::GetLayerDefn()
{
    if( !m_bFeatureDefnCompleted )
    {
        m_bFeatureDefnCompleted = true;
        ReadTableDefinition();
    }
    return m_poFeatureDefn;
}

/************************************************************************/
/*                      GetFIDColumn()                                  */
/************************************************************************/

const char* OGRGeoPackageTableLayer::GetFIDColumn()
{
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    return OGRGeoPackageLayer::GetFIDColumn();
}

/************************************************************************/
/*                            GetGeomType()                             */
/************************************************************************/

OGRwkbGeometryType OGRGeoPackageTableLayer::GetGeomType()
{
    return m_poFeatureDefn->GetGeomType();
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGRGeoPackageTableLayer::GetGeometryColumn()

{
    if( m_poFeatureDefn->GetGeomFieldCount() > 0 )
        return m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    else
        return "";
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
OGRErr OGRGeoPackageTableLayer::ReadTableDefinition()
{
    bool bReadExtent = false;
    sqlite3* poDb = m_poDS->GetDB();
    OGREnvelope oExtent;
    CPLString osGeomColumnName;
    CPLString osGeomColsType;
    bool bHasZ = false;
    bool bHasM = false;

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if( m_poDS->m_bHasGPKGOGRContents )
    {
        CPLString osTrigger1Name(
            CPLSPrintf("trigger_insert_feature_count_%s", m_pszTableName));
        CPLString osTrigger2Name(
            CPLSPrintf("trigger_delete_feature_count_%s", m_pszTableName));
        const std::map<CPLString, CPLString>& oMap =
                                m_poDS->GetNameTypeMapFromSQliteMaster();
        if( oMap.find( osTrigger1Name.toupper() ) != oMap.end() &&
            oMap.find( osTrigger2Name.toupper() ) != oMap.end() )
        {
            m_bOGRFeatureCountTriggersEnabled = true;
        }
        else if( m_bIsTable )
        {
            CPLDebug("GPKG", "Insert/delete feature_count triggers "
                    "missing on %s", m_pszTableName);
        }
    }
#endif

    if( m_bIsInGpkgContents )
    {
        /* Check that the table name is registered in gpkg_contents */
        const std::map< CPLString, GPKGContentsDesc >& oMapContents =
                                                    m_poDS->GetContents();
        std::map< CPLString, GPKGContentsDesc >::const_iterator
            oIterContents = oMapContents.find(
                                CPLString(m_pszTableName).toupper() );
        if( oIterContents == oMapContents.end() )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                        "layer '%s' is not registered in gpkg_contents",
                        m_pszTableName );
            return OGRERR_FAILURE;
        }

        const GPKGContentsDesc& oContents = oIterContents->second;

        const char* pszIdentifier = oContents.osIdentifier.c_str();
        if( pszIdentifier[0] != 0 && strcmp(pszIdentifier, m_pszTableName) != 0 )
            OGRLayer::SetMetadataItem("IDENTIFIER", pszIdentifier);
        const char* pszDescription = oContents.osDescription.c_str();
        if( pszDescription[0] )
            OGRLayer::SetMetadataItem("DESCRIPTION", pszDescription);

#ifdef ENABLE_GPKG_OGR_CONTENTS
        if( m_poDS->m_bHasGPKGOGRContents )
        {
            char* pszSQL = sqlite3_mprintf(
                "SELECT feature_count "
                "FROM gpkg_ogr_contents "
                "WHERE table_name = '%q'"
#ifdef WORKAROUND_SQLITE3_BUGS
                " OR 0"
#endif
                " LIMIT 2"
                , m_pszTableName);
            auto oResultFeatureCount = SQLQuery(poDb, pszSQL);
            sqlite3_free(pszSQL);
            if( oResultFeatureCount && oResultFeatureCount->RowCount() == 0 )
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
                oResultFeatureCount = SQLQuery(poDb, pszSQL);
                sqlite3_free(pszSQL);
            }

            if( oResultFeatureCount && oResultFeatureCount->RowCount() == 1 )
            {
                const char* pszFeatureCount =
                                oResultFeatureCount->GetValue(0, 0);
                if( pszFeatureCount )
                {
                    m_nTotalFeatureCount = CPLAtoGIntBig(pszFeatureCount);
                }
            }
        }
#endif

        if( m_bIsSpatial )
        {
            /* All the extrema have to be non-NULL for this to make sense */
            if ( !oContents.osMinX.empty() &&
                 !oContents.osMinY.empty() &&
                 !oContents.osMaxX.empty() &&
                 !oContents.osMaxY.empty() )
            {
                oExtent.MinX = CPLAtof(oContents.osMinX);
                oExtent.MinY = CPLAtof(oContents.osMinY);
                oExtent.MaxX = CPLAtof(oContents.osMaxX);
                oExtent.MaxY = CPLAtof(oContents.osMaxY);
                bReadExtent = oExtent.MinX <= oExtent.MaxX &&
                              oExtent.MinY <= oExtent.MaxY;
            }

            /* Check that the table name is registered in gpkg_geometry_columns */
            char* pszSQL = sqlite3_mprintf(
                        "SELECT table_name, column_name, "
                        "geometry_type_name, srs_id, z, m "
                        "FROM gpkg_geometry_columns "
                        "WHERE table_name = '%q'"
#ifdef WORKAROUND_SQLITE3_BUGS
                        " OR 0"
#endif
                        " LIMIT 2"
                        ,m_pszTableName);

            auto oResultGeomCols = SQLQuery(poDb, pszSQL);
            sqlite3_free(pszSQL);
            if( oResultGeomCols && oResultGeomCols->RowCount() == 0 )
            {
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

                oResultGeomCols = SQLQuery(poDb, pszSQL);
                sqlite3_free(pszSQL);
            }

            /* gpkg_geometry_columns query has to work */
            /* gpkg_geometry_columns.table_name is supposed to be unique */
            if ( !oResultGeomCols || oResultGeomCols->RowCount() != 1 )
            {
                if ( oResultGeomCols )
                    CPLError( CE_Failure, CPLE_AppDefined, "layer '%s' is not registered in gpkg_geometry_columns", m_pszTableName );

                return OGRERR_FAILURE;
            }

            const char* pszGeomColName = oResultGeomCols->GetValue(1, 0);
            if( pszGeomColName != nullptr )
                osGeomColumnName = pszGeomColName;
            const char* pszGeomColsType = oResultGeomCols->GetValue(2, 0);
            if( pszGeomColsType != nullptr )
                osGeomColsType = pszGeomColsType;
            m_iSrs = oResultGeomCols->GetValueAsInteger(3, 0);
            m_nZFlag = oResultGeomCols->GetValueAsInteger(4, 0);
            m_nMFlag = oResultGeomCols->GetValueAsInteger(5, 0);
            if( !(EQUAL(osGeomColsType, "GEOMETRY") && m_nZFlag == 2) )
            {
                bHasZ = CPL_TO_BOOL(m_nZFlag);
                bHasM = CPL_TO_BOOL(m_nMFlag);
            }
        }
    }


    // set names (in upper case) of fields with unique constraint
    std::set<std::string> uniqueFieldsUC;
    if( m_bIsTable )
    {
        uniqueFieldsUC = SQLGetUniqueFieldUCConstraints(poDb, m_pszTableName);
    }

    /* Use the "PRAGMA TABLE_INFO()" call to get table definition */
    /*  #|name|type|notnull|default|pk */
    /*  0|id|integer|0||1 */
    /*  1|name|varchar|0||0 */
    char* pszSQL = sqlite3_mprintf("pragma table_info('%q')", m_pszTableName);
    auto oResultTable = SQLQuery(poDb, pszSQL);
    sqlite3_free(pszSQL);

    if ( !oResultTable || oResultTable->RowCount() == 0 )
    {
        if (oResultTable)
            CPLError( CE_Failure, CPLE_AppDefined, "Cannot find table %s", m_pszTableName );
        return OGRERR_FAILURE;
    }

    /* Populate feature definition from table description */

    // First pass to determine if we have a single PKID column
    int nCountPKIDColumns = 0;
    for ( int iRecord = 0; iRecord < oResultTable->RowCount(); iRecord++ )
    {
        int nPKIDIndex = oResultTable->GetValueAsInteger(5, iRecord);
        if( nPKIDIndex > 0 )
            nCountPKIDColumns ++;
    }
    if( nCountPKIDColumns > 1 )
    {
        CPLDebug("GPKG", "For table %s, multiple columns make "
                         "the primary key. Ignoring them",
                 m_pszTableName);
    }

    bool bHasPreexistingSingleGeomColumn = m_poFeatureDefn->GetGeomFieldCount() ==  1;
    for ( int iRecord = 0; iRecord < oResultTable->RowCount(); iRecord++ )
    {
        const char *pszName = oResultTable->GetValue(1, iRecord);
        const char *pszType = oResultTable->GetValue(2, iRecord);
        int bNotNull = oResultTable->GetValueAsInteger(3, iRecord);
        const char* pszDefault = oResultTable->GetValue(4, iRecord);
        int nPKIDIndex = oResultTable->GetValueAsInteger(5, iRecord);
        OGRFieldSubType eSubType = OFSTNone;
        int nMaxWidth = 0;
        OGRFieldType oType = (OGRFieldType)(OFTMaxType + 1);

        if ( !EQUAL(pszType, "") || m_bIsTable )
        {
            oType = GPkgFieldToOGR(pszType, eSubType, nMaxWidth);
        }

        /* Not a standard field type... */
        if ( !EQUAL(pszType, "") && !EQUAL(pszName, "OGC_FID") &&
            ((oType > OFTMaxType && !osGeomColsType.empty() ) ||
             EQUAL(osGeomColumnName, pszName)) )
        {
            /* Maybe it is a geometry type? */
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

                if ( bHasPreexistingSingleGeomColumn || m_poFeatureDefn->GetGeomFieldCount() == 0 )
                {
                    if( !bHasPreexistingSingleGeomColumn )
                    {
                        OGRGeomFieldDefn oGeomField(pszName, oGeomType);
                        m_poFeatureDefn->AddGeomFieldDefn(&oGeomField);
                    }
                    bHasPreexistingSingleGeomColumn = false;
                    if( bNotNull )
                        m_poFeatureDefn->GetGeomFieldDefn(0)->SetNullable(FALSE);

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
                    return OGRERR_FAILURE;
                }
            }
            else
            {
                // CPLError( CE_Failure, CPLE_AppDefined, "invalid field type '%s'", pszType );
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

                if ( uniqueFieldsUC.find( CPLString( pszName ).toupper() ) != uniqueFieldsUC.end() )
                {
                    oField.SetUnique(TRUE);
                }

                if( pszDefault != nullptr )
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
                        if( strchr(pszDefault, '.') == nullptr )
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
    if ( m_bIsTable && m_pszFidColumn == nullptr )
    {
        CPLDebug("GPKG",
                 "no integer primary key defined for table '%s'",
                 m_pszTableName);
    }

    if ( bReadExtent )
    {
        m_poExtent = new OGREnvelope(oExtent);
    }

    // Look for sub-types such as JSON
    if( m_poDS->HasDataColumnsTable() )
    {
        pszSQL = sqlite3_mprintf(
            "SELECT column_name, mime_type, constraint_name FROM gpkg_data_columns "
            "WHERE table_name = '%q'",
            m_pszTableName);
        oResultTable = SQLQuery(poDb, pszSQL);
        sqlite3_free(pszSQL);
        if( oResultTable )
        {
            for ( int iRecord = 0; iRecord < oResultTable->RowCount(); iRecord++ )
            {
                const char *pszColumn = oResultTable->GetValue(0, iRecord);
                if( pszColumn == nullptr )
                    continue;
                const char *pszMimeType = oResultTable->GetValue(1, iRecord);
                const char *pszConstraintName = oResultTable->GetValue(2, iRecord);
                if( pszMimeType && EQUAL(pszMimeType, "application/json") )
                {
                    int iIdx = m_poFeatureDefn->GetFieldIndex(pszColumn);
                    if( iIdx >= 0 &&
                        m_poFeatureDefn->GetFieldDefn(iIdx)->GetType() == OFTString)
                    {
                        m_poFeatureDefn->GetFieldDefn(iIdx)->SetSubType(OFSTJSON);
                    }
                }
                else if( pszConstraintName )
                {
                    int iIdx = m_poFeatureDefn->GetFieldIndex(pszColumn);
                    if( iIdx >= 0 )
                    {
                        m_poFeatureDefn->GetFieldDefn(iIdx)->SetDomainName(
                            pszConstraintName);
                    }
                }
            }
        }
    }

    /* Update the columns string */
    BuildColumns();

    CheckUnknownExtensions();

    InitView();

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
    m_bIsSpatial(false),
    m_bIsInGpkgContents(false),
    m_bFeatureDefnCompleted(false),
    m_iSrs(0),
    m_poExtent(nullptr),
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
    m_poUpdateStatement(nullptr),
    m_bInsertStatementWithFID(false),
    m_poInsertStatement(nullptr),
    m_bDeferredSpatialIndexCreation(false),
    m_bHasSpatialIndex(-1),
    m_bDropRTreeTable(false),
    m_bPreservePrecision(true),
    m_bTruncateFields(false),
    m_bDeferredCreation(false),
    m_iFIDAsRegularColumnIndex(-1),
    m_bHasReadMetadataFromStorage(false),
    m_bHasTriedDetectingFID64(false),
    m_eASpatialVariant(GPKG_ATTRIBUTES)
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
    OGRGeoPackageTableLayer::SyncToDisk();

    /* Clean up resources in memory */
    if ( m_pszTableName )
        CPLFree( m_pszTableName );

    if ( m_poExtent )
        delete m_poExtent;

    if ( m_poUpdateStatement )
        sqlite3_finalize(m_poUpdateStatement);

    if ( m_poInsertStatement )
        sqlite3_finalize(m_poInsertStatement);

    if ( m_poGetFeatureStatement )
        sqlite3_finalize(m_poGetFeatureStatement);
}

/************************************************************************/
/*                        InitView()                                    */
/************************************************************************/

void OGRGeoPackageTableLayer::InitView()
{
#ifdef SQLITE_HAS_COLUMN_METADATA
    if( !m_bIsTable )
    {
        /* Detect if the view columns have the FID and geom columns of a */
        /* table that has itself a spatial index */
        sqlite3_stmt* hStmt = nullptr;
        char* pszSQL = sqlite3_mprintf("SELECT * FROM \"%w\"", m_pszTableName);
        CPL_IGNORE_RET_VAL(sqlite3_prepare_v2(m_poDS->GetDB(),
                                              pszSQL, -1, &hStmt, nullptr));
        sqlite3_free(pszSQL);
        if( hStmt )
        {
            if( sqlite3_step(hStmt) == SQLITE_ROW )
            {
                OGRGeoPackageTableLayer* poLayerGeom = nullptr;
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
                        (pszOriginName == nullptr ||
                         osColName != pszOriginName) )
                    {
                        // in the case we have a OGC_FID column, and that
                        // is not the name of the original column, then
                        // interpret this as an explicit intent to be a
                        // PKID.
                        // We cannot just take the FID of a source table as
                        // a FID because of potential joins that would result
                        // in multiple records with same source FID.
                        CPLFree(m_pszFidColumn);
                        m_pszFidColumn = CPLStrdup(osColName);
                        m_poFeatureDefn->DeleteFieldDefn(
                            m_poFeatureDefn->GetFieldIndex(osColName));
                    }
                    else if( iCol == 0 &&
                             sqlite3_column_type( hStmt, iCol ) == SQLITE_INTEGER )
                    {
                        // Assume the first column of integer type is the FID column
                        // per the latest requirements of the GPKG spec
                        CPLFree(m_pszFidColumn);
                        m_pszFidColumn = CPLStrdup(osColName);
                        m_poFeatureDefn->DeleteFieldDefn(
                            m_poFeatureDefn->GetFieldIndex(osColName));
                    }
                    else if( pszTableName != nullptr && pszOriginName != nullptr )
                    {
                        OGRGeoPackageTableLayer* poLayer =
                            dynamic_cast<OGRGeoPackageTableLayer*>(
                            m_poDS->GetLayerByName(pszTableName));
                        if( poLayer != nullptr &&
                            osColName == GetGeometryColumn() &&
                            strcmp(pszOriginName,
                                   poLayer->GetGeometryColumn()) == 0 )
                        {
                            poLayerGeom = poLayer;
                        }
                    }
                }

                if( poLayerGeom != nullptr && poLayerGeom->HasSpatialIndex() )
                {
                    for( int iCol = 0; iCol < nRawColumns; iCol++ )
                    {
                        CPLString osColName(SQLUnescape(
                                            sqlite3_column_name( hStmt, iCol )));
                        const char* pszTableName =
                            sqlite3_column_table_name( hStmt, iCol );
                        const char* pszOriginName =
                            sqlite3_column_origin_name( hStmt, iCol );
                        if( pszTableName != nullptr && pszOriginName != nullptr )
                        {
                            OGRGeoPackageTableLayer* poLayer =
                                dynamic_cast<OGRGeoPackageTableLayer*>(
                                m_poDS->GetLayerByName(pszTableName));
                            if( poLayer != nullptr && poLayer == poLayerGeom &&
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
                                             int /* bApproxOK */ )
{
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    if( !CheckUpdatableTable("CreateField") )
        return OGRERR_FAILURE;

    OGRFieldDefn oFieldDefn(poField);
    int nMaxWidth = 0;
    if( m_bPreservePrecision && poField->GetType() == OFTString )
        nMaxWidth = poField->GetWidth();
    else
        oFieldDefn.SetWidth(0);
    oFieldDefn.SetPrecision(0);

    if( m_pszFidColumn != nullptr &&
        EQUAL( poField->GetNameRef(), m_pszFidColumn ) &&
        poField->GetType() != OFTInteger &&
        poField->GetType() != OFTInteger64 &&
        // typically a GeoPackage exported with QGIS as a shapefile and re-imported
        // See https://github.com/qgis/QGIS/pull/43118
        !(poField->GetType() == OFTReal && poField->GetWidth() == 20 &&
          poField->GetPrecision() == 0) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Wrong field type for %s",
                 poField->GetNameRef());
        return OGRERR_FAILURE;
    }

    if( !m_bDeferredCreation )
    {
        CPLString osCommand;

        // ADD COLUMN has several restrictions
        // See https://www.sqlite.org/lang_altertable.html#altertabaddcol

        osCommand.Printf("ALTER TABLE \"%s\" ADD COLUMN \"%s\" %s",
                          SQLEscapeName(m_pszTableName).c_str(),
                          SQLEscapeName(poField->GetNameRef()).c_str(),
                          GPkgFieldFromOGR(poField->GetType(),
                                           poField->GetSubType(),
                                           nMaxWidth));
        if(  !poField->IsNullable() )
            osCommand += " NOT NULL";
        if(  poField->IsUnique() )
        {
            // this will fail when SQLCommand() is run, as it is not allowed
            // by SQLite. This is a bit of an artificial restriction.
            // We could override it by rewriting the table.
            osCommand += " UNIQUE";
        }
        if( poField->GetDefault() != nullptr && !poField->IsDefaultDriverSpecific() )
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
                if( strchr(poField->GetDefault(), '.') == nullptr )
                    osCommand += CPLSPrintf("'%04d-%02d-%02dT%02d:%02d:%02dZ'",
                                        nYear, nMonth, nDay, nHour, nMinute, (int)(fSecond+0.5));
                else
                    osCommand += CPLSPrintf("'%04d-%02d-%02dT%02d:%02d:%06.3fZ'",
                                            nYear, nMonth, nDay, nHour, nMinute, fSecond);
            }
            else
            {
                // This could fail if it is CURRENT_TIMESTAMP, etc.
                osCommand += poField->GetDefault();
            }
        }
        else if( !poField->IsNullable() )
        {
            // SQLite mandates a DEFAULT value when adding a NOT NULL column in
            // an ALTER TABLE ADD COLUMN.
            osCommand += " DEFAULT ''";
        }

        OGRErr err = SQLCommand(m_poDS->GetDB(), osCommand.c_str());

        if ( err != OGRERR_NONE )
            return err;

        if( !DoSpecialProcessingForColumnCreation(poField) )
        {
            return OGRERR_FAILURE;
        }
    }

    m_poFeatureDefn->AddFieldDefn( &oFieldDefn );

    if( m_pszFidColumn != nullptr &&
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
/*                DoSpecialProcessingForColumnCreation()                */
/************************************************************************/

bool OGRGeoPackageTableLayer::DoSpecialProcessingForColumnCreation(
                                                    OGRFieldDefn* poField)
{

    if( poField->GetType() == OFTString && poField->GetSubType() == OFSTJSON )
    {
        if( !m_poDS->CreateColumnsTableAndColumnConstraintsTablesIfNecessary() )
            return false;

        /* Now let's register our column. */
        char* pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_data_columns (table_name, column_name, name, "
            "title, description, mime_type, constraint_name) VALUES ("
            "'%q', '%q', NULL, NULL, NULL, 'application/json', NULL)",
            m_pszTableName, poField->GetNameRef());
        bool ok = SQLCommand(m_poDS->GetDB(), pszSQL) == OGRERR_NONE;
        sqlite3_free(pszSQL);
        return ok;
    }

    else if( !poField->GetDomainName().empty() )
    {
        if( !m_poDS->CreateColumnsTableAndColumnConstraintsTablesIfNecessary() )
            return false;

        char* pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_data_columns (table_name, column_name, name, "
            "title, description, mime_type, constraint_name) VALUES ("
            "'%q', '%q', NULL, NULL, NULL, NULL, '%q')",
            m_pszTableName, poField->GetNameRef(),
            poField->GetDomainName().c_str());
        bool ok = SQLCommand(m_poDS->GetDB(), pszSQL) == OGRERR_NONE;
        sqlite3_free(pszSQL);
        return ok;
    }

    return true;
}


/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::CreateGeomField( OGRGeomFieldDefn *poGeomFieldIn,
                                                 int /* bApproxOK */ )
{
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
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
    if( oGeomField.GetSpatialRef() )
    {
        oGeomField.GetSpatialRef()->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    if( EQUAL(oGeomField.GetNameRef(), "") )
    {
        oGeomField.SetName( "geom" );
    }

    OGRSpatialReference* poSRS = oGeomField.GetSpatialRef();
    if( poSRS != nullptr )
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

void OGRGeoPackageTableLayer::DisableFeatureCount()
{
    m_nTotalFeatureCount = -1;
}

/************************************************************************/
/*                      CreateTriggers()                                */
/************************************************************************/

void OGRGeoPackageTableLayer::CreateTriggers(const char* pszTableName)
{
    if( m_bAddOGRFeatureCountTriggers )
    {
        if( pszTableName == nullptr )
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
        if( poGeom != nullptr )
        {
            OGRwkbGeometryType eGeomType =
                wkbFlatten(poGeom->getGeometryType());
            if( !OGR_GT_IsSubClassOf(eGeomType, eLayerGeomType) &&
                m_eSetBadGeomTypeWarned.find(eGeomType) ==
                                        m_eSetBadGeomTypeWarned.end() )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "A geometry of type %s is inserted into layer %s "
                         "of geometry type %s, which is not normally allowed "
                         "by the GeoPackage specification, but the driver will "
                         "however do it. "
                         "To create a conformant GeoPackage, if using ogr2ogr, "
                         "the -nlt option can be used to override the layer "
                         "geometry type. "
                         "This warning will no longer be emitted for this "
                         "combination of layer and feature geometry type.",
                         OGRToOGCGeomType(eGeomType),
                         GetName(),
                         OGRToOGCGeomType(eLayerGeomType));
                m_eSetBadGeomTypeWarned.insert(eGeomType);
            }
        }
    }

    // wkbUnknown is a rather loose type in OGR. Make sure to update
    // the z and m columns of gpkg_geometry_columns to 2 if we have geometries
    // with Z and M components
    if( GetGeomType() == wkbUnknown && (m_nZFlag == 0 || m_nMFlag == 0) )
    {
        OGRGeometry* poGeom = poFeature->GetGeometryRef();
        if( poGeom != nullptr )
        {
            bool bUpdateGpkgGeometryColumnsTable = false;
            OGRwkbGeometryType eGeomType = poGeom->getGeometryType();
            if( m_nZFlag == 0 && wkbHasZ(eGeomType) )
            {
                m_nZFlag = 2;
                bUpdateGpkgGeometryColumnsTable = true;
            }
            if( m_nMFlag == 0 && wkbHasM(eGeomType) )
            {
                m_nMFlag = 2;
                bUpdateGpkgGeometryColumnsTable = true;
            }
            if( bUpdateGpkgGeometryColumnsTable )
            {
                /* Update gpkg_geometry_columns */
                char* pszSQL = sqlite3_mprintf(
                    "UPDATE gpkg_geometry_columns SET z = %d, m = %d WHERE "
                    "table_name = '%q' AND column_name = '%q'",
                    m_nZFlag, m_nMFlag, GetName(),GetGeometryColumn());
                CPL_IGNORE_RET_VAL(SQLCommand(m_poDS->GetDB(), pszSQL));
                sqlite3_free(pszSQL);
            }
        }
    }
}

/************************************************************************/
/*                   CheckFIDAndFIDColumnConsistency()                  */
/************************************************************************/

static bool CheckFIDAndFIDColumnConsistency( const OGRFeature* poFeature,
                                             int iFIDAsRegularColumnIndex)
{
    bool ok = false;
    if( !poFeature->IsFieldSetAndNotNull( iFIDAsRegularColumnIndex ) )
    {
        // nothing to do
    }
    else if( poFeature->GetDefnRef()->GetFieldDefn(iFIDAsRegularColumnIndex)->GetType() == OFTReal )
    {
        const double dfFID = poFeature->GetFieldAsDouble(iFIDAsRegularColumnIndex);
        if( dfFID >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
            dfFID <= static_cast<double>(std::numeric_limits<int64_t>::max()) )
        {
            const auto nFID = static_cast<GIntBig>(dfFID);
            if( nFID == poFeature->GetFID() )
            {
                ok = true;
            }
        }
    }
    else if( poFeature->GetFieldAsInteger64(iFIDAsRegularColumnIndex) == poFeature->GetFID() )
    {
        ok = true;
    }
    if( !ok )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Inconsistent values of FID and field of same name");
    }
    return ok;
}

/************************************************************************/
/*                      ICreateFeature()                                 */
/************************************************************************/

// rtreeValueDown() / rtreeValueUp() come from SQLite3 source code
// SQLite3 RTree stores min/max values as float. So do the same for our
// GPKGRTreeEntry

/*
** Rounding constants for float->double conversion.
*/
#define RNDTOWARDS  (1.0 - 1.0/8388608.0)  /* Round towards zero */
#define RNDAWAY     (1.0 + 1.0/8388608.0)  /* Round away from zero */

/*
** Convert an sqlite3_value into an RtreeValue (presumably a float)
** while taking care to round toward negative or positive, respectively.
*/
static float rtreeValueDown(double d){
  float f = static_cast<float>(d);
  if( f>d ){
    f = static_cast<float>(d*(d<0 ? RNDAWAY : RNDTOWARDS));
  }
  return f;
}

static float rtreeValueUp(double d){
  float f = static_cast<float>(d);
  if( f<d ){
    f = static_cast<float>(d*(d<0 ? RNDTOWARDS : RNDAWAY));
  }
  return f;
}

OGRErr OGRGeoPackageTableLayer::ICreateFeature( OGRFeature *poFeature )
{
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
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
    poFeature->FillUnsetWithDefault(FALSE, nullptr);
    bool bHasDefaultValue = false;
    const int nFieldCount = m_poFeatureDefn->GetFieldCount();
    for( int iField = 0; iField < nFieldCount; iField++ )
    {
        if( poFeature->IsFieldSet( iField ) )
            continue;
        const char* pszDefault = poFeature->GetFieldDefnRef(iField)->GetDefault();
        if( pszDefault != nullptr )
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
                if( m_poFeatureDefn->GetFieldDefn(m_iFIDAsRegularColumnIndex)->GetType() == OFTReal )
                {
                    bool ok = false;
                    const double dfFID = poFeature->GetFieldAsDouble(m_iFIDAsRegularColumnIndex);
                    if( dfFID >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
                        dfFID <= static_cast<double>(std::numeric_limits<int64_t>::max()) )
                    {
                        const auto nFID = static_cast<GIntBig>(dfFID);
                        if( static_cast<double>(nFID) == dfFID )
                        {
                            poFeature->SetFID(nFID);
                            ok = true;
                        }
                    }
                    if( !ok )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                            "Value of FID %g cannot be parsed to an Integer64",
                            dfFID);
                        return OGRERR_FAILURE;
                    }
                }
                else
                {
                    poFeature->SetFID(
                        poFeature->GetFieldAsInteger64(m_iFIDAsRegularColumnIndex));
                }
            }
        }
        else if( !CheckFIDAndFIDColumnConsistency(poFeature, m_iFIDAsRegularColumnIndex) )
        {
            return OGRERR_FAILURE;
        }
    }

    /* If there's a unset field with a default value, then we must create */
    /* a specific INSERT statement to avoid unset fields to be bound to NULL */
    if( m_poInsertStatement && (bHasDefaultValue || m_bInsertStatementWithFID != (poFeature->GetFID() != OGRNullFID)) )
    {
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = nullptr;
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
        int err = sqlite3_prepare_v2(poDb, osCommand, -1, &m_poInsertStatement, nullptr);
        if ( err != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "failed to prepare SQL: %s - %s", osCommand.c_str(), sqlite3_errmsg( poDb ));
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
        m_poInsertStatement = nullptr;
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
        m_poInsertStatement = nullptr;
        return OGRERR_FAILURE;
    }

    sqlite3_reset(m_poInsertStatement);
    sqlite3_clear_bindings(m_poInsertStatement);

    if( bHasDefaultValue )
    {
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = nullptr;
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

    /* Update the layer extents with this new object */
    if( IsGeomFieldSet(poFeature) )
    {
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(0);
        if( !poGeom->IsEmpty() )
        {
            OGREnvelope oEnv;
            poGeom->getEnvelope(&oEnv);
            UpdateExtent(&oEnv);

            if( !m_bDeferredSpatialIndexCreation && m_poDS->IsInTransaction() )
            {
                m_nCountInsertInTransaction ++;
                if( m_nCountInsertInTransactionThreshold < 0 )
                {
                    m_nCountInsertInTransactionThreshold = atoi(
                        CPLGetConfigOption("OGR_GPKG_DEFERRED_SPI_UPDATE_THRESHOLD", "100"));
                }
                if( m_nCountInsertInTransaction == m_nCountInsertInTransactionThreshold )
                {
                    StartDeferredSpatialIndexUpdate();
                }
                else if( !m_aoRTreeTriggersSQL.empty() )
                {
                    if( m_aoRTreeEntries.size() == 1000 * 1000 )
                    {
                        if( !FlushPendingSpatialIndexUpdate() )
                            return OGRERR_FAILURE;
                    }
                    GPKGRTreeEntry sEntry;
                    sEntry.nId = nFID;
                    sEntry.fMinX = rtreeValueDown(oEnv.MinX);
                    sEntry.fMaxX = rtreeValueUp(oEnv.MaxX);
                    sEntry.fMinY = rtreeValueDown(oEnv.MinY);
                    sEntry.fMaxY = rtreeValueUp(oEnv.MaxY);
                    m_aoRTreeEntries.push_back(sEntry);
                }
            }
        }
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
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    if( !m_poDS->GetUpdate() || m_pszFidColumn == nullptr )
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
    if( m_iFIDAsRegularColumnIndex >= 0 &&
        !CheckFIDAndFIDColumnConsistency(poFeature, m_iFIDAsRegularColumnIndex) )
    {
        return OGRERR_FAILURE;
    }

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    if( !RunDeferredSpatialIndexUpdate() )
        return OGRERR_FAILURE;

    CheckGeometryType(poFeature);

    if ( ! m_poUpdateStatement )
    {
        /* Construct a SQL UPDATE statement from the OGRFeature */
        /* Only work with fields that are set */
        /* Do not stick values into SQL, use placeholder and bind values later */
        CPLString osCommand = FeatureGenerateUpdateSQL(poFeature);
        if( osCommand.empty() )
            return OGRERR_NONE;

        /* Prepare the SQL into a statement */
        int err = sqlite3_prepare_v2(m_poDS->GetDB(), osCommand, -1, &m_poUpdateStatement, nullptr);
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
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : nullptr;

    if( pszQuery == nullptr )
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
        m_poInsertStatement = nullptr;
    }

    if ( m_poUpdateStatement )
    {
        sqlite3_finalize(m_poUpdateStatement);
        m_poUpdateStatement = nullptr;
    }

    if ( m_poGetFeatureStatement )
    {
        sqlite3_finalize(m_poGetFeatureStatement);
        m_poGetFeatureStatement = nullptr;
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

        if ( m_poFilterGeom != nullptr && m_pszAttrQueryString == nullptr &&
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
        m_poDS->GetDB(), soSQL.c_str(), -1, &m_poQueryStatement, nullptr);
    if ( err != SQLITE_OK )
    {
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
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return nullptr;

    if( m_poFilterGeom != nullptr )
    {
        // Both are exclusive
        CreateSpatialIndexIfNecessary();
        if( !RunDeferredSpatialIndexUpdate() )
            return nullptr;
    }

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
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return nullptr;

    if( m_pszFidColumn == nullptr )
        return OGRLayer::GetFeature(nFID);

    if( m_poGetFeatureStatement == nullptr )
    {
        CPLString soSQL;
        soSQL.Printf("SELECT %s FROM \"%s\" m "
                     "WHERE \"%s\" = ?",
                     m_soColumns.c_str(),
                     SQLEscapeName(m_pszTableName).c_str(),
                     SQLEscapeName(m_pszFidColumn).c_str());

        const int err = sqlite3_prepare_v2(
            m_poDS->GetDB(), soSQL.c_str(), -1, &m_poGetFeatureStatement, nullptr);
        if ( err != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "failed to prepare SQL: %s", soSQL.c_str());
            return nullptr;
        }
    }

    CPL_IGNORE_RET_VAL(sqlite3_bind_int64(m_poGetFeatureStatement, 1, nFID));

    /* Should be only one or zero results */
    const int err = sqlite3_step(m_poGetFeatureStatement);

    /* Aha, got one */
    if ( err == SQLITE_ROW )
    {
        OGRFeature* poFeature = TranslateFeature(m_poGetFeatureStatement);
        if( m_iFIDAsRegularColumnIndex >= 0 )
        {
            poFeature->SetField(m_iFIDAsRegularColumnIndex, poFeature->GetFID());
        }

        sqlite3_reset(m_poGetFeatureStatement);
        sqlite3_clear_bindings(m_poGetFeatureStatement);

        return poFeature;
    }

    sqlite3_reset(m_poGetFeatureStatement);
    sqlite3_clear_bindings(m_poGetFeatureStatement);

    /* Error out on all other return codes */
    return nullptr;
}

/************************************************************************/
/*                        DeleteFeature()                               */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::DeleteFeature(GIntBig nFID)
{
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    if ( !m_poDS->GetUpdate() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "DeleteFeature");
        return OGRERR_FAILURE;
    }
    if( m_pszFidColumn == nullptr )
    {
        return OGRERR_FAILURE;
    }

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    if( !RunDeferredSpatialIndexUpdate() )
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
/*                     DoJobAtTransactionCommit()                       */
/************************************************************************/

bool OGRGeoPackageTableLayer::DoJobAtTransactionCommit()
{
    bool ret = RunDeferredCreationIfNecessary() == OGRERR_NONE &&
               RunDeferredSpatialIndexUpdate();
    m_nCountInsertInTransaction = 0;
    m_aoRTreeTriggersSQL.clear();
    m_aoRTreeEntries.clear();
    return ret;
}

/************************************************************************/
/*                    DoJobAtTransactionRollback()                      */
/************************************************************************/

bool OGRGeoPackageTableLayer::DoJobAtTransactionRollback()
{
    m_nCountInsertInTransaction = 0;
    m_aoRTreeTriggersSQL.clear();
    m_aoRTreeEntries.clear();
    SyncToDisk();
    ResetReading();
    return true;
}

/************************************************************************/
/*                  StartDeferredSpatialIndexUpdate()                   */
/************************************************************************/

bool OGRGeoPackageTableLayer::StartDeferredSpatialIndexUpdate()
{
    if( m_poFeatureDefn->GetGeomFieldCount() == 0 )
        return true;

    m_aoRTreeTriggersSQL.clear();
    m_aoRTreeEntries.clear();

    const char* pszT = m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    m_osRTreeName = "rtree_";
    m_osRTreeName += pszT;
    m_osRTreeName += "_";
    m_osRTreeName += pszC;

    char* pszSQL = sqlite3_mprintf(
        "SELECT sql FROM sqlite_master WHERE type = 'trigger' "
        "AND name IN ('%q', '%q', '%q', '%q', '%q', '%q')",
       (m_osRTreeName + "_insert").c_str(),
       (m_osRTreeName + "_update1").c_str(),
       (m_osRTreeName + "_update2").c_str(),
       (m_osRTreeName + "_update3").c_str(),
       (m_osRTreeName + "_update4").c_str(),
       (m_osRTreeName + "_delete").c_str());
    auto oResult = SQLQuery(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if( oResult )
    {
        for ( int iRecord = 0; iRecord < oResult->RowCount(); iRecord++ )
        {
            const char *pszTriggerSQL = oResult->GetValue(0, iRecord);
            if( pszTriggerSQL )
            {
                m_aoRTreeTriggersSQL.push_back(pszTriggerSQL);
            }
        }
    }
    if( m_aoRTreeTriggersSQL.size() != 6 )
    {
        CPLDebug("GPKG", "Could not find expected 6 RTree triggers");
        m_aoRTreeTriggersSQL.clear();
        return false;
    }

    SQLCommand(m_poDS->GetDB(), ReturnSQLDropSpatialIndexTriggers());

    return true;
}

/************************************************************************/
/*                  FlushPendingSpatialIndexUpdate()                    */
/************************************************************************/

bool OGRGeoPackageTableLayer::FlushPendingSpatialIndexUpdate()
{
    bool ret = true;

    //CPLDebug("GPKG", "Insert %d features in spatial index",
    //         static_cast<int>(m_aoRTreeEntries.size()));

    const char* pszT = m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();

    m_osRTreeName = "rtree_";
    m_osRTreeName += pszT;
    m_osRTreeName += "_";
    m_osRTreeName += pszC;

    char* pszSQL = sqlite3_mprintf(
            "INSERT INTO \"%w\" VALUES (?,?,?,?,?)",
            m_osRTreeName.c_str());
    sqlite3_stmt* hInsertStmt = nullptr;
    if ( sqlite3_prepare_v2(m_poDS->GetDB(), pszSQL, -1, &hInsertStmt, nullptr)
                                                            != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "failed to prepare SQL: %s", pszSQL);
        sqlite3_free(pszSQL);
        m_aoRTreeEntries.clear();
        return false;
    }
    sqlite3_free(pszSQL);

    for( size_t i = 0; i < m_aoRTreeEntries.size(); ++i )
    {
        sqlite3_reset(hInsertStmt);

        sqlite3_bind_int64(hInsertStmt,1,m_aoRTreeEntries[i].nId);
        sqlite3_bind_double(hInsertStmt,2,m_aoRTreeEntries[i].fMinX);
        sqlite3_bind_double(hInsertStmt,3,m_aoRTreeEntries[i].fMaxX);
        sqlite3_bind_double(hInsertStmt,4,m_aoRTreeEntries[i].fMinY);
        sqlite3_bind_double(hInsertStmt,5,m_aoRTreeEntries[i].fMaxY);
        int sqlite_err = sqlite3_step(hInsertStmt);
        if ( sqlite_err != SQLITE_OK && sqlite_err != SQLITE_DONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "failed to execute insertion in RTree : %s",
                    sqlite3_errmsg( m_poDS->GetDB() ) );
            ret = false;
            break;
        }
    }
    sqlite3_finalize(hInsertStmt);
    m_aoRTreeEntries.clear();
    return ret;
}

/************************************************************************/
/*                   RunDeferredSpatialIndexUpdate()                    */
/************************************************************************/

bool OGRGeoPackageTableLayer::RunDeferredSpatialIndexUpdate()
{
    m_nCountInsertInTransaction = 0;
    if( m_aoRTreeTriggersSQL.empty() )
        return true;

    bool ret = FlushPendingSpatialIndexUpdate();

    for( const auto& osSQL: m_aoRTreeTriggersSQL )
    {
        ret &= SQLCommand(m_poDS->GetDB(), osSQL) == OGRERR_NONE;
    }
    m_aoRTreeTriggersSQL.clear();
    return ret;
}

/************************************************************************/
/*                        SyncToDisk()                                  */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::SyncToDisk()
{
    if( !m_bFeatureDefnCompleted )
        return OGRERR_NONE;

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

#ifdef ENABLE_GPKG_OGR_CONTENTS
    CreateTriggers();
#endif

    // Both are exclusive
    CreateSpatialIndexIfNecessary();
    if( !RunDeferredSpatialIndexUpdate() )
        return OGRERR_FAILURE;

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
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
#ifdef ENABLE_GPKG_OGR_CONTENTS
    if( m_poFilterGeom == nullptr && m_pszAttrQueryString == nullptr )
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
            auto oResult = SQLQuery( m_poDS->GetDB(), pszSQL);
            sqlite3_free(pszSQL);
            if( oResult && oResult->RowCount() == 1 )
            {
                const char* pszFeatureCount = oResult->GetValue(0, 0);
                if( pszFeatureCount )
                {
                    m_nTotalFeatureCount = CPLAtoGIntBig(pszFeatureCount);
                }
            }
            if( m_nTotalFeatureCount >= 0 )
            {
                return m_nTotalFeatureCount;
            }
        }
    }
#endif

    if( m_poFilterGeom != nullptr && !m_bFilterIsEnvelope )
        return OGRGeoPackageLayer::GetFeatureCount();

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return 0;

    /* Ignore bForce, because we always do a full count on the database */
    OGRErr err;
    CPLString soSQL;
    if ( m_bIsTable && m_poFilterGeom != nullptr && m_pszAttrQueryString == nullptr &&
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
        if( m_bIsTable && m_poFilterGeom == nullptr && m_pszAttrQueryString == nullptr )
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
/*                        findMinOrMax()                                */
/************************************************************************/

static bool findMinOrMax( GDALGeoPackageDataset* poDS,
                          const CPLString& osRTreeName,
                          const char *pszVarName, bool isMin, double &val )
{
    // We proceed by dichotomic search since unfortunately SELECT MIN(minx)
    // in a RTree is a slow operation
    double minval = -1e10;
    double maxval = 1e10;
    val = 0.0;
    double oldval = 0.0;
    for ( int i = 0; i < 100 && maxval - minval > 1e-18; i++ )
    {
        val = ( minval + maxval ) / 2;
        if ( i > 0 && val == oldval )
        {
            break;
        }
        oldval = val;
        CPLString osSQL = "SELECT 1 FROM ";
        osSQL += "\"" + SQLEscapeName(osRTreeName) + "\"";
        osSQL += " WHERE ";
        osSQL += pszVarName;
        osSQL += isMin ? " < " : " > ";
        osSQL += CPLSPrintf( "%.18g", val );
        osSQL += " LIMIT 1";
        auto oResult = SQLQuery(poDS->GetDB(), osSQL);
        if ( !oResult )
        {
            return false;
        }
        const bool bHasValue = oResult->RowCount() != 0;
        if ( ( isMin && !bHasValue ) || ( !isMin && bHasValue ) )
        {
            minval = val;
        }
        else
        {
            maxval = val;
        }
    }
    return true;
}

/************************************************************************/
/*                        GetExtent()                                   */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    /* Extent already calculated! We're done. */
    if ( m_poExtent != nullptr )
    {
        if ( psExtent )
        {
            *psExtent = *m_poExtent;
        }
        return OGRERR_NONE;
    }

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    /* User is OK with expensive calculation */
    if ( bForce && m_poFeatureDefn->GetGeomFieldCount() )
    {
        if( HasSpatialIndex() && CPLTestBool(
                CPLGetConfigOption("OGR_GPKG_USE_RTREE_FOR_GET_EXTENT", "TRUE")) )
        {
            CPLString osSQL = "SELECT 1 FROM ";
            osSQL += "\"" + SQLEscapeName(m_osRTreeName) + "\"";
            osSQL += " LIMIT 1";
            if( SQLGetInteger(m_poDS->GetDB(), osSQL, nullptr) == 0 )
            {
                UpdateContentsToNullExtent();
                return OGRERR_FAILURE;
            }

            double minx, miny, maxx, maxy;
            if ( findMinOrMax( m_poDS, m_osRTreeName, "MINX", true, minx ) &&
                 findMinOrMax( m_poDS, m_osRTreeName, "MINY", true, miny ) &&
                 findMinOrMax( m_poDS, m_osRTreeName, "MAXX", false, maxx ) &&
                 findMinOrMax( m_poDS, m_osRTreeName, "MAXY", false, maxy ) )
            {
                psExtent->MinX = minx;
                psExtent->MinY = miny;
                psExtent->MaxX = maxx;
                psExtent->MaxY = maxy;
                m_poExtent = new OGREnvelope( *psExtent );
                m_bExtentChanged = true;
                SaveExtent();
                return OGRERR_NONE;
            }
        }

        /* fall back to default implementation (scan all features) and save */
        /* the result for later */
        const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
        char* pszSQL = sqlite3_mprintf(
            "SELECT MIN(ST_MinX(\"%w\")), MIN(ST_MinY(\"%w\")), "
            "MAX(ST_MaxX(\"%w\")), MAX(ST_MaxY(\"%w\")) FROM \"%w\" WHERE "
            "\"%w\" IS NOT NULL AND NOT ST_IsEmpty(\"%w\")",
            pszC, pszC, pszC, pszC, m_pszTableName, pszC, pszC);
        auto oResult = SQLQuery( m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        delete m_poExtent;
        m_poExtent = nullptr;
        if( oResult && oResult->RowCount() == 1 &&
            oResult->GetValue(0, 0) != nullptr )
        {
            psExtent->MinX = CPLAtof(oResult->GetValue(0, 0));
            psExtent->MinY = CPLAtof(oResult->GetValue(1, 0));
            psExtent->MaxX = CPLAtof(oResult->GetValue(2, 0));
            psExtent->MaxY = CPLAtof(oResult->GetValue(3, 0));
            m_poExtent = new OGREnvelope( *psExtent );
            m_bExtentChanged = true;
            SaveExtent();
        }
        else
        {
            UpdateContentsToNullExtent();
            return OGRERR_FAILURE; // we didn't get an extent
        }
        return OGRERR_NONE;
    }

    return OGRERR_FAILURE;
}
/************************************************************************/
/*                     UpdateContentsToNullExtent()                     */
/************************************************************************/

void OGRGeoPackageTableLayer::UpdateContentsToNullExtent()
{
    if( m_poDS->GetUpdate() )
    {
        char* pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_contents SET "
            "min_x = NULL, min_y = NULL, "
            "max_x = NULL, max_y = NULL "
            "WHERE lower(table_name) = lower('%q') AND "
            "Lower(data_type) = 'features'",
            m_pszTableName);
        SQLCommand( m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }
    m_bExtentChanged = false;
}

/************************************************************************/
/*                      RecomputeExtent()                               */
/************************************************************************/

void OGRGeoPackageTableLayer::RecomputeExtent()
{
    m_bExtentChanged = true;
    delete m_poExtent;
    m_poExtent = nullptr;
    OGREnvelope sExtent;
    GetExtent(&sExtent, true);
}

/************************************************************************/
/*                      TestCapability()                                */
/************************************************************************/

int OGRGeoPackageTableLayer::TestCapability ( const char * pszCap )
{
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    if ( EQUAL(pszCap, OLCSequentialWrite) )
    {
        return m_poDS->GetUpdate();
    }
    else if ( EQUAL(pszCap, OLCCreateField) ||
              EQUAL(pszCap, OLCDeleteField) ||
              EQUAL(pszCap, OLCAlterFieldDefn) ||
              EQUAL(pszCap, OLCReorderFields) ||
              EQUAL(pszCap, OLCRename) )
    {
        return m_poDS->GetUpdate() && m_bIsTable;
    }
    else if ( EQUAL(pszCap, OLCDeleteFeature) ||
              EQUAL(pszCap, OLCRandomWrite) )
    {
        return m_poDS->GetUpdate() && m_pszFidColumn != nullptr;
    }
    else if ( EQUAL(pszCap, OLCRandomRead) )
    {
        return m_pszFidColumn != nullptr;
    }
    else if ( EQUAL(pszCap, OLCTransactions) )
    {
        return TRUE;
    }
#ifdef ENABLE_GPKG_OGR_CONTENTS
    else if ( EQUAL(pszCap, OLCFastFeatureCount) )
    {
        return  m_poFilterGeom == nullptr && m_pszAttrQueryString == nullptr &&
                m_nTotalFeatureCount >= 0;
    }
#endif
    else if ( EQUAL(pszCap, OLCFastSpatialFilter) )
    {
        return HasSpatialIndex() || m_bDeferredSpatialIndexCreation;
    }
    else if ( EQUAL(pszCap, OLCFastGetExtent) )
    {
        return ( m_poExtent != nullptr );
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

bool OGRGeoPackageTableLayer::CreateSpatialIndex(const char* pszTableName)
{
    OGRErr err;

    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();

    if( !CheckUpdatableTable("CreateSpatialIndex") )
        return false;

    if( m_bDropRTreeTable )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot run CreateSpatialIndex() after non-completed deferred DropSpatialIndex()");
        return false;
    }

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return false;

    m_bDeferredSpatialIndexCreation = false;

    if( m_pszFidColumn == nullptr )
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

    /* Create virtual table */
    char *pszSQL = sqlite3_mprintf(
                "CREATE VIRTUAL TABLE \"%w\" USING rtree(id, minx, maxx, miny, maxy)",
                m_osRTreeName.c_str() );
    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if( err != OGRERR_NONE )
    {
        m_poDS->SoftRollbackTransaction();
        return false;
    }

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
    sqlite3_stmt* hIterStmt = nullptr;
    if ( sqlite3_prepare_v2(m_poDS->GetDB(), pszSQL, -1, &hIterStmt, nullptr)
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
    sqlite3_stmt* hInsertStmt = nullptr;
    if ( sqlite3_prepare_v2(m_poDS->GetDB(), pszSQL, -1, &hInsertStmt, nullptr)
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

    // Insert entries in RTree by chunks of 500K features
    std::vector<GPKGRTreeEntry> aoEntries;
    GUIntBig nEntryCount = 0;
    constexpr size_t nChunkSize = 500 * 1000;
#ifdef ENABLE_GPKG_OGR_CONTENTS
    if( m_nTotalFeatureCount > 0 )
    {
        aoEntries.reserve(static_cast<size_t>(
            std::min(m_nTotalFeatureCount, static_cast<GIntBig>(nChunkSize))));
    }
#endif
    while( true )
    {
        int sqlite_err = sqlite3_step(hIterStmt);
        bool bFinished = false;
        if( sqlite_err == SQLITE_ROW )
        {
            GPKGRTreeEntry sEntry;
            sEntry.nId = sqlite3_column_int64(hIterStmt, 0);
            sEntry.fMinX = rtreeValueDown(sqlite3_column_double(hIterStmt, 1));
            sEntry.fMaxX = rtreeValueUp(sqlite3_column_double(hIterStmt, 2));
            sEntry.fMinY = rtreeValueDown(sqlite3_column_double(hIterStmt, 3));
            sEntry.fMaxY = rtreeValueUp(sqlite3_column_double(hIterStmt, 4));
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
                sqlite3_bind_double(hInsertStmt,2,aoEntries[i].fMinX);
                sqlite3_bind_double(hInsertStmt,3,aoEntries[i].fMaxX);
                sqlite3_bind_double(hInsertStmt,4,aoEntries[i].fMinY);
                sqlite3_bind_double(hInsertStmt,5,aoEntries[i].fMaxY);
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
        "'http://www.geopackage.org/spec120/#extension_rtree', 'write-only')",
        pszT, pszC );
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    /* Define Triggers to Maintain Spatial Index Values */
    osSQL += ";" + ReturnSQLCreateSpatialIndexTriggers(pszTableName);

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
/*                ReturnSQLCreateSpatialIndexTriggers()                 */
/************************************************************************/

CPLString OGRGeoPackageTableLayer::ReturnSQLCreateSpatialIndexTriggers(
                                                const char* pszTableName)
{
    char* pszSQL;
    CPLString osSQL;

    const char* pszT = (pszTableName) ? pszTableName : m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    const char* pszI = GetFIDColumn();

    CPLString osRTreeName = "rtree_";
    osRTreeName += pszT;
    osRTreeName += "_";
    osRTreeName += pszC;

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
                   osRTreeName.c_str(), pszT,
                   pszC, pszC,
                   osRTreeName.c_str(),
                   pszI,
                   pszC, pszC,
                   pszC, pszC);
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
                   osRTreeName.c_str(), pszC, pszT,
                   pszI, pszI,
                   pszC, pszC,
                   osRTreeName.c_str(),
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
                   osRTreeName.c_str(), pszC, pszT,
                   pszI, pszI,
                   pszC, pszC,
                   osRTreeName.c_str(), pszI);
    osSQL += ";";
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    /* Conditions: Update of any column
                    Row ID change
                    Non-empty geometry
        Actions   : Remove record from rtree for old <i>
                    Insert record into rtree for new <i> */
    pszSQL = sqlite3_mprintf(
                   "CREATE TRIGGER \"%w_update3\" AFTER UPDATE ON \"%w\" "
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
                   osRTreeName.c_str(), pszT,
                   pszI, pszI,
                   pszC, pszC,
                   osRTreeName.c_str(), pszI,
                   osRTreeName.c_str(),
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
                   osRTreeName.c_str(), pszT,
                   pszI, pszI,
                   pszC, pszC,
                   osRTreeName.c_str(), pszI, pszI);
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
                   osRTreeName.c_str(), pszT,
                   pszC,
                   osRTreeName.c_str(), pszI);
    osSQL += ";";
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    return osSQL;
}

/************************************************************************/
/*                    CheckUnknownExtensions()                          */
/************************************************************************/

void OGRGeoPackageTableLayer::CheckUnknownExtensions()
{
    const std::map< CPLString, std::vector<GPKGExtensionDesc> >& oMap =
                                 m_poDS->GetUnknownExtensionsTableSpecific();
    std::map< CPLString, std::vector<GPKGExtensionDesc> >::const_iterator
        oIter = oMap.find( CPLString(m_pszTableName).toupper() );
    if( oIter != oMap.end() )
    {
        for( size_t i=0; i<oIter->second.size();i++)
        {
            const char* pszExtName = oIter->second[i].osExtensionName.c_str();
            const char* pszDefinition = oIter->second[i].osDefinition.c_str();
            const char* pszScope = oIter->second[i].osScope.c_str();
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
}

/************************************************************************/
/*                     CreateGeometryExtensionIfNecessary()             */
/************************************************************************/

bool OGRGeoPackageTableLayer::CreateGeometryExtensionIfNecessary(
                                                    const OGRGeometry* poGeom)
{
    bool bRet = true;
    if( poGeom != nullptr )
    {
        OGRwkbGeometryType eGType = wkbFlatten(poGeom->getGeometryType());
        if( eGType >= wkbGeometryCollection )
        {
            if( eGType > wkbGeometryCollection )
                CreateGeometryExtensionIfNecessary(eGType);
            const OGRGeometryCollection* poGC =
                            dynamic_cast<const OGRGeometryCollection*>(poGeom);
            if( poGC != nullptr )
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
    const bool bExists = SQLGetInteger(m_poDS->GetDB(), pszSQL, nullptr) == 1;
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
                    "VALUES ('%q', '%q', 'gpkg_geom_%s', 'http://www.geopackage.org/spec120/#extension_geometry_types', 'read-write')",
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
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    if( m_bHasSpatialIndex >= 0 )
        return CPL_TO_BOOL(m_bHasSpatialIndex);
    m_bHasSpatialIndex = false;

    if( m_pszFidColumn == nullptr ||
        m_poFeatureDefn->GetGeomFieldCount() == 0 ||
        !m_poDS->HasExtensionsTable() )
        return false;

    const char* pszT = m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    CPLString osRTreeName("rtree_");
    osRTreeName += pszT;
    osRTreeName += "_";
    osRTreeName += pszC;
    const std::map<CPLString, CPLString>& oMap =
                                m_poDS->GetNameTypeMapFromSQliteMaster();
    if( oMap.find( CPLString(osRTreeName).toupper() ) != oMap.end() )
    {
        m_bHasSpatialIndex = true;
        m_osRTreeName = osRTreeName;
        m_osFIDForRTree = m_pszFidColumn;
    }

    return CPL_TO_BOOL(m_bHasSpatialIndex);
}

/************************************************************************/
/*                        DropSpatialIndex()                            */
/************************************************************************/

bool OGRGeoPackageTableLayer::DropSpatialIndex(bool bCalledFromSQLFunction)
{
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    if( !CheckUpdatableTable("DropSpatialIndex") )
        return false;

    if( m_bDropRTreeTable )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot run DropSpatialIndex() after non-completed deferred DropSpatialIndex()");
        return false;
    }

    if( !HasSpatialIndex() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Spatial index not existing");
        return false;
    }

    const char* pszT = m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    {
        char* pszSQL = sqlite3_mprintf(
            "DELETE FROM gpkg_extensions WHERE lower(table_name)=lower('%q') "
            "AND lower(column_name)=lower('%q') AND extension_name='gpkg_rtree_index'",
            pszT, pszC );
        SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }

    if( bCalledFromSQLFunction )
    {
        /* We cannot drop a table from a SQLite function call, so we just */
        /* memorize that we will have to delete the table later */
        m_bDropRTreeTable = true;
    }
    else
    {
        char* pszSQL = sqlite3_mprintf("DROP TABLE \"%w\"", m_osRTreeName.c_str());
        SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }

    m_poDS->RemoveTableFromSQLiteMasterCache(m_osRTreeName);

    SQLCommand(m_poDS->GetDB(), ReturnSQLDropSpatialIndexTriggers().c_str());

    m_bHasSpatialIndex = false;
    return true;
}

/************************************************************************/
/*               RunDeferredDropRTreeTableIfNecessary()                 */
/************************************************************************/

bool OGRGeoPackageTableLayer::RunDeferredDropRTreeTableIfNecessary()
{
    bool ret = true;
    if( m_bDropRTreeTable )
    {
        OGRGeoPackageTableLayer::ResetReading();

        char* pszSQL =
            sqlite3_mprintf("DROP TABLE \"%w\"", m_osRTreeName.c_str());
        ret = SQLCommand(m_poDS->GetDB(), pszSQL) == OGRERR_NONE;
        sqlite3_free(pszSQL);
        m_bDropRTreeTable = false;
    }
    return ret;
}

/************************************************************************/
/*                   ReturnSQLDropSpatialIndexTriggers()                */
/************************************************************************/

CPLString OGRGeoPackageTableLayer::ReturnSQLDropSpatialIndexTriggers()
{
    char* pszSQL = sqlite3_mprintf("DROP TRIGGER \"%w_insert\";"
                             "DROP TRIGGER \"%w_update1\";"
                             "DROP TRIGGER \"%w_update2\";"
                             "DROP TRIGGER \"%w_update3\";"
                             "DROP TRIGGER \"%w_update4\";"
                             "DROP TRIGGER \"%w_delete\";",
                             m_osRTreeName.c_str(),
                             m_osRTreeName.c_str(),
                             m_osRTreeName.c_str(),
                             m_osRTreeName.c_str(),
                             m_osRTreeName.c_str(),
                             m_osRTreeName.c_str());
    CPLString osSQL(pszSQL);
    sqlite3_free(pszSQL);

    return osSQL;
}

/************************************************************************/
/*                           Rename()                                   */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::Rename(const char* pszDstTableName)
{
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    if( !CheckUpdatableTable("Rename") )
        return OGRERR_FAILURE;

    ResetReading();
    SyncToDisk();

    char* pszSQL = sqlite3_mprintf(
        "SELECT 1 FROM sqlite_master WHERE lower(name) = lower('%q') "
        "AND type IN ('table', 'view')",
         pszDstTableName);
    const bool bAlreadyExists =
            SQLGetInteger(m_poDS->GetDB(), pszSQL, nullptr) == 1;
    sqlite3_free(pszSQL);
    if( bAlreadyExists )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Table %s already exists",
                 pszDstTableName);
        return OGRERR_FAILURE;
    }

    if( m_poDS->SoftStartTransaction() != OGRERR_NONE )
        return OGRERR_FAILURE;

#ifdef ENABLE_GPKG_OGR_CONTENTS
    DisableTriggers(false);
#endif

    CPLString osSQL;

    pszSQL = sqlite3_mprintf(
        "UPDATE gpkg_geometry_columns SET table_name = '%q' WHERE "
        "lower(table_name )= lower('%q')",
        pszDstTableName, m_pszTableName);
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    // Rename the identifier if it defaulted to the table name
    pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_contents SET identifier = '%q' WHERE "
            "lower(table_name) = lower('%q') AND identifier = '%q'",
            pszDstTableName, m_pszTableName, m_pszTableName);
    osSQL += ";";
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_contents SET table_name = '%q' WHERE "
            "lower(table_name )= lower('%q')",
            pszDstTableName, m_pszTableName);
    osSQL += ";";
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    if( m_poDS->HasExtensionsTable() )
    {
        pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_extensions SET table_name = '%q' WHERE "
            "lower(table_name )= lower('%q')",
            pszDstTableName, m_pszTableName);
        osSQL += ";";
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
    }

    if( m_poDS->HasMetadataTables() )
    {
        pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_metadata_reference SET table_name = '%q' WHERE "
            "lower(table_name )= lower('%q')",
            pszDstTableName, m_pszTableName);
        osSQL += ";";
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
    }

    if( m_poDS->HasDataColumnsTable() )
    {
        pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_data_columns SET table_name = '%q' WHERE "
            "lower(table_name )= lower('%q')",
            pszDstTableName, m_pszTableName);
        osSQL += ";";
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
    }

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if( m_poDS->m_bHasGPKGOGRContents )
    {
        pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_ogr_contents SET table_name = '%q' WHERE "
            "lower(table_name )= lower('%q')",
            pszDstTableName, m_pszTableName);
        osSQL += ";";
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
    }
#endif

    pszSQL = sqlite3_mprintf(
            "ALTER TABLE \"%w\" RENAME TO \"%w\"",
            m_pszTableName, pszDstTableName );
    osSQL += ";";
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    const bool bHasSpatialIndex = HasSpatialIndex();
    CPLString osRTreeNameNew;
    if( bHasSpatialIndex )
    {
        osRTreeNameNew = "rtree_";
        osRTreeNameNew += pszDstTableName;
        osRTreeNameNew += "_";
        osRTreeNameNew += m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();

        osSQL += ";";
        osSQL += ReturnSQLDropSpatialIndexTriggers();

        pszSQL = sqlite3_mprintf(
                "ALTER TABLE \"%w\" RENAME TO \"%w\"",
                m_osRTreeName.c_str(), osRTreeNameNew.c_str());
        osSQL += ";";
        osSQL += pszSQL;
        sqlite3_free(pszSQL);

        osSQL += ";";
        osSQL += ReturnSQLCreateSpatialIndexTriggers(pszDstTableName);
    }

    OGRErr eErr = SQLCommand(m_poDS->GetDB(), osSQL);

    // Check foreign key integrity
    if ( eErr == OGRERR_NONE )
    {
        eErr = m_poDS->PragmaCheck("foreign_key_check", "", 0);
    }

    if( eErr == OGRERR_NONE)
    {
#ifdef ENABLE_GPKG_OGR_CONTENTS
        CreateTriggers(pszDstTableName);
#endif

        eErr = m_poDS->SoftCommitTransaction();
        if( eErr == OGRERR_NONE)
        {
            m_poDS->RemoveTableFromSQLiteMasterCache(m_pszTableName);

            CPLFree(m_pszTableName);
            m_pszTableName = CPLStrdup(pszDstTableName);

            if( bHasSpatialIndex )
            {
                m_poDS->RemoveTableFromSQLiteMasterCache(m_osRTreeName);
                m_osRTreeName = osRTreeNameNew;
            }
        }
    }
    else
    {
        m_poDS->SoftRollbackTransaction();
    }

    if( eErr == OGRERR_NONE)
    {
        SetDescription(pszDstTableName);
        m_poFeatureDefn->SetName(pszDstTableName);
    }

    return eErr;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRGeoPackageTableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    if( InstallFilter( poGeomIn ) )
    {
        BuildWhere();

        ResetReading();
    }
}

/************************************************************************/
/*                        HasFastSpatialFilter()                        */
/************************************************************************/

bool OGRGeoPackageTableLayer::HasFastSpatialFilter( int iGeomColIn )
{
    if( iGeomColIn < 0 || iGeomColIn >= m_poFeatureDefn->GetGeomFieldCount() )
        return false;
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

    if( poFilterGeom != nullptr )
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
/*                        SetOpeningParameters()                        */
/************************************************************************/

void OGRGeoPackageTableLayer::SetOpeningParameters(const char* pszObjectType,
                                                   bool bIsInGpkgContents,
                                                   bool bIsSpatial,
                                                   const char* pszGeomColName,
                                                   const char* pszGeomType,
                                                   bool bHasZ,
                                                   bool bHasM)
{
    m_bIsTable = EQUAL(pszObjectType, "table");
    m_bIsInGpkgContents = bIsInGpkgContents;
    m_bIsSpatial = bIsSpatial;
    if( pszGeomType )
    {
        OGRwkbGeometryType eType =
                GPkgGeometryTypeToWKB(pszGeomType, bHasZ, bHasM);
        m_poFeatureDefn->SetGeomType(eType);
        if( eType != wkbNone )
        {
            m_poFeatureDefn->GetGeomFieldDefn(0)->SetName(pszGeomColName);
        }
    }
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
    m_bIsSpatial = eGType != wkbNone;
    m_bIsInGpkgContents = true;
    m_bFeatureDefnCompleted = true;
    m_bDeferredCreation = true;
    m_bHasTriedDetectingFID64 = true;
    m_pszFidColumn = CPLStrdup(pszFIDColumnName);

    if( eGType != wkbNone )
    {
        m_nZFlag = wkbHasZ(eGType) ? 1 : 0;
        m_nMFlag = wkbHasM(eGType) ? 1 : 0;
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

    /* Update gpkg_geometry_columns with the table info */
    char* pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_geometry_columns "
        "(table_name,column_name,geometry_type_name,srs_id,z,m)"
        " VALUES "
        "('%q','%q','%q',%d,%d,%d)",
        GetName(),GetGeometryColumn(),pszGeometryType,
        m_iSrs, m_nZFlag,
        m_nMFlag);

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

    char *pszSQL = nullptr;
    bool bNeedComma = false;
    if( m_pszFidColumn != nullptr )
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
        if( poFieldDefn->IsUnique() )
        {
            osSQL += " UNIQUE";
        }
        const char* pszDefault = poFieldDefn->GetDefault();
        if( pszDefault != nullptr &&
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

    for( auto& poField: apoFields )
    {
        if( !DoSpecialProcessingForColumnCreation(poField) )
        {
            return OGRERR_FAILURE;
        }
    }

    /* Update gpkg_contents with the table info */
    const OGRwkbGeometryType eGType = GetGeomType();
    const bool bIsSpatial = (eGType != wkbNone);
    if ( bIsSpatial )
        err = RegisterGeometryColumn();

    if ( err != OGRERR_NONE )
        return OGRERR_FAILURE;

    if( bIsSpatial ||
        m_eASpatialVariant == GPKG_ATTRIBUTES )
    {
        const char* pszIdentifier = GetMetadataItem("IDENTIFIER");
        if( pszIdentifier == nullptr )
            pszIdentifier = pszLayerName;
        const char* pszDescription = GetMetadataItem("DESCRIPTION");
        if( pszDescription == nullptr )
            pszDescription = "";

        pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_contents "
            "(table_name,data_type,identifier,description,last_change,srs_id) VALUES "
            "('%q','%q','%q','%q',%s,%d)",
            pszLayerName, (bIsSpatial ? "features": "attributes" ),
            pszIdentifier, pszDescription,
            GDALGeoPackageDataset::GetCurrentDateEscapedSQL().c_str(),
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
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
    if( !m_bHasTriedDetectingFID64 && m_pszFidColumn != nullptr )
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

            nMaxId = SQLGetInteger64( m_poDS->GetDB(), pszSQL, nullptr);
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
        "SELECT md.metadata, md.md_standard_uri, md.mime_type "
        "FROM gpkg_metadata md "
        "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
        "WHERE lower(mdr.table_name) = lower('%q') ORDER BY md.id "
        "LIMIT 1000", // to avoid denial of service
        m_pszTableName);

    auto oResult = SQLQuery(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if  ( !oResult )
    {
        return OGRLayer::GetMetadata( pszDomain );
    }

    char** papszMetadata = CSLDuplicate(OGRLayer::GetMetadata());

    /* GDAL metadata */
    for(int i=0;i<oResult->RowCount();i++)
    {
        const char *pszMetadata = oResult->GetValue(0, i);
        const char* pszMDStandardURI = oResult->GetValue(1, i);
        const char* pszMimeType = oResult->GetValue(2, i);
        if( pszMetadata && pszMDStandardURI && pszMimeType &&
            EQUAL(pszMDStandardURI, "http://gdal.org") &&
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
    papszMetadata = nullptr;

    /* Add non-GDAL metadata now */
    int nNonGDALMDILocal = 1;
    for(int i=0;i<oResult->RowCount();i++)
    {
        const char *pszMetadata = oResult->GetValue(0, i);
        const char* pszMDStandardURI = oResult->GetValue(1, i);
        const char* pszMimeType = oResult->GetValue(2, i);
        //const char* pszReferenceScope = oResult->GetValue(3, i);
        //int bIsGPKGScope = EQUAL(pszReferenceScope, "geopackage");
        if( EQUAL(pszMDStandardURI, "http://gdal.org") &&
            EQUAL(pszMimeType, "text/xml") )
            continue;

        if( EQUAL(pszMDStandardURI, "http://gdal.org") &&
            EQUAL(pszMimeType, "text/plain") )
        {
            if( STARTS_WITH_CI(pszMetadata, "coordinate_epoch=") )
            {
                continue;
            }
        }

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
    if( pszDomain == nullptr || EQUAL(pszDomain, "") )
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
        (pszDomain == nullptr || EQUAL(pszDomain, "")) )
        return CE_None;
    if( !m_osDescriptionLCO.empty() && EQUAL(pszName, "DESCRIPTION") &&
        (pszDomain == nullptr || EQUAL(pszDomain, "")) )
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
    OGRErr eErr = OGRERR_NONE;
    auto oTriggers = SQLQuery(hDB, pszSQL);
    sqlite3_free(pszSQL);

/* -------------------------------------------------------------------- */
/*      Make a temporary table with new content.                        */
/* -------------------------------------------------------------------- */
    if( oTriggers )
    {
        pszSQL = sqlite3_mprintf("CREATE TABLE \"%w_ogr_tmp\" (%s)",
                                m_pszTableName, osColumnsForCreate.c_str());
        eErr = SQLCommand( hDB, pszSQL );
        sqlite3_free(pszSQL);
    } else {
        eErr = OGRERR_FAILURE;
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
    for( int i = 0; oTriggers && i < oTriggers->RowCount() && eErr == OGRERR_NONE; i++)
    {
        const char* pszSQLTriggerIdx = oTriggers->GetValue( 0, i );
        if (pszSQLTriggerIdx != nullptr && *pszSQLTriggerIdx != '\0')
        {
            eErr = SQLCommand( hDB, pszSQLTriggerIdx );
        }
    }

    return eErr;
}

/************************************************************************/
/*                          BuildSelectFieldList()                      */
/************************************************************************/

CPLString OGRGeoPackageTableLayer::BuildSelectFieldList(const std::vector<OGRFieldDefn*>& apoFields)
{
    CPLString osFieldListForSelect;

    char *pszSQL = nullptr;
    bool bNeedComma = false;

    if( m_pszFidColumn != nullptr )
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
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
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
    if( !RunDeferredSpatialIndexUpdate() )
        return OGRERR_FAILURE;

    const char* pszFieldName =
        m_poFeatureDefn->GetFieldDefn(iFieldToDelete)->GetNameRef();

/* -------------------------------------------------------------------- */
/*      Drop any iterator since we change the DB structure              */
/* -------------------------------------------------------------------- */
    m_poDS->ResetReadingAllLayers();

    if( m_poDS->SoftStartTransaction() != OGRERR_NONE )
        return OGRERR_FAILURE;

    // ALTER TABLE ... DROP COLUMN ... was first implemented in 3.35.0 but
    // there was bug fixes related to it until 3.35.5
#if SQLITE_VERSION_NUMBER >= 3035005L
    OGRErr eErr = SQLCommand( m_poDS->GetDB(),
                       CPLString().Printf("ALTER TABLE \"%s\" DROP COLUMN \"%s\"",
                          SQLEscapeName(m_pszTableName).c_str(),
                          SQLEscapeName(pszFieldName).c_str()).c_str() );
#else
/* -------------------------------------------------------------------- */
/*      Recreate table in a transaction                                 */
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

    OGRErr eErr = RecreateTable(osColumnsForCreate, osFieldListForSelect);
#endif

/* -------------------------------------------------------------------- */
/*      Update gpkg_extensions if needed.                               */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE && m_poDS->HasExtensionsTable() )
    {
        char* pszSQL = sqlite3_mprintf(
            "DELETE FROM gpkg_extensions WHERE lower(table_name) = lower('%q') AND "
            "lower(column_name) = lower('%q')",
            m_pszTableName,
            pszFieldName);
        eErr = SQLCommand( m_poDS->GetDB(), pszSQL );
        sqlite3_free(pszSQL);
    }

/* -------------------------------------------------------------------- */
/*      Update gpkg_data_columns if needed.                             */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE && m_poDS->HasDataColumnsTable() )
    {
        char* pszSQL = sqlite3_mprintf(
            "DELETE FROM gpkg_data_columns WHERE lower(table_name) = lower('%q') AND "
            "lower(column_name) = lower('%q')",
            m_pszTableName,
            pszFieldName);
        eErr = SQLCommand( m_poDS->GetDB(), pszSQL );
        sqlite3_free(pszSQL);
    }

/* -------------------------------------------------------------------- */
/*      Update gpkg_metadata_reference if needed.                       */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE && m_poDS->HasMetadataTables() )
    {
        {
            // Delete from gpkg_metadata metadata records that are only referenced
            // by the column we are about to drop
            char* pszSQL = sqlite3_mprintf(
                    "DELETE FROM gpkg_metadata WHERE id IN ("
                    "SELECT DISTINCT md_file_id FROM "
                    "gpkg_metadata_reference WHERE "
                    "lower(table_name) = lower('%q') "
                    "AND lower(column_name) = lower('%q') "
                    "AND md_parent_id is NULL) "
                    "AND id NOT IN ("
                    "SELECT DISTINCT md_file_id FROM gpkg_metadata_reference WHERE "
                    "md_file_id IN ("
                    "SELECT DISTINCT md_file_id FROM "
                    "gpkg_metadata_reference WHERE "
                    "lower(table_name) = lower('%q') "
                    "AND lower(column_name) = lower('%q') "
                    "AND md_parent_id is NULL) "
                    "AND ("
                    "lower(table_name) <> lower('%q') "
                    "OR column_name IS NULL "
                    "OR lower(column_name) <> lower('%q')))",
                    m_pszTableName,
                    pszFieldName,
                    m_pszTableName,
                    pszFieldName,
                    m_pszTableName,
                    pszFieldName);
            eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
            sqlite3_free(pszSQL);
        }

        if( eErr == OGRERR_NONE )
        {
            char* pszSQL = sqlite3_mprintf(
                "DELETE FROM gpkg_metadata_reference WHERE "
                "lower(table_name) = lower('%q') AND "
                "lower(column_name) = lower('%q')",
                m_pszTableName,
                pszFieldName);
            eErr = SQLCommand( m_poDS->GetDB(), pszSQL );
            sqlite3_free(pszSQL);
        }
    }

/* -------------------------------------------------------------------- */
/*      Check foreign key integrity if enforcement of foreign keys      */
/*      constraint is enabled.                                          */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE &&
        SQLGetInteger(m_poDS->GetDB(), "PRAGMA foreign_keys", nullptr) )
    {
        CPLDebug("GPKG", "Running PRAGMA foreign_key_check");
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
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
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
    if( !RunDeferredSpatialIndexUpdate() )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Check that the new column name is not a duplicate.              */
/* -------------------------------------------------------------------- */

    OGRFieldDefn* poFieldDefnToAlter = m_poFeatureDefn->GetFieldDefn(iFieldToAlter);
    const CPLString osOldColName( poFieldDefnToAlter->GetNameRef() );
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
/*      Build the modified field definition from the flags.             */
/* -------------------------------------------------------------------- */
    OGRFieldDefn oTmpFieldDefn(poFieldDefnToAlter);
    bool bUseRewriteSchemaMethod = ( m_poDS->nSoftTransactionLevel == 0 );
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
        bUseRewriteSchemaMethod = false;
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
        bUseRewriteSchemaMethod = false;
        oTmpFieldDefn.SetUnique( poNewFieldDefn->IsUnique());
    }
    if( (nFlagsIn & ALTER_DOMAIN_FLAG) != 0 &&
        poFieldDefnToAlter->GetDomainName() != poNewFieldDefn->GetDomainName() )
    {
        nActualFlags |= ALTER_DOMAIN_FLAG;
        oTmpFieldDefn.SetDomainName( poNewFieldDefn->GetDomainName());
    }

/* -------------------------------------------------------------------- */
/*      Build list of old fields, and the list of new fields.           */
/* -------------------------------------------------------------------- */
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

    // ALTER TABLE ... RENAME COLUMN ... was first implemented in 3.25.0 but
    // 3.26.0 was required so that foreign key constraints are updated as well
#if SQLITE_VERSION_NUMBER >= 3026000L
    const bool bUseRenameColumn = (nActualFlags == ALTER_NAME_FLAG);
    if( bUseRenameColumn )
        bUseRewriteSchemaMethod = false;
#else
    constexpr bool bUseRenameColumn = false;
#endif

    if( m_poDS->SoftStartTransaction() != OGRERR_NONE )
        return OGRERR_FAILURE;

    sqlite3 *hDB = m_poDS->GetDB();
    OGRErr eErr = OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Drop triggers and index that look like to be related to the     */
/*      column if renaming. We re-install some indexes afterwards.      */
/* -------------------------------------------------------------------- */
    std::unique_ptr<SQLResult> oTriggers;
    // cppcheck-suppress knownConditionTrueFalse
    if( bRenameCol && !bUseRenameColumn )
    {
        char* pszSQL = sqlite3_mprintf(
            "SELECT name, type, sql FROM sqlite_master WHERE "
            "type IN ('trigger','index') "
            "AND lower(tbl_name)=lower('%q') AND sql LIKE '%%%q%%' LIMIT 10000",
            m_pszTableName,
            SQLEscapeName(osOldColName).c_str() );
        oTriggers = SQLQuery(hDB, pszSQL);
        sqlite3_free(pszSQL);

        if (!oTriggers) {
            eErr = OGRERR_FAILURE;
        }

        for( int i = 0; oTriggers && i < oTriggers->RowCount(); i++)
        {
            pszSQL = sqlite3_mprintf("DROP %s \"%w\"",
                                     oTriggers->GetValue(1, i),
                                     oTriggers->GetValue(0, i));
            eErr = SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
    }

#if SQLITE_VERSION_NUMBER >= 3026000L
    if( bUseRenameColumn )
    {
        if( eErr == OGRERR_NONE )
        {
            CPLDebug("GPKG", "Running ALTER TABLE RENAME COLUMN");
            eErr = SQLCommand( m_poDS->GetDB(),
                       CPLString().Printf("ALTER TABLE \"%s\" RENAME COLUMN \"%s\" TO \"%s\"",
                          SQLEscapeName(m_pszTableName).c_str(),
                          SQLEscapeName(osOldColName).c_str(),
                          SQLEscapeName(osNewColName).c_str()).c_str() );
        }
    }
    else
#endif
    if( !bUseRewriteSchemaMethod )
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
/*      Update gpkg_metadata_reference if needed.                       */
/* -------------------------------------------------------------------- */
    if( bRenameCol && eErr == OGRERR_NONE && m_poDS->HasMetadataTables() )
    {
        char* pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_metadata_reference SET column_name = '%q' WHERE "
            "lower(table_name) = lower('%q') AND lower(column_name) = lower('%q')",
            poNewFieldDefn->GetNameRef(),
            m_pszTableName,
            osOldColName.c_str() );
        eErr = SQLCommand( hDB, pszSQL );
        sqlite3_free(pszSQL);
    }

/* -------------------------------------------------------------------- */
/*      Run integrity check only if explicitly required.                */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE &&
        CPLTestBool(CPLGetConfigOption("OGR_GPKG_INTEGRITY_CHECK", "NO")) )
    {
        CPLDebug("GPKG", "Running PRAGMA integrity_check");
        eErr = m_poDS->PragmaCheck("integrity_check", "ok", 1);
    }

/* -------------------------------------------------------------------- */
/*      Otherwise check foreign key integrity if enforcement of foreign */
/*      kets constraint is enabled.                                     */
/* -------------------------------------------------------------------- */
    else if( eErr == OGRERR_NONE &&
        SQLGetInteger(m_poDS->GetDB(), "PRAGMA foreign_keys", nullptr) )
    {
        CPLDebug("GPKG", "Running PRAGMA foreign_key_check");
        eErr = m_poDS->PragmaCheck("foreign_key_check", "", 0);
    }

/* -------------------------------------------------------------------- */
/*      Finish                                                          */
/* -------------------------------------------------------------------- */
    if( eErr == OGRERR_NONE)
    {
        eErr = m_poDS->SoftCommitTransaction();

        // We need to force database reopening due to schema change
        if( eErr == OGRERR_NONE && bUseRewriteSchemaMethod && !m_poDS->ReOpenDB() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot reopen database");
            eErr = OGRERR_FAILURE;
        }
        hDB = m_poDS->GetDB();

/* -------------------------------------------------------------------- */
/*      Recreate indices.                                               */
/* -------------------------------------------------------------------- */
        for( int i = 0; oTriggers && i < oTriggers->RowCount() && eErr == OGRERR_NONE ; i++)
        {
            if( EQUAL(oTriggers->GetValue(1, i), "index") )
            {
                CPLString osSQL( oTriggers->GetValue(2, i) );
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
            bool bRunDoSpecialProcessingForColumnCreation = false;
            bool bDeleteFromGpkgDataColumns = false;
            if (nActualFlags & ALTER_TYPE_FLAG)
            {
                if( poFieldDefnToAlter->GetSubType() == OFSTJSON &&
                    poNewFieldDefn->GetSubType() == OFSTNone )
                {
                    bDeleteFromGpkgDataColumns = true;
                }
                else if ( poFieldDefnToAlter->GetSubType() == OFSTNone &&
                          poNewFieldDefn->GetType() == OFTString &&
                          poNewFieldDefn->GetSubType() == OFSTJSON )
                {
                    bRunDoSpecialProcessingForColumnCreation = true;
                }

                poFieldDefnToAlter->SetSubType(OFSTNone);
                poFieldDefnToAlter->SetType(poNewFieldDefn->GetType());
                poFieldDefnToAlter->SetSubType(poNewFieldDefn->GetSubType());
            }
            if (nActualFlags & ALTER_NAME_FLAG)
            {
                poFieldDefnToAlter->SetName(poNewFieldDefn->GetNameRef());
            }
            if (nActualFlags & ALTER_WIDTH_PRECISION_FLAG)
            {
                poFieldDefnToAlter->SetWidth(poNewFieldDefn->GetWidth());
                poFieldDefnToAlter->SetPrecision(poNewFieldDefn->GetPrecision());
            }
            if (nActualFlags & ALTER_NULLABLE_FLAG)
                poFieldDefnToAlter->SetNullable(poNewFieldDefn->IsNullable());
            if (nActualFlags & ALTER_DEFAULT_FLAG)
                poFieldDefnToAlter->SetDefault(poNewFieldDefn->GetDefault());
            if (nActualFlags & ALTER_UNIQUE_FLAG)
                poFieldDefnToAlter->SetUnique(poNewFieldDefn->IsUnique());
            if ( (nActualFlags & ALTER_DOMAIN_FLAG) &&
                poFieldDefnToAlter->GetDomainName() != poNewFieldDefn->GetDomainName() )
            {
                if( !poFieldDefnToAlter->GetDomainName().empty() )
                {
                    bDeleteFromGpkgDataColumns = true;
                }

                if( !poNewFieldDefn->GetDomainName().empty() )
                {
                    bRunDoSpecialProcessingForColumnCreation = true;
                }

                poFieldDefnToAlter->SetDomainName(poNewFieldDefn->GetDomainName());
            }

            if( bDeleteFromGpkgDataColumns )
            {
                char* pszSQL = sqlite3_mprintf(
                    "DELETE FROM gpkg_data_columns WHERE "
                    "lower(table_name) = lower('%q') AND "
                    "lower(column_name) = lower('%q')",
                    m_pszTableName,
                    poFieldDefnToAlter->GetNameRef() );
                eErr = SQLCommand( m_poDS->GetDB(), pszSQL );
                sqlite3_free(pszSQL);
            }

            if( bRunDoSpecialProcessingForColumnCreation )
            {
                if( !DoSpecialProcessingForColumnCreation(poFieldDefnToAlter) )
                    eErr = OGRERR_FAILURE;
            }

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
/*                           ReorderFields()                            */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::ReorderFields( int* panMap )
{
    if( !m_bFeatureDefnCompleted )
        GetLayerDefn();
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
    if( !RunDeferredSpatialIndexUpdate() )
        return OGRERR_FAILURE;

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
