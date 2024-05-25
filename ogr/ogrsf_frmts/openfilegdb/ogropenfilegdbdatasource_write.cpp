/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_openfilegdb.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "filegdbtable.h"
#include "gdal.h"
#include "ogr_core.h"
#include "ogrsf_frmts.h"

#include "filegdb_fielddomain.h"
#include "filegdb_relationship.h"

/***********************************************************************/
/*                    GetExistingSpatialRef()                          */
/***********************************************************************/

bool OGROpenFileGDBDataSource::GetExistingSpatialRef(
    const std::string &osWKT, double dfXOrigin, double dfYOrigin,
    double dfXYScale, double dfZOrigin, double dfZScale, double dfMOrigin,
    double dfMScale, double dfXYTolerance, double dfZTolerance,
    double dfMTolerance)
{
    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBSpatialRefsFilename.c_str(), false))
        return false;

    FETCH_FIELD_IDX(iSRTEXT, "SRTEXT", FGFT_STRING);
    FETCH_FIELD_IDX(iFalseX, "FalseX", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iFalseY, "FalseY", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iXYUnits, "XYUnits", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iFalseZ, "FalseZ", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iZUnits, "ZUnits", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iFalseM, "FalseM", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iMUnits, "MUnits", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iXYTolerance, "XYTolerance", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iZTolerance, "ZTolerance", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iMTolerance, "MTolerance", FGFT_FLOAT64);

    int iCurFeat = 0;
    while (iCurFeat < oTable.GetTotalRecordCount())
    {
        iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
        if (iCurFeat < 0)
            break;
        iCurFeat++;
        const auto psSRTEXT = oTable.GetFieldValue(iSRTEXT);
        if (psSRTEXT && psSRTEXT->String == osWKT)
        {
            const auto fetchRealVal = [&oTable](int idx, double dfExpected)
            {
                const auto psVal = oTable.GetFieldValue(idx);
                return psVal && psVal->Real == dfExpected;
            };
            if (fetchRealVal(iFalseX, dfXOrigin) &&
                fetchRealVal(iFalseY, dfYOrigin) &&
                fetchRealVal(iXYUnits, dfXYScale) &&
                fetchRealVal(iFalseZ, dfZOrigin) &&
                fetchRealVal(iZUnits, dfZScale) &&
                fetchRealVal(iFalseM, dfMOrigin) &&
                fetchRealVal(iMUnits, dfMScale) &&
                fetchRealVal(iXYTolerance, dfXYTolerance) &&
                fetchRealVal(iZTolerance, dfZTolerance) &&
                fetchRealVal(iMTolerance, dfMTolerance))
            {
                return true;
            }
        }
    }
    return false;
}

/***********************************************************************/
/*                       AddNewSpatialRef()                            */
/***********************************************************************/

bool OGROpenFileGDBDataSource::AddNewSpatialRef(
    const std::string &osWKT, double dfXOrigin, double dfYOrigin,
    double dfXYScale, double dfZOrigin, double dfZScale, double dfMOrigin,
    double dfMScale, double dfXYTolerance, double dfZTolerance,
    double dfMTolerance)
{
    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBSpatialRefsFilename.c_str(), true))
        return false;

    FETCH_FIELD_IDX(iSRTEXT, "SRTEXT", FGFT_STRING);
    FETCH_FIELD_IDX(iFalseX, "FalseX", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iFalseY, "FalseY", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iXYUnits, "XYUnits", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iFalseZ, "FalseZ", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iZUnits, "ZUnits", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iFalseM, "FalseM", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iMUnits, "MUnits", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iXYTolerance, "XYTolerance", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iZTolerance, "ZTolerance", FGFT_FLOAT64);
    FETCH_FIELD_IDX(iMTolerance, "MTolerance", FGFT_FLOAT64);

    std::vector<OGRField> fields(oTable.GetFieldCount(),
                                 FileGDBField::UNSET_FIELD);
    fields[iSRTEXT].String = const_cast<char *>(osWKT.c_str());
    fields[iFalseX].Real = dfXOrigin;
    fields[iFalseY].Real = dfYOrigin;
    fields[iXYUnits].Real = dfXYScale;
    fields[iFalseZ].Real = dfZOrigin;
    fields[iZUnits].Real = dfZScale;
    fields[iFalseM].Real = dfMOrigin;
    fields[iMUnits].Real = dfMScale;
    fields[iXYTolerance].Real = dfXYTolerance;
    fields[iZTolerance].Real = dfZTolerance;
    fields[iMTolerance].Real = dfMTolerance;

    return oTable.CreateFeature(fields, nullptr) && oTable.Sync();
}

/***********************************************************************/
/*                    RegisterLayerInSystemCatalog()                   */
/***********************************************************************/

bool OGROpenFileGDBDataSource::RegisterLayerInSystemCatalog(
    const std::string &osLayerName)
{
    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBSystemCatalogFilename.c_str(), true))
        return false;

    FETCH_FIELD_IDX(iName, "Name", FGFT_STRING);
    FETCH_FIELD_IDX(iFileFormat, "FileFormat", FGFT_INT32);

    std::vector<OGRField> fields(oTable.GetFieldCount(),
                                 FileGDBField::UNSET_FIELD);
    fields[iName].String = const_cast<char *>(osLayerName.c_str());
    fields[iFileFormat].Integer = 0;
    return oTable.CreateFeature(fields, nullptr) && oTable.Sync();
}

/***********************************************************************/
/*                    RegisterInItemRelationships()                    */
/***********************************************************************/

bool OGROpenFileGDBDataSource::RegisterInItemRelationships(
    const std::string &osOriginGUID, const std::string &osDestGUID,
    const std::string &osTypeGUID)
{
    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBItemRelationshipsFilename.c_str(), true))
        return false;

    FETCH_FIELD_IDX(iUUID, "UUID", FGFT_GLOBALID);
    FETCH_FIELD_IDX(iOriginID, "OriginID", FGFT_GUID);
    FETCH_FIELD_IDX(iDestID, "DestID", FGFT_GUID);
    FETCH_FIELD_IDX(iType, "Type", FGFT_GUID);
    FETCH_FIELD_IDX(iProperties, "Properties", FGFT_INT32);

    std::vector<OGRField> fields(oTable.GetFieldCount(),
                                 FileGDBField::UNSET_FIELD);
    const std::string osGUID = OFGDBGenerateUUID();
    fields[iUUID].String = const_cast<char *>(osGUID.c_str());
    fields[iOriginID].String = const_cast<char *>(osOriginGUID.c_str());
    fields[iDestID].String = const_cast<char *>(osDestGUID.c_str());
    fields[iType].String = const_cast<char *>(osTypeGUID.c_str());
    fields[iProperties].Integer = 1;
    return oTable.CreateFeature(fields, nullptr) && oTable.Sync();
}

/***********************************************************************/
/*              RegisterRelationshipInItemRelationships()              */
/***********************************************************************/

bool OGROpenFileGDBDataSource::RegisterRelationshipInItemRelationships(
    const std::string &osRelationshipGUID, const std::string &osOriginGUID,
    const std::string &osDestGUID)
{
    // Relationships to register:
    // 1. Origin table -> new relationship as DatasetsRelatedThrough
    // 2. Destination table -> new relationship as DatasetsRelatedThrough
    // 3. Root dataset -> new relationship as DatasetInFolder
    if (!RegisterInItemRelationships(osOriginGUID, osRelationshipGUID,
                                     pszDatasetsRelatedThroughUUID))
        return false;
    if (!RegisterInItemRelationships(osDestGUID, osRelationshipGUID,
                                     pszDatasetsRelatedThroughUUID))
        return false;
    if (!RegisterInItemRelationships(m_osRootGUID, osRelationshipGUID,
                                     pszDatasetInFolderUUID))
        return false;

    return true;
}

/***********************************************************************/
/*              RemoveRelationshipFromItemRelationships()              */
/***********************************************************************/

bool OGROpenFileGDBDataSource::RemoveRelationshipFromItemRelationships(
    const std::string &osRelationshipGUID)
{
    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBItemRelationshipsFilename.c_str(), true))
        return false;

    // while we've only found item relationships with the relationship UUID in
    // the DestID field, let's be super-careful and also check against the
    // OriginID UUID, just in case there's some previously unencountered
    // situations where a relationship UUID is placed in OriginID
    FETCH_FIELD_IDX_WITH_RET(iOriginID, "OriginID", FGFT_GUID, false);
    FETCH_FIELD_IDX_WITH_RET(iDestID, "DestID", FGFT_GUID, false);

    for (int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount(); ++iCurFeat)
    {
        iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
        if (iCurFeat < 0)
            break;

        const auto psOriginID = oTable.GetFieldValue(iOriginID);
        if (psOriginID && psOriginID->String == osRelationshipGUID)
        {
            oTable.DeleteFeature(iCurFeat + 1);
        }
        else
        {
            const auto psDestID = oTable.GetFieldValue(iDestID);
            if (psDestID && psDestID->String == osRelationshipGUID)
            {
                oTable.DeleteFeature(iCurFeat + 1);
            }
        }
    }

    return true;
}

/***********************************************************************/
/*                      LinkDomainToTable()                            */
/***********************************************************************/

bool OGROpenFileGDBDataSource::LinkDomainToTable(
    const std::string &osDomainName, const std::string &osLayerGUID)
{
    std::string osDomainUUID;
    if (!FindUUIDFromName(osDomainName, osDomainUUID))
        return false;

    // Check if the domain is already linked to this table
    {
        FileGDBTable oTable;
        if (!oTable.Open(m_osGDBItemRelationshipsFilename.c_str(), false))
            return false;

        FETCH_FIELD_IDX(iOriginID, "OriginID", FGFT_GUID);
        FETCH_FIELD_IDX(iDestID, "DestID", FGFT_GUID);

        for (int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount();
             ++iCurFeat)
        {
            iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
            if (iCurFeat < 0)
                break;

            const auto psOriginID = oTable.GetFieldValue(iOriginID);
            if (psOriginID && EQUAL(psOriginID->String, osLayerGUID.c_str()))
            {
                const auto psDestID = oTable.GetFieldValue(iDestID);
                if (psDestID && EQUAL(psDestID->String, osDomainUUID.c_str()))
                {
                    return true;
                }
            }
        }
    }

    return RegisterInItemRelationships(osLayerGUID, osDomainUUID,
                                       pszDomainInDatasetUUID);
}

/***********************************************************************/
/*                      UnlinkDomainToTable()                          */
/***********************************************************************/

