/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Pleiades imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015 NextGIS <info@nextgis.ru>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef READER_PLEIADES_H_INCLUDED
#define READER_PLEIADES_H_INCLUDED

#include "../gdal_mdreader.h"

/**
@brief Metadata reader for Pleiades

TIFF filename:      IMG_xxxxxx.tif
Metadata filename:  DIM_xxxxxx.XML
RPC filename:       RPC_xxxxxx.XML

Common metadata (from metadata filename):
    SatelliteId:         MISSION, MISSION_INDEX
    AcquisitionDateTime: IMAGING_DATE, IMAGING_TIME

*/

class CPL_DLL GDALMDReaderPleiades : public GDALMDReaderBase
{
  public:
    GDALMDReaderPleiades(const char *pszPath, char **papszSiblingFiles);
    virtual ~GDALMDReaderPleiades();
    virtual bool HasRequiredFiles() const override;
    virtual char **GetMetadataFiles() const override;

    static GDALMDReaderPleiades *
    CreateReaderForRPC(const char *pszRPCSourceFilename);

    char **LoadRPCXmlFile(const CPLXMLNode *psDIMRootNode = nullptr);

  protected:
    virtual void LoadMetadata() override;

  protected:
    CPLString m_osBaseFilename{};
    CPLString m_osIMDSourceFilename{};
    CPLString m_osRPBSourceFilename{};

  private:
    GDALMDReaderPleiades();
};

#endif  // READER_PLEIADES_H_INCLUDED
