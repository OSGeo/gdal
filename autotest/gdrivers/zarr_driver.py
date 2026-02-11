#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Zarr driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2021, Even Rouault <even.rouault@spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import base64
import json
import math
import os
import shutil
import struct
import sys

import gdaltest
import pytest
import webserver

from osgeo import gdal, osr

pytestmark = pytest.mark.require_driver("ZARR")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


_gdal_data_type_to_array_type = {
    gdal.GDT_Int8: "b",
    gdal.GDT_UInt8: "B",
    gdal.GDT_Int16: "h",
    gdal.GDT_UInt16: "H",
    gdal.GDT_Int32: "i",
    gdal.GDT_UInt32: "I",
    gdal.GDT_Int64: "q",
    gdal.GDT_UInt64: "Q",
    gdal.GDT_Float16: "e",
    gdal.GDT_Float32: "f",
    gdal.GDT_Float64: "d",
    gdal.GDT_CFloat16: "e",
    gdal.GDT_CFloat32: "f",
    gdal.GDT_CFloat64: "d",
}


@pytest.mark.parametrize(
    "dtype,gdaltype,fill_value,nodata_value",
    [
        ["!b1", gdal.GDT_UInt8, None, None],
        ["!i1", gdal.GDT_Int8, None, None],
        ["!i1", gdal.GDT_Int8, -1, -1],
        ["!u1", gdal.GDT_UInt8, None, None],
        [
            "!u1",
            gdal.GDT_UInt8,
            "1",
            1,
        ],  # not really legit to have the fill_value as a str
        ["<i2", gdal.GDT_Int16, None, None],
        [">i2", gdal.GDT_Int16, None, None],
        ["<i4", gdal.GDT_Int32, None, None],
        [">i4", gdal.GDT_Int32, None, None],
        ["<i8", gdal.GDT_Int64, None, None],
        ["<i8", gdal.GDT_Int64, -(1 << 63), -(1 << 63)],
        [
            "<i8",
            gdal.GDT_Int64,
            str(-(1 << 63)),
            -(1 << 63),
        ],  # not really legit to have the fill_value as a str
        [">i8", gdal.GDT_Int64, None, None],
        ["<u2", gdal.GDT_UInt16, None, None],
        [">u2", gdal.GDT_UInt16, None, None],
        ["<u4", gdal.GDT_UInt32, None, None],
        [">u4", gdal.GDT_UInt32, None, None],
        ["<u4", gdal.GDT_UInt32, 4000000000, 4000000000],
        [
            "<u8",
            gdal.GDT_UInt64,
            str((1 << 64) - 1),
            (1 << 64) - 1,
        ],  # not really legit to have the fill_value as a str, but libjson-c can't support numeric values in int64::max(), uint64::max() range.
        [">u8", gdal.GDT_UInt64, None, None],
        # We would like to test these, but SWIG does not support float16 (yet?)
        # ["<f2", gdal.GDT_Float16, None, None],
        # [">f2", gdal.GDT_Float16, None, None],
        # ["<f2", gdal.GDT_Float16, 1.5, 1.5],
        # ["<f2", gdal.GDT_Float16, "NaN", float("nan")],
        # ["<f2", gdal.GDT_Float16, "Infinity", float("infinity")],
        # ["<f2", gdal.GDT_Float16, "-Infinity", float("-infinity")],
        ["<f4", gdal.GDT_Float32, None, None],
        [">f4", gdal.GDT_Float32, None, None],
        ["<f4", gdal.GDT_Float32, 1.5, 1.5],
        ["<f4", gdal.GDT_Float32, "NaN", float("nan")],
        ["<f4", gdal.GDT_Float32, "Infinity", float("infinity")],
        ["<f4", gdal.GDT_Float32, "-Infinity", float("-infinity")],
        ["<f8", gdal.GDT_Float64, None, None],
        [">f8", gdal.GDT_Float64, None, None],
        ["<f8", gdal.GDT_Float64, "NaN", float("nan")],
        ["<f8", gdal.GDT_Float64, "Infinity", float("infinity")],
        ["<f8", gdal.GDT_Float64, "-Infinity", float("-infinity")],
        # We would like to test these, but SWIG does not support complex32 (yet?)
        # ["<c4", gdal.GDT_CFloat16, None, None],
        # [">c4", gdal.GDT_CFloat16, None, None],
        ["<c8", gdal.GDT_CFloat32, None, None],
        [">c8", gdal.GDT_CFloat32, None, None],
        ["<c16", gdal.GDT_CFloat64, None, None],
        [">c16", gdal.GDT_CFloat64, None, None],
    ],
)
@pytest.mark.parametrize("use_optimized_code_paths", [True, False])
def test_zarr_basic(
    tmp_vsimem, dtype, gdaltype, fill_value, nodata_value, use_optimized_code_paths
):

    structtype = _gdal_data_type_to_array_type[gdaltype]

    j = {
        "chunks": [2, 3],
        "compressor": None,
        "dtype": dtype,
        "fill_value": fill_value,
        "filters": None,
        "order": "C",
        "shape": [5, 4],
        "zarr_format": 2,
    }

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zarray", json.dumps(j))
    if gdaltype not in (gdal.GDT_CFloat16, gdal.GDT_CFloat32, gdal.GDT_CFloat64):
        tile_0_0_data = struct.pack(dtype[0] + (structtype * 6), 1, 2, 3, 5, 6, 7)
        tile_0_1_data = struct.pack(dtype[0] + (structtype * 6), 4, 0, 0, 8, 0, 0)
    else:
        tile_0_0_data = struct.pack(
            dtype[0] + (structtype * 12), 1, 11, 2, 0, 3, 0, 5, 0, 6, 0, 7, 0
        )
        tile_0_1_data = struct.pack(
            dtype[0] + (structtype * 12), 4, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0
        )
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/0.0", tile_0_0_data)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/0.1", tile_0_1_data)
    with gdaltest.config_option(
        "GDAL_ZARR_USE_OPTIMIZED_CODE_PATHS",
        "YES" if use_optimized_code_paths else "NO",
    ):
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.GetDimensionCount() == 2
    assert [ar.GetDimensions()[i].GetSize() for i in range(2)] == [5, 4]
    assert ar.GetBlockSize() == [2, 3]
    if nodata_value is not None and math.isnan(nodata_value):
        assert math.isnan(ar.GetNoDataValue())
    else:
        assert ar.GetNoDataValue() == nodata_value

    assert ar.GetOffset() is None
    assert ar.GetScale() is None
    assert ar.GetUnit() == ""
    assert ar.GetOverviewCount() == 0
    assert ar.GetOverview(0) is None

    # Check reading one single value
    assert ar[1, 2].Read(
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    ) == struct.pack("d" * 1, 7)

    structtype_read = structtype

    # Read block 0,0
    if gdaltype not in (gdal.GDT_CFloat16, gdal.GDT_CFloat32, gdal.GDT_CFloat64):
        assert ar[0:2, 0:3].Read(
            buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        ) == struct.pack("d" * 6, 1, 2, 3, 5, 6, 7)
        assert struct.unpack(structtype_read * 6, ar[0:2, 0:3].Read()) == (
            1,
            2,
            3,
            5,
            6,
            7,
        )
    else:
        assert ar[0:2, 0:3].Read(
            buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_CFloat64)
        ) == struct.pack("d" * 12, 1, 11, 2, 0, 3, 0, 5, 0, 6, 0, 7, 0)
        assert struct.unpack(structtype * 12, ar[0:2, 0:3].Read()) == (
            1,
            11,
            2,
            0,
            3,
            0,
            5,
            0,
            6,
            0,
            7,
            0,
        )

    # Read block 0,1
    assert ar[0:2, 3:4].Read(
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    ) == struct.pack("d" * 2, 4, 8)

    # Read block 1,1 (missing)
    nv = nodata_value if nodata_value else 0
    assert ar[2:4, 3:4].Read(
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    ) == struct.pack("d" * 2, nv, nv)

    # Read whole raster
    assert ar.Read(
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)
    ) == struct.pack(
        "d" * 20,
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        8,
        nv,
        nv,
        nv,
        nv,
        nv,
        nv,
        nv,
        nv,
        nv,
        nv,
        nv,
        nv,
    )

    if gdaltype not in (gdal.GDT_CFloat16, gdal.GDT_CFloat32, gdal.GDT_CFloat64):
        assert ar.Read() == array.array(
            structtype_read,
            [
                1,
                2,
                3,
                4,
                5,
                6,
                7,
                8,
                nv,
                nv,
                nv,
                nv,
                nv,
                nv,
                nv,
                nv,
                nv,
                nv,
                nv,
                nv,
            ],
        )
    else:
        assert ar.Read() == array.array(
            structtype,
            [
                1,
                11,
                2,
                0,
                3,
                0,
                4,
                0,
                5,
                0,
                6,
                0,
                7,
                0,
                8,
                0,
                nv,
                0,
                nv,
                0,
                nv,
                0,
                nv,
                0,
                nv,
                0,
                nv,
                0,
                nv,
                0,
                nv,
                0,
                nv,
                0,
                nv,
                0,
                nv,
                0,
                nv,
                0,
            ],
        )
    # Read with negative steps
    assert ar.Read(
        array_start_idx=[2, 1],
        count=[2, 2],
        array_step=[-1, -1],
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64),
    ) == struct.pack("d" * 4, nv, nv, 6, 5)

    # array_step > 2
    assert ar.Read(
        array_start_idx=[0, 0],
        count=[1, 2],
        array_step=[0, 2],
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64),
    ) == struct.pack("d" * 2, 1, 3)

    assert ar.Read(
        array_start_idx=[0, 0],
        count=[3, 1],
        array_step=[2, 0],
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64),
    ) == struct.pack("d" * 3, 1, nv, nv)

    assert ar.Read(
        array_start_idx=[0, 1],
        count=[1, 2],
        array_step=[0, 2],
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64),
    ) == struct.pack("d" * 2, 2, 4)

    assert ar.Read(
        array_start_idx=[0, 0],
        count=[1, 2],
        array_step=[0, 3],
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64),
    ) == struct.pack("d" * 2, 1, 4)


@pytest.mark.parametrize(
    "fill_value,expected_read_data",
    [[base64.b64encode(b"xyz").decode("utf-8"), ["abc", "xyz"]], [None, ["abc", None]]],
)
def test_zarr_string(tmp_vsimem, fill_value, expected_read_data):

    j = {
        "chunks": [1],
        "compressor": None,
        "dtype": "|S3",
        "fill_value": fill_value,
        "filters": [],
        "order": "C",
        "shape": [2],
        "zarr_format": 2,
    }

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zarray", json.dumps(j))
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/0", b"abc")
    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read() == expected_read_data


# Check that all required elements are present in .zarray
@pytest.mark.parametrize(
    "member",
    [
        None,
        "zarr_format",
        "chunks",
        "compressor",
        "dtype",
        "filters",
        "order",
        "shape",
        "fill_value",
    ],
)
def test_zarr_invalid_json_remove_member(tmp_vsimem, member):

    j = {
        "chunks": [2, 3],
        "compressor": None,
        "dtype": "!b1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [5, 4],
        "zarr_format": 2,
    }

    if member:
        del j[member]

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zarray", json.dumps(j))
    with gdal.quiet_errors():
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
    if member == "fill_value":
        assert ds is not None
        assert gdal.GetLastErrorMsg() != ""
    elif member is None:
        assert ds
    else:
        assert ds is None


# Check bad values of members in .zarray
@pytest.mark.parametrize(
    "dict_update",
    [
        {"chunks": None},
        {"chunks": "invalid"},
        {"chunks": [2]},
        {"chunks": [2, 0]},
        {"shape": None},
        {"shape": "invalid"},
        {"shape": [5]},
        {"shape": [5, 0]},
        {"chunks": [1 << 40, 1 << 40], "shape": [1 << 40, 1 << 40]},
        {"shape": [1 << 30, 1 << 30, 1 << 30], "chunks": [1, 1, 1]},
        {"dtype": None},
        {"dtype": 1},
        {"dtype": ""},
        {"dtype": "!"},
        {"dtype": "!b"},
        {"dtype": "<u16"},
        {"dtype": "<u0"},
        {"dtype": "<u10000"},
        {"fill_value": []},
        {"fill_value": "x"},
        {"fill_value": "NaN"},
        {"dtype": "!S1", "fill_value": 0},
        {"order": None},
        {"order": "invalid"},
        {"compressor": "invalid"},
        {"compressor": {}},
        {"compressor": {"id": "invalid"}},
        {"filters": "invalid"},
        {"filters": {}},
        {"filters": [{"missing_id": True}]},
        {"zarr_format": None},
        {"zarr_format": 1},
    ],
)
def test_zarr_invalid_json_wrong_values(tmp_vsimem, dict_update):

    j = {
        "chunks": [2, 3],
        "compressor": None,
        "dtype": "!b1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [5, 4],
        "zarr_format": 2,
    }

    j.update(dict_update)

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zarray", json.dumps(j))
    with gdal.quiet_errors():
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
    assert ds is None


# Check reading different compression methods
@pytest.mark.parametrize(
    "datasetname,compressor",
    [
        ("blosc.zarr", "blosc"),
        ("gzip.zarr", "gzip"),
        ("lz4.zarr", "lz4"),
        ("lzma.zarr", "lzma"),
        ("lzma_with_filters.zarr", "lzma"),
        ("zlib.zarr", "zlib"),
        ("zstd.zarr", "zstd"),
    ],
)
def test_zarr_read_compression_methods(datasetname, compressor):

    compressors = gdal.GetDriverByName("Zarr").GetMetadataItem("COMPRESSORS")
    filename = "data/zarr/" + datasetname

    if compressor not in compressors:
        with gdal.quiet_errors():
            ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        assert ds is None
    else:
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar
        assert ar.Read() == array.array("b", [1, 2])
        assert json.loads(ar.GetStructuralInfo()["COMPRESSOR"])["id"] == compressor


# Check reading different compression methods
@pytest.mark.parametrize(
    "datasetname,compressor",
    [
        ("gzip.zarr", "gzip"),
    ],
)
def test_zarr_v3_read_compression_methods(datasetname, compressor):

    compressors = gdal.GetDriverByName("Zarr").GetMetadataItem("COMPRESSORS")
    filename = "data/zarr/v3/" + datasetname

    if compressor not in compressors:
        with gdal.quiet_errors():
            ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        assert ds is None
    else:
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar
        assert ar.Read() == array.array("b", [1, 2])
        assert json.loads(ar.GetStructuralInfo()["COMPRESSOR"])["name"] == compressor


def test_zarr_read_shuffle_filter():

    filename = "data/zarr/shuffle.zarr"
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read() == array.array("h", [1, 2])
    assert json.loads(ar.GetStructuralInfo()["FILTERS"]) == [
        {"elementsize": 2, "id": "shuffle"}
    ]


