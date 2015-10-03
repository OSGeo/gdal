#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test librarified gdalwarp
# Author:   Faza Mahamood <fazamhd @ gmail dot com>
# 
###############################################################################
# Copyright (c) 2015, Faza Mahamood <fazamhd at gmail dot com>
# Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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

sys.path.append( '../pymod' )

from osgeo import gdal, ogr
import gdaltest

###############################################################################
# Simple test

def test_gdalwarp_lib_1():

    ds1 = gdal.Open('../gcore/data/byte.tif')
    dstDS = gdal.Warp('tmp/testgdalwarp1.tif', ds1)

    if dstDS.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    dstDS = None

    return 'success'


###############################################################################
# Test -of option

def test_gdalwarp_lib_2():

    ds1 = gdal.Open('../gcore/data/byte.tif')
    dstDS = gdal.Warp('tmp/testgdalwarp2.tif',[ds1], format = 'GTiff')

    if dstDS.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    dstDS = None

    return 'success'


###############################################################################
# Test -ot option

def test_gdalwarp_lib_3():

    ds1 = gdal.Open('../gcore/data/byte.tif')
    dstDS = gdal.Warp('', ds1, format = 'MEM', outputType = gdal.GDT_Int16)
    
    if dstDS.GetRasterBand(1).DataType != gdal.GDT_Int16:
        gdaltest.post_reason('Bad data type')
        return 'fail'

    if dstDS.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    dstDS = None

    return 'success'

###############################################################################
# Test -t_srs option

def test_gdalwarp_lib_4():
    
    ds1 = gdal.Open('../gcore/data/byte.tif')
    dstDS = gdal.Warp('', ds1, format = 'MEM', dstSRS = 'EPSG:32611')

    if dstDS.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    dstDS = None

    return 'success'

###############################################################################
# Test warping from GCPs without any explicit option

def test_gdalwarp_lib_5():

    ds = gdal.Open('../gcore/data/byte.tif')
    gcpList = [gdal.GCP(440720.000,3751320.000,0,0,0), gdal.GCP(441920.000,3751320.000,0,20,0), gdal.GCP(441920.000,3750120.000,0,20,20), gdal.GCP(440720.000,3750120.000,0,0,20)]
    ds1 = gdal.Translate('tmp/testgdalwarp_gcp.tif',ds,outputSRS = 'EPSG:26711',GCPs = gcpList)
    dstDS = gdal.Warp('', ds1, format = 'MEM', tps = True)

    if dstDS.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if not gdaltest.geotransform_equals(ds.GetGeoTransform(), dstDS.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    dstDS = None

    return 'success'


###############################################################################
# Test warping from GCPs with -tps

def test_gdalwarp_lib_6():
    
    ds1 = gdal.Open('tmp/testgdalwarp_gcp.tif')
    dstDS = gdal.Warp('',ds1, format = 'MEM', tps = True)

    if dstDS.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if not gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), dstDS.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    dstDS = None

    return 'success'


###############################################################################
# Test -tr

