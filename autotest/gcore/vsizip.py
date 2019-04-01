#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsizip/vsimem/
# Author:   Even Rouault <even dot rouault at mines dash parid dot org>
#
###############################################################################
# Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

import random


import gdaltest
from osgeo import gdal
import pytest

###############################################################################
# Test writing a ZIP with multiple files and directories


def test_vsizip_1():

    # We can keep the handle open during all the ZIP writing
    hZIP = gdal.VSIFOpenL("/vsizip/vsimem/test.zip", "wb")
    assert hZIP is not None, 'fail 1'

    # One way to create a directory
    f = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/subdir2/", "wb")
    assert f is not None, 'fail 2'
    gdal.VSIFCloseL(f)

    # A more natural one
    gdal.Mkdir("/vsizip/vsimem/test.zip/subdir1", 0)

    # Create 1st file
    f2 = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/subdir3/abcd", "wb")
    assert f2 is not None, 'fail 3'
    gdal.VSIFWriteL("abcd", 1, 4, f2)
    gdal.VSIFCloseL(f2)

    # Test that we cannot read a zip file being written
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    f = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/subdir3/abcd", "rb")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() == 'Cannot read a zip file being written', \
        'expected error'
    assert f is None, 'should not have been successful 1'

    # Create 2nd file
    f3 = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/subdir3/efghi", "wb")
    assert f3 is not None, 'fail 4'
    gdal.VSIFWriteL("efghi", 1, 5, f3)

    # Try creating a 3d file
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    f4 = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/that_wont_work", "wb")
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() == 'Cannot create that_wont_work while another file is being written in the .zip', \
        'expected error'
    assert f4 is None, 'should not have been successful 2'

    gdal.VSIFCloseL(f3)

    # Now we can close the main handle
    gdal.VSIFCloseL(hZIP)

    # ERROR 6: Support only 1 file in archive file /vsimem/test.zip when no explicit in-archive filename is specified
    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = gdal.VSIFOpenL('/vsizip/vsimem/test.zip', 'rb')
    if f is not None:
        gdal.VSIFCloseL(f)
    assert gdal.GetLastErrorMsg() != '', 'expected error'

    f = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/subdir3/abcd", "rb")
    assert f is not None, 'fail 5'
    data = gdal.VSIFReadL(1, 4, f)
    gdal.VSIFCloseL(f)

    assert data.decode('ASCII') == 'abcd'

    # Test alternate uri syntax
    gdal.Rename("/vsimem/test.zip", "/vsimem/test.xxx")
    f = gdal.VSIFOpenL("/vsizip/{/vsimem/test.xxx}/subdir3/abcd", "rb")
    assert f is not None
    data = gdal.VSIFReadL(1, 4, f)
    gdal.VSIFCloseL(f)

    assert data.decode('ASCII') == 'abcd'

    # With a trailing slash
    f = gdal.VSIFOpenL("/vsizip/{/vsimem/test.xxx}/subdir3/abcd/", "rb")
    assert f is not None
    gdal.VSIFCloseL(f)

    # Test ReadDir()
    assert len(gdal.ReadDir("/vsizip/{/vsimem/test.xxx}")) == 3

    # Unbalanced curls
    f = gdal.VSIFOpenL("/vsizip/{/vsimem/test.xxx", "rb")
    assert f is None

    # Non existing mainfile
    f = gdal.VSIFOpenL("/vsizip/{/vsimem/test.xxx}/bla", "rb")
    assert f is None

    # Non existing subfile
    f = gdal.VSIFOpenL("/vsizip/{/vsimem/test.zzz}/bla", "rb")
    assert f is None

    # Wrong syntax
    f = gdal.VSIFOpenL("/vsizip/{/vsimem/test.xxx}.aux.xml", "rb")
    assert f is None

    # Test nested { { } }
    hZIP = gdal.VSIFOpenL("/vsizip/{/vsimem/zipinzip.yyy}", "wb")
    assert hZIP is not None, 'fail 1'
    f = gdal.VSIFOpenL("/vsizip/{/vsimem/zipinzip.yyy}/test.xxx", "wb")
    f_src = gdal.VSIFOpenL("/vsimem/test.xxx", "rb")
    data = gdal.VSIFReadL(1, 10000, f_src)
    gdal.VSIFCloseL(f_src)
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)
    gdal.VSIFCloseL(hZIP)

    f = gdal.VSIFOpenL("/vsizip/{/vsizip/{/vsimem/zipinzip.yyy}/test.xxx}/subdir3/abcd/", "rb")
    assert f is not None
    data = gdal.VSIFReadL(1, 4, f)
    gdal.VSIFCloseL(f)

    assert data.decode('ASCII') == 'abcd'

    gdal.Unlink("/vsimem/test.xxx")
    gdal.Unlink("/vsimem/zipinzip.yyy")

    # Test VSIStatL on a non existing file
    assert gdal.VSIStatL('/vsizip//vsimem/foo.zip') is None

    # Test ReadDir on a non existing file
    assert gdal.ReadDir('/vsizip//vsimem/foo.zip') is None

