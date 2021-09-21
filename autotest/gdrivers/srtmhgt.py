#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SRTMHGT support.
# Author:   Even Rouault < even dot rouault @ spatialys.com >
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
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
import struct
from osgeo import gdal


import pytest


###############################################################################
# Test a SRTMHGT Level 1 (made from a DTED Level 0)

def test_srtmhgt_1():

    ds = gdal.Open('data/n43.dt0')

    bandSrc = ds.GetRasterBand(1)

    driver = gdal.GetDriverByName("GTiff")
    dsDst = driver.Create('tmp/n43.dt1.tif', 1201, 1201, 1, gdal.GDT_Int16)
    dsDst.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]')
    dsDst.SetGeoTransform((-80.0004166666666663, 0.0008333333333333, 0, 44.0004166666666670, 0, -0.0008333333333333))

    bandDst = dsDst.GetRasterBand(1)

    data = bandSrc.ReadRaster(0, 0, 121, 121, 1201, 1201, gdal.GDT_Int16)
    bandDst.WriteRaster(0, 0, 1201, 1201, data, 1201, 1201, gdal.GDT_Int16)

    bandDst.FlushCache()

    bandDst = None
    ds = None
    dsDst = None

    ds = gdal.Open('tmp/n43.dt1.tif')
    driver = gdal.GetDriverByName("SRTMHGT")
    dsDst = driver.CreateCopy('tmp/n43w080.hgt', ds)

    band = dsDst.GetRasterBand(1)
    chksum = band.Checksum()

    assert chksum == 60918, ('Wrong checksum. Checksum found %d' % chksum)


###############################################################################
# Test creating an in memory copy.

def test_srtmhgt_2():

    ds = gdal.Open('tmp/n43w080.hgt')
    driver = gdal.GetDriverByName("SRTMHGT")
    dsDst = driver.CreateCopy('/vsimem/n43w080.hgt', ds)

    band = dsDst.GetRasterBand(1)
    chksum = band.Checksum()

    assert chksum == 60918, ('Wrong checksum. Checksum found %d' % chksum)
    dsDst = None

    # Test update support
    dsDst = gdal.Open('/vsimem/n43w080.hgt', gdal.GA_Update)
    dsDst.WriteRaster(0, 0, dsDst.RasterXSize, dsDst.RasterYSize,
                      dsDst.ReadRaster())
    dsDst.FlushCache()

    assert chksum == 60918, ('Wrong checksum. Checksum found %d' % chksum)
    dsDst = None

###############################################################################
# Test reading from a .hgt.zip file


def test_srtmhgt_3():

    ds = gdal.Open('tmp/n43w080.hgt')
    driver = gdal.GetDriverByName("SRTMHGT")
    driver.CreateCopy('/vsizip//vsimem/N43W080.SRTMGL1.hgt.zip/N43W080.hgt', ds)

    dsDst = gdal.Open('/vsimem/N43W080.SRTMGL1.hgt.zip')

    band = dsDst.GetRasterBand(1)
    chksum = band.Checksum()

    assert chksum == 60918, ('Wrong checksum. Checksum found %d' % chksum)

###############################################################################
# Test reading from a .SRTMSWBD.raw.zip file (GRASS #3246)


def test_srtmhgt_4():

    f = gdal.VSIFOpenL('/vsizip//vsimem/N43W080.SRTMSWBD.raw.zip/N43W080.raw', 'wb')
    if f is None:
        pytest.skip()
    gdal.VSIFWriteL(' ' * (3601 * 3601), 1, 3601 * 3601, f)
    gdal.VSIFCloseL(f)

    ds = gdal.Open('/vsimem/N43W080.SRTMSWBD.raw.zip')
    assert ds is not None
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.Unlink('/vsimem/N43W080.SRTMSWBD.raw.zip')

    assert cs == 3636, ('Wrong checksum. Checksum found %d' % cs)

###############################################################################
# Test reading from a .hgts file (https://github.com/OSGeo/gdal/issues/4239


def test_srtmhgt_hgts():

    f = gdal.VSIFOpenL('/vsimem/n00e006.hgts', 'wb')
    if f is None:
        pytest.skip()
    gdal.VSIFWriteL(struct.pack('>f', 1.25) * (3601 * 3601), 4, 3601 * 3601, f)
    gdal.VSIFCloseL(f)

    ds = gdal.Open('/vsimem/n00e006.hgts')
    assert ds is not None
    min_, max_ = ds.GetRasterBand(1).ComputeRasterMinMax()
    gdal.Unlink('/vsimem/n00e006.hgts')

    assert min_ == 1.25
    assert max_ == 1.25

###############################################################################
# Cleanup.


def test_srtmhgt_cleanup():
    try:
        gdal.GetDriverByName("SRTMHGT").Delete('tmp/n43w080.hgt')
        gdal.GetDriverByName("SRTMHGT").Delete('/vsimem/n43w080.hgt')
        gdal.Unlink('/vsimem/N43W080.SRTMGL1.hgt.zip')
        os.remove('tmp/n43.dt1.tif')
    except (RuntimeError, OSError):
        pass




