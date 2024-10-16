/******************************************************************************
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster driver support functions.
 * Author:   Kor de Jong, Oliver Schmitz
 *
 ******************************************************************************
 * Copyright (c) PCRaster owners
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "pcrasterdataset.h"
#include "pcrasterdrivercore.h"

void GDALRegister_PCRaster()
{
    if (!GDAL_CHECK_VERSION("PCRaster driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    PCRasterDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = PCRasterDataset::open;
    poDriver->pfnCreate = PCRasterDataset::create;
    poDriver->pfnCreateCopy = PCRasterDataset::createCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
