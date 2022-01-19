#!/usr/bin/env pytest
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
# Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdal


import gdaltest
import pytest

###############################################################################
# Perform simple read test.


def test_aaigrid_1():

    tst = gdaltest.GDALTest('aaigrid', 'aaigrid/pixel_per_line.asc', 1, 1123)
    return tst.testOpen()

###############################################################################
# Verify some auxiliary data.


def test_aaigrid_2():

    ds = gdal.Open('data/aaigrid/pixel_per_line.asc')

    gt = ds.GetGeoTransform()

    assert gt[0] == 100000.0 and gt[1] == 50 and gt[2] == 0 and gt[3] == 650600.0 and gt[4] == 0 and gt[5] == -50, \
        'Aaigrid geotransform wrong.'

    prj = ds.GetProjection()
    assert prj == 'PROJCS["unnamed",GEOGCS["NAD83",DATUM["North_American_Datum_1983",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","6269"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4269"]],PROJECTION["Albers_Conic_Equal_Area"],PARAMETER["latitude_of_center",59],PARAMETER["longitude_of_center",-132.5],PARAMETER["standard_parallel_1",61.6666666666667],PARAMETER["standard_parallel_2",68],PARAMETER["false_easting",500000],PARAMETER["false_northing",500000],UNIT["METERS",1],AXIS["Easting",EAST],AXIS["Northing",NORTH]]', \
        ('Projection does not match expected:\n%s' % prj)

    band1 = ds.GetRasterBand(1)
    assert band1.GetNoDataValue() == -99999, 'Grid NODATA value wrong or missing.'

    assert band1.DataType == gdal.GDT_Float32, 'Data type is not Float32!'


###############################################################################
# Test reading a file where decimal separator is comma (#3668)

def test_aaigrid_comma():

    ds = gdal.Open('data/aaigrid/pixel_per_line_comma.asc')

    gt = ds.GetGeoTransform()

    assert gt[0] == 100000.0 and gt[1] == 50 and gt[2] == 0 and gt[3] == 650600.0 and gt[4] == 0 and gt[5] == -50, \
        'Aaigrid geotransform wrong.'

    band1 = ds.GetRasterBand(1)
    assert band1.Checksum() == 1123, 'Did not get expected nodata value.'

    assert band1.GetNoDataValue() == -99999, 'Grid NODATA value wrong or missing.'

    assert band1.DataType == gdal.GDT_Float32, 'Data type is not Float32!'

###############################################################################
# Create simple copy and check.


def test_aaigrid_3():

    tst = gdaltest.GDALTest('AAIGRID', 'byte.tif', 1, 4672)

    prj = 'PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke_1866",6378206.4,294.9786982138982]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1]]'

    return tst.testCreateCopy(check_gt=1, check_srs=prj)

###############################################################################
# Read subwindow.  Tests the tail recursion problem.


def test_aaigrid_4():

    tst = gdaltest.GDALTest('aaigrid', 'aaigrid/pixel_per_line.asc', 1, 187,
                            5, 5, 5, 5)
    return tst.testOpen()

###############################################################################
# Perform simple read test on mixed-case .PRJ filename


def test_aaigrid_5():

    # Mixed-case files pair used in the test:
    # - case_sensitive.ASC
    # - case_sensitive.PRJ

    tst = gdaltest.GDALTest('aaigrid', 'aaigrid/case_sensitive.ASC', 1, 1123)

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

    return tst.testOpen(check_prj=prj)

###############################################################################
# Verify data type determination from type of nodata


def test_aaigrid_6():

    ds = gdal.Open('data/aaigrid/nodata_float.asc')

    b = ds.GetRasterBand(1)
    assert b.GetNoDataValue() == -99999, 'Grid NODATA value wrong or missing.'

    assert b.DataType == gdal.GDT_Float32, 'Data type is not Float32!'

###############################################################################
# Verify data type determination from type of nodata


