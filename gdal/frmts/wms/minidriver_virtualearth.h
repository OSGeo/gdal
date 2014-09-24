/******************************************************************************
 * $Id$
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

H_GDALWMSMiniDriverFactory(VirtualEarth)

class GDALWMSMiniDriver_VirtualEarth : public GDALWMSMiniDriver {
public:
    GDALWMSMiniDriver_VirtualEarth();
    virtual ~GDALWMSMiniDriver_VirtualEarth();

public:
    virtual CPLErr Initialize(CPLXMLNode *config);
    virtual void GetCapabilities(GDALWMSMiniDriverCapabilities *caps);
    virtual void TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri);
    virtual const char* GetProjectionInWKT();

protected:
    CPLString m_base_url;
    CPLString m_projection_wkt;
};
