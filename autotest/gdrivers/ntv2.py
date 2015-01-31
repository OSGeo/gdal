#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  NTv2 Testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
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

import os
import sys

sys.path.append( '../pymod' )

import gdaltest

###############################################################################

def ntv2_online_1():

    if not gdaltest.download_file('http://download.osgeo.org/proj/nzgd2kgrid0005.gsb', 'nzgd2kgrid0005.gsb'):
        return 'skip'

    try:
        os.stat('tmp/cache/nzgd2kgrid0005.gsb')
    except:
        return 'skip'

    tst = gdaltest.GDALTest( 'NTV2', 'tmp/cache/nzgd2kgrid0005.gsb', 1, 54971, filename_absolute = 1 )
    gt = (165.95, 0.1, 0.0, -33.95, 0.0, -0.1)
    return tst.testOpen( check_gt = gt, check_prj = 'WGS84' )

###############################################################################

def ntv2_online_2():

    try:
        os.stat('tmp/cache/nzgd2kgrid0005.gsb')
    except:
        return 'skip'

    tst = gdaltest.GDALTest( 'NTV2', 'tmp/cache/nzgd2kgrid0005.gsb', 1, 54971, filename_absolute = 1 )
    return tst.testCreateCopy( vsimem = 1 )

###############################################################################

def ntv2_online_3():

    try:
        os.stat('tmp/cache/nzgd2kgrid0005.gsb')
    except:
        return 'skip'

    tst = gdaltest.GDALTest( 'NTV2', 'tmp/cache/nzgd2kgrid0005.gsb', 1, 54971, filename_absolute = 1 )
    return tst.testCreate( vsimem = 1 )
    
gdaltest_list = [
    ntv2_online_1,
    ntv2_online_2,
    ntv2_online_3,
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'NTV2' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

