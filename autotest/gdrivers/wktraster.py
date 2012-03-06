#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  PostGISRaster Testing.
# Author:   Jorge Arevalo <jorge.arevalo@libregis.org>
#           David Zwarg <dzwarg@azavea.com>
# 
###############################################################################
# Copyright (c) 2009, Jorge Arevalo <jorge.arevalo@libregis.org>
#               2012, David Zwarg <dzwarg@azavea.com>
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
# To initialize the required PostGISRaster DB instance, run data/load_wktraster_test_data.sh
#

###############################################################################
# 
def wktraster_init():
    try:
        gdaltest.wktrasterDriver = gdal.GetDriverByName('PostGISRaster')
    except Exception, ex:
        gdaltest.wktrasterDriver = None

    if gdaltest.wktrasterDriver is None:
        return 'skip'
        
    gdaltest.wktraster_connection_string="PG:host='localhost' dbname='gisdb' user='gis' password='gis' schema='gis_schema' "

    try:
        ds = gdal.Open( gdaltest.wktraster_connection_string + "table='utm'" )
    except Exception, ex:
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

    # removed mode, as it defaults to one raster per row
    ds = gdal.Open(gdaltest.wktraster_connection_string + "table='utm'")
    if ds is None:
        return 'fail'
    else:
        return 'success'        

    
###############################################################################
# 
def wktraster_compare_utm():
    if gdaltest.wktrasterDriver is None:
        return 'skip'
        
    src_ds = gdal.Open( 'data/utm.tif' )
    dst_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='utm'" )

    # dataset actually contains many sub-datasets. test the first one
    dst_ds = gdal.Open( dst_ds.GetMetadata('SUBDATASETS')['SUBDATASET_1_NAME'] )
    
    diff = gdaltest.compare_ds(src_ds, dst_ds, width=100, height=100, verbose = 1)
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
 
    # dataset actually contains many sub-datasets. test the first one
    dst_ds = gdal.Open( dst_ds.GetMetadata('SUBDATASETS')['SUBDATASET_1_NAME'] )
    
    diff = gdaltest.compare_ds(src_ds, dst_ds, width=40, height=20, verbose = 1)
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

    main_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='utm'" )

    # Try to open PostGISRaster with the same data than original tif file
    tst = gdaltest.GDALTest('PostGISRaster', main_ds.GetMetadata('SUBDATASETS')['SUBDATASET_1_NAME'], 1, cs, filename_absolute = 1)
    return tst.testOpen(check_prj = prj, check_gt = gt, skip_checksum = True)
    
    
    
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
    
    
    main_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='small_world'" )

    # Try to open PostGISRaster with the same data than original tif file
    tst = gdaltest.GDALTest('PostGISRaster', main_ds.GetMetadata('SUBDATASETS')['SUBDATASET_1_NAME'], 1, cs, filename_absolute = 1)    
    return tst.testOpen(check_prj = prj, check_gt = gt, skip_checksum = True)    
    
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
    
    
    main_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='small_world'" )

    # Try to open PostGISRaster with the same data than original tif file
    tst = gdaltest.GDALTest('PostGISRaster', main_ds.GetMetadata('SUBDATASETS')['SUBDATASET_1_NAME'], 2, cs, filename_absolute = 1)    
    return tst.testOpen(check_prj = prj, check_gt = gt, skip_checksum = True)
    
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

    main_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='small_world'" )

    
    main_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='small_world'" )

    # Checksum for each band can be obtained by gdalinfo -checksum <file>
    tst = gdaltest.GDALTest('PostGISRaster', main_ds.GetMetadata('SUBDATASETS')['SUBDATASET_1_NAME'], 3, cs, filename_absolute = 1)

    return tst.testOpen(check_prj = prj, check_gt = gt, skip_checksum = True)

def wktraster_test_create_copy_bad_conn_string():
    if gdaltest.wktrasterDriver is None:
        return 'skip'

    src_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='small_world'" )

    new_ds = gdaltest.wktrasterDriver.CreateCopy( "bogus connection string", src_ds, strict = True )

    if new_ds is None:
        return 'success'
    else:
        return 'fail'

def wktraster_test_create_copy_no_dbname():
    if gdaltest.wktrasterDriver is None:
        return 'skip'

    src_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='small_world'" )

    # This is set in order to prevent GDAL from attempting to auto-identify
    # a bogus PG: filename to the postgis raster driver
    options = ['APPEND_SUBDATASET=YES']

    new_ds = gdaltest.wktrasterDriver.CreateCopy( "PG: no database name", src_ds, strict = True, options = options )

    if new_ds is None:
        return 'success'
    else:
        return 'fail'

