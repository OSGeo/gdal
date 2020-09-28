/******************************************************************************
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

CPL_CVSID("$Id$")

WMSMiniDriver_TMS::WMSMiniDriver_TMS() {}

WMSMiniDriver_TMS::~WMSMiniDriver_TMS() {}

CPLErr WMSMiniDriver_TMS::Initialize(CPLXMLNode *config, CPL_UNUSED char **papszOpenOptions) {
    CPLErr ret = CE_None;

    {
        const char *base_url = CPLGetXMLValue(config, "ServerURL", "");
        if (base_url[0] != '\0') {
            m_base_url = base_url;
            if (m_base_url.find("${") == std::string::npos) {
                if (m_base_url.back() != '/') {
                    m_base_url += "/";
                }
                m_base_url += "${version}/${layer}/${z}/${x}/${y}.${format}";
            }
        } else {
            CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, TMS mini-driver: ServerURL missing.");
            ret = CE_Failure;
        }
    }

    // These never change
    const char *dataset = CPLGetXMLValue(config, "Layer", "");
    URLSearchAndReplace(&m_base_url, "${layer}", "%s", dataset);
    const char *version = CPLGetXMLValue(config, "Version", "1.0.0");
    URLSearchAndReplace(&m_base_url, "${version}", "%s", version);
    const char *format = CPLGetXMLValue(config, "Format", "jpg");
    URLSearchAndReplace(&m_base_url, "${format}", "%s", format);

    m_nTileXMultiplier = atoi(
        CPLGetXMLValue(config, "TileXMultiplier", "1"));

    return ret;
}

CPLErr WMSMiniDriver_TMS::TiledImageRequest(WMSHTTPRequest &request,
                                            const GDALWMSImageRequestInfo &iri,
                                            const GDALWMSTiledImageRequestInfo &tiri)
{
    CPLString &url = request.URL;
    const GDALWMSDataWindow *data_window = m_parent_dataset->WMSGetDataWindow();
    int tms_y;

    if (data_window->m_y_origin != GDALWMSDataWindow::TOP) {
        if( iri.m_y0 == iri.m_y1 )
            return CE_Failure;
        const double dfTmp = floor(((data_window->m_y1 - data_window->m_y0)
                                      / (iri.m_y1 - iri.m_y0)) + 0.5);
        if( !(dfTmp >= 0 && dfTmp < INT_MAX) )
            return CE_Failure;
        tms_y = static_cast<int>(dfTmp) - tiri.m_y - 1;
    } else {
        tms_y = tiri.m_y;
    }
    // http://tms25.arc.nasa.gov/tile/tile.aspx?T=geocover2000&L=0&X=86&Y=39
    url = m_base_url;

    URLSearchAndReplace(&url, "${x}", "%d", tiri.m_x * m_nTileXMultiplier);
    URLSearchAndReplace(&url, "${y}", "%d", tms_y);
    URLSearchAndReplace(&url, "${z}", "%d", tiri.m_level);

    /* Hack for some TMS like servers that require tile numbers split into 3 groups of */
    /* 3 digits, like http://tile8.geo.admin.ch/geoadmin/ch.swisstopo.pixelkarte-farbe */
    URLSearchAndReplace(&url, "${xxx}", "%03d/%03d/%03d", tiri.m_x / 1000000, (tiri.m_x / 1000) % 1000, tiri.m_x % 1000);
    URLSearchAndReplace(&url, "${yyy}", "%03d/%03d/%03d", tms_y / 1000000, (tms_y / 1000) % 1000, tms_y % 1000);

    return CE_None;
}
