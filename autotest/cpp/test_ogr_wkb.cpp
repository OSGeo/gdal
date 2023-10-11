///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test ogr_wkb.h
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

#include "ogr_geometry.h"
#include "ogr_wkb.h"

#include "gtest_include.h"

namespace
{

struct test_ogr_wkb : public ::testing::Test
{
};

}  // namespace

class OGRWKBFixupCounterClockWiseExternalRingFixture
    : public test_ogr_wkb,
      public ::testing::WithParamInterface<
          std::tuple<const char *, const char *, const char *>>
{
  public:
    static std::vector<std::tuple<const char *, const char *, const char *>>
    GetTupleValues()
    {
        return {
            std::make_tuple("MULTIPOLYGON (((0 1,0 0,1 1,0 1),(0.2 0.3,0.2 "
                            "0.8,0.7 0.8,0.2 0.3)))",
                            "MULTIPOLYGON (((0 1,0 0,1 1,0 1),(0.2 0.3,0.2 "
                            "0.8,0.7 0.8,0.2 0.3)))",
                            "MULTIPOLYGON_CCW"),
            std::make_tuple("MULTIPOLYGON (((1 1,0 0,0 1,1 1),(0.2 0.3,0.7 "
                            "0.8,0.2 0.8,0.2 0.3)))",
                            "MULTIPOLYGON (((1 1,0 1,0 0,1 1),(0.2 0.3,0.2 "
                            "0.8,0.7 0.8,0.2 0.3)))",
                            "MULTIPOLYGON_CW"),
            std::make_tuple(
                "MULTIPOLYGON Z (((0 0 10,0 1 10,1 1 10,0 0 10),(0.2 0.3 "
                "10,0.7 0.8 10,0.2 0.8 10,0.2 0.3 10)))",
                "MULTIPOLYGON Z (((0 0 10,1 1 10,0 1 10,0 0 10),(0.2 0.3 "
                "10,0.2 0.8 10,0.7 0.8 10,0.2 0.3 10)))",
                "MULTIPOLYGON_CW_3D"),
            std::make_tuple("MULTIPOLYGON (((0 0,0 0,1 1,1 1,0 1,0 1,0 0)))",
                            "MULTIPOLYGON (((0 0,0 0,1 1,1 1,0 1,0 1,0 0)))",
                            "MULTIPOLYGON_CCW_REPEATED_POINTS"),
            std::make_tuple("MULTIPOLYGON (((0 0,0 0,0 1,0 1,1 1,1 1,0 0)))",
                            "MULTIPOLYGON (((0 0,1 1,1 1,0 1,0 1,0 0,0 0)))",
                            "MULTIPOLYGON_CW_REPEATED_POINTS"),
            std::make_tuple("MULTIPOLYGON EMPTY", "MULTIPOLYGON EMPTY",
                            "MULTIPOLYGON_EMPTY"),
            std::make_tuple("POINT (1 2)", "POINT (1 2)", "POINT"),
        };
    }
};

TEST_P(OGRWKBFixupCounterClockWiseExternalRingFixture, test)
{
    const char *pszInput = std::get<0>(GetParam());
    const char *pszExpected = std::get<1>(GetParam());

    OGRGeometry *poGeom = nullptr;
    OGRGeometryFactory::createFromWkt(pszInput, nullptr, &poGeom);
    ASSERT_TRUE(poGeom != nullptr);
    std::vector<GByte> abyWkb(poGeom->WkbSize());
    poGeom->exportToWkb(wkbNDR, abyWkb.data());
    OGRWKBFixupCounterClockWiseExternalRing(abyWkb.data(), abyWkb.size());
    delete poGeom;
    poGeom = nullptr;
    OGRGeometryFactory::createFromWkb(abyWkb.data(), nullptr, &poGeom);
    ASSERT_TRUE(poGeom != nullptr);
    char *pszWKT = nullptr;
    poGeom->exportToWkt(&pszWKT, wkbVariantIso);
    EXPECT_STREQ(pszWKT, pszExpected);
    CPLFree(pszWKT);
    delete poGeom;
}

INSTANTIATE_TEST_SUITE_P(
    test_ogr_wkb, OGRWKBFixupCounterClockWiseExternalRingFixture,
    ::testing::ValuesIn(
        OGRWKBFixupCounterClockWiseExternalRingFixture::GetTupleValues()),
    [](const ::testing::TestParamInfo<
        OGRWKBFixupCounterClockWiseExternalRingFixture::ParamType> &l_info)
    { return std::get<2>(l_info.param); });
