#!/usr/bin/env pytest
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

import struct
import os


from osgeo import gdal, ogr, osr
import gdaltest
import ogrtest
import pytest

###############################################################################
# Simple test


def test_gdalwarp_lib_1():

    ds1 = gdal.Open('../gcore/data/byte.tif')
    dstDS = gdal.Warp('tmp/testgdalwarp1.tif', ds1)

    assert dstDS.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    dstDS = None


###############################################################################
# Test -of option

def test_gdalwarp_lib_2():

    ds1 = gdal.Open('../gcore/data/byte.tif')
    dstDS = gdal.Warp('tmp/testgdalwarp2.tif'.encode('ascii').decode('ascii'), [ds1], format='GTiff')

    assert dstDS.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    dstDS = None


###############################################################################
# Test -ot option

def test_gdalwarp_lib_3():

    ds1 = gdal.Open('../gcore/data/byte.tif')
    dstDS = gdal.Warp('', ds1, format='MEM', outputType=gdal.GDT_Int16)

    assert dstDS.GetRasterBand(1).DataType == gdal.GDT_Int16, 'Bad data type'

    assert dstDS.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    dstDS = None

###############################################################################
# Test -t_srs option


def test_gdalwarp_lib_4():

    ds1 = gdal.Open('../gcore/data/byte.tif')
    dstDS = gdal.Warp('', ds1, format='MEM', dstSRS='EPSG:32611')

    assert dstDS.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    dstDS = None

###############################################################################
# Test warping from GCPs without any explicit option


def test_gdalwarp_lib_5():

    ds = gdal.Open('../gcore/data/byte.tif')
    gcpList = [gdal.GCP(440720.000, 3751320.000, 0, 0, 0), gdal.GCP(441920.000, 3751320.000, 0, 20, 0), gdal.GCP(441920.000, 3750120.000, 0, 20, 20), gdal.GCP(440720.000, 3750120.000, 0, 0, 20)]
    ds1 = gdal.Translate('tmp/testgdalwarp_gcp.tif', ds, outputSRS='EPSG:26711', GCPs=gcpList)
    dstDS = gdal.Warp('', ds1, format='MEM', tps=True)

    assert dstDS.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    assert gdaltest.geotransform_equals(ds.GetGeoTransform(), dstDS.GetGeoTransform(), 1e-9), \
        'Bad geotransform'

    dstDS = None


###############################################################################
# Test warping from GCPs with -tps

def test_gdalwarp_lib_6():

    ds1 = gdal.Open('tmp/testgdalwarp_gcp.tif')
    dstDS = gdal.Warp('', ds1, format='MEM', tps=True)

    assert dstDS.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    assert gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), dstDS.GetGeoTransform(), 1e-9), \
        'Bad geotransform'

    dstDS = None


###############################################################################
# Test -tr

def test_gdalwarp_lib_7():

    ds1 = gdal.Open('tmp/testgdalwarp_gcp.tif')
    dstDS = gdal.Warp('', [ds1], format='MEM', xRes=120, yRes=120)
    assert dstDS is not None

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    assert gdaltest.geotransform_equals(expected_gt, dstDS.GetGeoTransform(), 1e-9), \
        'Bad geotransform'

    dstDS = None

###############################################################################
# Test -ts


def test_gdalwarp_lib_8():

    ds1 = gdal.Open('tmp/testgdalwarp_gcp.tif')
    dstDS = gdal.Warp('', [ds1], format='MEM', width=10, height=10)
    assert dstDS is not None

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    assert gdaltest.geotransform_equals(expected_gt, dstDS.GetGeoTransform(), 1e-9), \
        'Bad geotransform'

    dstDS = None

###############################################################################
# Test -te


def test_gdalwarp_lib_9():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM', outputBounds=[440720.000, 3750120.000, 441920.000, 3751320.000])

    assert gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), ds.GetGeoTransform(), 1e-9), \
        'Bad geotransform'

    ds = None

###############################################################################
# Test -rn


def test_gdalwarp_lib_10():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM', width=40, height=40, resampleAlg=gdal.GRIORA_NearestNeighbour)

    assert ds.GetRasterBand(1).Checksum() == 18784, 'Bad checksum'

    ds = None

###############################################################################
# Test -rb


def test_gdalwarp_lib_11():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM', width=40, height=40, resampleAlg=gdal.GRIORA_Bilinear)

    ref_ds = gdal.Open('ref_data/testgdalwarp11.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)
    ref_ds = None

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        pytest.fail('Image too different from reference')

    ds = None

###############################################################################
# Test -rc


def test_gdalwarp_lib_12():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM', width=40, height=40, resampleAlg=gdal.GRIORA_Cubic)

    ref_ds = gdal.Open('ref_data/testgdalwarp12.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)
    ref_ds = None

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        pytest.fail('Image too different from reference')

    ds = None

###############################################################################
# Test -rcs


def test_gdalwarp_lib_13():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM', width=40, height=40, resampleAlg=gdal.GRIORA_CubicSpline)

    ref_ds = gdal.Open('ref_data/testgdalwarp13.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)
    ref_ds = None

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        pytest.fail('Image too different from reference')

    ds = None

###############################################################################
# Test -r lanczos


def test_gdalwarp_lib_14():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM', width=40, height=40, resampleAlg=gdal.GRIORA_Lanczos)

    ref_ds = gdal.Open('ref_data/testgdalwarp14.tif')
    maxdiff = gdaltest.compare_ds(ds, ref_ds, verbose=0)
    ref_ds = None

    if maxdiff > 1:
        gdaltest.compare_ds(ds, ref_ds, verbose=1)
        pytest.fail('Image too different from reference')

    ds = None

###############################################################################
# Test -dstnodata


def test_gdalwarp_lib_15():

    ds = gdal.Warp('', 'tmp/testgdalwarp_gcp.tif', format='MEM', dstSRS='EPSG:32610', dstNodata=1)

    assert ds.GetRasterBand(1).GetNoDataValue() == 1, 'Bad nodata value'

    assert ds.GetRasterBand(1).Checksum() == 4523, 'Bad checksum'

    ds = None

###############################################################################
# Test -of VRT which is a special case


def test_gdalwarp_lib_16():

    ds = gdal.Warp('/vsimem/test_gdalwarp_lib_16.vrt', 'tmp/testgdalwarp_gcp.tif', format='VRT')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    ds = None

    gdal.Unlink('/vsimem/test_gdalwarp_lib_16.vrt')

    # Cannot write file
    with gdaltest.error_handler():
        ds = gdal.Warp('/i_dont/exist/test_gdalwarp_lib_16.vrt', 'tmp/testgdalwarp_gcp.tif', format='VRT')
    assert ds is None

###############################################################################
# Test -dstalpha


def test_gdalwarp_lib_17():

    ds = gdal.Warp('', '../gcore/data/rgbsmall.tif', format='MEM', dstAlpha=True)
    assert ds is not None

    assert ds.GetRasterBand(4) is not None, 'No alpha band generated'

    ds = None

###############################################################################
# Test -et 0 which is a special case


def test_gdalwarp_lib_19():

    ds = gdal.Warp('', 'tmp/testgdalwarp_gcp.tif', format='MEM', errorThreshold=0)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    ds = None

###############################################################################
# Test cutline from OGR datasource.


def test_gdalwarp_lib_21():

    ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format='MEM', cutlineDSName='data/cutline.vrt', cutlineLayer='cutline')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 19139, 'Bad checksum'

    ds = None

###############################################################################
# Test cutline with ALL_TOUCHED enabled.


def test_gdalwarp_lib_23():

    ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format='MEM', warpOptions=['CUTLINE_ALL_TOUCHED=TRUE'], cutlineDSName='data/cutline.vrt', cutlineLayer='cutline')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 20123, 'Bad checksum'

    ds = None

###############################################################################
# Test -tap


def test_gdalwarp_lib_32():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM', targetAlignedPixels=True, xRes=100, yRes=50)
    assert ds is not None

    expected_gt = (440700.0, 100.0, 0.0, 3751350.0, 0.0, -50.0)
    got_gt = ds.GetGeoTransform()
    assert gdaltest.geotransform_equals(expected_gt, got_gt, 1e-9), 'Bad geotransform'

    assert ds.RasterXSize == 13 and ds.RasterYSize == 25, \
        ('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize))

    ds = None

###############################################################################
# Test warping multiple sources


def test_gdalwarp_lib_34():

    srcds1 = gdal.Translate('', '../gcore/data/byte.tif', format='MEM', srcWin=[0, 0, 10, 20])
    srcds2 = gdal.Translate('', '../gcore/data/byte.tif', format='MEM', srcWin=[10, 0, 10, 20])
    ds = gdal.Warp('', [srcds1, srcds2], format='MEM')

    cs = ds.GetRasterBand(1).Checksum()
    gt = ds.GetGeoTransform()
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize
    ds = None

    assert xsize == 20 and ysize == 20, 'bad dimensions'

    assert cs == 4672, 'bad checksum'

    expected_gt = (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)
    for i in range(6):
        assert gt[i] == pytest.approx(expected_gt[i], abs=1e-5), 'bad gt'

    
###############################################################################
# Test -te_srs


def test_gdalwarp_lib_45():

    ds = gdal.Warp('', ['../gcore/data/byte.tif'], format='MEM', outputBounds=[-117.641087629972, 33.8915301685897, -117.628190189534, 33.9024195619201], outputBoundsSRS='EPSG:4267')
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = None

###############################################################################
# Test -crop_to_cutline


def test_gdalwarp_lib_46():

    ds = gdal.Warp('', ['../gcore/data/utmsmall.tif'], format='MEM', cutlineDSName='data/cutline.vrt', cropToCutline=True)
    assert ds.GetRasterBand(1).Checksum() == 18837, 'Bad checksum'

    ds = None

    # Precisely test output raster bounds in no raster reprojection ccase

    src_ds = gdal.Translate('', '../gcore/data/byte.tif', format='MEM',
                            outputBounds=[2, 49, 3, 48], outputSRS='EPSG:4326')

    cutlineDSName = '/vsimem/test_gdalwarp_lib_46.json'
    cutline_ds = ogr.GetDriverByName('GeoJSON').CreateDataSource(cutlineDSName)
    cutline_lyr = cutline_ds.CreateLayer('cutline')
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2.13 48.13,2.83 48.13,2.83 48.83,2.13 48.83,2.13 48.13))'))
    cutline_lyr.CreateFeature(f)
    f = None
    cutline_lyr = None
    cutline_ds = None

    # No CUTLINE_ALL_TOUCHED: the extent should be smaller than the cutline
    ds = gdal.Warp('', src_ds, format='MEM', cutlineDSName=cutlineDSName,
                   cropToCutline=True)
    got_gt = ds.GetGeoTransform()
    expected_gt = (2.15, 0.05, 0.0, 48.8, 0.0, -0.05)
    assert max([abs(got_gt[i]-expected_gt[i]) for i in range(6)]) <= 1e-8
    assert ds.RasterXSize == 13 and ds.RasterYSize == 13

    # Same but with CUTLINE_ALL_TOUCHED=YES: the extent should be larger
    # than the cutline
    ds = gdal.Warp('', src_ds, format='MEM', cutlineDSName=cutlineDSName,
                   cropToCutline=True,
                   warpOptions=['CUTLINE_ALL_TOUCHED=YES'])
    got_gt = ds.GetGeoTransform()
    expected_gt = (2.1, 0.05, 0.0, 48.85, 0.0, -0.05)
    assert max([abs(got_gt[i]-expected_gt[i]) for i in range(6)]) <= 1e-8
    assert ds.RasterXSize == 15 and ds.RasterYSize == 15

    # Test numeric stability when the cutline is exactly on pixel boundaries
    cutline_ds = ogr.GetDriverByName('GeoJSON').CreateDataSource(cutlineDSName)
    cutline_lyr = cutline_ds.CreateLayer('cutline')
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2.15 48.15,2.85 48.15,2.85 48.85,2.15 48.85,2.15 48.15))'))
    cutline_lyr.CreateFeature(f)
    f = None
    cutline_lyr = None
    cutline_ds = None

    for warpOptions in [[], ['CUTLINE_ALL_TOUCHED=YES']]:
        ds = gdal.Warp('', src_ds, format='MEM', cutlineDSName=cutlineDSName,
                       cropToCutline=True, warpOptions=warpOptions)
        got_gt = ds.GetGeoTransform()
        expected_gt = (2.15, 0.05, 0.0, 48.85, 0.0, -0.05)
        assert max([abs(got_gt[i]-expected_gt[i]) for i in range(6)]) <= 1e-8
        assert ds.RasterXSize == 14 and ds.RasterYSize == 14

    gdal.Unlink(cutlineDSName)

