/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Spot imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015 NextGIS <info@nextgis.ru>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef READER_SPOT_H_INCLUDED
#define READER_SPOT_H_INCLUDED

#include "reader_pleiades.h"

/**
@brief Metadata reader for Spot

TIFF filename:      aaaaaaaaaa.tif
Metadata filename:  METADATA.DIM
RPC filename:

Common metadata (from metadata filename):
    SatelliteId:         MISSION, MISSION_INDEX
    AcquisitionDateTime: IMAGING_DATE, IMAGING_TIME
*/

class GDALMDReaderSpot : public GDALMDReaderPleiades
{
  public:
    GDALMDReaderSpot(const char *pszPath, char **papszSiblingFiles);
    virtual ~GDALMDReaderSpot();

  protected:
    virtual void LoadMetadata() override;
    virtual char **ReadXMLToList(CPLXMLNode *psNode, char **papszList,
                                 const char *pszName = "") override;
};

#endif  // READER_SPOT_H_INCLUDED
