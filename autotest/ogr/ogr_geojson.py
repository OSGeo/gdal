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
            orig_feat.Destroy()
            feat.Destroy()
            return False

        if ogrtest.check_feature_geometry( feat, orig_feat.GetGeometryRef(),
                                           max_error = 0.001) != 0:
            print('Geometry test failed')
            orig_feat.Destroy()
            feat.Destroy()
            gdaltest.gjpoint_feat = None
            return False

        orig_feat.Destroy()
        feat.Destroy()

    gdaltest.gjpoint_feat = None

    lyr = None

    # Required by old-gen python bindings
    ds.Destroy()

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

    dst_feat.Destroy()

    shp_lyr = None
    lyr = None

    # Required by old-gen python bindings
    shp_ds.Destroy()
    ds.Destroy()

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

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 100.0, 0.0, 0.0)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbPoint, 0, extent)
    if rc is not True:
        return 'fail'

    lyr = None
    # Required by old-gen python bindings
    ds.Destroy()

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

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 101.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbLineString, 0, extent)
    if rc is not True:
        return 'fail'
        
    lyr = None
    # Required by old-gen python bindings
    ds.Destroy()

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

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 101.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbPolygon, 0, extent)
    if rc is not True:
        return 'fail'

    lyr = None
    # Required by old-gen python bindings
    ds.Destroy()

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

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 102.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbGeometryCollection, 0, extent)
    if rc is not True:
        return 'fail'

    lyr = None
    # Required by old-gen python bindings
    ds.Destroy()

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

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 101.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiPoint, 0, extent)
    if rc is not True:
        return 'fail'

    lyr = None
    # Required by old-gen python bindings
    ds.Destroy()

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

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 103.0, 0.0, 3.0)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiLineString, 0, extent)
    if rc is not True:
        return 'fail'

    lyr = None
    # Required by old-gen python bindings
    ds.Destroy()

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

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'

    extent = (100.0, 103.0, 0.0, 3.0)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiPolygon, 0, extent)
    if rc is not True:
        return 'fail'

    lyr = None
    # Required by old-gen python bindings
    ds.Destroy()

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
        if rc is False:
            gdaltest.post_reason('Failed making copy of ' + test[0] +'.shp')
            return 'fail'

        rc = verify_geojson_copy(test[0], test[1], test[2])
        if rc is False:
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
        if rc is False:
            gdaltest.post_reason('Failed making copy of ' + test[0] +'.shp')
            return 'fail'

        rc = verify_geojson_copy(test[0], test[1], test[2])
        if rc is False:
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

    lyr = ds.GetLayerByName('OGRGeoJSON')
    if lyr is None:
        gdaltest.post_reason('Missing layer called OGRGeoJSON')
        return 'fail'


    extent = (100.0, 102.0, 0.0, 1.0)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbGeometryCollection, 0, extent)
    if rc is not True:
        return 'fail'

    ref = lyr.GetSpatialRef()
    gcs = int(ref.GetAuthorityCode('GEOGCS'))
    pcs = ref.GetAuthorityCode('PROJCS')
    if pcs:
        pcs = int(pcs)
        
    if  not gcs == 4326 and not pcs == 26915:
        gdaltest.post_reason("Spatial reference was not valid")
        return 'fail'

    feature = lyr.GetNextFeature()
    geometry = feature.GetGeometryRef().GetGeometryRef(0)
    
    srs = geometry.GetSpatialReference()
    gcs = int(srs.GetAuthorityCode('GEOGCS'))
    pcs = srs.GetAuthorityCode('PROJCS')
    if not gcs == 4269 and not pcs == 26916:
        gdaltest.post_reason("Spatial reference for individual geometry was not valid")
        return 'fail'

    lyr = None
    # Required by old-gen python bindings
    ds.Destroy()

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
    if rc is False:
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
    ds.Destroy()
    out_ds.Destroy()

    return 'success'

###############################################################################
# Test Feature.ExportToJson (#3870)

