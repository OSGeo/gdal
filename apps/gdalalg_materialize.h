/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "materialize" pipeline step
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MATERIALIZE_INCLUDED
#define GDALALG_MATERIALIZE_INCLUDED

#include "gdalalg_abstract_pipeline.h"
#include "gdalalg_raster_pipeline.h"
#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALMaterializeStepAlgorithm                     */
/************************************************************************/

template <class BaseStepAlgorithm, int nDatasetType>
class GDALMaterializeStepAlgorithm /* non final */
    : public BaseStepAlgorithm
{
  public:
    static constexpr const char *NAME = "materialize";
    static constexpr const char *DESCRIPTION =
        "Materialize a piped dataset on disk to increase the efficiency of the "
        "following steps.";

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

    bool GeneratesFilesFromUserInput() const override
    {
        return !this->m_outputDataset.GetName().empty();
    }

  protected:
    explicit GDALMaterializeStepAlgorithm(const char *helpURL);

    int GetInputType() const override
    {
        return nDatasetType;
    }

    int GetOutputType() const override
    {
        return nDatasetType;
    }
};

/************************************************************************/
/*     GDALMaterializeStepAlgorithm::GDALMaterializeStepAlgorithm()     */
/************************************************************************/

template <class BaseStepAlgorithm, int nDatasetType>
GDALMaterializeStepAlgorithm<BaseStepAlgorithm, nDatasetType>::
    GDALMaterializeStepAlgorithm(const char *helpURL)
    : BaseStepAlgorithm(NAME, DESCRIPTION, helpURL,
                        GDALPipelineStepAlgorithm::ConstructorOptions()
                            .SetAddDefaultArguments(false))
{
}

/************************************************************************/
/*                    GDALMaterializeRasterAlgorithm                    */
/************************************************************************/

class GDALMaterializeRasterAlgorithm final
    : public GDALMaterializeStepAlgorithm<GDALRasterPipelineStepAlgorithm,
                                          GDAL_OF_RASTER>
{
  public:
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_materialize.html";

    GDALMaterializeRasterAlgorithm();

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
};

/************************************************************************/
/*                    GDALMaterializeVectorAlgorithm                    */
/************************************************************************/

class GDALMaterializeVectorAlgorithm final
    : public GDALMaterializeStepAlgorithm<GDALVectorPipelineStepAlgorithm,
                                          GDAL_OF_VECTOR>
{
  public:
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_materialize.html";

    GDALMaterializeVectorAlgorithm();

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
};

//! @endcond

#endif
