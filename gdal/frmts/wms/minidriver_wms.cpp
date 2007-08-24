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

#include "stdinc.h"

CPP_GDALWMSMiniDriverFactory(WMS)

GDALWMSMiniDriver_WMS::GDALWMSMiniDriver_WMS() {
}

GDALWMSMiniDriver_WMS::~GDALWMSMiniDriver_WMS() {
}

CPLErr GDALWMSMiniDriver_WMS::Initialize(CPLXMLNode *config) {
    m_version = CPLGetXMLValue(config, "Version", "1.1.0");
    m_base_url = CPLGetXMLValue(config, "ServerUrl", "");
    m_crs = CPLGetXMLValue(config, "CRS", "CRS:83");
    m_srs = CPLGetXMLValue(config, "SRS", "EPSG:4326");
    m_image_format = CPLGetXMLValue(config, "ImageFormat", "image/jpeg");
    m_layers = CPLGetXMLValue(config, "Layers", "");
    m_styles = CPLGetXMLValue(config, "Styles", "");

    return CE_None;
}

void GDALWMSMiniDriver_WMS::GetCapabilities(GDALWMSMiniDriverCapabilities *caps) {
    caps->m_has_arb_overviews = 1;
    caps->m_has_image_request = 1;
    caps->m_has_tiled_image_requeset = 1;
    caps->m_max_overview_count = 32;
}

void GDALWMSMiniDriver_WMS::ImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri) {
    // http://onearth.jpl.nasa.gov/wms.cgi?request=GetMap&width=1000&height=500&layers=modis,global_mosaic&styles=&srs=EPSG:4326&format=image/jpeg&bbox=-180.000000,-90.000000,180.000000,090.000000    
    *url = m_base_url;
    URLAppend(url, "&request=GetMap");
    URLAppendF(url, "&version=%s", m_version.c_str());
    URLAppendF(url, "&layers=%s", m_layers.c_str());
    URLAppendF(url, "&styles=%s", m_styles.c_str());
    URLAppendF(url, "&srs=%s", m_srs.c_str());
    URLAppendF(url, "&format=%s", m_image_format.c_str());
    URLAppendF(url, "&width=%d", iri.m_sx);
    URLAppendF(url, "&height=%d", iri.m_sy);
    URLAppendF(url, "&bbox=%f,%f,%f,%f", iri.m_x0, MIN(iri.m_y0, iri.m_y1), iri.m_x1, MAX(iri.m_y0, iri.m_y1));
}

void GDALWMSMiniDriver_WMS::TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri) {
    ImageRequest(url, iri);
}
