#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for OGR Parquet driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Planet Labs
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
import math
from osgeo import gdal, ogr, osr
import gdaltest
import pytest

pytestmark = pytest.mark.require_driver('Parquet')

###############################################################################
# Read invalid file


def test_ogr_parquet_invalid():

    with gdaltest.error_handler():
        assert ogr.Open('data/parquet/invalid.parquet') is None


###############################################################################
# Basic tests


def _check_test_parquet(filename,
                        expect_fast_feature_count=True,
                        expect_fast_get_extent=True,
                        expect_ignore_fields=True):
    with gdaltest.config_option('OGR_PARQUET_BATCH_SIZE', '2'):
        ds = gdal.OpenEx(filename)
    assert ds is not None, 'cannot open dataset'
    assert ds.TestCapability("foo") == 0
    assert ds.GetLayerCount() == 1, 'bad layer count'
    assert ds.GetLayer(-1) is None
    assert ds.GetLayer(1) is None
    lyr = ds.GetLayer(0)
    assert lyr is not None
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetGeomFieldCount() == 1
    assert lyr_defn.GetGeomFieldDefn(0).GetName() == 'geometry'
    srs = lyr_defn.GetGeomFieldDefn(0).GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == '4326'
    assert lyr_defn.GetGeomFieldDefn(0).GetType() == ogr.wkbPoint
    assert lyr_defn.GetFieldCount() == 71
    got_field_defns = [
        (lyr_defn.GetFieldDefn(i).GetName(),
         ogr.GetFieldTypeName(lyr_defn.GetFieldDefn(i).GetType()),
         ogr.GetFieldSubTypeName(lyr_defn.GetFieldDefn(i).GetSubType()),
         lyr_defn.GetFieldDefn(i).GetWidth(),
         lyr_defn.GetFieldDefn(i).GetPrecision()) for i in range(lyr_defn.GetFieldCount()) ]
    #import pprint
    #pprint.pprint(got_field_defns)
    expected_field_defns = [
        ('boolean', 'Integer', 'Boolean', 0, 0),
        ('uint8', 'Integer', 'None', 0, 0),
        ('int8', 'Integer', 'None', 0, 0),
        ('uint16', 'Integer', 'None', 0, 0),
        ('int16', 'Integer', 'Int16', 0, 0),
        ('uint32', 'Integer64', 'None', 0, 0),
        ('int32', 'Integer', 'None', 0, 0),
        ('uint64', 'Real', 'None', 0, 0),
        ('int64', 'Integer64', 'None', 0, 0),
        ('float32', 'Real', 'Float32', 0, 0),
        ('float64', 'Real', 'None', 0, 0),
        ('string', 'String', 'None', 0, 0),
        ('large_string', 'String', 'None', 0, 0),
        ('timestamp_ms_gmt', 'DateTime', 'None', 0, 0),
        ('timestamp_ms_gmt_plus_2', 'DateTime', 'None', 0, 0),
        ('timestamp_ms_gmt_minus_0215', 'DateTime', 'None', 0, 0),
        ('timestamp_s_no_tz', 'DateTime', 'None', 0, 0),
        ('time32_s', 'Time', 'None', 0, 0),
        ('time32_ms', 'Time', 'None', 0, 0),
        ('time64_us', 'Integer64', 'None', 0, 0),
        ('time64_ns', 'Integer64', 'None', 0, 0),
        ('date32', 'Date', 'None', 0, 0),
        ('date64', 'Date', 'None', 0, 0),
        ('binary', 'Binary', 'None', 0, 0),
        ('large_binary', 'Binary', 'None', 0, 0),
        ('fixed_size_binary', 'Binary', 'None', 2, 0),
        ('decimal128', 'Real', 'None', 7, 3),
        ('decimal256', 'Real', 'None', 7, 3),
        ('list_boolean', 'IntegerList', 'Boolean', 0, 0),
        ('list_uint8', 'IntegerList', 'None', 0, 0),
        ('list_int8', 'IntegerList', 'None', 0, 0),
        ('list_uint16', 'IntegerList', 'None', 0, 0),
        ('list_int16', 'IntegerList', 'None', 0, 0),
        ('list_uint32', 'Integer64List', 'None', 0, 0),
        ('list_int32', 'IntegerList', 'None', 0, 0),
        ('list_uint64', 'RealList', 'None', 0, 0),
        ('list_int64', 'Integer64List', 'None', 0, 0),
        ('list_float32', 'RealList', 'Float32', 0, 0),
        ('list_float64', 'RealList', 'None', 0, 0),
        ('list_string', 'StringList', 'None', 0, 0),
        ('fixed_size_list_boolean', 'IntegerList', 'Boolean', 0, 0),
        ('fixed_size_list_uint8', 'IntegerList', 'None', 0, 0),
        ('fixed_size_list_int8', 'IntegerList', 'None', 0, 0),
        ('fixed_size_list_uint16', 'IntegerList', 'None', 0, 0),
        ('fixed_size_list_int16', 'IntegerList', 'None', 0, 0),
        ('fixed_size_list_uint32', 'Integer64List', 'None', 0, 0),
        ('fixed_size_list_int32', 'IntegerList', 'None', 0, 0),
        ('fixed_size_list_uint64', 'RealList', 'None', 0, 0),
        ('fixed_size_list_int64', 'Integer64List', 'None', 0, 0),
        ('fixed_size_list_float32', 'RealList', 'Float32', 0, 0),
        ('fixed_size_list_float64', 'RealList', 'None', 0, 0),
        ('fixed_size_list_string', 'StringList', 'None', 0, 0),
        ('struct_field.a', 'Integer64', 'None', 0, 0),
        ('struct_field.b', 'Real', 'None', 0, 0),
        ('struct_field.c.d', 'String', 'None', 0, 0),
        ('struct_field.c.f', 'String', 'None', 0, 0),
        ('struct_field.h', 'Integer64List', 'None', 0, 0),
        ('struct_field.i', 'Integer64', 'None', 0, 0),
        ('map_boolean', 'String', 'JSON', 0, 0),
        ('map_uint8', 'String', 'JSON', 0, 0),
        ('map_int8', 'String', 'JSON', 0, 0),
        ('map_uint16', 'String', 'JSON', 0, 0),
        ('map_int16', 'String', 'JSON', 0, 0),
        ('map_uint32', 'String', 'JSON', 0, 0),
        ('map_int32', 'String', 'JSON', 0, 0),
        ('map_uint64', 'String', 'JSON', 0, 0),
        ('map_int64', 'String', 'JSON', 0, 0),
        ('map_float32', 'String', 'JSON', 0, 0),
        ('map_float64', 'String', 'JSON', 0, 0),
        ('map_string', 'String', 'JSON', 0, 0),
        ('dict', 'Integer', 'None', 0, 0)
    ]
    assert got_field_defns == expected_field_defns
    if expect_fast_feature_count:
        assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1
    assert lyr.TestCapability(ogr.OLCStringsAsUTF8) == 1
    if expect_fast_get_extent:
        assert lyr.TestCapability(ogr.OLCFastGetExtent) == 1
    if expect_ignore_fields:
        assert lyr.TestCapability(ogr.OLCIgnoreFields) == 1
    assert lyr.GetFeatureCount() == 5
    assert lyr.GetExtent() == (0.0, 4.0, 2.0, 2.0)
    assert lyr.GetExtent(geom_field=0) == (0.0, 4.0, 2.0, 2.0)
    with gdaltest.error_handler():
        lyr.GetExtent(geom_field=-1)
        lyr.GetExtent(geom_field=1)

    assert ds.GetFieldDomainNames() == ['dictDomain']
    assert ds.GetFieldDomain('not_existing') is None
    for _ in range(2):
        domain = ds.GetFieldDomain('dictDomain')
        assert domain is not None
        assert domain.GetName() == 'dictDomain'
        assert domain.GetDescription() == ''
        assert domain.GetDomainType() == ogr.OFDT_CODED
        assert domain.GetFieldType() == ogr.OFTInteger
        assert domain.GetFieldSubType() == ogr.OFSTNone
        assert domain.GetEnumeration() == {'0': 'foo', '1': 'bar', '2': 'baz'}

    f = lyr.GetNextFeature()
    assert f.GetFID() == 0
    assert f['boolean']
    assert f['uint8'] == 1
    assert f['int8'] == -2
    assert f['uint16'] == 1
    assert f['int16'] == -20000
    assert f['uint32'] == 1
    assert f['int32'] == -2000000000
    assert f['uint64'] == 1
    assert f['int64'] == -200000000000
    assert f['float32'] == 1.5
    assert f['float64'] == 1.5
    assert f['string'] == 'abcd'
    assert f['large_string'] == 'abcd'
    assert f['timestamp_ms_gmt'] == '2019/01/01 14:00:00+00'
    assert f['timestamp_ms_gmt_plus_2'] == '2019/01/01 14:00:00+02'
    assert f['timestamp_ms_gmt_minus_0215'] == '2019/01/01 14:00:00-0215'
    assert f['timestamp_s_no_tz'] == '2019/01/01 14:00:00'
    assert f['time32_s'] == '01:02:03'
    assert f['time32_ms'] == '01:02:03.456'
    assert f['time64_us'] == 3723000000
    assert f['time64_ns'] == 3723000000456
    assert f['date32'] == '1970/01/02'
    assert f['date64'] == '1970/01/02'
    assert f['binary'] == '0001'
    assert f['large_binary'] == '0001'
    assert f['fixed_size_binary'] == '0001'
    assert f['decimal128'] == 1234.567
    assert f['decimal256'] == 1234.567
    assert f['list_boolean'] == []
    assert f['list_uint8'] == []
    assert f['list_int8'] == []
    assert f['list_uint16'] == []
    assert f['list_int16'] == []
    assert f['list_uint32'] == []
    assert f['list_int32'] == []
    assert f['list_uint64'] == []
    assert f['list_int64'] == []
    assert f['list_float32'] == []
    assert f['list_float64'] == []
    assert f['list_string'] is None
    assert f['fixed_size_list_boolean'] == [1, 0]
    assert f['fixed_size_list_uint8'] == [0, 1]
    assert f['fixed_size_list_int8'] == [0, 1]
    assert f['fixed_size_list_uint16'] == [0, 1]
    assert f['fixed_size_list_int16'] == [0, 1]
    assert f['fixed_size_list_uint32'] == [0, 1]
    assert f['fixed_size_list_int32'] == [0, 1]
    assert f['fixed_size_list_uint64'] == [0, 1]
    assert f['fixed_size_list_int64'] == [0, 1]
    assert f['fixed_size_list_float32'][0] == 0
    assert math.isnan(f['fixed_size_list_float32'][1])
    assert f['fixed_size_list_float64'][0] == 0
    assert math.isnan(f['fixed_size_list_float64'][1])
    assert f['fixed_size_list_string'] == ['a', 'b']
    assert f['struct_field.a'] == 1
    assert f['struct_field.b'] == 2.5
    assert f['struct_field.c.d'] == 'e'
    assert f['struct_field.c.f'] == 'g'
    assert f['struct_field.h'] == [5,6]
    assert f['struct_field.i'] == 3
    assert f['map_boolean'] == '{"x":null,"y":true}'
    assert f['map_uint8'] == '{"x":1,"y":null}'
    assert f['map_int8'] == '{"x":1,"y":null}'
    assert f['map_uint16'] == '{"x":1,"y":null}'
    assert f['map_int16'] == '{"x":1,"y":null}'
    assert f['map_uint32'] == '{"x":4000000000,"y":null}'
    assert f['map_int32'] == '{"x":2000000000,"y":null}'
    assert f['map_uint64'] == '{"x":4000000000000.0,"y":null}'
    assert f['map_int64'] == '{"x":-2000000000000,"y":null}'
    assert f['map_float32'] == '{"x":1.5,"y":null}'
    assert f['map_float64'] == '{"x":1.5,"y":null}'
    assert f['map_string'] == '{"x":"x_val","y":null}'
    assert f['dict'] == 0
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (0 2)'

    f = lyr.GetNextFeature()
    assert f.GetFID() == 1
    assert not f['boolean']
    assert f['uint8'] == 2
    assert f.GetGeometryRef() is None

    f = lyr.GetNextFeature()
    assert f.GetFID() == 2
    assert f['uint8'] is None
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (2 2)'

    f = lyr.GetNextFeature()
    assert f.GetFID() == 3
    assert f['uint8'] == 4
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (3 2)'

    f = lyr.GetNextFeature()
    assert f.GetFID() == 4
    assert f['uint8'] == 5
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (4 2)'

    assert lyr.GetNextFeature() is None

    assert lyr.GetNextFeature() is None

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetFID() == 0

    lyr.SetSpatialFilterRect(4,2,4,2)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetFID() == 4
    lyr.SetSpatialFilter(None)

    if expect_ignore_fields:
        # Ignore just one member of a structure
        assert lyr.SetIgnoredFields(['struct_field.a']) == ogr.OGRERR_NONE
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f['fixed_size_list_string'] == ['a', 'b']
        assert f['struct_field.a'] is None
        assert f['struct_field.b'] == 2.5
        assert f['map_boolean'] == '{"x":null,"y":true}'
        assert f.GetGeometryRef().ExportToWkt() == 'POINT (0 2)'

        # Ignore all members of a structure
        assert lyr.SetIgnoredFields(['struct_field.a',
                                     'struct_field.b',
                                     'struct_field.c.d',
                                     'struct_field.c.f',
                                     'struct_field.h',
                                     'struct_field.i']) == ogr.OGRERR_NONE
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f['fixed_size_list_string'] == ['a', 'b']
        assert f['struct_field.a'] is None
        assert f['struct_field.b'] is None
        assert f['struct_field.c.d'] is None
        assert f['struct_field.c.f'] is None
        assert f['struct_field.h'] is None
        assert f['struct_field.i'] is None
        assert f['map_boolean'] == '{"x":null,"y":true}'
        assert f.GetGeometryRef().ExportToWkt() == 'POINT (0 2)'

        # Ignore a map
        assert lyr.SetIgnoredFields(['map_boolean']) == ogr.OGRERR_NONE
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f['fixed_size_list_string'] == ['a', 'b']
        assert f['struct_field.a'] == 1
        assert f['struct_field.b'] == 2.5
        assert f['map_boolean'] is None
        assert f['map_uint8'] == '{"x":1,"y":null}'
        assert f.GetGeometryRef().ExportToWkt() == 'POINT (0 2)'

        # Ignore geometry
        assert lyr.SetIgnoredFields(['geometry']) == ogr.OGRERR_NONE
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f['fixed_size_list_string'] == ['a', 'b']
        assert f['struct_field.a'] == 1
        assert f['struct_field.b'] == 2.5
        assert f['map_boolean'] == '{"x":null,"y":true}'
        assert f.GetGeometryRef() is None

        # Cancel ignored fields
        assert lyr.SetIgnoredFields([]) == ogr.OGRERR_NONE
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f['fixed_size_list_string'] == ['a', 'b']
        assert f['struct_field.a'] == 1
        assert f['struct_field.b'] == 2.5
        assert f['map_boolean'] == '{"x":null,"y":true}'
        assert f.GetGeometryRef().ExportToWkt() == 'POINT (0 2)'


