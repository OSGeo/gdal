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
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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

    if (poOpenInfo->nHeaderBytes == 0)
        return FALSE;

    if (strstr((const char *)poOpenInfo->pabyHeader, "<GDAL_WMTS"))
        return TRUE;

    return (strstr((const char *)poOpenInfo->pabyHeader, "<Capabilities") !=
                nullptr ||
            strstr((const char *)poOpenInfo->pabyHeader,
                   "<wmts:Capabilities") != nullptr) &&
           strstr((const char *)poOpenInfo->pabyHeader,
                  "http://www.opengis.net/wmts/1.0") != nullptr;
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
