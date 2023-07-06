#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Polygonize() algorithm.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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


import struct
from collections import defaultdict

import ogrtest
import pytest

from osgeo import gdal, ogr

###############################################################################
# Test a fairly simple case, with nodata masking.


@pytest.mark.require_driver("AAIGRID")
@pytest.mark.parametrize("is_int_polygonize", [True, False])
def test_polygonize_1(is_int_polygonize):

    src_ds = gdal.Open("data/polygonize_in.grd")
    src_band = src_ds.GetRasterBand(1)

    # Create a memory OGR datasource to put results in.
    mem_drv = ogr.GetDriverByName("Memory")
    mem_ds = mem_drv.CreateDataSource("out")

    mem_layer = mem_ds.CreateLayer("poly", None, ogr.wkbPolygon)

    fd = ogr.FieldDefn("DN", ogr.OFTInteger)
    mem_layer.CreateField(fd)

    # run the algorithm.
    if is_int_polygonize:
        result = gdal.Polygonize(src_band, src_band.GetMaskBand(), mem_layer, 0)
    else:
        result = gdal.FPolygonize(src_band, src_band.GetMaskBand(), mem_layer, 0)
    assert result == 0, "Polygonize failed"

    # Confirm we get the set of expected features in the output layer.

    expected_feature_number = 13
    assert mem_layer.GetFeatureCount() == expected_feature_number

    expect = [107, 123, 115, 115, 140, 148, 123, 140, 100, 101, 102, 156, 103]

    ogrtest.check_features_against_list(mem_layer, "DN", expect)

    # check at least one geometry.
    mem_layer.SetAttributeFilter("dn = 156")
    feat_read = mem_layer.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat_read,
        "POLYGON ((440720 3751200,440720 3751020,440900 3751020,440900 3751200,440720 3751200),(440780 3751140,440840 3751140,440840 3751080,440780 3751080,440780 3751140))",
    )

    feat_read.Destroy()


###############################################################################
# Test a simple case without masking.


@pytest.mark.require_driver("AAIGRID")
def test_polygonize_2():

    src_ds = gdal.Open("data/polygonize_in.grd")
    src_band = src_ds.GetRasterBand(1)

    # Create a memory OGR datasource to put results in.
    mem_drv = ogr.GetDriverByName("Memory")
    mem_ds = mem_drv.CreateDataSource("out")

    mem_layer = mem_ds.CreateLayer("poly", None, ogr.wkbPolygon)

    fd = ogr.FieldDefn("DN", ogr.OFTInteger)
    mem_layer.CreateField(fd)

    # run the algorithm.
    result = gdal.Polygonize(src_band, None, mem_layer, 0)
    assert result == 0, "Polygonize failed"

    # Confirm we get the set of expected features in the output layer.

    expected_feature_number = 17
    assert mem_layer.GetFeatureCount() == expected_feature_number

    expect = [
        107,
        123,
        115,
        132,
        115,
        140,
        132,
        132,
        148,
        123,
        140,
        132,
        100,
        101,
        102,
        156,
        103,
    ]

    ogrtest.check_features_against_list(mem_layer, "DN", expect)


###############################################################################
# A more involved case with a complex looping.


@pytest.mark.require_driver("AAIGRID")
def test_polygonize_3():

    src_ds = gdal.Open("data/polygonize_in_2.grd")
    src_band = src_ds.GetRasterBand(1)

    # Create a memory OGR datasource to put results in.
    mem_drv = ogr.GetDriverByName("Memory")
    mem_ds = mem_drv.CreateDataSource("out")

    mem_layer = mem_ds.CreateLayer("poly", None, ogr.wkbPolygon)

    fd = ogr.FieldDefn("DN", ogr.OFTInteger)
    mem_layer.CreateField(fd)

    # run the algorithm.
    result = gdal.Polygonize(src_band, None, mem_layer, 0)
    assert result == 0, "Polygonize failed"

    # Confirm we get the expected count of features.

    expected_feature_number = 125
    assert mem_layer.GetFeatureCount() == expected_feature_number

    # check at least one geometry.
    mem_layer.SetAttributeFilter("dn = 0")
    feat_read = mem_layer.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat_read,
        "POLYGON ((6 -3,6 -40,19 -40,19 -39,25 -39,25 -38,27 -38,27 -37,28 -37,28 -36,29 -36,29 -35,30 -35,30 -34,31 -34,31 -25,30 -25,30 -24,29 -24,29 -23,28 -23,28 -22,27 -22,27 -21,24 -21,24 -20,23 -20,23 -19,26 -19,26 -18,27 -18,27 -17,28 -17,28 -16,29 -16,29 -8,28 -8,28 -7,27 -7,27 -6,26 -6,26 -5,24 -5,24 -4,18 -4,18 -3,6 -3),(11 -7,23 -7,23 -8,24 -8,24 -9,25 -9,25 -16,24 -16,24 -17,23 -17,23 -18,11 -18,11 -7),(11 -22,24 -22,24 -23,26 -23,26 -25,27 -25,27 -33,26 -33,26 -35,24 -35,24 -36,11 -36,11 -22))",
    )

    feat_read.Destroy()


