///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test viewshed algorithm
// Author:   Andrew Bell
//
///////////////////////////////////////////////////////////////////////////////
/*
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <algorithm>
#include <array>
#include <utility>

#include <iomanip>

#include "gdal_unit_test.h"

#include "gtest_include.h"

#include "viewshed/viewshed.h"

namespace gdal
{
namespace viewshed
{

namespace
{
using Coord = std::pair<int, int>;
using DatasetPtr = std::unique_ptr<GDALDataset>;
using Transform = std::array<double, 6>;
Transform identity{0, 1, 0, 0, 0, 1};

Options stdOptions(int x, int y)
{
    Options opts;
    opts.observer.x = x;
    opts.observer.y = y;
    opts.outputFilename = "none";
    opts.outputFormat = "mem";
    opts.curveCoeff = 0;

    return opts;
}

Options stdOptions(const Coord &observer)
{
    return stdOptions(observer.first, observer.second);
}

DatasetPtr runViewshed(const int8_t *in, int xlen, int ylen,
                       const Options &opts)
{
    Viewshed v(opts);

    GDALDriver *driver = (GDALDriver *)GDALGetDriverByName("MEM");
    GDALDataset *dataset = driver->Create("", xlen, ylen, 1, GDT_Int8, nullptr);
    EXPECT_TRUE(dataset);
    dataset->SetGeoTransform(identity.data());
    GDALRasterBand *band = dataset->GetRasterBand(1);
    EXPECT_TRUE(band);
    CPLErr err = band->RasterIO(GF_Write, 0, 0, xlen, ylen, (int8_t *)in, xlen,
                                ylen, GDT_Int8, 0, 0, nullptr);
    EXPECT_EQ(err, CE_None);

    EXPECT_TRUE(v.run(band));
    return v.output();
}

DatasetPtr runViewshed(const double *in, const double *sd, int xlen, int ylen,
                       const Options &opts)
{
    Viewshed v(opts);

    GDALDriver *driver = (GDALDriver *)GDALGetDriverByName("MEM");
    GDALDataset *dataset =
        driver->Create("", xlen, ylen, 2, GDT_Float32, nullptr);
    EXPECT_TRUE(dataset);
    dataset->SetGeoTransform(identity.data());
    GDALRasterBand *band = dataset->GetRasterBand(1);
    EXPECT_TRUE(band);
    CPLErr err = band->RasterIO(GF_Write, 0, 0, xlen, ylen, (void *)in, xlen,
                                ylen, GDT_Float64, 0, 0, nullptr);
    EXPECT_EQ(err, CE_None);
    GDALRasterBand *sdBand = dataset->GetRasterBand(2);
    EXPECT_TRUE(sdBand);
    err = sdBand->RasterIO(GF_Write, 0, 0, xlen, ylen, (void *)sd, xlen, ylen,
                           GDT_Float64, 0, 0, nullptr);
    EXPECT_EQ(err, CE_None);

    EXPECT_TRUE(v.run(band, sdBand));
    return v.output();
}

}  // namespace

TEST(Viewshed, min_max_mask)
{
    const int xlen = 15;
    const int ylen = 15;
    std::array<int8_t, xlen * ylen> in;
    in.fill(0);

    SCOPED_TRACE("min_max_mask");
    Options opts(stdOptions(7, 7));
    opts.minDistance = 2;
    opts.maxDistance = 6;

    DatasetPtr output = runViewshed(in.data(), xlen, ylen, opts);

    std::array<int8_t, xlen * ylen> out;
    GDALRasterBand *band = output->GetRasterBand(1);

    int xOutLen = band->GetXSize();
    int yOutLen = band->GetYSize();
    EXPECT_EQ(xOutLen, 13);
    EXPECT_EQ(yOutLen, 13);

    CPLErr err = band->RasterIO(GF_Read, 0, 0, xOutLen, yOutLen, out.data(),
                                xOutLen, yOutLen, GDT_Int8, 0, 0, nullptr);
    EXPECT_EQ(err, CE_None);

    //clang-format off
    std::array<int8_t, 13 * 13> expected{
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   127, 0,   0,   0,   0,   0,   0,
        0,   0,   0,   127, 127, 127, 127, 127, 127, 127, 0,   0,   0,
        0,   0,   127, 127, 127, 127, 127, 127, 127, 127, 127, 0,   0,
        0,   127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 0,
        0,   127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 0,
        0,   127, 127, 127, 127, 0,   0,   0,   127, 127, 127, 127, 0,
        127, 127, 127, 127, 127, 0,   0,   0,   127, 127, 127, 127, 127,
        0,   127, 127, 127, 127, 0,   0,   0,   127, 127, 127, 127, 0,
        0,   127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 0,
        0,   127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 0,
        0,   0,   127, 127, 127, 127, 127, 127, 127, 127, 127, 0,   0,
        0,   0,   0,   127, 127, 127, 127, 127, 127, 127, 0,   0,   0};
    //clang-format on

    int8_t *o = out.data();
    int8_t *e = expected.data();
    for (size_t i = 0; i < 13 * 13; ++i)
        EXPECT_EQ(*e++, *o++);

    /**
    int8_t *p = out.data();
    for (int y = 0; y < yOutLen; ++y)
    {
        for (int x = 0; x < xOutLen; ++x)
        {
            char c;
            if (*p == 0)
                c = '*';
            else if (*p == 127)
                c = '.';
            else
                c = '?';
            std::cerr << c;
            p++;
        }
        std::cerr << "\n";
    }
    std::cerr << "\n";
    **/
}

