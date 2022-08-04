#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR Memory driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdal
from osgeo import osr
import pytest


###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    gdaltest.mem_ds = ogr.GetDriverByName('Memory').CreateDataSource('wrk_in_memory')

    assert gdaltest.mem_ds is not None

    yield

    gdaltest.mem_ds = None

###############################################################################
# Create table from data/poly.shp


def test_ogr_mem_2():

    assert gdaltest.mem_ds.TestCapability(ogr.ODsCCreateLayer) != 0, \
        'ODsCCreateLayer TestCapability failed.'

    #######################################################
    # Create memory Layer
    gdaltest.mem_lyr = gdaltest.mem_ds.CreateLayer('tpoly')

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(gdaltest.mem_lyr,
                                   [('AREA', ogr.OFTReal),
                                    ('EAS_ID', ogr.OFTInteger),
                                    ('PRFEDEA', ogr.OFTString),
                                    ('WHEN', ogr.OFTDateTime)])

    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=gdaltest.mem_lyr.GetLayerDefn())

    shp_ds = ogr.Open('data/poly.shp')
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    while feat is not None:

        gdaltest.poly_feat.append(feat)

        dst_feat.SetFrom(feat)
        ret = gdaltest.mem_lyr.CreateFeature(dst_feat)
        assert ret == 0, 'CreateFeature() failed.'

        feat = shp_lyr.GetNextFeature()


###############################################################################
# Verify that stuff we just wrote is still OK.


def test_ogr_mem_3():

    expect = [168, 169, 166, 158, 165]

    gdaltest.mem_lyr.SetAttributeFilter('eas_id < 170')
    tr = ogrtest.check_features_against_list(gdaltest.mem_lyr,
                                             'eas_id', expect)
    gdaltest.mem_lyr.SetAttributeFilter(None)

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.mem_lyr.GetNextFeature()

        assert (ogrtest.check_feature_geometry(read_feat, orig_feat.GetGeometryRef(),
                                          max_error=0.000000001) == 0)

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), \
                ('Attribute %d does not match' % fld)

    gdaltest.poly_feat = None
    gdaltest.shp_ds = None

    assert tr

###############################################################################
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.


def test_ogr_mem_4():

    dst_feat = ogr.Feature(feature_def=gdaltest.mem_lyr.GetLayerDefn())
    wkt_list = ['10', '2', '1', '3d_1', '4', '5', '6']

    for item in wkt_list:

        wkt = open('data/wkb_wkt/' + item + '.wkt').read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new memory feature.

        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField('PRFEDEA', item)
        gdaltest.mem_lyr.CreateFeature(dst_feat)

        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.mem_lyr.SetAttributeFilter("PRFEDEA = '%s'" % item)
        feat_read = gdaltest.mem_lyr.GetNextFeature()

        assert ogrtest.check_feature_geometry(feat_read, geom) == 0


###############################################################################
# Test ExecuteSQL() results layers without geometry.


def test_ogr_mem_5():

    expect = [179, 173, 172, 171, 170, 169, 168, 166, 165, 158, None]

    sql_lyr = gdaltest.mem_ds.ExecuteSQL('select distinct eas_id from tpoly order by eas_id desc')

    tr = ogrtest.check_features_against_list(sql_lyr, 'eas_id', expect)

    gdaltest.mem_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Test ExecuteSQL() results layers with geometry.


def test_ogr_mem_6():

    sql_lyr = gdaltest.mem_ds.ExecuteSQL(
        "select * from tpoly where prfedea = '2'")

    tr = ogrtest.check_features_against_list(sql_lyr, 'prfedea', ['2'])
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry(feat_read, 'MULTILINESTRING ((5.00121349 2.99853132,5.00121349 1.99853133),(5.00121349 1.99853133,5.00121349 0.99853133),(3.00121351 1.99853127,5.00121349 1.99853133),(5.00121349 1.99853133,6.00121348 1.99853135))') != 0:
            tr = 0

    gdaltest.mem_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Test spatial filtering.


