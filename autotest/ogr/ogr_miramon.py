#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  MiraMon driver testing.
# Author:   Abel Pau <a.pau@creaf.uab.cat>
#
###############################################################################
# Copyright (c) 2023, XAvier Pons
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os
import shutil
import struct
import sys
import time

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Open MiraMon file


def test_ogr_miramon_1():

    miramon_drv = ogr.GetDriverByName("MiraMon Vector")
    miramon_drv.DeleteDataSource("tmp")

    gdaltest.shape_ds = miramon_drv.CreateDataSource("tmp")

    assert gdaltest.shape_ds is not None


###############################################################################
# Test reading of MiraMon point geometry
#


def test_ogr_miramon_point_read():

    if not ogrtest.have_read_miramon:
        pytest.skip()

    kml_ds = ogr.Open("data/miramon/Points/SimplePointsFile.pnt")

    lyr = kml_ds.GetLayerByName("SimplePointsFile")
    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    wkt = "POINT(513.49 848.81 0)"

    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    assert feat is not None, "expected feature not found."

    wkt = "POINT(342.33 715.68 50)"

    assert not ogrtest.check_feature_geometry(feat, wkt)

    feat = lyr.GetNextFeature()
    assert feat is not None, "expected feature not found."
    
    wkt = "POINT(594.50 722.69 0)"

    assert not ogrtest.check_feature_geometry(feat, wkt)


def test_ogr_shape_cleanup():

    if gdaltest.shape_ds is None:
        pytest.skip()

    gdaltest.shape_ds = None

    miramon_drv = ogr.GetDriverByName("ESRI Shapefile")
    
