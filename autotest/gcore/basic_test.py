#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic GDAL open
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
import subprocess
import sys


import gdaltest
from osgeo import gdal
import pytest

# Nothing exciting here. Just trying to open non existing files,
# or empty names, or files that are not valid datasets...


def matches_non_existing_error_msg(msg):
    m1 = "does not exist in the file system, and is not recognized as a supported dataset name." in msg
    m2 = 'No such file or directory' in msg
    m3 = 'Permission denied' in msg
    return m1 or m2 or m3


def test_basic_test_1():
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('non_existing_ds', gdal.GA_ReadOnly)
    gdal.PopErrorHandler()
    if ds is None and matches_non_existing_error_msg(gdal.GetLastErrorMsg()):
        return
    pytest.fail('did not get expected error message, got %s' % gdal.GetLastErrorMsg())


@pytest.mark.skipif(sys.platform != 'linux', reason='Incorrect platform')
def test_basic_test_strace_non_existing_file():

    python_exe = sys.executable
    cmd = "strace -f %s -c \"from osgeo import gdal; " % python_exe + (
        "gdal.OpenEx('non_existing_ds', gdal.OF_RASTER)"
        " \" ")
    try:
        (_, err) = gdaltest.runexternal_out_and_err(cmd)
    except Exception:
        # strace not available
        pytest.skip()

    interesting_lines = []
    for line in err.split('\n'):
        if 'non_existing_ds' in line:
            interesting_lines += [ line ]
    # Only 3 calls on the file are legit: open(), stat() and readlink()
    assert len(interesting_lines) <= 3, 'too many system calls accessing file'


def test_basic_test_2():
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('non_existing_ds', gdal.GA_Update)
    gdal.PopErrorHandler()
    if ds is None and matches_non_existing_error_msg(gdal.GetLastErrorMsg()):
        return
    pytest.fail('did not get expected error message, got %s' % gdal.GetLastErrorMsg())


def test_basic_test_3():
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('', gdal.GA_ReadOnly)
    gdal.PopErrorHandler()
    if ds is None and matches_non_existing_error_msg(gdal.GetLastErrorMsg()):
        return
    pytest.fail('did not get expected error message, got %s' % gdal.GetLastErrorMsg())


def test_basic_test_4():
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('', gdal.GA_Update)
    gdal.PopErrorHandler()
    if ds is None and matches_non_existing_error_msg(gdal.GetLastErrorMsg()):
        return
    pytest.fail('did not get expected error message, got %s' % gdal.GetLastErrorMsg())


def test_basic_test_5():
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/doctype.xml', gdal.GA_ReadOnly)
    gdal.PopErrorHandler()
    last_error = gdal.GetLastErrorMsg()
    expected = '`data/doctype.xml\' not recognized as a supported file format'
    if ds is None and expected in last_error:
        return
    pytest.fail()

###############################################################################
# Issue several AllRegister() to check that GDAL drivers are good citizens


def test_basic_test_6():
    gdal.AllRegister()
    gdal.AllRegister()
    gdal.AllRegister()

###############################################################################
# Test fix for #3077 (check that errors are cleared when using UseExceptions())


def basic_test_7_internal():

    with pytest.raises(Exception):
        gdal.Open('non_existing_ds', gdal.GA_ReadOnly)

    # Special case: we should still be able to get the error message
    # until we call a new GDAL function
    assert matches_non_existing_error_msg(gdal.GetLastErrorMsg()), ('did not get expected error message, got %s' % gdal.GetLastErrorMsg())

    # Special case: we should still be able to get the error message
    # until we call a new GDAL function
    assert matches_non_existing_error_msg(gdal.GetLastErrorMsg()), 'did not get expected error message, got %s' % gdal.GetLastErrorMsg()

    assert gdal.GetLastErrorType() != 0, 'did not get expected error type'

    # Should issue an implicit CPLErrorReset()
    gdal.GetCacheMax()

    assert gdal.GetLastErrorType() == 0, 'got unexpected error type'


def test_basic_test_7():
    old_use_exceptions_status = gdal.GetUseExceptions()
    gdal.UseExceptions()
    ret = basic_test_7_internal()
    if old_use_exceptions_status == 0:
        gdal.DontUseExceptions()
    return ret

###############################################################################
# Test gdal.VersionInfo('RELEASE_DATE') and gdal.VersionInfo('LICENSE')


