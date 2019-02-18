#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for FITS driver.
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
#
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at mines-paris dot org>
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

import pytest

from osgeo import gdal


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

    ds = gdal.Open('../gdrivers/data/offscale_byte.tif')
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

