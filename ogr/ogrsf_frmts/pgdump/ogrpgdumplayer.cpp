/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDumpLayer class
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_pgdump.h"
#include "cpl_conv.h"
#include "cpl_md5.h"
#include "cpl_string.h"
#include "ogr_p.h"

#include <limits>

//
static CPLString
OGRPGDumpEscapeStringList(char **papszItems, bool bForInsertOrUpdate,
                          OGRPGCommonEscapeStringCbk pfnEscapeString,
                          void *userdata);

static CPLString OGRPGDumpEscapeStringWithUserData(
    CPL_UNUSED void *user_data, const char *pszStrValue, int nMaxLength,
    CPL_UNUSED const char *pszLayerName, const char *pszFieldName)
{
    return OGRPGDumpEscapeString(pszStrValue, nMaxLength, pszFieldName);
}

/************************************************************************/
/*                        OGRPGDumpLayer()                              */
/************************************************************************/

OGRPGDumpLayer::OGRPGDumpLayer(OGRPGDumpDataSource *poDSIn,
                               const char *pszSchemaNameIn,
                               const char *pszTableName,
                               const char *pszFIDColumnIn, int bWriteAsHexIn,
                               int bCreateTableIn)
    : m_pszSchemaName(CPLStrdup(pszSchemaNameIn)),
      m_pszSqlTableName(CPLStrdup(CPLString().Printf(
          "%s.%s", OGRPGDumpEscapeColumnName(m_pszSchemaName).c_str(),
          OGRPGDumpEscapeColumnName(pszTableName).c_str()))),
      m_pszFIDColumn(pszFIDColumnIn ? CPLStrdup(pszFIDColumnIn) : nullptr),
      m_poFeatureDefn(new OGRFeatureDefn(pszTableName)), m_poDS(poDSIn),
      m_bWriteAsHex(CPL_TO_BOOL(bWriteAsHexIn)), m_bCreateTable(bCreateTableIn)
{
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();
}

/************************************************************************/
/*                          ~OGRPGDumpLayer()                           */
/************************************************************************/

OGRPGDumpLayer::~OGRPGDumpLayer()
{
    EndCopy();
    LogDeferredFieldCreationIfNeeded();
    UpdateSequenceIfNeeded();
    for (const auto &osSQL : m_aosSpatialIndexCreationCommands)
    {
        m_poDS->Log(osSQL.c_str());
    }

    m_poFeatureDefn->Release();
    CPLFree(m_pszSchemaName);
    CPLFree(m_pszSqlTableName);
    CPLFree(m_pszFIDColumn);
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPGDumpLayer::GetNextFeature()
{
    CPLError(CE_Failure, CPLE_NotSupported, "PGDump driver is write only");
    return nullptr;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

int OGRPGDumpLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCSequentialWrite) || EQUAL(pszCap, OLCCreateField) ||
        EQUAL(pszCap, OLCCreateGeomField) ||
        EQUAL(pszCap, OLCCurveGeometries) || EQUAL(pszCap, OLCZGeometries) ||
        EQUAL(pszCap, OLCMeasuredGeometries))
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                   LogDeferredFieldCreationIfNeeded()                 */
/************************************************************************/

void OGRPGDumpLayer::LogDeferredFieldCreationIfNeeded()
{
    // Emit column creation
    if (!m_aosDeferrentNonGeomFieldCreationCommands.empty() ||
        !m_aosDeferredGeomFieldCreationCommands.empty())
    {
        CPLAssert(m_bCreateTable);
        CPLAssert(!m_bGeomColumnPositionImmediate);
        // In non-immediate mode, we put geometry fields after non-geometry
        // ones
        for (const auto &osSQL : m_aosDeferrentNonGeomFieldCreationCommands)
            m_poDS->Log(osSQL.c_str());
        for (const auto &osSQL : m_aosDeferredGeomFieldCreationCommands)
            m_poDS->Log(osSQL.c_str());
        m_aosDeferrentNonGeomFieldCreationCommands.clear();
        m_aosDeferredGeomFieldCreationCommands.clear();
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRErr OGRPGDumpLayer::ICreateFeature(OGRFeature *poFeature)
{
    if (nullptr == poFeature)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "NULL pointer to OGRFeature passed to CreateFeature().");
        return OGRERR_FAILURE;
    }

    LogDeferredFieldCreationIfNeeded();

    /* In case the FID column has also been created as a regular field */
    if (m_iFIDAsRegularColumnIndex >= 0)
    {
        if (poFeature->GetFID() == OGRNullFID)
        {
            if (poFeature->IsFieldSetAndNotNull(m_iFIDAsRegularColumnIndex))
            {
                poFeature->SetFID(
                    poFeature->GetFieldAsInteger64(m_iFIDAsRegularColumnIndex));
            }
        }
        else
        {
            if (!poFeature->IsFieldSetAndNotNull(m_iFIDAsRegularColumnIndex) ||
                poFeature->GetFieldAsInteger64(m_iFIDAsRegularColumnIndex) !=
                    poFeature->GetFID())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Inconsistent values of FID and field of same name");
                return OGRERR_FAILURE;
            }
        }
    }

    if (!poFeature->Validate((OGR_F_VAL_ALL & ~OGR_F_VAL_WIDTH) |
                                 OGR_F_VAL_ALLOW_DIFFERENT_GEOM_DIM,
                             TRUE))
        return OGRERR_FAILURE;

    // We avoid testing the config option too often.
    if (m_bUseCopy == USE_COPY_UNSET)
        m_bUseCopy = CPLTestBool(CPLGetConfigOption("PG_USE_COPY", "NO"));

    OGRErr eErr;
    if (!m_bUseCopy)
    {
        eErr = CreateFeatureViaInsert(poFeature);
    }
    else
    {
        // If there's a unset field with a default value, then we must use a
        // specific INSERT statement to avoid unset fields to be bound to NULL.
        bool bHasDefaultValue = false;
        const int nFieldCount = m_poFeatureDefn->GetFieldCount();
        for (int iField = 0; iField < nFieldCount; iField++)
        {
            if (!poFeature->IsFieldSetAndNotNull(iField) &&
                poFeature->GetFieldDefnRef(iField)->GetDefault() != nullptr)
            {
                bHasDefaultValue = true;
                break;
            }
        }
        if (bHasDefaultValue)
        {
            EndCopy();
            eErr = CreateFeatureViaInsert(poFeature);
        }
        else
        {
            const bool bFIDSet = poFeature->GetFID() != OGRNullFID;
            if (m_bCopyActive && bFIDSet != m_bCopyStatementWithFID)
            {
                EndCopy();
                eErr = CreateFeatureViaInsert(poFeature);
            }
            else
            {
                if (!m_bCopyActive)
                {
                    // This is a heuristics. If the first feature to be copied
                    // has a FID set (and that a FID column has been
                    // identified), then we will try to copy FID values from
                    // features. Otherwise, we will not do and assume that the
                    // FID column is an autoincremented column.
                    StartCopy(bFIDSet);
                    m_bCopyStatementWithFID = bFIDSet;
                    m_bNeedToUpdateSequence = bFIDSet;
                }

                eErr = CreateFeatureViaCopy(poFeature);
                if (bFIDSet)
                    m_bAutoFIDOnCreateViaCopy = false;
                if (eErr == OGRERR_NONE && m_bAutoFIDOnCreateViaCopy)
                {
                    poFeature->SetFID(++m_iNextShapeId);
                }
            }
        }
    }

    if (eErr == OGRERR_NONE && m_iFIDAsRegularColumnIndex >= 0)
    {
        poFeature->SetField(m_iFIDAsRegularColumnIndex, poFeature->GetFID());
    }
    return eErr;
}

/************************************************************************/
/*                       CreateFeatureViaInsert()                       */
/************************************************************************/

OGRErr OGRPGDumpLayer::CreateFeatureViaInsert(OGRFeature *poFeature)

