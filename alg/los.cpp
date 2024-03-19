/******************************************************************************
 *
 * Project:  Line of Sight
 * Purpose:  Core algorithm implementation for line of sight algorithms.
 * Author:   Ryan Friedman, ryanfriedman5410+gdal@gmail.com
 *
 ******************************************************************************
 *
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

// Test instructions:
// * cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
// * cmake --build build --target gdal_unit_test -j `nproc`
// * ./build/autotest/cpp/gdal_unit_test --gtest_filter="test_alg.GDALIsLineOfSightVisible*"
// Or, with GDB
// *  gdb ./build/autotest/cpp/gdal_unit_test
// * >>> r --gtest_filter="test_alg.GDALIsLineOfSightVisible*"

#include "cpl_port.h"
#include "gdal_alg.h"


/************************************************************************/
/*                        GDALIsLineOfSightVisible()                    */
/************************************************************************/

/**
 * Check Line of Sight between two points.
 *
 * This algorithm will check line of sight using a 3D bresenham algorithm.
 * Sightlines" published at
 * https://www.researchgate.net/publication/2411280_Efficient_Line-of-Sight_Algorithms_for_Real_Terrain_Data
 *
 *
 * @param hBand The band to read the DEM data from.
 *
 * @return True if the two points are within Line of Sight.
 *
 * @since GDAL 3.X
 */

bool GDALIsLineOfSightVisible(
    GDALRasterBandH hBand, double xA, double yA, double zA, double xB, double yB, double zB, char** papszOptions)
{
    if(hBand == nullptr) {
        return false;
    }


    // Always process as a double regardless of the input type.
    double val;
    const int iXa = static_cast<int>(xA);
    const int iYa = static_cast<int>(yA);

    if(GDALRasterIO(hBand, GF_Read, iXa, iYa, 1, 1, &val, 1, 1, GDT_Float64, 0, 0) != CE_None) {
        return false;
    }
    if (zA < val) {
        return false;
    }

    const int iXb = static_cast<int>(xB);
    const int iYb = static_cast<int>(yB);

    if(GDALRasterIO(hBand, GF_Read, iXb, iYb, 1, 1, &val, 1, 1, GDT_Float64, 0, 0) != CE_None) {
        return false;
    }
    if (zB < val) {
        return false;
    }


    // Perform a preliminary check of the start and end points first to check they are in LOS.
    // ASSERT_TRUE(hBand->RasterIO(GF_Write, 0, 0, 1, 1, &val, 1, 1, GDT_Byte, 1, nullptr, 0, 0, 0, nullptr) == CE_None);

    return true;
}