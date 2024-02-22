#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_footprint testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
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


def test_gdal_footprint_basic(gdal_footprint_path, tmp_path):

    footprint_json = str(tmp_path / "out_footprint.json")

    (_, err) = gdaltest.runexternal_out_and_err(
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

    (_, err) = gdaltest.runexternal_out_and_err(
        gdal_footprint_path + f" ../gcore/data/byte.tif {footprint_shp}"
    )
    assert err is None or err == "", "got error/warning"

    (_, err) = gdaltest.runexternal_out_and_err(
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

    (_, err) = gdaltest.runexternal_out_and_err(
        gdal_footprint_path + f" ../gcore/data/byte.tif {footprint_shp}"
    )
    assert err is None or err == "", "got error/warning"

    (_, err) = gdaltest.runexternal_out_and_err(
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

    (_, err) = gdaltest.runexternal_out_and_err(
        f"{gdal_footprint_path} {tmp_vsimem}/in.tif {tmp_vsimem}/out.json"
    )
    assert "ret code = 1" in err


###############################################################################


def test_gdal_footprint_wrong_output_dataset(gdal_footprint_path):

    (_, err) = gdaltest.runexternal_out_and_err(
        gdal_footprint_path + " ../gcore/data/byte.tif /invalid/output/file.json"
    )
    assert "ret code = 1" in err
