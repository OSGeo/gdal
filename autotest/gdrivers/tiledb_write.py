#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for GeoTIFF format.
# Author:   TileDB, Inc
#
###############################################################################
# Copyright (c) 2019, TileDB, Inc
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
import pytest

import gdaltest

@pytest.mark.require_driver('TileDB')
@pytest.mark.parametrize(
    'mode',
    ['BAND', 'PIXEL', 'ATTRIBUTES']
)
def test_tiledb_write_complex(mode):
    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    src_ds = gdal.Open('../gcore/data/cfloat64.tif')

    options = [
        'INTERLEAVE=%s' % (mode)
    ]

    new_ds = gdaltest.tiledb_drv.CreateCopy('tmp/tiledb_complex64', src_ds, options=options)
    meta = new_ds.GetMetadata('IMAGE_STRUCTURE')
    assert meta['INTERLEAVE'] == mode, 'Didn\'t get expected mode'

    bnd = new_ds.GetRasterBand(1)
    assert bnd.Checksum() == 5028, 'Didn\'t get expected checksum on still-open file'

    bnd = None
    new_ds = None
    gdaltest.tiledb_drv.Delete('tmp/tiledb_complex64')

@pytest.mark.require_driver('TileDB')
@pytest.mark.parametrize(
    'mode',
    ['BAND', 'PIXEL', 'ATTRIBUTES']
)
def test_tiledb_write_custom_blocksize(mode):
    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    src_ds = gdal.Open('../gcore/data/utmsmall.tif')

    options = ['BLOCKXSIZE=32', 'BLOCKYSIZE=32', 'INTERLEAVE=%s' % (mode)]

    new_ds = gdaltest.tiledb_drv.CreateCopy('tmp/tiledb_custom', src_ds,
                                        options=options)
    meta = new_ds.GetMetadata('IMAGE_STRUCTURE')
    assert meta['INTERLEAVE'] == mode, 'Didn\'t get expected mode'

    bnd = new_ds.GetRasterBand(1)
    assert bnd.Checksum() == 50054, 'Didn\'t get expected checksum on still-open file'
    assert bnd.GetBlockSize() == [32, 32]

    bnd = None
    new_ds = None

    gdaltest.tiledb_drv.Delete('tmp/tiledb_custom')

@pytest.mark.require_driver('TileDB')
@pytest.mark.parametrize(
    'mode',
    ['BAND', 'PIXEL']
)
def test_tiledb_write_update(mode):
    np = pytest.importorskip('numpy')

    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    options = [
        'INTERLEAVE=%s' % (mode)
    ]

    new_ds = gdaltest.tiledb_drv.Create('tmp/tiledb_update', 20, 20, 1, gdal.GDT_Byte, options=options)
    new_ds.GetRasterBand(1).WriteArray(np.zeros((20, 20)))
    meta = new_ds.GetMetadata('IMAGE_STRUCTURE')
    assert meta['INTERLEAVE'] == mode, 'Didn\'t get expected mode'
    del new_ds

    update_ds = gdal.Open('tmp/tiledb_update', gdal.GA_Update)
    update_bnd = update_ds.GetRasterBand(1)
    # make a partial block write
    update_bnd.WriteArray(np.ones((10, 10)) * 255)
    update_bnd = None
    update_ds = None

    test_ds = gdal.Open('tmp/tiledb_update')
    assert test_ds.GetRasterBand(1).Checksum() == 1217, 'Didn\'t get expected checksum on file update' 
    test_ds = None

    gdaltest.tiledb_drv.Delete('tmp/tiledb_update')

