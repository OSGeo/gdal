/******************************************************************************
 * Project:  OGR
 * Purpose:  OGRGMLASDriver implementation
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 * Initial development funded by the European Earth observation programme
 * Copernicus
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_gmlas.h"
#include "ogrgmlasdrivercore.h"

// g++ -I/usr/include/json -DxDEBUG_VERBOSE -DDEBUG   -g -DDEBUG -ftrapv  -Wall
// -Wextra -Winit-self -Wunused-parameter -Wformat -Werror=format-security
// -Wno-format-nonliteral -Wlogical-op -Wshadow -Werror=vla
// -Wmissing-declarations -Wnon-virtual-dtor -Woverloaded-virtual
// -fno-operator-names ogr/ogrsf_frmts/gmlas/*.cpp -fPIC -shared -o ogr_GMLAS.so
// -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/mem  -L. -lgdal
// -I/home/even/spatialys/eea/inspire_gml/install-xerces-c-3.1.3/include

/************************************************************************/
/*                           OGRGMLASDriverOpen()                       */
/************************************************************************/

static GDALDataset *OGRGMLASDriverOpen(GDALOpenInfo *poOpenInfo)

{
    OGRGMLASDataSource *poDS;

    if (poOpenInfo->eAccess == GA_Update)
        return nullptr;

    if (OGRGMLASDriverIdentify(poOpenInfo) == FALSE)
        return nullptr;

    poDS = new OGRGMLASDataSource();

    if (!poDS->Open(poOpenInfo))
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          RegisterOGRGMLAS()                          */
/************************************************************************/

void RegisterOGRGMLAS()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRGMLASDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRGMLASDriverOpen;
    poDriver->pfnCreateCopy = OGRGMLASDriverCreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