def test_zarr_read_shuffle_filter_update(tmp_path):

    out_filename = tmp_path / "filter_update.zarr"
    shutil.copytree("data/zarr/shuffle.zarr", out_filename)

    def write():
        ds = gdal.OpenEx(out_filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        ar.Write([3, 4])

    write()

    ds = gdal.OpenEx(out_filename, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar.Read() == array.array("h", [3, 4])


def test_zarr_read_shuffle_quantize():

    filename = "data/zarr/quantize.zarr"
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read() == array.array(
        "d",
        [
            0,
            0.125,
            0.1875,
            0.3125,
            0.375,
            0.5,
            0.625,
            0.6875,
            0.8125,
            0.875,
            1,
            1.125,
            1.1875,
            1.3125,
            1.375,
            1.5,
            1.625,
            1.6875,
            1.8125,
            1.875,
            2,
            2.125,
            2.1875,
            2.3125,
            2.375,
            2.5,
            2.625,
            2.6875,
            2.8125,
            2.875,
            3,
            3.125,
            3.1875,
            3.3125,
            3.375,
            3.5,
            3.625,
            3.6875,
            3.8125,
            3.875,
            4,
            4.125,
            4.1875,
            4.3125,
            4.375,
            4.5,
            4.625,
            4.6875,
            4.8125,
            4.875,
            5,
            5.125,
            5.1875,
            5.3125,
            5.375,
            5.5,
            5.625,
            5.6875,
            5.8125,
            5.875,
            6,
            6.125,
            6.1875,
            6.3125,
            6.375,
            6.5,
            6.625,
            6.6875,
            6.8125,
            6.875,
            7,
            7.125,
            7.1875,
            7.3125,
            7.375,
            7.5,
            7.625,
            7.6875,
            7.8125,
            7.875,
            8,
            8.125,
            8.1875,
            8.3125,
            8.375,
            8.5,
            8.625,
            8.6875,
            8.8125,
            8.875,
            9.0,
            9.125,
            9.1875,
            9.3125,
            9.375,
            9.5,
            9.625,
            9.6875,
            9.8125,
            9.875,
        ],
    )


def test_zarr_read_shuffle_quantize_update_not_supported():

    filename = "data/zarr/quantize.zarr"

    def write():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        ar.Write(ar.Read())

    with gdal.quiet_errors():
        write()
        assert gdal.GetLastErrorMsg() == "quantize filter not supported for writing"


@pytest.mark.parametrize(
    "filename",
    [
        "data/zarr/fixedscaleoffset_dtype_f4_astype_u1.zarr",
        "data/zarr/fixedscaleoffset_dtype_f8_astype_u1.zarr",
        "data/zarr/fixedscaleoffset_dtype_f8_astype_u2.zarr",
        "data/zarr/fixedscaleoffset_dtype_f8_astype_u4.zarr",
        "data/zarr/fixedscaleoffset_dtype_f8_astype_f4.zarr",
    ],
)
def test_zarr_read_fixedscaleoffset(filename):

    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    if "dtype_f8" in filename:
        assert ar.Read() == array.array(
            "d",
            [
                1000.0,
                1000.1,
                1000.2,
                1000.3,
                1000.4,
                1000.6,
                1000.7,
                1000.8,
                1000.9,
                1001.0,
            ],
        )
    else:
        assert ar.Read() == array.array(
            "f",
            [
                1000.0,
                1000.1,
                1000.2,
                1000.3,
                1000.4,
                1000.6,
                1000.7,
                1000.8,
                1000.9,
                1001.0,
            ],
        )


@pytest.mark.parametrize("name", ["u1", "u2", "u4", "u8"])
def test_zarr_read_fortran_order(name):

    filename = "data/zarr/order_f_" + name + ".zarr"
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read(
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
    ) == array.array("b", [i for i in range(16)])


def test_zarr_read_fortran_order_string():

    filename = "data/zarr/order_f_s3.zarr"
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read() == [
        "000",
        "111",
        "222",
        "333",
        "444",
        "555",
        "666",
        "777",
        "888",
        "999",
        "AAA",
        "BBB",
        "CCC",
        "DDD",
        "EEE",
        "FFF",
    ]


def test_zarr_read_fortran_order_3d():

    filename = "data/zarr/order_f_u1_3d.zarr"
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read(
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
    ) == array.array("b", [i for i in range(2 * 3 * 4)])


def test_zarr_read_compound_well_aligned():

    filename = "data/zarr/compound_well_aligned.zarr"
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None

    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar

    dt = ar.GetDataType()
    assert dt.GetSize() == 4
    comps = dt.GetComponents()
    assert len(comps) == 2
    assert comps[0].GetName() == "a"
    assert comps[0].GetOffset() == 0
    assert comps[0].GetType().GetNumericDataType() == gdal.GDT_UInt16
    assert comps[1].GetName() == "b"
    assert comps[1].GetOffset() == 2
    assert comps[1].GetType().GetNumericDataType() == gdal.GDT_UInt16

    assert ar["a"].Read() == array.array("H", [1000, 4000, 0])
    assert ar["b"].Read() == array.array("H", [3000, 5000, 0])

    j = gdal.MultiDimInfo(ds, detailed=True)
    assert j["arrays"]["compound_well_aligned"]["values"] == [
        {"a": 1000, "b": 3000},
        {"a": 4000, "b": 5000},
        {"a": 0, "b": 0},
    ]


def test_zarr_read_compound_not_aligned():

    filename = "data/zarr/compound_not_aligned.zarr"
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None

    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar

    dt = ar.GetDataType()
    assert dt.GetSize() == 6
    comps = dt.GetComponents()
    assert len(comps) == 3
    assert comps[0].GetName() == "a"
    assert comps[0].GetOffset() == 0
    assert comps[0].GetType().GetNumericDataType() == gdal.GDT_UInt16
    assert comps[1].GetName() == "b"
    assert comps[1].GetOffset() == 2
    assert comps[1].GetType().GetNumericDataType() == gdal.GDT_UInt8
    assert comps[2].GetName() == "c"
    assert comps[2].GetOffset() == 4
    assert comps[2].GetType().GetNumericDataType() == gdal.GDT_UInt16

    assert ar["a"].Read() == array.array("H", [1000, 4000, 0])
    assert ar["b"].Read() == array.array("B", [2, 4, 0])
    assert ar["c"].Read() == array.array("H", [3000, 5000, 0])

    j = gdal.MultiDimInfo(ds, detailed=True)
    assert j["arrays"]["compound_not_aligned"]["values"] == [
        {"a": 1000, "b": 2, "c": 3000},
        {"a": 4000, "b": 4, "c": 5000},
        {"a": 0, "b": 0, "c": 0},
    ]


def test_zarr_read_compound_complex():

    filename = "data/zarr/compound_complex.zarr"
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None

    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar

    is_64bit = sys.maxsize > 2**32

    dt = ar.GetDataType()
    assert dt.GetSize() == 24 if is_64bit else 16
    comps = dt.GetComponents()
    assert len(comps) == 4
    assert comps[0].GetName() == "a"
    assert comps[0].GetOffset() == 0
    assert comps[0].GetType().GetNumericDataType() == gdal.GDT_UInt8
    assert comps[1].GetName() == "b"
    assert comps[1].GetOffset() == 2
    assert comps[1].GetType().GetClass() == gdal.GEDTC_COMPOUND
    assert comps[1].GetType().GetSize() == 1 + 1 + 2 + 1 + 1  # last one is padding

    subcomps = comps[1].GetType().GetComponents()
    assert len(subcomps) == 4

    assert comps[2].GetName() == "c"
    assert comps[2].GetOffset() == 8
    assert comps[2].GetType().GetClass() == gdal.GEDTC_STRING
    assert comps[3].GetName() == "d"
    assert comps[3].GetOffset() == 16 if is_64bit else 12
    assert comps[3].GetType().GetNumericDataType() == gdal.GDT_Int8

    j = gdal.MultiDimInfo(ds, detailed=True)
    assert j["arrays"]["compound_complex"]["values"] == [
        {"a": 1, "b": {"b1": 2, "b2": 3, "b3": 1000, "b5": 4}, "c": "AAA", "d": -1},
        {
            "a": 2,
            "b": {"b1": 255, "b2": 254, "b3": 65534, "b5": 253},
            "c": "ZZ",
            "d": -2,
        },
    ]


def test_zarr_read_array_attributes():

    filename = "data/zarr/array_attrs.zarr"
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None

    j = gdal.MultiDimInfo(ds)
    assert j["arrays"]["array_attrs"]["attributes"] == {
        "bool": True,
        "double": 1.5,
        "doublearray": [1.5, 2.5],
        "int": 1,
        "intarray": [1, 2],
        "int64": 1234567890123,
        "int64array": [1234567890123, -1234567890123],
        "intdoublearray": [1, 2.5],
        "mixedstrintarray": ["foo", 1],
        "null": "",
        "obj": {},
        "str": "foo",
        "strarray": ["foo", "bar"],
    }


@pytest.mark.parametrize("crs_member", ["projjson", "wkt", "url"])
def test_zarr_read_crs(tmp_vsimem, crs_member):

    zarray = {
        "chunks": [2, 3],
        "compressor": None,
        "dtype": "!b1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [5, 4],
        "zarr_format": 2,
    }

    zattrs_all = {
        "_CRS": {
            "projjson": {
                "$schema": "https://proj.org/schemas/v0.2/projjson.schema.json",
                "type": "GeographicCRS",
                "name": "WGS 84",
                "datum_ensemble": {
                    "name": "World Geodetic System 1984 ensemble",
                    "members": [
                        {
                            "name": "World Geodetic System 1984 (Transit)",
                            "id": {"authority": "EPSG", "code": 1166},
                        },
                        {
                            "name": "World Geodetic System 1984 (G730)",
                            "id": {"authority": "EPSG", "code": 1152},
                        },
                        {
                            "name": "World Geodetic System 1984 (G873)",
                            "id": {"authority": "EPSG", "code": 1153},
                        },
                        {
                            "name": "World Geodetic System 1984 (G1150)",
                            "id": {"authority": "EPSG", "code": 1154},
                        },
                        {
                            "name": "World Geodetic System 1984 (G1674)",
                            "id": {"authority": "EPSG", "code": 1155},
                        },
                        {
                            "name": "World Geodetic System 1984 (G1762)",
                            "id": {"authority": "EPSG", "code": 1156},
                        },
                    ],
                    "ellipsoid": {
                        "name": "WGS 84",
                        "semi_major_axis": 6378137,
                        "inverse_flattening": 298.257223563,
                    },
                    "accuracy": "2.0",
                    "id": {"authority": "EPSG", "code": 6326},
                },
                "coordinate_system": {
                    "subtype": "ellipsoidal",
                    "axis": [
                        {
                            "name": "Geodetic latitude",
                            "abbreviation": "Lat",
                            "direction": "north",
                            "unit": "degree",
                        },
                        {
                            "name": "Geodetic longitude",
                            "abbreviation": "Lon",
                            "direction": "east",
                            "unit": "degree",
                        },
                    ],
                },
                "scope": "Horizontal component of 3D system.",
                "area": "World.",
                "bbox": {
                    "south_latitude": -90,
                    "west_longitude": -180,
                    "north_latitude": 90,
                    "east_longitude": 180,
                },
                "id": {"authority": "EPSG", "code": 4326},
            },
            "wkt": (
                'GEOGCRS["WGS 84",ENSEMBLE["World Geodetic System 1984 ensemble",'
                'MEMBER["World Geodetic System 1984 (Transit)"],'
                'MEMBER["World Geodetic System 1984 (G730)"],'
                'MEMBER["World Geodetic System 1984 (G873)"],'
                'MEMBER["World Geodetic System 1984 (G1150)"],'
                'MEMBER["World Geodetic System 1984 (G1674)"],'
                'MEMBER["World Geodetic System 1984 (G1762)"],'
                'ELLIPSOID["WGS 84",6378137,298.257223563,LENGTHUNIT["metre",1]],'
                "ENSEMBLEACCURACY[2.0]],"
                'PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433]],'
                "CS[ellipsoidal,2],"
                'AXIS["geodetic latitude (Lat)",north,ORDER[1],ANGLEUNIT["degree",0.0174532925199433]],'
                'AXIS["geodetic longitude (Lon)",east,ORDER[2],ANGLEUNIT["degree",0.0174532925199433]],'
                'USAGE[SCOPE["Horizontal component of 3D system."],AREA["World."],BBOX[-90,-180,90,180]],'
                'ID["EPSG",4326]]'
            ),
            "url": "http://www.opengis.net/def/crs/EPSG/0/4326",
        }
    }

    zattrs = {"_CRS": {crs_member: zattrs_all["_CRS"][crs_member]}}

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zarray", json.dumps(zarray))
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zattrs", json.dumps(zattrs))
    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    srs = ar.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == "4326"
    # Mapping is 1, 2 since the slowest varying axis in multidim
    # mode is the lines, which matches latitude as the first axis of the CRS.
    assert srs.GetDataAxisToSRSAxisMapping() == [1, 2]
    assert len(ar.GetAttributes()) == 0

    # Open as classic CRS
    ds = gdal.Open(tmp_vsimem / "test.zarr")
    srs = ds.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == "4326"
    # Inverted mapping in classic raster mode compared to multidim mode,
    # because the first "axis" in our data model is columns.
    assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]


@pytest.mark.parametrize("use_get_names", [True, False])
def test_zarr_read_group(use_get_names):

    filename = "data/zarr/group.zarr"
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None
    rg = ds.GetRootGroup()
    assert rg.GetName() == "/"
    assert rg.GetFullName() == "/"
    if use_get_names:
        assert rg.GetGroupNames() == ["foo"]
    assert len(rg.GetAttributes()) == 1
    assert rg.GetAttribute("key") is not None
    subgroup = rg.OpenGroup("foo")
    assert subgroup is not None
    assert rg.OpenGroup("not_existing") is None
    assert subgroup.GetName() == "foo"
    assert subgroup.GetFullName() == "/foo"
    assert len(rg.GetMDArrayNames()) == 0
    if use_get_names:
        assert subgroup.GetGroupNames() == ["bar"]
    assert subgroup.GetAttributes() == []
    subsubgroup = subgroup.OpenGroup("bar")
    assert subgroup.GetGroupNames() == ["bar"]
    assert subsubgroup.GetName() == "bar"
    assert subsubgroup.GetFullName() == "/foo/bar"
    if use_get_names:
        assert subsubgroup.GetMDArrayNames() == ["baz"]
    ar = subsubgroup.OpenMDArray("baz")
    assert subsubgroup.GetMDArrayNames() == ["baz"]
    assert ar is not None
    assert ar.Read() == array.array("i", [1])
    assert subsubgroup.OpenMDArray("not_existing") is None


def test_zarr_read_group_with_zmetadata():

    filename = "data/zarr/group_with_zmetadata.zarr"
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None
    rg = ds.GetRootGroup()
    assert rg.GetName() == "/"
    assert rg.GetFullName() == "/"
    assert rg.GetGroupNames() == ["foo"]
    assert len(rg.GetAttributes()) == 1
    assert rg.GetAttribute("key") is not None
    subgroup = rg.OpenGroup("foo")
    assert subgroup is not None
    assert rg.OpenGroup("not_existing") is None
    assert subgroup.GetName() == "foo"
    assert subgroup.GetFullName() == "/foo"
    assert len(rg.GetMDArrayNames()) == 0
    assert subgroup.GetGroupNames() == ["bar"]
    assert subgroup.GetAttributes() == []
    subsubgroup = subgroup.OpenGroup("bar")
    assert subgroup.GetGroupNames() == ["bar"]
    assert subsubgroup.GetName() == "bar"
    assert subsubgroup.GetFullName() == "/foo/bar"
    assert subsubgroup.GetMDArrayNames() == ["baz"]
    assert subsubgroup.GetAttribute("foo") is not None
    ar = subsubgroup.OpenMDArray("baz")
    assert subsubgroup.GetMDArrayNames() == ["baz"]
    assert ar is not None
    assert ar.Read() == array.array("i", [1])
    assert ar.GetAttribute("bar") is not None
    assert subsubgroup.OpenMDArray("not_existing") is None


@pytest.mark.parametrize(
    "use_zmetadata, filename",
    [
        (True, "data/zarr/array_dimensions.zarr"),
        (False, "data/zarr/array_dimensions.zarr"),
        (True, "data/zarr/array_dimensions_upper_level.zarr"),
        (False, "data/zarr/array_dimensions_upper_level.zarr"),
        (False, "data/zarr/array_dimensions_upper_level.zarr/subgroup/var"),
    ],
)
def test_zarr_read_ARRAY_DIMENSIONS(use_zmetadata, filename):

    ds = gdal.OpenEx(
        filename,
        gdal.OF_MULTIDIM_RASTER,
        open_options=["USE_ZMETADATA=" + str(use_zmetadata)],
    )
    assert ds is not None
    rg = ds.GetRootGroup()
    if filename != "data/zarr/array_dimensions_upper_level.zarr":
        ar = rg.OpenMDArray("var")
    else:
        ar = rg.OpenGroup("subgroup").OpenMDArray("var")
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetName() == "lat"
    assert dims[0].GetIndexingVariable() is not None
    assert dims[0].GetIndexingVariable().GetName() == "lat"
    assert dims[0].GetType() == gdal.DIM_TYPE_HORIZONTAL_Y
    assert dims[0].GetDirection() == "NORTH"
    assert dims[1].GetName() == "lon"
    assert dims[1].GetIndexingVariable() is not None
    assert dims[1].GetIndexingVariable().GetName() == "lon"
    assert dims[1].GetType() == gdal.DIM_TYPE_HORIZONTAL_X
    assert dims[1].GetDirection() == "EAST"
    assert len(rg.GetDimensions()) == 2

    ds = gdal.OpenEx(
        filename,
        gdal.OF_MULTIDIM_RASTER,
        open_options=["USE_ZMETADATA=" + str(use_zmetadata)],
    )
    assert ds is not None
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("lat")
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == 1
    assert dims[0].GetName() == "lat"
    assert dims[0].GetIndexingVariable() is not None
    assert dims[0].GetIndexingVariable().GetName() == "lat"
    assert dims[0].GetType() == gdal.DIM_TYPE_HORIZONTAL_Y
    assert len(rg.GetDimensions()) == 2

    ds = gdal.OpenEx(
        filename,
        gdal.OF_MULTIDIM_RASTER,
        open_options=["USE_ZMETADATA=" + str(use_zmetadata)],
    )
    assert ds is not None
    rg = ds.GetRootGroup()
    assert len(rg.GetDimensions()) == 2


@pytest.mark.parametrize(
    "ds_name", ["data/zarr/v3/test_deprecated_no_codecs.zr3", "data/zarr/v3/test.zr3"]
)
@pytest.mark.parametrize("use_get_names", [True, False])
def test_zarr_read_v3(ds_name, use_get_names):

    ds = gdal.OpenEx(ds_name, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None
    rg = ds.GetRootGroup()
    assert rg.GetName() == "/"
    assert rg.GetFullName() == "/"
    if use_get_names:
        assert rg.GetGroupNames() == ["marvin"]
    assert len(rg.GetAttributes()) == 1
    assert rg.GetAttribute("root_foo") is not None
    subgroup = rg.OpenGroup("marvin")
    assert subgroup is not None
    assert rg.OpenGroup("not_existing") is None
    assert subgroup.GetName() == "marvin"
    assert subgroup.GetFullName() == "/marvin"
    if use_get_names:
        assert rg.GetMDArrayNames() == ["ar"]

    ar = rg.OpenMDArray("ar")
    assert ar
    assert ar.Read() == array.array("b", [1, 2])

    if use_get_names:
        assert subgroup.GetGroupNames() == ["paranoid"]
    assert len(subgroup.GetAttributes()) == 1

    subsubgroup = subgroup.OpenGroup("paranoid")
    assert subsubgroup.GetName() == "paranoid"
    assert subsubgroup.GetFullName() == "/marvin/paranoid"

    if use_get_names:
        assert subgroup.GetMDArrayNames() == ["android"]
    ar = subgroup.OpenMDArray("android")
    assert ar is not None
    assert ar.Read() == array.array("b", [1] * 4 * 5)
    assert subgroup.OpenMDArray("not_existing") is None


@pytest.mark.parametrize("endianness", ["le", "be"])
def test_zarr_read_half_float(endianness):

    filename = "data/zarr/f2_" + endianness + ".zarr"
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar.Read() == array.array("f", [1.5, float("nan")])


def test_zarr_read_mdim_zarr_non_existing():

    with gdal.quiet_errors():
        assert (
            gdal.OpenEx('ZARR:"data/zarr/not_existing.zarr"', gdal.OF_MULTIDIM_RASTER)
            is None
        )

    with gdal.quiet_errors():
        assert (
            gdal.OpenEx(
                'ZARR:"https://example.org/not_existing.zarr"', gdal.OF_MULTIDIM_RASTER
            )
            is None
        )
    assert (
        "The filename should likely be prefixed with /vsicurl/"
        in gdal.GetLastErrorMsg()
    )

    with gdal.quiet_errors():
        assert (
            gdal.OpenEx(
                "ZARR:https://example.org/not_existing.zarr", gdal.OF_MULTIDIM_RASTER
            )
            is None
        )
    assert (
        "There is likely a quoting error of the whole connection string, and the filename should likely be prefixed with /vsicurl/"
        in gdal.GetLastErrorMsg()
    )

    with gdal.quiet_errors():
        assert (
            gdal.OpenEx(
                "ZARR:/vsicurl/https://example.org/not_existing.zarr",
                gdal.OF_MULTIDIM_RASTER,
            )
            is None
        )
    assert (
        "There is likely a quoting error of the whole connection string."
        in gdal.GetLastErrorMsg()
    )


def test_zarr_read_classic():

    ds = gdal.Open("data/zarr/zlib.zarr")
    assert ds
    assert not ds.GetSubDatasets()
    assert ds.ReadRaster() == array.array("b", [1, 2])

    ds = gdal.Open("ZARR:data/zarr/zlib.zarr")
    assert ds
    assert not ds.GetSubDatasets()
    assert ds.ReadRaster() == array.array("b", [1, 2])

    with gdal.quiet_errors():
        assert gdal.Open('ZARR:"data/zarr/not_existing.zarr"') is None
        assert gdal.Open('ZARR:"data/zarr/zlib.zarr":/not_existing') is None
        assert gdal.Open('ZARR:"data/zarr/zlib.zarr":/zlib:0') is None

    ds = gdal.Open('ZARR:"data/zarr/zlib.zarr":/zlib')
    assert ds
    assert not ds.GetSubDatasets()
    assert ds.ReadRaster() == array.array("b", [1, 2])

    ds = gdal.OpenEx("data/zarr/order_f_u1_3d.zarr", open_options=["MULTIBAND=NO"])
    assert ds
    subds = ds.GetSubDatasets()
    assert len(subds) == 2
    ds = gdal.Open(subds[0][0])
    assert ds
    assert ds.ReadRaster() == array.array("b", [i for i in range(12)])
    ds = gdal.Open(subds[1][0])
    assert ds
    assert ds.ReadRaster() == array.array("b", [12 + i for i in range(12)])

    with gdal.quiet_errors():
        assert (
            gdal.OpenEx(
                "ZARR:data/zarr/order_f_u1_3d.zarr:/order_f_u1_3d",
                open_options=["MULTIBAND=NO"],
            )
            is None
        )
        assert gdal.Open("ZARR:data/zarr/order_f_u1_3d.zarr:/order_f_u1_3d:2") is None
        assert gdal.Open(subds[0][0] + ":0") is None

    ds = gdal.OpenEx("data/zarr/v3/test.zr3", open_options=["LIST_ALL_ARRAYS=YES"])
    assert ds
    subds = ds.GetSubDatasets()
    assert set(subds) == set(
        [
            ('ZARR:"data/zarr/v3/test.zr3":/ar', "[2] /ar (Byte)"),
            (
                'ZARR:"data/zarr/v3/test.zr3":/marvin/android',
                "[5x4] /marvin/android (Byte)",
            ),
        ]
    )
    ds = gdal.Open('ZARR:"data/zarr/v3/test.zr3":/ar')
    assert ds
    assert ds.ReadRaster() == array.array("b", [1, 2])

    ds = gdal.OpenEx("data/zarr/v3/test.zr3")
    assert ds
    subds = ds.GetSubDatasets()
    assert set(subds) == set(
        [
            (
                'ZARR:"data/zarr/v3/test.zr3":/marvin/android',
                "[5x4] /marvin/android (Byte)",
            ),
        ]
    )


def test_zarr_read_classic_2d(tmp_vsimem):

    src_ds = gdal.Open("data/byte.tif")
    gdal.GetDriverByName("ZARR").CreateCopy(
        tmp_vsimem / "test.zarr", src_ds, strict=False
    )
    ds = gdal.Open(tmp_vsimem / "test.zarr")
    assert ds is not None
    assert len(ds.GetSubDatasets()) == 0
    srs = ds.GetSpatialRef()
    assert srs.GetDataAxisToSRSAxisMapping() == [1, 2]
    ds = None


def test_zarr_read_classic_2d_with_unrelated_auxiliary_1D_arrays(tmp_vsimem):
    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr"
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg
        assert rg.GetName() == "/"

        dim0 = rg.CreateDimension("dim0", None, None, 2)
        dim1 = rg.CreateDimension("dim1", None, None, 3)

        dt = gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        rg.CreateMDArray("main_array", [dim0, dim1], dt)
        rg.CreateMDArray("x", [dim0], dt)
        rg.CreateMDArray("y", [dim1], dt)

    create()

    ds = gdal.Open(tmp_vsimem / "test.zarr")
    assert ds is not None
    assert ds.RasterYSize == 2
    assert ds.RasterXSize == 3
    assert set(ds.GetSubDatasets()) == set(
        [(f'ZARR:"{tmp_vsimem}/test.zarr":/main_array', "[2x3] /main_array (Float64)")]
    )
    ds = None

    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", open_options=["LIST_ALL_ARRAYS=YES"])
    assert set(ds.GetSubDatasets()) == set(
        [
            (
                f'ZARR:"{tmp_vsimem}/test.zarr":/main_array',
                "[2x3] /main_array (Float64)",
            ),
            (f'ZARR:"{tmp_vsimem}/test.zarr":/x', "[2] /x (Float64)"),
            (f'ZARR:"{tmp_vsimem}/test.zarr":/y', "[3] /y (Float64)"),
        ]
    )
    ds = None


def test_zarr_read_classic_3d_multiband():

    ds = gdal.OpenEx("data/zarr/order_f_u1_3d.zarr", open_options=["MULTIBAND=YES"])
    assert ds.RasterXSize == 4
    assert ds.RasterYSize == 3
    assert ds.RasterCount == 2
    assert not ds.GetSubDatasets()
    assert ds.GetRasterBand(1).ReadRaster() == array.array("b", [i for i in range(12)])
    assert ds.GetRasterBand(2).ReadRaster() == array.array(
        "b", [12 + i for i in range(12)]
    )

    ds = gdal.OpenEx(
        "data/zarr/order_f_u1_3d.zarr",
        open_options=["MULTIBAND=YES", "DIM_X=dim1", "DIM_Y=dim2"],
    )
    assert ds.RasterXSize == 3
    assert ds.RasterYSize == 4
    assert ds.RasterCount == 2
    assert not ds.GetSubDatasets()
    assert ds.GetRasterBand(1).ReadRaster() == array.array(
        "b", [0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11]
    )
    assert ds.GetRasterBand(2).ReadRaster() == array.array(
        "b", [(x + 12) for x in [0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11]]
    )

    ds = gdal.OpenEx(
        "data/zarr/order_f_u1_3d.zarr",
        open_options=["MULTIBAND=YES", "DIM_X=1", "DIM_Y=2"],
    )
    assert ds.RasterXSize == 3
    assert ds.RasterYSize == 4
    assert ds.RasterCount == 2
    assert not ds.GetSubDatasets()
    assert ds.GetRasterBand(1).ReadRaster() == array.array(
        "b", [0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11]
    )
    assert ds.GetRasterBand(2).ReadRaster() == array.array(
        "b", [(x + 12) for x in [0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11]]
    )

    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdal.OpenEx(
            "data/zarr/order_f_u1_3d.zarr", open_options=["MULTIBAND=YES", "DIM_X=3"]
        )
    assert gdal.GetLastErrorMsg() != ""

    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdal.OpenEx(
            "data/zarr/order_f_u1_3d.zarr", open_options=["MULTIBAND=YES", "DIM_Y=3"]
        )
    assert gdal.GetLastErrorMsg() != ""

    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdal.OpenEx(
            "data/zarr/order_f_u1_3d.zarr",
            open_options=["MULTIBAND=YES", "DIM_X=not_found"],
        )
    assert gdal.GetLastErrorMsg() != ""

    gdal.ErrorReset()
    with gdaltest.error_handler():
        gdal.OpenEx(
            "data/zarr/order_f_u1_3d.zarr",
            open_options=["MULTIBAND=YES", "DIM_Y=not_found"],
        )
    assert gdal.GetLastErrorMsg() != ""


def test_zarr_read_classic_too_many_samples_3d(tmp_vsimem):

    j = {
        "chunks": [65537, 2, 1],
        "compressor": None,
        "dtype": "!u1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [65537, 2, 1],
        "zarr_format": 2,
    }

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zarray", json.dumps(j))
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", open_options=["MULTIBAND=NO"])
    assert gdal.GetLastErrorMsg() != ""
    assert len(ds.GetSubDatasets()) == 0

    gdal.ErrorReset()
    with gdaltest.error_handler():
        assert (
            gdal.OpenEx(tmp_vsimem / "test.zarr", open_options=["MULTIBAND=YES"])
            is None
        )
    assert gdal.GetLastErrorMsg() != ""


@pytest.mark.parametrize("interleave", ["BAND", "PIXEL"])
def test_zarr_write_single_array_3d(tmp_vsimem, interleave):

    src_ds = gdal.Open("data/rgbsmall.tif")
    gdal.GetDriverByName("ZARR").CreateCopy(
        tmp_vsimem / "test.zarr", src_ds, options=["INTERLEAVE=" + interleave]
    )
    ds = gdal.Open(tmp_vsimem / "test.zarr")
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)] == [
        src_ds.GetRasterBand(i + 1).Checksum() for i in range(src_ds.RasterCount)
    ]
    assert [
        ds.GetRasterBand(i + 1).GetColorInterpretation() for i in range(ds.RasterCount)
    ] == [gdal.GCI_RedBand, gdal.GCI_GreenBand, gdal.GCI_BlueBand]


def test_zarr_read_classic_4d(tmp_vsimem):

    j = {
        "chunks": [3, 2, 1, 1],
        "compressor": None,
        "dtype": "!u1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [3, 2, 1, 1],
        "zarr_format": 2,
    }

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zarray", json.dumps(j))
    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", open_options=["MULTIBAND=NO"])
    subds = ds.GetSubDatasets()
    assert len(subds) == 6
    for i in range(len(subds)):
        assert gdal.Open(subds[i][0]) is not None


def test_zarr_read_classic_too_many_samples_4d(tmp_vsimem):

    j = {
        "chunks": [257, 256, 1, 1],
        "compressor": None,
        "dtype": "!u1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [257, 256, 1, 1],
        "zarr_format": 2,
    }

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zarray", json.dumps(j))
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", open_options=["MULTIBAND=NO"])
    assert gdal.GetLastErrorMsg() != ""
    assert len(ds.GetSubDatasets()) == 0


def test_zarr_read_empty_shape():

    ds = gdal.OpenEx("data/zarr/empty.zarr", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read() == array.array("b", [120])


def test_zarr_read_BLOSC_COMPRESSORS():

    if "blosc" not in gdal.GetDriverByName("Zarr").GetMetadataItem("COMPRESSORS"):
        pytest.skip("blosc not available")
    assert "lz4" in gdal.GetDriverByName("Zarr").GetMetadataItem("BLOSC_COMPRESSORS")


@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
@pytest.mark.parametrize("create_consolidated_metadata", ["YES", "NO"])
def test_zarr_create_group(tmp_path, format, create_consolidated_metadata):

    filename = tmp_path / "test.zarr"

    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            filename,
            options=[
                "FORMAT=" + format,
                "CREATE_CONSOLIDATED_METADATA=" + create_consolidated_metadata,
            ],
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg
        assert rg.GetName() == "/"

        attr = rg.CreateAttribute("str_attr", [], gdal.ExtendedDataType.CreateString())
        assert attr
        assert attr.GetFullName() == "/_GLOBAL_/str_attr"
        assert attr.Write("my_string") == gdal.CE_None

        attr = rg.CreateAttribute(
            "json_attr", [], gdal.ExtendedDataType.CreateString(0, gdal.GEDTST_JSON)
        )
        assert attr
        assert attr.Write({"foo": "bar"}) == gdal.CE_None

        attr = rg.CreateAttribute(
            "str_array_attr", [2], gdal.ExtendedDataType.CreateString()
        )
        assert attr
        assert attr.Write(["first_string", "second_string"]) == gdal.CE_None

        with gdal.quiet_errors():
            attr = rg.CreateAttribute(
                "dim_2_not_supported", [2, 2], gdal.ExtendedDataType.CreateString()
            )
            assert attr is None

        attr = rg.CreateAttribute(
            "int_attr", [], gdal.ExtendedDataType.Create(gdal.GDT_Int32)
        )
        assert attr
        assert attr.Write(12345678) == gdal.CE_None

        attr = rg.CreateAttribute(
            "uint_attr", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt32)
        )
        assert attr
        assert attr.Write(4000000000) == gdal.CE_None

        attr = rg.CreateAttribute(
            "int64_attr", [], gdal.ExtendedDataType.Create(gdal.GDT_Int64)
        )
        assert attr
        assert attr.Write(12345678901234) == gdal.CE_None

        attr = rg.CreateAttribute(
            "uint64_attr", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt64)
        )
        assert attr
        # We cannot write UINT64_MAX
        # assert attr.Write(18000000000000000000) == gdal.CE_None
        assert attr.Write(9000000000000000000) == gdal.CE_None

        attr = rg.CreateAttribute(
            "int_array_attr", [2], gdal.ExtendedDataType.Create(gdal.GDT_Int32)
        )
        assert attr
        assert attr.Write([12345678, -12345678]) == gdal.CE_None

        attr = rg.CreateAttribute(
            "uint_array_attr", [2], gdal.ExtendedDataType.Create(gdal.GDT_UInt32)
        )
        assert attr
        assert attr.Write([12345678, 4000000000]) == gdal.CE_None

        attr = rg.CreateAttribute(
            "int64_array_attr", [2], gdal.ExtendedDataType.Create(gdal.GDT_Int64)
        )
        assert attr
        assert attr.Write([12345678091234, -12345678091234]) == gdal.CE_None

        attr = rg.CreateAttribute(
            "uint64_array_attr", [2], gdal.ExtendedDataType.Create(gdal.GDT_UInt64)
        )
        assert attr
        # We cannot write UINT64_MAX
        # assert attr.Write([12345678091234, 18000000000000000000]) == gdal.CE_None
        assert attr.Write([12345678091234, 9000000000000000000]) == gdal.CE_None

        attr = rg.CreateAttribute(
            "double_attr", [], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        )
        assert attr
        assert attr.Write(12345678.5) == gdal.CE_None

        attr = rg.CreateAttribute(
            "double_array_attr", [2], gdal.ExtendedDataType.Create(gdal.GDT_Float64)
        )
        assert attr
        assert attr.Write([12345678.5, -12345678.5]) == gdal.CE_None

        subgroup = rg.CreateGroup("foo")
        assert subgroup
        assert subgroup.GetName() == "foo"
        assert subgroup.GetFullName() == "/foo"
        assert rg.GetGroupNames() == ["foo"]
        subgroup = rg.OpenGroup("foo")
        assert subgroup

    create()

    if create_consolidated_metadata == "YES":
        with gdal.VSIFile(
            filename / ".zmetadata" if format == "ZARR_V2" else filename / "zarr.json",
            "rb",
        ) as f:
            data = f.read()
        j = json.loads(data)
        if format == "ZARR_V2":
            assert "foo/.zgroup" in j["metadata"]
            assert ".zattrs" in j["metadata"]
            assert j["metadata"][".zattrs"]["int_attr"] == 12345678
        else:
            assert j["consolidated_metadata"] == {
                "kind": "inline",
                "must_understand": False,
                "metadata": {
                    "foo": {"zarr_format": 3, "node_type": "group", "attributes": {}}
                },
            }
    else:
        if format == "ZARR_V2":
            assert gdal.VSIStatL(filename / ".zmetadata") is None
        else:
            with gdal.VSIFile(filename / "zarr.json", "rb") as f:
                data = f.read()
            j = json.loads(data)
            assert "consolidated_metadata" not in j

    def update():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        assert rg.GetGroupNames() == ["foo"]

        attr = rg.GetAttribute("str_attr")
        assert attr
        assert attr.Read() == "my_string"
        assert attr.Write("my_string_modified") == gdal.CE_None

        subgroup = rg.OpenGroup("foo")
        assert subgroup
        subgroup = rg.CreateGroup("bar")
        assert subgroup
        assert set(rg.GetGroupNames()) == set(["foo", "bar"])
        subgroup = rg.OpenGroup("foo")
        assert subgroup
        subsubgroup = subgroup.CreateGroup("baz")
        assert subsubgroup
        ds = None

    update()

    def check():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        assert rg

        attr = rg.GetAttribute("str_attr")
        assert attr
        assert attr.Read() == "my_string_modified"

        attr = rg.GetAttribute("json_attr")
        assert attr
        assert attr.GetDataType().GetSubType() == gdal.GEDTST_JSON
        assert attr.Read() == {"foo": "bar"}

        attr = rg.GetAttribute("str_array_attr")
        assert attr
        assert attr.Read() == ["first_string", "second_string"]

        attr = rg.GetAttribute("int_attr")
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Int32
        assert attr.ReadAsInt() == 12345678
        assert attr.ReadAsInt64() == 12345678
        assert attr.ReadAsDouble() == 12345678

        attr = rg.GetAttribute("uint_attr")
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Int64
        assert attr.ReadAsInt64() == 4000000000
        assert attr.ReadAsDouble() == 4000000000

        attr = rg.GetAttribute("int64_attr")
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Int64
        assert attr.ReadAsInt64() == 12345678901234
        assert attr.ReadAsDouble() == 12345678901234

        attr = rg.GetAttribute("uint64_attr")
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Int64
        assert attr.ReadAsInt64() == 9000000000000000000
        assert attr.ReadAsDouble() == 9000000000000000000

        attr = rg.GetAttribute("int_array_attr")
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Int32
        assert attr.ReadAsIntArray() == (12345678, -12345678)
        assert attr.ReadAsInt64Array() == (12345678, -12345678)
        assert attr.ReadAsDoubleArray() == (12345678, -12345678)

        attr = rg.GetAttribute("uint_array_attr")
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Int64
        assert attr.ReadAsInt64Array() == (12345678, 4000000000)
        assert attr.ReadAsDoubleArray() == (12345678, 4000000000)

        attr = rg.GetAttribute("int64_array_attr")
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Int64
        assert attr.ReadAsInt64Array() == (12345678091234, -12345678091234)
        assert attr.ReadAsDoubleArray() == (12345678091234, -12345678091234)

        attr = rg.GetAttribute("uint64_array_attr")
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Int64
        assert attr.ReadAsInt64Array() == (12345678091234, 9000000000000000000)
        assert attr.ReadAsDoubleArray() == (12345678091234, 9000000000000000000)

        attr = rg.GetAttribute("double_attr")
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Float64
        assert attr.ReadAsDouble() == 12345678.5

        attr = rg.GetAttribute("double_array_attr")
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Float64
        assert attr.Read() == (12345678.5, -12345678.5)

        assert set(rg.GetGroupNames()) == set(["foo", "bar"])
        with gdal.quiet_errors():
            assert rg.CreateGroup("not_opened_in_update_mode") is None
            assert (
                rg.CreateAttribute(
                    "not_opened_in_update_mode",
                    [],
                    gdal.ExtendedDataType.CreateString(),
                )
                is None
            )
        subgroup = rg.OpenGroup("foo")
        assert subgroup
        subsubgroup = subgroup.OpenGroup("baz")
        assert subsubgroup
        ds = None

    check()

    # With consolidated metadata, check that we don't try to access subfiles
    if create_consolidated_metadata == "YES":
        if format == "ZARR_V2":
            gdal.Unlink(filename / "foo" / ".zarray")
            gdal.Unlink(filename / ".zattrs")
        else:
            gdal.Unlink(filename / "foo" / "zarr.json")

        check()


