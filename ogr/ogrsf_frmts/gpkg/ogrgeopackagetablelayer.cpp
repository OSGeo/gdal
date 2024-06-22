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
#include "cpl_md5.h"
#include "cpl_time.h"
#include "ogr_p.h"
#include "sqlite_rtree_bulk_load/wrapper.h"
#include "gdal_priv_templates.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#undef SQLITE_STATIC
#define SQLITE_STATIC static_cast<sqlite3_destructor_type>(nullptr)
#undef SQLITE_TRANSIENT
#define SQLITE_TRANSIENT reinterpret_cast<sqlite3_destructor_type>(-1)

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
    if (!m_poDS->GetUpdate() || !m_bExtentChanged || !m_poExtent)
        return OGRERR_NONE;

    sqlite3 *poDb = m_poDS->GetDB();

    if (!poDb)
        return OGRERR_FAILURE;

    char *pszSQL =
        sqlite3_mprintf("UPDATE gpkg_contents SET "
                        "min_x = %.18g, min_y = %.18g, "
                        "max_x = %.18g, max_y = %.18g "
                        "WHERE lower(table_name) = lower('%q') AND "
                        "Lower(data_type) = 'features'",
                        m_poExtent->MinX, m_poExtent->MinY, m_poExtent->MaxX,
                        m_poExtent->MaxY, m_pszTableName);

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
    if (!m_poDS->GetUpdate() || !m_bContentChanged)
        return OGRERR_NONE;

    m_bContentChanged = false;

    OGRErr err = m_poDS->UpdateGpkgContentsLastChange(m_pszTableName);

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (m_bIsTable && err == OGRERR_NONE && m_poDS->m_bHasGPKGOGRContents &&
        !m_bOGRFeatureCountTriggersEnabled && m_nTotalFeatureCount >= 0)
    {
        CPLString osFeatureCount;
        osFeatureCount.Printf(CPL_FRMT_GIB, m_nTotalFeatureCount);
        char *pszSQL = sqlite3_mprintf("UPDATE gpkg_ogr_contents SET "
                                       "feature_count = %s "
                                       "WHERE lower(table_name) = lower('%q')",
                                       osFeatureCount.c_str(), m_pszTableName);
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
OGRErr OGRGeoPackageTableLayer::UpdateExtent(const OGREnvelope *poExtent)
{
    if (!m_poExtent)
    {
        m_poExtent = new OGREnvelope(*poExtent);
    }
    m_poExtent->Merge(*poExtent);
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
    m_anFieldOrdinals.resize(m_poFeatureDefn->GetFieldCount());
    int iCurCol = 0;

    /* Always start with a primary key */
    CPLString soColumns;
    if (m_bIsTable || m_pszFidColumn != nullptr)
    {
        soColumns += "m.";
        soColumns += m_pszFidColumn
                         ? "\"" + SQLEscapeName(m_pszFidColumn) + "\""
                         : "_rowid_";
        m_iFIDCol = iCurCol;
        iCurCol++;
    }

    /* Add a geometry column if there is one (just one) */
    if (m_poFeatureDefn->GetGeomFieldCount())
    {
        const auto poFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(0);
        if (poFieldDefn->IsIgnored())
        {
            m_iGeomCol = -1;
        }
        else
        {
            if (!soColumns.empty())
                soColumns += ", ";
            soColumns += "m.\"";
            soColumns += SQLEscapeName(poFieldDefn->GetNameRef());
            soColumns += "\"";
            m_iGeomCol = iCurCol;
            iCurCol++;
        }
    }

    /* Add all the attribute columns */
    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++)
    {
        const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        if (poFieldDefn->IsIgnored())
        {
            m_anFieldOrdinals[i] = -1;
        }
        else
        {
            if (!soColumns.empty())
                soColumns += ", ";
            soColumns += "m.\"";
            soColumns += SQLEscapeName(poFieldDefn->GetNameRef());
            soColumns += "\"";
            m_anFieldOrdinals[i] = iCurCol;
            iCurCol++;
        }
    }

    if (soColumns.empty())
    {
        // Can happen if ignoring all fields on a view...
        soColumns = "NULL";
    }
    m_soColumns = std::move(soColumns);
    return OGRERR_NONE;
}

//----------------------------------------------------------------------
// IsGeomFieldSet()
//
// Utility method to determine if there is a non-Null geometry
// in an OGRGeometry.
//
bool OGRGeoPackageTableLayer::IsGeomFieldSet(OGRFeature *poFeature)
{
    return poFeature->GetDefnRef()->GetGeomFieldCount() &&
           poFeature->GetGeomFieldRef(0);
}

OGRErr OGRGeoPackageTableLayer::FeatureBindParameters(
    OGRFeature *poFeature, sqlite3_stmt *poStmt, int *pnColCount, bool bAddFID,
    bool bBindUnsetFields, int nUpdatedFieldsCount,
    const int *panUpdatedFieldsIdx, int nUpdatedGeomFieldsCount,
    const int * /*panUpdatedGeomFieldsIdx*/)
{
    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    int nColCount = 1;
    if (bAddFID)
    {
        int err = sqlite3_bind_int64(poStmt, nColCount++, poFeature->GetFID());
        if (err != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "sqlite3_bind_int64() failed");
            return OGRERR_FAILURE;
        }
    }

    // Bind data values to the statement, here bind the blob for geometry.
    // We bind only if there's a geometry column (poFeatureDefn->GetGeomFieldCount() > 0)
    // and if we are:
    // - either in CreateFeature/SetFeature mode: nUpdatedGeomFieldsCount < 0
    // - or in UpdateFeature mode with nUpdatedGeomFieldsCount == 1, which
    //   implicitly involves that panUpdatedGeomFieldsIdx[0] == 0, so we don't
    //   need to test this condition.
    if ((nUpdatedGeomFieldsCount < 0 || nUpdatedGeomFieldsCount == 1) &&
        poFeatureDefn->GetGeomFieldCount())
    {
        // Non-NULL geometry.
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(0);
        if (poGeom)
        {
            size_t szWkb = 0;
            GByte *pabyWkb = GPkgGeometryFromOGR(poGeom, m_iSrs,
                                                 &m_sBinaryPrecision, &szWkb);
            if (!pabyWkb)
                return OGRERR_FAILURE;
            int err = sqlite3_bind_blob(poStmt, nColCount++, pabyWkb,
                                        static_cast<int>(szWkb), CPLFree);
            if (err != SQLITE_OK)
            {
                if (err == SQLITE_TOOBIG)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "sqlite3_bind_blob() failed: too big");
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "sqlite3_bind_blob() failed");
                }
                return OGRERR_FAILURE;
            }
            CreateGeometryExtensionIfNecessary(poGeom);
        }
        /* NULL geometry */
        else
        {
            int err = sqlite3_bind_null(poStmt, nColCount++);
            if (err != SQLITE_OK)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "sqlite3_bind_null() failed");
                return OGRERR_FAILURE;
            }
        }
    }

    /* Bind the attributes using appropriate SQLite data types */
    const int nFieldCount = poFeatureDefn->GetFieldCount();

    size_t nInsertionBufferPos = 0;
    if (m_osInsertionBuffer.empty())
        m_osInsertionBuffer.resize(OGR_SIZEOF_ISO8601_DATETIME_BUFFER *
                                   nFieldCount);

    for (int idx = 0;
         idx < (nUpdatedFieldsCount < 0 ? nFieldCount : nUpdatedFieldsCount);
         idx++)
    {
        const int iField =
            nUpdatedFieldsCount < 0 ? idx : panUpdatedFieldsIdx[idx];
        assert(iField >= 0);
        if (iField == m_iFIDAsRegularColumnIndex ||
            m_abGeneratedColumns[iField])
            continue;
        if (!poFeature->IsFieldSetUnsafe(iField))
        {
            if (bBindUnsetFields)
            {
                int err = sqlite3_bind_null(poStmt, nColCount++);
                if (err != SQLITE_OK)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "sqlite3_bind_null() failed");
                    return OGRERR_FAILURE;
                }
            }
            continue;
        }

        const OGRFieldDefn *poFieldDefn =
            poFeatureDefn->GetFieldDefnUnsafe(iField);
        int err = SQLITE_OK;

        if (!poFeature->IsFieldNullUnsafe(iField))
        {
            const auto eType = poFieldDefn->GetType();
            switch (eType)
            {
                case OFTInteger:
                {
                    err = sqlite3_bind_int(
                        poStmt, nColCount++,
                        poFeature->GetFieldAsIntegerUnsafe(iField));
                    break;
                }
                case OFTInteger64:
                {
                    err = sqlite3_bind_int64(
                        poStmt, nColCount++,
                        poFeature->GetFieldAsInteger64Unsafe(iField));
                    break;
                }
                case OFTReal:
                {
                    err = sqlite3_bind_double(
                        poStmt, nColCount++,
                        poFeature->GetFieldAsDoubleUnsafe(iField));
                    break;
                }
                case OFTBinary:
                {
                    int szBlob = 0;
                    GByte *pabyBlob =
                        poFeature->GetFieldAsBinary(iField, &szBlob);
                    err = sqlite3_bind_blob(poStmt, nColCount++, pabyBlob,
                                            szBlob, SQLITE_STATIC);
                    break;
                }
                default:
                {
                    const char *pszVal = "";
                    int nValLengthBytes = -1;
                    sqlite3_destructor_type destructorType = SQLITE_TRANSIENT;
                    if (eType == OFTDate)
                    {
                        destructorType = SQLITE_STATIC;
                        const auto psFieldRaw =
                            poFeature->GetRawFieldRef(iField);
                        char *pszValEdit =
                            &m_osInsertionBuffer[nInsertionBufferPos];
                        pszVal = pszValEdit;
                        if (psFieldRaw->Date.Year < 0 ||
                            psFieldRaw->Date.Year >= 10000)
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "OGRGetISO8601DateTime(): year %d unsupported ",
                                psFieldRaw->Date.Year);
                            nValLengthBytes = 0;
                        }
                        else
                        {
                            int nYear = psFieldRaw->Date.Year;
                            pszValEdit[3] = (nYear % 10) + '0';
                            nYear /= 10;
                            pszValEdit[2] = (nYear % 10) + '0';
                            nYear /= 10;
                            pszValEdit[1] = (nYear % 10) + '0';
                            nYear /= 10;
                            pszValEdit[0] =
                                static_cast<char>(nYear /*% 10*/ + '0');
                            pszValEdit[4] = '-';
                            pszValEdit[5] =
                                ((psFieldRaw->Date.Month / 10) % 10) + '0';
                            pszValEdit[6] = (psFieldRaw->Date.Month % 10) + '0';
                            pszValEdit[7] = '-';
                            pszValEdit[8] =
                                ((psFieldRaw->Date.Day / 10) % 10) + '0';
                            pszValEdit[9] = (psFieldRaw->Date.Day % 10) + '0';
                            nValLengthBytes = 10;
                            nInsertionBufferPos += 10;
                        }
                    }
                    else if (eType == OFTDateTime)
                    {
                        destructorType = SQLITE_STATIC;
                        const auto psFieldRaw =
                            poFeature->GetRawFieldRef(iField);
                        char *pszValEdit =
                            &m_osInsertionBuffer[nInsertionBufferPos];
                        pszVal = pszValEdit;
                        if (m_poDS->m_bDateTimeWithTZ ||
                            psFieldRaw->Date.TZFlag == 100)
                        {
                            nValLengthBytes = OGRGetISO8601DateTime(
                                psFieldRaw, m_sDateTimeFormat, pszValEdit);
                        }
                        else
                        {
                            OGRField sField(*psFieldRaw);
                            if (sField.Date.TZFlag == 0 ||
                                sField.Date.TZFlag == 1)
                            {
                                sField.Date.TZFlag = 100;
                            }
                            else
                            {
                                struct tm brokendowntime;
                                brokendowntime.tm_year =
                                    sField.Date.Year - 1900;
                                brokendowntime.tm_mon = sField.Date.Month - 1;
                                brokendowntime.tm_mday = sField.Date.Day;
                                brokendowntime.tm_hour = sField.Date.Hour;
                                brokendowntime.tm_min = sField.Date.Minute;
                                brokendowntime.tm_sec = 0;
                                GIntBig nDT =
                                    CPLYMDHMSToUnixTime(&brokendowntime);
                                const int TZOffset =
                                    std::abs(sField.Date.TZFlag - 100) * 15;
                                nDT -= TZOffset * 60;
                                CPLUnixTimeToYMDHMS(nDT, &brokendowntime);
                                sField.Date.Year = static_cast<GInt16>(
                                    brokendowntime.tm_year + 1900);
                                sField.Date.Month = static_cast<GByte>(
                                    brokendowntime.tm_mon + 1);
                                sField.Date.Day =
                                    static_cast<GByte>(brokendowntime.tm_mday);
                                sField.Date.Hour =
                                    static_cast<GByte>(brokendowntime.tm_hour);
                                sField.Date.Minute =
                                    static_cast<GByte>(brokendowntime.tm_min);
                                sField.Date.TZFlag = 100;
                            }

                            nValLengthBytes = OGRGetISO8601DateTime(
                                &sField, m_sDateTimeFormat, pszValEdit);
                        }
                        nInsertionBufferPos += nValLengthBytes;
                    }
                    else if (eType == OFTString)
                    {
                        pszVal = poFeature->GetFieldAsStringUnsafe(iField);
                        if (poFieldDefn->GetWidth() > 0)
                        {
                            if (!CPLIsUTF8(pszVal, -1))
                            {
                                CPLError(CE_Warning, CPLE_AppDefined,
                                         "Value of field '%s' is not a valid "
                                         "UTF-8 string.%s",
                                         poFeatureDefn->GetFieldDefn(iField)
                                             ->GetNameRef(),
                                         m_bTruncateFields
                                             ? " Value will be laundered."
                                             : "");
                                if (m_bTruncateFields)
                                {
                                    pszVal = CPLForceToASCII(pszVal, -1, '_');
                                    destructorType = CPLFree;
                                }
                            }

                            if (CPLStrlenUTF8(pszVal) > poFieldDefn->GetWidth())
                            {
                                CPLError(
                                    CE_Warning, CPLE_AppDefined,
                                    "Value of field '%s' has %d characters, "
                                    "whereas maximum allowed is %d.%s",
                                    poFeatureDefn->GetFieldDefn(iField)
                                        ->GetNameRef(),
                                    CPLStrlenUTF8(pszVal),
                                    poFieldDefn->GetWidth(),
                                    m_bTruncateFields
                                        ? " Value will be truncated."
                                        : "");
                                if (m_bTruncateFields)
                                {
                                    int countUTF8Chars = 0;
                                    nValLengthBytes = 0;
                                    while (pszVal[nValLengthBytes])
                                    {
                                        if ((pszVal[nValLengthBytes] & 0xc0) !=
                                            0x80)
                                        {
                                            // Stop at the start of the
                                            // character just beyond the maximum
                                            // accepted
                                            if (countUTF8Chars ==
                                                poFieldDefn->GetWidth())
                                                break;
                                            countUTF8Chars++;
                                        }
                                        nValLengthBytes++;
                                    }
                                }
                            }
                        }
                        else
                        {
                            destructorType = SQLITE_STATIC;
                        }
                    }
                    else
                    {
                        pszVal = poFeature->GetFieldAsString(iField);
                    }

                    err = sqlite3_bind_text(poStmt, nColCount++, pszVal,
                                            nValLengthBytes, destructorType);
                    break;
                }
            }
        }
        else
        {
            err = sqlite3_bind_null(poStmt, nColCount++);
        }
        if (err != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "sqlite3_bind_() for column %s failed: %s",
                     poFieldDefn->GetNameRef(),
                     sqlite3_errmsg(m_poDS->GetDB()));
            return OGRERR_FAILURE;
        }
    }

    if (pnColCount != nullptr)
        *pnColCount = nColCount;
    return OGRERR_NONE;
}

