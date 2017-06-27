/******************************************************************************
 *
 * Project:  Arc GIS Server Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Alexander Lisovenko
 *
 ******************************************************************************
 * Copyright (c) 2014-2015, NextGIS <info@nextgis.ru>
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
#include "minidriver_arcgis_server.h"

#include <algorithm>

CPL_CVSID("$Id$")

WMSMiniDriver_AGS::WMSMiniDriver_AGS() {}

WMSMiniDriver_AGS::~WMSMiniDriver_AGS() {}

static double GetBBoxCoord(const GDALWMSImageRequestInfo &iri, char what)
{
    switch (what)
    {
    case 'x': return std::min(iri.m_x0, iri.m_x1);
    case 'y': return std::min(iri.m_y0, iri.m_y1);
    case 'X': return std::max(iri.m_x0, iri.m_x1);
    case 'Y': return std::max(iri.m_y0, iri.m_y1);
    }
    return 0.0;
}

char **WMSMiniDriver_AGS::GetMetadataDomainList(void) {
    return CSLAddString(NULL, "LocationInfo");
}

CPLErr WMSMiniDriver_AGS::Initialize(CPLXMLNode *config, CPL_UNUSED char **papszOpenOptions)
{
    // Bounding box, if specified, has to be xyXY
    m_bbox_order = CPLGetXMLValue(config, "BBoxOrder", "xyXY");
    if (m_bbox_order.size() < 4 || m_bbox_order.find("xyXY") != 0) {
        CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS: ArcGIS BBoxOrder value has to be xyXY");
        return CE_Failure;
    }

    m_base_url = CPLGetXMLValue(config, "ServerURL", CPLGetXMLValue(config, "ServerUrl", ""));
    if (m_base_url.empty()) {
        CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS: ArcGIS Server mini-driver: ServerURL missing.");
        return CE_Failure;
    }

    m_image_format = CPLGetXMLValue(config, "ImageFormat", "png");
    m_time_range = CPLGetXMLValue(config, "TimeRange", "");
    m_transparent = CPLGetXMLValue(config, "Transparent", "");
    m_transparent.tolower();
    m_layers = CPLGetXMLValue(config, "Layers", "");

    const char* irs = CPLGetXMLValue(config, "SRS", "102100");

    if (irs != NULL)
    {
        if (STARTS_WITH_CI(irs, "EPSG:")) //if we have EPSG code just convert it to WKT
        {
            m_projection_wkt = ProjToWKT(irs);
            m_irs = irs + 5;
        }
        else //if we have AGS code - try if it's EPSG
        {
            m_irs = irs;
            m_projection_wkt = ProjToWKT("EPSG:" + m_irs);
        }
        // TODO: if we have AGS JSON
    }
    m_identification_tolerance = CPLGetXMLValue(config, "IdentificationTolerance", "2");

    return CE_None;
}

void WMSMiniDriver_AGS::GetCapabilities(WMSMiniDriverCapabilities *caps)
{
    caps->m_has_getinfo = 1;
}

CPLErr WMSMiniDriver_AGS::TiledImageRequest(WMSHTTPRequest &request,
                                                const GDALWMSImageRequestInfo &iri,
                                                CPL_UNUSED const GDALWMSTiledImageRequestInfo &tiri)
{
    CPLString &url = request.URL;
    url = m_base_url;

    // Assume map service if exportImage is not explicitly requested
    if ((url.ifind("/export?") == std::string::npos) && (url.ifind("/exportImage?") == std::string::npos))
        url += "/export?";

    URLPrepare(url);
    url += "f=image&dpi=&layerdefs=&layerTimeOptions=&dynamicLayers=";
    url += CPLOPrintf("&bbox=%.8f,%.8f,%.8f,%.8f",
                GetBBoxCoord(iri, m_bbox_order[0]), GetBBoxCoord(iri, m_bbox_order[1]),
                GetBBoxCoord(iri, m_bbox_order[2]), GetBBoxCoord(iri, m_bbox_order[3]))
        + CPLOPrintf("&size=%d,%d", iri.m_sx, iri.m_sy)
        + CPLOPrintf("&imageSR=%s", m_irs.c_str())
        + CPLOPrintf("&bboxSR=%s", m_irs.c_str())
        + CPLOPrintf("&format=%s", m_image_format.c_str())
        + CPLOPrintf("&layers=%s", m_layers.c_str());

    if (!m_transparent.empty() )
        url +=  CPLOPrintf("&transparent=%s", m_transparent.c_str());
    else
        url += "&transparent=false";

    if (!m_time_range.empty() )
        url += CPLOPrintf("&time=%s", m_time_range.c_str());
    else
        url += "&time=";

    return CE_None;
}

void WMSMiniDriver_AGS::GetTiledImageInfo(CPLString &url,
                                              const GDALWMSImageRequestInfo &iri,
                                              CPL_UNUSED const GDALWMSTiledImageRequestInfo &tiri,
                                              int nXInBlock,
                                              int nYInBlock)
{
    url = m_base_url;

    if (m_base_url.ifind("/identify?") == std::string::npos)
        url += "/identify?";

    URLPrepare(url);
    // Constant part
    url += "f=json&geometryType=esriGeometryPoint&returnGeometry=false"
           "&layerdefs=&time=&layerTimeOptions=&maxAllowableOffset=";

    double fX = GetBBoxCoord(iri, 'x') + nXInBlock * (GetBBoxCoord(iri, 'X') -
                GetBBoxCoord(iri, 'x')) / iri.m_sx;
    double fY = GetBBoxCoord(iri, 'y') + (iri.m_sy - nYInBlock) * (GetBBoxCoord(iri, 'Y') -
                GetBBoxCoord(iri, 'y')) / iri.m_sy;

    url += CPLOPrintf("&geometry=%8f,%8f", fX, fY)
        +  CPLOPrintf("&sr=%s", m_irs.c_str());

    CPLString layers("visible");

    if (m_layers.find("show") != std::string::npos)
    {
        layers = m_layers;
        layers.replace(layers.find("show"), 4, "all");
    }

    if (m_layers.find("hide") != std::string::npos
            || m_layers.find("include") != std::string::npos
            || m_layers.find("exclude") != std::string::npos)
        layers = "top";

    url += "&layers=";
    url += layers;
    url += "&tolerance=";
    url += m_identification_tolerance;
    url += CPLOPrintf("&mapExtent=%.8f,%.8f,%.8f,%.8f",
        GetBBoxCoord(iri, m_bbox_order[0]), GetBBoxCoord(iri, m_bbox_order[1]),
        GetBBoxCoord(iri, m_bbox_order[2]), GetBBoxCoord(iri, m_bbox_order[3]))
        + CPLOPrintf("&imageDisplay=%d,%d,96", iri.m_sx, iri.m_sy);
}
