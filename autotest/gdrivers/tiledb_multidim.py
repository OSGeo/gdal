#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write multidimensional functionality for TileDB format.
# Author:   TileDB, Inc
#
###############################################################################
# Copyright (c) 2023, TileDB, Inc
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
import math
import os
import shutil

import pytest

from osgeo import gdal, osr


def has_tiledb_multidim():
    drv = gdal.GetDriverByName("TileDB")
    if drv is None:
        return False
    return drv.GetMetadataItem(gdal.DCAP_CREATE_MULTIDIMENSIONAL) == "YES"


pytestmark = [
    pytest.mark.require_driver("TileDB"),
    pytest.mark.skipif(not has_tiledb_multidim(), reason="TileDB >= 2.15 required"),
]


def test_tiledb_multidim_basic():

    filename = "tmp/test_tiledb_multidim_basic.tiledb"

    def create():

        drv = gdal.GetDriverByName("TileDB")
        ds = drv.CreateMultiDimensional(filename)
        rg = ds.GetRootGroup()
        assert rg
        assert rg.GetName() == "/"
        assert rg.GetFullName() == "/"
        assert not rg.GetMDArrayNames()
        assert not rg.GetGroupNames()
        assert not rg.GetAttributes()
        assert not rg.GetDimensions()
        with pytest.raises(Exception):
            rg.OpenMDArray("not existing")
        with pytest.raises(Exception):
            rg.OpenGroup("not existing")
        with pytest.raises(Exception):
            rg.GetAttribute("not existing")

        group = rg.CreateGroup("group")
        assert rg.GetGroupNames() == ["group"]
        assert group.GetName() == "group"
        assert group.GetFullName() == "/group"
        group_got = rg.OpenGroup("group")
        assert group_got.GetName() == "group"

        with pytest.raises(Exception, match="already exists"):
            rg.CreateGroup("group")

        with pytest.raises(Exception, match="Zero-dimension"):
            group.CreateMDArray("ar", [], gdal.ExtendedDataType.Create(gdal.GDT_Byte))

        dim0 = rg.CreateDimension("dim0", None, None, 2)
        with pytest.raises(Exception, match="Only numeric data types"):
            group.CreateMDArray("ar", [dim0], gdal.ExtendedDataType.CreateString())

        with pytest.raises(Exception, match="Invalid array name"):
            group.CreateMDArray("", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_Byte))

        os.mkdir(filename + "/group/ar")
        with pytest.raises(Exception, match="Path .* already exists"):
            group.CreateMDArray(
                "ar", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_Byte)
            )
        os.rmdir(filename + "/group/ar")

        ar = group.CreateMDArray(
            "ar", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_Byte)
        )
        assert group.GetMDArrayNames() == ["ar"]
        assert ar.GetName() == "ar"
        assert ar.GetFullName() == "/group/ar"
        ar.SetUnit("my_unit")

        group.CreateGroup("subgroup")
        with pytest.raises(Exception, match="An array named ar already exists"):
            group.CreateGroup("ar")
        with pytest.raises(Exception, match="A group named subgroup already exists"):
            group.CreateMDArray(
                "subgroup", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_Byte)
            )

        attr = group.CreateAttribute(
            "str_attr", [], gdal.ExtendedDataType.CreateString()
        )
        assert attr
        assert attr.Write("my_string") == gdal.CE_None

        attr = group.CreateAttribute(
            "float64_attr", [], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        )
        assert attr
        assert attr.Write(1.25) == gdal.CE_None

        attr = group.CreateAttribute(
            "two_int32_attr", [2], gdal.ExtendedDataType.Create(gdal.GDT_Int32)
        )
        assert attr
        assert attr.Write([123456789, -123456789]) == gdal.CE_None

        attr = group.CreateAttribute(
            "to_be_deleted", [], gdal.ExtendedDataType.Create(gdal.GDT_Int32)
        )
        assert attr
        group.DeleteAttribute("to_be_deleted")
        with pytest.raises(Exception, match="has been deleted"):
            attr.Write(1)
        with pytest.raises(Exception, match="has been deleted"):
            attr.Read()

        attr = ar.CreateAttribute("foo", [], gdal.ExtendedDataType.CreateString())
        assert attr
        assert attr.Write("bar") == gdal.CE_None

        attr = ar.CreateAttribute(
            "to_be_deleted", [], gdal.ExtendedDataType.Create(gdal.GDT_Int32)
        )
        assert attr
        group.DeleteAttribute("to_be_deleted")

    def reopen_readonly():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        assert rg
        assert rg.GetGroupNames() == ["group"]
        group = rg.OpenGroup("group")
        assert group.GetName() == "group"
        assert group.GetFullName() == "/group"

        assert group.GetMDArrayNames() == ["ar"]
        assert group.GetGroupNames() == ["subgroup"]

        assert set([x.GetName() for x in group.GetAttributes()]) == set(
            ["str_attr", "float64_attr", "two_int32_attr"]
        )

        attr = group.GetAttribute("str_attr")
        assert attr.GetDataType().GetClass() == gdal.GEDTC_STRING
        assert attr.Read() == "my_string"
        with pytest.raises(Exception, match=r".*not .* in update mode.*"):
            attr.Write("modified")

        attr = group.GetAttribute("float64_attr")
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Float64
        assert attr.GetDimensionsSize() == [1]
        assert attr.Read() == 1.25

        attr = group.GetAttribute("two_int32_attr")
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Int32
        assert attr.GetDimensionsSize() == [2]
        assert attr.Read() == (123456789, -123456789)

        with pytest.raises(Exception, match=r".*not .* in update mode.*"):
            group.CreateGroup("new_subgroup")

        dim0 = rg.CreateDimension("dim0", None, None, 2)

        with pytest.raises(Exception, match=r".*not .* in update mode.*"):
            group.CreateMDArray(
                "new_ar", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_Byte)
            )

        with pytest.raises(Exception, match=r".*not .* in update mode.*"):
            group.CreateAttribute("new_attr", [], gdal.ExtendedDataType.CreateString())

        with pytest.raises(Exception, match=r".*not .* in update mode.*"):
            group.DeleteAttribute("str_attr")

        ar = group.OpenMDArray("ar")
        assert ar.GetDimensionCount() == 1
        assert ar.GetDimensions()[0].GetName() == "dim0"
        assert ar.GetDimensions()[0].GetSize() == 2
        assert set([x.GetName() for x in ar.GetAttributes()]) == set(["foo"])
        assert ar.GetAttribute("foo").Read() == "bar"
        assert ar.GetUnit() == "my_unit"

        with pytest.raises(Exception, match=r".*not .* in update mode.*"):
            ar.CreateAttribute("new_attr", [], gdal.ExtendedDataType.CreateString())

    if os.path.exists(filename):
        shutil.rmtree(filename)
    try:
        create()
        reopen_readonly()
    finally:
        if os.path.exists(filename):
            shutil.rmtree(filename)


