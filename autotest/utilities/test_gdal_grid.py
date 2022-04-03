#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_grid testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

import sys
import os
import struct
import pytest

sys.path.append('../gcore')

from osgeo import gdal
from osgeo import ogr

import gdaltest
import ogrtest
import test_cli_utilities

# List of output TIFF files that will be created by tests and later deleted
# in test_gdal_grid_cleanup()
outfiles = []

# Path to gdal_grid utility executable
gdal_grid = test_cli_utilities.get_gdal_grid_path()

###############################################################################
#


def test_gdal_grid_1():
    if gdal_grid is None:
        pytest.skip()

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    outfiles.append('tmp/n43.tif')

    try:
        os.remove('tmp/n43.shp')
    except OSError:
        pass
    try:
        os.remove('tmp/n43.dbf')
    except OSError:
        pass
    try:
        os.remove('tmp/n43.shx')
    except OSError:
        pass
    try:
        os.remove('tmp/n43.qix')
    except OSError:
        pass

    # Create an OGR grid from the values of n43.dt0
    ds = gdal.Open('../gdrivers/data/n43.dt0')
    geotransform = ds.GetGeoTransform()

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    shape_ds = shape_drv.CreateDataSource('tmp')
    shape_lyr = shape_ds.CreateLayer('n43')

    data = ds.ReadRaster(0, 0, 121, 121)
    array_val = struct.unpack('h' * 121 * 121, data)
    for j in range(121):
        for i in range(121):
            wkt = 'POINT(%f %f %s)' % (geotransform[0] + (i + .5) * geotransform[1],
                                       geotransform[3] + (j + .5) * geotransform[5],
                                       array_val[j * 121 + i])
            dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
            dst_feat.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
            shape_lyr.CreateFeature(dst_feat)

    dst_feat.Destroy()

    shape_ds.ExecuteSQL('CREATE SPATIAL INDEX ON n43')

    shape_ds.Destroy()

    # Create a GDAL dataset from the previous generated OGR grid
    (_, err) = gdaltest.runexternal_out_and_err(gdal_grid + ' -txe -80.0041667 -78.9958333 -tye 42.9958333 44.0041667 -outsize 121 121 -ot Int16 -a nearest:radius1=0.0:radius2=0.0:angle=0.0 -co TILED=YES -co BLOCKXSIZE=256 -co BLOCKYSIZE=256 tmp/n43.shp ' + outfiles[-1])
    assert (err is None or err == ''), 'got error/warning'

    # We should get the same values as in n43.td0
    ds2 = gdal.Open(outfiles[-1])
    assert ds.GetRasterBand(1).Checksum() == ds2.GetRasterBand(1).Checksum(), \
        ('bad checksum : got %d, expected %d' % (ds.GetRasterBand(1).Checksum(), ds2.GetRasterBand(1).Checksum()))
    assert ds2.GetRasterBand(1).GetNoDataValue() is None, 'did not expect nodata value'

    ds = None
    ds2 = None

