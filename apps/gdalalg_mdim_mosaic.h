/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim mosaic" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MDIM_MOSAIC_INCLUDED
#define GDALALG_MDIM_MOSAIC_INCLUDED

#include "gdalalgorithm.h"

#include "gdal_multidim.h"

#include <optional>
#include <utility>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALMdimMosaicAlgorithm                        */
/************************************************************************/

class GDALMdimMosaicAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "mosaic";
    static constexpr const char *DESCRIPTION =
        "Build a mosaic, either virtual (VRT) or materialized, from "
        "multidimensional datasets.";
    static constexpr const char *HELP_URL = "/programs/gdal_mdim_mosaic.html";

    explicit GDALMdimMosaicAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_outputFormat{};
    std::vector<GDALArgDatasetValue> m_inputDatasets{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite = false;
    std::vector<std::string> m_array{};

    // Describes a dimension of the mosaic array.
    struct DimensionDesc
    {
        std::string osName{};
        std::string osType{};
        std::string osDirection{};
        uint64_t nSize = 0;
        uint64_t nBlockSize = 0;
        std::vector<std::shared_ptr<GDALAttribute>> attributes{};

        // Used for dimensions with irregular spaced labels
        int nProgressionSign =
            0;  // 1=increasing, -1=decreasing, 0=single value
        // Groups of irregularly spaced values. In common cases,
        // aaValues[i].size() will be just one
        std::vector<std::vector<double>> aaValues{};

        // Used for dimensions with regularly spaced labels
        double dfStart = 0;
        double dfIncrement = 0;
    };

    // Minimum information about a dimension of a source array.
    struct SourceShortDimDesc
    {
        uint64_t nSize = 0;
        double dfStart = 0;
        bool bIsRegularlySpaced = false;
    };

    // For a given output array, gather parameters from source arrays and
    // output dimensions.
    struct ArrayParameters
    {
        std::vector<DimensionDesc> mosaicDimensions{};
        std::shared_ptr<GDALMDArray> poFirstSourceArray{};
        std::vector<std::vector<SourceShortDimDesc>> aaoSourceShortDimDesc{};
    };

    bool GetInputDatasetNames(GDALProgressFunc pfnProgress, void *pProgressData,
                              CPLStringList &aosInputDatasetNames) const;

    std::optional<DimensionDesc>
    // cppcheck-suppress functionStatic
    GetDimensionDesc(const std::string &osDSName,
                     const std::shared_ptr<GDALDimension> &poDim) const;

    bool BuildArrayParameters(const CPLStringList &aosInputDatasetNames,
                              std::vector<ArrayParameters> &aoArrayParameters);
};

//! @endcond

#endif
