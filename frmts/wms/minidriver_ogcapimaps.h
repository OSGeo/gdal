/******************************************************************************
 * Project:  WMS Client Driver
 * Purpose:  OGC API maps
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MINIDRIVER_OGCAPIMAPS_H_INCLUDED
#define MINIDRIVER_OGCAPIMAPS_H_INCLUDED

class WMSMiniDriver_OGCAPIMaps : public WMSMiniDriver
{
  public:
    WMSMiniDriver_OGCAPIMaps() = default;

  public:
    virtual CPLErr Initialize(CPLXMLNode *config,
                              char **papszOpenOptions) override;
    virtual CPLErr
    TiledImageRequest(WMSHTTPRequest &request,
                      const GDALWMSImageRequestInfo &iri,
                      const GDALWMSTiledImageRequestInfo &tiri) override;
};

#endif /* MINIDRIVER_OGCAPIMAPS_H_INCLUDED */
