/******************************************************************************
 * $Id$
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

CPP_GDALWMSMiniDriverFactory(TileService)

GDALWMSMiniDriver_TileService::GDALWMSMiniDriver_TileService() {
}

GDALWMSMiniDriver_TileService::~GDALWMSMiniDriver_TileService() {
}

CPLErr GDALWMSMiniDriver_TileService::Initialize(CPLXMLNode *config) {
    CPLErr ret = CE_None;

    if (ret == CE_None) {
        const char *version = CPLGetXMLValue(config, "Version", "1");
        if (version[0] != '\0') {
            m_version = version;
        }
    }

    if (ret == CE_None) {
        const char *base_url = CPLGetXMLValue(config, "ServerURL", "");
        if (base_url[0] != '\0') {
            /* Try the old name */
            base_url = CPLGetXMLValue(config, "ServerUrl", "");
        }
        if (base_url[0] != '\0') {
            m_base_url = base_url;
        } else {
            CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, TileService mini-driver: ServerURL missing.");
            ret = CE_Failure;
        }
    }

    m_dataset = CPLGetXMLValue(config, "Dataset", "");

    return ret;
}

void GDALWMSMiniDriver_TileService::GetCapabilities(GDALWMSMiniDriverCapabilities *caps) {
    caps->m_capabilities_version = 1;
    caps->m_has_arb_overviews = 0;
    caps->m_has_image_request = 0;
    caps->m_has_tiled_image_requeset = 1;
    caps->m_max_overview_count = 32;
}

void GDALWMSMiniDriver_TileService::ImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri) {
}

void GDALWMSMiniDriver_TileService::TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri) {
    // http://s0.tileservice.worldwindcentral.com/getTile?interface=map&version=1&dataset=bmng.topo.bathy.200401&level=5&x=18&y=6
    *url = m_base_url;
    URLAppend(url, "&interface=map");
    URLAppendF(url, "&version=%s", m_version.c_str());
    URLAppendF(url, "&dataset=%s", m_dataset.c_str());
    URLAppendF(url, "&level=%d", tiri.m_level);
    URLAppendF(url, "&x=%d", tiri.m_x);
    URLAppendF(url, "&y=%d", tiri.m_y);
}
