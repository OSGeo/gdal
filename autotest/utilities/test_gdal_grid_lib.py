#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_grid testing
# Author:   Even Rouault <even dot rouault @ spatialys dot com>
#
###############################################################################
# Copyright (c) 2008-2015, Even Rouault <even dot rouault at spatialys dot com>
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


from osgeo import gdal, ogr

import ogrtest

###############################################################################
#


def test_gdal_grid_lib_1():

    # Create an OGR grid from the values of n43.dt0
    ds = gdal.Open('../gdrivers/data/n43.dt0')
    geotransform = ds.GetGeoTransform()

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    shape_ds = shape_drv.CreateDataSource('/vsimem/tmp')
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

    shape_ds.ExecuteSQL('CREATE SPATIAL INDEX ON n43')

    shape_ds = None

    spatFilter = None
    if ogrtest.have_geos():
        spatFilter = [-180, -90, 180, 90]

    # Create a GDAL dataset from the previous generated OGR grid
    ds2 = gdal.Grid('', '/vsimem/tmp/n43.shp', format='MEM',
                    outputBounds=[-80.0041667, 42.9958333, -78.9958333, 44.0041667],
                    width=121, height=121, outputType=gdal.GDT_Int16,
                    algorithm='nearest:radius1=0.0:radius2=0.0:angle=0.0',
                    spatFilter=spatFilter)
    # We should get the same values as in n43.td0
    assert ds.GetRasterBand(1).Checksum() == ds2.GetRasterBand(1).Checksum(), \
        ('bad checksum : got %d, expected %d' % (ds.GetRasterBand(1).Checksum(), ds2.GetRasterBand(1).Checksum()))
    assert ds2.GetRasterBand(1).GetNoDataValue() is None, 'did not expect nodata value'

    ds = None
    ds2 = None

###############################################################################
# Test with a point number not multiple of 8 or 16


def test_gdal_grid_lib_2():

    shape_ds = ogr.Open('/vsimem/tmp', update=1)
    shape_lyr = shape_ds.CreateLayer('test_gdal_grid_lib_2')
    dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
    dst_feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0 100)'))
    shape_lyr.CreateFeature(dst_feat)
    shape_ds = None

    for env_list in [[('GDAL_USE_AVX', 'NO'), ('GDAL_USE_SSE', 'NO')], [('GDAL_USE_AVX', 'NO')], []]:

        for (key, value) in env_list:
            gdal.SetConfigOption(key, value)

        # Point strictly on grid
        ds1 = gdal.Grid('', '/vsimem/tmp/test_gdal_grid_lib_2.shp', format='MEM',
                        outputBounds=[-0.5, -0.5, 0.5, 0.5],
                        width=1, height=1, outputType=gdal.GDT_Byte)

        ds2 = gdal.Grid('', '/vsimem/tmp/test_gdal_grid_lib_2.shp', format='MEM',
                        outputBounds=[-0.4, -0.4, 0.6, 0.6],
                        width=10, height=10, outputType=gdal.GDT_Byte)

        gdal.SetConfigOption('GDAL_USE_AVX', None)
        gdal.SetConfigOption('GDAL_USE_SSE', None)

        cs = ds1.GetRasterBand(1).Checksum()
        assert cs == 2

        cs = ds2.GetRasterBand(1).Checksum()
        assert cs == 1064

    
###############################################################################
# Test bugfix for #7101 (segmentation fault with linear interpolation)


def test_gdal_grid_lib_3():

    wkt = 'POLYGON ((37.3495241627097 55.6901648563184 187.680953979492,37.349543273449 55.6901565410051 187.714370727539,37.3495794832707 55.6901531392856 187.67333984375,37.3496210575104 55.6901595647556 187.6396484375,37.3496398329735 55.6901716597552 187.596603393555,37.3496726900339 55.6901780852222 187.681350708008,37.3496793955565 55.6901829988139 187.933898925781,37.3496921360493 55.6901860225623 187.934280395508,37.3497162759304 55.6902037870796 187.435394287109,37.3497484624386 55.6902094566047 187.515319824219,37.3497618734837 55.6902241973661 190.329940795898,37.3497511446476 55.690238560154 190.345748901367,37.3497404158115 55.6902567026153 190.439697265625,37.3497142642736 55.6902650179072 189.086044311523,37.349688783288 55.6902608602615 187.763305664062,37.3496626317501 55.6902468754498 187.53678894043,37.3496378213167 55.6902412059301 187.598648071289,37.3496103286743 55.6902400720261 187.806274414062,37.3495902121067 55.6902313787607 187.759521484375,37.3495734483004 55.6902177719067 187.578125,37.349532879889 55.6902035980954 187.56965637207,37.3495161160827 55.6901939599008 187.541793823242,37.3495187982917 55.6901754394418 187.610427856445,37.3495241627097 55.6901648563184 187.680953979492))'
    polygon = ogr.CreateGeometryFromWkt(wkt)

    gdal.Grid('', polygon.ExportToJson(),
              width=115, height=93, outputBounds=[37.3495161160827, 55.6901531392856, 37.3497618734837, 55.6902650179072],
              format='MEM', algorithm='linear')

###############################################################################
# Cleanup


def test_gdal_grid_lib_cleanup():

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/tmp')




