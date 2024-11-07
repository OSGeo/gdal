#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_edit.py testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil
import sys

import pytest
import test_py_scripts

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdal_edit") is None,
    reason="gdal_edit not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdal_edit")


# Usage: gdal_edit [--help-general] [-a_srs srs_def] [-a_ullr ulx uly lrx lry]
#                 [-tr xres yres] [-a_nodata value]
#                 [-unsetgt] [-stats] [-approx_stats]
#                 [-gcp pixel line easting northing [elevation]]*
#                 [-mo "META-TAG=VALUE"]*  datasetname


###############################################################################
#


def test_gdal_edit_help(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_edit", "--help"
    )


###############################################################################
#


def test_gdal_edit_version(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_edit", "--version"
    )


###############################################################################
# Test -a_srs, -a_ullr, -a_nodata, -mo, -unit


@pytest.mark.parametrize("read_only", [True, False])
def test_gdal_edit_py_1(script_path, tmp_path, read_only):

    filename = str(tmp_path / "test_gdal_edit_py.tif")
    shutil.copy(test_py_scripts.get_data_path("gcore") + "byte.tif", filename)

    if sys.platform == "win32":
        # Passing utf-8 characters doesn't at least please Wine...
        val = "fake-utf8"
        val_encoded = val
    else:
        val = "\u00e9ven"
        val_encoded = val

    read_only_option = " -ro" if read_only else ""
    _, err = test_py_scripts.run_py_script(
        script_path,
        "gdal_edit",
        filename
        + " -a_srs EPSG:4326 -a_ullr 2 50 3 49 -a_nodata 123 -mo FOO=BAR -units metre -mo UTF8="
        + val_encoded
        + " -mo "
        + val_encoded
        + "=UTF8"
        + read_only_option,
        return_stderr=True,
    )
    assert "UseExceptions" not in err

    ds = gdal.Open(filename)
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    nd = ds.GetRasterBand(1).GetNoDataValue()
    md = ds.GetMetadata()
    units = ds.GetRasterBand(1).GetUnitType()
    ds = None

    assert wkt.find("4326") != -1

    expected_gt = (2.0, 0.050000000000000003, 0.0, 50.0, 0.0, -0.050000000000000003)
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1e-10)

    assert nd == 123

    assert md["FOO"] == "BAR"

    assert md["UTF8"] == val

    assert md[val] == "UTF8"

    assert units == "metre"


###############################################################################
# Test -a_ulurll


def test_gdal_edit_py_1b(script_path, tmp_path):

    filename = str(tmp_path / "test_gdal_edit_py.tif")
    shutil.copy(test_py_scripts.get_data_path("gcore") + "byte.tif", filename)

    for points, expected in (
        ("2 50 3 50 2 49", (2, 0.05, 0, 50, 0, -0.05)),  # not rotated
        ("25 70 55 80 35 40", (25, 1.5, 0.5, 70, 0.5, -1.5)),  # rotated CCW
        ("25 70 55 65 20 40", (25, 1.5, -0.25, 70, -0.25, -1.5)),  # rotated CW
    ):
        arguments = filename + " -a_ulurll " + points
        assert test_py_scripts.run_py_script(script_path, "gdal_edit", arguments) == ""
        assert gdal.Open(filename).GetGeoTransform() == pytest.approx(expected)


###############################################################################
# Test -unsetgt


def test_gdal_edit_py_2(script_path, tmp_path):

    filename = str(tmp_path / "test_gdal_edit_py.tif")
    shutil.copy(test_py_scripts.get_data_path("gcore") + "byte.tif", filename)

    test_py_scripts.run_py_script(script_path, "gdal_edit", filename + " -unsetgt")

    ds = gdal.Open(filename)
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform(can_return_null=True)
    ds = None

    assert gt is None

    assert wkt != ""


###############################################################################
# Test -a_srs ''


def test_gdal_edit_py_3(script_path, tmp_path):

    filename = str(tmp_path / "test_gdal_edit_py.tif")
    shutil.copy(test_py_scripts.get_data_path("gcore") + "byte.tif", filename)

    test_py_scripts.run_py_script(script_path, "gdal_edit", filename + " -a_srs None")

    ds = gdal.Open(filename)
    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    ds = None

    assert gt != (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)

    assert wkt == ""


###############################################################################
# Test -unsetstats


def test_gdal_edit_py_4(script_path, tmp_path):

    filename = str(tmp_path / "test_gdal_edit_py.tif")
    shutil.copy(test_py_scripts.get_data_path("gcore") + "byte.tif", filename)

    ds = gdal.Open(filename, gdal.GA_Update)
    band = ds.GetRasterBand(1)
    band.ComputeStatistics(False)
    band.SetMetadataItem("FOO", "BAR")
    ds = band = None

    ds = gdal.Open(filename)
    band = ds.GetRasterBand(1)
    assert not (
        band.GetMetadataItem("STATISTICS_MINIMUM") is None
        or band.GetMetadataItem("FOO") is None
    )
    ds = band = None

    test_py_scripts.run_py_script(script_path, "gdal_edit", f"{filename} -unsetstats")

    ds = gdal.Open(filename)
    band = ds.GetRasterBand(1)
    assert not (
        band.GetMetadataItem("STATISTICS_MINIMUM") is not None
        or band.GetMetadataItem("FOO") is None
    )
    ds = band = None

    with pytest.raises(OSError):
        os.stat(filename + ".aux.xml")


###############################################################################
# Test -stats


