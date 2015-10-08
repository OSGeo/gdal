#!/usr/bin/env python
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

import sys
import struct

sys.path.append( '../pymod' )

from osgeo import gdal, ogr

import gdaltest

###############################################################################
# 

def test_gdal_grid_lib_1():

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')

    # Create an OGR grid from the values of n43.dt0
    ds = gdal.Open('../gdrivers/data/n43.dt0')
    geotransform = ds.GetGeoTransform()

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    shape_ds = shape_drv.CreateDataSource( '/vsimem/tmp' )
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

    shape_ds.ExecuteSQL('CREATE SPATIAL INDEX ON n43')

    shape_ds = None

    # Create a GDAL dataset from the previous generated OGR grid
    ds2 = gdal.Grid('', '/vsimem/tmp/n43.shp', format = 'MEM', \
                    outputBounds = [ -80.0041667, 42.9958333, -78.9958333 , 44.0041667], \
                    width = 121, height = 121, outputType = gdal.GDT_Int16, \
                    algorithm = 'nearest:radius1=0.0:radius2=0.0:angle=0.0',
                    spatFilter = [ -180, -90, 180, 90 ])
    # We should get the same values as in n43.td0
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
# Cleanup

def test_gdal_grid_lib_cleanup():

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/tmp')

    return 'success'

gdaltest_list = [
    test_gdal_grid_lib_1,
    test_gdal_grid_lib_cleanup,
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_grid_lib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()





