#!/usr/bin/env python
###############################################################################
# $Id: wcs.py,v 1.4 2007/01/02 14:44:31 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test WCS client support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
# 
#  $Log: wcs.py,v $
#  Revision 1.4  2007/01/02 14:44:31  fwarmerdam
#  added service description in the url test
#
#  Revision 1.3  2006/10/27 04:38:34  fwarmerdam
#  Report skip on first test if driver not available.
#
#  Revision 1.2  2006/10/27 04:27:12  fwarmerdam
#  fixed license text
#
#  Revision 1.1  2006/10/27 04:20:14  fwarmerdam
#  New
#
#

import os
import sys
import string
import array
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Verify we have the driver.

def wcs_1():

    try:
        gdaltest.wcs_drv = gdal.GetDriverByName( 'WCS' )
    except:
	gdaltest.wcs_drv = None

    if gdaltest.wcs_drv is None:
        return 'skip'
    else:
        return 'success'

###############################################################################
# Open the srtm plus service.

def wcs_2():

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

def wcs_3():

    if gdaltest.wcs_drv is None or gdaltest.wcs_ds is None:
	return 'skip'

    if gdaltest.wcs_ds.RasterXSize != 43200 \
       and gdaltest.wcs_ds.RasterYSize != 21600 \
       and gdaltest.wcs_ds.BandCount != 1:
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
        print gt
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

def wcs_4():

    if gdaltest.wcs_drv is None or gdaltest.wcs_ds is None:
	return 'skip'

    cs = gdaltest.wcs_ds.GetRasterBand(1).Checksum( 0, 0, 100, 100 )
    if cs != 10469:
        gdaltest.post_reason( 'Wrong checksum: ' + str(cs) )
        return 'fail'

    return 'success'

###############################################################################
# Open the srtm plus service using XML as filename.

def wcs_5():

    if gdaltest.wcs_drv is None:
	return 'skip'

    fn = '<WCS_GDAL><ServiceURL>http://maps.gdal.org/cgi-bin/mapserv_dem?</ServiceURL><CoverageName>srtmplus_raw</CoverageName><Timeout>75</Timeout></WCS_GDAL>'

    ds = gdal.Open( fn )

    if ds is None:
        gdaltest.post_reason( 'open failed.' )
        return 'fail'

    if ds.RasterXSize != 43200 \
       and ds.RasterYSize != 21600 \
       and ds.BandCount != 1:
        gdaltest.post_reason( 'wrong size or bands' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
def wcs_cleanup():

    gdaltest.wcs_ds = None
    gdaltest.clean_tmp()
    
    return 'success'

gdaltest_list = [
    wcs_1,
    wcs_2,
    wcs_3,
    wcs_4,
    wcs_5,
    wcs_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'wcs' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

