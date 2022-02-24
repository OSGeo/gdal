#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  COG driver testing
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
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
import struct
import sys

from osgeo import gdal
from osgeo import osr

import gdaltest
from test_py_scripts import samples_path

###############################################################################


def _check_cog(filename):

    path = samples_path
    if path not in sys.path:
        sys.path.append(path)
    import validate_cloud_optimized_geotiff
    try:
        _, errors, _ = validate_cloud_optimized_geotiff.validate(filename, full_check=True)
        assert not errors, 'validate_cloud_optimized_geotiff failed'
    except OSError:
        pytest.fail('validate_cloud_optimized_geotiff failed')


###############################################################################

def check_libtiff_internal_or_at_least(expected_maj, expected_min, expected_micro):

    md = gdal.GetDriverByName('GTiff').GetMetadata()
    if md['LIBTIFF'] == 'INTERNAL':
        return True
    if md['LIBTIFF'].startswith('LIBTIFF, Version '):
        version = md['LIBTIFF'][len('LIBTIFF, Version '):]
        version = version[0:version.find('\n')]
        got_maj, got_min, got_micro = version.split('.')
        got_maj = int(got_maj)
        got_min = int(got_min)
        got_micro = int(got_micro)
        if got_maj > expected_maj:
            return True
        if got_maj < expected_maj:
            return False
        if got_min > expected_min:
            return True
        if got_min < expected_min:
            return False
        return got_micro >= expected_micro
    return False

###############################################################################
# Basic test


def test_cog_basic():

    tab = [ 0 ]
    def my_cbk(pct, _, arg):
        assert pct >= tab[0]
        tab[0] = pct
        return 1

    filename = '/vsimem/cog.tif'
    src_ds = gdal.Open('data/byte.tif')
    assert src_ds.GetMetadataItem('GDAL_STRUCTURAL_METADATA', 'TIFF') is None

    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                callback = my_cbk,
                                                callback_data = tab)
    src_ds = None
    assert tab[0] == 1.0
    assert ds
    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetMetadataItem('LAYOUT', 'IMAGE_STRUCTURE') == 'COG'
    assert ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE') == 'LZW'
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetRasterBand(1).GetBlockSize() == [512, 512]
    assert ds.GetMetadataItem('GDAL_STRUCTURAL_METADATA', 'TIFF') == """GDAL_STRUCTURAL_METADATA_SIZE=000140 bytes
LAYOUT=IFDS_BEFORE_DATA
BLOCK_ORDER=ROW_MAJOR
BLOCK_LEADER=SIZE_AS_UINT4
BLOCK_TRAILER=LAST_4_BYTES_REPEATED
KNOWN_INCOMPATIBLE_EDITION=NO
 """
    ds = None
    _check_cog(filename)
    gdal.GetDriverByName('GTiff').Delete(filename)


###############################################################################
# Test creation options


def test_cog_creation_options():

    filename = '/vsimem/cog.tif'
    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                options = ['COMPRESS=DEFLATE',
                                                           'LEVEL=1',
                                                           'NUM_THREADS=2'])
    assert ds
    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE') == 'DEFLATE'
    assert ds.GetMetadataItem('PREDICTOR', 'IMAGE_STRUCTURE') is None
    ds = None
    filesize = gdal.VSIStatL(filename).size
    _check_cog(filename)

    gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                options = ['COMPRESS=DEFLATE',
                                                           'BIGTIFF=YES',
                                                           'LEVEL=1'])
    assert gdal.VSIStatL(filename).size != filesize

    gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                options = ['COMPRESS=DEFLATE',
                                                           'PREDICTOR=YES',
                                                           'LEVEL=1'])
    assert gdal.VSIStatL(filename).size != filesize
    ds = gdal.Open(filename)
    assert ds.GetMetadataItem('PREDICTOR', 'IMAGE_STRUCTURE') == '2'
    ds = None

    gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                options = ['COMPRESS=DEFLATE',
                                                           'LEVEL=9'])
    assert gdal.VSIStatL(filename).size < filesize

    colist = gdal.GetDriverByName('COG').GetMetadataItem('DMD_CREATIONOPTIONLIST')
    if '<Value>ZSTD' in colist:

        gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                    options = ['COMPRESS=ZSTD'])
        ds = gdal.Open(filename)
        assert ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE') == 'ZSTD'
        ds = None

    if '<Value>LZMA' in colist:

        gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                    options = ['COMPRESS=LZMA'])
        ds = gdal.Open(filename)
        assert ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE') == 'LZMA'
        ds = None

    if '<Value>WEBP' in colist:

        with gdaltest.error_handler():
            assert not gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                    options = ['COMPRESS=WEBP'])

    if '<Value>LERC' in colist:

        assert gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                options = ['COMPRESS=LERC'])
        filesize_no_z_error = gdal.VSIStatL(filename).size
        assert gdal.VSIStatL(filename).size != filesize

        assert gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                options = ['COMPRESS=LERC', 'MAX_Z_ERROR=10'])
        filesize_with_z_error = gdal.VSIStatL(filename).size
        assert filesize_with_z_error < filesize_no_z_error

        assert gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                options = ['COMPRESS=LERC_DEFLATE'])
        filesize_lerc_deflate = gdal.VSIStatL(filename).size
        assert filesize_lerc_deflate < filesize_no_z_error

        assert gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                options = ['COMPRESS=LERC_DEFLATE', 'LEVEL=1'])
        filesize_lerc_deflate_level_1 = gdal.VSIStatL(filename).size
        assert filesize_lerc_deflate_level_1 > filesize_lerc_deflate

        if '<Value>ZSTD' in colist:
            assert gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                    options = ['COMPRESS=LERC_ZSTD'])
            filesize_lerc_zstd = gdal.VSIStatL(filename).size
            assert filesize_lerc_zstd < filesize_no_z_error

            assert gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                    options = ['COMPRESS=LERC_ZSTD', 'LEVEL=1'])
            filesize_lerc_zstd_level_1 = gdal.VSIStatL(filename).size
            assert filesize_lerc_zstd_level_1 > filesize_lerc_zstd

    src_ds = None
    with gdaltest.error_handler():
        gdal.GetDriverByName('GTiff').Delete(filename)


