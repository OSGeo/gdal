#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR ODS driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
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

import os
import shutil


import gdaltest
from osgeo import gdal
from osgeo import ogr
import pytest

pytestmark = pytest.mark.require_driver('ODS')

###############################################################################
# Check


def ogr_ods_check(ds):

    assert ds.TestCapability("foo") == 0

    assert ds.GetLayerCount() == 8, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Feuille1', 'bad layer name'

    assert lyr.GetGeomType() == ogr.wkbNone, 'bad layer geometry type'

    assert lyr.GetSpatialRef() is None, 'bad spatial ref'

    assert lyr.GetFeatureCount() == 26

    assert lyr.TestCapability("foo") == 0

    lyr = ds.GetLayer(6)
    assert lyr.GetName() == 'Feuille7', 'bad layer name'

    assert lyr.GetLayerDefn().GetFieldCount() == 12

    type_array = [ogr.OFTString,
                  ogr.OFTInteger,
                  ogr.OFTReal,
                  ogr.OFTReal,
                  ogr.OFTDate,
                  ogr.OFTDateTime,
                  ogr.OFTReal,
                  ogr.OFTTime,
                  ogr.OFTReal,
                  ogr.OFTInteger,
                  ogr.OFTReal,
                  ogr.OFTDateTime]

    for i, typ in enumerate(type_array):
        assert lyr.GetLayerDefn().GetFieldDefn(i).GetType() == typ

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString(0) != 'val' or \
       feat.GetFieldAsInteger(1) != 23 or \
       feat.GetFieldAsDouble(2) != 3.45 or \
       feat.GetFieldAsDouble(3) != 0.52 or \
       feat.GetFieldAsString(4) != '2012/01/22' or \
       feat.GetFieldAsString(5) != '2012/01/22 18:49:00':
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.IsFieldSet(2):
        feat.DumpReadable()
        pytest.fail()


###############################################################################
# Basic tests


def test_ogr_ods_1():

    drv = ogr.GetDriverByName('ODS')
    assert drv.TestCapability("foo") == 0

    ds = ogr.Open('data/ods/test.ods')
    assert ds is not None, 'cannot open dataset'

    return ogr_ods_check(ds)

###############################################################################
# Basic tests


def test_ogr_ods_kspread_1():

    drv = ogr.GetDriverByName('ODS')
    assert drv.TestCapability("foo") == 0

    ds = ogr.Open('data/ods/test_kspread.ods')
    assert ds is not None, 'cannot open dataset'

    assert ds.TestCapability("foo") == 0

    assert ds.GetLayerCount() == 8, 'bad layer count'

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Feuille1', 'bad layer name'

    assert lyr.GetGeomType() == ogr.wkbNone, 'bad layer geometry type'

    assert lyr.GetSpatialRef() is None, 'bad spatial ref'

    assert lyr.GetFeatureCount() == 26

    assert lyr.TestCapability("foo") == 0

    lyr = ds.GetLayer(6)
    assert lyr.GetName() == 'Feuille7', 'bad layer name'

    assert lyr.GetLayerDefn().GetFieldCount() == 12

    type_array = [ogr.OFTString,
                  ogr.OFTInteger,
                  ogr.OFTReal,
                  ogr.OFTReal,
                  ogr.OFTDate,
                  ogr.OFTString,  # ogr.OFTDateTime,
                  ogr.OFTReal,
                  ogr.OFTTime,
                  ogr.OFTReal,
                  ogr.OFTInteger,
                  ogr.OFTReal,
                  ogr.OFTString,  # ogr.OFTDateTime
                  ]

    for i, typ in enumerate(type_array):
        assert lyr.GetLayerDefn().GetFieldDefn(i).GetType() == typ

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString(0) != 'val' or \
       feat.GetFieldAsInteger(1) != 23 or \
       feat.GetFieldAsDouble(2) != 3.45 or \
       feat.GetFieldAsDouble(3) != 0.52 or \
       feat.GetFieldAsString(4) != '2012/01/22' or \
       feat.GetFieldAsString(5) != '22/01/2012 18:49:00':  # 2012/01/22 18:49:00
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.IsFieldSet(2):
        feat.DumpReadable()
        pytest.fail()


###############################################################################
# Test OGR_ODS_HEADERS = DISABLE


def test_ogr_ods_2():

    gdal.SetConfigOption('OGR_ODS_HEADERS', 'DISABLE')
    ds = ogr.Open('data/ods/test.ods')

    lyr = ds.GetLayerByName('Feuille7')

    assert lyr.GetFeatureCount() == 3

    gdal.SetConfigOption('OGR_ODS_HEADERS', None)

###############################################################################
# Test OGR_ODS_FIELD_TYPES = STRING


def test_ogr_ods_3():

    gdal.SetConfigOption('OGR_ODS_FIELD_TYPES', 'STRING')
    ds = ogr.Open('data/ods/test.ods')

    lyr = ds.GetLayerByName('Feuille7')

    assert lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString

    gdal.SetConfigOption('OGR_ODS_FIELD_TYPES', None)

###############################################################################
# Run test_ogrsf


def test_ogr_ods_4():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/ods/test.ods')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test write support


def test_ogr_ods_5():

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f ODS tmp/test.ods data/ods/test.ods')

    ds = ogr.Open('tmp/test.ods')
    ret = ogr_ods_check(ds)
    ds = None

    os.unlink('tmp/test.ods')

    return ret

###############################################################################
# Test formula evaluation


