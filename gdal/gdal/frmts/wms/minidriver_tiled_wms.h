/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Declarations for the OnEarth Tiled WMS minidriver.
 *           http://onearth.jpl.nasa.gov/tiled.html
 * Author:   Lucian Plesea (Lucian dot Plesea at jpl.nasa.gov)
 *           Adam Nowacki
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

H_GDALWMSMiniDriverFactory(TiledWMS)

class GDALWMSMiniDriver_TiledWMS : public GDALWMSMiniDriver {
public:
    GDALWMSMiniDriver_TiledWMS();
    virtual ~GDALWMSMiniDriver_TiledWMS();

public:
    virtual CPLErr Initialize(CPLXMLNode *config);
    virtual void GetCapabilities(GDALWMSMiniDriverCapabilities *caps);
    virtual void ImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri);
    virtual void TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri);
    virtual const char *GetProjectionInWKT();

protected:
    double Scale(const char *request);
    CPLString GetLowestScale(char **&list,int i);
    GDALWMSDataWindow m_data_window;
    char **m_requests;
    CPLString m_base_url;
    CPLString m_end_url;
    int m_bsx;
    int m_bsy;
    CPLString m_projection_wkt;
};
