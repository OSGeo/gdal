#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR Ingres driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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



import gdaltest
import ogrtest
from osgeo import ogr
import pytest

###############################################################################
# Open INGRES test datasource.


def test_ogr_ingres_1():

    gdaltest.ingres_ds = None

    drv = ogr.GetDriverByName('Ingres')
    if drv is None:
        pytest.skip()

    gdaltest.ingres_ds = ogr.Open('@driver=ingres,dbname=test', update=1)
    if gdaltest.ingres_ds is None:
        pytest.skip()

    
###############################################################################
# Create table from data/poly.shp


def test_ogr_ingres_2():

    if gdaltest.ingres_ds is None:
        pytest.skip()

    #######################################################
    # Create Layer
    gdaltest.ingres_lyr = gdaltest.ingres_ds.CreateLayer(
        'tpoly', geom_type=ogr.wkbPolygon,
        options=['OVERWRITE=YES'])

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(gdaltest.ingres_lyr,
                                   [('AREA', ogr.OFTReal),
                                    ('EAS_ID', ogr.OFTInteger),
                                    ('PRFEDEA', ogr.OFTString)])

    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=gdaltest.ingres_lyr.GetLayerDefn())

    shp_ds = ogr.Open('data/poly.shp')
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    while feat is not None:

        gdaltest.poly_feat.append(feat)

        dst_feat.SetFrom(feat)
        gdaltest.ingres_lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    
###############################################################################
# Verify that stuff we just wrote is still OK.


def test_ogr_ingres_3():
    if gdaltest.ingres_ds is None:
        pytest.skip()

    expect = [168, 169, 166, 158, 165]

    gdaltest.ingres_lyr.SetAttributeFilter('eas_id < 170')
    tr = ogrtest.check_features_against_list(gdaltest.ingres_lyr,
                                             'eas_id', expect)
    gdaltest.ingres_lyr.SetAttributeFilter(None)

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.ingres_lyr.GetNextFeature()

        assert (ogrtest.check_feature_geometry(read_feat, orig_feat.GetGeometryRef(),
                                          max_error=0.000000001) == 0)

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), \
                ('Attribute %d does not match' % fld)

    gdaltest.poly_feat = None
    gdaltest.shp_ds.Destroy()

    # This is to force cleanup of the transaction.  We need a way of
    # automating this in the driver.
    read_feat = gdaltest.ingres_lyr.GetNextFeature()

    assert tr

###############################################################################
# Test ExecuteSQL() results layers without geometry.


def test_ogr_ingres_4():

    if gdaltest.ingres_ds is None:
        pytest.skip()

    expect = [179, 173, 172, 171, 170, 169, 168, 166, 165, 158]

    sql_lyr = gdaltest.ingres_ds.ExecuteSQL('select distinct eas_id from tpoly order by eas_id desc')

    tr = ogrtest.check_features_against_list(sql_lyr, 'eas_id', expect)

    gdaltest.ingres_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Test ExecuteSQL() results layers with geometry.
#
# Unfortunately, for now an executesql result that includes new geometries
# fails to ever get any result records as executed by ogringresstatement.cpp,
# so we disable this test.


def test_ogr_ingres_5():

    if gdaltest.ingres_ds is None:
        pytest.skip()

    pytest.skip()

    # pylint: disable=unreachable
    sql_lyr = gdaltest.ingres_ds.ExecuteSQL(
        "select * from tpoly where prfedea = '35043413'")

    tr = ogrtest.check_features_against_list(sql_lyr, 'prfedea',
                                             ['35043413'])
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry(feat_read, 'POLYGON ((479750.6875 4764702.0,479658.59375 4764670.0,479640.09375 4764721.0,479735.90625 4764752.0,479750.6875 4764702.0))') != 0:
            tr = 0
        feat_read.Destroy()

    gdaltest.ingres_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Test spatial filtering.


def test_ogr_ingres_6():

    if gdaltest.ingres_ds is None:
        pytest.skip()

    gdaltest.ingres_lyr.SetAttributeFilter(None)

    geom = ogr.CreateGeometryFromWkt(
        'LINESTRING(479505 4763195,480526 4762819)')
    gdaltest.ingres_lyr.SetSpatialFilter(geom)
    geom.Destroy()

    tr = ogrtest.check_features_against_list(gdaltest.ingres_lyr, 'eas_id',
                                             [158])

    gdaltest.ingres_lyr.SetSpatialFilter(None)

    assert tr

###############################################################################
# Test adding a new field.


def test_ogr_ingres_7():

    if gdaltest.ingres_ds is None:
        pytest.skip()

    ####################################################################
    # Add new string field.
    field_defn = ogr.FieldDefn('new_string', ogr.OFTString)
    result = gdaltest.ingres_lyr.CreateField(field_defn)
    field_defn.Destroy()

    assert result is 0, 'CreateField failed!'

    ####################################################################
    # Apply a value to this field in one feature.

    gdaltest.ingres_lyr.SetAttributeFilter("prfedea = '35043423'")
    feat_read = gdaltest.ingres_lyr.GetNextFeature()
    assert feat_read is not None, 'failed to read target feature!'

    gdaltest.ingres_fid = feat_read.GetFID()

    feat_read.SetField('new_string', 'test1')
    gdaltest.ingres_lyr.SetFeature(feat_read)
    feat_read.Destroy()

    ####################################################################
    # Now fetch two features and verify the new column works OK.

    gdaltest.ingres_lyr.SetAttributeFilter(
        "PRFEDEA IN ( '35043423', '35043414' )")

    tr = ogrtest.check_features_against_list(gdaltest.ingres_lyr, 'new_string',
                                             [None, 'test1'])

    gdaltest.ingres_lyr.SetAttributeFilter(None)

    assert tr

###############################################################################
# Test deleting a feature.


def test_ogr_ingres_8():

    if gdaltest.ingres_ds is None:
        pytest.skip()

    assert gdaltest.ingres_lyr.TestCapability('DeleteFeature'), \
        'DeleteFeature capability test failed.'

    old_count = gdaltest.ingres_lyr.GetFeatureCount()

    ####################################################################
    # Delete target feature.

    target_fid = gdaltest.ingres_fid
    assert gdaltest.ingres_lyr.DeleteFeature(target_fid) == 0, \
        'DeleteFeature returned error code.'

    ####################################################################
    # Verify that count has dropped by one, and that the feature in question
    # can't be fetched.
    new_count = gdaltest.ingres_lyr.GetFeatureCount()
    if new_count != old_count - 1:
        gdaltest.post_reason('got feature count of %d, not expected %d.'
                             % (new_count, old_count - 1))

    assert gdaltest.ingres_lyr.GetFeature(target_fid) is None, 'Got deleted feature!'

###############################################################################
#


def test_ogr_ingres_cleanup():

    if gdaltest.ingres_ds is None:
        pytest.skip()

    gdaltest.ingres_ds.Destroy()
    gdaltest.ingres_ds = None