{
    OGRErr eErr = OGRERR_FAILURE;

    if (nullptr == poFeature)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "NULL pointer to OGRFeature passed to CreateFeatureViaInsert().");
        return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Form the INSERT command.                                        */
    /* -------------------------------------------------------------------- */
    CPLString osCommand;
    osCommand.Printf("INSERT INTO %s (", m_pszSqlTableName);

    bool bNeedComma = false;

    if (poFeature->GetFID() != OGRNullFID && m_pszFIDColumn != nullptr)
    {
        m_bNeedToUpdateSequence = true;

        osCommand += OGRPGDumpEscapeColumnName(m_pszFIDColumn);
        bNeedComma = true;
    }
    else
    {
        UpdateSequenceIfNeeded();
    }

    const auto AddGeomFieldsName = [this, poFeature, &bNeedComma, &osCommand]()
    {
        for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); i++)
        {
            OGRGeometry *poGeom = poFeature->GetGeomFieldRef(i);
            if (poGeom != nullptr)
            {
                if (bNeedComma)
                    osCommand += ", ";

                OGRGeomFieldDefn *poGFldDefn =
                    poFeature->GetGeomFieldDefnRef(i);
                osCommand +=
                    OGRPGDumpEscapeColumnName(poGFldDefn->GetNameRef());
                bNeedComma = true;
            }
        }
    };

    if (m_bGeomColumnPositionImmediate)
        AddGeomFieldsName();

    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++)
    {
        if (i == m_iFIDAsRegularColumnIndex)
            continue;
        if (!poFeature->IsFieldSet(i))
            continue;

        if (!bNeedComma)
            bNeedComma = true;
        else
            osCommand += ", ";

        osCommand += OGRPGDumpEscapeColumnName(
            m_poFeatureDefn->GetFieldDefn(i)->GetNameRef());
    }

    if (!m_bGeomColumnPositionImmediate)
        AddGeomFieldsName();

    const bool bEmptyInsert = !bNeedComma;

    osCommand += ") VALUES (";

    bNeedComma = false;

    /* Set the geometry */
    const auto AddGeomFieldsValue = [this, poFeature, &bNeedComma, &osCommand]()
    {
        for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); i++)
        {
            OGRGeometry *poGeom = poFeature->GetGeomFieldRef(i);
            if (poGeom != nullptr)
            {
                char *pszWKT = nullptr;

                OGRPGDumpGeomFieldDefn *poGFldDefn =
                    (OGRPGDumpGeomFieldDefn *)poFeature->GetGeomFieldDefnRef(i);

                poGeom->closeRings();
                poGeom->set3D(poGFldDefn->m_nGeometryTypeFlags &
                              OGRGeometry::OGR_G_3D);
                poGeom->setMeasured(poGFldDefn->m_nGeometryTypeFlags &
                                    OGRGeometry::OGR_G_MEASURED);

                if (bNeedComma)
                    osCommand += ", ";

                if (m_bWriteAsHex)
                {
                    char *pszHex =
                        OGRGeometryToHexEWKB(poGeom, poGFldDefn->m_nSRSId,
                                             m_nPostGISMajor, m_nPostGISMinor);
                    osCommand += "'";
                    if (pszHex)
                        osCommand += pszHex;
                    osCommand += "'";
                    CPLFree(pszHex);
                }
                else
                {
                    poGeom->exportToWkt(&pszWKT, wkbVariantIso);

                    if (pszWKT != nullptr)
                    {
                        osCommand += CPLString().Printf(
                            "GeomFromEWKT('SRID=%d;%s'::TEXT) ",
                            poGFldDefn->m_nSRSId, pszWKT);
                        CPLFree(pszWKT);
                    }
                    else
                        osCommand += "''";
                }

                bNeedComma = true;
            }
        }
    };

    /* Set the FID */
    if (poFeature->GetFID() != OGRNullFID && m_pszFIDColumn != nullptr)
    {
        if (bNeedComma)
            osCommand += ", ";
        osCommand += CPLString().Printf(CPL_FRMT_GIB, poFeature->GetFID());
        bNeedComma = true;
    }

    if (m_bGeomColumnPositionImmediate)
        AddGeomFieldsValue();

    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++)
    {
        if (i == m_iFIDAsRegularColumnIndex)
            continue;
        if (!poFeature->IsFieldSet(i))
            continue;

        if (bNeedComma)
            osCommand += ", ";
        else
            bNeedComma = true;

        OGRPGCommonAppendFieldValue(osCommand, poFeature, i,
                                    OGRPGDumpEscapeStringWithUserData, nullptr);
    }

    if (!m_bGeomColumnPositionImmediate)
        AddGeomFieldsValue();

    osCommand += ")";

    if (bEmptyInsert)
        osCommand.Printf("INSERT INTO %s DEFAULT VALUES", m_pszSqlTableName);

    /* -------------------------------------------------------------------- */
    /*      Execute the insert.                                             */
    /* -------------------------------------------------------------------- */
    m_poDS->Log(osCommand);

    if (poFeature->GetFID() == OGRNullFID)
        poFeature->SetFID(++m_iNextShapeId);

    return OGRERR_NONE;
}

/************************************************************************/
/*                        CreateFeatureViaCopy()                        */
/************************************************************************/

OGRErr OGRPGDumpLayer::CreateFeatureViaCopy(OGRFeature *poFeature)
{
    CPLString osCommand;

    if (m_bFIDColumnInCopyFields)
        OGRPGCommonAppendCopyFID(osCommand, poFeature);

    const auto AddGeomFieldsValue = [this, poFeature, &osCommand]()
    {
        for (int i = 0; i < poFeature->GetGeomFieldCount(); i++)
        {
            OGRGeometry *poGeometry = poFeature->GetGeomFieldRef(i);
            char *pszGeom = nullptr;
            if (nullptr !=
                poGeometry /* && (bHasWkb || bHasPostGISGeometry || bHasPostGISGeography) */)
            {
                OGRPGDumpGeomFieldDefn *poGFldDefn =
                    (OGRPGDumpGeomFieldDefn *)poFeature->GetGeomFieldDefnRef(i);

                poGeometry->closeRings();
                poGeometry->set3D(poGFldDefn->m_nGeometryTypeFlags &
                                  OGRGeometry::OGR_G_3D);
                poGeometry->setMeasured(poGFldDefn->m_nGeometryTypeFlags &
                                        OGRGeometry::OGR_G_MEASURED);

                pszGeom =
                    OGRGeometryToHexEWKB(poGeometry, poGFldDefn->m_nSRSId,
                                         m_nPostGISMajor, m_nPostGISMinor);
            }

            if (!osCommand.empty())
                osCommand += "\t";
            if (pszGeom)
            {
                osCommand += pszGeom;
                CPLFree(pszGeom);
            }
            else
            {
                osCommand += "\\N";
            }
        }
    };

    if (m_bGeomColumnPositionImmediate)
        AddGeomFieldsValue();

    OGRPGCommonAppendCopyRegularFields(
        osCommand, poFeature, m_pszFIDColumn,
        std::vector<bool>(m_poFeatureDefn->GetFieldCount(), true),
        OGRPGDumpEscapeStringWithUserData, nullptr);

    if (!m_bGeomColumnPositionImmediate)
        AddGeomFieldsValue();

    /* ------------------------------------------------------------ */
    /*      Execute the copy.                                       */
    /* ------------------------------------------------------------ */

    OGRErr result = OGRERR_NONE;

    m_poDS->Log(osCommand, false);

    return result;
}

/************************************************************************/
/*                      OGRPGCommonAppendCopyFID()                      */
/************************************************************************/

void OGRPGCommonAppendCopyFID(CPLString &osCommand, OGRFeature *poFeature)
{
    if (!osCommand.empty())
        osCommand += "\t";

    /* Set the FID */
    if (poFeature->GetFID() != OGRNullFID)
    {
        osCommand += CPLString().Printf(CPL_FRMT_GIB, poFeature->GetFID());
    }
    else
    {
        osCommand += "\\N";
    }
}

/************************************************************************/
/*                OGRPGCommonAppendCopyRegularFields()                  */
/************************************************************************/

