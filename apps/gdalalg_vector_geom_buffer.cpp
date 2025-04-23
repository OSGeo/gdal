/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector geom buffer"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_geom_buffer.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                  GDALVectorGeomBufferAlgorithm()                     */
/************************************************************************/

GDALVectorGeomBufferAlgorithm::GDALVectorGeomBufferAlgorithm(
    bool standaloneStep)
    : GDALVectorGeomAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep, m_opts)
{
    AddArg("distance", 0, _("Distance to which to extend the geometry."),
           &m_opts.m_distance)
        .SetPositional()
        .SetRequired();
    AddArg("endcap-style", 0, _("Endcap style."), &m_opts.m_endCapStyle)
        .SetChoices("round", "flat", "square")
        .SetDefault(m_opts.m_endCapStyle);
    AddArg("join-style", 0, _("Join style."), &m_opts.m_joinStyle)
        .SetChoices("round", "mitre", "bevel")
        .SetDefault(m_opts.m_joinStyle);
    AddArg("mitre-limit", 0,
           _("Mitre ratio limit (only affects mitered join style)."),
           &m_opts.m_mitreLimit)
        .SetDefault(m_opts.m_mitreLimit)
        .SetMinValueIncluded(0);
    AddArg("quadrant-segments", 0,
           _("Number of line segments used to approximate a quarter circle."),
           &m_opts.m_quadrantSegments)
        .SetDefault(m_opts.m_quadrantSegments)
        .SetMinValueIncluded(1);
    AddArg("side", 0,
           _("Sets whether the computed buffer should be single-sided or not."),
           &m_opts.m_side)
        .SetChoices("both", "left", "right")
        .SetDefault(m_opts.m_side);
}

#ifdef HAVE_GEOS

namespace
{

/************************************************************************/
/*                   GDALVectorGeomBufferAlgorithmLayer                 */
/************************************************************************/

class GDALVectorGeomBufferAlgorithmLayer final
    : public GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorGeomBufferAlgorithm>
{
  public:
    GDALVectorGeomBufferAlgorithmLayer(
        OGRLayer &oSrcLayer, const GDALVectorGeomBufferAlgorithm::Options &opts)
        : GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorGeomBufferAlgorithm>(
              oSrcLayer, opts)
    {
        m_aosBufferOptions.SetNameValue("ENDCAP_STYLE",
                                        opts.m_endCapStyle.c_str());
        m_aosBufferOptions.SetNameValue("JOIN_STYLE", opts.m_joinStyle.c_str());
        m_aosBufferOptions.SetNameValue("MITRE_LIMIT",
                                        CPLSPrintf("%.17g", opts.m_mitreLimit));
        m_aosBufferOptions.SetNameValue(
            "QUADRANT_SEGMENTS", CPLSPrintf("%d", opts.m_quadrantSegments));
        m_aosBufferOptions.SetNameValue("SINGLE_SIDED",
                                        m_opts.m_side != "both" ? "YES" : "NO");
    }

  protected:
    using GDALVectorGeomOneToOneAlgorithmLayer::TranslateFeature;

    std::unique_ptr<OGRFeature>
    TranslateFeature(std::unique_ptr<OGRFeature> poSrcFeature) const override;

  private:
    CPLStringList m_aosBufferOptions{};
};

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

std::unique_ptr<OGRFeature>
GDALVectorGeomBufferAlgorithmLayer::TranslateFeature(
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
                poGeom.reset(poGeom->BufferEx(m_opts.m_distance,
                                              m_aosBufferOptions.List()));
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
/*            GDALVectorGeomBufferAlgorithm::CreateAlgLayer()           */
/************************************************************************/

std::unique_ptr<OGRLayerWithTranslateFeature>
GDALVectorGeomBufferAlgorithm::CreateAlgLayer(
    [[maybe_unused]] OGRLayer &srcLayer)
{
#ifdef HAVE_GEOS
    return std::make_unique<GDALVectorGeomBufferAlgorithmLayer>(srcLayer,
                                                                m_opts);
#else
    CPLAssert(false);
    return nullptr;
#endif
}

/************************************************************************/
/*                GDALVectorGeomBufferAlgorithm::RunStep()              */
/************************************************************************/

bool GDALVectorGeomBufferAlgorithm::RunStep(GDALProgressFunc, void *)
{
#ifdef HAVE_GEOS
    if (m_opts.m_side == "right")
        m_opts.m_distance = -m_opts.m_distance;

    return GDALVectorGeomAbstractAlgorithm::RunStep(nullptr, nullptr);
#else
    ReportError(CE_Failure, CPLE_NotSupported,
                "This algorithm is only supported for builds against GEOS");
    return false;
#endif
}

//! @endcond
