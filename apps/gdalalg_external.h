/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "external" subcommand (always in pipeline)
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_EXTERNAL_INCLUDED
#define GDALALG_EXTERNAL_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalalg_abstract_pipeline.h"
#include "gdalalg_raster_pipeline.h"
#include "gdalalg_vector_pipeline.h"

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                      GDALExternalAlgorithmBase                       */
/************************************************************************/

class GDALExternalAlgorithmBase /* non final */
{
  protected:
    GDALExternalAlgorithmBase() = default;
    virtual ~GDALExternalAlgorithmBase();

    CPLString m_command{};

    std::string m_osTempInputFilename{};
    std::string m_osTempOutputFilename{};

    bool Run(const std::vector<std::string> &inputFormats,
             std::vector<GDALArgDatasetValue> &inputDataset,
             const std::string &outputFormat,
             GDALArgDatasetValue &outputDataset);
};

/************************************************************************/
/*                        GDALExternalAlgorithm                         */
/************************************************************************/

template <class BaseStepAlgorithm, int nDatasetType>
class GDALExternalAlgorithm /* non final */ : public BaseStepAlgorithm,
                                              public GDALExternalAlgorithmBase
{
  public:
    static constexpr const char *NAME = "external";
    static constexpr const char *DESCRIPTION =
        "Execute an external program as a step of a pipeline";
    static constexpr const char *HELP_URL = "/programs/gdal_external.html";

    GDALExternalAlgorithm()
        : BaseStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                            GDALPipelineStepAlgorithm::ConstructorOptions()
                                .SetAddDefaultArguments(false))
    {
        this->AddArg("command", 0,
                     _("External command, optionally with <INPUT> and/or "
                       "<OUTPUT> or <INPUT-OUTPUT> placeholders"),
                     &m_command)
            .SetRequired()
            .SetPositional()
            .SetMinCharCount(1);

        this->AddInputFormatsArg(&this->m_inputFormats)
            .SetMaxCount(1)
            .AddMetadataItem(GAAMDI_EXCLUDED_FORMATS, {"MEM"});
        this->AddOutputFormatArg(&this->m_format, /* bStreamAllowed = */ false,
                                 /* bGDALGAllowed = */ false)
            .AddMetadataItem(GAAMDI_EXCLUDED_FORMATS, {"MEM"});

        // Hidden
        this->AddInputDatasetArg(&this->m_inputDataset, nDatasetType, false)
            .SetMinCount(0)
            .SetMaxCount(1)
            .SetDatasetInputFlags(GADV_OBJECT)
            .SetHidden();
        this->AddOutputDatasetArg(&this->m_outputDataset, nDatasetType, false)
            .SetDatasetInputFlags(GADV_OBJECT)
            .SetHidden();
    }

    int GetInputType() const override
    {
        return nDatasetType;
    }

    int GetOutputType() const override
    {
        return nDatasetType;
    }

    bool CanBeFirstStep() const override
    {
        return true;
    }

    bool CanBeMiddleStep() const override
    {
        return true;
    }

    bool CanBeLastStep() const override
    {
        return true;
    }

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &) override
    {
        return GDALExternalAlgorithmBase::Run(
            this->m_inputFormats, this->m_inputDataset, this->m_format,
            this->m_outputDataset);
    }
};

/************************************************************************/
/*                 GDALExternalRasterOrVectorAlgorithm                  */
/************************************************************************/

class GDALExternalRasterOrVectorAlgorithm final
    : public GDALExternalAlgorithm<GDALPipelineStepAlgorithm, 0>
{
  public:
    GDALExternalRasterOrVectorAlgorithm() = default;

    ~GDALExternalRasterOrVectorAlgorithm() override;
};

/************************************************************************/
/*                     GDALExternalRasterAlgorithm                      */
/************************************************************************/

class GDALExternalRasterAlgorithm final
    : public GDALExternalAlgorithm<GDALRasterPipelineStepAlgorithm,
                                   GDAL_OF_RASTER>
{
  public:
    GDALExternalRasterAlgorithm() = default;

    ~GDALExternalRasterAlgorithm() override;
};

/************************************************************************/
/*                     GDALExternalVectorAlgorithm                      */
/************************************************************************/

class GDALExternalVectorAlgorithm final
    : public GDALExternalAlgorithm<GDALVectorPipelineStepAlgorithm,
                                   GDAL_OF_VECTOR>
{
  public:
    GDALExternalVectorAlgorithm() = default;

    ~GDALExternalVectorAlgorithm() override;
};

//! @endcond

#endif
