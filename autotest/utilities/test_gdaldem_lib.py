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
# Copyright (c) 2015, Even Rouault <even dot rouault at spatialys.com>
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

import struct


from osgeo import gdal
from osgeo import osr
import gdaltest
import pytest

###############################################################################
# Test gdaldem hillshade


def test_gdaldem_lib_hillshade():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', scale=111120, zFactor=30)
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
# Test gdaldem hillshade with source being floating point


def test_gdaldem_lib_hillshade_float():

    src_ds = gdal.Translate('', gdal.Open('../gdrivers/data/n43.dt0'), format='MEM', outputType=gdal.GDT_Float32)
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', scale=111120, zFactor=30)
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
# Test gdaldem hillshade with source being floating point


def test_gdaldem_lib_hillshade_float_png():

    src_ds = gdal.Translate('', gdal.Open('../gdrivers/data/n43.dt0'), format='MEM', outputType=gdal.GDT_Float32)
    ds = gdal.DEMProcessing('/vsimem/test_gdaldem_lib_hillshade_float_png.png', src_ds, 'hillshade', format='PNG', scale=111120, zFactor=30)
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

    gdal.GetDriverByName('PNG').Delete('/vsimem/test_gdaldem_lib_hillshade_float_png.png')

###############################################################################
# Test gdaldem hillshade -combined


def test_gdaldem_lib_hillshade_combined():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', combined=True, scale=111120, zFactor=30)
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
# Test gdaldem hillshade -alg ZevenbergenThorne


def test_gdaldem_lib_hillshade_ZevenbergenThorne():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', alg='ZevenbergenThorne', scale=111120, zFactor=30)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 46544, 'Bad checksum'

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
# Test gdaldem hillshade -alg ZevenbergenThorne -combined


def test_gdaldem_lib_hillshade_ZevenbergenThorne_combined():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', alg='ZevenbergenThorne', combined=True, scale=111120, zFactor=30)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 43112, 'Bad checksum'

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


def test_gdaldem_lib_hillshade_compute_edges():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', computeEdges=True, scale=111120, zFactor=30)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 50239, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem hillshade with -compute_edges with floating point


def test_gdaldem_lib_hillshade_compute_edges_float():

    src_ds = gdal.Translate('', gdal.Open('../gdrivers/data/n43.dt0'), format='MEM', outputType=gdal.GDT_Float32)
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', computeEdges=True, scale=111120, zFactor=30)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 50239, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem hillshade with -az parameter


def test_gdaldem_lib_hillshade_azimuth():

    src_ds = gdal.GetDriverByName('MEM').Create('', 100, 100, 1)
    src_ds.SetGeoTransform([2, 0.01, 0, 49, 0, -0.01])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    src_ds.SetProjection(sr.ExportToWkt())
    for j in range(100):
        data = ''
        for i in range(100):
            val = 255 - 5 * max(abs(50 - i), abs(50 - j))
            data = data + chr(val)
        data = data.encode('ISO-8859-1')
        src_ds.GetRasterBand(1).WriteRaster(0, j, 100, 1, data)

    # Light from the east
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', azimuth=90, scale=111120, zFactor=100)
    assert ds is not None
    ds_ref = gdal.Open('data/pyramid_shaded_ref.tif')
    assert gdaltest.compare_ds(ds, ds_ref, verbose=1) <= 1, 'Bad checksum'
    ds = None
    ds_ref = None

###############################################################################
# Test gdaldem hillshade -multidirectional


def test_gdaldem_lib_hillshade_multidirectional():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', multiDirectional=True, computeEdges=True, scale=111120, zFactor=30)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 51784, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem hillshade -multidirectional


def test_gdaldem_lib_hillshade_multidirectional_ZevenbergenThorne():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', alg='ZevenbergenThorne', multiDirectional=True, computeEdges=True, scale=111120, zFactor=30)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 50860, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem hillshade -igor


def test_gdaldem_lib_hillshade_igor():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', igor=True, computeEdges=True, scale=111120, zFactor=30)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 48830, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem hillshade -igor


def test_gdaldem_lib_hillshade_igor_ZevenbergenThorne():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', alg='ZevenbergenThorne', igor=True, computeEdges=True, scale=111120, zFactor=30)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 49014, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem color relief


