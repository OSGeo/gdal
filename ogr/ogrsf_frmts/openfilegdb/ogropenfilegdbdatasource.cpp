/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
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

#include "cpl_port.h"
#include "ogr_openfilegdb.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
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
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_mem.h"
#include "ogrsf_frmts.h"
#include "ogr_swq.h"

#include "filegdb_fielddomain.h"
#include "filegdb_relationship.h"

/***********************************************************************/
/*                      OGROpenFileGDBGroup                            */
/***********************************************************************/

class OGROpenFileGDBGroup final : public GDALGroup
{
  protected:
    friend class OGROpenFileGDBDataSource;
    std::vector<std::shared_ptr<GDALGroup>> m_apoSubGroups{};
    std::vector<OGRLayer *> m_apoLayers{};
    std::string m_osDefinition{};

  public:
    OGROpenFileGDBGroup(const std::string &osParentName, const char *pszName)
        : GDALGroup(osParentName, pszName)
    {
    }

    void SetDefinition(const std::string &osDefinition)
    {
        m_osDefinition = osDefinition;
    }

    const std::string &GetDefinition() const
    {
        return m_osDefinition;
    }

    std::vector<std::string>
    GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup>
    OpenGroup(const std::string &osName,
              CSLConstList papszOptions) const override;

    std::vector<std::string>
    GetVectorLayerNames(CSLConstList papszOptions) const override;
    OGRLayer *OpenVectorLayer(const std::string &osName,
                              CSLConstList papszOptions) const override;
};

/************************************************************************/
/*                      OGROpenFileGDBDataSource()                      */
/************************************************************************/
OGROpenFileGDBDataSource::OGROpenFileGDBDataSource()
    : m_papszFiles(nullptr), bLastSQLUsedOptimizedImplementation(false)
{
}

/************************************************************************/
/*                     ~OGROpenFileGDBDataSource()                      */
/************************************************************************/
OGROpenFileGDBDataSource::~OGROpenFileGDBDataSource()

{
    OGROpenFileGDBDataSource::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr OGROpenFileGDBDataSource::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (m_bInTransaction)
            OGROpenFileGDBDataSource::RollbackTransaction();

        if (OGROpenFileGDBDataSource::FlushCache(true) != CE_None)
            eErr = CE_Failure;

        m_apoLayers.clear();
        m_apoHiddenLayers.clear();
        CSLDestroy(m_papszFiles);

        if (GDALDataset::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                             FileExists()                             */
/************************************************************************/

int OGROpenFileGDBDataSource::FileExists(const char *pszFilename)
{
    if (m_papszFiles)
        return CSLFindString(m_papszFiles, CPLGetFilename(pszFilename)) >= 0;

    VSIStatBufL sStat;
    CPLString osFilename(pszFilename);
    return VSIStatExL(osFilename, &sStat, VSI_STAT_EXISTS_FLAG) == 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

bool OGROpenFileGDBDataSource::Open(const GDALOpenInfo *poOpenInfo,
                                    bool &bRetryFileGDBOut)
{
    bRetryFileGDBOut = false;

    if (poOpenInfo->nOpenFlags == (GDAL_OF_RASTER | GDAL_OF_UPDATE))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Update mode of rasters is not supported");
        return false;
    }

    m_osDirName = poOpenInfo->pszFilename;

    std::string osRasterLayerName;
    if (STARTS_WITH(poOpenInfo->pszFilename, "OpenFileGDB:"))
    {
        const CPLStringList aosTokens(CSLTokenizeString2(
            poOpenInfo->pszFilename, ":", CSLT_HONOURSTRINGS));
        if (aosTokens.size() == 4 && strlen(aosTokens[1]) == 1)
        {
            m_osDirName = aosTokens[1];
            m_osDirName += ':';
            m_osDirName += aosTokens[2];
            osRasterLayerName = aosTokens[3];
        }
        else if (aosTokens.size() == 3)
        {
            m_osDirName = aosTokens[1];
            osRasterLayerName = aosTokens[2];
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid connection string");
            return false;
        }
    }

    FileGDBTable oTable;

    // Whether to open directly a given .gdbtable file (mostly for debugging
    // purposes)
    int nInterestTable = 0;
    unsigned int unInterestTable = 0;
    const char *pszFilenameWithoutPath = CPLGetFilename(m_osDirName.c_str());
    if (poOpenInfo->nHeaderBytes > 0 &&
        strlen(pszFilenameWithoutPath) == strlen("a00000000.gdbtable") &&
        pszFilenameWithoutPath[0] == 'a' &&
        EQUAL(pszFilenameWithoutPath + strlen("a00000000"), ".gdbtable") &&
        sscanf(pszFilenameWithoutPath, "a%08x.gdbtable", &unInterestTable) == 1)
    {
        nInterestTable = static_cast<int>(unInterestTable);
        m_osDirName = CPLGetPath(m_osDirName);
    }

    if (EQUAL(CPLGetExtension(m_osDirName), "zip") &&
        !STARTS_WITH(m_osDirName, "/vsizip/"))
    {
        m_osDirName = "/vsizip/" + m_osDirName;
    }
    else if (EQUAL(CPLGetExtension(m_osDirName), "tar") &&
             !STARTS_WITH(m_osDirName, "/vsitar/"))
    {
        m_osDirName = "/vsitar/" + m_osDirName;
    }

    if (STARTS_WITH(m_osDirName, "/vsizip/") ||
        STARTS_WITH(m_osDirName, "/vsitar/"))
    {
        /* Look for one subdirectory ending with .gdb extension */
        char **papszDir = VSIReadDir(m_osDirName);
        int iCandidate = -1;
        for (int i = 0; papszDir && papszDir[i] != nullptr; i++)
        {
            VSIStatBufL sStat;
            if (EQUAL(CPLGetExtension(papszDir[i]), "gdb") &&
                VSIStatL(CPLSPrintf("%s/%s", m_osDirName.c_str(), papszDir[i]),
                         &sStat) == 0 &&
                VSI_ISDIR(sStat.st_mode))
            {
                if (iCandidate < 0)
                    iCandidate = i;
                else
                {
                    iCandidate = -1;
                    break;
                }
            }
        }
        if (iCandidate >= 0)
        {
            m_osDirName += "/";
            m_osDirName += papszDir[iCandidate];
        }
        CSLDestroy(papszDir);
    }

    const std::string osTransactionBackupDirname =
        CPLFormFilename(m_osDirName.c_str(), ".ogrtransaction_backup", nullptr);
    VSIStatBufL sStat;
    if (VSIStatL(osTransactionBackupDirname.c_str(), &sStat) == 0)
    {
        if (poOpenInfo->eAccess == GA_Update)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "A previous backup directory %s already exists, which means "
                "that a previous transaction was not cleanly committed or "
                "rolled back.\n"
                "Either manually restore the previous state from that "
                "directory or remove it, before opening in update mode.",
                osTransactionBackupDirname.c_str());
            return false;
        }
        else
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "A previous backup directory %s already exists, which means "
                "that a previous transaction was not cleanly committed or "
                "rolled back.\n"
                "You may want to restore the previous state from that "
                "directory or remove it.",
                osTransactionBackupDirname.c_str());
        }
    }

    m_papszFiles = VSIReadDir(m_osDirName);

    /* Explore catalog table */
    m_osGDBSystemCatalogFilename =
        CPLFormFilename(m_osDirName, "a00000001", "gdbtable");
    if (!FileExists(m_osGDBSystemCatalogFilename.c_str()) ||
        !oTable.Open(m_osGDBSystemCatalogFilename.c_str(),
                     poOpenInfo->eAccess == GA_Update))
    {
        if (nInterestTable > 0 && FileExists(poOpenInfo->pszFilename))
        {
            const char *pszLyrName = CPLSPrintf("a%08x", nInterestTable);
            auto poLayer = std::make_unique<OGROpenFileGDBLayer>(
                this, poOpenInfo->pszFilename, pszLyrName, "", "",
                eAccess == GA_Update);
            const char *pszTablX =
                CPLResetExtension(poOpenInfo->pszFilename, "gdbtablx");
            if ((!FileExists(pszTablX) &&
                 poLayer->GetLayerDefn()->GetFieldCount() == 0 &&
                 poLayer->GetFeatureCount() == 0) ||
                !poLayer->IsValidLayerDefn())
            {
                return false;
            }
            m_apoLayers.push_back(std::move(poLayer));
            return true;
        }
        return false;
    }

    const int idxName = oTable.GetFieldIdx("Name");
    const int idxFileformat = oTable.GetFieldIdx("FileFormat");
    if (!(idxName >= 0 && idxFileformat >= 0 &&
          oTable.GetField(idxName)->GetType() == FGFT_STRING &&
          (oTable.GetField(idxFileformat)->GetType() == FGFT_INT16 ||
           oTable.GetField(idxFileformat)->GetType() == FGFT_INT32)))
    {
        return false;
    }

    int iGDBItems = -1;          /* V10 */
    int iGDBFeatureClasses = -1; /* V9.X */
    int iGDBObjectClasses = -1;  /* V9.X */

    std::vector<std::string> aosTableNames;
    try
    {
        for (int i = 0; i < oTable.GetTotalRecordCount(); i++)
        {
            if (!oTable.SelectRow(i))
            {
                if (oTable.HasGotError())
                    break;
                aosTableNames.push_back("");
                continue;
            }

            const OGRField *psField = oTable.GetFieldValue(idxName);
            if (psField != nullptr)
            {
                aosTableNames.push_back(psField->String);
                if (strcmp(psField->String, "GDB_SpatialRefs") == 0)
                {
                    // Normally a00000003
                    m_osGDBSpatialRefsFilename = CPLFormFilename(
                        m_osDirName, CPLSPrintf("a%08x.gdbtable", i + 1),
                        nullptr);
                }
                else if (strcmp(psField->String, "GDB_Items") == 0)
                {
                    // Normally a00000004 but it has been seen in some datasets
                    // to not be a00000004
                    m_osGDBItemsFilename = CPLFormFilename(
                        m_osDirName, CPLSPrintf("a%08x.gdbtable", i + 1),
                        nullptr);
                    iGDBItems = i;
                }
                else if (strcmp(psField->String, "GDB_ItemRelationships") == 0)
                {
                    // Normally a00000006 but can be a00000005 sometimes (e.g
                    // autotest/ogr/data/filegdb/Domains.gdb)
                    m_osGDBItemRelationshipsFilename = CPLFormFilename(
                        m_osDirName, CPLSPrintf("a%08x.gdbtable", i + 1),
                        nullptr);
                }
                else if (strcmp(psField->String, "GDB_FeatureClasses") == 0)
                {
                    // FileGDB v9
                    iGDBFeatureClasses = i;
                }
                else if (strcmp(psField->String, "GDB_ObjectClasses") == 0)
                {
                    // FileGDB v9
                    iGDBObjectClasses = i;
                }
                m_osMapNameToIdx[psField->String] = 1 + i;
            }
            else
            {
                aosTableNames.push_back("");
            }
        }
    }
    catch (const std::exception &)
    {
        return false;
    }

    oTable.Close();

    std::set<int> oSetIgnoredRasterLayerTableNum;
    if (iGDBItems >= 0)
    {
        eAccess = poOpenInfo->eAccess;

        const bool bRet = OpenFileGDBv10(
            iGDBItems, nInterestTable, poOpenInfo, osRasterLayerName,
            oSetIgnoredRasterLayerTableNum, bRetryFileGDBOut);
        if (!bRet)
            return false;
    }
    else if (iGDBFeatureClasses >= 0 && iGDBObjectClasses >= 0)
    {
        if (poOpenInfo->eAccess == GA_Update)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Edition of existing FileGDB v9 datasets not supported");
            return false;
        }

        int bRet = OpenFileGDBv9(iGDBFeatureClasses, iGDBObjectClasses,
                                 nInterestTable, poOpenInfo, osRasterLayerName,
                                 oSetIgnoredRasterLayerTableNum);
        if (!bRet)
            return false;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No GDB_Items nor GDB_FeatureClasses table");
        return false;
    }

    if (m_apoLayers.empty() && nInterestTable > 0)
    {
        if (FileExists(poOpenInfo->pszFilename))
        {
            const char *pszLyrName = nullptr;
            if (nInterestTable <= static_cast<int>(aosTableNames.size()) &&
                !aosTableNames[nInterestTable - 1].empty())
                pszLyrName = aosTableNames[nInterestTable - 1].c_str();
            else
                pszLyrName = CPLSPrintf("a%08x", nInterestTable);
            m_apoLayers.push_back(std::make_unique<OGROpenFileGDBLayer>(
                this, poOpenInfo->pszFilename, pszLyrName, "", "",
                eAccess == GA_Update));
        }
        else
        {
            return false;
        }
    }

    if (nInterestTable == 0)
    {
        const bool bListAllTables = CPLTestBool(CSLFetchNameValueDef(
            poOpenInfo->papszOpenOptions, "LIST_ALL_TABLES", "NO"));

        // add additional tables which are not present in the
        // GDB_Items/GDB_FeatureClasses/GDB_ObjectClasses tables
        for (const auto &oIter : m_osMapNameToIdx)
        {
            // test if layer is already added
            if (OGRDataSource::GetLayerByName(oIter.first.c_str()))
                continue;

            if (bListAllTables || !IsPrivateLayerName(oIter.first))
            {
                const int idx = oIter.second;
                CPLString osFilename(CPLFormFilename(
                    m_osDirName, CPLSPrintf("a%08x", idx), "gdbtable"));
                if (oSetIgnoredRasterLayerTableNum.find(idx) ==
                        oSetIgnoredRasterLayerTableNum.end() &&
                    FileExists(osFilename))
                {
                    m_apoLayers.emplace_back(
                        std::make_unique<OGROpenFileGDBLayer>(
                            this, osFilename, oIter.first.c_str(), "", "",
                            eAccess == GA_Update));
                    if (m_poRootGroup)
                    {
                        cpl::down_cast<OGROpenFileGDBGroup *>(
                            m_poRootGroup.get())
                            ->m_apoLayers.emplace_back(
                                m_apoLayers.back().get());
                    }
                }
            }
        }
    }

    if (!osRasterLayerName.empty())
    {
        m_aosSubdatasets.Clear();
        SetDescription(poOpenInfo->pszFilename);
        return nBands != 0;
    }

    // If opening in raster-only mode, return false if there are no raster
    // layers
    if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 &&
        (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) == 0)
    {
        if (m_aosSubdatasets.empty())
        {
            return false;
        }
        else if (m_aosSubdatasets.size() == 2)
        {
            // If there is a single raster dataset, open it right away.
            return true;
        }
    }
    // If opening in vector-only mode, return false if there are no vector
    // layers and opened in read-only mode
    else if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
             (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0)
    {
        if (m_apoLayers.empty() && poOpenInfo->eAccess == GA_ReadOnly)
        {
            return false;
        }
    }

    return true;
}

