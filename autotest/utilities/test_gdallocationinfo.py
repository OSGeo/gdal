#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdallocationinfo testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys

import pytest

sys.path.append("../gcore")

import gdaltest
import test_cli_utilities

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdallocationinfo_path() is None,
    reason="gdallocationinfo not available",
)


@pytest.fixture()
def gdallocationinfo_path():
    return test_cli_utilities.get_gdallocationinfo_path()


###############################################################################
# Test basic usage


def test_gdallocationinfo_1(gdallocationinfo_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdallocationinfo_path + " ../gcore/data/byte.tif 0 0"
    )
    assert err is None or err == "", "got error/warning"

    ret = ret.replace("\r\n", "\n")
    expected_ret = """Report:
  Location: (0P,0L)
  Band 1:
    Value: 107"""
    assert ret.startswith(expected_ret)


###############################################################################
# Test -xml


def test_gdallocationinfo_2(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path + " -xml ../gcore/data/byte.tif 0 0"
    )
    ret = ret.replace("\r\n", "\n")
    expected_ret = """<Report pixel="0" line="0">
  <BandReport band="1">
    <Value>107</Value>
  </BandReport>
</Report>"""
    assert ret.startswith(expected_ret)


###############################################################################
# Test -valonly


def test_gdallocationinfo_3(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path + " -b 1 -valonly ../gcore/data/byte.tif 0 0"
    )
    expected_ret = """107"""
    assert ret.startswith(expected_ret)


###############################################################################
# Test -geoloc


def test_gdallocationinfo_4(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path + " -geoloc ../gcore/data/byte.tif 440720.000 3751320.000"
    )
    ret = ret.replace("\r\n", "\n")
    expected_ret = """Report:
  Location: (0P,0L)
  Band 1:
    Value: 107"""
    assert ret.startswith(expected_ret)


# Test -geoloc at lower right corner
def test_gdallocationinfo_lr(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path + " -geoloc ../gcore/data/byte.tif 441920.000 3750120.000"
    )
    ret = ret.replace("\r\n", "\n")
    expected_ret = """Report:
  Location: (20P,20L)
  Band 1:
    Value: 107"""
    assert ret.startswith(expected_ret)


###############################################################################
# Test -lifonly


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdallocationinfo_5(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path + " -lifonly ../gcore/data/byte.vrt 0 0"
    )
    expected_ret1 = """../gcore/data/byte.tif"""
    expected_ret2 = """../gcore/data\\byte.tif"""
    assert expected_ret1 in ret or expected_ret2 in ret


###############################################################################
# Test -overview


def test_gdallocationinfo_6(gdallocationinfo_path, tmp_path):

    tmp_tif = str(tmp_path / "test_gdallocationinfo_6.tif")

    src_ds = gdal.Open("../gcore/data/byte.tif")
    ds = gdal.GetDriverByName("GTiff").CreateCopy(tmp_tif, src_ds)
    ds.BuildOverviews("AVERAGE", overviewlist=[2])
    ds = None
    src_ds = None

    ret = gdaltest.runexternal(f"{gdallocationinfo_path} {tmp_tif} 10 10 -overview 1")

    expected_ret = """Value: 130"""
    assert expected_ret in ret


def test_gdallocationinfo_wgs84(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path
        + " -valonly -wgs84 ../gcore/data/byte.tif -117.6354747 33.8970515"
    )

    expected_ret = """115"""
    assert expected_ret in ret


###############################################################################


def test_gdallocationinfo_field_sep(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path + ' -valonly -field_sep "," ../gcore/data/byte.tif',
        strin="0 0",
    )

    assert "107" in ret
    assert "," not in ret

    ret = gdaltest.runexternal(
        gdallocationinfo_path + ' -valonly -field_sep "," ../gcore/data/rgbsmall.tif',
        strin="15 16",
    )

    assert "72,102,16" in ret


###############################################################################


