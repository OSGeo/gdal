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

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

try:
    from osgeo import gdal, gdalconst, ogr, osr
except:
    import gdal
    import gdalconst
    import ogr
    import osr

###############################################################################
# Simple polygon rasterization.

def rasterize_1():

    try:
        x = gdal.RasterizeLayer
        gdaltest.have_ng = 1
    except:
        gdaltest.have_ng = 0
        return 'skip'

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
        print err
        gdaltest.post_reason( 'got non-zero result code from RasterizeLayer' )
        return 'fail'

    # Check results.

    expected = 6452
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print checksum
        gdaltest.post_reason( 'Did not get expected image checksum' )

        gdal.GetDriverByName('GTiff').CreateCopy('tmp/rasterize_1.tif',target_ds)
        return 'fail'
    
    return 'success'

gdaltest_list = [
    rasterize_1
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'rasterize' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

