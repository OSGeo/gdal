#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_grid testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import shutil
import struct

sys.path.append( '../pymod' )
sys.path.append( '../gcore' )

from osgeo import gdal
from osgeo import ogr

import gdaltest
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
        return 'skip'

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    outfiles.append('tmp/n43.tif')

    try:
        os.remove('tmp/n43.shp')
    except:
        pass
    try:
        os.remove('tmp/n43.dbf')
    except:
        pass
    try:
        os.remove('tmp/n43.shx')
    except:
        pass
    try:
        os.remove('tmp/n43.qix')
    except:
        pass

    # Create an OGR grid from the values of n43.dt0
    ds = gdal.Open('../gdrivers/data/n43.dt0')
    geotransform = ds.GetGeoTransform()

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    shape_ds = shape_drv.CreateDataSource( 'tmp' )
    shape_lyr = shape_ds.CreateLayer( 'n43' )

    data = ds.ReadRaster(0, 0, 121, 121)
    array_val = struct.unpack('h' * 121*121, data)
    for j in range(121):
        for i in range(121):
            wkt = 'POINT(%f %f %s)' % ( geotransform[0] + (i + .5) * geotransform[1],
                                        geotransform[3] + (j + .5) * geotransform[5],
                                        array_val[j * 121 + i] )
            dst_feat = ogr.Feature( feature_def = shape_lyr.GetLayerDefn() )
            dst_feat.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
            shape_lyr.CreateFeature( dst_feat )

    dst_feat.Destroy()

    shape_ds.ExecuteSQL('CREATE SPATIAL INDEX ON n43')

    shape_ds.Destroy()

    # Create a GDAL dataset from the previous generated OGR grid
    (out, err) = gdaltest.runexternal_out_and_err(gdal_grid + ' -txe -80.0041667 -78.9958333 -tye 42.9958333 44.0041667 -outsize 121 121 -ot Int16 -l n43 -a nearest:radius1=0.0:radius2=0.0:angle=0.0 -co TILED=YES -co BLOCKXSIZE=256 -co BLOCKYSIZE=256 tmp/n43.shp ' + outfiles[-1])
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    # We should get the same values as in n43.td0
    ds2 = gdal.Open(outfiles[-1])
    if ds.GetRasterBand(1).Checksum() != ds2.GetRasterBand(1).Checksum():
        print('bad checksum : got %d, expected %d' % (ds.GetRasterBand(1).Checksum() , ds2.GetRasterBand(1).Checksum()))
        return 'fail'
    if ds2.GetRasterBand(1).GetNoDataValue() is not None:
        print('did not expect nodata value')
        return 'fail'

    ds = None
    ds2 = None

    return 'success'

###############################################################################
# Test Nearest Neighbour gridding algorithm

