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
#include "gdal_frmts.h"
#include "gdal_dataset.h"
#include "gdalplugindriverproxy.h"
#include "cpl_vsi_virtual.h"

/************************************************************************/
/*                       IcechunkDriverIdentify()                       */
/************************************************************************/

int IcechunkDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, ICECHUNK_PREFIX) ||
        poOpenInfo->IsSingleAllowedDriver(DRIVER_NAME))
    {
        return true;
    }

    // Detect /repo file
    if (poOpenInfo->nHeaderBytes >= HEADER_SIZE &&
        memcmp(poOpenInfo->pabyHeader, abySIG, SIG_SIZE) == 0 &&
        poOpenInfo->pabyHeader[SIG_SIZE + IMPLEMENTATION_NAME_SIZE +
                               SPEC_VERSION_SIZE] == FILE_TYPE_REPO_INFO)
    {
        return true;
    }

    if (poOpenInfo->bIsDirectory)
    {
        VSIStatBufL sStat;
        return VSIStatL(CPLFormFilenameSafe(poOpenInfo->pszFilename,
                                            "snapshots", nullptr)
                            .c_str(),
                        &sStat) == 0 &&
               VSIStatL(CPLFormFilenameSafe(poOpenInfo->pszFilename,
                                            "transactions", nullptr)
                            .c_str(),
                        &sStat) == 0 &&
               (VSIStatL(CPLFormFilenameSafe(poOpenInfo->pszFilename, "repo",
                                             nullptr)
                             .c_str(),
                         &sStat) == 0 ||
                VSIStatL(CPLFormFilenameSafe(poOpenInfo->pszFilename, "refs",
                                             nullptr)
                             .c_str(),
                         &sStat) == 0);
    }

    return false;
}

/************************************************************************/
/*                  IcechunkDriverSetCommonMetadata()                   */
/************************************************************************/

void IcechunkDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Icechunk");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, ICECHUNK_PREFIX);

    poDriver->DeclareAlgorithm({LIST_BRANCHES});
    poDriver->DeclareAlgorithm({LIST_TAGS});

    poDriver->pfnIdentify = IcechunkDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                   DeclareDeferredIcechunkPlugin()                    */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredIcechunkPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = std::make_unique<GDALPluginDriverProxy>(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    IcechunkDriverSetCommonMetadata(poDriver.get());
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver.release());

    // trigger the loading of the plugin if /vsiicechunk/ is queried
    VSIFileManager::RegisterHandlerLoader(
        FS_PREFIX,
        []()
        {
            auto poDrv = GetGDALDriverManager()->GetDriverByName(DRIVER_NAME);
            // Querying any non-standard driver metadata forces the plugin
            // to be loaded.
            if (poDrv)
                poDrv->GetMetadata("FORCE_LOAD");
        });
}
#endif
