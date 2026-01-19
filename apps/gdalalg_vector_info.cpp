/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector info" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_info.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*            GDALVectorInfoAlgorithm::GDALVectorInfoAlgorithm()        */
/************************************************************************/

GDALVectorInfoAlgorithm::GDALVectorInfoAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      ConstructorOptions()
                                          .SetStandaloneStep(standaloneStep)
                                          .SetInputDatasetMaxCount(1)
                                          .SetAddDefaultArguments(false))
{
    AddOutputFormatArg(&m_format).SetChoices("json", "text");
    AddOpenOptionsArg(&m_openOptions).SetHiddenForCLI(!standaloneStep);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR})
        .SetHiddenForCLI(!standaloneStep);
    GDALInConstructionAlgorithmArg *pDatasetArg = nullptr;
    if (standaloneStep)
    {
        auto &datasetArg =
            AddInputDatasetArg(&m_inputDataset, GDAL_OF_VECTOR,
                               /* positionalAndRequired = */ standaloneStep)
                .AddAlias("dataset")
                .SetHiddenForCLI(!standaloneStep);
        pDatasetArg = &datasetArg;
    }
    auto &layerArg = AddLayerNameArg(&m_layerNames)
                         .SetMutualExclusionGroup("layer-sql")
                         .AddAlias("layer");
    if (pDatasetArg)
        SetAutoCompleteFunctionForLayerName(layerArg, *pDatasetArg);
    auto &argFeature =
        AddArg(
            "features", 0,
            _("List all features (beware of RAM consumption on large layers)"),
            &m_listFeatures)
            .SetMutualExclusionGroup("summary-features");
    AddArg("summary", 0, _("List the layer names and the geometry type"),
           &m_summaryOnly)
        .SetMutualExclusionGroup("summary-features");
    AddArg("limit", 0,
           _("Limit the number of features per layer (implies --features)"),
           &m_limit)
        .SetMinValueIncluded(0)
        .SetMetaVar("FEATURE-COUNT")
        .AddAction([&argFeature]() { argFeature.Set(true); });
    AddArg("sql", 0,
           _("Execute the indicated SQL statement and return the result"),
           &m_sql)
        .SetReadFromFileAtSyntaxAllowed()
        .SetMetaVar("<statement>|@<filename>")
        .SetRemoveSQLCommentsEnabled()
        .SetMutualExclusionGroup("layer-sql");
    AddArg("where", 0,
           _("Attribute query in a restricted form of the queries used in the "
             "SQL WHERE statement"),
           &m_where)
        .SetReadFromFileAtSyntaxAllowed()
        .SetMetaVar("<WHERE>|@<filename>")
        .SetRemoveSQLCommentsEnabled();
    AddArg("dialect", 0, _("SQL dialect"), &m_dialect);
    AddOutputStringArg(&m_output);
    AddStdoutArg(&m_stdout);

    AddValidationAction(
        [this]()
        {
            if (!m_sql.empty() && !m_where.empty())
            {
                ReportError(CE_Failure, CPLE_NotSupported,
                            "Option 'sql' and 'where' are mutually exclusive");
                return false;
            }
            return true;
        });
}

/************************************************************************/
/*                  GDALVectorInfoAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALVectorInfoAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    CPLAssert(m_inputDataset.size() == 1);
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    if (m_format.empty())
        m_format = IsCalledFromCommandLine() ? "text" : "json";

    CPLStringList aosOptions;

    aosOptions.AddString("--cli");

    if (m_format == "json")
    {
        aosOptions.AddString("-json");
    }

    if (m_summaryOnly)
    {
        aosOptions.AddString("-summary");
    }
    else if (m_listFeatures)
    {
        aosOptions.AddString("-features");
    }

    if (!m_sql.empty())
    {
        aosOptions.AddString("-sql");
        aosOptions.AddString(m_sql.c_str());
    }
    if (!m_where.empty())
    {
        aosOptions.AddString("-where");
        aosOptions.AddString(m_where.c_str());
    }
    if (!m_dialect.empty())
    {
        aosOptions.AddString("-dialect");
        aosOptions.AddString(m_dialect.c_str());
    }
    if (m_stdout)
    {
        aosOptions.AddString("-stdout");
    }
    if (m_limit > 0)
    {
        aosOptions.AddString("-limit");
        aosOptions.AddString(std::to_string(m_limit));
    }

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

GDALVectorInfoAlgorithmStandalone::~GDALVectorInfoAlgorithmStandalone() =
    default;

//! @endcond
