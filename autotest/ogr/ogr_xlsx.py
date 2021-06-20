#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR XLSX driver.
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

pytestmark = pytest.mark.require_driver('XLSX')

###############################################################################
# Check


def ogr_xlsx_check(ds):

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


def test_ogr_xlsx_1():

    assert ogr.GetDriverByName('XLSX').TestCapability("foo") == 0

    ds = ogr.Open('data/xlsx/test.xlsx')
    assert ds is not None, 'cannot open dataset'

    return ogr_xlsx_check(ds)

###############################################################################
# Test OGR_XLSX_HEADERS = DISABLE


def test_ogr_xlsx_2():

    gdal.SetConfigOption('OGR_XLSX_HEADERS', 'DISABLE')
    ds = ogr.Open('data/xlsx/test.xlsx')

    lyr = ds.GetLayerByName('Feuille7')

    assert lyr.GetFeatureCount() == 3

    gdal.SetConfigOption('OGR_XLSX_HEADERS', None)

###############################################################################
# Test OGR_XLSX_FIELD_TYPES = STRING


def test_ogr_xlsx_3():

    gdal.SetConfigOption('OGR_XLSX_FIELD_TYPES', 'STRING')
    ds = ogr.Open('data/xlsx/test.xlsx')

    lyr = ds.GetLayerByName('Feuille7')

    assert lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString

    gdal.SetConfigOption('OGR_XLSX_FIELD_TYPES', None)

###############################################################################
# Run test_ogrsf


def test_ogr_xlsx_4():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/xlsx/test.xlsx')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test write support


def test_ogr_xlsx_5():

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f XLSX tmp/test.xlsx data/xlsx/test.xlsx')

    ds = ogr.Open('tmp/test.xlsx')
    ret = ogr_xlsx_check(ds)
    ds = None

    os.unlink('tmp/test.xlsx')

    return ret

###############################################################################
# Test reading a file using inlineStr representation.


def test_ogr_xlsx_6():

    # In this dataset the column titles are not recognised by default.
    gdal.SetConfigOption('OGR_XLSX_HEADERS', 'FORCE')
    ds = ogr.Open('data/xlsx/inlineStr.xlsx')

    lyr = ds.GetLayerByName('inlineStr')

    assert lyr.GetFeatureCount() == 1

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.Bl_District_t == 'text6', 'Did not get expected value(1)'

    assert float(feat.GetField('Lat')) == pytest.approx(23.6247122, abs=0.00001), \
        'Did not get expected value(2)'

    gdal.SetConfigOption('OGR_XLSX_HEADERS', None)

###############################################################################
# Test update support


def test_ogr_xlsx_7():

    gdal.Unlink('tmp/ogr_xlsx_7.xlsx')
    shutil.copy('data/xlsx/test.xlsx', 'tmp/ogr_xlsx_7.xlsx')

    ds = ogr.Open('tmp/ogr_xlsx_7.xlsx', update=1)
    lyr = ds.GetLayerByName('Feuille7')
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 2:
        feat.DumpReadable()
        pytest.fail('did not get expected FID')
    feat.SetField(0, 'modified_value')
    lyr.SetFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open('tmp/ogr_xlsx_7.xlsx')
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

    os.unlink('tmp/ogr_xlsx_7.xlsx')

###############################################################################
# Test number of columns > 26 (#5774)


def test_ogr_xlsx_8():

    ds = ogr.GetDriverByName('XLSX').CreateDataSource('/vsimem/ogr_xlsx_8.xlsx')
    lyr = ds.CreateLayer('foo')
    for i in range(30):
        lyr.CreateField(ogr.FieldDefn('Field%d' % (i + 1)))
    f = ogr.Feature(lyr.GetLayerDefn())
    for i in range(30):
        f.SetField(i, 'val%d' % (i + 1))
    lyr.CreateFeature(f)
    f = None
    ds = None

    f = gdal.VSIFOpenL('/vsizip//vsimem/ogr_xlsx_8.xlsx/xl/worksheets/sheet1.xml', 'rb')
    content = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    assert str(content).find('<c r="AA1" t="s">') >= 0

    gdal.Unlink('/vsimem/ogr_xlsx_8.xlsx')

###############################################################################
# Test Integer64


def test_ogr_xlsx_9():

    ds = ogr.GetDriverByName('XLSX').CreateDataSource('/vsimem/ogr_xlsx_9.xlsx')
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

    ds = ogr.Open('/vsimem/ogr_xlsx_9.xlsx')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger64
    f = lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    assert f.GetField(0) == 12345678901234
    ds = None

    gdal.Unlink('/vsimem/ogr_xlsx_9.xlsx')

###############################################################################
# Test DateTime with milliseconds


def test_ogr_xlsx_10():

    ds = ogr.GetDriverByName('XLSX').CreateDataSource('/vsimem/ogr_xlsx_10.xlsx')
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

    ds = ogr.Open('/vsimem/ogr_xlsx_10.xlsx')
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

    gdal.Unlink('/vsimem/ogr_xlsx_10.xlsx')

