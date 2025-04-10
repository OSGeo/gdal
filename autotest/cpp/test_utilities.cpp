///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test the C API of utilities as library functions
// Author:   Even Rouault <even.rouault at spatialys.com>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
/*
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_unit_test.h"

#include "cpl_error.h"
#include "cpl_string.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

#include "gtest_include.h"

namespace
{

struct test_utilities : public ::testing::Test
{
};

TEST_F(test_utilities, GDALFootprint)
{
    CPLErrorHandlerPusher oQuietErrors(CPLQuietErrorHandler);
    // Test if (pszDest == nullptr && hDstDS == nullptr)
    EXPECT_EQ(GDALFootprint(/* pszDest = */ nullptr,
                            /* hDstDS = */ nullptr,
                            /* hSrcDataset = */ nullptr,
                            /* psOptionsIn = */ nullptr,
                            /* pbUsageError = */ nullptr),
              nullptr);

    // Test if (hSrcDataset == nullptr)
    EXPECT_EQ(GDALFootprint(/* pszDest = */ "/vsimem/out",
                            /* hDstDS = */ nullptr,
                            /* hSrcDataset = */ nullptr,
                            /* psOptionsIn = */ nullptr,
                            /* pbUsageError = */ nullptr),
              nullptr);

    // Test if (hDstDS != nullptr && psOptionsIn && psOptionsIn->bCreateOutput)
    {
        CPLStringList aosArgv;
        aosArgv.AddString("-of");
        aosArgv.AddString("MEM");
        auto poMemDrv = GetGDALDriverManager()->GetDriverByName("MEM");
        if (poMemDrv)
        {
            auto psOptions = GDALFootprintOptionsNew(aosArgv.List(), nullptr);
            auto poInDS = std::unique_ptr<GDALDataset>(
                poMemDrv->Create("", 0, 0, 0, GDT_Unknown, nullptr));
            auto poOutDS = std::unique_ptr<GDALDataset>(
                poMemDrv->Create("", 0, 0, 0, GDT_Unknown, nullptr));
            EXPECT_EQ(
                GDALFootprint(
                    /* pszDest = */ nullptr,
                    /* hDstDS = */ GDALDataset::ToHandle(poOutDS.get()),
                    /* hSrcDataset = */ GDALDataset::ToHandle(poInDS.get()),
                    /* psOptionsIn = */ psOptions,
                    /* pbUsageError = */ nullptr),
                nullptr);
            GDALFootprintOptionsFree(psOptions);
        }
    }

    // Test if (psOptions == nullptr)
    // and if (poSrcDS->GetRasterCount() == 0)
    {
        auto poMemDrv = GetGDALDriverManager()->GetDriverByName("MEM");
        if (poMemDrv)
        {
            auto poInDS = std::unique_ptr<GDALDataset>(
                poMemDrv->Create("", 0, 0, 0, GDT_Unknown, nullptr));
            auto poOutDS = std::unique_ptr<GDALDataset>(
                poMemDrv->Create("", 0, 0, 0, GDT_Unknown, nullptr));
            EXPECT_EQ(
                GDALFootprint(
                    /* pszDest = */ nullptr,
                    /* hDstDS = */ GDALDataset::ToHandle(poOutDS.get()),
                    /* hSrcDataset = */ GDALDataset::ToHandle(poInDS.get()),
                    /* psOptionsIn = */ nullptr,
                    /* pbUsageError = */ nullptr),
                nullptr);
        }
    }
}

}  // namespace
