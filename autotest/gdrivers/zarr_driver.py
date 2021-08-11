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
                          ["!u1", 'B', gdal.GDT_Byte, "1", 1],
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

        assert ar.GetOffset() is None
        assert ar.GetScale() is None
        assert ar.GetUnit() == ''

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
        "filters": [],
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
                          'filters', 'order', 'shape', 'fill_value'])
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
        if member == 'fill_value':
            assert ds is not None
            assert gdal.GetLastErrorMsg() != ''
        elif member is None:
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
                                         {"filters": "invalid"},
                                         {"filters": {}},
                                         {"filters": [{"missing_id": True}]},
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


@pytest.mark.parametrize("use_zmetadata, filename",
                         [(True, 'data/zarr/array_dimensions.zarr'),
                          (False, 'data/zarr/array_dimensions.zarr'),
                          (True, 'data/zarr/array_dimensions_upper_level.zarr'),
                          (False, 'data/zarr/array_dimensions_upper_level.zarr'),
                          (False, 'data/zarr/array_dimensions_upper_level.zarr/subgroup/var')])
def test_zarr_read_ARRAY_DIMENSIONS(use_zmetadata, filename):

    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER, open_options=[
                     'USE_ZMETADATA=' + str(use_zmetadata)])
    assert ds is not None
    rg = ds.GetRootGroup()
    if filename != 'data/zarr/array_dimensions_upper_level.zarr':
        ar = rg.OpenMDArray('var')
    else:
        ar = rg.OpenGroup('subgroup').OpenMDArray('var')
    assert ar
    dims = ar.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetName() == 'lat'
    assert dims[0].GetIndexingVariable() is not None
    assert dims[0].GetIndexingVariable().GetName() == 'lat'
    assert dims[0].GetType() == gdal.DIM_TYPE_HORIZONTAL_Y
    assert dims[0].GetDirection() == 'NORTH'
    assert dims[1].GetName() == 'lon'
    assert dims[1].GetIndexingVariable() is not None
    assert dims[1].GetIndexingVariable().GetName() == 'lon'
    assert dims[1].GetType() == gdal.DIM_TYPE_HORIZONTAL_X
    assert dims[1].GetDirection() == 'EAST'
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
    assert dims[0].GetType() == gdal.DIM_TYPE_HORIZONTAL_Y
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


def test_zarr_read_classic():

    ds = gdal.Open('data/zarr/zlib.zarr')
    assert ds
    assert not ds.GetSubDatasets()
    assert ds.ReadRaster() == array.array('b', [1, 2])

    ds = gdal.Open('ZARR:data/zarr/zlib.zarr')
    assert ds
    assert not ds.GetSubDatasets()
    assert ds.ReadRaster() == array.array('b', [1, 2])

    with gdaltest.error_handler():
        assert gdal.Open('ZARR:"data/zarr/not_existing.zarr"') is None
        assert gdal.Open('ZARR:"data/zarr/zlib.zarr":/not_existing') is None
        assert gdal.Open('ZARR:"data/zarr/zlib.zarr":/zlib:0') is None

    ds = gdal.Open('ZARR:"data/zarr/zlib.zarr":/zlib')
    assert ds
    assert not ds.GetSubDatasets()
    assert ds.ReadRaster() == array.array('b', [1, 2])

    ds = gdal.Open('data/zarr/order_f_u1_3d.zarr')
    assert ds
    subds = ds.GetSubDatasets()
    assert len(subds) == 2
    ds = gdal.Open(subds[0][0])
    assert ds
    assert ds.ReadRaster() == array.array('b', [i for i in range(12)])
    ds = gdal.Open(subds[1][0])
    assert ds
    assert ds.ReadRaster() == array.array('b', [12 + i for i in range(12)])

    with gdaltest.error_handler():
        assert gdal.Open(
            'ZARR:data/zarr/order_f_u1_3d.zarr:/order_f_u1_3d') is None
        assert gdal.Open(
            'ZARR:data/zarr/order_f_u1_3d.zarr:/order_f_u1_3d:2') is None
        assert gdal.Open(subds[0][0] + ':0') is None

    ds = gdal.Open('data/zarr/v3/test.zr3')
    assert ds
    subds = ds.GetSubDatasets()
    assert len(subds) == 2
    ds = gdal.Open(subds[0][0])
    assert ds
    assert ds.ReadRaster() == array.array('i', [2] + ([1] * (10 * 5 - 1)))
    ds = gdal.Open(subds[1][0])
    assert ds
    assert ds.ReadRaster() == array.array('b', [1, 2])


def test_zarr_read_classic_too_many_samples_3d():

    j = {
        "chunks": [
            65536, 2, 1
        ],
        "compressor": None,
        "dtype": '!u1',
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [
            65536, 2, 1
        ],
        "zarr_format": 2
    }

    try:
        gdal.Mkdir('/vsimem/test.zarr', 0)
        gdal.FileFromMemBuffer('/vsimem/test.zarr/.zarray', json.dumps(j))
        gdal.ErrorReset()
        with gdaltest.error_handler():
            ds = gdal.Open('/vsimem/test.zarr')
        assert gdal.GetLastErrorMsg() != ''
        assert len(ds.GetSubDatasets()) == 0
    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


