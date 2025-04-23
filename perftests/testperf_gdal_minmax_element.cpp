/******************************************************************************
 * Project:  GDAL Core
 * Purpose:  Test performance of gdal_minmax_element.hpp
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_minmax_element.hpp"

#include <chrono>
#include <random>

template <class T> void randomFill(T *v, size_t size, bool withNaN = true)
{
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::normal_distribution<> dist{127, 30};
    for (size_t i = 0; i < size; i++)
    {
        v[i] = static_cast<T>(dist(gen));
        if constexpr (std::is_same<T, float>::value ||
                      std::is_same<T, double>::value)
        {
            if (withNaN && (i == 0 || (i > 10 && ((i + 1) % 1024) <= 4)))
                v[i] = std::numeric_limits<float>::quiet_NaN();
        }
    }
}

constexpr size_t SIZE = 10 * 1000 * 1000 + 1;
constexpr int N_ITERS = 1;

template <class T>
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void
benchIntegers(GDALDataType eDT, T noData)
{
    std::vector<T> x;
    x.resize(SIZE);
    randomFill(x.data(), x.size());
    {
        auto start = std::chrono::steady_clock::now();
        int idx = 0;
        for (int i = 0; i < N_ITERS; ++i)
        {
            idx += static_cast<int>(
                gdal::min_element(x.data(), x.size(), eDT, false, 0));
        }
        idx /= N_ITERS;
        printf("min at idx %d (optimized)\n", idx);
        auto end = std::chrono::steady_clock::now();
        printf("-> elapsed=%d\n", static_cast<int>((end - start).count()));
    }
    {
        auto start = std::chrono::steady_clock::now();
        int idx = 0;
        for (int i = 0; i < N_ITERS; ++i)
        {
            idx += static_cast<int>(
                std::distance(x.begin(), std::min_element(x.begin(), x.end())));
        }
        idx /= N_ITERS;
        printf("min at idx %d (using std::min_element)\n", idx);
        auto end = std::chrono::steady_clock::now();
        printf("-> elapsed=%d\n", static_cast<int>((end - start).count()));
    }
    {
        auto start = std::chrono::steady_clock::now();
        int idx = 0;
        for (int i = 0; i < N_ITERS; ++i)
        {
            idx += static_cast<int>(
                gdal::min_element(x.data(), x.size(), eDT, true, noData));
        }
        idx /= N_ITERS;
        printf("min at idx %d (nodata case, optimized)\n", idx);
        auto end = std::chrono::steady_clock::now();
        printf("-> elapsed=%d\n", static_cast<int>((end - start).count()));
    }
    {
        auto start = std::chrono::steady_clock::now();
        int idx = 0;
        for (int i = 0; i < N_ITERS; ++i)
        {
            idx += static_cast<int>(std::distance(
                x.begin(), std::min_element(x.begin(), x.end(),
                                            [noData](T a, T b) {
                                                return b == noData   ? true
                                                       : a == noData ? false
                                                                     : a < b;
                                            })));
        }
        idx /= N_ITERS;
        printf("min at idx %d (nodata case, using std::min_element with "
               "nodata aware comparison)\n",
               idx);
        auto end = std::chrono::steady_clock::now();
        printf("-> elapsed=%d\n", static_cast<int>((end - start).count()));
    }
}

template <class T>
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void
benchFloatingPointsWithNaN(GDALDataType eDT, T noData)
{
    std::vector<T> x;
    x.resize(SIZE);
    randomFill(x.data(), x.size());
    {
        auto start = std::chrono::steady_clock::now();
        int idx = 0;
        for (int i = 0; i < N_ITERS; ++i)
        {
            idx += static_cast<int>(
                gdal::min_element(x.data(), x.size(), eDT, false, 0));
        }
        idx /= N_ITERS;
        printf("min at idx %d (optimized)\n", idx);
        auto end = std::chrono::steady_clock::now();
        printf("-> elapsed=%d\n", static_cast<int>((end - start).count()));
    }
    {
        auto start = std::chrono::steady_clock::now();
        int idx = 0;
        for (int i = 0; i < N_ITERS; ++i)
        {
            idx += static_cast<int>(std::distance(
                x.begin(), std::min_element(x.begin(), x.end(),
                                            [](T a, T b) {
                                                return std::isnan(b)   ? true
                                                       : std::isnan(a) ? false
                                                                       : a < b;
                                            })));
        }
        idx /= N_ITERS;
        printf("min at idx %d (using std::min_element with NaN aware "
               "comparison)\n",
               idx);
        auto end = std::chrono::steady_clock::now();
        printf("-> elapsed=%d\n", static_cast<int>((end - start).count()));
    }
    {
        auto start = std::chrono::steady_clock::now();
        int idx = 0;
        for (int i = 0; i < N_ITERS; ++i)
        {
            idx += static_cast<int>(
                gdal::min_element(x.data(), x.size(), eDT, true, noData));
        }
        idx /= N_ITERS;
        printf("min at idx %d (nodata case, optimized)\n", idx);
        auto end = std::chrono::steady_clock::now();
        printf("-> elapsed=%d\n", static_cast<int>((end - start).count()));
    }
    {
        auto start = std::chrono::steady_clock::now();
        int idx = 0;
        for (int i = 0; i < N_ITERS; ++i)
        {
            idx += static_cast<int>(std::distance(
                x.begin(), std::min_element(x.begin(), x.end(),
                                            [noData](T a, T b)
                                            {
                                                return std::isnan(b)   ? true
                                                       : std::isnan(a) ? false
                                                       : b == noData   ? true
                                                       : a == noData   ? false
                                                                       : a < b;
                                            })));
        }
        idx /= N_ITERS;
        printf("min at idx %d (nodata case, using std::min_element with "
               "nodata aware and NaN aware comparison)\n",
               idx);
        auto end = std::chrono::steady_clock::now();
        printf("-> elapsed=%d\n", static_cast<int>((end - start).count()));
    }
}

template <class T>
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void
benchFloatingPointsWithoutNaN(GDALDataType eDT, T noData)
{
    std::vector<T> x;
    x.resize(SIZE);
    randomFill(x.data(), x.size(), false);
    {
        auto start = std::chrono::steady_clock::now();
        int idx = 0;
        for (int i = 0; i < N_ITERS; ++i)
        {
            idx += static_cast<int>(
                gdal::min_element(x.data(), x.size(), eDT, false, 0));
        }
        idx /= N_ITERS;
        printf("min at idx %d (optimized)\n", idx);
        auto end = std::chrono::steady_clock::now();
        printf("-> elapsed=%d\n", static_cast<int>((end - start).count()));
    }
    {
        auto start = std::chrono::steady_clock::now();
        int idx = 0;
        for (int i = 0; i < N_ITERS; ++i)
        {
            idx += static_cast<int>(
                std::distance(x.begin(), std::min_element(x.begin(), x.end())));
        }
        idx /= N_ITERS;
        printf("min at idx %d (using std::min_element)\n", idx);
        auto end = std::chrono::steady_clock::now();
        printf("-> elapsed=%d\n", static_cast<int>((end - start).count()));
    }
    {
        auto start = std::chrono::steady_clock::now();
        int idx = 0;
        for (int i = 0; i < N_ITERS; ++i)
        {
            idx += static_cast<int>(
                gdal::min_element(x.data(), x.size(), eDT, true, noData));
        }
        idx /= N_ITERS;
        printf("min at idx %d (nodata case, optimized)\n", idx);
        auto end = std::chrono::steady_clock::now();
        printf("-> elapsed=%d\n", static_cast<int>((end - start).count()));
    }
    {
        auto start = std::chrono::steady_clock::now();
        int idx = 0;
        for (int i = 0; i < N_ITERS; ++i)
        {
            idx += static_cast<int>(std::distance(
                x.begin(), std::min_element(x.begin(), x.end(),
                                            [noData](T a, T b) {
                                                return b == noData   ? true
                                                       : a == noData ? false
                                                                     : a < b;
                                            })));
        }
        idx /= N_ITERS;
        printf("min at idx %d (nodata case, using std::min_element with "
               "nodata aware comparison)\n",
               idx);
        auto end = std::chrono::steady_clock::now();
        printf("-> elapsed=%d\n", static_cast<int>((end - start).count()));
    }
}

int main(int /* argc */, char * /* argv */[])
{
    {
        using T = uint8_t;
        constexpr GDALDataType eDT = GDT_Byte;
        printf("uint8:\n");
        benchIntegers<T>(eDT, 0);
    }
    printf("--------------------\n");
    {
        using T = int8_t;
        constexpr GDALDataType eDT = GDT_Int8;
        printf("int8:\n");
        benchIntegers<T>(eDT, 0);
    }
    printf("--------------------\n");
    {
        using T = uint16_t;
        constexpr GDALDataType eDT = GDT_UInt16;
        printf("uint16:\n");
        benchIntegers<T>(eDT, 0);
    }
    printf("--------------------\n");
    {
        using T = int16_t;
        constexpr GDALDataType eDT = GDT_Int16;
        printf("int16:\n");
        benchIntegers<T>(eDT, 0);
    }
    printf("--------------------\n");
    {
        using T = uint32_t;
        constexpr GDALDataType eDT = GDT_UInt32;
        printf("uint32:\n");
        benchIntegers<T>(eDT, 0);
    }
    printf("--------------------\n");
    {
        using T = int32_t;
        constexpr GDALDataType eDT = GDT_Int32;
        printf("int32:\n");
        benchIntegers<T>(eDT, 0);
    }
    printf("--------------------\n");
    {
        using T = float;
        constexpr GDALDataType eDT = GDT_Float32;
        printf("float (*with* NaN):\n");
        benchFloatingPointsWithNaN<T>(eDT, 0);
    }
    printf("--------------------\n");
    {
        using T = float;
        constexpr GDALDataType eDT = GDT_Float32;
        printf("float (without NaN):\n");
        benchFloatingPointsWithoutNaN<T>(eDT, 0);
    }
    printf("--------------------\n");
    {
        using T = double;
        constexpr GDALDataType eDT = GDT_Float64;
        printf("double (*with* NaN):\n");
        benchFloatingPointsWithNaN<T>(eDT, 0);
    }
    printf("--------------------\n");
    {
        using T = double;
        constexpr GDALDataType eDT = GDT_Float64;
        printf("double (without NaN):\n");
        benchFloatingPointsWithoutNaN<T>(eDT, 0);
    }
    return 0;
}
