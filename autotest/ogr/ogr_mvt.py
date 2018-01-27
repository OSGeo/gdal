#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR MVT driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2018, Even Rouault <even dot rouault at spatialys dot com>
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

import json
import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr

###############################################################################

def ogr_mvt_datatypes():

    # With metadata.json
    ds = ogr.Open('data/mvt/datatypes/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['bool_false'] != 0 or \
       f['bool_true'] != 1 or \
       f['pos_int_value'] != 1 or \
       f['pos_int64_value'] != 123456789012345 or \
       f['neg_int_value'] != -1 or \
       f['neg_int64_value'] != -123456789012345 or \
       f['pos_sint_value'] != 1 or \
       f['pos_sint64_value'] != 123456789012345 or \
       f['neg_sint_value'] != -1 or \
       f['neg_sint64_value'] != -123456789012345 or \
       f['uint_value'] != 2000000000 or \
       f['uint64_value'] != 4000000000 or \
       f['float_value'] != 1.25 or \
       f['real_value'] != 1.23456789 or \
       f['string_value'] != 'str':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Without metadata.json
    ds = gdal.OpenEx('data/mvt/datatypes/0/0/0.pbf',
                     open_options = ['METADATA_FILE='])
    lyr = ds.GetLayer(0)

    count = lyr.GetLayerDefn().GetFieldCount()
    if count != 16:
        gdaltest.post_reason('fail')
        print(count)
        return 'fail'

    tab = []
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        fld_defn = lyr.GetLayerDefn().GetFieldDefn(i)
        tab += [ (fld_defn.GetName(), fld_defn.GetType(),
                  fld_defn.GetSubType()) ]
    expected_tab = [
        ('mvt_id', ogr.OFTInteger64, ogr.OFSTNone),
        ('bool_true', ogr.OFTInteger, ogr.OFSTBoolean),
        ('bool_false', ogr.OFTInteger, ogr.OFSTBoolean),
        ('pos_int_value', ogr.OFTInteger, ogr.OFSTNone),
        ('pos_int64_value', ogr.OFTInteger64, ogr.OFSTNone),
        ('neg_int_value', ogr.OFTInteger, ogr.OFSTNone),
        ('neg_int64_value', ogr.OFTInteger64, ogr.OFSTNone),
        ('pos_sint_value', ogr.OFTInteger, ogr.OFSTNone),
        ('pos_sint64_value', ogr.OFTInteger64, ogr.OFSTNone),
        ('neg_sint_value', ogr.OFTInteger, ogr.OFSTNone),
        ('neg_sint64_value', ogr.OFTInteger64, ogr.OFSTNone),
        ('uint_value', ogr.OFTInteger, ogr.OFSTNone),
        ('uint64_value', ogr.OFTInteger64, ogr.OFSTNone),
        ('float_value', ogr.OFTReal, ogr.OFSTFloat32),
        ('real_value', ogr.OFTReal, ogr.OFSTNone),
        ('string_value', ogr.OFTString, ogr.OFSTNone),
    ]
    if tab != expected_tab:
        gdaltest.post_reason('fail')
        print(tab)
        print(expected_tab)
        return 'fail'

    f = lyr.GetNextFeature()
    if f['bool_false'] != 0 or \
       f['bool_true'] != 1 or \
       f['pos_int_value'] != 1 or \
       f['pos_int64_value'] != 123456789012345 or \
       f['neg_int_value'] != -1 or \
       f['neg_int64_value'] != -123456789012345 or \
       f['pos_sint_value'] != 1 or \
       f['pos_sint64_value'] != 123456789012345 or \
       f['neg_sint_value'] != -1 or \
       f['neg_sint64_value'] != -123456789012345 or \
       f['uint_value'] != 2000000000 or \
       f['uint64_value'] != 4000000000 or \
       f['float_value'] != 1.25 or \
       f['real_value'] != 1.23456789 or \
       f['string_value'] != 'str':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_datatype_promotion():

    ds = ogr.Open('data/mvt/datatype_promotion.pbf')
    tab = [ ('int_to_int64', ogr.OFTInteger64),
            ('int_to_real', ogr.OFTReal),
            ('int64_to_real', ogr.OFTReal),
            ('bool_to_int', ogr.OFTInteger),
            ('bool_to_str', ogr.OFTString),
            ('float_to_double', ogr.OFTReal) ]
    for layer_name, dt in tab:
        lyr = ds.GetLayerByName(layer_name)
        fld_defn = lyr.GetLayerDefn().GetFieldDefn(1)
        if fld_defn.GetType() != dt:
            gdaltest.post_reason('fail')
            print(layer_name)
            print(fld_defn.GetType(), dt)
            return 'fail'
        if fld_defn.GetSubType() != ogr.OFSTNone:
            gdaltest.post_reason('fail')
            print(layer_name)
            return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_limit_cases():

    with gdaltest.error_handler():
        ds = ogr.Open('data/mvt/limit_cases.pbf')

    lyr = ds.GetLayerByName('empty')
    if lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayerByName('layer1')
    if lyr.GetFeatureCount() != 7:
        gdaltest.post_reason('fail')
        return 'fail'

    f = lyr.GetFeature(1)
    if f['mvt_id'] != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        f = lyr.GetFeature(6)
        if f['b'] != 1:
            gdaltest.post_reason('fail')
            return 'fail'

    lyr = ds.GetLayerByName('layer2')
    if lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayerByName('layer3')
    if lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayerByName('layer4')
    if lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayerByName('layer5')
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2070 2690)':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_mixed():

    ds = ogr.Open('data/mvt/mixed/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTIPOINT ((215246.671651058 6281289.23636264),(332653.947097085 6447616.20991119))') != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'LINESTRING (215246.671651058 6281289.23636264,332653.947097085 6447616.20991119)') != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_linestring():

    ds = ogr.Open('data/mvt/linestring/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTILINESTRING ((215246.671651058 6281289.23636264,332653.947097085 6447616.20991119))') != 0:
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_multilinestring():

    ds = ogr.Open('data/mvt/multilinestring/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTILINESTRING ((215246.671651058 6281289.23636264,332653.947097085 6447616.20991119),(440277.282922614 6623727.12308023,547900.618748143 6809621.97586978))') != 0:
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_point_polygon():

    ds = ogr.Open('data/mvt/point_polygon/0')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTIPOINT ((215246.671651058 6281289.23636264))') != 0:
        f.DumpReadable()
        return 'fail'
    lyr = ds.GetLayer(1)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTIPOLYGON (((440277.282922614 450061.222543117,440277.282922614 -440277.282922614,0.0 -440277.282922614,0.0 -215246.671651058,215246.671651058 -215246.671651058,215246.671651058 225030.61127156,0.0 225030.61127156,0.0 450061.222543117,440277.282922614 450061.222543117)),((0.0 117407.275446031,0.0 -107623.335825529,-117407.275446031 -107623.335825529,-117407.275446031 117407.275446031,0.0 117407.275446031)),((107623.335825529 58703.6377230138,107623.335825529 -48919.6981025115,48919.6981025115 -48919.6981025115,48919.6981025115 58703.6377230138,107623.335825529 58703.6377230138)))') != 0:
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_point_polygon_clip():

    if not ogrtest.have_geos() or gdal.GetConfigOption('OGR_MVT_CLIP') is not None:
        return 'skip'

    if gdal.GetConfigOption('APPVEYOR') is not None:
        return 'skip'
    if sys.platform == 'darwin' and gdal.GetConfigOption('TRAVIS', None) is not None:
        return 'skip'

    ds = ogr.Open('data/mvt/point_polygon/1')
    lyr = ds.GetLayer(1)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTIPOLYGON (((445169.252732867 450061.222543117,445169.252732867 0.0,220138.641461308 0.0,220138.641461308 225030.61127156,0.0 225030.61127156,0.0 450061.222543117,445169.252732867 450061.222543117)),((107623.335825528 58703.6377230138,107623.335825528 0.0,53811.6679127641 0.0,53811.6679127641 58703.6377230138,107623.335825528 58703.6377230138)))') != 0 and \
       ogrtest.check_feature_geometry(f, 'MULTIPOLYGON (((445169.252732867 0.0,445169.252732867 -445169.252732867,0.0 -445169.252732867,0.0 -220138.641461308,220138.641461308 -220138.641461308,220138.641461308 0.0,445169.252732867 0.0)),((107623.335825528 0.0,107623.335825528 -53811.6679127641,53811.6679127641 -53811.6679127641,53811.6679127641 0.0,107623.335825528 0.0)))') != 0:
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_tileset_without_readdir():

    with gdaltest.config_option('MVT_USE_READDIR', 'NO'):
        ds = gdal.OpenEx('data/mvt/linestring/0')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None:
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_tileset_without_metadata_file():

    ds = gdal.OpenEx('data/mvt/point_polygon/1',
                     open_options = ['METADATA_FILE=', 'CLIP=NO'])
    lyr = ds.GetLayerByName('point')
    if lyr.GetGeomType() != ogr.wkbMultiPoint:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayerByName('polygon2')
    if lyr.GetGeomType() != ogr.wkbMultiPolygon:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_tileset_json_field():

    ds = gdal.OpenEx('data/mvt/datatypes/0',
                     open_options = ['METADATA_FILE=', 'JSON_FIELD=YES', 'CLIP=NO'])
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    d = json.loads(f.GetFieldAsString("json"))
    if d != {
        "bool_true":True,
        "bool_false":False,
        "pos_int_value":1,
        "pos_int64_value":123456789012345,
        "neg_int_value":-1,
        "neg_int64_value":-123456789012345,
        "pos_sint_value":1,
        "pos_sint64_value":123456789012345,
        "neg_sint_value":-1,
        "neg_sint64_value":-123456789012345,
        "uint_value":2000000000,
        "uint64_value":4000000000,
        "float_value":1.25,
        "real_value":1.23456789,
        "string_value":"str"
        }:
        gdaltest.post_reason('fail')
        print(f.GetFieldAsString("json"))
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_open_variants():

    expected_geom = 'MULTILINESTRING ((215246.671651058 6281289.23636264,332653.947097085 6447616.20991119))'

    ds = ogr.Open('MVT:data/mvt/linestring/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, expected_geom) != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    ds = ogr.Open('MVT:data/mvt/linestring/0')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, expected_geom) != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    ds = ogr.Open('/vsigzip/data/mvt/linestring/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, expected_geom) != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    ds = ogr.Open('MVT:/vsigzip/data/mvt/linestring/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, expected_geom) != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_xyz_options():

    ds = gdal.OpenEx('data/mvt/datatypes/0/0/0.pbf',
                     open_options = ['X=1', 'Y=2', 'Z=3'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'POINT (-12496536.8802869 8299226.7830913)') != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_test_ogrsf_pbf():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() +
                                    ' -ro data/mvt/datatypes/0/0/0.pbf')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_test_ogrsf_directory():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() +
                                    ' -ro data/mvt/datatypes/0')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_mbtiles():

    if ogr.GetDriverByName('MBTILES') is None:
        return 'skip'

    ds = ogr.Open('data/mvt/point_polygon.mbtiles')
    lyr = ds.GetLayerByName('point')
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTIPOINT ((220138.641461308 6276397.26655239))') != 0:
        f.DumpReadable()
        return 'fail'

    lyr.SetSpatialFilterRect(0,0,10000000,10000000)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTIPOINT ((220138.641461308 6276397.26655239))') != 0:
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_mbtiles_json_field():

    if ogr.GetDriverByName('MBTILES') is None:
        return 'skip'

    ds = gdal.OpenEx('data/mvt/datatypes.mbtiles',
                     open_options = ['JSON_FIELD=YES', 'CLIP=NO'])
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    d = json.loads(f.GetFieldAsString("json"))
    if d != {'int64_value': 123456789012345,
             'string_value': 'str',
             'real_value': 1.23456789,
             'bool_false': False,
             'pos_int_value': 1,
             'neg_int_value': -1,
             'bool_true': True,
             'float_value': 1.25
             }:
        gdaltest.post_reason('fail')
        print(f.GetFieldAsString("json"))
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_mbtiles_json_field_auto():

    if ogr.GetDriverByName('MBTILES') is None:
        return 'skip'

    ds = gdal.OpenEx('data/mvt/datatypes_json_field_auto.mbtiles',
                     open_options = ['CLIP=NO'])
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    d = json.loads(f.GetFieldAsString("json"))
    if d != {'int64_value': 123456789012345,
             'string_value': 'str',
             'real_value': 1.23456789,
             'bool_false': False,
             'pos_int_value': 1,
             'neg_int_value': -1,
             'bool_true': True,
             'float_value': 1.25
             }:
        gdaltest.post_reason('fail')
        print(f.GetFieldAsString("json"))
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_mbtiles_test_ogrsf():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    if ogr.GetDriverByName('MBTILES') is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() +
                                    ' -ro data/mvt/point_polygon.mbtiles polygon2')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_mbtiles_open_vector_in_raster_mode():

    if ogr.GetDriverByName('MBTILES') is None:
        return 'skip'

    ds = gdal.OpenEx('data/mvt/datatypes.mbtiles', gdal.OF_RASTER)
    if ds is not None:
        return 'fail'

    return 'success'

