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

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALVectorGeomSetTypeAlgorithm                    */
/************************************************************************/

class GDALVectorGeomSetTypeAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "set-type";
    static constexpr const char *DESCRIPTION =
        "Modify the geometry type of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_geom_set_type.html";

    static std::vector<std::string> GetAliases()
    {
        return {};
    }

    explicit GDALVectorGeomSetTypeAlgorithm(bool standaloneStep = false);

    struct Options
    {
        std::string m_geomField{};
        bool m_layerOnly = false;
        bool m_featureGeomOnly = false;
        std::string m_type{};
        bool m_multi = false;
        bool m_single = false;
        bool m_linear = false;
        bool m_curve = false;
        bool m_xy = false;
        bool m_xyz = false;
        bool m_xym = false;
        bool m_xyzm = false;
        bool m_skip = false;

        // Computed value from m_type
        OGRwkbGeometryType m_eType = wkbUnknown;
    };

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_activeLayer{};
    Options m_opts{};
};

/************************************************************************/
/*                GDALVectorGeomSetTypeAlgorithmStandalone              */
/************************************************************************/

class GDALVectorGeomSetTypeAlgorithmStandalone final
    : public GDALVectorGeomSetTypeAlgorithm
{
  public:
    GDALVectorGeomSetTypeAlgorithmStandalone()
        : GDALVectorGeomSetTypeAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_SET_TYPE_INCLUDED */
