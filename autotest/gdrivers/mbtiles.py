#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for MBTiles driver.
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012-2016, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import gdal
from osgeo import ogr

sys.path.append('../pymod')

import gdaltest
import webserver

###############################################################################
# Get the mbtiles driver


def mbtiles_1():

    try:
        gdaltest.mbtiles_drv = gdal.GetDriverByName('MBTiles')
    except:
        gdaltest.mbtiles_drv = None

    return 'success'

###############################################################################
# Basic test


def mbtiles_2():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('JPEG') is None:
        return 'skip'

    ds = gdal.OpenEx('data/world_l1.mbtiles', open_options=['USE_BOUNDS=NO'])
    if ds is None:
        return 'fail'

    if ds.RasterCount != 4:
        gdaltest.post_reason('expected 3 bands')
        return 'fail'

    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('did not get expected overview count')
        return 'fail'

    expected_cs_tab = [6324, 19386, 45258]
    expected_cs_tab_jpeg8 = [6016, 13996, 45168]
    expected_cs_tab_jpeg9b = [6016, 14034, 45168]
    for i in range(3):
        cs = ds.GetRasterBand(i + 1).Checksum()
        if ds.GetRasterBand(i + 1).GetColorInterpretation() != gdal.GCI_RedBand + i:
            gdaltest.post_reason('bad color interpretation')
            return 'fail'
        expected_cs = expected_cs_tab[i]
        if cs != expected_cs and cs != expected_cs_tab_jpeg8[i] and cs != expected_cs_tab_jpeg9b[i]:
            gdaltest.post_reason('for band %d, cs = %d, different from expected_cs = %d' % (i + 1, cs, expected_cs))
            return 'fail'

    expected_cs_tab = [16642, 15772, 10029]
    expected_cs_tab_jpeg8 = [16621, 14725, 8988]
    expected_cs_tab_jpeg9b = [16621, 14723, 8988]
    for i in range(3):
        cs = ds.GetRasterBand(i + 1).GetOverview(0).Checksum()
        expected_cs = expected_cs_tab[i]
        if cs != expected_cs and cs != expected_cs_tab_jpeg8[i] and cs != expected_cs_tab_jpeg9b[i]:
            gdaltest.post_reason('for overview of band %d, cs = %d, different from expected_cs = %d' % (i + 1, cs, expected_cs))
            return 'fail'

    if ds.GetProjectionRef().find('3857') == -1:
        gdaltest.post_reason('projection_ref = %s' % ds.GetProjectionRef())
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = (-20037508.342789244, 78271.516964020484, 0.0, 20037508.342789244, 0.0, -78271.516964020484)
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-15:
            gdaltest.post_reason('bad gt')
            print(gt)
            print(expected_gt)
            return 'fail'

    md = ds.GetMetadata()
    if md['bounds'] != '-180.0,-85,180,85':
        gdaltest.post_reason('bad metadata')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Open a /vsicurl/ DB


def mbtiles_3():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('HTTP') is None:
        return 'skip'

    if sys.platform == 'darwin' and gdal.GetConfigOption('TRAVIS', None) is not None:
        print("Hangs on MacOSX Travis sometimes. Not sure why.")
        return 'skip'

    # Check that we have SQLite VFS support
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/mbtiles_3.db')
    gdal.PopErrorHandler()
    if ds is None:
        return 'skip'
    ds = None
    gdal.Unlink('/vsimem/mbtiles_3.db')

    ds = gdal.Open('/vsicurl/http://a.tiles.mapbox.com/v3/mapbox.geography-class.mbtiles')
    if ds is None:
        # Just skip. The service isn't perfectly reliable sometimes
        return 'skip'

    # long=2,lat=49 in WGS 84 --> x=222638,y=6274861 in Google Mercator
    locationInfo = ds.GetRasterBand(1).GetMetadataItem('GeoPixel_222638_6274861', 'LocationInfo')
    if locationInfo is None or locationInfo.find("France") == -1:
        gdaltest.post_reason('did not get expected LocationInfo')
        print(locationInfo)
        if gdaltest.skip_on_travis():
            return 'skip'
        return 'fail'

    locationInfo2 = ds.GetRasterBand(1).GetOverview(5).GetMetadataItem('GeoPixel_222638_6274861', 'LocationInfo')
    if locationInfo2 != locationInfo:
        gdaltest.post_reason('did not get expected LocationInfo on overview')
        print(locationInfo2)
        if gdaltest.skip_on_travis():
            return 'skip'
        return 'fail'

    return 'success'

