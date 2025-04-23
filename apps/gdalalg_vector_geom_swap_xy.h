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

#ifndef GDALALG_VECTOR_GEOM_SWAP_XY_INCLUDED
#define GDALALG_VECTOR_GEOM_SWAP_XY_INCLUDED

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALVectorGeomSwapXYAlgorithm                     */
/************************************************************************/

class GDALVectorGeomSwapXYAlgorithm final
    : public GDALVectorGeomAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "swap-xy";
    static constexpr const char *DESCRIPTION =
        "Swap X and Y coordinates of geometries of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_geom_swap_xy.html";

    struct Options : public GDALVectorGeomAbstractAlgorithm::OptionsBase
    {
    };

    std::unique_ptr<OGRLayerWithTranslateFeature>
    CreateAlgLayer(OGRLayer &srcLayer) override;

    explicit GDALVectorGeomSwapXYAlgorithm(bool standaloneStep);

  private:
    Options m_opts{};
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_SWAP_XY_INCLUDED */
