#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test the GenImgProjTransformer capabilities.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import gdal
from osgeo import osr

###############################################################################
# Test simple Geotransform based transformer.

def transformer_1():

    ds = gdal.Open('data/byte.tif')
    tr = gdal.Transformer( ds, None, [] )

    (success,pnt) = tr.TransformPoint( 0, 20, 10 )

    if not success \
       or abs(pnt[0]- 441920) > 0.00000001 \
       or abs(pnt[1]-3750720) > 0.00000001 \
       or pnt[2] != 0.0:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong forward transform result.' )
        return 'fail'

    (success,pnt) = tr.TransformPoint( 1, pnt[0], pnt[1], pnt[2] )

    if not success \
       or abs(pnt[0]-20) > 0.00000001 \
       or abs(pnt[1]-10) > 0.00000001 \
       or pnt[2] != 0.0:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong reverse transform result.' )
        return 'fail'

    return 'success' 

###############################################################################
# Test GCP based transformer with polynomials.

def transformer_2():

    ds = gdal.Open('data/gcps.vrt')
    tr = gdal.Transformer( ds, None, [ 'METHOD=GCP_POLYNOMIAL' ] )

    (success,pnt) = tr.TransformPoint( 0, 20, 10 )

    if not success \
       or abs(pnt[0]-441920) > 0.001 \
       or abs(pnt[1]-3750720) > 0.001 \
       or pnt[2] != 0:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong forward transform result.' )
        return 'fail'

    (success,pnt) = tr.TransformPoint( 1, pnt[0], pnt[1], pnt[2] )

    if not success \
       or abs(pnt[0]-20) > 0.001 \
       or abs(pnt[1]-10) > 0.001 \
       or pnt[2] != 0:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong reverse transform result.' )
        return 'fail'

    return 'success' 

###############################################################################
# Test GCP based transformer with thin plate splines.

def transformer_3():

    ds = gdal.Open('data/gcps.vrt')
    tr = gdal.Transformer( ds, None, [ 'METHOD=GCP_TPS' ] )

    (success,pnt) = tr.TransformPoint( 0, 20, 10 )

    if not success \
       or abs(pnt[0]-441920) > 0.001 \
       or abs(pnt[1]-3750720) > 0.001 \
       or pnt[2] != 0:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong forward transform result.' )
        return 'fail'

    (success,pnt) = tr.TransformPoint( 1, pnt[0], pnt[1], pnt[2] )

    if not success \
       or abs(pnt[0]-20) > 0.001 \
       or abs(pnt[1]-10) > 0.001 \
       or pnt[2] != 0:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong reverse transform result.' )
        return 'fail'

    return 'success' 

###############################################################################
# Test geolocation based transformer.

def transformer_4():

    ds = gdal.Open('data/sstgeo.vrt')
    tr = gdal.Transformer( ds, None, [ 'METHOD=GEOLOC_ARRAY' ] )

    (success,pnt) = tr.TransformPoint( 0, 20, 10 )

    if not success \
       or abs(pnt[0]+81.961341857910156) > 0.000001 \
       or abs(pnt[1]-29.612689971923828) > 0.000001 \
       or pnt[2] != 0:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong forward transform result.' )
        return 'fail'

    (success,pnt) = tr.TransformPoint( 1, pnt[0], pnt[1], pnt[2] )

    if not success \
       or abs(pnt[0]-19.554539744554866) > 0.001 \
       or abs(pnt[1]-9.1910760024906537) > 0.001 \
       or pnt[2] != 0:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong reverse transform result.' )
        return 'fail'

    return 'success' 

###############################################################################
# Test RPC based transformer.

