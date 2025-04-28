/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Common code for gdalalg_raster_clip and gdalalg_vector_clip
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_CLIP_COMMON_INCLUDED
#define GDALALG_CLIP_COMMON_INCLUDED

#include "gdalalgorithm.h"

#include "ogr_geometry.h"

#include <utility>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                         GDALClipCommon                               */
/************************************************************************/

class GDALClipCommon /* non final */
{
  public:
    virtual ~GDALClipCommon();

  protected:
    GDALClipCommon() = default;

    std::vector<double> m_bbox{};
    std::string m_bboxCrs{};
    std::string m_geometry{};
    std::string m_geometryCrs{};
    GDALArgDatasetValue m_likeDataset{};
    std::string m_likeLayer{};
    std::string m_likeSQL{};
    std::string m_likeWhere{};

    std::pair<std::unique_ptr<OGRGeometry>, std::string> GetClipGeometry();

  private:
    std::pair<std::unique_ptr<OGRGeometry>, std::string> LoadGeometry();
};

//! @endcond

#endif
