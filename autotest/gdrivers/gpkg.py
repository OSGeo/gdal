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
sys.path.append('../../gdal/swig/python/samples')

from osgeo import osr, gdal
import gdaltest

###############################################################################
# Validate a geopackage

try:
    import validate_gpkg
    has_validate = True
except:
    has_validate = False

def validate(filename, quiet = False):
    if has_validate:
        my_filename = filename
        if my_filename.startswith('/vsimem/'):
            my_filename = 'tmp/validate.gpkg'
            f = gdal.VSIFOpenL(filename, 'rb')
            if f is None:
                print('Cannot open %s' % filename)
                return False
            content = gdal.VSIFReadL(1, 10000000, f)
            gdal.VSIFCloseL(f)
            open(my_filename, 'wb').write(content)
        try:
            validate_gpkg.check(my_filename)
        except Exception as e:
            if not quiet:
                print(e)
            return False
        finally:
            if my_filename != filename:
                os.unlink(my_filename)
    return True

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
        if src_ds.RasterCount == 2 and working_bands == 3:
            src_band = 1
        elif src_ds.RasterCount == 2 and working_bands == 4:
            if i < 3:
                src_band = 1
            else:
                src_band = 2
        elif src_ds.RasterCount == 1:
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

    gdal.Unlink('/vsimem/tmp.gpkg')

    # With padding
    ds = gdal.Open('data/byte.tif')
    expected_cs = ds.GetRasterBand(1).Checksum()
    clamped_expected_cs = get_expected_checksums(ds, gdaltest.png_dr, 1, clamp_output = False)[0]
    expected_gt = ds.GetGeoTransform()
    expected_wkt = ds.GetProjectionRef()
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=PNG'] )
    out_ds = None
    ds = None

    if not validate('/vsimem/tmp.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    out_ds = gdal.Open('/vsimem/tmp.gpkg')

    # Check there's no ogr_empty_table
    sql_lyr = out_ds.ExecuteSQL("SELECT COUNT(*) FROM sqlite_master WHERE name = 'ogr_empty_table'")
    f = sql_lyr.GetNextFeature()
    if f.GetField(0) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)

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
    if check_tile_format(out_ds, 'PNG', 2, False) != 'success':
        return 'fail'

    # Check that there's no extensions
    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE type = 'table' AND name = 'gpkg_extensions'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds = None

    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options= ['BAND_COUNT=3'])
    expected_cs = expected_cs[0:3]
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(out_ds.RasterCount)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    out_ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['USE_TILE_EXTENT=YES'])
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

    # Test USE_TILE_EXTENT=YES with empty table
    ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_UPDATE)
    ds.ExecuteSQL('DELETE FROM tmp')
    ds = None
    ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER, open_options = ['USE_TILE_EXTENT=YES'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding
    ds = gdal.Open('data/byte.tif')
    expected_cs = ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=PNG', 'BLOCKSIZE=20'] )
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    expected_cs = [ expected_cs, expected_cs, expected_cs, 4873 ]
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 1, False) != 'success':
        return 'fail'
    out_ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    return 'success'

###############################################################################
# Single band, JPEG

def gpkg_2():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.jpeg_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    # With padding
    ds = gdal.Open('data/byte.tif')
    expected_cs = get_expected_checksums(ds, gdaltest.jpeg_dr, 1)[0]
    clamped_expected_cs = get_expected_checksums(ds, gdaltest.jpeg_dr, 3, clamp_output = False)
    clamped_expected_cs.append(17849)

    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=JPEG'] )
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    expected_cs = [ expected_cs, expected_cs, expected_cs, 4873 ]
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, 'JPEG', 1, False) != 'success':
        return 'fail'

    # Check that there's no extensions
    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE type = 'table' AND name = 'gpkg_extensions'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['USE_TILE_EXTENT=YES'])
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != clamped_expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(clamped_expected_cs)))
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding
    ds = gdal.Open('data/byte.tif')
    expected_cs = get_expected_checksums(ds, gdaltest.jpeg_dr, 1, extend_src = False)[0]
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=JPEG', 'BLOCKSIZE=20'] )
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
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

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    # Should give warning at pixel reading time
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    out_ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=JPEG'] )
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

    gdal.Unlink('/vsimem/tmp.gpkg')

    return 'success'

###############################################################################
# Single band, WEBP

def gpkg_3():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.webp_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    ds = gdal.Open('data/byte.tif')
    expected_cs = get_expected_checksums(ds, gdaltest.webp_dr, 3)
    clamped_expected_cs = get_expected_checksums(ds, gdaltest.webp_dr, 3, clamp_output = False)
    if gdaltest.webp_supports_rgba:
        clamped_expected_cs.append(4898)
    else:
        clamped_expected_cs.append(17849)

    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=WEBP'] )
    out_ds = None

    if not validate('/vsimem/tmp.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg')
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

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['USE_TILE_EXTENT=YES'])
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != clamped_expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(clamped_expected_cs)))
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding
    ds = gdal.Open('data/byte.tif')
    expected_cs = get_expected_checksums(ds, gdaltest.webp_dr, 3, extend_src = False)
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=WEBP', 'BLOCKSIZE=20'] )
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
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
    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.webp_dr.Register()
        gdaltest.post_reason('fail')
        return 'fail'

    # And at pixel reading time as well
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    out_ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.webp_dr.Register()
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Re-register driver
    gdaltest.webp_dr.Register()

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Check updating a non-WEBP dataset with TILE_FORMAT=WEBP
    out_ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg', 1, 1 )
    out_ds.SetGeoTransform([0,1,0,0,0,-1])
    out_ds = None

    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE, open_options=['TILE_FORMAT=WEBP'])
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_extensions WHERE table_name = 'tmp' AND column_name = 'tile_data' AND extension_name = 'gpkg_webp'")
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)
    out_ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

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

    gdal.Unlink('/vsimem/tmp.gpkg')

    ds = gdal.Open('data/rgbsmall.tif')
    expected_cs = get_expected_checksums(ds, tile_drv, 3)
    clamped_expected_cs = get_expected_checksums(ds, tile_drv, 3, clamp_output = False)
    if working_bands == 3:
        clamped_expected_cs.append(17849)
    else:
        clamped_expected_cs.append(30638)

    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=' + tile_drv_name] )
    ds = None
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    expected_cs.append(30658)
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, tile_drv_name, working_bands, False) != 'success':
        return 'fail'
    out_ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['USE_TILE_EXTENT=YES'])
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != clamped_expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(clamped_expected_cs)))
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding
    ds = gdal.Open('data/rgbsmall.tif')
    expected_cs = get_expected_checksums(ds, tile_drv, 3, extend_src = False)
    expected_cs.append(30658)
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=' + tile_drv_name, 'BLOCKSIZE=50'] )
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, tile_drv_name, 3, False) != 'success':
        return 'fail'
    out_ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    return 'success'

###############################################################################
# Three band, JPEG

def gpkg_5():
    return gpkg_4(tile_drv_name = 'JPEG')

###############################################################################
# Three band, WEBP

def gpkg_6():
    return gpkg_4(tile_drv_name = 'WEBP')

###############################################################################
# 4 band, PNG

def get_georeferenced_rgba_ds(alpha_fully_transparent = False, alpha_fully_opaque = False):
    assert(not (alpha_fully_transparent and alpha_fully_opaque))
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    tmp_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tmp.tif',
                                    src_ds.RasterXSize, src_ds.RasterYSize, 4)
    tmp_ds.SetGeoTransform([0,10,0,0,0,-10])
    tmp_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize,
                       src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize))
    if alpha_fully_opaque:
        tmp_ds.GetRasterBand(4).Fill(255)
    elif alpha_fully_transparent:
        tmp_ds.GetRasterBand(4).Fill(0)
    return tmp_ds

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

    gdal.Unlink('/vsimem/tmp.gpkg')

    src_ds = get_georeferenced_rgba_ds()
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', src_ds, options = ['TILE_FORMAT=' + tile_drv_name] )
    out_ds = None

    expected_cs = get_expected_checksums(src_ds, tile_drv, working_bands)

    src_filename = src_ds.GetDescription()
    src_ds = None
    gdal.Unlink(src_filename)

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(working_bands)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, tile_drv_name, working_bands, False) != 'success':
        return 'fail'
    out_ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding with alpha fully opaque
    tmp_ds = get_georeferenced_rgba_ds(alpha_fully_opaque = True)
    expected_cs = get_expected_checksums(tmp_ds, tile_drv, 3, extend_src = False)
    tmp_filename = tmp_ds.GetDescription()
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', tmp_ds, options = ['TILE_FORMAT=' + tile_drv_name, 'BLOCKXSIZE=%d' % tmp_ds.RasterXSize, 'BLOCKYSIZE=%d' % tmp_ds.RasterYSize] )
    out_ds = None
    tmp_ds = None
    gdal.Unlink(tmp_filename)

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, tile_drv_name, 3, False) != 'success':
        return 'fail'
    out_ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding with alpha fully transparent
    tmp_ds = get_georeferenced_rgba_ds(alpha_fully_transparent = True)
    tmp_filename = tmp_ds.GetDescription()
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', tmp_ds, options = ['TILE_FORMAT=' + tile_drv_name, 'BLOCKXSIZE=%d' % tmp_ds.RasterXSize, 'BLOCKYSIZE=%d' % tmp_ds.RasterYSize] )
    out_ds = None
    tmp_ds = None
    gdal.Unlink(tmp_filename)

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    expected_cs = [0, 0, 0, 0]
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    if check_tile_format(out_ds, None, None, None) != 'success':
        return 'fail'
    out_ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

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

    gdal.Unlink('/vsimem/tmp.gpkg')

    tmp_ds = get_georeferenced_ds_with_pct32()
    expected_ct = tmp_ds.GetRasterBand(1).GetColorTable().Clone()
    expected_cs_single_band = tmp_ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', tmp_ds, options = ['BLOCKXSIZE=%d' % tmp_ds.RasterXSize, 'BLOCKYSIZE=%d' % tmp_ds.RasterYSize]  )
    out_ds = None
    tmp_filename = tmp_ds.GetDescription()
    tmp_ds = None
    gdal.Unlink(tmp_filename)

    expected_cs = [ 10991, 57677, 34965, 10638 ]
    out_ds = gdal.Open('/vsimem/tmp.gpkg')
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
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['BAND_COUNT=1'])
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

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Repeated SetColorTable()
    out_ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg',1,1)
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

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Partial tile
    tmp_ds = get_georeferenced_ds_with_pct32()
    expected_ct = tmp_ds.GetRasterBand(1).GetColorTable().Clone()
    expected_cs_single_band = tmp_ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', tmp_ds)
    out_ds = None
    tmp_filename = tmp_ds.GetDescription()
    tmp_ds = None
    gdal.Unlink(tmp_filename)

    expected_cs = [ 10991, 57677, 34965, 10638 ]
    out_ds = gdal.Open('/vsimem/tmp.gpkg')
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

    gdal.Unlink('/vsimem/tmp.gpkg')
    
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
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', tmp_ds, options = ['TILE_FORMAT=' + tile_drv_name] )
    out_ds = None
    tmp_filename = tmp_ds.GetDescription()
    tmp_ds = None
    gdal.Unlink(tmp_filename)

    expected_cs = get_expected_checksums(rgba_ds, tile_drv, working_bands)
    rgba_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(working_bands)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    out_ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

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

    gdal.Unlink('/vsimem/tmp.gpkg')

    src_ds = gdal.Open('data/small_world_pct.tif')
    expected_ct = src_ds.GetRasterBand(1).GetColorTable().Clone()
    expected_cs_single_band = src_ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', src_ds, options = ['BLOCKXSIZE=%d' % src_ds.RasterXSize, 'BLOCKYSIZE=%d' % src_ds.RasterYSize]  )
    out_ds = None
    src_ds = None

    expected_cs = [ 63025, 48175, 12204 ]
    out_ds = gdal.Open('/vsimem/tmp.gpkg')
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
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['BAND_COUNT=1'])
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

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Partial tile
    src_ds = gdal.Open('data/small_world_pct.tif')
    expected_ct = src_ds.GetRasterBand(1).GetColorTable().Clone()
    expected_cs_single_band = src_ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', src_ds)
    out_ds = None
    src_ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['USE_TILE_EXTENT=YES'])
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

    gdal.Unlink('/vsimem/tmp.gpkg')
    return 'success'