bool OGROpenFileGDBDataSource::UnlinkDomainToTable(
    const std::string &osDomainName, const std::string &osLayerGUID)
{
    std::string osDomainUUID;
    if (!FindUUIDFromName(osDomainName, osDomainUUID))
        return false;

    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBItemRelationshipsFilename.c_str(), true))
        return false;

    FETCH_FIELD_IDX(iOriginID, "OriginID", FGFT_GUID);
    FETCH_FIELD_IDX(iDestID, "DestID", FGFT_GUID);

    for (int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount(); ++iCurFeat)
    {
        iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
        if (iCurFeat < 0)
            break;

        const auto psOriginID = oTable.GetFieldValue(iOriginID);
        if (psOriginID && EQUAL(psOriginID->String, osLayerGUID.c_str()))
        {
            const auto psDestID = oTable.GetFieldValue(iDestID);
            if (psDestID && EQUAL(psDestID->String, osDomainUUID.c_str()))
            {
                return oTable.DeleteFeature(iCurFeat + 1) && oTable.Sync();
            }
        }
    }

    return true;
}

/***********************************************************************/
/*                    UpdateXMLDefinition()                            */
/***********************************************************************/

bool OGROpenFileGDBDataSource::UpdateXMLDefinition(
    const std::string &osLayerName, const char *pszXMLDefinition)
{
    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBItemsFilename.c_str(), true))
        return false;

    FETCH_FIELD_IDX(iName, "Name", FGFT_STRING);
    FETCH_FIELD_IDX(iDefinition, "Definition", FGFT_XML);

    for (int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount(); ++iCurFeat)
    {
        iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
        if (iCurFeat < 0)
            break;
        const auto psName = oTable.GetFieldValue(iName);
        if (psName && psName->String == osLayerName)
        {
            auto asFields = oTable.GetAllFieldValues();
            if (!OGR_RawField_IsNull(&asFields[iDefinition]) &&
                !OGR_RawField_IsUnset(&asFields[iDefinition]))
            {
                CPLFree(asFields[iDefinition].String);
            }
            asFields[iDefinition].String = CPLStrdup(pszXMLDefinition);
            bool bRet = oTable.UpdateFeature(iCurFeat + 1, asFields, nullptr);
            oTable.FreeAllFieldValues(asFields);
            return bRet;
        }
    }

    CPLError(CE_Failure, CPLE_AppDefined,
             "Cannot find record for Name=%s in GDB_Items table",
             osLayerName.c_str());
    return false;
}

/***********************************************************************/
/*                    FindUUIDFromName()                               */
/***********************************************************************/

bool OGROpenFileGDBDataSource::FindUUIDFromName(const std::string &osName,
                                                std::string &osUUIDOut)
{
    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBItemsFilename.c_str(), true))
        return false;

    FETCH_FIELD_IDX(iUUID, "UUID", FGFT_GLOBALID);
    FETCH_FIELD_IDX(iName, "Name", FGFT_STRING);

    for (int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount(); ++iCurFeat)
    {
        iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
        if (iCurFeat < 0)
            break;
        const auto psName = oTable.GetFieldValue(iName);
        if (psName && psName->String == osName)
        {
            const auto psUUID = oTable.GetFieldValue(iUUID);
            if (psUUID)
            {
                osUUIDOut = psUUID->String;
                return true;
            }
        }
    }

    return false;
}

/***********************************************************************/
/*                  RegisterFeatureDatasetInItems()                    */
/***********************************************************************/

bool OGROpenFileGDBDataSource::RegisterFeatureDatasetInItems(
    const std::string &osFeatureDatasetGUID, const std::string &osName,
    const char *pszXMLDefinition)
{
    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBItemsFilename.c_str(), true))
        return false;

    FETCH_FIELD_IDX(iUUID, "UUID", FGFT_GLOBALID);
    FETCH_FIELD_IDX(iType, "Type", FGFT_GUID);
    FETCH_FIELD_IDX(iName, "Name", FGFT_STRING);
    FETCH_FIELD_IDX(iPhysicalName, "PhysicalName", FGFT_STRING);
    FETCH_FIELD_IDX(iPath, "Path", FGFT_STRING);
    FETCH_FIELD_IDX(iURL, "URL", FGFT_STRING);
    FETCH_FIELD_IDX(iDefinition, "Definition", FGFT_XML);
    FETCH_FIELD_IDX(iProperties, "Properties", FGFT_INT32);

    std::vector<OGRField> fields(oTable.GetFieldCount(),
                                 FileGDBField::UNSET_FIELD);
    fields[iUUID].String = const_cast<char *>(osFeatureDatasetGUID.c_str());
    fields[iType].String = const_cast<char *>(pszFeatureDatasetTypeUUID);
    fields[iName].String = const_cast<char *>(osName.c_str());
    CPLString osUCName(osName);
    osUCName.toupper();
    fields[iPhysicalName].String = const_cast<char *>(osUCName.c_str());
    std::string osPath("\\");
    osPath += osName;
    fields[iPath].String = const_cast<char *>(osPath.c_str());
    fields[iURL].String = const_cast<char *>("");
    fields[iDefinition].String = const_cast<char *>(pszXMLDefinition);
    fields[iProperties].Integer = 1;
    return oTable.CreateFeature(fields, nullptr) && oTable.Sync();
}

/***********************************************************************/
/*                  RegisterFeatureClassInItems()                      */
/***********************************************************************/

bool OGROpenFileGDBDataSource::RegisterFeatureClassInItems(
    const std::string &osLayerGUID, const std::string &osLayerName,
    const std::string &osPath, const FileGDBTable *poLyrTable,
    const char *pszXMLDefinition, const char *pszDocumentation)
{
    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBItemsFilename.c_str(), true))
        return false;

    FETCH_FIELD_IDX(iUUID, "UUID", FGFT_GLOBALID);
    FETCH_FIELD_IDX(iType, "Type", FGFT_GUID);
    FETCH_FIELD_IDX(iName, "Name", FGFT_STRING);
    FETCH_FIELD_IDX(iPhysicalName, "PhysicalName", FGFT_STRING);
    FETCH_FIELD_IDX(iPath, "Path", FGFT_STRING);
    FETCH_FIELD_IDX(iDatasetSubtype1, "DatasetSubtype1", FGFT_INT32);
    FETCH_FIELD_IDX(iDatasetSubtype2, "DatasetSubtype2", FGFT_INT32);
    FETCH_FIELD_IDX(iDatasetInfo1, "DatasetInfo1", FGFT_STRING);
    FETCH_FIELD_IDX(iURL, "URL", FGFT_STRING);
    FETCH_FIELD_IDX(iDefinition, "Definition", FGFT_XML);
    FETCH_FIELD_IDX(iDocumentation, "Documentation", FGFT_XML);
    FETCH_FIELD_IDX(iProperties, "Properties", FGFT_INT32);

    std::vector<OGRField> fields(oTable.GetFieldCount(),
                                 FileGDBField::UNSET_FIELD);
    fields[iUUID].String = const_cast<char *>(osLayerGUID.c_str());
    fields[iType].String = const_cast<char *>(pszFeatureClassTypeUUID);
    fields[iName].String = const_cast<char *>(osLayerName.c_str());
    CPLString osUCName(osLayerName);
    osUCName.toupper();
    fields[iPhysicalName].String = const_cast<char *>(osUCName.c_str());
    fields[iPath].String = const_cast<char *>(osPath.c_str());
    fields[iDatasetSubtype1].Integer = 1;
    fields[iDatasetSubtype2].Integer = poLyrTable->GetGeometryType();
    const auto poGeomFieldDefn = poLyrTable->GetGeomField();
    if (poGeomFieldDefn)  // should always be true
        fields[iDatasetInfo1].String =
            const_cast<char *>(poGeomFieldDefn->GetName().c_str());
    fields[iURL].String = const_cast<char *>("");
    fields[iDefinition].String = const_cast<char *>(pszXMLDefinition);
    if (pszDocumentation && pszDocumentation[0])
        fields[iDocumentation].String = const_cast<char *>(pszDocumentation);
    fields[iProperties].Integer = 1;
    return oTable.CreateFeature(fields, nullptr) && oTable.Sync();
}

/***********************************************************************/
/*                  RegisterASpatialTableInItems()                     */
/***********************************************************************/

bool OGROpenFileGDBDataSource::RegisterASpatialTableInItems(
    const std::string &osLayerGUID, const std::string &osLayerName,
    const std::string &osPath, const char *pszXMLDefinition,
    const char *pszDocumentation)
{
    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBItemsFilename.c_str(), true))
        return false;

    FETCH_FIELD_IDX(iUUID, "UUID", FGFT_GLOBALID);
    FETCH_FIELD_IDX(iType, "Type", FGFT_GUID);
    FETCH_FIELD_IDX(iName, "Name", FGFT_STRING);
    FETCH_FIELD_IDX(iPhysicalName, "PhysicalName", FGFT_STRING);
    FETCH_FIELD_IDX(iPath, "Path", FGFT_STRING);
    FETCH_FIELD_IDX(iURL, "URL", FGFT_STRING);
    FETCH_FIELD_IDX(iDefinition, "Definition", FGFT_XML);
    FETCH_FIELD_IDX(iDocumentation, "Documentation", FGFT_XML);
    FETCH_FIELD_IDX(iProperties, "Properties", FGFT_INT32);

    std::vector<OGRField> fields(oTable.GetFieldCount(),
                                 FileGDBField::UNSET_FIELD);
    fields[iUUID].String = const_cast<char *>(osLayerGUID.c_str());
    fields[iType].String = const_cast<char *>(pszTableTypeUUID);
    fields[iName].String = const_cast<char *>(osLayerName.c_str());
    CPLString osUCName(osLayerName);
    osUCName.toupper();
    fields[iPhysicalName].String = const_cast<char *>(osUCName.c_str());
    fields[iPath].String = const_cast<char *>(osPath.c_str());
    fields[iURL].String = const_cast<char *>("");
    fields[iDefinition].String = const_cast<char *>(pszXMLDefinition);
    if (pszDocumentation && pszDocumentation[0])
        fields[iDocumentation].String = const_cast<char *>(pszDocumentation);
    fields[iProperties].Integer = 1;
    return oTable.CreateFeature(fields, nullptr) && oTable.Sync();
}

/***********************************************************************/
/*                       CreateGDBSystemCatalog()                      */
/***********************************************************************/