/***********************************************************************/
/*                             AddLayer()                              */
/***********************************************************************/

OGRLayer *OGROpenFileGDBDataSource::AddLayer(
    const CPLString &osName, int nInterestTable, int &nCandidateLayers,
    int &nLayersSDCOrCDF, const CPLString &osDefinition,
    const CPLString &osDocumentation, OGRwkbGeometryType eGeomType,
    const std::string &osParentDefinition)
{
    std::map<std::string, int>::const_iterator oIter =
        m_osMapNameToIdx.find(osName);
    int idx = 0;
    if (oIter != m_osMapNameToIdx.end())
        idx = oIter->second;
    if (idx > 0 && (nInterestTable <= 0 || nInterestTable == idx))
    {
        m_osMapNameToIdx.erase(osName);

        CPLString osFilename =
            CPLFormFilename(m_osDirName, CPLSPrintf("a%08x", idx), "gdbtable");
        if (FileExists(osFilename))
        {
            nCandidateLayers++;

            if (m_papszFiles != nullptr)
            {
                CPLString osSDC = CPLResetExtension(osFilename, "gdbtable.sdc");
                CPLString osCDF = CPLResetExtension(osFilename, "gdbtable.cdf");
                if (FileExists(osSDC) || FileExists(osCDF))
                {
                    nLayersSDCOrCDF++;
                    if (GDALGetDriverByName("FileGDB") == nullptr)
                    {
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "%s layer has a %s file whose format is unhandled",
                            osName.c_str(),
                            FileExists(osSDC) ? osSDC.c_str() : osCDF.c_str());
                    }
                    else
                    {
                        CPLDebug(
                            "OpenFileGDB",
                            "%s layer has a %s file whose format is unhandled",
                            osName.c_str(),
                            FileExists(osSDC) ? osSDC.c_str() : osCDF.c_str());
                    }
                    return nullptr;
                }
            }

            m_apoLayers.emplace_back(std::make_unique<OGROpenFileGDBLayer>(
                this, osFilename, osName, osDefinition, osDocumentation,
                eAccess == GA_Update, eGeomType, osParentDefinition));
            return m_apoLayers.back().get();
        }
    }
    return nullptr;
}

std::vector<std::string> OGROpenFileGDBGroup::GetGroupNames(CSLConstList) const
{
    std::vector<std::string> ret;
    for (const auto &poSubGroup : m_apoSubGroups)
        ret.emplace_back(poSubGroup->GetName());
    return ret;
}

std::shared_ptr<GDALGroup>
OGROpenFileGDBGroup::OpenGroup(const std::string &osName, CSLConstList) const
{
    for (const auto &poSubGroup : m_apoSubGroups)
    {
        if (poSubGroup->GetName() == osName)
            return poSubGroup;
    }
    return nullptr;
}

