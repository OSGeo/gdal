#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal mdim mosaic' testing
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

pytestmark = pytest.mark.require_driver("netCDF")


def test_gdalalg_mdim_mosaic_labelled_axis_single_value_1D_array_and_glob(tmp_path):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test1.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 1)
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.CreateAttribute(
                "axis", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("Z")
            z_ar.CreateAttribute(
                "positive", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("up")
            z_ar.Write([10])
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [3]))

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test2.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 1)
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.CreateAttribute(
                "axis", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("Z")
            z_ar.CreateAttribute(
                "positive", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("up")
            z_ar.Write([20])
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [4]))

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test3.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 1)
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.CreateAttribute(
                "axis", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("Z")
            z_ar.CreateAttribute(
                "positive", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("up")
            z_ar.Write([0])
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [2]))

    create_sources()

    with gdal.Run(
        "mdim",
        "mosaic",
        input=tmp_path / "test*.nc",
        output=tmp_path / "out.vrt",
        array="test",
        output_format="VRT",
    ) as alg:
        ds = alg.Output()
        ar = ds.GetRootGroup().OpenMDArray("test")
        dims = ar.GetDimensions()
        assert dims[0].GetName() == "z"
        assert dims[0].GetType() == "VERTICAL"
        assert dims[0].GetDirection() == "UP"
        assert dims[0].GetSize() == 3
        assert array.array("d", dims[0].GetIndexingVariable().Read()) == array.array(
            "d", [0, 10, 20]
        )
        assert array.array("B", ar.Read()) == array.array("B", [2, 3, 4])


def test_gdalalg_mdim_mosaic_labelled_axis_multiple_value_1D_array_and_input_file_list(
    tmp_path,
):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test1.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 2)
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.CreateAttribute(
                "axis", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("Z")
            z_ar.CreateAttribute(
                "positive", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("up")
            z_ar.Write([10, 30])
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [3, 4]))

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test2.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 3)
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.CreateAttribute(
                "axis", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("Z")
            z_ar.CreateAttribute(
                "positive", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("up")
            z_ar.Write([100, 200, 250])
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [5, 6, 7]))

    create_sources()

    with gdal.VSIFile(tmp_path / "list.txt", "wb") as f:
        f.write(f"{tmp_path}/test1.nc\n".encode("UTF-8"))
        f.write(f"{tmp_path}/test2.nc\n".encode("UTF-8"))

    with gdal.Run(
        "mdim",
        "mosaic",
        input=f"@{tmp_path}/list.txt",
        output=tmp_path / "out.vrt",
        array="test",
        output_format="VRT",
    ) as alg:
        ds = alg.Output()
        ar = ds.GetRootGroup().OpenMDArray("test")
        dims = ar.GetDimensions()
        assert dims[0].GetName() == "z"
        assert dims[0].GetType() == "VERTICAL"
        assert dims[0].GetDirection() == "UP"
        assert dims[0].GetSize() == 5
        assert array.array("d", dims[0].GetIndexingVariable().Read()) == array.array(
            "d", [10, 30, 100, 200, 250]
        )
        assert array.array("B", ar.Read()) == array.array("B", [3, 4, 5, 6, 7])


def test_gdalalg_mdim_mosaic_regularly_spaced_axis_1D_array(tmp_path):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test1.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 3)
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.CreateAttribute(
                "axis", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("Z")
            z_ar.CreateAttribute(
                "positive", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("up")
            z_ar.Write([10, 20, 30])
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [3, 4, 5]))

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test2.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 3)
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.CreateAttribute(
                "axis", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("Z")
            z_ar.CreateAttribute(
                "positive", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("up")
            z_ar.Write([50, 60, 70])
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [6, 7, 8]))

    create_sources()

    with gdal.Run(
        "mdim",
        "mosaic",
        input=[tmp_path / "test1.nc", tmp_path / "test2.nc"],
        output=tmp_path / "out.vrt",
        array="test",
        output_format="VRT",
    ) as alg:
        ds = alg.Output()
        ar = ds.GetRootGroup().OpenMDArray("test")
        dims = ar.GetDimensions()
        assert dims[0].GetName() == "z"
        assert dims[0].GetType() == "VERTICAL"
        assert dims[0].GetDirection() == "UP"
        assert dims[0].GetSize() == 7
        assert array.array("d", dims[0].GetIndexingVariable().Read()) == array.array(
            "d", [10, 20, 30, 40, 50, 60, 70]
        )
        assert array.array("B", ar.Read()) == array.array("B", [3, 4, 5, 0, 6, 7, 8])


