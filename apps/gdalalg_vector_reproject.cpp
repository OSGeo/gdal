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
/*      GDALVectorReprojectAlgorithm::GDALVectorReprojectAlgorithm()    */
/************************************************************************/

GDALVectorReprojectAlgorithm::GDALVectorReprojectAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("src-crs", 's', _("Source CRS"), &m_srsCrs).AddHiddenAlias("s_srs");
    AddArg("dst-crs", 'd', _("Destination CRS"), &m_dstCrs)
        .SetRequired()
        .AddHiddenAlias("t_srs");
}

/************************************************************************/
/*               GDALVectorReprojectAlgorithmDataset                    */
/************************************************************************/

namespace
{
class GDALVectorReprojectAlgorithmDataset final : public GDALDataset
{
    std::vector<std::unique_ptr<OGRLayer>> m_layers{};

  public:
    GDALVectorReprojectAlgorithmDataset() = default;

    void AddLayer(std::unique_ptr<OGRLayer> poLayer)
    {
        m_layers.push_back(std::move(poLayer));
    }

    int GetLayerCount() override
    {
        return static_cast<int>(m_layers.size());
    }

    OGRLayer *GetLayer(int idx) override
    {
        return idx >= 0 && idx < GetLayerCount() ? m_layers[idx].get()
                                                 : nullptr;
    }
};
}  // namespace

/************************************************************************/
/*            GDALVectorReprojectAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALVectorReprojectAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    std::unique_ptr<OGRSpatialReference> poSrcCRS;
    if (!m_srsCrs.empty())
    {
        poSrcCRS = std::make_unique<OGRSpatialReference>();
        if (poSrcCRS->SetFromUserInput(m_srsCrs.c_str()) != OGRERR_NONE)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Invalid value for '--src-crs'");
            return false;
        }
        poSrcCRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    OGRSpatialReference oDstCRS;
    if (oDstCRS.SetFromUserInput(m_dstCrs.c_str()) != OGRERR_NONE)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Invalid value for '--dst-crs'");
        return false;
    }
    oDstCRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    auto poSrcDS = m_inputDataset.GetDatasetRef();

    auto reprojectedDataset =
        std::make_unique<GDALVectorReprojectAlgorithmDataset>();
    reprojectedDataset->SetDescription(poSrcDS->GetDescription());

    const int nLayerCount = poSrcDS->GetLayerCount();
    bool ret = true;
    for (int i = 0; ret && i < nLayerCount; ++i)
    {
        auto poSrcLayer = poSrcDS->GetLayer(i);
        ret = (poSrcLayer != nullptr);
        if (ret)
        {
            OGRSpatialReference *poSrcLayerCRS;
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
            auto poReversedCT = std::unique_ptr<OGRCoordinateTransformation>(
                OGRCreateCoordinateTransformation(&oDstCRS, poSrcLayerCRS));
            ret = (poCT != nullptr) && (poReversedCT != nullptr);
            if (ret)
            {
                reprojectedDataset->AddLayer(std::make_unique<OGRWarpedLayer>(
                    poSrcLayer, /* iGeomField = */ 0,
                    /*bTakeOwnership = */ false, poCT.release(),
                    poReversedCT.release()));
            }
        }
    }

    if (ret)
        m_outputDataset.Set(std::move(reprojectedDataset));

    return ret;
}

//! @endcond
