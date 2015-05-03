#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test TIL driver
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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

import sys
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test a fake TIL dataset

def til_1():

    tst = gdaltest.GDALTest( 'TIL', 'testtil.til', 1, 4672 )
    return tst.testOpen()

###############################################################################
# Check GetFileList() result (#4018) & IMD

def til_2():

    try:
        os.remove('data/testtil.til.aux.xml')
    except:
        pass

    ds = gdal.Open( 'data/testtil.til' )
    filelist = ds.GetFileList()

    if len(filelist) != 3:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'CLOUDCOVER' not in md:
        print('CLOUDCOVER not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    ds = None

    try:
        os.stat('data/testtil.til.aux.xml')
        gdaltest.post_reason('Expected not generation of data/testtil.til.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Check GetFileList() & XML

def til_3():

    try:
        os.remove('data/testtil.til.aux.xml')
    except:
        pass

    ds = gdal.Open( 'data/testtil2.til' )
    filelist = ds.GetFileList()

    if len(filelist) != 3:
        gdaltest.post_reason( 'did not get expected file list.' )
        return 'fail'

    md = ds.GetMetadata('IMAGERY')
    if 'SATELLITEID' not in md:
        print('SATELLITEID not present in IMAGERY Domain')
        return 'fail'
    if 'CLOUDCOVER' not in md:
        print('CLOUDCOVER not present in IMAGERY Domain')
        return 'fail'
    if 'ACQUISITIONDATETIME' not in md:
        print('ACQUISITIONDATETIME not present in IMAGERY Domain')
        return 'fail'

    ds = None

    try:
        os.stat('data/testtil.til.aux.xml')
        gdaltest.post_reason('Expected not generation of data/testtil.til.aux.xml')
        return 'fail'
    except:
        pass

    return 'success'

gdaltest_list = [
    til_1,
    til_2,
    til_3 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'til' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

