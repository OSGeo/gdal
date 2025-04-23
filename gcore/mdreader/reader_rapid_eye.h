/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from RapidEye imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015 NextGIS <info@nextgis.ru>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef READER_RAPID_EYE_H_INCLUDED
#define READER_RAPID_EYE_H_INCLUDED

#include "../gdal_mdreader.h"

/**
@brief Metadata reader for RapidEye

TIFF filename:      aaaaaaaa.tif
Metadata filename:  aaaaaaaa_metadata.xml
RPC filename:

Common metadata (from metadata filename):
    SatelliteId:         eop:serialIdentifier
    CloudCover:          opt:cloudCoverPercentage
    AcquisitionDateTime: re:acquisitionDateTime
*/

class GDALMDReaderRapidEye : public GDALMDReaderBase
{
  public:
    GDALMDReaderRapidEye(const char *pszPath, char **papszSiblingFiles);
    virtual ~GDALMDReaderRapidEye();
    virtual bool HasRequiredFiles() const override;
    virtual char **GetMetadataFiles() const override;

  protected:
    virtual void LoadMetadata() override;

  protected:
    CPLString m_osXMLSourceFilename{};
};

#endif  // READER_RAPID_EYE_H_INCLUDED