###############################################################################
# Test creation and opening options

def gpkg_14():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    src_ds = gdal.Open('data/small_world.tif')
    ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', src_ds, options = ['TILE_FORMAT=PNG', 'RASTER_TABLE=foo', 'RASTER_IDENTIFIER=bar', 'RASTER_DESCRIPTION=baz'])
    ds = None

    gdal.PushErrorHandler()
    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['TABLE=non_existing'])
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.Open('/vsimem/tmp.gpkg')
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
    ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)
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

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['ZOOM_LEVEL=2'])
    if ds.RasterXSize != 400:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['ZOOM_LEVEL=1'])
    if ds.RasterXSize != 400:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    # In update mode, we expose even empty overview levels
    ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_UPDATE, open_options = ['ZOOM_LEVEL=1'])
    if ds.RasterXSize != 400:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['ZOOM_LEVEL=0'])
    if ds.RasterXSize != 200:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Translate('/vsimem/tmp2.gpkg', 'data/byte.tif', format = 'GPKG')
    ds = gdal.OpenEx('/vsimem/tmp2.gpkg', gdal.OF_UPDATE)
    ds.ExecuteSQL('UPDATE gpkg_contents SET min_x = NULL')
    ds = None
    with gdaltest.error_handler():
        ds = gdal.OpenEx('/vsimem/tmp2.gpkg', open_options = ['ZOOM_LEVEL=-1'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/tmp2.gpkg')

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['USE_TILE_EXTENT=YES'])
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

    # Open with exactly one tile shift
    ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_UPDATE, open_options = ['TILE_FORMAT=PNG', 'MINX=-410.4','MAXY=320.4'])
    if ds.RasterXSize != 400+256 or ds.RasterYSize != 200+256:
        gdaltest.post_reason('fail')
        return 'fail'
    expected_cs = [29070,32796,41086,64288]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    data = ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    for i in range(ds.RasterCount):
        ds.GetRasterBand(i+1).Fill(0)
    ds.FlushCache()
    sql_lyr = ds.ExecuteSQL('SELECT * FROM foo')
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.WriteRaster(0,0,ds.RasterXSize,ds.RasterYSize, data)
    ds = None

    # Partial tile shift (enclosing tiles)
    ds = gdal.OpenEx('GPKG:/vsimem/tmp.gpkg:foo', gdal.OF_UPDATE, open_options = ['MINX=-270','MAXY=180','MINY=-180','MAXX=270'])
    if ds.RasterXSize != 600 or ds.RasterYSize != 400:
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        gdaltest.post_reason('fail')
        return 'fail'
    expected_cs = [28940, 32454, 40526, 64323]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'

    # Force full rewrite
    data = ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    # Do a clean just to be sure
    for i in range(ds.RasterCount):
        ds.GetRasterBand(i+1).Fill(0)
    ds.FlushCache()
    sql_lyr = ds.ExecuteSQL('SELECT * FROM foo')
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.WriteRaster(0,0,ds.RasterXSize,ds.RasterYSize, data)
    ds = None

    ds = gdal.OpenEx('GPKG:/vsimem/tmp.gpkg:foo', gdal.OF_UPDATE, open_options = ['MINX=-270','MAXY=180','MINY=-180','MAXX=270'])
    expected_cs = [28940, 32454, 40526, 64323]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    # Partial rewrite
    data = ds.GetRasterBand(1).ReadRaster(0,0,256,256)
    ds.GetRasterBand(1).WriteRaster(0,0,256,256, data)
    ds = None

    ds = gdal.OpenEx('GPKG:/vsimem/tmp.gpkg:foo', open_options = ['MINX=-270','MAXY=180','MINY=-180','MAXX=270'])
    expected_cs = [28940, 32454, 40526, 64323]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    ds = None

    # Partial tile shift (included in tiles)
    ds = gdal.OpenEx('GPKG:/vsimem/tmp.gpkg:foo', gdal.OF_UPDATE, open_options = ['MINX=-90','MAXY=45','MINY=-45','MAXX=90'])
    if ds.RasterXSize != 200 or ds.RasterYSize != 100:
        gdaltest.post_reason('fail')
        return 'fail'
    expected_cs = [9586,9360,26758,48827]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'

    # Force full rewrite
    data = ds.ReadRaster(0,0,ds.RasterXSize,ds.RasterYSize)
    ds.WriteRaster(0,0,ds.RasterXSize,ds.RasterYSize, data)
    ds = None

    ds = gdal.OpenEx('GPKG:/vsimem/tmp.gpkg:foo', open_options = ['MINX=-90','MAXY=45','MINY=-45','MAXX=90'])
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

    ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', src_ds, options = ['APPEND_SUBDATASET=YES', 'RASTER_TABLE=other', 'BLOCKSIZE=64', 'TILE_FORMAT=PNG'])
    ds = None
    another_src_ds = gdal.Open('data/byte.tif')
    ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', another_src_ds, options = ['APPEND_SUBDATASET=YES'])
    ds = None
    another_src_ds = None

    ds = gdal.Open('/vsimem/tmp.gpkg')
    md = ds.GetMetadata('SUBDATASETS')
    if md['SUBDATASET_1_NAME'] != 'GPKG:/vsimem/tmp.gpkg:foo':
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    if md['SUBDATASET_1_DESC'] != 'foo - bar':
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    if md['SUBDATASET_2_NAME'] != 'GPKG:/vsimem/tmp.gpkg:other':
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    if md['SUBDATASET_2_DESC'] != 'other - other':
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    if md['SUBDATASET_3_NAME'] != 'GPKG:/vsimem/tmp.gpkg:byte':
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    if md['SUBDATASET_3_DESC'] != 'byte - byte':
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    ds = None

    ds = gdal.Open('GPKG:/vsimem/tmp.gpkg:other')
    block_size = ds.GetRasterBand(1).GetBlockSize()
    if block_size != [64, 64]:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['TABLE=other', 'MINX=-90','MAXY=45','MINY=-45','MAXX=90'])
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

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Open and fill with an area of interest larger/containing the natural extent
    ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg', 20, 20, 1, options = ['BLOCKSIZE=20'])
    ds.SetGeoTransform([0,1,0,0,0,-1])
    ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_UPDATE, open_options = ['MINX=-5', 'MAXY=5', 'MAXX=25', 'MINY=-25', 'BAND_COUNT=1'])
    ds.GetRasterBand(1).Fill(255)
    ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['MINX=-10','MAXY=10','MINY=-30','MAXX=30'])
    expected_cs = [4934,4934,4934,4934]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Open and fill with an area of interest smaller/inside the natural extent
    # (and smaller than the block size actually)
    ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg', 20, 20, 1, options = ['BLOCKSIZE=20'])
    ds.SetGeoTransform([0,1,0,0,0,-1])
    ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_UPDATE, open_options = ['MINX=5', 'MAXY=-5', 'MAXX=15', 'MINY=-15', 'BAND_COUNT=1'])
    ds.GetRasterBand(1).Fill(255)
    ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['MINX=-10','MAXY=10','MINY=-30','MAXX=30'])
    ## There's some non null data in R,G,B bands that is masked by the alpha. Oh well...
    expected_cs = [2762,2762,2762,1223]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Other corner case : the block intersects a tile at the right of the raster
    # size (because the raster size is smaller than the block size)
    ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg', 400, 200, 1)
    ds.SetGeoTransform([-180,0.9,0,90,0,-0.9])
    ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_UPDATE, open_options = ['MINX=-5', 'MAXY=5', 'MAXX=25', 'MINY=-25', 'BAND_COUNT=1'])
    ds.GetRasterBand(1).Fill(255)
    ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg')
    expected_cs = [15080,15080,15080,13365]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Test reading block from partial tile database
    ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg', 512, 256, 4)
    ds.SetGeoTransform([0,1,0,0,0,-1])
    ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_UPDATE, open_options = ['MINX=-5', 'MAXY=5', 'TILE_FORMAT=PNG'])
    mem_ds = gdal.GetDriverByName('MEM').Create('', 256, 256)
    mem_ds.GetRasterBand(1).Fill(255)
    mem_ds.FlushCache()
    data = mem_ds.GetRasterBand(1).ReadRaster()
    mem_ds = None
    # Only write one of the tile
    ds.GetRasterBand(2).WriteRaster(0,0,256,256,data)

    # "Flush" into partial tile database, but not in definitive database
    oldSize = gdal.GetCacheMax()
    gdal.SetCacheMax(0)
    gdal.SetCacheMax(oldSize)

    sql_lyr = ds.ExecuteSQL('SELECT * FROM tmp')
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    expected_cs = [0,56451,0,0]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        ds.GetRasterBand(4).Fill(255)
        #sys.exit(0)
        return 'fail'
    ds = None

    # Overflow occurred in ComputeTileAndPixelShifts()
    with gdaltest.error_handler():
        ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['MINX=-1e12', 'MAXX=-0.9999e12'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Overflow occurred in ComputeTileAndPixelShifts()
    with gdaltest.error_handler():
        ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['MINY=-1e12', 'MAXY=-0.9999e12'])
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Overflow occurred in ComputeTileAndPixelShifts()
    gdal.Translate('/vsimem/tmp.gpkg', 'data/byte.tif', format = 'GPKG')
    ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_UPDATE)
    ds.ExecuteSQL('UPDATE gpkg_contents SET min_x=-1000000002000, max_x=-1000000000000')
    ds = None
    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/tmp.gpkg')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/tmp.gpkg')
    return 'success'