###############################################################################


@pytest.mark.parametrize(
    "gdal_data_type",
    [
        gdal.GDT_Int8,
        gdal.GDT_Byte,
        gdal.GDT_Int16,
        gdal.GDT_UInt16,
        gdal.GDT_Int32,
        gdal.GDT_UInt32,
        gdal.GDT_Int64,
        gdal.GDT_UInt64,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
        gdal.GDT_CInt16,
        gdal.GDT_CInt32,
        gdal.GDT_CFloat32,
        gdal.GDT_CFloat64,
    ],
)
def test_tiledb_multidim_array_data_types(gdal_data_type):

    filename = "tmp/test_tiledb_multidim_array_data_types.tiledb"

    def create():

        drv = gdal.GetDriverByName("TileDB")
        ds = drv.CreateMultiDimensional(filename)
        rg = ds.GetRootGroup()
        dim0 = rg.CreateDimension("dim0", None, None, 2)
        ar = rg.CreateMDArray(
            "ar", [dim0], gdal.ExtendedDataType.Create(gdal_data_type)
        )
        assert ar.GetDataType() == gdal.ExtendedDataType.Create(gdal_data_type)
        ar.Write(b"\0" * (ar.GetDataType().GetSize() * dim0.GetSize()))

    def reopen_readonly():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("ar")
        assert ar.GetDataType() == gdal.ExtendedDataType.Create(gdal_data_type)
        dim0 = ar.GetDimensions()[0]
        assert ar.Read() == b"\0" * (ar.GetDataType().GetSize() * dim0.GetSize())

    if os.path.exists(filename):
        shutil.rmtree(filename)
    try:
        create()
        reopen_readonly()
    finally:
        if os.path.exists(filename):
            shutil.rmtree(filename)


