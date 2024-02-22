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
import json
import math

import gdaltest
import pytest

from osgeo import gdal, osr


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


def test_multidim_asarray_epsg_4326():

    ds = gdal.Open("../gdrivers/data/small_world.tif")
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

    ds = gdal.Open("data/byte.tif")
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


@pytest.mark.parametrize(
    "resampling",
    [
        gdal.GRIORA_NearestNeighbour,
        gdal.GRIORA_Bilinear,
        gdal.GRIORA_Cubic,
        gdal.GRIORA_CubicSpline,
        gdal.GRIORA_Lanczos,
        gdal.GRIORA_Average,
        gdal.GRIORA_Mode,
        gdal.GRIORA_Gauss,  # unsupported
        gdal.GRIORA_RMS,
    ],
)
def test_multidim_getresampled(resampling):

    ds = gdal.Open("../gdrivers/data/small_world.tif")
    srs_ds = ds.GetSpatialRef()
    band = ds.GetRasterBand(1)
    ar = band.AsMDArray()
    assert ar

    if resampling == gdal.GRIORA_Gauss:
        with gdal.quiet_errors():
            resampled_ar = ar.GetResampled(
                [None] * ar.GetDimensionCount(), resampling, None
            )
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
    assert dims[0].GetName() == "dimY"
    assert dims[0].GetSize() == ds.RasterYSize
    assert dims[1].GetName() == "dimX"
    assert dims[1].GetSize() == ds.RasterXSize

    assert (
        resampled_ar.Read(buffer_datatype=gdal.ExtendedDataType.CreateString())
        != gdal.CE_None
    )

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
        assert ds2.ReadRaster(buf_type=gdal.GDT_UInt16) == ds.GetRasterBand(
            1
        ).ReadRaster(buf_type=gdal.GDT_UInt16)


