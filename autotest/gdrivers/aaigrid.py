#!/usr/bin/env python
###############################################################################
# $Id: aaigrid.py,v 1.3 2006/10/27 04:27:12 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Arc/Info ASCII Grid support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
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
#  $Log: aaigrid.py,v $
#  Revision 1.3  2006/10/27 04:27:12  fwarmerdam
#  fixed license text
#
#  Revision 1.2  2005/02/08 18:15:58  fwarmerdam
#  Added test of subwindow (aka backfill/tail recursion) case.
#
#  Revision 1.1  2005/02/08 18:02:56  fwarmerdam
#  New
#
#

import os
import sys
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Perform simple read test.

def aaigrid_1():

    tst = gdaltest.GDALTest( 'aaigrid', 'pixel_per_line.asc', 1, 1123 )
    return tst.testOpen()

###############################################################################
# Verify some auxilary data. 

def aaigrid_2():

    ds = gdal.Open( 'data/pixel_per_line.asc' )

    gt = ds.GetGeoTransform()

    if gt[0] != 100000.0 or gt[1] != 50 or gt[2] != 0 \
       or gt[3] != 650600.0 or gt[4] != 0 or gt[5] != -50:
        gdaltest.post_reason( 'Aaigrid geotransform wrong.' )
        return 'fail'

    prj = ds.GetProjection()
    if prj != 'PROJCS["unnamed",GEOGCS["NAD83",DATUM["North_American_Datum_1983",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4269"]],PROJECTION["Albers_Conic_Equal_Area"],PARAMETER["standard_parallel_1",61.66666666666666],PARAMETER["standard_parallel_2",68],PARAMETER["latitude_of_center",59],PARAMETER["longitude_of_center",-132.5],PARAMETER["false_easting",500000],PARAMETER["false_northing",500000],UNIT["METERS",1]]':
        gdaltest.post_reason( 'Projection does not match expected:\n%s' % prj )
        return 'fail'

    band1 = ds.GetRasterBand(1)
    if band1.GetNoDataValue() != -99999:
        gdaltest.post_reason( 'Grid NODATA value wrong or missing.' )
        return 'fail'

    if band1.DataType != gdal.GDT_Float32:
        gdaltest.post_reason( 'Data type is not Float32!' )
        return 'fail'

    return 'success'

###############################################################################
# Create simple copy and check.

def aaigrid_3():

    tst = gdaltest.GDALTest( 'AAIGRID', 'byte.tif', 1, 4672 )

    prj = 'PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke_1866",6378206.4,294.9786982138982]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1]]'

    return tst.testCreateCopy( check_gt = 1, check_srs = prj )
    
###############################################################################
# Read subwindow.  Tests the tail recursion problem. 

def aaigrid_4():

    tst = gdaltest.GDALTest( 'aaigrid', 'pixel_per_line.asc', 1, 187,
                             5, 5, 5, 5 )
    return tst.testOpen()

gdaltest_list = [
    aaigrid_1,
    aaigrid_2,
    aaigrid_3,
    aaigrid_4 ]
  


if __name__ == '__main__':

    gdaltest.setup_run( 'aaigrid' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

