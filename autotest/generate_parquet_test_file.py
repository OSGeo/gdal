#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR Parquet driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Planet Labs
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

wkt_epsg_4326 =  'GEOGCRS["WGS 84",ENSEMBLE["World Geodetic ' + \
                 'System 1984 ensemble",MEMBER["World Geodetic ' + \
                 'System 1984 (Transit)"],MEMBER["World ' + \
                 'Geodetic System 1984 (G730)"],MEMBER["World ' + \
                 'Geodetic System 1984 (G873)"],MEMBER["World ' + \
                 'Geodetic System 1984 (G1150)"],MEMBER["World ' + \
                 'Geodetic System 1984 (G1674)"],MEMBER["World ' + \
                 'Geodetic System 1984 (G1762)"],MEMBER["World ' + \
                 'Geodetic System 1984 ' + \
                 '(G2139)"],ELLIPSOID["WGS ' + \
                 '84",6378137,298.257223563],ENSEMBLEACCURACY[2.0]],CS[ellipsoidal,2],AXIS["geodetic ' + \
                 'latitude (Lat)",north],AXIS["geodetic ' + \
                 'longitude ' + \
                 '(Lon)",east],UNIT["degree",0.0174532925199433],USAGE[SCOPE["Horizontal ' + \
                 'component of 3D ' + \
                 'system."],AREA["World."],BBOX[-90,-180,90,180]],ID["EPSG",4326]]'

