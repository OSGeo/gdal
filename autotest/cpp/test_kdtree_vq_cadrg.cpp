///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test frmts/nitf/kdtree_vq_cadrg.h
// Author:   Even Rouault <even dot rouault at spatialys.com>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026, T-Kartor
/*
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_unit_test.h"

#include "../../frmts/nitf/kdtree_vq_cadrg.h"

#include "gtest_include.h"

#include <array>

namespace
{
struct test_kdtree_vq_cadrg : public ::testing::Test
{
};

TEST_F(test_kdtree_vq_cadrg, Vector_ColorTableBased4x4Pixels)
{
    std::vector<GByte> r{0,  10, 20,  30,  40,  50,  60,  70,
                         80, 90, 100, 110, 120, 130, 140, 150};
    std::vector<GByte> g{0 + 1,   10 + 1,  20 + 1,  30 + 1, 40 + 1,  50 + 1,
                         60 + 1,  70 + 1,  80 + 1,  90 + 1, 100 + 1, 110 + 1,
                         120 + 1, 130 + 1, 140 + 1, 150 + 1};
    std::vector<GByte> b{0 + 2,   10 + 2,  20 + 2,  30 + 2, 40 + 2,  50 + 2,
                         60 + 2,  70 + 2,  80 + 2,  90 + 2, 100 + 2, 110 + 2,
                         120 + 2, 130 + 2, 140 + 2, 150 + 2};
    ColorTableBased4x4Pixels colorTable(r, g, b);

    std::array<GByte, 16> array{0, 1, 2,  3,  4,  5,  6,  7,
                                8, 9, 10, 11, 12, 13, 14, 15};
    Vector<ColorTableBased4x4Pixels> v1(array);
    EXPECT_EQ(v1.val(0), 0);
    EXPECT_EQ(v1.val(15), 15);
    EXPECT_TRUE(memcmp(v1.vals(), array.data(), 16) == 0);
    const Vector<ColorTableBased4x4Pixels> &v1_const = v1;
    EXPECT_EQ(v1_const.vals(), array);
    EXPECT_EQ(v1.get(0, colorTable), 0);
    EXPECT_EQ(v1.get(15, colorTable), 150);
    EXPECT_EQ(v1.get(16, colorTable), 1);
    EXPECT_EQ(v1.get(31, colorTable), 151);
    EXPECT_EQ(v1.get(32, colorTable), 2);
    EXPECT_EQ(v1.get(47, colorTable), 152);
    EXPECT_EQ(v1.squared_distance(v1, colorTable), 0);
    for (int i = 0; i < 16; ++i)
    {
        std::array<GByte, 16> array2{0, 1, 2,  3,  4,  5,  6,  7,
                                     8, 9, 10, 11, 12, 13, 14, 15};
        if (i < 15)
            array2[i] += 1;
        else
            array2[i] -= 1;
        Vector<ColorTableBased4x4Pixels> v2(array2);
        EXPECT_EQ(v1.squared_distance(v2, colorTable), 300);
        EXPECT_EQ(v2.squared_distance(v1, colorTable), 300);
    }

    EXPECT_EQ(Vector<ColorTableBased4x4Pixels>::centroid(v1, 1000, v1, 300,
                                                         colorTable),
              v1);

    std::array<GByte, 16> array0{0, 0, 0, 0, 0, 0, 0, 0,
                                 0, 0, 0, 0, 0, 0, 0, 0};
    Vector<ColorTableBased4x4Pixels> v0(array0);
    std::array<GByte, 16> array15{15, 15, 15, 15, 15, 15, 15, 15,
                                  15, 15, 15, 15, 15, 15, 15, 15};
    Vector<ColorTableBased4x4Pixels> v15(array15);
    std::array<GByte, 16> array7{7, 7, 7, 7, 7, 7, 7, 7,
                                 7, 7, 7, 7, 7, 7, 7, 7};
    Vector<ColorTableBased4x4Pixels> v7(array7);
    EXPECT_EQ(
        Vector<ColorTableBased4x4Pixels>::centroid(v0, 10, v15, 10, colorTable),
        v7);
}

TEST_F(test_kdtree_vq_cadrg, kdtree)
{
    std::vector<GByte> r{0,  10, 20,  30,  40,  50,  60,  70,
                         80, 90, 100, 110, 120, 130, 140, 150};
    std::vector<GByte> g{0 + 1,   10 + 1,  20 + 1,  30 + 1, 40 + 1,  50 + 1,
                         60 + 1,  70 + 1,  80 + 1,  90 + 1, 100 + 1, 110 + 1,
                         120 + 1, 130 + 1, 140 + 1, 150 + 1};
    std::vector<GByte> b{0 + 2,   10 + 2,  20 + 2,  30 + 2, 40 + 2,  50 + 2,
                         60 + 2,  70 + 2,  80 + 2,  90 + 2, 100 + 2, 110 + 2,
                         120 + 2, 130 + 2, 140 + 2, 150 + 2};
    ColorTableBased4x4Pixels colorTable(r, g, b);

    PNNKDTree<ColorTableBased4x4Pixels> kdtree;
    std::vector<BucketItem<ColorTableBased4x4Pixels>> vectors;
    for (GByte i = 0; i < 15; ++i)
    {
        vectors.emplace_back(
            Vector<ColorTableBased4x4Pixels>{filled_array<GByte, 16>(i)}, 1,
            std::vector<int>{i});
    }
    EXPECT_EQ(kdtree.insert(std::move(vectors), colorTable), 15);

    EXPECT_EQ(kdtree.cluster(15, 4, colorTable), 4);

    std::vector<BucketItem<ColorTableBased4x4Pixels>> items;
    kdtree.iterateOverLeaves(
        [&items](PNNKDTree<ColorTableBased4x4Pixels> &node)
        {
            for (auto &it : node.bucketItems())
            {
                items.push_back(std::move(it));
            }
        });
    ASSERT_EQ(items.size(), 4U);

    EXPECT_EQ(items[0].m_count, 4);
    const Vector<ColorTableBased4x4Pixels> expect_v0(
        std::array<GByte, 16>{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
    const auto &got_0 = items[0].m_vec;
    EXPECT_EQ(got_0, expect_v0);
    const std::vector<int> indices_0{0, 1, 2, 3};
    EXPECT_EQ(indices_0, items[0].m_origVectorIndices);

    EXPECT_EQ(items[1].m_count, 3);
    const Vector<ColorTableBased4x4Pixels> expect_v1(std::array<GByte, 16>{
        13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13});
    const auto &got_1 = items[1].m_vec;
    EXPECT_EQ(got_1, expect_v1);
    const std::vector<int> indices_1{12, 13, 14};
    EXPECT_EQ(indices_1, items[1].m_origVectorIndices);

    EXPECT_EQ(items[2].m_count, 4);
    const Vector<ColorTableBased4x4Pixels> expect_v2(
        std::array<GByte, 16>{5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5});
    const auto &got_2 = items[2].m_vec;
    EXPECT_EQ(got_2, expect_v2);
    const std::vector<int> indices_2{4, 5, 6, 7};
    EXPECT_EQ(indices_2, items[2].m_origVectorIndices);

    EXPECT_EQ(items[3].m_count, 4);
    const Vector<ColorTableBased4x4Pixels> expect_v3(
        std::array<GByte, 16>{9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9});
    const auto &got_3 = items[3].m_vec;
    EXPECT_EQ(got_3, expect_v3);
    const std::vector<int> indices_3{8, 9, 10, 11};
    EXPECT_EQ(indices_3, items[3].m_origVectorIndices);

#ifdef DEBUG_VERBOSE
    for (const auto &it : items)
    {
        printf("vec: ");
        for (int i = 0; i < 16; ++i)
            printf("%d, ", it.m_vec.val(i));
        printf("\n");
        printf("count: %d\n", it.m_count);
        printf("origVectorIndices: ");
        for (auto idx : it.m_origVectorIndices)
            printf("%d, ", idx);
        printf("\n");
    }
#endif
}

}  // namespace
