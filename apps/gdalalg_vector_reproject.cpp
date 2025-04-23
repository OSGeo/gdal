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
    AddActiveLayerArg(&m_activeLayer);
    AddArg("src-crs", 's', _("Source CRS"), &m_srsCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("s_srs");
    AddArg("dst-crs", 'd', _("Destination CRS"), &m_dstCrs)
        .SetIsCRSArg()
        .SetRequired()
        .AddHiddenAlias("t_srs");
}

/************************************************************************/
/*            GDALVectorReprojectAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALVectorReprojectAlgorithm::RunStep(GDALProgressFunc, void *)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    std::unique_ptr<OGRSpatialReference> poSrcCRS;
    if (!m_srsCrs.empty())
    {
        poSrcCRS = std::make_unique<OGRSpatialReference>();
        poSrcCRS->SetFromUserInput(m_srsCrs.c_str());
        poSrcCRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    OGRSpatialReference oDstCRS;
    oDstCRS.SetFromUserInput(m_dstCrs.c_str());
    oDstCRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

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
            if (m_activeLayer.empty() ||
                m_activeLayer == poSrcLayer->GetDescription())
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
                            /*bTakeOwnership = */ false, poCT.release(),
                            poReversedCT.release()));
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

//! @endcond
