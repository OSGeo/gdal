/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Resurs-DK1 imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015 NextGIS <info@nextgis.ru>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef READER_RDK1_H_INCLUDED
#define READER_RDK1_H_INCLUDED

#include "../gdal_mdreader.h"

/**
@brief Metadata reader for RDK1

TIFF filename:      aaaaaaaaaa.tif
Metadata filename:  aaaaaaaaaa.xml
RPC filename:

Common metadata (from metadata filename):
    SatelliteId:         cCodeKA
    AcquisitionDateTime: dSceneDate, tSceneTime
*/

class GDALMDReaderResursDK1 final : public GDALMDReaderBase
{
  public:
    GDALMDReaderResursDK1(const char *pszPath, CSLConstList papszSiblingFiles);
    ~GDALMDReaderResursDK1() override;
    bool HasRequiredFiles() const override;
    char **GetMetadataFiles() const override;

  protected:
    void LoadMetadata() override;
    virtual GIntBig
    GetAcquisitionTimeFromString(const char *pszDateTime) override;
    virtual char **AddXMLNameValueToList(char **papszList, const char *pszName,
                                         const char *pszValue) override;

  protected:
    CPLString m_osXMLSourceFilename{};
};

#endif  // READER_RDK1_H_INCLUDED