void OGRPGCommonAppendCopyRegularFields(
    CPLString &osCommand, OGRFeature *poFeature, const char *pszFIDColumn,
    const std::vector<bool> &abFieldsToInclude,
    OGRPGCommonEscapeStringCbk pfnEscapeString, void *userdata)
{
    const OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();
    const int nFIDIndex =
        pszFIDColumn ? poFeatureDefn->GetFieldIndex(pszFIDColumn) : -1;

    const int nFieldCount = poFeatureDefn->GetFieldCount();
    bool bAddTab = !osCommand.empty();

    CPLAssert(nFieldCount == static_cast<int>(abFieldsToInclude.size()));

    for (int i = 0; i < nFieldCount; i++)
    {
        if (i == nFIDIndex)
            continue;
        if (!abFieldsToInclude[i])
            continue;

        const char *pszStrValue = poFeature->GetFieldAsString(i);
        char *pszNeedToFree = nullptr;

        if (bAddTab)
            osCommand += "\t";
        bAddTab = true;

        if (!poFeature->IsFieldSetAndNotNull(i))
        {
            osCommand += "\\N";

            continue;
        }

        const int nOGRFieldType = poFeatureDefn->GetFieldDefn(i)->GetType();

        // We need special formatting for integer list values.
        if (nOGRFieldType == OFTIntegerList)
        {
            int nCount, nOff = 0;
            const int *panItems = poFeature->GetFieldAsIntegerList(i, &nCount);

            const size_t nLen = nCount * 13 + 10;
            pszNeedToFree = (char *)CPLMalloc(nLen);
            strcpy(pszNeedToFree, "{");
            for (int j = 0; j < nCount; j++)
            {
                if (j != 0)
                    strcat(pszNeedToFree + nOff, ",");

                nOff += static_cast<int>(strlen(pszNeedToFree + nOff));
                snprintf(pszNeedToFree + nOff, nLen - nOff, "%d", panItems[j]);
            }
            strcat(pszNeedToFree + nOff, "}");
            pszStrValue = pszNeedToFree;
        }

        else if (nOGRFieldType == OFTInteger64List)
        {
            int nCount, nOff = 0;
            const GIntBig *panItems =
                poFeature->GetFieldAsInteger64List(i, &nCount);

            const size_t nLen = nCount * 26 + 10;
            pszNeedToFree = (char *)CPLMalloc(nLen);
            strcpy(pszNeedToFree, "{");
            for (int j = 0; j < nCount; j++)
            {
                if (j != 0)
                    strcat(pszNeedToFree + nOff, ",");

                nOff += static_cast<int>(strlen(pszNeedToFree + nOff));
                snprintf(pszNeedToFree + nOff, nLen - nOff, CPL_FRMT_GIB,
                         panItems[j]);
            }
            strcat(pszNeedToFree + nOff, "}");
            pszStrValue = pszNeedToFree;
        }

        // We need special formatting for real list values.
        else if (nOGRFieldType == OFTRealList)
        {
            int nOff = 0;
            int nCount = 0;
            const double *padfItems =
                poFeature->GetFieldAsDoubleList(i, &nCount);

            const size_t nLen = nCount * 40 + 10;
            pszNeedToFree = (char *)CPLMalloc(nLen);
            strcpy(pszNeedToFree, "{");
            for (int j = 0; j < nCount; j++)
            {
                if (j != 0)
                    strcat(pszNeedToFree + nOff, ",");

                nOff += static_cast<int>(strlen(pszNeedToFree + nOff));
                // Check for special values. They need to be quoted.
                if (CPLIsNan(padfItems[j]))
                    snprintf(pszNeedToFree + nOff, nLen - nOff, "NaN");
                else if (CPLIsInf(padfItems[j]))
                    snprintf(pszNeedToFree + nOff, nLen - nOff,
                             (padfItems[j] > 0) ? "Infinity" : "-Infinity");
                else
                    CPLsnprintf(pszNeedToFree + nOff, nLen - nOff, "%.16g",
                                padfItems[j]);
            }
            strcat(pszNeedToFree + nOff, "}");
            pszStrValue = pszNeedToFree;
        }

        // We need special formatting for string list values.
        else if (nOGRFieldType == OFTStringList)
        {
            CPLString osStr;
            char **papszItems = poFeature->GetFieldAsStringList(i);

            pszStrValue = pszNeedToFree = CPLStrdup(OGRPGDumpEscapeStringList(
                papszItems, false, pfnEscapeString, userdata));
        }

        // Binary formatting
        else if (nOGRFieldType == OFTBinary)
        {
            int nLen = 0;
            GByte *pabyData = poFeature->GetFieldAsBinary(i, &nLen);
            char *pszBytea = OGRPGCommonGByteArrayToBYTEA(pabyData, nLen);

            pszStrValue = pszNeedToFree = pszBytea;
        }

        else if (nOGRFieldType == OFTReal)
        {
            // Check for special values. They need to be quoted.
            double dfVal = poFeature->GetFieldAsDouble(i);
            if (CPLIsNan(dfVal))
                pszStrValue = "NaN";
            else if (CPLIsInf(dfVal))
                pszStrValue = (dfVal > 0) ? "Infinity" : "-Infinity";
        }

        if (nOGRFieldType != OFTIntegerList &&
            nOGRFieldType != OFTInteger64List && nOGRFieldType != OFTRealList &&
            nOGRFieldType != OFTInteger && nOGRFieldType != OFTInteger64 &&
            nOGRFieldType != OFTReal && nOGRFieldType != OFTBinary)
        {
            int iUTFChar = 0;
            const int nMaxWidth = poFeatureDefn->GetFieldDefn(i)->GetWidth();

            for (int iChar = 0; pszStrValue[iChar] != '\0'; iChar++)
            {
                // count of utf chars
                if (nOGRFieldType != OFTStringList &&
                    (pszStrValue[iChar] & 0xc0) != 0x80)
                {
                    if (nMaxWidth > 0 && iUTFChar == nMaxWidth)
                    {
                        CPLDebug("PG",
                                 "Truncated %s field value, it was too long.",
                                 poFeatureDefn->GetFieldDefn(i)->GetNameRef());
                        break;
                    }
                    iUTFChar++;
                }

                /* Escape embedded \, \t, \n, \r since they will cause COPY
                   to misinterpret a line of text and thus abort */
                if (pszStrValue[iChar] == '\\' || pszStrValue[iChar] == '\t' ||
                    pszStrValue[iChar] == '\r' || pszStrValue[iChar] == '\n')
                {
                    osCommand += '\\';
                }

                osCommand += pszStrValue[iChar];
            }
        }
        else
        {
            osCommand += pszStrValue;
        }

        if (pszNeedToFree)
            CPLFree(pszNeedToFree);
    }
}

/************************************************************************/
/*                             StartCopy()                              */
/************************************************************************/

OGRErr OGRPGDumpLayer::StartCopy(int bSetFID)

{
    /* Tell the datasource we are now planning to copy data */
    m_poDS->StartCopy(this);

    CPLString osFields = BuildCopyFields(bSetFID);

    size_t size = osFields.size() + strlen(m_pszSqlTableName) + 100;
    char *pszCommand = (char *)CPLMalloc(size);

    snprintf(pszCommand, size, "COPY %s (%s) FROM STDIN", m_pszSqlTableName,
             osFields.c_str());

    m_poDS->Log(pszCommand);
    m_bCopyActive = true;

    CPLFree(pszCommand);

    return OGRERR_NONE;
}

/************************************************************************/
/*                              EndCopy()                               */
/************************************************************************/

OGRErr OGRPGDumpLayer::EndCopy()

{
    if (!m_bCopyActive)
        return OGRERR_NONE;

    m_bCopyActive = false;

    m_poDS->Log("\\.", false);

    m_bUseCopy = USE_COPY_UNSET;

    UpdateSequenceIfNeeded();

    return OGRERR_NONE;
}

/************************************************************************/
/*                       UpdateSequenceIfNeeded()                       */
/************************************************************************/

void OGRPGDumpLayer::UpdateSequenceIfNeeded()
{
    if (m_bNeedToUpdateSequence && m_pszFIDColumn != nullptr)
    {
        CPLString osCommand;
        osCommand.Printf(
            "SELECT setval(pg_get_serial_sequence(%s, %s), MAX(%s)) FROM %s",
            OGRPGDumpEscapeString(m_pszSqlTableName).c_str(),
            OGRPGDumpEscapeString(m_pszFIDColumn).c_str(),
            OGRPGDumpEscapeColumnName(m_pszFIDColumn).c_str(),
            m_pszSqlTableName);
        m_poDS->Log(osCommand);
        m_bNeedToUpdateSequence = false;
    }
}

/************************************************************************/
/*                          BuildCopyFields()                           */
/************************************************************************/

