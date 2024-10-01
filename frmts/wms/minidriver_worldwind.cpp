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
#include "minidriver_worldwind.h"

WMSMiniDriver_WorldWind::WMSMiniDriver_WorldWind()
{
}

WMSMiniDriver_WorldWind::~WMSMiniDriver_WorldWind()
{
}

CPLErr WMSMiniDriver_WorldWind::Initialize(CPLXMLNode *config,
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
        const char *dataset = CPLGetXMLValue(config, "Layer", "");
        URLPrepare(m_base_url);
        m_base_url += CPLOPrintf("T=%s", dataset);
    }

    m_oSRS.importFromEPSG(4326);
    return ret;
}

CPLErr WMSMiniDriver_WorldWind::TiledImageRequest(
    WMSHTTPRequest &request, const GDALWMSImageRequestInfo &iri,
    const GDALWMSTiledImageRequestInfo &tiri)
{
    CPLString &url = request.URL;
    const GDALWMSDataWindow *data_window = m_parent_dataset->WMSGetDataWindow();
    int worldwind_y =
        static_cast<int>(floor(
            ((data_window->m_y1 - data_window->m_y0) / (iri.m_y1 - iri.m_y0)) +
            0.5)) -
        tiri.m_y - 1;
    // http://worldwind25.arc.nasa.gov/tile/tile.aspx?T=geocover2000&L=0&X=86&Y=39
    url = m_base_url +
          CPLOPrintf("L=%d&X=%d&Y=%d", tiri.m_level, tiri.m_x, worldwind_y);
    return CE_None;
}