std::vector<std::string>
OGROpenFileGDBGroup::GetVectorLayerNames(CSLConstList) const
{
    std::vector<std::string> ret;
    for (const auto &poLayer : m_apoLayers)
        ret.emplace_back(poLayer->GetName());
    return ret;
}

OGRLayer *OGROpenFileGDBGroup::OpenVectorLayer(const std::string &osName,
                                               CSLConstList) const
{
    for (const auto &poLayer : m_apoLayers)
    {
        if (poLayer->GetName() == osName)
            return poLayer;
    }
    return nullptr;
}

/***********************************************************************/
/*                         OpenFileGDBv10()                            */
/***********************************************************************/

bool OGROpenFileGDBDataSource::OpenFileGDBv10(
    int iGDBItems, int nInterestTable, const GDALOpenInfo *poOpenInfo,
    const std::string &osRasterLayerName,
    std::set<int> &oSetIgnoredRasterLayerTableNum, bool &bRetryFileGDBOut)
{

    CPLDebug("OpenFileGDB", "FileGDB v10 or later");

    FileGDBTable oTable;

    CPLString osFilename(CPLFormFilename(
        m_osDirName, CPLSPrintf("a%08x.gdbtable", iGDBItems + 1), nullptr));

    // Normally we don't need to update in update mode the GDB_Items table,
    // but this may help repairing it, if we have corrupted it with past
    // GDAL versions.
    const bool bOpenInUpdateMode =
        (poOpenInfo->nOpenFlags & GDAL_OF_UPDATE) != 0;
    if (!oTable.Open(osFilename, bOpenInUpdateMode))
        return false;

    const int iUUID = oTable.GetFieldIdx("UUID");
    const int iType = oTable.GetFieldIdx("Type");
    const int iName = oTable.GetFieldIdx("Name");
    const int iPath = oTable.GetFieldIdx("Path");
    const int iDefinition = oTable.GetFieldIdx("Definition");
    const int iDocumentation = oTable.GetFieldIdx("Documentation");
    if (iUUID < 0 || iType < 0 || iName < 0 || iPath < 0 || iDefinition < 0 ||
        iDocumentation < 0 ||
        oTable.GetField(iUUID)->GetType() != FGFT_GLOBALID ||
        oTable.GetField(iType)->GetType() != FGFT_GUID ||
        oTable.GetField(iName)->GetType() != FGFT_STRING ||
        oTable.GetField(iPath)->GetType() != FGFT_STRING ||
        oTable.GetField(iDefinition)->GetType() != FGFT_XML ||
        oTable.GetField(iDocumentation)->GetType() != FGFT_XML)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Wrong structure for GDB_Items table");
        return false;
    }

    auto poRootGroup = std::make_shared<OGROpenFileGDBGroup>(std::string(), "");
    m_poRootGroup = poRootGroup;
    std::map<std::string, std::shared_ptr<OGROpenFileGDBGroup>>
        oMapPathToFeatureDataset;

    // First pass to collect FeatureDatasets
    for (int i = 0; i < oTable.GetTotalRecordCount(); i++)
    {
        if (!oTable.SelectRow(i))
        {
            if (oTable.HasGotError())
                break;
            continue;
        }

        CPLString osType;
        const OGRField *psTypeField = oTable.GetFieldValue(iType);
        if (psTypeField != nullptr)
        {
            const char *pszType = psTypeField->String;
            if (pszType)
                osType = pszType;
        }

        std::string osPath;
        const OGRField *psField = oTable.GetFieldValue(iPath);
        if (psField != nullptr)
        {
            const char *pszPath = psField->String;
            if (pszPath)
                osPath = pszPath;
        }

        if (osPath == "\\")
        {
            psField = oTable.GetFieldValue(iUUID);
            if (psField != nullptr)
            {
                const char *pszUUID = psField->String;
                if (pszUUID)
                    m_osRootGUID = pszUUID;
            }
        }

        psField = oTable.GetFieldValue(iDefinition);
        if (psField != nullptr &&
            strstr(psField->String, "DEFeatureDataset") != nullptr)
        {
            const std::string osDefinition(psField->String);

            std::string osName;
            psField = oTable.GetFieldValue(iName);
            if (psField != nullptr)
            {
                const char *pszName = psField->String;
                if (pszName)
                    osName = pszName;
            }

            if (!osName.empty() && !osPath.empty())
            {
                auto poSubGroup = std::make_shared<OGROpenFileGDBGroup>(
                    poRootGroup->GetName(), osName.c_str());
                poSubGroup->SetDefinition(osDefinition);
                oMapPathToFeatureDataset[osPath] = poSubGroup;
                poRootGroup->m_apoSubGroups.emplace_back(poSubGroup);
            }
        }
        else if (psField != nullptr &&
                 osType.tolower().compare(pszRelationshipTypeUUID) == 0)
        {
            // relationship item
            auto poRelationship = ParseXMLRelationshipDef(psField->String);
            if (poRelationship)
            {
                const auto &relationshipName = poRelationship->GetName();
                m_osMapRelationships.insert(std::pair(
                    std::string(relationshipName), std::move(poRelationship)));
            }
        }
    }

    // Now collect layers
    int nCandidateLayers = 0;
    int nLayersSDCOrCDF = 0;
    bool bRet = true;
    for (int i = 0; i < oTable.GetTotalRecordCount(); i++)
    {
        if (!oTable.SelectRow(i))
        {
            if (oTable.HasGotError())
                break;
            continue;
        }

        const OGRField *psField = oTable.GetFieldValue(iDefinition);
        if (psField && (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 &&
            (strstr(psField->String, "DEFeatureClassInfo") != nullptr ||
             strstr(psField->String, "DETableInfo") != nullptr))
        {
            CPLString osDefinition(psField->String);

            psField = oTable.GetFieldValue(iDocumentation);
            CPLString osDocumentation(psField != nullptr ? psField->String
                                                         : "");

            std::shared_ptr<OGROpenFileGDBGroup> poParent;
            psField = oTable.GetFieldValue(iPath);
            if (psField != nullptr)
            {
                const char *pszPath = psField->String;
                if (pszPath)
                {
                    std::string osPath(pszPath);
                    const auto nPos = osPath.rfind('\\');
                    if (nPos != 0 && nPos != std::string::npos)
                    {
                        std::string osPathParent = osPath.substr(0, nPos);
                        const auto oIter =
                            oMapPathToFeatureDataset.find(osPathParent);
                        if (oIter == oMapPathToFeatureDataset.end())
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Cannot find feature dataset of "
                                     "path %s referenced by table %s",
                                     osPathParent.c_str(), pszPath);
                        }
                        else
                        {
                            poParent = oIter->second;
                        }
                    }
                }
            }

            psField = oTable.GetFieldValue(iName);
            if (psField != nullptr)
            {
                OGRLayer *poLayer = AddLayer(
                    psField->String, nInterestTable, nCandidateLayers,
                    nLayersSDCOrCDF, osDefinition, osDocumentation, wkbUnknown,
                    poParent ? poParent->GetDefinition() : std::string());

                if (poLayer)
                {
                    if (poParent)
                    {
                        // Add the layer to the group of the corresponding
                        // feature dataset
                        poParent->m_apoLayers.emplace_back(poLayer);
                    }
                    else
                    {
                        poRootGroup->m_apoLayers.emplace_back(poLayer);
                    }
                }
            }
        }
        else if (psField && (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 &&
                 (strstr(psField->String, "GPCodedValueDomain2") != nullptr ||
                  strstr(psField->String, "GPRangeDomain2") != nullptr))
        {
            auto poDomain = ParseXMLFieldDomainDef(psField->String);
            if (poDomain)
            {
                const auto &domainName = poDomain->GetName();
                m_oMapFieldDomains.insert(
                    std::pair(std::string(domainName), std::move(poDomain)));
            }
        }
        else if (psField && (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0 &&
                 strstr(psField->String, "DERasterDataset"))
        {
            const std::string osDefinition(psField->String);

            psField = oTable.GetFieldValue(iName);
            if (psField)
            {
                const std::string osLayerName(psField->String);

                psField = oTable.GetFieldValue(iDocumentation);
                const std::string osDocumentation(psField ? psField->String
                                                          : "");

                if (!osRasterLayerName.empty())
                {
                    if (osRasterLayerName == osLayerName)
                    {
                        bRet = OpenRaster(poOpenInfo, osLayerName, osDefinition,
                                          osDocumentation);
                    }
                }
                else
                {
                    const int iSubDSNum = 1 + m_aosSubdatasets.size() / 2;
                    m_aosSubdatasets.SetNameValue(
                        CPLSPrintf("SUBDATASET_%d_NAME", iSubDSNum),
                        CPLSPrintf("OpenFileGDB:\"%s\":%s",
                                   poOpenInfo->pszFilename,
                                   osLayerName.c_str()));

                    std::string osDesc(osLayerName);
                    if (!osDocumentation.empty())
                    {
                        CPLXMLTreeCloser psTree(
                            CPLParseXMLString(osDocumentation.c_str()));
                        if (psTree)
                        {
                            const auto psRastInfo = CPLGetXMLNode(
                                psTree.get(), "=metadata.spdoinfo.rastinfo");
                            if (psRastInfo)
                            {
                                const char *pszRowCount = CPLGetXMLValue(
                                    psRastInfo, "rowcount", nullptr);
                                const char *pszColCount = CPLGetXMLValue(
                                    psRastInfo, "colcount", nullptr);
                                const char *pszRastBand = CPLGetXMLValue(
                                    psRastInfo, "rastband", nullptr);
                                const char *pszRastBPP = CPLGetXMLValue(
                                    psRastInfo, "rastbpp", nullptr);
                                if (pszRowCount && pszColCount && pszRastBand &&
                                    pszRastBPP)
                                {
                                    osDesc += CPLSPrintf(
                                        " [%sx%sx%s], %s bits", pszColCount,
                                        pszRowCount, pszRastBand, pszRastBPP);
                                }
                            }
                        }
                    }

                    m_aosSubdatasets.SetNameValue(
                        CPLSPrintf("SUBDATASET_%d_DESC", iSubDSNum),
                        ("Raster " + osDesc).c_str());
                }
            }
        }
        else if (psField && (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
                 strstr(psField->String, "DERasterDataset"))
        {
            psField = oTable.GetFieldValue(iName);
            if (psField)
            {
                const std::string osLayerName(psField->String);
                auto oIter = m_osMapNameToIdx.find(osLayerName);
                if (oIter != m_osMapNameToIdx.end())
                {
                    oSetIgnoredRasterLayerTableNum.insert(oIter->second);

                    for (const char *pszPrefix :
                         {"fras_ras_", "fras_aux_", "fras_bnd_", "fras_blk_"})
                    {
                        oIter = m_osMapNameToIdx.find(
                            std::string(pszPrefix).append(osLayerName).c_str());
                        if (oIter != m_osMapNameToIdx.end())
                        {
                            oSetIgnoredRasterLayerTableNum.insert(
                                oIter->second);
                        }
                    }
                }
            }
        }
    }

    // If there's at least one .cdf layer and the FileGDB driver is present,
    // retry with it.
    if (nLayersSDCOrCDF > 0 && GDALGetDriverByName("FileGDB") != nullptr)
    {
        bRetryFileGDBOut = true;
        return false;
    }

    return bRet;
}

/***********************************************************************/
/*                         OpenFileGDBv9()                             */
/***********************************************************************/

int OGROpenFileGDBDataSource::OpenFileGDBv9(
    int iGDBFeatureClasses, int iGDBObjectClasses, int nInterestTable,
    const GDALOpenInfo *poOpenInfo, const std::string &osRasterLayerName,
    std::set<int> &oSetIgnoredRasterLayerTableNum)
{
    auto poTable = std::make_unique<FileGDBTable>();

    CPLDebug("OpenFileGDB", "FileGDB v9");

    /* Fetch names of layers */
    CPLString osFilename(CPLFormFilename(
        m_osDirName, CPLSPrintf("a%08x", iGDBObjectClasses + 1), "gdbtable"));
    if (!poTable->Open(osFilename, false))
        return FALSE;

    int iName = poTable->GetFieldIdx("Name");
    int iCLSID = poTable->GetFieldIdx("CLSID");
    if (iName < 0 || poTable->GetField(iName)->GetType() != FGFT_STRING ||
        iCLSID < 0 || poTable->GetField(iCLSID)->GetType() != FGFT_STRING)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Wrong structure for GDB_ObjectClasses table");
        return FALSE;
    }

    std::vector<std::string> aosName;
    int nCandidateLayers = 0, nLayersSDCOrCDF = 0;
    for (int i = 0; i < poTable->GetTotalRecordCount(); i++)
    {
        if (!poTable->SelectRow(i))
        {
            if (poTable->HasGotError())
                break;
            aosName.push_back("");
            continue;
        }

        const OGRField *psField = poTable->GetFieldValue(iName);
        if (psField != nullptr)
        {
            std::string osName(psField->String);
            psField = poTable->GetFieldValue(iCLSID);
            if (psField != nullptr)
            {
                /* Is it a non-spatial table ? */
                if (strcmp(psField->String,
                           "{7A566981-C114-11D2-8A28-006097AFF44E}") == 0)
                {
                    aosName.push_back("");
                    AddLayer(osName, nInterestTable, nCandidateLayers,
                             nLayersSDCOrCDF, "", "", wkbNone, std::string());
                }
                else
                {
                    /* We should perhaps also check that the CLSID is the one of
                     * a spatial table */
                    aosName.push_back(osName);
                }
            }
        }
    }
    poTable->Close();

    poTable = std::make_unique<FileGDBTable>();

    /* Find tables that are spatial layers */
    osFilename = CPLFormFilename(
        m_osDirName, CPLSPrintf("a%08x", iGDBFeatureClasses + 1), "gdbtable");
    if (!poTable->Open(osFilename, false))
        return FALSE;

    const int iObjectClassID = poTable->GetFieldIdx("ObjectClassID");
    const int iFeatureType = poTable->GetFieldIdx("FeatureType");
    const int iGeometryType = poTable->GetFieldIdx("GeometryType");
    if (iObjectClassID < 0 || iGeometryType < 0 || iFeatureType < 0 ||
        poTable->GetField(iObjectClassID)->GetType() != FGFT_INT32 ||
        poTable->GetField(iFeatureType)->GetType() != FGFT_INT32 ||
        poTable->GetField(iGeometryType)->GetType() != FGFT_INT32)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Wrong structure for GDB_FeatureClasses table");
        return FALSE;
    }

    bool bRet = true;
    for (int i = 0; i < poTable->GetTotalRecordCount(); i++)
    {
        if (!poTable->SelectRow(i))
        {
            if (poTable->HasGotError())
                break;
            continue;
        }

        const OGRField *psField = poTable->GetFieldValue(iGeometryType);
        if (psField == nullptr)
            continue;
        const int nGeomType = psField->Integer;
        OGRwkbGeometryType eGeomType = wkbUnknown;
        switch (nGeomType)
        {
            case FGTGT_NONE: /* doesn't make sense ! */
                break;
            case FGTGT_POINT:
                eGeomType = wkbPoint;
                break;
            case FGTGT_MULTIPOINT:
                eGeomType = wkbMultiPoint;
                break;
            case FGTGT_LINE:
                eGeomType = wkbMultiLineString;
                break;
            case FGTGT_POLYGON:
                eGeomType = wkbMultiPolygon;
                break;
            case FGTGT_MULTIPATCH:
                eGeomType = wkbUnknown;
                break;
        }

        psField = poTable->GetFieldValue(iObjectClassID);
        if (psField == nullptr)
            continue;

        const int idx = psField->Integer;
        if (idx > 0 && idx <= static_cast<int>(aosName.size()) &&
            !aosName[idx - 1].empty())
        {
            const std::string osName(aosName[idx - 1]);

            psField = poTable->GetFieldValue(iFeatureType);
            const bool bIsRaster = psField && psField->Integer == 14;

            if (bIsRaster && (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0)
            {
                if (!osRasterLayerName.empty())
                {
                    if (osRasterLayerName == osName)
                    {
                        bRet = OpenRaster(poOpenInfo, osName, "", "");
                    }
                }
                else
                {
                    const int iSubDSNum = 1 + m_aosSubdatasets.size() / 2;
                    m_aosSubdatasets.SetNameValue(
                        CPLSPrintf("SUBDATASET_%d_NAME", iSubDSNum),
                        CPLSPrintf("OpenFileGDB:\"%s\":%s",
                                   poOpenInfo->pszFilename, osName.c_str()));
                    m_aosSubdatasets.SetNameValue(
                        CPLSPrintf("SUBDATASET_%d_DESC", iSubDSNum),
                        ("Raster " + osName).c_str());
                }
            }
            else if (bIsRaster)
            {
                auto oIter = m_osMapNameToIdx.find(osName);
                if (oIter != m_osMapNameToIdx.end())
                {
                    oSetIgnoredRasterLayerTableNum.insert(oIter->second);

                    for (const char *pszPrefix :
                         {"fras_ras_", "fras_aux_", "fras_bnd_", "fras_blk_"})
                    {
                        oIter = m_osMapNameToIdx.find(
                            std::string(pszPrefix).append(osName).c_str());
                        if (oIter != m_osMapNameToIdx.end())
                        {
                            oSetIgnoredRasterLayerTableNum.insert(
                                oIter->second);
                        }
                    }
                }
            }
            else
            {
                AddLayer(osName, nInterestTable, nCandidateLayers,
                         nLayersSDCOrCDF, "", "", eGeomType, std::string());
            }
        }
    }

    if (m_apoLayers.empty() && nCandidateLayers > 0 &&
        nCandidateLayers == nLayersSDCOrCDF)
        return FALSE;

    return bRet;
}

