/******************************************************************************
*
* Project:  GDAL
* Purpose:  gdal "main" command
* Author:   Even Rouault <even dot rouault at spatialys.com>
*
******************************************************************************
* Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
*
* SPDX-License-Identifier: MIT
****************************************************************************/

#include "gdalalg_main.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                  GDALMainAlgorithm::GDALMainAlgorithm()              */
/************************************************************************/

GDALMainAlgorithm::GDALMainAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    for (const std::string &subAlgName :
         GDALGlobalAlgorithmRegistry::GetSingleton().GetNames())
    {
        const auto pInfo =
            GDALGlobalAlgorithmRegistry::GetSingleton().GetInfo(subAlgName);
        if (pInfo)
            RegisterSubAlgorithm(*pInfo);
    }

    m_longDescription = "'gdal <FILENAME>' can also be used as a shortcut for "
                        "'gdal info <FILENAME>'.\n"
                        "And 'gdal read <FILENAME> ! ...' as a shortcut for "
                        "'gdal pipeline <FILENAME> ! ...'.";

    SetDisplayInJSONUsage(false);
}

/************************************************************************/
/*              GDALMainAlgorithm::ParseCommandLineArguments()          */
/************************************************************************/

bool GDALMainAlgorithm::ParseCommandLineArguments(
    const std::vector<std::string> &args)
{
    if (args.size() >= 2 && args[0] == "read")
    {
        m_subAlg =
            GDALGlobalAlgorithmRegistry::GetSingleton().Instantiate("pipeline");
        if (m_subAlg)
        {
            bool ret = m_subAlg->ParseCommandLineArguments(args);
            if (ret)
            {
                m_selectedSubAlg = &(m_subAlg->GetActualAlgorithm());
                std::vector<std::string> callPath(m_callPath);
                callPath.push_back("vector");
                m_selectedSubAlg->SetCallPath(callPath);
                return true;
            }
            else if (strstr(CPLGetLastErrorMsg(),
                            "has both raster and vector content"))
            {
                m_showUsage = false;
                return false;
            }
        }
    }
    else if (!(args.size() >= 1 && InstantiateSubAlgorithm(args[0])))
    {
        VSIStatBufL sStat;
        for (const auto &arg : args)
        {
            if (VSIStatL(arg.c_str(), &sStat) == 0)
            {
                m_subAlg =
                    GDALGlobalAlgorithmRegistry::GetSingleton().Instantiate(
                        "info");
                if (m_subAlg)
                {
                    bool ret = m_subAlg->ParseCommandLineArguments(args);
                    if (ret)
                    {
                        m_selectedSubAlg = &(m_subAlg->GetActualAlgorithm());
                        std::vector<std::string> callPath(m_callPath);
                        callPath.push_back(m_selectedSubAlg->GetArg("layer")
                                               ? "vector"
                                               : "raster");
                        m_selectedSubAlg->SetCallPath(callPath);
                        return true;
                    }
                    else if (strstr(CPLGetLastErrorMsg(),
                                    "has both raster and vector content"))
                    {
                        m_showUsage = false;
                        return false;
                    }
                }
            }
        }
    }

    return GDALAlgorithm::ParseCommandLineArguments(args);
}

/************************************************************************/
/*                   GDALMainAlgorithm::GetUsageForCLI()                */
/************************************************************************/

std::string
GDALMainAlgorithm::GetUsageForCLI(bool shortUsage,
                                  const UsageOptions &usageOptions) const
{
    if (m_selectedSubAlg)
        return m_selectedSubAlg->GetUsageForCLI(shortUsage, usageOptions);
    if (m_showUsage)
        return GDALAlgorithm::GetUsageForCLI(shortUsage, usageOptions);
    return std::string();
}

//! @endcond
