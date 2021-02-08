#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdaldem testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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


from osgeo import gdal
from osgeo import osr
import gdaltest
import test_cli_utilities
import pytest

###############################################################################
# Test gdaldem hillshade


def test_gdaldem_hillshade():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdaldem_path() + ' hillshade -s 111120 -z 30 ../gdrivers/data/n43.dt0 tmp/n43_hillshade.tif')
    assert (err is None or err == ''), 'got error/warning'

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43_hillshade.tif')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 45587, 'Bad checksum'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), 'Bad geotransform'

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, 'Bad projection'

    assert ds.GetRasterBand(1).GetNoDataValue() == 0, 'Bad nodata value'

    src_ds = None
    ds = None

###############################################################################
# Test gdaldem hillshade


def test_gdaldem_hillshade_compressed_tiled_output():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdaldem_path() + ' hillshade -s 111120 -z 30 ../gdrivers/data/n43.dt0 tmp/n43_hillshade_compressed_tiled.tif -co TILED=YES -co COMPRESS=DEFLATE --config GDAL_CACHEMAX 0')
    assert (err is None or err == ''), 'got error/warning'

    ds = gdal.Open('tmp/n43_hillshade_compressed_tiled.tif')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 45587, 'Bad checksum'

    ds = None

    stat_uncompressed = os.stat('tmp/n43_hillshade.tif')
    stat_compressed = os.stat('tmp/n43_hillshade_compressed_tiled.tif')
    assert stat_uncompressed.st_size >= stat_compressed.st_size, \
        'failure: compressed size greater than uncompressed one'

###############################################################################
# Test gdaldem hillshade -combined


def test_gdaldem_hillshade_combined():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' hillshade -s 111120 -z 30 -combined ../gdrivers/data/n43.dt0 tmp/n43_hillshade_combined.tif')

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43_hillshade_combined.tif')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 43876, 'Bad checksum'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), 'Bad geotransform'

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, 'Bad projection'

    assert ds.GetRasterBand(1).GetNoDataValue() == 0, 'Bad nodata value'

    src_ds = None
    ds = None

###############################################################################
# Test gdaldem hillshade with -compute_edges


def test_gdaldem_hillshade_compute_edges():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' hillshade -compute_edges -s 111120 -z 30 ../gdrivers/data/n43.dt0 tmp/n43_hillshade_compute_edges.tif')

    ds = gdal.Open('tmp/n43_hillshade_compute_edges.tif')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 50239, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem hillshade with -az parameter


def test_gdaldem_hillshade_azimuth():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    ds = gdal.GetDriverByName('GTiff').Create('tmp/pyramid.tif', 100, 100, 1)
    ds.SetGeoTransform([2, 0.01, 0, 49, 0, -0.01])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    for j in range(100):
        data = ''
        for i in range(100):
            val = 255 - 5 * max(abs(50 - i), abs(50 - j))
            data = data + chr(val)
        data = data.encode('ISO-8859-1')
        ds.GetRasterBand(1).WriteRaster(0, j, 100, 1, data)

    ds = None

    # Light from the east
    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' hillshade -s 111120 -z 100 -az 90 -co COMPRESS=LZW tmp/pyramid.tif tmp/pyramid_shaded.tif')

    ds_ref = gdal.Open('data/pyramid_shaded_ref.tif')
    ds = gdal.Open('tmp/pyramid_shaded.tif')
    assert gdaltest.compare_ds(ds, ds_ref, verbose=1) <= 1, 'Bad checksum'
    ds = None
    ds_ref = None

###############################################################################
# Test gdaldem hillshade to PNG


def test_gdaldem_hillshade_png():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' hillshade -of PNG  -s 111120 -z 30 ../gdrivers/data/n43.dt0 tmp/n43_hillshade.png')

    ds = gdal.Open('tmp/n43_hillshade.png')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 45587, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem hillshade to PNG with -compute_edges


def test_gdaldem_hillshade_png_compute_edges():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' hillshade -compute_edges -of PNG  -s 111120 -z 30 ../gdrivers/data/n43.dt0 tmp/n43_hillshade_compute_edges.png')

    ds = gdal.Open('tmp/n43_hillshade_compute_edges.png')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 50239, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem slope


def test_gdaldem_slope():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' slope -s 111120 ../gdrivers/data/n43.dt0 tmp/n43_slope.tif')

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43_slope.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 63748, 'Bad checksum'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), 'Bad geotransform'

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, 'Bad projection'

    assert ds.GetRasterBand(1).GetNoDataValue() == -9999.0, 'Bad nodata value'

    src_ds = None
    ds = None

###############################################################################
# Test gdaldem aspect


