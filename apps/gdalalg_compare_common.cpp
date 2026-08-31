/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Common code between raster compare and mdim compare
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_compare_common.h"
#include "gdalalgorithm.h"
#include "gdal_dataset.h"
#include "gdal_driver.h"

#include "cpl_vsi_virtual.h"

#include <set>

//! @cond Doxygen_Suppress

GDALCompareCommon::GDALCompareCommon() = default;

GDALCompareCommon::~GDALCompareCommon() = default;

/************************************************************************/
/*                            CompareFile()                             */
/************************************************************************/

static bool CompareFile(std::vector<std::string> &aosReport,
                        const char *pszRefFilename,
                        const char *pszInputFilename)
{
    VSIStatBufL sStatRef;
    if (VSIStatL(pszRefFilename, &sStatRef) != 0)
    {
        aosReport.push_back(std::string("Reference file ")
                                .append(pszRefFilename)
                                .append(" does not exist"));
        return false;
    }

    VSIStatBufL sStatInput;
    if (VSIStatL(pszInputFilename, &sStatInput) != 0)
    {
        aosReport.push_back(std::string("Input file ")
                                .append(pszInputFilename)
                                .append(" does not exist"));
        return false;
    }

    if (VSI_ISDIR(sStatRef.st_mode) && !VSI_ISDIR(sStatInput.st_mode))
    {
        aosReport.push_back(std::string("Reference file ")
                                .append(pszRefFilename)
                                .append(" is a directory, but input file ")
                                .append(pszInputFilename)
                                .append(" is not"));
        return false;
    }
    else if (!VSI_ISDIR(sStatRef.st_mode) && VSI_ISDIR(sStatInput.st_mode))
    {
        aosReport.push_back(std::string("Reference file ")
                                .append(pszRefFilename)
                                .append(" is not a directory, but input file ")
                                .append(pszInputFilename)
                                .append(" is"));
        return false;
    }

    if (VSI_ISDIR(sStatRef.st_mode))
    {
        std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> psDirRef(
            VSIOpenDir(pszRefFilename, -1, nullptr), VSICloseDir);
        if (!psDirRef)
        {
            aosReport.push_back(std::string("Reference directory ")
                                    .append(pszRefFilename)
                                    .append(" cannot be opened"));
            return false;
        }

        std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> psDirInput(
            VSIOpenDir(pszInputFilename, -1, nullptr), VSICloseDir);
        if (!psDirInput)
        {
            aosReport.push_back(std::string("Input directory ")
                                    .append(pszInputFilename)
                                    .append(" cannot be opened"));
            return false;
        }

        std::set<std::string> oSetRefFilenames;
        while (auto psEntryRef = psDirRef->NextDirEntry())
        {
            oSetRefFilenames.insert(psEntryRef->pszName);
            if (!CompareFile(aosReport,
                             std::string(pszRefFilename)
                                 .append("/")
                                 .append(psEntryRef->pszName)
                                 .c_str(),
                             std::string(pszInputFilename)
                                 .append("/")
                                 .append(psEntryRef->pszName)
                                 .c_str()))
            {
                return false;
            }
        }
        while (auto psEntryInput = psDirInput->NextDirEntry())
        {
            if (!cpl::contains(oSetRefFilenames, psEntryInput->pszName))
            {
                aosReport.push_back(
                    std::string("Input file ")
                        .append(psEntryInput->pszName)
                        .append(" does not exist in reference directory"));
                return false;
            }
        }
    }
    else
    {
        VSIVirtualHandleUniquePtr fpRef(VSIFOpenL(pszRefFilename, "rb"));
        VSIVirtualHandleUniquePtr fpInput(VSIFOpenL(pszInputFilename, "rb"));
        if (!fpRef)
        {
            aosReport.push_back(std::string("Reference file '")
                                    .append(pszRefFilename)
                                    .append("' cannot be opened."));
            return false;
        }

        if (!fpInput)
        {
            aosReport.push_back(std::string("Input file '")
                                    .append(pszRefFilename)
                                    .append("' cannot be opened."));
            return false;
        }

        fpRef->Seek(0, SEEK_END);
        fpInput->Seek(0, SEEK_END);
        const auto nRefSize = fpRef->Tell();
        const auto nInputSize = fpInput->Tell();
        if (nRefSize != nInputSize)
        {
            aosReport.push_back(
                std::string("Reference file '")
                    .append(pszRefFilename)
                    .append("' has size ")
                    .append(std::to_string(nRefSize))
                    .append(" bytes, whereas input file has size ")
                    .append(std::to_string(nInputSize))
                    .append(" bytes."));

            return false;
        }

        constexpr size_t BUF_SIZE = 1024 * 1024;
        std::vector<GByte> abyRef(BUF_SIZE);
        std::vector<GByte> abyInput(BUF_SIZE);

        fpRef->Seek(0, SEEK_SET);
        fpInput->Seek(0, SEEK_SET);

        do
        {
            const size_t nRefRead = fpRef->Read(abyRef.data(), 1, BUF_SIZE);
            const size_t nInputRead =
                fpInput->Read(abyInput.data(), 1, BUF_SIZE);

            if (nRefRead != BUF_SIZE && fpRef->Tell() != nRefSize)
            {
                aosReport.push_back("Failed to fully read reference file");
                return false;
            }

            if (nInputRead != BUF_SIZE && fpInput->Tell() != nRefSize)
            {
                aosReport.push_back("Failed to fully read input file");
                return false;
            }

            if (abyRef != abyInput)
            {
                aosReport.push_back("Reference file and input file differ at "
                                    "the binary level.");
                return false;
            }
        } while (fpRef->Tell() < nRefSize);
    }

    return true;
}

