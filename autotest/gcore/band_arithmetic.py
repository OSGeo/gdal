#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Band arithmetic
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even.rouault@spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import math
import struct

import gdaltest
import pytest

from osgeo import gdal


def test_band_arithmetic_add():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 3)
        R = ds.GetRasterBand(1)
        R.Fill(1)
        G = ds.GetRasterBand(2)
        G.Fill(2)
        return R + G

    res = get()
    assert res.ComputeRasterMinMax(False) == (3, 3)
    assert res.GetNoDataValue() is None


def test_band_arithmetic_add_constant():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 1)
        R = ds.GetRasterBand(1)
        R.Fill(1)
        return R + 2

    res = get()
    assert res.ComputeRasterMinMax(False) == (3, 3)


def test_band_arithmetic_add_constant_to_band():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 1)
        R = ds.GetRasterBand(1)
        R.Fill(1)
        return 2 + R

    res = get()
    assert res.ComputeRasterMinMax(False) == (3, 3)


def test_band_arithmetic_add_error():
    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        ds1.GetRasterBand(1) + ds2.GetRasterBand(1)


def test_band_arithmetic_sub():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 3)
        R = ds.GetRasterBand(1)
        R.Fill(1)
        G = ds.GetRasterBand(2)
        G.Fill(2)
        return R - G

    res = get()
    assert res.ComputeRasterMinMax(False) == (-1, -1)


def test_band_arithmetic_sub_constant():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 1)
        R = ds.GetRasterBand(1)
        R.Fill(1)
        return R - 2

    res = get()
    assert res.ComputeRasterMinMax(False) == (-1, -1)


def test_band_arithmetic_sub_constant_to_band():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 1)
        R = ds.GetRasterBand(1)
        R.Fill(1)
        return 3 - R

    res = get()
    assert res.ComputeRasterMinMax(False) == (2, 2)


def test_band_arithmetic_sub_error():
    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        ds1.GetRasterBand(1) - ds2.GetRasterBand(1)


def test_band_arithmetic_mul():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 3)
        R = ds.GetRasterBand(1)
        R.Fill(2)
        G = ds.GetRasterBand(2)
        G.Fill(3)
        return R * G

    res = get()
    assert res.ComputeRasterMinMax(False) == (6, 6)


def test_band_arithmetic_mul_constant():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 1)
        R = ds.GetRasterBand(1)
        R.Fill(2)
        return R * 3

    res = get()
    assert res.ComputeRasterMinMax(False) == (6, 6)


def test_band_arithmetic_mul_constant_to_band():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 1)
        R = ds.GetRasterBand(1)
        R.Fill(2)
        return 3 * R

    res = get()
    assert res.ComputeRasterMinMax(False) == (6, 6)


def test_band_arithmetic_mul_error():
    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        ds1.GetRasterBand(1) * ds2.GetRasterBand(1)


def test_band_arithmetic_div():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 3)
        R = ds.GetRasterBand(1)
        R.Fill(2)
        G = ds.GetRasterBand(2)
        G.Fill(4)
        return R / G

    res = get()
    assert res.ComputeRasterMinMax(False) == (0.5, 0.5)


def test_band_arithmetic_div_constant():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 1)
        R = ds.GetRasterBand(1)
        R.Fill(4)
        return R / 2

    res = get()
    assert res.ComputeRasterMinMax(False) == (2, 2)


def test_band_arithmetic_div_constant_to_band():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 1)
        R = ds.GetRasterBand(1)
        R.Fill(4)
        return 2 / R

    res = get()
    assert res.ComputeRasterMinMax(False) == (0.5, 0.5)


def test_band_arithmetic_div_error():
    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        ds1.GetRasterBand(1) / ds2.GetRasterBand(1)


def test_band_arithmetic_astype():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    assert ds.GetRasterBand(1).astype(gdal.GDT_Float64).DataType == gdal.GDT_Float64

    with pytest.raises(
        Exception, match=r"GDALRasterBandAsDataType\(GDT_Unknown\) not supported"
    ):
        ds.GetRasterBand(1).astype(gdal.GDT_Unknown)

    with pytest.raises(Exception, match=r"Invalid dt value"):
        assert ds.GetRasterBand(1).astype("wrong")


