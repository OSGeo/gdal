/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector geom segmentize"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_GEOM_SEGMENTIZE_INCLUDED
#define GDALALG_VECTOR_GEOM_SEGMENTIZE_INCLUDED

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                 GDALVectorGeomSegmentizeAlgorithm                    */
/************************************************************************/

class GDALVectorGeomSegmentizeAlgorithm final
    : public GDALVectorGeomAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "segmentize";
    static constexpr const char *DESCRIPTION =
        "Segmentize geometries of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_geom_segmentize.html";

    struct Options : public GDALVectorGeomAbstractAlgorithm::OptionsBase
    {
        double m_maxLength = 0;
    };

    std::unique_ptr<OGRLayerWithTranslateFeature>
    CreateAlgLayer(OGRLayer &srcLayer) override;

    explicit GDALVectorGeomSegmentizeAlgorithm(bool standaloneStep);

  private:
    Options m_opts{};
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_SEGMENTIZE_INCLUDED */
