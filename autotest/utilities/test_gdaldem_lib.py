#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdaldem testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import collections
import struct

import gdaltest
import pytest

from osgeo import gdal, osr

###############################################################################
# Test incorrect use of scale parameters


@pytest.mark.parametrize(
    "options",
    [
        {"scale": 111120, "xscale": 1},
        {"scale": 111120, "yscale": 1},
        {"xscale": 1},
        {"yscale": 1},
    ],
)
def test_gdaldem_lib_hillshade_incorrect_use_of_scale_params(options):

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    kwargs = {"format": "MEM", "zFactor": 30}
    kwargs.update(options)
    with pytest.raises(Exception):
        gdal.DEMProcessing("", src_ds, "hillshade", **kwargs)


###############################################################################
# Test gdaldem hillshade


@pytest.mark.parametrize(
    "options,checksum",
    [
        ({"scale": 111120}, 45587),
        ({"xscale": 111120, "yscale": 111120}, 45587),
        ({"xscale": 111120, "yscale": 111120 + 1}, 45587),
        ({}, 46008),
    ],
)
def test_gdaldem_lib_hillshade(options, checksum):

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    kwargs = {"format": "MEM", "zFactor": 30}
    kwargs.update(options)
    ds = gdal.DEMProcessing("", src_ds, "hillshade", **kwargs)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == checksum, "Bad checksum"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    assert ds.GetRasterBand(1).GetNoDataValue() == 0, "Bad nodata value"

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem hillshade with projected CRS


@pytest.mark.parametrize("proj_unit", ["m", "ft", "us-ft"])
def test_gdaldem_lib_hillshade_proj_crs(proj_unit):

    src_ds = gdal.Warp(
        "",
        "../gdrivers/data/n43.tif",
        format="MEM",
        dstSRS=f"+proj=tmerc +datum=WGS84 +lon_0=-79.5 +units={proj_unit}",
    )
    ds = gdal.DEMProcessing("", src_ds, "hillshade", format="MEM", zFactor=30)
    assert ds.GetRasterBand(1).Checksum() == 36233


###############################################################################
# Test gdaldem hillshade with projected CRS and different vertical unit


@pytest.mark.parametrize(
    "name,factor",
    [("m", 1), ("unknown", 1), ("ft", 0.3048), ("us-ft", 0.3048006096012192)],
)
def test_gdaldem_lib_hillshade_projected_crs_vertical_units(tmp_path, name, factor):

    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    src_ds = gdal.Warp(
        "",
        "../gdrivers/data/n43.tif",
        outputType=gdal.GDT_Float32,
        format="MEM",
        dstSRS="+proj=tmerc +datum=WGS84 +lon_0=-79.5 +units=m",
        dstNodata=0,
    )
    src_ds.GetRasterBand(1).SetUnitType(name)
    src_ds.GetRasterBand(1).SetNoDataValue(
        src_ds.GetRasterBand(1).GetNoDataValue() / factor
    )
    src_ds.GetRasterBand(1).WriteArray(src_ds.GetRasterBand(1).ReadAsArray() / factor)

    if name == "unknown":
        with gdal.quiet_errors():
            ds = gdal.DEMProcessing("", src_ds, "hillshade", format="MEM", zFactor=30)
    else:
        ds = gdal.DEMProcessing("", src_ds, "hillshade", format="MEM", zFactor=30)
    assert ds.GetRasterBand(1).Checksum() == 36233


###############################################################################
# Test gdaldem hillshade with geographic CRS and different vertical unit


