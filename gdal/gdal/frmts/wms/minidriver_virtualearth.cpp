/******************************************************************************
 * $Id$
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "wmsdriver.h"
#include "minidriver_virtualearth.h"


CPP_GDALWMSMiniDriverFactory(VirtualEarth)

GDALWMSMiniDriver_VirtualEarth::GDALWMSMiniDriver_VirtualEarth()
{
}

GDALWMSMiniDriver_VirtualEarth::~GDALWMSMiniDriver_VirtualEarth()
{
}

CPLErr GDALWMSMiniDriver_VirtualEarth::Initialize(CPLXMLNode *config)
{
    CPLErr ret = CE_None;

    if (ret == CE_None) {
        const char *base_url = CPLGetXMLValue(config, "ServerURL", "");
        if (base_url[0] != '\0') {
            m_base_url = base_url;
            if (m_base_url.find("${quadkey}") == std::string::npos) {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "GDALWMS, VirtualEarth mini-driver: ${quadkey} missing in ServerURL.");
                ret = CE_Failure;
            }
        } else {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GDALWMS, VirtualEarth mini-driver: ServerURL missing.");
            ret = CE_Failure;
        }
    }

    m_parent_dataset->WMSSetDefaultBlockSize(256, 256);
    m_parent_dataset->WMSSetDefaultDataWindowCoordinates(-20037508.34,20037508.34,20037508.34,-20037508.34);
    m_parent_dataset->WMSSetDefaultTileLevel(19);
    m_parent_dataset->WMSSetDefaultOverviewCount(18);
    m_parent_dataset->WMSSetNeedsDataWindow(FALSE);

    m_projection_wkt=ProjToWKT("EPSG:900913");

    return ret;
}

void GDALWMSMiniDriver_VirtualEarth::GetCapabilities(GDALWMSMiniDriverCapabilities *caps)
{
    caps->m_capabilities_version = 1;
    caps->m_has_arb_overviews = 0;
    caps->m_has_image_request = 0;
    caps->m_has_tiled_image_requeset = 1;
    caps->m_max_overview_count = 32;
}

void GDALWMSMiniDriver_VirtualEarth::TiledImageRequest(CPLString *url,
                                                       CPL_UNUSED const GDALWMSImageRequestInfo &iri,
                                                       const GDALWMSTiledImageRequestInfo &tiri)
{

    *url = m_base_url;

    char szTileNumber[64];
    int x = tiri.m_x;
    int y = tiri.m_y;
    int z = MIN(32,tiri.m_level);

    for(int i = 0; i < z; i ++)
    {
        int row = (y & 1);
        int col = (x & 1);

        szTileNumber[z-1-i] = (char) ('0' + (col | (row << 1)));

        x = x >> 1;
        y = y >> 1;
    }
    szTileNumber[z] = 0;

    URLSearchAndReplace(url, "${quadkey}", "%s", szTileNumber);
    URLSearchAndReplace(url, "${server_num}", "%d",
                        (tiri.m_x + tiri.m_y + z) % 4);
}

const char *GDALWMSMiniDriver_VirtualEarth::GetProjectionInWKT() {
    return m_projection_wkt.c_str();
}
