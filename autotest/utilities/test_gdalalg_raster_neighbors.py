#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster neighbors' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array

import pytest

from osgeo import gdal


@pytest.fixture()
def neighbors():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("neighbors")


@pytest.mark.parametrize(
    "kernel,checksum", [("sharpen", 4252), ("edge1", 2278), ("edge2", 2311)]
)
def test_gdalalg_raster_neighbors_kernel_sharpen(neighbors, kernel, checksum):

    neighbors["input"] = "../gcore/data/byte.tif"
    neighbors["kernel"] = kernel
    neighbors["output-format"] = "MEM"
    neighbors["datatype"] = "Byte"
    assert neighbors.Run()

    out_ds = neighbors["output"].GetDataset()
    out_ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
    assert out_ds.GetRasterBand(1).Checksum() == checksum


def test_gdalalg_raster_neighbors_kernel_manual(neighbors):

    neighbors["input"] = "../gcore/data/byte.tif"
    neighbors["kernel"] = [[0, -1, 0], [-1, 5, -1], [0, -1, 0]]
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors["output"].GetDataset()
    out_ds.GetRasterBand(1).DataType == gdal.GDT_Float64
    assert out_ds.GetRasterBand(1).Checksum() == 4013


def test_gdalalg_raster_neighbors_kernel_manual2(neighbors):

    neighbors["input"] = "../gcore/data/byte.tif"
    neighbors["kernel"] = [
        [[0, -1, 0], [-1, 5, -1], [0, -1, 0]],
        [[0, 0, 0], [0, 1, 0], [0, 0, 0]],
    ]
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors["output"].GetDataset()
    assert out_ds.RasterCount == 2
    out_ds.GetRasterBand(1).DataType == gdal.GDT_Float64
    assert out_ds.GetRasterBand(1).Checksum() == 4013
    assert out_ds.GetRasterBand(2).Checksum() == 4672


def test_gdalalg_raster_neighbors_multible_band(neighbors):

    neighbors["input"] = "../gdrivers/data/small_world.tif"
    neighbors["kernel"] = "sharpen"
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors["output"].GetDataset()
    assert out_ds.RasterCount == 3
    assert out_ds.GetRasterBand(1).Checksum() == 31543
    assert out_ds.GetRasterBand(2).Checksum() == 35728
    assert out_ds.GetRasterBand(3).Checksum() == 32040


def test_gdalalg_raster_neighbors_mean(neighbors):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.WriteRaster(0, 0, 3, 3, array.array("B", [1, 2, 3, 4, 5, 6, 7, 8, 9]))

    neighbors["input"] = src_ds
    neighbors["kernel"] = "equal"
    neighbors["method"] = "mean"
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    out_ds = neighbors.Output()
    ar = array.array("d")
    ar.frombytes(out_ds.ReadRaster(1, 1, 1, 1))
    assert ar[0] == 5.0


def test_gdalalg_raster_neighbors_sum(neighbors):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.WriteRaster(0, 0, 3, 3, array.array("B", [1, 2, 3, 4, 5, 6, 7, 8, 9]))

    neighbors["input"] = src_ds
    neighbors["kernel"] = "equal"
    neighbors["method"] = "sum"
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    ar = array.array("d")
    ar.frombytes(out_ds.ReadRaster(1, 1, 1, 1))
    assert ar[0] == 45.0


def test_gdalalg_raster_neighbors_min(neighbors):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.WriteRaster(0, 0, 3, 3, array.array("B", [1, 2, 3, 4, 5, 6, 7, 8, 9]))

    neighbors["input"] = src_ds
    neighbors["kernel"] = "equal"
    neighbors["method"] = "min"
    neighbors["output-format"] = "MEM"
    neighbors["datatype"] = "Byte"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    ar = array.array("B")
    ar.frombytes(out_ds.ReadRaster(1, 1, 1, 1))
    assert ar[0] == 1


