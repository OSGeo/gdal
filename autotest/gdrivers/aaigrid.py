#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Arc/Info ASCII Grid support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>
# Copyright (c) 2014, Kyle Shannon <kyle at pobox dot com>
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
# Perform simple read test.

def aaigrid_1():

    tst = gdaltest.GDALTest( 'aaigrid', 'pixel_per_line.asc', 1, 1123 )
    return tst.testOpen()

###############################################################################
# Verify some auxiliary data.

def aaigrid_2():

    ds = gdal.Open( 'data/pixel_per_line.asc' )

    gt = ds.GetGeoTransform()

    if gt[0] != 100000.0 or gt[1] != 50 or gt[2] != 0 \
       or gt[3] != 650600.0 or gt[4] != 0 or gt[5] != -50:
        gdaltest.post_reason( 'Aaigrid geotransform wrong.' )
        return 'fail'

    prj = ds.GetProjection()
    if prj != 'PROJCS["unnamed",GEOGCS["NAD83",DATUM["North_American_Datum_1983",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4269"]],PROJECTION["Albers_Conic_Equal_Area"],PARAMETER["standard_parallel_1",61.66666666666666],PARAMETER["standard_parallel_2",68],PARAMETER["latitude_of_center",59],PARAMETER["longitude_of_center",-132.5],PARAMETER["false_easting",500000],PARAMETER["false_northing",500000],UNIT["METERS",1]]':
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
# Test reading a file where decimal separator is comma (#3668)

def aaigrid_comma():

    ds = gdal.Open( 'data/pixel_per_line_comma.asc' )

    gt = ds.GetGeoTransform()

    if gt[0] != 100000.0 or gt[1] != 50 or gt[2] != 0 \
       or gt[3] != 650600.0 or gt[4] != 0 or gt[5] != -50:
        gdaltest.post_reason( 'Aaigrid geotransform wrong.' )
        return 'fail'

    band1 = ds.GetRasterBand(1)
    if band1.Checksum() != 1123:
        gdaltest.post_reason( 'Did not get expected nodata value.' )
        return 'fail'

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

###############################################################################
# Perform simple read test on mixed-case .PRJ filename

def aaigrid_5():

    # Mixed-case files pair used in the test:
    # - case_sensitive.ASC
    # - case_sensitive.PRJ

    tst = gdaltest.GDALTest( 'aaigrid', 'case_sensitive.ASC', 1, 1123 )

    prj = """PROJCS["unnamed",
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
    PROJECTION["Albers_Conic_Equal_Area"],
    PARAMETER["standard_parallel_1",61.66666666666666],
    PARAMETER["standard_parallel_2",68],
    PARAMETER["latitude_of_center",59],
    PARAMETER["longitude_of_center",-132.5],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",500000],
    UNIT["METERS",1]]
    """

    return tst.testOpen(check_prj = prj)

###############################################################################
# Verify data type determination from type of nodata 

def aaigrid_6():

    ds = gdal.Open( 'data/nodata_float.asc' )

    b = ds.GetRasterBand(1)
    if b.GetNoDataValue() != -99999:
        gdaltest.post_reason( 'Grid NODATA value wrong or missing.' )
        return 'fail'

    if b.DataType != gdal.GDT_Float32:
        gdaltest.post_reason( 'Data type is not Float32!' )
        return 'fail'

    return 'success'

###############################################################################
# Verify data type determination from type of nodata 

def aaigrid_6bis():

    ds = gdal.Open( 'data/nodata_int.asc' )

    b = ds.GetRasterBand(1)
    if b.GetNoDataValue() != -99999:
        gdaltest.post_reason( 'Grid NODATA value wrong or missing.' )
        return 'fail'

    if b.DataType != gdal.GDT_Int32:
        gdaltest.post_reason( 'Data type is not Int32!' )
        return 'fail'

    return 'success'

###############################################################################
# Verify writing files with non-square pixels.

def aaigrid_7():

    tst = gdaltest.GDALTest( 'AAIGRID', 'nonsquare.vrt', 1, 12481 )

    return tst.testCreateCopy( check_gt = 1 )


###############################################################################
# Test creating an in memory copy.

def aaigrid_8():

    tst = gdaltest.GDALTest( 'AAIGRID', 'byte.tif', 1, 4672 )

    return tst.testCreateCopy( vsimem = 1 )


###############################################################################
# Test DECIMAL_PRECISION creation option

def aaigrid_9():

    ds = gdal.Open('data/float32.bil')
    ds2 = gdal.GetDriverByName('AAIGRID').CreateCopy('tmp/aaigrid.tmp', ds, options = ['DECIMAL_PRECISION=2'] )
    got_minmax = ds2.GetRasterBand(1).ComputeRasterMinMax()
    ds2 = None

    gdal.GetDriverByName('AAIGRID').Delete('tmp/aaigrid.tmp')

    if abs(got_minmax[0] - -0.84) < 1e-7:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test AAIGRID_DATATYPE configuration option and DATATYPE open options

