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

/************************************************************************/
/*                      GDALPipelineStepRunContext                      */
/************************************************************************/

class GDALPipelineStepAlgorithm;

class GDALPipelineStepRunContext
{
  public:
    GDALPipelineStepRunContext() = default;

    // Progress callback to use during execution of the step
    GDALProgressFunc m_pfnProgress = nullptr;
    void *m_pProgressData = nullptr;

    // If there is a step in the pipeline immediately following step to which
    // this instance of GDALRasterPipelineStepRunContext is passed, and that
    // this next step is usable by the current step (as determined by
    // CanHandleNextStep()), then this member will point to this next step.
    GDALPipelineStepAlgorithm *m_poNextUsableStep = nullptr;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALPipelineStepRunContext)
};

/************************************************************************/
/*                    GDALAbstractPipelineAlgorithm                     */
/************************************************************************/

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

    GDALAbstractPipelineAlgorithm(
        const std::string &name, const std::string &description,
        const std::string &helpURL,
        const typename StepAlgorithm::ConstructorOptions &options)
        : StepAlgorithm(name, description, helpURL, options)
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

    std::string m_pipeline{};
    GDALAlgorithmRegistry m_stepRegistry{};
    std::vector<std::unique_ptr<StepAlgorithm>> m_steps{};
    std::unique_ptr<StepAlgorithm> m_stepOnWhichHelpIsRequested{};

    std::unique_ptr<StepAlgorithm> GetStepAlg(const std::string &name) const;

    bool CheckFirstStep(const std::vector<StepAlgorithm *> &steps) const;

    static constexpr const char *RASTER_SUFFIX = "-raster";
    static constexpr const char *VECTOR_SUFFIX = "-vector";

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
};

/************************************************************************/
/*                     GDALPipelineStepAlgorithm                        */
/************************************************************************/

class GDALPipelineStepAlgorithm /* non final */ : public GDALAlgorithm
{
  public:
    const std::vector<GDALArgDatasetValue> &GetInputDatasets() const
    {
        return m_inputDataset;
    }

    const GDALArgDatasetValue &GetOutputDataset() const
    {
        return m_outputDataset;
    }

    const std::string &GetOutputFormat() const
    {
        return m_format;
    }

    const std::vector<std::string> &GetCreationOptions() const
    {
        return m_creationOptions;
    }

    virtual int GetInputType() const = 0;

    virtual int GetOutputType() const = 0;

  protected:
    struct ConstructorOptions
    {
        bool standaloneStep = false;
        bool addDefaultArguments = true;
        bool autoOpenInputDatasets = true;
        bool outputDatasetRequired = true;
        bool addInputLayerNameArgument = true;  // only for vector input
        int inputDatasetMaxCount = 1;
        std::string inputDatasetHelpMsg{};
        std::string inputDatasetAlias{};
        std::string inputDatasetMetaVar = "INPUT";
        std::string outputDatasetHelpMsg{};
        std::string updateMutualExclusionGroup{};
        std::string outputDatasetMutualExclusionGroup{};
        std::string outputFormatCreateCapability = GDAL_DCAP_CREATECOPY;

        inline ConstructorOptions &SetStandaloneStep(bool b)
        {
            standaloneStep = b;
            return *this;
        }

        inline ConstructorOptions &SetAddDefaultArguments(bool b)
        {
            addDefaultArguments = b;
            return *this;
        }

        inline ConstructorOptions &SetAddInputLayerNameArgument(bool b)
        {
            addInputLayerNameArgument = b;
            return *this;
        }

        inline ConstructorOptions &SetInputDatasetMaxCount(int maxCount)
        {
            inputDatasetMaxCount = maxCount;
            return *this;
        }

        inline ConstructorOptions &SetInputDatasetHelpMsg(const std::string &s)
        {
            inputDatasetHelpMsg = s;
            return *this;
        }

        inline ConstructorOptions &SetInputDatasetAlias(const std::string &s)
        {
            inputDatasetAlias = s;
            return *this;
        }

        inline ConstructorOptions &SetInputDatasetMetaVar(const std::string &s)
        {
            inputDatasetMetaVar = s;
            return *this;
        }