TEST(Viewshed, angle)
{
    const int xlen = 17;
    const int ylen = 17;
    std::array<int8_t, xlen * ylen> in;
    in.fill(0);

    SCOPED_TRACE("min_max_mask");
    Options opts(stdOptions(8, 8));
    opts.startAngle = 0;
    opts.endAngle = 30;

    DatasetPtr output = runViewshed(in.data(), xlen, ylen, opts);

    std::array<int8_t, xlen * ylen> out;
    GDALRasterBand *band = output->GetRasterBand(1);

    int xOutLen = band->GetXSize();
    int yOutLen = band->GetYSize();
    EXPECT_EQ(xOutLen, 6);
    EXPECT_EQ(yOutLen, 9);
    CPLErr err = band->RasterIO(GF_Read, 0, 0, xOutLen, yOutLen, out.data(),
                                xOutLen, yOutLen, GDT_Int8, 0, 0, nullptr);
    EXPECT_EQ(err, CE_None);

    // clang-format off
    std::array<int8_t, 6 * 9> expected{
        127, 127, 127, 127, 127, 127,
        127, 127, 127, 127, 127, 0,
        127, 127, 127, 127, 0,   0,
        127, 127, 127, 127, 0,   0,
        127, 127, 127, 0,   0,   0,
        127, 127, 127, 0,   0,   0,
        127, 127, 0,   0,   0,   0,
        127, 127, 0,   0,   0,   0,
        127, 0,   0,   0,   0,   0};
    // clang-format on

    int8_t *o = out.data();
    int8_t *e = expected.data();
    for (size_t i = 0; i < 6 * 9; ++i)
        EXPECT_EQ(*e++, *o++);

    /**
    int8_t *p = out.data();
    for (int y = 0; y < yOutLen; ++y)
    {
        for (int x = 0; x < xOutLen; ++x)
        {
            char c;
            if (*p == 0)
                c = '*';
            else if (*p == 127)
                c = '.';
            else
                c = '?';
            std::cerr << c;
            p++;
        }
        std::cerr << "\n";
    }
    std::cerr << "\n";
    **/
}

TEST(Viewshed, angle2)
{
    const int xlen = 11;
    const int ylen = 11;
    std::array<int8_t, xlen * ylen> in;
    in.fill(0);

    SCOPED_TRACE("min_max_mask");
    Options opts(stdOptions(5, 5));
    opts.startAngle = 0;
    opts.endAngle = 300;

    DatasetPtr output = runViewshed(in.data(), xlen, ylen, opts);

    std::array<int8_t, xlen * ylen> out;
    GDALRasterBand *band = output->GetRasterBand(1);

    int xOutLen = band->GetXSize();
    int yOutLen = band->GetYSize();
    EXPECT_EQ(xOutLen, 11);
    EXPECT_EQ(yOutLen, 11);
    CPLErr err = band->RasterIO(GF_Read, 0, 0, xOutLen, yOutLen, out.data(),
                                xOutLen, yOutLen, GDT_Int8, 0, 0, nullptr);
    EXPECT_EQ(err, CE_None);

    // clang-format off
    std::array<int8_t, 11 * 11> expected{
        0,   0,   0,   0,   0,   127, 127, 127, 127, 127, 127,
        0,   0,   0,    0,  0,   127, 127, 127, 127, 127, 127,
        127, 0,   0,   0,   0,   127, 127, 127, 127, 127, 127,
        127, 127, 127, 0,   0,   127, 127, 127, 127, 127, 127,
        127, 127, 127, 127, 0,   127, 127, 127, 127, 127, 127,
        127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
        127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
        127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
        127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
        127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
        127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127
    };
    // clang-format on

    int8_t *o = out.data();
    int8_t *e = expected.data();
    for (size_t i = 0; i < 11 * 11; ++i)
        EXPECT_EQ(*e++, *o++);
}

