/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector segmentize"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_segmentize.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                   GDALVectorSegmentizeAlgorithm()                    */
/************************************************************************/

GDALVectorSegmentizeAlgorithm::GDALVectorSegmentizeAlgorithm(
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
/*                  GDALVectorSegmentizeAlgorithmLayer                  */
/************************************************************************/

class GDALVectorSegmentizeAlgorithmLayer final
    : public GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorSegmentizeAlgorithm>
{
  protected:
    using GDALVectorGeomOneToOneAlgorithmLayer::TranslateFeature;

    std::unique_ptr<OGRFeature>
    TranslateFeature(std::unique_ptr<OGRFeature> poSrcFeature) const override;

  public:
    GDALVectorSegmentizeAlgorithmLayer(
        OGRLayer &oSrcLayer, const GDALVectorSegmentizeAlgorithm::Options &opts)
        : GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorSegmentizeAlgorithm>(
              oSrcLayer, opts)
    {
    }
};

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

std::unique_ptr<OGRFeature>
GDALVectorSegmentizeAlgorithmLayer::TranslateFeature(
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
/*           GDALVectorSegmentizeAlgorithm::CreateAlgLayer()            */
/************************************************************************/

std::unique_ptr<OGRLayerWithTranslateFeature>
GDALVectorSegmentizeAlgorithm::CreateAlgLayer(OGRLayer &srcLayer)
{
    return std::make_unique<GDALVectorSegmentizeAlgorithmLayer>(srcLayer,
                                                                m_opts);
}

GDALVectorSegmentizeAlgorithmStandalone::
    ~GDALVectorSegmentizeAlgorithmStandalone() = default;

//! @endcond
