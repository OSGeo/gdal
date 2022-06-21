#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Geolocation warper.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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

import array
import random
from osgeo import gdal, osr

import gdaltest
import pytest

###############################################################################
# Verify warped result.


def test_geoloc_1():

    tst = gdaltest.GDALTest('VRT', 'warpsst.vrt', 1, 63034)
    return tst.testOpen(check_filelist=False)


###############################################################################
# Test that we take into account the min/max of the geoloc arrays


@pytest.mark.parametrize("use_temp_datasets", ['YES', 'NO'])
def test_geoloc_bounds(use_temp_datasets):

    lon_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/lon.tif', 360, 1, 1, gdal.GDT_Float32)
    lon_ds.WriteRaster(0, 0, 360, 1, array.array('f', [91 + 0.5 * x for x in range(178)] + [-179.9 + 0.5 * x for x in range(182)]))
    lon_ds = None

    lat_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/lat.tif', 80, 1, 1, gdal.GDT_Float32)
    lat_ds.WriteRaster(0, 0, 80, 1, array.array('f', [60.4 + 0.5 * x for x in range(60)] + [89 - 0.5 * x for x in range(20)]))
    lat_ds = None

    ds = gdal.GetDriverByName('MEM').Create('', 360, 80)
    md = {
        'LINE_OFFSET': '0',
        'LINE_STEP': '1',
        'PIXEL_OFFSET': '0',
        'PIXEL_STEP': '1',
        'X_DATASET': '/vsimem/lon.tif',
        'X_BAND' : '1',
        'Y_DATASET': '/vsimem/lat.tif',
        'Y_BAND' : '1'
    }
    ds.SetMetadata(md, 'GEOLOCATION')
    with gdaltest.config_option('GDAL_GEOLOC_USE_TEMP_DATASETS', use_temp_datasets):
        warped_ds = gdal.Warp('', ds, format='MEM')
        assert warped_ds

    gdal.Unlink('/vsimem/lon.tif')
    gdal.Unlink('/vsimem/lat.tif')

    gt = warped_ds.GetGeoTransform()
    assert gt[0] == pytest.approx(-179.9)
    assert gt[3] == pytest.approx(60.4 + 0.5 * 59)

###############################################################################
# Test that the line filling logic works


@pytest.mark.parametrize("use_temp_datasets", ['YES', 'NO'])
def test_geoloc_fill_line(use_temp_datasets):


    ds = gdal.GetDriverByName('MEM').Create('', 200, 372)
    md = {
        'LINE_OFFSET': '0',
        'LINE_STEP': '1',
        'PIXEL_OFFSET': '0',
        'PIXEL_STEP': '1',
        'X_DATASET': '../alg/data/geoloc/longitude_including_pole.tif',
        'X_BAND' : '1',
        'Y_DATASET': '../alg/data/geoloc/latitude_including_pole.tif',
        'Y_BAND' : '1',
        'SRS': 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]'
    }
    ds.SetMetadata(md, 'GEOLOCATION')
    ds.GetRasterBand(1).Fill(1)
    with gdaltest.config_option('GDAL_GEOLOC_USE_TEMP_DATASETS', use_temp_datasets):
        warped_ds = gdal.Warp('', ds, format='MEM')
        assert warped_ds
        assert warped_ds.GetRasterBand(1).Checksum() in (22338,
                                                         22335) # 22335 with Intel(R) oneAPI DPC++/C++ Compiler 2022.1.0



###############################################################################
# Test warping from rectified to referenced-by-geoloc