###############################################################################


def test_tiledb_multidim_array_nodata():

    filename = "tmp/test_tiledb_multidim_array_nodata.tiledb"

    def test():

        drv = gdal.GetDriverByName("TileDB")
        ds = drv.CreateMultiDimensional(filename)
        rg = ds.GetRootGroup()
        dim0 = rg.CreateDimension("dim0", None, None, 3)
        dim1 = rg.CreateDimension("dim1", None, None, 5)
        ar = rg.CreateMDArray(
            "ar", [dim0, dim1], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        )
        ar.SetNoDataValue(1.5)

    def reopen_readonly():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("ar")
        assert ar.GetNoDataValueAsDouble() == 1.5

    if os.path.exists(filename):
        shutil.rmtree(filename)
    try:
        test()
        reopen_readonly()
    finally:
        if os.path.exists(filename):
            shutil.rmtree(filename)


###############################################################################


def test_tiledb_multidim_array_nodata_cannot_be_set_after_finalize():

    filename = (
        "tmp/test_tiledb_multidim_array_nodata_cannot_be_set_after_finalize.tiledb"
    )

    def test():

        drv = gdal.GetDriverByName("TileDB")
        ds = drv.CreateMultiDimensional(filename)
        rg = ds.GetRootGroup()
        dim0 = rg.CreateDimension("dim0", None, None, 3)
        dim1 = rg.CreateDimension("dim1", None, None, 5)
        ar = rg.CreateMDArray(
            "ar", [dim0, dim1], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        )
        ar.Write(array.array("d", [i for i in range(3 * 5)]))
        with pytest.raises(
            Exception, match="not supported after array has been finalized"
        ):
            ar.SetNoDataValue(1.5)

    def reopen_readonly():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("ar")
        assert math.isnan(ar.GetNoDataValueAsDouble())

    if os.path.exists(filename):
        shutil.rmtree(filename)
    try:
        test()
        reopen_readonly()
    finally:
        if os.path.exists(filename):
            shutil.rmtree(filename)


###############################################################################


def test_tiledb_multidim_array_blocksize():

    filename = "tmp/test_tiledb_multidim_array_blocksize.tiledb"

    def test():

        drv = gdal.GetDriverByName("TileDB")
        ds = drv.CreateMultiDimensional(filename)
        rg = ds.GetRootGroup()
        dim0 = rg.CreateDimension("dim0", None, None, 3)
        dim1 = rg.CreateDimension("dim1", None, None, 5)
        rg.CreateMDArray(
            "ar",
            [dim0, dim1],
            gdal.ExtendedDataType.Create(gdal.GDT_Float64),
            ["BLOCKSIZE=2,4"],
        )

    def reopen_readonly():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("ar")
        assert ar.GetBlockSize() == [2, 4]

    if os.path.exists(filename):
        shutil.rmtree(filename)
    try:
        test()
        reopen_readonly()
    finally:
        if os.path.exists(filename):
            shutil.rmtree(filename)


###############################################################################


