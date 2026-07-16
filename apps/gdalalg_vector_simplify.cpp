/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector simplify"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_simplify.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                    GDALVectorSimplifyAlgorithm()                     */
/************************************************************************/

GDALVectorSimplifyAlgorithm::GDALVectorSimplifyAlgorithm(bool standaloneStep)
    : GDALVectorGeomAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep, m_opts)
{
    AddArg("tolerance", 0, _("Distance tolerance for simplification."),
           &m_opts.m_tolerance)
        .SetPositional()
        .SetRequired()
        .SetMinValueIncluded(0);
}

#ifdef HAVE_GEOS

namespace
{

/************************************************************************/
/*                   GDALVectorSimplifyAlgorithmLayer                   */
/************************************************************************/

class GDALVectorSimplifyAlgorithmLayer final
    : public GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorSimplifyAlgorithm>
{
  protected:
    using GDALVectorGeomOneToOneAlgorithmLayer::TranslateFeature;

    std::unique_ptr<OGRFeature>
    TranslateFeature(std::unique_ptr<OGRFeature> poSrcFeature) const override;

  public:
    GDALVectorSimplifyAlgorithmLayer(
        OGRLayer &oSrcLayer, const GDALVectorSimplifyAlgorithm::Options &opts)
        : GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorSimplifyAlgorithm>(
              oSrcLayer, opts)
    {
    }
};

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

std::unique_ptr<OGRFeature> GDALVectorSimplifyAlgorithmLayer::TranslateFeature(
    std::unique_ptr<OGRFeature> poSrcFeature) const
{
    const int nGeomFieldCount = poSrcFeature->GetGeomFieldCount();
    for (int i = 0; i < nGeomFieldCount; ++i)
    {
        if (IsSelectedGeomField(i))
        {
            if (auto poGeom = std::unique_ptr<OGRGeometry>(
                    poSrcFeature->StealGeometry(i)))
            {
                poGeom.reset(
                    poGeom->SimplifyPreserveTopology(m_opts.m_tolerance));
                if (poGeom)
                {
                    poGeom->assignSpatialReference(m_srcLayer.GetLayerDefn()
                                                       ->GetGeomFieldDefn(i)
                                                       ->GetSpatialRef());
                    poSrcFeature->SetGeomField(i, std::move(poGeom));
                }
            }
        }
    }

    return poSrcFeature;
}

}  // namespace

#endif  // HAVE_GEOS

/************************************************************************/
/*            GDALVectorSimplifyAlgorithm::CreateAlgLayer()             */
/************************************************************************/

std::unique_ptr<OGRLayerWithTranslateFeature>
GDALVectorSimplifyAlgorithm::CreateAlgLayer([[maybe_unused]] OGRLayer &srcLayer)
{
#ifdef HAVE_GEOS
    return std::make_unique<GDALVectorSimplifyAlgorithmLayer>(srcLayer, m_opts);
#else
    CPLAssert(false);
    return nullptr;
#endif
}

/************************************************************************/
/*                GDALVectorSimplifyAlgorithm::RunStep()                */
/************************************************************************/

bool GDALVectorSimplifyAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
#ifdef HAVE_GEOS
    return GDALVectorGeomAbstractAlgorithm::RunStep(ctxt);
#else
    (void)ctxt;
    ReportError(CE_Failure, CPLE_NotSupported,
                "This algorithm is only supported for builds against GEOS");
    return false;
#endif
}

GDALVectorSimplifyAlgorithmStandalone::
    ~GDALVectorSimplifyAlgorithmStandalone() = default;

//! @endcond
