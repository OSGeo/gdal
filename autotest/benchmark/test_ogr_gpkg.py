#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Benchmarking of GeoPackage driver
# Author:   Even Rouault <even dot rouault at spatialys.com>
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

import pytest

from osgeo import ogr

# Must be set to run the test_XXX functions under the benchmark fixture
pytestmark = [
    pytest.mark.require_driver("GPKG"),
    pytest.mark.usefixtures("decorate_with_benchmark"),
]


def create_file(filename, numfeatures=50000):
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    lyr = ds.CreateLayer("test")
    for i in range(20):
        lyr.CreateField(ogr.FieldDefn(f"field{i}"))
    f = ogr.Feature(lyr.GetLayerDefn())
    for i in range(20):
        f.SetField(f"field{i}", f"value{i}")
    lyr.StartTransaction()
    for i in range(numfeatures):
        f.SetFID(-1)
        g = ogr.Geometry(ogr.wkbPoint)
        g.SetPoint_2D(0, i, i)
        f.SetGeometry(g)
        lyr.CreateFeature(f)
    lyr.CommitTransaction()


def test_ogr_gpkg_create(tmp_vsimem):
    filename = str(tmp_vsimem / "test.gpkg")
    create_file(filename)


@pytest.fixture()
def source_file(tmp_vsimem):
    filename = str(tmp_vsimem / "test.gpkg")
    create_file(filename)
    return filename


def test_ogr_gpkg_spatial_index(source_file):
    ds = ogr.Open(source_file)
    lyr = ds.GetLayer(0)
    lyr.SetSpatialFilterRect(1000, 1000, 10000, 10000)
    count = 0
    for f in lyr:
        count += 1
    assert count == 10000 - 1000 + 1
