#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR MiraMon vector driver.
# Author:   Abel Pau <a dot pau at creaf.uab.cat>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
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

# import os
# import pdb

# import gdaltest
# import ogrtest
import pytest

# from osgeo import gdal, ogr, osr
from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("MiraMonVector")

###############################################################################
# basic point test


def test_ogr_miramon_simple_point():
    try:
        ds = gdal.OpenEx("data/miramon/Points/SimplePoints/SimplePointsFile.pnt")

        assert ds is not None, "Failed to get dataset"

        lyr = ds.GetLayer(0)

        assert lyr is not None, "Failed to get layer"

        assert lyr.GetFeatureCount() == 3
        assert lyr.GetGeomType() == ogr.wkbPoint

        f = lyr.GetNextFeature()
        assert f is not None, "Failed to get feature"
        assert f.GetFID() == 0
        assert (
            f.GetGeometryRef().ExportToWkt()
            == "POINT (513.488106565226 848.806850618409)"
        )
        assert f.GetField("ID_GRAFIC") == 0
        assert f.GetFieldAsString("ATT1") == "A"
        assert f.GetFieldAsString("ATTRIBUTE_2") == "B"

        f = lyr.GetNextFeature()
        assert f is not None, "Failed to get feature"
        assert (
            f.GetGeometryRef().ExportToWkt()
            == "POINT (342.325404376834 715.680304471881)"
        )
        assert f.GetField("ID_GRAFIC") == 1
        assert f.GetFieldAsString("ATT1") == "C"
        assert f.GetFieldAsString("ATTRIBUTE_2") == "D"

        f = lyr.GetNextFeature()
        assert f is not None, "Failed to get feature"
        assert (
            f.GetGeometryRef().ExportToWkt()
            == "POINT (594.503182156354 722.692543360232)"
        )
        assert f.GetField("ID_GRAFIC") == 2
        assert f.GetFieldAsString("ATT1") == ""
        assert f.GetFieldAsString("ATTRIBUTE_2") == ""

        ds = None
    except Exception as e:
        pytest.fail(f"Test failed with exception: {e}")


###############################################################################
# basic linestring test


def test_ogr_miramon_simple_arc():
    try:
        ds = gdal.OpenEx("data/miramon/Arcs/SimpleArcs/SimpleArcFile.arc")

        assert ds is not None, "Failed to get dataset"

        lyr = ds.GetLayer(0)

        assert lyr is not None, "Failed to get layer"

        assert lyr.GetFeatureCount() == 4
        assert lyr.GetGeomType() == ogr.wkbLineString

        f = lyr.GetNextFeature()
        assert f is not None, "Failed to get feature"
        assert f.GetFID() == 0
        assert (
            f.GetGeometryRef().ExportToWkt()
            == "LINESTRING (351.333967649907 610.58039961936,474.450999048575 824.784015223546,758.721217887776 838.797335870549,1042.99143672698 610.58039961936,1369.30161750719 562.534728829636)"
        )
        assert f.GetField("ID_GRAFIC") == 0
        assert f.GetField("N_VERTEXS") == 5
        assert f.GetField("LONG_ARC") == 1226.052755
        assert f.GetField("NODE_INI") == 0
        assert f.GetField("NODE_FI") == 1
        assert f.GetFieldAsString("ATT1") == "A"
        assert f.GetFieldAsString("ATT2") == "B"

        ds = None
    except Exception as e:
        pytest.fail(f"Test failed with exception: {e}")


###############################################################################
# basic polygon test


# def test_ogr_miramon_simple_polygon():
#    try:
#        ds = gdal.OpenEx("data/miramon/Polygons/SimplePolygons/SimplePolFile.pol")
#        assert ds is not None, "Failed to get dataset"

#        lyr = ds.GetLayer(0)

#        assert lyr is not None, "Failed to get layer"

#        assert lyr.GetFeatureCount() == 3
#        assert lyr.GetGeomType() == ogr.wkbPolygon

#        f = lyr.GetNextFeature()
#        assert f is not None, "Failed to get feature"
#        assert f.GetFID() == 1
#        assert (
#            f.GetGeometryRef().ExportToWkt()
#            == "POLYGON ((335.318744053333 769.731684110321,552.525214081877 856.814462416696,775.737392959137 707.672692673594,648.616555661325 493.469077069408,386.367269267414 498.473834443337,335.318744053333 769.731684110321))"
#        )
#        assert f.GetField("ID_GRAFIC") == 1
#        assert f.GetField("N_VERTEXS") == 6
#        assert f.GetField("PERIMETRE") == 1289.866489
#        assert f.GetField("AREA") == 112471.221989
#        assert f.GetField("N_ARCS") == 1
#        assert f.GetField("N_POLIG") == 1
#        assert f.GetFieldAsString("ATT1") == "A"
#        assert f.GetFieldAsString("ATT2") == "B"

#        ds = None
#    except Exception as e:
#        pytest.fail(f"Test failed with exception: {e}")
