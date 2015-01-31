#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ACE2 driver
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

import sys
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test a fake ACE2 dataset

def ace2_1():

    f = gdal.VSIFOpenL('/vsimem/45N015E_5M.ACE2', 'wb')
    gdal.VSIFSeekL(f, 180*180*4-1, 0)
    gdal.VSIFWriteL('\0', 1, 1, f)
    gdal.VSIFCloseL(f)

    tst = gdaltest.GDALTest( 'ACE2', '/vsimem/45N015E_5M.ACE2', 1, 0, filename_absolute = 1 )
    expected_gt = [ 15.0, 0.08333333333333333, 0.0, 60.0, 0.0, -0.08333333333333333 ]
    expected_srs = """GEOGCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        TOWGS84[0,0,0,0,0,0,0],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9108"]],
    AUTHORITY["EPSG","4326"]]"""
    ret = tst.testOpen(check_gt = expected_gt, check_prj = expected_srs)

    gdal.Unlink('/vsimem/45N015E_5M.ACE2')

    return ret

gdaltest_list = [
    ace2_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ace2' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