def test_band_arithmetic_astype_numpy():

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    assert ds.GetRasterBand(1).astype(np.float64).DataType == gdal.GDT_Float64


def test_band_arithmetic_rgb_to_greylevel_subfunc():
    def greylevel():
        ds = gdal.Open("data/rgbsmall.tif")
        R = ds.GetRasterBand(1)
        G = ds.GetRasterBand(2)
        B = ds.GetRasterBand(3)
        return (0.299 * R + 0.587 * G + 0.114 * B).astype(gdal.GDT_Byte)

    assert greylevel().Checksum() == 21466


def test_band_arithmetic_rgb_to_greylevel_with():

    with gdal.Open("data/rgbsmall.tif") as ds:
        R = ds.GetRasterBand(1)
        G = ds.GetRasterBand(2)
        B = ds.GetRasterBand(3)
        greylevel = (0.299 * R + 0.587 * G + 0.114 * B).astype(gdal.GDT_Byte)
        assert greylevel.Checksum() == 21466


def test_band_arithmetic_rgb_to_greylevel_with_error():

    with gdal.Open("data/rgbsmall.tif") as ds:
        R = ds.GetRasterBand(1)
        G = ds.GetRasterBand(2)
        B = ds.GetRasterBand(3)
        greylevel = (0.299 * R + 0.587 * G + 0.114 * B).astype(gdal.GDT_Byte)

    with pytest.raises(Exception):
        greylevel.Checksum()


def test_band_arithmetic_rgb_to_greylevel_using_numpy_array():

    pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    def greylevel():
        ds = gdal.Open("data/rgbsmall.tif")
        R = ds.GetRasterBand(1)
        G = ds.GetRasterBand(2).ReadAsArray()
        B = ds.GetRasterBand(3).ReadAsArray()
        return (0.299 * R + 0.587 * G + 0.114 * B).astype(gdal.GDT_Byte)

    assert greylevel().Checksum() == 21466

    ds = gdal.Open("data/rgbsmall.tif")
    with pytest.raises(Exception, match="numpy array must hold a single band"):
        ds.GetRasterBand(1) + ds.ReadAsArray()


def test_band_arithmetic_minimum():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 3)
        R = ds.GetRasterBand(1)
        R.Fill(2)
        G = ds.GetRasterBand(2)
        G.Fill(4)
        return gdal.minimum(R, G)

    res = get()
    assert res.DataType == gdal.GDT_Byte
    assert res.ComputeRasterMinMax(False) == (2, 2)


def test_band_arithmetic_minimum_with_constant():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 3)
        R = ds.GetRasterBand(1)
        R.Fill(2)
        G = ds.GetRasterBand(2)
        G.Fill(4)
        return gdal.minimum(R, 3, G)

    res = get()
    assert res.DataType == gdal.GDT_Byte
    assert res.ComputeRasterMinMax(False) == (2, 2)


def test_band_arithmetic_minimum_with_constant_bis():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 3)
        R = ds.GetRasterBand(1)
        R.Fill(2)
        G = ds.GetRasterBand(2)
        G.Fill(4)
        return gdal.minimum(R, 1.5, G)

    res = get()
    assert res.DataType == gdal.GDT_Float32
    assert res.ComputeRasterMinMax(False) == (1.5, 1.5)


def test_band_arithmetic_minimum_error():
    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        gdal.minimum(ds1.GetRasterBand(1), ds2.GetRasterBand(1))

    with pytest.raises(
        Exception,
        match=r"At least one argument should be a band \(or convertible to a band\)",
    ):
        gdal.minimum()


def test_band_arithmetic_maximum():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 3)
        R = ds.GetRasterBand(1)
        R.Fill(2)
        G = ds.GetRasterBand(2)
        G.Fill(4)
        return gdal.maximum(R, G)

    res = get()
    assert res.DataType == gdal.GDT_Byte
    assert res.ComputeRasterMinMax(False) == (4, 4)