###############################################################################
#


def mbtiles_start_webserver():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('HTTP') is None:
        return 'skip'

    (gdaltest.webserver_process, gdaltest.webserver_port) = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if gdaltest.webserver_port == 0:
        return 'skip'

    return 'success'

###############################################################################
#


def mbtiles_http_jpeg_three_bands():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('HTTP') is None:
        return 'skip'

    if gdal.GetDriverByName('JPEG') is None:
        return 'skip'

    if gdaltest.webserver_port == 0:
        return 'skip'

    handler = webserver.FileHandler(
        {'/world_l1.mbtiles': open('data/world_l1.mbtiles', 'rb').read()})
    with webserver.install_http_handler(handler):
        ds = gdal.Open('/vsicurl/http://localhost:%d/world_l1.mbtiles' % gdaltest.webserver_port)
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
#


def mbtiles_http_jpeg_single_band():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('HTTP') is None:
        return 'skip'

    if gdal.GetDriverByName('JPEG') is None:
        return 'skip'

    if gdaltest.webserver_port == 0:
        return 'skip'

    handler = webserver.FileHandler(
        {'/byte_jpeg.mbtiles': open('data/byte_jpeg.mbtiles', 'rb').read()})
    with webserver.install_http_handler(handler):
        ds = gdal.Open('/vsicurl/http://localhost:%d/byte_jpeg.mbtiles' % gdaltest.webserver_port)
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
#


def mbtiles_http_png():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('HTTP') is None:
        return 'skip'

    if gdal.GetDriverByName('PNG') is None:
        return 'skip'

    if gdaltest.webserver_port == 0:
        return 'skip'

    handler = webserver.FileHandler(
        {'/byte.mbtiles': open('data/byte.mbtiles', 'rb').read()})
    with webserver.install_http_handler(handler):
        ds = gdal.Open('/vsicurl/http://localhost:%d/byte.mbtiles' % gdaltest.webserver_port)
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
#


def mbtiles_stop_webserver():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('HTTP') is None:
        return 'skip'

    if gdaltest.webserver_port != 0:
        webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

    return 'success'

###############################################################################
# Basic test without any option


def mbtiles_4():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('JPEG') is None:
        return 'skip'

    ds = gdal.Open('data/world_l1.mbtiles')
    if ds is None:
        return 'fail'

    if ds.RasterCount != 4:
        gdaltest.post_reason('expected 4 bands')
        return 'fail'

    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('did not get expected overview count')
        return 'fail'

    if ds.RasterXSize != 512 or ds.RasterYSize != 510:
        gdaltest.post_reason('bad dimensions')
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = (-20037508.342789244, 78271.516964020484, 0.0, 19971868.880408563, 0.0, -78271.516964020484)
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-15:
            gdaltest.post_reason('bad gt')
            print(gt)
            print(expected_gt)
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test write support of a single band dataset