//----------------------------------------------------------------------
// FeatureBindUpdateParameters()
//
// Selectively bind the values of an OGRFeature to a prepared
// statement, prior to execution. Carefully binds exactly the
// same parameters that have been set up by FeatureGenerateUpdateSQL()
// as bindable.
//
OGRErr
OGRGeoPackageTableLayer::FeatureBindUpdateParameters(OGRFeature *poFeature,
                                                     sqlite3_stmt *poStmt)
{

    int nColCount = 0;
    const OGRErr err = FeatureBindParameters(
        poFeature, poStmt, &nColCount, false, false, -1, nullptr, -1, nullptr);
    if (err != OGRERR_NONE)
        return err;

    // Bind the FID to the "WHERE" clause.
    const int sqlite_err =
        sqlite3_bind_int64(poStmt, nColCount, poFeature->GetFID());
    if (sqlite_err != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "failed to bind FID '" CPL_FRMT_GIB "' to statement",
                 poFeature->GetFID());
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
OGRErr OGRGeoPackageTableLayer::FeatureBindInsertParameters(
    OGRFeature *poFeature, sqlite3_stmt *poStmt, bool bAddFID,
    bool bBindUnsetFields)
{
    return FeatureBindParameters(poFeature, poStmt, nullptr, bAddFID,
                                 bBindUnsetFields, -1, nullptr, -1, nullptr);
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
CPLString OGRGeoPackageTableLayer::FeatureGenerateInsertSQL(
    OGRFeature *poFeature, bool bAddFID, bool bBindUnsetFields, bool bUpsert,
    const std::string &osUpsertUniqueColumnName)
{
    bool bNeedComma = false;
    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    if (poFeatureDefn->GetFieldCount() ==
            ((m_iFIDAsRegularColumnIndex >= 0) ? 1 : 0) &&
        poFeatureDefn->GetGeomFieldCount() == 0 && !bAddFID)
        return CPLSPrintf("INSERT INTO \"%s\" DEFAULT VALUES",
                          SQLEscapeName(m_pszTableName).c_str());

    /* Set up our SQL string basics */
    CPLString osSQLFront("INSERT");
    if (bUpsert && osUpsertUniqueColumnName.empty())
        osSQLFront += " OR REPLACE";
    osSQLFront +=
        CPLSPrintf(" INTO \"%s\" ( ", SQLEscapeName(m_pszTableName).c_str());

    CPLString osSQLBack;
    osSQLBack = ") VALUES (";

    CPLString osSQLColumn;

    if (bAddFID)
    {
        osSQLColumn.Printf("\"%s\"", SQLEscapeName(GetFIDColumn()).c_str());
        osSQLFront += osSQLColumn;
        osSQLBack += "?";
        bNeedComma = true;
    }

    if (poFeatureDefn->GetGeomFieldCount())
    {
        if (bNeedComma)
        {
            osSQLFront += ", ";
            osSQLBack += ", ";
        }

        osSQLColumn.Printf(
            "\"%s\"",
            SQLEscapeName(poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef())
                .c_str());
        osSQLFront += osSQLColumn;
        osSQLBack += "?";
        bNeedComma = true;
    }

    /* Add attribute column names (except FID) to the SQL */
    for (int i = 0; i < poFeatureDefn->GetFieldCount(); i++)
    {
        if (i == m_iFIDAsRegularColumnIndex || m_abGeneratedColumns[i])
            continue;
        if (!bBindUnsetFields && !poFeature->IsFieldSet(i))
            continue;

        if (!bNeedComma)
        {
            bNeedComma = true;
        }
        else
        {
            osSQLFront += ", ";
            osSQLBack += ", ";
        }

        osSQLColumn.Printf(
            "\"%s\"",
            SQLEscapeName(poFeatureDefn->GetFieldDefn(i)->GetNameRef())
                .c_str());
        osSQLFront += osSQLColumn;
        osSQLBack += "?";
    }

    osSQLBack += ")";

    if (!bNeedComma)
        return CPLSPrintf("INSERT INTO \"%s\" DEFAULT VALUES",
                          SQLEscapeName(m_pszTableName).c_str());

    if (bUpsert && !osUpsertUniqueColumnName.empty())
    {
        osSQLBack += " ON CONFLICT ";
#if SQLITE_VERSION_NUMBER < 3035000L
        osSQLBack += "(\"";
        osSQLBack += SQLEscapeName(osUpsertUniqueColumnName.c_str());
        osSQLBack += "\") ";
#endif
        osSQLBack += "DO UPDATE SET ";
        bNeedComma = false;
        if (poFeatureDefn->GetGeomFieldCount())
        {
            osSQLBack += CPLSPrintf(
                "\"%s\" = excluded.\"%s\"",
                SQLEscapeName(poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef())
                    .c_str(),
                SQLEscapeName(poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef())
                    .c_str());
            bNeedComma = true;
        }
        for (int i = 0; i < poFeatureDefn->GetFieldCount(); i++)
        {
            if (i == m_iFIDAsRegularColumnIndex)
                continue;
            if (!bBindUnsetFields && !poFeature->IsFieldSet(i))
                continue;

            if (!bNeedComma)
            {
                bNeedComma = true;
            }
            else
            {
                osSQLBack += ", ";
            }

            osSQLBack += CPLSPrintf(
                "\"%s\" = excluded.\"%s\"",
                SQLEscapeName(poFeatureDefn->GetFieldDefn(i)->GetNameRef())
                    .c_str(),
                SQLEscapeName(poFeatureDefn->GetFieldDefn(i)->GetNameRef())
                    .c_str());
        }
#if SQLITE_VERSION_NUMBER >= 3035000L
        osSQLBack += " RETURNING \"";
        osSQLBack += SQLEscapeName(GetFIDColumn()).c_str();
        osSQLBack += "\"";
#endif
    }

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
std::string OGRGeoPackageTableLayer::FeatureGenerateUpdateSQL(
    const OGRFeature *poFeature) const
{
    bool bNeedComma = false;
    const OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    /* Set up our SQL string basics */
    std::string osUpdate("UPDATE \"");
    osUpdate += SQLEscapeName(m_pszTableName);
    osUpdate += "\" SET ";

    if (poFeatureDefn->GetGeomFieldCount() > 0)
    {
        osUpdate += '"';
        osUpdate +=
            SQLEscapeName(poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef());
        osUpdate += "\"=?";
        bNeedComma = true;
    }

    /* Add attribute column names (except FID) to the SQL */
    const int nFieldCount = poFeatureDefn->GetFieldCount();
    for (int i = 0; i < nFieldCount; i++)
    {
        if (i == m_iFIDAsRegularColumnIndex || m_abGeneratedColumns[i])
            continue;
        if (!poFeature->IsFieldSet(i))
            continue;
        if (!bNeedComma)
            bNeedComma = true;
        else
            osUpdate += ", ";

        osUpdate += '"';
        osUpdate += SQLEscapeName(poFeatureDefn->GetFieldDefn(i)->GetNameRef());
        osUpdate += "\"=?";
    }
    if (!bNeedComma)
        return CPLString();

    osUpdate += " WHERE \"";
    osUpdate += SQLEscapeName(m_pszFidColumn);
    osUpdate += "\" = ?";

    return osUpdate;
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn *OGRGeoPackageTableLayer::GetLayerDefn()
{
    if (!m_bFeatureDefnCompleted)
    {
        m_bFeatureDefnCompleted = true;
        ReadTableDefinition();
        m_poFeatureDefn->Seal(/* bSealFields = */ true);
    }
    return m_poFeatureDefn;
}

/************************************************************************/
/*                      GetFIDColumn()                                  */
/************************************************************************/

const char *OGRGeoPackageTableLayer::GetFIDColumn()
{
    if (!m_bFeatureDefnCompleted)
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
    if (m_poFeatureDefn->GetGeomFieldCount() > 0)
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
    m_poDS->IncrementReadTableDefCounter();

    bool bReadExtent = false;
    sqlite3 *poDb = m_poDS->GetDB();
    OGREnvelope oExtent;
    CPLString osGeomColumnName;
    CPLString osGeomColsType;
    bool bHasZ = false;
    bool bHasM = false;

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (m_poDS->m_bHasGPKGOGRContents)
    {
        CPLString osTrigger1Name(
            CPLSPrintf("trigger_insert_feature_count_%s", m_pszTableName));
        CPLString osTrigger2Name(
            CPLSPrintf("trigger_delete_feature_count_%s", m_pszTableName));
        const std::map<CPLString, CPLString> &oMap =
            m_poDS->GetNameTypeMapFromSQliteMaster();
        if (oMap.find(osTrigger1Name.toupper()) != oMap.end() &&
            oMap.find(osTrigger2Name.toupper()) != oMap.end())
        {
            m_bOGRFeatureCountTriggersEnabled = true;
        }
        else if (m_bIsTable)
        {
            CPLDebug("GPKG",
                     "Insert/delete feature_count triggers "
                     "missing on %s",
                     m_pszTableName);
        }
    }
#endif

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (m_poDS->m_bHasGPKGOGRContents)
    {
        char *pszSQL = sqlite3_mprintf("SELECT feature_count "
                                       "FROM gpkg_ogr_contents "
                                       "WHERE table_name = '%q'"
#ifdef WORKAROUND_SQLITE3_BUGS
                                       " OR 0"
#endif
                                       " LIMIT 2",
                                       m_pszTableName);
        auto oResultFeatureCount = SQLQuery(poDb, pszSQL);
        sqlite3_free(pszSQL);
        if (oResultFeatureCount && oResultFeatureCount->RowCount() == 0)
        {
            pszSQL = sqlite3_mprintf("SELECT feature_count "
                                     "FROM gpkg_ogr_contents "
                                     "WHERE lower(table_name) = lower('%q')"
#ifdef WORKAROUND_SQLITE3_BUGS
                                     " OR 0"
#endif
                                     " LIMIT 2",
                                     m_pszTableName);
            oResultFeatureCount = SQLQuery(poDb, pszSQL);
            sqlite3_free(pszSQL);
        }

        if (oResultFeatureCount && oResultFeatureCount->RowCount() == 1)
        {
            const char *pszFeatureCount = oResultFeatureCount->GetValue(0, 0);
            if (pszFeatureCount)
            {
                m_nTotalFeatureCount = CPLAtoGIntBig(pszFeatureCount);
            }
        }
    }
#endif

    bool bHasPreexistingSingleGeomColumn =
        m_poFeatureDefn->GetGeomFieldCount() == 1;
    bool bHasMultipleGeomColsInGpkgGeometryColumns = false;

    if (m_bIsInGpkgContents)
    {
        /* Check that the table name is registered in gpkg_contents */
        const std::map<CPLString, GPKGContentsDesc> &oMapContents =
            m_poDS->GetContents();
        std::map<CPLString, GPKGContentsDesc>::const_iterator oIterContents =
            oMapContents.find(CPLString(m_pszTableName).toupper());
        if (oIterContents == oMapContents.end())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "layer '%s' is not registered in gpkg_contents",
                     m_pszTableName);
            return OGRERR_FAILURE;
        }

        const GPKGContentsDesc &oContents = oIterContents->second;

        const char *pszIdentifier = oContents.osIdentifier.c_str();
        if (pszIdentifier[0] != 0 && strcmp(pszIdentifier, m_pszTableName) != 0)
            OGRLayer::SetMetadataItem("IDENTIFIER", pszIdentifier);
        const char *pszDescription = oContents.osDescription.c_str();
        if (pszDescription[0])
            OGRLayer::SetMetadataItem("DESCRIPTION", pszDescription);

        if (m_bIsSpatial)
        {
            /* All the extrema have to be non-NULL for this to make sense */
            if (!oContents.osMinX.empty() && !oContents.osMinY.empty() &&
                !oContents.osMaxX.empty() && !oContents.osMaxY.empty())
            {
                oExtent.MinX = CPLAtof(oContents.osMinX);
                oExtent.MinY = CPLAtof(oContents.osMinY);
                oExtent.MaxX = CPLAtof(oContents.osMaxX);
                oExtent.MaxY = CPLAtof(oContents.osMaxY);
                bReadExtent = oExtent.MinX <= oExtent.MaxX &&
                              oExtent.MinY <= oExtent.MaxY;
            }

            /* Check that the table name is registered in gpkg_geometry_columns
             */
            char *pszSQL = sqlite3_mprintf("SELECT table_name, column_name, "
                                           "geometry_type_name, srs_id, z, m "
                                           "FROM gpkg_geometry_columns "
                                           "WHERE table_name = '%q'"
#ifdef WORKAROUND_SQLITE3_BUGS
                                           " OR 0"
#endif
                                           " LIMIT 2000",
                                           m_pszTableName);

            auto oResultGeomCols = SQLQuery(poDb, pszSQL);
            sqlite3_free(pszSQL);
            if (oResultGeomCols && oResultGeomCols->RowCount() == 0)
            {
                pszSQL = sqlite3_mprintf("SELECT table_name, column_name, "
                                         "geometry_type_name, srs_id, z, m "
                                         "FROM gpkg_geometry_columns "
                                         "WHERE lower(table_name) = lower('%q')"
#ifdef WORKAROUND_SQLITE3_BUGS
                                         " OR 0"
#endif
                                         " LIMIT 2000",
                                         m_pszTableName);

                oResultGeomCols = SQLQuery(poDb, pszSQL);
                sqlite3_free(pszSQL);
            }

            /* gpkg_geometry_columns query has to work */
            if (!(oResultGeomCols && oResultGeomCols->RowCount() > 0))
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "layer '%s' is not registered in gpkg_geometry_columns",
                    m_pszTableName);
            }
            else
            {
                int iRow = -1;
                bHasMultipleGeomColsInGpkgGeometryColumns =
                    oResultGeomCols->RowCount() > 1;
                for (int i = 0; i < oResultGeomCols->RowCount(); ++i)
                {
                    const char *pszGeomColName =
                        oResultGeomCols->GetValue(1, i);
                    if (!pszGeomColName)
                        continue;
                    if (!bHasPreexistingSingleGeomColumn ||
                        strcmp(pszGeomColName,
                               m_poFeatureDefn->GetGeomFieldDefn(0)
                                   ->GetNameRef()) == 0)
                    {
                        iRow = i;
                        break;
                    }
                }

                if (iRow >= 0)
                {
                    const char *pszGeomColName =
                        oResultGeomCols->GetValue(1, iRow);
                    if (pszGeomColName != nullptr)
                        osGeomColumnName = pszGeomColName;
                    const char *pszGeomColsType =
                        oResultGeomCols->GetValue(2, iRow);
                    if (pszGeomColsType != nullptr)
                        osGeomColsType = pszGeomColsType;
                    m_iSrs = oResultGeomCols->GetValueAsInteger(3, iRow);
                    m_nZFlag = oResultGeomCols->GetValueAsInteger(4, iRow);
                    m_nMFlag = oResultGeomCols->GetValueAsInteger(5, iRow);
                    if (!(EQUAL(osGeomColsType, "GEOMETRY") && m_nZFlag == 2))
                    {
                        bHasZ = CPL_TO_BOOL(m_nZFlag);
                        bHasM = CPL_TO_BOOL(m_nMFlag);
                    }
                }
                else
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Cannot find record for layer '%s' and geometry column "
                        "'%s' in gpkg_geometry_columns",
                        m_pszTableName,
                        bHasPreexistingSingleGeomColumn
                            ? m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef()
                            : "unknown");
                }
            }
        }
    }

    // set names (in upper case) of fields with unique constraint
    std::set<std::string> uniqueFieldsUC;
    if (m_bIsTable)
    {
        // If resolving the layer definition of a substantial number of tables,
        // fetch in a single time the content of the sqlite_master to increase
        // performance
        // Threshold somewhat arbitrary. If changing it, change
        // ogr_gpkg.py::test_ogr_gpkg_unique_many_layers as well
        constexpr int THRESHOLD_GET_SQLITE_MASTER = 10;
        if (m_poDS->GetReadTableDefCounter() >= THRESHOLD_GET_SQLITE_MASTER)
        {
            uniqueFieldsUC = SQLGetUniqueFieldUCConstraints(
                poDb, m_pszTableName, m_poDS->GetSqliteMasterContent());
        }
        else
        {
            uniqueFieldsUC =
                SQLGetUniqueFieldUCConstraints(poDb, m_pszTableName);
        }
    }

    /* Use the "PRAGMA TABLE_INFO()" call to get table definition */
    /*  #|name|type|notnull|default|pk */
    /*  0|id|integer|0||1 */
    /*  1|name|varchar|0||0 */
    char *pszSQL = sqlite3_mprintf("pragma table_xinfo('%q')", m_pszTableName);
    auto oResultTable = SQLQuery(poDb, pszSQL);
    sqlite3_free(pszSQL);

    if (!oResultTable || oResultTable->RowCount() == 0)
    {
        if (oResultTable)
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find table %s",
                     m_pszTableName);
        return OGRERR_FAILURE;
    }

    /* Populate feature definition from table description */

    // First pass to determine if we have a single PKID column
    int nCountPKIDColumns = 0;
    for (int iRecord = 0; iRecord < oResultTable->RowCount(); iRecord++)
    {
        int nPKIDIndex = oResultTable->GetValueAsInteger(5, iRecord);
        if (nPKIDIndex > 0)
            nCountPKIDColumns++;
    }
    if (nCountPKIDColumns > 1)
    {
        CPLDebug("GPKG",
                 "For table %s, multiple columns make "
                 "the primary key. Ignoring them",
                 m_pszTableName);
    }

    m_abGeneratedColumns.resize(oResultTable->RowCount());
    for (int iRecord = 0; iRecord < oResultTable->RowCount(); iRecord++)
    {
        const char *pszName = oResultTable->GetValue(1, iRecord);
        std::string osType = oResultTable->GetValue(2, iRecord);
        int bNotNull = oResultTable->GetValueAsInteger(3, iRecord);
        const char *pszDefault = oResultTable->GetValue(4, iRecord);
        int nPKIDIndex = oResultTable->GetValueAsInteger(5, iRecord);
        int nHiddenValue = oResultTable->GetValueAsInteger(6, iRecord);

        OGRFieldSubType eSubType = OFSTNone;
        int nMaxWidth = 0;
        int nType = OFTMaxType + 1;

        // SQLite 3.31 has a " GENERATED ALWAYS" suffix in the type column,
        // but more recent versions no longer have it.
        bool bIsGenerated = false;
        constexpr const char *GENERATED_ALWAYS_SUFFIX = " GENERATED ALWAYS";
        if (osType.size() > strlen(GENERATED_ALWAYS_SUFFIX) &&
            CPLString(osType).toupper().compare(
                osType.size() - strlen(GENERATED_ALWAYS_SUFFIX),
                strlen(GENERATED_ALWAYS_SUFFIX), GENERATED_ALWAYS_SUFFIX) == 0)
        {
            bIsGenerated = true;
            osType.resize(osType.size() - strlen(GENERATED_ALWAYS_SUFFIX));
        }
        constexpr int GENERATED_VIRTUAL = 2;
        constexpr int GENERATED_STORED = 3;
        if (nHiddenValue == GENERATED_VIRTUAL ||
            nHiddenValue == GENERATED_STORED)
        {
            bIsGenerated = true;
        }

        if (!osType.empty() || m_bIsTable)
        {
            nType = GPkgFieldToOGR(osType.c_str(), eSubType, nMaxWidth);
        }
        else
        {
            // For a view, if the geometry column is computed, we don't
            // get a type, so trust the one from gpkg_geometry_columns
            if (EQUAL(osGeomColumnName, pszName))
            {
                osType = osGeomColsType;
            }
        }

        /* Not a standard field type... */
        if (!osType.empty() && !EQUAL(pszName, "OGC_FID") &&
            ((nType > OFTMaxType && !osGeomColsType.empty()) ||
             EQUAL(osGeomColumnName, pszName)))
        {
            /* Maybe it is a geometry type? */
            OGRwkbGeometryType oGeomType;
            if (nType > OFTMaxType)
                oGeomType = GPkgGeometryTypeToWKB(osType.c_str(), bHasZ, bHasM);
            else
                oGeomType = wkbUnknown;
            if (oGeomType != wkbNone)
            {
                if ((bHasPreexistingSingleGeomColumn &&
                     (!bHasMultipleGeomColsInGpkgGeometryColumns ||
                      strcmp(pszName, m_poFeatureDefn->GetGeomFieldDefn(0)
                                          ->GetNameRef()) == 0)) ||
                    m_poFeatureDefn->GetGeomFieldCount() == 0)
                {
                    OGRwkbGeometryType oGeomTypeGeomCols =
                        GPkgGeometryTypeToWKB(osGeomColsType.c_str(), bHasZ,
                                              bHasM);
                    /* Enforce consistency between table and metadata */
                    if (wkbFlatten(oGeomType) == wkbUnknown)
                        oGeomType = oGeomTypeGeomCols;
                    if (oGeomType != oGeomTypeGeomCols)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "geometry column type for layer '%s' in "
                                 "'%s.%s' (%s) is not "
                                 "consistent with type in "
                                 "gpkg_geometry_columns (%s)",
                                 GetName(), m_pszTableName, pszName,
                                 osType.c_str(), osGeomColsType.c_str());
                    }

                    if (!bHasPreexistingSingleGeomColumn)
                    {
                        OGRGeomFieldDefn oGeomField(pszName, oGeomType);
                        m_poFeatureDefn->AddGeomFieldDefn(&oGeomField);
                    }
                    bHasPreexistingSingleGeomColumn = false;
                    if (bNotNull)
                        m_poFeatureDefn->GetGeomFieldDefn(0)->SetNullable(
                            FALSE);

                    /* Read the SRS */
                    OGRSpatialReference *poSRS = m_poDS->GetSpatialRef(m_iSrs);
                    if (poSRS)
                    {
                        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(
                            poSRS);
                        poSRS->Dereference();
                    }
                }
                else if (!STARTS_WITH(
                             GetName(),
                             (std::string(m_pszTableName) + " (").c_str()))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "table '%s' has multiple geometry fields. "
                             "Ignoring field '%s' for this layer",
                             m_pszTableName, pszName);
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "geometry column '%s' of type '%s' ignored", pszName,
                         osType.c_str());
            }
        }
        else
        {
            if (nType > OFTMaxType)
            {
                CPLDebug("GPKG",
                         "For table %s, unrecognized type name %s for "
                         "column %s. Using string type",
                         m_pszTableName, osType.c_str(), pszName);
                nType = OFTString;
            }

            /* Is this the FID column? */
            if (nPKIDIndex > 0 && nCountPKIDColumns == 1 &&
                (nType == OFTInteger || nType == OFTInteger64))
            {
                m_pszFidColumn = CPLStrdup(pszName);
            }
            else
            {
                OGRFieldDefn oField(pszName, static_cast<OGRFieldType>(nType));
                oField.SetSubType(eSubType);
                oField.SetWidth(nMaxWidth);
                if (bNotNull)
                    oField.SetNullable(FALSE);

                if (uniqueFieldsUC.find(CPLString(pszName).toupper()) !=
                    uniqueFieldsUC.end())
                {
                    oField.SetUnique(TRUE);
                }

                if (pszDefault != nullptr)
                {
                    int nYear = 0;
                    int nMonth = 0;
                    int nDay = 0;
                    int nHour = 0;
                    int nMinute = 0;
                    float fSecond = 0.0f;
                    if (oField.GetType() == OFTString &&
                        !EQUAL(pszDefault, "NULL") &&
                        !STARTS_WITH_CI(pszDefault, "CURRENT_") &&
                        pszDefault[0] != '(' && pszDefault[0] != '\'' &&
                        CPLGetValueType(pszDefault) == CPL_VALUE_STRING)
                    {
                        CPLString osDefault("'");
                        char *pszTmp =
                            CPLEscapeString(pszDefault, -1, CPLES_SQL);
                        osDefault += pszTmp;
                        CPLFree(pszTmp);
                        osDefault += "'";
                        oField.SetDefault(osDefault);
                    }
                    else if (nType == OFTDateTime &&
                             sscanf(pszDefault, "'%d-%d-%dT%d:%d:%fZ'", &nYear,
                                    &nMonth, &nDay, &nHour, &nMinute,
                                    &fSecond) == 6)
                    {
                        if (strchr(pszDefault, '.') == nullptr)
                            oField.SetDefault(
                                CPLSPrintf("'%04d/%02d/%02d %02d:%02d:%02d'",
                                           nYear, nMonth, nDay, nHour, nMinute,
                                           static_cast<int>(fSecond + 0.5)));
                        else
                            oField.SetDefault(CPLSPrintf(
                                "'%04d/%02d/%02d %02d:%02d:%06.3f'", nYear,
                                nMonth, nDay, nHour, nMinute, fSecond));
                    }
                    else if ((oField.GetType() == OFTDate ||
                              oField.GetType() == OFTDateTime) &&
                             !EQUAL(pszDefault, "NULL") &&
                             !STARTS_WITH_CI(pszDefault, "CURRENT_") &&
                             pszDefault[0] != '(' && pszDefault[0] != '\'' &&
                             !(pszDefault[0] >= '0' && pszDefault[0] <= '9') &&
                             CPLGetValueType(pszDefault) == CPL_VALUE_STRING)
                    {
                        CPLString osDefault("(");
                        osDefault += pszDefault;
                        osDefault += ")";
                        if (EQUAL(osDefault,
                                  "(strftime('%Y-%m-%dT%H:%M:%fZ','now'))"))
                            oField.SetDefault("CURRENT_TIMESTAMP");
                        else
                            oField.SetDefault(osDefault);
                    }
                    else
                    {
                        oField.SetDefault(pszDefault);
                    }
                }
                m_abGeneratedColumns[m_poFeatureDefn->GetFieldCount()] =
                    bIsGenerated;
                m_poFeatureDefn->AddFieldDefn(&oField);
            }
        }
    }

    m_abGeneratedColumns.resize(m_poFeatureDefn->GetFieldCount());

    /* Wait, we didn't find a FID? Some operations will not be possible */
    if (m_bIsTable && m_pszFidColumn == nullptr)
    {
        CPLDebug("GPKG", "no integer primary key defined for table '%s'",
                 m_pszTableName);
    }

    if (bReadExtent)
    {
        m_poExtent = new OGREnvelope(oExtent);
    }

    // Look for sub-types such as JSON
    if (m_poDS->HasDataColumnsTable())
    {
        pszSQL = sqlite3_mprintf(
            "SELECT column_name, name, mime_type, "
            "constraint_name, description FROM gpkg_data_columns "
            "WHERE table_name = '%q'",
            m_pszTableName);
        oResultTable = SQLQuery(poDb, pszSQL);
        sqlite3_free(pszSQL);
        if (oResultTable)
        {
            for (int iRecord = 0; iRecord < oResultTable->RowCount(); iRecord++)
            {
                const char *pszColumn = oResultTable->GetValue(0, iRecord);
                if (pszColumn == nullptr)
                    continue;
                const char *pszName = oResultTable->GetValue(1, iRecord);

                // We use the "name" attribute from gpkg_data_columns as the
                // field alternative name, so long as it isn't just a copy
                // of the column name
                const char *pszAlias = nullptr;
                if (pszName && !EQUAL(pszName, pszColumn))
                    pszAlias = pszName;

                if (pszAlias)
                {
                    const int iIdx = m_poFeatureDefn->GetFieldIndex(pszColumn);
                    if (iIdx >= 0)
                    {
                        m_poFeatureDefn->GetFieldDefn(iIdx)->SetAlternativeName(
                            pszAlias);
                    }
                }

                if (const char *pszDescription =
                        oResultTable->GetValue(4, iRecord))
                {
                    const int iIdx = m_poFeatureDefn->GetFieldIndex(pszColumn);
                    if (iIdx >= 0)
                    {
                        m_poFeatureDefn->GetFieldDefn(iIdx)->SetComment(
                            pszDescription);
                    }
                }

                const char *pszMimeType = oResultTable->GetValue(2, iRecord);
                const char *pszConstraintName =
                    oResultTable->GetValue(3, iRecord);
                if (pszMimeType && EQUAL(pszMimeType, "application/json"))
                {
                    const int iIdx = m_poFeatureDefn->GetFieldIndex(pszColumn);
                    if (iIdx >= 0 &&
                        m_poFeatureDefn->GetFieldDefn(iIdx)->GetType() ==
                            OFTString)
                    {
                        m_poFeatureDefn->GetFieldDefn(iIdx)->SetSubType(
                            OFSTJSON);
                    }
                }
                else if (pszConstraintName)
                {
                    const int iIdx = m_poFeatureDefn->GetFieldIndex(pszColumn);
                    if (iIdx >= 0)
                    {
                        m_poFeatureDefn->GetFieldDefn(iIdx)->SetDomainName(
                            pszConstraintName);
                    }
                }
            }
        }
    }

    // Look for geometry column coordinate precision in gpkg_metadata
    if (m_poDS->HasMetadataTables() && m_poFeatureDefn->GetGeomFieldCount() > 0)
    {
        pszSQL = sqlite3_mprintf(
            "SELECT md.metadata, mdr.column_name "
            "FROM gpkg_metadata md "
            "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id) "
            "WHERE lower(mdr.table_name) = lower('%q') "
            "AND md.md_standard_uri = 'http://gdal.org' "
            "AND md.mime_type = 'text/xml' "
            "AND mdr.reference_scope = 'column' "
            "AND md.metadata LIKE '<CoordinatePrecision%%' "
            "ORDER BY md.id LIMIT 1000",  // to avoid denial of service
            m_pszTableName);

        auto oResult = SQLQuery(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);

        for (int i = 0; oResult && i < oResult->RowCount(); i++)
        {
            const char *pszMetadata = oResult->GetValue(0, i);
            const char *pszColumn = oResult->GetValue(1, i);
            if (pszMetadata && pszColumn)
            {
                const int iGeomCol =
                    m_poFeatureDefn->GetGeomFieldIndex(pszColumn);
                if (iGeomCol >= 0)
                {
                    auto psXMLNode =
                        CPLXMLTreeCloser(CPLParseXMLString(pszMetadata));
                    if (psXMLNode)
                    {
                        OGRGeomCoordinatePrecision sCoordPrec;
                        if (const char *pszVal = CPLGetXMLValue(
                                psXMLNode.get(), "xy_resolution", nullptr))
                        {
                            sCoordPrec.dfXYResolution = CPLAtof(pszVal);
                        }
                        if (const char *pszVal = CPLGetXMLValue(
                                psXMLNode.get(), "z_resolution", nullptr))
                        {
                            sCoordPrec.dfZResolution = CPLAtof(pszVal);
                        }
                        if (const char *pszVal = CPLGetXMLValue(
                                psXMLNode.get(), "m_resolution", nullptr))
                        {
                            sCoordPrec.dfMResolution = CPLAtof(pszVal);
                        }
                        m_poFeatureDefn->GetGeomFieldDefn(iGeomCol)
                            ->SetCoordinatePrecision(sCoordPrec);
                        if (CPLTestBool(CPLGetXMLValue(
                                psXMLNode.get(), "discard_coord_lsb", "false")))
                        {
                            m_sBinaryPrecision.SetFrom(sCoordPrec);
                            m_bUndoDiscardCoordLSBOnReading =
                                CPLTestBool(CPLGetXMLValue(
                                    psXMLNode.get(),
                                    "undo_discard_coord_lsb_on_reading",
                                    "false"));
                        }
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

OGRGeoPackageTableLayer::OGRGeoPackageTableLayer(GDALGeoPackageDataset *poDS,
                                                 const char *pszTableName)
    : OGRGeoPackageLayer(poDS), m_pszTableName(CPLStrdup(pszTableName))
{
    memset(m_abHasGeometryExtension, 0, sizeof(m_abHasGeometryExtension));

    m_poFeatureDefn = new OGRFeatureDefn(m_pszTableName);
    SetDescription(m_poFeatureDefn->GetName());
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
    if (m_pszTableName)
        CPLFree(m_pszTableName);

    if (m_poExtent)
        delete m_poExtent;

    if (m_poUpdateStatement)
        sqlite3_finalize(m_poUpdateStatement);

    if (m_poInsertStatement)
        sqlite3_finalize(m_poInsertStatement);

    if (m_poGetFeatureStatement)
        sqlite3_finalize(m_poGetFeatureStatement);

    CancelAsyncNextArrowArray();
}

/************************************************************************/
/*                 CancelAsyncNextArrowArray()                          */
/************************************************************************/

void OGRGeoPackageTableLayer::CancelAsyncNextArrowArray()
{
    if (m_poFillArrowArray)
    {
        std::lock_guard<std::mutex> oLock(m_poFillArrowArray->oMutex);
        m_poFillArrowArray->nCountRows = -1;
        m_poFillArrowArray->oCV.notify_one();
    }

    if (m_oThreadNextArrowArray.joinable())
    {
        m_oThreadNextArrowArray.join();
    }

    m_poFillArrowArray.reset();

    while (!m_oQueueArrowArrayPrefetchTasks.empty())
    {
        auto task = std::move(m_oQueueArrowArrayPrefetchTasks.front());
        m_oQueueArrowArrayPrefetchTasks.pop();

        {
            std::lock_guard<std::mutex> oLock(task->m_oMutex);
            task->m_bStop = true;
            task->m_oCV.notify_one();
        }
        if (task->m_oThread.joinable())
            task->m_oThread.join();

        if (task->m_psArrowArray)
        {
            if (task->m_psArrowArray->release)
                task->m_psArrowArray->release(task->m_psArrowArray.get());
        }
    }
}

/************************************************************************/
/*                        InitView()                                    */
/************************************************************************/

void OGRGeoPackageTableLayer::InitView()
{
#ifdef SQLITE_HAS_COLUMN_METADATA
    if (!m_bIsTable)
    {
        /* Detect if the view columns have the FID and geom columns of a */
        /* table that has itself a spatial index */
        sqlite3_stmt *hStmt = nullptr;
        char *pszSQL = sqlite3_mprintf("SELECT * FROM \"%w\"", m_pszTableName);
        CPL_IGNORE_RET_VAL(
            sqlite3_prepare_v2(m_poDS->GetDB(), pszSQL, -1, &hStmt, nullptr));
        sqlite3_free(pszSQL);
        if (hStmt)
        {
            if (sqlite3_step(hStmt) == SQLITE_ROW)
            {
                OGRGeoPackageTableLayer *poLayerGeom = nullptr;
                const int nRawColumns = sqlite3_column_count(hStmt);
                for (int iCol = 0; iCol < nRawColumns; iCol++)
                {
                    CPLString osColName(
                        SQLUnescape(sqlite3_column_name(hStmt, iCol)));
                    const char *pszTableName =
                        sqlite3_column_table_name(hStmt, iCol);
                    const char *pszOriginName =
                        sqlite3_column_origin_name(hStmt, iCol);
                    if (EQUAL(osColName, "OGC_FID") &&
                        (pszOriginName == nullptr ||
                         osColName != pszOriginName))
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
                    else if (iCol == 0 &&
                             sqlite3_column_type(hStmt, iCol) == SQLITE_INTEGER)
                    {
                        // Assume the first column of integer type is the FID
                        // column per the latest requirements of the GPKG spec
                        CPLFree(m_pszFidColumn);
                        m_pszFidColumn = CPLStrdup(osColName);
                        m_poFeatureDefn->DeleteFieldDefn(
                            m_poFeatureDefn->GetFieldIndex(osColName));
                    }
                    else if (pszTableName != nullptr &&
                             pszOriginName != nullptr)
                    {
                        OGRGeoPackageTableLayer *poLayer =
                            dynamic_cast<OGRGeoPackageTableLayer *>(
                                m_poDS->GetLayerByName(pszTableName));
                        if (poLayer != nullptr &&
                            osColName == GetGeometryColumn() &&
                            strcmp(pszOriginName,
                                   poLayer->GetGeometryColumn()) == 0)
                        {
                            poLayerGeom = poLayer;
                        }
                    }
                }

                if (poLayerGeom != nullptr && poLayerGeom->HasSpatialIndex())
                {
                    for (int iCol = 0; iCol < nRawColumns; iCol++)
                    {
                        const std::string osColName(
                            SQLUnescape(sqlite3_column_name(hStmt, iCol)));
                        const char *pszTableName =
                            sqlite3_column_table_name(hStmt, iCol);
                        const char *pszOriginName =
                            sqlite3_column_origin_name(hStmt, iCol);
                        if (pszTableName != nullptr && pszOriginName != nullptr)
                        {
                            OGRGeoPackageTableLayer *poLayer =
                                dynamic_cast<OGRGeoPackageTableLayer *>(
                                    m_poDS->GetLayerByName(pszTableName));
                            if (poLayer != nullptr && poLayer == poLayerGeom &&
                                strcmp(pszOriginName,
                                       poLayer->GetFIDColumn()) == 0)
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

bool OGRGeoPackageTableLayer::CheckUpdatableTable(const char *pszOperation)
{
    if (!m_poDS->GetUpdate())
    {
        CPLError(CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
                 pszOperation);
        return false;
    }
    /* -------------------------------------------------------------------- */
    /*      Check that is a table and not a view                            */
    /* -------------------------------------------------------------------- */
    if (!m_bIsTable)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Layer %s is not a table",
                 m_pszTableName);
        return false;
    }
    return true;
}

/************************************************************************/
/*                      CreateField()                                   */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::CreateField(const OGRFieldDefn *poField,
                                            int /* bApproxOK */)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (!CheckUpdatableTable("CreateField"))
        return OGRERR_FAILURE;

    OGRFieldDefn oFieldDefn(poField);
    int nMaxWidth = 0;
    if (m_bPreservePrecision && poField->GetType() == OFTString)
        nMaxWidth = poField->GetWidth();
    else
        oFieldDefn.SetWidth(0);
    oFieldDefn.SetPrecision(0);

    if (m_bLaunder)
        oFieldDefn.SetName(
            GDALGeoPackageDataset::LaunderName(oFieldDefn.GetNameRef())
                .c_str());

    if (m_pszFidColumn != nullptr &&
        EQUAL(oFieldDefn.GetNameRef(), m_pszFidColumn) &&
        poField->GetType() != OFTInteger &&
        poField->GetType() != OFTInteger64 &&
        // typically a GeoPackage exported with QGIS as a shapefile and
        // re-imported See https://github.com/qgis/QGIS/pull/43118
        !(poField->GetType() == OFTReal && poField->GetWidth() == 20 &&
          poField->GetPrecision() == 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Wrong field type for %s",
                 oFieldDefn.GetNameRef());
        return OGRERR_FAILURE;
    }

    if (!m_bDeferredCreation)
    {
        CPLString osCommand;

        // ADD COLUMN has several restrictions
        // See https://www.sqlite.org/lang_altertable.html#altertabaddcol

        osCommand.Printf("ALTER TABLE \"%s\" ADD COLUMN \"%s\" %s",
                         SQLEscapeName(m_pszTableName).c_str(),
                         SQLEscapeName(oFieldDefn.GetNameRef()).c_str(),
                         GPkgFieldFromOGR(poField->GetType(),
                                          poField->GetSubType(), nMaxWidth));
        if (!poField->IsNullable())
            osCommand += " NOT NULL";
        if (poField->IsUnique())
        {
            // this will fail when SQLCommand() is run, as it is not allowed
            // by SQLite. This is a bit of an artificial restriction.
            // We could override it by rewriting the table.
            osCommand += " UNIQUE";
        }
        if (poField->GetDefault() != nullptr &&
            !poField->IsDefaultDriverSpecific())
        {
            osCommand += " DEFAULT ";
            int nYear = 0;
            int nMonth = 0;
            int nDay = 0;
            int nHour = 0;
            int nMinute = 0;
            float fSecond = 0.0f;
            if (poField->GetType() == OFTDateTime &&
                sscanf(poField->GetDefault(), "'%d/%d/%d %d:%d:%f'", &nYear,
                       &nMonth, &nDay, &nHour, &nMinute, &fSecond) == 6)
            {
                if (strchr(poField->GetDefault(), '.') == nullptr)
                    osCommand += CPLSPrintf("'%04d-%02d-%02dT%02d:%02d:%02dZ'",
                                            nYear, nMonth, nDay, nHour, nMinute,
                                            static_cast<int>(fSecond + 0.5));
                else
                    osCommand +=
                        CPLSPrintf("'%04d-%02d-%02dT%02d:%02d:%06.3fZ'", nYear,
                                   nMonth, nDay, nHour, nMinute, fSecond);
            }
            else
            {
                // This could fail if it is CURRENT_TIMESTAMP, etc.
                osCommand += poField->GetDefault();
            }
        }
        else if (!poField->IsNullable())
        {
            // SQLite mandates a DEFAULT value when adding a NOT NULL column in
            // an ALTER TABLE ADD COLUMN.
            osCommand += " DEFAULT ''";
        }

        OGRErr err = SQLCommand(m_poDS->GetDB(), osCommand.c_str());

        if (err != OGRERR_NONE)
            return err;

        if (!DoSpecialProcessingForColumnCreation(poField))
        {
            return OGRERR_FAILURE;
        }
    }

    whileUnsealing(m_poFeatureDefn)->AddFieldDefn(&oFieldDefn);

    m_abGeneratedColumns.resize(m_poFeatureDefn->GetFieldCount());

    if (m_pszFidColumn != nullptr &&
        EQUAL(oFieldDefn.GetNameRef(), m_pszFidColumn))
    {
        m_iFIDAsRegularColumnIndex = m_poFeatureDefn->GetFieldCount() - 1;
    }

    if (!m_bDeferredCreation)
    {
        ResetReading();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                DoSpecialProcessingForColumnCreation()                */
/************************************************************************/

bool OGRGeoPackageTableLayer::DoSpecialProcessingForColumnCreation(
    const OGRFieldDefn *poField)
{
    const std::string &osConstraintName(poField->GetDomainName());
    const std::string osName(poField->GetAlternativeNameRef());
    const std::string &osDescription(poField->GetComment());

    std::string osMimeType;
    if (poField->GetType() == OFTString && poField->GetSubType() == OFSTJSON)
    {
        osMimeType = "application/json";
    }

    if (osConstraintName.empty() && osName.empty() && osDescription.empty() &&
        osMimeType.empty())
    {
        // no record required
        return true;
    }

    if (!m_poDS->CreateColumnsTableAndColumnConstraintsTablesIfNecessary())
        return false;

    /* Now let's register our column. */
    std::string osNameSqlValue;
    if (osName.empty())
    {
        osNameSqlValue = "NULL";
    }
    else
    {
        char *pszName = sqlite3_mprintf("'%q'", osName.c_str());
        osNameSqlValue = std::string(pszName);
        sqlite3_free(pszName);
    }

    std::string osDescriptionSqlValue;
    if (osDescription.empty())
    {
        osDescriptionSqlValue = "NULL";
    }
    else
    {
        char *pszDescription = sqlite3_mprintf("'%q'", osDescription.c_str());
        osDescriptionSqlValue = std::string(pszDescription);
        sqlite3_free(pszDescription);
    }

    std::string osMimeTypeSqlValue;
    if (osMimeType.empty())
    {
        osMimeTypeSqlValue = "NULL";
    }
    else
    {
        char *pszMimeType = sqlite3_mprintf("'%q'", osMimeType.c_str());
        osMimeTypeSqlValue = std::string(pszMimeType);
        sqlite3_free(pszMimeType);
    }

    std::string osConstraintNameValue;
    if (osConstraintName.empty())
    {
        osConstraintNameValue = "NULL";
    }
    else
    {
        char *pszConstraintName =
            sqlite3_mprintf("'%q'", osConstraintName.c_str());
        osConstraintNameValue = std::string(pszConstraintName);
        sqlite3_free(pszConstraintName);
    }

    char *pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_data_columns (table_name, column_name, name, "
        "title, description, mime_type, constraint_name) VALUES ("
        "'%q', '%q', %s, NULL, %s, %s, %s)",
        m_pszTableName, poField->GetNameRef(), osNameSqlValue.c_str(),
        osDescriptionSqlValue.c_str(), osMimeTypeSqlValue.c_str(),
        osConstraintNameValue.c_str());

    bool ok = SQLCommand(m_poDS->GetDB(), pszSQL) == OGRERR_NONE;
    sqlite3_free(pszSQL);
    return ok;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr
OGRGeoPackageTableLayer::CreateGeomField(const OGRGeomFieldDefn *poGeomFieldIn,
                                         int /* bApproxOK */)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (!CheckUpdatableTable("CreateGeomField"))
        return OGRERR_FAILURE;

    if (m_poFeatureDefn->GetGeomFieldCount() == 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create more than on geometry field in GeoPackage");
        return OGRERR_FAILURE;
    }

    OGRwkbGeometryType eType = poGeomFieldIn->GetType();
    if (eType == wkbNone)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create geometry field of type wkbNone");
        return OGRERR_FAILURE;
    }

    OGRGeomFieldDefn oGeomField(poGeomFieldIn);
    auto poSRSOri = poGeomFieldIn->GetSpatialRef();
    if (poSRSOri)
    {
        auto poSRS = poSRSOri->Clone();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        oGeomField.SetSpatialRef(poSRS);
        poSRS->Release();
    }
    if (EQUAL(oGeomField.GetNameRef(), ""))
    {
        oGeomField.SetName("geom");
    }

    const OGRSpatialReference *poSRS = oGeomField.GetSpatialRef();
    m_iSrs = m_poDS->GetSrsId(poSRS);

    /* -------------------------------------------------------------------- */
    /*      Create the new field.                                           */
    /* -------------------------------------------------------------------- */
    if (!m_bDeferredCreation)
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
        if (err != OGRERR_NONE)
            return err;
    }

    whileUnsealing(m_poFeatureDefn)->AddGeomFieldDefn(&oGeomField);

    if (!m_bDeferredCreation)
    {
        OGRErr err = RegisterGeometryColumn();
        if (err != OGRERR_NONE)
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
/*                     CreateFeatureCountTriggers()                     */
/************************************************************************/

void OGRGeoPackageTableLayer::CreateFeatureCountTriggers(
    const char *pszTableName)
{
    if (m_bAddOGRFeatureCountTriggers)
    {
        if (pszTableName == nullptr)
            pszTableName = m_pszTableName;

        m_bOGRFeatureCountTriggersEnabled = true;
        m_bAddOGRFeatureCountTriggers = false;
        m_bFeatureCountTriggersDeletedInTransaction = false;

        CPLDebug("GPKG", "Creating insert/delete feature_count triggers");
        char *pszSQL = sqlite3_mprintf(
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
/*                   DisableFeatureCountTriggers()                      */
/************************************************************************/

void OGRGeoPackageTableLayer::DisableFeatureCountTriggers(
    bool bNullifyFeatureCount)
{
    if (m_bOGRFeatureCountTriggersEnabled)
    {
        m_bOGRFeatureCountTriggersEnabled = false;
        m_bAddOGRFeatureCountTriggers = true;
        m_bFeatureCountTriggersDeletedInTransaction = m_poDS->IsInTransaction();

        CPLDebug("GPKG", "Deleting insert/delete feature_count triggers");

        char *pszSQL = sqlite3_mprintf(
            "DROP TRIGGER \"trigger_insert_feature_count_%w\"", m_pszTableName);
        SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);

        pszSQL = sqlite3_mprintf(
            "DROP TRIGGER \"trigger_delete_feature_count_%w\"", m_pszTableName);
        SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);

        if (m_poDS->m_bHasGPKGOGRContents && bNullifyFeatureCount)
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

#endif  // #ifdef ENABLE_GPKG_OGR_CONTENTS

/************************************************************************/
/*                      CheckGeometryType()                             */
/************************************************************************/

/** Check that the feature geometry type is consistent with the layer geometry
 * type.
 *
 * And potentially update the Z and M flags of gpkg_geometry_columns to
 * reflect the dimensionality of feature geometries.
 */
void OGRGeoPackageTableLayer::CheckGeometryType(const OGRFeature *poFeature)
{
    const OGRwkbGeometryType eLayerGeomType = GetGeomType();
    const OGRwkbGeometryType eFlattenLayerGeomType = wkbFlatten(eLayerGeomType);
    const OGRGeometry *poGeom = poFeature->GetGeometryRef();
    if (eFlattenLayerGeomType != wkbNone && eFlattenLayerGeomType != wkbUnknown)
    {
        if (poGeom != nullptr)
        {
            OGRwkbGeometryType eGeomType =
                wkbFlatten(poGeom->getGeometryType());
            if (!OGR_GT_IsSubClassOf(eGeomType, eFlattenLayerGeomType) &&
                m_eSetBadGeomTypeWarned.find(eGeomType) ==
                    m_eSetBadGeomTypeWarned.end())
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
                         OGRToOGCGeomType(eGeomType), GetName(),
                         OGRToOGCGeomType(eFlattenLayerGeomType));
                m_eSetBadGeomTypeWarned.insert(eGeomType);
            }
        }
    }

    // Make sure to update the z and m columns of gpkg_geometry_columns to 2
    // if we have geometries with Z and M components
    if (m_nZFlag == 0 || m_nMFlag == 0)
    {
        if (poGeom != nullptr)
        {
            bool bUpdateGpkgGeometryColumnsTable = false;
            const OGRwkbGeometryType eGeomType = poGeom->getGeometryType();
            if (m_nZFlag == 0 && wkbHasZ(eGeomType))
            {
                if (eLayerGeomType != wkbUnknown && !wkbHasZ(eLayerGeomType))
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Layer '%s' has been declared with non-Z geometry type "
                        "%s, but it does contain geometries with Z. Setting "
                        "the Z=2 hint into gpkg_geometry_columns",
                        GetName(),
                        OGRToOGCGeomType(eLayerGeomType, true, true, true));
                }
                m_nZFlag = 2;
                bUpdateGpkgGeometryColumnsTable = true;
            }
            if (m_nMFlag == 0 && wkbHasM(eGeomType))
            {
                if (eLayerGeomType != wkbUnknown && !wkbHasM(eLayerGeomType))
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Layer '%s' has been declared with non-M geometry type "
                        "%s, but it does contain geometries with M. Setting "
                        "the M=2 hint into gpkg_geometry_columns",
                        GetName(),
                        OGRToOGCGeomType(eLayerGeomType, true, true, true));
                }
                m_nMFlag = 2;
                bUpdateGpkgGeometryColumnsTable = true;
            }
            if (bUpdateGpkgGeometryColumnsTable)
            {
                /* Update gpkg_geometry_columns */
                char *pszSQL = sqlite3_mprintf(
                    "UPDATE gpkg_geometry_columns SET z = %d, m = %d WHERE "
                    "table_name = '%q' AND column_name = '%q'",
                    m_nZFlag, m_nMFlag, GetName(), GetGeometryColumn());
                CPL_IGNORE_RET_VAL(SQLCommand(m_poDS->GetDB(), pszSQL));
                sqlite3_free(pszSQL);
            }
        }
    }
}

/************************************************************************/
/*                   CheckFIDAndFIDColumnConsistency()                  */
/************************************************************************/

static bool CheckFIDAndFIDColumnConsistency(const OGRFeature *poFeature,
                                            int iFIDAsRegularColumnIndex)
{
    bool ok = false;
    if (!poFeature->IsFieldSetAndNotNull(iFIDAsRegularColumnIndex))
    {
        // nothing to do
    }
    else if (poFeature->GetDefnRef()
                 ->GetFieldDefn(iFIDAsRegularColumnIndex)
                 ->GetType() == OFTReal)
    {
        const double dfFID =
            poFeature->GetFieldAsDouble(iFIDAsRegularColumnIndex);
        if (GDALIsValueInRange<int64_t>(dfFID))
        {
            const auto nFID = static_cast<GIntBig>(dfFID);
            if (nFID == poFeature->GetFID())
            {
                ok = true;
            }
        }
    }
    else if (poFeature->GetFieldAsInteger64(iFIDAsRegularColumnIndex) ==
             poFeature->GetFID())
    {
        ok = true;
    }
    if (!ok)
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
#define RNDTOWARDS (1.0 - 1.0 / 8388608.0) /* Round towards zero */
#define RNDAWAY (1.0 + 1.0 / 8388608.0)    /* Round away from zero */

/*
** Convert an sqlite3_value into an RtreeValue (presumably a float)
** while taking care to round toward negative or positive, respectively.
*/
static float rtreeValueDown(double d)
{
    float f = static_cast<float>(d);
    if (f > d)
    {
        f = static_cast<float>(d * (d < 0 ? RNDAWAY : RNDTOWARDS));
    }
    return f;
}

static float rtreeValueUp(double d)
{
    float f = static_cast<float>(d);
    if (f < d)
    {
        f = static_cast<float>(d * (d < 0 ? RNDTOWARDS : RNDAWAY));
    }
    return f;
}

OGRErr OGRGeoPackageTableLayer::CreateOrUpsertFeature(OGRFeature *poFeature,
                                                      bool bUpsert)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (!m_poDS->GetUpdate())
    {
        CPLError(CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
                 "CreateFeature");
        return OGRERR_FAILURE;
    }

    if (m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE)
        return OGRERR_FAILURE;

    CancelAsyncNextArrowArray();

    std::string osUpsertUniqueColumnName;
    if (bUpsert && poFeature->GetFID() == OGRNullFID)
    {
        int nUniqueColumns = 0;
        const int nFieldCount = m_poFeatureDefn->GetFieldCount();
        for (int i = 0; i < nFieldCount; ++i)
        {
            const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
            if (poFieldDefn->IsUnique())
            {
                if (osUpsertUniqueColumnName.empty())
                    osUpsertUniqueColumnName = poFieldDefn->GetNameRef();
                nUniqueColumns++;
            }
        }
        if (nUniqueColumns == 0)
        {
            // This is just a regular INSERT
            bUpsert = false;
        }
    }

    if (bUpsert)
    {
        if (m_bThreadRTreeStarted)
            CancelAsyncRTree();
        if (!RunDeferredSpatialIndexUpdate())
            return OGRERR_FAILURE;
        if (!m_bUpdate1TriggerDisabled && HasSpatialIndex())
            WorkaroundUpdate1TriggerIssue();
    }

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (bUpsert)
    {
        if (m_nTotalFeatureCount >= 0)
        {
            // There's no reliable way of knowing if a new row has been inserted
            // or just updated, so serialize known value and then
            // invalidate feature count.
            if (m_poDS->m_bHasGPKGOGRContents)
            {
                const char *pszCount =
                    CPLSPrintf(CPL_FRMT_GIB, m_nTotalFeatureCount);
                char *pszSQL = sqlite3_mprintf(
                    "UPDATE gpkg_ogr_contents SET feature_count = %s WHERE "
                    "lower(table_name )= lower('%q')",
                    pszCount, m_pszTableName);
                SQLCommand(m_poDS->GetDB(), pszSQL);
                sqlite3_free(pszSQL);
            }
            m_nTotalFeatureCount = -1;

            if (!m_bOGRFeatureCountTriggersEnabled)
                CreateFeatureCountTriggers();
        }
    }
    else
    {
        // To maximize performance of insertion, disable feature count triggers
        if (m_bOGRFeatureCountTriggersEnabled)
        {
            DisableFeatureCountTriggers();
        }
    }
#endif

    CheckGeometryType(poFeature);

    /* Substitute default values for null Date/DateTime fields as the standard
     */
    /* format of SQLite is not the one mandated by GeoPackage */
    poFeature->FillUnsetWithDefault(FALSE, nullptr);
    bool bHasDefaultValue = false;
    const int nFieldCount = m_poFeatureDefn->GetFieldCount();
    for (int iField = 0; iField < nFieldCount; iField++)
    {
        if (poFeature->IsFieldSetUnsafe(iField))
            continue;
        const char *pszDefault =
            m_poFeatureDefn->GetFieldDefnUnsafe(iField)->GetDefault();
        if (pszDefault != nullptr)
        {
            bHasDefaultValue = true;
        }
    }

    /* In case the FID column has also been created as a regular field */
    if (m_iFIDAsRegularColumnIndex >= 0)
    {
        if (poFeature->GetFID() == OGRNullFID)
        {
            if (poFeature->IsFieldSetAndNotNull(m_iFIDAsRegularColumnIndex))
            {
                if (m_poFeatureDefn->GetFieldDefn(m_iFIDAsRegularColumnIndex)
                        ->GetType() == OFTReal)
                {
                    bool ok = false;
                    const double dfFID =
                        poFeature->GetFieldAsDouble(m_iFIDAsRegularColumnIndex);
                    if (dfFID >= static_cast<double>(
                                     std::numeric_limits<int64_t>::min()) &&
                        dfFID <= static_cast<double>(
                                     std::numeric_limits<int64_t>::max()))
                    {
                        const auto nFID = static_cast<GIntBig>(dfFID);
                        if (static_cast<double>(nFID) == dfFID)
                        {
                            poFeature->SetFID(nFID);
                            ok = true;
                        }
                    }
                    if (!ok)
                    {
                        CPLError(
                            CE_Failure, CPLE_AppDefined,
                            "Value of FID %g cannot be parsed to an Integer64",
                            dfFID);
                        return OGRERR_FAILURE;
                    }
                }
                else
                {
                    poFeature->SetFID(poFeature->GetFieldAsInteger64(
                        m_iFIDAsRegularColumnIndex));
                }
            }
        }
        else if (!CheckFIDAndFIDColumnConsistency(poFeature,
                                                  m_iFIDAsRegularColumnIndex))
        {
            return OGRERR_FAILURE;
        }
    }

    /* If there's a unset field with a default value, then we must create */
    /* a specific INSERT statement to avoid unset fields to be bound to NULL */
    if (m_poInsertStatement &&
        (bHasDefaultValue ||
         m_bInsertStatementWithFID != (poFeature->GetFID() != OGRNullFID) ||
         m_bInsertStatementWithUpsert != bUpsert ||
         m_osInsertStatementUpsertUniqueColumnName != osUpsertUniqueColumnName))
    {
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = nullptr;
    }

    if (!m_poInsertStatement)
    {
        /* Construct a SQL INSERT statement from the OGRFeature */
        /* Only work with fields that are set */
        /* Do not stick values into SQL, use placeholder and bind values later
         */
        m_bInsertStatementWithFID = poFeature->GetFID() != OGRNullFID;
        m_bInsertStatementWithUpsert = bUpsert;
        m_osInsertStatementUpsertUniqueColumnName = osUpsertUniqueColumnName;
        CPLString osCommand = FeatureGenerateInsertSQL(
            poFeature, m_bInsertStatementWithFID, !bHasDefaultValue, bUpsert,
            osUpsertUniqueColumnName);

        /* Prepare the SQL into a statement */
        sqlite3 *poDb = m_poDS->GetDB();
        int err = sqlite3_prepare_v2(poDb, osCommand, -1, &m_poInsertStatement,
                                     nullptr);
        if (err != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "failed to prepare SQL: %s - %s", osCommand.c_str(),
                     sqlite3_errmsg(poDb));
            return OGRERR_FAILURE;
        }
    }

    /* Bind values onto the statement now */
    OGRErr errOgr = FeatureBindInsertParameters(poFeature, m_poInsertStatement,
                                                m_bInsertStatementWithFID,
                                                !bHasDefaultValue);
    if (errOgr != OGRERR_NONE)
    {
        sqlite3_reset(m_poInsertStatement);
        sqlite3_clear_bindings(m_poInsertStatement);
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = nullptr;
        return errOgr;
    }

    /* From here execute the statement and check errors */
    const int err = sqlite3_step(m_poInsertStatement);
    if (!(err == SQLITE_OK || err == SQLITE_DONE
#if SQLITE_VERSION_NUMBER >= 3035000L
          || err == SQLITE_ROW
#endif
          ))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to execute insert : %s",
                 sqlite3_errmsg(m_poDS->GetDB())
                     ? sqlite3_errmsg(m_poDS->GetDB())
                     : "");
        sqlite3_reset(m_poInsertStatement);
        sqlite3_clear_bindings(m_poInsertStatement);
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = nullptr;
        return OGRERR_FAILURE;
    }

    /* Read the latest FID value */
    const GIntBig nFID = (bUpsert && !osUpsertUniqueColumnName.empty())
                             ?
#if SQLITE_VERSION_NUMBER >= 3035000L
                             sqlite3_column_int64(m_poInsertStatement, 0)
#else
                             OGRNullFID
#endif
                             : sqlite3_last_insert_rowid(m_poDS->GetDB());

    sqlite3_reset(m_poInsertStatement);
    sqlite3_clear_bindings(m_poInsertStatement);

    if (bHasDefaultValue)
    {
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = nullptr;
    }

    if (nFID != OGRNullFID)
    {
        poFeature->SetFID(nFID);
        if (m_iFIDAsRegularColumnIndex >= 0)
            poFeature->SetField(m_iFIDAsRegularColumnIndex, nFID);
    }
    else
    {
        poFeature->SetFID(OGRNullFID);
    }

    /* Update the layer extents with this new object */
    if (IsGeomFieldSet(poFeature))
    {
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(0);
        if (!poGeom->IsEmpty())
        {
            OGREnvelope oEnv;
            poGeom->getEnvelope(&oEnv);
            UpdateExtent(&oEnv);

            if (!bUpsert && !m_bDeferredSpatialIndexCreation &&
                HasSpatialIndex() && m_poDS->IsInTransaction())
            {
                m_nCountInsertInTransaction++;
                if (m_nCountInsertInTransactionThreshold < 0)
                {
                    m_nCountInsertInTransactionThreshold =
                        atoi(CPLGetConfigOption(
                            "OGR_GPKG_DEFERRED_SPI_UPDATE_THRESHOLD", "100"));
                }
                if (m_nCountInsertInTransaction ==
                    m_nCountInsertInTransactionThreshold)
                {
                    StartDeferredSpatialIndexUpdate();
                }
                else if (!m_aoRTreeTriggersSQL.empty())
                {
                    if (m_aoRTreeEntries.size() == 1000 * 1000)
                    {
                        if (!FlushPendingSpatialIndexUpdate())
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
            else if (!bUpsert && m_bAllowedRTreeThread &&
                     !m_bErrorDuringRTreeThread)
            {
                GPKGRTreeEntry sEntry;
#ifdef DEBUG_VERBOSE
                if (m_aoRTreeEntries.empty())
                    CPLDebug("GPKG",
                             "Starting to fill m_aoRTreeEntries at "
                             "FID " CPL_FRMT_GIB,
                             nFID);
#endif
                sEntry.nId = nFID;
                sEntry.fMinX = rtreeValueDown(oEnv.MinX);
                sEntry.fMaxX = rtreeValueUp(oEnv.MaxX);
                sEntry.fMinY = rtreeValueDown(oEnv.MinY);
                sEntry.fMaxY = rtreeValueUp(oEnv.MaxY);
                try
                {
                    m_aoRTreeEntries.push_back(sEntry);
                    if (m_aoRTreeEntries.size() == m_nRTreeBatchSize)
                    {
                        m_oQueueRTreeEntries.push(std::move(m_aoRTreeEntries));
                        m_aoRTreeEntries = std::vector<GPKGRTreeEntry>();
                    }
                    if (!m_bThreadRTreeStarted &&
                        m_oQueueRTreeEntries.size() ==
                            m_nRTreeBatchesBeforeStart)
                    {
                        StartAsyncRTree();
                    }
                }
                catch (const std::bad_alloc &)
                {
                    CPLDebug("GPKG",
                             "Memory allocation error regarding RTree "
                             "structures. Falling back to slower method");
                    if (m_bThreadRTreeStarted)
                        CancelAsyncRTree();
                    else
                        m_bAllowedRTreeThread = false;
                }
            }
        }
    }

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (m_nTotalFeatureCount >= 0)
        m_nTotalFeatureCount++;
#endif

    m_bContentChanged = true;

    /* All done! */
    return OGRERR_NONE;
}

OGRErr OGRGeoPackageTableLayer::ICreateFeature(OGRFeature *poFeature)
{
    return CreateOrUpsertFeature(poFeature, /* bUpsert=*/false);
}

/************************************************************************/
/*                  SetDeferredSpatialIndexCreation()                   */
/************************************************************************/

void OGRGeoPackageTableLayer::SetDeferredSpatialIndexCreation(bool bFlag)
{
    m_bDeferredSpatialIndexCreation = bFlag;
    if (bFlag)
    {
        // This method is invoked before the layer is added to the dataset,
        // so GetLayerCount() will return 0 for the first layer added.
        m_bAllowedRTreeThread =
            m_poDS->GetLayerCount() == 0 && sqlite3_threadsafe() != 0 &&
            CPLGetNumCPUs() >= 2 &&
            CPLTestBool(
                CPLGetConfigOption("OGR_GPKG_ALLOW_THREADED_RTREE", "YES"));

        // For unit tests
        if (CPLTestBool(CPLGetConfigOption(
                "OGR_GPKG_THREADED_RTREE_AT_FIRST_FEATURE", "NO")))
        {
            m_nRTreeBatchSize = 10;
            m_nRTreeBatchesBeforeStart = 1;
        }
    }
}

/************************************************************************/
/*                          StartAsyncRTree()                           */
/************************************************************************/

// We create a temporary database with only the RTree, and we insert
// records into it in a dedicated thread, in parallel of the main thread
// that inserts rows in the user table. When the layer is finalized, we
// just use bulk copy statements of the form
// INSERT INTO rtree_xxxx_rowid/node/parent SELECT * FROM
// temp_rtree.my_rtree_rowid/node/parent to copy the RTree auxiliary tables into
// the main database, which is a very fast operation.

void OGRGeoPackageTableLayer::StartAsyncRTree()
{
    m_osAsyncDBName = m_poDS->GetDescription();
    m_osAsyncDBName += ".tmp_rtree_";
    bool bCanUseTableName = false;
    if (strlen(m_pszTableName) <= 32)
    {
        bCanUseTableName = true;
        constexpr char DIGIT_ZERO = '0';
        for (int i = 0; m_pszTableName[i] != '\0'; ++i)
        {
            const char ch = m_pszTableName[i];
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= DIGIT_ZERO && ch <= '9') || ch == '.' || ch == '_'))
            {
                bCanUseTableName = false;
                break;
            }
        }
    }
    if (bCanUseTableName)
        m_osAsyncDBName += m_pszTableName;
    else
    {
        m_osAsyncDBName += CPLMD5String(m_pszTableName);
    }
    m_osAsyncDBName += ".db";

    m_osAsyncDBAttachName = "temp_rtree_";
    m_osAsyncDBAttachName += CPLMD5String(m_pszTableName);

    VSIUnlink(m_osAsyncDBName.c_str());
    CPLDebug("GPKG", "Creating background RTree DB %s",
             m_osAsyncDBName.c_str());
    if (sqlite3_open_v2(m_osAsyncDBName.c_str(), &m_hAsyncDBHandle,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        m_poDS->GetVFS() ? m_poDS->GetVFS()->zName : nullptr) !=
        SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "sqlite3_open_v2() of %s failed",
                 m_osAsyncDBName.c_str());
        sqlite3_close(m_hAsyncDBHandle);
        m_hAsyncDBHandle = nullptr;
    }
    if (m_hAsyncDBHandle != nullptr)
    {
        /* Make sure our auxiliary DB has the same page size as the main one.
         * Because the number of RTree cells depends on the SQLite page size.
         * However the sqlite implementation limits to 51 cells maximum per page,
         * which is reached starting with a page size of 2048 bytes.
         * As the default SQLite page size is 4096 currently, having potentially
         * different page sizes >= 4096 between the main and auxiliary DBs would
         * not be a practical issue, but better be consistent.
         */
        const int nPageSize =
            SQLGetInteger(m_poDS->GetDB(), "PRAGMA page_size", nullptr);

        if (SQLCommand(m_hAsyncDBHandle,
                       CPLSPrintf("PRAGMA page_size = %d;\n"
                                  "PRAGMA journal_mode = OFF;\n"
                                  "PRAGMA synchronous = OFF;",
                                  nPageSize)) == OGRERR_NONE)
        {
            char *pszSQL = sqlite3_mprintf("ATTACH DATABASE '%q' AS '%q'",
                                           m_osAsyncDBName.c_str(),
                                           m_osAsyncDBAttachName.c_str());
            OGRErr eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
            sqlite3_free(pszSQL);

            if (eErr == OGRERR_NONE)
            {
                m_hRTree = gdal_sqlite_rtree_bl_new(nPageSize);
                try
                {
                    m_oThreadRTree =
                        std::thread([this]() { AsyncRTreeThreadFunction(); });
                    m_bThreadRTreeStarted = true;
                }
                catch (const std::exception &e)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "RTree thread cannot be created: %s", e.what());
                }
            }
        }

        if (!m_bThreadRTreeStarted)
        {
            if (m_hRTree)
            {
                gdal_sqlite_rtree_bl_free(m_hRTree);
                m_hRTree = nullptr;
            }
            m_oQueueRTreeEntries.clear();
            m_bErrorDuringRTreeThread = true;
            sqlite3_close(m_hAsyncDBHandle);
            m_hAsyncDBHandle = nullptr;
            VSIUnlink(m_osAsyncDBName.c_str());
        }
    }
    else
    {
        m_oQueueRTreeEntries.clear();
        m_bErrorDuringRTreeThread = true;
    }
}

/************************************************************************/
/*                        RemoveAsyncRTreeTempDB()                      */
/************************************************************************/

void OGRGeoPackageTableLayer::RemoveAsyncRTreeTempDB()
{
    if (!m_osAsyncDBAttachName.empty())
    {
        SQLCommand(
            m_poDS->GetDB(),
            CPLSPrintf("DETACH DATABASE \"%s\"",
                       SQLEscapeName(m_osAsyncDBAttachName.c_str()).c_str()));
        m_osAsyncDBAttachName.clear();
        VSIUnlink(m_osAsyncDBName.c_str());
        m_osAsyncDBName.clear();
    }
}

/************************************************************************/
/*                          CancelAsyncRTree()                          */
/************************************************************************/

void OGRGeoPackageTableLayer::CancelAsyncRTree()
{
    CPLDebug("GPKG", "Cancel background RTree creation");
    m_oQueueRTreeEntries.push({});
    m_oThreadRTree.join();
    m_bThreadRTreeStarted = false;
    if (m_hAsyncDBHandle)
    {
        sqlite3_close(m_hAsyncDBHandle);
        m_hAsyncDBHandle = nullptr;
    }
    gdal_sqlite_rtree_bl_free(m_hRTree);
    m_hRTree = nullptr;
    m_bErrorDuringRTreeThread = true;
    RemoveAsyncRTreeTempDB();
}

/************************************************************************/
/*                     FinishOrDisableThreadedRTree()                   */
/************************************************************************/

void OGRGeoPackageTableLayer::FinishOrDisableThreadedRTree()
{
    if (m_bThreadRTreeStarted)
    {
        CreateSpatialIndexIfNecessary();
    }
    m_bAllowedRTreeThread = false;
}

/************************************************************************/
/*                       FlushInMemoryRTree()                           */
/************************************************************************/

bool OGRGeoPackageTableLayer::FlushInMemoryRTree(sqlite3 *hRTreeDB,
                                                 const char *pszRTreeName)
{
    if (hRTreeDB == m_hAsyncDBHandle)
        SQLCommand(hRTreeDB, "BEGIN");

    char *pszErrMsg = nullptr;
    bool bRet = gdal_sqlite_rtree_bl_serialize(m_hRTree, hRTreeDB, pszRTreeName,
                                               "id", "minx", "miny", "maxx",
                                               "maxy", &pszErrMsg);
    if (hRTreeDB == m_hAsyncDBHandle)
    {
        if (bRet)
            bRet = SQLCommand(hRTreeDB, "COMMIT") == OGRERR_NONE;
        else
            SQLCommand(hRTreeDB, "ROLLBACK");
    }

    gdal_sqlite_rtree_bl_free(m_hRTree);
    m_hRTree = nullptr;

    if (!bRet)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "sqlite_rtree_bl_serialize() failed with %s",
                 pszErrMsg ? pszErrMsg : "(null)");

        m_bErrorDuringRTreeThread = true;

        if (m_hAsyncDBHandle)
        {
            sqlite3_close(m_hAsyncDBHandle);
            m_hAsyncDBHandle = nullptr;
        }

        m_oQueueRTreeEntries.clear();
    }
    sqlite3_free(pszErrMsg);

    return bRet;
}

