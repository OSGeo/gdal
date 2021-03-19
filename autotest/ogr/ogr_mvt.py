#!/usr/bin/env pytest
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

import gdaltest
import ogrtest
import webserver
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import pytest

###############################################################################


def test_ogr_mvt_datatypes():

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
        f.DumpReadable()
        pytest.fail()

    # Without metadata.json
    ds = gdal.OpenEx('data/mvt/datatypes/0/0/0.pbf',
                     open_options=['METADATA_FILE='])
    lyr = ds.GetLayer(0)

    count = lyr.GetLayerDefn().GetFieldCount()
    assert count == 16

    tab = []
    for i in range(lyr.GetLayerDefn().GetFieldCount()):
        fld_defn = lyr.GetLayerDefn().GetFieldDefn(i)
        tab += [(fld_defn.GetName(), fld_defn.GetType(),
                 fld_defn.GetSubType())]
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
    assert tab == expected_tab

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
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_mvt_datatype_promotion():

    ds = ogr.Open('data/mvt/datatype_promotion.pbf')
    tab = [('int_to_int64', ogr.OFTInteger64),
           ('int_to_real', ogr.OFTReal),
           ('int64_to_real', ogr.OFTReal),
           ('bool_to_int', ogr.OFTInteger),
           ('bool_to_str', ogr.OFTString),
           ('float_to_double', ogr.OFTReal)]
    for layer_name, dt in tab:
        lyr = ds.GetLayerByName(layer_name)
        fld_defn = lyr.GetLayerDefn().GetFieldDefn(1)
        assert fld_defn.GetType() == dt, layer_name
        assert fld_defn.GetSubType() == ogr.OFSTNone, layer_name

    
###############################################################################


def test_ogr_mvt_limit_cases():

    with gdaltest.error_handler():
        ds = ogr.Open('data/mvt/limit_cases.pbf')

    lyr = ds.GetLayerByName('empty')
    assert lyr.GetFeatureCount() == 0

    lyr = ds.GetLayerByName('layer1')
    assert lyr.GetFeatureCount() == 7

    f = lyr.GetFeature(1)
    assert f['mvt_id'] == 1

    with gdaltest.error_handler():
        f = lyr.GetFeature(6)
        assert f['b'] == 1

    lyr = ds.GetLayerByName('layer2')
    assert lyr.GetFeatureCount() == 0

    lyr = ds.GetLayerByName('layer3')
    assert lyr.GetFeatureCount() == 0

    lyr = ds.GetLayerByName('layer4')
    assert lyr.GetFeatureCount() == 0

    lyr = ds.GetLayerByName('layer5')
    assert lyr.GetFeatureCount() == 1
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (2070 2690)'

###############################################################################


def test_ogr_mvt_with_extension_fields():

    ds = ogr.Open('data/mvt/with_extension_fields.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'LINESTRING (2070 2690,2082 2707)') != 0:
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_mvt_mixed():

    ds = ogr.Open('data/mvt/mixed/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTIPOINT ((215246.671651058 6281289.23636264),(332653.947097085 6447616.20991119))') != 0:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'LINESTRING (215246.671651058 6281289.23636264,332653.947097085 6447616.20991119)') != 0:
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_mvt_linestring():

    ds = ogr.Open('data/mvt/linestring/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTILINESTRING ((215246.671651058 6281289.23636264,332653.947097085 6447616.20991119))') != 0:
        f.DumpReadable()
        pytest.fail()

    ds = gdal.OpenEx('data/mvt/linestring/0/0/0.pbf', open_options=['METADATA_FILE='])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'LINESTRING (215246.671651058 6281289.23636264,332653.947097085 6447616.20991119)') != 0:
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_mvt_multilinestring():

    ds = ogr.Open('data/mvt/multilinestring/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTILINESTRING ((215246.671651058 6281289.23636264,332653.947097085 6447616.20991119),(440277.282922614 6623727.12308023,547900.618748143 6809621.97586978),(665307.894194175 6985732.88903883,772931.230019704 7171627.74182838))') != 0:
        f.DumpReadable()
        pytest.fail()

    ds = gdal.OpenEx('data/mvt/multilinestring/0/0/0.pbf', open_options=['METADATA_FILE='])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTILINESTRING ((215246.671651058 6281289.23636264,332653.947097085 6447616.20991119),(440277.282922614 6623727.12308023,547900.618748143 6809621.97586978),(665307.894194175 6985732.88903883,772931.230019704 7171627.74182838))') != 0:
        f.DumpReadable()
        pytest.fail()

    

###############################################################################

def test_ogr_mvt_polygon():

    ds = ogr.Open('data/mvt/polygon/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTIPOLYGON (((332653.947097085 6447616.20991119,332653.947097085 6281289.23636264,215246.671651058 6281289.23636264,215246.671651058 6447616.20991119,332653.947097085 6447616.20991119)))') != 0:
        f.DumpReadable()
        pytest.fail()

    ds = gdal.OpenEx('data/mvt/polygon/0/0/0.pbf', open_options=['METADATA_FILE='])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'POLYGON ((332653.947097085 6447616.20991119,332653.947097085 6281289.23636264,215246.671651058 6281289.23636264,215246.671651058 6447616.20991119,332653.947097085 6447616.20991119))') != 0:
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_mvt_point_polygon():

    ds = ogr.Open('data/mvt/point_polygon/0')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTIPOINT ((215246.671651058 6281289.23636264))') != 0:
        f.DumpReadable()
        pytest.fail()
    lyr = ds.GetLayer(1)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTIPOLYGON (((440277.282922614 450061.222543117,440277.282922614 -440277.282922614,0.0 -440277.282922614,0.0 -215246.671651058,215246.671651058 -215246.671651058,215246.671651058 225030.61127156,0.0 225030.61127156,0.0 450061.222543117,440277.282922614 450061.222543117)),((0.0 117407.275446031,0.0 -107623.335825529,-117407.275446031 -107623.335825529,-117407.275446031 117407.275446031,0.0 117407.275446031)),((107623.335825529 58703.6377230138,107623.335825529 -48919.6981025115,48919.6981025115 -48919.6981025115,48919.6981025115 58703.6377230138,107623.335825529 58703.6377230138)))') != 0:
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_mvt_point_polygon_clip():

    if not ogrtest.have_geos() or gdal.GetConfigOption('OGR_MVT_CLIP') is not None:
        pytest.skip()

    ds = ogr.Open('data/mvt/point_polygon/1')
    lyr = ds.GetLayer(1)
    f = lyr.GetNextFeature()
    expected_wkt = 'MULTIPOLYGON (((0.0 112515.30563578,0 0,-112515.30563578 0.0,-112515.30563578 112515.30563578,0.0 112515.30563578)))'
    expected_wkt2 = 'MULTIPOLYGON (((-112515.30563578 112515.30563578,0.0 112515.30563578,0 0,-112515.30563578 0.0,-112515.30563578 112515.30563578)))'
    expected_wkt3 = 'MULTIPOLYGON (((0 0,-112515.30563578 0.0,-112515.30563578 112515.30563578,0.0 112515.30563578,0 0)))'
    assert ogrtest.check_feature_geometry(f, expected_wkt) == 0 or \
           ogrtest.check_feature_geometry(f, expected_wkt2) == 0 or \
           ogrtest.check_feature_geometry(f, expected_wkt3) == 0, \
            f.GetGeometryRef().ExportToWkt()

    