@pytest.mark.parametrize(
    "name,factor",
    [("m", 1), ("unknown", 1), ("ft", 0.3048), ("us-ft", 0.3048006096012192)],
)
def test_gdaldem_lib_hillshade_geographic_crs_vertical_units(name, factor):

    gdaltest.importorskip_gdal_array()
    pytest.importorskip("numpy")

    src_ds = gdal.Translate(
        "", "../gdrivers/data/n43.tif", format="MEM", outputType=gdal.GDT_Float32
    )
    src_ds.GetRasterBand(1).SetUnitType(name)
    src_ds.GetRasterBand(1).SetNoDataValue(
        src_ds.GetRasterBand(1).GetNoDataValue() / factor
    )
    src_ds.GetRasterBand(1).WriteArray(src_ds.GetRasterBand(1).ReadAsArray() / factor)
    if name == "unknown":
        with gdal.quiet_errors():
            ds = gdal.DEMProcessing("", src_ds, "hillshade", format="MEM", zFactor=30)
    else:
        ds = gdal.DEMProcessing("", src_ds, "hillshade", format="MEM", zFactor=30)
    assert ds.GetRasterBand(1).Checksum() == 46008


###############################################################################
# Test gdaldem hillshade with source being floating point


def test_gdaldem_lib_hillshade_float():

    src_ds = gdal.Translate(
        "",
        gdal.Open("../gdrivers/data/n43.tif"),
        format="MEM",
        outputType=gdal.GDT_Float32,
    )
    ds = gdal.DEMProcessing(
        "", src_ds, "hillshade", format="MEM", scale=111120, zFactor=30
    )
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 45587, "Bad checksum"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    assert ds.GetRasterBand(1).GetNoDataValue() == 0, "Bad nodata value"

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem hillshade with source being floating point


@pytest.mark.require_driver("PNG")
def test_gdaldem_lib_hillshade_float_png(tmp_vsimem):

    src_ds = gdal.Translate(
        "",
        gdal.Open("../gdrivers/data/n43.tif"),
        format="MEM",
        outputType=gdal.GDT_Float32,
    )
    ds = gdal.DEMProcessing(
        tmp_vsimem / "test_gdaldem_lib_hillshade_float_png.png",
        src_ds,
        "hillshade",
        format="PNG",
        scale=111120,
        zFactor=30,
    )
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 45587, "Bad checksum"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    assert ds.GetRasterBand(1).GetNoDataValue() == 0, "Bad nodata value"

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem hillshade -combined


@pytest.mark.parametrize(
    "options,checksum",
    [
        ({"scale": 111120}, 43876),
        ({"xscale": 111120, "yscale": 111120}, 43876),
        ({"xscale": 111120, "yscale": 111120 + 0.1}, 43876),
        ({}, 42176),
    ],
)
def test_gdaldem_lib_hillshade_combined(options, checksum):

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    kwargs = {"format": "MEM", "combined": True, "zFactor": 30}
    kwargs.update(options)
    ds = gdal.DEMProcessing("", src_ds, "hillshade", **kwargs)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == checksum, "Bad checksum"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    assert ds.GetRasterBand(1).GetNoDataValue() == 0, "Bad nodata value"

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem hillshade -alg ZevenbergenThorne


@pytest.mark.parametrize(
    "options,checksum",
    [
        ({"scale": 111120}, 46544),
        ({"xscale": 111120, "yscale": 111120}, 46544),
        ({"xscale": 111120, "yscale": 111120 + 0.1}, 46544),
        ({}, 45373),
    ],
)
def test_gdaldem_lib_hillshade_ZevenbergenThorne(options, checksum):

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    kwargs = {"format": "MEM", "alg": "ZevenbergenThorne", "zFactor": 30}
    kwargs.update(options)
    ds = gdal.DEMProcessing("", src_ds, "hillshade", **kwargs)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == checksum, "Bad checksum"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    assert ds.GetRasterBand(1).GetNoDataValue() == 0, "Bad nodata value"

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem hillshade -alg ZevenbergenThorne -combined


