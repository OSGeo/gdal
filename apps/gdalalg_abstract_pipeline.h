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

#include "cpl_conv.h"
#include "cpl_json.h"
#include "gdalalgorithm.h"
#include "gdal_priv.h"

#include <algorithm>

template <class StepAlgorithm>
class GDALAbstractPipelineAlgorithm CPL_NON_FINAL : public StepAlgorithm
{
  public:
    std::vector<std::string> GetAutoComplete(std::vector<std::string> &args,
                                             bool lastWordIsComplete,
                                             bool /* showAllOptions*/) override;

    bool Finalize() override;

    std::string GetUsageAsJSON() const override;

    /* cppcheck-suppress functionStatic */
    void SetDataset(GDALDataset *)
    {
    }

  protected:
    GDALAbstractPipelineAlgorithm(const std::string &name,
                                  const std::string &description,
                                  const std::string &helpURL,
                                  bool standaloneStep)
        : StepAlgorithm(name, description, helpURL, standaloneStep)
    {
    }

    ~GDALAbstractPipelineAlgorithm() override
    {
        // Destroy steps in the reverse order they have been constructed,
        // as a step can create object that depends on the validity of
        // objects of previous steps, and while cleaning them it needs those
        // prior objects to be still alive.
        // Typically for "gdal vector pipeline read ... ! sql ..."
        for (auto it = std::rbegin(m_steps); it != std::rend(m_steps); it++)
        {
            it->reset();
        }
    }

    virtual GDALArgDatasetValue &GetOutputDataset() = 0;

    std::string m_pipeline{};

    std::unique_ptr<StepAlgorithm> GetStepAlg(const std::string &name) const;

    GDALAlgorithmRegistry m_stepRegistry{};
    std::vector<std::unique_ptr<StepAlgorithm>> m_steps{};
    std::unique_ptr<StepAlgorithm> m_stepOnWhichHelpIsRequested{};

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;
};

/************************************************************************/
/*              GDALAbstractPipelineAlgorithm::GetStepAlg()             */
/************************************************************************/

template <class StepAlgorithm>
std::unique_ptr<StepAlgorithm>
GDALAbstractPipelineAlgorithm<StepAlgorithm>::GetStepAlg(
    const std::string &name) const
{
    auto alg = m_stepRegistry.Instantiate(name);
    return std::unique_ptr<StepAlgorithm>(
        cpl::down_cast<StepAlgorithm *>(alg.release()));
}

/************************************************************************/
/*         GDALAbstractPipelineAlgorithm::GetAutoComplete()             */
/************************************************************************/

template <class StepAlgorithm>
std::vector<std::string>
GDALAbstractPipelineAlgorithm<StepAlgorithm>::GetAutoComplete(
    std::vector<std::string> &args, bool lastWordIsComplete,
    bool /* showAllOptions*/)
{
    std::vector<std::string> ret;
    if (args.size() <= 1)
    {
        if (args.empty() || args.front() != "read")
            ret.push_back("read");
    }
    else if (args.back() == "!" ||
             (args[args.size() - 2] == "!" && !GetStepAlg(args.back())))
    {
        for (const std::string &name : m_stepRegistry.GetNames())
        {
            if (name != "read")
            {
                ret.push_back(name);
            }
        }
    }
    else
    {
        std::string lastStep = "read";
        std::vector<std::string> lastArgs;
        for (size_t i = 1; i < args.size(); ++i)
        {
            lastArgs.push_back(args[i]);
            if (i + 1 < args.size() && args[i] == "!")
            {
                ++i;
                lastArgs.clear();
                lastStep = args[i];
            }
        }

        auto curAlg = GetStepAlg(lastStep);
        if (curAlg)
        {
            ret = curAlg->GetAutoComplete(lastArgs, lastWordIsComplete,
                                          /* showAllOptions = */ false);
        }
    }
    return ret;
}

/************************************************************************/
/*              GDALAbstractPipelineAlgorithm::RunStep()                */
/************************************************************************/

