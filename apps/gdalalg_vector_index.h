/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector index" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_INDEX_INCLUDED
#define GDALALG_VECTOR_INDEX_INCLUDED

#include "gdalalg_vector_output_abstract.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALVectorIndexAlgorithm                       */
/************************************************************************/

class CPL_DLL GDALVectorIndexAlgorithm final
    : public GDALVectorOutputAbstractAlgorithm
{
  public:
    static constexpr const char *NAME = "index";
    static constexpr const char *DESCRIPTION =
        "Create a vector index of vector datasets.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_index.html";

    GDALVectorIndexAlgorithm();

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::vector<GDALArgDatasetValue> m_inputDatasets{};
    bool m_recursive = false;
    std::vector<std::string> m_filenameFilter{};
    std::string m_locationName = "location";
    bool m_writeAbsolutePaths = false;
    std::string m_crs{};
    std::string m_sourceCrsName{};
    std::string m_sourceCrsFormat = "auto";
    std::vector<std::string> m_metadata{};
    std::vector<std::string> m_layerNames{};
    std::vector<int> m_layerIndices{};
    bool m_datasetNameOnly = false;
    bool m_skipDifferentCRS = false;  // for compatibility with ogrtindex
    bool m_acceptDifferentCRS = false;
    bool m_acceptDifferentSchemas = false;
    bool m_calledFromOgrTIndex = false;
};

//! @endcond

#endif
