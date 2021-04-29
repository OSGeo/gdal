#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test non-driver specific multidimensional support
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2021, Even Rouault <even.rouault@spatialys.com>
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

import array
from osgeo import gdal
from osgeo import osr

import gdaltest
import pytest

def test_multidim_asarray_epsg_4326():

    ds = gdal.Open('../gdrivers/data/small_world.tif')
    srs_ds = ds.GetSpatialRef()
    assert srs_ds.GetDataAxisToSRSAxisMapping() == [2, 1]
    band = ds.GetRasterBand(1)

    ar = band.AsMDArray()
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetSize() == ds.RasterYSize
    assert dims[1].GetSize() == ds.RasterXSize
    srs_ar = ar.GetSpatialRef()
    assert srs_ar.GetDataAxisToSRSAxisMapping() == [1, 2]

    assert ar.Read() == ds.GetRasterBand(1).ReadRaster()

    ixdim = 1
    iydim = 0
    ds2 = ar.AsClassicDataset(ixdim, iydim)
    assert ds2.RasterYSize == ds.RasterYSize
    assert ds2.RasterXSize == ds.RasterXSize
    srs_ds2 = ds2.GetSpatialRef()
    assert srs_ds2.GetDataAxisToSRSAxisMapping() == [2, 1]
    assert srs_ds2.IsSame(srs_ds)

    assert ds2.ReadRaster() == ds.GetRasterBand(1).ReadRaster()


def test_multidim_asarray_epsg_26711():

    ds = gdal.Open('data/byte.tif')
    srs_ds = ds.GetSpatialRef()
    assert srs_ds.GetDataAxisToSRSAxisMapping() == [1, 2]
    band = ds.GetRasterBand(1)

    ar = band.AsMDArray()
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetSize() == ds.RasterYSize
    assert dims[1].GetSize() == ds.RasterXSize
    srs_ar = ar.GetSpatialRef()
    assert srs_ar.GetDataAxisToSRSAxisMapping() == [2, 1]

    assert ar.Read() == ds.GetRasterBand(1).ReadRaster()

    ixdim = 1
    iydim = 0
    ds2 = ar.AsClassicDataset(ixdim, iydim)
    assert ds2.RasterYSize == ds.RasterYSize
    assert ds2.RasterXSize == ds.RasterXSize
    srs_ds2 = ds2.GetSpatialRef()
    assert srs_ds2.GetDataAxisToSRSAxisMapping() == [1, 2]
    assert srs_ds2.IsSame(srs_ds)

    assert ds2.ReadRaster() == ds.GetRasterBand(1).ReadRaster()


@pytest.mark.parametrize("resampling",[gdal.GRIORA_NearestNeighbour,
                                       gdal.GRIORA_Bilinear,
                                       gdal.GRIORA_Cubic,
                                       gdal.GRIORA_CubicSpline,
                                       gdal.GRIORA_Lanczos,
                                       gdal.GRIORA_Average,
                                       gdal.GRIORA_Mode,
                                       gdal.GRIORA_Gauss, #unsupported
                                       gdal.GRIORA_RMS])
def test_multidim_getresampled(resampling):

    ds = gdal.Open('../gdrivers/data/small_world.tif')
    srs_ds = ds.GetSpatialRef()
    band = ds.GetRasterBand(1)
    ar = band.AsMDArray()
    assert ar

    if resampling == gdal.GRIORA_Gauss:
        with gdaltest.error_handler():
            resampled_ar = ar.GetResampled([None] * ar.GetDimensionCount(), resampling, None)
            assert resampled_ar is None
            return

    resampled_ar = ar.GetResampled([None] * ar.GetDimensionCount(), resampling, None)
    assert resampled_ar
    assert resampled_ar.GetDataType() == ar.GetDataType()
    srs = resampled_ar.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == srs_ds.GetAuthorityCode(None)
    dims = resampled_ar.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetName() == 'dimY'
    assert dims[0].GetSize() == ds.RasterYSize
    assert dims[1].GetName() == 'dimX'
    assert dims[1].GetSize() == ds.RasterXSize

    assert resampled_ar.Read(buffer_datatype = gdal.ExtendedDataType.CreateString()) != gdal.CE_None

    ixdim = 1
    iydim = 0
    ds2 = resampled_ar.AsClassicDataset(ixdim, iydim)
    srs_ds2 = ds2.GetSpatialRef()
    assert srs_ds2.GetDataAxisToSRSAxisMapping() == srs_ds.GetDataAxisToSRSAxisMapping()
    assert srs_ds2.IsSame(srs_ds)
    assert ds2.GetGeoTransform() == pytest.approx(ds.GetGeoTransform())
    if resampling == gdal.GRIORA_CubicSpline:
        # Apparently, cubicspline is not a no-op when there is
        # no resampling
        assert ds2.ReadRaster() != ds.GetRasterBand(1).ReadRaster()
    else:
        assert ds2.ReadRaster() == ds.GetRasterBand(1).ReadRaster()
        assert ds2.ReadRaster(buf_type = gdal.GDT_UInt16) == ds.GetRasterBand(1).ReadRaster(buf_type = gdal.GDT_UInt16)


