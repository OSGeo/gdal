#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Benchmarking of ogr2ogr
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

from osgeo import gdal, ogr, osr

# Must be set to run the test_XXX functions under the benchmark fixture
pytestmark = [
    pytest.mark.require_driver("GPKG"),
    pytest.mark.usefixtures("decorate_with_benchmark"),
]


def create_file(filename, numfeatures=50000):
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds.CreateLayer("test", srs=srs)
    for i in range(20):
        lyr.CreateField(ogr.FieldDefn(f"field{i}"))
    f = ogr.Feature(lyr.GetLayerDefn())
    for i in range(20):
        f.SetField(f"field{i}", f"value{i}")
    lyr.StartTransaction()
    for i in range(numfeatures):
        f.SetFID(-1)
        g = ogr.Geometry(ogr.wkbPoint)
        g.SetPoint_2D(0, 400000 + i, i)
        f.SetGeometry(g)
        lyr.CreateFeature(f)
    lyr.CommitTransaction()


@pytest.fixture()
def source_file(tmp_vsimem, request):
    filename = str(tmp_vsimem / "source_file.gpkg")
    create_file(filename, numfeatures=request.param)
    return filename


@pytest.mark.parametrize("source_file", [50000], indirect=True)
def test_ogr2ogr(tmp_vsimem, source_file):
    filename = str(tmp_vsimem / "test_ogr2ogr.gpkg")
    if gdal.VSIStatL(filename):
        gdal.Unlink(filename)
    gdal.VectorTranslate(filename, source_file)


@pytest.mark.parametrize("source_file", [10000], indirect=True)
def test_ogr2ogr_reproject(tmp_vsimem, source_file):
    filename = str(tmp_vsimem / "test_ogr2ogr.gpkg")
    if gdal.VSIStatL(filename):
        gdal.Unlink(filename)
    gdal.VectorTranslate(filename, source_file, dstSRS="EPSG:4326", reproject=True)
