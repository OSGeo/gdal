#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test BNA driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
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


import gdaltest
import ogrtest
from osgeo import ogr

###############################################################################
# Test points bna layer.


def test_ogr_bna_1():

    gdaltest.bna_ds = ogr.Open('data/bna/test.bna')

    lyr = gdaltest.bna_ds.GetLayerByName('test_points')

    expect = ['PID5', 'PID4']

    tr = ogrtest.check_features_against_list(lyr, 'Primary ID', expect)
    assert tr

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT (573.736 476.563)',
                                      max_error=0.0001) == 0)

    feat = lyr.GetNextFeature()
    assert (ogrtest.check_feature_geometry(feat, 'POINT (532.991 429.121)',
                                      max_error=0.0001) == 0)

###############################################################################
# Test lines bna layer.


def test_ogr_bna_2():

    gdaltest.bna_ds = ogr.Open('data/bna/test.bna')

    lyr = gdaltest.bna_ds.GetLayerByName('test_lines')

    expect = ['PID3']

    tr = ogrtest.check_features_against_list(lyr, 'Primary ID', expect)
    assert tr

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat, 'LINESTRING (224.598 307.425,333.043 341.461,396.629 304.952)', max_error=0.0001) == 0

###############################################################################
# Test polygons bna layer.


def test_ogr_bna_3():

    gdaltest.bna_ds = ogr.Open('data/bna/test.bna')

    lyr = gdaltest.bna_ds.GetLayerByName('test_polygons')

    expect = ['PID2', 'PID1', 'PID7', 'PID8', 'PID9', 'PID10']

    tr = ogrtest.check_features_against_list(lyr, 'Primary ID', expect)
    assert tr
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat, 'MULTIPOLYGON (((0 0,1 0,1 1,0 1,0 0)))', max_error=0.0001) == 0
    feat = lyr.GetFeature(2)
    assert ogrtest.check_feature_geometry(feat, 'MULTIPOLYGON (((0 0,1 0,1 1,0 1,0 0)))', max_error=0.0001) == 0
    feat = lyr.GetFeature(3)
    assert ogrtest.check_feature_geometry(feat, 'POLYGON ((0 0,0 10,10 10,10 0,0 0),(2 2,2 8,8 8,8 2,2 2))', max_error=0.0001) == 0
    feat = lyr.GetFeature(4)
    assert ogrtest.check_feature_geometry(feat, 'MULTIPOLYGON (((0 0,0 10,10 10,10 0,0 0)))', max_error=0.0001) == 0
    feat = lyr.GetFeature(5)
    assert ogrtest.check_feature_geometry(feat, 'MULTIPOLYGON (((0 0,0 10,10 10,10 0,0 0)))', max_error=0.0001) == 0

###############################################################################
# Test ellipses bna layer.


def test_ogr_bna_4():

    gdaltest.bna_ds = ogr.Open('data/bna/test.bna')

    lyr = gdaltest.bna_ds.GetLayerByName('test_ellipses')

    expect = ['PID6']

    tr = ogrtest.check_features_against_list(lyr, 'Primary ID', expect)
    assert tr

    lyr.ResetReading()
    lyr.GetNextFeature()


###############################################################################
# Test write support

def ogr_bna_check_content(lyr1, lyr2):

    assert lyr1.GetFeatureCount() == lyr2.GetFeatureCount()

    feat1 = lyr1.GetNextFeature()
    feat2 = lyr2.GetNextFeature()
    while feat1 is not None:
        for i in range(lyr1.GetLayerDefn().GetFieldCount()):
            assert feat1.GetField(i) == feat2.GetField(i)

        assert (ogrtest.check_feature_geometry(feat1, feat2.GetGeometryRef(),
                                          max_error=0.000000001) == 0)

        feat1 = lyr1.GetNextFeature()
        feat2 = lyr2.GetNextFeature()

    

def ogr_bna_write(creation_options):

    output_ds = ogr.GetDriverByName('BNA').CreateDataSource('tmp/out.bna', options=creation_options)

    # Duplicate data/test.bna into tmp/out.bna
    for layer_name in ['points', 'lines', 'polygons', 'ellipses']:
        src_lyr = gdaltest.bna_ds.GetLayerByName('test_' + layer_name)
        dst_lyr = output_ds.CreateLayer(layer_name, geom_type=src_lyr.GetLayerDefn().GetGeomType())

        for i in range(src_lyr.GetLayerDefn().GetFieldCount()):
            field_defn = src_lyr.GetLayerDefn().GetFieldDefn(i)
            dst_lyr.CreateField(field_defn)

        dst_feat = ogr.Feature(feature_def=dst_lyr.GetLayerDefn())

        src_lyr.ResetReading()
        feat = src_lyr.GetNextFeature()

        while feat is not None:
            dst_feat.SetFrom(feat)
            assert dst_lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

            feat = src_lyr.GetNextFeature()

    output_ds = None

    # Check features
    output_ds = ogr.Open('tmp/out.bna')
    for layer_name in ['points', 'lines', 'polygons', 'ellipses']:
        src_lyr = gdaltest.bna_ds.GetLayerByName('test_' + layer_name)
        dst_lyr = output_ds.GetLayerByName('out_' + layer_name)
        ogr_bna_check_content(src_lyr, dst_lyr)

    

def test_ogr_bna_5():

    return ogr_bna_write(['ELLIPSES_AS_ELLIPSES=YES'])


def test_ogr_bna_6():

    try:
        os.remove('tmp/out.bna')
    except OSError:
        pass

    ogr_bna_write(['LINEFORMAT=LF', 'MULTILINE=NO', 'COORDINATE_PRECISION=3'])

    size = os.stat('tmp/out.bna').st_size
    assert size == 1601, ('Got size %d. Expected %d' % (size, 1601))

    os.remove('tmp/out.bna')

    ogr_bna_write(['LINEFORMAT=CRLF', 'MULTILINE=NO', 'COORDINATE_PRECISION=3'])

    size = os.stat('tmp/out.bna').st_size
    assert size == 1611, ('Got size %d. Expected %d' % (size, 1611))

###############################################################################
#


def test_ogr_bna_cleanup():

    gdaltest.bna_ds = None

    try:
        os.remove('tmp/out.bna')
    except OSError:
        pass

    


