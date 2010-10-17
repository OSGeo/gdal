#!/usr/bin/env python
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

    # We must keep the handle open during all the ZIP writing
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

    # Create 2nd file
    f3 = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/subdir3/efghi", "wb")
    if f3 is None:
        gdaltest.post_reason('fail 4')
        return 'fail'
    gdal.VSIFWriteL("efghi", 1, 5, f3)

    # Try creating a 3d file
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    f4 = gdal.VSIFOpenL("/vsizip/vsimem/test.zip/that_wont_work", "wb")
    gdal.PopErrorHandler()
    if f4 is not None:
        gdaltest.post_reason('should not have been successful')
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

    # Now append a second file
    fmain = gdal.VSIFOpenL("/vsizip/vsimem/test2.zip/bar.baz", "wb")
    if fmain is None:
        gdaltest.post_reason('fail 2')
        return 'fail'
    gdal.VSIFWriteL("67890", 1, 5, fmain)
    gdal.VSIFCloseL(fmain)

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


gdaltest_list = [ vsizip_1,
                  vsizip_2 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'vsizip' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