/************************************************************************/
/*                     GetMaxRAMUsageAllowedForRTree()                  */
/************************************************************************/

static size_t GetMaxRAMUsageAllowedForRTree()
{
    const uint64_t nUsableRAM = CPLGetUsablePhysicalRAM();
    uint64_t nMaxRAMUsageAllowed =
        (nUsableRAM ? nUsableRAM / 10 : 100 * 1024 * 1024);
    const char *pszMaxRAMUsageAllowed =
        CPLGetConfigOption("OGR_GPKG_MAX_RAM_USAGE_RTREE", nullptr);
    if (pszMaxRAMUsageAllowed)
    {
        nMaxRAMUsageAllowed = static_cast<uint64_t>(
            std::strtoull(pszMaxRAMUsageAllowed, nullptr, 10));
    }
    if (nMaxRAMUsageAllowed > std::numeric_limits<size_t>::max() - 1U)
    {
        nMaxRAMUsageAllowed = std::numeric_limits<size_t>::max() - 1U;
    }
    return static_cast<size_t>(nMaxRAMUsageAllowed);
}

/************************************************************************/
/*                      AsyncRTreeThreadFunction()                      */
/************************************************************************/

void OGRGeoPackageTableLayer::AsyncRTreeThreadFunction()
{
    CPLAssert(m_hRTree);

    const size_t nMaxRAMUsageAllowed = GetMaxRAMUsageAllowedForRTree();
    sqlite3_stmt *hStmt = nullptr;
    GIntBig nCount = 0;
    while (true)
    {
        const auto aoEntries = m_oQueueRTreeEntries.get_and_pop_front();
        if (aoEntries.empty())
            break;

        constexpr int NOTIFICATION_INTERVAL = 500 * 1000;

        auto oIter = aoEntries.begin();
        if (m_hRTree)
        {
            for (; oIter != aoEntries.end(); ++oIter)
            {
                const auto &entry = *oIter;
                if (gdal_sqlite_rtree_bl_ram_usage(m_hRTree) >
                        nMaxRAMUsageAllowed ||
                    !gdal_sqlite_rtree_bl_insert(m_hRTree, entry.nId,
                                                 entry.fMinX, entry.fMinY,
                                                 entry.fMaxX, entry.fMaxY))
                {
                    CPLDebug("GPKG", "Too large in-memory RTree. "
                                     "Flushing it and using memory friendly "
                                     "algorithm for the rest");
                    if (!FlushInMemoryRTree(m_hAsyncDBHandle, "my_rtree"))
                        return;
                    break;
                }
                ++nCount;
                if ((nCount % NOTIFICATION_INTERVAL) == 0)
                {
                    CPLDebug("GPKG", CPL_FRMT_GIB " rows indexed in rtree",
                             nCount);
                }
            }
            if (oIter == aoEntries.end())
                continue;
        }

        if (hStmt == nullptr)
        {
            const char *pszInsertSQL =
                CPLGetConfigOption(
                    "OGR_GPKG_SIMULATE_INSERT_INTO_MY_RTREE_PREPARATION_ERROR",
                    nullptr)
                    ? "INSERT INTO my_rtree_SIMULATE_ERROR VALUES (?,?,?,?,?)"
                    : "INSERT INTO my_rtree VALUES (?,?,?,?,?)";
            if (sqlite3_prepare_v2(m_hAsyncDBHandle, pszInsertSQL, -1, &hStmt,
                                   nullptr) != SQLITE_OK)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "failed to prepare SQL: %s: %s", pszInsertSQL,
                         sqlite3_errmsg(m_hAsyncDBHandle));

                m_bErrorDuringRTreeThread = true;

                sqlite3_close(m_hAsyncDBHandle);
                m_hAsyncDBHandle = nullptr;

                m_oQueueRTreeEntries.clear();
                return;
            }

            SQLCommand(m_hAsyncDBHandle, "BEGIN");
        }

#ifdef DEBUG_VERBOSE
        CPLDebug("GPKG",
                 "AsyncRTreeThreadFunction(): "
                 "Processing batch of %d features, "
                 "starting at FID " CPL_FRMT_GIB " and ending "
                 "at FID " CPL_FRMT_GIB,
                 static_cast<int>(aoEntries.size()), aoEntries.front().nId,
                 aoEntries.back().nId);
#endif
        for (; oIter != aoEntries.end(); ++oIter)
        {
            const auto &entry = *oIter;
            sqlite3_reset(hStmt);

            sqlite3_bind_int64(hStmt, 1, entry.nId);
            sqlite3_bind_double(hStmt, 2, entry.fMinX);
            sqlite3_bind_double(hStmt, 3, entry.fMaxX);
            sqlite3_bind_double(hStmt, 4, entry.fMinY);
            sqlite3_bind_double(hStmt, 5, entry.fMaxY);
            int sqlite_err = sqlite3_step(hStmt);
            if (sqlite_err != SQLITE_OK && sqlite_err != SQLITE_DONE)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "failed to execute insertion in RTree : %s",
                         sqlite3_errmsg(m_hAsyncDBHandle));
                m_bErrorDuringRTreeThread = true;
                break;
            }
            ++nCount;
            if ((nCount % NOTIFICATION_INTERVAL) == 0)
            {
                CPLDebug("GPKG", CPL_FRMT_GIB " rows indexed in rtree", nCount);
                if (SQLCommand(m_hAsyncDBHandle, "COMMIT") != OGRERR_NONE)
                {
                    m_bErrorDuringRTreeThread = true;
                    break;
                }
                SQLCommand(m_hAsyncDBHandle, "BEGIN");
            }
        }
    }
    if (!m_hRTree)
    {
        if (m_bErrorDuringRTreeThread)
        {
            SQLCommand(m_hAsyncDBHandle, "ROLLBACK");
        }
        else if (SQLCommand(m_hAsyncDBHandle, "COMMIT") != OGRERR_NONE)
        {
            m_bErrorDuringRTreeThread = true;
        }

        sqlite3_finalize(hStmt);

        if (m_bErrorDuringRTreeThread)
        {
            sqlite3_close(m_hAsyncDBHandle);
            m_hAsyncDBHandle = nullptr;

            VSIUnlink(m_osAsyncDBName.c_str());

            m_oQueueRTreeEntries.clear();
        }
    }
    CPLDebug("GPKG",
             "AsyncRTreeThreadFunction(): " CPL_FRMT_GIB
             " rows inserted into RTree",
             nCount);
}

/************************************************************************/
/*                          ISetFeature()                                */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::ISetFeature(OGRFeature *poFeature)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (!m_poDS->GetUpdate() || m_pszFidColumn == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
                 "SetFeature");
        return OGRERR_FAILURE;
    }

    /* No FID? */
    if (poFeature->GetFID() == OGRNullFID)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FID required on features given to SetFeature().");
        return OGRERR_FAILURE;
    }

    /* In case the FID column has also been created as a regular field */
    if (m_iFIDAsRegularColumnIndex >= 0 &&
        !CheckFIDAndFIDColumnConsistency(poFeature, m_iFIDAsRegularColumnIndex))
    {
        return OGRERR_FAILURE;
    }

    if (m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE)
        return OGRERR_FAILURE;

    CancelAsyncNextArrowArray();

    if (m_bThreadRTreeStarted)
        CancelAsyncRTree();
    if (!RunDeferredSpatialIndexUpdate())
        return OGRERR_FAILURE;

    const sqlite3_int64 nTotalChangesBefore =
#if SQLITE_VERSION_NUMBER >= 3037000L
        sqlite3_total_changes64(m_poDS->GetDB());
#else
        sqlite3_total_changes(m_poDS->GetDB());
#endif

    CheckGeometryType(poFeature);

    if (!m_osUpdateStatementSQL.empty())
    {
        m_osUpdateStatementSQL.clear();
        if (m_poUpdateStatement)
            sqlite3_finalize(m_poUpdateStatement);
        m_poUpdateStatement = nullptr;
    }
    if (!m_poUpdateStatement)
    {
        /* Construct a SQL UPDATE statement from the OGRFeature */
        /* Only work with fields that are set */
        /* Do not stick values into SQL, use placeholder and bind values later
         */
        const std::string osCommand = FeatureGenerateUpdateSQL(poFeature);
        if (osCommand.empty())
            return OGRERR_NONE;

        /* Prepare the SQL into a statement */
        int err = sqlite3_prepare_v2(m_poDS->GetDB(), osCommand.c_str(),
                                     static_cast<int>(osCommand.size()),
                                     &m_poUpdateStatement, nullptr);
        if (err != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "failed to prepare SQL: %s",
                     osCommand.c_str());
            return OGRERR_FAILURE;
        }
    }

    /* Bind values onto the statement now */
    OGRErr errOgr = FeatureBindUpdateParameters(poFeature, m_poUpdateStatement);
    if (errOgr != OGRERR_NONE)
    {
        sqlite3_reset(m_poUpdateStatement);
        sqlite3_clear_bindings(m_poUpdateStatement);
        return errOgr;
    }

    /* From here execute the statement and check errors */
    int err = sqlite3_step(m_poUpdateStatement);
    if (!(err == SQLITE_OK || err == SQLITE_DONE))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to execute update : %s",
                 sqlite3_errmsg(m_poDS->GetDB()));
        sqlite3_reset(m_poUpdateStatement);
        sqlite3_clear_bindings(m_poUpdateStatement);
        return OGRERR_FAILURE;
    }

    sqlite3_reset(m_poUpdateStatement);
    sqlite3_clear_bindings(m_poUpdateStatement);

    const sqlite3_int64 nTotalChangesAfter =
#if SQLITE_VERSION_NUMBER >= 3037000L
        sqlite3_total_changes64(m_poDS->GetDB());
#else
        sqlite3_total_changes(m_poDS->GetDB());
#endif

    /* Only update the envelope if we changed something */
    OGRErr eErr = nTotalChangesAfter != nTotalChangesBefore
                      ? OGRERR_NONE
                      : OGRERR_NON_EXISTING_FEATURE;
    if (eErr == OGRERR_NONE)
    {
        /* Update the layer extents with this new object */
        if (IsGeomFieldSet(poFeature))
        {
            OGRGeometry *poGeom = poFeature->GetGeomFieldRef(0);
            if (!poGeom->IsEmpty())
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
/*                           IUpsertFeature()                           */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::IUpsertFeature(OGRFeature *poFeature)

{
    return CreateOrUpsertFeature(poFeature, /* bUpsert = */ true);
}

//----------------------------------------------------------------------
// FeatureGenerateUpdateSQL()
//
// Build a SQL UPDATE statement that references all the columns in
// the OGRFeatureDefn that the user asked to be updated, then prepare it for
// repeated use in a prepared statement. All statements start off with geometry
// (if it exists, and if it is asked to be updated), then reference each column
// in the order it appears in the OGRFeatureDefn.
// FeatureBindParameters operates on the expectation of this
// column ordering.

//
std::string OGRGeoPackageTableLayer::FeatureGenerateUpdateSQL(
    const OGRFeature *poFeature, int nUpdatedFieldsCount,
    const int *panUpdatedFieldsIdx, int nUpdatedGeomFieldsCount,
    const int * /*panUpdatedGeomFieldsIdx*/) const
{
    bool bNeedComma = false;
    const OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    /* Set up our SQL string basics */
    std::string osUpdate("UPDATE \"");
    osUpdate += SQLEscapeName(m_pszTableName);
    osUpdate += "\" SET ";

    if (nUpdatedGeomFieldsCount == 1 && poFeatureDefn->GetGeomFieldCount() > 0)
    {
        osUpdate += '"';
        osUpdate +=
            SQLEscapeName(poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef());
        osUpdate += "\"=?";
        bNeedComma = true;
    }

    /* Add attribute column names (except FID) to the SQL */
    for (int i = 0; i < nUpdatedFieldsCount; i++)
    {
        const int iField = panUpdatedFieldsIdx[i];
        if (iField == m_iFIDAsRegularColumnIndex ||
            m_abGeneratedColumns[iField])
            continue;
        if (!poFeature->IsFieldSet(iField))
            continue;
        if (!bNeedComma)
            bNeedComma = true;
        else
            osUpdate += ", ";

        osUpdate += '"';
        osUpdate +=
            SQLEscapeName(poFeatureDefn->GetFieldDefn(iField)->GetNameRef());
        osUpdate += "\"=?";
    }
    if (!bNeedComma)
        return CPLString();

    osUpdate += " WHERE \"";
    osUpdate += SQLEscapeName(m_pszFidColumn);
    osUpdate += "\" = ?";

    return osUpdate;
}

/************************************************************************/
/*                           UpdateFeature()                            */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::IUpdateFeature(
    OGRFeature *poFeature, int nUpdatedFieldsCount,
    const int *panUpdatedFieldsIdx, int nUpdatedGeomFieldsCount,
    const int *panUpdatedGeomFieldsIdx, bool /* bUpdateStyleString*/)

{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (!m_poDS->GetUpdate() || m_pszFidColumn == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
                 "UpdateFeature");
        return OGRERR_FAILURE;
    }

    /* No FID? */
    if (poFeature->GetFID() == OGRNullFID)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FID required on features given to SetFeature().");
        return OGRERR_FAILURE;
    }

    /* In case the FID column has also been created as a regular field */
    if (m_iFIDAsRegularColumnIndex >= 0 &&
        !CheckFIDAndFIDColumnConsistency(poFeature, m_iFIDAsRegularColumnIndex))
    {
        return OGRERR_FAILURE;
    }

    if (m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE)
        return OGRERR_FAILURE;

    CancelAsyncNextArrowArray();

    if (m_bThreadRTreeStarted)
        CancelAsyncRTree();
    if (!RunDeferredSpatialIndexUpdate())
        return OGRERR_FAILURE;

    CheckGeometryType(poFeature);

    /* Construct a SQL UPDATE statement from the OGRFeature */
    /* Only work with fields that are set */
    /* Do not stick values into SQL, use placeholder and bind values later
     */
    const std::string osUpdateStatementSQL = FeatureGenerateUpdateSQL(
        poFeature, nUpdatedFieldsCount, panUpdatedFieldsIdx,
        nUpdatedGeomFieldsCount, panUpdatedGeomFieldsIdx);
    if (osUpdateStatementSQL.empty())
        return OGRERR_NONE;

    if (m_osUpdateStatementSQL != osUpdateStatementSQL)
    {
        if (m_poUpdateStatement)
            sqlite3_finalize(m_poUpdateStatement);
        m_poUpdateStatement = nullptr;
        /* Prepare the SQL into a statement */
        int err =
            sqlite3_prepare_v2(m_poDS->GetDB(), osUpdateStatementSQL.c_str(),
                               static_cast<int>(osUpdateStatementSQL.size()),
                               &m_poUpdateStatement, nullptr);
        if (err != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "failed to prepare SQL: %s",
                     osUpdateStatementSQL.c_str());
            return OGRERR_FAILURE;
        }
        m_osUpdateStatementSQL = osUpdateStatementSQL;
    }

    /* Bind values onto the statement now */
    int nColCount = 0;
    const OGRErr errOgr =
        FeatureBindParameters(poFeature, m_poUpdateStatement, &nColCount, false,
                              false, nUpdatedFieldsCount, panUpdatedFieldsIdx,
                              nUpdatedGeomFieldsCount, panUpdatedGeomFieldsIdx);
    if (errOgr != OGRERR_NONE)
    {
        sqlite3_reset(m_poUpdateStatement);
        sqlite3_clear_bindings(m_poUpdateStatement);
        return errOgr;
    }

    // Bind the FID to the "WHERE" clause.
    const int sqlite_err =
        sqlite3_bind_int64(m_poUpdateStatement, nColCount, poFeature->GetFID());
    if (sqlite_err != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "failed to bind FID '" CPL_FRMT_GIB "' to statement",
                 poFeature->GetFID());
        sqlite3_reset(m_poUpdateStatement);
        sqlite3_clear_bindings(m_poUpdateStatement);
        return OGRERR_FAILURE;
    }

    const sqlite3_int64 nTotalChangesBefore =
#if SQLITE_VERSION_NUMBER >= 3037000L
        sqlite3_total_changes64(m_poDS->GetDB());
#else
        sqlite3_total_changes(m_poDS->GetDB());
#endif

    /* From here execute the statement and check errors */
    int err = sqlite3_step(m_poUpdateStatement);
    if (!(err == SQLITE_OK || err == SQLITE_DONE))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to execute update : %s",
                 sqlite3_errmsg(m_poDS->GetDB()));
        sqlite3_reset(m_poUpdateStatement);
        sqlite3_clear_bindings(m_poUpdateStatement);
        return OGRERR_FAILURE;
    }

    sqlite3_reset(m_poUpdateStatement);
    sqlite3_clear_bindings(m_poUpdateStatement);

    const sqlite3_int64 nTotalChangesAfter =
#if SQLITE_VERSION_NUMBER >= 3037000L
        sqlite3_total_changes64(m_poDS->GetDB());
#else
        sqlite3_total_changes(m_poDS->GetDB());