###############################################################################
# Test -crop_to_cutline -tr X Y -wo CUTLINE_ALL_TOUCHED=YES (fixes for #1360)


def test_gdalwarp_lib_cutline_all_touched_single_pixel():

    cutlineDSName = '/vsimem/test_gdalwarp_lib_cutline_all_touched_single_pixel.json'
    cutline_ds = ogr.GetDriverByName('GeoJSON').CreateDataSource(cutlineDSName)
    cutline_lyr = cutline_ds.CreateLayer('cutline')
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2.15 48.15,2.15000001 48.15000001,2.15 48.15000001,2.15 48.15))'))
    cutline_lyr.CreateFeature(f)
    f = None
    cutline_lyr = None
    cutline_ds = None

    src_ds = gdal.Translate('', '../gcore/data/byte.tif', format='MEM',
                            outputBounds=[2, 49, 3, 48], outputSRS='EPSG:4326')

    ds = gdal.Warp('', src_ds, format='MEM', cutlineDSName=cutlineDSName,
                   cropToCutline=True, warpOptions=['CUTLINE_ALL_TOUCHED=YES'],
                   xRes=0.001, yRes=0.001)
    got_gt = ds.GetGeoTransform()
    expected_gt = (2.15, 0.001, 0.0, 48.151, 0.0, -0.001)
    assert max([abs(got_gt[i]-expected_gt[i]) for i in range(6)]) <= 1e-8, got_gt
    assert ds.RasterXSize == 1 and ds.RasterYSize == 1

    gdal.Unlink(cutlineDSName)

###############################################################################
# Test callback


def mycallback(pct, msg, user_data):
    # pylint: disable=unused-argument
    user_data[0] = pct
    return 1


def test_gdalwarp_lib_100():

    tab = [0]
    ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM', callback=mycallback, callback_data=tab)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    assert tab[0] == 1.0, 'Bad percentage'

    ds = None

###############################################################################
# Test with color table


def test_gdalwarp_lib_101():

    ds = gdal.Warp('', '../gdrivers/data/small_world_pct.tif', format='MEM')
    assert ds.GetRasterBand(1).GetColorTable() is not None, 'Did not get color table'

###############################################################################
# Test with a dataset with no bands


def test_gdalwarp_lib_102():

    no_band_ds = gdal.GetDriverByName('MEM').Create('no band', 1, 1, 0)
    with gdaltest.error_handler():
        ds = gdal.Warp('', ['../gdrivers/data/small_world_pct.tif', no_band_ds], format='MEM')
    assert ds is None, 'Did not expected dataset'

###############################################################################
# Test failed transformer


def test_gdalwarp_lib_103():

    with gdaltest.error_handler():
        ds = gdal.Warp('', ['../gdrivers/data/small_world_pct.tif', '../gcore/data/stefan_full_rgba.tif'], format='MEM')
    assert ds is None, 'Did not expected dataset'

###############################################################################
# Test no usable source image


def test_gdalwarp_lib_104():

    with gdaltest.error_handler():
        ds = gdal.Warp('', [], format='MEM')
    assert ds is None, 'Did not expected dataset'

###############################################################################
# Test failure in GDALSuggestedWarpOutput2


def test_gdalwarp_lib_105():

    # with proj 4.9.3 this will success. We limit the width and height
    # otherwise a very big raster will be created with 4.9.3 which may cause
    # hangups in Travis MacOSX
    with gdaltest.error_handler():
        gdal.Warp('', ['../gdrivers/data/small_world_pct.tif', '../gcore/data/byte.tif'], format='MEM', dstSRS='EPSG:32645', width=100, height=100)

    
###############################################################################
# Test failure in creation


def test_gdalwarp_lib_106():

    with gdaltest.error_handler():
        ds = gdal.Warp('/not_existing_dir/not_existing_file', ['../gdrivers/data/small_world_pct.tif', '../gcore/data/byte.tif'])
    assert ds is None, 'Did not expected dataset'

###############################################################################
# Test forced width only


def test_gdalwarp_lib_107():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM', width=20)
    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

###############################################################################
# Test forced height only


def test_gdalwarp_lib_108():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM', height=20)
    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

###############################################################################
# Test wrong cutline name


def test_gdalwarp_lib_109():

    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM', cutlineDSName='/does/not/exist')
    assert ds is None, 'Did not expected dataset'

###############################################################################
# Test wrong cutline layer name


def test_gdalwarp_lib_110():

    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM', cutlineDSName='data/cutline.vrt', cutlineLayer='wrong_name')
    assert ds is None, 'Did not expected dataset'

###############################################################################
# Test cutline SQL


def test_gdalwarp_lib_111():

    ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format='MEM', cutlineDSName='data/cutline.vrt', cutlineSQL='SELECT * FROM cutline', cutlineWhere='1 = 1')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 19139, 'Bad checksum'

    ds = None

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
        ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format='MEM', cutlineDSName='/vsimem/cutline.shp', cutlineSQL='SELECT * FROM cutline')
    assert ds is None, 'Did not expected dataset'
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/cutline.shp')

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
        ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format='MEM', cutlineDSName='/vsimem/cutline.shp')
    assert ds is None, 'Did not expected dataset'
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/cutline.shp')

###############################################################################
# Test cutline without feature


def test_gdalwarp_lib_114():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/cutline.shp')
    ds.CreateLayer('cutline')
    ds = None
    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format='MEM', cutlineDSName='/vsimem/cutline.shp')
    assert ds is None, 'Did not expected dataset'
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/cutline.shp')

###############################################################################
# Test source dataset without band


def test_gdalwarp_lib_115():

    no_band_ds = gdal.GetDriverByName('MEM').Create('no band', 1, 1, 0)
    out_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 0)

    with gdaltest.error_handler():
        ret = gdal.Warp(out_ds, no_band_ds, cutlineDSName='data/cutline.vrt', cutlineLayer='cutline')
    assert ret == 0, 'Expected failure'

###############################################################################
# Test failed cropToCutline due to invalid SRC_SRS


def test_gdalwarp_lib_116():

    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format='MEM', cutlineDSName='data/cutline.vrt', cutlineLayer='cutline', cropToCutline=True, transformerOptions=['SRC_SRS=invalid'])
    assert ds is None, 'Did not expected dataset'