/***********************************************************************/
/*                         TestCapability()                            */
/***********************************************************************/

int OGROpenFileGDBDataSource::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, ODsCCreateLayer) || EQUAL(pszCap, ODsCDeleteLayer) ||
        EQUAL(pszCap, ODsCAddFieldDomain) ||
        EQUAL(pszCap, ODsCDeleteFieldDomain) ||
        EQUAL(pszCap, ODsCUpdateFieldDomain) ||
        EQUAL(pszCap, GDsCAddRelationship) ||
        EQUAL(pszCap, GDsCDeleteRelationship) ||
        EQUAL(pszCap, GDsCUpdateRelationship) ||
        EQUAL(pszCap, ODsCEmulatedTransactions))
    {
        return eAccess == GA_Update;
    }

    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCCurveGeometries))
        return TRUE;

    return FALSE;
}

/***********************************************************************/
/*                            GetLayer()                               */
/***********************************************************************/

OGRLayer *OGROpenFileGDBDataSource::GetLayer(int iIndex)
{
    if (iIndex < 0 || iIndex >= static_cast<int>(m_apoLayers.size()))
        return nullptr;
    return m_apoLayers[iIndex].get();
}

/***********************************************************************/
/*                      BuildLayerFromName()                           */
/***********************************************************************/

