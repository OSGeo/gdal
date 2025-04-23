/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  TopoJSON driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_geojson.h"

#include <stdlib.h>
#include <string.h>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogrgeojsonutils.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                       OGRTopoJSONDriverIdentify()                    */
/************************************************************************/

static int OGRTopoJSONDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    GeoJSONSourceType nSrcType = TopoJSONDriverGetSourceType(poOpenInfo);
    if (nSrcType == eGeoJSONSourceUnknown)
        return FALSE;
    if (nSrcType == eGeoJSONSourceService)
    {
        if (poOpenInfo->IsSingleAllowedDriver("TopoJSON"))
            return TRUE;
        if (!STARTS_WITH_CI(poOpenInfo->pszFilename, "TopoJSON:"))
        {
            return -1;
        }
    }
    return TRUE;
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

static GDALDataset *OGRTopoJSONDriverOpen(GDALOpenInfo *poOpenInfo)
{
    GeoJSONSourceType nSrcType = TopoJSONDriverGetSourceType(poOpenInfo);
    if (nSrcType == eGeoJSONSourceUnknown)
        return nullptr;
    return OGRGeoJSONDriverOpenInternal(poOpenInfo, nSrcType, "TopoJSON");
}

/************************************************************************/
/*                          RegisterOGRTopoJSON()                       */
/************************************************************************/

void RegisterOGRTopoJSON()
{
    if (!GDAL_CHECK_VERSION("OGR/TopoJSON driver"))
        return;

    if (GDALGetDriverByName("TopoJSON") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("TopoJSON");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "TopoJSON");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "json topojson");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/topojson.html");

    poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList/>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
                              "<CreationOptionList/>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = OGRTopoJSONDriverOpen;
    poDriver->pfnIdentify = OGRTopoJSONDriverIdentify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
