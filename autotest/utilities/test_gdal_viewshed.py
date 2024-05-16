#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_viewshed testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import struct

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal

pytestmark = [
    pytest.mark.skipif(
        test_cli_utilities.get_gdalwarp_path() is None, reason="gdalwarp not available"
    ),
    pytest.mark.skipif(
        test_cli_utilities.get_gdal_viewshed_path() is None,
        reason="gdal_viewshed not available",
    ),
]


@pytest.fixture()
def gdal_viewshed_path():
    return test_cli_utilities.get_gdal_viewshed_path()


###############################################################################

ox = [621528]
oy = [4817617]
oz = [100, 10]


@pytest.fixture()
def viewshed_input(tmp_path):

    fname = str(tmp_path / "test_gdal_viewshed_in.tif")

    gdaltest.runexternal(
        test_cli_utilities.get_gdalwarp_path()
        + " -t_srs EPSG:32617 -overwrite ../gdrivers/data/n43.tif "
        + fname,
    )

    return fname


def test_gdal_viewshed(gdal_viewshed_path, tmp_path, viewshed_input):

    viewshed_out = str(tmp_path / "test_gdal_viewshed_out.tif")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path
        + " -oz {} -ox {} -oy {} {} {}".format(
            oz[0], ox[0], oy[0], viewshed_input, viewshed_out
        )
    )
    assert err is None or err == ""
    ds = gdal.Open(viewshed_out)
    assert ds
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    assert cs == 14613
    assert nodata is None


@pytest.mark.parametrize("cc_option", ["", " -cc 1.0"])
def test_gdal_viewshed_non_earth_crs(
    gdal_viewshed_path, tmp_path, viewshed_input, cc_option
):

    viewshed_out = str(tmp_path / "test_gdal_viewshed_out.tif")
    viewshed_tmp = str(tmp_path / "test_gdal_viewshed_tmp.tif")

    gdal.Translate(
        viewshed_tmp, viewshed_input, outputSRS="+proj=utm +zone=17 +a=1000000 +rf=300"
    )
    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path
        + cc_option
        + " -oz {} -ox {} -oy {} {} {}".format(
            oz[0], ox[0], oy[0], viewshed_tmp, viewshed_out
        )
    )
    assert err is None or err == ""
    ds = gdal.Open(viewshed_out)
    assert ds
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    assert cs == 14609
    assert nodata is None


###############################################################################


def test_gdal_viewshed_alternative_modes(gdal_viewshed_path, tmp_path, viewshed_input):

    viewshed_out = str(tmp_path / "test_gdal_viewshed_out.tif")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path
        + " -om DEM -oz {} -ox {} -oy {} {} {}".format(
            oz[0], ox[0], oy[0], viewshed_input, viewshed_out
        )
    )
    assert err is None or err == ""
    ds = gdal.Open(viewshed_out)
    assert ds
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    gdal.Unlink(viewshed_out)
    assert cs == 45734
    assert nodata is None

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path
        + " -om GROUND -oz {} -ox {} -oy {} {} {}".format(
            oz[0], ox[0], oy[0], viewshed_input, viewshed_out
        )
    )
    assert err is None or err == ""
    ds = gdal.Open(viewshed_out)
    assert ds
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    assert cs == 8381
    assert nodata is None

###############################################################################


def test_gdal_viewshed_api(viewshed_input):
    src_ds = gdal.Open(viewshed_input)
    ds = gdal.ViewshedGenerate(
        src_ds.GetRasterBand(1),
        "MEM",
        "unused_target_raster_name",
        ["INTERLEAVE=BAND"],
        ox[0],
        oy[0],
        oz[0],
        0,  # targetHeight
        255,  # visibleVal
        0,  # invisibleVal
        0,  # outOfRangeVal
        -1.0,  # noDataVal,
        0.85714,  # dfCurvCoeff
        gdal.GVM_Edge,
        0,  # maxDistance
        heightMode=gdal.GVOT_MIN_TARGET_HEIGHT_FROM_GROUND,
        options=["UNUSED=YES"],
    )

    assert ds.GetRasterBand(1).Checksum() == 8381


###############################################################################