###############################################################################
# Test failed cropToCutline due to invalid DST_SRS


def test_gdalwarp_lib_117():

    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format='MEM', cutlineDSName='data/cutline.vrt', cutlineLayer='cutline', cropToCutline=True, transformerOptions=['DST_SRS=invalid'])
    assert ds is None, 'Did not expected dataset'

###############################################################################
# Test failed cropToCutline due to no source raster


def test_gdalwarp_lib_118():

    with gdaltest.error_handler():
        ds = gdal.Warp('', [], format='MEM', cutlineDSName='data/cutline.vrt', cutlineLayer='cutline', cropToCutline=True)
    assert ds is None, 'Did not expected dataset'

###############################################################################
# Test failed cropToCutline due to source raster without projection


def test_gdalwarp_lib_119():

    no_proj_ds = gdal.GetDriverByName('MEM').Create('no_proj_ds', 1, 1)
    with gdaltest.error_handler():
        ds = gdal.Warp('', no_proj_ds, format='MEM', cutlineDSName='data/cutline.vrt', cutlineLayer='cutline', cropToCutline=True)
    assert ds is None, 'Did not expected dataset'

###############################################################################
# Test failed cropToCutline due to source raster with dummy projection


def test_gdalwarp_lib_120():

    dummy_proj_ds = gdal.GetDriverByName('MEM').Create('no_proj_ds', 1, 1)
    dummy_proj_ds.SetProjection('dummy')
    with gdaltest.error_handler():
        ds = gdal.Warp('', dummy_proj_ds, format='MEM', cutlineDSName='data/cutline.vrt', cutlineLayer='cutline', cropToCutline=True)
    assert ds is None, 'Did not expected dataset'

###############################################################################
# Test internal wrappers


def test_gdalwarp_lib_121():

    # No option
    with gdaltest.error_handler():
        gdal.wrapper_GDALWarpDestName('', [], None)

    # Will create an implicit options structure
    with gdaltest.error_handler():
        gdal.wrapper_GDALWarpDestName('', [], None, gdal.TermProgress_nocb)

    # Null dest name
    try:
        gdal.wrapper_GDALWarpDestName(None, [], None)
    except:
        pass

    # No option
    with gdaltest.error_handler():
        gdal.wrapper_GDALWarpDestDS(gdal.GetDriverByName('MEM').Create('', 1, 1), [], None)

    # Will create an implicit options structure
    with gdaltest.error_handler():
        gdal.wrapper_GDALWarpDestDS(gdal.GetDriverByName('MEM').Create('', 1, 1), [], None, gdal.TermProgress_nocb)

    
###############################################################################
# Test unnamed output VRT


def test_gdalwarp_lib_122():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format='VRT')
    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

###############################################################################
# Test failure during warping


def test_gdalwarp_lib_123():

    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/byte_truncated.tif', format='MEM')
    assert ds is None

###############################################################################
# Test warping to dataset with existing nodata


def test_gdalwarp_lib_124():

    src_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    src_ds.SetGeoTransform([10, 1, 0, 10, 0, -1])
    src_ds.GetRasterBand(1).SetNoDataValue(12)
    src_ds.GetRasterBand(1).Fill(12)

    out_ds = gdal.GetDriverByName('MEM').Create('', 2, 2)
    out_ds.SetGeoTransform([10, 1, 0, 10, 0, -1])
    out_ds.GetRasterBand(1).SetNoDataValue(21)
    out_ds.GetRasterBand(1).Fill(21)
    expected_cs = out_ds.GetRasterBand(1).Checksum()

    gdal.Warp(out_ds, src_ds)

    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == expected_cs, 'Bad checksum'

###############################################################################
# Test that statistics are not propagated


def test_gdalwarp_lib_125():

    for i in range(3):

        src_ds_1 = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds_1.SetGeoTransform([10, 1, 0, 10, 0, -1])
        if i == 1 or i == 3:
            src_ds_1.GetRasterBand(1).SetMetadataItem('STATISTICS_MINIUM', '5')

        src_ds_2 = gdal.GetDriverByName('MEM').Create('', 2, 2)
        src_ds_2.SetGeoTransform([10, 1, 0, 10, 0, -1])
        if i == 2 or i == 3:
            src_ds_2.GetRasterBand(1).SetMetadataItem('STATISTICS_MINIUM', '5')

        out_ds = gdal.Warp('', [src_ds_1, src_ds_2], format='MEM')

        assert out_ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIUM') is None, i

    
###############################################################################
# Test cutline with invalid geometry


def test_gdalwarp_lib_126():

    if not ogrtest.have_geos():
        pytest.skip()

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/cutline.shp')
    lyr = ds.CreateLayer('cutline')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,1 1,0 1,1 0,0 0))'))  # Self intersecting
    lyr.CreateFeature(f)
    f = None
    ds = None
    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format='MEM', cutlineDSName='/vsimem/cutline.shp')
    assert ds is None, 'Did not expected dataset'
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/cutline.shp')

###############################################################################
# Test -srcnodata (#6315)


def test_gdalwarp_lib_127():

    ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM', srcNodata=1)
    assert ds.GetRasterBand(1).GetNoDataValue() == 1, 'bad nodata value'
    assert ds.GetRasterBand(1).Checksum() == 4672, 'bad checksum'

###############################################################################
# Test automatic densification of cutline (#6375)


def test_gdalwarp_lib_128():

    mem_ds = gdal.GetDriverByName('MEM').Create('', 1177, 4719)
    rpc = ["HEIGHT_OFF=109",
           "LINE_NUM_COEFF=-0.001245683 -0.09427649 -1.006342 -1.954469e-05 0.001033926 2.020534e-08 -3.845472e-07 -0.002075817 0.0005520694 0 -4.642442e-06 -3.271793e-06 2.705977e-05 -7.634384e-07 -2.132832e-05 -3.248862e-05 -8.17894e-06 -3.678094e-07 2.002032e-06 3.693162e-08",
           "LONG_OFF=7.1477",
           "SAMP_DEN_COEFF=1 0.01415176 -0.003715018 -0.001205632 -0.0007738299 4.057763e-05 -1.649126e-05 0.0001453584 0.0001628194 -7.354731e-05 4.821444e-07 -4.927701e-06 -1.942371e-05 -2.817499e-06 1.946396e-06 3.04243e-06 2.362282e-07 -2.5371e-07 -1.36993e-07 1.132432e-07",
           "LINE_SCALE=2360",
           "SAMP_NUM_COEFF=0.04337163 1.775948 -0.87108 0.007425391 0.01783631 0.0004057179 -0.000184695 -0.04257537 -0.01127869 -1.531228e-06 1.017961e-05 0.000572344 -0.0002941 -0.0001301705 -0.0003289546 5.394918e-05 6.388447e-05 -4.038289e-06 -7.525785e-06 -5.431241e-07",
           "LONG_SCALE=0.8383",
           "SAMP_SCALE=593",
           "SAMP_OFF=589",
           "LAT_SCALE=1.4127",
           "LAT_OFF=33.8992",
           "LINE_OFF=2359",
           "LINE_DEN_COEFF=1 0.0007273139 -0.0006006867 -4.272095e-07 2.578717e-05 4.718479e-06 -2.116976e-06 -1.347805e-05 -2.209958e-05 8.131258e-06 -7.290143e-08 5.105109e-08 -7.353388e-07 0 2.131142e-06 9.697701e-08 1.237039e-08 7.153246e-08 6.758015e-08 5.811124e-08",
           "HEIGHT_SCALE=96.3"]
    mem_ds.SetMetadata(rpc, "RPC")
    mem_ds.GetRasterBand(1).Fill(255)

    cutlineDSName = '/vsimem/test_gdalwarp_lib_128.json'
    cutline_ds = ogr.GetDriverByName('GeoJSON').CreateDataSource(cutlineDSName)
    cutline_lyr = cutline_ds.CreateLayer('cutline')
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON ((7.2151 32.51930,7.214316 32.58116,7.216043 32.59476,7.21666 32.5193,7.2151 32.51930))'))
    cutline_lyr.CreateFeature(f)
    f = None
    cutline_lyr = None
    cutline_ds = None

    # Default is GDALWARP_DENSIFY_CUTLINE=YES
    ds = gdal.Warp('', mem_ds, format='MEM', cutlineDSName=cutlineDSName,
                   dstSRS='EPSG:4326',
                   outputBounds=[7.2, 32.52, 7.217, 32.59],
                   xRes=0.000226555, yRes=0.000226555,
                   transformerOptions=['RPC_DEM=data/test_gdalwarp_lib_128_dem.tif'])
    cs = ds.GetRasterBand(1).Checksum()

    assert cs == 4248, 'bad checksum'

    # Below steps depend on GEOS
    if not ogrtest.have_geos():
        gdal.Unlink(cutlineDSName)
        return

    gdal.SetConfigOption('GDALWARP_DENSIFY_CUTLINE', 'ONLY_IF_INVALID')
    ds = gdal.Warp('', mem_ds, format='MEM', cutlineDSName=cutlineDSName,
                   dstSRS='EPSG:4326',
                   outputBounds=[7.2, 32.52, 7.217, 32.59],
                   xRes=0.000226555, yRes=0.000226555,
                   transformerOptions=['RPC_DEM=data/test_gdalwarp_lib_128_dem.tif'])
    gdal.SetConfigOption('GDALWARP_DENSIFY_CUTLINE', None)
    cs = ds.GetRasterBand(1).Checksum()

    assert cs == 4248, 'bad checksum'

    gdal.SetConfigOption('GDALWARP_DENSIFY_CUTLINE', 'NO')
    with gdaltest.error_handler():
        ds = gdal.Warp('', mem_ds, format='MEM', cutlineDSName=cutlineDSName,
                       dstSRS='EPSG:4326',
                       outputBounds=[7.2, 32.52, 7.217, 32.59],
                       xRes=0.000226555, yRes=0.000226555,
                       transformerOptions=['RPC_DEM=data/test_gdalwarp_lib_128_dem.tif'])
    gdal.SetConfigOption('GDALWARP_DENSIFY_CUTLINE', None)
    assert ds is None, 'expected none return'

    gdal.Unlink(cutlineDSName)

