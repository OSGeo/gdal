/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Icechunk driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "icechunkdrivercore.h"

#include "cpl_json.h"
#include "cpl_time.h"

#include "gdalalgorithm.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"

#include "ogr_p.h"

#include "icechunkrepo.h"
#include "icechunksnapshot.h"
#include "icechunkutils.h"

#ifndef _
#define _(x) (x)
#endif

namespace gdal::icechunk
{
/************************************************************************/
/*                            DatasetOpen()                             */
/************************************************************************/

static GDALDataset *DatasetOpen(GDALOpenInfo *poOpenInfo)
{
    if (!IcechunkDriverIdentify(poOpenInfo) || poOpenInfo->eAccess == GA_Update)
        return nullptr;

    std::unique_ptr<GDALOpenInfo> poTmpOpenInfo;  // keep in that scope
    std::string osBranchName;
    std::string osTagName;
    const std::string osFullFilename = poOpenInfo->pszFilename;
    bool ignoreTimestampEtag;
    std::string osFilename = GetFilenameFromDatasetName(
        poOpenInfo->pszFilename, osBranchName, osTagName, ignoreTimestampEtag);
    if (osFilename.empty())
        return nullptr;  // Error emitted by GetFilenameFromDatasetName
    if (osFilename != poOpenInfo->pszFilename)
    {
        poTmpOpenInfo =
            std::make_unique<GDALOpenInfo>(osFilename.c_str(), GA_ReadOnly);
        poTmpOpenInfo->nOpenFlags = poOpenInfo->nOpenFlags;
        poOpenInfo = poTmpOpenInfo.get();
    }

    auto repo = IcechunkRepo::Open(poOpenInfo->pszFilename,
                                   poOpenInfo->bIsDirectory ? nullptr
                                                            : poOpenInfo->fpL);
    if (!repo)
        return nullptr;

    class DummyDataset : public GDALDataset
    {
      public:
        DummyDataset()
        {
            nRasterXSize = 0;
            nRasterYSize = 0;
        }

        std::shared_ptr<GDALGroup> GetRootGroup() const override
        {
            class DummyGroup : public GDALGroup
            {
              public:
                DummyGroup() : GDALGroup(std::string(), "/")
                {
                }
            };

            return std::make_shared<DummyGroup>();
        }
    };

    const auto ConcatBranchOrTagNames =
        [](const std::map<std::string, std::string> &mapNameToSnapshotId)
    {
        std::string s;
        for (const auto &[name, _] : mapNameToSnapshotId)
        {
            if (!s.empty())
                s += ", ";
            s += '"';
            s += name;
            s += '"';
        }
        return s;
    };

    std::unique_ptr<IcechunkSnapshot> snapshot;
    if (osTagName.empty())
    {
        if (osBranchName.empty())
        {
            const auto branches = repo->GetBranches();
            if (branches.empty())
            {
                return std::make_unique<DummyDataset>().release();
            }
            else if (branches.find("main") != branches.end())
            {
                osBranchName = "main";
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "You need to specify a branch name among %s",
                         ConcatBranchOrTagNames(repo->GetBranches()).c_str());
                return nullptr;
            }
        }

        const auto nErrorCount = CPLGetErrorCounter();
        snapshot = repo->OpenSnapshotOnBranch(osBranchName, false);
        if (!snapshot)
        {
            if (nErrorCount == CPLGetErrorCounter())
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Invalid branch name \"%s\". Valid branch names are: %s",
                    osBranchName.c_str(),
                    ConcatBranchOrTagNames(repo->GetBranches()).c_str());
            }
            return nullptr;
        }
    }
    else
    {
        const auto nErrorCount = CPLGetErrorCounter();
        snapshot = repo->OpenSnapshotOnTag(osTagName, false);
        if (!snapshot)
        {
            if (nErrorCount == CPLGetErrorCounter())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid tag name \"%s\". Valid tag names are: %s",
                         osTagName.c_str(),
                         ConcatBranchOrTagNames(repo->GetTags()).c_str());
            }
            return nullptr;
        }
    }

    if (snapshot->GetNodeCount() <= 1)
    {
        return std::make_unique<DummyDataset>().release();
    }

    auto poZarrDriver = GetGDALDriverManager()->GetDriverByName("ZARR");
    if (!poZarrDriver)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot open Icechunk dataset due to missing Zarr driver");
        return nullptr;
    }
    const auto pfnOpen = poZarrDriver->GetOpenCallback();
    if (!pfnOpen)
    {
        // Cannot happen if using official GDAL Zarr driver!
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot open Icechunk dataset due to missing Open() method in "
                 "Zarr driver");
        return nullptr;
    }

    const std::string osVSIIcechunkFilename =
        std::string("ZARR:\"/vsiicechunk/{")
            .append(osFullFilename)
            .append("}\"");
    GDALOpenInfo oOpenInfoZarr(osVSIIcechunkFilename.c_str(), GA_ReadOnly);
    oOpenInfoZarr.nOpenFlags = poOpenInfo->nOpenFlags;
    oOpenInfoZarr.papszOpenOptions = poOpenInfo->papszOpenOptions;
    // cppcheck-suppress returnDanglingLifetime
    return pfnOpen(&oOpenInfoZarr);
}