@pytest.mark.parametrize("with_dim_x,with_var_x,with_dim_y,with_var_y",
                         [[True, False, True, False],
                          [True, False, False, False],
                          [False, False, True, False],
                          [True, True, True, True]])
def test_multidim_getresampled_new_dims_with_variables(with_dim_x,with_var_x,with_dim_y,with_var_y):

    ds = gdal.Open('../gdrivers/data/small_world.tif')
    srs_ds = ds.GetSpatialRef()
    band = ds.GetRasterBand(1)
    ar = band.AsMDArray()
    assert ar

    drv = gdal.GetDriverByName('MEM')
    mem_ds = drv.CreateMultiDimensional('myds')
    rg = mem_ds.GetRootGroup()

    dimY = None
    if with_dim_y:
        dimY = rg.CreateDimension('dimY', None, None, ds.RasterYSize // 2)
        if with_var_y:
            varY = rg.CreateMDArray(dimY.GetName(), [dimY],
                                    gdal.ExtendedDataType.Create(gdal.GDT_Float64))
            varY.Write(array.array('d', [ 90 - 0.9 - 1.8 * i for i in range(dimY.GetSize()) ]))
            dimY.SetIndexingVariable(varY)

    dimX = None
    if with_dim_x:
        dimX = rg.CreateDimension('dimX', None, None, ds.RasterXSize // 2)
        if with_var_x:
            varX = rg.CreateMDArray(dimX.GetName(), [dimX],
                                    gdal.ExtendedDataType.Create(gdal.GDT_Float64))
            varX.Write(array.array('d', [ -180 + 0.9 + 1.8 * i for i in range(dimX.GetSize()) ]))
            dimX.SetIndexingVariable(varX)

    resampled_ar = ar.GetResampled([dimY, dimX], gdal.GRIORA_Cubic, None)
    assert resampled_ar
    srs = resampled_ar.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == srs_ds.GetAuthorityCode(None)
    dims = resampled_ar.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetSize() == ds.RasterYSize // 2
    assert dims[1].GetSize() == ds.RasterXSize // 2

    expected_ds = gdal.Warp('', ds, options = '-of MEM -ts 200 100 -r cubic')
    assert expected_ds.GetRasterBand(1).ReadRaster() == resampled_ar.Read()


def test_multidim_getresampled_with_srs():

    ds = gdal.Open('data/byte.tif')
    band = ds.GetRasterBand(1)
    ar = band.AsMDArray()
    assert ar

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4267)
    resampled_ar = ar.GetResampled([None] * ar.GetDimensionCount(), gdal.GRIORA_NearestNeighbour, srs)
    assert resampled_ar
    got_srs = resampled_ar.GetSpatialRef()
    assert got_srs is not None
    assert got_srs.GetAuthorityCode(None) == srs.GetAuthorityCode(None)
    dims = resampled_ar.GetDimensions()

    expected_ds = gdal.Warp('', ds, options = '-of MEM -t_srs EPSG:4267 -r nearest')
    assert expected_ds.RasterXSize == dims[1].GetSize()
    assert expected_ds.RasterYSize == dims[0].GetSize()
    assert expected_ds.GetRasterBand(1).ReadRaster() == resampled_ar.Read()

    ixdim = 1
    iydim = 0
    ds2 = resampled_ar.AsClassicDataset(ixdim, iydim)
    assert ds2.GetGeoTransform() == pytest.approx(expected_ds.GetGeoTransform())


def test_multidim_getresampled_3d():

    ds = gdal.Open('../gdrivers/data/small_world.tif')
    ar_b1 = ds.GetRasterBand(1).AsMDArray()

    drv = gdal.GetDriverByName('MEM')
    mem_ds = drv.CreateMultiDimensional('myds')
    rg = mem_ds.GetRootGroup()
    dimBand = rg.CreateDimension('dimBand', None, None, ds.RasterCount)
    dimY = ar_b1.GetDimensions()[0]
    dimX = ar_b1.GetDimensions()[1]
    ar = rg.CreateMDArray("ar", [dimBand, dimY, dimX],
                          gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    ar.SetOffset(1.5)
    ar.SetScale(2.5)
    ar.SetUnit('foo')
    ar.SetNoDataValueDouble(-0.5)

    attr = ar.CreateAttribute('attr', [],
                              gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    assert attr.Write(1) == gdal.CE_None

    srs = ds.GetSpatialRef().Clone()
    srs.SetDataAxisToSRSAxisMapping([1, 2])
    ar.SetSpatialRef(srs)
    for i in range(ds.RasterCount):
        ar[i].Write(ds.GetRasterBand(i+1).ReadRaster())

    resampled_ar = ar.GetResampled([None] * ar.GetDimensionCount(), gdal.GRIORA_NearestNeighbour, None)
    assert resampled_ar
    dims = resampled_ar.GetDimensions()
    assert len(dims) ==  3
    assert dims[0].GetName() == dimBand.GetName()
    assert dims[0].GetSize() == dimBand.GetSize()
    assert dims[1].GetSize() == dimY.GetSize()
    assert dims[2].GetSize() == dimX.GetSize()

    assert resampled_ar.GetOffset() == ar.GetOffset()
    assert resampled_ar.GetScale() == ar.GetScale()
    assert resampled_ar.GetUnit() == ar.GetUnit()
    assert resampled_ar.GetNoDataValueAsRaw() == ar.GetNoDataValueAsRaw()
    block_size = resampled_ar.GetBlockSize()
    assert len(block_size) == 3
    assert block_size[0] == 0
    assert block_size[1] != 0
    assert block_size[2] != 0
    assert resampled_ar.GetAttribute('attr') is not None
    assert len(resampled_ar.GetAttributes()) == 1

    assert resampled_ar.Read() == ds.ReadRaster()



def test_multidim_getresampled_error_single_dim():

    drv = gdal.GetDriverByName('MEM')
    mem_ds = drv.CreateMultiDimensional('myds')
    rg = mem_ds.GetRootGroup()
    dimX = rg.CreateDimension('X', None, None, 3)
    ar = rg.CreateMDArray("ar", [dimX],
                          gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    with gdaltest.error_handler():
        resampled_ar = ar.GetResampled([None], gdal.GRIORA_NearestNeighbour, None)
        assert resampled_ar is None


def test_multidim_getresampled_error_too_large_y():

    drv = gdal.GetDriverByName('MEM')
    mem_ds = drv.CreateMultiDimensional('myds')
    rg = mem_ds.GetRootGroup()
    dimY = rg.CreateDimension('Y', None, None, 4)
    dimX = rg.CreateDimension('X', None, None, 3)
    ar = rg.CreateMDArray("ar", [dimY, dimX],
                          gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    new_dimY = rg.CreateDimension('Y', None, None, 4 * 1000 * 1000 * 1000)
    with gdaltest.error_handler():
        resampled_ar = ar.GetResampled([new_dimY, None], gdal.GRIORA_NearestNeighbour, None)
        assert resampled_ar is None


def test_multidim_getresampled_error_too_large_x():

    drv = gdal.GetDriverByName('MEM')
    mem_ds = drv.CreateMultiDimensional('myds')
    rg = mem_ds.GetRootGroup()
    dimY = rg.CreateDimension('Y', None, None, 4)
    dimX = rg.CreateDimension('X', None, None, 3)
    ar = rg.CreateMDArray("ar", [dimY, dimX],
                          gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    new_dimX = rg.CreateDimension('Y', None, None, 4 * 1000 * 1000 * 1000)
    with gdaltest.error_handler():
        resampled_ar = ar.GetResampled([None, new_dimX], gdal.GRIORA_NearestNeighbour, None)
        assert resampled_ar is None


def test_multidim_getresampled_error_no_geotransform():

    drv = gdal.GetDriverByName('MEM')
    mem_ds = drv.CreateMultiDimensional('myds')
    rg = mem_ds.GetRootGroup()
    dimY = rg.CreateDimension('Y', None, None, 2)
    dimX = rg.CreateDimension('X', None, None, 3)
    ar = rg.CreateMDArray("ar", [dimY, dimX],
                          gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    with gdaltest.error_handler():
        resampled_ar = ar.GetResampled([None, None], gdal.GRIORA_NearestNeighbour, None)
        assert resampled_ar is None


def test_multidim_getresampled_error_extra_dim_not_same():

    ds = gdal.Open('../gdrivers/data/small_world.tif')
    ar_b1 = ds.GetRasterBand(1).AsMDArray()

    drv = gdal.GetDriverByName('MEM')
    mem_ds = drv.CreateMultiDimensional('myds')
    rg = mem_ds.GetRootGroup()
    dimOther = rg.CreateDimension('other', None, None, 2)
    dimY = ar_b1.GetDimensions()[0]
    dimX = ar_b1.GetDimensions()[1]
    ar = rg.CreateMDArray("ar", [dimOther, dimY, dimX],
                          gdal.ExtendedDataType.Create(gdal.GDT_Byte))

    dimOtherNew = rg.CreateDimension('otherNew', None, None, 1)
    with gdaltest.error_handler():
        resampled_ar = ar.GetResampled([dimOtherNew, None, None], gdal.GRIORA_NearestNeighbour, None)
        assert resampled_ar is None


def test_multidim_getresampled_bad_input_dim_count():

    ds = gdal.Open('data/byte.tif')
    band = ds.GetRasterBand(1)
    ar = band.AsMDArray()
    assert ar

    with gdaltest.error_handler():
        resampled_ar = ar.GetResampled([None], gdal.GRIORA_NearestNeighbour, None)
        assert resampled_ar is None

    with gdaltest.error_handler():
        resampled_ar = ar.GetResampled([None, None, None], gdal.GRIORA_NearestNeighbour, None)
        assert resampled_ar is None

