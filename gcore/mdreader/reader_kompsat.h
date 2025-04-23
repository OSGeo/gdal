/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Kompsat imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015 NextGIS <info@nextgis.ru>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef READER_KOMPSAT_H_INCLUDED
#define READER_KOMPSAT_H_INCLUDED

#include "reader_pleiades.h"

/**
@brief Metadata reader for Kompsat

TIFF filename:      aaaaaaaaaa.tif
Metadata filename:  aaaaaaaaaa.eph aaaaaaaaaa.txt
RPC filename:       aaaaaaaaaa.rpc

Common metadata (from metadata filename):
    SatelliteId:            AUX_SATELLITE_NAME
    AcquisitionDateTime:    IMG_ACQISITION_START_TIME, IMG_ACQISITION_END_TIME
*/

class GDALMDReaderKompsat : public GDALMDReaderBase
{
  public:
    GDALMDReaderKompsat(const char *pszPath, char **papszSiblingFiles);
    virtual ~GDALMDReaderKompsat();
    virtual bool HasRequiredFiles() const override;
    virtual char **GetMetadataFiles() const override;

  protected:
    virtual void LoadMetadata() override;
    char **ReadTxtToList();
    virtual GIntBig
    GetAcquisitionTimeFromString(const char *pszDateTime) override;

  protected:
    CPLString m_osIMDSourceFilename{};
    CPLString m_osRPBSourceFilename{};
};

#endif  // READER_KOMPSAT_H_INCLUDED