###############################################################################
# Test Nearest Neighbour gridding algorithm
def test_gdal_grid_2():
    if gdal_grid is None:
        pytest.skip()

    # Open reference dataset
    ds_ref = gdal.Open('../gcore/data/byte.tif')
    checksum_ref = ds_ref.GetRasterBand(1).Checksum()
    ds_ref = None

    #################
    outfiles.append('tmp/grid_near.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Grid nodes are located exactly in raster nodes.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=0.0:radius2=0.0:angle=0.0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "gcore/data/byte.tif"
    ds = gdal.Open(outfiles[-1])
    if ds.GetRasterBand(1).Checksum() != checksum_ref:
        print('bad checksum : got %d, expected %d' %
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        pytest.fail('bad checksum')
    assert ds.GetRasterBand(1).GetNoDataValue() == 0.0, 'expected a nodata value'
    ds = None

    #################
    outfiles.append('tmp/grid_near_shift.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Now the same, but shift grid nodes a bit in both horizontal and vertical
    # directions.
    gdaltest.runexternal(gdal_grid + ' -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=0.0:radius2=0.0:angle=0.0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "gcore/data/byte.tif"
    ds = gdal.Open(outfiles[-1])
    if ds.GetRasterBand(1).Checksum() != checksum_ref:
        print('bad checksum : got %d, expected %d' %
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        pytest.fail('bad checksum')
    ds = None

@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_3(use_quadtree):
    if gdal_grid is None:
        pytest.skip()

    # Open reference dataset
    ds_ref = gdal.Open('../gcore/data/byte.tif')
    checksum_ref = ds_ref.GetRasterBand(1).Checksum()
    ds_ref = None

    #################
    outfiles.append('tmp/grid_near_search3.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Try the search ellipse larger than the raster cell.
    with gdaltest.config_option('GDAL_GRID_POINT_COUNT_THRESHOLD', '0' if use_quadtree else '1000000000'):
        gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=180.0:radius2=180.0:angle=0.0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "gcore/data/byte.tif"
        ds = gdal.Open(outfiles[-1])
        if ds.GetRasterBand(1).Checksum() != checksum_ref:
            print('bad checksum : got %d, expected %d' %
                (ds.GetRasterBand(1).Checksum(), checksum_ref))
            pytest.fail('bad checksum')
        ds = None

        #################
        outfiles.append('tmp/grid_near_search1.tif')
        try:
            os.remove(outfiles[-1])
        except OSError:
            pass

        # Search ellipse smaller than the raster cell.
        gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=20.0:radius2=20.0:angle=0.0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "gcore/data/byte.tif"
        ds = gdal.Open(outfiles[-1])
        if ds.GetRasterBand(1).Checksum() != checksum_ref:
            print('bad checksum : got %d, expected %d' %
                (ds.GetRasterBand(1).Checksum(), checksum_ref))
            pytest.fail('bad checksum')
        ds = None

        #################
        outfiles.append('tmp/grid_near_shift_search3.tif')
        try:
            os.remove(outfiles[-1])
        except OSError:
            pass

        # Large search ellipse and the grid shift.
        gdaltest.runexternal(gdal_grid + ' -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=180.0:radius2=180.0:angle=0.0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "gcore/data/byte.tif"
        ds = gdal.Open(outfiles[-1])
        if ds.GetRasterBand(1).Checksum() != checksum_ref:
            print('bad checksum : got %d, expected %d' %
                (ds.GetRasterBand(1).Checksum(), checksum_ref))
            pytest.fail('bad checksum')
        ds = None

        #################
        outfiles.append('tmp/grid_near_shift_search1.tif')
        try:
            os.remove(outfiles[-1])
        except OSError:
            pass

        # Small search ellipse and the grid shift.
        gdaltest.runexternal(gdal_grid + ' -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=20.0:radius2=20.0:angle=0.0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "gcore/data/byte.tif"
        ds = gdal.Open(outfiles[-1])
        if ds.GetRasterBand(1).Checksum() != checksum_ref:
            print('bad checksum : got %d, expected %d' %
                (ds.GetRasterBand(1).Checksum(), checksum_ref))
            pytest.fail('bad checksum')
        ds = None

###############################################################################
# Test Inverse Distance to a Power gridding algorithm


def test_gdal_grid_4():
    if gdal_grid is None:
        pytest.skip()

    #################
    # Test generic implementation (no AVX, no SSE)
    outfiles.append('tmp/grid_invdist_generic.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    print('Step 1: Disabling AVX/SSE optimized versions...')
    (_, err) = gdaltest.runexternal_out_and_err(gdal_grid + ' --debug on --config GDAL_USE_AVX NO --config GDAL_USE_SSE NO -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])
    pos = err.find(' threads')
    if pos >= 0:
        pos_blank = err[0:pos - 1].rfind(' ')
        if pos_blank >= 0:
            print('Step 1: %s threads used' % err[pos_blank + 1:pos])

    # We should get the same values as in "ref_data/gdal_invdist.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdist.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from the reference')
    ds_ref = None
    ds = None

    #################
    # Potentially test optimized SSE implementation

    outfiles.append('tmp/grid_invdist_sse.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    print('Step 2: Trying SSE optimized version...')
    (_, err) = gdaltest.runexternal_out_and_err(gdal_grid + ' --debug on --config GDAL_USE_AVX NO -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])
    if 'SSE' in err:
        print('...SSE optimized version used')
    else:
        print('...SSE optimized version NOT used')

    # We should get the same values as in "ref_data/gdal_invdist.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdist.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from the reference')
    ds_ref = None
    ds = None

    #################
    # Potentially test optimized AVX implementation

    outfiles.append('tmp/grid_invdist_avx.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    print('Step 3: Trying AVX optimized version...')
    (_, err) = gdaltest.runexternal_out_and_err(gdal_grid + ' --debug on -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])
    if 'AVX' in err:
        print('...AVX optimized version used')
    else:
        print('...AVX optimized version NOT used')

    # We should get the same values as in "ref_data/gdal_invdist.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdist.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from the reference')
    ds_ref = None
    ds = None

    #################
    # Test GDAL_NUM_THREADS config option to 1

    outfiles.append('tmp/grid_invdist_1thread.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    gdaltest.runexternal(gdal_grid + ' --config GDAL_NUM_THREADS 1 -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/gdal_invdist.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdist.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from the reference')
    ds_ref = None
    ds = None

    #################
    # Test GDAL_NUM_THREADS config option to 2

    outfiles.append('tmp/grid_invdist_2threads.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    gdaltest.runexternal(gdal_grid + ' --config GDAL_NUM_THREADS 2 -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/gdal_invdist.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdist.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from the reference')
    ds_ref = None
    ds = None

    #################
    outfiles.append('tmp/grid_invdist_90_90_8p.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Circular window, shifted, test min points and NODATA setting.
    gdaltest.runexternal(gdal_grid + ' -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:radius1=90.0:radius2=90.0:angle=0.0:max_points=0:min_points=8:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_invdist_90_90_8p.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdist_90_90_8p.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from the reference')
    ds_ref = None
    ds = None

###############################################################################
# Test Moving Average gridding algorithm


def test_gdal_grid_5():
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_average.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # We are using all the points from input dataset to average, so
    # the result is a raster filled with the same value in each node.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_average.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_average.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    ds_ref = None
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from the reference')
    ds = None

    #################
    outfiles.append('tmp/grid_average_300_100_40.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Elliptical window, rotated.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average:radius1=300.0:radius2=100.0:angle=40.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_average_300_100_40.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_average_300_100_40.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    ds_ref = None
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from the reference')
    ds = None

@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_6(use_quadtree):
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_average_190_190.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass
    
    with gdaltest.config_option('GDAL_GRID_POINT_COUNT_THRESHOLD', '0' if use_quadtree else '1000000000'):
        # Create a GDAL dataset from the values of "grid.csv".
        # This time using a circular window.
        gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average:radius1=190.0:radius2=190.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "ref_data/grid_average_190_190.tif"
        ds = gdal.Open(outfiles[-1])
        ds_ref = gdal.Open('ref_data/grid_average_190_190.tif')
        maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
        ds_ref = None
        if maxdiff > 1:
            gdaltest.compare_ds(ds, ds_ref, verbose=1)
            pytest.fail('Image too different from the reference')
        ds = None

        #################
        outfiles.append('tmp/grid_average_90_90_8p.tif')
        try:
            os.remove(outfiles[-1])
        except OSError:
            pass

        # Create a GDAL dataset from the values of "grid.csv".
        # Circular window, shifted, test min points and NODATA setting.
        gdaltest.runexternal(gdal_grid + ' -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average:radius1=90.0:radius2=90.0:angle=0.0:min_points=8:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "ref_data/grid_average_90_90_8p.tif"
        ds = gdal.Open(outfiles[-1])
        ds_ref = gdal.Open('ref_data/grid_average_90_90_8p.tif')
        maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
        ds_ref = None
        if maxdiff > 1:
            gdaltest.compare_ds(ds, ds_ref, verbose=1)
            pytest.fail('Image too different from the reference')
        ds = None

###############################################################################
# Test Minimum data metric


def test_gdal_grid_7():
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_minimum.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Search the whole dataset for minimum.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a minimum:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_minimum.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_minimum.tif')
    assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum(), \
        ('bad checksum : got %d, expected %d' %
              (ds.GetRasterBand(1).Checksum(), ds_ref.GetRasterBand(1).Checksum()))
    ds_ref = None
    ds = None

    #################
    outfiles.append('tmp/grid_minimum_400_100_120.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Elliptical window, rotated.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a minimum:radius1=400.0:radius2=100.0:angle=120.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_minimum_400_100_120.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_minimum_400_100_120.tif')
    assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum(), \
        ('bad checksum : got %d, expected %d' %
              (ds.GetRasterBand(1).Checksum(), ds_ref.GetRasterBand(1).Checksum()))
    ds_ref = None
    ds = None

@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_8(use_quadtree):
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_minimum_180_180.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    with gdaltest.config_option('GDAL_GRID_POINT_COUNT_THRESHOLD', '0' if use_quadtree else '1000000000'):
        # Create a GDAL dataset from the values of "grid.csv".
        # Search ellipse larger than the raster cell.   
        gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a minimum:radius1=180.0:radius2=180.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "ref_data/grid_minimum_180_180.tif"
        ds = gdal.Open(outfiles[-1])
        ds_ref = gdal.Open('ref_data/grid_minimum_180_180.tif')
        assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum(), \
            ('bad checksum : got %d, expected %d' %
                (ds.GetRasterBand(1).Checksum(), ds_ref.GetRasterBand(1).Checksum()))
        ds_ref = None
        ds = None

        #################
        outfiles.append('tmp/grid_minimum_20_20.tif')
        try:
            os.remove(outfiles[-1])
        except OSError:
            pass

        # Create a GDAL dataset from the values of "grid.csv".
        # Search ellipse smaller than the raster cell.
        gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a minimum:radius1=20.0:radius2=20.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "ref_data/grid_minimum_20_20.tif"
        ds = gdal.Open(outfiles[-1])
        ds_ref = gdal.Open('ref_data/grid_minimum_20_20.tif')
        assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum(), \
            ('bad checksum : got %d, expected %d' %
                (ds.GetRasterBand(1).Checksum(), ds_ref.GetRasterBand(1).Checksum()))
        ds_ref = None
        ds = None

###############################################################################
# Test Maximum data metric


def test_gdal_grid_9():
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_maximum.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Search the whole dataset for maximum.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a maximum:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_maximum.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_maximum.tif')
    assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum(), \
        ('bad checksum : got %d, expected %d' %
              (ds.GetRasterBand(1).Checksum(), ds_ref.GetRasterBand(1).Checksum()))
    ds_ref = None
    ds = None

    #################
    outfiles.append('tmp/grid_maximum_100_100.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Circular window.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a maximum:radius1=100.0:radius2=100.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_maximum_100_100.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_maximum_100_100.tif')
    assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum(), \
        ('bad checksum : got %d, expected %d' %
              (ds.GetRasterBand(1).Checksum(), ds_ref.GetRasterBand(1).Checksum()))
    ds_ref = None
    ds = None

@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_10(use_quadtree):
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_maximum_180_180.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    with gdaltest.config_option('GDAL_GRID_POINT_COUNT_THRESHOLD', '0' if use_quadtree else '1000000000'):
        # Create a GDAL dataset from the values of "grid.csv".
        # Search ellipse larger than the raster cell.   
        gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a maximum:radius1=180.0:radius2=180.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "ref_data/grid_maximum_180_180.tif"
        ds = gdal.Open(outfiles[-1])
        ds_ref = gdal.Open('ref_data/grid_maximum_180_180.tif')
        assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum(), \
            ('bad checksum : got %d, expected %d' %
                (ds.GetRasterBand(1).Checksum(), ds_ref.GetRasterBand(1).Checksum()))
        ds_ref = None
        ds = None

        #################
        outfiles.append('tmp/grid_maximum_20_20.tif')
        try:
            os.remove(outfiles[-1])
        except OSError:
            pass

        # Create a GDAL dataset from the values of "grid.csv".
        # Search ellipse smaller than the raster cell.
        gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a maximum:radius1=20.0:radius2=20.0:angle=120.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "ref_data/grid_maximum_20_20.tif"
        ds = gdal.Open(outfiles[-1])
        ds_ref = gdal.Open('ref_data/grid_maximum_20_20.tif')
        assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum(), \
            ('bad checksum : got %d, expected %d' %
                (ds.GetRasterBand(1).Checksum(), ds_ref.GetRasterBand(1).Checksum()))
        ds_ref = None
        ds = None

###############################################################################
# Test Range data metric


def test_gdal_grid_11():
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_range.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Search the whole dataset.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a range:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_range.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_range.tif')
    assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum(), \
        ('bad checksum : got %d, expected %d' %
              (ds.GetRasterBand(1).Checksum(), ds_ref.GetRasterBand(1).Checksum()))
    ds_ref = None
    ds = None  

@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_12(use_quadtree):
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_range_90_90_8p.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    with gdaltest.config_option('GDAL_GRID_POINT_COUNT_THRESHOLD', '0' if use_quadtree else '1000000000'):
         # Create a GDAL dataset from the values of "grid.csv".
        # Circular window, fill node with NODATA value if less than required
        # points found.
        gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a range:radius1=90.0:radius2=90.0:angle=0.0:min_points=8:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "ref_data/grid_range_90_90_8p.tif"
        ds = gdal.Open(outfiles[-1])
        ds_ref = gdal.Open('ref_data/grid_range_90_90_8p.tif')
        assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum(), \
            ('bad checksum : got %d, expected %d' %
                (ds.GetRasterBand(1).Checksum(), ds_ref.GetRasterBand(1).Checksum()))
        ds_ref = None
        ds = None

###############################################################################
# Test Count data metric


@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_13(use_quadtree):
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_count_70_70.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass
    
    with gdaltest.config_option('GDAL_GRID_POINT_COUNT_THRESHOLD', '0' if use_quadtree else '1000000000'):
        # Create a GDAL dataset from the values of "grid.csv".
        gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a count:radius1=70.0:radius2=70.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "ref_data/grid_count_70_70.tif"
        ds = gdal.Open(outfiles[-1])
        ds_ref = gdal.Open('ref_data/grid_count_70_70.tif')
        assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum(), \
            ('bad checksum : got %d, expected %d' %
                (ds.GetRasterBand(1).Checksum(), ds_ref.GetRasterBand(1).Checksum()))
        ds_ref = None
        ds = None

        #################
        outfiles.append('tmp/grid_count_300_300.tif')
        try:
            os.remove(outfiles[-1])
        except OSError:
            pass

        # Create a GDAL dataset from the values of "grid.csv".
        gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a count:radius1=300.0:radius2=300.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "ref_data/grid_count_300_300.tif"
        ds = gdal.Open(outfiles[-1])
        ds_ref = gdal.Open('ref_data/grid_count_300_300.tif')
        assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum(), \
            ('bad checksum : got %d, expected %d' %
                (ds.GetRasterBand(1).Checksum(), ds_ref.GetRasterBand(1).Checksum()))
        ds_ref = None
        ds = None

###############################################################################
# Test Average Distance data metric


def test_gdal_grid_14():
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_avdist.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # We are using all the points from input dataset to average, so
    # the result is a raster filled with the same value in each node.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average_distance:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_avdist.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_avdist.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from the reference')
    ds = None
    ds_ref = None


@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_15(use_quadtree):
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_avdist_150_150.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    with gdaltest.config_option('GDAL_GRID_POINT_COUNT_THRESHOLD', '0' if use_quadtree else '1000000000'):
        # Create a GDAL dataset from the values of "grid.csv".
        # We are using all the points from input dataset to average, so
        # the result is a raster filled with the same value in each node.
        gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average_distance:radius1=150.0:radius2=150.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "ref_data/grid_avdist_150_150.tif"
        ds = gdal.Open(outfiles[-1])
        ds_ref = gdal.Open('ref_data/grid_avdist_150_150.tif')
        maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
        if maxdiff > 1:
            gdaltest.compare_ds(ds, ds_ref, verbose=1)
            pytest.fail('Image too different from the reference')
        ds = None
        ds_ref = None


###############################################################################
# Test Average Distance Between Points data metric


def test_gdal_grid_16():
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_avdistpts_150_50_-15.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # We are using all the points from input dataset to average, so
    # the result is a raster filled with the same value in each node.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average_distance_pts:radius1=150.0:radius2=50.0:angle=-15.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_avdistpts_150_50_-15.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_avdistpts_150_50_-15.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from the reference')
    ds = None
    ds_ref = None


@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_17(use_quadtree):
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_avdistpts_150_150.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass
    
    with gdaltest.config_option('GDAL_GRID_POINT_COUNT_THRESHOLD', '0' if use_quadtree else '1000000000'):
        # Create a GDAL dataset from the values of "grid.csv".
        # We are using all the points from input dataset to average, so
        # the result is a raster filled with the same value in each node.
        gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average_distance_pts:radius1=150.0:radius2=150.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

        # We should get the same values as in "ref_data/grid_avdistpts_150_150.tif"
        ds = gdal.Open(outfiles[-1])
        ds_ref = gdal.Open('ref_data/grid_avdistpts_150_150.tif')
        maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
        if maxdiff > 1:
            gdaltest.compare_ds(ds, ds_ref, verbose=1)
            pytest.fail('Image too different from the reference')
        ds = None
        ds_ref = None


###############################################################################
# Test linear


def test_gdal_grid_18():
    if gdal_grid is None:
        pytest.skip()

    outfiles.append('tmp/n43_linear.tif')

    # Create a GDAL dataset from the previous generated OGR grid
    (_, err) = gdaltest.runexternal_out_and_err(gdal_grid + ' -txe -80.0041667 -78.9958333 -tye 42.9958333 44.0041667 -outsize 121 121 -ot Int16 -l n43 -a linear -co TILED=YES -co BLOCKXSIZE=256 -co BLOCKYSIZE=256 tmp/n43.shp ' + outfiles[-1])
    assert (err is None or err == ''), 'got error/warning'

    # We should get the same values as in n43.td0
    ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds2 = gdal.Open(outfiles[-1])
    assert ds.GetRasterBand(1).Checksum() == ds2.GetRasterBand(1).Checksum(), \
        ('bad checksum : got %d, expected %d' % (ds.GetRasterBand(1).Checksum(), ds2.GetRasterBand(1).Checksum()))

    ds = None
    ds2 = None

###############################################################################
# Test Inverse Distance to a Power with Nearest Neighbor gridding algorithm


def test_gdal_grid_19():
    if gdal_grid is None:
        pytest.skip()

    #################
    # Test generic implementation (no AVX, no SSE)
    outfiles.append('tmp/grid_invdistnn_generic.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    (_, _) = gdaltest.runexternal_out_and_err(gdal_grid + ' -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdistnn:power=2.0:radius=1.0:max_points=12:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/gdal_invdistnn.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdistnn.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 0.00001:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from the reference')
    ds_ref = None
    ds = None

    #################
    outfiles.append('tmp/grid_invdistnn_250_8minp.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Circular window, shifted, test min points and NODATA setting.
    gdaltest.runexternal(gdal_grid + ' -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdistnn:power=2.0:radius=250.0:max_points=12:min_points=8:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_invdistnn_250_8minp.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdistnn_250_8minp.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 0.00001:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from the reference')
    ds_ref = None
    ds = None

    #################
    # Test generic implementation with max_points and radius specified
    outfiles.append('tmp/grid_invdistnn_250_10maxp_3pow.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    gdaltest.runexternal(gdal_grid + ' -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdistnn:power=3.0:radius=250.0:max_points=10:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/gdal_invdistnn_250_10maxp_3pow.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdistnn_250_10maxp_3pow.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 0.00001:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail('Image too different from the reference')
    ds_ref = None
    ds = None

###############################################################################
# Test -clipsrc


def test_gdal_grid_clipsrc():
    if gdal_grid is None:
        pytest.skip()

    if not ogrtest.have_geos():
        pytest.skip()

    #################
    outfiles.append('tmp/grid_clipsrc.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    open('tmp/clip.csv', 'wt').write(
        'id,WKT\n1,"POLYGON((440750 3751340,440750 3750100,441900 3750100,441900 3751340,440750 3751340))"\n')

    # Create a GDAL dataset from the values of "grid.csv".
    # Grid nodes are located exactly in raster nodes.
    gdaltest.runexternal_out_and_err(gdal_grid + ' -clipsrc tmp/clip.csv -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=0.0:radius2=0.0:angle=0.0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    os.unlink('tmp/clip.csv')

    # We should get the same values as in "gcore/data/byte.tif"
    ds = gdal.Open(outfiles[-1])
    cs = ds.GetRasterBand(1).Checksum()
    assert not (cs == 0 or cs == 4672), 'bad checksum'
    ds = None

###############################################################################
# Test -tr


def test_gdal_grid_tr():
    if gdal_grid is None:
        pytest.skip()

    #################
    outfiles.append('tmp/grid_count_70_70.tif')
    try:
        os.remove(outfiles[-1])
    except OSError:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -tr 60 60 -ot Byte -l grid -a count:radius1=70.0:radius2=70.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_count_70_70.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_count_70_70.tif')
    assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum(), \
        ('bad checksum : got %d, expected %d' %
              (ds.GetRasterBand(1).Checksum(), ds_ref.GetRasterBand(1).Checksum()))
    ds_ref = None
    ds = None

###############################################################################
# Cleanup


def test_gdal_grid_cleanup():

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/n43.shp')
    drv = gdal.GetDriverByName('GTiff')
    for outfile in outfiles:
        drv.Delete(outfile)

    