def test_gdaldem_aspect():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' aspect ../gdrivers/data/n43.dt0 tmp/n43_aspect.tif')

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43_aspect.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 54885, 'Bad checksum'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), 'Bad geotransform'

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, 'Bad projection'

    assert ds.GetRasterBand(1).GetNoDataValue() == -9999.0, 'Bad nodata value'

    src_ds = None
    ds = None

###############################################################################
# Test gdaldem color relief


def test_gdaldem_color_relief():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief ../gdrivers/data/n43.dt0 data/color_file.txt tmp/n43_colorrelief.tif')
    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43_colorrelief.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 55009, 'Bad checksum'

    assert ds.GetRasterBand(2).Checksum() == 37543, 'Bad checksum'

    assert ds.GetRasterBand(3).Checksum() == 47711, 'Bad checksum'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), 'Bad geotransform'

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, 'Bad projection'

    src_ds = None
    ds = None


###############################################################################
# Test gdaldem color relief on a GMT .cpt file

def test_gdaldem_color_relief_cpt():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief ../gdrivers/data/n43.dt0 data/color_file.cpt tmp/n43_colorrelief_cpt.tif')
    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43_colorrelief_cpt.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 55009, 'Bad checksum'

    assert ds.GetRasterBand(2).Checksum() == 37543, 'Bad checksum'

    assert ds.GetRasterBand(3).Checksum() == 47711, 'Bad checksum'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), 'Bad geotransform'

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, 'Bad projection'

    src_ds = None
    ds = None

###############################################################################
# Test gdaldem color relief to VRT


def test_gdaldem_color_relief_vrt():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief -of VRT ../gdrivers/data/n43.dt0 data/color_file.txt tmp/n43_colorrelief.vrt')
    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.Open('tmp/n43_colorrelief.vrt')
    assert ds is not None

    ds_ref = gdal.Open('tmp/n43_colorrelief.tif')
    assert gdaltest.compare_ds(ds, ds_ref, verbose=0) <= 1, 'Bad checksum'
    ds_ref = None

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), 'Bad geotransform'

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, 'Bad projection'

    src_ds = None
    ds = None

###############################################################################
# Test gdaldem color relief from a Float32 dataset


