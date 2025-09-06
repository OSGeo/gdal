/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Mini driver for International Image Interoperability Framework
 *           Image API (IIIFImage)
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MINIDRIVER_IIIFImage_H_INCLUDED
#define MINIDRIVER_IIIFImage_H_INCLUDED

class WMSMiniDriver_IIIFImage final : public WMSMiniDriver
{
  public:
    WMSMiniDriver_IIIFImage();
    ~WMSMiniDriver_IIIFImage() override;

  public:
    virtual CPLErr Initialize(CPLXMLNode *config,
                              char **papszOpenOptions) override;
    void GetCapabilities(WMSMiniDriverCapabilities *caps) override;
    virtual CPLErr
    TiledImageRequest(WMSHTTPRequest &request,
                      const GDALWMSImageRequestInfo &iri,
                      const GDALWMSTiledImageRequestInfo &tiri) override;

  private:
    std::string m_imageExtension = "jpg";
};

#endif /* MINIDRIVER_IIIFImage_H_INCLUDED */
