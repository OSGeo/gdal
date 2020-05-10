#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for GIF driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
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


import gdaltest

###############################################################################
# Get the GIF driver, and verify a few things about it.


def test_gif_1():

    gdaltest.gif_drv = gdal.GetDriverByName('GIF')
    if gdaltest.gif_drv is None:
        gdaltest.post_reason('GIF driver not found!')
        return 'false'

    # Move the BIGGIF driver after the GIF driver.
    drv = gdal.GetDriverByName('BIGGIF')
    drv.Deregister()
    drv.Register()

    drv_md = gdaltest.gif_drv.GetMetadata()
    if drv_md['DMD_MIMETYPE'] != 'image/gif':
        gdaltest.post_reason('mime type is wrong')
        return 'false'

    
###############################################################################
# Read test of simple byte reference data.


def test_gif_2():

    tst = gdaltest.GDALTest('GIF', 'gif/bug407.gif', 1, 57921)
    return tst.testOpen()

###############################################################################
# Test lossless copying.


def test_gif_3():

    tst = gdaltest.GDALTest('GIF', 'gif/bug407.gif', 1, 57921,
                            options=['INTERLACING=NO'])

    return tst.testCreateCopy()

###############################################################################
# Verify the colormap, and nodata setting for test file.


def test_gif_4():

    ds = gdal.Open('data/gif/bug407.gif')
    cm = ds.GetRasterBand(1).GetRasterColorTable()
    assert cm.GetCount() == 16 and cm.GetColorEntry(0) == (255, 255, 255, 255) and cm.GetColorEntry(1) == (255, 255, 208, 255), \
        'Wrong colormap entries'

    cm = None

    assert ds.GetRasterBand(1).GetNoDataValue() is None, 'Wrong nodata value.'

    md = ds.GetRasterBand(1).GetMetadata()
    assert 'GIF_BACKGROUND' in md and md['GIF_BACKGROUND'] == '0', \
        'background metadata missing.'

###############################################################################
# Test creating an in memory copy.


def test_gif_5():

    tst = gdaltest.GDALTest('GIF', 'byte.tif', 1, 4672)

    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Verify nodata support


def test_gif_6():

    src_ds = gdal.Open('../gcore/data/nodata_byte.tif')

    new_ds = gdaltest.gif_drv.CreateCopy('tmp/nodata_byte.gif', src_ds)
    if new_ds is None:
        gdaltest.post_reason('Create copy operation failure')
        return 'false'

    bnd = new_ds.GetRasterBand(1)
    if bnd.Checksum() != 4440:
        gdaltest.post_reason('Wrong checksum')
        return 'false'

    bnd = None
    new_ds = None
    src_ds = None

    new_ds = gdal.Open('tmp/nodata_byte.gif')

    bnd = new_ds.GetRasterBand(1)
    if bnd.Checksum() != 4440:
        gdaltest.post_reason('Wrong checksum')
        return 'false'

    # NOTE - mloskot: condition may fail as nodata is a float-point number
    nodata = bnd.GetNoDataValue()
    if nodata != 0:
        gdaltest.post_reason('Got unexpected nodata value.')
        return 'false'

    bnd = None
    new_ds = None

    gdaltest.gif_drv.Delete('tmp/nodata_byte.gif')


###############################################################################
# Confirm reading with the BIGGIF driver.

def test_gif_7():

    # Move the GIF driver after the BIGGIF driver.
    drv = gdal.GetDriverByName('GIF')
    drv.Deregister()
    drv.Register()

    tst = gdaltest.GDALTest('BIGGIF', 'gif/bug407.gif', 1, 57921)

    tst.testOpen()

    ds = gdal.Open('data/gif/bug407.gif')
    assert ds is not None

    assert ds.GetDriver().ShortName == 'BIGGIF'

###############################################################################
# Confirm that BIGGIF driver is selected for huge gifs


def test_gif_8():

    # Move the BIGGIF driver after the GIF driver.
    drv = gdal.GetDriverByName('BIGGIF')
    drv.Deregister()
    drv.Register()

    ds = gdal.Open('data/gif/fakebig.gif')
    assert ds is not None

    assert ds.GetDriver().ShortName == 'BIGGIF'

###############################################################################
# Test writing to /vsistdout/


def test_gif_9():

    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('GIF').CreateCopy(
        '/vsistdout_redirect//vsimem/tmp.gif', src_ds)
    assert ds.GetRasterBand(1).Checksum() == 0
    src_ds = None
    ds = None

    ds = gdal.Open('/vsimem/tmp.gif')
    assert ds is not None
    assert ds.GetRasterBand(1).Checksum() == 4672

    gdal.Unlink('/vsimem/tmp.gif')

###############################################################################
# Test interlacing


def test_gif_10():

    tst = gdaltest.GDALTest('GIF', 'byte.tif', 1, 4672,
                            options=['INTERLACING=YES'])

    return tst.testCreateCopy(vsimem=1)

###############################################################################
# Cleanup.


def test_gif_cleanup():
    gdaltest.clean_tmp()



