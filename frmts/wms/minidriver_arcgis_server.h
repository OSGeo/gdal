/******************************************************************************
 *
 * Project:  Arc GIS Server Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Alexander Lisovenko
 *
 ******************************************************************************
 * Copyright (c) 2014-2015, NextGIS <info@nextgis.ru>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MINIDRIVER_AGS_H_INCLUDED
#define MINIDRIVER_AGS_H_INCLUDED

class WMSMiniDriver_AGS : public WMSMiniDriver
{
  public:
    WMSMiniDriver_AGS();
    virtual ~WMSMiniDriver_AGS();

  public:
    virtual CPLErr Initialize(CPLXMLNode *config,
                              char **papszOpenOptions) override;
    virtual void GetCapabilities(WMSMiniDriverCapabilities *caps) override;
    virtual CPLErr
    TiledImageRequest(WMSHTTPRequest &request,
                      const GDALWMSImageRequestInfo &iri,
                      const GDALWMSTiledImageRequestInfo &tiri) override;
    virtual void GetTiledImageInfo(CPLString &url,
                                   const GDALWMSImageRequestInfo &iri,
                                   const GDALWMSTiledImageRequestInfo &tiri,
                                   int nXInBlock, int nYInBlock) override;

    virtual char **GetMetadataDomainList() override;

  protected:
    /*
     * png | png8 | png24 | jpg | pdf | bmp | gif | svg | png32
     * https://developers.arcgis.com/rest/services-reference/enterprise/export-map/
     * Parameter - format
     */
    CPLString m_image_format;
    CPLString m_transparent;
    CPLString m_bbox_order;
    CPLString m_irs;

    CPLString m_layers;
    CPLString m_srs;
    CPLString m_crs;
    CPLString m_time_range;

    CPLString m_identification_tolerance;
};

#endif /* MINIDRIVER_AGS_H_INCLUDED */
