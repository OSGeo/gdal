/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector convert" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_CONVERT_INCLUDED
#define GDALALG_VECTOR_CONVERT_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALVectorConvertAlgorithm                       */
/************************************************************************/

class GDALVectorConvertAlgorithm final : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "convert";
    static constexpr const char *DESCRIPTION = "Convert a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_convert.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR, "translate"};
    }

    explicit GDALVectorConvertAlgorithm(bool /* standaloneStep */ = true);

    void SetDataset(GDALDataset *poDS)
    {
        auto arg = GetArg(GDAL_ARG_NAME_INPUT);
        if (arg)
        {
            auto &val = arg->Get<std::vector<GDALArgDatasetValue>>();
            val.resize(1);
            val[0].Set(poDS);
            arg->NotifyValueSet();
            arg->SetSkipIfAlreadySet();
        }
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
};

//! @endcond

#endif
