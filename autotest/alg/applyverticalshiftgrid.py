#!/usr/bin/env python
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

import sys

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal, osr

###############################################################################
# Rather dummy test: grid = DEM

def applyverticalshiftgrid_1():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    src_ds = gdal.Translate('', src_ds, format = 'MEM',
                            width = 20, height = 40)
    grid_ds = gdal.Translate('', src_ds, format = 'MEM')
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds)
    if out_ds.GetRasterBand(1).DataType != gdal.GDT_Byte:
        gdaltest.post_reason('fail')
        print(out_ds.GetRasterBand(1).DataType)
        return 'fail'
    if out_ds.RasterXSize != src_ds.RasterXSize:
        gdaltest.post_reason('fail')
        print(out_ds.RasterXSize)
        return 'fail' 
    if out_ds.RasterYSize != src_ds.RasterYSize:
        gdaltest.post_reason('fail')
        print(out_ds.RasterYSize)
        return 'fail' 
    if out_ds.GetGeoTransform() != src_ds.GetGeoTransform():
        gdaltest.post_reason('fail')
        print(out_ds.GetGeoTransform())
        return 'fail' 
    if out_ds.GetProjectionRef() != src_ds.GetProjectionRef():
        gdaltest.post_reason('fail')
        print(out_ds.GetProjectionRef())
        return 'fail' 
    # Check that we can drop the reference to the sources
    src_ds = None
    grid_ds = None

    cs = out_ds.GetRasterBand(1).Checksum()
    if cs != 10038:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    src_ds = gdal.Open('../gcore/data/byte.tif')
    src_ds = gdal.Translate('', src_ds, format = 'MEM',
                            width = 20, height = 40)

    # Test block size
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, src_ds,
                                         options = ['BLOCKSIZE=15'])
    cs = out_ds.GetRasterBand(1).Checksum()
    if cs != 10038:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    # Inverse transformer
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, src_ds, True,
                                         options = ['DATATYPE=Float32'])
    if out_ds.GetRasterBand(1).DataType != gdal.GDT_Float32:
        gdaltest.post_reason('fail')
        print(out_ds.GetRasterBand(1).DataType)
        return 'fail'
    cs = out_ds.GetRasterBand(1).Checksum()
    if cs != 0:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Error cases

def applyverticalshiftgrid_2():

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
        if out_ds is not None:
            gdaltest.post_reason('fail')
            print(i)
            return 'fail'

    # Non invertable source geotransform
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([0, 0, 0, 0, 0, 0])
    src_ds.SetProjection(sr.ExportToWkt())
    grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    grid_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    grid_ds.SetProjection(sr.ExportToWkt())
    with gdaltest.error_handler():
        out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds)
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Non invertable grid geotransform
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.SetProjection(sr.ExportToWkt())
    grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    grid_ds.SetGeoTransform([0, 0, 0, 0, 0, 0])
    grid_ds.SetProjection(sr.ExportToWkt())
    with gdaltest.error_handler():
        out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds)
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
                                                options = ['BLOCKSIZE=2000000000'])
        if out_ds is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    # Wrong DATATYPE
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.SetProjection(sr.ExportToWkt())
    grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    grid_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    grid_ds.SetProjection(sr.ExportToWkt())
    with gdaltest.error_handler():
        out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds,
                                             options = ['DATATYPE=x'])
    if out_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test with grid and src not in same projection

def applyverticalshiftgrid_3():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    grid_ds = gdal.Warp('', src_ds, format = 'MEM', dstSRS = 'EPSG:4326',
                        width = 40, height = 40 )
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds,
                                         options = ['RESAMPLING=NEAREST'])
    if out_ds.RasterXSize != src_ds.RasterXSize:
        gdaltest.post_reason('fail')
        print(out_ds.RasterXSize)
        return 'fail' 
    if out_ds.RasterYSize != src_ds.RasterYSize:
        gdaltest.post_reason('fail')
        print(out_ds.RasterYSize)
        return 'fail' 
    if out_ds.GetGeoTransform() != src_ds.GetGeoTransform():
        gdaltest.post_reason('fail')
        print(out_ds.GetGeoTransform())
        return 'fail' 
    if out_ds.GetProjectionRef() != src_ds.GetProjectionRef():
        gdaltest.post_reason('fail')
        print(out_ds.GetProjectionRef())
        return 'fail' 
    cs = out_ds.GetRasterBand(1).Checksum()
    if cs != 5112:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds,
                                         options = ['RESAMPLING=BILINEAR'])
    cs = out_ds.GetRasterBand(1).Checksum()
    if cs != 4867 and cs != 4868:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds,
                                         options = ['RESAMPLING=CUBIC'])
    cs = out_ds.GetRasterBand(1).Checksum()
    if cs != 4841 and cs != 4854:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test nodata

