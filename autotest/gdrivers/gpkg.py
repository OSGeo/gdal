#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GeoPackage raster functionality.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
# 
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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

# Make sure we run from the directory of the script
if os.path.basename(sys.argv[0]) == os.path.basename(__file__):
    if os.path.dirname(sys.argv[0]) != '':
        os.chdir(os.path.dirname(sys.argv[0]))

sys.path.append( '../pymod' )

from osgeo import osr, gdal
import gdaltest

###############################################################################
# Test if GPKG and tile drivers are available

def gpkg_init():

    gdaltest.gpkg_dr = None

    try:
        gdaltest.gpkg_dr = gdal.GetDriverByName( 'GPKG' )
        if gdaltest.gpkg_dr is None:
            return 'skip'
    except:
        return 'skip'

    try:
        gdaltest.png_dr = gdal.GetDriverByName( 'PNG' )
    except:
        gdaltest.png_dr = None

    try:
        gdaltest.jpeg_dr = gdal.GetDriverByName( 'JPEG' )
    except:
        gdaltest.jpeg_dr = None

    try:
        gdaltest.webp_dr = gdal.GetDriverByName( 'WEBP' )
    except:
        gdaltest.webp_dr = None
    gdaltest.webp_supports_rgba = False
    if gdaltest.webp_dr is not None and gdal.GetConfigOption("GPKG_SIMUL_WEBP_3BAND") is None:
        md = gdaltest.webp_dr.GetMetadata()
        if md['DMD_CREATIONOPTIONLIST'].find('LOSSLESS') >= 0:
            gdaltest.webp_supports_rgba = True

    # This is to speed-up the runtime of tests on EXT4 filesystems
    # Do not use this for production environment if you care about data safety
    # w.r.t system/OS crashes, unless you know what you are doing.
    gdal.SetConfigOption('OGR_SQLITE_SYNCHRONOUS', 'OFF')
    
    gdal.SetConfigOption('GPKG_DEBUG', 'ON')

    return 'success'

###############################################################################
#
def get_expected_checksums(src_ds, tile_drv, working_bands, extend_src = True, clamp_output = True):
    if extend_src:
        mem_ds = gdal.GetDriverByName('MEM').Create('', 256, 256, working_bands)
    else:
        mem_ds = gdal.GetDriverByName('MEM').Create('', src_ds.RasterXSize, src_ds.RasterYSize, working_bands)
    for i in range(working_bands):
        if src_ds.RasterCount == 1:
            src_band = 1
        else:
            src_band = i + 1
        data = src_ds.GetRasterBand(src_band).ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize)
        mem_ds.GetRasterBand(i+1).WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, data)
    if tile_drv.ShortName == 'PNG':
        options = []
    else:
        options = ['QUALITY=75']
    tmp_ds = tile_drv.CreateCopy('/vsimem/tmp.' + tile_drv.ShortName, mem_ds, options = options)
    if clamp_output:
        mem_ds = gdal.GetDriverByName('MEM').Create('', src_ds.RasterXSize, src_ds.RasterYSize, working_bands)
        mem_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize,
                        tmp_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize))
        expected_cs = [mem_ds.GetRasterBand(i+1).Checksum() for i in range(working_bands)]
    else:
        tmp_ds.FlushCache()
        expected_cs = [tmp_ds.GetRasterBand(i+1).Checksum() for i in range(working_bands)]
    mem_ds = None
    tmp_ds = None
    gdal.Unlink('/vsimem/tmp.' + tile_drv.ShortName)
    return expected_cs

###############################################################################
#
def check_tile_format(out_ds, expected_format, expected_band_count, expected_ct, row = 0, col = 0, zoom_level = None):
    if zoom_level is None:
        zoom_level_str = "(SELECT MAX(zoom_level) FROM tmp)"
    else:
        zoom_level_str = str(zoom_level)
    sql_lyr = out_ds.ExecuteSQL('SELECT GDAL_GetMimeType(tile_data), ' +
                                'GDAL_GetBandCount(tile_data), ' +
                                'GDAL_HasColorTable(tile_data) FROM tmp ' + 
                                'WHERE zoom_level = %s AND tile_column = %d AND tile_row = %d' % (zoom_level_str, col, row))
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        mime_type = feat.GetField(0)
        band_count = feat.GetField(1)
        has_ct = feat.GetField(2)
    else:
        mime_type = None
        band_count = None
        has_ct = None
    out_ds.ReleaseResultSet(sql_lyr)
    out_ds = None

    if expected_format is None:
        if mime_type is None:
            return 'success'
        else:
            return 'fail'

    if expected_format == 'PNG':
        expected_mime_type = 'image/png'
    elif expected_format == 'JPEG':
        expected_mime_type = 'image/jpeg'
    elif expected_format == 'WEBP':
        expected_mime_type = 'image/x-webp'

    if mime_type != expected_mime_type:
        gdaltest.post_reason('fail')
        print(mime_type)
        return 'fail'
    if band_count != expected_band_count:
        gdaltest.post_reason('fail')
        print(band_count)
        return 'fail'
    if expected_ct != has_ct:
        gdaltest.post_reason('fail')
        print(has_ct)
        return 'fail'
    return 'success'

###############################################################################
# Single band, PNG

