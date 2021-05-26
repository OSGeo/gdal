#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GTM driver functionality.
# Author:   Leonardo de Paula Rosa Piga <leonardo dot piga at gmail dot com>
#
###############################################################################
# Copyright (c) 2009, Leonardo de P. R. Piga <leonardo dot piga at gmail dot com>
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

import os


import ogrtest
from osgeo import ogr
import pytest

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    yield
    os.remove('tmp/gtm.gtm')


###############################################################################
# Test waypoints gtm layer.


def test_ogr_gtm_read_1():

    gtm_ds = ogr.Open('data/gtm/samplemap.gtm')
    assert gtm_ds.GetLayerCount() == 2, 'wrong number of layers'

    lyr = gtm_ds.GetLayerByName('samplemap_waypoints')

    assert lyr.GetFeatureCount() == 3, 'wrong number of features'

    # Test 1st feature
    feat = lyr.GetNextFeature()

    assert feat.GetField('name') == 'WAY6', 'Wrong name field value'

    assert feat.GetField('comment') == 'Santa Cruz Stadium', 'Wrong comment field value'

    assert feat.GetField('icon') == 92, 'Wrong icon field value'

    assert feat.GetField('time') == '2009/12/18 17:32:41', 'Wrong time field value'

    wkt = 'POINT (-47.789974212646484 -21.201919555664062)'
    assert not ogrtest.check_feature_geometry(feat, wkt), 'Unexpected geometry'

    # Test 2nd feature
    feat = lyr.GetNextFeature()

    assert feat.GetField('name') == 'WAY6', 'Wrong name field value'

    assert feat.GetField('comment') == 'Joe\'s Goalkeeper Pub', \
        'Wrong comment field value'

    assert feat.GetField('icon') == 4, 'Wrong icon field value'

    assert feat.GetField('time') == '2009/12/18 17:34:46', 'Wrong time field value'

    wkt = 'POINT (-47.909481048583984 -21.294229507446289)'
    assert not ogrtest.check_feature_geometry(feat, wkt), 'Unexpected geometry'

    # Test 3rd feature
    feat = lyr.GetNextFeature()

    assert feat.GetField('name') == '33543400', 'Wrong name field value'

    assert feat.GetField('comment') == 'City Hall', 'Wrong comment field value'

    assert feat.GetField('icon') == 61, 'Wrong icon field value'

    assert feat.GetField('time') is None, 'Wrong time field value'

    wkt = 'POINT (-47.806097491943362 -21.176849600708007)'
    assert not ogrtest.check_feature_geometry(feat, wkt), 'Unexpected geometry'

###############################################################################
# Test tracks gtm layer.


def test_ogr_gtm_read_2():

    gtm_ds = ogr.Open('data/gtm/samplemap.gtm')
    lyr = gtm_ds.GetLayerByName('samplemap_tracks')

    assert lyr.GetFeatureCount() == 3, 'wrong number of features'

    # Test 1st feature
    feat = lyr.GetNextFeature()

    assert feat.GetField('name') == 'San Sebastian Street', 'Wrong name field value'

    assert feat.GetField('type') == 2, 'Wrong type field value'

    assert feat.GetField('color') == 0, 'Wrong color field value'

    # if feat.GetField('time') is not None:
    #    gdaltest.post_reason( 'Wrong time field value' )
    #    return 'fail'

    wkt = 'LINESTRING (-47.807481607448054 -21.177795963939211,' + \
          '-47.808151245117188 -21.177299499511719,' + \
          '-47.809136624130645 -21.176562836150087,' + \
          '-47.809931418108405 -21.175971104366582)'
    assert not ogrtest.check_feature_geometry(feat, wkt), 'Unexpected geometry'

    # Test 2nd feature
    feat = lyr.GetNextFeature()

    assert feat.GetField('name') == 'Barao do Amazonas Street', 'Wrong name field value'

    assert feat.GetField('type') == 1, 'Wrong type field value'

    assert feat.GetField('color') == 0, 'Wrong color field value'

    # if feat.GetField('time') is not None:
    #    gdaltest.post_reason( 'Wrong time field value' )
    #    return 'fail'

    wkt = 'LINESTRING (-47.808751751608561 -21.178029550275486,' + \
        '-47.808151245117188 -21.177299499511719,' + \
        '-47.807561550927701 -21.176617693474089,' + \
        '-47.806959118447779 -21.175900153727685)'
    assert not ogrtest.check_feature_geometry(feat, wkt)

    # Test 3rd feature
    feat = lyr.GetNextFeature()

    assert feat.GetField('name') == 'Curupira Park', 'Wrong name field value'

    assert feat.GetField('type') == 17, 'Wrong type field value'

    assert feat.GetField('color') == 46848, 'Wrong color field value'

    # if feat.GetField('time') is not None:
    #    gdaltest.post_reason( 'Wrong time field value' )
    #    return 'fail'

    wkt = 'LINESTRING (-47.7894287109375 -21.194473266601562,' + \
        '-47.793514591064451 -21.197530536743162,' + \
        '-47.797027587890625 -21.19483757019043,' + \
        '-47.794818878173828 -21.192028045654297,' + \
        '-47.794120788574219 -21.193340301513672,' + \
        '-47.792263031005859 -21.194267272949219,' + \
        '-47.7894287109375 -21.194473266601562)'

    assert not ogrtest.check_feature_geometry(feat, wkt), 'Unexpected geometry'


