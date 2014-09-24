#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test CTG driver
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
# Test a fake CTG dataset

def ctg_1():

    tst = gdaltest.GDALTest( 'CTG', 'fake_grid_cell', 1, 21 )
    expected_gt = [ 421000.0, 200.0, 0.0, 5094400.0, 0.0, -200.0 ]
    expected_srs = """PROJCS["WGS 84 / UTM zone 14N",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4326"]],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-99],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    AUTHORITY["EPSG","32614"],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""
    ret = tst.testOpen(check_gt = expected_gt, check_prj = expected_srs)

    if ret == 'success':
        ds = gdal.Open('data/fake_grid_cell')
        lst = ds.GetRasterBand(1).GetCategoryNames()
        if lst is None or len(lst) == 0:
            gdaltest.post_reason('expected non empty category names for band 1')
            return 'fail'
        lst = ds.GetRasterBand(2).GetCategoryNames()
        if lst is not None:
            gdaltest.post_reason('expected empty category names for band 2')
            return 'fail'
        if ds.GetRasterBand(1).GetNoDataValue() != 0:
            gdaltest.post_reason('did not get expected nodata value')
            return 'fail'

    return ret

gdaltest_list = [
    ctg_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ctg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

