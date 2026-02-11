#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for XYZ driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import struct

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("XYZ")

###############################################################################
# Test CreateCopy() of byte.tif


def test_xyz_1():

    tst = gdaltest.GDALTest("XYZ", "byte.tif", 1, 4672)
    tst.testCreateCopy(
        vsimem=1,
        check_gt=(-67.00041667, 0.00083333, 0.0, 50.000416667, 0.0, -0.00083333),
    )


###############################################################################
# Test CreateCopy() of float.img


def test_xyz_2():

    src_ds = gdal.Open("data/hfa/float.img")
    ds = gdal.GetDriverByName("XYZ").CreateCopy(
        "tmp/float.xyz", src_ds, options=["COLUMN_SEPARATOR=,", "ADD_HEADER_LINE=YES"]
    )
    got_cs = ds.GetRasterBand(1).Checksum()
    expected_cs = src_ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.GetDriverByName("XYZ").Delete("tmp/float.xyz")
    assert got_cs == expected_cs or got_cs == 24387


###############################################################################
# Test random access to lines of imagery


def test_xyz_3():

    content = """Y X Z
0 0 65


0 1 66

1 0 67

1 1 68
2 0 69
2 1 70


"""
    gdal.FileFromMemBuffer("/vsimem/grid.xyz", content)
    ds = gdal.Open("/vsimem/grid.xyz")
    buf = ds.ReadRaster(0, 2, 2, 1)
    assert struct.unpack("B" * 2, buf) == (69, 70)
    buf = ds.ReadRaster(0, 1, 2, 1)
    assert struct.unpack("B" * 2, buf) == (67, 68)
    buf = ds.ReadRaster(0, 0, 2, 1)
    assert struct.unpack("B" * 2, buf) == (65, 66)
    buf = ds.ReadRaster(0, 2, 2, 1)
    assert struct.unpack("B" * 2, buf) == (69, 70)
    ds = None
    gdal.Unlink("/vsimem/grid.xyz")


###############################################################################
# Test regularly spaced XYZ but with missing values at beginning and/or end of lines
# and missing value in the middle. And a not so exact spacing


def xyz_4_checkline(ds, i, expected_bytes):
    buf = ds.ReadRaster(0, i, ds.RasterXSize, 1)
    return struct.unpack("B" * ds.RasterXSize, buf) == expected_bytes


def test_xyz_4():

    content = """
440750.001 3751290 1
440809.999 3751290 2

440690 3751170.001 3
440750.001 3751170.001 4
440870 3751170.001 6

440810 3751050 7"""
    gdal.FileFromMemBuffer("/vsimem/grid.xyz", content)
    expected = [(0, 1, 2, 0), (3, 4, 0, 6), (0, 0, 7, 0)]

    ds = gdal.Open("/vsimem/grid.xyz")

    got_gt = ds.GetGeoTransform()
    expected_gt = (440660.0, 60.0, 0.0, 3751350.0, 0.0, -120.0)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-5)

    assert ds.GetRasterBand(1).GetMinimum() == 1
    assert ds.GetRasterBand(1).GetMaximum() == 7
    assert ds.GetRasterBand(1).GetNoDataValue() == 0
    for i in [0, 1, 2, 1, 0, 2, 0, 2, 0, 1, 2]:
        assert xyz_4_checkline(ds, i, expected[i])
    ds = None
    gdal.Unlink("/vsimem/grid.xyz")


###############################################################################
# Test XYZ with only integral values and comma field separator


def test_xyz_5():

    content = """0,1,100
0.5,1,100
1,1,100
0,2,100
0.5,2,100
1,2,100
"""
    gdal.FileFromMemBuffer("/vsimem/grid.xyz", content)

    ds = gdal.Open("/vsimem/grid.xyz")
    assert ds.RasterXSize == 3 and ds.RasterYSize == 2
    got_gt = ds.GetGeoTransform()
    expected_gt = (-0.25, 0.5, 0.0, 0.5, 0.0, 1.0)
    ds = None
    gdal.Unlink("/vsimem/grid.xyz")

    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-5)


###############################################################################
# Test XYZ with comma decimal separator and semi-colon field separator


def test_xyz_6():

    content = """0;1;100
0,5;1;100
1;1;100
0;2;100
0,5;2;100
1;2;100
"""
    gdal.FileFromMemBuffer("/vsimem/grid.xyz", content)

    ds = gdal.Open("/vsimem/grid.xyz")
    assert ds.RasterXSize == 3 and ds.RasterYSize == 2
    got_gt = ds.GetGeoTransform()
    expected_gt = (-0.25, 0.5, 0.0, 0.5, 0.0, 1.0)
    ds = None
    gdal.Unlink("/vsimem/grid.xyz")

    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-5)


