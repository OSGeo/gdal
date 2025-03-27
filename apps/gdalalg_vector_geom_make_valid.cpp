/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector geom make-valid"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_geom_make_valid.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                 GDALVectorGeomMakeValidAlgorithm()                   */
/************************************************************************/

GDALVectorGeomMakeValidAlgorithm::GDALVectorGeomMakeValidAlgorithm(
    bool standaloneStep)
    : GDALVectorGeomAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep, m_opts)
{
    AddArg("keep-lower-dim", 0,
           _("Keep components of lower dimension after MakeValid()"),
           &m_opts.m_keepLowerDim);
}

#ifdef HAVE_GEOS

namespace
{

/************************************************************************/
/*                  GDALVectorGeomMakeValidAlgorithmLayer               */
/************************************************************************/

class GDALVectorGeomMakeValidAlgorithmLayer final
    : public GDALVectorGeomOneToOneAlgorithmLayer<
          GDALVectorGeomMakeValidAlgorithm>
{
  protected:
    using GDALVectorGeomOneToOneAlgorithmLayer::TranslateFeature;

    std::unique_ptr<OGRFeature>
    TranslateFeature(std::unique_ptr<OGRFeature> poSrcFeature) const override;

  public:
    GDALVectorGeomMakeValidAlgorithmLayer(
        OGRLayer &oSrcLayer,
        const GDALVectorGeomMakeValidAlgorithm::Options &opts)
        : GDALVectorGeomOneToOneAlgorithmLayer<
              GDALVectorGeomMakeValidAlgorithm>(oSrcLayer, opts)
    {
    }
};

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

std::unique_ptr<OGRFeature>
GDALVectorGeomMakeValidAlgorithmLayer::TranslateFeature(
    std::unique_ptr<OGRFeature> poSrcFeature) const
{
    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
    const int nGeomFieldCount = poSrcFeature->GetGeomFieldCount();
    for (int i = 0; i < nGeomFieldCount; ++i)
    {
        if (IsSelectedGeomField(i))
        {
            auto poGeom =
                std::unique_ptr<OGRGeometry>(poSrcFeature->StealGeometry(i));
            if (poGeom && poGeom->getCoordinateDimension() == 2 &&
                !poGeom->IsValid())
            {
                const bool bIsGeomCollection =
                    wkbFlatten(poGeom->getGeometryType()) ==
                    wkbGeometryCollection;
                poGeom.reset(poGeom->MakeValid());
                if (poGeom)
                {
                    if (!bIsGeomCollection && !m_opts.m_keepLowerDim)
                    {
                        poGeom.reset(
                            OGRGeometryFactory::removeLowerDimensionSubGeoms(
                                poGeom.get()));
                    }
                    poGeom->assignSpatialReference(m_srcLayer.GetLayerDefn()
                                                       ->GetGeomFieldDefn(i)
                                                       ->GetSpatialRef());
                }
            }
            if (poGeom)
            {
                poSrcFeature->SetGeomField(i, std::move(poGeom));
            }
        }
    }

    return poSrcFeature;
}

}  // namespace

#endif  // HAVE_GEOS

/************************************************************************/
/*          GDALVectorGeomMakeValidAlgorithm::CreateAlgLayer()          */
/************************************************************************/

std::unique_ptr<OGRLayerWithTranslateFeature>
GDALVectorGeomMakeValidAlgorithm::CreateAlgLayer(
    [[maybe_unused]] OGRLayer &srcLayer)
{
#ifdef HAVE_GEOS
    return std::make_unique<GDALVectorGeomMakeValidAlgorithmLayer>(srcLayer,
                                                                   m_opts);
#else
    CPLAssert(false);
    return nullptr;
#endif
}

/************************************************************************/
/*                GDALVectorGeomMakeValidAlgorithm::RunStep()           */
/************************************************************************/

bool GDALVectorGeomMakeValidAlgorithm::RunStep(GDALProgressFunc, void *)
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