@pytest.mark.parametrize(
    "group_name",
    [
        "foo",  # already existing
        "directory_with_that_name",
        "",
        ".",
        "..",
        "a/b",
        "a\\n",
        "a:b",
        ".zarray",
    ],
)
@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_create_group_errors(tmp_vsimem, group_name, format):
    def at_creation():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr", options=["FORMAT=" + format]
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg
        subgroup = rg.CreateGroup("foo")
        assert subgroup
        gdal.Mkdir(tmp_vsimem / "test.zarr/directory_with_that_name", 0)
        with gdal.quiet_errors():
            assert rg.CreateGroup(group_name) is None

    at_creation()

    def after_reopen():
        ds = gdal.OpenEx(
            tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE
        )
        rg = ds.GetRootGroup()
        gdal.Mkdir(tmp_vsimem / "test.zarr/directory_with_that_name", 0)
        with gdal.quiet_errors():
            assert rg.CreateGroup(group_name) is None

    after_reopen()


def getCompoundDT():
    x = gdal.EDTComponent.Create("x", 0, gdal.ExtendedDataType.Create(gdal.GDT_Int16))
    y = gdal.EDTComponent.Create("y", 0, gdal.ExtendedDataType.Create(gdal.GDT_Int32))
    subcompound = gdal.ExtendedDataType.CreateCompound("", 4, [y])
    subcompound_component = gdal.EDTComponent.Create("y", 4, subcompound)
    return gdal.ExtendedDataType.CreateCompound("", 8, [x, subcompound_component])


@pytest.mark.parametrize(
    "datatype,nodata",
    [
        [gdal.ExtendedDataType.Create(gdal.GDT_UInt8), None],
        [gdal.ExtendedDataType.Create(gdal.GDT_UInt8), 1],
        [gdal.ExtendedDataType.Create(gdal.GDT_UInt16), None],
        [gdal.ExtendedDataType.Create(gdal.GDT_Int16), None],
        [gdal.ExtendedDataType.Create(gdal.GDT_UInt32), None],
        [gdal.ExtendedDataType.Create(gdal.GDT_Int32), None],
        [gdal.ExtendedDataType.Create(gdal.GDT_Float16), None],
        [gdal.ExtendedDataType.Create(gdal.GDT_Float32), None],
        [gdal.ExtendedDataType.Create(gdal.GDT_Float64), None],
        [gdal.ExtendedDataType.Create(gdal.GDT_Float64), 1.5],
        [gdal.ExtendedDataType.Create(gdal.GDT_Float64), float("nan")],
        [gdal.ExtendedDataType.Create(gdal.GDT_Float64), float("infinity")],
        [gdal.ExtendedDataType.Create(gdal.GDT_Float64), float("-infinity")],
        [gdal.ExtendedDataType.Create(gdal.GDT_CInt16), None],
        [gdal.ExtendedDataType.Create(gdal.GDT_CInt32), None],
        # CFloat16 is not yet supported in Python
        # [gdal.ExtendedDataType.Create(gdal.GDT_CFloat16), None],
        [gdal.ExtendedDataType.Create(gdal.GDT_CFloat32), None],
        [gdal.ExtendedDataType.Create(gdal.GDT_CFloat64), None],
        [gdal.ExtendedDataType.CreateString(10), None],
        [gdal.ExtendedDataType.CreateString(10), "ab"],
        [getCompoundDT(), None],
        [
            getCompoundDT(),
            bytes(array.array("h", [12]))
            + bytes(array.array("h", [0]))
            + bytes(array.array("i", [2345678])),  # padding
        ],
    ],
)
@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_create_array(tmp_vsimem, datatype, nodata, format):

    error_expected = False
    if datatype.GetNumericDataType() in (gdal.GDT_CInt16, gdal.GDT_CInt32):
        error_expected = True
    elif format == "ZARR_V3" and datatype.GetClass() != gdal.GEDTC_NUMERIC:
        error_expected = True

    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr", options=["FORMAT=" + format]
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg
        assert rg.GetName() == "/"

        dim0 = rg.CreateDimension("dim0", None, None, 2)
        dim1 = rg.CreateDimension("dim1", None, None, 3)

        if error_expected:
            with gdal.quiet_errors():
                ar = rg.CreateMDArray("my_ar", [dim0, dim1], datatype)
            assert ar is None
            return False
        else:
            ar = rg.CreateMDArray("my_ar", [dim0, dim1], datatype)
            assert ar
            if nodata:
                if datatype.GetClass() == gdal.GEDTC_STRING:
                    assert ar.SetNoDataValueString(nodata) == gdal.CE_None
                elif datatype.GetClass() == gdal.GEDTC_NUMERIC:
                    assert ar.SetNoDataValueDouble(nodata) == gdal.CE_None
                else:
                    assert ar.SetNoDataValueRaw(nodata) == gdal.CE_None
            return True

    if create():
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray("my_ar")
        assert ar
        got_dt = ar.GetDataType()
        if got_dt.GetClass() == gdal.GEDTC_COMPOUND:
            comps = got_dt.GetComponents()
            assert len(comps) == 2
            assert comps[1].GetType().GetClass() == gdal.GEDTC_COMPOUND
            comps[1] = gdal.EDTComponent.Create(
                comps[1].GetName(),
                comps[1].GetType().GetSize(),
                gdal.ExtendedDataType.CreateCompound(
                    "",
                    comps[1].GetType().GetSize(),
                    comps[1].GetType().GetComponents(),
                ),
            )
            got_dt = gdal.ExtendedDataType.CreateCompound("", got_dt.GetSize(), comps)
        assert got_dt == datatype
        assert len(ar.GetDimensions()) == 2
        assert [ar.GetDimensions()[i].GetSize() for i in range(2)] == [2, 3]
        if nodata:
            if datatype.GetClass() == gdal.GEDTC_STRING:
                got_nodata = ar.GetNoDataValueAsString()
                assert got_nodata == nodata
            elif datatype.GetClass() == gdal.GEDTC_NUMERIC:
                got_nodata = ar.GetNoDataValueAsDouble()
                if math.isnan(nodata):
                    assert math.isnan(got_nodata)
                else:
                    assert got_nodata == nodata
            else:
                got_nodata = ar.GetNoDataValueAsRaw()
                assert got_nodata == nodata
        else:
            if format == "ZARR_V3":
                assert ar.GetNoDataValueAsRaw() is None or math.isnan(
                    ar.GetNoDataValueAsDouble()
                )
            else:
                assert ar.GetNoDataValueAsRaw() is None


@pytest.mark.parametrize(
    "array_name",
    [
        "foo",  # already existing
        "directory_with_that_name",
        "",
        ".",
        "..",
        "a/b",
        "a\\n",
        "a:b",
        ".zarray",
    ],
)
@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_create_array_errors(tmp_vsimem, array_name, format):
    def at_creation():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr", options=["FORMAT=" + format]
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg
        assert (
            rg.CreateMDArray("foo", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8))
            is not None
        )
        gdal.Mkdir(tmp_vsimem / "test.zarr/directory_with_that_name", 0)
        with gdal.quiet_errors():
            assert (
                rg.CreateMDArray(
                    array_name, [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
                )
                is None
            )

    at_creation()

    def after_reopen():
        ds = gdal.OpenEx(
            tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE
        )
        rg = ds.GetRootGroup()
        gdal.Mkdir(tmp_vsimem / "test.zarr/directory_with_that_name", 0)
        with gdal.quiet_errors():
            assert (
                rg.CreateMDArray(
                    array_name, [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
                )
                is None
            )

    after_reopen()


@pytest.mark.parametrize(
    "compressor,options,expected_json",
    [
        ["NONE", [], None],
        ["zlib", [], {"id": "zlib", "level": 6}],
        ["zlib", ["ZLIB_LEVEL=1"], {"id": "zlib", "level": 1}],
        [
            "blosc",
            [],
            {"blocksize": 0, "clevel": 5, "cname": "lz4", "id": "blosc", "shuffle": 1},
        ],
    ],
)
def test_zarr_create_array_compressor(tmp_vsimem, compressor, options, expected_json):

    compressors = gdal.GetDriverByName("Zarr").GetMetadataItem("COMPRESSORS")
    if compressor != "NONE" and compressor not in compressors:
        pytest.skip("compressor %s not available" % compressor)

    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr"
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg
        assert (
            rg.CreateMDArray(
                "test",
                [],
                gdal.ExtendedDataType.Create(gdal.GDT_UInt8),
                ["COMPRESS=" + compressor] + options,
            )
            is not None
        )

    create()

    f = gdal.VSIFOpenL(tmp_vsimem / "test.zarr/test/.zarray", "rb")
    assert f
    data = gdal.VSIFReadL(1, 1000, f)
    gdal.VSIFCloseL(f)
    j = json.loads(data)
    assert j["compressor"] == expected_json


@pytest.mark.parametrize(
    "compressor,options,expected_json",
    [
        ["NONE", [], [{"name": "bytes", "configuration": {"endian": "little"}}]],
        [
            "gzip",
            [],
            [
                {"name": "bytes", "configuration": {"endian": "little"}},
                {"name": "gzip", "configuration": {"level": 6}},
            ],
        ],
        [
            "gzip",
            ["GZIP_LEVEL=1"],
            [
                {"name": "bytes", "configuration": {"endian": "little"}},
                {"name": "gzip", "configuration": {"level": 1}},
            ],
        ],
        [
            "blosc",
            [],
            [
                {"name": "bytes", "configuration": {"endian": "little"}},
                {
                    "name": "blosc",
                    "configuration": {
                        "cname": "lz4",
                        "clevel": 5,
                        "shuffle": "shuffle",
                        "typesize": 1,
                        "blocksize": 0,
                    },
                },
            ],
        ],
        [
            "blosc",
            [
                "BLOSC_CNAME=zlib",
                "BLOSC_CLEVEL=1",
                "BLOSC_SHUFFLE=NONE",
                "BLOSC_BLOCKSIZE=2",
            ],
            [
                {"name": "bytes", "configuration": {"endian": "little"}},
                {
                    "name": "blosc",
                    "configuration": {
                        "cname": "zlib",
                        "clevel": 1,
                        "shuffle": "noshuffle",
                        "blocksize": 2,
                    },
                },
            ],
        ],
        [
            "zstd",
            ["ZSTD_LEVEL=20"],
            [
                {"name": "bytes", "configuration": {"endian": "little"}},
                {"name": "zstd", "configuration": {"level": 20, "checksum": False}},
            ],
        ],
        [
            "zstd",
            ["ZSTD_CHECKSUM=YES"],
            [
                {"name": "bytes", "configuration": {"endian": "little"}},
                {"name": "zstd", "configuration": {"level": 13, "checksum": True}},
            ],
        ],
    ],
)
def test_zarr_create_array_compressor_v3(
    tmp_vsimem, compressor, options, expected_json
):

    compressors = gdal.GetDriverByName("Zarr").GetMetadataItem("COMPRESSORS")
    if compressor != "NONE" and compressor not in compressors:
        pytest.skip("compressor %s not available" % compressor)

    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr", options=["FORMAT=ZARR_V3"]
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg
        dim = rg.CreateDimension("dim0", None, None, 2)
        ar = rg.CreateMDArray(
            "test",
            [dim],
            gdal.ExtendedDataType.Create(gdal.GDT_UInt8),
            ["COMPRESS=" + compressor] + options,
        )
        assert ar.Write(array.array("b", [1, 2])) == gdal.CE_None

    create()

    f = gdal.VSIFOpenL(tmp_vsimem / "test.zarr/test/zarr.json", "rb")
    assert f
    data = gdal.VSIFReadL(1, 1000, f)
    gdal.VSIFCloseL(f)
    j = json.loads(data)
    if expected_json is None:
        assert "codecs" not in j
    else:
        assert j["codecs"] == expected_json

    def read():
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("test")
        assert ar.Read() == array.array("b", [1, 2])

    read()


@pytest.mark.parametrize(
    "options,expected_json",
    [
        [
            ["@ENDIAN=little"],
            [{"configuration": {"endian": "little"}, "name": "bytes"}],
        ],
        [["@ENDIAN=big"], [{"configuration": {"endian": "big"}, "name": "bytes"}]],
        [
            ["@ENDIAN=little", "CHUNK_MEMORY_LAYOUT=F"],
            [
                {"name": "transpose", "configuration": {"order": [1, 0]}},
                {"configuration": {"endian": "little"}, "name": "bytes"},
            ],
        ],
        [
            ["@ENDIAN=big", "CHUNK_MEMORY_LAYOUT=F"],
            [
                {"name": "transpose", "configuration": {"order": [1, 0]}},
                {"configuration": {"endian": "big"}, "name": "bytes"},
            ],
        ],
        [
            ["@ENDIAN=big", "CHUNK_MEMORY_LAYOUT=F", "COMPRESS=GZIP"],
            [
                {"name": "transpose", "configuration": {"order": [1, 0]}},
                {"name": "bytes", "configuration": {"endian": "big"}},
                {"name": "gzip", "configuration": {"level": 6}},
            ],
        ],
    ],
)
@pytest.mark.parametrize(
    "gdal_data_type",
    [
        gdal.GDT_Int8,
        gdal.GDT_UInt8,
        gdal.GDT_Int16,
        gdal.GDT_UInt16,
        gdal.GDT_Int32,
        gdal.GDT_UInt32,
        gdal.GDT_Int64,
        gdal.GDT_UInt64,
        # Float16 not yet supported by SWIG
        # gdal.GDT_Float16,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
    ],
)
def test_zarr_create_array_endian_v3(
    tmp_vsimem, options, expected_json, gdal_data_type
):

    array_type = _gdal_data_type_to_array_type[gdal_data_type]

    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr", options=["FORMAT=ZARR_V3"]
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg
        dim0 = rg.CreateDimension("dim0", None, None, 2)
        dim1 = rg.CreateDimension("dim1", None, None, 1)
        ar = rg.CreateMDArray(
            "test", [dim0, dim1], gdal.ExtendedDataType.Create(gdal_data_type), options
        )
        assert ar.Write(array.array(array_type, [1, 2])) == gdal.CE_None

    create()

    f = gdal.VSIFOpenL(tmp_vsimem / "test.zarr/test/zarr.json", "rb")
    assert f
    data = gdal.VSIFReadL(1, 1000, f)
    gdal.VSIFCloseL(f)
    j = json.loads(data)
    assert j["codecs"] == expected_json

    def read():
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("test")
        assert ar.Read() == array.array(array_type, [1, 2])

    read()


@pytest.mark.parametrize(
    "j, error_msg",
    [
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "MISSING_shape": [1],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": 0,
            },
            "shape missing or not an array",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": "invalid",
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": 0,
            },
            "shape missing or not an array",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1, 2]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": 0,
            },
            "shape and chunks arrays are of different size",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "MISSING_data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": 0,
            },
            "data_type missing",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8_INVALID",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": 0,
            },
            "Invalid or unsupported format for data_type",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "MISSING_chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": 0,
            },
            "chunk_grid missing or not an object",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "chunk_grid": {"name": "invalid"},
                "chunk_key_encoding": {"name": "default"},
                "fill_value": 0,
            },
            "Only chunk_grid.name = regular supported",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "chunk_grid": {"name": "regular"},
                "chunk_key_encoding": {"name": "default"},
                "fill_value": 0,
            },
            "chunk_grid.configuration.chunk_shape missing or not an array",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "MISSING_chunk_key_encoding": {"name": "default"},
                "fill_value": 0,
            },
            "chunk_key_encoding missing or not an object",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {"name": "invalid"},
                "fill_value": 0,
            },
            "Unsupported chunk_key_encoding.name",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {
                    "name": "default",
                    "configuration": {"separator": "invalid"},
                },
                "fill_value": 0,
            },
            "Separator can only be '/' or '.'",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": 0,
                "storage_transformers": [{}],
            },
            "storage_transformers are not supported",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": "invalid",
            },
            "Invalid fill_value",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": "0",
                "dimension_names": "invalid",
            },
            "dimension_names should be an array",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": "0",
                "dimension_names": [],
            },
            "Size of dimension_names[] different from the one of shape",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": "NaN",
            },
            "Invalid fill_value for this data type",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": "0x00",
            },
            "Hexadecimal representation of fill_value no supported for this data type",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": "0b00",
            },
            "Binary representation of fill_value no supported for this data type",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1 << 40, 1 << 40],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1 << 40, 1 << 40]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": 0,
            },
            "Too large chunks",
        ],
        [
            {
                "zarr_format": 3,
                "node_type": "array",
                "shape": [1 << 30, 1 << 30, 1 << 30],
                "data_type": "uint8",
                "chunk_grid": {
                    "name": "regular",
                    "configuration": {"chunk_shape": [1, 1, 1]},
                },
                "chunk_key_encoding": {"name": "default"},
                "fill_value": 0,
            },
            "Array test has more than 2^64 blocks. This is not supported.",
        ],
    ],
)
def test_zarr_read_invalid_zarr_v3(tmp_vsimem, j, error_msg):

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/zarr.json", json.dumps(j))
    gdal.ErrorReset()
    with gdal.quiet_errors():
        assert gdal.Open(tmp_vsimem / "test.zarr") is None
    assert error_msg in gdal.GetLastErrorMsg()


def test_zarr_read_data_type_fallback_zarr_v3(tmp_vsimem):

    j = {
        "zarr_format": 3,
        "node_type": "array",
        "shape": [1],
        "data_type": {
            "name": "datetime",
            "configuration": {"unit": "ns"},
            "fallback": "int64",
        },
        "chunk_grid": {"name": "regular", "configuration": {"chunk_shape": [1]}},
        "chunk_key_encoding": {"name": "default"},
        "fill_value": 0,
    }

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/zarr.json", json.dumps(j))
    ds = gdal.Open(tmp_vsimem / "test.zarr")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int64


@pytest.mark.parametrize(
    "data_type,fill_value,nodata",
    [
        # JSON NoDataValues cannot be Float16
        # ("float16", "0x3e00", 1.5),
        # ("float16", str(bin(0x3E00)), 1.5),
        ("float32", "0x3fc00000", 1.5),
        ("float32", str(bin(0x3FC00000)), 1.5),
        ("float64", "0x3ff8000000000000", 1.5),
        ("float64", str(bin(0x3FF8000000000000)), 1.5),
    ],
)
def test_zarr_read_fill_value_v3(tmp_vsimem, data_type, fill_value, nodata):

    j = {
        "zarr_format": 3,
        "node_type": "array",
        "shape": [1],
        "data_type": data_type,
        "chunk_grid": {"name": "regular", "configuration": {"chunk_shape": [1]}},
        "chunk_key_encoding": {"name": "default"},
        "fill_value": fill_value,
    }

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/zarr.json", json.dumps(j))
    ds = gdal.Open(tmp_vsimem / "test.zarr")
    assert ds.GetRasterBand(1).GetNoDataValue() == nodata


@gdaltest.enable_exceptions()
# complex32 is not yet supported by Python
# @pytest.mark.parametrize("data_type", ["complex128", "complex64", "complex32"])
@pytest.mark.parametrize("data_type", ["complex128", "complex64"])
@pytest.mark.parametrize(
    "fill_value,nodata",
    [
        ([1, 2], [1, 2]),
        ([1.5, "NaN"], [1.5, float("nan")]),
        ([1234567890123, "Infinity"], [1234567890123, float("inf")]),
        ([1, "-Infinity"], [1, float("-inf")]),
        (["NaN", 2.5], [float("nan"), 2.5]),
        (["Infinity", 2], [float("inf"), 2]),
        (["-Infinity", 2], [float("-inf"), 2]),
        (["0x7ff8000000000000", 2], [float("nan"), 2]),
        # Invalid ones
        (1, None),
        ("NaN", None),
        ([], None),
        ([1, 2, 3], None),
        (["invalid", 1], None),
        ([1, "invalid"], None),
    ],
)
def test_zarr_read_fill_value_complex_datatype_v3(
    tmp_vsimem, data_type, fill_value, nodata
):

    import copy

    fill_value = copy.copy(fill_value)
    nodata = copy.copy(nodata)
    if fill_value and isinstance(fill_value, list):
        # float32 precision not sufficient to hold 1234567890123
        if data_type == "complex64" and fill_value[0] == 1234567890123:
            fill_value[0] = 123456
            nodata[0] = 123456
        # float16 precision not sufficient to hold 1234567890123
        if data_type == "complex32" and fill_value[0] == 1234567890123:
            fill_value[0] = 1234
            nodata[0] = 1234

        # convert float64 nan hexadecimal representation to float32
        if data_type == "complex64" and str(fill_value[0]) == "0x7ff8000000000000":
            fill_value[0] = "0x7fc00000"
        # convert float64 nan hexadecimal representation to float16
        if data_type == "complex32" and str(fill_value[0]) == "0x7ff8000000000000":
            fill_value[0] = "0x7e00"

    j = {
        "zarr_format": 3,
        "node_type": "array",
        "shape": [1],
        "data_type": data_type,
        "chunk_grid": {"name": "regular", "configuration": {"chunk_shape": [1]}},
        "chunk_key_encoding": {"name": "default"},
        "fill_value": fill_value,
    }

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/zarr.json", json.dumps(j))

    if nodata is None:
        with pytest.raises(Exception):
            gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
    else:

        def open_and_modify():
            ds = gdal.OpenEx(
                tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE
            )
            rg = ds.GetRootGroup()
            ar = rg.OpenMDArray("test")
            dtype = (
                "d"
                if ar.GetDataType().GetNumericDataType() == gdal.GDT_CFloat64
                else "f"
            )
            assert ar.GetNoDataValueAsRaw() == bytes(
                array.array(dtype, nodata)
            ), struct.unpack(dtype * 2, ar.GetNoDataValueAsRaw())

            # To force a reserialization of the array
            attr = ar.CreateAttribute("attr", [], gdal.ExtendedDataType.CreateString())
            attr.Write("foo")

        open_and_modify()

        if not str(fill_value[0]).startswith("0x"):
            f = gdal.VSIFOpenL(tmp_vsimem / "test.zarr/zarr.json", "rb")
            assert f
            data = gdal.VSIFReadL(1, 10000, f)
            gdal.VSIFCloseL(f)
            j = json.loads(data)
            assert j["fill_value"] == fill_value


@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_create_array_bad_compressor(tmp_vsimem, format):

    ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
        tmp_vsimem / "test.zarr", options=["FORMAT=" + format]
    )
    assert ds is not None
    rg = ds.GetRootGroup()
    assert rg
    with gdal.quiet_errors():
        assert (
            rg.CreateMDArray(
                "test",
                [],
                gdal.ExtendedDataType.Create(gdal.GDT_UInt8),
                ["COMPRESS=invalid"],
            )
            is None
        )


@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_create_array_attributes(tmp_vsimem, format):
    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr", options=["FORMAT=" + format]
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.CreateMDArray("test", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8))
        assert ar
        assert ar.GetFullName() == "/test"

        attr = ar.CreateAttribute("str_attr", [], gdal.ExtendedDataType.CreateString())
        assert attr
        assert attr.GetFullName() == "/test/str_attr"
        assert attr.Write("my_string") == gdal.CE_None

        with gdal.quiet_errors():
            assert (
                ar.CreateAttribute(
                    "invalid_2d", [2, 3], gdal.ExtendedDataType.CreateString()
                )
                is None
            )

    create()

    def update():
        ds = gdal.OpenEx(
            tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE
        )
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray("test")
        assert ar

        attr = ar.GetAttribute("str_attr")
        assert attr
        assert attr.Read() == "my_string"
        assert attr.Write("my_string_modified") == gdal.CE_None

    update()

    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray("test")
    assert ar

    attr = ar.GetAttribute("str_attr")
    assert attr
    assert attr.Read() == "my_string_modified"
    with gdal.quiet_errors():
        assert attr.Write("foo") == gdal.CE_Failure

    with gdal.quiet_errors():
        assert (
            ar.CreateAttribute("another_attr", [], gdal.ExtendedDataType.CreateString())
            is None
        )


def test_zarr_create_array_set_crs(tmp_vsimem):
    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr"
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.CreateMDArray("test", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8))
        assert ar
        crs = osr.SpatialReference()
        crs.ImportFromEPSG(4326)
        assert ar.SetSpatialRef(crs) == gdal.CE_None

    create()

    f = gdal.VSIFOpenL(tmp_vsimem / "test.zarr/test/.zattrs", "rb")
    assert f
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    j = json.loads(data)
    assert "_CRS" in j
    crs = j["_CRS"]
    assert "wkt" in crs
    assert "url" in crs
    if "projjson" in crs:
        assert crs["projjson"]["type"] == "GeographicCRS"


def test_zarr_create_array_set_dimension_name(tmp_vsimem):
    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr"
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg

        dim0 = rg.CreateDimension("dim0", None, None, 2)
        dim0_ar = rg.CreateMDArray(
            "dim0", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        dim0.SetIndexingVariable(dim0_ar)

        rg.CreateMDArray("test", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_UInt8))

    create()

    f = gdal.VSIFOpenL(tmp_vsimem / "test.zarr/test/.zattrs", "rb")
    assert f
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    j = json.loads(data)
    assert "_ARRAY_DIMENSIONS" in j
    assert j["_ARRAY_DIMENSIONS"] == ["dim0"]


