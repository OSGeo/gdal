/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MINIDRIVER_VE_H_INCLUDED
#define MINIDRIVER_VE_H_INCLUDED

class WMSMiniDriver_VirtualEarth : public WMSMiniDriver
{
  public:
    WMSMiniDriver_VirtualEarth();
    virtual ~WMSMiniDriver_VirtualEarth();

  public:
    virtual CPLErr Initialize(CPLXMLNode *config,
                              char **papszOpenOptions) override;
    virtual CPLErr
    TiledImageRequest(WMSHTTPRequest &request,
                      const GDALWMSImageRequestInfo &iri,
                      const GDALWMSTiledImageRequestInfo &tiri) override;
};

#endif /* MINIDRIVER_VE_H_INCLUDED */