###############################################################################
# Test XYZ with not completely equal stepX and stepY


def test_xyz_7():

    content = """y x z
   51.500000  354.483333     54.721
   51.500000  354.516667     54.714
   51.500000  354.550000     54.705
   51.475000  354.483333     54.694
   51.475000  354.516667     54.687
   51.475000  354.550000     54.678
   51.450000  354.483333     54.671
   51.450000  354.516667     54.663
   51.450000  354.550000     54.654
   51.425000  354.483333     54.652
   51.425000  354.516667     54.642
   51.425000  354.550000     54.632
   51.400000  354.483333     54.636
   51.400000  354.516667     54.625
   51.400000  354.550000     54.614
"""
    gdal.FileFromMemBuffer("/vsimem/grid.xyz", content)

    ds = gdal.Open("/vsimem/grid.xyz")
    assert ds.RasterXSize == 3 and ds.RasterYSize == 5
    got_gt = ds.GetGeoTransform()
    expected_gt = (354.46666625, 0.0333335, 0.0, 51.5125, 0.0, -0.025)
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.Unlink("/vsimem/grid.xyz")

    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-8)

    assert cs == 146


###############################################################################
# Test particular case of XYZ file with missed samples (#6934)


def test_xyz_8():

    content = """0 500 50
750 500 100
500 750 150
750 750 200

"""
    gdal.FileFromMemBuffer("/vsimem/grid.xyz", content)

    ds = gdal.Open("/vsimem/grid.xyz")
    assert ds.RasterXSize == 4 and ds.RasterYSize == 2
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.Unlink("/vsimem/grid.xyz")

    assert cs == 35


###############################################################################
# Test case of XYZ file with header file with 3 columns not named X Y Z, and quoted


def test_xyz_9():

    content = """"A"    "B"     "C"
0 0 50
10 0 100
0 10 150
10 10 200
"""

    with gdaltest.tempfile("/vsimem/grid.xyz", content):
        with gdal.quiet_errors():
            ds = gdal.Open("/vsimem/grid.xyz")
        assert ds.RasterXSize == 2 and ds.RasterYSize == 2
        cs = ds.GetRasterBand(1).Checksum()

    assert cs == 22


###############################################################################
# Test case of XYZ file where grid points are organized by columns, ascending Y


def test_xyz_organized_by_columns_int16():

    content = """X Y Z
0 0 50
0 10 100
0 20 150
10 0 200
10 10 250
10 20 300

"""

    with gdaltest.tempfile("/vsimem/grid.xyz", content):
        ds = gdal.Open("/vsimem/grid.xyz")
        assert ds.RasterXSize == 2 and ds.RasterYSize == 3
        assert ds.GetGeoTransform() == (-5.0, 10.0, 0.0, 25.0, 0.0, -10.0)
        assert ds.GetRasterBand(1).DataType == gdal.GDT_Int16
        assert struct.unpack("h" * 6, ds.ReadRaster()) == (150, 300, 100, 250, 50, 200)


###############################################################################
# Test case of XYZ file where grid points are organized by columns, descending Y


def test_xyz_organized_by_columns_float32():

    content = """X Y Z
0 20 50.5
0 10 100
0 0 150
10 20 200
10 10 250
10 0 300

"""

    with gdaltest.tempfile("/vsimem/grid.xyz", content):
        ds = gdal.Open("/vsimem/grid.xyz")
        assert ds.RasterXSize == 2 and ds.RasterYSize == 3
        assert ds.GetGeoTransform() == (-5.0, 10.0, 0.0, 25.0, 0.0, -10.0)
        assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
        assert struct.unpack("f" * 6, ds.ReadRaster()) == (
            50.5,
            200.0,
            100.0,
            250.0,
            150.0,
            300.0,
        )


###############################################################################
# Test case "non-exact" floating point step size (0.1), organized by rows, ascending X