def mbtiles_5():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('PNG') is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')
    gdaltest.mbtiles_drv.CreateCopy('/vsimem/mbtiles_5.mbtiles', src_ds)
    src_ds = None

    ds = gdal.OpenEx('/vsimem/mbtiles_5.mbtiles', open_options=['BAND_COUNT=2'])
    if ds.RasterXSize != 19 or ds.RasterYSize != 19:
        gdaltest.post_reason('fail')
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'
    if ds.RasterCount != 2:
        gdaltest.post_reason('fail')
        print(ds.RasterCount)
        return 'fail'
    got_gt = ds.GetGeoTransform()
    expected_gt = (-13095853.550435878, 76.437028285176254, 0.0, 4015708.8887064462, 0.0, -76.437028285176254)
    for i in range(6):
        if abs(expected_gt[i] - got_gt[i]) > 1e-6:
            gdaltest.post_reason('fail')
            print(got_gt)
            print(expected_gt)
            return 'fail'
    got_cs = ds.GetRasterBand(1).Checksum()
    if got_cs != 4118:
        gdaltest.post_reason('fail')
        print(got_cs)
        return 'fail'
    got_cs = ds.GetRasterBand(2).Checksum()
    if got_cs != 4406:
        gdaltest.post_reason('fail')
        print(got_cs)
        return 'fail'
    got_md = ds.GetMetadata()
    expected_md = {'ZOOM_LEVEL': '11', 'minzoom': '11', 'maxzoom': '11', 'name': 'mbtiles_5', 'format': 'png', 'bounds': '-117.6420540294745,33.89160566594387,-117.6290077648261,33.90243460427036', 'version': '1.1', 'type': 'overlay', 'description': 'mbtiles_5'}
    if set(got_md.keys()) != set(expected_md.keys()):
        gdaltest.post_reason('fail')
        print(got_md)
        return 'fail'
    for key in got_md:
        if key != 'bounds' and got_md[key] != expected_md[key]:
            gdaltest.post_reason('fail')
            print(got_md)
            return 'fail'
    ds = None

    gdal.Unlink('/vsimem/mbtiles_5.mbtiles')

    return 'success'

###############################################################################
# Test write support with options


def mbtiles_6():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('JPEG') is None:
        return 'skip'

    # Test options
    src_ds = gdal.Open('data/byte.tif')
    options = []
    options += ['TILE_FORMAT=JPEG']
    options += ['QUALITY=50']
    options += ['NAME=name']
    options += ['DESCRIPTION=description']
    options += ['TYPE=baselayer']
    options += ['VERSION=version']
    options += ['WRITE_BOUNDS=no']
    gdaltest.mbtiles_drv.CreateCopy('tmp/mbtiles_6.mbtiles', src_ds, options=options)
    src_ds = None

    ds = gdal.Open('tmp/mbtiles_6.mbtiles')
    got_cs = ds.GetRasterBand(1).Checksum()
    if got_cs == 0:
        gdaltest.post_reason('fail')
        print(got_cs)
        return 'fail'
    got_md = ds.GetMetadata()
    expected_md = {'ZOOM_LEVEL': '11', 'minzoom': '11', 'maxzoom': '11', 'format': 'jpg', 'version': 'version', 'type': 'baselayer', 'name': 'name', 'description': 'description'}
    if got_md != expected_md:
        gdaltest.post_reason('fail')
        print(got_md)
        return 'fail'
    ds = None

    gdal.Unlink('tmp/mbtiles_6.mbtiles')

    return 'success'

###############################################################################
# Test building overview


def mbtiles_7():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('PNG') is None:
        return 'skip'

    src_ds = gdal.Open('data/small_world.tif')
    data = src_ds.ReadRaster()
    mem_ds = gdal.GetDriverByName('MEM').Create('',
                                                src_ds.RasterXSize * 2,
                                                src_ds.RasterYSize * 2,
                                                src_ds.RasterCount)
    mem_ds.SetProjection(src_ds.GetProjectionRef())
    gt = src_ds.GetGeoTransform()
    gt = [gt[i] for i in range(6)]
    gt[1] /= 2
    gt[5] /= 2
    mem_ds.SetGeoTransform(gt)
    mem_ds.WriteRaster(0, 0, mem_ds.RasterXSize, mem_ds.RasterYSize,
                       data, src_ds.RasterXSize, src_ds.RasterYSize)
    src_ds = None

    gdaltest.mbtiles_drv.CreateCopy('/vsimem/mbtiles_7.mbtiles', mem_ds, options=['TILE_FORMAT=PNG8', 'DITHER=YES', 'RESAMPLING=NEAREST'])
    mem_ds = None

    ds = gdal.Open('/vsimem/mbtiles_7.mbtiles', gdal.GA_Update)
    ds.BuildOverviews('NEAR', [2, 4])
    ds = None

    ds = gdal.Open('/vsimem/mbtiles_7.mbtiles')
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetOverviewCount())
        return 'fail'
    expected_ovr_cs = [21179, 22577, 11996, 17849]
    got_ovr_cs = [ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(ds.RasterCount)]
    if expected_ovr_cs != got_ovr_cs:
        gdaltest.post_reason('fail')
        print(got_ovr_cs)
        return 'fail'
    if ds.GetMetadataItem('minzoom') != '0':
        gdaltest.post_reason('fail')
        print(ds.GetMetadata())
        return 'fail'
    ds = None

    ds = gdal.Open('/vsimem/mbtiles_7.mbtiles', gdal.GA_Update)
    ds.BuildOverviews('NONE', [])
    ds = None

    ds = gdal.Open('/vsimem/mbtiles_7.mbtiles')
    if ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetOverviewCount())
        return 'fail'
    if ds.GetMetadataItem('minzoom') != '1':
        gdaltest.post_reason('fail')
        print(ds.GetMetadata())
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/mbtiles_7.mbtiles')

    return 'success'

