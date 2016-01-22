#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test E00GRID driver
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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
# Test a fake E00GRID dataset

def e00grid_1():

    tst = gdaltest.GDALTest( 'E00GRID', 'fake_e00grid.e00', 1, 65359 )
    expected_gt = [ 500000.0, 1000.0, 0.0, 4000000.0, 0.0, -1000.0 ]
    expected_srs = """PROJCS["UTM Zone 15, Northern Hemisphere",
    GEOGCS["NAD83",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6269"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9108"]],
        AUTHORITY["EPSG","4269"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-93],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["METERS",1]]"""
    ret = tst.testOpen(check_gt = expected_gt, check_prj = expected_srs)

    if ret == 'success':
        ds = gdal.Open('data/fake_e00grid.e00')
        if ds.GetRasterBand(1).GetNoDataValue() != -32767:
            gdaltest.post_reason('did not get expected nodata value')
            return 'fail'
        if ds.GetRasterBand(1).GetUnitType() != 'ft':
            gdaltest.post_reason('did not get expected nodata value')
            return 'fail'

    return ret

###############################################################################
# Test a fake E00GRID dataset, compressed and with statistics

def e00grid_2():

    tst = gdaltest.GDALTest( 'E00GRID', 'fake_e00grid_compressed.e00', 1, 65347 )
    expected_gt = [ 500000.0, 1000.0, 0.0, 4000000.0, 0.0, -1000.0 ]
    expected_srs = """PROJCS["UTM Zone 15, Northern Hemisphere",
    GEOGCS["NAD83",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6269"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9108"]],
        AUTHORITY["EPSG","4269"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-93],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["METERS",1]]"""
    ret = tst.testOpen(check_gt = expected_gt, check_prj = expected_srs)

    if ret == 'success':
        ds = gdal.Open('data/fake_e00grid_compressed.e00')
        line0 = ds.ReadRaster(0,0,5,1)
        ds.ReadRaster(0,1,5,1)
        line2 = ds.ReadRaster(0,2,5,1)
        if line0 == line2:
            gdaltest.post_reason('should not have gotten the same values')
            return 'fail'
        ds.ReadRaster(0,0,5,1)
        line2_bis = ds.ReadRaster(0,2,5,1)
        if line2 != line2_bis:
            gdaltest.post_reason('did not get the same values for the same line')
            return 'fail'

        if ds.GetRasterBand(1).GetMinimum() != 1:
            gdaltest.post_reason('did not get expected minimum value')
            print(ds.GetRasterBand(1).GetMinimum())
            return 'fail'
        if ds.GetRasterBand(1).GetMaximum() != 50:
            gdaltest.post_reason('did not get expected maximum value')
            print(ds.GetRasterBand(1).GetMaximum())
            return 'fail'
        stats = ds.GetRasterBand(1).GetStatistics(False, True)
        if stats != [1.0, 50.0, 25.5, 24.5]:
            gdaltest.post_reason('did not get expected statistics')
            print(stats)
            return 'fail'

    return ret

gdaltest_list = [
    e00grid_1,
    e00grid_2 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'e00grid' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