bool OGROpenFileGDBDataSource::CreateGDBSystemCatalog()
{
    // Write GDB_SystemCatalog file
    m_osGDBSystemCatalogFilename =
        CPLFormFilename(m_osDirName.c_str(), "a00000001.gdbtable", nullptr);
    FileGDBTable oTable;
    if (!oTable.Create(m_osGDBSystemCatalogFilename.c_str(), 4, FGTGT_NONE,
                       false, false) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "ID", std::string(), FGFT_OBJECTID,
            /* bNullable = */ false,
            /* bRequired = */ true,
            /* bEditable = */ false, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Name", std::string(), FGFT_STRING,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 160, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "FileFormat", std::string(), FGFT_INT32,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)))
    {
        return false;
    }

    std::vector<OGRField> fields(oTable.GetFieldCount(),
                                 FileGDBField::UNSET_FIELD);

    for (const auto &pair : std::vector<std::pair<const char *, int>>{
             {"GDB_SystemCatalog", 0},
             {"GDB_DBTune", 0},
             {"GDB_SpatialRefs", 0},
             {"GDB_Items", 0},
             {"GDB_ItemTypes", 0},
             {"GDB_ItemRelationships", 0},
             {"GDB_ItemRelationshipTypes", 0},
             {"GDB_ReplicaLog", 2}})
    {
        fields[1].String = const_cast<char *>(pair.first);
        fields[2].Integer = pair.second;
        if (!oTable.CreateFeature(fields, nullptr))
            return false;
    }

    m_apoHiddenLayers.emplace_back(std::make_unique<OGROpenFileGDBLayer>(
        this, m_osGDBSystemCatalogFilename.c_str(), "GDB_SystemCatalog", "", "",
        true));

    return oTable.Sync();
}

/***********************************************************************/
/*                       CreateGDBDBTune()                             */
/***********************************************************************/

bool OGROpenFileGDBDataSource::CreateGDBDBTune()
{
    // Write GDB_DBTune file
    const std::string osFilename(
        CPLFormFilename(m_osDirName.c_str(), "a00000002.gdbtable", nullptr));
    FileGDBTable oTable;
    if (!oTable.Create(osFilename.c_str(), 4, FGTGT_NONE, false, false) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Keyword", std::string(), FGFT_STRING,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 32, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "ParameterName", std::string(), FGFT_STRING,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 32, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "ConfigString", std::string(), FGFT_STRING,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 2048, FileGDBField::UNSET_FIELD)))
    {
        return false;
    }

    std::vector<OGRField> fields(oTable.GetFieldCount(),
                                 FileGDBField::UNSET_FIELD);

    static const struct
    {
        const char *pszKeyword;
        const char *pszParameterName;
        const char *pszConfigString;
    } apsData[] = {
        {"DEFAULTS", "UI_TEXT", "The default datafile configuration."},
        {"DEFAULTS", "CHARACTER_FORMAT", "UTF8"},
        {"DEFAULTS", "GEOMETRY_FORMAT", "Compressed"},
        {"DEFAULTS", "GEOMETRY_STORAGE", "InLine"},
        {"DEFAULTS", "BLOB_STORAGE", "InLine"},
        {"DEFAULTS", "MAX_FILE_SIZE", "1TB"},
        {"DEFAULTS", "RASTER_STORAGE", "InLine"},
        {"TEXT_UTF16", "UI_TEXT", "The UTF16 text format configuration."},
        {"TEXT_UTF16", "CHARACTER_FORMAT", "UTF16"},
        {"MAX_FILE_SIZE_4GB", "UI_TEXT",
         "The 4GB maximum datafile size configuration."},
        {"MAX_FILE_SIZE_4GB", "MAX_FILE_SIZE", "4GB"},
        {"MAX_FILE_SIZE_256TB", "UI_TEXT",
         "The 256TB maximum datafile size configuration."},
        {"MAX_FILE_SIZE_256TB", "MAX_FILE_SIZE", "256TB"},
        {"GEOMETRY_UNCOMPRESSED", "UI_TEXT",
         "The Uncompressed Geometry configuration."},
        {"GEOMETRY_UNCOMPRESSED", "GEOMETRY_FORMAT", "Uncompressed"},
        {"GEOMETRY_OUTOFLINE", "UI_TEXT",
         "The Outofline Geometry configuration."},
        {"GEOMETRY_OUTOFLINE", "GEOMETRY_STORAGE", "OutOfLine"},
        {"BLOB_OUTOFLINE", "UI_TEXT", "The Outofline Blob configuration."},
        {"BLOB_OUTOFLINE", "BLOB_STORAGE", "OutOfLine"},
        {"GEOMETRY_AND_BLOB_OUTOFLINE", "UI_TEXT",
         "The Outofline Geometry and Blob configuration."},
        {"GEOMETRY_AND_BLOB_OUTOFLINE", "GEOMETRY_STORAGE", "OutOfLine"},
        {"GEOMETRY_AND_BLOB_OUTOFLINE", "BLOB_STORAGE", "OutOfLine"},
        {"TERRAIN_DEFAULTS", "UI_TERRAIN_TEXT",
         "The terrains default configuration."},
        {"TERRAIN_DEFAULTS", "GEOMETRY_STORAGE", "OutOfLine"},
        {"TERRAIN_DEFAULTS", "BLOB_STORAGE", "OutOfLine"},
        {"MOSAICDATASET_DEFAULTS", "UI_MOSAIC_TEXT",
         "The Outofline Raster and Blob configuration."},
        {"MOSAICDATASET_DEFAULTS", "RASTER_STORAGE", "OutOfLine"},
        {"MOSAICDATASET_DEFAULTS", "BLOB_STORAGE", "OutOfLine"},
        {"MOSAICDATASET_INLINE", "UI_MOSAIC_TEXT",
         "The mosaic dataset inline configuration."},
        {"MOSAICDATASET_INLINE", "CHARACTER_FORMAT", "UTF8"},
        {"MOSAICDATASET_INLINE", "GEOMETRY_FORMAT", "Compressed"},
        {"MOSAICDATASET_INLINE", "GEOMETRY_STORAGE", "InLine"},
        {"MOSAICDATASET_INLINE", "BLOB_STORAGE", "InLine"},
        {"MOSAICDATASET_INLINE", "MAX_FILE_SIZE", "1TB"},
        {"MOSAICDATASET_INLINE", "RASTER_STORAGE", "InLine"}};

    for (const auto &record : apsData)
    {
        fields[0].String = const_cast<char *>(record.pszKeyword);
        fields[1].String = const_cast<char *>(record.pszParameterName);
        fields[2].String = const_cast<char *>(record.pszConfigString);
        if (!oTable.CreateFeature(fields, nullptr))
            return false;
    }

    m_apoHiddenLayers.emplace_back(std::make_unique<OGROpenFileGDBLayer>(
        this, osFilename.c_str(), "GDB_DBTune", "", "", true));

    return oTable.Sync();
}

/***********************************************************************/
/*                       CreateGDBSpatialRefs()                        */
/***********************************************************************/

bool OGROpenFileGDBDataSource::CreateGDBSpatialRefs()
{
    // Write GDB_SpatialRefs file
    m_osGDBSpatialRefsFilename =
        CPLFormFilename(m_osDirName.c_str(), "a00000003.gdbtable", nullptr);
    FileGDBTable oTable;
    if (!oTable.Create(m_osGDBSpatialRefsFilename.c_str(), 4, FGTGT_NONE, false,
                       false) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "ID", std::string(), FGFT_OBJECTID,
            /* bNullable = */ false,
            /* bRequired = */ true,
            /* bEditable = */ false, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "SRTEXT", std::string(), FGFT_STRING,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 2048, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "FalseX", std::string(), FGFT_FLOAT64,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "FalseY", std::string(), FGFT_FLOAT64,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "XYUnits", std::string(), FGFT_FLOAT64,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "FalseZ", std::string(), FGFT_FLOAT64,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "ZUnits", std::string(), FGFT_FLOAT64,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "FalseM", std::string(), FGFT_FLOAT64,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "MUnits", std::string(), FGFT_FLOAT64,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "XYTolerance", std::string(), FGFT_FLOAT64,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "ZTolerance", std::string(), FGFT_FLOAT64,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "MTolerance", std::string(), FGFT_FLOAT64,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)))
    {
        return false;
    }

    m_apoHiddenLayers.emplace_back(std::make_unique<OGROpenFileGDBLayer>(
        this, m_osGDBSpatialRefsFilename.c_str(), "GDB_SpatialRefs", "", "",
        true));

    return oTable.Sync();
}

/***********************************************************************/
/*                       CreateGDBItems()                              */
/***********************************************************************/

