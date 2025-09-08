/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "dataset identify" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_dataset_identify.h"

#include "cpl_string.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                       GDALDatasetIdentifyAlgorithm()                 */
/************************************************************************/

GDALDatasetIdentifyAlgorithm::GDALDatasetIdentifyAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL), m_oWriter(JSONPrint, this)
{
    AddProgressArg();

    auto &arg = AddArg("filename", 0, _("File or directory name"), &m_filename)
                    .SetPositional()
                    .SetRequired();
    SetAutoCompleteFunctionForFilename(arg, 0);

    AddOutputFormatArg(&m_format).SetChoices("json", "text");

    AddArg("recursive", 'r', _("Recursively scan files/folders for datasets"),
           &m_recursive);

    AddArg("force-recursive", 0,
           _("Recursively scan folders for datasets, forcing "
             "recursion in folders recognized as valid formats"),
           &m_forceRecursive);

    AddArg("report-failures", 0,
           _("Report failures if file type is unidentified"),
           &m_reportFailures);

    AddOutputStringArg(&m_output);
    AddStdoutArg(&m_stdout);
}

/************************************************************************/
/*                GDALDatasetIdentifyAlgorithm::Print()                 */
/************************************************************************/

void GDALDatasetIdentifyAlgorithm::Print(const char *str)
{
    if (m_stdout)
        fwrite(str, 1, strlen(str), stdout);
    else
        m_output += str;
}

/************************************************************************/
/*                 GDALDatasetIdentifyAlgorithm::JSONPrint()            */
/************************************************************************/

/* static */ void GDALDatasetIdentifyAlgorithm::JSONPrint(const char *pszTxt,
                                                          void *pUserData)
{
    static_cast<GDALDatasetIdentifyAlgorithm *>(pUserData)->Print(pszTxt);
}

/************************************************************************/
/*                              Process()                               */
/************************************************************************/

bool GDALDatasetIdentifyAlgorithm::Process(const char *pszTarget,
                                           CSLConstList papszSiblingList,
                                           GDALProgressFunc pfnProgress,
                                           void *pProgressData)

{
    if (IsCalledFromCommandLine())
        pfnProgress = nullptr;

    if (m_format.empty())
        m_format = IsCalledFromCommandLine() ? "text" : "json";

    GDALDriverH hDriver = nullptr;
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        hDriver = GDALIdentifyDriver(pszTarget, papszSiblingList);
    }

    if (m_format == "json")
    {
        if (hDriver)
        {
            m_oWriter.StartObj();
            m_oWriter.AddObjKey("name");
            m_oWriter.Add(pszTarget);
            m_oWriter.AddObjKey("driver");
            m_oWriter.Add(GDALGetDriverShortName(hDriver));
            m_oWriter.EndObj();
        }
        else if (m_reportFailures)
        {
            m_oWriter.StartObj();
            m_oWriter.AddObjKey("name");
            m_oWriter.Add(pszTarget);
            m_oWriter.AddObjKey("driver");
            m_oWriter.AddNull();
            m_oWriter.EndObj();
        }
    }
    else
    {
        if (hDriver)
            Print(CPLSPrintf("%s: %s\n", pszTarget,
                             GDALGetDriverShortName(hDriver)));
        else if (m_reportFailures)
            Print(CPLSPrintf("%s: unrecognized\n", pszTarget));
    }

    bool ret = true;
    VSIStatBufL sStatBuf;
    if ((m_forceRecursive || (m_recursive && hDriver == nullptr)) &&
        VSIStatL(pszTarget, &sStatBuf) == 0 && VSI_ISDIR(sStatBuf.st_mode))
    {
        const CPLStringList aosSiblingList(VSIReadDir(pszTarget));
        const int nListSize = aosSiblingList.size();
        for (int i = 0; i < nListSize; ++i)
        {
            const char *pszSubTarget = aosSiblingList[i];
            if (!(EQUAL(pszSubTarget, "..") || EQUAL(pszSubTarget, ".")))
            {
                const std::string osSubTarget =
                    CPLFormFilenameSafe(pszTarget, pszSubTarget, nullptr);

                std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
                    pScaledProgress(GDALCreateScaledProgress(
                                        static_cast<double>(i) / nListSize,
                                        static_cast<double>(i + 1) / nListSize,
                                        pfnProgress, pProgressData),
                                    GDALDestroyScaledProgress);
                ret = ret &&
                      Process(osSubTarget.c_str(), aosSiblingList.List(),
                              pScaledProgress ? GDALScaledProgress : nullptr,
                              pScaledProgress.get());
            }
        }
    }

    return ret && (!pfnProgress || pfnProgress(1.0, "", pProgressData));
}

/************************************************************************/
/*                  GDALDatasetIdentifyAlgorithm::RunImpl()             */
/************************************************************************/

bool GDALDatasetIdentifyAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                           void *pProgressData)
{
    if (m_format.empty())
        m_format = IsCalledFromCommandLine() ? "text" : "json";

    if (m_format == "json")
        m_oWriter.StartArray();
    int i = 0;
    bool ret = true;
    for (const std::string &osPath : m_filename)
    {
        std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
            pScaledProgress(GDALCreateScaledProgress(
                                static_cast<double>(i) /
                                    static_cast<int>(m_filename.size()),
                                static_cast<double>(i + 1) /
                                    static_cast<int>(m_filename.size()),
                                pfnProgress, pProgressData),
                            GDALDestroyScaledProgress);
        ret = ret && Process(osPath.c_str(), nullptr,
                             pScaledProgress ? GDALScaledProgress : nullptr,
                             pScaledProgress.get());
        ++i;
    }
    if (m_format == "json")
        m_oWriter.EndArray();

    return ret;
}

//! @endcond
