#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DB2 raster functionality.
# Author:   David Adler - dadler@adtechgeospatial.com
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
# Copyright (c) 2016, David Adler
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
import pytest

# Make sure we run from the directory of the script
if os.path.basename(sys.argv[0]) == os.path.basename(__file__):
    if os.path.dirname(sys.argv[0]) != '':
        os.chdir(os.path.dirname(sys.argv[0]))


from osgeo import gdal
import gdaltest

###############################################################################
# Test if DB2 and tile drivers are available


def test_gpkg_init():

    gdaltest.db2_drv = None

    gdaltest.db2_drv = gdal.GetDriverByName('DB2ODBC')
    if gdaltest.db2_drv is None:
        pytest.skip()

    gdaltest.png_dr = gdal.GetDriverByName('PNG')
    gdaltest.jpeg_dr = gdal.GetDriverByName('JPEG')
    gdaltest.webp_dr = gdal.GetDriverByName('WEBP')
    gdaltest.webp_supports_rgba = False
    if gdaltest.webp_dr is not None and gdal.GetConfigOption("GPKG_SIMUL_WEBP_3BAND") is None:
        md = gdaltest.webp_dr.GetMetadata()
        if md['DMD_CREATIONOPTIONLIST'].find('LOSSLESS') >= 0:
            gdaltest.webp_supports_rgba = True

    if 'DB2_TEST_SERVER' in os.environ:
        gdaltest.db2_test_server = "DB2ODBC:" + os.environ['DB2_TEST_SERVER']
    else:
        gdaltest.db2_drv = None
        pytest.skip('Environment variable DB2_TEST_SERVER not found')

    print("\ntest server: " + gdaltest.db2_test_server + "\n")

###############################################################################
#


def get_expected_checksums(src_ds, tile_drv, working_bands, extend_src=True, clamp_output=True):
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
        mem_ds.GetRasterBand(i + 1).WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize, data)
    if tile_drv.ShortName == 'PNG':
        options = []
    else:
        options = ['QUALITY=75']
    tmp_ds = tile_drv.CreateCopy('/vsimem/tmp.' + tile_drv.ShortName, mem_ds, options=options)
    if clamp_output:
        mem_ds = gdal.GetDriverByName('MEM').Create('', src_ds.RasterXSize, src_ds.RasterYSize, working_bands)
        mem_ds.WriteRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize,
                           tmp_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize))
        expected_cs = [mem_ds.GetRasterBand(i + 1).Checksum() for i in range(working_bands)]
    else:
        tmp_ds.FlushCache()
        expected_cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(working_bands)]
    mem_ds = None
    tmp_ds = None
    gdal.Unlink('/vsimem/tmp.' + tile_drv.ShortName)
    return expected_cs

###############################################################################
#


def check_tile_format(out_ds, expected_format, expected_band_count, expected_ct, row=0, col=0, zoom_level=None):
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
            return
        pytest.fail()

    if expected_format == 'PNG':
        expected_mime_type = 'image/png'
    elif expected_format == 'JPEG':
        expected_mime_type = 'image/jpeg'
    elif expected_format == 'WEBP':
        expected_mime_type = 'image/x-webp'

    assert mime_type == expected_mime_type
    assert band_count == expected_band_count
    assert expected_ct == has_ct

###############################################################################
# Single band, PNG


def test_gpkg_1():

    if gdaltest.db2_drv is None:
        pytest.skip()
    if gdaltest.png_dr is None:
        pytest.skip()

    # With padding
    ds = gdal.Open('data/byte.tif')
    expected_cs = ds.GetRasterBand(1).Checksum()
#   clamped_expected_cs = get_expected_checksums(ds, gdaltest.png_dr, 1, clamp_output = False)[0]
    expected_gt = ds.GetGeoTransform()
    expected_wkt = ds.GetProjectionRef()
    out_ds = gdaltest.db2_drv.CreateCopy('DB2ODBC:database=samp105;DSN=SAMP105A', ds, options=['TILE_FORMAT=PNG'])
    out_ds = None
    ds = None

    out_ds = gdal.OpenEx(gdaltest.db2_test_server, gdal.OF_RASTER | gdal.OF_UPDATE, open_options=['TABLE=byte'])

    bnd = out_ds.GetRasterBand(1)
    assert bnd.Checksum() == 4672, 'Didnt get expected checksum on reopened file'

    assert bnd.ComputeRasterMinMax() == (74.0, 255.0), \
        'ComputeRasterMinMax() returned wrong value'

    got_gt = out_ds.GetGeoTransform()
    for i in range(6):
        assert expected_gt[i] == pytest.approx(got_gt[i], abs=1e-8)
    got_wkt = out_ds.GetProjectionRef()
    print("\n** expected_wkt " + expected_wkt + " **\n")
    print("\n** got_wkt " + got_wkt + " **\n")
#   string comparison doesn't work with DB2 due to differences in
#   the WKT (similar but different)
#   just check if it contains '11N' for NAD27 UTM zone 11N
    assert got_wkt.find('11N') != -1
    expected_cs = [expected_cs, expected_cs, expected_cs, 4873]
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs

###############################################################################