def test_zarr_read_classic_4d():

    j = {
        "chunks": [
            3, 2, 1, 1
        ],
        "compressor": None,
        "dtype": '!u1',
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [
            3, 2, 1, 1
        ],
        "zarr_format": 2
    }

    try:
        gdal.Mkdir('/vsimem/test.zarr', 0)
        gdal.FileFromMemBuffer('/vsimem/test.zarr/.zarray', json.dumps(j))
        ds = gdal.Open('/vsimem/test.zarr')
        subds = ds.GetSubDatasets()
        assert len(subds) == 6
        for i in range(len(subds)):
            assert gdal.Open(subds[i][0]) is not None
    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


def test_zarr_read_classic_too_many_samples_4d():

    j = {
        "chunks": [
            256, 256, 1, 1
        ],
        "compressor": None,
        "dtype": '!u1',
        "fill_value": None,
        "filters": None,
        "order": "C",
        "shape": [
            256, 256, 1, 1
        ],
        "zarr_format": 2
    }

    try:
        gdal.Mkdir('/vsimem/test.zarr', 0)
        gdal.FileFromMemBuffer('/vsimem/test.zarr/.zarray', json.dumps(j))
        gdal.ErrorReset()
        with gdaltest.error_handler():
            ds = gdal.Open('/vsimem/test.zarr')
        assert gdal.GetLastErrorMsg() != ''
        assert len(ds.GetSubDatasets()) == 0
    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