std::unique_ptr<OGROpenFileGDBLayer>
OGROpenFileGDBDataSource::BuildLayerFromName(const char *pszName)
{

    std::map<std::string, int>::const_iterator oIter =
        m_osMapNameToIdx.find(pszName);
    if (oIter != m_osMapNameToIdx.end())
    {
        int idx = oIter->second;
        CPLString osFilename(
            CPLFormFilename(m_osDirName, CPLSPrintf("a%08x", idx), "gdbtable"));
        if (FileExists(osFilename))
        {
            return std::make_unique<OGROpenFileGDBLayer>(
                this, osFilename, pszName, "", "", eAccess == GA_Update);
        }
    }
    return nullptr;
}

/***********************************************************************/
/*                          GetLayerByName()                           */
/***********************************************************************/

OGROpenFileGDBLayer *
OGROpenFileGDBDataSource::GetLayerByName(const char *pszName)
{
    for (auto &poLayer : m_apoLayers)
    {
        if (EQUAL(poLayer->GetName(), pszName))
            return poLayer.get();
    }

    for (auto &poLayer : m_apoHiddenLayers)
    {
        if (EQUAL(poLayer->GetName(), pszName))
            return poLayer.get();
    }

    auto poLayer = BuildLayerFromName(pszName);
    if (poLayer)
    {
        m_apoHiddenLayers.emplace_back(std::move(poLayer));
        return m_apoHiddenLayers.back().get();
    }
    return nullptr;
}

/***********************************************************************/
/*                          IsPrivateLayerName()                       */
/***********************************************************************/

bool OGROpenFileGDBDataSource::IsPrivateLayerName(const CPLString &osName)
{
    const CPLString osLCTableName(CPLString(osName).tolower());

    // tables beginning with "GDB_" are private tables
    // Also consider "VAT_" tables as private ones.
    return osLCTableName.size() >= 4 && (osLCTableName.substr(0, 4) == "gdb_" ||
                                         osLCTableName.substr(0, 4) == "vat_");
}

/***********************************************************************/
/*                          IsLayerPrivate()                           */
/***********************************************************************/

bool OGROpenFileGDBDataSource::IsLayerPrivate(int iLayer) const
{
    if (iLayer < 0 || iLayer >= static_cast<int>(m_apoLayers.size()))
        return false;

    const std::string osName(m_apoLayers[iLayer]->GetName());
    return IsPrivateLayerName(osName);
}

/***********************************************************************/
/*                           GetMetadata()                             */
/***********************************************************************/

char **OGROpenFileGDBDataSource::GetMetadata(const char *pszDomain)
{
    if (pszDomain && EQUAL(pszDomain, "SUBDATASETS"))
        return m_aosSubdatasets.List();
    return OGRDataSource::GetMetadata(pszDomain);
}

/************************************************************************/
/*                 OGROpenFileGDBSingleFeatureLayer()                   */
/************************************************************************/

OGROpenFileGDBSingleFeatureLayer::OGROpenFileGDBSingleFeatureLayer(
    const char *pszLayerName, const char *pszValIn)
    : pszVal(pszValIn ? CPLStrdup(pszValIn) : nullptr),
      poFeatureDefn(new OGRFeatureDefn(pszLayerName)), iNextShapeId(0)
{
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();
    OGRFieldDefn oField("FIELD_1", OFTString);
    poFeatureDefn->AddFieldDefn(&oField);
}

/************************************************************************/
/*                 ~OGROpenFileGDBSingleFeatureLayer()                  */
/************************************************************************/

OGROpenFileGDBSingleFeatureLayer::~OGROpenFileGDBSingleFeatureLayer()
{
    if (poFeatureDefn != nullptr)
        poFeatureDefn->Release();
    CPLFree(pszVal);
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGROpenFileGDBSingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return nullptr;

    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    if (pszVal)
        poFeature->SetField(0, pszVal);
    poFeature->SetFID(iNextShapeId++);
    return poFeature;
}

/***********************************************************************/
/*                     OGROpenFileGDBSimpleSQLLayer                    */
/***********************************************************************/

class OGROpenFileGDBSimpleSQLLayer final : public OGRLayer
{
    OGRLayer *poBaseLayer;
    FileGDBIterator *poIter;
    OGRFeatureDefn *poFeatureDefn;
    GIntBig m_nOffset;
    GIntBig m_nLimit;
    GIntBig m_nSkipped = 0;
    GIntBig m_nIterated = 0;

    CPL_DISALLOW_COPY_ASSIGN(OGROpenFileGDBSimpleSQLLayer)

  public:
    OGROpenFileGDBSimpleSQLLayer(OGRLayer *poBaseLayer, FileGDBIterator *poIter,
                                 int nColumns, const swq_col_def *pasColDefs,
                                 GIntBig nOffset, GIntBig nLimit);
    virtual ~OGROpenFileGDBSimpleSQLLayer();

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    virtual int TestCapability(const char *) override;

    virtual const char *GetFIDColumn() override
    {
        return poBaseLayer->GetFIDColumn();
    }

    virtual OGRErr GetExtent(OGREnvelope *psExtent, int bForce) override
    {
        return poBaseLayer->GetExtent(psExtent, bForce);
    }

    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                             int bForce) override
    {
        return OGRLayer::GetExtent(iGeomField, psExtent, bForce);
    }

    virtual GIntBig GetFeatureCount(int bForce) override;
};

/***********************************************************************/
/*                    OGROpenFileGDBSimpleSQLLayer()                   */
/***********************************************************************/

OGROpenFileGDBSimpleSQLLayer::OGROpenFileGDBSimpleSQLLayer(
    OGRLayer *poBaseLayerIn, FileGDBIterator *poIterIn, int nColumns,
    const swq_col_def *pasColDefs, GIntBig nOffset, GIntBig nLimit)
    : poBaseLayer(poBaseLayerIn), poIter(poIterIn), poFeatureDefn(nullptr),
      m_nOffset(nOffset), m_nLimit(nLimit)
{
    if (nColumns == 1 && strcmp(pasColDefs[0].field_name, "*") == 0)
    {
        poFeatureDefn = poBaseLayer->GetLayerDefn();
        poFeatureDefn->Reference();
    }
    else
    {
        poFeatureDefn = new OGRFeatureDefn(poBaseLayer->GetName());
        poFeatureDefn->SetGeomType(poBaseLayer->GetGeomType());
        poFeatureDefn->Reference();
        if (poBaseLayer->GetGeomType() != wkbNone)
        {
            poFeatureDefn->GetGeomFieldDefn(0)->SetName(
                poBaseLayer->GetGeometryColumn());
            poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(
                poBaseLayer->GetSpatialRef());
        }
        for (int i = 0; i < nColumns; i++)
        {
            if (strcmp(pasColDefs[i].field_name, "*") == 0)
            {
                for (int j = 0;
                     j < poBaseLayer->GetLayerDefn()->GetFieldCount(); j++)
                    poFeatureDefn->AddFieldDefn(
                        poBaseLayer->GetLayerDefn()->GetFieldDefn(j));
            }
            else
            {
                OGRFieldDefn *poFieldDefn =
                    poBaseLayer->GetLayerDefn()->GetFieldDefn(
                        poBaseLayer->GetLayerDefn()->GetFieldIndex(
                            pasColDefs[i].field_name));
                CPLAssert(poFieldDefn != nullptr); /* already checked before */
                poFeatureDefn->AddFieldDefn(poFieldDefn);
            }
        }
    }
    SetDescription(poFeatureDefn->GetName());
    OGROpenFileGDBSimpleSQLLayer::ResetReading();
}

/***********************************************************************/
/*                   ~OGROpenFileGDBSimpleSQLLayer()                   */
/***********************************************************************/

