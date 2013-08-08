#!/usr/bin/env python
# -*- coding: utf-8 -*-
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
import shutil

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Verify we have the driver.

def wms_1():

    try:
        gdaltest.wms_drv = gdal.GetDriverByName( 'WMS' )
    except:
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
        
    # NOTE - mloskot:
    # This is a dirty hack checking if remote WMS service is online.
    # Nothing genuine but helps to keep the buildbot waterfall green.
    
    srv = 'http://sedac.ciesin.columbia.edu/mapserver/map/GPWv3?'
    gdaltest.wms_srv1_ok = gdaltest.gdalurlopen(srv) is not None
    gdaltest.wms_ds = None
    
    if not gdaltest.wms_srv1_ok:
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

    if not gdaltest.wms_srv1_ok:
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

    if not gdaltest.wms_srv1_ok:
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

    # We don't need to check if the remote service is online as we
    # don't need a connection for this test
    
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
    
    # We don't need to check if the remote service is online as we
    # don't need a connection for this test
    
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

    srv = 'http://tilecache.osgeo.org/wms-c/Basic.py'
    gdaltest.metacarta_tms = False
    if gdaltest.gdalurlopen(srv) is None:
        return 'skip'
    gdaltest.metacarta_tms = True

    tms = """<GDAL_WMS>
    <Service name="TMS">
        <ServerUrl>http://tilecache.osgeo.org/wms-c/Basic.py</ServerUrl>
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

    if gdaltest.metacarta_tms is not True:
        return 'skip'

    tms = """<GDAL_WMS>
    <Service name="TMS">
        <ServerUrl>http://tilecache.osgeo.org/wms-c/Basic.py</ServerUrl>
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

    try:
        shutil.rmtree('tmp/gdalwmscache')
    except:
        pass

    ds = gdal.Open( tms )

    if ds is None:
        gdaltest.post_reason( 'open failed.' )
        return 'fail'

    ds.GetRasterBand(1).GetOverview(18).ReadRaster(0, 0, 512, 256)

    ds = None

    try:
        os.stat('tmp/gdalwmscache')
    except:
        gdaltest.post_reason( 'tmp/gdalwmscache should exist')
        return 'fail'

    # Now, we should read from the cache
    ds = gdal.Open( tms )
    ds.GetRasterBand(1).GetOverview(18).ReadRaster(0, 0, 512, 256)
    ds = None

    return 'success'

###############################################################################
# Test OnEarth Tiled WMS minidriver

def wms_9():

    if gdaltest.wms_drv is None:
        return 'skip'

    tms = """<GDAL_WMS>
    <Service name="TiledWMS">
	<ServerUrl>http://onearth.jpl.nasa.gov/wms.cgi?</ServerUrl>
	<TiledGroupName>Global SRTM Elevation</TiledGroupName>
    </Service>
</GDAL_WMS>
"""

    ds = gdal.Open( tms )

    if ds is None:
        srv = 'http://onearth.jpl.nasa.gov/wms.cgi?'
        if gdaltest.gdalurlopen(srv) is None:
            return 'skip'
        gdaltest.post_reason( 'open failed.' )
        return 'fail'

    expected_cs = 5478
    cs = ds.GetRasterBand(1).GetOverview(9).Checksum()

    if cs != expected_cs:
        gdaltest.post_reason( 'Did not get expected SRTM checksum.' )
        print(cs)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test getting subdatasets from GetCapabilities

def wms_10():

    if gdaltest.wms_drv is None:
        return 'skip'

    if not gdaltest.wms_srv1_ok:
        return 'skip'

    name = "WMS:http://sedac.ciesin.columbia.edu/mapserver/map/GPWv3?"
    ds = gdal.Open( name )
    if ds is None:
        gdaltest.post_reason( 'open of %s failed.' % name)
        return 'fail'

    subdatasets = ds.GetMetadata("SUBDATASETS")
    if len(subdatasets) == 0:
        gdaltest.post_reason( 'did not get expected subdataset count' )
        print(subdatasets)
        return 'fail'

    ds = None

    name = subdatasets['SUBDATASET_1_NAME']
    ds = gdal.Open( name )
    if ds is None:
        gdaltest.post_reason( 'open of %s failed.' % name)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test getting subdatasets from GetTileService

def wms_11():

    if gdaltest.wms_drv is None:
        return 'skip'

    name = "WMS:http://onearth.jpl.nasa.gov/wms.cgi?request=GetTileService"
    ds = gdal.Open( name )
    if ds is None:
        srv = 'http://onearth.jpl.nasa.gov/wms.cgi?request=GetTileService'
        if gdaltest.gdalurlopen(srv) is None:
            return 'skip'
        gdaltest.post_reason( 'open of %s failed.' % name)
        return 'fail'

    subdatasets = ds.GetMetadata("SUBDATASETS")
    if len(subdatasets) == 0:
        gdaltest.post_reason( 'did not get expected subdataset count' )
        print(subdatasets)
        return 'fail'

    ds = None

    name = subdatasets['SUBDATASET_1_NAME']
    ds = gdal.Open( name )
    if ds is None:
        gdaltest.post_reason( 'open of %s failed.' % name)
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test getting subdatasets from a TMS server