CPLString OGRPGDumpLayer::BuildCopyFields(int bSetFID)
{
    CPLString osFieldList;

    int nFIDIndex = -1;
    m_bFIDColumnInCopyFields = m_pszFIDColumn != nullptr && bSetFID;
    if (m_bFIDColumnInCopyFields)
    {
        nFIDIndex = m_poFeatureDefn->GetFieldIndex(m_pszFIDColumn);

        osFieldList += OGRPGDumpEscapeColumnName(m_pszFIDColumn);
    }

    const auto AddGeomFields = [this, &osFieldList]()
    {
        for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); i++)
        {
            if (!osFieldList.empty())
                osFieldList += ", ";

            OGRGeomFieldDefn *poGFldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);

            osFieldList += OGRPGDumpEscapeColumnName(poGFldDefn->GetNameRef());
        }
    };

    if (m_bGeomColumnPositionImmediate)
        AddGeomFields();

    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++)
    {
        if (i == nFIDIndex)
            continue;

        const char *pszName = m_poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if (!osFieldList.empty())
            osFieldList += ", ";

        osFieldList += OGRPGDumpEscapeColumnName(pszName);
    }

    if (!m_bGeomColumnPositionImmediate)
        AddGeomFields();

    return osFieldList;
}

/************************************************************************/
/*                       OGRPGDumpEscapeColumnName( )                   */
/************************************************************************/

CPLString OGRPGDumpEscapeColumnName(const char *pszColumnName)
{
    CPLString osStr = "\"";

    char ch = '\0';
    for (int i = 0; (ch = pszColumnName[i]) != '\0'; i++)
    {
        if (ch == '"')
            osStr.append(1, ch);
        osStr.append(1, ch);
    }

    osStr += "\"";

    return osStr;
}

/************************************************************************/
/*                             EscapeString( )                          */
/************************************************************************/

CPLString OGRPGDumpEscapeString(const char *pszStrValue, int nMaxLength,
                                const char *pszFieldName)
{
    CPLString osCommand;

    /* We need to quote and escape string fields. */
    osCommand += '\'';

    int nSrcLen = static_cast<int>(strlen(pszStrValue));
    const int nSrcLenUTF = CPLStrlenUTF8(pszStrValue);

    if (nMaxLength > 0 && nSrcLenUTF > nMaxLength)
    {
        CPLDebug("PG", "Truncated %s field value, it was too long.",
                 pszFieldName);

        int iUTF8Char = 0;
        for (int iChar = 0; iChar < nSrcLen; iChar++)
        {
            if ((((unsigned char *)pszStrValue)[iChar] & 0xc0) != 0x80)
            {
                if (iUTF8Char == nMaxLength)
                {
                    nSrcLen = iChar;
                    break;
                }
                iUTF8Char++;
            }
        }
    }

    for (int i = 0; i < nSrcLen; i++)
    {
        if (pszStrValue[i] == '\'')
        {
            osCommand += '\'';
            osCommand += '\'';
        }
        else
        {
            osCommand += pszStrValue[i];
        }
    }

    osCommand += '\'';

    return osCommand;
}

/************************************************************************/
/*                    OGRPGDumpEscapeStringList( )                      */
/************************************************************************/

static CPLString
OGRPGDumpEscapeStringList(char **papszItems, bool bForInsertOrUpdate,
                          OGRPGCommonEscapeStringCbk pfnEscapeString,
                          void *userdata)
{
    bool bFirstItem = true;
    CPLString osStr;
    if (bForInsertOrUpdate)
        osStr += "ARRAY[";
    else
        osStr += "{";
    while (papszItems && *papszItems)
    {
        if (!bFirstItem)
        {
            osStr += ',';
        }

        char *pszStr = *papszItems;
        if (*pszStr != '\0')
        {
            if (bForInsertOrUpdate)
                osStr += pfnEscapeString(userdata, pszStr, 0, "", "");
            else
            {
                osStr += '"';

                while (*pszStr)
                {
                    if (*pszStr == '"')
                        osStr += "\\";
                    osStr += *pszStr;
                    pszStr++;
                }

                osStr += '"';
            }
        }
        else
            osStr += "NULL";

        bFirstItem = false;

        papszItems++;
    }
    if (bForInsertOrUpdate)
    {
        osStr += "]";
        if (papszItems == nullptr)
            osStr += "::varchar[]";
    }
    else
        osStr += "}";
    return osStr;
}

/************************************************************************/
/*                          AppendFieldValue()                          */
/*                                                                      */
/* Used by CreateFeatureViaInsert() and SetFeature() to format a        */
/* non-empty field value                                                */
/************************************************************************/

void OGRPGCommonAppendFieldValue(CPLString &osCommand, OGRFeature *poFeature,
                                 int i,
                                 OGRPGCommonEscapeStringCbk pfnEscapeString,
                                 void *userdata)
{
    if (poFeature->IsFieldNull(i))
    {
        osCommand += "NULL";
        return;
    }

    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();
    OGRFieldType nOGRFieldType = poFeatureDefn->GetFieldDefn(i)->GetType();
    OGRFieldSubType eSubType = poFeatureDefn->GetFieldDefn(i)->GetSubType();

    // We need special formatting for integer list values.
    if (nOGRFieldType == OFTIntegerList)
    {
        int nCount, nOff = 0, j;
        const int *panItems = poFeature->GetFieldAsIntegerList(i, &nCount);

        const size_t nLen = nCount * 13 + 10;
        char *pszNeedToFree = (char *)CPLMalloc(nLen);
        strcpy(pszNeedToFree, "'{");
        for (j = 0; j < nCount; j++)
        {
            if (j != 0)
                strcat(pszNeedToFree + nOff, ",");

            nOff += static_cast<int>(strlen(pszNeedToFree + nOff));
            snprintf(pszNeedToFree + nOff, nLen - nOff, "%d", panItems[j]);
        }
        strcat(pszNeedToFree + nOff, "}'");

        osCommand += pszNeedToFree;
        CPLFree(pszNeedToFree);

        return;
    }

    else if (nOGRFieldType == OFTInteger64List)
    {
        int nCount, nOff = 0, j;
        const GIntBig *panItems =
            poFeature->GetFieldAsInteger64List(i, &nCount);

        const size_t nLen = nCount * 26 + 10;
        char *pszNeedToFree = (char *)CPLMalloc(nLen);
        strcpy(pszNeedToFree, "'{");
        for (j = 0; j < nCount; j++)
        {
            if (j != 0)
                strcat(pszNeedToFree + nOff, ",");

            nOff += static_cast<int>(strlen(pszNeedToFree + nOff));
            snprintf(pszNeedToFree + nOff, nLen - nOff, CPL_FRMT_GIB,
                     panItems[j]);
        }
        strcat(pszNeedToFree + nOff, "}'");

        osCommand += pszNeedToFree;
        CPLFree(pszNeedToFree);

        return;
    }

    // We need special formatting for real list values.
    else if (nOGRFieldType == OFTRealList)
    {
        int nCount = 0;
        int nOff = 0;
        const double *padfItems = poFeature->GetFieldAsDoubleList(i, &nCount);

        const size_t nLen = nCount * 40 + 10;
        char *pszNeedToFree = (char *)CPLMalloc(nLen);
        strcpy(pszNeedToFree, "'{");
        for (int j = 0; j < nCount; j++)
        {
            if (j != 0)
                strcat(pszNeedToFree + nOff, ",");

            nOff += static_cast<int>(strlen(pszNeedToFree + nOff));
            // Check for special values. They need to be quoted.
            if (CPLIsNan(padfItems[j]))
                snprintf(pszNeedToFree + nOff, nLen - nOff, "NaN");
            else if (CPLIsInf(padfItems[j]))
                snprintf(pszNeedToFree + nOff, nLen - nOff,
                         (padfItems[j] > 0) ? "Infinity" : "-Infinity");
            else
                CPLsnprintf(pszNeedToFree + nOff, nLen - nOff, "%.16g",
                            padfItems[j]);
        }
        strcat(pszNeedToFree + nOff, "}'");

        osCommand += pszNeedToFree;
        CPLFree(pszNeedToFree);

        return;
    }

    // We need special formatting for string list values.
    else if (nOGRFieldType == OFTStringList)
    {
        char **papszItems = poFeature->GetFieldAsStringList(i);

        osCommand += OGRPGDumpEscapeStringList(papszItems, true,
                                               pfnEscapeString, userdata);

        return;
    }

    // Binary formatting
    else if (nOGRFieldType == OFTBinary)
    {
        osCommand += "E'";

        int nLen = 0;
        GByte *pabyData = poFeature->GetFieldAsBinary(i, &nLen);
        char *pszBytea = OGRPGCommonGByteArrayToBYTEA(pabyData, nLen);

        osCommand += pszBytea;

        CPLFree(pszBytea);
        osCommand += "'";

        return;
    }

    // Flag indicating NULL or not-a-date date value
    // e.g. 0000-00-00 - there is no year 0
    bool bIsDateNull = false;

    const char *pszStrValue = poFeature->GetFieldAsString(i);

    // Check if date is NULL: 0000-00-00
    if (nOGRFieldType == OFTDate)
    {
        if (STARTS_WITH_CI(pszStrValue, "0000"))
        {
            pszStrValue = "NULL";
            bIsDateNull = true;
        }
    }
    else if (nOGRFieldType == OFTReal)
    {
        // Check for special values. They need to be quoted.
        double dfVal = poFeature->GetFieldAsDouble(i);
        if (CPLIsNan(dfVal))
            pszStrValue = "'NaN'";
        else if (CPLIsInf(dfVal))
            pszStrValue = (dfVal > 0) ? "'Infinity'" : "'-Infinity'";
    }
    else if ((nOGRFieldType == OFTInteger || nOGRFieldType == OFTInteger64) &&
             eSubType == OFSTBoolean)
        pszStrValue = poFeature->GetFieldAsInteger(i) ? "'t'" : "'f'";

    if (nOGRFieldType != OFTInteger && nOGRFieldType != OFTInteger64 &&
        nOGRFieldType != OFTReal && nOGRFieldType != OFTStringList &&
        !bIsDateNull)
    {
        osCommand += pfnEscapeString(
            userdata, pszStrValue, poFeatureDefn->GetFieldDefn(i)->GetWidth(),
            poFeatureDefn->GetName(),
            poFeatureDefn->GetFieldDefn(i)->GetNameRef());
    }
    else
    {
        osCommand += pszStrValue;
    }
}

