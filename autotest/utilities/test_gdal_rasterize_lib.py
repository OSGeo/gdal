#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_rasterize testing
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

from osgeo import gdal, ogr, osr
import gdaltest

###############################################################################
# Simple polygon rasterization (adapted from alg/rasterize.py).

def test_gdal_rasterize_lib_1():


    # Setup working spatial reference
    #sr_wkt = 'LOCAL_CS["arbitrary"]'
    #sr = osr.SpatialReference( sr_wkt )
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)
    sr_wkt = sr.ExportToWkt()

    # Create a raster to rasterize into.

    target_ds = gdal.GetDriverByName('MEM').Create( '', 100, 100, 3,
                                                    gdal.GDT_Byte )
    target_ds.SetGeoTransform( (1000,1,0,1100,0,-1) )
    target_ds.SetProjection( sr_wkt )

    # Create a layer to rasterize from.

    vector_ds = \
              gdal.GetDriverByName('Memory').Create( '', 0, 0, 0 )
    rast_lyr = vector_ds.CreateLayer( 'rast1', srs=sr )

    rast_lyr.GetLayerDefn()
    field_defn = ogr.FieldDefn('foo')
    rast_lyr.CreateField(field_defn)

    # Add a polygon.

    wkt_geom = 'POLYGON((1020 1030,1020 1045,1050 1045,1050 1030,1020 1030))'

    feat = ogr.Feature( rast_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )

    rast_lyr.CreateFeature( feat )

    # Add feature without geometry to test fix for #3310
    feat = ogr.Feature( rast_lyr.GetLayerDefn() )
    rast_lyr.CreateFeature( feat )

    # Add a linestring.

    wkt_geom = 'LINESTRING(1000 1000, 1100 1050)'

    feat = ogr.Feature( rast_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )

    rast_lyr.CreateFeature( feat )

    ret = gdal.Rasterize(target_ds, vector_ds, bands = [3,2,1], burnValues = [200,220,240], layers = 'rast1')
    if ret != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check results.
    expected = 6452
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )

        return 'fail'

    target_ds = None

    return 'success'

###############################################################################
# Test creating an output file

def test_gdal_rasterize_lib_3():

    import test_cli_utilities
    if test_cli_utilities.get_gdal_contour_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_contour_path() + ' ../gdrivers/data/n43.dt0 tmp/n43dt0.shp -i 10 -3d')

    with gdaltest.error_handler():
        ds = gdal.Rasterize('/vsimem/bogus.tif', 'tmp/n43dt0.shp')
    if ds is not None:
        gdaltest.post_reason('did not expected success')
        return 'fail'

    ds = gdal.Rasterize('', 'tmp/n43dt0.shp', format = 'MEM', outputType = gdal.GDT_Byte, useZ = True, layers = ['n43dt0'], width = 121, height = 121, noData = 0)

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource( 'tmp/n43dt0.shp' )

    ds_ref = gdal.Open('../gdrivers/data/n43.dt0')

    if ds.GetRasterBand(1).GetNoDataValue() != 0.0:
        gdaltest.post_reason('did not get expected nodata value')
        return 'fail'

    if ds.RasterXSize != 121 or ds.RasterYSize != 121:
        gdaltest.post_reason('did not get expected dimensions')
        return 'fail'

    gt_ref = ds_ref.GetGeoTransform()
    gt = ds.GetGeoTransform()
    for i in range(6):
        if (abs(gt[i]-gt_ref[i])>1e-6):
            gdaltest.post_reason('did not get expected geotransform')
            print(gt)
            print(gt_ref)
            return 'fail'

    wkt = ds.GetProjectionRef()
    if wkt.find("WGS_1984") == -1:
        gdaltest.post_reason('did not get expected SRS')
        print(wkt)
        return 'fail'

    return 'success'

###############################################################################
# Rasterization without georeferencing

def test_gdal_rasterize_lib_100():

    target_ds = gdal.GetDriverByName('MEM').Create( '', 100, 100 )

    # Create a layer to rasterize from.

    vector_ds = \
              gdal.GetDriverByName('Memory').Create( '', 0, 0, 0 )
    rast_lyr = vector_ds.CreateLayer( 'rast1' )

    wkt_geom = 'POLYGON((20 20,20 80,80 80,80 20,20 20))'

    feat = ogr.Feature( rast_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( ogr.Geometry(wkt = wkt_geom) )

    rast_lyr.CreateFeature( feat )

    ret = gdal.Rasterize(target_ds, vector_ds, burnValues = [255])
    if ret != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check results.
    expected = 44190
    checksum = target_ds.GetRasterBand(1).Checksum()
    if checksum != expected:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )

        return 'fail'

    target_ds = None

    return 'success'

###############################################################################
# Rasterization on empty geometry

def test_gdal_rasterize_lib_101():

    target_ds = gdal.GetDriverByName('MEM').Create( '', 100, 100 )

    # Create a layer to rasterize from.

    vector_ds = \
              gdal.GetDriverByName('Memory').Create( '', 0, 0, 0 )
    rast_lyr = vector_ds.CreateLayer( 'rast1' )

    # polygon with empty exterior ring
    geom = ogr.CreateGeometryFromJson('{ "type": "Polygon", "coordinates": [ [ ] ] }')

    feat = ogr.Feature( rast_lyr.GetLayerDefn() )
    feat.SetGeometryDirectly( geom )

    rast_lyr.CreateFeature( feat )

    ret = gdal.Rasterize(target_ds, vector_ds, burnValues = [255])
    if ret != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check results.
    checksum = target_ds.GetRasterBand(1).Checksum()
    if checksum != 0:
        print(checksum)
        gdaltest.post_reason( 'Did not get expected image checksum' )

        return 'fail'

    target_ds = None

    return 'success'

gdaltest_list = [
    test_gdal_rasterize_lib_1,
    test_gdal_rasterize_lib_3,
    test_gdal_rasterize_lib_100,
    test_gdal_rasterize_lib_101
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_rasterize_lib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()





