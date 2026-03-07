/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "external" subcommand (always in pipeline)
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//! @cond Doxygen_Suppress

#include "gdalalg_external.h"
#include "gdalalg_materialize.h"
#include "gdal_dataset.h"

#include "cpl_atomic_ops.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"

#include <stdio.h>
#if !defined(_WIN32)
#include <sys/wait.h>
#endif

/************************************************************************/
/*                     ~GDALExternalAlgorithmBase()                     */
/************************************************************************/

GDALExternalAlgorithmBase::~GDALExternalAlgorithmBase()
{
    if (!m_osTempInputFilename.empty())
        VSIUnlink(m_osTempInputFilename.c_str());
    if (!m_osTempOutputFilename.empty() &&
        m_osTempOutputFilename != m_osTempInputFilename)
        VSIUnlink(m_osTempOutputFilename.c_str());
}

/************************************************************************/
/*                   GDALExternalAlgorithmBase::Run()                   */
/************************************************************************/

bool GDALExternalAlgorithmBase::Run(
    const std::vector<std::string> &inputFormats,
    std::vector<GDALArgDatasetValue> &inputDataset,
    const std::string &outputFormat, GDALArgDatasetValue &outputDataset)
{
    if (!CPLTestBool(CPLGetConfigOption("GDAL_ENABLE_EXTERNAL", "NO")))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot execute command '%s', because GDAL_ENABLE_EXTERNAL "
                 "configuration option is not set to YES.",
                 m_command.c_str());
        return false;
    }

    const bool bHasInput = m_command.find("<INPUT>") != std::string::npos;
    const bool bHasInputOutput =
        m_command.find("<INPUT-OUTPUT>") != std::string::npos;
    const bool bHasOutput = m_command.find("<OUTPUT>") != std::string::npos;

    const std::string osTempDirname =
        CPLGetDirnameSafe(CPLGenerateTempFilenameSafe(nullptr).c_str());
    if (bHasInput || bHasInputOutput || bHasOutput)
    {
        VSIStatBufL sStat;
        if (VSIStatL(osTempDirname.c_str(), &sStat) != 0 ||
            !VSI_ISDIR(sStat.st_mode))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "'%s', used for temporary directory, is not a valid directory",
                osTempDirname.c_str());
            return false;
        }
        // Check to avoid any possibility of command injection
        if (osTempDirname.find_first_of("'\"^&|<>%!;") != std::string::npos)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "'%s', used for temporary directory, contains a reserved "
                     "character ('\"^&|<>%%!;)",
                     osTempDirname.c_str());
            return false;
        }
    }

    const auto QuoteFilename = [](const std::string &s)
    {
#ifdef _WIN32
        return "\"" + s + "\"";
#else
        return "'" + s + "'";
#endif
    };

    // Process <INPUT> and <INPUT-OUTPUT> placeholder
    if (bHasInput || bHasInputOutput)
    {
        if (inputDataset.size() != 1 || !inputDataset[0].GetDatasetRef())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Command '%s' expects an input dataset to be provided, "
                     "but none is available.",
                     m_command.c_str());
            return false;
        }
        auto poSrcDS = inputDataset[0].GetDatasetRef();

        const std::string osDriverName = !inputFormats.empty() ? inputFormats[0]
                                         : poSrcDS->GetRasterCount() != 0
                                             ? "GTiff"
                                             : "GPKG";

        auto poDriver =
            GetGDALDriverManager()->GetDriverByName(osDriverName.c_str());
        if (!poDriver)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Driver %s not available",
                     osDriverName.c_str());
            return false;
        }

        const CPLStringList aosExts(CSLTokenizeString2(
            poDriver->GetMetadataItem(GDAL_DMD_EXTENSIONS), " ", 0));
        const char *pszExt = aosExts.size() >= 1 ? aosExts[0] : "bin";
        static int nTempFileCounter = 0;
        m_osTempInputFilename = CPLFormFilenameSafe(
            osTempDirname.c_str(),
            CPLSPrintf("input_%d_%d", CPLGetCurrentProcessID(),
                       CPLAtomicInc(&nTempFileCounter)),
            pszExt);

        if (bHasInputOutput)
        {
            m_command.replaceAll("<INPUT-OUTPUT>",
                                 QuoteFilename(m_osTempInputFilename));
            m_osTempOutputFilename = m_osTempInputFilename;
        }
        else
        {
            m_command.replaceAll("<INPUT>",
                                 QuoteFilename(m_osTempInputFilename));
        }

        std::unique_ptr<GDALPipelineStepAlgorithm> poMaterializeStep;
        if (poSrcDS->GetRasterCount() != 0)
        {
            poMaterializeStep =
                std::make_unique<GDALMaterializeRasterAlgorithm>();
        }
        else
        {
            poMaterializeStep =
                std::make_unique<GDALMaterializeVectorAlgorithm>();
        }
        poMaterializeStep->SetInputDataset(poSrcDS);
        poMaterializeStep->GetArg(GDAL_ARG_NAME_OUTPUT_FORMAT)
            ->Set(osDriverName.c_str());
        poMaterializeStep->GetArg(GDAL_ARG_NAME_OUTPUT)
            ->Set(m_osTempInputFilename);
        if (EQUAL(osDriverName.c_str(), "GTIFF"))
            poMaterializeStep->GetArg(GDAL_ARG_NAME_CREATION_OPTION)
                ->Set("COPY_SRC_OVERVIEWS=NO");
        if (!poMaterializeStep->Run() || !poMaterializeStep->Finalize())
        {
            return false;
        }
    }

    // Process <OUTPUT> placeholder
    if (bHasOutput)
    {
        auto poSrcDS = inputDataset.size() == 1
                           ? inputDataset[0].GetDatasetRef()
                           : nullptr;

        const std::string osDriverName =
            !outputFormat.empty()                       ? outputFormat
            : !inputFormats.empty()                     ? inputFormats[0]
            : poSrcDS && poSrcDS->GetRasterCount() != 0 ? "GTiff"
                                                        : "GPKG";

        auto poDriver =
            GetGDALDriverManager()->GetDriverByName(osDriverName.c_str());
        if (!poDriver)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Driver %s not available",
                     osDriverName.c_str());
            return false;
        }

        const CPLStringList aosExts(CSLTokenizeString2(
            poDriver->GetMetadataItem(GDAL_DMD_EXTENSIONS), " ", 0));
        const char *pszExt = aosExts.size() >= 1 ? aosExts[0] : "bin";
        m_osTempOutputFilename = CPLResetExtensionSafe(
            CPLGenerateTempFilenameSafe("output").c_str(), pszExt);

        m_command.replaceAll("<OUTPUT>", QuoteFilename(m_osTempOutputFilename));
    }

    CPLDebug("GDAL", "Execute '%s'", m_command.c_str());

