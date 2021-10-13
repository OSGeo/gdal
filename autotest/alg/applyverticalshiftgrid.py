#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GDALApplyVerticalShiftGrid algorithm.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2017, Even Rouault <even dot rouault at spatialys dot com>
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



import gdaltest
from osgeo import gdal, osr

###############################################################################
# Rather dummy test: grid = DEM


def test_applyverticalshiftgrid_1():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    src_ds = gdal.Translate('', src_ds, format='MEM',
                            width=20, height=40)
    grid_ds = gdal.Translate('', src_ds, format='MEM')
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds)
    assert out_ds.GetRasterBand(1).DataType == gdal.GDT_Byte
    assert out_ds.RasterXSize == src_ds.RasterXSize
    assert out_ds.RasterYSize == src_ds.RasterYSize
    assert out_ds.GetGeoTransform() == src_ds.GetGeoTransform()
    assert out_ds.GetProjectionRef() == src_ds.GetProjectionRef()
    # Check that we can drop the reference to the sources
    src_ds = None
    grid_ds = None

    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 10038

    src_ds = gdal.Open('../gcore/data/byte.tif')
    src_ds = gdal.Translate('', src_ds, format='MEM',
                            width=20, height=40)

    # Test block size
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, src_ds,
                                         options=['BLOCKSIZE=15'])
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 10038

    # Inverse transformer
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, src_ds, True,
                                         options=['DATATYPE=Float32'])
    assert out_ds.GetRasterBand(1).DataType == gdal.GDT_Float32
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 0

###############################################################################
# Error cases


def test_applyverticalshiftgrid_2():

    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")
    for i in range(6):
        src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
        if i != 0:
            src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
        if i != 1:
            src_ds.SetProjection(sr.ExportToWkt())
        if i == 2:
            src_ds.AddBand(gdal.GDT_Byte)
        grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
        if i != 3:
            grid_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
        if i != 4:
            grid_ds.SetProjection(sr.ExportToWkt())
        if i == 5:
            grid_ds.AddBand(gdal.GDT_Byte)
        with gdaltest.error_handler():
            out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds)
        assert out_ds is None, i

    # Non invertable source geotransform
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([0, 0, 0, 0, 0, 0])
    src_ds.SetProjection(sr.ExportToWkt())
    grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    grid_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    grid_ds.SetProjection(sr.ExportToWkt())
    with gdaltest.error_handler():
        out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds)
    assert out_ds is None

    # Non invertable grid geotransform
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.SetProjection(sr.ExportToWkt())
    grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    grid_ds.SetGeoTransform([0, 0, 0, 0, 0, 0])
    grid_ds.SetProjection(sr.ExportToWkt())
    with gdaltest.error_handler():
        out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds)
    assert out_ds is None

    # No PROJ.4 translation for source SRS, coordinate transformation
    # initialization has failed
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.SetProjection(sr.ExportToWkt())
    grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    grid_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    grid_ds.SetProjection('LOCAL_CS["foo"]')
    with gdaltest.error_handler():
        out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds)
    assert out_ds is None

    # Out of memory
    if gdal.GetConfigOption('SKIP_MEM_INTENSIVE_TEST') is None:
        src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
        src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
        src_ds.SetProjection(sr.ExportToWkt())
        grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
        grid_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
        grid_ds.SetProjection(sr.ExportToWkt())
        with gdaltest.error_handler():
            out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds,
                                                 options=['BLOCKSIZE=2000000000'])
        assert out_ds is None

    # Wrong DATATYPE
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.SetProjection(sr.ExportToWkt())
    grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    grid_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    grid_ds.SetProjection(sr.ExportToWkt())
    with gdaltest.error_handler():
        out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds,
                                             options=['DATATYPE=x'])
    assert out_ds is None

###############################################################################
# Test with grid and src not in same projection


def test_applyverticalshiftgrid_3():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    grid_ds = gdal.Warp('', src_ds, format='MEM', dstSRS='EPSG:4326',
                        width=40, height=40)
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds,
                                         options=['RESAMPLING=NEAREST'])
    assert out_ds.RasterXSize == src_ds.RasterXSize
    assert out_ds.RasterYSize == src_ds.RasterYSize
    assert out_ds.GetGeoTransform() == src_ds.GetGeoTransform()
    assert out_ds.GetProjectionRef() == src_ds.GetProjectionRef()
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 5112

    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds,
                                         options=['RESAMPLING=BILINEAR'])
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 4867 or cs == 4868

    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds,
                                         options=['RESAMPLING=CUBIC'])
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs in (4841, 4854, 4842) # 4842 on Mac / Conda

