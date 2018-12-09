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


import gdaltest
import webserver

###############################################################################
# Get the mbtiles driver


def test_mbtiles_1():

    gdaltest.mbtiles_drv = gdal.GetDriverByName('MBTiles')

    return 'success'

###############################################################################
# Basic test


def test_mbtiles_2():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('JPEG') is None:
        return 'skip'

    ds = gdal.OpenEx('data/world_l1.mbtiles', open_options=['USE_BOUNDS=NO'])
    assert ds is not None

    assert ds.RasterCount == 4, 'expected 3 bands'

    assert ds.GetRasterBand(1).GetOverviewCount() == 1, \
        'did not get expected overview count'

    expected_cs_tab = [6324, 19386, 45258]
    expected_cs_tab_jpeg8 = [6016, 13996, 45168]
    expected_cs_tab_jpeg9b = [6016, 14034, 45168]
    for i in range(3):
        cs = ds.GetRasterBand(i + 1).Checksum()
        assert ds.GetRasterBand(i + 1).GetColorInterpretation() == gdal.GCI_RedBand + i, \
            'bad color interpretation'
        expected_cs = expected_cs_tab[i]
        assert cs == expected_cs or cs == expected_cs_tab_jpeg8[i] or cs == expected_cs_tab_jpeg9b[i], \
            ('for band %d, cs = %d, different from expected_cs = %d' % (i + 1, cs, expected_cs))

    expected_cs_tab = [16642, 15772, 10029]
    expected_cs_tab_jpeg8 = [16621, 14725, 8988]
    expected_cs_tab_jpeg9b = [16621, 14723, 8988]
    for i in range(3):
        cs = ds.GetRasterBand(i + 1).GetOverview(0).Checksum()
        expected_cs = expected_cs_tab[i]
        assert cs == expected_cs or cs == expected_cs_tab_jpeg8[i] or cs == expected_cs_tab_jpeg9b[i], \
            ('for overview of band %d, cs = %d, different from expected_cs = %d' % (i + 1, cs, expected_cs))

    assert ds.GetProjectionRef().find('3857') != -1, \
        ('projection_ref = %s' % ds.GetProjectionRef())

    gt = ds.GetGeoTransform()
    expected_gt = (-20037508.342789244, 78271.516964020484, 0.0, 20037508.342789244, 0.0, -78271.516964020484)
    for i in range(6):
        assert abs(gt[i] - expected_gt[i]) <= 1e-15, 'bad gt'

    md = ds.GetMetadata()
    assert md['bounds'] == '-180.0,-85,180,85', 'bad metadata'

    ds = None

    return 'success'

###############################################################################
# Open a /vsicurl/ DB


def test_mbtiles_3():

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


def test_mbtiles_start_webserver():

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


def test_mbtiles_http_jpeg_three_bands():

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
    assert ds is not None

    return 'success'

###############################################################################
#


def test_mbtiles_http_jpeg_single_band():

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
    assert ds is not None

    return 'success'

###############################################################################
#


def test_mbtiles_http_png():

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
    assert ds is not None

    return 'success'

###############################################################################
#


def test_mbtiles_stop_webserver():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('HTTP') is None:
        return 'skip'

    if gdaltest.webserver_port != 0:
        webserver.server_stop(gdaltest.webserver_process, gdaltest.webserver_port)

    return 'success'

###############################################################################
# Basic test without any option


def test_mbtiles_4():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('JPEG') is None:
        return 'skip'

    ds = gdal.Open('data/world_l1.mbtiles')
    assert ds is not None

    assert ds.RasterCount == 4, 'expected 4 bands'

    assert ds.GetRasterBand(1).GetOverviewCount() == 1, \
        'did not get expected overview count'

    assert ds.RasterXSize == 512 and ds.RasterYSize == 510, 'bad dimensions'

    gt = ds.GetGeoTransform()
    expected_gt = (-20037508.342789244, 78271.516964020484, 0.0, 19971868.880408563, 0.0, -78271.516964020484)
    for i in range(6):
        assert abs(gt[i] - expected_gt[i]) <= 1e-15, 'bad gt'

    ds = None

    return 'success'

###############################################################################
# Test write support of a single band dataset


