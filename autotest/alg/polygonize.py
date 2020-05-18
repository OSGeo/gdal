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



import ogrtest

from osgeo import gdal, ogr

###############################################################################
# Test a fairly simple case, with nodata masking.


def test_polygonize_1(is_int_polygonize=True):

    src_ds = gdal.Open('data/polygonize_in.grd')
    src_band = src_ds.GetRasterBand(1)

    # Create a memory OGR datasource to put results in.
    mem_drv = ogr.GetDriverByName('Memory')
    mem_ds = mem_drv.CreateDataSource('out')

    mem_layer = mem_ds.CreateLayer('poly', None, ogr.wkbPolygon)

    fd = ogr.FieldDefn('DN', ogr.OFTInteger)
    mem_layer.CreateField(fd)

    # run the algorithm.
    if is_int_polygonize:
        result = gdal.Polygonize(src_band, src_band.GetMaskBand(), mem_layer, 0)
    else:
        result = gdal.FPolygonize(src_band, src_band.GetMaskBand(), mem_layer, 0)
    assert result == 0, 'Polygonize failed'

    # Confirm we get the set of expected features in the output layer.

    expected_feature_number = 13
    assert mem_layer.GetFeatureCount() == expected_feature_number

    expect = [107, 123, 115, 115, 140, 148, 123, 140, 156,
              100, 101, 102, 103]

    tr = ogrtest.check_features_against_list(mem_layer, 'DN', expect)

    # check at least one geometry.
    if tr:
        mem_layer.SetAttributeFilter('dn = 156')
        feat_read = mem_layer.GetNextFeature()
        if ogrtest.check_feature_geometry(feat_read, 'POLYGON ((440720 3751200,440720 3751020,440900 3751020,440900 3751200,440720 3751200),(440780 3751140,440780 3751080,440840 3751080,440840 3751140,440780 3751140))') != 0:
            tr = 0
        feat_read.Destroy()

    assert tr


def test_polygonize_1_float():
    return test_polygonize_1(is_int_polygonize=False)

###############################################################################
# Test a simple case without masking.


def test_polygonize_2():

    src_ds = gdal.Open('data/polygonize_in.grd')
    src_band = src_ds.GetRasterBand(1)

    # Create a memory OGR datasource to put results in.
    mem_drv = ogr.GetDriverByName('Memory')
    mem_ds = mem_drv.CreateDataSource('out')

    mem_layer = mem_ds.CreateLayer('poly', None, ogr.wkbPolygon)

    fd = ogr.FieldDefn('DN', ogr.OFTInteger)
    mem_layer.CreateField(fd)

    # run the algorithm.
    result = gdal.Polygonize(src_band, None, mem_layer, 0)
    assert result == 0, 'Polygonize failed'

    # Confirm we get the set of expected features in the output layer.

    expected_feature_number = 17
    assert mem_layer.GetFeatureCount() == expected_feature_number

    expect = [107, 123, 115, 132, 115, 132, 140, 132, 148, 123, 140,
              132, 156, 100, 101, 102, 103]

    tr = ogrtest.check_features_against_list(mem_layer, 'DN', expect)

    assert tr

###############################################################################
# A more involved case with a complex looping.


def test_polygonize_3():

    src_ds = gdal.Open('data/polygonize_in_2.grd')
    src_band = src_ds.GetRasterBand(1)

    # Create a memory OGR datasource to put results in.
    mem_drv = ogr.GetDriverByName('Memory')
    mem_ds = mem_drv.CreateDataSource('out')

    mem_layer = mem_ds.CreateLayer('poly', None, ogr.wkbPolygon)

    fd = ogr.FieldDefn('DN', ogr.OFTInteger)
    mem_layer.CreateField(fd)

    # run the algorithm.
    result = gdal.Polygonize(src_band, None, mem_layer, 0)
    assert result == 0, 'Polygonize failed'

    # Confirm we get the expected count of features.

    expected_feature_number = 125
    assert mem_layer.GetFeatureCount() == expected_feature_number

    # check at least one geometry.
    mem_layer.SetAttributeFilter('dn = 0')
    feat_read = mem_layer.GetNextFeature()
    if ogrtest.check_feature_geometry(feat_read, 'POLYGON ((6 -3,6 -40,19 -40,19 -39,25 -39,25 -38,27 -38,27 -37,28 -37,28 -36,29 -36,29 -35,30 -35,30 -34,31 -34,31 -25,30 -25,30 -24,29 -24,29 -23,28 -23,28 -22,27 -22,27 -21,24 -21,24 -20,23 -20,23 -19,26 -19,26 -18,27 -18,27 -17,28 -17,28 -16,29 -16,29 -8,28 -8,28 -7,27 -7,27 -6,26 -6,26 -5,24 -5,24 -4,18 -4,18 -3,6 -3),(11 -7,11 -18,23 -18,23 -17,24 -17,24 -16,25 -16,25 -9,24 -9,24 -8,23 -8,23 -7,11 -7),(11 -22,11 -36,24 -36,24 -35,26 -35,26 -33,27 -33,27 -25,26 -25,26 -23,24 -23,24 -22,11 -22))') != 0:
        print(feat_read.GetGeometryRef().ExportToWkt())
        tr = 0
    else:
        tr = 1
    feat_read.Destroy()

    assert tr

###############################################################################
# Test a simple case without masking but with 8-connectedness.


def test_polygonize_4():

    src_ds = gdal.Open('data/polygonize_in.grd')
    src_band = src_ds.GetRasterBand(1)

    # Create a memory OGR datasource to put results in.
    mem_drv = ogr.GetDriverByName('Memory')
    mem_ds = mem_drv.CreateDataSource('out')

    mem_layer = mem_ds.CreateLayer('poly', None, ogr.wkbPolygon)

    fd = ogr.FieldDefn('DN', ogr.OFTInteger)
    mem_layer.CreateField(fd)

    # run the algorithm.
    result = gdal.Polygonize(src_band, None, mem_layer, 0, ["8CONNECTED=8"])
    assert result == 0, 'Polygonize failed'

    # Confirm we get the set of expected features in the output layer.

    expected_feature_number = 16
    assert mem_layer.GetFeatureCount() == expected_feature_number

    expect = [107, 123, 132, 115, 132, 115, 140, 148,
              123, 140, 132, 156, 100, 101, 102, 103]

    tr = ogrtest.check_features_against_list(mem_layer, 'DN', expect)

    assert tr