def test_band_arithmetic_maximum_with_constant():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 3)
        R = ds.GetRasterBand(1)
        R.Fill(2)
        G = ds.GetRasterBand(2)
        G.Fill(4)
        return gdal.maximum(R, 3, G)

    res = get()
    assert res.DataType == gdal.GDT_Byte
    assert res.ComputeRasterMinMax(False) == (4, 4)


def test_band_arithmetic_maximum_with_constant_bis():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 3)
        R = ds.GetRasterBand(1)
        R.Fill(2)
        G = ds.GetRasterBand(2)
        G.Fill(4)
        return gdal.maximum(R, 4.56789012345, G)

    res = get()
    assert res.DataType == gdal.GDT_Float64
    assert res.ComputeRasterMinMax(False) == (4.56789012345, 4.56789012345)


def test_band_arithmetic_maximum_error():
    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        gdal.maximum(ds1.GetRasterBand(1), ds2.GetRasterBand(1))

    with pytest.raises(
        Exception,
        match=r"At least one argument should be a band \(or convertible to a band\)",
    ):
        gdal.maximum()


def test_band_arithmetic_rgb_to_greylevel_vrt(tmp_vsimem):

    with gdal.Open("data/rgbsmall.tif") as ds:
        R = ds.GetRasterBand(1)
        G = ds.GetRasterBand(2)
        B = ds.GetRasterBand(3)
        greylevel = (0.299 * R + 0.587 * G + 0.114 * B).astype(gdal.GDT_Byte)
        gdal.GetDriverByName("VRT").CreateCopy(tmp_vsimem / "out.vrt", greylevel)

    ds = gdal.Open(tmp_vsimem / "out.vrt")
    assert ds.GetRasterBand(1).Checksum() == 21466


def test_band_arithmetic_mean():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 3)
        R = ds.GetRasterBand(1)
        R.Fill(2)
        G = ds.GetRasterBand(2)
        G.Fill(4)
        return gdal.mean(R, G)

    res = get()
    assert res.ComputeRasterMinMax(False) == (3, 3)


def test_band_arithmetic_mean_error():
    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        gdal.mean(ds1.GetRasterBand(1), ds2.GetRasterBand(1))

    with pytest.raises(
        Exception,
        match="At least one band should be passed",
    ):
        gdal.mean()


def test_band_arithmetic_add_nodata():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
        R = ds.GetRasterBand(1)
        R.SetNoDataValue(1)
        R.WriteRaster(0, 0, 2, 1, b"\x01\x02")
        G = ds.GetRasterBand(2)
        G.SetNoDataValue(1)
        G.WriteRaster(0, 0, 2, 1, b"\x03\x04")
        return R + G

    res = get()
    assert res.GetNoDataValue() == 1
    assert res.ReadRaster(buf_type=gdal.GDT_Byte) == b"\x01\x06"


def test_band_arithmetic_add_nodata_nan():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2, gdal.GDT_Float32)
        R = ds.GetRasterBand(1)
        R.SetNoDataValue(float("nan"))
        G = ds.GetRasterBand(2)
        G.SetNoDataValue(float("nan"))
        return R + G

    res = get()
    assert math.isnan(res.GetNoDataValue())


def test_band_arithmetic_add_nodata_not_all_bands():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
        R = ds.GetRasterBand(1)
        R.SetNoDataValue(1)
        R.WriteRaster(0, 0, 2, 1, b"\x01\x02")
        G = ds.GetRasterBand(2)
        G.WriteRaster(0, 0, 2, 1, b"\x01\x03")
        return R + G

    res = get()
    assert math.isnan(res.GetNoDataValue())
    got_data = struct.unpack("f" * 2, res.ReadRaster())
    assert math.isnan(got_data[0])
    assert got_data[1] == 2 + 3


def test_band_arithmetic_add_nodata_not_same_value():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 3, 1, 2)
        R = ds.GetRasterBand(1)
        R.SetNoDataValue(1)
        R.WriteRaster(0, 0, 3, 1, b"\x01\x02\x03")
        G = ds.GetRasterBand(2)
        G.SetNoDataValue(2)
        G.WriteRaster(0, 0, 3, 1, b"\x01\x02\x04")
        return R + G

    res = get()
    assert math.isnan(res.GetNoDataValue())
    got_data = struct.unpack("f" * 3, res.ReadRaster())
    assert math.isnan(got_data[0])
    assert math.isnan(got_data[1])
    assert got_data[2] == 3 + 4


