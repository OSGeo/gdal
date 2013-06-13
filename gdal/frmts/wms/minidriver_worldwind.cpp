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
#include "minidriver_worldwind.h"

CPP_GDALWMSMiniDriverFactory(WorldWind)

GDALWMSMiniDriver_WorldWind::GDALWMSMiniDriver_WorldWind() {
}

GDALWMSMiniDriver_WorldWind::~GDALWMSMiniDriver_WorldWind() {
}

CPLErr GDALWMSMiniDriver_WorldWind::Initialize(CPLXMLNode *config) {
    CPLErr ret = CE_None;

    if (ret == CE_None) {
        const char *base_url = CPLGetXMLValue(config, "ServerURL", "");
        if (base_url[0] != '\0') {
            /* Try the old name */
            base_url = CPLGetXMLValue(config, "ServerUrl", "");
        }
        if (base_url[0] != '\0') {
            m_base_url = base_url;
        } else {
            CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, WorldWind mini-driver: ServerURL missing.");
            ret = CE_Failure;
        }
    }

    m_dataset = CPLGetXMLValue(config, "Layer", "");
    m_projection_wkt = ProjToWKT("EPSG:4326");

    return ret;
}

void GDALWMSMiniDriver_WorldWind::GetCapabilities(GDALWMSMiniDriverCapabilities *caps) {
    caps->m_capabilities_version = 1;
    caps->m_has_arb_overviews = 0;
    caps->m_has_image_request = 0;
    caps->m_has_tiled_image_requeset = 1;
    caps->m_max_overview_count = 32;
}

void GDALWMSMiniDriver_WorldWind::ImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri) {
}

void GDALWMSMiniDriver_WorldWind::TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri) {
    const GDALWMSDataWindow *data_window = m_parent_dataset->WMSGetDataWindow();
    int worldwind_y = static_cast<int>(floor(((data_window->m_y1 - data_window->m_y0) / (iri.m_y1 - iri.m_y0)) + 0.5)) - tiri.m_y - 1;
    // http://worldwind25.arc.nasa.gov/tile/tile.aspx?T=geocover2000&L=0&X=86&Y=39
    *url = m_base_url;
    URLAppendF(url, "&T=%s", m_dataset.c_str());
    URLAppendF(url, "&L=%d", tiri.m_level);
    URLAppendF(url, "&X=%d", tiri.m_x);
    URLAppendF(url, "&Y=%d", worldwind_y);
}

const char *GDALWMSMiniDriver_WorldWind::GetProjectionInWKT() {
    return m_projection_wkt.c_str();
}