@pytest.mark.parametrize(
    "options,checksum",
    [
        ({"scale": 111120}, 43112),
        ({"xscale": 111120, "yscale": 111120}, 43112),
        ({"xscale": 111120, "yscale": 111120 + 0.1}, 43112),
        ({}, 42340),
    ],
)
def test_gdaldem_lib_hillshade_ZevenbergenThorne_combined(options, checksum):

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    kwargs = {
        "format": "MEM",
        "alg": "ZevenbergenThorne",
        "combined": True,
        "zFactor": 30,
    }
    kwargs.update(options)
    ds = gdal.DEMProcessing("", src_ds, "hillshade", **kwargs)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == checksum, "Bad checksum"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    assert ds.GetRasterBand(1).GetNoDataValue() == 0, "Bad nodata value"

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem hillshade with -compute_edges


def test_gdaldem_lib_hillshade_compute_edges():

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.DEMProcessing(
        "",
        src_ds,
        "hillshade",
        format="MEM",
        computeEdges=True,
        scale=111120,
        zFactor=30,
    )
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 50239, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem hillshade with -compute_edges with floating point


def test_gdaldem_lib_hillshade_compute_edges_float():

    src_ds = gdal.Translate(
        "",
        gdal.Open("../gdrivers/data/n43.tif"),
        format="MEM",
        outputType=gdal.GDT_Float32,
    )
    ds = gdal.DEMProcessing(
        "",
        src_ds,
        "hillshade",
        format="MEM",
        computeEdges=True,
        scale=111120,
        zFactor=30,
    )
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 50239, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem hillshade with -az parameter


def test_gdaldem_lib_hillshade_azimuth():

    src_ds = gdal.GetDriverByName("MEM").Create("", 100, 100, 1)
    src_ds.SetGeoTransform([2, 0.01, 0, 49, 0, -0.01])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    src_ds.SetProjection(sr.ExportToWkt())
    for j in range(100):
        data = ""
        for i in range(100):
            val = 255 - 5 * max(abs(50 - i), abs(50 - j))
            data = data + chr(val)
        data = data.encode("ISO-8859-1")
        src_ds.GetRasterBand(1).WriteRaster(0, j, 100, 1, data)

    # Light from the east
    ds = gdal.DEMProcessing(
        "", src_ds, "hillshade", format="MEM", azimuth=90, scale=111120, zFactor=100
    )
    assert ds is not None
    ds_ref = gdal.Open("data/pyramid_shaded_ref.tif")
    assert gdaltest.compare_ds(ds, ds_ref, verbose=1) <= 1, "Bad checksum"
    ds = None
    ds_ref = None


###############################################################################
# Test gdaldem hillshade -multidirectional


def test_gdaldem_lib_hillshade_multidirectional():

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.DEMProcessing(
        "",
        src_ds,
        "hillshade",
        format="MEM",
        multiDirectional=True,
        computeEdges=True,
        scale=111120,
        zFactor=30,
    )
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 51784, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem hillshade -multidirectional


def test_gdaldem_lib_hillshade_multidirectional_ZevenbergenThorne():

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.DEMProcessing(
        "",
        src_ds,
        "hillshade",
        format="MEM",
        alg="ZevenbergenThorne",
        multiDirectional=True,
        computeEdges=True,
        scale=111120,
        zFactor=30,
    )
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 50860, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem hillshade -igor


def test_gdaldem_lib_hillshade_igor():

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.DEMProcessing(
        "",
        src_ds,
        "hillshade",
        format="MEM",
        igor=True,
        computeEdges=True,
        scale=111120,
        zFactor=30,
    )
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 48830, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem hillshade -igor


def test_gdaldem_lib_hillshade_igor_ZevenbergenThorne():

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.DEMProcessing(
        "",
        src_ds,
        "hillshade",
        format="MEM",
        alg="ZevenbergenThorne",
        igor=True,
        computeEdges=True,
        scale=111120,
        zFactor=30,
    )
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 49014, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem color relief


