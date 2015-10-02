#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test librarified gdalwarp
# Author:   Faza Mahamood <fazamhd @ gmail dot com>
# 
###############################################################################
# Copyright (c) 2015, Faza Mahamood <fazamhd at gmail dot com>
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

sys.path.append( '../pymod' )

from osgeo import gdal
import gdaltest

###############################################################################
# Simple test

def test_gdalwarp_lib_1():

    ds1 = gdal.Open('../gcore/data/byte.tif')
    dstDS = gdal.Warp('tmp/testgdalwarp1.tif', ds1)

    if dstDS.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    dstDS = None

    return 'success'


###############################################################################
# Test -of option

def test_gdalwarp_lib_2():

    ds1 = gdal.Open('../gcore/data/byte.tif')
    dstDS = gdal.Warp('tmp/testgdalwarp2.tif',[ds1], format = 'GTiff')

    if dstDS.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    dstDS = None

    return 'success'


###############################################################################
# Test -ot option

def test_gdalwarp_lib_3():

    ds1 = gdal.Open('../gcore/data/byte.tif')
    dstDS = gdal.Warp('tmp/testgdalwarp3.tif', ds1, outputType = gdal.GDT_Int16)
    
    if dstDS.GetRasterBand(1).DataType != gdal.GDT_Int16:
        gdaltest.post_reason('Bad data type')
        return 'fail'

    if dstDS.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    dstDS = None

    return 'success'

###############################################################################
# Test -t_srs option

def test_gdalwarp_lib_4():
    
    ds1 = gdal.Open('../gcore/data/byte.tif')
    dstDS = gdal.Warp('tmp/testgdalwarp4.tif', ds1, dstSRS = 'EPSG:32611')

    if dstDS.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    dstDS = None

    return 'success'

###############################################################################
# Test warping from GCPs without any explicit option

def test_gdalwarp_lib_5():

    ds = gdal.Open('../gcore/data/byte.tif')
    gcpList = [gdal.GCP(440720.000,3751320.000,0,0,0), gdal.GCP(441920.000,3751320.000,0,20,0), gdal.GCP(441920.000,3750120.000,0,20,20), gdal.GCP(440720.000,3750120.000,0,0,20)]
    ds1 = gdal.Translate('tmp/testgdalwarp_gcp.tif',ds,outputSRS = 'EPSG:26711',GCPs = gcpList)
    dstDS = gdal.Warp('tmp/testgdalwarp5.tif', ds1, tps = True)

    if dstDS.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if not gdaltest.geotransform_equals(ds.GetGeoTransform(), dstDS.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    dstDS = None

    return 'success'


###############################################################################
# Test warping from GCPs with -tps

def test_gdalwarp_lib_6():
    
    ds1 = gdal.Open('tmp/testgdalwarp_gcp.tif')
    dstDS = gdal.Warp('tmp/testgdalwarp6.tif',ds1, tps = True)

    if dstDS.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    if not gdaltest.geotransform_equals(gdal.Open('../gcore/data/byte.tif').GetGeoTransform(), dstDS.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    dstDS = None

    return 'success'


###############################################################################
# Test -tr

def test_gdalwarp_lib_7():
    
    ds1 = gdal.Open('tmp/testgdalwarp_gcp.tif')
    dstDS = gdal.Warp('tmp/testgdalwarp7.tif',[ds1],xRes = 120,yRes = 120)
    if dstDS is None:
        return 'fail'

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    if not gdaltest.geotransform_equals(expected_gt, dstDS.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    dstDS = None

    return 'success'

###############################################################################
# Test -ts

def test_gdalwarp_lib_8():
    
    ds1 = gdal.Open('tmp/testgdalwarp_gcp.tif')
    dstDS = gdal.Warp('tmp/testgdalwarp8.tif',[ds1],oXSizePixel = 10,oYSizePixel = 10)
    if dstDS is None:
        return 'fail'

    expected_gt = (440720.0, 120.0, 0.0, 3751320.0, 0.0, -120.0)
    if not gdaltest.geotransform_equals(expected_gt, dstDS.GetGeoTransform(), 1e-9) :
        gdaltest.post_reason('Bad geotransform')
        return 'fail'

    dstDS = None

    return 'success'

###############################################################################
# Cleanup

def test_gdalwarp_lib_cleanup():

    # We don't clean up when run in debug mode.
    if gdal.GetConfigOption( 'CPL_DEBUG', 'OFF' ) == 'ON':
        return 'success'
    
    for i in range(8):
        try:
            os.remove('tmp/testgdalwarp' + str(i+1) + '.tif')
        except:
            pass
        try:
            os.remove('tmp/testgdalwarp' + str(i+1) + '.vrt')
        except:
            pass
        try:
            os.remove('tmp/testgdalwarp' + str(i+1) + '.tif.aux.xml')
        except:
            pass
    try:
        os.remove('tmp/testgdalwarp_gcp.tif')
    except:
        pass
    
    return 'success'

gdaltest_list = [
    test_gdalwarp_lib_cleanup,
    test_gdalwarp_lib_1,
    test_gdalwarp_lib_2,
    test_gdalwarp_lib_3,
    test_gdalwarp_lib_4,
    test_gdalwarp_lib_5,
    test_gdalwarp_lib_6,
    test_gdalwarp_lib_7,
    test_gdalwarp_lib_8,
    test_gdalwarp_lib_cleanup,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdalwarp_lib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