@pytest.mark.parametrize(
    "with_dim_x,with_var_x,with_dim_y,with_var_y",
    [
        [True, False, True, False],
        [True, False, False, False],
        [False, False, True, False],
        [True, True, True, True],
    ],
)
def test_multidim_getresampled_new_dims_with_variables(
    with_dim_x, with_var_x, with_dim_y, with_var_y
):

    ds = gdal.Open("../gdrivers/data/small_world.tif")
    srs_ds = ds.GetSpatialRef()
    band = ds.GetRasterBand(1)
    ar = band.AsMDArray()
    assert ar

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()

    dimY = None
    if with_dim_y:
        dimY = rg.CreateDimension("dimY", None, None, ds.RasterYSize // 2)
        if with_var_y:
            varY = rg.CreateMDArray(
                dimY.GetName(), [dimY], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            varY.Write(
                array.array("d", [90 - 0.9 - 1.8 * i for i in range(dimY.GetSize())])
            )
            dimY.SetIndexingVariable(varY)

    dimX = None
    if with_dim_x:
        dimX = rg.CreateDimension("dimX", None, None, ds.RasterXSize // 2)
        if with_var_x:
            varX = rg.CreateMDArray(
                dimX.GetName(), [dimX], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            varX.Write(
                array.array("d", [-180 + 0.9 + 1.8 * i for i in range(dimX.GetSize())])
            )
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

    expected_ds = gdal.Warp("", ds, options="-of MEM -ts 200 100 -r cubic")
    assert expected_ds.GetRasterBand(1).ReadRaster() == resampled_ar.Read()


def test_multidim_getresampled_with_srs():

    ds = gdal.Open("data/byte.tif")
    band = ds.GetRasterBand(1)
    ar = band.AsMDArray()
    assert ar

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4267)
    resampled_ar = ar.GetResampled(
        [None] * ar.GetDimensionCount(), gdal.GRIORA_NearestNeighbour, srs
    )
    assert resampled_ar
    got_srs = resampled_ar.GetSpatialRef()
    assert got_srs is not None
    assert got_srs.GetAuthorityCode(None) == srs.GetAuthorityCode(None)
    dims = resampled_ar.GetDimensions()

    expected_ds = gdal.Warp("", ds, options="-of MEM -t_srs EPSG:4267 -r nearest")
    assert expected_ds.RasterXSize == dims[1].GetSize()
    assert expected_ds.RasterYSize == dims[0].GetSize()
    assert expected_ds.GetRasterBand(1).ReadRaster() == resampled_ar.Read()

    ixdim = 1
    iydim = 0
    ds2 = resampled_ar.AsClassicDataset(ixdim, iydim)
    assert ds2.GetGeoTransform() == pytest.approx(expected_ds.GetGeoTransform())


def test_multidim_getresampled_3d():

    ds = gdal.Open("../gdrivers/data/small_world.tif")
    ar_b1 = ds.GetRasterBand(1).AsMDArray()

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()
    dimBand = rg.CreateDimension("dimBand", None, None, ds.RasterCount)
    dimY = ar_b1.GetDimensions()[0]
    dimX = ar_b1.GetDimensions()[1]
    ar = rg.CreateMDArray(
        "ar", [dimBand, dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_Byte)
    )
    ar.SetOffset(1.5)
    ar.SetScale(2.5)
    ar.SetUnit("foo")
    ar.SetNoDataValueDouble(-0.5)

    attr = ar.CreateAttribute(
        "attr", [], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    assert attr.Write(1) == gdal.CE_None

    srs = ds.GetSpatialRef().Clone()
    srs.SetDataAxisToSRSAxisMapping([2, 3])
    ar.SetSpatialRef(srs)
    for i in range(ds.RasterCount):
        ar[i].Write(ds.GetRasterBand(i + 1).ReadRaster())

    resampled_ar = ar.GetResampled(
        [None] * ar.GetDimensionCount(), gdal.GRIORA_NearestNeighbour, None
    )
    assert resampled_ar
    dims = resampled_ar.GetDimensions()
    assert len(dims) == 3
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
    assert resampled_ar.GetAttribute("attr") is not None
    assert len(resampled_ar.GetAttributes()) == 1

    assert resampled_ar.Read() == ds.ReadRaster()


def test_multidim_getresampled_error_single_dim():

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()
    dimX = rg.CreateDimension("X", None, None, 3)
    ar = rg.CreateMDArray("ar", [dimX], gdal.ExtendedDataType.Create(gdal.GDT_Byte))
    with gdal.quiet_errors():
        resampled_ar = ar.GetResampled([None], gdal.GRIORA_NearestNeighbour, None)
        assert resampled_ar is None


def test_multidim_getresampled_error_too_large_y():

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()
    dimY = rg.CreateDimension("Y", None, None, 4)
    dimX = rg.CreateDimension("X", None, None, 3)
    ar = rg.CreateMDArray(
        "ar", [dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_Byte)
    )
    new_dimY = rg.CreateDimension("Y", None, None, 4 * 1000 * 1000 * 1000)
    with gdal.quiet_errors():
        resampled_ar = ar.GetResampled(
            [new_dimY, None], gdal.GRIORA_NearestNeighbour, None
        )
        assert resampled_ar is None


def test_multidim_getresampled_error_too_large_x():

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()
    dimY = rg.CreateDimension("Y", None, None, 4)
    dimX = rg.CreateDimension("X", None, None, 3)
    ar = rg.CreateMDArray(
        "ar", [dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_Byte)
    )
    new_dimX = rg.CreateDimension("Y", None, None, 4 * 1000 * 1000 * 1000)
    with gdal.quiet_errors():
        resampled_ar = ar.GetResampled(
            [None, new_dimX], gdal.GRIORA_NearestNeighbour, None
        )
        assert resampled_ar is None


def test_multidim_getresampled_error_no_geotransform():

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()
    dimY = rg.CreateDimension("Y", None, None, 2)
    dimX = rg.CreateDimension("X", None, None, 3)
    ar = rg.CreateMDArray(
        "ar", [dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_Byte)
    )
    with gdal.quiet_errors():
        resampled_ar = ar.GetResampled([None, None], gdal.GRIORA_NearestNeighbour, None)
        assert resampled_ar is None


def test_multidim_getresampled_error_extra_dim_not_same():

    ds = gdal.Open("../gdrivers/data/small_world.tif")
    ar_b1 = ds.GetRasterBand(1).AsMDArray()

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()
    dimOther = rg.CreateDimension("other", None, None, 2)
    dimY = ar_b1.GetDimensions()[0]
    dimX = ar_b1.GetDimensions()[1]
    ar = rg.CreateMDArray(
        "ar", [dimOther, dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_Byte)
    )

    dimOtherNew = rg.CreateDimension("otherNew", None, None, 1)
    with gdal.quiet_errors():
        resampled_ar = ar.GetResampled(
            [dimOtherNew, None, None], gdal.GRIORA_NearestNeighbour, None
        )
        assert resampled_ar is None


def test_multidim_getresampled_bad_input_dim_count():

    ds = gdal.Open("data/byte.tif")
    band = ds.GetRasterBand(1)
    ar = band.AsMDArray()
    assert ar

    with gdal.quiet_errors():
        resampled_ar = ar.GetResampled([None], gdal.GRIORA_NearestNeighbour, None)
        assert resampled_ar is None

    with gdal.quiet_errors():
        resampled_ar = ar.GetResampled(
            [None, None, None], gdal.GRIORA_NearestNeighbour, None
        )
        assert resampled_ar is None


def test_multidim_getgridded():

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()

    dimOther = rg.CreateDimension("other", None, None, 2)
    other = rg.CreateMDArray(
        "other", [dimOther], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    assert other

    dimNode = rg.CreateDimension("node", None, None, 6)
    varX = rg.CreateMDArray(
        "varX", [dimNode], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    assert varX
    varX.Write(array.array("d", [0, 0, 1, 1, 2, 2]))
    varY = rg.CreateMDArray(
        "varY", [dimNode], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    assert varY
    varY.Write(array.array("d", [0, 1, 0, 1, 0, 1]))

    ar = rg.CreateMDArray(
        "ar", [dimOther, dimNode], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    assert (
        ar.Write(array.array("d", [1, 2, 3, 4, 5, 6, 20, 30, 20, 30, 20, 30]))
        == gdal.CE_None
    )

    with gdal.quiet_errors():
        assert ar.GetGridded("invdist") is None
        assert ar.GetGridded("invdist", varX) is None
        assert ar.GetGridded("invdist", None, varY) is None

        assert ar.GetGridded("invalid", varX, varY) is None

        zero_dim_ar = rg.CreateMDArray(
            "zero_dim_ar", [], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        )
        assert zero_dim_ar.GetGridded("invdist", varX, varY) is None

        dimUnrelated = rg.CreateDimension("unrelated", None, None, 2)
        unrelated = rg.CreateMDArray(
            "unrelated", [dimUnrelated], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        )
        unrelated.Write(array.array("d", [0, 0, 1, 1, 2, 2]))
        assert ar.GetGridded("invdist", unrelated, varY) is None
        assert ar.GetGridded("invdist", varX, unrelated) is None

        non_numeric = rg.CreateMDArray(
            "non_numeric", [dimNode], gdal.ExtendedDataType.CreateString()
        )
        assert ar.GetGridded("invdist", non_numeric, varY) is None
        assert ar.GetGridded("invdist", varX, non_numeric) is None

        assert non_numeric.GetGridded("invdist", varX, varY) is None

        assert ar.GetGridded("invdist", ar, varX) is None
        assert ar.GetGridded("invdist", varX, ar) is None

        dimOneSample = rg.CreateDimension("dimOneSample", None, None, 1)
        varXOneSample = rg.CreateMDArray(
            "varXOneSample",
            [dimOneSample],
            gdal.ExtendedDataType.Create(gdal.GDT_Float64),
        )
        assert varXOneSample.GetGridded("invdist", varXOneSample, varXOneSample) is None

        dimNodeHuge = rg.CreateDimension("node_huge", None, None, 20 * 1024 * 1024)
        varXHuge = rg.CreateMDArray(
            "varXHuge", [dimNodeHuge], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        )
        varYHuge = rg.CreateMDArray(
            "varYHuge", [dimNodeHuge], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        )
        arHuge = rg.CreateMDArray(
            "arHuge", [dimNodeHuge], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        )
        assert arHuge.GetGridded("invdist", varXHuge, varYHuge) is None

    # Explicit varX, varY provided
    gridded = ar.GetGridded("invdist", varX, varY)
    assert gridded
    assert gridded.GetDimensionCount() == 3
    assert gridded.GetDimensions()[0].GetName() == "other"
    assert gridded.GetDimensions()[1].GetName() == "dimY"
    assert gridded.GetDimensions()[1].GetSize() == 2
    dimY = gridded.GetDimensions()[1].GetIndexingVariable()
    assert dimY.Read() == array.array("d", [0, 1])
    assert gridded.GetDimensions()[2].GetName() == "dimX"
    assert gridded.GetDimensions()[2].GetSize() == 3
    dimX = gridded.GetDimensions()[2].GetIndexingVariable()
    assert dimX.Read() == array.array("d", [0, 1, 2])

    # varX and varY guessed from coordinates attribute
    coordinates = ar.CreateAttribute(
        "coordinates", [], gdal.ExtendedDataType.CreateString()
    )

    # Not enough coordinate variables
    assert coordinates.WriteString("other varY") == 0
    with gdal.quiet_errors():
        assert ar.GetGridded("invdist:nodata=nan") is None

    # Too many coordinate variables
    assert coordinates.WriteString("other unrelated varY varX") == 0
    with gdal.quiet_errors():
        assert ar.GetGridded("invdist:nodata=nan") is None

    # poYArray->GetDimensions()[0]->GetFullName() != poXArray->GetDimensions()[0]->GetFullName()
    assert coordinates.WriteString("other unrelated varX") == 0
    with gdal.quiet_errors():
        assert ar.GetGridded("invdist:nodata=nan") is None

    assert coordinates.WriteString("other varY varX") == 0
    ar.SetUnit("foo")
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    ar.SetSpatialRef(srs)
    gridded = ar.GetGridded("nearest:radius1=2:radius2=2:nodata=nan")
    assert gridded
    assert math.isnan(gridded.GetNoDataValueAsDouble())
    assert gridded.GetDimensionCount() == 3
    assert gridded.GetDimensions()[0].GetName() == "other"
    assert gridded.GetDimensions()[1].GetName() == "dimY"
    assert gridded.GetDimensions()[1].GetSize() == 2
    dimY = gridded.GetDimensions()[1].GetIndexingVariable()
    assert dimY.Read() == array.array("d", [0, 1])
    assert gridded.GetDimensions()[2].GetName() == "dimX"
    assert gridded.GetDimensions()[2].GetSize() == 3
    dimX = gridded.GetDimensions()[2].GetIndexingVariable()
    assert dimX.Read() == array.array("d", [0, 1, 2])
    assert gridded.GetBlockSize() == [0, 256, 256]
    assert gridded.GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert gridded.GetUnit() == ar.GetUnit()
    assert gridded.GetSpatialRef().IsSame(ar.GetSpatialRef())
    assert (
        gridded.GetAttribute("coordinates").Read()
        == ar.GetAttribute("coordinates").Read()
    )
    assert len(gridded.GetAttributes()) == len(ar.GetAttributes())
    # print(gridded.ReadAsArray(array_start_idx=[0, 0, 0], count=[1, 2, 3]))
    assert gridded.Read(array_start_idx=[0, 0, 0], count=[1, 2, 3]) == array.array(
        "d", [1, 3, 5, 2, 4, 6]
    )
    assert gridded.Read(array_start_idx=[0, 1, 2], count=[1, 1, 1]) == array.array(
        "d", [6]
    )
    assert gridded.Read(array_start_idx=[1, 0, 0], count=[1, 2, 3]) == array.array(
        "d", [20, 20, 20, 30, 30, 30]
    )

    with gdal.quiet_errors():
        # Cannot read more than one sample in the non X/Y dimensions
        assert gridded.Read() is None

        # Negative array_step not support currently
        assert (
            gridded.Read(
                array_start_idx=[0, 0, 2], count=[1, 2, 3], array_step=[1, 1, -1]
            )
            is None
        )

        assert ar.GetGridded("invdist", options=["RESOLUTION=0"]) is None

        assert ar.GetGridded("invdist", options=["RESOLUTION=1e-200"]) is None

    gridded = ar.GetGridded("average:radius1=1:radius2=1", options=["RESOLUTION=0.5"])
    assert gridded
    assert gridded.GetDimensions()[1].GetSize() == 3
    assert gridded.GetDimensions()[2].GetSize() == 5
    # print(gridded.ReadAsArray(array_start_idx = [0, 0, 0], count = [1, 3, 5]))
    assert gridded.Read(array_start_idx=[0, 0, 0], count=[1, 3, 5]) == array.array(
        "d",
        [
            2,
            2,
            3.25,
            4,
            4.666666666666666667,
            1.5,
            2.5,
            3.5,
            4.5,
            5.5,
            2.333333333333333333,
            3.0,
            3.75,
            5,
            5,
        ],
    )


@gdaltest.enable_exceptions()
def test_multidim_asclassicsubdataset_band_metadata():

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()

    dimOther = rg.CreateDimension("other", None, None, 2)
    other = rg.CreateMDArray(
        "other", [dimOther], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    other.Write(array.array("d", [10.5, 20]))
    dimOther.SetIndexingVariable(other)
    numeric_attr = other.CreateAttribute(
        "numeric_attr", [], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    assert numeric_attr.Write(1) == gdal.CE_None
    string_attr = other.CreateAttribute(
        "string_attr", [], gdal.ExtendedDataType.CreateString()
    )
    assert string_attr.Write("string_attr_value") == gdal.CE_None

    dimX = rg.CreateDimension("X", None, None, 2)
    X = rg.CreateMDArray("X", [dimX], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    X.Write(array.array("d", [10, 20]))
    dimX.SetIndexingVariable(X)
    dimY = rg.CreateDimension("Y", None, None, 2)

    ar = rg.CreateMDArray(
        "ar", [dimOther, dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )

    aux_var = rg.CreateMDArray(
        "aux_var", [dimOther], gdal.ExtendedDataType.CreateString()
    )
    aux_var.Write(["foo", "bar"])

    with pytest.raises(
        Exception, match="Root group should be provided when BAND_METADATA is set"
    ):
        ar.AsClassicDataset(2, 1, None, ["BAND_METADATA=[]"])

    with pytest.raises(Exception, match="Invalid JSON content for BAND_METADATA"):
        ar.AsClassicDataset(2, 1, rg, ["BAND_METADATA=invalid"])

    with pytest.raises(Exception, match="Value of BAND_METADATA should be an array"):
        ar.AsClassicDataset(2, 1, rg, ["BAND_METADATA=false"])

    band_metadata = [{"item_name": "name"}]
    with pytest.raises(Exception, match=r"BAND_METADATA\[0\]\[\"array\"\] is missing"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = [{"array": "/i/do/not/exist", "item_name": "name"}]
    with pytest.raises(Exception, match="Array /i/do/not/exist cannot be found"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = [{"array": "/ar", "item_name": "name"}]
    with pytest.raises(Exception, match="Array /ar is not a 1D array"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = [{"array": "/X", "item_name": "name"}]
    with pytest.raises(
        Exception,
        match="Dimension X of array /X is not a non-X/Y dimension of array ar",
    ):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = [{"array": "/other"}]
    with pytest.raises(
        Exception, match=r"BAND_METADATA\[0\]\[\"item_name\"\] is missing"
    ):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = [{"array": "/other", "item_name": "other", "item_value": "%f %f"}]
    with pytest.raises(Exception, match="formatters should be specified at most once"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = [{"array": "/other", "item_name": "other", "item_value": "%d"}]
    with pytest.raises(
        Exception, match=r"only %\[x\]\[\.y\]f\|g or \%s formatters are accepted"
    ):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = [{"array": "/other", "item_name": "other", "item_value": "%"}]
    with pytest.raises(Exception, match="is invalid at offset 0"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = [
        {"array": "/other", "item_name": "other", "item_value": "${numeric_attr"}
    ]
    with pytest.raises(Exception, match="is invalid at offset 0"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = [
        {"array": "/other", "item_name": "other", "item_value": "${i_do_not_exist}"}
    ]
    with pytest.raises(Exception, match="i_do_not_exist is not an attribute of other"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = [{"array": "/aux_var", "item_name": "AUX_VAR", "item_value": "%f"}]
    with pytest.raises(Exception, match="Data type of other array is not numeric"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = []
    ds = ar.AsClassicDataset(2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)])
    assert ds.GetRasterBand(1).GetMetadata() == {
        "DIM_other_INDEX": "0",
        "DIM_other_VALUE": "10.5",
    }

    band_metadata = [{"array": "/other", "item_name": "OTHER"}]
    ds = ar.AsClassicDataset(2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)])
    assert ds.GetRasterBand(1).GetMetadata() == {
        "DIM_other_INDEX": "0",
        "OTHER": "10.5",
    }

    band_metadata = [
        {
            "array": "/other",
            "item_name": "OTHER",
            "item_value": "%s in pct(%%) with numeric_attr=${numeric_attr} and string_attr=${string_attr}",
        }
    ]
    ds = ar.AsClassicDataset(2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)])
    assert ds.GetRasterBand(1).GetMetadata() == {
        "DIM_other_INDEX": "0",
        "OTHER": "10.5 in pct(%) with numeric_attr=1 and string_attr=string_attr_value",
    }

    band_metadata = [{"array": "/other", "item_name": "OTHER", "item_value": "%f"}]
    ds = ar.AsClassicDataset(2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)])
    assert ds.GetRasterBand(1).GetMetadata() == {
        "DIM_other_INDEX": "0",
        "OTHER": "10.500000",
    }

    band_metadata = [{"array": "/other", "item_name": "OTHER", "item_value": "%2.3f"}]
    ds = ar.AsClassicDataset(2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)])
    assert ds.GetRasterBand(1).GetMetadata() == {
        "DIM_other_INDEX": "0",
        "OTHER": "10.500",
    }

    band_metadata = [{"array": "/other", "item_name": "OTHER", "item_value": "%g"}]
    ds = ar.AsClassicDataset(2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)])
    assert float(ds.GetRasterBand(1).GetMetadataItem("OTHER")) == 10.5

    band_metadata = [
        {"array": "/other", "item_name": "OTHER"},
        {
            "array": "/other",
            "item_name": "OTHER_STRING_ATTR",
            "item_value": "${string_attr}",
        },
        {"array": "/aux_var", "item_name": "AUX_VAR"},
    ]
    ds = ar.AsClassicDataset(2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)])
    assert ds.GetRasterBand(1).GetMetadata() == {
        "DIM_other_INDEX": "0",
        "OTHER": "10.5",
        "OTHER_STRING_ATTR": "string_attr_value",
        "AUX_VAR": "foo",
    }
    assert ds.GetRasterBand(2).GetMetadata() == {
        "DIM_other_INDEX": "1",
        "OTHER": "20",
        "OTHER_STRING_ATTR": "string_attr_value",
        "AUX_VAR": "bar",
    }


@gdaltest.enable_exceptions()
def test_multidim_SubsetDimensionFromSelection():

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()

    numeric_attr = rg.CreateAttribute(
        "numeric_attr", [], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    assert numeric_attr.Write(1) == gdal.CE_None

    subgroup = rg.CreateGroup("band_properties")

    dimBand = rg.CreateDimension("band", None, None, 3)
    validity = subgroup.CreateMDArray(
        "validity", [dimBand], gdal.ExtendedDataType.Create(gdal.GDT_Int32)
    )
    validity.Write(array.array("i", [1, 0, 1]))
    dimBand.SetIndexingVariable(validity)

    dimX = rg.CreateDimension("X", None, None, 2)
    X = rg.CreateMDArray("X", [dimX], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    X.Write(array.array("d", [10, 20]))
    dimX.SetIndexingVariable(X)

    dimY = rg.CreateDimension("Y", None, None, 2)

    ar = rg.CreateMDArray(
        "ar", [dimBand, dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    assert ar.Write(array.array("d", [i for i in range(3 * 2 * 2)])) == gdal.CE_None
    ar.SetUnit("foo")
    ar.SetNoDataValueDouble(5)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    ar.SetSpatialRef(srs)

    ar_numeric_attr = ar.CreateAttribute(
        "ar_numeric_attr", [], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    assert ar_numeric_attr.Write(1) == gdal.CE_None

    subgroup = rg.CreateGroup("subgroup")

    indexed_twice_by_band = subgroup.CreateMDArray(
        "indexed_twice_by_band",
        [dimBand, dimX, dimBand],
        gdal.ExtendedDataType.Create(gdal.GDT_Float64),
    )
    assert (
        indexed_twice_by_band.Write(array.array("d", [i for i in range(3 * 3 * 2)]))
        == gdal.CE_None
    )

    rg.CreateMDArray("non_numeric_ar", [dimBand], gdal.ExtendedDataType.CreateString())

    too_large_dim = rg.CreateDimension(
        "too_large_dim", None, None, 10 * 1024 * 1024 + 1
    )
    rg.CreateMDArray(
        "too_large_dim_ar", [too_large_dim], gdal.ExtendedDataType.Create(gdal.GDT_Byte)
    )

    same_value = rg.CreateMDArray(
        "same_value", [dimX], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    same_value.Write(array.array("d", [10, 10]))

    with pytest.raises(Exception, match="Invalid value for selection"):
        rg.SubsetDimensionFromSelection("")
    with pytest.raises(Exception, match="Invalid value for selection"):
        rg.SubsetDimensionFromSelection("/band_properties/validity")
    with pytest.raises(Exception, match="Non-numeric value in selection criterion"):
        rg.SubsetDimensionFromSelection("/band_properties/validity='foo'")
    with pytest.raises(Exception, match="Cannot find array /i/do/not/exist"):
        rg.SubsetDimensionFromSelection("/i/do/not/exist=1")
    with pytest.raises(Exception, match="Array /ar is not single dimensional"):
        rg.SubsetDimensionFromSelection("/ar=1")
    with pytest.raises(Exception, match="Array /non_numeric_ar is not of numeric type"):
        rg.SubsetDimensionFromSelection("/non_numeric_ar=1")
    with pytest.raises(Exception, match="Too many values in /too_large_dim_ar"):
        rg.SubsetDimensionFromSelection("/too_large_dim_ar=1")
    with pytest.raises(
        Exception, match="No value in /band_properties/validity matching 2.000000"
    ):
        rg.SubsetDimensionFromSelection("/band_properties/validity=2")

    # In C++, should return the same object as rg, but we can't easily check
    rg.SubsetDimensionFromSelection("/same_value=10")

    rg_subset = rg.SubsetDimensionFromSelection("/band_properties/validity=1")
    assert rg_subset
    dims = rg_subset.GetDimensions()
    assert len(dims) == 4
    bandFound = False
    for dim in dims:
        if dim.GetName() == "band":
            bandFound = True
            assert dim.GetFullName() == "/band"
            assert dim.GetSize() == 2
            var = dim.GetIndexingVariable()
            assert var
            assert var.GetName() == "validity"
            assert var.GetFullName() == "/band_properties/validity"
    assert bandFound

    assert rg_subset.GetGroupNames() == rg.GetGroupNames()
    with pytest.raises(Exception, match="Group i_do_not_exist does not exist"):
        assert rg_subset.OpenGroup("i_do_not_exist") is None
    assert len(rg_subset.GetAttributes()) == 1
    assert rg_subset.GetAttribute("numeric_attr") is not None

    subgroup_subset = rg_subset.OpenGroup("band_properties")
    assert subgroup_subset.GetFullName() == "/band_properties"
    assert subgroup_subset.GetMDArrayNames() == ["validity"]
    validity_subset = subgroup_subset.OpenMDArray("validity")
    assert validity_subset.GetFullName() == "/band_properties/validity"
    assert validity_subset.Read() == array.array("i", [1, 1])

    assert rg_subset.GetMDArrayNames() == rg.GetMDArrayNames()
    with pytest.raises(Exception, match="Array i_do_not_exist does not exist"):
        assert rg_subset.OpenMDArray("i_do_not_exist") is None
    ar_subset = rg_subset.OpenMDArray("ar")
    assert ar_subset.GetFullName() == "/ar"
    assert [dim.GetFullName() for dim in ar_subset.GetDimensions()] == [
        "/band",
        "/Y",
        "/X",
    ]
    assert ar_subset.GetDataType() == ar.GetDataType()
    assert ar_subset.Read() == array.array(
        "d", [0.0, 1.0, 2.0, 3.0, 8.0, 9.0, 10.0, 11.0]
    )
    assert ar_subset.Read(
        array_start_idx=[1, 0, 0], array_step=[-1, 1, 1]
    ) == array.array("d", [8.0, 9.0, 10.0, 11.0, 0.0, 1.0, 2.0, 3.0])
    assert len(ar_subset.GetAttributes()) == 1
    assert ar_subset.GetAttribute("ar_numeric_attr") is not None
    assert ar_subset.GetUnit() == ar.GetUnit()
    assert ar_subset.GetSpatialRef().IsSame(ar.GetSpatialRef())
    assert ar_subset.GetNoDataValueAsRaw() == ar.GetNoDataValueAsRaw()
    assert ar_subset.GetBlockSize() == [0, 1, 0]

    subgroup_subset = rg_subset.OpenGroup("subgroup")
    indexed_twice_by_band_subset = subgroup_subset.OpenMDArray("indexed_twice_by_band")
    assert (
        indexed_twice_by_band_subset.GetFullName() == "/subgroup/indexed_twice_by_band"
    )
    assert indexed_twice_by_band_subset.Read() == array.array(
        "d", [0.0, 2.0, 3.0, 5.0, 12.0, 14.0, 15.0, 17.0]
    )
    assert indexed_twice_by_band_subset.Read(
        array_start_idx=[1, 0, 0], array_step=[-1, 1, 1]
    ) == array.array("d", [12.0, 14.0, 15.0, 17.0, 0.0, 2.0, 3.0, 5.0])


@gdaltest.enable_exceptions()
def test_multidim_CreateRasterAttributeTableFromMDArrays():

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()
    dim = rg.CreateDimension("dim", None, None, 2)
    other_dim = rg.CreateDimension("other_dim", None, None, 2)
    ar_double = rg.CreateMDArray(
        "ar_double", [dim], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    ar_int = rg.CreateMDArray(
        "ar_int", [dim], gdal.ExtendedDataType.Create(gdal.GDT_Int32)
    )
    ar_string = rg.CreateMDArray(
        "ar_string", [dim], gdal.ExtendedDataType.CreateString()
    )
    ar_other_dim = rg.CreateMDArray(
        "ar_other_dim", [other_dim], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    ar_2D = rg.CreateMDArray(
        "ar_2D", [dim, dim], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )

    with pytest.raises(Exception, match="apoArrays should not be empty"):
        gdal.CreateRasterAttributeTableFromMDArrays(gdal.GRTT_ATHEMATIC, [])
    with pytest.raises(Exception, match="object of wrong GDALMDArrayHS"):
        gdal.CreateRasterAttributeTableFromMDArrays(gdal.GRTT_ATHEMATIC, [None])
    with pytest.raises(Exception, match="not a sequence"):
        gdal.CreateRasterAttributeTableFromMDArrays(gdal.GRTT_ATHEMATIC, 1)
    with pytest.raises(Exception, match="object of wrong GDALMDArrayHS"):
        gdal.CreateRasterAttributeTableFromMDArrays(gdal.GRTT_ATHEMATIC, [dim])
    with pytest.raises(Exception, match=r"apoArrays\[0\] has a dimension count != 1"):
        gdal.CreateRasterAttributeTableFromMDArrays(gdal.GRTT_ATHEMATIC, [ar_2D])
    with pytest.raises(
        Exception,
        match=r"apoArrays\[1\] does not have the same dimension has apoArrays\[0\]",
    ):
        gdal.CreateRasterAttributeTableFromMDArrays(
            gdal.GRTT_ATHEMATIC, [ar_double, ar_other_dim]
        )
    with pytest.raises(Exception, match="nUsages != nArrays"):
        gdal.CreateRasterAttributeTableFromMDArrays(
            gdal.GRTT_ATHEMATIC, [ar_double], [gdal.GFU_Generic, gdal.GFU_Generic]
        )
    with pytest.raises(Exception, match="not a sequence"):
        gdal.CreateRasterAttributeTableFromMDArrays(gdal.GRTT_ATHEMATIC, [ar_double], 1)
    with pytest.raises(Exception, match="not a valid GDALRATFieldUsage"):
        gdal.CreateRasterAttributeTableFromMDArrays(
            gdal.GRTT_ATHEMATIC, [ar_double], [None]
        )
    with pytest.raises(Exception, match="not a valid GDALRATFieldUsage"):
        gdal.CreateRasterAttributeTableFromMDArrays(
            gdal.GRTT_ATHEMATIC, [ar_double], [-1]
        )
    with pytest.raises(Exception, match="not a valid GDALRATFieldUsage"):
        gdal.CreateRasterAttributeTableFromMDArrays(
            gdal.GRTT_ATHEMATIC, [ar_double], [gdal.GFU_MaxCount]
        )

    rat = gdal.CreateRasterAttributeTableFromMDArrays(
        gdal.GRTT_ATHEMATIC, [ar_double, ar_int, ar_string]
    )
    assert rat.GetTableType() == gdal.GRTT_ATHEMATIC
    assert rat.GetColumnCount() == 3
    assert rat.GetRowCount() == 2
    assert rat.Clone().GetColumnCount() == 3
    assert rat.GetNameOfCol(-1) is None
    assert rat.GetNameOfCol(rat.GetColumnCount()) is None
    assert rat.GetNameOfCol(0) == "ar_double"
    assert rat.GetUsageOfCol(-1) == gdal.GFU_Generic
    assert rat.GetUsageOfCol(0) == gdal.GFU_Generic
    assert rat.GetUsageOfCol(rat.GetColumnCount()) == gdal.GFU_Generic
    assert rat.GetTypeOfCol(-1) == gdal.GFT_Integer
    assert rat.GetTypeOfCol(rat.GetColumnCount()) == gdal.GFT_Integer
    assert rat.GetTypeOfCol(0) == gdal.GFT_Real
    assert rat.GetTypeOfCol(1) == gdal.GFT_Integer
    assert rat.GetTypeOfCol(2) == gdal.GFT_String

    ar_double.Write([0.5, 1.5])

    icol = 0
    assert rat.GetValueAsDouble(-1, icol) == 0
    assert rat.GetValueAsDouble(0, icol) == 0.5
    assert rat.GetValueAsDouble(1, icol) == 1.5
    assert rat.GetValueAsDouble(2, icol) == 0

    assert rat.ReadValuesIOAsDouble(icol, 0, 2) == [0.5, 1.5]
    with pytest.raises(Exception, match="Invalid iStartRow/iLength"):
        rat.ReadValuesIOAsDouble(icol, -1, 1)
    with pytest.raises(Exception, match="Invalid iStartRow/iLength"):
        rat.ReadValuesIOAsDouble(icol, 0, rat.GetRowCount() + 1)
    with pytest.raises(Exception, match="invalid length"):
        rat.ReadValuesIOAsDouble(icol, 0, -1)
    with pytest.raises(Exception, match="Invalid iField"):
        rat.ReadValuesIOAsDouble(-1, 0, 1)
    with pytest.raises(Exception, match="Invalid iField"):
        rat.ReadValuesIOAsDouble(rat.GetColumnCount(), 0, 1)

    ar_int.Write([1, 2])

    icol = 1
    assert rat.GetValueAsInt(-1, icol) == 0
    assert rat.GetValueAsInt(0, icol) == 1
    assert rat.GetValueAsInt(1, icol) == 2
    assert rat.GetValueAsInt(2, icol) == 0

    assert rat.ReadValuesIOAsInteger(icol, 0, 2) == [1, 2]
    with pytest.raises(Exception, match="Invalid iStartRow/iLength"):
        rat.ReadValuesIOAsInteger(icol, -1, 1)
    with pytest.raises(Exception, match="Invalid iStartRow/iLength"):
        rat.ReadValuesIOAsInteger(icol, 0, rat.GetRowCount() + 1)
    with pytest.raises(Exception, match="invalid length"):
        rat.ReadValuesIOAsInteger(icol, 0, -1)
    with pytest.raises(Exception, match="Invalid iField"):
        rat.ReadValuesIOAsInteger(-1, 0, 1)
    with pytest.raises(Exception, match="Invalid iField"):
        rat.ReadValuesIOAsInteger(rat.GetColumnCount(), 0, 1)

    ar_string.Write(["foo", "bar"])

    icol = 2
    assert rat.GetValueAsString(-1, icol) is None
    assert rat.GetValueAsString(0, icol) == "foo"
    assert rat.GetValueAsString(1, icol) == "bar"
    assert rat.GetValueAsString(2, icol) is None

    assert rat.ReadValuesIOAsString(icol, 0, 2) == ["foo", "bar"]
    with pytest.raises(Exception, match="Invalid iStartRow/iLength"):
        rat.ReadValuesIOAsString(icol, -1, 1)
    with pytest.raises(Exception, match="Invalid iStartRow/iLength"):
        rat.ReadValuesIOAsString(icol, 0, rat.GetRowCount() + 1)
    with pytest.raises(Exception, match="invalid length"):
        rat.ReadValuesIOAsString(icol, 0, -1)
    with pytest.raises(Exception, match="Invalid iField"):
        rat.ReadValuesIOAsString(-1, 0, 1)
    with pytest.raises(Exception, match="Invalid iField"):
        rat.ReadValuesIOAsString(rat.GetColumnCount(), 0, 1)

    with pytest.raises(
        Exception,
        match=r"GDALRasterAttributeTableFromMDArrays::SetValue\(\): not supported",
    ):
        rat.SetValueAsString(0, 0, "foo")
    with pytest.raises(
        Exception,
        match=r"GDALRasterAttributeTableFromMDArrays::SetValue\(\): not supported",
    ):
        rat.SetValueAsInt(0, 0, 0)
    with pytest.raises(
        Exception,
        match=r"GDALRasterAttributeTableFromMDArrays::SetValue\(\): not supported",
    ):
        rat.SetValueAsDouble(0, 0, 0.5)
    assert rat.ChangesAreWrittenToFile() == False
    with pytest.raises(
        Exception,
        match=r"GDALRasterAttributeTableFromMDArrays::SetTableType\(\): not supported",
    ):
        rat.SetTableType(gdal.GRTT_ATHEMATIC)
    rat.RemoveStatistics()

    rat = gdal.CreateRasterAttributeTableFromMDArrays(
        gdal.GRTT_ATHEMATIC,
        [ar_double, ar_int, ar_string],
        [gdal.GFU_Generic, gdal.GFU_PixelCount, gdal.GFU_Generic],
    )
    assert rat.GetUsageOfCol(1) == gdal.GFU_PixelCount
    assert rat.GetColOfUsage(gdal.GFU_PixelCount) == 1
    assert rat.GetColOfUsage(gdal.GFU_Min) == -1