def test_basic_test_8():

    ret = gdal.VersionInfo('RELEASE_DATE')
    assert len(ret) == 8

    python_exe = sys.executable
    if sys.platform == 'win32':
        python_exe = python_exe.replace('\\', '/')

    license_text = gdal.VersionInfo('LICENSE')
    assert (
        license_text.startswith('GDAL/OGR is released under the MIT/X license')
        or 'GDAL/OGR Licensing' in license_text
    )

    # Use a subprocess to avoid the cached license text
    env = os.environ.copy()
    env['GDAL_DATA'] = 'tmp'
    with open('tmp/LICENSE.TXT', 'wt') as f:
        f.write('fake_license')
    license_text = subprocess.check_output(
        [sys.executable, 'basic_test_subprocess.py'],
        env=env
    ).decode('utf-8')
    os.unlink('tmp/LICENSE.TXT')
    assert (
        license_text.startswith(u'fake_license')
    )


###############################################################################
# Test gdal.PushErrorHandler() with a Python error handler


def my_python_error_handler(eErrClass, err_no, msg):
    gdaltest.eErrClass = eErrClass
    gdaltest.err_no = err_no
    gdaltest.msg = msg


def test_basic_test_9():

    gdaltest.eErrClass = 0
    gdaltest.err_no = 0
    gdaltest.msg = ''
    gdal.PushErrorHandler(my_python_error_handler)
    gdal.Error(1, 2, 'test')
    gdal.PopErrorHandler()

    assert gdaltest.eErrClass == 1

    assert gdaltest.err_no == 2

    assert gdaltest.msg == 'test'

###############################################################################
# Test gdal.PushErrorHandler() with a Python error handler as a method (#5186)


class my_python_error_handler_class(object):
    def __init__(self):
        self.eErrClass = None
        self.err_no = None
        self.msg = None

    def handler(self, eErrClass, err_no, msg):
        self.eErrClass = eErrClass
        self.err_no = err_no
        self.msg = msg


def test_basic_test_10():

    # Check that reference counting works OK
    gdal.PushErrorHandler(my_python_error_handler_class().handler)
    gdal.Error(1, 2, 'test')
    gdal.PopErrorHandler()

    error_handler = my_python_error_handler_class()
    gdal.PushErrorHandler(error_handler.handler)
    gdal.Error(1, 2, 'test')
    gdal.PopErrorHandler()

    assert error_handler.eErrClass == 1

    assert error_handler.err_no == 2

    assert error_handler.msg == 'test'

###############################################################################
# Test gdal.OpenEx()


def test_basic_test_11():

    ds = gdal.OpenEx('data/byte.tif')
    assert ds is not None

    ds = gdal.OpenEx('data/byte.tif', gdal.OF_RASTER)
    assert ds is not None

    ds = gdal.OpenEx('data/byte.tif', gdal.OF_VECTOR)
    assert ds is None

    ds = gdal.OpenEx('data/byte.tif', gdal.OF_RASTER | gdal.OF_VECTOR)
    assert ds is not None

    ds = gdal.OpenEx('data/byte.tif', gdal.OF_ALL)
    assert ds is not None

    ds = gdal.OpenEx('data/byte.tif', gdal.OF_UPDATE)
    assert ds is not None

    ds = gdal.OpenEx('data/byte.tif', gdal.OF_RASTER | gdal.OF_VECTOR | gdal.OF_UPDATE | gdal.OF_VERBOSE_ERROR)
    assert ds is not None

    ds = gdal.OpenEx('data/byte.tif', allowed_drivers=[])
    assert ds is not None

    ds = gdal.OpenEx('data/byte.tif', allowed_drivers=['GTiff'])
    assert ds is not None

    ds = gdal.OpenEx('data/byte.tif', allowed_drivers=['PNG'])
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.OpenEx('data/byte.tif', open_options=['FOO'])
    assert ds is not None

    ar_ds = [gdal.OpenEx('data/byte.tif', gdal.OF_SHARED) for _ in range(1024)]
    assert ar_ds[1023] is not None
    ar_ds = None

    ds = gdal.OpenEx('../ogr/data/poly.shp', gdal.OF_RASTER)
    assert ds is None

    ds = gdal.OpenEx('../ogr/data/poly.shp', gdal.OF_VECTOR)
    assert ds is not None
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(0) is not None
    ds.GetLayer(0).GetMetadata()

    ds = gdal.OpenEx('../ogr/data/poly.shp', allowed_drivers=['ESRI Shapefile'])
    assert ds is not None

    ds = gdal.OpenEx('../ogr/data/poly.shp', gdal.OF_RASTER | gdal.OF_VECTOR)
    assert ds is not None

    ds = gdal.OpenEx('non existing')
    assert ds is None and gdal.GetLastErrorMsg() == ''

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.OpenEx('non existing', gdal.OF_VERBOSE_ERROR)
    gdal.PopErrorHandler()
    assert ds is None and gdal.GetLastErrorMsg() != ''

    old_use_exceptions_status = gdal.GetUseExceptions()
    gdal.UseExceptions()
    got_exception = False
    try:
        ds = gdal.OpenEx('non existing')
    except RuntimeError:
        got_exception = True
    if old_use_exceptions_status == 0:
        gdal.DontUseExceptions()
    assert got_exception

