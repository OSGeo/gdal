#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsizip/vsimem/
# Author:   Even Rouault <even dot rouault at mines dash parid dot org>
#
###############################################################################
# Copyright (c) 2010 Even Rouault <even dot rouault at mines dash parid dot org>
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
import gdal

###############################################################################
# Test writing a ZIP with multiple files and directories

def vsizip_1():

    # We can keep the handle open during all the ZIP writing
    hZIP = gdal.VSIFOpenL("/vsizip/vsimem/test.zip", "wb")
    if hZIP is None:
        gdaltest.post_reason('fail 1')
        return 'fail'

    # One way to create a directory
    f = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/subdir2/", "wb")
    if f is None:
        gdaltest.post_reason('fail 2')
        return 'fail'
    gdal.VSIFCloseL(f)

    # A more natural one
    gdal.Mkdir("/vsizip/vsimem/test.zip/subdir1", 0)

    # Create 1st file
    f2 = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/subdir3/abcd", "wb")
    if f2 is None:
        gdaltest.post_reason('fail 3')
        return 'fail'
    gdal.VSIFWriteL("abcd", 1, 4, f2)
    gdal.VSIFCloseL(f2)

    # Test that we cannot read a zip file being written
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    f = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/subdir3/abcd", "rb")
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() != 'Cannot read a zip file being written':
        gdaltest.post_reason('expected error')
        print(gdal.GetLastErrorMsg())
        return 'fail'
    if f is not None:
        gdaltest.post_reason('should not have been successful 1')
        return 'fail'

    # Create 2nd file
    f3 = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/subdir3/efghi", "wb")
    if f3 is None:
        gdaltest.post_reason('fail 4')
        return 'fail'
    gdal.VSIFWriteL("efghi", 1, 5, f3)

    # Try creating a 3d file
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    f4 = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/that_wont_work", "wb")
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() != 'Cannot create that_wont_work while another file is being written in the .zip':
        gdaltest.post_reason('expected error')
        print(gdal.GetLastErrorMsg())
        return 'fail'
    if f4 is not None:
        gdaltest.post_reason('should not have been successful 2')
        return 'fail'
    
    gdal.VSIFCloseL(f3)

    # Now we can close the main handle
    gdal.VSIFCloseL(hZIP)

    f = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/subdir3/abcd", "rb")
    if f is None:
        gdaltest.post_reason('fail 5')
        return 'fail'
    data = gdal.VSIFReadL(1, 4, f)
    gdal.VSIFCloseL(f)

    gdal.Unlink("/vsimem/test.zip")

    if data.decode('ASCII') != 'abcd':
        print(data)
        return 'fail'

    return 'success'

###############################################################################
# Test writing 2 files in the ZIP by closing it completely between the 2

def vsizip_2():

    fmain = gdal.VSIFOpenL("/vsizip/vsimem/test2.zip/foo.bar", "wb")
    if fmain is None:
        gdaltest.post_reason('fail 1')
        return 'fail'
    gdal.VSIFWriteL("12345", 1, 5, fmain)
    gdal.VSIFCloseL(fmain)

    content = gdal.ReadDir("/vsizip/vsimem/test2.zip")
    if content != ['foo.bar']:
        gdaltest.post_reason('bad content 1')
        print(content)
        return 'fail'

    # Now append a second file
    fmain = gdal.VSIFOpenL("/vsizip/vsimem/test2.zip/bar.baz", "wb")
    if fmain is None:
        gdaltest.post_reason('fail 2')
        return 'fail'
    gdal.VSIFWriteL("67890", 1, 5, fmain)

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    content = gdal.ReadDir("/vsizip/vsimem/test2.zip")
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() != 'Cannot read a zip file being written':
        gdaltest.post_reason('expected error')
        print(gdal.GetLastErrorMsg())
        return 'fail'
    if content != None:
        gdaltest.post_reason('bad content 2')
        print(content)
        return 'fail'

    gdal.VSIFCloseL(fmain)

    content = gdal.ReadDir("/vsizip/vsimem/test2.zip")
    if content != ['foo.bar', 'bar.baz']:
        gdaltest.post_reason('bad content 3')
        print(content)
        return 'fail'

    fmain = gdal.VSIFOpenL("/vsizip/vsimem/test2.zip/foo.bar", "rb")
    if fmain is None:
        gdaltest.post_reason('fail 3')
        return 'fail'
    data = gdal.VSIFReadL(1, 5, fmain)
    gdal.VSIFCloseL(fmain)

    if data.decode('ASCII') != '12345':
        print(data)
        return 'fail'

    fmain = gdal.VSIFOpenL("/vsizip/vsimem/test2.zip/bar.baz", "rb")
    if fmain is None:
        gdaltest.post_reason('fail 4')
        return 'fail'
    data = gdal.VSIFReadL(1, 5, fmain)
    gdal.VSIFCloseL(fmain)

    if data.decode('ASCII') != '67890':
        print(data)
        return 'fail'

    gdal.Unlink("/vsimem/test2.zip")

    return 'success'


