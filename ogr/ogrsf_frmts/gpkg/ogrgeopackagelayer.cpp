/******************************************************************************
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements OGRGeoPackageLayer class
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
#include "ogr_p.h"
#include "ogr_recordbatch.h"
#include "ograrrowarrayhelper.h"

/************************************************************************/
/*                      OGRGeoPackageLayer()                            */
/************************************************************************/

OGRGeoPackageLayer::OGRGeoPackageLayer(GDALGeoPackageDataset *poDS)
    : m_poDS(poDS)
{
}

/************************************************************************/
/*                      ~OGRGeoPackageLayer()                           */
/************************************************************************/

OGRGeoPackageLayer::~OGRGeoPackageLayer()
{

    CPLFree(m_pszFidColumn);

    if (m_poQueryStatement)
        sqlite3_finalize(m_poQueryStatement);

    if (m_poFeatureDefn)
        m_poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeoPackageLayer::ResetReading()

{
    ClearStatement();
    m_iNextShapeId = 0;
    m_bEOF = false;
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRGeoPackageLayer::ClearStatement()

{
    if (m_poQueryStatement != nullptr)
    {
        CPLDebug("GPKG", "finalize %p", m_poQueryStatement);
        sqlite3_finalize(m_poQueryStatement);
        m_poQueryStatement = nullptr;
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoPackageLayer::GetNextFeature()

{
    if (m_bEOF)
        return nullptr;

    if (m_poQueryStatement == nullptr)
    {
        ResetStatement();
        if (m_poQueryStatement == nullptr)
            return nullptr;
    }

    for (; true;)
    {
        /* --------------------------------------------------------------------
         */
        /*      Fetch a record (unless otherwise instructed) */
        /* --------------------------------------------------------------------
         */
        if (m_bDoStep)
        {
            int rc = sqlite3_step(m_poQueryStatement);
            if (rc != SQLITE_ROW)
            {
                if (rc != SQLITE_DONE)
                {
                    sqlite3_reset(m_poQueryStatement);
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "In GetNextRawFeature(): sqlite3_step() : %s",
                             sqlite3_errmsg(m_poDS->GetDB()));
                }

                ClearStatement();
                m_bEOF = true;

                return nullptr;
            }
        }
        else
        {
            m_bDoStep = true;
        }

        OGRFeature *poFeature = TranslateFeature(m_poQueryStatement);

        if ((m_poFilterGeom == nullptr ||
             FilterGeometry(poFeature->GetGeomFieldRef(m_iGeomFieldFilter))) &&
            (m_poAttrQuery == nullptr || m_poAttrQuery->Evaluate(poFeature)))
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                         ParseDateField()                             */
/************************************************************************/

bool OGRGeoPackageLayer::ParseDateField(sqlite3_stmt *hStmt, int iRawField,
                                        int nSqlite3ColType, OGRField *psField,
                                        const OGRFieldDefn *poFieldDefn,
                                        GIntBig nFID)
{
    if (nSqlite3ColType == SQLITE_TEXT)
    {
        const char *pszTxt = reinterpret_cast<const char *>(
            sqlite3_column_text(hStmt, iRawField));
        return ParseDateField(pszTxt, psField, poFieldDefn, nFID);
    }
    else
    {
        constexpr int line = __LINE__;
        if (!m_poDS->m_oSetGPKGLayerWarnings[line])
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unexpected data type for record " CPL_FRMT_GIB
                     " in column %s",
                     nFID, poFieldDefn->GetNameRef());
            m_poDS->m_oSetGPKGLayerWarnings[line] = true;
        }
        return false;
    }
}

bool OGRGeoPackageLayer::ParseDateField(const char *pszTxt, OGRField *psField,
                                        const OGRFieldDefn *poFieldDefn,
                                        GIntBig nFID)
{
    if (pszTxt == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 sqlite3_errmsg(m_poDS->GetDB()));
        return false;
    }
    const size_t nLen = strlen(pszTxt);
    // nominal format: "YYYY-MM-DD" (10 characters)
    const bool bNominalFormat =
        (nLen == 10 && pszTxt[4] == '-' && pszTxt[7] == '-' &&
         static_cast<unsigned>(pszTxt[0] - '0') <= 9 &&
         static_cast<unsigned>(pszTxt[1] - '0') <= 9 &&
         static_cast<unsigned>(pszTxt[2] - '0') <= 9 &&
         static_cast<unsigned>(pszTxt[3] - '0') <= 9 &&
         static_cast<unsigned>(pszTxt[5] - '0') <= 9 &&
         static_cast<unsigned>(pszTxt[6] - '0') <= 9 &&
         static_cast<unsigned>(pszTxt[8] - '0') <= 9 &&
         static_cast<unsigned>(pszTxt[9] - '0') <= 9);

    bool bError = false;
    if (bNominalFormat)
    {
        psField->Date.Year = static_cast<GUInt16>(
            ((((pszTxt[0] - '0') * 10 + (pszTxt[1] - '0')) * 10) +
             (pszTxt[2] - '0')) *
                10 +
            (pszTxt[3] - '0'));
        psField->Date.Month =
            static_cast<GByte>((pszTxt[5] - '0') * 10 + (pszTxt[6] - '0'));
        psField->Date.Day =
            static_cast<GByte>((pszTxt[8] - '0') * 10 + (pszTxt[9] - '0'));
        psField->Date.Hour = 0;
        psField->Date.Minute = 0;
        psField->Date.Second = 0.0f;
        psField->Date.TZFlag = 0;
        if (psField->Date.Month == 0 || psField->Date.Month > 12 ||
            psField->Date.Day == 0 || psField->Date.Day > 31)
        {
            bError = true;
        }
    }
    else if (OGRParseDate(pszTxt, psField, OGRPARSEDATE_OPTION_LAX))
    {
        constexpr int line = __LINE__;
        if (!m_poDS->m_oSetGPKGLayerWarnings[line])
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Non-conformant content for record " CPL_FRMT_GIB
                     " in column %s, %s, "
                     "successfully parsed",
                     nFID, poFieldDefn->GetNameRef(), pszTxt);
            m_poDS->m_oSetGPKGLayerWarnings[line] = true;
        }
    }
    else
    {
        bError = true;
    }

    if (bError)
    {
        OGR_RawField_SetUnset(psField);
        constexpr int line = __LINE__;
        if (!m_poDS->m_oSetGPKGLayerWarnings[line])
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Invalid content for record " CPL_FRMT_GIB
                     " in column %s: %s",
                     nFID, poFieldDefn->GetNameRef(), pszTxt);
            m_poDS->m_oSetGPKGLayerWarnings[line] = true;
        }
        return false;
    }

    return true;
}