OGROpenFileGDBSimpleSQLLayer::~OGROpenFileGDBSimpleSQLLayer()
{
    if (poFeatureDefn)
    {
        poFeatureDefn->Release();
    }
    delete poIter;
}

/***********************************************************************/
/*                          ResetReading()                             */
/***********************************************************************/

void OGROpenFileGDBSimpleSQLLayer::ResetReading()
{
    poIter->Reset();
    m_nSkipped = 0;
    m_nIterated = 0;
}

/***********************************************************************/
/*                          GetFeature()                               */
/***********************************************************************/

OGRFeature *OGROpenFileGDBSimpleSQLLayer::GetFeature(GIntBig nFeatureId)
{
    OGRFeature *poSrcFeature = poBaseLayer->GetFeature(nFeatureId);
    if (poSrcFeature == nullptr)
        return nullptr;

    if (poFeatureDefn == poBaseLayer->GetLayerDefn())
        return poSrcFeature;
    else
    {
        OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
        poFeature->SetFrom(poSrcFeature);
        poFeature->SetFID(poSrcFeature->GetFID());
        delete poSrcFeature;
        return poFeature;
    }
}

/***********************************************************************/
/*                         GetNextFeature()                            */
/***********************************************************************/

OGRFeature *OGROpenFileGDBSimpleSQLLayer::GetNextFeature()
{
    while (true)
    {
        if (m_nLimit >= 0 && m_nIterated == m_nLimit)
            return nullptr;

        int nRow = poIter->GetNextRowSortedByValue();
        if (nRow < 0)
            return nullptr;
        OGRFeature *poFeature = GetFeature(nRow + 1);
        if (poFeature == nullptr)
            return nullptr;
        if (m_nOffset >= 0 && m_nSkipped < m_nOffset)
        {
            delete poFeature;
            m_nSkipped++;
            continue;
        }
        m_nIterated++;

        if ((m_poFilterGeom == nullptr ||
             FilterGeometry(poFeature->GetGeometryRef())) &&
            (m_poAttrQuery == nullptr || m_poAttrQuery->Evaluate(poFeature)))
        {
            return poFeature;
        }

        delete poFeature;
    }
}

/***********************************************************************/
/*                         GetFeatureCount()                           */
/***********************************************************************/

GIntBig OGROpenFileGDBSimpleSQLLayer::GetFeatureCount(int bForce)
{

    /* No filter */
    if (m_poFilterGeom == nullptr && m_poAttrQuery == nullptr)
    {
        GIntBig nRowCount = poIter->GetRowCount();
        if (m_nOffset > 0)
        {
            if (m_nOffset <= nRowCount)
                nRowCount -= m_nOffset;
            else
                nRowCount = 0;
        }
        if (m_nLimit >= 0 && nRowCount > m_nLimit)
            nRowCount = m_nLimit;
        return nRowCount;
    }

    return OGRLayer::GetFeatureCount(bForce);
}

/***********************************************************************/
/*                         TestCapability()                            */
/***********************************************************************/

int OGROpenFileGDBSimpleSQLLayer::TestCapability(const char *pszCap)
{

    if (EQUAL(pszCap, OLCFastFeatureCount))
    {
        return m_poFilterGeom == nullptr && m_poAttrQuery == nullptr;
    }
    else if (EQUAL(pszCap, OLCFastGetExtent))
    {
        return TRUE;
    }
    else if (EQUAL(pszCap, OLCRandomRead))
    {
        return TRUE;
    }
    else if (EQUAL(pszCap, OLCStringsAsUTF8))
    {
        return TRUE; /* ? */
    }

    return FALSE;
}

/***********************************************************************/
/*                            ExecuteSQL()                             */
/***********************************************************************/