TEST(Viewshed, high_mask)
{
    const int xlen = 15;
    const int ylen = 15;
    std::array<int8_t, xlen * ylen> in;
    in.fill(0);
    in[15 * 7 + 5] = 1;
    in[15 * 7 + 6] = 3;
    in[15 * 7 + 7] = 5;
    in[15 * 7 + 8] = 7;
    in[15 * 7 + 9] = 7;
    in[15 * 7 + 10] = 7;
    in[15 * 7 + 11] = 7;
    in[15 * 7 + 12] = 12;
    in[15 * 7 + 13] = 6;
    in[15 * 7 + 14] = 15;

    SCOPED_TRACE("high_mask");
    Options opts(stdOptions(3, 7));

    opts.highPitch = 45;
    opts.outOfRangeVal = 2;
    opts.visibleVal = 1;
    opts.invisibleVal = 0;

    DatasetPtr output = runViewshed(in.data(), xlen, ylen, opts);

    std::array<int8_t, xlen * ylen> out;
    GDALRasterBand *band = output->GetRasterBand(1);

    int xOutLen = band->GetXSize();
    int yOutLen = band->GetYSize();
    EXPECT_EQ(xOutLen, 15);
    EXPECT_EQ(yOutLen, 15);
    CPLErr err = band->RasterIO(GF_Read, 0, 0, xOutLen, yOutLen, out.data(),
                                xOutLen, yOutLen, GDT_Int8, 0, 0, nullptr);
    EXPECT_EQ(err, CE_None);

    // clang-format off
    std::array<int8_t, 15 * 15> expected{
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 0, 2, 0, 2,
        1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0
    };
    // clang-format on

    int8_t *o = out.data();
    int8_t *e = expected.data();
    for (int y = 0; y < 15; ++y)
    {
        for (int x = 0; x < 15; ++x)
        {
            EXPECT_EQ(*e, *o) << "X/Y exp/act = " << x << "/" << y << "/"
                              << (int)*e << "/" << (int)*o << "!\n";
            e++;
            o++;
        }
    }
}

TEST(Viewshed, low_mask)
{
    const int xlen = 5;
    const int ylen = 5;
    std::array<int8_t, xlen * ylen> in;
    in.fill(0);
    in[12] = 5;

    SCOPED_TRACE("low_mask");
    Options opts(stdOptions(2, 2));

    opts.lowPitch = -45;
    opts.outputMode = OutputMode::DEM;

    DatasetPtr output = runViewshed(in.data(), xlen, ylen, opts);

    std::array<double, xlen * ylen> out;
    GDALRasterBand *band = output->GetRasterBand(1);

    int xOutLen = band->GetXSize();
    int yOutLen = band->GetYSize();
    CPLErr err = band->RasterIO(GF_Read, 0, 0, xOutLen, yOutLen, out.data(),
                                xOutLen, yOutLen, GDT_Float64, 0, 0, nullptr);
    EXPECT_EQ(err, CE_None);

    std::array<double, 5 * 5> expected{2.17157, 2.76393, 3, 2.76393, 2.17157,
                                       2.76393, 3.58579, 4, 3.58579, 2.76393,
                                       3,       4,       5, 4,       3,
                                       2.76393, 3.58579, 4, 3.58579, 2.76393,
                                       2.17157, 2.76393, 3, 2.76393, 2.17157};

    const double *o = out.data();
    const double *e = expected.data();
    for (size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_NEAR(*o, *e, .00001);
        o++;
        e++;
    }
}

TEST(Viewshed, all_visible)
{
    // clang-format off
    const int xlen = 3;
    const int ylen = 3;
    std::array<int8_t, xlen * ylen> in
    {
        1, 2, 3,
        4, 5, 6,
        3, 2, 1
    };
    // clang-format on

    SCOPED_TRACE("all_visible");
    DatasetPtr output = runViewshed(in.data(), xlen, ylen, stdOptions(1, 1));

    std::array<uint8_t, xlen * ylen> out;
    GDALRasterBand *band = output->GetRasterBand(1);
    CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(), xlen,
                                ylen, GDT_Int8, 0, 0, nullptr);
    EXPECT_EQ(err, CE_None);

    for (uint8_t o : out)
        EXPECT_EQ(o, 127);
}