/************************************************************************/
/*                      OGRPGCommonGByteArrayToBYTEA()                  */
/************************************************************************/

char *OGRPGCommonGByteArrayToBYTEA(const GByte *pabyData, size_t nLen)
{
    if (nLen > (std::numeric_limits<size_t>::max() - 1) / 5)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too big byte array");
        return CPLStrdup("");
    }
    const size_t nTextBufLen = nLen * 5 + 1;
    char *pszTextBuf = static_cast<char *>(VSI_MALLOC_VERBOSE(nTextBufLen));
    if (pszTextBuf == nullptr)
        return CPLStrdup("");

    size_t iDst = 0;

    for (size_t iSrc = 0; iSrc < nLen; iSrc++)
    {
        if (pabyData[iSrc] < 40 || pabyData[iSrc] > 126 ||
            pabyData[iSrc] == '\\')
        {
            snprintf(pszTextBuf + iDst, nTextBufLen - iDst, "\\\\%03o",
                     pabyData[iSrc]);
            iDst += 5;
        }
        else
            pszTextBuf[iDst++] = pabyData[iSrc];
    }
    pszTextBuf[iDst] = '\0';

    return pszTextBuf;
}

/************************************************************************/
/*                       OGRPGCommonLayerGetType()                      */
/************************************************************************/

CPLString OGRPGCommonLayerGetType(const OGRFieldDefn &oField,
                                  bool bPreservePrecision, bool bApproxOK)
{
    const char *pszFieldType = "";

    /* -------------------------------------------------------------------- */
    /*      Work out the PostgreSQL type.                                   */
    /* -------------------------------------------------------------------- */
    if (oField.GetType() == OFTInteger)
    {
        if (oField.GetSubType() == OFSTBoolean)
            pszFieldType = "BOOLEAN";
        else if (oField.GetSubType() == OFSTInt16)
            pszFieldType = "SMALLINT";
        else if (oField.GetWidth() > 0 && bPreservePrecision)
            pszFieldType = CPLSPrintf("NUMERIC(%d,0)", oField.GetWidth());
        else
            pszFieldType = "INTEGER";
    }
    else if (oField.GetType() == OFTInteger64)
    {
        if (oField.GetWidth() > 0 && bPreservePrecision)
            pszFieldType = CPLSPrintf("NUMERIC(%d,0)", oField.GetWidth());
        else
            pszFieldType = "INT8";
    }
    else if (oField.GetType() == OFTReal)
    {
        if (oField.GetSubType() == OFSTFloat32)
            pszFieldType = "REAL";
        else if (oField.GetWidth() > 0 && oField.GetPrecision() > 0 &&
                 bPreservePrecision)
            pszFieldType = CPLSPrintf("NUMERIC(%d,%d)", oField.GetWidth(),
                                      oField.GetPrecision());
        else
            pszFieldType = "FLOAT8";
    }
    else if (oField.GetType() == OFTString)
    {
        if (oField.GetSubType() == OFSTJSON)
            pszFieldType = CPLGetConfigOption("OGR_PG_JSON_TYPE", "JSON");
        else if (oField.GetSubType() == OFSTUUID)
            pszFieldType = CPLGetConfigOption("OGR_PG_UUID_TYPE", "UUID");
        else if (oField.GetWidth() > 0 && oField.GetWidth() < 10485760 &&
                 bPreservePrecision)
            pszFieldType = CPLSPrintf("VARCHAR(%d)", oField.GetWidth());
        else
            pszFieldType = CPLGetConfigOption("OGR_PG_STRING_TYPE", "VARCHAR");
    }
    else if (oField.GetType() == OFTIntegerList)
    {
        if (oField.GetSubType() == OFSTBoolean)
            pszFieldType = "BOOLEAN[]";
        else if (oField.GetSubType() == OFSTInt16)
            pszFieldType = "INT2[]";
        else
            pszFieldType = "INTEGER[]";
    }
    else if (oField.GetType() == OFTInteger64List)
    {
        pszFieldType = "INT8[]";
    }
    else if (oField.GetType() == OFTRealList)
    {
        if (oField.GetSubType() == OFSTFloat32)
            pszFieldType = "REAL[]";
        else
            pszFieldType = "FLOAT8[]";
    }
    else if (oField.GetType() == OFTStringList)
    {
        pszFieldType = "varchar[]";
    }
    else if (oField.GetType() == OFTDate)
    {
        pszFieldType = "date";
    }
    else if (oField.GetType() == OFTTime)
    {
        pszFieldType = "time";
    }
    else if (oField.GetType() == OFTDateTime)
    {
        pszFieldType = "timestamp with time zone";
    }
    else if (oField.GetType() == OFTBinary)
    {
        pszFieldType = "bytea";
    }
    else if (bApproxOK)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Can't create field %s with type %s on PostgreSQL layers.  "
                 "Creating as VARCHAR.",
                 oField.GetNameRef(),
                 OGRFieldDefn::GetFieldTypeName(oField.GetType()));
        pszFieldType = "VARCHAR";
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Can't create field %s with type %s on PostgreSQL layers.",
                 oField.GetNameRef(),
                 OGRFieldDefn::GetFieldTypeName(oField.GetType()));
    }

    return pszFieldType;
}

/************************************************************************/
/*                         OGRPGCommonLayerSetType()                    */
/************************************************************************/

