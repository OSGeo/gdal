/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  ESRIJSON driver
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
/*                       OGRESRIJSONDriverIdentify()                    */
/************************************************************************/

static int OGRESRIJSONDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    GeoJSONSourceType nSrcType = ESRIJSONDriverGetSourceType(poOpenInfo);
    if (nSrcType == eGeoJSONSourceUnknown)
        return FALSE;
    if (nSrcType == eGeoJSONSourceService)
    {
        if (poOpenInfo->IsSingleAllowedDriver("ESRIJSON"))
            return TRUE;
        if (!STARTS_WITH_CI(poOpenInfo->pszFilename, "ESRIJSON:"))
        {
            return -1;
        }
    }
    return TRUE;
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

static GDALDataset *OGRESRIJSONDriverOpen(GDALOpenInfo *poOpenInfo)
{
    GeoJSONSourceType nSrcType = ESRIJSONDriverGetSourceType(poOpenInfo);
    if (nSrcType == eGeoJSONSourceUnknown)
        return nullptr;
    return OGRGeoJSONDriverOpenInternal(poOpenInfo, nSrcType, "ESRIJSON");
}

/************************************************************************/
/*                          RegisterOGRESRIJSON()                       */
/************************************************************************/

void RegisterOGRESRIJSON()
{
    if (!GDAL_CHECK_VERSION("OGR/ESRIJSON driver"))
        return;

    if (GDALGetDriverByName("ESRIJSON") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("ESRIJSON");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "ESRIJSON");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "json");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/esrijson.html");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='FEATURE_SERVER_PAGING' type='boolean' "
        "description='Whether to automatically scroll through results with a "
        "ArcGIS Feature Service endpoint'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
                              "<CreationOptionList/>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnOpen = OGRESRIJSONDriverOpen;
    poDriver->pfnIdentify = OGRESRIJSONDriverIdentify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
