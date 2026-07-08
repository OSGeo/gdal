/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "reproject" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_reproject.h"

#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "ogrwarpedlayer.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*     GDALVectorReprojectAlgorithm::GDALVectorReprojectAlgorithm()     */
/************************************************************************/

GDALVectorReprojectAlgorithm::GDALVectorReprojectAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddActiveLayerArg(&m_activeLayer);
    AddArg(GDAL_ARG_NAME_INPUT_CRS, 's', _("Input CRS"), &m_srcCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("src-crs")
        .AddHiddenAlias("s_srs");
    AddArg(GDAL_ARG_NAME_OUTPUT_CRS, 'd', _("Output CRS"), &m_dstCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("dst-crs")
        .AddHiddenAlias("t_srs")
        .SetMutualExclusionGroup("output-crs");
    AddArg("like", 0, _("Dataset from which an output CRS should be extracted"),
           &m_likeDataset, GDAL_OF_RASTER | GDAL_OF_VECTOR)
        .SetMetaVar("DATASET")
        .SetMutualExclusionGroup("output-crs");
    AddArg("like-layer", 0, ("Name of the layer of the 'like' dataset"),
           &m_likeLayer)
        .SetMetaVar("LAYER-NAME");
}

/************************************************************************/
/*               GDALVectorReprojectAlgorithm::RunStep()                */
/************************************************************************/

bool GDALVectorReprojectAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    std::unique_ptr<OGRSpatialReference> poSrcCRS;
    if (!m_srcCrs.empty())
    {
        poSrcCRS = std::make_unique<OGRSpatialReference>();
        poSrcCRS->SetFromUserInput(m_srcCrs.c_str());
        poSrcCRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    OGRSpatialReference oDstCRS;
    if (!m_dstCrs.empty())
    {
        oDstCRS.SetFromUserInput(m_dstCrs.c_str());
        oDstCRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    else if (auto *poLikeDS = m_likeDataset.GetDatasetRef())
    {
        if (!m_likeLayer.empty())
        {
            const auto *poLikeLayer =
                poLikeDS->GetLayerByName(m_likeLayer.c_str());
            if (!poLikeLayer)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Specified layer '%s' not found.",
                            m_likeLayer.c_str());
                return false;
            }
            if (!poLikeLayer->GetSpatialRef())
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "Specified layer '%s' has no spatial reference system.",
                    m_likeLayer.c_str());
                return false;
            }

            oDstCRS = *poLikeLayer->GetSpatialRef();
        }
        else
        {
            const auto *poLikeCRS = poLikeDS->GetSpatialRef();

            if (!poLikeCRS)
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "Dataset specified by --like has no spatial reference "
                    "system, or has multiple layers with different spatial "
                    "reference systems. An individual layer can be specified "
                    "with --like-layer.");
                return false;
            }
            oDstCRS = *poLikeCRS;
        }
    }
    else
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Must specify --output-crs or --like");
        return false;
    }

    auto reprojectedDataset =
        std::make_unique<GDALVectorPipelineOutputDataset>(*poSrcDS);

    const int nLayerCount = poSrcDS->GetLayerCount();
    bool ret = true;
    for (int i = 0; ret && i < nLayerCount; ++i)
    {
        auto poSrcLayer = poSrcDS->GetLayer(i);
        ret = (poSrcLayer != nullptr);
        if (ret)
        {
            if ((m_activeLayer.empty() &&
                 poSrcLayer->GetGeomType() != wkbNone) ||
                m_activeLayer == poSrcLayer->GetDescription())
            {
                const OGRSpatialReference *poSrcLayerCRS;
                if (poSrcCRS)
                    poSrcLayerCRS = poSrcCRS.get();
                else
                    poSrcLayerCRS = poSrcLayer->GetSpatialRef();
                if (!poSrcLayerCRS)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Layer '%s' has no spatial reference system",
                                poSrcLayer->GetName());
                    return false;
                }
                auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
                    OGRCreateCoordinateTransformation(poSrcLayerCRS, &oDstCRS));
                auto poReversedCT =
                    std::unique_ptr<OGRCoordinateTransformation>(
                        OGRCreateCoordinateTransformation(&oDstCRS,
                                                          poSrcLayerCRS));
                ret = (poCT != nullptr) && (poReversedCT != nullptr);
                if (ret)
                {
                    reprojectedDataset->AddLayer(
                        *poSrcLayer,
                        std::make_unique<OGRWarpedLayer>(
                            poSrcLayer, /* iGeomField = */ 0,
                            /*bTakeOwnership = */ false, std::move(poCT),
                            std::move(poReversedCT)));
                }
            }
            else
            {
                reprojectedDataset->AddLayer(
                    *poSrcLayer,
                    std::make_unique<GDALVectorPipelinePassthroughLayer>(
                        *poSrcLayer));
            }
        }
    }

    if (ret)
        m_outputDataset.Set(std::move(reprojectedDataset));

    return ret;
}

GDALVectorReprojectAlgorithmStandalone::
    ~GDALVectorReprojectAlgorithmStandalone() = default;

//! @endcond
