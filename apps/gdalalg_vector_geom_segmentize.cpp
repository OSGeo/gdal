/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector geom segmentize"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_geom_segmentize.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                 GDALVectorGeomSegmentizeAlgorithm()                  */
/************************************************************************/

GDALVectorGeomSegmentizeAlgorithm::GDALVectorGeomSegmentizeAlgorithm(
    bool standaloneStep)
    : GDALVectorGeomAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep, m_opts)
{
    AddArg("max-length", 0, _("Maximum length of a segment"),
           &m_opts.m_maxLength)
        .SetPositional()
        .SetRequired()
        .SetMinValueExcluded(0);
}

namespace
{

/************************************************************************/
/*                  GDALVectorGeomSegmentizeAlgorithmLayer              */
/************************************************************************/

class GDALVectorGeomSegmentizeAlgorithmLayer final
    : public GDALVectorGeomOneToOneAlgorithmLayer<
          GDALVectorGeomSegmentizeAlgorithm>
{
  protected:
    using GDALVectorGeomOneToOneAlgorithmLayer::TranslateFeature;

    std::unique_ptr<OGRFeature>
    TranslateFeature(std::unique_ptr<OGRFeature> poSrcFeature) const override;

  public:
    GDALVectorGeomSegmentizeAlgorithmLayer(
        OGRLayer &oSrcLayer,
        const GDALVectorGeomSegmentizeAlgorithm::Options &opts)
        : GDALVectorGeomOneToOneAlgorithmLayer<
              GDALVectorGeomSegmentizeAlgorithm>(oSrcLayer, opts)
    {
    }
};

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

std::unique_ptr<OGRFeature>
GDALVectorGeomSegmentizeAlgorithmLayer::TranslateFeature(
    std::unique_ptr<OGRFeature> poSrcFeature) const
{
    const int nGeomFieldCount = poSrcFeature->GetGeomFieldCount();
    for (int i = 0; i < nGeomFieldCount; ++i)
    {
        if (IsSelectedGeomField(i))
        {
            if (auto poGeom = poSrcFeature->GetGeomFieldRef(i))
            {
                poGeom->segmentize(m_opts.m_maxLength);
            }
        }
    }

    return poSrcFeature;
}

}  // namespace

/************************************************************************/
/*           GDALVectorGeomSegmentizeAlgorithm::CreateAlgLayer()        */
/************************************************************************/

std::unique_ptr<OGRLayerWithTranslateFeature>
GDALVectorGeomSegmentizeAlgorithm::CreateAlgLayer(OGRLayer &srcLayer)
{
    return std::make_unique<GDALVectorGeomSegmentizeAlgorithmLayer>(srcLayer,
                                                                    m_opts);
}

//! @endcond