###############################################################################
# Test automatic densification of cutline, but with initial guess leading
# to an invalid geometry (#6375)


def test_gdalwarp_lib_129():
    if not ogrtest.have_geos():
        pytest.skip()

    mem_ds = gdal.GetDriverByName('MEM').Create('', 1000, 2000)
    rpc = ["HEIGHT_OFF=1767",
           "LINE_NUM_COEFF=0.0004430579 -0.06200816 -1.007087 1.614683e-05 0.0009263463 -1.003745e-07 -2.346893e-06 -0.001179024 -0.0007413534 0 9.41488e-08 -4.566652e-07 2.895947e-05 -2.925327e-07 -2.308839e-05 -1.502702e-05 -4.775127e-06 0 4.290483e-07 2.850458e-08",
           "LONG_OFF=-.2282",
           "SAMP_DEN_COEFF=1 -0.01907542 0.01651069 -0.001340671 -0.0005495095 -1.072863e-05 -1.157626e-05 0.0003737224 0.0002712591 -0.0001363199 3.614417e-08 3.584749e-06 9.175671e-06 2.661593e-06 -1.045511e-05 -1.293648e-06 -2.769964e-06 5.931109e-07 -1.018687e-07 2.366109e-07",
           "LINE_SCALE=11886",
           "SAMP_NUM_COEFF=0.007334337 1.737166 -0.7954719 -0.004635387 -0.007478255 0.0006381186 -0.0003313475 0.0002313095 -0.002883101 -1.625925e-06 -6.409095e-06 -0.000403506 -0.0004441055 -0.0002360882 8.940442e-06 -0.0001780485 0.0001081517 -6.592931e-06 2.642496e-06 6.316508e-07",
           "LONG_SCALE=0.6996",
           "SAMP_SCALE=2945",
           "SAMP_OFF=2926",
           "LAT_SCALE=1.4116",
           "LAT_OFF=.4344",
           "LINE_OFF=-115",
           "LINE_DEN_COEFF=1 0.0008882352 -0.0002437686 -2.380782e-06 2.69128e-05 0 2.144654e-07 -2.093549e-05 -7.055149e-06 4.740057e-06 0 -1.588607e-08 -1.397592e-05 0 -7.717698e-07 6.505002e-06 0 -1.225041e-08 3.608499e-08 -4.463376e-08",
           "HEIGHT_SCALE=1024"]

    mem_ds.SetMetadata(rpc, "RPC")
    mem_ds.GetRasterBand(1).Fill(255)

    cutlineDSName = '/vsimem/test_gdalwarp_lib_129.json'
    cutline_ds = ogr.GetDriverByName('GeoJSON').CreateDataSource(cutlineDSName)
    cutline_lyr = cutline_ds.CreateLayer('cutline')
    f = ogr.Feature(cutline_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON ((-0.873086 0.511332,-0.626502 0.507654,-0.630715 0.282053,-0.876863 0.285693,-0.873086 0.511332))'))
    cutline_lyr.CreateFeature(f)
    f = None
    cutline_lyr = None
    cutline_ds = None

    ds = gdal.Warp('', mem_ds, format='MEM', cutlineDSName=cutlineDSName,
                   dstSRS='EPSG:4326',
                   outputBounds=[-1, 0, 0, 1],
                   xRes=0.01, yRes=0.01,
                   transformerOptions=['RPC_DEM=data/test_gdalwarp_lib_129_dem.vrt'])
    cs = ds.GetRasterBand(1).Checksum()

    assert cs == 399, 'bad checksum'

    gdal.Unlink(cutlineDSName)

###############################################################################
# Test automatic detection and setting of alpha channel, and setting RGB on
# GTiff output


def test_gdalwarp_lib_130():

    src_ds = gdal.GetDriverByName('GTiff').Create(
        '/vsimem/test_gdalwarp_lib_130.tif', 1, 1, 5, options=['PHOTOMETRIC=RGB'])
    src_ds.SetGeoTransform([100, 1, 0, 200, 0, -1])
    src_ds.GetRasterBand(5).SetColorInterpretation(gdal.GCI_AlphaBand)
    src_ds.GetRasterBand(1).Fill(1)
    src_ds.GetRasterBand(2).Fill(2)
    src_ds.GetRasterBand(3).Fill(3)
    src_ds.GetRasterBand(4).Fill(4)
    src_ds.GetRasterBand(5).Fill(255)

    ds = gdal.Warp('/vsimem/test_gdalwarp_lib_130_dst.tif', src_ds)
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand, \
        'bad color interpretation'
    assert ds.GetRasterBand(5).GetColorInterpretation() == gdal.GCI_AlphaBand, \
        'bad color interpretation'
    expected_val = [1, 2, 3, 4, 255]
    for i in range(5):
        data = struct.unpack('B' * 1, ds.GetRasterBand(i + 1).ReadRaster())[0]
        assert data == expected_val[i], 'bad checksum'

    # Wrap onto existing file
    for i in range(5):
        ds.GetRasterBand(i + 1).Fill(0)
    gdal.Warp(ds, src_ds)
    for i in range(5):
        data = struct.unpack('B' * 1, ds.GetRasterBand(i + 1).ReadRaster())[0]
        assert data == expected_val[i], 'bad checksum'

    src_ds = None
    ds = None

    assert gdal.VSIStatL('/vsimem/test_gdalwarp_lib_130_dst.tif.aux.xml') is None, \
        'got PAM file'

    gdal.Unlink('/vsimem/test_gdalwarp_lib_130.tif')
    gdal.Unlink('/vsimem/test_gdalwarp_lib_130_dst.tif')

###############################################################################
# Test -nosrcalpha


def test_gdalwarp_lib_131():

    src_ds = gdal.GetDriverByName('GTiff').Create(
        '/vsimem/test_gdalwarp_lib_131.tif', 1, 1, 2)
    src_ds.SetGeoTransform([100, 1, 0, 200, 0, -1])
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    src_ds.GetRasterBand(1).Fill(1)
    src_ds.GetRasterBand(2).Fill(0)

    ds = gdal.Warp('/vsimem/test_gdalwarp_lib_131_dst.tif', src_ds, options='-nosrcalpha')
    expected_val = [1, 0]
    for i in range(2):
        data = struct.unpack('B' * 1, ds.GetRasterBand(i + 1).ReadRaster())[0]
        assert data == expected_val[i], 'bad checksum'
    src_ds = None
    ds = None
    gdal.Unlink('/vsimem/test_gdalwarp_lib_131.tif')
    gdal.Unlink('/vsimem/test_gdalwarp_lib_131_dst.tif')
    gdal.Unlink('/vsimem/test_gdalwarp_lib_131_dst.tif.aux.xml')

###############################################################################
# Test that alpha blending works by warping onto an existing dataset
# with alpha > 0 and < 255


def test_gdalwarp_lib_132():

    for dt in [gdal.GDT_Byte, gdal.GDT_Float32]:
        src_ds = gdal.GetDriverByName('GTiff').Create(
            '/vsimem/test_gdalwarp_lib_132.tif', 33, 1, 2, dt)
        src_ds.SetGeoTransform([100, 1, 0, 200, 0, -1])
        src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)

        ds = gdal.Translate('/vsimem/test_gdalwarp_lib_132_dst.tif', src_ds)
        dst_grey = 60
        dst_alpha = 100
        ds.GetRasterBand(1).Fill(dst_grey)
        ds.GetRasterBand(2).Fill(dst_alpha)

        src_grey = 170
        src_alpha = 200
        src_ds.GetRasterBand(1).Fill(src_grey)
        src_ds.GetRasterBand(2).Fill(src_alpha)
        gdal.Warp(ds, src_ds)
        expected_alpha = int(src_alpha + dst_alpha * (255 - src_alpha) / 255. + 0.5)
        expected_grey = int((src_grey * src_alpha + dst_grey * dst_alpha * (255 - src_alpha) / 255.) / expected_alpha + 0.5)
        expected_val = [expected_grey, expected_alpha]
        for i in range(2):
            for x in range(33):
                data = struct.unpack('B' * 1, ds.GetRasterBand(i + 1).ReadRaster(i, 0, 1, 1, buf_type=gdal.GDT_Byte))[0]
                if data != pytest.approx(expected_val[i], abs=1):
                    print(dt)
                    print(x)
                    pytest.fail('bad checksum')
        ds = None

        src_ds = None

        gdal.Unlink('/vsimem/test_gdalwarp_lib_132.tif')
        gdal.Unlink('/vsimem/test_gdalwarp_lib_132_dst.tif')
        gdal.Unlink('/vsimem/test_gdalwarp_lib_132_dst.tif.aux.xml')

    