def test_gdal_edit_py_5(script_path, tmp_path):

    gdal_array = pytest.importorskip("osgeo.gdal_array")
    try:
        gdal_array.BandRasterIONumPy
    except AttributeError:
        pytest.skip("osgeo.gdal_array.BandRasterIONumPy is unavailable")

    filename = str(tmp_path / "test_gdal_edit_py.tif")
    shutil.copy(test_py_scripts.get_data_path("gcore") + "byte.tif", filename)

    ds = gdal.Open(filename, gdal.GA_Update)
    band = ds.GetRasterBand(1)
    array = band.ReadAsArray()
    # original minimum is 74; modify a pixel value from 99 to 22
    array[15, 12] = 22
    band.WriteArray(array)
    ds = band = None

    ds = gdal.Open(filename)
    assert ds.ReadAsArray()[15, 12] == 22
    ds = None

    test_py_scripts.run_py_script(script_path, "gdal_edit", filename + " -stats")

    ds = gdal.Open(filename)
    stat_min = ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM")
    assert stat_min is not None and float(stat_min) == 22
    ds = None

    ds = gdal.Open(filename, gdal.GA_Update)
    band = ds.GetRasterBand(1)
    array = band.ReadAsArray()
    array[15, 12] = 26
    band.WriteArray(array)
    ds = band = None

    ds = gdal.Open(filename)
    assert ds.ReadAsArray()[15, 12] == 26
    ds = None

    test_py_scripts.run_py_script(script_path, "gdal_edit", f"{filename} -stats")

    ds = gdal.Open(filename)
    stat_min = ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM")
    assert stat_min is not None and float(stat_min) == 26
    ds = None


###############################################################################
# Test -setstats


def test_gdal_edit_py_6(script_path, tmp_path):

    filename = str(tmp_path / "test_gdal_edit_py.tif")
    shutil.copy(test_py_scripts.get_data_path("gcore") + "byte.tif", filename)

    # original values should be min=74, max=255, mean=126.765 StdDev=22.928470838676
    test_py_scripts.run_py_script(
        script_path, "gdal_edit", filename + " -setstats None None None None"
    )

    ds = gdal.Open(filename)
    stat_min = ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM")
    assert stat_min is not None and float(stat_min) == 74
    stat_max = ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MAXIMUM")
    assert stat_max is not None and float(stat_max) == 255
    stat_mean = ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MEAN")
    assert not (
        stat_mean is None or float(stat_mean) != pytest.approx(126.765, abs=0.001)
    )
    stat_stddev = ds.GetRasterBand(1).GetMetadataItem("STATISTICS_STDDEV")
    assert not (
        stat_stddev is None or float(stat_stddev) != pytest.approx(22.928, abs=0.001)
    )

    ds = None

    test_py_scripts.run_py_script(
        script_path, "gdal_edit", filename + " -setstats 22 217 100 30"
    )

    ds = gdal.Open(filename)
    stat_min = ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM")
    assert stat_min is not None and float(stat_min) == 22
    stat_max = ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MAXIMUM")
    assert stat_max is not None and float(stat_max) == 217
    stat_mean = ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MEAN")
    assert stat_mean is not None and float(stat_mean) == 100
    stat_stddev = ds.GetRasterBand(1).GetMetadataItem("STATISTICS_STDDEV")
    assert stat_stddev is not None and float(stat_stddev) == 30
    ds = None


###############################################################################
# Test -scale and -offset


def test_gdal_edit_py_7(script_path, tmp_path):

    filename = str(tmp_path / "test_gdal_edit_py.tif")
    shutil.copy(test_py_scripts.get_data_path("gcore") + "byte.tif", filename)

    test_py_scripts.run_py_script(
        script_path, "gdal_edit", filename + " -scale 2 -offset 3"
    )
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetScale() == 2
    assert ds.GetRasterBand(1).GetOffset() == 3
    ds = None

    shutil.copy(test_py_scripts.get_data_path("gcore") + "1bit_2bands.tif", filename)
    test_py_scripts.run_py_script(
        script_path, "gdal_edit", filename + " -scale 2 4 -offset 10 20"
    )

    ds = gdal.Open(filename)
    for i in [1, 2]:
        assert ds.GetRasterBand(i).GetScale() == i * 2
        assert ds.GetRasterBand(i).GetOffset() == i * 10

    ds = None


###############################################################################
# Test -colorinterp_X


def test_gdal_edit_py_8(script_path, tmp_path):

    filename = str(tmp_path / "test_gdal_edit_py.tif")

    gdal.Translate(
        filename,
        test_py_scripts.get_data_path("gcore") + "byte.tif",
        options="-b 1 -b 1 -b 1 -b 1 -co PHOTOMETRIC=RGB -co ALPHA=NO",
    )

    test_py_scripts.run_py_script(
        script_path, "gdal_edit", filename + " -colorinterp_4 alpha"
    )

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand

    test_py_scripts.run_py_script(
        script_path, "gdal_edit", filename + " -colorinterp_4 undefined"
    )

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_Undefined
    ds = None


###############################################################################


def test_gdal_edit_py_unsetrpc(script_path, tmp_path):

    filename = str(tmp_path / "test_gdal_edit_py.tif")

    gdal.Translate(filename, test_py_scripts.get_data_path("gcore") + "byte_rpc.tif")

    test_py_scripts.run_py_script(script_path, "gdal_edit", filename + " -unsetrpc")

    ds = gdal.Open(filename)
    assert not ds.GetMetadata("RPC")
    ds = None


###############################################################################
# Test -a_coord_epoch


def test_gdal_edit_py_epoch(script_path, tmp_path):

    filename = str(tmp_path / "test_gdal_edit_py.tif")
    shutil.copy(test_py_scripts.get_data_path("gcore") + "byte.tif", filename)

    test_py_scripts.run_py_script(
        script_path, "gdal_edit", filename + " -a_coord_epoch 2021.3"
    )

    ds = gdal.Open(filename)
    srs = ds.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3
    ds = None