bool OGRPGCommonLayerSetType(OGRFieldDefn &oField, const char *pszType,
                             const char *pszFormatType, int nWidth)
{
    if (EQUAL(pszType, "text"))
    {
        oField.SetType(OFTString);
    }
    else if (EQUAL(pszType, "_bpchar") || EQUAL(pszType, "_varchar") ||
             EQUAL(pszType, "_text"))
    {
        oField.SetType(OFTStringList);
    }
    else if (EQUAL(pszType, "bpchar") || EQUAL(pszType, "varchar"))
    {
        if (nWidth == -1)
        {
            if (STARTS_WITH_CI(pszFormatType, "character("))
                nWidth = atoi(pszFormatType + 10);
            else if (STARTS_WITH_CI(pszFormatType, "character varying("))
                nWidth = atoi(pszFormatType + 18);
            else
                nWidth = 0;
        }
        oField.SetType(OFTString);
        oField.SetWidth(nWidth);
    }
    else if (EQUAL(pszType, "bool"))
    {
        oField.SetType(OFTInteger);
        oField.SetSubType(OFSTBoolean);
        oField.SetWidth(1);
    }
    else if (EQUAL(pszType, "_numeric"))
    {
        if (EQUAL(pszFormatType, "numeric[]"))
            oField.SetType(OFTRealList);
        else
        {
            const char *pszPrecision = strstr(pszFormatType, ",");
            int nPrecision = 0;

            nWidth = atoi(pszFormatType + 8);
            if (pszPrecision != nullptr)
                nPrecision = atoi(pszPrecision + 1);

            if (nPrecision == 0)
            {
                if (nWidth >= 10)
                    oField.SetType(OFTInteger64List);
                else
                    oField.SetType(OFTIntegerList);
            }
            else
                oField.SetType(OFTRealList);

            oField.SetWidth(nWidth);
            oField.SetPrecision(nPrecision);
        }
    }
    else if (EQUAL(pszType, "numeric"))
    {
        if (EQUAL(pszFormatType, "numeric"))
            oField.SetType(OFTReal);
        else
        {
            const char *pszPrecision = strstr(pszFormatType, ",");
            int nPrecision = 0;

            nWidth = atoi(pszFormatType + 8);
            if (pszPrecision != nullptr)
                nPrecision = atoi(pszPrecision + 1);

            if (nPrecision == 0)
            {
                if (nWidth >= 10)
                    oField.SetType(OFTInteger64);
                else
                    oField.SetType(OFTInteger);
            }
            else
                oField.SetType(OFTReal);

            oField.SetWidth(nWidth);
            oField.SetPrecision(nPrecision);
        }
    }
    else if (EQUAL(pszFormatType, "integer[]"))
    {
        oField.SetType(OFTIntegerList);
    }
    else if (EQUAL(pszFormatType, "smallint[]"))
    {
        oField.SetType(OFTIntegerList);
        oField.SetSubType(OFSTInt16);
    }
    else if (EQUAL(pszFormatType, "boolean[]"))
    {
        oField.SetType(OFTIntegerList);
        oField.SetSubType(OFSTBoolean);
    }
    else if (EQUAL(pszFormatType, "float[]") || EQUAL(pszFormatType, "real[]"))
    {
        oField.SetType(OFTRealList);
        oField.SetSubType(OFSTFloat32);
    }
    else if (EQUAL(pszFormatType, "double precision[]"))
    {
        oField.SetType(OFTRealList);
    }
    else if (EQUAL(pszType, "int2"))
    {
        oField.SetType(OFTInteger);
        oField.SetSubType(OFSTInt16);
        oField.SetWidth(5);
    }
    else if (EQUAL(pszType, "int8"))
    {
        oField.SetType(OFTInteger64);
    }
    else if (EQUAL(pszFormatType, "bigint[]"))
    {
        oField.SetType(OFTInteger64List);
    }
    else if (STARTS_WITH_CI(pszType, "int"))
    {
        oField.SetType(OFTInteger);
    }
    else if (EQUAL(pszType, "float4"))
    {
        oField.SetType(OFTReal);
        oField.SetSubType(OFSTFloat32);
    }
    else if (STARTS_WITH_CI(pszType, "float") ||
             STARTS_WITH_CI(pszType, "double") || EQUAL(pszType, "real"))
    {
        oField.SetType(OFTReal);
    }
    else if (STARTS_WITH_CI(pszType, "timestamp"))
    {
        oField.SetType(OFTDateTime);
    }
    else if (STARTS_WITH_CI(pszType, "date"))
    {
        oField.SetType(OFTDate);
    }
    else if (STARTS_WITH_CI(pszType, "time"))
    {
        oField.SetType(OFTTime);
    }
    else if (EQUAL(pszType, "bytea"))
    {
        oField.SetType(OFTBinary);
    }
    else if (EQUAL(pszType, "json") || EQUAL(pszType, "jsonb"))
    {
        oField.SetType(OFTString);
        oField.SetSubType(OFSTJSON);
    }
    else if (EQUAL(pszType, "uuid"))
    {
        oField.SetType(OFTString);
        oField.SetSubType(OFSTUUID);
    }
    else
    {
        CPLDebug("PGCommon", "Field %s is of unknown format type %s (type=%s).",
                 oField.GetNameRef(), pszFormatType, pszType);
        return false;
    }
    return true;
}

/************************************************************************/
/*                  OGRPGCommonLayerNormalizeDefault()                  */
/************************************************************************/

void OGRPGCommonLayerNormalizeDefault(OGRFieldDefn *poFieldDefn,
                                      const char *pszDefault)
{
    if (pszDefault == nullptr)
        return;
    CPLString osDefault(pszDefault);
    size_t nPos = osDefault.find("::character varying");
    if (nPos != std::string::npos &&
        nPos + strlen("::character varying") == osDefault.size())
    {
        osDefault.resize(nPos);
    }
    else if ((nPos = osDefault.find("::text")) != std::string::npos &&
             nPos + strlen("::text") == osDefault.size())
    {
        osDefault.resize(nPos);
    }
    else if (strcmp(osDefault, "now()") == 0)
        osDefault = "CURRENT_TIMESTAMP";
    else if (strcmp(osDefault, "('now'::text)::date") == 0)
        osDefault = "CURRENT_DATE";
    else if (strcmp(osDefault, "('now'::text)::time with time zone") == 0)
        osDefault = "CURRENT_TIME";
    else
    {
        nPos = osDefault.find("::timestamp with time zone");
        if (poFieldDefn->GetType() == OFTDateTime && nPos != std::string::npos)
        {
            osDefault.resize(nPos);
            nPos = osDefault.find("'+");
            if (nPos != std::string::npos)
            {
                osDefault.resize(nPos);
                osDefault += "'";
            }
            int nYear = 0;
            int nMonth = 0;
            int nDay = 0;
            int nHour = 0;
            int nMinute = 0;
            float fSecond = 0.0f;
            if (sscanf(osDefault, "'%d-%d-%d %d:%d:%f'", &nYear, &nMonth, &nDay,
                       &nHour, &nMinute, &fSecond) == 6 ||
                sscanf(osDefault, "'%d-%d-%d %d:%d:%f+00'", &nYear, &nMonth,
                       &nDay, &nHour, &nMinute, &fSecond) == 6)
            {
                if (osDefault.find('.') == std::string::npos)
                    osDefault = CPLSPrintf("'%04d/%02d/%02d %02d:%02d:%02d'",
                                           nYear, nMonth, nDay, nHour, nMinute,
                                           (int)(fSecond + 0.5));
                else
                    osDefault =
                        CPLSPrintf("'%04d/%02d/%02d %02d:%02d:%06.3f'", nYear,
                                   nMonth, nDay, nHour, nMinute, fSecond);
            }
        }
    }
    poFieldDefn->SetDefault(osDefault);
}

/************************************************************************/
/*                     OGRPGCommonLayerGetPGDefault()                   */
/************************************************************************/

CPLString OGRPGCommonLayerGetPGDefault(OGRFieldDefn *poFieldDefn)
{
    CPLString osRet = poFieldDefn->GetDefault();
    int nYear = 0;
    int nMonth = 0;
    int nDay = 0;
    int nHour = 0;
    int nMinute = 0;
    float fSecond = 0.0f;
    if (sscanf(osRet, "'%d/%d/%d %d:%d:%f'", &nYear, &nMonth, &nDay, &nHour,
               &nMinute, &fSecond) == 6)
    {
        osRet.resize(osRet.size() - 1);
        osRet += "+00'::timestamp with time zone";
    }
    return osRet;
}

/************************************************************************/
/*                OGRPGCommonGenerateShortEnoughIdentifier()            */
/************************************************************************/