###############################################################################
# Test error cases

def gpkg_15():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    # SetGeoTransform() and SetProjection() on a non-raster GPKG
    out_ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg',0,0,0)
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

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Repeated SetGeoTransform()
    out_ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg',1,1)
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
    out_ds = None

    # Repeated SetProjection()
    out_ds = gdal.Open('/vsimem/tmp.gpkg',gdal.GA_Update)
    if out_ds.GetProjectionRef() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    ret = out_ds.SetProjection(srs.ExportToWkt())
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetProjectionRef().find('4326') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg',gdal.GA_Update)
    if out_ds.GetProjectionRef().find('4326') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.SetProjection('')
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    if out_ds.GetProjectionRef() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    # Test setting on read-only dataset
    gdal.PushErrorHandler()
    ret = out_ds.SetProjection('')
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ret = out_ds.SetGeoTransform([0,1,0,0,0,-1])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Test SetColorInterpretation()
    out_ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg',1,1)
    out_ds.SetGeoTransform([0,1,0,0,0,-1])
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

    gdal.Unlink('/vsimem/tmp.gpkg')

    out_ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg',1,1,3)
    out_ds.SetGeoTransform([0,1,0,0,0,-1])
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

    gdal.Unlink('/vsimem/tmp.gpkg')

    out_ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg',1,1,2)
    out_ds.SetGeoTransform([0,1,0,0,0,-1])
    ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_GrayIndex)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = out_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
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

    gdal.Unlink('/vsimem/tmp.gpkg')

    return 'success'

###############################################################################
# Test block/tile caching

def gpkg_16():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.jpeg_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    out_ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg',1,1,3, options = ['TILE_FORMAT=JPEG'])
    out_ds.SetGeoTransform([0,1,0,0,0,-1])
    out_ds.GetRasterBand(1).Fill(255)
    out_ds.GetRasterBand(1).FlushCache()
    # Rewrite same tile after re-reading it
    # Will cause a debug message to be emitted
    out_ds.GetRasterBand(2).Fill(127)
    out_ds.GetRasterBand(3).Checksum()
    out_ds.GetRasterBand(2).FlushCache()
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
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
    gdal.Unlink('/vsimem/tmp.gpkg')

    return 'success'

###############################################################################
# Test overviews with single band dataset

def gpkg_17():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding, immediately after create copy
    ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=PNG', 'BLOCKSIZE=10'] )
    out_ds.BuildOverviews('NEAR', [2])
    out_ds = None
    ds = None

    if not validate('/vsimem/tmp.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
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
    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding, after reopening, and BAND_COUNT = 1
    ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=PNG', 'BLOCKSIZE=10'] )
    out_ds = None
    # FIXME? Should we eventually write the driver somewhere in metadata ?
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE, open_options = ['TILE_FORMAT=PNG', 'BAND_COUNT=1'])
    out_ds.BuildOverviews('NEAR', [2])
    out_ds = None
    ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    got_cs = out_ds.GetRasterBand(1).GetOverview(0).Checksum()
    if got_cs != 1087:
        gdaltest.post_reason('fail')
        print(got_cs)
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 1, False, zoom_level = 0) != 'success':
        return 'fail'
    out_ds = None
    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding, after reopening
    ds = gdal.Open('data/byte.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=PNG', 'BLOCKSIZE=10'] )
    out_ds = None
    # FIXME? Should we eventually write the driver somewhere in metadata ?
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE, open_options = ['TILE_FORMAT=PNG'])
    out_ds.BuildOverviews('NEAR', [2])
    out_ds = None
    ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    got_cs = out_ds.GetRasterBand(1).GetOverview(0).Checksum()
    if got_cs != 1087:
        gdaltest.post_reason('fail')
        print(got_cs)
        return 'fail'
    if check_tile_format(out_ds, 'PNG', 3, False, zoom_level = 0) != 'success':
        return 'fail'

    # Check that there's no extensions
    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE type = 'table' AND name = 'gpkg_extensions'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds = None

    # Test clearing overviews
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    out_ds.BuildOverviews('NONE', [])
    out_ds = None
    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    if out_ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Test building on an overview dataset --> error
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    gdal.PushErrorHandler()
    ret = out_ds.GetRasterBand(1).GetOverview(0).GetDataset().BuildOverviews('NONE', [])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Test building overview factor 1 --> error
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    gdal.PushErrorHandler()
    ret = out_ds.BuildOverviews('NEAR', [1])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Test building non-supported overview levels
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    gdal.PushErrorHandler()
    gdal.SetConfigOption('ALLOW_GPKG_ZOOM_OTHER_EXTENSION', 'NO')
    ret = out_ds.BuildOverviews('NEAR', [3])
    gdal.SetConfigOption('ALLOW_GPKG_ZOOM_OTHER_EXTENSION', None)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Test building non-supported overview levels
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    gdal.PushErrorHandler()
    gdal.SetConfigOption('ALLOW_GPKG_ZOOM_OTHER_EXTENSION', 'NO')
    ret = out_ds.BuildOverviews('NEAR', [2, 4])
    gdal.SetConfigOption('ALLOW_GPKG_ZOOM_OTHER_EXTENSION', None)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Test gpkg_zoom_other extension
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
    # Will fail because results in a 6x6 overview
    gdal.PushErrorHandler()
    ret = out_ds.BuildOverviews('NEAR', [3])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    # Test building overviews on read-only dataset
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER)
    gdal.PushErrorHandler()
    ret = out_ds.BuildOverviews('NEAR', [2])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    return 'success'

###############################################################################
# Test overviews with 3 band dataset

def gpkg_18():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'
    try:
        gdal.Unlink('/vsimem/tmp.gpkg')
    except:
        pass

    # Without padding, immediately after create copy
    ds = gdal.Open('data/small_world.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=PNG', 'BLOCKXSIZE=100', 'BLOCKYSIZE=100'] )
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

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
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
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
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

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
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
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER | gdal.OF_UPDATE)
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

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding, immediately after create copy
    ds = gdal.Open('data/small_world.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=PNG', 'BLOCKXSIZE=100', 'BLOCKYSIZE=100'] )
    # Should not result in gpkg_zoom_other
    ret = out_ds.BuildOverviews('NEAR', [8])
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    # Check that there's no extensions
    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE type = 'table' AND name = 'gpkg_extensions'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)
    out_ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    return 'success'

###############################################################################
# Test overviews with 24-bit color palette single band dataset

def gpkg_19():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding, immediately after create copy
    ds = gdal.Open('data/small_world_pct.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=PNG', 'BLOCKXSIZE=100', 'BLOCKYSIZE=100'] )
    out_ds.BuildOverviews('NEAR', [2, 4])
    out_ds = None

    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/tmp.tif', ds)
    tmp_ds.BuildOverviews('NEAR', [2, 4])
    expected_cs_ov0 = [tmp_ds.GetRasterBand(i+1).GetOverview(0).Checksum() for i in range(1)]
    expected_cs_ov1 = [tmp_ds.GetRasterBand(i+1).GetOverview(1).Checksum() for i in range(1)]
    tmp_ds = None
    gdal.GetDriverByName('GTiff').Delete('/vsimem/tmp.tif')

    ds = None

    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER, open_options=['BAND_COUNT=1'])
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
    gdal.Unlink('/vsimem/tmp.gpkg')

    return 'success'

###############################################################################
# Test PNG8

def gpkg_20():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding, with small tiles (<=256x256)
    ds = gdal.Open('data/small_world.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=PNG8', 'BLOCKSIZE=200'] )
    out_ds = None
    ds = None

    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER)
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
    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding, with big tiles (>256x256)
    ds = gdal.Open('data/small_world.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=PNG8', 'BLOCKXSIZE=400', 'BLOCKYSIZE=200'] )
    out_ds = None
    ds = None

    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER)
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
    gdal.Unlink('/vsimem/tmp.gpkg')

    # With and without padding, with small tiles
    ds = gdal.Open('data/small_world.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=PNG8', 'BLOCKSIZE=150'] )
    out_ds = None
    ds = None

    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER)
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
    gdal.Unlink('/vsimem/tmp.gpkg')

    # Without padding, with small tiles (<=256x256), but especially less
    # than 256 colors.
    ds = gdal.GetDriverByName('MEM').Create('',50,50,3)
    ds.SetGeoTransform
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(2).Fill(2)
    ds.GetRasterBand(3).Fill(3)
    ds.SetGeoTransform([0,1,0,0,0,-1])
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', ds, options = ['TILE_FORMAT=PNG8', 'BLOCKSIZE=50'] )
    out_ds = None
    ds = None

    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER)
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
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', gdal.OF_RASTER, open_options = ['BAND_COUNT=1'])
    if out_ds.GetRasterBand(1).GetColorTable().GetCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None
    gdal.Unlink('/vsimem/tmp.gpkg')

    return 'success'