###############################################################################
# Test opening in write mode a file inside a zip archive whose content has been listed before (testcase for fix of r22625)

def vsizip_3():

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

    if res != ['foo', 'bar', 'baz']:
        print(res)
        return 'fail'

    return 'success'

###############################################################################
# Test ReadRecursive on valid zip

def vsizip_4():

    # read recursive and validate content
    res = gdal.ReadDirRecursive("/vsizip/data/testzip.zip")
    if res is None:
        gdaltest.post_reason('fail read')
        return 'fail'
    if res != ['subdir/', 'subdir/subdir/', 'subdir/subdir/uint16.tif', \
                   'subdir/subdir/test_rpc.txt', 'subdir/test_rpc.txt', \
                   'test_rpc.txt', 'uint16.tif']:
        gdaltest.post_reason('bad content')
        print(res)
        return 'fail'

    return 'success'

###############################################################################
# Test ReadRecursive on deep zip

def vsizip_5():

    # make file in memory
    fmain = gdal.VSIFOpenL('/vsizip/vsimem/bigdepthzip.zip', 'wb')
    if fmain is None:
        gdaltest.post_reason('fail')
        return 'fail'

    filename = "a"
    for i in range(1000):
        filename = filename + "/a"
    finside = gdal.VSIFOpenL('/vsizip/vsimem/bigdepthzip.zip/' + filename, 'wb')
    if finside is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFCloseL(finside)
    gdal.VSIFCloseL(fmain)

    # read recursive and validate content
    res = gdal.ReadDirRecursive("/vsizip/vsimem/bigdepthzip.zip")
    if res is None:
        gdaltest.post_reason('fail read')
        return 'fail'
    if len(res) != 1001:
        gdaltest.post_reason('wrong size: '+str(len(res)))
        return 'fail'
    if res[10] != 'a/a/a/a/a/a/a/a/a/a/a/':
        gdaltest.post_reason('bad content: '+res[10])
        return 'fail'

    return 'success'

###############################################################################
# Test writing 2 files with same name in a ZIP (#4785)

def vsizip_6():

    # Maintain ZIP file opened
    fmain = gdal.VSIFOpenL("/vsizip/vsimem/test6.zip", "wb")
    f = gdal.VSIFOpenL("/vsizip/vsimem/test6.zip/foo.bar", "wb")
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFWriteL("12345", 1, 5, f)
    gdal.VSIFCloseL(f)
    f = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    f = gdal.VSIFOpenL("/vsizip/vsimem/test6.zip/foo.bar", "wb")
    gdal.PopErrorHandler()
    if f is not None:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(f)
        return 'fail'
    gdal.VSIFCloseL(fmain)
    fmain = None

    gdal.Unlink("/vsimem/test6.zip")

    # Now close it each time
    f = gdal.VSIFOpenL("/vsizip/vsimem/test6.zip/foo.bar", "wb")
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.VSIFWriteL("12345", 1, 5, f)
    gdal.VSIFCloseL(f)
    f = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    f = gdal.VSIFOpenL("/vsizip/vsimem/test6.zip/foo.bar", "wb")
    gdal.PopErrorHandler()
    if f is not None:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(f)
        return 'fail'

    gdal.Unlink("/vsimem/test6.zip")

    return 'success'


gdaltest_list = [ vsizip_1,
                  vsizip_2,
                  vsizip_3,
                  vsizip_4,
                  vsizip_5,
                  vsizip_6,
                  ]


if __name__ == '__main__':

    gdaltest.setup_run( 'vsizip' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