###############################################################################
# Test GDAL layer API


def test_basic_test_12():

    ds = gdal.GetDriverByName('MEMORY').Create('bar', 0, 0, 0)
    assert ds.GetDescription() == 'bar'
    lyr = ds.CreateLayer("foo")
    assert lyr is not None
    assert lyr.GetDescription() == 'foo'
    from osgeo import ogr
    assert lyr.TestCapability(ogr.OLCCreateField) == 1
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayerByName("foo")
    assert lyr is not None
    lyr = ds.GetLayerByIndex(0)
    assert lyr is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None
    sql_lyr = ds.ExecuteSQL('SELECT * FROM foo')
    assert sql_lyr is not None
    ds.ReleaseResultSet(sql_lyr)
    new_lyr = ds.CopyLayer(lyr, 'bar')
    assert new_lyr is not None
    assert ds.DeleteLayer(0) == 0
    assert ds.DeleteLayer('bar') == 0
    ds.SetStyleTable(ds.GetStyleTable())
    ds = None

###############################################################################
# Test correct sorting of StringList / metadata (#5540, #5557)


def test_basic_test_13():

    ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    for i in range(3):
        if i == 0:
            ds.SetMetadataItem("ScaleBounds", "True")
            ds.SetMetadataItem("ScaleBounds.MinScale", "0")
            ds.SetMetadataItem("ScaleBounds.MaxScale", "2000000")
        elif i == 1:
            ds.SetMetadataItem("ScaleBounds.MaxScale", "2000000")
            ds.SetMetadataItem("ScaleBounds.MinScale", "0")
            ds.SetMetadataItem("ScaleBounds", "True")
        else:
            ds.SetMetadataItem("ScaleBounds.MinScale", "0")
            ds.SetMetadataItem("ScaleBounds", "True")
            ds.SetMetadataItem("ScaleBounds.MaxScale", "2000000")

        assert ds.GetMetadataItem('scalebounds') == 'True'
        assert ds.GetMetadataItem('ScaleBounds') == 'True'
        assert ds.GetMetadataItem('SCALEBOUNDS') == 'True'
        assert ds.GetMetadataItem('ScaleBounds.MinScale') == '0'
        assert ds.GetMetadataItem('ScaleBounds.MaxScale') == '2000000'
    ds = None

    ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    for i in range(200):
        ds.SetMetadataItem("FILENAME_%d" % i, "%d" % i)
    for i in range(200):
        assert ds.GetMetadataItem("FILENAME_%d" % i) == '%d' % i


###############################################################################
# Test SetMetadata()


def test_basic_test_14():

    ds = gdal.GetDriverByName('MEM').Create('', 1, 1)

    ds.SetMetadata('foo')
    assert ds.GetMetadata_List() == ['foo']

    with pytest.raises(Exception):
        ds.SetMetadata(5)
    

    ds.SetMetadata(['foo=bar'])
    assert ds.GetMetadata_List() == ['foo=bar']

    with pytest.raises(Exception):
        ds.SetMetadata([5])
    

    ds.SetMetadata({'foo': 'baz'})
    assert ds.GetMetadata_List() == ['foo=baz']

    with pytest.raises(Exception):
        ds.SetMetadata({'foo': 5})
    

    with pytest.raises(Exception):
        ds.SetMetadata({5: 'baz'})
    

    with pytest.raises(Exception):
        ds.SetMetadata({5: 6})
    

    val = '\u00e9ven'

    ds.SetMetadata({'bar': val})
    assert ds.GetMetadata()['bar'] == val

    ds.SetMetadata({val: 'baz'})
    assert ds.GetMetadata()[val] == 'baz'

    with pytest.raises(Exception):
        ds.SetMetadata({val: 5})
    

    with pytest.raises(Exception):
        ds.SetMetadata({5: val})
    

    