#endif

    /* Only update the envelope if we changed something */
    OGRErr eErr = nTotalChangesAfter != nTotalChangesBefore
                      ? OGRERR_NONE
                      : OGRERR_NON_EXISTING_FEATURE;
    if (eErr == OGRERR_NONE)
    {
        /* Update the layer extents with this new object */
        if (nUpdatedGeomFieldsCount == 1 && IsGeomFieldSet(poFeature))
        {
            OGRGeometry *poGeom = poFeature->GetGeomFieldRef(0);
            if (!poGeom->IsEmpty())
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

OGRErr OGRGeoPackageTableLayer::SetAttributeFilter(const char *pszQuery)

{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : nullptr;

    if (pszQuery == nullptr)
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
    if (m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE)
        return;

    OGRGeoPackageLayer::ResetReading();

    if (m_poInsertStatement)
    {
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = nullptr;
    }

    if (m_poUpdateStatement)
    {
        sqlite3_finalize(m_poUpdateStatement);
        m_poUpdateStatement = nullptr;
    }
    m_osUpdateStatementSQL.clear();

    if (m_poGetFeatureStatement)
    {
        sqlite3_finalize(m_poGetFeatureStatement);
        m_poGetFeatureStatement = nullptr;
    }

    CancelAsyncNextArrowArray();

    m_bGetNextArrowArrayCalledSinceResetReading = false;

    BuildColumns();
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::SetNextByIndex(GIntBig nIndex)
{
    if (nIndex < 0)
        return OGRERR_FAILURE;
    if (m_soColumns.empty())
        BuildColumns();
    return ResetStatementInternal(nIndex);
}

/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::ResetStatement()

{
    return ResetStatementInternal(0);
}

/************************************************************************/
/*                       ResetStatementInternal()                       */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::ResetStatementInternal(GIntBig nStartIndex)

{
    ClearStatement();

    /* There is no active query statement set up, */
    /* so job #1 is to prepare the statement. */
    /* Append the attribute filter, if there is one */
    CPLString soSQL;
    if (!m_soFilter.empty())
    {
        soSQL.Printf("SELECT %s FROM \"%s\" m WHERE %s", m_soColumns.c_str(),
                     SQLEscapeName(m_pszTableName).c_str(), m_soFilter.c_str());

        if (m_poFilterGeom != nullptr && m_pszAttrQueryString == nullptr &&
            HasSpatialIndex())
        {
            OGREnvelope sEnvelope;

            m_poFilterGeom->getEnvelope(&sEnvelope);

            bool bUseSpatialIndex = true;
            if (m_poExtent && sEnvelope.MinX <= m_poExtent->MinX &&
                sEnvelope.MinY <= m_poExtent->MinY &&
                sEnvelope.MaxX >= m_poExtent->MaxX &&
                sEnvelope.MaxY >= m_poExtent->MaxY)
            {
                // Selecting from spatial filter on whole extent can be rather
                // slow. So use function based filtering, just in case the
                // advertized global extent might be wrong. Otherwise we might
                // just discard completely the spatial filter.
                bUseSpatialIndex = false;
            }

            if (bUseSpatialIndex && !CPLIsInf(sEnvelope.MinX) &&
                !CPLIsInf(sEnvelope.MinY) && !CPLIsInf(sEnvelope.MaxX) &&
                !CPLIsInf(sEnvelope.MaxY))
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
        soSQL.Printf("SELECT %s FROM \"%s\" m", m_soColumns.c_str(),
                     SQLEscapeName(m_pszTableName).c_str());
    if (nStartIndex > 0)
    {
        soSQL += CPLSPrintf(" LIMIT -1 OFFSET " CPL_FRMT_GIB, nStartIndex);
    }

    CPLDebug("GPKG", "ResetStatement(%s)", soSQL.c_str());

    int err = sqlite3_prepare_v2(m_poDS->GetDB(), soSQL.c_str(), -1,
                                 &m_poQueryStatement, nullptr);
    if (err != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to prepare SQL: %s",
                 soSQL.c_str());
        return OGRERR_FAILURE;
    }

    m_iNextShapeId = nStartIndex;
    m_bGetNextArrowArrayCalledSinceResetReading = false;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoPackageTableLayer::GetNextFeature()
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE)
        return nullptr;

    CancelAsyncNextArrowArray();

    if (m_poFilterGeom != nullptr)
    {
        // Both are exclusive
        CreateSpatialIndexIfNecessary();
        if (!RunDeferredSpatialIndexUpdate())
            return nullptr;
    }

    OGRFeature *poFeature = OGRGeoPackageLayer::GetNextFeature();
    if (poFeature && m_iFIDAsRegularColumnIndex >= 0)
    {
        poFeature->SetField(m_iFIDAsRegularColumnIndex, poFeature->GetFID());
    }
    return poFeature;
}

/************************************************************************/
/*                        GetFeature()                                  */
/************************************************************************/

OGRFeature *OGRGeoPackageTableLayer::GetFeature(GIntBig nFID)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE)
        return nullptr;
    CancelAsyncNextArrowArray();

    if (m_pszFidColumn == nullptr)
        return OGRLayer::GetFeature(nFID);

    if (m_poGetFeatureStatement == nullptr)
    {
        CPLString soSQL;
        soSQL.Printf("SELECT %s FROM \"%s\" m "
                     "WHERE \"%s\" = ?",
                     m_soColumns.c_str(), SQLEscapeName(m_pszTableName).c_str(),
                     SQLEscapeName(m_pszFidColumn).c_str());

        const int err = sqlite3_prepare_v2(m_poDS->GetDB(), soSQL.c_str(), -1,
                                           &m_poGetFeatureStatement, nullptr);
        if (err != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "failed to prepare SQL: %s",
                     soSQL.c_str());
            return nullptr;
        }
    }

    CPL_IGNORE_RET_VAL(sqlite3_bind_int64(m_poGetFeatureStatement, 1, nFID));

    /* Should be only one or zero results */
    const int err = sqlite3_step(m_poGetFeatureStatement);

    /* Aha, got one */
    if (err == SQLITE_ROW)
    {
        OGRFeature *poFeature = TranslateFeature(m_poGetFeatureStatement);
        if (m_iFIDAsRegularColumnIndex >= 0)
        {
            poFeature->SetField(m_iFIDAsRegularColumnIndex,
                                poFeature->GetFID());
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
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (!m_poDS->GetUpdate())
    {
        CPLError(CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
                 "DeleteFeature");
        return OGRERR_FAILURE;
    }
    if (m_pszFidColumn == nullptr)
    {
        return OGRERR_FAILURE;
    }

    if (m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE)
        return OGRERR_FAILURE;

    CancelAsyncNextArrowArray();

    if (m_bThreadRTreeStarted)
        CancelAsyncRTree();

    if (!RunDeferredSpatialIndexUpdate())
        return OGRERR_FAILURE;

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (m_bOGRFeatureCountTriggersEnabled)
    {
        DisableFeatureCountTriggers();
    }
#endif

    /* Clear out any existing query */
    ResetReading();

    /* No filters apply, just use the FID */
    CPLString soSQL;
    soSQL.Printf("DELETE FROM \"%s\" WHERE \"%s\" = " CPL_FRMT_GIB,
                 SQLEscapeName(m_pszTableName).c_str(),
                 SQLEscapeName(m_pszFidColumn).c_str(), nFID);

    const sqlite3_int64 nTotalChangesBefore =
#if SQLITE_VERSION_NUMBER >= 3037000L
        sqlite3_total_changes64(m_poDS->GetDB());
#else
        sqlite3_total_changes(m_poDS->GetDB());
#endif

    OGRErr eErr = SQLCommand(m_poDS->GetDB(), soSQL.c_str());
    if (eErr == OGRERR_NONE)
    {
        const sqlite3_int64 nTotalChangesAfter =
#if SQLITE_VERSION_NUMBER >= 3037000L
            sqlite3_total_changes64(m_poDS->GetDB());
#else
            sqlite3_total_changes(m_poDS->GetDB());
#endif

        eErr = nTotalChangesAfter != nTotalChangesBefore
                   ? OGRERR_NONE
                   : OGRERR_NON_EXISTING_FEATURE;

        if (eErr == OGRERR_NONE)
        {
#ifdef ENABLE_GPKG_OGR_CONTENTS
            if (m_nTotalFeatureCount >= 0)
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
    if (m_bAllowedRTreeThread)
        return true;

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
    if (m_bThreadRTreeStarted)
        CancelAsyncRTree();
    m_nCountInsertInTransaction = 0;
    m_aoRTreeTriggersSQL.clear();
    m_aoRTreeEntries.clear();
    if (m_bTableCreatedInTransaction)
    {
        SyncToDisk();
    }
    else
    {
        bool bDeferredSpatialIndexCreationBackup =
            m_bDeferredSpatialIndexCreation;
        m_bDeferredSpatialIndexCreation = false;
        SyncToDisk();
        m_bDeferredSpatialIndexCreation = bDeferredSpatialIndexCreationBackup;
    }
    ResetReading();
    return true;
}

/************************************************************************/
/*                  StartDeferredSpatialIndexUpdate()                   */
/************************************************************************/

bool OGRGeoPackageTableLayer::StartDeferredSpatialIndexUpdate()
{
    if (m_poFeatureDefn->GetGeomFieldCount() == 0)
        return true;

    RevertWorkaroundUpdate1TriggerIssue();

    m_aoRTreeTriggersSQL.clear();
    m_aoRTreeEntries.clear();

    const char *pszT = m_pszTableName;
    const char *pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    m_osRTreeName = "rtree_";
    m_osRTreeName += pszT;
    m_osRTreeName += "_";
    m_osRTreeName += pszC;

    char *pszSQL = sqlite3_mprintf(
        "SELECT sql FROM sqlite_master WHERE type = 'trigger' "
        "AND name IN ('%q', '%q', '%q', '%q', '%q', '%q', "
        "'%q', '%q', '%q')",
        (m_osRTreeName + "_insert").c_str(),
        (m_osRTreeName + "_update1").c_str(),
        (m_osRTreeName + "_update2").c_str(),
        (m_osRTreeName + "_update3").c_str(),
        (m_osRTreeName + "_update4").c_str(),
        // update5 replaces update3 in GPKG 1.4
        // cf https://github.com/opengeospatial/geopackage/pull/661
        (m_osRTreeName + "_update5").c_str(),
        // update6 and update7 replace update1 in GPKG 1.4
        // cf https://github.com/opengeospatial/geopackage/pull/661
        (m_osRTreeName + "_update6").c_str(),
        (m_osRTreeName + "_update7").c_str(),
        (m_osRTreeName + "_delete").c_str());
    auto oResult = SQLQuery(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if (oResult)
    {
        for (int iRecord = 0; iRecord < oResult->RowCount(); iRecord++)
        {
            const char *pszTriggerSQL = oResult->GetValue(0, iRecord);
            if (pszTriggerSQL)
            {
                m_aoRTreeTriggersSQL.push_back(pszTriggerSQL);
            }
        }
    }
    if (m_aoRTreeTriggersSQL.size() != 6 && m_aoRTreeTriggersSQL.size() != 7)
    {
        CPLDebug("GPKG", "Could not find expected RTree triggers");
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

    // CPLDebug("GPKG", "Insert %d features in spatial index",
    //          static_cast<int>(m_aoRTreeEntries.size()));

    const char *pszT = m_pszTableName;
    const char *pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();

    m_osRTreeName = "rtree_";
    m_osRTreeName += pszT;
    m_osRTreeName += "_";
    m_osRTreeName += pszC;

    char *pszSQL = sqlite3_mprintf("INSERT INTO \"%w\" VALUES (?,?,?,?,?)",
                                   m_osRTreeName.c_str());
    sqlite3_stmt *hInsertStmt = nullptr;
    if (sqlite3_prepare_v2(m_poDS->GetDB(), pszSQL, -1, &hInsertStmt,
                           nullptr) != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "failed to prepare SQL: %s",
                 pszSQL);
        sqlite3_free(pszSQL);
        m_aoRTreeEntries.clear();
        return false;
    }
    sqlite3_free(pszSQL);

    for (size_t i = 0; i < m_aoRTreeEntries.size(); ++i)
    {
        sqlite3_reset(hInsertStmt);

        sqlite3_bind_int64(hInsertStmt, 1, m_aoRTreeEntries[i].nId);
        sqlite3_bind_double(hInsertStmt, 2, m_aoRTreeEntries[i].fMinX);
        sqlite3_bind_double(hInsertStmt, 3, m_aoRTreeEntries[i].fMaxX);
        sqlite3_bind_double(hInsertStmt, 4, m_aoRTreeEntries[i].fMinY);
        sqlite3_bind_double(hInsertStmt, 5, m_aoRTreeEntries[i].fMaxY);
        int sqlite_err = sqlite3_step(hInsertStmt);
        if (sqlite_err != SQLITE_OK && sqlite_err != SQLITE_DONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "failed to execute insertion in RTree : %s",
                     sqlite3_errmsg(m_poDS->GetDB()));
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
    if (m_aoRTreeTriggersSQL.empty())
        return true;

    bool ret = FlushPendingSpatialIndexUpdate();

    RevertWorkaroundUpdate1TriggerIssue();

    for (const auto &osSQL : m_aoRTreeTriggersSQL)
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
    if (!m_bFeatureDefnCompleted)
        return OGRERR_NONE;

    if (m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE)
        return OGRERR_FAILURE;

    // Both are exclusive
    CreateSpatialIndexIfNecessary();
    if (!RunDeferredSpatialIndexUpdate())
        return OGRERR_FAILURE;
    RevertWorkaroundUpdate1TriggerIssue();

    /* Save metadata back to the database */
    SaveExtent();
    SaveTimestamp();

#ifdef ENABLE_GPKG_OGR_CONTENTS
    CreateFeatureCountTriggers();
#endif

    return OGRERR_NONE;
}

/************************************************************************/
/*                        StartTransaction()                            */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::StartTransaction()
{
    CancelAsyncNextArrowArray();
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
/*                      GetTotalFeatureCount()                          */
/************************************************************************/

GIntBig OGRGeoPackageTableLayer::GetTotalFeatureCount()
{
#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (m_nTotalFeatureCount < 0 && m_poDS->m_bHasGPKGOGRContents)
    {
        char *pszSQL =
            sqlite3_mprintf("SELECT feature_count FROM gpkg_ogr_contents WHERE "
                            "lower(table_name) = lower('%q') LIMIT 2",
                            m_pszTableName);
        auto oResult = SQLQuery(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        if (oResult && oResult->RowCount() == 1)
        {
            const char *pszFeatureCount = oResult->GetValue(0, 0);
            if (pszFeatureCount)
            {
                m_nTotalFeatureCount = CPLAtoGIntBig(pszFeatureCount);
            }
        }
    }
    return m_nTotalFeatureCount;
#else
    return 0;
#endif
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig OGRGeoPackageTableLayer::GetFeatureCount(int /*bForce*/)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (m_poFilterGeom == nullptr && m_pszAttrQueryString == nullptr)
    {
        const auto nCount = GetTotalFeatureCount();
        if (nCount >= 0)
            return nCount;
    }
#endif

    if (m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE)
        return 0;

    CancelAsyncNextArrowArray();

    /* Ignore bForce, because we always do a full count on the database */
    OGRErr err;
    CPLString soSQL;
    bool bUnregisterSQLFunction = false;
    if (m_bIsTable && m_poFilterGeom != nullptr &&
        m_pszAttrQueryString == nullptr && HasSpatialIndex())
    {
        OGREnvelope sEnvelope;

        m_poFilterGeom->getEnvelope(&sEnvelope);

        if (!CPLIsInf(sEnvelope.MinX) && !CPLIsInf(sEnvelope.MinY) &&
            !CPLIsInf(sEnvelope.MaxX) && !CPLIsInf(sEnvelope.MaxY))
        {
            soSQL.Printf("SELECT COUNT(*) FROM \"%s\" WHERE "
                         "maxx >= %.12f AND minx <= %.12f AND "
                         "maxy >= %.12f AND miny <= %.12f",
                         SQLEscapeName(m_osRTreeName).c_str(),
                         sEnvelope.MinX - 1e-11, sEnvelope.MaxX + 1e-11,
                         sEnvelope.MinY - 1e-11, sEnvelope.MaxY + 1e-11);

            if (OGRGeometryFactory::haveGEOS() &&
                !(m_bFilterIsEnvelope &&
                  wkbFlatten(
                      m_poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter)
                          ->GetType()) == wkbPoint))
            {
                bUnregisterSQLFunction = true;
                sqlite3_create_function(
                    m_poDS->hDB, "OGR_GPKG_Intersects_Spatial_Filter", 1,
                    SQLITE_UTF8, this, OGR_GPKG_Intersects_Spatial_Filter,
                    nullptr, nullptr);
                const char *pszC =
                    m_poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter)
                        ->GetNameRef();
                soSQL.Printf("SELECT COUNT(*) FROM \"%s\" m "
                             "JOIN \"%s\" r "
                             "ON m.\"%s\" = r.id WHERE "
                             "r.maxx >= %.12f AND r.minx <= %.12f AND "
                             "r.maxy >= %.12f AND r.miny <= %.12f AND "
                             "OGR_GPKG_Intersects_Spatial_Filter(m.\"%s\")",
                             SQLEscapeName(m_pszTableName).c_str(),
                             SQLEscapeName(m_osRTreeName).c_str(),
                             SQLEscapeName(m_osFIDForRTree).c_str(),
                             sEnvelope.MinX - 1e-11, sEnvelope.MaxX + 1e-11,
                             sEnvelope.MinY - 1e-11, sEnvelope.MaxY + 1e-11,
                             SQLEscapeName(pszC).c_str());
            }
        }
    }

    if (soSQL.empty())
    {
        if (!m_soFilter.empty())
            soSQL.Printf("SELECT Count(*) FROM \"%s\" WHERE %s",
                         SQLEscapeName(m_pszTableName).c_str(),
                         m_soFilter.c_str());
        else
            soSQL.Printf("SELECT Count(*) FROM \"%s\"",
                         SQLEscapeName(m_pszTableName).c_str());
    }

    /* Just run the query directly and get back integer */
    GIntBig iFeatureCount =
        SQLGetInteger64(m_poDS->GetDB(), soSQL.c_str(), &err);

    if (bUnregisterSQLFunction)
    {
        sqlite3_create_function(m_poDS->hDB,
                                "OGR_GPKG_Intersects_Spatial_Filter", 1,
                                SQLITE_UTF8, this, nullptr, nullptr, nullptr);
    }

    /* Generic implementation uses -1 for error condition, so we will too */
    if (err == OGRERR_NONE)
    {
#ifdef ENABLE_GPKG_OGR_CONTENTS
        if (m_bIsTable && m_poFilterGeom == nullptr &&
            m_pszAttrQueryString == nullptr)
        {
            m_nTotalFeatureCount = iFeatureCount;

            if (m_poDS->GetUpdate() && m_poDS->m_bHasGPKGOGRContents)
            {
                const char *pszCount =
                    CPLSPrintf(CPL_FRMT_GIB, m_nTotalFeatureCount);
                char *pszSQL = sqlite3_mprintf(
                    "UPDATE gpkg_ogr_contents SET feature_count = %s WHERE "
                    "lower(table_name )= lower('%q')",
                    pszCount, m_pszTableName);
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
/*                      GetExtentFromRTree()                            */
/************************************************************************/

static bool GetExtentFromRTree(sqlite3 *hDB, const std::string &osRTreeName,
                               double &minx, double &miny, double &maxx,
                               double &maxy)
{
    // Cf https://github.com/sqlite/sqlite/blob/master/ext/rtree/rtree.c
    // for the description of the content of the rtree _node table
    // We fetch the root node (nodeno = 1) and iterates over its cells, to
    // take the min/max of their minx/maxx/miny/maxy values.
    char *pszSQL = sqlite3_mprintf(
        "SELECT data FROM \"%w_node\" WHERE nodeno = 1", osRTreeName.c_str());
    sqlite3_stmt *hStmt = nullptr;
    CPL_IGNORE_RET_VAL(sqlite3_prepare_v2(hDB, pszSQL, -1, &hStmt, nullptr));
    sqlite3_free(pszSQL);
    bool bOK = false;
    if (hStmt)
    {
        if (sqlite3_step(hStmt) == SQLITE_ROW &&
            sqlite3_column_type(hStmt, 0) == SQLITE_BLOB)
        {
            const int nBytes = sqlite3_column_bytes(hStmt, 0);
            // coverity[tainted_data_return]
            const GByte *pabyData =
                static_cast<const GByte *>(sqlite3_column_blob(hStmt, 0));
            constexpr int BLOB_HEADER_SIZE = 4;
            if (nBytes > BLOB_HEADER_SIZE)
            {
                const int nCellCount = (pabyData[2] << 8) | pabyData[3];
                constexpr int SIZEOF_CELL = 24;  // int64_t + 4 float
                if (nCellCount >= 1 &&
                    nBytes >= BLOB_HEADER_SIZE + SIZEOF_CELL * nCellCount)
                {
                    minx = std::numeric_limits<double>::max();
                    miny = std::numeric_limits<double>::max();
                    maxx = -std::numeric_limits<double>::max();
                    maxy = -std::numeric_limits<double>::max();
                    size_t offset = BLOB_HEADER_SIZE;
                    for (int i = 0; i < nCellCount; ++i)
                    {
                        offset += sizeof(int64_t);

                        float fMinX;
                        memcpy(&fMinX, pabyData + offset, sizeof(float));
                        offset += sizeof(float);
                        CPL_MSBPTR32(&fMinX);
                        minx = std::min(minx, static_cast<double>(fMinX));

                        float fMaxX;
                        memcpy(&fMaxX, pabyData + offset, sizeof(float));
                        offset += sizeof(float);
                        CPL_MSBPTR32(&fMaxX);
                        maxx = std::max(maxx, static_cast<double>(fMaxX));

                        float fMinY;
                        memcpy(&fMinY, pabyData + offset, sizeof(float));
                        offset += sizeof(float);
                        CPL_MSBPTR32(&fMinY);
                        miny = std::min(miny, static_cast<double>(fMinY));

                        float fMaxY;
                        memcpy(&fMaxY, pabyData + offset, sizeof(float));
                        offset += sizeof(float);
                        CPL_MSBPTR32(&fMaxY);
                        maxy = std::max(maxy, static_cast<double>(fMaxY));
                    }

                    bOK = true;
                }
            }
        }
        sqlite3_finalize(hStmt);
    }
    return bOK;
}

/************************************************************************/
/*                        GetExtent()                                   */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    /* Extent already calculated! We're done. */
    if (m_poExtent != nullptr)
    {
        if (psExtent)
        {
            *psExtent = *m_poExtent;
        }
        return OGRERR_NONE;
    }

    if (m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE)
        return OGRERR_FAILURE;

    CancelAsyncNextArrowArray();

    if (m_poFeatureDefn->GetGeomFieldCount() && HasSpatialIndex() &&
        CPLTestBool(
            CPLGetConfigOption("OGR_GPKG_USE_RTREE_FOR_GET_EXTENT", "TRUE")))
    {
        if (GetExtentFromRTree(m_poDS->GetDB(), m_osRTreeName, psExtent->MinX,
                               psExtent->MinY, psExtent->MaxX, psExtent->MaxY))
        {
            m_poExtent = new OGREnvelope(*psExtent);
            m_bExtentChanged = true;
            SaveExtent();
            return OGRERR_NONE;
        }
        else
        {
            UpdateContentsToNullExtent();
            return OGRERR_FAILURE;
        }
    }

    /* User is OK with expensive calculation */
    if (bForce && m_poFeatureDefn->GetGeomFieldCount())
    {
        /* fall back to default implementation (scan all features) and save */
        /* the result for later */
        const char *pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
        char *pszSQL = sqlite3_mprintf(
            "SELECT MIN(ST_MinX(\"%w\")), MIN(ST_MinY(\"%w\")), "
            "MAX(ST_MaxX(\"%w\")), MAX(ST_MaxY(\"%w\")) FROM \"%w\" WHERE "
            "\"%w\" IS NOT NULL AND NOT ST_IsEmpty(\"%w\")",
            pszC, pszC, pszC, pszC, m_pszTableName, pszC, pszC);
        auto oResult = SQLQuery(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        delete m_poExtent;
        m_poExtent = nullptr;
        if (oResult && oResult->RowCount() == 1 &&
            oResult->GetValue(0, 0) != nullptr)
        {
            psExtent->MinX = CPLAtof(oResult->GetValue(0, 0));
            psExtent->MinY = CPLAtof(oResult->GetValue(1, 0));
            psExtent->MaxX = CPLAtof(oResult->GetValue(2, 0));
            psExtent->MaxY = CPLAtof(oResult->GetValue(3, 0));
            m_poExtent = new OGREnvelope(*psExtent);
            m_bExtentChanged = true;
            SaveExtent();
        }
        else
        {
            UpdateContentsToNullExtent();
            return OGRERR_FAILURE;  // we didn't get an extent
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
    if (m_poDS->GetUpdate())
    {
        char *pszSQL =
            sqlite3_mprintf("UPDATE gpkg_contents SET "
                            "min_x = NULL, min_y = NULL, "
                            "max_x = NULL, max_y = NULL "
                            "WHERE lower(table_name) = lower('%q') AND "
                            "Lower(data_type) = 'features'",
                            m_pszTableName);
        SQLCommand(m_poDS->GetDB(), pszSQL);
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

int OGRGeoPackageTableLayer::TestCapability(const char *pszCap)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (EQUAL(pszCap, OLCSequentialWrite))
    {
        return m_poDS->GetUpdate();
    }
    else if (EQUAL(pszCap, OLCCreateField) || EQUAL(pszCap, OLCDeleteField) ||
             EQUAL(pszCap, OLCAlterFieldDefn) ||
             EQUAL(pszCap, OLCAlterGeomFieldDefn) ||
             EQUAL(pszCap, OLCReorderFields) || EQUAL(pszCap, OLCRename))
    {
        return m_poDS->GetUpdate() && m_bIsTable;
    }
    else if (EQUAL(pszCap, OLCDeleteFeature) ||
             EQUAL(pszCap, OLCUpsertFeature) ||
             EQUAL(pszCap, OLCUpdateFeature) || EQUAL(pszCap, OLCRandomWrite))
    {
        return m_poDS->GetUpdate() && m_pszFidColumn != nullptr;
    }
    else if (EQUAL(pszCap, OLCRandomRead))
    {
        return m_pszFidColumn != nullptr;
    }
    else if (EQUAL(pszCap, OLCTransactions))
    {
        return TRUE;
    }
#ifdef ENABLE_GPKG_OGR_CONTENTS
    else if (EQUAL(pszCap, OLCFastFeatureCount))
    {
        return m_poFilterGeom == nullptr && m_pszAttrQueryString == nullptr &&
               m_nTotalFeatureCount >= 0;
    }
#endif
    else if (EQUAL(pszCap, OLCFastSpatialFilter))
    {
        return HasSpatialIndex() || m_bDeferredSpatialIndexCreation;
    }
    else if (EQUAL(pszCap, OLCFastSetNextByIndex))
    {
        // Fast may not be that true on large layers, but better than the
        // default implementation for sure...
        return TRUE;
    }
    else if (EQUAL(pszCap, OLCFastGetExtent))
    {
        return (m_poExtent != nullptr);
    }
    else if (EQUAL(pszCap, OLCCurveGeometries))
        return TRUE;
    else if (EQUAL(pszCap, OLCMeasuredGeometries))
        return TRUE;
    else if (EQUAL(pszCap, OLCZGeometries))
        return TRUE;
    if (EQUAL(pszCap, OLCFastGetExtent3D))
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
    if (m_bDeferredSpatialIndexCreation)
    {
        CreateSpatialIndex();
    }
}

/************************************************************************/
/*                       CreateSpatialIndex()                           */
/************************************************************************/

bool OGRGeoPackageTableLayer::CreateSpatialIndex(const char *pszTableName)
{
    OGRErr err;

    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();

    if (!CheckUpdatableTable("CreateSpatialIndex"))
        return false;

    if (m_bDropRTreeTable)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot run CreateSpatialIndex() after non-completed deferred "
                 "DropSpatialIndex()");
        return false;
    }

    if (m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE)
        return false;

    CancelAsyncNextArrowArray();

    m_bDeferredSpatialIndexCreation = false;

    if (m_pszFidColumn == nullptr)
        return false;

    if (HasSpatialIndex())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Spatial index already existing");
        return false;
    }

    if (m_poFeatureDefn->GetGeomFieldCount() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No geometry column");
        return false;
    }
    if (m_poDS->CreateExtensionsTableIfNecessary() != OGRERR_NONE)
        return false;

    const char *pszT = (pszTableName) ? pszTableName : m_pszTableName;
    const char *pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    const char *pszI = GetFIDColumn();

    m_osRTreeName = "rtree_";
    m_osRTreeName += pszT;
    m_osRTreeName += "_";
    m_osRTreeName += pszC;
    m_osFIDForRTree = m_pszFidColumn;

    bool bPopulateFromThreadRTree = false;
    if (m_bThreadRTreeStarted)
    {
        const bool bThreadHasFinished = m_oQueueRTreeEntries.empty();
        if (!m_aoRTreeEntries.empty())
            m_oQueueRTreeEntries.push(std::move(m_aoRTreeEntries));
        m_aoRTreeEntries = std::vector<GPKGRTreeEntry>();
        m_oQueueRTreeEntries.push(m_aoRTreeEntries);
        if (!bThreadHasFinished)
            CPLDebug("GPKG", "Waiting for background RTree building to finish");
        m_oThreadRTree.join();
        if (!bThreadHasFinished)
        {
            CPLDebug("GPKG", "Background RTree building finished");
        }
        m_bAllowedRTreeThread = false;
        m_bThreadRTreeStarted = false;

        if (m_hAsyncDBHandle)
        {
            sqlite3_close(m_hAsyncDBHandle);
            m_hAsyncDBHandle = nullptr;
        }
        if (m_bErrorDuringRTreeThread)
        {
            RemoveAsyncRTreeTempDB();
        }
        else
        {
            bPopulateFromThreadRTree = true;
        }
    }

    m_poDS->SoftStartTransaction();

    if (m_hRTree)
    {
        if (!FlushInMemoryRTree(m_poDS->GetDB(), m_osRTreeName.c_str()))
        {
            m_poDS->SoftRollbackTransaction();
            return false;
        }
    }
    else if (bPopulateFromThreadRTree)
    {
        /* Create virtual table */
        char *pszSQL = sqlite3_mprintf("CREATE VIRTUAL TABLE \"%w\" USING "
                                       "rtree(id, minx, maxx, miny, maxy)",
                                       m_osRTreeName.c_str());
        err = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        if (err != OGRERR_NONE)
        {
            m_poDS->SoftRollbackTransaction();
            return false;
        }

        pszSQL = sqlite3_mprintf(
            "DELETE FROM \"%w_node\";\n"
            "INSERT INTO \"%w_node\" SELECT * FROM \"%w\".my_rtree_node;\n"
            "INSERT INTO \"%w_rowid\" SELECT * FROM "
            "\"%w\".my_rtree_rowid;\n"
            "INSERT INTO \"%w_parent\" SELECT * FROM "
            "\"%w\".my_rtree_parent;\n",
            m_osRTreeName.c_str(), m_osRTreeName.c_str(),
            m_osAsyncDBAttachName.c_str(), m_osRTreeName.c_str(),
            m_osAsyncDBAttachName.c_str(), m_osRTreeName.c_str(),
            m_osAsyncDBAttachName.c_str());
        err = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        if (err != OGRERR_NONE)
        {
            m_poDS->SoftRollbackTransaction();
            RemoveAsyncRTreeTempDB();
            return false;
        }
    }
    else
    {
        /* Populate the RTree */
        const size_t nMaxRAMUsageAllowed = GetMaxRAMUsageAllowedForRTree();
        char *pszErrMsg = nullptr;

        struct ProgressCbk
        {
            static bool progressCbk(const char *pszMessage, void *)
            {
                CPLDebug("GPKG", "%s", pszMessage);
                return true;
            }
        };

        if (!gdal_sqlite_rtree_bl_from_feature_table(
                m_poDS->GetDB(), pszT, pszI, pszC, m_osRTreeName.c_str(), "id",
                "minx", "miny", "maxx", "maxy", nMaxRAMUsageAllowed, &pszErrMsg,
                ProgressCbk::progressCbk, nullptr))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "gdal_sqlite_rtree_bl_from_feature_table() failed "
                     "with %s",
                     pszErrMsg ? pszErrMsg : "(null)");
            m_poDS->SoftRollbackTransaction();
            sqlite3_free(pszErrMsg);
            return false;
        }
    }

    CPLString osSQL;

    /* Register the table in gpkg_extensions */
    char *pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_extensions "
        "(table_name,column_name,extension_name,definition,scope) "
        "VALUES ('%q', '%q', 'gpkg_rtree_index', "
        "'http://www.geopackage.org/spec120/#extension_rtree', 'write-only')",
        pszT, pszC);
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    /* Define Triggers to Maintain Spatial Index Values */
    osSQL += ";" + ReturnSQLCreateSpatialIndexTriggers(pszTableName, nullptr);

    err = SQLCommand(m_poDS->GetDB(), osSQL);
    if (err != OGRERR_NONE)
    {
        m_poDS->SoftRollbackTransaction();
        if (bPopulateFromThreadRTree)
        {
            RemoveAsyncRTreeTempDB();
        }
        return false;
    }

    m_poDS->SoftCommitTransaction();

    if (bPopulateFromThreadRTree)
    {
        RemoveAsyncRTreeTempDB();
    }

    m_bHasSpatialIndex = true;

    return true;
}

/************************************************************************/
/*                   WorkaroundUpdate1TriggerIssue()                    */
/************************************************************************/

void OGRGeoPackageTableLayer::WorkaroundUpdate1TriggerIssue()
{
    // Workaround issue of https://sqlite.org/forum/forumpost/8c8de6ff91
    // Basically the official _update1 spatial index trigger doesn't work
    // with current versions of SQLite when invoked from an UPSERT statement.
    // In GeoPackage 1.4, the update6 and update7 triggers replace update1

    if (m_bHasUpdate6And7Triggers || m_poFeatureDefn->GetGeomFieldCount() == 0)
        return;

    const char *pszT = m_pszTableName;
    const char *pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    const char *pszI = GetFIDColumn();

    CPLString osRTreeName = "rtree_";
    osRTreeName += pszT;
    osRTreeName += "_";
    osRTreeName += pszC;

    // Check if update6 and update7 triggers are there
    {
        char *pszSQL = sqlite3_mprintf(
            "SELECT * FROM sqlite_master WHERE type = 'trigger' "
            "AND name IN ('%q', '%q')",
            (m_osRTreeName + "_update6").c_str(),
            (m_osRTreeName + "_update7").c_str());
        auto oResult = SQLQuery(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        if (oResult && oResult->RowCount() == 2)
        {
            m_bHasUpdate6And7Triggers = true;
            return;
        }
    }

    char *pszSQL =
        sqlite3_mprintf("SELECT sql FROM sqlite_master WHERE type = 'trigger' "
                        "AND name = '%q'",
                        (m_osRTreeName + "_update1").c_str());
    auto oResult = SQLQuery(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if (oResult && oResult->RowCount() == 1)
    {
        const char *pszTriggerSQL = oResult->GetValue(0, 0);
        if (pszTriggerSQL)
        {
            m_osUpdate1Trigger = pszTriggerSQL;
        }
    }
    if (m_osUpdate1Trigger.empty())
        return;

    m_bUpdate1TriggerDisabled = true;

    pszSQL =
        sqlite3_mprintf("DROP TRIGGER \"%w_update1\"", osRTreeName.c_str());
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf(
        "CREATE TRIGGER \"%w_update6\" AFTER UPDATE OF \"%w\" "
        "ON \"%w\" "
        "WHEN OLD.\"%w\" = NEW.\"%w\" AND "
        "(NEW.\"%w\" NOTNULL AND NOT ST_IsEmpty(NEW.\"%w\")) AND "
        "(OLD.\"%w\" NOTNULL AND NOT ST_IsEmpty(OLD.\"%w\")) "
        "BEGIN "
        "UPDATE \"%w\" SET "
        "minx = ST_MinX(NEW.\"%w\"), maxx = ST_MaxX(NEW.\"%w\"),"
        "miny = ST_MinY(NEW.\"%w\"), maxy = ST_MaxY(NEW.\"%w\") "
        "WHERE id = NEW.\"%w\";"
        "END",
        osRTreeName.c_str(), pszC, pszT, pszI, pszI, pszC, pszC, pszC, pszC,
        osRTreeName.c_str(), pszC, pszC, pszC, pszC, pszI);
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf(
        "CREATE TRIGGER \"%w_update7\" AFTER UPDATE OF \"%w\" ON "
        "\"%w\" "
        "WHEN OLD.\"%w\" = NEW.\"%w\" AND "
        "(NEW.\"%w\" NOTNULL AND NOT ST_IsEmpty(NEW.\"%w\")) AND "
        "(OLD.\"%w\" ISNULL OR ST_IsEmpty(OLD.\"%w\")) "
        "BEGIN "
        "INSERT INTO \"%w\" VALUES ("
        "NEW.\"%w\","
        "ST_MinX(NEW.\"%w\"), ST_MaxX(NEW.\"%w\"),"
        "ST_MinY(NEW.\"%w\"), ST_MaxY(NEW.\"%w\")"
        "); "
        "END",
        osRTreeName.c_str(), pszC, pszT, pszI, pszI, pszC, pszC, pszC, pszC,
        osRTreeName.c_str(), pszI, pszC, pszC, pszC, pszC);
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
}

/************************************************************************/
/*                RevertWorkaroundUpdate1TriggerIssue()                 */
/************************************************************************/

void OGRGeoPackageTableLayer::RevertWorkaroundUpdate1TriggerIssue()
{
    if (!m_bUpdate1TriggerDisabled)
        return;
    m_bUpdate1TriggerDisabled = false;
    CPLAssert(!m_bHasUpdate6And7Triggers);

    const char *pszT = m_pszTableName;
    const char *pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();

    CPLString osRTreeName = "rtree_";
    osRTreeName += pszT;
    osRTreeName += "_";
    osRTreeName += pszC;

    char *pszSQL;

    SQLCommand(m_poDS->GetDB(), m_osUpdate1Trigger.c_str());
    m_osUpdate1Trigger.clear();

    pszSQL =
        sqlite3_mprintf("DROP TRIGGER \"%w_update6\"", osRTreeName.c_str());
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL =
        sqlite3_mprintf("DROP TRIGGER \"%w_update7\"", osRTreeName.c_str());
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
}

/************************************************************************/
/*                ReturnSQLCreateSpatialIndexTriggers()                 */
/************************************************************************/

CPLString OGRGeoPackageTableLayer::ReturnSQLCreateSpatialIndexTriggers(
    const char *pszTableName, const char *pszGeomColName)
{
    char *pszSQL;
    CPLString osSQL;

    const char *pszT = (pszTableName) ? pszTableName : m_pszTableName;
    const char *pszC = (pszGeomColName)
                           ? pszGeomColName
                           : m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    const char *pszI = GetFIDColumn();

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
        osRTreeName.c_str(), pszT, pszC, pszC, osRTreeName.c_str(), pszI, pszC,
        pszC, pszC, pszC);
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    if (m_poDS->m_nApplicationId == GPKG_APPLICATION_ID &&
        m_poDS->m_nUserVersion >= GPKG_1_4_VERSION)
    {
        /* Conditions: Update a non-empty geometry with another non-empty geometry
           Actions   : Replace record from R-tree
        */
        pszSQL = sqlite3_mprintf(
            "CREATE TRIGGER \"%w_update6\" AFTER UPDATE OF \"%w\" "
            "ON \"%w\" "
            "WHEN OLD.\"%w\" = NEW.\"%w\" AND "
            "(NEW.\"%w\" NOTNULL AND NOT ST_IsEmpty(NEW.\"%w\")) AND "
            "(OLD.\"%w\" NOTNULL AND NOT ST_IsEmpty(OLD.\"%w\")) "
            "BEGIN "
            "UPDATE \"%w\" SET "
            "minx = ST_MinX(NEW.\"%w\"), maxx = ST_MaxX(NEW.\"%w\"),"
            "miny = ST_MinY(NEW.\"%w\"), maxy = ST_MaxY(NEW.\"%w\") "
            "WHERE id = NEW.\"%w\";"
            "END",
            osRTreeName.c_str(), pszC, pszT, pszI, pszI, pszC, pszC, pszC, pszC,
            osRTreeName.c_str(), pszC, pszC, pszC, pszC, pszI);
        osSQL += ";";
        osSQL += pszSQL;
        sqlite3_free(pszSQL);

        /* Conditions: Update a null/empty geometry with a non-empty geometry
           Actions : Insert record into R-tree
        */
        pszSQL = sqlite3_mprintf(
            "CREATE TRIGGER \"%w_update7\" AFTER UPDATE OF \"%w\" ON "
            "\"%w\" "
            "WHEN OLD.\"%w\" = NEW.\"%w\" AND "
            "(NEW.\"%w\" NOTNULL AND NOT ST_IsEmpty(NEW.\"%w\")) AND "
            "(OLD.\"%w\" ISNULL OR ST_IsEmpty(OLD.\"%w\")) "
            "BEGIN "
            "INSERT INTO \"%w\" VALUES ("
            "NEW.\"%w\","
            "ST_MinX(NEW.\"%w\"), ST_MaxX(NEW.\"%w\"),"
            "ST_MinY(NEW.\"%w\"), ST_MaxY(NEW.\"%w\")"
            "); "
            "END",
            osRTreeName.c_str(), pszC, pszT, pszI, pszI, pszC, pszC, pszC, pszC,
            osRTreeName.c_str(), pszI, pszC, pszC, pszC, pszC);
        osSQL += ";";
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
    }
    else
    {
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
            osRTreeName.c_str(), pszC, pszT, pszI, pszI, pszC, pszC,
            osRTreeName.c_str(), pszI, pszC, pszC, pszC, pszC);
        osSQL += ";";
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
    }

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
        osRTreeName.c_str(), pszC, pszT, pszI, pszI, pszC, pszC,
        osRTreeName.c_str(), pszI);
    osSQL += ";";
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    /* Conditions: Update of any column
                    Row ID change
                    Non-empty geometry
        Actions   : Remove record from rtree for old <i>
                    Insert record into rtree for new <i> */
    pszSQL =
        sqlite3_mprintf("CREATE TRIGGER \"%w_%s\" AFTER UPDATE ON \"%w\" "
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
                        osRTreeName.c_str(),
                        (m_poDS->m_nApplicationId == GPKG_APPLICATION_ID &&
                         m_poDS->m_nUserVersion >= GPKG_1_4_VERSION)
                            ? "update5"
                            : "update3",
                        pszT, pszI, pszI, pszC, pszC, osRTreeName.c_str(), pszI,
                        osRTreeName.c_str(), pszI, pszC, pszC, pszC, pszC);
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
        osRTreeName.c_str(), pszT, pszI, pszI, pszC, pszC, osRTreeName.c_str(),
        pszI, pszI);
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
        osRTreeName.c_str(), pszT, pszC, osRTreeName.c_str(), pszI);
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
    const std::map<CPLString, std::vector<GPKGExtensionDesc>> &oMap =
        m_poDS->GetUnknownExtensionsTableSpecific();
    std::map<CPLString, std::vector<GPKGExtensionDesc>>::const_iterator oIter =
        oMap.find(CPLString(m_pszTableName).toupper());
    if (oIter != oMap.end())
    {
        for (size_t i = 0; i < oIter->second.size(); i++)
        {
            const char *pszExtName = oIter->second[i].osExtensionName.c_str();
            const char *pszDefinition = oIter->second[i].osDefinition.c_str();
            const char *pszScope = oIter->second[i].osScope.c_str();
            if (m_poDS->GetUpdate() && EQUAL(pszScope, "write-only"))
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Layer %s relies on the '%s' (%s) extension that should "
                    "be implemented for safe write-support, but is not "
                    "currently. "
                    "Update of that layer are strongly discouraged to avoid "
                    "corruption.",
                    GetName(), pszExtName, pszDefinition);
            }
            else if (m_poDS->GetUpdate() && EQUAL(pszScope, "read-write"))
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Layer %s relies on the '%s' (%s) extension that should "
                    "be implemented in order to read/write it safely, but is "
                    "not currently. "
                    "Some data may be missing while reading that layer, and "
                    "updates are strongly discouraged.",
                    GetName(), pszExtName, pszDefinition);
            }
            else if (EQUAL(pszScope, "read-write") &&
                     // None of the NGA extensions at
                     // http://ngageoint.github.io/GeoPackage/docs/extensions/
                     // affect read-only scenarios
                     !STARTS_WITH(pszExtName, "nga_"))
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Layer %s relies on the '%s' (%s) extension that should "
                    "be implemented in order to read it safely, but is not "
                    "currently. "
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
    const OGRGeometry *poGeom)
{
    bool bRet = true;
    if (poGeom != nullptr)
    {
        OGRwkbGeometryType eGType = wkbFlatten(poGeom->getGeometryType());
        if (eGType >= wkbGeometryCollection)
        {
            if (eGType > wkbGeometryCollection)
                CreateGeometryExtensionIfNecessary(eGType);
            const OGRGeometryCollection *poGC =
                dynamic_cast<const OGRGeometryCollection *>(poGeom);
            if (poGC != nullptr)
            {
                const int nSubGeoms = poGC->getNumGeometries();
                for (int i = 0; i < nSubGeoms; i++)
                {
                    bRet &= CreateGeometryExtensionIfNecessary(
                        poGC->getGeometryRef(i));
                }
            }
        }
    }
    return bRet;
}

/************************************************************************/
/*                     CreateGeometryExtensionIfNecessary()             */
/************************************************************************/

bool OGRGeoPackageTableLayer::CreateGeometryExtensionIfNecessary(
    OGRwkbGeometryType eGType)
{
    eGType = wkbFlatten(eGType);
    CPLAssert(eGType > wkbGeometryCollection && eGType <= wkbTriangle);
    if (m_abHasGeometryExtension[eGType])
        return true;

    if (m_poDS->CreateExtensionsTableIfNecessary() != OGRERR_NONE)
        return false;

    const char *pszT = m_pszTableName;
    const char *pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    const char *pszGeometryType = m_poDS->GetGeometryTypeString(eGType);

    // Check first if the extension isn't registered
    char *pszSQL = sqlite3_mprintf(
        "SELECT 1 FROM gpkg_extensions WHERE lower(table_name) = lower('%q') "
        "AND "
        "lower(column_name) = lower('%q') AND extension_name = 'gpkg_geom_%s'",
        pszT, pszC, pszGeometryType);
    const bool bExists = SQLGetInteger(m_poDS->GetDB(), pszSQL, nullptr) == 1;
    sqlite3_free(pszSQL);

    if (!bExists)
    {
        if (eGType == wkbPolyhedralSurface || eGType == wkbTIN ||
            eGType == wkbTriangle)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Registering non-standard gpkg_geom_%s extension",
                     pszGeometryType);
        }

        /* Register the table in gpkg_extensions */
        pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_extensions "
            "(table_name,column_name,extension_name,definition,scope) "
            "VALUES ('%q', '%q', 'gpkg_geom_%s', "
            "'http://www.geopackage.org/spec120/#extension_geometry_types', "
            "'read-write')",
            pszT, pszC, pszGeometryType);
        OGRErr err = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        if (err != OGRERR_NONE)
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
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (m_bHasSpatialIndex >= 0)
        return CPL_TO_BOOL(m_bHasSpatialIndex);
    m_bHasSpatialIndex = false;

    if (m_pszFidColumn == nullptr ||
        m_poFeatureDefn->GetGeomFieldCount() == 0 ||
        !m_poDS->HasExtensionsTable())
        return false;

    const char *pszT = m_pszTableName;
    const char *pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    const CPLString osRTreeName(
        CPLString("rtree_").append(pszT).append("_").append(pszC));
    const std::map<CPLString, CPLString> &oMap =
        m_poDS->GetNameTypeMapFromSQliteMaster();
    if (oMap.find(CPLString(osRTreeName).toupper()) != oMap.end())
    {
        m_bHasSpatialIndex = true;
        m_osRTreeName = osRTreeName;
        m_osFIDForRTree = m_pszFidColumn;
    }

    // Add heuristics to try to detect corrupted RTree generated by GDAL 3.6.0
    // Cf https://github.com/OSGeo/gdal/pull/6911
    if (m_bHasSpatialIndex)
    {
        const auto nFC = GetTotalFeatureCount();
        if (nFC >= atoi(CPLGetConfigOption(
                       "OGR_GPKG_THRESHOLD_DETECT_BROKEN_RTREE", "100000")))
        {
            CPLString osSQL = "SELECT 1 FROM \"";
            osSQL += SQLEscapeName(pszT);
            osSQL += "\" WHERE \"";
            osSQL += SQLEscapeName(GetFIDColumn());
            osSQL += "\" = ";
            osSQL += CPLSPrintf(CPL_FRMT_GIB, nFC);
            osSQL += " AND \"";
            osSQL += SQLEscapeName(pszC);
            osSQL += "\" IS NOT NULL AND NOT ST_IsEmpty(\"";
            osSQL += SQLEscapeName(pszC);
            osSQL += "\")";
            if (SQLGetInteger(m_poDS->GetDB(), osSQL, nullptr) == 1)
            {
                osSQL = "SELECT 1 FROM \"";
                osSQL += SQLEscapeName(m_osRTreeName);
                osSQL += "\" WHERE id = ";
                osSQL += CPLSPrintf(CPL_FRMT_GIB, nFC);
                if (SQLGetInteger(m_poDS->GetDB(), osSQL, nullptr) == 0)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Spatial index (perhaps created with GDAL 3.6.0) "
                             "of table %s is corrupted. Disabling its use. "
                             "This file should be recreated or its spatial "
                             "index recreated",
                             m_pszTableName);
                    m_bHasSpatialIndex = false;
                }
            }
        }
    }

    return CPL_TO_BOOL(m_bHasSpatialIndex);
}