@pytest.mark.require_driver('TileDB')
@pytest.mark.parametrize(
    'mode',
    ['BAND', 'PIXEL', 'ATTRIBUTES']
)
def test_tiledb_write_rgb(mode):
    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    src_ds = gdal.Open('../gcore/data/rgbsmall.tif')

    options = [
        'INTERLEAVE=%s' % (mode)
    ]
    new_ds = gdaltest.tiledb_drv.CreateCopy('tmp/tiledb_rgb', src_ds, options=options)
    meta = new_ds.GetMetadata('IMAGE_STRUCTURE')
    assert meta['INTERLEAVE'] == mode, 'Didn\'t get expected mode'
    assert new_ds.RasterCount == 3, 'Didn\'t get expected band count'
    bnd = new_ds.GetRasterBand(2)
    assert bnd.Checksum() == 21053, 'Didn\'t get expected checksum on still-open file'

    new_ds = None

    gdaltest.tiledb_drv.Delete('tmp/tiledb_rgb')

@pytest.mark.require_driver('TileDB')
@pytest.mark.parametrize(
    'mode',
    ['BAND', 'PIXEL']
)
def test_tiledb_write_attributes(mode):
    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    src_ds = gdal.Open('../gcore/data/rgbsmall.tif')
    w, h, num_bands = src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount

    # build attribute data in memory
    gdal.GetDriverByName('GTiff').Create('/vsimem/temp1.tif', w, h, num_bands, gdal.GDT_Int32)
    gdal.GetDriverByName('GTiff').Create('/vsimem/temp2.tif', w, h, num_bands, gdal.GDT_Float32)

    options = [
        'TILEDB_ATTRIBUTE=%s' % ('/vsimem/temp1.tif'),
        'TILEDB_ATTRIBUTE=%s' % ('/vsimem/temp2.tif'),
        'INTERLEAVE=%s' % (mode)
    ]

    new_ds = gdaltest.tiledb_drv.CreateCopy('tmp/tiledb_rgb_atts', src_ds, options=options)
    assert new_ds is not None
    assert new_ds.RasterXSize == src_ds.RasterXSize
    assert new_ds.RasterYSize == src_ds.RasterYSize
    assert new_ds.RasterCount == src_ds.RasterCount
    meta = new_ds.GetMetadata('IMAGE_STRUCTURE')
    assert meta['INTERLEAVE'] == mode, 'Didn\'t get expected mode'

    new_ds = None

    # check we can open the attributes with the band as well as the pixel values
    att1_ds = gdal.OpenEx('tmp/tiledb_rgb_atts', open_options=['TILEDB_ATTRIBUTE=temp1'])
    att2_ds = gdal.OpenEx('tmp/tiledb_rgb_atts', open_options=['TILEDB_ATTRIBUTE=temp2'])
    val_ds = gdal.Open('tmp/tiledb_rgb_atts')

    meta = val_ds.GetMetadata('IMAGE_STRUCTURE')
    assert 'TILEDB_ATTRIBUTE_1' in meta
    assert meta['TILEDB_ATTRIBUTE_1'] == 'temp1'
    assert 'TILEDB_ATTRIBUTE_2' in meta
    assert meta['TILEDB_ATTRIBUTE_2'] == 'temp2'

    assert att1_ds is not None
    assert att2_ds is not None
    assert val_ds is not None

    assert att1_ds.GetRasterBand(1).DataType == gdal.GDT_Int32
    assert att2_ds.GetRasterBand(1).DataType == gdal.GDT_Float32
    assert val_ds.RasterCount == src_ds.RasterCount
    assert val_ds.GetRasterBand(1).DataType == src_ds.GetRasterBand(1).DataType

    att1_ds = None
    att2_ds = None
    val_ds = None

    gdaltest.tiledb_drv.Delete('tmp/tiledb_rgb_atts')

    src_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/temp1.tif')
    gdal.GetDriverByName('GTiff').Delete('/vsimem/temp2.tif')