def test_zarr_read_empty_shape():

    ds = gdal.OpenEx('data/zarr/empty.zarr', gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read() == array.array('b', [120])


def test_zarr_read_BLOSC_COMPRESSORS():

    if 'blosc' not in gdal.GetDriverByName('Zarr').GetMetadataItem('COMPRESSORS'):
        pytest.skip('blosc not available')
    assert 'lz4' in gdal.GetDriverByName(
        'Zarr').GetMetadataItem('BLOSC_COMPRESSORS')


@pytest.mark.parametrize("format", ['ZARR_V2', 'ZARR_V3'])
def test_zarr_create_group(format):

    try:
        def create():
            ds = gdal.GetDriverByName(
                'ZARR').CreateMultiDimensional('/vsimem/test.zarr', options=['FORMAT='+format])
            assert ds is not None
            rg = ds.GetRootGroup()
            assert rg
            assert rg.GetName() == '/'

            attr = rg.CreateAttribute(
                'str_attr', [], gdal.ExtendedDataType.CreateString())
            assert attr
            assert attr.Write('my_string') == gdal.CE_None

            attr = rg.CreateAttribute(
                'str_array_attr', [2], gdal.ExtendedDataType.CreateString())
            assert attr
            assert attr.Write(
                ['first_string', 'second_string']) == gdal.CE_None

            with gdaltest.error_handler():
                attr = rg.CreateAttribute('dim_2_not_supported', [
                                          2, 2], gdal.ExtendedDataType.CreateString())
                assert attr is None

            attr = rg.CreateAttribute(
                'int_attr', [], gdal.ExtendedDataType.Create(gdal.GDT_Int32))
            assert attr
            assert attr.Write(12345678) == gdal.CE_None

            attr = rg.CreateAttribute(
                'uint_attr', [], gdal.ExtendedDataType.Create(gdal.GDT_UInt32))
            assert attr
            assert attr.Write(4000000000) == gdal.CE_None

            attr = rg.CreateAttribute(
                'int_array_attr', [2], gdal.ExtendedDataType.Create(gdal.GDT_Int32))
            assert attr
            assert attr.Write([12345678, -12345678]) == gdal.CE_None

            attr = rg.CreateAttribute(
                'double_attr', [], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
            assert attr
            assert attr.Write(12345678.5) == gdal.CE_None

            attr = rg.CreateAttribute('double_array_attr', [
                                      2], gdal.ExtendedDataType.Create(gdal.GDT_Float64))
            assert attr
            assert attr.Write([12345678.5, -12345678.5]) == gdal.CE_None

            subgroup = rg.CreateGroup('foo')
            assert subgroup
            assert subgroup.GetName() == 'foo'
            assert subgroup.GetFullName() == '/foo'
            assert rg.GetGroupNames() == ['foo']
            subgroup = rg.OpenGroup('foo')
            assert subgroup

        create()

        def update():
            ds = gdal.OpenEx('/vsimem/test.zarr',
                             gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
            assert ds
            rg = ds.GetRootGroup()
            assert rg
            assert rg.GetGroupNames() == ['foo']

            attr = rg.GetAttribute('str_attr')
            assert attr
            assert attr.Read() == 'my_string'
            assert attr.Write('my_string_modified') == gdal.CE_None

            subgroup = rg.OpenGroup('foo')
            assert subgroup
            subgroup = rg.CreateGroup('bar')
            assert subgroup
            assert set(rg.GetGroupNames()) == set(['foo', 'bar'])
            subgroup = rg.OpenGroup('foo')
            assert subgroup
            subsubgroup = subgroup.CreateGroup('baz')
            assert subsubgroup
            ds = None

        update()

        ds = gdal.OpenEx('/vsimem/test.zarr', gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        assert rg

        attr = rg.GetAttribute('str_attr')
        assert attr
        assert attr.Read() == 'my_string_modified'

        attr = rg.GetAttribute('str_array_attr')
        assert attr
        assert attr.Read() == ['first_string', 'second_string']

        attr = rg.GetAttribute('int_attr')
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Int32
        assert attr.ReadAsDouble() == 12345678

        attr = rg.GetAttribute('uint_attr')
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Float64
        assert attr.ReadAsDouble() == 4000000000

        attr = rg.GetAttribute('int_array_attr')
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Int32
        assert attr.ReadAsIntArray() == (12345678, -12345678)

        attr = rg.GetAttribute('double_attr')
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Float64
        assert attr.ReadAsDouble() == 12345678.5

        attr = rg.GetAttribute('double_array_attr')
        assert attr
        assert attr.GetDataType().GetNumericDataType() == gdal.GDT_Float64
        assert attr.Read() == (12345678.5, -12345678.5)

        assert set(rg.GetGroupNames()) == set(['foo', 'bar'])
        with gdaltest.error_handler():
            assert rg.CreateGroup('not_opened_in_update_mode') is None
            assert rg.CreateAttribute(
                'not_opened_in_update_mode', [], gdal.ExtendedDataType.CreateString()) is None
        subgroup = rg.OpenGroup('foo')
        assert subgroup
        subsubgroup = subgroup.OpenGroup('baz')
        assert subsubgroup
        ds = None

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


@pytest.mark.parametrize("group_name", ["foo",  # already existing
                                        "directory_with_that_name",
                                        "",
                                        ".",
                                        "..",
                                        "a/b",
                                        "a\\n",
                                        "a:b",
                                        ".zarray",
                                        ])
@pytest.mark.parametrize("format", ['ZARR_V2', 'ZARR_V3'])
def test_zarr_create_group_errors(group_name, format):

    try:
        ds = gdal.GetDriverByName(
            'ZARR').CreateMultiDimensional('/vsimem/test.zarr', options=['FORMAT='+format])
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg
        subgroup = rg.CreateGroup('foo')
        assert subgroup
        if format == 'ZARR_V2':
            gdal.Mkdir('/vsimem/test.zarr/directory_with_that_name', 0)
        else:
            gdal.Mkdir(
                '/vsimem/test.zarr/meta/root/directory_with_that_name', 0)
        with gdaltest.error_handler():
            assert rg.CreateGroup(group_name) is None

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


def getCompoundDT():
    x = gdal.EDTComponent.Create(
        'x', 0, gdal.ExtendedDataType.Create(gdal.GDT_Int16))
    y = gdal.EDTComponent.Create(
        'y', 0, gdal.ExtendedDataType.Create(gdal.GDT_Int32))
    subcompound = gdal.ExtendedDataType.CreateCompound("", 4, [y])
    subcompound_component = gdal.EDTComponent.Create(
        'y', 4, subcompound)
    return gdal.ExtendedDataType.CreateCompound("", 8, [x, subcompound_component])


@pytest.mark.parametrize("datatype,nodata", [
    [gdal.ExtendedDataType.Create(gdal.GDT_Byte), None],
    [gdal.ExtendedDataType.Create(gdal.GDT_Byte), 1],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_UInt16), None],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_Int16), None],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_UInt32), None],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_Int32), None],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_Float32), None],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_Float64), None],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_Float64), 1.5],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_Float64), float('nan')],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_Float64), float('infinity')],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_Float64), float('-infinity')],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_CInt16), None],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_CInt32), None],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_CFloat32), None],
    [gdal.ExtendedDataType.Create(
        gdal.GDT_CFloat64), None],
    [gdal.ExtendedDataType.CreateString(10), None],
    [gdal.ExtendedDataType.CreateString(10), "ab"],
    [getCompoundDT(), None],
    [getCompoundDT(), bytes(array.array('h', [12])) +
     bytes(array.array('h', [0])) +  # padding
     bytes(array.array('i', [2345678]))],
])
@pytest.mark.parametrize("format", ['ZARR_V2', 'ZARR_V3'])
def test_zarr_create_array(datatype, nodata, format):

    error_expected = False
    if format == 'ZARR_V3':
        if datatype.GetClass() != gdal.GEDTC_NUMERIC or \
                gdal.DataTypeIsComplex(datatype.GetNumericDataType()):
            error_expected = True
    elif datatype.GetNumericDataType() in (gdal.GDT_CInt16, gdal.GDT_CInt32):
        error_expected = True

    try:
        def create():
            ds = gdal.GetDriverByName(
                'ZARR').CreateMultiDimensional('/vsimem/test.zarr',
                                               options=['FORMAT='+format])
            assert ds is not None
            rg = ds.GetRootGroup()
            assert rg
            assert rg.GetName() == '/'

            dim0 = rg.CreateDimension("dim0", None, None, 2)
            dim1 = rg.CreateDimension("dim1", None, None, 3)

            if error_expected:
                with gdaltest.error_handler():
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
            ds = gdal.OpenEx('/vsimem/test.zarr', gdal.OF_MULTIDIM_RASTER)
            assert ds
            rg = ds.GetRootGroup()
            assert rg
            ar = rg.OpenMDArray('my_ar')
            assert ar
            got_dt = ar.GetDataType()
            if got_dt.GetClass() == gdal.GEDTC_COMPOUND:
                comps = got_dt.GetComponents()
                assert len(comps) == 2
                assert comps[1].GetType().GetClass() == gdal.GEDTC_COMPOUND
                comps[1] = gdal.EDTComponent.Create(
                    comps[1].GetName(), comps[1].GetType().GetSize(),
                    gdal.ExtendedDataType.CreateCompound(
                        "", comps[1].GetType().GetSize(),
                        comps[1].GetType().GetComponents()))
                got_dt = gdal.ExtendedDataType.CreateCompound(
                    "", got_dt.GetSize(), comps)
            assert got_dt == datatype
            assert len(ar.GetDimensions()) == 2
            assert [ar.GetDimensions()[i].GetSize()
                    for i in range(2)] == [2, 3]
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
                assert ar.GetNoDataValueAsRaw() is None

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