def test_xyz_floating_point_step_organized_by_rows_int16():

    content = """X Y Z
1.1 1.1 50
1.2 1.1 100
1.3 1.1 150
1.1 1.2 200
1.2 1.2 250
1.3 1.2 300
1.1 1.3 350
1.2 1.3 400
1.3 1.3 450

"""

    with gdaltest.tempfile("/vsimem/grid.xyz", content):
        ds = gdal.Open("/vsimem/grid.xyz")
        assert ds.RasterXSize == 3 and ds.RasterYSize == 3
        assert ds.GetGeoTransform() == pytest.approx((1.05, 0.1, 0.0, 1.05, 0, 0.1))
        assert ds.GetRasterBand(1).DataType == gdal.GDT_Int16
        assert struct.unpack("h" * 9, ds.ReadRaster()) == (
            50,
            100,
            150,
            200,
            250,
            300,
            350,
            400,
            450,
        )


###############################################################################
# Test case "non-exact" floating point step size (0.1), organized by columns, ascending Y


def test_xyz_floating_point_step_organized_by_columns_int16():

    content = """X Y Z
1.1 1.1 50
1.1 1.2 100
1.1 1.3 150
1.2 1.1 200
1.2 1.2 250
1.2 1.3 300
1.3 1.1 350
1.3 1.2 400
1.3 1.3 450

"""

    with gdaltest.tempfile("/vsimem/grid.xyz", content):
        ds = gdal.Open("/vsimem/grid.xyz")
        assert ds.RasterXSize == 3 and ds.RasterYSize == 3
        assert ds.GetGeoTransform() == pytest.approx((1.05, 0.1, 0.0, 1.35, 0, -0.1))
        assert ds.GetRasterBand(1).DataType == gdal.GDT_Int16
        assert struct.unpack("h" * 9, ds.ReadRaster()) == (
            150,
            300,
            450,
            100,
            250,
            400,
            50,
            200,
            350,
        )


###############################################################################
# Test case "non-exact" floating point step size (0.1), organized by columns, descending Y


def test_xyz_floating_point_step_organized_by_columns_float32():

    content = """X Y Z
1.1 1.3 50.5
1.1 1.2 100
1.1 1.1 150
1.2 1.3 200
1.2 1.2 250
1.2 1.1 300
1.3 1.3 350
1.3 1.2 400
1.3 1.1 450

"""

    with gdaltest.tempfile("/vsimem/grid.xyz", content):
        ds = gdal.Open("/vsimem/grid.xyz")
        assert ds.RasterXSize == 3 and ds.RasterYSize == 3
        assert ds.GetGeoTransform() == pytest.approx((1.05, 0.1, 0.0, 1.35, 0, -0.1))
        assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
        assert struct.unpack("f" * 9, ds.ReadRaster()) == (
            50.5,
            200.0,
            350.0,
            100.0,
            250.0,
            400.0,
            150.0,
            300.0,
            450.0,
        )


###############################################################################
# Test bug fix for https://github.com/OSGeo/gdal/issues/6736


def test_xyz_looks_like_organized_by_columns_but_is_not():

    content = """X Y Z
371999.50 5806917.50 1
371999.50 5806918.50 2
371998.50 5806919.50 3
371999.50 5806919.50 4
"""

    with gdaltest.tempfile("/vsimem/grid.xyz", content):
        ds = gdal.Open("/vsimem/grid.xyz")
        assert ds.RasterXSize == 2 and ds.RasterYSize == 3
        assert ds.GetGeoTransform() == pytest.approx(
            (371998.0, 1.0, 0.0, 5806917.0, 0.0, 1.0)
        )
        assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
        assert struct.unpack("b" * (2 * 3), ds.ReadRaster()) == (0, 1, 0, 2, 3, 4)


###############################################################################
# Test bug fix for https://github.com/OSGeo/gdal/issues/6736


def test_xyz_looks_like_organized_by_columns_but_is_not_case2():

    content = """X Y Z
395999.50 5807443.50 1
395999.50 5807444.50 2
395999.50 5807445.50 3
395998.50 5807446.50 4
395999.50 5807446.50 5
395998.50 5807447.50 6
395999.50 5807447.50 7
"""

    with gdaltest.tempfile("/vsimem/grid.xyz", content):
        ds = gdal.Open("/vsimem/grid.xyz")
        assert ds.RasterXSize == 2 and ds.RasterYSize == 5
        assert ds.GetGeoTransform() == pytest.approx(
            (395998.0, 1.0, 0.0, 5807443.0, 0.0, 1.0)
        )
        assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
        assert struct.unpack("b" * (2 * 5), ds.ReadRaster()) == (
            0,
            1,
            0,
            2,
            0,
            3,
            4,
            5,
            6,
            7,
        )


###############################################################################
# Test bug fix for https://github.com/OSGeo/gdal/issues/6736