###############################################################################
# Single band with 24 bit color table, PNG


def mbtiles_8():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('PNG') is None:
        return 'skip'

    src_ds = gdal.Open('data/small_world_pct.tif')
    out_ds = gdaltest.mbtiles_drv.CreateCopy('/vsimem/mbtiles_8.mbtiles', src_ds, options=['RESAMPLING=NEAREST'])
    out_ds = None
    src_ds = None

    expected_cs = [993, 50461, 64354]
    out_ds = gdal.Open('/vsimem/mbtiles_8.mbtiles')
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    if got_ct is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetRasterBand(1).GetBlockSize() != [256, 256]:
        gdaltest.post_reason('fail')
        print(out_ds.GetRasterBand(1).GetBlockSize())
        return 'fail'
    out_ds = None

    # 512 pixel tiles
    src_ds = gdal.Open('data/small_world_pct.tif')
    out_ds = gdaltest.mbtiles_drv.CreateCopy('/vsimem/mbtiles_8.mbtiles', src_ds, options=['RESAMPLING=NEAREST', 'BLOCKSIZE=512'])
    out_ds = None
    src_ds = None

    expected_cs = [60844, 7388, 53813]
    out_ds = gdal.Open('/vsimem/mbtiles_8.mbtiles')
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    if got_ct is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetRasterBand(1).GetBlockSize() != [512, 512]:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    gdal.Unlink('/vsimem/mbtiles_8.mbtiles')
    return 'success'

###############################################################################
# Test we are robust to invalid bounds


def mbtiles_9():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('PNG') is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')
    gdaltest.mbtiles_drv.CreateCopy('/vsimem/mbtiles_9.mbtiles', src_ds, options=['RESAMPLING=NEAREST'])
    src_ds = None
    ds = ogr.Open('SQLITE:/vsimem/mbtiles_9.mbtiles', update=1)
    ds.ExecuteSQL("UPDATE metadata SET value='invalid' WHERE name='bounds'")
    ds = None

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/mbtiles_9.mbtiles')
    if ds.RasterXSize != 256 or ds.RasterYSize != 256:
        gdaltest.post_reason('fail')
        return 'fail'
    if abs(ds.GetGeoTransform()[0] - -13110479.091473430395126) > 1e-6:
        gdaltest.post_reason('fail')
        print(ds.GetGeoTransform())
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/mbtiles_9.mbtiles')
    return 'success'

###############################################################################
# Test compaction of temporary database


def mbtiles_10():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('PNG') is None:
        return 'skip'

    old_val_GPKG_FORCE_TEMPDB_COMPACTION = gdal.GetConfigOption('GPKG_FORCE_TEMPDB_COMPACTION')
    gdal.SetConfigOption('GPKG_FORCE_TEMPDB_COMPACTION', 'YES')
    with gdaltest.SetCacheMax(0):
        gdal.Translate('/vsimem/mbtiles_10.mbtiles', '../gcore/data/byte.tif', options='-of MBTILES -outsize 512 512')
    gdal.SetConfigOption('GPKG_FORCE_TEMPDB_COMPACTION', old_val_GPKG_FORCE_TEMPDB_COMPACTION)

    ds = gdal.Open('/vsimem/mbtiles_10.mbtiles')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 29925:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/mbtiles_10.mbtiles')
    return 'success'

