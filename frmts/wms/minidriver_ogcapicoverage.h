/******************************************************************************
 * Project:  WMS Client Driver
 * Purpose:  OGC API coverage
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MINIDRIVER_OGCAPICOVERAGE_H_INCLUDED
#define MINIDRIVER_OGCAPICOVERAGE_H_INCLUDED

class WMSMiniDriver_OGCAPICoverage : public WMSMiniDriver
{
  public:
    WMSMiniDriver_OGCAPICoverage() = default;

  public:
    virtual CPLErr Initialize(CPLXMLNode *config,
                              char **papszOpenOptions) override;
    virtual CPLErr
    TiledImageRequest(WMSHTTPRequest &request,
                      const GDALWMSImageRequestInfo &iri,
                      const GDALWMSTiledImageRequestInfo &tiri) override;
};

#endif /* MINIDRIVER_OGCAPICOVERAGE_H_INCLUDED */