def test_tiledb_multidim_array_compression():

    filename = "tmp/test_tiledb_multidim_array_compression.tiledb"

    def test():

        drv = gdal.GetDriverByName("TileDB")
        ds = drv.CreateMultiDimensional(filename)
        rg = ds.GetRootGroup()
        dim0 = rg.CreateDimension("dim0", None, None, 3)
        rg.CreateMDArray(
            "ar",
            [dim0],
            gdal.ExtendedDataType.Create(gdal.GDT_Float64),
            ["COMPRESSION=ZSTD"],
        )

    def reopen_readonly():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("ar")
        assert ar.GetStructuralInfo() == {"FILTER_LIST": "ZSTD"}

    if os.path.exists(filename):
        shutil.rmtree(filename)
    try:
        test()
        reopen_readonly()
    finally:
        if os.path.exists(filename):
            shutil.rmtree(filename)


###############################################################################


def test_tiledb_multidim_array_same_name_as_dim():

    filename = "tmp/test_tiledb_multidim_array_same_name_as_dim.tiledb"

    def test():

        drv = gdal.GetDriverByName("TileDB")
        ds = drv.CreateMultiDimensional(filename)
        rg = ds.GetRootGroup()
        dim0 = rg.CreateDimension("t", None, None, 3)
        rg.CreateMDArray("t", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_Float64))

    def reopen_readonly():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("t")
        assert ar.GetDimensions()[0].GetName() == "t_dim"

    if os.path.exists(filename):
        shutil.rmtree(filename)
    try:
        test()
        reopen_readonly()
    finally:
        if os.path.exists(filename):
            shutil.rmtree(filename)


###############################################################################


def test_tiledb_multidim_array_read_write():

    filename = "tmp/test_tiledb_multidim_array_read_write.tiledb"

    def test():

        drv = gdal.GetDriverByName("TileDB")
        ds = drv.CreateMultiDimensional(filename)
        rg = ds.GetRootGroup()
        dim0 = rg.CreateDimension("dim0", None, None, 3)
        dim1 = rg.CreateDimension("dim1", None, None, 5)
        ar = rg.CreateMDArray(
            "ar", [dim0, dim1], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        )
        ar.Write(
            array.array("d", [i for i in range(1 * 5)]),
            array_start_idx=[0, 0],
            count=[1, 5],
        )
        ar.Write(
            array.array("d", [5 + i for i in range(2 * 5)]),
            array_start_idx=[1, 0],
            count=[2, 5],
        )

        with pytest.raises(Exception, match="Write parameters not supported"):
            ar.Write(
                array.array("f", [0 for i in range(3 * 5)]),
                buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float32),
            )

        assert array.array("d", ar.Read()).tolist() == [i for i in range(3 * 5)]
        assert array.array(
            "d", ar.Read(array_start_idx=[1, 2], count=[1, 1])
        ).tolist() == [7]
        assert array.array(
            "d", ar.Read(array_start_idx=[0, 1], count=[2, 1])
        ).tolist() == [1, 6]
        assert array.array(
            "d", ar.Read(array_start_idx=[1, 0], count=[1, 2])
        ).tolist() == [5, 6]

        # Uses GDALMDArray::ReadUsingContiguousIRead()
        assert array.array(
            "f", ar.Read(buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float32))
        ).tolist() == [i for i in range(3 * 5)]
        assert array.array("d", ar.Read(buffer_stride=[1, 3])).tolist() == [
            0.0,
            5.0,
            10.0,
            1.0,
            6.0,
            11.0,
            2.0,
            7.0,
            12.0,
            3.0,
            8.0,
            13.0,
            4.0,
            9.0,
            14.0,
        ]
        assert (
            array.array(
                "d", ar.Read(array_start_idx=[2, 4], count=[3, 5], array_step=[-1, -1])
            ).tolist()
            == [i for i in range(3 * 5)][::-1]
        )
        assert array.array(
            "d", ar.Read(array_start_idx=[2, 1], count=[1, 2], array_step=[1, 2])
        ).tolist() == [11, 13]

    if os.path.exists(filename):
        shutil.rmtree(filename)
    try:
        test()
    finally:
        if os.path.exists(filename):
            shutil.rmtree(filename)