def test_gdalwarp_lib_7():
    
    ds1 = gdal.Open('tmp/testgdalwarp_gcp.tif')
    dstDS = gdal.Warp('',[ds1], format = 'MEM',xRes = 120,yRes = 120)
    if dstDS is None:
        return 'fail'

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    if not gdaltest.geotransform_equals(expected_gt, dstDS.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    dstDS = None

    return 'success'

###############################################################################
# Test -ts

def test_gdalwarp_lib_8():
    
    ds1 = gdal.Open('tmp/testgdalwarp_gcp.tif')
    dstDS = gdal.Warp('',[ds1], format = 'MEM',width = 10,height = 10)
    if dstDS is None:
        return 'fail'

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    if not gdaltest.geotransform_equals(expected_gt, dstDS.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    dstDS = None

    return 'success'

###############################################################################
# Test -te

def test_gdalwarp_lib_9():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM', outputBounds = [440720.000, 3750120.000, 441920.000, 3751320.000])

    if not gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -rn

def test_gdalwarp_lib_10():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM', width = 40, height = 40, resampleAlg = gdal.GRIORA_NearestNeighbour)

    if ds.GetRasterBand(1).Checksum() != 18784:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -rb

def test_gdalwarp_lib_11():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM', width = 40, height = 40, resampleAlg = gdal.GRIORA_Bilinear)

    ref_ds = gdal.Open('ref_data/testgdalwarp11.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)
    ref_ds = None

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -rc

def test_gdalwarp_lib_12():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM', width = 40, height = 40, resampleAlg = gdal.GRIORA_Cubic)

    ref_ds = gdal.Open('ref_data/testgdalwarp12.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)
    ref_ds = None

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -rcs

def test_gdalwarp_lib_13():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM', width = 40, height = 40, resampleAlg = gdal.GRIORA_CubicSpline)

    ref_ds = gdal.Open('ref_data/testgdalwarp13.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)
    ref_ds = None

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -r lanczos

def test_gdalwarp_lib_14():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM', width = 40, height = 40, resampleAlg = gdal.GRIORA_Lanczos)

    ref_ds = gdal.Open('ref_data/testgdalwarp14.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)
    ref_ds = None

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -dstnodata

def test_gdalwarp_lib_15():

    ds = gdal.Warp('', 'tmp/testgdalwarp_gcp.tif', format = 'MEM', dstSRS = 'EPSG:32610', dstNodata = 1)

    if ds.GetRasterBand(1).GetNoDataValue() != 1:
        print(ds.GetRasterBand(1).GetNoDataValue())
        gdaltest.post_reason('Bad nodata value')
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4523:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -of VRT which is a special case

def test_gdalwarp_lib_16():

    ds = gdal.Warp('/vsimem/test_gdalwarp_lib_16.vrt', 'tmp/testgdalwarp_gcp.tif', format = 'VRT')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    gdal.Unlink('/vsimem/test_gdalwarp_lib_16.vrt')

    return 'success'

###############################################################################
# Test -dstalpha

def test_gdalwarp_lib_17():

    ds = gdal.Warp('', '../gcore/data/rgbsmall.tif', format = 'MEM', dstAlpha = True)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(4) is None:
        gdaltest.post_reason('No alpha band generated')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -et 0 which is a special case

def test_gdalwarp_lib_19():

    ds = gdal.Warp('', 'tmp/testgdalwarp_gcp.tif', format = 'MEM', errorThreshold = 0)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test cutline from OGR datasource.

def test_gdalwarp_lib_21():

    ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format = 'MEM', cutlineDSName = 'data/cutline.vrt', cutlineLayer = 'cutline')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 19139:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test cutline with ALL_TOUCHED enabled.

def test_gdalwarp_lib_23():

    ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format = 'MEM', warpOptions = [ 'CUTLINE_ALL_TOUCHED=TRUE' ], cutlineDSName = 'data/cutline.vrt', cutlineLayer = 'cutline')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 20123:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -tap

def test_gdalwarp_lib_32():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM', targetAlignedPixels = True, xRes = 100, yRes = 50)
    if ds is None:
        return 'fail'

    expected_gt = (440700.0, 100.0, 0.0, 3751350.0, 0.0, -50.0)
    got_gt = ds.GetGeoTransform()
    if not gdaltest.geotransform_equals(expected_gt, got_gt, 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        print(got_gt)
        return 'fail'

    if ds.RasterXSize != 13 or ds.RasterYSize != 25:
        gdaltest.post_reason('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize) )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test warping multiple sources

def test_gdalwarp_lib_34():

    srcds1 = gdal.Translate('', '../gcore/data/byte.tif', format = 'MEM', srcWin = [0,0,10,20])
    srcds2 = gdal.Translate('', '../gcore/data/byte.tif', format = 'MEM', srcWin = [10,0,10,20])
    ds = gdal.Warp('', [srcds1, srcds2], format = 'MEM')

    cs = ds.GetRasterBand(1).Checksum()
    gt = ds.GetGeoTransform()
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize
    ds = None

    if xsize != 20 or ysize != 20:
        gdaltest.post_reason('bad dimensions')
        print(xsize)
        print(ysize)
        return 'fail'

    if cs != 4672:
        gdaltest.post_reason('bad checksum')
        print(cs)
        return 'fail'

    expected_gt = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-5:
            gdaltest.post_reason('bad gt')
            print(gt)
            return 'fail'

    return 'success'

###############################################################################
# Test -te_srs

def test_gdalwarp_lib_45():

    ds = gdal.Warp('', ['../gcore/data/byte.tif'], format = 'MEM', outputBounds = [-117.641087629972, 33.8915301685897, -117.628190189534, 33.9024195619201 ], outputBoundsSRS = 'EPSG:4267')
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -crop_to_cutline

def test_gdalwarp_lib_46():

    ds = gdal.Warp('', ['../gcore/data/utmsmall.tif'], format = 'MEM', cutlineDSName = 'data/cutline.vrt', cropToCutline = True)
    if ds.GetRasterBand(1).Checksum() != 19582:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test callback

def mycallback(pct, msg, user_data):
    user_data[0] = pct
    return 1

def test_gdalwarp_lib_100():

    tab = [ 0 ]
    ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM', callback = mycallback, callback_data = tab)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if tab[0] != 1.0:
        gdaltest.post_reason('Bad percentage')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test with color table

def test_gdalwarp_lib_101():

    ds = gdal.Warp('', '../gdrivers/data/small_world_pct.tif', format = 'MEM')
    if ds.GetRasterBand(1).GetColorTable() is None:
        gdaltest.post_reason('Did not get color table')
        return 'fail'

    return 'success'

###############################################################################
# Test with a dataset with no bands

def test_gdalwarp_lib_102():

    no_band_ds = gdal.GetDriverByName('MEM').Create('no band', 1, 1, 0)
    with gdaltest.error_handler():
        ds = gdal.Warp('', [ '../gdrivers/data/small_world_pct.tif', no_band_ds ], format = 'MEM')
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'
    return 'success'

###############################################################################
# Test failed transformer

def test_gdalwarp_lib_103():

    with gdaltest.error_handler():
        ds = gdal.Warp('', [ '../gdrivers/data/small_world_pct.tif', '../gcore/data/stefan_full_rgba.tif'], format = 'MEM')
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'
    return 'success'

###############################################################################
# Test no usable source image

def test_gdalwarp_lib_104():

    with gdaltest.error_handler():
        ds = gdal.Warp('', [], format = 'MEM')
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'
    return 'success'

###############################################################################
# Test failure in GDALSuggestedWarpOutput2

def test_gdalwarp_lib_105():

    with gdaltest.error_handler():
        ds = gdal.Warp('', [ '../gdrivers/data/small_world_pct.tif', '../gcore/data/byte.tif' ], format = 'MEM', dstSRS = 'EPSG:32645')
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'
    return 'success'

###############################################################################
# Test failure in creation

def test_gdalwarp_lib_106():

    with gdaltest.error_handler():
        ds = gdal.Warp('/not_existing_dir/not_existing_file', [ '../gdrivers/data/small_world_pct.tif', '../gcore/data/byte.tif' ])
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'
    return 'success'

###############################################################################
# Test forced width only

def test_gdalwarp_lib_107():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM', width = 20)
    if ds.GetRasterBand(1).Checksum() != 4672:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    return 'success'

###############################################################################
# Test forced height only

def test_gdalwarp_lib_108():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM', height = 20)
    if ds.GetRasterBand(1).Checksum() != 4672:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'
    
    return 'success'

###############################################################################
# Test wrong cutline name

def test_gdalwarp_lib_109():

    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM', cutlineDSName = '/does/not/exist')
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'
    return 'success'

###############################################################################
# Test wrong cutline layer name

def test_gdalwarp_lib_110():

    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM', cutlineDSName = 'data/cutline.vrt', cutlineLayer = 'wrong_name')
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'
    return 'success'

###############################################################################
# Test cutline SQL

def test_gdalwarp_lib_111():

    ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format = 'MEM', cutlineDSName = 'data/cutline.vrt', cutlineSQL = 'SELECT * FROM cutline', cutlineWhere = '1 = 1')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 19139:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test cutline without geometry

def test_gdalwarp_lib_112():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/cutline.shp')
    lyr = ds.CreateLayer('cutline')
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None
    ds = None
    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format = 'MEM', cutlineDSName = '/vsimem/cutline.shp', cutlineSQL = 'SELECT * FROM cutline')
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/cutline.shp')

    return 'success'

###############################################################################
# Test cutline with non polygon geometry

def test_gdalwarp_lib_113():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/cutline.shp')
    lyr = ds.CreateLayer('cutline')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    f = None
    ds = None
    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format = 'MEM', cutlineDSName = '/vsimem/cutline.shp')
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/cutline.shp')

    return 'success'

###############################################################################
# Test cutline without feature

def test_gdalwarp_lib_114():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/cutline.shp')
    ds.CreateLayer('cutline')
    ds = None
    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format = 'MEM', cutlineDSName = '/vsimem/cutline.shp')
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/cutline.shp')

    return 'success'

###############################################################################
# Test source dataset without band

def test_gdalwarp_lib_115():

    no_band_ds = gdal.GetDriverByName('MEM').Create('no band', 1, 1, 0)
    out_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 0)

    with gdaltest.error_handler():
        ret = gdal.Warp(out_ds, no_band_ds, cutlineDSName = 'data/cutline.vrt', cutlineLayer = 'cutline')
    if ret != 0:
        gdaltest.post_reason('Expected failure')
        return 'fail'

    return 'success'

###############################################################################
# Test failed cropToCutline due to invalid SRC_SRS

def test_gdalwarp_lib_116():

    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format = 'MEM', cutlineDSName = 'data/cutline.vrt', cutlineLayer = 'cutline', cropToCutline = True, transformerOptions = ['SRC_SRS=invalid'])
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'

    return 'success'

