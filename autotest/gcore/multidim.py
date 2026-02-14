#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test non-driver specific multidimensional support
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2021, Even Rouault <even.rouault@spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import itertools
import json
import math
import struct

import gdaltest
import pytest

from osgeo import gdal, osr


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


@pytest.mark.parametrize("obj_name", ["dataset", "band"])
def test_multidim_asarray_epsg_4326(obj_name):

    ds = gdal.Open("../gdrivers/data/small_world.tif")

    obj = ds if obj_name == "dataset" else ds.GetRasterBand(1)

    srs_ds = ds.GetSpatialRef()
    assert srs_ds.GetDataAxisToSRSAxisMapping() == [2, 1]

    ar = (
        obj.AsMDArray(["DIM_ORDER=BAND,Y,X"])
        if obj_name == "dataset"
        else obj.AsMDArray()
    )
    ixdim = 2 if obj_name == "dataset" else 1
    iydim = 1 if obj_name == "dataset" else 0
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == (3 if obj_name == "dataset" else 2)
    assert dims[iydim].GetSize() == ds.RasterYSize
    assert dims[ixdim].GetSize() == ds.RasterXSize
    srs_ar = ar.GetSpatialRef()
    assert (
        srs_ar.GetDataAxisToSRSAxisMapping() == [2, 3]
        if obj_name == "dataset"
        else [1, 2]
    )

    assert ar.Read() == obj.ReadRaster()

    ds2 = ar.AsClassicDataset(ixdim, iydim)
    assert ds2.RasterYSize == ds.RasterYSize
    assert ds2.RasterXSize == ds.RasterXSize
    srs_ds2 = ds2.GetSpatialRef()
    assert srs_ds2.GetDataAxisToSRSAxisMapping() == [2, 1]
    assert srs_ds2.IsSame(srs_ds)

    assert ds2.ReadRaster() == obj.ReadRaster()


@pytest.mark.parametrize("obj_name", ["dataset", "band"])
def test_multidim_asarray_epsg_26711(obj_name):

    ds = gdal.Open("data/byte.tif")

    obj = ds if obj_name == "dataset" else ds.GetRasterBand(1)

    srs_ds = ds.GetSpatialRef()
    assert srs_ds.GetDataAxisToSRSAxisMapping() == [1, 2]

    ar = obj.AsMDArray()
    ixdim = 2 if obj_name == "dataset" else 1
    iydim = 1 if obj_name == "dataset" else 0
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == (3 if obj_name == "dataset" else 2)
    assert dims[iydim].GetSize() == ds.RasterYSize
    assert dims[ixdim].GetSize() == ds.RasterXSize
    srs_ar = ar.GetSpatialRef()
    assert (
        srs_ar.GetDataAxisToSRSAxisMapping() == [3, 2]
        if obj_name == "dataset"
        else [2, 1]
    )

    assert ar.Read() == obj.ReadRaster()

    ds2 = ar.AsClassicDataset(ixdim, iydim)
    assert ds2.RasterYSize == ds.RasterYSize
    assert ds2.RasterXSize == ds.RasterXSize
    srs_ds2 = ds2.GetSpatialRef()
    assert srs_ds2.GetDataAxisToSRSAxisMapping() == [1, 2]
    assert srs_ds2.IsSame(srs_ds)

    assert ds2.ReadRaster() == obj.ReadRaster()


