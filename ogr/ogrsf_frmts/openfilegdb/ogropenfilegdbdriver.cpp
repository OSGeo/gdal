/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_openfilegdb.h"
#include "ogropenfilegdbdrivercore.h"

#include <cstddef>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdalalgorithm.h"
#include "ogr_core.h"

// g++ -O2 -Wall -Wextra -g -shared -fPIC ogr/ogrsf_frmts/openfilegdb/*.cpp
// -o ogr_OpenFileGDB.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts
// -Iogr/ogrsf_frmts/mem -Iogr/ogrsf_frmts/openfilegdb -L. -lgdal

extern "C" void RegisterOGROpenFileGDB();

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGROpenFileGDBDriverOpen(GDALOpenInfo *poOpenInfo)

{
    const char *pszFilename = poOpenInfo->pszFilename;
    if (OGROpenFileGDBDriverIdentify(poOpenInfo, pszFilename) ==
        GDAL_IDENTIFY_FALSE)
        return nullptr;

#ifdef DEBUG
    /* For AFL, so that .cur_input is detected as the archive filename */
    if (poOpenInfo->fpL != nullptr &&
        !STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/") &&
        EQUAL(CPLGetFilename(poOpenInfo->pszFilename), ".cur_input"))
    {
        GDALOpenInfo oOpenInfo(
            (CPLString("/vsitar/") + poOpenInfo->pszFilename).c_str(),
            poOpenInfo->nOpenFlags);
        oOpenInfo.papszOpenOptions = poOpenInfo->papszOpenOptions;
        return OGROpenFileGDBDriverOpen(&oOpenInfo);
    }
#endif

    auto poDS = std::make_unique<OGROpenFileGDBDataSource>();
    bool bRetryFileGDB = false;
    if (poDS->Open(poOpenInfo, bRetryFileGDB))
    {
        if (poDS->GetSubdatasets().size() == 2)
        {
            // If there is a single raster dataset, open it right away.
            GDALOpenInfo oOpenInfo(
                poDS->GetSubdatasets().FetchNameValue("SUBDATASET_1_NAME"),
                poOpenInfo->nOpenFlags);
            poDS = std::make_unique<OGROpenFileGDBDataSource>();
            if (poDS->Open(&oOpenInfo, bRetryFileGDB))
            {
                poDS->SetDescription(poOpenInfo->pszFilename);
            }
            else
            {
                poDS.reset();
            }
        }
        return poDS.release();
    }
    else if (bRetryFileGDB)
    {
        auto poDriver = GetGDALDriverManager()->GetDriverByName("FileGDB");
        if (poDriver)
        {
            GDALOpenInfo oOpenInfo(pszFilename, poOpenInfo->nOpenFlags);
            CPLStringList aosOpenOptions;
            aosOpenOptions.SetNameValue("@MAY_USE_OPENFILEGDB", "NO");
            oOpenInfo.papszOpenOptions = aosOpenOptions.List();
            return poDriver->Open(&oOpenInfo, false);
        }
    }

    return nullptr;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *OGROpenFileGDBDriverCreate(const char *pszName, int nXSize,
                                               int nYSize, int nBands,
                                               GDALDataType eType,
                                               char ** /* papszOptions*/)

{
    if (!(nXSize == 0 && nYSize == 0 && nBands == 0 && eType == GDT_Unknown))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "OpenFileGDB::Create(): only vector datasets supported");
        return nullptr;
    }

    auto poDS = std::make_unique<OGROpenFileGDBDataSource>();
    if (!poDS->Create(pszName))
        return nullptr;
    return poDS.release();
}

/************************************************************************/
/*                     OGROpenFileGDBDriverDelete()                     */
/************************************************************************/

static CPLErr OGROpenFileGDBDriverDelete(const char *pszFilename)
{
    CPLStringList aosFiles(VSIReadDir(pszFilename));
    if (aosFiles.empty())
        return CE_Failure;

    for (int i = 0; i < aosFiles.size(); ++i)
    {
        if (strcmp(aosFiles[i], ".") != 0 && strcmp(aosFiles[i], "..") != 0)
        {
            const std::string osFilename(
                CPLFormFilenameSafe(pszFilename, aosFiles[i], nullptr));
            if (VSIUnlink(osFilename.c_str()) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot delete %s",
                         osFilename.c_str());
                return CE_Failure;
            }
        }
    }
    if (VSIRmdir(pszFilename) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot delete %s", pszFilename);
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                    OpenFileGDBRepackAlgorithm                        */
/************************************************************************/

#ifndef _
#define _(x) x
#endif

class OpenFileGDBRepackAlgorithm final : public GDALAlgorithm
{
  public:
    OpenFileGDBRepackAlgorithm()
        : GDALAlgorithm("repack", "Repack a FileGeoDatabase dataset",
                        "/drivers/vector/openfilegdb.html")
    {
        AddProgressArg();

        constexpr int type = GDAL_OF_RASTER | GDAL_OF_VECTOR | GDAL_OF_UPDATE;
        auto &arg =
            AddArg("dataset", 0, _("FileGeoDatabase dataset"), &m_dataset, type)
                .SetPositional()
                .SetRequired();
        SetAutoCompleteFunctionForFilename(arg, type);
    }

  protected:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override
    {
        auto poDS =
            dynamic_cast<OGROpenFileGDBDataSource *>(m_dataset.GetDatasetRef());
        if (!poDS)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "%s is not a FileGeoDatabase",
                        m_dataset.GetName().c_str());
            return false;
        }
        bool bSuccess = true;
        int iLayer = 0;
        for (auto &poLayer : poDS->GetLayers())
        {
            void *pScaledData = GDALCreateScaledProgress(
                static_cast<double>(iLayer) / poDS->GetLayerCount(),
                static_cast<double>(iLayer + 1) / poDS->GetLayerCount(),
                pfnProgress, pProgressData);
            const bool bRet = poLayer->Repack(
                pScaledData ? GDALScaledProgress : nullptr, pScaledData);
            GDALDestroyScaledProgress(pScaledData);
            if (!bRet)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Repack of layer %s failed", poLayer->GetName());
                bSuccess = false;
            }
            ++iLayer;
        }
        return bSuccess;
    }

  private:
    GDALArgDatasetValue m_dataset{};
};

/************************************************************************/
/*                 OGROpenFileGDBInstantiateAlgorithm()                 */
/************************************************************************/

static GDALAlgorithm *
OGROpenFileGDBInstantiateAlgorithm(const std::vector<std::string> &aosPath)
{
    if (aosPath.size() == 1 && aosPath[0] == "repack")
    {
        return std::make_unique<OpenFileGDBRepackAlgorithm>().release();
    }
    else
    {
        return nullptr;
    }
}

/***********************************************************************/
/*                       RegisterOGROpenFileGDB()                      */
/***********************************************************************/

void RegisterOGROpenFileGDB()

{
    if (!GDAL_CHECK_VERSION("OGR OpenFileGDB"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGROpenFileGDBDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGROpenFileGDBDriverOpen;
    poDriver->pfnCreate = OGROpenFileGDBDriverCreate;
    poDriver->pfnDelete = OGROpenFileGDBDriverDelete;
    poDriver->pfnInstantiateAlgorithm = OGROpenFileGDBInstantiateAlgorithm;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
