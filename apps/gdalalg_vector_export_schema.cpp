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
    AddArg(GDAL_ARG_NAME_OUTPUT, 'o',
           _("Output file name. If not specified, output is sent to stdout"),
           &m_outputFileName);
    AddOverwriteArg(&m_overwrite,
                    _("Whether overwriting existing output file is allowed"));
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

    const auto outFileNameArg = GetArg(GDAL_ARG_NAME_OUTPUT);
    if (outFileNameArg && outFileNameArg->IsExplicitlySet())
    {
        // Check if file exists
        VSIStatBufL sStat;
        if (VSIStatL(m_outputFileName.c_str(), &sStat) == 0)
        {
            const auto overwriteArg = GetArg(GDAL_ARG_NAME_OVERWRITE);
            if (overwriteArg && overwriteArg->GetType() == GAAT_BOOLEAN)
            {
                if (!overwriteArg->GDALAlgorithmArg::Get<bool>())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "File '%s' already exists. Specify the "
                             "--overwrite option to overwrite it.",
                             m_outputFileName.c_str());
                    CPLFree(ret);
                    return false;
                }
                else
                {
                    if (VSIUnlink(m_outputFileName.c_str()) != 0)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Failed to delete existing file '%s'",
                                 m_outputFileName.c_str());
                        CPLFree(ret);
                        return false;
                    }
                }
            }
        }

        VSILFILE *fp = VSIFOpenL(m_outputFileName.c_str(), "wb");
        if (!fp)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Failed to open output file '%s'",
                     m_outputFileName.c_str());
            CPLFree(ret);
            return false;
        }
        auto nBytesWritten = VSIFWriteL(ret, 1, strlen(ret), fp);
        VSIFCloseL(fp);
        if (nBytesWritten != strlen(ret))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to write output file '%s'",
                     m_outputFileName.c_str());
            CPLFree(ret);
            return false;
        }
    }
    else
    {
        m_output = ret;
    }
    CPLFree(ret);

    return true;
}

GDALVectorExportSchemaAlgorithmStandalone::
    ~GDALVectorExportSchemaAlgorithmStandalone() = default;

//! @endcond