###############################################################################


def test_ogr_mvt_tileset_without_readdir():

    with gdaltest.config_option('MVT_USE_READDIR', 'NO'):
        ds = gdal.OpenEx('data/mvt/linestring/0')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is not None

###############################################################################


def test_ogr_mvt_tileset_tilegl():

    ds = ogr.Open('data/mvt/linestring_tilejson_gl/0')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    f = lyr.GetNextFeature()
    assert f is not None

###############################################################################


def test_ogr_mvt_tileset_without_metadata_file():

    ds = gdal.OpenEx('data/mvt/point_polygon/1',
                     open_options=['METADATA_FILE=', 'CLIP=NO'])
    lyr = ds.GetLayerByName('point')
    assert lyr.GetGeomType() == ogr.wkbMultiPoint

    lyr = ds.GetLayerByName('polygon2')
    assert lyr.GetGeomType() == ogr.wkbMultiPolygon
    assert lyr.GetLayerDefn().GetFieldCount() == 2

###############################################################################


def test_ogr_mvt_tileset_json_field():

    ds = gdal.OpenEx('data/mvt/datatypes/0',
                     open_options=['METADATA_FILE=', 'JSON_FIELD=YES', 'CLIP=NO'])
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    f = lyr.GetNextFeature()
    d = json.loads(f.GetFieldAsString("json"))
    assert (d == {
            "bool_true": True,
            "bool_false": False,
            "pos_int_value": 1,
            "pos_int64_value": 123456789012345,
            "neg_int_value": -1,
            "neg_int64_value": -123456789012345,
            "pos_sint_value": 1,
            "pos_sint64_value": 123456789012345,
            "neg_sint_value": -1,
            "neg_sint64_value": -123456789012345,
            "uint_value": 2000000000,
            "uint64_value": 4000000000,
            "float_value": 1.25,
            "real_value": 1.23456789,
            "string_value": "str"
    }), f.GetFieldAsString("json")

###############################################################################


def test_ogr_mvt_open_variants():

    expected_geom = 'MULTILINESTRING ((215246.671651058 6281289.23636264,332653.947097085 6447616.20991119))'

    ds = ogr.Open('MVT:data/mvt/linestring/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, expected_geom) != 0:
        f.DumpReadable()
        pytest.fail()

    ds = ogr.Open('MVT:data/mvt/linestring/0')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, expected_geom) != 0:
        f.DumpReadable()
        pytest.fail()

    ds = ogr.Open('/vsigzip/data/mvt/linestring/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, expected_geom) != 0:
        f.DumpReadable()
        pytest.fail()

    ds = ogr.Open('MVT:/vsigzip/data/mvt/linestring/0/0/0.pbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, expected_geom) != 0:
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_mvt_xyz_options():

    ds = gdal.OpenEx('data/mvt/datatypes/0/0/0.pbf',
                     open_options=['X=1', 'Y=2', 'Z=3'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'POINT (-12496536.8802869 8299226.7830913)') != 0:
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_mvt_test_ogrsf_pbf():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() +
                               ' -ro data/mvt/datatypes/0/0/0.pbf')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################


def test_ogr_mvt_test_ogrsf_directory():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() +
                               ' -ro data/mvt/datatypes/0')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################


def test_ogr_mvt_mbtiles():

    if ogr.GetDriverByName('MBTILES') is None:
        pytest.skip()

    ds = ogr.Open('data/mvt/point_polygon.mbtiles')
    lyr = ds.GetLayerByName('point')
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTIPOINT ((220138.641461308 6276397.26655239))') != 0:
        f.DumpReadable()
        pytest.fail()

    lyr.SetSpatialFilterRect(0, 0, 10000000, 10000000)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'MULTIPOINT ((220138.641461308 6276397.26655239))') != 0:
        f.DumpReadable()
        pytest.fail()

    
###############################################################################


def test_ogr_mvt_mbtiles_json_field():

    if ogr.GetDriverByName('MBTILES') is None:
        pytest.skip()

    ds = gdal.OpenEx('data/mvt/datatypes.mbtiles',
                     open_options=['JSON_FIELD=YES', 'CLIP=NO'])
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    f = lyr.GetNextFeature()
    d = json.loads(f.GetFieldAsString("json"))
    assert (d == {'int64_value': 123456789012345,
             'string_value': 'str',
             'real_value': 1.23456789,
             'bool_false': False,
             'pos_int_value': 1,
             'neg_int_value': -1,
             'bool_true': True,
             'float_value': 1.25
             }), f.GetFieldAsString("json")

###############################################################################