###############################################################################
# Test writing 2 files in the ZIP by closing it completely between the 2


def test_vsizip_2():

    zip_name = '/vsimem/test2.zip'

    fmain = gdal.VSIFOpenL("/vsizip/" + zip_name + "/foo.bar", "wb")
    assert fmain is not None, 'fail 1'
    gdal.VSIFWriteL("12345", 1, 5, fmain)
    gdal.VSIFCloseL(fmain)

    content = gdal.ReadDir("/vsizip/" + zip_name)
    assert content == ['foo.bar'], 'bad content 1'

    # Now append a second file
    fmain = gdal.VSIFOpenL("/vsizip/" + zip_name + "/bar.baz", "wb")
    assert fmain is not None, 'fail 2'
    gdal.VSIFWriteL("67890", 1, 5, fmain)

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    content = gdal.ReadDir("/vsizip/" + zip_name)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() == 'Cannot read a zip file being written', \
        'expected error'
    assert content is None, 'bad content 2'

    gdal.VSIFCloseL(fmain)

    content = gdal.ReadDir("/vsizip/" + zip_name)
    assert content == ['foo.bar', 'bar.baz'], 'bad content 3'

    fmain = gdal.VSIFOpenL("/vsizip/" + zip_name + "/foo.bar", "rb")
    assert fmain is not None, 'fail 3'
    data = gdal.VSIFReadL(1, 5, fmain)
    gdal.VSIFCloseL(fmain)

    assert data.decode('ASCII') == '12345'

    fmain = gdal.VSIFOpenL("/vsizip/" + zip_name + "/bar.baz", "rb")
    assert fmain is not None, 'fail 4'
    data = gdal.VSIFReadL(1, 5, fmain)
    gdal.VSIFCloseL(fmain)

    assert data.decode('ASCII') == '67890'

    gdal.Unlink(zip_name)


###############################################################################
# Test opening in write mode a file inside a zip archive whose content has been listed before (testcase for fix of r22625)

def test_vsizip_3():

    fmain = gdal.VSIFOpenL("/vsizip/vsimem/test3.zip", "wb")

    f = gdal.VSIFOpenL("/vsizip/vsimem/test3.zip/foo", "wb")
    gdal.VSIFWriteL("foo", 1, 3, f)
    gdal.VSIFCloseL(f)
    f = gdal.VSIFOpenL("/vsizip/vsimem/test3.zip/bar", "wb")
    gdal.VSIFWriteL("bar", 1, 3, f)
    gdal.VSIFCloseL(f)

    gdal.VSIFCloseL(fmain)

    gdal.ReadDir("/vsizip/vsimem/test3.zip")

    f = gdal.VSIFOpenL("/vsizip/vsimem/test3.zip/baz", "wb")
    gdal.VSIFWriteL("baz", 1, 3, f)
    gdal.VSIFCloseL(f)

    res = gdal.ReadDir("/vsizip/vsimem/test3.zip")

    gdal.Unlink("/vsimem/test3.zip")

    assert res == ['foo', 'bar', 'baz']

###############################################################################
# Test ReadRecursive on valid zip


