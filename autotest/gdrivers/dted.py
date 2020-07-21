#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DTED support.
# Author:   Mateusz Loskot <mateusz@loskot.net>
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2007-2012, Even Rouault <even dot rouault at spatialys.com>
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


def test_dted_1():

    tst = gdaltest.GDALTest('dted', 'n43.dt0', 1, 49187)
    return tst.testOpen()

###############################################################################
# Verify some auxiliary data.


def test_dted_2():

    ds = gdal.Open('data/n43.dt0')

    gt = ds.GetGeoTransform()

    max_error = 0.000001

    assert gt[0] == pytest.approx((-80.004166666666663), abs=max_error) and gt[1] == pytest.approx(0.0083333333333333332, abs=max_error) and gt[2] == pytest.approx(0, abs=max_error) and gt[3] == pytest.approx(44.00416666666667, abs=max_error) and gt[4] == pytest.approx(0, abs=max_error) and gt[5] == pytest.approx((-0.0083333333333333332), abs=max_error), \
        'DTED geotransform wrong.'

    prj = ds.GetProjection()
    assert prj == 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]', \
        ('Projection does not match expected:\n%s' % prj)

    band1 = ds.GetRasterBand(1)
    assert band1.GetNoDataValue() == -32767, 'Grid NODATA value wrong or missing.'

    assert band1.DataType == gdal.GDT_Int16, 'Data type is not Int16!'

###############################################################################
# Create simple copy and check.


def test_dted_3():

    tst = gdaltest.GDALTest('DTED', 'n43.dt0', 1, 49187)

    prj = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]'

    return tst.testCreateCopy(check_gt=1, check_srs=prj)

###############################################################################
# Read subwindow.  Tests the tail recursion problem.


def test_dted_4():

    tst = gdaltest.GDALTest('dted', 'n43.dt0', 1, 305,
                            5, 5, 5, 5)
    return tst.testOpen()

###############################################################################
# Test a DTED Level 1 (made from a DTED Level 0)


def test_dted_5():

    driver = gdal.GetDriverByName("GTiff")
    ds = driver.Create('tmp/n43.dt1.tif', 1201, 1201, 1, gdal.GDT_Int16)
    ds.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]')
    ref_geotransform = (-80.0004166666666663, 0.0008333333333333, 0, 44.0004166666666670, 0, -0.0008333333333333)
    ds.SetGeoTransform(ref_geotransform)

    ds = None

    ds = gdal.Open('tmp/n43.dt1.tif')
    geotransform = ds.GetGeoTransform()
    for i in range(6):
        assert geotransform[i] == pytest.approx(ref_geotransform[i], abs=1e-10)

    ds = None

###############################################################################
# Test a DTED Level 2 (made from a DTED Level 0)


def test_dted_6():

    driver = gdal.GetDriverByName("GTiff")
    ds = driver.Create('tmp/n43.dt2.tif', 3601, 3601, 1, gdal.GDT_Int16)
    ds.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]')
    ref_geotransform = (-80.0001388888888888888, 0.0002777777777777777, 0, 44.0001388888888888, 0, -0.0002777777777777777)
    ds.SetGeoTransform(ref_geotransform)

    ds = None

    ds = gdal.Open('tmp/n43.dt2.tif')
    geotransform = ds.GetGeoTransform()
    for i in range(6):
        assert geotransform[i] == pytest.approx(ref_geotransform[i], abs=1e-10)

    ds = None

###############################################################################
# Test a WGS72 georeferenced DTED


def test_dted_7():
    ds = gdal.Open('data/dted/n43_wgs72.dt0')

    # a warning is issued
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    prj = ds.GetProjection()
    gdal.PopErrorHandler()

    assert gdal.GetLastErrorMsg() is not None, 'An expected warning was not emitted'

    assert prj.startswith('GEOGCS["WGS 72"')

###############################################################################
# Test a file whose checksum is corrupted


def test_dted_8():
    # this will enable DTED_VERIFY_CHECKSUM
    gdal.SetConfigOption('DTED_VERIFY_CHECKSUM', 'YES')

    ds = gdal.Open('data/dted/n43_bad_crc.dt0')
    band = ds.GetRasterBand(1)

    # numerous errors would be reported
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    chksum = band.Checksum()
    gdal.PopErrorHandler()

    gdal.SetConfigOption('DTED_VERIFY_CHECKSUM', 'NO')

    assert gdal.GetLastErrorMsg() is not None, 'An expected warning was not emitted'

    # 49187 is the checksum of data is the DTED is read without checking its checksum
    # so we should not get this value
    assert chksum != 49187, 'DTED_VERIFY_CHECKSUM=YES has had no effect!'

###############################################################################
# Test a DTED Level 1 above latitude 50 (made from a DTED Level 0)


