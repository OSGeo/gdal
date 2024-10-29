#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test InterpolateAtPoint functionality
# Author:   Javier Jimenez Shaw <j1@jimenezshaw.com>
#
###############################################################################
# Copyright (c) 2024, Javier Jimenez Shaw <j1@jimenezshaw.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

###############################################################################
# Test some cases of InterpolateAtPoint


def test_interpolateatpoint_1():

    ds = gdal.Open("data/byte.tif")

    got_nearest = ds.GetRasterBand(1).InterpolateAtPoint(
        10, 12, gdal.GRIORA_NearestNeighbour
    )
    assert got_nearest == pytest.approx(173, 1e-6)
    got_bilinear = ds.GetRasterBand(1).InterpolateAtPoint(10, 12, gdal.GRIORA_Bilinear)
    assert got_bilinear == pytest.approx(139.75, 1e-6)
    got_cubic = ds.GetRasterBand(1).InterpolateAtPoint(10, 12, gdal.GRIORA_CubicSpline)
    assert got_cubic == pytest.approx(138.02, 1e-2)
    got_cubic = ds.GetRasterBand(1).InterpolateAtPoint(10, 12, gdal.GRIORA_Cubic)
    assert got_cubic == pytest.approx(145.57, 1e-2)


def test_interpolateatpoint_outofrange():

    ds = gdal.Open("data/byte.tif")
    res = ds.GetRasterBand(1).InterpolateAtPoint(1000, 12, gdal.GRIORA_Bilinear)
    assert res is None
    res = ds.GetRasterBand(1).InterpolateAtPoint(10, 1200, gdal.GRIORA_Bilinear)
    assert res is None


@gdaltest.disable_exceptions()
def test_interpolateatpoint_error():

    ds = gdal.Open("data/byte.tif")
    with gdal.quiet_errors():
        res = ds.GetRasterBand(1).InterpolateAtPoint(10, 12, gdal.GRIORA_Mode)
    assert res is None


@gdaltest.enable_exceptions()
def test_interpolateatpoint_throw():

    ds = gdal.Open("data/byte.tif")
    with pytest.raises(
        Exception,
        match="Only nearest, bilinear, cubic and cubicspline interpolation methods allowed",
    ):
        ds.GetRasterBand(1).InterpolateAtPoint(10, 12, gdal.GRIORA_Mode)


def test_interpolateatpoint_2_bands():

    mem_ds = gdal.GetDriverByName("MEM").Create(
        "", xsize=2, ysize=2, bands=2, eType=gdal.GDT_Float32
    )
    np = pytest.importorskip("numpy")
    # First band with values values
    raster_array_1 = np.array(([10.5, 1.3], [2.4, 3.8]))
    mem_ds.GetRasterBand(1).WriteArray(raster_array_1)
    # Second band with float values, also negative
    raster_array_2 = np.array(([10.5, 1.3], [-2.4, 3.8]))
    mem_ds.GetRasterBand(2).WriteArray(raster_array_2)

    got_bilinear = mem_ds.GetRasterBand(1).InterpolateAtPoint(
        1, 1, gdal.GRIORA_Bilinear
    )
    expected = np.sum(raster_array_1) / 4.0
    assert got_bilinear == pytest.approx(expected, 1e-6)

    got_bilinear = mem_ds.GetRasterBand(2).InterpolateAtPoint(
        1, 1, gdal.GRIORA_Bilinear
    )
    expected = np.sum(raster_array_2) / 4.0
    assert got_bilinear == pytest.approx(expected, 1e-6)


