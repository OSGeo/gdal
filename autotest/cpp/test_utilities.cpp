///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test the C API of utilities as library functions
// Author:   Even Rouault <even.rouault at spatialys.com>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
/*
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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
        aosArgv.AddString("Memory");
        auto poMemDrv = GetGDALDriverManager()->GetDriverByName("Memory");
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
        auto poMemDrv = GetGDALDriverManager()->GetDriverByName("Memory");
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