###############################################################################
# Test cutline with multiple touching polygons


def test_gdalwarp_lib_133():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/test_gdalwarp_lib_133.shp')
    lyr = ds.CreateLayer('cutline')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,1 0,1 1,0 1,0 0))'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((1 0,2 0,2 1,1 1,1 0))'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    src_ds = gdal.GetDriverByName('MEM').Create('', 4, 1)
    src_ds.SetGeoTransform([0, 1, 0, 1, 0, -1])
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.Warp('', src_ds, format='MEM', cutlineDSName='/vsimem/test_gdalwarp_lib_133.shp')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 5, 'Bad checksum'

    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/test_gdalwarp_lib_133.shp')

###############################################################################
# Test SRC_METHOD=NO_GEOTRANSFORM and DST_METHOD=NO_GEOTRANSFORM (#6721)


def test_gdalwarp_lib_134():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/test_gdalwarp_lib_134.shp')
    lyr = ds.CreateLayer('cutline')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((2 2,2 18,18 18,18 2,2 2))'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    src_src_ds = gdal.Open('../gcore/data/byte.tif')
    src_ds = gdal.GetDriverByName('MEM').Create('', 20, 20)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 20, 20, src_src_ds.GetRasterBand(1).ReadRaster())
    ds = gdal.Warp('', src_ds, format='MEM', transformerOptions=['SRC_METHOD=NO_GEOTRANSFORM', 'DST_METHOD=NO_GEOTRANSFORM'], outputBounds=[1, 2, 4, 6])
    assert ds is not None

    assert ds.GetRasterBand(1).ReadRaster() == src_src_ds.GetRasterBand(1).ReadRaster(1, 2, 4 - 1, 6 - 2), \
        'Bad checksum'

    ds = None

    ds = gdal.Warp('', src_ds, format='MEM', transformerOptions=['SRC_METHOD=NO_GEOTRANSFORM', 'DST_METHOD=NO_GEOTRANSFORM'], cutlineDSName='/vsimem/test_gdalwarp_lib_134.shp', cropToCutline=True)
    assert ds is not None

    assert ds.GetRasterBand(1).ReadRaster() == src_src_ds.GetRasterBand(1).ReadRaster(2, 2, 16, 16), \
        'Bad checksum'

    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/test_gdalwarp_lib_134.shp')

###############################################################################
# Test vertical datum shift


def test_gdalwarp_lib_135():

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    src_ds.GetRasterBand(1).Fill(100)

    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")

    src_ds_longlat = gdal.GetDriverByName('MEM').Create('', 2, 1)
    src_ds_longlat.SetProjection(sr.ExportToWkt())
    src_ds_longlat.SetGeoTransform([-180, 180, 0, 90, 0, -180])
    src_ds_longlat.GetRasterBand(1).Fill(100)

    grid_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/grid.tif', 1, 1)
    grid_ds.SetProjection(sr.ExportToWkt())
    grid_ds.SetGeoTransform([-180, 360, 0, 90, 0, -180])
    grid_ds.GetRasterBand(1).Fill(20)
    grid_ds = None

    grid_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/grid2.tif', 1, 1)
    grid_ds.SetProjection(sr.ExportToWkt())
    grid_ds.SetGeoTransform([-180, 360, 0, 90, 0, -180])
    grid_ds.GetRasterBand(1).Fill(5)
    grid_ds = None

    gdal.GetDriverByName('GTiff').Create('/vsimem/ungeoref_grid.tif', 1, 1)

    # Forward transform
    ds = gdal.Warp('', src_ds, format='MEM',
                   srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/grid.tif +vunits=m +no_defs',
                   dstSRS='EPSG:4979')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, 'Bad value'

    ds = gdal.Warp('', src_ds_longlat, format='MEM',
                   srcSRS='+proj=longlat +datum=WGS84 +geoidgrids=/vsimem/grid.tif +vunits=m +no_defs',
                   dstSRS='EPSG:4979')
    assert ds.GetGeoTransform() == (-180, 180, 0, 90, 0, -180)
    data = struct.unpack('B' * 2, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, 'Bad value'

    # Inverse transform
    ds = gdal.Warp('', src_ds, format='MEM',
                   srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +no_defs',
                   dstSRS='+proj=longlat +datum=WGS84 +geoidgrids=/vsimem/grid.tif +vunits=m +no_defs')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 80, 'Bad value'

    ds = gdal.Warp('', src_ds_longlat, format='MEM',
                   srcSRS='EPSG:4979',
                   dstSRS='+proj=longlat +datum=WGS84 +geoidgrids=/vsimem/grid.tif +vunits=m +no_defs')
    assert ds.GetGeoTransform() == (-180, 180, 0, 90, 0, -180)
    data = struct.unpack('B' * 2, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 80, 'Bad value'

    # Both transforms
    ds = gdal.Warp('', src_ds, format='MEM',
                   srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/grid.tif +vunits=m +no_defs',
                   dstSRS='+proj=longlat +datum=WGS84 +geoidgrids=/vsimem/grid2.tif +vunits=m +no_defs')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 115, 'Bad value'

    # Both transforms, but none of them have geoidgrids
    ds = gdal.Warp('', src_ds, format='MEM',
                   srcSRS='EPSG:32631+5730',
                   dstSRS='EPSG:4326+5621')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 100, 'Bad value'

    # Both transforms being a no-op
    ds = gdal.Warp('', src_ds, format='MEM',
                   srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/grid.tif +vunits=m +no_defs',
                   dstSRS='+proj=longlat +datum=WGS84 +geoidgrids=/vsimem/grid.tif +vunits=m +no_defs')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 100, 'Bad value'

    # Both transforms to anonymous VRT
    ds = gdal.Warp('', src_ds, format='VRT',
                   srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/grid.tif +vunits=m +no_defs',
                   dstSRS='+proj=longlat +datum=WGS84 +geoidgrids=/vsimem/grid2.tif +vunits=m +no_defs')
    src_ds = None  # drop the ref to src_ds before for fun
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 115, 'Bad value'

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    src_ds.GetRasterBand(1).Fill(100)

    # Both transforms to regular VRT
    gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/dem.tif', src_ds)
    gdal.Warp('/vsimem/tmp.vrt', '/vsimem/dem.tif', format='VRT',
              srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/grid.tif +vunits=m +no_defs',
              dstSRS='+proj=longlat +datum=WGS84 +geoidgrids=/vsimem/grid2.tif +vunits=m +no_defs')
    ds = gdal.Open('/vsimem/tmp.vrt')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    ds = None
    gdal.Unlink('/vsimem/dem.tif')
    gdal.Unlink('/vsimem/tmp.vrt')
    assert data == 115, 'Bad value'

    # Missing grid in forward path
    with gdaltest.error_handler():
        ds = gdal.Warp('', src_ds, format='MEM',
                       srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=i_dont_exist.tif +vunits=m +no_defs',
                       dstSRS='EPSG:4979')
    assert ds is None

    # Missing grid in forward path with PROJ_LIB
    old_proj_lib = os.environ.get('PROJ_LIB', None)
    os.environ['PROJ_LIB'] = '/i_dont/exist'
    with gdaltest.error_handler():
        ds = gdal.Warp('', src_ds, format='MEM',
                       srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=i_dont_exist.tif +vunits=m +no_defs',
                       dstSRS='EPSG:4979')
    if old_proj_lib:
        os.environ['PROJ_LIB'] = old_proj_lib
    else:
        del os.environ['PROJ_LIB']
    assert ds is None

    # Missing grid in forward path
    with gdaltest.error_handler():
        ds = gdal.Warp('', src_ds, format='MEM',
                       srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=~/i_dont_exist.tif +vunits=m +no_defs',
                       dstSRS='EPSG:4979')
    assert ds is None

    # Missing grid in forward path
    with gdaltest.error_handler():
        ds = gdal.Warp('', src_ds, format='MEM',
                       srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/i_dont/exist.tif +vunits=m +no_defs',
                       dstSRS='EPSG:4979')
    assert ds is None

    # Ungeoref grid in forward path
    with gdaltest.error_handler():
        ds = gdal.Warp('', src_ds, format='MEM',
                       srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/ungeoref_grid.tif +vunits=m +no_defs',
                       dstSRS='EPSG:4979')
    assert ds is None

    # Missing grid in inverse path
    with gdaltest.error_handler():
        ds = gdal.Warp('', src_ds, format='MEM',
                       srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +no_defs',
                       dstSRS='+proj=longlat +datum=WGS84 +geoidgrids=i_dont_exist.tif +vunits=m +no_defs')
    assert ds is None

    # Ungeoref grid in inverse path
    with gdaltest.error_handler():
        ds = gdal.Warp('', src_ds, format='MEM',
                       srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +no_defs',
                       dstSRS='+proj=longlat +datum=WGS84 +geoidgrids=/vsimem/ungeoref_grid.tif +vunits=m +no_defs')
    assert ds is None

    # Missing grid in forward path, but this is OK
    ds = gdal.Warp('', src_ds, format='MEM',
                   srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=@i_dont_exist.tif +vunits=m +no_defs',
                   dstSRS='EPSG:4979')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 100, 'Bad value'

    # Missing grid in inverse path but this is OK
    ds = gdal.Warp('', src_ds, format='MEM',
                   srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +no_defs',
                   dstSRS='+proj=longlat +datum=WGS84 +geoidgrids=@i_dont_exist.tif +vunits=m +no_defs')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 100, 'Bad value'

    # Forward transform with explicit m unit
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/grid.tif +vunits=m +no_defs')
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(100)
    src_ds.GetRasterBand(1).SetUnitType('m')

    ds = gdal.Warp('', src_ds, format='MEM',
                   dstSRS='EPSG:4979')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, 'Bad value'

    # Forward transform with explicit ft unit
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 1, gdal.GDT_Float32)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/grid.tif +vunits=m +no_defs')
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(100 / 0.3048)
    src_ds.GetRasterBand(1).SetUnitType('ft')

    ds = gdal.Warp('', src_ds, format='MEM',
                   dstSRS='EPSG:4979', outputType=gdal.GDT_Byte)
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, 'Bad value'

    # Forward transform with explicit unhandled unit
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/grid.tif +vunits=m +no_defs')
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(100)
    src_ds.GetRasterBand(1).SetUnitType('unhandled')

    with gdaltest.error_handler():
        ds = gdal.Warp('', src_ds, format='MEM',
                       dstSRS='EPSG:4979')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, 'Bad value'

    grid_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/empty_grid.tif', 1, 1)
    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")
    grid_ds.SetProjection(sr.ExportToWkt())
    grid_ds.SetGeoTransform([-180, 360, 0, 90, 0, -180])
    grid_ds.GetRasterBand(1).Fill(255)
    grid_ds.GetRasterBand(1).SetNoDataValue(255)
    grid_ds = None

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([500000, 1, 0, 4000000, 0, -1])
    src_ds.GetRasterBand(1).Fill(100)

    # Test missing shift values in area of interest
    ds = gdal.Warp('', src_ds, format='MEM',
                   srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/empty_grid.tif +vunits=m +no_defs',
                   dstSRS='EPSG:4979')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 100, 'Bad value'

    # Same, but make it an error
    with gdaltest.error_handler():
        ds = gdal.Warp('', src_ds, format='MEM',
                       srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/empty_grid.tif +vunits=m +no_defs',
                       dstSRS='EPSG:4979',
                       transformerOptions=['ERROR_ON_MISSING_VERT_SHIFT=YES'])
    if ds is not None:
        print(data)
        pytest.fail('Bad value')

    # Test several grids
    ds = gdal.Warp('', src_ds, format='MEM',
                   srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/grid.tif,/vsimem/empty_grid.tif +vunits=m +no_defs',
                   dstSRS='EPSG:4979')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, 'Bad value'

    # Same, but different order
    ds = gdal.Warp('', src_ds, format='MEM',
                   srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/empty_grid.tif,/vsimem/grid.tif +vunits=m +no_defs',
                   dstSRS='EPSG:4979')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, 'Bad value'

    # Test several grids, with some missing
    with gdaltest.error_handler():
        ds = gdal.Warp('', src_ds, format='MEM',
                       srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/grid.tif,i_dont_exist.tif +vunits=m +no_defs',
                       dstSRS='EPSG:4979')
    if ds is not None:
        print(data)
        pytest.fail('Bad value')

    # Test several grids, with some missing, but that's OK
    ds = gdal.Warp('', src_ds, format='MEM',
                   srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=/vsimem/grid.tif,@i_dont_exist.tif +vunits=m +no_defs',
                   dstSRS='EPSG:4979')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 120, 'Bad value'

    # Test several grids, with all missing, but that's OK
    ds = gdal.Warp('', src_ds, format='MEM',
                   srcSRS='+proj=utm +zone=31 +datum=WGS84 +units=m +geoidgrids=@i_dont_exist.tif,@i_dont_exist_either.tif +vunits=m +no_defs',
                   dstSRS='EPSG:4979')
    data = struct.unpack('B' * 1, ds.GetRasterBand(1).ReadRaster())[0]
    assert data == 100, 'Bad value'

    gdal.GetDriverByName('GTiff').Delete('/vsimem/grid.tif')
    gdal.GetDriverByName('GTiff').Delete('/vsimem/grid2.tif')
    gdal.GetDriverByName('GTiff').Delete('/vsimem/ungeoref_grid.tif')
    gdal.GetDriverByName('GTiff').Delete('/vsimem/empty_grid.tif')

###############################################################################
# Test error code path linked with failed warper initialization


def test_gdalwarp_lib_136():

    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format='MEM', warpOptions=['CUTLINE=invalid'])
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Warp('', '../gcore/data/utmsmall.tif', format='VRT', warpOptions=['CUTLINE=invalid'])
    assert ds is None

