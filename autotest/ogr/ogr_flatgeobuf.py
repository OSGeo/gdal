#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  FlatGeobuf driver test suite.
# Author:   Björn Harrtell <bjorn@wololo.org>
#
###############################################################################
# Copyright (c) 2018-2019, Björn Harrtell <bjorn@wololo.org>
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

from osgeo import ogr

import gdaltest
import ogrtest
import pytest

### utils

def verify_flatgeobuf_copy(name, fids, names):

    if gdaltest.features is None:
        print('Missing features collection')
        return False

    fname = os.path.join('tmp', name + '.fgb')
    ds = ogr.Open(fname)
    if ds is None:
        print('Can not open \'' + fname + '\'')
        return False

    lyr = ds.GetLayer(0)
    if lyr is None:
        print('Missing layer')
        return False

    ######################################################
    # Test attributes
    ret = ogrtest.check_features_against_list(lyr, 'FID', fids)
    if ret != 1:
        print('Wrong values in \'FID\' field')
        return False

    lyr.ResetReading()
    ret = ogrtest.check_features_against_list(lyr, 'NAME', names)
    if ret != 1:
        print('Wrong values in \'NAME\' field')
        return False

    ######################################################
    # Test geometries
    lyr.ResetReading()
    for i in range(len(gdaltest.features)):

        orig_feat = gdaltest.features[i]
        feat = lyr.GetNextFeature()

        if feat is None:
            print('Failed trying to read feature')
            return False

        if ogrtest.check_feature_geometry(feat, orig_feat.GetGeometryRef(),
                                          max_error=0.001) != 0:
            print('Geometry test failed')
            gdaltest.features = None
            return False

    gdaltest.features = None

    lyr = None

    return True


def copy_shape_to_flatgeobuf(name, wkbType, compress=None):
    if gdaltest.flatgeobuf_drv is None:
        return False

    if compress is not None:
        if compress[0:5] == '/vsig':
            dst_name = os.path.join('/vsigzip/', 'tmp', name + '.fgb' + '.gz')
        elif compress[0:4] == '/vsiz':
            dst_name = os.path.join('/vsizip/', 'tmp', name + '.fgb' + '.zip')
        elif compress == '/vsistdout/':
            dst_name = compress
        else:
            return False
    else:
        dst_name = os.path.join('tmp', name + '.fgb')

    ds = gdaltest.flatgeobuf_drv.CreateDataSource(dst_name)
    if ds is None:
        return False

    ######################################################
    # Create layer
    lyr = ds.CreateLayer(name, None, wkbType)
    if lyr is None:
        return False

    ######################################################
    # Setup schema (all test shapefiles use common schmea)
    ogrtest.quick_create_layer_def(lyr,
                                   [('FID', ogr.OFTReal),
                                    ('NAME', ogr.OFTString)])

    ######################################################
    # Copy in shp

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())

    src_name = os.path.join('data', name + '.shp')
    shp_ds = ogr.Open(src_name)
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.features = []

    while feat is not None:
        gdaltest.features.append(feat)

        dst_feat.SetFrom(feat)
        lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    shp_lyr = None
    lyr = None

    ds = None

    return True

### tests

def test_ogr_flatgeobuf_1():

    gdaltest.flatgeobuf_drv = ogr.GetDriverByName('FlatGeobuf')

    if gdaltest.flatgeobuf_drv is not None:
        return
    pytest.fail()

def test_ogr_flatgeobuf_2():
    fgb_ds = ogr.Open('data/testfgb/poly.fgb')
    fgb_lyr = fgb_ds.GetLayer(0)

    # test expected spatial filter feature count consistency
    c = fgb_lyr.GetFeatureCount()
    assert c == 10
    c = fgb_lyr.SetSpatialFilterRect(478315.531250, 4762880.500000, 481645.312500, 4765610.500000)
    c = fgb_lyr.GetFeatureCount()
    assert c == 10
    c = fgb_lyr.SetSpatialFilterRect(878315.531250, 4762880.500000, 881645.312500, 4765610.500000)
    c = fgb_lyr.GetFeatureCount()
    assert c == 0
    c = fgb_lyr.SetSpatialFilterRect(479586.0,4764618.6,479808.2,4764797.8)
    c = fgb_lyr.GetFeatureCount()
    if ogrtest.have_geos():
        assert c == 4
    else:
        assert c == 5



    # check that ResetReading does not affect subsequent enumeration or filtering
    num = len(list([x for x in fgb_lyr]))
    if ogrtest.have_geos():
        assert num == 4
    else:
        assert num == 5
    fgb_lyr.ResetReading()
    c = fgb_lyr.GetFeatureCount()
    if ogrtest.have_geos():
        assert c == 4
    else:
        assert c == 5
    fgb_lyr.ResetReading()
    num = len(list([x for x in fgb_lyr]))
    if ogrtest.have_geos():
        assert num == 4
    else:
        assert num == 5

# Run test_ogrsf
def test_ogr_flatgeobuf_8():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/testfgb/poly.fgb')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

def test_ogr_flatgeobuf_9():
    if gdaltest.flatgeobuf_drv is None:
        pytest.skip()

    gdaltest.tests = [
        ['gjpoint', [1], ['Point 1'], ogr.wkbPoint],
        ['gjline', [1], ['Line 1'], ogr.wkbLineString],
        ['gjpoly', [1], ['Polygon 1'], ogr.wkbPolygon],
        ['gjmultipoint', [1], ['MultiPoint 1'], ogr.wkbMultiPoint],
        ['gjmultiline', [2], ['MultiLine 1'], ogr.wkbMultiLineString],
        ['gjmultipoly', [2], ['MultiPoly 1'], ogr.wkbMultiPolygon]
    ]

    for i in range(len(gdaltest.tests)):
        test = gdaltest.tests[i]

        rc = copy_shape_to_flatgeobuf(test[0], test[3])
        assert rc, ('Failed making copy of ' + test[0] + '.shp')

        rc = verify_flatgeobuf_copy(test[0], test[1], test[2])
        assert rc, ('Verification of copy of ' + test[0] + '.shp failed')

