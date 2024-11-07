/******************************************************************************
 * Project:  WMS Client Driver
 * Purpose:  OGC API maps
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "wmsdriver.h"
#include "minidriver_ogcapimaps.h"

CPLErr WMSMiniDriver_OGCAPIMaps::Initialize(CPLXMLNode *config,
                                            CPL_UNUSED char **papszOpenOptions)
{
    CPLErr ret = CE_None;

    {
        const char *base_url = CPLGetXMLValue(config, "ServerURL", "");
        if (base_url[0] != '\0')
        {
            m_base_url = base_url;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GDALWMS, OGCAPIMaps mini-driver: ServerURL missing.");
            ret = CE_Failure;
        }
    }

    return ret;
}

CPLErr WMSMiniDriver_OGCAPIMaps::TiledImageRequest(
    WMSHTTPRequest &request, const GDALWMSImageRequestInfo &iri,
    const GDALWMSTiledImageRequestInfo &)
{
    CPLString &url = request.URL;

    url = m_base_url;

    URLPrepare(url);
    url +=
        CPLOPrintf("width=%d&height=%d&bbox=%.17g,%.17g,%.17g,%.17g", iri.m_sx,
                   iri.m_sy, iri.m_x0, iri.m_y1, iri.m_x1, iri.m_y0);

    return CE_None;
}
