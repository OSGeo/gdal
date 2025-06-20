/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read S100 bathymetric datasets.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
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
    GDALGeoTransform m_gt{};
    std::string m_osMetadataFile{};

    explicit S100BaseDataset(const std::string &osFilename);

    bool Init();

  public:
    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
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

bool S100GetGeoTransform(const GDALGroup *poGroup, GDALGeoTransform &gt,
                         bool bNorthUp);

void S100ReadVerticalDatum(GDALDataset *poDS, const GDALGroup *poRootGroup);

std::string S100ReadMetadata(GDALDataset *poDS, const std::string &osFilename,
                             const GDALGroup *poRootGroup);

#endif  // S100_H
