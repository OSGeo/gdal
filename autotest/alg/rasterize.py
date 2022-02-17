#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RasterizeLayer() and related calls.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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


import ogrtest

from osgeo import gdal, ogr, osr
import pytest

###############################################################################
# Simple polygon rasterization.


def test_rasterize_1():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    # Create a memory raster to rasterize into.

    target_ds = gdal.GetDriverByName('MEM').Create('', 100, 100, 3,
                                                   gdal.GDT_Byte)
    target_ds.SetGeoTransform((1000, 1, 0, 1100, 0, -1))
    target_ds.SetProjection(sr_wkt)

    # Create a memory layer to rasterize from.

    rast_ogr_ds = \
        ogr.GetDriverByName('Memory').CreateDataSource('wrk')
    rast_mem_lyr = rast_ogr_ds.CreateLayer('poly', srs=sr)

    # Add a polygon.

    wkt_geom = 'POLYGON((1020 1030,1020 1045,1050 1045,1050 1030,1020 1030))'

    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))

    rast_mem_lyr.CreateFeature(feat)

    # Add a linestring.

    wkt_geom = 'LINESTRING(1000 1000, 1100 1050)'

    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))

    rast_mem_lyr.CreateFeature(feat)

    # Run the algorithm.

    err = gdal.RasterizeLayer(target_ds, [3, 2, 1], rast_mem_lyr,
                              burn_values=[256, 220, -1])

    assert err == 0, 'got non-zero result code from RasterizeLayer'

    # Check results.

    expected = 6452
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdal.GetDriverByName('GTiff').CreateCopy('tmp/rasterize_1.tif', target_ds)
        pytest.fail('Did not get expected image checksum')

    _, maxval = target_ds.GetRasterBand(3).ComputeRasterMinMax()
    assert maxval == 255

    minval, _ = target_ds.GetRasterBand(1).ComputeRasterMinMax()
    assert minval == 0



###############################################################################
# Test rasterization with ALL_TOUCHED.


def test_rasterize_2():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'

    # Create a memory raster to rasterize into.

    target_ds = gdal.GetDriverByName('MEM').Create('', 12, 12, 3,
                                                   gdal.GDT_Byte)
    target_ds.SetGeoTransform((0, 1, 0, 12, 0, -1))
    target_ds.SetProjection(sr_wkt)

    # Create a memory layer to rasterize from.

    cutline_ds = ogr.Open('data/cutline.csv')

    # Run the algorithm.

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    err = gdal.RasterizeLayer(target_ds, [3, 2, 1], cutline_ds.GetLayer(0),
                              burn_values=[200, 220, 240],
                              options=["ALL_TOUCHED=TRUE"])
    gdal.PopErrorHandler()

    assert err == 0, 'got non-zero result code from RasterizeLayer'

    # Check results.

    expected = 121
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdal.GetDriverByName('GTiff').CreateCopy('tmp/rasterize_2.tif', target_ds)
        pytest.fail('Did not get expected image checksum')


###############################################################################
# Rasterization with BURN_VALUE_FROM.


def test_rasterize_3():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    # Create a memory raster to rasterize into.

    target_ds = gdal.GetDriverByName('MEM').Create('', 100, 100, 3,
                                                   gdal.GDT_Byte)
    target_ds.SetGeoTransform((1000, 1, 0, 1100, 0, -1))
    target_ds.SetProjection(sr_wkt)

    # Create a memory layer to rasterize from.

    rast_ogr_ds = \
        ogr.GetDriverByName('Memory').CreateDataSource('wrk')
    rast_mem_lyr = rast_ogr_ds.CreateLayer('poly', srs=sr)

    # Add polygons and linestrings.
    wkt_geom = ['POLYGON((1020 1030 40,1020 1045 30,1050 1045 20,1050 1030 35,1020 1030 40))',
                'POLYGON((1010 1046 85,1015 1055 35,1055 1060 26,1054 1048 35,1010 1046 85))',
                'POLYGON((1020 1076 190,1025 1085 35,1065 1090 26,1064 1078 35,1020 1076 190),(1023 1079 5,1061 1081 35,1062 1087 26,1028 1082 35,1023 1079 85))',
                'LINESTRING(1005 1000 10, 1100 1050 120)',
                'LINESTRING(1000 1000 150, 1095 1050 -5, 1080 1080 200)']
    for g in wkt_geom:
        feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
        feat.SetGeometryDirectly(ogr.Geometry(wkt=g))
        rast_mem_lyr.CreateFeature(feat)

    # Run the algorithm.

    err = gdal.RasterizeLayer(target_ds, [3, 2, 1], rast_mem_lyr,
                              burn_values=[10, 10, 55], options=["BURN_VALUE_FROM=Z"])

    assert err == 0, 'got non-zero result code from RasterizeLayer'

    # Check results.

    expected = 15037
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdal.GetDriverByName('GTiff').CreateCopy('tmp/rasterize_3.tif', target_ds)
        pytest.fail('Did not get expected image checksum')


