/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster compare" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_COMPARE_INCLUDED
#define GDALALG_RASTER_COMPARE_INCLUDED

#include "gdalrasterpipelinestepalgorithm.h"

class GDALRasterBand;
class GDALDataset;

#include <algorithm>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALRasterCompareAlgorithm                      */
/************************************************************************/

class GDALRasterCompareAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "compare";
    static constexpr const char *DESCRIPTION = "Compare two raster datasets.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_compare.html";

    explicit GDALRasterCompareAlgorithm(bool standaloneStep = false);

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

    bool CanBeLastStep() const override
    {
        return true;
    }

    int GetOutputType() const override
    {
        return 0;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    bool BinaryComparison(std::vector<std::string> &aosReport,
                          GDALDataset *poRefDS, GDALDataset *poInputDS);

    void DatasetComparison(std::vector<std::string> &aosReport,
                           GDALDataset *poRefDS, GDALDataset *poInputDS,
                           GDALProgressFunc pfnProgress, void *pProgressData);

    static void CRSComparison(std::vector<std::string> &aosReport,
                              GDALDataset *poRefDS, GDALDataset *poInputDS);

    static void GeoTransformComparison(std::vector<std::string> &aosReport,
                                       GDALDataset *poRefDS,
                                       GDALDataset *poInputDS);

    void BandComparison(std::vector<std::string> &aosReport,
                        const std::string &bandId,
                        bool doBandBasedPixelComparison,
                        GDALRasterBand *poRefBand, GDALRasterBand *poInputBand,
                        GDALProgressFunc pfnProgress, void *pProgressData);

    static void MetadataComparison(std::vector<std::string> &aosReport,
                                   const std::string &metadataDomain,
                                   CSLConstList aosRef, CSLConstList aosInput);

    static constexpr const char *METRIC_ALL = "all";
    static constexpr const char *METRIC_NONE = "none";
    static constexpr const char *METRIC_DIFF = "diff";
    static constexpr const char *METRIC_RMSD = "RMSD";
    static constexpr const char *METRIC_PSNR = "PSNR";

    bool HasMetric(const char *pszMetric) const
    {
        CPLAssert(!EQUAL(pszMetric, METRIC_ALL));
        return std::find(m_metrics.begin(), m_metrics.end(), pszMetric) !=
                   m_metrics.end() ||
               (!EQUAL(pszMetric, METRIC_NONE) &&
                std::find(m_metrics.begin(), m_metrics.end(), METRIC_ALL) !=
                    m_metrics.end());
    }

    GDALArgDatasetValue m_referenceDataset{};

    static constexpr const char *METRIC_DEFAULT = METRIC_DIFF;
    std::vector<std::string> m_metrics{METRIC_DEFAULT};
    bool m_skipAllOptional = false;
    bool m_skipBinary = false;
    bool m_skipCRS = false;
    bool m_skipGeotransform = false;
    bool m_skipOverview = false;
    bool m_skipMetadata = false;
    bool m_skipRPC = false;
    bool m_skipGeolocation = false;
    bool m_skipSubdataset = false;
    // If adding a new skip flag, make sure that m_skipAll takes it into account
    int m_retCode = 0;
};

/************************************************************************/
/*                 GDALRasterCompareAlgorithmStandalone                 */
/************************************************************************/

class GDALRasterCompareAlgorithmStandalone final
    : public GDALRasterCompareAlgorithm
{
  public:
    GDALRasterCompareAlgorithmStandalone()
        : GDALRasterCompareAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterCompareAlgorithmStandalone() override;
};

//! @endcond

#endif
