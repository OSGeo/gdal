#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  WKTRaster Testing.
# Author:   Jorge Arevalo <jorge dot arevalo @ gis4free dot org>
# 
###############################################################################
# Copyright (c) 2009, Jorge Arevalo <jorge dot arevalo @ gis4free dot org>
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
try:
    from osgeo import gdal
except ImportError:
    import gdal

sys.path.append( '../pymod' )
sys.path.append( '../../wktraster/scripts' )

import gdaltest

#
# To initialize the required WKTRaster DB instance, run data/load_wktraster_test_data.sh
#

###############################################################################
# 
def wktraster_init():
    try:
        gdaltest.wktrasterDriver = gdal.GetDriverByName('WKTRaster')
    except:
        gdaltest.wktrasterDriver = None

    if gdaltest.wktrasterDriver is None:
        return 'skip'
        
    gdaltest.wktraster_connection_string="PG:host='localhost' dbname='gisdb' user='gis' password='gis' schema='gis_schema' "

    try:
        ds = gdal.Open( gdaltest.wktraster_connection_string + "table='utm'" )
    except:
        gdaltest.wktrasterDriver = None

    if ds is None:
        gdaltest.wktrasterDriver = None

    if gdaltest.wktrasterDriver is None:
        return 'skip'

    return 'success'
    

###############################################################################
# 
def wktraster_test_open_error1():
    if gdaltest.wktrasterDriver is None:
        return 'skip'

    ds = gdal.Open(gdaltest.wktraster_connection_string + "table='unexistent'")
    if ds is None:
        return 'success'
    else:
        return 'fail'
        
        
###############################################################################
# 
def wktraster_test_open_error2():
    if gdaltest.wktrasterDriver is None:
        return 'skip'

    ds = gdal.Open(gdaltest.wktraster_connection_string + "table='utm' mode='unexistent'")
    if ds is None:
        return 'success'
    else:
        return 'fail'        

    
###############################################################################
# 
def wktraster_compare_utm():
    if gdaltest.wktrasterDriver is None:
        return 'skip'
        
    src_ds = gdal.Open( 'data/utm.tif' )
    dst_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='utm'" )
    
    diff = gdaltest.compare_ds(src_ds, dst_ds, verbose = 1)
    if diff == 0:
        return 'success'
    else:
        return 'fail'
        
###############################################################################
# 
def wktraster_compare_small_world():
    if gdaltest.wktrasterDriver is None:
        return 'skip'
        
    src_ds = gdal.Open( 'data/small_world.tif' )
    dst_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='small_world'" )
    
    diff = gdaltest.compare_ds(src_ds, dst_ds, verbose = 1)
    if diff == 0:
        return 'success'
    else:
        return 'fail'        
        
        
###############################################################################
# 
def wktraster_test_utm_open():    
    if gdaltest.wktrasterDriver is None:
        return 'skip'    
        
    # First open tif file
    src_ds = gdal.Open( 'data/utm.tif' )
    prj = src_ds.GetProjectionRef()   
    gt = src_ds.GetGeoTransform() 
    
    # Get band data
    rb = src_ds.GetRasterBand(1)
    st = rb.GetStatistics(0, 1)
    cs = rb.Checksum()
    
    # Try to open WKTRaster with the same data than original tif file
    tst = gdaltest.GDALTest('WKTRaster', gdaltest.wktraster_connection_string + "table='utm'", 1, cs, filename_absolute = 1)
    return tst.testOpen(check_prj = prj, check_gt = gt)
    
    
    
###############################################################################
# 
def wktraster_test_small_world_open_b1():    
    if gdaltest.wktrasterDriver is None:
        return 'skip'    
        
    # First open tif file
    src_ds = gdal.Open( 'data/small_world.tif' )    
    prj = src_ds.GetProjectionRef()   
    gt = src_ds.GetGeoTransform() 
    
    # Get band data
    rb = src_ds.GetRasterBand(1)
    st = rb.GetStatistics(0, 1)
    cs = rb.Checksum()
    
    # Try to open WKTRaster with the same data than original tif file
    tst = gdaltest.GDALTest('WKTRaster', gdaltest.wktraster_connection_string + "table='small_world'", 1, cs, filename_absolute = 1)    
    return tst.testOpen(check_prj = prj, check_gt = gt)    
    
    
###############################################################################
# 
def wktraster_test_small_world_open_b2():    
    if gdaltest.wktrasterDriver is None:
        return 'skip'    
        
    # First open tif file
    src_ds = gdal.Open( 'data/small_world.tif' )    
    prj = src_ds.GetProjectionRef()   
    gt = src_ds.GetGeoTransform() 
    
    # Get band data
    rb = src_ds.GetRasterBand(2)
    st = rb.GetStatistics(0, 1)
    cs = rb.Checksum()
    
    # Try to open WKTRaster with the same data than original tif file
    tst = gdaltest.GDALTest('WKTRaster', gdaltest.wktraster_connection_string + "table='small_world'", 2, cs, filename_absolute = 1)    
    return tst.testOpen(check_prj = prj, check_gt = gt)
    
    
###############################################################################
# 
def wktraster_test_small_world_open_b3():    
    if gdaltest.wktrasterDriver is None:
        return 'skip'    
        
    # First open tif file
    src_ds = gdal.Open( 'data/small_world.tif' )
    prj = src_ds.GetProjectionRef()   
    gt = src_ds.GetGeoTransform() 
    
    # Get band data
    rb = src_ds.GetRasterBand(3)
    st = rb.GetStatistics(0, 1)
    cs = rb.Checksum()
    
    # Checksum for each band can be obtained by gdalinfo -checksum <file>
    tst = gdaltest.GDALTest('WKTRaster', gdaltest.wktraster_connection_string + "table='small_world'", 3, cs, filename_absolute = 1)
    
    return tst.testOpen(check_prj = prj, check_gt = gt)        



gdaltest_list = [
    wktraster_init,
    wktraster_test_open_error1,
    wktraster_test_open_error2,
    wktraster_compare_utm,
    wktraster_compare_small_world,
    wktraster_test_utm_open,
    wktraster_test_small_world_open_b1,
    wktraster_test_small_world_open_b2,
    wktraster_test_small_world_open_b3]

if __name__ == '__main__':

    gdaltest.setup_run( 'WKTRASTER' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
