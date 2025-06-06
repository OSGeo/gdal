/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "geom" step of "vector pipeline", or "gdal vector geom" standalone
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_geom.h"
#include "gdalalg_vector_set_geom_type.h"
#include "gdalalg_vector_explode_collections.h"
#include "gdalalg_vector_make_valid.h"
#include "gdalalg_vector_segmentize.h"
#include "gdalalg_vector_simplify.h"
#include "gdalalg_vector_buffer.h"
#include "gdalalg_vector_swap_xy.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*           GDALVectorGeomAlgorithm::GDALVectorGeomAlgorithm()         */
/************************************************************************/

GDALVectorGeomAlgorithm::GDALVectorGeomAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      /* standaloneStep = */ false)
{
    m_hidden = true;

    RegisterSubAlgorithm<GDALVectorSetGeomTypeAlgorithm>(standaloneStep);
    RegisterSubAlgorithm<GDALVectorExplodeCollectionsAlgorithm>(standaloneStep);
    RegisterSubAlgorithm<GDALVectorMakeValidAlgorithm>(standaloneStep);
    RegisterSubAlgorithm<GDALVectorSegmentizeAlgorithm>(standaloneStep);
    RegisterSubAlgorithm<GDALVectorSimplifyAlgorithm>(standaloneStep);
    RegisterSubAlgorithm<GDALVectorBufferAlgorithm>(standaloneStep);
    RegisterSubAlgorithm<GDALVectorSwapXYAlgorithm>(standaloneStep);
}

/************************************************************************/
/*              GDALVectorGeomAlgorithm::WarnIfDeprecated()             */
/************************************************************************/

void GDALVectorGeomAlgorithm::WarnIfDeprecated()
{
    ReportError(CE_Warning, CPLE_AppDefined,
                "'gdal vector geom' is deprecated in GDAL 3.12, and will be "
                "removed in GDAL 3.13. Is subcommands are directly available "
                "under 'gdal vector'");
}

/************************************************************************/
/*                GDALVectorGeomAlgorithm::RunStep()                    */
/************************************************************************/

bool GDALVectorGeomAlgorithm::RunStep(GDALVectorPipelineStepRunContext &)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "The Run() method should not be called directly on the \"gdal "
             "vector geom\" program.");
    return false;
}

/************************************************************************/
/*                 GDALVectorGeomAbstractAlgorithm()                    */
/************************************************************************/

GDALVectorGeomAbstractAlgorithm::GDALVectorGeomAbstractAlgorithm(
    const std::string &name, const std::string &description,
    const std::string &helpURL, bool standaloneStep, OptionsBase &opts)
    : GDALVectorPipelineStepAlgorithm(name, description, helpURL,
                                      standaloneStep),
      m_activeLayer(opts.m_activeLayer)
{
    AddActiveLayerArg(&opts.m_activeLayer);
    AddArg("active-geometry", 0,
           _("Geometry field name to which to restrict the processing (if not "
             "specified, all)"),
           &opts.m_geomField);
}

/************************************************************************/
/*               GDALVectorGeomAbstractAlgorithm::RunStep()             */
/************************************************************************/

bool GDALVectorGeomAbstractAlgorithm::RunStep(
    GDALVectorPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    auto outDS = std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);

    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_activeLayer.empty() ||
            m_activeLayer == poSrcLayer->GetDescription())
        {
            outDS->AddLayer(*poSrcLayer, CreateAlgLayer(*poSrcLayer));
        }
        else
        {
            outDS->AddLayer(
                *poSrcLayer,
                std::make_unique<GDALVectorPipelinePassthroughLayer>(
                    *poSrcLayer));
        }
    }

    m_outputDataset.Set(std::move(outDS));

    return true;
}

GDALVectorGeomAlgorithmStandalone::~GDALVectorGeomAlgorithmStandalone() =
    default;

//! @endcond