def test_band_arithmetic_add_nodata_not_same_value_nan():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2, gdal.GDT_Float32)
        R = ds.GetRasterBand(1)
        R.SetNoDataValue(1)
        R.WriteRaster(0, 0, 2, 1, struct.pack("f" * 2, 1, 2))
        G = ds.GetRasterBand(2)
        G.SetNoDataValue(float("nan"))
        G.WriteRaster(0, 0, 2, 1, struct.pack("f" * 2, float("nan"), 3))
        return R + G

    res = get()
    assert math.isnan(res.GetNoDataValue())
    got_data = struct.unpack("f" * 2, res.ReadRaster())
    assert math.isnan(got_data[0])
    assert got_data[1] == 2 + 3


def test_band_arithmetic_greater():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
    R = ds.GetRasterBand(1)
    R.Fill(1)
    G = ds.GetRasterBand(2)
    G.Fill(2)

    assert (R > G).ComputeRasterMinMax(False)[0] == 0
    assert (R > R).ComputeRasterMinMax(False)[0] == 0
    assert (G > R).ComputeRasterMinMax(False)[0] == 1
    assert (1 > G).ComputeRasterMinMax(False)[0] == 0
    assert (R > 1).ComputeRasterMinMax(False)[0] == 0
    assert (R > 0).ComputeRasterMinMax(False)[0] == 1


def test_band_arithmetic_greater_error():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        ds1.GetRasterBand(1) > ds2.GetRasterBand(1)


def test_band_arithmetic_greater_or_equal():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
    R = ds.GetRasterBand(1)
    R.Fill(1)
    G = ds.GetRasterBand(2)
    G.Fill(2)

    assert (R >= G).ComputeRasterMinMax(False)[0] == 0
    assert (R >= R).ComputeRasterMinMax(False)[0] == 1
    assert (G >= R).ComputeRasterMinMax(False)[0] == 1
    assert (1 >= R).ComputeRasterMinMax(False)[0] == 1
    assert (0 >= R).ComputeRasterMinMax(False)[0] == 0
    assert (R >= 1).ComputeRasterMinMax(False)[0] == 1
    assert (R >= 2).ComputeRasterMinMax(False)[0] == 0


def test_band_arithmetic_greater_or_equal_error():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        ds1.GetRasterBand(1) >= ds2.GetRasterBand(1)


def test_band_arithmetic_lesser():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
    R = ds.GetRasterBand(1)
    R.Fill(1)
    G = ds.GetRasterBand(2)
    G.Fill(2)

    assert (R < G).ComputeRasterMinMax(False)[0] == 1
    assert (R < R).ComputeRasterMinMax(False)[0] == 0
    assert (G < R).ComputeRasterMinMax(False)[0] == 0
    assert (1 < G).ComputeRasterMinMax(False)[0] == 1
    assert (R < 1).ComputeRasterMinMax(False)[0] == 0
    assert (R < 2).ComputeRasterMinMax(False)[0] == 1


def test_band_arithmetic_lesser_error():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        ds1.GetRasterBand(1) < ds2.GetRasterBand(1)


def test_band_arithmetic_lesser_or_equal():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
    R = ds.GetRasterBand(1)
    R.Fill(1)
    G = ds.GetRasterBand(2)
    G.Fill(2)

    assert (R <= G).ComputeRasterMinMax(False)[0] == 1
    assert (R >= R).ComputeRasterMinMax(False)[0] == 1
    assert (G <= R).ComputeRasterMinMax(False)[0] == 0
    assert (1 <= R).ComputeRasterMinMax(False)[0] == 1
    assert (2 <= R).ComputeRasterMinMax(False)[0] == 0
    assert (R <= 1).ComputeRasterMinMax(False)[0] == 1
    assert (R <= 0).ComputeRasterMinMax(False)[0] == 0


def test_band_arithmetic_lesser_or_equal_error():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        ds1.GetRasterBand(1) <= ds2.GetRasterBand(1)