def gpkg_1():

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if gdaltest.png_dr is None: 
        return 'skip'

    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    # With padding
    ds = gdal.Open('data/byte.tif')
    expected_cs = ds.GetRasterBand(1).Checksum()
    clamped_expected_cs = get_expected_checksums(ds, gdaltest.png_dr, 1, clamp_output = False)[0]
    expected_gt = ds.GetGeoTransform()
    expected_wkt = ds.GetProjectionRef()
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=PNG'] )
    out_ds = None
    ds = None

    out_ds = gdal.Open('tmp/tmp.gpkg')
    got_gt = out_ds.GetGeoTransform()
    for i in range(6):
        if abs(expected_gt[i]-got_gt[i])>1e-8:
            gdaltest.post_reason('fail')
            return 'fail'
    got_wkt = out_ds.GetProjectionRef()
    if expected_wkt != got_wkt:
        gdaltest.post_reason('fail')
        return 'fail'
    expected_cs = [ expected_cs, expected_cs, expected_cs, 4873 ]
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 4, False) != 'success':
        return 'fail'

    # Check that there's no extensions
    out_ds = gdal.Open('tmp/tmp.gpkg')
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE type = 'table' AND name = 'gpkg_extensions'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds = None

    out_ds = gdal.OpenEx('tmp/tmp.gpkg', open_options= ['BAND_COUNT=3'])
    expected_cs = expected_cs[0:3]
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(out_ds.RasterCount)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    out_ds = None

    ds = gdal.OpenEx('tmp/tmp.gpkg', open_options = ['USE_TILE_EXTENT=YES'])
    if ds.RasterXSize != 256 or ds.RasterYSize != 256:
        gdaltest.post_reason('fail')
        return 'fail'
    expected_cs = [clamped_expected_cs,clamped_expected_cs,clamped_expected_cs,4898]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    ds = None

    os.remove('tmp/tmp.gpkg')

    # Without padding
    ds = gdal.Open('data/byte.tif')
    expected_cs = ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=PNG', 'BLOCKSIZE=20'] )
    out_ds = None
    
    out_ds = gdal.Open('tmp/tmp.gpkg')
    expected_cs = [ expected_cs, expected_cs, expected_cs, 4873 ]
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 1, False) != 'success':
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    return 'success'

###############################################################################
# Single band, JPEG

def gpkg_2():

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if gdaltest.jpeg_dr is None: 
        return 'skip'

    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    # With padding
    ds = gdal.Open('data/byte.tif')
    expected_cs = get_expected_checksums(ds, gdaltest.jpeg_dr, 1)[0]
    clamped_expected_cs = get_expected_checksums(ds, gdaltest.jpeg_dr, 3, clamp_output = False)
    clamped_expected_cs.append(17849)

    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=JPEG'] )
    out_ds = None

    out_ds = gdal.Open('tmp/tmp.gpkg')
    expected_cs = [ expected_cs, expected_cs, expected_cs, 4873 ]
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, 'JPEG', 1, False) != 'success':
        return 'fail'

    # Check that there's no extensions
    out_ds = gdal.Open('tmp/tmp.gpkg')
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE type = 'table' AND name = 'gpkg_extensions'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds = None

    ds = gdal.OpenEx('tmp/tmp.gpkg', open_options = ['USE_TILE_EXTENT=YES'])
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != clamped_expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(clamped_expected_cs)))
        return 'fail'
    ds = None

    os.remove('tmp/tmp.gpkg')

    # Without padding
    ds = gdal.Open('data/byte.tif')
    expected_cs = get_expected_checksums(ds, gdaltest.jpeg_dr, 1, extend_src = False)[0]
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=JPEG', 'BLOCKSIZE=20'] )
    out_ds = None
    
    out_ds = gdal.Open('tmp/tmp.gpkg')
    expected_cs = [ expected_cs, expected_cs, expected_cs, 4873 ]
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, 'JPEG', 1, False) != 'success':
        return 'fail'

    # Try deregistering JPEG driver
    gdaltest.jpeg_dr.Deregister()

    out_ds = gdal.Open('tmp/tmp.gpkg')
    # Should give warning at pixel reading time
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    out_ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')
    
    ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=JPEG'] )
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    out_ds.FlushCache()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Re-register driver
    gdaltest.jpeg_dr.Register()

    os.remove('tmp/tmp.gpkg')

    return 'success'

###############################################################################
# Single band, WEBP

def gpkg_3():

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if gdaltest.webp_dr is None: 
        return 'skip'

    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    ds = gdal.Open('data/byte.tif')
    expected_cs = get_expected_checksums(ds, gdaltest.webp_dr, 3)
    clamped_expected_cs = get_expected_checksums(ds, gdaltest.webp_dr, 3, clamp_output = False)
    if gdaltest.webp_supports_rgba:
        clamped_expected_cs.append(4898)
    else:
        clamped_expected_cs.append(17849)

    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=WEBP'] )
    out_ds = None

    out_ds = gdal.OpenEx('tmp/tmp.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'

    # Check that extension is declared
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_extensions WHERE table_name = 'tmp' AND column_name = 'tile_data' AND extension_name = 'gpkg_webp'")
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)

    if gdaltest.webp_supports_rgba:
        expected_band_count = 4
    else:
        expected_band_count = 3
    if check_tile_format(out_ds, 'WEBP', expected_band_count, False) != 'success':
        return 'fail'

    out_ds = None

    ds = gdal.OpenEx('tmp/tmp.gpkg', open_options = ['USE_TILE_EXTENT=YES'])
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != clamped_expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(clamped_expected_cs)))
        return 'fail'
    ds = None

    os.remove('tmp/tmp.gpkg')

    # Without padding
    ds = gdal.Open('data/byte.tif')
    expected_cs = get_expected_checksums(ds, gdaltest.webp_dr, 3, extend_src = False)
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=WEBP', 'BLOCKSIZE=20'] )
    out_ds = None
    
    out_ds = gdal.Open('tmp/tmp.gpkg')
    expected_cs.append(4873)
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, 'WEBP', 3, False) != 'success':
        return 'fail'

    # Try deregistering WEBP driver
    gdaltest.webp_dr.Deregister()

    # Should give warning at open time since the webp extension is declared
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    out_ds = gdal.Open('tmp/tmp.gpkg')
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    # And at pixel reading time as well
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    out_ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Re-register driver
    gdaltest.webp_dr.Register()

    os.remove('tmp/tmp.gpkg')

    # Check updating a non-WEBP dataset with DRIVER=WEBP
    out_ds = gdaltest.gpkg_dr.Create('tmp/tmp.gpkg', 1, 1 )
    out_ds.SetGeoTransform([0,1,0,0,0,-1])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    out_ds.SetProjection(srs.ExportToWkt())
    out_ds = None

    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE, open_options=['DRIVER=WEBP'])
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_extensions WHERE table_name = 'tmp' AND column_name = 'tile_data' AND extension_name = 'gpkg_webp'")
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    return 'success'