###############################################################################
# Test creation of overviews


def test_cog_creation_of_overviews():

    tab = [ 0 ]
    def my_cbk(pct, _, arg):
        assert pct >= tab[0]
        tab[0] = pct
        return 1

    directory = '/vsimem/test_cog_creation_of_overviews'
    filename = directory + '/cog.tif'
    src_ds = gdal.Translate('', 'data/byte.tif',
                            options='-of MEM -outsize 2048 300')

    with gdaltest.config_option('GDAL_TIFF_INTERNAL_MASK', 'YES'):
        check_filename = '/vsimem/tmp.tif'
        ds = gdal.GetDriverByName('GTiff').CreateCopy(check_filename, src_ds,
                                                      options = ['TILED=YES'])
        ds.BuildOverviews('CUBIC', [2, 4])
        cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
        cs2 = ds.GetRasterBand(1).GetOverview(1).Checksum()
        ds = None
        gdal.Unlink(check_filename)

    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                callback = my_cbk,
                                                callback_data = tab)
    assert tab[0] == 1.0
    assert ds
    assert len(gdal.ReadDir(directory)) == 1 # check that the temp file has gone away

    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == cs1
    assert ds.GetRasterBand(1).GetOverview(1).Checksum() == cs2
    ds = None
    _check_cog(filename)

    src_ds = None
    gdal.GetDriverByName('GTiff').Delete(filename)
    gdal.Unlink(directory)

###############################################################################
# Test creation of overviews with a different compression method

def test_cog_creation_of_overviews_with_compression():
    directory = '/vsimem/test_cog_creation_of_overviews_with_compression'
    filename = directory + '/cog.tif'
    src_ds = gdal.Translate('', 'data/byte.tif',
                            options='-of MEM -outsize 2048 300')

    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                            options = ['COMPRESS=LZW', 'OVERVIEW_COMPRESS=JPEG', 'OVERVIEW_QUALITY=50'])

    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert ds.GetMetadata('IMAGE_STRUCTURE')['COMPRESSION'] == 'LZW'

    ds_overview_a = gdal.Open('GTIFF_DIR:2:' + filename)
    assert ds_overview_a.GetMetadata('IMAGE_STRUCTURE')['COMPRESSION'] == 'JPEG'
    assert ds_overview_a.GetMetadata('IMAGE_STRUCTURE')['JPEG_QUALITY'] == '50'

    ds_overview_b = gdal.Open('GTIFF_DIR:3:' + filename)
    assert ds_overview_b.GetMetadata('IMAGE_STRUCTURE')['COMPRESSION'] == 'JPEG'
    assert ds_overview_a.GetMetadata('IMAGE_STRUCTURE')['JPEG_QUALITY'] == '50'

    ds_overview_a = None
    ds_overview_b = None
    ds = None

    src_ds = None
    gdal.GetDriverByName('GTiff').Delete(filename)
    gdal.Unlink(directory)


###############################################################################
# Test creation of overviews with a dataset with a mask


