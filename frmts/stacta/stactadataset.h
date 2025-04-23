/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  STACTA (Spatio-Temporal Asset Catalog Tiled Assets) driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef STACTADATASET_H
#define STACTADATASET_H

#include "cpl_mem_cache.h"
#include "cpl_string.h"
#include "gdal_pam.h"
#include "memdataset.h"
#include "tilematrixset.hpp"

#include <array>
#include <map>
#include <memory>
#include <vector>

namespace
{
struct Limits
{
    int min_tile_col = 0;
    int max_tile_col = 0;
    int min_tile_row = 0;
    int max_tile_row = 0;
};
}  // namespace
/************************************************************************/
/* ==================================================================== */
/*                          STACTADataset                               */
/* ==================================================================== */
/************************************************************************/

class STACTARawDataset;

class STACTADataset final : public GDALPamDataset
{
    friend class STACTARasterBand;
    friend class STACTARawDataset;
    friend class STACTARawRasterBand;

    std::array<double, 6> m_adfGeoTransform = {{0.0, 1.0, 0, 0.0, 0.0, 1.0}};
    OGRSpatialReference m_oSRS{};
    std::unique_ptr<GDALDataset> m_poDS{};
    // Array of overview datasets, that are guaranteed to have the same
    // georeferenced extent as m_poDS (and this dataset), for compliance
    // with the GDAL data model. They are thus possibly VRT subsets of
    // the STACTARawDataset stored in m_apoIntermediaryDS
    std::vector<std::unique_ptr<GDALDataset>> m_apoOverviewDS{};
    std::vector<std::unique_ptr<GDALDataset>> m_apoIntermediaryDS{};

    // Cache of tile datasets
    lru11::Cache<std::string, std::unique_ptr<GDALDataset>> m_oCacheTileDS{32};

    bool m_bDownloadWholeMetaTile = false;
    bool m_bSkipMissingMetaTile = false;

    bool Open(GDALOpenInfo *poOpenInfo);

  public:
    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *OpenStatic(GDALOpenInfo *poOpenInfo);

    ~STACTADataset() override;

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr GetGeoTransform(double *padfGeoTransform) override;
    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, int nBandCount,
                     BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
                     GSpacing nLineSpace, GSpacing nBandSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;
    CPLErr FlushCache(bool bAtClosing) override;
};

/************************************************************************/
/* ==================================================================== */
/*                        STACTARasterBand                              */
/* ==================================================================== */
/************************************************************************/

class STACTARasterBand final : public GDALRasterBand
{
    friend class STACTADataset;
    GDALColorInterp m_eColorInterp = GCI_Undefined;
    int m_bHasNoDataValue = false;
    double m_dfNoData = 0;
    double m_dfScale = 1.0;
    double m_dfOffset = 0.0;
    std::string m_osUnit{};

  public:
    STACTARasterBand(STACTADataset *poDSIn, int nBandIn,
                     GDALRasterBand *poProtoBand);
    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage) override;
    CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                     GDALDataType, GSpacing, GSpacing,
                     GDALRasterIOExtraArg *psExtraArg) override;

    GDALColorInterp GetColorInterpretation() override
    {
        return m_eColorInterp;
    }

    int GetOverviewCount() override;
    GDALRasterBand *GetOverview(int nIdx) override;
    double GetNoDataValue(int *pbHasNoData = nullptr) override;

    const char *GetUnitType() override
    {
        return m_osUnit.c_str();
    }

    double GetScale(int *pbHasValue = nullptr) override
    {
        if (pbHasValue)
            *pbHasValue = m_dfScale != 1.0;
        return m_dfScale;
    }

    double GetOffset(int *pbHasValue = nullptr) override
    {
        if (pbHasValue)
            *pbHasValue = m_dfOffset != 0.0;
        return m_dfOffset;
    }
};

/************************************************************************/
/* ==================================================================== */
/*                        STACTARawDataset                              */
/* ==================================================================== */
/************************************************************************/

class STACTARawDataset final : public GDALDataset
{
    friend class STACTADataset;
    friend class STACTARawRasterBand;

    CPLString m_osURLTemplate{};
    int m_nMinMetaTileCol = 0;
    int m_nMinMetaTileRow = 0;
    int m_nMetaTileWidth = 0;
    int m_nMetaTileHeight = 0;
    STACTADataset *m_poMasterDS = nullptr;

    std::array<double, 6> m_adfGeoTransform = {{0.0, 1.0, 0, 0.0, 0.0, 1.0}};
    OGRSpatialReference m_oSRS{};

  public:
    bool InitRaster(GDALDataset *poProtoDS,
                    const std::vector<GDALDataType> &aeDT,
                    const std::vector<bool> &abSetNoData,
                    const std::vector<double> &adfNoData,
                    const gdal::TileMatrixSet *poTMS, const std::string &osTMId,
                    const gdal::TileMatrixSet::TileMatrix &oTM,
                    const std::map<CPLString, Limits> &oMapLimits);

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return &m_oSRS;
    }

    CPLErr GetGeoTransform(double *padfGeoTransform) override;
    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, int nBandCount,
                     BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
                     GSpacing nLineSpace, GSpacing nBandSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;
};

/************************************************************************/
/* ==================================================================== */
/*                        STACTARawRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class STACTARawRasterBand final : public GDALRasterBand
{
    GDALColorInterp m_eColorInterp = GCI_Undefined;
    int m_bHasNoDataValue = false;
    double m_dfNoData = 0;

  public:
    STACTARawRasterBand(STACTARawDataset *poDSIn, int nBandIn,
                        GDALRasterBand *poProtoBand);

    STACTARawRasterBand(STACTARawDataset *poDSIn, int nBandIn, GDALDataType eDT,
                        bool bSetNoData, double dfNoData);

    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage) override;
    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpace,
                     GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

    GDALColorInterp GetColorInterpretation() override
    {
        return m_eColorInterp;
    }

    double GetNoDataValue(int *pbHasNoData = nullptr) override;
};

#endif  // STACTADATASET_H
