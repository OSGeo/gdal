///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test GDAL multidim API
// Author:   Even Rouault <even.rouault at spatialys.com>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026, Even Rouault <even.rouault at spatialys.com>
/*
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_unit_test.h"

#include "gdal_multidim_cpp.h"
#include "memdataset.h"

#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace test_gdal_mdim
{

struct test_gdal_mdim : public ::testing::Test
{
};

TEST_F(test_gdal_mdim, RecursivelyVisitArrays)
{
    auto poDS = std::unique_ptr<GDALDataset>(
        MEMDataset::CreateMultiDimensional("", nullptr, nullptr));

    auto poRG = poDS->GetRootGroup();
    auto poArray1 = poRG->CreateMDArray("array1", {},
                                        GDALExtendedDataType::Create(GDT_Byte));
    auto poSubGroup = poRG->CreateGroup("subgroup");
    auto poArray2 = poSubGroup->CreateMDArray(
        "array2", {}, GDALExtendedDataType::Create(GDT_Byte));
    auto poSubGroup2 = poRG->CreateGroup("subgroup2");
    auto poArray3 = poSubGroup2->CreateMDArray(
        "array3", {}, GDALExtendedDataType::Create(GDT_Byte));

    std::vector<std::shared_ptr<GDALMDArray>> visitedArrays;
    poRG->RecursivelyVisitArrays(
        [&visitedArrays](const std::shared_ptr<GDALMDArray> &array)
        { visitedArrays.push_back(array); });

    ASSERT_EQ(visitedArrays.size(), 3U);
    EXPECT_EQ(visitedArrays[0].get(), poArray1.get());
    EXPECT_EQ(visitedArrays[1].get(), poArray2.get());
    EXPECT_EQ(visitedArrays[2].get(), poArray3.get());
}

}  // namespace test_gdal_mdim

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
