/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_PIPELINE_INCLUDED
#define GDALALG_RASTER_PIPELINE_INCLUDED

#include "gdalalgorithm.h"
#include "gdalalg_abstract_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                  GDALRasterPipelineStepRunContext                    */
/************************************************************************/

class GDALRasterPipelineStepAlgorithm;

class GDALRasterPipelineStepRunContext
{
  public:
    GDALRasterPipelineStepRunContext() = default;

    // Progress callback to use during execution of the step
    GDALProgressFunc m_pfnProgress = nullptr;
    void *m_pProgressData = nullptr;

    // If there is a step in the pipeline immediately following step to which
    // this instance of GDALRasterPipelineStepRunContext is passed, and that
    // this next step is usable by the current step (as determined by
    // CanHandleNextStep()), then this member will point to this next step.
    GDALRasterPipelineStepAlgorithm *m_poNextUsableStep = nullptr;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALRasterPipelineStepRunContext)
};

/************************************************************************/
/*                GDALRasterPipelineStepAlgorithm                       */
/************************************************************************/

class GDALRasterPipelineStepAlgorithm /* non final */ : public GDALAlgorithm
{
  public:
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

  protected:
    using StepRunContext = GDALRasterPipelineStepRunContext;

    GDALRasterPipelineStepAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    bool standaloneStep);

    struct ConstructorOptions
    {
        bool standaloneStep = false;
        bool addDefaultArguments = true;
        std::string inputDatasetHelpMsg{};
        std::string inputDatasetAlias{};
        std::string inputDatasetMetaVar = "INPUT";
        std::string outputDatasetHelpMsg{};

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
    };

    GDALRasterPipelineStepAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    const ConstructorOptions &options);

    friend class GDALRasterPipelineAlgorithm;
    friend class GDALAbstractPipelineAlgorithm<GDALRasterPipelineStepAlgorithm>;

    virtual bool IsNativelyStreamingCompatible() const
    {
        return true;
    }

    virtual bool CanHandleNextStep(GDALRasterPipelineStepAlgorithm *) const
    {
        return false;
    }

    virtual bool RunStep(GDALRasterPipelineStepRunContext &ctxt) = 0;

    void AddInputArgs(bool openForMixedRasterVector, bool hiddenForCLI);
    void AddOutputArgs(bool hiddenForCLI);
    void AddHiddenInputDatasetArg();

    void SetOutputVRTCompatible(bool b);

    bool m_outputVRTCompatible = true;
    bool m_standaloneStep = false;
    const ConstructorOptions m_constructorOptions;

    // Input arguments
    GDALArgDatasetValue m_inputDataset{};
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

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;
    GDALAlgorithm::ProcessGDALGOutputRet ProcessGDALGOutput() override;
    bool CheckSafeForStreamOutput() override;

    CPL_DISALLOW_COPY_ASSIGN(GDALRasterPipelineStepAlgorithm)
};

/************************************************************************/
/*           GDALRasterPipelineNonNativelyStreamingAlgorithm            */
/************************************************************************/

class GDALRasterPipelineNonNativelyStreamingAlgorithm /* non-final */
    : public GDALRasterPipelineStepAlgorithm
{
  protected:
    GDALRasterPipelineNonNativelyStreamingAlgorithm(
        const std::string &name, const std::string &description,
        const std::string &helpURL, bool standaloneStep);

    bool IsNativelyStreamingCompatible() const override;

    std::unique_ptr<GDALDataset>
    CreateTemporaryDataset(int nWidth, int nHeight, int nBands,
                           GDALDataType eDT, bool bTiledIfPossible,
                           GDALDataset *poSrcDSForMetadata,
                           bool bCopyMetadata = true);
    std::unique_ptr<GDALDataset>
    CreateTemporaryCopy(GDALDataset *poSrcDS, int nSingleBand,
                        bool bTiledIfPossible, GDALProgressFunc pfnProgress,
                        void *pProgressData);
};

/************************************************************************/
/*                     GDALRasterPipelineAlgorithm                      */
/************************************************************************/

// This is an easter egg to pay tribute to PROJ pipeline syntax
// We accept "gdal vector +gdal=pipeline +step +gdal=read +input=in.tif +step +gdal=reproject +dst-crs=EPSG:32632 +step +gdal=write +output=out.tif +overwrite"
// as an alternative to (recommended):
// "gdal vector pipeline ! read in.tif ! reproject--dst-crs=EPSG:32632 ! write out.tif --overwrite"
#ifndef GDAL_PIPELINE_PROJ_NOSTALGIA
#define GDAL_PIPELINE_PROJ_NOSTALGIA
#endif

class GDALRasterPipelineAlgorithm final
    : public GDALAbstractPipelineAlgorithm<GDALRasterPipelineStepAlgorithm>
{
  public:
    static constexpr const char *NAME = "pipeline";
    static constexpr const char *DESCRIPTION = "Process a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_pipeline.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {
#ifdef GDAL_PIPELINE_PROJ_NOSTALGIA
            GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR,
            "+pipeline",
            "+gdal=pipeline",
#endif
        };
    }

    explicit GDALRasterPipelineAlgorithm(bool openForMixedRasterVector = false);

    bool
    ParseCommandLineArguments(const std::vector<std::string> &args) override;

    std::string GetUsageForCLI(bool shortUsage,
                               const UsageOptions &usageOptions) const override;

    GDALDataset *GetDatasetRef()
    {
        return m_inputDataset.GetDatasetRef();
    }

  protected:
    GDALArgDatasetValue &GetOutputDataset() override
    {
        return m_outputDataset;
    }

  private:
    std::string m_helpDocCategory{};
};

//! @endcond

#endif
