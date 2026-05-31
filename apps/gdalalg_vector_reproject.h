/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal vector reproject"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_REPROJECT_INCLUDED
#define GDALALG_VECTOR_REPROJECT_INCLUDED

#include "gdalvectorpipelinestepalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALVectorReprojectAlgorithm                     */
/************************************************************************/

class GDALVectorReprojectAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "reproject";
    static constexpr const char *DESCRIPTION = "Reproject a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_reproject.html";

    explicit GDALVectorReprojectAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::string m_activeLayer{};
    std::string m_srcCrs{};
    std::string m_dstCrs{};
};

/************************************************************************/
/*                GDALVectorReprojectAlgorithmStandalone                */
/************************************************************************/

class GDALVectorReprojectAlgorithmStandalone final
    : public GDALVectorReprojectAlgorithm
{
  public:
    GDALVectorReprojectAlgorithmStandalone()
        : GDALVectorReprojectAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorReprojectAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_REPROJECT_INCLUDED */
