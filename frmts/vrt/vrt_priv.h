/******************************************************************************
 * Project:  Virtual GDAL Datasets
 * Purpose:  Declaration of VRT private stuff
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef VRT_PRIV_H_INCLUDED
#define VRT_PRIV_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "cpl_port.h"
#include "gdal_priv.h"

#include <string>
#include <vector>

struct CPL_DLL GTISourceDesc
{
    std::string osFilename{};
    int nDstXOff = 0;
    int nDstYOff = 0;
    int nDstXSize = 0;
    int nDstYSize = 0;
};

class GDALTileIndexDataset;

GDALTileIndexDataset CPL_DLL *GDALDatasetCastToGTIDataset(GDALDataset *poDS);

std::vector<GTISourceDesc> CPL_DLL
GTIGetSourcesMoreRecentThan(GDALTileIndexDataset *poDS, int64_t mTime);

CPLStringList CPL_DLL VRTParseCategoryNames(const CPLXMLNode *psCategoryNames);

std::unique_ptr<GDALColorTable>
    CPL_DLL VRTParseColorTable(const CPLXMLNode *psColorTable);

#endif

#endif  // VRT_PRIV_H_INCLUDED