###############################################################################
# Test failed cropToCutline due to invalid DST_SRS

def test_gdalwarp_lib_117():

    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format = 'MEM', cutlineDSName = 'data/cutline.vrt', cutlineLayer = 'cutline', cropToCutline = True, transformerOptions = ['DST_SRS=invalid'])
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'

    return 'success'

###############################################################################
# Test failed cropToCutline due to no source raster

def test_gdalwarp_lib_118():

    with gdaltest.error_handler():
        ds = gdal.Warp('', [], format = 'MEM', cutlineDSName = 'data/cutline.vrt', cutlineLayer = 'cutline', cropToCutline = True)
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'

    return 'success'

###############################################################################
# Test failed cropToCutline due to source raster without projection

def test_gdalwarp_lib_119():

    no_proj_ds = gdal.GetDriverByName('MEM').Create('no_proj_ds', 1, 1)
    with gdaltest.error_handler():
        ds = gdal.Warp('', no_proj_ds, format = 'MEM', cutlineDSName = 'data/cutline.vrt', cutlineLayer = 'cutline', cropToCutline = True)
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'

    return 'success'

###############################################################################
# Test failed cropToCutline due to source raster with dummy projection

def test_gdalwarp_lib_120():

    dummy_proj_ds = gdal.GetDriverByName('MEM').Create('no_proj_ds', 1, 1)
    dummy_proj_ds.SetProjection('dummy')
    with gdaltest.error_handler():
        ds = gdal.Warp('', dummy_proj_ds, format = 'MEM', cutlineDSName = 'data/cutline.vrt', cutlineLayer = 'cutline', cropToCutline = True)
    if ds is not None:
        gdaltest.post_reason('Did not expected dataset')
        return 'fail'

    return 'success'

