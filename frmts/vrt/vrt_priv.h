/******************************************************************************
 * Project:  Virtual GDAL Datasets
 * Purpose:  Declaration of VRT private stuff
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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

CPLStringList VRTParseCategoryNames(const CPLXMLNode *psCategoryNames);

std::unique_ptr<GDALColorTable>
VRTParseColorTable(const CPLXMLNode *psColorTable);

#endif

#endif  // VRT_PRIV_H_INCLUDED
