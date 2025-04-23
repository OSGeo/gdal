/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Landsat imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015 NextGIS <info@nextgis.ru>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef READER_LANDSAT_H_INCLUDED
#define READER_LANDSAT_H_INCLUDED

#include "../gdal_mdreader.h"

/**
@brief Metadata reader for Landsat

TIFF filename:      xxxxxx_aaa.tif
Metadata filename:  xxxxxx_MTL.txt
RPC filename:

Common metadata (from metadata filename):
    SatelliteId:         SPACECRAFT_ID
    CloudCover:          CLOUD_COVER (Landsat 8)
    AcquisitionDateTime: ACQUISITION_DATE,
                         SCENE_CENTER_SCAN_TIME (Landsat 5,7) or
                         DATE_ACQUIRED, SCENE_CENTER_TIME (Landsat 8);

*/

class GDALMDReaderLandsat : public GDALMDReaderBase
{
  public:
    GDALMDReaderLandsat(const char *pszPath, char **papszSiblingFiles);
    virtual ~GDALMDReaderLandsat();
    virtual bool HasRequiredFiles() const override;
    virtual char **GetMetadataFiles() const override;

  protected:
    virtual void LoadMetadata() override;

  protected:
    CPLString m_osIMDSourceFilename{};
};

#endif  // READER_LANDSAT_H_INCLUDED