TEST(Viewshed, simple_height)
{
    // clang-format off
    const int xlen = 5;
    const int ylen = 5;
    std::array<int8_t, xlen * ylen> in
    {
        -1, 0, 1, 0, -1,
        -1, 2, 0, 4, -1,
        -1, 1, 0, -1, -1,
         0, 3, 0, 2, 0,
        -1, 0, 0, 3, -1
    };

    std::array<double, xlen * ylen> observable
    {
        4, 2, 1, 4, 8,
        3, 2, 0, 4, 3,
        2, 1, 0, -1, -1,
        4, 3, 0, 2, 1,
        6, 3, 0, 3, 4
    };
    // clang-format on

    {
        SCOPED_TRACE("simple_height:normal");

        DatasetPtr output =
            runViewshed(in.data(), xlen, ylen, stdOptions(2, 2));

        std::array<int8_t, xlen * ylen> out;
        GDALRasterBand *band = output->GetRasterBand(1);
        CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(), xlen,
                                    ylen, GDT_Int8, 0, 0, nullptr);
        EXPECT_EQ(err, CE_None);

        // We expect the cell to be observable when the input is higher than the observable
        // height.
        std::array<int8_t, xlen * ylen> expected;
        for (size_t i = 0; i < in.size(); ++i)
            expected[i] = (in[i] >= observable[i] ? 127 : 0);

        EXPECT_EQ(expected, out);
    }

    {
        std::array<double, xlen * ylen> dem;
        SCOPED_TRACE("simple_height:dem");
        Options opts = stdOptions(2, 2);
        opts.outputMode = OutputMode::DEM;

        DatasetPtr output = runViewshed(in.data(), xlen, ylen, opts);

        GDALRasterBand *band = output->GetRasterBand(1);
        CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, dem.data(), xlen,
                                    ylen, GDT_Float64, 0, 0, nullptr);
        EXPECT_EQ(err, CE_None);

        std::array<double, xlen *ylen> expected = observable;
        // Double equality is fine here as all the values are small integers.
        EXPECT_EQ(dem, expected);
    }

    {
        std::array<double, xlen * ylen> ground;
        SCOPED_TRACE("simple_height:ground");
        Options opts = stdOptions(2, 2);
        opts.outputMode = OutputMode::Ground;
        DatasetPtr output = runViewshed(in.data(), xlen, ylen, opts);

        GDALRasterBand *band = output->GetRasterBand(1);
        CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, ground.data(),
                                    xlen, ylen, GDT_Float64, 0, 0, nullptr);
        EXPECT_EQ(err, CE_None);

        std::array<double, xlen * ylen> expected;
        for (size_t i = 0; i < expected.size(); ++i)
            expected[i] = std::max(0.0, observable[i] - in[i]);

        // Double equality is fine here as all the values are small integers.
        EXPECT_EQ(expected, ground);
    }
}

// Addresses cases in #9501
TEST(Viewshed, dem_vs_ground)
{
    // Run gdal_viewshed on the input 8 x 1 array in both ground and dem mode and
    // verify the results are what are expected.
    auto run = [](const std::array<int8_t, 8> &in, Coord observer,
                  const std::array<double, 8> &ground,
                  const std::array<double, 8> &dem)
    {
        const int xlen = 8;
        const int ylen = 1;

        std::array<double, xlen * ylen> out;
        Options opts = stdOptions(observer);
        opts.outputMode = OutputMode::Ground;

        // Verify ground mode.
        DatasetPtr ds = runViewshed(in.data(), xlen, ylen, opts);
        GDALRasterBand *band = ds->GetRasterBand(1);
        CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(), xlen,
                                    ylen, GDT_Float64, 0, 0, nullptr);
        EXPECT_EQ(err, CE_None);
        for (size_t i = 0; i < ground.size(); ++i)
            EXPECT_DOUBLE_EQ(out[i], ground[i]);

        // Verify DEM mode.
        opts.outputMode = OutputMode::DEM;
        ds = runViewshed(in.data(), xlen, ylen, opts);
        band = ds->GetRasterBand(1);
        err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(), xlen, ylen,
                             GDT_Float64, 0, 0, nullptr);
        EXPECT_EQ(err, CE_None);
        for (size_t i = 0; i < dem.size(); ++i)
            EXPECT_DOUBLE_EQ(out[i], dem[i]);
    };

    // Input / Observer / Minimum expected above ground / Minimum expected  above zero (DEM)
    run({0, 0, 0, 1, 0, 0, 0, 0}, {2, 0}, {0, 0, 0, 0, 2, 3, 4, 5},
        {0, 0, 0, 1, 2, 3, 4, 5});
    run({1, 1, 0, 1, 0, 1, 2, 2}, {3, 0}, {0, 0, 0, 0, 0, 0, 0, 1 / 3.0},
        {1, 1, 0, 1, 0, 1, 2, 7 / 3.0});
    run({0, 0, 0, 1, 1, 0, 0, 0}, {0, 0},
        {0, 0, 0, 0, 1 / 3.0, 5 / 3.0, 6 / 3.0, 7 / 3.0},
        {0, 0, 0, 1, 4 / 3.0, 5 / 3.0, 6 / 3.0, 7 / 3.0});
    run({0, 0, 1, 2, 3, 4, 5, 6}, {0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 2, 3, 4, 5, 6});
    run({0, 0, 1, 1, 3, 4, 5, 4}, {0, 0}, {0, 0, 0, .5, 0, 0, 0, 11 / 6.0},
        {0, 0, 1, 1.5, 3, 4, 5, 35 / 6.0});
}