@pytest.mark.parametrize(
    "dtype,gdaltype,fill_value,nodata_value",
    [
        ["!b1", gdal.GDT_UInt8, None, None],
        ["!i1", gdal.GDT_Int16, None, None],
        ["!i1", gdal.GDT_Int16, -1, -1],
        ["!u1", gdal.GDT_UInt8, None, None],
        ["!u1", gdal.GDT_UInt8, "1", 1],
        ["<i2", gdal.GDT_Int16, None, None],
        [">i2", gdal.GDT_Int16, None, None],
        ["<i4", gdal.GDT_Int32, None, None],
        [">i4", gdal.GDT_Int32, None, None],
        ["<i8", gdal.GDT_Float64, None, None],
        [">i8", gdal.GDT_Float64, None, None],
        ["<u2", gdal.GDT_UInt16, None, None],
        [">u2", gdal.GDT_UInt16, None, None],
        ["<u4", gdal.GDT_UInt32, None, None],
        [">u4", gdal.GDT_UInt32, None, None],
        ["<u4", gdal.GDT_UInt32, 4000000000, 4000000000],
        ["<u8", gdal.GDT_Float64, 4000000000, 4000000000],
        [">u8", gdal.GDT_Float64, None, None],
        # TODO: Test reading/writing GDT_Float16 via float32 Python data
        # ["<f2", gdal.GDT_Float16, None, None],
        # [">f2", gdal.GDT_Float16, None, None],
        # ["<f2", gdal.GDT_Float16, 1.5, 1.5],
        # ["<f2", gdal.GDT_Float16, "NaN", float("nan")],
        # ["<f2", gdal.GDT_Float16, "Infinity", float("infinity")],
        # ["<f2", gdal.GDT_Float16, "-Infinity", float("-infinity")],
        ["<f4", gdal.GDT_Float32, None, None],
        [">f4", gdal.GDT_Float32, None, None],
        ["<f4", gdal.GDT_Float32, 1.5, 1.5],
        ["<f4", gdal.GDT_Float32, "NaN", float("nan")],
        ["<f4", gdal.GDT_Float32, "Infinity", float("infinity")],
        ["<f4", gdal.GDT_Float32, "-Infinity", float("-infinity")],
        ["<f8", gdal.GDT_Float64, None, None],
        [">f8", gdal.GDT_Float64, None, None],
        ["<f8", gdal.GDT_Float64, "NaN", float("nan")],
        ["<f8", gdal.GDT_Float64, "Infinity", float("infinity")],
        ["<f8", gdal.GDT_Float64, "-Infinity", float("-infinity")],
        # TODO: Test reading/writing GDT_CFloat16 via complex64 Python data
        # ["<c4", gdal.GDT_CFloat16, None, None],
        # [">c4", gdal.GDT_CFloat16, None, None],
        ["<c8", gdal.GDT_CFloat32, None, None],
        [">c8", gdal.GDT_CFloat32, None, None],
        ["<c16", gdal.GDT_CFloat64, None, None],
        [">c16", gdal.GDT_CFloat64, None, None],
    ],
)
@pytest.mark.parametrize("use_optimized_code_paths", [True, False])
def test_zarr_write_array_content(
    tmp_vsimem, dtype, gdaltype, fill_value, nodata_value, use_optimized_code_paths
):

    structtype = _gdal_data_type_to_array_type[gdaltype]

    j = {
        "chunks": [2, 3],
        "compressor": None,
        "dtype": dtype,
        "fill_value": fill_value,
        "filters": None,
        "order": "C",
        "shape": [5, 4],
        "zarr_format": 2,
    }

    filename = (
        f"{tmp_vsimem}/test"
        + dtype.replace("<", "lt").replace(">", "gt").replace("!", "not")
        + structtype
        + ".zarr"
    )
    gdal.Mkdir(filename, 0o755)
    f = gdal.VSIFOpenL(filename + "/.zarray", "wb")
    assert f
    data = json.dumps(j)
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    if gdaltype not in (gdal.GDT_CFloat16, gdal.GDT_CFloat32, gdal.GDT_CFloat64):
        tile_0_0_data = struct.pack(dtype[0] + (structtype * 6), 1, 2, 3, 5, 6, 7)
        tile_0_1_data = struct.pack(dtype[0] + (structtype * 6), 4, 0, 0, 8, 0, 0)
    else:
        tile_0_0_data = struct.pack(
            dtype[0] + (structtype * 12), 1, 11, 2, 0, 3, 0, 5, 0, 6, 0, 7, 0
        )
        tile_0_1_data = struct.pack(
            dtype[0] + (structtype * 12), 4, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0
        )
    gdal.FileFromMemBuffer(filename + "/0.0", tile_0_0_data)
    gdal.FileFromMemBuffer(filename + "/0.1", tile_0_1_data)

    with gdaltest.config_option(
        "GDAL_ZARR_USE_OPTIMIZED_CODE_PATHS",
        "YES" if use_optimized_code_paths else "NO",
    ):
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar

    dt = gdal.ExtendedDataType.Create(
        gdal.GDT_CFloat64
        if gdaltype in (gdal.GDT_CFloat32, gdal.GDT_CFloat64)
        else gdal.GDT_Float64
    )

    # Write all nodataset. That should cause tiles to be removed.
    nv = nodata_value if nodata_value else 0
    buf_nodata = array.array(
        "d",
        [nv]
        * (
            5
            * 4
            * (
                2
                if gdaltype in (gdal.GDT_CFloat16, gdal.GDT_CFloat32, gdal.GDT_CFloat64)
                else 1
            )
        ),
    )
    assert ar.Write(buf_nodata, buffer_datatype=dt) == gdal.CE_None
    assert ar.Read(buffer_datatype=dt) == bytearray(buf_nodata)

    if fill_value is None or fill_value == 0 or not gdal.DataTypeIsComplex(gdaltype):
        assert gdal.VSIStatL(filename + "/0.0") is None

    # Write all ones
    ones = array.array(
        "d",
        [0]
        * (
            5
            * 4
            * (
                2
                if gdaltype in (gdal.GDT_CFloat16, gdal.GDT_CFloat32, gdal.GDT_CFloat64)
                else 1
            )
        ),
    )
    assert ar.Write(ones, buffer_datatype=dt) == gdal.CE_None
    assert ar.Read(buffer_datatype=dt) == bytearray(ones)

    # Write with odd array_step
    assert (
        ar.Write(
            struct.pack("d" * 4, nv, nv, 6, 5),
            array_start_idx=[2, 1],
            count=[2, 2],
            array_step=[-1, -1],
            buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64),
        )
        == gdal.CE_None
    )

    # Check back
    assert ar.Read(
        array_start_idx=[2, 1],
        count=[2, 2],
        array_step=[-1, -1],
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64),
    ) == struct.pack("d" * 4, nv, nv, 6, 5)

    # Force dirty block eviction
    ar.Read(buffer_datatype=dt)

    # Check back again
    assert ar.Read(
        array_start_idx=[2, 1],
        count=[2, 2],
        array_step=[-1, -1],
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64),
    ) == struct.pack("d" * 4, nv, nv, 6, 5)


@pytest.mark.parametrize(
    "dt,array_type",
    [
        (gdal.GDT_UInt8, "B"),
        (gdal.GDT_UInt16, "H"),
        (gdal.GDT_UInt32, "I"),
        (gdal.GDT_UInt64, "Q"),
        (gdal.GDT_CFloat64, "d"),
    ],
)
def test_zarr_write_interleave(tmp_vsimem, dt, array_type):
    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr"
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg

        dim0 = rg.CreateDimension("dim0", None, None, 3)
        dim1 = rg.CreateDimension("dim1", None, None, 2)

        ar = rg.CreateMDArray(
            "test",
            [dim0, dim1],
            gdal.ExtendedDataType.Create(dt),
            ["BLOCKSIZE=2,2"],
        )
        assert (
            ar.Write(
                array.array(
                    array_type,
                    [0, 2, 4] if dt != gdal.GDT_CFloat64 else [0, 0.5, 2, 2.5, 4, 4.5],
                ),
                array_start_idx=[0, 0],
                count=[3, 1],
                array_step=[1, 0],
            )
            == gdal.CE_None
        )
        assert (
            ar.Write(
                array.array(
                    array_type,
                    [1, 3, 5] if dt != gdal.GDT_CFloat64 else [1, 1.5, 3, 3.5, 5, 5.5],
                ),
                array_start_idx=[0, 1],
                count=[3, 1],
                array_step=[1, 0],
            )
            == gdal.CE_None
        )

    create()

    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar.Read() == array.array(
        array_type,
        (
            [0, 1, 2, 3, 4, 5]
            if dt != gdal.GDT_CFloat64
            else [0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5, 5, 5.5]
        ),
    )
    if dt != gdal.GDT_CFloat64:
        assert ar.Read(
            buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        ) == array.array("B", [0, 1, 2, 3, 4, 5])


@pytest.mark.parametrize(
    "string_format,input_str,output_str",
    [
        ("ASCII", "0123456789truncated", "0123456789"),
        ("UNICODE", "\u00e9" + "123456789truncated", "\u00e9" + "123456789"),
    ],
    ids=("ASCII", "UNICODE"),
)
def test_zarr_create_array_string(tmp_vsimem, string_format, input_str, output_str):
    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr"
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg

        dim0 = rg.CreateDimension("dim0", None, None, 2)

        ar = rg.CreateMDArray(
            "test",
            [dim0],
            gdal.ExtendedDataType.CreateString(10),
            ["STRING_FORMAT=" + string_format, "COMPRESS=ZLIB"],
        )
        assert ar.Write(["ab", input_str]) == gdal.CE_None

    create()

    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar.Read() == ["ab", output_str]


@pytest.mark.parametrize(
    "srcfilename", ["data/zarr/unicode_le.zarr", "data/zarr/unicode_be.zarr"]
)
def test_zarr_update_array_string(tmp_vsimem, srcfilename):

    filename = str(tmp_vsimem / "test.zarr")

    gdal.Mkdir(filename, 0)
    gdal.FileFromMemBuffer(
        filename + "/.zarray", open(srcfilename + "/.zarray", "rb").read()
    )
    gdal.FileFromMemBuffer(filename + "/0", open(srcfilename + "/0", "rb").read())

    eta = "\u03b7"

    def update():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar.Read() == ["\u00e9"]
        assert ar.Write([eta]) == gdal.CE_None
        assert gdal.GetLastErrorMsg() == ""

    update()

    def check():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar.Read() == [eta]

    check()


@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
@pytest.mark.parametrize(
    "gdal_data_type",
    [
        gdal.GDT_Int8,
        gdal.GDT_UInt8,
        gdal.GDT_Int16,
        gdal.GDT_UInt16,
        gdal.GDT_Int32,
        gdal.GDT_UInt32,
        gdal.GDT_Int64,
        gdal.GDT_UInt64,
        # SWIG does not support Float16
        # gdal.GDT_Float16,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
    ],
)
def test_zarr_create_fortran_order_3d_and_compression_and_dim_separator(
    tmp_vsimem, format, gdal_data_type
):

    array_type = _gdal_data_type_to_array_type[gdal_data_type]

    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr", options=["FORMAT=" + format]
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg

        dim0 = rg.CreateDimension("dim0", None, None, 2)
        dim1 = rg.CreateDimension("dim1", None, None, 3)
        dim2 = rg.CreateDimension("dim2", None, None, 4)

        ar = rg.CreateMDArray(
            "test",
            [dim0, dim1, dim2],
            gdal.ExtendedDataType.Create(gdal_data_type),
            ["CHUNK_MEMORY_LAYOUT=F", "COMPRESS=gzip", "DIM_SEPARATOR=/"],
        )
        assert (
            ar.Write(array.array(array_type, [i for i in range(2 * 3 * 4)]))
            == gdal.CE_None
        )

    create()

    if format == "ZARR_V2":
        f = gdal.VSIFOpenL(tmp_vsimem / "test.zarr/test/.zarray", "rb")
    else:
        f = gdal.VSIFOpenL(tmp_vsimem / "test.zarr/test/zarr.json", "rb")
    assert f
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    j = json.loads(data)
    if format == "ZARR_V2":
        assert "order" in j
        assert j["order"] == "F"
    else:
        assert "codecs" in j
        assert j["codecs"] == [
            {"name": "transpose", "configuration": {"order": [2, 1, 0]}},
            {"name": "bytes", "configuration": {"endian": "little"}},
            {"name": "gzip", "configuration": {"level": 6}},
        ]

    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar.Read() == array.array(array_type, [i for i in range(2 * 3 * 4)])


def test_zarr_create_unit_offset_scale(tmp_vsimem):
    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr"
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg

        ar = rg.CreateMDArray("test", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8))
        assert ar.SetOffset(1.5) == gdal.CE_None
        assert ar.SetScale(2.5) == gdal.CE_None
        assert ar.SetUnit("my unit") == gdal.CE_None

    create()

    f = gdal.VSIFOpenL(tmp_vsimem / "test.zarr/test/.zattrs", "rb")
    assert f
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    j = json.loads(data)
    assert "add_offset" in j
    assert j["add_offset"] == 1.5
    assert "scale_factor" in j
    assert j["scale_factor"] == 2.5
    assert "units" in j
    assert j["units"] == "my unit"

    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar.GetOffset() == 1.5
    assert ar.GetScale() == 2.5
    assert ar.GetUnit() == "my unit"


def test_zarr_getcoordinatevariables(tmp_vsimem):

    src_ds = gdal.OpenEx(
        "data/netcdf/expanded_form_of_grid_mapping.nc", gdal.OF_MULTIDIM_RASTER
    )
    if src_ds is None:
        pytest.skip()

    def create(src_ds):
        ds = gdal.MultiDimTranslate(tmp_vsimem / "test.zarr", src_ds, format="Zarr")
        src_ds = None
        assert ds
        rg = ds.GetRootGroup()

        ar = rg.OpenMDArray("temp")
        coordinate_vars = ar.GetCoordinateVariables()
        assert len(coordinate_vars) == 2
        assert coordinate_vars[0].GetName() == "lat"
        assert coordinate_vars[1].GetName() == "lon"

        assert len(coordinate_vars[0].GetCoordinateVariables()) == 0

    create(src_ds)


def test_zarr_create_copy(tmp_vsimem):

    tst = gdaltest.GDALTest("Zarr", "../../gcore/data/uint16.tif", 1, 4672)

    tst.testCreate(vsimem=1, new_filename=tmp_vsimem / "test.zarr")


@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_create(tmp_vsimem, format):

    ds = gdal.GetDriverByName("Zarr").Create(
        tmp_vsimem / "test.zarr",
        1,
        1,
        3,
        options=["ARRAY_NAME=foo", "FORMAT=" + format, "SINGLE_ARRAY=NO"],
    )
    assert ds.GetGeoTransform(can_return_null=True) is None
    assert ds.GetSpatialRef() is None
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetRasterBand(1).SetNoDataValue(10) == gdal.CE_None
    assert ds.GetRasterBand(1).GetOffset() is None
    assert ds.GetRasterBand(1).SetOffset(1.5) == gdal.CE_None
    assert ds.GetRasterBand(1).GetScale() is None
    assert ds.GetRasterBand(1).SetScale(2.5) == gdal.CE_None
    assert ds.GetRasterBand(1).GetUnitType() == ""
    assert ds.GetRasterBand(1).SetUnitType("my_unit") == gdal.CE_None
    assert ds.SetMetadata({"FOO": "BAR"}) == gdal.CE_None
    ds = None

    ds = gdal.Open(f"ZARR:{tmp_vsimem}/test.zarr:/foo_band1")
    assert ds
    assert ds.GetMetadata() == {"FOO": "BAR"}
    assert ds.GetRasterBand(1).GetNoDataValue() == 10.0
    assert ds.GetRasterBand(1).GetOffset() == 1.5


def test_zarr_create_append_subdataset(tmp_vsimem):
    def create():
        ds = gdal.GetDriverByName("Zarr").Create(
            tmp_vsimem / "test.zarr", 3, 2, 1, options=["ARRAY_NAME=foo"]
        )
        assert ds
        ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
        ds = None

        # Same dimensions. Will reuse the ones of foo
        ds = gdal.GetDriverByName("Zarr").Create(
            tmp_vsimem / "test.zarr",
            3,
            2,
            1,
            options=["APPEND_SUBDATASET=YES", "ARRAY_NAME=bar"],
        )
        assert ds
        ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
        ds = None

        # Different dimensions.
        ds = gdal.GetDriverByName("Zarr").Create(
            tmp_vsimem / "test.zarr",
            30,
            20,
            1,
            options=["APPEND_SUBDATASET=YES", "ARRAY_NAME=baz"],
        )
        assert ds
        ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
        ds = None

    create()

    def check():
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()

        foo = rg.OpenMDArray("foo")
        assert foo
        assert foo.GetDimensions()[0].GetName() == "Y"
        assert foo.GetDimensions()[1].GetName() == "X"

        bar = rg.OpenMDArray("bar")
        assert bar
        assert bar.GetDimensions()[0].GetName() == "Y"
        assert bar.GetDimensions()[1].GetName() == "X"

        baz = rg.OpenMDArray("baz")
        assert baz
        assert baz.GetDimensions()[0].GetName() == "baz_Y"
        assert baz.GetDimensions()[1].GetName() == "baz_X"

    check()


@pytest.mark.parametrize("blocksize", ["1,2", "4000000000,4000000000,4000000000"])
def test_zarr_create_array_invalid_blocksize(tmp_vsimem, blocksize):
    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr"
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg

        dim0 = rg.CreateDimension("dim0", None, None, 2)
        dim1 = rg.CreateDimension("dim1", None, None, 2)
        dim2 = rg.CreateDimension("dim2", None, None, 2)

        with gdal.quiet_errors():
            ar = rg.CreateMDArray(
                "test",
                [dim0, dim1, dim2],
                gdal.ExtendedDataType.Create(gdal.GDT_UInt8),
                ["BLOCKSIZE=" + blocksize],
            )
            assert ar is None

    create()


def test_zarr_read_filters():

    filename = "data/zarr/delta_filter_i4.zarr"
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read() == array.array("i", [i for i in range(10)])


def test_zarr_update_with_filters(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.zarr/.zarray",
        open("data/zarr/delta_filter_i4.zarr/.zarray", "rb").read(),
    )
    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.zarr/0",
        open("data/zarr/delta_filter_i4.zarr/0", "rb").read(),
    )

    def update():
        ds = gdal.OpenEx(
            tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE
        )
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar
        assert ar.Read() == array.array("i", [i for i in range(10)])
        assert ar.Write(array.array("i", [10 - i for i in range(10)])) == gdal.CE_None

    update()

    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read() == array.array("i", [10 - i for i in range(10)])


def test_zarr_create_with_filter(tmp_vsimem):

    tst = gdaltest.GDALTest(
        "Zarr", "../../gcore/data/uint16.tif", 1, 4672, options=["FILTER=delta"]
    )

    tst.testCreate(
        vsimem=1, new_filename=tmp_vsimem / "test.zarr", delete_output_file=False
    )

    f = gdal.VSIFOpenL(tmp_vsimem / "test.zarr/test/.zarray", "rb")
    assert f
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    j = json.loads(data)
    assert "filters" in j
    assert j["filters"] == [{"id": "delta", "dtype": "<u2"}]


def test_zarr_pam_spatial_ref(tmp_vsimem):
    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            tmp_vsimem / "test.zarr"
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg

        dim0 = rg.CreateDimension("dim0", None, None, 2)
        dim1 = rg.CreateDimension("dim1", None, None, 2)
        rg.CreateMDArray(
            "test", [dim0, dim1], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )

    create()

    assert gdal.VSIStatL(tmp_vsimem / "test.zarr/pam.aux.xml") is None

    def check_crs_before():
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar
        crs = ar.GetSpatialRef()
        assert crs is None

    check_crs_before()

    assert gdal.VSIStatL(tmp_vsimem / "zarr/pam.aux.xml") is None

    def set_crs():
        # Open in read-only
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar
        crs = osr.SpatialReference()
        crs.ImportFromEPSG(4326)
        # lat first
        crs.SetDataAxisToSRSAxisMapping([1, 2])
        crs.SetCoordinateEpoch(2021.2)
        assert ar.SetSpatialRef(crs) == gdal.CE_None

    set_crs()

    assert gdal.VSIStatL(tmp_vsimem / "test.zarr/pam.aux.xml") is not None

    f = gdal.VSIFOpenL(tmp_vsimem / "test.zarr/pam.aux.xml", "rb+")
    assert f
    data = gdal.VSIFReadL(1, 1000, f).decode("utf-8")
    assert data.endswith("</PAMDataset>\n")
    data = data[0 : -len("</PAMDataset>\n")] + "<Other/>" + "</PAMDataset>\n"
    gdal.VSIFSeekL(f, 0, 0)
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    def check_crs():
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar
        crs = ar.GetSpatialRef()
        assert crs is not None
        assert crs.GetAuthorityCode(None) == "4326"
        assert crs.GetDataAxisToSRSAxisMapping() == [1, 2]
        assert crs.GetCoordinateEpoch() == 2021.2

    check_crs()

    def check_crs_classic_dataset():
        ds = gdal.Open(tmp_vsimem / "test.zarr")
        crs = ds.GetSpatialRef()
        assert crs is not None
        assert crs.GetAuthorityCode(None) == "4326"
        assert crs.GetDataAxisToSRSAxisMapping() == [2, 1]

    check_crs_classic_dataset()

    def unset_crs():
        # Open in read-only
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar
        assert ar.SetSpatialRef(None) == gdal.CE_None

    unset_crs()

    f = gdal.VSIFOpenL(tmp_vsimem / "test.zarr/pam.aux.xml", "rb")
    assert f
    data = gdal.VSIFReadL(1, 1000, f).decode("utf-8")
    gdal.VSIFCloseL(f)
    assert "<Other />" in data

    def check_unset_crs():
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar
        crs = ar.GetSpatialRef()
        assert crs is None

    check_unset_crs()


def test_zarr_read_too_large_tile_size(tmp_vsimem):

    j = {
        "chunks": [1000000, 2000],
        "compressor": None,
        "dtype": "!b1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [5, 4],
        "zarr_format": 2,
    }

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zarray", json.dumps(j))
    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
    assert ds is not None
    with gdal.quiet_errors():
        assert ds.GetRootGroup().OpenMDArray("test").Read() is None


def test_zarr_read_recursive_array_loading(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)

    j = {"zarr_format": 2}
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zgroup", json.dumps(j))

    j = {
        "chunks": [1],
        "compressor": None,
        "dtype": "!b1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [1],
        "zarr_format": 2,
    }
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/a/.zarray", json.dumps(j))
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/b/.zarray", json.dumps(j))

    j = {"_ARRAY_DIMENSIONS": ["b"]}
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/a/.zattrs", json.dumps(j))

    j = {"_ARRAY_DIMENSIONS": ["a"]}
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/b/.zattrs", json.dumps(j))

    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
    assert ds is not None
    with gdal.quiet_errors():
        ar = ds.GetRootGroup().OpenMDArray("a")
        assert ar
        assert (
            gdal.GetLastErrorMsg()
            == f"Attempt at recursively loading {tmp_vsimem}/test.zarr/a/.zarray"
        )


def test_zarr_read_too_deep_array_loading(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)

    j = {"zarr_format": 2}
    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zgroup", json.dumps(j))

    j = {
        "chunks": [1],
        "compressor": None,
        "dtype": "!b1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [1],
        "zarr_format": 2,
    }

    N = 33
    for i in range(N):
        gdal.FileFromMemBuffer(tmp_vsimem / f"test.zarr/{i}/.zarray", json.dumps(j))

    for i in range(N - 1):
        j = {"_ARRAY_DIMENSIONS": ["%d" % (i + 1)]}
        gdal.FileFromMemBuffer(tmp_vsimem / f"test.zarr/{i}/.zattrs", json.dumps(j))

    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
    assert ds is not None
    with gdal.quiet_errors():
        ar = ds.GetRootGroup().OpenMDArray("0")
        assert ar
        assert gdal.GetLastErrorMsg() == "Too deep call stack in LoadArray()"


@pytest.mark.parametrize(
    "filename,path",
    [
        ("data/zarr/nczarr_v2.zarr", "/MyGroup/Group_A"),
        ("data/zarr/nczarr_v2.zarr/MyGroup", "/Group_A"),
        ("data/zarr/nczarr_v2.zarr/MyGroup/Group_A", ""),
        ("data/zarr/nczarr_v2.zarr/MyGroup/Group_A/dset2", None),
    ],
)
def test_zarr_read_nczarr_v2(filename, path):

    with gdal.quiet_errors():
        assert gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE) is None

    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None
    rg = ds.GetRootGroup()

    ar = rg.OpenMDArrayFromFullname((path if path else "") + "/dset2")
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetSize() == 3
    assert dims[0].GetName() == "lat"
    assert dims[0].GetFullName() == "/MyGroup/lat"
    assert dims[0].GetIndexingVariable() is not None
    assert dims[0].GetIndexingVariable().GetName() == "lat"
    assert dims[0].GetType() == gdal.DIM_TYPE_HORIZONTAL_Y
    assert dims[0].GetDirection() == "NORTH"

    assert dims[1].GetSize() == 3
    assert dims[1].GetName() == "lon"
    assert dims[1].GetFullName() == "/MyGroup/lon"
    assert dims[1].GetIndexingVariable() is not None
    assert dims[1].GetIndexingVariable().GetName() == "lon"
    assert dims[1].GetType() == gdal.DIM_TYPE_HORIZONTAL_X
    assert dims[1].GetDirection() == "EAST"

    if path:
        ar = rg.OpenMDArrayFromFullname(path + "/dset3")
        assert ar
        dims = ar.GetDimensions()
        assert len(dims) == 2
        assert dims[0].GetSize() == 2
        assert dims[0].GetName() == "lat"
        assert dims[0].GetFullName() == "/MyGroup/Group_A/lat"

        assert dims[1].GetSize() == 2
        assert dims[1].GetName() == "lon"
        assert dims[1].GetFullName() == "/MyGroup/Group_A/lon"

    if filename == "data/zarr/nczarr_v2.zarr":
        mygroup = rg.OpenGroup("MyGroup")
        assert mygroup.GetMDArrayNames() == ["lon", "lat", "dset1"]


