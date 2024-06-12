///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test general OGR features.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
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

#include <filesystem>

#include "gdal_unit_test.h"

#include "gtest_include.h"

#include "viewshed.h"

namespace gdal
{

namespace
{
// clang-format off
std::array<double, 100> test1{
    2, 2, 2, 7, 7, 2, 7, 2, 7, 7,
    1, 1, 4, 6, 8, 10, 4, 6, 8, 10,
    1, 2, 3, 4, 5, 6, 7, 8, 8, 10,
    1, 2, 3, 4, 5, 4, 3, 3, 2, 2,
    1, 1, 6, 6, 2, 3, 5, 10, 5, 5,
    3, 3, 3, 6, 6, 5, 5, 10, 5, 5,
    2, 2, 2, 3, 4, 4, 4, 5, 5, 5,
    9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    4, 8, 6, 3, 2, 2, 8, 7, 6, 5,
};
// clang-format on
}  // namespace

std::filesystem::path datadir(tut::common::data_basedir);

TEST(Viewshed, simple)
{
    Viewshed::Options opts;
    opts.observer.x = 1;
    opts.observer.y = .1;
    opts.outputFilename = "none";
    opts.outputFormat = "mem";
    opts.curveCoeff = 0;

    Viewshed v(opts);

    GDALDriver *driver = (GDALDriver *)GDALGetDriverByName("MEM");
    GDALDataset *dataset = driver->Create("", 10, 10, 1, GDT_Byte, nullptr);
    GDALRasterBand *band = dataset->GetRasterBand(1);
    [[maybe_unused]] CPLErr err1 = band->RasterIO(
        GF_Write, 0, 0, 10, 10, test1.data(), 10, 10, GDT_Byte, 0, 0, nullptr);

    /**
    std::filesystem::path input(datadir / "viewshed.grd");
    GDALDataset *dataset = (GDALDataset *)GDALOpen(input.c_str(), GA_ReadOnly);
    EXPECT_NE(dataset, nullptr);
    GDALRasterBand *band = dataset->GetRasterBand(1);
    **/

    ASSERT_TRUE(v.run(band));
    std::unique_ptr<GDALDataset> output = v.output();

    std::array<uint8_t, 100> data;
    band = output->GetRasterBand(1);
    CPLErr err = band->RasterIO(GF_Read, 0, 0, 10, 10, data.data(), 10, 10,
                                GDT_Byte, 0, 0, nullptr);
    EXPECT_EQ(err, CE_None);

    for (int i = 0; i < (int)data.size(); ++i)
    {
        std::cerr << (int)data[i] << " ";
        if ((i + 1) % 10 == 0)
            std::cerr << "\n";
    }
}

}  // namespace gdal