@pytest.mark.parametrize("use_vsi", [False, True])
def test_ogr_parquet_1(use_vsi):

    filename = 'data/parquet/test.parquet'
    if use_vsi:
        vsifilename = '/vsimem/test.parquet'
        gdal.FileFromMemBuffer(vsifilename, open(filename, 'rb').read())
        filename = vsifilename

    try:
        _check_test_parquet(filename)
    finally:
        if use_vsi:
            gdal.Unlink(vsifilename)

###############################################################################
# Run test_ogrsf


def test_ogr_parquet_test_ogrsf_test():
    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/parquet/test.parquet')

    assert 'INFO' in ret
    assert 'ERROR' not in ret

###############################################################################
# Run test_ogrsf


def test_ogr_parquet_test_ogrsf_example():
    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/parquet/example.parquet')

    assert 'INFO' in ret
    assert 'ERROR' not in ret

###############################################################################
# Run test_ogrsf


def test_ogr_parquet_test_ogrsf_all_geoms():
    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/parquet/all_geoms.parquet')

    assert 'INFO' in ret
    assert 'ERROR' not in ret


###############################################################################
# Test write support


@pytest.mark.parametrize("use_vsi,row_group_size,fid", [(False, None, None), (True, 2, "fid")])
def test_ogr_parquet_write_from_another_dataset(use_vsi, row_group_size, fid):

    outfilename = '/vsimem/out.parquet' if use_vsi else 'tmp/out.parquet'
    try:
        layerCreationOptions = []
        if row_group_size:
            layerCreationOptions.append('ROW_GROUP_SIZE=' + str(row_group_size))
        if fid:
            layerCreationOptions.append('FID=' + fid)
        gdal.VectorTranslate(outfilename, 'data/parquet/test.parquet',
                             layerCreationOptions=layerCreationOptions)

        ds = gdal.OpenEx(outfilename)
        lyr = ds.GetLayer(0)

        assert lyr.GetFIDColumn() == (fid if fid else "")

        if fid:
            f = lyr.GetFeature(4)
            assert f is not None
            assert f.GetFID() == 4

            assert lyr.GetFeature(5) is None

            lyr.SetIgnoredFields([lyr.GetLayerDefn().GetFieldDefn(0).GetName()])

            f = lyr.GetFeature(4)
            assert f is not None
            assert f.GetFID() == 4

            assert lyr.GetFeature(5) is None

            lyr.SetIgnoredFields([])

        if row_group_size:
            num_features = lyr.GetFeatureCount()
            expected_num_row_groups = int(math.ceil(num_features / row_group_size))
            assert lyr.GetMetadataItem("NUM_ROW_GROUPS", "_PARQUET_") == str(expected_num_row_groups)
            for i in range(expected_num_row_groups):
                got_num_rows = lyr.GetMetadataItem("ROW_GROUPS[%d].NUM_ROWS" % i, "_PARQUET_")
                if i < expected_num_row_groups - 1:
                    assert got_num_rows == str(row_group_size)
                else:
                    assert got_num_rows == str(num_features - (expected_num_row_groups - 1) * row_group_size)

        geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
        assert geo is not None
        j = json.loads(geo)
        assert j is not None
        assert 'primary_column' in j
        assert j['primary_column'] == 'geometry'
        assert 'columns' in j
        assert 'geometry' in j['columns']
        assert 'encoding' in j['columns']['geometry']
        assert j['columns']['geometry']['encoding'] == 'WKB'

        md = lyr.GetMetadata("_PARQUET_METADATA_")
        assert 'geo' in md

        ds = None

        _check_test_parquet(outfilename)

    finally:
        gdal.Unlink(outfilename)