def wms_12():

    if gdaltest.wms_drv is None:
        return 'skip'

    if gdaltest.metacarta_tms is not True:
        return 'skip'

    name = "http://tilecache.osgeo.org/wms-c/Basic.py/1.0.0/"
    ds = gdal.Open( name )
    if ds is None:
        gdaltest.post_reason( 'open of %s failed.' % name)
        return 'fail'

    subdatasets = ds.GetMetadata("SUBDATASETS")
    if len(subdatasets) == 0:
        gdaltest.post_reason( 'did not get expected subdataset count' )
        print(subdatasets)
        return 'fail'

    ds = None

    for i in range(len(subdatasets) // 2):
        desc = subdatasets['SUBDATASET_%d_DESC' % (i+1)]
        if desc == 'basic':
            name = subdatasets['SUBDATASET_%d_NAME' % (i+1)]
            ds = gdal.Open( name )
            if ds is None:
                gdaltest.post_reason( 'open of %s failed.' % name)
                return 'fail'
            ds = None

    return 'success'

###############################################################################
# Test reading WMS through VRT (test effect of r21866)

def wms_13():

    if gdaltest.wms_drv is None:
        return 'skip'

    ds = gdal.Open( "data/DNEC_250K.vrt" )
    if ds.ReadRaster(0, 0, 1024, 682) is None:
        srv = 'http://wms.geobase.ca/wms-bin/cubeserv.cgi?SERVICE=WMS&VERSION=1.1.1&REQUEST=GeCapabilities'
        if gdaltest.gdalurlopen(srv) is None:
            return 'skip'
        return 'fail'
    ds = None

    return 'success'



###############################################################################
# Test reading Virtual Earth layer

def wms_14():

    if gdaltest.wms_drv is None:
        return 'skip'
    ds = gdal.Open( """<GDAL_WMS>
  <Service name="VirtualEarth">
    <ServerUrl>http://a${server_num}.ortho.tiles.virtualearth.net/tiles/a${quadkey}.jpeg?g=90</ServerUrl>
  </Service>
</GDAL_WMS>""")
    if ds is None:
        return' fail'

    if ds.RasterXSize != 134217728 \
       or ds.RasterYSize != 134217728 \
       or ds.RasterCount != 3:
        gdaltest.post_reason( 'wrong size or bands' )
        return 'fail'

    wkt = ds.GetProjectionRef()
    if wkt.find('PROJCS["Google Maps Global Mercator"') != 0:
        gdaltest.post_reason( 'Got wrong SRS: ' + wkt )
        return 'fail'

    gt = ds.GetGeoTransform()
    if abs(gt[0]- -20037508.339999999850988) > 0.00001 \
       or abs(gt[3]- 20037508.339999999850988) > 0.00001 \
       or abs(gt[1] - 0.298582141697407) > 0.00001 \
       or abs(gt[2] - 0) > 0.00001 \
       or abs(gt[5] - -0.298582141697407) > 0.00001 \
       or abs(gt[4] - 0) > 0.00001:
        gdaltest.post_reason( 'wrong geotransform' )
        print(gt)
        return 'fail'

    if ds.GetRasterBand(1).GetOverviewCount() != 18:
        gdaltest.post_reason( 'bad overview count' )
        print(ds.GetRasterBand(1).GetOverviewCount())
        return 'fail'

    (block_xsize, block_ysize) = ds.GetRasterBand(1).GetBlockSize()
    if block_xsize != 256 or block_ysize != 256:
        gdaltest.post_reason( 'bad block size' )
        print("(%d, %d)" % (block_xsize, block_ysize))
        return 'fail'

    return 'success'

###############################################################################
# Test reading ArcGIS MapServer JSon definition and CreateCopy()

def wms_15():

    if gdaltest.wms_drv is None:
        return 'skip'
    src_ds = gdal.Open( "http://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer?f=json&pretty=true")
    if src_ds is None:
        srv = 'http://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer?f=json&pretty=true'
        if gdaltest.gdalurlopen(srv) is None:
            return 'skip'
        return 'fail'
    ds = gdal.GetDriverByName("WMS").CreateCopy("/vsimem/wms.xml", src_ds)
    src_ds = None

    if ds is None:
        return' fail'

    if ds.RasterXSize != 134217728 \
       or ds.RasterYSize != 134217728 \
       or ds.RasterCount != 3:
        gdaltest.post_reason( 'wrong size or bands' )
        return 'fail'

    wkt = ds.GetProjectionRef()
    if wkt.find('PROJCS["WGS 84 / Pseudo-Mercator"') != 0:
        gdaltest.post_reason( 'Got wrong SRS: ' + wkt )
        return 'fail'

    gt = ds.GetGeoTransform()
    if abs(gt[0]- -20037508.342787001) > 0.00001 \
       or abs(gt[3]- 20037508.342787001) > 0.00001 \
       or abs(gt[1] - 0.298582141697407) > 0.00001 \
       or abs(gt[2] - 0) > 0.00001 \
       or abs(gt[5] - -0.298582141697407) > 0.00001 \
       or abs(gt[4] - 0) > 0.00001:
        gdaltest.post_reason( 'wrong geotransform' )
        print(gt)
        return 'fail'

    if ds.GetRasterBand(1).GetOverviewCount() != 19:
        gdaltest.post_reason( 'bad overview count' )
        print(ds.GetRasterBand(1).GetOverviewCount())
        return 'fail'

    (block_xsize, block_ysize) = ds.GetRasterBand(1).GetBlockSize()
    if block_xsize != 256 or block_ysize != 256:
        gdaltest.post_reason( 'bad block size' )
        print("(%d, %d)" % (block_xsize, block_ysize))
        return 'fail'

    ds = None
    gdal.Unlink("/vsimem/wms.xml")

    return 'success'

###############################################################################
# Test getting subdatasets from WMS-C Capabilities

def wms_16():

    if gdaltest.wms_drv is None:
        return 'skip'

    name = "WMS:http://demo.opengeo.org/geoserver/gwc/service/wms?tiled=TRUE"
    ds = gdal.Open( name )
    if ds is None:
        srv = 'http://demo.opengeo.org/geoserver/gwc/service/wms?'
        if gdaltest.gdalurlopen(srv) is None:
            return 'skip'
        gdaltest.post_reason( 'open of %s failed.' % name)
        return 'fail'
 
    subdatasets = ds.GetMetadata("SUBDATASETS")
    if len(subdatasets) == 0:
        gdaltest.post_reason( 'did not get expected subdataset count' )
        print(subdatasets)
        return 'fail'

    ds = None

    name = None
    for key in subdatasets:
        if key[-5:] == '_NAME' and subdatasets[key].find('bugsites') != -1:
            name = subdatasets[key]
            break
    if name is None:
        return 'fail'

    name = 'http://demo.opengeo.org/geoserver/wms?SERVICE=WMS&request=GetMap&version=1.1.1&layers=og:bugsites&styles=&srs=EPSG:26713&bbox=599351.50000000,4914096.00000000,608471.00000000,4920512.00000000'
    ds = gdal.Open( name )
    if ds is None:
        gdaltest.post_reason( 'open of %s failed.' % name)
        return 'fail'

    # Matches feature of "WFS:http://demo.opengeo.org/geoserver/wfs?SRSNAME=EPSG:900913" og:bugsites
    # OGRFeature(og:bugsites):68846
    #   gml_id (String) = bugsites.68846
    #   cat (Integer) = 86
    #   str1 (String) = Beetle site
    #   POINT (-11547069.564865021035075 5528605.849725087173283)

    pixel = "GeoPixel_601228_4917635"
    val = ds.GetRasterBand(1).GetMetadataItem(pixel, "LocationInfo")
    
    # Some bug in GeoServer ?
    if val is not None and val.find('java.lang.NoSuchMethodError: org.geoserver.wms.WMS.pixelToWorld') >= 0:
        print(val)
        return 'skip'

    if val is None or val.find('<og:cat>86</og:cat>') == -1:
        gdaltest.post_reason('expected a value')
        print(val)
        return 'fail'

    # Ask again. Should be cached
    val_again = ds.GetRasterBand(1).GetMetadataItem(pixel, "LocationInfo")
    if val_again != val:
        gdaltest.post_reason('expected a value')
        return 'fail'

    # Ask another band. Should be cached
    val2 = ds.GetRasterBand(2).GetMetadataItem(pixel, "LocationInfo")
    if val2 != val:
        gdaltest.post_reason('expected a value')
        return 'fail'

    # Ask an overview band
    val2 = ds.GetRasterBand(1).GetOverview(0).GetMetadataItem(pixel, "LocationInfo")
    if val2 != val:
        gdaltest.post_reason('expected a value')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test a TiledWMS dataset with a color table (#4613)

def wms_17():

    if gdaltest.wms_drv is None:
        return 'skip'

    name = '<GDAL_WMS><Service name="TiledWMS"><ServerUrl>http://onmoon.lmmp.nasa.gov/sites/wms.cgi?</ServerUrl><TiledGroupName>King Crater DEM Color Confidence, LMMP</TiledGroupName></Service></GDAL_WMS>'
    ds = gdal.Open( name )
    if ds is None:
        srv = 'http://onmoon.lmmp.nasa.gov/sites/wms.cgi?'
        if gdaltest.gdalurlopen(srv) is None:
            return 'skip'
        gdaltest.post_reason( 'open of %s failed.' % name)
        return 'fail'

    band = ds.GetRasterBand(1)
    if band.GetColorTable() is None:
        return 'fail'

    ds = None

    return 'success'

###############################################################################
def wms_cleanup():

    gdaltest.wms_ds = None
    gdaltest.clean_tmp()
    
    return 'success'

gdaltest_list = [
    wms_1,
    #wms_2,
    #wms_3,
    #wms_4,
    wms_5,
    wms_6,
    wms_7,
    wms_8,
    #wms_9,
    #wms_10,
    wms_11,
    wms_12,
    wms_13,
    wms_14,
    wms_15,
    #wms_16, #FIXME: reenable after adapting test
    wms_17,
    wms_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'wms' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

