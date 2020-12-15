#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test STACTA driver
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

import gdaltest
import pytest
import struct

from osgeo import gdal

pytestmark = pytest.mark.require_driver('STACTA')


def test_stacta_basic():

    ds = gdal.Open('data/stacta/test.json')
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 2048
    assert ds.RasterYSize == 1024
    assert ds.GetSpatialRef().GetName() == 'WGS 84'
    assert ds.GetGeoTransform() == pytest.approx([-180.0, 0.17578125, 0.0, 90.0, 0.0, -0.17578125], rel=1e-8)
    assert ds.GetRasterBand(1).GetNoDataValue() == 0.0
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert len(ds.GetSubDatasets()) == 0

    # Create a reference dataset, that is externally the same as the STACTA one
    vrt_ds = gdal.BuildVRT('', [ 'data/stacta/WorldCRS84Quad/2/0/0.tif', 'data/stacta/WorldCRS84Quad/2/0/1.tif'])
    ref_ds = gdal.Translate('', vrt_ds, format = 'MEM')
    ref_ds.BuildOverviews('NEAR', [2, 4])

    # Whole dataset reading
    assert ds.ReadRaster() == ref_ds.ReadRaster()

    # Whole band reading
    assert ds.GetRasterBand(2).ReadRaster() == \
       ref_ds.GetRasterBand(2).ReadRaster()

    # Subwindow intersecting 2 tiles
    assert ds.ReadRaster(1000, 500, 50, 100) == \
       ref_ds.ReadRaster(1000, 500, 50, 100)

    # Subwindow intersecting 2 tiles with downsampling, but at the same zoom level
    assert ds.ReadRaster(1000, 500, 50, 100, 30, 60) == \
       ref_ds.ReadRaster(1000, 500, 50, 100, 30, 60)

    # Subwindow intersecting 2 tiles with downsampling, but at another zoom level
    assert ds.ReadRaster(1000, 500, 50, 100, 10, 20) == \
       ref_ds.ReadRaster(1000, 500, 50, 100, 10, 20)

    # Subwindow intersecting 2 tiles with downsampling, but at another zoom level
    assert ds.GetRasterBand(1).ReadRaster(1000, 500, 50, 100, 10, 20) == \
       ref_ds.GetRasterBand(1).ReadRaster(1000, 500, 50, 100, 10, 20)

    # Same as above but with bilinear resampling
    assert ds.ReadRaster(1000, 500, 50, 100, 30, 60, resample_alg=gdal.GRIORA_Bilinear) == \
       ref_ds.ReadRaster(1000, 500, 50, 100, 30, 60, resample_alg=gdal.GRIORA_Bilinear)

    # Downsampling with floating point coordinates, intersecting one tile
    assert ds.ReadRaster(0.5, 500.5, 50.25, 100.25, 30, 60, resample_alg=gdal.GRIORA_Bilinear) == \
       ref_ds.ReadRaster(0.5, 500.5, 50.25, 100.25, 30, 60, resample_alg=gdal.GRIORA_Bilinear)

    # Downsampling with floating point coordinates, intersecting two tiles
    assert ds.ReadRaster(0.5, 500.5, 50.25, 100.25, 30, 60, resample_alg=gdal.GRIORA_Bilinear) == \
       ref_ds.ReadRaster(0.5, 500.5, 50.25, 100.25, 30, 60, resample_alg=gdal.GRIORA_Bilinear)


def test_stacta_east_hemisphere():

    # Test a json file with min_tile_col = 1 at zoom level 2
    ds = gdal.OpenEx('data/stacta/test_east_hemisphere.json', open_options = ['WHOLE_METATILE=YES'])
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 1024
    assert ds.RasterYSize == 1024
    assert ds.GetSpatialRef().GetName() == 'WGS 84'
    assert ds.GetGeoTransform() == pytest.approx([0.0, 0.17578125, 0.0, 90.0, 0.0, -0.17578125], rel=1e-8)
    assert ds.GetRasterBand(1).GetOverviewCount() == 2

    # Create a reference dataset, that is externally the same as the STACTA one
    vrt_ds = gdal.BuildVRT('', [ 'data/stacta/WorldCRS84Quad/2/0/1.tif'])
    ref_ds = gdal.Translate('', vrt_ds, format = 'MEM')
    ref_ds.BuildOverviews('NEAR', [2, 4])

    assert ds.ReadRaster(600, 500, 50, 100, 10, 20) == \
       ref_ds.ReadRaster(600, 500, 50, 100, 10, 20)