def test_geoloc_warp_to_geoloc():

    lon_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/lon.tif', 10, 1, 1, gdal.GDT_Float32)
    lon_ds.WriteRaster(0, 0, 10, 1, array.array('f', [-79.5 + 1 * x for x in range(10)]))
    lon_ds = None

    lat_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/lat.tif', 10, 1, 1, gdal.GDT_Float32)
    lat_ds.WriteRaster(0, 0, 10, 1, array.array('f', [49.5 - 1 * x for x in range(10)]))
    lat_ds = None

    ds = gdal.GetDriverByName('MEM').Create('', 10, 10)
    md = {
        'LINE_OFFSET': '0',
        'LINE_STEP': '1',
        'PIXEL_OFFSET': '0',
        'PIXEL_STEP': '1',
        'X_DATASET': '/vsimem/lon.tif',
        'X_BAND' : '1',
        'Y_DATASET': '/vsimem/lat.tif',
        'Y_BAND' : '1',
        'SRS': 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]',
        'GEOREFERENCING_CONVENTION': 'PIXEL_CENTER'
    }
    ds.SetMetadata(md, 'GEOLOCATION')

    input_ds = gdal.GetDriverByName('MEM').Create('', 10, 10)
    input_ds.SetGeoTransform([-80, 1, 0, 50, 0, -1])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    input_ds.SetSpatialRef(srs)
    input_ds.GetRasterBand(1).Fill(255)

    transformer = gdal.Transformer(input_ds, ds, [])

    success, pnt = transformer.TransformPoint(0, 0.5, 0.5)
    assert success
    assert pnt == pytest.approx((0.5, 0.5, 0))

    ds.GetRasterBand(1).Fill(0)
    assert gdal.Warp(ds, input_ds)

    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (255, 255), ds.ReadAsArray()

    # Try with projected coordinates
    input_ds = gdal.GetDriverByName('MEM').Create('', 10, 10)
    input_ds.SetGeoTransform([-8905559.26346189, (-7792364.35552915 - -8905559.26346189)/10, 0, 6446275.84101716, 0, -(6446275.84101716-4865942.27950318)/10])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    input_ds.SetSpatialRef(srs)
    input_ds.GetRasterBand(1).Fill(255)

    transformer = gdal.Transformer(input_ds, ds, [])

    success, pnt = transformer.TransformPoint(0, 0.5, 0.5)
    assert success
    assert pnt == pytest.approx((0.5, 0.5, 0), abs=0.1)

    ds.GetRasterBand(1).Fill(0)
    assert gdal.Warp(ds, input_ds)

    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (255, 255), ds.ReadAsArray()


    gdal.Unlink('/vsimem/lon.tif')
    gdal.Unlink('/vsimem/lat.tif')



###############################################################################
# Test error cases


def test_geoloc_error_cases():

    lon_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/lon.tif', 10, 1, 1, gdal.GDT_Float32)
    lon_ds.WriteRaster(0, 0, 10, 1, array.array('f', [-179.5 + 1 * x for x in range(10)]))
    lon_ds = None

    lat_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/lat.tif', 10, 1, 1, gdal.GDT_Float32)
    lat_ds.WriteRaster(0, 0, 10, 1, array.array('f', [89.5 - 1 * x for x in range(10)]))
    lat_ds = None

    ds = gdal.GetDriverByName('MEM').Create('', 10, 10)
    ds.SetMetadata({'invalid': 'content'}, 'GEOLOCATION')

    with gdaltest.error_handler():
        transformer = gdal.Transformer(ds, None, [])
    assert transformer is None

    with gdaltest.error_handler():
        transformer = gdal.Transformer(None, ds, [])
    assert transformer is None


###############################################################################
# Test geolocation array where the transformation is just an affine transformation


