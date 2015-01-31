#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GeoRaster Testing.
# Author:   Ivan Lucena <ivan.lucena@pmldnet.com>
# 
###############################################################################
# Copyright (c) 2008, Ivan Lucena <ivan.lucena@pmldnet.com>
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
from osgeo import gdal
from osgeo import ogr
import string

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# 
def get_connection_str():

    oci_dsname = os.environ.get('OCI_DSNAME')

    if oci_dsname is None:
        return '<error: informe ORACLE connection>'
    else:
        return 'geor:' + oci_dsname.split(':')[1]

###############################################################################
# 
def georaster_init():

    gdaltest.oci_ds = None
    
    try:
        gdaltest.georasterDriver = gdal.GetDriverByName('GeoRaster')
    except:
        gdaltest.georasterDriver = None

    if gdaltest.georasterDriver is None:
        return 'skip'

    if os.environ.get('OCI_DSNAME') is None:
        return 'skip'
    
    gdaltest.oci_ds = ogr.Open( os.environ.get('OCI_DSNAME') )

    if gdaltest.oci_ds is None:
        return 'skip'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    rs = gdaltest.oci_ds.ExecuteSQL( 'select owner from all_sdo_geor_sysdata' )
    gdal.PopErrorHandler()
    
    err_msg = gdal.GetLastErrorMsg()
    
    if rs is not None:
        gdaltest.oci_ds.ReleaseResultSet(rs)
        rs = None

    if err_msg != '':
        gdaltest.post_reason( 'ALL_SDO_GEOR_SYSDATA inaccesable, likely georaster unavailable.' )

        gdaltest.oci_ds = None
        return 'skip'

    return 'success'

###############################################################################
# 