def test_gdal_viewshed_all_options(gdal_viewshed_path, tmp_path, viewshed_input):

    viewshed_out = str(tmp_path / "test_gdal_viewshed_out.tif")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path
        + " -om NORMAL -f GTiff -oz {} -ox {} -oy {} -b 1 -a_nodata 0 -tz 5 -md 20000 -cc 0 -iv 127 -vv 254 -ov 0 {} {}".format(
            oz[1], ox[0], oy[0], viewshed_input, viewshed_out
        )
    )
    assert err is None or err == ""
    ds = gdal.Open(viewshed_out)
    assert ds
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    assert cs == 24435
    assert nodata == 0

###############################################################################

def test_gdal_viewshed_value_options(gdal_viewshed_path, tmp_path, viewshed_input):

    viewshed_out = str(tmp_path / "test_gdal_viewshed_out.tif")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path
        + " -om NORMAL -f GTiff -oz {} -ox {} -oy {} -b 1 -a_nodata 0 -iv 127 -vv 254 -ov 0 {} {}".format(
            oz[1], ox[0], oy[0], viewshed_input, viewshed_out
        )
    )
    assert err is None or err == ""
    ds = gdal.Open(viewshed_out)
    assert ds
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    assert cs == 35091
    assert nodata == 0

###############################################################################

def test_gdal_viewshed_tz_option(gdal_viewshed_path, tmp_path, viewshed_input):

    viewshed_out = str(tmp_path / "test_gdal_viewshed_out.tif")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path
        + " -om NORMAL -f GTiff -oz {} -ox {} -oy {} -b 1 -a_nodata 0 -tz 5 {} {}".format(
            oz[1], ox[0], oy[0], viewshed_input, viewshed_out
        )
    )
    assert err is None or err == ""
    ds = gdal.Open(viewshed_out)
    assert ds
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    assert cs == 33725
    assert nodata == 0

###############################################################################

def test_gdal_viewshed_cc_option(gdal_viewshed_path, tmp_path, viewshed_input):

    viewshed_out = str(tmp_path / "test_gdal_viewshed_out.tif")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path
        + " -om NORMAL -f GTiff -oz {} -ox {} -oy {} -b 1 -a_nodata 0 -cc 0 {} {}".format(
            oz[1], ox[0], oy[0], viewshed_input, viewshed_out
        )
    )
    assert err is None or err == ""
    ds = gdal.Open(viewshed_out)
    assert ds
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    assert cs == 17241
    assert nodata == 0

###############################################################################

def test_gdal_viewshed_md_option(gdal_viewshed_path, tmp_path, viewshed_input):

    viewshed_out = str(tmp_path / "test_gdal_viewshed_out.tif")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path
        + " -om NORMAL -f GTiff -oz {} -ox {} -oy {} -b 1 -a_nodata 0 -tz 5 -md 20000 {} {}".format(
            oz[1], ox[0], oy[0], viewshed_input, viewshed_out
        )
    )
    assert err is None or err == ""
    ds = gdal.Open(viewshed_out)
    assert ds
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    assert cs == 22617
    assert nodata == 0

###############################################################################


def test_gdal_viewshed_missing_source(gdal_viewshed_path):

    _, err = gdaltest.runexternal_out_and_err(gdal_viewshed_path + " -ox 0 -oy 0")
    assert "dst_filename: 1 argument(s) expected. 0 provided" in err


###############################################################################


def test_gdal_viewshed_missing_destination(gdal_viewshed_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path + " -ox 0 -oy 0 /dev/null"
    )
    assert "Error: dst_filename: 1 argument(s) expected. 0 provided" in err


###############################################################################


def test_gdal_viewshed_missing_ox(gdal_viewshed_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path + " /dev/null /dev/null"
    )
    assert "-ox: required" in err


###############################################################################


def test_gdal_viewshed_missing_oy(gdal_viewshed_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path + " -ox 0 /dev/null /dev/null"
    )
    assert "-oy: required" in err


###############################################################################


def test_gdal_viewshed_invalid_input(gdal_viewshed_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path + " -ox 0 -oy 0 /dev/null /dev/null"
    )
    assert ("not recognized as" in err) or ("No such file or directory" in err)


###############################################################################


def test_gdal_viewshed_invalid_band(gdal_viewshed_path, tmp_path):

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_viewshed_path} -ox 0 -oy 0 -b 2 ../gdrivers/data/n43.tif {tmp_path}/tmp.tif"
    )
    assert "Illegal band" in err