###############################################################################
# Three band, PNG

def gpkg_4(tile_drv_name = 'PNG'):

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if tile_drv_name == 'PNG':
        tile_drv = gdaltest.png_dr
        working_bands = 4
    elif tile_drv_name == 'JPEG':
        tile_drv = gdaltest.jpeg_dr
        working_bands = 3
    elif tile_drv_name == 'WEBP':
        tile_drv = gdaltest.webp_dr
        if gdaltest.webp_supports_rgba:
            working_bands = 4
        else:
            working_bands = 3
    if tile_drv is None: 
        return 'skip'

    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    ds = gdal.Open('data/rgbsmall.tif')
    expected_cs = get_expected_checksums(ds, tile_drv, 3)
    clamped_expected_cs = get_expected_checksums(ds, tile_drv, 3, clamp_output = False)
    if working_bands == 3:
        clamped_expected_cs.append(17849)
    else:
        clamped_expected_cs.append(30638)

    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=' + tile_drv_name] )
    ds = None
    out_ds = None

    out_ds = gdal.Open('tmp/tmp.gpkg')
    expected_cs.append(30658)
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, tile_drv_name, working_bands, False) != 'success':
        return 'fail'
    out_ds = None

    ds = gdal.OpenEx('tmp/tmp.gpkg', open_options = ['USE_TILE_EXTENT=YES'])
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != clamped_expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(clamped_expected_cs)))
        return 'fail'
    ds = None

    os.remove('tmp/tmp.gpkg')

    # Without padding
    ds = gdal.Open('data/rgbsmall.tif')
    expected_cs = get_expected_checksums(ds, tile_drv, 3, extend_src = False)
    expected_cs.append(30658)
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=' + tile_drv_name, 'BLOCKSIZE=50'] )
    out_ds = None
    
    out_ds = gdal.Open('tmp/tmp.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, tile_drv_name, 3, False) != 'success':
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    return 'success'

###############################################################################
# Three band, JPEG

def gpkg_5():
    return gpkg_4(tile_drv_name = 'JPEG')

###############################################################################
# Three band, WEBP

def gpkg_6():
    return gpkg_4(tile_drv_name = 'WEBP')

def get_georeferenced_rgba_ds(alpha_fully_transparent = False, alpha_fully_opaque = False):
    assert(not (alpha_fully_transparent and alpha_fully_opaque))
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    tmp_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tmp.tif',
                                    src_ds.RasterXSize, src_ds.RasterYSize, 4)
    tmp_ds.SetGeoTransform([0,10,0,0,0,-10])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    tmp_ds.SetProjection(srs.ExportToWkt())
    tmp_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize,
                       src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize))
    if alpha_fully_opaque:
        tmp_ds.GetRasterBand(4).Fill(255)
    elif alpha_fully_transparent:
        tmp_ds.GetRasterBand(4).Fill(0)
    return tmp_ds

###############################################################################
# 4 band, PNG

def gpkg_7(tile_drv_name = 'PNG'):

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if tile_drv_name == 'PNG':
        tile_drv = gdaltest.png_dr
        working_bands = 4
    elif tile_drv_name == 'JPEG':
        tile_drv = gdaltest.jpeg_dr
        working_bands = 3
    elif tile_drv_name == 'WEBP':
        tile_drv = gdaltest.webp_dr
        if gdaltest.webp_supports_rgba:
            working_bands = 4
        else:
            working_bands = 3
    if tile_drv is None: 
        return 'skip'
    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    src_ds = get_georeferenced_rgba_ds()
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', src_ds, options = ['DRIVER=' + tile_drv_name] )
    out_ds = None

    expected_cs = get_expected_checksums(src_ds, tile_drv, working_bands)

    src_filename = src_ds.GetDescription()
    src_ds = None
    gdal.Unlink(src_filename)
    
    out_ds = gdal.Open('tmp/tmp.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(working_bands)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, tile_drv_name, working_bands, False) != 'success':
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    # Without padding with alpha fully opaque
    tmp_ds = get_georeferenced_rgba_ds(alpha_fully_opaque = True)
    expected_cs = get_expected_checksums(tmp_ds, tile_drv, 3, extend_src = False)
    tmp_filename = tmp_ds.GetDescription()
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', tmp_ds, options = ['DRIVER=' + tile_drv_name, 'BLOCKXSIZE=%d' % tmp_ds.RasterXSize, 'BLOCKYSIZE=%d' % tmp_ds.RasterYSize] )
    out_ds = None
    tmp_ds = None
    gdal.Unlink(tmp_filename)
    
    out_ds = gdal.Open('tmp/tmp.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, tile_drv_name, 3, False) != 'success':
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    # Without padding with alpha fully transparent
    tmp_ds = get_georeferenced_rgba_ds(alpha_fully_transparent = True)
    tmp_filename = tmp_ds.GetDescription()
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', tmp_ds, options = ['DRIVER=' + tile_drv_name, 'BLOCKXSIZE=%d' % tmp_ds.RasterXSize, 'BLOCKYSIZE=%d' % tmp_ds.RasterYSize] )
    out_ds = None
    tmp_ds = None
    gdal.Unlink(tmp_filename)
    
    out_ds = gdal.Open('tmp/tmp.gpkg')
    expected_cs = [0, 0, 0, 0]
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, None, None, None) != 'success':
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    return 'success'

###############################################################################
# 4 band, JPEG

def gpkg_8():
    return gpkg_7(tile_drv_name = 'JPEG')

###############################################################################
# 4 band, WEBP

def gpkg_9():
    return gpkg_7(tile_drv_name = 'WEBP')

###############################################################################
#
def get_georeferenced_ds_with_pct32():
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba_pct32.png')
    tmp_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tmp.tif',
                                    src_ds.RasterXSize, src_ds.RasterYSize)
    tmp_ds.SetGeoTransform([0,10,0,0,0,-10])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    tmp_ds.SetProjection(srs.ExportToWkt())
    tmp_ds.GetRasterBand(1).SetColorTable(src_ds.GetRasterBand(1).GetColorTable())
    tmp_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize,
                       src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize))
    return tmp_ds

