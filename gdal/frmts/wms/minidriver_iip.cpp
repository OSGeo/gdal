/******************************************************************************
 * $Id$
 *
 * Project:  WMS Client Driver
 * Purpose:  Mini driver for Internel Imaging Protocol (IIP)
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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
#include "minidriver_iip.h"


CPP_GDALWMSMiniDriverFactory(IIP)

GDALWMSMiniDriver_IIP::GDALWMSMiniDriver_IIP() {
}

GDALWMSMiniDriver_IIP::~GDALWMSMiniDriver_IIP() {
}

CPLErr GDALWMSMiniDriver_IIP::Initialize(CPLXMLNode *config) {
    CPLErr ret = CE_None;

    if (ret == CE_None) {
        const char *base_url = CPLGetXMLValue(config, "ServerURL", "");
        if (base_url[0] != '\0') {
            m_base_url = base_url;
        } else {
            CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, IIP mini-driver: ServerURL missing.");
            ret = CE_Failure;
        }
    }

    return ret;
}

void GDALWMSMiniDriver_IIP::GetCapabilities(GDALWMSMiniDriverCapabilities *caps) {
    caps->m_capabilities_version = 1;
    caps->m_has_arb_overviews = 0;
    caps->m_has_image_request = 0;
    caps->m_has_tiled_image_requeset = 1;
    caps->m_max_overview_count = 32;
    caps->m_overview_dim_computation_method = OVERVIEW_FLOOR;
    caps->m_has_geotransform = false;
}

void GDALWMSMiniDriver_IIP::TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri)
{
    int nTileXCount = ((m_parent_dataset->GetRasterXSize()>> (m_parent_dataset->GetRasterBand(1)->GetOverviewCount()-tiri.m_level)) + 255) / 256;
    int numTile = tiri.m_x + tiri.m_y * nTileXCount;
    
    *url = m_base_url;
    *url += CPLSPrintf("&jtl=%d,%d", tiri.m_level, numTile);
}
