/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "sql" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_SQL_INCLUDED
#define GDALALG_VECTOR_SQL_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALVectorSQLAlgorithm                          */
/************************************************************************/

class GDALVectorSQLAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "sql";
    static constexpr const char *DESCRIPTION =
        "Apply SQL statement(s) to a dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_sql.html";

    explicit GDALVectorSQLAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::vector<std::string> m_sql{};
    std::vector<std::string> m_outputLayer{};
    std::string m_dialect{};
};

/************************************************************************/
/*                     GDALVectorSQLAlgorithmStandalone                 */
/************************************************************************/

class GDALVectorSQLAlgorithmStandalone final : public GDALVectorSQLAlgorithm
{
  public:
    GDALVectorSQLAlgorithmStandalone()
        : GDALVectorSQLAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_VECTOR_SQL_INCLUDED */
