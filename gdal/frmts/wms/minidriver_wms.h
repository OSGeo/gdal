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

/*
 * Base class for a WMS minidriver.
 * At least Initialize() and one of the ImageRequest() or TiledImageRequest() has to be provided
 * All minidrivers are instantiated in wmsdriver.cpp, in GDALRegister_WMS()
 */

class WMSMiniDriver_WMS : public WMSMiniDriver {
public:
    WMSMiniDriver_WMS();
    virtual ~WMSMiniDriver_WMS();

public:
    virtual CPLErr Initialize(CPLXMLNode *config, char **papszOpenOptions) override;
    virtual void GetCapabilities(WMSMiniDriverCapabilities *caps) override;

    // Return error message in request.Error
    virtual CPLErr TiledImageRequest(WMSHTTPRequest &request,
                                        const GDALWMSImageRequestInfo &iri,
                                        const GDALWMSTiledImageRequestInfo &tiri) override;

    virtual void GetTiledImageInfo(CPLString &url,
                                        const GDALWMSImageRequestInfo &iri,
                                        const GDALWMSTiledImageRequestInfo &tiri,
                                        int nXInBlock,
                                        int nYInBlock) override;

protected:
    void   BuildURL(CPLString &url, const GDALWMSImageRequestInfo &iri, const char* pszRequest);

protected:
    CPLString m_version;
    int m_iversion;
    CPLString m_layers;
    CPLString m_styles;
    CPLString m_srs;
    CPLString m_crs;
    CPLString m_image_format;
    CPLString m_info_format;
    CPLString m_bbox_order;
    CPLString m_transparent;
};