#ifdef _WIN32
    wchar_t *pwszCmmand =
        CPLRecodeToWChar(m_command.c_str(), CPL_ENC_UTF8, CPL_ENC_UCS2);
    FILE *fout = _wpopen(pwszCmmand, L"r");
    CPLFree(pwszCmmand);
#else
    FILE *fout = popen(m_command.c_str(), "r");
#endif
    if (!fout)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot start external command '%s'", m_command.c_str());
        return false;
    }

    // Read child standard output
    std::string s;
    constexpr int BUFFER_SIZE = 1024;
    s.resize(BUFFER_SIZE);
    while (!feof(fout) && !ferror(fout))
    {
        if (fgets(s.data(), BUFFER_SIZE - 1, fout))
        {
            size_t nLineLen = strlen(s.c_str());
            // Remove end-of-line characters
            for (char chEOL : {'\n', '\r'})
            {
                if (nLineLen > 0 && s[nLineLen - 1] == chEOL)
                {
                    --nLineLen;
                    s[nLineLen] = 0;
                }
            }
            if (nLineLen)
            {
                bool bOnlyPrintableChars = true;
                for (size_t i = 0; bOnlyPrintableChars && i < nLineLen; ++i)
                {
                    const char ch = s[i];
                    bOnlyPrintableChars = ch >= 32 || ch == '\t';
                }
                if (bOnlyPrintableChars)
                    CPLDebug("GDAL", "External process: %s", s.c_str());
            }
        }
    }

#ifdef _WIN32
    const int ret = _pclose(fout);
#else
    int ret = pclose(fout);
    if (WIFEXITED(ret))
        ret = WEXITSTATUS(ret);
#endif
    if (ret)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "External command '%s' failed with error code %d",
                 m_command.c_str(), ret);
        return false;
    }

    if (!m_osTempOutputFilename.empty())
    {
        auto poOutDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            m_osTempOutputFilename.c_str(), GDAL_OF_VERBOSE_ERROR));
        if (!poOutDS)
            return false;

        outputDataset.Set(std::move(poOutDS));
    }
    else if (inputDataset.size() == 1)
    {
        // If no output dataset was expected from the external command,
        // reuse the input dataset as the output of this step.
        outputDataset.Set(inputDataset[0].GetDatasetRef());
    }

    return true;
}

GDALExternalRasterOrVectorAlgorithm::~GDALExternalRasterOrVectorAlgorithm() =
    default;

GDALExternalRasterAlgorithm::~GDALExternalRasterAlgorithm() = default;

GDALExternalVectorAlgorithm::~GDALExternalVectorAlgorithm() = default;

//! @endcond
