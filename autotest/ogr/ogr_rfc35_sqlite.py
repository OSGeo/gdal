#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RFC35 for SQLite driver
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import gdal

###############################################################################
# Initiate the test file

def ogr_rfc35_sqlite_1():

    gdaltest.rfc35_sqlite_ds = None
    gdaltest.rfc35_sqlite_ds_name = None
    try:
        sqlite_dr = ogr.GetDriverByName( 'SQLite' )
        if sqlite_dr is None:
            return 'skip'
    except:
        return 'skip'

    try:
        os.unlink('tmp/rfc35_test.sqlite')
    except:
        pass

    # This is to speed-up the runtime of tests on EXT4 filesystems
    # Do not use this for production environment if you care about data safety
    # w.r.t system/OS crashes, unless you know what you are doing.
    gdal.SetConfigOption('OGR_SQLITE_SYNCHRONOUS', 'OFF')

    gdaltest.rfc35_sqlite_ds_name = '/vsimem/rfc35_test.sqlite'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.rfc35_sqlite_ds = ogr.GetDriverByName('SQLite').CreateDataSource(gdaltest.rfc35_sqlite_ds_name)
    gdal.PopErrorHandler()
    if gdaltest.rfc35_sqlite_ds is None:
        gdaltest.rfc35_sqlite_ds_name = 'tmp/rfc35_test.sqlite'
        gdaltest.rfc35_sqlite_ds = ogr.GetDriverByName('SQLite').CreateDataSource(gdaltest.rfc35_sqlite_ds_name)
    lyr = gdaltest.rfc35_sqlite_ds.CreateLayer('rfc35_test')

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

    return 'success'

###############################################################################
# Test ReorderField()

def Truncate(val, lyr_defn, fieldname):
    #if val is None:
    #    return val

    #return val[0:lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex(fieldname)).GetWidth()]
    # Mem driver doesn't actually truncate
    return val

def CheckFeatures(lyr, foo = 'foo5', bar = 'bar10', baz = 'baz15', baw = 'baw20'):

    expected_values = [
        [ 'foo0', None, None, None ],
        [ 'foo1', 'bar1', None, None ],
        [ 'foo2', 'bar2_01234', 'baz2_0123456789', None ],
        [ 'foo3', 'bar3_01234', 'baz3_0123456789', 'baw3_012345678901234' ]
    ]

    lyr_defn = lyr.GetLayerDefn()

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    i = 0
    while feat is not None:
        if (foo is not None and feat.GetField(foo) != Truncate(expected_values[i][0], lyr_defn, foo)) or \
           (bar is not None and feat.GetField(bar) != Truncate(expected_values[i][1], lyr_defn, bar)) or \
           (baz is not None and feat.GetField(baz) != Truncate(expected_values[i][2], lyr_defn, baz)) or \
           (baw is not None and feat.GetField(baw) != Truncate(expected_values[i][3], lyr_defn, baw)):
               feat.DumpReadable()
               return 'fail'
        feat = lyr.GetNextFeature()
        i = i + 1

    return 'success'

def CheckColumnOrder(lyr, expected_order):

    lyr_defn = lyr.GetLayerDefn()
    for i in range(len(expected_order)):
        if lyr_defn.GetFieldDefn(i).GetName() != expected_order[i]:
            return 'fail'

    return 'success'

def Check(lyr, expected_order):

    ret = CheckColumnOrder(lyr, expected_order)
    if ret != 'success':
        return ret

    ret = CheckFeatures(lyr)
    if ret != 'success':
        return ret

    return 'success'

def ogr_rfc35_sqlite_2():

    if gdaltest.rfc35_sqlite_ds is None:
        return 'skip'

    lyr = gdaltest.rfc35_sqlite_ds.GetLayer(0)

    if lyr.TestCapability(ogr.OLCReorderFields) != 1:
        gdaltest.post_reason('failed')
        return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'foo3')
    feat.SetField(1, 'bar3_01234')
    feat.SetField(2, 'baz3_0123456789')
    feat.SetField(3, 'baw3_012345678901234')
    lyr.CreateFeature(feat)
    feat = None

    if lyr.ReorderField(1,3) != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    ret = Check(lyr, ['foo5', 'baz15', 'baw20', 'bar10'])
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    lyr.ReorderField(3,1)
    ret = Check(lyr, ['foo5', 'bar10', 'baz15', 'baw20'])
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    lyr.ReorderField(0,2)
    ret = Check(lyr, ['bar10', 'baz15', 'foo5', 'baw20'])
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    lyr.ReorderField(2,0)
    ret = Check(lyr, ['foo5', 'bar10', 'baz15', 'baw20'])
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    lyr.ReorderField(0,1)
    ret = Check(lyr, ['bar10', 'foo5', 'baz15', 'baw20'])
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    lyr.ReorderField(1,0)
    ret = Check(lyr, ['foo5', 'bar10', 'baz15', 'baw20'])
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    lyr.ReorderFields([3,2,1,0])
    ret = Check(lyr, ['baw20', 'baz15', 'bar10', 'foo5'])
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    lyr.ReorderFields([3,2,1,0])
    ret = Check(lyr, ['foo5', 'bar10', 'baz15', 'baw20'])
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.ReorderFields([0,0,0,0])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('failed')
        return 'fail'

    return 'success'

###############################################################################
# Test AlterFieldDefn() for change of name and width

