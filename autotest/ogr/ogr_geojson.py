#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GeoJSON driver test suite.
# Author:   Mateusz Loskot <mateusz@loskot.net>
#
###############################################################################
# Copyright (c) 2007, Mateusz Loskot <mateusz@loskot.net>
# Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
# Copyright (c) 2013, Kyle Shannon <kyle at pobox dot com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import json
import math
import os
import struct

from osgeo import osr
from osgeo import ogr
from osgeo import gdal

import gdaltest
import ogrtest
import pytest

pytestmark = pytest.mark.require_driver('GeoJSON')

###############################################################################
# Test utilities


def validate_layer(lyr, name, features, typ, fields, box):

    if name is not None and name != lyr.GetName():
        print('Wrong layer name')
        return False

    if features != lyr.GetFeatureCount():
        print('Wrong number of features')
        return False

    lyrDefn = lyr.GetLayerDefn()
    if lyrDefn is None:
        print('Layer definition is none')
        return False

    if typ != lyrDefn.GetGeomType():
        print('Wrong geometry type')
        print(lyrDefn.GetGeomType())
        return False

    if fields != lyrDefn.GetFieldCount():
        print('Wrong number of fields')
        return False

    extent = lyr.GetExtent()

    minx = abs(extent[0] - box[0])
    maxx = abs(extent[1] - box[1])
    miny = abs(extent[2] - box[2])
    maxy = abs(extent[3] - box[3])

    if max(minx, maxx, miny, maxy) > 0.0001:
        print('Wrong spatial extent of layer')
        print(extent)
        return False

    return True


def verify_geojson_copy(fname, fids, names):

    if gdaltest.gjpoint_feat is None:
        print('Missing features collection')
        return False

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
    for i in range(len(gdaltest.gjpoint_feat)):

        orig_feat = gdaltest.gjpoint_feat[i]
        feat = lyr.GetNextFeature()

        if feat is None:
            print('Failed trying to read feature')
            return False

        if ogrtest.check_feature_geometry(feat, orig_feat.GetGeometryRef(),
                                          max_error=0.001) != 0:
            print('Geometry test failed')
            gdaltest.gjpoint_feat = None
            return False

    gdaltest.gjpoint_feat = None

    lyr = None

    return True


def copy_shape_to_geojson(gjname, compress=None):

    if compress is not None:
        if compress[0:5] == '/vsig':
            dst_name = os.path.join('/vsigzip/', 'tmp', gjname + '.geojson' + '.gz')
        elif compress[0:4] == '/vsiz':
            dst_name = os.path.join('/vsizip/', 'tmp', gjname + '.geojson' + '.zip')
        elif compress == '/vsistdout/':
            dst_name = compress
        else:
            return False, None
    else:
        dst_name = os.path.join('tmp', gjname + '.geojson')

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource(dst_name)
    if ds is None:
        return False, dst_name

    ######################################################
    # Create layer
    lyr = ds.CreateLayer(gjname)
    if lyr is None:
        return False, dst_name

    ######################################################
    # Setup schema (all test shapefiles use common schema)
    ogrtest.quick_create_layer_def(lyr,
                                   [('FID', ogr.OFTReal),
                                    ('NAME', ogr.OFTString)])

    ######################################################
    # Copy in gjpoint.shp

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())

    src_name = os.path.join('data', 'shp', gjname + '.shp')
    shp_ds = ogr.Open(src_name)
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.gjpoint_feat = []

    while feat is not None:

        gdaltest.gjpoint_feat.append(feat)

        dst_feat.SetFrom(feat)
        lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    shp_lyr = None
    lyr = None

    ds = None

    return True, dst_name

###############################################################################
# Test file-based DS with standalone "Point" feature object.


def test_ogr_geojson_2():

    ds = ogr.Open('data/geojson/point.geojson')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayerByName('point')
    assert lyr is not None, 'Missing layer called point'

    extent = (100.0, 100.0, 0.0, 0.0)

    rc = validate_layer(lyr, 'point', 1, ogr.wkbPoint, 0, extent)
    assert rc

    lyr = None

###############################################################################
# Test file-based DS with standalone "LineString" feature object.


def test_ogr_geojson_3():

    ds = ogr.Open('data/geojson/linestring.geojson')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayerByName('linestring')
    assert lyr is not None, 'Missing layer called linestring'

    extent = (100.0, 101.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'linestring', 1, ogr.wkbLineString, 0, extent)
    assert rc

    lyr = None

##############################################################################
# Test file-based DS with standalone "Polygon" feature object.


def test_ogr_geojson_4():

    ds = ogr.Open('data/geojson/polygon.geojson')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayerByName('polygon')
    assert lyr is not None, 'Missing layer called polygon'

    extent = (100.0, 101.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'polygon', 1, ogr.wkbPolygon, 0, extent)
    assert rc

    lyr = None

##############################################################################
# Test file-based DS with standalone "GeometryCollection" feature object.


def test_ogr_geojson_5():

    ds = ogr.Open('data/geojson/geometrycollection.geojson')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayerByName('geometrycollection')
    assert lyr is not None, 'Missing layer called geometrycollection'

    extent = (100.0, 102.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'geometrycollection', 1, ogr.wkbGeometryCollection, 0, extent)
    assert rc

    lyr = None

##############################################################################
# Test file-based DS with standalone "MultiPoint" feature object.


def test_ogr_geojson_6():

    ds = ogr.Open('data/geojson/multipoint.geojson')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayerByName('multipoint')
    assert lyr is not None, 'Missing layer called multipoint'

    extent = (100.0, 101.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'multipoint', 1, ogr.wkbMultiPoint, 0, extent)
    assert rc

    lyr = None

##############################################################################
# Test file-based DS with standalone "MultiLineString" feature object.


def test_ogr_geojson_7():

    ds = ogr.Open('data/geojson/multilinestring.geojson')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayerByName('multilinestring')
    assert lyr is not None, 'Missing layer called multilinestring'

    extent = (100.0, 103.0, 0.0, 3.0)

    rc = validate_layer(lyr, 'multilinestring', 1, ogr.wkbMultiLineString, 0, extent)
    assert rc

    lyr = None

##############################################################################
# Test file-based DS with standalone "MultiPolygon" feature object.


def test_ogr_geojson_8():

    ds = ogr.Open('data/geojson/multipolygon.geojson')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayerByName('multipolygon')
    assert lyr is not None, 'Missing layer called multipolygon'

    extent = (100.0, 103.0, 0.0, 3.0)

    rc = validate_layer(lyr, 'multipolygon', 1, ogr.wkbMultiPolygon, 0, extent)
    assert rc

    lyr = None

##############################################################################
# Test translation of data/gjpoint.shp to GeoJSON file


def test_ogr_geojson_9():

    tests = [
        ['gjpoint', [1], ['Point 1']],
        ['gjline', [1], ['Line 1']],
        ['gjpoly', [1], ['Polygon 1']],
        ['gjmultipoint', [1], ['MultiPoint 1']],
        ['gjmultiline', [2], ['MultiLine 1']],
        ['gjmultipoly', [2], ['MultiPoly 1']]
    ]

    for test in tests:

        rc, dstname = copy_shape_to_geojson(test[0])
        try:
            assert rc, ('Failed making copy of ' + test[0] + '.shp')

            rc = verify_geojson_copy(dstname, test[1], test[2])
            assert rc, ('Verification of copy of ' + test[0] + '.shp failed')
        finally:
            if dstname:
                gdal.Unlink(dstname)

##############################################################################
# Test translation of data/gjpoint.shp to GZip compressed GeoJSON file


def test_ogr_geojson_10():

    tests = [
        ['gjpoint', [1], ['Point 1']],
        ['gjline', [1], ['Line 1']],
        ['gjpoly', [1], ['Polygon 1']],
        ['gjmultipoint', [1], ['MultiPoint 1']],
        ['gjmultiline', [2], ['MultiLine 1']],
        ['gjmultipoly', [2], ['MultiPoly 1']]
    ]

    for test in tests:

        rc, dstname = copy_shape_to_geojson(test[0], '/vsigzip/')
        try:
            assert rc, ('Failed making copy of ' + test[0] + '.shp')

            rc = verify_geojson_copy(dstname, test[1], test[2])
            assert rc, ('Verification of copy of ' + test[0] + '.shp failed')
        finally:
            if dstname:
                dstname = dstname[len("/vsigzip/"):]
                gdal.Unlink(dstname)
                gdal.Unlink(dstname + ".properties")

###############################################################################


def test_ogr_geojson_11():

    ds = ogr.Open('data/geojson/srs_name.geojson')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayerByName('srs_name')
    assert lyr is not None, 'Missing layer called srs_name'

    extent = (100.0, 102.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'srs_name', 1, ogr.wkbGeometryCollection, 0, extent)
    assert rc

    ref = lyr.GetSpatialRef()
    pcs = int(ref.GetAuthorityCode('PROJCS'))
    assert pcs == 26915, 'Spatial reference was not valid'

    feature = lyr.GetNextFeature()
    geometry = feature.GetGeometryRef().GetGeometryRef(0)

    srs = geometry.GetSpatialReference()
    pcs = int(srs.GetAuthorityCode('PROJCS'))
    assert pcs == 26916, 'Spatial reference for individual geometry was not valid'

    lyr = None

###############################################################################
# Test DS passed as name with standalone "Point" feature object (#3377)


def test_ogr_geojson_12():

    if os.name == 'nt':
        pytest.skip()

    import test_cli_utilities

    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -ro -al \'{"type": "Point","coordinates": [100.0, 0.0]}\'')
    assert ret.find(' POINT (100 0)') != -1


###############################################################################
# Test writing to stdout (#3381)

def test_ogr_geojson_13():

    test = ['gjpoint', [1], ['Point 1']]

    rc, _ = copy_shape_to_geojson(test[0], '/vsistdout/')
    assert rc, ('Failed making copy of ' + test[0] + '.shp')

###############################################################################
# Test reading & writing various degenerated geometries


def test_ogr_geojson_14():

    with gdaltest.error_handler():
        ds = ogr.Open('data/geojson/ogr_geojson_14.geojson')
    lyr = ds.GetLayer(0)

    try:
        out_ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('tmp/out_ogr_geojson_14.geojson')
        out_lyr = out_ds.CreateLayer('lyr')

        with gdaltest.error_handler():
            for feat in lyr:
                geom = feat.GetGeometryRef()
                if geom is not None:
                    # print(geom)
                    out_feat = ogr.Feature(feature_def=out_lyr.GetLayerDefn())
                    out_feat.SetGeometry(geom)
                    out_lyr.CreateFeature(out_feat)

        out_ds = None
    finally:
        try:
            os.remove('tmp/out_ogr_geojson_14.geojson')
        except OSError:
            pass

###############################################################################
# Test Feature.ExportToJson (#3870)


def test_ogr_geojson_15():

    feature_defn = ogr.FeatureDefn()
    feature_defn.AddFieldDefn(ogr.FieldDefn("foo"))
    field_defn = ogr.FieldDefn("boolfield", ogr.OFTInteger)
    field_defn.SetSubType(ogr.OFSTBoolean)
    feature_defn.AddFieldDefn(field_defn)

    feature = ogr.Feature(feature_defn)
    feature.SetField("foo", "bar")
    feature.SetField("boolfield", True)
    feature.SetFID(0)

    geom = ogr.CreateGeometryFromWkt("POINT(1 2)")
    feature.SetGeometry(geom)

    try:
        out = feature.ExportToJson()
    except ImportError:
        pytest.skip()

    expected_out = """{"geometry": {"type": "Point", "coordinates": [1.0, 2.0]}, "type": "Feature", "properties": {"foo": "bar", "boolfield": true}, "id": 0}"""

    if out != expected_out:
        out_json = json.loads(out)
        expected_out_json = json.loads(expected_out)
        assert out_json == expected_out_json, out

    out = feature.ExportToJson(as_object=True)
    expected_out = {'geometry': {'type': 'Point', 'coordinates': [1.0, 2.0]}, 'type': 'Feature', 'properties': {'foo': 'bar', "boolfield": True}, 'id': 0}

    assert out == expected_out

###############################################################################
# Test reading files with no extension (#4314)


def test_ogr_geojson_20():

    from glob import glob

    geojson_files = glob('data/*.json')
    geojson_files.extend(glob('data/*.geojson'))

    for gj in geojson_files:
        # create tmp file with no file extension
        data = open(gj, 'rb').read()

        f = gdal.VSIFOpenL('/vsimem/testgj', 'wb')
        gdal.VSIFWriteL(data, 1, len(data), f)
        gdal.VSIFCloseL(f)

        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ds = ogr.Open('/vsimem/testgj')
        gdal.PopErrorHandler()
        if ds is None:
            print(gj)
            print(data.decode('LATIN1'))
            pytest.fail('Failed to open datasource')
        ds = None

        gdal.Unlink('/vsimem/testgj')