def test_gdalalg_raster_neighbors_max(neighbors):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.WriteRaster(0, 0, 3, 3, array.array("B", [1, 2, 3, 4, 5, 6, 7, 8, 9]))

    neighbors["input"] = src_ds
    neighbors["kernel"] = "equal"
    neighbors["method"] = "max"
    neighbors["output-format"] = "MEM"
    neighbors["datatype"] = "Byte"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    ar = array.array("B")
    ar.frombytes(out_ds.ReadRaster(1, 1, 1, 1))
    assert ar[0] == 9


def test_gdalalg_raster_neighbors_median_odd_number(neighbors):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.WriteRaster(0, 0, 3, 3, array.array("B", [1, 2, 3, 4, 5, 60, 70, 80, 90]))

    neighbors["input"] = src_ds
    neighbors["kernel"] = "equal"
    neighbors["method"] = "median"
    neighbors["output-format"] = "MEM"
    neighbors["datatype"] = "Byte"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    ar = array.array("B")
    ar.frombytes(out_ds.ReadRaster(1, 1, 1, 1))
    assert ar[0] == 5


def test_gdalalg_raster_neighbors_median_even_number(neighbors):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.WriteRaster(0, 0, 3, 3, array.array("B", [1, 2, 3, 4, 5, 60, 70, 80, 90]))
    src_ds.GetRasterBand(1).SetNoDataValue(90)

    neighbors["input"] = src_ds
    neighbors["kernel"] = "equal"
    neighbors["method"] = "median"
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    ar = array.array("d")
    ar.frombytes(out_ds.ReadRaster(1, 1, 1, 1))
    assert ar[0] == 4.5


def test_gdalalg_raster_neighbors_mode(neighbors):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.WriteRaster(0, 0, 3, 3, array.array("B", [1, 2, 3, 4, 5, 6, 3, 8, 9]))

    neighbors["input"] = src_ds
    neighbors["kernel"] = "equal"
    neighbors["method"] = "mode"
    neighbors["output-format"] = "MEM"
    neighbors["datatype"] = "Byte"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    ar = array.array("b")
    ar.frombytes(out_ds.ReadRaster(1, 1, 1, 1))
    assert ar[0] == 3


def test_gdalalg_raster_neighbors_stddev(neighbors):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.WriteRaster(0, 0, 3, 3, array.array("B", [1, 2, 3, 4, 5, 6, 7, 8, 9]))

    neighbors["input"] = src_ds
    neighbors["kernel"] = "equal"
    neighbors["method"] = "stddev"
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    ar = array.array("d")
    ar.frombytes(out_ds.ReadRaster(1, 1, 1, 1))
    assert ar[0] == pytest.approx(2.58198881149292)


def test_gdalalg_raster_neighbors_u(neighbors):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.WriteRaster(0, 0, 3, 3, array.array("B", [1, 2, 3, 4, 5, 6, 7, 8, 9]))

    neighbors["input"] = src_ds
    neighbors["kernel"] = "u"
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    ar = array.array("d")
    ar.frombytes(out_ds.ReadRaster(1, 1, 1, 1))
    assert ar[0] == 1.0


def test_gdalalg_raster_neighbors_v(neighbors):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.WriteRaster(0, 0, 3, 3, array.array("B", [1, 2, 3, 4, 5, 6, 7, 8, 9]))

    neighbors["input"] = src_ds
    neighbors["kernel"] = "v"
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    ar = array.array("d")
    ar.frombytes(out_ds.ReadRaster(1, 1, 1, 1))
    assert ar[0] == 3.0


def test_gdalalg_raster_neighbors_gaussian_3x3(neighbors):

    neighbors["input"] = "../gcore/data/byte.tif"
    neighbors["kernel"] = "gaussian"
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    ar = array.array("d")
    ar.frombytes(out_ds.ReadRaster(10, 10, 1, 1))
    assert ar[0] == 114.0625


def test_gdalalg_raster_neighbors_gaussian_5x5(neighbors):

    neighbors["input"] = "../gcore/data/byte.tif"
    neighbors["kernel"] = "gaussian"
    neighbors["size"] = 5
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    ar = array.array("d")
    ar.frombytes(out_ds.ReadRaster(10, 10, 1, 1))
    assert ar[0] == pytest.approx(119.00390625)