def test_ogr_mvt_mbtiles_json_field_auto():

    if ogr.GetDriverByName('MBTILES') is None:
        pytest.skip()

    ds = gdal.OpenEx('data/mvt/datatypes_json_field_auto.mbtiles',
                     open_options=['CLIP=NO'])
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    f = lyr.GetNextFeature()
    d = json.loads(f.GetFieldAsString("json"))
    assert (d == {'int64_value': 123456789012345,
             'string_value': 'str',
             'real_value': 1.23456789,
             'bool_false': False,
             'pos_int_value': 1,
             'neg_int_value': -1,
             'bool_true': True,
             'float_value': 1.25
             }), f.GetFieldAsString("json")

###############################################################################


def test_ogr_mvt_mbtiles_test_ogrsf():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    if ogr.GetDriverByName('MBTILES') is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() +
                               ' -ro data/mvt/point_polygon.mbtiles polygon2')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################


def test_ogr_mvt_mbtiles_open_vector_in_raster_mode():

    if ogr.GetDriverByName('MBTILES') is None:
        pytest.skip()

    ds = gdal.OpenEx('data/mvt/datatypes.mbtiles', gdal.OF_RASTER)
    assert ds is None

###############################################################################


def test_ogr_mvt_x_y_z_filename_scheme():

    tmpfilename = '/vsimem/0-0-0.pbf'
    gdal.FileFromMemBuffer(tmpfilename,
                           open('data/mvt/linestring/0/0/0.pbf', 'rb').read())
    ds = ogr.Open(tmpfilename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f, 'LINESTRING (215246.671651058 6281289.23636264,332653.947097085 6447616.20991119)') != 0:
        f.DumpReadable()
        pytest.fail()
    ds = None
    gdal.Unlink(tmpfilename)

###############################################################################


