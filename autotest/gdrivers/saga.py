#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SAGA GIS Binary driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import gdalconst

import gdaltest

###############################################################################
# Test opening


def test_saga_1():

    tst = gdaltest.GDALTest('SAGA', 'saga/4byteFloat.sdat', 1, 108)
    return tst.testOpen(check_prj="""PROJCS["NAD_1927_UTM_Zone_11N",
    GEOGCS["GCS_North_American_1927",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke_1866",6378206.4,294.9786982]],
        PRIMEM["Greenwich",0],
        UNIT["Degree",0.017453292519943295]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-117],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["Meter",1]]""")

###############################################################################
# Test copying a reference sample with CreateCopy()


def test_saga_2():

    tst = gdaltest.GDALTest('SAGA', 'saga/4byteFloat.sdat', 1, 108)
    return tst.testCreateCopy(new_filename='tmp/createcopy.sdat', check_srs=True)

###############################################################################
# Test copying a reference sample with Create()


def test_saga_3():

    tst = gdaltest.GDALTest('SAGA', 'saga/4byteFloat.sdat', 1, 108)
    return tst.testCreate(new_filename='tmp/copy.sdat', out_bands=1)

###############################################################################
# Test CreateCopy() for various data types


def test_saga_4():

    src_files = ['byte.tif',
                 'int16.tif',
                 '../../gcore/data/uint16.tif',
                 '../../gcore/data/int32.tif',
                 '../../gcore/data/uint32.tif',
                 '../../gcore/data/float32.tif',
                 '../../gcore/data/float64.tif']

    for src_file in src_files:
        tst = gdaltest.GDALTest('SAGA', src_file, 1, 4672)
        if src_file == 'byte.tif':
            check_minmax = 0
        else:
            check_minmax = 1
        tst.testCreateCopy(new_filename='tmp/test4.sdat', check_minmax=check_minmax)


###############################################################################
# Test Create() for various data types


def test_saga_5():

    src_files = ['byte.tif',
                 'int16.tif',
                 '../../gcore/data/uint16.tif',
                 '../../gcore/data/int32.tif',
                 '../../gcore/data/uint32.tif',
                 '../../gcore/data/float32.tif',
                 '../../gcore/data/float64.tif']

    for src_file in src_files:
        tst = gdaltest.GDALTest('SAGA', src_file, 1, 4672)
        if src_file == 'byte.tif':
            check_minmax = 0
        else:
            check_minmax = 1
        tst.testCreate(new_filename='tmp/test5.sdat', out_bands=1, check_minmax=check_minmax)


###############################################################################
# Test creating empty datasets and check that nodata values are properly written


def test_saga_6():

    gdal_types = [gdal.GDT_Byte,
                  gdal.GDT_Int16,
                  gdal.GDT_UInt16,
                  gdal.GDT_Int32,
                  gdal.GDT_UInt32,
                  gdal.GDT_Float32,
                  gdal.GDT_Float64]

    expected_nodata = [255, -32767, 65535, -2147483647, 4294967295, -99999.0, -99999.0]

    for i, gdal_type in enumerate(gdal_types):

        ds = gdal.GetDriverByName('SAGA').Create('tmp/test6.sdat', 2, 2, 1, gdal_type)
        ds = None

        ds = gdal.Open('tmp/test6.sdat')

        data = ds.GetRasterBand(1).ReadRaster(1, 1, 1, 1, buf_type=gdal.GDT_Float64)

        # Read raw data into tuple of float numbers
        import struct
        value = struct.unpack('d' * 1, data)[0]
        assert value == expected_nodata[i], 'did not get expected pixel value'

        nodata = ds.GetRasterBand(1).GetNoDataValue()
        assert nodata == expected_nodata[i], 'did not get expected nodata value'

        ds = None

    try:
        os.remove('tmp/test6.sgrd')
        os.remove('tmp/test6.sdat')
    except OSError:
        pass


###############################################################################
# Test /vsimem


def test_saga_7():

    tst = gdaltest.GDALTest('SAGA', 'saga/4byteFloat.sdat', 1, 108)
    return tst.testCreateCopy(new_filename='/vsimem/createcopy.sdat')


###############################################################################
# Test zipped saga grid (.sg-grd-z)

def test_saga_8():
    tst = gdaltest.GDALTest('SAGA', 'saga/4byteFloat.sg-grd-z', 1, 108)
    return tst.testOpen(check_prj="""PROJCS["NAD_1927_UTM_Zone_11N",
    GEOGCS["GCS_North_American_1927",
        DATUM["North_American_Datum_1927",
            SPHEROID["Clarke_1866",6378206.4,294.9786982]],
        PRIMEM["Greenwich",0],
        UNIT["Degree",0.017453292519943295]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-117],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["Meter",1]]""")

##############################################################################
# Test setnodata
def test_saga_9():

    gdal_type = gdal.GDT_Float64

    ds = gdal.GetDriverByName('SAGA').Create('tmp/test9.sdat', 2, 2, 1, gdal_type)
    ds = None

    ds = gdal.Open('tmp/test9.sdat')
    with gdaltest.error_handler():
        ret = ds.GetRasterBand(1).SetNoDataValue(56)
    assert ret == gdalconst.CE_Failure
    # make sure nodata value is not changed
    assert ds.GetRasterBand(1).GetNoDataValue() == -99999

    ds = None
    ds = gdal.Open('tmp/test9.sdat', gdal.GA_Update)

    ret = ds.GetRasterBand(1).SetNoDataValue(56)
    assert ret == gdalconst.CE_None
    nd = ds.GetRasterBand(1).GetNoDataValue()
    assert nd == 56

    ds = None

    with open('tmp/test9.sgrd', 'r') as f:
        header_string = f.read()
        assert "NODATA_VALUE\t= 56.000000" in header_string
    try:
        os.remove('tmp/test9.sgrd')
        os.remove('tmp/test9.sdat')
    except OSError:
        pass

