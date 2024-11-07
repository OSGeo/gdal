#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdallocationinfo.py testing
# Author:   Idan Miara <idan@miara.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
# Copyright (c) 2021, Idan Miara <idan@miara.com>
#
# SPDX-License-Identifier: MIT
###############################################################################
import pytest

# test that numpy is available, if not skip all tests
np = pytest.importorskip("numpy")
pytest.importorskip("osgeo_utils.samples.gdallocationinfo")

from itertools import product

import test_py_scripts
from numpy.testing import assert_allclose

from osgeo import gdal, osr
from osgeo_utils.auxiliary.osr_util import get_srs, get_transform, transform_points
from osgeo_utils.auxiliary.raster_creation import copy_raster_and_add_overviews
from osgeo_utils.auxiliary.util import open_ds
from osgeo_utils.samples import gdallocationinfo
from osgeo_utils.samples.gdallocationinfo import LocationInfoSRS

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdallocationinfo") is None,
    reason="gdallocationinfo.py not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdallocationinfo")


def test_gdallocationinfo_py_1(script_path):
    """Test basic usage"""

    ret = test_py_scripts.run_py_script(
        script_path, "gdallocationinfo", " ../gcore/data/byte.tif 0 0"
    )
    ret = ret.replace("\r\n", "\n")
    expected_ret = """Report:
  Location: (0P,0L)
  Band 1:
    Value: 107"""
    assert ret.startswith(expected_ret)


def test_gdallocationinfo_py_3(script_path):
    """Test -valonly"""

    ret = test_py_scripts.run_py_script(
        script_path, "gdallocationinfo", " -b 1 -valonly ../gcore/data/byte.tif 0 0"
    )
    expected_ret = """107"""
    assert ret.startswith(expected_ret)


def test_gdallocationinfo_py_4(script_path):
    """Test -geoloc"""

    ret = test_py_scripts.run_py_script(
        script_path,
        "gdallocationinfo",
        " -geoloc ../gcore/data/byte.tif 440720.000 3751320.000",
    )
    ret = ret.replace("\r\n", "\n")
    expected_ret = """Report:
  Location: (0P,0L)
  Band 1:
    Value: 107"""
    assert ret.startswith(expected_ret)


def test_gdallocationinfo_py_6(script_path, tmp_path):
    """Test -overview"""

    test_tif = str(tmp_path / "test_gdallocationinfo_py_6.tif")

    src_ds = gdal.Open("../gcore/data/byte.tif")
    ds = gdal.GetDriverByName("GTiff").CreateCopy(test_tif, src_ds)
    ds.BuildOverviews("AVERAGE", overviewlist=[2])
    ds = None
    src_ds = None

    ret = test_py_scripts.run_py_script(
        script_path,
        "gdallocationinfo",
        f" {test_tif} 10 10 -overview 1",
    )

    expected_ret = """Value: 130"""
    assert expected_ret in ret


def test_gdallocationinfo_py_wgs84(script_path):

    ret = test_py_scripts.run_py_script(
        script_path,
        "gdallocationinfo",
        " -valonly -wgs84 ../gcore/data/byte.tif -117.6354747 33.8970515",
    )

    expected_ret = """115"""
    assert expected_ret in ret


def test_gdallocationinfo_py_7(tmp_path):
    filename_template = str(tmp_path / "byte{}.tif")
    overview_list = [2]
    overview_list, file_list = copy_raster_and_add_overviews(
        filename_src="../gcore/data/byte.tif",
        output_filename_template=filename_template,
        overview_list=overview_list,
    )

    ds = open_ds(filename_template.format(""))
    ds_srs = ds.GetSpatialRef()
    gt = ds.GetGeoTransform()
    ovr_fact = 1
    pix_offset = 0
    p0 = list(i * ovr_fact + pix_offset for i in range(0, 3))
    l0 = list(i * ovr_fact + pix_offset for i in range(0, 3))
    p0, l0 = zip(*product(p0, l0))
    x0 = list(gt[0] + pixel * gt[1] for pixel in p0)
    y0 = list(gt[3] + line * gt[5] for line in l0)

    dtype = np.float64
    p0 = np.array(p0, dtype=dtype)
    l0 = np.array(l0, dtype=dtype)
    x0 = np.array(x0, dtype=dtype)
    y0 = np.array(y0, dtype=dtype)

    expected_results = [
        [
            (0.0, 0.0, 107),
            (0.0, 1.0, 115),
            (0.0, 2.0, 115),
            (1.0, 0.0, 123),
            (1.0, 1.0, 132),
            (1.0, 2.0, 132),
            (2.0, 0.0, 132),
            (2.0, 1.0, 107),
            (2.0, 2.0, 140),
        ],
        [
            (0.0, 0.0, 120),
            (0.0, 0.5, 120),
            (0.0, 1.0, 132),
            (0.5, 0.0, 120),
            (0.5, 0.5, 120),
            (0.5, 1.0, 132),
            (1.0, 0.0, 124),
            (1.0, 0.5, 124),
            (1.0, 1.0, 129),
        ],
    ]
    expected_results = np.array(expected_results, dtype=dtype)

    do_print = False
    for ovr_idx, f in enumerate(overview_list):
        for srs in [LocationInfoSRS.PixelLine, LocationInfoSRS.SameAsDS_SRS, 4326]:
            if srs == LocationInfoSRS.PixelLine:
                x = p0
                y = l0
            elif srs == LocationInfoSRS.SameAsDS_SRS:
                x = x0
                y = y0
            else:
                points_srs = get_srs(srs, axis_order=osr.OAMS_TRADITIONAL_GIS_ORDER)
                ct = get_transform(ds_srs, points_srs)
                x = x0.copy()
                y = y0.copy()
                transform_points(ct, x, y)
            pixels, lines, results = gdallocationinfo.gdallocationinfo(
                filename_or_ds=ds,
                ovr_idx=ovr_idx,
                x=x,
                y=y,
                transform_round_digits=2,
                return_ovr_pixel_line=True,
                srs=srs,
                axis_order=osr.OAMS_TRADITIONAL_GIS_ORDER,
            )

            actual = list(zip(pixels, lines, *results))
            actual = np.array(actual, dtype=dtype)
            expected = expected_results[ovr_idx]
            if do_print:
                print(actual)
                print(expected)
                outputs = list(zip(x, y, pixels, lines, *results))
                print(f"ovr: {ovr_idx}, srs: {srs}, x/y/pixel/line/result: {outputs}")
            assert_allclose(expected, actual, rtol=1e-4, atol=1e-3)