/************************************************************************/
/*                        DropSpatialIndex()                            */
/************************************************************************/

bool OGRGeoPackageTableLayer::DropSpatialIndex(bool bCalledFromSQLFunction)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (!CheckUpdatableTable("DropSpatialIndex"))
        return false;

    if (m_bDropRTreeTable)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot run DropSpatialIndex() after non-completed deferred "
                 "DropSpatialIndex()");
        return false;
    }

    if (!HasSpatialIndex())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Spatial index not existing");
        return false;
    }

    const char *pszT = m_pszTableName;
    const char *pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    {
        char *pszSQL = sqlite3_mprintf(
            "DELETE FROM gpkg_extensions WHERE lower(table_name)=lower('%q') "
            "AND lower(column_name)=lower('%q') AND "
            "extension_name='gpkg_rtree_index'",
            pszT, pszC);
        SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }

    if (bCalledFromSQLFunction)
    {
        /* We cannot drop a table from a SQLite function call, so we just */
        /* memorize that we will have to delete the table later */
        m_bDropRTreeTable = true;
    }
    else
    {
        char *pszSQL =
            sqlite3_mprintf("DROP TABLE \"%w\"", m_osRTreeName.c_str());
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
    if (m_bDropRTreeTable)
    {
        OGRGeoPackageTableLayer::ResetReading();

        char *pszSQL =
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
    char *pszSQL = sqlite3_mprintf(
        "DROP TRIGGER \"%w_insert\";"
        "DROP TRIGGER IF EXISTS \"%w_update1\";"  // replaced by update6 and update7 in GPKG 1.4
        "DROP TRIGGER \"%w_update2\";"
        "DROP TRIGGER IF EXISTS \"%w_update3\";"  // replace by update5 in GPKG 1.4
        "DROP TRIGGER \"%w_update4\";"
        "DROP TRIGGER IF EXISTS \"%w_update5\";"  // replace update3 in GPKG 1.4
        "DROP TRIGGER IF EXISTS \"%w_update6\";"  // replace update1 in GPKG 1.4
        "DROP TRIGGER IF EXISTS \"%w_update7\";"  // replace update1 in GPKG 1.4
        "DROP TRIGGER \"%w_delete\";",
        m_osRTreeName.c_str(), m_osRTreeName.c_str(), m_osRTreeName.c_str(),
        m_osRTreeName.c_str(), m_osRTreeName.c_str(), m_osRTreeName.c_str(),
        m_osRTreeName.c_str(), m_osRTreeName.c_str(), m_osRTreeName.c_str());
    CPLString osSQL(pszSQL);
    sqlite3_free(pszSQL);

    return osSQL;
}

/************************************************************************/
/*                           Rename()                                   */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::Rename(const char *pszDstTableName)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (!CheckUpdatableTable("Rename"))
        return OGRERR_FAILURE;

    ResetReading();
    SyncToDisk();

    char *pszSQL = sqlite3_mprintf(
        "SELECT 1 FROM sqlite_master WHERE lower(name) = lower('%q') "
        "AND type IN ('table', 'view')",
        pszDstTableName);
    const bool bAlreadyExists =
        SQLGetInteger(m_poDS->GetDB(), pszSQL, nullptr) == 1;
    sqlite3_free(pszSQL);
    if (bAlreadyExists)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Table %s already exists",
                 pszDstTableName);
        return OGRERR_FAILURE;
    }

    // Temporary remove foreign key checks
    const GPKGTemporaryForeignKeyCheckDisabler
        oGPKGTemporaryForeignKeyCheckDisabler(m_poDS);

    if (m_poDS->SoftStartTransaction() != OGRERR_NONE)
    {
        return OGRERR_FAILURE;
    }

#ifdef ENABLE_GPKG_OGR_CONTENTS
    DisableFeatureCountTriggers(false);
#endif

    CPLString osSQL;

    pszSQL = sqlite3_mprintf(
        "UPDATE gpkg_geometry_columns SET table_name = '%q' WHERE "
        "lower(table_name )= lower('%q');",
        pszDstTableName, m_pszTableName);
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    // Rename the identifier if it defaulted to the table name
    pszSQL = sqlite3_mprintf(
        "UPDATE gpkg_contents SET identifier = '%q' WHERE "
        "lower(table_name) = lower('%q') AND identifier = '%q';",
        pszDstTableName, m_pszTableName, m_pszTableName);
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("UPDATE gpkg_contents SET table_name = '%q' WHERE "
                             "lower(table_name )= lower('%q');",
                             pszDstTableName, m_pszTableName);
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    if (m_poDS->HasExtensionsTable())
    {
        pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_extensions SET table_name = '%q' WHERE "
            "lower(table_name )= lower('%q');",
            pszDstTableName, m_pszTableName);
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
    }

    if (m_poDS->HasMetadataTables())
    {
        pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_metadata_reference SET table_name = '%q' WHERE "
            "lower(table_name )= lower('%q');",
            pszDstTableName, m_pszTableName);
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
    }

    if (m_poDS->HasDataColumnsTable())
    {
        pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_data_columns SET table_name = '%q' WHERE "
            "lower(table_name )= lower('%q');",
            pszDstTableName, m_pszTableName);
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
    }

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (m_poDS->m_bHasGPKGOGRContents)
    {
        pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_ogr_contents SET table_name = '%q' WHERE "
            "lower(table_name )= lower('%q');",
            pszDstTableName, m_pszTableName);
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
    }
#endif

    if (m_poDS->HasGpkgextRelationsTable())
    {
        pszSQL = sqlite3_mprintf(
            "UPDATE gpkgext_relations SET base_table_name = '%q' WHERE "
            "lower(base_table_name )= lower('%q');",
            pszDstTableName, m_pszTableName);
        osSQL += pszSQL;
        sqlite3_free(pszSQL);

        pszSQL = sqlite3_mprintf(
            "UPDATE gpkgext_relations SET related_table_name = '%q' WHERE "
            "lower(related_table_name )= lower('%q');",
            pszDstTableName, m_pszTableName);
        osSQL += pszSQL;
        ;
        sqlite3_free(pszSQL);

        pszSQL = sqlite3_mprintf(
            "UPDATE gpkgext_relations SET mapping_table_name = '%q' WHERE "
            "lower(mapping_table_name )= lower('%q');",
            pszDstTableName, m_pszTableName);
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
    }

    if (m_poDS->HasQGISLayerStyles())
    {
        pszSQL =
            sqlite3_mprintf("UPDATE layer_styles SET f_table_name = '%q' WHERE "
                            "f_table_name = '%q';",
                            pszDstTableName, m_pszTableName);
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
    }

    pszSQL = sqlite3_mprintf("ALTER TABLE \"%w\" RENAME TO \"%w\";",
                             m_pszTableName, pszDstTableName);
    osSQL += pszSQL;
    sqlite3_free(pszSQL);

    const bool bHasSpatialIndex = HasSpatialIndex();
    CPLString osRTreeNameNew;
    if (bHasSpatialIndex)
    {
        osRTreeNameNew = "rtree_";
        osRTreeNameNew += pszDstTableName;
        osRTreeNameNew += "_";
        osRTreeNameNew += m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();

        osSQL += ReturnSQLDropSpatialIndexTriggers();
        osSQL += ';';

        pszSQL = sqlite3_mprintf("ALTER TABLE \"%w\" RENAME TO \"%w\";",
                                 m_osRTreeName.c_str(), osRTreeNameNew.c_str());
        osSQL += pszSQL;
        sqlite3_free(pszSQL);

        osSQL += ReturnSQLCreateSpatialIndexTriggers(pszDstTableName, nullptr);
    }

    OGRErr eErr = SQLCommand(m_poDS->GetDB(), osSQL);

    // Check foreign key integrity
    if (eErr == OGRERR_NONE)
    {
        eErr = m_poDS->PragmaCheck("foreign_key_check", "", 0);
    }

    if (eErr == OGRERR_NONE)
    {
#ifdef ENABLE_GPKG_OGR_CONTENTS
        CreateFeatureCountTriggers(pszDstTableName);
#endif

        eErr = m_poDS->SoftCommitTransaction();
        if (eErr == OGRERR_NONE)
        {
            m_poDS->RemoveTableFromSQLiteMasterCache(m_pszTableName);

            CPLFree(m_pszTableName);
            m_pszTableName = CPLStrdup(pszDstTableName);

            if (bHasSpatialIndex)
            {
                m_poDS->RemoveTableFromSQLiteMasterCache(m_osRTreeName);
                m_osRTreeName = std::move(osRTreeNameNew);
            }
        }
    }
    else
    {
        m_poDS->SoftRollbackTransaction();
    }

    if (eErr == OGRERR_NONE)
    {
        m_poDS->ClearCachedRelationships();

        SetDescription(pszDstTableName);
        whileUnsealing(m_poFeatureDefn)->SetName(pszDstTableName);
    }

    return eErr;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRGeoPackageTableLayer::SetSpatialFilter(OGRGeometry *poGeomIn)

{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (InstallFilter(poGeomIn))
    {
        BuildWhere();

        ResetReading();
    }
}

/************************************************************************/
/*                        HasFastSpatialFilter()                        */
/************************************************************************/

bool OGRGeoPackageTableLayer::HasFastSpatialFilter(int m_iGeomColIn)
{
    if (m_iGeomColIn < 0 ||
        m_iGeomColIn >= m_poFeatureDefn->GetGeomFieldCount())
        return false;
    return HasSpatialIndex();
}

/************************************************************************/
/*                           GetSpatialWhere()                          */
/************************************************************************/