def test_ogr_mvt_polygon_larger_than_header():

    ds = gdal.OpenEx('data/mvt/polygon_larger_than_header.pbf',
                     open_options=['CLIP=NO'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is not None

###############################################################################


def test_ogr_mvt_errors():

    assert ogr.Open('MVT:/i_do_not/exist') is None

    # Cannot detect Z in directory name
    assert ogr.Open('MVT:data') is None

    # Invalid Z
    gdal.Mkdir('/vsimem/33', 0)

    assert ogr.Open('MVT:/vsimem/33') is None

    gdal.Rmdir('/vsimem/33')

    # Inexisting metadata
    with gdaltest.error_handler():
        assert (gdal.OpenEx('data/mvt/linestring/0/0/0.pbf',
                       open_options=['METADATA_FILE=/i_do_not/exist']) is not None)

    # Invalid metadata
    with gdaltest.error_handler():
        assert (gdal.OpenEx('data/mvt/linestring/0/0/0.pbf',
                       open_options=['METADATA_FILE=ogr_mvt.py']) is not None)

    # Invalid metadata
    gdal.FileFromMemBuffer('/vsimem/my.json', '{}')
    with gdaltest.error_handler():
        assert (gdal.OpenEx('data/mvt/linestring/0/0/0.pbf',
                       open_options=['METADATA_FILE=/vsimem/my.json']) is not None)
    gdal.Unlink('/vsimem/my.json')

    # Invalid metadata
    gdal.FileFromMemBuffer('/vsimem/my.json', '{ "json": "x y" }')
    with gdaltest.error_handler():
        assert (gdal.OpenEx('data/mvt/linestring/0/0/0.pbf',
                       open_options=['METADATA_FILE=/vsimem/my.json']) is not None)
    gdal.Unlink('/vsimem/my.json')

    # Too big file
    tmpfilename = '/vsimem/foo.pbf'
    gdal.FileFromMemBuffer(tmpfilename,
                           open('data/mvt/polygon_larger_than_header.pbf', 'rb').read())
    f = gdal.VSIFOpenL(tmpfilename, 'rb+')
    gdal.VSIFSeekL(f, 20 * 1024 * 1024, 0)
    gdal.VSIFWriteL(' ', 1, 1, f)
    gdal.VSIFCloseL(f)
    ds = ogr.Open(tmpfilename)
    gdal.Unlink(tmpfilename)
    assert ds is None

###############################################################################


def test_ogr_mvt_http_start():

    gdaltest.webserver_process = None
    gdaltest.webserver_port = 0

    if not gdaltest.built_against_curl():
        pytest.skip()

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        pytest.skip()

    
###############################################################################


def test_ogr_mvt_http():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    handler = webserver.SequentialHandler()
    handler.add('GET', '/linestring/metadata.json', 200, {},
                open('data/mvt/linestring/metadata.json', 'rb').read())
    handler.add('GET', '/linestring/0/0/0.pbf', 200, {},
                open('data/mvt/linestring/0/0/0.pbf', 'rb').read())
    handler.add('GET', '/linestring/0/0/0.pbf', 200, {},
                open('data/mvt/linestring/0/0/0.pbf', 'rb').read())
    with webserver.install_http_handler(handler):
        ds = ogr.Open('MVT:http://127.0.0.1:%d/linestring/0' % gdaltest.webserver_port)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f is not None

    # No metadata file nor tile
    handler = webserver.SequentialHandler()
    handler.add('GET', '/linestring/metadata.json', 404, {})
    handler.add('GET', '/linestring.json', 404, {})
    handler.add('GET', '/linestring/0/0/0.pbf', 404, {})
    with webserver.install_http_handler(handler):
        with gdaltest.error_handler():
            ds = ogr.Open('MVT:http://127.0.0.1:%d/linestring/0' % gdaltest.webserver_port)
        assert ds is None

    # No metadata file, but tiles
    handler = webserver.SequentialHandler()
    handler.add('GET', '/linestring/metadata.json', 404, {})
    handler.add('GET', '/linestring.json', 404, {})
    handler.add('GET', '/linestring/0/0/0.pbf', 200, {},
                open('data/mvt/linestring/0/0/0.pbf', 'rb').read())
    handler.add('GET', '/linestring/0/0/0.pbf', 200, {},
                open('data/mvt/linestring/0/0/0.pbf', 'rb').read())
    with webserver.install_http_handler(handler):
        ds = ogr.Open('MVT:http://127.0.0.1:%d/linestring/0' % gdaltest.webserver_port)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f is not None

    # metadata.json file, but no tiles
    handler = webserver.SequentialHandler()
    handler.add('GET', '/linestring/metadata.json', 200, {},
                open('data/mvt/linestring/metadata.json', 'rb').read())
    handler.add('GET', '/linestring/0/0/0.pbf', 404, {})
    handler.add('GET', '/linestring/0/0/0.pbf', 404, {})
    with webserver.install_http_handler(handler):
        ds = ogr.Open('MVT:http://127.0.0.1:%d/linestring/0' % gdaltest.webserver_port)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f is None

    # No metadata.json file, but a linestring.json and no tiles
    handler = webserver.SequentialHandler()
    handler.add('GET', '/linestring/metadata.json', 404, {})
    handler.add('GET', '/linestring.json', 200, {},
                open('data/mvt/linestring/metadata.json', 'rb').read())
    handler.add('GET', '/linestring/0/0/0.pbf', 404, {})
    handler.add('GET', '/linestring/0/0/0.pbf', 404, {})
    with webserver.install_http_handler(handler):
        ds = ogr.Open('MVT:http://127.0.0.1:%d/linestring/0' % gdaltest.webserver_port)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f is None

    # Open pbf file
    handler = webserver.SequentialHandler()
    handler.add('GET', '/linestring/0/0/0.pbf', 200, {},
                open('data/mvt/linestring/0/0/0.pbf', 'rb').read())
    with webserver.install_http_handler(handler):
        ds = ogr.Open('MVT:http://127.0.0.1:%d/linestring/0/0/0.pbf' % gdaltest.webserver_port)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f is not None

    
###############################################################################


def test_ogr_mvt_http_stop():

    if gdaltest.webserver_port == 0:
        pytest.skip()

    webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

###############################################################################


def test_ogr_mvt_write_one_layer():

    if not ogrtest.have_geos() or ogr.GetDriverByName('SQLITE') is None:
        pytest.skip()

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, gdal.GDT_Unknown)
    lyr = src_ds.CreateLayer('mylayer')
    lyr.CreateField(ogr.FieldDefn('strfield', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('int64field', ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn('realfield', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('datefield', ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn('datetimefield', ogr.OFTDateTime))
    boolfield = ogr.FieldDefn('boolfield', ogr.OFTInteger)
    boolfield.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(boolfield)

    # Test empty layer: OK
    with gdaltest.error_handler():
        out_ds = gdal.VectorTranslate('/vsimem/outmvt', src_ds, format='MVT')
    assert out_ds is not None

    # Cannot create directory
    with gdaltest.error_handler():
        out_ds = gdal.VectorTranslate('/i_dont/exist/outmvt', src_ds, format='MVT')
    assert out_ds is None

    # Directory already exists
    with gdaltest.error_handler():
        out_ds = gdal.VectorTranslate('/vsimem/outmvt', src_ds, format='MVT')
    assert out_ds is None

    gdal.RmdirRecursive('/vsimem/outmvt')

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(123)
    f['strfield'] = 'foo'
    f['intfield'] = -1
    f['int64field'] = 123456789012345
    f['realfield'] = 1.25
    f['datefield'] = '2018/02/01'
    f['datetimefield'] = '2018/02/01 12:34:56'
    f['boolfield'] = True
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 1000000)'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(124)
    f['strfield'] = 'foo'
    f['intfield'] = -1
    f['int64field'] = 123456789012345
    f['realfield'] = 1.25
    f['datefield'] = '2018/02/01'
    f['datetimefield'] = '2018/02/01 12:34:56'
    f['boolfield'] = True
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(500000 1000000,510000 1010000,520000 1020000)'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(125)
    f['strfield'] = 'foobarbazbaw'
    f['intfield'] = 1
    f['int64field'] = -123456789012345
    f['realfield'] = -1.25678
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((500000 1000000,510000 1000000,510000 1010000,500000 1000000))'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(126)
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOINT(500000 1000000,510000 1010000)'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(127)
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING((500000 1000000,510000 1010000),(510000 1010000,510000 1000000))'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(128)
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((500000 1000000,510000 1000000,510000 1010000,500000 1000000)),((-500000 1000000,-510000 1000000,-510000 1010000,-500000 1000000),(-502000 1001000,-509000 1001000,-509000 1008500,-502000 1001000)))'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(129)
    f.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(500000 1000000),LINESTRING(500000 1000000,510000 1010000,520000 1020000))'))
    lyr.CreateFeature(f)

    with gdaltest.error_handler():
        out_ds = gdal.VectorTranslate('/vsimem/outmvt', src_ds,
                                      options='-f MVT -preserve_fid')
    assert out_ds is not None
    out_ds = None

    out_ds = ogr.Open('/vsimem/outmvt/0')
    assert out_ds is not None
    out_lyr = out_ds.GetLayerByName('mylayer')
    out_f = out_lyr.GetNextFeature()
    if out_f['mvt_id'] != 123 or \
       out_f['strfield'] != 'foo' or \
       out_f['intfield'] != -1 or \
       out_f['int64field'] != 123456789012345 or \
       out_f['realfield'] != 1.25 or \
       out_f['datefield'] != '2018-02-01' or \
       out_f['datetimefield'] != '2018-02-01T12:34:56' or \
       out_f['boolfield'] is False or \
       ogrtest.check_feature_geometry(out_f, 'POINT (498980.920645632 997961.84129126)') != 0:
        out_f.DumpReadable()
        pytest.fail()

    out_f = out_lyr.GetNextFeature()
    if out_f['strfield'] != 'foo' or \
       out_f['intfield'] != -1 or \
       out_f['int64field'] != 123456789012345 or \
       out_f['realfield'] != 1.25 or \
       out_f['datefield'] != '2018-02-01' or \
       out_f['datetimefield'] != '2018-02-01T12:34:56' or \
       out_f['boolfield'] is False or \
       ogrtest.check_feature_geometry(out_f, 'MULTILINESTRING ((498980.920645632 997961.84129126,508764.860266134 1007745.78091176,518548.799886636 1017529.72053226))') != 0:
        out_f.DumpReadable()
        pytest.fail()

    out_f = out_lyr.GetNextFeature()
    if out_f['strfield'] != 'foobarbazbaw' or \
       out_f['intfield'] != 1 or \
       out_f['int64field'] != -123456789012345 or \
       out_f['realfield'] != -1.25678 or \
       out_f.IsFieldSet('datefield') or \
       out_f.IsFieldSet('datetimefield') or \
       out_f.IsFieldSet('boolfield') or \
       ogrtest.check_feature_geometry(out_f, 'POLYGON ((498980.920645632 997961.84129126,508764.860266134 1007745.78091176,508764.860266134 997961.84129126,498980.920645632 997961.84129126))') != 0:
        out_f.DumpReadable()
        pytest.fail()

    out_f = out_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(out_f, 'MULTIPOINT ((498980.920645632 997961.84129126),(508764.860266134 1007745.78091176))') != 0:
        out_f.DumpReadable()
        pytest.fail()

    out_f = out_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(out_f, 'MULTILINESTRING ((498980.920645632 997961.84129126,508764.860266134 1007745.78091176),(508764.860266134 1007745.78091176,508764.860266134 997961.84129126))') != 0:
        out_f.DumpReadable()
        pytest.fail()

    out_f = out_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(out_f, 'MULTIPOLYGON (((498980.920645632 997961.84129126,508764.860266134 1007745.78091176,508764.860266134 997961.84129126,498980.920645632 997961.84129126)),((-498980.920645632 997961.84129126,-508764.860266134 997961.84129126,-508764.860266134 1007745.78091176,-498980.920645632 997961.84129126)))') != 0:
        out_f.DumpReadable()
        pytest.fail()

    out_f = out_lyr.GetNextFeature()
    if (out_f.GetGeometryRef().GetGeometryType() == ogr.wkbPoint and ogrtest.check_feature_geometry(out_f, 'POINT (498980.920645632 997961.84129126)') != 0) or \
       (out_f.GetGeometryRef().GetGeometryType() == ogr.wkbMultiLineString and ogrtest.check_feature_geometry(out_f, 'MULTILINESTRING ((498980.920645632 997961.84129126,508764.860266134 1007745.78091176,518548.799886636 1017529.72053226))') != 0) or \
       out_f.GetGeometryRef().GetGeometryType() not in (ogr.wkbPoint, ogr.wkbMultiLineString):
        out_f.DumpReadable()
        pytest.fail()

    out_f = out_lyr.GetNextFeature()
    if (out_f.GetGeometryRef().GetGeometryType() == ogr.wkbPoint and ogrtest.check_feature_geometry(out_f, 'POINT (498980.920645632 997961.84129126)') != 0) or \
       (out_f.GetGeometryRef().GetGeometryType() == ogr.wkbMultiLineString and ogrtest.check_feature_geometry(out_f, 'MULTILINESTRING ((498980.920645632 997961.84129126,508764.860266134 1007745.78091176,518548.799886636 1017529.72053226))') != 0) or \
       out_f.GetGeometryRef().GetGeometryType() not in (ogr.wkbPoint, ogr.wkbMultiLineString):
        out_f.DumpReadable()
        pytest.fail()

    out_ds = ogr.Open('/vsimem/outmvt/5')
    assert out_ds is not None
    out_lyr = out_ds.GetLayerByName('mylayer')
    assert out_lyr.GetFeatureCount() == 9

    f = gdal.VSIFOpenL('/vsimem/outmvt/metadata.json', 'rb')
    assert f is not None
    data = gdal.VSIFReadL(1, 100000, f).decode('ASCII')
    gdal.VSIFCloseL(f)
    data_json = json.loads(data)
    expected_json = {
        "name": "outmvt",
        "description": "",
        "version": 2,
        "minzoom": 0,
        "maxzoom": 5,
        "center": "0.0449158,9.0352907,0",
        "bounds": "-4.5814079,8.9465739,4.6712395,9.1240075",
        "type": "overlay",
        "format": "pbf",
    }
    for k in expected_json:
        assert k in data_json and data_json[k] == expected_json[k], data
    json_json = json.loads(data_json['json'])
    expected_json_json = {
        "vector_layers": [
            {
                "id": "mylayer",
                "description": "",
                "minzoom": 0,
                "maxzoom": 5,
                "fields": {
                    "strfield": "String",
                    "intfield": "Number",
                    "int64field": "Number",
                    "realfield": "Number",
                    "datefield": "String",
                    "datetimefield": "String",
                    "boolfield": "Boolean"
                }
            }
        ],
        "tilestats": {
            "layerCount": 1,
            "layers": [
                {
                    "layer": "mylayer",
                    "count": 7,
                    "geometry": "LineString",
                    "attributeCount": 7,
                    "attributes": [
                        {
                            "attribute": "strfield",
                            "count": 2,
                            "type": "string",
                            "values": [
                                "foo",
                                "foobarbazbaw"
                            ]
                        },
                        {
                            "attribute": "intfield",
                            "count": 2,
                            "type": "numeric",
                            "values": [
                                -1,
                                1
                            ],
                            "min":-1,
                            "max":1
                        },
                        {
                            "attribute": "int64field",
                            "count": 2,
                            "type": "numeric",
                            "values": [
                                -123456789012345,
                                123456789012345
                            ],
                            "min":-123456789012345,
                            "max":123456789012345
                        },
                        {
                            "attribute": "realfield",
                            "count": 2,
                            "type": "numeric",
                            "values": [
                                -1.256780,
                                1.250000
                            ],
                            "min":-1.25678,
                            "max":1.25
                        },
                        {
                            "attribute": "datefield",
                            "count": 1,
                            "type": "string",
                            "values": [
                                "2018-02-01"
                            ]
                        },
                        {
                            "attribute": "datetimefield",
                            "count": 1,
                            "type": "string",
                            "values": [
                                "2018-02-01T12:34:56"
                            ]
                        },
                        {
                            "attribute": "boolfield",
                            "count": 1,
                            "type": "boolean",
                            "values": [
                                True
                            ]
                        }
                    ]
                }
            ]
        }
    }

    assert json_json == expected_json_json, data_json['json']

    gdal.RmdirRecursive('/vsimem/outmvt')

###############################################################################


def test_ogr_mvt_write_conf():

    if not ogrtest.have_geos() or ogr.GetDriverByName('SQLITE') is None:
        pytest.skip()

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, gdal.GDT_Unknown)
    lyr = src_ds.CreateLayer('mylayer')

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 1000000)'))
    lyr.CreateFeature(f)

    conf = {'mylayer': {'target_name': 'TheLayer',
                        'description': 'the layer',
                        'minzoom': 1,
                        'maxzoom': 2}}
    with gdaltest.tempfile('/vsimem/conf.json', json.dumps(conf)):
        out_ds = gdal.VectorTranslate('/vsimem/outmvt', src_ds,
                                    format='MVT', datasetCreationOptions=["CONF=/vsimem/conf.json"])
    assert out_ds is not None
    out_ds = None

    out_ds = ogr.Open('/vsimem/outmvt/1')
    assert out_ds is not None
    out_lyr = out_ds.GetLayerByName('TheLayer')
    assert out_lyr
    out_ds = None

    gdal.RmdirRecursive('/vsimem/outmvt')

    out_ds = gdal.VectorTranslate('/vsimem/outmvt', src_ds,
                                  format='MVT', datasetCreationOptions=["CONF=%s" % json.dumps(conf)])
    assert out_ds is not None
    out_ds = None

    out_ds = ogr.Open('/vsimem/outmvt/1')
    assert out_ds is not None
    out_lyr = out_ds.GetLayerByName('TheLayer')
    assert out_lyr
    out_f = out_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(out_f, 'MULTIPOINT (498980.920645632 997961.84129126)') != 0:
        out_f.DumpReadable()
        pytest.fail()

    f = gdal.VSIFOpenL('/vsimem/outmvt/metadata.json', 'rb')
    assert f is not None
    data = gdal.VSIFReadL(1, 100000, f).decode('ASCII')
    gdal.VSIFCloseL(f)
    data_json = json.loads(data)
    json_json = json.loads(data_json['json'])
    expected_json_json = {
        "vector_layers": [
            {
                "id": "TheLayer",
                "description": "the layer",
                "minzoom": 1,
                "maxzoom": 2,
                "fields": {
                }
            }
        ],
        "tilestats": {
            "layerCount": 1,
            "layers": [
                {
                    "layer": "TheLayer",
                    "count": 1,
                    "geometry": "Point",
                    "attributeCount": 0,
                    "attributes": [
                    ]
                }
            ]
        }
    }

    assert json_json == expected_json_json, data_json['json']

    gdal.RmdirRecursive('/vsimem/outmvt')