def test_aaigrid_6bis():

    ds = gdal.Open('data/aaigrid/nodata_int.asc')

    b = ds.GetRasterBand(1)
    assert b.GetNoDataValue() == -99999, 'Grid NODATA value wrong or missing.'

    assert b.DataType == gdal.GDT_Int32, 'Data type is not Int32!'

###############################################################################
# Verify writing files with non-square pixels.


def test_aaigrid_7():

    tst = gdaltest.GDALTest('AAIGRID', 'aaigrid/nonsquare.vrt', 1, 12481)

    return tst.testCreateCopy(check_gt=1)


###############################################################################
# Test creating an in memory copy.

def test_aaigrid_8():

    tst = gdaltest.GDALTest('AAIGRID', 'byte.tif', 1, 4672)

    return tst.testCreateCopy(vsimem=1)


###############################################################################
# Test DECIMAL_PRECISION creation option

def test_aaigrid_9():

    ds = gdal.Open('data/ehdr/float32.bil')
    ds2 = gdal.GetDriverByName('AAIGRID').CreateCopy('tmp/aaigrid.tmp', ds, options=['DECIMAL_PRECISION=2'])
    got_minmax = ds2.GetRasterBand(1).ComputeRasterMinMax()
    ds2 = None

    gdal.GetDriverByName('AAIGRID').Delete('tmp/aaigrid.tmp')

    if got_minmax[0] == pytest.approx(-0.84, abs=1e-7):
        return
    pytest.fail()

###############################################################################
# Test AAIGRID_DATATYPE configuration option and DATATYPE open options


def test_aaigrid_10():

    # By default detected as 32bit float
    ds = gdal.Open('data/aaigrid/float64.asc')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32, 'Data type is not Float32!'

    for i in range(2):

        try:
            os.remove('data/aaigrid/float64.asc.aux.xml')
        except OSError:
            pass

        if i == 0:
            gdal.SetConfigOption('AAIGRID_DATATYPE', 'Float64')
            ds = gdal.Open('data/aaigrid/float64.asc')
            gdal.SetConfigOption('AAIGRID_DATATYPE', None)
        else:
            ds = gdal.OpenEx('data/aaigrid/float64.asc', open_options=['DATATYPE=Float64'])

        assert ds.GetRasterBand(1).DataType == gdal.GDT_Float64, 'Data type is not Float64!'

        nv = ds.GetRasterBand(1).GetNoDataValue()
        assert nv == pytest.approx(-1.234567890123, abs=1e-16), 'did not get expected nodata value'

        got_minmax = ds.GetRasterBand(1).ComputeRasterMinMax()
        assert got_minmax[0] == pytest.approx(1.234567890123, abs=1e-16), \
            'did not get expected min value'
        assert got_minmax[1] == pytest.approx(1.234567890123, abs=1e-16), \
            'did not get expected max value'

        try:
            os.remove('data/aaigrid/float64.asc.aux.xml')
        except OSError:
            pass


###############################################################################
# Test SIGNIFICANT_DIGITS creation option (same as DECIMAL_PRECISION test)


def test_aaigrid_11():

    ds = gdal.Open('data/ehdr/float32.bil')
    ds2 = gdal.GetDriverByName('AAIGRID').CreateCopy('tmp/aaigrid.tmp', ds, options=['SIGNIFICANT_DIGITS=2'])
    got_minmax = ds2.GetRasterBand(1).ComputeRasterMinMax()
    ds2 = None

    gdal.GetDriverByName('AAIGRID').Delete('tmp/aaigrid.tmp')

    if got_minmax[0] == pytest.approx(-0.84, abs=1e-7):
        return
    pytest.fail()

###############################################################################
# Test no data is written to correct precision with DECIMAL_PRECISION.


