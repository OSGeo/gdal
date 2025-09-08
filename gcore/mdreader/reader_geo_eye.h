/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from GeoEye imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015, NextGIS info@nextgis.ru
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef READER_GEO_EYE_H_INCLUDED
#define READER_GEO_EYE_H_INCLUDED

#include "../gdal_mdreader.h"

/**
@brief Metadata reader for Geo Eye

TIFF filename:      aaaaaaaaaa.tif
Metadata filename:  *_metadata*
RPC filename:       aaaaaaaaaa_rpc.txt

Common metadata (from metadata filename):
    SatelliteId:         Sensor
    CloudCover:          Percent Cloud Cover
    AcquisitionDateTime: Acquisition Date/Time

*/

class GDALMDReaderGeoEye final : public GDALMDReaderBase
{
  public:
    GDALMDReaderGeoEye(const char *pszPath, CSLConstList papszSiblingFiles);
    ~GDALMDReaderGeoEye() override;
    bool HasRequiredFiles() const override;
    char **GetMetadataFiles() const override;

  protected:
    void LoadMetadata() override;
    virtual GIntBig
    GetAcquisitionTimeFromString(const char *pszDateTime) override;
    char **LoadRPCWktFile() const;
    char **LoadIMDWktFile() const;

  protected:
    CPLString m_osIMDSourceFilename{};
    CPLString m_osRPBSourceFilename{};
};

#endif  // READER_GEO_EYE_H_INCLUDED
