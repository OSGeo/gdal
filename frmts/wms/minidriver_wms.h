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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MINIDRIVER_WMS_H_INCLUDED
#define MINIDRIVER_WMS_H_INCLUDED

/*
 * Base class for a WMS minidriver.
 * At least Initialize() and one of the ImageRequest() or TiledImageRequest()
 * has to be provided All minidrivers are instantiated in wmsdriver.cpp, in
 * GDALRegister_WMS()
 */

class WMSMiniDriver_WMS : public WMSMiniDriver
{
  public:
    WMSMiniDriver_WMS();
    virtual ~WMSMiniDriver_WMS();

  public:
    virtual CPLErr Initialize(CPLXMLNode *config,
                              char **papszOpenOptions) override;
    virtual void GetCapabilities(WMSMiniDriverCapabilities *caps) override;

    // Return error message in request.Error
    virtual CPLErr
    TiledImageRequest(WMSHTTPRequest &request,
                      const GDALWMSImageRequestInfo &iri,
                      const GDALWMSTiledImageRequestInfo &tiri) override;

    virtual void GetTiledImageInfo(CPLString &url,
                                   const GDALWMSImageRequestInfo &iri,
                                   const GDALWMSTiledImageRequestInfo &tiri,
                                   int nXInBlock, int nYInBlock) override;

  protected:
    void BuildURL(CPLString &url, const GDALWMSImageRequestInfo &iri,
                  const char *pszRequest);

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

#endif /* MINIDRIVER_WMS_H_INCLUDED */