def test_gdallocationinfo_extra_input(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path + " ../gcore/data/byte.tif", strin="0 0 foo bar"
    )

    assert "Extra input: foo bar" in ret

    ret = gdaltest.runexternal(
        gdallocationinfo_path + " -valonly ../gcore/data/byte.tif", strin="0 0 foo bar"
    )

    assert "107" in ret
    assert "foo bar" not in ret

    ret = gdaltest.runexternal(
        gdallocationinfo_path + ' -valonly -field_sep "," ../gcore/data/byte.tif',
        strin="0 0 foo bar",
    )

    assert "107,foo bar" in ret

    ret = gdaltest.runexternal(
        gdallocationinfo_path + " -xml ../gcore/data/byte.tif", strin="0 0 foo bar"
    )

    assert "<ExtraInput>foo bar</ExtraInput>" in ret


###############################################################################


def test_gdallocationinfo_extra_input_ignored(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path
        + ' -valonly -field_sep "," -ignore_extra_input ../gcore/data/byte.tif',
        strin="0 0 foo bar",
    )

    assert "107" in ret
    assert "foo bar" not in ret


###############################################################################
# Test echo mode


def test_gdallocationinfo_echo(gdallocationinfo_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdallocationinfo_path + " -E ../gcore/data/byte.tif 1 2"
    )
    assert "-E can only be used with -valonly" in err

    _, err = gdaltest.runexternal_out_and_err(
        gdallocationinfo_path + " -E -valonly ../gcore/data/byte.tif 1 2"
    )
    assert (
        "-E can only be used if -field_sep is specified (to a non-newline value)" in err
    )

    ret = gdaltest.runexternal(
        gdallocationinfo_path + ' -E -valonly -field_sep "," ../gcore/data/byte.tif',
        strin="1 2",
    )
    assert "1,2,132" in ret

    ret = gdaltest.runexternal(
        gdallocationinfo_path
        + ' -geoloc -E -valonly -field_sep "," ../gcore/data/byte.tif',
        strin="440780.5 3751200.5",
    )
    assert "440780.5,3751200.5,132" in ret

    ret = gdaltest.runexternal(
        gdallocationinfo_path
        + ' -geoloc -E -valonly -field_sep "," ../gcore/data/byte.tif',
        strin="440780.5 3751200.5 extra_content",
    )
    assert "440780.5,3751200.5,132,extra_content" in ret


###############################################################################
# Test out of raster coordinates


def test_gdallocationinfo_out_of_raster_coordinates_valonly(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path + " -valonly ../gcore/data/byte.tif",
        strin="1 2\n-1 -1\n1 2",
    )

    ret = ret.replace("\r\n", "\n")
    assert "132\n\n132\n" in ret

    ret = gdaltest.runexternal(
        gdallocationinfo_path + ' -E -valonly -field_sep "," ../gcore/data/byte.tif',
        strin="1 2\n-1 -1\n1 2",
    )

    ret = ret.replace("\r\n", "\n")
    assert "1,2,132\n-1,-1,\n1,2,132\n" in ret


def test_gdallocationinfo_out_of_raster_coordinates_valonly_multiband(
    gdallocationinfo_path,
):

    ret = gdaltest.runexternal(
        gdallocationinfo_path + " -valonly ../gcore/data/rgbsmall.tif",
        strin="1 2\n-1 -1\n1 2",
    )

    ret = ret.replace("\r\n", "\n")
    assert "0\n0\n0\n\n\n\n0\n0\n0\n" in ret

    ret = gdaltest.runexternal(
        gdallocationinfo_path
        + ' -E -valonly -field_sep "," ../gcore/data/rgbsmall.tif',
        strin="1 2\n-1 -1\n1 2",
    )

    ret = ret.replace("\r\n", "\n")
    assert "1,2,0,0,0\n-1,-1,,,\n1,2,0,0,0\n" in ret


###############################################################################


