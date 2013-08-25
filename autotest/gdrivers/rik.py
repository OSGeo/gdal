#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  RIK Testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at mines dash paris dot org>
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
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test a RIK map
# Data downloaded from : http://www.lantmateriet.se/upload/filer/kartor/programvaror/sverige500_swe99.zip

def rik_online_1():

    try:
        if gdal.GetDriverByName('RIK') is None:
            return 'skip'
    except:
        return 'skip'

    if not gdaltest.download_file('http://www.lantmateriet.se/upload/filer/kartor/programvaror/sverige500_swe99.zip', 'sverige500_swe99.zip'):
        return 'skip'

    try:
        os.stat('tmp/cache/sverige500_swe99.rik')
        file_to_test = 'tmp/cache/sverige500_swe99.rik'
    except:
        try:
            print('Uncompressing ZIP file...')
            import zipfile
            zfobj = zipfile.ZipFile('tmp/cache/sverige500_swe99.zip')
            outfile = open('tmp/cache/sverige500_swe99.rik', 'wb')
            outfile.write(zfobj.read('sverige500_swe99.rik'))
            outfile.close()
            file_to_test = 'tmp/cache/sverige500_swe99.rik'
        except:
            return 'skip'

    tst = gdaltest.GDALTest('RIK', file_to_test, 1, 17162, filename_absolute = 1 )
    return tst.testOpen()

###############################################################################
# Test a LZW compressed RIK dataset

def rik_online_2():

    try:
        if gdal.GetDriverByName('RIK') is None:
            return 'skip'
    except:
        return 'skip'

    if not gdaltest.download_file('http://trac.osgeo.org/gdal/raw-attachment/ticket/3674/ab-del.rik', 'ab-del.rik'):
        return 'skip'

    tst = gdaltest.GDALTest('RIK', 'tmp/cache/ab-del.rik', 1, 44974, filename_absolute = 1 )
    return tst.testOpen()

gdaltest_list = [
    rik_online_1,
    rik_online_2
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'RIK' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