def transformer_5():

    ds = gdal.Open('data/rpc.vrt')
    tr = gdal.Transformer( ds, None, [ 'METHOD=RPC' ] )

    (success,pnt) = tr.TransformPoint( 0, 20, 10 )

    if not success \
       or abs(pnt[0]-125.64830100509131) > 0.000001 \
       or abs(pnt[1]-39.869433991997553) > 0.000001 \
       or pnt[2] != 0:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong forward transform result.' )
        return 'fail'

    (success,pnt) = tr.TransformPoint( 1, pnt[0], pnt[1], pnt[2] )

    if not success \
       or abs(pnt[0]-20) > 0.001 \
       or abs(pnt[1]-10) > 0.001 \
       or pnt[2] != 0:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong reverse transform result.' )
        return 'fail'

    # Try with a different height.
    
    (success,pnt) = tr.TransformPoint( 0, 20, 10, 30 )

    if not success \
       or abs(pnt[0]-125.64828521533849) > 0.000001 \
       or abs(pnt[1]-39.869345204440144) > 0.000001 \
       or pnt[2] != 30:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong forward transform result.(2)' )
        return 'fail'

    (success,pnt) = tr.TransformPoint( 1, pnt[0], pnt[1], pnt[2] )

    if not success \
       or abs(pnt[0]-20) > 0.001 \
       or abs(pnt[1]-10) > 0.001 \
       or pnt[2] != 30:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong reverse transform result.(2)' )
        return 'fail'

    # Test RPC_HEIGHT option
    tr = gdal.Transformer( ds, None, [ 'METHOD=RPC', 'RPC_HEIGHT=30' ] )

    (success,pnt) = tr.TransformPoint( 0, 20, 10 )

    if not success \
       or abs(pnt[0]-125.64828521533849) > 0.000001 \
       or abs(pnt[1]-39.869345204440144) > 0.000001 :
        print(success, pnt)
        gdaltest.post_reason( 'got wrong forward transform result.(3)' )
        return 'fail'

    (success,pnt) = tr.TransformPoint( 1, pnt[0], pnt[1], pnt[2] )

    if not success \
       or abs(pnt[0]-20) > 0.001 \
       or abs(pnt[1]-10) > 0.001 :
        print(success, pnt)
        gdaltest.post_reason( 'got wrong reverse transform result.(3)' )
        return 'fail'

    # Test RPC_DEM and RPC_HEIGHT_SCALE options

    # (long,lat)=(125.64828521533849 39.869345204440144) -> (Easting,Northing)=(213324.662167036 4418634.47813677) in EPSG:32652
    ds_dem = gdal.GetDriverByName('GTiff').Create('/vsimem/dem.tif', 100, 100, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32652)
    ds_dem.SetProjection(sr.ExportToWkt())
    ds_dem.SetGeoTransform([213300,1,0,4418700,0,-1])
    ds_dem.GetRasterBand(1).Fill(15)
    ds_dem = None

    tr = gdal.Transformer( ds, None, [ 'METHOD=RPC', 'RPC_HEIGHT_SCALE=2', 'RPC_DEM=/vsimem/dem.tif' ] )

    (success,pnt) = tr.TransformPoint( 0, 20, 10, 0 )

    if not success \
       or abs(pnt[0]-125.64828521533849) > 0.000001 \
       or abs(pnt[1]-39.869345204440144) > 0.000001 :
        print(success, pnt)
        gdaltest.post_reason( 'got wrong forward transform result.(4)' )
        return 'fail'

    (success,pnt) = tr.TransformPoint( 1, pnt[0], pnt[1], pnt[2] )

    if not success \
       or abs(pnt[0]-20) > 0.001 \
       or abs(pnt[1]-10) > 0.001 :
        print(success, pnt)
        gdaltest.post_reason( 'got wrong reverse transform result.(4)' )
        return 'fail'

    tr = None

    # Test RPC_DEMINTERPOLATION=cubic

    tr = gdal.Transformer( ds, None, [ 'METHOD=RPC', 'RPC_HEIGHT_SCALE=2', 'RPC_DEM=/vsimem/dem.tif', 'RPC_DEMINTERPOLATION=cubic' ] )

    (success,pnt) = tr.TransformPoint( 0, 20, 10, 0 )

    if not success \
       or abs(pnt[0]-125.64828521533849) > 0.000001 \
       or abs(pnt[1]-39.869345204440144) > 0.000001 :
        print(success, pnt)
        gdaltest.post_reason( 'got wrong forward transform result.(4)' )
        return 'fail'

    (success,pnt) = tr.TransformPoint( 1, pnt[0], pnt[1], pnt[2] )

    if not success \
       or abs(pnt[0]-20) > 0.001 \
       or abs(pnt[1]-10) > 0.001 :
        print(success, pnt)
        gdaltest.post_reason( 'got wrong reverse transform result.(4)' )
        return 'fail'

    tr = None

    # Test RPC_DEMINTERPOLATION=near

    tr = gdal.Transformer( ds, None, [ 'METHOD=RPC', 'RPC_HEIGHT_SCALE=2', 'RPC_DEM=/vsimem/dem.tif', 'RPC_DEMINTERPOLATION=near' ] )

    (success,pnt) = tr.TransformPoint( 0, 20, 10, 0 )

    if not success \
       or abs(pnt[0]-125.64828521503811) > 0.000001 \
       or abs(pnt[1]-39.869345204874911) > 0.000001 :
        print(success, pnt)
        gdaltest.post_reason( 'got wrong forward transform result.(4)' )
        return 'fail'

    (success,pnt) = tr.TransformPoint( 1, pnt[0], pnt[1], pnt[2] )

    if not success \
       or abs(pnt[0]-20) > 0.001 \
       or abs(pnt[1]-10) > 0.001 :
        print(success, pnt)
        gdaltest.post_reason( 'got wrong reverse transform result.(4)' )
        return 'fail'

    tr = None
    
    # Test outside DEM extent : default behaviour --> error
    tr = gdal.Transformer( ds, None, [ 'METHOD=RPC', 'RPC_HEIGHT_SCALE=2', 'RPC_DEM=/vsimem/dem.tif' ] )

    (success,pnt) = tr.TransformPoint( 0, -100, 0, 0 )
    if success != 0:
        gdaltest.post_reason( 'fail' )
        return 'fail'

    (success,pnt) = tr.TransformPoint( 1, 125, 40, 0 )
    if success != 0:
        gdaltest.post_reason( 'fail' )
        return 'fail'

    tr = None

    # Test outside DEM extent with RPC_DEM_MISSING_VALUE=0
    tr = gdal.Transformer( ds, None, [ 'METHOD=RPC', 'RPC_HEIGHT_SCALE=2', 'RPC_DEM=/vsimem/dem.tif', 'RPC_DEM_MISSING_VALUE=0' ] )

    (success,pnt) = tr.TransformPoint( 0, -100, 0, 0 )
    if not success \
       or abs(pnt[0]-125.64746155942839) > 0.000001 \
       or abs(pnt[1]-39.869506789921168) > 0.000001 :
        print(success, pnt)
        gdaltest.post_reason( 'got wrong forward transform result.' )
        return 'fail'

    (success,pnt) = tr.TransformPoint( 1, pnt[0], pnt[1], pnt[2] )
    if not success \
       or abs(pnt[0]--100) > 0.001 \
       or abs(pnt[1]-0) > 0.001 :
        print(success, pnt)
        gdaltest.post_reason( 'got wrong reverse transform result.' )
        return 'fail'

    tr = None

    gdal.Unlink('/vsimem/dem.tif')

    return 'success' 


