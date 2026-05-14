/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector create" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2026, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_CREATE_INCLUDED
#define GDALALG_VECTOR_CREATE_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                      GDALVectorCreateAlgorithm                       */
/************************************************************************/

class GDALVectorCreateAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "create";
    static constexpr const char *DESCRIPTION = "Create a vector dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector_create.html";

    explicit GDALVectorCreateAlgorithm(bool /* standaloneStep */ = true);

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    /** Get the list of fields to create in the output layer,
     * based on the given OGR_SCHEMA or the --like argument
     * and/or the --field parameters.
     * OGR_SCHEMA and --like and --field parameters are mutually exclusive
     */
    std::vector<OGRFieldDefn> GetOutputFields() const;

    bool CanBeFirstStep() const override
    {
        return true;
    }

    bool CanBeMiddleStep() const override
    {
        return true;
    }

    bool CanBeLastStep() const override
    {
        return true;
    }

    /**
     * Create a layer in the given dataset given the fields defined in fieldDefinitions
     * and geometry fields defined in geometryFieldDefinitions.
     */
    bool CreateLayer(
        GDALDataset *poDstDS, const std::string &layerName,
        const std::string &fidColumnName,
        const std::vector<OGRFieldDefn> &fieldDefinitions,
        const std::vector<OGRGeomFieldDefn> &geometryFieldDefinitions) const;

    std::string m_crs{};
    std::string m_fidColumnName{};
    std::string m_geometryType{};
    std::string m_geometryFieldName{"geom"};
    std::string m_schemaJsonOrPath{};
    std::vector<OGRFieldDefn> m_fieldDefinitions{};
    std::vector<std::string> m_fieldStrDefinitions{};
};

/************************************************************************/
/*                 GDALVectorCreateAlgorithmStandalone                  */
/************************************************************************/

class GDALVectorCreateAlgorithmStandalone final
    : public GDALVectorCreateAlgorithm
{
  public:
    GDALVectorCreateAlgorithmStandalone()
        : GDALVectorCreateAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorCreateAlgorithmStandalone() override;
};

//! @endcond

#endif  // GDALALG_VECTOR_CREATE_INCLUDED
