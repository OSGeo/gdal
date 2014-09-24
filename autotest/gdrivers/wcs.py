#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test WCS client support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
import string
import array
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Verify we have the driver.

def wcs_1():

    # Disable wcs tests till we have a more reliable test server.
    gdaltest.wcs_drv = None

    try:
        gdaltest.wcs_drv = gdal.GetDriverByName( 'WCS' )
    except:
        gdaltest.wcs_drv = None

    # NOTE - mloskot:
    # This is a dirty hack checking if remote WCS service is online.
    # Nothing genuine but helps to keep the buildbot waterfall green.
    srv = 'http://demo.opengeo.org/geoserver/wcs?'
    if gdaltest.gdalurlopen(srv) is None:
        gdaltest.wcs_drv = None

    gdaltest.wcs_ds = None
    if gdaltest.wcs_drv is None:
        return 'skip'
    else:
        return 'success'

###############################################################################
# Open the GeoServer WCS service.

def wcs_2():

    if gdaltest.wcs_drv is None:
        return 'skip'

    # first, copy to tmp directory.
    open('tmp/geoserver.wcs','w').write(open('data/geoserver.wcs').read())

    gdaltest.wcs_ds = None
    gdaltest.wcs_ds = gdal.Open( 'tmp/geoserver.wcs' )

    if gdaltest.wcs_ds is not None:
        return 'success'
    else:
        gdaltest.post_reason( 'open failed.' )
        return 'fail'

###############################################################################
# Check various things about the configuration.

def wcs_3():

    if gdaltest.wcs_drv is None or gdaltest.wcs_ds is None:
        return 'skip'

    if gdaltest.wcs_ds.RasterXSize != 983 \
       or gdaltest.wcs_ds.RasterYSize != 598 \
       or gdaltest.wcs_ds.RasterCount != 3:
        gdaltest.post_reason( 'wrong size or bands' )
        print(gdaltest.wcs_ds.RasterXSize)
        print(gdaltest.wcs_ds.RasterYSize)
        print(gdaltest.wcs_ds.RasterCount)
        return 'fail'

    wkt = gdaltest.wcs_ds.GetProjectionRef()
    if wkt[:14] != 'GEOGCS["WGS 84':
        gdaltest.post_reason( 'Got wrong SRS: ' + wkt )
        return 'fail'

    gt = gdaltest.wcs_ds.GetGeoTransform()
    expected_gt = (-130.85167999999999, 0.070036907426246159, 0.0, 54.114100000000001, 0.0, -0.055867725752508368)
    for i in range(6):
        if abs(gt[i]- expected_gt[i]) > 0.00001:
            gdaltest.post_reason( 'wrong geotransform' )
            print(gt)
            return 'fail'

    if gdaltest.wcs_ds.GetRasterBand(1).GetOverviewCount() < 1:
        gdaltest.post_reason( 'no overviews!' )
        return 'fail'

    if gdaltest.wcs_ds.GetRasterBand(1).DataType != gdal.GDT_Byte:
        gdaltest.post_reason( 'wrong band data type' )
        return 'fail'

    return 'success'

###############################################################################
# Check checksum

def wcs_4():

    if gdaltest.wcs_drv is None or gdaltest.wcs_ds is None:
        return 'skip'

    cs = gdaltest.wcs_ds.GetRasterBand(1).Checksum()
    if cs != 58765:
        gdaltest.post_reason( 'Wrong checksum: ' + str(cs) )
        return 'fail'

    return 'success'

###############################################################################
# Open the service using XML as filename.

def wcs_5():

    if gdaltest.wcs_drv is None:
        return 'skip'

    fn = """<WCS_GDAL>
  <ServiceURL>http://demo.opengeo.org/geoserver/wcs?</ServiceURL>
  <CoverageName>Img_Sample</CoverageName>
</WCS_GDAL>
"""

    ds = gdal.Open( fn )

    if ds is None:
        gdaltest.post_reason( 'open failed.' )
        return 'fail'

    if ds.RasterXSize != 983 \
       or ds.RasterYSize != 598 \
       or ds.RasterCount != 3:
        gdaltest.post_reason( 'wrong size or bands' )
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        print(ds.RasterCount)
        return 'fail'

    ds = None

    return 'success'