bool OGROpenFileGDBDataSource::CreateGDBItems()
{
    // Write GDB_Items file
    const char *ESRI_WKT_WGS84 =
        "GEOGCS[\"GCS_WGS_1984\",DATUM[\"D_WGS_1984\",SPHEROID[\"WGS_1984\","
        "6378137.0,298.257223563]],PRIMEM[\"Greenwich\",0.0],UNIT[\"Degree\",0."
        "0174532925199433]]";
    auto poGeomField = std::unique_ptr<FileGDBGeomField>(
        new FileGDBGeomField("Shape", "", true, ESRI_WKT_WGS84, -180, -90,
                             1000000, 0.000002, {0.012, 0.4, 12.0}));
    poGeomField->SetZOriginScaleTolerance(-100000, 10000, 0.001);
    poGeomField->SetMOriginScaleTolerance(-100000, 10000, 0.001);

    if (!AddNewSpatialRef(poGeomField->GetWKT(), poGeomField->GetXOrigin(),
                          poGeomField->GetYOrigin(), poGeomField->GetXYScale(),
                          poGeomField->GetZOrigin(), poGeomField->GetZScale(),
                          poGeomField->GetMOrigin(), poGeomField->GetMScale(),
                          poGeomField->GetXYTolerance(),
                          poGeomField->GetZTolerance(),
                          poGeomField->GetMTolerance()))
    {
        return false;
    }

    m_osGDBItemsFilename =
        CPLFormFilename(m_osDirName.c_str(), "a00000004.gdbtable", nullptr);
    FileGDBTable oTable;
    if (!oTable.Create(m_osGDBItemsFilename.c_str(), 4, FGTGT_POLYGON, false,
                       false) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "ObjectID", std::string(), FGFT_OBJECTID,
            /* bNullable = */ false,
            /* bRequired = */ true,
            /* bEditable = */ false, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "UUID", std::string(), FGFT_GLOBALID,
            /* bNullable = */ false,
            /* bRequired = */ true,
            /* bEditable = */ false, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Type", std::string(), FGFT_GUID,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Name", std::string(), FGFT_STRING,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 160, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "PhysicalName", std::string(), FGFT_STRING,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 160, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Path", std::string(), FGFT_STRING,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 260, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "DatasetSubtype1", std::string(), FGFT_INT32,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "DatasetSubtype2", std::string(), FGFT_INT32,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "DatasetInfo1", std::string(), FGFT_STRING,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 255, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "DatasetInfo2", std::string(), FGFT_STRING,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 255, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "URL", std::string(), FGFT_STRING,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 255, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Definition", std::string(), FGFT_XML,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Documentation", std::string(), FGFT_XML,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "ItemInfo", std::string(), FGFT_XML,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Properties", std::string(), FGFT_INT32,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Defaults", std::string(), FGFT_BINARY,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::move(poGeomField)))
    {
        return false;
    }

    std::vector<OGRField> fields(oTable.GetFieldCount(),
                                 FileGDBField::UNSET_FIELD);
    m_osRootGUID = OFGDBGenerateUUID();
    fields[1].String = const_cast<char *>(m_osRootGUID.c_str());
    fields[2].String = const_cast<char *>(pszFolderTypeUUID);
    fields[3].String = const_cast<char *>("");
    fields[4].String = const_cast<char *>("");
    fields[5].String = const_cast<char *>("\\");
    fields[10].String = const_cast<char *>("");
    fields[14].Integer = 1;
    if (!oTable.CreateFeature(fields, nullptr))
        return false;

    const std::string osWorkspaceUUID(OFGDBGenerateUUID());
    fields[1].String = const_cast<char *>(osWorkspaceUUID.c_str());
    fields[2].String = const_cast<char *>(pszWorkspaceTypeUUID);
    fields[3].String = const_cast<char *>("Workspace");
    fields[4].String = const_cast<char *>("WORKSPACE");
    fields[5].String = const_cast<char *>("");   // Path
    fields[10].String = const_cast<char *>("");  // URL
    fields[11].String = const_cast<char *>(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<DEWorkspace xmlns:typens=\"http://www.esri.com/schemas/ArcGIS/10.3\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" "
        "xsi:type=\"typens:DEWorkspace\">\n"
        "  <CatalogPath>\\</CatalogPath>\n"
        "  <Name/>\n"
        "  <ChildrenExpanded>false</ChildrenExpanded>\n"
        "  <WorkspaceType>esriLocalDatabaseWorkspace</WorkspaceType>\n"
        "  <WorkspaceFactoryProgID/>\n"
        "  <ConnectionString/>\n"
        "  <ConnectionInfo xsi:nil=\"true\"/>\n"
        "  <Domains xsi:type=\"typens:ArrayOfDomain\"/>\n"
        "  <MajorVersion>3</MajorVersion>\n"
        "  <MinorVersion>0</MinorVersion>\n"
        "  <BugfixVersion>0</BugfixVersion>\n"
        "</DEWorkspace>");
    fields[14].Integer = 0;

    m_apoHiddenLayers.emplace_back(std::make_unique<OGROpenFileGDBLayer>(
        this, m_osGDBItemsFilename.c_str(), "GDB_Items", "", "", true));

    return oTable.CreateFeature(fields, nullptr) && oTable.Sync();
}

/***********************************************************************/
/*                       CreateGDBItemTypes()                          */
/***********************************************************************/

bool OGROpenFileGDBDataSource::CreateGDBItemTypes()
{
    // Write GDB_ItemTypes file
    const std::string osFilename(
        CPLFormFilename(m_osDirName.c_str(), "a00000005.gdbtable", nullptr));
    FileGDBTable oTable;
    if (!oTable.Create(osFilename.c_str(), 4, FGTGT_NONE, false, false) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "ObjectID", std::string(), FGFT_OBJECTID,
            /* bNullable = */ false,
            /* bRequired = */ true,
            /* bEditable = */ false, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "UUID", std::string(), FGFT_GUID,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "ParentTypeID", std::string(), FGFT_GUID,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Name", std::string(), FGFT_STRING,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 160, FileGDBField::UNSET_FIELD)))
    {
        return false;
    }

    std::vector<OGRField> fields(oTable.GetFieldCount(),
                                 FileGDBField::UNSET_FIELD);

    static const struct
    {
        const char *pszUUID;
        const char *pszParentTypeID;
        const char *pszName;
    } apsData[] = {
        {"{8405add5-8df8-4227-8fac-3fcade073386}",
         "{00000000-0000-0000-0000-000000000000}", "Item"},
        {pszFolderTypeUUID, "{8405add5-8df8-4227-8fac-3fcade073386}", "Folder"},
        {"{ffd09c28-fe70-4e25-907c-af8e8a5ec5f3}",
         "{8405add5-8df8-4227-8fac-3fcade073386}", "Resource"},
        {"{28da9e89-ff80-4d6d-8926-4ee2b161677d}",
         "{ffd09c28-fe70-4e25-907c-af8e8a5ec5f3}", "Dataset"},
        {"{fbdd7dd6-4a25-40b7-9a1a-ecc3d1172447}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "Tin"},
        {"{d4912162-3413-476e-9da4-2aefbbc16939}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "AbstractTable"},
        {"{b606a7e1-fa5b-439c-849c-6e9c2481537b}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "Relationship Class"},
        {pszFeatureDatasetTypeUUID, "{28da9e89-ff80-4d6d-8926-4ee2b161677d}",
         "Feature Dataset"},
        {"{73718a66-afb9-4b88-a551-cffa0ae12620}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "Geometric Network"},
        {"{767152d3-ed66-4325-8774-420d46674e07}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "Topology"},
        {"{e6302665-416b-44fa-be33-4e15916ba101}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "Survey Dataset"},
        {"{d5a40288-029e-4766-8c81-de3f61129371}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "Schematic Dataset"},
        {"{db1b697a-3bb6-426a-98a2-6ee7a4c6aed3}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "Toolbox"},
        {pszWorkspaceTypeUUID, "{28da9e89-ff80-4d6d-8926-4ee2b161677d}",
         "Workspace"},
        {"{dc9ef677-1aa3-45a7-8acd-303a5202d0dc}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "Workspace Extension"},
        {"{77292603-930f-475d-ae4f-b8970f42f394}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "Extension Dataset"},
        {"{8637f1ed-8c04-4866-a44a-1cb8288b3c63}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "Domain"},
        {"{4ed4a58e-621f-4043-95ed-850fba45fcbc}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "Replica"},
        {"{d98421eb-d582-4713-9484-43304d0810f6}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "Replica Dataset"},
        {"{dc64b6e4-dc0f-43bd-b4f5-f22385dcf055}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "Historical Marker"},
        {pszTableTypeUUID, "{d4912162-3413-476e-9da4-2aefbbc16939}", "Table"},
        {pszFeatureClassTypeUUID, "{d4912162-3413-476e-9da4-2aefbbc16939}",
         "Feature Class"},
        {"{5ed667a3-9ca9-44a2-8029-d95bf23704b9}",
         "{d4912162-3413-476e-9da4-2aefbbc16939}", "Raster Dataset"},
        {"{35b601f7-45ce-4aff-adb7-7702d3839b12}",
         "{d4912162-3413-476e-9da4-2aefbbc16939}", "Raster Catalog"},
        {"{7771fc7d-a38b-4fd3-8225-639d17e9a131}",
         "{77292603-930f-475d-ae4f-b8970f42f394}", "Network Dataset"},
        {"{76357537-3364-48af-a4be-783c7c28b5cb}",
         "{77292603-930f-475d-ae4f-b8970f42f394}", "Terrain"},
        {"{a3803369-5fc2-4963-bae0-13effc09dd73}",
         "{77292603-930f-475d-ae4f-b8970f42f394}", "Parcel Fabric"},
        {"{a300008d-0cea-4f6a-9dfa-46af829a3df2}",
         "{77292603-930f-475d-ae4f-b8970f42f394}", "Representation Class"},
        {"{787bea35-4a86-494f-bb48-500b96145b58}",
         "{77292603-930f-475d-ae4f-b8970f42f394}", "Catalog Dataset"},
        {"{f8413dcb-2248-4935-bfe9-315f397e5110}",
         "{77292603-930f-475d-ae4f-b8970f42f394}", "Mosaic Dataset"},
        {pszRangeDomainTypeUUID, "{8637f1ed-8c04-4866-a44a-1cb8288b3c63}",
         "Range Domain"},
        {pszCodedDomainTypeUUID, "{8637f1ed-8c04-4866-a44a-1cb8288b3c63}",
         "Coded Value Domain"}};

    for (const auto &record : apsData)
    {
        fields[1].String = const_cast<char *>(record.pszUUID);
        fields[2].String = const_cast<char *>(record.pszParentTypeID);
        fields[3].String = const_cast<char *>(record.pszName);
        if (!oTable.CreateFeature(fields, nullptr))
            return false;
    }

    m_apoHiddenLayers.emplace_back(std::make_unique<OGROpenFileGDBLayer>(
        this, osFilename.c_str(), "GDB_ItemTypes", "", "", true));

    return oTable.Sync();
}

/***********************************************************************/
/*                  CreateGDBItemRelationships()                       */
/***********************************************************************/

bool OGROpenFileGDBDataSource::CreateGDBItemRelationships()
{
    // Write GDB_ItemRelationships file
    m_osGDBItemRelationshipsFilename =
        CPLFormFilename(m_osDirName.c_str(), "a00000006.gdbtable", nullptr);
    FileGDBTable oTable;
    if (!oTable.Create(m_osGDBItemRelationshipsFilename.c_str(), 4, FGTGT_NONE,
                       false, false) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "ObjectID", std::string(), FGFT_OBJECTID,
            /* bNullable = */ false,
            /* bRequired = */ true,
            /* bEditable = */ false, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "UUID", std::string(), FGFT_GLOBALID,
            /* bNullable = */ false,
            /* bRequired = */ true,
            /* bEditable = */ false, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "OriginID", std::string(), FGFT_GUID,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "DestID", std::string(), FGFT_GUID,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Type", std::string(), FGFT_GUID,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Attributes", std::string(), FGFT_XML,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Properties", std::string(), FGFT_INT32,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)))
    {
        return false;
    }

    m_apoHiddenLayers.emplace_back(std::make_unique<OGROpenFileGDBLayer>(
        this, m_osGDBItemRelationshipsFilename.c_str(), "GDB_ItemRelationships",
        "", "", true));

    return oTable.Sync();
}

/***********************************************************************/
/*                 CreateGDBItemRelationshipTypes()                    */
/***********************************************************************/

bool OGROpenFileGDBDataSource::CreateGDBItemRelationshipTypes()
{
    // Write GDB_ItemRelationshipTypes file
    const std::string osFilename(
        CPLFormFilename(m_osDirName.c_str(), "a00000007.gdbtable", nullptr));
    FileGDBTable oTable;
    if (!oTable.Create(osFilename.c_str(), 4, FGTGT_NONE, false, false) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "ObjectID", std::string(), FGFT_OBJECTID,
            /* bNullable = */ false,
            /* bRequired = */ true,
            /* bEditable = */ false, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "UUID", std::string(), FGFT_GUID,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "OrigItemTypeID", std::string(), FGFT_GUID,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "DestItemTypeID", std::string(), FGFT_GUID,
            /* bNullable = */ false,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "Name", std::string(), FGFT_STRING,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 160, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "ForwardLabel", std::string(), FGFT_STRING,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 255, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "BackwardLabel", std::string(), FGFT_STRING,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 255, FileGDBField::UNSET_FIELD)) ||
        !oTable.CreateField(std::make_unique<FileGDBField>(
            "IsContainment", std::string(), FGFT_INT16,
            /* bNullable = */ true,
            /* bRequired = */ false,
            /* bEditable = */ true, 0, FileGDBField::UNSET_FIELD)))
    {
        return false;
    }

    static const struct
    {
        const char *pszUUID;
        const char *pszOrigItemTypeID;
        const char *pszDestItemTypeID;
        const char *pszName;
        const char *pszForwardLabel;
        const char *pszBackwardLabel;
        int IsContainment;
    } apsData[] = {
        {"{0d10b3a7-2f64-45e6-b7ac-2fc27bf2133c}", pszFolderTypeUUID,
         pszFolderTypeUUID, "FolderInFolder", "Parent Folder Of",
         "Child Folder Of", 1},
        {"{5dd0c1af-cb3d-4fea-8c51-cb3ba8d77cdb}", pszFolderTypeUUID,
         "{8405add5-8df8-4227-8fac-3fcade073386}", "ItemInFolder",
         "Contains Item", "Contained In Folder", 1},
        {pszDatasetInFeatureDatasetUUID, pszFeatureDatasetTypeUUID,
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "DatasetInFeatureDataset",
         "Contains Dataset", "Contained In FeatureDataset", 1},
        {pszDatasetInFolderUUID, pszFolderTypeUUID,
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "DatasetInFolder",
         "Contains Dataset", "Contained in Dataset", 1},
        {pszDomainInDatasetUUID, "{28da9e89-ff80-4d6d-8926-4ee2b161677d}",
         "{8637f1ed-8c04-4866-a44a-1cb8288b3c63}", "DomainInDataset",
         "Contains Domain", "Contained in Dataset", 0},
        {"{725badab-3452-491b-a795-55f32d67229c}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "DatasetsRelatedThrough",
         "Origin Of", "Destination Of", 0},
        {"{d088b110-190b-4229-bdf7-89fddd14d1ea}",
         "{767152d3-ed66-4325-8774-420d46674e07}", pszFeatureClassTypeUUID,
         "FeatureClassInTopology", "Spatially Manages Feature Class",
         "Participates In Topology", 0},
        {"{dc739a70-9b71-41e8-868c-008cf46f16d7}",
         "{73718a66-afb9-4b88-a551-cffa0ae12620}", pszFeatureClassTypeUUID,
         "FeatureClassInGeometricNetwork", "Spatially Manages Feature Class",
         "Participates In Geometric Network", 0},
        {"{b32b8563-0b96-4d32-92c4-086423ae9962}",
         "{7771fc7d-a38b-4fd3-8225-639d17e9a131}", pszFeatureClassTypeUUID,
         "FeatureClassInNetworkDataset", "Spatially Manages Feature Class",
         "Participates In Network Dataset", 0},
        {"{908a4670-1111-48c6-8269-134fdd3fe617}",
         "{7771fc7d-a38b-4fd3-8225-639d17e9a131}", pszTableTypeUUID,
         "TableInNetworkDataset", "Manages Table",
         "Participates In Network Dataset", 0},
        {"{55d2f4dc-cb17-4e32-a8c7-47591e8c71de}",
         "{76357537-3364-48af-a4be-783c7c28b5cb}", pszFeatureClassTypeUUID,
         "FeatureClassInTerrain", "Spatially Manages Feature Class",
         "Participates In Terrain", 0},
        {"{583a5baa-3551-41ae-8aa8-1185719f3889}",
         "{a3803369-5fc2-4963-bae0-13effc09dd73}", pszFeatureClassTypeUUID,
         "FeatureClassInParcelFabric", "Spatially Manages Feature Class",
         "Participates In Parcel Fabric", 0},
        {"{5f9085e0-788f-4354-ae3c-34c83a7ea784}",
         "{a3803369-5fc2-4963-bae0-13effc09dd73}", pszTableTypeUUID,
         "TableInParcelFabric", "Manages Table",
         "Participates In Parcel Fabric", 0},
        {"{e79b44e3-f833-4b12-90a1-364ec4ddc43e}", pszFeatureClassTypeUUID,
         "{a300008d-0cea-4f6a-9dfa-46af829a3df2}",
         "RepresentationOfFeatureClass", "Feature Class Representation",
         "Represented Feature Class", 0},
        {"{8db31af1-df7c-4632-aa10-3cc44b0c6914}",
         "{4ed4a58e-621f-4043-95ed-850fba45fcbc}",
         "{d98421eb-d582-4713-9484-43304d0810f6}", "ReplicaDatasetInReplica",
         "Replicated Dataset", "Participates In Replica", 1},
        {"{d022de33-45bd-424c-88bf-5b1b6b957bd3}",
         "{d98421eb-d582-4713-9484-43304d0810f6}",
         "{28da9e89-ff80-4d6d-8926-4ee2b161677d}", "DatasetOfReplicaDataset",
         "Replicated Dataset", "Dataset of Replicated Dataset", 0},
    };

    std::vector<OGRField> fields(oTable.GetFieldCount(),
                                 FileGDBField::UNSET_FIELD);
    for (const auto &record : apsData)
    {
        fields[1].String = const_cast<char *>(record.pszUUID);
        fields[2].String = const_cast<char *>(record.pszOrigItemTypeID);
        fields[3].String = const_cast<char *>(record.pszDestItemTypeID);
        fields[4].String = const_cast<char *>(record.pszName);
        fields[5].String = const_cast<char *>(record.pszForwardLabel);
        fields[6].String = const_cast<char *>(record.pszBackwardLabel);
        fields[7].Integer = record.IsContainment;
        if (!oTable.CreateFeature(fields, nullptr))
            return false;
    }

    m_apoHiddenLayers.emplace_back(std::make_unique<OGROpenFileGDBLayer>(
        this, osFilename.c_str(), "GDB_ItemRelationshipTypes", "", "", true));

    return oTable.Sync();
}

/***********************************************************************/
/*                             Create()                                */
/***********************************************************************/

bool OGROpenFileGDBDataSource::Create(const char *pszName)
{

    if (!EQUAL(CPLGetExtension(pszName), "gdb"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Extension of the directory should be gdb");
        return false;
    }

    /* Don't try to create on top of something already there */
    VSIStatBufL sStat;
    if (VSIStatL(pszName, &sStat) == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s already exists.", pszName);
        return false;
    }

    if (VSIMkdir(pszName, 0755) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create directory %s.",
                 pszName);
        return false;
    }

    m_osDirName = pszName;
    eAccess = GA_Update;

    {
        // Write "gdb" file
        const std::string osFilename(CPLFormFilename(pszName, "gdb", nullptr));
        VSILFILE *fp = VSIFOpenL(osFilename.c_str(), "wb");
        if (!fp)
            return false;
        // Write what the FileGDB SDK writes...
        VSIFWriteL("\x05\x00\x00\x00\xDE\xAD\xBE\xEF", 1, 8, fp);
        VSIFCloseL(fp);
    }

    {
        // Write "timestamps" file
        const std::string osFilename(
            CPLFormFilename(pszName, "timestamps", nullptr));
        VSILFILE *fp = VSIFOpenL(osFilename.c_str(), "wb");
        if (!fp)
            return false;
        // Write what the FileGDB SDK writes...
        std::vector<GByte> values(400, 0xFF);
        VSIFWriteL(values.data(), 1, values.size(), fp);
        VSIFCloseL(fp);
    }

    return CreateGDBSystemCatalog() && CreateGDBDBTune() &&
           CreateGDBSpatialRefs() && CreateGDBItems() && CreateGDBItemTypes() &&
           CreateGDBItemRelationships() && CreateGDBItemRelationshipTypes();
    // GDB_ReplicaLog can be omitted.
}

/************************************************************************/
/*                             ICreateLayer()                           */
/************************************************************************/

OGRLayer *
OGROpenFileGDBDataSource::ICreateLayer(const char *pszLayerName,
                                       const OGRGeomFieldDefn *poGeomFieldDefn,
                                       CSLConstList papszOptions)
{
    if (eAccess != GA_Update)
        return nullptr;

    if (m_bInTransaction && !BackupSystemTablesForTransaction())
        return nullptr;

    if (m_osRootGUID.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Root UUID missing");
        return nullptr;
    }

    auto eType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;

    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBSystemCatalogFilename.c_str(), false))
        return nullptr;
    const int nTableNum = 1 + oTable.GetTotalRecordCount();
    oTable.Close();

    const std::string osFilename(CPLFormFilename(
        m_osDirName.c_str(), CPLSPrintf("a%08x.gdbtable", nTableNum), nullptr));

    if (wkbFlatten(eType) == wkbLineString)
        eType = OGR_GT_SetModifier(wkbMultiLineString, OGR_GT_HasZ(eType),
                                   OGR_GT_HasM(eType));
    else if (wkbFlatten(eType) == wkbPolygon)
        eType = OGR_GT_SetModifier(wkbMultiPolygon, OGR_GT_HasZ(eType),
                                   OGR_GT_HasM(eType));

    auto poLayer = std::make_unique<OGROpenFileGDBLayer>(
        this, osFilename.c_str(), pszLayerName, eType, papszOptions);
    if (!poLayer->Create(poGeomFieldDefn))
        return nullptr;
    if (m_bInTransaction)
    {
        if (!poLayer->BeginEmulatedTransaction())
            return nullptr;
        m_oSetLayersCreatedInTransaction.insert(poLayer.get());
    }
    m_apoLayers.emplace_back(std::move(poLayer));

    return m_apoLayers.back().get();
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGROpenFileGDBDataSource::DeleteLayer(int iLayer)
{
    if (eAccess != GA_Update)
        return OGRERR_FAILURE;

    if (iLayer < 0 || iLayer >= static_cast<int>(m_apoLayers.size()))
        return OGRERR_FAILURE;

    if (m_bInTransaction && !BackupSystemTablesForTransaction())
        return false;

    auto poLayer = m_apoLayers[iLayer].get();

    // Remove from GDB_SystemCatalog
    {
        FileGDBTable oTable;
        if (!oTable.Open(m_osGDBSystemCatalogFilename.c_str(), true))
            return OGRERR_FAILURE;

        FETCH_FIELD_IDX_WITH_RET(iName, "Name", FGFT_STRING, OGRERR_FAILURE);

        for (int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount();
             ++iCurFeat)
        {
            iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
            if (iCurFeat < 0)
                break;
            const auto psName = oTable.GetFieldValue(iName);
            if (psName && strcmp(psName->String, poLayer->GetName()) == 0)
            {
                oTable.DeleteFeature(iCurFeat + 1);
                break;
            }
        }
    }

    // Remove from GDB_Items
    std::string osUUID;
    {
        FileGDBTable oTable;
        if (!oTable.Open(m_osGDBItemsFilename.c_str(), true))
            return OGRERR_FAILURE;

        FETCH_FIELD_IDX_WITH_RET(iUUID, "UUID", FGFT_GLOBALID, OGRERR_FAILURE);
        FETCH_FIELD_IDX_WITH_RET(iName, "Name", FGFT_STRING, OGRERR_FAILURE);

        for (int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount();
             ++iCurFeat)
        {
            iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
            if (iCurFeat < 0)
                break;
            const auto psName = oTable.GetFieldValue(iName);
            if (psName && strcmp(psName->String, poLayer->GetName()) == 0)
            {
                const auto psUUID = oTable.GetFieldValue(iUUID);
                if (psUUID)
                {
                    osUUID = psUUID->String;
                }

                oTable.DeleteFeature(iCurFeat + 1);
                break;
            }
        }
    }

    // Remove from GDB_ItemRelationships
    if (!osUUID.empty())
    {
        FileGDBTable oTable;
        if (!oTable.Open(m_osGDBItemRelationshipsFilename.c_str(), true))
            return OGRERR_FAILURE;

        FETCH_FIELD_IDX_WITH_RET(iOriginID, "OriginID", FGFT_GUID,
                                 OGRERR_FAILURE);
        FETCH_FIELD_IDX_WITH_RET(iDestID, "DestID", FGFT_GUID, OGRERR_FAILURE);

        for (int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount();
             ++iCurFeat)
        {
            iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
            if (iCurFeat < 0)
                break;

            const auto psOriginID = oTable.GetFieldValue(iOriginID);
            if (psOriginID && psOriginID->String == osUUID)
            {
                oTable.DeleteFeature(iCurFeat + 1);
            }
            else
            {
                const auto psDestID = oTable.GetFieldValue(iDestID);
                if (psDestID && psDestID->String == osUUID)
                {
                    oTable.DeleteFeature(iCurFeat + 1);
                }
            }
        }
    }

    const std::string osDirname = CPLGetPath(poLayer->GetFilename().c_str());
    const std::string osFilenameBase =
        CPLGetBasename(poLayer->GetFilename().c_str());

    if (m_bInTransaction)
    {
        auto oIter =
            m_oSetLayersCreatedInTransaction.find(m_apoLayers[iLayer].get());
        if (oIter != m_oSetLayersCreatedInTransaction.end())
        {
            m_oSetLayersCreatedInTransaction.erase(oIter);
        }
        else
        {
            poLayer->BeginEmulatedTransaction();
            poLayer->Close();
            m_oSetLayersDeletedInTransaction.insert(
                std::move(m_apoLayers[iLayer]));
        }
    }

    // Delete OGR layer
    m_apoLayers.erase(m_apoLayers.begin() + iLayer);

    // Remove files associated with the layer
    char **papszFiles = VSIReadDir(osDirname.c_str());
    for (char **papszIter = papszFiles; papszIter && *papszIter; ++papszIter)
    {
        if (STARTS_WITH(*papszIter, osFilenameBase.c_str()))
        {
            VSIUnlink(CPLFormFilename(osDirname.c_str(), *papszIter, nullptr));
        }
    }
    CSLDestroy(papszFiles);

    return OGRERR_NONE;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

CPLErr OGROpenFileGDBDataSource::FlushCache(bool /*bAtClosing*/)
{
    if (eAccess != GA_Update)
        return CE_None;

    CPLErr eErr = CE_None;
    for (auto &poLayer : m_apoLayers)
    {
        if (poLayer->SyncToDisk() != OGRERR_NONE)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                          AddFieldDomain()                            */
/************************************************************************/

bool OGROpenFileGDBDataSource::AddFieldDomain(
    std::unique_ptr<OGRFieldDomain> &&domain, std::string &failureReason)
{
    const std::string domainName(domain->GetName());
    if (eAccess != GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "AddFieldDomain() not supported on read-only dataset");
        return false;
    }

    if (GetFieldDomain(domainName) != nullptr)
    {
        failureReason = "A domain of identical name already exists";
        return false;
    }

    if (m_bInTransaction && !BackupSystemTablesForTransaction())
        return false;

    std::string osXML =
        BuildXMLFieldDomainDef(domain.get(), false, failureReason);
    if (osXML.empty())
    {
        return false;
    }

    const std::string osThisGUID = OFGDBGenerateUUID();

    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBItemsFilename.c_str(), true))
        return false;

    FETCH_FIELD_IDX(iUUID, "UUID", FGFT_GLOBALID);
    FETCH_FIELD_IDX(iType, "Type", FGFT_GUID);
    FETCH_FIELD_IDX(iName, "Name", FGFT_STRING);
    FETCH_FIELD_IDX(iPhysicalName, "PhysicalName", FGFT_STRING);
    FETCH_FIELD_IDX(iPath, "Path", FGFT_STRING);
    FETCH_FIELD_IDX(iURL, "URL", FGFT_STRING);
    FETCH_FIELD_IDX(iDefinition, "Definition", FGFT_XML);
    FETCH_FIELD_IDX(iProperties, "Properties", FGFT_INT32);

    std::vector<OGRField> fields(oTable.GetFieldCount(),
                                 FileGDBField::UNSET_FIELD);
    fields[iUUID].String = const_cast<char *>(osThisGUID.c_str());
    switch (domain->GetDomainType())
    {
        case OFDT_CODED:
            fields[iType].String = const_cast<char *>(pszCodedDomainTypeUUID);
            break;

        case OFDT_RANGE:
            fields[iType].String = const_cast<char *>(pszRangeDomainTypeUUID);
            break;

        case OFDT_GLOB:
            CPLAssert(false);
            break;
    }
    fields[iName].String = const_cast<char *>(domainName.c_str());
    CPLString osUCName(domainName);
    osUCName.toupper();
    fields[iPhysicalName].String = const_cast<char *>(osUCName.c_str());
    fields[iPath].String = const_cast<char *>("");
    fields[iURL].String = const_cast<char *>("");
    fields[iDefinition].String = const_cast<char *>(osXML.c_str());
    fields[iProperties].Integer = 1;

    if (!(oTable.CreateFeature(fields, nullptr) && oTable.Sync()))
        return false;

    m_oMapFieldDomains[domainName] = std::move(domain);

    return true;
}

/************************************************************************/
/*                         DeleteFieldDomain()                          */
/************************************************************************/

bool OGROpenFileGDBDataSource::DeleteFieldDomain(
    const std::string &name, std::string & /*failureReason*/)
{
    if (eAccess != GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "DeleteFieldDomain() not supported on read-only dataset");
        return false;
    }

    if (m_bInTransaction && !BackupSystemTablesForTransaction())
        return false;

    // Remove object from GDB_Items
    std::string osUUID;
    {
        FileGDBTable oTable;
        if (!oTable.Open(m_osGDBItemsFilename.c_str(), true))
            return false;

        FETCH_FIELD_IDX(iUUID, "UUID", FGFT_GLOBALID);
        FETCH_FIELD_IDX(iType, "Type", FGFT_GUID);
        FETCH_FIELD_IDX(iName, "Name", FGFT_STRING);

        for (int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount();
             ++iCurFeat)
        {
            iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
            if (iCurFeat < 0)
                break;
            const auto psName = oTable.GetFieldValue(iName);
            if (psName && psName->String == name)
            {
                const auto psType = oTable.GetFieldValue(iType);
                if (psType && (EQUAL(psType->String, pszRangeDomainTypeUUID) ||
                               EQUAL(psType->String, pszCodedDomainTypeUUID)))
                {
                    const auto psUUID = oTable.GetFieldValue(iUUID);
                    if (psUUID)
                    {
                        osUUID = psUUID->String;
                    }

                    if (!(oTable.DeleteFeature(iCurFeat + 1) && oTable.Sync()))
                    {
                        return false;
                    }
                    break;
                }
            }
        }
    }
    if (osUUID.empty())
        return false;

    // Remove links from layers to domain, into GDB_ItemRelationships
    {
        FileGDBTable oTable;
        if (!oTable.Open(m_osGDBItemRelationshipsFilename.c_str(), true))
            return false;

        FETCH_FIELD_IDX(iDestID, "DestID", FGFT_GUID);
        FETCH_FIELD_IDX(iType, "Type", FGFT_GUID);

        for (int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount();
             ++iCurFeat)
        {
            iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
            if (iCurFeat < 0)
                break;

            const auto psType = oTable.GetFieldValue(iType);
            if (psType && EQUAL(psType->String, pszDomainInDatasetUUID))
            {
                const auto psDestID = oTable.GetFieldValue(iDestID);
                if (psDestID && EQUAL(psDestID->String, osUUID.c_str()))
                {
                    if (!(oTable.DeleteFeature(iCurFeat + 1) && oTable.Sync()))
                    {
                        return false;
                    }
                }
            }
        }

        if (!oTable.Sync())
        {
            return false;
        }
    }

    m_oMapFieldDomains.erase(name);

    return true;
}

/************************************************************************/
/*                        UpdateFieldDomain()                           */
/************************************************************************/

bool OGROpenFileGDBDataSource::UpdateFieldDomain(
    std::unique_ptr<OGRFieldDomain> &&domain, std::string &failureReason)
{
    const std::string domainName(domain->GetName());
    if (eAccess != GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "UpdateFieldDomain() not supported on read-only dataset");
        return false;
    }

    if (GetFieldDomain(domainName) == nullptr)
    {
        failureReason = "The domain should already exist to be updated";
        return false;
    }

    if (m_bInTransaction && !BackupSystemTablesForTransaction())
        return false;

    std::string osXML =
        BuildXMLFieldDomainDef(domain.get(), false, failureReason);
    if (osXML.empty())
    {
        return false;
    }

    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBItemsFilename.c_str(), true))
        return false;

    FETCH_FIELD_IDX(iType, "Type", FGFT_GUID);
    FETCH_FIELD_IDX(iName, "Name", FGFT_STRING);
    FETCH_FIELD_IDX(iDefinition, "Definition", FGFT_XML);

    bool bMatchFound = false;
    for (int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount(); ++iCurFeat)
    {
        iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
        if (iCurFeat < 0)
            break;
        const auto psName = oTable.GetFieldValue(iName);
        if (psName && psName->String == domainName)
        {
            const auto psType = oTable.GetFieldValue(iType);
            if (psType && (EQUAL(psType->String, pszRangeDomainTypeUUID) ||
                           EQUAL(psType->String, pszCodedDomainTypeUUID)))
            {
                auto asFields = oTable.GetAllFieldValues();

                if (!OGR_RawField_IsNull(&asFields[iDefinition]) &&
                    !OGR_RawField_IsUnset(&asFields[iDefinition]))
                {
                    CPLFree(asFields[iDefinition].String);
                }
                asFields[iDefinition].String = CPLStrdup(osXML.c_str());

                const char *pszNewTypeUUID = "";
                switch (domain->GetDomainType())
                {
                    case OFDT_CODED:
                        pszNewTypeUUID = pszCodedDomainTypeUUID;
                        break;

                    case OFDT_RANGE:
                        pszNewTypeUUID = pszRangeDomainTypeUUID;
                        break;

                    case OFDT_GLOB:
                        CPLAssert(false);
                        break;
                }

                if (!OGR_RawField_IsNull(&asFields[iType]) &&
                    !OGR_RawField_IsUnset(&asFields[iType]))
                {
                    CPLFree(asFields[iType].String);
                }
                asFields[iType].String = CPLStrdup(pszNewTypeUUID);

                bool bRet =
                    oTable.UpdateFeature(iCurFeat + 1, asFields, nullptr);
                oTable.FreeAllFieldValues(asFields);
                if (!bRet)
                    return false;
                bMatchFound = true;
                break;
            }
        }

        if (!oTable.Sync())
        {
            return false;
        }
    }

    if (!bMatchFound)
        return false;

    m_oMapFieldDomains[domainName] = std::move(domain);

    return true;
}

/************************************************************************/
/*                        GetRelationshipNames()                        */
/************************************************************************/

std::vector<std::string> OGROpenFileGDBDataSource::GetRelationshipNames(
    CPL_UNUSED CSLConstList papszOptions) const

{
    std::vector<std::string> oasNames;
    oasNames.reserve(m_osMapRelationships.size());
    for (auto it = m_osMapRelationships.begin();
         it != m_osMapRelationships.end(); ++it)
    {
        oasNames.emplace_back(it->first);
    }
    return oasNames;
}

/************************************************************************/
/*                        GetRelationship()                             */
/************************************************************************/

const GDALRelationship *
OGROpenFileGDBDataSource::GetRelationship(const std::string &name) const

{
    auto it = m_osMapRelationships.find(name);
    if (it == m_osMapRelationships.end())
        return nullptr;

    return it->second.get();
}

/************************************************************************/
/*                          AddRelationship()                           */
/************************************************************************/

bool OGROpenFileGDBDataSource::AddRelationship(
    std::unique_ptr<GDALRelationship> &&relationship,
    std::string &failureReason)
{
    const std::string relationshipName(relationship->GetName());
    if (eAccess != GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "AddRelationship() not supported on read-only dataset");
        return false;
    }

    if (GetRelationship(relationshipName) != nullptr)
    {
        failureReason = "A relationship of identical name already exists";
        return false;
    }

    if (relationship->GetCardinality() ==
        GDALRelationshipCardinality::GRC_MANY_TO_ONE)
    {
        failureReason = "Many to one relationships are not supported";
        return false;
    }
    else if (relationship->GetCardinality() ==
                 GDALRelationshipCardinality::GRC_MANY_TO_MANY &&
             !relationship->GetMappingTableName().empty() &&
             relationship->GetName() != relationship->GetMappingTableName())
    {
        failureReason = "Mapping table name must match relationship name for "
                        "many-to-many relationships";
        return false;
    }

    if (m_bInTransaction && !BackupSystemTablesForTransaction())
        return false;

    const std::string osThisGUID = OFGDBGenerateUUID();

    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBItemsFilename.c_str(), true))
        return false;

    // hopefully this just needs to be a unique value. Seems to autoincrement
    // when created from ArcMap at least!
    const int iDsId = oTable.GetTotalRecordCount() + 1;

    std::string osMappingTableOidName;
    if (relationship->GetCardinality() ==
        GDALRelationshipCardinality::GRC_MANY_TO_MANY)
    {
        if (!relationship->GetMappingTableName().empty())
        {
            auto poLayer =
                GetLayerByName(relationship->GetMappingTableName().c_str());
            if (poLayer)
            {
                osMappingTableOidName = poLayer->GetFIDColumn();
            }
        }
        else
        {
            // auto create mapping table
            CPLStringList aosOptions;
            aosOptions.SetNameValue("FID", "RID");
            OGRLayer *poMappingTable = ICreateLayer(
                relationship->GetName().c_str(), nullptr, aosOptions.List());
            if (!poMappingTable)
            {
                failureReason = "Could not create mapping table " +
                                relationship->GetMappingTableName();
                return false;
            }

            OGRFieldDefn oOriginFkFieldDefn("origin_fk", OFTString);
            if (poMappingTable->CreateField(&oOriginFkFieldDefn) != OGRERR_NONE)
            {
                failureReason =
                    "Could not create origin_fk field in mapping table " +
                    relationship->GetMappingTableName();
                return false;
            }

            OGRFieldDefn oDestinationFkFieldDefn("destination_fk", OFTString);
            if (poMappingTable->CreateField(&oDestinationFkFieldDefn) !=
                OGRERR_NONE)
            {
                failureReason =
                    "Could not create destination_fk field in mapping table " +
                    relationship->GetMappingTableName();
                return false;
            }

            osMappingTableOidName = "RID";
            relationship->SetMappingTableName(relationship->GetName());
            relationship->SetLeftMappingTableFields({"origin_fk"});
            relationship->SetRightMappingTableFields({"destination_fk"});
        }
    }

    std::string osXML = BuildXMLRelationshipDef(
        relationship.get(), iDsId, osMappingTableOidName, failureReason);
    if (osXML.empty())
    {
        return false;
    }

    std::string osItemInfoXML =
        BuildXMLRelationshipItemInfo(relationship.get(), failureReason);
    if (osItemInfoXML.empty())
    {
        return false;
    }

    std::string osDocumentationXML =
        BuildXMLRelationshipDocumentation(relationship.get(), failureReason);
    if (osDocumentationXML.empty())
    {
        return false;
    }

    std::string osOriginUUID;
    if (!FindUUIDFromName(relationship->GetLeftTableName(), osOriginUUID))
    {
        failureReason = ("Left table " + relationship->GetLeftTableName() +
                         " is not an existing layer in the dataset")
                            .c_str();
        return false;
    }
    std::string osDestinationUUID;
    if (!FindUUIDFromName(relationship->GetRightTableName(), osDestinationUUID))
    {
        failureReason = ("Right table " + relationship->GetRightTableName() +
                         " is not an existing layer in the dataset")
                            .c_str();
        return false;
    }

    FETCH_FIELD_IDX(iUUID, "UUID", FGFT_GLOBALID);
    FETCH_FIELD_IDX(iType, "Type", FGFT_GUID);
    FETCH_FIELD_IDX(iName, "Name", FGFT_STRING);
    FETCH_FIELD_IDX(iPhysicalName, "PhysicalName", FGFT_STRING);
    FETCH_FIELD_IDX(iPath, "Path", FGFT_STRING);
    FETCH_FIELD_IDX(iDatasetSubtype1, "DatasetSubtype1", FGFT_INT32);
    FETCH_FIELD_IDX(iDatasetSubtype2, "DatasetSubtype2", FGFT_INT32);
    FETCH_FIELD_IDX(iURL, "URL", FGFT_STRING);
    FETCH_FIELD_IDX(iDefinition, "Definition", FGFT_XML);
    FETCH_FIELD_IDX(iDocumentation, "Documentation", FGFT_XML);
    FETCH_FIELD_IDX(iItemInfo, "ItemInfo", FGFT_XML);
    FETCH_FIELD_IDX(iProperties, "Properties", FGFT_INT32);

    std::vector<OGRField> fields(oTable.GetFieldCount(),
                                 FileGDBField::UNSET_FIELD);
    fields[iUUID].String = const_cast<char *>(osThisGUID.c_str());
    fields[iType].String = const_cast<char *>(pszRelationshipTypeUUID);
    fields[iName].String = const_cast<char *>(relationshipName.c_str());
    CPLString osUCName(relationshipName);
    osUCName.toupper();
    fields[iPhysicalName].String = const_cast<char *>(osUCName.c_str());
    const std::string osPath = "\\" + relationshipName;
    fields[iPath].String = const_cast<char *>(osPath.c_str());
    switch (relationship->GetCardinality())
    {
        case GDALRelationshipCardinality::GRC_ONE_TO_ONE:
            fields[iDatasetSubtype1].Integer = 1;
            break;
        case GDALRelationshipCardinality::GRC_ONE_TO_MANY:
            fields[iDatasetSubtype1].Integer = 2;
            break;
        case GDALRelationshipCardinality::GRC_MANY_TO_MANY:
            fields[iDatasetSubtype1].Integer = 3;
            break;
        case GDALRelationshipCardinality::GRC_MANY_TO_ONE:
            // unreachable
            break;
    }
    fields[iDatasetSubtype2].Integer = 0;
    fields[iURL].String = const_cast<char *>("");
    fields[iDefinition].String = const_cast<char *>(osXML.c_str());
    fields[iDocumentation].String =
        const_cast<char *>(osDocumentationXML.c_str());
    fields[iItemInfo].String = const_cast<char *>(osItemInfoXML.c_str());
    fields[iProperties].Integer = 1;

    if (!(oTable.CreateFeature(fields, nullptr) && oTable.Sync()))
        return false;

    if (!RegisterRelationshipInItemRelationships(osThisGUID, osOriginUUID,
                                                 osDestinationUUID))
        return false;

    m_osMapRelationships[relationshipName] = std::move(relationship);

    return true;
}

/************************************************************************/
/*                         DeleteRelationship()                         */
/************************************************************************/

bool OGROpenFileGDBDataSource::DeleteRelationship(const std::string &name,
                                                  std::string &failureReason)
{
    if (eAccess != GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "DeleteRelationship() not supported on read-only dataset");
        return false;
    }

    if (m_bInTransaction && !BackupSystemTablesForTransaction())
        return false;

    // Remove from GDB_Items
    std::string osUUID;
    {
        FileGDBTable oTable;
        if (!oTable.Open(m_osGDBItemsFilename.c_str(), true))
            return false;

        FETCH_FIELD_IDX_WITH_RET(iUUID, "UUID", FGFT_GLOBALID, false);
        FETCH_FIELD_IDX_WITH_RET(iType, "Type", FGFT_GUID, false);
        FETCH_FIELD_IDX_WITH_RET(iName, "Name", FGFT_STRING, false);

        for (int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount();
             ++iCurFeat)
        {
            iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
            if (iCurFeat < 0)
                break;

            const auto psType = oTable.GetFieldValue(iType);
            if (!psType || !EQUAL(psType->String, pszRelationshipTypeUUID))
            {
                continue;
            }

            const auto psName = oTable.GetFieldValue(iName);
            if (psName && strcmp(psName->String, name.c_str()) != 0)
            {
                continue;
            }

            const auto psUUID = oTable.GetFieldValue(iUUID);
            if (psUUID)
            {
                osUUID = psUUID->String;
                if (!(oTable.DeleteFeature(iCurFeat + 1) && oTable.Sync()))
                {
                    failureReason =
                        "Could not delete relationship from GDB_Items table";
                    return false;
                }
            }
        }
    }

    if (osUUID.empty())
    {
        failureReason = "Could not find relationship with name " + name;
        return false;
    }

    if (!RemoveRelationshipFromItemRelationships(osUUID))
    {
        failureReason =
            "Could not remove relationship from GDB_ItemRelationships";
        return false;
    }

    m_osMapRelationships.erase(name);
    return true;
}

/************************************************************************/
/*                        UpdateRelationship()                          */
/************************************************************************/

bool OGROpenFileGDBDataSource::UpdateRelationship(
    std::unique_ptr<GDALRelationship> &&relationship,
    std::string &failureReason)
{
    const std::string relationshipName(relationship->GetName());
    if (eAccess != GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "UpdateRelationship() not supported on read-only dataset");
        return false;
    }

    if (GetRelationship(relationshipName) == nullptr)
    {
        failureReason = "The relationship should already exist to be updated";
        return false;
    }

    if (relationship->GetCardinality() ==
        GDALRelationshipCardinality::GRC_MANY_TO_ONE)
    {
        failureReason = "Many to one relationships are not supported";
        return false;
    }

    if (m_bInTransaction && !BackupSystemTablesForTransaction())
        return false;

    std::string osOriginUUID;
    if (!FindUUIDFromName(relationship->GetLeftTableName(), osOriginUUID))
    {
        failureReason = ("Left table " + relationship->GetLeftTableName() +
                         " is not an existing layer in the dataset")
                            .c_str();
        return false;
    }
    std::string osDestinationUUID;
    if (!FindUUIDFromName(relationship->GetRightTableName(), osDestinationUUID))
    {
        failureReason = ("Right table " + relationship->GetRightTableName() +
                         " is not an existing layer in the dataset")
                            .c_str();
        return false;
    }

    FileGDBTable oTable;
    if (!oTable.Open(m_osGDBItemsFilename.c_str(), true))
        return false;

    // hopefully this just needs to be a unique value. Seems to autoincrement
    // when created from ArcMap at least!
    const int iDsId = oTable.GetTotalRecordCount() + 1;

    std::string osMappingTableOidName;
    if (relationship->GetCardinality() ==
        GDALRelationshipCardinality::GRC_MANY_TO_MANY)
    {
        if (!relationship->GetMappingTableName().empty())
        {
            auto poLayer =
                GetLayerByName(relationship->GetMappingTableName().c_str());
            if (poLayer)
            {
                osMappingTableOidName = poLayer->GetFIDColumn();
            }
        }
        if (osMappingTableOidName.empty())
        {
            failureReason = "Relationship mapping table does not exist";
            return false;
        }
    }

    std::string osXML = BuildXMLRelationshipDef(
        relationship.get(), iDsId, osMappingTableOidName, failureReason);
    if (osXML.empty())
    {
        return false;
    }

    FETCH_FIELD_IDX_WITH_RET(iUUID, "UUID", FGFT_GLOBALID, false);
    FETCH_FIELD_IDX_WITH_RET(iType, "Type", FGFT_GUID, false);
    FETCH_FIELD_IDX_WITH_RET(iName, "Name", FGFT_STRING, false);
    FETCH_FIELD_IDX_WITH_RET(iDefinition, "Definition", FGFT_XML, false);
    FETCH_FIELD_IDX_WITH_RET(iDatasetSubtype1, "DatasetSubtype1", FGFT_INT32,
                             false);

    bool bMatchFound = false;
    std::string osUUID;
    for (int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount(); ++iCurFeat)
    {
        iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
        if (iCurFeat < 0)
            break;
        const auto psName = oTable.GetFieldValue(iName);
        if (psName && psName->String == relationshipName)
        {
            const auto psType = oTable.GetFieldValue(iType);
            if (psType && EQUAL(psType->String, pszRelationshipTypeUUID))
            {
                const auto psUUID = oTable.GetFieldValue(iUUID);
                if (psUUID)
                {
                    osUUID = psUUID->String;
                }

                auto asFields = oTable.GetAllFieldValues();

                if (!OGR_RawField_IsNull(&asFields[iDefinition]) &&
                    !OGR_RawField_IsUnset(&asFields[iDefinition]))
                {
                    CPLFree(asFields[iDefinition].String);
                }
                asFields[iDefinition].String = CPLStrdup(osXML.c_str());

                switch (relationship->GetCardinality())
                {
                    case GDALRelationshipCardinality::GRC_ONE_TO_ONE:
                        asFields[iDatasetSubtype1].Integer = 1;
                        break;
                    case GDALRelationshipCardinality::GRC_ONE_TO_MANY:
                        asFields[iDatasetSubtype1].Integer = 2;
                        break;
                    case GDALRelationshipCardinality::GRC_MANY_TO_MANY:
                        asFields[iDatasetSubtype1].Integer = 3;
                        break;
                    case GDALRelationshipCardinality::GRC_MANY_TO_ONE:
                        // unreachable
                        break;
                }

                bool bRet =
                    oTable.UpdateFeature(iCurFeat + 1, asFields, nullptr);
                oTable.FreeAllFieldValues(asFields);
                if (!bRet)
                    return false;
                bMatchFound = true;
                break;
            }
        }

        if (!oTable.Sync())
        {
            return false;
        }
    }

    if (!bMatchFound)
        return false;

    // First delete all existing item relationships for the item, and then we'll
    // rebuild them again.
    if (!RemoveRelationshipFromItemRelationships(osUUID))
    {
        failureReason =
            "Could not remove relationship from GDB_ItemRelationships";
        return false;
    }
    if (!RegisterRelationshipInItemRelationships(osUUID, osOriginUUID,
                                                 osDestinationUUID))
    {
        failureReason =
            "Could not register relationship in GDB_ItemRelationships";
        return false;
    }

    m_osMapRelationships[relationshipName] = std::move(relationship);

    return true;
}

/************************************************************************/
/*                        StartTransaction()                            */
/************************************************************************/

OGRErr OGROpenFileGDBDataSource::StartTransaction(int bForce)
{
    if (!bForce)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Transactions only supported in forced mode");
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    if (eAccess != GA_Update)
        return OGRERR_FAILURE;

    if (m_bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Transaction is already in progress");
        return OGRERR_FAILURE;
    }

    m_osTransactionBackupDirname =
        CPLFormFilename(m_osDirName.c_str(), ".ogrtransaction_backup", nullptr);
    VSIStatBufL sStat;
    if (VSIStatL(m_osTransactionBackupDirname.c_str(), &sStat) == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A previous backup directory %s already exists, which means "
                 "that a previous transaction was not cleanly committed or "
                 "rolled back.\n"
                 "Either manually restore the previous state from that "
                 "directory or remove it, before creating a new transaction.",
                 m_osTransactionBackupDirname.c_str());
        return OGRERR_FAILURE;
    }
    else if (VSIMkdir(m_osTransactionBackupDirname.c_str(), 0755) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create directory %s",
                 m_osTransactionBackupDirname.c_str());
        return OGRERR_FAILURE;
    }

    m_bInTransaction = true;
    return OGRERR_NONE;
}

/************************************************************************/
/*                   BackupSystemTablesForTransaction()                 */
/************************************************************************/

bool OGROpenFileGDBDataSource::BackupSystemTablesForTransaction()
{
    if (m_bSystemTablesBackedup)
        return true;

    char **papszFiles = VSIReadDir(m_osDirName.c_str());
    for (char **papszIter = papszFiles;
         papszIter != nullptr && *papszIter != nullptr; ++papszIter)
    {
        const std::string osBasename = CPLGetBasename(*papszIter);
        if (osBasename.size() == strlen("a00000001") &&
            osBasename.compare(0, 8, "a0000000") == 0 && osBasename[8] >= '1' &&
            osBasename[8] <= '8')
        {
            std::string osDestFilename = CPLFormFilename(
                m_osTransactionBackupDirname.c_str(), *papszIter, nullptr);
            std::string osSourceFilename =
                CPLFormFilename(m_osDirName.c_str(), *papszIter, nullptr);
            if (CPLCopyFile(osDestFilename.c_str(), osSourceFilename.c_str()) !=
                0)
            {
                CSLDestroy(papszFiles);
                return false;
            }
        }
    }

    CSLDestroy(papszFiles);
    m_bSystemTablesBackedup = true;
    return true;
}

/************************************************************************/
/*                        CommitTransaction()                           */
/************************************************************************/

OGRErr OGROpenFileGDBDataSource::CommitTransaction()
{
    if (!m_bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No transaction in progress");
        return OGRERR_FAILURE;
    }

    for (auto &poLayer : m_apoLayers)
        poLayer->CommitEmulatedTransaction();

    VSIRmdirRecursive(m_osTransactionBackupDirname.c_str());

    m_bInTransaction = false;
    m_bSystemTablesBackedup = false;
    m_oSetLayersCreatedInTransaction.clear();
    m_oSetLayersDeletedInTransaction.clear();

    return OGRERR_NONE;
}

/************************************************************************/
/*                       RollbackTransaction()                          */
/************************************************************************/

OGRErr OGROpenFileGDBDataSource::RollbackTransaction()
{
    if (!m_bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No transaction in progress");
        return OGRERR_FAILURE;
    }

    OGRErr eErr = OGRERR_NONE;

    // Restore system tables
    {
        char **papszFiles = VSIReadDir(m_osTransactionBackupDirname.c_str());
        if (papszFiles == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Backup directory %s no longer found! Original database "
                     "cannot be restored",
                     m_osTransactionBackupDirname.c_str());
            return OGRERR_FAILURE;
        }
        for (char **papszIter = papszFiles;
             papszIter != nullptr && *papszIter != nullptr; ++papszIter)
        {
            const std::string osBasename = CPLGetBasename(*papszIter);
            if (osBasename.size() == strlen("a00000001") &&
                osBasename.compare(0, 8, "a0000000") == 0 &&
                osBasename[8] >= '1' && osBasename[8] <= '8')
            {
                std::string osDestFilename =
                    CPLFormFilename(m_osDirName.c_str(), *papszIter, nullptr);
                std::string osSourceFilename = CPLFormFilename(
                    m_osTransactionBackupDirname.c_str(), *papszIter, nullptr);
                if (CPLCopyFile(osDestFilename.c_str(),
                                osSourceFilename.c_str()) != 0)
                {
                    eErr = OGRERR_FAILURE;
                }
            }
        }
        CSLDestroy(papszFiles);
    }

    // Restore layers in their original state
    for (auto &poLayer : m_apoLayers)
        poLayer->RollbackEmulatedTransaction();
    for (auto &poLayer : m_oSetLayersDeletedInTransaction)
        poLayer->RollbackEmulatedTransaction();

    // Remove layers created during transaction
    for (auto poLayer : m_oSetLayersCreatedInTransaction)
    {
        const std::string osThisBasename =
            CPLGetBasename(poLayer->GetFilename().c_str());
        poLayer->Close();

        char **papszFiles = VSIReadDir(m_osDirName.c_str());
        for (char **papszIter = papszFiles;
             papszIter != nullptr && *papszIter != nullptr; ++papszIter)
        {
            const std::string osBasename = CPLGetBasename(*papszIter);
            if (osBasename == osThisBasename)
            {
                std::string osDestFilename =
                    CPLFormFilename(m_osDirName.c_str(), *papszIter, nullptr);
                VSIUnlink(osDestFilename.c_str());
            }
        }
        CSLDestroy(papszFiles);
    }

    if (eErr == OGRERR_NONE)
    {
        if (VSIRmdirRecursive(m_osTransactionBackupDirname.c_str()) != 0)
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Backup directory %s could not be destroyed. But original "
                "dataset "
                "should have been properly restored. You will need to manually "
                "remove the backup directory.",
                m_osTransactionBackupDirname.c_str());
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Backup directory %s could not be properly restored onto "
                 "live database. Corruption is likely!",
                 m_osTransactionBackupDirname.c_str());
    }

    m_bInTransaction = false;
    m_bSystemTablesBackedup = false;
    m_oSetLayersCreatedInTransaction.clear();
    m_oSetLayersDeletedInTransaction.clear();

    return eErr;
}