def test_gdaldem_color_relief_from_float32():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -ot Float32 ../gdrivers/data/n43.dt0 tmp/n43_float32.tif')
    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief tmp/n43_float32.tif data/color_file.txt tmp/n43_colorrelief_from_float32.tif')
    ds = gdal.Open('tmp/n43_colorrelief_from_float32.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 55009, 'Bad checksum'

    assert ds.GetRasterBand(2).Checksum() == 37543, 'Bad checksum'

    assert ds.GetRasterBand(3).Checksum() == 47711, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem color relief to PNG


def test_gdaldem_color_relief_png():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief -of PNG ../gdrivers/data/n43.dt0 data/color_file.txt tmp/n43_colorrelief.png')
    ds = gdal.Open('tmp/n43_colorrelief.png')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 55009, 'Bad checksum'

    assert ds.GetRasterBand(2).Checksum() == 37543, 'Bad checksum'

    assert ds.GetRasterBand(3).Checksum() == 47711, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem color relief from a Float32 to PNG


def test_gdaldem_color_relief_from_float32_to_png():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief -of PNG tmp/n43_float32.tif data/color_file.txt tmp/n43_colorrelief_from_float32.png')
    ds = gdal.Open('tmp/n43_colorrelief_from_float32.png')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 55009, 'Bad checksum'

    assert ds.GetRasterBand(2).Checksum() == 37543, 'Bad checksum'

    assert ds.GetRasterBand(3).Checksum() == 47711, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem color relief with -nearest_color_entry


def test_gdaldem_color_relief_nearest_color_entry():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief -nearest_color_entry ../gdrivers/data/n43.dt0 data/color_file.txt tmp/n43_colorrelief_nearest.tif')
    ds = gdal.Open('tmp/n43_colorrelief_nearest.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 57296, 'Bad checksum'

    assert ds.GetRasterBand(2).Checksum() == 42926, 'Bad checksum'

    assert ds.GetRasterBand(3).Checksum() == 47181, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem color relief with -nearest_color_entry and -of VRT


def test_gdaldem_color_relief_nearest_color_entry_vrt():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief -of VRT -nearest_color_entry ../gdrivers/data/n43.dt0 data/color_file.txt tmp/n43_colorrelief_nearest.vrt')
    ds = gdal.Open('tmp/n43_colorrelief_nearest.vrt')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 57296, 'Bad checksum'

    assert ds.GetRasterBand(2).Checksum() == 42926, 'Bad checksum'

    assert ds.GetRasterBand(3).Checksum() == 47181, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem color relief with a nan nodata


def test_gdaldem_color_relief_nodata_nan():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    f = open('tmp/nodata_nan_src.asc', 'wt')
    f.write("""ncols        2
nrows        2
xllcorner    440720
yllcorner    3750120
cellsize     60
NODATA_value nan
 0.0 0
 0 nan""")
    f.close()

    f = open('tmp/nodata_nan_plt.txt', 'wt')
    f.write('0 0 0 0\n')
    f.write('nv 1 1 1\n')
    f.close()

    gdaltest.runexternal(test_cli_utilities.get_gdaldem_path() + ' color-relief tmp/nodata_nan_src.asc tmp/nodata_nan_plt.txt tmp/nodata_nan_out.tif')

    ds = gdal.Open('tmp/nodata_nan_out.tif')
    val = ds.GetRasterBand(1).ReadRaster()
    ds = None

    import struct
    val = struct.unpack('B' * 4, val)
    assert val == (0, 0, 0, 1)

    os.unlink('tmp/nodata_nan_src.asc')
    os.unlink('tmp/nodata_nan_plt.txt')
    os.unlink('tmp/nodata_nan_out.tif')

###############################################################################
# Test gdaldem color relief with entries with repeated DEM values in the color table (#6422)


def test_gdaldem_color_relief_repeated_entry():
    if test_cli_utilities.get_gdaldem_path() is None:
        pytest.skip()

    f = open('tmp/test_gdaldem_color_relief_repeated_entry.asc', 'wt')
    f.write("""ncols        2
nrows        3
xllcorner    440720
yllcorner    3750120
cellsize     60
NODATA_value 5
 1 4.9
 5 5.1
 6 7 """)
    f.close()

    f = open('tmp/test_gdaldem_color_relief_repeated_entry.txt', 'wt')
    f.write('1 1 1 1\n')
    f.write('6 10 10 10\n')
    f.write('6 20 20 20\n')
    f.write('8 30 30 30\n')
    f.write('nv 5 5 5\n')
    f.close()

    gdaltest.runexternal(
        test_cli_utilities.get_gdaldem_path() + ' color-relief tmp/test_gdaldem_color_relief_repeated_entry.asc tmp/test_gdaldem_color_relief_repeated_entry.txt tmp/test_gdaldem_color_relief_repeated_entry_out.tif',
        display_live_on_parent_stdout=True,
    )

    ds = gdal.Open('tmp/test_gdaldem_color_relief_repeated_entry_out.tif')
    val = ds.GetRasterBand(1).ReadRaster()
    ds = None

    import struct
    val = struct.unpack('B' * 6, val)
    assert val == (1, 1, 5, 10, 10, 25)

    gdaltest.runexternal(
        test_cli_utilities.get_gdaldem_path() + ' color-relief tmp/test_gdaldem_color_relief_repeated_entry.asc tmp/test_gdaldem_color_relief_repeated_entry.txt tmp/test_gdaldem_color_relief_repeated_entry_out.vrt -of VRT',
        display_live_on_parent_stdout=True,
    )

    ds = gdal.Open('tmp/test_gdaldem_color_relief_repeated_entry_out.vrt')
    val = ds.GetRasterBand(1).ReadRaster()
    ds = None

    val = struct.unpack('B' * 6, val)
    assert val == (1, 1, 5, 10, 10, 25)

    os.unlink('tmp/test_gdaldem_color_relief_repeated_entry.asc')
    os.unlink('tmp/test_gdaldem_color_relief_repeated_entry.txt')
    os.unlink('tmp/test_gdaldem_color_relief_repeated_entry_out.tif')
    os.unlink('tmp/test_gdaldem_color_relief_repeated_entry_out.vrt')

###############################################################################
# Cleanup


def test_gdaldem_cleanup():
    try:
        os.remove('tmp/n43_hillshade.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_hillshade_compressed_tiled.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_hillshade_combined.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_hillshade_compute_edges.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/pyramid.tif')
        os.remove('tmp/pyramid_shaded.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_hillshade.png')
        os.remove('tmp/n43_hillshade.png.aux.xml')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_hillshade_compute_edges.png')
        os.remove('tmp/n43_hillshade_compute_edges.png.aux.xml')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_slope.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_aspect.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_colorrelief.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_colorrelief_cpt.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_colorrelief.vrt')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_float32.tif')
        os.remove('tmp/n43_colorrelief_from_float32.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_colorrelief.png')
        os.remove('tmp/n43_colorrelief.png.aux.xml')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_colorrelief_from_float32.png')
        os.remove('tmp/n43_colorrelief_from_float32.png.aux.xml')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_colorrelief_nearest.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/n43_colorrelief_nearest.vrt')
    except OSError:
        pass
    