###############################################################################
# Write test

###############################################################################
# Waypoint write
def test_ogr_gtm_write_1():

    ds = ogr.GetDriverByName('GPSTrackMaker').CreateDataSource('tmp/gtm.gtm')
    lyr = ds.CreateLayer('gtm_waypoints', geom_type=ogr.wkbPoint)

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetField('name', 'WAY0000000')
    dst_feat.SetField('comment', 'Waypoint 0')
    dst_feat.SetField('icon', 10)
    dst_feat.SetField('time', '2009/12/23 14:25:46')
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (-21 -47)'))
    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetField('name', 'WAY0000001')
    dst_feat.SetField('comment', 'Waypoint 1')
    dst_feat.SetField('icon', 31)
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (-21.123 -47.231 800)'))
    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    lyr = ds.CreateLayer('gtm_tracks', geom_type=ogr.wkbLineString)

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetField('name', '1st Street')
    dst_feat.SetField('type', 2)
    dst_feat.SetField('color', 0x0000FF)
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING (-21.1 -47.1, -21.2 -47.2, -21.3 -47.3, -21.4 -47.4)'))
    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetField('name', '2nd Street')
    dst_feat.SetField('type', 1)
    dst_feat.SetField('color', 0x000000)
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING (-21.11 -47.1, -21.21 -47.2, -21.31 -47.3, -21.41 -47.4)'))
    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat = ogr.Feature(lyr.GetLayerDefn())
    dst_feat.SetField('name', '3rd Street')
    dst_feat.SetField('type', 2)
    dst_feat.SetField('color', 0x000000)
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING ((-21.12 -47.1, -21.22 -47.2, -21.32 -47.3, -21.42 -47.4),' +
                                                   '(-21.12 -47.1, -21.02 -47.0, -20.92 -46.9))'))
    assert lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    ds = None

###############################################################################
# Check previous test


def test_ogr_gtm_check_write_1():

    ds = ogr.Open('tmp/gtm.gtm')
    lyr = ds.GetLayerByName('gtm_waypoints')
    assert lyr.GetFeatureCount() == 2, 'Bad feature count.'

    # Test 1st waypoint
    feat = lyr.GetNextFeature()

    assert feat.GetField('name') == 'WAY0000000', 'Wrong name field value'

    assert feat.GetField('comment') == 'Waypoint 0', 'Wrong comment field value'

    assert feat.GetField('icon') == 10, 'Wrong icon field value'

    assert feat.GetField('time') == '2009/12/23 14:25:46', 'Wrong time field value'

    wkt = 'POINT (-21 -47)'
    assert not ogrtest.check_feature_geometry(feat, wkt), 'Unexpected geometry'

    # Test 2nd waypoint
    feat = lyr.GetNextFeature()

    assert feat.GetField('name') == 'WAY0000001', 'Wrong name field value'

    assert feat.GetField('comment') == 'Waypoint 1', 'Wrong comment field value'

    assert feat.GetField('icon') == 31, 'Wrong icon field value'

    assert feat.GetField('time') is None, 'Wrong time field value'

    wkt = 'POINT (-21.123 -47.231 800)'
    assert not ogrtest.check_feature_geometry(feat, wkt), 'Unexpected geometry'

    # Test tracks
    lyr = ds.GetLayerByName('gtm_tracks')
    assert lyr.GetFeatureCount() == 4, 'Bad feature count.'

    # Test 1st track
    feat = lyr.GetNextFeature()

    assert feat.GetField('name') == '1st Street', 'Wrong name field value'

    assert feat.GetField('type') == 2, 'Wrong type field value'

    assert feat.GetField('color') == 0x0000FF, 'Wrong color field value'

    wkt = 'LINESTRING (-21.1 -47.1, -21.2 -47.2, -21.3 -47.3, -21.4 -47.4)'
    assert not ogrtest.check_feature_geometry(feat, wkt), 'Unexpected geometry'

    # Test 2nd track
    feat = lyr.GetNextFeature()

    assert feat.GetField('name') == '2nd Street', 'Wrong name field value'

    assert feat.GetField('type') == 1, 'Wrong type field value'

    assert feat.GetField('color') == 0x000000, 'Wrong color field value'

    wkt = 'LINESTRING (-21.11 -47.1, -21.21 -47.2, -21.31 -47.3, -21.41 -47.4)'
    assert not ogrtest.check_feature_geometry(feat, wkt), 'Unexpected geometry'

    # Test 3rd track
    feat = lyr.GetNextFeature()

    assert feat.GetField('name') == '3rd Street', 'Wrong name field value'

    assert feat.GetField('type') == 2, 'Wrong type field value'

    assert feat.GetField('color') == 0x000000, 'Wrong color field value'

    wkt = 'LINESTRING (-21.12 -47.1, -21.22 -47.2, -21.32 -47.3, -21.42 -47.4)'
    assert not ogrtest.check_feature_geometry(feat, wkt), 'Unexpected geometry'

    # Test 4th track
    feat = lyr.GetNextFeature()

    assert feat.GetField('name') == '3rd Street', 'Wrong name field value'

    assert feat.GetField('type') == 2, 'Wrong type field value'

    assert feat.GetField('color') == 0x000000, 'Wrong color field value'

    wkt = 'LINESTRING (-21.12 -47.1, -21.02 -47.0, -20.92 -46.9)'
    assert not ogrtest.check_feature_geometry(feat, wkt), 'Unexpected geometry'