def test_gdalalg_raster_neighbors_unsharp_masking(neighbors):

    neighbors["input"] = "../gcore/data/byte.tif"
    neighbors["kernel"] = "unsharp-masking"
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    ar = array.array("d")
    ar.frombytes(out_ds.ReadRaster(10, 10, 1, 1))
    assert ar[0] == pytest.approx(110.99609375)


def test_gdalalg_raster_neighbors_src_nodata(neighbors):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.WriteRaster(0, 0, 3, 3, array.array("B", [1, 2, 3, 4, 5, 6, 7, 8, 9]))
    src_ds.GetRasterBand(1).SetNoDataValue(9)

    neighbors["input"] = src_ds
    neighbors["kernel"] = "equal"
    neighbors["method"] = "max"
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    assert out_ds.GetRasterBand(1).GetNoDataValue() == 9.0
    ar = array.array("d")
    ar.frombytes(out_ds.ReadRaster(1, 1, 1, 1))
    assert ar[0] == 8.0


def test_gdalalg_raster_neighbors_src_nodata_and_dst_nodata(neighbors):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.WriteRaster(0, 0, 3, 3, array.array("B", [1, 2, 3, 4, 5, 6, 7, 8, 9]))
    src_ds.GetRasterBand(1).SetNoDataValue(9)

    neighbors["input"] = src_ds
    neighbors["kernel"] = "equal"
    neighbors["method"] = "max"
    neighbors["output-format"] = "MEM"
    neighbors["nodata"] = -1
    assert neighbors.Run()

    out_ds = neighbors.Output()
    assert out_ds.GetRasterBand(1).GetNoDataValue() == -1.0
    ar = array.array("d")
    ar.frombytes(out_ds.ReadRaster(1, 1, 1, 1))
    assert ar[0] == 8.0


def test_gdalalg_raster_neighbors_src_nodata_and_dst_nodata_none(neighbors):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.WriteRaster(0, 0, 3, 3, array.array("B", [1, 2, 3, 4, 5, 6, 7, 8, 9]))
    src_ds.GetRasterBand(1).SetNoDataValue(9)

    neighbors["input"] = src_ds
    neighbors["kernel"] = "equal"
    neighbors["method"] = "max"
    neighbors["output-format"] = "MEM"
    neighbors["nodata"] = "none"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    assert out_ds.GetRasterBand(1).GetNoDataValue() is None
    ar = array.array("d")
    ar.frombytes(out_ds.ReadRaster(1, 1, 1, 1))
    assert ar[0] == 8.0


def test_gdalalg_raster_neighbors_several_kernels(neighbors):

    neighbors["input"] = "../gcore/data/byte.tif"
    neighbors["kernel"] = ["u", "v"]
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    assert out_ds.RasterCount == 2


def test_gdalalg_raster_neighbors_dst_nodata_incompatible_of_type(neighbors):

    neighbors["input"] = "../gcore/data/byte.tif"
    neighbors["kernel"] = "equal"
    neighbors["method"] = "max"
    neighbors["output-format"] = "MEM"
    neighbors["nodata"] = "-1"
    neighbors["datatype"] = "Byte"
    with pytest.raises(
        Exception, match="Band output type Byte cannot represent NoData value -1"
    ):
        neighbors.Run()


def test_gdalalg_raster_neighbors_error_number_of_kernel_not_matching_method(
    neighbors,
):

    neighbors["input"] = "../gdrivers/data/small_world.tif"
    neighbors["kernel"] = "sharpen"
    neighbors["method"] = ["sum", "sum"]
    neighbors["output-format"] = "MEM"
    with pytest.raises(
        Exception,
        match="The number of values for the 'method' argument should be one or exactly the number of values of 'kernel'",
    ):
        neighbors.Run()


