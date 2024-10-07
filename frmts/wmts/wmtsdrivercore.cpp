/******************************************************************************
 *
 * Project:  GDAL WMTS driver
 * Purpose:  Implement GDAL WMTS support
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 * Funded by Land Information New Zealand (LINZ)
 *
 **********************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "wmtsdrivercore.h"

/************************************************************************/
/*                     WMTSDriverIdentify()                             */
/************************************************************************/

int WMTSDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "WMTS:"))
        return TRUE;

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "<GDAL_WMTS"))
        return TRUE;

    const bool bIsSingleDriver = poOpenInfo->IsSingleAllowedDriver("WMTS");
    if (bIsSingleDriver && (STARTS_WITH(poOpenInfo->pszFilename, "http://") ||
                            STARTS_WITH(poOpenInfo->pszFilename, "https://")))
    {
        return true;
    }

    if (poOpenInfo->nHeaderBytes == 0)
        return FALSE;

    const char *pszHeader =
        reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
    if (strstr(pszHeader, "<GDAL_WMTS") ||
        ((strstr(pszHeader, "<Capabilities") ||
          strstr(pszHeader, "<wmts:Capabilities")) &&
         strstr(pszHeader, "http://www.opengis.net/wmts/1.0")))
    {
        return TRUE;
    }

    if (bIsSingleDriver)
    {
        while (*pszHeader != 0 &&
               std::isspace(static_cast<unsigned char>(*pszHeader)))
            ++pszHeader;
        return *pszHeader == '<';
    }

    return FALSE;
}

/************************************************************************/
/*                      WMTSDriverSetCommonMetadata()                   */
/************************************************************************/

void WMTSDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "OGC Web Map Tile Service");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/wmts.html");

    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, "WMTS:");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='URL' type='string' description='URL that points to "
        "GetCapabilities response' required='YES'/>"
        "  <Option name='LAYER' type='string' description='Layer identifier'/>"
        "  <Option name='TILEMATRIXSET' alias='TMS' type='string' "
        "description='Tile matrix set identifier'/>"
        "  <Option name='TILEMATRIX' type='string' description='Tile matrix "
        "identifier of maximum zoom level. Exclusive with ZOOM_LEVEL.'/>"
        "  <Option name='ZOOM_LEVEL' alias='ZOOMLEVEL' type='int' "
        "description='Maximum zoom level. Exclusive with TILEMATRIX.'/>"
        "  <Option name='STYLE' type='string' description='Style identifier'/>"
        "  <Option name='EXTENDBEYONDDATELINE' type='boolean' "
        "description='Whether to enable extend-beyond-dateline behaviour' "
        "default='NO'/>"
        "  <Option name='EXTENT_METHOD' type='string-select' description='How "
        "the raster extent is computed' default='AUTO'>"
        "       <Value>AUTO</Value>"
        "       <Value>LAYER_BBOX</Value>"
        "       <Value>TILE_MATRIX_SET</Value>"
        "       <Value>MOST_PRECISE_TILE_MATRIX</Value>"
        "  </Option>"
        "  <Option name='CLIP_EXTENT_WITH_MOST_PRECISE_TILE_MATRIX' "
        "type='boolean' description='Whether to use the implied bounds of the "
        "most precise tile matrix to clip the layer extent (defaults to NO if "
        "layer bounding box is used, YES otherwise)'/>"
        "  <Option name='CLIP_EXTENT_WITH_MOST_PRECISE_TILE_MATRIX_LIMITS' "
        "type='boolean' description='Whether to use the implied bounds of the "
        "most precise tile matrix limits to clip the layer extent (defaults to "
        "NO if layer bounding box is used, YES otherwise)'/>"
        "</OpenOptionList>");

    poDriver->pfnIdentify = WMTSDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredWMTSPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredWMTSPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    WMTSDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
