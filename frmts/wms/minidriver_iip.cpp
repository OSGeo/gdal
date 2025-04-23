/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Mini driver for Internet Imaging Protocol (IIP)
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "wmsdriver.h"
#include "minidriver_iip.h"

WMSMiniDriver_IIP::WMSMiniDriver_IIP()
{
}

WMSMiniDriver_IIP::~WMSMiniDriver_IIP()
{
}

CPLErr WMSMiniDriver_IIP::Initialize(CPLXMLNode *config,
                                     CPL_UNUSED char **papszOpenOptions)
{
    CPLErr ret = CE_None;

    m_base_url = CPLGetXMLValue(config, "ServerURL", "");
    if (m_base_url.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALWMS, IIP mini-driver: ServerURL missing.");
        return CE_Failure;
    }

    return ret;
}

void WMSMiniDriver_IIP::GetCapabilities(WMSMiniDriverCapabilities *caps)
{
    caps->m_overview_dim_computation_method = OVERVIEW_FLOOR;
    caps->m_has_geotransform = false;
}

CPLErr
WMSMiniDriver_IIP::TiledImageRequest(WMSHTTPRequest &request,
                                     const GDALWMSImageRequestInfo & /* iri */,
                                     const GDALWMSTiledImageRequestInfo &tiri)
{
    CPLString &url = request.URL;
    url = m_base_url;
    URLPrepare(url);

    int nTileXCount =
        ((m_parent_dataset->GetRasterXSize() >>
          (m_parent_dataset->GetRasterBand(1)->GetOverviewCount() -
           tiri.m_level)) +
         255) /
        256;
    int numTile = tiri.m_x + tiri.m_y * nTileXCount;
    url += CPLOPrintf("jtl=%d,%d", tiri.m_level, numTile);
    return CE_None;
}
