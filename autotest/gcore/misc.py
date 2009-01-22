#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Various test of GDAL core.
# Author:   Even Rouault <even dot rouault at mines dash parid dot org>
# 
###############################################################################
# Copyright (c) 2009 Even Rouault <even dot rouault at mines dash parid dot org>
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

import gdal
import sys

sys.path.append( '../pymod' )

import gdaltest


###############################################################################
# Test that the constructor of GDALDataset() behaves well with a big number of
# opened/created datasets

def misc_1():

    tab_ds = [None for i in range(5000)]
    drv = gdal.GetDriverByName('MEM')
    for i in range(len(tab_ds)):
        name = 'mem_%d' % i
        tab_ds[i] = drv.Create( name, 1, 1, 1 )
        if tab_ds[i] is None:
            return 'fail'

    for i in range(len(tab_ds)):
        tab_ds[i] = None

    return 'success'

###############################################################################
# Test that OpenShared() works as expected by opening a big number of times
# the same dataset with it. If it didn't work, that would exhaust the system limit
# of maximum file descriptors opened at the same time

def misc_2():

    tab_ds = [None for i in range(5000)]
    for i in range(len(tab_ds)):
        tab_ds[i] = gdal.OpenShared('data/byte.tif')
        if tab_ds[i] is None:
            return 'fail'

    for i in range(len(tab_ds)):
        tab_ds[i] = None

    return 'success'

###############################################################################
# Test OpenShared() with a dataset whose filename != description (#2797)

def misc_3():

    ds = gdal.OpenShared('../gdrivers/data/small16.aux')
    ds.GetRasterBand(1).Checksum()
    cache_size = gdal.GetCacheUsed()

    ds2 = gdal.OpenShared('../gdrivers/data/small16.aux')
    ds2.GetRasterBand(1).Checksum()
    cache_size2 = gdal.GetCacheUsed()

    if cache_size != cache_size2:
        print "--> OpenShared didn't work as expected"

    ds = None
    ds2 = None

    return 'success'


gdaltest_list = [ misc_1,
                  misc_2,
                  misc_3 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'misc' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