def test_ogr_mem_7():

    gdaltest.mem_lyr.SetAttributeFilter(None)

    geom = ogr.CreateGeometryFromWkt(
        'LINESTRING(479505 4763195,480526 4762819)')
    gdaltest.mem_lyr.SetSpatialFilter(geom)
    geom.Destroy()

    assert not gdaltest.mem_lyr.TestCapability(ogr.OLCFastSpatialFilter), \
        'OLCFastSpatialFilter capability test should have failed.'

    tr = ogrtest.check_features_against_list(gdaltest.mem_lyr, 'eas_id',
                                             [158])

    gdaltest.mem_lyr.SetSpatialFilter(None)

    assert tr

###############################################################################
# Test adding a new field.


def test_ogr_mem_8():

    ####################################################################
    # Add new string field.
    field_defn = ogr.FieldDefn('new_string', ogr.OFTString)
    gdaltest.mem_lyr.CreateField(field_defn)

    ####################################################################
    # Apply a value to this field in one feature.

    gdaltest.mem_lyr.SetAttributeFilter("PRFEDEA = '2'")
    feat_read = gdaltest.mem_lyr.GetNextFeature()
    feat_read.SetField('new_string', 'test1')
    gdaltest.mem_lyr.SetFeature(feat_read)

    # Test expected failed case of SetFeature()
    new_feat = ogr.Feature(gdaltest.mem_lyr.GetLayerDefn())
    new_feat.SetFID(-2)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = gdaltest.mem_lyr.SetFeature(new_feat)
    gdal.PopErrorHandler()
    assert ret != 0
    new_feat = None

    ####################################################################
    # Now fetch two features and verify the new column works OK.

    gdaltest.mem_lyr.SetAttributeFilter("PRFEDEA IN ( '2', '1' )")

    tr = ogrtest.check_features_against_list(gdaltest.mem_lyr, 'new_string',
                                             ['test1', None])

    gdaltest.mem_lyr.SetAttributeFilter(None)

    assert tr

###############################################################################
# Test deleting a feature.


def test_ogr_mem_9():

    assert gdaltest.mem_lyr.TestCapability(ogr.OLCDeleteFeature), \
        'OLCDeleteFeature capability test failed.'

    assert gdaltest.mem_lyr.TestCapability(ogr.OLCFastFeatureCount), \
        'OLCFastFeatureCount capability test failed.'

    old_count = gdaltest.mem_lyr.GetFeatureCount()

    ####################################################################
    # Delete target feature.

    target_fid = 2
    assert gdaltest.mem_lyr.DeleteFeature(target_fid) == 0, \
        'DeleteFeature returned error code.'

    assert gdaltest.mem_lyr.DeleteFeature(target_fid) != 0, \
        'DeleteFeature should have returned error code.'

    ####################################################################
    # Verify that count has dropped by one, and that the feature in question
    # can't be fetched.
    new_count = gdaltest.mem_lyr.GetFeatureCount()
    if new_count != old_count - 1:
        gdaltest.post_reason('got feature count of %d, not expected %d.'
                             % (new_count, old_count - 1))

    assert gdaltest.mem_lyr.TestCapability(ogr.OLCRandomRead), \
        'OLCRandomRead capability test failed.'

    assert gdaltest.mem_lyr.GetFeature(target_fid) is None, 'Got deleted feature!'

    assert gdaltest.mem_lyr.GetFeature(-1) is None, 'GetFeature() should have failed'

    assert gdaltest.mem_lyr.GetFeature(1000) is None, 'GetFeature() should have failed'

###############################################################################
# Test GetDriver() / name bug (#1674)
#
# Mostly we are verifying that this doesn't still cause a crash.


def test_ogr_mem_10():

    d = ogr.GetDriverByName('Memory')
    ds = d.CreateDataSource('xxxxxx')

    d2 = ds.GetDriver()
    assert d2 is not None and d2.GetName() == 'Memory', \
        'Did not get expected driver name.'

###############################################################################
# Verify that we can delete layers properly


def test_ogr_mem_11():

    assert gdaltest.mem_ds.TestCapability('DeleteLayer') != 0, \
        'Deletelayer TestCapability failed.'

    gdaltest.mem_ds.CreateLayer('extra')
    gdaltest.mem_ds.CreateLayer('extra2')
    layer_count = gdaltest.mem_ds.GetLayerCount()

    gdaltest.mem_lyr = None
    # Delete extra layer
    assert gdaltest.mem_ds.DeleteLayer(layer_count - 2) == 0, 'DeleteLayer() failed'

    assert gdaltest.mem_ds.DeleteLayer(-1) != 0, 'DeleteLayer() should have failed'

    assert gdaltest.mem_ds.DeleteLayer(gdaltest.mem_ds.GetLayerCount()) != 0, \
        'DeleteLayer() should have failed'

    assert gdaltest.mem_ds.GetLayer(-1) is None, 'GetLayer() should have failed'

    assert gdaltest.mem_ds.GetLayer(gdaltest.mem_ds.GetLayerCount()) is None, \
        'GetLayer() should have failed'

    lyr = gdaltest.mem_ds.GetLayer(gdaltest.mem_ds.GetLayerCount() - 1)

    assert lyr.GetName() == 'extra2', 'delete layer seems iffy'

