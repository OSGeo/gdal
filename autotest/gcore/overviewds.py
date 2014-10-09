#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GDALOverviewDataset
# Author:   Even Rouault <even dot rouault at mines spatialys dot com>
# 
###############################################################################
# Copyright (c) 2014 Even Rouault <even dot rouault at mines spatialys dot com>
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
import shutil

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal

###############################################################################
# Error cases

def overviewds_1():
    ds = gdal.OpenEx('data/byte.tif', open_options = ['OVERVIEW_LEVEL=-1'])
    if ds is not None:
        return 'fail'
    ds = gdal.OpenEx('data/byte.tif', open_options = ['OVERVIEW_LEVEL=0'])
    if ds is not None:
        return 'fail'

    return 'success'

###############################################################################
# Nominal cases

def overviewds_2():
    
    shutil.copy('data/byte.tif', 'tmp')
    ds = gdal.Open('tmp/byte.tif')
    ds.BuildOverviews( 'NEAR', overviewlist = [2, 4] )
    ds = None

    src_ds = gdal.Open('tmp/byte.tif')
    ds = gdal.OpenEx('tmp/byte.tif', open_options = ['OVERVIEW_LEVEL=0'])
    if ds is None:
        return 'fail'
    if ds.RasterXSize != 10 or ds.RasterYSize != 10 or ds.RasterCount != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetProjectionRef() != src_ds.GetProjectionRef():
        gdaltest.post_reason('fail')
        return 'fail'
    src_gt = src_ds.GetGeoTransform()
    expected_gt = ( src_gt[0], src_gt[1] * 2, src_gt[2], src_gt[3], src_gt[4], src_gt[5] * 2 )
    gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(expected_gt[i] - gt[i]) > 1e-5:
            gdaltest.post_reason('fail')
            print(expected_gt)
            print(gt)
            return 'fail'
    if ds.GetGCPCount() != 0 or ds.GetGCPProjection() != src_ds.GetGCPProjection() or len(ds.GetGCPs()) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    expected_data = src_ds.ReadRaster(0,0,20,20,10,10)
    got_data = ds.ReadRaster(0,0,10,10)
    if expected_data != got_data:
        gdaltest.post_reason('fail')
        return 'fail'
    got_data = ds.GetRasterBand(1).ReadRaster(0,0,10,10)
    if expected_data != got_data:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    expected_data = src_ds.ReadRaster(0,0,20,20,5,5)
    got_data = ds.GetRasterBand(1).GetOverview(0).ReadRaster(0,0,5,5)
    if expected_data != got_data:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetMetadata() != src_ds.GetMetadata():
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetMetadataItem('AREA_OR_POINT') != src_ds.GetMetadataItem('AREA_OR_POINT'):
        gdaltest.post_reason('fail')
        return 'fail'
    if len(ds.GetMetadata('RPC')) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if len(ds.GetMetadata('GEOLOCATION')) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetMetadataItem('RPC', 'FOO') != None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test GCP

def overviewds_3():

    src_ds = gdal.Open('tmp/byte.tif')
    ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/byte.tif', src_ds)
    ds.SetGeoTransform([0,1,0,0,0,1]) # cancel geotransform
    gcp1 = gdal.GCP()
    gcp1.GCPPixel = 0
    gcp1.GCPLine = 0
    gcp1.GCPX = 440720.000
    gcp1.GCPY = 3751320.000
    gcp2 = gdal.GCP()
    gcp2.GCPPixel = 0
    gcp2.GCPLine = 20
    gcp2.GCPX = 440720.000
    gcp2.GCPY = 3750120.000
    gcp3 = gdal.GCP()
    gcp3.GCPPixel = 20
    gcp3.GCPLine = 0
    gcp3.GCPX = 441920.000
    gcp3.GCPY = 3751320.000
    src_gcps = (gcp1, gcp2, gcp3)
    ds.SetGCPs(src_gcps, src_ds.GetProjectionRef())
    
    tr = gdal.Transformer( ds, None, [ 'METHOD=GCP_POLYNOMIAL' ] )
    (ref_success,ref_pnt) = tr.TransformPoint( 0, 20, 10 )

    ds.BuildOverviews( 'NEAR', overviewlist = [2, 4] )
    ds = None

    ds = gdal.OpenEx('tmp/byte.tif', open_options = ['OVERVIEW_LEVEL=0'])
    gcps = ds.GetGCPs()
    for i in range(3):
        if gcps[i].GCPPixel != src_gcps[i].GCPPixel / 2 or gcps[i].GCPLine != src_gcps[i].GCPLine / 2 or \
           gcps[i].GCPX != src_gcps[i].GCPX or gcps[i].GCPY != src_gcps[i].GCPY:
            gdaltest.post_reason('fail')
            return 'fail'

    # Really check that the transformer works
    tr = gdal.Transformer( ds, None, [ 'METHOD=GCP_POLYNOMIAL' ] )
    (success,pnt) = tr.TransformPoint( 0, 20 / 2.0, 10 / 2.0 )
    
    for i in range(3):
        if abs(ref_pnt[i] - pnt[i]) > 1e-5:
            gdaltest.post_reason('fail')
            print(ref_pnt)
            print(pnt)
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test RPC

def myfloat(s):
    p = s.rfind(' ')
    if p >= 0:
        s = s[0:p]
    return float(s)