def applyverticalshiftgrid_4():

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
    if out_ds.GetRasterBand(1).GetNoDataValue() != 1:
        gdaltest.post_reason('fail')
        print(out_ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'
    cs = out_ds.GetRasterBand(1).Checksum()
    if cs != 1:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

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
    if out_ds.GetRasterBand(1).GetNoDataValue() is not None:
        gdaltest.post_reason('fail')
        print(out_ds.GetRasterBand(1).GetNoDataValue())
        return 'fail'
    cs = out_ds.GetRasterBand(1).Checksum()
    if cs != 1:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    # ERROR_ON_MISSING_VERT_SHIFT due to non compatible extents
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.SetProjection(sr.ExportToWkt())
    src_ds.GetRasterBand(1).Fill(255)
    grid_ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    grid_ds.SetGeoTransform([10, 1, 0, 0, 0, -1])
    grid_ds.SetProjection(sr.ExportToWkt())
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds,
                                options = ['ERROR_ON_MISSING_VERT_SHIFT=YES'])
    with gdaltest.error_handler():
        data = out_ds.GetRasterBand(1).ReadRaster()
    if data is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
                                options = ['ERROR_ON_MISSING_VERT_SHIFT=YES'])
    with gdaltest.error_handler():
        data = out_ds.GetRasterBand(1).ReadRaster()
    if data is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test scaling parameters

def applyverticalshiftgrid_5():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    grid_ds = gdal.Translate('', src_ds, format = 'MEM')
    grid_ds.GetRasterBand(1).Fill(0)
    src_ds = gdal.Translate('', src_ds, format = 'MEM',
                            outputType = gdal.GDT_Float32,
                            scaleParams = [[0,1,0,0.5]])
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds, srcUnitToMeter = 2)
    cs = out_ds.GetRasterBand(1).Checksum()
    if cs != 4672:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    src_ds = gdal.Open('../gcore/data/byte.tif')
    grid_ds = gdal.Translate('', src_ds, format = 'MEM')
    grid_ds.GetRasterBand(1).Fill(0)
    src_ds = gdal.Translate('', src_ds, format = 'MEM',
                            outputType = gdal.GDT_Float32,
                            scaleParams = [[0,1,0,0.5]])
    out_ds = gdal.ApplyVerticalShiftGrid(src_ds, grid_ds, dstUnitToMeter = 0.5)
    cs = out_ds.GetRasterBand(1).Checksum()
    if cs != 4672:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Simulate EGM grids

def applyverticalshiftgrid_6():

    grid_ds = gdal.GetDriverByName('GTX').Create(
        '/vsimem/applyverticalshiftgrid_6.gtx', 1440, 721, 1, gdal.GDT_Float32)
    grid_ds.SetGeoTransform([-180.125,0.25,0,90.125,0,-0.25])
    grid_ds.GetRasterBand(1).Fill(10)
    grid_ds = None

    ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM',
                   dstSRS = '+proj=utm +zone=11 +datum=NAD27 +geoidgrids=/vsimem/applyverticalshiftgrid_6.gtx +vunits=m +no_defs')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 4783:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    gdal.Unlink('/vsimem/applyverticalshiftgrid_6.gtx')

    return 'success'

###############################################################################
# Simulate USA geoid grids with long origin > 180

def applyverticalshiftgrid_7():

    grid_ds = gdal.GetDriverByName('GTX').Create(
        '/vsimem/applyverticalshiftgrid_7.gtx', 700, 721, 1, gdal.GDT_Float32)
    grid_ds.SetGeoTransform([-150 + 360,0.25,0,90.125,0,-0.25])
    grid_ds.GetRasterBand(1).Fill(10)
    grid_ds = None

    ds = gdal.Warp('', '../gcore/data/byte.tif', format = 'MEM',
                   dstSRS = '+proj=utm +zone=11 +datum=NAD27 +geoidgrids=/vsimem/applyverticalshiftgrid_7.gtx +vunits=m +no_defs')
    cs = ds.GetRasterBand(1).Checksum()
    if cs != 4783:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'


    gdal.Unlink('/vsimem/applyverticalshiftgrid_7.gtx')

    return 'success'


gdaltest_list = [
    applyverticalshiftgrid_1,
    applyverticalshiftgrid_2,
    applyverticalshiftgrid_3,
    applyverticalshiftgrid_4,
    applyverticalshiftgrid_5,
    applyverticalshiftgrid_6,
    applyverticalshiftgrid_7
]

if __name__ == '__main__':

    gdaltest.setup_run( 'applyverticalshiftgrid' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