template <class StepAlgorithm>
bool GDALAbstractPipelineAlgorithm<StepAlgorithm>::RunStep(
    GDALProgressFunc pfnProgress, void *pProgressData)
{
    if (m_stepOnWhichHelpIsRequested)
    {
        printf(
            "%s",
            m_stepOnWhichHelpIsRequested->GetUsageForCLI(false).c_str()); /*ok*/
        return true;
    }

    if (m_steps.empty())
    {
        // If invoked programmatically, not from the command line.

        if (m_pipeline.empty())
        {
            StepAlgorithm::ReportError(CE_Failure, CPLE_AppDefined,
                                       "'pipeline' argument not set");
            return false;
        }

        const CPLStringList aosTokens(CSLTokenizeString(m_pipeline.c_str()));
        if (!this->ParseCommandLineArguments(aosTokens))
            return false;
    }

    // Handle output to GDALG file
    if (!m_steps.empty() && m_steps.back()->GetName() == "write")
    {
        if (m_steps.back()->IsGDALGOutput())
        {
            const auto outputArg = m_steps.back()->GetArg(GDAL_ARG_NAME_OUTPUT);
            const auto &filename =
                outputArg->GDALAlgorithmArg::template Get<GDALArgDatasetValue>()
                    .GetName();
            const char *pszType = "";
            if (GDALDoesFileOrDatasetExist(filename.c_str(), &pszType))
            {
                const auto overwriteArg =
                    m_steps.back()->GetArg(GDAL_ARG_NAME_OVERWRITE);
                if (overwriteArg && overwriteArg->GetType() == GAAT_BOOLEAN)
                {
                    if (!overwriteArg->GDALAlgorithmArg::template Get<bool>())
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "%s '%s' already exists. Specify the "
                                 "--overwrite option to overwrite it.",
                                 pszType, filename.c_str());
                        return false;
                    }
                }
            }

            std::string osCommandLine;

            for (const auto &path : GDALAlgorithm::m_callPath)
            {
                if (!osCommandLine.empty())
                    osCommandLine += ' ';
                osCommandLine += path;
            }

            // Do not include the last step
            for (size_t i = 0; i + 1 < m_steps.size(); ++i)
            {
                const auto &step = m_steps[i];
                if (i > 0)
                    osCommandLine += " !";
                for (const auto &path : step->GDALAlgorithm::m_callPath)
                {
                    if (!osCommandLine.empty())
                        osCommandLine += ' ';
                    osCommandLine += path;
                }

                for (const auto &arg : step->GetArgs())
                {
                    if (arg->IsExplicitlySet())
                    {
                        osCommandLine += ' ';
                        std::string strArg;
                        if (!arg->Serialize(strArg))
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Cannot serialize argument %s",
                                     arg->GetName().c_str());
                            return false;
                        }
                        osCommandLine += strArg;
                    }
                }
            }

            return GDALAlgorithm::SaveGDALG(filename, osCommandLine);
        }

        const auto outputFormatArg =
            m_steps.back()->GetArg(GDAL_ARG_NAME_OUTPUT_FORMAT);
        const auto outputArg = m_steps.back()->GetArg(GDAL_ARG_NAME_OUTPUT);
        if (outputArg && outputArg->GetType() == GAAT_DATASET &&
            outputArg->IsExplicitlySet())
        {
            const auto &outputFile =
                outputArg
                    ->GDALAlgorithmArg::template Get<GDALArgDatasetValue>();
            bool isVRTOutput;
            if (outputFormatArg && outputFormatArg->GetType() == GAAT_STRING &&
                outputFormatArg->IsExplicitlySet())
            {
                const auto &val =
                    outputFormatArg
                        ->GDALAlgorithmArg::template Get<std::string>();
                isVRTOutput = EQUAL(val.c_str(), "vrt");
            }
            else
            {
                isVRTOutput = EQUAL(
                    CPLGetExtensionSafe(outputFile.GetName().c_str()).c_str(),
                    "vrt");
            }
            if (isVRTOutput && !outputFile.GetName().empty() &&
                m_steps.size() > 3)
            {
                StepAlgorithm::ReportError(
                    CE_Failure, CPLE_NotSupported,
                    "VRT output is not supported when there are more than 3 "
                    "steps. Consider using the GDALG driver (files with "
                    ".gdalg.json extension)");
                return false;
            }
            if (isVRTOutput)
            {
                for (const auto &step : m_steps)
                {
                    if (!step->m_outputVRTCompatible)
                    {
                        step->ReportError(
                            CE_Failure, CPLE_NotSupported,
                            "VRT output is not supported. Consider using the "
                            "GDALG driver instead (files with .gdalg.json "
                            "extension)");
                        return false;
                    }
                }
            }
        }
    }

    if (GDALAlgorithm::m_executionForStreamOutput)
    {
        // For security reasons, to avoid that reading a .gdalg.json file writes
        // a file on the file system.
        for (const auto &step : m_steps)
        {
            if (step->GetName() == "write" &&
                !EQUAL(step->m_format.c_str(), "stream"))
            {
                StepAlgorithm::ReportError(CE_Failure, CPLE_AppDefined,
                                           "in streamed execution, --format "
                                           "stream should be used");
                return false;
            }
        }
    }

    GDALDataset *poCurDS = nullptr;
    for (size_t i = 0; i < m_steps.size(); ++i)
    {
        auto &step = m_steps[i];
        if (i > 0)
        {
            if constexpr (std::is_same_v<decltype(step->m_inputDataset),
                                         GDALArgDatasetValue>)
            {
                if (step->m_inputDataset.GetDatasetRef())
                {
                    // Shouldn't happen
                    StepAlgorithm::ReportError(
                        CE_Failure, CPLE_AppDefined,
                        "Step nr %d (%s) has already an input dataset",
                        static_cast<int>(i), step->GetName().c_str());
                    return false;
                }
                step->m_inputDataset.Set(poCurDS);
            }
            else if constexpr (std::is_same_v<decltype(step->m_inputDataset),
                                              std::vector<GDALArgDatasetValue>>)
            {
                if (!step->m_inputDataset.empty() &&
                    step->m_inputDataset[0].GetDatasetRef())
                {
                    // Shouldn't happen
                    StepAlgorithm::ReportError(
                        CE_Failure, CPLE_AppDefined,
                        "Step nr %d (%s) has already an input dataset",
                        static_cast<int>(i), step->GetName().c_str());
                    return false;
                }
                step->m_inputDataset.clear();
                step->m_inputDataset.resize(1);
                step->m_inputDataset[0].Set(poCurDS);
            }
        }
        if (i + 1 < m_steps.size() && step->m_outputDataset.GetDatasetRef())
        {
            // Shouldn't happen
            StepAlgorithm::ReportError(
                CE_Failure, CPLE_AppDefined,
                "Step nr %d (%s) has already an output dataset",
                static_cast<int>(i), step->GetName().c_str());
            return false;
        }
        if (!step->Run(i < m_steps.size() - 1 ? nullptr : pfnProgress,
                       i < m_steps.size() - 1 ? nullptr : pProgressData))
        {
            return false;
        }
        poCurDS = step->m_outputDataset.GetDatasetRef();
        if (!poCurDS)
        {
            StepAlgorithm::ReportError(
                CE_Failure, CPLE_AppDefined,
                "Step nr %d (%s) failed to produce an output dataset",
                static_cast<int>(i), step->GetName().c_str());
            return false;
        }
    }

    if (!GetOutputDataset().GetDatasetRef())
    {
        GetOutputDataset().Set(poCurDS);
    }

    return true;
}