def test_gdaldem_lib_color_relief():

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.DEMProcessing(
        "", src_ds, "color-relief", format="MEM", colorFilename="data/color_file.txt"
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 55066, "Bad checksum"

    assert ds.GetRasterBand(2).Checksum() == 37594, "Bad checksum"

    assert ds.GetRasterBand(3).Checksum() == 47768, "Bad checksum"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    ds = gdal.DEMProcessing(
        "",
        src_ds,
        "color-relief",
        format="MEM",
        colorFilename="data/color_file.txt",
        colorSelection="nearest_color_entry",
    )
    assert ds.GetRasterBand(1).Checksum() == 57296

    ds = gdal.DEMProcessing(
        "",
        src_ds,
        "color-relief",
        format="MEM",
        colorFilename="data/color_file.txt",
        colorSelection="exact_color_entry",
    )
    assert ds.GetRasterBand(1).Checksum() == 8073

    ds = gdal.DEMProcessing(
        "",
        src_ds,
        "color-relief",
        format="MEM",
        colorFilename="data/color_file.txt",
        colorSelection="linear_interpolation",
    )
    assert ds.GetRasterBand(1).Checksum() == 55066

    with pytest.raises(ValueError):
        gdal.DEMProcessing(
            "",
            src_ds,
            "color-relief",
            format="MEM",
            colorFilename="data/color_file.txt",
            colorSelection="unsupported",
        )

    ds = gdal.DEMProcessing(
        "",
        src_ds,
        "color-relief",
        format="MEM",
        colorFilename="data/color_file.txt",
        addAlpha=True,
    )
    assert ds.RasterCount == 4, "Bad RasterCount"

    src_ds = None
    ds = None


def test_gdaldem_lib_color_relief_nodata_value(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    colorFilename = tmp_vsimem / "color_file.txt"
    gdal.FileFromMemBuffer(colorFilename, """nv 255 255 255""")

    with gdal.quiet_errors():
        ds = gdal.DEMProcessing(
            "", src_ds, "color-relief", format="MEM", colorFilename=colorFilename
        )
    assert ds.GetRasterBand(1).Checksum() == 0

    src_ds.GetRasterBand(1).SetNoDataValue(1)
    ds = gdal.DEMProcessing(
        "", src_ds, "color-relief", format="MEM", colorFilename=colorFilename
    )
    assert ds.GetRasterBand(1).Checksum() != 0

    gdal.Unlink(colorFilename)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
@pytest.mark.parametrize(
    "colorSelection",
    ["nearest_color_entry", "exact_color_entry", "linear_interpolation"],
)
@pytest.mark.parametrize("format", ["MEM", "VRT"])
def test_gdaldem_lib_color_relief_synthetic(tmp_path, colorSelection, format):

    src_filename = "" if format == "MEM" else str(tmp_path / "in.tif")
    src_ds = gdal.GetDriverByName("MEM" if format == "MEM" else "GTiff").Create(
        src_filename, 4, 1
    )
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 4, 1, b"\x00\x01\x02\x03")
    if format != "MEM":
        src_ds.Close()
        src_ds = gdal.Open(src_filename)
    colorFilename = tmp_path / "color_file.txt"
    with open(colorFilename, "wb") as f:
        f.write(b"""0 0 0 0\n1 10 11 12\n2 20 21 22\n3 30 31 32\n""")

    out_filename = "" if format == "MEM" else str(tmp_path / "out.vrt")
    ds = gdal.DEMProcessing(
        out_filename,
        src_ds,
        "color-relief",
        format=format,
        colorFilename=colorFilename,
        colorSelection=colorSelection,
    )
    if format != "MEM":
        ds.Close()
        ds = gdal.Open(out_filename)
    assert struct.unpack("B" * 4, ds.GetRasterBand(1).ReadRaster()) == (0, 10, 20, 30)
    assert struct.unpack("B" * 4, ds.GetRasterBand(2).ReadRaster()) == (0, 11, 21, 31)
    assert struct.unpack("B" * 4, ds.GetRasterBand(3).ReadRaster()) == (0, 12, 22, 32)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
@pytest.mark.parametrize(
    "colorSelection",
    ["nearest_color_entry", "exact_color_entry", "linear_interpolation"],
)
@pytest.mark.parametrize("format", ["MEM", "VRT"])
def test_gdaldem_lib_color_relief_synthetic_nodata_255(
    tmp_path, colorSelection, format
):

    src_filename = "" if format == "MEM" else str(tmp_path / "in.tif")
    src_ds = gdal.GetDriverByName("MEM" if format == "MEM" else "GTiff").Create(
        src_filename, 4, 1
    )
    src_ds.GetRasterBand(1).SetNoDataValue(255)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 4, 1, b"\x00\x01\x02\xff")
    if format != "MEM":
        src_ds.Close()
        src_ds = gdal.Open(src_filename)
    colorFilename = tmp_path / "color_file.txt"
    with open(colorFilename, "wb") as f:
        f.write(b"""0 0 1 2\n1 10 11 12\n2 20 21 22\n255 255 255 255\n""")

    out_filename = "" if format == "MEM" else str(tmp_path / "out.vrt")
    ds = gdal.DEMProcessing(
        out_filename,
        src_ds,
        "color-relief",
        format=format,
        colorFilename=colorFilename,
        colorSelection=colorSelection,
    )
    if format != "MEM":
        ds.Close()
        ds = gdal.Open(out_filename)
    assert struct.unpack("B" * 4, ds.GetRasterBand(1).ReadRaster()) == (0, 10, 20, 255)
    assert struct.unpack("B" * 4, ds.GetRasterBand(2).ReadRaster()) == (1, 11, 21, 255)
    assert struct.unpack("B" * 4, ds.GetRasterBand(3).ReadRaster()) == (2, 12, 22, 255)