// Test an observer to the right of the raster.
TEST(Viewshed, oor_right)
{
    // clang-format off
    const int xlen = 5;
    const int ylen = 3;
    std::array<int8_t, xlen * ylen> in
    {
        1, 2, 0, 4, 1,
        0, 0, 2, 1, 0,
        1, 0, 0, 3, 3
    };
    // clang-format on

    {
        Options opts = stdOptions(6, 1);
        opts.outputMode = OutputMode::DEM;
        DatasetPtr ds = runViewshed(in.data(), xlen, ylen, opts);
        GDALRasterBand *band = ds->GetRasterBand(1);
        std::array<double, xlen * ylen> out;
        CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(), xlen,
                                    ylen, GDT_Float64, 0, 0, nullptr);
        EXPECT_EQ(err, CE_None);

        // clang-format off
        std::array<double, xlen * ylen> expected
        {
            16 / 3.0, 29 / 6.0, 13 / 3.0, 4, 1,
            3,        2.5,      2,  1, 0,
            13 / 3.0, 23 / 6.0, 10 / 3.0, 3, 3
        };
        // clang-format on

        for (size_t i = 0; i < out.size(); ++i)
            EXPECT_DOUBLE_EQ(out[i], expected[i]);
    }

    {
        Options opts = stdOptions(6, 2);
        opts.outputMode = OutputMode::DEM;
        DatasetPtr ds = runViewshed(in.data(), xlen, ylen, opts);
        GDALRasterBand *band = ds->GetRasterBand(1);
        std::array<double, xlen * ylen> out;
        CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(), xlen,
                                    ylen, GDT_Float64, 0, 0, nullptr);
        EXPECT_EQ(err, CE_None);

        // clang-format off
        std::array<double, xlen * ylen> expected
        {
            26 / 5.0, 17 / 4.0, 11 / 3.0, 4,  1,
            6,        4.5,      3,        1.5, 0,
            9,        7.5,      6,        4.5, 3
        };
        // clang-format on

        for (size_t i = 0; i < out.size(); ++i)
            EXPECT_DOUBLE_EQ(out[i], expected[i]);
    }
}

// Test an observer to the left of the raster.
TEST(Viewshed, oor_left)
{
    // clang-format off
    const int xlen = 5;
    const int ylen = 3;
    std::array<int8_t, xlen * ylen> in
    {
        1, 2, 0, 4, 1,
        0, 0, 2, 1, 0,
        1, 0, 0, 3, 3
    };
    // clang-format on

    {
        Options opts = stdOptions(-2, 1);
        opts.outputMode = OutputMode::DEM;
        DatasetPtr ds = runViewshed(in.data(), xlen, ylen, opts);
        GDALRasterBand *band = ds->GetRasterBand(1);
        std::array<double, xlen * ylen> out;
        CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(), xlen,
                                    ylen, GDT_Float64, 0, 0, nullptr);
        EXPECT_EQ(err, CE_None);

        // clang-format off
        std::array<double, xlen * ylen> expected
        {
            1, 2, 2, 4, 4.5,
            0, 0, 2, 2.5, 3,
            1, 1, 1, 3, 3.5
        };
        // clang-format on

        for (size_t i = 0; i < out.size(); ++i)
            EXPECT_DOUBLE_EQ(out[i], expected[i]);
    }

    {
        Options opts = stdOptions(-2, 2);
        opts.outputMode = OutputMode::DEM;
        DatasetPtr ds = runViewshed(in.data(), xlen, ylen, opts);
        GDALRasterBand *band = ds->GetRasterBand(1);
        std::array<double, xlen * ylen> out;
        CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(), xlen,
                                    ylen, GDT_Float64, 0, 0, nullptr);
        EXPECT_EQ(err, CE_None);

        // clang-format off
        std::array<double, xlen * ylen> expected
        {
            1, 2,   5 / 3.0, 4,   4.2,
            0, .5,  2,       2.5, 3.1,
            1, 1.5, 2,       3,   3.6
        };
        // clang-format on

        for (size_t i = 0; i < out.size(); ++i)
            EXPECT_DOUBLE_EQ(out[i], expected[i]);
    }
}

