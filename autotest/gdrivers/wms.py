#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test WMS client support.
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

import os
import sys
import string
import array
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Verify we have the driver.

def wms_1():

    try:
        gdaltest.wms_drv = gdal.GetDriverByName( 'WMS' )
    except:
        gdaltest.wms_drv = None


    # NOTE - mloskot:
    # This is a dirty hack checking if remote WMS service is online.
    # Nothing genuine but helps to keep the buildbot waterfall green.
    
    srv = 'http://sedac.ciesin.columbia.edu/mapserver/map/GPWv3?'
    if gdaltest.gdalurlopen(srv) is None:
        gdaltest.wms_drv = None

    if gdaltest.wms_drv is None:
        return 'skip'
    else:
        return 'success'

###############################################################################
# Open the WMS dataset

def wms_2():

    if gdaltest.wms_drv is None:
        return 'skip'

    gdaltest.wms_ds = gdal.Open( 'data/pop_wms.xml' )

    if gdaltest.wms_ds is not None:
        return 'success'
    else:
        gdaltest.post_reason( 'open failed.' )
        return 'fail'

###############################################################################
# Check various things about the configuration.

def wms_3():

    if gdaltest.wms_drv is None or gdaltest.wms_ds is None:
        return 'skip'

    if gdaltest.wms_ds.RasterXSize != 36000 \
       or gdaltest.wms_ds.RasterYSize != 14500 \
       or gdaltest.wms_ds.RasterCount != 3:
        gdaltest.post_reason( 'wrong size or bands' )
        return 'fail'
    
    wkt = gdaltest.wms_ds.GetProjectionRef()
    if wkt[:14] != 'GEOGCS["WGS 84':
        gdaltest.post_reason( 'Got wrong SRS: ' + wkt )
        return 'fail'
        
    gt = gdaltest.wms_ds.GetGeoTransform()
    if abs(gt[0]- -180) > 0.00001 \
       or abs(gt[3]- 85) > 0.00001 \
       or abs(gt[1] - 0.01) > 0.00001 \
       or abs(gt[2] - 0) > 0.00001 \
       or abs(gt[5] - -0.01) > 0.00001 \
       or abs(gt[4] - 0) > 0.00001:
        gdaltest.post_reason( 'wrong geotransform' )
        print(gt)
        return 'fail'
    
    if gdaltest.wms_ds.GetRasterBand(1).GetOverviewCount() < 1:
        gdaltest.post_reason( 'no overviews!' )
        return 'fail'

    if gdaltest.wms_ds.GetRasterBand(1).DataType < gdal.GDT_Byte:
        gdaltest.post_reason( 'wrong band data type' )
        return 'fail'

    return 'success'

###############################################################################
# Check checksum for a small region.

def wms_4():

    if gdaltest.wms_drv is None or gdaltest.wms_ds is None:
        return 'skip'

    gdal.SetConfigOption('CPL_ACCUM_ERROR_MSG', 'ON')
    gdal.PushErrorHandler('CPLQuietErrorHandler')

    cs = gdaltest.wms_ds.GetRasterBand(1).Checksum( 0, 0, 100, 100 )

    gdal.PopErrorHandler()
    gdal.SetConfigOption('CPL_ACCUM_ERROR_MSG', 'OFF')
    msg = gdal.GetLastErrorMsg()
    gdal.ErrorReset()

    if msg is not None and msg.find('Service denied due to system overload') != -1:
        print(msg)
        return 'skip'

    if cs != 57182:
        gdaltest.post_reason( 'Wrong checksum: ' + str(cs) )
        return 'fail'

    return 'success'

###############################################################################
# Open the WMS service using XML as filename.