###############################################################################
# Test a simple case without masking but with 8-connectedness.


@pytest.mark.require_driver("AAIGRID")
def test_polygonize_4():

    src_ds = gdal.Open("data/polygonize_in.grd")
    src_band = src_ds.GetRasterBand(1)

    # Create a memory OGR datasource to put results in.
    mem_drv = ogr.GetDriverByName("Memory")
    mem_ds = mem_drv.CreateDataSource("out")

    mem_layer = mem_ds.CreateLayer("poly", None, ogr.wkbPolygon)

    fd = ogr.FieldDefn("DN", ogr.OFTInteger)
    mem_layer.CreateField(fd)

    # run the algorithm.
    result = gdal.Polygonize(src_band, None, mem_layer, 0, ["8CONNECTED=8"])
    assert result == 0, "Polygonize failed"

    # Confirm we get the set of expected features in the output layer.

    expected_feature_number = 16
    assert mem_layer.GetFeatureCount() == expected_feature_number

    expect = [
        107,
        123,
        115,
        132,
        115,
        140,
        132,
        148,
        123,
        140,
        132,
        100,
        101,
        102,
        156,
        103,
    ]

    ogrtest.check_features_against_list(mem_layer, "DN", expect)


###############################################################################
# Test a simple case with two inner wholes touchs at a vertex.


@pytest.mark.require_driver("AAIGRID")
def test_polygonize_5():

    src_ds = gdal.Open("data/polygonize_in_3.grd")
    src_band = src_ds.GetRasterBand(1)

    # Create a memory OGR datasource to put results in.
    mem_drv = ogr.GetDriverByName("Memory")
    mem_ds = mem_drv.CreateDataSource("out")

    mem_layer = mem_ds.CreateLayer("poly", None, ogr.wkbPolygon)

    fd = ogr.FieldDefn("DN", ogr.OFTInteger)
    mem_layer.CreateField(fd)

    # run the algorithm.
    result = gdal.Polygonize(src_band, None, mem_layer, 0)
    assert result == 0, "Polygonize failed"

    # Confirm we get the set of expected features in the output layer.

    expected_feature_number = 3
    assert mem_layer.GetFeatureCount() == expected_feature_number

    expect = [0, 0, 1]
    expect_wkt = [
        "POLYGON ((1 3,1 2,2 2,2 3,1 3))",
        "POLYGON ((2 2,2 1,3 1,3 2,2 2))",
        "POLYGON ((0 4,0 0,4 0,4 4,0 4),(1 3,2 3,2 2,1 2,1 3),(2 2,3 2,3 1,2 1,2 2))",
    ]

    idx = 0
    for feature in mem_layer:
        id = feature.GetField("DN")
        assert id == expect[idx]

        geom_poly = feature.GetGeometryRef()
        wkt = geom_poly.ExportToWkt()
        assert wkt == expect_wkt[idx]

        idx += 1


###############################################################################
# Test a simple case with two inner wholes touchs at a vertex.


@pytest.mark.require_driver("AAIGRID")
def test_polygonize_6():

    src_ds = gdal.Open("data/polygonize_in_4.grd")
    src_band = src_ds.GetRasterBand(1)

    # Create a memory OGR datasource to put results in.
    mem_drv = ogr.GetDriverByName("Memory")
    mem_ds = mem_drv.CreateDataSource("out")

    mem_layer = mem_ds.CreateLayer("poly", None, ogr.wkbPolygon)

    fd = ogr.FieldDefn("DN", ogr.OFTInteger)
    mem_layer.CreateField(fd)

    # run the algorithm.
    result = gdal.Polygonize(src_band, None, mem_layer, 0)
    assert result == 0, "Polygonize failed"

    # Confirm we get the set of expected features in the output layer.

    expected_feature_number = 3
    assert mem_layer.GetFeatureCount() == expected_feature_number

    expect = [0, 0, 1]
    expect_wkt = [
        "POLYGON ((2 3,2 2,3 2,3 3,2 3))",
        "POLYGON ((1 2,1 1,2 1,2 2,1 2))",
        "POLYGON ((0 4,0 0,4 0,4 4,0 4),(2 3,3 3,3 2,2 2,2 3),(1 2,2 2,2 1,1 1,1 2))",
    ]

    idx = 0
    for feature in mem_layer:
        id = feature.GetField("DN")
        assert id == expect[idx]

        geom_poly = feature.GetGeometryRef()
        wkt = geom_poly.ExportToWkt()
        assert wkt == expect_wkt[idx]

        idx += 1