def test_stacta_subdatasets():

    ds = gdal.Open('data/stacta/test_multiple_asset_templates.json')
    assert len(ds.GetSubDatasets()) == 2
    subds1 = gdal.Open(ds.GetSubDatasets()[0][0])
    assert subds1 is not None
    subds2 = gdal.Open(ds.GetSubDatasets()[1][0])
    assert subds2 is not None
    assert subds1.GetRasterBand(1).ReadRaster() != subds2.GetRasterBand(1).ReadRaster()


    ds = gdal.Open('data/stacta/test_multiple_tms.json')
    assert len(ds.GetSubDatasets()) == 2
    subds1 = gdal.Open(ds.GetSubDatasets()[0][0])
    assert subds1 is not None


def test_stacta_missing_metatile():

    gdal.FileFromMemBuffer('/vsimem/stacta/test.json', open('data/stacta/test.json', 'rb').read())
    gdal.FileFromMemBuffer('/vsimem/stacta/WorldCRS84Quad/0/0/0.tif', open('data/stacta/WorldCRS84Quad/0/0/0.tif', 'rb').read())
    gdal.FileFromMemBuffer('/vsimem/stacta/WorldCRS84Quad/1/0/0.tif', open('data/stacta/WorldCRS84Quad/1/0/0.tif', 'rb').read())
    gdal.FileFromMemBuffer('/vsimem/stacta/WorldCRS84Quad/2/0/0.tif', open('data/stacta/WorldCRS84Quad/2/0/0.tif', 'rb').read())

    ds = gdal.Open('/vsimem/stacta/test.json')
    with gdaltest.error_handler():
        assert ds.ReadRaster() is None

    # Missing right tile
    with gdaltest.config_option('GDAL_STACTA_SKIP_MISSING_METATILE', 'YES'):
        ds = gdal.Open('/vsimem/stacta/test.json')
        got_data = ds.ReadRaster()
        assert got_data is not None
        got_data = struct.unpack('B' * len(got_data), got_data)
        for i in range(3):
            assert got_data[i * 2048 * 1024 + 2048 * 1000 + 500] != 0
            assert got_data[i * 2048 * 1024 + 2048 * 1000 + 1500] == 0

    gdal.Unlink('/vsimem/stacta/WorldCRS84Quad/1/0/0.tif')
    gdal.Unlink('/vsimem/stacta/WorldCRS84Quad/2/0/0.tif')
    gdal.FileFromMemBuffer('/vsimem/stacta/WorldCRS84Quad/2/0/1.tif', open('data/stacta/WorldCRS84Quad/2/0/1.tif', 'rb').read())

    # Missing left tile
    with gdaltest.config_option('GDAL_STACTA_SKIP_MISSING_METATILE', 'YES'):
        ds = gdal.Open('/vsimem/stacta/test.json')
        got_data = ds.ReadRaster()
        assert got_data is not None
        got_data = struct.unpack('B' * len(got_data), got_data)
        for i in range(3):
            assert got_data[i * 2048 * 1024 + 2048 * 1000 + 500] == 0
            assert got_data[i * 2048 * 1024 + 2048 * 1000 + 1500] != 0

    gdal.Unlink('/vsimem/stacta/test.json')
    gdal.Unlink('/vsimem/stacta/WorldCRS84Quad/1/0/0.tif')
    gdal.Unlink('/vsimem/stacta/WorldCRS84Quad/2/0/1.tif')