CPLString OGRGeoPackageTableLayer::GetSpatialWhere(int m_iGeomColIn,
                                                   OGRGeometry *poFilterGeom)
{
    CPLString osSpatialWHERE;

    if (m_iGeomColIn < 0 ||
        m_iGeomColIn >= m_poFeatureDefn->GetGeomFieldCount())
        return osSpatialWHERE;

    if (poFilterGeom != nullptr)
    {
        OGREnvelope sEnvelope;

        poFilterGeom->getEnvelope(&sEnvelope);

        const char *pszC =
            m_poFeatureDefn->GetGeomFieldDefn(m_iGeomColIn)->GetNameRef();

        if (CPLIsInf(sEnvelope.MinX) && sEnvelope.MinX < 0 &&
            CPLIsInf(sEnvelope.MinY) && sEnvelope.MinY < 0 &&
            CPLIsInf(sEnvelope.MaxX) && sEnvelope.MaxX > 0 &&
            CPLIsInf(sEnvelope.MaxY) && sEnvelope.MaxY > 0)
        {
            osSpatialWHERE.Printf(
                "(\"%s\" IS NOT NULL AND NOT ST_IsEmpty(\"%s\"))",
                SQLEscapeName(pszC).c_str(), SQLEscapeName(pszC).c_str());
            return osSpatialWHERE;
        }

        bool bUseSpatialIndex = true;
        if (m_poExtent && sEnvelope.MinX <= m_poExtent->MinX &&
            sEnvelope.MinY <= m_poExtent->MinY &&
            sEnvelope.MaxX >= m_poExtent->MaxX &&
            sEnvelope.MaxY >= m_poExtent->MaxY)
        {
            // Selecting from spatial filter on whole extent can be rather
            // slow. So use function based filtering, just in case the
            // advertized global extent might be wrong. Otherwise we might
            // just discard completely the spatial filter.
            bUseSpatialIndex = false;
        }

        if (bUseSpatialIndex && HasSpatialIndex())
        {
            osSpatialWHERE.Printf(
                "\"%s\" IN ( SELECT id FROM \"%s\" WHERE "
                "maxx >= %.12f AND minx <= %.12f AND "
                "maxy >= %.12f AND miny <= %.12f)",
                SQLEscapeName(m_osFIDForRTree).c_str(),
                SQLEscapeName(m_osRTreeName).c_str(), sEnvelope.MinX - 1e-11,
                sEnvelope.MaxX + 1e-11, sEnvelope.MinY - 1e-11,
                sEnvelope.MaxY + 1e-11);
        }
        else
        {
            if (HasSpatialIndex())
            {
                // If we do have a spatial index, and our filter contains the
                // bounding box of the RTree, then just filter on non-null
                // non-empty geometries.
                double minx, miny, maxx, maxy;
                if (GetExtentFromRTree(m_poDS->GetDB(), m_osRTreeName, minx,
                                       miny, maxx, maxy) &&
                    sEnvelope.MinX <= minx && sEnvelope.MinY <= miny &&
                    sEnvelope.MaxX >= maxx && sEnvelope.MaxY >= maxy)
                {
                    osSpatialWHERE.Printf(
                        "(\"%s\" IS NOT NULL AND NOT ST_IsEmpty(\"%s\"))",
                        SQLEscapeName(pszC).c_str(),
                        SQLEscapeName(pszC).c_str());
                    return osSpatialWHERE;
                }
            }

            /* A bit inefficient but still faster than OGR filtering */
            osSpatialWHERE.Printf(
                "ST_EnvelopesIntersects(\"%s\", %.12f, %.12f, %.12f, %.12f)",
                SQLEscapeName(pszC).c_str(), sEnvelope.MinX - 1e-11,
                sEnvelope.MinY - 1e-11, sEnvelope.MaxX + 1e-11,
                sEnvelope.MaxY + 1e-11);
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

    CPLString osSpatialWHERE =
        GetSpatialWhere(m_iGeomFieldFilter, m_poFilterGeom);
    if (!osSpatialWHERE.empty())
    {
        m_soFilter += osSpatialWHERE;
    }

    if (!osQuery.empty())
    {
        if (m_soFilter.empty())
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

void OGRGeoPackageTableLayer::SetOpeningParameters(
    const char *pszTableName, const char *pszObjectType, bool bIsInGpkgContents,
    bool bIsSpatial, const char *pszGeomColName, const char *pszGeomType,
    bool bHasZ, bool bHasM)
{
    CPLFree(m_pszTableName);
    m_pszTableName = CPLStrdup(pszTableName);
    m_bIsTable = EQUAL(pszObjectType, "table");
    m_bIsInGpkgContents = bIsInGpkgContents;
    m_bIsSpatial = bIsSpatial;
    if (pszGeomType)
    {
        OGRwkbGeometryType eType =
            GPkgGeometryTypeToWKB(pszGeomType, bHasZ, bHasM);
        m_poFeatureDefn->SetGeomType(eType);
        if (eType != wkbNone)
        {
            m_poFeatureDefn->GetGeomFieldDefn(0)->SetName(pszGeomColName);
        }
    }
}

/************************************************************************/
/*                        SetCreationParameters()                       */
/************************************************************************/

void OGRGeoPackageTableLayer::SetCreationParameters(
    OGRwkbGeometryType eGType, const char *pszGeomColumnName, int bGeomNullable,
    const OGRSpatialReference *poSRS, const char *pszSRID,
    const OGRGeomCoordinatePrecision &oCoordPrec, bool bDiscardCoordLSB,
    bool bUndoDiscardCoordLSBOnReading, const char *pszFIDColumnName,
    const char *pszIdentifier, const char *pszDescription)
{
    m_bIsSpatial = eGType != wkbNone;
    m_bIsInGpkgContents =
        m_bIsSpatial ||
        !m_poDS->HasNonSpatialTablesNonRegisteredInGpkgContents();
    m_bFeatureDefnCompleted = true;
    m_bDeferredCreation = true;
    m_bTableCreatedInTransaction = m_poDS->IsInTransaction();
    m_bHasTriedDetectingFID64 = true;
    m_pszFidColumn = CPLStrdup(pszFIDColumnName);
    m_bUndoDiscardCoordLSBOnReading = bUndoDiscardCoordLSBOnReading;

    if (eGType != wkbNone)
    {
        m_nZFlag = wkbHasZ(eGType) ? 1 : 0;
        m_nMFlag = wkbHasM(eGType) ? 1 : 0;
        OGRGeomFieldDefn oGeomFieldDefn(pszGeomColumnName, eGType);

        oGeomFieldDefn.SetSpatialRef(poSRS);
        if (pszSRID)
        {
            m_iSrs = atoi(pszSRID);
            if (m_iSrs == GDALGeoPackageDataset::FIRST_CUSTOM_SRSID - 1)
            {
                m_iSrs = m_poDS->GetSrsId(nullptr);
                oGeomFieldDefn.SetSpatialRef(nullptr);
            }
            else
            {
                auto poGotSRS =
                    m_poDS->GetSpatialRef(m_iSrs, /* bFallbackToEPSG = */ false,
                                          /* bEmitErrorIfNotFound = */ false);
                if (poGotSRS)
                {
                    oGeomFieldDefn.SetSpatialRef(poGotSRS);
                    poGotSRS->Release();
                }
                else
                {
                    bool bOK = false;
                    OGRSpatialReference *poSRSTmp = new OGRSpatialReference();
                    if (m_iSrs < 32767)
                    {
                        CPLErrorHandlerPusher oErrorHandler(
                            CPLQuietErrorHandler);
                        CPLErrorStateBackuper oBackuper;
                        if (poSRSTmp->importFromEPSG(m_iSrs) == OGRERR_NONE)
                        {
                            bOK = true;
                            poSRSTmp->SetAxisMappingStrategy(
                                OAMS_TRADITIONAL_GIS_ORDER);
                            m_iSrs = m_poDS->GetSrsId(poSRSTmp);
                            oGeomFieldDefn.SetSpatialRef(poSRSTmp);
                        }
                    }
                    if (!bOK)
                    {
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "No entry in gpkg_spatial_ref_sys matching SRID=%s",
                            pszSRID);
                    }
                    poSRSTmp->Release();
                }
            }
        }
        else
        {
            m_iSrs = m_poDS->GetSrsId(poSRS);
        }
        oGeomFieldDefn.SetNullable(bGeomNullable);
        oGeomFieldDefn.SetCoordinatePrecision(oCoordPrec);

        if (bDiscardCoordLSB)
            m_sBinaryPrecision.SetFrom(oCoordPrec);

        // Save coordinate precision in gpkg_metadata/gpkg_metadata_reference
        if ((oCoordPrec.dfXYResolution != OGRGeomCoordinatePrecision::UNKNOWN ||
             oCoordPrec.dfZResolution != OGRGeomCoordinatePrecision::UNKNOWN ||
             oCoordPrec.dfMResolution != OGRGeomCoordinatePrecision::UNKNOWN) &&
            (m_poDS->HasMetadataTables() || m_poDS->CreateMetadataTables()))
        {
            std::string osCoordPrecision = "<CoordinatePrecision ";
            if (oCoordPrec.dfXYResolution !=
                OGRGeomCoordinatePrecision::UNKNOWN)
                osCoordPrecision += CPLSPrintf(" xy_resolution=\"%g\"",
                                               oCoordPrec.dfXYResolution);
            if (oCoordPrec.dfZResolution != OGRGeomCoordinatePrecision::UNKNOWN)
                osCoordPrecision += CPLSPrintf(" z_resolution=\"%g\"",
                                               oCoordPrec.dfZResolution);
            if (oCoordPrec.dfMResolution != OGRGeomCoordinatePrecision::UNKNOWN)
                osCoordPrecision += CPLSPrintf(" m_resolution=\"%g\"",
                                               oCoordPrec.dfMResolution);
            osCoordPrecision += CPLSPrintf(" discard_coord_lsb=\"%s\"",
                                           bDiscardCoordLSB ? "true" : "false");
            osCoordPrecision +=
                CPLSPrintf(" undo_discard_coord_lsb_on_reading=\"%s\"",
                           m_bUndoDiscardCoordLSBOnReading ? "true" : "false");
            osCoordPrecision += " />";

            char *pszSQL = sqlite3_mprintf(
                "INSERT INTO gpkg_metadata "
                "(md_scope, md_standard_uri, mime_type, metadata) VALUES "
                "('dataset','http://gdal.org','text/xml','%q')",
                osCoordPrecision.c_str());
            CPL_IGNORE_RET_VAL(SQLCommand(m_poDS->GetDB(), pszSQL));
            sqlite3_free(pszSQL);

            const sqlite_int64 nFID =
                sqlite3_last_insert_rowid(m_poDS->GetDB());
            pszSQL = sqlite3_mprintf(
                "INSERT INTO gpkg_metadata_reference (reference_scope, "
                "table_name, column_name, timestamp, md_file_id) VALUES "
                "('column', '%q', '%q', %s, %d)",
                m_pszTableName, pszGeomColumnName,
                m_poDS->GetCurrentDateEscapedSQL().c_str(),
                static_cast<int>(nFID));
            CPL_IGNORE_RET_VAL(SQLCommand(m_poDS->GetDB(), pszSQL));
            sqlite3_free(pszSQL);
        }

        m_poFeatureDefn->AddGeomFieldDefn(&oGeomFieldDefn);
    }
    if (pszIdentifier)
    {
        m_osIdentifierLCO = pszIdentifier;
        OGRLayer::SetMetadataItem("IDENTIFIER", pszIdentifier);
    }
    if (pszDescription)
    {
        m_osDescriptionLCO = pszDescription;
        OGRLayer::SetMetadataItem("DESCRIPTION", pszDescription);
    }

    m_poFeatureDefn->Seal(/* bSealFields = */ true);
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
    char *pszSQL =
        sqlite3_mprintf("INSERT INTO gpkg_geometry_columns "
                        "(table_name,column_name,geometry_type_name,srs_id,z,m)"
                        " VALUES "
                        "('%q','%q','%q',%d,%d,%d)",
                        GetName(), GetGeometryColumn(), pszGeometryType, m_iSrs,
                        m_nZFlag, m_nMFlag);

    OGRErr err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if (err != OGRERR_NONE)
        return OGRERR_FAILURE;

    if (wkbFlatten(eGType) > wkbGeometryCollection)
    {
        CreateGeometryExtensionIfNecessary(eGType);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        GetColumnsOfCreateTable()                     */
/************************************************************************/

CPLString OGRGeoPackageTableLayer::GetColumnsOfCreateTable(
    const std::vector<OGRFieldDefn *> &apoFields)
{
    CPLString osSQL;

    char *pszSQL = nullptr;
    bool bNeedComma = false;
    if (m_pszFidColumn != nullptr)
    {
        pszSQL =
            sqlite3_mprintf("\"%w\" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL",
                            m_pszFidColumn);
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
        bNeedComma = true;
    }

    const OGRwkbGeometryType eGType = GetGeomType();
    if (eGType != wkbNone)
    {
        if (bNeedComma)
        {
            osSQL += ", ";
        }
        bNeedComma = true;

        /* Requirement 25: The geometry_type_name value in a
         * gpkg_geometry_columns */
        /* row SHALL be one of the uppercase geometry type names specified in */
        /* Geometry Types (Normative). */
        const char *pszGeometryType = m_poDS->GetGeometryTypeString(eGType);

        pszSQL =
            sqlite3_mprintf("\"%w\" %s", GetGeometryColumn(), pszGeometryType);
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
        if (!m_poFeatureDefn->GetGeomFieldDefn(0)->IsNullable())
        {
            osSQL += " NOT NULL";
        }
    }

    for (size_t i = 0; i < apoFields.size(); i++)
    {
        OGRFieldDefn *poFieldDefn = apoFields[i];
        // Eg. when a geometry type is specified + an sql statement returns no
        // or NULL geometry values, the geom column is incorrectly treated as
        // an attribute column as well with the same name. Not ideal, but skip
        // this column here to avoid duplicate column name error. Issue: #6976.
        if ((eGType != wkbNone) &&
            (EQUAL(poFieldDefn->GetNameRef(), GetGeometryColumn())))
        {
            continue;
        }
        if (bNeedComma)
        {
            osSQL += ", ";
        }
        bNeedComma = true;

        pszSQL = sqlite3_mprintf("\"%w\" %s", poFieldDefn->GetNameRef(),
                                 GPkgFieldFromOGR(poFieldDefn->GetType(),
                                                  poFieldDefn->GetSubType(),
                                                  poFieldDefn->GetWidth()));
        osSQL += pszSQL;
        sqlite3_free(pszSQL);
        if (!poFieldDefn->IsNullable())
        {
            osSQL += " NOT NULL";
        }
        if (poFieldDefn->IsUnique())
        {
            osSQL += " UNIQUE";
        }
        const char *pszDefault = poFieldDefn->GetDefault();
        if (pszDefault != nullptr &&
            (!poFieldDefn->IsDefaultDriverSpecific() ||
             (pszDefault[0] == '(' &&
              pszDefault[strlen(pszDefault) - 1] == ')' &&
              (STARTS_WITH_CI(pszDefault + 1, "strftime") ||
               STARTS_WITH_CI(pszDefault + 1, " strftime")))))
        {
            osSQL += " DEFAULT ";
            OGRField sField;
            if (poFieldDefn->GetType() == OFTDateTime &&
                OGRParseDate(pszDefault, &sField, 0))
            {
                char szBuffer[OGR_SIZEOF_ISO8601_DATETIME_BUFFER];
                OGRGetISO8601DateTime(&sField, false, szBuffer);
                osSQL += szBuffer;
            }
            /* Make sure CURRENT_TIMESTAMP is translated into appropriate format
             * for GeoPackage */
            else if (poFieldDefn->GetType() == OFTDateTime &&
                     EQUAL(pszDefault, "CURRENT_TIMESTAMP"))
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
    if (!m_bDeferredCreation)
        return OGRERR_NONE;
    m_bDeferredCreation = false;

    const char *pszLayerName = m_poFeatureDefn->GetName();

    /* Create the table! */
    CPLString osCommand;

    char *pszSQL = sqlite3_mprintf("CREATE TABLE \"%w\" ( ", pszLayerName);
    osCommand += pszSQL;
    sqlite3_free(pszSQL);

    std::vector<OGRFieldDefn *> apoFields;
    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++)
    {
        if (i == m_iFIDAsRegularColumnIndex)
            continue;
        apoFields.push_back(m_poFeatureDefn->GetFieldDefn(i));
    }

    osCommand += GetColumnsOfCreateTable(apoFields);

    osCommand += ")";

#ifdef DEBUG
    CPLDebug("GPKG", "exec(%s)", osCommand.c_str());
#endif
    OGRErr err = SQLCommand(m_poDS->GetDB(), osCommand.c_str());
    if (OGRERR_NONE != err)
        return OGRERR_FAILURE;

    for (auto &poField : apoFields)
    {
        if (!DoSpecialProcessingForColumnCreation(poField))
        {
            return OGRERR_FAILURE;
        }
    }

    /* Update gpkg_contents with the table info */
    const OGRwkbGeometryType eGType = GetGeomType();
    const bool bIsSpatial = (eGType != wkbNone);

    if (bIsSpatial || m_eASpatialVariant == GPKG_ATTRIBUTES)
    {
        const char *pszIdentifier = GetMetadataItem("IDENTIFIER");
        if (pszIdentifier == nullptr)
            pszIdentifier = pszLayerName;
        const char *pszDescription = GetMetadataItem("DESCRIPTION");
        if (pszDescription == nullptr)
            pszDescription = "";

        pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_contents "
            "(table_name,data_type,identifier,description,last_change,srs_id) "
            "VALUES "
            "('%q','%q','%q','%q',%s,%d)",
            pszLayerName, (bIsSpatial ? "features" : "attributes"),
            pszIdentifier, pszDescription,
            GDALGeoPackageDataset::GetCurrentDateEscapedSQL().c_str(), m_iSrs);

        err = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        if (err != OGRERR_NONE)
            return OGRERR_FAILURE;
    }

    if (bIsSpatial)
    {
        // Insert into gpkg_geometry_columns after gpkg_contents because of
        // foreign key constraints
        err = RegisterGeometryColumn();
        if (err != OGRERR_NONE)
            return OGRERR_FAILURE;
    }

#ifdef ENABLE_GPKG_OGR_CONTENTS
    if (m_poDS->m_bHasGPKGOGRContents)
    {
        pszSQL = sqlite3_mprintf("DELETE FROM gpkg_ogr_contents WHERE "
                                 "lower(table_name) = lower('%q')",
                                 pszLayerName);
        SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);

        pszSQL = sqlite3_mprintf(
            "INSERT INTO gpkg_ogr_contents (table_name, feature_count) "
            "VALUES ('%q', NULL)",
            pszLayerName);
        err = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        if (err == OGRERR_NONE)
        {
            m_nTotalFeatureCount = 0;
            m_bAddOGRFeatureCountTriggers = true;
        }
    }
#endif

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **OGRGeoPackageTableLayer::GetMetadata(const char *pszDomain)

{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (!m_bHasTriedDetectingFID64 && m_pszFidColumn != nullptr)
    {
        m_bHasTriedDetectingFID64 = true;

        /* --------------------------------------------------------------------
         */
        /*      Find if the FID holds 64bit values */
        /* --------------------------------------------------------------------
         */

        // Normally the fid should be AUTOINCREMENT, so check sqlite_sequence
        OGRErr err = OGRERR_NONE;
        char *pszSQL =
            sqlite3_mprintf("SELECT seq FROM sqlite_sequence WHERE name = '%q'",
                            m_pszTableName);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        GIntBig nMaxId = SQLGetInteger64(m_poDS->GetDB(), pszSQL, &err);
        CPLPopErrorHandler();
        sqlite3_free(pszSQL);
        if (err != OGRERR_NONE)
        {
            CPLErrorReset();

            // In case of error, fallback to taking the MAX of the FID
            pszSQL = sqlite3_mprintf("SELECT MAX(\"%w\") FROM \"%w\"",
                                     m_pszFidColumn, m_pszTableName);

            nMaxId = SQLGetInteger64(m_poDS->GetDB(), pszSQL, nullptr);
            sqlite3_free(pszSQL);
        }
        if (nMaxId > INT_MAX)
            OGRLayer::SetMetadataItem(OLMD_FID64, "YES");
    }

    if (m_bHasReadMetadataFromStorage)
        return OGRLayer::GetMetadata(pszDomain);

    m_bHasReadMetadataFromStorage = true;

    if (!m_poDS->HasMetadataTables())
        return OGRLayer::GetMetadata(pszDomain);

    char *pszSQL = sqlite3_mprintf(
        "SELECT md.metadata, md.md_standard_uri, md.mime_type, "
        "mdr.reference_scope "
        "FROM gpkg_metadata md "
        "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
        "WHERE lower(mdr.table_name) = lower('%q') ORDER BY md.id "
        "LIMIT 1000",  // to avoid denial of service
        m_pszTableName);

    auto oResult = SQLQuery(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if (!oResult)
    {
        return OGRLayer::GetMetadata(pszDomain);
    }

    char **papszMetadata = CSLDuplicate(OGRLayer::GetMetadata());

    /* GDAL metadata */
    for (int i = 0; i < oResult->RowCount(); i++)
    {
        const char *pszMetadata = oResult->GetValue(0, i);
        const char *pszMDStandardURI = oResult->GetValue(1, i);
        const char *pszMimeType = oResult->GetValue(2, i);
        const char *pszReferenceScope = oResult->GetValue(3, i);
        if (pszMetadata && pszMDStandardURI && pszMimeType &&
            pszReferenceScope && EQUAL(pszMDStandardURI, "http://gdal.org") &&
            EQUAL(pszMimeType, "text/xml") && EQUAL(pszReferenceScope, "table"))
        {
            CPLXMLNode *psXMLNode = CPLParseXMLString(pszMetadata);
            if (psXMLNode)
            {
                GDALMultiDomainMetadata oLocalMDMD;
                oLocalMDMD.XMLInit(psXMLNode, FALSE);

                papszMetadata =
                    CSLMerge(papszMetadata, oLocalMDMD.GetMetadata());
                CSLConstList papszDomainList = oLocalMDMD.GetDomainList();
                CSLConstList papszIter = papszDomainList;
                while (papszIter && *papszIter)
                {
                    if (!EQUAL(*papszIter, ""))
                        oMDMD.SetMetadata(oLocalMDMD.GetMetadata(*papszIter),
                                          *papszIter);
                    papszIter++;
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
    for (int i = 0; i < oResult->RowCount(); i++)
    {
        const char *pszMetadata = oResult->GetValue(0, i);
        const char *pszMDStandardURI = oResult->GetValue(1, i);
        const char *pszMimeType = oResult->GetValue(2, i);
        // const char* pszReferenceScope = oResult->GetValue(3, i);
        if (pszMetadata == nullptr || pszMDStandardURI == nullptr ||
            pszMimeType == nullptr
            /* || pszReferenceScope == nullptr */)
        {
            // should not happen as there are NOT NULL constraints
            // But a database could lack such NOT NULL constraints or have
            // large values that would cause a memory allocation failure.
            continue;
        }
        // int bIsGPKGScope = EQUAL(pszReferenceScope, "geopackage");
        if (EQUAL(pszMDStandardURI, "http://gdal.org") &&
            EQUAL(pszMimeType, "text/xml"))
            continue;

        if (EQUAL(pszMDStandardURI, "http://gdal.org") &&
            EQUAL(pszMimeType, "text/plain"))
        {
            if (STARTS_WITH_CI(pszMetadata, "coordinate_epoch="))
            {
                continue;
            }
        }

        /*if( strcmp( pszMDStandardURI, "http://www.isotc211.org/2005/gmd" ) ==
        0 && strcmp( pszMimeType, "text/xml" ) == 0 )
        {
            char* apszMD[2];
            apszMD[0] = (char*)pszMetadata;
            apszMD[1] = NULL;
            oMDMD.SetMetadata(apszMD, "xml:MD_Metadata");
        }
        else*/
        {
            oMDMD.SetMetadataItem(
                CPLSPrintf("GPKG_METADATA_ITEM_%d", nNonGDALMDILocal),
                pszMetadata);
            nNonGDALMDILocal++;
        }
    }

    return OGRLayer::GetMetadata(pszDomain);
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *OGRGeoPackageTableLayer::GetMetadataItem(const char *pszName,
                                                     const char *pszDomain)
{
    return CSLFetchNameValue(GetMetadata(pszDomain), pszName);
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

CPLErr OGRGeoPackageTableLayer::SetMetadata(char **papszMetadata,
                                            const char *pszDomain)
{
    GetMetadata(); /* force loading from storage if needed */
    CPLErr eErr = OGRLayer::SetMetadata(papszMetadata, pszDomain);
    m_poDS->SetMetadataDirty();
    if (pszDomain == nullptr || EQUAL(pszDomain, ""))
    {
        if (!m_osIdentifierLCO.empty())
            OGRLayer::SetMetadataItem("IDENTIFIER", m_osIdentifierLCO);
        if (!m_osDescriptionLCO.empty())
            OGRLayer::SetMetadataItem("DESCRIPTION", m_osDescriptionLCO);
    }
    return eErr;
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr OGRGeoPackageTableLayer::SetMetadataItem(const char *pszName,
                                                const char *pszValue,
                                                const char *pszDomain)
{
    GetMetadata(); /* force loading from storage if needed */
    if (!m_osIdentifierLCO.empty() && EQUAL(pszName, "IDENTIFIER") &&
        (pszDomain == nullptr || EQUAL(pszDomain, "")))
        return CE_None;
    if (!m_osDescriptionLCO.empty() && EQUAL(pszName, "DESCRIPTION") &&
        (pszDomain == nullptr || EQUAL(pszDomain, "")))
        return CE_None;
    m_poDS->SetMetadataDirty();
    return OGRLayer::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                          RecreateTable()                             */
/************************************************************************/

OGRErr
OGRGeoPackageTableLayer::RecreateTable(const CPLString &osColumnsForCreate,
                                       const CPLString &osFieldListForSelect)
{
    /* -------------------------------------------------------------------- */
    /*      Save existing related triggers and index                        */
    /* -------------------------------------------------------------------- */
    sqlite3 *hDB = m_poDS->GetDB();

    char *pszSQL = sqlite3_mprintf(
        "SELECT sql FROM sqlite_master WHERE type IN ('trigger','index') "
        "AND lower(tbl_name)=lower('%q') LIMIT 10000",
        m_pszTableName);
    OGRErr eErr = OGRERR_NONE;
    auto oTriggers = SQLQuery(hDB, pszSQL);
    sqlite3_free(pszSQL);

    /* -------------------------------------------------------------------- */
    /*      Make a temporary table with new content.                        */
    /* -------------------------------------------------------------------- */
    if (oTriggers)
    {
        pszSQL = sqlite3_mprintf("CREATE TABLE \"%w_ogr_tmp\" (%s)",
                                 m_pszTableName, osColumnsForCreate.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }
    else
    {
        eErr = OGRERR_FAILURE;
    }

    if (eErr == OGRERR_NONE)
    {
        pszSQL = sqlite3_mprintf(
            "INSERT INTO \"%w_ogr_tmp\" SELECT %s FROM \"%w\"", m_pszTableName,
            osFieldListForSelect.c_str(), m_pszTableName);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    /* -------------------------------------------------------------------- */
    /*      Drop the original table                                         */
    /* -------------------------------------------------------------------- */
    if (eErr == OGRERR_NONE)
    {
        pszSQL = sqlite3_mprintf("DROP TABLE \"%w\"", m_pszTableName);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    /* -------------------------------------------------------------------- */
    /*      Rename temporary table as new table                             */
    /* -------------------------------------------------------------------- */
    if (eErr == OGRERR_NONE)
    {
        pszSQL = sqlite3_mprintf("ALTER TABLE \"%w_ogr_tmp\" RENAME TO \"%w\"",
                                 m_pszTableName, m_pszTableName);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    /* -------------------------------------------------------------------- */
    /*      Recreate existing related tables, triggers and index            */
    /* -------------------------------------------------------------------- */
    for (int i = 0;
         oTriggers && i < oTriggers->RowCount() && eErr == OGRERR_NONE; i++)
    {
        const char *pszSQLTriggerIdx = oTriggers->GetValue(0, i);
        if (pszSQLTriggerIdx != nullptr && *pszSQLTriggerIdx != '\0')
        {
            eErr = SQLCommand(hDB, pszSQLTriggerIdx);
        }
    }

    return eErr;
}

/************************************************************************/
/*                          BuildSelectFieldList()                      */
/************************************************************************/

CPLString OGRGeoPackageTableLayer::BuildSelectFieldList(
    const std::vector<OGRFieldDefn *> &apoFields)
{
    CPLString osFieldListForSelect;

    char *pszSQL = nullptr;
    bool bNeedComma = false;

    if (m_pszFidColumn != nullptr)
    {
        pszSQL = sqlite3_mprintf("\"%w\"", m_pszFidColumn);
        osFieldListForSelect += pszSQL;
        sqlite3_free(pszSQL);
        bNeedComma = true;
    }

    if (GetGeomType() != wkbNone)
    {
        if (bNeedComma)
        {
            osFieldListForSelect += ", ";
        }
        bNeedComma = true;

        pszSQL = sqlite3_mprintf("\"%w\"", GetGeometryColumn());
        osFieldListForSelect += pszSQL;
        sqlite3_free(pszSQL);
    }

    for (size_t iField = 0; iField < apoFields.size(); iField++)
    {
        if (bNeedComma)
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

OGRErr OGRGeoPackageTableLayer::DeleteField(int iFieldToDelete)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (!CheckUpdatableTable("DeleteField"))
        return OGRERR_FAILURE;

    if (iFieldToDelete < 0 ||
        iFieldToDelete >= m_poFeatureDefn->GetFieldCount())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid field index");
        return OGRERR_FAILURE;
    }

    ResetReading();
    RunDeferredCreationIfNecessary();
    if (!RunDeferredSpatialIndexUpdate())
        return OGRERR_FAILURE;

    const char *pszFieldName =
        m_poFeatureDefn->GetFieldDefn(iFieldToDelete)->GetNameRef();

    /* -------------------------------------------------------------------- */
    /*      Drop any iterator since we change the DB structure              */
    /* -------------------------------------------------------------------- */
    m_poDS->ResetReadingAllLayers();

    // Temporary remove foreign key checks
    const GPKGTemporaryForeignKeyCheckDisabler
        oGPKGTemporaryForeignKeyCheckDisabler(m_poDS);

    if (m_poDS->SoftStartTransaction() != OGRERR_NONE)
    {
        return OGRERR_FAILURE;
    }

    // ALTER TABLE ... DROP COLUMN ... was first implemented in 3.35.0 but
    // there was bug fixes related to it until 3.35.5
#if SQLITE_VERSION_NUMBER >= 3035005L
    OGRErr eErr = SQLCommand(
        m_poDS->GetDB(), CPLString()
                             .Printf("ALTER TABLE \"%s\" DROP COLUMN \"%s\"",
                                     SQLEscapeName(m_pszTableName).c_str(),
                                     SQLEscapeName(pszFieldName).c_str())
                             .c_str());
#else
    /* -------------------------------------------------------------------- */
    /*      Recreate table in a transaction                                 */
    /*      Build list of old fields, and the list of new fields.           */
    /* -------------------------------------------------------------------- */
    std::vector<OGRFieldDefn *> apoFields;
    for (int iField = 0; iField < m_poFeatureDefn->GetFieldCount(); iField++)
    {
        if (iField == iFieldToDelete)
            continue;

        OGRFieldDefn *poFieldDefn = m_poFeatureDefn->GetFieldDefn(iField);
        apoFields.push_back(poFieldDefn);
    }

    CPLString osFieldListForSelect(BuildSelectFieldList(apoFields));
    CPLString osColumnsForCreate(GetColumnsOfCreateTable(apoFields));

    OGRErr eErr = RecreateTable(osColumnsForCreate, osFieldListForSelect);
#endif

    /* -------------------------------------------------------------------- */
    /*      Update gpkg_extensions if needed.                               */
    /* -------------------------------------------------------------------- */
    if (eErr == OGRERR_NONE && m_poDS->HasExtensionsTable())
    {
        char *pszSQL = sqlite3_mprintf("DELETE FROM gpkg_extensions WHERE "
                                       "lower(table_name) = lower('%q') AND "
                                       "lower(column_name) = lower('%q')",
                                       m_pszTableName, pszFieldName);
        eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }

    /* -------------------------------------------------------------------- */
    /*      Update gpkg_data_columns if needed.                             */
    /* -------------------------------------------------------------------- */
    if (eErr == OGRERR_NONE && m_poDS->HasDataColumnsTable())
    {
        char *pszSQL = sqlite3_mprintf("DELETE FROM gpkg_data_columns WHERE "
                                       "lower(table_name) = lower('%q') AND "
                                       "lower(column_name) = lower('%q')",
                                       m_pszTableName, pszFieldName);
        eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }

    /* -------------------------------------------------------------------- */
    /*      Update gpkg_metadata_reference if needed.                       */
    /* -------------------------------------------------------------------- */
    if (eErr == OGRERR_NONE && m_poDS->HasMetadataTables())
    {
        {
            // Delete from gpkg_metadata metadata records that are only
            // referenced by the column we are about to drop
            char *pszSQL = sqlite3_mprintf(
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
                m_pszTableName, pszFieldName, m_pszTableName, pszFieldName,
                m_pszTableName, pszFieldName);
            eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
            sqlite3_free(pszSQL);
        }

        if (eErr == OGRERR_NONE)
        {
            char *pszSQL =
                sqlite3_mprintf("DELETE FROM gpkg_metadata_reference WHERE "
                                "lower(table_name) = lower('%q') AND "
                                "lower(column_name) = lower('%q')",
                                m_pszTableName, pszFieldName);
            eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
            sqlite3_free(pszSQL);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Check foreign key integrity if enforcement of foreign keys      */
    /*      constraint is enabled.                                          */
    /* -------------------------------------------------------------------- */
    if (eErr == OGRERR_NONE &&
        SQLGetInteger(m_poDS->GetDB(), "PRAGMA foreign_keys", nullptr))
    {
        CPLDebug("GPKG", "Running PRAGMA foreign_key_check");
        eErr = m_poDS->PragmaCheck("foreign_key_check", "", 0);
    }

    /* -------------------------------------------------------------------- */
    /*      Finish                                                          */
    /* -------------------------------------------------------------------- */
    if (eErr == OGRERR_NONE)
    {
        eErr = m_poDS->SoftCommitTransaction();
        if (eErr == OGRERR_NONE)
        {
            eErr = whileUnsealing(m_poFeatureDefn)
                       ->DeleteFieldDefn(iFieldToDelete);

            if (eErr == OGRERR_NONE)
            {
#if SQLITE_VERSION_NUMBER >= 3035005L
                m_abGeneratedColumns.erase(m_abGeneratedColumns.begin() +
                                           iFieldToDelete);
#else
                // We have recreated the table from scratch, and lost the
                // generated column property
                std::fill(m_abGeneratedColumns.begin(),
                          m_abGeneratedColumns.end(), false);
#endif
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
/*                    RenameFieldInAuxiliaryTables()                    */
/************************************************************************/

OGRErr
OGRGeoPackageTableLayer::RenameFieldInAuxiliaryTables(const char *pszOldName,
                                                      const char *pszNewName)
{
    OGRErr eErr = OGRERR_NONE;
    sqlite3 *hDB = m_poDS->GetDB();

    /* -------------------------------------------------------------------- */
    /*      Update gpkg_extensions if needed.                               */
    /* -------------------------------------------------------------------- */
    if (/* eErr == OGRERR_NONE && */ m_poDS->HasExtensionsTable())
    {
        char *pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_extensions SET column_name = '%q' WHERE "
            "lower(table_name) = lower('%q') AND lower(column_name) = "
            "lower('%q')",
            pszNewName, m_pszTableName, pszOldName);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    /* -------------------------------------------------------------------- */
    /*      Update gpkg_data_columns if needed.                             */
    /* -------------------------------------------------------------------- */
    if (eErr == OGRERR_NONE && m_poDS->HasDataColumnsTable())
    {
        char *pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_data_columns SET column_name = '%q' WHERE "
            "lower(table_name) = lower('%q') AND lower(column_name) = "
            "lower('%q')",
            pszNewName, m_pszTableName, pszOldName);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    /* -------------------------------------------------------------------- */
    /*      Update gpkg_metadata_reference if needed.                       */
    /* -------------------------------------------------------------------- */
    if (eErr == OGRERR_NONE && m_poDS->HasMetadataTables())
    {
        char *pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_metadata_reference SET column_name = '%q' WHERE "
            "lower(table_name) = lower('%q') AND lower(column_name) = "
            "lower('%q')",
            pszNewName, m_pszTableName, pszOldName);
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);
    }

    return eErr;
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::AlterFieldDefn(int iFieldToAlter,
                                               OGRFieldDefn *poNewFieldDefn,
                                               int nFlagsIn)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (!CheckUpdatableTable("AlterFieldDefn"))
        return OGRERR_FAILURE;

    if (iFieldToAlter < 0 || iFieldToAlter >= m_poFeatureDefn->GetFieldCount())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid field index");
        return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /*      Deferred actions, reset state.                                   */
    /* -------------------------------------------------------------------- */
    ResetReading();
    RunDeferredCreationIfNecessary();
    if (m_bThreadRTreeStarted)
        CancelAsyncRTree();
    if (!RunDeferredSpatialIndexUpdate())
        return OGRERR_FAILURE;

    /* -------------------------------------------------------------------- */
    /*      Check that the new column name is not a duplicate.              */
    /* -------------------------------------------------------------------- */

    OGRFieldDefn *poFieldDefnToAlter =
        m_poFeatureDefn->GetFieldDefn(iFieldToAlter);
    const CPLString osOldColName(poFieldDefnToAlter->GetNameRef());
    const CPLString osNewColName((nFlagsIn & ALTER_NAME_FLAG)
                                     ? CPLString(poNewFieldDefn->GetNameRef())
                                     : osOldColName);

    const bool bRenameCol =
        (nFlagsIn & ALTER_NAME_FLAG) &&
        strcmp(poNewFieldDefn->GetNameRef(), osOldColName) != 0;
    if (bRenameCol)
    {
        if ((m_pszFidColumn &&
             strcmp(poNewFieldDefn->GetNameRef(), m_pszFidColumn) == 0) ||
            (GetGeomType() != wkbNone &&
             strcmp(poNewFieldDefn->GetNameRef(),
                    m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef()) == 0) ||
            m_poFeatureDefn->GetFieldIndex(poNewFieldDefn->GetNameRef()) >= 0)
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
    bool bUseRewriteSchemaMethod = (m_poDS->nSoftTransactionLevel == 0);
    int nActualFlags = 0;
    if (bRenameCol)
    {
        nActualFlags |= ALTER_NAME_FLAG;
        oTmpFieldDefn.SetName(poNewFieldDefn->GetNameRef());
    }
    if ((nFlagsIn & ALTER_TYPE_FLAG) != 0 &&
        (poFieldDefnToAlter->GetType() != poNewFieldDefn->GetType() ||
         poFieldDefnToAlter->GetSubType() != poNewFieldDefn->GetSubType()))
    {
        nActualFlags |= ALTER_TYPE_FLAG;
        oTmpFieldDefn.SetSubType(OFSTNone);
        oTmpFieldDefn.SetType(poNewFieldDefn->GetType());
        oTmpFieldDefn.SetSubType(poNewFieldDefn->GetSubType());
    }
    if ((nFlagsIn & ALTER_WIDTH_PRECISION_FLAG) != 0 &&
        (poFieldDefnToAlter->GetWidth() != poNewFieldDefn->GetWidth() ||
         poFieldDefnToAlter->GetPrecision() != poNewFieldDefn->GetPrecision()))
    {
        nActualFlags |= ALTER_WIDTH_PRECISION_FLAG;
        oTmpFieldDefn.SetWidth(poNewFieldDefn->GetWidth());
        oTmpFieldDefn.SetPrecision(poNewFieldDefn->GetPrecision());
    }
    if ((nFlagsIn & ALTER_NULLABLE_FLAG) != 0 &&
        poFieldDefnToAlter->IsNullable() != poNewFieldDefn->IsNullable())
    {
        nActualFlags |= ALTER_NULLABLE_FLAG;
        bUseRewriteSchemaMethod = false;
        oTmpFieldDefn.SetNullable(poNewFieldDefn->IsNullable());
    }
    if ((nFlagsIn & ALTER_DEFAULT_FLAG) != 0 &&
        !((poFieldDefnToAlter->GetDefault() == nullptr &&
           poNewFieldDefn->GetDefault() == nullptr) ||
          (poFieldDefnToAlter->GetDefault() != nullptr &&
           poNewFieldDefn->GetDefault() != nullptr &&
           strcmp(poFieldDefnToAlter->GetDefault(),
                  poNewFieldDefn->GetDefault()) == 0)))
    {
        nActualFlags |= ALTER_DEFAULT_FLAG;
        oTmpFieldDefn.SetDefault(poNewFieldDefn->GetDefault());
    }
    if ((nFlagsIn & ALTER_UNIQUE_FLAG) != 0 &&
        poFieldDefnToAlter->IsUnique() != poNewFieldDefn->IsUnique())
    {
        nActualFlags |= ALTER_UNIQUE_FLAG;
        bUseRewriteSchemaMethod = false;
        oTmpFieldDefn.SetUnique(poNewFieldDefn->IsUnique());
    }
    if ((nFlagsIn & ALTER_DOMAIN_FLAG) != 0 &&
        poFieldDefnToAlter->GetDomainName() != poNewFieldDefn->GetDomainName())
    {
        nActualFlags |= ALTER_DOMAIN_FLAG;
        oTmpFieldDefn.SetDomainName(poNewFieldDefn->GetDomainName());
    }
    if ((nFlagsIn & ALTER_ALTERNATIVE_NAME_FLAG) != 0 &&
        strcmp(poFieldDefnToAlter->GetAlternativeNameRef(),
               poNewFieldDefn->GetAlternativeNameRef()) != 0)
    {
        nActualFlags |= ALTER_ALTERNATIVE_NAME_FLAG;
        oTmpFieldDefn.SetAlternativeName(
            poNewFieldDefn->GetAlternativeNameRef());
    }
    if ((nFlagsIn & ALTER_COMMENT_FLAG) != 0 &&
        poFieldDefnToAlter->GetComment() != poNewFieldDefn->GetComment())
    {
        nActualFlags |= ALTER_COMMENT_FLAG;
        oTmpFieldDefn.SetComment(poNewFieldDefn->GetComment());
    }

    /* -------------------------------------------------------------------- */
    /*      Build list of old fields, and the list of new fields.           */
    /* -------------------------------------------------------------------- */
    std::vector<OGRFieldDefn *> apoFields;
    std::vector<OGRFieldDefn *> apoFieldsOld;
    for (int iField = 0; iField < m_poFeatureDefn->GetFieldCount(); iField++)
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

    const CPLString osColumnsForCreate(GetColumnsOfCreateTable(apoFields));

    /* -------------------------------------------------------------------- */
    /*      Drop any iterator since we change the DB structure              */
    /* -------------------------------------------------------------------- */
    m_poDS->ResetReadingAllLayers();

    const bool bUseRenameColumn = (nActualFlags == ALTER_NAME_FLAG);
    if (bUseRenameColumn)
        bUseRewriteSchemaMethod = false;

    if (m_poDS->SoftStartTransaction() != OGRERR_NONE)
        return OGRERR_FAILURE;

    sqlite3 *hDB = m_poDS->GetDB();
    OGRErr eErr = OGRERR_NONE;

    /* -------------------------------------------------------------------- */
    /*      Drop triggers and index that look like to be related to the     */
    /*      column if renaming. We re-install some indexes afterwards.      */
    /* -------------------------------------------------------------------- */
    std::unique_ptr<SQLResult> oTriggers;
    // cppcheck-suppress knownConditionTrueFalse
    if (bRenameCol && !bUseRenameColumn)
    {
        char *pszSQL = sqlite3_mprintf(
            "SELECT name, type, sql FROM sqlite_master WHERE "
            "type IN ('trigger','index') "
            "AND lower(tbl_name)=lower('%q') AND sql LIKE '%%%q%%' LIMIT 10000",
            m_pszTableName, SQLEscapeName(osOldColName).c_str());
        oTriggers = SQLQuery(hDB, pszSQL);
        sqlite3_free(pszSQL);

        if (!oTriggers)
        {
            eErr = OGRERR_FAILURE;
        }

        for (int i = 0; oTriggers && i < oTriggers->RowCount(); i++)
        {
            pszSQL =
                sqlite3_mprintf("DROP %s \"%w\"", oTriggers->GetValue(1, i),
                                oTriggers->GetValue(0, i));
            eErr = SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
    }

    if (bUseRenameColumn)
    {
        if (eErr == OGRERR_NONE)
        {
            CPLDebug("GPKG", "Running ALTER TABLE RENAME COLUMN");
            eErr = SQLCommand(
                m_poDS->GetDB(),
                CPLString()
                    .Printf("ALTER TABLE \"%s\" RENAME COLUMN \"%s\" TO \"%s\"",
                            SQLEscapeName(m_pszTableName).c_str(),
                            SQLEscapeName(osOldColName).c_str(),
                            SQLEscapeName(osNewColName).c_str())
                    .c_str());
        }
    }
    else if (!bUseRewriteSchemaMethod)
    {
        /* --------------------------------------------------------------------
         */
        /*      If we are within a transaction, we cannot use the method */
        /*      that consists in altering the database in a raw way. */
        /* --------------------------------------------------------------------
         */
        const CPLString osFieldListForSelect(
            BuildSelectFieldList(apoFieldsOld));

        if (eErr == OGRERR_NONE)
        {
            eErr = RecreateTable(osColumnsForCreate, osFieldListForSelect);
        }
    }
    else
    {
        /* --------------------------------------------------------------------
         */
        /*      Rewrite schema in a transaction by altering the database */
        /*      schema in a rather raw way, as described at bottom of */
        /*      https://www.sqlite.org/lang_altertable.html */
        /* --------------------------------------------------------------------
         */

        /* --------------------------------------------------------------------
         */
        /*      Collect schema version number. */
        /* --------------------------------------------------------------------
         */
        int nSchemaVersion = SQLGetInteger(hDB, "PRAGMA schema_version", &eErr);

        /* --------------------------------------------------------------------
         */
        /*      Turn on writable schema. */
        /* --------------------------------------------------------------------
         */
        if (eErr == OGRERR_NONE)
        {
            eErr = m_poDS->PragmaCheck("writable_schema=ON", "", 0);
        }

        /* --------------------------------------------------------------------
         */
        /*      Rewrite CREATE TABLE statement. */
        /* --------------------------------------------------------------------
         */
        if (eErr == OGRERR_NONE)
        {
            char *psSQLCreateTable =
                sqlite3_mprintf("CREATE TABLE \"%w\" (%s)", m_pszTableName,
                                osColumnsForCreate.c_str());
            char *pszSQL = sqlite3_mprintf("UPDATE sqlite_master SET sql='%q' "
                                           "WHERE type='table' AND name='%q'",
                                           psSQLCreateTable, m_pszTableName);
            eErr = SQLCommand(hDB, pszSQL);
            sqlite3_free(psSQLCreateTable);
            sqlite3_free(pszSQL);
        }

        /* --------------------------------------------------------------------
         */
        /*      Increment schema number. */
        /* --------------------------------------------------------------------
         */
        if (eErr == OGRERR_NONE)
        {
            char *pszSQL = sqlite3_mprintf("PRAGMA schema_version = %d",
                                           nSchemaVersion + 1);
            eErr = SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }

        /* --------------------------------------------------------------------
         */
        /*      Turn off writable schema. */
        /* --------------------------------------------------------------------
         */
        if (eErr == OGRERR_NONE)
        {
            eErr = m_poDS->PragmaCheck("writable_schema=OFF", "", 0);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Update auxiliary tables                                        */
    /* -------------------------------------------------------------------- */
    if (bRenameCol && eErr == OGRERR_NONE)
    {
        eErr = RenameFieldInAuxiliaryTables(osOldColName.c_str(),
                                            poNewFieldDefn->GetNameRef());
    }

    /* -------------------------------------------------------------------- */
    /*      Update gpkgext_relations if needed.                             */
    /* -------------------------------------------------------------------- */
    if (bRenameCol && eErr == OGRERR_NONE && m_poDS->HasGpkgextRelationsTable())
    {
        char *pszSQL = sqlite3_mprintf(
            "UPDATE gpkgext_relations SET base_primary_column = '%q' WHERE "
            "lower(base_table_name) = lower('%q') AND "
            "lower(base_primary_column) = lower('%q')",
            poNewFieldDefn->GetNameRef(), m_pszTableName, osOldColName.c_str());
        eErr = SQLCommand(hDB, pszSQL);
        sqlite3_free(pszSQL);

        if (eErr == OGRERR_NONE)
        {
            pszSQL =
                sqlite3_mprintf("UPDATE gpkgext_relations SET "
                                "related_primary_column = '%q' WHERE "
                                "lower(related_table_name) = lower('%q') AND "
                                "lower(related_primary_column) = lower('%q')",
                                poNewFieldDefn->GetNameRef(), m_pszTableName,
                                osOldColName.c_str());
            eErr = SQLCommand(hDB, pszSQL);
            sqlite3_free(pszSQL);
        }
        m_poDS->ClearCachedRelationships();
    }

    /* -------------------------------------------------------------------- */
    /*      Run integrity check only if explicitly required.                */
    /* -------------------------------------------------------------------- */
    if (eErr == OGRERR_NONE &&
        CPLTestBool(CPLGetConfigOption("OGR_GPKG_INTEGRITY_CHECK", "NO")))
    {
        CPLDebug("GPKG", "Running PRAGMA integrity_check");
        eErr = m_poDS->PragmaCheck("integrity_check", "ok", 1);
    }

    /* -------------------------------------------------------------------- */
    /*      Otherwise check foreign key integrity if enforcement of foreign */
    /*      kets constraint is enabled.                                     */
    /* -------------------------------------------------------------------- */
    else if (eErr == OGRERR_NONE &&
             SQLGetInteger(m_poDS->GetDB(), "PRAGMA foreign_keys", nullptr))
    {
        CPLDebug("GPKG", "Running PRAGMA foreign_key_check");
        eErr = m_poDS->PragmaCheck("foreign_key_check", "", 0);
    }

    /* -------------------------------------------------------------------- */
    /*      Finish                                                          */
    /* -------------------------------------------------------------------- */
    if (eErr == OGRERR_NONE)
    {
        eErr = m_poDS->SoftCommitTransaction();

        // We need to force database reopening due to schema change
        if (eErr == OGRERR_NONE && bUseRewriteSchemaMethod &&
            !m_poDS->ReOpenDB())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot reopen database");
            eErr = OGRERR_FAILURE;
        }
        hDB = m_poDS->GetDB();

        /* --------------------------------------------------------------------
         */
        /*      Recreate indices. */
        /* --------------------------------------------------------------------
         */
        for (int i = 0;
             oTriggers && i < oTriggers->RowCount() && eErr == OGRERR_NONE; i++)
        {
            if (EQUAL(oTriggers->GetValue(1, i), "index"))
            {
                CPLString osSQL(oTriggers->GetValue(2, i));
                // CREATE INDEX idx_name ON table_name(column_name)
                char **papszTokens = SQLTokenize(osSQL);
                if (CSLCount(papszTokens) == 8 &&
                    EQUAL(papszTokens[0], "CREATE") &&
                    EQUAL(papszTokens[1], "INDEX") &&
                    EQUAL(papszTokens[3], "ON") && EQUAL(papszTokens[5], "(") &&
                    EQUAL(papszTokens[7], ")"))
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

        if (eErr == OGRERR_NONE)
        {
            auto oTemporaryUnsealer(poFieldDefnToAlter->GetTemporaryUnsealer());
            bool bNeedsEntryInGpkgDataColumns = false;

            // field type
            if (nActualFlags & ALTER_TYPE_FLAG)
            {
                poFieldDefnToAlter->SetSubType(OFSTNone);
                poFieldDefnToAlter->SetType(poNewFieldDefn->GetType());
                poFieldDefnToAlter->SetSubType(poNewFieldDefn->GetSubType());
            }
            if (poFieldDefnToAlter->GetType() == OFTString &&
                poFieldDefnToAlter->GetSubType() == OFSTJSON)
            {
                bNeedsEntryInGpkgDataColumns = true;
            }

            // name
            if (nActualFlags & ALTER_NAME_FLAG)
            {
                poFieldDefnToAlter->SetName(poNewFieldDefn->GetNameRef());
            }

            // width/precision
            if (nActualFlags & ALTER_WIDTH_PRECISION_FLAG)
            {
                poFieldDefnToAlter->SetWidth(poNewFieldDefn->GetWidth());
                poFieldDefnToAlter->SetPrecision(
                    poNewFieldDefn->GetPrecision());
            }

            // constraints
            if (nActualFlags & ALTER_NULLABLE_FLAG)
                poFieldDefnToAlter->SetNullable(poNewFieldDefn->IsNullable());
            if (nActualFlags & ALTER_DEFAULT_FLAG)
                poFieldDefnToAlter->SetDefault(poNewFieldDefn->GetDefault());
            if (nActualFlags & ALTER_UNIQUE_FLAG)
                poFieldDefnToAlter->SetUnique(poNewFieldDefn->IsUnique());

            // domain
            if ((nActualFlags & ALTER_DOMAIN_FLAG) &&
                poFieldDefnToAlter->GetDomainName() !=
                    poNewFieldDefn->GetDomainName())
            {
                poFieldDefnToAlter->SetDomainName(
                    poNewFieldDefn->GetDomainName());
            }
            if (!poFieldDefnToAlter->GetDomainName().empty())
            {
                bNeedsEntryInGpkgDataColumns = true;
            }

            // alternative name
            if ((nActualFlags & ALTER_ALTERNATIVE_NAME_FLAG) &&
                strcmp(poFieldDefnToAlter->GetAlternativeNameRef(),
                       poNewFieldDefn->GetAlternativeNameRef()) != 0)
            {
                poFieldDefnToAlter->SetAlternativeName(
                    poNewFieldDefn->GetAlternativeNameRef());
            }
            if (!std::string(poFieldDefnToAlter->GetAlternativeNameRef())
                     .empty())
            {
                bNeedsEntryInGpkgDataColumns = true;
            }

            // comment
            if ((nActualFlags & ALTER_COMMENT_FLAG) &&
                poFieldDefnToAlter->GetComment() !=
                    poNewFieldDefn->GetComment())
            {
                poFieldDefnToAlter->SetComment(poNewFieldDefn->GetComment());
            }
            if (!poFieldDefnToAlter->GetComment().empty())
            {
                bNeedsEntryInGpkgDataColumns = true;
            }

            if (m_poDS->HasDataColumnsTable())
            {
                char *pszSQL = sqlite3_mprintf(
                    "DELETE FROM gpkg_data_columns WHERE "
                    "lower(table_name) = lower('%q') AND "
                    "lower(column_name) = lower('%q')",
                    m_pszTableName, poFieldDefnToAlter->GetNameRef());
                eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
                sqlite3_free(pszSQL);
            }

            if (bNeedsEntryInGpkgDataColumns)
            {
                if (!DoSpecialProcessingForColumnCreation(poFieldDefnToAlter))
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
/*                         AlterGeomFieldDefn()                         */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::AlterGeomFieldDefn(
    int iGeomFieldToAlter, const OGRGeomFieldDefn *poNewGeomFieldDefn,
    int nFlagsIn)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (!CheckUpdatableTable("AlterGeomFieldDefn"))
        return OGRERR_FAILURE;

    if (iGeomFieldToAlter < 0 ||
        iGeomFieldToAlter >= m_poFeatureDefn->GetGeomFieldCount())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid field index");
        return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /*      Deferred actions, reset state.                                   */
    /* -------------------------------------------------------------------- */
    ResetReading();
    RunDeferredCreationIfNecessary();
    if (m_bThreadRTreeStarted)
        CancelAsyncRTree();
    if (!RunDeferredSpatialIndexUpdate())
        return OGRERR_FAILURE;
    RevertWorkaroundUpdate1TriggerIssue();

    /* -------------------------------------------------------------------- */
    /*      Drop any iterator since we change the DB structure              */
    /* -------------------------------------------------------------------- */
    m_poDS->ResetReadingAllLayers();

    auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(iGeomFieldToAlter);
    auto oTemporaryUnsealer(poGeomFieldDefn->GetTemporaryUnsealer());

    if (nFlagsIn & ALTER_GEOM_FIELD_DEFN_TYPE_FLAG)
    {
        // could be potentially done. Requires rewriting the CREATE TABLE
        // statement
        if (poGeomFieldDefn->GetType() != poNewGeomFieldDefn->GetType())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Altering the geometry field type is not currently "
                     "supported for "
                     "GeoPackage");
            return OGRERR_FAILURE;
        }
    }

    if (nFlagsIn & ALTER_GEOM_FIELD_DEFN_NULLABLE_FLAG)
    {
        // could be potentially done. Requires rewriting the CREATE TABLE
        // statement
        if (poGeomFieldDefn->IsNullable() != poNewGeomFieldDefn->IsNullable())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Altering the nullable state of the geometry field "
                     "is not currently supported for GeoPackage");
            return OGRERR_FAILURE;
        }
    }

    if ((nFlagsIn & ALTER_GEOM_FIELD_DEFN_NAME_FLAG) != 0 &&
        strcmp(poGeomFieldDefn->GetNameRef(),
               poNewGeomFieldDefn->GetNameRef()) != 0)
    {
        const bool bHasSpatialIndex = HasSpatialIndex();

        if (m_poDS->SoftStartTransaction() != OGRERR_NONE)
            return OGRERR_FAILURE;

        // Rename geometry field
        auto eErr = SQLCommand(
            m_poDS->GetDB(),
            CPLString()
                .Printf("ALTER TABLE \"%s\" RENAME COLUMN \"%s\" TO \"%s\"",
                        SQLEscapeName(m_pszTableName).c_str(),
                        SQLEscapeName(poGeomFieldDefn->GetNameRef()).c_str(),
                        SQLEscapeName(poNewGeomFieldDefn->GetNameRef()).c_str())
                .c_str());
        if (eErr != OGRERR_NONE)
        {
            m_poDS->SoftRollbackTransaction();
            return OGRERR_FAILURE;
        }

        // Update gpkg_geometry_columns
        eErr = SQLCommand(
            m_poDS->GetDB(),
            CPLString()
                .Printf("UPDATE gpkg_geometry_columns SET column_name = \"%s\" "
                        "WHERE lower(table_name) = lower(\"%s\") "
                        "AND lower(column_name) = lower(\"%s\")",
                        SQLEscapeName(poNewGeomFieldDefn->GetNameRef()).c_str(),
                        SQLEscapeName(m_pszTableName).c_str(),
                        SQLEscapeName(poGeomFieldDefn->GetNameRef()).c_str())
                .c_str());
        if (eErr != OGRERR_NONE)
        {
            m_poDS->SoftRollbackTransaction();
            return OGRERR_FAILURE;
        }

        // Update auxiliary tables
        eErr = RenameFieldInAuxiliaryTables(poGeomFieldDefn->GetNameRef(),
                                            poNewGeomFieldDefn->GetNameRef());
        if (eErr != OGRERR_NONE)
        {
            m_poDS->SoftRollbackTransaction();
            return OGRERR_FAILURE;
        }

        std::string osNewRTreeName;
        if (bHasSpatialIndex)
        {
            osNewRTreeName = "rtree_";
            osNewRTreeName += m_pszTableName;
            osNewRTreeName += "_";
            osNewRTreeName += poNewGeomFieldDefn->GetNameRef();

            // Rename spatial index tables (not strictly needed, but for
            // consistency)
            eErr =
                SQLCommand(m_poDS->GetDB(),
                           CPLString().Printf(
                               "ALTER TABLE \"%s\" RENAME TO \"%s\"",
                               SQLEscapeName(m_osRTreeName.c_str()).c_str(),
                               SQLEscapeName(osNewRTreeName.c_str()).c_str()));
            if (eErr != OGRERR_NONE)
            {
                m_poDS->SoftRollbackTransaction();
                return OGRERR_FAILURE;
            }

            // Finally rename triggers (not strictly needed, but for
            // consistency)
            std::string osTriggerSQL;
            osTriggerSQL = ReturnSQLDropSpatialIndexTriggers();
            osTriggerSQL += ";";
            osTriggerSQL += ReturnSQLCreateSpatialIndexTriggers(
                nullptr, poNewGeomFieldDefn->GetNameRef());
            eErr = SQLCommand(m_poDS->GetDB(), osTriggerSQL.c_str());
            if (eErr != OGRERR_NONE)
            {
                m_poDS->SoftRollbackTransaction();
                return OGRERR_FAILURE;
            }
        }

        eErr = m_poDS->SoftCommitTransaction();
        if (eErr != OGRERR_NONE)
        {
            return OGRERR_FAILURE;
        }

        poGeomFieldDefn->SetName(poNewGeomFieldDefn->GetNameRef());

        if (bHasSpatialIndex)
        {
            m_osRTreeName = osNewRTreeName;
        }
    }

    if ((nFlagsIn & ALTER_GEOM_FIELD_DEFN_SRS_FLAG) != 0 ||
        (nFlagsIn & ALTER_GEOM_FIELD_DEFN_SRS_COORD_EPOCH_FLAG) != 0)
    {
        const auto poOldSRS = poGeomFieldDefn->GetSpatialRef();
        const auto poNewSRSRef = poNewGeomFieldDefn->GetSpatialRef();

        std::unique_ptr<OGRSpatialReference> poNewSRS;
        if ((nFlagsIn & ALTER_GEOM_FIELD_DEFN_SRS_FLAG) != 0)
        {
            if (poNewSRSRef != nullptr)
            {
                poNewSRS.reset(poNewSRSRef->Clone());
                if ((nFlagsIn & ALTER_GEOM_FIELD_DEFN_SRS_COORD_EPOCH_FLAG) ==
                    0)
                {
                    if (poOldSRS)
                        poNewSRS->SetCoordinateEpoch(
                            poOldSRS->GetCoordinateEpoch());
                }
            }
        }
        else if ((nFlagsIn & ALTER_GEOM_FIELD_DEFN_SRS_COORD_EPOCH_FLAG) != 0)
        {
            if (poOldSRS != nullptr)
            {
                poNewSRS.reset(poOldSRS->Clone());
                if (poNewSRSRef)
                    poNewSRS->SetCoordinateEpoch(
                        poNewSRSRef->GetCoordinateEpoch());
            }
        }

        const char *const apszOptions[] = {
            "IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES", nullptr};
        if ((poOldSRS == nullptr && poNewSRS != nullptr) ||
            (poOldSRS != nullptr && poNewSRS == nullptr) ||
            (poOldSRS != nullptr && poNewSRS != nullptr &&
             !poOldSRS->IsSame(poNewSRS.get(), apszOptions)))
        {
            // Temporary remove foreign key checks
            const GPKGTemporaryForeignKeyCheckDisabler
                oGPKGTemporaryForeignKeyCheckDisabler(m_poDS);

            if (m_poDS->SoftStartTransaction() != OGRERR_NONE)
                return OGRERR_FAILURE;

            const int nNewSRID = m_poDS->GetSrsId(poNewSRS.get());

            // Replace the old SRID by the new ones in geometry blobs
            uint32_t nNewSRID_LSB = nNewSRID;
            CPL_LSBPTR32(&nNewSRID_LSB);
            GByte abySRID_LSB[5] = {0, 0, 0, 0};
            memcpy(abySRID_LSB, &nNewSRID_LSB, 4);
            char *pszSRID_LSB_HEX = CPLBinaryToHex(4, abySRID_LSB);

            uint32_t nNewSRID_MSB = nNewSRID;
            CPL_MSBPTR32(&nNewSRID_MSB);
            GByte abySRID_MSB[5] = {0, 0, 0, 0};
            memcpy(abySRID_MSB, &nNewSRID_MSB, 4);
            char *pszSRID_MSB_HEX = CPLBinaryToHex(4, abySRID_MSB);

            // Black magic below...
            // the substr(hex(...) IN ('0','2',...'E') checks if bit 0 of the
            // 4th byte is 0 and use that to decide how to replace the old SRID
            // by the new one.
            CPLString osSQL;
            osSQL.Printf(
                "UPDATE \"%s\" SET \"%s\" = "
                "CAST(substr(\"%s\", 1, 4) || "
                "(CASE WHEN substr(hex(substr(\"%s\", 4, 1)),2) IN "
                "('0','2','4','6','8','A','C','E') "
                "THEN x'%s' ELSE x'%s' END) || substr(\"%s\", 9) AS BLOB) "
                "WHERE \"%s\" IS NOT NULL",
                SQLEscapeName(m_pszTableName).c_str(),
                SQLEscapeName(poGeomFieldDefn->GetNameRef()).c_str(),
                SQLEscapeName(poGeomFieldDefn->GetNameRef()).c_str(),
                SQLEscapeName(poGeomFieldDefn->GetNameRef()).c_str(),
                pszSRID_MSB_HEX, pszSRID_LSB_HEX,
                SQLEscapeName(poGeomFieldDefn->GetNameRef()).c_str(),
                SQLEscapeName(poGeomFieldDefn->GetNameRef()).c_str());
            OGRErr eErr = SQLCommand(m_poDS->GetDB(), osSQL.c_str());
            CPLFree(pszSRID_MSB_HEX);
            CPLFree(pszSRID_LSB_HEX);
            if (eErr != OGRERR_NONE)
            {
                m_poDS->SoftRollbackTransaction();
                return OGRERR_FAILURE;
            }

            char *pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_contents SET srs_id = %d WHERE table_name = '%q'",
                nNewSRID, m_pszTableName);
            eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
            sqlite3_free(pszSQL);
            if (eErr != OGRERR_NONE)
            {
                m_poDS->SoftRollbackTransaction();
                return OGRERR_FAILURE;
            }

            pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_geometry_columns SET srs_id = %d WHERE "
                "table_name = '%q' AND column_name = '%q'",
                nNewSRID, m_pszTableName, poGeomFieldDefn->GetNameRef());
            eErr = SQLCommand(m_poDS->GetDB(), pszSQL);
            sqlite3_free(pszSQL);
            if (eErr != OGRERR_NONE)
            {
                m_poDS->SoftRollbackTransaction();
                return OGRERR_FAILURE;
            }

            if (m_poDS->SoftCommitTransaction() != OGRERR_NONE)
            {
                return OGRERR_FAILURE;
            }

            m_iSrs = nNewSRID;
            OGRSpatialReference *poSRS = poNewSRS.release();
            poGeomFieldDefn->SetSpatialRef(poSRS);
            if (poSRS)
                poSRS->Release();
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ReorderFields()                            */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::ReorderFields(int *panMap)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (!CheckUpdatableTable("ReorderFields"))
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
    if (m_bThreadRTreeStarted)
        CancelAsyncRTree();
    if (!RunDeferredSpatialIndexUpdate())
        return OGRERR_FAILURE;

    /* -------------------------------------------------------------------- */
    /*      Drop any iterator since we change the DB structure              */
    /* -------------------------------------------------------------------- */
    m_poDS->ResetReadingAllLayers();

    /* -------------------------------------------------------------------- */
    /*      Build list of old fields, and the list of new fields.           */
    /* -------------------------------------------------------------------- */
    std::vector<OGRFieldDefn *> apoFields;
    for (int iField = 0; iField < m_poFeatureDefn->GetFieldCount(); iField++)
    {
        OGRFieldDefn *poFieldDefn =
            m_poFeatureDefn->GetFieldDefn(panMap[iField]);
        apoFields.push_back(poFieldDefn);
    }

    const CPLString osFieldListForSelect(BuildSelectFieldList(apoFields));
    const CPLString osColumnsForCreate(GetColumnsOfCreateTable(apoFields));

    /* -------------------------------------------------------------------- */
    /*      Recreate table in a transaction                                 */
    /* -------------------------------------------------------------------- */
    if (m_poDS->SoftStartTransaction() != OGRERR_NONE)
        return OGRERR_FAILURE;

    eErr = RecreateTable(osColumnsForCreate, osFieldListForSelect);

    /* -------------------------------------------------------------------- */
    /*      Finish                                                          */
    /* -------------------------------------------------------------------- */
    if (eErr == OGRERR_NONE)
    {
        eErr = m_poDS->SoftCommitTransaction();

        if (eErr == OGRERR_NONE)
        {
            eErr = whileUnsealing(m_poFeatureDefn)->ReorderFieldDefns(panMap);
        }

        if (eErr == OGRERR_NONE)
        {
            // We have recreated the table from scratch, and lost the
            // generated column property
            std::fill(m_abGeneratedColumns.begin(), m_abGeneratedColumns.end(),
                      false);
        }

        ResetReading();
    }
    else
    {
        m_poDS->SoftRollbackTransaction();
    }

    return eErr;
}

/************************************************************************/
/*                   OGR_GPKG_GeometryTypeAggregate()                   */
/************************************************************************/

namespace
{
struct GeometryTypeAggregateContext
{
    sqlite3 *m_hDB = nullptr;
    int m_nFlags = 0;
    bool m_bIsGeometryTypeAggregateInterrupted = false;
    std::map<OGRwkbGeometryType, int64_t> m_oMapCount{};
    std::set<OGRwkbGeometryType> m_oSetNotNull{};

    explicit GeometryTypeAggregateContext(sqlite3 *hDB, int nFlags)
        : m_hDB(hDB), m_nFlags(nFlags)
    {
    }

    GeometryTypeAggregateContext(const GeometryTypeAggregateContext &) = delete;
    GeometryTypeAggregateContext &
    operator=(const GeometryTypeAggregateContext &) = delete;

    void SetGeometryTypeAggregateInterrupted(bool b)
    {
        m_bIsGeometryTypeAggregateInterrupted = b;
        if (b)
            sqlite3_interrupt(m_hDB);
    }
};

}  // namespace

static void OGR_GPKG_GeometryTypeAggregate_Step(sqlite3_context *pContext,
                                                int /*argc*/,
                                                sqlite3_value **argv)
{
    const GByte *pabyBLOB =
        reinterpret_cast<const GByte *>(sqlite3_value_blob(argv[0]));

    auto poContext = static_cast<GeometryTypeAggregateContext *>(
        sqlite3_user_data(pContext));

    OGRwkbGeometryType eGeometryType = wkbNone;
    OGRErr err = OGRERR_FAILURE;
    if (pabyBLOB != nullptr)
    {
        GPkgHeader sHeader;
        const int nBLOBLen = sqlite3_value_bytes(argv[0]);
        if (GPkgHeaderFromWKB(pabyBLOB, nBLOBLen, &sHeader) == OGRERR_NONE &&
            static_cast<size_t>(nBLOBLen) >= sHeader.nHeaderLen + 5)
        {
            err = OGRReadWKBGeometryType(pabyBLOB + sHeader.nHeaderLen,
                                         wkbVariantIso, &eGeometryType);
            if (eGeometryType == wkbGeometryCollection25D &&
                (poContext->m_nFlags & OGR_GGT_GEOMCOLLECTIONZ_TINZ) != 0)
            {
                auto poGeom = std::unique_ptr<OGRGeometry>(
                    GPkgGeometryToOGR(pabyBLOB, nBLOBLen, nullptr));
                if (poGeom)
                {
                    const auto poGC = poGeom->toGeometryCollection();
                    if (poGC->getNumGeometries() > 0)
                    {
                        auto eSubGeomType =
                            poGC->getGeometryRef(0)->getGeometryType();
                        if (eSubGeomType == wkbTINZ)
                            eGeometryType = wkbTINZ;
                    }
                }
            }
        }
    }
    else
    {
        // NULL geometry
        err = OGRERR_NONE;
    }
    if (err == OGRERR_NONE)
    {
        ++poContext->m_oMapCount[eGeometryType];
        if (eGeometryType != wkbNone &&
            (poContext->m_nFlags & OGR_GGT_STOP_IF_MIXED) != 0)
        {
            poContext->m_oSetNotNull.insert(eGeometryType);
            if (poContext->m_oSetNotNull.size() == 2)
            {
                poContext->SetGeometryTypeAggregateInterrupted(true);
            }
        }
    }
}

static void OGR_GPKG_GeometryTypeAggregate_Finalize(sqlite3_context *)
{
}

/************************************************************************/
/*                         GetGeometryTypes()                           */
/************************************************************************/

OGRGeometryTypeCounter *OGRGeoPackageTableLayer::GetGeometryTypes(
    int iGeomField, int nFlagsGGT, int &nEntryCountOut,
    GDALProgressFunc pfnProgress, void *pProgressData)
{
    OGRFeatureDefn *poDefn = GetLayerDefn();

    /* -------------------------------------------------------------------- */
    /*      Deferred actions, reset state.                                   */
    /* -------------------------------------------------------------------- */
    RunDeferredCreationIfNecessary();
    if (!RunDeferredSpatialIndexUpdate())
    {
        nEntryCountOut = 0;
        return nullptr;
    }

    const int nGeomFieldCount = poDefn->GetGeomFieldCount();
    if (iGeomField < 0 || iGeomField >= nGeomFieldCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for iGeomField");
        nEntryCountOut = 0;
        return nullptr;
    }

#ifdef SQLITE_HAS_PROGRESS_HANDLER
    struct CancelCallback
    {
        sqlite3 *m_hDB = nullptr;
        GDALProgressFunc m_pfnProgress = nullptr;
        void *m_pProgressData = nullptr;

        CancelCallback(sqlite3 *hDB, GDALProgressFunc pfnProgressIn,
                       void *pProgressDataIn)
            : m_hDB(hDB),
              m_pfnProgress(pfnProgressIn != GDALDummyProgress ? pfnProgressIn
                                                               : nullptr),
              m_pProgressData(pProgressDataIn)
        {
            if (m_pfnProgress)
            {
                // If changing that value, update
                // ogr_gpkg.py::test_ogr_gpkg_get_geometry_types
                constexpr int COUNT_VM_INSTRUCTIONS = 1000;
                sqlite3_progress_handler(m_hDB, COUNT_VM_INSTRUCTIONS,
                                         ProgressHandler, this);
            }
        }

        ~CancelCallback()
        {
            if (m_pfnProgress)
            {
                sqlite3_progress_handler(m_hDB, 0, nullptr, nullptr);
            }
        }

        CancelCallback(const CancelCallback &) = delete;
        CancelCallback &operator=(const CancelCallback &) = delete;

        static int ProgressHandler(void *pData)
        {
            CancelCallback *psCancelCallback =
                static_cast<CancelCallback *>(pData);
            return psCancelCallback->m_pfnProgress != nullptr &&
                           psCancelCallback->m_pfnProgress(
                               0.0, "", psCancelCallback->m_pProgressData)
                       ? 0
                       : 1;
        }
    };

    CancelCallback oCancelCallback(m_poDS->hDB, pfnProgress, pProgressData);
#else
    CPL_IGNORE_RET_VAL(pfnProgress);
    CPL_IGNORE_RET_VAL(pProgressData);
#endif

    // For internal use only

    GeometryTypeAggregateContext sContext(m_poDS->hDB, nFlagsGGT);

    CPLString osFuncName;
    osFuncName.Printf("OGR_GPKG_GeometryTypeAggregate_INTERNAL_%p", &sContext);

    sqlite3_create_function(m_poDS->hDB, osFuncName.c_str(), 1, SQLITE_UTF8,
                            &sContext, nullptr,
                            OGR_GPKG_GeometryTypeAggregate_Step,
                            OGR_GPKG_GeometryTypeAggregate_Finalize);

    // Using this aggregate function is slightly faster than using
    // sqlite3_step() to loop over each geometry blob (650 ms vs 750ms on a 1.6
    // GB db with 3.3 million features)
    char *pszSQL = sqlite3_mprintf(
        "SELECT %s(\"%w\") FROM \"%w\"%s", osFuncName.c_str(),
        poDefn->GetGeomFieldDefn(iGeomField)->GetNameRef(), m_pszTableName,
        m_soFilter.empty() ? "" : (" WHERE " + m_soFilter).c_str());
    char *pszErrMsg = nullptr;
    const int rc =
        sqlite3_exec(m_poDS->hDB, pszSQL, nullptr, nullptr, &(pszErrMsg));

    // Delete function
    sqlite3_create_function(m_poDS->GetDB(), osFuncName.c_str(), 1, SQLITE_UTF8,
                            nullptr, nullptr, nullptr, nullptr);

    if (rc != SQLITE_OK && !sContext.m_bIsGeometryTypeAggregateInterrupted)
    {
        if (rc != SQLITE_INTERRUPT)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "sqlite3_exec(%s) failed: %s",
                     pszSQL, pszErrMsg);
        }
        sqlite3_free(pszErrMsg);
        sqlite3_free(pszSQL);
        nEntryCountOut = 0;
        return nullptr;
    }
    sqlite3_free(pszErrMsg);
    sqlite3_free(pszSQL);

    // Format result
    nEntryCountOut = static_cast<int>(sContext.m_oMapCount.size());
    OGRGeometryTypeCounter *pasRet = static_cast<OGRGeometryTypeCounter *>(
        CPLCalloc(1 + nEntryCountOut, sizeof(OGRGeometryTypeCounter)));
    int i = 0;
    for (const auto &sEntry : sContext.m_oMapCount)
    {
        pasRet[i].eGeomType = sEntry.first;
        pasRet[i].nCount = sEntry.second;
        ++i;
    }
    return pasRet;
}

/************************************************************************/
/*                    OGR_GPKG_FillArrowArray_Step()                    */
/************************************************************************/

void OGR_GPKG_FillArrowArray_Step(sqlite3_context *pContext, int /*argc*/,
                                  sqlite3_value **argv);

void OGR_GPKG_FillArrowArray_Step(sqlite3_context *pContext, int /*argc*/,
                                  sqlite3_value **argv)
{
    auto psFillArrowArray = static_cast<OGRGPKGTableLayerFillArrowArray *>(
        sqlite3_user_data(pContext));

    if (psFillArrowArray->nCountRows >=
        psFillArrowArray->psHelper->m_nMaxBatchSize)
    {
        if (psFillArrowArray->bAsynchronousMode)
        {
            std::unique_lock<std::mutex> oLock(psFillArrowArray->oMutex);
            psFillArrowArray->psHelper->Shrink(psFillArrowArray->nCountRows);
            psFillArrowArray->oCV.notify_one();
            while (psFillArrowArray->nCountRows > 0)
            {
                psFillArrowArray->oCV.wait(oLock);
            }
            // Note that psFillArrowArray->psHelper.get() will generally now be
            // different from before the wait()
        }
        else
        {
            // should not happen !
            psFillArrowArray->osErrorMsg =
                "OGR_GPKG_FillArrowArray_Step() got more rows than expected!";
            sqlite3_interrupt(psFillArrowArray->hDB);
            psFillArrowArray->bErrorOccurred = true;
            return;
        }
    }
    if (psFillArrowArray->nCountRows < 0)
        return;

    if (psFillArrowArray->nMemLimit == 0)
        psFillArrowArray->nMemLimit = OGRArrowArrayHelper::GetMemLimit();
    const auto nMemLimit = psFillArrowArray->nMemLimit;
    const int SQLITE_MAX_FUNCTION_ARG =
        sqlite3_limit(psFillArrowArray->hDB, SQLITE_LIMIT_FUNCTION_ARG, -1);
begin:
    const int iFeat = psFillArrowArray->nCountRows;
    auto psHelper = psFillArrowArray->psHelper.get();
    int iCol = 0;
    const int iFieldStart = sqlite3_value_int(argv[iCol]);
    ++iCol;
    int iField = std::max(0, iFieldStart);

    GIntBig nFID;
    if (iFieldStart < 0)
    {
        nFID = sqlite3_value_int64(argv[iCol]);
        iCol++;
        if (psHelper->m_panFIDValues)
        {
            psHelper->m_panFIDValues[iFeat] = nFID;
        }
        psFillArrowArray->nCurFID = nFID;
    }
    else
    {
        nFID = psFillArrowArray->nCurFID;
    }

    if (iFieldStart < 0 && !psHelper->m_mapOGRGeomFieldToArrowField.empty() &&
        psHelper->m_mapOGRGeomFieldToArrowField[0] >= 0)
    {
        const int iArrowField = psHelper->m_mapOGRGeomFieldToArrowField[0];
        auto psArray = psHelper->m_out_array->children[iArrowField];
        size_t nWKBSize = 0;
        const int nSqlite3ColType = sqlite3_value_type(argv[iCol]);
        if (nSqlite3ColType == SQLITE_BLOB)
        {
            GPkgHeader oHeader;
            memset(&oHeader, 0, sizeof(oHeader));

            const GByte *pabyWkb = nullptr;
            const int nBlobSize = sqlite3_value_bytes(argv[iCol]);
            // coverity[tainted_data_return]
            const GByte *pabyBlob =
                static_cast<const GByte *>(sqlite3_value_blob(argv[iCol]));
            std::vector<GByte> abyWkb;
            if (nBlobSize >= 8 && pabyBlob && pabyBlob[0] == 'G' &&
                pabyBlob[1] == 'P')
            {
                if (psFillArrowArray->poLayer->m_bUndoDiscardCoordLSBOnReading)
                {
                    OGRGeometry *poGeomPtr =
                        GPkgGeometryToOGR(pabyBlob, nBlobSize, nullptr);
                    if (poGeomPtr)
                    {
                        poGeomPtr->roundCoordinates(
                            psFillArrowArray->poFeatureDefn->GetGeomFieldDefn(0)
                                ->GetCoordinatePrecision());
                        nWKBSize = poGeomPtr->WkbSize();
                        abyWkb.resize(nWKBSize);
                        if (poGeomPtr->exportToWkb(wkbNDR, abyWkb.data(),
                                                   wkbVariantIso) !=
                            OGRERR_NONE)
                        {
                            nWKBSize = 0;
                        }
                        else
                        {
                            pabyWkb = abyWkb.data();
                        }
                        delete poGeomPtr;
                    }
                }
                else
                {
                    /* Read header */
                    OGRErr err =
                        GPkgHeaderFromWKB(pabyBlob, nBlobSize, &oHeader);
                    if (err == OGRERR_NONE)
                    {
                        /* WKB pointer */
                        pabyWkb = pabyBlob + oHeader.nHeaderLen;
                        nWKBSize = nBlobSize - oHeader.nHeaderLen;
                    }
                }
            }
            else if (nBlobSize > 0 && pabyBlob)
            {
                // Try also spatialite geometry blobs, although that is
                // not really expected...
                OGRGeometry *poGeomPtr = nullptr;
                if (OGRSQLiteImportSpatiaLiteGeometry(
                        pabyBlob, nBlobSize, &poGeomPtr) != OGRERR_NONE)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unable to read geometry");
                }
                else
                {
                    nWKBSize = poGeomPtr->WkbSize();
                    abyWkb.resize(nWKBSize);
                    if (poGeomPtr->exportToWkb(wkbNDR, abyWkb.data(),
                                               wkbVariantIso) != OGRERR_NONE)
                    {
                        nWKBSize = 0;
                    }
                    else
                    {
                        pabyWkb = abyWkb.data();
                    }
                }
                delete poGeomPtr;
            }

            if (nWKBSize != 0)
            {
                // Deal with spatial filter
                if (psFillArrowArray->poLayerForFilterGeom)
                {
                    OGREnvelope sEnvelope;
                    bool bEnvelopeAlreadySet = false;
                    if (oHeader.bEmpty)
                    {
                        bEnvelopeAlreadySet = true;
                    }
                    else if (oHeader.bExtentHasXY)
                    {
                        bEnvelopeAlreadySet = true;
                        sEnvelope.MinX = oHeader.MinX;
                        sEnvelope.MinY = oHeader.MinY;
                        sEnvelope.MaxX = oHeader.MaxX;
                        sEnvelope.MaxY = oHeader.MaxY;
                    }

                    if (!psFillArrowArray->poLayerForFilterGeom
                             ->FilterWKBGeometry(pabyWkb, nWKBSize,
                                                 bEnvelopeAlreadySet,
                                                 sEnvelope))
                    {
                        return;
                    }
                }

                if (psFillArrowArray->nCountRows > 0)
                {
                    auto panOffsets = static_cast<int32_t *>(
                        const_cast<void *>(psArray->buffers[1]));
                    const uint32_t nCurLength =
                        static_cast<uint32_t>(panOffsets[iFeat]);
                    if (nWKBSize <= nMemLimit &&
                        nWKBSize > nMemLimit - nCurLength)
                    {
                        CPLDebug("GPKG",
                                 "OGR_GPKG_FillArrowArray_Step(): premature "
                                 "notification of %d features to consumer due "
                                 "to too big array",
                                 psFillArrowArray->nCountRows);
                        psFillArrowArray->bMemoryLimitReached = true;
                        if (psFillArrowArray->bAsynchronousMode)
                        {
                            std::unique_lock<std::mutex> oLock(
                                psFillArrowArray->oMutex);
                            psFillArrowArray->psHelper->Shrink(
                                psFillArrowArray->nCountRows);
                            psFillArrowArray->oCV.notify_one();
                            while (psFillArrowArray->nCountRows > 0)
                            {
                                psFillArrowArray->oCV.wait(oLock);
                            }
                            goto begin;
                        }
                        else
                        {
                            sqlite3_interrupt(psFillArrowArray->hDB);
                            return;
                        }
                    }
                }

                GByte *outPtr = psHelper->GetPtrForStringOrBinary(
                    iArrowField, iFeat, nWKBSize);
                if (outPtr == nullptr)
                {
                    goto error;
                }
                memcpy(outPtr, pabyWkb, nWKBSize);
            }
            else
            {
                psHelper->SetEmptyStringOrBinary(psArray, iFeat);
            }
        }

        if (nWKBSize == 0)
        {
            if (!psHelper->SetNull(iArrowField, iFeat))
            {
                goto error;
            }
        }
        iCol++;
    }

    for (; iField < psHelper->m_nFieldCount; iField++)
    {
        const int iArrowField = psHelper->m_mapOGRFieldToArrowField[iField];
        if (iArrowField < 0)
            continue;
        if (iCol == SQLITE_MAX_FUNCTION_ARG)
            break;

        const OGRFieldDefn *poFieldDefn =
            psFillArrowArray->poFeatureDefn->GetFieldDefnUnsafe(iField);

        auto psArray = psHelper->m_out_array->children[iArrowField];

        const int nSqlite3ColType = sqlite3_value_type(argv[iCol]);
        if (nSqlite3ColType == SQLITE_NULL)
        {
            if (!psHelper->SetNull(iArrowField, iFeat))
            {
                goto error;
            }
            iCol++;
            continue;
        }

        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
            {
                const int nVal = sqlite3_value_int(argv[iCol]);
                if (poFieldDefn->GetSubType() == OFSTBoolean)
                {
                    if (nVal != 0)
                    {
                        psHelper->SetBoolOn(psArray, iFeat);
                    }
                }
                else if (poFieldDefn->GetSubType() == OFSTInt16)
                {
                    psHelper->SetInt16(psArray, iFeat,
                                       static_cast<int16_t>(nVal));
                }
                else
                {
                    psHelper->SetInt32(psArray, iFeat, nVal);
                }
                break;
            }

            case OFTInteger64:
            {
                psHelper->SetInt64(psArray, iFeat,
                                   sqlite3_value_int64(argv[iCol]));
                break;
            }

            case OFTReal:
            {
                const double dfVal = sqlite3_value_double(argv[iCol]);
                if (poFieldDefn->GetSubType() == OFSTFloat32)
                {
                    psHelper->SetFloat(psArray, iFeat,
                                       static_cast<float>(dfVal));
                }
                else
                {
                    psHelper->SetDouble(psArray, iFeat, dfVal);
                }
                break;
            }

            case OFTBinary:
            {
                const uint32_t nBytes =
                    static_cast<uint32_t>(sqlite3_value_bytes(argv[iCol]));
                // coverity[tainted_data_return]
                const void *pabyData = sqlite3_value_blob(argv[iCol]);
                if (pabyData != nullptr || nBytes == 0)
                {
                    if (psFillArrowArray->nCountRows > 0)
                    {
                        auto panOffsets = static_cast<int32_t *>(
                            const_cast<void *>(psArray->buffers[1]));
                        const uint32_t nCurLength =
                            static_cast<uint32_t>(panOffsets[iFeat]);
                        if (nBytes <= nMemLimit &&
                            nBytes > nMemLimit - nCurLength)
                        {
                            CPLDebug("GPKG",
                                     "OGR_GPKG_FillArrowArray_Step(): "
                                     "premature notification of %d features to "
                                     "consumer due to too big array",
                                     psFillArrowArray->nCountRows);
                            psFillArrowArray->bMemoryLimitReached = true;
                            if (psFillArrowArray->bAsynchronousMode)
                            {
                                std::unique_lock<std::mutex> oLock(
                                    psFillArrowArray->oMutex);
                                psFillArrowArray->psHelper->Shrink(
                                    psFillArrowArray->nCountRows);
                                psFillArrowArray->oCV.notify_one();
                                while (psFillArrowArray->nCountRows > 0)
                                {
                                    psFillArrowArray->oCV.wait(oLock);
                                }
                                goto begin;
                            }
                            else
                            {
                                sqlite3_interrupt(psFillArrowArray->hDB);
                                return;
                            }
                        }
                    }

                    GByte *outPtr = psHelper->GetPtrForStringOrBinary(
                        iArrowField, iFeat, nBytes);
                    if (outPtr == nullptr)
                    {
                        goto error;
                    }
                    if (nBytes)
                        memcpy(outPtr, pabyData, nBytes);
                }
                else
                {
                    psHelper->SetEmptyStringOrBinary(psArray, iFeat);
                }
                break;
            }

            case OFTDate:
            {
                OGRField ogrField;
                const auto pszTxt = reinterpret_cast<const char *>(
                    sqlite3_value_text(argv[iCol]));
                if (pszTxt != nullptr &&
                    psFillArrowArray->poLayer->ParseDateField(
                        pszTxt, &ogrField, poFieldDefn, nFID))
                {
                    psHelper->SetDate(psArray, iFeat,
                                      psFillArrowArray->brokenDown, ogrField);
                }
                break;
            }

            case OFTDateTime:
            {
                OGRField ogrField;
                const auto pszTxt = reinterpret_cast<const char *>(
                    sqlite3_value_text(argv[iCol]));
                if (pszTxt != nullptr &&
                    psFillArrowArray->poLayer->ParseDateTimeField(
                        pszTxt, &ogrField, poFieldDefn, nFID))
                {
                    psHelper->SetDateTime(
                        psArray, iFeat, psFillArrowArray->brokenDown,
                        psHelper->m_anTZFlags[iField], ogrField);
                }
                break;
            }

            case OFTString:
            {
                const auto pszTxt = reinterpret_cast<const char *>(
                    sqlite3_value_text(argv[iCol]));
                if (pszTxt != nullptr)
                {
                    const size_t nBytes = strlen(pszTxt);
                    if (psFillArrowArray->nCountRows > 0)
                    {
                        auto panOffsets = static_cast<int32_t *>(
                            const_cast<void *>(psArray->buffers[1]));
                        const uint32_t nCurLength =
                            static_cast<uint32_t>(panOffsets[iFeat]);
                        if (nBytes <= nMemLimit &&
                            nBytes > nMemLimit - nCurLength)
                        {
                            CPLDebug("GPKG",
                                     "OGR_GPKG_FillArrowArray_Step(): "
                                     "premature notification of %d features to "
                                     "consumer due to too big array",
                                     psFillArrowArray->nCountRows);
                            psFillArrowArray->bMemoryLimitReached = true;
                            if (psFillArrowArray->bAsynchronousMode)
                            {
                                std::unique_lock<std::mutex> oLock(
                                    psFillArrowArray->oMutex);
                                psFillArrowArray->psHelper->Shrink(
                                    psFillArrowArray->nCountRows);
                                psFillArrowArray->oCV.notify_one();
                                while (psFillArrowArray->nCountRows > 0)
                                {
                                    psFillArrowArray->oCV.wait(oLock);
                                }
                                goto begin;
                            }
                            else
                            {
                                sqlite3_interrupt(psFillArrowArray->hDB);
                                return;
                            }
                        }
                    }

                    GByte *outPtr = psHelper->GetPtrForStringOrBinary(
                        iArrowField, iFeat, nBytes);
                    if (outPtr == nullptr)
                    {
                        goto error;
                    }
                    if (nBytes)
                        memcpy(outPtr, pszTxt, nBytes);
                }
                else
                {
                    psHelper->SetEmptyStringOrBinary(psArray, iFeat);
                }
                break;
            }

            default:
                break;
        }

        iCol++;
    }

    if (iField == psHelper->m_nFieldCount)
        psFillArrowArray->nCountRows++;
    return;

error:
    sqlite3_interrupt(psFillArrowArray->hDB);
    psFillArrowArray->bErrorOccurred = true;
}

/************************************************************************/
/*                   OGR_GPKG_FillArrowArray_Finalize()                 */
/************************************************************************/

static void OGR_GPKG_FillArrowArray_Finalize(sqlite3_context * /*pContext*/)
{
}

/************************************************************************/
/*                    GetNextArrowArrayAsynchronous()                   */
/************************************************************************/

int OGRGeoPackageTableLayer::GetNextArrowArrayAsynchronous(
    struct ArrowArrayStream *stream, struct ArrowArray *out_array)
{
    memset(out_array, 0, sizeof(*out_array));

    m_bGetNextArrowArrayCalledSinceResetReading = true;

    if (m_poFillArrowArray)
    {
        std::lock_guard<std::mutex> oLock(m_poFillArrowArray->oMutex);
        if (m_poFillArrowArray->bIsFinished)
        {
            return 0;
        }
    }

    auto psHelper = std::make_unique<OGRArrowArrayHelper>(
        m_poDS, m_poFeatureDefn, m_aosArrowArrayStreamOptions, out_array);
    if (out_array->release == nullptr)
    {
        return ENOMEM;
    }

    if (m_poFillArrowArray == nullptr)
    {
        // Check that the total number of arguments passed to
        // OGR_GPKG_FillArrowArray_INTERNAL() doesn't exceed SQLITE_MAX_FUNCTION_ARG
        // If it does, we cannot reliably use GetNextArrowArrayAsynchronous() in
        // the situation where the ArrowArray would exceed the nMemLimit.
        // So be on the safe side, and rely on the base OGRGeoPackageLayer
        // implementation
        const int SQLITE_MAX_FUNCTION_ARG =
            sqlite3_limit(m_poDS->GetDB(), SQLITE_LIMIT_FUNCTION_ARG, -1);
        int nCountArgs = 1     // field index
                         + 1;  // FID column
        if (!psHelper->m_mapOGRGeomFieldToArrowField.empty() &&
            psHelper->m_mapOGRGeomFieldToArrowField[0] >= 0)
        {
            ++nCountArgs;
        }
        for (int iField = 0; iField < psHelper->m_nFieldCount; iField++)
        {
            const int iArrowField = psHelper->m_mapOGRFieldToArrowField[iField];
            if (iArrowField >= 0)
            {
                if (nCountArgs == SQLITE_MAX_FUNCTION_ARG)
                {
                    psHelper.reset();
                    if (out_array->release)
                        out_array->release(out_array);
                    return OGRGeoPackageLayer::GetNextArrowArray(stream,
                                                                 out_array);
                }
                ++nCountArgs;
            }
        }

        m_poFillArrowArray =
            std::make_unique<OGRGPKGTableLayerFillArrowArray>();
        m_poFillArrowArray->psHelper = std::move(psHelper);
        m_poFillArrowArray->nCountRows = 0;
        m_poFillArrowArray->bErrorOccurred = false;
        m_poFillArrowArray->poFeatureDefn = m_poFeatureDefn;
        m_poFillArrowArray->poLayer = this;
        m_poFillArrowArray->hDB = m_poDS->GetDB();
        memset(&m_poFillArrowArray->brokenDown, 0,
               sizeof(m_poFillArrowArray->brokenDown));
        m_poFillArrowArray->nMaxBatchSize =
            OGRArrowArrayHelper::GetMaxFeaturesInBatch(
                m_aosArrowArrayStreamOptions);
        m_poFillArrowArray->bAsynchronousMode = true;
        if (m_poFilterGeom)
            m_poFillArrowArray->poLayerForFilterGeom = this;

        try
        {
            m_oThreadNextArrowArray = std::thread(
                [this]() { GetNextArrowArrayAsynchronousWorker(); });
        }
        catch (const std::exception &e)
        {
            m_poFillArrowArray.reset();
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot start worker thread: %s", e.what());
            out_array->release(out_array);
            return ENOMEM;
        }
    }
    else
    {
        std::lock_guard<std::mutex> oLock(m_poFillArrowArray->oMutex);
        if (m_poFillArrowArray->bErrorOccurred)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                     m_poFillArrowArray->osErrorMsg.c_str());
            out_array->release(out_array);
            return EIO;
        }

        // Resume worker thread
        m_poFillArrowArray->psHelper = std::move(psHelper);
        m_poFillArrowArray->nCountRows = 0;
        m_poFillArrowArray->oCV.notify_one();
    }

    // Wait for GetNextArrowArrayAsynchronousWorker() /
    // OGR_GPKG_FillArrowArray_Step() to have generated a result set (or an
    // error)
    bool bIsFinished;
    {
        std::unique_lock<std::mutex> oLock(m_poFillArrowArray->oMutex);
        while (m_poFillArrowArray->nCountRows == 0 &&
               !m_poFillArrowArray->bIsFinished)
        {
            m_poFillArrowArray->oCV.wait(oLock);
        }
        bIsFinished = m_poFillArrowArray->bIsFinished;
    }

    if (m_poFillArrowArray->bErrorOccurred)
    {
        m_oThreadNextArrowArray.join();
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 m_poFillArrowArray->osErrorMsg.c_str());
        m_poFillArrowArray->psHelper->ClearArray();
        return EIO;
    }
    else if (bIsFinished)
    {
        m_oThreadNextArrowArray.join();
    }

    return 0;
}

/************************************************************************/
/*                  GetNextArrowArrayAsynchronousWorker()               */
/************************************************************************/

void OGRGeoPackageTableLayer::GetNextArrowArrayAsynchronousWorker()
{
    sqlite3_create_function(
        m_poDS->GetDB(), "OGR_GPKG_FillArrowArray_INTERNAL", -1,
        SQLITE_UTF8 | SQLITE_DETERMINISTIC, m_poFillArrowArray.get(), nullptr,
        OGR_GPKG_FillArrowArray_Step, OGR_GPKG_FillArrowArray_Finalize);

    std::string osSQL;
    osSQL = "SELECT OGR_GPKG_FillArrowArray_INTERNAL(-1,";

    const auto AddFields = [this, &osSQL]()
    {
        if (m_pszFidColumn)
        {
            osSQL += "m.\"";
            osSQL += SQLEscapeName(m_pszFidColumn);
            osSQL += '"';
        }
        else
        {
            osSQL += "NULL";
        }

        if (!m_poFillArrowArray->psHelper->m_mapOGRGeomFieldToArrowField
                 .empty() &&
            m_poFillArrowArray->psHelper->m_mapOGRGeomFieldToArrowField[0] >= 0)
        {
            osSQL += ",m.\"";
            osSQL += SQLEscapeName(GetGeometryColumn());
            osSQL += '"';
        }
        for (int iField = 0;
             iField < m_poFillArrowArray->psHelper->m_nFieldCount; iField++)
        {
            const int iArrowField =
                m_poFillArrowArray->psHelper->m_mapOGRFieldToArrowField[iField];
            if (iArrowField >= 0)
            {
                const OGRFieldDefn *poFieldDefn =
                    m_poFeatureDefn->GetFieldDefnUnsafe(iField);
                osSQL += ",m.\"";
                osSQL += SQLEscapeName(poFieldDefn->GetNameRef());
                osSQL += '"';
            }
        }
    };

    AddFields();

    osSQL += ") FROM ";
    if (m_iNextShapeId > 0)
    {
        osSQL += "(SELECT ";
        AddFields();
        osSQL += " FROM ";
    }
    osSQL += '\"';
    osSQL += SQLEscapeName(m_pszTableName);
    osSQL += "\" m";
    if (!m_soFilter.empty())
    {
        if (m_poFilterGeom != nullptr && m_pszAttrQueryString == nullptr &&
            HasSpatialIndex())
        {
            OGREnvelope sEnvelope;

            m_poFilterGeom->getEnvelope(&sEnvelope);

            bool bUseSpatialIndex = true;
            if (m_poExtent && sEnvelope.MinX <= m_poExtent->MinX &&
                sEnvelope.MinY <= m_poExtent->MinY &&
                sEnvelope.MaxX >= m_poExtent->MaxX &&
                sEnvelope.MaxY >= m_poExtent->MaxY)
            {
                // Selecting from spatial filter on whole extent can be rather
                // slow. So use function based filtering, just in case the
                // advertized global extent might be wrong. Otherwise we might
                // just discard completely the spatial filter.
                bUseSpatialIndex = false;
            }

            if (bUseSpatialIndex && !CPLIsInf(sEnvelope.MinX) &&
                !CPLIsInf(sEnvelope.MinY) && !CPLIsInf(sEnvelope.MaxX) &&
                !CPLIsInf(sEnvelope.MaxY))
            {
                osSQL +=
                    CPLSPrintf(" JOIN \"%s\" r "
                               "ON m.\"%s\" = r.id WHERE "
                               "r.maxx >= %.12f AND r.minx <= %.12f AND "
                               "r.maxy >= %.12f AND r.miny <= %.12f",
                               SQLEscapeName(m_osRTreeName).c_str(),
                               SQLEscapeName(m_osFIDForRTree).c_str(),
                               sEnvelope.MinX - 1e-11, sEnvelope.MaxX + 1e-11,
                               sEnvelope.MinY - 1e-11, sEnvelope.MaxY + 1e-11);
            }
        }
        else
        {
            osSQL += " WHERE ";
            osSQL += m_soFilter;
        }
    }

    if (m_iNextShapeId > 0)
        osSQL +=
            CPLSPrintf(" LIMIT -1 OFFSET " CPL_FRMT_GIB ") m", m_iNextShapeId);

    // CPLDebug("GPKG", "%s", osSQL.c_str());

    char *pszErrMsg = nullptr;
    if (sqlite3_exec(m_poDS->GetDB(), osSQL.c_str(), nullptr, nullptr,
                     &pszErrMsg) != SQLITE_OK)
    {
        m_poFillArrowArray->bErrorOccurred = true;
        m_poFillArrowArray->osErrorMsg =
            pszErrMsg ? pszErrMsg : "unknown error";
    }
    sqlite3_free(pszErrMsg);

    // Delete function
    sqlite3_create_function(m_poDS->GetDB(), "OGR_GPKG_FillArrowArray_INTERNAL",
                            -1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
                            nullptr, nullptr, nullptr);

    std::lock_guard<std::mutex> oLock(m_poFillArrowArray->oMutex);
    m_poFillArrowArray->bIsFinished = true;
    if (m_poFillArrowArray->nCountRows >= 0)
    {
        m_poFillArrowArray->psHelper->Shrink(m_poFillArrowArray->nCountRows);
        if (m_poFillArrowArray->nCountRows == 0)
        {
            m_poFillArrowArray->psHelper->ClearArray();
        }
    }
    m_poFillArrowArray->oCV.notify_one();
}

/************************************************************************/
/*                      GetNextArrowArray()                             */
/************************************************************************/

int OGRGeoPackageTableLayer::GetNextArrowArray(struct ArrowArrayStream *stream,
                                               struct ArrowArray *out_array)
{
    if (!m_bFeatureDefnCompleted)
        GetLayerDefn();
    if (m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE)
    {
        memset(out_array, 0, sizeof(*out_array));
        return EIO;
    }

    if (m_poFilterGeom != nullptr)
    {
        // Both are exclusive
        CreateSpatialIndexIfNecessary();
        if (!RunDeferredSpatialIndexUpdate())
        {
            memset(out_array, 0, sizeof(*out_array));
            return EIO;
        }
    }

    if (CPLTestBool(CPLGetConfigOption("OGR_GPKG_STREAM_BASE_IMPL", "NO")))
    {
        return OGRGeoPackageLayer::GetNextArrowArray(stream, out_array);
    }

    if (m_nIsCompatOfOptimizedGetNextArrowArray == FALSE ||
        m_pszFidColumn == nullptr || !m_soFilter.empty() ||
        m_poFillArrowArray ||
        (!m_bGetNextArrowArrayCalledSinceResetReading && m_iNextShapeId > 0))
    {
        return GetNextArrowArrayAsynchronous(stream, out_array);
    }

    // We can use this optimized version only if there is no hole in FID
    // numbering. That is min(fid) == 1 and max(fid) == m_nTotalFeatureCount
    if (m_nIsCompatOfOptimizedGetNextArrowArray < 0)
    {
        m_nIsCompatOfOptimizedGetNextArrowArray = FALSE;
        const auto nTotalFeatureCount = GetTotalFeatureCount();
        if (nTotalFeatureCount < 0)
            return GetNextArrowArrayAsynchronous(stream, out_array);
        {
            char *pszSQL = sqlite3_mprintf("SELECT MAX(\"%w\") FROM \"%w\"",
                                           m_pszFidColumn, m_pszTableName);
            OGRErr err;
            const auto nMaxFID = SQLGetInteger64(m_poDS->GetDB(), pszSQL, &err);
            sqlite3_free(pszSQL);
            if (nMaxFID != nTotalFeatureCount)
                return GetNextArrowArrayAsynchronous(stream, out_array);
        }
        {
            char *pszSQL = sqlite3_mprintf("SELECT MIN(\"%w\") FROM \"%w\"",
                                           m_pszFidColumn, m_pszTableName);
            OGRErr err;
            const auto nMinFID = SQLGetInteger64(m_poDS->GetDB(), pszSQL, &err);
            sqlite3_free(pszSQL);
            if (nMinFID != 1)
                return GetNextArrowArrayAsynchronous(stream, out_array);
        }
        m_nIsCompatOfOptimizedGetNextArrowArray = TRUE;
    }

    m_bGetNextArrowArrayCalledSinceResetReading = true;

    // CPLDebug("GPKG", "m_iNextShapeId = " CPL_FRMT_GIB, m_iNextShapeId);

    const int nMaxBatchSize = OGRArrowArrayHelper::GetMaxFeaturesInBatch(
        m_aosArrowArrayStreamOptions);

    // Fetch the answer from a potentially queued asynchronous task
    if (!m_oQueueArrowArrayPrefetchTasks.empty())
    {
        const size_t nTasks = m_oQueueArrowArrayPrefetchTasks.size();
        auto task = std::move(m_oQueueArrowArrayPrefetchTasks.front());
        m_oQueueArrowArrayPrefetchTasks.pop();

        // Wait for thread to be ready
        {
            std::unique_lock<std::mutex> oLock(task->m_oMutex);
            while (!task->m_bArrayReady)
            {
                task->m_oCV.wait(oLock);
            }
            task->m_bArrayReady = false;
        }
        if (!task->m_osErrorMsg.empty())
            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                     task->m_osErrorMsg.c_str());

        const auto stopThread = [&task]()
        {
            {
                std::lock_guard<std::mutex> oLock(task->m_oMutex);
                task->m_bStop = true;
                task->m_oCV.notify_one();
            }
            if (task->m_oThread.joinable())
                task->m_oThread.join();
        };

        if (task->m_iStartShapeId != m_iNextShapeId)
        {
            // Should not normally happen, unless the user messes with
            // GetNextFeature()
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Worker thread task has not expected m_iStartShapeId "
                     "value. Got " CPL_FRMT_GIB ", expected " CPL_FRMT_GIB,
                     task->m_iStartShapeId, m_iNextShapeId);
            if (task->m_psArrowArray->release)
                task->m_psArrowArray->release(task->m_psArrowArray.get());

            stopThread();
        }
        else if (task->m_psArrowArray->release)
        {
            m_iNextShapeId += task->m_psArrowArray->length;

            // Transfer the task ArrowArray to the client array
            memcpy(out_array, task->m_psArrowArray.get(),
                   sizeof(struct ArrowArray));
            memset(task->m_psArrowArray.get(), 0, sizeof(struct ArrowArray));

            if (task->m_bMemoryLimitReached)
            {
                m_nIsCompatOfOptimizedGetNextArrowArray = false;
                stopThread();
                CancelAsyncNextArrowArray();
                return 0;
            }
            // Are the records still available for reading beyond the current
            // queued tasks ? If so, recycle this task to read them
            else if (task->m_iStartShapeId +
                         static_cast<GIntBig>(nTasks) * nMaxBatchSize <=
                     m_nTotalFeatureCount)
            {
                task->m_iStartShapeId +=
                    static_cast<GIntBig>(nTasks) * nMaxBatchSize;
                task->m_poLayer->m_iNextShapeId = task->m_iStartShapeId;
                try
                {
                    // Wake-up thread with new task
                    {
                        std::lock_guard<std::mutex> oLock(task->m_oMutex);
                        task->m_bFetchRows = true;
                        task->m_oCV.notify_one();
                    }
                    m_oQueueArrowArrayPrefetchTasks.push(std::move(task));
                    return 0;
                }
                catch (const std::exception &e)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot start worker thread: %s", e.what());
                }
            }
            else
            {
                stopThread();
                return 0;
            }
        }

        stopThread();
    }

    const auto GetThreadsAvailable = []()
    {
        const char *pszMaxThreads =
            CPLGetConfigOption("OGR_GPKG_NUM_THREADS", nullptr);
        if (pszMaxThreads == nullptr)
            return std::min(4, CPLGetNumCPUs());
        else if (EQUAL(pszMaxThreads, "ALL_CPUS"))
            return CPLGetNumCPUs();
        else
            return atoi(pszMaxThreads);
    };

    // Start asynchronous tasks to prefetch the next ArrowArray
    if (m_poDS->GetAccess() == GA_ReadOnly &&
        m_oQueueArrowArrayPrefetchTasks.empty() &&
        m_iNextShapeId + 2 * static_cast<GIntBig>(nMaxBatchSize) <=
            m_nTotalFeatureCount &&
        sqlite3_threadsafe() != 0 && GetThreadsAvailable() >= 2 &&
        CPLGetUsablePhysicalRAM() > 1024 * 1024 * 1024)
    {
        const int nMaxTasks = static_cast<int>(std::min<GIntBig>(
            DIV_ROUND_UP(m_nTotalFeatureCount - nMaxBatchSize - m_iNextShapeId,
                         nMaxBatchSize),
            GetThreadsAvailable()));
        CPLDebug("GPKG", "Using %d threads", nMaxTasks);
        GDALOpenInfo oOpenInfo(m_poDS->GetDescription(), GA_ReadOnly);
        oOpenInfo.papszOpenOptions = m_poDS->GetOpenOptions();
        oOpenInfo.nOpenFlags = GDAL_OF_VECTOR;
        for (int iTask = 0; iTask < nMaxTasks; ++iTask)
        {
            auto task = std::make_unique<ArrowArrayPrefetchTask>();
            task->m_iStartShapeId =
                m_iNextShapeId +
                static_cast<GIntBig>(iTask + 1) * nMaxBatchSize;
            task->m_poDS = std::make_unique<GDALGeoPackageDataset>();
            if (!task->m_poDS->Open(&oOpenInfo, m_poDS->m_osFilenameInZip))
            {
                break;
            }
            auto poOtherLayer = dynamic_cast<OGRGeoPackageTableLayer *>(
                task->m_poDS->GetLayerByName(GetName()));
            if (poOtherLayer == nullptr ||
                poOtherLayer->GetLayerDefn()->GetFieldCount() !=
                    m_poFeatureDefn->GetFieldCount())
            {
                break;
            }

            // Install query logging callback
            if (m_poDS->pfnQueryLoggerFunc)
            {
                task->m_poDS->SetQueryLoggerFunc(m_poDS->pfnQueryLoggerFunc,
                                                 m_poDS->poQueryLoggerArg);
            }

            task->m_poLayer = poOtherLayer;
            task->m_psArrowArray = std::make_unique<struct ArrowArray>();
            memset(task->m_psArrowArray.get(), 0, sizeof(struct ArrowArray));

            poOtherLayer->m_nTotalFeatureCount = m_nTotalFeatureCount;
            poOtherLayer->m_aosArrowArrayStreamOptions =
                m_aosArrowArrayStreamOptions;
            auto poOtherFDefn = poOtherLayer->GetLayerDefn();
            for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
            {
                poOtherFDefn->GetGeomFieldDefn(i)->SetIgnored(
                    m_poFeatureDefn->GetGeomFieldDefn(i)->IsIgnored());
            }
            for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
            {
                poOtherFDefn->GetFieldDefn(i)->SetIgnored(
                    m_poFeatureDefn->GetFieldDefn(i)->IsIgnored());
            }

            poOtherLayer->m_iNextShapeId = task->m_iStartShapeId;

            auto taskPtr = task.get();
            auto taskRunner = [taskPtr]()
            {
                std::unique_lock<std::mutex> oLock(taskPtr->m_oMutex);
                do
                {
                    taskPtr->m_bFetchRows = false;
                    taskPtr->m_poLayer->GetNextArrowArrayInternal(
                        taskPtr->m_psArrowArray.get(), taskPtr->m_osErrorMsg,
                        taskPtr->m_bMemoryLimitReached);
                    taskPtr->m_bArrayReady = true;
                    taskPtr->m_oCV.notify_one();
                    if (taskPtr->m_bMemoryLimitReached)
                        break;
                    // cppcheck-suppress knownConditionTrueFalse
                    while (!taskPtr->m_bStop && !taskPtr->m_bFetchRows)
                    {
                        taskPtr->m_oCV.wait(oLock);
                    }
                } while (!taskPtr->m_bStop);
            };

            task->m_bFetchRows = true;
            try
            {
                task->m_oThread = std::thread(taskRunner);
            }
            catch (const std::exception &e)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot start worker thread: %s", e.what());
                break;
            }
            m_oQueueArrowArrayPrefetchTasks.push(std::move(task));
        }
    }

    std::string osErrorMsg;
    bool bMemoryLimitReached = false;
    int ret =
        GetNextArrowArrayInternal(out_array, osErrorMsg, bMemoryLimitReached);
    if (!osErrorMsg.empty())
        CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrorMsg.c_str());
    if (bMemoryLimitReached)
    {
        CancelAsyncNextArrowArray();
        m_nIsCompatOfOptimizedGetNextArrowArray = false;
    }
    return ret;
}

/************************************************************************/
/*                      GetNextArrowArrayInternal()                     */
/************************************************************************/

int OGRGeoPackageTableLayer::GetNextArrowArrayInternal(
    struct ArrowArray *out_array, std::string &osErrorMsg,
    bool &bMemoryLimitReached)
{
    bMemoryLimitReached = false;
    memset(out_array, 0, sizeof(*out_array));

    if (m_iNextShapeId >= m_nTotalFeatureCount)
    {
        return 0;
    }

    auto psHelper = std::make_unique<OGRArrowArrayHelper>(
        m_poDS, m_poFeatureDefn, m_aosArrowArrayStreamOptions, out_array);
    if (out_array->release == nullptr)
    {
        return ENOMEM;
    }

    OGRGPKGTableLayerFillArrowArray sFillArrowArray;
    sFillArrowArray.psHelper = std::move(psHelper);
    sFillArrowArray.nCountRows = 0;
    sFillArrowArray.bMemoryLimitReached = false;
    sFillArrowArray.bErrorOccurred = false;
    sFillArrowArray.poFeatureDefn = m_poFeatureDefn;
    sFillArrowArray.poLayer = this;
    sFillArrowArray.hDB = m_poDS->GetDB();
    memset(&sFillArrowArray.brokenDown, 0, sizeof(sFillArrowArray.brokenDown));

    sqlite3_create_function(
        m_poDS->GetDB(), "OGR_GPKG_FillArrowArray_INTERNAL", -1,
        SQLITE_UTF8 | SQLITE_DETERMINISTIC, &sFillArrowArray, nullptr,
        OGR_GPKG_FillArrowArray_Step, OGR_GPKG_FillArrowArray_Finalize);

    std::string osSQL;
    osSQL = "SELECT OGR_GPKG_FillArrowArray_INTERNAL(-1,";
    int nCountArgs = 1;

    osSQL += '"';
    osSQL += SQLEscapeName(m_pszFidColumn);
    osSQL += '"';
    ++nCountArgs;

    if (!sFillArrowArray.psHelper->m_mapOGRGeomFieldToArrowField.empty() &&
        sFillArrowArray.psHelper->m_mapOGRGeomFieldToArrowField[0] >= 0)
    {
        osSQL += ',';
        osSQL += '"';
        osSQL += SQLEscapeName(GetGeometryColumn());
        osSQL += '"';
        ++nCountArgs;
    }
    const int SQLITE_MAX_FUNCTION_ARG =
        sqlite3_limit(m_poDS->GetDB(), SQLITE_LIMIT_FUNCTION_ARG, -1);
    for (int iField = 0; iField < sFillArrowArray.psHelper->m_nFieldCount;
         iField++)
    {
        const int iArrowField =
            sFillArrowArray.psHelper->m_mapOGRFieldToArrowField[iField];
        if (iArrowField >= 0)
        {
            if (nCountArgs == SQLITE_MAX_FUNCTION_ARG)
            {
                // We cannot pass more than SQLITE_MAX_FUNCTION_ARG args
                // to a function... So we have to split in several calls...
                osSQL += "), OGR_GPKG_FillArrowArray_INTERNAL(";
                osSQL += CPLSPrintf("%d", iField);
                nCountArgs = 1;
            }
            const OGRFieldDefn *poFieldDefn =
                m_poFeatureDefn->GetFieldDefnUnsafe(iField);
            osSQL += ',';
            osSQL += '"';
            osSQL += SQLEscapeName(poFieldDefn->GetNameRef());
            osSQL += '"';
            ++nCountArgs;
        }
    }
    osSQL += ") FROM \"";
    osSQL += SQLEscapeName(m_pszTableName);
    osSQL += "\" WHERE \"";
    osSQL += SQLEscapeName(m_pszFidColumn);
    osSQL += "\" BETWEEN ";
    osSQL += std::to_string(m_iNextShapeId + 1);
    osSQL += " AND ";
    osSQL += std::to_string(m_iNextShapeId +
                            sFillArrowArray.psHelper->m_nMaxBatchSize);

    // CPLDebug("GPKG", "%s", osSQL.c_str());

    char *pszErrMsg = nullptr;
    if (sqlite3_exec(m_poDS->GetDB(), osSQL.c_str(), nullptr, nullptr,
                     &pszErrMsg) != SQLITE_OK)
    {
        if (!sFillArrowArray.bErrorOccurred &&
            !sFillArrowArray.bMemoryLimitReached)
        {
            osErrorMsg = pszErrMsg ? pszErrMsg : "unknown error";
        }
    }
    sqlite3_free(pszErrMsg);

    bMemoryLimitReached = sFillArrowArray.bMemoryLimitReached;

    // Delete function
    sqlite3_create_function(m_poDS->GetDB(), "OGR_GPKG_FillArrowArray_INTERNAL",
                            -1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
                            nullptr, nullptr, nullptr);

    if (sFillArrowArray.bErrorOccurred)
    {
        sFillArrowArray.psHelper->ClearArray();
        return ENOMEM;
    }

    sFillArrowArray.psHelper->Shrink(sFillArrowArray.nCountRows);
    if (sFillArrowArray.nCountRows == 0)
    {
        sFillArrowArray.psHelper->ClearArray();
    }

    m_iNextShapeId += sFillArrowArray.nCountRows;

    return 0;
}

/************************************************************************/
/*               OGR_GPKG_GeometryExtent3DAggregate()                   */
/************************************************************************/

namespace
{
struct GeometryExtent3DAggregateContext
{
    sqlite3 *m_hDB = nullptr;
    OGREnvelope3D m_oExtent3D;

    explicit GeometryExtent3DAggregateContext(sqlite3 *hDB)
        : m_hDB(hDB), m_oExtent3D()
    {
    }

    GeometryExtent3DAggregateContext(const GeometryExtent3DAggregateContext &) =
        delete;
    GeometryExtent3DAggregateContext &
    operator=(const GeometryExtent3DAggregateContext &) = delete;
};

}  // namespace

static void OGR_GPKG_GeometryExtent3DAggregate_Step(sqlite3_context *pContext,
                                                    int /*argc*/,
                                                    sqlite3_value **argv)
{
    const GByte *pabyBLOB =
        reinterpret_cast<const GByte *>(sqlite3_value_blob(argv[0]));

    auto poContext = static_cast<GeometryExtent3DAggregateContext *>(
        sqlite3_user_data(pContext));

    if (pabyBLOB != nullptr)
    {
        GPkgHeader sHeader;
        if (OGRGeoPackageGetHeader(pContext, 0, argv, &sHeader, true, true))
        {
            OGREnvelope3D extent3D;
            extent3D.MinX = sHeader.MinX;
            extent3D.MaxX = sHeader.MaxX;
            extent3D.MinY = sHeader.MinY;
            extent3D.MaxY = sHeader.MaxY;
            extent3D.MinZ = sHeader.MinZ;
            extent3D.MaxZ = sHeader.MaxZ;
            poContext->m_oExtent3D.Merge(extent3D);
        }
        else if (!sHeader.bEmpty)
        {
            // Try also spatialite geometry blobs
            const int nBLOBLen = sqlite3_value_bytes(argv[0]);
            OGRGeometry *poGeom = nullptr;
            if (OGRSQLiteImportSpatiaLiteGeometry(pabyBLOB, nBLOBLen,
                                                  &poGeom) == OGRERR_NONE &&
                poGeom && !poGeom->IsEmpty())
            {
                OGREnvelope3D extent3D;
                poGeom->getEnvelope(&extent3D);
                poContext->m_oExtent3D.Merge(extent3D);
            }
            delete poGeom;
        }
    }
}

static void OGR_GPKG_GeometryExtent3DAggregate_Finalize(sqlite3_context *)
{
}

/************************************************************************/
/*                      GetExtent3D                                     */
/************************************************************************/
OGRErr OGRGeoPackageTableLayer::GetExtent3D(int iGeomField,
                                            OGREnvelope3D *psExtent3D,
                                            int bForce)
{

    OGRFeatureDefn *poDefn = GetLayerDefn();

    /* -------------------------------------------------------------------- */
    /*      Deferred actions, reset state.                                   */
    /* -------------------------------------------------------------------- */
    RunDeferredCreationIfNecessary();
    if (!RunDeferredSpatialIndexUpdate())
    {
        return OGRERR_FAILURE;
    }

    const int nGeomFieldCount = poDefn->GetGeomFieldCount();
    if (iGeomField < 0 || iGeomField >= nGeomFieldCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for iGeomField");
        return OGRERR_FAILURE;
    }

    if (m_nZFlag == 0 && m_soFilter.empty())
    {
        // If the layer doesn't contain any 3D geometry and no filter is set,
        // we can fallback to the fast 2D GetExtent()
        const OGRErr retVal{GetExtent(iGeomField, psExtent3D, bForce)};
        psExtent3D->MinZ = std::numeric_limits<double>::infinity();
        psExtent3D->MaxZ = -std::numeric_limits<double>::infinity();
        return retVal;
    }
    else
    {
        *psExtent3D = OGREnvelope3D();
    }

    // For internal use only

    GeometryExtent3DAggregateContext sContext(m_poDS->hDB);

    CPLString osFuncName;
    osFuncName.Printf("OGR_GPKG_GeometryExtent3DAggregate_INTERNAL_%p",
                      &sContext);

    sqlite3_create_function(m_poDS->hDB, osFuncName.c_str(), 1, SQLITE_UTF8,
                            &sContext, nullptr,
                            OGR_GPKG_GeometryExtent3DAggregate_Step,
                            OGR_GPKG_GeometryExtent3DAggregate_Finalize);

    char *pszSQL = sqlite3_mprintf(
        "SELECT %s(\"%w\") FROM \"%w\"%s", osFuncName.c_str(),
        poDefn->GetGeomFieldDefn(iGeomField)->GetNameRef(), m_pszTableName,
        m_soFilter.empty() ? "" : (" WHERE " + m_soFilter).c_str());
    char *pszErrMsg = nullptr;
    const int rc =
        sqlite3_exec(m_poDS->hDB, pszSQL, nullptr, nullptr, &(pszErrMsg));

    // Delete function
    sqlite3_create_function(m_poDS->GetDB(), osFuncName.c_str(), 1, SQLITE_UTF8,
                            nullptr, nullptr, nullptr, nullptr);

    if (rc != SQLITE_OK)
    {
        if (rc != SQLITE_INTERRUPT)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "sqlite3_exec(%s) failed: %s",
                     pszSQL, pszErrMsg);
        }
        sqlite3_free(pszErrMsg);
        sqlite3_free(pszSQL);
        return OGRERR_FAILURE;
    }
    sqlite3_free(pszErrMsg);
    sqlite3_free(pszSQL);

    *psExtent3D = sContext.m_oExtent3D;

    return OGRERR_NONE;
}
