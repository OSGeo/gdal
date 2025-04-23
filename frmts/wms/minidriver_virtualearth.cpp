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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "wmsdriver.h"
#include "minidriver_virtualearth.h"

#include <algorithm>

// These should be global, they are used all over the place
const double SPHERICAL_RADIUS = 6378137.0;
const double MAX_GM = SPHERICAL_RADIUS * M_PI;  // 20037508.342789244

WMSMiniDriver_VirtualEarth::WMSMiniDriver_VirtualEarth()
{
}

WMSMiniDriver_VirtualEarth::~WMSMiniDriver_VirtualEarth()
{
}

CPLErr
WMSMiniDriver_VirtualEarth::Initialize(CPLXMLNode *config,
                                       CPL_UNUSED char **papszOpenOptions)
{
    m_base_url = CPLGetXMLValue(config, "ServerURL", "");
    if (m_base_url.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALWMS, VirtualEarth mini-driver: ServerURL missing.");
        return CE_Failure;
    }

    if (m_base_url.find("${quadkey}") == std::string::npos)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALWMS, VirtualEarth mini-driver: ${quadkey} missing in "
                 "ServerURL.");
        return CE_Failure;
    }

    m_parent_dataset->WMSSetDefaultBlockSize(256, 256);
    m_parent_dataset->WMSSetDefaultDataWindowCoordinates(-MAX_GM, MAX_GM,
                                                         MAX_GM, -MAX_GM);
    m_parent_dataset->WMSSetDefaultTileLevel(21);
    m_parent_dataset->WMSSetDefaultOverviewCount(20);
    m_parent_dataset->WMSSetNeedsDataWindow(FALSE);
    m_oSRS.importFromEPSG(3857);
    return CE_None;
}

CPLErr WMSMiniDriver_VirtualEarth::TiledImageRequest(
    WMSHTTPRequest &request, CPL_UNUSED const GDALWMSImageRequestInfo &iri,
    const GDALWMSTiledImageRequestInfo &tiri)
{
    CPLString &url = request.URL;
    url = m_base_url;

    char szTileNumber[64];
    int x = tiri.m_x;
    int y = tiri.m_y;
    int z = std::min(32, tiri.m_level);

    for (int i = 0; i < z; i++)
    {
        int row = (y & 1);
        int col = (x & 1);

        szTileNumber[z - 1 - i] = (char)('0' + (col | (row << 1)));

        x = x >> 1;
        y = y >> 1;
    }
    szTileNumber[z] = 0;

    URLSearchAndReplace(&url, "${quadkey}", "%s", szTileNumber);
    // Sounds like this should be random
    URLSearchAndReplace(&url, "${server_num}", "%d",
                        (tiri.m_x + tiri.m_y + z) % 4);
    return CE_None;
}