###############################################################################


def test_ogr_mvt_write_mbtiles():

    if not ogrtest.have_geos() or ogr.GetDriverByName('SQLITE') is None:
        pytest.skip()

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, gdal.GDT_Unknown)
    lyr = src_ds.CreateLayer('mylayer')

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 1000000)'))
    lyr.CreateFeature(f)

    out_ds = gdal.VectorTranslate('/vsimem/out.mbtiles', src_ds)
    assert out_ds is not None
    out_ds = None

    out_ds = ogr.Open('/vsimem/out.mbtiles')
    assert out_ds is not None
    out_lyr = out_ds.GetLayerByName('mylayer')
    out_f = out_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(out_f, 'MULTIPOINT ((499898.164985053 1000102.07808325))') != 0:
        out_f.DumpReadable()
        pytest.fail()
    out_ds = None

    gdal.Unlink('/vsimem/out.mbtiles')

###############################################################################


def test_ogr_mvt_write_limitations_max_size():

    if not ogrtest.have_geos() or ogr.GetDriverByName('SQLITE') is None:
        pytest.skip()

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, gdal.GDT_Unknown)
    lyr = src_ds.CreateLayer('mylayer')
    lyr.CreateField(ogr.FieldDefn('field'))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field', '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ')
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 1000000)'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(500000 1000000,510000 1000000)'))
    lyr.CreateFeature(f)

    # Also test single threaded execution
    with gdaltest.config_option('GDAL_NUM_THREADS', '1'):
        out_ds = gdal.VectorTranslate('/vsimem/out.mbtiles', src_ds,
                                      datasetCreationOptions=['MAX_SIZE=100', 'SIMPLIFICATION=1'])
    assert out_ds is not None
    out_ds = None

    out_ds = ogr.Open('/vsimem/out.mbtiles')
    assert out_ds is not None
    out_lyr = out_ds.GetLayerByName('mylayer')
    out_f = out_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(out_f, 'MULTILINESTRING ((498980.920645631 1007745.78091176,508764.860266133 1007745.78091176))') != 0:
        out_f.DumpReadable()
        pytest.fail()
    out_f = out_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(out_f, 'POINT (498980.920645631 1007745.78091176)') != 0:
        out_f.DumpReadable()
        pytest.fail()
    out_ds = None

    gdal.Unlink('/vsimem/out.mbtiles')