###############################################################################
# Test warping two input datasets with different SRS, with no explicit target SRS


def test_gdalwarp_lib_several_sources_with_different_srs_no_explicit_target_srs():
    src_ds = gdal.Open('../gcore/data/byte.tif')
    src_ds_32611_left = gdal.Translate('', src_ds, format='MEM',
                                       srcWin=[0, 0, 10, 20],
                                       outputSRS='EPSG:32611')
    src_ds_32611_right = gdal.Translate('', src_ds, format='MEM',
                                        srcWin=[10, 0, 10, 20],
                                        outputSRS='EPSG:32611')
    src_ds_4326_right = gdal.Warp('', src_ds_32611_right, format='MEM',
                                  dstSRS='EPSG:4326')
    out_ds = gdal.Warp('', [src_ds_4326_right, src_ds_32611_left], format='MEM')
    assert out_ds is not None
    assert out_ds.RasterXSize == 23
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 5048

###############################################################################
# Test fix for https://trac.osgeo.org/gdal/ticket/7243


def test_gdalwarp_lib_touching_dateline():

    src_ds = gdal.GetDriverByName('MEM').Create('', 100, 100)
    src_ds.SetGeoTransform([-2050000, 500, 0, 2100000, 0, -500])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3411)
    src_ds.SetProjection(sr.ExportToWkt())
    out_ds = gdal.Warp('', src_ds, dstSRS='EPSG:4326', format='MEM')
    assert out_ds.RasterXSize == 319

###############################################################################
# Test fix for https://trac.osgeo.org/gdal/ticket/7245


def test_gdalwarp_lib_override_default_output_nodata():

    drv = gdal.GetDriverByName('netCDF')
    if drv is None:
        pytest.skip()

    creationoptionlist = drv.GetMetadataItem('DMD_CREATIONOPTIONLIST')
    formats = ['NC']
    if '<Value>NC2</Value>' in creationoptionlist:
        formats += ['NC2']
    if '<Value>NC4</Value>' in creationoptionlist:
        formats += ['NC4', 'NC4C']

    for frmt in formats:
        gdal.Warp('tmp/out.nc', '../gcore/data/byte.tif', srcNodata=255,
                  format='netCDF', creationOptions=['FORMAT=' + frmt])
        ds = gdal.Open('tmp/out.nc')
        assert ds.GetRasterBand(1).GetNoDataValue() == 255, frmt
        assert ds.GetProjection() != '', frmt
        ds = None
        os.unlink('tmp/out.nc')

    
###############################################################################
# Test automatting setting (or not) of SKIP_NOSOURCE=YES


def test_gdalwarp_lib_auto_skip_nosource():

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    src_ds = gdal.GetDriverByName('MEM').Create('', 1000, 500)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src_ds.SetProjection(sr.ExportToWkt())

    tmpfilename = '/vsimem/test_gdalwarp_lib_auto_skip_nosource.tif'

    for options in ['-wo SKIP_NOSOURCE=NO',
                    '',
                    '-wo INIT_DEST=0',
                    '-wo INIT_DEST=NO_DATA',
                    '-dstnodata 0']:
        gdal.Unlink(tmpfilename)
        out_ds = gdal.Warp(tmpfilename, src_ds,
                           options='-te 1.5 48 3.5 49.5 -wm 100000 ' +
                           '-of GTiff ' + options)
        cs = out_ds.GetRasterBand(1).Checksum()
        assert cs == 41500, (options, cs)

    # Same with MEM
    for options in ['',
                    '-wo INIT_DEST=0',
                    '-dstnodata 0']:
        gdal.Unlink(tmpfilename)
        out_ds = gdal.Warp(tmpfilename, src_ds,
                           options='-te 1.5 48 3.5 49.5 -wm 100000 ' +
                           '-of MEM ' + options)
        cs = out_ds.GetRasterBand(1).Checksum()
        assert cs == 41500, (options, cs)

    # Use fill/nodata at 1
    for options in [  # '-wo SKIP_NOSOURCE=NO -dstnodata 1',
                    '-dstnodata 1',
                    '-dstnodata 1 -wo INIT_DEST=NO_DATA',
                    '-dstnodata 1 -wo INIT_DEST=1']:
        gdal.Unlink(tmpfilename)
        out_ds = gdal.Warp(tmpfilename, src_ds,
                           options='-te 1.5 48 3.5 49.5 -wm 100000 ' +
                           '-of GTiff ' + options)
        cs = out_ds.GetRasterBand(1).Checksum()
        assert cs == 51132, (options, cs)

    # Same with MEM
    for options in [  # '-wo SKIP_NOSOURCE=NO -dstnodata 1',
                    '-dstnodata 1',
                    '-dstnodata 1 -wo INIT_DEST=NO_DATA',
                    '-dstnodata 1 -wo INIT_DEST=1']:
        gdal.Unlink(tmpfilename)
        out_ds = gdal.Warp(tmpfilename, src_ds,
                           options='-te 1.5 48 3.5 49.5 -wm 100000 ' +
                           '-of MEM ' + options)
        cs = out_ds.GetRasterBand(1).Checksum()
        assert cs == 51132, (options, cs)

    # Rather dummy: use a INIT_DEST different of the target dstnodata
    for options in [  # '-wo SKIP_NOSOURCE=NO -dstnodata 1 -wo INIT_DEST=0',
                    '-dstnodata 127 -wo INIT_DEST=0']:
        gdal.Unlink(tmpfilename)
        out_ds = gdal.Warp(tmpfilename, src_ds,
                           options='-te 1.5 48 3.5 49.5 -wm 100000 ' +
                           '-of GTiff ' + options)
        cs = out_ds.GetRasterBand(1).Checksum()
        assert cs == 41500, (options, cs)

    # Test with 2 input datasets
    src_ds1 = gdal.GetDriverByName('MEM').Create('', 500, 500)
    src_ds1.GetRasterBand(1).Fill(255)
    src_ds1.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src_ds1.SetProjection(sr.ExportToWkt())

    src_ds2 = gdal.GetDriverByName('MEM').Create('', 500, 500)
    src_ds2.GetRasterBand(1).Fill(255)
    src_ds2.SetGeoTransform([2.5, 0.001, 0, 49, 0, -0.001])
    src_ds2.SetProjection(sr.ExportToWkt())

    for options in ['']:
        gdal.Unlink(tmpfilename)
        out_ds = gdal.Warp(tmpfilename, [src_ds1, src_ds2],
                           options='-te 1.5 48 3.5 49.5 -wm 100000 ' +
                           '-of GTiff ' + options)
        cs = out_ds.GetRasterBand(1).Checksum()
        assert cs == 41500, (options, cs)

    gdal.Unlink(tmpfilename)

