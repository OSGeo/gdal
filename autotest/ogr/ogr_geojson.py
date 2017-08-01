#!/usr/bin/env python
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
# Copyright (c) 2009-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
import os
import sys

sys.path.append( '../pymod' )

from osgeo import osr
from osgeo import ogr
from osgeo import gdal

import gdaltest
import ogrtest
###############################################################################
# Test utilities
def validate_layer(lyr, name, features, type, fields, box):

    if name != lyr.GetName():
        print('Wrong layer name')
        return False

    if features != lyr.GetFeatureCount():
        print('Wrong number of features')
        return False

    lyrDefn = lyr.GetLayerDefn()
    if lyrDefn is None:
        print('Layer definition is none')
        return False

    if type != lyrDefn.GetGeomType():
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


def verify_geojson_copy(name, fids, names):

    if gdaltest.gjpoint_feat is None:
        print('Missing features collection')
        return False

    fname = os.path.join('tmp', name + '.geojson')
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
    ret = ogrtest.check_features_against_list( lyr, 'FID', fids)
    if ret != 1:
        print('Wrong values in \'FID\' field')
        return False

    lyr.ResetReading()
    ret = ogrtest.check_features_against_list( lyr, 'NAME', names)
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

        if ogrtest.check_feature_geometry( feat, orig_feat.GetGeometryRef(),
                                           max_error = 0.001) != 0:
            print('Geometry test failed')
            gdaltest.gjpoint_feat = None
            return False

    gdaltest.gjpoint_feat = None

    lyr = None

    return True


def copy_shape_to_geojson(gjname, compress = None):

    if gdaltest.geojson_drv is None:
        return False

    if compress is not None:
        if compress[0:5] == '/vsig':
            dst_name = os.path.join('/vsigzip/', 'tmp', gjname + '.geojson' + '.gz')
        elif compress[0:4] == '/vsiz':
            dst_name = os.path.join('/vsizip/', 'tmp', gjname + '.geojson' + '.zip')
        elif compress == '/vsistdout/':
            dst_name = compress
        else:
            return False
    else:
        dst_name = os.path.join('tmp', gjname + '.geojson')

    ds = gdaltest.geojson_drv.CreateDataSource(dst_name)
    if ds is None:
        return False

    ######################################################
    # Create layer
    lyr = ds.CreateLayer(gjname)
    if lyr is None:
        return False

    ######################################################
    # Setup schema (all test shapefiles use common schmea)
    ogrtest.quick_create_layer_def(lyr,
                                   [ ('FID', ogr.OFTReal),
                                     ('NAME', ogr.OFTString) ])

    ######################################################
    # Copy in gjpoint.shp

    dst_feat = ogr.Feature(feature_def = lyr.GetLayerDefn())

    src_name = os.path.join('data', gjname + '.shp')
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

    return True

###############################################################################
# Find GeoJSON driver

def ogr_geojson_1():

    gdaltest.geojson_drv = ogr.GetDriverByName('GeoJSON')

    if gdaltest.geojson_drv is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test file-based DS with standalone "Point" feature object.

def ogr_geojson_2():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/point.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('point')
    if lyr is None:
        gdaltest.post_reason('Missing layer called point')
        return 'fail'

    extent = (100.0, 100.0, 0.0, 0.0)

    rc = validate_layer(lyr, 'point', 1, ogr.wkbPoint, 0, extent)
    if not rc:
        return 'fail'

    lyr = None

    return 'success'

###############################################################################
# Test file-based DS with standalone "LineString" feature object.

def ogr_geojson_3():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/linestring.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('linestring')
    if lyr is None:
        gdaltest.post_reason('Missing layer called linestring')
        return 'fail'

    extent = (100.0, 101.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'linestring', 1, ogr.wkbLineString, 0, extent)
    if not rc:
        return 'fail'

    lyr = None

    return 'success'

##############################################################################
# Test file-based DS with standalone "Polygon" feature object.

def ogr_geojson_4():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/polygon.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('polygon')
    if lyr is None:
        gdaltest.post_reason('Missing layer called polygon')
        return 'fail'

    extent = (100.0, 101.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'polygon', 1, ogr.wkbPolygon, 0, extent)
    if not rc:
        return 'fail'

    lyr = None

    return 'success'

##############################################################################
# Test file-based DS with standalone "GeometryCollection" feature object.

def ogr_geojson_5():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/geometrycollection.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('geometrycollection')
    if lyr is None:
        gdaltest.post_reason('Missing layer called geometrycollection')
        return 'fail'

    extent = (100.0, 102.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'geometrycollection', 1, ogr.wkbGeometryCollection, 0, extent)
    if not rc:
        return 'fail'

    lyr = None

    return 'success'

##############################################################################
# Test file-based DS with standalone "MultiPoint" feature object.

def ogr_geojson_6():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/multipoint.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('multipoint')
    if lyr is None:
        gdaltest.post_reason('Missing layer called multipoint')
        return 'fail'

    extent = (100.0, 101.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'multipoint', 1, ogr.wkbMultiPoint, 0, extent)
    if not rc:
        return 'fail'

    lyr = None

    return 'success'

##############################################################################
# Test file-based DS with standalone "MultiLineString" feature object.

def ogr_geojson_7():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/multilinestring.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('multilinestring')
    if lyr is None:
        gdaltest.post_reason('Missing layer called multilinestring')
        return 'fail'

    extent = (100.0, 103.0, 0.0, 3.0)

    rc = validate_layer(lyr, 'multilinestring', 1, ogr.wkbMultiLineString, 0, extent)
    if not rc:
        return 'fail'

    lyr = None

    return 'success'

##############################################################################
# Test file-based DS with standalone "MultiPolygon" feature object.

def ogr_geojson_8():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/multipolygon.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('multipolygon')
    if lyr is None:
        gdaltest.post_reason('Missing layer called multipolygon')
        return 'fail'

    extent = (100.0, 103.0, 0.0, 3.0)

    rc = validate_layer(lyr, 'multipolygon', 1, ogr.wkbMultiPolygon, 0, extent)
    if not rc:
        return 'fail'

    lyr = None

    return 'success'

##############################################################################
# Test translation of data/gjpoint.shp to GeoJSON file

def ogr_geojson_9():

    if gdaltest.geojson_drv is None:
        return 'skip'

    gdaltest.tests = [
        ['gjpoint', [ 1 ], [ 'Point 1' ] ],
        ['gjline',  [ 1 ], [ 'Line 1' ] ],
        ['gjpoly',  [ 1 ], [ 'Polygon 1' ] ],
        ['gjmultipoint', [ 1 ], [ 'MultiPoint 1' ] ],
        ['gjmultiline', [ 2 ], [ 'MultiLine 1' ] ],
        ['gjmultipoly', [ 2 ], [ 'MultiPoly 1' ] ]
    ]

    for i in range(len(gdaltest.tests)):
        test = gdaltest.tests[i]

        rc = copy_shape_to_geojson(test[0])
        if not rc:
            gdaltest.post_reason('Failed making copy of ' + test[0] +'.shp')
            return 'fail'

        rc = verify_geojson_copy(test[0], test[1], test[2])
        if not rc:
            gdaltest.post_reason('Verification of copy of ' + test[0] + '.shp failed')
            return 'fail'

    return 'success'

##############################################################################
# Test translation of data/gjpoint.shp to GZip compressed GeoJSON file

def ogr_geojson_10():

    if gdaltest.geojson_drv is None:
        return 'skip'

    gdaltest.tests = [
        ['gjpoint', [ 1 ], [ 'Point 1' ] ],
        ['gjline',  [ 1 ], [ 'Line 1' ] ],
        ['gjpoly',  [ 1 ], [ 'Polygon 1' ] ],
        ['gjmultipoint', [ 1 ], [ 'MultiPoint 1' ] ],
        ['gjmultiline', [ 2 ], [ 'MultiLine 1' ] ],
        ['gjmultipoly', [ 2 ], [ 'MultiPoly 1' ] ]
    ]

    for i in range(len(gdaltest.tests)):
        test = gdaltest.tests[i]

        rc = copy_shape_to_geojson(test[0], '/vsigzip/')
        if not rc:
            gdaltest.post_reason('Failed making copy of ' + test[0] +'.shp')
            return 'fail'

        rc = verify_geojson_copy(test[0], test[1], test[2])
        if not rc:
            gdaltest.post_reason('Verification of copy of ' + test[0] + '.shp failed')
            return 'fail'

    return 'success'

###############################################################################

def ogr_geojson_11():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/srs_name.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('srs_name')
    if lyr is None:
        gdaltest.post_reason('Missing layer called srs_name')
        return 'fail'

    extent = (100.0, 102.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'srs_name', 1, ogr.wkbGeometryCollection, 0, extent)
    if not rc:
        return 'fail'

    ref = lyr.GetSpatialRef()
    pcs = int(ref.GetAuthorityCode('PROJCS'))
    if pcs != 26915:
        gdaltest.post_reason('Spatial reference was not valid')
        return 'fail'

    feature = lyr.GetNextFeature()
    geometry = feature.GetGeometryRef().GetGeometryRef(0)

    srs = geometry.GetSpatialReference()
    pcs = int(srs.GetAuthorityCode('PROJCS'))
    if pcs != 26916:
        gdaltest.post_reason('Spatial reference for individual geometry was not valid')
        return 'fail'

    lyr = None

    return 'success'