def test_dted_9():

    ds = gdal.Open('data/n43.dt0')

    bandSrc = ds.GetRasterBand(1)

    driver = gdal.GetDriverByName("GTiff")
    dsDst = driver.Create('tmp/n53.dt1.tif', 601, 1201, 1, gdal.GDT_Int16)
    dsDst.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]')
    dsDst.SetGeoTransform((-80.0008333333333333, 0.001666666666667, 0, 54.0004166666666670, 0, -0.0008333333333333))

    bandDst = dsDst.GetRasterBand(1)

    data = bandSrc.ReadRaster(0, 0, 121, 121, 601, 1201, gdal.GDT_Int16)
    bandDst.WriteRaster(0, 0, 601, 1201, data, 601, 1201, gdal.GDT_Int16)

    bandDst.FlushCache()

    bandDst = None
    ds = None
    dsDst = None

    ds = gdal.Open('tmp/n53.dt1.tif')
    driver = gdal.GetDriverByName("DTED")
    dsDst = driver.CreateCopy('tmp/n53.dt1', ds)

    band = dsDst.GetRasterBand(1)
    chksum = band.Checksum()

    assert chksum == 36542, ('Wrong checksum. Checksum found %d' % chksum)

###############################################################################
# Test creating an in memory copy.


def test_dted_10():

    tst = gdaltest.GDALTest('dted', 'n43.dt0', 1, 49187)
    return tst.testCreateCopy(vsimem=1)


###############################################################################
# Test a DTED file that strictly the original edition of MIL-D-89020 that was
# buggy.  The latitude and longitude of the LL corner of the UHF record was
# inverted.  This was fixed in MIL-D-89020 Amendment 1, but some products may
# be affected.

def test_dted_11():

    ds = gdal.Open('data/dted/n43_coord_inverted.dt0')

    gt = ds.GetGeoTransform()

    max_error = 0.000001

    assert gt[0] == pytest.approx((-80.004166666666663), abs=max_error) and gt[1] == pytest.approx(0.0083333333333333332, abs=max_error) and gt[2] == pytest.approx(0, abs=max_error) and gt[3] == pytest.approx(44.00416666666667, abs=max_error) and gt[4] == pytest.approx(0, abs=max_error) and gt[5] == pytest.approx((-0.0083333333333333332), abs=max_error), \
        'DTED geotransform wrong.'

    prj = ds.GetProjection()
    assert prj == 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]', \
        ('Projection does not match expected:\n%s' % prj)

    band1 = ds.GetRasterBand(1)
    assert band1.GetNoDataValue() == -32767, 'Grid NODATA value wrong or missing.'

    assert band1.DataType == gdal.GDT_Int16, 'Data type is not Int16!'

###############################################################################
# Test a DTED file that begins with a HDR record, and not directly the UHL record (#2951)


def test_dted_12():

    ds = gdal.Open('data/dted/w118n033_trunc.dt1')
    assert ds is not None

###############################################################################
# Test a DTED file that has only a few (sequential) columns. Derived from
# a real-world DTED file


def test_dted_13():

    tst = gdaltest.GDALTest('dted', 'dted/n43_partial_cols.dt0', 1, 56006)
    return tst.testOpen()

###############################################################################
# Test a DTED file that has only a few (non-sequential) columns. Only theoretical
# case for now.


def test_dted_14():

    tst = gdaltest.GDALTest('dted', 'dted/n43_sparse_cols.dt0', 1, 56369)
    return tst.testOpen()

###############################################################################
# Perform simple read test with GDAL_DTED_SINGLE_BLOCK = YES


def test_dted_15():

    gdal.SetConfigOption('GDAL_DTED_SINGLE_BLOCK', 'YES')
    tst = gdaltest.GDALTest('dted', 'n43.dt0', 1, 49187)
    ret = tst.testOpen()
    gdal.SetConfigOption('GDAL_DTED_SINGLE_BLOCK', None)
    return ret


def test_dted_16():

    with gdaltest.config_option('DTED_APPLY_PIXEL_IS_POINT', 'TRUE'):
        ds = gdal.Open('data/n43.dt0')
        assert ds is not None

        max_error = 0.000001
        gt = ds.GetGeoTransform()
        assert gt == pytest.approx((-80.0, 0.0083333333333333332, 0, 44.0, 0, -0.0083333333333333332), abs=max_error)


###############################################################################
# Cleanup.


def test_dted_cleanup():
    try:
        os.remove('tmp/n43.dt1.tif')
        os.remove('tmp/n43.dt1.aux.xml')
        os.remove('tmp/n43.dt1')
        os.remove('tmp/n53.dt1.tif')
        os.remove('tmp/n53.dt1.aux.xml')
        os.remove('tmp/n53.dt1')
        os.remove('tmp/n43.dt2.tif')
        os.remove('tmp/n43.dt2.aux.xml')
        os.remove('tmp/n43.dt2')
    except OSError:
        pass