/************************************************************************/
/*                        ParseDateTimeField()                          */
/************************************************************************/

bool OGRGeoPackageLayer::ParseDateTimeField(sqlite3_stmt *hStmt, int iRawField,
                                            int nSqlite3ColType,
                                            OGRField *psField,
                                            const OGRFieldDefn *poFieldDefn,
                                            GIntBig nFID)
{
    if (nSqlite3ColType == SQLITE_TEXT)
    {
        const char *pszTxt = reinterpret_cast<const char *>(
            sqlite3_column_text(hStmt, iRawField));
        return ParseDateTimeField(pszTxt, psField, poFieldDefn, nFID);
    }
    else
    {
        constexpr int line = __LINE__;
        if (!m_poDS->m_oSetGPKGLayerWarnings[line])
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unexpected data type for record " CPL_FRMT_GIB
                     " in column %s",
                     nFID, poFieldDefn->GetNameRef());
            m_poDS->m_oSetGPKGLayerWarnings[line] = true;
        }
        return false;
    }
}

bool OGRGeoPackageLayer::ParseDateTimeField(const char *pszTxt,
                                            OGRField *psField,
                                            const OGRFieldDefn *poFieldDefn,
                                            GIntBig nFID)
{
    if (pszTxt == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 sqlite3_errmsg(m_poDS->GetDB()));
        return false;
    }

    const size_t nLen = strlen(pszTxt);

    if (OGRParseDateTimeYYYYMMDDTHHMMSSsssZ(pszTxt, nLen, psField) ||
        OGRParseDateTimeYYYYMMDDTHHMMSSZ(pszTxt, nLen, psField) ||
        OGRParseDateTimeYYYYMMDDTHHMMZ(pszTxt, nLen, psField))
    {
        // nominal format is YYYYMMDDTHHMMSSsssZ before GeoPackage 1.4
        // GeoPackage 1.4 also accepts omission of seconds and milliseconds
    }
    else if (OGRParseDate(pszTxt, psField, OGRPARSEDATE_OPTION_LAX))
    {
        constexpr int line = __LINE__;
        if (!m_poDS->m_oSetGPKGLayerWarnings[line])
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Non-conformant content for record " CPL_FRMT_GIB
                     " in column %s, %s, "
                     "successfully parsed",
                     nFID, poFieldDefn->GetNameRef(), pszTxt);
            m_poDS->m_oSetGPKGLayerWarnings[line] = true;
        }
    }
    else
    {
        OGR_RawField_SetUnset(psField);
        constexpr int line = __LINE__;
        if (!m_poDS->m_oSetGPKGLayerWarnings[line])
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Invalid content for record " CPL_FRMT_GIB
                     " in column %s: %s",
                     nFID, poFieldDefn->GetNameRef(), pszTxt);
            m_poDS->m_oSetGPKGLayerWarnings[line] = true;
        }
        return false;
    }

    return true;
}