###############################################################################
# Test reading sheet with more than 26 columns with holes (#6363)"


def test_ogr_xlsx_11():

    ds = ogr.Open('data/xlsx/not_all_columns_present.xlsx')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    for i in (0, 27, 28, 29):
        if f['Field%d' % (i + 1)] != 'val%d' % (i + 1):
            f.DumpReadable()
            pytest.fail()
    ds = None

###############################################################################
# Test reading a sheet whose file is stored as "absolute" in
# workbook.xml.rels (#6733)


def test_ogr_xlsx_12():

    ds = ogr.Open('data/xlsx/absolute_sheet_filename.xlsx')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is not None
    ds = None

###############################################################################
# Test that data types are correctly picked up even if first row is missing data


def test_ogr_xlsx_13():

    gdal.SetConfigOption('OGR_XLSX_FIELD_TYPES', None)
    ds = ogr.Open('data/xlsx/test_missing_row1_data.xlsx')

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Sheet1', 'bad layer name'

    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == 'Asset Reference', \
        'invalid field name'

    assert lyr.GetLayerDefn().GetFieldCount() == 18, \
        'invalid field count ({})'.format(lyr.GetLayerDefn().GetFieldCount())

    type_array = [ogr.OFTInteger,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTInteger,
                  ogr.OFTString,
                  ogr.OFTDate,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTDate,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString]

    for i, typ in enumerate(type_array):
        assert lyr.GetLayerDefn().GetFieldDefn(i).GetType() == typ, \
            'invalid type for field {}'.format(i + 1)


###############################################################################
# Test that field names are picked up even if last field has no data


def test_ogr_xlsx_14():

    gdal.SetConfigOption('OGR_XLSX_FIELD_TYPES', None)
    ds = ogr.Open('data/xlsx/test_empty_last_field.xlsx')

    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'Sheet1', 'bad layer name'

    assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == 'Asset Reference', \
        'invalid field name'

    assert lyr.GetLayerDefn().GetFieldCount() == 18, \
        'invalid field count ({})'.format(lyr.GetLayerDefn().GetFieldCount())

    type_array = [ogr.OFTInteger,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTInteger,
                  ogr.OFTString,
                  ogr.OFTDate,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTDate,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString,
                  ogr.OFTString]

    for i, typ in enumerate(type_array):
        assert lyr.GetLayerDefn().GetFieldDefn(i).GetType() == typ, \
            'invalid type for field {}'.format(i + 1)


###############################################################################
# Test appending a layer to an existing document


def test_ogr_xlsx_15():

    out_filename = '/vsimem/ogr_xlsx_15.xlsx'
    gdal.VectorTranslate(out_filename, 'data/poly.shp', options='-f XLSX -nln first')
    gdal.VectorTranslate(out_filename, 'data/poly.shp', options='-update -nln second')

    ds = ogr.Open(out_filename)
    assert ds.GetLayerByName('first').GetFeatureCount() != 0
    assert ds.GetLayerByName('second').GetFeatureCount() != 0
    ds = None

    gdal.Unlink(out_filename)

###############################################################################
# Test Boolean


def test_ogr_xlsx_boolean():

    out_filename = '/vsimem/ogr_xlsx_boolean.xlsx'
    ds = ogr.GetDriverByName('XLSX').CreateDataSource(out_filename)
    lyr = ds.CreateLayer('foo')
    fld_defn = ogr.FieldDefn('Field1', ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 1)
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open(out_filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTBoolean
    f = lyr.GetNextFeature()
    assert f.GetField(0) == 1
    ds = None

    gdal.Unlink(out_filename)


###############################################################################
# Test reading DateTime, and numeric precision issues (#2683)


def test_ogr_xlsx_read_datetime():

    ds = ogr.Open('data/xlsx/datetime.xlsx')
    lyr = ds.GetLayer(0)
    got = [ f.GetFieldAsString(0) for f in lyr ]
    assert got == ['2020/04/07 09:58:00',
                   '2020/04/07 09:58:01',
                   '2020/04/07 09:58:02',
                   '2020/04/07 09:58:03',
                   '2020/04/07 09:58:04',
                   '2020/04/07 09:58:05',
                   '2020/04/07 10:03:00',
                   '2020/04/07 10:10:00',
                   '2020/04/07 10:29:00',
                   '2020/04/07 10:42:00']


###############################################################################
# Test reading cells with inline formatting (#3729)


def test_ogr_xlsx_read_cells_with_inline_formatting():

    ds = ogr.Open('data/xlsx/cells_with_inline_formatting.xlsx')
    lyr = ds.GetLayer(0)
    got = [ (f[0], f[1], f[2]) for f in lyr ]
    assert got == [(1, 'text 2', 'text 3'), (2, 'text 4', 'text5')]
