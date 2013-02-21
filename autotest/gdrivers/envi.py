#!/usr/bin/env python
# -*- coding: utf-8 -*-
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

###############################################################################
# Test RPC reading and writing

def envi_10():

    src_ds = gdal.Open('data/envirpc.img')
    out_ds = gdal.GetDriverByName('ENVI').CreateCopy('/vsimem/envirpc.img', src_ds)
    src_ds = None
    out_ds = None

    gdal.Unlink('/vsimem/envirpc.img.aux.xml')

    ds = gdal.Open('/vsimem/envirpc.img')
    md = ds.GetMetadata('RPC')
    ds = None

    gdal.GetDriverByName('ENVI').Delete('/vsimem/envirpc.img')

    if md['HEIGHT_OFF'] != '3355':
        print(md)
        return 'fail'

    return 'success'

###############################################################################
# Check .sta reading

def envi_11():

    ds = gdal.Open('data/envistat')
    val = ds.GetRasterBand(1).GetStatistics(0, 0)
    ds = None

    if val != [1.0, 3.0, 2.0, 0.5]:
        gdaltest.post_reason('bad stats')
        print(val)
        return 'fail'

    return 'success'

###############################################################################
# Test category names reading and writing

def envi_12():

    src_ds = gdal.Open('data/testenviclasses')
    out_ds = gdal.GetDriverByName('ENVI').CreateCopy('/vsimem/testenviclasses', src_ds)
    src_ds = None
    out_ds = None

    gdal.Unlink('/vsimem/testenviclasses.aux.xml')

    ds = gdal.Open('/vsimem/testenviclasses')
    category = ds.GetRasterBand(1).GetCategoryNames()
    ct = ds.GetRasterBand(1).GetColorTable()

    if category != ['Black', 'White']:
        gdaltest.post_reason('bad category names')
        print(category)
        return 'fail'

    if ct.GetCount() != 2:
        gdaltest.post_reason('bad color entry count')
        print(ct.GetCount())
        return 'fail'

    if ct.GetColorEntry(0) != (0,0,0,255):
        gdaltest.post_reason('bad color entry')
        print(ct.GetColorEntry(0))
        return 'fail'

    ds = None
    gdal.GetDriverByName('ENVI').Delete('/vsimem/testenviclasses')

    return 'success'

###############################################################################
# Test writing of metadata from the ENVI metadata domain and read it back (#4957)

def envi_13():

    ds = gdal.GetDriverByName('ENVI').Create('/vsimem/envi_13.dat', 1, 1)
    ds.SetMetadata(['lines=100', 'sensor_type=Landsat TM', 'foo'], 'ENVI')
    ds = None

    gdal.Unlink('/vsimem/envi_13.dat.aux.xml')

    ds = gdal.Open('/vsimem/envi_13.dat')
    lines = ds.RasterYSize
    val = ds.GetMetadataItem('sensor_type', 'ENVI')
    ds = None
    gdal.GetDriverByName('ENVI').Delete('/vsimem/envi_13.dat')
    
    if lines != 1:
        return 'fail'

    if val != 'Landsat TM':
        return 'fail'

    return 'success'

gdaltest_list = [
    envi_1,
    envi_2,
    envi_3,
    envi_4,
    envi_5,
    envi_6,
    envi_7,
    envi_8,
    envi_9,
    envi_10,
    envi_11,
    envi_12,
    envi_13,
    ]
  


if __name__ == '__main__':

    gdaltest.setup_run( 'envi' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