def test_mbtiles_5():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdal.GetDriverByName('PNG') is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')
    gdaltest.mbtiles_drv.CreateCopy('/vsimem/mbtiles_5.mbtiles', src_ds)
    src_ds = None

    ds = gdal.OpenEx('/vsimem/mbtiles_5.mbtiles', open_options=['BAND_COUNT=2'])
    assert ds.RasterXSize == 19 and ds.RasterYSize == 19
    assert ds.RasterCount == 2
    got_gt = ds.GetGeoTransform()
    expected_gt = (-13095853.550435878, 76.437028285176254, 0.0, 4015708.8887064462, 0.0, -76.437028285176254)
    for i in range(6):
        assert abs(expected_gt[i] - got_gt[i]) <= 1e-6
    got_cs = ds.GetRasterBand(1).Checksum()
    assert got_cs == 4118
    got_cs = ds.GetRasterBand(2).Checksum()
    assert got_cs == 4406
    got_md = ds.GetMetadata()
    expected_md = {'ZOOM_LEVEL': '11', 'minzoom': '11', 'maxzoom': '11', 'name': 'mbtiles_5', 'format': 'png', 'bounds': '-117.6420540294745,33.89160566594387,-117.6290077648261,33.90243460427036', 'version': '1.1', 'type': 'overlay', 'description': 'mbtiles_5'}
    assert set(got_md.keys()) == set(expected_md.keys())
    for key in got_md:
        assert key == 'bounds' or got_md[key] == expected_md[key]
    ds = None

    gdal.Unlink('/vsimem/mbtiles_5.mbtiles')

    return 'success'

###############################################################################
# Test write support with options


def test_mbtiles_6():

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
    assert got_cs != 0
    got_md = ds.GetMetadata()
    expected_md = {'ZOOM_LEVEL': '11', 'minzoom': '11', 'maxzoom': '11', 'format': 'jpg', 'version': 'version', 'type': 'baselayer', 'name': 'name', 'description': 'description'}
    assert got_md == expected_md
    ds = None

    gdal.Unlink('tmp/mbtiles_6.mbtiles')

    return 'success'

###############################################################################
# Test building overview


def test_mbtiles_7():

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
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    expected_ovr_cs = [21179, 22577, 11996, 17849]
    got_ovr_cs = [ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(ds.RasterCount)]
    assert expected_ovr_cs == got_ovr_cs
    assert ds.GetMetadataItem('minzoom') == '0', ds.GetMetadata()
    ds = None

    ds = gdal.Open('/vsimem/mbtiles_7.mbtiles', gdal.GA_Update)
    ds.BuildOverviews('NONE', [])
    ds = None

    ds = gdal.Open('/vsimem/mbtiles_7.mbtiles')
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetMetadataItem('minzoom') == '1', ds.GetMetadata()
    ds = None

    gdal.Unlink('/vsimem/mbtiles_7.mbtiles')

    return 'success'

###############################################################################
# Single band with 24 bit color table, PNG


def test_mbtiles_8():

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
    assert got_cs == expected_cs
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    assert got_ct is None
    assert out_ds.GetRasterBand(1).GetBlockSize() == [256, 256]
    out_ds = None

    # 512 pixel tiles
    src_ds = gdal.Open('data/small_world_pct.tif')
    out_ds = gdaltest.mbtiles_drv.CreateCopy('/vsimem/mbtiles_8.mbtiles', src_ds, options=['RESAMPLING=NEAREST', 'BLOCKSIZE=512'])
    out_ds = None
    src_ds = None

    expected_cs = [60844, 7388, 53813]
    out_ds = gdal.Open('/vsimem/mbtiles_8.mbtiles')
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert got_cs == expected_cs
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    assert got_ct is None
    assert out_ds.GetRasterBand(1).GetBlockSize() == [512, 512]
    out_ds = None

    gdal.Unlink('/vsimem/mbtiles_8.mbtiles')
    return 'success'

###############################################################################
# Test we are robust to invalid bounds


def test_mbtiles_9():

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
    assert ds.RasterXSize == 256 and ds.RasterYSize == 256
    assert abs(ds.GetGeoTransform()[0] - -13110479.091473430395126) <= 1e-6
    ds = None

    gdal.Unlink('/vsimem/mbtiles_9.mbtiles')
    return 'success'

###############################################################################
# Test compaction of temporary database