###############################################################################
# Test DS passed as name with standalone "Point" feature object (#3377)

def ogr_geojson_12():

    if gdaltest.geojson_drv is None:
        return 'skip'

    import os
    if os.name == 'nt':
        return 'skip'

    import test_cli_utilities

    if test_cli_utilities.get_ogrinfo_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' -ro -al \'{"type": "Point","coordinates": [100.0, 0.0]}\'')
    if ret.find(' POINT (100 0)') == -1:
        print(ret)
        return 'fail'

    return 'success'


###############################################################################
# Test writing to stdout (#3381)

def ogr_geojson_13():

    if gdaltest.geojson_drv is None:
        return 'skip'

    test = ['gjpoint', [ 1 ], [ 'Point 1' ] ]

    rc = copy_shape_to_geojson(test[0], '/vsistdout/')
    if not rc:
        gdaltest.post_reason('Failed making copy of ' + test[0] +'.shp')
        return 'fail'

    return 'success'

###############################################################################
# Test reading & writing various degenerated geometries

def ogr_geojson_14():

    if gdaltest.geojson_drv is None:
        return 'skip'

    if int(gdal.VersionInfo('VERSION_NUM')) < 1800:
        return 'skip'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('data/ogr_geojson_14.geojson')
    gdal.PopErrorHandler()
    lyr = ds.GetLayer(0)

    out_ds = gdaltest.geojson_drv.CreateDataSource('tmp/out_ogr_geojson_14.geojson')
    out_lyr = out_ds.CreateLayer('lyr')

    feat = lyr.GetNextFeature()
    while feat is not None:
        geom = feat.GetGeometryRef()
        if geom is not None:
            #print(geom)
            out_feat = ogr.Feature(feature_def = out_lyr.GetLayerDefn())
            out_feat.SetGeometry(geom)
            out_lyr.CreateFeature(out_feat)
        feat = lyr.GetNextFeature()

    out_ds = None

    return 'success'

###############################################################################
# Test Feature.ExportToJson (#3870)

def ogr_geojson_15():

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
        return 'skip'

    expected_out = """{"geometry": {"type": "Point", "coordinates": [1.0, 2.0]}, "type": "Feature", "properties": {"foo": "bar", "boolfield": true}, "id": 0}"""

    if out != expected_out:
        out_json = json.loads(out)
        expected_out_json = json.loads(expected_out)
        if out_json != expected_out_json:
            gdaltest.post_reason('fail')
            print(out)
            return 'fail'


    out = feature.ExportToJson(as_object = True)
    expected_out = {'geometry': {'type': 'Point', 'coordinates': [1.0, 2.0]}, 'type': 'Feature', 'properties': {'foo': 'bar', "boolfield": True}, 'id': 0}

    if out != expected_out:
        gdaltest.post_reason('fail')
        print(out)
        return 'fail'

    return 'success'

###############################################################################
# Test reading ESRI point file

def ogr_geojson_16():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/esripoint.json')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (2,2,49,49)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbPoint, 4, extent)
    if not rc:
        return 'fail'

    ref = lyr.GetSpatialRef()
    gcs = int(ref.GetAuthorityCode('GEOGCS'))

    if not gcs == 4326:
        gdaltest.post_reason("Spatial reference was not valid")
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    if feature.GetFID() != 1:
        feature.DumpReadable()
        return 'fail'

    if feature.GetFieldAsInteger('fooInt') != 2:
        feature.DumpReadable()
        return 'fail'

    if feature.GetFieldAsDouble('fooDouble') != 3.4:
        feature.DumpReadable()
        return 'fail'

    if feature.GetFieldAsString('fooString') != '56':
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test reading ESRI linestring file

def ogr_geojson_17():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/esrilinestring.json')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (2,3,49,50)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbLineString, 0, extent)
    if not rc:
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('LINESTRING (2 49,3 50)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

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
        return 'fail'

    return 'success'

###############################################################################
# Test reading ESRI polygon file

def ogr_geojson_18():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/esripolygon.json')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (-3,3,49,50)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbPolygon, 0, extent)
    if not rc:
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOLYGON (((2 49,2 50,3 50,3 49,2 49),(2.1 49.1,2.1 49.9,2.9 49.9,2.9 49.1,2.1 49.1)),((-2 49,-2 50,-3 50,-3 49,-2 49)))')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    ds = ogr.Open('data/esripolygonempty.json')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'
    lyr = ds.GetLayer(0)
    feature = lyr.GetNextFeature()
    if feature.GetGeometryRef().ExportToWkt() != 'POLYGON EMPTY':
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test reading ESRI multipoint file

def ogr_geojson_19():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/esrimultipoint.json')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (2,3,49,50)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiPoint, 4, extent)
    if not rc:
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOINT (2 49,3 50)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test reading files with no extension (#4314)

def ogr_geojson_20():

    if gdaltest.geojson_drv is None:
        return 'skip'

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
            gdaltest.post_reason('Failed to open datasource')
            return 'fail'
        ds = None

        gdal.Unlink('/vsimem/testgj')

    return 'success'

###############################################################################
# Test reading output of geocouch spatiallist

def ogr_geojson_21():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature",
 "geometry": {"type":"Point","coordinates":[1,2]},
 "properties": {"_id":"aid", "_rev":"arev", "type":"Feature",
                "properties":{"intvalue" : 2, "floatvalue" : 3.2, "strvalue" : "foo"}}}]}""")
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POINT (1 2)')
    if feature.GetFieldAsString("_id") != 'aid' or \
       feature.GetFieldAsString("_rev") != 'arev' or \
       feature.GetFieldAsInteger("intvalue") != 2 or \
       ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Same as ogr_geojson_21 with several features

def ogr_geojson_22():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature",
 "geometry": {"type":"Point","coordinates":[1,2]},
 "properties": {"_id":"aid", "_rev":"arev", "type":"Feature",
                "properties":{"intvalue" : 2, "floatvalue" : 3.2, "strvalue" : "foo"}}},
{"type": "Feature",
 "geometry": {"type":"Point","coordinates":[3,4]},
 "properties": {"_id":"aid2", "_rev":"arev2", "type":"Feature",
                "properties":{"intvalue" : 3.5, "str2value" : "bar"}}}]}""")
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POINT (1 2)')
    if feature.GetFieldAsString("_id") != 'aid' or \
       feature.GetFieldAsString("_rev") != 'arev' or \
       feature.GetFieldAsDouble("intvalue") != 2 or \
       ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POINT (3 4)')
    if feature.GetFieldAsString("_id") != 'aid2' or \
       feature.GetFieldAsString("_rev") != 'arev2' or \
       feature.GetFieldAsDouble("intvalue") != 3.5 or \
       feature.GetFieldAsString("str2value") != 'bar' or \
       ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Write GeoJSON with bbox and test SRS writing&reading back

def ogr_geojson_23():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = gdaltest.geojson_drv.CreateDataSource('/vsimem/ogr_geojson_23.json')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4322)
    lyr = ds.CreateLayer('foo', srs = sr, options = ['WRITE_BBOX=YES'])
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 10)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 20)'))
    lyr.CreateFeature(feat)
    lyr = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_geojson_23.json')
    lyr = ds.GetLayer(0)
    sr_got = lyr.GetSpatialRef()
    ds = None

    if sr_got.ExportToWkt() != sr.ExportToWkt():
        gdaltest.post_reason('did not get expected SRS')
        print(sr_got)
        return 'fail'

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_23.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_23.json')

    if data.find('"bbox": [ 1, 10, 2, 20 ]') == -1:
        gdaltest.post_reason('did not find global bbox')
        print(data)
        return 'fail'

    if data.find('"bbox": [ 1.0, 10.0, 1.0, 10.0 ]') == -1:
        gdaltest.post_reason('did not find first feature bbox')
        print(data)
        return 'fail'

    return 'success'

###############################################################################
# Test alternate form of geojson

def ogr_geojson_24():

    if gdaltest.geojson_drv is None:
        return 'skip'

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

        if ds is None:
            gdaltest.post_reason('Failed to open datasource')
            return 'fail'

        lyr = ds.GetLayerByName('layerFoo')
        if lyr is None:
            gdaltest.post_reason('cannot find layer')
            return 'fail'

        feature = lyr.GetNextFeature()
        ref_geom = ogr.CreateGeometryFromWkt('POINT (2 49)')
        if feature.GetFieldAsString("name") != 'bar' or \
        ogrtest.check_feature_geometry(feature, ref_geom) != 0:
            feature.DumpReadable()
            return 'fail'

        lyr = ds.GetLayerByName('layerBar')
        if lyr is None:
            gdaltest.post_reason('cannot find layer')
            return 'fail'

        feature = lyr.GetNextFeature()
        ref_geom = ogr.CreateGeometryFromWkt('POINT (2 49)')
        if feature.GetFieldAsString("other_name") != 'baz' or \
        ogrtest.check_feature_geometry(feature, ref_geom) != 0:
            feature.DumpReadable()
            return 'fail'

        ds = None

    return 'success'

