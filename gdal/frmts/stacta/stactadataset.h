/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  STACTA (Spatio-Temporal Asset Catalog Tiled Assets) driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
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

namespace {
struct Limits
{
    int min_tile_col = 0;
    int max_tile_col = 0;
    int min_tile_row = 0;
    int max_tile_row = 0;
};
}
/************************************************************************/
/* ==================================================================== */
/*                          STACTADataset                               */
/* ==================================================================== */
/************************************************************************/

class STACTARawDataset;

class STACTADataset final: public GDALPamDataset
{
        friend class STACTARasterBand;
        friend class STACTARawDataset;
        friend class STACTARawRasterBand;

        std::array<double, 6> m_adfGeoTransform = {{0.0,1.0,0,0.0,0.0,1.0}};
        OGRSpatialReference m_oSRS{};
        std::unique_ptr<GDALDataset> m_poDS{};
        // Array of overview datasets, that are guaranteed to have the same
        // georeferenced extent as m_poDS (and this dataset), for compliance
        // with the GDAL data model. They are thus possibly VRT subsets of
        // the STACTARawDataset stored in m_apoIntermediaryDS
        std::vector<std::unique_ptr<GDALDataset>> m_apoOverviewDS{};
        std::vector<std::unique_ptr<GDALDataset>> m_apoIntermediaryDS{};

        // Cache of tile datasets
        lru11::Cache<std::string, std::shared_ptr<GDALDataset>> m_oCacheTileDS{32};

        bool m_bDownloadWholeMetaTile = false;
        bool m_bSkipMissingMetaTile = false;

        bool Open(GDALOpenInfo* poOpenInfo);

    public:
        static int Identify(GDALOpenInfo* poOpenInfo);
        static GDALDataset* OpenStatic(GDALOpenInfo* poOpenInfo);

        ~STACTADataset() override;

        const OGRSpatialReference* GetSpatialRef() const override;
        CPLErr GetGeoTransform(double* padfGeoTransform) override;
        CPLErr IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg) override;
        void FlushCache(bool bAtClosing) override;
};

/************************************************************************/
/* ==================================================================== */
/*                        STACTARasterBand                              */
/* ==================================================================== */
/************************************************************************/

class STACTARasterBand final: public GDALRasterBand
{
        GDALColorInterp m_eColorInterp = GCI_Undefined;
        int             m_bHasNoDataValue = false;
        double          m_dfNoData = 0;

    public:
        STACTARasterBand(STACTADataset* poDSIn, int nBandIn,
                         GDALRasterBand* poProtoBand);
        CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void* pImage) override;
        CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                      void *, int, int, GDALDataType,
                      GSpacing, GSpacing,
                      GDALRasterIOExtraArg* psExtraArg ) override;
        GDALColorInterp GetColorInterpretation() override { return m_eColorInterp; }
        int GetOverviewCount() override;
        GDALRasterBand* GetOverview(int nIdx) override;
        double GetNoDataValue(int* pbHasNoData = nullptr) override;
};

/************************************************************************/
/* ==================================================================== */
/*                        STACTARawDataset                              */
/* ==================================================================== */
/************************************************************************/

class STACTARawDataset final: public GDALDataset
{
        friend class STACTADataset;
        friend class STACTARawRasterBand;

        CPLString m_osURLTemplate{};
        int m_nMinMetaTileCol = 0;
        int m_nMinMetaTileRow = 0;
        int m_nMetaTileWidth = 0;
        int m_nMetaTileHeight = 0;
        STACTADataset  *m_poMasterDS = nullptr;

        std::array<double, 6> m_adfGeoTransform = {{0.0,1.0,0,0.0,0.0,1.0}};
        OGRSpatialReference m_oSRS{};

    public:

        bool InitRaster(GDALDataset* poProtoDS,
                        const gdal::TileMatrixSet* poTMS,
                        const std::string& osTMId,
                        const gdal::TileMatrixSet::TileMatrix& oTM,
                        const std::map<CPLString, Limits>& oMapLimits);

        const OGRSpatialReference* GetSpatialRef() const override { return &m_oSRS; }
        CPLErr GetGeoTransform(double* padfGeoTransform) override;
        CPLErr IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg) override;

};

/************************************************************************/
/* ==================================================================== */
/*                        STACTARawRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class STACTARawRasterBand final: public GDALRasterBand
{
        GDALColorInterp m_eColorInterp = GCI_Undefined;
        int             m_bHasNoDataValue = false;
        double          m_dfNoData = 0;

    public:
        STACTARawRasterBand(STACTARawDataset* poDSIn, int nBandIn,
                            GDALRasterBand* poProtoBand);
        CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void* pImage) override;
        CPLErr IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GDALRasterIOExtraArg* psExtraArg) override;
        GDALColorInterp GetColorInterpretation() override { return m_eColorInterp; }
        double GetNoDataValue(int* pbHasNoData = nullptr) override;
};

#endif // STACTADATASET_H