###############################################################################
# Test some date handling


def test_ogr_mem_12():

    #######################################################
    # Create memory Layer
    lyr = gdaltest.mem_ds.GetLayerByName('tpoly')
    assert lyr is not None

    # Set the date of the first feature
    f = lyr.GetFeature(1)
    f.SetField("WHEN", 2008, 3, 19, 16, 15, 00, 0)
    lyr.SetFeature(f)
    f = lyr.GetFeature(1)
    idx = f.GetFieldIndex('WHEN')
    expected = [2008, 3, 19, 16, 15, 0.0, 0]
    result = f.GetFieldAsDateTime(idx)
    for i, value in enumerate(result):
        assert value == expected[i], ('%s != %s' % (result, expected))

###############################################################################
# Test Get/Set on StringList, IntegerList, RealList


def test_ogr_mem_13():

    lyr = gdaltest.mem_ds.CreateLayer('listlayer')
    field_defn = ogr.FieldDefn('stringlist', ogr.OFTStringList)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('intlist', ogr.OFTIntegerList)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('reallist', ogr.OFTRealList)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(feature_def=lyr.GetLayerDefn())

    try:
        feat.SetFieldStringList
    except AttributeError:
        # OG python bindings
        pytest.skip()

    feat.SetFieldStringList(0, ['a', 'b'])
    assert feat.GetFieldAsStringList(0) == ['a', 'b']

    feat.SetFieldIntegerList(1, [2, 3])
    assert feat.GetFieldAsIntegerList(1) == [2, 3]

    feat.SetFieldDoubleList(2, [4., 5.])
    assert feat.GetFieldAsDoubleList(2) == [4., 5.]

###############################################################################
# Test SetNextByIndex


def test_ogr_mem_14():

    lyr = gdaltest.mem_ds.CreateLayer('SetNextByIndex')
    field_defn = ogr.FieldDefn('foo', ogr.OFTString)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    feat.SetField(0, 'first feature')
    lyr.CreateFeature(feat)
    feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    feat.SetField(0, 'second feature')
    lyr.CreateFeature(feat)
    feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    feat.SetField(0, 'third feature')
    lyr.CreateFeature(feat)

    assert lyr.TestCapability(ogr.OLCFastSetNextByIndex), \
        'OLCFastSetNextByIndex capability test failed.'

    assert lyr.SetNextByIndex(1) == 0, 'SetNextByIndex() failed'
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString(0) == 'second feature', 'did not get expected feature'

    assert lyr.SetNextByIndex(-1) != 0, 'SetNextByIndex() should have failed'

    assert lyr.SetNextByIndex(100) != 0, 'SetNextByIndex() should have failed'

    lyr.SetAttributeFilter("foo != 'second feature'")

    assert not lyr.TestCapability(ogr.OLCFastSetNextByIndex), \
        'OLCFastSetNextByIndex capability test should have failed.'

    assert lyr.SetNextByIndex(1) == 0, 'SetNextByIndex() failed'
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString(0) == 'third feature', 'did not get expected feature'

###############################################################################
# Test non-linear geometries