def test_cog_creation_of_overviews_with_mask():

    tab = [ 0 ]
    def my_cbk(pct, _, arg):
        assert pct >= tab[0]
        tab[0] = pct
        return 1

    directory = '/vsimem/test_cog_creation_of_overviews_with_mask'
    gdal.Mkdir(directory, 0o755)
    filename = directory + '/cog.tif'
    src_ds = gdal.Translate('', 'data/byte.tif',
                            options='-of MEM -outsize 2048 300')
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 1024, 300, b'\xFF',
                                                      buf_xsize = 1, buf_ysize = 1)

    with gdaltest.config_option('GDAL_TIFF_INTERNAL_MASK', 'YES'):
        check_filename = '/vsimem/tmp.tif'
        ds = gdal.GetDriverByName('GTiff').CreateCopy(check_filename, src_ds,
                                                      options = ['TILED=YES'])
        ds.BuildOverviews('CUBIC', [2, 4])
        cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
        cs2 = ds.GetRasterBand(1).GetOverview(1).Checksum()
        ds = None
        gdal.Unlink(check_filename)

    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                callback = my_cbk,
                                                callback_data = tab)
    assert tab[0] == 1.0
    assert ds
    assert len(gdal.ReadDir(directory)) == 1 # check that the temp file has gone away

    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert ds.GetRasterBand(1).GetOverview(0).GetBlockSize() == [512, 512]
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == cs1
    assert ds.GetRasterBand(1).GetOverview(1).Checksum() == cs2
    ds = None
    _check_cog(filename)

    src_ds = None
    gdal.GetDriverByName('GTiff').Delete(filename)
    gdal.Unlink(directory)



###############################################################################
# Test full world reprojection to WebMercator


def test_cog_small_world_to_web_mercator():

    tab = [ 0 ]
    def my_cbk(pct, _, arg):
        assert pct >= tab[0]
        tab[0] = pct
        return 1

    directory = '/vsimem/test_cog_small_world_to_web_mercator'
    gdal.Mkdir(directory, 0o755)
    filename = directory + '/cog.tif'
    src_ds = gdal.Open('../gdrivers/data/small_world.tif')
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
        options = ['TILING_SCHEME=GoogleMapsCompatible', 'COMPRESS=JPEG'],
        callback = my_cbk,
        callback_data = tab)
    assert tab[0] == 1.0
    assert ds
    assert len(gdal.ReadDir(directory)) == 1 # check that the temp file has gone away

    ds = None
    ds = gdal.Open(filename)
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 256
    assert ds.RasterYSize == 256
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    assert ds.GetRasterBand(1).GetBlockSize() == [256, 256]
    gt = ds.GetGeoTransform()
    assert gt[1] == -gt[5] # yes, checking for strict equality
    expected_gt = [-20037508.342789248, 156543.033928041, 0.0,
                   20037508.342789248, 0.0, -156543.033928041]
    for i in range(6):
        if gt[i] != pytest.approx(expected_gt[i], abs=1e-10 * abs(expected_gt[i])):
            assert False, gt
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    assert got_cs in ([26293, 23439, 14955],
                      [26228, 22085, 12992],
                      [25088, 23140, 13265], # libjpeg 9e
                     )
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 17849
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    ds = None
    _check_cog(filename)

    src_ds = None
    gdal.GetDriverByName('GTiff').Delete(filename)
    gdal.Unlink(directory)



###############################################################################
# Test reprojection of small extent to WebMercator


def test_cog_byte_to_web_mercator():

    tab = [ 0 ]
    def my_cbk(pct, _, arg):
        assert pct >= tab[0]
        tab[0] = pct
        return 1

    directory = '/vsimem/test_cog_byte_to_web_mercator'
    gdal.Mkdir(directory, 0o755)
    filename = directory + '/cog.tif'
    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
        options = ['TILING_SCHEME=GoogleMapsCompatible', 'ALIGNED_LEVELS=3'],
        callback = my_cbk,
        callback_data = tab)
    assert tab[0] == 1.0
    assert ds
    assert len(gdal.ReadDir(directory)) == 1 # check that the temp file has gone away

    ds = None
    ds = gdal.Open(filename)
    assert ds.RasterCount == 2
    assert ds.RasterXSize == 1024
    assert ds.RasterYSize == 1024
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALPHA + gdal.GMF_PER_DATASET
    assert ds.GetRasterBand(1).GetBlockSize() == [256,256]
    gt = ds.GetGeoTransform()
    assert gt[1] == -gt[5] # yes, checking for strict equality
    expected_gt = [-13149614.849955443, 76.43702828517598, 0.0,
                   4070118.8821290657, 0.0, -76.43702828517598]
    for i in range(6):
        if gt[i] != pytest.approx(expected_gt[i], abs=1e-10 * abs(expected_gt[i])):
            assert False, gt
    assert ds.GetRasterBand(1).Checksum() in (4363,
                                              4264, # got on Mac at some point
                                              4362, # libjpeg 9d
                                              4569, # libjpeg 9e
                                             )
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 4356
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    ds = None
    _check_cog(filename)

    # Use our generated COG as the input of the same COG generation: reprojection
    # should be skipped
    filename2 = directory + '/cog2.tif'
    src_ds = gdal.Open(filename)

    class my_error_handler(object):
        def __init__(self):
            self.debug_msg_list = []
            self.other_msg_list = []

        def handler(self, eErrClass, err_no, msg):
            if eErrClass == gdal.CE_Debug:
                self.debug_msg_list.append(msg)
            else:
                self.other_msg_list.append(msg)

    handler = my_error_handler();
    try:
        gdal.PushErrorHandler(handler.handler)
        gdal.SetCurrentErrorHandlerCatchDebug(True)
        with gdaltest.config_option('CPL_DEBUG', 'COG'):
            ds = gdal.GetDriverByName('COG').CreateCopy(filename2, src_ds,
                options = ['TILING_SCHEME=GoogleMapsCompatible', 'ALIGNED_LEVELS=3'])
    finally:
        gdal.PopErrorHandler()

    assert ds
    assert 'COG: Skipping reprojection step: source dataset matches reprojection specifications' in handler.debug_msg_list
    assert handler.other_msg_list == []
    src_ds = None
    ds = None

    # Cleanup
    gdal.GetDriverByName('GTiff').Delete(filename)
    gdal.GetDriverByName('GTiff').Delete(filename2)
    gdal.Unlink(directory)




