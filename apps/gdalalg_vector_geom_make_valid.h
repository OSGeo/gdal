/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector geom make-valid"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_GEOM_MAKE_VALID_INCLUDED
#define GDALALG_VECTOR_GEOM_MAKE_VALID_INCLUDED

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                 GDALVectorGeomMakeValidAlgorithm                     */
/************************************************************************/

class GDALVectorGeomMakeValidAlgorithm final
    : public GDALVectorGeomAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "make-valid";
    static constexpr const char *DESCRIPTION =
        "Fix validity of geometries of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_geom_make_valid.html";

    struct Options : public GDALVectorGeomAbstractAlgorithm::OptionsBase
    {
        std::string m_method = "linework";
        bool m_keepLowerDim = false;
    };

    std::unique_ptr<OGRLayerWithTranslateFeature>
    CreateAlgLayer(OGRLayer &srcLayer) override;

    explicit GDALVectorGeomMakeValidAlgorithm(bool standaloneStep);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    Options m_opts{};
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_MAKE_VALID_INCLUDED */