def test_ogr_mem_15():

    lyr = gdaltest.mem_ds.CreateLayer('wkbCircularString', geom_type=ogr.wkbCircularString)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('CIRCULARSTRING(0 0,1 0,0 0)'))
    lyr.CreateFeature(f)
    f = None

    assert lyr.GetGeomType() == ogr.wkbCircularString
    assert lyr.GetLayerDefn().GetGeomType() == ogr.wkbCircularString
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbCircularString
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryType() == ogr.wkbCircularString

    # Test SetNonLinearGeometriesEnabledFlag(False)
    old_val = ogr.GetNonLinearGeometriesEnabledFlag()
    ogr.SetNonLinearGeometriesEnabledFlag(False)
    try:
        assert lyr.GetGeomType() == ogr.wkbLineString
        assert lyr.GetLayerDefn().GetGeomType() == ogr.wkbLineString
        assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbLineString
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        g = f.GetGeometryRef()
        assert g.GetGeometryType() == ogr.wkbLineString

        lyr.ResetReading()
        f = lyr.GetNextFeature()
        g = f.GetGeomFieldRef(0)
        assert g.GetGeometryType() == ogr.wkbLineString
    finally:
        ogr.SetNonLinearGeometriesEnabledFlag(old_val)

###############################################################################
# Test map implementation


def test_ogr_mem_16():

    lyr = gdaltest.mem_ds.CreateLayer('ogr_mem_16')
    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    assert ret == 0
    assert f.GetFID() == 0

    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    assert ret == 0
    assert f.GetFID() == 1

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(100000000)
    ret = lyr.CreateFeature(f)
    assert ret == 0

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(100000000)
    ret = lyr.SetFeature(f)
    assert ret == 0

    assert lyr.GetFeatureCount() == 3

    assert lyr.GetFeature(0) is not None
    assert lyr.GetFeature(1) is not None
    assert lyr.GetFeature(2) is None
    assert lyr.GetFeature(100000000) is not None

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetFID() == 0
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1
    f = lyr.GetNextFeature()
    assert f.GetFID() == 100000000
    f = lyr.GetNextFeature()
    assert f is None

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(100000000)
    ret = lyr.CreateFeature(f)
    assert ret == 0
    assert f.GetFID() == 2

    assert lyr.GetFeatureCount() == 4

    ret = lyr.DeleteFeature(1)
    assert ret == 0

    assert lyr.GetFeatureCount() == 3

    ret = lyr.DeleteFeature(1)
    assert ret != 0

    assert lyr.GetFeatureCount() == 3

    # Test first feature with huge ID
    lyr = gdaltest.mem_ds.CreateLayer('ogr_mem_16_bis')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1234567890123)
    ret = lyr.CreateFeature(f)
    assert ret == 0
    assert f.GetFID() == 1234567890123
    f = None  # Important we must not have dangling references before modifying the schema !

    # Create a field so as to test OGRMemLayerIteratorMap
    lyr.CreateField(ogr.FieldDefn('foo'))

###############################################################################
# Test Dataset.GetNextFeature() implementation


def test_ogr_mem_17():

    ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds.CreateLayer('ogr_mem_1')
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    lyr = ds.CreateLayer('ogr_mem_2')
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    lyr = ds.CreateLayer('ogr_mem_3')
    # Empty layer

    lyr = ds.CreateLayer('ogr_mem_4')
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    f, lyr = ds.GetNextFeature()
    assert f is not None and lyr.GetName() == 'ogr_mem_1'

    f, lyr = ds.GetNextFeature()
    assert f is not None and lyr.GetName() == 'ogr_mem_1'

    f, lyr = ds.GetNextFeature()
    assert f is not None and lyr.GetName() == 'ogr_mem_2'

    f, lyr = ds.GetNextFeature()
    assert f is not None and lyr.GetName() == 'ogr_mem_4'

    f, lyr = ds.GetNextFeature()
    assert f is None and lyr is None

    f, lyr = ds.GetNextFeature()
    assert f is None and lyr is None

    ds.ResetReading()

    f, lyr = ds.GetNextFeature()
    assert f is not None and lyr.GetName() == 'ogr_mem_1'

    ds.ResetReading()

    f, lyr, pct = ds.GetNextFeature(include_pct=True)
    assert f is not None and lyr.GetName() == 'ogr_mem_1' and pct == 0.25

    f, pct = ds.GetNextFeature(include_layer=False, include_pct=True)
    assert f is not None and pct == 0.50

    f, pct = ds.GetNextFeature(include_layer=False, include_pct=True)
    assert f is not None and pct == 0.75

    f, pct = ds.GetNextFeature(include_layer=False, include_pct=True)
    assert f is not None and pct == 1.0

    f, pct = ds.GetNextFeature(include_layer=False, include_pct=True)
    assert f is None and pct == 1.0

    f, pct = ds.GetNextFeature(include_layer=False, include_pct=True)
    assert f is None and pct == 1.0

    ds.ResetReading()

    f = ds.GetNextFeature(include_layer=False)
    assert f is not None


