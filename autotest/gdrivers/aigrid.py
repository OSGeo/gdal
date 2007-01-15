#!/usr/bin/env python
###############################################################################
# $Id: aigrid.py,v 1.3 2006/12/09 06:20:16 dreamil Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for AIGRID driver.
# Author:   Swapnil Hajare <dreamil@gmail.com>
# 
###############################################################################
# Copyright (c) 2006, Swapnil Hajare <dreamil@gmail.com>
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

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Read test of simple byte reference data.

def aigrid_1():

    tst = gdaltest.GDALTest( 'AIG', 'abc3x1', 1, 3 )
    return tst.testOpen()

###############################################################################
# Verify some auxilary data. 

def aigrid_2():

    ds = gdal.Open( 'data/abc3x1/prj.adf' )

    gt = ds.GetGeoTransform()

    if gt[0] != -0.5 or gt[1] != 1.0 or gt[2] != 0.0 \
       or gt[3] != 0.5 or gt[4] != 0.0 or gt[5] != -1.0:
        gdaltest.post_reason( 'Aigrid geotransform wrong.' )
        return 'fail'

    prj = ds.GetProjection()
    if prj != 'PROJCS["UTM Zone 55, Southern Hemisphere",GEOGCS["Unknown datum based upon the GRS 1980 ellipsoid",DATUM["Not_specified_based_on_GRS_1980_ellipsoid",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","6019"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.01745329251994328,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4019"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",147],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",10000000],UNIT["METERS",1]]':
        gdaltest.post_reason( 'Projection does not match expected:\n%s' % prj )
        return 'fail'

    band1 = ds.GetRasterBand(1)
    if band1.GetNoDataValue() != 255:
        gdaltest.post_reason( 'Grid NODATA value wrong or missing.' )
        return 'fail'

    if band1.DataType != gdal.GDT_Byte:
        gdaltest.post_reason( 'Data type is not Byte!' )
        return 'fail'

    return 'success'

###############################################################################
# Verify the colormap, and nodata setting for test file. 

def aigrid_3():

    ds = gdal.Open( 'data/abc3x1' )
    cm = ds.GetRasterBand(1).GetRasterColorTable()
    if cm.GetCount() != 256 \
       or cm.GetColorEntry(0) != (95, 113, 150, 255)\
       or cm.GetColorEntry(1) != (95, 57, 29, 255):
        gdaltest.post_reason( 'Wrong colormap entries' )
        return 'fail'

    cm = None

    if ds.GetRasterBand(1).GetNoDataValue() != 255.0:
        gdaltest.post_reason( 'Wrong nodata value.' )
        return 'fail'

    return 'success'
###############################################################################
# Read test of simple byte reference data with data directory name in all uppercase

def aigrid_4():

    tst = gdaltest.GDALTest( 'AIG', 'ABC3X1UC', 1, 3 )
    return tst.testOpen()
###############################################################################
# Verify the colormap, and nodata setting for test file with names of coverage directory and all files in it in all uppercase. Additionally also test for case where clr file resides in parent directory of coverage.

def aigrid_5():

    ds = gdal.Open( 'data/ABC3X1UC' )
    cm = ds.GetRasterBand(1).GetRasterColorTable()
    if cm.GetCount() != 256 \
       or cm.GetColorEntry(0) != (95, 113, 150, 255)\
       or cm.GetColorEntry(1) != (95, 57, 29, 255):
        gdaltest.post_reason( 'Wrong colormap entries' )
        return 'fail'

    cm = None

    if ds.GetRasterBand(1).GetNoDataValue() != 255.0:
        gdaltest.post_reason( 'Wrong nodata value.' )
        return 'fail'

    return 'success'    
###############################################################################

gdaltest_list = [
    aigrid_1,
    aigrid_2,
    aigrid_3,
    aigrid_4,
    aigrid_5    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'aigrid' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