###############################################################################
# Test reading output of geocouch spatiallist


def test_ogr_geojson_21():

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature",
 "geometry": {"type":"Point","coordinates":[1,2]},
 "properties": {"_id":"aid", "_rev":"arev", "type":"Feature",
                "properties":{"intvalue" : 2, "floatvalue" : 3.2, "strvalue" : "foo", "properties": { "foo": "bar"}}}}]}""")
    assert ds is not None, 'Failed to open datasource'

    lyr = ds.GetLayerByName('OGRGeoJSON')

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POINT (1 2)')
    if feature.GetFieldAsString("_id") != 'aid' or \
       feature.GetFieldAsString("_rev") != 'arev' or \
       feature.GetFieldAsInteger("intvalue") != 2 or \
       ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Same as ogr_geojson_21 with several features


def test_ogr_geojson_22():

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature",
 "geometry": {"type":"Point","coordinates":[1,2]},
 "properties": {"_id":"aid", "_rev":"arev", "type":"Feature",
                "properties":{"intvalue" : 2, "floatvalue" : 3.2, "strvalue" : "foo"}}},
{"type": "Feature",
 "geometry": {"type":"Point","coordinates":[3,4]},
 "properties": {"_id":"aid2", "_rev":"arev2", "type":"Feature",
                "properties":{"intvalue" : 3.5, "str2value" : "bar"}}}]}""")
    assert ds is not None, 'Failed to open datasource'

    lyr = ds.GetLayerByName('OGRGeoJSON')

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POINT (1 2)')
    if feature.GetFieldAsString("_id") != 'aid' or \
       feature.GetFieldAsString("_rev") != 'arev' or \
       feature.GetFieldAsDouble("intvalue") != 2 or \
       ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POINT (3 4)')
    if feature.GetFieldAsString("_id") != 'aid2' or \
       feature.GetFieldAsString("_rev") != 'arev2' or \
       feature.GetFieldAsDouble("intvalue") != 3.5 or \
       feature.GetFieldAsString("str2value") != 'bar' or \
       ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Write GeoJSON with bbox and test SRS writing&reading back


def test_ogr_geojson_23():

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_23.json')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4322)
    lyr = ds.CreateLayer('foo', srs=sr, options=['WRITE_BBOX=YES'])
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 10)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 20)'))
    lyr.CreateFeature(feat)
    assert lyr.GetExtent() == (1.0, 2.0, 10.0, 20.0)
    assert lyr.GetExtent(geom_field=0) == (1.0, 2.0, 10.0, 20.0)
    assert lyr.GetExtent(geom_field=1, can_return_null=True) is None
    lyr = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_geojson_23.json')
    lyr = ds.GetLayer(0)
    sr_got = lyr.GetSpatialRef()
    ds = None

    sr.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    assert sr_got.IsSame(sr), 'did not get expected SRS'

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_23.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_23.json')

    assert data.find('"bbox": [ 1, 10, 2, 20 ]') != -1, 'did not find global bbox'

    assert data.find('"bbox": [ 1.0, 10.0, 1.0, 10.0 ]') != -1, \
        'did not find first feature bbox'

###############################################################################
# Test alternate form of geojson


def test_ogr_geojson_24():

    content = """loadGeoJSON({"layerFoo": { "type": "Feature",
  "geometry": {
    "type": "Point",
    "coordinates": [2, 49]
    },
  "name": "bar"
},
"layerBar": { "type": "FeatureCollection", "features" : [  { "type": "Feature",
  "geometry": {
    "type": "Point",
    "coordinates": [2, 49]
    },
  "other_name": "baz"
}]}})"""

    for i in range(2):
        if i == 0:
            ds = ogr.Open(content)
        else:
            gdal.FileFromMemBuffer('/vsimem/ogr_geojson_24.js', content)
            ds = ogr.Open('/vsimem/ogr_geojson_24.js')
            gdal.Unlink('/vsimem/ogr_geojson_24.js')

        assert ds is not None, 'Failed to open datasource'

        lyr = ds.GetLayerByName('layerFoo')
        assert lyr is not None, 'cannot find layer'

        feature = lyr.GetNextFeature()
        ref_geom = ogr.CreateGeometryFromWkt('POINT (2 49)')
        if feature.GetFieldAsString("name") != 'bar' or \
                ogrtest.check_feature_geometry(feature, ref_geom) != 0:
            feature.DumpReadable()
            pytest.fail()

        lyr = ds.GetLayerByName('layerBar')
        assert lyr is not None, 'cannot find layer'

        feature = lyr.GetNextFeature()
        ref_geom = ogr.CreateGeometryFromWkt('POINT (2 49)')
        if feature.GetFieldAsString("other_name") != 'baz' or \
                ogrtest.check_feature_geometry(feature, ref_geom) != 0:
            feature.DumpReadable()
            pytest.fail()

        ds = None


###############################################################################
# Test 64bit support


def test_ogr_geojson_26():

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature", "id": 1,
 "geometry": {"type":"Point","coordinates":[1,2]},
 "properties": { "intvalue" : 1, "int64" : 1234567890123, "intlist" : [1] }},
{"type": "Feature", "id": 1234567890123,
 "geometry": {"type":"Point","coordinates":[3,4]},
 "properties": { "intvalue" : 1234567890123, "intlist" : [1, 1234567890123] }},
 ]}""")
    assert ds is not None, 'Failed to open datasource'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None

    feature = lyr.GetNextFeature()
    if feature.GetFID() != 1:
        feature.DumpReadable()
        pytest.fail()
    if feature.GetField("intvalue") != 1:
        feature.DumpReadable()
        pytest.fail()
    if feature.GetField("int64") != 1234567890123:
        feature.DumpReadable()
        pytest.fail()

    feature = lyr.GetNextFeature()
    if feature.GetFID() != 1234567890123:
        feature.DumpReadable()
        pytest.fail()
    if feature.GetField("intvalue") != 1234567890123:
        feature.DumpReadable()
        pytest.fail()
    if feature.GetField("intlist") != [1, 1234567890123]:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_26.json')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('int64', ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn('int64list', ogr.OFTInteger64List))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1234567890123)
    f.SetField(0, 1234567890123)
    f.SetFieldInteger64List(1, [1234567890123])
    lyr.CreateFeature(f)
    f = None
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_26.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_26.json')

    assert '{ "type": "Feature", "id": 1234567890123, "properties": { "int64": 1234567890123, "int64list": [ 1234567890123 ] }, "geometry": null }' in data

###############################################################################
# Test workaround for 64bit values (returned as strings)


def test_ogr_geojson_27():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    # Warning 1: Integer values probably ranging out of 64bit integer range
    # have been found. Will be clamped to INT64_MIN/INT64_MAX
    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature",
 "geometry": {"type":"Point","coordinates":[1,2]},
 "properties": { "intvalue" : 1 }},
{"type": "Feature",
 "geometry": {"type":"Point","coordinates":[3,4]},
 "properties": { "intvalue" : 12345678901231234567890123 }},
 ]}""")
    gdal.PopErrorHandler()
    assert ds is not None, 'Failed to open datasource'

    lyr = ds.GetLayerByName('OGRGeoJSON')

    feature = lyr.GetNextFeature()
    if feature.GetField("intvalue") != 1:
        feature.DumpReadable()
        pytest.fail()

    feature = lyr.GetNextFeature()
    if feature.GetField("intvalue") != 9223372036854775807:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Test handling of huge coordinates (#5377)


def test_ogr_geojson_35():

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_35.json')
    lyr = ds.CreateLayer('foo')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(1)
    geom = ogr.Geometry(ogr.wkbPoint)
    geom.AddPoint_2D(-1.79769313486231571e+308, -1.79769313486231571e+308)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    gdal.PushErrorHandler('CPLQuietErrorHandler')

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(2)
    geom = ogr.Geometry(ogr.wkbPoint)
    geom.AddPoint(-1.7e308 * 2, 1.7e308 * 2, 1.7e308 * 2)  # evaluates to -inf, inf
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(3)
    geom = ogr.Geometry(ogr.wkbLineString)
    geom.AddPoint_2D(0, 0)
    geom.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2)  # evaluates to -inf, inf
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(4)
    geom = ogr.Geometry(ogr.wkbPolygon)
    geom2 = ogr.Geometry(ogr.wkbLinearRing)
    geom2.AddPoint_2D(0, 0)
    geom2.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2)  # evaluates to -inf, inf
    geom.AddGeometry(geom2)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(5)
    geom = ogr.Geometry(ogr.wkbMultiPoint)
    geom2 = ogr.Geometry(ogr.wkbPoint)
    geom2.AddPoint_2D(0, 0)
    geom2 = ogr.Geometry(ogr.wkbPoint)
    geom2.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2)  # evaluates to -inf, inf
    geom.AddGeometry(geom2)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(6)
    geom = ogr.Geometry(ogr.wkbMultiLineString)
    geom2 = ogr.Geometry(ogr.wkbLineString)
    geom2.AddPoint_2D(0, 0)
    geom2 = ogr.Geometry(ogr.wkbLineString)
    geom2.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2)  # evaluates to -inf, inf
    geom.AddGeometry(geom2)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(7)
    geom = ogr.Geometry(ogr.wkbMultiPolygon)
    geom2 = ogr.Geometry(ogr.wkbPolygon)
    geom3 = ogr.Geometry(ogr.wkbLinearRing)
    geom3.AddPoint_2D(0, 0)
    geom2.AddGeometry(geom3)
    geom2 = ogr.Geometry(ogr.wkbPolygon)
    geom3 = ogr.Geometry(ogr.wkbLinearRing)
    geom3.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2)  # evaluates to -inf, inf
    geom2.AddGeometry(geom3)
    geom.AddGeometry(geom2)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    gdal.PopErrorHandler()

    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_35.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_35.json')

    assert '-1.79' in data and 'e+308' in data
    for ident in range(2, 8):
        assert data.find('{ "type": "Feature", "id": %d, "properties": { }, "geometry": null }' % ident) != -1


###############################################################################
# Test reading file with UTF-8 BOM (which is supposed to be illegal in JSON...) (#5630)


def test_ogr_geojson_36():

    ds = ogr.Open('data/geojson/point_with_utf8bom.json')
    assert ds is not None, 'Failed to open datasource'
    ds = None

#########################################################################
# Test boolean type support


