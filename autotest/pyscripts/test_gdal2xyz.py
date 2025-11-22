#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal2xyz.py testing
# Author:   Idan Miara <idan@miara.com>
#
###############################################################################
# Copyright (c) 2021, Idan Miara <idan@miara.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest
import test_py_scripts

# test that osgeo_utils is available, if not skip all tests
pytest.importorskip("osgeo_utils")
gdaltest.importorskip_gdal_array()
pytest.importorskip("numpy")

from itertools import product

import numpy as np

from osgeo import gdal
from osgeo.gdal_array import flip_code
from osgeo_utils import gdal2xyz
from osgeo_utils.auxiliary.raster_creation import create_flat_raster
from osgeo_utils.samples import gdallocationinfo

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdal2xyz") is None,
    reason="gdal2xyz not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdal2xyz")


###############################################################################
#


def test_gdal2xyz_help(script_path):

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("fails on sanitize for unknown reason")

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal2xyz", "--help"
    )


###############################################################################
#


def test_gdal2xyz_version(script_path):

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("fails on sanitize for unknown reason")

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal2xyz", "--version"
    )


def test_gdal2xyz_py_1():
    """test get_ovr_idx, create_flat_raster"""
    pytest.importorskip("numpy")

    size = (3, 3)
    origin = (500_000, 0)
    pixel_size = (10, -10)
    nodata_value = 255
    band_count = 2
    dt = gdal.GDT_UInt8
    np_dt = flip_code(dt)
    ds = create_flat_raster(
        filename="",
        size=size,
        origin=origin,
        pixel_size=pixel_size,
        nodata_value=nodata_value,
        fill_value=nodata_value,
        band_count=band_count,
        dt=dt,
    )
    src_nodata = nodata_value
    np.random.seed()
    for bnd_idx in range(band_count):
        bnd = ds.GetRasterBand(bnd_idx + 1)
        data = (np.random.random_sample(size) * 255).astype(np_dt)
        data[1, 1] = src_nodata
        bnd.WriteArray(data, 0, 0)
    dst_nodata = 254
    for pre_allocate_np_arrays, skip_nodata in product((True, False), (True, False)):
        geo_x, geo_y, data, nodata = gdal2xyz.gdal2xyz(
            ds,
            None,
            return_np_arrays=True,
            src_nodata=src_nodata,
            dst_nodata=dst_nodata,
            skip_nodata=skip_nodata,
            pre_allocate_np_arrays=pre_allocate_np_arrays,
            progress_callback=None,
        )
        _pixels, _lines, data2 = gdallocationinfo.gdallocationinfo(
            ds,
            x=geo_x,
            y=geo_y,
            resample_alg=gdal.GRIORA_NearestNeighbour,
            srs=gdallocationinfo.LocationInfoSRS.SameAsDS_SRS,
        )
        data2[data2 == src_nodata] = dst_nodata
        assert np.all(np.equal(data, data2))


###############################################################################
# Test -b at beginning


def test_gdal2xyz_py_2(script_path, tmp_path):

    out_xyz = str(tmp_path / "out.xyz")

    arguments = "-b 1"
    arguments += " " + test_py_scripts.get_data_path("gcore") + "byte.tif "
    arguments += out_xyz

    _, err = test_py_scripts.run_py_script(
        script_path, "gdal2xyz", arguments, return_stderr=True
    )
    assert "UseExceptions" not in err

    assert os.path.exists(out_xyz)


###############################################################################
# Test -b at end


def test_gdal2xyz_py_3(script_path, tmp_path):

    out_xyz = str(tmp_path / "out.xyz")

    arguments = test_py_scripts.get_data_path("gcore") + "byte.tif "
    arguments += out_xyz
    arguments += " -b 1"

    test_py_scripts.run_py_script(script_path, "gdal2xyz", arguments)

    assert os.path.exists(out_xyz)


###############################################################################
# Test -srcnodata and -dstnodata


def test_gdal2xyz_py_srcnodata_dstnodata(script_path, tmp_path):

    out_xyz = str(tmp_path / "out.xyz")

    arguments = "-allbands -srcnodata 0 0 0 -dstnodata 1 2 3"
    arguments += " " + test_py_scripts.get_data_path("gcore") + "rgbsmall.tif "
    arguments += out_xyz

    test_py_scripts.run_py_script(script_path, "gdal2xyz", arguments)

    assert os.path.exists(out_xyz)
    with open(out_xyz, "rb") as f:
        l = f.readline()

    assert l.startswith(b"-44.838604 -22.9343 1 2 3")


###############################################################################
# Test output to /vsistdout/


@pytest.mark.require_driver("XYZ")
def test_gdal2xyz_py_vsistdout(script_path, tmp_path):

    arguments = test_py_scripts.get_data_path("gcore") + "byte.tif /vsistdout/"

    out = test_py_scripts.run_py_script(script_path, "gdal2xyz", arguments)

    out_filename = str(tmp_path / "out.xyz")
    open(out_filename, "wb").write(out.encode("UTF-8"))

    with gdal.Open(out_filename) as ds:
        assert ds.GetGeoTransform() == (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)
        assert ds.GetRasterBand(1).Checksum() == 4672
