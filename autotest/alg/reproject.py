#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ReprojectImage() algorithm.
# Author:   Even Rouault <even dot rouault at spatialys.com>
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




from osgeo import gdal
from osgeo import osr
import pytest

###############################################################################
# Test a trivial case.


def test_reproject_1():

    drv = gdal.GetDriverByName('GTiff')
    src_ds = gdal.Open('../gcore/data/byte.tif')

    dst_ds = drv.Create('tmp/byte.tif', src_ds.RasterXSize, src_ds.RasterYSize, gdal.GDT_Byte)
    dst_ds.SetProjection(src_ds.GetProjectionRef())
    dst_ds.SetGeoTransform(src_ds.GetGeoTransform())

    gdal.ReprojectImage(src_ds, dst_ds)

    cs_expected = src_ds.GetRasterBand(1).Checksum()
    cs = dst_ds.GetRasterBand(1).Checksum()

    dst_ds = None

    drv.Delete('tmp/byte.tif')

    if cs != cs_expected:
        print('Got: ', cs)
        pytest.fail('got wrong checksum')
    
###############################################################################
# Test a real reprojection case.


def test_reproject_2():

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32611)

    sr2 = osr.SpatialReference()
    sr2.ImportFromEPSG(4326)

    drv = gdal.GetDriverByName('GTiff')
    src_ds = gdal.Open('../gcore/data/byte.tif')

    dst_ds = drv.Create('tmp/byte_4326.tif', 22, 18, gdal.GDT_Byte)
    dst_ds.SetGeoTransform([-117.641169915168746, 0.000598105625684, 0, 33.900668703925191, 0, -0.000598105625684])

    gdal.ReprojectImage(src_ds, dst_ds, sr.ExportToWkt(), sr2.ExportToWkt())

    cs_expected = 4727
    cs = dst_ds.GetRasterBand(1).Checksum()

    dst_ds = None

    drv.Delete('tmp/byte_4326.tif')

    if cs != cs_expected:
        print('Got: ', cs)
        pytest.fail('got wrong checksum')
    
###############################################################################
# Test nodata values


def test_reproject_3():

    data = '\x02\x7f\x7f\x02\x02\x7f\x7f\x02\x02\x7f\x7f\x02'
    src_ds = gdal.GetDriverByName('MEM').Create('', 4, 3)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 4, 3, data)
    src_ds.GetRasterBand(1).SetNoDataValue(2)
    src_ds.SetGeoTransform([10, 1, 0, 10, 0, -1])

    dst_ds = gdal.GetDriverByName('MEM').Create('', 6, 3)
    dst_ds.GetRasterBand(1).SetNoDataValue(3)
    dst_ds.GetRasterBand(1).Fill(3)
    dst_ds.SetGeoTransform([10, 2. / 3., 0, 10, 0, -1])

    gdal.ReprojectImage(src_ds, dst_ds, '', '', gdal.GRA_Bilinear)
    got_data = dst_ds.GetRasterBand(1).ReadRaster(0, 0, 6, 3).decode('latin1')
    expected_data = '\x03\x7f\x7f\x7f\x03\x03\x03\x7f\x7f\x7f\x03\x03\x03\x7f\x7f\x7f\x03\x03'
    if got_data != expected_data:
        import struct
        pytest.fail(struct.unpack('B' * 18, got_data))

    
###############################################################################
# Test warp options


def test_reproject_4():

    data = '\x02\x7f\x7f\x02\x02\x7f\x7f\x02\x02\x7f\x7f\x02'
    src_ds = gdal.GetDriverByName('MEM').Create('', 4, 3)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 4, 3, data)
    src_ds.GetRasterBand(1).SetNoDataValue(2)
    src_ds.SetGeoTransform([10, 1, 0, 10, 0, -1])

    dst_ds = gdal.GetDriverByName('MEM').Create('', 6, 3)
    dst_ds.GetRasterBand(1).SetNoDataValue(3)
    dst_ds.SetGeoTransform([10, 2. / 3., 0, 10, 0, -1])

    gdal.ReprojectImage(src_ds, dst_ds, '', '', gdal.GRA_Bilinear, options=['INIT_DEST=NO_DATA'])
    got_data = dst_ds.GetRasterBand(1).ReadRaster(0, 0, 6, 3).decode('latin1')
    expected_data = '\x03\x7f\x7f\x7f\x03\x03\x03\x7f\x7f\x7f\x03\x03\x03\x7f\x7f\x7f\x03\x03'
    if got_data != expected_data:
        import struct
        pytest.fail(struct.unpack('B' * 18, got_data))

    