def ogr_rfc35_sqlite_3():

    if gdaltest.rfc35_sqlite_ds is None:
        return 'skip'

    lyr = gdaltest.rfc35_sqlite_ds.GetLayer(0)

    fd = ogr.FieldDefn("baz25", ogr.OFTString)
    fd.SetWidth(25)

    lyr_defn = lyr.GetLayerDefn()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.AlterFieldDefn(-1, fd, ogr.ALTER_ALL_FLAG)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('failed')
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.AlterFieldDefn(lyr_defn.GetFieldCount(), fd, ogr.ALTER_ALL_FLAG)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("baz15"), fd, ogr.ALTER_ALL_FLAG)

    expected_values = [
        [ 'foo0', None, None, None ],
        [ 'foo1', 'bar1', None, None ],
        [ 'foo2', 'bar2_01234', 'baz2_0123456789', None ],
        [ 'foo3', 'bar3_01234', 'baz3_0123456789', 'baw3_012345678901234' ]
    ]

    ret = CheckFeatures(lyr, baz = 'baz25')
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    fd = ogr.FieldDefn("baz5", ogr.OFTString)
    fd.SetWidth(5)

    lyr_defn = lyr.GetLayerDefn()
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("baz25"), fd, ogr.ALTER_ALL_FLAG)

    ret = CheckFeatures(lyr, baz = 'baz5')
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    lyr_defn = lyr.GetLayerDefn()
    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('baz5'))
    if fld_defn.GetWidth() != 5:
        gdaltest.post_reason('failed')
        return 'fail'

    ret = CheckFeatures(lyr, baz = 'baz5')
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    return 'success'

###############################################################################
# Test AlterFieldDefn() for change of type

def ogr_rfc35_sqlite_4():

    if gdaltest.rfc35_sqlite_ds is None:
        return 'skip'

    lyr = gdaltest.rfc35_sqlite_ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    if lyr.TestCapability(ogr.OLCAlterFieldDefn) != 1:
        gdaltest.post_reason('failed')
        return 'fail'

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
    if feat.GetField("intfield") != 12345:
        gdaltest.post_reason('failed')
        return 'fail'
    feat = None

    ret = CheckFeatures(lyr, baz = 'baz5')
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    fd.SetWidth(5)
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetField("intfield") != 12345:
        gdaltest.post_reason('failed')
        return 'fail'
    feat = None

    ret = CheckFeatures(lyr, baz = 'baz5')
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    fd.SetWidth(4)
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    #if feat.GetField("intfield") != 1234:
    if feat.GetField("intfield") != 12345:
        gdaltest.post_reason('failed')
        return 'fail'
    feat = None

    ret = CheckFeatures(lyr, baz = 'baz5')
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    fd = ogr.FieldDefn("oldintfld", ogr.OFTString)
    fd.SetWidth(15)
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    #if feat.GetField("oldintfld") != '1234':
    if feat.GetField("oldintfld") != '12345':
        gdaltest.post_reason('failed')
        return 'fail'
    feat = None

    ret = CheckFeatures(lyr, baz = 'baz5')
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    lyr.DeleteField(lyr_defn.GetFieldIndex("oldintfld"))

    fd = ogr.FieldDefn("intfield", ogr.OFTInteger)
    fd.SetWidth(10)
    if lyr.CreateField(fd) != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    if lyr.ReorderField(lyr_defn.GetFieldIndex("intfield"), 0) != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    feat.SetField("intfield", 98765)
    if lyr.SetFeature(feat) != 0:
        gdaltest.post_reason('failed')
        return 'fail'
    feat = None

    fd = ogr.FieldDefn("oldintfld", ogr.OFTString)
    fd.SetWidth(6)
    lyr.AlterFieldDefn(lyr_defn.GetFieldIndex("intfield"), fd, ogr.ALTER_ALL_FLAG)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetField("oldintfld") != '98765':
        gdaltest.post_reason('failed')
        return 'fail'
    feat = None

    ret = CheckFeatures(lyr, baz = 'baz5')
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    return 'success'

###############################################################################
# Test DeleteField()

def ogr_rfc35_sqlite_5():

    if gdaltest.rfc35_sqlite_ds is None:
        return 'skip'

    lyr = gdaltest.rfc35_sqlite_ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    if lyr.TestCapability(ogr.OLCDeleteField) != 1:
        gdaltest.post_reason('failed')
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.DeleteField(-1)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('failed')
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.DeleteField(lyr.GetLayerDefn().GetFieldCount())
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('failed')
        return 'fail'

    if lyr.DeleteField(0) != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    ret = CheckFeatures(lyr, baz = 'baz5')
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    if lyr.DeleteField(lyr_defn.GetFieldIndex('baw20')) != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    ret = CheckFeatures(lyr, baz = 'baz5', baw = None)
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    if lyr.DeleteField(lyr_defn.GetFieldIndex('baz5')) != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    ret = CheckFeatures(lyr, baz = None, baw = None)
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    if lyr.DeleteField(lyr_defn.GetFieldIndex('foo5')) != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    if lyr.DeleteField(lyr_defn.GetFieldIndex('bar10')) != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    ret = CheckFeatures(lyr, foo = None, bar = None, baz = None, baw = None)
    if ret != 'success':
        gdaltest.post_reason('failed')
        return ret

    return 'success'

###############################################################################
# Initiate the test file

def ogr_rfc35_sqlite_cleanup():

    if gdaltest.rfc35_sqlite_ds_name is None:
        return 'skip'

    gdaltest.rfc35_sqlite_ds = None
    ogr.GetDriverByName('SQLite').DeleteDataSource(gdaltest.rfc35_sqlite_ds_name)

    return 'success'

gdaltest_list = [
    ogr_rfc35_sqlite_1,
    ogr_rfc35_sqlite_2,
    ogr_rfc35_sqlite_3,
    ogr_rfc35_sqlite_4,
    ogr_rfc35_sqlite_5,
    ogr_rfc35_sqlite_cleanup ]


if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_rfc35_sqlite' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