def overviewds_4():

    shutil.copy('data/byte.tif', 'tmp/byte.tif')
    shutil.copy('data/test_rpc.txt', 'tmp/byte_rpc.txt')
    ds = gdal.Open('tmp/byte.tif')
    rpc_md = ds.GetMetadata('RPC')
    
    tr = gdal.Transformer( ds, None, [ 'METHOD=RPC' ] )
    (ref_success,ref_pnt) = tr.TransformPoint( 0, 20, 10 )

    ds.BuildOverviews( 'NEAR', overviewlist = [2, 4] )
    ds = None

    ds = gdal.OpenEx('tmp/byte.tif', open_options = ['OVERVIEW_LEVEL=0'])
    got_md = ds.GetMetadata('RPC')
    
    for key in rpc_md:
        if ds.GetMetadataItem(key, 'RPC') != got_md[key]:
            gdaltest.post_reason('fail')
            return 'fail'
        if key == 'LINE_SCALE' or key == 'SAMP_SCALE' or key == 'LINE_OFF' or key == 'SAMP_OFF':
            if float(got_md[key]) != myfloat(rpc_md[key]) / 2:
                gdaltest.post_reason('fail')
                print(key)
                print(got_md[key])
                print(rpc_md[key])
                return 'fail'
        elif got_md[key] != rpc_md[key]:
            gdaltest.post_reason('fail')
            print(key)
            print(got_md[key])
            print(rpc_md[key])
            return 'fail'

    # Really check that the transformer works
    tr = gdal.Transformer( ds, None, [ 'METHOD=RPC' ] )
    (success,pnt) = tr.TransformPoint( 0, 20 / 2.0, 10 / 2.0 )
    
    for i in range(3):
        if abs(ref_pnt[i] - pnt[i]) > 1e-5:
            gdaltest.post_reason('fail')
            print(ref_pnt)
            print(pnt)
            return 'fail'

    ds = None
    
    try:
        os.remove('tmp/byte_rpc.txt')
    except:
        pass

    return 'success'

###############################################################################
# Test GEOLOCATION

def overviewds_5():
    
    shutil.copy('data/sstgeo.tif', 'tmp/sstgeo.tif')
    shutil.copy('data/sstgeo.vrt', 'tmp/sstgeo.vrt')
    
    ds = gdal.Open('tmp/sstgeo.vrt')
    geoloc_md = ds.GetMetadata('GEOLOCATION')
    
    tr = gdal.Transformer( ds, None, [ 'METHOD=GEOLOC_ARRAY' ] )
    (ref_success,ref_pnt) = tr.TransformPoint( 0, 20, 10 )

    ds.BuildOverviews( 'NEAR', overviewlist = [2, 4] )
    ds = None

    ds = gdal.OpenEx('tmp/sstgeo.vrt', open_options = ['OVERVIEW_LEVEL=0'])
    got_md = ds.GetMetadata('GEOLOCATION')
    
    for key in geoloc_md:
        if ds.GetMetadataItem(key, 'GEOLOCATION') != got_md[key]:
            gdaltest.post_reason('fail')
            return 'fail'
        if key == 'PIXEL_OFFSET' or key == 'LINE_OFFSET':
            if abs(float(got_md[key]) - myfloat(geoloc_md[key]) * 2) > 1e-1:
                gdaltest.post_reason('fail')
                print(key)
                print(got_md[key])
                print(geoloc_md[key])
                return 'fail'
        elif key == 'PIXEL_STEP' or key == 'LINE_STEP':
            if abs(float(got_md[key]) - myfloat(geoloc_md[key]) / 2) > 1e-1:
                gdaltest.post_reason('fail')
                print(key)
                print(got_md[key])
                print(geoloc_md[key])
                return 'fail'
        elif got_md[key] != geoloc_md[key]:
            gdaltest.post_reason('fail')
            print(key)
            print(got_md[key])
            print(geoloc_md[key])
            return 'fail'

    # Really check that the transformer works
    tr = gdal.Transformer( ds, None, [ 'METHOD=GEOLOC_ARRAY' ] )
    expected_xyz = ( 20.0 / 2, 10.0 / 2, 0 )
    (success,pnt) = tr.TransformPoint( 1, ref_pnt[0], ref_pnt[1] )
    
    for i in range(3):
        if abs(pnt[i] - expected_xyz[i]) > 0.5:
            gdaltest.post_reason('fail')
            print(pnt)
            print(expected_xyz)
            return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test VRT

def overviewds_6():
    
    shutil.copy('data/byte.tif', 'tmp')
    ds = gdal.Open('tmp/byte.tif')
    ds.BuildOverviews( 'NEAR', overviewlist = [2, 4] )
    ds = None

    src_ds = gdal.OpenEx('tmp/byte.tif', open_options = ['OVERVIEW_LEVEL=0'])
    expected_cs = src_ds.GetRasterBand(1).Checksum()
    ds = gdal.GetDriverByName('VRT').CreateCopy('tmp/byte.vrt', src_ds)
    ds = None
    src_ds = None

    ds = gdal.Open('tmp/byte.vrt')
    if ds.RasterXSize != 10 or ds.RasterYSize != 10 or ds.RasterCount != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    got_cs = ds.GetRasterBand(1).Checksum()
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Cleanup

def overviewds_cleanup():

    gdal.GetDriverByName('GTiff').Delete('tmp/byte.tif')
    try:
        os.remove('tmp/byte_rpc.txt')
    except:
        pass
    try:
        os.remove('tmp/sstgeo.tif')
        os.remove('tmp/sstgeo.vrt')
        os.remove('tmp/sstgeo.vrt.ovr')
    except:
        pass
    try:
        os.remove('tmp/byte.vrt')
    except:
        pass

    return 'success'


gdaltest_list = [ overviewds_1,
                  overviewds_2,
                  overviewds_3,
                  overviewds_4,
                  overviewds_5,
                  overviewds_6,
                  overviewds_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'overviewds' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