###############################################################################


def test_gdal_viewshed_invalid_observer_point(gdal_viewshed_path, tmp_path):

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_viewshed_path} -ox 0 -oy 0 ../gdrivers/data/n43.tif {tmp_path}/tmp.tif"
    )
    assert "The observer location falls outside of the DEM area" in err


###############################################################################


def test_gdal_viewshed_invalid_output_driver(gdal_viewshed_path, tmp_path):

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_viewshed_path} -ox -79.5 -oy 43.5 -of FOOBAR ../gdrivers/data/n43.tif {tmp_path}/tmp.tif"
    )
    assert "Cannot get driver" in err


###############################################################################


def test_gdal_viewshed_invalid_output_filename(gdal_viewshed_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path
        + " -ox -79.5 -oy 43.5 ../gdrivers/data/n43.tif i/do_not/exist.tif"
    )
    assert "Cannot create dataset" in err


###############################################################################
# Test bug fix for https://github.com/OSGeo/gdal/issues/9432


def test_gdal_viewshed_south_up(gdal_viewshed_path, tmp_path, viewshed_input):

    width = 7
    height = 5
    res = 1
    left_x = 1000
    top_y = 2000

    # "Reference" case with north-up dataset
    src_ds_north_up_filename = str(tmp_path / "test_gdal_viewshed_src_ds_north_up.tif")
    src_ds_north_up = gdal.GetDriverByName("GTiff").Create(
        src_ds_north_up_filename, width, height
    )
    src_ds_north_up.SetGeoTransform([left_x, res, 0, top_y, 0, -res])
    expected_gt = src_ds_north_up.GetGeoTransform()
    src_ds_north_up.GetRasterBand(1).WriteRaster(width // 2, height // 2, 1, 1, b"\x80")
    src_ds_north_up.Close()

    viewshed_out = str(tmp_path / "test_gdal_viewshed_north_up_out.tif")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path
        + " -oz {} -ox {} -oy {} {} {}".format(
            130,
            left_x + float(width) / 2 * res,
            top_y - float(height) / 2 * res,
            src_ds_north_up_filename,
            viewshed_out,
        )
    )
    assert err is None or err == ""
    ds = gdal.Open(viewshed_out)
    assert ds
    assert ds.RasterXSize == width
    assert ds.RasterYSize == height
    assert ds.GetGeoTransform() == pytest.approx(expected_gt)
    expected_data = (
        255,
        255,
        255,
        255,
        255,
        255,
        255,  # end of line
        255,
        255,
        0,
        0,
        0,
        255,
        255,  # end of line
        255,
        255,
        255,
        255,
        255,
        255,
        255,  # end of line
        255,
        255,
        0,
        0,
        0,
        255,
        255,  # end of line
        255,
        255,
        255,
        255,
        255,
        255,
        255,
    )
    assert (
        struct.unpack("B" * (width * height), ds.GetRasterBand(1).ReadRaster())
        == expected_data
    )

    # Tested case with south-up dataset
    src_ds_south_up_filename = str(tmp_path / "test_gdal_viewshed_src_ds_south_up.tif")
    src_ds_south_up = gdal.GetDriverByName("GTiff").Create(
        src_ds_south_up_filename, width, height
    )
    src_ds_south_up.SetGeoTransform([left_x, res, 0, top_y - res * height, 0, res])
    expected_gt = src_ds_south_up.GetGeoTransform()
    src_ds_south_up.GetRasterBand(1).WriteRaster(width // 2, height // 2, 1, 1, b"\x80")
    src_ds_south_up.Close()

    viewshed_out = str(tmp_path / "test_gdal_viewshed_south_up_out.tif")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_viewshed_path
        + " -oz {} -ox {} -oy {} {} {}".format(
            130,
            left_x + float(width) / 2 * res,
            top_y - float(height) / 2 * res,
            src_ds_south_up_filename,
            viewshed_out,
        )
    )
    assert err is None or err == ""
    ds = gdal.Open(viewshed_out)
    assert ds
    assert ds.RasterXSize == width
    assert ds.RasterYSize == height
    assert ds.GetGeoTransform() == pytest.approx(expected_gt)
    assert (
        struct.unpack("B" * (width * height), ds.GetRasterBand(1).ReadRaster())
        == expected_data
    )
