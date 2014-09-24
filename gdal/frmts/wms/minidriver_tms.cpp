/******************************************************************************
 * $Id$
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Chris Schmidt
 *
 ******************************************************************************
 * Copyright (c) 2007, Chris Schmidt
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
#include "minidriver_tms.h"


CPP_GDALWMSMiniDriverFactory(TMS)

GDALWMSMiniDriver_TMS::GDALWMSMiniDriver_TMS() {
}

GDALWMSMiniDriver_TMS::~GDALWMSMiniDriver_TMS() {
}

CPLErr GDALWMSMiniDriver_TMS::Initialize(CPLXMLNode *config) {
    CPLErr ret = CE_None;

    if (ret == CE_None) {
        const char *base_url = CPLGetXMLValue(config, "ServerURL", "");
        if (base_url[0] != '\0') {
            m_base_url = base_url;
            if (m_base_url.find("${") == std::string::npos) {
                if (m_base_url[m_base_url.size()-1] != '/') {
                    m_base_url += "/";
                }
                m_base_url += "${version}/${layer}/${z}/${x}/${y}.${format}";
            }
        } else {
            CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, TMS mini-driver: ServerURL missing.");
            ret = CE_Failure;
        }
    }

    m_dataset  = CPLGetXMLValue(config, "Layer", "");
    m_version  = CPLGetXMLValue(config, "Version", "1.0.0");
    m_format   = CPLGetXMLValue(config, "Format", "jpg");

    return ret;
}

void GDALWMSMiniDriver_TMS::GetCapabilities(GDALWMSMiniDriverCapabilities *caps) {
    caps->m_capabilities_version = 1;
    caps->m_has_arb_overviews = 0;
    caps->m_has_image_request = 0;
    caps->m_has_tiled_image_requeset = 1;
    caps->m_max_overview_count = 32;
}

void GDALWMSMiniDriver_TMS::ImageRequest(CPL_UNUSED CPLString *url,
                                         CPL_UNUSED const GDALWMSImageRequestInfo &iri) {
}

void GDALWMSMiniDriver_TMS::TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri) {
    const GDALWMSDataWindow *data_window = m_parent_dataset->WMSGetDataWindow();
    int tms_y;

    if (data_window->m_y_origin != GDALWMSDataWindow::TOP) {
        tms_y = static_cast<int>(floor(((data_window->m_y1 - data_window->m_y0)
                                      / (iri.m_y1 - iri.m_y0)) + 0.5)) - tiri.m_y - 1;
    } else {
        tms_y = tiri.m_y;
    }
    // http://tms25.arc.nasa.gov/tile/tile.aspx?T=geocover2000&L=0&X=86&Y=39
    *url = m_base_url;

    URLSearchAndReplace(url, "${version}", "%s", m_version.c_str());
    URLSearchAndReplace(url, "${layer}", "%s", m_dataset.c_str());
    URLSearchAndReplace(url, "${format}", "%s", m_format.c_str());
    URLSearchAndReplace(url, "${x}", "%d", tiri.m_x);
    URLSearchAndReplace(url, "${y}", "%d", tms_y);
    URLSearchAndReplace(url, "${z}", "%d", tiri.m_level);

    /* Hack for some TMS like servers that require tile numbers split into 3 groups of */
    /* 3 digits, like http://tile8.geo.admin.ch/geoadmin/ch.swisstopo.pixelkarte-farbe */
    URLSearchAndReplace(url, "${xxx}", "%03d/%03d/%03d", tiri.m_x / 1000000, (tiri.m_x / 1000) % 1000, tiri.m_x % 1000);
    URLSearchAndReplace(url, "${yyy}", "%03d/%03d/%03d", tms_y / 1000000, (tms_y / 1000) % 1000, tms_y % 1000);

}