/************************************************************************/
/*                            ClearCaches()                             */
/************************************************************************/

static void ClearCaches(GDALDriver *)
{
    gdal::icechunk::IcechunkRepo::ClearCaches();
    VSIIcechunkFileSystemClearCaches();
}

/************************************************************************/
/*                    TimestampInMicrosecToISO8211()                    */
/************************************************************************/

static std::string TimestampInMicrosecToISO8211(uint64_t nTimestamp)
{
    struct tm brokendown;
    constexpr int MICROSECONDS_IN_SEC = 1000 * 1000;
    CPLUnixTimeToYMDHMS(nTimestamp / MICROSECONDS_IN_SEC, &brokendown);
    OGRField sField;
    sField.Date.Year = static_cast<GInt16>(brokendown.tm_year + 1900);
    sField.Date.Month = static_cast<GByte>(brokendown.tm_mon + 1);
    sField.Date.Day = static_cast<GByte>(brokendown.tm_mday);
    sField.Date.Hour = static_cast<GByte>(brokendown.tm_hour);
    sField.Date.Minute = static_cast<GByte>(brokendown.tm_min);
    sField.Date.Second = static_cast<float>(
        brokendown.tm_sec + (nTimestamp % MICROSECONDS_IN_SEC) /
                                static_cast<float>(MICROSECONDS_IN_SEC));
    sField.Date.TZFlag = OGR_TZFLAG_UTC;
    std::unique_ptr<char, VSIFreeReleaser> pszDateTime(
        OGRGetXMLDateTime(&sField, /* bAlwaysMillisecond = */ false));
    return pszDateTime.get();
}

/************************************************************************/
/*                          ListRefsAlgorithm                           */
/************************************************************************/

class ListRefsAlgorithm /* non-final */ : public GDALAlgorithm
{
  public:
    ~ListRefsAlgorithm() override;

  protected:
    ListRefsAlgorithm(const std::string &osName,
                      const std::string &osDescription,
                      const std::string &osHelpURL)
        : GDALAlgorithm(osName, osDescription, osHelpURL)
    {
        AddInputDatasetArg(&m_dataset, GDAL_OF_MULTIDIM_RASTER);
        AddOutputStringArg(&m_outputString);
    }

    GDALArgDatasetValue m_dataset{};
    std::string m_outputString{};
};

ListRefsAlgorithm::~ListRefsAlgorithm() = default;

/************************************************************************/
/*                        ListBranchesAlgorithm                         */
/************************************************************************/

class ListBranchesAlgorithm final : public ListRefsAlgorithm
{
  public:
    static constexpr const char *NAME = LIST_BRANCHES;

    ListBranchesAlgorithm()
        : ListRefsAlgorithm(
              NAME, std::string("List branches of an Icechunk repository"),
              "/programs/gdal_driver_icechunk_list_branches.html")
    {
    }