def aaigrid_10():

    # By default detected as 32bit float
    ds = gdal.Open('data/float64.asc')
    if ds.GetRasterBand(1).DataType != gdal.GDT_Float32:
        gdaltest.post_reason( 'Data type is not Float32!' )
        return 'fail'

    for i in range(2):

        try:
            os.remove('data/float64.asc.aux.xml')
        except:
            pass

        if i == 0:
            gdal.SetConfigOption('AAIGRID_DATATYPE', 'Float64')
            ds = gdal.Open('data/float64.asc')
            gdal.SetConfigOption('AAIGRID_DATATYPE', None)
        else:
            ds = gdal.OpenEx('data/float64.asc', open_options = ['DATATYPE=Float64'])

        if ds.GetRasterBand(1).DataType != gdal.GDT_Float64:
            gdaltest.post_reason( 'Data type is not Float64!' )
            return 'fail'

        nv = ds.GetRasterBand(1).GetNoDataValue()
        if abs(nv - -1.234567890123) > 1e-16:
            gdaltest.post_reason( 'did not get expected nodata value' )
            return 'fail'

        got_minmax = ds.GetRasterBand(1).ComputeRasterMinMax()
        if abs(got_minmax[0] - 1.234567890123) > 1e-16:
            gdaltest.post_reason( 'did not get expected min value' )
            return 'fail'
        if abs(got_minmax[1] - 1.234567890123) > 1e-16:
            gdaltest.post_reason( 'did not get expected max value' )
            return 'fail'

        try:
            os.remove('data/float64.asc.aux.xml')
        except:
            pass

    return 'success'

###############################################################################
# Test SIGNIFICANT_DIGITS creation option (same as DECIMAL_PRECISION test)

def aaigrid_11():

    ds = gdal.Open('data/float32.bil')
    ds2 = gdal.GetDriverByName('AAIGRID').CreateCopy('tmp/aaigrid.tmp', ds, options = ['SIGNIFICANT_DIGITS=2'] )
    got_minmax = ds2.GetRasterBand(1).ComputeRasterMinMax()
    ds2 = None

    gdal.GetDriverByName('AAIGRID').Delete('tmp/aaigrid.tmp')

    if abs(got_minmax[0] - -0.84) < 1e-7:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test no data is written to correct precision with DECIMAL_PRECISION.

def aaigrid_12():

    ds = gdal.Open('data/nodata_float.asc')
    ds2 = gdal.GetDriverByName('AAIGRID').CreateCopy('tmp/aaigrid.tmp', ds,
                               options = ['DECIMAL_PRECISION=3'] )
    del ds2

    aai = open('tmp/aaigrid.tmp')
    if not aai:
        return 'fail'
    for i in range(5):
        aai.readline()
    ndv = aai.readline().strip().lower()
    aai.close()
    gdal.GetDriverByName('AAIGRID').Delete('tmp/aaigrid.tmp')
    if not ndv.startswith('nodata_value'):
        gdaltest.post_reason('fail')
        print(ndv)
        return 'fail'
    if not ndv.endswith('-99999.000'):
        gdaltest.post_reason('fail')
        print(ndv)
        return 'fail'
    return 'success'

###############################################################################
# Test no data is written to correct precision WITH SIGNIFICANT_DIGITS.

def aaigrid_13():

    ds = gdal.Open('data/nodata_float.asc')
    ds2 = gdal.GetDriverByName('AAIGRID').CreateCopy('tmp/aaigrid.tmp', ds,
                               options = ['SIGNIFICANT_DIGITS=3'] )
    del ds2

    aai = open('tmp/aaigrid.tmp')
    if not aai:
        return 'fail'
    for i in range(5):
        aai.readline()
    ndv = aai.readline().strip().lower()
    aai.close()
    gdal.GetDriverByName('AAIGRID').Delete('tmp/aaigrid.tmp')
    if not ndv.startswith('nodata_value'):
        gdaltest.post_reason('fail')
        print(ndv)
        return 'fail'
    if not ndv.endswith('-1e+05') and not ndv.endswith('-1e+005'):
        gdaltest.post_reason('fail')
        print(ndv)
        return 'fail'
    return 'success'

###############################################################################
# Test fix for #6060

def aaigrid_14():

    ds = gdal.Open('data/byte.tif')
    mem_ds = gdal.GetDriverByName('MEM').Create('', 20, 20, 1, gdal.GDT_Float32)
    mem_ds.GetRasterBand(1).WriteRaster(0, 0, 20, 20, ds.ReadRaster(0, 0, 20, 20, buf_type = gdal.GDT_Float32))
    ds = None
    gdal.GetDriverByName('AAIGRID').CreateCopy('/vsimem/aaigrid_14.asc', mem_ds )

    f = gdal.VSIFOpenL('/vsimem/aaigrid_14.asc', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    gdal.GetDriverByName('AAIGRID').Delete('/vsimem/aaigrid_14.asc')

    if data.find('107.0 123') < 0:
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    return 'success'

###############################################################################

gdaltest_list = [
    aaigrid_1,
    aaigrid_2,
    aaigrid_comma,
    aaigrid_3,
    aaigrid_4,
    aaigrid_5,
    aaigrid_6,
    aaigrid_6bis,
    aaigrid_7,
    aaigrid_8,
    aaigrid_9,
    aaigrid_10,
    aaigrid_11,
    aaigrid_12,
    aaigrid_13,
    aaigrid_14 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'aaigrid' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