@pytest.mark.parametrize("array_name", ["foo",  # already existing
                                        "directory_with_that_name",
                                        "",
                                        ".",
                                        "..",
                                        "a/b",
                                        "a\\n",
                                        "a:b",
                                        ".zarray",
                                        ])
def test_zarr_create_array_errors(array_name):

    try:
        ds = gdal.GetDriverByName(
            'ZARR').CreateMultiDimensional('/vsimem/test.zarr')
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg
        assert rg.CreateMDArray(
            'foo', [], gdal.ExtendedDataType.Create(gdal.GDT_Byte)) is not None
        gdal.Mkdir('/vsimem/test.zarr/directory_with_that_name', 0)
        with gdaltest.error_handler():
            assert rg.CreateMDArray(
                array_name, [], gdal.ExtendedDataType.Create(gdal.GDT_Byte)) is None

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


@pytest.mark.parametrize("compressor,options,expected_json", [
    ["NONE", [], None],
    ["zlib", [], {'id': 'zlib', 'level': 6}],
    ["zlib", ["ZLIB_LEVEL=1"], {'id': 'zlib', 'level': 1}],
    ["blosc", [], {'blocksize': 0,
                   'clevel': 5,
                   'cname': 'lz4',
                   'id': 'blosc',
                   'shuffle': 1}]])
def test_zarr_create_array_compressor(compressor, options, expected_json):

    compressors = gdal.GetDriverByName('Zarr').GetMetadataItem('COMPRESSORS')
    if compressor != 'NONE' and compressor not in compressors:
        pytest.skip('compressor %s not available' % compressor)

    try:
        def create():
            ds = gdal.GetDriverByName(
                'ZARR').CreateMultiDimensional('/vsimem/test.zarr')
            assert ds is not None
            rg = ds.GetRootGroup()
            assert rg
            assert rg.CreateMDArray(
                "test", [], gdal.ExtendedDataType.Create(gdal.GDT_Byte),
                ['COMPRESS=' + compressor] + options) is not None

        create()

        f = gdal.VSIFOpenL('/vsimem/test.zarr/test/.zarray', 'rb')
        assert f
        data = gdal.VSIFReadL(1, 1000, f)
        gdal.VSIFCloseL(f)
        j = json.loads(data)
        assert j['compressor'] == expected_json

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


@pytest.mark.parametrize("compressor,options,expected_json", [
    ["NONE", [], None],
    ["gzip", [], {'codec': 'https://purl.org/zarr/spec/codec/gzip/1.0',
                  'configuration': {'level': 6}}]])
def test_zarr_create_array_compressor_v3(compressor, options, expected_json):

    compressors = gdal.GetDriverByName('Zarr').GetMetadataItem('COMPRESSORS')
    if compressor != 'NONE' and compressor not in compressors:
        pytest.skip('compressor %s not available' % compressor)

    try:
        def create():
            ds = gdal.GetDriverByName(
                'ZARR').CreateMultiDimensional('/vsimem/test.zarr',
                                               options=['FORMAT=ZARR_V3'])
            assert ds is not None
            rg = ds.GetRootGroup()
            assert rg
            assert rg.CreateMDArray(
                "test", [], gdal.ExtendedDataType.Create(gdal.GDT_Byte),
                ['COMPRESS=' + compressor] + options) is not None

        create()

        f = gdal.VSIFOpenL('/vsimem/test.zarr/meta/root/test.array.json', 'rb')
        assert f
        data = gdal.VSIFReadL(1, 1000, f)
        gdal.VSIFCloseL(f)
        j = json.loads(data)
        if expected_json is None:
            assert 'compressor' not in j
        else:
            assert j['compressor'] == expected_json

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


@pytest.mark.parametrize("format", ['ZARR_V2', 'ZARR_V3'])
def test_zarr_create_array_bad_compressor(format):

    try:
        ds = gdal.GetDriverByName(
            'ZARR').CreateMultiDimensional('/vsimem/test.zarr',
                                           options=['FORMAT='+format])
        assert ds is not None
        rg = ds.GetRootGroup()
        assert rg
        with gdaltest.error_handler():
            assert rg.CreateMDArray(
                "test", [], gdal.ExtendedDataType.Create(gdal.GDT_Byte),
                ['COMPRESS=invalid']) is None
    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