def test_gdalalg_raster_neighbors_error_not_even(neighbors):

    with pytest.raises(
        Exception,
        match="The number of values in the 'kernel' argument must be an odd square number",
    ):
        neighbors["kernel"] = "[[0,0],[0,0]]"


def test_gdalalg_raster_neighbors_error_not_square(neighbors):

    with pytest.raises(
        Exception,
        match="The number of values in the 'kernel' argument must be an odd square number",
    ):
        neighbors["kernel"] = "[[0,0,0],[0,0,0]]"


def test_gdalalg_raster_neighbors_error_kernel_not_numeric(neighbors):

    with pytest.raises(
        Exception,
        match="Non-numeric value found in the 'kernel' argument",
    ):
        neighbors["kernel"] = "[[1,1,1],[1,foo,1],[1,1,1]]"


def test_gdalalg_raster_neighbors_error_size_not_odd(neighbors):

    with pytest.raises(
        Exception,
        match="The value of 'size' must be an odd number",
    ):
        neighbors["size"] = 4


def test_gdalalg_raster_neighbors_error_size_inconsistent(neighbors):

    neighbors["kernel"] = "[[1,1,1],[1,1,1],[1,1,1]]"
    neighbors["size"] = 5

    with pytest.raises(
        Exception,
        match=r"Value of 'size' argument \(5\) inconsistent with the one deduced from the kernel matrix \(3\)",
    ):
        neighbors.Run()


def test_gdalalg_raster_neighbors_error_method(neighbors):

    with pytest.raises(
        Exception,
        match="Invalid value 'invalid' for string argument 'method'",
    ):
        neighbors["method"] = "invalid"


def test_gdalalg_raster_neighbors_error_kernel_name(neighbors):

    with pytest.raises(
        Exception,
        match="Valid values for 'kernel' argument are:",
    ):
        neighbors["kernel"] = "invalid"


def test_gdalalg_raster_neighbors_error_u(neighbors):

    neighbors["kernel"] = "u"
    neighbors["size"] = 7
    with pytest.raises(
        Exception,
        match="Currently only size = 3 is supported for kernel 'u'",
    ):
        neighbors.Run()


def test_gdalalg_raster_neighbors_error_gaussian(neighbors):

    neighbors["kernel"] = "gaussian"
    neighbors["size"] = 7
    with pytest.raises(
        Exception,
        match="Currently only size = 3 or 5 is supported for kernel 'gaussian'",
    ):
        neighbors.Run()


def test_gdalalg_raster_neighbors_error_unsharp_masking(neighbors):

    neighbors["kernel"] = "unsharp-masking"
    neighbors["size"] = 7
    with pytest.raises(
        Exception,
        match="Currently only size = 5 is supported for kernel 'unsharp-masking'",
    ):
        neighbors.Run()


def test_gdalalg_raster_neighbors_complete():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster neighbors --kernel"
    ).split(" ")
    assert "sharpen" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster neighbors --kernel ["
    ).split(" ")
    assert "sharpen" not in out


def test_gdalalg_raster_neighbors_custom_kernel_0_sum(neighbors):

    neighbors["input"] = "../gcore/data/byte.tif"
    neighbors["kernel"] = [[0, -0.04, 0], [-0.04, 0.16, -0.04], [0, -0.04, 0]]
    neighbors["output-format"] = "MEM"
    assert neighbors.Run()

    out_ds = neighbors.Output()
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == pytest.approx(
        (-7.56, 12.24)
    )


def test_gdalalg_raster_neighbors_custom_kernel_0_sum_error(neighbors):

    neighbors["input"] = "../gcore/data/byte.tif"
    neighbors["kernel"] = [[0, -0.04, 0], [-0.04, 0.16, -0.04], [0, -0.04, 0]]
    neighbors["method"] = "mean"
    neighbors["output-format"] = "MEM"
    with pytest.raises(
        Exception,
        match="Specifying method = 'mean' for a kernel whose sum of coefficients is zero is not allowed. Use 'sum' instead",
    ):
        neighbors.Run()