def generate_test_parquet():
    import pyarrow as pa
    import datetime
    import decimal
    import json
    import pandas as pd
    import pathlib
    import pyarrow.parquet as pq
    import struct

    boolean = pa.array([True, False, None, False, True], type=pa.bool_())
    uint8 = pa.array([None if i == 2 else 1 + i for i in range(5)], type=pa.uint8())
    int8 = pa.array([None if i == 2 else -2 + i for i in range(5)], type=pa.int8())
    uint16 = pa.array([None if i == 2 else 1 + i * 10000 for i in range(5)], type=pa.uint16())
    int16 = pa.array([None if i == 2 else -20000 + i * 10000 for i in range(5)], type=pa.int16())
    uint32 = pa.array([None if i == 2 else 1 + i * 1000000000 for i in range(5)], type=pa.uint32())
    int32 = pa.array([None if i == 2 else -2000000000 + i*1000000000 for i in range(5)], type=pa.int32())
    uint64 = pa.array([None if i == 2 else 1 + i * 100000000000 for i in range(5)], type=pa.uint64())
    int64 = pa.array([None if i == 2 else -200000000000 + i*100000000000 for i in range(5)], type=pa.int64())
    float32 = pa.array([None if i == 2 else 1.5 + i for i in range(5)], type=pa.float32())
    float64 = pa.array([None if i == 2 else 1.5 + i for i in range(5)], type=pa.float64())
    string = pa.array(["abcd", "", None, "c", "d"], type=pa.string())
    large_string = pa.array(["abcd", "", None, "c", "d"], type=pa.large_string())
    gmt_plus_2 = datetime.timezone(datetime.timedelta(hours=2))
    timestamp_ms_gmt_plus_2 = pa.array(
        [pd.Timestamp(year=2019, month=1, day=1, hour=14, nanosecond=500*1e6,
                      tz=gmt_plus_2)] * 5, type=pa.timestamp('ms', tz=gmt_plus_2))
    gmt = datetime.timezone(datetime.timedelta(hours=0))
    timestamp_ms_gmt = pa.array(
        [pd.Timestamp(year=2019, month=1, day=1, hour=14, nanosecond=500*1e6,
                      tz=gmt)] * 5, type=pa.timestamp('ms', tz=gmt))
    gmt_minus_0215 = datetime.timezone(datetime.timedelta(hours=-2.25))
    timestamp_ms_gmt_minus_0215 = pa.array(
        [pd.Timestamp(year=2019, month=1, day=1, hour=14, nanosecond=500*1e6,
                      tz=gmt_minus_0215)] * 5, type=pa.timestamp('ms', tz=gmt_minus_0215))
    timestamp_s_no_tz = pa.array(
        [pd.Timestamp(year=2019, month=1, day=1, hour=14, nanosecond=500*1e6)] * 5, type=pa.timestamp('s'))
    time32_s = pa.array([3600 + 120 + 3,None,3,4,5], type=pa.time32('s'))
    time32_ms = pa.array([(3600 + 120 + 3) * 1000 + 456,2,3,4,5], type=pa.time32('ms'))
    time64_us = pa.array([(3600 + 120 + 3) * 1e6,None,3,4,5], type=pa.time64('us'))
    time64_ns = pa.array([(3600 + 120 + 3) * 1e9 + 456,2,3,4,5], type=pa.time64('ns'))
    date32 = pa.array([1,2,3,4,5], type=pa.date32())
    date64 = pa.array([86400*1000,2,3,4,5], type=pa.date64())
    duration_s = pa.array([1,2,3,4,5], type=pa.duration('s'))
    duration_ms = pa.array([1,2,3,4,5], type=pa.duration('ms'))
    binary = pa.array([b'\x00\x01'] * 5, type=pa.binary())
    large_binary = pa.array([b'\x00\x01'] * 5, type=pa.large_binary())
    fixed_size_binary = pa.array([b'\x00\x01'] * 5, type=pa.binary(2))
    decimal128 = pa.array([decimal.Decimal('1234.567'),decimal.Decimal('-1234.567'),None,
                           decimal.Decimal('1234.567'),decimal.Decimal('-1234.567')], type=pa.decimal128(7,3))
    decimal256 = pa.array([decimal.Decimal('1234.567'),decimal.Decimal('-1234.567'),None,
                           decimal.Decimal('1234.567'),decimal.Decimal('-1234.567')], type=pa.decimal256(7,3))
    list_boolean = pa.array([None if i == 2 else [None if j == 0 else True if (j % 2) == 0 else False for j in range(i)] for i in range(5)], type=pa.list_(pa.bool_()))
    list_uint8 = pa.array([None if i == 2 else [None if j == 0 else j + i * (i-1)//2 for j in range(i)] for i in range(5)], type=pa.list_(pa.uint8()))
    list_int8 = pa.array([None if i == 2 else [None if j == 0 else j + i * (i-1)//2 for j in range(i)] for i in range(5)], type=pa.list_(pa.int8()))
    list_uint16 = pa.array([None if i == 2 else [None if j == 0 else j + i * (i-1)//2 for j in range(i)] for i in range(5)], type=pa.list_(pa.uint16()))
    list_int16 = pa.array([None if i == 2 else [None if j == 0 else j + i * (i-1)//2 for j in range(i)] for i in range(5)], type=pa.list_(pa.int16()))
    list_uint32 = pa.array([None if i == 2 else [None if j == 0 else j + i * (i-1)//2 for j in range(i)] for i in range(5)], type=pa.list_(pa.uint32()))
    list_int32 = pa.array([None if i == 2 else [None if j == 0 else j + i * (i-1)//2 for j in range(i)] for i in range(5)], type=pa.list_(pa.int32()))
    list_uint64 = pa.array([None if i == 2 else [None if j == 0 else j + i * (i-1)//2 for j in range(i)] for i in range(5)], type=pa.list_(pa.uint64()))
    list_int64 = pa.array([None if i == 2 else [None if j == 0 else j + i * (i-1)//2 for j in range(i)] for i in range(5)], type=pa.list_(pa.int64()))
    list_float32 = pa.array([None if i == 2 else [None if j == 0 else 0.5 + j + i * (i-1)//2 for j in range(i)] for i in range(5)], type=pa.list_(pa.float32()))
    list_float64 = pa.array([None if i == 2 else [None if j == 0 else 0.5 + j + i * (i-1)//2 for j in range(i)] for i in range(5)], type=pa.list_(pa.float64()))
    list_string = pa.array([None if i == 2 else ["".join(["%c" % (65+j+k) for k in range(1+j)]) for j in range(i)] for i in range(5)])
    fixed_size_list_boolean = pa.array([[True, False], [False,True], [True, False], [False,True], [True, False]], type=pa.list_(pa.bool_(), 2))
    fixed_size_list_uint8 = pa.array([[0, 1], [2,3], [4, 5], [6,7], [8, 9]], type=pa.list_(pa.uint8(), 2))
    fixed_size_list_int8 = pa.array([[0, 1], [2,3], [4, 5], [6,7], [8, 9]], type=pa.list_(pa.int8(), 2))
    fixed_size_list_uint16 = pa.array([[0, 1], [2,3], [4, 5], [6,7], [8, 9]], type=pa.list_(pa.uint16(), 2))
    fixed_size_list_int16 = pa.array([[0, 1], [2,3], [4, 5], [6,7], [8, 9]], type=pa.list_(pa.int16(), 2))
    fixed_size_list_uint32 = pa.array([[0, 1], [2,3], [4, 5], [6,7], [8, 9]], type=pa.list_(pa.uint32(), 2))
    fixed_size_list_int32 = pa.array([[0, 1], [2,3], [4, 5], [6,7], [8, 9]], type=pa.list_(pa.int32(), 2))
    fixed_size_list_uint64 = pa.array([[0, 1], [2,3], [4, 5], [6,7], [8, 9]], type=pa.list_(pa.uint64(), 2))
    fixed_size_list_int64 = pa.array([[0, 1], [2,3], [4, 5], [6,7], [8, 9]], type=pa.list_(pa.int64(), 2))
    fixed_size_list_float32 = pa.array([[0, None], [2,3], [4, 5], [6,7], [8, 9]], type=pa.list_(pa.float32(), 2))
    fixed_size_list_float64 = pa.array([[0, None], [2,3], [4, 5], [6,7], [8, 9]], type=pa.list_(pa.float64(), 2))
    fixed_size_list_string = pa.array([["a", "b"], ["c", "d"], ["e", "f"], ["g", "h"], ["i", "j"]], type=pa.list_(pa.string(), 2))
    struct_field = pa.array([{"a": 1, "b": 2.5, "c" : { "d": "e", "f": "g"}, "h":[5,6], "i":3 }] * 5)

    #struct_val = { "a": 5 }
    #for i in range(123):
    #    struct_val = { "a": struct_val }
    #struct_field = pa.array([struct_val] * 5)

    map_boolean = pa.array( [[('x', None), ('y', True)],[('z', True)],None,[],[]],  type=pa.map_(pa.string(), pa.bool_()))
    map_uint8 = pa.array( [[('x', 1), ('y', None)],[('z', 3)],None,[],[]],  type=pa.map_(pa.string(), pa.uint8()))
    map_int8 = pa.array( [[('x', 1), ('y', None)],[('z', 3)],None,[],[]],  type=pa.map_(pa.string(), pa.int8()))
    map_uint16 = pa.array( [[('x', 1), ('y', None)],[('z', 3)],None,[],[]],  type=pa.map_(pa.string(), pa.uint16()))
    map_int16 = pa.array( [[('x', 1), ('y', None)],[('z', 3)],None,[],[]],  type=pa.map_(pa.string(), pa.int16()))
    map_uint32 = pa.array( [[('x', 4*1000*1000*1000), ('y', None)],[('z', 3)],None,[],[]],  type=pa.map_(pa.string(), pa.uint32()))
    map_int32 = pa.array( [[('x', 2*1000*1000*1000), ('y', None)],[('z', 3)],None,[],[]],  type=pa.map_(pa.string(), pa.int32()))
    map_uint64 = pa.array( [[('x', 4*1000*1000*1000*1000), ('y', None)],[('z', 3)],None,[],[]],  type=pa.map_(pa.string(), pa.uint64()))
    map_int64 = pa.array( [[('x', -2*1000*1000*1000*1000), ('y', None)],[('z', 3)],None,[],[]],  type=pa.map_(pa.string(), pa.int64()))
    map_float32 = pa.array( [[('x', 1.5), ('y', None)],[('z', 3)],None,[],[]],  type=pa.map_(pa.string(), pa.float32()))
    map_float64 = pa.array( [[('x', 1.5), ('y', None)],[('z', 3)],None,[],[]],  type=pa.map_(pa.string(), pa.float64()))
    map_string = pa.array( [[('x', 'x_val'), ('y', None)],[('z', 'z_val')],None,[],[]],  type=pa.map_(pa.string(), pa.string()))

    indices = pa.array([0, 1, 2, None, 2])
    dictionary = pa.array(['foo', 'bar', 'baz'])
    dict = pa.DictionaryArray.from_arrays(indices, dictionary)

    map_list = pa.array( [[('x', []), ('y', [])],[('z', [])],None,[],[]],  type=pa.map_(pa.string(), pa.list_(pa.uint32())))

    geometry = pa.array( [None if i == 1 else (b'\x01\x01\x00\x00\x00' + struct.pack('<dd', i, 2)) for i in range(5)], type=pa.binary() )


    names=["boolean",
           "uint8",
           "int8",
           "uint16",
           "int16",
           "uint32",
           "int32",
           "uint64",
           "int64",
           "float32",
           "float64",
           "string",
           "large_string",
           "timestamp_ms_gmt",
           "timestamp_ms_gmt_plus_2",
           "timestamp_ms_gmt_minus_0215",
           "timestamp_s_no_tz",
           "time32_s",
           "time32_ms",
           "time64_us",
           "time64_ns",
           "date32",
           "date64",
           # "duration_s",
           # "duration_ms",
           "binary",
           "large_binary",
           "fixed_size_binary",
           "decimal128",
           "decimal256",
           "list_boolean",
           "list_uint8",
           "list_int8",
           "list_uint16",
           "list_int16",
           "list_uint32",
           "list_int32",
           "list_uint64",
           "list_int64",
           "list_float32",
           "list_float64",
           "list_string",
           "fixed_size_list_boolean",
           "fixed_size_list_uint8",
           "fixed_size_list_int8",
           "fixed_size_list_uint16",
           "fixed_size_list_int16",
           "fixed_size_list_uint32",
           "fixed_size_list_int32",
           "fixed_size_list_uint64",
           "fixed_size_list_int64",
           "fixed_size_list_float32",
           "fixed_size_list_float64",
           "fixed_size_list_string",
           "struct_field",
           "map_boolean",
           "map_uint8",
           "map_int8",
           "map_uint16",
           "map_int16",
           "map_uint32",
           "map_int32",
           "map_uint64",
           "map_int64",
           "map_float32",
           "map_float64",
           "map_string",
           # "map_list",
           "dict",
           "geometry",
    ]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    my_schema = table.schema.with_metadata(
        {"geo": json.dumps({"version": "0.1.0",
                            "primary_column": "geometry",
                            "columns": {
                                "geometry": {
                                     'crs':  wkt_epsg_4326,
                                     'bbox': [0, 2, 4, 2],
                                     'encoding': 'WKB'}}}) }  )

    table = table.cast(my_schema)
    HERE = pathlib.Path(__file__).parent
    pq.write_table(table, HERE / "ogr/data/parquet/test.parquet", compression='NONE', row_group_size=3)


def generate_all_geoms_parquet():
    import pyarrow as pa
    import json
    import pathlib
    import pyarrow.parquet as pq
    from osgeo import ogr

    g1 = ogr.CreateGeometryFromWkt('POINT(1 2)')
    g2 = ogr.CreateGeometryFromWkt('LINESTRING(3 4,5 6)')
    g3 = ogr.CreateGeometryFromWkt('POLYGON((10 0,11 0,11 -1,11 0,10 0),(10.2 -0.2,10.8 -0.2,10.8 -0.8,10.2 -0.8,10.2 -0.2))')
    g4 = ogr.CreateGeometryFromWkt('MULTIPOINT(7 8,9 10)')
    g5 = ogr.CreateGeometryFromWkt('MULTILINESTRING((11 12,13 14),(15 16,17 18))')
    g6 = ogr.CreateGeometryFromWkt('MULTIPOLYGON(((100 0,101 0,101 1,101 0,100 0),(100.2 0.2,100.8 0.2,100.8 0.8,100.2 0.8,100.2 0.2)))')
    g7 = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(19 20),LINESTRING(21 22, 23 24))')
    geometry = pa.array( [x.ExportToWkb(byte_order=ogr.wkbXDR) for x in (g1, g2, g3, g4, g5, g6, g7)], type=pa.binary() )
    names = ["geometry"]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    my_schema = table.schema.with_metadata(
        {"geo": json.dumps({"version": "0.1.0",
                            "primary_column": "geometry",
                            "columns": {
                                "geometry": {
                                     'crs':  wkt_epsg_4326,
                                     'encoding': 'WKB'}}}) }  )

    table = table.cast(my_schema)
    HERE = pathlib.Path(__file__).parent
    pq.write_table(table, HERE / "ogr/data/parquet/all_geoms.parquet", compression='NONE')


if __name__ == '__main__':
    generate_test_parquet()
    generate_all_geoms_parquet()
