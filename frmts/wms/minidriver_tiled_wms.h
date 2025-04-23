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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MINIDRIVER_TILED_WMS_H_INCLUDED
#define MINIDRIVER_TILED_WMS_H_INCLUDED

class WMSMiniDriver_TiledWMS : public WMSMiniDriver
{

  public:
    WMSMiniDriver_TiledWMS();
    virtual ~WMSMiniDriver_TiledWMS();

    virtual CPLErr Initialize(CPLXMLNode *config,
                              char **papszOpenOptions) override;
    virtual CPLErr
    TiledImageRequest(WMSHTTPRequest &request,
                      const GDALWMSImageRequestInfo &iri,
                      const GDALWMSTiledImageRequestInfo &tiri) override;

  protected:
    double Scale(const char *request) const;
    CPLString GetLowestScale(CPLStringList &list, int i) const;
    GDALWMSDataWindow m_data_window;
    CPLStringList m_requests{};
    int m_bsx = 0;
    int m_bsy = 0;
};

#endif /* MINIDRIVER_TILED_WMS_H_INCLUDED */