###############################################################################
# Test TopoJSON

def ogr_geojson_25():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/topojson1.topojson')
    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'a_layer':
        gdaltest.post_reason('failure')
        return 'fail'
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'LINESTRING (100 1000,110 1000,110 1100)') != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    lyr = ds.GetLayer(1)
    if lyr.GetName() != 'TopoJSON':
        gdaltest.post_reason('failure')
        return 'fail'
    expected_results = [
        ( None, None, 'POINT EMPTY'),
        ( None, None, 'POINT EMPTY'),
        ( None, None, 'POINT EMPTY'),
        ( None, None, 'POINT (100 1010)'),
        ( None, None, 'LINESTRING EMPTY'),
        ( None, None, 'LINESTRING EMPTY'),
        ( None, None, 'LINESTRING EMPTY'),
        ( None, None, 'LINESTRING EMPTY'),
        ( None, None, 'LINESTRING EMPTY'),
        ( None, None, 'LINESTRING EMPTY'),
        ( None, None, 'LINESTRING EMPTY'),
        ( None, None, 'LINESTRING EMPTY'),
        ( None, '0', 'LINESTRING EMPTY'),
        ( None, 'foo', 'LINESTRING EMPTY'),
        ( '1', None, 'LINESTRING (100 1000,110 1000,110 1100)'),
        ( '2', None, 'LINESTRING (110 1100,110 1000,100 1000)'),
        ( None, None, 'POLYGON EMPTY'),
        ( None, None, 'POLYGON EMPTY'),
        ( None, None, 'POLYGON EMPTY'),
        ( None, None, 'POLYGON ((100 1000,110 1000,110 1100,100 1100,100 1000),(101 1010,101 1090,109 1090,109 1010,101 1010))'),
        ( None, None, 'POLYGON ((110 1100,110 1000,100 1000,100 1100,110 1100),(101 1010,109 1010,109 1090,101 1090,101 1010))'),
        ( None, None, 'MULTIPOINT EMPTY'),
        ( None, None, 'MULTIPOINT EMPTY'),
        ( None, None, 'MULTIPOINT EMPTY'),
        ( None, None, 'MULTIPOINT EMPTY'),
        ( None, None, 'MULTIPOINT (100 1010,101 1020)'),
        ( None, None, 'MULTIPOLYGON EMPTY'),
        ( None, None, 'MULTIPOLYGON EMPTY'),
        ( None, None, 'MULTIPOLYGON EMPTY'),
        ( None, None, 'MULTIPOLYGON (((110 1100,110 1000,100 1000,100 1100,110 1100)),((101 1010,109 1010,109 1090,101 1090,101 1010)))'),
        ( None, None, 'MULTILINESTRING EMPTY'),
        ( None, None, 'MULTILINESTRING EMPTY'),
        ( None, None, 'MULTILINESTRING ((100 1000,110 1000,110 1100))'),
        ( None, None, 'MULTILINESTRING ((100 1000,110 1000,110 1100,100 1100,100 1000))'),
        ( None, None, 'MULTILINESTRING ((100 1000,110 1000,110 1100,100 1100,100 1000),(101 1010,101 1090,109 1090,109 1010,101 1010))'),
    ]
    if lyr.GetFeatureCount() != len(expected_results):
        gdaltest.post_reason('failure')
        return 'fail'
    for i in range(len(expected_results)):
        feat = lyr.GetNextFeature()
        if feat.GetField('id') != expected_results[i][0] or \
           feat.GetField('name') != expected_results[i][1] or \
           feat.GetGeometryRef().ExportToWkt() != expected_results[i][2]:
            gdaltest.post_reason('failure at feat index %d' % i)
            feat.DumpReadable()
            print(expected_results[i])
            print(feat.GetField('name'))
            return 'fail'
    ds = None

    ds = ogr.Open('data/topojson2.topojson')
    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'a_layer':
        gdaltest.post_reason('failure')
        return 'fail'
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'LINESTRING (100 1000,110 1000,110 1100)') != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    lyr = ds.GetLayer(1)
    if lyr.GetName() != 'TopoJSON':
        gdaltest.post_reason('failure')
        return 'fail'
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'LINESTRING (100 1000,110 1000,110 1100)') != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    ds = ogr.Open('data/topojson3.topojson')
    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'a_layer':
        gdaltest.post_reason('failure')
        return 'fail'
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'LINESTRING (0 0,10 0,0 10,10 0,0 0)') != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    lyr = ds.GetLayer(1)
    if lyr.GetName() != 'TopoJSON':
        gdaltest.post_reason('failure')
        return 'fail'
    feat = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(feat, 'LINESTRING (0 0,10 0,0 10,10 0,0 0)') != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test 64bit support

