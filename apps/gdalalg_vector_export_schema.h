/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector export-schema" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2026, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_EXPORT_SCHEMA_INCLUDED
#define GDALALG_VECTOR_EXPORT_SCHEMA_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALVectorExportSchemaAlgorithm                    */
/************************************************************************/

class GDALVectorExportSchemaAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "export-schema";
    static constexpr const char *DESCRIPTION =
        "Export the OGR_SCHEMA from a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_export_schema.html";

    explicit GDALVectorExportSchemaAlgorithm(bool standaloneStep = false);

    bool CanBeLastStep() const override
    {
        return true;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    std::vector<std::string> m_layerNames{};
};

/************************************************************************/
/*              GDALVectorExportSchemaAlgorithmStandalone               */
/************************************************************************/

class GDALVectorExportSchemaAlgorithmStandalone final
    : public GDALVectorExportSchemaAlgorithm
{
  public:
    GDALVectorExportSchemaAlgorithmStandalone()
        : GDALVectorExportSchemaAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorExportSchemaAlgorithmStandalone() override;
};

//! @endcond

#endif