// Test an observer above the raster
TEST(Viewshed, oor_above)
{
    // clang-format off
    const int xlen = 5;
    const int ylen = 3;
    std::array<int8_t, xlen * ylen> in
    {
        1, 2, 0, 4, 1,
        0, 0, 2, 1, 0,
        1, 0, 0, 3, 3
    };
    // clang-format on

    {
        Options opts = stdOptions(2, -2);
        opts.outputMode = OutputMode::DEM;
        DatasetPtr ds = runViewshed(in.data(), xlen, ylen, opts);
        GDALRasterBand *band = ds->GetRasterBand(1);
        std::array<double, xlen * ylen> out;
        CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(), xlen,
                                    ylen, GDT_Float64, 0, 0, nullptr);

        EXPECT_EQ(err, CE_None);

        // clang-format off
        std::array<double, xlen * ylen> expected
        {
            1,   2,       0,       4,        1,
            2.5, 2,       2,       4,        4.5,
            3,   8 / 3.0, 8 / 3.0, 14 / 3.0, 17 / 3.0
        };
        // clang-format on

        for (size_t i = 0; i < out.size(); ++i)
            EXPECT_DOUBLE_EQ(out[i], expected[i]);
    }

    {
        Options opts = stdOptions(-2, -2);
        opts.outputMode = OutputMode::DEM;
        DatasetPtr ds = runViewshed(in.data(), xlen, ylen, opts);
        GDALRasterBand *band = ds->GetRasterBand(1);
        std::array<double, xlen * ylen> out;
        CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(), xlen,
                                    ylen, GDT_Float64, 0, 0, nullptr);
        EXPECT_EQ(err, CE_None);

        // clang-format off
        std::array<double, xlen * ylen> expected
        {
            1, 2,   0,   4,    1,
            0, 1.5, 2.5, 1.25, 3.15,
            1, 0.5, 2,   3,    3
        };
        // clang-format on

        for (size_t i = 0; i < out.size(); ++i)
            EXPECT_DOUBLE_EQ(out[i], expected[i]);
    }
}

// Test an observer below the raster
TEST(Viewshed, oor_below)
{
    // clang-format off
    const int xlen = 5;
    const int ylen = 3;
    std::array<int8_t, xlen * ylen> in
    {
        1, 2, 0, 4, 1,
        0, 0, 2, 1, 0,
        1, 0, 0, 3, 3
    };
    // clang-format on

    {
        Options opts = stdOptions(2, 4);
        opts.outputMode = OutputMode::DEM;
        DatasetPtr ds = runViewshed(in.data(), xlen, ylen, opts);
        GDALRasterBand *band = ds->GetRasterBand(1);
        std::array<double, xlen * ylen> out;
        CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(), xlen,
                                    ylen, GDT_Float64, 0, 0, nullptr);

        EXPECT_EQ(err, CE_None);

        // clang-format off
        std::array<double, xlen * ylen> expected
        {
            1,    2,  8 / 3.0,  4,  5,
            0.5,  0,  2,        3,  4.5,
            1,    0,  0,        3,  3
        };
        // clang-format on

        for (size_t i = 0; i < out.size(); ++i)
            EXPECT_DOUBLE_EQ(out[i], expected[i]);
    }

    {
        Options opts = stdOptions(6, 4);
        opts.outputMode = OutputMode::DEM;
        DatasetPtr ds = runViewshed(in.data(), xlen, ylen, opts);
        GDALRasterBand *band = ds->GetRasterBand(1);
        std::array<double, xlen * ylen> out;
        CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(), xlen,
                                    ylen, GDT_Float64, 0, 0, nullptr);
        EXPECT_EQ(err, CE_None);

        // clang-format off
        std::array<double, xlen * ylen> expected
        {
            4.2,  6,    6,   4,   1,
            1.35, 2.25, 4.5, 4.5, 0,
            1,    0,    0,   3,   3
        };
        // clang-format on

        for (size_t i = 0; i < out.size(); ++i)
            EXPECT_DOUBLE_EQ(out[i], expected[i]);
    }
}

// Test a handling of SD raster right and left.
TEST(Viewshed, sd)
{
    {
        // clang-format off
        const int xlen = 8;
        const int ylen = 1;
        std::array<double, xlen * ylen> in
        {
            0, 1, 1, 3.1, 1.5, 2.7, 3.7, 7.5
        };

        std::array<double, xlen * ylen> sd
        {
            1, 100, .1, 100, .1, .1, 100, .1
        };
        // clang-format on

        {
            Options opts = stdOptions(0, 0);
            opts.outputMode = OutputMode::Normal;
            DatasetPtr ds = runViewshed(in.data(), sd.data(), xlen, ylen, opts);
            GDALRasterBand *band = ds->GetRasterBand(1);
            std::array<double, xlen * ylen> out;
            CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(),
                                        xlen, ylen, GDT_Float64, 0, 0, nullptr);

            EXPECT_EQ(err, CE_None);

            // clang-format off
            std::array<double, xlen * ylen> expected
            {
                255, 255, 2, 255, 0, 2, 2, 255
            };
            // clang-format on

            for (size_t i = 0; i < out.size(); ++i)
                EXPECT_DOUBLE_EQ(out[i], expected[i])
                    << "Error at position " << i << ".";
        }
    }
    {
        // clang-format off
        const int xlen = 8;
        const int ylen = 1;
        std::array<double, xlen * ylen> in
        {
            7.5, 3.7, 2.7, 1.5, 3.1, 1, 1, 0
        };

        std::array<double, xlen * ylen> sd
        {
            .1, 100, .1, .1, 100, .1, 100, 1
        };
        // clang-format on

        {
            Options opts = stdOptions(7, 0);
            opts.outputMode = OutputMode::Normal;
            DatasetPtr ds = runViewshed(in.data(), sd.data(), xlen, ylen, opts);
            GDALRasterBand *band = ds->GetRasterBand(1);
            std::array<double, xlen * ylen> out;
            CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(),
                                        xlen, ylen, GDT_Float64, 0, 0, nullptr);

            EXPECT_EQ(err, CE_None);

            // clang-format off
            std::array<double, xlen * ylen> expected
            {
                255, 2, 2, 0, 255, 2, 255, 255
            };
            // clang-format on

            for (size_t i = 0; i < out.size(); ++i)
                EXPECT_DOUBLE_EQ(out[i], expected[i])
                    << "Error at position " << i << ".";
        }
    }
}