def wms_5():

    if gdaltest.wms_drv is None:
        return 'skip'

    fn = '<GDAL_WMS><Service name="WMS"><Version>1.1.1</Version><ServerUrl>http://onearth.jpl.nasa.gov/wms.cgi?</ServerUrl><SRS>EPSG:4326</SRS><ImageFormat>image/jpeg</ImageFormat><Layers>modis,global_mosaic</Layers><Styles></Styles></Service><DataWindow><UpperLeftX>-180.0</UpperLeftX><UpperLeftY>90.0</UpperLeftY><LowerRightX>180.0</LowerRightX><LowerRightY>-90.0</LowerRightY><SizeX>2666666</SizeX><SizeY>1333333</SizeY></DataWindow><Projection>EPSG:4326</Projection><BandsCount>3</BandsCount></GDAL_WMS>'

    ds = gdal.Open( fn )

    if ds is None:
        gdaltest.post_reason( 'open failed.' )
        return 'fail'

    if ds.RasterXSize != 2666666 \
       or ds.RasterYSize != 1333333 \
       or ds.RasterCount != 3:
        gdaltest.post_reason( 'wrong size or bands' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test TileService

def wms_6():

    if gdaltest.wms_drv is None:
        return 'skip'

    # NOTE - mloskot:
    # This is a dirty hack checking if remote WMS service is online.
    # Nothing genuine but helps to keep the buildbot waterfall green.
    srv = 'http://s0.tileservice.worldwindcentral.com/getTile?'
    if gdaltest.gdalurlopen(srv) is None:
        gdaltest.wms_drv = None

    if gdaltest.wms_drv is None:
        return 'skip'
    
    fn = '<GDAL_WMS><Service name="TileService"><Version>1</Version><ServerUrl>http://s0.tileservice.worldwindcentral.com/getTile?</ServerUrl><Dataset>za.johannesburg_2006_20cm</Dataset></Service><DataWindow><UpperLeftX>-180.0</UpperLeftX><UpperLeftY>90.0</UpperLeftY><LowerRightX>180.0</LowerRightX><LowerRightY>-90.0</LowerRightY><SizeX>268435456</SizeX><SizeY>134217728</SizeY><TileLevel>19</TileLevel></DataWindow><Projection>EPSG:4326</Projection><OverviewCount>16</OverviewCount><BlockSizeX>512</BlockSizeX><BlockSizeY>512</BlockSizeY><BandsCount>3</BandsCount></GDAL_WMS>'

    ds = gdal.Open( fn )

    if ds is None:
        gdaltest.post_reason( 'open failed.' )
        return 'fail'

    if ds.RasterXSize != 268435456 \
       or ds.RasterYSize != 134217728 \
       or ds.RasterCount != 3:
        gdaltest.post_reason( 'wrong size or bands' )
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test TMS

def wms_7():

    if gdaltest.wms_drv is None:
        return 'skip'

    tms = """<GDAL_WMS>
    <Service name="TMS">
        <ServerUrl>http://labs.metacarta.com/wms-c/Basic.py</ServerUrl>
        <Layer>basic</Layer>
        <Format>png</Format>
    </Service>
    <DataWindow>
        <UpperLeftX>-180.0</UpperLeftX>
        <UpperLeftY>90.0</UpperLeftY>
        <LowerRightX>180.0</LowerRightX>
        <LowerRightY>-90.0</LowerRightY>
        <TileLevel>19</TileLevel>
        <TileCountX>2</TileCountX>
        <TileCountY>1</TileCountY>
    </DataWindow>
    <Projection>EPSG:4326</Projection>
    <BlockSizeX>256</BlockSizeX>
    <BlockSizeY>256</BlockSizeY>
    <BandsCount>3</BandsCount>
</GDAL_WMS>"""

    ds = gdal.Open( tms )

    if ds is None:
        gdaltest.post_reason( 'open failed.' )
        return 'fail'

    if ds.RasterXSize != 268435456 \
       or ds.RasterYSize != 134217728 \
       or ds.RasterCount != 3:
        gdaltest.post_reason( 'wrong size or bands' )
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'

    if ds.GetRasterBand(1).GetOverview(18).XSize != 512 \
       or ds.GetRasterBand(1).GetOverview(18).YSize != 256:
        print(ds.GetRasterBand(1).GetOverview(18).XSize)
        print(ds.GetRasterBand(1).GetOverview(18).YSize)
        return 'fail'

    ds.GetRasterBand(1).GetOverview(18).ReadRaster(0, 0, 512, 256)

    ds = None

    return 'success'


###############################################################################
# Test TMS with cache

def wms_8():

    if gdaltest.wms_drv is None:
        return 'skip'

    tms = """<GDAL_WMS>
    <Service name="TMS">
        <ServerUrl>http://labs.metacarta.com/wms-c/Basic.py</ServerUrl>
        <Layer>basic</Layer>
        <Format>png</Format>
    </Service>
    <DataWindow>
        <UpperLeftX>-180.0</UpperLeftX>
        <UpperLeftY>90.0</UpperLeftY>
        <LowerRightX>180.0</LowerRightX>
        <LowerRightY>-90.0</LowerRightY>
        <TileLevel>19</TileLevel>
        <TileCountX>2</TileCountX>
        <TileCountY>1</TileCountY>
    </DataWindow>
    <Projection>EPSG:4326</Projection>
    <BlockSizeX>256</BlockSizeX>
    <BlockSizeY>256</BlockSizeY>
    <BandsCount>3</BandsCount>
    <Cache><Path>./tmp/gdalwmscache</Path></Cache>
</GDAL_WMS>"""

    ds = gdal.Open( tms )

    if ds is None:
        gdaltest.post_reason( 'open failed.' )
        return 'fail'

    ds.GetRasterBand(1).GetOverview(18).ReadRaster(0, 0, 512, 256)

    ds = None

    return 'success'

###############################################################################
def wms_cleanup():

    gdaltest.wms_ds = None
    gdaltest.clean_tmp()
    
    return 'success'

gdaltest_list = [
    wms_1,
    wms_2,
    wms_3,
    wms_4,
    wms_5,
    wms_6,
    wms_7,
    wms_8,
    wms_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'wms' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