def test_gdal_grid_2():
    if gdal_grid is None:
        return 'skip'

    # Open reference dataset
    ds_ref = gdal.Open('../gcore/data/byte.tif')
    checksum_ref = ds_ref.GetRasterBand(1).Checksum()
    ds_ref = None

    #################
    outfiles.append('tmp/grid_near.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Grid nodes are located exactly in raster nodes.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=0.0:radius2=0.0:angle=0.0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "gcore/data/byte.tif"
    ds = gdal.Open(outfiles[-1])
    if ds.GetRasterBand(1).Checksum() != checksum_ref:
        gdaltest.post_reason('bad checksum')
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    if ds.GetRasterBand(1).GetNoDataValue() != 0.0:
        print('expected a nodata value')
        return 'fail'
    ds = None

    #################
    outfiles.append('tmp/grid_near_shift.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Now the same, but shift grid nodes a bit in both horizontal and vertical
    # directions.
    gdaltest.runexternal(gdal_grid + ' -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=0.0:radius2=0.0:angle=0.0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "gcore/data/byte.tif"
    ds = gdal.Open(outfiles[-1])
    if ds.GetRasterBand(1).Checksum() != checksum_ref:
        gdaltest.post_reason('bad checksum')
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    ds = None

    #################
    outfiles.append('tmp/grid_near_search3.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Now try the search ellipse larger than the raster cell.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=180.0:radius2=180.0:angle=0.0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "gcore/data/byte.tif"
    ds = gdal.Open(outfiles[-1])
    if ds.GetRasterBand(1).Checksum() != checksum_ref:
        gdaltest.post_reason('bad checksum')
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    ds = None

    #################
    outfiles.append('tmp/grid_near_search1.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Search ellipse smaller than the raster cell.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=20.0:radius2=20.0:angle=0.0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "gcore/data/byte.tif"
    ds = gdal.Open(outfiles[-1])
    if ds.GetRasterBand(1).Checksum() != checksum_ref:
        gdaltest.post_reason('bad checksum')
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    ds = None

    #################
    outfiles.append('tmp/grid_near_shift_search3.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Large search ellipse and the grid shift.
    gdaltest.runexternal(gdal_grid + ' -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=180.0:radius2=180.0:angle=0.0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "gcore/data/byte.tif"
    ds = gdal.Open(outfiles[-1])
    if ds.GetRasterBand(1).Checksum() != checksum_ref:
        gdaltest.post_reason('bad checksum')
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    ds = None

    #################
    outfiles.append('tmp/grid_near_shift_search1.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Small search ellipse and the grid shift.
    gdaltest.runexternal(gdal_grid + ' -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=20.0:radius2=20.0:angle=0.0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "gcore/data/byte.tif"
    ds = gdal.Open(outfiles[-1])
    if ds.GetRasterBand(1).Checksum() != checksum_ref:
        gdaltest.post_reason('bad checksum')
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test Inverse Distance to a Power gridding algorithm

def test_gdal_grid_3():
    if gdal_grid is None:
        return 'skip'

    #################
    # Test generic implementation (no AVX, no SSE)
    outfiles.append('tmp/grid_invdist_generic.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    gdaltest.runexternal(gdal_grid + ' --config GDAL_USE_AVX NO --config GDAL_USE_SSE NO -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/gdal_invdist.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdist.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose = 0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose = 1)
        gdaltest.post_reason('Image too different from the reference')
        return 'fail'
    ds_ref = None
    ds = None

    #################
    # Potentially test optimized SSE implementation

    outfiles.append('tmp/grid_invdist_sse.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    gdaltest.runexternal(gdal_grid + ' --config GDAL_USE_AVX NO -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/gdal_invdist.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdist.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose = 0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose = 1)
        gdaltest.post_reason('Image too different from the reference')
        return 'fail'
    ds_ref = None
    ds = None

    #################
    # Potentially test optimized AVX implementation

    outfiles.append('tmp/grid_invdist_avx.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/gdal_invdist.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdist.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose = 0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose = 1)
        gdaltest.post_reason('Image too different from the reference')
        return 'fail'
    ds_ref = None
    ds = None
    
    #################
    # Test GDAL_NUM_THREADS config option to 1

    outfiles.append('tmp/grid_invdist_1thread.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    gdaltest.runexternal(gdal_grid + ' --config GDAL_NUM_THREADS 1 -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/gdal_invdist.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdist.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose = 0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose = 1)
        gdaltest.post_reason('Image too different from the reference')
        return 'fail'
    ds_ref = None
    ds = None

    #################
    # Test GDAL_NUM_THREADS config option to 2

    outfiles.append('tmp/grid_invdist_2threads.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    gdaltest.runexternal(gdal_grid + ' --config GDAL_NUM_THREADS 2 -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/gdal_invdist.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdist.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose = 0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose = 1)
        gdaltest.post_reason('Image too different from the reference')
        return 'fail'
    ds_ref = None
    ds = None

    #################
    outfiles.append('tmp/grid_invdist_90_90_8p.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Circular window, shifted, test min points and NODATA setting.
    gdaltest.runexternal(gdal_grid + ' -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:radius1=90.0:radius2=90.0:angle=0.0:max_points=0:min_points=8:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_invdist_90_90_8p.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_invdist_90_90_8p.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose = 0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose = 1)
        gdaltest.post_reason('Image too different from the reference')
        return 'fail'
    ds_ref = None
    ds = None

    return 'success'

###############################################################################
# Test Moving Average gridding algorithm

def test_gdal_grid_4():
    if gdal_grid is None:
        return 'skip'

    #################
    outfiles.append('tmp/grid_average.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # We are using all the points from input dataset to average, so
    # the result is a raster filled with the same value in each node.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_average.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_average.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose = 0)
    ds_ref = None
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose = 1)
        gdaltest.post_reason('Image too different from the reference')
        return 'fail'
    ds = None

    #################
    outfiles.append('tmp/grid_average_190_190.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # This time using a circular window.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average:radius1=190.0:radius2=190.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_average_190_190.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_average_190_190.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose = 0)
    ds_ref = None
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose = 1)
        gdaltest.post_reason('Image too different from the reference')
        return 'fail'
    ds = None

    #################
    outfiles.append('tmp/grid_average_300_100_40.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Elliptical window, rotated.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average:radius1=300.0:radius2=100.0:angle=40.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_average_300_100_40.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_average_300_100_40.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose = 0)
    ds_ref = None
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose = 1)
        gdaltest.post_reason('Image too different from the reference')
        return 'fail'
    ds = None

    #################
    outfiles.append('tmp/grid_average_90_90_8p.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Circular window, shifted, test min points and NODATA setting.
    gdaltest.runexternal(gdal_grid + ' -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average:radius1=90.0:radius2=90.0:angle=0.0:min_points=8:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_average_90_90_8p.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_average_90_90_8p.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose = 0)
    ds_ref = None
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose = 1)
        gdaltest.post_reason('Image too different from the reference')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test Minimum data metric

def test_gdal_grid_5():
    if gdal_grid is None:
        return 'skip'

    #################
    outfiles.append('tmp/grid_minimum.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Search the whole dataset for minimum.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a minimum:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_minimum.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_minimum.tif')
    if ds.GetRasterBand(1).Checksum() != ds_ref.GetRasterBand(1).Checksum():
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    ds_ref = None
    ds = None

    #################
    outfiles.append('tmp/grid_minimum_400_100_120.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Elliptical window, rotated.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a minimum:radius1=400.0:radius2=100.0:angle=120.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_minimum_400_100_120.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_minimum_400_100_120.tif')
    if ds.GetRasterBand(1).Checksum() != ds_ref.GetRasterBand(1).Checksum():
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    ds_ref = None
    ds = None

    return 'success'

###############################################################################
# Test Maximum data metric

def test_gdal_grid_6():
    if gdal_grid is None:
        return 'skip'

    #################
    outfiles.append('tmp/grid_maximum.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Search the whole dataset for maximum.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a maximum:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_maximum.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_maximum.tif')
    if ds.GetRasterBand(1).Checksum() != ds_ref.GetRasterBand(1).Checksum():
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    ds_ref = None
    ds = None

    #################
    outfiles.append('tmp/grid_maximum_100_100.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Circular window.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a maximum:radius1=100.0:radius2=100.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_maximum_100_100.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_maximum_100_100.tif')
    if ds.GetRasterBand(1).Checksum() != ds_ref.GetRasterBand(1).Checksum():
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    ds_ref = None
    ds = None

    return 'success'

###############################################################################
# Test Range data metric

def test_gdal_grid_7():
    if gdal_grid is None:
        return 'skip'

    #################
    outfiles.append('tmp/grid_range.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Search the whole dataset.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a range:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_range.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_range.tif')
    if ds.GetRasterBand(1).Checksum() != ds_ref.GetRasterBand(1).Checksum():
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    ds_ref = None
    ds = None

    #################
    outfiles.append('tmp/grid_range_90_90_8p.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # Circular window, fill node with NODATA value if less than required
    # points found.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a range:radius1=90.0:radius2=90.0:angle=0.0:min_points=8:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_range_90_90_8p.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_range_90_90_8p.tif')
    if ds.GetRasterBand(1).Checksum() != ds_ref.GetRasterBand(1).Checksum():
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    ds_ref = None
    ds = None

    return 'success'

###############################################################################
# Test Count data metric

def test_gdal_grid_8():
    if gdal_grid is None:
        return 'skip'

    #################
    outfiles.append('tmp/grid_count_70_70.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a count:radius1=70.0:radius2=70.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_count_70_70.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_count_70_70.tif')
    if ds.GetRasterBand(1).Checksum() != ds_ref.GetRasterBand(1).Checksum():
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    ds_ref = None
    ds = None

    #################
    outfiles.append('tmp/grid_count_300_300.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a count:radius1=300.0:radius2=300.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_count_300_300.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_count_300_300.tif')
    if ds.GetRasterBand(1).Checksum() != ds_ref.GetRasterBand(1).Checksum():
        print('bad checksum : got %d, expected %d' % \
              (ds.GetRasterBand(1).Checksum(), checksum_ref))
        return 'fail'
    ds_ref = None
    ds = None

    return 'success'

###############################################################################
# Test Average Distance data metric

def test_gdal_grid_9():
    if gdal_grid is None:
        return 'skip'

    #################
    outfiles.append('tmp/grid_avdist.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # We are using all the points from input dataset to average, so
    # the result is a raster filled with the same value in each node.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average_distance:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_avdist.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_avdist.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose = 0)
    ds_ref = None
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose = 1)
        gdaltest.post_reason('Image too different from the reference')
        return 'fail'
    ds = None

    #################
    outfiles.append('tmp/grid_avdist_150_150.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # We are using all the points from input dataset to average, so
    # the result is a raster filled with the same value in each node.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average_distance:radius1=150.0:radius2=150.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_avdist_150_150.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_avdist_150_150.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose = 0)
    ds_ref = None
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose = 1)
        gdaltest.post_reason('Image too different from the reference')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test Average Distance Between Points data metric

def test_gdal_grid_10():
    if gdal_grid is None:
        return 'skip'

    #################
    outfiles.append('tmp/grid_avdist_150_50_-15.tif')
    try:
        os.remove(outfiles[-1])
    except:
        pass

    # Create a GDAL dataset from the values of "grid.csv".
    # We are using all the points from input dataset to average, so
    # the result is a raster filled with the same value in each node.
    gdaltest.runexternal(gdal_grid + ' -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average_distance_pts:radius1=150.0:radius2=50.0:angle=-15.0:min_points=0:nodata=0.0 data/grid.vrt ' + outfiles[-1])

    # We should get the same values as in "ref_data/grid_avdist_150_50_-15.tif"
    ds = gdal.Open(outfiles[-1])
    ds_ref = gdal.Open('ref_data/grid_avdist_150_50_-15.tif')
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose = 0)
    ds_ref = None
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose = 1)
        gdaltest.post_reason('Image too different from the reference')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Cleanup

def test_gdal_grid_cleanup():

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/n43.shp')
    drv = gdal.GetDriverByName('GTiff')
    for outfile in outfiles:
        drv.Delete(outfile)

    return 'success'

gdaltest_list = [
    test_gdal_grid_1,
    test_gdal_grid_2,
    test_gdal_grid_3,
    test_gdal_grid_4,
    test_gdal_grid_5,
    test_gdal_grid_6,
    test_gdal_grid_7,
    test_gdal_grid_8,
    test_gdal_grid_9,
    test_gdal_grid_10,
    test_gdal_grid_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_grid' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()