def test_gdalalg_mdim_mosaic_labelled_axis_2D_array(tmp_path):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test1.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 1)
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.CreateAttribute(
                "axis", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("Z")
            z_ar.CreateAttribute(
                "positive", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("up")
            z_ar.Write([20])
            dim1 = rg.CreateDimension("dim1", None, None, 3)
            dim1_ar = rg.CreateMDArray(
                "dim1", [dim1], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            dim1_ar.Write(array.array("d", [100, 200, 300]))
            ar = rg.CreateMDArray(
                "test", [z, dim1], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [4, 5, 6]))

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test2.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 1)
            z_ar.CreateAttribute(
                "axis", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("Z")
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.CreateAttribute(
                "positive", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("up")
            z_ar.Write([10])
            dim1 = rg.CreateDimension("dim1", None, None, 3)
            dim1_ar = rg.CreateMDArray(
                "dim1", [dim1], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            dim1_ar.Write(array.array("d", [100, 200, 300]))
            ar = rg.CreateMDArray(
                "test", [z, dim1], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [1, 2, 3]))

    create_sources()

    gdal.Run(
        "mdim",
        "mosaic",
        input=[tmp_path / "test1.nc", tmp_path / "test2.nc"],
        output=tmp_path / "out.nc",
    )
    with gdal.OpenEx(tmp_path / "out.nc", gdal.OF_MULTIDIM_RASTER) as ds:
        ar = ds.GetRootGroup().OpenMDArray("test")
        dims = ar.GetDimensions()
        assert dims[0].GetName() == "z"
        assert dims[0].GetType() == "VERTICAL"
        assert dims[0].GetDirection() == "UP"
        assert dims[0].GetSize() == 2
        assert array.array("d", dims[0].GetIndexingVariable().Read()) == array.array(
            "d", [10, 20]
        )
        assert dims[1].GetName() == "dim1"
        assert dims[1].GetSize() == 3
        assert array.array("d", dims[1].GetIndexingVariable().Read()) == array.array(
            "d", [100, 200, 300]
        )
        assert array.array("B", ar.Read()) == array.array("B", [1, 2, 3, 4, 5, 6])


@pytest.mark.parametrize(
    "values1,values2,values3,error_msg",
    [
        (
            [10, 20, 30],
            [70, 60, 50],
            None,
            "is indexed by a variable with spacing -10, whereas it is 10 in other datasets",
        ),
        (
            [10, 20, 30],
            [25, 35, 45],
            None,
            "is indexed by a variable whose start value is not aligned with the one of other datasets",
        ),
        (
            [10, 20, 30],
            [35],
            None,
            "has irregularly-spaced values, contrary to other datasets",
        ),
        (
            [10],
            [30, 40, 50],
            None,
            "has regularly spaced labels, contrary to other datasets",
        ),
        (
            [10, 11, 20],
            [30, 29, 25],
            None,
            "must be either increasing or decreasing in all input datasets",
        ),
        (
            [10, 20, 40],
            [10, 20, 50],
            None,
            "values in indexing variable z of dimension z are not the same as in other datasets",
        ),
        (
            [10, 20, 40],
            [5, 20, 50],
            None,
            "values in indexing variable z of dimension z are not the same as in other datasets",
        ),
        (
            [10, 20, 40],
            [40, 50, 70],
            None,
            "values in indexing variable z of dimension z are overlapping with the ones of other datasets",
        ),
        (
            [40, 50, 70],
            [100, 110, 130],
            [90, 105, 106],
            "values in indexing variable z of dimension z are overlapping with the ones of other datasets",
        ),
    ],
)
def test_gdalalg_mdim_mosaic_errors(tmp_path, values1, values2, values3, error_msg):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test1.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", len(values1))
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.Write(values1)
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [1] * len(values1)))

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test2.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", len(values2))
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.Write(values2)
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [2] * len(values2)))

        inputs = [tmp_path / "test1.nc", tmp_path / "test2.nc"]
        if values3:
            inputs.append(tmp_path / "test3.nc")
            with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
                tmp_path / "test3.nc"
            ) as ds:
                rg = ds.GetRootGroup()
                z = rg.CreateDimension("z", "VERTICAL", "UP", len(values3))
                z_ar = rg.CreateMDArray(
                    "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
                )
                z_ar.Write(values3)
                ar = rg.CreateMDArray(
                    "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
                )
                ar.Write(array.array("B", [3] * len(values3)))

        return inputs

    with pytest.raises(Exception, match=error_msg):
        gdal.Run(
            "mdim",
            "mosaic",
            input=create_sources(),
            output="",
            array="test",
            output_format="VRT",
        )