/************************************************************************/
/*                         TranslateFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoPackageLayer::TranslateFeature(sqlite3_stmt *hStmt)

{
    /* -------------------------------------------------------------------- */
    /*      Create a feature from the current result.                       */
    /* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature(m_poFeatureDefn);

    /* -------------------------------------------------------------------- */
    /*      Set FID if we have a column to set it from.                     */
    /* -------------------------------------------------------------------- */
    if (m_iFIDCol >= 0)
    {
        poFeature->SetFID(sqlite3_column_int64(hStmt, m_iFIDCol));
        if (m_pszFidColumn == nullptr && poFeature->GetFID() == 0)
        {
            // Miht be the case for views with joins.
            poFeature->SetFID(m_iNextShapeId);
        }
    }
    else
        poFeature->SetFID(m_iNextShapeId);

    m_iNextShapeId++;

    m_nFeaturesRead++;

    /* -------------------------------------------------------------------- */
    /*      Process Geometry if we have a column.                           */
    /* -------------------------------------------------------------------- */
    if (m_iGeomCol >= 0)
    {
        OGRGeomFieldDefn *poGeomFieldDefn =
            m_poFeatureDefn->GetGeomFieldDefn(0);
        if (sqlite3_column_type(hStmt, m_iGeomCol) != SQLITE_NULL &&
            !poGeomFieldDefn->IsIgnored())
        {
            const OGRSpatialReference *poSrs = poGeomFieldDefn->GetSpatialRef();
            int iGpkgSize = sqlite3_column_bytes(hStmt, m_iGeomCol);
            // coverity[tainted_data_return]
            const GByte *pabyGpkg = static_cast<const GByte *>(
                sqlite3_column_blob(hStmt, m_iGeomCol));
            OGRGeometry *poGeom =
                GPkgGeometryToOGR(pabyGpkg, iGpkgSize, nullptr);
            if (poGeom == nullptr)
            {
                // Try also spatialite geometry blobs
                if (OGRSQLiteImportSpatiaLiteGeometry(pabyGpkg, iGpkgSize,
                                                      &poGeom) != OGRERR_NONE)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unable to read geometry");
                }
            }
            if (poGeom)
            {
                if (m_bUndoDiscardCoordLSBOnReading)
                {
                    poGeom->roundCoordinates(
                        poGeomFieldDefn->GetCoordinatePrecision());
                }
                poGeom->assignSpatialReference(poSrs);
            }

            poFeature->SetGeometryDirectly(poGeom);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      set the fields.                                                 */
    /* -------------------------------------------------------------------- */
    const int nFieldCount = m_poFeatureDefn->GetFieldCount();
    for (int iField = 0; iField < nFieldCount; iField++)
    {
        const OGRFieldDefn *poFieldDefn =
            m_poFeatureDefn->GetFieldDefnUnsafe(iField);
        if (poFieldDefn->IsIgnored())
            continue;

        const int iRawField = m_anFieldOrdinals[iField];

        const int nSqlite3ColType = sqlite3_column_type(hStmt, iRawField);
        if (nSqlite3ColType == SQLITE_NULL)
        {
            poFeature->SetFieldNull(iField);
            continue;
        }

        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
                poFeature->SetFieldSameTypeUnsafe(
                    iField, sqlite3_column_int(hStmt, iRawField));
                break;

            case OFTInteger64:
                poFeature->SetFieldSameTypeUnsafe(
                    iField, sqlite3_column_int64(hStmt, iRawField));
                break;

            case OFTReal:
                poFeature->SetFieldSameTypeUnsafe(
                    iField, sqlite3_column_double(hStmt, iRawField));
                break;

            case OFTBinary:
            {
                const int nBytes = sqlite3_column_bytes(hStmt, iRawField);
                // coverity[tainted_data_return]
                const void *pabyData = sqlite3_column_blob(hStmt, iRawField);
                if (pabyData != nullptr || nBytes == 0)
                {
                    poFeature->SetField(iField, nBytes, pabyData);
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "%s",
                             sqlite3_errmsg(m_poDS->GetDB()));
                }
                break;
            }

            case OFTDate:
            {
                auto psField = poFeature->GetRawFieldRef(iField);
                CPL_IGNORE_RET_VAL(
                    ParseDateField(hStmt, iRawField, nSqlite3ColType, psField,
                                   poFieldDefn, poFeature->GetFID()));
                break;
            }

            case OFTDateTime:
            {
                auto psField = poFeature->GetRawFieldRef(iField);
                CPL_IGNORE_RET_VAL(ParseDateTimeField(
                    hStmt, iRawField, nSqlite3ColType, psField, poFieldDefn,
                    poFeature->GetFID()));
                break;
            }

            case OFTString:
            {
                const char *pszTxt = reinterpret_cast<const char *>(
                    sqlite3_column_text(hStmt, iRawField));
                if (pszTxt)
                {
                    char *pszTxtDup = VSI_STRDUP_VERBOSE(pszTxt);
                    if (pszTxtDup)
                    {
                        poFeature->SetFieldSameTypeUnsafe(iField, pszTxtDup);
                    }
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "%s",
                             sqlite3_errmsg(m_poDS->GetDB()));
                }
                break;
            }

            default:
                break;
        }
    }

    return poFeature;
}