def test_aaigrid_12():

    ds = gdal.Open('data/aaigrid/nodata_float.asc')
    ds2 = gdal.GetDriverByName('AAIGRID').CreateCopy('tmp/aaigrid.tmp', ds,
                                                     options=['DECIMAL_PRECISION=3'])
    del ds2

    aai = open('tmp/aaigrid.tmp')
    assert aai
    for _ in range(5):
        aai.readline()
    ndv = aai.readline().strip().lower()
    aai.close()
    gdal.GetDriverByName('AAIGRID').Delete('tmp/aaigrid.tmp')
    assert ndv.startswith('nodata_value')
    assert ndv.endswith('-99999.000')

###############################################################################
# Test no data is written to correct precision WITH SIGNIFICANT_DIGITS.


def test_aaigrid_13():

    ds = gdal.Open('data/aaigrid/nodata_float.asc')
    ds2 = gdal.GetDriverByName('AAIGRID').CreateCopy('tmp/aaigrid.tmp', ds,
                                                     options=['SIGNIFICANT_DIGITS=3'])
    del ds2

    aai = open('tmp/aaigrid.tmp')
    assert aai
    for _ in range(5):
        aai.readline()
    ndv = aai.readline().strip().lower()
    aai.close()
    gdal.GetDriverByName('AAIGRID').Delete('tmp/aaigrid.tmp')
    assert ndv.startswith('nodata_value')
    assert ndv.endswith('-1e+05') or ndv.endswith('-1e+005')

###############################################################################
# Test fix for #6060


def test_aaigrid_14():

    ds = gdal.Open('data/byte.tif')
    mem_ds = gdal.GetDriverByName('MEM').Create('', 20, 20, 1, gdal.GDT_Float32)
    mem_ds.GetRasterBand(1).WriteRaster(0, 0, 20, 20, ds.ReadRaster(0, 0, 20, 20, buf_type=gdal.GDT_Float32))
    ds = None
    gdal.GetDriverByName('AAIGRID').CreateCopy('/vsimem/aaigrid_14.asc', mem_ds)

    f = gdal.VSIFOpenL('/vsimem/aaigrid_14.asc', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    gdal.GetDriverByName('AAIGRID').Delete('/vsimem/aaigrid_14.asc')

    assert '107.0 123' in data

###############################################################################
# Test Float64 detection when nodata = DBL_MIN


def test_aaigrid_15():

    gdal.FileFromMemBuffer('/vsimem/aaigrid_15.asc', """ncols        4
nrows        1
xllcorner    0
yllcorner    -1
cellsize     1
NODATA_value  2.2250738585072014e-308
 2.2250738585072014e-308 0 1 2.3e-308
""")

    ds = gdal.Open('/vsimem/aaigrid_15.asc')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float64
    ds = None

    gdal.Unlink('/vsimem/aaigrid_15.asc')


###############################################################################
# Test support for D12 AAIGRID with null value (#5095)


def test_aaigrid_null():

    gdal.FileFromMemBuffer('/vsimem/test_aaigrid_null.asc', """ncols        4
nrows        1
xllcorner    0
yllcorner    -1
cellsize     1
NODATA_value  null
null 1.5 null 3.5
""")

    ds = gdal.Open('/vsimem/test_aaigrid_null.asc')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
    assert ds.GetRasterBand(1).GetNoDataValue() < -1e38
    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (1.5,3.5)
    ds = None

    gdal.Unlink('/vsimem/test_aaigrid_null.asc')


###############################################################################
# Test support for D12 AAIGRID with null value and force AAIGRID_DATATYPE=Float64 (#5095)


def test_aaigrid_null_float64():

    gdal.FileFromMemBuffer('/vsimem/test_aaigrid_null.asc', """ncols        4
nrows        1
xllcorner    0
yllcorner    -1
cellsize     1
NODATA_value  null
null 1.5 null 3.5
""")

    with gdaltest.config_option('AAIGRID_DATATYPE', 'Float64'):
        ds = gdal.Open('/vsimem/test_aaigrid_null.asc')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float64
    assert ds.GetRasterBand(1).GetNoDataValue() < -1e308
    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (1.5,3.5)
    ds = None

    gdal.Unlink('/vsimem/test_aaigrid_null.asc')

