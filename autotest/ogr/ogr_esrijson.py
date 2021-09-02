#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ESRIJson driver test suite.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009-2019, Even Rouault <even dot rouault at spatialys.com>
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

import contextlib

from osgeo import ogr
from osgeo import gdal

import gdaltest
import ogrtest
import pytest

pytestmark = pytest.mark.require_driver('ESRIJson')

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


###############################################################################
# Test reading ESRI point file


def test_ogr_esrijson_read_point():

    ds = ogr.Open('data/esrijson/esripoint.json')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayerByName('esripoint')
    assert lyr is not None, 'Missing layer called esripoint'

    extent = (2, 2, 49, 49)

    rc = validate_layer(lyr, 'esripoint', 1, ogr.wkbPoint, 4, extent)
    assert rc

    ref = lyr.GetSpatialRef()
    gcs = int(ref.GetAuthorityCode('GEOGCS'))

    assert gcs == 4326, "Spatial reference was not valid"

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    if feature.GetFID() != 1:
        feature.DumpReadable()
        pytest.fail()

    if feature.GetFieldAsInteger('fooInt') != 2:
        feature.DumpReadable()
        pytest.fail()

    if feature.GetFieldAsDouble('fooDouble') != 3.4:
        feature.DumpReadable()
        pytest.fail()

    if feature.GetFieldAsString('fooString') != '56':
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Test reading ESRI linestring file