/************************************************************************/
/*                          GetArrowStream()                            */
/************************************************************************/

bool OGRGeoPackageLayer::GetArrowStream(struct ArrowArrayStream *out_stream,
                                        CSLConstList papszOptions)
{
    CPLStringList aosOptions;
    aosOptions.Assign(CSLDuplicate(papszOptions), true);
    // GeoPackage are assumed to be in UTC. Even if another timezone is used,
    // we'll do the conversion to UTC
    if (aosOptions.FetchNameValue("TIMEZONE") == nullptr)
    {
        aosOptions.SetNameValue("TIMEZONE", "UTC");
    }
    return OGRLayer::GetArrowStream(out_stream, aosOptions.List());
}

/************************************************************************/
/*                      GetNextArrowArray()                             */
/************************************************************************/

int OGRGeoPackageLayer::GetNextArrowArray(struct ArrowArrayStream *stream,
                                          struct ArrowArray *out_array)
{
    if (CPLTestBool(CPLGetConfigOption("OGR_GPKG_STREAM_BASE_IMPL", "NO")))
    {
        return OGRLayer::GetNextArrowArray(stream, out_array);
    }

    int errorErrno = EIO;
    memset(out_array, 0, sizeof(*out_array));

    if (m_bEOF)
        return 0;

    if (m_poQueryStatement == nullptr)
    {
        GetLayerDefn();
        ResetStatement();
        if (m_poQueryStatement == nullptr)
            return 0;
    }
    sqlite3_stmt *hStmt = m_poQueryStatement;

    OGRArrowArrayHelper sHelper(m_poDS, m_poFeatureDefn,
                                m_aosArrowArrayStreamOptions, out_array);
    if (out_array->release == nullptr)
    {
        return ENOMEM;
    }

    struct tm brokenDown;
    memset(&brokenDown, 0, sizeof(brokenDown));

    const uint32_t nMemLimit = OGRArrowArrayHelper::GetMemLimit();
    int iFeat = 0;
    while (iFeat < sHelper.m_nMaxBatchSize)
    {
        /* --------------------------------------------------------------------
         */
        /*      Fetch a record (unless otherwise instructed) */
        /* --------------------------------------------------------------------
         */
        if (m_bDoStep)
        {
            int rc = sqlite3_step(hStmt);
            if (rc != SQLITE_ROW)
            {
                if (rc != SQLITE_DONE)
                {
                    sqlite3_reset(hStmt);
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "In GetNextArrowArray(): sqlite3_step() : %s",
                             sqlite3_errmsg(m_poDS->GetDB()));
                }

                ClearStatement();
                m_bEOF = true;

                break;
            }
        }
        else
        {
            m_bDoStep = true;
        }

        m_iNextShapeId++;

        m_nFeaturesRead++;

        GIntBig nFID;
        if (m_iFIDCol >= 0)
        {
            nFID = sqlite3_column_int64(hStmt, m_iFIDCol);
            if (m_pszFidColumn == nullptr && nFID == 0)
            {
                // Might be the case for views with joins.
                nFID = m_iNextShapeId;
            }
        }
        else
            nFID = m_iNextShapeId;

        if (sHelper.m_panFIDValues)
        {
            sHelper.m_panFIDValues[iFeat] = nFID;
        }

