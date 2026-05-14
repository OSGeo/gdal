/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector layer-algebra" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_LAYER_ALGEBRA_INCLUDED
#define GDALALG_VECTOR_LAYER_ALGEBRA_INCLUDED

#include "gdalalgorithm.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALVectorLayerAlgebraAlgorithm                    */
/************************************************************************/

class GDALVectorLayerAlgebraAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "layer-algebra";
    static constexpr const char *DESCRIPTION =
        "Perform algebraic operation between 2 layers.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_layer_algebra.html";

    GDALVectorLayerAlgebraAlgorithm();

  private:
    std::string m_operation{};

    std::vector<std::string> m_inputFormats{};
    std::vector<std::string> m_openOptions{};
    GDALArgDatasetValue m_inputDataset{};
    GDALArgDatasetValue m_methodDataset{};
    std::string m_inputLayerName{};
    std::string m_methodLayerName{};

    // Output arguments
    GDALArgDatasetValue m_outputDataset{};
    std::string m_format{};
    std::vector<std::string> m_creationOptions{};
    std::vector<std::string> m_layerCreationOptions{};
    bool m_overwrite = false;
    bool m_update = false;
    bool m_overwriteLayer = false;
    bool m_appendLayer = false;
    std::string m_outputLayerName{};
    std::string m_geometryType{};

    std::string m_inputPrefix{};
    std::vector<std::string> m_inputFields{};
    bool m_noInputFields = false;
    bool m_allInputFields = false;

    std::string m_methodPrefix{};
    std::vector<std::string> m_methodFields{};
    bool m_noMethodFields = false;
    bool m_allMethodFields = false;

    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;
};

//! @endcond

#endif
