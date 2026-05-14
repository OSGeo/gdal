/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "partition" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_PARTITION_INCLUDED
#define GDALALG_VECTOR_PARTITION_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALVectorPartitionAlgorithm                     */
/************************************************************************/

class GDALVectorPartitionAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "partition";
    static constexpr const char *DESCRIPTION =
        "Partition a vector dataset into multiple files.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_partition.html";

    explicit GDALVectorPartitionAlgorithm(bool standaloneStep = false);

    bool CanBeLastStep() const override
    {
        return true;
    }

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

    static constexpr const char *SCHEME_HIVE = "hive";
    static constexpr const char *SCHEME_FLAT = "flat";

  private:
    static ConstructorOptions GetConstructorOptions(bool standaloneStep);
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::vector<std::string> m_fields{};
    int m_featureLimit = 0;
    std::string m_maxFileSizeStr{};
    GIntBig m_maxFileSize = 0;
    bool m_omitPartitionedFields = false;
    int m_maxCacheSize = 400;
    int m_transactionSize = 65536;
    std::string m_scheme = SCHEME_HIVE;
    std::string m_pattern{};

    // Computed
    bool m_partDigitLeadingZeroes = true;
    size_t m_partDigitCount = 10;
};

/************************************************************************/
/*                GDALVectorPartitionAlgorithmStandalone                */
/************************************************************************/

class GDALVectorPartitionAlgorithmStandalone final
    : public GDALVectorPartitionAlgorithm
{
  public:
    GDALVectorPartitionAlgorithmStandalone()
        : GDALVectorPartitionAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorPartitionAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_PARTITION_INCLUDED */