###############################################################################


def test_ogr_mvt_write_polygon_repaired():

    if not ogrtest.have_geos() or ogr.GetDriverByName('SQLITE') is None:
        pytest.skip()

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, gdal.GDT_Unknown)
    lyr = src_ds.CreateLayer('mylayer')
    lyr.CreateField(ogr.FieldDefn('field'))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 500000,100000 500000,100000 200000,100500 200000,100500 500000,500000 500000,500000 0,0 0))'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((0 0,0 500000,100000 500000,100000 200000,100500 200000,100500 500000,500000 500000,500000 0,0 0)),((1000000 0,1000000 1000000,2000000 1000000,1000000 0)))'))
    lyr.CreateFeature(f)

    # Cannot be repaired
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)

    out_ds = gdal.VectorTranslate('/vsimem/out.mbtiles', src_ds, datasetCreationOptions=['MAXZOOM=0'])
    assert out_ds is not None
    out_ds = None

    out_ds = ogr.Open('/vsimem/out.mbtiles')
    assert out_ds is not None
    out_lyr = out_ds.GetLayerByName('mylayer')
    out_f = out_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(out_f, 'MULTIPOLYGON (((0 0,0.0 498980.920645632,498980.920645632 498980.920645632,498980.920645632 0.0,0 0)))') != 0:
        out_f.DumpReadable()
        pytest.fail()
    out_f = out_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(out_f, 'MULTIPOLYGON (((0 0,0.0 498980.920645632,498980.920645632 498980.920645632,498980.920645632 0.0,0 0)),((997961.84129126 0.0,997961.84129126 997961.84129126,1995923.68258252 997961.84129126,997961.84129126 0.0)))') != 0:
        out_f.DumpReadable()
        pytest.fail()
    out_f = out_lyr.GetNextFeature()
    if out_f is not None:
        out_f.DumpReadable()
        pytest.fail()
    out_ds = None

    gdal.Unlink('/vsimem/out.mbtiles')