def wktraster_test_create_copy_no_tablename():
    if gdaltest.wktrasterDriver is None:
        return 'skip'

    src_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='small_world'" )

    # This is set in order to prevent GDAL from attempting to auto-identify
    # a bogus PG: filename to the postgis raster driver
    options = ['APPEND_SUBDATASET=YES']

    new_ds = gdaltest.wktrasterDriver.CreateCopy( gdaltest.wktraster_connection_string, src_ds, strict = True, options = options )

    if new_ds is None:
        return 'success'
    else:
        return 'fail'

def wktraster_test_create_copy_and_delete():
    """
    Test the "CreateCopy" implementation. What to do when we're done?
    Why, test "Delete", of course!
    """
    if gdaltest.wktrasterDriver is None:
        return 'skip'

    src_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='small_world'" )

    new_ds = gdaltest.wktrasterDriver.CreateCopy( gdaltest.wktraster_connection_string + "table='small_world_copy'", src_ds, strict = True )

    if new_ds is None:
        return 'fail'

    deleted = gdaltest.wktrasterDriver.Delete( gdaltest.wktraster_connection_string + "table='small_world_copy'")

    if deleted:
        return 'fail'
    else:
        return 'success'

def wktraster_test_create_copy_and_delete_phases():
    """
    Create a copy of the dataset, then delete it in phases.
    """
    if gdaltest.wktrasterDriver is None:
        return 'skip'

    src_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='small_world'" )

    src_md = src_ds.GetMetadata('SUBDATASETS').keys()

    new_ds = gdaltest.wktrasterDriver.CreateCopy( gdaltest.wktraster_connection_string + "table='small_world_copy'", src_ds, strict = True )

    new_md = new_ds.GetMetadata('SUBDATASETS').keys()

    # done with src
    src_ds = None

    if new_ds is None:
        gdaltest.post_reason( 'No new dataset was created during copy.' )
        return 'fail'
    elif len(src_md) != len(new_md):
        gdaltest.post_reason( 'Metadata differs between new and old rasters.' )
        return 'fail'

    # should delete all raster parts over 50
    deleted = gdaltest.wktrasterDriver.Delete( gdaltest.wktraster_connection_string + "table='small_world_copy' where='rid>50'")

    if deleted:
        gdaltest.post_reason( 'Delete returned an error.' )
        return 'fail'

    src_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='small_world_copy'")

    src_md = src_ds.GetMetadata('SUBDATASETS').keys()

    if src_ds is None:
        gdaltest.post_reason( 'Could not open reduced dataset (1).' )
        return 'fail'
    elif len(src_md) != 100:
        # The length of the metadata contains two pcs of 
        # information per raster, so 50 rasters remaining = 100 keys
        gdaltest.post_reason( 'Expected 100 keys of metadata for 50 subadataset rasters.' )
        print len(src_md)
        return 'fail'

    # done with src
    src_ds = None

    deleted = gdaltest.wktrasterDriver.Delete( gdaltest.wktraster_connection_string + "table='small_world_copy' where='rid<=25'")

    if deleted:
        gdaltest.post_reason( 'Delete returned an error.' )
        return 'fail'

    src_ds = gdal.Open( gdaltest.wktraster_connection_string + "table='small_world_copy'")

    src_md = src_ds.GetMetadata('SUBDATASETS').keys()

    if src_ds is None:
        gdaltest.post_reason( 'Could not open reduced dataset (2).' )
        return 'fail'
    elif len(src_md) != 50:
        # The length of the metadata contains two pcs of 
        # information per raster, so 25 rasters remaining = 50 keys
        gdaltest.post_reason( 'Expected 50 keys of metadata for 25 subdataset rasters.' )
        print len(src_md)
        return 'fail'

    # done with src
    src_ds = None

    deleted = gdaltest.wktrasterDriver.Delete( gdaltest.wktraster_connection_string + "table='small_world_copy'")

    if deleted:
        gdaltest.post_reason( 'Delete returned an error.' )
        return 'fail'

    return 'success'


gdaltest_list = [
    wktraster_init,
    wktraster_test_open_error1,
    wktraster_test_open_error2,
    wktraster_compare_utm,
    wktraster_compare_small_world,
    wktraster_test_utm_open,
    wktraster_test_small_world_open_b1,
    wktraster_test_small_world_open_b2,
    wktraster_test_small_world_open_b3,
    wktraster_test_create_copy_bad_conn_string,
    wktraster_test_create_copy_no_dbname,
    wktraster_test_create_copy_no_tablename,
    wktraster_test_create_copy_and_delete,
    wktraster_test_create_copy_and_delete_phases]

if __name__ == '__main__':

    gdaltest.setup_run( 'WKTRASTER' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
