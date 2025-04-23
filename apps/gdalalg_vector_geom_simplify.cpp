/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector geom simplify"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_geom_simplify.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                 GDALVectorGeomSimplifyAlgorithm()                    */
/************************************************************************/

GDALVectorGeomSimplifyAlgorithm::GDALVectorGeomSimplifyAlgorithm(
    bool standaloneStep)
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
/*                  GDALVectorGeomSimplifyAlgorithmLayer                */
/************************************************************************/

class GDALVectorGeomSimplifyAlgorithmLayer final
    : public GDALVectorGeomOneToOneAlgorithmLayer<
          GDALVectorGeomSimplifyAlgorithm>
{
  protected:
    using GDALVectorGeomOneToOneAlgorithmLayer::TranslateFeature;

    std::unique_ptr<OGRFeature>
    TranslateFeature(std::unique_ptr<OGRFeature> poSrcFeature) const override;

  public:
    GDALVectorGeomSimplifyAlgorithmLayer(
        OGRLayer &oSrcLayer,
        const GDALVectorGeomSimplifyAlgorithm::Options &opts)
        : GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorGeomSimplifyAlgorithm>(
              oSrcLayer, opts)
    {
    }
};

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

std::unique_ptr<OGRFeature>
GDALVectorGeomSimplifyAlgorithmLayer::TranslateFeature(
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
/*           GDALVectorGeomSimplifyAlgorithm::CreateAlgLayer()          */
/************************************************************************/

std::unique_ptr<OGRLayerWithTranslateFeature>
GDALVectorGeomSimplifyAlgorithm::CreateAlgLayer(
    [[maybe_unused]] OGRLayer &srcLayer)
{
#ifdef HAVE_GEOS
    return std::make_unique<GDALVectorGeomSimplifyAlgorithmLayer>(srcLayer,
                                                                  m_opts);
#else
    CPLAssert(false);
    return nullptr;
#endif
}

/************************************************************************/
/*                GDALVectorGeomSimplifyAlgorithm::RunStep()            */
/************************************************************************/

bool GDALVectorGeomSimplifyAlgorithm::RunStep(GDALProgressFunc, void *)
{
#ifdef HAVE_GEOS
    return GDALVectorGeomAbstractAlgorithm::RunStep(nullptr, nullptr);
#else
    ReportError(CE_Failure, CPLE_NotSupported,
                "This algorithm is only supported for builds against GEOS");
    return false;
#endif
}

//! @endcond