###############################################################################


@pytest.mark.parametrize("epsg_code,axis_mapping", [(4326, [1, 2]), (32631, [2, 1])])
def test_tiledb_multidim_array_read_dim_label_and_spatial_ref(epsg_code, axis_mapping):

    filename = "tmp/test_tiledb_multidim_array_read_dim_label.tiledb"

    def test():

        drv = gdal.GetDriverByName("TileDB")
        ds = drv.CreateMultiDimensional(filename)
        rg = ds.GetRootGroup()
        dim0 = rg.CreateDimension("dim0", "type", "direction", 3)
        dim0_ar = rg.CreateMDArray(
            "dim0",
            [dim0],
            gdal.ExtendedDataType.Create(gdal.GDT_Float32),
            ["IN_MEMORY=YES"],
        )
        dim0_ar.Write(array.array("f", [3, 2, 0]))
        dim0.SetIndexingVariable(dim0_ar)
        dim1 = rg.CreateDimension("dim1", None, None, 5)
        dim1_ar = rg.CreateMDArray(
            "dim1",
            [dim1],
            gdal.ExtendedDataType.Create(gdal.GDT_Float64),
            ["IN_MEMORY=YES"],
        )
        dim1_ar.Write(array.array("d", [10, 20, 30, 40, 60]))
        dim1.SetIndexingVariable(dim1_ar)
        ar = rg.CreateMDArray(
            "ar", [dim0, dim1], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        )
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(epsg_code)
        ar.SetSpatialRef(srs)

    def reopen_readonly():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("ar")
        dims = ar.GetDimensions()
        assert dims[0].GetName() == "dim0"
        assert dims[0].GetType() == "type"
        assert dims[0].GetDirection() == "direction"
        assert array.array("f", dims[0].GetIndexingVariable().Read()).tolist() == [
            3,
            2,
            0,
        ]
        assert dims[1].GetName() == "dim1"
        assert array.array("d", dims[1].GetIndexingVariable().Read()).tolist() == [
            10,
            20,
            30,
            40,
            60,
        ]
        srs = ar.GetSpatialRef()
        assert srs.GetAuthorityCode(None) == str(epsg_code)
        assert srs.GetDataAxisToSRSAxisMapping() == axis_mapping

        # Test that gdalmdiminfo can deal with dimensions whose indexing variable
        # is not owned by a group, but by the array itself
        info = gdal.MultiDimInfo(ds, detailed=True)
        assert info["arrays"]["ar"]["dimensions"][0]["indexing_variable"] == {
            "dim0": {
                "datatype": "Float32",
                "dimensions": [
                    {"name": "index", "full_name": "/ar/dim0/index", "size": 3}
                ],
                "dimension_size": [3],
                "block_size": [3],
                "attributes": {
                    "_DIM_DIRECTION": {"datatype": "String", "value": "direction"},
                    "_DIM_TYPE": {"datatype": "String", "value": "type"},
                },
                "nodata_value": "NaN",
                "values": [3, 2, 0],
            }
        }

    if os.path.exists(filename):
        shutil.rmtree(filename)
    try:
        test()
        reopen_readonly()
    finally:
        if os.path.exists(filename):
            shutil.rmtree(filename)


###############################################################################


