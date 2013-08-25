#!/usr/bin/env python
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
    if prj != 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]':
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

###############################################################################
# Test a DTED Level 1 (made from a DTED Level 0)

def dted_5():

    driver = gdal.GetDriverByName( "GTiff" );
    ds = driver.Create( 'tmp/n43.dt1.tif', 1201, 1201, 1, gdal.GDT_Int16 )
    ds.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]')
    ref_geotransform = (-80.0004166666666663, 0.0008333333333333, 0, 44.0004166666666670, 0, -0.0008333333333333)
    ds.SetGeoTransform(ref_geotransform)

    ds = None

    ds = gdal.Open( 'tmp/n43.dt1.tif' )
    geotransform = ds.GetGeoTransform()
    for i in range(6):
        if abs(geotransform[i] - ref_geotransform[i]) > 1e-10:
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test a DTED Level 2 (made from a DTED Level 0)

def dted_6():

    driver = gdal.GetDriverByName( "GTiff" );
    ds = driver.Create( 'tmp/n43.dt2.tif', 3601, 3601, 1, gdal.GDT_Int16 )
    ds.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]')
    ref_geotransform = (-80.0001388888888888888, 0.0002777777777777777, 0, 44.0001388888888888, 0, -0.0002777777777777777)
    ds.SetGeoTransform(ref_geotransform)

    ds = None

    ds = gdal.Open( 'tmp/n43.dt2.tif' )
    geotransform = ds.GetGeoTransform()
    for i in range(6):
        if abs(geotransform[i] - ref_geotransform[i]) > 1e-10:
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test a WGS72 georeferenced DTED

def dted_7():
    ds = gdal.Open( 'data/n43_wgs72.dt0' )

    # a warning is issued
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    prj = ds.GetProjection()
    gdal.PopErrorHandler()

    if gdal.GetLastErrorMsg() is None:
        gdaltest.post_reason( 'An expected warning was not emitted' )
        return 'fail'

    if prj != 'GEOGCS["WGS 72",DATUM["WGS_1972",SPHEROID["WGS 72",6378135,298.26]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4322"]]':
        gdaltest.post_reason( 'Projection does not match expected:\n%s' % prj )
        return 'fail'

    return 'success'

###############################################################################
# Test a file whose checksum is corrupted

def dted_8():
    # this will enable DTED_VERIFY_CHECKSUM
    gdal.SetConfigOption('DTED_VERIFY_CHECKSUM', 'YES')

    ds = gdal.Open( 'data/n43_bad_crc.dt0' )
    band = ds.GetRasterBand(1)

    # numerous errors would be reported 
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    chksum = band.Checksum()
    gdal.PopErrorHandler()

    gdal.SetConfigOption('DTED_VERIFY_CHECKSUM', 'NO')

    if gdal.GetLastErrorMsg() is None:
        gdaltest.post_reason( 'An expected warning was not emitted' )
        return 'fail'

    # 49187 is the checksum of data is the DTED is read without checking its checksum
    # so we should not get this value
    if chksum == 49187:
        gdaltest.post_reason('DTED_VERIFY_CHECKSUM=YES has had no effect!')
        return 'fail'

    return 'success'

###############################################################################
# Test a DTED Level 1 above latitude 50 (made from a DTED Level 0)

def dted_9():

    ds = gdal.Open( 'data/n43.dt0' )
    
    bandSrc = ds.GetRasterBand(1)
    
    driver = gdal.GetDriverByName( "GTiff" );
    dsDst = driver.Create( 'tmp/n53.dt1.tif', 601, 1201, 1, gdal.GDT_Int16 )
    dsDst.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]')
    dsDst.SetGeoTransform((-80.0008333333333333, 0.001666666666667, 0, 54.0004166666666670, 0, -0.0008333333333333))

    bandDst = dsDst.GetRasterBand(1)
    
    data = bandSrc.ReadRaster( 0, 0, 121, 121, 601, 1201, gdal.GDT_Int16 )
    bandDst.WriteRaster( 0, 0, 601, 1201, data, 601, 1201, gdal.GDT_Int16 )

    bandDst.FlushCache()

    bandDst = None
    ds = None
    dsDst = None

    ds = gdal.Open( 'tmp/n53.dt1.tif' )
    driver = gdal.GetDriverByName( "DTED" );
    dsDst = driver.CreateCopy( 'tmp/n53.dt1', ds)

    band = dsDst.GetRasterBand(1)
    chksum = band.Checksum()

    if chksum != 36542:
        gdaltest.post_reason('Wrong checksum. Checksum found %d' % chksum)
        return 'fail'

    return 'success'