  protected:
    bool RunImpl(GDALProgressFunc, void *) override;
};

bool ListBranchesAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    std::string osBranchName;
    std::string osTagName;
    bool ignoreTimestampEtag = false;
    const std::string osFilename = GetFilenameFromDatasetName(
        m_dataset.GetName(), osBranchName, osTagName, ignoreTimestampEtag);
    auto repo = IcechunkRepo::Open(osFilename.c_str());
    if (!repo)
        return false;

    CPLJSONArray oArray;
    for (const auto &[branchName, _] : repo->GetBranches())
    {
        CPLJSONObject oCommit;
        oCommit.Set("name", branchName);
        auto snapshot = repo->OpenSnapshotOnBranch(branchName);
        if (snapshot)
        {
            oCommit.Set("commit_message", snapshot->GetCommitMessage());
            if (const uint64_t nTimestamp = snapshot->GetFlushTimestamp())
            {
                oCommit.Set("timestamp",
                            TimestampInMicrosecToISO8211(nTimestamp));
            }
        }
        oArray.Add(oCommit);
    }
    m_outputString = oArray.ToString();
    m_outputString += '\n';

    return true;
}

/************************************************************************/
/*                          ListTagsAlgorithm                           */
/************************************************************************/

class ListTagsAlgorithm final : public ListRefsAlgorithm
{
  public:
    static constexpr const char *NAME = LIST_TAGS;

    ListTagsAlgorithm()
        : ListRefsAlgorithm(NAME,
                            std::string("List tags of an Icechunk repository"),
                            "/programs/gdal_driver_icechunk_list_tags.html")
    {
    }

  protected:
    bool RunImpl(GDALProgressFunc, void *) override;
};

bool ListTagsAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    std::string osBranchName;
    std::string osTagName;
    bool ignoreTimestampEtag = false;
    const std::string osFilename = GetFilenameFromDatasetName(
        m_dataset.GetName(), osBranchName, osTagName, ignoreTimestampEtag);
    auto repo = IcechunkRepo::Open(osFilename.c_str());
    if (!repo)
        return false;

    CPLJSONArray oArray;
    for (const auto &[tagName, _] : repo->GetTags())
    {
        CPLJSONObject oCommit;
        oCommit.Set("name", tagName);
        auto snapshot = repo->OpenSnapshotOnTag(tagName);
        if (snapshot)
        {
            oCommit.Set("commit_message", snapshot->GetCommitMessage());
            if (const uint64_t nTimestamp = snapshot->GetFlushTimestamp())
            {
                oCommit.Set("timestamp",
                            TimestampInMicrosecToISO8211(nTimestamp));
            }
        }
        oArray.Add(oCommit);
    }
    m_outputString = oArray.ToString();
    m_outputString += '\n';

    return true;
}

/************************************************************************/
/*                        InstantiateAlgorithm()                        */
/************************************************************************/

static GDALAlgorithm *
InstantiateAlgorithm(const std::vector<std::string> &aosPath)
{
    if (aosPath.size() == 1 && aosPath[0] == ListBranchesAlgorithm::NAME)
    {
        return std::make_unique<ListBranchesAlgorithm>().release();
    }
    else if (aosPath.size() == 1 && aosPath[0] == ListTagsAlgorithm::NAME)
    {
        return std::make_unique<ListTagsAlgorithm>().release();
    }
    else
    {
        return nullptr;
    }
}

}  // namespace gdal::icechunk

/************************************************************************/
/*                       GDALRegister_Icechunk()                        */
/************************************************************************/

void GDALRegister_Icechunk()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    gdal::icechunk::VSIInstallIcechunkFileSystem();

    auto poDriver = std::make_unique<GDALDriver>();
    IcechunkDriverSetCommonMetadata(poDriver.get());

    poDriver->pfnOpen = gdal::icechunk::DatasetOpen;
    poDriver->pfnClearCaches = gdal::icechunk::ClearCaches;
    poDriver->pfnInstantiateAlgorithm = gdal::icechunk::InstantiateAlgorithm;

    GetGDALDriverManager()->RegisterDriver(poDriver.release());
}
