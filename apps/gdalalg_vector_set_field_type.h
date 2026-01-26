/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "set-field-type" step of "vector pipeline"
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_SET_FIELD_TYPE_INCLUDED
#define GDALALG_VECTOR_SET_FIELD_TYPE_INCLUDED

#include "gdalalg_vector_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALVectorSetFieldTypeAlgorithm                    */
/************************************************************************/

class GDALVectorSetFieldTypeAlgorithm /* non final */
    : public GDALVectorPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "set-field-type";
    static constexpr const char *DESCRIPTION =
        "Modify the type of a field of a vector dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_vector_set_field_type.html";

    explicit GDALVectorSetFieldTypeAlgorithm(bool standaloneStep = false);

    CPLJSONObject Get_OGR_SCHEMA_OpenOption_Layer() const override;

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    bool GlobalValidation() const;

    // NOTE: if adding any option, please update Get_OGR_SCHEMA_OpenOption_Layer() */
    std::string m_activeLayer{};
    std::string m_fieldName{};
    OGRFieldType m_srcFieldType{OGRFieldType::OFTString};
    OGRFieldSubType m_srcFieldSubType{OGRFieldSubType::OFSTNone};
    std::string m_srcFieldTypeSubTypeStr{};
    OGRFieldType m_newFieldType{OGRFieldType::OFTString};
    OGRFieldSubType m_newFieldSubType{OGRFieldSubType::OFSTNone};
    std::string m_newFieldTypeSubTypeStr{};
    // NOTE: if adding any option, please update Get_OGR_SCHEMA_OpenOption_Layer() */
};

/************************************************************************/
/*              GDALVectorSetFieldTypeAlgorithmStandalone               */
/************************************************************************/

class GDALVectorSetFieldTypeAlgorithmStandalone final
    : public GDALVectorSetFieldTypeAlgorithm
{
  public:
    GDALVectorSetFieldTypeAlgorithmStandalone()
        : GDALVectorSetFieldTypeAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALVectorSetFieldTypeAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_VECTOR_SET_FIELD_TYPE_INCLUDED */