###############################################################################
# Rasterization with ATTRIBUTE.


def test_rasterize_4():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    # Create a memory raster to rasterize into.
    target_ds = gdal.GetDriverByName('MEM').Create('', 100, 100, 3,
                                                   gdal.GDT_Byte)
    target_ds.SetGeoTransform((1000, 1, 0, 1100, 0, -1))
    target_ds.SetProjection(sr_wkt)

    # Create a memory layer to rasterize from.
    rast_ogr_ds = ogr.GetDriverByName('Memory').CreateDataSource('wrk')
    rast_mem_lyr = rast_ogr_ds.CreateLayer('poly', srs=sr)
    # Setup Schema
    ogrtest.quick_create_layer_def(rast_mem_lyr,
                                   [('CELSIUS', ogr.OFTReal)])

    # Add polygons and linestrings and a field named CELSIUS.
    wkt_geom = ['POLYGON((1020 1030 40,1020 1045 30,1050 1045 20,1050 1030 35,1020 1030 40))',
                'POLYGON((1010 1046 85,1015 1055 35,1055 1060 26,1054 1048 35,1010 1046 85))',
                'POLYGON((1020 1076 190,1025 1085 35,1065 1090 26,1064 1078 35,1020 1076 190),(1023 1079 5,1061 1081 35,1062 1087 26,1028 1082 35,1023 1079 85))',
                'LINESTRING(1005 1000 10, 1100 1050 120)',
                'LINESTRING(1000 1000 150, 1095 1050 -5, 1080 1080 200)']
    celsius_field_values = [50, 255, 60, 100, 180]

    i = 0
    for g in wkt_geom:
        feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
        feat.SetGeometryDirectly(ogr.Geometry(wkt=g))
        feat.SetField('CELSIUS', celsius_field_values[i])
        rast_mem_lyr.CreateFeature(feat)
        i = i + 1

    # Run the algorithm.
    err = gdal.RasterizeLayer(target_ds, [1, 2, 3], rast_mem_lyr,
                              options=["ATTRIBUTE=CELSIUS"])

    assert err == 0, 'got non-zero result code from RasterizeLayer'

    # Check results.
    expected = 16265
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdal.GetDriverByName('GTiff').CreateCopy('tmp/rasterize_4.tif', target_ds)
        pytest.fail('Did not get expected image checksum')


###############################################################################
# Rasterization with MERGE_ALG=ADD.


def test_rasterize_5():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    # Create a memory raster to rasterize into.

    target_ds = gdal.GetDriverByName('MEM').Create('', 100, 100, 3,
                                                   gdal.GDT_Byte)
    target_ds.SetGeoTransform((1000, 1, 0, 1100, 0, -1))
    target_ds.SetProjection(sr_wkt)

    # Create a memory layer to rasterize from.

    rast_ogr_ds = \
        ogr.GetDriverByName('Memory').CreateDataSource('wrk')
    rast_mem_lyr = rast_ogr_ds.CreateLayer('poly', srs=sr)

    # Add polygons.

    wkt_geom = 'POLYGON((1020 1030,1020 1045,1050 1045,1050 1030,1020 1030))'
    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))
    rast_mem_lyr.CreateFeature(feat)

    wkt_geom = 'POLYGON((1045 1050,1055 1050,1055 1020,1045 1020,1045 1050))'
    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))
    rast_mem_lyr.CreateFeature(feat)

    # Add linestrings.

    wkt_geom = 'LINESTRING(1000 1000, 1100 1050)'
    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))
    rast_mem_lyr.CreateFeature(feat)

    wkt_geom = 'LINESTRING(1005 1000, 1000 1050)'
    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))
    rast_mem_lyr.CreateFeature(feat)

    # Run the algorithm.

    err = gdal.RasterizeLayer(target_ds, [1, 2, 3], rast_mem_lyr,
                              burn_values=[256, 110, -1],
                              options=["MERGE_ALG=ADD"])

    assert err == 0, 'got non-zero result code from RasterizeLayer'

    # Check results.

    expected = 13022
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdal.GetDriverByName('GTiff').CreateCopy('tmp/rasterize_5.tif', target_ds)
        pytest.fail('Did not get expected image checksum')

    _, maxval = target_ds.GetRasterBand(1).ComputeRasterMinMax()
    assert maxval == 255

    minval, _ = target_ds.GetRasterBand(3).ComputeRasterMinMax()
    assert minval == 0


###############################################################################
# Test bug fix for #5580 (used to hang)