###############################################################################
# Same as previous test case but with other input options


def test_cog_byte_to_web_mercator_manual():

    directory = '/vsimem/test_cog_byte_to_web_mercator_manual'
    gdal.Mkdir(directory, 0o755)
    filename = directory + '/cog.tif'
    src_ds = gdal.Open('data/byte.tif')
    res = 76.43702828517598
    minx = -13149614.849955443
    maxx = minx + 1024 * res
    maxy = 4070118.8821290657
    miny = maxy - 1024 * res
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
        options = ['BLOCKSIZE=256',
                   'TARGET_SRS=EPSG:3857',
                   'RES=%.18g' % res,
                   'EXTENT=%.18g,%.18g,%.18g,%.18g' % (minx,miny,maxx,maxy)])
    assert ds

    ds = None
    ds = gdal.Open(filename)
    assert ds.RasterCount == 2
    assert ds.RasterXSize == 1024
    assert ds.RasterYSize == 1024
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALPHA + gdal.GMF_PER_DATASET
    assert ds.GetRasterBand(1).GetBlockSize() == [256,256]
    gt = ds.GetGeoTransform()
    expected_gt = [-13149614.849955443, 76.43702828517598, 0.0,
                   4070118.8821290657, 0.0, -76.43702828517598]
    for i in range(6):
        if gt[i] != pytest.approx(expected_gt[i], abs=1e-10 * abs(expected_gt[i])):
            assert False, gt
    assert ds.GetRasterBand(1).Checksum() in (4363,
                                              4264, # got on Mac at some point
                                              4362, # libjpeg 9d
                                              4569, # libjpeg 9e
                                             )
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 4356
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    ds = None

    src_ds = None
    gdal.GetDriverByName('GTiff').Delete(filename)
    gdal.Unlink(directory)



###############################################################################
# Test OVERVIEWS creation option


def test_cog_overviews_co():

    def my_cbk(pct, _, arg):
        assert pct >= tab[0]
        tab[0] = pct
        return 1

    directory = '/vsimem/test_cog_overviews_co'
    filename = directory + '/cog.tif'
    src_ds = gdal.Translate('', 'data/byte.tif',
                            options='-of MEM -outsize 2048 300')

    for val in ['NONE', 'FORCE_USE_EXISTING']:

        tab = [ 0 ]
        ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                    options = ['OVERVIEWS=' + val],
                                                    callback = my_cbk,
                                                    callback_data = tab)
        assert tab[0] == 1.0
        assert ds

        ds = None
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
        assert ds.GetRasterBand(1).GetOverviewCount() == 0
        ds = None
        _check_cog(filename)

    for val in ['AUTO', 'IGNORE_EXISTING']:

        tab = [ 0 ]
        ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                    options = ['OVERVIEWS=' + val],
                                                    callback = my_cbk,
                                                    callback_data = tab)
        assert tab[0] == 1.0
        assert ds

        ds = None
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
        assert ds.GetRasterBand(1).GetOverviewCount() == 2
        assert ds.GetRasterBand(1).GetOverview(0).Checksum() != 0
        ds = None
        _check_cog(filename)


    # Add overviews to source
    src_ds.BuildOverviews('NONE', [2])

    tab = [ 0 ]
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                options = ['OVERVIEWS=NONE'],
                                                callback = my_cbk,
                                                callback_data = tab)
    assert tab[0] == 1.0
    assert ds

    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    ds = None
    _check_cog(filename)

    tab = [ 0 ]
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                options = ['OVERVIEWS=FORCE_USE_EXISTING'],
                                                callback = my_cbk,
                                                callback_data = tab)
    assert tab[0] == 1.0
    assert ds

    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 0
    ds = None
    _check_cog(filename)

    tab = [ 0 ]
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                options = ['OVERVIEWS=IGNORE_EXISTING'],
                                                callback = my_cbk,
                                                callback_data = tab)
    assert tab[0] == 1.0
    assert ds

    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() != 0
    ds = None
    _check_cog(filename)


    src_ds = None
    gdal.GetDriverByName('GTiff').Delete(filename)
    gdal.Unlink(directory)

