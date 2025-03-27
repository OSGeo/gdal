/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector geom explode-collections"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_GEOM_EXPLODE_COLLECTIONS_INCLUDED
#define GDALALG_VECTOR_GEOM_EXPLODE_COLLECTIONS_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*              GDALVectorGeomExplodeCollectionsAlgorithm               */
/************************************************************************/

class GDALVectorGeomExplodeCollectionsAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "explode-collections";
    static constexpr const char *DESCRIPTION =
        "Explode geometries of type collection of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_geom_explode_collections.html";

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

    explicit GDALVectorGeomExplodeCollectionsAlgorithm(
        bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_activeLayer{};
    Options m_opts{};
};

/************************************************************************/
/*            GDALVectorGeomExplodeCollectionsAlgorithmStandalone       */
/************************************************************************/

class GDALVectorGeomExplodeCollectionsAlgorithmStandalone final
    : public GDALVectorGeomExplodeCollectionsAlgorithm
{
  public:
    GDALVectorGeomExplodeCollectionsAlgorithmStandalone()
        : GDALVectorGeomExplodeCollectionsAlgorithm(
              /* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_EXPLODE_COLLECTIONS_INCLUDED */
