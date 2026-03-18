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

#include "gdalalg_vector_export_schema.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*  GDALVectorExportSchemaAlgorithm::GDALVectorExportSchemaAlgorithm()  */
/************************************************************************/

GDALVectorExportSchemaAlgorithm::GDALVectorExportSchemaAlgorithm(
    bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      ConstructorOptions()
                                          .SetStandaloneStep(standaloneStep)
                                          .SetInputDatasetMaxCount(1)
                                          .SetAddDefaultArguments(false))
{
    AddOpenOptionsArg(&m_openOptions).SetHiddenForCLI(!standaloneStep);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR})
        .SetHiddenForCLI(!standaloneStep);

    auto &datasetArg =
        AddInputDatasetArg(&m_inputDataset, GDAL_OF_VECTOR).AddAlias("dataset");
    if (!standaloneStep)
        datasetArg.SetHidden();
    auto &layerArg = AddLayerNameArg(&m_layerNames).AddAlias("layer");
    SetAutoCompleteFunctionForLayerName(layerArg, datasetArg);
    AddOutputStringArg(&m_output);
    AddStdoutArg(&m_stdout);
}

/************************************************************************/
/*              GDALVectorExportSchemaAlgorithm::RunStep()              */
/************************************************************************/

bool GDALVectorExportSchemaAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    CPLAssert(m_inputDataset.size() == 1);
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLStringList aosOptions;

    aosOptions.AddString("-schema");
    aosOptions.AddString("--cli");

    // Must be last, as positional
    aosOptions.AddString("dummy");

    for (const std::string &name : m_layerNames)
        aosOptions.AddString(name.c_str());

    if (m_layerNames.empty())
    {
        aosOptions.AddString("-al");
    }

    GDALVectorInfoOptions *psInfo =
        GDALVectorInfoOptionsNew(aosOptions.List(), nullptr);

    char *ret = GDALVectorInfo(GDALDataset::ToHandle(poSrcDS), psInfo);
    GDALVectorInfoOptionsFree(psInfo);
    if (!ret)
        return false;

    m_output = ret;
    CPLFree(ret);

    return true;
}

GDALVectorExportSchemaAlgorithmStandalone::
    ~GDALVectorExportSchemaAlgorithmStandalone() = default;

//! @endcond
