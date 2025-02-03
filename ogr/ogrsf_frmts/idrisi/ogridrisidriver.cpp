/******************************************************************************
 * $I$
 *
 * Project:  Idrisi Translator
 * Purpose:  Implements OGRIdrisiDriver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "ogr_idrisi.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRIdrisiOpen(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->eAccess == GA_Update)
    {
        return nullptr;
    }

    // --------------------------------------------------------------------
    //      Does this appear to be a .vct file?
    // --------------------------------------------------------------------
    if (!poOpenInfo->IsExtensionEqualToCI("vct"))
        return nullptr;

    auto poDS = std::make_unique<OGRIdrisiDataSource>();

    if (!poDS->Open(poOpenInfo->pszFilename))
    {
        return nullptr;
    }

    return poDS.release();
}

/************************************************************************/
/*                        RegisterOGRIdrisi()                           */
/************************************************************************/

void RegisterOGRIdrisi()

{
    if (GDALGetDriverByName("Idrisi") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("Idrisi");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Idrisi Vector (.vct)");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "vct");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->pfnOpen = OGRIdrisiOpen;
    GetGDALDriverManager()->RegisterDriver(poDriver);
}
