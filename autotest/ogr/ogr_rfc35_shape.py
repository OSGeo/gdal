#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RFC35 for shape driver
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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


from osgeo import ogr
from osgeo import gdal
import pytest

###############################################################################
#

def CheckFileSize(src_filename):

    import test_py_scripts
    script_path = test_py_scripts.get_py_script('ogr2ogr')
    if script_path is None:
        pytest.skip()

    test_py_scripts.run_py_script(script_path, 'ogr2ogr', 'tmp/CheckFileSize.dbf ' + src_filename)
    statBufSrc = gdal.VSIStatL(src_filename, gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    statBufDst = gdal.VSIStatL('tmp/CheckFileSize.dbf', gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/CheckFileSize.dbf')

    assert statBufSrc.size == statBufDst.size, \
        ('src_size = %d, dst_size = %d', statBufSrc.size, statBufDst.size)

###############################################################################
# Initiate the test file


def test_ogr_rfc35_shape_1():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/rfc35_test.dbf')
    lyr = ds.CreateLayer('rfc35_test')

    lyr.ReorderFields([])

    fd = ogr.FieldDefn('foo5', ogr.OFTString)
    fd.SetWidth(5)
    lyr.CreateField(fd)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'foo0')
    lyr.CreateFeature(feat)
    feat = None

    fd = ogr.FieldDefn('bar10', ogr.OFTString)
    fd.SetWidth(10)
    lyr.CreateField(fd)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'foo1')
    feat.SetField(1, 'bar1')
    lyr.CreateFeature(feat)
    feat = None

    fd = ogr.FieldDefn('baz15', ogr.OFTString)
    fd.SetWidth(15)
    lyr.CreateField(fd)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'foo2')
    feat.SetField(1, 'bar2_01234')
    feat.SetField(2, 'baz2_0123456789')
    lyr.CreateFeature(feat)
    feat = None

    fd = ogr.FieldDefn('baw20', ogr.OFTString)
    fd.SetWidth(20)
    lyr.CreateField(fd)

###############################################################################
# Test ReorderField()


def Truncate(val, lyr_defn, fieldname):
    if val is None:
        return val

    return val[0:lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex(fieldname)).GetWidth()]


def CheckFeatures(lyr, field1='foo5', field2='bar10', field3='baz15', field4='baw20'):

    expected_values = [
        ['foo0', None, None, None],
        ['foo1', 'bar1', None, None],
        ['foo2', 'bar2_01234', 'baz2_0123456789', None],
        ['foo3', 'bar3_01234', 'baz3_0123456789', 'baw3_012345678901234']
    ]

    lyr_defn = lyr.GetLayerDefn()

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    i = 0
    while feat is not None:
        if (field1 is not None and feat.GetField(field1) != Truncate(expected_values[i][0], lyr_defn, field1)) or \
           (field2 is not None and feat.GetField(field2) != Truncate(expected_values[i][1], lyr_defn, field2)) or \
           (field3 is not None and feat.GetField(field3) != Truncate(expected_values[i][2], lyr_defn, field3)) or \
           (field4 is not None and feat.GetField(field4) != Truncate(expected_values[i][3], lyr_defn, field4)):
            feat.DumpReadable()
            pytest.fail()
        feat = lyr.GetNextFeature()
        i = i + 1

    

def CheckColumnOrder(lyr, expected_order):

    lyr_defn = lyr.GetLayerDefn()
    for i, exp_order in enumerate(expected_order):
        assert lyr_defn.GetFieldDefn(i).GetName() == exp_order

    

def Check(lyr, expected_order):

    CheckColumnOrder(lyr, expected_order)

    CheckFeatures(lyr)

    ds = ogr.Open('tmp/rfc35_test.dbf', update=1)
    lyr_reopen = ds.GetLayer(0)

    CheckColumnOrder(lyr_reopen, expected_order)

    CheckFeatures(lyr_reopen)


def test_ogr_rfc35_shape_2():

    ds = ogr.Open('tmp/rfc35_test.dbf', update=1)
    lyr = ds.GetLayer(0)

    assert lyr.TestCapability(ogr.OLCReorderFields) == 1

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'foo3')
    feat.SetField(1, 'bar3_01234')
    feat.SetField(2, 'baz3_0123456789')
    feat.SetField(3, 'baw3_012345678901234')
    lyr.CreateFeature(feat)
    feat = None

    assert lyr.ReorderField(1, 3) == 0
    Check(lyr, ['foo5', 'baz15', 'baw20', 'bar10'])

    lyr.ReorderField(3, 1)
    Check(lyr, ['foo5', 'bar10', 'baz15', 'baw20'])

    lyr.ReorderField(0, 2)
    Check(lyr, ['bar10', 'baz15', 'foo5', 'baw20'])

    lyr.ReorderField(2, 0)
    Check(lyr, ['foo5', 'bar10', 'baz15', 'baw20'])

    lyr.ReorderField(0, 1)
    Check(lyr, ['bar10', 'foo5', 'baz15', 'baw20'])

    lyr.ReorderField(1, 0)
    Check(lyr, ['foo5', 'bar10', 'baz15', 'baw20'])

    lyr.ReorderFields([3, 2, 1, 0])
    Check(lyr, ['baw20', 'baz15', 'bar10', 'foo5'])

    lyr.ReorderFields([3, 2, 1, 0])
    Check(lyr, ['foo5', 'bar10', 'baz15', 'baw20'])

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.ReorderFields([0, 0, 0, 0])
    gdal.PopErrorHandler()
    assert ret != 0

    ds = None

    ds = ogr.Open('tmp/rfc35_test.dbf', update=1)
    lyr = ds.GetLayer(0)

    CheckColumnOrder(lyr, ['foo5', 'bar10', 'baz15', 'baw20'])

    CheckFeatures(lyr)