def test_gdaldem_lib_color_relief():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'color-relief', format='MEM', colorFilename='data/color_file.txt')
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

    ds = gdal.DEMProcessing('', src_ds, 'color-relief', format='MEM', colorFilename='data/color_file.txt', colorSelection='nearest_color_entry')
    assert ds.GetRasterBand(1).Checksum() == 57296

    ds = gdal.DEMProcessing('', src_ds, 'color-relief', format='MEM', colorFilename='data/color_file.txt', colorSelection='exact_color_entry')
    assert ds.GetRasterBand(1).Checksum() == 0

    ds = gdal.DEMProcessing('', src_ds, 'color-relief', format='MEM', colorFilename='data/color_file.txt', colorSelection='linear_interpolation')
    assert ds.GetRasterBand(1).Checksum() == 55009

    with pytest.raises(ValueError):
        gdal.DEMProcessing('', src_ds, 'color-relief', format='MEM', colorFilename='data/color_file.txt', colorSelection='unsupported')

    ds = gdal.DEMProcessing('', src_ds, 'color-relief', format='MEM', colorFilename='data/color_file.txt', addAlpha=True)
    assert ds.RasterCount == 4, 'Bad RasterCount'

    src_ds = None
    ds = None


def test_gdaldem_lib_color_relief_nodata_value():

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    colorFilename = '/vsimem/color_file.txt'
    gdal.FileFromMemBuffer(colorFilename, """nv 255 255 255""")

    with gdaltest.error_handler():
        ds = gdal.DEMProcessing('', src_ds, 'color-relief', format='MEM', colorFilename=colorFilename)
    assert ds.GetRasterBand(1).Checksum() == 0

    src_ds.GetRasterBand(1).SetNoDataValue(1)
    ds = gdal.DEMProcessing('', src_ds, 'color-relief', format='MEM', colorFilename=colorFilename)
    assert ds.GetRasterBand(1).Checksum() != 0

    gdal.Unlink(colorFilename)

###############################################################################
# Test gdaldem tpi


def test_gdaldem_lib_tpi():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'tpi', format='MEM')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 60504, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem tri with Wilson formula


def test_gdaldem_lib_tri_wilson():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'tri', format='MEM', alg='Wilson')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 61143, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem tri with Riley formula


def test_gdaldem_lib_tri_riley():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'tri', format='MEM')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 41233, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem roughness


def test_gdaldem_lib_roughness():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'roughness', format='MEM')
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 38624, 'Bad checksum'

    ds = None

###############################################################################
# Test gdaldem slope -alg ZevenbergenThorne


def test_gdaldem_lib_slope_ZevenbergenThorne():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'slope', format='MEM', alg='ZevenbergenThorne', scale=111120, zFactor=30)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 64393, 'Bad checksum'

###############################################################################
# Test gdaldem aspect -alg ZevenbergenThorne


def test_gdaldem_lib_aspect_ZevenbergenThorne():

    src_ds = gdal.Open('../gdrivers/data/n43.dt0')
    ds = gdal.DEMProcessing('', src_ds, 'aspect', format='MEM', alg='ZevenbergenThorne', scale=111120, zFactor=30)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 50539, 'Bad checksum'

###############################################################################
# Test gdaldem hillshade with nodata values


def test_gdaldem_lib_nodata():

    for (value, typ) in [(0, gdal.GDT_Byte),
                         (1, gdal.GDT_Byte),
                         (255, gdal.GDT_Byte),
                         (0, gdal.GDT_UInt16),
                         (1, gdal.GDT_UInt16),
                         (65535, gdal.GDT_UInt16),
                         (0, gdal.GDT_Int16),
                         (-32678, gdal.GDT_Int16),
                         (32767, gdal.GDT_Int16)]:
        src_ds = gdal.GetDriverByName('MEM').Create('', 10, 10, 1, typ)
        src_ds.GetRasterBand(1).SetNoDataValue(value)
        src_ds.GetRasterBand(1).Fill(value)

        ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM')
        assert ds is not None

        cs = ds.GetRasterBand(1).Checksum()
        assert cs == 0, 'Bad checksum'

        src_ds = None
        ds = None

    src_ds = gdal.GetDriverByName('MEM').Create('', 3, 3, 1)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(1, 1, 1, 1, struct.pack('B', 255))

    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        print(ds.ReadAsArray())
        pytest.fail('Bad checksum')

    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', computeEdges=True)
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 10:
        print(ds.ReadAsArray())  # Should be 0 0 0 0 181 0 0 0 0
        pytest.fail('Bad checksum')

    # Same with floating point
    src_ds = gdal.GetDriverByName('MEM').Create('', 3, 3, 1, gdal.GDT_Float32)
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(1).WriteRaster(1, 1, 1, 1, struct.pack('f', 255))

    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 0:
        print(ds.ReadAsArray())
        pytest.fail('Bad checksum')

    ds = gdal.DEMProcessing('', src_ds, 'hillshade', format='MEM', computeEdges=True)
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 10:
        print(ds.ReadAsArray())  # Should be 0 0 0 0 181 0 0 0 0
        pytest.fail('Bad checksum')





