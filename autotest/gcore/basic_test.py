#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic GDAL open
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal

# Nothing exciting here. Just trying to open non existing files,
# or empty names, or files that are not valid datasets...

def basic_test_1():
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open('non_existing_ds', gdal.GA_ReadOnly)
    gdal.PopErrorHandler()
    if ds is None and gdal.GetLastErrorMsg() == '`non_existing_ds\' does not exist in the file system,\nand is not recognised as a supported dataset name.\n':
        return 'success'
    else:
        return 'fail'

def basic_test_2():
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open('non_existing_ds', gdal.GA_Update)
    gdal.PopErrorHandler()
    if ds is None and gdal.GetLastErrorMsg() == '`non_existing_ds\' does not exist in the file system,\nand is not recognised as a supported dataset name.\n':
        return 'success'
    else:
        return 'fail'

def basic_test_3():
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open('', gdal.GA_ReadOnly)
    gdal.PopErrorHandler()
    if ds is None and gdal.GetLastErrorMsg() == '`\' does not exist in the file system,\nand is not recognised as a supported dataset name.\n':
        return 'success'
    else:
        return 'fail'

def basic_test_4():
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open('', gdal.GA_Update)
    gdal.PopErrorHandler()
    if ds is None and gdal.GetLastErrorMsg() == '`\' does not exist in the file system,\nand is not recognised as a supported dataset name.\n':
        return 'success'
    else:
        return 'fail'

def basic_test_5():
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open('data/doctype.xml', gdal.GA_ReadOnly)
    gdal.PopErrorHandler()
    if ds is None and gdal.GetLastErrorMsg() == '`data/doctype.xml\' not recognised as a supported file format.\n':
        return 'success'
    else:
        return 'fail'

###############################################################################
# Issue several AllRegister() to check that GDAL drivers are good citizens

def basic_test_6():
    gdal.AllRegister()
    gdal.AllRegister()
    gdal.AllRegister()

    return 'success'

###############################################################################
# Test fix for #3077 (check that errors are cleared when using UseExceptions())

def basic_test_7_internal():
    try:
        ds = gdal.Open('non_existing_ds', gdal.GA_ReadOnly)
        gdaltest.post_reason('opening should have thrown an exception')
        return 'fail'
    except:
        # Special case: we should still be able to get the error message
        # until we call a new GDAL function
        if gdal.GetLastErrorMsg() != '`non_existing_ds\' does not exist in the file system,\nand is not recognised as a supported dataset name.\n':
            gdaltest.post_reason('did not get expected error message')
            return 'fail'

        if gdal.GetLastErrorType() == 0:
            gdaltest.post_reason('did not get expected error type')
            return 'fail'

        # Should issue an implicit CPLErrorReset()
        gdal.GetCacheMax()

        if gdal.GetLastErrorType() != 0:
            gdaltest.post_reason('got unexpected error type')
            return 'fail'

        return 'success'

def basic_test_7():
    old_use_exceptions_status = gdal.GetUseExceptions()
    gdal.UseExceptions()
    ret = basic_test_7_internal()
    if old_use_exceptions_status == 0:
        gdal.DontUseExceptions()
    return ret

###############################################################################
# Test gdal.VersionInfo('RELEASE_DATE') and gdal.VersionInfo('LICENSE')

def basic_test_8():

    ret = gdal.VersionInfo('RELEASE_DATE')
    if len(ret) != 8:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    python_exe = sys.executable
    if sys.platform == 'win32':
        python_exe = python_exe.replace('\\', '/')

    ret = gdaltest.runexternal(python_exe + ' basic_test.py LICENSE 0')
    if ret.find('GDAL/OGR is released under the MIT/X license') != 0 and ret.find('GDAL/OGR Licensing') < 0:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    f = open('tmp/LICENSE.TXT', 'wt')
    f.write('fake_license')
    f.close()
    ret = gdaltest.runexternal(python_exe + ' basic_test.py LICENSE 1')
    os.unlink('tmp/LICENSE.TXT')
    if ret.find('fake_license') != 0 and ret.find('GDAL/OGR Licensing') < 0:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test gdal.PushErrorHandler() with a Python error handler

def my_python_error_handler(eErrClass, err_no, msg):
    gdaltest.eErrClass = eErrClass
    gdaltest.err_no = err_no
    gdaltest.msg = msg

