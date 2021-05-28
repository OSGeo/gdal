#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Zarr driver
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
import base64
import json
import math
import struct

from osgeo import gdal

import gdaltest
import pytest


@pytest.mark.parametrize("dtype,structtype,gdaltype,fill_value,nodata_value",
                         [["!b1", 'B', gdal.GDT_Byte, None, None],
                          ["!u1", 'B', gdal.GDT_Byte, None, None],
                          ["!u1", 'B', gdal.GDT_Byte, 1, 1],
                          ["<i2", 'h', gdal.GDT_Int16, None, None],
                          [">i2", 'h', gdal.GDT_Int16, None, None],
                          ["<i4", 'i', gdal.GDT_Int32, None, None],
                          [">i4", 'i', gdal.GDT_Int32, None, None],
                          ["<u2", 'H', gdal.GDT_UInt16, None, None],
                          [">u2", 'H', gdal.GDT_UInt16, None, None],
                          ["<u4", 'I', gdal.GDT_UInt32, None, None],
                          [">u4", 'I', gdal.GDT_UInt32, None, None],
                          ["<u4", 'I', gdal.GDT_UInt32, 4000000000, 4000000000],
                          ["<f4", 'f', gdal.GDT_Float32, None, None],
                          [">f4", 'f', gdal.GDT_Float32, None, None],
                          ["<f4", 'f', gdal.GDT_Float32, 1.5, 1.5],
                          ["<f4", 'f', gdal.GDT_Float32, "NaN", float('nan')],
                          ["<f4", 'f', gdal.GDT_Float32,
                           "Infinity", float('infinity')],
                          ["<f4", 'f', gdal.GDT_Float32,
                           "-Infinity", float('-infinity')],
                          ["<f8", 'd', gdal.GDT_Float64, None, None],
                          [">f8", 'd', gdal.GDT_Float64, None, None],
                          ["<f8", 'd', gdal.GDT_Float64, "NaN", float('nan')],
                          ["<f8", 'd', gdal.GDT_Float64,
                           "Infinity", float('infinity')],
                          ["<f8", 'd', gdal.GDT_Float64,
                           "-Infinity", float('-infinity')],
                          ["<c8", 'f', gdal.GDT_CFloat32, None, None],
                          [">c8", 'f', gdal.GDT_CFloat32, None, None],
                          ["<c16", 'd', gdal.GDT_CFloat64, None, None],
                          [">c16", 'd', gdal.GDT_CFloat64, None, None]])
