/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVFKDriver class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2018, Martin Landa <landa.martin gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_vfk.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogrvfkdrivercore.h"

/*
  \brief Open existing data source
  \return NULL on failure
*/
static GDALDataset *OGRVFKDriverOpen(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->eAccess == GA_Update || !OGRVFKDriverIdentify(poOpenInfo))
        return nullptr;

    OGRVFKDataSource *poDS = new OGRVFKDataSource();

    if (!poDS->Open(poOpenInfo) || poDS->GetLayerCount() == 0)
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/*!
  \brief Register VFK driver
*/
void RegisterOGRVFK()
{
    if (!GDAL_CHECK_VERSION("OGR/VFK driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRVFKDriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = OGRVFKDriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