        /* --------------------------------------------------------------------
         */
        /*      Process Geometry if we have a column. */
        /* --------------------------------------------------------------------
         */
        if (m_iGeomCol >= 0 && sHelper.m_mapOGRGeomFieldToArrowField[0] >= 0)
        {
            const int iArrowField = sHelper.m_mapOGRGeomFieldToArrowField[0];
            auto psArray = out_array->children[iArrowField];

            size_t nWKBSize = 0;
            if (sqlite3_column_type(hStmt, m_iGeomCol) != SQLITE_NULL)
            {
                std::unique_ptr<OGRGeometry> poGeom;
                const GByte *pabyWkb = nullptr;
                const int iGpkgSize = sqlite3_column_bytes(hStmt, m_iGeomCol);
                // coverity[tainted_data_return]
                const GByte *pabyGpkg = static_cast<const GByte *>(
                    sqlite3_column_blob(hStmt, m_iGeomCol));
                if (m_poFilterGeom == nullptr && iGpkgSize >= 8 && pabyGpkg &&
                    pabyGpkg[0] == 'G' && pabyGpkg[1] == 'P' &&
                    !m_bUndoDiscardCoordLSBOnReading)
                {
                    GPkgHeader oHeader;

                    /* Read header */
                    OGRErr err =
                        GPkgHeaderFromWKB(pabyGpkg, iGpkgSize, &oHeader);
                    if (err == OGRERR_NONE)
                    {
                        /* WKB pointer */
                        pabyWkb = pabyGpkg + oHeader.nHeaderLen;
                        nWKBSize = iGpkgSize - oHeader.nHeaderLen;
                    }
                }
                else
                {
                    poGeom.reset(
                        GPkgGeometryToOGR(pabyGpkg, iGpkgSize, nullptr));
                    if (poGeom == nullptr)
                    {
                        // Try also spatialite geometry blobs
                        OGRGeometry *poGeomPtr = nullptr;
                        if (OGRSQLiteImportSpatiaLiteGeometry(
                                pabyGpkg, iGpkgSize, &poGeomPtr) != OGRERR_NONE)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Unable to read geometry");
                        }
                        poGeom.reset(poGeomPtr);
                    }
                    else if (m_bUndoDiscardCoordLSBOnReading)
                    {
                        poGeom->roundCoordinates(
                            m_poFeatureDefn->GetGeomFieldDefn(0)
                                ->GetCoordinatePrecision());
                    }
                    if (poGeom != nullptr)
                    {
                        nWKBSize = poGeom->WkbSize();
                    }
                    if (m_poFilterGeom != nullptr &&
                        !FilterGeometry(poGeom.get()))
                    {
                        continue;
                    }
                }

                if (nWKBSize != 0)
                {
                    if (iFeat > 0)
                    {
                        auto panOffsets = static_cast<int32_t *>(
                            const_cast<void *>(psArray->buffers[1]));
                        const uint32_t nCurLength =
                            static_cast<uint32_t>(panOffsets[iFeat]);
                        if (nWKBSize <= nMemLimit &&
                            nWKBSize > nMemLimit - nCurLength)
                        {
                            m_bDoStep = false;
                            break;
                        }
                    }

                    GByte *outPtr = sHelper.GetPtrForStringOrBinary(
                        iArrowField, iFeat, nWKBSize);
                    if (outPtr == nullptr)
                    {
                        errorErrno = ENOMEM;
                        goto error;
                    }
                    if (poGeom)
                    {
                        poGeom->exportToWkb(wkbNDR, outPtr, wkbVariantIso);
                    }
                    else
                    {
                        memcpy(outPtr, pabyWkb, nWKBSize);
                    }
                }
                else
                {
                    sHelper.SetEmptyStringOrBinary(psArray, iFeat);
                }
            }

