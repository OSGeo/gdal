#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ENVI format driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
# See also: gcore/envi_read.py for a driver focused on raster data types.
# 
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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
# Perform simple read test.

def envi_1():

    tst = gdaltest.GDALTest( 'envi', 'aea.dat', 1, 14823 )

    prj = """PROJCS["unnamed",
    GEOGCS["Ellipse Based",
        DATUM["Ellipse Based",
            SPHEROID["Unnamed",6378206.4,294.9786982139109]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Albers_Conic_Equal_Area"],
    PARAMETER["standard_parallel_1",29.5],
    PARAMETER["standard_parallel_2",45.5],
    PARAMETER["latitude_of_center",23],
    PARAMETER["longitude_of_center",-96],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["Meter",1]]"""
    
    return tst.testOpen( check_prj = prj,
                         check_gt = (-936408.178, 28.5, 0.0,
                                     2423902.344, 0.0, -28.5) )

###############################################################################
# Verify this can be exported losslessly.

def envi_2():

    tst = gdaltest.GDALTest( 'envi', 'aea.dat', 1, 14823 )
    return tst.testCreateCopy( check_gt = 1 )
    
###############################################################################
# Try the Create interface with an RGB image. 

def envi_3():

    tst = gdaltest.GDALTest( 'envi', 'rgbsmall.tif', 2, 21053 )
    return tst.testCreate()
    
###############################################################################
# Test LCC Projection.

def envi_4():

    tst = gdaltest.GDALTest( 'envi', 'aea.dat', 1, 24 )

    prj = """PROJCS["unnamed",
    GEOGCS["NAD83",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 1980",6378137,298.257222101],
            TOWGS84[0,0,0,0,0,0,0]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",33.90363402775256],
    PARAMETER["standard_parallel_2",33.62529002776137],
    PARAMETER["latitude_of_origin",33.76446202775696],
    PARAMETER["central_meridian",-117.4745428888127],
    PARAMETER["false_easting",20000],
    PARAMETER["false_northing",30000]]"""

    return tst.testSetProjection( prj = prj )

###############################################################################
# Test TM Projection.

def envi_5():

    tst = gdaltest.GDALTest( 'envi', 'aea.dat', 1, 24 )
    prj = """PROJCS["OSGB 1936 / British National Grid",
    GEOGCS["OSGB 1936",
        DATUM["OSGB_1936",
            SPHEROID["Airy 1830",6377563.396,299.3249646,
                AUTHORITY["EPSG","7001"]],
            TOWGS84[446.448,-125.157,542.06,0.15,0.247,0.842,-20.489],
            AUTHORITY["EPSG","6277"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.01745329251994328,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4277"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",49],
    PARAMETER["central_meridian",-2],
    PARAMETER["scale_factor",0.9996012717],
    PARAMETER["false_easting",400000],
    PARAMETER["false_northing",-100000],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AUTHORITY["EPSG","27700"]]"""

    # now it goes through ESRI WKT processing.
    expected_prj = """PROJCS["OSGB_1936_British_National_Grid",
    GEOGCS["GCS_OSGB 1936",
        DATUM["OSGB_1936",
            SPHEROID["Airy_1830",6377563.396,299.3249646]],
        PRIMEM["Greenwich",0],
        UNIT["Degree",0.017453292519943295]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",49],
    PARAMETER["central_meridian",-2],
    PARAMETER["scale_factor",0.9996012717],
    PARAMETER["false_easting",400000],
    PARAMETER["false_northing",-100000],
    UNIT["Meter",1]]"""

    return tst.testSetProjection( prj = prj, expected_prj = expected_prj )

###############################################################################
# Test LAEA Projection.

def envi_6():

    gdaltest.envi_tst = gdaltest.GDALTest( 'envi', 'aea.dat', 1, 24 )

    prj = """PROJCS["unnamed",
    GEOGCS["Sphere",
        DATUM["Ellipse Based",
            SPHEROID["Sphere",6370997,0]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Azimuthal_Equal_Area"],
    PARAMETER["latitude_of_center",33.76446202775696],
    PARAMETER["longitude_of_center",-117.4745428888127],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0]]"""

    return gdaltest.envi_tst.testSetProjection( prj = prj )

###############################################################################
# Verify VSIF*L capacity

def envi_7():

    tst = gdaltest.GDALTest( 'envi', 'aea.dat', 1, 14823 )
    return tst.testCreateCopy( check_gt = 1, vsimem = 1 )

###############################################################################
# Test fix for #3751

def envi_8():

    ds = gdal.GetDriverByName('ENVI').Create('/vsimem/foo.bsq', 10, 10, 1)
    set_gt = (50000, 1, 0, 4500000, 0, -1)
    ds.SetGeoTransform(set_gt)
    got_gt = ds.GetGeoTransform()
    if set_gt != got_gt:
        gdaltest.post_reason('did not get expected geotransform')
        print(got_gt)
        return 'fail'
    ds = None

    gdal.GetDriverByName('ENVI').Delete('/vsimem/foo.bsq')

    return 'success'

###############################################################################
# Verify reading a compressed file

def envi_9():

    tst = gdaltest.GDALTest( 'envi', 'aea_compressed.dat', 1, 14823 )
    return tst.testCreateCopy( check_gt = 1 )

gdaltest_list = [
    envi_1,
    envi_2,
    envi_3,
    envi_4,
    envi_5,
    envi_6,
    envi_7,
    envi_8,
    envi_9
    ]
  


if __name__ == '__main__':

    gdaltest.setup_run( 'envi' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

