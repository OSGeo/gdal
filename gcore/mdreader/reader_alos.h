/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Alos imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015 NextGIS <info@nextgis.ru>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef READER_ALOS_H_INCLUDED
#define READER_ALOS_H_INCLUDED

#include "reader_pleiades.h"

/**
Metadata reader for ALOS

TIFF filename:      IMG-sssssssssssssss-pppppppp.tif or
                    IMG-01-sssssssssssssss-pppppppp.tif
                    IMG-02-sssssssssssssss-pppppppp.tif
Metadata filename:  summary.txt
RPC filename:       RPC-sssssssssssssss-pppppppp.txt

Common metadata (from metadata filename):
    AcquisitionDateTime: Img_SceneCenterDateTime or Lbi_ObservationDate
    SatelliteId:         Lbi_Satellite
    CloudCover:          Img_CloudQuantityOfAllImage
*/

class GDALMDReaderALOS : public GDALMDReaderBase
{
  public:
    GDALMDReaderALOS(const char *pszPath, char **papszSiblingFiles);
    virtual ~GDALMDReaderALOS();
    virtual bool HasRequiredFiles() const override;
    virtual char **GetMetadataFiles() const override;

  protected:
    virtual void LoadMetadata() override;
    char **LoadRPCTxtFile();
    virtual GIntBig
    GetAcquisitionTimeFromString(const char *pszDateTime) override;

  protected:
    CPLString m_osIMDSourceFilename{};
    CPLString m_osHDRSourceFilename{};
    CPLString m_osRPBSourceFilename{};
};

#endif  // READER_ALOS_H_INCLUDED
