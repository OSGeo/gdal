#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR SDTS driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2009, Even Rouault <even dot rouault at spatialys.com>
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
import pytest

from osgeo import ogr

pytestmark = pytest.mark.require_driver("OGR_SDTS")

###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Test reading


def test_ogr_sdts_1():

    ds = ogr.Open("data/sdts/D3607551_rd0s_1_sdts_truncated/TR01CATD.DDF")

    assert ds is not None

    layers = [
        ("ARDF", 164, ogr.wkbNone, [("ENTITY_LABEL", "1700005")]),
        ("ARDM", 21, ogr.wkbNone, [("ROUTE_NUMBER", "SR 1200")]),
        (
            "AHDR",
            1,
            ogr.wkbNone,
            [
                (
                    "BANNER",
                    "USGS-NMD  DLG DATA - CHARACTER FORMAT - 09-29-87 VERSION                ",
                )
            ],
        ),
        ("NP01", 4, ogr.wkbPoint, [("RCID", "1")]),
        ("NA01", 34, ogr.wkbPoint, [("RCID", "2")]),
        ("NO01", 88, ogr.wkbPoint, [("RCID", "1")]),
        ("LE01", 27, ogr.wkbLineString, [("RCID", "1")]),
        ("PC01", 35, ogr.wkbPolygon, [("RCID", "1")]),
    ]

    for layer in layers:
        lyr = ds.GetLayerByName(layer[0])
        assert lyr is not None, "could not get layer %s" % (layer[0])
        with gdaltest.error_handler():
            assert (
                lyr.GetFeatureCount() == layer[1]
            ), "wrong number of features for layer %s : %d. %d were expected " % (
                layer[0],
                lyr.GetFeatureCount(),
                layer[1],
            )
        assert lyr.GetLayerDefn().GetGeomType() == layer[2]
        feat_read = lyr.GetNextFeature()
        for item in layer[3]:
            if feat_read.GetFieldAsString(item[0]) != item[1]:
                print(layer[0])
                print('"%s"' % (item[1]))
                pytest.fail('"%s"' % (feat_read.GetField(item[0])))

    # Check that we get non-empty polygons
    lyr = ds.GetLayerByName("PC01")
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g
    assert g.GetGeometryType() == ogr.wkbPolygon25D
    assert not g.IsEmpty()

    ds = None


###############################################################################
#