###############################################################################


def test_ogr_mem_coordinate_epoch():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srs.SetCoordinateEpoch(2021.3)

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('foo', srs=srs)
    srs = lyr.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == '4326'
    assert srs.GetCoordinateEpoch() == 2021.3
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]


###############################################################################


def test_ogr_mem_alter_geom_field_defn():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('foo')
    assert lyr.TestCapability(ogr.OLCAlterGeomFieldDefn)

    new_geom_field_defn = ogr.GeomFieldDefn('my_name', ogr.wkbPoint)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4269)
    new_geom_field_defn.SetSpatialRef(srs)
    assert lyr.AlterGeomFieldDefn(0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG) == ogr.OGRERR_NONE
    assert lyr.GetSpatialRef().IsSame(srs)

    srs.SetCoordinateEpoch(2022)
    new_geom_field_defn.SetSpatialRef(srs)
    assert lyr.AlterGeomFieldDefn(0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG) == ogr.OGRERR_NONE
    assert lyr.GetSpatialRef().GetCoordinateEpoch() == 2022

    srs.SetCoordinateEpoch(0)
    new_geom_field_defn.SetSpatialRef(srs)
    assert lyr.AlterGeomFieldDefn(0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG) == ogr.OGRERR_NONE
    assert lyr.GetSpatialRef().GetCoordinateEpoch() == 0

    srs.SetCoordinateEpoch(2022)
    new_geom_field_defn.SetSpatialRef(srs)
    assert lyr.AlterGeomFieldDefn(0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_COORD_EPOCH_FLAG) == ogr.OGRERR_NONE
    assert lyr.GetSpatialRef().GetCoordinateEpoch() == 2022

    new_geom_field_defn.SetSpatialRef(None)
    assert lyr.AlterGeomFieldDefn(0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_COORD_EPOCH_FLAG) == ogr.OGRERR_NONE
    assert lyr.GetSpatialRef().GetCoordinateEpoch() == 2022

    new_geom_field_defn.SetSpatialRef(None)
    assert lyr.AlterGeomFieldDefn(0, new_geom_field_defn, ogr.ALTER_GEOM_FIELD_DEFN_ALL_FLAG) == ogr.OGRERR_NONE
    assert lyr.GetSpatialRef() is None

###############################################################################


