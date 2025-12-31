# SPDX-License-Identifier: MIT
# Copyright 2021 Even Rouault

import timeit

from osgeo import gdal

tab_ds = {}
for dt in (
    gdal.GDT_Byte,
    gdal.GDT_Int8,
    gdal.GDT_UInt16,
    gdal.GDT_Int16,
    gdal.GDT_UInt32,
    gdal.GDT_Int32,
    gdal.GDT_UInt64,
    gdal.GDT_Int64,
    gdal.GDT_Float16,
    gdal.GDT_Float32,
    gdal.GDT_Float64,
):
    tab_ds[dt] = gdal.GetDriverByName("MEM").Create("", 10000, 1000, 1, dt)
    tab_ds[dt].GetRasterBand(1).Fill(1)


def test(dt):
    tab_ds[dt].GetRasterBand(1).ComputeRasterMinMax(False)


NITERS = 500
setup = "from osgeo import gdal; from __main__ import test"
print(
    "testByte(): %.3f"
    % timeit.timeit("test(gdal.GDT_Byte)", setup=setup, number=NITERS)
)
print(
    "testInt8(): %.3f"
    % timeit.timeit("test(gdal.GDT_Int8)", setup=setup, number=NITERS)
)
print(
    "testUInt16(): %.3f"
    % timeit.timeit("test(gdal.GDT_UInt16)", setup=setup, number=NITERS)
)
print(
    "testInt16(): %.3f"
    % timeit.timeit("test(gdal.GDT_Int16)", setup=setup, number=NITERS)
)
print(
    "testUInt32(): %.3f"
    % timeit.timeit("test(gdal.GDT_UInt32)", setup=setup, number=NITERS)
)
print(
    "testInt32(): %.3f"
    % timeit.timeit("test(gdal.GDT_Int32)", setup=setup, number=NITERS)
)
print(
    "testUInt64(): %.3f"
    % timeit.timeit("test(gdal.GDT_UInt64)", setup=setup, number=NITERS)
)
print(
    "testInt64(): %.3f"
    % timeit.timeit("test(gdal.GDT_Int64)", setup=setup, number=NITERS)
)
print(
    "testFloat16(): %.3f"
    % timeit.timeit("test(gdal.GDT_Float16)", setup=setup, number=NITERS)
)
print(
    "testFloat32(): %.3f"
    % timeit.timeit("test(gdal.GDT_Float32)", setup=setup, number=NITERS)
)
print(
    "testFloat64(): %.3f"
    % timeit.timeit("test(gdal.GDT_Float64)", setup=setup, number=NITERS)
)
