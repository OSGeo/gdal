#!/usr/bin/env python
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

import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

from osgeo import gdal, ogr, osr

###############################################################################
# Simple polygon rasterization.

def rasterize_1():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference( sr_wkt )
    
    # Create a memory raster to rasterize into.

    target_ds = gdal.GetDriverByName('MEM').Create( '', 100, 100, 3,
                                                    gdal.GDT_Byte )
    target_ds.SetGeoTransform( (1000,1,0,1100,0,-1) )
    target_ds.SetProjection( sr_wkt )
    
    # Create a memory layer to rasterize from.

    rast_ogr_ds = \
              ogr.GetDriverByName('Memory').CreateDataSource( 'wrk' )
    rast_mem_lyr = rast_ogr_ds.CreateLayer( 'poly', srs=sr )

    # Add a polygon.
    
    wkt_geom = 'POLYGON((1020 1030,1020 1045,1050 1045,1050 1030,1020 1030))'
    
    feat = ogr.Feature( rast_mem_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )

    rast_mem_lyr.CreateFeature( feat )

    # Add a linestring.
    
    wkt_geom = 'LINESTRING(1000 1000, 1100 1050)'
    
    feat = ogr.Feature( rast_mem_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )

    rast_mem_lyr.CreateFeature( feat )

    # Run the algorithm.

    err = gdal.RasterizeLayer( target_ds, [3,2,1], rast_mem_lyr,
                               burn_values = [200,220,240] )

    if err != 0:
        print(err)
        gdaltest.post_reason( 'got non-zero result code from RasterizeLayer' )
        return 'fail'

    # Check results.

    expected = 6452
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )

        gdal.GetDriverByName('GTiff').CreateCopy('tmp/rasterize_1.tif',target_ds)
        return 'fail'
    
    return 'success'

###############################################################################
# Test rasterization with ALL_TOUCHED.

def rasterize_2():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    
    # Create a memory raster to rasterize into.

    target_ds = gdal.GetDriverByName('MEM').Create( '', 12, 12, 3,
                                                    gdal.GDT_Byte )
    target_ds.SetGeoTransform( (0,1,0,12,0,-1) )
    target_ds.SetProjection( sr_wkt )
    
    # Create a memory layer to rasterize from.

    cutline_ds = ogr.Open( 'data/cutline.csv' )

    # Run the algorithm.

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    err = gdal.RasterizeLayer( target_ds, [3,2,1], cutline_ds.GetLayer(0),
                               burn_values = [200,220,240],
                               options = ["ALL_TOUCHED=TRUE"] )
    gdal.PopErrorHandler()

    if err != 0:
        print(err)
        gdaltest.post_reason( 'got non-zero result code from RasterizeLayer' )
        return 'fail'

    # Check results.

    expected = 121
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )

        gdal.GetDriverByName('GTiff').CreateCopy('tmp/rasterize_2.tif',target_ds)
        return 'fail'
    
    return 'success'

###############################################################################
# Rasterization with BURN_VALUE_FROM.

def rasterize_3():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference( sr_wkt )
    
    # Create a memory raster to rasterize into.

    target_ds = gdal.GetDriverByName('MEM').Create( '', 100, 100, 3,
                                                    gdal.GDT_Byte )
    target_ds.SetGeoTransform( (1000,1,0,1100,0,-1) )
    target_ds.SetProjection( sr_wkt )
    
    # Create a memory layer to rasterize from.

    rast_ogr_ds = \
              ogr.GetDriverByName('Memory').CreateDataSource( 'wrk' )
    rast_mem_lyr = rast_ogr_ds.CreateLayer( 'poly', srs=sr )

    # Add polygons and linestrings.
    wkt_geom = ['POLYGON((1020 1030 40,1020 1045 30,1050 1045 20,1050 1030 35,1020 1030 40))',
                'POLYGON((1010 1046 85,1015 1055 35,1055 1060 26,1054 1048 35,1010 1046 85))',
                'POLYGON((1020 1076 190,1025 1085 35,1065 1090 26,1064 1078 35,1020 1076 190),(1023 1079 5,1061 1081 35,1062 1087 26,1028 1082 35,1023 1079 85))',
                'LINESTRING(1005 1000 10, 1100 1050 120)',
                'LINESTRING(1000 1000 150, 1095 1050 -5, 1080 1080 200)']
    for g in wkt_geom:
        feat = ogr.Feature( rast_mem_lyr.GetLayerDefn() )
        feat.SetGeometryDirectly( ogr.Geometry(wkt = g) )
        rast_mem_lyr.CreateFeature( feat )

    # Run the algorithm.

    err = gdal.RasterizeLayer( target_ds, [3,2,1], rast_mem_lyr,
                               burn_values = [10,10,55], options = ["BURN_VALUE_FROM=Z"] )

    if err != 0:
        print(err)
        gdaltest.post_reason( 'got non-zero result code from RasterizeLayer' )
        return 'fail'

    # Check results.

    expected = 15006
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )
        gdal.GetDriverByName('GTiff').CreateCopy('tmp/rasterize_3.tif',target_ds)
        return 'fail'
    
    return 'success'