def test_mbtiles_10():

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
    assert cs == 29925
    ds = None

    gdal.Unlink('/vsimem/mbtiles_10.mbtiles')
    return 'success'

###############################################################################
# Test opening a .mbtiles.sql file


def test_mbtiles_11():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    if gdaltest.mbtiles_drv.GetMetadataItem("ENABLE_SQL_SQLITE_FORMAT") != 'YES':
        return 'skip'

    if gdal.GetDriverByName('PNG') is None:
        return 'skip'
    ds = gdal.Open('data/byte.mbtiles.sql')
    assert ds.GetRasterBand(1).Checksum() == 4118, 'validation failed'

    return 'success'

###############################################################################


def test_mbtiles_raster_open_in_vector_mode():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    ds = ogr.Open('data/byte.mbtiles')
    assert ds is None

    return 'success'

###############################################################################


def test_mbtiles_create():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    filename = '/vsimem/mbtiles_create.mbtiles'
    gdaltest.mbtiles_drv.Create(filename, 1, 1, 1)
    with gdaltest.error_handler():
        assert gdal.Open(filename) is None

    # Nominal case
    gdal.Unlink(filename)
    src_ds = gdal.Open('data/byte.mbtiles')
    ds = gdaltest.mbtiles_drv.Create(filename, src_ds.RasterXSize, src_ds.RasterYSize)
    ds.SetGeoTransform(src_ds.GetGeoTransform())
    ds.SetProjection(src_ds.GetProjectionRef())

    # Cannot modify geotransform once set"
    with gdaltest.error_handler():
        ret = ds.SetGeoTransform(src_ds.GetGeoTransform())
    assert ret != 0
    ds = None

    ds = gdal.Open('data/byte.mbtiles')
    # SetGeoTransform() not supported on read-only dataset"
    with gdaltest.error_handler():
        ret = ds.SetGeoTransform(src_ds.GetGeoTransform())
    assert ret != 0
    # SetProjection() not supported on read-only dataset
    with gdaltest.error_handler():
        ret = ds.SetProjection(src_ds.GetProjectionRef())
    assert ret != 0
    ds = None

    gdal.Unlink(filename)
    ds = gdaltest.mbtiles_drv.Create(filename, src_ds.RasterXSize, src_ds.RasterYSize)
    # Only EPSG:3857 supported on MBTiles dataset
    with gdaltest.error_handler():
        ret = ds.SetProjection('LOCAL_CS["foo"]')
    assert ret != 0
    ds = None

    gdal.Unlink(filename)
    ds = gdaltest.mbtiles_drv.Create(filename, src_ds.RasterXSize, src_ds.RasterYSize)
    # Only north-up non rotated geotransform supported
    with gdaltest.error_handler():
        ret = ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    assert ret != 0
    ds = None

    gdal.Unlink(filename)
    ds = gdaltest.mbtiles_drv.Create(filename, src_ds.RasterXSize, src_ds.RasterYSize)
    # Could not find an appropriate zoom level that matches raster pixel size
    with gdaltest.error_handler():
        ret = ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    assert ret != 0
    ds = None

    gdal.Unlink(filename)

    return 'success'


###############################################################################
# Cleanup

def test_mbtiles_cleanup():

    if gdaltest.mbtiles_drv is None:
        return 'skip'

    return 'success'


gdaltest_list = [
    test_mbtiles_1,
    test_mbtiles_2,
    test_mbtiles_3,
    test_mbtiles_start_webserver,
    test_mbtiles_http_jpeg_three_bands,
    test_mbtiles_http_jpeg_single_band,
    test_mbtiles_http_png,
    test_mbtiles_stop_webserver,
    test_mbtiles_4,
    test_mbtiles_5,
    test_mbtiles_6,
    test_mbtiles_7,
    test_mbtiles_8,
    test_mbtiles_9,
    test_mbtiles_10,
    test_mbtiles_11,
    test_mbtiles_raster_open_in_vector_mode,
    test_mbtiles_create,
    test_mbtiles_cleanup]

# gdaltest_list = [ mbtiles_1, mbtiles_create ]

if __name__ == '__main__':

    gdaltest.setup_run('mbtiles')

    gdaltest.run_tests(gdaltest_list)

    sys.exit(gdaltest.summarize())