###############################################################################
# Test gdaldem tpi


def test_gdaldem_lib_tpi():

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.DEMProcessing("", src_ds, "tpi", format="MEM")
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 60504, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem tri with Wilson formula


def test_gdaldem_lib_tri_wilson():

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.DEMProcessing("", src_ds, "tri", format="MEM", alg="Wilson")
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 61143, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem tri with Riley formula


def test_gdaldem_lib_tri_riley():

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.DEMProcessing("", src_ds, "tri", format="MEM")
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 41233, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem roughness


def test_gdaldem_lib_roughness():

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.DEMProcessing("", src_ds, "roughness", format="MEM")
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 38624, "Bad checksum"

    ds = None


###############################################################################
# Test gdaldem slope -alg ZevenbergenThorne


@pytest.mark.parametrize(
    "options,checksum",
    [
        ({"scale": 111120}, 64393),
        ({"xscale": 111120, "yscale": 111120}, 64393),
        ({"xscale": 111120, "yscale": 111120 + 0.1}, 64393),
        ({}, 65468),
    ],
)
def test_gdaldem_lib_slope_ZevenbergenThorne(options, checksum):

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    kwargs = {"format": "MEM", "alg": "ZevenbergenThorne"}
    kwargs.update(options)
    ds = gdal.DEMProcessing("", src_ds, "slope", **kwargs)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == checksum, "Bad checksum"


###############################################################################
# Test gdaldem aspect -alg ZevenbergenThorne


def test_gdaldem_lib_aspect_ZevenbergenThorne():

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ds = gdal.DEMProcessing(
        "",
        src_ds,
        "aspect",
        format="MEM",
        alg="ZevenbergenThorne",
        scale=111120,
        zFactor=30,
    )
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 50539, "Bad checksum"


###############################################################################
# Test gdaldem hillshade with nodata values


