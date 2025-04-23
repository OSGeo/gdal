/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Mini driver for Internet Imaging Protocol (IIP)
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MINIDRIVER_IIP_H_INCLUDED
#define MINIDRIVER_IIP_H_INCLUDED

class WMSMiniDriver_IIP : public WMSMiniDriver
{
  public:
    WMSMiniDriver_IIP();
    virtual ~WMSMiniDriver_IIP();

  public:
    virtual CPLErr Initialize(CPLXMLNode *config,
                              char **papszOpenOptions) override;
    virtual void GetCapabilities(WMSMiniDriverCapabilities *caps) override;
    virtual CPLErr
    TiledImageRequest(WMSHTTPRequest &request,
                      const GDALWMSImageRequestInfo &iri,
                      const GDALWMSTiledImageRequestInfo &tiri) override;
};

#endif /* MINIDRIVER_IIP_H_INCLUDED */