###############################################################################
# Test editing and invalidating a COG file


def test_cog_invalidation_by_data_change():

    filename = '/vsimem/cog.tif'
    src_ds = gdal.GetDriverByName('MEM').Create('', 100, 100)
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                options = ['COMPRESS=DEFLATE'])
    ds = None

    ds = gdal.Open(filename, gdal.GA_Update)
    assert ds.GetMetadataItem('LAYOUT', 'IMAGE_STRUCTURE') == 'COG'
    src_ds = gdal.Open('data/byte.tif')
    data = src_ds.ReadRaster()
    ds.GetRasterBand(1).WriteRaster(0, 0, 20, 20, data)
    with gdaltest.error_handler():
        ds.FlushCache()
    ds = None

    with gdaltest.error_handler():
        ds = gdal.Open(filename)
    assert ds.GetMetadataItem('LAYOUT', 'IMAGE_STRUCTURE') is None
    ds = None

    with pytest.raises(AssertionError, match='KNOWN_INCOMPATIBLE_EDITION=YES is declared in the file'):
        _check_cog(filename)

    with gdaltest.error_handler():
        gdal.GetDriverByName('GTiff').Delete(filename)

###############################################################################
# Test editing and invalidating a COG file


def test_cog_invalidation_by_metadata_change():

    filename = '/vsimem/cog.tif'
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                options = ['COMPRESS=DEFLATE'])
    ds = None

    ds = gdal.Open(filename, gdal.GA_Update)
    ds.GetRasterBand(1).ComputeStatistics(False)
    ds = None

    with gdaltest.error_handler():
        ds = gdal.Open(filename)
    assert ds.GetMetadataItem('LAYOUT', 'IMAGE_STRUCTURE') is None
    ds = None

    with gdaltest.error_handler():
        gdal.GetDriverByName('GTiff').Delete(filename)


###############################################################################
# Test a tiling scheme with a CRS with northing/easting axis order
# and non power-of-two ratios of scales.


def test_cog_northing_easting_and_non_power_of_two_ratios():

    filename = '/vsimem/cog.tif'

    x0_NZTM2000 = -1000000
    y0_NZTM2000 = 10000000
    blocksize = 256
    scale_denom_zoom_level_14 = 1000
    scale_denom_zoom_level_13 = 2500
    scale_denom_zoom_level_12 = 5000

    ds = gdal.Translate(filename, 'data/byte.tif',
        options='-of COG -a_srs EPSG:2193 -a_ullr 1000001 5000001 1000006.6 4999995.4 -co TILING_SCHEME=NZTM2000 -co ALIGNED_LEVELS=2')
    assert ds.RasterXSize == 1280
    assert ds.RasterYSize == 1280
    b = ds.GetRasterBand(1)
    assert [(b.GetOverview(i).XSize, b.GetOverview(i).YSize) for i in range(b.GetOverviewCount())] == [(512, 512), (256, 256)]

    gt = ds.GetGeoTransform()
    assert gt[1] == -gt[5] # yes, checking for strict equality

    res_zoom_level_14 = scale_denom_zoom_level_14 * 0.28e-3 # According to OGC Tile Matrix Set formula
    assert gt == pytest.approx((999872, res_zoom_level_14, 0, 5000320, 0, -res_zoom_level_14), abs=1e-8)

    # Check that gt origin matches the corner of a tile at zoom 14
    res = gt[1]
    tile_x = (gt[0] - x0_NZTM2000) / (blocksize * res)
    assert tile_x == pytest.approx(round(tile_x))
    tile_y = (y0_NZTM2000 - gt[3]) / (blocksize * res)
    assert tile_y == pytest.approx(round(tile_y))

    # Check that overview=0 corresponds to the resolution of zoom level=13 / OGC ScaleDenom = 2500
    ovr0_xsize = b.GetOverview(0).XSize
    assert float(ovr0_xsize) / ds.RasterXSize == float(scale_denom_zoom_level_14) / scale_denom_zoom_level_13
    # Check that gt origin matches the corner of a tile at zoom 13
    ovr0_res = res * scale_denom_zoom_level_13 / scale_denom_zoom_level_14
    tile_x = (gt[0] - x0_NZTM2000) / (blocksize * ovr0_res)
    assert tile_x == pytest.approx(round(tile_x))
    tile_y = (y0_NZTM2000 - gt[3]) / (blocksize * ovr0_res)
    assert tile_y == pytest.approx(round(tile_y))

    # Check that overview=1 corresponds to the resolution of zoom level=12 / OGC ScaleDenom = 5000
    ovr1_xsize = b.GetOverview(1).XSize
    assert float(ovr1_xsize) / ds.RasterXSize == float(scale_denom_zoom_level_14) / scale_denom_zoom_level_12
    # Check that gt origin matches the corner of a tile at zoom 12
    ovr1_res = res * scale_denom_zoom_level_12 / scale_denom_zoom_level_14
    tile_x = (gt[0] - x0_NZTM2000) / (blocksize * ovr1_res)
    assert tile_x == pytest.approx(round(tile_x))
    tile_y = (y0_NZTM2000 - gt[3]) / (blocksize * ovr1_res)
    assert tile_y == pytest.approx(round(tile_y))

    assert ds.GetMetadata("TILING_SCHEME") == {
        "NAME": "NZTM2000",
        "ZOOM_LEVEL": "14",
        "ALIGNED_LEVELS": "2"
    }

    ds = None
    gdal.GetDriverByName('GTiff').Delete(filename)


