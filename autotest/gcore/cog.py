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
import sys

from osgeo import gdal

import gdaltest

sys.path.append('../../gdal/swig/python/samples') # For validate_cloud_optimized_geotiff

###############################################################################


def _check_cog(filename):

    import validate_cloud_optimized_geotiff
    try:
        _, errors, _ = validate_cloud_optimized_geotiff.validate(filename, full_check=True)
        assert not errors, 'validate_cloud_optimized_geotiff failed'
    except OSError:
        pytest.fail('validate_cloud_optimized_geotiff failed')

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
    ds = gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                callback = my_cbk,
                                                callback_data = tab)
    src_ds = None
    assert tab[0] == 1.0
    assert ds
    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE') is None
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetRasterBand(1).GetBlockSize() == [512, 512]
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

    gdal.GetDriverByName('COG').CreateCopy(filename, src_ds,
                                                options = ['COMPRESS=DEFLATE',
                                                           'LEVEL=9'])
    assert gdal.VSIStatL(filename).size < filesize

    src_ds = None
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
    expected_gt = [-20037508.342789248, 156543.033928041, 0.0,
                   20037508.342789248, 0.0, -156543.033928041]
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-10 * abs(expected_gt[i]):
            assert False, gt
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    if sys.platform == 'darwin' and gdal.GetConfigOption('TRAVIS', None) is not None:
        assert got_cs != [0, 0, 0]
    else:
        assert got_cs == [26293, 23439, 14955]
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
    expected_gt = [-13149614.849955443, 76.43702828517598, 0.0,
                   4070118.8821290657, 0.0, -76.43702828517598]
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-10 * abs(expected_gt[i]):
            assert False, gt
    assert ds.GetRasterBand(1).Checksum() == 4363
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 4356
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    ds = None
    _check_cog(filename)

    src_ds = None
    gdal.GetDriverByName('GTiff').Delete(filename)
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
        if abs(gt[i] - expected_gt[i]) > 1e-10 * abs(expected_gt[i]):
            assert False, gt
    assert ds.GetRasterBand(1).Checksum() == 4363
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