###############################################################################
# Test opening a .mbtiles.sql file


def mbtiles_11():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdaltest.mbtiles_drv.GetMetadataItem("ENABLE_SQL_SQLITE_FORMAT") != 'YES':
        return 'skip'

    if gdal.GetDriverByName('PNG') is None:
        return 'skip'
    ds = gdal.Open('data/byte.mbtiles.sql')
    if ds.GetRasterBand(1).Checksum() != 4118:
        gdaltest.post_reason('validation failed')
        return 'fail'

    return 'success'

###############################################################################


def mbtiles_raster_open_in_vector_mode():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    ds = ogr.Open('data/byte.mbtiles')
    if ds is not None:
        return 'fail'

    return 'success'

###############################################################################


def mbtiles_create():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    filename = '/vsimem/mbtiles_create.mbtiles'
    gdaltest.mbtiles_drv.Create(filename, 1, 1, 1)
    with gdaltest.error_handler():
        if gdal.Open(filename) is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    # Nominal case
    gdal.Unlink(filename)
    src_ds = gdal.Open('data/byte.mbtiles')
    ds = gdaltest.mbtiles_drv.Create(filename, src_ds.RasterXSize, src_ds.RasterYSize)
    ds.SetGeoTransform(src_ds.GetGeoTransform())
    ds.SetProjection(src_ds.GetProjectionRef())

    # Cannot modify geotransform once set"
    with gdaltest.error_handler():
        ret = ds.SetGeoTransform(src_ds.GetGeoTransform())
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = gdal.Open('data/byte.mbtiles')
    # SetGeoTransform() not supported on read-only dataset"
    with gdaltest.error_handler():
        ret = ds.SetGeoTransform(src_ds.GetGeoTransform())
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    # SetProjection() not supported on read-only dataset
    with gdaltest.error_handler():
        ret = ds.SetProjection(src_ds.GetProjectionRef())
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink(filename)
    ds = gdaltest.mbtiles_drv.Create(filename, src_ds.RasterXSize, src_ds.RasterYSize)
    # Only EPSG:3857 supported on MBTiles dataset
    with gdaltest.error_handler():
        ret = ds.SetProjection('LOCAL_CS["foo"]')
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink(filename)
    ds = gdaltest.mbtiles_drv.Create(filename, src_ds.RasterXSize, src_ds.RasterYSize)
    # Only north-up non rotated geotransform supported
    with gdaltest.error_handler():
        ret = ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink(filename)
    ds = gdaltest.mbtiles_drv.Create(filename, src_ds.RasterXSize, src_ds.RasterYSize)
    # Could not find an appropriate zoom level that matches raster pixel size
    with gdaltest.error_handler():
        ret = ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink(filename)

    return 'success'


###############################################################################
# Cleanup

def mbtiles_cleanup():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    return 'success'


gdaltest_list = [
    mbtiles_1,
    mbtiles_2,
    mbtiles_3,
    mbtiles_start_webserver,
    mbtiles_http_jpeg_three_bands,
    mbtiles_http_jpeg_single_band,
    mbtiles_http_png,
    mbtiles_stop_webserver,
    mbtiles_4,
    mbtiles_5,
    mbtiles_6,
    mbtiles_7,
    mbtiles_8,
    mbtiles_9,
    mbtiles_10,
    mbtiles_11,
    mbtiles_raster_open_in_vector_mode,
    mbtiles_create,
    mbtiles_cleanup]

# gdaltest_list = [ mbtiles_1, mbtiles_create ]

if __name__ == '__main__':

    gdaltest.setup_run('mbtiles')

    gdaltest.run_tests(gdaltest_list)

    gdaltest.summarize()