def georaster_byte():

    if gdaltest.georasterDriver is None:
        return 'skip'
        
    if gdaltest.oci_ds is None:
        return 'skip'

    ds_src = gdal.Open('data/byte.tif')

    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST,RASTER', ds_src, 1, 
        [ "DESCRIPTION=(id number, raster sdo_georaster)" , 
        "INSERT=(1001, sdo_geor.init('GDAL_TEST_RDT',1001))" ] ) 

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 4672, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_int16():

    if gdaltest.georasterDriver is None:
        return 'skip'
        
    if gdaltest.oci_ds is None:
        return 'skip'

    get_connection_str()

    ds_src = gdal.Open('data/int16.tif')

    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST,RASTER', ds_src, 1, 
        [ "DESCRIPTION=(id number, raster sdo_georaster)" , 
        "INSERT=(1002, sdo_geor.init('GDAL_TEST_RDT',1002))" ] ) 

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 4672, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_int32():

    if gdaltest.georasterDriver is None:
        return 'skip'
        
    if gdaltest.oci_ds is None:
        return 'skip'

    get_connection_str()

    ds_src = gdal.Open('data/int32.tif')

    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST,RASTER', ds_src, 1,
        [ "DESCRIPTION=(id number, raster sdo_georaster)" ,
        "INSERT=(1003, sdo_geor.init('GDAL_TEST_RDT',1003))" ] )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 4672, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_rgb_b1():

    if gdaltest.georasterDriver is None:
        return 'skip'
        
    if gdaltest.oci_ds is None:
        return 'skip'

    ds_src = gdal.Open('data/rgbsmall.tif')

    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST,RASTER', ds_src, 1,
        [ "DESCRIPTION=(id number, raster sdo_georaster)" ,
        "INSERT=(1004, sdo_geor.init('GDAL_TEST_RDT',1004))",
        "BLOCKBSIZE=1" ] )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 21212, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_rgb_b2():

    if gdaltest.georasterDriver is None:
        return 'skip'
        
    if gdaltest.oci_ds is None:
        return 'skip'

    ds_src = gdal.Open('data/rgbsmall.tif')

    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST,RASTER', ds_src, 1,
        [ "DESCRIPTION=(id number, raster sdo_georaster)" ,
        "INSERT=(1005, sdo_geor.init('GDAL_TEST_RDT',1005))",
        "BLOCKBSIZE=2" ] )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 21212, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_rgb_b3_bsq():

    if gdaltest.georasterDriver is None:
        return 'skip'
        
    if gdaltest.oci_ds is None:
        return 'skip'
    
    ds_src = gdal.Open('data/rgbsmall.tif')
    
    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST,RASTER', ds_src, 1,
        [ "DESCRIPTION=(id number, raster sdo_georaster)" ,
        "INSERT=(1006, sdo_geor.init('GDAL_TEST_RDT',1006))",
        "BLOCKBSIZE=3", "INTERLEAVE=BSQ" ] )
    
    ds_name = ds.GetDescription()
        
    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 21212, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_rgb_b3_bip():

    if gdaltest.georasterDriver is None:
        return 'skip'
        
    if gdaltest.oci_ds is None:
        return 'skip'
    
    ds_src = gdal.Open('data/rgbsmall.tif')
    
    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST,RASTER', ds_src, 1,
        [ "DESCRIPTION=(id number, raster sdo_georaster)" ,
        "INSERT=(1007, sdo_geor.init('GDAL_TEST_RDT',1007))",
        "BLOCKBSIZE=3", "INTERLEAVE=BIP" ] )
    
    ds_name = ds.GetDescription()
        
    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 21212, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_rgb_b3_bil():

    if gdaltest.georasterDriver is None:
        return 'skip'
        
    if gdaltest.oci_ds is None:
        return 'skip'
    
    ds_src = gdal.Open('data/rgbsmall.tif')
    
    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST,RASTER', ds_src, 1,
        [ "DESCRIPTION=(id number, raster sdo_georaster)" ,
        "INSERT=(1008, sdo_geor.init('GDAL_TEST_RDT',1008))",
        "BLOCKBSIZE=3", "INTERLEAVE=BIL" ] )
    
    ds_name = ds.GetDescription()
        
    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 21212, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_byte_deflate():

    if gdaltest.georasterDriver is None:
        return 'skip'
        
    if gdaltest.oci_ds is None:
        return 'skip'

    ds_src = gdal.Open('data/byte.tif')

    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST,RASTER', ds_src, 1,
        [ "DESCRIPTION=(id number, raster sdo_georaster)" ,
        "INSERT=(1009, sdo_geor.init('GDAL_TEST_RDT',1009))",
        "COMPRESS=DEFLATE" ] )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 4672, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_rgb_deflate_b3():

    if gdaltest.georasterDriver is None:
        return 'skip'
        
    if gdaltest.oci_ds is None:
        return 'skip'

    ds_src = gdal.Open('data/rgbsmall.tif')

    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST,RASTER', ds_src, 1,
        [ "DESCRIPTION=(id number, raster sdo_georaster)" ,
        "INSERT=(1010, sdo_geor.init('GDAL_TEST_RDT',1010))",
        "COMPRESS=DEFLATE", "BLOCKBSIZE=3", "INTERLEAVE=PIXEL" ] )

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 21212, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_1bit():

    if gdaltest.georasterDriver is None:
        return 'skip'
        
    if gdaltest.oci_ds is None:
        return 'skip'

    ds_src = gdal.Open('data/byte.tif')

    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST,RASTER', ds_src, 1, 
        [ "DESCRIPTION=(id number, raster sdo_georaster)" , 
        "INSERT=(1011, sdo_geor.init('GDAL_TEST_RDT',1011))" , 
        "NBITS=1"] ) 

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 252, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_2bit():

    if gdaltest.georasterDriver is None:
        return 'skip'
        
    if gdaltest.oci_ds is None:
        return 'skip'

    ds_src = gdal.Open('data/byte.tif')

    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST,RASTER', ds_src, 1, 
        [ "DESCRIPTION=(id number, raster sdo_georaster)" , 
        "INSERT=(1012, sdo_geor.init('GDAL_TEST_RDT',1012))" , 
        "NBITS=2"] ) 

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 718, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_4bit():

    if gdaltest.georasterDriver is None:
        return 'skip'
        
    if gdaltest.oci_ds is None:
        return 'skip'

    ds_src = gdal.Open('data/byte.tif')

    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST,RASTER', ds_src, 1, 
        [ "DESCRIPTION=(id number, raster sdo_georaster)" , 
        "INSERT=(1013, sdo_geor.init('GDAL_TEST_RDT',1013))" , 
        "NBITS=4"] ) 

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 2578, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_cleanup():

    if gdaltest.georasterDriver is None:
        return 'skip'

    if gdaltest.oci_ds is None:
        return 'skip'

    gdaltest.oci_ds.ExecuteSQL( 'drop table GDAL_TEST' )
    gdaltest.oci_ds.ExecuteSQL( 'drop table GDAL_TEST_RDT' )

    gdaltest.oci_ds.Destroy()
    gdaltest.oci_ds = None

    return 'success'

###############################################################################
# 

gdaltest_list = [
    georaster_init,
    georaster_byte,
    georaster_int16,
    georaster_int32,
    georaster_rgb_b1,
    georaster_rgb_b2,
    georaster_rgb_b3_bsq,
    georaster_rgb_b3_bip,
    georaster_rgb_b3_bil,
    georaster_byte_deflate,
    georaster_rgb_deflate_b3,
    georaster_1bit,
    georaster_2bit,
    georaster_4bit,
    georaster_cleanup
    ]

if __name__ == '__main__':

    if 'OCI_DSNAME' not in os.environ:
        print('Enter ORACLE connection (eg. OCI:scott/tiger@orcl): ')
        oci_dsname = string.strip(sys.stdin.readline())
        os.environ['OCI_DSNAME'] = oci_dsname

    gdaltest.setup_run( 'GeoRaster' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

