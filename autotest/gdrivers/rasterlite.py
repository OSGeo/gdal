#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for Rasterlite driver.
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import ogr


import gdaltest
import pytest

###############################################################################
# Get the rasterlite driver


def test_rasterlite_1():

    gdaltest.rasterlite_drv = gdal.GetDriverByName('RASTERLITE')
    gdaltest.epsilon_drv = gdal.GetDriverByName('EPSILON')

    # This is to speed-up the runtime of tests on EXT4 filesystems
    # Do not use this for production environment if you care about data safety
    # w.r.t system/OS crashes, unless you know what you are doing.
    gdal.SetConfigOption('OGR_SQLITE_SYNCHRONOUS', 'OFF')

###############################################################################
# Test opening a rasterlite DB without overviews


def test_rasterlite_2():

    if gdaltest.rasterlite_drv is None:
        pytest.skip()

    # Test if SQLite3 supports rtrees
    try:
        os.remove('tmp/testrtree.sqlite')
    except OSError:
        pass
    ds2 = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/testrtree.sqlite')
    gdal.ErrorReset()
    ds2.ExecuteSQL('CREATE VIRTUAL TABLE testrtree USING rtree(id,minX,maxX,minY,maxY)')
    ds2.Destroy()
    try:
        os.remove('tmp/testrtree.sqlite')
    except OSError:
        pass
    if gdal.GetLastErrorMsg().find('rtree') != -1:
        gdaltest.rasterlite_drv = None
        pytest.skip('Please upgrade your sqlite3 library to be able to read Rasterlite DBs (needs rtree support)!')

    gdal.ErrorReset()
    ds = gdal.Open('data/rasterlite/rasterlite.sqlite')
    if ds is None:
        if gdal.GetLastErrorMsg().find('unsupported file format') != -1:
            gdaltest.rasterlite_drv = None
            pytest.skip('Please upgrade your sqlite3 library to be able to read Rasterlite DBs!')
        pytest.fail()

    assert ds.RasterCount == 3, 'expected 3 bands'

    assert ds.GetRasterBand(1).GetOverviewCount() == 0, 'did not expect overview'

    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 11746
    assert cs == expected_cs or cs == 11751, \
        ('for band 1, cs = %d, different from expected_cs = %d' % (cs, expected_cs))

    cs = ds.GetRasterBand(2).Checksum()
    expected_cs = 19843
    assert cs == expected_cs or cs == 20088 or cs == 20083, \
        ('for band 2, cs = %d, different from expected_cs = %d' % (cs, expected_cs))

    cs = ds.GetRasterBand(3).Checksum()
    expected_cs = 48911
    assert cs == expected_cs or cs == 47978, \
        ('for band 3, cs = %d, different from expected_cs = %d' % (cs, expected_cs))

    assert ds.GetProjectionRef().find('WGS_1984') != -1, \
        ('projection_ref = %s' % ds.GetProjectionRef())

    gt = ds.GetGeoTransform()
    expected_gt = (-180.0, 360. / ds.RasterXSize, 0.0, 90.0, 0.0, -180. / ds.RasterYSize)
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1e-15)

    ds = None

###############################################################################
# Test opening a rasterlite DB with overviews


def test_rasterlite_3():

    if gdaltest.rasterlite_drv is None:
        pytest.skip()

    ds = gdal.Open('RASTERLITE:data/rasterlite/rasterlite_pyramids.sqlite,table=test')

    assert ds.RasterCount == 3, 'expected 3 bands'

    assert ds.GetRasterBand(1).GetOverviewCount() == 1, 'expected 1 overview'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 59551
    assert cs == expected_cs or cs == 59833, \
        ('for overview of band 1, cs = %d, different from expected_cs = %d' % (cs, expected_cs))

    cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
    expected_cs = 59603
    assert cs == expected_cs or cs == 59588, \
        ('for overview of band 2, cs = %d, different from expected_cs = %d' % (cs, expected_cs))

    cs = ds.GetRasterBand(3).GetOverview(0).Checksum()
    expected_cs = 42173
    assert cs == expected_cs or cs == 42361, \
        ('for overview of band 3, cs = %d, different from expected_cs = %d' % (cs, expected_cs))

    ds = None

