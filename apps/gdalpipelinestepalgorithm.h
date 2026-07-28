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

#ifndef GDALPIPELINESTEPALGORITHM_INCLUDED
#define GDALPIPELINESTEPALGORITHM_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalalgorithm.h"

#include "gdalpipelinestepruncontext.h"

/************************************************************************/
/*                      GDALPipelineStepAlgorithm                       */
/************************************************************************/

class OGRLayer;

class GDALPipelineStepAlgorithm /* non final */ : public GDALAlgorithm
{
  public:
    std::vector<GDALArgDatasetValue> &GetInputDatasets()
    {
        return m_inputDataset;
    }

    const std::vector<GDALArgDatasetValue> &GetInputDatasets() const
    {
        return m_inputDataset;
    }

    GDALArgDatasetValue &GetOutputDataset()
    {
        return m_outputDataset;
    }

    const GDALArgDatasetValue &GetOutputDataset() const
    {
        return m_outputDataset;
    }

    const std::string &GetOutputString() const
    {
        return m_output;
    }

    const std::string &GetOutputLayerName() const
    {
        return m_outputLayerName;
    }

    const std::string &GetOutputFormat() const
    {
        return m_format;
    }

    const std::vector<std::string> &GetCreationOptions() const
    {
        return m_creationOptions;
    }

    const std::vector<std::string> &GetLayerCreationOptions() const
    {
        return m_layerCreationOptions;
    }

    bool GetOverwriteLayer() const
    {
        return m_overwriteLayer;
    }

    bool GetAppendLayer() const
    {
        return m_appendLayer;
    }

    virtual int GetInputType() const = 0;

    virtual int GetOutputType() const = 0;

    bool Finalize() override;

    // Used by GDALDispatcherAlgorithm for vector info/convert
    GDALDataset *GetInputDatasetRef()
    {
        return m_inputDataset.empty() ? nullptr
                                      : m_inputDataset[0].GetDatasetRef();
    }

    // Used by GDALDispatcherAlgorithm for vector info/convert
    void SetInputDataset(GDALDataset *poDS);

  protected:
    struct ConstructorOptions
    {
        bool standaloneStep = false;
        bool addDefaultArguments = true;
        bool autoOpenInputDatasets = true;
        bool inputDatasetRequired = true;
        bool inputDatasetPositional = true;
        bool outputDatasetRequired = true;
        bool addInputLayerNameArgument = true;        // only for vector input
        bool addUpdateArgument = true;                // only for vector output
        bool addAppendLayerArgument = true;           // only for vector output
        bool addNoCreateEmptyLayersArgument = false;  // only for vector output
        bool addOverwriteLayerArgument = true;        // only for vector output
        bool addUpsertArgument = true;                // only for vector output
        bool addSkipErrorsArgument = true;            // only for vector output
        bool addOutputLayerNameArgument = true;       // only for vector output
        bool outputLayerNameAvailableInPipelineStep = false;
        int inputDatasetMaxCount = 1;
        int inputDatasetInputFlags = GADV_NAME | GADV_OBJECT;
        std::string inputDatasetHelpMsg{};
        std::string inputDatasetAlias{};
        std::string inputDatasetMetaVar = "INPUT";
        std::string outputDatasetHelpMsg{};
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

        inline ConstructorOptions &SetInputDatasetRequired(bool b)
        {
            inputDatasetRequired = b;
            return *this;
        }

        inline ConstructorOptions &SetInputDatasetPositional(bool b)
        {
            inputDatasetPositional = b;
            return *this;
        }

        inline ConstructorOptions &SetInputDatasetMaxCount(int maxCount)
        {
            inputDatasetMaxCount = maxCount;
            return *this;
        }

