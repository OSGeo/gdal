/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector dissolve"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_DISSOLVE_INCLUDED
#define GDALALG_VECTOR_DISSOLVE_INCLUDED

#include "gdalalg_vector_geom.h"
#include "cpl_progress.h"

#include <string>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALVectorDissolveAlgorithm                      */
/************************************************************************/

class GDALVectorDissolveAlgorithm : public GDALVectorGeomAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "dissolve";
    static constexpr const char *DESCRIPTION = "Dissolves multipart features";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_dissolve.html";

    explicit GDALVectorDissolveAlgorithm(bool standaloneStep = false);

    std::unique_ptr<OGRLayerWithTranslateFeature>
    CreateAlgLayer(OGRLayer &srcLayer) override;

    struct Options : OptionsBase
    {
    };

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    Options m_opts{};
};

/************************************************************************/
/*                GDALVectorDissolveAlgorithmStandalone                 */
/************************************************************************/

class GDALVectorDissolveAlgorithmStandalone final
    : public GDALVectorDissolveAlgorithm
{
  public:
    GDALVectorDissolveAlgorithmStandalone()
        : GDALVectorDissolveAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorDissolveAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_DISSOLVE_INCLUDED */