def test_vsizip_4():

    # read recursive and validate content
    res = gdal.ReadDirRecursive("/vsizip/data/testzip.zip")
    assert res is not None, 'fail read'
    assert (res == ['subdir/', 'subdir/subdir/', 'subdir/subdir/uint16.tif',
               'subdir/subdir/test_rpc.txt', 'subdir/test_rpc.txt',
               'test_rpc.txt', 'uint16.tif']), 'bad content'

###############################################################################
# Test ReadRecursive on deep zip


def test_vsizip_5():

    # make file in memory
    fmain = gdal.VSIFOpenL('/vsizip/vsimem/bigdepthzip.zip', 'wb')
    assert fmain is not None

    filename = "a" + "/a" * 1000
    finside = gdal.VSIFOpenL('/vsizip/vsimem/bigdepthzip.zip/' + filename, 'wb')
    assert finside is not None
    gdal.VSIFCloseL(finside)
    gdal.VSIFCloseL(fmain)

    # read recursive and validate content
    res = gdal.ReadDirRecursive("/vsizip/vsimem/bigdepthzip.zip")
    assert res is not None, 'fail read'
    assert len(res) == 1001, ('wrong size: ' + str(len(res)))
    assert res[10] == 'a/a/a/a/a/a/a/a/a/a/a/', ('bad content: ' + res[10])

    gdal.Unlink("/vsimem/bigdepthzip.zip")

###############################################################################
# Test writing 2 files with same name in a ZIP (#4785)


def test_vsizip_6():

    # Maintain ZIP file opened
    fmain = gdal.VSIFOpenL("/vsizip/vsimem/test6.zip", "wb")
    f = gdal.VSIFOpenL("/vsizip/vsimem/test6.zip/foo.bar", "wb")
    assert f is not None
    gdal.VSIFWriteL("12345", 1, 5, f)
    gdal.VSIFCloseL(f)
    f = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    f = gdal.VSIFOpenL("/vsizip/vsimem/test6.zip/foo.bar", "wb")
    gdal.PopErrorHandler()
    if f is not None:
        gdal.VSIFCloseL(f)
        pytest.fail()
    gdal.VSIFCloseL(fmain)
    fmain = None

    gdal.Unlink("/vsimem/test6.zip")

    # Now close it each time
    f = gdal.VSIFOpenL("/vsizip/vsimem/test6.zip/foo.bar", "wb")
    assert f is not None
    gdal.VSIFWriteL("12345", 1, 5, f)
    gdal.VSIFCloseL(f)
    f = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    f = gdal.VSIFOpenL("/vsizip/vsimem/test6.zip/foo.bar", "wb")
    gdal.PopErrorHandler()
    if f is not None:
        gdal.VSIFCloseL(f)
        pytest.fail()

    gdal.Unlink("/vsimem/test6.zip")

###############################################################################
# Test that we use the extended field for UTF-8 filenames (#5361).


def test_vsizip_7():

    content = gdal.ReadDir("/vsizip/data/cp866_plus_utf8.zip")
    ok = 0
    try:
        local_vars = {'content': content, 'ok': ok}
        exec("if content == [u'\u0430\u0431\u0432\u0433\u0434\u0435', u'\u0436\u0437\u0438\u0439\u043a\u043b']: ok = 1", None, local_vars)
        ok = local_vars['ok']
    except:
        if content == ['\u0430\u0431\u0432\u0433\u0434\u0435', '\u0436\u0437\u0438\u0439\u043a\u043b']:
            ok = 1

    if ok == 0:
        print(content)
        pytest.fail('bad content')

    
###############################################################################
# Basic test for ZIP64 support (5 GB file that compresses in less than 4 GB)


def test_vsizip_8():

    assert gdal.VSIStatL('/vsizip/vsizip/data/zero.bin.zip.zip/zero.bin.zip').size == 5000 * 1000 * 1000 + 1

###############################################################################
# Basic test for ZIP64 support (5 GB file that is stored)