###############################################################################
# Test SPARSE_OK=YES


def test_cog_sparse():

    filename = '/vsimem/cog.tif'
    src_ds = gdal.GetDriverByName('MEM').Create('', 512, 512)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.WriteRaster(0, 0, 256, 256, '\x00' * 256 * 256)
    src_ds.WriteRaster(256, 256, 128, 128, '\x00' * 128 * 128)
    src_ds.BuildOverviews('NEAREST', [2])
    gdal.GetDriverByName('COG').CreateCopy(filename, src_ds, options = ['BLOCKSIZE=128', 'SPARSE_OK=YES', 'COMPRESS=LZW'])
    _check_cog(filename)
    with gdaltest.config_option('GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE', 'YES'):
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_1_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_2_0', 'TIFF') is not None
        assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('BLOCK_OFFSET_1_0', 'TIFF') is not None
        assert ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256) == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256)

        if check_libtiff_internal_or_at_least(4, 0, 11):
            # This file is the same as the one generated above, except that we have,
            # with an hex editor, zeroify all entries of TileByteCounts except the
            # last tile of the main IFD, and for a tile when the next tile is sparse
            ds = gdal.Open('data/cog_sparse_strile_arrays_zeroified_when_possible.tif')
            assert ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512)

    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test SPARSE_OK=YES with mask


def test_cog_sparse_mask():

    filename = '/vsimem/cog.tif'
    src_ds = gdal.GetDriverByName('MEM').Create('', 512, 512, 4)
    for i in range(4):
        src_ds.GetRasterBand(i+1).SetColorInterpretation(gdal.GCI_RedBand + i)
        src_ds.GetRasterBand(i+1).Fill(255)
        src_ds.GetRasterBand(i+1).WriteRaster(0, 0, 256, 256, '\x00' * 256 * 256)
        src_ds.GetRasterBand(i+1).WriteRaster(256, 256, 128, 128, '\x00' * 128 * 128)
    src_ds.BuildOverviews('NEAREST', [2])
    gdal.GetDriverByName('COG').CreateCopy(filename, src_ds, options = ['BLOCKSIZE=128', 'SPARSE_OK=YES', 'COMPRESS=JPEG', 'RESAMPLING=NEAREST'])
    assert gdal.GetLastErrorMsg() == ''
    _check_cog(filename)
    with gdaltest.config_option('GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE', 'YES'):
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_1_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_2_0', 'TIFF') is not None
        assert ds.GetRasterBand(1).GetMaskBand().GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetMaskBand().GetMetadataItem('BLOCK_OFFSET_1_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetMaskBand().GetMetadataItem('BLOCK_OFFSET_2_0', 'TIFF') is not None
        assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('BLOCK_OFFSET_1_0', 'TIFF') is not None
        assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().GetMetadataItem('BLOCK_OFFSET_1_0', 'TIFF') is not None
        assert ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetMaskBand().ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(4).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256) == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256)
        assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().ReadRaster(0, 0, 256, 256) == src_ds.GetRasterBand(4).GetOverview(0).ReadRaster(0, 0, 256, 256)

    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test SPARSE_OK=YES with imagery at 0 and mask at 255


def test_cog_sparse_imagery_0_mask_255():

    filename = '/vsimem/cog.tif'
    src_ds = gdal.GetDriverByName('MEM').Create('', 512, 512, 4)
    for i in range(4):
        src_ds.GetRasterBand(i+1).SetColorInterpretation(gdal.GCI_RedBand + i)
        src_ds.GetRasterBand(i+1).Fill(0 if i < 3 else 255)
    src_ds.BuildOverviews('NEAREST', [2])
    gdal.GetDriverByName('COG').CreateCopy(filename, src_ds, options = ['BLOCKSIZE=128', 'SPARSE_OK=YES', 'COMPRESS=JPEG'])
    _check_cog(filename)
    with gdaltest.config_option('GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE', 'YES'):
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetMaskBand().GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is not None
        assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is not None
        assert ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetMaskBand().ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(4).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256) == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256)
        assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().ReadRaster(0, 0, 256, 256) == src_ds.GetRasterBand(4).GetOverview(0).ReadRaster(0, 0, 256, 256)

    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test SPARSE_OK=YES with imagery at 0 or 255 and mask at 255


