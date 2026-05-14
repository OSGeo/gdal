#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGRAPISPY
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
from difflib import unified_diff

import gdaltest
import pytest
import test_py_scripts

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("CSV")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Basic test without snapshotting


def test_ograpispy_1(tmp_path):

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("leaks memory")

    fname = str(tmp_path / "ograpispy_1.py")

    os.environ["OGR_API_SPY_FILE"] = fname
    test_py_scripts.run_py_script("data", "testograpispy", "")
    del os.environ["OGR_API_SPY_FILE"]

    if not os.path.exists(fname):
        pytest.skip("OGR API spy not enabled")

    ref_data = open("data/testograpispy.py", "rt").read()
    ref_data = ref_data.replace("gdal.DontUseExceptions()\n", "")
    ref_data = ref_data.replace("ogr.DontUseExceptions()\n", "")
    got_data = open(fname, "rt").read()

    if ref_data != got_data:
        print()
        for line in unified_diff(
            ref_data.splitlines(),
            got_data.splitlines(),
            fromfile="expected",
            tofile="got",
            lineterm="",
        ):
            print(line)
        pytest.fail("did not get expected script")


###############################################################################
# With snapshotting


def test_ograpispy_2(tmp_path):

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("leaks memory")

    os.environ["OGR_API_SPY_FILE"] = str(tmp_path / "ograpispy_1.py")
    test_py_scripts.run_py_script("data", "testograpispy", "")
    del os.environ["OGR_API_SPY_FILE"]

    if not os.path.exists(tmp_path / "ograpispy_1.py"):
        pytest.skip("OGR API spy not enabled")

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        tmp_path / "ograpispy_2.shp"
    )
    lyr = ds.CreateLayer("ograpispy_2")
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    ds = None

    with gdal.config_options(
        {
            "OGR_API_SPY_FILE": str(tmp_path / "ograpispy_2.py"),
            "OGR_API_SPY_SNAPSHOT_PATH": str(tmp_path),
        }
    ):
        ds = ogr.Open(tmp_path / "ograpispy_2.shp", update=1)
        lyr = ds.GetLayer(0)
        lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
        ds = None

    ds = ogr.Open(tmp_path / "snapshot_1/source/ograpispy_2.shp")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0
    ds = None

    ds = ogr.Open(tmp_path / "snapshot_1/working/ograpispy_2.shp", update=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1

    # Add a feature to check that running the script will work
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    ds = None

    # Check script
    test_py_scripts.run_py_script(tmp_path, "ograpispy_2", "")

    ds = ogr.Open(tmp_path / "snapshot_1/working/ograpispy_2.shp")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None