def test_interpolateatpoint_bilinear_several_points():

    mem_ds = gdal.GetDriverByName("MEM").Create(
        "", xsize=3, ysize=2, bands=1, eType=gdal.GDT_Float32
    )
    np = pytest.importorskip("numpy")
    raster_array_1 = np.array(([10.5, 1.3, 0.5], [2.4, 3.8, -1.0]))
    mem_ds.GetRasterBand(1).WriteArray(raster_array_1)

    got_bilinear = mem_ds.GetRasterBand(1).InterpolateAtPoint(
        0.5, 0.5, gdal.GRIORA_Bilinear
    )
    assert got_bilinear == pytest.approx(10.5, 1e-6)

    got_bilinear = mem_ds.GetRasterBand(1).InterpolateAtPoint(
        1.5, 1.5, gdal.GRIORA_Bilinear
    )
    assert got_bilinear == pytest.approx(3.8, 1e-6)

    got_bilinear = mem_ds.GetRasterBand(1).InterpolateAtPoint(
        1.5, 1.5, gdal.GRIORA_Bilinear
    )
    assert got_bilinear == pytest.approx(3.8, 1e-6)

    got_bilinear = mem_ds.GetRasterBand(1).InterpolateAtPoint(
        2, 1, gdal.GRIORA_Bilinear
    )
    assert got_bilinear == pytest.approx((1.3 + 0.5 + 3.8 - 1) / 4, 1e-6)

    # compute the linear intepolation manually
    got_bilinear = mem_ds.GetRasterBand(1).InterpolateAtPoint(
        2.1, 1.2, gdal.GRIORA_Bilinear
    )
    expected = 1.3 * 0.4 * 0.3 + 0.5 * 0.6 * 0.3 + 3.8 * 0.4 * 0.7 - 1 * 0.6 * 0.7
    assert got_bilinear == pytest.approx(expected, 1e-6)


def test_interpolateatpoint_cubicspline_several_points():

    mem_ds = gdal.GetDriverByName("MEM").Create(
        "", xsize=4, ysize=4, bands=1, eType=gdal.GDT_Float32
    )
    np = pytest.importorskip("numpy")
    raster_array_1 = np.array(
        (
            [1.0, 2.0, 1.5, -0.3],
            [1.0, 2.0, 1.5, -0.3],
            [1.0, 2.0, 1.5, -0.3],
            [1.0, 2.0, 1.5, -0.3],
        )
    )
    mem_ds.GetRasterBand(1).WriteArray(raster_array_1)

    # see that cubicspline is not the same as cubic
    # the curve does not have to pass over the sampling points.
    got_cubic = mem_ds.GetRasterBand(1).InterpolateAtPoint(
        1.5, 1.5, gdal.GRIORA_CubicSpline
    )
    assert got_cubic == pytest.approx(1.75, 1e-6)

    got_cubic = mem_ds.GetRasterBand(1).InterpolateAtPoint(
        2.0, 2.0, gdal.GRIORA_CubicSpline
    )
    assert got_cubic == pytest.approx(1.6916666, 1e-6)


def test_interpolateatpoint_cubic_several_points():

    mem_ds = gdal.GetDriverByName("MEM").Create(
        "", xsize=4, ysize=4, bands=1, eType=gdal.GDT_Float32
    )
    np = pytest.importorskip("numpy")
    raster_array_1 = np.array(
        (
            [1.0, 2.0, 1.5, -0.3],
            [1.0, 2.0, 1.5, -0.3],
            [1.0, 2.0, 1.5, -0.3],
            [1.0, 2.0, 1.5, -0.3],
        )
    )
    mem_ds.GetRasterBand(1).WriteArray(raster_array_1)

    got_cubic = mem_ds.GetRasterBand(1).InterpolateAtPoint(0.5, 1.5, gdal.GRIORA_Cubic)
    assert got_cubic == pytest.approx(1.0, 1e-6)

    got_cubic = mem_ds.GetRasterBand(1).InterpolateAtPoint(1.5, 1.5, gdal.GRIORA_Cubic)
    assert got_cubic == pytest.approx(2.0, 1e-6)

    got_cubic = mem_ds.GetRasterBand(1).InterpolateAtPoint(2.5, 1.5, gdal.GRIORA_Cubic)
    assert got_cubic == pytest.approx(1.5, 1e-6)

    got_cubic = mem_ds.GetRasterBand(1).InterpolateAtPoint(3.5, 1.5, gdal.GRIORA_Cubic)
    assert got_cubic == pytest.approx(-0.3, 1e-6)

    got_cubic = mem_ds.GetRasterBand(1).InterpolateAtPoint(2.0, 2.0, gdal.GRIORA_Cubic)
    assert got_cubic == pytest.approx(1.925, 1e-6)

    # cubic may exceed the highest value
    got_cubic = mem_ds.GetRasterBand(1).InterpolateAtPoint(1.6, 1.5, gdal.GRIORA_Cubic)
    assert got_cubic == pytest.approx(2.0166, 1e-6)


