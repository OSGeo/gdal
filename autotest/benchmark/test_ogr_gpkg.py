#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Benchmarking of GeoPackage driver
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
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