###############################################################################
# Open the srtm plus service.

def old_wcs_2():

    if gdaltest.wcs_drv is None:
        return 'skip'

    # first, copy to tmp directory.
    open('tmp/srtmplus.wcs','w').write(open('data/srtmplus.wcs').read())

    gdaltest.wcs_ds = None
    gdaltest.wcs_ds = gdal.Open( 'tmp/srtmplus.wcs' )

    if gdaltest.wcs_ds is not None:
        return 'success'
    else:
        gdaltest.post_reason( 'open failed.' )
        return 'fail'

###############################################################################
# Check various things about the configuration.

def old_wcs_3():

    if gdaltest.wcs_drv is None or gdaltest.wcs_ds is None:
        return 'skip'

    if gdaltest.wcs_ds.RasterXSize != 43200 \
       or gdaltest.wcs_ds.RasterYSize != 21600 \
       or gdaltest.wcs_ds.RasterCount != 1:
        gdaltest.post_reason( 'wrong size or bands' )
        return 'fail'
    
    wkt = gdaltest.wcs_ds.GetProjectionRef()
    if wkt[:12] != 'GEOGCS["NAD8':
        gdaltest.post_reason( 'Got wrong SRS: ' + wkt )
        return 'fail'
        
    gt = gdaltest.wcs_ds.GetGeoTransform()
    if abs(gt[0]- -180.0041667) > 0.00001 \
       or abs(gt[3]- 90.004167) > 0.00001 \
       or abs(gt[1] - 0.00833333) > 0.00001 \
       or abs(gt[2] - 0) > 0.00001 \
       or abs(gt[5] - -0.00833333) > 0.00001 \
       or abs(gt[4] - 0) > 0.00001:
        gdaltest.post_reason( 'wrong geotransform' )
        print(gt)
        return 'fail'
    
    if gdaltest.wcs_ds.GetRasterBand(1).GetOverviewCount() < 1:
        gdaltest.post_reason( 'no overviews!' )
        return 'fail'

    if gdaltest.wcs_ds.GetRasterBand(1).DataType < gdal.GDT_Int16:
        gdaltest.post_reason( 'wrong band data type' )
        return 'fail'

    return 'success'

###############################################################################
# Check checksum for a small region.

def old_wcs_4():

    if gdaltest.wcs_drv is None or gdaltest.wcs_ds is None:
        return 'skip'

    cs = gdaltest.wcs_ds.GetRasterBand(1).Checksum( 0, 0, 100, 100 )
    if cs != 10469:
        gdaltest.post_reason( 'Wrong checksum: ' + str(cs) )
        return 'fail'

    return 'success'

###############################################################################
# Open the srtm plus service using XML as filename.

def old_wcs_5():

    if gdaltest.wcs_drv is None:
        return 'skip'

    fn = '<WCS_GDAL><ServiceURL>http://geodata.telascience.org/cgi-bin/mapserv_dem?</ServiceURL><CoverageName>srtmplus_raw</CoverageName><Timeout>75</Timeout></WCS_GDAL>'

    ds = gdal.Open( fn )

    if ds is None:
        gdaltest.post_reason( 'open failed.' )
        return 'fail'

    if ds.RasterXSize != 43200 \
       or ds.RasterYSize != 21600 \
       or ds.RasterCount != 1:
        gdaltest.post_reason( 'wrong size or bands' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
def wcs_cleanup():

    gdaltest.wcs_drv = None
    gdaltest.wcs_ds = None

    try:
        os.remove( 'tmp/geoserver.wcs' )
    except:
        pass
    
    return 'success'

gdaltest_list = [
    wcs_1,
    #wcs_2, #FIXME: reenable after adapting test
    wcs_3,
    wcs_4,
    #wcs_5, #FIXME: reenable after adapting test
    wcs_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'wcs' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

