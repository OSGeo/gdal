/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "rename-layer" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_RENAME_LAYER_INCLUDED
#define GDALALG_VECTOR_RENAME_LAYER_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALVectorRenameLayerAlgorithm                    */
/************************************************************************/

class GDALVectorRenameLayerAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "rename-layer";
    static constexpr const char *DESCRIPTION =
        "Rename layer(s) of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_rename_layer.html";

    explicit GDALVectorRenameLayerAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::string m_inputLayerName{};
    bool m_ascii = false;
    bool m_filenameCompatible = false;
    bool m_lowerCase = false;
    std::string m_reservedChars{};
    std::string m_replacementChar{};
    int m_maxLength = 0;
};

/************************************************************************/
/*               GDALVectorRenameLayerAlgorithmStandalone               */
/************************************************************************/

class GDALVectorRenameLayerAlgorithmStandalone final
    : public GDALVectorRenameLayerAlgorithm
{
  public:
    GDALVectorRenameLayerAlgorithmStandalone()
        : GDALVectorRenameLayerAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorRenameLayerAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_RENAME_LAYER_INCLUDED */