def test_cog_sparse_imagery_0_or_255_mask_255():

    filename = '/vsimem/cog.tif'
    src_ds = gdal.GetDriverByName('MEM').Create('', 512, 512, 4)
    for i in range(4):
        src_ds.GetRasterBand(i+1).SetColorInterpretation(gdal.GCI_RedBand + i)
    for i in range(3):
        src_ds.GetRasterBand(i+1).Fill(255)
        src_ds.GetRasterBand(i+1).WriteRaster(0, 0, 256, 256, '\x00' * 256 * 256)
        src_ds.GetRasterBand(i+1).WriteRaster(256, 256, 128, 128, '\x00' * 128 * 128)
    src_ds.GetRasterBand(4).Fill(255)
    src_ds.BuildOverviews('NEAREST', [2])
    gdal.GetDriverByName('COG').CreateCopy(filename, src_ds, options = ['BLOCKSIZE=128', 'SPARSE_OK=YES', 'COMPRESS=JPEG', 'RESAMPLING=NEAREST'])
    _check_cog(filename)
    with gdaltest.config_option('GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE', 'YES'):
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_2_0', 'TIFF') is not None
        assert ds.GetRasterBand(1).GetMaskBand().GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is not None
        assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is not None
        assert ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetMaskBand().ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(4).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256) == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256)
        assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().ReadRaster(0, 0, 256, 256) == src_ds.GetRasterBand(4).GetOverview(0).ReadRaster(0, 0, 256, 256)

    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test SPARSE_OK=YES with imagery and mask at 0


def test_cog_sparse_imagery_mask_0():

    filename = '/vsimem/cog.tif'
    src_ds = gdal.GetDriverByName('MEM').Create('', 512, 512, 4)
    for i in range(4):
        src_ds.GetRasterBand(i+1).SetColorInterpretation(gdal.GCI_RedBand + i)
        src_ds.GetRasterBand(i+1).Fill(0)
    src_ds.BuildOverviews('NEAREST', [2])
    gdal.GetDriverByName('COG').CreateCopy(filename, src_ds, options = ['BLOCKSIZE=128', 'SPARSE_OK=YES', 'COMPRESS=JPEG'])
    _check_cog(filename)
    with gdaltest.config_option('GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE', 'YES'):
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetMaskBand().GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().GetMetadataItem('BLOCK_OFFSET_0_0', 'TIFF') is None
        assert ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetMaskBand().ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(4).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256) == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256)
        assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().ReadRaster(0, 0, 256, 256) == src_ds.GetRasterBand(4).GetOverview(0).ReadRaster(0, 0, 256, 256)

    ds = None
    gdal.Unlink(filename)



###############################################################################
# Test ZOOM_LEVEL_STRATEGY option

@pytest.mark.parametrize('zoom_level_strategy,expected_gt',
    [('AUTO', (-13110479.09147343, 76.43702828517416, 0.0, 4030983.1236470547, 0.0, -76.43702828517416)),
     ('LOWER', (-13110479.09147343, 76.43702828517416, 0.0, 4030983.1236470547, 0.0, -76.43702828517416)),
     ('UPPER', (-13100695.151852928, 38.21851414258708, 0.0, 4021199.1840265524, 0.0, -38.21851414258708))
    ])
def test_cog_zoom_level_strategy(zoom_level_strategy,expected_gt):

    filename = '/vsimem/test_cog_zoom_level_strategy.tif'
    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
        options = ['TILING_SCHEME=GoogleMapsCompatible',
                   'ZOOM_LEVEL_STRATEGY=' + zoom_level_strategy])
    gt = ds.GetGeoTransform()
    assert gt == pytest.approx(expected_gt, rel=1e-10)

    # Test that the zoom level strategy applied on input data already on a
    # zoom level doesn't lead to selecting another zoom level
    filename2 = '/vsimem/test_cog_zoom_level_strategy_2.tif'
    src_ds = gdal.Open('data/byte.tif')
    ds2 = gdal.GetDriverByName('COG').CreateCopy(filename2, ds,
        options = ['TILING_SCHEME=GoogleMapsCompatible',
                    'ZOOM_LEVEL_STRATEGY=' + zoom_level_strategy])
    gt = ds2.GetGeoTransform()
    assert gt == pytest.approx(expected_gt, rel=1e-10)
    ds2 = None
    gdal.Unlink(filename2)

    ds = None
    gdal.Unlink(filename)



###############################################################################

