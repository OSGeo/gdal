#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsicurl_streaming
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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
import time
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
#

def vsicurl_streaming_1():
    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    fp = gdal.VSIFOpenL('/vsicurl_streaming/http://download.osgeo.org/gdal/data/usgsdem/cded/114p01_0100_deme.dem', 'rb')
    if fp is None:
        if gdaltest.gdalurlopen('http://download.osgeo.org/gdal/data/usgsdem/cded/114p01_0100_deme.dem') is None:
            print('cannot open URL')
            return 'skip'
        gdaltest.post_reason('fail')
        return 'fail'

    if gdal.VSIFTellL(fp) != 0:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'
    data = gdal.VSIFReadL(1, 50, fp)
    if data.decode('ascii') != '                              114p01DEMe   Base Ma':
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'
    if gdal.VSIFTellL(fp) != 50:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'

    gdal.VSIFSeekL(fp, 0, 0)

    if gdal.VSIFTellL(fp) != 0:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'
    data = gdal.VSIFReadL(1, 50, fp)
    if data.decode('ascii') != '                              114p01DEMe   Base Ma':
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'
    if gdal.VSIFTellL(fp) != 50:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'

    time.sleep(0.5)
    gdal.VSIFSeekL(fp, 2001, 0)
    data_2001 = gdal.VSIFReadL(1, 20, fp)
    if data_2001.decode('ascii') != '7-32767-32767-32767-':
        print(data_2001)
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'
    if gdal.VSIFTellL(fp) != 2001+20:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'

    gdal.VSIFSeekL(fp, 0, 2)
    if gdal.VSIFTellL(fp) != 9839616:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'

    nRet = len(gdal.VSIFReadL(1, 10, fp))
    if nRet != 0:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'

    gdal.VSIFSeekL(fp, 2001, 0)
    data_2001_2 = gdal.VSIFReadL(1, 20, fp)
    if gdal.VSIFTellL(fp) != 2001+20:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'

    if data_2001 != data_2001_2:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'

    gdal.VSIFSeekL(fp, 1024 * 1024 + 100, 0)
    data = gdal.VSIFReadL(1, 20, fp)
    if data.decode('ascii') != '67-32767-32767-32767':
        print(data)
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'
    if gdal.VSIFTellL(fp) != 1024 * 1024 + 100+ 20:
        gdaltest.post_reason('fail')
        gdal.VSIFCloseL(fp)
        return 'fail'

    gdal.VSIFCloseL(fp)

    return 'success'

gdaltest_list = [ vsicurl_streaming_1 ]

if __name__ == '__main__':

    gdal.SetConfigOption('GDAL_RUN_SLOW_TESTS', 'YES')

    gdaltest.setup_run( 'vsicurl_streaming' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

