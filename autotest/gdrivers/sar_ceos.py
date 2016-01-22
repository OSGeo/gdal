#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SAR_CEOS driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
def sar_ceos_online_1():
    list_files = [ 'ottawa_patch.img',
                   'ottawa_patch.led',
                   'ottawa_patch.nul',
                   'ottawa_patch.trl',
                   'ottawa_patch.vol' ]

    for filename in list_files:
        if not gdaltest.download_file('http://download.osgeo.org/gdal/data/ceos/' + filename , filename):
            return 'skip'

    tst = gdaltest.GDALTest( 'SAR_CEOS', 'tmp/cache/ottawa_patch.img', 1, 23026, filename_absolute = 1 )
    return tst.testOpen()

gdaltest_list = [
    sar_ceos_online_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'sar_ceos' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