###############################################################################
# Test AlterFieldDefn() for change of name and width


def test_ogr_rfc35_shape_3():

    ds = ogr.Open('tmp/rfc35_test.dbf', update=1)
    lyr = ds.GetLayer(0)

    fd = ogr.FieldDefn("baz25", ogr.OFTString)
    fd.SetWidth(25)

    lyr_defn = lyr.GetLayerDefn()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.AlterFieldDefn(-1, fd, ogr.ALTER_ALL_FLAG)
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.AlterFieldDefn(lyr_defn.GetFieldCount(), fd, ogr.ALTER_ALL_FLAG)
    gdal.PopErrorHandler()
    assert ret != 0

    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("baz15"), fd, ogr.ALTER_ALL_FLAG)

    CheckFeatures(lyr, field3='baz25')

    fd = ogr.FieldDefn("baz5", ogr.OFTString)
    fd.SetWidth(5)

    lyr_defn = lyr.GetLayerDefn()
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("baz25"), fd, ogr.ALTER_ALL_FLAG)

    CheckFeatures(lyr, field3='baz5')

    ds = None

    ds = ogr.Open('tmp/rfc35_test.dbf', update=1)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('baz5'))
    assert fld_defn.GetWidth() == 5

    CheckFeatures(lyr, field3='baz5')

###############################################################################
# Test AlterFieldDefn() for change of type


def test_ogr_rfc35_shape_4():

    ds = ogr.Open('tmp/rfc35_test.dbf', update=1)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    assert lyr.TestCapability(ogr.OLCAlterFieldDefn) == 1

    fd = ogr.FieldDefn("intfield", ogr.OFTInteger)
    lyr.CreateField(fd)

    lyr.ReorderField(lyr_defn.GetFieldIndex("intfield"), 0)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    feat.SetField("intfield", 12345)
    lyr.SetFeature(feat)
    feat = None

    fd.SetWidth(10)
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("intfield") == 12345
    feat = None

    CheckFeatures(lyr, field3='baz5')

    fd.SetWidth(5)
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("intfield") == 12345
    feat = None

    CheckFeatures(lyr, field3='baz5')

    ds = None

    ds = ogr.Open('tmp/rfc35_test.dbf', update=1)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    fd.SetWidth(4)
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("intfield") == 1234
    feat = None

    CheckFeatures(lyr, field3='baz5')

    ds = None

    # Check that the file size has decreased after column shrinking
    CheckFileSize('tmp/rfc35_test.dbf')

    ds = ogr.Open('tmp/rfc35_test.dbf', update=1)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    fd = ogr.FieldDefn("oldintfld", ogr.OFTString)
    fd.SetWidth(15)
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("oldintfld") == '1234'
    feat = None

    CheckFeatures(lyr, field3='baz5')

    ds = None

    ds = ogr.Open('tmp/rfc35_test.dbf', update=1)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("oldintfld") == '1234'
    feat = None

    CheckFeatures(lyr, field3='baz5')

    lyr.DeleteField(lyr_defn.GetFieldIndex("oldintfld"))

    fd = ogr.FieldDefn("intfield", ogr.OFTInteger)
    fd.SetWidth(10)
    assert lyr.CreateField(fd) == 0

    assert lyr.ReorderField(lyr_defn.GetFieldIndex("intfield"), 0) == 0

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    feat.SetField("intfield", 98765)
    assert lyr.SetFeature(feat) == 0
    feat = None

    fd = ogr.FieldDefn("oldintfld", ogr.OFTString)
    fd.SetWidth(6)
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("oldintfld") == '98765'
    feat = None

    CheckFeatures(lyr, field3='baz5')

    ds = None

    ds = ogr.Open('tmp/rfc35_test.dbf', update=1)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField("oldintfld") == '98765'
    feat = None

    CheckFeatures(lyr, field3='baz5')

###############################################################################
# Test DeleteField()


def test_ogr_rfc35_shape_5():

    ds = ogr.Open('tmp/rfc35_test.dbf', update=1)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    assert lyr.TestCapability(ogr.OLCDeleteField) == 1

    assert lyr.DeleteField(0) == 0

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.DeleteField(-1)
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.DeleteField(lyr.GetLayerDefn().GetFieldCount())
    gdal.PopErrorHandler()
    assert ret != 0

    CheckFeatures(lyr, field3='baz5')

    assert lyr.DeleteField(lyr_defn.GetFieldIndex('baw20')) == 0

    ds = None

    # Check that the file size has decreased after column removing
    CheckFileSize('tmp/rfc35_test.dbf')
    if ret == 'fail':
        return ret

    ds = ogr.Open('tmp/rfc35_test.dbf', update=1)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    CheckFeatures(lyr, field3='baz5', field4=None)

    assert lyr.DeleteField(lyr_defn.GetFieldIndex('baz5')) == 0

    CheckFeatures(lyr, field3=None, field4=None)

    assert lyr.DeleteField(lyr_defn.GetFieldIndex('foo5')) == 0

    assert lyr.DeleteField(lyr_defn.GetFieldIndex('bar10')) == 0

    CheckFeatures(lyr, field1=None, field2=None, field3=None, field4=None)

    ds = None

    ds = ogr.Open('tmp/rfc35_test.dbf', update=1)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    CheckFeatures(lyr, field1=None, field2=None, field3=None, field4=None)

###############################################################################
# Initiate the test file


def test_ogr_rfc35_shape_cleanup():

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/rfc35_test.dbf')




