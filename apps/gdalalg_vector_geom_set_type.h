/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector geom set-type"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_GEOM_SET_TYPE_INCLUDED
#define GDALALG_VECTOR_GEOM_SET_TYPE_INCLUDED

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALVectorGeomSetTypeAlgorithm                    */
/************************************************************************/

class GDALVectorGeomSetTypeAlgorithm final
    : public GDALVectorGeomAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "set-type";
    static constexpr const char *DESCRIPTION =
        "Modify the geometry type of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_geom_set_type.html";

    explicit GDALVectorGeomSetTypeAlgorithm(bool standaloneStep);

    struct Options : public GDALVectorGeomAbstractAlgorithm::OptionsBase
    {
        bool m_layerOnly = false;
        bool m_featureGeomOnly = false;
        std::string m_type{};
        bool m_multi = false;
        bool m_single = false;
        bool m_linear = false;
        bool m_curve = false;
        std::string m_dim{};
        bool m_skip = false;

        // Computed value from m_type
        OGRwkbGeometryType m_eType = wkbUnknown;
    };

    std::unique_ptr<OGRLayerWithTranslateFeature>
    CreateAlgLayer(OGRLayer &srcLayer) override;

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    Options m_opts{};
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_SET_TYPE_INCLUDED */