@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
@pytest.mark.require_driver("netCDF")
def test_zarr_cache_tile_presence(tmp_path, format):

    filename = str(tmp_path / "test.zarr")

    # Create a Zarr array with sparse tiles
    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            filename, options=["FORMAT=" + format]
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg

        dim0 = rg.CreateDimension("dim0", None, None, 2)
        dim1 = rg.CreateDimension("dim1", None, None, 5)
        ar = rg.CreateMDArray(
            "test",
            [dim0, dim1],
            gdal.ExtendedDataType.Create(gdal.GDT_UInt8),
            ["BLOCKSIZE=1,2"],
        )
        assert ar
        assert (
            ar.Write(struct.pack("B" * 1, 10), array_start_idx=[0, 0], count=[1, 1])
            == gdal.CE_None
        )
        assert (
            ar.Write(struct.pack("B" * 1, 100), array_start_idx=[1, 3], count=[1, 1])
            == gdal.CE_None
        )

    create()

    # Create the tile presence cache
    def open_with_cache_tile_presence_option():
        ds = gdal.OpenEx(
            filename,
            gdal.OF_MULTIDIM_RASTER,
            open_options=["CACHE_TILE_PRESENCE=YES"],
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg.OpenMDArray("test") is not None

    open_with_cache_tile_presence_option()

    # Check that the cache exists
    if format == "ZARR_V2":
        cache_filename = filename + "/test/.zarray.gmac"
    else:
        cache_filename = filename + "/test/zarr.json.gmac"
    assert gdal.VSIStatL(cache_filename) is not None

    # Read content of the array
    def read_content():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        assert ds is not None
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("test")
        assert ar is not None
        assert struct.unpack("B" * 2 * 5, ar.Read()) == (
            10,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            100,
            0,
        )

    read_content()

    # again
    open_with_cache_tile_presence_option()

    read_content()

    # Now alter the cache to mark a present tile as missing
    def alter_cache():
        ds = gdal.OpenEx(cache_filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg.GetMDArrayNames() == ["_test_tile_presence"]
        ar = rg.OpenMDArray("_test_tile_presence")
        assert struct.unpack("B" * 2 * 3, ar.Read()) == (1, 0, 0, 0, 1, 0)
        assert (
            ar.Write(struct.pack("B" * 1, 0), array_start_idx=[1, 1], count=[1, 1])
            == gdal.CE_None
        )

    alter_cache()

    # Check that reading the array reflects the above modification
    def read_content_altered():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        assert ds is not None
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("test")
        assert ar is not None
        assert struct.unpack("B" * 2 * 5, ar.Read()) == (
            10,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
        )

    read_content_altered()


@pytest.mark.parametrize("compression", ["NONE", "GZIP"])
@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_advise_read(tmp_path, compression, format):

    filename = str(tmp_path / "test.zarr")

    dim0_size = 1230
    dim1_size = 2570
    dim0_blocksize = 20
    dim1_blocksize = 30
    data_ar = [(i % 256) for i in range(dim0_size * dim1_size)]

    # Create empty block
    y_offset = dim0_blocksize
    x_offset = dim1_blocksize
    for y in range(dim0_blocksize):
        for x in range(dim1_blocksize):
            data_ar[dim1_size * (y + y_offset) + x + x_offset] = 0

    data = array.array("B", data_ar)

    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            filename, options=["FORMAT=" + format]
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg

        dim0 = rg.CreateDimension("dim0", None, None, dim0_size)
        dim1 = rg.CreateDimension("dim1", None, None, dim1_size)
        ar = rg.CreateMDArray(
            "test",
            [dim0, dim1],
            gdal.ExtendedDataType.Create(gdal.GDT_UInt8),
            [
                "COMPRESS=" + compression,
                "BLOCKSIZE=%d,%d" % (dim0_blocksize, dim1_blocksize),
            ],
        )
        assert ar
        ar.SetNoDataValueDouble(0)
        assert ar.Write(data) == gdal.CE_None

    create()

    def read():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        assert ds is not None
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("test")

        with gdal.quiet_errors():
            assert ar.AdviseRead(options=["CACHE_SIZE=1"]) == gdal.CE_Failure

        got_data_before_advise_read = ar.Read(
            array_start_idx=[40, 51], count=[2 * dim0_blocksize, 2 * dim1_blocksize]
        )

        assert ar.AdviseRead() == gdal.CE_None
        assert ar.Read() == data

        assert (
            ar.AdviseRead(
                array_start_idx=[40, 51], count=[2 * dim0_blocksize, dim1_blocksize]
            )
            == gdal.CE_None
        )
        # Read more than AdviseRead() window
        got_data = ar.Read(
            array_start_idx=[40, 51], count=[2 * dim0_blocksize, 2 * dim1_blocksize]
        )
        assert got_data == got_data_before_advise_read

    read()


def test_zarr_read_invalid_nczarr_dim(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)

    j = {
        "chunks": [1, 1],
        "compressor": None,
        "dtype": "!b1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [1, 1],
        "zarr_format": 2,
        "_NCZARR_ARRAY": {"dimrefs": ["/MyGroup/lon", "/OtherGroup/lat"]},
    }

    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zarray", json.dumps(j))

    j = {
        "chunks": [1],
        "compressor": None,
        "dtype": "!b1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [1],
        "zarr_format": 2,
    }

    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/MyGroup/lon/.zarray", json.dumps(j))

    j = {"_NCZARR_GROUP": {"dims": {"lon": 0}}}

    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/MyGroup/.zgroup", json.dumps(j))

    j = {
        "chunks": [2],
        "compressor": None,
        "dtype": "!b1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [2],
        "zarr_format": 2,
    }

    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.zarr/OtherGroup/lat/.zarray", json.dumps(j)
    )

    j = {"_NCZARR_GROUP": {"dims": {"lat": 2, "invalid.name": 2}}}

    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/OtherGroup/.zgroup", json.dumps(j))

    with gdal.quiet_errors():
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("test")
        assert ar


def test_zarr_read_nczar_repeated_array_names(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)

    j = {
        "_NCZARR_GROUP": {
            "dims": {"lon": 1},
            "vars": ["a", "a", "lon", "lon"],
            "groups": ["g", "g"],
        }
    }

    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zgroup", json.dumps(j))

    j = {
        "chunks": [1, 1],
        "compressor": None,
        "dtype": "!b1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [1, 1],
        "zarr_format": 2,
    }

    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/a/.zarray", json.dumps(j))

    j = {
        "chunks": [1],
        "compressor": None,
        "dtype": "!b1",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [1],
        "zarr_format": 2,
    }

    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/lon/.zarray", json.dumps(j))

    with gdal.quiet_errors():
        ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        assert rg.GetMDArrayNames() == ["lon", "a"]
        ar = rg.OpenMDArray("a")
        assert ar
        assert rg.GetGroupNames() == ["g"]


def test_zarr_read_test_overflow_in_AllocateWorkingBuffers_due_to_fortran(tmp_vsimem):

    if sys.maxsize < (1 << 32):
        pytest.skip()

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)

    j = {
        "chunks": [(1 << 32) - 1, (1 << 32) - 1],
        "compressor": None,
        "dtype": "!b1",
        "fill_value": None,
        "filters": None,
        "order": "F",
        "shape": [1, 1],
        "zarr_format": 2,
    }

    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zarray", json.dumps(j))

    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("test")
    with gdal.quiet_errors():
        assert ar.Read(count=[1, 1]) is None


def test_zarr_read_test_overflow_in_AllocateWorkingBuffers_due_to_type_change(
    tmp_vsimem,
):

    if sys.maxsize < (1 << 32):
        pytest.skip()

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)

    j = {
        "chunks": [(1 << 32) - 1, ((1 << 32) - 1) / 8],
        "compressor": None,
        "dtype": "<u8",
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [1, 1],
        "zarr_format": 2,
    }

    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zarray", json.dumps(j))

    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray("test")
    with gdal.quiet_errors():
        assert ar.Read(count=[1, 1]) is None


