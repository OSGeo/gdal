/******************************************************************************
 * $Id$
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

H_GDALWMSMiniDriverFactory(AGS)

class GDALWMSMiniDriver_AGS : public GDALWMSMiniDriver
{
public:
    GDALWMSMiniDriver_AGS();
    virtual ~GDALWMSMiniDriver_AGS();

public:
    virtual CPLErr Initialize(CPLXMLNode *config);
    virtual void GetCapabilities(GDALWMSMiniDriverCapabilities *caps);
    virtual void ImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri);
    virtual void TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri,
                                   const GDALWMSTiledImageRequestInfo &tiri);
    virtual void GetTiledImageInfo(CPLString *url,
                                   const GDALWMSImageRequestInfo &iri,
                                   const GDALWMSTiledImageRequestInfo &tiri,
                                   int nXInBlock,
                                   int nYInBlock);
    virtual const char *GetProjectionInWKT();

protected:
    double GetBBoxCoord(const GDALWMSImageRequestInfo &iri, char what);

protected:
    CPLString m_base_url;
	/*
	 * png | png8 | png24 | jpg | pdf | bmp | gif | svg | png32
	 * http://resources.arcgis.com/en/help/rest/apiref/
	 * Parameter - format
	 */
	CPLString m_image_format;
	CPLString m_transparent;
	CPLString m_bbox_order;
	CPLString m_irs;

    CPLString m_layers;
    CPLString m_srs;
    CPLString m_crs;
    CPLString m_projection_wkt;

	CPLString m_identification_tolerance;
};