            if (nWKBSize == 0)
            {
                if (!sHelper.SetNull(iArrowField, iFeat))
                {
                    errorErrno = ENOMEM;
                    goto error;
                }
            }
        }

        for (int iField = 0; iField < sHelper.m_nFieldCount; iField++)
        {
            const int iArrowField = sHelper.m_mapOGRFieldToArrowField[iField];
            if (iArrowField < 0)
                continue;
            const OGRFieldDefn *poFieldDefn =
                m_poFeatureDefn->GetFieldDefnUnsafe(iField);

            auto psArray = out_array->children[iArrowField];
            const int iRawField = m_anFieldOrdinals[iField];

            const int nSqlite3ColType = sqlite3_column_type(hStmt, iRawField);
            if (nSqlite3ColType == SQLITE_NULL)
            {
                if (!sHelper.SetNull(iArrowField, iFeat))
                {
                    errorErrno = ENOMEM;
                    goto error;
                }
                continue;
            }

            switch (poFieldDefn->GetType())
            {
                case OFTInteger:
                {
                    const int nVal = sqlite3_column_int(hStmt, iRawField);
                    if (poFieldDefn->GetSubType() == OFSTBoolean)
                    {
                        if (nVal != 0)
                        {
                            sHelper.SetBoolOn(psArray, iFeat);
                        }
                    }
                    else if (poFieldDefn->GetSubType() == OFSTInt16)
                    {
                        sHelper.SetInt16(psArray, iFeat,
                                         static_cast<int16_t>(nVal));
                    }
                    else
                    {
                        sHelper.SetInt32(psArray, iFeat, nVal);
                    }
                    break;
                }

                case OFTInteger64:
                {
                    sHelper.SetInt64(psArray, iFeat,
                                     sqlite3_column_int64(hStmt, iRawField));
                    break;
                }

                case OFTReal:
                {
                    const double dfVal =
                        sqlite3_column_double(hStmt, iRawField);
                    if (poFieldDefn->GetSubType() == OFSTFloat32)
                    {
                        sHelper.SetFloat(psArray, iFeat,
                                         static_cast<float>(dfVal));
                    }
                    else
                    {
                        sHelper.SetDouble(psArray, iFeat, dfVal);
                    }
                    break;
                }

                case OFTBinary:
                {
                    const uint32_t nBytes = static_cast<uint32_t>(
                        sqlite3_column_bytes(hStmt, iRawField));
                    // coverity[tainted_data_return]
                    const void *pabyData =
                        sqlite3_column_blob(hStmt, iRawField);
                    if (pabyData != nullptr || nBytes == 0)
                    {
                        if (iFeat > 0)
                        {
                            auto panOffsets = static_cast<int32_t *>(
                                const_cast<void *>(psArray->buffers[1]));
                            const uint32_t nCurLength =
                                static_cast<uint32_t>(panOffsets[iFeat]);
                            if (nBytes <= nMemLimit &&
                                nBytes > nMemLimit - nCurLength)
                            {
                                m_bDoStep = false;
                                m_iNextShapeId--;
                                m_nFeaturesRead--;
                                goto after_loop;
                            }
                        }

                        GByte *outPtr = sHelper.GetPtrForStringOrBinary(
                            iArrowField, iFeat, nBytes);
                        if (outPtr == nullptr)
                        {
                            errorErrno = ENOMEM;
                            goto error;
                        }
                        if (nBytes)
                            memcpy(outPtr, pabyData, nBytes);
                    }
                    else
                    {
                        sHelper.SetEmptyStringOrBinary(psArray, iFeat);
                    }
                    break;
                }

                case OFTDate:
                {
                    OGRField ogrField;
                    if (ParseDateField(hStmt, iRawField, nSqlite3ColType,
                                       &ogrField, poFieldDefn, nFID))
                    {
                        sHelper.SetDate(psArray, iFeat, brokenDown, ogrField);
                    }
                    break;
                }

                case OFTDateTime:
                {
                    OGRField ogrField;
                    if (ParseDateTimeField(hStmt, iRawField, nSqlite3ColType,
                                           &ogrField, poFieldDefn, nFID))
                    {
                        sHelper.SetDateTime(psArray, iFeat, brokenDown,
                                            sHelper.m_anTZFlags[iField],
                                            ogrField);
                    }
                    break;
                }

                case OFTString:
                {
                    const auto pszTxt = reinterpret_cast<const char *>(
                        sqlite3_column_text(hStmt, iRawField));
                    if (pszTxt != nullptr)
                    {
                        const size_t nBytes = strlen(pszTxt);
                        if (iFeat > 0)
                        {
                            auto panOffsets = static_cast<int32_t *>(
                                const_cast<void *>(psArray->buffers[1]));
                            const uint32_t nCurLength =
                                static_cast<uint32_t>(panOffsets[iFeat]);
                            if (nBytes <= nMemLimit &&
                                nBytes > nMemLimit - nCurLength)
                            {
                                m_bDoStep = false;
                                m_iNextShapeId--;
                                m_nFeaturesRead--;
                                goto after_loop;
                            }
                        }

                        GByte *outPtr = sHelper.GetPtrForStringOrBinary(
                            iArrowField, iFeat, nBytes);
                        if (outPtr == nullptr)
                        {
                            errorErrno = ENOMEM;
                            goto error;
                        }
                        if (nBytes)
                            memcpy(outPtr, pszTxt, nBytes);
                    }
                    else
                    {
                        sHelper.SetEmptyStringOrBinary(psArray, iFeat);
                        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                                 sqlite3_errmsg(m_poDS->GetDB()));
                    }
                    break;
                }

                default:
                    break;
            }
        }

        ++iFeat;
    }
after_loop:
    sHelper.Shrink(iFeat);
    if (iFeat == 0)
        sHelper.ClearArray();

    return 0;

error:
    sHelper.ClearArray();
    return errorErrno;
}

/************************************************************************/
/*                      GetFIDColumn()                                  */
/************************************************************************/

const char *OGRGeoPackageLayer::GetFIDColumn()
{
    if (!m_pszFidColumn)
        return "";
    else
        return m_pszFidColumn;
}

/************************************************************************/
/*                      TestCapability()                                */
/************************************************************************/

int OGRGeoPackageLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCIgnoreFields))
        return TRUE;
    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;
    else if (EQUAL(pszCap, OLCFastGetArrowStream))
        return TRUE;
    else if (EQUAL(pszCap, OLCZGeometries))
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                          BuildFeatureDefn()                          */
/*                                                                      */
/*      Build feature definition from a set of column definitions       */
/*      set on a statement.  Sift out geometry and FID fields.          */
/************************************************************************/

void OGRGeoPackageLayer::BuildFeatureDefn(const char *pszLayerName,
                                          sqlite3_stmt *hStmt)