def test_ogr_geojson_37():

    # Test read support
    ds = ogr.Open("""{"type": "FeatureCollection","features": [
{ "type": "Feature", "properties": { "bool" : false, "not_bool": false, "bool_list" : [false, true], "notbool_list" : [false, 3]}, "geometry": null  },
{ "type": "Feature", "properties": { "bool" : true, "not_bool": 2, "bool_list" : [true] }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert (feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('bool')).GetType() == ogr.OFTInteger and \
       feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('bool')).GetSubType() == ogr.OFSTBoolean)
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('not_bool')).GetSubType() == ogr.OFSTNone
    assert (feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('bool_list')).GetType() == ogr.OFTIntegerList and \
       feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('bool_list')).GetSubType() == ogr.OFSTBoolean)
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('notbool_list')).GetSubType() == ogr.OFSTNone
    f = lyr.GetNextFeature()
    if f.GetField('bool') != 0 or f.GetField('bool_list') != [0, 1]:
        f.DumpReadable()
        pytest.fail()

    out_ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_37.json')
    out_lyr = out_ds.CreateLayer('test')
    for i in range(feat_defn.GetFieldCount()):
        out_lyr.CreateField(feat_defn.GetFieldDefn(i))
    out_f = ogr.Feature(out_lyr.GetLayerDefn())
    out_f.SetFrom(f)
    out_lyr.CreateFeature(out_f)
    out_ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_37.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_37.json')

    assert '"bool": false, "not_bool": 0, "bool_list": [ false, true ], "notbool_list": [ 0, 3 ]' in data

###############################################################################
# Test datetime/date/time type support


def test_ogr_geojson_38():

    # Test read support
    ds = gdal.OpenEx("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "properties": { "dt": "2014-11-20 12:34:56+0100", "dt2": "2014\\/11\\/20", "date":"2014\\/11\\/20", "time":"12:34:56", "no_dt": "2014-11-20 12:34:56+0100", "no_dt2": "2014-11-20 12:34:56+0100" }, "geometry": null },
{ "type": "Feature", "properties": { "dt": "2014\\/11\\/20", "dt2": "2014\\/11\\/20T12:34:56Z", "date":"2014-11-20", "time":"12:34:56", "no_dt": "foo", "no_dt2": 1 }, "geometry": null }
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('dt')).GetType() == ogr.OFTDateTime
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('dt2')).GetType() == ogr.OFTDateTime
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('date')).GetType() == ogr.OFTDate
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('time')).GetType() == ogr.OFTTime
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('no_dt')).GetType() == ogr.OFTString
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('no_dt2')).GetType() == ogr.OFTString
    f = lyr.GetNextFeature()
    if f.GetField('dt') != '2014/11/20 12:34:56+01' or f.GetField('dt2') != '2014/11/20 00:00:00' or \
       f.GetField('date') != '2014/11/20' or f.GetField('time') != '12:34:56':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetField('dt') != '2014/11/20 00:00:00' or f.GetField('dt2') != '2014/11/20 12:34:56+00' or \
       f.GetField('date') != '2014/11/20' or f.GetField('time') != '12:34:56':
        f.DumpReadable()
        pytest.fail()

    tmpfilename = '/vsimem/out.json'
    gdal.VectorTranslate(tmpfilename, ds, options = '-lco NATIVE_DATA=dummy') # dummy NATIVE_DATA so that input values are not copied directly

    fp = gdal.VSIFOpenL(tmpfilename, 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink(tmpfilename)

    assert '"dt": "2014-11-20T12:34:56+01:00", "dt2": "2014-11-20T00:00:00", "date": "2014-11-20", "time": "12:34:56"' in data, data

    ds = gdal.OpenEx("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "properties": { "dt": "2014-11-20 12:34:56+0100", "dt2": "2014\\/11\\/20", "date":"2014\\/11\\/20", "time":"12:34:56", "no_dt": "2014-11-20 12:34:56+0100", "no_dt2": "2014-11-20 12:34:56+0100" }, "geometry": null }
] }""", open_options = ['DATE_AS_STRING=YES'])
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    for i in range(feat_defn.GetFieldCount()):
        assert feat_defn.GetFieldDefn(i).GetType() == ogr.OFTString

###############################################################################
# Test id top-object level


