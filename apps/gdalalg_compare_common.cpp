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

//! @cond Doxygen_Suppress

GDALCompareCommon::GDALCompareCommon() = default;

GDALCompareCommon::~GDALCompareCommon() = default;

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

    VSIVirtualHandleUniquePtr fpRef(VSIFOpenL(poRefDS->GetDescription(), "rb"));
    VSIVirtualHandleUniquePtr fpInput(
        VSIFOpenL(poInputDS->GetDescription(), "rb"));
    if (!fpRef)
    {
        alg->ReportError(
            CE_Warning, CPLE_AppDefined,
            "Reference dataset '%s' is not a file. Skipping binary "
            "file comparison",
            poRefDS->GetDescription());
        return false;
    }

    if (!fpInput)
    {
        alg->ReportError(
            CE_Warning, CPLE_AppDefined,
            "Input dataset '%s' is not a file. Skipping binary file comparison",
            poInputDS->GetDescription());
        return false;
    }

    fpRef->Seek(0, SEEK_END);
    fpInput->Seek(0, SEEK_END);
    const auto nRefSize = fpRef->Tell();
    const auto nInputSize = fpInput->Tell();
    if (nRefSize != nInputSize)
    {
        aosReport.push_back("Reference file has size " +
                            std::to_string(nRefSize) +
                            " bytes, whereas input file has size " +
                            std::to_string(nInputSize) + " bytes.");

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
        const size_t nInputRead = fpInput->Read(abyInput.data(), 1, BUF_SIZE);

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
            aosReport.push_back(
                "Reference file and input file differ at the binary level.");
            return false;
        }
    } while (fpRef->Tell() < nRefSize);

    return true;
}

//! @endcond
