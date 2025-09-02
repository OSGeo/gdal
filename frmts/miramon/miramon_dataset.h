/******************************************************************************
 *
 * Project:  MiraMon Raster Driver
 * Purpose:  Implements MMRDataset class: responsible for generating the
 *           main dataset or the subdatasets as needed.
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MMRDATASET_H_INCLUDED
#define MMRDATASET_H_INCLUDED

#include <cstddef>
#include <vector>
#include <optional>
#include <array>

#include "gdal_pam.h"
#include "gdal_rat.h"

#include "../miramon_common/mm_gdal_constants.h"  // For MM_EXT_DBF_N_FIELDS
#include "miramon_rel.h"

/* ==================================================================== */
/*                              MMRDataset                              */
/* ==================================================================== */

class MMRRasterBand;
class MMRRel;

class MMRDataset final : public GDALPamDataset
{
  public:
    explicit MMRDataset(GDALOpenInfo *poOpenInfo);
    MMRDataset(const MMRDataset &) =
        delete;  // I don't want to construct a MMRDataset from another MMRDataset (effc++)
    MMRDataset &operator=(const MMRDataset &) =
        delete;  // I don't want to assign a MMRDataset to another MMRDataset (effc++)
    ~MMRDataset();

    static int Identify(GDALOpenInfo *);
    static GDALDataset *Open(GDALOpenInfo *);

    MMRRel *GetRel()
    {
        return m_pMMRRel.get();
    }

  private:
    void ReadProjection();
    void AssignBandsToSubdataSets();
    void CreateSubdatasetsFromBands();
    bool CreateRasterBands();
    bool IsNextBandInANewDataSet(int nIBand) const;

    int UpdateGeoTransform();
    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;

    bool IsValid() const
    {
        return m_bIsValid;
    }

    GDALGeoTransform m_gt{};
    OGRSpatialReference m_oSRS{};

    bool m_bIsValid =
        false;  // Determines if the created object is valid or not.
    std::unique_ptr<MMRRel> m_pMMRRel = nullptr;

    std::vector<gdal::GCP> m_aoGCPs{};

    // Numbers of subdatasets (if any) in this dataset.
    int m_nNSubdataSets = 0;
};

#endif  // MMRDATASET_H_INCLUDED
