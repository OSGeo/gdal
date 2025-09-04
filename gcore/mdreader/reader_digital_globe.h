/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from DigitalGlobe imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015, NextGIS info@nextgis.ru
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef READER_DIGITAL_GLOBE_H_INCLUDED
#define READER_DIGITAL_GLOBE_H_INCLUDED

#include "../gdal_mdreader.h"

/**
@brief Metadata reader for DigitalGlobe

TIFF filename:      aaaaaaaaaa.tif
Metadata filename:  aaaaaaaaaa.IMD
RPC filename:       aaaaaaaaaa.RPB

Common metadata (from metadata filename):
    SatelliteId:         satId
    CloudCover:          cloudCover
    AcquisitionDateTime: earliestAcqTime, latestAcqTime

OR
Metadata and RPC filename:    aaaaaaaaaa.XML
Common metadata (from metadata filename):
    SatelliteId:         SATID
    CloudCover:          CLOUDCOVER
    AcquisitionDateTime: EARLIESTACQTIME, LATESTACQTIME

*/

class GDALMDReaderDigitalGlobe final : public GDALMDReaderBase
{
  public:
    GDALMDReaderDigitalGlobe(const char *pszPath,
                             CSLConstList papszSiblingFiles);
    ~GDALMDReaderDigitalGlobe() override;
    bool HasRequiredFiles() const override;
    char **GetMetadataFiles() const override;

  protected:
    void LoadMetadata() override;
    char **LoadRPBXmlNode(CPLXMLNode *psNode);
    char **LoadIMDXmlNode(CPLXMLNode *psNode);

  protected:
    CPLString m_osXMLSourceFilename{};
    CPLString m_osIMDSourceFilename{};
    CPLString m_osRPBSourceFilename{};
};

#endif  // READER_DIGITAL_GLOBE_H_INCLUDED
