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

GDALVectorInfoAlgorithm::GDALVectorInfoAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddOutputFormatArg(&m_format).SetDefault("json").SetChoices("json", "text");
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR});
    auto &datasetArg =
        AddInputDatasetArg(&m_dataset, GDAL_OF_VECTOR).AddAlias("dataset");
    auto &layerArg =
        AddLayerNameArg(&m_layerNames).SetMutualExclusionGroup("layer-sql");
    SetAutoCompleteFunctionForLayerName(layerArg, datasetArg);
    AddArg("features", 0,
           _("List all features (beware of RAM consumption on large layers)"),
           &m_listFeatures)
        .SetMutualExclusionGroup("summary-features");
    AddArg("summary", 0, _("List the layer names and the geometry type"),
           &m_summaryOnly)
        .SetMutualExclusionGroup("summary-features");
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
    AddArg(GDAL_ARG_NAME_UPDATE, 0, _("Open the dataset in update mode"),
           &m_update)
        .AddAction(
            [this]()
            {
                if (m_update)
                {
                    ReportError(CE_Warning, CPLE_AppDefined,
                                "Option 'update' is deprecated since GDAL 3.12 "
                                "and will be removed in GDAL 3.13. Use 'gdal "
                                "vector sql --update' instead.");
                }
            });
    AddOutputStringArg(&m_output);
    AddArg("stdout", 0,
           _("Directly output on stdout (format=text mode only). If enabled, "
             "output-string will be empty"),
           &m_stdout)
        .SetHiddenForCLI();
}

/************************************************************************/
/*                  GDALVectorInfoAlgorithm::RunImpl()                  */
/************************************************************************/

bool GDALVectorInfoAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    CPLAssert(m_dataset.GetDatasetRef());

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

    char *ret = GDALVectorInfo(GDALDataset::ToHandle(m_dataset.GetDatasetRef()),
                               psInfo);
    GDALVectorInfoOptionsFree(psInfo);
    if (!ret)
        return false;

    m_output = ret;
    CPLFree(ret);

    return true;
}

//! @endcond
