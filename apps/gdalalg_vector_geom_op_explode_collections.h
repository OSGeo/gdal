/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector geom-op explode-collections"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_GEOM_OP_EXPLODE_COLLECTIONS_INCLUDED
#define GDALALG_VECTOR_GEOM_OP_EXPLODE_COLLECTIONS_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*             GDALVectorGeomOpExplodeCollectionsAlgorithm              */
/************************************************************************/

class GDALVectorGeomOpExplodeCollectionsAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "explode-collections";
    static constexpr const char *DESCRIPTION =
        "Explode geometries of type collection of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_geom_op_explode_collections.html";

    static std::vector<std::string> GetAliases()
    {
        return {};
    }

    struct Options
    {
        std::string m_geomField{};
        std::string m_type{};
        bool m_skip = false;

        // Computed value from m_type
        OGRwkbGeometryType m_eType = wkbUnknown;
    };

    explicit GDALVectorGeomOpExplodeCollectionsAlgorithm(
        bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_activeLayer{};
    Options m_opts{};
};

/************************************************************************/
/*           GDALVectorGeomOpExplodeCollectionsAlgorithmStandalone      */
/************************************************************************/

class GDALVectorGeomOpExplodeCollectionsAlgorithmStandalone final
    : public GDALVectorGeomOpExplodeCollectionsAlgorithm
{
  public:
    GDALVectorGeomOpExplodeCollectionsAlgorithmStandalone()
        : GDALVectorGeomOpExplodeCollectionsAlgorithm(
              /* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_OP_EXPLODE_COLLECTIONS_INCLUDED */