###############################################################################
# Test metadata

def gpkg_21():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    out_ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg', 1, 1)
    out_ds.SetGeoTransform([0,1,0,0,0,-1])
    mddlist = out_ds.GetMetadataDomainList()
    if len(mddlist) != 3:
        gdaltest.post_reason('fail')
        print(mddlist)
        return 'fail'
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)

    # No metadata for now
    sql_lyr = out_ds.ExecuteSQL('SELECT count(*) FROM gpkg_metadata')
    feat = sql_lyr.GetNextFeature()
    v = feat.GetField(0)
    out_ds.ReleaseResultSet(sql_lyr)
    if v != 0:
        gdaltest.post_reason('fail')
        print(v)
        return 'fail'
    sql_lyr = out_ds.ExecuteSQL('SELECT count(*) FROM gpkg_metadata_reference')
    feat = sql_lyr.GetNextFeature()
    v = feat.GetField(0)
    out_ds.ReleaseResultSet(sql_lyr)
    if v != 0:
        gdaltest.post_reason('fail')
        print(v)
        return 'fail'

    # Set a metadata item now
    out_ds.SetMetadataItem('foo', 'bar')
    out_ds = None

    foo_value = 'bar'
    for i in range(4):

        out_ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)

        if len(out_ds.GetMetadata('GEOPACKAGE')) != 0:
            gdaltest.post_reason('fail')
            return 'fail'
        if out_ds.GetMetadataItem('foo') != foo_value:
            gdaltest.post_reason('fail')
            print(out_ds.GetMetadataItem('foo'))
            feat.DumpReadable()
            return 'fail'
        md = out_ds.GetMetadata()
        if len(md) != 3 or md['foo'] != foo_value or \
            md['IDENTIFIER'] != 'tmp' or md['ZOOM_LEVEL'] != '0':
            gdaltest.post_reason('fail')
            print(md)
            feat.DumpReadable()
            return 'fail'

        sql_lyr = out_ds.ExecuteSQL('SELECT * FROM gpkg_metadata')
        feat = sql_lyr.GetNextFeature()
        if feat.GetField('id') != 1 or feat.GetField('md_scope') != 'dataset' or \
        feat.GetField('md_standard_uri') != 'http://gdal.org' or \
        feat.GetField('mime_type') != 'text/xml' or \
        feat.GetField('metadata') != """<GDALMultiDomainMetadata>
  <Metadata>
    <MDI key="foo">%s</MDI>
  </Metadata>
</GDALMultiDomainMetadata>
""" % foo_value:
            gdaltest.post_reason('fail')
            print(i)
            feat.DumpReadable()
            return 'fail'
        out_ds.ReleaseResultSet(sql_lyr)

        sql_lyr = out_ds.ExecuteSQL('SELECT * FROM gpkg_metadata_reference')
        feat = sql_lyr.GetNextFeature()
        if feat.GetField('reference_scope') != 'table' or \
        feat.GetField('table_name') != 'tmp' or \
        not feat.IsFieldNull('column_name') or \
        not feat.IsFieldNull('row_id_value') or \
        not feat.IsFieldSet('timestamp') or \
        feat.GetField('md_file_id') != 1 or \
        not feat.IsFieldNull('md_parent_id'):
            gdaltest.post_reason('fail')
            print(i)
            feat.DumpReadable()
            return 'fail'
        out_ds.ReleaseResultSet(sql_lyr)

        if i == 1:
            out_ds.SetMetadataItem('foo', 'bar')
        elif i == 2:
            out_ds.SetMetadataItem('foo', 'baz')
            foo_value = 'baz'

        out_ds = None

    # Clear metadata
    out_ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)
    out_ds.SetMetadata(None)
    out_ds = None

    # No more metadata
    out_ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)
    sql_lyr = out_ds.ExecuteSQL('SELECT * FROM gpkg_metadata')
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)
    sql_lyr = out_ds.ExecuteSQL('SELECT * FROM gpkg_metadata_reference')
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds.SetMetadataItem('IDENTIFIER', 'my_identifier')
    out_ds.SetMetadataItem('DESCRIPTION', 'my_description')
    out_ds = None

    # Still no metadata
    out_ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)
    if out_ds.GetMetadataItem('IDENTIFIER') != 'my_identifier':
        gdaltest.post_reason('fail')
        return 'fail'
    if out_ds.GetMetadataItem('DESCRIPTION') != 'my_description':
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr = out_ds.ExecuteSQL('SELECT * FROM gpkg_metadata')
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)
    sql_lyr = out_ds.ExecuteSQL('SELECT * FROM gpkg_metadata_reference')
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)

    # Write metadata in global scope
    out_ds.SetMetadata({'bar':'foo'}, 'GEOPACKAGE')

    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)

    if out_ds.GetMetadataItem('bar', 'GEOPACKAGE') != 'foo':
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = out_ds.ExecuteSQL('SELECT * FROM gpkg_metadata')
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('id') != 1 or feat.GetField('md_scope') != 'dataset' or \
        feat.GetField('md_standard_uri') != 'http://gdal.org' or \
        feat.GetField('mime_type') != 'text/xml' or \
        feat.GetField('metadata') != """<GDALMultiDomainMetadata>
  <Metadata>
    <MDI key="bar">foo</MDI>
  </Metadata>
</GDALMultiDomainMetadata>
""":
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = out_ds.ExecuteSQL('SELECT * FROM gpkg_metadata_reference')
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('reference_scope') != 'geopackage' or \
        not feat.IsFieldNull('table_name') or \
        not feat.IsFieldNull('column_name') or \
        not feat.IsFieldNull('row_id_value') or \
        not feat.IsFieldSet('timestamp') or \
        feat.GetField('md_file_id') != 1 or \
        not feat.IsFieldNull('md_parent_id'):
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds.SetMetadataItem('bar', 'baz', 'GEOPACKAGE')
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)
    if out_ds.GetMetadataItem('bar', 'GEOPACKAGE') != 'baz':
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds.SetMetadata(None, 'GEOPACKAGE')
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)
    if len(out_ds.GetMetadata('GEOPACKAGE')) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    out_ds.SetMetadataItem('1','2')
    out_ds.SetMetadataItem('3','4','CUSTOM_DOMAIN')
    out_ds.SetMetadataItem('6','7', 'GEOPACKAGE')
    # Non GDAL metdata
    out_ds.ExecuteSQL("INSERT INTO gpkg_metadata VALUES (10, 'dataset', 'uri', 'text/plain', 'my_metadata')")
    out_ds.ExecuteSQL("INSERT INTO gpkg_metadata_reference VALUES ('geopackage',NULL,NULL,NULL,'2012-08-17T14:49:32.932Z',10,NULL)")
    out_ds.ExecuteSQL("INSERT INTO gpkg_metadata VALUES (11, 'dataset', 'uri', 'text/plain', 'other_metadata')")
    out_ds.ExecuteSQL("INSERT INTO gpkg_metadata_reference VALUES ('geopackage',NULL,NULL,NULL,'2012-08-17T14:49:32.932Z',11,NULL)")
    out_ds.ExecuteSQL("INSERT INTO gpkg_metadata VALUES (12, 'dataset', 'uri', 'text/plain', 'my_metadata_local')")
    out_ds.ExecuteSQL("INSERT INTO gpkg_metadata_reference VALUES ('table','tmp',NULL,NULL,'2012-08-17T14:49:32.932Z',12,NULL)")
    out_ds.ExecuteSQL("INSERT INTO gpkg_metadata VALUES (13, 'dataset', 'uri', 'text/plain', 'other_metadata_local')")
    out_ds.ExecuteSQL("INSERT INTO gpkg_metadata_reference VALUES ('table','tmp',NULL,NULL,'2012-08-17T14:49:32.932Z',13,NULL)")
    out_ds = None

    for i in range(2):
        out_ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)
        if out_ds.GetMetadataItem('1') != '2':
            gdaltest.post_reason('fail')
            return 'fail'
        if out_ds.GetMetadataItem('GPKG_METADATA_ITEM_1') != 'my_metadata_local':
            gdaltest.post_reason('fail')
            print(out_ds.GetMetadata())
            return 'fail'
        if out_ds.GetMetadataItem('GPKG_METADATA_ITEM_2') != 'other_metadata_local':
            gdaltest.post_reason('fail')
            print(out_ds.GetMetadata())
            return 'fail'
        if out_ds.GetMetadataItem('GPKG_METADATA_ITEM_1', 'GEOPACKAGE') != 'my_metadata':
            gdaltest.post_reason('fail')
            print(out_ds.GetMetadata('GEOPACKAGE'))
            return 'fail'
        if out_ds.GetMetadataItem('GPKG_METADATA_ITEM_2', 'GEOPACKAGE') != 'other_metadata':
            gdaltest.post_reason('fail')
            print(out_ds.GetMetadata('GEOPACKAGE'))
            return 'fail'
        if out_ds.GetMetadataItem('3', 'CUSTOM_DOMAIN') != '4':
            gdaltest.post_reason('fail')
            return 'fail'
        if out_ds.GetMetadataItem('6', 'GEOPACKAGE') != '7':
            gdaltest.post_reason('fail')
            return 'fail'
        out_ds.SetMetadata(out_ds.GetMetadata())
        out_ds.SetMetadata(out_ds.GetMetadata('GEOPACKAGE'),'GEOPACKAGE')
        out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)
    out_ds.SetMetadata(None)
    out_ds.SetMetadata(None, 'CUSTOM_DOMAIN')
    out_ds.SetMetadata(None, 'GEOPACKAGE')
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)
    if out_ds.GetMetadataItem('GPKG_METADATA_ITEM_1', 'GEOPACKAGE') != 'my_metadata':
        gdaltest.post_reason('fail')
        print(out_ds.GetMetadata())
        return 'fail'
    sql_lyr = out_ds.ExecuteSQL('SELECT * FROM gpkg_metadata WHERE id < 10')
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)
    sql_lyr = out_ds.ExecuteSQL('SELECT * FROM gpkg_metadata_reference WHERE md_file_id < 10')
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        return 'fail'
    out_ds.ReleaseResultSet(sql_lyr)
    out_ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    return 'success'