def test_xyz_looks_like_missing_lines():

    content = """X Y Z
98.50 91.50 1
99.50 91.50 2
97.50 92.50 3
98.50 92.50 4
99.50 92.50 5
99.50 93.50 6
98.50 95.50 7
99.50 98.50 8
98.50 99.50 9
99.50 99.50 10
"""

    with gdaltest.tempfile("/vsimem/grid.xyz", content):
        ds = gdal.Open("/vsimem/grid.xyz")
        assert ds.RasterXSize == 3 and ds.RasterYSize == 9
        assert ds.GetGeoTransform() == pytest.approx((97.0, 1.0, 0.0, 91.0, 0.0, 1.0))
        assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
        assert struct.unpack("b" * (3 * 9), ds.ReadRaster()) == (
            0,
            1,
            2,
            3,
            4,
            5,
            0,
            0,
            6,
            0,
            0,
            0,
            0,
            7,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            8,
            0,
            9,
            10,
        )


###############################################################################


def yxzContent():
    content = """0 0 65
0 1 66
1 0 67
1 1 68
2 0 69
2 1 70
"""
    return content


###############################################################################
# Test with open option COLUMN_ORDER. Basic case with YXZ


def test_xyz_column_order_basic_yxz():

    content = yxzContent()

    gdal.FileFromMemBuffer("/vsimem/grid.xyz", content)
    ds = gdal.OpenEx("/vsimem/grid.xyz", open_options=["COLUMN_ORDER=YXZ"])
    assert ds.RasterXSize == 2 and ds.RasterYSize == 3
    buf = ds.ReadRaster(0, 2, 2, 1)
    assert struct.unpack("B" * 2, buf) == (69, 70)
    buf = ds.ReadRaster(0, 1, 2, 1)
    assert struct.unpack("B" * 2, buf) == (67, 68)
    buf = ds.ReadRaster(0, 0, 2, 1)
    assert struct.unpack("B" * 2, buf) == (65, 66)
    buf = ds.ReadRaster(0, 2, 2, 1)
    assert struct.unpack("B" * 2, buf) == (69, 70)
    ds = None
    gdal.Unlink("/vsimem/grid.xyz")


###############################################################################
# Test with open option COLUMN_ORDER. Overrides header


def test_xyz_column_order_overrides_header():

    content = """x y z
""" + yxzContent()

    gdal.FileFromMemBuffer("/vsimem/grid.xyz", content)
    ds = gdal.OpenEx("/vsimem/grid.xyz", open_options=["COLUMN_ORDER=YXZ"])
    assert ds.RasterXSize == 2 and ds.RasterYSize == 3
    buf = ds.ReadRaster(0, 2, 2, 1)
    assert struct.unpack("B" * 2, buf) == (69, 70)
    ds = None
    gdal.Unlink("/vsimem/grid.xyz")


###############################################################################
# Test with open option COLUMN_ORDER. Auto


def test_xyz_column_order_auto():

    content = """y x z
""" + yxzContent()

    gdal.FileFromMemBuffer("/vsimem/grid.xyz", content)
    ds = gdal.OpenEx("/vsimem/grid.xyz", open_options=["COLUMN_ORDER=AUTO"])
    assert ds.RasterXSize == 2 and ds.RasterYSize == 3
    buf = ds.ReadRaster(0, 2, 2, 1)
    assert struct.unpack("B" * 2, buf) == (69, 70)
    ds = None
    gdal.Unlink("/vsimem/grid.xyz")


###############################################################################
# Test with open option COLUMN_ORDER. wrong option


def test_xyz_column_order_wrong_option():

    content = """y x z
""" + yxzContent()

    gdal.FileFromMemBuffer("/vsimem/grid.xyz", content)
    with pytest.raises(Exception):
        gdal.OpenEx("/vsimem/grid.xyz", open_options=["COLUMN_ORDER=WRONG"])
    gdal.Unlink("/vsimem/grid.xyz")


###############################################################################
# Test with open option COLUMN_ORDER. XYZ


def test_xyz_column_order_xyz():

    content = """y x z
""" + yxzContent()

    gdal.FileFromMemBuffer("/vsimem/grid.xyz", content)
    ds = gdal.OpenEx("/vsimem/grid.xyz", open_options=["COLUMN_ORDER=XYZ"])
    assert ds.RasterXSize == 3 and ds.RasterYSize == 2
    ds = None
    gdal.Unlink("/vsimem/grid.xyz")