@pytest.mark.parametrize("step", [1, 2])
@pytest.mark.parametrize("convention", ['TOP_LEFT_CORNER', 'PIXEL_CENTER'])
@pytest.mark.parametrize("inverse_method", ['BACKMAP', 'QUADTREE'])
def test_geoloc_affine_transformation(step, convention, inverse_method):

    shift = 0.5 if convention == 'PIXEL_CENTER' else 0
    lon_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/lon.tif', 20 // step, 1, 1, gdal.GDT_Float32)
    vals = array.array('f', [-80 + step * (x + shift) for x in range(20 // step)])
    lon_ds.WriteRaster(0, 0, 20 // step, 1, vals)
    lon_ds = None
    lat_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/lat.tif', 20 // step, 1, 1, gdal.GDT_Float32)
    vals = array.array('f', [50 - step * (x + shift) for x in range(20 // step)])
    lat_ds.WriteRaster(0, 0, 20 // step, 1, vals)
    lat_ds = None
    ds = gdal.GetDriverByName('MEM').Create('', 20, 20)
    md = {
        'LINE_OFFSET': '0',
        'LINE_STEP': str(step),
        'PIXEL_OFFSET': '0',
        'PIXEL_STEP': str(step),
        'X_DATASET': '/vsimem/lon.tif',
        'X_BAND' : '1',
        'Y_DATASET': '/vsimem/lat.tif',
        'Y_BAND' : '1',
        'SRS': 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]',
        'GEOREFERENCING_CONVENTION' : convention
    }
    ds.SetMetadata(md, 'GEOLOCATION')

    with gdaltest.config_option('GDAL_GEOLOC_INVERSE_METHOD', inverse_method):
        tr = gdal.Transformer(ds, None, [])

        def check_point(x,y,X,Y):
            success, pnt = tr.TransformPoint(False, x, y)
            assert success
            assert pnt == (X, Y, 0)

            success, pnt = tr.TransformPoint(True, pnt[0], pnt[1])
            assert success
            assert pnt == pytest.approx((x, y, 0))

        check_point(10, 10, -70.0, 40.0)
        check_point(1.23, 2.34, -78.77, 47.66)
        check_point(0,   0, -80.0, 50.0)
        check_point(20,  0, -60.0, 50.0)
        check_point(0,  20, -80.0, 30.0)
        check_point(20, 20, -60.0, 30.0)

    ds = None

    gdal.Unlink('/vsimem/lon.tif')
    gdal.Unlink('/vsimem/lat.tif')


###############################################################################
# Test geolocation array where the transformation is just an affine transformation


@pytest.mark.parametrize("step", [1, 2])
@pytest.mark.parametrize("convention", ['TOP_LEFT_CORNER', 'PIXEL_CENTER'])
def test_geoloc_affine_transformation_with_noise(step, convention):

    r = random.Random(0)

    shift = 0.5 if convention == 'PIXEL_CENTER' else 0
    lon_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/lon.tif', 20 // step, 20 // step, 1, gdal.GDT_Float32)
    for y in range(lon_ds.RasterYSize):
        vals = array.array('f', [-80 + step * (x + shift) + r.uniform(-0.25,0.25) for x in range(lon_ds.RasterXSize)])
        lon_ds.WriteRaster(0, y, lon_ds.RasterXSize, 1, vals)
    lon_ds = None
    lat_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/lat.tif', 20 // step, 20 // step, 1, gdal.GDT_Float32)
    for x in range(lat_ds.RasterXSize):
        vals = array.array('f', [50 - step * (y + shift) + r.uniform(-0.25,0.25) for y in range(lat_ds.RasterYSize)])
        lat_ds.WriteRaster(x, 0, 1, lat_ds.RasterYSize, vals)
    lat_ds = None
    ds = gdal.GetDriverByName('MEM').Create('', 20, 20)
    md = {
        'LINE_OFFSET': '0',
        'LINE_STEP': str(step),
        'PIXEL_OFFSET': '0',
        'PIXEL_STEP': str(step),
        'X_DATASET': '/vsimem/lon.tif',
        'X_BAND' : '1',
        'Y_DATASET': '/vsimem/lat.tif',
        'Y_BAND' : '1',
        'SRS': 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]',
        'GEOREFERENCING_CONVENTION' : convention
    }
    ds.SetMetadata(md, 'GEOLOCATION')
    tr = gdal.Transformer(ds, None, [])

    def check_point(x,y):
        success, pnt = tr.TransformPoint(False, x, y)
        assert success
        success, pnt = tr.TransformPoint(True, pnt[0], pnt[1])
        assert success
        assert pnt == pytest.approx((x, y, 0))

    check_point(10, 10)
    check_point(1.23, 2.34)
    check_point(0, 0)
    check_point(20, 0)
    check_point(0, 20)
    check_point(20, 20)

    ds = None

    gdal.Unlink('/vsimem/lon.tif')
    gdal.Unlink('/vsimem/lat.tif')
