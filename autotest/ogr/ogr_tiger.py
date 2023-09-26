#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR TIGER driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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

import os
import pathlib

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("Tiger")

###############################################################################


@pytest.fixture(scope="module")
def TGR01001_dir():

    gdaltest.download_or_skip(
        "http://www2.census.gov/geo/tiger/tiger2006se/AL/TGR01001.ZIP", "TGR01001.ZIP"
    )

    dirname = pathlib.Path("tmp") / "cache" / "TGR01001"

    try:
        os.stat("tmp/cache/TGR01001/TGR01001.MET")
    except OSError:
        try:
            try:
                os.stat("tmp/cache/TGR01001")
            except OSError:
                os.mkdir("tmp/cache/TGR01001")
            gdaltest.unzip("tmp/cache/TGR01001", "tmp/cache/TGR01001.ZIP")
            try:
                os.stat("tmp/cache/TGR01001/TGR01001.MET")
            except OSError:
                pytest.skip()
        except Exception:
            pytest.skip()

    return dirname


def test_ogr_tiger_1(TGR01001_dir):

    tiger_ds = ogr.Open(TGR01001_dir)
    assert tiger_ds is not None

    tiger_ds = None
    # also test opening with a filename (#4443)
    tiger_ds = ogr.Open(TGR01001_dir / "TGR01001.RT1")
    assert tiger_ds is not None

    # Check a few features.
    cc_layer = tiger_ds.GetLayerByName("CompleteChain")
    assert cc_layer.GetFeatureCount() == 19289, "wrong cc feature count"

    feat = cc_layer.GetNextFeature()
    feat = cc_layer.GetNextFeature()
    feat = cc_layer.GetNextFeature()

    assert (
        feat.TLID == 2833200 and feat.FRIADDL is None and feat.BLOCKL == 5000
    ), "wrong attribute on cc feature."

    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING (-86.4402 32.504137,-86.440313 32.504009,-86.440434 32.503884,-86.440491 32.503805,-86.44053 32.503757,-86.440578 32.503641,-86.440593 32.503515,-86.440588 32.503252,-86.440596 32.50298)",
        max_error=0.000001,
    )

    feat = tiger_ds.GetLayerByName("TLIDRange").GetNextFeature()
    assert (
        feat.MODULE == "TGR01001" and feat.TLMINID == 2822718
    ), "got wrong TLIDRange attributes"


###############################################################################
# Run test_ogrsf


def test_ogr_tiger_2(TGR01001_dir):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" -ro {TGR01001_dir}"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Load into a /vsimem instance to test virtualization.


def test_ogr_tiger_4(tmp_vsimem, TGR01001_dir):

    # load all the files into memory.
    for filename in gdal.ReadDir(TGR01001_dir):

        if filename.startswith("."):
            continue

        data = open(TGR01001_dir / filename, "r").read()

        f = gdal.VSIFOpenL(tmp_vsimem / filename, "wb")
        gdal.VSIFWriteL(data, 1, len(data), f)
        gdal.VSIFCloseL(f)

    # Try reading.
    ogrtest.tiger_ds = ogr.Open(tmp_vsimem / "TGR01001.RT1")
    assert ogrtest.tiger_ds is not None, "fail to open."

    ogrtest.tiger_ds = None
    # also test opening with a filename (#4443)
    ogrtest.tiger_ds = ogr.Open(tmp_vsimem / "TGR01001.RT1")
    assert ogrtest.tiger_ds is not None

    # Check a few features.
    cc_layer = ogrtest.tiger_ds.GetLayerByName("CompleteChain")
    assert cc_layer.GetFeatureCount() == 19289, "wrong cc feature count"

    feat = cc_layer.GetNextFeature()
    feat = cc_layer.GetNextFeature()
    feat = cc_layer.GetNextFeature()

    assert (
        feat.TLID == 2833200 and feat.FRIADDL is None and feat.BLOCKL == 5000
    ), "wrong attribute on cc feature."

    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING (-86.4402 32.504137,-86.440313 32.504009,-86.440434 32.503884,-86.440491 32.503805,-86.44053 32.503757,-86.440578 32.503641,-86.440593 32.503515,-86.440588 32.503252,-86.440596 32.50298)",
        max_error=0.000001,
    )

    feat = ogrtest.tiger_ds.GetLayerByName("TLIDRange").GetNextFeature()
    assert (
        feat.MODULE == "TGR01001" and feat.TLMINID == 2822718
    ), "got wrong TLIDRange attributes"