def test_rasterize_6():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    wkb = struct.pack('B' * 93, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 5, 65, 28, 138, 141, 120, 239, 76, 104, 65, 87, 9, 185, 80, 29, 20, 208, 65, 28, 144, 191, 125, 165, 41, 54, 65, 87, 64, 14, 111, 103, 53, 124, 65, 30, 132, 127, 255, 255, 255, 254, 65, 87, 63, 241, 218, 241, 62, 127, 65, 30, 132, 128, 0, 0, 0, 0, 65, 87, 9, 156, 142, 126, 144, 236, 65, 28, 138, 141, 120, 239, 76, 104, 65, 87, 9, 185, 80, 29, 20, 208)

    data_source = ogr.GetDriverByName('MEMORY').CreateDataSource('')
    layer = data_source.CreateLayer('', sr, geom_type=ogr.wkbPolygon)
    feature = ogr.Feature(layer.GetLayerDefn())
    feature.SetGeometryDirectly(ogr.CreateGeometryFromWkb(wkb))
    layer.CreateFeature(feature)

    mask_ds = gdal.GetDriverByName('Mem').Create('', 5000, 5000, 1, gdal.GDT_Byte)
    mask_ds.SetGeoTransform([499000, 0.4, 0, 6095000, 0, -0.4])
    mask_ds.SetProjection(sr_wkt)

    gdal.RasterizeLayer(mask_ds, [1], layer, burn_values=[1], options=["ALL_TOUCHED"])


###############################################################################
# Test rasterizing linestring with multiple segments and MERGE_ALG=ADD
# Tests https://github.com/OSGeo/gdal/issues/1307

def test_rasterize_merge_alg_add_multiple_segment_linestring():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    data_source = ogr.GetDriverByName('MEMORY').CreateDataSource('')
    layer = data_source.CreateLayer('', sr, geom_type=ogr.wkbLineString)
    feature = ogr.Feature(layer.GetLayerDefn())
    # Diagonal segments
    feature.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING(0.5 0.5,100.5 50.5,199.5 99.5)'))
    layer.CreateFeature(feature)
    feature = ogr.Feature(layer.GetLayerDefn())
    # Vertical and horizontal segments
    feature.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING(30.5 40.5,30.5 70.5,50.5 70.5)'))
    layer.CreateFeature(feature)

    ds = gdal.GetDriverByName('Mem').Create('', 10, 10, 1, gdal.GDT_Byte)
    ds.SetGeoTransform([0, 20, 0, 100, 0, -10])
    ds.SetProjection(sr_wkt)

    ds.GetRasterBand(1).Fill(0)
    gdal.RasterizeLayer(ds, [1], layer, burn_values=[1], options=["MERGE_ALG=ADD"])

    got = struct.unpack('B' * 100, ds.ReadRaster())
    expected = (0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
                0, 1, 1, 0, 0, 0, 0, 1, 0, 0,
                0, 1, 0, 0, 0, 0, 1, 0, 0, 0,
                0, 1, 0, 0, 0, 1, 0, 0, 0, 0,
                0, 1, 0, 0, 1, 0, 0, 0, 0, 0,
                0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
                0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
                0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
                1, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    assert got == expected, '%s' % str(got)

    ds.GetRasterBand(1).Fill(0)
    gdal.RasterizeLayer(ds, [1], layer, burn_values=[1], options=["MERGE_ALG=ADD", "ALL_TOUCHED"])

    got = struct.unpack('B' * 100, ds.ReadRaster())
    expected = (0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
                0, 1, 1, 0, 0, 0, 1, 1, 1, 0,
                0, 1, 0, 0, 0, 1, 1, 0, 0, 0,
                0, 1, 0, 0, 1, 1, 0, 0, 0, 0,
                0, 1, 0, 1, 1, 0, 0, 0, 0, 0,
                0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
                0, 1, 1, 0, 0, 0, 0, 0, 0, 0,
                1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
                1, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    assert got == expected, '%s' % str(got)

###############################################################################
# Test rasterizing polygon with horizontal segments and MERGE_ALG=ADD
# to check that we don't redraw several times the top segment, depending on
# the winding order


@pytest.mark.parametrize("wkt",
                         ['POLYGON((0 0,0 1,1 1,1 0,0 0))',
                          'POLYGON((0 0,1 0,1 1,0 1,0 0))'],
                         ids=['clockwise', 'counterclockwise'])
def test_rasterize_merge_alg_add_polygon(wkt):

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    data_source = ogr.GetDriverByName('MEMORY').CreateDataSource('')
    layer = data_source.CreateLayer('', sr, geom_type=ogr.wkbPolygon)
    feature = ogr.Feature(layer.GetLayerDefn())
    feature.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
    layer.CreateFeature(feature)

    ds = gdal.GetDriverByName('Mem').Create('', 5, 5, 1, gdal.GDT_Byte)
    ds.SetGeoTransform([-0.125, 0.25, 0, 1.125, 0, -0.25])
    ds.SetProjection(sr_wkt)

    ds.GetRasterBand(1).Fill(0)
    gdal.RasterizeLayer(ds, [1], layer, burn_values=[10], options=["MERGE_ALG=ADD"])

    got = struct.unpack('B' * 25, ds.ReadRaster())
    expected = (0, 10, 10, 10, 10,
                0, 10, 10, 10, 10,
                0, 10, 10, 10, 10,
                0, 10, 10, 10, 10,
                0, 10, 10, 10, 10,)
    assert got == expected, '%s' % str(got)