def test_vsizip_9():

    assert gdal.VSIStatL('/vsizip//vsisparse/data/zero_stored.bin.xml.zip/zero.bin').size == 5000 * 1000 * 1000 + 1

    assert gdal.VSIStatL('/vsizip//vsisparse/data/zero_stored.bin.xml.zip/hello.txt').size == 6

    f = gdal.VSIFOpenL('/vsizip//vsisparse/data/zero_stored.bin.xml.zip/zero.bin', 'rb')
    gdal.VSIFSeekL(f, 5000 * 1000 * 1000, 0)
    data = gdal.VSIFReadL(1, 1, f)
    gdal.VSIFCloseL(f)
    assert data.decode('ascii') == '\x03'

    f = gdal.VSIFOpenL('/vsizip//vsisparse/data/zero_stored.bin.xml.zip/hello.txt', 'rb')
    data = gdal.VSIFReadL(1, 6, f)
    gdal.VSIFCloseL(f)
    assert data.decode('ascii') == 'HELLO\n'

###############################################################################
# Test that we recode filenames in ZIP (#5361)


def test_vsizip_10():

    gdal.SetConfigOption('CPL_ZIP_ENCODING', 'CP866')
    content = gdal.ReadDir("/vsizip/data/cp866.zip")
    gdal.SetConfigOption('CPL_ZIP_ENCODING', None)
    ok = 0
    try:
        local_vars = {'content': content, 'ok': ok}
        exec("if content == [u'\u0430\u0431\u0432\u0433\u0434\u0435', u'\u0436\u0437\u0438\u0439\u043a\u043b']: ok = 1", None, local_vars)
        ok = local_vars['ok']
    except:
        if content == ['\u0430\u0431\u0432\u0433\u0434\u0435', '\u0436\u0437\u0438\u0439\u043a\u043b']:
            ok = 1

    if ok == 0:
        if gdal.GetLastErrorMsg().find('Recode from CP866 to UTF-8 not supported') >= 0:
            pytest.skip()

        print(content)
        pytest.fail('bad content')

    
###############################################################################
# Test that we don't do anything with ZIP with filenames in UTF-8 already (#5361)


def test_vsizip_11():

    content = gdal.ReadDir("/vsizip/data/utf8.zip")
    ok = 0
    try:
        local_vars = {'content': content, 'ok': ok}
        exec("if content == [u'\u0430\u0431\u0432\u0433\u0434\u0435', u'\u0436\u0437\u0438\u0439\u043a\u043b']: ok = 1", None, local_vars)
        ok = local_vars['ok']
    except:
        if content == ['\u0430\u0431\u0432\u0433\u0434\u0435', '\u0436\u0437\u0438\u0439\u043a\u043b']:
            ok = 1

    if ok == 0:
        print(content)
        pytest.fail('bad content')

    
###############################################################################
# Test changing the content of a zip file (#6005)


def test_vsizip_12():

    fmain = gdal.VSIFOpenL("/vsizip/vsimem/vsizip_12_src1.zip", "wb")
    f = gdal.VSIFOpenL("/vsizip/vsimem/vsizip_12_src1.zip/foo.bar", "wb")
    data = '0123456'
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)
    gdal.VSIFCloseL(fmain)

    fmain = gdal.VSIFOpenL("/vsizip/vsimem/vsizip_12_src2.zip", "wb")
    f = gdal.VSIFOpenL("/vsizip/vsimem/vsizip_12_src2.zip/bar.baz", "wb")
    data = '01234567'
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)
    gdal.VSIFCloseL(fmain)

    # Copy vsizip_12_src1 into vsizip_12
    f = gdal.VSIFOpenL('/vsimem/vsizip_12_src1.zip', 'rb')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL('/vsimem/vsizip_12.zip', 'wb')
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    gdal.ReadDir('/vsizip/vsimem/vsizip_12.zip')

    # Copy vsizip_12_src2 into vsizip_12
    f = gdal.VSIFOpenL('/vsimem/vsizip_12_src2.zip', 'rb')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    f = gdal.VSIFOpenL('/vsimem/vsizip_12.zip', 'wb')
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    content = gdal.ReadDir('/vsizip/vsimem/vsizip_12.zip')

    gdal.Unlink('/vsimem/vsizip_12_src1.zip')
    gdal.Unlink('/vsimem/vsizip_12_src2.zip')
    gdal.Unlink('/vsimem/vsizip_12.zip')

    assert content == ['bar.baz']

