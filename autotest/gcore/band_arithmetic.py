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
        return gdal.Band.minimum(R, G)

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
        return gdal.Band.minimum(R, 3, G)

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
        return gdal.Band.minimum(R, 1.5, G)

    res = get()
    assert res.DataType == gdal.GDT_Float32
    assert res.ComputeRasterMinMax(False) == (1.5, 1.5)


def test_band_arithmetic_minimum_error():
    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        gdal.Band.minimum(ds1.GetRasterBand(1), ds2.GetRasterBand(1))

    with pytest.raises(
        Exception,
        match=r"At least one argument should be a band \(or convertible to a band\)",
    ):
        gdal.Band.minimum()


def test_band_arithmetic_maximum():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 3)
        R = ds.GetRasterBand(1)
        R.Fill(2)
        G = ds.GetRasterBand(2)
        G.Fill(4)
        return gdal.Band.maximum(R, G)

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
        return gdal.Band.maximum(R, 3, G)

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
        return gdal.Band.maximum(R, 4.56789012345, G)

    res = get()
    assert res.DataType == gdal.GDT_Float64
    assert res.ComputeRasterMinMax(False) == (4.56789012345, 4.56789012345)


def test_band_arithmetic_maximum_error():
    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        gdal.Band.maximum(ds1.GetRasterBand(1), ds2.GetRasterBand(1))

    with pytest.raises(
        Exception,
        match=r"At least one argument should be a band \(or convertible to a band\)",
    ):
        gdal.Band.maximum()


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
        return gdal.Band.mean(R, G)

    res = get()
    assert res.ComputeRasterMinMax(False) == (3, 3)


def test_band_arithmetic_mean_error():
    ds1 = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds2 = gdal.GetDriverByName("MEM").Create("", 2, 1)
    with pytest.raises(Exception, match="Bands do not have the same dimensions"):
        gdal.Band.mean(ds1.GetRasterBand(1), ds2.GetRasterBand(1))

    with pytest.raises(
        Exception,
        match="At least one band should be passed",
    ):
        gdal.Band.mean()


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
        G = ds.GetRasterBand(2)
        with gdaltest.error_raised(
            gdal.CE_Warning,
            "Some sources have a nodata value, and others none. Ignoring nodata",
        ):
            return R + G

    res = get()
    assert res.GetNoDataValue() is None


def test_band_arithmetic_add_nodata_not_same_value():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
        R = ds.GetRasterBand(1)
        R.SetNoDataValue(1)
        G = ds.GetRasterBand(2)
        G.SetNoDataValue(2)
        with gdaltest.error_raised(
            gdal.CE_Warning,
            "Not all sources have the same nodata value. Ignoring nodata",
        ):
            return R + G

    res = get()
    assert res.GetNoDataValue() is None


def test_band_arithmetic_add_nodata_not_same_value_nan():
    def get():
        ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2, gdal.GDT_Float32)
        R = ds.GetRasterBand(1)
        R.SetNoDataValue(1)
        G = ds.GetRasterBand(2)
        G.SetNoDataValue(float("nan"))
        with gdaltest.error_raised(
            gdal.CE_Warning,
            "Not all sources have the same nodata value. Ignoring nodata",
        ):
            return R + G

    res = get()
    assert res.GetNoDataValue() is None
