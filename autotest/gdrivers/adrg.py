#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for ADRG driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2007-2009, Even Rouault <even dot rouault at spatialys.com>
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
import shutil
from osgeo import gdal


import gdaltest

###############################################################################
# Read test of simple byte reference data.


def test_adrg_read_gen():

    tst = gdaltest.GDALTest('ADRG', 'adrg/SMALL_ADRG/ABCDEF01.GEN', 1, 62833)
    return tst.testOpen()

###############################################################################
# Read test of simple byte reference data by the TRANSH01.THF file .


def test_adrg_read_transh():

    tst = gdaltest.GDALTest('ADRG', 'adrg/SMALL_ADRG/TRANSH01.THF', 1, 62833)
    return tst.testOpen()

###############################################################################
# Read test of simple byte reference data by a subdataset file


def test_adrg_read_subdataset_img():

    tst = gdaltest.GDALTest('ADRG', 'ADRG:data/adrg/SMALL_ADRG/ABCDEF01.GEN,data/adrg/SMALL_ADRG/ABCDEF01.IMG', 1, 62833, filename_absolute=1)
    return tst.testOpen()

###############################################################################
# Test copying.


def test_adrg_copy():

    drv = gdal.GetDriverByName('ADRG')
    srcds = gdal.Open('data/adrg/SMALL_ADRG/ABCDEF01.GEN')

    dstds = drv.CreateCopy('tmp/ABCDEF01.GEN', srcds)

    chksum = dstds.GetRasterBand(1).Checksum()

    assert chksum == 62833, 'Wrong checksum'

    dstds = None

    drv.Delete('tmp/ABCDEF01.GEN')

###############################################################################
# Test creating a fake 2 subdataset image and reading it.


def test_adrg_2subdatasets():

    drv = gdal.GetDriverByName('ADRG')
    srcds = gdal.Open('data/adrg/SMALL_ADRG/ABCDEF01.GEN')

    gdal.SetConfigOption('ADRG_SIMULATE_MULTI_IMG', 'ON')
    dstds = drv.CreateCopy('tmp/XXXXXX01.GEN', srcds)
    del dstds
    gdal.SetConfigOption('ADRG_SIMULATE_MULTI_IMG', 'OFF')

    shutil.copy('tmp/XXXXXX01.IMG', 'tmp/XXXXXX02.IMG')

    ds = gdal.Open('tmp/TRANSH01.THF')
    assert ds.RasterCount == 0, 'did not expected non 0 RasterCount'
    ds = None

    ds = gdal.Open('ADRG:tmp/XXXXXX01.GEN,tmp/XXXXXX02.IMG')
    chksum = ds.GetRasterBand(1).Checksum()

    assert chksum == 62833, 'Wrong checksum'

    md = ds.GetMetadata('')
    assert md['ADRG_NAM'] == 'XXXXXX02', 'metadata wrong.'

    ds = None

    os.remove('tmp/XXXXXX01.GEN')
    os.remove('tmp/XXXXXX01.GEN.aux.xml')
    os.remove('tmp/XXXXXX01.IMG')
    os.remove('tmp/XXXXXX02.IMG')
    os.remove('tmp/TRANSH01.THF')

###############################################################################
# Test creating an in memory copy.


def test_adrg_copy_vsimem():

    drv = gdal.GetDriverByName('ADRG')
    srcds = gdal.Open('data/adrg/SMALL_ADRG/ABCDEF01.GEN')

    dstds = drv.CreateCopy('/vsimem/ABCDEF01.GEN', srcds)

    chksum = dstds.GetRasterBand(1).Checksum()

    assert chksum == 62833, 'Wrong checksum'

    dstds = None

    # Reopen file
    ds = gdal.Open('/vsimem/ABCDEF01.GEN')

    chksum = ds.GetRasterBand(1).Checksum()
    assert chksum == 62833, 'Wrong checksum'

    ds = None

    drv.Delete('/vsimem/ABCDEF01.GEN')
    gdal.Unlink('/vsimem/TRANSH01.THF')

###############################################################################
# Test reading a fake North Polar dataset (#6560)


def test_adrg_zna_9():

    ds = gdal.Open('data/adrg/SMALL_ADRG_ZNA9/ABCDEF01.GEN')
    expected_gt = (-307675.73602473765, 100.09145391818853, 0.0, -179477.5051066006, 0.0, -100.09145391818853)
    gt = ds.GetGeoTransform()
    assert max(abs(gt[i] - expected_gt[i]) for i in range(6)) <= 1e-5, \
        'Wrong geotransfsorm'
    wkt = ds.GetProjectionRef()
    assert wkt == """PROJCS["ARC_System_Zone_09",GEOGCS["Unknown datum based upon the Authalic Sphere",DATUM["Not_specified_based_on_Authalic_Sphere",SPHEROID["Sphere",6378137,0],AUTHORITY["EPSG","6035"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Azimuthal_Equidistant"],PARAMETER["latitude_of_center",90],PARAMETER["longitude_of_center",0],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1],AXIS["Easting",EAST],AXIS["Northing",NORTH]]""", \
        'Wrong WKT'

###############################################################################
# Test reading a fake South Polar dataset (#6560)


def test_adrg_zna_18():

    ds = gdal.Open('data/adrg/SMALL_ADRG_ZNA18/ABCDEF01.GEN')
    expected_gt = (-307675.73602473765, 100.09145391818853, 0.0, 179477.5051066006, 0.0, -100.09145391818853)
    gt = ds.GetGeoTransform()
    assert max(abs(gt[i] - expected_gt[i]) for i in range(6)) <= 1e-5, \
        'Wrong geotransfsorm'
    wkt = ds.GetProjectionRef()
    assert wkt == """PROJCS["ARC_System_Zone_18",GEOGCS["Unknown datum based upon the Authalic Sphere",DATUM["Not_specified_based_on_Authalic_Sphere",SPHEROID["Sphere",6378137,0],AUTHORITY["EPSG","6035"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]],PROJECTION["Azimuthal_Equidistant"],PARAMETER["latitude_of_center",-90],PARAMETER["longitude_of_center",0],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1],AXIS["Easting",EAST],AXIS["Northing",NORTH]]""", \
        'Wrong WKT'


###############################################################################