def ogr_geojson_26():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature", "id": 1,
 "geometry": {"type":"Point","coordinates":[1,2]},
 "properties": { "intvalue" : 1, "int64" : 1234567890123, "intlist" : [1] }},
{"type": "Feature", "id": 1234567890123,
 "geometry": {"type":"Point","coordinates":[3,4]},
 "properties": { "intvalue" : 1234567890123, "intlist" : [1, 1234567890123] }},
 ]}""")
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr.GetMetadataItem(ogr.OLMD_FID64) is None:
        gdaltest.post_reason('fail')
        return 'fail'

    feature = lyr.GetNextFeature()
    if feature.GetFID() != 1:
        gdaltest.post_reason('fail')
        feature.DumpReadable()
        return 'fail'
    if feature.GetField("intvalue") != 1:
        gdaltest.post_reason('fail')
        feature.DumpReadable()
        return 'fail'
    if feature.GetField("int64") != 1234567890123:
        gdaltest.post_reason('fail')
        feature.DumpReadable()
        return 'fail'

    feature = lyr.GetNextFeature()
    if feature.GetFID() != 1234567890123:
        gdaltest.post_reason('fail')
        feature.DumpReadable()
        return 'fail'
    if feature.GetField("intvalue") != 1234567890123:
        gdaltest.post_reason('fail')
        feature.DumpReadable()
        return 'fail'
    if feature.GetField("intlist") != [1, 1234567890123]:
        gdaltest.post_reason('fail')
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    ds = gdaltest.geojson_drv.CreateDataSource('/vsimem/ogr_geojson_26.json')
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

    if data.find('{ "type": "Feature", "id": 1234567890123, "properties": { "int64": 1234567890123, "int64list": [ 1234567890123 ] }, "geometry": null }') < 0:
        gdaltest.post_reason('failure')
        print(data)
        return 'fail'

    return 'success'

###############################################################################
# Test workaround for 64bit values (returned as strings)

def ogr_geojson_27():

    if gdaltest.geojson_drv is None:
        return 'skip'

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
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')

    feature = lyr.GetNextFeature()
    if feature.GetField("intvalue") != 1:
        feature.DumpReadable()
        return 'fail'

    feature = lyr.GetNextFeature()
    if feature.GetField("intvalue") != 9223372036854775807:
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test reading ESRI point file with z value

def ogr_geojson_28():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/esrizpoint.json')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    # validate layer doesn't check z, but put it in
    extent = (2,2,49,49,1,1)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbPoint, 4, extent)
    if not rc:
        return 'fail'

    ref = lyr.GetSpatialRef()
    gcs = int(ref.GetAuthorityCode('GEOGCS'))

    if not gcs == 4326:
        gdaltest.post_reason("Spatial reference was not valid")
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POINT(2 49 1)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    if feature.GetFID() != 1:
        feature.DumpReadable()
        return 'fail'

    if feature.GetFieldAsInteger('fooInt') != 2:
        feature.DumpReadable()
        return 'fail'

    if feature.GetFieldAsDouble('fooDouble') != 3.4:
        feature.DumpReadable()
        return 'fail'

    if feature.GetFieldAsString('fooString') != '56':
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test reading ESRI linestring file with z

def ogr_geojson_29():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/esrizlinestring.json')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    # validate layer doesn't check z, but put it in
    extent = (2,3,49,50,1,2)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbLineString, 0, extent)
    if not rc:
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('LINESTRING (2 49 1,3 50 2)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test reading ESRI multipoint file with z

def ogr_geojson_30():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/esrizmultipoint.json')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    # validate layer doesn't check z, but put it in
    extent = (2,3,49,50,1,2)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiPoint, 4, extent)
    if not rc:
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOINT (2 49 1,3 50 2)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test reading ESRI polygon file with z

def ogr_geojson_31():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/esrizpolygon.json')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    # validate layer doesn't check z, but put it in
    extent = (2,3,49,50,1,4)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbPolygon, 0, extent)
    if not rc:
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POLYGON ((2 49 1,2 50 2,3 50 3,3 49 4,2 49 1))')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test reading ESRI multipoint file with m, but no z (hasM=true, hasZ omitted)

def ogr_geojson_32():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/esrihasmnozmultipoint.json')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (2,3,49,50)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiPoint, 4, extent)
    if not rc:
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOINT M ((2 49 1),(3 50 2))')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test reading ESRI multipoint file with hasZ=true, but only 2 components.

def ogr_geojson_33():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/esriinvalidhaszmultipoint.json')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (2,3,49,50)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiPoint, 4, extent)
    if not rc:
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOINT (2 49,3 50)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test reading ESRI multipoint file with z and m

def ogr_geojson_34():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/esrizmmultipoint.json')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (2,3,49,50)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiPoint, 4, extent)
    if not rc:
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOINT ZM ((2 49 1 100),(3 50 2 100))')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test handling of huge coordinates (#5377)

def ogr_geojson_35():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = gdaltest.geojson_drv.CreateDataSource('/vsimem/ogr_geojson_35.json')
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
    geom.AddPoint(-1.7e308 * 2, 1.7e308 * 2, 1.7e308 * 2) # evaluates to -inf, inf
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(3)
    geom = ogr.Geometry(ogr.wkbLineString)
    geom.AddPoint_2D(0,0)
    geom.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2) # evaluates to -inf, inf
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(4)
    geom = ogr.Geometry(ogr.wkbPolygon)
    geom2 = ogr.Geometry(ogr.wkbLinearRing)
    geom2.AddPoint_2D(0,0)
    geom2.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2) # evaluates to -inf, inf
    geom.AddGeometry(geom2)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(5)
    geom = ogr.Geometry(ogr.wkbMultiPoint)
    geom2 = ogr.Geometry(ogr.wkbPoint)
    geom2.AddPoint_2D(0,0)
    geom2 = ogr.Geometry(ogr.wkbPoint)
    geom2.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2) # evaluates to -inf, inf
    geom.AddGeometry(geom2)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(6)
    geom = ogr.Geometry(ogr.wkbMultiLineString)
    geom2 = ogr.Geometry(ogr.wkbLineString)
    geom2.AddPoint_2D(0,0)
    geom2 = ogr.Geometry(ogr.wkbLineString)
    geom2.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2) # evaluates to -inf, inf
    geom.AddGeometry(geom2)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(7)
    geom = ogr.Geometry(ogr.wkbMultiPolygon)
    geom2 = ogr.Geometry(ogr.wkbPolygon)
    geom3 = ogr.Geometry(ogr.wkbLinearRing)
    geom3.AddPoint_2D(0,0)
    geom2.AddGeometry(geom3)
    geom2 = ogr.Geometry(ogr.wkbPolygon)
    geom3 = ogr.Geometry(ogr.wkbLinearRing)
    geom3.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2) # evaluates to -inf, inf
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

    if data.find('-1.79') == -1 and data.find('e+308') == -1:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    for id in range(2,8):
        if data.find('{ "type": "Feature", "id": %d, "properties": { }, "geometry": null }' % id) == -1:
            gdaltest.post_reason('fail')
            print(data)
            return 'fail'

    return 'success'

###############################################################################
# Test reading file with UTF-8 BOM (which is supposed to be illegal in JSON...) (#5630)

def ogr_geojson_36():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/point_with_utf8bom.json')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'
    ds = None

    return 'success'

#########################################################################
# Test boolean type support

def ogr_geojson_37():

    if gdaltest.geojson_drv is None:
        return 'skip'

    # Test read support
    ds = ogr.Open("""{"type": "FeatureCollection","features": [
{ "type": "Feature", "properties": { "bool" : false, "not_bool": false, "bool_list" : [false, true], "notbool_list" : [false, 3]}, "geometry": null  },
{ "type": "Feature", "properties": { "bool" : true, "not_bool": 2, "bool_list" : [true] }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    if feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('bool')).GetType() != ogr.OFTInteger or \
       feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('bool')).GetSubType() != ogr.OFSTBoolean:
        gdaltest.post_reason('failure')
        return 'fail'
    if feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('not_bool')).GetSubType() != ogr.OFSTNone:
        gdaltest.post_reason('failure')
        return 'fail'
    if feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('bool_list')).GetType() != ogr.OFTIntegerList or \
       feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('bool_list')).GetSubType() != ogr.OFSTBoolean:
        gdaltest.post_reason('failure')
        return 'fail'
    if feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('notbool_list')).GetSubType() != ogr.OFSTNone:
        gdaltest.post_reason('failure')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetField('bool') != 0 or f.GetField('bool_list') != [0, 1]:
        gdaltest.post_reason('failure')
        f.DumpReadable()
        return 'fail'

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

    if data.find('"bool": false, "not_bool": 0, "bool_list": [ false, true ], "notbool_list": [ 0, 3 ]') < 0:
        gdaltest.post_reason('failure')
        print(data)
        return 'fail'

    return 'success'

###############################################################################
# Test datetime/date/time type support

def ogr_geojson_38():

    if gdaltest.geojson_drv is None:
        return 'skip'

    # Test read support
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "properties": { "dt": "2014-11-20 12:34:56+0100", "dt2": "2014\/11\/20", "date":"2014\/11\/20", "time":"12:34:56", "no_dt": "2014-11-20 12:34:56+0100", "no_dt2": "2014-11-20 12:34:56+0100" }, "geometry": null },
{ "type": "Feature", "properties": { "dt": "2014\/11\/20", "dt2": "2014\/11\/20T12:34:56Z", "date":"2014-11-20", "time":"12:34:56", "no_dt": "foo", "no_dt2": 1 }, "geometry": null }
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    if feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('dt')).GetType() != ogr.OFTDateTime:
        gdaltest.post_reason('failure')
        return 'fail'
    if feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('dt2')).GetType() != ogr.OFTDateTime:
        gdaltest.post_reason('failure')
        return 'fail'
    if feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('date')).GetType() != ogr.OFTDate:
        gdaltest.post_reason('failure')
        return 'fail'
    if feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('time')).GetType() != ogr.OFTTime:
        gdaltest.post_reason('failure')
        return 'fail'
    if feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('no_dt')).GetType() != ogr.OFTString:
        gdaltest.post_reason('failure')
        return 'fail'
    if feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('no_dt2')).GetType() != ogr.OFTString:
        gdaltest.post_reason('failure')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetField('dt') != '2014/11/20 12:34:56+01' or f.GetField('dt2') != '2014/11/20 00:00:00' or \
       f.GetField('date') != '2014/11/20' or f.GetField('time') != '12:34:56' :
        gdaltest.post_reason('failure')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetField('dt') != '2014/11/20 00:00:00' or f.GetField('dt2') != '2014/11/20 12:34:56+00' or \
       f.GetField('date') != '2014/11/20' or f.GetField('time') != '12:34:56' :
        gdaltest.post_reason('failure')
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test id top-object level