###############################################################################
# Test errors with progress callback


def basic_test_15_cbk_no_argument():
    return None


def basic_test_15_cbk_no_ret(a, b, c):
    # pylint: disable=unused-argument
    return None


def basic_test_15_cbk_bad_ret(a, b, c):
    # pylint: disable=unused-argument
    return 'ok'


def test_basic_test_15():
    mem_driver = gdal.GetDriverByName('MEM')

    with pytest.raises(Exception):
        with gdaltest.error_handler():
            gdal.GetDriverByName('MEM').CreateCopy('', gdal.GetDriverByName('MEM').Create('', 1, 1), callback='foo')


    with gdaltest.error_handler():
        ds = mem_driver.CreateCopy('', mem_driver.Create('', 1, 1), callback=basic_test_15_cbk_no_argument)
    assert ds is None

    with gdaltest.error_handler():
        ds = mem_driver.CreateCopy('', mem_driver.Create('', 1, 1), callback=basic_test_15_cbk_no_ret)
    assert ds is not None

    with gdaltest.error_handler():
        ds = mem_driver.CreateCopy('', mem_driver.Create('', 1, 1), callback=basic_test_15_cbk_bad_ret)
    assert ds is None

###############################################################################
# Test unrecognized and recognized open options prefixed by @


def test_basic_test_16():

    gdal.ErrorReset()
    gdal.OpenEx('data/byte.tif', open_options=['@UNRECOGNIZED=FOO'])
    assert gdal.GetLastErrorMsg() == ''

    gdal.ErrorReset()
    gdal.Translate('/vsimem/temp.tif', 'data/byte.tif', options='-co BLOCKYSIZE=10')
    with gdaltest.error_handler():
        gdal.OpenEx('/vsimem/temp.tif', gdal.OF_UPDATE, open_options=['@NUM_THREADS=INVALID'])
    gdal.Unlink('/vsimem/temp.tif')
    assert 'Invalid value for NUM_THREADS: INVALID' in gdal.GetLastErrorMsg()

###############################################################################
# Test mix of gdal/ogr.UseExceptions()/DontUseExceptions()


def test_basic_test_17():

    from osgeo import ogr

    for _ in range(2):
        ogr.UseExceptions()
        gdal.UseExceptions()
        try:
            gdal.Open('do_not_exist')
        except RuntimeError:
            pass
        gdal.DontUseExceptions()
        ogr.DontUseExceptions()
        assert not gdal.GetUseExceptions()
        assert not ogr.GetUseExceptions()

    for _ in range(2):
        ogr.UseExceptions()
        gdal.UseExceptions()
        try:
            gdal.Open('do_not_exist')
        except RuntimeError:
            pass
        flag = False
        try:
            ogr.DontUseExceptions()
            gdal.DontUseExceptions()
            flag = True
        except:
            gdal.DontUseExceptions()
            ogr.DontUseExceptions()
        assert not flag, 'expected failure'
        assert not gdal.GetUseExceptions()
        assert not ogr.GetUseExceptions()


def test_gdal_getspatialref():

    ds = gdal.Open('data/byte.tif')
    assert ds.GetSpatialRef() is not None

    ds = gdal.Open('data/minfloat.tif')
    assert ds.GetSpatialRef() is None


def test_gdal_setspatialref():

    ds = gdal.Open('data/byte.tif')
    sr = ds.GetSpatialRef()
    ds = gdal.GetDriverByName('MEM').Create('',1,1)
    assert ds.SetSpatialRef(sr) == gdal.CE_None
    sr_got = ds.GetSpatialRef()
    assert sr_got
    assert sr_got.IsSame(sr)


def test_gdal_getgcpspatialref():

    ds = gdal.Open('data/byte.tif')
    assert ds.GetGCPSpatialRef() is None

    ds = gdal.Open('data/byte_gcp.tif')
    assert ds.GetGCPSpatialRef() is not None


def test_gdal_setgcpspatialref():

    ds = gdal.Open('data/byte.tif')
    sr = ds.GetSpatialRef()
    ds = gdal.GetDriverByName('MEM').Create('',1,1)
    gcp = gdal.GCP()
    gcp.GCPPixel = 0
    gcp.GCPLine = 0
    gcp.GCPX = 440720.000
    gcp.GCPY = 3751320.000
    ds.SetGCPs([gcp], sr)
    sr_got = ds.GetGCPSpatialRef()
    assert sr_got
    assert sr_got.IsSame(sr)