        inline ConstructorOptions &SetOutputDatasetHelpMsg(const std::string &s)
        {
            outputDatasetHelpMsg = s;
            return *this;
        }

        inline ConstructorOptions &SetAutoOpenInputDatasets(bool b)
        {
            autoOpenInputDatasets = b;
            return *this;
        }

        inline ConstructorOptions &SetOutputDatasetRequired(bool b)
        {
            outputDatasetRequired = b;
            return *this;
        }

        inline ConstructorOptions &
        SetUpdateMutualExclusionGroup(const std::string &s)
        {
            updateMutualExclusionGroup = s;
            return *this;
        }

        inline ConstructorOptions &
        SetOutputDatasetMutualExclusionGroup(const std::string &s)
        {
            outputDatasetMutualExclusionGroup = s;
            return *this;
        }

        inline ConstructorOptions &
        SetOutputFormatCreateCapability(const std::string &capability)
        {
            outputFormatCreateCapability = capability;
            return *this;
        }
    };

    GDALPipelineStepAlgorithm(const std::string &name,
                              const std::string &description,
                              const std::string &helpURL,
                              const ConstructorOptions &);

    friend class GDALPipelineAlgorithm;
    friend class GDALAbstractPipelineAlgorithm<GDALPipelineStepAlgorithm>;

    virtual bool CanBeFirstStep() const
    {
        return false;
    }

    virtual bool IsNativelyStreamingCompatible() const
    {
        return true;
    }

    virtual bool CanHandleNextStep(GDALPipelineStepAlgorithm *) const
    {
        return false;
    }

    virtual bool RunStep(GDALPipelineStepRunContext &ctxt) = 0;

    bool m_standaloneStep = false;
    const ConstructorOptions m_constructorOptions;
    bool m_outputVRTCompatible = true;
    std::string m_helpDocCategory{};

    // Input arguments
    std::vector<GDALArgDatasetValue> m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    std::vector<std::string> m_inputLayerNames{};

