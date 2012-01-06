#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for MBTiles driver.
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2012, Even Rouault, <even dot rouault at mines dash paris dot org>
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
import gdal
import ogr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Get the mbtiles driver

def mbtiles_1():

    try:
        gdaltest.mbtiles_drv = gdal.GetDriverByName( 'MBTiles' )
    except:
        gdaltest.mbtiles_drv = None

    return 'success'

###############################################################################
# Basic test

def mbtiles_2():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName( 'JPEG' ) is None:
        return 'skip'

    ds = gdal.Open('data/world_l1.mbtiles')
    if ds is None:
        return 'fail'

    if ds.RasterCount != 3:
        gdaltest.post_reason('expected 3 bands')
        return 'fail'

    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('did not get expected overview count')
        return 'fail'

    expected_cs_tab = [6324, 19386, 45258]
    for i in range(3):
        cs = ds.GetRasterBand(i + 1).Checksum()
        if ds.GetRasterBand(i + 1).GetColorInterpretation() != gdal.GCI_RedBand + i:
            gdaltest.post_reason('bad color interpretation')
            return 'fail'
        expected_cs = expected_cs_tab[i]
        if cs != expected_cs:
            gdaltest.post_reason('for band %d, cs = %d, different from expected_cs = %d' % (i + 1, cs, expected_cs))
            return 'fail'

    expected_cs_tab = [16642, 15772, 10029]
    for i in range(3):
        cs = ds.GetRasterBand(i + 1).GetOverview(0).Checksum()
        expected_cs = expected_cs_tab[i]
        if cs != expected_cs:
            gdaltest.post_reason('for overview of band %d, cs = %d, different from expected_cs = %d' % (i + 1, cs, expected_cs))
            return 'fail'

    if ds.GetProjectionRef().find('3857') == -1:
        gdaltest.post_reason('projection_ref = %s' % ds.GetProjectionRef())
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = ( -20037500.0, 78271.484375, 0.0, 20037500.0, 0.0, -78271.484375 )
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-15:
            gdaltest.post_reason('bad gt')
            print(gt)
            print(expected_gt)
            return 'fail'

    md = ds.GetMetadata()
    if md['bounds'] != '-180.0,-85,180,85':
        gdaltest.post_reason('bad metadata')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Cleanup

def mbtiles_cleanup():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    return 'success'

gdaltest_list = [
    mbtiles_1,
    mbtiles_2,
    mbtiles_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'mbtiles' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