###############################################################################
# Test opening a rasterlite DB with color table and user-defined spatial extent


def test_rasterlite_4():

    if gdaltest.rasterlite_drv is None:
        pytest.skip()

    ds = gdal.Open('RASTERLITE:data/rasterlite/rasterlite_pct.sqlite,minx=0,miny=0,maxx=180,maxy=90')

    assert ds.RasterCount == 1, 'expected 1 band'

    assert ds.RasterXSize == 169 and ds.RasterYSize == 85

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    assert ct is not None, 'did not get color table'

    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 36473
    assert cs == expected_cs, \
        ('for band 1, cs = %d, different from expected_cs = %d' % (cs, expected_cs))

    ds = None

###############################################################################
# Test opening a rasterlite DB with color table and do color table expansion


def test_rasterlite_5():

    if gdaltest.rasterlite_drv is None:
        pytest.skip()

    ds = gdal.Open('RASTERLITE:data/rasterlite/rasterlite_pct.sqlite,bands=3')

    assert ds.RasterCount == 3, 'expected 3 bands'

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    assert ct is None, 'did not expect color table'

    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 506
    assert cs == expected_cs, \
        ('for band 1, cs = %d, different from expected_cs = %d' % (cs, expected_cs))

    cs = ds.GetRasterBand(2).Checksum()
    expected_cs = 3842
    assert cs == expected_cs, \
        ('for band 2, cs = %d, different from expected_cs = %d' % (cs, expected_cs))

    cs = ds.GetRasterBand(3).Checksum()
    expected_cs = 59282
    assert cs == expected_cs, \
        ('for band 3, cs = %d, different from expected_cs = %d' % (cs, expected_cs))

    ds = None

###############################################################################
# Test CreateCopy()


def test_rasterlite_6():

    if gdaltest.rasterlite_drv is None:
        pytest.skip()

    # Test first if spatialite is available
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ogr_ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/spatialite_test.db', options=['SPATIALITE=YES'])
    if ogr_ds is not None:
        sql_lyr = ogr_ds.ExecuteSQL("SELECT AsText(GeomFromText('POINT(0 1)'))")
    else:
        sql_lyr = None
    gdal.PopErrorHandler()
    if sql_lyr is None:
        gdaltest.has_spatialite = False
        ogr_ds = None
        pytest.skip()

    gdaltest.has_spatialite = True
    ogr_ds.ReleaseResultSet(sql_lyr)
    ogr_ds.Destroy()

    # Test now CreateCopy()
    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('RASTERLITE').CreateCopy('RASTERLITE:tmp/byte.sqlite,table=byte', src_ds)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum(), \
        'Wrong checksum'

    gt = ds.GetGeoTransform()
    expected_gt = src_ds.GetGeoTransform()
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), \
            ('Expected : %s\nGot : %s' % (expected_gt, gt))

    assert ds.GetProjectionRef().find('NAD27 / UTM zone 11N') != -1, 'Wrong SRS'

    src_ds = None
    ds = None

###############################################################################
# Test BuildOverviews()


def test_rasterlite_7():

    if gdaltest.rasterlite_drv is None:
        pytest.skip()

    if gdaltest.has_spatialite is False:
        pytest.skip()

    ds = gdal.Open('tmp/byte.sqlite', gdal.GA_Update)

    # Resampling method is not taken into account
    ds.BuildOverviews('NEAREST', overviewlist=[2, 4])

    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1192, \
        'Wrong checksum for overview 0'

    assert ds.GetRasterBand(1).GetOverview(1).Checksum() == 233, \
        'Wrong checksum for overview 1'

    # Reopen and test
    ds = None
    ds = gdal.Open('tmp/byte.sqlite')

    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1192, \
        'Wrong checksum for overview 0'

    assert ds.GetRasterBand(1).GetOverview(1).Checksum() == 233, \
        'Wrong checksum for overview 1'

