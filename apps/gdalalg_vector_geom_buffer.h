/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector geom buffer"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_GEOM_BUFFER_INCLUDED
#define GDALALG_VECTOR_GEOM_BUFFER_INCLUDED

#include "gdalalg_vector_geom.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                  GDALVectorGeomBufferAlgorithm                       */
/************************************************************************/

class GDALVectorGeomBufferAlgorithm final
    : public GDALVectorGeomAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "buffer";
    static constexpr const char *DESCRIPTION =
        "Compute a buffer around geometries of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_geom_buffer.html";

    struct Options : public GDALVectorGeomAbstractAlgorithm::OptionsBase
    {
        double m_distance = 0;
        std::string m_endCapStyle = "round";
        std::string m_joinStyle = "round";
        double m_mitreLimit = 5;
        int m_quadrantSegments = 8;
        std::string m_side = "both";
    };

    std::unique_ptr<OGRLayerWithTranslateFeature>
    CreateAlgLayer(OGRLayer &srcLayer) override;

    explicit GDALVectorGeomBufferAlgorithm(bool standaloneStep);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    Options m_opts{};
};

//! @endcond

#endif /* GDALALG_VECTOR_GEOM_BUFFER_INCLUDED */
