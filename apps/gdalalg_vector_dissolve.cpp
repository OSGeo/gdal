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

#include <cinttypes>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                    GDALVectorDissolveAlgorithm()                     */
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

std::unique_ptr<OGRGeometry> LineMerge(const OGRMultiLineString *poGeom)
{
    GEOSContextHandle_t hContext = OGRGeometry::createGEOSContext();
    GEOSGeometry *hGeosGeom = poGeom->exportToGEOS(hContext);
    if (!hGeosGeom)
    {
        OGRGeometry::freeGEOSContext(hContext);
        return nullptr;
    }

    GEOSGeometry *hGeosResult = GEOSLineMerge_r(hContext, hGeosGeom);
    GEOSGeom_destroy_r(hContext, hGeosGeom);

    if (!hGeosResult)
    {
        OGRGeometry::freeGEOSContext(hContext);
        return nullptr;
    }

    std::unique_ptr<OGRGeometry> ret(
        OGRGeometryFactory::createFromGEOS(hContext, hGeosResult));
    GEOSGeom_destroy_r(hContext, hGeosResult);
    OGRGeometry::freeGEOSContext(hContext);

    if (ret)
    {
        const auto eRetType = wkbFlatten(ret->getGeometryType());
        if (eRetType != wkbLineString && eRetType != wkbMultiLineString)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "LineMerge returned a geometry of type %s, expected "
                     "LineString or MultiLineString",
                     OGRGeometryTypeToName(eRetType));
            return nullptr;
        }
    }

    return ret;
}

std::unique_ptr<OGRFeature> GDALVectorDissolveAlgorithmLayer::TranslateFeature(
    std::unique_ptr<OGRFeature> poSrcFeature) const
{
    const int nGeomFieldCount = poSrcFeature->GetGeomFieldCount();
    for (int iGeomField = 0; iGeomField < nGeomFieldCount; ++iGeomField)
    {
        if (IsSelectedGeomField(iGeomField))
        {
            if (auto poGeom = std::unique_ptr<OGRGeometry>(
                    poSrcFeature->StealGeometry(iGeomField)))
            {
                poGeom.reset(poGeom->UnaryUnion());
                if (!poGeom)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Failed to perform union of geometry on feature "
                             "%" PRId64,
                             static_cast<int64_t>(poSrcFeature->GetFID()));
                    return nullptr;
                }

                const auto eResultType = wkbFlatten(poGeom->getGeometryType());

                if (eResultType == wkbMultiLineString)
                {
                    poGeom = LineMerge(poGeom->toMultiLineString());
                    if (!poGeom)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Failed to merge lines of feature %" PRId64,
                                 static_cast<int64_t>(poSrcFeature->GetFID()));
                        return nullptr;
                    }
                }
                else if (eResultType == wkbGeometryCollection)
                {
                    OGRGeometryCollection *poColl =
                        poGeom->toGeometryCollection();

                    OGRMultiLineString oMLS;

                    const auto nGeoms = poColl->getNumGeometries();
                    for (int i = nGeoms - 1; i >= 0; i--)
                    {
                        const auto eComponentType = wkbFlatten(
                            poColl->getGeometryRef(i)->getGeometryType());
                        if (eComponentType == wkbLineString)
                        {
                            oMLS.addGeometryDirectly(poColl->stealGeometry(i)
                                                         .release()
                                                         ->toLineString());
                        }
                    }

                    if (oMLS.getNumGeometries() > 0)
                    {
                        std::unique_ptr<OGRGeometry> poMerged =
                            LineMerge(&oMLS);
                        if (!poMerged)
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "Failed to merge lines of feature %" PRId64,
                                static_cast<int64_t>(poSrcFeature->GetFID()));
                            return nullptr;
                        }

                        const auto eMergedType =
                            wkbFlatten(poMerged->getGeometryType());
                        if (eMergedType == wkbLineString)
                        {
                            poColl->addGeometry(std::move(poMerged));
                        }
                        else  // eMergedType == wkbMultiLineString
                        {
                            OGRMultiLineString *poMergedMLS =
                                poMerged->toMultiLineString();
                            const auto nMergedGeoms =
                                poMergedMLS->getNumGeometries();
                            for (int i = nMergedGeoms - 1; i >= 0; i--)
                            {
                                poColl->addGeometryDirectly(
                                    poMergedMLS->stealGeometry(i)
                                        .release()
                                        ->toLineString());
                            }
                        }
                    }
                }

                if (poGeom)
                {
                    poGeom->assignSpatialReference(
                        m_srcLayer.GetLayerDefn()
                            ->GetGeomFieldDefn(iGeomField)
                            ->GetSpatialRef());
                    poSrcFeature->SetGeomField(iGeomField, std::move(poGeom));
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
/*                GDALVectorDissolveAlgorithm::RunStep()                */
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