def test_gdalalg_mdim_mosaic_error_dim_not_same_name(tmp_path):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        values1 = [1]
        values2 = [2]

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test1.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", len(values1))
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.Write(values1)
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [1] * len(values1)))

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test2.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            y = rg.CreateDimension("y", "VERTICAL", "UP", len(values2))
            y_ar = rg.CreateMDArray(
                "y", [y], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            y_ar.Write(values2)
            ar = rg.CreateMDArray(
                "test", [y], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [2] * len(values2)))

    create_sources()

    with pytest.raises(
        Exception, match="does not have the same name as in other datasets"
    ):
        gdal.Run(
            "mdim",
            "mosaic",
            input=[tmp_path / "test1.nc", tmp_path / "test2.nc"],
            output="",
            array="test",
            output_format="VRT",
        )


def test_gdalalg_mdim_mosaic_error_array_not_same_type(tmp_path):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        values1 = [1]
        values2 = [2]

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test1.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", len(values1))
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.Write(values1)
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [1] * len(values1)))

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test2.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", len(values2))
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.Write(values2)
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_Int32)
            )
            ar.Write(array.array("I", [2] * len(values2)))

    create_sources()

    with pytest.raises(
        Exception, match="does not have the same data type as in other datasets"
    ):
        gdal.Run(
            "mdim",
            "mosaic",
            input=[tmp_path / "test1.nc", tmp_path / "test2.nc"],
            output="",
            array="test",
            output_format="VRT",
        )


@pytest.mark.parametrize("nd1,nd2", [(1, 2), (None, 2), (1, None)])
def test_gdalalg_mdim_mosaic_error_array_not_same_nodata_value(tmp_path, nd1, nd2):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        values1 = [1]
        values2 = [2]

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test1.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", len(values1))
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.Write(values1)
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            if nd1:
                ar.SetNoDataValue(nd1)
            ar.Write(array.array("B", [1] * len(values1)))

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test2.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", len(values2))
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.Write(values2)
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            if nd2:
                ar.SetNoDataValue(nd2)
            ar.Write(array.array("B", [2] * len(values2)))

    create_sources()

    with pytest.raises(
        Exception, match="does not have the same nodata value as in other datasets"
    ):
        gdal.Run(
            "mdim",
            "mosaic",
            input=[tmp_path / "test1.nc", tmp_path / "test2.nc"],
            output="",
            array="test",
            output_format="VRT",
        )


def test_gdalalg_mdim_mosaic_error_non_existing_arrays(tmp_path):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        values1 = [1]

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test1.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", len(values1))
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.Write(values1)
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [1] * len(values1)))

    create_sources()

    with pytest.raises(
        Exception, match="No array of dimension count >= 2 found in dataset"
    ):
        gdal.Run(
            "mdim",
            "mosaic",
            input=[tmp_path / "test1.nc"],
            output="",
            output_format="VRT",
        )

    with pytest.raises(Exception, match="Cannot find array"):
        gdal.Run(
            "mdim",
            "mosaic",
            input=[tmp_path / "test1.nc"],
            output="",
            array="non_existing",
            output_format="VRT",
        )


def test_gdalalg_mdim_mosaic_error_zero_dim(tmp_path):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test1.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            ar = rg.CreateMDArray(
                "test", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(b"\x00")

    create_sources()

    with pytest.raises(Exception, match="has no dimension"):
        gdal.Run(
            "mdim",
            "mosaic",
            input=[tmp_path / "test1.nc"],
            output="",
            array="test",
            output_format="VRT",
        )


def test_gdalalg_mdim_mosaic_error_non_numeric_indexing_var(tmp_path):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        values1 = ["foo"]

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test1.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", len(values1))
            z_ar = rg.CreateMDArray("z", [z], gdal.ExtendedDataType.CreateString())
            z_ar.Write(values1)
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [1] * len(values1)))

    create_sources()

    with pytest.raises(
        Exception,
        match="indexing variable z of dimension z has a non-numeric data type",
    ):
        gdal.Run(
            "mdim",
            "mosaic",
            input=[tmp_path / "test1.nc"],
            output="",
            array="test",
            output_format="VRT",
        )