def test_gdallocationinfo_nad27_interpolate_bilinear(gdallocationinfo_path):

    # run on nad27 explicitly to avoid datum transformations.
    ret = gdaltest.runexternal(
        gdallocationinfo_path
        + " -valonly  -r bilinear -l_srs EPSG:4267 ../gcore/data/byte.tif -117.6354747 33.8970515"
    )

    assert float(ret) == pytest.approx(130.476908, rel=1e-4)


def test_gdallocationinfo_nad27_interpolate_cubic(gdallocationinfo_path):

    # run on nad27 explicitly to avoid datum transformations.
    ret = gdaltest.runexternal(
        gdallocationinfo_path
        + " -valonly  -r cubic -l_srs EPSG:4267 ../gcore/data/byte.tif -117.6354747 33.8970515"
    )

    assert float(ret) == pytest.approx(134.65629, rel=1e-4)


def test_gdallocationinfo_nad27_interpolate_cubicspline(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path
        + " -valonly  -r cubicspline -l_srs EPSG:4267 ../gcore/data/byte.tif -117.6354747 33.8970515"
    )

    assert float(ret) == pytest.approx(125.795025, rel=1e-4)


def test_gdallocationinfo_report_geoloc_interpolate_bilinear(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path
        + " -r bilinear -geoloc ../gcore/data/byte.tif 441319.09 3750601.80"
    )
    ret = ret.replace("\r\n", "\n")
    assert "Report:" in ret
    assert "Location: (9.98" in ret
    assert "P,11.97" in ret
    assert "Value: 137.2524" in ret


def test_gdallocationinfo_report_interpolate_bilinear(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path + " -r bilinear ../gcore/data/byte.tif 9.98 11.97"
    )
    ret = ret.replace("\r\n", "\n")
    assert "Report:" in ret
    assert "Location: (9.98" in ret
    assert "P,11.97" in ret
    assert "Value: 137.24" in ret


def test_gdallocationinfo_report_interpolate_cubic(gdallocationinfo_path):

    ret = gdaltest.runexternal(
        gdallocationinfo_path + " -r cubic ../gcore/data/byte.tif 9.98 11.97"
    )
    ret = ret.replace("\r\n", "\n")
    assert "Report:" in ret
    assert "Location: (9.98" in ret
    assert "P,11.97" in ret
    assert "Value: 141.58" in ret


def test_gdallocationinfo_value_interpolate_bilinear(gdallocationinfo_path):

    # Those coordinates are almost 10,12. It is testing that they are not converted to integer.
    ret = gdaltest.runexternal(
        gdallocationinfo_path
        + " -valonly -r bilinear ../gcore/data/byte.tif 9.9999999 11.9999999"
    )
    assert float(ret) == pytest.approx(139.75, rel=1e-6)


def test_gdallocationinfo_value_interpolate_bilinear_near_border(gdallocationinfo_path):

    # Those coordinates are almost 10,12. It is testing that they are not converted to integer.
    ret = gdaltest.runexternal(
        gdallocationinfo_path
        + " -valonly -r bilinear ../gcore/data/byte.tif 19 19.9999999"  # should we allow 20.0?
    )
    assert float(ret) == pytest.approx(103, rel=1e-6)


def test_gdallocationinfo_value_interpolate_invalid_method(gdallocationinfo_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdallocationinfo_path + " -valonly -r mode ../gcore/data/byte.tif 10 12"
    )
    assert "-r can only be used with values" in err


def test_gdallocationinfo_interpolate_float_data(gdallocationinfo_path, tmp_path):

    gdaltest.importorskip_gdal_array()

    dst_filename = str(tmp_path / "tmp_float.tif")
    driver = gdal.GetDriverByName("GTiff")
    dst_ds = driver.Create(
        dst_filename, xsize=2, ysize=2, bands=1, eType=gdal.GDT_Float32
    )
    np = pytest.importorskip("numpy")
    raster_array = np.array(([10.5, 1.1], [2.4, 3.8]))
    dst_ds.GetRasterBand(1).WriteArray(raster_array)
    dst_ds = None

    ret = gdaltest.runexternal(
        gdallocationinfo_path + " -valonly -r bilinear {} 1 1".format(dst_filename)
    )
    assert float(ret) == pytest.approx(4.45, rel=1e-6)


