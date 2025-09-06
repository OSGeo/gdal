/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "tee" pipeline step
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_TEE_INCLUDED
#define GDALALG_TEE_INCLUDED

#include "gdalalg_abstract_pipeline.h"

#include <utility>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                         GDALTeeStepAlgorithm                         */
/************************************************************************/

class GDALTeeStepAlgorithm /* non final */
    : public GDALPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "tee";
    static constexpr const char *DESCRIPTION =
        "Pipes the input into the output stream and side nested pipelines.";
    static constexpr const char *HELP_URL = "/programs/gdal_pipeline.html";

    bool BindFilename(const std::string &filename,
                      GDALPipelineStepAlgorithm *alg,
                      const std::vector<std::string> &args)
    {
        if (cpl::contains(m_oMapNameToAlg, filename))
            return false;
        m_oMapNameToAlg[filename] = std::make_pair(alg, args);
        return true;
    }

    void CopyFilenameBindingsFrom(const GDALTeeStepAlgorithm *other)
    {
        m_oMapNameToAlg = other->m_oMapNameToAlg;
    }

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

    bool CanBeMiddleStep() const override
    {
        return true;
    }

    bool CanBeLastStep() const override
    {
        return true;
    }

    bool GeneratesFilesFromUserInput() const override
    {
        return true;
    }

  protected:
    explicit GDALTeeStepAlgorithm(int nDatasetType);

    int GetInputType() const override
    {
        return m_nDatasetType;
    }

    int GetOutputType() const override
    {
        return m_nDatasetType;
    }

  private:
    int m_nDatasetType = 0;
    std::vector<GDALArgDatasetValue> m_pipelines{};
    std::map<std::string,
             std::pair<GDALPipelineStepAlgorithm *, std::vector<std::string>>>
        m_oMapNameToAlg{};

    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
};

class GDALTeeRasterAlgorithm final : public GDALTeeStepAlgorithm
{
  public:
    GDALTeeRasterAlgorithm() : GDALTeeStepAlgorithm(GDAL_OF_RASTER)
    {
    }

    ~GDALTeeRasterAlgorithm();
};

class GDALTeeVectorAlgorithm final : public GDALTeeStepAlgorithm
{
  public:
    GDALTeeVectorAlgorithm() : GDALTeeStepAlgorithm(GDAL_OF_VECTOR)
    {
    }

    ~GDALTeeVectorAlgorithm();
};

//! @endcond

#endif