def test_ogr_mem_arrow_stream_numpy():
    pytest.importorskip('osgeo.gdal_array')
    numpy = pytest.importorskip('numpy')
    import datetime

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('foo')
    stream = lyr.GetArrowStreamAsNumPy()

    with pytest.raises(Exception):
        with gdaltest.error_handler():
            lyr.GetArrowStreamAsNumPy()

    it = iter(stream)
    with pytest.raises(StopIteration):
        next(it)

    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    stream = lyr.GetArrowStreamAsNumPy(options = ['USE_MASKED_ARRAYS=NO'])
    batches = [ batch for batch in stream ]
    assert len(batches) == 1
    assert batches[0].keys() == { "OGC_FID", "wkb_geometry" }
    assert batches[0]["OGC_FID"][0] == 0
    assert batches[0]["wkb_geometry"][0] is None

    field = ogr.FieldDefn("str", ogr.OFTString)
    lyr.CreateField(field)

    field = ogr.FieldDefn("bool", ogr.OFTInteger)
    field.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int16", ogr.OFTInteger)
    field.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int32", ogr.OFTInteger)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int64", ogr.OFTInteger64)
    lyr.CreateField(field)

    field = ogr.FieldDefn("float32", ogr.OFTReal)
    field.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(field)

    field = ogr.FieldDefn("float64", ogr.OFTReal)
    lyr.CreateField(field)

    field = ogr.FieldDefn("date", ogr.OFTDate)
    lyr.CreateField(field)

    field = ogr.FieldDefn("time", ogr.OFTTime)
    lyr.CreateField(field)

    field = ogr.FieldDefn("datetime", ogr.OFTDateTime)
    lyr.CreateField(field)

    field = ogr.FieldDefn("binary", ogr.OFTBinary)
    lyr.CreateField(field)

    field = ogr.FieldDefn("strlist", ogr.OFTStringList)
    lyr.CreateField(field)

    field = ogr.FieldDefn("boollist", ogr.OFTIntegerList)
    field.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int16list", ogr.OFTIntegerList)
    field.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int32list", ogr.OFTIntegerList)
    lyr.CreateField(field)

    field = ogr.FieldDefn("int64list", ogr.OFTInteger64List)
    lyr.CreateField(field)

    field = ogr.FieldDefn("float32list", ogr.OFTRealList)
    field.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(field)

    field = ogr.FieldDefn("float64list", ogr.OFTRealList)
    lyr.CreateField(field)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField("bool", 1)
    f.SetField("int16", -12345)
    f.SetField("int32", 12345678)
    f.SetField("int64", 12345678901234)
    f.SetField("float32", 1.25)
    f.SetField("float64", 1.250123)
    f.SetField("str", "abc")
    f.SetField("date", "2022-05-31")
    f.SetField("time", "12:34:56.789")
    f.SetField("datetime", "2022-05-31T12:34:56.789Z")
    f.SetField("boollist", "[False,True]")
    f.SetField("int16list", "[-12345,12345]")
    f.SetField("int32list", "[-12345678,12345678]")
    f.SetField("int64list", "[-12345678901234,12345678901234]")
    f.SetField("float32list", "[-1.25,1.25]")
    f.SetField("float64list", "[-1.250123,1.250123]")
    f.SetField("strlist", "[\"abc\",\"defghi\"]")
    f.SetFieldBinaryFromHexString("binary", 'DEAD')
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    lyr.CreateFeature(f)

    stream = lyr.GetArrowStreamAsNumPy(options = ['USE_MASKED_ARRAYS=NO'])
    batches = [ batch for batch in stream ]
    assert len(batches) == 1
    batch = batches[0]
    assert batch.keys() == {
        'OGC_FID', 'str', 'bool', 'int16', 'int32', 'int64',
        'float32', 'float64', 'date', 'time', 'datetime', 'binary',
        'strlist', 'boollist', 'int16list', 'int32list', 'int64list',
        'float32list', 'float64list', 'wkb_geometry' }
    assert batch["OGC_FID"][1] == 1
    for fieldname in ('bool', 'int16', 'int32', 'int64',
                      'float32', 'float64'):
        assert batch[fieldname][1] == f.GetField(fieldname)
    assert batch['str'][1] == f.GetField('str').encode('utf-8')
    assert batch['date'][1] == numpy.datetime64('2022-05-31')
    assert batch['time'][1] == datetime.time(12, 34, 56, 789000)
    assert batch['datetime'][1] == numpy.datetime64('2022-05-31T12:34:56.789')
    assert numpy.array_equal(batch['boollist'][1], numpy.array([False,  True]))
    assert numpy.array_equal(batch['int16list'][1], numpy.array([-12345, 12345]))
    assert numpy.array_equal(batch['int32list'][1], numpy.array([-12345678, 12345678]))
    assert numpy.array_equal(batch['int64list'][1], numpy.array([-12345678901234, 12345678901234]))
    assert numpy.array_equal(batch['float32list'][1], numpy.array([-1.25, 1.25]))
    assert numpy.array_equal(batch['float64list'][1], numpy.array([-1.250123, 1.250123]))
    assert numpy.array_equal(batch['strlist'][1], numpy.array([b'abc', b'defghi'], dtype='|S6'))
    assert batch['binary'][1] == b'\xDE\xAD'
    assert len(batch["wkb_geometry"][1]) == 21

###############################################################################


def test_ogr_mem_arrow_stream_pyarrow():
    pytest.importorskip('pyarrow')

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('foo')
    stream = lyr.GetArrowStreamAsPyArrow()

    with pytest.raises(Exception):
        with gdaltest.error_handler():
            lyr.GetArrowStreamAsPyArrow()

    it = iter(stream)
    with pytest.raises(StopIteration):
        next(it)

    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    stream = lyr.GetArrowStreamAsPyArrow()
    assert str(stream.schema) == 'struct<OGC_FID: int64 not null, wkb_geometry: binary>'
    md = stream.schema['wkb_geometry'].metadata
    assert b'ARROW:extension:name' in md
    assert md[b'ARROW:extension:name'] == b'ogc.wkb'
    batches = [ batch for batch in stream ]
    assert len(batches) == 1
    arrays = batches[0].flatten()
    assert len(arrays) == 2