def test_gdallocationinfo_nodata(gdallocationinfo_path, tmp_path):

    filename = tmp_path / "out.tif"
    # 64 because this is the size of the cache window of GDALInterpolateAtPoint
    with gdal.GetDriverByName("MEM").Create("", 1, 64 + 1, 2) as src_ds:
        src_ds.GetRasterBand(1).SetNoDataValue(16)
        gdal.Translate(
            filename, src_ds, creationOptions=["BLOCKYSIZE=1", "INTERLEAVE=PIXEL"]
        )
    with gdal.Open(filename, gdal.GA_Update) as ds:
        ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, b"\x10")
        ds.GetRasterBand(2).WriteRaster(0, 0, 1, 1, b"\x10")
        ds.GetRasterBand(1).WriteRaster(0, 1, 1, 1, b"\x10")
        ds.GetRasterBand(2).WriteRaster(0, 1, 1, 1, b"\x11")

    f = gdal.VSIFOpenL(filename, "rb+")
    assert f
    gdal.VSIFTruncateL(f, gdal.VSIStatL(filename).size - 1)
    gdal.VSIFCloseL(f)

    ret = gdaltest.runexternal(gdallocationinfo_path + f" {filename} 0 0")
    ret = ret.replace("\r\n", "\n")
    expected_ret = """Report:
  Location: (0P,0L)
  Band 1:
    Value: 16
  Band 2:
    Value: 16"""
    assert ret.startswith(expected_ret)

    ret = gdaltest.runexternal(gdallocationinfo_path + f" -xml {filename} 0 0")
    ret = ret.replace("\r\n", "\n")
    expected_ret = """<Report pixel="0" line="0">
  <BandReport band="1">
    <Value>16</Value>
  </BandReport>
  <BandReport band="2">
    <Value>16</Value>
  </BandReport>
</Report>
"""
    assert ret.startswith(expected_ret)

    ret = gdaltest.runexternal(
        gdallocationinfo_path + f" -valonly -field_sep , {filename} 0 0"
    )
    ret = ret.replace("\r\n", "\n")
    expected_ret = """16,16"""
    assert ret.startswith(expected_ret)

    ret = gdaltest.runexternal(
        gdallocationinfo_path + f" -valonly -field_sep , {filename} 0 1"
    )
    ret = ret.replace("\r\n", "\n")
    expected_ret = """16,17"""
    assert ret.startswith(expected_ret)

    ret, err = gdaltest.runexternal_out_and_err(
        gdallocationinfo_path + f" {filename} 0 64"
    )
    ret = ret.replace("\r\n", "\n")
    expected_ret = """Report:
  Location: (0P,64L)
  Band 1:
  Band 2:"""
    assert ret.startswith(expected_ret)
    assert "ret code = 1" in err

    ret, err = gdaltest.runexternal_out_and_err(
        gdallocationinfo_path + f" -xml {filename} 0 64"
    )
    ret = ret.replace("\r\n", "\n")
    expected_ret = """<Report pixel="0" line="64">
  <BandReport band="1">
    <IOError />
  </BandReport>
  <BandReport band="2">
    <IOError />
  </BandReport>
</Report>"""
    assert ret.startswith(expected_ret)
    assert "ret code = 1" in err

    ret, err = gdaltest.runexternal_out_and_err(
        gdallocationinfo_path + f" -valonly -field_sep , {filename} 0 64"
    )
    ret = ret.replace("\r\n", "\n")
    expected_ret = """,
"""
    assert ret.startswith(expected_ret)
    assert "ret code = 1" in err
