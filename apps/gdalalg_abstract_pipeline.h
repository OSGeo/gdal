/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster/vector pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_ABSTRACT_PIPELINE_INCLUDED
#define GDALALG_ABSTRACT_PIPELINE_INCLUDED

//! @cond Doxygen_Suppress

#include "cpl_json.h"

#include "gdalalgorithm.h"
#include "gdalpipelinestepalgorithm.h"
#include "gdal_priv.h"

#include <algorithm>

// This is an easter egg to pay tribute to PROJ pipeline syntax
// We accept "gdal vector +gdal=pipeline +step +gdal=read +input=in.tif +step +gdal=reproject +output-crs=EPSG:32632 +step +gdal=write +output=out.tif +overwrite"
// as an alternative to (recommended):
// "gdal vector pipeline ! read in.tif ! reproject--output-crs=EPSG:32632 ! write out.tif --overwrite"
#ifndef GDAL_PIPELINE_PROJ_NOSTALGIA
#define GDAL_PIPELINE_PROJ_NOSTALGIA
#endif

/************************************************************************/
/*                    GDALAbstractPipelineAlgorithm                     */
/************************************************************************/

class GDALAbstractPipelineAlgorithm CPL_NON_FINAL
    : public GDALPipelineStepAlgorithm
{
  public:
    std::vector<std::string> GetAutoComplete(std::vector<std::string> &args,
                                             bool lastWordIsComplete,
                                             bool /* showAllOptions*/) override;

    bool Finalize() override;

    std::string GetUsageAsJSON() const override;

    bool
    ParseCommandLineArguments(const std::vector<std::string> &args) override;

    bool HasSteps() const
    {
        return !m_steps.empty();
    }

    static constexpr const char *OPEN_NESTED_PIPELINE = "[";
    static constexpr const char *CLOSE_NESTED_PIPELINE = "]";

    static constexpr const char *RASTER_SUFFIX = "-raster";
    static constexpr const char *VECTOR_SUFFIX = "-vector";

  protected:
    friend class GDALTeeStepAlgorithmAbstract;

    GDALAbstractPipelineAlgorithm(
        const std::string &name, const std::string &description,
        const std::string &helpURL,
        const GDALPipelineStepAlgorithm::ConstructorOptions &options)
        : GDALPipelineStepAlgorithm(
              name, description, helpURL,
              ConstructorOptions(options).SetAutoOpenInputDatasets(false))
    {
    }

    std::string m_pipeline{};

    virtual GDALAlgorithmRegistry &GetStepRegistry() = 0;

    virtual const GDALAlgorithmRegistry &GetStepRegistry() const = 0;

    std::unique_ptr<GDALPipelineStepAlgorithm>
    GetStepAlg(const std::string &name) const;

    bool HasOutputString() const override;

    static bool IsReadSpecificArgument(const char *pszArgName);
    static bool IsWriteSpecificArgument(const char *pszArgName);

  private:
    friend class GDALPipelineAlgorithm;
    friend class GDALRasterPipelineAlgorithm;
    friend class GDALVectorPipelineAlgorithm;

    std::vector<std::unique_ptr<GDALPipelineStepAlgorithm>> m_steps{};

    std::unique_ptr<GDALPipelineStepAlgorithm> m_stepOnWhichHelpIsRequested{};

    bool m_bInnerPipeline = false;
    bool m_bExpectReadStep = true;
    int m_nFirstStepWithUnknownInputType = -1;

    enum class StepConstraint
    {
        MUST_BE,
        CAN_BE,
        CAN_NOT_BE
    };

    StepConstraint m_eLastStepAsWrite = StepConstraint::CAN_BE;

    std::vector<std::unique_ptr<GDALAbstractPipelineAlgorithm>>
        m_apoNestedPipelines{};

    // More would lead to unreadable pipelines
    static constexpr int MAX_NESTING_LEVEL = 3;

    bool
    CheckFirstAndLastStep(const std::vector<GDALPipelineStepAlgorithm *> &steps,
                          bool forAutoComplete) const;

    static int GetInputDatasetType(const GDALPipelineStepAlgorithm *alg);

    bool CopyStepAlgorithmFromAnother(GDALPipelineStepAlgorithm *dst,
                                      const GDALPipelineStepAlgorithm *src,
                                      bool maybeWriteStep) const;

    bool ParseCommandLineArguments(
        const std::vector<std::string> &args, bool forAutoComplete,
        std::vector<std::string> *pCurArgsForAutocomplete);

    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    enum class RunStepState
    {
        PROCESSED,
        GO_ON,
        ERROR,
    };
    RunStepState
    RunStepDealWithMultiProcessing(GDALPipelineStepRunContext &ctxt);
    RunStepState RunStepDealWithGDALGJson();
    bool RunStepDealWithStepUnknownInputType(size_t i, int nCurDatasetType);
    bool CheckStepHasNoInputDatasetAlreadySet(size_t i, GDALDataset *poCurDS);

    std::string
    BuildNestedPipeline(GDALPipelineStepAlgorithm *curAlg,
                        std::vector<std::string> &nestedPipelineArgs,
                        bool forAutoComplete,
                        std::vector<std::string> *pCurArgsForAutocomplete);

    bool SaveGDALGIntoFileOrString(const std::string &outFilename,
                                   std::string &outString) const;

    virtual std::unique_ptr<GDALAbstractPipelineAlgorithm>
    CreateNestedPipeline() const = 0;
};

//! @endcond

#endif