###############################################################################
# Test nodata


def test_applyverticalshiftgrid_4():

    sr = osr.SpatialReference()
    sr.SetFromUserInput("WGS84")

    # Nodata on source
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(1)
    src_ds.GetRasterBand(1).SetNoDataValue(1)
    grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    grid_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    grid_ds.SetProjection(sr.ExportToWkt())
    grid_ds.GetRasterBand(1).Fill(30)
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds)
    assert out_ds.GetRasterBand(1).GetNoDataValue() == 1
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 1

    # Nodata on grid
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(1)
    grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    grid_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    grid_ds.SetProjection(sr.ExportToWkt())
    grid_ds.GetRasterBand(1).Fill(30)
    grid_ds.GetRasterBand(1).SetNoDataValue(30)
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds)
    assert out_ds.GetRasterBand(1).GetNoDataValue() is None
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 1

    # ERROR_ON_MISSING_VERT_SHIFT due to non compatible extents
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(255)
    grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    grid_ds.SetGeoTransform([10, 1, 0, 0, 0, -1])
    grid_ds.SetProjection(sr.ExportToWkt())
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds,
                                         options=['ERROR_ON_MISSING_VERT_SHIFT=YES'])
    with gdaltest.error_handler():
        data = out_ds.GetRasterBand(1).ReadRaster()
    assert data is None

    # ERROR_ON_MISSING_VERT_SHIFT due to nodata in grid
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(255)
    grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    grid_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    grid_ds.SetProjection(sr.ExportToWkt())
    grid_ds.GetRasterBand(1).SetNoDataValue(0)
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds,
                                         options=['ERROR_ON_MISSING_VERT_SHIFT=YES'])
    with gdaltest.error_handler():
        data = out_ds.GetRasterBand(1).ReadRaster()
    assert data is None

###############################################################################
# Test scaling parameters


def test_applyverticalshiftgrid_5():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    grid_ds = gdal.Translate('', src_ds, format='MEM')
    grid_ds.GetRasterBand(1).Fill(0)
    src_ds = gdal.Translate('', src_ds, format='MEM',
                            outputType=gdal.GDT_Float32,
                            scaleParams=[[0, 1, 0, 0.5]])
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds, srcUnitToMeter=2)
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 4672

    src_ds = gdal.Open('../gcore/data/byte.tif')
    grid_ds = gdal.Translate('', src_ds, format='MEM')
    grid_ds.GetRasterBand(1).Fill(0)
    src_ds = gdal.Translate('', src_ds, format='MEM',
                            outputType=gdal.GDT_Float32,
                            scaleParams=[[0, 1, 0, 0.5]])
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds, dstUnitToMeter=0.5)
    cs = out_ds.GetRasterBand(1).Checksum()
    assert cs == 4672

###############################################################################
# Simulate EGM grids


def test_applyverticalshiftgrid_6():

    grid_ds = gdal.GetDriverByName('GTX').Create(
        'tmp/applyverticalshiftgrid_6.gtx', 1440, 721, 1, gdal.GDT_Float32)
    grid_ds.SetGeoTransform([-180.125, 0.25, 0, 90.125, 0, -0.25])
    grid_ds.GetRasterBand(1).Fill(10)
    grid_ds = None

    ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM',
                   dstSRS='+proj=utm +zone=11 +datum=NAD27 +geoidgrids=./tmp/applyverticalshiftgrid_6.gtx +vunits=m +no_defs')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4783

    gdal.Unlink('tmp/applyverticalshiftgrid_6.gtx')

###############################################################################
# Simulate USA geoid grids with long origin > 180


def test_applyverticalshiftgrid_7():

    grid_ds = gdal.GetDriverByName('GTX').Create(
        'tmp/applyverticalshiftgrid_7.gtx', 700, 721, 1, gdal.GDT_Float32)
    grid_ds.SetGeoTransform([-150 + 360, 0.25, 0, 90.125, 0, -0.25])
    grid_ds.GetRasterBand(1).Fill(10)
    grid_ds = None

    ds = gdal.Warp('', '../gcore/data/byte.tif', format='MEM',
                   dstSRS='+proj=utm +zone=11 +datum=NAD27 +geoidgrids=./tmp/applyverticalshiftgrid_7.gtx +vunits=m +no_defs')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4783

    gdal.Unlink('tmp/applyverticalshiftgrid_7.gtx')



