/******************************************************************************
 * Project:  WMS Client Driver
 * Purpose:  OGC API coverage
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "wmsdriver.h"
#include "minidriver_ogcapicoverage.h"

CPLErr
WMSMiniDriver_OGCAPICoverage::Initialize(CPLXMLNode *config,
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
                     "GDALWMS, OGCAPICoverage mini-driver: ServerURL missing.");
            ret = CE_Failure;
        }
    }

    return ret;
}

CPLErr WMSMiniDriver_OGCAPICoverage::TiledImageRequest(
    WMSHTTPRequest &request, const GDALWMSImageRequestInfo &iri,
    const GDALWMSTiledImageRequestInfo &)
{
    CPLString &url = request.URL;

    url = m_base_url;

    URLSearchAndReplace(&url, "${width}", "%d", iri.m_sx);
    URLSearchAndReplace(&url, "${height}", "%d", iri.m_sy);
    URLSearchAndReplace(&url, "${minx}", "%.17g", iri.m_x0);
    URLSearchAndReplace(&url, "${miny}", "%.17g", iri.m_y1);
    URLSearchAndReplace(&url, "${maxx}", "%.17g", iri.m_x1);
    URLSearchAndReplace(&url, "${maxy}", "%.17g", iri.m_y0);
    /*URLSearchAndReplace(&url, "${minx_centerpixel}", "%.17g", iri.m_x0 + 0.5 *
    (iri.m_x1 - iri.m_x0) / iri.m_sx); URLSearchAndReplace(&url,
    "${miny_centerpixel}", "%.17g", iri.m_y1 - 0.5 * (iri.m_y1 - iri.m_y0) /
    iri.m_sy); URLSearchAndReplace(&url, "${maxx_centerpixel}", "%.17g",
    iri.m_x1 - 0.5 * (iri.m_x1 - iri.m_x0) / iri.m_sx);
    URLSearchAndReplace(&url, "${maxy_centerpixel}", "%.17g", iri.m_y0 + 0.5 *
    (iri.m_y1 - iri.m_y0) / iri.m_sy);*/

    return CE_None;
}
