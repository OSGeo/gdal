/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector explode-collections"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_EXPLODE_COLLECTIONS_INCLUDED
#define GDALALG_VECTOR_EXPLODE_COLLECTIONS_INCLUDED

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                GDALVectorExplodeCollectionsAlgorithm                 */
/************************************************************************/

class GDALVectorExplodeCollectionsAlgorithm /* non final */
    : public GDALVectorGeomAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "explode-collections";
    static constexpr const char *DESCRIPTION =
        "Explode geometries of type collection of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_explode_collections.html";

    struct Options : public GDALVectorGeomAbstractAlgorithm::OptionsBase
    {
        std::string m_type{};
        bool m_skip = false;

        // Computed value from m_type
        OGRwkbGeometryType m_eType = wkbUnknown;
    };

    std::unique_ptr<OGRLayerWithTranslateFeature>
    CreateAlgLayer(OGRLayer &srcLayer) override;

    explicit GDALVectorExplodeCollectionsAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    Options m_opts{};
};

/************************************************************************/
/*           GDALVectorExplodeCollectionsAlgorithmStandalone            */
/************************************************************************/

class GDALVectorExplodeCollectionsAlgorithmStandalone final
    : public GDALVectorExplodeCollectionsAlgorithm
{
  public:
    GDALVectorExplodeCollectionsAlgorithmStandalone()
        : GDALVectorExplodeCollectionsAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorExplodeCollectionsAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_EXPLODE_COLLECTIONS_INCLUDED */