###############################################################################
# Test CleanOverviews()


def test_rasterlite_8():

    if gdaltest.rasterlite_drv is None:
        pytest.skip()

    if gdaltest.has_spatialite is False:
        pytest.skip()

    ds = gdal.Open('tmp/byte.sqlite', gdal.GA_Update)

    ds.BuildOverviews(overviewlist=[])

    assert ds.GetRasterBand(1).GetOverviewCount() == 0

###############################################################################
# Create a rasterlite dataset with EPSILON tiles


def test_rasterlite_9():

    if gdaltest.rasterlite_drv is None:
        pytest.skip()

    if gdaltest.has_spatialite is False:
        pytest.skip()

    if gdaltest.epsilon_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('RASTERLITE', 'byte.tif', 1, 4866, options=['DRIVER=EPSILON'])

    return tst.testCreateCopy(check_gt=1, check_srs=1, check_minmax=0)

###############################################################################
# Create a rasterlite dataset with EPSILON tiles


def test_rasterlite_10():

    if gdaltest.rasterlite_drv is None:
        pytest.skip()

    if gdaltest.has_spatialite is False:
        pytest.skip()

    if gdaltest.epsilon_drv is None:
        pytest.skip()

    tst = gdaltest.GDALTest('RASTERLITE', 'rgbsmall.tif', 1, 23189, options=['DRIVER=EPSILON'])

    return tst.testCreateCopy(check_gt=1, check_srs=1, check_minmax=0)

###############################################################################
# Test BuildOverviews() with AVERAGE resampling


def test_rasterlite_11():

    if gdaltest.rasterlite_drv is None:
        pytest.skip()

    if gdaltest.has_spatialite is False:
        pytest.skip()

    ds = gdal.Open('tmp/byte.sqlite', gdal.GA_Update)

    ds.BuildOverviews(overviewlist=[])

    # Resampling method is not taken into account
    ds.BuildOverviews('AVERAGE', overviewlist=[2, 4])

    # Reopen and test
    ds = None
    ds = gdal.Open('tmp/byte.sqlite')

    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1152, \
        'Wrong checksum for overview 0'

    assert ds.GetRasterBand(1).GetOverview(1).Checksum() == 215, \
        'Wrong checksum for overview 1'

###############################################################################
# Test opening a .rasterlite file


def test_rasterlite_12():

    if gdaltest.rasterlite_drv is None:
        pytest.skip()

    if gdaltest.has_spatialite is False:
        pytest.skip()

    ds = gdal.Open('data/rasterlite/byte.rasterlite')
    assert ds.GetRasterBand(1).Checksum() == 4672, 'validation failed'

###############################################################################
# Test opening a .rasterlite.sql file


def test_rasterlite_13():

    if gdaltest.rasterlite_drv is None:
        pytest.skip()

    if gdaltest.has_spatialite is False:
        pytest.skip()

    if gdaltest.rasterlite_drv.GetMetadataItem("ENABLE_SQL_SQLITE_FORMAT") != 'YES':
        pytest.skip()

    ds = gdal.Open('data/rasterlite/byte.rasterlite.sql')
    assert ds.GetRasterBand(1).Checksum() == 4672, 'validation failed'

###############################################################################
# Cleanup


def test_rasterlite_cleanup():

    if gdaltest.rasterlite_drv is None:
        pytest.skip()

    try:
        os.remove('tmp/spatialite_test.db')
    except OSError:
        pass

    try:
        os.remove('tmp/byte.sqlite')
    except OSError:
        pass

    try:
        os.remove('tmp/byte.tif.tst')
    except OSError:
        pass

    try:
        os.remove('tmp/rgbsmall.tif.tst')
    except OSError:
        pass

    


