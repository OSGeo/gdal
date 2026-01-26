/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector make-valid"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_MAKE_VALID_INCLUDED
#define GDALALG_VECTOR_MAKE_VALID_INCLUDED

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALVectorMakeValidAlgorithm                     */
/************************************************************************/

class GDALVectorMakeValidAlgorithm /* non final */
    : public GDALVectorGeomAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "make-valid";
    static constexpr const char *DESCRIPTION =
        "Fix validity of geometries of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_make_valid.html";

    struct Options : public GDALVectorGeomAbstractAlgorithm::OptionsBase
    {
        std::string m_method = "linework";
        bool m_keepLowerDim = false;
    };

    std::unique_ptr<OGRLayerWithTranslateFeature>
    CreateAlgLayer(OGRLayer &srcLayer) override;

    explicit GDALVectorMakeValidAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    Options m_opts{};
};

/************************************************************************/
/*                GDALVectorMakeValidAlgorithmStandalone                */
/************************************************************************/

class GDALVectorMakeValidAlgorithmStandalone final
    : public GDALVectorMakeValidAlgorithm
{
  public:
    GDALVectorMakeValidAlgorithmStandalone()
        : GDALVectorMakeValidAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorMakeValidAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_MAKE_VALID_INCLUDED */