def test_cog_resampling_options():

    filename = '/vsimem/test_cog_resampling_options.tif'
    src_ds = gdal.Open('data/byte.tif')

    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
        options = ['TILING_SCHEME=GoogleMapsCompatible', 'WARP_RESAMPLING=NEAREST'])
    cs1 = ds.GetRasterBand(1).Checksum()

    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
        options = ['TILING_SCHEME=GoogleMapsCompatible', 'WARP_RESAMPLING=CUBIC'])
    cs2 = ds.GetRasterBand(1).Checksum()

    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
        options = ['TILING_SCHEME=GoogleMapsCompatible', 'RESAMPLING=NEAREST', 'WARP_RESAMPLING=CUBIC'])
    cs3 = ds.GetRasterBand(1).Checksum()

    assert cs1 != cs2
    assert cs2 == cs3

    src_ds = gdal.Translate('', 'data/byte.tif', options='-of MEM -outsize 129 0')
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
        options = ['BLOCKSIZE=128', 'OVERVIEW_RESAMPLING=NEAREST'])
    cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()

    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
        options = ['BLOCKSIZE=128','OVERVIEW_RESAMPLING=BILINEAR'])
    cs2 = ds.GetRasterBand(1).GetOverview(0).Checksum()

    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
        options = ['BLOCKSIZE=128','RESAMPLING=NEAREST', 'OVERVIEW_RESAMPLING=BILINEAR'])
    cs3 = ds.GetRasterBand(1).GetOverview(0).Checksum()

    assert cs1 != cs2
    assert cs2 == cs3

    ds = None
    gdal.Unlink(filename)


###############################################################################

def test_cog_invalid_warp_resampling():

    filename = '/vsimem/test_cog_invalid_warp_resampling.tif'
    src_ds = gdal.Open('data/byte.tif')

    with gdaltest.error_handler():
        assert gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
            options = ['TILING_SCHEME=GoogleMapsCompatible', 'RESAMPLING=INVALID']) is None
    gdal.Unlink(filename)


###############################################################################

def test_cog_overview_size():

    src_ds = gdal.GetDriverByName('MEM').Create('', 20480 // 4, 40960 // 4)
    src_ds.SetGeoTransform([1723840, 7 * 4, 0, 5555840, 0, -7 * 4])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(2193)
    src_ds.SetProjection(srs.ExportToWkt())
    filename = '/vsimem/test_cog_overview_size.tif'
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds, options = ['TILING_SCHEME=NZTM2000', 'ALIGNED_LEVELS=4', 'OVERVIEW_RESAMPLING=NONE'])
    assert (ds.RasterXSize, ds.RasterYSize) == (20480 // 4, 40960 // 4)
    ovr_size = [ (ds.GetRasterBand(1).GetOverview(i).XSize, ds.GetRasterBand(1).GetOverview(i).YSize) for i in range(ds.GetRasterBand(1).GetOverviewCount()) ]
    assert ovr_size == [(2048, 4096), (1024, 2048), (512, 1024), (256, 512), (128, 256)]
    gdal.Unlink(filename)


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/2946


def test_cog_float32_color_table():

    src_ds = gdal.GetDriverByName('MEM').Create('', 1024, 1024, 1, gdal.GDT_Float32)
    src_ds.GetRasterBand(1).Fill(1.0)
    ct = gdal.ColorTable()
    src_ds.GetRasterBand(1).SetColorTable(ct)
    filename = '/vsimem/test_cog_float32_color_table.tif'
    # Silence warning about color table not being copied
    with gdaltest.error_handler():
        ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds) # segfault
    assert ds
    assert ds.GetRasterBand(1).GetColorTable() is None
    assert struct.unpack('f', ds.ReadRaster(0,0,1,1))[0] == 1.0
    assert struct.unpack('f', ds.GetRasterBand(1).GetOverview(0).ReadRaster(0,0,1,1))[0] == 1.0
    gdal.Unlink(filename)

###############################################################################
# Test copy XMP


def test_cog_copy_xmp():

    filename = '/vsimem/cog_xmp.tif'
    src_ds = gdal.Open('../gdrivers/data/gtiff/byte_with_xmp.tif')
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds)
    assert ds
    ds = None

    ds = gdal.Open(filename)
    xmp = ds.GetMetadata('xml:XMP')
    ds = None
    assert 'W5M0MpCehiHzreSzNTczkc9d' in xmp[0], 'Wrong input file without XMP'
    _check_cog(filename)

    gdal.Unlink(filename)

###############################################################################
# Test creating COG from a source dataset that has overview with 'odd' sizes
# and a mask without overview


def test_cog_odd_overview_size_and_msk():

    filename = '/vsimem/test_cog_odd_overview_size_and_msk.tif'
    src_ds = gdal.GetDriverByName('MEM').Create('', 511, 511)
    src_ds.BuildOverviews('NEAR', [2])
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds, options=['BLOCKSIZE=256'])
    assert ds
    assert ds.GetRasterBand(1).GetOverview(0).XSize == 256
    assert ds.GetRasterBand(1).GetMaskBand().GetOverview(0).XSize == 256
    ds = None

    gdal.Unlink(filename)