###############################################################################
# Test warping a full EPSG:4326 extent to +proj=ortho
# (https://github.com/OSGeo/gdal/issues/862)


def test_gdalwarp_lib_to_ortho():

    out_ds = gdal.Warp("/tmp/out.tif", "../gdrivers/data/small_world.tif",
                       options='-of MEM -t_srs "+proj=ortho +datum=WGS84" -ts 1024 1024')

    line = out_ds.GetRasterBand(1).ReadRaster(0, 0, out_ds.RasterXSize, 1)
    line = struct.unpack('B' * out_ds.RasterXSize, line)
    # Fail if the first line is completely black
    assert line.count(0) != out_ds.RasterXSize, 'first line is completely black'

    line = out_ds.GetRasterBand(1).ReadRaster(0, out_ds.RasterYSize - 1,
                                              out_ds.RasterXSize, 1)
    line = struct.unpack('B' * out_ds.RasterXSize, line)
    # Fail if the last line is completely black
    assert line.count(0) != out_ds.RasterXSize, 'last line is completely black'


###############################################################################
def test_gdalwarp_lib_insufficient_dst_band_count():

    src_ds = gdal.Translate('', '../gcore/data/byte.tif', options='-of MEM -b 1 -b 1')
    dst_ds = gdal.Translate('', '../gcore/data/byte.tif', options='-of MEM')
    with gdaltest.error_handler():
        assert gdal.Warp(dst_ds, src_ds) == 0


###############################################################################
# Test -ct

def test_gdalwarp_lib_ct():

    dstDS = gdal.Warp('', '../gcore/data/byte.tif', options = '-r cubic -f MEM -t_srs EPSG:4326 -ct "proj=pipeline step inv proj=utm zone=11 ellps=clrk66 step proj=unitconvert xy_in=rad xy_out=deg step proj=axisswap order=2,1"')

    assert dstDS.GetRasterBand(1).Checksum() == 4705, 'Bad checksum'


def test_gdalwarp_lib_ct_wkt():

    wkt = """CONCATENATEDOPERATION["Inverse of UTM zone 11N + Null geographic offset from NAD27 to WGS 84",
    SOURCECRS[
        PROJCRS["NAD27 / UTM zone 11N",
            BASEGEOGCRS["NAD27",
                DATUM["North American Datum 1927",
                    ELLIPSOID["Clarke 1866",6378206.4,294.978698213898,
                        LENGTHUNIT["metre",1]]],
                PRIMEM["Greenwich",0,
                    ANGLEUNIT["degree",0.0174532925199433]]],
            CONVERSION["UTM zone 11N",
                METHOD["Transverse Mercator",
                    ID["EPSG",9807]],
                PARAMETER["Latitude of natural origin",0,
                    ANGLEUNIT["degree",0.0174532925199433],
                    ID["EPSG",8801]],
                PARAMETER["Longitude of natural origin",-117,
                    ANGLEUNIT["degree",0.0174532925199433],
                    ID["EPSG",8802]],
                PARAMETER["Scale factor at natural origin",0.9996,
                    SCALEUNIT["unity",1],
                    ID["EPSG",8805]],
                PARAMETER["False easting",500000,
                    LENGTHUNIT["metre",1],
                    ID["EPSG",8806]],
                PARAMETER["False northing",0,
                    LENGTHUNIT["metre",1],
                    ID["EPSG",8807]]],
            CS[Cartesian,2],
                AXIS["(E)",east,
                    ORDER[1],
                    LENGTHUNIT["metre",1]],
                AXIS["(N)",north,
                    ORDER[2],
                    LENGTHUNIT["metre",1]],
            USAGE[
                SCOPE["unknown"],
                AREA["North America - 120°W to 114°W and NAD27 by country - onshore"],
                BBOX[26.93,-120,78.13,-114]],
            ID["EPSG",26711]]],
    TARGETCRS[
        GEOGCRS["WGS 84",
            DATUM["World Geodetic System 1984",
                ELLIPSOID["WGS 84",6378137,298.257223563,
                    LENGTHUNIT["metre",1]]],
            PRIMEM["Greenwich",0,
                ANGLEUNIT["degree",0.0174532925199433]],
            CS[ellipsoidal,2],
                AXIS["geodetic latitude (Lat)",north,
                    ORDER[1],
                    ANGLEUNIT["degree",0.0174532925199433]],
                AXIS["geodetic longitude (Lon)",east,
                    ORDER[2],
                    ANGLEUNIT["degree",0.0174532925199433]],
            USAGE[
                SCOPE["unknown"],
                AREA["World"],
                BBOX[-90,-180,90,180]],
            ID["EPSG",4326]]],
    STEP[
        CONVERSION["Inverse of UTM zone 11N",
            METHOD["Inverse of Transverse Mercator",
                ID["INVERSE(EPSG)",9807]],
            PARAMETER["Latitude of natural origin",0,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8801]],
            PARAMETER["Longitude of natural origin",-117,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8802]],
            PARAMETER["Scale factor at natural origin",0.9996,
                SCALEUNIT["unity",1],
                ID["EPSG",8805]],
            PARAMETER["False easting",500000,
                LENGTHUNIT["metre",1],
                ID["EPSG",8806]],
            PARAMETER["False northing",0,
                LENGTHUNIT["metre",1],
                ID["EPSG",8807]],
            ID["INVERSE(EPSG)",16011]]],
    STEP[
        COORDINATEOPERATION["Null geographic offset from NAD27 to WGS 84",
            SOURCECRS[
                GEOGCRS["NAD27",
                    DATUM["North American Datum 1927",
                        ELLIPSOID["Clarke 1866",6378206.4,294.978698213898,
                            LENGTHUNIT["metre",1]]],
                    PRIMEM["Greenwich",0,
                        ANGLEUNIT["degree",0.0174532925199433]],
                    CS[ellipsoidal,2],
                        AXIS["geodetic latitude (Lat)",north,
                            ORDER[1],
                            ANGLEUNIT["degree",0.0174532925199433]],
                        AXIS["geodetic longitude (Lon)",east,
                            ORDER[2],
                            ANGLEUNIT["degree",0.0174532925199433]],
                    USAGE[
                        SCOPE["unknown"],
                        AREA["North America - NAD27"],
                        BBOX[7.15,167.65,83.17,-47.74]],
                    ID["EPSG",4267]]],
            TARGETCRS[
                GEOGCRS["WGS 84",
                    DATUM["World Geodetic System 1984",
                        ELLIPSOID["WGS 84",6378137,298.257223563,
                            LENGTHUNIT["metre",1]]],
                    PRIMEM["Greenwich",0,
                        ANGLEUNIT["degree",0.0174532925199433]],
                    CS[ellipsoidal,2],
                        AXIS["geodetic latitude (Lat)",north,
                            ORDER[1],
                            ANGLEUNIT["degree",0.0174532925199433]],
                        AXIS["geodetic longitude (Lon)",east,
                            ORDER[2],
                            ANGLEUNIT["degree",0.0174532925199433]],
                    USAGE[
                        SCOPE["unknown"],
                        AREA["World"],
                        BBOX[-90,-180,90,180]],
                    ID["EPSG",4326]]],
            METHOD["Geographic2D offsets",
                ID["EPSG",9619]],
            PARAMETER["Latitude offset",0,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8601]],
            PARAMETER["Longitude offset",0,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8602]],
            USAGE[
                SCOPE["unknown"],
                AREA["World"],
                BBOX[-90,-180,90,180]]]],
    USAGE[
        SCOPE["unknown"],
        AREA["World"],
        BBOX[-90,-180,90,180]]]"""

    dstDS = gdal.Warp('', '../gcore/data/byte.tif',
                      resampleAlg=gdal.GRIORA_Cubic, format='MEM',
                      dstSRS='EPSG:4326',
                      coordinateOperation=wkt)

    assert dstDS.GetRasterBand(1).Checksum() == 4705, 'Bad checksum'