    // Output arguments
    GDALArgDatasetValue m_outputDataset{};
    std::string m_format{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite = false;
    std::string m_outputLayerName{};
    GDALInConstructionAlgorithmArg *m_outputFormatArg = nullptr;

    // Output arguments (vector specific)
    std::vector<std::string> m_layerCreationOptions{};
    bool m_update = false;
    bool m_overwriteLayer = false;
    bool m_appendLayer = false;

    void AddRasterInputArgs(bool openForMixedRasterVector, bool hiddenForCLI);
    void AddRasterOutputArgs(bool hiddenForCLI);
    void AddRasterHiddenInputDatasetArg();

    void AddVectorInputArgs(bool hiddenForCLI);
    void AddVectorOutputArgs(bool hiddenForCLI,
                             bool shortNameOutputLayerAllowed);

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;
    GDALAlgorithm::ProcessGDALGOutputRet ProcessGDALGOutput() override;
    bool CheckSafeForStreamOutput() override;

    CPL_DISALLOW_COPY_ASSIGN(GDALPipelineStepAlgorithm)
};

/************************************************************************/
/*            GDALAbstractPipelineAlgorithm::CheckFirstStep()           */
/************************************************************************/

template <class StepAlgorithm>
bool GDALAbstractPipelineAlgorithm<StepAlgorithm>::CheckFirstStep(
    const std::vector<StepAlgorithm *> &steps) const
{
    if (!steps.front()->CanBeFirstStep())
    {
        std::vector<std::string> firstStepNames{"read"};
        for (const auto &stepName : m_stepRegistry.GetNames())
        {
            auto alg = GetStepAlg(stepName);
            if (alg && alg->CanBeFirstStep() && stepName != "read")
            {
                firstStepNames.push_back(stepName);
            }
        }

        std::string msg = "First step should be ";
        for (size_t i = 0; i < firstStepNames.size(); ++i)
        {
            if (i == firstStepNames.size() - 1)
                msg += " or ";
            else if (i > 0)
                msg += ", ";
            msg += '\'';
            msg += firstStepNames[i];
            msg += '\'';
        }

        StepAlgorithm::ReportError(CE_Failure, CPLE_AppDefined, "%s",
                                   msg.c_str());
        return false;
    }
    for (size_t i = 1; i < steps.size() - 1; ++i)
    {
        if (steps[i]->CanBeFirstStep())
        {
            StepAlgorithm::ReportError(CE_Failure, CPLE_AppDefined,
                                       "Only first step can be '%s'",
                                       steps[i]->GetName().c_str());
            return false;
        }
    }
    return true;
}

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
    GDALPipelineStepRunContext &ctxt)
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
                if (!step->IsNativelyStreamingCompatible())
                {
                    GDALAlgorithm::ReportError(
                        CE_Warning, CPLE_AppDefined,
                        "Step %s is not natively streaming compatible, and "
                        "may cause significant processing time at opening",
                        step->GDALAlgorithm::GetName().c_str());
                }

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

    int countPipelinesWithProgress = 0;
    for (size_t i = 1; i < m_steps.size(); ++i)
    {
        const bool bCanHandleNextStep =
            i < m_steps.size() - 1 &&
            !m_steps[i]->CanHandleNextStep(m_steps[i + 1].get());
        if (bCanHandleNextStep &&
            !m_steps[i + 1]->IsNativelyStreamingCompatible())
            ++countPipelinesWithProgress;
        else if (!m_steps[i]->IsNativelyStreamingCompatible())
            ++countPipelinesWithProgress;
        if (bCanHandleNextStep)
            ++i;
    }
    if (countPipelinesWithProgress == 0)
        countPipelinesWithProgress = 1;

    GDALDataset *poCurDS = nullptr;
    int iCurPipelineWithProgress = 0;
    for (size_t i = 0; i < m_steps.size(); ++i)
    {
        auto &step = m_steps[i];
        if (i > 0)
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
        if (i + 1 < m_steps.size() && step->m_outputDataset.GetDatasetRef())
        {
            // Shouldn't happen
            StepAlgorithm::ReportError(
                CE_Failure, CPLE_AppDefined,
                "Step nr %d (%s) has already an output dataset",
                static_cast<int>(i), step->GetName().c_str());
            return false;
        }

        const bool bCanHandleNextStep =
            i < m_steps.size() - 1 &&
            step->CanHandleNextStep(m_steps[i + 1].get());

        std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)> pScaledData(
            nullptr, GDALDestroyScaledProgress);
        GDALPipelineStepRunContext stepCtxt;
        if ((bCanHandleNextStep &&
             m_steps[i + 1]->IsNativelyStreamingCompatible()) ||
            !step->IsNativelyStreamingCompatible())
        {
            pScaledData.reset(GDALCreateScaledProgress(
                iCurPipelineWithProgress /
                    static_cast<double>(countPipelinesWithProgress),
                (iCurPipelineWithProgress + 1) /
                    static_cast<double>(countPipelinesWithProgress),
                ctxt.m_pfnProgress, ctxt.m_pProgressData));
            ++iCurPipelineWithProgress;
            stepCtxt.m_pfnProgress = pScaledData ? GDALScaledProgress : nullptr;
            stepCtxt.m_pProgressData = pScaledData.get();
        }
        if (bCanHandleNextStep)
        {
            stepCtxt.m_poNextUsableStep = m_steps[i + 1].get();
        }
        if (!step->ValidateArguments() || !step->RunStep(stepCtxt))
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

        if (bCanHandleNextStep)
        {
            ++i;
        }
    }

    if (ctxt.m_pfnProgress)
        ctxt.m_pfnProgress(1.0, "", ctxt.m_pProgressData);

    if (!StepAlgorithm::m_outputDataset.GetDatasetRef())
    {
        StepAlgorithm::m_outputDataset.Set(poCurDS);
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
        if (!alg->IsHidden())
        {
            CPLJSONDocument oStepDoc;
            CPL_IGNORE_RET_VAL(oStepDoc.LoadMemory(alg->GetUsageAsJSON()));
            jPipelineSteps.Add(oStepDoc.GetRoot());
        }
    }
    oDoc.GetRoot().Add("pipeline_algorithms", jPipelineSteps);

    return oDoc.SaveAsString();
}

//! @endcond

#endif