###############################################################################


def test_ogr_mvt_write_conflicting_innner_ring():

    if not ogrtest.have_geos() or ogr.GetDriverByName('SQLITE') is None:
        pytest.skip()

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, gdal.GDT_Unknown)
    lyr = src_ds.CreateLayer('mylayer')
    lyr.CreateField(ogr.FieldDefn('field'))

    f = ogr.Feature(lyr.GetLayerDefn())
    # the second inner ring conflicts with the first one once transformed to integer coordinates
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((-500000 1000000,-510000 1000000,-510000 1010000,-500000 1000000),(-502000 1001000,-509000 1001000,-509000 1008500,-502000 1001000),(-502000 1000900,-509000 1000900,-509000 1000800,-502000 1000900))'))
    lyr.CreateFeature(f)

    out_ds = gdal.VectorTranslate('/vsimem/out.mbtiles', src_ds)
    assert out_ds is not None
    out_ds = None

    out_ds = ogr.Open('/vsimem/out.mbtiles')
    assert out_ds is not None
    out_lyr = out_ds.GetLayerByName('mylayer')
    out_f = out_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(out_f, 'MULTIPOLYGON (((-499898.164985052 1000102.07808325,-509987.852718695 1000102.07808325,-509987.852718695 1009886.01770375,-499898.164985052 1000102.07808325),(-502038.401777037 1001019.32242267,-509070.608379273 1008357.27713804,-509070.608379273 1001019.32242267,-502038.401777037 1001019.32242267)))') != 0:
        out_f.DumpReadable()
        pytest.fail()
    out_ds = None

    gdal.Unlink('/vsimem/out.mbtiles')

###############################################################################


def test_ogr_mvt_write_limitations_max_size_polygon():

    if not ogrtest.have_geos() or ogr.GetDriverByName('SQLITE') is None:
        pytest.skip()

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, gdal.GDT_Unknown)
    lyr = src_ds.CreateLayer('mylayer')
    lyr.CreateField(ogr.FieldDefn('field'))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field', '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ')
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((500000 1000000,510000 1000000,510000 1010000,500000 1000000),(503000 1003000,507000 1003000,507000 1005000,503000 1003000))'))
    lyr.CreateFeature(f)

    out_ds = gdal.VectorTranslate('/vsimem/out.mbtiles', src_ds,
                                  datasetCreationOptions=['MAX_SIZE=100'])
    assert out_ds is not None
    out_ds = None

    out_ds = ogr.Open('/vsimem/out.mbtiles')
    assert out_ds is not None
    out_lyr = out_ds.GetLayerByName('mylayer')
    out_f = out_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(out_f, 'MULTIPOLYGON (((498980.920645631 1007745.78091176,508764.860266133 1017529.72053227,508764.860266133 1007745.78091176,498980.920645631 1007745.78091176)))') != 0:
        out_f.DumpReadable()
        pytest.fail()
    out_ds = None

    gdal.Unlink('/vsimem/out.mbtiles')

###############################################################################


def test_ogr_mvt_write_limitations_max_features():

    if not ogrtest.have_geos() or ogr.GetDriverByName('SQLITE') is None:
        pytest.skip()

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, gdal.GDT_Unknown)
    lyr = src_ds.CreateLayer('mylayer')

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(500000 1000000)'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((500000 1000000,510000 1000000,510000 1100000,500000 1000000))'))
    lyr.CreateFeature(f)

    out_ds = gdal.VectorTranslate('/vsimem/out.mbtiles', src_ds, format='MVT',
                                  datasetCreationOptions=['MAX_FEATURES=1'])
    assert out_ds is not None
    out_ds = None

    out_ds = ogr.Open('/vsimem/out.mbtiles')
    assert out_ds is not None
    out_lyr = out_ds.GetLayerByName('mylayer')
    out_f = out_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(out_f, 'POLYGON ((499898.164985053 1000102.07808325,509987.852718696 1100081.71108026,509987.852718696 1000102.07808325,499898.164985053 1000102.07808325))') != 0:
        out_f.DumpReadable()
        pytest.fail()
    out_f = out_lyr.GetNextFeature()
    if out_f is not None:
        out_f.DumpReadable()
        pytest.fail()
    out_ds = None

    gdal.Unlink('/vsimem/out.mbtiles')

###############################################################################


def test_ogr_mvt_write_custom_tiling_scheme():

    if not ogrtest.have_geos() or ogr.GetDriverByName('SQLITE') is None:
        pytest.skip()

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")
    lyr = src_ds.CreateLayer('mylayer', srs=srs)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(12 71,13 72)'))
    lyr.CreateFeature(f)

    out_ds = gdal.VectorTranslate('/vsimem/out', src_ds, format='MVT',
                                  datasetCreationOptions=['TILING_SCHEME=EPSG:3067,-548576,8388608,2097152'])
    assert out_ds is not None
    out_ds = None

    out_ds = ogr.Open('/vsimem/out/1')
    assert out_ds is not None
    out_lyr = out_ds.GetLayerByName('mylayer')
    assert out_lyr.GetSpatialRef().ExportToWkt().find('3067') >= 0
    out_f = out_lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(out_f, 'MULTILINESTRING ((-40160 7944704,21024 8044800))') != 0:
        out_f.DumpReadable()
        pytest.fail()
    out_ds = None

    gdal.RmdirRecursive('/vsimem/out')

###############################################################################