def test_band_arithmetic_equal():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
    R = ds.GetRasterBand(1)
    R.Fill(1)
    G = ds.GetRasterBand(2)
    G.Fill(2)

    assert (R == G).ComputeRasterMinMax(False)[0] == 0
    assert (R == R).ComputeRasterMinMax(False)[0] == 1
    assert (1 == R).ComputeRasterMinMax(False)[0] == 1
    assert (0 == R).ComputeRasterMinMax(False)[0] == 0
    assert (R == 1).ComputeRasterMinMax(False)[0] == 1
    assert (R == 0).ComputeRasterMinMax(False)[0] == 0


def test_band_arithmetic_equal_error():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        ds1.GetRasterBand(1) == ds2.GetRasterBand(1)


def test_band_arithmetic_not_equal():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
    R = ds.GetRasterBand(1)
    R.Fill(1)
    G = ds.GetRasterBand(2)
    G.Fill(2)

    assert (R != G).ComputeRasterMinMax(False)[0] == 1
    assert (R != R).ComputeRasterMinMax(False)[0] == 0
    assert (1 != R).ComputeRasterMinMax(False)[0] == 0
    assert (0 != R).ComputeRasterMinMax(False)[0] == 1
    assert (R != 1).ComputeRasterMinMax(False)[0] == 0
    assert (R != 0).ComputeRasterMinMax(False)[0] == 1


def test_band_arithmetic_not_equal_error():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        ds1.GetRasterBand(1) != ds2.GetRasterBand(1)


def test_band_arithmetic_ternary():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 3)
    cond = ds.GetRasterBand(1)
    cond.Fill(1)
    then = ds.GetRasterBand(2)
    then.Fill(2)
    elseBand = ds.GetRasterBand(3)
    elseBand.Fill(3)

    assert gdal.where(cond, then, elseBand).DataType == gdal.GDT_Byte
    assert gdal.where(cond, then, elseBand).ComputeRasterMinMax(False)[0] == 2

    assert gdal.where(cond, 10, elseBand).DataType == gdal.GDT_Byte
    assert gdal.where(cond, 10, elseBand).ComputeRasterMinMax(False)[0] == 10

    assert gdal.where(cond, 10, 11).ComputeRasterMinMax(False)[0] == 10

    cond.Fill(0)
    assert gdal.where(cond, then, elseBand).DataType == gdal.GDT_Byte
    assert gdal.where(cond, then, elseBand).ComputeRasterMinMax(False)[0] == 3

    assert gdal.where(cond, then, 11).DataType == gdal.GDT_Byte
    assert gdal.where(cond, then, 11).ComputeRasterMinMax(False)[0] == 11

    assert gdal.where(cond, 10, 11).ComputeRasterMinMax(False)[0] == 11


def test_band_arithmetic_ternary_error():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        gdal.where(ds1.GetRasterBand(1), ds1.GetRasterBand(1), ds2.GetRasterBand(1))
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        gdal.where(ds1.GetRasterBand(1), ds2.GetRasterBand(1), ds1.GetRasterBand(1))


def test_band_arithmetic_logical_and():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
    true_band = ds.GetRasterBand(1)
    true_band.Fill(True)
    false_band = ds.GetRasterBand(2)
    false_band.Fill(False)

    assert gdal.logical_and(true_band, true_band).ComputeRasterMinMax(False)[0] == True
    assert (
        gdal.logical_and(true_band, false_band).ComputeRasterMinMax(False)[0] == False
    )
    assert (
        gdal.logical_and(false_band, true_band).ComputeRasterMinMax(False)[0] == False
    )
    assert (
        gdal.logical_and(false_band, false_band).ComputeRasterMinMax(False)[0] == False
    )

    assert gdal.logical_and(true_band, True).ComputeRasterMinMax(False)[0] == True
    assert gdal.logical_and(true_band, False).ComputeRasterMinMax(False)[0] == False
    assert gdal.logical_and(false_band, True).ComputeRasterMinMax(False)[0] == False
    assert gdal.logical_and(false_band, False).ComputeRasterMinMax(False)[0] == False

    assert gdal.logical_and(True, true_band).ComputeRasterMinMax(False)[0] == True
    assert gdal.logical_and(True, false_band).ComputeRasterMinMax(False)[0] == False
    assert gdal.logical_and(False, true_band).ComputeRasterMinMax(False)[0] == False
    assert gdal.logical_and(False, false_band).ComputeRasterMinMax(False)[0] == False


