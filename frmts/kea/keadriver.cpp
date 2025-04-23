/*
 *  keadriver.cpp
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gdal_frmts.h"
#include "keadataset.h"
#include "keadrivercore.h"

// method to register this driver
void GDALRegister_KEA()
{
    if (!GDAL_CHECK_VERSION("KEA"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    KEADriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = KEADataset::Open;
    poDriver->pfnCreate = KEADataset::Create;
    poDriver->pfnCreateCopy = KEADataset::CreateCopy;
    poDriver->pfnUnloadDriver = KEADatasetDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
