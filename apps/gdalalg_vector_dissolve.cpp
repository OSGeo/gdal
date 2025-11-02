/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector dissolve"
 * Author:   Dan Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_dissolve.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                    GDALVectorDissolveAlgorithm()                       */
/************************************************************************/

GDALVectorDissolveAlgorithm::GDALVectorDissolveAlgorithm(bool standaloneStep)
    : GDALVectorGeomAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep, m_opts)
{
}

#ifdef HAVE_GEOS

namespace
{

/************************************************************************/
/*                   GDALVectorDissolveAlgorithmLayer                   */
/************************************************************************/

class GDALVectorDissolveAlgorithmLayer final
    : public GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorDissolveAlgorithm>
{
  public:
    GDALVectorDissolveAlgorithmLayer(
        OGRLayer &oSrcLayer, const GDALVectorDissolveAlgorithm::Options &opts)
        : GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorDissolveAlgorithm>(
              oSrcLayer, opts)
    {
    }

  protected:
    using GDALVectorGeomOneToOneAlgorithmLayer::TranslateFeature;

    std::unique_ptr<OGRFeature>
    TranslateFeature(std::unique_ptr<OGRFeature> poSrcFeature) const override;

  private:
};

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

std::unique_ptr<OGRFeature> GDALVectorDissolveAlgorithmLayer::TranslateFeature(
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
                poGeom.reset(poGeom->UnaryUnion());

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
/*            GDALVectorDissolveAlgorithm::CreateAlgLayer()             */
/************************************************************************/

std::unique_ptr<OGRLayerWithTranslateFeature>
GDALVectorDissolveAlgorithm::CreateAlgLayer([[maybe_unused]] OGRLayer &srcLayer)
{
#ifdef HAVE_GEOS
    return std::make_unique<GDALVectorDissolveAlgorithmLayer>(srcLayer, m_opts);
#else
    CPLAssert(false);
    return nullptr;
#endif
}

/************************************************************************/
/*                  GDALVectorDissolveAlgorithm::RunStep()                */
/************************************************************************/

bool GDALVectorDissolveAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
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

GDALVectorDissolveAlgorithmStandalone::
    ~GDALVectorDissolveAlgorithmStandalone() = default;

//! @endcond
