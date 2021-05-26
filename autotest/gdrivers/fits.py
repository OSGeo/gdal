#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for FITS driver.
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008,2020, Even Rouault <even dot rouault at spatialys.com>
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
import pytest

from osgeo import gdal
from osgeo import ogr
import gdaltest

pytestmark = pytest.mark.require_driver('FITS')


@pytest.mark.parametrize(
    'filename',
    ['byte', 'int16', 'uint16', 'int32', 'uint32', 'float32', 'float64']
)
def test_fits(filename):
    driver = gdal.GetDriverByName('FITS')

    ds = gdal.Open('../gcore/data/' + filename + '.tif')
    driver.CreateCopy('tmp/' + filename + '.fits', ds, options=['PAGESIZE=2,2'])

    ds2 = gdal.Open('tmp/' + filename + '.fits')
    assert ds2.GetRasterBand(1).Checksum() == ds.GetRasterBand(1).Checksum()

    assert ds2.GetRasterBand(1).DataType == ds.GetRasterBand(1).DataType

    ds2 = None
    driver.Delete('tmp/' + filename + '.fits')

def test_fits_metadata():
    driver = gdal.GetDriverByName('FITS')

    ds = gdal.Open('../gcore/data/byte.tif')
    ds2 = driver.CreateCopy('tmp/byte.fits', ds)
    md = {'TEST': 'test_value'}
    ds2.SetMetadata(md)
    ds2 = None
    gdal.Unlink('tmp/byte.fits.aux.xml')

    ds2 = gdal.Open('tmp/byte.fits')
    md = ds2.GetMetadata()
    ds2 = None

    assert md['TEST'] == 'test_value'

    ds2 = gdal.Open('tmp/byte.fits', gdal.GA_Update)
    md = {'TEST2': 'test_value2'}
    ds2.SetMetadata(md)
    ds2 = None
    gdal.Unlink('tmp/byte.fits.aux.xml')

    ds2 = gdal.Open('tmp/byte.fits')
    md = ds2.GetMetadata()
    ds2 = None

    assert md['TEST2'] == 'test_value2'

def test_fits_nodata():
    driver = gdal.GetDriverByName('FITS')

    ds = gdal.Open('../gcore/data/nodata_byte.tif')
    ds2 = driver.CreateCopy('tmp/nodata_byte.fits', ds)
    ds2 = None
    gdal.Unlink('tmp/nodata_byte.fits.aux.xml')

    ds2 = gdal.Open('tmp/nodata_byte.fits')
    nd = ds2.GetRasterBand(1).GetNoDataValue()
    ds2 = None
    driver.Delete('tmp/nodata_byte.fits')

    assert nd == 0

def test_fits_offscale():
    driver = gdal.GetDriverByName('FITS')

    ds = gdal.Open('../gdrivers/data/fits/offscale_byte.tif')
    ds2 = driver.CreateCopy('tmp/offscale_byte.fits', ds)
    ds2 = None
    gdal.Unlink('tmp/offscale_byte.fits.aux.xml')

    ds2 = gdal.Open('tmp/offscale_byte.fits')
    offset = ds2.GetRasterBand(1).GetOffset()
    scale = ds2.GetRasterBand(1).GetScale()
    ds2 = None
    driver.Delete('tmp/offscale_byte.fits')

    assert offset == -0.0039525691699605
    assert scale == 1.00395256917


def test_fits_read_offset_scale_no_georef():

    ds = gdal.Open('data/fits/offset_scale_no_georef.fits')
    assert gdal.GetLastErrorMsg() == ''
    assert ds.GetRasterBand(1).GetOffset() != 0
    assert ds.GetRasterBand(1).GetScale() != 1


def test_fits_read_georef_merc():

    ds = gdal.Open('data/fits/byte_merc.fits')
    assert gdal.GetLastErrorMsg() == ''
    wkt = ds.GetProjectionRef()
    assert wkt == 'PROJCS["Mercator_Earth",GEOGCS["GCS_Earth",DATUM["D_Earth",SPHEROID["Earth",6378206.4,294.978698213898]],PRIMEM["Reference_Meridian",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]]],PROJECTION["Mercator_1SP"],PARAMETER["central_meridian",0],PARAMETER["scale_factor",1],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH]]'
    gt = ds.GetGeoTransform()
    assert gt == pytest.approx((-13095897.481058259, 72.23522015778646, 0.0, 3991653.2130816197, 0.0, -72.23522015778646), abs=1e-3)


