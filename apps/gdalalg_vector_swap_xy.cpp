/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector swap-xy"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_swap_xy.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                     GDALVectorSwapXYAlgorithm()                      */
/************************************************************************/

GDALVectorSwapXYAlgorithm::GDALVectorSwapXYAlgorithm(bool standaloneStep)
    : GDALVectorGeomAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep, m_opts)
{
}

namespace
{

/************************************************************************/
/*                    GDALVectorSwapXYAlgorithmLayer                    */
/************************************************************************/

class GDALVectorSwapXYAlgorithmLayer final
    : public GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorSwapXYAlgorithm>
{
  public:
    GDALVectorSwapXYAlgorithmLayer(
        OGRLayer &oSrcLayer, const GDALVectorSwapXYAlgorithm::Options &opts)
        : GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorSwapXYAlgorithm>(
              oSrcLayer, opts)
    {
    }

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override
    {
        OGRErr eErr = m_srcLayer.GetExtent(iGeomField, psExtent, bForce);
        if (eErr == CE_None)
        {
            std::swap(psExtent->MinX, psExtent->MinY);
            std::swap(psExtent->MaxX, psExtent->MaxY);
        }
        return eErr;
    }

  protected:
    using GDALVectorGeomOneToOneAlgorithmLayer::TranslateFeature;

    std::unique_ptr<OGRFeature>
    TranslateFeature(std::unique_ptr<OGRFeature> poSrcFeature) const override;
};

/************************************************************************/
/*                          TranslateFeature()                          */
/************************************************************************/

std::unique_ptr<OGRFeature> GDALVectorSwapXYAlgorithmLayer::TranslateFeature(
    std::unique_ptr<OGRFeature> poSrcFeature) const
{
    const int nGeomFieldCount = poSrcFeature->GetGeomFieldCount();
    for (int i = 0; i < nGeomFieldCount; ++i)
    {
        if (IsSelectedGeomField(i))
        {
            if (auto poGeom = poSrcFeature->GetGeomFieldRef(i))
            {
                poGeom->swapXY();
            }
        }
    }

    return poSrcFeature;
}

}  // namespace

/************************************************************************/
/*             GDALVectorSwapXYAlgorithm::CreateAlgLayer()              */
/************************************************************************/

std::unique_ptr<OGRLayerWithTranslateFeature>
GDALVectorSwapXYAlgorithm::CreateAlgLayer(OGRLayer &srcLayer)
{
    return std::make_unique<GDALVectorSwapXYAlgorithmLayer>(srcLayer, m_opts);
}

GDALVectorSwapXYAlgorithmStandalone::~GDALVectorSwapXYAlgorithmStandalone() =
    default;

//! @endcond