###############################################################################
# Test ReadDir() truncation


def test_vsizip_13():

    fmain = gdal.VSIFOpenL("/vsizip/vsimem/vsizip_13.zip", "wb")
    for i in range(10):
        f = gdal.VSIFOpenL("/vsizip/vsimem/vsizip_13.zip/%d" % i, "wb")
        gdal.VSIFCloseL(f)
    gdal.VSIFCloseL(fmain)

    lst = gdal.ReadDir('/vsizip/vsimem/vsizip_13.zip')
    assert len(lst) >= 4
    # Test truncation
    lst_truncated = gdal.ReadDir('/vsizip/vsimem/vsizip_13.zip', int(len(lst) / 2))
    assert len(lst_truncated) > int(len(lst) / 2)

    gdal.Unlink('/vsimem/vsizip_13.zip')

###############################################################################
# Test that we can recode filenames in ZIP when writing (#6631)


def test_vsizip_14():

    fmain = gdal.VSIFOpenL('/vsizip//vsimem/vsizip_14.zip', 'wb')
    try:
        x = ['']
        exec("x[0] = u'\u0430\u0431\u0432\u0433\u0434\u0435'")
        cp866_filename = x[0]
    except:
        cp866_filename = '\u0430\u0431\u0432\u0433\u0434\u0435'

    with gdaltest.error_handler():
        f = gdal.VSIFOpenL('/vsizip//vsimem/vsizip_14.zip/' + cp866_filename, 'wb')
    if f is None:
        gdal.VSIFCloseL(fmain)
        gdal.Unlink('/vsimem/vsizip_14.zip')
        pytest.skip()

    gdal.VSIFWriteL('hello', 1, 5, f)
    gdal.VSIFCloseL(f)
    gdal.VSIFCloseL(fmain)

    content = gdal.ReadDir("/vsizip//vsimem/vsizip_14.zip")
    assert content == [cp866_filename], 'bad content'

    gdal.Unlink('/vsimem/vsizip_14.zip')

###############################################################################
# Test multithreaded compression


def test_vsizip_multi_thread():

    with gdaltest.config_options({'GDAL_NUM_THREADS': 'ALL_CPUS',
                                  'CPL_VSIL_DEFLATE_CHUNK_SIZE': '32K'}):
        fmain = gdal.VSIFOpenL('/vsizip//vsimem/vsizip_multi_thread.zip', 'wb')
        f = gdal.VSIFOpenL('/vsizip//vsimem/vsizip_multi_thread.zip/test', 'wb')
        for i in range(100000):
            gdal.VSIFWriteL('hello', 1, 5, f)
        gdal.VSIFCloseL(f)
        gdal.VSIFCloseL(fmain)

    f = gdal.VSIFOpenL('/vsizip//vsimem/vsizip_multi_thread.zip/test', 'rb')
    data = gdal.VSIFReadL(100000, 5, f).decode('ascii')
    gdal.VSIFCloseL(f)

    gdal.Unlink('/vsimem/vsizip_multi_thread.zip')

    if data != 'hello' * 100000:
        for i in range(10000):
            if data[i*5:i*5+5] != 'hello':
                print(i*5, data[i*5:i*5+5], data[i*5-5:i*5+5-5])
                break

        pytest.fail()

###############################################################################
# Test multithreaded compression, with I/O error


def test_vsizip_multi_thread_error():

    with gdaltest.error_handler():
        with gdaltest.config_options({'GDAL_NUM_THREADS': 'ALL_CPUS',
                                    'CPL_VSIL_DEFLATE_CHUNK_SIZE': '16K'}):
            fmain = gdal.VSIFOpenL('/vsizip/{/vsimem/vsizip_multi_thread.zip||maxlength=1000}', 'wb')
            f = gdal.VSIFOpenL('/vsizip/{/vsimem/vsizip_multi_thread.zip||maxlength=1000}/test', 'wb')
            for i in range(100000):
                gdal.VSIFWriteL('hello', 1, 5, f)
            gdal.VSIFCloseL(f)
            gdal.VSIFCloseL(fmain)

    gdal.Unlink('/vsimem/vsizip_multi_thread.zip')