def ogr_geojson_39():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : "foo", "properties": { "bar" : "baz" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    if feat_defn.GetFieldDefn(0).GetName() != 'id' or feat_defn.GetFieldDefn(0).GetType() != ogr.OFTString:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != 'foo' or feat.GetField('bar') != 'baz':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # Crazy case: properties.id has the precedence because we arbitrarily decided that...
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : "foo", "properties": { "id" : 6 }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    if feat_defn.GetFieldDefn(0).GetName() != 'id' or feat_defn.GetFieldDefn(0).GetType() != ogr.OFTInteger:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != 6:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # Crazy case: properties.id has the precedence because we arbitrarily decided that...
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : "foo", "properties": { "id" : "baz" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    if feat_defn.GetFieldDefn(0).GetName() != 'id' or feat_defn.GetFieldDefn(0).GetType() != ogr.OFTString:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != 'baz':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # id and properties.ID (#6538)
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : 1, "properties": { "ID": 2 }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    if feat_defn.GetFieldDefn(0).GetName() != 'ID' or feat_defn.GetFieldDefn(0).GetType() != ogr.OFTInteger:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1 or feat.GetField('ID') != 2:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # Test handling of duplicated id
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : 1, "properties": { "foo": "bar" }, "geometry": null },
{ "type": "Feature", "id" : 1, "properties": { "foo": "baz" }, "geometry": null },
{ "type": "Feature", "id" : 2, "properties": { "foo": "baw" }, "geometry": null }
] }""")
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('expected warning')
        return 'fail'
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1 or feat.GetField('foo') != 'bar':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 2 or feat.GetField('foo') != 'baz':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 3 or feat.GetField('foo') != 'baw':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # negative id
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : -1, "properties": { "foo": "bar" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    if feat_defn.GetFieldDefn(0).GetName() != 'id' or feat_defn.GetFieldDefn(0).GetType() != ogr.OFTInteger:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != -1:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # negative id 64bit
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : -1234567890123, "properties": { "foo": "bar" }, "geometry": null },
{ "type": "Feature", "id" : -2, "properties": { "foo": "baz" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    if feat_defn.GetFieldDefn(0).GetName() != 'id' or feat_defn.GetFieldDefn(0).GetType() != ogr.OFTInteger64:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != -1234567890123:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # negative id
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : -2, "properties": { "foo": "baz" }, "geometry": null },
{ "type": "Feature", "id" : -1234567890123, "properties": { "foo": "bar" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    if feat_defn.GetFieldDefn(0).GetName() != 'id' or feat_defn.GetFieldDefn(0).GetType() != ogr.OFTInteger64:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != -2:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # positive and then negative id
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : 1, "properties": { "foo": "baz" }, "geometry": null },
{ "type": "Feature", "id" : -1, "properties": { "foo": "bar" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    if feat_defn.GetFieldDefn(1).GetName() != 'id' or feat_defn.GetFieldDefn(1).GetType() != ogr.OFTInteger:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != 1:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # mix of int and string id
    ds = ogr.Open("""{"type": "FeatureCollection", "features": [
{ "type": "Feature", "id" : -2, "properties": { "foo": "baz" }, "geometry": null },
{ "type": "Feature", "id" : "str", "properties": { "foo": "bar" }, "geometry": null },
{ "type": "Feature", "id" : -3, "properties": { "foo": "baz" }, "geometry": null },
] }""")
    lyr = ds.GetLayer(0)
    feat_defn = lyr.GetLayerDefn()
    if feat_defn.GetFieldDefn(0).GetName() != 'id' or feat_defn.GetFieldDefn(0).GetType() != ogr.OFTString:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField('id') != '-2':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test nested attributes

def ogr_geojson_40():

    if gdaltest.geojson_drv is None:
        return 'skip'

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
}""", gdal.OF_VECTOR, open_options = ['FLATTEN_NESTED_ATTRIBUTES=YES', 'NESTED_ATTRIBUTE_SEPARATOR=.'])
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    if feat.GetField('a_property') != 'foo' or feat.GetField('some_object.a_property') != 1 or \
       feat.GetField('some_object.another_property') != 2.34:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test ogr.CreateGeometryFromJson()

def ogr_geojson_41():

    if gdaltest.geojson_drv is None:
        return 'skip'

    # Check that by default we return a WGS 84 SRS
    g = ogr.CreateGeometryFromJson("{ 'type': 'Point', 'coordinates' : [ 2, 49] }")
    if g.ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason('fail')
        return 'fail'
    srs = g.GetSpatialReference()
    g = None

    if srs.ExportToWkt().find('WGS 84') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # But if a crs object is set (allowed originally, but not recommended!), we use it
    g = ogr.CreateGeometryFromJson('{ "type": "Point", "coordinates" : [ 2, 49], "crs": { "type": "name", "properties": { "name": "urn:ogc:def:crs:EPSG::4322" } } }')
    srs = g.GetSpatialReference()
    if srs.ExportToWkt().find('4322') < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # But if a crs object is set to null, set no crs
    g = ogr.CreateGeometryFromJson('{ "type": "Point", "coordinates" : [ 2, 49], "crs": null }')
    srs = g.GetSpatialReference()
    if srs:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test ESRI FeatureService scrolling

def ogr_geojson_42():

    if gdaltest.geojson_drv is None:
        return 'skip'

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

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

    gdal.FileFromMemBuffer('/vsimem/geojson/test.json?resultRecordCount=1', resultOffset0)
    ds = ogr.Open('/vsimem/geojson/test.json?resultRecordCount=1')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/geojson/test.json?resultRecordCount=1')

    gdal.FileFromMemBuffer('/vsimem/geojson/test.json?resultRecordCount=10', resultOffset0)
    gdal.PushErrorHandler()
    ds = ogr.Open('/vsimem/geojson/test.json?resultRecordCount=10')
    gdal.PopErrorHandler()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/geojson/test.json?resultRecordCount=10')

    gdal.FileFromMemBuffer('/vsimem/geojson/test.json?', resultOffset0)
    gdal.FileFromMemBuffer('/vsimem/geojson/test.json?resultRecordCount=1&resultOffset=0', resultOffset0)

    ds = ogr.Open('/vsimem/geojson/test.json?')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

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
    gdal.FileFromMemBuffer('/vsimem/geojson/test.json?resultRecordCount=1&resultOffset=1', resultOffset1)
    f = lyr.GetNextFeature()
    if f is None or f.GetFID() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    fc = lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    if fc != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/geojson/test.json?resultRecordCount=1&returnCountOnly=true',
"""{ "count": 123456}""")
    fc = lyr.GetFeatureCount()
    if fc != 123456:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    extent = lyr.GetExtent()
    gdal.PopErrorHandler()
    if extent != (2,2,49,49):
        gdaltest.post_reason('fail')
        print(extent)
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/geojson/test.json?resultRecordCount=1&returnExtentOnly=true&f=geojson',
"""{"type":"FeatureCollection","bbox":[1, 2, 3, 4],"features":[]}""")
    extent = lyr.GetExtent()
    if extent != (1.0, 3.0, 2.0, 4.0):
        gdaltest.post_reason('fail')
        print(extent)
        return 'fail'

    if lyr.TestCapability(ogr.OLCFastFeatureCount) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.TestCapability(ogr.OLCFastGetExtent) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.TestCapability('foo') != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test Feature without geometry