def test_ogr_mvt_write_errors():

    if not ogrtest.have_geos() or ogr.GetDriverByName('SQLITE') is None:
        pytest.skip()

    # Raster creation attempt
    gdal.RmdirRecursive('/vsimem/foo')
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('MVT').Create('/vsimem/foo', 1, 1)
    assert ds is None

    # should have mbtiles extension
    gdal.RmdirRecursive('/vsimem/foo.bar')
    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo.bar',
                                                         options=['FORMAT=MBTILES'])
    assert ds is None

    # Cannot create temporary database
    gdal.RmdirRecursive('/vsimem/foo')
    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo',
                                                         options=['TEMPORARY_DB=/i/do_not/exist.db'])
    assert ds is None

    # cannot create mbtiles file
    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('MVT').CreateDataSource('/i/do_not/exist.mbtiles',
                                                         options=['TEMPORARY_DB=/vsimem/temp.db'])
    assert ds is None
    gdal.Unlink('/vsimem/temp.db')

    # invalid MINZOOM
    gdal.RmdirRecursive('/vsimem/foo')
    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo',
                                                         options=['MINZOOM=-1'])
    assert ds is None
    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo',
                                                         options=['MINZOOM=30'])
    assert ds is None

    # invalid MAXZOOM
    gdal.RmdirRecursive('/vsimem/foo')
    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo',
                                                         options=['MAXZOOM=-1'])
    assert ds is None
    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo',
                                                         options=['MAXZOOM=30'])
    assert ds is None

    # invalid MINZOOM vs MAXZOOM
    gdal.RmdirRecursive('/vsimem/foo')
    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo',
                                                         options=['MINZOOM=1', 'MAXZOOM=0'])
    assert ds is None

    # invalid MINZOOM for layer
    gdal.RmdirRecursive('/vsimem/foo')
    ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo')
    with gdaltest.error_handler():
        lyr = ds.CreateLayer('foo', options=['MINZOOM=-1'])
    assert lyr is None

    # invalid CONF
    gdal.RmdirRecursive('/vsimem/foo')
    gdal.FileFromMemBuffer('/vsimem/invalid.json', 'foo bar')
    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo',
                                                         options=['CONF=/vsimem/invalid.json'])
    gdal.Unlink('/vsimem/invalid.json')
    assert ds is None

    # TILING_SCHEME not allowed with MBTILES
    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo.mbtiles',
                                                         options=['TILING_SCHEME=EPSG:4326,-180,180,360'])
    assert ds is None

    # Invalid TILING_SCHEME
    gdal.RmdirRecursive('/vsimem/foo')
    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo',
                                                         options=['TILING_SCHEME=EPSG:4326'])
    assert ds is None

    # Test failure in creating tile
    gdal.RmdirRecursive('tmp/tmpmvt')
    ds = ogr.GetDriverByName('MVT').CreateDataSource('tmp/tmpmvt')
    gdal.RmdirRecursive('tmp/tmpmvt')
    lyr = ds.CreateLayer('test')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    with gdaltest.error_handler():
        ds = None
    assert gdal.GetLastErrorMsg() != ''
    gdal.RmdirRecursive('tmp/tmpmvt')

    # Test failure in writing in temp db (multi-threaded)
    gdal.RmdirRecursive('/vsimem/foo')
    with gdaltest.config_option('OGR_MVT_REMOVE_TEMP_FILE', 'NO'):
        ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo')
    temp_ds = ogr.Open('/vsimem/foo.temp.db', update=1)
    temp_ds.ExecuteSQL('DROP TABLE temp')
    temp_ds = None
    gdal.Unlink('/vsimem/foo.temp.db')
    lyr = ds.CreateLayer('test')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(0 0))'))
    with gdaltest.error_handler():
        lyr.CreateFeature(f)
        ds = None
    assert gdal.GetLastErrorMsg() != ''
    gdal.RmdirRecursive('tmp/tmpmvt')

    # Test failure in writing in temp db (single-threaded)
    gdal.RmdirRecursive('/vsimem/foo')
    with gdaltest.config_option('OGR_MVT_REMOVE_TEMP_FILE', 'NO'):
        with gdaltest.config_option('GDAL_NUM_THREADS', '1'):
            ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo')
    temp_ds = ogr.Open('/vsimem/foo.temp.db', update=1)
    temp_ds.ExecuteSQL('DROP TABLE temp')
    temp_ds = None
    gdal.Unlink('/vsimem/foo.temp.db')
    lyr = ds.CreateLayer('test')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(0 0))'))
    with gdaltest.error_handler():
        lyr.CreateFeature(f)
        ds = None
    assert gdal.GetLastErrorMsg() != ''
    gdal.RmdirRecursive('tmp/tmpmvt')

    # Test reprojection failure
    gdal.RmdirRecursive('/vsimem/foo')
    ds = ogr.GetDriverByName('MVT').CreateDataSource('/vsimem/foo')
    with gdaltest.error_handler():
        lyr = ds.CreateLayer('test', srs=osr.SpatialReference())
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    ds = None

    gdal.RmdirRecursive('/vsimem/foo')

###############################################################################
#


def test_ogr_mvt_write_reuse_temp_db():

    if not ogrtest.have_geos() or ogr.GetDriverByName('SQLITE') is None:
        pytest.skip()

    src_ds = gdal.GetDriverByName('Memory').Create('', 0, 0, 0, gdal.GDT_Unknown)
    lyr = src_ds.CreateLayer('mylayer')

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(0 0,100000 100000,200000 0)'))
    lyr.CreateFeature(f)

    with gdaltest.config_option('OGR_MVT_REMOVE_TEMP_FILE', 'NO'):
        gdal.VectorTranslate('/vsimem/out', src_ds, format='MVT')

    assert gdal.VSIStatL('/vsimem/out.temp.db') is not None

    gdal.RmdirRecursive('/vsimem/out')

    with gdaltest.config_option('OGR_MVT_REUSE_TEMP_FILE', 'YES'):
        gdal.VectorTranslate('/vsimem/out', src_ds, format='MVT')

    out_ds = ogr.Open('/vsimem/out/5')
    assert out_ds is not None
    out_lyr = out_ds.GetLayerByName('mylayer')
    out_f = out_lyr.GetNextFeature()
    assert out_f is not None
    out_ds = None

    gdal.RmdirRecursive('/vsimem/out')
    gdal.Unlink('/vsimem/out.temp.db')


###############################################################################
#