###############################################################################
# Test write support


def test_ogr_parquet_write_edge_cases():

    outfilename = '/vsimem/out.parquet'

    # No layer
    ds = gdal.GetDriverByName('Parquet').Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    assert ds is not None
    assert ds.GetLayerCount() == 0
    assert ds.GetLayer(0) is None
    assert ds.TestCapability(ogr.ODsCCreateLayer) == 1
    assert ds.TestCapability(ogr.ODsCAddFieldDomain) == 0
    domain = ogr.CreateCodedFieldDomain('name', 'desc', ogr.OFTInteger, ogr.OFSTNone, {1: "one", "2": None})
    assert ds.AddFieldDomain(domain) == False
    assert ds.GetFieldDomainNames() is None
    assert ds.GetFieldDomain('foo') is None
    ds = None
    gdal.Unlink(outfilename)

    # No field, no record
    ds = gdal.GetDriverByName('Parquet').Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    assert ds is not None
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    with gdaltest.error_handler():
        assert ds.CreateLayer('out', srs=srs, geom_type=ogr.wkbPointM) is None
        assert ds.CreateLayer('out', srs=srs, geom_type=ogr.wkbPoint, options=['COMPRESSION=invalid']) is None
    lyr = ds.CreateLayer('out', srs=srs, geom_type=ogr.wkbPoint)
    assert lyr is not None
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(0) is not None
    assert ds.TestCapability(ogr.ODsCCreateLayer) == 0
    assert ds.TestCapability(ogr.ODsCAddFieldDomain) == 1
    # Test creating a second layer
    with gdaltest.error_handler():
        assert ds.CreateLayer('out2', srs=srs, geom_type=ogr.wkbPoint) is None
    ds = None
    ds = gdal.OpenEx(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetNextFeature() is None
    lyr = None
    ds = None
    gdal.Unlink(outfilename)

    # No geometry field, one record
    ds = gdal.GetDriverByName('Parquet').Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    assert ds is not None
    lyr = ds.CreateLayer('out', geom_type=ogr.wkbNone)
    assert lyr.TestCapability(ogr.OLCCreateField) == 1
    assert lyr.TestCapability(ogr.OLCCreateGeomField) == 1
    assert lyr.TestCapability(ogr.OLCSequentialWrite) == 1
    assert lyr.TestCapability(ogr.OLCStringsAsUTF8) == 1
    fld_defn = ogr.FieldDefn('foo')
    fld_defn.SetNullable(False)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
    assert lyr is not None
    f = ogr.Feature(lyr.GetLayerDefn())
    with gdaltest.error_handler():
        # violation of not-null constraint
        assert lyr.CreateFeature(f) != ogr.OGRERR_NONE
    f['foo'] = 'bar'
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == 1
    assert lyr.TestCapability(ogr.OLCCreateField) == 0
    assert lyr.TestCapability(ogr.OLCCreateGeomField) == 0
    with gdaltest.error_handler():
        assert lyr.CreateField(ogr.FieldDefn('bar')) != ogr.OGRERR_NONE
        assert lyr.CreateGeomField(ogr.GeomFieldDefn('baz', ogr.wkbPoint)) != ogr.OGRERR_NONE
    ds = None
    ds = gdal.OpenEx(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetNextFeature() is not None
    lyr = None
    ds = None
    gdal.Unlink(outfilename)


###############################################################################
# Test compression support


@pytest.mark.parametrize("compression", ['uncompressed', 'snappy', 'zstd'])
def test_ogr_parquet_write_compression(compression):

    lco = gdal.GetDriverByName('Parquet').GetMetadataItem("DS_LAYER_CREATIONOPTIONLIST")
    if compression.upper() not in lco:
        pytest.skip()

    outfilename = '/vsimem/out.parquet'
    ds = gdal.GetDriverByName('Parquet').Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    options = ['FID=fid', 'COMPRESSION=' + compression]
    lyr = ds.CreateLayer('out', geom_type=ogr.wkbNone, options=options)
    assert lyr is not None
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    lyr = None
    ds = None

    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadataItem('ROW_GROUPS[0].COLUMNS[0].COMPRESSION', '_PARQUET_') == compression
    lyr = None
    ds = None

    gdal.Unlink(outfilename)


###############################################################################
# Test coordinate epoch support


def test_ogr_parquet_coordinate_epoch():

    outfilename = '/vsimem/out.parquet'
    ds = gdal.GetDriverByName('Parquet').Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srs.SetCoordinateEpoch(2022.3)
    ds.CreateLayer('out', geom_type=ogr.wkbPoint, srs=srs)
    ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None

    geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
    assert geo is not None
    j = json.loads(geo)
    assert j is not None
    assert 'columns' in j
    assert 'geometry' in j['columns']
    assert 'crs' in j['columns']['geometry']
    assert j['columns']['geometry']['crs'].startswith('GEOGCRS')
    assert 'epoch' in j['columns']['geometry']

    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == '4326'
    assert srs.GetCoordinateEpoch() == 2022.3
    lyr = None
    ds = None

    gdal.Unlink(outfilename)


###############################################################################
# Test missing CRS member (on reading, implicitly fallback to EPSG:4326)


def test_ogr_parquet_missing_crs_member():

    outfilename = '/vsimem/out.parquet'
    ds = gdal.GetDriverByName('Parquet').Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    with gdaltest.config_option('OGR_PARQUET_WRITE_CRS', 'NO'):
        ds.CreateLayer('out', geom_type=ogr.wkbPoint)
        ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None

    geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
    assert geo is not None
    j = json.loads(geo)
    assert j is not None
    assert 'columns' in j
    assert 'geometry' in j['columns']
    assert 'crs' not in j['columns']['geometry']

    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == '4326'
    lyr = None
    ds = None

    gdal.Unlink(outfilename)


###############################################################################
# Test CRS as PROJJSON (extension)


def test_ogr_parquet_crs_as_projjson():

    outfilename = '/vsimem/out.parquet'
    ds = gdal.GetDriverByName('Parquet').Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    with gdaltest.config_option('OGR_PARQUET_CRS_ENCODING', 'PROJJSON'):
        ds.CreateLayer('out', geom_type=ogr.wkbPoint, srs=srs)
        ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None

    geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
    assert geo is not None
    j = json.loads(geo)
    assert j is not None
    assert 'columns' in j
    assert 'geometry' in j['columns']
    assert 'crs' in j['columns']['geometry']
    assert 'type' in j['columns']['geometry']['crs']
    assert j['columns']['geometry']['crs']['type'] == 'GeographicCRS'

    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == '4326'
    lyr = None
    ds = None

    gdal.Unlink(outfilename)


###############################################################################
# Test EDGES option


@pytest.mark.parametrize("edges", [None, 'PLANAR', 'SPHERICAL'])
def test_ogr_parquet_edges(edges):

    outfilename = '/vsimem/out.parquet'
    ds = gdal.GetDriverByName('Parquet').Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    options=[]
    if edges is not None:
        options = ['EDGES='+edges]
    ds.CreateLayer('out', geom_type=ogr.wkbPoint, options=options)
    ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None
    if edges == 'SPHERICAL':
        assert lyr.GetMetadataItem('EDGES') == 'SPHERICAL'
    else:
        assert lyr.GetMetadataItem('EDGES') is None
    ds = None

    gdal.Unlink(outfilename)


###############################################################################
# Test geometry_type support


@pytest.mark.parametrize("written_geom_type,written_wkt,expected_geom_type,expected_wkts", [
    (ogr.wkbPoint, ["POINT (1 2)"], ogr.wkbPoint, None),
    (ogr.wkbLineString, ["LINESTRING (1 2,3 4)"], ogr.wkbLineString, None),
    (ogr.wkbPolygon, ["POLYGON ((0 0,1 0,1 1,0 1,0 0))",
                      "POLYGON ((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2))"],  # winding order must be fixed
     ogr.wkbPolygon, ["POLYGON ((0 0,1 0,1 1,0 1,0 0))",
                      "POLYGON ((0 0,1 0,1 1,0 1,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.2 0.2))"]),
    (ogr.wkbMultiPoint, ["MULTIPOINT ((1 2))"], ogr.wkbMultiPoint, None),
    (ogr.wkbMultiLineString, ["MULTILINESTRING ((1 2,3 4))"], ogr.wkbMultiLineString, None),
    (ogr.wkbMultiPolygon, ["MULTIPOLYGON (((0 0,1 0,1 1,0 1,0 0)))"], ogr.wkbMultiPolygon, None),
    (ogr.wkbGeometryCollection, ["GEOMETRYCOLLECTION (POINT (1 2))"], ogr.wkbGeometryCollection, None),
    (ogr.wkbPoint25D, ["POINT Z (1 2 3)"], ogr.wkbPoint25D, None),
    (ogr.wkbUnknown, ["POINT (1 2)"], ogr.wkbPoint, None),
    (ogr.wkbUnknown, ["POINT Z (1 2 3)"], ogr.wkbPoint25D, None),
    (ogr.wkbUnknown, ["POLYGON ((0 0,0 1,1 1,1 0,0 0))",
                      "MULTIPOLYGON (((0 0,1 0,1 1,0 1,0 0)))"],
     ogr.wkbMultiPolygon, ["MULTIPOLYGON (((0 0,1 0,1 1,0 1,0 0)))",
                           "MULTIPOLYGON (((0 0,1 0,1 1,0 1,0 0)))"]),
    (ogr.wkbUnknown, ["LINESTRING (1 2,3 4)",
                      "MULTILINESTRING ((10 2,3 4))"],
     ogr.wkbMultiLineString, ["MULTILINESTRING ((1 2,3 4))",
                              "MULTILINESTRING ((10 2,3 4))"]),
    (ogr.wkbUnknown, ["LINESTRING Z (1 2 10,3 4 20)",
                      "MULTILINESTRING Z ((1 2 10,3 4 20))"],
     ogr.wkbMultiLineString25D, ["MULTILINESTRING Z ((1 2 10,3 4 20))",
                                 "MULTILINESTRING Z ((1 2 10,3 4 20))"]),
    (ogr.wkbUnknown, ["POINT (1 2)", "LINESTRING (1 2,3 4)"], ogr.wkbUnknown, None),
])
def test_ogr_parquet_geometry_type(written_geom_type,written_wkt,expected_geom_type,expected_wkts):

    outfilename = '/vsimem/out.parquet'
    ds = gdal.GetDriverByName('Parquet').Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    lyr = ds.CreateLayer('out', geom_type=written_geom_type)
    for wkt in written_wkt:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == expected_geom_type
    assert lyr.GetSpatialRef() is None
    if expected_wkts is None:
        expected_wkts = written_wkt
    for wkt in expected_wkts:
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef().ExportToIsoWkt() == wkt

    gdal.Unlink(outfilename)


###############################################################################
# Test POLYGON_ORIENTATION option


@pytest.mark.parametrize("option_value,written_wkt,expected_wkt", [
    (None,
     "POLYGON ((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2))",
     "POLYGON ((0 0,1 0,1 1,0 1,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.2 0.2))"),
    ("COUNTERCLOCKWISE",
     "POLYGON ((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2))",
     "POLYGON ((0 0,1 0,1 1,0 1,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.2 0.2))"),
    ("UNMODIFIED",
     "POLYGON ((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2))",
     "POLYGON ((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2))"),
])
def test_ogr_parquet_polygon_orientation(option_value,written_wkt,expected_wkt):

    outfilename = '/vsimem/out.parquet'
    ds = gdal.GetDriverByName('Parquet').Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    if option_value:
        options = ['POLYGON_ORIENTATION=' + option_value]
    else:
        options = []
    lyr = ds.CreateLayer('out', geom_type=ogr.wkbPolygon, options = options)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(written_wkt))
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == expected_wkt

    geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
    assert geo is not None
    j = json.loads(geo)
    assert j is not None
    assert 'columns' in j
    assert 'geometry' in j['columns']
    if option_value != 'UNMODIFIED':
        assert 'orientation' in j['columns']['geometry']
        assert j['columns']['geometry']['orientation'] == 'counterclockwise'
    else:
        assert 'orientation' not in j['columns']['geometry']

    gdal.Unlink(outfilename)



###############################################################################
# Test using statistics for SQL MIN, MAX, COUNT functions

def test_ogr_parquet_statistics():

    filename = 'data/parquet/test.parquet'
    ds = ogr.Open(filename)
    expected_fields = [
        ('boolean', 'Integer', 'Boolean', 0, 1),
        ('uint8', 'Integer', 'None', 1, 5),
        ('int8', 'Integer', 'None', -2, 2),
        ('uint16', 'Integer', 'None', 1, 40001),
        ('int16', 'Integer', 'Int16', -20000, 20000),
        ('uint32', 'Integer64', 'None', 1, 4000000001),
        ('int32', 'Integer', 'None', -2000000000, 2000000000),
        ('uint64', 'Integer64', 'None', 1, 400000000001),
        ('int64', 'Integer64', 'None', -200000000000, 200000000000),
        ('float32', 'Real', 'Float32', 1.5, 5.5),
        ('float64', 'Real', 'None', 1.5, 5.5),
        ('string', 'String', 'None', '', 'd'),
        ('timestamp_ms_gmt', 'DateTime', 'None', '2019/01/01 14:00:00+00', '2019/01/01 14:00:00+00'),
    ]

    sql = ''
    for field in expected_fields:
        name = field[0]
        if sql:
            sql += ', '
        else:
            sql = 'SELECT '
        sql += 'MIN(' + name + '), MAX(' + name + ')'
    sql += ', COUNT(int32)'
    sql += ' FROM test'
    sql_lyr = ds.ExecuteSQL(sql)
    try:
        f = sql_lyr.GetNextFeature()
        i = 0
        for name, type, subtype, minval, maxval in expected_fields:
            fld_defn = sql_lyr.GetLayerDefn().GetFieldDefn(i)
            assert fld_defn.GetName() == 'MIN_' + name
            assert ogr.GetFieldTypeName(fld_defn.GetType()) == type, name
            assert ogr.GetFieldSubTypeName(fld_defn.GetSubType()) == subtype, name
            assert f[fld_defn.GetName()] == minval, name
            i += 1

            fld_defn = sql_lyr.GetLayerDefn().GetFieldDefn(i)
            assert fld_defn.GetName() == 'MAX_' + name
            assert ogr.GetFieldTypeName(fld_defn.GetType()) == type, name
            assert ogr.GetFieldSubTypeName(fld_defn.GetSubType()) == subtype, name
            assert f[fld_defn.GetName()] == maxval, name
            i += 1

        fld_defn = sql_lyr.GetLayerDefn().GetFieldDefn(i)
        assert fld_defn.GetName() == 'COUNT_int32'
        assert f[fld_defn.GetName()] == 4
    finally:
        ds.ReleaseResultSet(sql_lyr)

    # Non-optimized due to WHERE condition
    sql_lyr = ds.ExecuteSQL('SELECT MIN(uint32) FROM test WHERE 1 = 0')
    try:
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) is None
    finally:
        ds.ReleaseResultSet(sql_lyr)

    # Non-optimized
    sql_lyr = ds.ExecuteSQL('SELECT AVG(uint8) FROM test')
    try:
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == 3.0
    finally:
        ds.ReleaseResultSet(sql_lyr)

    # Should be optimized, but this Parquet file has a wrong value for
    # distinct_count
    sql_lyr = ds.ExecuteSQL('SELECT COUNT(DISTINCT int32) FROM test')
    try:
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == 4
    finally:
        ds.ReleaseResultSet(sql_lyr)

    # Errors
    with gdaltest.error_handler():
        assert ds.ExecuteSQL('SELECT MIN(int32) FROM i_dont_exist') is None
        assert ds.ExecuteSQL('SELECT MIN(i_dont_exist) FROM test') is None

    # File without statistics
    outfilename = '/vsimem/out.parquet'
    try:
        with gdaltest.error_handler():
            gdal.VectorTranslate(outfilename, 'data/parquet/test.parquet',
                                 options='-lco STATISTICS=NO')
        ds = ogr.Open(outfilename)
        with gdaltest.error_handler():
            gdal.ErrorReset()
            # Generic OGR SQL doesn't support MIN() on string field
            sql_lyr = ds.ExecuteSQL('SELECT MIN(string) FROM out')
            assert sql_lyr is None
            assert gdal.GetLastErrorMsg() == 'Use of field function MIN() on string field string illegal.'
        ds = None

    finally:
        gdal.Unlink(outfilename)