std::string OGRPGCommonGenerateShortEnoughIdentifier(const char *pszIdentifier)
{
    if (strlen(pszIdentifier) <= static_cast<size_t>(OGR_PG_NAMEDATALEN - 1))
        return pszIdentifier;

    constexpr int FIRST_8_CHARS_OF_MD5 = 8;
    std::string osRet(pszIdentifier,
                      OGR_PG_NAMEDATALEN - 1 - 1 - FIRST_8_CHARS_OF_MD5);
    osRet += '_';
    osRet += std::string(CPLMD5String(pszIdentifier), FIRST_8_CHARS_OF_MD5);
    return osRet;
}

/************************************************************************/
/*                 OGRPGCommonGenerateSpatialIndexName()                 */
/************************************************************************/

/** Generates the name of the spatial index on table pszTableName
 * using pszGeomFieldName, such that it fits in OGR_PG_NAMEDATALEN - 1 bytes.
 * The index of the geometry field may be used if the geometry field name
 * is too long.
 */
std::string OGRPGCommonGenerateSpatialIndexName(const char *pszTableName,
                                                const char *pszGeomFieldName,
                                                int nGeomFieldIdx)
{
    // Nominal case: use full table and geometry field name
    for (const char *pszSuffix : {"_geom_idx", "_idx"})
    {
        if (strlen(pszTableName) + 1 + strlen(pszGeomFieldName) +
                strlen(pszSuffix) <=
            static_cast<size_t>(OGR_PG_NAMEDATALEN - 1))
        {
            std::string osRet(pszTableName);
            osRet += '_';
            osRet += pszGeomFieldName;
            osRet += pszSuffix;
            return osRet;
        }
    }

    // Slightly degraded case: use table name and geometry field index
    const std::string osGeomFieldIdx(CPLSPrintf("%d", nGeomFieldIdx));
    if (strlen(pszTableName) + 1 + osGeomFieldIdx.size() +
            strlen("_geom_idx") <=
        static_cast<size_t>(OGR_PG_NAMEDATALEN - 1))
    {
        std::string osRet(pszTableName);
        osRet += '_';
        osRet += osGeomFieldIdx;
        osRet += "_geom_idx";
        return osRet;
    }

    // Fallback case: use first characters of table name,
    // first 8 chars of its MD5 and then the geometry field index.
    constexpr int FIRST_8_CHARS_OF_MD5 = 8;
    std::string osSuffix("_");
    osSuffix += std::string(CPLMD5String(pszTableName), FIRST_8_CHARS_OF_MD5);
    osSuffix += '_';
    osSuffix += osGeomFieldIdx;
    osSuffix += "_geom_idx";
    std::string osRet(pszTableName, OGR_PG_NAMEDATALEN - 1 - osSuffix.size());
    osRet += osSuffix;
    return osRet;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRErr OGRPGDumpLayer::CreateField(const OGRFieldDefn *poFieldIn, int bApproxOK)
{
    if (m_poFeatureDefn->GetFieldCount() +
            m_poFeatureDefn->GetGeomFieldCount() ==
        1600)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Maximum number of fields supported is 1600.");
        return OGRERR_FAILURE;
    }

    CPLString osFieldType;
    OGRFieldDefn oField(poFieldIn);

    // Can be set to NO to test ogr2ogr default behavior
    const bool bAllowCreationOfFieldWithFIDName =
        CPLTestBool(CPLGetConfigOption(
            "PGDUMP_DEBUG_ALLOW_CREATION_FIELD_WITH_FID_NAME", "YES"));

    if (bAllowCreationOfFieldWithFIDName && m_pszFIDColumn != nullptr &&
        EQUAL(oField.GetNameRef(), m_pszFIDColumn) &&
        oField.GetType() != OFTInteger && oField.GetType() != OFTInteger64)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Wrong field type for %s",
                 oField.GetNameRef());
        return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /*      Do we want to "launder" the column names into Postgres          */
    /*      friendly format?                                                */
    /* -------------------------------------------------------------------- */
    if (m_bLaunderColumnNames)
    {
        char *pszSafeName = OGRPGCommonLaunderName(oField.GetNameRef(),
                                                   "PGDump", m_bUTF8ToASCII);

        oField.SetName(pszSafeName);
        CPLFree(pszSafeName);

        if (EQUAL(oField.GetNameRef(), "oid"))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Renaming field 'oid' to 'oid_' to avoid conflict with "
                     "internal oid field.");
            oField.SetName("oid_");
        }
    }

    const char *pszOverrideType =
        m_apszOverrideColumnTypes.FetchNameValue(oField.GetNameRef());
    if (pszOverrideType != nullptr)
    {
        osFieldType = pszOverrideType;
    }
    else
    {
        osFieldType = OGRPGCommonLayerGetType(oField, m_bPreservePrecision,
                                              CPL_TO_BOOL(bApproxOK));
        if (osFieldType.empty())
            return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /*      Create the new field.                                           */
    /* -------------------------------------------------------------------- */
    CPLString osCommand;
    osCommand.Printf("ALTER TABLE %s ADD COLUMN %s %s", m_pszSqlTableName,
                     OGRPGDumpEscapeColumnName(oField.GetNameRef()).c_str(),
                     osFieldType.c_str());
    if (!oField.IsNullable())
        osCommand += " NOT NULL";
    if (oField.IsUnique())
        osCommand += " UNIQUE";
    if (oField.GetDefault() != nullptr && !oField.IsDefaultDriverSpecific())
    {
        osCommand += " DEFAULT ";
        osCommand += OGRPGCommonLayerGetPGDefault(&oField);
    }

    m_poFeatureDefn->AddFieldDefn(&oField);

    if (bAllowCreationOfFieldWithFIDName && m_pszFIDColumn != nullptr &&
        EQUAL(oField.GetNameRef(), m_pszFIDColumn))
    {
        m_iFIDAsRegularColumnIndex = m_poFeatureDefn->GetFieldCount() - 1;
    }
    else if (m_bCreateTable)
    {
        const auto Log = [this](const std::string &osSQL)
        {
            if (m_bGeomColumnPositionImmediate)
                m_poDS->Log(osSQL.c_str());
            else
                m_aosDeferrentNonGeomFieldCreationCommands.push_back(osSQL);
        };

        Log(osCommand);

        if (!oField.GetComment().empty())
        {
            std::string osCommentON;
            osCommentON = "COMMENT ON COLUMN ";
            osCommentON += m_pszSqlTableName;
            osCommentON += '.';
            osCommentON += OGRPGDumpEscapeColumnName(oField.GetNameRef());
            osCommentON += " IS ";
            osCommentON += OGRPGDumpEscapeString(oField.GetComment().c_str());
            Log(osCommentON);
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRPGDumpLayer::CreateGeomField(const OGRGeomFieldDefn *poGeomFieldIn,
                                       int /* bApproxOK */)
{
    if (m_poFeatureDefn->GetFieldCount() +
            m_poFeatureDefn->GetGeomFieldCount() ==
        1600)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Maximum number of fields supported is 1600.");
        return OGRERR_FAILURE;
    }

    OGRwkbGeometryType eType = poGeomFieldIn->GetType();
    if (eType == wkbNone)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create geometry field of type wkbNone");
        return OGRERR_FAILURE;
    }

    // Check if GEOMETRY_NAME layer creation option was set, but no initial
    // column was created in ICreateLayer()
    const CPLString osGeomFieldName =
        !m_osFirstGeometryFieldName.empty()
            ? m_osFirstGeometryFieldName
            : CPLString(poGeomFieldIn->GetNameRef());

    m_osFirstGeometryFieldName = "";  // reset for potential next geom columns

    OGRGeomFieldDefn oTmpGeomFieldDefn(poGeomFieldIn);
    oTmpGeomFieldDefn.SetName(osGeomFieldName);

    CPLString osCommand;
    auto poGeomField =
        std::make_unique<OGRPGDumpGeomFieldDefn>(&oTmpGeomFieldDefn);

    /* -------------------------------------------------------------------- */
    /*      Do we want to "launder" the column names into Postgres          */
    /*      friendly format?                                                */
    /* -------------------------------------------------------------------- */
    if (m_bLaunderColumnNames)
    {
        char *pszSafeName = OGRPGCommonLaunderName(poGeomField->GetNameRef(),
                                                   "PGDump", m_bUTF8ToASCII);

        poGeomField->SetName(pszSafeName);
        CPLFree(pszSafeName);
    }

    const OGRSpatialReference *poSRS = poGeomField->GetSpatialRef();
    int nSRSId = m_nUnknownSRSId;
    if (m_nForcedSRSId != -2)
        nSRSId = m_nForcedSRSId;
    else if (poSRS != nullptr)
    {
        const char *pszAuthorityName = poSRS->GetAuthorityName(nullptr);
        if (pszAuthorityName != nullptr && EQUAL(pszAuthorityName, "EPSG"))
        {
            /* Assume the EPSG Id is the SRS ID. Might be a wrong guess ! */
            nSRSId = atoi(poSRS->GetAuthorityCode(nullptr));
        }
        else
        {
            const char *pszGeogCSName = poSRS->GetAttrValue("GEOGCS");
            if (pszGeogCSName != nullptr &&
                EQUAL(pszGeogCSName, "GCS_WGS_1984"))
                nSRSId = 4326;
        }
    }

    poGeomField->m_nSRSId = nSRSId;

    int nGeometryTypeFlags = 0;
    if (OGR_GT_HasZ((OGRwkbGeometryType)eType))
        nGeometryTypeFlags |= OGRGeometry::OGR_G_3D;
    if (OGR_GT_HasM((OGRwkbGeometryType)eType))
        nGeometryTypeFlags |= OGRGeometry::OGR_G_MEASURED;
    if (m_nForcedGeometryTypeFlags >= 0)
    {
        nGeometryTypeFlags = m_nForcedGeometryTypeFlags;
        eType = OGR_GT_SetModifier(
            eType, nGeometryTypeFlags & OGRGeometry::OGR_G_3D,
            nGeometryTypeFlags & OGRGeometry::OGR_G_MEASURED);
    }
    poGeomField->SetType(eType);
    poGeomField->m_nGeometryTypeFlags = nGeometryTypeFlags;

    /* -------------------------------------------------------------------- */
    /*      Create the new field.                                           */
    /* -------------------------------------------------------------------- */
    if (m_bCreateTable)
    {
        const char *suffix = "";
        int dim = 2;
        if ((poGeomField->m_nGeometryTypeFlags & OGRGeometry::OGR_G_3D) &&
            (poGeomField->m_nGeometryTypeFlags & OGRGeometry::OGR_G_MEASURED))
            dim = 4;
        else if (poGeomField->m_nGeometryTypeFlags &
                 OGRGeometry::OGR_G_MEASURED)
        {
            if (wkbFlatten(poGeomField->GetType()) != wkbUnknown)
                suffix = "M";
            dim = 3;
        }
        else if (poGeomField->m_nGeometryTypeFlags & OGRGeometry::OGR_G_3D)
            dim = 3;

        const char *pszGeometryType = OGRToOGCGeomType(poGeomField->GetType());
        osCommand.Printf(
            "SELECT AddGeometryColumn(%s,%s,%s,%d,'%s%s',%d)",
            OGRPGDumpEscapeString(m_pszSchemaName).c_str(),
            OGRPGDumpEscapeString(m_poFeatureDefn->GetName()).c_str(),
            OGRPGDumpEscapeString(poGeomField->GetNameRef()).c_str(), nSRSId,
            pszGeometryType, suffix, dim);

        if (m_bGeomColumnPositionImmediate)
            m_poDS->Log(osCommand);
        else
            m_aosDeferredGeomFieldCreationCommands.push_back(osCommand);

        if (!poGeomField->IsNullable())
        {
            osCommand.Printf(
                "ALTER TABLE %s ALTER COLUMN %s SET NOT NULL",
                OGRPGDumpEscapeColumnName(m_poFeatureDefn->GetName()).c_str(),
                OGRPGDumpEscapeColumnName(poGeomField->GetNameRef()).c_str());

            if (m_bGeomColumnPositionImmediate)
                m_poDS->Log(osCommand);
            else
                m_aosDeferredGeomFieldCreationCommands.push_back(osCommand);
        }

        if (m_bCreateSpatialIndexFlag)
        {
            const std::string osIndexName(OGRPGCommonGenerateSpatialIndexName(
                GetName(), poGeomField->GetNameRef(),
                m_poFeatureDefn->GetGeomFieldCount()));

            osCommand.Printf(
                "CREATE INDEX %s ON %s USING %s (%s)",
                OGRPGDumpEscapeColumnName(osIndexName.c_str()).c_str(),
                m_pszSqlTableName, m_osSpatialIndexType.c_str(),
                OGRPGDumpEscapeColumnName(poGeomField->GetNameRef()).c_str());

            m_aosSpatialIndexCreationCommands.push_back(osCommand);
        }
    }

    m_poFeatureDefn->AddGeomFieldDefn(std::move(poGeomField));

    return OGRERR_NONE;
}

/************************************************************************/
/*                        SetOverrideColumnTypes()                      */
/************************************************************************/

void OGRPGDumpLayer::SetOverrideColumnTypes(const char *pszOverrideColumnTypes)
{
    if (pszOverrideColumnTypes == nullptr)
        return;

    const char *pszIter = pszOverrideColumnTypes;
    std::string osCur;
    while (*pszIter != '\0')
    {
        if (*pszIter == '(')
        {
            /* Ignore commas inside ( ) pair */
            while (*pszIter != '\0')
            {
                if (*pszIter == ')')
                {
                    osCur += *pszIter;
                    pszIter++;
                    break;
                }
                osCur += *pszIter;
                pszIter++;
            }
            if (*pszIter == '\0')
                break;
        }

        if (*pszIter == ',')
        {
            m_apszOverrideColumnTypes.AddString(osCur.c_str());
            osCur.clear();
        }
        else
            osCur += *pszIter;
        pszIter++;
    }
    if (!osCur.empty())
        m_apszOverrideColumnTypes.AddString(osCur.c_str());
}

/************************************************************************/
/*                              SetMetadata()                           */
/************************************************************************/

CPLErr OGRPGDumpLayer::SetMetadata(char **papszMD, const char *pszDomain)
{
    OGRLayer::SetMetadata(papszMD, pszDomain);
    if (!m_osForcedDescription.empty() &&
        (pszDomain == nullptr || EQUAL(pszDomain, "")))
    {
        OGRLayer::SetMetadataItem("DESCRIPTION", m_osForcedDescription);
    }

    if ((pszDomain == nullptr || EQUAL(pszDomain, "")) &&
        m_osForcedDescription.empty())
    {
        const char *l_pszDescription = OGRLayer::GetMetadataItem("DESCRIPTION");
        CPLString osCommand;

        osCommand.Printf("COMMENT ON TABLE %s IS %s", m_pszSqlTableName,
                         l_pszDescription && l_pszDescription[0] != '\0'
                             ? OGRPGDumpEscapeString(l_pszDescription).c_str()
                             : "NULL");
        m_poDS->Log(osCommand);
    }

    return CE_None;
}

/************************************************************************/
/*                            SetMetadataItem()                         */
/************************************************************************/

CPLErr OGRPGDumpLayer::SetMetadataItem(const char *pszName,
                                       const char *pszValue,
                                       const char *pszDomain)
{
    if ((pszDomain == nullptr || EQUAL(pszDomain, "")) && pszName != nullptr &&
        EQUAL(pszName, "DESCRIPTION") && !m_osForcedDescription.empty())
    {
        return CE_None;
    }
    OGRLayer::SetMetadataItem(pszName, pszValue, pszDomain);
    if ((pszDomain == nullptr || EQUAL(pszDomain, "")) && pszName != nullptr &&
        EQUAL(pszName, "DESCRIPTION"))
    {
        SetMetadata(GetMetadata());
    }
    return CE_None;
}

/************************************************************************/
/*                      SetForcedDescription()                          */
/************************************************************************/

void OGRPGDumpLayer::SetForcedDescription(const char *pszDescriptionIn)
{
    m_osForcedDescription = pszDescriptionIn;
    OGRLayer::SetMetadataItem("DESCRIPTION", m_osForcedDescription);

    if (pszDescriptionIn[0] != '\0')
    {
        CPLString osCommand;
        osCommand.Printf("COMMENT ON TABLE %s IS %s", m_pszSqlTableName,
                         OGRPGDumpEscapeString(pszDescriptionIn).c_str());
        m_poDS->Log(osCommand);
    }
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

GDALDataset *OGRPGDumpLayer::GetDataset()
{
    return m_poDS;
}