###############################################################################
# Single band with 32 bit color table, PNG

def gpkg_10():

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if gdaltest.png_dr is None: 
        return 'skip'

    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    tmp_ds = get_georeferenced_ds_with_pct32()
    expected_ct = tmp_ds.GetRasterBand(1).GetColorTable().Clone()
    expected_cs_single_band = tmp_ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', tmp_ds, options = ['BLOCKXSIZE=%d' % tmp_ds.RasterXSize, 'BLOCKYSIZE=%d' % tmp_ds.RasterYSize]  )
    out_ds = None
    tmp_filename = tmp_ds.GetDescription()
    tmp_ds = None
    gdal.Unlink(tmp_filename)

    expected_cs = [ 10991, 57677, 34965, 10638 ]
    out_ds = gdal.Open('tmp/tmp.gpkg')
    block_size = out_ds.GetRasterBand(1).GetBlockSize()
    if block_size != [out_ds.RasterXSize, out_ds.RasterYSize]:
        gdaltest.post_reason('fail')
        print(block_size)
        return 'fail'
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 1, True) != 'success':
        return 'fail'
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    if got_ct is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # SetColorTable() on a non single-band dataset
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds.GetRasterBand(1).SetColorTable(None)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    out_ds = None

    expected_cs = [ expected_cs_single_band ]
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', open_options = ['BAND_COUNT=1'])
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(out_ds.RasterCount)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    if expected_ct.GetCount() != got_ct.GetCount():
        gdaltest.post_reason('fail')
        return 'fail'

    # SetColorTable() on a re-opened dataset
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds.GetRasterBand(1).SetColorTable(None)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    out_ds = None

    os.remove('tmp/tmp.gpkg')

    # Repeated SetColorTable()
    out_ds = gdaltest.gpkg_dr.Create('tmp/tmp.gpkg',1,1)
    out_ds.GetRasterBand(1).SetColorTable(None)

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    out_ds.GetRasterBand(1).SetColorTable(None)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    out_ds = None
    gdal.PopErrorHandler()

    os.remove('tmp/tmp.gpkg')

    # Partial tile
    tmp_ds = get_georeferenced_ds_with_pct32()
    expected_ct = tmp_ds.GetRasterBand(1).GetColorTable().Clone()
    expected_cs_single_band = tmp_ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', tmp_ds)
    out_ds = None
    tmp_filename = tmp_ds.GetDescription()
    tmp_ds = None
    gdal.Unlink(tmp_filename)

    expected_cs = [ 10991, 57677, 34965, 10638 ]
    out_ds = gdal.Open('tmp/tmp.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 4, False) != 'success':
        return 'fail'
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    if got_ct is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Single band with 32 bit color table, JPEG

def gpkg_11(tile_drv_name = 'JPEG'):

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if tile_drv_name == 'JPEG':
        tile_drv = gdaltest.jpeg_dr
        working_bands = 3
    elif tile_drv_name == 'WEBP':
        tile_drv = gdaltest.webp_dr
        if gdaltest.webp_supports_rgba:
            working_bands = 4
        else:
            working_bands = 3
    if tile_drv is None: 
        return 'skip'

    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    rgba_xml = '<VRTDataset rasterXSize="162" rasterYSize="150">'
    for i in range(4):
        rgba_xml += """<VRTRasterBand dataType="Byte" band="%d">
    <ComplexSource>
      <SourceFilename relativeToVRT="0">../gcore/data/stefan_full_rgba_pct32.png</SourceFilename>
      <SourceBand>1</SourceBand>
      <ColorTableComponent>%d</ColorTableComponent>
    </ComplexSource>
  </VRTRasterBand>""" % (i+1, i+1)
    rgba_xml += '</VRTDataset>'
    rgba_ds = gdal.Open(rgba_xml)

    tmp_ds = get_georeferenced_ds_with_pct32()
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', tmp_ds, options = ['DRIVER=' + tile_drv_name] )
    out_ds = None
    tmp_filename = tmp_ds.GetDescription()
    tmp_ds = None
    gdal.Unlink(tmp_filename)

    expected_cs = get_expected_checksums(rgba_ds, tile_drv, working_bands)
    rgba_ds = None

    out_ds = gdal.Open('tmp/tmp.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(working_bands)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    return 'success'

###############################################################################
# Single band with 32 bit color table, WEBP

def gpkg_12():
    return gpkg_11(tile_drv_name = 'WEBP')

###############################################################################
# Single band with 24 bit color table, PNG

def gpkg_13():

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if gdaltest.png_dr is None: 
        return 'skip'

    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    src_ds = gdal.Open('data/small_world_pct.tif')
    expected_ct = src_ds.GetRasterBand(1).GetColorTable().Clone()
    expected_cs_single_band = src_ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', src_ds, options = ['BLOCKXSIZE=%d' % src_ds.RasterXSize, 'BLOCKYSIZE=%d' % src_ds.RasterYSize]  )
    out_ds = None
    src_ds = None

    expected_cs = [ 63025, 48175, 12204 ]
    out_ds = gdal.Open('tmp/tmp.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    if got_ct is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    expected_cs = [ expected_cs_single_band ]
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', open_options = ['BAND_COUNT=1'])
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(out_ds.RasterCount)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    if expected_ct.GetCount() != got_ct.GetCount():
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    # Partial tile
    src_ds = gdal.Open('data/small_world_pct.tif')
    expected_ct = src_ds.GetRasterBand(1).GetColorTable().Clone()
    expected_cs_single_band = src_ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', src_ds)
    out_ds = None
    src_ds = None

    ds = gdal.OpenEx('tmp/tmp.gpkg', open_options = ['USE_TILE_EXTENT=YES'])
    if ds.RasterXSize != 512 or ds.RasterYSize != 256:
        gdaltest.post_reason('fail')
        return 'fail'
    expected_cs = [62358, 45823, 12238, 64301]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    ds = None

    os.remove('tmp/tmp.gpkg')
    return 'success'

###############################################################################
# Test creation and opening options

def gpkg_14():

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if gdaltest.png_dr is None: 
        return 'skip'

    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    src_ds = gdal.Open('data/small_world.tif')
    ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', src_ds, options = ['RASTER_TABLE=foo', 'RASTER_IDENTIFIER=bar', 'RASTER_DESCRIPTION=baz'])
    ds = None

    ds = gdal.Open('tmp/tmp.gpkg')
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents WHERE table_name='foo'")
    feat_count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    if feat_count != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetMetadataItem('IDENTIFIER') != 'bar':
        gdaltest.post_reason('fail')
        print(ds.GetMetadata())
        return 'fail'
    if ds.GetMetadataItem('DESCRIPTION') != 'baz':
        gdaltest.post_reason('fail')
        print(ds.GetMetadata())
        return 'fail'
    if ds.GetMetadataItem('ZOOM_LEVEL') != '1':
        gdaltest.post_reason('fail')
        print(ds.GetMetadata())
        return 'fail'
    if ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverview(0) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    # In update mode, we expose even empty overview levels
    ds = gdal.Open('tmp/tmp.gpkg', gdal.GA_Update)
    if ds.GetMetadataItem('ZOOM_LEVEL') != '1':
        gdaltest.post_reason('fail')
        print(ds.GetMetadata())
        return 'fail'
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverview(0) is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverview(0).Checksum() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    
    ds = gdal.OpenEx('tmp/tmp.gpkg', open_options = ['ZOOM_LEVEL=2'])
    if ds.RasterXSize != 400:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = gdal.OpenEx('tmp/tmp.gpkg', open_options = ['ZOOM_LEVEL=1'])
    if ds.RasterXSize != 400:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    # In update mode, we expose even empty overview levels
    ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_UPDATE, open_options = ['ZOOM_LEVEL=1'])
    if ds.RasterXSize != 400:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = gdal.OpenEx('tmp/tmp.gpkg', open_options = ['ZOOM_LEVEL=0'])
    if ds.RasterXSize != 200:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = gdal.OpenEx('tmp/tmp.gpkg', open_options = ['USE_TILE_EXTENT=YES'])
    if ds.RasterXSize != 512 or ds.RasterYSize != 256:
        gdaltest.post_reason('fail')
        return 'fail'
    expected_cs = [27644,31968,38564,64301]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    ds = None

    ds = gdal.OpenEx('tmp/tmp.gpkg', open_options = ['MINX=-410.4','MAXY=320.4'])
    if ds.RasterXSize != 400+256 or ds.RasterYSize != 200+256:
        gdaltest.post_reason('fail')
        return 'fail'
    expected_cs = [29070,32796,41086,64288]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    ds = None

    ds = gdal.OpenEx('GPKG:tmp/tmp.gpkg:foo', open_options = ['MINX=-90','MAXY=45','MINY=-45','MAXX=90'])
    if ds.RasterXSize != 200 or ds.RasterYSize != 100:
        gdaltest.post_reason('fail')
        return 'fail'
    expected_cs = [9586,9360,26758,48827]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    ds = None

    ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', src_ds, options = ['RASTER_TABLE=other', 'BLOCKSIZE=64', 'DRIVER=PNG'])
    ds = None

    ds = gdal.Open('tmp/tmp.gpkg')
    md = ds.GetMetadata('SUBDATASETS')
    if md['SUBDATASET_1_NAME'] != 'GPKG:tmp/tmp.gpkg:foo':
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    if md['SUBDATASET_1_DESC'] != 'foo - bar':
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    if md['SUBDATASET_2_NAME'] != 'GPKG:tmp/tmp.gpkg:other':
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    if md['SUBDATASET_2_DESC'] != 'other - other':
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    ds = None

    ds = gdal.Open('GPKG:tmp/tmp.gpkg:other')
    block_size = ds.GetRasterBand(1).GetBlockSize()
    if block_size != [64, 64]:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = gdal.OpenEx('tmp/tmp.gpkg', open_options = ['TABLE=other', 'MINX=-90','MAXY=45','MINY=-45','MAXX=90'])
    if ds.RasterXSize != 200 or ds.RasterYSize != 100:
        gdaltest.post_reason('fail')
        return 'fail'
    block_size = ds.GetRasterBand(1).GetBlockSize()
    if block_size != [64, 64]:
        gdaltest.post_reason('fail')
        return 'fail'
    expected_cs = [9586,9360,26758,48827]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    ds = None

    os.remove('tmp/tmp.gpkg')

    return 'success'

###############################################################################
# Test error cases

def gpkg_15():

    if gdaltest.gpkg_dr is None: 
        return 'skip'

    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    # SetGeoTransform() and SetProjection() on a non-raster GPKG
    out_ds = gdaltest.gpkg_dr.Create('tmp/tmp.gpkg',0,0,0)
    if out_ds.GetGeoTransform(can_return_null = True) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetProjectionRef() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ret = out_ds.SetGeoTransform([0,1,0,0,0,-1])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    gdal.PushErrorHandler()
    ret = out_ds.SetProjection(srs.ExportToWkt())
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    # Repeated SetGeoTransform() and SetProjection()
    out_ds = gdaltest.gpkg_dr.Create('tmp/tmp.gpkg',1,1)
    ret = out_ds.SetGeoTransform([0,1,0,0,0,-1])
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ret = out_ds.SetGeoTransform([0,1,0,0,0,-1])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    ret = out_ds.SetProjection(srs.ExportToWkt())
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ret = out_ds.SetProjection(srs.ExportToWkt())
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    # Test SetColorInterpretation()
    out_ds = gdaltest.gpkg_dr.Create('tmp/tmp.gpkg',1,1)
    out_ds.SetGeoTransform([0,1,0,0,0,-1])
    out_ds.SetProjection(srs.ExportToWkt())
    ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_Undefined)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_GrayIndex)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_PaletteIndex)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')
    
    out_ds = gdaltest.gpkg_dr.Create('tmp/tmp.gpkg',1,1,3)
    out_ds.SetGeoTransform([0,1,0,0,0,-1])
    out_ds.SetProjection(srs.ExportToWkt())
    ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ret = out_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_RedBand)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    return 'success'