def ogr_geojson_43():
    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature", "properties": {"foo": "bar"}}]}""")
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    lyr = ds.GetLayerByName('OGRGeoJSON')

    feature = lyr.GetNextFeature()
    if feature.GetFieldAsString("foo") != 'bar':
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test null Feature (#6166)

def ogr_geojson_44():
    if gdaltest.geojson_drv is None:
        return 'skip'

    with gdaltest.error_handler():
        ogr.Open("""{"type": "FeatureCollection", "features":[ null ]}""")

    return 'success'

###############################################################################
# Test native data support

def ogr_geojson_45():
    if gdaltest.geojson_drv is None:
        return 'skip'

    # Test read support
    ds = gdal.OpenEx(
"""{"type": "FeatureCollection", "foo": "bar", "bar": "baz",
    "features":[ { "type": "Feature", "foo": "bar", "properties": { "myprop": "myvalue" }, "geometry": null } ]}""", gdal.OF_VECTOR, open_options = ['NATIVE_DATA=YES'])
    lyr = ds.GetLayer(0)
    native_data = lyr.GetMetadataItem("NATIVE_DATA", "NATIVE_DATA")
    if native_data != '{ "foo": "bar", "bar": "baz" }':
        gdaltest.post_reason('fail')
        print(native_data)
        return 'fail'
    native_media_type = lyr.GetMetadataItem("NATIVE_MEDIA_TYPE", "NATIVE_DATA")
    if native_media_type != 'application/vnd.geo+json':
        gdaltest.post_reason('fail')
        print(native_media_type)
        return 'fail'
    f = lyr.GetNextFeature()
    native_data = f.GetNativeData()
    if native_data != '{ "type": "Feature", "foo": "bar", "properties": { "myprop": "myvalue" }, "geometry": null }':
        gdaltest.post_reason('fail')
        print(native_data)
        return 'fail'
    native_media_type = f.GetNativeMediaType()
    if native_media_type != 'application/vnd.geo+json':
        gdaltest.post_reason('fail')
        print(native_media_type)
        return 'fail'

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_45.json')
    lyr = ds.CreateLayer('test', options = [
            'NATIVE_DATA={ "type": "ignored", "bbox": [ 0, 0, 0, 0 ], "foo": "bar", "bar": "baz", "features": "ignored" }',
            'NATIVE_MEDIA_TYPE=application/vnd.geo+json'] )
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

    if data.find('"bbox": [ 0, 1, 2, 0, 1, 2 ],') < 0 or \
       data.find('"foo": "bar"') < 0 or data.find('"bar": "baz"') < 0 or \
       data.find('"foo_feature": "bar_feature"') < 0 or \
       data.find('"foo_gc": "bar_gc"') < 0 or \
       data.find('"foo_point": "bar_point"') < 0 or data.find('3') < 0 or \
       data.find('"foo_linestring": "bar_linestring"') < 0 or data.find('4') < 0 or \
       data.find('"foo_multipoint": "bar_multipoint"') < 0 or data.find('5') < 0 or \
       data.find('"foo_multilinestring": "bar_multilinestring"') < 0 or data.find('6') < 0 or \
       data.find('"foo_polygon": "bar_polygon"') < 0 or data.find('7') < 0 or \
       data.find('"foo_multipolygon": "bar_multipolygon"') < 0 or data.find('8') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

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
""", open_options = ['NATIVE_DATA=YES'])
    gdal.VectorTranslate('/vsimem/out.json', src_ds, format = 'GeoJSON')

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"features": [
{ "type": "Feature", "id": "foobarbaz", "properties": { }, "geometry": null }
]
}
"""
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'

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
""", open_options = ['NATIVE_DATA=YES'])
    gdal.VectorTranslate('/vsimem/out.json', src_ds, format = 'GeoJSON')

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"features": [
{ "type": "Feature", "id": 1234657890123, "properties": { }, "geometry": null }
]
}
"""
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'

    return 'success'

###############################################################################
# Test that writing JSon content as value of a string field is serialized as it

def ogr_geojson_46():
    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_46.json')
    lyr = ds.CreateLayer('test' )
    lyr.CreateField(ogr.FieldDefn('myprop'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['myprop'] = '{ "a": "b" }'
    lyr.CreateFeature(f)
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_46.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_46.json')

    if data.find('{ "myprop": { "a": "b" } }') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    return 'success'

###############################################################################
# Test update support

def ogr_geojson_47():
    if gdaltest.geojson_drv is None:
        return 'skip'

    #ERROR 6: Update from inline definition not supported
    with gdaltest.error_handler():
        ds = ogr.Open('{"type": "FeatureCollection", "features":[]}', update = 1)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/ogr_geojson_47.json',
"""{"type": "FeatureCollection", "foo": "bar",
    "features":[ { "type": "Feature", "bar": "baz", "properties": { "myprop": "myvalue" }, "geometry": null } ]}""")

    # Test read support
    ds = ogr.Open('/vsimem/ogr_geojson_47.json', update = 1)
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

    gdal.Unlink('/vsimem/ogr_geojson_47.json')

    # we don't want crs if there's no in the source
    if data.find('"foo": "bar"') < 0 or data.find('"bar": "baz"') < 0 or \
       data.find('crs') >= 0 or \
       data.find('"myprop": "another_value"') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    return 'success'

###############################################################################
# Test update support with file that has a single feature not in a FeatureCollection

def ogr_geojson_48():
    if gdaltest.geojson_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_geojson_48.json',
"""{ "type": "Feature", "bar": "baz", "bbox": [2,49,2,49], "properties": { "myprop": "myvalue" }, "geometry": {"type": "Point", "coordinates": [ 2, 49]} }""")

    # Test read support
    ds = ogr.Open('/vsimem/ogr_geojson_48.json', update = 1)
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
    if data.find('"bar": "baz"') < 0 or \
       data.find('"bbox": [ 3.0, 50.0, 3.0, 50.0 ]') < 0 or \
       data.find('crs') >= 0 or \
       data.find('FeatureCollection') >= 0 or \
       data.find('"myprop": "another_value"') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    return 'success'

###############################################################################
# Test ARRAY_AS_STRING

def ogr_geojson_49():
    if gdaltest.geojson_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer('/vsimem/ogr_geojson_49.json',
"""{ "type": "Feature", "properties": { "foo": ["bar"] }, "geometry": null }""")

    # Test read support
    ds = gdal.OpenEx('/vsimem/ogr_geojson_49.json', open_options = ['ARRAY_AS_STRING=YES'])
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldDefn(0).GetType() != ogr.OFTString:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f['foo'] != '[ "bar" ]':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test that we serialize floating point values with enough significant figures

def ogr_geojson_50():
    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_50.json')
    lyr = ds.CreateLayer('test' )
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

    if data.find('1.23456789012456') < 0 and data.find('5268.813 ') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    # If SIGNIFICANT_FIGURES is explicitly specified, and COORDINATE_PRECISION not,
    # then it also applies to coordinates
    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_50.json')
    lyr = ds.CreateLayer('test', options = ['SIGNIFICANT_FIGURES=17'] )
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

    if data.find('1.23456789012456') < 0 and data.find('-5') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'


    # If SIGNIFICANT_FIGURES is explicitly specified, and COORDINATE_PRECISION too,
    # then SIGNIFICANT_FIGURES only applies to non-coordinates floating point values.
    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_50.json')
    lyr = ds.CreateLayer('test', options = ['COORDINATE_PRECISION=15', 'SIGNIFICANT_FIGURES=17'] )
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

    if data.find('0.00001234') < 0 or data.find('1.23456789012456') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'


    return 'success'

###############################################################################
# Test writing empty geometries

def ogr_geojson_51():
    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_51.json')
    lyr = ds.CreateLayer('test' )
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

    if data.find('{ "id": 1 }, "geometry": null') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    if data.find('{ "id": 2 }, "geometry": { "type": "LineString", "coordinates": [ ] } }') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    if data.find('{ "id": 3 }, "geometry": { "type": "Polygon", "coordinates": [ ] } }') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    if data.find('{ "id": 4 }, "geometry": { "type": "MultiPoint", "coordinates": [ ] } }') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    if data.find('{ "id": 5 }, "geometry": { "type": "MultiLineString", "coordinates": [ ] } }') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    if data.find('{ "id": 6 }, "geometry": { "type": "MultiPolygon", "coordinates": [ ] } }') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    if data.find('{ "id": 7 }, "geometry": { "type": "GeometryCollection", "geometries": [ ] } }') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    return 'success'

###############################################################################
# Test NULL type detection

def ogr_geojson_52():
    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('data/nullvalues.geojson')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    if ds.GetLayerCount() is not 1:
        gdaltest.post_reason('Wrong number of layers')
        return 'fail'

    lyr = ds.GetLayerByName('nullvalues')
    if lyr is None:
        gdaltest.post_reason('Missing layer called nullvalues')
        return 'fail'

    fld = lyr.GetLayerDefn().GetFieldDefn(0)
    if fld.GetNameRef() != 'int':
        return 'fail'
    if fld.GetType() != ogr.OFTInteger:
        return 'fail'
    fld = lyr.GetLayerDefn().GetFieldDefn(1)
    if fld.GetNameRef() != 'string':
        return 'fail'
    if fld.GetType() != ogr.OFTString:
        return 'fail'
    fld = lyr.GetLayerDefn().GetFieldDefn(2)
    if fld.GetNameRef() != 'double':
        return 'fail'
    if fld.GetType() != ogr.OFTReal:
        return 'fail'

    return 'success'

###############################################################################
# Test that M is ignored (this is a test of OGRLayer::CreateFeature() actually)

def ogr_geojson_53():
    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_53.json')
    lyr = ds.CreateLayer('test' )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT ZM (1 2 3 4)'))
    lyr.CreateFeature(f)
    ds = None

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_53.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_53.json')

    if data.find('{ "type": "Point", "coordinates": [ 1.0, 2.0, 3.0 ] }') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    return 'success'

###############################################################################

def ogr_geojson_cleanup():

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', None)

    try:
        if gdaltest.tests is not None:
            gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
            for i in range(len(gdaltest.tests)):

                fname = os.path.join('tmp', gdaltest.tests[i][0] + '.geojson')
                ogr.GetDriverByName('GeoJSON').DeleteDataSource( fname )

                fname = os.path.join('tmp', gdaltest.tests[i][0] + '.geojson.gz')
                gdal.Unlink(fname)

                fname = os.path.join('tmp', gdaltest.tests[i][0] + '.geojson.gz.properties')
                gdal.Unlink(fname)

            gdal.PopErrorHandler()

        gdaltest.tests = None
    except:
        pass

    try:
        os.remove('tmp/out_ogr_geojson_14.geojson')
    except:
        pass

    for f in gdal.ReadDir('/vsimem/geojson'):
        gdal.Unlink('/vsimem/geojson/' + f)

    return 'success'

###############################################################################
# Test NULL type detection when first value is null

def ogr_geojson_54():
    if gdaltest.geojson_drv is None:
        return 'skip'

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
    if fld.GetType() != ogr.OFTInteger:
        gdaltest.post_reason('fail')
        return 'fail'
    fld = lyr.GetLayerDefn().GetFieldDefn(1)
    if fld.GetType() != ogr.OFTString:
        gdaltest.post_reason('fail')
        return 'fail'
    fld = lyr.GetLayerDefn().GetFieldDefn(2)
    if fld.GetType() != ogr.OFTReal:
        gdaltest.post_reason('fail')
        return 'fail'
    fld = lyr.GetLayerDefn().GetFieldDefn(3)
    if fld.GetType() != ogr.OFTDateTime:
        gdaltest.post_reason('fail')
        return 'fail'
    fld = lyr.GetLayerDefn().GetFieldDefn(4)
    if fld.GetType() != ogr.OFTInteger:
        gdaltest.post_reason('fail')
        return 'fail'
    if fld.GetSubType() != ogr.OFSTBoolean:
        gdaltest.post_reason('fail')
        return 'fail'
    if fld.GetWidth() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    fld = lyr.GetLayerDefn().GetFieldDefn(5)
    if fld.GetType() != ogr.OFTString:
        gdaltest.post_reason('fail')
        return 'fail'
    return 'success'

###############################################################################
# Test RFC 7946

def read_file(filename):
    f = gdal.VSIFOpenL(filename, "rb")
    if f is None:
        return None
    content = gdal.VSIFReadL(1, 10000, f).decode('UTF-8')
    gdal.VSIFCloseL(f)
    return content

def ogr_geojson_55():
    if gdaltest.geojson_drv is None:
        return 'skip'

    # Basic test for standard bbox and coordinate truncation
    gdal.VectorTranslate('/vsimem/out.json', """{
   "type": "FeatureCollection",
  "features": [
      { "type": "Feature", "geometry": { "type": "Point", "coordinates": [2.123456789, 49] } },
      { "type": "Feature", "geometry": { "type": "Point", "coordinates": [3, 50] } }
  ]
}""", format = 'GeoJSON', layerCreationOptions = ['RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ 2.1234568, 49.0000000, 3.0000000, 50.0000000 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 2.1234568, 49.0, 2.1234568, 49.0 ], "geometry": { "type": "Point", "coordinates": [ 2.1234568, 49.0 ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 3.0, 50.0, 3.0, 50.0 ], "geometry": { "type": "Point", "coordinates": [ 3.0, 50.0 ] } }
]
}
"""
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'


    # Test polygon winding order
    gdal.VectorTranslate('/vsimem/out.json', """{
"type": "FeatureCollection",
"features": [
{ "type": "Feature", "geometry": { "type": "Polygon", "coordinates": [[[2,49],[3,49],[3,50],[2,50],[2,49]],[[2.1,49.1],[2.1,49.9],[2.9,49.9],[2.9,49.1],[2.1,49.1]]] } },
{ "type": "Feature", "geometry": { "type": "Polygon", "coordinates": [[[2,49],[2,50],[3,50],[3,49],[2,49]],[[2.1,49.1],[2.9,49.1],[2.9,49.9],[2.1,49.9],[2.1,49.1]]] } },
]
}
""", format = 'GeoJSON', layerCreationOptions = ['RFC7946=YES', 'WRITE_BBOX=YES'])

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
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'

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
""", open_options = ['NATIVE_DATA=YES'])
    gdal.VectorTranslate('/vsimem/out.json', src_ds, format = 'GeoJSON', layerCreationOptions = ['RFC7946=YES'])

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
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'

    return 'success'