        inline ConstructorOptions &SetInputDatasetInputFlags(int flags)
        {
            inputDatasetInputFlags = flags;
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
        SetOutputFormatCreateCapability(const std::string &capability)
        {
            outputFormatCreateCapability = capability;
            return *this;
        }

        inline ConstructorOptions &SetAddAppendLayerArgument(bool b)
        {
            addAppendLayerArgument = b;
            return *this;
        }

        inline ConstructorOptions &SetAddOverwriteLayerArgument(bool b)
        {
            addOverwriteLayerArgument = b;
            return *this;
        }

        inline ConstructorOptions &SetAddUpdateArgument(bool b)
        {
            addUpdateArgument = b;
            return *this;
        }

        inline ConstructorOptions &SetAddUpsertArgument(bool b)
        {
            addUpsertArgument = b;
            return *this;
        }

        inline ConstructorOptions &SetNoCreateEmptyLayersArgument(bool b)
        {
            addNoCreateEmptyLayersArgument = b;
            return *this;
        }

        inline ConstructorOptions &SetAddSkipErrorsArgument(bool b)
        {
            addSkipErrorsArgument = b;
            return *this;
        }

        inline ConstructorOptions &SetAddOutputLayerNameArgument(bool b)
        {
            addOutputLayerNameArgument = b;
            return *this;
        }

        inline ConstructorOptions &
        SetOutputLayerNameAvailableInPipelineStep(bool b)
        {
            outputLayerNameAvailableInPipelineStep = b;
            return *this;
        }
    };

    GDALPipelineStepAlgorithm(const std::string &name,
                              const std::string &description,
                              const std::string &helpURL,
                              const ConstructorOptions &);

    friend class GDALPipelineAlgorithm;
    friend class GDALRasterPipelineAlgorithm;
    friend class GDALVectorPipelineAlgorithm;
    friend class GDALMdimPipelineAlgorithm;
    friend class GDALAbstractPipelineAlgorithm;

    virtual bool CanBeFirstStep() const
    {
        return false;
    }

    virtual bool CanBeMiddleStep() const
    {
        return !CanBeFirstStep() && !CanBeLastStep();
    }

    virtual bool CanBeLastStep() const
    {
        return false;
    }

    //! Whether a user parameter can cause a file to be written at a specified location
    virtual bool GeneratesFilesFromUserInput() const
    {
        return false;
    }

    virtual bool IsNativelyStreamingCompatible() const
    {
        return true;
    }

    virtual bool SupportsInputMultiThreading() const
    {
        return false;
    }

    virtual bool CanHandleNextStep(GDALPipelineStepAlgorithm *) const
    {
        return false;
    }

    virtual bool OutputDatasetAllowedBeforeRunningStep() const
    {
        return false;
    }

    virtual CPLJSONObject Get_OGR_SCHEMA_OpenOption_Layer() const
    {
        CPLJSONObject obj;
        obj.Deinit();
        return obj;
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
    bool m_stdout = false;
    std::string m_output{};
    GDALArgDatasetValue m_outputDataset{};
    std::string m_format{};
    std::vector<std::string> m_outputOpenOptions{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite = false;
    std::string m_outputLayerName{};
    GDALInConstructionAlgorithmArg *m_outputFormatArg = nullptr;
    bool m_appendRaster = false;

    // Output arguments (vector specific)
    std::vector<std::string> m_layerCreationOptions{};
    bool m_update = false;
    bool m_overwriteLayer = false;
    bool m_appendLayer = false;
    bool m_upsert = false;
    bool m_skipErrors = false;
    bool m_noCreateEmptyLayers = false;

    void AddRasterInputArgs(bool openForMixedRasterVector, bool hiddenForCLI);
    void AddRasterOutputArgs(bool hiddenForCLI);
    void AddRasterHiddenInputDatasetArg();

    void AddVectorInputArgs(bool hiddenForCLI);
    void AddVectorHiddenInputDatasetArg();
    void AddVectorOutputArgs(bool hiddenForCLI,
                             bool shortNameOutputLayerAllowed);
    using GDALAlgorithm::AddOutputLayerNameArg;
    void AddOutputLayerNameArg(bool hiddenForCLI,
                               bool shortNameOutputLayerAllowed);

    void AddMdimInputArgs(bool openForMixedRasterVector, bool hiddenForCLI,
                          bool acceptRaster);
    void AddMdimOutputArgs(bool hiddenForCLI);
    void AddMdimHiddenInputDatasetArg();

    bool CreateDatasetSingleOutputLayerIfNeeded(
        GDALPipelineStepRunContext &ctxt, const std::string &defaultLayerName,
        GDALDataset *&poDstDS, bool &bTemporaryFile,
        std::unique_ptr<GDALDataset> &poNewRetDS, std::string &outputLayerName,
        OGRLayer *&poDstLayer);

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;
    GDALAlgorithm::ProcessGDALGOutputRet ProcessGDALGOutput() override;
    bool CheckSafeForStreamOutput() override;

    CPL_DISALLOW_COPY_ASSIGN(GDALPipelineStepAlgorithm)
};

//! @endcond

#endif
