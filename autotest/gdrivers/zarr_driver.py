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
import sys

from osgeo import gdal
from osgeo import osr

import gdaltest
import pytest


@pytest.mark.parametrize("dtype,structtype,gdaltype,fill_value,nodata_value",
                         [["!b1", 'B', gdal.GDT_Byte, None, None],
                          ["!i1", 'b', gdal.GDT_Int16, None, None],
                          ["!i1", 'b', gdal.GDT_Int16, -1, -1],
                          ["!u1", 'B', gdal.GDT_Byte, None, None],
                          ["!u1", 'B', gdal.GDT_Byte, 1, 1],
                          ["<i2", 'h', gdal.GDT_Int16, None, None],
                          [">i2", 'h', gdal.GDT_Int16, None, None],
                          ["<i4", 'i', gdal.GDT_Int32, None, None],
                          [">i4", 'i', gdal.GDT_Int32, None, None],
                          ["<i8", 'q', gdal.GDT_Float64, None, None],
                          [">i8", 'q', gdal.GDT_Float64, None, None],
                          ["<u2", 'H', gdal.GDT_UInt16, None, None],
                          [">u2", 'H', gdal.GDT_UInt16, None, None],
                          ["<u4", 'I', gdal.GDT_UInt32, None, None],
                          [">u4", 'I', gdal.GDT_UInt32, None, None],
                          ["<u4", 'I', gdal.GDT_UInt32, 4000000000, 4000000000],
                          ["<u8", 'Q', gdal.GDT_Float64, 4000000000, 4000000000],
                          [">u8", 'Q', gdal.GDT_Float64, None, None],
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

        if structtype == 'b':
            structtype_read = 'h'
        elif structtype in ('q', 'Q'):
            structtype_read = 'd'
        else:
            structtype_read = structtype

        # Read block 0,0
        if gdaltype not in (gdal.GDT_CFloat32, gdal.GDT_CFloat64):
            assert ar[0:2, 0:3].Read(buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == \
                struct.pack('d' * 6, 1, 2, 3, 5, 6, 7)
            assert struct.unpack(
                structtype_read * 6, ar[0:2, 0:3].Read()) == (1, 2, 3, 5, 6, 7)
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
            assert ar.Read() == array.array(structtype_read, [1, 2, 3, 4,
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
                                         {"compressor": "invalid"},
                                         {"compressor": {}},
                                         {"compressor": {"id": "invalid"}},
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


# Check reading different compression methods
@pytest.mark.parametrize("datasetname,compressor", [('blosc.zarr', 'blosc'),
                                                    ('gzip.zarr', 'gzip'),
                                                    ('lz4.zarr', 'lz4'),
                                                    ('lzma.zarr', 'lzma'),
                                                    ('lzma_with_filters.zarr',
                                                     'lzma'),
                                                    ('zlib.zarr', 'zlib'),
                                                    ('zstd.zarr', 'zstd'),
                                                    ])
def test_zarr_read_compression_methods(datasetname, compressor):

    compressors = gdal.GetDriverByName('Zarr').GetMetadataItem('COMPRESSORS')
    filename = 'data/zarr/' + datasetname

    if compressor not in compressors:
        with gdaltest.error_handler():
            ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        assert ds is None
    else:
        ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar
        assert ar.Read() == array.array('b', [1, 2])


@pytest.mark.parametrize("name", ["u1", "u2", "u4", "u8"])
def test_zarr_read_fortran_order(name):

    filename = 'data/zarr/order_f_' + name + '.zarr'
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read(buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Byte)) == \
        array.array('b', [i for i in range(16)])


def test_zarr_read_fortran_order_string():

    filename = 'data/zarr/order_f_s3.zarr'
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read() == ['000', '111', '222', '333',
                         '444', '555', '666', '777',
                         '888', '999', 'AAA', 'BBB',
                         'CCC', 'DDD', 'EEE', 'FFF']


def test_zarr_read_fortran_order_3d():

    filename = 'data/zarr/order_f_u1_3d.zarr'
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read(buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Byte)) == \
        array.array('b', [i for i in range(2 * 3 * 4)])


def test_zarr_read_compound_well_aligned():

    filename = 'data/zarr/compound_well_aligned.zarr'
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
    assert comps[0].GetName() == 'a'
    assert comps[0].GetOffset() == 0
    assert comps[0].GetType().GetNumericDataType() == gdal.GDT_UInt16
    assert comps[1].GetName() == 'b'
    assert comps[1].GetOffset() == 2
    assert comps[1].GetType().GetNumericDataType() == gdal.GDT_UInt16

    assert ar['a'].Read() == array.array('H', [1000, 4000, 0])
    assert ar['b'].Read() == array.array('H', [3000, 5000, 0])

    j = gdal.MultiDimInfo(ds, detailed=True)
    assert j['arrays']['compound_well_aligned']['values'] == [
        {"a": 1000, "b": 3000},
        {"a": 4000, "b": 5000},
        {"a": 0, "b": 0}]


def test_zarr_read_compound_not_aligned():

    filename = 'data/zarr/compound_not_aligned.zarr'
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
    assert comps[0].GetName() == 'a'
    assert comps[0].GetOffset() == 0
    assert comps[0].GetType().GetNumericDataType() == gdal.GDT_UInt16
    assert comps[1].GetName() == 'b'
    assert comps[1].GetOffset() == 2
    assert comps[1].GetType().GetNumericDataType() == gdal.GDT_Byte
    assert comps[2].GetName() == 'c'
    assert comps[2].GetOffset() == 4
    assert comps[2].GetType().GetNumericDataType() == gdal.GDT_UInt16

    assert ar['a'].Read() == array.array('H', [1000, 4000, 0])
    assert ar['b'].Read() == array.array('B', [2, 4, 0])
    assert ar['c'].Read() == array.array('H', [3000, 5000, 0])

    j = gdal.MultiDimInfo(ds, detailed=True)
    assert j['arrays']['compound_not_aligned']['values'] == [
        {"a": 1000, "b": 2, "c": 3000},
        {"a": 4000, "b": 4, "c": 5000},
        {"a": 0, "b": 0, "c": 0}]


def test_zarr_read_compound_complex():

    filename = 'data/zarr/compound_complex.zarr'
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
    assert comps[0].GetName() == 'a'
    assert comps[0].GetOffset() == 0
    assert comps[0].GetType().GetNumericDataType() == gdal.GDT_Byte
    assert comps[1].GetName() == 'b'
    assert comps[1].GetOffset() == 2
    assert comps[1].GetType().GetClass() == gdal.GEDTC_COMPOUND
    assert comps[1].GetType().GetSize() == 1 + 1 + 2 + \
        1 + 1  # last one is padding

    subcomps = comps[1].GetType().GetComponents()
    assert len(subcomps) == 4

    assert comps[2].GetName() == 'c'
    assert comps[2].GetOffset() == 8
    assert comps[2].GetType().GetClass() == gdal.GEDTC_STRING
    assert comps[3].GetName() == 'd'
    assert comps[3].GetOffset() == 16 if is_64bit else 12
    assert comps[3].GetType().GetNumericDataType() == gdal.GDT_Int16

    j = gdal.MultiDimInfo(ds, detailed=True)
    assert j['arrays']['compound_complex']['values'] == [
        {"a": 1, "b": {"b1": 2, "b2": 3, "b3": 1000, "b5": 4}, "c": "AAA", "d": -1},
        {"a": 2, "b": {"b1": 255, "b2": 254, "b3": 65534, "b5": 253}, "c": "ZZ", "d": -2}]


def test_zarr_read_array_attributes():

    filename = 'data/zarr/array_attrs.zarr'
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None

    j = gdal.MultiDimInfo(ds)
    assert j['arrays']['array_attrs']['attributes'] == {
        "bool": "true",
        "double": 1.5,
        "doublearray": [1.5, 2.5],
        "int": 1,
        "int64": 1234567890123,
        "int64array": [1234567890123, -1234567890123],
        "intarray": [1, 2],
        "intdoublearray": [1, 2.5],
        "mixedstrintarray": "[ \"foo\", 1 ]",
        "null": "",
        "obj": "{ }",
        "str": "foo",
        "strarray": ["foo", "bar"]
    }


@pytest.mark.parametrize("crs_member", ["projjson", "wkt", "url"])
def test_zarr_read_crs(crs_member):

    zarray = {
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

    zattrs_all = {
        "crs": {
            "projjson": {
                "$schema": "https://proj.org/schemas/v0.2/projjson.schema.json",
                "type": "GeographicCRS",
                "name": "WGS 84",
                "datum_ensemble": {
                    "name": "World Geodetic System 1984 ensemble",
                    "members": [
                        {
                            "name": "World Geodetic System 1984 (Transit)",
                            "id": {
                                "authority": "EPSG",
                                "code": 1166
                            }
                        },
                        {
                            "name": "World Geodetic System 1984 (G730)",
                            "id": {
                                "authority": "EPSG",
                                "code": 1152
                            }
                        },
                        {
                            "name": "World Geodetic System 1984 (G873)",
                            "id": {
                                "authority": "EPSG",
                                "code": 1153
                            }
                        },
                        {
                            "name": "World Geodetic System 1984 (G1150)",
                            "id": {
                                "authority": "EPSG",
                                "code": 1154
                            }
                        },
                        {
                            "name": "World Geodetic System 1984 (G1674)",
                            "id": {
                                "authority": "EPSG",
                                "code": 1155
                            }
                        },
                        {
                            "name": "World Geodetic System 1984 (G1762)",
                            "id": {
                                "authority": "EPSG",
                                "code": 1156
                            }
                        }
                    ],
                    "ellipsoid": {
                        "name": "WGS 84",
                        "semi_major_axis": 6378137,
                        "inverse_flattening": 298.257223563
                    },
                    "accuracy": "2.0",
                    "id": {
                        "authority": "EPSG",
                        "code": 6326
                    }
                },
                "coordinate_system": {
                    "subtype": "ellipsoidal",
                    "axis": [
                        {
                            "name": "Geodetic latitude",
                            "abbreviation": "Lat",
                            "direction": "north",
                            "unit": "degree"
                        },
                        {
                            "name": "Geodetic longitude",
                            "abbreviation": "Lon",
                            "direction": "east",
                            "unit": "degree"
                        }
                    ]
                },
                "scope": "Horizontal component of 3D system.",
                "area": "World.",
                "bbox": {
                    "south_latitude": -90,
                    "west_longitude": -180,
                    "north_latitude": 90,
                    "east_longitude": 180
                },
                "id": {
                    "authority": "EPSG",
                    "code": 4326
                }
            },
            "wkt": 'GEOGCRS["WGS 84",ENSEMBLE["World Geodetic System 1984 ensemble",MEMBER["World Geodetic System 1984 (Transit)"],MEMBER["World Geodetic System 1984 (G730)"],MEMBER["World Geodetic System 1984 (G873)"],MEMBER["World Geodetic System 1984 (G1150)"],MEMBER["World Geodetic System 1984 (G1674)"],MEMBER["World Geodetic System 1984 (G1762)"],ELLIPSOID["WGS 84",6378137,298.257223563,LENGTHUNIT["metre",1]],ENSEMBLEACCURACY[2.0]],PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433]],CS[ellipsoidal,2],AXIS["geodetic latitude (Lat)",north,ORDER[1],ANGLEUNIT["degree",0.0174532925199433]],AXIS["geodetic longitude (Lon)",east,ORDER[2],ANGLEUNIT["degree",0.0174532925199433]],USAGE[SCOPE["Horizontal component of 3D system."],AREA["World."],BBOX[-90,-180,90,180]],ID["EPSG",4326]]',
            "url": "http://www.opengis.net/def/crs/EPSG/0/4326"
        }
    }

    zattrs = {"crs": {crs_member: zattrs_all["crs"][crs_member]}}

    try:
        gdal.Mkdir('/vsimem/test.zarr', 0)
        gdal.FileFromMemBuffer('/vsimem/test.zarr/.zarray', json.dumps(zarray))
        gdal.FileFromMemBuffer('/vsimem/test.zarr/.zattrs', json.dumps(zattrs))
        ds = gdal.OpenEx('/vsimem/test.zarr', gdal.OF_MULTIDIM_RASTER)
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        srs = ar.GetSpatialRef()
        if not(osr.GetPROJVersionMajor() > 6 or osr.GetPROJVersionMinor() >= 2) and crs_member == 'projjson':
            assert srs is None
        else:
            assert srs is not None
            assert srs.GetAuthorityCode(None) == '4326'
            assert len(ar.GetAttributes()) == 0
    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


@pytest.mark.parametrize("use_get_names", [True, False])
def test_zarr_read_group(use_get_names):

    filename = 'data/zarr/group.zarr'
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None
    rg = ds.GetRootGroup()
    assert rg.GetName() == '/'
    assert rg.GetFullName() == '/'
    if use_get_names:
        assert rg.GetGroupNames() == ['foo']
    assert len(rg.GetAttributes()) == 1
    assert rg.GetAttribute('key') is not None
    subgroup = rg.OpenGroup('foo')
    assert subgroup is not None
    assert rg.OpenGroup('not_existing') is None
    assert subgroup.GetName() == 'foo'
    assert subgroup.GetFullName() == '/foo'
    assert rg.GetMDArrayNames() is None
    if use_get_names:
        assert subgroup.GetGroupNames() == ['bar']
    assert subgroup.GetAttributes() == []
    subsubgroup = subgroup.OpenGroup('bar')
    assert subsubgroup.GetName() == 'bar'
    assert subsubgroup.GetFullName() == '/foo/bar'
    if use_get_names:
        assert subsubgroup.GetMDArrayNames() == ['baz']
    ar = subsubgroup.OpenMDArray('baz')
    assert ar is not None
    assert ar.Read() == array.array('i', [1])
    assert subsubgroup.OpenMDArray('not_existing') is None


def test_zarr_read_group_with_zmetadata():

    filename = 'data/zarr/group_with_zmetadata.zarr'
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None
    rg = ds.GetRootGroup()
    assert rg.GetName() == '/'
    assert rg.GetFullName() == '/'
    assert rg.GetGroupNames() == ['foo']
    assert len(rg.GetAttributes()) == 1
    assert rg.GetAttribute('key') is not None
    subgroup = rg.OpenGroup('foo')
    assert subgroup is not None
    assert rg.OpenGroup('not_existing') is None
    assert subgroup.GetName() == 'foo'
    assert subgroup.GetFullName() == '/foo'
    assert rg.GetMDArrayNames() is None
    assert subgroup.GetGroupNames() == ['bar']
    assert subgroup.GetAttributes() == []
    subsubgroup = subgroup.OpenGroup('bar')
    assert subsubgroup.GetName() == 'bar'
    assert subsubgroup.GetFullName() == '/foo/bar'
    assert subsubgroup.GetMDArrayNames() == ['baz']
    assert subsubgroup.GetAttribute('foo') is not None
    ar = subsubgroup.OpenMDArray('baz')
    assert ar is not None
    assert ar.Read() == array.array('i', [1])
    assert ar.GetAttribute('bar') is not None
    assert subsubgroup.OpenMDArray('not_existing') is None


@pytest.mark.parametrize("use_zmetadata", [True, False])
def test_zarr_read_ARRAY_DIMENSIONS(use_zmetadata):

    filename = 'data/zarr/array_dimensions.zarr'

    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER, open_options=[
                     'USE_ZMETADATA=' + str(use_zmetadata)])
    assert ds is not None
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray('var')
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetName() == 'lat'
    assert dims[0].GetIndexingVariable() is not None
    assert dims[0].GetIndexingVariable().GetName() == 'lat'
    assert dims[1].GetName() == 'lon'
    assert dims[1].GetIndexingVariable() is not None
    assert dims[1].GetIndexingVariable().GetName() == 'lon'
    assert len(rg.GetDimensions()) == 2

    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER, open_options=[
                     'USE_ZMETADATA=' + str(use_zmetadata)])
    assert ds is not None
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray('lat')
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == 1
    assert dims[0].GetName() == 'lat'
    assert dims[0].GetIndexingVariable() is not None
    assert dims[0].GetIndexingVariable().GetName() == 'lat'
    assert len(rg.GetDimensions()) == 2

    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER, open_options=[
                     'USE_ZMETADATA=' + str(use_zmetadata)])
    assert ds is not None
    rg = ds.GetRootGroup()
    assert len(rg.GetDimensions()) == 2


@pytest.mark.parametrize("use_get_names", [True, False])
def test_zarr_read_v3(use_get_names):

    filename = 'data/zarr/v3/test.zr3'
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None
    rg = ds.GetRootGroup()
    assert rg.GetName() == '/'
    assert rg.GetFullName() == '/'
    if use_get_names:
        assert rg.GetGroupNames() == ['marvin']
    assert len(rg.GetAttributes()) == 1
    assert rg.GetAttribute('root_foo') is not None
    subgroup = rg.OpenGroup('marvin')
    assert subgroup is not None
    assert rg.OpenGroup('not_existing') is None
    assert subgroup.GetName() == 'marvin'
    assert subgroup.GetFullName() == '/marvin'
    if use_get_names:
        assert rg.GetMDArrayNames() == ['/', 'ar']

    ar = rg.OpenMDArray('/')
    assert ar
    assert ar.Read() == array.array('i', [2] + ([1] * (5 * 10 - 1)))

    ar = rg.OpenMDArray('ar')
    assert ar
    assert ar.Read() == array.array('b', [1, 2])

    if use_get_names:
        assert subgroup.GetGroupNames() == ['paranoid']
    assert len(subgroup.GetAttributes()) == 1

    subsubgroup = subgroup.OpenGroup('paranoid')
    assert subsubgroup.GetName() == 'paranoid'
    assert subsubgroup.GetFullName() == '/marvin/paranoid'

    if use_get_names:
        assert subgroup.GetMDArrayNames() == ['android']
    ar = subgroup.OpenMDArray('android')
    assert ar is not None
    assert ar.Read() == array.array('b', [1] * 4 * 5)
    assert subgroup.OpenMDArray('not_existing') is None


@pytest.mark.parametrize("endianness", ['le', 'be'])
def test_zarr_read_half_float(endianness):

    filename = 'data/zarr/f2_' + endianness + '.zarr'
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    assert ds is not None
    rg = ds.GetRootGroup()
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar.Read() == array.array('f', [1.5, float('nan')])