@pytest.mark.parametrize("format", ['ZARR_V2', 'ZARR_V3'])
def test_zarr_create_array_attributes(format):

    try:
        def create():
            ds = gdal.GetDriverByName(
                'ZARR').CreateMultiDimensional('/vsimem/test.zarr',
                                               options=['FORMAT='+format])
            assert ds is not None
            rg = ds.GetRootGroup()
            assert rg
            ar = rg.CreateMDArray(
                "test", [], gdal.ExtendedDataType.Create(gdal.GDT_Byte))
            assert ar

            attr = ar.CreateAttribute(
                'str_attr', [], gdal.ExtendedDataType.CreateString())
            assert attr
            assert attr.Write('my_string') == gdal.CE_None

            with gdaltest.error_handler():
                assert ar.CreateAttribute(
                    'invalid_2d', [2, 3], gdal.ExtendedDataType.CreateString()) is None

        create()

        def update():
            ds = gdal.OpenEx('/vsimem/test.zarr',
                             gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
            assert ds
            rg = ds.GetRootGroup()
            assert rg
            ar = rg.OpenMDArray('test')
            assert ar

            attr = ar.GetAttribute('str_attr')
            assert attr
            assert attr.Read() == 'my_string'
            assert attr.Write('my_string_modified') == gdal.CE_None

        update()

        ds = gdal.OpenEx('/vsimem/test.zarr', gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray('test')
        assert ar

        attr = ar.GetAttribute('str_attr')
        assert attr
        assert attr.Read() == 'my_string_modified'
        with gdaltest.error_handler():
            assert attr.Write('foo') == gdal.CE_Failure

        with gdaltest.error_handler():
            assert ar.CreateAttribute(
                'another_attr', [], gdal.ExtendedDataType.CreateString()) is None
    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


def test_zarr_create_array_set_crs():

    try:
        def create():
            ds = gdal.GetDriverByName(
                'ZARR').CreateMultiDimensional('/vsimem/test.zarr')
            assert ds is not None
            rg = ds.GetRootGroup()
            assert rg
            ar = rg.CreateMDArray(
                "test", [], gdal.ExtendedDataType.Create(gdal.GDT_Byte))
            assert ar
            crs = osr.SpatialReference()
            crs.ImportFromEPSG(4326)
            assert ar.SetSpatialRef(crs) == gdal.CE_None

        create()

        f = gdal.VSIFOpenL('/vsimem/test.zarr/test/.zattrs', 'rb')
        assert f
        data = gdal.VSIFReadL(1, 10000, f)
        gdal.VSIFCloseL(f)
        j = json.loads(data)
        assert 'crs' in j
        crs = j['crs']
        assert 'wkt' in crs
        assert 'url' in crs
        if 'projjson' in crs:
            assert crs['projjson']['type'] == 'GeographicCRS'

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


def test_zarr_create_array_set_dimension_name():

    try:
        def create():
            ds = gdal.GetDriverByName(
                'ZARR').CreateMultiDimensional('/vsimem/test.zarr')
            assert ds is not None
            rg = ds.GetRootGroup()
            assert rg

            dim0 = rg.CreateDimension("dim0", None, None, 2)
            dim0_ar = rg.CreateMDArray(
                "dim0", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_Byte))
            dim0.SetIndexingVariable(dim0_ar)

            rg.CreateMDArray(
                "test", [dim0], gdal.ExtendedDataType.Create(gdal.GDT_Byte))

        create()

        f = gdal.VSIFOpenL('/vsimem/test.zarr/test/.zattrs', 'rb')
        assert f
        data = gdal.VSIFReadL(1, 10000, f)
        gdal.VSIFCloseL(f)
        j = json.loads(data)
        assert '_ARRAY_DIMENSIONS' in j
        assert j['_ARRAY_DIMENSIONS'] == ['dim0']

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


@pytest.mark.parametrize("dtype,structtype,gdaltype,fill_value,nodata_value",
                         [["!b1", 'B', gdal.GDT_Byte, None, None],
                          ["!i1", 'b', gdal.GDT_Int16, None, None],
                          ["!i1", 'b', gdal.GDT_Int16, -1, -1],
                          ["!u1", 'B', gdal.GDT_Byte, None, None],
                          ["!u1", 'B', gdal.GDT_Byte, "1", 1],
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
def test_zarr_write_array_content(dtype, structtype, gdaltype, fill_value, nodata_value, use_optimized_code_paths):

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

    filename = '/vsimem/test' + \
        dtype.replace('<', 'lt').replace('>', 'gt').replace(
            '!', 'not') + structtype + '.zarr'
    try:
        gdal.Mkdir(filename, 0o755)
        f = gdal.VSIFOpenL(filename + '/.zarray', 'wb')
        assert f
        data = json.dumps(j)
        gdal.VSIFWriteL(data, 1, len(data), f)
        gdal.VSIFCloseL(f)

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
        gdal.FileFromMemBuffer(filename + '/0.0', tile_0_0_data)
        gdal.FileFromMemBuffer(filename + '/0.1', tile_0_1_data)

        with gdaltest.config_option('GDAL_ZARR_USE_OPTIMIZED_CODE_PATHS',
                                    'YES' if use_optimized_code_paths else 'NO'):
            ds = gdal.OpenEx(
                filename, gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
            assert ds
            rg = ds.GetRootGroup()
            assert rg
            ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar

        dt = gdal.ExtendedDataType.Create(gdal.GDT_CFloat64 if gdaltype in (
            gdal.GDT_CFloat32, gdal.GDT_CFloat64) else gdal.GDT_Float64)

        # Write all nodataset. That should cause tiles to be removed.
        nv = nodata_value if nodata_value else 0
        buf_nodata = array.array(
            'd', [nv] * (5 * 4 * (2 if gdaltype in (gdal.GDT_CFloat32, gdal.GDT_CFloat64) else 1)))
        assert ar.Write(buf_nodata, buffer_datatype=dt) == gdal.CE_None
        assert ar.Read(buffer_datatype=dt) == bytearray(buf_nodata)

        if fill_value is None or fill_value == 0 or not gdal.DataTypeIsComplex(gdaltype):
            assert gdal.VSIStatL(filename + '/0.0') is None

        # Write all ones
        ones = array.array('d', [
                           0] * (5 * 4 * (2 if gdaltype in (gdal.GDT_CFloat32, gdal.GDT_CFloat64) else 1)))
        assert ar.Write(ones, buffer_datatype=dt) == gdal.CE_None
        assert ar.Read(buffer_datatype=dt) == bytearray(ones)

        # Write with odd array_step
        assert ar.Write(struct.pack('d' * 4, nv, nv, 6, 5),
                        array_start_idx=[2, 1],
                        count=[2, 2],
                        array_step=[-1, -1],
                        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == gdal.CE_None

        # Check back
        assert ar.Read(array_start_idx=[2, 1],
                       count=[2, 2],
                       array_step=[-1, -1],
                       buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == struct.pack('d' * 4, nv, nv, 6, 5)

        # Force dirty block eviction
        ar.Read(buffer_datatype=dt)

        # Check back again
        assert ar.Read(array_start_idx=[2, 1],
                       count=[2, 2],
                       array_step=[-1, -1],
                       buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Float64)) == struct.pack('d' * 4, nv, nv, 6, 5)

    finally:
        gdal.RmdirRecursive(filename)


def test_zarr_create_array_string():

    try:
        def create():
            ds = gdal.GetDriverByName(
                'ZARR').CreateMultiDimensional('/vsimem/test.zarr')
            assert ds is not None
            rg = ds.GetRootGroup()
            assert rg

            dim0 = rg.CreateDimension("dim0", None, None, 2)

            ar = rg.CreateMDArray(
                "test", [dim0], gdal.ExtendedDataType.CreateString(10))
            assert ar.Write(['ab', '0123456789truncated']) == gdal.CE_None
        create()

        ds = gdal.OpenEx(
            '/vsimem/test.zarr', gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar.Read() == ['ab', '0123456789']

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


@pytest.mark.parametrize("format", ['ZARR_V2', 'ZARR_V3'])
def test_zarr_create_fortran_order_3d_and_compression_and_dim_separator(format):

    try:
        def create():
            ds = gdal.GetDriverByName(
                'ZARR').CreateMultiDimensional('/vsimem/test.zarr',
                                               options=['FORMAT='+format])
            assert ds is not None
            rg = ds.GetRootGroup()
            assert rg

            dim0 = rg.CreateDimension("dim0", None, None, 2)
            dim1 = rg.CreateDimension("dim1", None, None, 3)
            dim2 = rg.CreateDimension("dim2", None, None, 4)

            ar = rg.CreateMDArray(
                "test", [dim0, dim1, dim2],
                gdal.ExtendedDataType.Create(gdal.GDT_Byte),
                ['CHUNK_MEMORY_LAYOUT=F', 'COMPRESS=zlib', 'DIM_SEPARATOR=/'])
            assert ar.Write(array.array(
                'b', [i for i in range(2 * 3 * 4)])) == gdal.CE_None

        create()

        if format == 'ZARR_V2':
            f = gdal.VSIFOpenL('/vsimem/test.zarr/test/.zarray', 'rb')
        else:
            f = gdal.VSIFOpenL(
                '/vsimem/test.zarr/meta/root/test.array.json', 'rb')
        assert f
        data = gdal.VSIFReadL(1, 10000, f)
        gdal.VSIFCloseL(f)
        j = json.loads(data)
        if format == 'ZARR_V2':
            assert 'order' in j
            assert j['order'] == 'F'
        else:
            assert 'chunk_memory_layout' in j
            assert j['chunk_memory_layout'] == 'F'

        ds = gdal.OpenEx(
            '/vsimem/test.zarr', gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar.Read() == \
            array.array('b', [i for i in range(2 * 3 * 4)])

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


def test_zarr_create_unit_offset_scale():

    try:
        def create():
            ds = gdal.GetDriverByName(
                'ZARR').CreateMultiDimensional('/vsimem/test.zarr')
            assert ds is not None
            rg = ds.GetRootGroup()
            assert rg

            ar = rg.CreateMDArray(
                "test", [], gdal.ExtendedDataType.Create(gdal.GDT_Byte))
            assert ar.SetOffset(1.5) == gdal.CE_None
            assert ar.SetScale(2.5) == gdal.CE_None
            assert ar.SetUnit("my unit") == gdal.CE_None

        create()

        f = gdal.VSIFOpenL('/vsimem/test.zarr/test/.zattrs', 'rb')
        assert f
        data = gdal.VSIFReadL(1, 10000, f)
        gdal.VSIFCloseL(f)
        j = json.loads(data)
        assert 'add_offset' in j
        assert j['add_offset'] == 1.5
        assert 'scale_factor' in j
        assert j['scale_factor'] == 2.5
        assert 'units' in j
        assert j['units'] == 'my unit'

        ds = gdal.OpenEx(
            '/vsimem/test.zarr', gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar.GetOffset() == 1.5
        assert ar.GetScale() == 2.5
        assert ar.GetUnit() == 'my unit'

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


def test_zarr_getcoordinatevariables():

    src_ds = gdal.OpenEx(
        'data/netcdf/expanded_form_of_grid_mapping.nc', gdal.OF_MULTIDIM_RASTER)
    if src_ds is None:
        pytest.skip()

    try:
        def create(src_ds):
            ds = gdal.MultiDimTranslate(
                '/vsimem/test.zarr', src_ds, format='Zarr')
            src_ds = None
            assert ds
            rg = ds.GetRootGroup()

            ar = rg.OpenMDArray('temp')
            coordinate_vars = ar.GetCoordinateVariables()
            assert len(coordinate_vars) == 2
            assert coordinate_vars[0].GetName() == 'lat'
            assert coordinate_vars[1].GetName() == 'lon'

            assert len(coordinate_vars[0].GetCoordinateVariables()) == 0

        create(src_ds)

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


def test_zarr_create_copy():

    tst = gdaltest.GDALTest('Zarr', '../../gcore/data/uint16.tif', 1, 4672)

    try:
        return tst.testCreate(vsimem=1, new_filename='/vsimem/test.zarr')
    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


@pytest.mark.parametrize("format", ['ZARR_V2', 'ZARR_V3'])
def test_zarr_create(format):

    try:
        ds = gdal.GetDriverByName('Zarr').Create('/vsimem/test.zarr', 1, 1, 3,
                                                 options=['ARRAY_NAME=foo',
                                                          'FORMAT=' + format])
        assert ds.GetGeoTransform(can_return_null=True) is None
        assert ds.GetSpatialRef() is None
        assert ds.GetRasterBand(1).GetNoDataValue() is None
        assert ds.GetRasterBand(1).SetNoDataValue(10) == gdal.CE_None
        assert ds.GetRasterBand(1).GetOffset() is None
        assert ds.GetRasterBand(1).SetOffset(1.5) == gdal.CE_None
        assert ds.GetRasterBand(1).GetScale() is None
        assert ds.GetRasterBand(1).SetScale(2.5) == gdal.CE_None
        assert ds.GetRasterBand(1).GetUnitType() == ''
        assert ds.GetRasterBand(1).SetUnitType("my_unit") == gdal.CE_None
        assert ds.SetMetadata({"FOO": "BAR"}) == gdal.CE_None
        ds = None

        ds = gdal.Open('ZARR:/vsimem/test.zarr:/foo_band1')
        assert ds
        assert ds.GetMetadata() == {"FOO": "BAR"}
        assert ds.GetRasterBand(1).GetNoDataValue() == 10.0
        assert ds.GetRasterBand(1).GetOffset() == 1.5

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


def test_zarr_create_append_subdataset():

    try:
        def create():
            ds = gdal.GetDriverByName('Zarr').Create('/vsimem/test.zarr', 3, 2, 1,
                                                     options=['ARRAY_NAME=foo'])
            assert ds
            ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
            ds = None

            # Same dimensions. Will reuse the ones of foo
            ds = gdal.GetDriverByName('Zarr').Create('/vsimem/test.zarr', 3, 2, 1,
                                                     options=['APPEND_SUBDATASET=YES',
                                                              'ARRAY_NAME=bar'])
            assert ds
            ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
            ds = None

            # Different dimensions.
            ds = gdal.GetDriverByName('Zarr').Create('/vsimem/test.zarr', 30, 20, 1,
                                                     options=['APPEND_SUBDATASET=YES',
                                                              'ARRAY_NAME=baz'])
            assert ds
            ds.SetGeoTransform([2, .1, 0, 49, 0, -.1])
            ds = None

        create()

        def check():
            ds = gdal.OpenEx('/vsimem/test.zarr', gdal.OF_MULTIDIM_RASTER)
            rg = ds.GetRootGroup()

            foo = rg.OpenMDArray('foo')
            assert foo
            assert foo.GetDimensions()[0].GetName() == 'Y'
            assert foo.GetDimensions()[1].GetName() == 'X'

            bar = rg.OpenMDArray('bar')
            assert bar
            assert bar.GetDimensions()[0].GetName() == 'Y'
            assert bar.GetDimensions()[1].GetName() == 'X'

            baz = rg.OpenMDArray('baz')
            assert baz
            assert baz.GetDimensions()[0].GetName() == 'baz_Y'
            assert baz.GetDimensions()[1].GetName() == 'baz_X'

        check()

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


@pytest.mark.parametrize("blocksize", ['1,2',
                                       '2,2,0',
                                       '4000000000,4000000000,4000000000'])
def test_zarr_create_array_invalid_blocksize(blocksize):

    try:
        def create():
            ds = gdal.GetDriverByName(
                'ZARR').CreateMultiDimensional('/vsimem/test.zarr')
            assert ds is not None
            rg = ds.GetRootGroup()
            assert rg

            dim0 = rg.CreateDimension("dim0", None, None, 2)
            dim1 = rg.CreateDimension("dim1", None, None, 2)
            dim2 = rg.CreateDimension("dim2", None, None, 2)

            with gdaltest.error_handler():
                ar = rg.CreateMDArray(
                    "test", [dim0, dim1, dim2],
                    gdal.ExtendedDataType.Create(gdal.GDT_Byte),
                    ['BLOCKSIZE=' + blocksize])
                assert ar is None

        create()

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


def test_zarr_read_filters():

    filename = 'data/zarr/delta_filter_i4.zarr'
    ds = gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER)
    rg = ds.GetRootGroup()
    assert rg
    ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
    assert ar
    assert ar.Read() == array.array('i', [i for i in range(10)])


def test_zarr_update_with_filters():

    try:
        gdal.Mkdir('/vsimem/test.zarr', 0)
        gdal.FileFromMemBuffer('/vsimem/test.zarr/.zarray',
                               open('data/zarr/delta_filter_i4.zarr/.zarray', 'rb').read())
        gdal.FileFromMemBuffer('/vsimem/test.zarr/0',
                               open('data/zarr/delta_filter_i4.zarr/0', 'rb').read())

        def update():
            ds = gdal.OpenEx('/vsimem/test.zarr',
                             gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
            assert ds
            rg = ds.GetRootGroup()
            assert rg
            ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
            assert ar
            assert ar.Read() == array.array('i', [i for i in range(10)])
            assert ar.Write(array.array(
                'i', [10-i for i in range(10)])) == gdal.CE_None

        update()

        ds = gdal.OpenEx('/vsimem/test.zarr',
                         gdal.OF_MULTIDIM_RASTER | gdal.OF_UPDATE)
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
        assert ar
        assert ar.Read() == array.array('i', [10 - i for i in range(10)])

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


def test_zarr_create_with_filter():

    tst = gdaltest.GDALTest('Zarr', '../../gcore/data/uint16.tif', 1, 4672,
                            options=['FILTER=delta'])

    try:
        ret = tst.testCreate(vsimem=1, new_filename='/vsimem/test.zarr')

        f = gdal.VSIFOpenL('/vsimem/test.zarr/test/.zarray', 'rb')
        assert f
        data = gdal.VSIFReadL(1, 10000, f)
        gdal.VSIFCloseL(f)
        j = json.loads(data)
        assert 'filters' in j
        assert j['filters'] == [{'id': 'delta', 'dtype': '<u2'}]

        return ret
    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')


def test_zarr_pam_spatial_ref():

    try:
        def create():
            ds = gdal.GetDriverByName(
                'ZARR').CreateMultiDimensional('/vsimem/test.zarr')
            assert ds is not None
            rg = ds.GetRootGroup()
            assert rg

            dim0 = rg.CreateDimension("dim0", None, None, 2)
            dim1 = rg.CreateDimension("dim1", None, None, 2)
            rg.CreateMDArray("test", [dim0, dim1],
                    gdal.ExtendedDataType.Create(gdal.GDT_Byte))

        create()

        assert gdal.VSIStatL('/vsimem/test.zarr/pam.aux.xml') is None

        def check_crs_before():
            ds = gdal.OpenEx('/vsimem/test.zarr',
                             gdal.OF_MULTIDIM_RASTER)
            assert ds
            rg = ds.GetRootGroup()
            assert rg
            ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
            assert ar
            crs = ar.GetSpatialRef()
            assert crs is None

        check_crs_before()

        assert gdal.VSIStatL('/vsimem/test.zarr/pam.aux.xml') is None

        def set_crs():
            # Open in read-only
            ds = gdal.OpenEx('/vsimem/test.zarr',
                             gdal.OF_MULTIDIM_RASTER)
            assert ds
            rg = ds.GetRootGroup()
            assert rg
            ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
            assert ar
            crs = osr.SpatialReference()
            crs.ImportFromEPSG(4326)
            crs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
            crs.SetCoordinateEpoch(2021.2)
            assert ar.SetSpatialRef(crs) == gdal.CE_None

        set_crs()

        assert gdal.VSIStatL('/vsimem/test.zarr/pam.aux.xml') is not None

        f = gdal.VSIFOpenL('/vsimem/test.zarr/pam.aux.xml', 'rb+')
        assert f
        data = gdal.VSIFReadL(1, 1000, f).decode('utf-8')
        assert data.endswith('</PAMDataset>\n')
        data = data[0:-len('</PAMDataset>\n')] + '<Other/>' + '</PAMDataset>\n'
        gdal.VSIFSeekL(f, 0, 0)
        gdal.VSIFWriteL(data, 1, len(data), f)
        gdal.VSIFCloseL(f)

        def check_crs():
            ds = gdal.OpenEx('/vsimem/test.zarr',
                             gdal.OF_MULTIDIM_RASTER)
            assert ds
            rg = ds.GetRootGroup()
            assert rg
            ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
            assert ar
            crs = ar.GetSpatialRef()
            assert crs is not None
            assert crs.GetAuthorityCode(None) == '4326'
            assert crs.GetDataAxisToSRSAxisMapping() == [2, 1]
            assert crs.GetCoordinateEpoch() == 2021.2

        check_crs()

        def check_crs_classic_dataset():
            ds = gdal.Open('/vsimem/test.zarr')
            crs = ds.GetSpatialRef()
            assert crs is not None

        check_crs_classic_dataset()

        def unset_crs():
            # Open in read-only
            ds = gdal.OpenEx('/vsimem/test.zarr',
                             gdal.OF_MULTIDIM_RASTER)
            assert ds
            rg = ds.GetRootGroup()
            assert rg
            ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
            assert ar
            assert ar.SetSpatialRef(None) == gdal.CE_None

        unset_crs()

        f = gdal.VSIFOpenL('/vsimem/test.zarr/pam.aux.xml', 'rb')
        assert f
        data = gdal.VSIFReadL(1, 1000, f).decode('utf-8')
        gdal.VSIFCloseL(f)
        assert '<Other />' in data

        def check_unset_crs():
            ds = gdal.OpenEx('/vsimem/test.zarr',
                             gdal.OF_MULTIDIM_RASTER)
            assert ds
            rg = ds.GetRootGroup()
            assert rg
            ar = rg.OpenMDArray(rg.GetMDArrayNames()[0])
            assert ar
            crs = ar.GetSpatialRef()
            assert crs is None

        check_unset_crs()

    finally:
        gdal.RmdirRecursive('/vsimem/test.zarr')