def test_fits_read_empty_primary_hdu():

    filename = 'data/fits/empty_primary_hdu.fits'
    assert os.path.exists(filename)
    with gdaltest.error_handler():
        assert gdal.Open(filename) is None

def test_fits_read_image_in_second_hdu():

    ds = gdal.Open('data/fits/image_in_second_hdu.fits')
    assert gdal.GetLastErrorMsg() == ''
    assert ds is not None
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 2
    assert ds.RasterCount == 1
    assert ds.GetMetadata() == {'BAR': 'BAZ     ', 'FOO': 'BAR_override', 'FOO2': 'BAR2    '}


def test_fits_read_image_in_first_and_second_hdu():

    ds = gdal.Open('data/fits/image_in_first_and_second_hdu.fits')
    assert gdal.GetLastErrorMsg() == ''
    assert ds is not None
    assert ds.RasterCount == 0
    assert ds.GetMetadata() == {'EXTNAME': 'FIRST_IMAGE'}

    sub_ds = ds.GetSubDatasets()
    assert len(sub_ds) == 2
    assert sub_ds[0][0] == 'FITS:"data/fits/image_in_first_and_second_hdu.fits":1'
    assert sub_ds[0][1] == 'HDU 1 (1x2, 1 band), FIRST_IMAGE'
    assert sub_ds[1][0] == 'FITS:"data/fits/image_in_first_and_second_hdu.fits":2'
    assert sub_ds[1][1] == 'HDU 2 (1x3, 1 band)'

    with gdaltest.error_handler():
        assert gdal.Open('FITS:') is None
        assert gdal.Open('FITS:"data/fits/image_in_first_and_second_hdu.fits"') is None
        assert gdal.Open('FITS:"data/fits/image_in_first_and_second_hdu.fits":0') is None
        assert gdal.Open('FITS:"data/fits/image_in_first_and_second_hdu.fits":3') is None
        assert gdal.Open('FITS:"data/fits/non_existing.fits":1') is None

    ds = gdal.Open('FITS:"data/fits/image_in_first_and_second_hdu.fits":1')
    assert ds.GetSubDatasets() == []
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 2
    assert ds.GetMetadata() == {'EXTNAME': 'FIRST_IMAGE'}

    ds = gdal.Open('FITS:"data/fits/image_in_first_and_second_hdu.fits":2')
    assert ds.GetSubDatasets() == []
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 3
    assert ds.GetMetadata() == {}


def test_fits_read_image_in_second_and_fourth_hdu_table_in_third():

    ds = gdal.Open('data/fits/image_in_second_and_fourth_hdu_table_in_third.fits')
    assert gdal.GetLastErrorMsg() == ''
    assert ds is not None
    assert ds.RasterCount == 0
    assert ds.GetMetadata() == {'FOO': 'BAR     '}

    sub_ds = ds.GetSubDatasets()
    assert len(sub_ds) == 2

    ds = gdal.Open(sub_ds[0][0])
    assert ds.GetSubDatasets() == []
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 2
    assert ds.GetMetadata() == {'EXTNAME': 'FIRST_IMAGE', 'FOO': 'BAR     '}

    ds = gdal.Open(sub_ds[1][0])
    assert ds.GetSubDatasets() == []
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 3
    assert ds.GetMetadata() == {'EXTNAME': 'SECOND_IMAGE', 'FOO': 'BAR     '}


