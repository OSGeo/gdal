/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "wmsdriver.h"
#include "minidriver_tileservice.h"

WMSMiniDriver_TileService::WMSMiniDriver_TileService()
{
}

WMSMiniDriver_TileService::~WMSMiniDriver_TileService()
{
}

CPLErr WMSMiniDriver_TileService::Initialize(CPLXMLNode *config,
                                             CPL_UNUSED char **papszOpenOptions)
{
    CPLErr ret = CE_None;

    // Try both spellings
    m_base_url = CPLGetXMLValue(config, "ServerURL",
                                CPLGetXMLValue(config, "ServerUrl", ""));

    if (m_base_url.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALWMS, TileService mini-driver: ServerURL missing.");
        ret = CE_Failure;
    }
    else
    {  // Prepare the url, leave it ready for extra arguments
        URLPrepare(m_base_url);
        const char *dataset = CPLGetXMLValue(config, "Dataset", "");
        const char *version = CPLGetXMLValue(config, "Version", "1");
        m_base_url += CPLOPrintf("interface=map&version=%s&dataset=%s&",
                                 version, dataset);
    }

    return ret;
}

CPLErr WMSMiniDriver_TileService::TiledImageRequest(
    WMSHTTPRequest &request, CPL_UNUSED const GDALWMSImageRequestInfo &iri,
    const GDALWMSTiledImageRequestInfo &tiri)
{
    // http://s0.tileservice.worldwindcentral.com/getTile?interface=map&version=1&dataset=bmng.topo.bathy.200401&level=5&x=18&y=6
    CPLString &url = request.URL;
    url = m_base_url;
    url += CPLOPrintf("level=%d&x=%d&y=%d", tiri.m_level, tiri.m_x, tiri.m_y);
    return CE_None;
}