###############################################################################
# Test multithreaded compression, below the threshold where it triggers


def test_vsizip_multi_thread_below_threshold():

    with gdaltest.config_options({'GDAL_NUM_THREADS': 'ALL_CPUS'}):
        fmain = gdal.VSIFOpenL('/vsizip//vsimem/vsizip_multi_thread.zip', 'wb')
        f = gdal.VSIFOpenL('/vsizip//vsimem/vsizip_multi_thread.zip/test', 'wb')
        gdal.VSIFWriteL('hello', 1, 5, f)
        gdal.VSIFCloseL(f)
        gdal.VSIFCloseL(fmain)

    f = gdal.VSIFOpenL('/vsizip//vsimem/vsizip_multi_thread.zip/test', 'rb')
    data = gdal.VSIFReadL(1, 5, f).decode('ascii')
    gdal.VSIFCloseL(f)

    gdal.Unlink('/vsimem/vsizip_multi_thread.zip')

    assert data == 'hello'

###############################################################################
# Test creating ZIP64 file: uncompressed larger than 4GB, but compressed
# data stream < 4 GB


def test_vsizip_create_zip64():

    if not gdaltest.run_slow_tests():
        pytest.skip()

    niters = 1000
    s = 'hello' * 1000 * 1000
    zip_name = '/vsimem/vsizip_create_zip64.zip'
    with gdaltest.config_options({'GDAL_NUM_THREADS': 'ALL_CPUS'}):
        fmain = gdal.VSIFOpenL('/vsizip/' + zip_name, 'wb')
        f = gdal.VSIFOpenL('/vsizip/' + zip_name + '/test', 'wb')
        for i in range(niters):
            gdal.VSIFWriteL(s, 1, len(s), f)
        gdal.VSIFCloseL(f)
        gdal.VSIFCloseL(fmain)

    size = gdal.VSIStatL(zip_name).size
    assert size <= 0xFFFFFFFF

    size = gdal.VSIStatL('/vsizip/' + zip_name + '/test').size
    assert size == len(s) * niters

    f = gdal.VSIFOpenL('/vsizip/' + zip_name + '/test', 'rb')
    data = gdal.VSIFReadL(1, len(s), f).decode('ascii')
    gdal.VSIFCloseL(f)
    assert data == s

    gdal.Unlink(zip_name)

###############################################################################
# Test creating ZIP64 file: compressed data stream > 4 GB


def test_vsizip_create_zip64_stream_larger_than_4G():

    if not gdaltest.run_slow_tests():
        pytest.skip()

    zip_name = 'tmp/vsizip_create_zip64_stream_larger_than_4G.zip'

    gdal.Unlink(zip_name)

    niters = 999
    s = ''.join([chr(random.randint(0, 127)) for i in range(5 * 1000 * 1000)])
    with gdaltest.config_options({'GDAL_NUM_THREADS': 'ALL_CPUS'}):
        f = gdal.VSIFOpenL('/vsizip/' + zip_name + '/test2', 'wb')
        for i in range(niters):
            gdal.VSIFWriteL(s, 1, len(s), f)
        gdal.VSIFCloseL(f)

    size = gdal.VSIStatL(zip_name).size
    assert size > 0xFFFFFFFF

    size = gdal.VSIStatL('/vsizip/' + zip_name + '/test2').size
    assert size == len(s) * niters

    f = gdal.VSIFOpenL('/vsizip/' + zip_name + '/test2', 'rb')
    data = gdal.VSIFReadL(1, len(s), f).decode('ascii')
    gdal.VSIFCloseL(f)
    assert data == s

    gdal.Unlink(zip_name)


###############################################################################
def test_vsizip_byte_zip64_local_header_zeroed():

    size = gdal.VSIStatL('/vsizip/data/byte_zip64_local_header_zeroed.zip/byte.tif').size
    assert size == 736



