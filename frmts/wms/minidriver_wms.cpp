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

#include "stdinc.h"

CPP_GDALWMSMiniDriverFactory(WMS)

GDALWMSMiniDriver_WMS::GDALWMSMiniDriver_WMS() {
}

GDALWMSMiniDriver_WMS::~GDALWMSMiniDriver_WMS() {
}

CPLErr GDALWMSMiniDriver_WMS::Initialize(CPLXMLNode *config) {
    CPLErr ret = CE_None;

    if (ret == CE_None) {
        const char *version = CPLGetXMLValue(config, "Version", "1.1.0");
        if (version[0] != '\0') {
            m_version = version;
            m_iversion = VersionStringToInt(version);
            if (m_iversion == -1) {
                CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, WMS mini-driver: Invalid version.");
                ret = CE_Failure;
            }
        } else {
            CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, WMS mini-driver: Version missing.");
            ret = CE_Failure;
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
            CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, WMS mini-driver: ServerURL missing.");
            ret = CE_Failure;
        }
    }

    if (ret == CE_None) {
/* SRS is WMS version 1.1 and earlier, if SRS is not set use default unless CRS is set
   CRS is WMS version 1.3, if CRS is not set use default unless SRS is set */
        const char *crs = CPLGetXMLValue(config, "CRS", "");
        const char *srs = CPLGetXMLValue(config, "SRS", "");
        if (m_iversion >= VersionStringToInt("1.3")) {
            /* Version 1.3 and above */
            if ((srs[0] != '\0') && (crs[0] == '\0')) {
                CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, WMS mini-driver: WMS version 1.3 and above expects CRS however SRS was set instead.");
                ret = CE_Failure;
            } else if (crs[0] != '\0') {
                m_crs = crs;
            } else {
                m_crs = "EPSG:4326";
            }
        } else {
            /* Version 1.1.1 and below */
            if ((srs[0] == '\0') && (crs[0] != '\0')) {
                CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, WMS mini-driver: WMS version 1.1.1 and below expects SRS however CRS was set instead.");
                ret = CE_Failure;
            } else if (srs[0] != '\0') {
                m_srs = srs;
            } else {
                m_srs = "EPSG:4326";
            }
        }
    }

    if (ret == CE_None) {
        if (m_srs.size()) {
            m_projection_wkt = ProjToWKT(m_srs);
        } else if (m_crs.size()) {
            m_projection_wkt = ProjToWKT(m_crs);
        }
    }

    if (ret == CE_None) {
        m_image_format = CPLGetXMLValue(config, "ImageFormat", "image/jpeg");
        m_layers = CPLGetXMLValue(config, "Layers", "");
        m_styles = CPLGetXMLValue(config, "Styles", "");
        m_transparent = CPLGetXMLValue(config, "Transparent","");
        // the transparent flag needs to be "TRUE" or "FALSE" in upper case according to the WMS spec so force upper case
        for(int i=0; i<(int)m_transparent.size();i++)
        {
            m_transparent[i] = (char) toupper(m_transparent[i]);
        }
    }

    if (ret == CE_None) {
        const char *bbox_order = CPLGetXMLValue(config, "BBoxOrder", "xyXY");
        if (bbox_order[0] != '\0') {
            int i;
            for (i = 0; i < 4; ++i) {
                if ((bbox_order[i] != 'x') && (bbox_order[i] != 'y') && (bbox_order[i] != 'X') && (bbox_order[i] != 'Y')) break;
            }
            if (i == 4) {
                m_bbox_order = bbox_order;
            } else {
                CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, WMS mini-driver: Incorrect BBoxOrder.");
                ret = CE_Failure;
            }
        } else {
            CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, WMS mini-driver: BBoxOrder missing.");
            ret = CE_Failure;
        }
    }

    return ret;
}

void GDALWMSMiniDriver_WMS::GetCapabilities(GDALWMSMiniDriverCapabilities *caps) {
    caps->m_capabilities_version = 1;
    caps->m_has_arb_overviews = 1;
    caps->m_has_image_request = 1;
    caps->m_has_tiled_image_requeset = 1;
    caps->m_max_overview_count = 32;
}

void GDALWMSMiniDriver_WMS::ImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri) {
    // http://onearth.jpl.nasa.gov/wms.cgi?request=GetMap&width=1000&height=500&layers=modis,global_mosaic&styles=&srs=EPSG:4326&format=image/jpeg&bbox=-180.000000,-90.000000,180.000000,090.000000    
    *url = m_base_url;
    if (m_base_url.ifind( "service=") == std::string::npos)
        URLAppend(url, "&service=WMS");
    URLAppend(url, "&request=GetMap");
    URLAppendF(url, "&version=%s", m_version.c_str());
    URLAppendF(url, "&layers=%s", m_layers.c_str());
    URLAppendF(url, "&styles=%s", m_styles.c_str());
    if (m_srs.size()) URLAppendF(url, "&srs=%s", m_srs.c_str());
    if (m_crs.size()) URLAppendF(url, "&crs=%s", m_crs.c_str());
    if (m_transparent.size()) URLAppendF(url, "&transparent=%s", m_transparent.c_str());
    URLAppendF(url, "&format=%s", m_image_format.c_str());
    URLAppendF(url, "&width=%d", iri.m_sx);
    URLAppendF(url, "&height=%d", iri.m_sy);
    URLAppendF(url, "&bbox=%.8f,%.8f,%.8f,%.8f", 
        GetBBoxCoord(iri, m_bbox_order[0]), GetBBoxCoord(iri, m_bbox_order[1]), 
        GetBBoxCoord(iri, m_bbox_order[2]), GetBBoxCoord(iri, m_bbox_order[3]));
    CPLDebug("WMS", "URL = %s", url->c_str());
}

void GDALWMSMiniDriver_WMS::TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri) {
    ImageRequest(url, iri);
}

const char *GDALWMSMiniDriver_WMS::GetProjectionInWKT() {
    return m_projection_wkt.c_str();
}

double GDALWMSMiniDriver_WMS::GetBBoxCoord(const GDALWMSImageRequestInfo &iri, char what) {
    switch (what) {
        case 'x': return MIN(iri.m_x0, iri.m_x1);
        case 'y': return MIN(iri.m_y0, iri.m_y1);
        case 'X': return MAX(iri.m_x0, iri.m_x1);
        case 'Y': return MAX(iri.m_y0, iri.m_y1);
    }
    return 0.0;
}