# Test opening raster files in vector mode
def test_fits_open_raster_only_in_vector_mode():

    filename = 'data/fits/empty_primary_hdu.fits'
    assert os.path.exists(filename)
    with gdaltest.error_handler():
        assert ogr.Open(filename) is None

    filename = 'data/fits/byte_merc.fits'
    assert os.path.exists(filename)
    with gdaltest.error_handler():
        assert ogr.Open(filename) is None
    assert 'but contains image' in gdal.GetLastErrorMsg()

    filename = 'data/fits/image_in_first_and_second_hdu.fits'
    assert os.path.exists(filename)
    with gdaltest.error_handler():
        assert ogr.Open(filename) is None

    ds = gdal.OpenEx('data/fits/image_in_second_and_fourth_hdu_table_in_third.fits', gdal.OF_VECTOR)
    assert ds.GetMetadata('SUBDATASETS') == {}


# Test opening vector files in raster mode
def test_fits_open_vector_only_in_raster_mode():

    filename = 'data/fits/binary_table.fits'
    assert os.path.exists(filename)
    with gdaltest.error_handler():
        assert gdal.Open(filename) is None
    assert 'but contains binary table' in gdal.GetLastErrorMsg()

# Test opening with gdal.OF_RASTER | gdal.OF_VECTOR
def test_fits_open_mix_mode():

    # Raster only content
    ds = gdal.OpenEx('data/fits/byte_merc.fits', gdal.OF_RASTER | gdal.OF_VECTOR)
    assert ds.RasterCount != 0
    assert ds.GetLayerCount() == 0

    # Vector only content
    ds = gdal.OpenEx('data/fits/binary_table.fits', gdal.OF_RASTER | gdal.OF_VECTOR)
    assert ds.RasterCount == 0
    assert ds.GetLayerCount() != 0

    # Mix of raster and vector
    ds = gdal.OpenEx('data/fits/image_in_second_and_fourth_hdu_table_in_third.fits', gdal.OF_RASTER | gdal.OF_VECTOR)
    assert ds.GetMetadata('SUBDATASETS') != {}
    assert ds.GetLayerCount() != 0


