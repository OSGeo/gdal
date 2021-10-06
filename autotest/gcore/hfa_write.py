#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for Erdas Imagine (.img) HFA driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

import pytest

from osgeo import gdal, osr

import gdaltest

###############################################################################
# test that we can write a small file with a custom layer name.


def test_hfa_write_desc():

    src_ds = gdal.Open('data/byte.tif')

    new_ds = gdal.GetDriverByName('HFA').CreateCopy('tmp/test_desc.img',
                                                    src_ds)

    bnd = new_ds.GetRasterBand(1)
    bnd.SetDescription('CustomBandName')
    bnd = None

    src_ds = None
    new_ds = None

    new_ds = gdal.Open('tmp/test_desc.img')
    bnd = new_ds.GetRasterBand(1)
    assert bnd.GetDescription() == 'CustomBandName', 'Didnt get custom band name.'

    bnd = None
    new_ds = None

    gdal.GetDriverByName('HFA').Delete('tmp/test_desc.img')

###############################################################################
# test writing 4 bit files.


def test_hfa_write_4bit():
    drv = gdal.GetDriverByName('HFA')
    src_ds = gdal.Open('data/byte.tif')
    ds = drv.CreateCopy('tmp/4bit.img', src_ds, options=['NBITS=1'])
    ds = None
    src_ds = None

    ds = gdal.Open('tmp/4bit.img')
    cs = ds.GetRasterBand(1).Checksum()

    assert cs == 252, 'Got wrong checksum on 4bit image.'

    ds = None

    drv.Delete('tmp/4bit.img')

###############################################################################
# test writing 4 bit files compressed.


def test_hfa_write_4bit_compressed():
    drv = gdal.GetDriverByName('HFA')
    src_ds = gdal.Open('data/byte.tif')
    ds = drv.CreateCopy('tmp/4bitc.img', src_ds,
                        options=['NBITS=1', 'COMPRESSED=YES'])
    ds = None
    src_ds = None

    ds = gdal.Open('tmp/4bitc.img')
    cs = ds.GetRasterBand(1).Checksum()

    assert cs == 252, 'Got wrong checksum on 4bit image.'

    ds = None

    drv.Delete('tmp/4bitc.img')

###############################################################################
# Test creating a file with a nodata value, and fetching otherwise unread
# blocks and verifying they are the nodata value.  (#2427)


def test_hfa_write_nd_invalid():

    drv = gdal.GetDriverByName('HFA')
    ds = drv.Create('tmp/ndinvalid.img', 512, 512, 1, gdal.GDT_Byte, [])
    ds.GetRasterBand(1).SetNoDataValue(200)
    ds = None

    ds = gdal.Open('tmp/ndinvalid.img')
    cs = ds.GetRasterBand(1).Checksum()

    assert cs == 29754, 'Got wrong checksum on invalid image.'

    ds = None

    drv.Delete('tmp/ndinvalid.img')

###############################################################################
# Test updating .rrd overviews in place (#2524).


def test_hfa_update_overviews():

    shutil.copyfile('data/small_ov.img', 'tmp/small.img')
    shutil.copyfile('data/small_ov.rrd', 'tmp/small.rrd')

    ds = gdal.Open('tmp/small.img', gdal.GA_Update)
    result = ds.BuildOverviews(overviewlist=[2])

    assert result == 0, 'BuildOverviews() failed.'
    ds = None

###############################################################################
# Test cleaning external overviews.


def test_hfa_clean_external_overviews():

    ds = gdal.Open('tmp/small.img', gdal.GA_Update)
    result = ds.BuildOverviews(overviewlist=[])

    assert result == 0, 'BuildOverviews() failed.'

    assert ds.GetRasterBand(1).GetOverviewCount() == 0, 'Overviews still exist.'

    ds = None
    ds = gdal.Open('tmp/small.img')
    assert ds.GetRasterBand(1).GetOverviewCount() == 0, 'Overviews still exist.'
    ds = None

    assert not os.path.exists('tmp/small.rrd')

    gdal.GetDriverByName('HFA').Delete('tmp/small.img')

###############################################################################
# Test writing high frequency data (#2525).


def test_hfa_bug_2525():
    drv = gdal.GetDriverByName('HFA')
    ds = drv.Create('tmp/test_hfa.img', 64, 64, 1, gdal.GDT_UInt16, options=['COMPRESSED=YES'])
    import struct
    data = struct.pack('H' * 64, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535, 0, 65535)
    for i in range(64):
        ds.GetRasterBand(1).WriteRaster(0, i, 64, 1, data)
    ds = None

    drv.Delete('tmp/test_hfa.img')