###############################################################################
# Two band, PNG

def get_georeferenced_greyalpha_ds():
    src_ds = gdal.Open('../gcore/data/stefan_full_greyalpha.tif')
    tmp_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/tmp.tif',
                                    src_ds.RasterXSize, src_ds.RasterYSize, 2)
    tmp_ds.SetGeoTransform([0,10,0,0,0,-10])
    tmp_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize,
                       src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize))
    return tmp_ds

def gpkg_22(tile_drv_name = 'PNG'):

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if tile_drv_name is None:
        tile_drv = gdaltest.png_dr
        if gdaltest.jpeg_dr is None:
            return 'skip'
        expected_cs = [ 2466, 10807 ]
        clamped_expected_cs = [ 1989, 1989, 1989, 11580 ]
    if tile_drv_name == 'PNG':
        tile_drv = gdaltest.png_dr
        expected_cs = [ 1970, 10807 ]
        clamped_expected_cs = [ 2100, 2100, 2100, 11580 ]
    elif tile_drv_name == 'JPEG':
        tile_drv = gdaltest.jpeg_dr
        expected_cs = [ 6782, 32706 ]
        clamped_expected_cs = [ 6538, 6538, 6538, 32744 ]
    elif tile_drv_name == 'WEBP':
        tile_drv = gdaltest.webp_dr
        if gdaltest.webp_supports_rgba:
            expected_cs = [ 13112, 10807 ]
            clamped_expected_cs = [ 13380, 13380, 13380, 11580 ]
        else:
            expected_cs = [ 13112, 32706 ]
            clamped_expected_cs = [ 13380, 13380, 13380, 32744 ]
    if tile_drv is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    tmp_ds = get_georeferenced_greyalpha_ds()
    if tile_drv_name:
        options = ['TILE_FORMAT=' + tile_drv_name, 'BLOCKSIZE=16']
    else:
        options = ['BLOCKSIZE=16']
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', tmp_ds, options = options)
    tmp_ds_filename = tmp_ds.GetDescription()
    ds = None
    gdal.Unlink(tmp_ds_filename)
    out_ds = None

    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['BAND_COUNT=2'])
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(2)]
    if got_cs != expected_cs:
        if tile_drv_name != 'WEBP' or got_cs not in ([4899, 10807], [6274, 10807]):
            gdaltest.post_reason('fail')
            print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
            return 'fail'
    out_ds = None

    out_ds = gdal.Open('/vsimem/tmp.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    expected_cs = [ expected_cs[0], expected_cs[0], expected_cs[0], expected_cs[1] ]
    if got_cs != expected_cs:
        if tile_drv_name != 'WEBP' or got_cs not in ([4899, 4899, 4899, 10807], [4899, 4984, 4899, 10807], [6274, 6274, 6274, 10807]):
            gdaltest.post_reason('fail')
            print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
            return 'fail'
    out_ds = None

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['USE_TILE_EXTENT=YES'])
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != clamped_expected_cs:
        if tile_drv_name != 'WEBP' or got_cs not in ([5266, 5266, 5266, 11580], [5266, 5310, 5266, 11580], [6436, 6436, 6436, 11580]):
            gdaltest.post_reason('fail')
            print('Got %s, expected %s' % (str(got_cs), str(clamped_expected_cs)))
            return 'fail'
    ds = None

    gdal.Unlink('/vsimem/tmp.gpkg')

    return 'success'

###############################################################################
# Two band, JPEG

def gpkg_23():
    return gpkg_22(tile_drv_name = 'JPEG')

###############################################################################
# Two band, WEBP

def gpkg_24():
    return gpkg_22(tile_drv_name = 'WEBP')

###############################################################################
# Two band, mixed

def gpkg_25():
    return gpkg_22(tile_drv_name = None)

###############################################################################
# Test TILING_SCHEME

def gpkg_26():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    tests =  [ ('CUSTOM', [4672, 4672, 4672, 4873], None),
               ('GoogleCRS84Quad', [3562, 3562, 3562, 3691], None),
               ('GoogleCRS84Quad', [3562, 3562, 3562, 3691], ['RESAMPLING=BILINEAR']),
               ('GoogleCRS84Quad', [3417, 3417, 3417, 3691], ['RESAMPLING=CUBIC']),
               ('GoogleCRS84Quad', [3562, 3562, 3562, 3691], ['ZOOM_LEVEL_STRATEGY=AUTO']),
               ('GoogleCRS84Quad', [14445, 14445, 14445, 14448], ['ZOOM_LEVEL_STRATEGY=UPPER']),
               ('GoogleCRS84Quad', [3562, 3562, 3562, 3691], ['ZOOM_LEVEL_STRATEGY=LOWER']),
               ('GoogleMapsCompatible', [4118, 4118, 4118, 4406], None),
               ('PseudoTMS_GlobalGeodetic', [3562, 3562, 3562, 3691], None),
               ('PseudoTMS_GlobalMercator', [4118, 4118, 4118, 4406], None) ]

    for (scheme, expected_cs, other_options) in tests:

        src_ds = gdal.Open('data/byte.tif')
        options = ['TILE_FORMAT=PNG', 'TILING_SCHEME='+scheme]
        if other_options:
            options = options + other_options
        ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', src_ds, options = options)
        ds = None

        ds = gdal.Open('/vsimem/tmp.gpkg')
        got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
        # VC12 returns [3561, 3561, 3561, 3691] for GoogleCRS84Quad
        # and For GoogleCRS84Quad RESAMPLING=CUBIC, got [3415, 3415, 3415, 3691]
        if max([ abs(got_cs[i] - expected_cs[i]) for i in range(4)]) > 2:
            gdaltest.post_reason('fail')
            print('For %s, got %s, expected %s' % (scheme, str(got_cs), str(expected_cs)))
            if gdal.GetConfigOption('APPVEYOR') is None:
                return 'fail'
        ds = None

        gdal.Unlink('/vsimem/tmp.gpkg')

    tests =  [ ('GoogleCRS84Quad', [[42255, 47336, 24963, 35707],[42253, 47333, 24961, 35707]], None),
               ('GoogleMapsCompatible', [[35429, 36787, 20035, 17849]], None) ]

    for (scheme, expected_cs, other_options) in tests:

        src_ds = gdal.Open('data/small_world.tif')
        options = ['TILE_FORMAT=PNG', 'TILING_SCHEME='+scheme]
        if other_options:
            options = options + other_options
        ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', src_ds, options = options)
        ds = None

        ds = gdal.Open('/vsimem/tmp.gpkg')
        got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
        if got_cs not in expected_cs:
            gdaltest.post_reason('fail')
            print('For %s, got %s, expected %s' % (scheme, str(got_cs), str(expected_cs)))
            if gdal.GetConfigOption('APPVEYOR') is None:
                return 'fail'
        ds = None

        gdal.Unlink('/vsimem/tmp.gpkg')

    # Test a few error cases
    gdal.PushErrorHandler()
    ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg', 1, 1, 1, options = ['TILING_SCHEME=GoogleCRS84Quad', 'BLOCKSIZE=128'])
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.Unlink('/vsimem/tmp.gpkg')

    ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg', 1, 1, 1, options = ['TILING_SCHEME=GoogleCRS84Quad'])
    # Test that implicit SRS registration works.
    if ds.GetProjectionRef().find('4326') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ret = ds.SetGeoTransform([0,10,0,0,0,-10])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32630)
    gdal.PushErrorHandler()
    ret = ds.SetProjection(srs.ExportToWkt())
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ds = None
    gdal.PopErrorHandler()

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Invalid TILING_SCHEME
    src_ds = gdal.Open('data/byte.tif')
    gdal.PushErrorHandler()
    ds = gdaltest.gpkg_dr.CreateCopy('/foo/tmp.gpkg', src_ds, options = ['TILING_SCHEME=invalid'])
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid target filename
    src_ds = gdal.Open('data/byte.tif')
    gdal.PushErrorHandler()
    ds = gdaltest.gpkg_dr.CreateCopy('/foo/tmp.gpkg', src_ds, options = ['TILING_SCHEME=GoogleCRS84Quad'])
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Source is not georeferenced
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    gdal.PushErrorHandler()
    ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', src_ds, options = ['TILING_SCHEME=GoogleCRS84Quad'])
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test behaviour with low block cache max