@pytest.mark.require_driver('TileDB')
@pytest.mark.require_driver('HDF5')
def test_tiledb_write_subdatasets():
    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    src_ds = gdal.Open('data/tiledb_input/DeepBlue-SeaWiFS-1.0_L3_20100101_v004-20130604T131317Z.h5')

    new_ds = gdaltest.tiledb_drv.CreateCopy('tmp/test_sds_array', src_ds,
                                        False)

    assert new_ds is not None
    new_ds = None
    src_ds = None

    src_ds = gdal.Open('TILEDB:"tmp/test_sds_array":viewing_zenith_angle')
    assert src_ds.GetRasterBand(1).Checksum() == 42472
    src_ds = None

    src_ds = gdal.Open('TILEDB:"tmp/test_sds_array":i_dont_exist')
    assert not src_ds

    gdaltest.tiledb_drv.Delete('tmp/test_sds_array')

@pytest.mark.require_driver('TileDB')
@pytest.mark.parametrize(
    'mode',
    ['BAND', 'PIXEL', 'ATTRIBUTES']
)
def test_tiledb_write_band_meta(mode):
    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    src_ds = gdal.Open('../gcore/data/rgbsmall.tif')

    options = [
        'INTERLEAVE=%s' % (mode)
    ]

    new_ds = gdaltest.tiledb_drv.CreateCopy('tmp/tiledb_meta', src_ds, options=options)

    meta = new_ds.GetMetadata('IMAGE_STRUCTURE')
    assert meta['INTERLEAVE'] == mode, 'Didn\'t get expected mode'

    bnd = new_ds.GetRasterBand(1)
    bnd.SetMetadataItem('Item', 'Value')

    bnd = None
    new_ds = None

    new_ds = gdal.Open('tmp/tiledb_meta')
    assert new_ds.GetRasterBand(1).GetMetadataItem('Item') == 'Value'

    gdaltest.tiledb_drv.Delete('tmp/tiledb_meta')

    src_ds = None

@pytest.mark.require_driver('TileDB')
@pytest.mark.parametrize(
    'mode',
    ['BAND', 'PIXEL']
)
def test_tiledb_write_history(mode):
    np = pytest.importorskip('numpy')

    options = [
        'INTERLEAVE=%s' % (mode),
        'TILEDB_TIMESTAMP=1'
    ]

    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    new_ds = gdaltest.tiledb_drv.Create('tmp/tiledb_versioning', 20, 20, 1,
                                         gdal.GDT_Byte, options=options)
    new_ds.GetRasterBand(1).WriteArray(np.zeros((20, 20)))

    meta = new_ds.GetMetadata('IMAGE_STRUCTURE')
    assert meta['INTERLEAVE'] == mode, 'Didn\'t get expected mode'
    del new_ds

    ts = [2, 3, 4, 5]

    for t in ts:
        update_ds = gdal.OpenEx('tmp/tiledb_versioning', gdal.GA_Update,
                                open_options=['TILEDB_TIMESTAMP=%i' % (t)])
        update_bnd = update_ds.GetRasterBand(1)
        update_bnd.SetMetadataItem('TILEDB_TIMESTAMP', str(t))
        data = np.ones((20, 20)) * t
        update_bnd.WriteArray(data)
        update_bnd = None
        update_ds = None

    for t in ts:
        ds = gdal.OpenEx('tmp/tiledb_versioning',
                         open_options=['TILEDB_TIMESTAMP=%i' % (t)])
        bnd = ds.GetRasterBand(1)
        assert int(bnd.GetMetadataItem('TILEDB_TIMESTAMP')) == t
        assert bnd.Checksum() == 20 * 20 * t
        bnd = None
        ds = None

    # open at a later non-existent timestamp
    ds = gdal.OpenEx('tmp/tiledb_versioning',
                      open_options=['TILEDB_TIMESTAMP=6'])
    bnd = ds.GetRasterBand(1)
    assert int(bnd.GetMetadataItem('TILEDB_TIMESTAMP')) == 5
    bnd = None
    ds = None

    gdaltest.tiledb_drv.Delete('tmp/tiledb_versioning')
