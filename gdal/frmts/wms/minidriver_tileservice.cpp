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
#include "minidriver_tileservice.h"

CPL_CVSID("$Id$")

WMSMiniDriver_TileService::WMSMiniDriver_TileService() {}

WMSMiniDriver_TileService::~WMSMiniDriver_TileService() {}

CPLErr WMSMiniDriver_TileService::Initialize(CPLXMLNode *config, CPL_UNUSED char **papszOpenOptions) {
    CPLErr ret = CE_None;

    // Try both spellings
    m_base_url = CPLGetXMLValue(config, "ServerURL",
                                CPLGetXMLValue(config, "ServerUrl", ""));

    if (m_base_url.empty()) {
        CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, TileService mini-driver: ServerURL missing.");
        ret = CE_Failure;
    }
    else { // Prepare the url, leave it ready for extra arguments
        URLPrepare(m_base_url);
        const char *dataset = CPLGetXMLValue(config, "Dataset", "");
        const char *version = CPLGetXMLValue(config, "Version", "1");
        m_base_url += CPLOPrintf("interface=map&version=%s&dataset=%s&", version, dataset);
    }

    return ret;
}

CPLErr WMSMiniDriver_TileService::TiledImageRequest(WMSHTTPRequest &request,
                                                      CPL_UNUSED const GDALWMSImageRequestInfo &iri,
                                                      const GDALWMSTiledImageRequestInfo &tiri) {
    // http://s0.tileservice.worldwindcentral.com/getTile?interface=map&version=1&dataset=bmng.topo.bathy.200401&level=5&x=18&y=6
    CPLString &url = request.URL;
    url = m_base_url;
    url += CPLOPrintf("level=%d&x=%d&y=%d", tiri.m_level, tiri.m_x, tiri.m_y);
    return CE_None;
}