def test_band_arithmetic_logical_and_error():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        gdal.logical_and(ds1.GetRasterBand(1), ds2.GetRasterBand(1))


def test_band_arithmetic_logical_or():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
    true_band = ds.GetRasterBand(1)
    true_band.Fill(True)
    false_band = ds.GetRasterBand(2)
    false_band.Fill(False)

    assert gdal.logical_or(true_band, true_band).ComputeRasterMinMax(False)[0] == True
    assert gdal.logical_or(true_band, false_band).ComputeRasterMinMax(False)[0] == True
    assert gdal.logical_or(false_band, true_band).ComputeRasterMinMax(False)[0] == True
    assert (
        gdal.logical_or(false_band, false_band).ComputeRasterMinMax(False)[0] == False
    )

    assert gdal.logical_or(true_band, True).ComputeRasterMinMax(False)[0] == True
    assert gdal.logical_or(true_band, False).ComputeRasterMinMax(False)[0] == True
    assert gdal.logical_or(false_band, True).ComputeRasterMinMax(False)[0] == True
    assert gdal.logical_or(false_band, False).ComputeRasterMinMax(False)[0] == False

    assert gdal.logical_or(True, true_band).ComputeRasterMinMax(False)[0] == True
    assert gdal.logical_or(True, false_band).ComputeRasterMinMax(False)[0] == True
    assert gdal.logical_or(False, true_band).ComputeRasterMinMax(False)[0] == True
    assert gdal.logical_or(False, false_band).ComputeRasterMinMax(False)[0] == False


def test_band_arithmetic_logical_or_error():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        gdal.logical_or(ds1.GetRasterBand(1), ds2.GetRasterBand(1))


def test_band_arithmetic_logical_not():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
    true_band = ds.GetRasterBand(1)
    true_band.Fill(True)
    false_band = ds.GetRasterBand(2)
    false_band.Fill(False)

    assert gdal.logical_not(true_band).ComputeRasterMinMax(False)[0] == False
    assert gdal.logical_not(false_band).ComputeRasterMinMax(False)[0] == True


def test_band_arithmetic_abs():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float32)
    ds.GetRasterBand(1).Fill(-1.5)

    assert gdal.abs(ds.GetRasterBand(1)).ComputeRasterMinMax(False)[0] == 1.5


def test_band_arithmetic_sqrt():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float32)
    ds.GetRasterBand(1).Fill(2.25)

    assert gdal.sqrt(ds.GetRasterBand(1)).ComputeRasterMinMax(False)[0] == 1.5


def test_band_arithmetic_log10():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float64)
    ds.GetRasterBand(1).Fill(100)

    assert gdal.log10(ds.GetRasterBand(1)).ComputeRasterMinMax(False)[0] == 2.0


def test_band_arithmetic_log():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float64)
    ds.GetRasterBand(1).Fill(math.exp(1))

    assert gdal.log(ds.GetRasterBand(1)).ComputeRasterMinMax(False)[0] == 1.0


def test_band_arithmetic_pow_cst_band():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ds.GetRasterBand(1).Fill(3)

    assert gdal.pow(2, ds.GetRasterBand(1)).ComputeRasterMinMax(False)[0] == 8.0


def test_band_arithmetic_pow_band_cst():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ds.GetRasterBand(1).Fill(3)

    assert gdal.pow(ds.GetRasterBand(1), 2).ComputeRasterMinMax(False)[0] == 9.0


def test_band_arithmetic_pow_band_band():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    ds.GetRasterBand(1).Fill(2)
    ds.GetRasterBand(2).Fill(3)

    assert (
        gdal.pow(ds.GetRasterBand(1), ds.GetRasterBand(2)).ComputeRasterMinMax(False)[0]
        == 8.0
    )


def test_band_arithmetic_pow_band_band_error():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("Expression dialect muparser is not available")

    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        gdal.pow(ds1.GetRasterBand(1), ds2.GetRasterBand(1))