def test_tiledb_multidim_array_read_gdal_raster_classic():

    filename = "tmp/test_tiledb_multidim_array_read_gdal_raster_classic.tiledb"

    def test():
        gdal.Translate(filename, "data/small_world.tif", format="TileDB")
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        dims = ar.GetDimensions()
        assert len(dims) == 3
        assert dims[0].GetName() == "BANDS"
        assert dims[0].GetSize() == 3
        assert dims[1].GetName() == "Y"
        assert dims[1].GetType() == "HORIZONTAL_Y"
        assert dims[1].GetSize() == 200
        assert dims[2].GetName() == "X"
        assert dims[2].GetType() == "HORIZONTAL_X"
        assert dims[2].GetSize() == 400
        Y = dims[1].GetIndexingVariable()
        assert array.array("d", Y.Read()).tolist() == pytest.approx(
            [89.55 - i * 0.90 for i in range(200)]
        )
        X = dims[2].GetIndexingVariable()
        assert array.array("d", X.Read()).tolist() == pytest.approx(
            [-179.55 + i * 0.90 for i in range(400)]
        )
        srs = ar.GetSpatialRef()
        assert srs.GetAuthorityCode(None) == "4326"
        assert srs.GetDataAxisToSRSAxisMapping() == [2, 3]
        ds_ref = gdal.Open("data/small_world.tif")
        assert ar.Read() == ds_ref.ReadRaster()

    if os.path.exists(filename):
        shutil.rmtree(filename)
    try:
        test()
    finally:
        if os.path.exists(filename):
            shutil.rmtree(filename)


###############################################################################


def test_tiledb_multidim_array_read_gdal_raster_classic_interleave_attributes():

    filename = "tmp/test_tiledb_multidim_array_read_gdal_raster_classic_interleave_attributes.tiledb"

    def test():
        gdal.Translate(
            filename,
            "data/small_world.tif",
            format="TileDB",
            creationOptions=["INTERLEAVE=ATTRIBUTES"],
        )
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        assert rg.GetMDArrayNames() == [
            filename[len("tmp/") :] + ".TDB_VALUES_%d" % i for i in range(1, 3 + 1)
        ]
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        dims = ar.GetDimensions()
        assert len(dims) == 2
        assert dims[0].GetName() == "Y"
        assert dims[0].GetSize() == 200
        assert dims[1].GetName() == "X"
        assert dims[1].GetSize() == 400
        ds_ref = gdal.Open("data/small_world.tif")
        assert ar.Read() == ds_ref.GetRasterBand(1).ReadRaster()

    if os.path.exists(filename):
        shutil.rmtree(filename)
    try:
        test()
    finally:
        if os.path.exists(filename):
            shutil.rmtree(filename)


###############################################################################


@pytest.mark.require_driver("netCDF")
def test_tiledb_multidim_translate_from_netcdf():

    filename = "tmp/test_tiledb_multidim_translate_from_netcdf.tiledb"

    def test():
        gdal.MultiDimTranslate(
            filename, "data/netcdf/netcdf-4d.nc", format="TileDB", arraySpecs=["t"]
        )
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("t")
        assert ar.GetDimensionCount() == 4
        assert ar.GetDimensions()[0].GetIndexingVariable().GetName() == "time"
        assert ar.GetDimensions()[1].GetIndexingVariable().GetName() == "levelist"
        assert ar.GetDimensions()[2].GetIndexingVariable().GetName() == "latitude"
        assert ar.GetDimensions()[3].GetIndexingVariable().GetName() == "longitude"
        assert ar.GetNoDataValueAsDouble() == -32767.0

    if os.path.exists(filename):
        shutil.rmtree(filename)
    try:
        test()
    finally:
        if os.path.exists(filename):
            shutil.rmtree(filename)


###############################################################################


def test_tiledb_multidim_open_converted_by_tiledb_cf_netcdf_convert():

    filename = "data/tiledb/byte_epsg_3949_cf1.tiledb"
    ds = gdal.Open(filename)
    assert (
        ds.GetSpatialRef().ExportToProj4()
        == "+proj=lcc +lat_0=49 +lon_0=3 +lat_1=48.25 +lat_2=49.75 +x_0=1700000 +y_0=8200000 +ellps=GRS80 +units=m +no_defs"
    )
    assert ds.GetGeoTransform() == pytest.approx(
        (440720.0, 60.0, 0.0, 3750120.0, 0.0, 60.0)
    )