{
    m_poFeatureDefn = new OGRSQLiteFeatureDefn(pszLayerName);
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();

    const int nRawColumns = sqlite3_column_count(hStmt);

    m_anFieldOrdinals.resize(nRawColumns);

    const bool bPromoteToInteger64 =
        CPLTestBool(CPLGetConfigOption("OGR_PROMOTE_TO_INTEGER64", "FALSE"));

#ifdef SQLITE_HAS_COLUMN_METADATA
    // Check that there are not several FID fields referenced.
    // This is not a sufficient condition to ensure that we can get a true FID,
    // but when this occurs, we are (almost) sure that this cannot be a FID.
    int nFIDCandidates = 0;
    for (int iCol = 0; iCol < nRawColumns; iCol++)
    {
        const char *pszTableName = sqlite3_column_table_name(hStmt, iCol);
        const char *pszOriginName = sqlite3_column_origin_name(hStmt, iCol);
        if (pszTableName != nullptr && pszOriginName != nullptr)
        {
            OGRLayer *poLayer = m_poDS->GetLayerByName(pszTableName);
            if (poLayer != nullptr)
            {
                if (EQUAL(pszOriginName, poLayer->GetFIDColumn()))
                {
                    nFIDCandidates++;
                }
            }
        }
    }
#endif

    bool bGeometryColumnGuessed = false;
    for (int iCol = 0; iCol < nRawColumns; iCol++)
    {
        OGRFieldDefn oField(SQLUnescape(sqlite3_column_name(hStmt, iCol)),
                            OFTString);

        // In some cases, particularly when there is a real name for
        // the primary key/_rowid_ column we will end up getting the
        // primary key column appearing twice.  Ignore any repeated names.
        if (m_poFeatureDefn->GetFieldIndex(oField.GetNameRef()) != -1)
            continue;

        if (m_pszFidColumn != nullptr &&
            EQUAL(m_pszFidColumn, oField.GetNameRef()))
            continue;

        // The rowid is for internal use, not a real column.
        if (EQUAL(oField.GetNameRef(), "_rowid_"))
            continue;

        // this will avoid the old geom field to appear when running something
        // like "select st_buffer(geom,5) as geom, * from my_layer"
        if (m_poFeatureDefn->GetGeomFieldCount() &&
            EQUAL(oField.GetNameRef(),
                  m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef()))
        {
            continue;
        }

#ifdef SQLITE_HAS_COLUMN_METADATA
        const char *pszTableName = sqlite3_column_table_name(hStmt, iCol);
        const char *pszOriginName = sqlite3_column_origin_name(hStmt, iCol);
        if (pszTableName != nullptr && pszOriginName != nullptr)
        {
            OGRLayer *poLayer = m_poDS->GetLayerByName(pszTableName);
            if (poLayer != nullptr)
            {
                if (EQUAL(pszOriginName, poLayer->GetGeometryColumn()))
                {
                    if (bGeometryColumnGuessed ||
                        m_poFeatureDefn->GetGeomFieldCount() == 0)
                    {
                        if (bGeometryColumnGuessed)
                            m_poFeatureDefn->DeleteGeomFieldDefn(0);
                        OGRGeomFieldDefn oGeomField(
                            poLayer->GetLayerDefn()->GetGeomFieldDefn(0));
                        oGeomField.SetName(oField.GetNameRef());
                        m_poFeatureDefn->AddGeomFieldDefn(&oGeomField);
                        m_iGeomCol = iCol;
                    }
                    continue;
                }
                else if (EQUAL(pszOriginName, poLayer->GetFIDColumn()) &&
                         m_pszFidColumn == nullptr && nFIDCandidates == 1)
                {
                    m_pszFidColumn = CPLStrdup(oField.GetNameRef());
                    m_iFIDCol = iCol;
                    continue;
                }
                int nSrcIdx =
                    poLayer->GetLayerDefn()->GetFieldIndex(oField.GetNameRef());
                if (nSrcIdx >= 0)
                {
                    OGRFieldDefn *poSrcField =
                        poLayer->GetLayerDefn()->GetFieldDefn(nSrcIdx);
                    oField.SetType(poSrcField->GetType());
                    oField.SetSubType(poSrcField->GetSubType());
                    oField.SetWidth(poSrcField->GetWidth());
                    oField.SetPrecision(poSrcField->GetPrecision());
                    oField.SetDomainName(poSrcField->GetDomainName());
                    m_poFeatureDefn->AddFieldDefn(&oField);
                    m_anFieldOrdinals[m_poFeatureDefn->GetFieldCount() - 1] =
                        iCol;
                    continue;
                }
            }
        }
#endif

        const int nColType = sqlite3_column_type(hStmt, iCol);
        if (m_poFeatureDefn->GetGeomFieldCount() == 0 &&
            m_pszFidColumn == nullptr && nColType == SQLITE_INTEGER &&
            EQUAL(oField.GetNameRef(), "FID"))
        {
            m_pszFidColumn = CPLStrdup(oField.GetNameRef());
            m_iFIDCol = iCol;
            continue;
        }

        // Heuristics to help for https://github.com/OSGeo/gdal/issues/8587
        if (nColType == SQLITE_NULL && m_iGeomCol < 0
#ifdef SQLITE_HAS_COLUMN_METADATA
            && !pszTableName && !pszOriginName
#endif
        )
        {
            bool bIsLikelyGeomColName = EQUAL(oField.GetNameRef(), "geom") ||
                                        EQUAL(oField.GetNameRef(), "geometry");
            bool bIsGeomFunction = false;
            if (!bIsLikelyGeomColName)
                bIsGeomFunction = OGRSQLiteIsSpatialFunctionReturningGeometry(
                    oField.GetNameRef());
            if (bIsLikelyGeomColName || bIsGeomFunction)
            {
                bGeometryColumnGuessed = bIsLikelyGeomColName;
                OGRGeomFieldDefn oGeomField(oField.GetNameRef(), wkbUnknown);
                m_poFeatureDefn->AddGeomFieldDefn(&oGeomField);
                m_iGeomCol = iCol;
                continue;
            }
        }

        const char *pszDeclType = sqlite3_column_decltype(hStmt, iCol);

        // Recognize a geometry column from trying to build the geometry
        if (nColType == SQLITE_BLOB &&
            m_poFeatureDefn->GetGeomFieldCount() == 0)
        {
            const int nBytes = sqlite3_column_bytes(hStmt, iCol);
            if (nBytes >= 8)
            {
                // coverity[tainted_data_return]
                const GByte *pabyGpkg = reinterpret_cast<const GByte *>(
                    sqlite3_column_blob(hStmt, iCol));
                GPkgHeader oHeader;
                OGRGeometry *poGeom = nullptr;
                int nSRID = 0;

                if (GPkgHeaderFromWKB(pabyGpkg, nBytes, &oHeader) ==
                    OGRERR_NONE)
                {
                    poGeom = GPkgGeometryToOGR(pabyGpkg, nBytes, nullptr);
                    nSRID = oHeader.iSrsId;
                }
                else
                {
                    // Try also spatialite geometry blobs
                    if (OGRSQLiteImportSpatiaLiteGeometry(
                            pabyGpkg, nBytes, &poGeom, &nSRID) != OGRERR_NONE)
                    {
                        delete poGeom;
                        poGeom = nullptr;
                    }
                }

                if (poGeom)
                {
                    OGRGeomFieldDefn oGeomField(oField.GetNameRef(),
                                                wkbUnknown);

                    /* Read the SRS */
                    OGRSpatialReference *poSRS =
                        m_poDS->GetSpatialRef(nSRID, true);
                    if (poSRS)
                    {
                        oGeomField.SetSpatialRef(poSRS);
                        poSRS->Dereference();
                    }

                    OGRwkbGeometryType eGeomType = poGeom->getGeometryType();
                    if (pszDeclType != nullptr)
                    {
                        OGRwkbGeometryType eDeclaredGeomType =
                            GPkgGeometryTypeToWKB(pszDeclType, false, false);
                        if (eDeclaredGeomType != wkbUnknown)
                        {
                            eGeomType = OGR_GT_SetModifier(
                                eDeclaredGeomType, OGR_GT_HasZ(eGeomType),
                                OGR_GT_HasM(eGeomType));
                        }
                    }
                    oGeomField.SetType(eGeomType);

                    delete poGeom;
                    poGeom = nullptr;

                    m_poFeatureDefn->AddGeomFieldDefn(&oGeomField);
                    m_iGeomCol = iCol;
                    continue;
                }
            }
        }

        switch (nColType)
        {
            case SQLITE_INTEGER:
                if (bPromoteToInteger64)
                    oField.SetType(OFTInteger64);
                else
                {
                    GIntBig nVal = sqlite3_column_int64(hStmt, iCol);
                    if (CPL_INT64_FITS_ON_INT32(nVal))
                        oField.SetType(OFTInteger);
                    else
                        oField.SetType(OFTInteger64);
                }
                break;

            case SQLITE_FLOAT:
                oField.SetType(OFTReal);
                break;

            case SQLITE_BLOB:
                oField.SetType(OFTBinary);
                break;

            default:
                /* leave it as OFTString */;
        }

        if (pszDeclType != nullptr)
        {
            OGRFieldSubType eSubType;
            int nMaxWidth = 0;
            const int nFieldType =
                GPkgFieldToOGR(pszDeclType, eSubType, nMaxWidth);
            if (nFieldType <= OFTMaxType)
            {
                oField.SetType(static_cast<OGRFieldType>(nFieldType));
                oField.SetSubType(eSubType);
                oField.SetWidth(nMaxWidth);
            }
        }

        m_poFeatureDefn->AddFieldDefn(&oField);
        m_anFieldOrdinals[m_poFeatureDefn->GetFieldCount() - 1] = iCol;
    }
}

/************************************************************************/
/*                          SetIgnoredFields()                          */
/************************************************************************/

OGRErr OGRGeoPackageLayer::SetIgnoredFields(CSLConstList papszFields)
{
    OGRErr eErr = OGRLayer::SetIgnoredFields(papszFields);
    if (eErr == OGRERR_NONE)
    {
        // So that OGRGeoPackageTableLayer::BuildColumns() is called
        ResetReading();
    }
    return eErr;
}