def test_zarr_read_do_not_crash_on_invalid_byteswap_on_ascii_string(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "test.zarr", 0)

    j = {
        "chunks": [1],
        "compressor": None,
        "dtype": [["x", ">S2"]],  # byteswap here is not really valid...
        "fill_value": base64.b64encode(b"XX").decode("utf-8"),
        "filters": None,
        "order": "C",
        "shape": [1],
        "zarr_format": 2,
    }

    gdal.FileFromMemBuffer(tmp_vsimem / "test.zarr/.zarray", json.dumps(j))

    ds = gdal.OpenEx(tmp_vsimem / "test.zarr", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    rg.OpenMDArray("test")


@pytest.mark.parametrize(
    "format,create_z_metadata",
    [("ZARR_V2", "YES"), ("ZARR_V2", "NO"), ("ZARR_V3", "NO")],
)
def test_zarr_resize(tmp_vsimem, format, create_z_metadata):

    filename = str(tmp_vsimem / "test.zarr")

    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            filename,
            options=["FORMAT=" + format, "CREATE_ZMETADATA=" + create_z_metadata],
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg

        dim0 = rg.CreateDimension("dim0", None, None, 2)
        dim1 = rg.CreateDimension("dim1", None, None, 2)
        var = rg.CreateMDArray(
            "test", [dim0, dim1], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        assert var.Write(struct.pack("B" * (2 * 2), 1, 2, 3, 4)) == gdal.CE_None

    create()

    def resize_read_only():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        var = rg.OpenMDArray("test")

        with gdal.quiet_errors():
            assert var.Resize([5, 2]) == gdal.CE_Failure

    resize_read_only()

    def resize():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        rg = ds.GetRootGroup()
        var = rg.OpenMDArray("test")
        with gdal.quiet_errors():
            assert var.Resize([1, 2]) == gdal.CE_Failure, "shrinking not allowed"

        assert var.Resize([5, 2]) == gdal.CE_None
        assert var.GetDimensions()[0].GetSize() == 5
        assert var.GetDimensions()[1].GetSize() == 2
        assert (
            var.Write(
                struct.pack("B" * (3 * 2), 5, 6, 7, 8, 9, 10),
                array_start_idx=[2, 0],
                count=[3, 2],
            )
            == gdal.CE_None
        )

    resize()

    if create_z_metadata == "YES":
        f = gdal.VSIFOpenL(filename + "/.zmetadata", "rb")
        assert f
        data = gdal.VSIFReadL(1, 10000, f)
        gdal.VSIFCloseL(f)
        j = json.loads(data)
        assert j["metadata"]["test/.zarray"]["shape"] == [5, 2]

    def check():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        var = rg.OpenMDArray("test")
        assert var.GetDimensions()[0].GetSize() == 5
        assert var.GetDimensions()[1].GetSize() == 2
        assert struct.unpack("B" * (5 * 2), var.Read()) == (
            1,
            2,
            3,
            4,
            5,
            6,
            7,
            8,
            9,
            10,
        )

    check()


@pytest.mark.parametrize("create_z_metadata", [True, False])
def test_zarr_resize_XARRAY(tmp_vsimem, create_z_metadata):

    filename = str(tmp_vsimem / "test.zarr")

    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
            filename,
            options=["CREATE_ZMETADATA=" + ("YES" if create_z_metadata else "NO")],
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg

        dim0 = rg.CreateDimension("dim0", None, None, 2)
        dim0_var = rg.CreateMDArray(
            "dim0", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        dim0.SetIndexingVariable(dim0_var)
        dim1 = rg.CreateDimension("dim1", None, None, 2)
        dim1_var = rg.CreateMDArray(
            "dim1", [dim1], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        dim1.SetIndexingVariable(dim1_var)
        var = rg.CreateMDArray(
            "test", [dim0, dim1], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        assert var.Write(struct.pack("B" * (2 * 2), 1, 2, 3, 4)) == gdal.CE_None

        var2 = rg.CreateMDArray(
            "test2", [dim0, dim1], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        assert var2 is not None

    create()

    def resize():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        rg = ds.GetRootGroup()
        var = rg.OpenMDArray("test")
        with gdal.quiet_errors():
            assert var.Resize([1, 2]) == gdal.CE_Failure, "shrinking not allowed"

        dim0 = rg.OpenMDArray("dim0")

        assert var.Resize([5, 2]) == gdal.CE_None
        assert var.GetDimensions()[0].GetSize() == 5
        assert var.GetDimensions()[1].GetSize() == 2
        assert (
            var.Write(
                struct.pack("B" * (3 * 2), 5, 6, 7, 8, 9, 10),
                array_start_idx=[2, 0],
                count=[3, 2],
            )
            == gdal.CE_None
        )

        assert dim0.GetDimensions()[0].GetSize() == 5

    resize()

    if create_z_metadata:
        f = gdal.VSIFOpenL(filename + "/.zmetadata", "rb")
        assert f
        data = gdal.VSIFReadL(1, 10000, f)
        gdal.VSIFCloseL(f)
        j = json.loads(data)
        assert j["metadata"]["test/.zarray"]["shape"] == [5, 2]
        assert j["metadata"]["dim0/.zarray"]["shape"] == [5]

    def check():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        var = rg.OpenMDArray("test")
        assert var.GetDimensions()[0].GetSize() == 5
        assert var.GetDimensions()[1].GetSize() == 2
        assert struct.unpack("B" * (5 * 2), var.Read()) == (
            1,
            2,
            3,
            4,
            5,
            6,
            7,
            8,
            9,
            10,
        )

        dim0 = rg.OpenMDArray("dim0")
        assert dim0.GetDimensions()[0].GetSize() == 5

        var2 = rg.OpenMDArray("test")
        assert var2.GetDimensions()[0].GetSize() == 5

    check()


###############################################################################


def test_zarr_resize_dim_referenced_twice(tmp_vsimem):

    filename = str(tmp_vsimem / "test.zarr")

    def create():
        ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(filename)
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg

        dim0 = rg.CreateDimension("dim0", None, None, 2)
        dim0_var = rg.CreateMDArray(
            "dim0", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        dim0.SetIndexingVariable(dim0_var)

        var = rg.CreateMDArray(
            "test", [dim0, dim0], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        assert var.Write(struct.pack("B" * (2 * 2), 1, 2, 3, 4)) == gdal.CE_None

    create()

    def resize():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        rg = ds.GetRootGroup()
        var = rg.OpenMDArray("test")

        with gdal.quiet_errors():
            assert var.Resize([3, 4]) == gdal.CE_Failure
            assert var.Resize([4, 3]) == gdal.CE_Failure

        assert var.Resize([3, 3]) == gdal.CE_None
        assert var.GetDimensions()[0].GetSize() == 3
        assert var.GetDimensions()[1].GetSize() == 3

    resize()

    f = gdal.VSIFOpenL(filename + "/.zmetadata", "rb")
    assert f
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    j = json.loads(data)
    assert j["metadata"]["test/.zarray"]["shape"] == [3, 3]
    assert j["metadata"]["dim0/.zarray"]["shape"] == [3]

    def check():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        var = rg.OpenMDArray("test")
        assert var.GetDimensions()[0].GetSize() == 3
        assert var.GetDimensions()[1].GetSize() == 3

    check()


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "format,create_z_metadata",
    [("ZARR_V2", "YES"), ("ZARR_V2", "NO"), ("ZARR_V3", "NO")],
)
def test_zarr_multidim_rename_group_at_creation(tmp_vsimem, format, create_z_metadata):

    drv = gdal.GetDriverByName("ZARR")
    filename = str(tmp_vsimem / "test.zarr")

    def test():
        ds = drv.CreateMultiDimensional(
            filename,
            options=["FORMAT=" + format, "CREATE_ZMETADATA=" + create_z_metadata],
        )
        rg = ds.GetRootGroup()
        group = rg.CreateGroup("group")
        group_attr = group.CreateAttribute(
            "group_attr", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        rg.CreateGroup("other_group")
        dim = group.CreateDimension(
            "dim0", "unspecified type", "unspecified direction", 2
        )
        ar = group.CreateMDArray(
            "ar", [dim], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        attr = ar.CreateAttribute(
            "attr", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )

        subgroup = group.CreateGroup("subgroup")
        subgroup_attr = subgroup.CreateAttribute(
            "subgroup_attr", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        subgroup_ar = subgroup.CreateMDArray(
            "subgroup_ar", [dim], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        subgroup_ar_attr = subgroup_ar.CreateAttribute(
            "subgroup_ar_attr", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )

        # Cannot rename root group
        with pytest.raises(Exception):
            rg.Rename("foo")

        # Empty name
        with pytest.raises(Exception):
            group.Rename("")

        # Existing name
        with pytest.raises(Exception):
            group.Rename("other_group")

        # Existing array name (group and array names share the same namespace)
        with pytest.raises(Exception):
            subgroup.Rename("ar")

        # Rename group and test effects
        group.Rename("group_renamed")
        assert group.GetName() == "group_renamed"
        assert group.GetFullName() == "/group_renamed"

        assert set(rg.GetGroupNames()) == {"group_renamed", "other_group"}

        assert dim.GetName() == "dim0"
        assert dim.GetFullName() == "/group_renamed/dim0"

        assert group_attr.GetName() == "group_attr"
        assert group_attr.GetFullName() == "/group_renamed/_GLOBAL_/group_attr"

        assert ar.GetName() == "ar"
        assert ar.GetFullName() == "/group_renamed/ar"

        assert attr.GetName() == "attr"
        assert attr.GetFullName() == "/group_renamed/ar/attr"

        assert subgroup.GetName() == "subgroup"
        assert subgroup.GetFullName() == "/group_renamed/subgroup"

        assert subgroup_attr.GetName() == "subgroup_attr"
        assert (
            subgroup_attr.GetFullName()
            == "/group_renamed/subgroup/_GLOBAL_/subgroup_attr"
        )

        assert subgroup_ar.GetName() == "subgroup_ar"
        assert subgroup_ar.GetFullName() == "/group_renamed/subgroup/subgroup_ar"

        assert subgroup_ar_attr.GetName() == "subgroup_ar_attr"
        assert (
            subgroup_ar_attr.GetFullName()
            == "/group_renamed/subgroup/subgroup_ar/subgroup_ar_attr"
        )

    def reopen():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()

        assert set(rg.GetGroupNames()) == {"group_renamed", "other_group"}

        group = rg.OpenGroup("group_renamed")
        assert set([attr.GetName() for attr in group.GetAttributes()]) == {"group_attr"}

        assert group.GetMDArrayNames() == ["ar"]

        # Read-only
        with pytest.raises(Exception):
            group.Rename("group_renamed2")

        assert set(rg.GetGroupNames()) == {"group_renamed", "other_group"}

    test()
    reopen()


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "format,create_z_metadata",
    [("ZARR_V2", "YES"), ("ZARR_V2", "NO"), ("ZARR_V3", "NO")],
)
def test_zarr_multidim_rename_group_after_reopening(
    tmp_vsimem, format, create_z_metadata
):

    drv = gdal.GetDriverByName("ZARR")
    filename = str(tmp_vsimem / "test.zarr")

    def create():
        ds = drv.CreateMultiDimensional(
            filename,
            options=["FORMAT=" + format, "CREATE_ZMETADATA=" + create_z_metadata],
        )
        rg = ds.GetRootGroup()
        group = rg.CreateGroup("group")
        group_attr = group.CreateAttribute(
            "group_attr", [], gdal.ExtendedDataType.CreateString()
        )
        group_attr.Write("my_string")
        rg.CreateGroup("other_group")
        dim = group.CreateDimension(
            "dim0", "unspecified type", "unspecified direction", 2
        )
        ar = group.CreateMDArray(
            "ar", [dim], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        attr = ar.CreateAttribute("attr", [], gdal.ExtendedDataType.CreateString())
        attr.Write("foo")
        attr2 = ar.CreateAttribute("attr2", [], gdal.ExtendedDataType.CreateString())
        attr2.Write("foo2")

        group.CreateGroup("subgroup")

    def reopen_readonly():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        # Read-only
        with pytest.raises(Exception):
            group.Rename("group_renamed2")

        assert set(rg.GetGroupNames()) == {"group", "other_group"}

    def rename():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        # Cannot rename root group
        with pytest.raises(Exception):
            rg.Rename("foo")

        # Empty name
        with pytest.raises(Exception):
            group.Rename("")

        # Existing name
        with pytest.raises(Exception):
            group.Rename("other_group")

        group_attr = group.GetAttribute("group_attr")
        ar = group.OpenMDArray("ar")
        attr = ar.GetAttribute("attr")
        attr.Write("bar")

        # Rename group and test effects
        group.Rename("group_renamed")
        assert group.GetName() == "group_renamed"
        assert group.GetFullName() == "/group_renamed"

        assert set(rg.GetGroupNames()) == {"group_renamed", "other_group"}

        assert group_attr.GetFullName() == "/group_renamed/_GLOBAL_/group_attr"

        attr2 = ar.GetAttribute("attr2")
        attr2.Write("bar2")

    def reopen_after_rename():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()

        group = rg.OpenGroup("group_renamed")
        assert set([attr.GetName() for attr in group.GetAttributes()]) == {"group_attr"}

        assert group.GetMDArrayNames() == ["ar"]

        assert set(rg.GetGroupNames()) == {"group_renamed", "other_group"}

        ar = group.OpenMDArray("ar")

        attr = ar.GetAttribute("attr")
        assert attr.Read() == "bar"

        attr2 = ar.GetAttribute("attr2")
        assert attr2.Read() == "bar2"

    create()
    reopen_readonly()
    rename()
    reopen_after_rename()


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "format,create_z_metadata",
    [("ZARR_V2", "YES"), ("ZARR_V2", "NO"), ("ZARR_V3", "NO")],
)
def test_zarr_multidim_rename_array_at_creation(tmp_path, format, create_z_metadata):

    drv = gdal.GetDriverByName("ZARR")
    filename = str(tmp_path / "test.zarr")

    def test():
        ds = drv.CreateMultiDimensional(
            filename,
            options=["FORMAT=" + format, "CREATE_ZMETADATA=" + create_z_metadata],
        )
        rg = ds.GetRootGroup()
        group = rg.CreateGroup("group")
        group.CreateGroup("subgroup")

        dim = group.CreateDimension(
            "dim0", "unspecified type", "unspecified direction", 2
        )
        ar = group.CreateMDArray(
            "ar", [dim], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        group.CreateMDArray(
            "other_ar", [dim], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        attr = ar.CreateAttribute(
            "attr", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )

        # Empty name
        with pytest.raises(Exception):
            ar.Rename("")

        # Existing name
        with pytest.raises(Exception):
            ar.Rename("other_ar")

        # Existing subgroup name (group and array names share the same namespace)
        with pytest.raises(Exception):
            ar.Rename("subgroup")

        # Rename array and test effects
        ar.Rename("ar_renamed")

        if format == "ZARR_V2":
            assert gdal.VSIStatL(filename + "/group/ar/.zarray") is None
            assert gdal.VSIStatL(filename + "/group/ar_renamed/.zarray") is not None
        else:
            assert gdal.VSIStatL(filename + "/group/ar/zarr.json") is None
            assert gdal.VSIStatL(filename + "/group/ar_renamed/zarr.json") is not None

        assert ar.GetName() == "ar_renamed"
        assert ar.GetFullName() == "/group/ar_renamed"

        assert set(group.GetMDArrayNames()) == {"ar_renamed", "other_ar"}

        with pytest.raises(Exception):
            group.OpenMDArray("ar")
        assert group.OpenMDArray("ar_renamed") is not None

        assert attr.GetName() == "attr"
        assert attr.GetFullName() == "/group/ar_renamed/attr"

    def reopen():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        assert set(group.GetMDArrayNames()) == {"ar_renamed", "other_ar"}

        ar_renamed = group.OpenMDArray("ar_renamed")
        assert set([attr.GetName() for attr in ar_renamed.GetAttributes()]) == {"attr"}

        # Read-only
        with pytest.raises(Exception):
            ar_renamed.Rename("ar_renamed2")

        assert set(group.GetMDArrayNames()) == {"ar_renamed", "other_ar"}

    test()
    reopen()


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "format,create_z_metadata",
    [("ZARR_V2", "YES"), ("ZARR_V2", "NO"), ("ZARR_V3", "NO")],
)
def test_zarr_multidim_rename_array_after_reopening(
    tmp_vsimem, format, create_z_metadata
):

    drv = gdal.GetDriverByName("ZARR")
    filename = str(tmp_vsimem / "test.zarr")

    def create():
        ds = drv.CreateMultiDimensional(
            filename,
            options=["FORMAT=" + format, "CREATE_ZMETADATA=" + create_z_metadata],
        )
        rg = ds.GetRootGroup()
        group = rg.CreateGroup("group")

        dim = group.CreateDimension(
            "dim0", "unspecified type", "unspecified direction", 2
        )
        ar = group.CreateMDArray(
            "ar", [dim], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        group.CreateMDArray(
            "other_ar", [dim], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        attr = ar.CreateAttribute("attr", [], gdal.ExtendedDataType.CreateString())
        attr.Write("foo")

    def reopen_readonly():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        assert set(group.GetMDArrayNames()) == {"ar", "other_ar"}

        ar = group.OpenMDArray("ar")

        # Read-only
        with pytest.raises(Exception):
            ar.Rename("ar_renamed")

        assert set(group.GetMDArrayNames()) == {"ar", "other_ar"}

    def rename():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        assert set(group.GetMDArrayNames()) == {"ar", "other_ar"}

        ar = group.OpenMDArray("ar")
        attr = ar.GetAttribute("attr")

        # Rename array and test effects
        ar.Rename("ar_renamed")

        if format == "ZARR_V2":
            assert gdal.VSIStatL(filename + "/group/ar/.zarray") is None
            assert gdal.VSIStatL(filename + "/group/ar_renamed/.zarray") is not None
        else:
            assert gdal.VSIStatL(filename + "/group/ar/zarr.json") is None
            assert gdal.VSIStatL(filename + "/group/ar_renamed/zarr.json") is not None

        assert ar.GetName() == "ar_renamed"
        assert ar.GetFullName() == "/group/ar_renamed"

        assert set(group.GetMDArrayNames()) == {"ar_renamed", "other_ar"}

        with pytest.raises(Exception):
            assert group.OpenMDArray("ar") is None
        assert group.OpenMDArray("ar_renamed") is not None

        assert attr.GetName() == "attr"
        assert attr.GetFullName() == "/group/ar_renamed/attr"

        attr.Write("bar")

    def reopen_after_rename():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        assert set(group.GetMDArrayNames()) == {"ar_renamed", "other_ar"}

        ar = group.OpenMDArray("ar_renamed")
        attr = ar.GetAttribute("attr")
        assert attr.Read() == "bar"

    create()
    reopen_readonly()
    rename()
    reopen_after_rename()


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "format,create_z_metadata",
    [("ZARR_V2", "YES"), ("ZARR_V2", "NO"), ("ZARR_V3", "NO")],
)
def test_zarr_multidim_rename_attr_after_reopening(
    tmp_vsimem, format, create_z_metadata
):

    drv = gdal.GetDriverByName("ZARR")
    filename = str(tmp_vsimem / "test.zarr")

    def create():
        ds = drv.CreateMultiDimensional(
            filename,
            options=["FORMAT=" + format, "CREATE_ZMETADATA=" + create_z_metadata],
        )
        rg = ds.GetRootGroup()
        group = rg.CreateGroup("group")
        group_attr = group.CreateAttribute(
            "group_attr", [], gdal.ExtendedDataType.CreateString()
        )
        group_attr.Write("foo")

        dim = group.CreateDimension(
            "dim0", "unspecified type", "unspecified direction", 2
        )
        ar = group.CreateMDArray(
            "ar", [dim], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        group.CreateMDArray(
            "other_ar", [dim], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        attr = ar.CreateAttribute("attr", [], gdal.ExtendedDataType.CreateString())
        attr.Write("foo")

    def rename():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        # Rename group attribute and test effects
        group_attr = group.GetAttribute("group_attr")
        group_attr.Rename("group_attr_renamed")

        assert group_attr.GetName() == "group_attr_renamed"
        assert group_attr.GetFullName() == "/group/_GLOBAL_/group_attr_renamed"

        group_attr.Write("bar")

        ar = group.OpenMDArray("ar")
        attr = ar.GetAttribute("attr")

        # Rename attribute and test effects
        attr.Rename("attr_renamed")

        assert attr.GetName() == "attr_renamed"
        assert attr.GetFullName() == "/group/ar/attr_renamed"

        attr.Write("bar")

    def reopen_after_rename():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        group_attr_renamed = group.GetAttribute("group_attr_renamed")
        assert group_attr_renamed.Read() == "bar"

        ar = group.OpenMDArray("ar")
        attr_renamed = ar.GetAttribute("attr_renamed")
        assert attr_renamed.Read() == "bar"

    create()
    rename()
    reopen_after_rename()


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "format,create_z_metadata",
    [("ZARR_V2", "YES"), ("ZARR_V2", "NO"), ("ZARR_V3", "NO")],
)
def test_zarr_multidim_rename_dim_at_creation(tmp_vsimem, format, create_z_metadata):

    drv = gdal.GetDriverByName("ZARR")
    filename = str(tmp_vsimem / "test.zarr")

    def create():
        ds = drv.CreateMultiDimensional(
            filename,
            options=["FORMAT=" + format, "CREATE_ZMETADATA=" + create_z_metadata],
        )
        rg = ds.GetRootGroup()
        dim = rg.CreateDimension("dim", None, None, 2)
        other_dim = rg.CreateDimension("other_dim", None, None, 2)
        var = rg.CreateMDArray(
            "var", [dim, other_dim], gdal.ExtendedDataType.Create(gdal.GDT_Int16)
        )

        # Empty name
        with pytest.raises(Exception):
            dim.Rename("")

        # Existing name
        with pytest.raises(Exception):
            dim.Rename("other_dim")
        assert dim.GetName() == "dim"
        assert dim.GetFullName() == "/dim"

        dim.Rename("dim_renamed")
        assert dim.GetName() == "dim_renamed"
        assert dim.GetFullName() == "/dim_renamed"

        assert set(x.GetName() for x in rg.GetDimensions()) == {
            "dim_renamed",
            "other_dim",
        }

        assert [x.GetName() for x in var.GetDimensions()] == [
            "dim_renamed",
            "other_dim",
        ]

    def reopen():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()

        assert set(x.GetName() for x in rg.GetDimensions()) == {
            "dim_renamed",
            "other_dim",
        }

        # Read-only
        with pytest.raises(Exception):
            rg.GetDimensions()[0].Rename("dim_renamed2")

        assert set(x.GetName() for x in rg.GetDimensions()) == {
            "dim_renamed",
            "other_dim",
        }

    create()
    reopen()


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "format,create_z_metadata",
    [("ZARR_V2", "YES"), ("ZARR_V2", "NO"), ("ZARR_V3", "NO")],
)
def test_zarr_multidim_rename_dim_after_reopening(
    tmp_vsimem, format, create_z_metadata
):

    drv = gdal.GetDriverByName("ZARR")
    filename = str(tmp_vsimem / "test.zarr")

    def create():
        ds = drv.CreateMultiDimensional(
            filename,
            options=["FORMAT=" + format, "CREATE_ZMETADATA=" + create_z_metadata],
        )
        rg = ds.GetRootGroup()
        dim = rg.CreateDimension("dim", None, None, 2)
        other_dim = rg.CreateDimension("other_dim", None, None, 2)
        rg.CreateMDArray(
            "var", [dim, other_dim], gdal.ExtendedDataType.Create(gdal.GDT_Int16)
        )

    def rename():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        rg = ds.GetRootGroup()
        dim = list(filter(lambda dim: dim.GetName() == "dim", rg.GetDimensions()))[0]

        # Empty name
        with pytest.raises(Exception):
            dim.Rename("")

        # Existing name
        with pytest.raises(Exception):
            dim.Rename("other_dim")
        assert dim.GetName() == "dim"
        assert dim.GetFullName() == "/dim"

        dim.Rename("dim_renamed")
        assert dim.GetName() == "dim_renamed"
        assert dim.GetFullName() == "/dim_renamed"

        assert set(x.GetName() for x in rg.GetDimensions()) == {
            "dim_renamed",
            "other_dim",
        }

    def reopen_after_rename():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()

        assert set(x.GetName() for x in rg.GetDimensions()) == {
            "dim_renamed",
            "other_dim",
        }

        # Read-only
        with pytest.raises(Exception):
            rg.GetDimensions()[0].Rename("dim_renamed2")

        assert set(x.GetName() for x in rg.GetDimensions()) == {
            "dim_renamed",
            "other_dim",
        }

        var = rg.OpenMDArray("var")
        assert [x.GetName() for x in var.GetDimensions()] == [
            "dim_renamed",
            "other_dim",
        ]

    create()
    rename()
    reopen_after_rename()


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "format,create_z_metadata",
    [("ZARR_V2", "YES"), ("ZARR_V2", "NO"), ("ZARR_V3", "NO")],
)
@pytest.mark.parametrize("get_before_delete", [True, False])
def test_zarr_multidim_delete_group_after_reopening(
    tmp_vsimem, format, create_z_metadata, get_before_delete
):

    drv = gdal.GetDriverByName("ZARR")
    filename = str(tmp_vsimem / "test.zarr")

    def create():
        ds = drv.CreateMultiDimensional(
            filename,
            options=["FORMAT=" + format, "CREATE_ZMETADATA=" + create_z_metadata],
        )
        rg = ds.GetRootGroup()
        group = rg.CreateGroup("group")
        group_attr = group.CreateAttribute(
            "group_attr", [], gdal.ExtendedDataType.CreateString()
        )
        group_attr.Write("my_string")
        rg.CreateGroup("other_group")
        dim = group.CreateDimension(
            "dim0", "unspecified type", "unspecified direction", 2
        )
        ar = group.CreateMDArray(
            "ar", [dim], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )
        attr = ar.CreateAttribute("attr", [], gdal.ExtendedDataType.CreateString())
        attr.Write("foo")
        attr2 = ar.CreateAttribute("attr2", [], gdal.ExtendedDataType.CreateString())
        attr2.Write("foo")

        group.CreateGroup("subgroup")

    def reopen_readonly():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()

        # Read-only
        with pytest.raises(Exception, match="not open in update mode"):
            rg.DeleteGroup("group")

        assert set(rg.GetGroupNames()) == {"group", "other_group"}

        assert rg.OpenGroup("group")

    def delete():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        rg = ds.GetRootGroup()

        with pytest.raises(Exception, match="is not a sub-group of this group"):
            rg.DeleteGroup("non_existing")

        if not get_before_delete:
            rg.DeleteGroup("group")

            assert set(rg.GetGroupNames()) == {"other_group"}

            with pytest.raises(Exception, match="does not exist"):
                rg.OpenGroup("group")

        else:
            group = rg.OpenGroup("group")
            group_attr = group.GetAttribute("group_attr")
            ar = group.OpenMDArray("ar")
            attr = ar.GetAttribute("attr")

            # Delete group and test effects
            rg.DeleteGroup("group")

            assert set(rg.GetGroupNames()) == {"other_group"}

            with pytest.raises(Exception, match="does not exist"):
                rg.OpenGroup("group")

            with pytest.raises(Exception, match="has been deleted"):
                group.Rename("renamed")

            with pytest.raises(Exception, match="has been deleted"):
                group_attr.Rename("renamed")

            with pytest.raises(Exception, match="has been deleted"):
                ar.GetAttributes()

            with pytest.raises(Exception, match="has been deleted"):
                attr.Write("foo2")

            with pytest.raises(Exception, match="has been deleted"):
                ar.GetAttribute("attr2")

    def reopen_after_delete():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()

        assert set(rg.GetGroupNames()) == {"other_group"}

    create()
    reopen_readonly()
    delete()
    reopen_after_delete()


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "format,create_z_metadata",
    [("ZARR_V2", "YES"), ("ZARR_V2", "NO"), ("ZARR_V3", "NO")],
)
@pytest.mark.parametrize("get_before_delete", [True, False])
def test_zarr_multidim_delete_array_after_reopening(
    tmp_vsimem, format, create_z_metadata, get_before_delete
):

    drv = gdal.GetDriverByName("ZARR")
    filename = str(tmp_vsimem / "test.zarr")

    def create():
        ds = drv.CreateMultiDimensional(
            filename,
            options=["FORMAT=" + format, "CREATE_ZMETADATA=" + create_z_metadata],
        )
        rg = ds.GetRootGroup()
        group = rg.CreateGroup("group")
        ar = group.CreateMDArray("ar", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8))
        attr = ar.CreateAttribute("attr", [], gdal.ExtendedDataType.CreateString())
        attr.Write("foo")
        attr2 = ar.CreateAttribute("attr2", [], gdal.ExtendedDataType.CreateString())
        attr2.Write("foo")

        group.CreateMDArray(
            "other_ar", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
        )

    def reopen_readonly():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        # Read-only
        with pytest.raises(Exception, match="not open in update mode"):
            group.DeleteMDArray("ar")

        assert set(group.GetMDArrayNames()) == {"ar", "other_ar"}

        assert group.OpenMDArray("ar")

    def delete():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        with pytest.raises(Exception, match="is not an array of this group"):
            group.DeleteMDArray("non_existing")

        if not get_before_delete:
            group.DeleteMDArray("ar")

            assert set(group.GetMDArrayNames()) == {"other_ar"}

            with pytest.raises(Exception, match="does not exist"):
                group.OpenMDArray("ar")

        else:
            ar = group.OpenMDArray("ar")
            attr = ar.GetAttribute("attr")

            # Delete array and test effects
            group.DeleteMDArray("ar")

            assert set(group.GetMDArrayNames()) == {"other_ar"}

            with pytest.raises(Exception, match="does not exist"):
                group.OpenMDArray("ar")

            with pytest.raises(Exception, match="has been deleted"):
                ar.GetAttributes()

            with pytest.raises(Exception, match="has been deleted"):
                attr.Write("foo2")

    def reopen_after_delete():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        assert set(group.GetMDArrayNames()) == {"other_ar"}

    create()
    reopen_readonly()
    delete()
    reopen_after_delete()


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "format,create_z_metadata",
    [("ZARR_V2", "YES"), ("ZARR_V2", "NO"), ("ZARR_V3", "NO")],
)
@pytest.mark.parametrize("get_before_delete", [True, False])
def test_zarr_multidim_delete_attribute_after_reopening(
    tmp_vsimem, format, create_z_metadata, get_before_delete
):

    drv = gdal.GetDriverByName("ZARR")
    filename = str(tmp_vsimem / "test.zarr")

    def create():
        ds = drv.CreateMultiDimensional(
            filename,
            options=["FORMAT=" + format, "CREATE_ZMETADATA=" + create_z_metadata],
        )
        rg = ds.GetRootGroup()
        group = rg.CreateGroup("group")
        group_attr = group.CreateAttribute(
            "group_attr", [], gdal.ExtendedDataType.CreateString()
        )
        group_attr.Write("foo")
        group_attr2 = group.CreateAttribute(
            "group_attr2", [], gdal.ExtendedDataType.CreateString()
        )
        group_attr2.Write("foo")

        ar = group.CreateMDArray("ar", [], gdal.ExtendedDataType.Create(gdal.GDT_UInt8))
        attr = ar.CreateAttribute("attr", [], gdal.ExtendedDataType.CreateString())
        attr.Write("foo")
        attr2 = ar.CreateAttribute("attr2", [], gdal.ExtendedDataType.CreateString())
        attr2.Write("foo")

    def reopen_readonly():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        # Read-only
        with pytest.raises(Exception, match="not open in update mode"):
            group.DeleteAttribute("group_attr")

        assert set(x.GetName() for x in group.GetAttributes()) == {
            "group_attr",
            "group_attr2",
        }

        ar = group.OpenMDArray("ar")
        with pytest.raises(Exception, match="not open in update mode"):
            ar.DeleteAttribute("attr")

        assert set(x.GetName() for x in ar.GetAttributes()) == {"attr", "attr2"}

    def delete():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        with pytest.raises(Exception, match="is not an attribute"):
            group.DeleteAttribute("non_existing")

        ar = group.OpenMDArray("ar")
        with pytest.raises(Exception, match="is not an attribute"):
            ar.DeleteAttribute("non_existing")

        if not get_before_delete:
            group.DeleteAttribute("group_attr")

            assert set(x.GetName() for x in group.GetAttributes()) == {"group_attr2"}

            ar.DeleteAttribute("attr")

            assert set(x.GetName() for x in ar.GetAttributes()) == {"attr2"}

        else:
            group_attr = group.GetAttribute("group_attr")
            attr = ar.GetAttribute("attr")

            group.DeleteAttribute("group_attr")

            assert set(x.GetName() for x in group.GetAttributes()) == {"group_attr2"}

            with pytest.raises(Exception, match="has been deleted"):
                group_attr.Write("foo2")

            ar.DeleteAttribute("attr")

            assert set(x.GetName() for x in ar.GetAttributes()) == {"attr2"}

            with pytest.raises(Exception, match="has been deleted"):
                attr.Write("foo2")

    def reopen_after_delete():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        group = rg.OpenGroup("group")

        assert set(x.GetName() for x in group.GetAttributes()) == {"group_attr2"}

        ar = group.OpenMDArray("ar")
        assert set(x.GetName() for x in ar.GetAttributes()) == {"attr2"}

    create()
    reopen_readonly()
    delete()
    reopen_after_delete()


###############################################################################
# Test GDALDriver::Delete()


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_driver_delete(tmp_vsimem, format):

    drv = gdal.GetDriverByName("ZARR")
    filename = tmp_vsimem / "test.zarr"

    drv.Create(filename, 1, 1, options=["FORMAT=" + format])

    assert gdal.Open(filename)

    assert drv.Delete(filename) == gdal.CE_None
    assert gdal.VSIStatL(filename) is None

    with pytest.raises(Exception):
        gdal.Open(filename)


###############################################################################
# Test GDALDriver::Rename()


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_driver_rename(tmp_vsimem, format):

    drv = gdal.GetDriverByName("ZARR")
    filename = tmp_vsimem / "test.zarr"
    newfilename = tmp_vsimem / "newtest.zarr"

    drv.Create(filename, 1, 1, options=["FORMAT=" + format])

    with gdal.quiet_warnings():
        # ZARR_V3 gives: "Warning 1: fill_value = null is invalid" on open.
        assert gdal.Open(filename)

    assert drv.Rename(newfilename, filename) == gdal.CE_None

    assert gdal.VSIStatL(filename) is None
    with pytest.raises(Exception):
        gdal.Open(filename)

    assert gdal.VSIStatL(newfilename)
    with gdal.quiet_warnings():
        # ZARR_V3 gives: "Warning 1: fill_value = null is invalid" on open.
        assert gdal.Open(newfilename)


###############################################################################
# Test GDALDriver::CopyFiles()


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_driver_copy_files(tmp_vsimem, format):

    drv = gdal.GetDriverByName("ZARR")
    filename = tmp_vsimem / "test.zarr"
    newfilename = tmp_vsimem / "newtest.zarr"

    drv.Create(filename, 1, 1, options=["FORMAT=" + format])

    assert gdal.Open(filename)

    assert drv.CopyFiles(newfilename, filename) == gdal.CE_None

    with gdal.quiet_warnings():
        # ZARR_V3 gives: "Warning 1: fill_value = null is invalid" on open.
        assert gdal.VSIStatL(filename)
        assert gdal.Open(filename)

        assert gdal.VSIStatL(newfilename)
        assert gdal.Open(newfilename)


###############################################################################


@gdaltest.enable_exceptions()
def test_zarr_multidim_compute_statistics_update_metadata(tmp_vsimem):

    filename = str(
        tmp_vsimem / "test_netcdf_multidim_compute_statistics_update_metadata.zarr"
    )

    def test():
        drv = gdal.GetDriverByName("ZARR")
        ds = drv.CreateMultiDimensional(filename)
        rg = ds.GetRootGroup()
        dim0 = rg.CreateDimension("dim0", None, None, 2)
        ar = rg.CreateMDArray(
            "ar", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_Float32)
        )
        ar.Write(array.array("f", [1.5, 2.5]))
        stats = ar.ComputeStatistics(options=["UPDATE_METADATA=YES"])
        assert stats.min == 1.5
        assert stats.max == 2.5

    def reopen():
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        ar = rg.OpenMDArray("ar")
        stats = ar.GetStatistics()
        assert stats.min == 1.5
        assert stats.max == 2.5
        attr = ar.GetAttribute("actual_range")
        assert list(attr.Read()) == [1.5, 2.5]

    test()
    reopen()


###############################################################################


@gdaltest.enable_exceptions()
def test_zarr_read_cf1():

    ds = gdal.Open("data/zarr/byte_cf1.zarr")
    assert ds
    srs = ds.GetSpatialRef()
    assert srs.ExportToProj4() == "+proj=utm +zone=11 +ellps=clrk66 +units=m +no_defs"
    assert srs.GetDataAxisToSRSAxisMapping() == [1, 2]


###############################################################################


@gdaltest.enable_exceptions()
def test_zarr_read_cf1_zarrv3():

    ds = gdal.Open("data/zarr/byte_cf1.zr3")
    assert ds
    assert (
        ds.GetSpatialRef().ExportToProj4()
        == "+proj=utm +zone=11 +ellps=clrk66 +units=m +no_defs"
    )


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.require_proj(9)
def test_zarr_write_WGS84_and_EGM96_height(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "out.zarr")
    with gdal.GetDriverByName("Zarr").Create(tmp_filename, 1, 1) as ds:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(9707)
        ds.SetSpatialRef(srs)
    with gdal.Open(tmp_filename) as ds:
        srs = ds.GetSpatialRef()
        assert srs.GetAuthorityCode(None) == "9707"
        assert srs.GetDataAxisToSRSAxisMapping() == [2, 1]


###############################################################################


@gdaltest.enable_exceptions()
def test_zarr_write_UTM31N_and_EGM96_height(tmp_vsimem):

    tmp_filename = str(tmp_vsimem / "out.zarr")
    with gdal.GetDriverByName("Zarr").Create(tmp_filename, 1, 1) as ds:
        srs = osr.SpatialReference()
        srs.SetFromUserInput("EPSG:32631+5773")
        ds.SetSpatialRef(srs)
    with gdal.Open(tmp_filename) as ds:
        srs = ds.GetSpatialRef()
        assert srs.GetDataAxisToSRSAxisMapping() == [1, 2]


###############################################################################
# Test bug fix for https://github.com/OSGeo/gdal/issues/11016


@gdaltest.enable_exceptions()
def test_zarr_write_partial_blocks_compressed(tmp_vsimem):

    out_filename = str(tmp_vsimem / "test.zarr")
    src_ds = gdal.Open("data/small_world.tif")
    gdal.Translate(
        out_filename,
        src_ds,
        options="-of ZARR -co FORMAT=ZARR_V2 -co BLOCKSIZE=3,50,50 -co COMPRESS=ZLIB -co INTERLEAVE=BAND",
    )
    out_ds = gdal.Open(out_filename)
    assert out_ds.ReadRaster() == src_ds.ReadRaster()


###############################################################################
# Test bug fix for https://github.com/OSGeo/gdal/issues/11023


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_write_cleanup_create_dir_if_bad_blocksize(tmp_path, format):

    out_dirname = str(tmp_path / "test.zarr")
    with pytest.raises(Exception):
        gdal.Translate(
            out_dirname,
            "data/byte.tif",
            options=f"-of ZARR -co FORMAT={format} -co BLOCKSIZE=1,20,20",
        )
    assert not os.path.exists(out_dirname)


###############################################################################
# Test bug fix for https://github.com/OSGeo/gdal/issues/11023


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_write_cleanup_create_dir_if_bad_blocksize_append_subdataset(
    tmp_path, format
):

    out_dirname = str(tmp_path / "test.zarr")
    gdal.Translate(out_dirname, "data/byte.tif", format="ZARR")
    assert os.path.exists(out_dirname)
    with pytest.raises(Exception):
        gdal.Translate(
            out_dirname,
            "data/utm.tif",
            options=f"-of ZARR -co APPEND_SUBDATASET=YES -co FORMAT={format} -co ARRAY_NAME=other -co BLOCKSIZE=1,20,20",
        )
    assert os.path.exists(out_dirname)
    ds = gdal.Open(out_dirname)
    assert ds.GetRasterBand(1).Checksum() == 4672


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_read_imagecodecs_tiff():
    with gdal.Open("data/zarr/uint16_imagecodecs_tiff.zarr") as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672

    with pytest.raises(
        Exception,
        match="Only decompression supported for 'imagecodecs_tiff' compression method",
    ):
        with gdal.Open("data/zarr/uint16_imagecodecs_tiff.zarr", gdal.GA_Update) as ds:
            ds.GetRasterBand(1).Fill(255)


###############################################################################
#


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "dirname",
    [
        "data/zarr/uint16_imagecodecs_tiff_invalid_tiff.zarr",
        "data/zarr/uint16_imagecodecs_tiff_inconsistent_size.zarr",
        "data/zarr/uint16_imagecodecs_tiff_too_many_bands.zarr",
    ],
)
def test_zarr_read_imagecodecs_tiff_errors(dirname):
    assert gdal.VSIStatL(dirname)
    with pytest.raises(Exception):
        with gdal.Open(dirname) as ds:
            ds.ReadRaster()


###############################################################################
#


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_write_error_at_close_on_group(tmp_path, format):
    out_filename = tmp_path / "test.zarr"

    ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
        out_filename, options=["FORMAT=" + format]
    )
    rg = ds.GetRootGroup()
    subgroup = rg.CreateGroup("subgroup")
    attr = subgroup.CreateAttribute(
        "str_attr", [], gdal.ExtendedDataType.CreateString()
    )
    assert attr.Write("my_string") == gdal.CE_None
    del attr
    del subgroup
    del rg

    gdal.RmdirRecursive(out_filename)

    with pytest.raises(Exception, match="cannot be opened for writing"):
        ds.Close()


###############################################################################
#


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_write_error_at_close_on_array(tmp_path, format):
    out_filename = tmp_path / "test.zarr"

    ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
        out_filename, options=["FORMAT=" + format]
    )
    rg = ds.GetRootGroup()
    dim0 = rg.CreateDimension("dim0", None, None, 2)

    ar = rg.CreateMDArray("my_ar", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_UInt8))
    attr = ar.CreateAttribute("str_attr", [], gdal.ExtendedDataType.CreateString())
    assert attr.Write("my_string") == gdal.CE_None
    del attr
    del ar
    del rg

    gdal.RmdirRecursive(out_filename)

    with pytest.raises(Exception, match="cannot be opened for writing"):
        ds.Close()


###############################################################################
#


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("format", ["ZARR_V2", "ZARR_V3"])
def test_zarr_write_vsizip(tmp_vsimem, format):
    out_filename = "/vsizip/" + str(tmp_vsimem) + "test.zarr.zip/test.zarr"

    gdal.GetDriverByName("Zarr").CreateCopy(
        out_filename, gdal.Open("data/byte.tif"), options=["FORMAT=" + format]
    )

    ds = gdal.Open(out_filename)
    assert ds.GetMetadata() == {"AREA_OR_POINT": "Area"}


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_read_ossfuzz_444714656():

    ds = gdal.OpenEx("/vsitar/data/zarr/ossfuzz_444714656.tar", gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg.GetGroupNames() == ["x"]


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_read_zarr_with_stac_proj_epsg():

    ds = gdal.Open("data/zarr/zarr_with_stac_proj_epsg.zarr")
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "26711"


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_read_zarr_with_stac_proj_wkt2():

    ds = gdal.Open("data/zarr/zarr_with_stac_proj_wkt2.zarr")
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "26711"


###############################################################################
#


@pytest.mark.parametrize(
    "file_path",
    [
        ("data/zarr/array_attrs.zarr/.zarray"),
        ("data/zarr/group.zarr/.zgroup"),
        ("data/zarr/group_with_zmetadata.zarr/.zmetadata"),
        ("data/zarr/v3/test.zr3/zarr.json"),
    ],
)
@gdaltest.enable_exceptions()
def test_zarr_identify_file_extensions(file_path):
    ds = gdal.Open(file_path)
    ds.GetRasterBand(1).Checksum()


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_read_zarr_v2_GetRawBlockInfo():

    ds = gdal.OpenEx("data/zarr/zlib.zarr", gdal.OF_MULTIDIM_RASTER)
    ar = ds.GetRootGroup().OpenMDArray("zlib")
    info = ar.GetRawBlockInfo([0])
    assert info.GetOffset() == 0
    assert info.GetSize() == 10
    assert info.GetFilename().replace("\\", "/") == "data/zarr/zlib.zarr/0"
    assert info.GetInfo() == ['COMPRESSOR={ "id": "zlib", "level": 1 }']

    with pytest.raises(Exception, match="invalid block coordinate"):
        ar.GetRawBlockInfo([1])


###############################################################################
#


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "filename,expected_info",
    [
        ("data/zarr/f2_le.zarr", ["ENDIANNESS=LITTLE"]),
        ("data/zarr/f2_be.zarr", ["ENDIANNESS=BIG"]),
        ("data/zarr/order_f_u2.zarr", ["ENDIANNESS=LITTLE", "TRANSPOSE_ORDER=[1,0]"]),
        ("data/zarr/v3/f2_le.zarr", ["ENDIANNESS=LITTLE"]),
        ("data/zarr/v3/f2_be.zarr", ["ENDIANNESS=BIG"]),
        (
            "data/zarr/v3/order_f_u2.zarr",
            ["ENDIANNESS=LITTLE", "TRANSPOSE_ORDER=[1,0]"],
        ),
    ],
)
def test_zarr_read_zarr_endian_or_transpose_GetRawBlockInfo(filename, expected_info):

    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    ar = ds.GetRootGroup().OpenMDArray(ds.GetRootGroup().GetMDArrayNames()[0])
    info = ar.GetRawBlockInfo([0] * ar.GetDimensionCount())
    assert info.GetInfo() == expected_info


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_read_zarr_v2_missing_block_GetRawBlockInfo(tmp_vsimem):

    ds = gdal.GetDriverByName("ZARR").CreateMultiDimensional(
        tmp_vsimem / "test.zarr", ["FORMAT=ZARR_V2"]
    )
    dim0 = ds.GetRootGroup().CreateDimension("dim0", None, None, 2)
    ar = ds.GetRootGroup().CreateMDArray(
        "test", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_UInt8)
    )
    info = ar.GetRawBlockInfo([0])
    assert info.GetOffset() == 0
    assert info.GetSize() == 0
    assert info.GetFilename() is None
    assert info.GetInfo() is None
    assert info.GetInlineData() is None


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_read_zarr_v2_kerchunk_GetRawBlockInfo():

    ds = gdal.OpenEx(
        "data/zarr/kerchunk_json/json_ref_v0_min/ref.json", gdal.OF_MULTIDIM_RASTER
    )
    ar = ds.GetRootGroup().OpenMDArray("x")
    info = ar.GetRawBlockInfo([0])
    assert info.GetOffset() == 0
    assert info.GetSize() == 1
    assert (
        info.GetFilename().replace("\\", "/")
        == "data/zarr/kerchunk_json/json_ref_v0_min/0.bin"
    )
    assert info.GetInfo() is None
    assert info.GetInlineData() is None


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_read_zarr_v3_GetRawBlockInfo():

    ds = gdal.OpenEx("data/zarr/v3/gzip.zarr", gdal.OF_MULTIDIM_RASTER)
    ar = ds.GetRootGroup().OpenMDArray("gzip")
    info = ar.GetRawBlockInfo([0, 0])
    assert info.GetOffset() == 0
    assert info.GetSize() == 25
    assert info.GetFilename().replace("\\", "/") == "data/zarr/v3/gzip.zarr/gzip/c/0/0"
    assert info.GetInfo() == [
        'COMPRESSOR={ "name": "gzip", "configuration": { "level": 6 } }'
    ]
    assert info.GetInlineData() is None

    with pytest.raises(Exception, match="invalid block coordinate"):
        ar.GetRawBlockInfo([1, 0])


###############################################################################
#


@gdaltest.enable_exceptions()
@pytest.mark.require_driver("Parquet")
def test_zarr_read_kerchunk_parquet_GetRawBlockInfo():

    ds = gdal.OpenEx(
        "ZARR:data/zarr/kerchunk_parquet/parquet_ref_0_dim_inline_content",
        gdal.OF_MULTIDIM_RASTER,
    )
    ar = ds.GetRootGroup().OpenMDArray("x")
    info = ar.GetRawBlockInfo([])
    assert info.GetOffset() == 0
    assert info.GetSize() == 1
    assert info.GetFilename() is None
    assert info.GetInfo() is None
    assert info.GetInlineData() == b"\x01"


###############################################################################
# Basic tests for a rather canonical sharded dataset, using ZSTD compression for
# inner chunks, and whose inner chunk index is protected by CRC32C.


@gdaltest.enable_exceptions()
def test_zarr_read_simple_sharding(tmp_path):

    compressors = gdal.GetDriverByName("Zarr").GetMetadataItem("COMPRESSORS")
    if "zstd" not in compressors:
        pytest.skip("compressor zstd not available")

    ds = gdal.OpenEx(
        "data/zarr/v3/simple_sharding.zarr",
        gdal.OF_MULTIDIM_RASTER,
    )
    ar = ds.GetRootGroup().OpenMDArray("simple_sharding")
    assert ar.GetBlockSize() == [5, 6]

    expected = [i for i in range(24 * 26)]

    assert list(struct.unpack("f" * (24 * 26), ar.Read())) == expected

    assert list(struct.unpack("f" * (24 * 26), ar.Read())) == expected

    assert ar.AdviseRead() == gdal.CE_None

    assert list(struct.unpack("f" * (24 * 26), ar.Read())) == expected

    info = ar.GetRawBlockInfo([0, 0])
    assert info.GetOffset() == 0
    assert info.GetSize() == 392
    assert (
        info.GetFilename().replace("\\", "/")
        == "data/zarr/v3/simple_sharding.zarr/c/0/0"
    )
    assert "CHUNK_TYPE=INNER" in info.GetInfo()

    info = ar.GetRawBlockInfo([4, 0])
    assert (
        info.GetFilename().replace("\\", "/")
        == "data/zarr/v3/simple_sharding.zarr/c/2/0"
    )

    with pytest.raises(Exception):
        ar.GetRawBlockInfo([5, 0])

    shutil.copytree(
        "data/zarr/v3/simple_sharding.zarr", tmp_path / "simple_sharding.zarr"
    )

    # Create the tile presence cache
    def open_with_cache_tile_presence_option():
        ds = gdal.OpenEx(
            tmp_path / "simple_sharding.zarr",
            gdal.OF_MULTIDIM_RASTER,
            open_options=["CACHE_TILE_PRESENCE=YES"],
        )
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg.OpenMDArray("simple_sharding") is not None

    open_with_cache_tile_presence_option()

    assert (
        gdal.VSIStatL(tmp_path / "simple_sharding.zarr" / "zarr.json.gmac") is not None
    )


###############################################################################
# Test various errors in the shard data file


@gdaltest.enable_exceptions()
def test_zarr_read_simple_sharding_read_errors(tmp_vsimem):

    compressors = gdal.GetDriverByName("Zarr").GetMetadataItem("COMPRESSORS")
    if "zstd" not in compressors:
        pytest.skip("compressor zstd not available")

    gdal.alg.vsi.copy(
        "data/zarr/v3/simple_sharding.zarr",
        tmp_vsimem,
        recursive=True,
    )

    data = bytearray(open("data/zarr/v3/simple_sharding.zarr/c/0/0", "rb").read())

    error_msgs = ""

    for pass_ in range(2):
        length_to_mutate = 1 if pass_ == 0 else 4
        for i in range(len(data) // length_to_mutate):

            # Corrupt some bytes(s)
            for k in range(length_to_mutate):
                data[i * length_to_mutate + k] = 255 - data[i * length_to_mutate + k]

            gdal.FileFromMemBuffer(tmp_vsimem / "simple_sharding.zarr/c/0/0", data)

            # Restore original values
            for k in range(length_to_mutate):
                data[i * length_to_mutate + k] = 255 - data[i * length_to_mutate + k]

            ds = gdal.OpenEx(
                tmp_vsimem / "simple_sharding.zarr",
                gdal.OF_MULTIDIM_RASTER,
            )
            ar = ds.GetRootGroup().OpenMDArray("simple_sharding")

            # Just test this does not crash
            try:
                ar.Read()
            except Exception as e:
                error_msgs += str(e) + "\n"

    assert "DecodePartial(): cannot decode chunk 0" in error_msgs
    assert "cannot read data for chunk" in error_msgs
    assert "invalid chunk location for chunk" in error_msgs


###############################################################################
# Test accessing a sharded dataset via /vsicurl/


@pytest.mark.require_curl()
@gdaltest.enable_exceptions()
def test_zarr_read_simple_sharding_network():

    compressors = gdal.GetDriverByName("Zarr").GetMetadataItem("COMPRESSORS")
    if "zstd" not in compressors:
        pytest.skip("compressor zstd not available")

    webserver_process = None
    webserver_port = 0

    webserver_process, webserver_port = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    if webserver_port == 0:
        pytest.skip()

    zarr_json = json.dumps(
        {
            "shape": [2, 2],
            "data_type": "uint8",
            "chunk_grid": {
                "name": "regular",
                "configuration": {"chunk_shape": [2, 2]},
            },
            "chunk_key_encoding": {
                "name": "default",
                "configuration": {"separator": "/"},
            },
            "fill_value": 0,
            "codecs": [
                {
                    "name": "sharding_indexed",
                    "configuration": {
                        "chunk_shape": [2, 2],
                        "codecs": [
                            {"name": "bytes", "configuration": {"endian": "little"}}
                        ],
                        "index_codecs": [
                            {"name": "bytes", "configuration": {"endian": "little"}}
                        ],
                    },
                }
            ],
            "attributes": {},
            "zarr_format": 3,
            "node_type": "array",
            "storage_transformers": [],
        }
    )
    shard_index = struct.pack("<Q", 0) + struct.pack("<Q", 4)
    shard_tail = b"\x00" * (16384 - len(shard_index)) + shard_index
    chunk_block = b"\x01\x02\x03\x04" + (16384 - 4) * b"\x00"

    try:
        with gdaltest.config_options({"GDAL_PAM_ENABLED": "NO"}):
            # Loop twice: second iteration proves ClearMemoryCaches() lets the
            # driver re-fetch everything cleanly.
            for _ in range(2):
                handler = webserver.SequentialHandler()
                handler.add("GET", "/test.zarr/", 404)
                handler.add("HEAD", "/test.zarr/.zmetadata", 404)
                # Probe zarr.json first; v2 probes (.zarray, .zgroup) skipped.
                handler.add(
                    "HEAD",
                    "/test.zarr/zarr.json",
                    200,
                    {"Content-Length": "%d" % len(zarr_json)},
                )
                handler.add(
                    "GET",
                    "/test.zarr/zarr.json",
                    200,
                    {"Content-Length": "%d" % len(zarr_json)},
                    zarr_json,
                )
                handler.add(
                    "HEAD", "/test.zarr/c/0/0", 200, {"Content-Length": "65536"}
                )
                handler.add(
                    "GET",
                    "/test.zarr/c/0/0",
                    206,
                    {
                        "Content-Length": "16384",
                        "Content-Range": "bytes 49152-65535/65536",
                    },
                    shard_tail,
                    expected_headers={"Range": "bytes=49152-65535"},
                )
                handler.add(
                    "GET",
                    "/test.zarr/c/0/0",
                    206,
                    {
                        "Content-Length": "16384",
                        "Content-Range": "bytes 0-16383/65536",
                    },
                    chunk_block,
                    expected_headers={"Range": "bytes=0-16383"},
                )
                with webserver.install_http_handler(handler):
                    ds = gdal.Open(
                        'ZARR:"/vsicurl/http://localhost:%d/test.zarr"' % webserver_port
                    )
                    assert ds.GetRasterBand(1).ReadBlock(0, 0) == b"\x01\x02\x03\x04"
                ds = None
                gdal.ClearMemoryCaches()

    finally:
        webserver.server_stop(webserver_process, webserver_port)


###############################################################################
# Test batch reads: full-array read via multidim API on a sharded dataset
# uses BatchDecodePartial (ReadMultiRange) and produces correct data.


@gdaltest.enable_exceptions()
def test_zarr_batch_reads_sharding():

    compressors = gdal.GetDriverByName("Zarr").GetMetadataItem("COMPRESSORS")
    if "zstd" not in compressors:
        pytest.skip("compressor zstd not available")

    ds = gdal.OpenEx(
        "data/zarr/v3/simple_sharding.zarr",
        gdal.OF_MULTIDIM_RASTER,
    )
    ar = ds.GetRootGroup().OpenMDArray("simple_sharding")
    assert ar.GetBlockSize() == [5, 6]

    expected = [i for i in range(24 * 26)]

    # Full-extent read triggers PreloadShardedBlocks → BatchDecodePartial.
    # Verify data matches single-block reads.
    data = list(struct.unpack("f" * (24 * 26), ar.Read()))
    assert data == expected

    # Partial read spanning multiple inner chunks within a shard
    partial = list(struct.unpack("f" * (10 * 12), ar.Read([0, 0], [10, 12])))
    for row in range(10):
        for col in range(12):
            assert partial[row * 12 + col] == expected[row * 26 + col]


###############################################################################
# Test a sharded dataset, where sharding happens after a transpose codec.


@gdaltest.enable_exceptions()
def test_zarr_read_transposed_sharding():

    compressors = gdal.GetDriverByName("Zarr").GetMetadataItem("COMPRESSORS")
    if "zstd" not in compressors:
        pytest.skip("compressor zstd not available")

    # At time of writing, zarr-python does not support yet this:
    # https://github.com/zarr-developers/zarr-python/issues/2050
    ds = gdal.OpenEx(
        "data/zarr/v3/transposed_sharding.zarr",
        gdal.OF_MULTIDIM_RASTER,
    )
    ar = ds.GetRootGroup().OpenMDArray("transposed_sharding")
    assert ar.GetBlockSize() == [5, 6]

    assert list(struct.unpack("f" * (24 * 26), ar.Read()))[0:14] == [
        0,
        24,
        48,
        72,
        96,
        120,
        144,
        168,
        192,
        216,
        240,
        264,
        10,
        34,
    ]


###############################################################################
# Test reading sharded dataset with (all) missing inner chunks and a non-zero
# fill_value.


@gdaltest.enable_exceptions()
def test_zarr_read_sharding_fill_value():

    compressors = gdal.GetDriverByName("Zarr").GetMetadataItem("COMPRESSORS")
    if "zstd" not in compressors:
        pytest.skip("compressor zstd not available")

    ds = gdal.OpenEx(
        "data/zarr/v3/sharding_fill_value.zarr",
        gdal.OF_MULTIDIM_RASTER,
    )
    ar = ds.GetRootGroup().OpenMDArray("sharding_fill_value")

    assert ar.GetBlockSize() == [1, 2]

    assert list(struct.unpack("h" * 3 * 3, ar.Read())) == [1] * (3 * 3)


###############################################################################
# Test reading a sharded dataset where both inner chunk data and inner indexe
# use big endian ordering.


@gdaltest.enable_exceptions()
def test_zarr_read_sharding_big_endian():

    # Test dataset generated with:
    #
    # import zarr
    # sharding = zarr.codecs.ShardingCodec(chunk_shape=(1, 2), codecs=[zarr.codecs.BytesCodec(endian='big')], index_codecs=[zarr.codecs.BytesCodec(endian='big'), zarr.codecs.Crc32cCodec()])
    # z = zarr.array([[1, 2],[3, 4]], chunk_shape=(2, 4), codecs=[sharding], store='sharding_big_endian.zarr')

    ds = gdal.OpenEx(
        "data/zarr/v3/sharding_big_endian.zarr",
        gdal.OF_MULTIDIM_RASTER,
    )
    ar = ds.GetRootGroup().OpenMDArray("sharding_big_endian")

    assert ar.GetBlockSize() == [1, 2]

    assert list(struct.unpack("Q" * 4, ar.Read())) == [1, 2, 3, 4]


###############################################################################
# Test reading a sharded dataset where the sharding codec is followed by a
# bytes->bytes codec, which makes partial decoding impossible, and thus falling
# back to full shard decoding.


@gdaltest.enable_exceptions()
def test_zarr_read_sharding_little_endian_then_byte_byte_codec():

    # Test dataset generated with:
    #
    # import zarr
    # sharding = zarr.codecs.ShardingCodec(chunk_shape=(1, 2), codecs=[zarr.codecs.BytesCodec(endian='little')], index_codecs=[zarr.codecs.BytesCodec(endian='little')])
    # z = zarr.array([[1, 2],[3, 4]], chunk_shape=(2, 4), codecs=[sharding, zarr.codecs.Crc32cCodec()], store='sharding_little_endian_then_byte_byte_codec.zarr')

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Sharding codec found, but not in last position. Consequently partial shard decoding will not be possible",
    ):
        ds = gdal.OpenEx(
            "data/zarr/v3/sharding_little_endian_then_byte_byte_codec.zarr",
            gdal.OF_MULTIDIM_RASTER,
        )
        ar = ds.GetRootGroup().OpenMDArray(
            "sharding_little_endian_then_byte_byte_codec"
        )

    # Not the inner shard shape
    assert ar.GetBlockSize() == [2, 4]

    assert list(struct.unpack("Q" * 4, ar.Read())) == [1, 2, 3, 4]


###############################################################################
# Same as test_zarr_read_sharding_little_endian_then_byte_byte_codec(), but
# where the byte order is big endian


@gdaltest.enable_exceptions()
def test_zarr_read_sharding_big_endian_then_byte_byte_codec():

    # Test dataset generated with:
    #
    # import zarr
    # sharding = zarr.codecs.ShardingCodec(chunk_shape=(1, 2), codecs=[zarr.codecs.BytesCodec(endian='big')], index_codecs=[zarr.codecs.BytesCodec(endian='big')])
    # z = zarr.array([[1, 2],[3, 4]], chunk_shape=(2, 4), codecs=[sharding, zarr.codecs.Crc32cCodec()], store='sharding_big_endian_then_byte_byte_codec.zarr')

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Sharding codec found, but not in last position. Consequently partial shard decoding will not be possible",
    ):
        ds = gdal.OpenEx(
            "data/zarr/v3/sharding_big_endian_then_byte_byte_codec.zarr",
            gdal.OF_MULTIDIM_RASTER,
        )
        ar = ds.GetRootGroup().OpenMDArray("sharding_big_endian_then_byte_byte_codec")

    # Not the inner shard shape
    assert ar.GetBlockSize() == [2, 4]

    assert list(struct.unpack("Q" * 4, ar.Read())) == [1, 2, 3, 4]


###############################################################################
# Test reading a *corrupted* sharded dataset where the sharding codec is followed by a
# bytes->bytes codec, which makes partial decoding impossible, and thus falling
# back to full shard decoding.


@gdaltest.enable_exceptions()
def test_zarr_read_sharding_little_endian_then_byte_byte_codec_read_errors(tmp_vsimem):

    gdal.alg.vsi.copy(
        "data/zarr/v3/sharding_little_endian_then_byte_byte_codec.zarr",
        tmp_vsimem,
        recursive=True,
    )

    data = bytearray(
        open(
            "data/zarr/v3/sharding_little_endian_then_byte_byte_codec.zarr/c/0/0", "rb"
        ).read()
    )

    # Corrupt last byte (CRC32c)
    data[-1] = 0
    gdal.FileFromMemBuffer(
        tmp_vsimem / "sharding_little_endian_then_byte_byte_codec.zarr/c/0/0", data
    )

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Sharding codec found, but not in last position. Consequently partial shard decoding will not be possible",
    ):
        ds = gdal.OpenEx(
            tmp_vsimem / "sharding_little_endian_then_byte_byte_codec.zarr",
            gdal.OF_MULTIDIM_RASTER,
        )
        ar = ds.GetRootGroup().OpenMDArray(
            "sharding_little_endian_then_byte_byte_codec"
        )

    with pytest.raises(
        Exception,
        match="CRC32C decoder: computed CRC value is B5176337 whereas expected value is 00176337",
    ):
        ar.Read()