###############################################################################
# Test RPC convergence bug (bug # 5395)

def transformer_6():

    ds = gdal.Open('data/rpc_5395.vrt')
    tr = gdal.Transformer( ds, None, [ 'METHOD=RPC' ] )

    (success,pnt) = tr.TransformPoint( 0, 0, 0 )

    if not success \
       or abs(pnt[0]-28.26163232) > 0.0001 \
       or abs(pnt[1]--27.79853245) > 0.0001 \
       or pnt[2] != 0:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong forward transform result.' )
        return 'fail'

    return 'success'

###############################################################################
# Test Transformer.TransformPoints

def transformer_7():

    ds = gdal.Open('data/byte.tif')
    tr = gdal.Transformer( ds, None, [] )

    (pnt, success) = tr.TransformPoints( 0, [(20, 10)] )

    if success[0] == 0 \
       or abs(pnt[0][0]- 441920) > 0.00000001 \
       or abs(pnt[0][1]-3750720) > 0.00000001 \
       or pnt[0][2] != 0.0:
        print(success, pnt)
        gdaltest.post_reason( 'got wrong forward transform result.' )
        return 'fail'

    return 'success' 

###############################################################################
# Test handling of nodata in RPC DEM (#5680)

def transformer_8():

    ds = gdal.Open('data/rpc.vrt')

    # (long,lat)=(125.64828521533849 39.869345204440144) -> (Easting,Northing)=(213324.662167036 4418634.47813677) in EPSG:32652
    ds_dem = gdal.GetDriverByName('GTiff').Create('/vsimem/dem.tif', 100, 100, 1, gdal.GDT_Int16)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32652)
    ds_dem.SetProjection(sr.ExportToWkt())
    ds_dem.SetGeoTransform([213300,1,0,4418700,0,-1])
    ds_dem.GetRasterBand(1).SetNoDataValue(-32768)
    ds_dem.GetRasterBand(1).Fill(-32768)
    ds_dem = None

    for method in [ 'near', 'bilinear', 'cubic' ]:
        tr = gdal.Transformer( ds, None, [ 'METHOD=RPC', 'RPC_DEM=/vsimem/dem.tif', 'RPC_DEMINTERPOLATION=%s' % method ] )

        (success,pnt) = tr.TransformPoint( 0, 20, 10, 0 )

        if success:
            print(success, pnt)
            gdaltest.post_reason( 'got wrong forward transform result.' )
            return 'fail'

        (success,pnt) = tr.TransformPoint( 1, 125.64828521533849, 39.869345204440144, 0 )

        if success:
            print(success, pnt)
            gdaltest.post_reason( 'got wrong reverse transform result.' )
            return 'fail'

   
    gdal.Unlink('/vsimem/dem.tif')

    return 'success' 