OGRLayer *OGROpenFileGDBDataSource::ExecuteSQL(const char *pszSQLCommand,
                                               OGRGeometry *poSpatialFilter,
                                               const char *pszDialect)
{

    /* -------------------------------------------------------------------- */
    /*      Special case GetLayerDefinition                                 */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "GetLayerDefinition "))
    {
        const char *pszLayerName =
            pszSQLCommand + strlen("GetLayerDefinition ");
        auto poLayer = GetLayerByName(pszLayerName);
        if (poLayer)
        {
            OGRLayer *poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerDefinition", poLayer->GetXMLDefinition().c_str());
            return poRet;
        }
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid layer name: %s",
                 pszLayerName);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Special case GetLayerMetadata                                   */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "GetLayerMetadata "))
    {
        const char *pszLayerName = pszSQLCommand + strlen("GetLayerMetadata ");
        auto poLayer = GetLayerByName(pszLayerName);
        if (poLayer)
        {
            OGRLayer *poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerMetadata", poLayer->GetXMLDocumentation().c_str());
            return poRet;
        }
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid layer name: %s",
                 pszLayerName);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Special case GetLayerAttrIndexUse (only for debugging purposes) */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "GetLayerAttrIndexUse "))
    {
        auto poLayer =
            GetLayerByName(pszSQLCommand + strlen("GetLayerAttrIndexUse "));
        if (poLayer)
        {
            OGRLayer *poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerAttrIndexUse",
                CPLSPrintf("%d", poLayer->GetAttrIndexUse()));
            return poRet;
        }

        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Special case GetLayerSpatialIndexState (only for debugging purposes)
     */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "GetLayerSpatialIndexState "))
    {
        auto poLayer = GetLayerByName(pszSQLCommand +
                                      strlen("GetLayerSpatialIndexState "));
        if (poLayer)
        {
            OGRLayer *poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerSpatialIndexState",
                CPLSPrintf("%d", poLayer->GetSpatialIndexState()));
            return poRet;
        }

        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Special case GetLastSQLUsedOptimizedImplementation (only for
     * debugging purposes) */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszSQLCommand, "GetLastSQLUsedOptimizedImplementation"))
    {
        OGRLayer *poRet = new OGROpenFileGDBSingleFeatureLayer(
            "GetLastSQLUsedOptimizedImplementation",
            CPLSPrintf("%d",
                       static_cast<int>(bLastSQLUsedOptimizedImplementation)));
        return poRet;
    }

    /* -------------------------------------------------------------------- */
    /*      Special case for "CREATE SPATIAL INDEX ON "                     */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "CREATE SPATIAL INDEX ON "))
    {
        const char *pszLayerName =
            pszSQLCommand + strlen("CREATE SPATIAL INDEX ON ");
        auto poLayer = GetLayerByName(pszLayerName);
        if (poLayer)
        {
            poLayer->CreateSpatialIndex();
        }
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid layer name: %s",
                 pszLayerName);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Special case for "CREATE INDEX idx_name ON table_name(col_name) */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "CREATE INDEX "))
    {
        CPLString osSQL(pszSQLCommand);
        const auto nPosON = osSQL.ifind(" ON ");
        if (nPosON != std::string::npos)
        {
            const std::string osIdxName = osSQL.substr(
                strlen("CREATE INDEX "), nPosON - strlen("CREATE INDEX "));
            const std::string afterOn = osSQL.substr(nPosON + strlen(" ON "));
            const auto nOpenParPos = afterOn.find('(');
            if (nOpenParPos != std::string::npos && afterOn.back() == ')')
            {
                const std::string osLayerName = afterOn.substr(0, nOpenParPos);
                const std::string osExpression = afterOn.substr(
                    nOpenParPos + 1, afterOn.size() - nOpenParPos - 2);
                auto poLayer = GetLayerByName(osLayerName.c_str());
                if (poLayer)
                {
                    poLayer->CreateIndex(osIdxName, osExpression);
                    return nullptr;
                }
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid layer name: %s",
                         osLayerName.c_str());
                return nullptr;
            }
        }
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Bad syntax. Expected CREATE INDEX idx_name ON "
                 "table_name(col_name)");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Special case for "RECOMPUTE EXTENT ON "                         */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "RECOMPUTE EXTENT ON "))
    {
        const char *pszLayerName =
            pszSQLCommand + strlen("RECOMPUTE EXTENT ON ");
        auto poLayer = GetLayerByName(pszLayerName);
        if (poLayer)
        {
            poLayer->RecomputeExtent();
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid layer name: %s",
                     pszLayerName);
        }
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Special case for "DELLAYER:"                                    */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "DELLAYER:"))
    {
        const char *pszLayerName = pszSQLCommand + strlen("DELLAYER:");
        for (int i = 0; i < static_cast<int>(m_apoLayers.size()); ++i)
        {
            if (strcmp(pszLayerName, m_apoLayers[i]->GetName()) == 0)
            {
                DeleteLayer(i);
                return nullptr;
            }
        }
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid layer name: %s",
                 pszLayerName);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Special case for "CHECK_FREELIST_CONSISTENCY:"                  */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "CHECK_FREELIST_CONSISTENCY:"))
    {
        const char *pszLayerName =
            pszSQLCommand + strlen("CHECK_FREELIST_CONSISTENCY:");
        auto poLayer = GetLayerByName(pszLayerName);
        if (poLayer)
        {
            return new OGROpenFileGDBSingleFeatureLayer(
                "result",
                CPLSPrintf("%d", static_cast<int>(
                                     poLayer->CheckFreeListConsistency())));
        }
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid layer name: %s",
                 pszLayerName);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Special case for "REPACK"                                       */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszSQLCommand, "REPACK"))
    {
        bool bSuccess = true;
        for (auto &poLayer : m_apoLayers)
        {
            if (!poLayer->Repack())
                bSuccess = false;
        }
        return new OGROpenFileGDBSingleFeatureLayer(
            "result", bSuccess ? "true" : "false");
    }
    else if (STARTS_WITH(pszSQLCommand, "REPACK "))
    {
        const char *pszLayerName = pszSQLCommand + strlen("REPACK ");
        auto poLayer = GetLayerByName(pszLayerName);
        if (poLayer)
        {
            const bool bSuccess = poLayer->Repack();
            return new OGROpenFileGDBSingleFeatureLayer(
                "result", bSuccess ? "true" : "false");
        }
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid layer name: %s",
                 pszLayerName);
        return nullptr;
    }

    bLastSQLUsedOptimizedImplementation = false;

    /* -------------------------------------------------------------------- */
    /*      Special cases for SQL optimizations                             */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "SELECT ") &&
        (pszDialect == nullptr || EQUAL(pszDialect, "") ||
         EQUAL(pszDialect, "OGRSQL")) &&
        CPLTestBool(CPLGetConfigOption("OPENFILEGDB_USE_INDEX", "YES")))
    {
        swq_select oSelect;
        if (oSelect.preparse(pszSQLCommand) != CE_None)
            return nullptr;

        /* --------------------------------------------------------------------
         */
        /*      MIN/MAX/SUM/AVG/COUNT optimization */
        /* --------------------------------------------------------------------
         */
        if (oSelect.join_count == 0 && oSelect.poOtherSelect == nullptr &&
            oSelect.table_count == 1 && oSelect.order_specs == 0 &&
            oSelect.query_mode != SWQM_DISTINCT_LIST &&
            oSelect.where_expr == nullptr)
        {
            OGROpenFileGDBLayer *poLayer =
                reinterpret_cast<OGROpenFileGDBLayer *>(
                    GetLayerByName(oSelect.table_defs[0].table_name));
            if (poLayer)
            {
                OGRMemLayer *poMemLayer = nullptr;

                int i = 0;  // Used after for.
                for (; i < oSelect.result_columns(); i++)
                {
                    swq_col_func col_func = oSelect.column_defs[i].col_func;
                    if (!(col_func == SWQCF_MIN || col_func == SWQCF_MAX ||
                          col_func == SWQCF_COUNT || col_func == SWQCF_AVG ||
                          col_func == SWQCF_SUM))
                        break;

                    if (oSelect.column_defs[i].field_name == nullptr)
                        break;
                    if (oSelect.column_defs[i].distinct_flag)
                        break;
                    if (oSelect.column_defs[i].target_type != SWQ_OTHER)
                        break;

                    int idx = poLayer->GetLayerDefn()->GetFieldIndex(
                        oSelect.column_defs[i].field_name);
                    if (idx < 0)
                        break;

                    OGRFieldDefn *poFieldDefn =
                        poLayer->GetLayerDefn()->GetFieldDefn(idx);

                    if (col_func == SWQCF_SUM &&
                        poFieldDefn->GetType() == OFTDateTime)
                        break;

                    int eOutOGRType = -1;
                    const OGRField *psField = nullptr;
                    OGRField sField;
                    if (col_func == SWQCF_MIN || col_func == SWQCF_MAX)
                    {
                        psField = poLayer->GetMinMaxValue(
                            poFieldDefn, col_func == SWQCF_MIN, eOutOGRType);
                        if (eOutOGRType < 0)
                            break;
                    }
                    else
                    {
                        double dfMin = 0.0;
                        double dfMax = 0.0;
                        int nCount = 0;
                        double dfSum = 0.0;

                        if (!poLayer->GetMinMaxSumCount(poFieldDefn, dfMin,
                                                        dfMax, dfSum, nCount))
                            break;
                        psField = &sField;
                        if (col_func == SWQCF_AVG)
                        {
                            if (nCount == 0)
                            {
                                eOutOGRType = OFTReal;
                                psField = nullptr;
                            }
                            else
                            {
                                if (poFieldDefn->GetType() == OFTDateTime)
                                {
                                    eOutOGRType = OFTDateTime;
                                    FileGDBDoubleDateToOGRDate(dfSum / nCount,
                                                               false, &sField);
                                }
                                else
                                {
                                    eOutOGRType = OFTReal;
                                    sField.Real = dfSum / nCount;
                                }
                            }
                        }
                        else if (col_func == SWQCF_COUNT)
                        {
                            sField.Integer = nCount;
                            eOutOGRType = OFTInteger;
                        }
                        else
                        {
                            sField.Real = dfSum;
                            eOutOGRType = OFTReal;
                        }
                    }

                    if (poMemLayer == nullptr)
                    {
                        poMemLayer =
                            new OGRMemLayer("SELECT", nullptr, wkbNone);
                        OGRFeature *poFeature =
                            new OGRFeature(poMemLayer->GetLayerDefn());
                        CPL_IGNORE_RET_VAL(
                            poMemLayer->CreateFeature(poFeature));
                        delete poFeature;
                    }

                    const char *pszMinMaxFieldName =
                        CPLSPrintf("%s_%s",
                                   (col_func == SWQCF_MIN)   ? "MIN"
                                   : (col_func == SWQCF_MAX) ? "MAX"
                                   : (col_func == SWQCF_AVG) ? "AVG"
                                   : (col_func == SWQCF_SUM) ? "SUM"
                                                             : "COUNT",
                                   oSelect.column_defs[i].field_name);
                    OGRFieldDefn oFieldDefn(
                        pszMinMaxFieldName,
                        static_cast<OGRFieldType>(eOutOGRType));
                    poMemLayer->CreateField(&oFieldDefn);
                    if (psField != nullptr)
                    {
                        OGRFeature *poFeature = poMemLayer->GetFeature(0);
                        poFeature->SetField(oFieldDefn.GetNameRef(), psField);
                        CPL_IGNORE_RET_VAL(poMemLayer->SetFeature(poFeature));
                        delete poFeature;
                    }
                }
                if (i != oSelect.result_columns())
                {
                    delete poMemLayer;
                }
                else
                {
                    CPLDebug(
                        "OpenFileGDB",
                        "Using optimized MIN/MAX/SUM/AVG/COUNT implementation");
                    bLastSQLUsedOptimizedImplementation = true;
                    return poMemLayer;
                }
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      ORDER BY optimization */
        /* --------------------------------------------------------------------
         */
        if (oSelect.join_count == 0 && oSelect.poOtherSelect == nullptr &&
            oSelect.table_count == 1 && oSelect.order_specs == 1 &&
            oSelect.query_mode != SWQM_DISTINCT_LIST)
        {
            OGROpenFileGDBLayer *poLayer =
                reinterpret_cast<OGROpenFileGDBLayer *>(
                    GetLayerByName(oSelect.table_defs[0].table_name));
            if (poLayer != nullptr &&
                poLayer->HasIndexForField(oSelect.order_defs[0].field_name))
            {
                OGRErr eErr = OGRERR_NONE;
                if (oSelect.where_expr != nullptr)
                {
                    /* The where must be a simple comparison on the column */
                    /* that is used for ordering */
                    if (oSelect.where_expr->eNodeType == SNT_OPERATION &&
                        OGROpenFileGDBIsComparisonOp(
                            oSelect.where_expr->nOperation) &&
                        oSelect.where_expr->nOperation != SWQ_NE &&
                        oSelect.where_expr->nSubExprCount == 2 &&
                        (oSelect.where_expr->papoSubExpr[0]->eNodeType ==
                             SNT_COLUMN ||
                         oSelect.where_expr->papoSubExpr[0]->eNodeType ==
                             SNT_CONSTANT) &&
                        oSelect.where_expr->papoSubExpr[0]->field_type ==
                            SWQ_STRING &&
                        EQUAL(oSelect.where_expr->papoSubExpr[0]->string_value,
                              oSelect.order_defs[0].field_name) &&
                        oSelect.where_expr->papoSubExpr[1]->eNodeType ==
                            SNT_CONSTANT)
                    {
                        /* ok */
                    }
                    else
                        eErr = OGRERR_FAILURE;
                }
                if (eErr == OGRERR_NONE)
                {
                    int i = 0;  // Used after for.
                    for (; i < oSelect.result_columns(); i++)
                    {
                        if (oSelect.column_defs[i].col_func != SWQCF_NONE)
                            break;
                        if (oSelect.column_defs[i].field_name == nullptr)
                            break;
                        if (oSelect.column_defs[i].distinct_flag)
                            break;
                        if (oSelect.column_defs[i].target_type != SWQ_OTHER)
                            break;
                        if (strcmp(oSelect.column_defs[i].field_name, "*") !=
                                0 &&
                            poLayer->GetLayerDefn()->GetFieldIndex(
                                oSelect.column_defs[i].field_name) < 0)
                            break;
                    }
                    if (i != oSelect.result_columns())
                        eErr = OGRERR_FAILURE;
                }
                if (eErr == OGRERR_NONE)
                {
                    int op = -1;
                    swq_expr_node *poValue = nullptr;
                    if (oSelect.where_expr != nullptr)
                    {
                        op = oSelect.where_expr->nOperation;
                        poValue = oSelect.where_expr->papoSubExpr[1];
                    }

                    FileGDBIterator *poIter = poLayer->BuildIndex(
                        oSelect.order_defs[0].field_name,
                        oSelect.order_defs[0].ascending_flag, op, poValue);

                    /* Check that they are no NULL values */
                    if (oSelect.where_expr == nullptr && poIter != nullptr &&
                        poIter->GetRowCount() !=
                            poLayer->GetFeatureCount(FALSE))
                    {
                        delete poIter;
                        poIter = nullptr;
                    }

                    if (poIter != nullptr)
                    {
                        CPLDebug("OpenFileGDB",
                                 "Using OGROpenFileGDBSimpleSQLLayer");
                        bLastSQLUsedOptimizedImplementation = true;
                        return new OGROpenFileGDBSimpleSQLLayer(
                            poLayer, poIter, oSelect.result_columns(),
                            oSelect.column_defs.data(), oSelect.offset,
                            oSelect.limit);
                    }
                }
            }
        }
    }

    return OGRDataSource::ExecuteSQL(pszSQLCommand, poSpatialFilter,
                                     pszDialect);
}

/***********************************************************************/
/*                           ReleaseResultSet()                        */
/***********************************************************************/

void OGROpenFileGDBDataSource::ReleaseResultSet(OGRLayer *poResultsSet)
{
    delete poResultsSet;
}

/***********************************************************************/
/*                           GetFileList()                             */
/***********************************************************************/

char **OGROpenFileGDBDataSource::GetFileList()
{
    int nInterestTable = -1;
    const char *pszFilenameWithoutPath = CPLGetFilename(m_osDirName.c_str());
    CPLString osFilenameRadix;
    unsigned int unInterestTable = 0;
    if (strlen(pszFilenameWithoutPath) == strlen("a00000000.gdbtable") &&
        pszFilenameWithoutPath[0] == 'a' &&
        sscanf(pszFilenameWithoutPath, "a%08x.gdbtable", &unInterestTable) == 1)
    {
        nInterestTable = static_cast<int>(unInterestTable);
        osFilenameRadix = CPLSPrintf("a%08x.", nInterestTable);
    }

    char **papszFiles = VSIReadDir(m_osDirName);
    CPLStringList osStringList;
    char **papszIter = papszFiles;
    for (; papszIter != nullptr && *papszIter != nullptr; papszIter++)
    {
        if (strcmp(*papszIter, ".") == 0 || strcmp(*papszIter, "..") == 0)
            continue;
        if (osFilenameRadix.empty() ||
            strncmp(*papszIter, osFilenameRadix, osFilenameRadix.size()) == 0)
        {
            osStringList.AddString(
                CPLFormFilename(m_osDirName, *papszIter, nullptr));
        }
    }
    CSLDestroy(papszFiles);
    return osStringList.StealList();
}

/************************************************************************/
/*                           BuildSRS()                                 */
/************************************************************************/

OGRSpatialReference *
OGROpenFileGDBDataSource::BuildSRS(const CPLXMLNode *psInfo)
{
    const char *pszWKT =
        CPLGetXMLValue(psInfo, "SpatialReference.WKT", nullptr);
    const int nWKID =
        atoi(CPLGetXMLValue(psInfo, "SpatialReference.WKID", "0"));
    // The concept of LatestWKID is explained in
    // https://support.esri.com/en/technical-article/000013950
    int nLatestWKID =
        atoi(CPLGetXMLValue(psInfo, "SpatialReference.LatestWKID", "0"));

    std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser> poSRS;
    if (nWKID > 0 || nLatestWKID > 0)
    {
        const auto ImportFromCode =
            [](OGRSpatialReference &oSRS, int nLatestCode, int nCode)
        {
            bool bSuccess = false;
            CPLErrorStateBackuper oQuietError(CPLQuietErrorHandler);

            // Try first with nLatestWKID as there is a higher chance it is a
            // EPSG code and not an ESRI one.
            if (nLatestCode > 0)
            {
                if (nLatestCode > 32767)
                {
                    if (oSRS.SetFromUserInput(
                            CPLSPrintf("ESRI:%d", nLatestCode)) == OGRERR_NONE)
                    {
                        bSuccess = true;
                    }
                }
                else if (oSRS.importFromEPSG(nLatestCode) == OGRERR_NONE)
                {
                    bSuccess = true;
                }
                if (!bSuccess)
                {
                    CPLDebug("OpenFileGDB", "Cannot import SRID %d",
                             nLatestCode);
                }
            }
            if (!bSuccess && nCode > 0)
            {
                if (nCode > 32767)
                {
                    if (oSRS.SetFromUserInput(CPLSPrintf("ESRI:%d", nCode)) ==
                        OGRERR_NONE)
                    {
                        bSuccess = true;
                    }
                }
                else if (oSRS.importFromEPSG(nCode) == OGRERR_NONE)
                {
                    bSuccess = true;
                }
                if (!bSuccess)
                {
                    CPLDebug("OpenFileGDB", "Cannot import SRID %d", nCode);
                }
            }

            return bSuccess;
        };

        poSRS =
            std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser>(
                new OGRSpatialReference());
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (!ImportFromCode(*poSRS.get(), nLatestWKID, nWKID))
        {
            poSRS.reset();
        }
        else
        {
            const int nLatestVCSWKID = atoi(
                CPLGetXMLValue(psInfo, "SpatialReference.LatestVCSWKID", "0"));
            const int nVCSWKID =
                atoi(CPLGetXMLValue(psInfo, "SpatialReference.VCSWKID", "0"));
            if (nVCSWKID > 0 || nLatestVCSWKID > 0)
            {
                auto poVertSRS = std::unique_ptr<OGRSpatialReference,
                                                 OGRSpatialReferenceReleaser>(
                    new OGRSpatialReference());
                if (ImportFromCode(*poVertSRS.get(), nLatestVCSWKID, nVCSWKID))
                {
                    auto poCompoundSRS =
                        std::unique_ptr<OGRSpatialReference,
                                        OGRSpatialReferenceReleaser>(
                            new OGRSpatialReference());
                    if (poCompoundSRS->SetCompoundCS(
                            std::string(poSRS->GetName())
                                .append(" + ")
                                .append(poVertSRS->GetName())
                                .c_str(),
                            poSRS.get(), poVertSRS.get()) == OGRERR_NONE)
                    {
                        poCompoundSRS->SetAxisMappingStrategy(
                            OAMS_TRADITIONAL_GIS_ORDER);
                        poSRS = std::move(poCompoundSRS);
                    }
                }
                if (!poSRS->IsCompound() &&
                    !(pszWKT != nullptr && pszWKT[0] != '{'))
                {
                    poSRS.reset();
                }
            }
        }
    }
    if (pszWKT != nullptr && pszWKT[0] != '{' &&
        (poSRS == nullptr ||
         (strstr(pszWKT, "VERTCS") && !poSRS->IsCompound())))
    {
        poSRS.reset(BuildSRS(pszWKT));
    }
    return poSRS.release();
}

/************************************************************************/
/*                           BuildSRS()                                 */
/************************************************************************/

OGRSpatialReference *OGROpenFileGDBDataSource::BuildSRS(const char *pszWKT)
{
    std::shared_ptr<OGRSpatialReference> poSharedObj;
    m_oCacheWKTToSRS.tryGet(pszWKT, poSharedObj);
    if (poSharedObj)
        return poSharedObj->Clone();

    OGRSpatialReference *poSRS = new OGRSpatialReference();
    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if (poSRS->importFromWkt(pszWKT) != OGRERR_NONE)
    {
        delete poSRS;
        poSRS = nullptr;
    }
    if (poSRS != nullptr)
    {
        if (CPLTestBool(CPLGetConfigOption("USE_OSR_FIND_MATCHES", "YES")))
        {
            auto poSRSMatch = poSRS->FindBestMatch(100);
            if (poSRSMatch)
            {
                poSRS->Release();
                poSRS = poSRSMatch;
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            }
            m_oCacheWKTToSRS.insert(
                pszWKT, std::shared_ptr<OGRSpatialReference>(poSRS->Clone()));
        }
        else
        {
            poSRS->AutoIdentifyEPSG();
        }
    }
    return poSRS;
}
