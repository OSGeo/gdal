#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for ADRG driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2007-2009, Even Rouault <even dot rouault at mines-paris dot org>
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
import shutil

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Read test of simple byte reference data.

def adrg_read_gen():

    tst = gdaltest.GDALTest( 'ADRG', 'SMALL_ADRG/ABCDEF01.GEN', 1, 62833 )
    return tst.testOpen()

###############################################################################
# Read test of simple byte reference data by the TRANSH01.THF file .

def adrg_read_transh():

    tst = gdaltest.GDALTest( 'ADRG', 'SMALL_ADRG/TRANSH01.THF', 1, 62833 )
    return tst.testOpen()

###############################################################################
# Read test of simple byte reference data by a subdataset file

def adrg_read_subdataset_img():

    tst = gdaltest.GDALTest( 'ADRG', 'ADRG:data/SMALL_ADRG/ABCDEF01.GEN,data/SMALL_ADRG/ABCDEF01.IMG', 1, 62833, filename_absolute = 1 )
    return tst.testOpen()
    
###############################################################################
# Test copying.

def adrg_copy():

    drv = gdal.GetDriverByName( 'ADRG' )
    srcds = gdal.Open( 'data/SMALL_ADRG/ABCDEF01.GEN' )
    
    dstds = drv.CreateCopy( 'tmp/ABCDEF01.GEN', srcds )
    
    chksum = dstds.GetRasterBand(1).Checksum()

    if chksum != 62833:
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    dstds = None
    
    drv.Delete( 'tmp/ABCDEF01.GEN' )

    return 'success'
    
###############################################################################
# Test creating a fake 2 subdataset image and reading it.

def adrg_2subdatasets():

    drv = gdal.GetDriverByName( 'ADRG' )
    srcds = gdal.Open( 'data/SMALL_ADRG/ABCDEF01.GEN' )
    
    gdal.SetConfigOption('ADRG_SIMULATE_MULTI_IMG', 'ON')
    dstds = drv.CreateCopy( 'tmp/XXXXXX01.GEN', srcds )
    del dstds
    gdal.SetConfigOption('ADRG_SIMULATE_MULTI_IMG', 'OFF')
    
    shutil.copy('tmp/XXXXXX01.IMG', 'tmp/XXXXXX02.IMG')
    
    ds = gdal.Open('tmp/TRANSH01.THF')
    if ds.RasterCount != 0:
        gdaltest.post_reason('did not expected non 0 RasterCount')
        return 'fail'
    ds = None
    
    ds = gdal.Open('ADRG:tmp/XXXXXX01.GEN,tmp/XXXXXX02.IMG')
    chksum = ds.GetRasterBand(1).Checksum()

    if chksum != 62833:
        gdaltest.post_reason('Wrong checksum')
        return 'fail'
        
    md = ds.GetMetadata('')
    if md['ADRG_NAM'] != 'XXXXXX02':
        gdaltest.post_reason( 'metadata wrong.' )
        return 'fail'

    ds = None        
        
    os.remove('tmp/XXXXXX01.GEN')
    os.remove('tmp/XXXXXX01.GEN.aux.xml')
    os.remove('tmp/XXXXXX01.IMG')
    os.remove('tmp/XXXXXX02.IMG')
    os.remove('tmp/TRANSH01.THF')

    return 'success'
    
###############################################################################
# Test creating an in memory copy.

def adrg_copy_vsimem():

    drv = gdal.GetDriverByName( 'ADRG' )
    srcds = gdal.Open( 'data/SMALL_ADRG/ABCDEF01.GEN' )
    
    dstds = drv.CreateCopy( '/vsimem/ABCDEF01.GEN', srcds )
    
    chksum = dstds.GetRasterBand(1).Checksum()

    if chksum != 62833:
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    dstds = None
    
    # Reopen file
    ds = gdal.Open( '/vsimem/ABCDEF01.GEN' )
    
    chksum = ds.GetRasterBand(1).Checksum()
    if chksum != 62833:
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    ds = None
    
    drv.Delete( '/vsimem/ABCDEF01.GEN' )

    return 'success'
    

###############################################################################
gdaltest_list = [
    adrg_read_gen,
    adrg_read_transh,
    adrg_read_subdataset_img,
    adrg_copy,
    adrg_2subdatasets,
    adrg_copy_vsimem ]

if __name__ == '__main__':

    gdaltest.setup_run( 'adrg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

