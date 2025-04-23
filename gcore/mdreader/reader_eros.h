/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from EROS imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015, NextGIS info@nextgis.ru
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef READER_EROS_H_INCLUDED
#define READER_EROS_H_INCLUDED

#include "../gdal_mdreader.h"

/**
@brief Metadata reader for EROS

TIFF filename:      aaaaaaa.bb.ccc.tif
Metadata filename:  aaaaaaa.pass

Common metadata (from metadata filename):
    SatelliteId:         satellite
    AcquisitionDateTime: sweep_start_utc, sweep_end_utc
*/

class GDALMDReaderEROS : public GDALMDReaderBase
{
  public:
    GDALMDReaderEROS(const char *pszPath, char **papszSiblingFiles);
    virtual ~GDALMDReaderEROS();
    virtual bool HasRequiredFiles() const override;
    virtual char **GetMetadataFiles() const override;

  protected:
    virtual void LoadMetadata() override;
    char **LoadImdTxtFile();
    virtual GIntBig
    GetAcquisitionTimeFromString(const char *pszDateTime) override;

  protected:
    CPLString m_osIMDSourceFilename{};
    CPLString m_osRPBSourceFilename{};
};

#endif  // READER_EROS_H_INCLUDED
