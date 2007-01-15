#!/usr/bin/env python
###############################################################################
# $Id: dted.py,v 1.2 2006/11/21 18:06:52 mloskot Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DTED support.
# Author:   Mateusz Loskot <mateusz@loskot.net>
# 
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
# 
#  $Log: dted.py,v $
#  Revision 1.2  2006/11/21 18:06:52  mloskot
#  Fixed geo transform factors comparison.
#
#  Revision 1.1  2006/11/21 17:49:51  mloskot
#  Added dted.py test script to gdalautotest.
#
#

import os
import sys
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Perform simple read test.

def dted_1():

    tst = gdaltest.GDALTest( 'dted', 'n43.dt0', 1, 49187)
    return tst.testOpen()

###############################################################################
# Verify some auxilary data. 

def dted_2():

    ds = gdal.Open( 'data/n43.dt0' )

    gt = ds.GetGeoTransform()

    max_error = 0.000001

    if abs(gt[0] - (-80.004166666666663)) > max_error or abs(gt[1] - 0.0083333333333333332) > max_error \
        or abs(gt[2] - 0) > max_error or abs(gt[3] - 44.00416666666667) > max_error \
        or abs(gt[4] - 0) > max_error or abs(gt[5] - (-0.0083333333333333332)) > max_error:
        gdaltest.post_reason( 'DTED geotransform wrong.' )
        return 'fail'

    prj = ds.GetProjection()
    if prj != 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]':
        gdaltest.post_reason( 'Projection does not match expected:\n%s' % prj )
        return 'fail'

    band1 = ds.GetRasterBand(1)
    if band1.GetNoDataValue() != -32767:
        gdaltest.post_reason( 'Grid NODATA value wrong or missing.' )
        return 'fail'

    if band1.DataType != gdal.GDT_Int16:
        gdaltest.post_reason( 'Data type is not Int16!' )
        return 'fail'

    return 'success'

###############################################################################
# Create simple copy and check.

def dted_3():

    tst = gdaltest.GDALTest( 'DTED', 'n43.dt0', 1, 49187 )

    prj = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]'

    return tst.testCreateCopy( check_gt = 1, check_srs = prj )
    
###############################################################################
# Read subwindow.  Tests the tail recursion problem. 

def dted_4():

    tst = gdaltest.GDALTest( 'dted', 'n43.dt0', 1, 305,
                             5, 5, 5, 5 )
    return tst.testOpen()

gdaltest_list = [
    dted_1,
    dted_2,
    dted_3,
    dted_4
    ]
  


if __name__ == '__main__':

    gdaltest.setup_run( 'dted' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