###############################################################################
# Test setting/getting creator

def test_ogr_parquet_creator():

    outfilename = '/vsimem/out.parquet'
    try:
        ds = ogr.GetDriverByName('Parquet').CreateDataSource(outfilename)
        ds.CreateLayer('test')
        ds = None

        ds = gdal.OpenEx(outfilename)
        lyr = ds.GetLayer(0)
        assert lyr.GetMetadataItem("CREATOR", "_PARQUET_").startswith('GDAL ')
        ds = None

        ds = ogr.GetDriverByName('Parquet').CreateDataSource(outfilename)
        ds.CreateLayer('test', options = ['CREATOR=my_creator'])
        ds = None

        ds = gdal.OpenEx(outfilename)
        lyr = ds.GetLayer(0)
        assert lyr.GetMetadataItem("CREATOR", "_PARQUET_") == 'my_creator'
        ds = None

    finally:
        gdal.Unlink(outfilename)



###############################################################################
# Test creating multiple geometry columns

def test_ogr_parquet_multiple_geom_columns():

    outfilename = '/vsimem/out.parquet'
    try:
        ds = ogr.GetDriverByName('Parquet').CreateDataSource(outfilename)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)
        lyr.CreateGeomField(ogr.GeomFieldDefn('geom_point', ogr.wkbPoint)) == ogr.OGRERR_NONE
        lyr.CreateGeomField(ogr.GeomFieldDefn('geom_line', ogr.wkbLineString)) == ogr.OGRERR_NONE
        ds = None

        ds = gdal.OpenEx(outfilename)
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()
        assert lyr_defn.GetGeomFieldCount() == 2
        assert lyr_defn.GetGeomFieldDefn(0).GetName() == 'geom_point'
        assert lyr_defn.GetGeomFieldDefn(1).GetName() == 'geom_line'
        ds = None

    finally:
        gdal.Unlink(outfilename)


