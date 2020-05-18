#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test EXR driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even.rouault@spatialys.com>
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
import pytest

pytestmark = pytest.mark.require_driver('EXR')

def test_exr_byte_createcopy():
    tst = gdaltest.GDALTest('EXR', 'byte.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)


def test_exr_byte_createcopy_pixel_type_half():
    tst = gdaltest.GDALTest('EXR', 'byte.tif', 1, 4672, options = ['PIXEL_TYPE=HALF'])
    return tst.testCreateCopy(vsimem=1)


def test_exr_byte_createcopy_pixel_type_float():
    tst = gdaltest.GDALTest('EXR', 'byte.tif', 1, 4672, options = ['PIXEL_TYPE=FLOAT'])
    return tst.testCreateCopy(vsimem=1)


def test_exr_byte_createcopy_pixel_type_uint():
    tst = gdaltest.GDALTest('EXR', 'byte.tif', 1, 4672, options = ['PIXEL_TYPE=UINT'])
    return tst.testCreateCopy(vsimem=1)


def test_exr_byte_create():
    tst = gdaltest.GDALTest('EXR', 'byte.tif', 1, 4672)
    return tst.testCreate(vsimem=1)


def test_exr_uint16_createcopy():
    tst = gdaltest.GDALTest('EXR', '../../gcore/data/uint16.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)


def test_exr_uint16_create():
    tst = gdaltest.GDALTest('EXR', '../../gcore/data/uint16.tif', 1, 4672)
    return tst.testCreate(vsimem=1)


def test_exr_uint32_createcopy():
    tst = gdaltest.GDALTest('EXR', '../../gcore/data/uint32.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)


def test_exr_uint32_create():
    tst = gdaltest.GDALTest('EXR', '../../gcore/data/uint32.tif', 1, 4672)
    return tst.testCreate(vsimem=1)


def test_exr_float32_createcopy():
    tst = gdaltest.GDALTest('EXR', '../../gcore/data/float32.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)


def test_exr_float32_create():
    tst = gdaltest.GDALTest('EXR', '../../gcore/data/float32.tif', 1, 4672)
    return tst.testCreate(vsimem=1)


def test_exr_float64_createcopy():
    tst = gdaltest.GDALTest('EXR', '../../gcore/data/float64.tif', 1, 4672)
    return tst.testCreateCopy(vsimem=1)


def test_exr_float64_create():
    tst = gdaltest.GDALTest('EXR', '../../gcore/data/float64.tif', 1, 4672)
    return tst.testCreate(vsimem=1)


def test_exr_compression_createcopy():
    src_ds = gdal.Open('data/byte.tif')
    tmpfilename = '/vsimem/temp.exr'
    ds = gdal.GetDriverByName('EXR').CreateCopy(tmpfilename, src_ds,
                                                options = ['COMPRESS=RLE'])
    assert ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE') == 'RLE'
    band = ds.GetRasterBand(1)
    assert band.Checksum() == 4672
    ds = None
    gdal.Unlink(tmpfilename)


def test_exr_compression_create():
    src_ds = gdal.Open('data/byte.tif')
    tmpfilename = '/vsimem/temp.exr'
    ds = gdal.GetDriverByName('EXR').Create(tmpfilename, 20, 20,
                                                options = ['COMPRESS=RLE'])
    ds.GetRasterBand(1).WriteRaster(0, 0, 20, 20,
                                    src_ds.GetRasterBand(1).ReadRaster())
    ds = None
    ds = gdal.Open(tmpfilename)
    assert ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE') == 'RLE'
    band = ds.GetRasterBand(1)
    assert band.Checksum() == 4672
    ds = None
    gdal.Unlink(tmpfilename)


def test_exr_compression_dwa_compression_level():
    src_ds = gdal.Open('data/small_world.tif')
    tmpfilename = '/vsimem/temp.exr'
    ds = gdal.GetDriverByName('EXR').CreateCopy(tmpfilename, src_ds,
                                                options = ['COMPRESS=DWAB',
                                                           'DWA_COMPRESSION_LEVEL=100'])
    assert ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE') == 'DWAB'
    band = ds.GetRasterBand(1)
    assert band.Checksum() in (12863, 12864) # 12864 on s390x
    ds = None
    gdal.Unlink(tmpfilename)


def test_exr_tiling():
    src_ds = gdal.Open('data/byte.tif')
    tmpfilename = '/vsimem/temp.exr'
    ds = gdal.GetDriverByName('EXR').CreateCopy(tmpfilename, src_ds,
                                                options = ['TILED=YES'])
    band = ds.GetRasterBand(1)
    assert band.GetBlockSize() == [256, 256]
    assert band.Checksum() == 4672
    ds = None
    gdal.Unlink(tmpfilename)


def test_exr_tiling_custom_tile_size():
    src_ds = gdal.Open('data/byte.tif')
    tmpfilename = '/vsimem/temp.exr'
    ds = gdal.GetDriverByName('EXR').CreateCopy(tmpfilename, src_ds,
                                                options = ['TILED=YES',
                                                           'BLOCKXSIZE=13',
                                                           'BLOCKYSIZE=15'])
    band = ds.GetRasterBand(1)
    assert band.GetBlockSize() == [13, 15]
    assert band.Checksum() == 4672
    ds = None
    gdal.Unlink(tmpfilename)


def test_exr_rgb_byte_tiled():
    src_ds = gdal.Open('data/small_world.tif')
    tmpfilename = '/vsimem/temp.exr'
    ds = gdal.GetDriverByName('EXR').CreateCopy(tmpfilename, src_ds,
                                                options = ['TILED=YES',
                                                           'BLOCKXSIZE=64',
                                                           'BLOCKYSIZE=32'])
    assert ds.GetRasterBand(1).Checksum() == 12852
    assert ds.GetRasterBand(2).Checksum() == 12226
    assert ds.GetRasterBand(3).Checksum() == 10731
    ds = None
    gdal.Unlink(tmpfilename)


def test_exr_rgb_byte_strip_no_auto_rescale():
    src_ds = gdal.Open('data/small_world.tif')
    tmpfilename = '/vsimem/temp.exr'
    ds = gdal.GetDriverByName('EXR').CreateCopy(tmpfilename, src_ds,
                                                options = ['AUTO_RESCALE=NO'])
    assert [ds.GetRasterBand(i+1).Checksum() for i in range(3)] == \
        [src_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    ds = None
    gdal.Unlink(tmpfilename)


def test_exr_overviews():
    src_ds = gdal.Open('data/small_world.tif')
    tmpfilename = '/vsimem/temp.exr'
    ds = gdal.GetDriverByName('EXR').CreateCopy(tmpfilename, src_ds,
                                                options = ['OVERVIEWS=YES'])
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(-1) is None
    assert ds.GetRasterBand(1).GetOverview(1) is None
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 2666
    ds = None
    gdal.Unlink(tmpfilename)


def test_exr_preview():
    src_ds = gdal.Open('data/small_world.tif')
    tmpfilename = '/vsimem/temp.exr'
    ds = gdal.GetDriverByName('EXR').CreateCopy(tmpfilename, src_ds,
                                                options = ['PREVIEW=YES'])
    assert len(ds.GetSubDatasets()) == 1
    subds_name = ds.GetSubDatasets()[0][0]
    ds = None
    ds = gdal.Open(subds_name)
    assert ds.RasterCount == 4
    assert [ds.GetRasterBand(i+1).Checksum() for i in range(4)] == [51312, 51623, 55830, 61313]
    ds = None
    gdal.Unlink(tmpfilename)