def basic_test_9():

    gdaltest.eErrClass = 0
    gdaltest.err_no = 0
    gdaltest.msg = ''
    gdal.PushErrorHandler(my_python_error_handler)
    gdal.Error(1,2,'test')
    gdal.PopErrorHandler()

    if gdaltest.eErrClass != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    if gdaltest.err_no != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    if gdaltest.msg != 'test':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test gdal.PushErrorHandler() with a Python error handler as a method (#5186)

class my_python_error_handler_class:
    def __init__(self):
        pass

    def handler(self, eErrClass, err_no, msg):
        gdaltest.eErrClass = eErrClass
        gdaltest.err_no = err_no
        gdaltest.msg = msg

def basic_test_10():

    gdaltest.eErrClass = 0
    gdaltest.err_no = 0
    gdaltest.msg = ''
    # Check that reference counting works OK
    gdal.PushErrorHandler(my_python_error_handler_class().handler)
    gdal.Error(1,2,'test')
    gdal.PopErrorHandler()

    if gdaltest.eErrClass != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    if gdaltest.err_no != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    if gdaltest.msg != 'test':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test gdal.OpenEx()

def basic_test_11():

    ds = gdal.OpenEx('data/byte.tif')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('data/byte.tif', gdal.OF_RASTER)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('data/byte.tif', gdal.OF_VECTOR)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('data/byte.tif', gdal.OF_RASTER | gdal.OF_VECTOR)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('data/byte.tif', gdal.OF_ALL)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('data/byte.tif', gdal.OF_UPDATE)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('data/byte.tif', gdal.OF_RASTER | gdal.OF_VECTOR | gdal.OF_UPDATE | gdal.OF_VERBOSE_ERROR)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('data/byte.tif', allowed_drivers = [] )
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('data/byte.tif', allowed_drivers = ['GTiff'] )
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('data/byte.tif', allowed_drivers = ['PNG'] )
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('data/byte.tif', open_options = ['FOO'] )
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ar_ds = [ gdal.OpenEx('data/byte.tif', gdal.OF_SHARED) for i in range(1024) ]
    if ar_ds[1023] is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ar_ds = None

    ds = gdal.OpenEx('../ogr/data/poly.shp', gdal.OF_RASTER)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('../ogr/data/poly.shp', gdal.OF_VECTOR)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayer(0) is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.GetLayer(0).GetMetadata()

    ds = gdal.OpenEx('../ogr/data/poly.shp', allowed_drivers = ['ESRI Shapefile'] )
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('../ogr/data/poly.shp', gdal.OF_RASTER | gdal.OF_VECTOR)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.OpenEx('non existing')
    if ds is not None or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.OpenEx('non existing', gdal.OF_VERBOSE_ERROR)
    gdal.PopErrorHandler()
    if ds is not None or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test GDAL layer API

def basic_test_12():

    ds = gdal.GetDriverByName('MEMORY').Create('bar', 0, 0, 0)
    if ds.GetDescription() != 'bar':
        gdaltest.post_reason('failure')
        print(ds.GetDescription())
        return 'fail'
    lyr = ds.CreateLayer("foo")
    if lyr is None:
        gdaltest.post_reason('failure')
        return 'fail'
    if lyr.GetDescription() != 'foo':
        gdaltest.post_reason('failure')
        print(lyr.GetDescription())
        return 'fail'
    from osgeo import ogr
    if lyr.TestCapability(ogr.OLCCreateField) != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    lyr = ds.GetLayerByName("foo")
    if lyr is None:
        gdaltest.post_reason('failure')
        return 'fail'
    lyr = ds.GetLayerByIndex(0)
    if lyr is None:
        gdaltest.post_reason('failure')
        return 'fail'
    lyr = ds.GetLayer(0)
    if lyr is None:
        gdaltest.post_reason('failure')
        return 'fail'
    sql_lyr = ds.ExecuteSQL('SELECT * FROM foo')
    if sql_lyr is None:
        gdaltest.post_reason('failure')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    new_lyr = ds.CopyLayer(lyr, 'bar')
    if new_lyr is None:
        gdaltest.post_reason('failure')
        return 'fail'
    if ds.DeleteLayer(0) != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    if ds.DeleteLayer('bar') != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    ds.SetStyleTable(ds.GetStyleTable())
    ds = None

    return 'success'
    
gdaltest_list = [ basic_test_1,
                  basic_test_2,
                  basic_test_3,
                  basic_test_4,
                  basic_test_5,
                  basic_test_6,
                  basic_test_7,
                  basic_test_8,
                  basic_test_9,
                  basic_test_10,
                  basic_test_11,
                  basic_test_12 ]


if __name__ == '__main__':
    
    if len(sys.argv) == 3 and sys.argv[1] == "LICENSE":
        if sys.argv[2] == '0':
            gdal.SetConfigOption('GDAL_DATA', '/foo')
        else:
            gdal.SetConfigOption('GDAL_DATA', 'tmp')
        gdal.VersionInfo('LICENSE')
        print(gdal.VersionInfo('LICENSE'))
        import testnonboundtoswig
        testnonboundtoswig.GDALDestroyDriverManager()
        sys.exit(0)

    gdaltest.setup_run( 'basic_test' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