def gpkg_27():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    oldSize = gdal.GetCacheMax()
    gdal.SetCacheMax(0)
    src_ds = gdal.Open('data/small_world.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', src_ds, options = ['TILE_FORMAT=PNG', 'BLOCKXSIZE=200', 'BLOCKYSIZE=200'])
    gdal.SetCacheMax(oldSize)

    expected_cs = [src_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'

    return 'success'

###############################################################################
# Test that reading a block in a band doesn't wipe another band of the same
# block that would have gone through the GPKG in-memory cache

def gpkg_28():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    src_ds = gdal.Open('data/small_world.tif')
    data = []
    for b in range(3):
        data.append( src_ds.GetRasterBand(b+1).ReadRaster() )
    expected_cs = [src_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    src_ds = None

    out_ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg', 400 ,200, 3, options = ['TILE_FORMAT=PNG', 'BLOCKXSIZE=400', 'BLOCKYSIZE=200'])
    out_ds.SetGeoTransform([0,10,0,0,0,-10])

    out_ds.GetRasterBand(1).WriteRaster(0,0,400,200, data[0])
    # Force the block to go through IWriteBlock()
    oldSize = gdal.GetCacheMax()
    gdal.SetCacheMax(0)
    gdal.SetCacheMax(oldSize)
    # Read (another, but could be any) band
    out_ds.GetRasterBand(2).ReadRaster(0,0,400,200)
    # Write remaining bands 2 and 3
    for b in range(2):
        out_ds.GetRasterBand(b+2).WriteRaster(0,0,400,200, data[b+1])
    out_ds = None
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['BAND_COUNT=3'])
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'

    return 'success'

###############################################################################
# Variation of gpkg_28 with 2 blocks

def gpkg_29(x = 0):

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    src_ds = gdal.Open('data/small_world.tif')
    left = []
    right = []
    for b in range(3):
        left.append( src_ds.GetRasterBand(b+1).ReadRaster(0,0,200,200) )
        right.append( src_ds.GetRasterBand(b+1).ReadRaster(200,0,200,200) )
    expected_cs = [src_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    src_ds = None

    out_ds = gdaltest.gpkg_dr.Create('/vsimem/tmp.gpkg', 400 ,200, 3, options = ['TILE_FORMAT=PNG', 'BLOCKXSIZE=200', 'BLOCKYSIZE=200'])
    out_ds.SetGeoTransform([0,10,0,0,0,-10])

    out_ds.GetRasterBand(1).WriteRaster(0,0,200,200, left[0])
    # Force the block to go through IWriteBlock()
    oldSize = gdal.GetCacheMax()
    gdal.SetCacheMax(0)
    gdal.SetCacheMax(oldSize)
    out_ds.GetRasterBand(2).ReadRaster(x,0,200,200)
    for b in range(2):
        out_ds.GetRasterBand(b+2).WriteRaster(0,0,200,200, left[b+1])
    for b in range(3):
        out_ds.GetRasterBand(b+1).WriteRaster(200,0,200,200, right[b])
    out_ds = None
    out_ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['BAND_COUNT=3'])
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(3)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'

    return 'success'

###############################################################################
# Variation of gpkg_29 where the read is done in another block

def gpkg_30():

    return gpkg_29(x = 200)

###############################################################################
# 1 band to RGBA

def gpkg_31():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Force use of RGBA instead of Grey-Alpha (the natural use case is WEBP)
    # but here we can test losslessly
    gdal.SetConfigOption('GPKG_PNG_SUPPORTS_2BANDS', 'NO')
    gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', gdal.Open('data/byte.tif'), options = ['TILE_FORMAT=PNG', 'BLOCKSIZE=21'])
    gdal.SetConfigOption('GPKG_PNG_SUPPORTS_2BANDS', None)

    ds = gdal.Open('/vsimem/tmp.gpkg')
    if check_tile_format(ds, 'PNG', 4, False) != 'success':
        return 'fail'
    expected_cs = [ 4672, 4672, 4672, 4873 ]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'

    return 'success'

###############################################################################
# grey-alpha to RGBA

def gpkg_32():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Force use of RGBA instead of Grey-Alpha (the natural use case is WEBP)
    # but here we can test losslessly
    gdal.SetConfigOption('GPKG_PNG_SUPPORTS_2BANDS', 'NO')
    gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', get_georeferenced_greyalpha_ds(), options = ['TILE_FORMAT=PNG', 'BLOCKSIZE=200'])
    gdal.SetConfigOption('GPKG_PNG_SUPPORTS_2BANDS', None)

    ds = gdal.Open('/vsimem/tmp.gpkg')
    if check_tile_format(ds, 'PNG', 4, False) != 'success':
        return 'fail'
    expected_cs = [ 1970, 1970, 1970, 10807 ]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(ds.RasterCount)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'

    ds = gdal.OpenEx('/vsimem/tmp.gpkg', open_options = ['BAND_COUNT=2'])
    expected_cs = [ 1970, 10807 ]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(ds.RasterCount)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'

    return 'success'

###############################################################################
# Single band with 32 bit color table -> RGBA

def gpkg_33():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    # Force use of RGBA instead of color-table (the natural use case is WEBP)
    # but here we can test losslessly
    gdal.SetConfigOption('GPKG_PNG_SUPPORTS_CT', 'NO')
    gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', get_georeferenced_ds_with_pct32(), options = ['TILE_FORMAT=PNG'])
    gdal.SetConfigOption('GPKG_PNG_SUPPORTS_CT', None)

    ds = gdal.Open('/vsimem/tmp.gpkg')
    if check_tile_format(ds, 'PNG', 4, False) != 'success':
        return 'fail'
    expected_cs = [ 10991, 57677, 34965, 10638 ]
    got_cs = [ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'

    return 'success'

###############################################################################
# Test partial tiles with overviews (#6335)

def gpkg_34():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    src_ds = gdal.GetDriverByName('MEM').Create('', 512, 417)
    src_ds.SetGeoTransform([-20037508.342789299786091, 2 * 20037508.342789299786091 / 512, 0,
                            16213801.067584000527859, 0, -2 * 16213801.067584000527859 / 417])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    src_ds.SetProjection(srs.ExportToWkt())
    gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', src_ds, options = ['TILE_FORMAT=PNG', 'TILING_SCHEME=GoogleMapsCompatible'])
    ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)
    gdal.ErrorReset()
    ds.BuildOverviews('NEAR', [2])
    ds = None
    if gdal.GetLastErrorMsg() != '':
        return 'fail'

    return 'success'

###############################################################################
# Test dirty block flushing while reading block (#6365)

def gpkg_35():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    gdal.Unlink('/vsimem/tmp.gpkg')

    src_ds = gdal.GetDriverByName('MEM').Create('', 512, 417, 4)
    src_ds.SetGeoTransform([-20037508.342789299786091, 2 * 20037508.342789299786091 / 512, 0,
                            16213801.067584000527859, 0, -2 * 16213801.067584000527859 / 417])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    src_ds.SetProjection(srs.ExportToWkt())
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/tmp.gpkg', src_ds, options = ['TILE_FORMAT=PNG', 'TILING_SCHEME=GoogleMapsCompatible'])
    out_ds.GetRasterBand(1).Fill(32)
    out_ds.GetRasterBand(2).Fill(64)
    out_ds.GetRasterBand(3).Fill(128)
    out_ds.GetRasterBand(4).Fill(255)
    height = out_ds.RasterYSize
    expected_data = out_ds.ReadRaster(0, 0, 256, height)
    out_ds = None

    src_ds = gdal.GetDriverByName('MEM').Create('', 256, height, 4)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.GetRasterBand(2).Fill(255)
    src_ds.GetRasterBand(3).Fill(255)
    src_ds.GetRasterBand(4).Fill(255)
    white_data = src_ds.ReadRaster(0, 0, 256, height)

    ds = gdal.Open('/vsimem/tmp.gpkg', gdal.GA_Update)
    ds.WriteRaster(256, 0, 256, height, white_data)

    oldSize = gdal.GetCacheMax()
    gdal.SetCacheMax(256 * 256 * 4)

    got_data = ds.ReadRaster(0, 0, 256, height)

    gdal.SetCacheMax(oldSize)

    if got_data != expected_data:
        return 'fail'

    return 'success'

###############################################################################
# Single band with 24 bit color table, PNG, GoogleMapsCompatible

def gpkg_36():

    if gdaltest.gpkg_dr is None:
        return 'skip'
    if gdaltest.png_dr is None:
        return 'skip'

    src_ds = gdal.Open('data/small_world_pct.tif')
    out_ds = gdaltest.gpkg_dr.CreateCopy('/vsimem/gpkg_36.gpkg', src_ds, options = ['TILE_FORMAT=PNG', 'TILING_SCHEME=GoogleMapsCompatible','RESAMPLING=NEAREST']  )
    out_ds = None
    src_ds = None

    expected_cs = [ 993, 50461, 64354, 17849 ]
    out_ds = gdal.Open('/vsimem/gpkg_36.gpkg')
    got_cs = [out_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        gdaltest.post_reason('fail')
        print('Got %s, expected %s' % (str(got_cs), str(expected_cs)))
        return 'fail'
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    if got_ct is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    out_ds = None

    gdal.Unlink('/vsimem/gpkg_36.gpkg')
    return 'success'

###############################################################################
# Test that we don't crash when generating big overview factors on rasters with big dimensions
# due to issues in comparing the factor of overviews with the user specified
# factors

def gpkg_37():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdal.GetDriverByName('GPKG').Create('/vsimem/gpkg_37.gpkg',205000, 200000)
    ds.SetGeoTransform([100,0.000001,0,100,0,-0.000001])
    ds = None

    ds = gdal.Open('/vsimem/gpkg_37.gpkg', gdal.GA_Update)
    ret = ds.BuildOverviews('NONE', [2,4,8,16,32,64,128,256,512,1024,2048])
    if ret != 0 or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/gpkg_37.gpkg')
    return 'success'

###############################################################################
# Test generating more than 1000 tiles

def gpkg_38():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    # Without padding, immediately after create copy
    src_ds = gdal.Open('data/small_world.tif')
    gdaltest.gpkg_dr.CreateCopy('/vsimem/gpkg_38.gpkg', src_ds, options = ['TILE_FORMAT=PNG', 'BLOCKSIZE=8'] )

    ds = gdal.Open('/vsimem/gpkg_38.gpkg')
    if ds.GetRasterBand(1).Checksum() != src_ds.GetRasterBand(1).Checksum():
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    filesize = gdal.VSIStatL('/vsimem/gpkg_38.gpkg').size
    gdal.Unlink('/vsimem/gpkg_38.gpkg')

    filename = '/vsimem/||maxlength=%d||gpkg_38.gpkg' % (filesize-100000)
    with gdaltest.error_handler():
        ds = gdaltest.gpkg_dr.CreateCopy(filename, src_ds, options = ['TILE_FORMAT=PNG', 'BLOCKSIZE=8'] )
        ds_is_none = ds is None
        ds = None
    gdal.Unlink(filename)
    if not ds_is_none and gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    filename = '/vsimem/||maxlength=%d||gpkg_38.gpkg' % (filesize-1)
    with gdaltest.error_handler():
        ds = gdaltest.gpkg_dr.CreateCopy(filename, src_ds, options = ['TILE_FORMAT=PNG', 'BLOCKSIZE=8'] )
        ds_is_none = ds is None
        ds = None
    gdal.Unlink(filename)
    if not ds_is_none and gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test tile gridded elevation data

def gpkg_39():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    src_ds = gdal.Open('data/int16.tif')
    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG')

    if not validate('/vsimem/gpkg_39.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    ds = gdal.Open('/vsimem/gpkg_39.gpkg')

    # Check there a ogr_empty_table
    sql_lyr = ds.ExecuteSQL("SELECT COUNT(*) FROM sqlite_master WHERE name = 'ogr_empty_table'")
    f = sql_lyr.GetNextFeature()
    if f.GetField(0) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    if ds.GetRasterBand(1).DataType != gdal.GDT_Int16:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr = ds.ExecuteSQL('SELECT scale, offset FROM gpkg_2d_gridded_tile_ancillary')
    f = sql_lyr.GetNextFeature()
    if f['scale'] != 1.0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL('PRAGMA application_id')
    f = sql_lyr.GetNextFeature()
    if f['application_id'] != 1196444487:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL('PRAGMA user_version')
    f = sql_lyr.GetNextFeature()
    if f['user_version'] != 10200:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    # Statistics not available on partial tile without nodata
    md = ds.GetRasterBand(1).GetMetadata()
    if md != {}:
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    ds = None

    # With nodata: statistics available
    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG', noData = 0)

    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    md = ds.GetRasterBand(1).GetMetadata()
    if md != {'STATISTICS_MINIMUM': '74', 'STATISTICS_MAXIMUM': '255'}:
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    ds = None

    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    mdi = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM')
    if mdi != '74':
        gdaltest.post_reason('fail')
        print(mdi)
        return 'fail'
    ds = None

    # Entire tile: statistics available
    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG', width = 256, height = 256)

    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    md = ds.GetRasterBand(1).GetMetadata()
    if md != {'STATISTICS_MINIMUM': '74', 'STATISTICS_MAXIMUM': '255'}:
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    ds = None

    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    mdi = ds.GetRasterBand(1).GetMetadataItem('STATISTICS_MINIMUM')
    if mdi != '74':
        gdaltest.post_reason('fail')
        print(mdi)
        return 'fail'
    ds = None


    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG', noData = 1)
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetNoDataValue() != -32768.0:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'

    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG', noData = 74)
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 4649:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    if ds.GetRasterBand(1).GetNoDataValue() != -32768.0:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'

    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG', noData = 1, creationOptions = ['TILING_SCHEME=GoogleMapsCompatible'])
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 4118 and cs != 4077:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    gdal.SetConfigOption('GPKG_ADD_DEFINITION_12_063', 'YES')
    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG', noData = 1, creationOptions = ['TILING_SCHEME=GoogleMapsCompatible'])
    gdal.SetConfigOption('GPKG_ADD_DEFINITION_12_063', None)
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 4118 and cs != 4077:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG', width = 1024, height = 1024)
    ds = gdal.Open('/vsimem/gpkg_39.gpkg', gdal.GA_Update)
    ds.BuildOverviews('NEAR', [2, 4])
    if ds.GetRasterBand(1).GetOverview(0).Checksum() != 37308:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetOverview(0).Checksum())
        return 'fail'
    ds.BuildOverviews('NONE', [])
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    if ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG', outputType = gdal.GDT_UInt16)
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    if ds.GetRasterBand(1).DataType != gdal.GDT_UInt16:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr = ds.ExecuteSQL('SELECT scale, offset FROM gpkg_2d_gridded_tile_ancillary')
    f = sql_lyr.GetNextFeature()
    if f['scale'] != 1.0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG', outputType = gdal.GDT_UInt16, noData = 1)
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetNoDataValue() != 1.0:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'

    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG', outputType = gdal.GDT_UInt16, noData = 74)
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 4672:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    if ds.GetRasterBand(1).GetNoDataValue() != 74.0:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'

    src_ds = gdal.Open('data/float32.tif')
    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG')
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    if ds.GetRasterBand(1).DataType != gdal.GDT_Float32:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr = ds.ExecuteSQL('SELECT scale, offset FROM gpkg_2d_gridded_tile_ancillary')
    f = sql_lyr.GetNextFeature()
    if f.GetField('scale') != 1.0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG', noData = 1)
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetNoDataValue() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG', creationOptions = ['TILE_FORMAT=PNG'])
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    if ds.GetRasterBand(1).DataType != gdal.GDT_Float32:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr = ds.ExecuteSQL('SELECT scale, offset FROM gpkg_2d_gridded_tile_ancillary')
    f = sql_lyr.GetNextFeature()
    if f['scale'] == 1.0 or not f.IsFieldSetAndNotNull('scale'):
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    gdal.Translate('/vsimem/gpkg_39.gpkg', src_ds, format = 'GPKG', noData = 74, creationOptions = ['TILE_FORMAT=PNG'])
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    if ds.GetRasterBand(1).DataType != gdal.GDT_Float32:
        gdaltest.post_reason('fail')
        return 'fail'
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 4680:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    sql_lyr = ds.ExecuteSQL('SELECT scale, offset FROM gpkg_2d_gridded_tile_ancillary')
    f = sql_lyr.GetNextFeature()
    if f['scale'] == 1.0 or not f.IsFieldSetAndNotNull('scale'):
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    # Particular case with nodata = -32768 for Int16
    gdal.FileFromMemBuffer('/vsimem/gpkg_39.asc',
"""ncols        6
nrows        1
xllcorner    440720
yllcorner    3750120
cellsize     60
NODATA_value -32768
 -32768 -32767 -32766 0 32766 32767""")
    gdal.Translate('/vsimem/gpkg_39.gpkg', '/vsimem/gpkg_39.asc', format = 'GPKG', outputType = gdal.GDT_Int16)
    src_ds = gdal.Open('/vsimem/gpkg_39.asc')
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    if ds.GetRasterBand(1).DataType != gdal.GDT_Int16:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetNoDataValue() != -32768.0:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != src_ds.GetRasterBand(1).Checksum():
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'
    ds = None
    src_ds = None
    gdal.Unlink('/vsimem/gpkg_39.asc')


    # Particular case with nodata = 65535 for UInt16
    gdal.FileFromMemBuffer('/vsimem/gpkg_39.asc',
"""ncols        6
nrows        1
xllcorner    440720
yllcorner    3750120
cellsize     60
NODATA_value 65535
0 1 2 65533 65534 65535""")
    gdal.Translate('/vsimem/gpkg_39.gpkg', '/vsimem/gpkg_39.asc', format = 'GPKG', outputType = gdal.GDT_UInt16)
    src_ds = gdal.Open('/vsimem/gpkg_39.asc')
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    if ds.GetRasterBand(1).DataType != gdal.GDT_UInt16:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetNoDataValue() != 65535.0:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != src_ds.GetRasterBand(1).Checksum():
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'
    ds = None
    src_ds = None
    gdal.Unlink('/vsimem/gpkg_39.asc')

    # Particular case with nodata = 0 for UInt16
    gdal.FileFromMemBuffer('/vsimem/gpkg_39.asc',
"""ncols        6
nrows        1
xllcorner    440720
yllcorner    3750120
cellsize     60
NODATA_value 0
0 1 2 65533 65534 65535""")
    gdal.Translate('/vsimem/gpkg_39.gpkg', '/vsimem/gpkg_39.asc', format = 'GPKG', outputType = gdal.GDT_UInt16)
    src_ds = gdal.Open('/vsimem/gpkg_39.asc')
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    if ds.GetRasterBand(1).DataType != gdal.GDT_UInt16:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).GetNoDataValue() != 0:
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != src_ds.GetRasterBand(1).Checksum():
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'
    ds = None
    src_ds = None
    gdal.Unlink('/vsimem/gpkg_39.asc')

    # Test large dynamic for Float32 and TILE_FORMAT=PNG
    gdal.FileFromMemBuffer('/vsimem/gpkg_39.asc',
"""ncols        2
nrows        1
xllcorner    440720
yllcorner    3750120
cellsize     60
-100000 100000""")
    gdal.Translate('/vsimem/gpkg_39.gpkg', '/vsimem/gpkg_39.asc', format = 'GPKG', outputType = gdal.GDT_Float32, creationOptions = ['TILE_FORMAT=PNG'])

    if not validate('/vsimem/gpkg_39.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    src_ds = gdal.Open('/vsimem/gpkg_39.asc')
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    if ds.GetRasterBand(1).DataType != gdal.GDT_Float32:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != src_ds.GetRasterBand(1).Checksum():
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'
    ds = None
    src_ds = None
    gdal.Unlink('/vsimem/gpkg_39.asc')

    # Test large dynamic for Float32 and TILE_FORMAT=PNG and nodata
    gdal.FileFromMemBuffer('/vsimem/gpkg_39.asc',
"""ncols        2
nrows        1
xllcorner    440720
yllcorner    3750120
cellsize     60
-100000 100000""")
    gdal.Translate('/vsimem/gpkg_39.gpkg', '/vsimem/gpkg_39.asc', format = 'GPKG', outputType = gdal.GDT_Float32, noData = 0, creationOptions = ['TILE_FORMAT=PNG'])
    src_ds = gdal.Open('/vsimem/gpkg_39.asc')
    ds = gdal.Open('/vsimem/gpkg_39.gpkg')
    if ds.GetRasterBand(1).DataType != gdal.GDT_Float32:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != src_ds.GetRasterBand(1).Checksum():
        gdaltest.post_reason('fail')
        print(ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'
    ds = None
    src_ds = None
    gdal.Unlink('/vsimem/gpkg_39.asc')


    # Test that we can delete an existing tile
    ds = gdaltest.gpkg_dr.Create('/vsimem/gpkg_39.gpkg', 256, 256, 1, gdal.GDT_UInt16)
    ds.SetGeoTransform([2,0.001,0,49,0,-0.001])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    ds.GetRasterBand(1).SetNoDataValue(0)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).FlushCache()
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(1).FlushCache()
    sql_lyr = ds.ExecuteSQL('SELECT scale, offset FROM gpkg_2d_gridded_tile_ancillary')
    f = sql_lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink('/vsimem/gpkg_39.gpkg')

    return 'success'

###############################################################################
# Test VERSION

def gpkg_40():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')
    # Should default to 1.0
    gdal.Translate('/vsimem/gpkg_40.gpkg', src_ds, format = 'GPKG')
    ds = gdal.Open('/vsimem/gpkg_40.gpkg')
    sql_lyr = ds.ExecuteSQL('PRAGMA application_id')
    f = sql_lyr.GetNextFeature()
    if f['application_id'] != 1196437808:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL('PRAGMA user_version')
    f = sql_lyr.GetNextFeature()
    if f['user_version'] != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    # Should default to 1.2 if we didn't override it.
    gdal.Translate('/vsimem/gpkg_40.gpkg', src_ds, format = 'GPKG',
                   outputType = gdal.GDT_Int16, creationOptions = ['VERSION=1.0'])
    ds = gdal.Open('/vsimem/gpkg_40.gpkg')
    sql_lyr = ds.ExecuteSQL('PRAGMA application_id')
    f = sql_lyr.GetNextFeature()
    if f['application_id'] != 1196437808:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL('PRAGMA user_version')
    f = sql_lyr.GetNextFeature()
    if f['user_version'] != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    gdal.Translate('/vsimem/gpkg_40.gpkg', src_ds, format = 'GPKG',
                   creationOptions = ['VERSION=1.1'])
    ds = gdal.Open('/vsimem/gpkg_40.gpkg')
    sql_lyr = ds.ExecuteSQL('PRAGMA application_id')
    f = sql_lyr.GetNextFeature()
    if f['application_id'] != 1196437809:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL('PRAGMA user_version')
    f = sql_lyr.GetNextFeature()
    if f['user_version'] != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    gdal.Translate('/vsimem/gpkg_40.gpkg', src_ds, format = 'GPKG',
                   creationOptions = ['VERSION=1.2'])
    ds = gdal.Open('/vsimem/gpkg_40.gpkg')
    sql_lyr = ds.ExecuteSQL('PRAGMA application_id')
    f = sql_lyr.GetNextFeature()
    if f['application_id'] != 1196444487:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL('PRAGMA user_version')
    f = sql_lyr.GetNextFeature()
    if f['user_version'] != 10200:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    gdal.Unlink('/vsimem/gpkg_40.gpkg')

    return 'success'

###############################################################################
# Robustness test

def gpkg_41():

    if gdaltest.gpkg_dr is None or gdal.GetConfigOption('TRAVIS') is not None or \
       gdal.GetConfigOption('APPVEYOR') is not None:
        return 'skip'

    gdal.SetConfigOption('GPKG_ALLOW_CRAZY_SETTINGS', 'YES')
    with gdaltest.error_handler():
        gdal.Translate('/vsimem/gpkg_41.gpkg', 'data/huge_line.tif',
                        format = 'GPKG', creationOptions = [
                            'BLOCKXSIZE=500000000', 'BLOCKYSIZE=1' ])
    gdal.SetConfigOption('GPKG_ALLOW_CRAZY_SETTINGS', None)
    return 'success'

###############################################################################
# Test opening in vector mode a database without gpkg_geometry_columns

def gpkg_42():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    gdal.SetConfigOption('CREATE_GEOMETRY_COLUMNS', 'NO')
    gdal.Translate('/vsimem/gpkg_42.gpkg', 'data/byte.tif', format = 'GPKG')
    gdal.SetConfigOption('CREATE_GEOMETRY_COLUMNS', None)

    ds = gdal.OpenEx('/vsimem/gpkg_42.gpkg', gdal.OF_VECTOR | gdal.OF_UPDATE)
    sql_lyr = ds.ExecuteSQL("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_geometry_columns'")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.CreateLayer('test')
    if lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.FlushCache()
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/gpkg_42.gpkg')

    return 'success'

###############################################################################
# Test adding raster to a database without pre-existing raster support tables.

def gpkg_43():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    gdal.SetConfigOption('CREATE_RASTER_TABLES', 'NO')
    ds = gdaltest.gpkg_dr.Create('/vsimem/gpkg_43.gpkg', 0, 0, 0, gdal.GDT_Unknown)
    gdal.SetConfigOption('CREATE_RASTER_TABLES', None)
    ds.CreateLayer('foo')
    ds = None

    ds = gdal.OpenEx('/vsimem/gpkg_43.gpkg', gdal.OF_UPDATE)
    sql_lyr = ds.ExecuteSQL("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_tile_matrix_set'")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Translate('/vsimem/gpkg_43.gpkg', 'data/byte.tif',
                   format = 'GPKG', creationOptions = ['APPEND_SUBDATASET=YES'])
    ds = gdal.OpenEx('/vsimem/gpkg_43.gpkg')
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    if not validate('/vsimem/gpkg_43.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    gdal.Unlink('/vsimem/gpkg_43.gpkg')

    return 'success'

###############################################################################
# Test opening a .gpkg.sql file

def gpkg_44():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    if gdaltest.gpkg_dr.GetMetadataItem("ENABLE_SQL_GPKG_FORMAT") != 'YES':
        return 'skip'

    ds = gdal.Open('data/byte.gpkg.sql')
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('validation failed')
        return 'fail'
    return 'success'

###############################################################################
# Test opening a .gpkg file

def gpkg_45():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdal.Open('data/byte.gpkg')
    if ds.GetRasterBand(1).Checksum() != 4672:
        gdaltest.post_reason('validation failed')
        return 'fail'
    return 'success'

###############################################################################
# Test fix for #6932

def gpkg_46():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.Create('/vsimem/gpkg_46.gpkg', 6698, 6698,
                                 options = ['TILING_SCHEME=GoogleMapsCompatible'])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    ds.SetProjection(srs.ExportToWkt())
    ds.SetGeoTransform([500,0.037322767717371,0,750,0,-0.037322767717371])
    ds = None

    ds = gdal.Open('/vsimem/gpkg_46.gpkg', gdal.GA_Update)
    ds.BuildOverviews('NEAR', [2,4,8,16,32,64,128,256])
    ds = None

    ds = gdal.Open('/vsimem/gpkg_46.gpkg')
    sql_lyr = ds.ExecuteSQL('SELECT zoom_level, matrix_width * pixel_x_size * tile_width, matrix_height * pixel_y_size * tile_height FROM gpkg_tile_matrix ORDER BY zoom_level')
    count = 0
    for f in sql_lyr:
        count += 1
        if abs(f.GetField(1) - 40075016.6855785) > 1e-7 or \
           abs(f.GetField(2) - 40075016.6855785) > 1e-7:
            gdaltest.post_reason('fail')
            f.DumpReadable()
            ds.ReleaseResultSet(sql_lyr)
            gdal.Unlink('/vsimem/gpkg_46.gpkg')
            return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink('/vsimem/gpkg_46.gpkg')

    if count != 23:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test fix for #6976

def gpkg_47():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    tmpfile = '/vsimem/gpkg_47.gpkg'
    ds = gdaltest.gpkg_dr.CreateCopy(tmpfile,
                                     gdal.Open('data/byte.tif'))
    ds.ExecuteSQL('UPDATE gpkg_contents SET min_x = 1, max_x = 0')
    ds = None
    with gdaltest.error_handler():
        ds = gdal.Open(tmpfile)
    if ds.RasterXSize != 256:
        return 'fail'
    ds = None

    gdal.Unlink(tmpfile)

    return 'success'

###############################################################################
# Test fix for https://issues.qgis.org/issues/16997 (opening a file with
# subdatasets on Windows)

def gpkg_48():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    if sys.platform == 'win32':
        filename = os.path.join(os.getcwd(), 'tmp', 'byte.gpkg')
    else:
        # Test Windows code path in a weird way...
        filename = 'C:\\byte.gpkg'

    gdal.Translate(filename, 'data/byte.tif', format = 'GPKG', creationOptions = ['RASTER_TABLE=foo'])
    gdal.Translate(filename, 'data/byte.tif', format = 'GPKG', creationOptions = ['APPEND_SUBDATASET=YES', 'RASTER_TABLE=bar'])
    ds = gdal.Open( 'GPKG:' + filename + ':foo')
    if ds is None:
        gdal.Unlink(filename)
        return 'fail'
    ds = None
    ds = gdal.Open( 'GPKG:' + filename + ':bar')
    if ds is None:
        gdal.Unlink(filename)
        return 'fail'
    ds = None

    gdal.Unlink(filename)

    return 'success'

###############################################################################
#

def gpkg_cleanup():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    try:
        gdal.Unlink('/vsimem/tmp.gpkg')
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
    gpkg_21,
    gpkg_22,
    gpkg_23,
    gpkg_24,
    gpkg_25,
    gpkg_26,
    gpkg_27,
    gpkg_28,
    gpkg_29,
    gpkg_30,
    gpkg_31,
    gpkg_32,
    gpkg_33,
    gpkg_34,
    gpkg_35,
    gpkg_36,
    gpkg_37,
    gpkg_38,
    gpkg_39,
    gpkg_40,
    gpkg_41,
    gpkg_42,
    gpkg_43,
    gpkg_44,
    gpkg_45,
    gpkg_46,
    gpkg_47,
    gpkg_48,
    gpkg_cleanup,
]
#gdaltest_list = [ gpkg_init, gpkg_47, gpkg_cleanup ]
if __name__ == '__main__':

    gdaltest.setup_run( 'gpkg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

