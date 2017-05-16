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

def matches_non_existing_error_msg(msg):
    m1 = "does not exist in the file system, and is not recognized as a supported dataset name." in msg
    m2 = 'No such file or directory' in msg
    m3 = 'Permission denied' in msg
    return m1 or m2 or m3

def basic_test_1():
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open('non_existing_ds', gdal.GA_ReadOnly)
    gdal.PopErrorHandler()
    if ds is None and matches_non_existing_error_msg(gdal.GetLastErrorMsg()):
        return 'success'
    else:
        gdaltest.post_reason('did not get expected error message, got %s' % gdal.GetLastErrorMsg())
        return 'fail'

def basic_test_2():
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open('non_existing_ds', gdal.GA_Update)
    gdal.PopErrorHandler()
    if ds is None and matches_non_existing_error_msg(gdal.GetLastErrorMsg()):
        return 'success'
    else:
        gdaltest.post_reason('did not get expected error message, got %s' % gdal.GetLastErrorMsg())
        return 'fail'

def basic_test_3():
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open('', gdal.GA_ReadOnly)
    gdal.PopErrorHandler()
    if ds is None and matches_non_existing_error_msg(gdal.GetLastErrorMsg()):
        return 'success'
    else:
        gdaltest.post_reason('did not get expected error message, got %s' % gdal.GetLastErrorMsg())
        return 'fail'

def basic_test_4():
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open('', gdal.GA_Update)
    gdal.PopErrorHandler()
    if ds is None and matches_non_existing_error_msg(gdal.GetLastErrorMsg()):
        return 'success'
    else:
        gdaltest.post_reason('did not get expected error message, got %s' % gdal.GetLastErrorMsg())
        return 'fail'

def basic_test_5():
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds = gdal.Open('data/doctype.xml', gdal.GA_ReadOnly)
    gdal.PopErrorHandler()
    last_error = gdal.GetLastErrorMsg()
    expected = '`data/doctype.xml\' not recognized as a supported file format'
    if ds is None and expected in last_error:
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
        gdal.Open('non_existing_ds', gdal.GA_ReadOnly)
        gdaltest.post_reason('opening should have thrown an exception')
        return 'fail'
    except:
        # Special case: we should still be able to get the error message
        # until we call a new GDAL function
        if not matches_non_existing_error_msg(gdal.GetLastErrorMsg()):
            gdaltest.post_reason('did not get expected error message, got %s' % gdal.GetLastErrorMsg())
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
        self.eErrClass = None
        self.err_no = None
        self.msg = None

    def handler(self, eErrClass, err_no, msg):
        self.eErrClass = eErrClass
        self.err_no = err_no
        self.msg = msg

def basic_test_10():

    # Check that reference counting works OK
    gdal.PushErrorHandler(my_python_error_handler_class().handler)
    gdal.Error(1,2,'test')
    gdal.PopErrorHandler()

    error_handler = my_python_error_handler_class()
    gdal.PushErrorHandler(error_handler.handler)
    gdal.Error(1,2,'test')
    gdal.PopErrorHandler()

    if error_handler.eErrClass != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    if error_handler.err_no != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    if error_handler.msg != 'test':
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

    with gdaltest.error_handler():
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

    old_use_exceptions_status = gdal.GetUseExceptions()
    gdal.UseExceptions()
    got_exception = False
    try:
        ds = gdal.OpenEx('non existing')
    except:
        got_exception = True
    if old_use_exceptions_status == 0:
        gdal.DontUseExceptions()
    if not got_exception:
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

###############################################################################
# Test correct sorting of StringList / metadata (#5540, #5557)

def basic_test_13():

    ds = gdal.GetDriverByName('MEM').Create('',1,1)
    for i in range(3):
        if i == 0:
            ds.SetMetadataItem("ScaleBounds","True")
            ds.SetMetadataItem("ScaleBounds.MinScale","0")
            ds.SetMetadataItem("ScaleBounds.MaxScale","2000000")
        elif i == 1:
            ds.SetMetadataItem("ScaleBounds.MaxScale","2000000")
            ds.SetMetadataItem("ScaleBounds.MinScale","0")
            ds.SetMetadataItem("ScaleBounds","True")
        else:
            ds.SetMetadataItem("ScaleBounds.MinScale","0")
            ds.SetMetadataItem("ScaleBounds","True")
            ds.SetMetadataItem("ScaleBounds.MaxScale","2000000")

        if ds.GetMetadataItem('scalebounds') != 'True':
            gdaltest.post_reason('failure')
            return 'fail'
        if ds.GetMetadataItem('ScaleBounds') != 'True':
            gdaltest.post_reason('failure')
            return 'fail'
        if ds.GetMetadataItem('SCALEBOUNDS') != 'True':
            gdaltest.post_reason('failure')
            return 'fail'
        if ds.GetMetadataItem('ScaleBounds.MinScale') != '0':
            gdaltest.post_reason('failure')
            return 'fail'
        if ds.GetMetadataItem('ScaleBounds.MaxScale') != '2000000':
            gdaltest.post_reason('failure')
            return 'fail'
    ds = None

    ds = gdal.GetDriverByName('MEM').Create('',1,1)
    for i in range(200):
        ds.SetMetadataItem("FILENAME_%d" % i, "%d" % i)
    for i in range(200):
        if ds.GetMetadataItem("FILENAME_%d" % i) != '%d' % i:
            gdaltest.post_reason('failure')
            return 'fail'

    return 'success'