###############################################################################
# Test building external overviews with HFA_USE_RRD=YES


def test_hfa_use_rrd():

    shutil.copyfile('data/small_ov.img', 'tmp/small.img')

    old_value = gdal.GetConfigOption('HFA_USE_RRD', 'NO')
    gdal.SetConfigOption('HFA_USE_RRD', 'YES')
    ds = gdal.Open('tmp/small.img', gdal.GA_Update)
    result = ds.BuildOverviews(overviewlist=[2])
    gdal.SetConfigOption('HFA_USE_RRD', old_value)

    assert result == 0, 'BuildOverviews() failed.'
    ds = None

    try:
        os.stat('tmp/small.rrd')
    except OSError:
        pytest.fail('small.rrd not present.')

    ds = gdal.Open('tmp/small.img')
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 26148, \
        'Unexpected checksum.'

    ds = None

    gdal.GetDriverByName('HFA').Delete('tmp/small.img')


###############################################################################
# Test fix for #4831

def test_hfa_update_existing_aux_overviews():

    gdal.SetConfigOption('USE_RRD', 'YES')

    ds = gdal.GetDriverByName('BMP').Create('tmp/hfa_update_existing_aux_overviews.bmp', 100, 100, 1)
    ds.GetRasterBand(1).Fill(255)
    ds = None

    # Create overviews
    ds = gdal.Open('tmp/hfa_update_existing_aux_overviews.bmp')
    ds.BuildOverviews('NEAR', overviewlist=[2, 4])
    ds = None

    # Save overviews checksum
    ds = gdal.Open('tmp/hfa_update_existing_aux_overviews.bmp')
    cs_ovr0 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs_ovr1 = ds.GetRasterBand(1).GetOverview(1).Checksum()

    # and regenerate them
    ds.BuildOverviews('NEAR', overviewlist=[2, 4])
    ds = None

    ds = gdal.Open('tmp/hfa_update_existing_aux_overviews.bmp')
    # Check overviews checksum
    new_cs_ovr0 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    new_cs_ovr1 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    if cs_ovr0 != new_cs_ovr0:
        gdal.SetConfigOption('USE_RRD', None)
        pytest.fail()
    if cs_ovr1 != new_cs_ovr1:
        gdal.SetConfigOption('USE_RRD', None)
        pytest.fail()

    # and regenerate them twice in a row
    ds.BuildOverviews('NEAR', overviewlist=[2, 4])
    ds.BuildOverviews('NEAR', overviewlist=[2, 4])
    ds = None

    ds = gdal.Open('tmp/hfa_update_existing_aux_overviews.bmp')
    # Check overviews checksum
    new_cs_ovr0 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    new_cs_ovr1 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    if cs_ovr0 != new_cs_ovr0:
        gdal.SetConfigOption('USE_RRD', None)
        pytest.fail()
    if cs_ovr1 != new_cs_ovr1:
        gdal.SetConfigOption('USE_RRD', None)
        pytest.fail()

    # and regenerate them with an extra overview level
    ds.BuildOverviews('NEAR', overviewlist=[8])
    ds = None

    ds = gdal.Open('tmp/hfa_update_existing_aux_overviews.bmp')
    # Check overviews checksum
    new_cs_ovr0 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    new_cs_ovr1 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    if cs_ovr0 != new_cs_ovr0:
        gdal.SetConfigOption('USE_RRD', None)
        pytest.fail()
    if cs_ovr1 != new_cs_ovr1:
        gdal.SetConfigOption('USE_RRD', None)
        pytest.fail()
    ds = None

    gdal.GetDriverByName('BMP').Delete('tmp/hfa_update_existing_aux_overviews.bmp')

    gdal.SetConfigOption('USE_RRD', None)

###############################################################################
# Test writing invalid WKT (#5258)


def test_hfa_write_invalid_wkt():

    # No GEOGCS
    ds = gdal.GetDriverByName('HFA').Create('/vsimem/hfa_write_invalid_wkt.img', 1, 1)
    ds.SetProjection("""PROJCS["NAD27 / UTM zone 11N",
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-117],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AUTHORITY["EPSG","26711"]]""")
    ds = None

    # No DATUM in GEOGCS
    ds = gdal.GetDriverByName('HFA').Create('/vsimem/hfa_write_invalid_wkt.img', 1, 1)
    ds.SetProjection("""PROJCS["NAD27 / UTM zone 11N",
    GEOGCS["NAD27",
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-117],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AUTHORITY["EPSG","26711"]]""")
    ds = None

    # No SPHEROID in DATUM
    ds = gdal.GetDriverByName('HFA').Create('/vsimem/hfa_write_invalid_wkt.img', 1, 1)
    ds.SetProjection("""PROJCS["NAD27 / UTM zone 11N",
    GEOGCS["NAD27",
        DATUM["North_American_Datum_1927",
            AUTHORITY["EPSG","6267"]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433],
        AUTHORITY["EPSG","4267"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-117],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["metre",1,
        AUTHORITY["EPSG","9001"]],
    AUTHORITY["EPSG","26711"]]""")
    ds = None

    gdal.GetDriverByName('HFA').Delete('/vsimem/hfa_write_invalid_wkt.img')