###############################################################################
# Test block/tile caching

def gpkg_16():

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if gdaltest.jpeg_dr is None: 
        return 'skip'
    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    out_ds = gdaltest.gpkg_dr.Create('tmp/tmp.gpkg',1,1,3, options = ['DRIVER=JPEG'])
    out_ds.SetGeoTransform([0,1,0,0,0,-1])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    out_ds.SetProjection(srs.ExportToWkt())
    out_ds.GetRasterBand(1).Fill(255)
    out_ds.GetRasterBand(1).FlushCache()
    # Rewrite same tile after re-reading it
    # Will cause a debug message to be emitted
    out_ds.GetRasterBand(2).Fill(127)
    out_ds.GetRasterBand(3).Checksum()
    out_ds.GetRasterBand(2).FlushCache()
    out_ds = None
    
    out_ds = gdal.Open('tmp/tmp.gpkg')
    val1 = ord(out_ds.GetRasterBand(1).ReadRaster(0,0,1,1))
    val2 = ord(out_ds.GetRasterBand(2).ReadRaster(0,0,1,1))
    val3 = ord(out_ds.GetRasterBand(3).ReadRaster(0,0,1,1))
    out_ds = None
    
    if abs(val1-255)>1:
        gdaltest.post_reason('fail')
        print(val1)
        return 'fail'
    if abs(val2-127)>1:
        gdaltest.post_reason('fail')
        print(val2)
        return 'fail'
    if abs(val3-0)>1:
        gdaltest.post_reason('fail')
        print(val3)
        return 'fail'
    os.remove('tmp/tmp.gpkg')

    return 'success'

