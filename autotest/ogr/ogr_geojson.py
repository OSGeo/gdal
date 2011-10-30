#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GeoJSON driver test suite.
# Author:   Mateusz Loskot <mateusz@loskot.net>
# 
###############################################################################
# Copyright (c) 2007, Mateusz Loskot <mateusz@loskot.net>
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
import string

sys.path.append( '../pymod' )


try:
    from osgeo import osr
    from osgeo import ogr
    from osgeo import gdal
except ImportError:
    
    import ogr
    import osr
    import gdal

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

    geom = ogr.CreateGeometryFromWkt("POINT(1 2)")
    feature.SetGeometry(geom)

    try:
        out = feature.ExportToJson()
    except ImportError:
        return 'skip'

    expected_out = """{"geometry": {"type": "Point", "coordinates": [1.0, 2.0]}, "type": "Feature", "properties": {"foo": "bar"}, "id": -1}"""

    if out != expected_out:
        print(out)
        return 'fail'


    out = feature.ExportToJson(as_object = True)
    expected_out = {'geometry': {'type': 'Point', 'coordinates': [1.0, 2.0]}, 'type': 'Feature', 'properties': {'foo': 'bar'}, 'id': -1}

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

    extent = (2,3,49,50)

    rc = validate_layer(lyr, 'OGRGeoJSON', 1, ogr.wkbPolygon, 0, extent)
    if rc is not True:
        return 'fail'

    feature = lyr.GetNextFeature()
    ref_geom = ogr.CreateGeometryFromWkt('POLYGON ((2 49,2 50,3 50,3 49,2 49))')
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

        lyr = ds.GetLayerByName('OGRGeoJSON') 
        if lyr is None: 
            gdaltest.post_reason('Missing layer called OGRGeoJSON') 
            return 'fail' 
            
        gdal.Unlink('/vsimem/testgj')

    return 'success' 

###############################################################################

def ogr_geojson_cleanup():

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
    ogr_geojson_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_geojson' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