###############################################################################
# Get the driver, and verify a few things about it.


init_list = [
    ('byte.tif', 4672),
    ('int16.tif', 4672),
    ('uint16.tif', 4672),
    ('int32.tif', 4672),
    ('uint32.tif', 4672),
    ('float32.tif', 4672),
    ('float64.tif', 4672),
    ('cfloat32.tif', 5028),
    ('cfloat64.tif', 5028),
    ('utmsmall.tif', 50054)]

# full set of tests for normal mode.

@pytest.mark.parametrize(
    'filename,checksum',
    init_list,
    ids=[tup[0].split('.')[0] for tup in init_list],
)
@pytest.mark.parametrize(
    'testfunction', [
        'testCreateCopy',
        'testCreate',
        'testSetGeoTransform',
        'testSetMetadata',
    ]
)
@pytest.mark.require_driver('HFA')
def test_hfa_create_normal(filename, checksum, testfunction):
    ut = gdaltest.GDALTest('HFA', filename, 1, checksum)
    getattr(ut, testfunction)()


# Just a few for spill file, and compressed support.
short_list = [
    ('byte.tif', 4672),
    ('uint16.tif', 4672),
    ('float64.tif', 4672)]

@pytest.mark.parametrize(
    'filename,checksum',
    short_list,
    ids=[tup[0].split('.')[0] for tup in short_list],
)
@pytest.mark.parametrize(
    'testfunction', [
        'testCreateCopy',
        'testCreate',
    ]
)
@pytest.mark.require_driver('HFA')
def test_hfa_create_spill(filename, checksum, testfunction):
    ut = gdaltest.GDALTest('HFA', filename, 1, checksum, options=['USE_SPILL=YES'])
    getattr(ut, testfunction)()


@pytest.mark.parametrize(
    'filename,checksum',
    short_list,
    ids=[tup[0].split('.')[0] for tup in short_list],
)
@pytest.mark.parametrize(
    'testfunction', [
        # 'testCreateCopy',
        'testCreate',
    ]
)
@pytest.mark.require_driver('HFA')
def test_hfa_create_compress(filename, checksum, testfunction):
    ut = gdaltest.GDALTest('HFA', filename, 1, checksum, options=['COMPRESS=YES'])
    getattr(ut, testfunction)()


def test_hfa_create_compress_big_block():
    src_ds = gdal.GetDriverByName('MEM').Create('/vsimem/big_block.img', 128, 128, 1, gdal.GDT_UInt32)
    src_ds.GetRasterBand(1).Fill(4 * 1000 * 1000 * 1000)
    src_ds.GetRasterBand(1).WriteRaster(0,0,1,1,b'\0')
    gdal.GetDriverByName('HFA').CreateCopy('/vsimem/big_block.img', src_ds, options=['COMPRESS=YES', 'BLOCKSIZE=128'])
    ds = gdal.Open('/vsimem/big_block.img')
    got_data = ds.GetRasterBand(1).ReadRaster()
    ds = None
    gdal.Unlink('/vsimem/big_block.img')
    assert got_data == src_ds.GetRasterBand(1).ReadRaster()


# GCPs go to PAM currently
def test_hfa_create_gcp():
    filename = '/vsimem/test.img'
    ds = gdal.GetDriverByName('HFA').Create(filename, 1, 1)
    gcp1 = gdal.GCP()
    gcp1.GCPPixel = 0
    gcp1.GCPLine = 0
    gcp1.GCPX = 440720.000
    gcp1.GCPY = 3751320.000
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    assert ds.SetGCPs((gcp1, ), sr.ExportToWkt()) == gdal.CE_None
    ds = None
    ds = gdal.Open(filename)
    assert ds.GetGCPCount() == 1
    assert ds.GetGCPSpatialRef() is not None
    assert len(ds.GetGCPs()) == 1
    ds = None
    gdal.GetDriverByName('HFA').Delete(filename)
