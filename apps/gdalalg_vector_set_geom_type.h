/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector set-geom-type"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_SET_TYPE_INCLUDED
#define GDALALG_VECTOR_SET_TYPE_INCLUDED

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALVectorSetGeomTypeAlgorithm                    */
/************************************************************************/

class GDALVectorSetGeomTypeAlgorithm /* non final */
    : public GDALVectorGeomAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "set-geom-type";
    static constexpr const char *DESCRIPTION =
        "Modify the geometry type of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_set_geom_type.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        // Old name of GDAL 3.11
        return {
            GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR,
            "set-type",
        };
    }

    explicit GDALVectorSetGeomTypeAlgorithm(bool standaloneStep = false);

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
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    Options m_opts{};
};

/************************************************************************/
/*               GDALVectorSetGeomTypeAlgorithmStandalone               */
/************************************************************************/

class GDALVectorSetGeomTypeAlgorithmStandalone final
    : public GDALVectorSetGeomTypeAlgorithm
{
  public:
    GDALVectorSetGeomTypeAlgorithmStandalone()
        : GDALVectorSetGeomTypeAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorSetGeomTypeAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_SET_TYPE_INCLUDED */