def test_ogr_esrijson_read_linestring():

    ds = ogr.Open('data/esrijson/esrilinestring.json')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayer(0)

    extent = (2, 3, 49, 50)

    rc = validate_layer(lyr, None, 1, ogr.wkbLineString, 0, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('LINESTRING (2 49,3 50)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

    # MultiLineString
    ds = ogr.Open("""{
  "geometryType": "esriGeometryPolyline",
  "fields": [],
  "features": [
  {
   "geometry": {
      "paths" : [
       [ [2,49],[2.1,49.1] ],
       [ [3,50],[3.1,50.1] ]
      ]
   }
  }
 ]
}""")
    lyr = ds.GetLayer(0)
    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTILINESTRING ((2 49,2.1 49.1),(3 50,3.1 50.1))')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()


###############################################################################
# Test reading ESRI polygon file


def test_ogr_esrijson_read_polygon():

    ds = ogr.Open('data/esrijson/esripolygon.json')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayer(0)

    extent = (-3, 3, 49, 50)

    rc = validate_layer(lyr, None, 1, ogr.wkbPolygon, 0, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOLYGON (((2 49,2 50,3 50,3 49,2 49),(2.1 49.1,2.1 49.9,2.9 49.9,2.9 49.1,2.1 49.1)),((-2 49,-2 50,-3 50,-3 49,-2 49)))')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

    ds = ogr.Open('data/esrijson/esripolygonempty.json')
    assert ds is not None, 'Failed to open datasource'
    lyr = ds.GetLayer(0)
    feature = lyr.GetNextFeature()
    if feature.GetGeometryRef().ExportToWkt() != 'POLYGON EMPTY':
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Test reading ESRI multipoint file


def test_ogr_esrijson_read_multipoint():

    ds = ogr.Open('data/esrijson/esrimultipoint.json')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayer(0)

    extent = (2, 3, 49, 50)

    rc = validate_layer(lyr, None, 1, ogr.wkbMultiPoint, 4, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOINT (2 49,3 50)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Test reading ESRI point file with z value


def test_ogr_esrijson_read_pointz():

    ds = ogr.Open('data/esrijson/esrizpoint.json')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayer(0)

    # validate layer doesn't check z, but put it in
    extent = (2, 2, 49, 49, 1, 1)

    rc = validate_layer(lyr, None, 1, ogr.wkbPoint, 4, extent)
    assert rc

    ref = lyr.GetSpatialRef()
    gcs = int(ref.GetAuthorityCode('GEOGCS'))

    assert gcs == 4326, "Spatial reference was not valid"

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POINT(2 49 1)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    if feature.GetFID() != 1:
        feature.DumpReadable()
        pytest.fail()

    if feature.GetFieldAsInteger('fooInt') != 2:
        feature.DumpReadable()
        pytest.fail()

    if feature.GetFieldAsDouble('fooDouble') != 3.4:
        feature.DumpReadable()
        pytest.fail()

    if feature.GetFieldAsString('fooString') != '56':
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Test reading ESRI linestring file with z


def test_ogr_esrijson_read_linestringz():

    ds = ogr.Open('data/esrijson/esrizlinestring.json')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayer(0)

    # validate layer doesn't check z, but put it in
    extent = (2, 3, 49, 50, 1, 2)

    rc = validate_layer(lyr, None, 1, ogr.wkbLineString, 0, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('LINESTRING (2 49 1,3 50 2)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Test reading ESRI multipoint file with z


def test_ogr_esrijson_read_multipointz():

    ds = ogr.Open('data/esrijson/esrizmultipoint.json')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayer(0)

    # validate layer doesn't check z, but put it in
    extent = (2, 3, 49, 50, 1, 2)

    rc = validate_layer(lyr, None, 1, ogr.wkbMultiPoint, 4, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOINT (2 49 1,3 50 2)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Test reading ESRI polygon file with z


def test_ogr_esrijson_read_polygonz():

    ds = ogr.Open('data/esrijson/esrizpolygon.json')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayer(0)

    # validate layer doesn't check z, but put it in
    extent = (2, 3, 49, 50, 1, 4)

    rc = validate_layer(lyr, None, 1, ogr.wkbPolygon, 0, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POLYGON ((2 49 1,2 50 2,3 50 3,3 49 4,2 49 1))')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Test reading ESRI multipoint file with m, but no z (hasM=true, hasZ omitted)


def test_ogr_esrijson_read_multipointm():

    ds = ogr.Open('data/esrijson/esrihasmnozmultipoint.json')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayer(0)

    extent = (2, 3, 49, 50)

    rc = validate_layer(lyr, None, 1, ogr.wkbMultiPoint, 4, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOINT M ((2 49 1),(3 50 2))')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Test reading ESRI multipoint file with hasZ=true, but only 2 components.


def test_ogr_esrijson_read_pointz_withou_z():

    ds = ogr.Open('data/esrijson/esriinvalidhaszmultipoint.json')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayer(0)

    extent = (2, 3, 49, 50)

    rc = validate_layer(lyr, None, 1, ogr.wkbMultiPoint, 4, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOINT (2 49,3 50)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Test reading ESRI multipoint file with z and m


def test_ogr_esrijson_read_multipointzm():

    ds = ogr.Open('data/esrijson/esrizmmultipoint.json')
    assert ds is not None, 'Failed to open datasource'

    assert ds.GetLayerCount() == 1, 'Wrong number of layers'

    lyr = ds.GetLayer(0)

    extent = (2, 3, 49, 50)

    rc = validate_layer(lyr, None, 1, ogr.wkbMultiPoint, 4, extent)
    assert rc

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOINT ZM ((2 49 1 100),(3 50 2 100))')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        pytest.fail()

    lyr = None
    ds = None

###############################################################################
# Test ESRI FeatureService scrolling

@pytest.mark.parametrize("prefix", ["", "ESRIJSON:"])
def test_ogr_esrijson_featureservice_scrolling(prefix):

    @contextlib.contextmanager
    def cleanup_after_me():
        yield
        files = gdal.ReadDir('/vsimem/esrijson')
        if files:
            for f in files:
                gdal.Unlink('/vsimem/esrijson/' + f)


    with cleanup_after_me():
        with gdaltest.config_option('CPL_CURL_ENABLE_VSIMEM', 'YES'):

            resultOffset0 = """
        { "type":"FeatureCollection",
        "properties" : {
            "exceededTransferLimit" : true
        },
        "features" :
        [
            {
            "type": "Feature",
            "geometry": {
                "type": "Point",
                "coordinates": [ 2, 49 ]
            },
            "properties": {
                "id": 1,
                "a_property": 1,
            }
            } ] }"""

            gdal.FileFromMemBuffer('/vsimem/esrijson/test.json?resultRecordCount=1', resultOffset0)
            ds = ogr.Open('/vsimem/esrijson/test.json?resultRecordCount=1')
            lyr = ds.GetLayer(0)
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 1
            f = lyr.GetNextFeature()
            assert f is None
            ds = None
            gdal.Unlink('/vsimem/esrijson/test.json?resultRecordCount=1')

            gdal.FileFromMemBuffer('/vsimem/esrijson/test.json?resultRecordCount=10', resultOffset0)
            gdal.PushErrorHandler()
            ds = ogr.Open('/vsimem/esrijson/test.json?resultRecordCount=10')
            gdal.PopErrorHandler()
            lyr = ds.GetLayer(0)
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 1
            f = lyr.GetNextFeature()
            assert f is None
            ds = None
            gdal.Unlink('/vsimem/esrijson/test.json?resultRecordCount=10')

            gdal.FileFromMemBuffer('/vsimem/esrijson/test.json?', resultOffset0)
            gdal.FileFromMemBuffer('/vsimem/esrijson/test.json?resultRecordCount=1&resultOffset=0', resultOffset0)

            ds = ogr.Open('/vsimem/esrijson/test.json?')
            lyr = ds.GetLayer(0)
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 1
            f = lyr.GetNextFeature()
            assert f is None
            lyr.ResetReading()
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 1

            resultOffset1 = """
        { "type":"FeatureCollection",
        "features" :
        [
            {
            "type": "Feature",
            "geometry": {
                "type": "Point",
                "coordinates": [ 2, 49 ]
            },
            "properties": {
                "id": 2,
                "a_property": 1,
            }
            } ] }"""
            gdal.FileFromMemBuffer('/vsimem/esrijson/test.json?resultRecordCount=1&resultOffset=1', resultOffset1)
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 2
            f = lyr.GetNextFeature()
            assert f is None

            gdal.PushErrorHandler()
            fc = lyr.GetFeatureCount()
            gdal.PopErrorHandler()
            assert fc == 2

            gdal.FileFromMemBuffer('/vsimem/esrijson/test.json?returnCountOnly=true',
                                """{ "count": 123456}""")
            fc = lyr.GetFeatureCount()
            assert fc == 123456

            gdal.PushErrorHandler()
            extent = lyr.GetExtent()
            gdal.PopErrorHandler()
            assert extent == (2, 2, 49, 49)

            gdal.FileFromMemBuffer('/vsimem/esrijson/test.json?returnExtentOnly=true&f=geojson',
                                """{"type":"FeatureCollection","bbox":[1, 2, 3, 4],"features":[]}""")
            extent = lyr.GetExtent()
            assert extent == (1.0, 3.0, 2.0, 4.0)

            assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1

            assert lyr.TestCapability(ogr.OLCFastGetExtent) == 0

            assert lyr.TestCapability('foo') == 0

            # Test scrolling with ESRI json
            resultOffset0 = """
        {
        "objectIdFieldName" : "objectid",
        "geometryType" : "esriGeometryPoint",
        "fields" : [
            {
            "name" : "objectid",
            "alias" : "Object ID",
            "type" : "esriFieldTypeOID"
            },

        ],
        "features" : [
            {
            "geometry" : {
                "x" : 2,
                "y" : 49,
                "z" : 1
            },
            "attributes" : {
                "objectid" : 1
            }
            }
        ],
        "exceededTransferLimit": true
        }
        """

            resultOffset1 = """
        {
        "objectIdFieldName" : "objectid",
        "geometryType" : "esriGeometryPoint",
        "fields" : [
            {
            "name" : "objectid",
            "alias" : "Object ID",
            "type" : "esriFieldTypeOID"
            },

        ],
        "features" : [
            {
            "geometry": null,
            "attributes" : {
                "objectid" : 20
            }
            }
        ]
        }
        """

            gdal.FileFromMemBuffer('/vsimem/esrijson/test.json?resultRecordCount=1', resultOffset0)
            gdal.FileFromMemBuffer('/vsimem/esrijson/test.json?resultRecordCount=1&resultOffset=1', resultOffset1)
            ds = ogr.Open(prefix + '/vsimem/esrijson/test.json?resultRecordCount=1')
            lyr = ds.GetLayer(0)
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 1
            f = lyr.GetNextFeature()
            assert f is not None and f.GetFID() == 20
            ds = None
            gdal.Unlink('/vsimem/esrijson/test.json?resultRecordCount=1')
            gdal.Unlink('/vsimem/esrijson/test.json?resultRecordCount=1&resultOffset=1')

###############################################################################
# Test reading ESRIJSON files starting with {"features":[{"geometry":.... (#7198)

def test_ogr_esrijson_read_starting_with_features_geometry():

    ds = ogr.Open('data/esrijson/esrijsonstartingwithfeaturesgeometry.json')
    assert ds is not None
    assert ds.GetDriver().GetName() == 'ESRIJSON'
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1


###############################################################################
# Test ogr.CreateGeometryFromEsriJson()


def test_ogr_esrijson_create_geometry_from_esri_json():

    with gdaltest.error_handler():
        assert not ogr.CreateGeometryFromEsriJson('error')

    g = ogr.CreateGeometryFromEsriJson('{ "x": 2, "y": 49 }')
    assert g.ExportToWkt() == 'POINT (2 49)'


###############################################################################
# Test for https://github.com/OSGeo/gdal/issues/2007

def test_ogr_esrijson_identify_srs():

    data = """
        {
        "objectIdFieldName" : "objectid",
        "geometryType" : "esriGeometryPoint",
        "spatialReference":{"wkt":"PROJCS[\\"NAD_1983_StatePlane_Arizona_Central_FIPS_0202_IntlFeet\\",GEOGCS[\\"GCS_North_American_1983\\",DATUM[\\"D_North_American_1983\\",SPHEROID[\\"GRS_1980\\",6378137.0,298.257222101]],PRIMEM[\\"Greenwich\\",0.0],UNIT[\\"Degree\\",0.0174532925199433]],PROJECTION[\\"Transverse_Mercator\\"],PARAMETER[\\"False_Easting\\",700000.0],PARAMETER[\\"False_Northing\\",0.0],PARAMETER[\\"Central_Meridian\\",-111.9166666666667],PARAMETER[\\"Scale_Factor\\",0.9999],PARAMETER[\\"Latitude_Of_Origin\\",31.0],UNIT[\\"Foot\\",0.3048]]"},
        "fields" : [],
        "features" : []
        }
        """

    ds = ogr.Open(data)
    assert ds is not None
    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    assert sr
    assert sr.GetAuthorityCode(None) == '2223'