###############################################################################
# Test RPC DEM line optimization

def transformer_9():

    ds = gdal.Open('data/rpc.vrt')

    # (long,lat)=(125.64828521533849 39.869345204440144) -> (Easting,Northing)=(213324.662167036 4418634.47813677) in EPSG:32652
    ds_dem = gdal.GetDriverByName('GTiff').Create('/vsimem/dem.tif', 100, 100, 1, gdal.GDT_Byte)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds_dem.SetProjection(sr.ExportToWkt())
    ds_dem.SetGeoTransform([125.647968621436,1.2111052640051412e-05,0,39.869926216038,0,-8.6569068979969188e-06])
    import random
    random.seed(0)
    data = ''.join([ chr(40 + int(10 * random.random()) ) for i in range(100*100) ])
    ds_dem.GetRasterBand(1).WriteRaster(0, 0, 100, 100, data)
    ds_dem = None

    for method in [ 'near', 'bilinear', 'cubic' ]:
        tr = gdal.Transformer( ds, None, [ 'METHOD=RPC', 'RPC_DEM=/vsimem/dem.tif', 'RPC_DEMINTERPOLATION=%s' % method ] )

        points = []
        for i in range(10):
            points.append((125.64828521533849, 39.869345204440144))
        (pnt, success) = tr.TransformPoints( 1, points )
        if not success[0]:
            gdaltest.post_reason( 'failure' )
            print(method)
            return 'fail'
        pnt_optimized = pnt[0]

        (success,pnt) = tr.TransformPoint( 1, 125.64828521533849, 39.869345204440144, 0 )
        if not success:
            gdaltest.post_reason( 'failure' )
            print(method)
            return 'fail'

        if pnt != pnt_optimized:
            gdaltest.post_reason( 'failure' )
            print(method)
            print(pnt)
            print(pnt_optimized)
            return 'fail'

   
    gdal.Unlink('/vsimem/dem.tif')

    return 'success' 

gdaltest_list = [
    transformer_1,
    transformer_2,
    transformer_3,
    transformer_4,
    transformer_5,
    transformer_6,
    transformer_7,
    transformer_8,
    transformer_9
    ]

disabled_gdaltest_list = [
    transformer_9
]

if __name__ == '__main__':

    gdaltest.setup_run( 'transformer' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