@pytest.mark.require_driver("Zarr")
def test_gdalalg_mdim_mosaic_error_no_indexing_var(tmp_path):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        with gdal.GetDriverByName("Zarr").CreateMultiDimensional(
            tmp_path / "test1.zarr"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 1)
            ar = rg.CreateMDArray(
                "test", [z], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [1]))

    create_sources()

    with pytest.raises(
        Exception,
        match="dimension z lacks an indexing variable",
    ):
        gdal.Run(
            "mdim",
            "mosaic",
            input=[tmp_path / "test1.zarr"],
            output="",
            array="test",
            output_format="VRT",
        )


def test_gdalalg_mdim_mosaic_multiple_arrays(tmp_path):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test1.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 1)
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.CreateAttribute(
                "axis", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("Z")
            z_ar.CreateAttribute(
                "positive", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("up")
            z_ar.Write([10])

            dim1 = rg.CreateDimension("dim1", None, None, 1)
            dim1_ar = rg.CreateMDArray(
                "dim1", [dim1], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            dim1_ar.Write(array.array("d", [100]))

            ar = rg.CreateMDArray(
                "test", [z, dim1], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [3]))

            ar2 = rg.CreateMDArray(
                "test2", [z, dim1], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar2.Write(array.array("B", [30]))

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test2.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 1)
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.CreateAttribute(
                "axis", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("Z")
            z_ar.CreateAttribute(
                "positive", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("up")
            z_ar.Write([20])

            dim1 = rg.CreateDimension("dim1", None, None, 1)
            dim1_ar = rg.CreateMDArray(
                "dim1", [dim1], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            dim1_ar.Write(array.array("d", [100]))

            ar = rg.CreateMDArray(
                "test", [z, dim1], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar.Write(array.array("B", [4]))

            ar2 = rg.CreateMDArray(
                "test2", [z, dim1], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
            )
            ar2.Write(array.array("B", [40]))

    create_sources()

    with gdal.Run(
        "mdim",
        "mosaic",
        input=tmp_path / "test*.nc",
        output=tmp_path / "out.vrt",
        output_format="VRT",
    ) as alg:
        ds = alg.Output()
        assert set(ds.GetRootGroup().GetMDArrayNames()) == set(
            ["dim1", "test", "test2", "z"]
        )
        ar = ds.GetRootGroup().OpenMDArray("test")
        dims = ar.GetDimensions()
        assert dims[0].GetName() == "z"
        assert dims[0].GetType() == "VERTICAL"
        assert dims[0].GetDirection() == "UP"
        assert dims[0].GetSize() == 2
        assert array.array("d", dims[0].GetIndexingVariable().Read()) == array.array(
            "d", [10, 20]
        )
        assert array.array("B", ar.Read()) == array.array("B", [3, 4])

        ar2 = ds.GetRootGroup().OpenMDArray("test2")
        dims = ar2.GetDimensions()
        assert dims[0].GetName() == "z"
        assert dims[0].GetType() == "VERTICAL"
        assert dims[0].GetDirection() == "UP"
        assert dims[0].GetSize() == 2
        assert array.array("d", dims[0].GetIndexingVariable().Read()) == array.array(
            "d", [10, 20]
        )
        assert array.array("B", ar2.Read()) == array.array("B", [30, 40])

    with gdal.Run(
        "mdim",
        "mosaic",
        input=tmp_path / "test*.nc",
        output=tmp_path / "out.vrt",
        output_format="VRT",
        array=["test", "test2"],
        overwrite=True,
    ) as alg:
        ds = alg.Output()
        assert set(ds.GetRootGroup().GetMDArrayNames()) == set(
            ["dim1", "test", "test2", "z"]
        )

    with pytest.raises(Exception, match="You may specify the --overwrite option"):
        gdal.Run(
            "mdim",
            "mosaic",
            input=tmp_path / "test*.nc",
            output=tmp_path / "out.vrt",
            output_format="VRT",
            array=["test", "non_existing"],
        )

    with pytest.raises(Exception, match="Cannot find array /non_existing"):
        gdal.Run(
            "mdim",
            "mosaic",
            input=tmp_path / "test*.nc",
            output=tmp_path / "out.vrt",
            output_format="VRT",
            overwrite=True,
            array=["test", "non_existing"],
        )


def test_gdalalg_mdim_mosaic_copy_blocksize(tmp_path):

    with gdal.Run(
        "mdim",
        "mosaic",
        input="../gdrivers/data/netcdf/byte_chunked_not_multiple.nc",
        output=tmp_path / "out.nc",
    ) as alg:
        ds = alg.Output()
        assert ds.GetRootGroup().OpenMDArray("Band1").GetBlockSize() == [6, 15]


def test_gdalalg_mdim_mosaic_copy_blocksize_not_same(tmp_path):
    # Use a inner function to make sure all native objects have their
    # reference count down to 0, so the data is actually serialized to disk
    # Ideally we should have a better solution...
    def create_sources():

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test1.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 1)
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.CreateAttribute(
                "axis", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("Z")
            z_ar.CreateAttribute(
                "positive", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("up")
            z_ar.Write([10])

            dim1 = rg.CreateDimension("dim1", None, None, 3)
            dim1_ar = rg.CreateMDArray(
                "dim1", [dim1], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            dim1_ar.Write(array.array("d", [100, 200, 300]))

            rg.CreateMDArray(
                "test",
                [z, dim1],
                gdal.ExtendedDataType.Create(gdal.GDT_UInt8),
                ["BLOCKSIZE=1,2"],
            )

        with gdal.GetDriverByName("netCDF").CreateMultiDimensional(
            tmp_path / "test2.nc"
        ) as ds:
            rg = ds.GetRootGroup()
            z = rg.CreateDimension("z", "VERTICAL", "UP", 1)
            z_ar = rg.CreateMDArray(
                "z", [z], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            z_ar.CreateAttribute(
                "axis", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("Z")
            z_ar.CreateAttribute(
                "positive", [1], gdal.ExtendedDataType.CreateString()
            ).WriteString("up")
            z_ar.Write([20])

            dim1 = rg.CreateDimension("dim1", None, None, 3)
            dim1_ar = rg.CreateMDArray(
                "dim1", [dim1], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
            )
            dim1_ar.Write(array.array("d", [100, 200, 300]))

            rg.CreateMDArray(
                "test",
                [z, dim1],
                gdal.ExtendedDataType.Create(gdal.GDT_UInt8),
                ["BLOCKSIZE=1,1"],
            )

    create_sources()

    gdal.Run("mdim", "mosaic", input=tmp_path / "test*.nc", output=tmp_path / "out.nc")

    with gdal.OpenEx(tmp_path / "out.nc", gdal.OF_MULTIDIM_RASTER) as ds:
        assert ds.GetRootGroup().OpenMDArray("test").GetBlockSize() == [0, 0]


def test_gdalalg_mdim_mosaic_two_sources(tmp_path):

    gdal.Run(
        "raster clip",
        input="../gcore/data/byte.tif",
        output=tmp_path / "out_top.nc",
        window=[0, 0, 20, 10],
    )
    gdal.Run(
        "raster clip",
        input="../gcore/data/byte.tif",
        output=tmp_path / "out_bottom.nc",
        window=[0, 10, 20, 10],
    )

    gdal.Run(
        "mdim mosaic",
        input=[tmp_path / "out_top.nc", tmp_path / "out_bottom.nc"],
        output=tmp_path / "out.vrt",
    )

    with gdal.OpenEx(tmp_path / "out.vrt", gdal.OF_MULTIDIM_RASTER) as ds:
        assert (
            ds.GetRootGroup()
            .OpenMDArray("Band1")
            .AsClassicDataset(1, 0)
            .GetRasterBand(1)
            .Checksum()
            == 4855
        )

    gdal.Run(
        "mdim mosaic",
        input=[tmp_path / "out_bottom.nc", tmp_path / "out_top.nc"],
        output=tmp_path / "out.vrt",
        overwrite=True,
    )

    with gdal.OpenEx(tmp_path / "out.vrt", gdal.OF_MULTIDIM_RASTER) as ds:
        assert (
            ds.GetRootGroup()
            .OpenMDArray("Band1")
            .AsClassicDataset(1, 0)
            .GetRasterBand(1)
            .Checksum()
            == 4855
        )