def test_gdaldem_lib_nodata():

    for value, typ in [
        (0, gdal.GDT_UInt8),
        (1, gdal.GDT_UInt8),
        (255, gdal.GDT_UInt8),
        (0, gdal.GDT_UInt16),
        (1, gdal.GDT_UInt16),
        (65535, gdal.GDT_UInt16),
        (0, gdal.GDT_Int16),
        (-32678, gdal.GDT_Int16),
        (32767, gdal.GDT_Int16),
    ]:
        src_ds = gdal.GetDriverByName("MEM").Create("", 10, 10, 1, typ)
        src_ds.GetRasterBand(1).SetNoDataValue(value)
        src_ds.GetRasterBand(1).Fill(value)

        ds = gdal.DEMProcessing("", src_ds, "hillshade", format="MEM")
        assert ds is not None

        cs = ds.GetRasterBand(1).Checksum()
        assert cs == 0, "Bad checksum"

        src_ds = None
        ds = None

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3, 1)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(1, 1, 1, 1, struct.pack("B", 255))

    ds = gdal.DEMProcessing("", src_ds, "hillshade", format="MEM")
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        print(ds.ReadAsArray())
        pytest.fail("Bad checksum")

    ds = gdal.DEMProcessing("", src_ds, "hillshade", format="MEM", computeEdges=True)
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 10:
        print(ds.ReadAsArray())  # Should be 0 0 0 0 181 0 0 0 0
        pytest.fail("Bad checksum")

    # Same with floating point
    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3, 1, gdal.GDT_Float32)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(1, 1, 1, 1, struct.pack("f", 255))

    ds = gdal.DEMProcessing("", src_ds, "hillshade", format="MEM")
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        print(ds.ReadAsArray())
        pytest.fail("Bad checksum")

    ds = gdal.DEMProcessing("", src_ds, "hillshade", format="MEM", computeEdges=True)
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 10:
        print(ds.ReadAsArray())  # Should be 0 0 0 0 181 0 0 0 0
        pytest.fail("Bad checksum")


###############################################################################
# Test option argument handling


def test_gdaldem_lib_dict_arguments():

    opt = gdal.DEMProcessingOptions(
        "__RETURN_OPTION_LIST__",
        creationOptions=collections.OrderedDict(
            (("COMPRESS", "DEFLATE"), ("LEVEL", 4))
        ),
    )

    ind = opt.index("-co")

    assert opt[ind : ind + 4] == ["-co", "COMPRESS=DEFLATE", "-co", "LEVEL=4"]


###############################################################################


def flipped_ds():

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    gt = src_ds.GetGeoTransform()
    ulx = "%.17g" % gt[0]
    uly = "%.17g" % gt[3]
    lrx = "%.17g" % (gt[0] + src_ds.RasterXSize * gt[1])
    lry = "%.17g" % (gt[3] + src_ds.RasterYSize * gt[5])
    return gdal.Warp("", src_ds, options=f"-of VRT -te {ulx} {uly} {lrx} {lry}")


@pytest.mark.parametrize("alg", ["aspect", "TPI", "TRI"])
def test_gdaldem_lib_flipped_aspect_tpi_tri(alg):

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ref_ds = gdal.DEMProcessing("", src_ds, alg, format="MEM", computeEdges=True)
    out_ds = gdal.DEMProcessing("", flipped_ds(), alg, format="MEM", computeEdges=True)
    assert ref_ds.GetGeoTransform() == pytest.approx(out_ds.GetGeoTransform())
    assert ref_ds.ReadRaster() == out_ds.ReadRaster()


@pytest.mark.parametrize("alg", ["hillshade", "slope", "roughness"])
def test_gdaldem_lib_flipped_hillshade_slope_roughness(alg):

    src_ds = gdal.Open("../gdrivers/data/n43.tif")
    ref_ds = gdal.DEMProcessing("", src_ds, alg, format="MEM", computeEdges=True)
    out_ds = gdal.DEMProcessing("", flipped_ds(), alg, format="MEM", computeEdges=True)
    out_ds = gdal.Warp("", out_ds, format="MEM")
    assert ref_ds.GetGeoTransform() == pytest.approx(out_ds.GetGeoTransform())
    assert ref_ds.ReadRaster() == out_ds.ReadRaster()
