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

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*              GDALVectorGeomExplodeCollectionsAlgorithm               */
/************************************************************************/

class GDALVectorGeomExplodeCollectionsAlgorithm final
    : public GDALVectorGeomAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "explode-collections";
    static constexpr const char *DESCRIPTION =
        "Explode geometries of type collection of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_geom_explode_collections.html";

    struct Options : public GDALVectorGeomAbstractAlgorithm::OptionsBase
    {
        std::string m_type{};
        bool m_skip = false;

        // Computed value from m_type
        OGRwkbGeometryType m_eType = wkbUnknown;
    };

    std::unique_ptr<OGRLayerWithTranslateFeature>
    CreateAlgLayer(OGRLayer &srcLayer) override;

    explicit GDALVectorGeomExplodeCollectionsAlgorithm(bool standaloneStep);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    Options m_opts{};
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_EXPLODE_COLLECTIONS_INCLUDED */