###############################################################################
# Test overviews with single band dataset

def gpkg_17():

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if gdaltest.png_dr is None: 
        return 'skip'
    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    # Without padding, immediately after create copy
    ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=PNG', 'BLOCKSIZE=10'] )
    out_ds.BuildOverviews('NEAR', [2])
    out_ds = None
    ds = None
    
    out_ds = gdal.Open('tmp/tmp.gpkg')
    got_cs = out_ds.GetRasterBand(1).GetOverview(0).Checksum()
    if got_cs != 1087:
        gdaltest.post_reason('fail')
        print(got_cs)
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 1, False, zoom_level = 0) != 'success':
        return 'fail'
    if out_ds.GetRasterBand(1).GetOverview(0).GetColorTable() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None
    os.remove('tmp/tmp.gpkg')

    # Without padding, after reopening, and BAND_COUNT = 1
    ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=PNG', 'BLOCKSIZE=10'] )
    out_ds = None
    # FIXME? Should we eventually write the driver somewhere in metadata ?
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE, open_options = ['DRIVER=PNG', 'BAND_COUNT=1'])
    out_ds.BuildOverviews('NEAR', [2])
    out_ds = None
    ds = None
    
    out_ds = gdal.Open('tmp/tmp.gpkg')
    got_cs = out_ds.GetRasterBand(1).GetOverview(0).Checksum()
    if got_cs != 1087:
        gdaltest.post_reason('fail')
        print(got_cs)
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 1, False, zoom_level = 0) != 'success':
        return 'fail'
    out_ds = None
    os.remove('tmp/tmp.gpkg')

    # Without padding, after reopening
    ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=PNG', 'BLOCKSIZE=10'] )
    out_ds = None
    # FIXME? Should we eventually write the driver somewhere in metadata ?
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE, open_options = ['DRIVER=PNG'])
    out_ds.BuildOverviews('NEAR', [2])
    out_ds = None
    ds = None
    
    out_ds = gdal.Open('tmp/tmp.gpkg')
    got_cs = out_ds.GetRasterBand(1).GetOverview(0).Checksum()
    if got_cs != 1087:
        gdaltest.post_reason('fail')
        print(got_cs)
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 3, False, zoom_level = 0) != 'success':
        return 'fail'

    # Check that there's no extensions
    out_ds = gdal.Open('tmp/tmp.gpkg')
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE type = 'table' AND name = 'gpkg_extensions'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds = None
    
    # Test clearing overviews
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    out_ds.BuildOverviews('NONE', [])
    out_ds = None
    out_ds = gdal.Open('tmp/tmp.gpkg')
    if out_ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Test building on an overview dataset --> error
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    gdal.PushErrorHandler()
    ret = out_ds.GetRasterBand(1).GetOverview(0).GetDataset().BuildOverviews('NONE', [])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Test building overview factor 1 --> error
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    gdal.PushErrorHandler()
    ret = out_ds.BuildOverviews('NEAR', [1])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Test building non-supported overview levels
    gdal.SetConfigOption('ALLOW_GPKG_ZOOM_OTHER_EXTENSION', 'NO')
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    gdal.SetConfigOption('ALLOW_GPKG_ZOOM_OTHER_EXTENSION', None)
    gdal.PushErrorHandler()
    ret = out_ds.BuildOverviews('NEAR', [3])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Test building non-supported overview levels
    gdal.SetConfigOption('ALLOW_GPKG_ZOOM_OTHER_EXTENSION', 'NO')
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    gdal.SetConfigOption('ALLOW_GPKG_ZOOM_OTHER_EXTENSION', None)
    gdal.PushErrorHandler()
    ret = out_ds.BuildOverviews('NEAR', [2, 4])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Test gpkg_zoom_other extension
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    # Will fail because results in a 6x6 overview
    gdal.PushErrorHandler()
    ret = out_ds.BuildOverviews('NEAR', [3])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Test building overviews on read-only dataset
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER)
    gdal.PushErrorHandler()
    ret = out_ds.BuildOverviews('NEAR', [2])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    return 'success'

