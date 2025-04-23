/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Chris Schmidt
 *
 ******************************************************************************
 * Copyright (c) 2007, Chris Schmidt
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MINIDRIVER_TMS_H_INCLUDED
#define MINIDRIVER_TMS_H_INCLUDED

class WMSMiniDriver_TMS : public WMSMiniDriver
{
    int m_nTileXMultiplier = 1;

  public:
    WMSMiniDriver_TMS();
    virtual ~WMSMiniDriver_TMS();

  public:
    virtual CPLErr Initialize(CPLXMLNode *config,
                              char **papszOpenOptions) override;
    virtual CPLErr
    TiledImageRequest(WMSHTTPRequest &request,
                      const GDALWMSImageRequestInfo &iri,
                      const GDALWMSTiledImageRequestInfo &tiri) override;
};

#endif /* MINIDRIVER_TMS_H_INCLUDED */