###############################################################################
# Test warping from a RPC dataset to a new dataset larger than needed

def test_gdalwarp_lib_restrict_output_dataset_warp_rpc_new():

    dstDS = gdal.Warp('', 'data/unstable_rpc_with_dem_source.tif',
              options = '-f MEM -et 0 -to RPC_DEM=data/unstable_rpc_with_dem_elevation.tif -to RPC_MAX_ITERATIONS=40 -to RPC_DEM_MISSING_VALUE=0 -t_srs EPSG:3857 -te 12693400.445 2547311.740 12700666.740 2553269.051 -ts 380 311')
    cs = dstDS.GetRasterBand(1).Checksum()
    assert cs == 53230

    with gdaltest.config_option('RESTRICT_OUTPUT_DATASET_UPDATE', 'NO'):
        dstDS = gdal.Warp('', 'data/unstable_rpc_with_dem_source.tif',
                options = '-f MEM -et 0 -to RPC_DEM=data/unstable_rpc_with_dem_elevation.tif -to RPC_MAX_ITERATIONS=40 -to RPC_DEM_MISSING_VALUE=0 -t_srs EPSG:3857 -te 12693400.445 2547311.740 12700666.740 2553269.051 -ts 380 311')
    cs = dstDS.GetRasterBand(1).Checksum()
    assert cs != 53230


###############################################################################
# Test warping from a RPC dataset to an existing dataset

def test_gdalwarp_lib_restrict_output_dataset_warp_rpc_existing():

    dstDS = gdal.Translate('', 'data/unstable_rpc_with_dem_blank_output.tif',
                           format = 'MEM')
    gdal.Warp(dstDS, 'data/unstable_rpc_with_dem_source.tif',
              options = '-et 0 -to RPC_DEM=data/unstable_rpc_with_dem_elevation.tif -to RPC_MAX_ITERATIONS=40 -to RPC_DEM_MISSING_VALUE=0')
    cs = dstDS.GetRasterBand(1).Checksum()
    assert cs == 53230


###############################################################################
# Test warping from a RPC dataset to an existing dataset, with using RPC_FOOTPRINT

def test_gdalwarp_lib_restrict_output_dataset_warp_rpc_existing_RPC_FOOTPRINT():

    if not ogrtest.have_geos():
        pytest.skip()

    with gdaltest.config_option('RESTRICT_OUTPUT_DATASET_UPDATE', 'NO'):
        dstDS = gdal.Translate('', 'data/unstable_rpc_with_dem_blank_output.tif',
                            format = 'MEM')
        gdal.Warp(dstDS, 'data/unstable_rpc_with_dem_source.tif',
                options = '-et 0 -to RPC_DEM=data/unstable_rpc_with_dem_elevation.tif -to RPC_MAX_ITERATIONS=40 -to RPC_DEM_MISSING_VALUE=0 -to "RPC_FOOTPRINT=POLYGON ((114.070906445526 22.329620213341,114.085953272341 22.3088955493586,114.075520805749 22.3027084861851,114.060942102434 22.3236815197571,114.060942102434 22.3236815197571,114.060942102434 22.3236815197571,114.060942102434 22.3236815197571,114.070906445526 22.329620213341))"')
        cs = dstDS.GetRasterBand(1).Checksum()
        assert cs == 53230


###############################################################################
# Test warping from EPSG:4326 to EPSG:3857

def test_gdalwarp_lib_bug_4326_to_3857():

    ds = gdal.Warp('', 'data/test_bug_4326_to_3857.tif',
              options = '-f MEM -t_srs EPSG:3857 -ts 20 20')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4672


###############################################################################
# Test warping of single source to COG


def test_gdalwarp_lib_to_cog():

    tmpfilename = '/vsimem/cog.tif'
    ds = gdal.Warp(tmpfilename, '../gcore/data/byte.tif',
              options = '-f COG -t_srs EPSG:3857 -ts 20 20')
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None
    gdal.Unlink(tmpfilename)


###############################################################################
# Test warping of single source to COG with reprojection options


def test_gdalwarp_lib_to_cog_reprojection_options():

    tmpfilename = '/vsimem/cog.tif'
    ds = gdal.Warp(tmpfilename, '../gcore/data/byte.tif',
              options = '-f COG -co TILING_SCHEME=GoogleMapsCompatible')
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() in (4187, 4300) # 4300 on Mac
    assert ds.GetRasterBand(2).Checksum() == 4415
    ds = None
    gdal.Unlink(tmpfilename)


###############################################################################
# Test warping of multiple source, compatible of BuildVRT mosaicing, to COG


def test_gdalwarp_lib_multiple_source_compatible_buildvrt_to_cog():

    tmpfilename = '/vsimem/cog.tif'
    left_ds = gdal.Translate('/vsimem/left.tif', '../gcore/data/byte.tif',
                             options='-srcwin 0 0 10 20')
    right_ds = gdal.Translate('/vsimem/right.tif', '../gcore/data/byte.tif',
                              options='-srcwin 10 0 10 20')
    ds = gdal.Warp(tmpfilename, [left_ds, right_ds], options = '-f COG')
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None
    gdal.Unlink(tmpfilename)
    gdal.Unlink('/vsimem/left.tif')
    gdal.Unlink('/vsimem/right.tif')


###############################################################################
# Test warping of multiple source, compatible of BuildVRT mosaicing, to COG,
# with reprojection options


def test_gdalwarp_lib_multiple_source_compatible_buildvrt_to_cog_reprojection_options():

    tmpfilename = '/vsimem/cog.tif'
    left_ds = gdal.Translate('/vsimem/left.tif', '../gcore/data/byte.tif',
                             options='-srcwin 0 0 10 20')
    right_ds = gdal.Translate('/vsimem/right.tif', '../gcore/data/byte.tif',
                              options='-srcwin 10 0 10 20')
    ds = gdal.Warp(tmpfilename, [left_ds, right_ds],
                   options = '-f COG -co TILING_SCHEME=GoogleMapsCompatible')
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() in (4187, 4300) # 4300 on Mac
    assert ds.GetRasterBand(2).Checksum() == 4415
    ds = None
    gdal.Unlink(tmpfilename)
    gdal.Unlink('/vsimem/left.tif')
    gdal.Unlink('/vsimem/right.tif')


###############################################################################
# Test warping of multiple source, incompatible of BuildVRT mosaicing, to COG


def test_gdalwarp_lib_multiple_source_incompatible_buildvrt_to_cog():

    tmpfilename = '/vsimem/cog.tif'
    left_ds = gdal.Translate('/vsimem/left.tif', '../gcore/data/byte.tif',
                             options='-srcwin 0 0 15 20 -b 1 -b 1 -colorinterp_2 alpha -scale_2 0 255 255 255')
    right_ds = gdal.Translate('/vsimem/right.tif', '../gcore/data/byte.tif',
                              options='-srcwin 5 0 15 20 -b 1 -b 1 -colorinterp_2 alpha -scale_2 0 255 255 255')
    ds = gdal.Warp(tmpfilename, [left_ds, right_ds], options = '-f COG')
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(2).Checksum() == 4873
    ds = None
    gdal.Unlink(tmpfilename)
    gdal.Unlink('/vsimem/left.tif')
    gdal.Unlink('/vsimem/right.tif')


###############################################################################
# Test warping of multiple source, incompatible of BuildVRT mosaicing, to COG,
# with reprojection options


def test_gdalwarp_lib_multiple_source_incompatible_buildvrt_to_cog_reprojection_options():

    tmpfilename = '/vsimem/cog.tif'
    left_ds = gdal.Translate('/vsimem/left.tif', '../gcore/data/byte.tif',
                             options='-srcwin 0 0 15 20 -b 1 -b 1 -colorinterp_2 alpha -scale_2 0 255 255 255')
    right_ds = gdal.Translate('/vsimem/right.tif', '../gcore/data/byte.tif',
                              options='-srcwin 5 0 15 20 -b 1 -b 1 -colorinterp_2 alpha -scale_2 0 255 255 255')
    ds = gdal.Warp(tmpfilename, [left_ds, right_ds],
                   options = '-f COG -co TILING_SCHEME=GoogleMapsCompatible')
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).Checksum() in (4207, 4315) # 4300 on Mac
    assert ds.GetRasterBand(2).Checksum() == 4415
    ds = None
    gdal.Unlink(tmpfilename)
    gdal.Unlink('/vsimem/left.tif')
    gdal.Unlink('/vsimem/right.tif')

###############################################################################

def test_gdalwarp_lib_no_crs():

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([0, 10, 0, 0, 0, -10])
    out_ds = gdal.Warp('', src_ds, options = '-of MEM -ct "+proj=unitconvert +xy_in=1 +xy_out=2"')
    assert out_ds.GetGeoTransform() == (0.0, 5.0, 0.0, 0.0, 0.0, -5.0)

###############################################################################
# Cleanup


def test_gdalwarp_lib_cleanup():

    # We don't clean up when run in debug mode.
    if gdal.GetConfigOption('CPL_DEBUG', 'OFF') == 'ON':
        return

    for i in range(2):
        try:
            os.remove('tmp/testgdalwarp' + str(i + 1) + '.tif')
        except OSError:
            pass
    try:
        os.remove('tmp/testgdalwarp_gcp.tif')
    except OSError:
        pass

    