@pytest.mark.parametrize("use_optimized_code_paths", [True, False])
def test_zarr_basic(dtype, structtype, gdaltype, fill_value, nodata_value, use_optimized_code_paths):

    j = {
        "chunks": [
            2,
            3
        ],
        "compressor": None,
        "dtype": dtype,
        "fill_value": fill_value,
        "filters": None,
        "order": "C",
        "shape": [
            5,
            4
        ],
        "zarr_format": 2
    }

    try:
        gdal.Mkdir('/vsimem/test.zarr', 0)
        gdal.FileFromMemBuffer('/vsimem/test.zarr/.zarray', json.dumps(j))
        if gdaltype not in (gdal.GDT_CFloat32, gdal.GDT_CFloat64):
            tile_0_0_data = struct.pack(
                dtype[0] + (structtype * 6), 1, 2, 3, 5, 6, 7)
            tile_0_1_data = struct.pack(
                dtype[0] + (structtype * 6), 4, 0, 0, 8, 0, 0)
        else:
            tile_0_0_data = struct.pack(
                dtype[0] + (structtype * 12), 1, 11, 2, 0, 3, 0, 5, 0, 6, 0, 7, 0)
            tile_0_1_data = struct.pack(
                dtype[0] + (structtype * 12), 4, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0)
        gdal.FileFromMemBuffer('/vsimem/test.zarr/0.0', tile_0_0_data)
        gdal.FileFromMemBuffer('/vsimem/test.zarr/0.1', tile_0_1_data)
        with gdaltest.config_option('GDAL_ZARR_USE_OPTIMIZED_CODE_PATHS',
                                    'YES' if use_optimized_code_paths else 'NO'):
            ds = gdal.OpenEx('/vsimem/test.zarr', gdal.OF_MULTIDIM_RASTER)
            assert ds
            rg = ds.GetRootGroup()
            assert rg
            ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar
        assert ar.GetDimensionCount() == 2
        assert [ar.GetDimensions()[i].GetSize() for i in range(2)] == [5, 4]
        assert ar.GetBlockSize() == [2, 3]
        if nodata_value is not None and math.isnan(nodata_value):
            assert math.isnan(ar.GetNoDataValueAsDouble())
        else:
            assert ar.GetNoDataValueAsDouble() == nodata_value

        # Check reading one single value
        assert ar[1, 2].Read(buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == \
            struct.pack('d' * 1, 7)

        # Read block 0,0
        if gdaltype not in (gdal.GDT_CFloat32, gdal.GDT_CFloat64):
            assert ar[0:2, 0:3].Read(buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == \
                struct.pack('d' * 6, 1, 2, 3, 5, 6, 7)
            assert struct.unpack(
                structtype * 6, ar[0:2, 0:3].Read()) == (1, 2, 3, 5, 6, 7)
        else:
            assert ar[0:2, 0:3].Read(buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_CFloat64)) == \
                struct.pack('d' * 12, 1, 11, 2, 0, 3, 0, 5, 0, 6, 0, 7, 0)
            assert struct.unpack(
                structtype * 12, ar[0:2, 0:3].Read()) == (1, 11, 2, 0, 3, 0, 5, 0, 6, 0, 7, 0)

        # Read block 0,1
        assert ar[0:2, 3:4].Read(buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == \
            struct.pack('d' * 2, 4, 8)

        # Read block 1,1 (missing)
        nv = nodata_value if nodata_value else 0
        assert ar[2:4, 3:4].Read(buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == \
            struct.pack('d' * 2, nv, nv)

        # Read whole raster
        assert ar.Read(buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == \
            struct.pack('d' * 20,
                        1, 2, 3, 4,
                        5, 6, 7, 8,
                        nv, nv, nv, nv,
                        nv, nv, nv, nv,
                        nv, nv, nv, nv)

        if gdaltype not in (gdal.GDT_CFloat32, gdal.GDT_CFloat64):
            assert ar.Read() == array.array(structtype, [1, 2, 3, 4,
                                                         5, 6, 7, 8,
                                                         nv, nv, nv, nv,
                                                         nv, nv, nv, nv,
                                                         nv, nv, nv, nv])
        else:
            assert ar.Read() == array.array(structtype, [1, 11, 2, 0, 3, 0, 4, 0,
                                                         5, 0, 6, 0, 7, 0, 8, 0,
                                                         nv, 0, nv, 0, nv, 0, nv, 0,
                                                         nv, 0, nv, 0, nv, 0, nv, 0,
                                                         nv, 0, nv, 0, nv, 0, nv, 0])
        # Read with negative steps
        assert ar.Read(array_start_idx=[2, 1],
                       count=[2, 2],
                       array_step=[-1, -1],
                       buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == \
            struct.pack('d' * 4, nv, nv, 6, 5)

        # array_step > 2
        assert ar.Read(array_start_idx=[0, 0],
                       count=[1, 2],
                       array_step=[0, 2],
                       buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == \
            struct.pack('d' * 2, 1, 3)

        assert ar.Read(array_start_idx=[0, 0],
                       count=[3, 1],
                       array_step=[2, 0],
                       buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == \
            struct.pack('d' * 3, 1, nv, nv)

        assert ar.Read(array_start_idx=[0, 1],
                       count=[1, 2],
                       array_step=[0, 2],
                       buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == \
            struct.pack('d' * 2, 2, 4)

        assert ar.Read(array_start_idx=[0, 0],
                       count=[1, 2],
                       array_step=[0, 3],
                       buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == \
            struct.pack('d' * 2, 1, 4)

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


@pytest.mark.parametrize("fill_value,expected_read_data", [[base64.b64encode(b'xyz').decode('utf-8'), ['abc', 'xyz']],
                                                           [None, ['abc', None]]])
def test_zarr_string(fill_value, expected_read_data):

    j = {
        "chunks": [
            1
        ],
        "compressor": None,
        "dtype": '|S3',
        "fill_value": fill_value,
        "filters": None,
        "order": "C",
        "shape": [
            2
        ],
        "zarr_format": 2
    }

    try:
        gdal.Mkdir('/vsimem/test.zarr', 0)
        gdal.FileFromMemBuffer('/vsimem/test.zarr/.zarray', json.dumps(j))
        gdal.FileFromMemBuffer('/vsimem/test.zarr/0', b'abc')
        ds = gdal.OpenEx('/vsimem/test.zarr', gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar
        assert ar.Read() == expected_read_data

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


# Check that all required elements are present in .zarray
@pytest.mark.parametrize("member",
                         [None, 'zarr_format', 'chunks', 'compressor', 'dtype',
                          'filters', 'order', 'shape'])
def test_zarr_invalid_json_remove_member(member):

    j = {
        "chunks": [
            2,
            3
        ],
        "compressor": None,
        "dtype": '!b1',
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [
            5,
            4
        ],
        "zarr_format": 2
    }

    if member:
        del j[member]

    try:
        gdal.Mkdir('/vsimem/test.zarr', 0)
        gdal.FileFromMemBuffer('/vsimem/test.zarr/.zarray', json.dumps(j))
        with gdaltest.error_handler():
            ds = gdal.OpenEx('/vsimem/test.zarr', gdal.OF_MULTIDIM_RASTER)
        if member is None:
            assert ds
        else:
            assert ds is None
    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


# Check bad values of members in .zarray
@pytest.mark.parametrize("dict_update", [{"chunks": None},
                                         {"chunks": "invalid"},
                                         {"chunks": [2]},
                                         {"chunks": [2, 0]},
                                         {"shape": None},
                                         {"shape": "invalid"},
                                         {"shape": [5]},
                                         {"shape": [5, 0]},
                                         {"chunks": [], "shape": []},
                                         {"chunks": [1 << 40, 1 << 40],
                                          "shape": [1 << 40, 1 << 40]},
                                         {"dtype": None},
                                         {"dtype": 1},
                                         {"dtype": ""},
                                         {"dtype": "!"},
                                         {"dtype": "!b"},
                                         {"dtype": "<u16"},
                                         {"fill_value": []},
                                         {"fill_value": "x"},
                                         {"fill_value": "NaN"},
                                         {"dtype": "!S1", "fill_value": 0},
                                         {"order": None},
                                         {"order": "invalid"},
                                         {"zarr_format": None},
                                         {"zarr_format": 1},
                                         ])
def test_zarr_invalid_json_wrong_values(dict_update):

    j = {
        "chunks": [
            2,
            3
        ],
        "compressor": None,
        "dtype": '!b1',
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [
            5,
            4
        ],
        "zarr_format": 2
    }

    j.update(dict_update)

    try:
        gdal.Mkdir('/vsimem/test.zarr', 0)
        gdal.FileFromMemBuffer('/vsimem/test.zarr/.zarray', json.dumps(j))
        with gdaltest.error_handler():
            ds = gdal.OpenEx('/vsimem/test.zarr', gdal.OF_MULTIDIM_RASTER)
        assert ds is None
    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')