###############################################################################

def ogr_mvt_x_y_z_filename_scheme():

    tmpfilename ='/vsimem/0-0-0.pbf'
    gdal.FileFromMemBuffer(tmpfilename,
        open('data/mvt/linestring/0/0/0.pbf', 'rb').read() )
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'LINESTRING (215246.671651058 6281289.23636264,332653.947097085 6447616.20991119)') != 0:
        f.DumpReadable()
        return 'fail'
    ds = None
    gdal.Unlink(tmpfilename)

    return 'success'

###############################################################################

def ogr_mvt_polygon_larger_than_header():

    ds = gdal.OpenEx('data/mvt/polygon_larger_than_header.pbf', 
                     open_options = ['CLIP=NO'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None:
        return 'fail'

    return 'success'

###############################################################################
#

gdaltest_list = [
    ogr_mvt_datatypes,
    ogr_mvt_datatype_promotion,
    ogr_mvt_limit_cases,
    ogr_mvt_mixed,
    ogr_mvt_linestring,
    ogr_mvt_multilinestring,
    ogr_mvt_point_polygon,
    ogr_mvt_point_polygon_clip,
    ogr_mvt_tileset_without_readdir,
    ogr_mvt_tileset_without_metadata_file,
    ogr_mvt_tileset_json_field,
    ogr_mvt_open_variants,
    ogr_mvt_xyz_options,
    ogr_mvt_test_ogrsf_pbf,
    ogr_mvt_test_ogrsf_directory,
    ogr_mvt_mbtiles,
    ogr_mvt_mbtiles_json_field,
    ogr_mvt_mbtiles_json_field_auto,
    ogr_mvt_mbtiles_open_vector_in_raster_mode,
    ogr_mvt_mbtiles_test_ogrsf,
    ogr_mvt_x_y_z_filename_scheme,
    ogr_mvt_polygon_larger_than_header,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_mvt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