def test_ogr_geojson_39():

    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : "foo", "properties": { "bar" : "baz" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(0).GetName() == 'id' and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTString
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != 'foo' or feat.GetField('bar') != 'baz':
        feat.DumpReadable()
        pytest.fail()

    # Crazy case: properties.id has the precedence because we arbitrarily decided that...
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : "foo", "properties": { "id" : 6 }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(0).GetName() == 'id' and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != 6:
        feat.DumpReadable()
        pytest.fail()

    # Same with 2 features
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : "foo", "properties": { "id" : 6 }, "geometry": null },
{ "type": "Feature", "id" : "bar", "properties": { "id" : 7 }, "geometry": null }
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(0).GetName() == 'id' and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != 6:
        feat.DumpReadable()
        pytest.fail()

    # Crazy case: properties.id has the precedence because we arbitrarily decided that...
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : "foo", "properties": { "id" : "baz" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(0).GetName() == 'id' and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTString
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != 'baz':
        feat.DumpReadable()
        pytest.fail()

    # id and properties.ID (#6538)
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : 1, "properties": { "ID": 2 }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(0).GetName() == 'ID' and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1 or feat.GetField('ID') != 2:
        feat.DumpReadable()
        pytest.fail()

    # Test handling of duplicated id
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : 1, "properties": { "foo": "bar" }, "geometry": null },
{ "type": "Feature", "id" : 1, "properties": { "foo": "baz" }, "geometry": null },
{ "type": "Feature", "id" : 2, "properties": { "foo": "baw" }, "geometry": null }
] }""")
    assert gdal.GetLastErrorMsg() != '', 'expected warning'
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1 or feat.GetField('foo') != 'bar':
        feat.DumpReadable()
        pytest.fail()
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 2 or feat.GetField('foo') != 'baz':
        feat.DumpReadable()
        pytest.fail()
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 3 or feat.GetField('foo') != 'baw':
        feat.DumpReadable()
        pytest.fail()

    # negative id
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : -1, "properties": { "foo": "bar" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(0).GetName() == 'id' and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != -1:
        feat.DumpReadable()
        pytest.fail()

    # negative id 64bit
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : -1234567890123, "properties": { "foo": "bar" }, "geometry": null },
{ "type": "Feature", "id" : -2, "properties": { "foo": "baz" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(0).GetName() == 'id' and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger64
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != -1234567890123:
        feat.DumpReadable()
        pytest.fail()

    # negative id
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : -2, "properties": { "foo": "baz" }, "geometry": null },
{ "type": "Feature", "id" : -1234567890123, "properties": { "foo": "bar" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(0).GetName() == 'id' and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger64
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != -2:
        feat.DumpReadable()
        pytest.fail()

    # positive and then negative id
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : 1, "properties": { "foo": "baz" }, "geometry": null },
{ "type": "Feature", "id" : -1, "properties": { "foo": "bar" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(1).GetName() == 'id' and feat_defn.GetFieldDefn(1).GetType() == ogr.OFTInteger
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != 1:
        feat.DumpReadable()
        pytest.fail()

    # mix of int and string id
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : -2, "properties": { "foo": "baz" }, "geometry": null },
{ "type": "Feature", "id" : "str", "properties": { "foo": "bar" }, "geometry": null },
{ "type": "Feature", "id" : -3, "properties": { "foo": "baz" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(0).GetName() == 'id' and feat_defn.GetFieldDefn(0).GetType() == ogr.OFTString
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != '-2':
        feat.DumpReadable()
        pytest.fail()


###############################################################################
# Test nested attributes


def test_ogr_geojson_40():

    ds = gdal.OpenEx("""{
  "type": "FeatureCollection",
  "features" :
  [
    {
      "type": "Feature",
      "geometry": {
        "type": "Point",
        "coordinates": [ 2, 49 ]
      },
      "properties": {
        "a_property": 1,
        "some_object": {
          "a_property": 1,
          "another_property": 2
        }
      }
    },
    {
      "type": "Feature",
      "geometry": {
        "type": "Point",
        "coordinates": [ 2, 49 ]
      },
      "properties": {
        "a_property": "foo",
        "some_object": {
          "a_property": 1,
          "another_property": 2.34
        }
      }
    }
  ]
}""", gdal.OF_VECTOR, open_options=['FLATTEN_NESTED_ATTRIBUTES=YES', 'NESTED_ATTRIBUTE_SEPARATOR=.'])
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    if feat.GetField('a_property') != 'foo' or feat.GetField('some_object.a_property') != 1 or \
       feat.GetField('some_object.another_property') != 2.34:
        feat.DumpReadable()
        pytest.fail()


###############################################################################
# Test ogr.CreateGeometryFromJson()


def test_ogr_geojson_41():

    # Check that by default we return a WGS 84 SRS
    g = ogr.CreateGeometryFromJson("{ 'type': 'Point', 'coordinates' : [ 2, 49] }")
    assert g.ExportToWkt() == 'POINT (2 49)'
    srs = g.GetSpatialReference()
    g = None

    assert srs.ExportToWkt().find('WGS 84') >= 0

    # But if a crs object is set (allowed originally, but not recommended!), we use it
    g = ogr.CreateGeometryFromJson('{ "type": "Point", "coordinates" : [ 2, 49], "crs": { "type": "name", "properties": { "name": "urn:ogc:def:crs:EPSG::4322" } } }')
    srs = g.GetSpatialReference()
    assert srs.ExportToWkt().find('4322') >= 0

    # But if a crs object is set to null, set no crs
    g = ogr.CreateGeometryFromJson('{ "type": "Point", "coordinates" : [ 2, 49], "crs": null }')
    srs = g.GetSpatialReference()
    assert not srs

###############################################################################
# Test Feature without geometry


def test_ogr_geojson_43():

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature", "properties": {"foo": "bar"}}]}""")
    assert ds is not None, 'Failed to open datasource'

    lyr = ds.GetLayerByName('OGRGeoJSON')

    feature = lyr.GetNextFeature()
    if feature.GetFieldAsString("foo") != 'bar':
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Test null Feature (#6166)


def test_ogr_geojson_44():

    with gdaltest.error_handler():
        ogr.Open("""{"type": "FeatureCollection", "features":[ null ]}""")


###############################################################################
# Test native data support


def test_ogr_geojson_45():

    # Test read support
    content = """{"type": "FeatureCollection", "foo": "bar", "bar": "baz",
    "features":[ { "type": "Feature", "foo": ["bar", "baz", 1.0, true, false,[],{}], "properties": { "myprop": "myvalue" }, "geometry": null } ]}"""
    for i in range(2):
        if i == 0:
            ds = gdal.OpenEx(content, gdal.OF_VECTOR, open_options=['NATIVE_DATA=YES'])
        else:
            gdal.FileFromMemBuffer('/vsimem/ogr_geojson_45.json', content)
            ds = gdal.OpenEx('/vsimem/ogr_geojson_45.json', gdal.OF_VECTOR, open_options=['NATIVE_DATA=YES'])
        lyr = ds.GetLayer(0)
        native_data = lyr.GetMetadataItem("NATIVE_DATA", "NATIVE_DATA")
        assert native_data == '{ "foo": "bar", "bar": "baz" }'
        native_media_type = lyr.GetMetadataItem("NATIVE_MEDIA_TYPE", "NATIVE_DATA")
        assert native_media_type == 'application/vnd.geo+json'
        f = lyr.GetNextFeature()
        native_data = f.GetNativeData()
        if i == 0:
            expected = [
                '{ "type": "Feature", "foo": [ "bar", "baz", 1.000000, true, false, [ ], { } ], "properties": { "myprop": "myvalue" }, "geometry": null }',
                '{ "type": "Feature", "foo": [ "bar", "baz", 1.0, true, false, [ ], { } ], "properties": { "myprop": "myvalue" }, "geometry": null }']
        else:
            expected = ['{"type":"Feature","foo":["bar","baz",1.0,true,false,[],{}],"properties":{"myprop":"myvalue"},"geometry":null}']
        assert native_data in expected
        native_media_type = f.GetNativeMediaType()
        assert native_media_type == 'application/vnd.geo+json'
        ds = None
        if i == 1:
            gdal.Unlink('/vsimem/ogr_geojson_45.json')

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_45.json')
    lyr = ds.CreateLayer('test', options=[
        'NATIVE_DATA={ "type": "ignored", "bbox": [ 0, 0, 0, 0 ], "foo": "bar", "bar": "baz", "features": "ignored" }',
        'NATIVE_MEDIA_TYPE=application/vnd.geo+json'])
    f = ogr.Feature(lyr.GetLayerDefn())
    json_geom = """{ "type": "GeometryCollection", "foo_gc": "bar_gc", "geometries" : [
                        { "type": "Point", "foo_point": "bar_point", "coordinates": [0,1,2, 3] },
                        { "type": "LineString", "foo_linestring": "bar_linestring", "coordinates": [[0,1,2, 4]] },
                        { "type": "MultiPoint", "foo_multipoint": "bar_multipoint", "coordinates": [[0,1,2, 5]] },
                        { "type": "MultiLineString", "foo_multilinestring": "bar_multilinestring", "coordinates": [[[0,1,2, 6]]] },
                        { "type": "Polygon", "foo_polygon": "bar_polygon", "coordinates": [[[0,1,2, 7]]] },
                        { "type": "MultiPolygon", "foo_multipolygon": "bar_multipolygon", "coordinates": [[[[0,1,2, 8]]]] }
                        ] }"""
    f.SetNativeData('{ "type": "ignored", "bbox": "ignored", "properties" : "ignored", "foo_feature": "bar_feature", "geometry": %s }' % json_geom)
    f.SetNativeMediaType('application/vnd.geo+json')
    f.SetGeometry(ogr.CreateGeometryFromJson(json_geom))
    lyr.CreateFeature(f)
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_45.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_45.json')

    assert ('"bbox": [ 0, 1, 2, 0, 1, 2 ],' in data and \
       '"foo": "bar"' in data and '"bar": "baz"' in data and \
       '"foo_feature": "bar_feature"' in data and \
       '"foo_gc": "bar_gc"' in data and \
       '"foo_point": "bar_point"' in data and '3' in data and \
       '"foo_linestring": "bar_linestring"' in data and '4' in data and \
       '"foo_multipoint": "bar_multipoint"' in data and '5' in data and \
       '"foo_multilinestring": "bar_multilinestring"' in data and '6' in data and \
       '"foo_polygon": "bar_polygon"' in data and '7' in data and \
       '"foo_multipolygon": "bar_multipolygon"' in data and '8' in data)

    # Test native support with string id
    src_ds = gdal.OpenEx("""{
"type": "FeatureCollection",
"features": [
{ "type": "Feature",
  "id": "foobarbaz",
  "properties": {},
  "geometry": null
}
]
}
""", open_options=['NATIVE_DATA=YES'])
    gdal.VectorTranslate('/vsimem/out.json', src_ds, format='GeoJSON')

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"features": [
{ "type": "Feature", "id": "foobarbaz", "properties": { }, "geometry": null }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Test native support with numeric id
    src_ds = gdal.OpenEx("""{
"type": "FeatureCollection",
"features": [
{ "type": "Feature",
  "id": 1234657890123,
  "properties": {},
  "geometry": null
}
]
}
""", open_options=['NATIVE_DATA=YES'])
    gdal.VectorTranslate('/vsimem/out.json', src_ds, format='GeoJSON')

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"features": [
{ "type": "Feature", "id": 1234657890123, "properties": { }, "geometry": null }
]
}
"""
    assert json.loads(got) == json.loads(expected)

###############################################################################
# Test that writing JSon content as value of a string field is serialized as it


def test_ogr_geojson_46():

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_46.json')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('myprop'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['myprop'] = '{ "a": "b" }'
    lyr.CreateFeature(f)
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_46.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_46.json')

    assert '{ "myprop": { "a": "b" } }' in data

###############################################################################
# Test update support


def test_ogr_geojson_47():

    # ERROR 6: Update from inline definition not supported
    with gdaltest.error_handler():
        ds = ogr.Open('{"type": "FeatureCollection", "features":[]}', update=1)
    assert ds is None

    gdal.FileFromMemBuffer('/vsimem/ogr_geojson_47.json',
                           """{"type": "FeatureCollection", "foo": "bar",
    "features":[ { "type": "Feature", "bar": "baz", "properties": { "myprop": "myvalue" }, "geometry": null } ]}""")

    # Test read support
    ds = ogr.Open('/vsimem/ogr_geojson_47.json', update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    f.SetField("myprop", "another_value")
    lyr.SetFeature(f)
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_47.json', 'rb')
    if fp is not None:
        data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    else:
        data = None

    # we don't want crs if there's no in the source
    assert ('"foo": "bar"' in data and '"bar": "baz"' in data and \
       'crs' not in data and \
       '"myprop": "another_value"' in data)

    # Test append support
    ds = ogr.Open('/vsimem/ogr_geojson_47.json', update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    lyr.CreateFeature(f)
    if f.GetFID() != 1:
        f.DumpReadable()
        pytest.fail()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 3)'))
    lyr.CreateFeature(f)
    f = lyr.GetNextFeature()
    if f.GetFID() != 0:
        f.DumpReadable()
        pytest.fail()
    ds = None

    # Test append support
    ds = ogr.Open('/vsimem/ogr_geojson_47.json', update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(4 5)'))
    lyr.CreateFeature(f)
    f.SetField("myprop", "value_of_point_4_5")
    lyr.SetFeature(f)
    ds = None

    ds = ogr.Open('/vsimem/ogr_geojson_47.json')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 4
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_47.json', 'rb')
    if fp is not None:
        data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    else:
        data = None

    # we don't want crs if there's no in the source
    assert ('"foo": "bar"' in data and '"bar": "baz"' in data and \
       'crs' not in data and \
       '"myprop": "another_value"' in data and \
       '"myprop": "value_of_point_4_5"' in data and \
       'id' not in data)

    gdal.Unlink('/vsimem/ogr_geojson_47.json')

    # Test appending to empty features array
    gdal.FileFromMemBuffer('/vsimem/ogr_geojson_47.json', """{ "type": "FeatureCollection", "features": []}""")
    ds = ogr.Open('/vsimem/ogr_geojson_47.json', update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None
    ds = ogr.Open('/vsimem/ogr_geojson_47.json')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None

    # Test appending to array ending with non feature
    gdal.FileFromMemBuffer('/vsimem/ogr_geojson_47.json', """{ "type": "FeatureCollection", "features": [ null ]}""")
    ds = ogr.Open('/vsimem/ogr_geojson_47.json', update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None
    ds = ogr.Open('/vsimem/ogr_geojson_47.json')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None

    # Test appending to feature collection not ending with "features"
    gdal.FileFromMemBuffer('/vsimem/ogr_geojson_47.json', """{ "type": "FeatureCollection", "features": [], "something": "else"}""")
    ds = ogr.Open('/vsimem/ogr_geojson_47.json', update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None
    ds = ogr.Open('/vsimem/ogr_geojson_47.json')
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_47.json', 'rb')
    if fp is not None:
        data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    else:
        data = None

    assert 'something' in data

    with gdaltest.config_option('OGR_GEOJSON_REWRITE_IN_PLACE', 'YES'):
        # Test appending to feature collection with "bbox"
        gdal.FileFromMemBuffer('/vsimem/ogr_geojson_47.json', """{ "type": "FeatureCollection", "bbox": [0,0,0,0], "features": [ { "type": "Feature", "geometry": { "type": "Point", "coordinates": [0,0]} } ]}""")
        ds = ogr.Open('/vsimem/ogr_geojson_47.json', update=1)
        lyr = ds.GetLayer(0)
        f = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(f)
        ds = None
        ds = ogr.Open('/vsimem/ogr_geojson_47.json')
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2
        ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_47.json', 'rb')
    if fp is not None:
        data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    else:
        data = None

    assert 'bbox' in data

    gdal.Unlink('/vsimem/ogr_geojson_47.json')

###############################################################################
# Test update support with file that has a single feature not in a FeatureCollection


def test_ogr_geojson_48():

    gdal.FileFromMemBuffer('/vsimem/ogr_geojson_48.json',
                           """{ "type": "Feature", "bar": "baz", "bbox": [2,49,2,49], "properties": { "myprop": "myvalue" }, "geometry": {"type": "Point", "coordinates": [ 2, 49]} }""")

    # Test read support
    ds = ogr.Open('/vsimem/ogr_geojson_48.json', update=1)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    f.SetField("myprop", "another_value")
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (3 50)'))
    lyr.SetFeature(f)
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_48.json', 'rb')
    if fp is not None:
        data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
        gdal.VSIFCloseL(fp)
    else:
        data = None

    gdal.Unlink('/vsimem/ogr_geojson_48.json')

    # we don't want crs if there's no in the source
    assert ('"bar": "baz"' in data and \
       '"bbox": [ 3.0, 50.0, 3.0, 50.0 ]' in data and \
       'crs' not in data and \
       'FeatureCollection' not in data and \
       '"myprop": "another_value"' in data)

###############################################################################
# Test ARRAY_AS_STRING


def test_ogr_geojson_49():

    gdal.FileFromMemBuffer('/vsimem/ogr_geojson_49.json',
                           """{ "type": "Feature", "properties": { "foo": ["bar"] }, "geometry": null }""")

    # Test read support
    ds = gdal.OpenEx('/vsimem/ogr_geojson_49.json', open_options=['ARRAY_AS_STRING=YES'])
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
    f = lyr.GetNextFeature()
    if f['foo'] != '[ "bar" ]':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_geojson_49.json')

###############################################################################
# Test that we serialize floating point values with enough significant figures


def test_ogr_geojson_50():

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_50.json')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('val', ogr.OFTReal))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['val'] = 1.23456789012456
    lyr.CreateFeature(f)
    # To test smart rounding
    f = ogr.Feature(lyr.GetLayerDefn())
    f['val'] = 5268.813
    lyr.CreateFeature(f)
    f = None
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_50.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_50.json')

    assert '1.23456789012456' in data or '5268.813 ' in data

    # If SIGNIFICANT_FIGURES is explicitly specified, and COORDINATE_PRECISION not,
    # then it also applies to coordinates
    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_50.json')
    lyr = ds.CreateLayer('test', options=['SIGNIFICANT_FIGURES=17'])
    lyr.CreateField(ogr.FieldDefn('val', ogr.OFTReal))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (0.0000123456789012456 0)'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_50.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_50.json')

    assert '1.23456789012456' in data or '-5' in data

    # If SIGNIFICANT_FIGURES is explicitly specified, and COORDINATE_PRECISION too,
    # then SIGNIFICANT_FIGURES only applies to non-coordinates floating point values.
    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_50.json')
    lyr = ds.CreateLayer('test', options=['COORDINATE_PRECISION=15', 'SIGNIFICANT_FIGURES=17'])
    lyr.CreateField(ogr.FieldDefn('val', ogr.OFTReal))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['val'] = 1.23456789012456
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (0.0000123456789012456 0)'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_50.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_50.json')

    assert '0.00001234' in data and '1.23456789012456' in data

###############################################################################
# Test writing empty geometries


def test_ogr_geojson_51():

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_51.json')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 1
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT EMPTY'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 2
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING EMPTY'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 3
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON EMPTY'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 4
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOINT EMPTY'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 5
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING EMPTY'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 6
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON EMPTY'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 7
    f.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION EMPTY'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_51.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_51.json')

    assert '{ "id": 1 }, "geometry": null' in data

    assert '{ "id": 2 }, "geometry": { "type": "LineString", "coordinates": [ ] } }' in data

    assert '{ "id": 3 }, "geometry": { "type": "Polygon", "coordinates": [ ] } }' in data

    assert '{ "id": 4 }, "geometry": { "type": "MultiPoint", "coordinates": [ ] } }' in data

    assert '{ "id": 5 }, "geometry": { "type": "MultiLineString", "coordinates": [ ] } }' in data

    assert '{ "id": 6 }, "geometry": { "type": "MultiPolygon", "coordinates": [ ] } }' in data

    assert '{ "id": 7 }, "geometry": { "type": "GeometryCollection", "geometries": [ ] } }' in data

###############################################################################
# Test NULL type detection


def test_ogr_geojson_52():

    ds = ogr.Open('data/geojson/nullvalues.geojson')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayerByName('nullvalues')
    assert lyr is not None, 'Missing layer called nullvalues'

    fld = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld.GetNameRef() == 'int'
    assert fld.GetType() == ogr.OFTInteger
    fld = lyr.GetLayerDefn().GetFieldDefn(1)
    assert fld.GetNameRef() == 'string'
    assert fld.GetType() == ogr.OFTString
    fld = lyr.GetLayerDefn().GetFieldDefn(2)
    assert fld.GetNameRef() == 'double'
    assert fld.GetType() == ogr.OFTReal

###############################################################################
# Test that M is ignored (this is a test of OGRLayer::CreateFeature() actually)


def test_ogr_geojson_53():

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_53.json')
    lyr = ds.CreateLayer('test')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT ZM (1 2 3 4)'))
    lyr.CreateFeature(f)
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_53.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_53.json')

    assert '{ "type": "Point", "coordinates": [ 1.0, 2.0, 3.0 ] }' in data

###############################################################################
# Test NULL type detection when first value is null


def test_ogr_geojson_54():

    ds = ogr.Open("""{
   "type": "FeatureCollection",

  "features": [
      { "type": "Feature", "properties": { "int": null, "string": null, "double": null, "dt" : null, "boolean": null, "null": null }, "geometry": null },
      { "type": "Feature", "properties": { "int": 168, "string": "string", "double": 1.23, "dt" : "2016-05-18T12:34:56Z", "boolean": true }, "geometry": null }
  ]
}
""")
    lyr = ds.GetLayer(0)

    fld = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld.GetType() == ogr.OFTInteger
    fld = lyr.GetLayerDefn().GetFieldDefn(1)
    assert fld.GetType() == ogr.OFTString
    fld = lyr.GetLayerDefn().GetFieldDefn(2)
    assert fld.GetType() == ogr.OFTReal
    fld = lyr.GetLayerDefn().GetFieldDefn(3)
    assert fld.GetType() == ogr.OFTDateTime
    fld = lyr.GetLayerDefn().GetFieldDefn(4)
    assert fld.GetType() == ogr.OFTInteger
    assert fld.GetSubType() == ogr.OFSTBoolean
    assert fld.GetWidth() == 1
    fld = lyr.GetLayerDefn().GetFieldDefn(5)
    assert fld.GetType() == ogr.OFTString

###############################################################################
# Test RFC 7946


def read_file(filename):
    f = gdal.VSIFOpenL(filename, "rb")
    if f is None:
        return None
    content = gdal.VSIFReadL(1, 10000, f).decode('UTF-8')
    gdal.VSIFCloseL(f)
    return content


def test_ogr_geojson_55():

    # Basic test for standard bbox and coordinate truncation
    gdal.VectorTranslate('/vsimem/out.json', """{
   "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "id": 123, "properties": {}, "geometry": { "type": "Point", "coordinates": [2.123456789, 49] } },
      { "type": "Feature", "id": 124, "properties": {}, "geometry": { "type": "Point", "coordinates": [3, 50] } }
  ]
}""", options='-f GeoJSON -lco RFC7946=YES -lco WRITE_BBOX=YES -preserve_fid')

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ 2.1234568, 49.0000000, 3.0000000, 50.0000000 ],
"features": [
{ "type": "Feature", "id": 123, "properties": { }, "bbox": [ 2.1234568, 49.0, 2.1234568, 49.0 ], "geometry": { "type": "Point", "coordinates": [ 2.1234568, 49.0 ] } },
{ "type": "Feature", "id": 124, "properties": { }, "bbox": [ 3.0, 50.0, 3.0, 50.0 ], "geometry": { "type": "Point", "coordinates": [ 3.0, 50.0 ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Test polygon winding order
    gdal.VectorTranslate('/vsimem/out.json', """{
"type": "FeatureCollection",
"features": [
{ "type": "Feature", "geometry": { "type": "Polygon", "coordinates": [[[2,49],[3,49],[3,50],[2,50],[2,49]],[[2.1,49.1],[2.1,49.9],[2.9,49.9],[2.9,49.1],[2.1,49.1]]] } },
{ "type": "Feature", "geometry": { "type": "Polygon", "coordinates": [[[2,49],[2,50],[3,50],[3,49],[2,49]],[[2.1,49.1],[2.9,49.1],[2.9,49.9],[2.1,49.9],[2.1,49.1]]] } },
]
}
""", format='GeoJSON', layerCreationOptions=['RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ 2.0000000, 49.0000000, 3.0000000, 50.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 2.0, 49.0, 3.0, 50.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 2.0, 49.0 ], [ 3.0, 49.0 ], [ 3.0, 50.0 ], [ 2.0, 50.0 ], [ 2.0, 49.0 ] ], [ [ 2.1, 49.1 ], [ 2.1, 49.9 ], [ 2.9, 49.9 ], [ 2.9, 49.1 ], [ 2.1, 49.1 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 2.0, 49.0, 3.0, 50.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 2.0, 49.0 ], [ 3.0, 49.0 ], [ 3.0, 50.0 ], [ 2.0, 50.0 ], [ 2.0, 49.0 ] ], [ [ 2.1, 49.1 ], [ 2.1, 49.9 ], [ 2.9, 49.9 ], [ 2.9, 49.1 ], [ 2.1, 49.1 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Test foreign member
    src_ds = gdal.OpenEx("""{
"type": "FeatureCollection",
"coordinates": "should not be found in output",
"geometries": "should not be found in output",
"geometry": "should not be found in output",
"properties": "should not be found in output",
"valid": "should be in output",
"crs": "should not be found in output",
"bbox": [0,0,0,0],
"features": [
{ "type": "Feature",
  "id": ["not expected as child of features"],
  "coordinates": "should not be found in output",
  "geometries": "should not be found in output",
  "features": "should not be found in output",
  "valid": "should be in output",
  "properties": { "foo": "bar" },
  "geometry": {
    "type": "Point",
    "bbox": [0,0,0,0],
    "geometry": "should not be found in output",
    "properties": "should not be found in output",
    "features": "should not be found in output",
    "valid": "should be in output",
    "coordinates": [2,49]
  }
}
]
}
""", open_options=['NATIVE_DATA=YES'])
    gdal.VectorTranslate('/vsimem/out.json', src_ds, format='GeoJSON', layerCreationOptions=['RFC7946=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"valid": "should be in output",
"bbox": [ 2.0000000, 49.0000000, 2.0000000, 49.0000000 ],
"features": [
{ "type": "Feature",
  "valid": "should be in output",
  "properties": { "id": [ "not expected as child of features" ], "foo": "bar" },
  "geometry": { "type": "Point", "coordinates": [ 2.0, 49.0 ], "valid": "should be in output" } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

###############################################################################
# Test RFC 7946 (that require geos)


def test_ogr_geojson_56():

    if not ogrtest.have_geos():
        pytest.skip()

    # Test offsetting longitudes beyond antimeridian
    gdal.VectorTranslate('/vsimem/out.json', """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "geometry": { "type": "Point", "coordinates": [182, 49] } },
      { "type": "Feature", "geometry": { "type": "Point", "coordinates": [-183, 50] } },
      { "type": "Feature", "geometry": { "type": "LineString", "coordinates": [[-183, 51],[-182, 48]] } },
      { "type": "Feature", "geometry": { "type": "LineString", "coordinates": [[182, 52],[183, 47]] } },
      { "type": "Feature", "geometry": { "type": "Polygon", "coordinates": [[[-183, 51],[-183, 48],[-182, 48],[-183, 48],[-183, 51]]] } },
      { "type": "Feature", "geometry": { "type": "Polygon", "coordinates": [[[183, 51],[183, 48],[182, 48],[183, 48],[183, 51]]] } },
  ]
}""", format='GeoJSON', layerCreationOptions=['RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ -178.0000000, 47.0000000, 178.0000000, 52.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ -178.0, 49.0, -178.0, 49.0 ], "geometry": { "type": "Point", "coordinates": [ -178.0, 49.0 ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 177.0, 50.0, 177.0, 50.0 ], "geometry": { "type": "Point", "coordinates": [ 177.0, 50.0 ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 177.0, 48.0, 178.0, 51.0 ], "geometry": { "type": "LineString", "coordinates": [ [ 177.0, 51.0 ], [ 178.0, 48.0 ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ -178.0, 47.0, -177.0, 52.0 ], "geometry": { "type": "LineString", "coordinates": [ [ -178.0, 52.0 ], [ -177.0, 47.0 ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 177.0, 48.0, 178.0, 51.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 177.0, 51.0 ], [ 177.0, 48.0 ], [ 178.0, 48.0 ], [ 177.0, 48.0 ], [ 177.0, 51.0 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ -178.0, 48.0, -177.0, 51.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ -177.0, 51.0 ], [ -177.0, 48.0 ], [ -178.0, 48.0 ], [ -177.0, 48.0 ], [ -177.0, 51.0 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Test geometries across the antimeridian
    gdal.VectorTranslate('/vsimem/out.json', """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "geometry": { "type": "LineString", "coordinates": [[179, 51],[-179, 48]] } },
      { "type": "Feature", "geometry": { "type": "LineString", "coordinates": [[-179, 52],[179, 47]] } },
      { "type": "Feature", "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 179.0, 51.0 ], [ 180.0, 49.5 ] ], [ [ -180.0, 49.5 ], [ -179.0, 48.0 ] ] ] } },
      { "type": "Feature", "geometry": { "type": "Polygon", "coordinates": [[[177, 51],[-175, 51],[-175, 48],[177, 48],[177, 51]]] } },
      { "type": "Feature", "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 177.0, 51.0 ], [ 177.0, 48.0 ], [ 180.0, 48.0 ], [ 180.0, 51.0 ], [ 177.0, 51.0 ] ] ], [ [ [ -180.0, 51.0 ], [ -180.0, 48.0 ], [ -175.0, 48.0 ], [ -175.0, 51.0 ], [ -180.0, 51.0 ] ] ] ] } }
  ]
}""", format='GeoJSON', layerCreationOptions=['RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ 177.0000000, 47.0000000, -175.0000000, 52.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 179.0, 48.0, -179.0, 51.0 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 179.0, 51.0 ], [ 180.0, 49.5 ] ], [ [ -180.0, 49.5 ], [ -179.0, 48.0 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 179.0, 47.0, -179.0, 52.0 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ -179.0, 52.0 ], [ -180.0, 49.5 ] ], [ [ 180.0, 49.5 ], [ 179.0, 47.0 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 179.0, 48.0, -179.0, 51.0 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 179.0, 51.0 ], [ 180.0, 49.5 ] ], [ [ -180.0, 49.5 ], [ -179.0, 48.0 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 177.0, 48.0, -175.0, 51.0 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 177.0, 51.0 ], [ 177.0, 48.0 ], [ 180.0, 48.0 ], [ 180.0, 51.0 ], [ 177.0, 51.0 ] ] ], [ [ [ -180.0, 51.0 ], [ -180.0, 48.0 ], [ -175.0, 48.0 ], [ -175.0, 51.0 ], [ -180.0, 51.0 ] ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 177.0, 48.0, -175.0, 51.0 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 177.0, 51.0 ], [ 177.0, 48.0 ], [ 180.0, 48.0 ], [ 180.0, 51.0 ], [ 177.0, 51.0 ] ] ], [ [ [ -180.0, 51.0 ], [ -180.0, 48.0 ], [ -175.0, 48.0 ], [ -175.0, 51.0 ], [ -180.0, 51.0 ] ] ] ] } }
]
}
"""

    j_got = json.loads(got)
    j_expected = json.loads(expected)
    assert j_got["bbox"] == j_expected["bbox"]
    assert len(j_expected["features"]) == 5
    assert ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(json.dumps(j_got["features"][0]["geometry"])), ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][0]["geometry"]))) == 0
    assert ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(json.dumps(j_got["features"][1]["geometry"])), ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][1]["geometry"]))) == 0
    assert ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(json.dumps(j_got["features"][2]["geometry"])), ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][2]["geometry"]))) == 0
    assert ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(json.dumps(j_got["features"][3]["geometry"])), ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][3]["geometry"]))) == 0
    assert ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(json.dumps(j_got["features"][4]["geometry"])), ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][4]["geometry"]))) == 0


    # Test polygon geometry that covers the whole world (#2833)
    gdal.VectorTranslate('/vsimem/out.json', """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "geometry": {"type":"Polygon","coordinates":[[[-180,-90.0],[180,-90.0],[180,90.0],[-180,90.0],[-180,-90.0]]]} }
  ]
}""", format='GeoJSON', layerCreationOptions=['RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ -180.0000000, -90.0000000, 180.0000000, 90.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ -180.0, -90.0, 180.0, 90.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ -180.0, -90.0 ], [ 180.0, -90.0 ], [ 180.0, 90.0 ], [ -180.0, 90.0 ], [ -180.0, -90.0 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)


###############################################################################
# Test RFC 7946 and reprojection


def test_ogr_geojson_57():

    if not ogrtest.have_geos():
        pytest.skip()

    # Standard case: EPSG:32662: WGS 84 / Plate Carre
    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=eqc +lat_ts=0 +lat_0=0 +lon_0=0 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2000000 2000000,2000000 -2000000,-2000000 -2000000,-2000000 2000000,2000000 2000000))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format='GeoJSON', layerCreationOptions=['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ -17.9663057, -17.9663057, 17.9663057, 17.9663057 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ -17.9663057, -17.9663057, 17.9663057, 17.9663057 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 17.9663057, 17.9663057 ], [ -17.9663057, 17.9663057 ], [ -17.9663057, -17.9663057 ], [ 17.9663057, -17.9663057 ], [ 17.9663057, 17.9663057 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Polar case: EPSG:3995: WGS 84 / Arctic Polar Stereographic
    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=stere +lat_0=90 +lat_ts=71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2000000 2000000,2000000 -2000000,-2000000 -2000000,-2000000 2000000,2000000 2000000))'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((-2000000 -2000000,-1000000 -2000000,-1000000 2000000,-2000000 2000000,-2000000 -2000000))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format='GeoJSON', layerCreationOptions=['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ -180.0000000, 64.3861643, 180.0000000, 90.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ -180.0, 64.3861643, 180.0, 90.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 135.0, 64.3861643 ], [ 180.0, 71.7425119 ], [ 180.0, 90.0 ], [ -180.0, 90.0 ], [ -180.0, 71.7425119 ], [ -135.0, 64.3861643 ], [ -45.0, 64.3861643 ], [ 45.0, 64.3861643 ], [ 135.0, 64.3861643 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ -153.4349488, 64.3861643, -26.5650512, 69.6286694 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ -45.0, 64.3861643 ], [ -26.5650512, 69.6286694 ], [ -153.4349488, 69.6286694 ], [ -135.0, 64.3861643 ], [ -45.0, 64.3861643 ] ] ] } }
]
}
"""

    j_got = json.loads(got)
    j_expected = json.loads(expected)
    assert j_got["bbox"] == j_expected["bbox"]
    assert len(j_expected["features"]) == 2
    assert ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(json.dumps(j_got["features"][0]["geometry"])), ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][0]["geometry"]))) == 0
    assert ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(json.dumps(j_got["features"][1]["geometry"])), ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][1]["geometry"]))) == 0


    # Polar case: slice of spherical cap (not intersecting antimeridian, west hemisphere)
    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=stere +lat_0=90 +lat_ts=71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((-2000000 2000000,0 0,-2000000 -2000000,-2000000 2000000))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format='GeoJSON', layerCreationOptions=['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ -135.0000000, 64.3861643, -45.0000000, 90.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ -135.0, 64.3861643, -45.0, 90.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ -135.0, 64.3861643 ], [ -45.0, 64.3861643 ], [ -45.0, 90.0 ], [ -135.0, 90.0 ], [ -135.0, 64.3861643 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Polar case: slice of spherical cap (not intersecting antimeridian, east hemisphere)
    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=stere +lat_0=90 +lat_ts=71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((2000000 2000000,0 0,2000000 -2000000,2000000 2000000)))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format='GeoJSON', layerCreationOptions=['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ 45.0000000, 64.3861643, 135.0000000, 90.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 45.0, 64.3861643, 135.0, 90.0 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 135.0, 64.3861643 ], [ 135.0, 90.0 ], [ 45.0, 90.0 ], [ 45.0, 64.3861643 ], [ 135.0, 64.3861643 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

    # Polar case: slice of spherical cap crossing the antimeridian
    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=stere +lat_0=90 +lat_ts=71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((100000 100000,-100000 100000,0 0,100000 100000))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format='GeoJSON', layerCreationOptions=['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ 135.0000000, 88.6984598, -135.0000000, 90.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 135.0, 88.6984598, -135.0, 90.0 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 180.0, 89.0796531 ], [ 180.0, 90.0 ], [ 135.0, 88.6984598 ], [ 180.0, 89.0796531 ] ] ], [ [ [ -180.0, 90.0 ], [ -180.0, 89.0796531 ], [ -135.0, 88.6984598 ] ] ] ] } }
]
}
"""
    expected_geos_overlay_ng = """{
"type": "FeatureCollection",
"bbox": [ 135.0000000, 88.6984598, -135.0000000, 90.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 135.0, 88.6984598, -135.0, 90.0 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ -135.0, 88.6984598 ], [ -180.0, 90.0 ], [ -180.0, 89.0796531 ], [ -135.0, 88.6984598 ] ] ], [ [ [ 180.0, 90.0 ], [ 135.0, 88.6984598 ], [ 180.0, 89.0796531 ], [ 180.0, 90.0 ] ] ] ] } }
]
}"""
    assert json.loads(got) == json.loads(expected) or json.loads(got) == json.loads(expected_geos_overlay_ng), got

    # Polar case: EPSG:3031: WGS 84 / Antarctic Polar Stereographic
    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=stere +lat_0=-90 +lat_ts=-71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((2000000 2000000,2000000 -2000000,-2000000 -2000000,-2000000 2000000,2000000 2000000)))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format='GeoJSON', layerCreationOptions=['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ -180.0000000, -90.0000000, 180.0000000, -64.3861643 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ -180.0, -90.0, 180.0, -64.3861643 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 45.0, -64.3861643 ], [ -45.0, -64.3861643 ], [ -135.0, -64.3861643 ], [ -180.0, -71.7425119 ], [ -180.0, -90.0 ], [ 180.0, -90.0 ], [ 180.0, -71.7425119 ], [ 135.0, -64.3861643 ], [ 45.0, -64.3861643 ] ] ] } }
]
}
"""
    j_got = json.loads(got)
    j_expected = json.loads(expected)
    assert j_got["bbox"] == j_expected["bbox"]
    assert len(j_expected["features"]) == 1
    assert ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(json.dumps(j_got["features"][0]["geometry"])), ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][0]["geometry"]))) == 0

    # Antimeridian case: EPSG:32660: WGS 84 / UTM zone 60N with polygon and line crossing
    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=utm +zone=60 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((670000 4000000,850000 4000000,850000 4100000,670000 4100000,670000 4000000))'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING((670000 4000000,850000 4100000))'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(670000 0,850000 0)'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format='GeoJSON', layerCreationOptions=['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ 178.5275649, 0.0000000, -179.0681936, 37.0308258 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 178.8892102, 36.0816324, -179.0681936, 37.0308258 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 180.0, 36.1071354 ], [ 180.0, 36.1071354 ], [ 180.0, 37.0082839 ], [ 180.0, 37.0082839 ], [ 178.9112998, 37.0308258 ], [ 178.8892102, 36.1298163 ], [ 180.0, 36.1071354 ] ] ], [ [ [ -180.0, 37.0082839 ], [ -180.0, 36.1071354 ], [ -180.0, 36.1071354 ], [ -179.1135277, 36.0816324 ], [ -179.0681936, 36.9810434 ], [ -180.0, 37.0082839 ] ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 178.8892102, 36.1298163, -179.0681936, 36.9810434 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 178.8892102, 36.1298163 ], [ 180.0, 36.5995612 ] ], [ [ -180.0, 36.5995612 ], [ -179.0681936, 36.9810434 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 178.5275649, 0.0, -179.8562277, 0.0 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 178.5275649, 0.0 ], [ 180.0, 0.0 ] ], [ [ -180.0, 0.0 ], [ -179.8562277, 0.0 ] ] ] } }
]
}
"""
    j_got = json.loads(got)
    j_expected = json.loads(expected)
    assert j_got["bbox"] == j_expected["bbox"]
    assert len(j_expected["features"]) == 3
    assert ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(json.dumps(j_got["features"][0]["geometry"])), ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][0]["geometry"]))) == 0
    assert ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(json.dumps(j_got["features"][1]["geometry"])), ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][1]["geometry"]))) == 0
    assert ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(json.dumps(j_got["features"][2]["geometry"])), ogr.CreateGeometryFromJson(json.dumps(j_expected["features"][2]["geometry"]))) == 0

    # Antimeridian case: EPSG:32660: WGS 84 / UTM zone 60N wit polygon on west of antimeridian
    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=utm +zone=60 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs=sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((670000 4000000,700000 4000000,700000 4100000,670000 4100000,670000 4000000))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format='GeoJSON', layerCreationOptions=['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ 178.8892102, 36.1240958, 179.2483693, 37.0308258 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 178.8892102, 36.1240958, 179.2483693, 37.0308258 ], "geometry": { "type": "Polygon", "coordinates": [ [ [ 178.8892102, 36.1298163 ], [ 179.2223914, 36.1240958 ], [ 179.2483693, 37.0249155 ], [ 178.9112998, 37.0308258 ], [ 178.8892102, 36.1298163 ] ] ] } }
]
}
"""
    assert json.loads(got) == json.loads(expected)

###############################################################################
# Test using the name member of FeatureCollection


def test_ogr_geojson_58():

    ds = ogr.Open('{ "type": "FeatureCollection", "name": "layer_name", "features": []}')
    assert ds is not None, 'Failed to open datasource'

    lyr = ds.GetLayerByName('layer_name')
    assert lyr is not None, 'Missing layer called layer_name'
    ds = None

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_58.json')
    lyr = ds.CreateLayer('foo')
    ds = None
    ds = ogr.Open('/vsimem/ogr_geojson_58.json')
    assert ds.GetLayerByName('foo') is not None, 'Missing layer called foo'
    ds = None
    gdal.Unlink('/vsimem/ogr_geojson_58.json')

###############################################################################
# Test using the description member of FeatureCollection


def test_ogr_geojson_59():

    ds = ogr.Open('{ "type": "FeatureCollection", "description": "my_description", "features": []}')
    assert ds is not None, 'Failed to open datasource'

    lyr = ds.GetLayer(0)
    assert lyr.GetMetadataItem('DESCRIPTION') == 'my_description', \
        'Did not get DESCRIPTION'
    ds = None

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_59.json')
    lyr = ds.CreateLayer('foo', options=['DESCRIPTION=my desc'])
    ds = None
    ds = ogr.Open('/vsimem/ogr_geojson_59.json')
    lyr = ds.GetLayerByName('foo')
    assert lyr.GetMetadataItem('DESCRIPTION') == 'my desc', 'Did not get DESCRIPTION'
    ds = None
    gdal.Unlink('/vsimem/ogr_geojson_59.json')

###############################################################################
# Test null vs unset field


def test_ogr_geojson_60():

    ds = gdal.OpenEx("""{ "type": "FeatureCollection", "features": [
{ "type": "Feature", "properties" : { "foo" : "bar" } },
{ "type": "Feature", "properties" : { "foo": null } },
{ "type": "Feature", "properties" : {  } } ] }""")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['foo'] != 'bar':
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if not f.IsFieldNull('foo'):
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.IsFieldSet('foo'):
        f.DumpReadable()
        pytest.fail()

    # Test writing side
    gdal.VectorTranslate('/vsimem/ogr_geojson_60.json', ds, format='GeoJSON')

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_60.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_60.json')
    assert ('"properties": { "foo": "bar" }' in data and \
       '"properties": { "foo": null }' in data and \
       '"properties": { }' in data)


###############################################################################
# Test corner cases

def test_ogr_geojson_61():

    # Invalid JSon
    gdal.FileFromMemBuffer('/vsimem/ogr_geojson_61.json',
                           """{ "type": "FeatureCollection", "features": [""")
    with gdaltest.error_handler():
        ds = gdal.OpenEx('/vsimem/ogr_geojson_61.json')
    assert ds is None
    gdal.Unlink('/vsimem/ogr_geojson_61.json')

    # Invalid single geometry
    with gdaltest.error_handler():
        ds = gdal.OpenEx("""{ "type": "Point", "x" : { "coordinates" : null } } """)
    assert ds is None

    # Empty property name
    gdal.FileFromMemBuffer('/vsimem/ogr_geojson_61.json',
                           """{ "type": "FeatureCollection", "features": [ { "type": "Feature", "properties": {"": 1}, "geometry": null }] }""")
    ds = gdal.OpenEx('/vsimem/ogr_geojson_61.json')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetField("") == 1
    ds = None
    gdal.Unlink('/vsimem/ogr_geojson_61.json')

###############################################################################
# Test crs object


def test_ogr_geojson_62():

    # crs type=name tests
    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"name" }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"name", "properties":null }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"name", "properties":1 }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"name", "properties":{"name":null} }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"name", "properties":{"name":1} }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"name", "properties":{"name":"x"} }, "features":[] }""")

    ds = gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"name", "properties":{"name": "urn:ogc:def:crs:EPSG::32631"} }, "features":[] }""")
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == '32631'
    assert srs.GetDataAxisToSRSAxisMapping() == [1, 2]


    # See https://github.com/OSGeo/gdal/issues/2035
    ds = gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"name", "properties":{"name": "urn:ogc:def:crs:OGC:1.3:CRS84"} }, "features":[] }""")
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == '4326'
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]

    # crs type=EPSG (not even documented in GJ2008 spec!) tests. Just for coverage completeness
    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"EPSG" }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"EPSG", "properties":null }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"EPSG", "properties":1 }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"EPSG", "properties":{"code":null} }, "features":[] }""")

    with gdaltest.error_handler():
        gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"EPSG", "properties":{"code":1} }, "features":[] }""")

    with gdaltest.error_handler():
        gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"EPSG", "properties":{"code":"x"} }, "features":[] }""")

    ds = gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"EPSG", "properties":{"code": 32631} }, "features":[] }""")
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.ExportToWkt().find('32631') >= 0

    # crs type=link tests
    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"link" }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"link", "properties":null }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"link", "properties":1 }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"link", "properties":{"href":null} }, "features":[] }""")

    with gdaltest.error_handler():
        gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"link", "properties":{"href":1} }, "features":[] }""")

    with gdaltest.error_handler():
        gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"link", "properties":{"href": "1"} }, "features":[] }""")

    # crs type=OGC (not even documented in GJ2008 spec!) tests. Just for coverage completeness
    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"OGC" }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"OGC", "properties":null }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"OGC", "properties":1 }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"OGC", "properties":{"urn":null} }, "features":[] }""")

    with gdaltest.error_handler():
        gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"OGC", "properties":{"urn":1} }, "features":[] }""")

    with gdaltest.error_handler():
        gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"OGC", "properties":{"urn":"x"} }, "features":[] }""")

    ds = gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"OGC", "properties":{"urn": "urn:ogc:def:crs:EPSG::32631"} }, "features":[] }""")
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs.ExportToWkt().find('32631') >= 0

###############################################################################
# Extensive test of field type promotion


def test_ogr_geojson_63():

    ds_ref = ogr.Open('data/geojson/test_type_promotion_ref.json')
    lyr_ref = ds_ref.GetLayer(0)
    ds = ogr.Open('data/geojson/test_type_promotion.json')
    lyr = ds.GetLayer(0)
    return ogrtest.compare_layers(lyr, lyr_ref)

###############################################################################
# Test exporting XYM / XYZM (#6935)


def test_ogr_geojson_64():

    g = ogr.CreateGeometryFromWkt('POINT ZM(1 2 3 4)')
    assert (ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(g.ExportToJson()),
                                      ogr.CreateGeometryFromWkt('POINT Z(1 2 3)')) == 0)

    g = ogr.CreateGeometryFromWkt('POINT M(1 2 3)')
    assert (ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(g.ExportToJson()),
                                      ogr.CreateGeometryFromWkt('POINT (1 2)')) == 0)

    g = ogr.CreateGeometryFromWkt('LINESTRING ZM(1 2 3 4,5 6 7 8)')
    assert (ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(g.ExportToJson()),
                                      ogr.CreateGeometryFromWkt('LINESTRING Z(1 2 3,5 6 7)')) == 0)

    g = ogr.CreateGeometryFromWkt('LINESTRING M(1 2 3,4 5 6)')
    assert (ogrtest.check_feature_geometry(ogr.CreateGeometryFromJson(g.ExportToJson()),
                                      ogr.CreateGeometryFromWkt('LINESTRING (1 2,4 5)')) == 0)

###############################################################################
# Test feature geometry CRS when CRS set on the FeatureCollection
# See https://github.com/r-spatial/sf/issues/449#issuecomment-319369945


def test_ogr_geojson_65():

    ds = ogr.Open("""{
"type": "FeatureCollection",
"crs": { "type": "name", "properties": { "name": "urn:ogc:def:crs:EPSG::32631" } },
"features": [{
"type": "Feature",
"geometry": {
"type": "Point",
"coordinates": [500000,4500000]},
"properties": {
}}]}""")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    srs = f.GetGeometryRef().GetSpatialReference()
    pcs = int(srs.GetAuthorityCode('PROJCS'))
    assert pcs == 32631, 'Spatial reference for individual geometry was not valid'

###############################################################################
# Test features with properties not being a dictionary


def test_ogr_geojson_66():

    ds = ogr.Open("""{
"type": "FeatureCollection",
"features": [
{
    "type": "Feature",
    "geometry": null,
    "properties": null
},
{
    "type": "Feature",
    "geometry": null,
    "properties": []
}
]}""")
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 0


###############################################################################
# Test reading GeoJSON files starting with {"features":[{"geometry":.... (#7198)

def test_ogr_geojson_67():

    ds = ogr.Open('data/geojson/grenada.geojson')
    assert ds is not None
    assert ds.GetDriver().GetName() == 'GeoJSON'
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1

###############################################################################


def test_ogr_geojson_id_field_and_id_type():

    gdal.VectorTranslate('/vsimem/out.json', 'data/poly.shp', options='-f GeoJSON -lco ID_TYPE=String -preserve_fid -limit 1 -fid 2')
    got = read_file('/vsimem/out.json')
    assert '"id": "2", "properties": { "AREA": 261752.781, "EAS_ID": 171, "PRFEDEA": "35043414" }' in got

    gdal.VectorTranslate('/vsimem/out.json', 'data/poly.shp', options='-f GeoJSON -lco ID_TYPE=Integer -preserve_fid -limit 1 -fid 2')
    got = read_file('/vsimem/out.json')
    assert '"id": 2, "properties": { "AREA": 261752.781, "EAS_ID": 171, "PRFEDEA": "35043414" }' in got

    gdal.VectorTranslate('/vsimem/out.json', 'data/poly.shp', format='GeoJSON', layerCreationOptions=['ID_FIELD=EAS_ID'], limit=1)
    got = read_file('/vsimem/out.json')
    assert '"id": 168, "properties": { "AREA": 215229.266, "PRFEDEA": "35043411" }' in got

    src_ds = gdal.OpenEx('/vsimem/out.json', open_options=['NATIVE_DATA=YES'])
    gdal.VectorTranslate('/vsimem/out2.json', src_ds, format='GeoJSON')
    src_ds = None
    got = read_file('/vsimem/out2.json')
    gdal.Unlink('/vsimem/out2.json')
    assert '"id": 168, "properties": { "AREA": 215229.266, "PRFEDEA": "35043411" }' in got

    src_ds = gdal.OpenEx('/vsimem/out.json', open_options=['NATIVE_DATA=YES'])
    gdal.VectorTranslate('/vsimem/out2.json', src_ds, format='GeoJSON', layerCreationOptions=['ID_TYPE=String'])
    src_ds = None
    got = read_file('/vsimem/out2.json')
    gdal.Unlink('/vsimem/out2.json')
    assert '"id": "168", "properties": { "AREA": 215229.266, "PRFEDEA": "35043411" }' in got

    src_ds = gdal.OpenEx('/vsimem/out.json', open_options=['NATIVE_DATA=YES'])
    gdal.VectorTranslate('/vsimem/out2.json', src_ds, format='GeoJSON', layerCreationOptions=['ID_TYPE=Integer'])
    src_ds = None
    got = read_file('/vsimem/out2.json')
    gdal.Unlink('/vsimem/out2.json')
    assert '"id": 168, "properties": { "AREA": 215229.266, "PRFEDEA": "35043411" }' in got

    gdal.Unlink('/vsimem/out.json')

    gdal.VectorTranslate('/vsimem/out.json', 'data/poly.shp', format='GeoJSON', layerCreationOptions=['ID_FIELD=EAS_ID', 'ID_TYPE=String'], limit=1)
    got = read_file('/vsimem/out.json')
    assert '"id": "168", "properties": { "AREA": 215229.266, "PRFEDEA": "35043411" }' in got

    src_ds = gdal.OpenEx('/vsimem/out.json', open_options=['NATIVE_DATA=YES'])
    gdal.VectorTranslate('/vsimem/out2.json', src_ds, format='GeoJSON')
    src_ds = None
    got = read_file('/vsimem/out2.json')
    gdal.Unlink('/vsimem/out2.json')
    assert '"id": "168", "properties": { "AREA": 215229.266, "PRFEDEA": "35043411" }' in got

    src_ds = gdal.OpenEx('/vsimem/out.json', open_options=['NATIVE_DATA=YES'])
    gdal.VectorTranslate('/vsimem/out2.json', src_ds, format='GeoJSON', layerCreationOptions=['ID_TYPE=String'])
    src_ds = None
    got = read_file('/vsimem/out2.json')
    gdal.Unlink('/vsimem/out2.json')
    assert '"id": "168", "properties": { "AREA": 215229.266, "PRFEDEA": "35043411" }' in got

    src_ds = gdal.OpenEx('/vsimem/out.json', open_options=['NATIVE_DATA=YES'])
    gdal.VectorTranslate('/vsimem/out2.json', src_ds, format='GeoJSON', layerCreationOptions=['ID_TYPE=Integer'])
    src_ds = None
    got = read_file('/vsimem/out2.json')
    gdal.Unlink('/vsimem/out2.json')
    assert '"id": 168, "properties": { "AREA": 215229.266, "PRFEDEA": "35043411" }' in got

    gdal.Unlink('/vsimem/out.json')

    gdal.VectorTranslate('/vsimem/out.json', 'data/poly.shp', format='GeoJSON', layerCreationOptions=['ID_FIELD=PRFEDEA'], limit=1)
    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    assert '"id": "35043411", "properties": { "AREA": 215229.266, "EAS_ID": 168 }' in got

    gdal.VectorTranslate('/vsimem/out.json', 'data/poly.shp', format='GeoJSON', layerCreationOptions=['ID_FIELD=PRFEDEA', 'ID_TYPE=Integer'], limit=1)
    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    assert '"id": 35043411, "properties": { "AREA": 215229.266, "EAS_ID": 168 }' in got

    gdal.VectorTranslate('/vsimem/out.json', 'data/poly.shp', format='GeoJSON', layerCreationOptions=['ID_GENERATE=YES'], limit=1)
    got = read_file('/vsimem/out.json')
    assert '"id": 0, "properties": { "AREA": 215229.266, "EAS_ID": 168, "PRFEDEA": "35043411" }' in got

    gdal.VectorTranslate('/vsimem/out.json', 'data/poly.shp', format='GeoJSON', layerCreationOptions=['ID_GENERATE=YES', 'ID_TYPE=Integer'], limit=1)
    got = read_file('/vsimem/out.json')
    assert '"id": 0, "properties": { "AREA": 215229.266, "EAS_ID": 168, "PRFEDEA": "35043411" }' in got

    gdal.VectorTranslate('/vsimem/out.json', 'data/poly.shp', format='GeoJSON', layerCreationOptions=['ID_GENERATE=YES', 'ID_TYPE=String'], limit=1)
    got = read_file('/vsimem/out.json')
    assert '"id": "0", "properties": { "AREA": 215229.266, "EAS_ID": 168, "PRFEDEA": "35043411" }' in got

###############################################################################


def test_ogr_geojson_geom_export_failure():

    g = ogr.CreateGeometryFromWkt('POINT EMPTY')
    geojson = g.ExportToJson()
    assert geojson is None

    g = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(TIN EMPTY)')
    geojson = json.loads(g.ExportToJson())
    assert geojson == {"type": "GeometryCollection", "geometries": None}

    g = ogr.Geometry(ogr.wkbLineString)
    g.AddPoint_2D(float('nan'), 0)
    with gdaltest.error_handler():
        geojson = g.ExportToJson()
    assert geojson is None

    g = ogr.Geometry(ogr.wkbPolygon)
    lr = ogr.Geometry(ogr.wkbLinearRing)
    lr.AddPoint_2D(0, 0)
    lr.AddPoint_2D(0, 1)
    lr.AddPoint_2D(1, 1)
    lr.AddPoint_2D(0, 0)
    g.AddGeometry(lr)
    lr = ogr.Geometry(ogr.wkbLinearRing)
    lr.AddPoint_2D(0, 0)
    lr.AddPoint_2D(float('nan'), 1)
    lr.AddPoint_2D(1, 1)
    lr.AddPoint_2D(0, 0)
    g.AddGeometry(lr)
    with gdaltest.error_handler():
        geojson = g.ExportToJson()
    assert geojson is None

###############################################################################


def test_ogr_geojson_starting_with_crs():

    ds = ogr.Open("""{
"crs": { "type": "name", "properties": { "name": "urn:ogc:def:crs:EPSG::32631" } },
"type": "FeatureCollection",
"features": [{
"type": "Feature",
"geometry": {
"type": "Point",
"coordinates": [500000,4500000]},
"properties": {
}}]}""")
    assert ds is not None

###############################################################################
# Test we properly flush the file in SyncToDisk() in append situations


def test_ogr_geojson_append_flush():

    tmpfilename = 'tmp/ogr_geojson_append_flush.json'
    f = gdal.VSIFOpenL(tmpfilename, 'wb')
    content = """{
"type": "FeatureCollection",
"features": [
{ "type": "Feature", "properties": { "x": 1, "y": 2, "z": 3, "w": 4 }, "geometry": { "type": "Point", "coordinates": [ 0, 0 ] } } ] }"""
    gdal.VSIFWriteL(content, 1, len(content), f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open(tmpfilename, update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f['x'] = 10
    lyr.CreateFeature(f)
    lyr.SyncToDisk()

    ds2 = ogr.Open(tmpfilename, update=1)
    lyr = ds2.GetLayer(0)
    lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    assert f is not None and f['x'] == 10

    ds = None
    ds2 = None
    gdal.Unlink(tmpfilename)


###############################################################################


def test_ogr_geojson_empty_geometrycollection():

    g = ogr.CreateGeometryFromJson('{"type": "GeometryCollection", "geometries": []}')
    assert g.ExportToWkt() == 'GEOMETRYCOLLECTION EMPTY'


###############################################################################


def test_ogr_geojson_read_fields_with_different_case():

    ds = ogr.Open("""{
"type": "FeatureCollection",
"features": [
{ "type": "Feature", "id": "my_id", "geometry": null, "properties":
                                { "ID": "MY_ID", "x": "foo", "X": "FOO"} }
]}""")

    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetField(0) != 'my_id' or f.GetField(1) != 'MY_ID' or f.GetField(2) != 'foo' or f.GetField(3) != 'FOO':
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/1068


def test_ogr_geojson_clip_geometries_rfc7946():

    if not ogrtest.have_geos():
        pytest.skip()

    tmpfilename = '/vsimem/out.json'
    gdal.VectorTranslate(tmpfilename, """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "geometry": {"type":"Polygon","coordinates":[[[-220,-20],[-220,30],[16,30],[16,-20],[-220,-20]]]} },
      { "type": "Feature", "geometry": {"type":"Polygon","coordinates":[[[220,40],[220,70],[-16,70],[-16,40],[220,40]]]} },
      { "type": "Feature", "geometry": {"type":"Polygon","coordinates":[[[170,-40],[170,-70],[-16,70],[-16,-40],[170,-40]]]} }
  ]
}""", options='-f GeoJSON -lco RFC7946=YES')

    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)

    f = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOLYGON (((-180 30,-180 -20,16 -20,16 30,-180 30)),((140 -20,180 -20,180 30,140 30,140 -20)))')
    if ogrtest.check_feature_geometry(f, ref_geom) != 0:
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOLYGON (((180 40,180 70,-16 70,-16 40,180 40)),((-180 70,-180 40,-140 40,-140 70,-180 70)))')
    if ogrtest.check_feature_geometry(f, ref_geom) != 0:
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POLYGON ((170 -40,-16 -40,-16 70,170 -70,170 -40))')
    if ogrtest.check_feature_geometry(f, ref_geom) != 0:
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink(tmpfilename)

###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/1109


def test_ogr_geojson_non_finite():

    json_content = """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "properties": { "inf_prop": infinity, "minus_inf_prop": -infinity, "nan_prop": nan }, "geometry": null }
  ]
}"""
    with gdaltest.error_handler():
        ds = ogr.Open(json_content)
    if ds is None:
        # Might fail with older libjson-c versions
        pytest.skip()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    for i in range(3):
        assert lyr.GetLayerDefn().GetFieldDefn(i).GetType() == ogr.OFTReal

    if f['inf_prop'] != float('inf'):
        f.DumpReadable()
        pytest.fail()
    if f['minus_inf_prop'] != float('-inf'):
        f.DumpReadable()
        pytest.fail()
    if not math.isnan(f['nan_prop']):
        f.DumpReadable()
        pytest.fail(str(f['nan_prop']))
    ds = None

    tmpfilename = '/vsimem/out.json'

    with gdaltest.error_handler():
        gdal.VectorTranslate(tmpfilename, json_content, options='-f GeoJSON')
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 0
    ds = None

    gdal.VectorTranslate(tmpfilename, json_content, options='-f GeoJSON -lco WRITE_NON_FINITE_VALUES=YES')
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 3
    f = lyr.GetNextFeature()
    if f['inf_prop'] != float('inf'):
        f.DumpReadable()
        pytest.fail()
    if f['minus_inf_prop'] != float('-inf'):
        f.DumpReadable()
        pytest.fail()
    if not math.isnan(f['nan_prop']):
        f.DumpReadable()
        pytest.fail(str(f['nan_prop']))
    ds = None

    gdal.Unlink(tmpfilename)

###############################################################################

def test_ogr_geojson_random_reading_with_id():

    json_content = """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "id": 1, "properties": { "a": "a" }, "geometry": null },
      { "type": "Feature", "id": 2, "properties": { "a": "bc" }, "geometry": null }
  ]
}"""
    tmpfilename = '/vsimem/temp.json'
    gdal.FileFromMemBuffer(tmpfilename, json_content)
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    f1_ref = lyr.GetNextFeature()
    f2_ref = lyr.GetNextFeature()
    f1 = lyr.GetFeature(1)
    f2 = lyr.GetFeature(2)
    assert f1.Equal(f1_ref)
    assert f2.Equal(f2_ref)
    assert not lyr.GetFeature(3)
    ds = None
    gdal.Unlink(tmpfilename)

###############################################################################

def test_ogr_geojson_random_reading_without_id():

    json_content = """{
  "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "properties": { "a": "a" }, "geometry": null },
      { "type": "Feature", "properties": { "a": "bc" }, "geometry": null }
  ]
}"""
    tmpfilename = '/vsimem/temp.json'
    gdal.FileFromMemBuffer(tmpfilename, json_content)
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    f1_ref = lyr.GetNextFeature()
    f2_ref = lyr.GetNextFeature()
    f1 = lyr.GetFeature(0)
    f2 = lyr.GetFeature(1)
    assert f1.Equal(f1_ref)
    assert f2.Equal(f2_ref)
    assert not lyr.GetFeature(2)
    ds = None
    gdal.Unlink(tmpfilename)

###############################################################################

def test_ogr_geojson_single_feature_random_reading_with_id():

    json_content = """
      { "type": "Feature", "id": 1, "properties": { "a": "a" }, "geometry": null }
}"""
    tmpfilename = '/vsimem/temp.json'
    gdal.FileFromMemBuffer(tmpfilename, json_content)
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    f1_ref = lyr.GetNextFeature()
    f1 = lyr.GetFeature(1)
    assert f1.Equal(f1_ref)
    ds = None
    gdal.Unlink(tmpfilename)

###############################################################################

def test_ogr_geojson_3D_geom_type():

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2,3]}, "properties": null},
{"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2,4]}, "properties": null}
]}""")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint25D

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2,3]}, "properties": null},
{"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2]}, "properties": null}
]}""")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint25D

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2]}, "properties": null},
{"type": "Feature", "geometry": {"type":"Point","coordinates":[1,2,4]}, "properties": null}
]}""")
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbPoint25D