###############################################################################
# Test SetMetadata()

def basic_test_14():

    ds = gdal.GetDriverByName('MEM').Create('',1,1)

    ds.SetMetadata('foo')
    if ds.GetMetadata_List() != ['foo']:
        gdaltest.post_reason('failure')
        return 'fail'

    try:
        ds.SetMetadata(5)
        gdaltest.post_reason('failure')
        return 'fail'
    except:
        pass

    ds.SetMetadata(['foo=bar'])
    if ds.GetMetadata_List() != ['foo=bar']:
        gdaltest.post_reason('failure')
        return 'fail'

    try:
        ds.SetMetadata([5])
        gdaltest.post_reason('failure')
        return 'fail'
    except:
        pass

    ds.SetMetadata({'foo' : 'baz' })
    if ds.GetMetadata_List() != ['foo=baz']:
        gdaltest.post_reason('failure')
        return 'fail'

    try:
        ds.SetMetadata({'foo' : 5 })
        gdaltest.post_reason('failure')
        return 'fail'
    except:
        pass

    try:
        ds.SetMetadata({ 5 : 'baz' })
        gdaltest.post_reason('failure')
        return 'fail'
    except:
        pass

    try:
        ds.SetMetadata({ 5 : 6 })
        gdaltest.post_reason('failure')
        return 'fail'
    except:
        pass

    if sys.version_info >= (3,0,0):
        val = '\u00e9ven'
    else:
        exec("val = u'\\u00e9ven'")

    ds.SetMetadata({'bar' : val })
    if ds.GetMetadata()['bar'] != val:
        gdaltest.post_reason('failure')
        return 'fail'

    ds.SetMetadata({val : 'baz' })
    if ds.GetMetadata()[val] != 'baz':
        gdaltest.post_reason('failure')
        return 'fail'

    try:
        ds.SetMetadata({val : 5 })
        gdaltest.post_reason('failure')
        return 'fail'
    except:
        pass

    try:
        ds.SetMetadata({ 5 : val })
        gdaltest.post_reason('failure')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Test errors with progress callback

def basic_test_15_cbk_no_argument():
    return None

def basic_test_15_cbk_no_ret(a, b, c):
    return None

def basic_test_15_cbk_bad_ret(a, b, c):
    return 'ok'

def basic_test_15():

    try:
        with gdaltest.error_handler():
            gdal.GetDriverByName('MEM').CreateCopy('', gdal.GetDriverByName('MEM').Create('',1,1), callback = 'foo')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('MEM').CreateCopy('', gdal.GetDriverByName('MEM').Create('',1,1), callback = basic_test_15_cbk_no_argument)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('MEM').CreateCopy('', gdal.GetDriverByName('MEM').Create('',1,1), callback = basic_test_15_cbk_no_ret)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('MEM').CreateCopy('', gdal.GetDriverByName('MEM').Create('',1,1), callback = basic_test_15_cbk_bad_ret)
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test unrecognized and recognized open options prefixed by @

def basic_test_16():

    gdal.ErrorReset()
    gdal.OpenEx('data/byte.tif', open_options=['@UNRECOGNIZED=FOO'])
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdal.OpenEx('data/byte.tif', gdal.OF_UPDATE, open_options=['@NUM_THREADS=INVALID'])
    if gdal.GetLastErrorMsg() != 'Invalid value for NUM_THREADS: INVALID':
        gdaltest.post_reason('fail')
        print(gdal.GetLastErrorMsg())
        return 'fail'

    return 'success'

###############################################################################
# Test mix of gdal/ogr.UseExceptions()/DontUseExceptions()

def basic_test_17():

    from osgeo import ogr

    for i in range(2):
        ogr.UseExceptions()
        gdal.UseExceptions()
        try:
            gdal.Open('do_not_exist')
        except:
            pass
        gdal.DontUseExceptions()
        ogr.DontUseExceptions()
        if gdal.GetUseExceptions():
            gdaltest.post_reason('fail')
            return 'fail'
        if ogr.GetUseExceptions():
            gdaltest.post_reason('fail')
            return 'fail'

    for i in range(2):
        ogr.UseExceptions()
        gdal.UseExceptions()
        try:
            gdal.Open('do_not_exist')
        except:
            pass
        flag = False
        try:
            ogr.DontUseExceptions()
            gdal.DontUseExceptions()
            flag = True
        except:
            gdal.DontUseExceptions()
            ogr.DontUseExceptions()
        if flag:
            gdaltest.post_reason('expected failure')
            return 'fail'
        if gdal.GetUseExceptions():
            gdaltest.post_reason('fail')
            return 'fail'
        if ogr.GetUseExceptions():
            gdaltest.post_reason('fail')
            return 'fail'

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
                  basic_test_12,
                  basic_test_13,
                  basic_test_14,
                  basic_test_15,
                  basic_test_16,
                  basic_test_17 ]


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
