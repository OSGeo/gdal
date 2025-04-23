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

#ifndef MINIDRIVER_WORLDWIND_H_INCLUDED
#define MINIDRIVER_WORLDWIND_H_INCLUDED

class WMSMiniDriver_WorldWind : public WMSMiniDriver
{
  public:
    WMSMiniDriver_WorldWind();
    virtual ~WMSMiniDriver_WorldWind();

  public:
    virtual CPLErr Initialize(CPLXMLNode *config,
                              char **papszOpenOptions) override;
    virtual CPLErr
    TiledImageRequest(WMSHTTPRequest &request,
                      const GDALWMSImageRequestInfo &iri,
                      const GDALWMSTiledImageRequestInfo &tiri) override;
};

#endif /* MINIDRIVER_WORLDWIND_H_INCLUDED */
