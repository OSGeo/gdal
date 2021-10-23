/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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

#include <algorithm>

CPL_CVSID("$Id$")

// These should be global, they are used all over the place
const double SPHERICAL_RADIUS = 6378137.0;
const double MAX_GM = SPHERICAL_RADIUS * M_PI;  // 20037508.342789244

WMSMiniDriver_VirtualEarth::WMSMiniDriver_VirtualEarth() {}

WMSMiniDriver_VirtualEarth::~WMSMiniDriver_VirtualEarth() {}

CPLErr WMSMiniDriver_VirtualEarth::Initialize(CPLXMLNode *config, CPL_UNUSED char **papszOpenOptions)
{
    m_base_url = CPLGetXMLValue(config, "ServerURL", "");
    if (m_base_url.empty()) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "GDALWMS, VirtualEarth mini-driver: ServerURL missing.");
        return CE_Failure;
    }

    if (m_base_url.find("${quadkey}") == std::string::npos) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "GDALWMS, VirtualEarth mini-driver: ${quadkey} missing in ServerURL.");
        return CE_Failure;
    }

    m_parent_dataset->WMSSetDefaultBlockSize(256, 256);
    m_parent_dataset->WMSSetDefaultDataWindowCoordinates(-MAX_GM, MAX_GM, MAX_GM, -MAX_GM);
    m_parent_dataset->WMSSetDefaultTileLevel(21);
    m_parent_dataset->WMSSetDefaultOverviewCount(20);
    m_parent_dataset->WMSSetNeedsDataWindow(FALSE);
    m_projection_wkt = ProjToWKT("EPSG:3857");
    return CE_None;
}

CPLErr WMSMiniDriver_VirtualEarth::TiledImageRequest(WMSHTTPRequest &request,
                                                CPL_UNUSED const GDALWMSImageRequestInfo &iri,
                                                const GDALWMSTiledImageRequestInfo &tiri)
{
    CPLString &url = request.URL;
    url = m_base_url;

    char szTileNumber[64];
    int x = tiri.m_x;
    int y = tiri.m_y;
    int z = std::min(32, tiri.m_level);

    for(int i = 0; i < z; i ++)
    {
        int row = (y & 1);
        int col = (x & 1);

        szTileNumber[z-1-i] = (char) ('0' + (col | (row << 1)));

        x = x >> 1;
        y = y >> 1;
    }
    szTileNumber[z] = 0;

    URLSearchAndReplace(&url, "${quadkey}", "%s", szTileNumber);
    // Sounds like this should be random
    URLSearchAndReplace(&url, "${server_num}", "%d", (tiri.m_x + tiri.m_y + z) % 4);
    return CE_None;
}