def test_interpolateatpoint_at_borders():

    mem_ds = gdal.GetDriverByName("MEM").Create(
        "", xsize=6, ysize=5, bands=1, eType=gdal.GDT_Float32
    )
    np = pytest.importorskip("numpy")
    raster_array_1 = np.array(
        (
            [1, 2, 4, 4, 5, 6],
            [9, 8, 7, 6, 3, 4],
            [4, 7, 6, 2, 1, 3],
            [7, 8, 9, 6, 2, 1],
            [2, 5, 2, 1, 7, 8],
        )
    )
    mem_ds.GetRasterBand(1).WriteArray(raster_array_1)

    # out of borders
    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(-0.01, 0.0, gdal.GRIORA_Bilinear)
    assert res == None

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(6.01, 0.0, gdal.GRIORA_Bilinear)
    assert res == None

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(0.01, -0.01, gdal.GRIORA_Bilinear)
    assert res == None

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(0.01, 5.01, gdal.GRIORA_Bilinear)
    assert res == None

    # near borders bilinear
    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(0.0, 0.0, gdal.GRIORA_Bilinear)
    assert res == pytest.approx(1.0, 1e-6)

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(1.0, 0.0, gdal.GRIORA_Bilinear)
    assert res == pytest.approx(1.5, 1e-6)

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(0.0, 1.0, gdal.GRIORA_Bilinear)
    assert res == pytest.approx(5.0, 1e-6)

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(0.0, 2.0, gdal.GRIORA_Bilinear)
    assert res == pytest.approx(6.5, 1e-6)

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(6.0, 0.0, gdal.GRIORA_Bilinear)
    assert res == pytest.approx(6.0, 1e-6)

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(3.0, 5.0, gdal.GRIORA_Bilinear)
    assert res == pytest.approx(1.5, 1e-6)

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(3.0, 4.6, gdal.GRIORA_Bilinear)
    assert res == pytest.approx(1.5, 1e-6)

    # near borders cubic
    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(0.0, 0.0, gdal.GRIORA_Cubic)
    assert res == pytest.approx(0.4296875, 1e-6)

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(1.0, 0.0, gdal.GRIORA_Cubic)
    assert res == pytest.approx(0.92578125, 1e-6)

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(0.0, 1.0, gdal.GRIORA_Cubic)
    assert res == pytest.approx(5.328125, 1e-6)

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(0.0, 2.0, gdal.GRIORA_Cubic)
    assert res == pytest.approx(6.75, 1e-6)

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(6.0, 0.0, gdal.GRIORA_Cubic)
    assert res == pytest.approx(6.1875, 1e-6)

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(3.0, 5.0, gdal.GRIORA_Cubic)
    assert res == pytest.approx(0.5078125, 1e-6)

    res = mem_ds.GetRasterBand(1).InterpolateAtPoint(3.0, 4.6, gdal.GRIORA_Cubic)
    assert res == pytest.approx(0.6590625, 1e-6)


def test_interpolateatpoint_complex_int():

    ds = gdal.Open("data/complex_int32.tif")

    res = ds.GetRasterBand(1).InterpolateAtPoint(0.0, 0.0, gdal.GRIORA_NearestNeighbour)
    assert res == (-1 + 2j)
    res = ds.GetRasterBand(1).InterpolateAtPoint(5.1, 7.2, gdal.GRIORA_NearestNeighbour)
    assert res == (32 - 32j)
    res = ds.GetRasterBand(1).InterpolateAtPoint(5.5, 7.5, gdal.GRIORA_Bilinear)
    assert res == (32 - 32j)
    res = ds.GetRasterBand(1).InterpolateAtPoint(5.5, 7.5, gdal.GRIORA_Cubic)
    assert res == (32 - 32j)
    res = ds.GetRasterBand(1).InterpolateAtPoint(5.5, 7.5, gdal.GRIORA_CubicSpline)
    assert res == pytest.approx((34.361111 - 37.055555j), 1e-6)