def test_fits_vector():

    ds = ogr.Open('data/fits/binary_table.fits')
    assert ds.GetLayerCount() == 1
    assert ds.GetLayer(-1) is None
    assert ds.GetLayer(1) is None
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'MyTable'
    assert lyr.GetGeomType() == ogr.wkbNone
    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1
    assert lyr.TestCapability(ogr.OLCRandomRead) == 1
    assert lyr.TestCapability("something_else") == 0
    assert lyr.GetFeatureCount() == 3
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldCount() == 78
    mapType = {}
    for x in ["ogr.OFTInteger", "ogr.OFTInteger64", "ogr.OFTReal",
              "ogr.OFTString", "ogr.OFTIntegerList", "ogr.OFTInteger64List",
              "ogr.OFTRealList", "ogr.OFTStringList"]:
        mapType[eval(x)] = x
    mapSubType = {}
    for x in ["ogr.OFSTNone", "ogr.OFSTBoolean", "ogr.OFSTFloat32", "ogr.OFSTInt16"]:
        mapSubType[eval(x)] = x
    got = [(lyr_defn.GetFieldDefn(i).GetNameRef(),
            mapType[lyr_defn.GetFieldDefn(i).GetType()],
            mapSubType[lyr_defn.GetFieldDefn(i).GetSubType()],
            lyr_defn.GetFieldDefn(i).GetWidth()) for i in range(lyr_defn.GetFieldCount())]
    expected = [('B_scaled_integer', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('B_scaled', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('I_scaled_integer', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('I_scaled', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('J_scaled_integer', 'ogr.OFTInteger64', 'ogr.OFSTNone', 0),
                ('J_scaled', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('K_scaled', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('E_scaled', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('D_scaled', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('C_scaled', 'ogr.OFTString', 'ogr.OFSTNone', 0),
                ('M_scaled', 'ogr.OFTString', 'ogr.OFSTNone', 0),
                ('L', 'ogr.OFTInteger', 'ogr.OFSTBoolean', 0),
                ('2L', 'ogr.OFTIntegerList', 'ogr.OFSTBoolean', 0),
                ('PL', 'ogr.OFTIntegerList', 'ogr.OFSTBoolean', 0),
                ('QL', 'ogr.OFTIntegerList', 'ogr.OFSTBoolean', 0),
                ('X_bit1', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit1', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit2', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit3', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit4', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit5', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit6', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit7', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit8', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit9', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit10', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit11', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit12', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit13', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit14', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit15', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit16', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit17', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit18', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit19', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit20', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit21', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit22', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit23', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit24', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit25', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit26', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit27', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit28', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit29', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit30', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit31', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit32', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit33', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('B', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('2B', 'ogr.OFTIntegerList', 'ogr.OFSTNone', 0),
                ('PB', 'ogr.OFTIntegerList', 'ogr.OFSTNone', 0),
                ('BDIM', 'ogr.OFTIntegerList', 'ogr.OFSTNone', 0),
                ('I', 'ogr.OFTInteger', 'ogr.OFSTInt16', 0),
                ('2I', 'ogr.OFTIntegerList', 'ogr.OFSTInt16', 0),
                ('PI', 'ogr.OFTIntegerList', 'ogr.OFSTInt16', 0),
                ('J', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('2J', 'ogr.OFTIntegerList', 'ogr.OFSTNone', 0),
                ('PJ', 'ogr.OFTIntegerList', 'ogr.OFSTNone', 0),
                ('K', 'ogr.OFTInteger64', 'ogr.OFSTNone', 0),
                ('2K', 'ogr.OFTInteger64List', 'ogr.OFSTNone', 0),
                ('PK', 'ogr.OFTInteger64List', 'ogr.OFSTNone', 0),
                ('A', 'ogr.OFTString', 'ogr.OFSTNone', 1),
                ('A2', 'ogr.OFTString', 'ogr.OFSTNone', 2),
                ('PA', 'ogr.OFTString', 'ogr.OFSTNone', 0),
                ('ADIM', 'ogr.OFTStringList', 'ogr.OFSTNone', 2),
                ('E', 'ogr.OFTReal', 'ogr.OFSTFloat32', 0),
                ('2E', 'ogr.OFTRealList', 'ogr.OFSTFloat32', 0),
                ('PE', 'ogr.OFTRealList', 'ogr.OFSTFloat32', 0),
                ('D', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('2D', 'ogr.OFTRealList', 'ogr.OFSTNone', 0),
                ('PD', 'ogr.OFTRealList', 'ogr.OFSTNone', 0),
                ('C', 'ogr.OFTString', 'ogr.OFSTNone', 0),
                ('2C', 'ogr.OFTStringList', 'ogr.OFSTNone', 0),
                ('PC', 'ogr.OFTStringList', 'ogr.OFSTNone', 0),
                ('M', 'ogr.OFTString', 'ogr.OFSTNone', 0),
                ('2M', 'ogr.OFTStringList', 'ogr.OFSTNone', 0),
                ('PM', 'ogr.OFTStringList', 'ogr.OFSTNone', 0)]
    if got != expected:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(got)
        assert False

    assert lyr.GetFeature(0) is None
    assert lyr.GetFeature(4) is None

    expected_f1 = [
        -128,
        5.5,
        0,
        -49149.5,
        0,
        -3221225469.5,
        -1.3835058055282164e+19,
        4.375,
        4.375,
        '4.375 + 5.875j',
        '4.375 + 5.875j',
        0,
        [0, 0],
        [1, 0],
        [1, 0],
        1,
        1,
        1,
        1,
        1,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        1,
        0,
        [255, 0],
        [255, 0],
        [0, 255, 0, 255, 0, 255],
        -32768,
        [-32768, 32767],
        [-32768, 32767],
        -2147483648,
        [-2147483648, 2147483647],
        [-2147483648, 2147483647],
        -9223372036854775808,
        [-9223372036854775808, 9223372036854775807],
        [-9223372036854775808, 9223372036854775807],
        'A',
        'AB',
        'AB',
        ['AB', 'ab', 'Ab'],
        1.25,
        [1.25, 2.25],
        [1.25, 2.25],
        1.2534,
        [1.2534, 2.25],
        [1.2534, 2.25],
        '1.25 + 2.25j',
        ['1.25 + 2.25j', '2.25 + 1.25j'],
        ['1.25 + 2.25j', '2.25 + 1.25j'],
        '1.25340000000000007 + 2.25j',
        ['1.25340000000000007 + 2.25j', '2.25 + 1.25j'],
        ['1.25340000000000007 + 2.25j', '2.25 + 1.25j']]

    f = lyr.GetNextFeature()
    got = [f.GetField(i) for i in range(f.GetFieldCount())]
    assert got == expected_f1

    f = lyr.GetFeature(1)
    got = [f.GetField(i) for i in range(f.GetFieldCount())]
    assert got == expected_f1

    expected_f2 = [
        127,
        385.0,
        65535,
        49153.0,
        4294967295,
        3221225473.0,
        1.3835058055282164e+19,
        5.875,
        5.875,
        '2.5 + 2.5j',
        '2.5 + 2.5j',
        0,
        [0, 0],
        [0, 1, 0],
        [0, 1, 0],
        0,
        1,
        1,
        1,
        1,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        0,
        1,
        1,
        255,
        [0, 255],
        [0, 255, 0],
        [255, 255, 0, 0, 0, 255],
        32767,
        [32767, -32768],
        [32767, 0, -32768],
        2147483647,
        [2147483647, -2147483648],
        [2147483647, 0, -2147483648],
        9223372036854775807,
        [9223372036854775807, -9223372036854775808],
        [9223372036854775807, 0, -9223372036854775808],
        'B',
        'CD',
        'CDE',
        ['CD', 'cd', 'Cd'],
        2.25,
        [2.25, 1.25],
        [2.25, 1.25, 2.25],
        2.25,
        [2.2534, 1.25],
        [2.2534, 1.25, 2.25],
        '2.25 + 1.25j',
        ['2.25 + 1.25j', '1.25 + 2.25j'],
        ['2.25 + 1.25j', '1.25 + 2.25j', '2.25 + 1.25j'],
        '2.25 + 1.25j',
        ['2.25 + 1.25j', '1.25 + 2.25j'],
        ['2.25 + 1.25j', '1.25 + 2.25j', '2.25 + 1.25j']]

    f = lyr.GetNextFeature()
    got = [f.GetField(i) for i in range(f.GetFieldCount())]
    assert got == expected_f2

    f = lyr.GetNextFeature()
    assert f.GetField('B') is None


def _check_lyr_defn_after_write(lyr_defn):
    assert lyr_defn.GetFieldCount() == 73
    mapType = {}
    for x in ["ogr.OFTInteger", "ogr.OFTInteger64", "ogr.OFTReal",
            "ogr.OFTString", "ogr.OFTIntegerList", "ogr.OFTInteger64List",
            "ogr.OFTRealList", "ogr.OFTStringList"]:
        mapType[eval(x)] = x
    mapSubType = {}
    for x in ["ogr.OFSTNone", "ogr.OFSTBoolean", "ogr.OFSTFloat32", "ogr.OFSTInt16"]:
        mapSubType[eval(x)] = x
    got = [(lyr_defn.GetFieldDefn(i).GetNameRef(),
            mapType[lyr_defn.GetFieldDefn(i).GetType()],
            mapSubType[lyr_defn.GetFieldDefn(i).GetSubType()],
            lyr_defn.GetFieldDefn(i).GetWidth()) for i in range(lyr_defn.GetFieldCount())]
    expected = [('B_scaled_integer', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('B_scaled', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('I_scaled_integer', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('I_scaled', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('J_scaled_integer', 'ogr.OFTInteger64', 'ogr.OFSTNone', 0),
                ('J_scaled', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('K_scaled', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('E_scaled', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('D_scaled', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('C_scaled', 'ogr.OFTString', 'ogr.OFSTNone', 0),
                ('M_scaled', 'ogr.OFTString', 'ogr.OFSTNone', 0),
                ('L', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('2L', 'ogr.OFTIntegerList', 'ogr.OFSTNone', 0),
                ('PL', 'ogr.OFTIntegerList', 'ogr.OFSTNone', 0),
                ('QL', 'ogr.OFTIntegerList', 'ogr.OFSTNone', 0),
                ('X_bit1', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit1', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit2', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit3', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit4', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit5', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit6', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit7', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit8', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit9', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit10', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit11', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit12', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit13', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit14', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit15', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit16', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit17', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit18', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit19', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit20', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit21', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit22', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit23', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit24', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit25', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit26', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit27', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit28', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit29', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit30', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit31', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit32', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('33X_bit33', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('B', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('2B', 'ogr.OFTIntegerList', 'ogr.OFSTNone', 0),
                ('PB', 'ogr.OFTIntegerList', 'ogr.OFSTNone', 0),
                ('BDIM', 'ogr.OFTIntegerList', 'ogr.OFSTNone', 0),
                ('I', 'ogr.OFTInteger', 'ogr.OFSTInt16', 0),
                ('2I', 'ogr.OFTIntegerList', 'ogr.OFSTInt16', 0),
                ('PI', 'ogr.OFTIntegerList', 'ogr.OFSTInt16', 0),
                ('J', 'ogr.OFTInteger', 'ogr.OFSTNone', 0),
                ('2J', 'ogr.OFTIntegerList', 'ogr.OFSTNone', 0),
                ('PJ', 'ogr.OFTIntegerList', 'ogr.OFSTNone', 0),
                ('K', 'ogr.OFTInteger64', 'ogr.OFSTNone', 0),
                ('2K', 'ogr.OFTInteger64List', 'ogr.OFSTNone', 0),
                ('PK', 'ogr.OFTInteger64List', 'ogr.OFSTNone', 0),
                ('A', 'ogr.OFTString', 'ogr.OFSTNone', 1),
                ('A2', 'ogr.OFTString', 'ogr.OFSTNone', 2),
                ('PA', 'ogr.OFTString', 'ogr.OFSTNone', 0),
                ('E', 'ogr.OFTReal', 'ogr.OFSTFloat32', 0),
                ('2E', 'ogr.OFTRealList', 'ogr.OFSTFloat32', 0),
                ('PE', 'ogr.OFTRealList', 'ogr.OFSTFloat32', 0),
                ('D', 'ogr.OFTReal', 'ogr.OFSTNone', 0),
                ('2D', 'ogr.OFTRealList', 'ogr.OFSTNone', 0),
                ('PD', 'ogr.OFTRealList', 'ogr.OFSTNone', 0),
                ('C', 'ogr.OFTString', 'ogr.OFSTNone', 0),
                ('M', 'ogr.OFTString', 'ogr.OFSTNone', 0)]

    if got != expected:
        import pprint
        pp = pprint.PrettyPrinter()
        pp.pprint(got)
        assert False


def test_fits_vector_write_with_source_fits_metadata():

    filename = 'tmp/out.fits'
    with gdaltest.error_handler():
        gdal.VectorTranslate(filename, 'data/fits/binary_table.fits', options='-f FITS')
    try:
        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 3
        lyr_defn = lyr.GetLayerDefn()
        _check_lyr_defn_after_write(lyr_defn)

        md = lyr.GetMetadata()
        assert md['TFORM13'].rstrip() == '2J'
        assert md['TFORM16'].rstrip() == '1X'
        assert md['TFORM17'].rstrip() == '33X'
        assert md['TFORM23'].rstrip() == '2I'
        assert md['TFORM29'].rstrip() == '2K'
        assert md['TFORM32'].rstrip() == '2A'
        assert md['TFORM35'].rstrip() == '2E'
        assert md['TFORM38'].rstrip() == '2D'
        assert md['TFORM40'].rstrip() == 'C'
        assert md['TFORM41'].rstrip() == 'M'

        expected_f1 = [
            -128,
            5.5,
            0,
            -49149.5,
            0,
            -3221225469.5,
            -1.3835058055282164e+19,
            4.375,
            4.375,
            '4.375 + 5.875j',
            '4.375 + 5.875j',
            0,
            [0, 0],
            [1, 0],
            [1, 0],
            1,
            1,
            1,
            1,
            1,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            1,
            0,
            [255, 0],
            [255, 0],
            [0, 255, 0, 255, 0, 255],
            -32768,
            [-32768, 32767],
            [-32768, 32767],
            -2147483648,
            [-2147483648, 2147483647],
            [-2147483648, 2147483647],
            -9223372036854775808,
            [-9223372036854775808, 9223372036854775807],
            [-9223372036854775808, 9223372036854775807],
            'A',
            'AB',
            'AB',
            # ['AB', 'ab', 'Ab'],
            1.25,
            [1.25, 2.25],
            [1.25, 2.25],
            1.2534,
            [1.2534, 2.25],
            [1.2534, 2.25],
            '1.25 + 2.25j',
            # ['1.25 + 2.25j', '2.25 + 1.25j'],
            # ['1.25 + 2.25j', '2.25 + 1.25j'],
            '1.25340000000000007 + 2.25j',
            # ['1.25340000000000007 + 2.25j', '2.25 + 1.25j'],
            # ['1.25340000000000007 + 2.25j', '2.25 + 1.25j']
        ]

        f = lyr.GetNextFeature()
        got = [f.GetField(i) for i in range(f.GetFieldCount())]
        assert got == expected_f1

    except:
        ds = None
        os.unlink(filename)
        raise


def test_fits_vector_write_without_source_fits_metadata():

    filename = 'tmp/out.fits'
    with gdaltest.error_handler():
        gdal.VectorTranslate(filename, 'data/fits/binary_table.fits', options='-f FITS -nomd')
    try:
        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 3
        lyr_defn = lyr.GetLayerDefn()
        assert lyr_defn.GetFieldCount() == 73
        _check_lyr_defn_after_write(lyr_defn)

        md = lyr.GetMetadata()
        assert md['TFORM13'].rstrip() == 'PJ(2)'
        assert md['TFORM16'].rstrip() == '1X'
        assert md['TFORM17'].rstrip() == '33X'
        assert md['TFORM23'].rstrip() == 'PI(2)'
        assert md['TFORM29'].rstrip() == 'PK(2)'
        assert md['TFORM32'].rstrip() == '2A'  # from field width
        assert md['TFORM35'].rstrip() == 'PE(2)'
        assert md['TFORM38'].rstrip() == 'PD(2)'
        assert md['TFORM40'].rstrip().startswith('PA(')  # not recognized as complex
        assert md['TFORM41'].rstrip().startswith('PA(')  # not recognized as complex

        expected_f1 = [
            -128,
            5.5,
            0,
            -49149.5,
            0,
            -3221225469.5,
            -1.3835058055282164e+19,
            4.375,
            4.375,
            '4.375 + 5.875j',
            '4.375 + 5.875j',
            0,
            [0, 0],
            [1, 0],
            [1, 0],
            1,
            1,
            1,
            1,
            1,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            1,
            0,
            [255, 0],
            [255, 0],
            [0, 255, 0, 255, 0, 255],
            -32768,
            [-32768, 32767],
            [-32768, 32767],
            -2147483648,
            [-2147483648, 2147483647],
            [-2147483648, 2147483647],
            -9223372036854775808,
            [-9223372036854775808, 9223372036854775807],
            [-9223372036854775808, 9223372036854775807],
            'A',
            'AB',
            'AB',
            # ['AB', 'ab', 'Ab'],
            1.25,
            [1.25, 2.25],
            [1.25, 2.25],
            1.2534,
            [1.2534, 2.25],
            [1.2534, 2.25],
            '1.25 + 2.25j',
            # ['1.25 + 2.25j', '2.25 + 1.25j'],
            # ['1.25 + 2.25j', '2.25 + 1.25j'],
            '1.25340000000000007 + 2.25j',
            # ['1.25340000000000007 + 2.25j', '2.25 + 1.25j'],
            # ['1.25340000000000007 + 2.25j', '2.25 + 1.25j']
        ]

        f = lyr.GetNextFeature()
        got = [f.GetField(i) for i in range(f.GetFieldCount())]
        assert got == expected_f1

    except:
        ds = None
        os.unlink(filename)
        raise


def test_fits_vector_write_without_source_fits_metadata_compute_repeat():

    filename = 'tmp/out.fits'
    with gdaltest.error_handler():
        gdal.VectorTranslate(filename, 'data/fits/binary_table.fits', options='-f FITS -nomd -lco COMPUTE_REPEAT=AT_FIRST_FEATURE_CREATION -lco REPEAT_2E=3')
    try:
        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 3
        lyr_defn = lyr.GetLayerDefn()
        _check_lyr_defn_after_write(lyr_defn)

        md = lyr.GetMetadata()
        assert md['TFORM13'].rstrip() == '2J'
        assert md['TFORM16'].rstrip() == '1X'
        assert md['TFORM17'].rstrip() == '33X'
        assert md['TFORM23'].rstrip() == '2I'
        assert md['TFORM29'].rstrip() == '2K'
        assert md['TFORM32'].rstrip() == '2A'  # from field width
        assert md['TFORM35'].rstrip() == '3E'  # should normally be 2E, but overridden by REPEAT_2E=3
        assert md['TFORM38'].rstrip() == '2D'
        assert md['TFORM40'].rstrip().startswith('PA(')  # not recognized as complex
        assert md['TFORM41'].rstrip().startswith('PA(')  # not recognized as complex

        expected_f1 = [
            -128,
            5.5,
            0,
            -49149.5,
            0,
            -3221225469.5,
            -1.3835058055282164e+19,
            4.375,
            4.375,
            '4.375 + 5.875j',
            '4.375 + 5.875j',
            0,
            [0, 0],
            [1, 0],
            [1, 0],
            1,
            1,
            1,
            1,
            1,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            0,
            1,
            1,
            0,
            [255, 0],
            [255, 0],
            [0, 255, 0, 255, 0, 255],
            -32768,
            [-32768, 32767],
            [-32768, 32767],
            -2147483648,
            [-2147483648, 2147483647],
            [-2147483648, 2147483647],
            -9223372036854775808,
            [-9223372036854775808, 9223372036854775807],
            [-9223372036854775808, 9223372036854775807],
            'A',
            'AB',
            'AB',
            # ['AB', 'ab', 'Ab'],
            1.25,
            [1.25, 2.25, 0],
            [1.25, 2.25],
            1.2534,
            [1.2534, 2.25],
            [1.2534, 2.25],
            '1.25 + 2.25j',
            # ['1.25 + 2.25j', '2.25 + 1.25j'],
            # ['1.25 + 2.25j', '2.25 + 1.25j'],
            '1.25340000000000007 + 2.25j',
            # ['1.25340000000000007 + 2.25j', '2.25 + 1.25j'],
            # ['1.25340000000000007 + 2.25j', '2.25 + 1.25j']
        ]

        f = lyr.GetNextFeature()
        got = [f.GetField(i) for i in range(f.GetFieldCount())]
        assert got == expected_f1

    except:
        ds = None
        os.unlink(filename)
        raise


def test_fits_vector_editing():

    filename = 'tmp/out.fits'
    with gdaltest.error_handler():
        gdal.VectorTranslate(filename, 'data/fits/binary_table.fits', options='-f FITS -nomd')
    try:
        ds = ogr.Open(filename, update = 1)
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 3
        f = lyr.GetNextFeature()
        f['A2'] = 'XY'
        assert lyr.SetFeature(f) == ogr.OGRERR_NONE

        f = lyr.GetFeature(1)
        assert f['A2'] == 'XY'

        f = None

        assert lyr.CreateField(ogr.FieldDefn('new_field', ogr.OFTReal)) == ogr.OGRERR_NONE

        f = lyr.GetFeature(1)
        f.SetFID(-1)
        f['new_field'] = 1.25
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        assert lyr.GetFeatureCount() == 4
        assert f.GetFID() == 4
        f = lyr.GetFeature(4)
        assert f['A2'] == 'XY'
        assert f['new_field'] == 1.25

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(0)
        assert lyr.SetFeature(f) == ogr.OGRERR_NON_EXISTING_FEATURE
        f.SetFID(5)
        assert lyr.SetFeature(f) == ogr.OGRERR_NON_EXISTING_FEATURE

        assert lyr.DeleteFeature(0) == ogr.OGRERR_NON_EXISTING_FEATURE
        assert lyr.DeleteFeature(5) == ogr.OGRERR_NON_EXISTING_FEATURE
        assert lyr.DeleteFeature(1) == ogr.OGRERR_NONE
        assert lyr.GetFeatureCount() == 3

    except:
        ds = None
        os.unlink(filename)
        raise