###############################################################################
# Rasterization with ATTRIBUTE.

def rasterize_4():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference( sr_wkt )
    
    # Create a memory raster to rasterize into.
    target_ds = gdal.GetDriverByName('MEM').Create( '', 100, 100, 3,
                                                    gdal.GDT_Byte )
    target_ds.SetGeoTransform( (1000,1,0,1100,0,-1) )
    target_ds.SetProjection( sr_wkt )
    
    # Create a memory layer to rasterize from.
    rast_ogr_ds = ogr.GetDriverByName('Memory').CreateDataSource( 'wrk' )
    rast_mem_lyr = rast_ogr_ds.CreateLayer( 'poly', srs=sr )
    # Setup Schema
    ogrtest.quick_create_layer_def( rast_mem_lyr,
                                    [ ('CELSIUS', ogr.OFTReal) ] )
    
    # Add polygons and linestrings and a field named CELSIUS.
    wkt_geom = ['POLYGON((1020 1030 40,1020 1045 30,1050 1045 20,1050 1030 35,1020 1030 40))',
                'POLYGON((1010 1046 85,1015 1055 35,1055 1060 26,1054 1048 35,1010 1046 85))',
                'POLYGON((1020 1076 190,1025 1085 35,1065 1090 26,1064 1078 35,1020 1076 190),(1023 1079 5,1061 1081 35,1062 1087 26,1028 1082 35,1023 1079 85))',
                'LINESTRING(1005 1000 10, 1100 1050 120)',
                'LINESTRING(1000 1000 150, 1095 1050 -5, 1080 1080 200)']
    celsius_field_values = [50,255,60,100,180]

    i = 0
    for g in wkt_geom:
        feat = ogr.Feature( rast_mem_lyr.GetLayerDefn() )
        feat.SetGeometryDirectly( ogr.Geometry(wkt = g) )
        feat.SetField( 'CELSIUS', celsius_field_values[i] )
        rast_mem_lyr.CreateFeature( feat )
        i = i + 1

    # Run the algorithm.
    err = gdal.RasterizeLayer( target_ds, [1,2,3], rast_mem_lyr,
                               options = ["ATTRIBUTE=CELSIUS"] )

    if err != 0:
        print(err)
        gdaltest.post_reason( 'got non-zero result code from RasterizeLayer' )
        return 'fail'

    # Check results.
    expected = 16265
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )
        gdal.GetDriverByName('GTiff').CreateCopy('tmp/rasterize_4.tif',target_ds)
        return 'fail'
    
    return 'success'

###############################################################################
# Rasterization with MERGE_ALG=ADD.

def rasterize_5():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference( sr_wkt )
    
    # Create a memory raster to rasterize into.

    target_ds = gdal.GetDriverByName('MEM').Create( '', 100, 100, 3,
                                                    gdal.GDT_Byte )
    target_ds.SetGeoTransform( (1000,1,0,1100,0,-1) )
    target_ds.SetProjection( sr_wkt )
    
    # Create a memory layer to rasterize from.

    rast_ogr_ds = \
              ogr.GetDriverByName('Memory').CreateDataSource( 'wrk' )
    rast_mem_lyr = rast_ogr_ds.CreateLayer( 'poly', srs=sr )

    # Add polygons.
    
    wkt_geom = 'POLYGON((1020 1030,1020 1045,1050 1045,1050 1030,1020 1030))'
    feat = ogr.Feature( rast_mem_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )
    rast_mem_lyr.CreateFeature( feat )

    wkt_geom = 'POLYGON((1045 1050,1055 1050,1055 1020,1045 1020,1045 1050))'
    feat = ogr.Feature( rast_mem_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )
    rast_mem_lyr.CreateFeature( feat )

    # Add linestrings.
    
    wkt_geom = 'LINESTRING(1000 1000, 1100 1050)'
    feat = ogr.Feature( rast_mem_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )
    rast_mem_lyr.CreateFeature( feat )

    wkt_geom = 'LINESTRING(1005 1000, 1000 1050)'
    feat = ogr.Feature( rast_mem_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )
    rast_mem_lyr.CreateFeature( feat )

    # Run the algorithm.

    err = gdal.RasterizeLayer( target_ds, [1, 2, 3], rast_mem_lyr,
                               burn_values = [100,110,120],
                               options = ["MERGE_ALG=ADD"])

    if err != 0:
        print(err)
        gdaltest.post_reason( 'got non-zero result code from RasterizeLayer' )
        return 'fail'

    # Check results.

    expected = 13022
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )

        gdal.GetDriverByName('GTiff').CreateCopy('tmp/rasterize_5.tif',target_ds)
        return 'fail'
    
    return 'success'

gdaltest_list = [
    rasterize_1,
    rasterize_2,
    rasterize_3,
    rasterize_4,
    rasterize_5,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'rasterize' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