###############################################################################

def test_ogr_geojson_update_in_loop():

    tmpfilename = '/vsimem/temp.json'

    # No explicit id
    gdal.FileFromMemBuffer(tmpfilename, '{"type": "FeatureCollection", "name": "test", "features": [{ "type": "Feature", "properties": { "foo": 1 }, "geometry": null }, { "type": "Feature", "properties": { "foo": 2 }, "geometry": null }]}')
    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR | gdal.GA_Update)
    layer = ds.GetLayer()
    fids = []
    for feature in layer:
        fids.append(feature.GetFID())
        layer.SetFeature(feature)
    assert fids == [0, 1]
    ds = None

    # Explicit id no holes
    gdal.FileFromMemBuffer(tmpfilename, '{"type": "FeatureCollection", "name": "test", "features": [{ "type": "Feature", "id": 0, "properties": { "foo": 1 }, "geometry": null }, { "type": "Feature", "properties": { "foo": 2 }, "id": 1, "geometry": null }]}')

    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR | gdal.GA_Update)
    layer = ds.GetLayer()
    fids = []
    for feature in layer:
        fids.append(feature.GetFID())
        layer.SetFeature(feature)
    assert fids == [0, 1]
    ds = None

    # Explicit id with holes
    gdal.FileFromMemBuffer(tmpfilename, '{"type": "FeatureCollection", "name": "test", "features": [{ "type": "Feature", "id": 1, "properties": { "foo": 1 }, "geometry": null }, { "type": "Feature", "properties": { "foo": 2 }, "id": 3, "geometry": null }]}')
    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR | gdal.GA_Update)
    layer = ds.GetLayer()
    fids = []
    for feature in layer:
        fids.append(feature.GetFID())
        layer.SetFeature(feature)
    assert fids == [1, 3]
    ds = None

    gdal.Unlink(tmpfilename)