###############################################################################
# Test internal wrappers

def test_gdalwarp_lib_121():

    # No option
    with gdaltest.error_handler():
        gdal.wrapper_GDALWarpDestName('', [], None)

    # Will create an implicit options structure
    with gdaltest.error_handler():
        gdal.wrapper_GDALWarpDestName('', [], None, gdal.TermProgress)

    # Null dest name
    with gdaltest.error_handler():
        gdal.wrapper_GDALWarpDestName(None, [], None)

    # No option
    with gdaltest.error_handler():
        gdal.wrapper_GDALWarpDestDS(gdal.GetDriverByName('MEM').Create('', 1, 1), [], None)

    # Will create an implicit options structure
    with gdaltest.error_handler():
        gdal.wrapper_GDALWarpDestDS(gdal.GetDriverByName('MEM').Create('', 1, 1), [], None, gdal.TermProgress)

    return 'success'

###############################################################################
# Cleanup

def test_gdalwarp_lib_cleanup():

    # We don't clean up when run in debug mode.
    if gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) == 'ON':
        return 'success'
    
    for i in range(2):
        try:
            os.remove('tmp/testgdalwarp' + str(i+1) + '.tif')
        except:
            pass
    try:
        os.remove('tmp/testgdalwarp_gcp.tif')
    except:
        pass
    
    return 'success'

gdaltest_list = [
    test_gdalwarp_lib_cleanup,
    test_gdalwarp_lib_1,
    test_gdalwarp_lib_2,
    test_gdalwarp_lib_3,
    test_gdalwarp_lib_4,
    test_gdalwarp_lib_5,
    test_gdalwarp_lib_6,
    test_gdalwarp_lib_7,
    test_gdalwarp_lib_8,
    test_gdalwarp_lib_9,
    test_gdalwarp_lib_10,
    test_gdalwarp_lib_11,
    test_gdalwarp_lib_12,
    test_gdalwarp_lib_13,
    test_gdalwarp_lib_14,
    test_gdalwarp_lib_15,
    test_gdalwarp_lib_16,
    test_gdalwarp_lib_17,
    test_gdalwarp_lib_19,
    test_gdalwarp_lib_21,
    test_gdalwarp_lib_23,
    test_gdalwarp_lib_32,
    test_gdalwarp_lib_34,
    test_gdalwarp_lib_45,
    test_gdalwarp_lib_46,
    test_gdalwarp_lib_100,
    test_gdalwarp_lib_101,
    test_gdalwarp_lib_102,
    test_gdalwarp_lib_103,
    test_gdalwarp_lib_104,
    test_gdalwarp_lib_105,
    test_gdalwarp_lib_106,
    test_gdalwarp_lib_107,
    test_gdalwarp_lib_108,
    test_gdalwarp_lib_109,
    test_gdalwarp_lib_110,
    test_gdalwarp_lib_111,
    test_gdalwarp_lib_112,
    test_gdalwarp_lib_113,
    test_gdalwarp_lib_114,
    test_gdalwarp_lib_115,
    test_gdalwarp_lib_116,
    test_gdalwarp_lib_117,
    test_gdalwarp_lib_118,
    test_gdalwarp_lib_119,
    test_gdalwarp_lib_120,
    test_gdalwarp_lib_121,
    test_gdalwarp_lib_cleanup,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdalwarp_lib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