###############################################################################
# Test SetAttributeFilter()

@pytest.mark.parametrize("filter", [
    "boolean = 0",
    "boolean = 1",
    "boolean = 1.5",
    #"boolean = '0'",
    "uint8 = 2",
    "uint8 = -1",
    "int8 = -1",
    "int8 = 0",
    "int8 != 0",
    "int8 < 0",
    "int8 > 0",
    "int8 <= 0",
    "int8 >= 0",
    "0 = int8",
    "0 != int8",
    "0 < int8",
    "0 > int8",
    "0 <= int8",
    "0 >= int8",
    "int8 IS NULL",
    "int8 IS NOT NULL",
    "uint16 = 10001",
    "uint32 = 1000000001",
    "uint32 != 1000000001",
    "uint32 < 1000000001",
    "uint32 > 1000000001",
    "uint32 <= 1000000001",
    "uint32 >= 1000000001",
    "int32 = -1000000000",
    "uint64 = 100000000001",
    "int64 = -100000000000",
    "float32 = 2.5",
    "float64 = 2.5",
    "float64 != 2.5",
    "float64 < 2.5",
    "float64 > 2.5",
    "float64 <= 2.5",
    "float64 >= 2.5",
    "string = ''",
    "string != ''",
    "string < 'l'",
    "string > 'l'",
    "string <= 'l'",
    "string >= 'l'",
    "decimal128 = -1234.567",
    "decimal256 = -1234.567",

     # not optimized
    "boolean = 0 OR boolean = 1",
    "1 = 1",
    "boolean = boolean",
])
def test_ogr_parquet_attribute_filter(filter):

    with gdaltest.config_option('OGR_PARQUET_OPTIMIZED_ATTRIBUTE_FILTER', 'NO'):
        ds = ogr.Open('data/parquet/test.parquet')
        lyr = ds.GetLayer(0)
        assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
        ref_fc = lyr.GetFeatureCount()
        ds = None

    ds = ogr.Open('data/parquet/test.parquet')
    lyr = ds.GetLayer(0)
    assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == ref_fc


def test_ogr_parquet_attribute_filter_and_spatial_filter():

    filter = 'int8 != 0'

    with gdaltest.config_option('OGR_PARQUET_OPTIMIZED_ATTRIBUTE_FILTER', 'NO'):
        ds = ogr.Open('data/parquet/test.parquet')
        lyr = ds.GetLayer(0)
        lyr.SetSpatialFilterRect(4, 2, 4, 2)
        assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
        ref_fc = lyr.GetFeatureCount()
        assert ref_fc > 0
        ds = None

    ds = ogr.Open('data/parquet/test.parquet')
    lyr = ds.GetLayer(0)
    lyr.SetSpatialFilterRect(4, 2, 4, 2)
    assert lyr.SetAttributeFilter(filter) == ogr.OGRERR_NONE
    assert lyr.GetFeatureCount() == ref_fc