def test_interpolateatpoint_complex_float():

    ds = gdal.Open("data/complex_float32.tif")

    res = ds.GetRasterBand(1).InterpolateAtPoint(0.0, 0.0, gdal.GRIORA_NearestNeighbour)
    assert res == pytest.approx((-1.2027 + 1.60096j), 1e-4)
    res = ds.GetRasterBand(1).InterpolateAtPoint(5.1, 7.2, gdal.GRIORA_NearestNeighbour)
    assert res == pytest.approx((32.031867 - 31.546283j), 1e-4)
    res = ds.GetRasterBand(1).InterpolateAtPoint(5.5, 7.5, gdal.GRIORA_Bilinear)
    assert res == pytest.approx((32.031867 - 31.546283j), 1e-4)
    res = ds.GetRasterBand(1).InterpolateAtPoint(5.5, 7.5, gdal.GRIORA_Cubic)
    assert res == pytest.approx((32.031867 - 31.546283j), 1e-4)
    res = ds.GetRasterBand(1).InterpolateAtPoint(5.5, 7.5, gdal.GRIORA_CubicSpline)
    assert res == pytest.approx((34.433130 - 36.741504j), 1e-4)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_interpolateatpoint_big_complex():
    # The purpose of this test is to check that the algorithm implementation
    # works for bigger values above the first block of 64x64 pixels.
    # To do that we are creating a virtual complex band from an existing RGB image.

    ds = gdal.Open(
        """<VRTDataset rasterXSize="512" rasterYSize="384">
  <VRTRasterBand dataType="CFloat64" band="1" subClass="VRTDerivedRasterBand">
    <Description>Complex</Description>
    <PixelFunctionType>complex</PixelFunctionType>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/quad-lzw-old-style.tif</SourceFilename>
      <SourceBand>3</SourceBand>
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/quad-lzw-old-style.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""
    )
    res = ds.GetRasterBand(1).InterpolateAtPoint(256, 64, gdal.GRIORA_NearestNeighbour)
    assert res == (179.0 + 127.0j)
    res = ds.GetRasterBand(1).InterpolateAtPoint(256, 63, gdal.GRIORA_NearestNeighbour)
    assert res == (182.0 + 127.0j)
    res = ds.GetRasterBand(1).InterpolateAtPoint(255, 64, gdal.GRIORA_NearestNeighbour)
    assert res == (179.0 + 123.0j)
    res = ds.GetRasterBand(1).InterpolateAtPoint(255, 63, gdal.GRIORA_NearestNeighbour)
    assert res == (182.0 + 122.0j)

    res = ds.GetRasterBand(1).InterpolateAtPoint(256, 64, gdal.GRIORA_Bilinear)
    assert (
        res
        == ((179.0 + 127.0j) + (182.0 + 127.0j) + (179.0 + 123.0j) + (182.0 + 122.0j))
        / 4
    )

    res = ds.GetRasterBand(1).InterpolateAtPoint(256.2, 64.3, gdal.GRIORA_Bilinear)
    assert res == pytest.approx((179.6 + 125.74j), 1e-6)
    res = ds.GetRasterBand(1).InterpolateAtPoint(256.2, 64.3, gdal.GRIORA_Cubic)
    assert res == pytest.approx((179.088 + 125.90542j), 1e-6)
    res = ds.GetRasterBand(1).InterpolateAtPoint(256.2, 64.3, gdal.GRIORA_CubicSpline)
    assert res == pytest.approx((179.482666 + 125.512282j), 1e-6)

    res = ds.GetRasterBand(1).InterpolateAtPoint(255.5, 63.5, gdal.GRIORA_Bilinear)
    assert res == pytest.approx((182 + 122j), 1e-6)
    res = ds.GetRasterBand(1).InterpolateAtPoint(255.5, 63.5, gdal.GRIORA_Cubic)
    assert res == pytest.approx((182 + 122j), 1e-6)
