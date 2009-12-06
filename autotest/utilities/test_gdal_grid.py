#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_grid testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault @ mines-paris dot org>
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

sys.path.append( '../pymod' )
sys.path.append( '../gcore' )

import gdal
import ogr
import gdaltest
import test_cli_utilities
import struct

###############################################################################
# 

def test_gdal_grid_1():
    if test_cli_utilities.get_gdal_grid_path() is None:
        return 'skip'

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')

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
    gdaltest.runexternal(test_cli_utilities.get_gdal_grid_path() + ' -txe -80.0041667 -78.9958333 -tye 42.9958333 44.0041667 -outsize 121 121 -ot Int16 -l n43 tmp/n43.shp tmp/n43.tif  -a nearest:radius1=0.0:radius2=0.0:angle=0.0:nodata=0.0 -co TILED=YES -co BLOCKXSIZE=256 -co BLOCKYSIZE=256')

    # We should get the same values as in n43.td0
    ds2 = gdal.Open('tmp/n43.tif')
    if ds.GetRasterBand(1).Checksum() != ds2.GetRasterBand(1).Checksum():
        print('bad checksum : got %d, expected %d' % (ds.GetRasterBand(1).Checksum() , ds2.GetRasterBand(1).Checksum()))
        return 'fail'

    ds = None
    ds2 = None

    return 'success'

###############################################################################
# Cleanup

def test_gdal_grid_cleanup():

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/n43.shp')
    drv = gdal.GetDriverByName('GTiff')
    drv.Delete('tmp/n43.tif')

    return 'success'

gdaltest_list = [
    test_gdal_grid_1,
    test_gdal_grid_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_grid' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()