###############################################################################
# Test creating an in memory copy.

def dted_10():

    tst = gdaltest.GDALTest( 'dted', 'n43.dt0', 1, 49187)
    return tst.testCreateCopy( vsimem = 1 )


###############################################################################
# Test a DTED file that strictly the original edition of MIL-D-89020 that was buggy.
# The latitude and longitude of the LL cornder of the UHF record was inverted.
# This was fixed in MIL-D-89020 Amendement 1, but some products may be affected.

def dted_11():

    ds = gdal.Open( 'data/n43_coord_inverted.dt0' )

    gt = ds.GetGeoTransform()

    max_error = 0.000001

    if abs(gt[0] - (-80.004166666666663)) > max_error or abs(gt[1] - 0.0083333333333333332) > max_error \
        or abs(gt[2] - 0) > max_error or abs(gt[3] - 44.00416666666667) > max_error \
        or abs(gt[4] - 0) > max_error or abs(gt[5] - (-0.0083333333333333332)) > max_error:
        gdaltest.post_reason( 'DTED geotransform wrong.' )
        return 'fail'

    prj = ds.GetProjection()
    if prj != 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]':
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
# Test a DTED file that begins with a HDR record, and not directly the UHL record (#2951)

def dted_12():

    ds = gdal.Open( 'data/w118n033_trunc.dt1' )
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Test a DTED file that has only a few (sequential) columns. Derived from
# a real-world DTED file

def dted_13():

    tst = gdaltest.GDALTest( 'dted', 'n43_partial_cols.dt0', 1, 56006)
    return tst.testOpen()

###############################################################################
# Test a DTED file that has only a few (non-sequential) columns. Only theoretical
# case for now.

def dted_14():

    tst = gdaltest.GDALTest( 'dted', 'n43_sparse_cols.dt0', 1, 56369)
    return tst.testOpen()

###############################################################################
# Perform simple read test with GDAL_DTED_SINGLE_BLOCK = YES

def dted_15():

    gdal.SetConfigOption('GDAL_DTED_SINGLE_BLOCK', 'YES')
    tst = gdaltest.GDALTest( 'dted', 'n43.dt0', 1, 49187)
    ret = tst.testOpen()
    gdal.SetConfigOption('GDAL_DTED_SINGLE_BLOCK', None)
    return ret

###############################################################################
# Cleanup.

def dted_cleanup():
    try:
        os.remove( 'tmp/n43.dt1.tif' )
        os.remove( 'tmp/n43.dt1.aux.xml' )
        os.remove( 'tmp/n43.dt1' )
        os.remove( 'tmp/n53.dt1.tif' )
        os.remove( 'tmp/n53.dt1.aux.xml' )
        os.remove( 'tmp/n53.dt1' )
        os.remove( 'tmp/n43.dt2.tif' )
        os.remove( 'tmp/n43.dt2.aux.xml' )
        os.remove( 'tmp/n43.dt2' )
    except:
        pass
    return 'success'

gdaltest_list = [
    dted_1,
    dted_2,
    dted_3,
    dted_4,
    dted_5,
    dted_6,
    dted_7,
    dted_8,
    dted_9,
    dted_10,
    dted_11,
    dted_12,
    dted_13,
    dted_14,
    dted_15,
    dted_cleanup
    ]
  


if __name__ == '__main__':
    
    gdaltest.setup_run( 'dted' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

