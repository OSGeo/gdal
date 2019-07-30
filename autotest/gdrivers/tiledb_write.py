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
def test_tiledb_write_complex():
    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    src_ds = gdal.Open('../gcore/data/cfloat64.tif')

    new_ds = gdaltest.tiledb_drv.CreateCopy('tmp/tiledb_complex64', src_ds)

    bnd = new_ds.GetRasterBand(1)
    assert bnd.Checksum() == 5028, 'Didnt get expected checksum on still-open file'

    bnd = None
    new_ds = None
    gdaltest.tiledb_drv.Delete('tmp/tiledb_complex64')

@pytest.mark.require_driver('TileDB')
def test_tiledb_write_custom_blocksize():
    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    src_ds = gdal.Open('../gcore/data/utmsmall.tif')

    options = ['BLOCKXSIZE=32', 'BLOCKYSIZE=32']

    new_ds = gdaltest.tiledb_drv.CreateCopy('tmp/tiledb_custom', src_ds,
                                          options=options)

    bnd = new_ds.GetRasterBand(1)
    assert bnd.Checksum() == 50054, 'Didnt get expected checksum on still-open file'
    assert bnd.GetBlockSize() == [32, 32]

    bnd = None
    new_ds = None

    gdaltest.tiledb_drv.Delete('tmp/tiledb_custom')

@pytest.mark.require_driver('TileDB')
def test_tiledb_write_update():
    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    src_ds = gdal.Open('../gcore/data/byte.tif')
    array = src_ds.ReadAsArray()

    new_ds = gdaltest.tiledb_drv.Create('tmp/tiledb_update', src_ds.RasterYSize,
                                         src_ds.RasterXSize)
    del new_ds

    update_ds = gdal.Open('tmp/tiledb_update', gdal.GA_Update)
    update_bnd = update_ds.GetRasterBand(1)    
    # this write is in TileDB row major order
    update_bnd.WriteArray(array)
    update_bnd = None
    update_ds = None

    test_ds = gdal.Open('tmp/tiledb_update')
    assert test_ds.GetRasterBand(1).Checksum() == 4672, 'Didnt get expected checksum on file update' 
    test_ds = None

    gdaltest.tiledb_drv.Delete('tmp/tiledb_update')

@pytest.mark.require_driver('TileDB')
def test_tiledb_write_rgb():
    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    src_ds = gdal.Open('../gcore/data/rgbsmall.tif')

    new_ds = gdaltest.tiledb_drv.CreateCopy('tmp/tiledb_rgb', src_ds)

    assert new_ds.RasterCount == 3, 'Didnt get expected band count'
    bnd = new_ds.GetRasterBand(2)
    assert bnd.Checksum() == 21053, 'Didnt get expected checksum on still-open file'

    new_ds = None

    gdaltest.tiledb_drv.Delete('tmp/tiledb_rgb')    

@pytest.mark.require_driver('TileDB')
def test_tiledb_write_attributes():
    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    src_ds = gdal.Open('../gcore/data/rgbsmall.tif')
    w, h, num_bands = src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount

    # build attribute data in memory
    gdal.GetDriverByName('GTiff').Create('/vsimem/temp1.tif', w, h, num_bands, gdal.GDT_Int32)
    gdal.GetDriverByName('GTiff').Create('/vsimem/temp2.tif', w, h, num_bands, gdal.GDT_Float32)

    options = [
        'TILEDB_ATTRIBUTE=%s' % ('/vsimem/temp1.tif'),
        'TILEDB_ATTRIBUTE=%s' % ('/vsimem/temp2.tif')
    ]

    new_ds = gdaltest.tiledb_drv.CreateCopy('tmp/tiledb_rgb_atts', src_ds, options=options)
    assert new_ds is not None
    assert new_ds.RasterXSize == src_ds.RasterXSize
    assert new_ds.RasterYSize == src_ds.RasterYSize
    assert new_ds.RasterCount == src_ds.RasterCount

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

    src_ds = None
    att1_ds = None
    att2_ds = None
    val_ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/temp1.tif')
    gdal.GetDriverByName('GTiff').Delete('/vsimem/temp2.tif')
    gdaltest.tiledb_drv.Delete('tmp/tiledb_rgb_atts')


@pytest.mark.require_driver('TileDB')
@pytest.mark.require_driver('HDF5')
def test_tiledb_write_subdatasets():
    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    src_ds = gdal.Open('data/DeepBlue-SeaWiFS-1.0_L3_20100101_v004-20130604T131317Z.h5')

    new_ds = gdaltest.tiledb_drv.CreateCopy('tmp/test_sds_array', src_ds,
                                        False)

    assert new_ds is not None
    new_ds = None
    src_ds = None

    src_ds = gdal.Open('tmp/test_sds_array')
    assert 'tmp/test_sds_array/test_sds_array.tdb.aux.xml' in src_ds.GetFileList()
    src_ds = None
  
    src_ds = gdal.Open('TILEDB:"tmp/test_sds_array":viewing_zenith_angle')
    assert src_ds.GetRasterBand(1).Checksum() == 42472
    src_ds = None

    src_ds = gdal.Open('TILEDB:"tmp/test_sds_array":i_dont_exist')
    assert not src_ds

    gdaltest.tiledb_drv.Delete('tmp/test_sds_array')

@pytest.mark.require_driver('TileDB')
def test_tiledb_write_band_meta():
    gdaltest.tiledb_drv = gdal.GetDriverByName('TileDB')

    src_ds = gdal.Open('../gcore/data/rgbsmall.tif')

    new_ds = gdaltest.tiledb_drv.CreateCopy('tmp/tiledb_meta', src_ds)

    bnd = new_ds.GetRasterBand(1)
    bnd.SetMetadataItem('Item', 'Value')

    bnd = None
    new_ds = None
    src_ds = None
    
    new_ds = gdal.Open('tmp/tiledb_meta')
    assert new_ds.GetRasterBand(1).GetMetadataItem('Item') == 'Value'

    gdaltest.tiledb_drv.Delete('tmp/tiledb_meta')