// Test a handling of SD raster up and down.
TEST(Viewshed, sd_up_down)
{
    // Up.
    {
        // clang-format off
        const int xlen = 1;
        const int ylen = 8;
        std::array<double, xlen * ylen> in
        {
            0, 1, 1, 3.1, 1.5, 2.7, 3.7, 7.5
        };

        std::array<double, xlen * ylen> sd
        {
            1, 100, .1, 100, .1, .1, 100, .1
        };
        // clang-format on

        {
            Options opts = stdOptions(0, 0);
            opts.outputMode = OutputMode::Normal;
            DatasetPtr ds = runViewshed(in.data(), sd.data(), xlen, ylen, opts);
            GDALRasterBand *band = ds->GetRasterBand(1);
            std::array<double, xlen * ylen> out;
            CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(),
                                        xlen, ylen, GDT_Float64, 0, 0, nullptr);

            EXPECT_EQ(err, CE_None);

            // clang-format off
            std::array<double, xlen * ylen> expected
            {
                255, 255, 2, 255, 0, 2, 2, 255
            };
            // clang-format on

            for (size_t i = 0; i < out.size(); ++i)
                EXPECT_DOUBLE_EQ(out[i], expected[i])
                    << "Error at position " << i << ".";
        }
    }
    // Down.
    {
        // clang-format off
        const int xlen = 1;
        const int ylen = 8;
        std::array<double, xlen * ylen> in
        {
            7.5, 3.7, 2.7, 1.5, 3.1, 1, 1, 0
        };

        std::array<double, xlen * ylen> sd
        {
            .1, 100, .1, .1, 100, .1, 100, 1
        };
        // clang-format on

        {
            Options opts = stdOptions(0, 7);
            opts.outputMode = OutputMode::Normal;
            DatasetPtr ds = runViewshed(in.data(), sd.data(), xlen, ylen, opts);
            GDALRasterBand *band = ds->GetRasterBand(1);
            std::array<double, xlen * ylen> out;
            CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(),
                                        xlen, ylen, GDT_Float64, 0, 0, nullptr);

            EXPECT_EQ(err, CE_None);

            // clang-format off
            std::array<double, xlen * ylen> expected
            {
                255, 2, 2, 0, 255, 2, 255, 255
            };
            // clang-format on

            for (size_t i = 0; i < out.size(); ++i)
                EXPECT_DOUBLE_EQ(out[i], expected[i]);
        }
    }
}

