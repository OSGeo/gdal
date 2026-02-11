#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_footprint testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import ogrtest
import pytest
import test_cli_utilities

from osgeo import ogr

pytestmark = [
    pytest.mark.require_geos,
    pytest.mark.skipif(
        test_cli_utilities.get_gdal_footprint_path() is None,
        reason="gdal_footprint not available",
    ),
]


@pytest.fixture()
def gdal_footprint_path():
    return test_cli_utilities.get_gdal_footprint_path()


###############################################################################


@pytest.mark.require_driver("GeoJSON")
def test_gdal_footprint_basic(gdal_footprint_path, tmp_path):

    footprint_json = str(tmp_path / "out_footprint.json")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_footprint_path + f" -q -f GeoJSON ../gcore/data/byte.tif {footprint_json}"
    )
    assert err is None or err == "", "got error/warning"

    ds = ogr.Open(footprint_json)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTIPOLYGON (((440720 3751320,440720 3750120,441920 3750120,441920 3751320,440720 3751320)))",
    )

    ds = None


###############################################################################


def test_gdal_footprint_appending(gdal_footprint_path, tmp_path):

    footprint_shp = str(tmp_path / "out_footprint.shp")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_footprint_path + f" ../gcore/data/byte.tif {footprint_shp}"
    )
    assert err is None or err == "", "got error/warning"

    _, err = gdaltest.runexternal_out_and_err(
        gdal_footprint_path + f" ../gcore/data/byte.tif {footprint_shp}"
    )
    assert err is None or err == "", "got error/warning"

    ds = ogr.Open(footprint_shp)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((440720 3751320,440720 3750120,441920 3750120,441920 3751320,440720 3751320))",
    )
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((440720 3751320,440720 3750120,441920 3750120,441920 3751320,440720 3751320))",
    )

    ds = None


###############################################################################


def test_gdal_footprint_overwrite(gdal_footprint_path, tmp_path):

    footprint_shp = str(tmp_path / "out_footprint.shp")

    _, err = gdaltest.runexternal_out_and_err(
        gdal_footprint_path + f" ../gcore/data/byte.tif {footprint_shp}"
    )
    assert err is None or err == "", "got error/warning"

    _, err = gdaltest.runexternal_out_and_err(
        gdal_footprint_path + f" -overwrite ../gcore/data/byte.tif {footprint_shp}"
    )
    assert err is None or err == "", "got error/warning"

    ds = ogr.Open(footprint_shp)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((440720 3751320,440720 3750120,441920 3750120,441920 3751320,440720 3751320))",
    )

    ds = None


###############################################################################


def test_gdal_footprint_wrong_input_dataset(gdal_footprint_path, tmp_vsimem):

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_footprint_path} {tmp_vsimem}/in.tif {tmp_vsimem}/out.json"
    )
    assert "ret code = 1" in err


###############################################################################


def test_gdal_footprint_wrong_output_dataset(gdal_footprint_path):

    _, err = gdaltest.runexternal_out_and_err(
        gdal_footprint_path + " ../gcore/data/byte.tif /invalid/output/file.json"
    )
    assert "ret code = 1" in err
