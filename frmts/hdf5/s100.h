/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read S100 bathymetric datasets.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef S100_H
#define S100_H

#include "cpl_port.h"

#include "gdal_pam.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"

/************************************************************************/
/*                            S100BaseDataset                           */
/************************************************************************/

class S100BaseDataset CPL_NON_FINAL : public GDALPamDataset
{
  private:
    void ReadSRS();

  protected:
    std::string m_osFilename{};
    std::shared_ptr<GDALGroup> m_poRootGroup{};
    OGRSpatialReference m_oSRS{};
    bool m_bHasGT = false;
    double m_adfGeoTransform[6] = {0, 1, 0, 0, 0, 1};
    std::string m_osMetadataFile{};

    explicit S100BaseDataset(const std::string &osFilename);

    bool Init();

  public:
    CPLErr GetGeoTransform(double *) override;
    const OGRSpatialReference *GetSpatialRef() const override;

    char **GetFileList() override;
};

bool S100GetNumPointsLongitudinalLatitudinal(const GDALGroup *poGroup,
                                             int &nNumPointsLongitudinal,
                                             int &nNumPointsLatitudinal);

bool S100ReadSRS(const GDALGroup *poRootGroup, OGRSpatialReference &oSRS);

bool S100GetDimensions(
    const GDALGroup *poGroup,
    std::vector<std::shared_ptr<GDALDimension>> &apoDims,
    std::vector<std::shared_ptr<GDALMDArray>> &apoIndexingVars);

bool S100GetGeoTransform(const GDALGroup *poGroup, double adfGeoTransform[6],
                         bool bNorthUp);

void S100ReadVerticalDatum(GDALDataset *poDS, const GDALGroup *poRootGroup);

std::string S100ReadMetadata(GDALDataset *poDS, const std::string &osFilename,
                             const GDALGroup *poRootGroup);

#endif  // S100_H
