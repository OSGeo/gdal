#!/usr/bin/env python
###############################################################################
# $Id: georaster.py $
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
import gdal
import ogr
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
    try:
        gdaltest.georasterDriver = gdal.GetDriverByName('GeoRaster')
    except:
        gdaltest.georasterDriver = None

    if gdaltest.georasterDriver is None:
        return 'skip'

    gdaltest.oci_ds = ogr.Open( os.environ.get('OCI_DSNAME') )

    if gdaltest.oci_ds is None:
        return 'skip'

    return 'success'

###############################################################################
# 

def georaster_1():

    if gdaltest.georasterDriver is None:
        return 'skip'

    ds_src = gdal.Open('data/byte.tif')

    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST_TABLE,RASTER', ds_src, 1, 
        [ "DESCRIPTION=(id number, raster sdo_georaster)" , 
        "INSERT=(1001, sdo_geor.init('GDAL_TEST_RDT',1001))" ] ) 

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 4672, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_2():

    if gdaltest.georasterDriver is None:
        return 'skip'

    connection = get_connection_str()

    ds_src = gdal.Open('../gcore/data/uint16.tif')

    ds = gdaltest.georasterDriver.CreateCopy( get_connection_str() +
        ',GDAL_TEST_TABLE,RASTER', ds_src, 1, 
        [ "DESCRIPTION=(id number, raster sdo_georaster)" , 
        "INSERT=(1002, sdo_geor.init('GDAL_TEST_RDT',1002))" ] ) 

    ds_name = ds.GetDescription()

    ds = None

    tst = gdaltest.GDALTest( 'GeoRaster', ds_name, 1, 4672, filename_absolute = 1 )

    return tst.testOpen()

###############################################################################
# 

def georaster_cleanup():

    if gdaltest.georasterDriver is None:
        return 'skip'

    if gdaltest.oci_ds is None:
        return 'skip'

    gdaltest.oci_ds.ExecuteSQL( 'drop table GDAL_TEST_TABLE' )
    gdaltest.oci_ds.ExecuteSQL( 'drop table GDAL_TEST_RDT' )

    gdaltest.oci_ds.Destroy()
    gdaltest.oci_ds = None

    return 'success'

###############################################################################
# 

gdaltest_list = [
    georaster_init,
    georaster_1,
    georaster_2,
    georaster_cleanup
    ]

if __name__ == '__main__':

    if not os.environ.has_key('OCI_DSNAME'):
        print 'Enter ORACLE connection (eg. OCI:scott/tiger@orcl):'
        oci_dsname = string.strip(sys.stdin.readline())
        os.environ['OCI_DSNAME'] = oci_dsname

    gdaltest.setup_run( 'GeoRaster' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

