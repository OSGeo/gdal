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

/*
    * -oo  RAT_OR_CT
    Controls whether the Raster Attribute Table (RAT) and/or the Color Table (CT) are exposed.

      ALL
            Expose both the attribute table and the color table. Note that in some software this option may cause visualization and/or legend issues.
      RAT
            Expose the attribute table only, without the color table.
      PER_BAND_ONLY
            Expose the color table only, without the attribute table.
    */
enum class RAT_OR_CT
{
    ALL,
    RAT,
    CT
};

class MMRDataset final : public GDALPamDataset
{
  public:
    explicit MMRDataset(GDALOpenInfo *poOpenInfo);
    MMRDataset(const MMRDataset &) =
        delete;  // I don't want to construct a MMRDataset from another MMRDataset (effc++)
    MMRDataset &operator=(const MMRDataset &) =
        delete;  // I don't want to assign a MMRDataset to another MMRDataset (effc++)
    ~MMRDataset() override;

    static int Identify(GDALOpenInfo *);
    static GDALDataset *Open(GDALOpenInfo *);

    MMRRel *GetRel()
    {
        return m_pMMRRel.get();
    }

    RAT_OR_CT GetRatOrCT() const
    {
        return nRatOrCT;
    }

  private:
    void ReadProjection();
    void AssignBandsToSubdataSets();
    void CreateSubdatasetsFromBands();
    bool CreateRasterBands();
    bool BandInTheSameDataset(int nIBand1, int nIBan2) const;

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

    // To expose CT, RAT or both
    RAT_OR_CT nRatOrCT = RAT_OR_CT::ALL;
};

#endif  // MMRDATASET_H_INCLUDED