###############################################################################
# Test reading a *corrupted* sharded dataset where the data of inner chunks
# is protected by a CRC32C


@gdaltest.enable_exceptions()
def test_zarr_read_sharding_with_crc32_then_byte_byte_codec_read_errors(tmp_vsimem):

    gdal.alg.vsi.copy(
        "data/zarr/v3/sharding_with_crc32_then_byte_byte_codec.zarr",
        tmp_vsimem,
        recursive=True,
    )

    with gdal.VSIFile(
        tmp_vsimem / "sharding_with_crc32_then_byte_byte_codec.zarr/zarr.json", "rb"
    ) as f:
        j = json.loads(f.read())
    assert j["codecs"][1]["name"] == "crc32c"
    j["codecs"][1]["configuration"] = {"check_crc": False}
    with gdal.VSIFile(
        tmp_vsimem / "sharding_with_crc32_then_byte_byte_codec.zarr/zarr.json", "wb"
    ) as f:
        f.write(bytes(json.dumps(j), encoding="ascii"))

    data = bytearray(
        open(
            "data/zarr/v3/sharding_with_crc32_then_byte_byte_codec.zarr/c/0/0", "rb"
        ).read()
    )

    error_msgs = ""

    for pass_ in range(2):
        length_to_mutate = 1 if pass_ == 0 else 4
        for i in range(len(data) // length_to_mutate):

            # Corrupt some bytes(s)
            for k in range(length_to_mutate):
                data[i * length_to_mutate + k] = 255 - data[i * length_to_mutate + k]

            gdal.FileFromMemBuffer(
                tmp_vsimem / "sharding_with_crc32_then_byte_byte_codec.zarr/c/0/0", data
            )

            # Restore original values
            for k in range(length_to_mutate):
                data[i * length_to_mutate + k] = 255 - data[i * length_to_mutate + k]

            with gdaltest.error_raised(
                gdal.CE_Warning,
                match="Sharding codec found, but not in last position. Consequently partial shard decoding will not be possible",
            ):
                ds = gdal.OpenEx(
                    tmp_vsimem / "sharding_with_crc32_then_byte_byte_codec.zarr",
                    gdal.OF_MULTIDIM_RASTER,
                )
                ar = ds.GetRootGroup().OpenMDArray(
                    "sharding_with_crc32_then_byte_byte_codec"
                )

            # Just test this does not crash
            try:
                ar.Read()
            except Exception as e:
                error_msgs += str(e) + "\n"

    assert "cannot decode chunk 0" in error_msgs
    assert "CRC32C decoder: computed CRC value" in error_msgs
    assert "invalid chunk location for chunk 0" in error_msgs


###############################################################################
# Test reading a sharded dataset where the configuration of the sharding_indexed
# codec is invalid.


def mutate_missing_configuration(j):
    del j["codecs"][0]["configuration"]


def mutate_configuration_not_object(j):
    j["codecs"][0]["configuration"] = "foo"


def mutate_missing_chunk_shape(j):
    del j["codecs"][0]["configuration"]["chunk_shape"]


def mutate_chunk_shape_not_array(j):
    j["codecs"][0]["configuration"]["chunk_shape"] = "foo"


def mutate_chunk_shape_not_right_dim_count(j):
    j["codecs"][0]["configuration"]["chunk_shape"] = [1]


def mutate_chunk_shape_minus_one_val(j):
    j["codecs"][0]["configuration"]["chunk_shape"] = [-1, 1]


def mutate_chunk_shape_zero_val(j):
    j["codecs"][0]["configuration"]["chunk_shape"] = [0, 1]


def mutate_chunk_shape_three_val(j):
    j["codecs"][0]["configuration"]["chunk_shape"] = [3, 1]


def mutate_missing_codecs(j):
    del j["codecs"][0]["configuration"]["codecs"]


def mutate_codecs_not_array(j):
    j["codecs"][0]["configuration"]["codecs"] = "foo"


def mutate_codecs_empty_array(j):
    j["codecs"][0]["configuration"]["codecs"] = []


def mutate_codecs_invalid_subcodec(j):
    j["codecs"][0]["configuration"]["codecs"] = [{"name": "invalid"}]


def mutate_missing_index_codecs(j):
    del j["codecs"][0]["configuration"]["index_codecs"]


def mutate_index_codecs_not_array(j):
    j["codecs"][0]["configuration"]["index_codecs"] = "foo"


def mutate_index_codecs_empty_array(j):
    j["codecs"][0]["configuration"]["index_codecs"] = []


def mutate_index_codecs_invalid_subcodec(j):
    j["codecs"][0]["configuration"]["index_codecs"] = [{"name": "invalid"}]


def mutate_index_codecs_unsupported_type(j):
    j["codecs"][0]["configuration"]["index_codecs"] = [
        {"name": "transpose", "configuration": {"order": [2, 1, 0]}}
    ]


def mutate_wrong_index_location(j):
    j["codecs"][0]["configuration"]["index_location"] = "middle of nowhere"


@pytest.mark.parametrize(
    "mutate_func,expected_error_msg",
    [
        (
            mutate_missing_configuration,
            "Codec sharding_indexed: configuration missing or not an object",
        ),
        (
            mutate_configuration_not_object,
            "Codec sharding_indexed: configuration missing or not an object",
        ),
        (
            mutate_missing_chunk_shape,
            "Codec sharding_indexed: configuration.chunk_shape missing or not an array",
        ),
        (
            mutate_chunk_shape_not_array,
            "Codec sharding_indexed: configuration.chunk_shape missing or not an array",
        ),
        (
            mutate_chunk_shape_not_right_dim_count,
            "Codec sharding_indexed: configuration.chunk_shape should have the same shape as the array",
        ),
        (
            mutate_chunk_shape_minus_one_val,
            r"Codec sharding_indexed: configuration.chunk_shape\[0\]=-1 should be a strictly positive value that is a divisor of 10",
        ),
        (
            mutate_chunk_shape_zero_val,
            r"Codec sharding_indexed: configuration.chunk_shape\[0\]=0 should be a strictly positive value that is a divisor of 10",
        ),
        (
            mutate_chunk_shape_three_val,
            r"Codec sharding_indexed: configuration.chunk_shape\[0\]=3 should be a strictly positive value that is a divisor of 10",
        ),
        (
            mutate_missing_codecs,
            "Codec sharding_indexed: configuration.codecs missing or not an array",
        ),
        (
            mutate_codecs_not_array,
            "Codec sharding_indexed: configuration.codecs missing or not an array",
        ),
        (
            mutate_codecs_empty_array,
            r"Codec sharding_indexed: configuration.codecs\[\] is empty",
        ),
        (
            mutate_codecs_invalid_subcodec,
            r"Codec sharding_indexed: initialization of codecs failed\nMay be caused by: Unsupported codec: invalid",
        ),
        (
            mutate_missing_index_codecs,
            "Codec sharding_indexed: configuration.index_codecs missing or not an array",
        ),
        (
            mutate_index_codecs_not_array,
            "Codec sharding_indexed: configuration.index_codecs missing or not an array",
        ),
        (
            mutate_index_codecs_empty_array,
            r"Codec sharding_indexed: configuration.index_codecs\[\] is empty",
        ),
        (
            mutate_index_codecs_invalid_subcodec,
            r"Codec sharding_indexed: initialization of index_codecs failed\nMay be caused by: Unsupported codec: invalid",
        ),
        (
            mutate_index_codecs_unsupported_type,
            "Codec sharding_indexed: this implementation only supports Bytes, possibly followed by CRC32C, as index_codecs",
        ),
        (
            mutate_wrong_index_location,
            "Codec sharding_indexed: invalid value for index_location",
        ),
    ],
)
@gdaltest.enable_exceptions()
def test_zarr_read_sharding_errors(tmp_vsimem, mutate_func, expected_error_msg):

    j = {
        "shape": [24, 26],
        "data_type": "float32",
        "chunk_grid": {"name": "regular", "configuration": {"chunk_shape": [10, 12]}},
        "chunk_key_encoding": {"name": "default", "configuration": {"separator": "/"}},
        "fill_value": 0,
        "codecs": [
            {
                "name": "sharding_indexed",
                "configuration": {
                    "chunk_shape": [5, 6],
                    "codecs": [
                        {"name": "bytes", "configuration": {"endian": "little"}}
                    ],
                    "index_codecs": [
                        {"name": "bytes", "configuration": {"endian": "little"}}
                    ],
                },
            }
        ],
        "attributes": {},
        "zarr_format": 3,
        "node_type": "array",
        "storage_transformers": [],
    }

    mutate_func(j)

    gdal.FileFromMemBuffer(tmp_vsimem / "zarr.json", json.dumps(j))

    with gdal.quiet_errors():
        with pytest.raises(Exception, match=expected_error_msg):
            gdal.OpenEx(tmp_vsimem / "zarr.json", gdal.OF_MULTIDIM_RASTER)


###############################################################################
# Test reading a sharded dataset whose inner chunks are themselves sharded...
# This is partially supported, as we fallback to full outer shard decoding
# currently.


@gdaltest.enable_exceptions()
def test_zarr_read_nested_sharding():

    # Dataset generated with:
    # import zarr
    # from zarr.codecs import ShardingCodec
    # inner_sharding = zarr.codecs.ShardingCodec(chunk_shape=(1, 2), codecs=[zarr.codecs.BytesCodec()], index_codecs=[zarr.codecs.BytesCodec()])
    # outer_sharding = zarr.codecs.ShardingCodec(chunk_shape=(2, 4), codecs=[inner_sharding], index_codecs=[zarr.codecs.BytesCodec()])
    # z = zarr.array([[0,1,2,3,4,5,6,7,8,9],[10,11,12,13,14,15,16,17,18,19],[20,21,22,23,24,25,26,27,28,29],[30,31,32,33,34,35,36,37,38,39],[40,41,42,43,44,45,46,47,48,49]], chunk_shape=(4,8), codecs=[outer_sharding], store='nested_sharding.zarr', dtype='uint16')

    # At time of writing, zarr-python does not support reading this back
    # https://github.com/zarr-developers/zarr-python/issues/3652

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Nested sharding detected. For now, partial decoding is only implemented on the outer-most shard level",
    ):
        ds = gdal.OpenEx(
            "data/zarr/v3/nested_sharding.zarr",
            gdal.OF_MULTIDIM_RASTER,
        )

    ar = ds.GetRootGroup().OpenMDArray("nested_sharding")
    assert ar.GetBlockSize() == [2, 4]

    assert list(struct.unpack("H" * (5 * 10), ar.Read())) == [i for i in range(50)]


