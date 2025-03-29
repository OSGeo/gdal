/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector geom swap-xy"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_geom_swap_xy.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                   GDALVectorGeomSwapXYAlgorithm()                    */
/************************************************************************/

GDALVectorGeomSwapXYAlgorithm::GDALVectorGeomSwapXYAlgorithm(
    bool standaloneStep)
    : GDALVectorGeomAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep, m_opts)
{
}

namespace
{

/************************************************************************/
/*                    GDALVectorGeomSwapXYAlgorithmLayer                */
/************************************************************************/

class GDALVectorGeomSwapXYAlgorithmLayer final
    : public GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorGeomSwapXYAlgorithm>
{
  public:
    GDALVectorGeomSwapXYAlgorithmLayer(
        OGRLayer &oSrcLayer, const GDALVectorGeomSwapXYAlgorithm::Options &opts)
        : GDALVectorGeomOneToOneAlgorithmLayer<GDALVectorGeomSwapXYAlgorithm>(
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

std::unique_ptr<OGRFeature>
GDALVectorGeomSwapXYAlgorithmLayer::TranslateFeature(
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
/*           GDALVectorGeomSwapXYAlgorithm::CreateAlgLayer()            */
/************************************************************************/

std::unique_ptr<OGRLayerWithTranslateFeature>
GDALVectorGeomSwapXYAlgorithm::CreateAlgLayer(OGRLayer &srcLayer)
{
    return std::make_unique<GDALVectorGeomSwapXYAlgorithmLayer>(srcLayer,
                                                                m_opts);
}

//! @endcond