// Test SD raster
TEST(Viewshed, sd_2)
{
    // Right, down
    {
        // clang-format off
        const int xlen = 8;
        const int ylen = 2;
        std::array<double, xlen * ylen> in
        {
            0, 1,   1,   3.1, 1.5, 2.7, 3.7, 7.5,  // Row 0
            0, 1.1, 1.4, 3.1, 1.5, 2.7, 3.7, 7.5   // Row 1
        };

        std::array<double, xlen * ylen> sd
        {
            1, 100, .1, 100, .1, .1, 100, .1,  // Row 0
            1, 100, .1, 100, .1, .1, 100, .1   // Row 1
        };
        // clang-format on

        {
            Options opts = stdOptions(0, 0);
            opts.outputMode = OutputMode::Normal;
            DatasetPtr ds = runViewshed(in.data(), sd.data(), xlen, ylen, opts);
            GDALRasterBand *band = ds->GetRasterBand(1);
            std::array<double, xlen * ylen> out;
            CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(),
                                        xlen, ylen, GDT_Float64, 0, 0, nullptr);

            EXPECT_EQ(err, CE_None);

            // clang-format off
            std::array<double, xlen * ylen> expected
            {
                255, 255, 2, 255, 0, 2, 2, 255,
                255, 255, 2, 2,   0, 0, 2, 255
            };
            // clang-format on

            size_t i = 0;
            for (int y = 0; y < ylen; y++)
                for (int x = 0; x < xlen; x++)
                {
                    EXPECT_DOUBLE_EQ(out[i], expected[i])
                        << "Mismatch at (" << x << ", " << y << ")";
                    i++;
                }
        }
    }
    // Right, up
    {
        // clang-format off
        const int xlen = 8;
        const int ylen = 2;
        std::array<double, xlen * ylen> in
        {
            0, 1.1, 1.4, 3.1, 1.5, 2.7, 3.7, 7.5,  // Row 0
            0, 1,   1,   3.1, 1.5, 2.7, 3.7, 7.5   // Row 1
        };

        std::array<double, xlen * ylen> sd
        {
            1, 100, .1, 100, .1, .1, 100, .1,  // Row 0
            1, 100, .1, 100, .1, .1, 100, .1   // Row 1
        };
        // clang-format on

        {
            Options opts = stdOptions(0, 1);
            opts.outputMode = OutputMode::Normal;
            DatasetPtr ds = runViewshed(in.data(), sd.data(), xlen, ylen, opts);
            GDALRasterBand *band = ds->GetRasterBand(1);
            std::array<double, xlen * ylen> out;
            CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(),
                                        xlen, ylen, GDT_Float64, 0, 0, nullptr);

            EXPECT_EQ(err, CE_None);

            // clang-format off
            std::array<double, xlen * ylen> expected
            {
                255, 255, 2, 2,   0, 0, 2, 255,
                255, 255, 2, 255, 0, 2, 2, 255
            };
            // clang-format on

            size_t i = 0;
            for (int y = 0; y < ylen; y++)
                for (int x = 0; x < xlen; x++)
                {
                    EXPECT_DOUBLE_EQ(out[i], expected[i])
                        << "Mismatch at (" << x << ", " << y << ")";
                    i++;
                }
        }
    }

    // Left, down
    {
        // clang-format off
        const int xlen = 8;
        const int ylen = 2;
        std::array<double, xlen * ylen> in
        {
            7.5, 3.7, 2.7, 1.5, 3.1, 1,   1,   0, // Row 0
            7.5, 3.7, 2.7, 1.5, 3.1, 1.4, 1.1, 0  // Row 1
        };

        std::array<double, xlen * ylen> sd
        {
            .1, 100, .1, .1, 100, .1, 100, 1,  // Row 0
            .1, 100, .1, .1, 100, .1, 100, 1   // Row 1
        };
        // clang-format on

        {
            Options opts = stdOptions(7, 0);
            opts.outputMode = OutputMode::Normal;
            DatasetPtr ds = runViewshed(in.data(), sd.data(), xlen, ylen, opts);
            GDALRasterBand *band = ds->GetRasterBand(1);
            std::array<double, xlen * ylen> out;
            CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(),
                                        xlen, ylen, GDT_Float64, 0, 0, nullptr);

            EXPECT_EQ(err, CE_None);

            // clang-format off
            std::array<double, xlen * ylen> expected
            {
                255, 2, 2, 0, 255, 2, 255, 255,  // Row 0
                255, 2, 0, 0, 2,   2, 255, 255   // Row 1
            };
            // clang-format on

            size_t i = 0;
            for (int y = 0; y < ylen; y++)
                for (int x = 0; x < xlen; x++)
                {
                    EXPECT_DOUBLE_EQ(out[i], expected[i])
                        << "Mismatch at (" << x << ", " << y << ")";
                    i++;
                }
        }
    }

    // Left, up
    {
        // clang-format off
        const int xlen = 8;
        const int ylen = 2;
        std::array<double, xlen * ylen> in
        {
            7.5, 3.7, 2.7, 1.5, 3.1, 1.4, 1.1, 0, // Row 0
            7.5, 3.7, 2.7, 1.5, 3.1, 1,   1,   0  // Row 1
        };

        std::array<double, xlen * ylen> sd
        {
            .1, 100, .1, .1, 100, .1, 100, 1,  // Row 0
            .1, 100, .1, .1, 100, .1, 100, 1   // Row 1
        };
        // clang-format on

        {
            Options opts = stdOptions(7, 1);
            opts.outputMode = OutputMode::Normal;
            DatasetPtr ds = runViewshed(in.data(), sd.data(), xlen, ylen, opts);
            GDALRasterBand *band = ds->GetRasterBand(1);
            std::array<double, xlen * ylen> out;
            CPLErr err = band->RasterIO(GF_Read, 0, 0, xlen, ylen, out.data(),
                                        xlen, ylen, GDT_Float64, 0, 0, nullptr);

            EXPECT_EQ(err, CE_None);

            // clang-format off
            std::array<double, xlen * ylen> expected
            {
                255, 2, 0, 0, 2,   2, 255, 255,  // Row 0
                255, 2, 2, 0, 255, 2, 255, 255   // Row 1
            };
            // clang-format on

            size_t i = 0;
            for (int y = 0; y < ylen; y++)
                for (int x = 0; x < xlen; x++)
                {
                    EXPECT_DOUBLE_EQ(out[i], expected[i])
                        << "Mismatch at (" << x << ", " << y << ")";
                    i++;
                }
        }
    }
}

}  // namespace viewshed
}  // namespace gdal