def ogr_geojson_15():

    feature_defn = ogr.FeatureDefn()
    feature_defn.AddFieldDefn(ogr.FieldDefn("foo"))

    feature = ogr.Feature(feature_defn)
    feature.SetField("foo", "bar")
    feature.SetFID(0)

    geom = ogr.CreateGeometryFromWkt("POINT(1 2)")
    feature.SetGeometry(geom)

    try:
        out = feature.ExportToJson()
    except ImportError:
        return 'skip'

    expected_out = """{"geometry": {"type": "Point", "coordinates": [1.0, 2.0]}, "type": "Feature", "properties": {"foo": "bar"}, "id": 0}"""

    if out != expected_out:
        print(out)
        return 'fail'


    out = feature.ExportToJson(as_object = True)
    expected_out = {'geometry': {'type': 'Point', 'coordinates': [1.0, 2.0]}, 'type': 'Feature', 'properties': {'foo': 'bar'}, 'id': 0}

    if out != expected_out:
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
    if rc is not True:
        return 'fail'

    ref = lyr.GetSpatialRef()
    gcs = int(ref.GetAuthorityCode('GEOGCS'))

    if  not gcs == 4326 :
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
    if rc is not True:
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('LINESTRING (2 49,3 50)')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
        feature.DumpReadable()
        return 'fail'

    lyr = None
    ds = None

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
    if rc is not True:
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('MULTIPOLYGON (((2 49,2 50,3 50,3 49,2 49),(2.1 49.1,2.1 49.9,2.9 49.9,2.9 49.1,2.1 49.1)),((-2 49,-2 50,-3 50,-3 49,-2 49)))')
    if ogrtest.check_feature_geometry(feature, ref_geom) != 0:
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
    if rc is not True:
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
        f = open(gj, 'rb')
        data = f.read()
        #print(gj)
        #print(data.decode('LATIN1'))
        f.close()
        
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
# Test reading outpout of geocouch spatiallist

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

    if data.find('"bbox": [ 1.0, 10.0, 2.0, 20.0 ]') == -1:
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
        feature.DumpReadable()
        return 'fail'
    if feature.GetField("intvalue") != 1:
        feature.DumpReadable()
        return 'fail'
    if feature.GetField("int64") != 1234567890123:
        feature.DumpReadable()
        return 'fail'

    feature = lyr.GetNextFeature()
    if feature.GetFID() != 1234567890123:
        feature.DumpReadable()
        return 'fail'
    if feature.GetField("intvalue") != 1234567890123:
        feature.DumpReadable()
        return 'fail'
    if feature.GetField("intlist") != [1, 1234567890123]:
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

    ds = ogr.Open("""{"type": "FeatureCollection", "features":[
{"type": "Feature",
 "geometry": {"type":"Point","coordinates":[1,2]},
 "properties": { "intvalue" : 1 }},
{"type": "Feature",
 "geometry": {"type":"Point","coordinates":[3,4]},
 "properties": { "intvalue" : 12345678901231234567890123 }},
 ]}""")
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
    if rc is not True:
        return 'fail'

    ref = lyr.GetSpatialRef()
    gcs = int(ref.GetAuthorityCode('GEOGCS'))

    if  not gcs == 4326 :
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
    if rc is not True:
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
    if rc is not True:
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
    if rc is not True:
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

    # validate layer doesn't check z, but put it in
    extent = (2,3,49,50)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiPoint, 4, extent)
    if rc is not True:
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
# Test reading ESRI multipoint file with hasZ=true, but only 2 points.

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

    # validate layer doesn't check z, but put it in
    extent = (2,3,49,50)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiPoint, 4, extent)
    if rc is not True:
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
# Test reading ESRI multipoint file with m, but no z (hasM=true, hasZ omitted)

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

    # validate layer doesn't check z, but put it in
    extent = (2,3,49,50)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbMultiPoint, 4, extent)
    if rc is not True:
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
# Test handling of huge coordinates (#5377)

def ogr_geojson_35():

    if gdaltest.geojson_drv is None:
        return 'skip'

    ds = gdaltest.geojson_drv.CreateDataSource('/vsimem/ogr_geojson_35.json')
    lyr = ds.CreateLayer('foo')
    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.Geometry(ogr.wkbPoint)
    geom.AddPoint_2D(-1.79769313486231571e+308, -1.79769313486231571e+308)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)
    
    gdal.PushErrorHandler('CPLQuietErrorHandler')

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.Geometry(ogr.wkbPoint)
    geom.AddPoint(-1.7e308 * 2, 1.7e308 * 2, 1.7e308 * 2) # evaluates to -inf, inf
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.Geometry(ogr.wkbLineString)
    geom.AddPoint_2D(0,0)
    geom.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2) # evaluates to -inf, inf
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.Geometry(ogr.wkbPolygon)
    geom2 = ogr.Geometry(ogr.wkbLinearRing)
    geom2.AddPoint_2D(0,0)
    geom2.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2) # evaluates to -inf, inf
    geom.AddGeometry(geom2)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.Geometry(ogr.wkbMultiPoint)
    geom2 = ogr.Geometry(ogr.wkbPoint)
    geom2.AddPoint_2D(0,0)
    geom2 = ogr.Geometry(ogr.wkbPoint)
    geom2.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2) # evaluates to -inf, inf
    geom.AddGeometry(geom2)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.Geometry(ogr.wkbMultiLineString)
    geom2 = ogr.Geometry(ogr.wkbLineString)
    geom2.AddPoint_2D(0,0)
    geom2 = ogr.Geometry(ogr.wkbLineString)
    geom2.AddPoint_2D(-1.7e308 * 2, 1.7e308 * 2) # evaluates to -inf, inf
    geom.AddGeometry(geom2)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
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
    #print(data)

    gdal.Unlink('/vsimem/ogr_geojson_35.json')

    if data.find('-1.79') == -1 and data.find('e+308') == -1:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('"type": "Point", "coordinates": null }') == -1:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('"type": "LineString", "coordinates": null }') == -1:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('"type": "Polygon", "coordinates": null }') == -1:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('"type": "MultiPoint", "coordinates": null }') == -1:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('"type": "MultiLineString", "coordinates": null }') == -1:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'
    if data.find('"type": "MultiPolygon", "coordinates": null }') == -1:
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
    gdal.FileFromMemBuffer('/vsimem/geojson/test.json?', resultOffset0)
    gdal.FileFromMemBuffer('/vsimem/geojson/test.json?resultRecordCount=1000&resultOffset=0', resultOffset0)

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
    gdal.FileFromMemBuffer('/vsimem/geojson/test.json?resultRecordCount=1000&resultOffset=1', resultOffset1)
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

    gdal.FileFromMemBuffer('/vsimem/geojson/test.json?resultRecordCount=1000&returnCountOnly=true',
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

    gdal.FileFromMemBuffer('/vsimem/geojson/test.json?resultRecordCount=1000&returnExtentOnly=true&f=geojson',
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
    ogr_geojson_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_geojson' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

