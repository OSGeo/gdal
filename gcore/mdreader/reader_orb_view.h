/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from OrbView imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015 NextGIS <info@nextgis.ru>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef READER_ORB_VIEW_H_INCLUDED
#define READER_ORB_VIEW_H_INCLUDED

#include "../gdal_mdreader.h"

/**
@brief Metadata reader for OrbView

TIFF filename:      aaaaaaaaa.tif
Metadata filename:  aaaaaaaaa.pvl
RPC filename:       aaaaaaaaa_rpc.txt

Common metadata (from metadata filename):
    SatelliteId:         sensorInfo.satelliteName
    CloudCover:          productInfo.productCloudCoverPercentage
    AcquisitionDateTime: inputImageInfo.firstLineAcquisitionDateTime
*/
class GDALMDReaderOrbView : public GDALMDReaderBase
{
  public:
    GDALMDReaderOrbView(const char *pszPath, char **papszSiblingFiles);
    virtual ~GDALMDReaderOrbView();
    virtual bool HasRequiredFiles() const override;
    virtual char **GetMetadataFiles() const override;

  protected:
    virtual void LoadMetadata() override;

  protected:
    CPLString m_osIMDSourceFilename{};
    CPLString m_osRPBSourceFilename{};
};

#endif  // READER_ORB_VIEW_H_INCLUDED