###############################################################################
# Test overviews with 3 band dataset

def gpkg_18():

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if gdaltest.png_dr is None: 
        return 'skip'
    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    # Without padding, immediately after create copy
    ds = gdal.Open('data/small_world.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=PNG', 'BLOCKXSIZE=100', 'BLOCKYSIZE=100'] )
    out_ds.BuildOverviews('CUBIC', [2, 4])
    out_ds = None
    
    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/tmp.tif', ds)
    tmp_ds.BuildOverviews('CUBIC', [2, 4])
    expected_cs_ov0 = [tmp_ds.GetRasterBand(i+1).GetOverview(0).Checksum() for i in range(3)]
    expected_cs_ov1 = [tmp_ds.GetRasterBand(i+1).GetOverview(1).Checksum() for i in range(3)]
    #tmp_ds.BuildOverviews('NEAR', [3])
    #expected_cs_ov_factor3 = [tmp_ds.GetRasterBand(i+1).GetOverview(2).Checksum() for i in range(3)]
    tmp_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/tmp.tif')
    
    ds = None

    out_ds = gdal.Open('tmp/tmp.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).GetOverview(0).Checksum() for i in range(3)]
    if got_cs != expected_cs_ov0:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs_ov0)
        return 'fail'
    got_cs = [out_ds.GetRasterBand(i+1).GetOverview(1).Checksum() for i in range(3)]
    if got_cs != expected_cs_ov1:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs_ov1)
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 3, False, zoom_level = 1) != 'success':
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 4, False, zoom_level = 0) != 'success':
        return 'fail'
    out_ds = None

    # Test gpkg_zoom_other extension
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    # We expect a warning 
    gdal.PushErrorHandler()
    ret = out_ds.BuildOverviews('NEAR', [3])
    gdal.PopErrorHandler()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetRasterBand(1).GetOverviewCount() != 3:
        gdaltest.post_reason('fail')
        return 'fail'
    got_cs = [out_ds.GetRasterBand(i+1).GetOverview(0).Checksum() for i in range(3)]
    if got_cs != expected_cs_ov0:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs_ov0)
        return 'fail'
    expected_cs = [24807, 25544, 34002]
    got_cs = [out_ds.GetRasterBand(i+1).GetOverview(1).Checksum() for i in range(3)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs)
        return 'fail'
    got_cs = [out_ds.GetRasterBand(i+1).GetOverview(2).Checksum() for i in range(3)]
    if got_cs != expected_cs_ov1:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs_ov1)
        return 'fail'

    # Check that extension is declared
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_extensions WHERE table_name = 'tmp' AND extension_name = 'gpkg_zoom_other'")
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds = None
    
    out_ds = gdal.Open('tmp/tmp.gpkg')
    if out_ds.GetRasterBand(1).GetOverviewCount() != 3:
        gdaltest.post_reason('fail')
        return 'fail'
    got_cs = [out_ds.GetRasterBand(i+1).GetOverview(0).Checksum() for i in range(3)]
    if got_cs != expected_cs_ov0:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs_ov0)
        return 'fail'
    expected_cs = [24807, 25544, 34002]
    got_cs = [out_ds.GetRasterBand(i+1).GetOverview(1).Checksum() for i in range(3)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs)
        return 'fail'
    got_cs = [out_ds.GetRasterBand(i+1).GetOverview(2).Checksum() for i in range(3)]
    if got_cs != expected_cs_ov1:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs_ov1)
        return 'fail'
    out_ds = None

    # Add terminating overview
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    ret = out_ds.BuildOverviews('NEAR', [8])
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    expected_cs = [12725, 12539, 13553]
    got_cs = [out_ds.GetRasterBand(i+1).GetOverview(3).Checksum() for i in range(3)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs)
        return 'fail'
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    # Without padding, immediately after create copy
    ds = gdal.Open('data/small_world.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=PNG', 'BLOCKXSIZE=100', 'BLOCKYSIZE=100'] )
    # Should not result in gpkg_zoom_other
    ret = out_ds.BuildOverviews('NEAR', [8])
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    # Check that there's no extensions
    out_ds = gdal.Open('tmp/tmp.gpkg')
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE type = 'table' AND name = 'gpkg_extensions'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)
    out_ds = None

    os.remove('tmp/tmp.gpkg')

    return 'success'