/************************************************************************/
/*               GDALAbstractPipelineAlgorithm::Finalize()              */
/************************************************************************/

template <class StepAlgorithm>
bool GDALAbstractPipelineAlgorithm<StepAlgorithm>::Finalize()
{
    bool ret = GDALAlgorithm::Finalize();
    for (auto &step : m_steps)
    {
        ret = step->Finalize() && ret;
    }
    return ret;
}

/************************************************************************/
/*             GDALAbstractPipelineAlgorithm::GetUsageAsJSON()          */
/************************************************************************/

template <class StepAlgorithm>
std::string GDALAbstractPipelineAlgorithm<StepAlgorithm>::GetUsageAsJSON() const
{
    CPLJSONDocument oDoc;
    CPL_IGNORE_RET_VAL(oDoc.LoadMemory(GDALAlgorithm::GetUsageAsJSON()));

    CPLJSONArray jPipelineSteps;
    for (const std::string &name : m_stepRegistry.GetNames())
    {
        auto alg = GetStepAlg(name);
        CPLJSONDocument oStepDoc;
        CPL_IGNORE_RET_VAL(oStepDoc.LoadMemory(alg->GetUsageAsJSON()));
        jPipelineSteps.Add(oStepDoc.GetRoot());
    }
    oDoc.GetRoot().Add("pipeline_algorithms", jPipelineSteps);

    return oDoc.SaveAsString();
}

//! @endcond

#endif