def test_ogr_ods_6():

    src_ds = ogr.Open('ODS:data/ods/content_formulas.xml')
    filepath = '/vsimem/content_formulas.csv'
    with gdaltest.error_handler():
        out_ds = ogr.GetDriverByName('CSV').CopyDataSource(src_ds, filepath)
    assert out_ds is not None, ('Unable to create %s.' % filepath)
    out_ds = None
    src_ds = None

    fp = gdal.VSIFOpenL('/vsimem/content_formulas.csv', 'rb')
    res = gdal.VSIFReadL(1, 10000, fp)
    gdal.VSIFCloseL(fp)

    gdal.Unlink('/vsimem/content_formulas.csv')

    res = res.decode('ascii').split()

    expected_res = """Field1,Field2,Field3,Field4,Field5,Field6,Field7,Field8,Field9,Field10,Field11,Field12,Field13,Field14,Field15,Field16,Field17,Field18,Field19,Field20,Field21,Field22,Field23,Field24,Field25,Field26,Field27,Field28,Field29,Field30,Field31,Field32
of:=[.B1],of:=[.C1],of:=[.A1],,,,,,,,,,,,,,,,,,,,,,,,,,,,,
"1","1","1",,,,,,,,,,,,,,,,,,,,,,,,,,,,,
ab,ab,ab,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
"1",a,,"3.5",MIN,"1",MIN,"3.5",SUM,"4.5",AVERAGE,"2.25",COUNT,"2",COUNTA,"3",,,,,,,,,,,,,,,,
abcdef,"6",,a,abcdef,,f,abcdef,"of:=MID([.A5];0;1)",,a,abcdef,,a,ef,ef,,,,,,,,,,,,,,,,
"1",,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
AB,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
"2","2","0","3","1","0","0","1","1","1","0","0","0","1","1","0",,,,,,,,,,,,,,,,
"1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1","1"
"0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0"
""".split()

    assert res == expected_res, ('did not get expected result: %s' % res)

###############################################################################
# Test update support


def test_ogr_ods_7():

    filepath = 'tmp/ogr_ods_7.ods'
    if os.path.exists(filepath):
        os.unlink(filepath)
    shutil.copy('data/ods/test.ods', filepath)

    ds = ogr.Open(filepath, update=1)
    lyr = ds.GetLayerByName('Feuille7')
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 2:
        feat.DumpReadable()
        pytest.fail('did not get expected FID')
    feat.SetField(0, 'modified_value')
    lyr.SetFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open('tmp/ogr_ods_7.ods')
    lyr = ds.GetLayerByName('Feuille7')
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 2:
        feat.DumpReadable()
        pytest.fail('did not get expected FID')
    if feat.GetField(0) != 'modified_value':
        feat.DumpReadable()
        pytest.fail('did not get expected value')
    feat = None
    ds = None

    os.unlink(filepath)

###############################################################################
# Test Integer64


def test_ogr_ods_8():

    drv = ogr.GetDriverByName('ODS')
    ds = drv.CreateDataSource('/vsimem/ogr_ods_8.ods')
    lyr = ds.CreateLayer('foo')
    lyr.CreateField(ogr.FieldDefn('Field1', ogr.OFTInteger64))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 1)
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 12345678901234)
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 1)
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_ods_8.ods')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger64
    f = lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    assert f.GetField(0) == 12345678901234
    ds = None

    gdal.Unlink('/vsimem/ogr_ods_8.ods')

###############################################################################
# Test DateTime with milliseconds


def test_ogr_ods_9():

    drv = ogr.GetDriverByName('ODS')
    ds = drv.CreateDataSource('/vsimem/ogr_ods_9.ods')
    lyr = ds.CreateLayer('foo')
    lyr.CreateField(ogr.FieldDefn('Field1', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('Field2', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('Field3', ogr.OFTDateTime))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, '2015/12/23 12:34:56.789')
    f.SetField(1, '2015/12/23 12:34:56.000')
    f.SetField(2, '2015/12/23 12:34:56')
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_ods_9.ods')
    lyr = ds.GetLayer(0)
    for i in range(3):
        assert lyr.GetLayerDefn().GetFieldDefn(i).GetType() == ogr.OFTDateTime
    f = lyr.GetNextFeature()
    if f.GetField(0) != '2015/12/23 12:34:56.789':
        f.DumpReadable()
        pytest.fail()
    if f.GetField(1) != '2015/12/23 12:34:56':
        f.DumpReadable()
        pytest.fail()
    if f.GetField(2) != '2015/12/23 12:34:56':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink('/vsimem/ogr_ods_9.ods')

###############################################################################
# Test Boolean


def test_ogr_ods_boolean():

    drv = ogr.GetDriverByName('ODS')
    out_filename = '/vsimem/ogr_ods_boolean.ods'
    ds = drv.CreateDataSource(out_filename)
    lyr = ds.CreateLayer('foo')
    fld_defn = ogr.FieldDefn('Field1', ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, True)
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, False)
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open(out_filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTBoolean
    f = lyr.GetNextFeature()
    assert f.GetField(0)
    f = lyr.GetNextFeature()
    assert not f.GetField(0)
    ds = None

    gdal.Unlink(out_filename)

###############################################################################
# Test number-columns-repeated at end of row.

def test_ogr_ods_number_columns_repeated_at_end_of_row():

    ds = ogr.Open('data/ods/testrepeatedcolatendofrow.ods')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    assert f['vbz'] == 1002
    assert f['b'] == 0

###############################################################################
# Test multiple <text:p> elements in the same cell

def test_ogr_ods_multiple_text_p_elements():

    ds = ogr.Open('data/ods/multiple_text_p_elements.ods')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    assert f['value'] == 'First line\nSecond line'