###############################################################################
# Test overviews with 24-bit color palette single band dataset

def gpkg_19():

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if gdaltest.png_dr is None: 
        return 'skip'
    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    # Without padding, immediately after create copy
    ds = gdal.Open('data/small_world_pct.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=PNG', 'BLOCKXSIZE=100', 'BLOCKYSIZE=100'] )
    out_ds.BuildOverviews('NEAR', [2, 4])
    out_ds = None
    
    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/tmp.tif', ds)
    tmp_ds.BuildOverviews('NEAR', [2, 4])
    expected_cs_ov0 = [tmp_ds.GetRasterBand(i+1).GetOverview(0).Checksum() for i in range(1)]
    expected_cs_ov1 = [tmp_ds.GetRasterBand(i+1).GetOverview(1).Checksum() for i in range(1)]
    tmp_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/tmp.tif')
    
    ds = None

    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER, open_options=['BAND_COUNT=1'])
    if out_ds.GetRasterBand(1).GetOverview(0).GetColorTable() is None:
        gdaltest.post_reason('fail')
        return 'fail'
    got_cs = [out_ds.GetRasterBand(i+1).GetOverview(0).Checksum() for i in range(1)]
    if got_cs != expected_cs_ov0:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs_ov0)
        return 'fail'
    got_cs = [out_ds.GetRasterBand(i+1).GetOverview(1).Checksum() for i in range(1)]
    if got_cs != expected_cs_ov1:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs_ov1)
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 1, True, zoom_level = 1) != 'success':
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 4, False, zoom_level = 0) != 'success':
        return 'fail'
    out_ds = None
    os.remove('tmp/tmp.gpkg')

    return 'success'

###############################################################################
# Test PNG8

def gpkg_20():

    if gdaltest.gpkg_dr is None: 
        return 'skip'
    if gdaltest.png_dr is None: 
        return 'skip'
    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    # Without padding, with small tiles (<=256x256)
    ds = gdal.Open('data/small_world.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=PNG8', 'BLOCKSIZE=200'] )
    out_ds = None
    ds = None

    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER)
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    expected_cs = [30875,31451,38110,64269]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs)
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 1, True) != 'success':
        return 'fail'
    out_ds = None
    os.remove('tmp/tmp.gpkg')

    # Without padding, with big tiles (>256x256)
    ds = gdal.Open('data/small_world.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=PNG8', 'BLOCKXSIZE=400', 'BLOCKYSIZE=200'] )
    out_ds = None
    ds = None

    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER)
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    expected_cs = [27001,30168,34800,64269]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs)
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 1, True) != 'success':
        return 'fail'
    out_ds = None
    os.remove('tmp/tmp.gpkg')

    # With and without padding, with small tiles
    ds = gdal.Open('data/small_world.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=PNG8', 'BLOCKSIZE=150'] )
    out_ds = None
    ds = None

    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER)
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    expected_cs = [27718,31528,42062,64269]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs)
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 1, True) != 'success':
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 4, False, row = 0, col = 2) != 'success':
        return 'fail'
    out_ds = None
    os.remove('tmp/tmp.gpkg')
    
    # Without padding, with small tiles (<=256x256), but espcially less than 256 colors
    ds = gdal.GetDriverByName('MEM').Create('',50,50,3)
    ds.SetGeoTransform
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(2).Fill(2)
    ds.GetRasterBand(3).Fill(3)
    ds.SetGeoTransform([0,1,0,0,0,-1])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    ds.SetProjection(srs.ExportToWkt())
    out_ds = gdaltest.gpkg_dr.CreateCopy('tmp/tmp.gpkg', ds, options = ['DRIVER=PNG8', 'BLOCKSIZE=50'] )
    out_ds = None
    ds = None

    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER)
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    expected_cs = [2500, 5000, 7500, 30658]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print(got_cs)
        print(expected_cs)
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 1, True) != 'success':
        return 'fail'
    out_ds = None
    out_ds = gdal.OpenEx('tmp/tmp.gpkg', gdal.OF_RASTER, open_options = ['BAND_COUNT=1'])
    if out_ds.GetRasterBand(1).GetColorTable().GetCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None
    os.remove('tmp/tmp.gpkg')

    return 'success'

###############################################################################
#

def gpkg_cleanup():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    try:
        os.remove('tmp/tmp.gpkg')
    except:
        pass

    gdal.SetConfigOption('GPKG_DEBUG', None)

    return 'success'

###############################################################################


gdaltest_list = [ 
    gpkg_init,
    gpkg_1,
    gpkg_2,
    gpkg_3,
    gpkg_4,
    gpkg_5,
    gpkg_6,
    gpkg_7,
    gpkg_8,
    gpkg_9,
    gpkg_10,
    gpkg_11,
    gpkg_12,
    gpkg_13,
    gpkg_14,
    gpkg_15,
    gpkg_16,
    gpkg_17,
    gpkg_18,
    gpkg_19,
    gpkg_20,
    gpkg_cleanup,
]
#gdaltest_list = [ gpkg_init, gpkg_20, gpkg_cleanup ]
if __name__ == '__main__':

    gdaltest.setup_run( 'gpkg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