###############################################################################
# Test a complex case to make sure the polygonized area match original raster.


def test_polygonize_7():

    src_ds = gdal.Open("data/polygonize_check_area.tif")
    src_band = src_ds.GetRasterBand(1)

    # Create a memory OGR datasource to put results in.
    mem_drv = ogr.GetDriverByName("Memory")
    mem_ds = mem_drv.CreateDataSource("out")

    mem_layer = mem_ds.CreateLayer("poly", None, ogr.wkbPolygon)

    fd = ogr.FieldDefn("DN", ogr.OFTInteger)
    mem_layer.CreateField(fd)

    # run the algorithm.
    result = gdal.Polygonize(src_band, src_band.GetMaskBand(), mem_layer, 0)
    assert result == 0, "Polygonize failed"

    # collect raster image areas by DN value
    transform = src_ds.GetGeoTransform()
    pixel_area = abs(transform[1] * transform[5])

    data = struct.unpack(
        "h" * src_ds.RasterXSize * src_ds.RasterYSize,
        src_band.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize),
    )
    dn_area_raster = defaultdict(int)

    for v in data:
        if v != src_band.GetNoDataValue():
            dn_area_raster[v] += pixel_area

    # collect vector image areas by DN value
    dn_area_vector = defaultdict(float)

    for feature in mem_layer:
        id = feature.GetField("DN")
        geom = feature.GetGeometryRef()
        dn_area_vector[id] += geom.GetArea()

    assert len(dn_area_raster) == len(dn_area_vector), "DN value inconsistent"

    for key, value in dn_area_raster.items():
        assert (
            abs(value - dn_area_vector[key]) < pixel_area
        ), "polygonized vector area not match raster area"


###############################################################################
# Test a none nodata mask(a user defined mask) case to make sure the polygonized area match that mask.


@pytest.mark.require_driver("AAIGRID")
def test_polygonize_8():

    src_ds = gdal.Open("data/polygonize_in_5.grd")
    src_band = src_ds.GetRasterBand(1)

    mask_ds = gdal.Open("data/polygonize_in_5_mask.grd")
    mask_band = mask_ds.GetRasterBand(1)

    # Create a memory OGR datasource to put results in.
    mem_drv = ogr.GetDriverByName("Memory")
    mem_ds = mem_drv.CreateDataSource("out")

    ######################################
    # Test four connectedness conversion
    ######################################

    mem_layer = mem_ds.CreateLayer("poly_4connected", None, ogr.wkbPolygon)

    fd = ogr.FieldDefn("DN", ogr.OFTInteger)
    mem_layer.CreateField(fd)

    # run the algorithm.
    result = gdal.Polygonize(src_band, mask_band, mem_layer, 0)
    assert result == 0, "Polygonize failed"

    expected_feature_number = 4
    assert mem_layer.GetFeatureCount() == expected_feature_number

    expect = [1, 1, 1, 1]
    expect_wkt = [
        "POLYGON ((1 4,1 3,3 3,3 4,1 4))",
        "POLYGON ((0 3,0 1,1 1,1 3,0 3))",
        "POLYGON ((3 3,3 1,4 1,4 3,3 3))",
        "POLYGON ((1 1,1 0,3 0,3 1,1 1))",
    ]
    idx = 0
    for feature in mem_layer:
        id = feature.GetField("DN")
        assert id == expect[idx]

        geom_poly = feature.GetGeometryRef()
        wkt = geom_poly.ExportToWkt()
        assert wkt == expect_wkt[idx]

        idx += 1

    ######################################
    # Test eight connectedness conversion
    ######################################

    mem_layer = mem_ds.CreateLayer("poly_8connected", None, ogr.wkbPolygon)

    fd = ogr.FieldDefn("DN", ogr.OFTInteger)
    mem_layer.CreateField(fd)

    # run the algorithm.
    result = gdal.Polygonize(src_band, mask_band, mem_layer, 0, ["8CONNECTED=8"])
    assert result == 0, "Polygonize failed"

    expected_feature_number = 1
    assert mem_layer.GetFeatureCount() == expected_feature_number

    feature = mem_layer.GetNextFeature()
    assert feature.GetField("DN") == 1

    geom_poly = feature.GetGeometryRef()
    wkt = geom_poly.ExportToWkt()
    assert (
        wkt
        == "POLYGON ((1 4,1 3,0 3,0 1,1 1,1 0,3 0,3 1,4 1,4 3,3 3,3 4,1 4),(1 3,3 3,3 1,1 1,1 3))"
    )