@gdaltest.enable_exceptions()
def test_multidim_getview_with_indexing_var():

    ds = gdal.Open("data/byte.tif")
    gt = ds.GetGeoTransform()

    ar = ds.GetRasterBand(1).AsMDArray()
    view = ar[:, :]
    assert view.AsClassicDataset(1, 0).GetGeoTransform() == gt

    ar = ds.GetRasterBand(1).AsMDArray()
    view = ar[11:20, :]
    assert view.AsClassicDataset(1, 0).GetGeoTransform() == (
        gt[0],
        gt[1],
        gt[2],
        gt[3] + 11 * gt[5],
        gt[4],
        gt[5],
    )

    out_ds = gdal.MultiDimTranslate(
        "", ds, format="MEM", arraySpecs=["name=Band1,view=[:,11:20,:]"]
    )
    assert out_ds.GetRootGroup().OpenMDArray("Band1").AsClassicDataset(
        2, 1
    ).GetGeoTransform() == (
        gt[0],
        gt[1],
        gt[2],
        gt[3] + 11 * gt[5],
        gt[4],
        gt[5],
    )

    tmp_ds = gdal.MultiDimTranslate("", ds, format="MEM")
    out_ds = gdal.MultiDimTranslate(
        "", tmp_ds, format="MEM", arraySpecs=["name=Band1,view=[0,11:20,:]"]
    )
    assert out_ds.GetRootGroup().OpenMDArray("Band1").AsClassicDataset(
        1, 0
    ).GetGeoTransform() == (
        gt[0],
        gt[1],
        gt[2],
        gt[3] + 11 * gt[5],
        gt[4],
        gt[5],
    )


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
        "ar", [dimBand, dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
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
    ar = rg.CreateMDArray("ar", [dimX], gdal.ExtendedDataType.Create(gdal.GDT_UInt8))
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
        "ar", [dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
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
        "ar", [dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
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
        "ar", [dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
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
        "ar", [dimOther, dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
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
def test_multidim_asclassicdataset_single_dim():

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()

    dim = rg.CreateDimension("dim", None, None, 2)
    ar = rg.CreateMDArray("ar", [dim], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    ar.Write(array.array("d", [10.5, 20]))

    assert ar.AsClassicDataset(0, 0).ReadRaster() == array.array("d", [10.5, 20])

    assert ar.AsClassicDataset(0, 0).AdviseRead(0, 0, 2, 1) == gdal.CE_None

    with pytest.raises(Exception, match="Invalid iXDim and/or iYDim"):
        ar.AsClassicDataset(0, 1)

    with pytest.raises(Exception, match="Invalid iXDim and/or iYDim"):
        ar.AsClassicDataset(1, 0)


@gdaltest.enable_exceptions()
def test_multidim_asclassicdataset_band_metadata():

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

    assert ar.AsClassicDataset(2, 1).AdviseRead(0, 0, 2, 2) == gdal.CE_None

    with pytest.raises(
        Exception, match="Root group should be provided when BAND_METADATA is set"
    ):
        ar.AsClassicDataset(2, 1, None, ["BAND_METADATA=[]"])

    with pytest.raises(Exception, match="Invalid JSON content for BAND_METADATA"):
        ar.AsClassicDataset(2, 1, rg, ["BAND_METADATA=invalid"])

    with pytest.raises(Exception, match="Value of BAND_METADATA should be an array"):
        ar.AsClassicDataset(2, 1, rg, ["BAND_METADATA=false"])

    band_metadata = [{"item_name": "name"}]
    with pytest.raises(
        Exception,
        match=r"BAND_METADATA\[0\]\[\"array\"\] or BAND_METADATA\[0\]\[\"attribute\"\] is missing",
    ):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = [{"array": "foo", "attribute": "bar"}]
    with pytest.raises(
        Exception,
        match=r"BAND_METADATA\[0\]\[\"array\"\] and BAND_METADATA\[0\]\[\"attribute\"\] are mutually exclusive",
    ):
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
    with pytest.raises(Exception, match="Data type of aux_var array is not numeric"):
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

    band_metadata = [{"attribute": "foo", "item_name": "name"}]
    with pytest.raises(Exception, match="Attribute foo cannot be found"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = [{"attribute": "/foo", "item_name": "name"}]
    with pytest.raises(Exception, match="Attribute /foo cannot be found"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    ar.CreateAttribute(
        "2D_attr", [2, 3], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )

    band_metadata = [{"attribute": "2D_attr", "item_name": "name"}]
    with pytest.raises(Exception, match="Attribute 2D_attr is not a 1D array"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = [{"attribute": "/ar/2D_attr", "item_name": "name"}]
    with pytest.raises(Exception, match="Attribute /ar/2D_attr is not a 1D array"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    rg.CreateAttribute(
        "2D_attr_on_rg", [2, 3], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )

    band_metadata = [{"attribute": "/2D_attr_on_rg", "item_name": "name"}]
    with pytest.raises(Exception, match="Attribute /2D_attr_on_rg is not a 1D array"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    subg = rg.CreateGroup("subgroup")
    subg.CreateAttribute(
        "2D_attr_on_subg", [2, 3], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )

    band_metadata = [{"attribute": "/subgroup/2D_attr_on_subg", "item_name": "name"}]
    with pytest.raises(
        Exception, match="Attribute /subgroup/2D_attr_on_subg is not a 1D array"
    ):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    ar.CreateAttribute(
        "non_matching_attr", [100], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )

    band_metadata = [{"attribute": "non_matching_attr", "item_name": "name"}]
    with pytest.raises(
        Exception,
        match="No dimension of ar has the same size as attribute non_matching_attr",
    ):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    dimOther2 = rg.CreateDimension("other2", None, None, 2)
    ar2 = rg.CreateMDArray(
        "ar2",
        [dimOther2, dimOther, dimY, dimX],
        gdal.ExtendedDataType.Create(gdal.GDT_Float64),
    )
    ar2.CreateAttribute(
        "attr", [dimOther.GetSize()], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    units = ar2.CreateAttribute("units", [], gdal.ExtendedDataType.CreateString())
    units.Write("my_units")
    band_metadata = [{"attribute": "attr", "item_name": "name"}]
    with pytest.raises(
        Exception,
        match="Several dimensions of ar2 have the same size as attribute attr. Cannot infer which one to bind to!",
    ):
        ds = ar2.AsClassicDataset(
            3, 2, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )

    attr = ar.CreateAttribute(
        "attr", [dimOther.GetSize()], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    attr.Write([123, 456])

    band_metadata = [
        {
            "attribute": "attr",
            "item_name": "val_attr",
            "item_value": "%.0f ${/ar2/units}",
        }
    ]
    ds = ar.AsClassicDataset(2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)])
    assert ds.GetRasterBand(1).GetMetadata() == {
        "DIM_other_INDEX": "0",
        "DIM_other_VALUE": "10.5",
        "val_attr": "123 my_units",
    }
    assert ds.GetRasterBand(2).GetMetadata() == {
        "DIM_other_INDEX": "1",
        "DIM_other_VALUE": "20",
        "val_attr": "456 my_units",
    }

    band_metadata = [
        {
            "attribute": "attr",
            "item_name": "name",
            "item_value": "%.0f ${/i/do/not/exist}",
        }
    ]
    with pytest.raises(Exception, match="/i/do/not/exist is not an attribute"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_METADATA=" + json.dumps(band_metadata)]
        )


@gdaltest.enable_exceptions()
def test_multidim_asclassicdataset_band_imagery_metadata():

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()

    dimOther = rg.CreateDimension("other", None, None, 2)

    other = rg.CreateMDArray(
        "other", [dimOther], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    other.Write(array.array("d", [10.5, 20]))

    fwhm = rg.CreateMDArray(
        "fwhm", [dimOther], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    fwhm.Write(array.array("d", [3.5, 4.5]))

    string_attr = other.CreateAttribute(
        "string_attr", [], gdal.ExtendedDataType.CreateString()
    )
    assert string_attr.Write("nm") == gdal.CE_None

    dimX = rg.CreateDimension("X", None, None, 2)
    X = rg.CreateMDArray("X", [dimX], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    X.Write(array.array("d", [10, 20]))
    dimX.SetIndexingVariable(X)
    dimY = rg.CreateDimension("Y", None, None, 2)

    ar = rg.CreateMDArray(
        "ar", [dimOther, dimY, dimX], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )

    with pytest.raises(
        Exception,
        match="Root group should be provided when BAND_IMAGERY_METADATA is set",
    ):
        ar.AsClassicDataset(2, 1, None, ["BAND_IMAGERY_METADATA={}"])

    with pytest.raises(
        Exception, match="Invalid JSON content for BAND_IMAGERY_METADATA"
    ):
        ar.AsClassicDataset(2, 1, rg, ["BAND_IMAGERY_METADATA=invalid"])

    with pytest.raises(
        Exception, match="Value of BAND_IMAGERY_METADATA should be an object"
    ):
        ar.AsClassicDataset(2, 1, rg, ["BAND_IMAGERY_METADATA=false"])

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {}}
    with pytest.raises(
        Exception,
        match=r'BAND_IMAGERY_METADATA\["CENTRAL_WAVELENGTH_UM"\]\["array"\] or BAND_IMAGERY_METADATA\["CENTRAL_WAVELENGTH_UM"\]\["attribute"\] is missing',
    ):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {"array": "foo", "attribute": "bar"}}
    with pytest.raises(
        Exception,
        match=r'BAND_IMAGERY_METADATA\["CENTRAL_WAVELENGTH_UM"\]\["array"\] and BAND_IMAGERY_METADATA\["CENTRAL_WAVELENGTH_UM"\]\["attribute"\] are mutually exclusive',
    ):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {"array": "/i/do/not/exist"}}
    with pytest.raises(Exception, match="Array /i/do/not/exist cannot be found"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {"array": "/ar"}}
    with pytest.raises(Exception, match="Array /ar is not a 1D array"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {"array": "/X"}}
    with pytest.raises(
        Exception,
        match='Dimension "X" of array "/X" is not a non-X/Y dimension of array "ar"',
    ):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {"array": "/other", "unit": "${invalid"}}
    with pytest.raises(
        Exception,
        match=r'Value of BAND_IMAGERY_METADATA\["CENTRAL_WAVELENGTH_UM"\]\["unit"\] = \${invalid is invalid',
    ):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {"array": "/other", "unit": "${invalid}"}}
    with pytest.raises(
        Exception,
        match=r'Value of BAND_IMAGERY_METADATA\["CENTRAL_WAVELENGTH_UM"\]\["unit"\] = \${invalid} is invalid: invalid is not an attribute of \/other',
    ):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {"array": "/other", "unit": "unhandled"}}
    with pytest.raises(
        Exception,
        match=r'Unhandled value for BAND_IMAGERY_METADATA\["CENTRAL_WAVELENGTH_UM"\]\["unit"\] = unhandled',
    ):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = {"ignored": {}}
    with gdal.quiet_errors():
        ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = {
        "CENTRAL_WAVELENGTH_UM": {"array": "/other", "unit": "${string_attr}"},
        "FWHM_UM": {"array": "/fwhm"},
    }
    ds = ar.AsClassicDataset(
        2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
    )
    assert ds.GetRasterBand(1).GetMetadata("IMAGERY") == {
        "CENTRAL_WAVELENGTH_UM": "0.0105",
        "FWHM_UM": "3.5",
    }
    assert ds.GetRasterBand(2).GetMetadata("IMAGERY") == {
        "CENTRAL_WAVELENGTH_UM": "0.02",
        "FWHM_UM": "4.5",
    }

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {"attribute": "i_do_not_exist"}}
    with pytest.raises(Exception, match="Attribute i_do_not_exist cannot be found"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {"attribute": "/i/do/not/exist"}}
    with pytest.raises(Exception, match="Attribute /i/do/not/exist cannot be found"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    ar.CreateAttribute(
        "2D_attr", [2, 3], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {"attribute": "2D_attr"}}
    with pytest.raises(Exception, match="Attribute 2D_attr is not a 1D array"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {"attribute": "/ar/2D_attr"}}
    with pytest.raises(Exception, match="Attribute /ar/2D_attr is not a 1D array"):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    ar.CreateAttribute(
        "non_matching_attr", [100], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {"attribute": "non_matching_attr"}}
    with pytest.raises(
        Exception,
        match="No dimension of ar has the same size as attribute non_matching_attr",
    ):
        ds = ar.AsClassicDataset(
            2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    dimOther2 = rg.CreateDimension("other2", None, None, 2)
    ar2 = rg.CreateMDArray(
        "ar2",
        [dimOther2, dimOther, dimY, dimX],
        gdal.ExtendedDataType.Create(gdal.GDT_Float64),
    )
    ar2.CreateAttribute(
        "attr", [dimOther.GetSize()], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    units = ar2.CreateAttribute("units", [], gdal.ExtendedDataType.CreateString())
    units.Write("micrometer")

    band_metadata = {"CENTRAL_WAVELENGTH_UM": {"attribute": "attr"}}
    with pytest.raises(
        Exception,
        match="Several dimensions of ar2 have the same size as attribute attr. Cannot infer which one to bind to!",
    ):
        ds = ar2.AsClassicDataset(
            3, 2, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
        )

    attr = ar.CreateAttribute(
        "attr", [dimOther.GetSize()], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    attr.Write([123, 456])
    band_metadata = {
        "CENTRAL_WAVELENGTH_UM": {"attribute": "attr", "unit": "${/ar2/units}"}
    }
    ds = ar.AsClassicDataset(
        2, 1, rg, ["BAND_IMAGERY_METADATA=" + json.dumps(band_metadata)]
    )
    assert ds.GetRasterBand(1).GetMetadata("IMAGERY") == {
        "CENTRAL_WAVELENGTH_UM": "123",
    }
    assert ds.GetRasterBand(2).GetMetadata("IMAGERY") == {
        "CENTRAL_WAVELENGTH_UM": "456",
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
        "too_large_dim_ar",
        [too_large_dim],
        gdal.ExtendedDataType.Create(gdal.GDT_UInt8),
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
        rg_subset.OpenGroup("i_do_not_exist")
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
        rg_subset.OpenMDArray("i_do_not_exist")
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
    assert rat.GetValueAsBoolean(0, icol)
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

    with pytest.raises(Exception, match="Invalid iStartRow/iLength"):
        rat.ReadValuesIOAsBoolean(icol, -1, 1)
    with pytest.raises(Exception, match="Invalid iStartRow/iLength"):
        rat.ReadValuesIOAsBoolean(icol, 0, rat.GetRowCount() + 1)
    with pytest.raises(Exception, match="invalid length"):
        rat.ReadValuesIOAsBoolean(icol, 0, -1)
    with pytest.raises(Exception, match="Invalid iField"):
        rat.ReadValuesIOAsBoolean(-1, 0, 1)
    with pytest.raises(Exception, match="Invalid iField"):
        rat.ReadValuesIOAsBoolean(rat.GetColumnCount(), 0, 1)

    ar_string.Write(["foo", "bar"])

    icol = 2
    assert rat.GetValueAsString(-1, icol) is None
    assert rat.GetValueAsString(0, icol) == "foo"
    assert rat.GetValueAsString(1, icol) == "bar"
    assert rat.GetValueAsString(2, icol) is None
    assert rat.GetValueAsDateTime(0, icol) is None
    assert rat.GetValueAsWKBGeometry(0, icol) == b""

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
    with pytest.raises(
        Exception,
        match=r"GDALRasterAttributeTableFromMDArrays::SetValue\(\): not supported",
    ):
        rat.SetValueAsBoolean(0, 0, False)
    with pytest.raises(
        Exception,
        match=r"GDALRasterAttributeTableFromMDArrays::SetValue\(\): not supported",
    ):
        rat.SetValueAsDateTime(0, 0, None)
    with pytest.raises(
        Exception,
        match=r"GDALRasterAttributeTableFromMDArrays::SetValue\(\): not supported",
    ):
        rat.SetValueAsWKBGeometry(0, 0, b"")
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


@gdaltest.enable_exceptions()
def test_multidim_GetMeshGrid():

    drv = gdal.GetDriverByName("MEM")
    mem_ds = drv.CreateMultiDimensional("myds")
    rg = mem_ds.GetRootGroup()
    dim2 = rg.CreateDimension("dim2", None, None, 2)
    dim3 = rg.CreateDimension("dim3", None, None, 3)
    ar2 = rg.CreateMDArray(
        "ar2", [dim2], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    )
    ar2_vals = [1, 2]
    ar2.SetNoDataValueDouble(0)
    ar2.SetUnit("m")
    ar2.SetOffset(1)
    ar2.SetScale(10)
    ar2.CreateAttribute("attr", [], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
    ar2.Write(ar2_vals)
    ar3 = rg.CreateMDArray(
        "ar3", [dim3], gdal.ExtendedDataType.Create(gdal.GDT_Float32)
    )
    ar3_vals = [3, 4, 5]
    ar3.Write(ar3_vals)

    with pytest.raises(Exception, match="Only 1-D input arrays are accepted"):
        gdal.MDArray.GetMeshGrid(
            [
                rg.CreateMDArray(
                    "2d_array",
                    [dim2, dim3],
                    gdal.ExtendedDataType.Create(gdal.GDT_Float64),
                )
            ]
        )

    with pytest.raises(Exception, match="Only INDEXING=xy or ij is accepted"):
        gdal.MDArray.GetMeshGrid([ar2, ar3], ["INDEXING=unsupported"])

    ret = gdal.MDArray.GetMeshGrid([])
    assert len(ret) == 0

    ret = gdal.MDArray.GetMeshGrid([ar2, ar3], ["INDEXING=IJ"])
    assert len(ret) == 2

    assert ret[0].GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert ret[0].GetDimensionCount() == 2
    assert ret[0].GetDimensions()[0].GetSize() == dim2.GetSize()
    assert ret[0].GetDimensions()[1].GetSize() == dim3.GetSize()
    assert ret[0].GetNoDataValueAsDouble() == 0
    assert ret[0].GetUnit() == "m"
    assert ret[0].GetOffset() == 1
    assert ret[0].GetScale() == 10
    assert len(ret[0].GetAttributes()) == 1
    assert ret[0].GetAttribute("attr")
    with pytest.raises(Exception, match="Cannot cache an array with an empty filename"):
        ret[0].Cache()  # Test GetFilename()
    with pytest.raises(
        Exception,
        match="Write operation not permitted on dataset opened in read-only mode",
    ):
        ret[0].AsClassicDataset(0, 1).WriteRaster(
            0, 0, 2, 3, array.array("d", [0] * 6)
        )  # Test IsWritable()

    assert ret[1].GetDataType().GetNumericDataType() == gdal.GDT_Float32
    assert ret[1].GetDimensionCount() == 2
    assert ret[1].GetDimensions()[0].GetSize() == dim2.GetSize()
    assert ret[1].GetDimensions()[1].GetSize() == dim3.GetSize()

    assert ret[0].Read() == array.array(
        "d",
        list(itertools.chain.from_iterable([[v] * dim3.GetSize() for v in ar2_vals])),
    )
    assert ret[1].Read() == array.array("f", ar3_vals * dim2.GetSize())

    # Check interoperability with numpy.meshgrid()
    try:
        import numpy as np

        from osgeo import gdal_array  # NOQA

        has_numpy = True
    except ImportError:
        has_numpy = False

    if has_numpy:
        xv, yv = np.meshgrid(ar2.ReadAsArray(), ar3.ReadAsArray(), indexing="ij")
        assert np.all(xv == ret[0].ReadAsArray())
        assert np.all(yv == ret[1].ReadAsArray())

    ret = gdal.MDArray.GetMeshGrid([ar2, ar3], ["INDEXING=XY"])
    assert len(ret) == 2
    assert ret[0].GetDataType().GetNumericDataType() == gdal.GDT_Float64
    assert ret[0].GetDimensionCount() == 2
    assert ret[0].GetDimensions()[0].GetSize() == dim3.GetSize()
    assert ret[0].GetDimensions()[1].GetSize() == dim2.GetSize()
    assert ret[1].GetDataType().GetNumericDataType() == gdal.GDT_Float32
    assert ret[1].GetDimensionCount() == 2
    assert ret[1].GetDimensions()[0].GetSize() == dim3.GetSize()
    assert ret[1].GetDimensions()[1].GetSize() == dim2.GetSize()

    assert ret[0].Read() == array.array("d", ar2_vals * dim3.GetSize())
    assert ret[1].Read() == array.array(
        "f",
        list(itertools.chain.from_iterable([[v] * dim2.GetSize() for v in ar3_vals])),
    )

    # Check interoperability with numpy.meshgrid()
    if has_numpy:
        xv, yv = np.meshgrid(ar2.ReadAsArray(), ar3.ReadAsArray(), indexing="xy")
        assert np.all(xv == ret[0].ReadAsArray())
        assert np.all(yv == ret[1].ReadAsArray())

    # Test 3D

    dim4 = rg.CreateDimension("dim4", None, None, 4)
    ar4 = rg.CreateMDArray("ar4", [dim4], gdal.ExtendedDataType.CreateString())
    ar4_vals = ["a", "bc", "def", "ghij"]
    ar4.Write(ar4_vals)

    ret = gdal.MDArray.GetMeshGrid([ar2, ar3, ar4], ["INDEXING=IJ"])
    assert len(ret) == 3
    assert ret[0].GetDimensionCount() == 3
    assert ret[0].GetDimensions()[0].GetSize() == dim2.GetSize()
    assert ret[0].GetDimensions()[1].GetSize() == dim3.GetSize()
    assert ret[0].GetDimensions()[2].GetSize() == dim4.GetSize()

    # print(ret[0].ReadAsArray())
    # print(ret[1].ReadAsArray())
    # print(ret[2].Read())

    assert ret[0].Read() == array.array(
        "d",
        list(
            itertools.chain.from_iterable(
                [[v] * (dim3.GetSize() * dim4.GetSize()) for v in ar2_vals]
            )
        ),
    )
    assert ret[1].Read() == array.array(
        "f",
        list(itertools.chain.from_iterable([[v] * dim4.GetSize() for v in ar3_vals]))
        * dim2.GetSize(),
    )
    assert ret[2].Read() == ar4_vals * (dim2.GetSize() * dim3.GetSize())

    # Check interoperability with numpy.meshgrid()
    if has_numpy:
        xv, yv, zv = np.meshgrid(
            ar2.ReadAsArray(), ar3.ReadAsArray(), ar4_vals, indexing="ij"
        )
        assert np.all(xv == ret[0].ReadAsArray())
        assert np.all(yv == ret[1].ReadAsArray())
        assert np.all(zv == np.array(ret[2].Read()).reshape(zv.shape))

    ret = gdal.MDArray.GetMeshGrid([ar2, ar3, ar4], ["INDEXING=XY"])
    assert len(ret) == 3
    assert ret[0].GetDimensionCount() == 3
    assert ret[0].GetDimensions()[0].GetSize() == dim3.GetSize()
    assert ret[0].GetDimensions()[1].GetSize() == dim2.GetSize()
    assert ret[0].GetDimensions()[2].GetSize() == dim4.GetSize()

    # print(ret[0].ReadAsArray())
    # print(ret[1].ReadAsArray())
    # print(ret[2].Read())

    assert ret[0].Read() == array.array(
        "d",
        list(itertools.chain.from_iterable([[v] * dim4.GetSize() for v in ar2_vals]))
        * dim3.GetSize(),
    )
    assert ret[1].Read() == array.array(
        "f",
        list(
            itertools.chain.from_iterable(
                [[v] * (dim2.GetSize() * dim4.GetSize()) for v in ar3_vals]
            )
        ),
    )
    assert ret[2].Read() == ar4_vals * (dim2.GetSize() * dim3.GetSize())

    # Check interoperability with numpy.meshgrid()

    if has_numpy:
        xv, yv, zv = np.meshgrid(
            ar2.ReadAsArray(), ar3.ReadAsArray(), ar4_vals, indexing="xy"
        )
        assert np.all(xv == ret[0].ReadAsArray())
        assert np.all(yv == ret[1].ReadAsArray())
        assert np.all(zv == np.array(ret[2].Read()).reshape(zv.shape))


def test_multidim_dataset_as_mdarray():

    drv = gdal.GetDriverByName("MEM")

    def get_array(options=[]):
        ds = drv.Create("my_ds", 10, 5, 2, gdal.GDT_UInt16)
        ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
        ds.SetMetadataItem("FOO", "BAR")
        sr = osr.SpatialReference()
        sr.ImportFromEPSG(32631)
        ds.SetSpatialRef(sr)

        band = ds.GetRasterBand(1)
        band.SetUnitType("foo")
        band.SetNoDataValue(2)
        band.SetOffset(1.5)
        band.SetScale(2.5)
        band.WriteRaster(0, 0, 10, 5, struct.pack("H", 1), 1, 1)
        band.WriteRaster(0, 1, 1, 1, struct.pack("H", 2))
        band.WriteRaster(1, 0, 1, 1, struct.pack("H", 3))

        band = ds.GetRasterBand(2)
        band.SetUnitType("foo")
        band.SetNoDataValue(2)
        band.SetOffset(1.5)
        band.SetScale(2.5)
        band.WriteRaster(0, 0, 10, 5, struct.pack("H", 4), 1, 1)
        band.WriteRaster(0, 1, 1, 1, struct.pack("H", 5))
        band.WriteRaster(1, 0, 1, 1, struct.pack("H", 6))

        return (ds.AsMDArray(options), ds.ReadRaster())

    ar, expected_data = get_array()
    assert ar
    assert ar.GetView("[...]")
    assert ar.GetBlockSize() == [1, 1, 10]
    assert ar.GetDimensionCount() == 3
    dims = ar.GetDimensions()
    assert dims[0].GetName() == "Band"
    assert dims[0].GetSize() == 2
    var_band = dims[0].GetIndexingVariable()
    assert var_band
    assert var_band.GetDimensions()[0].GetSize() == 2
    assert var_band.Read() == ["Band 1", "Band 2"]

    assert dims[1].GetName() == "Y"
    assert dims[1].GetSize() == 5
    var_y = dims[1].GetIndexingVariable()
    assert var_y
    assert var_y.GetDimensions()[0].GetSize() == 5
    assert struct.unpack("d" * 5, var_y.Read()) == (48.95, 48.85, 48.75, 48.65, 48.55)

    assert dims[2].GetName() == "X"
    assert dims[2].GetSize() == 10
    var_x = dims[2].GetIndexingVariable()
    assert var_x
    assert var_x.GetDimensions()[0].GetSize() == 10
    assert struct.unpack("d" * 10, var_x.Read()) == (
        2.05,
        2.15,
        2.25,
        2.35,
        2.45,
        2.55,
        2.65,
        2.75,
        2.85,
        2.95,
    )

    assert ar.GetDataType().GetClass() == gdal.GEDTC_NUMERIC
    assert ar.GetDataType().GetNumericDataType() == gdal.GDT_UInt16
    assert ar.Read() == expected_data
    assert struct.unpack(
        "H" * 8,
        ar.Read(array_start_idx=[1, 1, 0], count=[2, 2, 2], array_step=[-1, -1, 1]),
    ) == (5, 4, 4, 6, 2, 1, 1, 3)
    assert ar.Write(expected_data) == gdal.CE_None
    assert ar.Read() == expected_data
    assert ar.GetUnit() == "foo"
    assert ar.GetNoDataValueAsDouble() == 2
    assert ar.GetOffset() == 1.5
    assert ar.GetScale() == 2.5
    assert ar.GetSpatialRef() is not None
    assert len(ar.GetAttributes()) == 1
    attr = ar.GetAttribute("FOO")
    assert attr
    assert attr.Read() == "BAR"
    del ar

    ar, _ = get_array(["DIM_ORDER=Y,X,Band"])
    assert ar.GetBlockSize() == [1, 10, 1]
    del ar

    def get_array(options=[]):
        ds = drv.Create("my_ds", 10, 5, 2)
        band = ds.GetRasterBand(1)
        band.SetDescription("FirstBand")
        band.SetMetadataItem("ITEM", "5.4")
        band.SetUnitType("foo")
        band.SetNoDataValue(2)
        band.SetOffset(1.5)
        band.SetScale(2.5)
        band.SetColorInterpretation(gdal.GCI_RedBand)
        return (ds.AsMDArray(options), ds.ReadRaster())

    ar, expected_data = get_array()
    var_band = ar.GetDimensions()[0].GetIndexingVariable()
    assert var_band.Read() == ["FirstBand", "Band 2"]
    assert ar.GetSpatialRef() is None
    assert ar.GetUnit() == ""
    assert ar.GetNoDataValueAsRaw() is None
    assert ar.GetOffset() is None
    assert ar.GetScale() is None
    del ar

    ar, _ = get_array(options=["BAND_INDEXING_VAR_ITEM={None}"])
    var_band = ar.GetDimensions()[0].GetIndexingVariable()
    assert var_band is None
    del ar

    ar, _ = get_array(options=["BAND_INDEXING_VAR_ITEM={Index}"])
    var_band = ar.GetDimensions()[0].GetIndexingVariable()
    assert struct.unpack("i" * 2, var_band.Read()) == (1, 2)
    del ar

    ar, _ = get_array(options=["BAND_INDEXING_VAR_ITEM={ColorInterpretation}"])
    var_band = ar.GetDimensions()[0].GetIndexingVariable()
    assert var_band.Read() == ["Red", "Undefined"]
    del ar

    ar, _ = get_array(options=["BAND_INDEXING_VAR_ITEM=ITEM"])
    var_band = ar.GetDimensions()[0].GetIndexingVariable()
    assert var_band.Read() == ["5.4", ""]
    del ar

    ar, _ = get_array(
        options=["BAND_INDEXING_VAR_ITEM=ITEM", "BAND_INDEXING_VAR_TYPE=Integer"]
    )
    var_band = ar.GetDimensions()[0].GetIndexingVariable()
    assert struct.unpack("i" * 2, var_band.Read()) == (5, 0)
    del ar

    ar, _ = get_array(
        options=["BAND_INDEXING_VAR_ITEM=ITEM", "BAND_INDEXING_VAR_TYPE=Real"]
    )
    var_band = ar.GetDimensions()[0].GetIndexingVariable()
    assert struct.unpack("d" * 2, var_band.Read()) == (5.4, 0)
    del ar


@gdaltest.enable_exceptions()
def test_multidim_dataset_as_mdarray_errors():

    with pytest.raises(Exception, match="Degenerated array"):
        with gdal.GetDriverByName("MEM").Create("", 0, 0, 0) as ds:
            ds.AsMDArray()

    with pytest.raises(Exception, match="Non-uniform data type amongst bands"):
        with gdal.GetDriverByName("MEM").Create("", 1, 1, 0) as ds:
            ds.AddBand(gdal.GDT_UInt8)
            ds.AddBand(gdal.GDT_UInt16)
            ds.AsMDArray()

    with pytest.raises(Exception, match="Illegal value for DIM_ORDER option"):
        with gdal.GetDriverByName("MEM").Create("", 1, 1, 1) as ds:
            ds.AsMDArray(["DIM_ORDER=invalid"])