###############################################################################
# Test reading a dataset with the "multiscales" convention


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "filename",
    [
        "data/zarr/v3/simple_multiscales/zarr.json",
        "data/zarr/v3/simple_multiscales_ref_array/zarr.json",
        "data/zarr/v3/simple_multiscales_spatial/zarr.json",
    ],
)
def test_zarr_read_simple_multiscales(filename):

    with gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER) as ds:
        rg = ds.GetRootGroup()

        level0 = rg.OpenGroup("level0")
        ar0 = level0.OpenMDArray("ar")
        assert ar0.GetOverviewCount() == 2
        assert ar0.GetOverview(-1) is None
        assert ar0.GetOverview(2) is None
        assert ar0.GetOverview(0).GetFullName() == "/level1/ar"
        assert ar0.GetOverview(1).GetFullName() == "/level2/ar"

        level1 = rg.OpenGroup("level1")
        ar1 = level1.OpenMDArray("ar")
        assert ar1.GetOverviewCount() == 1
        assert ar1.GetOverview(0).GetFullName() == "/level2/ar"

        level2 = rg.OpenGroup("level2")
        ar2 = level2.OpenMDArray("ar")
        assert ar2.GetOverviewCount() == 0

        unrelated = level0.OpenMDArray("unrelated")
        assert unrelated.GetOverviewCount() == 0

        y = level0.OpenMDArray("y")
        if "_ref_array" not in filename:
            assert y.GetOverviewCount() == 1
            assert y.GetOverview(0).GetFullName() == "/level1/y"
        else:
            assert y.GetOverviewCount() == 0

    # Without calling GetOverviewCount()
    with gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER) as ds:
        rg = ds.GetRootGroup()
        level0 = rg.OpenGroup("level0")
        ar0 = level0.OpenMDArray("ar")
        assert ar0.GetOverview(-1) is None
        assert ar0.GetOverview(2) is None
        assert ar0.GetOverview(0).GetFullName() == "/level1/ar"
        assert ar0.GetOverview(1).GetFullName() == "/level2/ar"

    filename_with_zarr_json = filename.replace("/zarr.json", "")
    with gdal.Open(f'ZARR:"{filename_with_zarr_json}":/level0/ar') as ds:
        band = ds.GetRasterBand(1)
        assert band.GetOverviewCount() == 2
        assert band.GetOverview(-1) is None
        assert band.GetOverview(2) is None

        assert band.GetOverview(0).YSize == 100
        assert band.GetOverview(0).XSize == 50

        assert band.GetOverview(1).YSize == 50
        assert band.GetOverview(1).XSize == 25


###############################################################################
# Test reading a dataset with errors in the "multiscales" convention


def mutate_multiscale_missing_layout(j):
    del j["attributes"]["multiscales"]["layout"]


def mutate_multiscale_missing_asset(j):
    j["attributes"]["multiscales"]["layout"] = [{}]


def mutate_multiscale_assert_not_existing(j):
    j["attributes"]["multiscales"]["layout"] = [{"asset": "non_existing"}]


def mutate_multiscale_asset_not_same_dim_count(j):
    j["consolidated_metadata"]["metadata"]["level1/ar"]["shape"] = [100]
    j["consolidated_metadata"]["metadata"]["level1/ar"]["chunk_grid"]["configuration"][
        "chunk_shape"
    ] = [100]
    j["consolidated_metadata"]["metadata"]["level1/ar"]["dimension_names"] = ["y"]


def mutate_multiscale_asset_not_same_type(j):
    j["consolidated_metadata"]["metadata"]["level1/ar"]["data_type"] = "uint8"
    j["consolidated_metadata"]["metadata"]["level1/ar"]["fill_value"] = 0


def mutate_multiscale_derived_from_non_existing(j):
    j["attributes"]["multiscales"]["layout"][1]["derived_from"] = "non_existing"


def mutate_multiscale_derived_from_not_same_dim_count(j):
    j["attributes"]["multiscales"]["layout"][1]["derived_from"] = "level0/y"


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "mutate_func,expected_error_msg",
    [
        (mutate_multiscale_missing_layout, "layout not found in multiscales"),
        (mutate_multiscale_missing_asset, "multiscales.layout[].asset not found"),
        (
            mutate_multiscale_assert_not_existing,
            "multiscales.layout[].asset=non_existing ignored, because it is not a valid group or array name",
        ),
        (
            mutate_multiscale_asset_not_same_dim_count,
            "multiscales.layout[].asset=level1 (/level1/ar) ignored, because it  has not the same dimension count as ar (/level0/ar)",
        ),
        (
            mutate_multiscale_asset_not_same_type,
            "multiscales.layout[].asset=level1 (/level1/ar) ignored, because it has not the same data type as ar (/level0/ar)",
        ),
        (
            mutate_multiscale_derived_from_non_existing,
            "multiscales.layout[].asset=level1 refers to derived_from=non_existing which does not exist",
        ),
        (
            mutate_multiscale_derived_from_not_same_dim_count,
            "multiscales.layout[].asset=level1 refers to derived_from=level0/y that does not have the expected number of dimensions. Ignoring that asset",
        ),
    ],
)
def test_zarr_read_simple_multiscales_error(
    tmp_vsimem, mutate_func, expected_error_msg
):

    with gdal.VSIFile("data/zarr/v3/simple_multiscales/zarr.json", "rb") as f:
        j = json.loads(f.read())

    mutate_func(j)

    gdal.FileFromMemBuffer(tmp_vsimem / "zarr.json", json.dumps(j))

    with gdal.OpenEx(tmp_vsimem / "zarr.json", gdal.OF_MULTIDIM_RASTER) as ds:
        rg = ds.GetRootGroup()
        level0 = rg.OpenGroup("level0")
        ar0 = level0.OpenMDArray("ar")
        with gdaltest.error_raised(gdal.CE_Warning, match=expected_error_msg):
            ar0.GetOverviewCount()


###############################################################################
# Test reading a dataset with errors in the "multiscales" convention


def mutate_multiscale_transform_scale_not_expected_number_of_vals(j):
    j["attributes"]["multiscales"]["layout"][1]["transform"]["scale"] = [2]


def mutate_multiscale_transform_scale_unexpected_vals(j):
    j["attributes"]["multiscales"]["layout"][1]["transform"]["scale"] = [1.5, 1.5]


def mutate_multiscale_transform_translation_not_expected_number_of_vals(j):
    j["attributes"]["multiscales"]["layout"][1]["transform"]["translation"] = [0]


def mutate_multiscale_transform_translation_unexpected_vals(j):
    j["attributes"]["multiscales"]["layout"][1]["transform"]["translation"] = [0, 0.5]


def mutate_multiscale_spatial_shape_not_array(j):
    j["attributes"]["multiscales"]["layout"][1]["spatial:shape"] = None


def mutate_multiscale_spatial_shape_not_expected_number_of_vals(j):
    j["attributes"]["multiscales"]["layout"][1]["spatial:shape"] = [100]


def mutate_multiscale_spatial_shape_not_expected_val(j):
    j["attributes"]["multiscales"]["layout"][1]["spatial:shape"] = [100, 49]


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "mutate_func,expected_error_msg",
    [
        (
            mutate_multiscale_transform_scale_not_expected_number_of_vals,
            "multiscales.layout[].asset=level1 has a transform.scale array with an unexpected number of values. Ignoring the asset",
        ),
        (
            mutate_multiscale_transform_scale_unexpected_vals,
            "multiscales.layout[].asset=level1 has a transform.scale[1]=1.500000 value whereas 2.000000 was expected. Assuming that later value as the scale.",
        ),
        (
            mutate_multiscale_transform_translation_not_expected_number_of_vals,
            "multiscales.layout[].asset=level1 has a transform.translation array with an unexpected number of values. Ignoring the asset",
        ),
        (
            mutate_multiscale_transform_translation_unexpected_vals,
            "multiscales.layout[].asset=level1 has a transform.translation[1]=0.500000 value. Ignoring that offset.",
        ),
        (
            mutate_multiscale_spatial_shape_not_array,
            "multiscales.layout[].asset=level1 ignored, because its spatial:shape property is not an array",
        ),
        (
            mutate_multiscale_spatial_shape_not_expected_number_of_vals,
            "multiscales.layout[].asset=level1 ignored, because its spatial:shape property has not the expected number of values",
        ),
        (
            mutate_multiscale_spatial_shape_not_expected_val,
            "multiscales.layout[].asset=level1 ignored, because its spatial:shape[1] value is 49 whereas 50 was expected.",
        ),
    ],
)
def test_zarr_read_simple_multiscales_spatial_error(
    tmp_vsimem, mutate_func, expected_error_msg
):

    with gdal.VSIFile("data/zarr/v3/simple_multiscales_spatial/zarr.json", "rb") as f:
        j = json.loads(f.read())

    mutate_func(j)

    gdal.FileFromMemBuffer(tmp_vsimem / "zarr.json", json.dumps(j))

    with gdal.OpenEx(tmp_vsimem / "zarr.json", gdal.OF_MULTIDIM_RASTER) as ds:
        rg = ds.GetRootGroup()
        level0 = rg.OpenGroup("level0")
        ar0 = level0.OpenMDArray("ar")
        with gdaltest.error_raised(gdal.CE_Warning, match=expected_error_msg):
            ar0.GetOverviewCount()


###############################################################################


@gdaltest.enable_exceptions()
def test_zarr_read_multiscales_cannot_get_root_group(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "zarr.json",
        gdal.VSIFile("data/zarr/v3/simple_multiscales_spatial/zarr.json", "rb").read(),
    )

    def get_array():
        with gdal.OpenEx(tmp_vsimem / "zarr.json", gdal.OF_MULTIDIM_RASTER) as ds:
            rg = ds.GetRootGroup()
            level0 = rg.OpenGroup("level0")
            ar0 = level0.OpenMDArray("ar")
            return ar0

    ar = get_array()
    gdal.Unlink(tmp_vsimem / "zarr.json")
    with gdaltest.error_raised(
        gdal.CE_Warning, match="LoadOverviews(): cannot access root group"
    ):
        assert ar.GetOverviewCount() == 0


###############################################################################
# Test multiscales overview discovery when convention is on a nested group.


@gdaltest.enable_exceptions()
def test_zarr_read_multiscales_nested_group(tmp_vsimem):

    with gdal.VSIFile("data/zarr/v3/simple_multiscales/zarr.json", "rb") as f:
        orig = json.loads(f.read())

    nested = {
        "zarr_format": 3,
        "node_type": "group",
        "attributes": {},
        "consolidated_metadata": {
            "kind": "inline",
            "must_understand": False,
            "metadata": {
                "container": {
                    "zarr_format": 3,
                    "node_type": "group",
                    "attributes": orig["attributes"],
                    "consolidated_metadata": {
                        "kind": "inline",
                        "must_understand": False,
                        "metadata": {},
                    },
                },
            },
        },
    }
    for key, val in orig["consolidated_metadata"]["metadata"].items():
        nested["consolidated_metadata"]["metadata"]["container/" + key] = val

    gdal.FileFromMemBuffer(tmp_vsimem / "zarr.json", json.dumps(nested))

    with gdal.OpenEx(tmp_vsimem / "zarr.json", gdal.OF_MULTIDIM_RASTER) as ds:
        rg = ds.GetRootGroup()
        container = rg.OpenGroup("container")
        level0 = container.OpenGroup("level0")
        ar0 = level0.OpenMDArray("ar")
        assert ar0.GetOverviewCount() == 2
        assert ar0.GetOverview(0).GetFullName() == "/container/level1/ar"
        assert ar0.GetOverview(1).GetFullName() == "/container/level2/ar"

    zarr_root = str(tmp_vsimem).rstrip("/")
    with gdal.Open(f'ZARR:"{zarr_root}":/container/level0/ar') as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() == 2


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_read_spatial_proj_at_array_level():

    ds = gdal.Open("data/zarr/v3/spatial_proj_at_array_level.zarr")
    assert ds.GetGeoTransform() == (450000, 10, 0, 5000000, 0, -10)
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "32631"
    assert ds.GetMetadata() == {"AREA_OR_POINT": "Area"}


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_read_spatial_proj_at_parent_level():

    ds = gdal.Open("data/zarr/v3/spatial_proj_at_parent_level.zarr")
    assert ds.GetGeoTransform() == (450000, 10, 0, 5000000, 0, -10)
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "32631"
    assert ds.GetMetadata() == {"AREA_OR_POINT": "Area"}


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_read_spatial_proj_convention_at_root_level():

    ds = gdal.Open("data/zarr/v3/spatial_proj_convention_at_root_level.zarr")
    assert ds.GetGeoTransform() == (450000, 10, 0, 5000000, 0, -10)
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "32631"
    assert ds.GetMetadata() == {"AREA_OR_POINT": "Area"}


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_read_spatial_geotransform():

    ds = gdal.Open("data/zarr/v3/spatial_geotransform.zarr")
    assert ds.GetGeoTransform() == (450000, 10, 0.1, 5000000, 0.15, -10)
    assert ds.GetSpatialRef() is None
    assert ds.GetMetadata() == {"AREA_OR_POINT": "Area"}


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_write_spatial_geotransform(tmp_vsimem):

    src_ds = gdal.Open("data/byte.tif")
    gdal.GetDriverByName("Zarr").CreateCopy(
        tmp_vsimem / "out.zarr",
        src_ds,
        options={"FORMAT": "ZARR_V3", "GEOREFERENCING_CONVENTION": "SPATIAL_PROJ"},
    )

    with gdal.VSIFile(tmp_vsimem / "out.zarr" / "zarr.json", "rb") as f:
        j = json.loads(f.read())

    assert j == {
        "zarr_format": 3,
        "node_type": "group",
        "attributes": {},
        "consolidated_metadata": {
            "kind": "inline",
            "must_understand": False,
            "metadata": {
                "X": {
                    "zarr_format": 3,
                    "node_type": "array",
                    "shape": [20],
                    "data_type": "float64",
                    "chunk_grid": {
                        "name": "regular",
                        "configuration": {"chunk_shape": [20]},
                    },
                    "chunk_key_encoding": {
                        "name": "default",
                        "configuration": {"separator": "/"},
                    },
                    "fill_value": "NaN",
                    "codecs": [
                        {"name": "bytes", "configuration": {"endian": "little"}}
                    ],
                    "attributes": {},
                    "dimension_names": ["X"],
                },
                "Y": {
                    "zarr_format": 3,
                    "node_type": "array",
                    "shape": [20],
                    "data_type": "float64",
                    "chunk_grid": {
                        "name": "regular",
                        "configuration": {"chunk_shape": [20]},
                    },
                    "chunk_key_encoding": {
                        "name": "default",
                        "configuration": {"separator": "/"},
                    },
                    "fill_value": "NaN",
                    "codecs": [
                        {"name": "bytes", "configuration": {"endian": "little"}}
                    ],
                    "attributes": {},
                    "dimension_names": ["Y"],
                },
                "out": {
                    "zarr_format": 3,
                    "node_type": "array",
                    "shape": [20, 20],
                    "data_type": "uint8",
                    "chunk_grid": {
                        "name": "regular",
                        "configuration": {"chunk_shape": [20, 20]},
                    },
                    "chunk_key_encoding": {
                        "name": "default",
                        "configuration": {"separator": "/"},
                    },
                    "fill_value": None,
                    "codecs": [
                        {"name": "bytes", "configuration": {"endian": "little"}}
                    ],
                    "attributes": {
                        "COLOR_INTERPRETATION": "Gray",
                        "proj:code": "EPSG:26711",
                        "spatial:bbox": [
                            440720.0,
                            3750120.0,
                            441920.0,
                            3751320.0,
                        ],
                        "spatial:transform_type": "affine",
                        "spatial:transform": [
                            60.0,
                            0.0,
                            440720.0,
                            0.0,
                            -60.0,
                            3751320.0,
                        ],
                        "spatial:registration": "pixel",
                        "spatial:dimensions": ["Y", "X"],
                        "zarr_conventions": [
                            {
                                "schema_url": "https://raw.githubusercontent.com/zarr-experimental/geo-proj/refs/tags/v1/schema.json",
                                "spec_url": "https://github.com/zarr-experimental/geo-proj/blob/v1/README.md",
                                "uuid": "f17cb550-5864-4468-aeb7-f3180cfb622f",
                                "name": "proj:",
                                "description": "Coordinate reference system information for geospatial data",
                            },
                            {
                                "schema_url": "https://raw.githubusercontent.com/zarr-conventions/spatial/refs/tags/v1/schema.json",
                                "spec_url": "https://github.com/zarr-conventions/spatial/blob/v1/README.md",
                                "uuid": "689b58e2-cf7b-45e0-9fff-9cfc0883d6b4",
                                "name": "spatial:",
                                "description": "Spatial coordinate information",
                            },
                        ],
                    },
                    "dimension_names": ["Y", "X"],
                },
            },
        },
    }

    # Remove CF-like X and Y coordinate variables, to be sure we read from
    # spatial:transform
    gdal.RmdirRecursive(tmp_vsimem / "out.zarr" / "X")
    gdal.RmdirRecursive(tmp_vsimem / "out.zarr" / "Y")

    ds = gdal.Open(tmp_vsimem / "out.zarr")
    assert ds.GetSpatialRef().IsSame(src_ds.GetSpatialRef())
    assert ds.GetGeoTransform() == src_ds.GetGeoTransform()
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()


###############################################################################
#


@gdaltest.enable_exceptions()
def test_zarr_write_spatial_geotransform_no_epsg_code_rotated_gt_and_pixel_center(
    tmp_vsimem,
):

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 3)
    src_ds.SetGeoTransform([1, 2, 3, 4, 5, 6])
    src_ds.SetSpatialRef(
        osr.SpatialReference("+proj=longlat +ellps=GRS80 +towgs84=0,0,0")
    )
    src_ds.SetMetadataItem("AREA_OR_POINT", "Point")

    gdal.GetDriverByName("Zarr").CreateCopy(
        tmp_vsimem / "out.zarr",
        src_ds,
        options={"FORMAT": "ZARR_V3", "GEOREFERENCING_CONVENTION": "SPATIAL_PROJ"},
    )

    with gdal.VSIFile(tmp_vsimem / "out.zarr" / "zarr.json", "rb") as f:
        j = json.loads(f.read())

    assert j["consolidated_metadata"]["metadata"]["out"]["attributes"][
        "proj:wkt2"
    ].startswith("BOUNDCRS")
    assert (
        j["consolidated_metadata"]["metadata"]["out"]["attributes"]["proj:projjson"][
            "type"
        ]
        == "BoundCRS"
    )

    # Set those 2 elements to dummy value for comparison, as their content might be PROJ dependent
    j["consolidated_metadata"]["metadata"]["out"]["attributes"][
        "proj:wkt2"
    ] = "redacted"
    j["consolidated_metadata"]["metadata"]["out"]["attributes"][
        "proj:projjson"
    ] = "redacted"

    assert j == {
        "zarr_format": 3,
        "node_type": "group",
        "attributes": {},
        "consolidated_metadata": {
            "kind": "inline",
            "must_understand": False,
            "metadata": {
                "out": {
                    "zarr_format": 3,
                    "node_type": "array",
                    "shape": [3, 2],
                    "data_type": "uint8",
                    "chunk_grid": {
                        "name": "regular",
                        "configuration": {"chunk_shape": [3, 2]},
                    },
                    "chunk_key_encoding": {
                        "name": "default",
                        "configuration": {"separator": "/"},
                    },
                    "fill_value": None,
                    "codecs": [
                        {"name": "bytes", "configuration": {"endian": "little"}}
                    ],
                    "attributes": {
                        "proj:wkt2": "redacted",
                        "proj:projjson": "redacted",
                        "spatial:bbox": [3.5, 9.5, 11.5, 26.5],
                        "spatial:transform_type": "affine",
                        "spatial:transform": [2.0, 3.0, 3.5, 5.0, 6.0, 9.5],
                        "spatial:dimensions": ["Y", "X"],
                        "spatial:registration": "node",
                        "zarr_conventions": [
                            {
                                "schema_url": "https://raw.githubusercontent.com/zarr-experimental/geo-proj/refs/tags/v1/schema.json",
                                "spec_url": "https://github.com/zarr-experimental/geo-proj/blob/v1/README.md",
                                "uuid": "f17cb550-5864-4468-aeb7-f3180cfb622f",
                                "name": "proj:",
                                "description": "Coordinate reference system information for geospatial data",
                            },
                            {
                                "schema_url": "https://raw.githubusercontent.com/zarr-conventions/spatial/refs/tags/v1/schema.json",
                                "spec_url": "https://github.com/zarr-conventions/spatial/blob/v1/README.md",
                                "uuid": "689b58e2-cf7b-45e0-9fff-9cfc0883d6b4",
                                "name": "spatial:",
                                "description": "Spatial coordinate information",
                            },
                        ],
                    },
                    "dimension_names": ["Y", "X"],
                },
            },
        },
    }

    ds = gdal.Open(tmp_vsimem / "out.zarr")
    assert ds.GetSpatialRef().IsSame(src_ds.GetSpatialRef())
    assert ds.GetGeoTransform() == src_ds.GetGeoTransform()
    assert ds.GetMetadataItem("AREA_OR_POINT") == src_ds.GetMetadataItem(
        "AREA_OR_POINT"
    )
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()


###############################################################################
#

eopf_sample_service_zarray = {
    "zarr_format": 2,
    "shape": [1, 1],
    "chunks": [1, 1],
    "dtype": "<u2",
    "fill_value": 0,
    "filters": None,
    "compressor": None,
    "order": "C",
}
eopf_sample_service_zattrs = {"_eopf_attrs": {}}

eopf_sample_service_zgroup = {"zarr_format": 2}


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "filename,j",
    [
        (
            "test.zarr",
            {
                "metadata": {
                    ".zattrs": {"stac_discovery": {"properties": {"proj:epsg": 32632}}},
                    ".zgroup": eopf_sample_service_zgroup,
                    "myar/.zarray": eopf_sample_service_zarray,
                    "myar/.zattrs": eopf_sample_service_zattrs,
                }
            },
        ),
        (
            "test.zarr",
            {
                "metadata": {
                    ".zattrs": {
                        "stac_discovery": {"properties": {"proj:epsg": "32632"}}
                    },
                    ".zgroup": eopf_sample_service_zgroup,
                    "myar/.zarray": eopf_sample_service_zarray,
                    "myar/.zattrs": eopf_sample_service_zattrs,
                }
            },
        ),
        (
            "test.zarr",
            {
                "metadata": {
                    ".zattrs": {"proj:epsg": 32632},
                    ".zgroup": eopf_sample_service_zgroup,
                    "myar/.zarray": eopf_sample_service_zarray,
                    "myar/.zattrs": eopf_sample_service_zattrs,
                }
            },
        ),
        (
            "test.zarr",
            {
                "metadata": {
                    ".zattrs": {"proj:epsg": "32632"},
                    ".zgroup": eopf_sample_service_zgroup,
                    "myar/.zarray": eopf_sample_service_zarray,
                    "myar/.zattrs": eopf_sample_service_zattrs,
                }
            },
        ),
        (
            "test.zarr",
            {
                "metadata": {
                    ".zattrs": {"some_child": {"proj:epsg": 32632}},
                    ".zgroup": eopf_sample_service_zgroup,
                    "myar/.zarray": eopf_sample_service_zarray,
                    "myar/.zattrs": eopf_sample_service_zattrs,
                }
            },
        ),
        (
            "test.zarr",
            {
                "metadata": {
                    ".zattrs": {"some_child": {"proj:epsg": "32632"}},
                    ".zgroup": eopf_sample_service_zgroup,
                    "myar/.zarray": eopf_sample_service_zarray,
                    "myar/.zattrs": eopf_sample_service_zattrs,
                }
            },
        ),
        (
            "test.zarr",
            {
                "metadata": {
                    ".zattrs": {
                        "stac_discovery": {
                            "geometry": {"crs": {"properties": {"code": 32632}}}
                        }
                    },
                    ".zgroup": eopf_sample_service_zgroup,
                    "myar/.zarray": eopf_sample_service_zarray,
                    "myar/.zattrs": eopf_sample_service_zattrs,
                }
            },
        ),
        (
            "foo_T32VLK_bla.zarr",
            {
                "metadata": {
                    ".zgroup": eopf_sample_service_zgroup,
                    "myar/.zarray": eopf_sample_service_zarray,
                    "myar/.zattrs": eopf_sample_service_zattrs,
                }
            },
        ),
        (
            "test.zarr",
            {
                "metadata": {
                    ".zattrs": {
                        "stac_discovery": {"properties": {"s2:mgrs_tile": "T32VLK"}}
                    },
                    ".zgroup": eopf_sample_service_zgroup,
                    "myar/.zarray": eopf_sample_service_zarray,
                    "myar/.zattrs": eopf_sample_service_zattrs,
                }
            },
        ),
        (
            "test.zarr",
            {
                "metadata": {
                    ".zattrs": {
                        "stac_discovery": {"properties": {"mgrs_tile": "T32VLK"}}
                    },
                    ".zgroup": eopf_sample_service_zgroup,
                    "myar/.zarray": eopf_sample_service_zarray,
                    "myar/.zattrs": eopf_sample_service_zattrs,
                }
            },
        ),
        (
            "test.zarr",
            {
                "metadata": {
                    ".zattrs": {
                        "stac_discovery": {"properties": {"tile_id": "T32VLK"}}
                    },
                    ".zgroup": eopf_sample_service_zgroup,
                    "myar/.zarray": eopf_sample_service_zarray,
                    "myar/.zattrs": eopf_sample_service_zattrs,
                }
            },
        ),
        (
            "test.zarr",
            {
                "metadata": {
                    ".zattrs": {
                        "spatial_ref": 'PROJCS["WGS 84 / UTM zone 32N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",9],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","32632"]]'
                    },
                    ".zgroup": eopf_sample_service_zgroup,
                    "myar/.zarray": eopf_sample_service_zarray,
                    "myar/.zattrs": eopf_sample_service_zattrs,
                }
            },
        ),
        (
            "test.zarr",
            {
                "metadata": {
                    ".zattrs": {
                        "stac_discovery": {
                            "properties": {
                                "spatial_ref": 'PROJCS["WGS 84 / UTM zone 32N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",9],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","32632"]]'
                            }
                        }
                    },
                    ".zgroup": eopf_sample_service_zgroup,
                    "myar/.zarray": eopf_sample_service_zarray,
                    "myar/.zattrs": eopf_sample_service_zattrs,
                }
            },
        ),
    ],
)
def test_zarr_read_srs_eopf_sample_service(tmp_vsimem, filename, j):

    gdal.FileFromMemBuffer(tmp_vsimem / filename / ".zmetadata", json.dumps(j))
    gdal.FileFromMemBuffer(tmp_vsimem / filename / ".zgroup", '{"zarr_format": 2}')

    ds = gdal.Open(tmp_vsimem / filename)
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "32632"
