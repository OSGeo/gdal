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
