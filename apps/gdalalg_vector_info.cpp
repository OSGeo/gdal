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
    AddInputDatasetArg(&m_dataset, GDAL_OF_VECTOR).AddAlias("dataset");
    AddLayerNameArg(&m_layerNames).SetMutualExclusionGroup("layer-sql");
    AddArg("features", 0,
           _("List all features (beware of RAM consumption on large layers)"),
           &m_listFeatures);
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
           &m_update);
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
    if (m_format == "json")
    {
        aosOptions.AddString("-json");
    }
    else
    {
        aosOptions.AddString("-al");
        if (!m_listFeatures)
            aosOptions.AddString("-so");
    }
    if (m_listFeatures)
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