###############################################################################
# Test RFC 7946 (that require geos)

def ogr_geojson_56():
    if gdaltest.geojson_drv is None:
        return 'skip'
    if not ogrtest.have_geos():
        return 'skip'

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
}""", format = 'GeoJSON', layerCreationOptions = ['RFC7946=YES', 'WRITE_BBOX=YES'])

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
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'


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
}""", format = 'GeoJSON', layerCreationOptions = ['RFC7946=YES', 'WRITE_BBOX=YES'])

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
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'

    return 'success'

###############################################################################
# Test RFC 7946 and reprojection

def ogr_geojson_57():
    if gdaltest.geojson_drv is None:
        return 'skip'
    if not ogrtest.have_geos():
        return 'skip'

    # Standard case: EPSG:32662: WGS 84 / Plate Carre
    src_ds = gdal.GetDriverByName('Memory').Create('',0,0,0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=eqc +lat_ts=0 +lat_0=0 +lon_0=0 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs = sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2000000 2000000,2000000 -2000000,-2000000 -2000000,-2000000 2000000,2000000 2000000))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format = 'GeoJSON', layerCreationOptions = ['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

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
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'

    # Polar case: EPSG:3995: WGS 84 / Arctic Polar Stereographic
    src_ds = gdal.GetDriverByName('Memory').Create('',0,0,0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=stere +lat_0=90 +lat_ts=71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs = sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2000000 2000000,2000000 -2000000,-2000000 -2000000,-2000000 2000000,2000000 2000000))'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((-2000000 -2000000,-1000000 -2000000,-1000000 2000000,-2000000 2000000,-2000000 -2000000))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format = 'GeoJSON', layerCreationOptions = ['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

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
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'

    # Polar case: slice of spherical cap (not intersecting antimeridian, west hemisphere)
    src_ds = gdal.GetDriverByName('Memory').Create('',0,0,0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=stere +lat_0=90 +lat_ts=71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs = sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((-2000000 2000000,0 0,-2000000 -2000000,-2000000 2000000))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format = 'GeoJSON', layerCreationOptions = ['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

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
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'

    # Polar case: slice of spherical cap (not intersecting antimeridian, east hemisphere)
    src_ds = gdal.GetDriverByName('Memory').Create('',0,0,0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=stere +lat_0=90 +lat_ts=71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs = sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((2000000 2000000,0 0,2000000 -2000000,2000000 2000000)))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format = 'GeoJSON', layerCreationOptions = ['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

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
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'

    # Polar case: slice of spherical cap crossing the antimeridian
    src_ds = gdal.GetDriverByName('Memory').Create('',0,0,0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=stere +lat_0=90 +lat_ts=71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs = sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((100000 100000,-100000 100000,0 0,100000 100000))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format = 'GeoJSON', layerCreationOptions = ['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

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
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'

    # Polar case: EPSG:3031: WGS 84 / Antarctic Polar Stereographic
    src_ds = gdal.GetDriverByName('Memory').Create('',0,0,0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=stere +lat_0=-90 +lat_ts=-71 +lon_0=0 +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs = sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((2000000 2000000,2000000 -2000000,-2000000 -2000000,-2000000 2000000,2000000 2000000)))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format = 'GeoJSON', layerCreationOptions = ['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

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
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'

    # Antimeridian case: EPSG:32660: WGS 84 / UTM zone 60N with polygon and line crossing
    src_ds = gdal.GetDriverByName('Memory').Create('',0,0,0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=utm +zone=60 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs = sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((670000 4000000,850000 4000000,850000 4100000,670000 4100000,670000 4000000))'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING((670000 4000000,850000 4100000))'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(670000 0,850000 0)'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format = 'GeoJSON', layerCreationOptions = ['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

    got = read_file('/vsimem/out.json')
    gdal.Unlink('/vsimem/out.json')
    expected = """{
"type": "FeatureCollection",
"bbox": [ 178.5275649, 0.0000000, -179.0681936, 37.0308258 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 178.8892102, 36.0816324, -179.0681936, 37.0308258 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ 180.0, 36.1071354 ], [ 180.0, 37.0082839 ], [ 178.9112998, 37.0308258 ], [ 178.8892102, 36.1298163 ], [ 180.0, 36.1071354 ] ] ], [ [ [ -179.0681936, 36.9810434 ], [ -180.0, 37.0082839 ], [ -180.0, 36.1071354 ], [ -179.1135277, 36.0816324 ], [ -179.0681936, 36.9810434 ] ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 178.8892102, 36.1298163, -179.0681936, 36.9810434 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 178.8892102, 36.1298163 ], [ 180.0, 36.5995612 ] ], [ [ -180.0, 36.5995612 ], [ -179.0681936, 36.9810434 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 178.5275649, 0.0, -179.8562277, 0.0 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 178.5275649, 0.0 ], [ 180.0, 0.0 ] ], [ [ -180.0, 0.0 ], [ -179.8562277, 0.0 ] ] ] } }
]
}
"""

    # with proj 4.9.3
    expected2 = """{
"type": "FeatureCollection",
"bbox": [ 178.5275649, 0.0000000, -179.0681936, 37.0308258 ],
"features": [
{ "type": "Feature", "properties": { }, "bbox": [ 178.8892102, 36.0816324, -179.0681936, 37.0308258 ], "geometry": { "type": "MultiPolygon", "coordinates": [ [ [ [ -179.0681936, 36.9810434 ], [ -180.0, 37.0082839 ], [ -180.0, 36.1071354 ], [ -179.1135277, 36.0816324 ], [ -179.0681936, 36.9810434 ] ] ], [ [ [ 178.8892102, 36.1298163 ], [ 180.0, 36.1071354 ], [ 180.0, 37.0082839 ], [ 178.9112998, 37.0308258 ], [ 178.8892102, 36.1298163 ] ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 178.8892102, 36.1298163, -179.0681936, 36.9810434 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 178.8892102, 36.1298163 ], [ 180.0, 36.5995612 ] ], [ [ -180.0, 36.5995612 ], [ -179.0681936, 36.9810434 ] ] ] } },
{ "type": "Feature", "properties": { }, "bbox": [ 178.5275649, 0.0, -179.8562277, 0.0 ], "geometry": { "type": "MultiLineString", "coordinates": [ [ [ 178.5275649, 0.0 ], [ 180.0, 0.0 ] ], [ [ -180.0, 0.0 ], [ -179.8562277, 0.0 ] ] ] } }
]
}
"""

    if json.loads(got) != json.loads(expected) and json.loads(got) != json.loads(expected2):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'


    # Antimeridian case: EPSG:32660: WGS 84 / UTM zone 60N wit polygon on west of antimeridian
    src_ds = gdal.GetDriverByName('Memory').Create('',0,0,0)
    sr = osr.SpatialReference()
    sr.SetFromUserInput('+proj=utm +zone=60 +datum=WGS84 +units=m +no_defs')
    lyr = src_ds.CreateLayer('test', srs = sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((670000 4000000,700000 4000000,700000 4100000,670000 4100000,670000 4000000))'))
    lyr.CreateFeature(f)

    gdal.VectorTranslate('/vsimem/out.json', src_ds, format = 'GeoJSON', layerCreationOptions = ['WRITE_NAME=NO', 'RFC7946=YES', 'WRITE_BBOX=YES'])

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
    if json.loads(got) != json.loads(expected):
        gdaltest.post_reason('fail')
        print(got)
        return 'fail'


    return 'success'

###############################################################################
# Test using the name member of FeatureCollection

def ogr_geojson_58():
    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('{ "type": "FeatureCollection", "name": "layer_name", "features": []}')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    lyr = ds.GetLayerByName('layer_name')
    if lyr is None:
        gdaltest.post_reason('Missing layer called layer_name')
        return 'fail'
    ds = None

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_58.json')
    lyr = ds.CreateLayer('foo')
    ds = None
    ds = ogr.Open('/vsimem/ogr_geojson_58.json')
    if ds.GetLayerByName('foo') is None:
        gdaltest.post_reason('Missing layer called foo')
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/ogr_geojson_58.json')

    return 'success'

###############################################################################
# Test using the description member of FeatureCollection

def ogr_geojson_59():
    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = ogr.Open('{ "type": "FeatureCollection", "description": "my_description", "features": []}')
    if ds is None:
        gdaltest.post_reason('Failed to open datasource')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetMetadataItem('DESCRIPTION') != 'my_description':
        gdaltest.post_reason('Did not get DESCRIPTION')
        return 'fail'
    ds = None

    ds = ogr.GetDriverByName('GeoJSON').CreateDataSource('/vsimem/ogr_geojson_59.json')
    lyr = ds.CreateLayer('foo', options = ['DESCRIPTION=my desc'])
    ds = None
    ds = ogr.Open('/vsimem/ogr_geojson_59.json')
    lyr = ds.GetLayerByName('foo')
    if lyr.GetMetadataItem('DESCRIPTION') != 'my desc':
        gdaltest.post_reason('Did not get DESCRIPTION')
        return 'fail'
    ds = None
    gdal.Unlink('/vsimem/ogr_geojson_59.json')

    return 'success'

###############################################################################
# Test null vs unset field

def ogr_geojson_60():
    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = gdal.OpenEx("""{ "type": "FeatureCollection", "features": [
{ "type": "Feature", "properties" : { "foo" : "bar" } },
{ "type": "Feature", "properties" : { "foo": null } },
{ "type": "Feature", "properties" : {  } } ] }""")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['foo'] != 'bar':
        gdaltest.post_reason('failure')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if not f.IsFieldNull('foo'):
        gdaltest.post_reason('failure')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.IsFieldSet('foo'):
        gdaltest.post_reason('failure')
        f.DumpReadable()
        return 'fail'

    # Test writing side
    gdal.VectorTranslate('/vsimem/ogr_geojson_60.json', ds, format = 'GeoJSON')

    fp = gdal.VSIFOpenL('/vsimem/ogr_geojson_60.json', 'rb')
    data = gdal.VSIFReadL(1, 10000, fp).decode('ascii')
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/ogr_geojson_60.json')
    if data.find('"properties": { "foo": "bar" }') < 0 or \
       data.find('"properties": { "foo": null }') < 0 or \
       data.find('"properties": { }') < 0:
        gdaltest.post_reason('failure')
        print(data)
        return 'fail'

    return 'success'


###############################################################################
# Test corner cases

def ogr_geojson_61():

    # Invalid JSon
    with gdaltest.error_handler():
        ds = gdal.OpenEx("""{ "type": "FeatureCollection", """)
    if ds is not None:
        gdaltest.post_reason('failure')
        return 'fail'

    # Invalid single geometry
    with gdaltest.error_handler():
        ds = gdal.OpenEx("""{ "type": "Point", "x" : { "coordinates" : null } } """)
    if ds is not None:
        gdaltest.post_reason('failure')
        return 'fail'

    return 'success'

###############################################################################
# Test crs object

def ogr_geojson_62():

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
    if srs.ExportToWkt().find('32631') < 0:
        gdaltest.post_reason('failure')
        return 'fail'

    # crs type=EPSG (not even documented in GJ2008 spec!) tests. Just for coverage completness
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
    if srs.ExportToWkt().find('32631') < 0:
        gdaltest.post_reason('failure')
        return 'fail'

    # crs type=link tests
    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"link" }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"link", "properties":null }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"link", "properties":1 }, "features":[] }""")

    gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"link", "properties":{"href":null} }, "features":[] }""")

    with gdaltest.error_handler():
        gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"link", "properties":{"href":1} }, "features":[] }""")

    with gdaltest.error_handler():
        gdal.OpenEx("""{ "type": "FeatureCollection", "crs": { "type":"link", "properties":{"href": "1"} }, "features":[] }""")


    # crs type=OGC (not even documented in GJ2008 spec!) tests. Just for coverage completness
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
    if srs.ExportToWkt().find('32631') < 0:
        gdaltest.post_reason('failure')
        return 'fail'

    return 'success'

###############################################################################
# Extensive test of field tye promotion

def ogr_geojson_63():

    ds_ref = ogr.Open('data/test_type_promotion_ref.json')
    lyr_ref = ds_ref.GetLayer(0)
    ds = ogr.Open('data/test_type_promotion.json')
    lyr = ds.GetLayer(0)
    return ogrtest.compare_layers(lyr, lyr_ref)

###############################################################################
# Test exporting XYM / XYZM (#6935)

def ogr_geojson_64():

    g = ogr.CreateGeometryFromWkt('POINT ZM(1 2 3 4)')
    if ogrtest.check_feature_geometry( ogr.CreateGeometryFromJson(g.ExportToJson()), 
                            ogr.CreateGeometryFromWkt('POINT Z(1 2 3)') ) != 0:
        return 'fail'

    g = ogr.CreateGeometryFromWkt('POINT M(1 2 3)')
    if ogrtest.check_feature_geometry( ogr.CreateGeometryFromJson(g.ExportToJson()), 
                            ogr.CreateGeometryFromWkt('POINT (1 2)') ) != 0:
        return 'fail'

    g = ogr.CreateGeometryFromWkt('LINESTRING ZM(1 2 3 4,5 6 7 8)')
    if ogrtest.check_feature_geometry( ogr.CreateGeometryFromJson(g.ExportToJson()), 
                            ogr.CreateGeometryFromWkt('LINESTRING Z(1 2 3,5 6 7)') ) != 0:
        return 'fail'

    g = ogr.CreateGeometryFromWkt('LINESTRING M(1 2 3,4 5 6)')
    if ogrtest.check_feature_geometry( ogr.CreateGeometryFromJson(g.ExportToJson()), 
                            ogr.CreateGeometryFromWkt('LINESTRING (1 2,4 5)') ) != 0:
        return 'fail'

    return 'success'

###############################################################################
# Test feature geometry CRS when CRS set on the FeatureCollection
# See https://github.com/r-spatial/sf/issues/449#issuecomment-319369945

def ogr_geojson_65():

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
    if pcs != 32631:
        gdaltest.post_reason('Spatial reference for individual geometry was not valid')
        return 'fail'

    return 'success'

gdaltest_list = [
    ogr_geojson_1,
    ogr_geojson_2,
    ogr_geojson_3,
    ogr_geojson_4,
    ogr_geojson_5,
    ogr_geojson_6,
    ogr_geojson_7,
    ogr_geojson_8,
    ogr_geojson_9,
    ogr_geojson_10,
    ogr_geojson_11,
    ogr_geojson_12,
    ogr_geojson_13,
    ogr_geojson_14,
    ogr_geojson_15,
    ogr_geojson_16,
    ogr_geojson_17,
    ogr_geojson_18,
    ogr_geojson_19,
    ogr_geojson_20,
    ogr_geojson_21,
    ogr_geojson_22,
    ogr_geojson_23,
    ogr_geojson_24,
    ogr_geojson_25,
    ogr_geojson_26,
    ogr_geojson_27,
    ogr_geojson_28,
    ogr_geojson_29,
    ogr_geojson_30,
    ogr_geojson_31,
    ogr_geojson_32,
    ogr_geojson_33,
    ogr_geojson_34,
    ogr_geojson_35,
    ogr_geojson_36,
    ogr_geojson_37,
    ogr_geojson_38,
    ogr_geojson_39,
    ogr_geojson_40,
    ogr_geojson_41,
    ogr_geojson_42,
    ogr_geojson_43,
    ogr_geojson_44,
    ogr_geojson_45,
    ogr_geojson_46,
    ogr_geojson_47,
    ogr_geojson_48,
    ogr_geojson_49,
    ogr_geojson_50,
    ogr_geojson_51,
    ogr_geojson_52,
    ogr_geojson_53,
    ogr_geojson_54,
    ogr_geojson_55,
    ogr_geojson_56,
    ogr_geojson_57,
    ogr_geojson_58,
    ogr_geojson_59,
    ogr_geojson_60,
    ogr_geojson_61,
    ogr_geojson_62,
    ogr_geojson_63,
    ogr_geojson_64,
    ogr_geojson_65,
    ogr_geojson_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_geojson' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