/************************************************************************/
/*            GDALRasterCompareAlgorithm::BinaryComparison()            */
/************************************************************************/

/* static */
bool GDALCompareCommon::BinaryComparison(GDALAlgorithm *alg,
                                         std::vector<std::string> &aosReport,
                                         GDALDataset *poRefDS,
                                         GDALDataset *poInputDS)
{
    if (poRefDS->GetDescription()[0] == 0)
    {
        alg->ReportError(
            CE_Warning, CPLE_AppDefined,
            "Reference dataset has no name. Skipping binary file comparison");
        return false;
    }

    auto poRefDrv = poRefDS->GetDriver();
    if (poRefDrv && EQUAL(poRefDrv->GetDescription(), "MEM"))
    {
        alg->ReportError(
            CE_Warning, CPLE_AppDefined,
            "Reference dataset is a in-memory dataset. Skipping binary "
            "file comparison");
        return false;
    }

    if (poInputDS->GetDescription()[0] == 0)
    {
        alg->ReportError(
            CE_Warning, CPLE_AppDefined,
            "Input dataset has no name. Skipping binary file comparison");
        return false;
    }

    auto poInputDrv = poInputDS->GetDriver();
    if (poInputDrv && EQUAL(poInputDrv->GetDescription(), "MEM"))
    {
        alg->ReportError(
            CE_Warning, CPLE_AppDefined,
            "Input dataset is a in-memory dataset. Skipping binary "
            "file comparison");
        return false;
    }

    VSIStatBufL sStat;
    if (VSIStatL(poRefDS->GetDescription(), &sStat) != 0)
    {
        alg->ReportError(
            CE_Warning, CPLE_AppDefined,
            "Reference dataset '%s' is not a file. Skipping binary "
            "file comparison",
            poRefDS->GetDescription());
        return false;
    }

    if (VSIStatL(poInputDS->GetDescription(), &sStat) != 0)
    {
        alg->ReportError(
            CE_Warning, CPLE_AppDefined,
            "Input dataset '%s' is not a file. Skipping binary file comparison",
            poInputDS->GetDescription());
        return false;
    }

    return CompareFile(aosReport, poRefDS->GetDescription(),
                       poInputDS->GetDescription());
}

//! @endcond