###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/2720

def test_ogr_geojson_starting_with_coordinates():

    tmpfilename = '/vsimem/temp.json'
    gdal.FileFromMemBuffer(tmpfilename, '{ "coordinates": [' + (' ' * 10000) + '2,49], "type": "Point"}')
    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR)
    assert ds is not None

    gdal.Unlink(tmpfilename)

###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/2787

def test_ogr_geojson_starting_with_geometry_coordinates():

    tmpfilename = '/vsimem/temp.json'
    gdal.FileFromMemBuffer(tmpfilename, '{ "geometry": {"coordinates": [' + (' ' * 10000) + '2,49], "type": "Point"}, "type": "Feature", "properties": {} }')
    ds = gdal.OpenEx(tmpfilename, gdal.OF_VECTOR)
    assert ds is not None

    gdal.Unlink(tmpfilename)


###############################################################################
# Test serialization of Float32 values

def test_ogr_geojson_write_float32():

    def cast_as_float(x):
        return struct.unpack('f', struct.pack('f', x))[0]

    filename = '/vsimem/test_ogr_geojson_write_float32.json'
    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource(filename)
    lyr = ds.CreateLayer('foo')

    fldn_defn = ogr.FieldDefn('float32', ogr.OFTReal)
    fldn_defn.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fldn_defn)

    fldn_defn = ogr.FieldDefn('float32list', ogr.OFTRealList)
    fldn_defn.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fldn_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f['float32'] = cast_as_float(0.35)
    f['float32list'] = [
        cast_as_float(123.0),
        cast_as_float(0.35),
        cast_as_float(0.15),
        cast_as_float(0.12345678),
        cast_as_float(1.2345678e-15),
        cast_as_float(1.2345678e15),
        cast_as_float(0.123456789), # more decimals than Float32 can hold
    ]
    lyr.CreateFeature(f)

    ds = None

    fp = gdal.VSIFOpenL(filename, 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink(filename)

    data = data.replace('e+0', 'e+').replace('e-0', 'e-')

    assert '"float32": 0.35,' in data
    assert '"float32list": [ 123.0, 0.35, 0.15, 0.12345678, 1.2345678e-15, 1.2345678e+15, 0.12345679 ]' in data


###############################################################################
# Test bugfix for #3172

def test_ogr_geojson_write_float_exponential_without_dot():

    filename = '/vsimem/test_ogr_geojson_write_float_exponential_without_dot.json'
    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource(filename)
    lyr = ds.CreateLayer('foo')

    fldn_defn = ogr.FieldDefn('float32', ogr.OFTReal)
    fldn_defn.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fldn_defn)

    fldn_defn = ogr.FieldDefn('float64', ogr.OFTReal)
    lyr.CreateField(fldn_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f['float32'] = 1e-7
    f['float64'] = 1e-8
    lyr.CreateFeature(f)

    ds = None

    fp = gdal.VSIFOpenL(filename, 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink(filename)

    # Check that the json can be parsed
    json.loads(data)


###############################################################################
# Test bugfix for #3280

def test_ogr_geojson_feature_starting_with_big_properties():

    filename = '/vsimem/test_ogr_geojson_feature_starting_with_big_properties.json'
    gdal.FileFromMemBuffer(filename,
                           '{"properties":{"foo":"%s"},"type":"Feature","geometry":null}' % ('x' * 10000))
    assert ogr.Open(filename) is not None
    gdal.Unlink(filename)


###############################################################################

def test_ogr_geojson_export_geometry_axis_order():

    # EPSG:4326 and lat,long data order
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    g = ogr.CreateGeometryFromWkt('POINT (49 2)')
    g.AssignSpatialReference(sr)
    before_wkt = g.ExportToWkt()
    assert json.loads(g.ExportToJson()) == { "type": "Point", "coordinates": [ 2.0, 49.0 ] }
    assert g.ExportToWkt() == before_wkt

    # EPSG:4326 and long,lat data order
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    sr.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    g = ogr.CreateGeometryFromWkt('POINT (2 49)')
    g.AssignSpatialReference(sr)
    assert json.loads(g.ExportToJson()) == { "type": "Point", "coordinates": [ 2.0, 49.0 ] }

    # CRS84 with long,lat CRS and data order
    sr = osr.SpatialReference()
    sr.SetFromUserInput("OGC:CRS84")
    g = ogr.CreateGeometryFromWkt('POINT (2 49)')
    g.AssignSpatialReference(sr)
    assert json.loads(g.ExportToJson()) == { "type": "Point", "coordinates": [ 2.0, 49.0 ] }

    # Projected CRS with easting, northing order
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    g = ogr.CreateGeometryFromWkt('POINT (2 49)')
    g.AssignSpatialReference(sr)
    assert json.loads(g.ExportToJson()) == { "type": "Point", "coordinates": [ 2.0, 49.0 ] }

    # Projected CRS with northing, easting order
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(2393)
    g = ogr.CreateGeometryFromWkt('POINT (49 2)')
    g.AssignSpatialReference(sr)
    assert json.loads(g.ExportToJson()) == { "type": "Point", "coordinates": [ 2.0, 49.0 ] }

    # No CRS
    g = ogr.CreateGeometryFromWkt('POINT (2 49)')
    assert json.loads(g.ExportToJson()) == { "type": "Point", "coordinates": [ 2.0, 49.0 ] }
