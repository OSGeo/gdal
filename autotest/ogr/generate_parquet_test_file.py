#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR Parquet driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Planet Labs
#
# SPDX-License-Identifier: MIT
###############################################################################

wkt_epsg_4326 = (
    'GEOGCRS["WGS 84",ENSEMBLE["World Geodetic '
    + 'System 1984 ensemble",MEMBER["World Geodetic '
    + 'System 1984 (Transit)"],MEMBER["World '
    + 'Geodetic System 1984 (G730)"],MEMBER["World '
    + 'Geodetic System 1984 (G873)"],MEMBER["World '
    + 'Geodetic System 1984 (G1150)"],MEMBER["World '
    + 'Geodetic System 1984 (G1674)"],MEMBER["World '
    + 'Geodetic System 1984 (G1762)"],MEMBER["World '
    + "Geodetic System 1984 "
    + '(G2139)"],ELLIPSOID["WGS '
    + '84",6378137,298.257223563],ENSEMBLEACCURACY[2.0]],CS[ellipsoidal,2],AXIS["geodetic '
    + 'latitude (Lat)",north],AXIS["geodetic '
    + "longitude "
    + '(Lon)",east],UNIT["degree",0.0174532925199433],USAGE[SCOPE["Horizontal '
    + "component of 3D "
    + 'system."],AREA["World."],BBOX[-90,-180,90,180]],ID["EPSG",4326]]'
)


def generate_test_parquet():
    import datetime
    import decimal
    import json
    import pathlib
    import struct

    import numpy as np
    import pandas as pd
    import pyarrow as pa
    import pyarrow.parquet as pq

    boolean = pa.array([True, False, None, False, True], type=pa.bool_())
    uint8 = pa.array([None if i == 2 else 1 + i for i in range(5)], type=pa.uint8())
    int8 = pa.array([None if i == 2 else -2 + i for i in range(5)], type=pa.int8())
    uint16 = pa.array(
        [None if i == 2 else 1 + i * 10000 for i in range(5)], type=pa.uint16()
    )
    int16 = pa.array(
        [None if i == 2 else -20000 + i * 10000 for i in range(5)], type=pa.int16()
    )
    uint32 = pa.array(
        [None if i == 2 else 1 + i * 1000000000 for i in range(5)], type=pa.uint32()
    )
    int32 = pa.array(
        [None if i == 2 else -2000000000 + i * 1000000000 for i in range(5)],
        type=pa.int32(),
    )
    uint64 = pa.array(
        [None if i == 2 else 1 + i * 100000000000 for i in range(5)], type=pa.uint64()
    )
    int64 = pa.array(
        [None if i == 2 else -200000000000 + i * 100000000000 for i in range(5)],
        type=pa.int64(),
    )
    float32 = pa.array(
        [None if i == 2 else 1.5 + i for i in range(5)], type=pa.float32()
    )
    float64 = pa.array(
        [None if i == 2 else 1.5 + i for i in range(5)], type=pa.float64()
    )
    string = pa.array(["abcd", "", None, "c", "d"], type=pa.string())
    large_string = pa.array(["abcd", "", None, "c", "d"], type=pa.large_string())
    gmt_plus_2 = datetime.timezone(datetime.timedelta(hours=2))
    timestamp_ms_gmt_plus_2 = pa.array(
        [
            pd.Timestamp(
                year=2019,
                month=1,
                day=1,
                hour=14,
                microsecond=500 * 1000,
                tz=gmt_plus_2,
            )
        ]
        * 5,
        type=pa.timestamp("ms", tz=gmt_plus_2),
    )
    gmt = datetime.timezone(datetime.timedelta(hours=0))
    timestamp_ms_gmt = pa.array(
        [
            pd.Timestamp(
                year=2019, month=1, day=1, hour=14, microsecond=500 * 1000, tz=gmt
            )
        ]
        * 5,
        type=pa.timestamp("ms", tz=gmt),
    )
    gmt_minus_0215 = datetime.timezone(datetime.timedelta(hours=-2.25))
    timestamp_ms_gmt_minus_0215 = pa.array(
        [
            pd.Timestamp(
                year=2019,
                month=1,
                day=1,
                hour=14,
                microsecond=500 * 1000,
                tz=gmt_minus_0215,
            )
        ]
        * 5,
        type=pa.timestamp("ms", tz=gmt_minus_0215),
    )
    timestamp_s_no_tz = pa.array(
        [pd.Timestamp(year=2019, month=1, day=1, hour=14)] * 5,
        type=pa.timestamp("s"),
    )
    timestamp_us_no_tz = pa.array(
        [pd.Timestamp(year=2019, month=1, day=1, hour=14, microsecond=500)] * 5,
        type=pa.timestamp("us"),
    )
    timestamp_ns_no_tz = pa.array(
        [pd.Timestamp(year=2019, month=1, day=1, hour=14, microsecond=1)] * 5,
        type=pa.timestamp("ns"),
    )
    time32_s = pa.array([3600 + 120 + 3, None, 3, 4, 5], type=pa.time32("s"))
    time32_ms = pa.array(
        [(3600 + 120 + 3) * 1000 + 456, 2, 3, 4, 5], type=pa.time32("ms")
    )
    time64_us = pa.array([(3600 + 120 + 3) * 1e6, None, 3, 4, 5], type=pa.time64("us"))
    time64_ns = pa.array(
        [(3600 + 120 + 3) * 1e9 + 456, 2, 3, 4, 5], type=pa.time64("ns")
    )
    date32 = pa.array([1, 2, 3, 4, 5], type=pa.date32())
    date64 = pa.array([86400 * 1000, 2, 3, 4, 5], type=pa.date64())
    duration_s = pa.array([1, 2, 3, 4, 5], type=pa.duration("s"))
    duration_ms = pa.array([1, 2, 3, 4, 5], type=pa.duration("ms"))
    binary = pa.array([b"\x00\x01"] * 5, type=pa.binary())
    large_binary = pa.array([b"\x00\x01"] * 5, type=pa.large_binary())
    fixed_size_binary = pa.array(
        [b"\x00\x01", b"\x00\x00", b"\x01\x01", b"\x01\x00", b"\x00\x01"],
        type=pa.binary(2),
    )
    decimal128 = pa.array(
        [
            decimal.Decimal("1234.567"),
            decimal.Decimal("-1234.567"),
            None,
            decimal.Decimal("1234.567"),
            decimal.Decimal("-1234.567"),
        ],
        type=pa.decimal128(7, 3),
    )
    decimal256 = pa.array(
        [
            decimal.Decimal("1234.567"),
            decimal.Decimal("-1234.567"),
            None,
            decimal.Decimal("1234.567"),
            decimal.Decimal("-1234.567"),
        ],
        type=pa.decimal256(7, 3),
    )
    list_boolean = pa.array(
        [
            (
                None
                if i == 2
                else [
                    None if j == 0 else True if (j % 2) == 0 else False
                    for j in range(i)
                ]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.bool_()),
    )
    list_uint8 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.uint8()),
    )
    list_int8 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.int8()),
    )
    list_uint16 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.uint16()),
    )
    list_int16 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.int16()),
    )
    list_uint32 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.uint32()),
    )
    list_int32 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.int32()),
    )
    list_uint64 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.uint64()),
    )
    list_int64 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.int64()),
    )
    list_float32 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else 0.5 + j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.float32()),
    )
    list_float64 = pa.array(
        [
            (
                None
                if i == 2
                else [None if j == 0 else 0.5 + j + i * (i - 1) // 2 for j in range(i)]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.float64()),
    )
    list_decimal128 = pa.array(
        [
            [decimal.Decimal("1234.567")],
            [decimal.Decimal("-1234.567")],
            None,
            [None],
            [decimal.Decimal("-1234.567")],
        ],
        type=pa.list_(pa.decimal128(7, 3)),
    )
    list_decimal256 = pa.array(
        [
            [decimal.Decimal("1234.567")],
            [decimal.Decimal("-1234.567")],
            None,
            [None],
            [decimal.Decimal("-1234.567")],
        ],
        type=pa.list_(pa.decimal256(7, 3)),
    )
    list_string = pa.array(
        [
            (
                None
                if i == 2
                else (
                    [None]
                    if i == 4
                    else [
                        "".join(["%c" % (65 + j + k) for k in range(1 + j)])
                        for j in range(i)
                    ]
                )
            )
            for i in range(5)
        ]
    )
    list_large_string = pa.array(
        [
            (
                None
                if i == 2
                else (
                    [None]
                    if i == 4
                    else [
                        "".join(["%c" % (65 + j + k) for k in range(1 + j)])
                        for j in range(i)
                    ]
                )
            )
            for i in range(5)
        ],
        type=pa.list_(pa.large_string()),
    )
    fixed_size_list_boolean = pa.array(
        [[True, False], [False, True], [True, False], [False, True], [True, False]],
        type=pa.list_(pa.bool_(), 2),
    )
    fixed_size_list_uint8 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.uint8(), 2)
    )
    fixed_size_list_int8 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.int8(), 2)
    )
    fixed_size_list_uint16 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.uint16(), 2)
    )
    fixed_size_list_int16 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.int16(), 2)
    )
    fixed_size_list_uint32 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.uint32(), 2)
    )
    fixed_size_list_int32 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.int32(), 2)
    )
    fixed_size_list_uint64 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.uint64(), 2)
    )
    fixed_size_list_int64 = pa.array(
        [[0, 1], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.int64(), 2)
    )
    fixed_size_list_float32 = pa.array(
        [[0, None], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.float32(), 2)
    )
    fixed_size_list_float64 = pa.array(
        [[0, None], [2, 3], [4, 5], [6, 7], [8, 9]], type=pa.list_(pa.float64(), 2)
    )
    fixed_size_list_string = pa.array(
        [["a", "b"], ["c", "d"], ["e", "f"], ["g", "h"], ["i", "j"]],
        type=pa.list_(pa.string(), 2),
    )
    struct_field = pa.array(
        [
            {"a": 1, "b": 2.5, "c": {"d": "e", "f": "g"}, "h": [5, 6], "i": 3},
            {"a": 2, "b": 3.5, "c": {"d": "e1", "f": "g1"}, "h": [7, 8, 9], "i": 4},
            {
                "a": 3,
                "b": 4.5,
                "c": {"d": "e23", "f": "g23"},
                "h": [10, 11, 12, 13],
                "i": 5,
            },
            {
                "a": 4,
                "b": 5.5,
                "c": {"d": "e345", "f": "g345"},
                "h": [14, 15, 16],
                "i": 6,
            },
            {
                "a": 5,
                "b": 6.5,
                "c": {"d": "e4567", "f": "g4567"},
                "h": [17, 18],
                "i": 7,
            },
        ]
    )

    list_struct = pa.array(
        [
            [{"a": 1, "b": 2.5}, {"a": 3, "c": 4.5}],
            [{"a": 2, "b": 2.5}, {"a": 3, "c": 4.5}],
            [{"a": 3, "b": 2.5}, {"a": 3, "c": 4.5}],
            [{"a": 4, "b": 2.5}, {"a": 3, "c": 4.5}],
            [{"a": 5, "b": 2.5}, {"a": 3, "c": 4.5}],
        ]
    )

    # struct_val = { "a": 5 }
    # for i in range(123):
    #    struct_val = { "a": struct_val }
    # struct_field = pa.array([struct_val] * 5)

    map_boolean = pa.array(
        [[("x", None), ("y", True)], [("z", True)], None, [], []],
        type=pa.map_(pa.string(), pa.bool_()),
    )
    map_uint8 = pa.array(
        [[("x", 1), ("y", None)], [("z", 3)], None, [], []],
        type=pa.map_(pa.string(), pa.uint8()),
    )
    map_int8 = pa.array(
        [[("x", 1), ("y", None)], [("z", 3)], None, [], []],
        type=pa.map_(pa.string(), pa.int8()),
    )
    map_uint16 = pa.array(
        [[("x", 1), ("y", None)], [("z", 3)], None, [], []],
        type=pa.map_(pa.string(), pa.uint16()),
    )
    map_int16 = pa.array(
        [[("x", 1), ("y", None)], [("z", 3)], None, [], []],
        type=pa.map_(pa.string(), pa.int16()),
    )
    map_uint32 = pa.array(
        [[("x", 4 * 1000 * 1000 * 1000), ("y", None)], [("z", 3)], None, [], []],
        type=pa.map_(pa.string(), pa.uint32()),
    )
    map_int32 = pa.array(
        [[("x", 2 * 1000 * 1000 * 1000), ("y", None)], [("z", 3)], None, [], []],
        type=pa.map_(pa.string(), pa.int32()),
    )
    map_uint64 = pa.array(
        [[("x", 4 * 1000 * 1000 * 1000 * 1000), ("y", None)], [("z", 3)], None, [], []],
        type=pa.map_(pa.string(), pa.uint64()),
    )
    map_int64 = pa.array(
        [
            [("x", -2 * 1000 * 1000 * 1000 * 1000), ("y", None)],
            [("z", 3)],
            None,
            [],
            [],
        ],
        type=pa.map_(pa.string(), pa.int64()),
    )
    map_float32 = pa.array(
        [[("x", 1.5), ("y", None)], [("z", 3)], None, [], []],
        type=pa.map_(pa.string(), pa.float32()),
    )
    map_float64 = pa.array(
        [[("x", 1.5), ("y", None)], [("z", 3)], None, [], []],
        type=pa.map_(pa.string(), pa.float64()),
    )
    map_decimal128 = pa.array(
        [
            [("x", decimal.Decimal("1234.567")), ("y", None)],
            [("z", decimal.Decimal("-1234.567"))],
            None,
            [],
            [],
        ],
        type=pa.map_(pa.string(), pa.decimal128(7, 3)),
    )
    map_decimal256 = pa.array(
        [
            [("x", decimal.Decimal("1234.567")), ("y", None)],
            [("z", decimal.Decimal("-1234.567"))],
            None,
            [],
            [],
        ],
        type=pa.map_(pa.string(), pa.decimal256(7, 3)),
    )
    map_string = pa.array(
        [[("x", "x_val"), ("y", None)], [("z", "z_val")], None, [], []],
        type=pa.map_(pa.string(), pa.string()),
    )
    map_large_string = pa.array(
        [[("x", "x_val"), ("y", None)], [("z", "z_val")], None, [], []],
        type=pa.map_(pa.string(), pa.large_string()),
    )
    map_list_string = pa.array(
        [[("x", ["x_val"]), ("y", None)], [("z", [None, "z_val"])], None, [], []],
        type=pa.map_(pa.string(), pa.list_(pa.string())),
    )
    map_large_list_string = pa.array(
        [[("x", ["x_val"]), ("y", None)], [("z", [None, "z_val"])], None, [], []],
        type=pa.map_(pa.string(), pa.large_list(pa.string())),
    )
    map_fixed_size_list_string = pa.array(
        [
            [("x", ["x_val", None]), ("y", [None, None])],
            [("z", [None, "z_val"])],
            None,
            [],
            [],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.string(), 2)),
    )

    indices = pa.array([0, 1, 2, None, 2], type=pa.int32())
    dictionary = pa.array(["foo", "bar", "baz"])
    dict = pa.DictionaryArray.from_arrays(indices, dictionary)

    map_list = pa.array(
        [[("x", []), ("y", [])], [("z", [])], None, [], []],
        type=pa.map_(pa.string(), pa.list_(pa.uint32())),
    )

    geometry = pa.array(
        [
            None if i == 1 else (b"\x01\x01\x00\x00\x00" + struct.pack("<dd", i, 2))
            for i in range(5)
        ],
        type=pa.binary(),
    )

    null = pa.array([None] * 5, type=pa.null())

    names = [
        "boolean",
        "null",
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
        "timestamp_us_no_tz",
        "timestamp_ns_no_tz",
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
        "list_decimal128",
        "list_decimal256",
        "list_string",
        "list_large_string",
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
        "list_struct",
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
        "map_decimal128",
        "map_decimal256",
        "map_string",
        "map_large_string",
        "map_list_string",
        "map_large_list_string",
        "map_fixed_size_list_string",
        "dict",
        "geometry",
    ]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    my_schema = table.schema.with_metadata(
        {
            "geo": json.dumps(
                {
                    "version": "0.1.0",
                    "primary_column": "geometry",
                    "columns": {
                        "geometry": {
                            "crs": wkt_epsg_4326,
                            "bbox": [0, 2, 4, 2],
                            "encoding": "WKB",
                        }
                    },
                }
            )
        }
    )

    table = table.cast(my_schema)
    HERE = pathlib.Path(__file__).parent
    pq.write_table(
        table,
        HERE / "data/parquet/test.parquet",
        compression="NONE",
        row_group_size=3,
        version="1.0",
    )
    pq.write_table(
        table,
        HERE / "data/parquet/test_single_group.parquet",
        compression="NONE",
        version="1.0",
    )

    import pyarrow.feather as feather

    float16 = pa.array(
        [None if i == 2 else np.float16(1.5 + i) for i in range(5)], type=pa.float16()
    )
    list_float16 = pa.array(
        [
            (
                None
                if i == 2
                else [
                    None if j == 0 else np.float16(0.5 + j + i * (i - 1) // 2)
                    for j in range(i)
                ]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.float16()),
    )
    map_float16 = pa.array(
        [[("x", np.float16(1.5)), ("y", None)], [("z", np.float16(3))], None, [], []],
        type=pa.map_(pa.string(), pa.float16()),
    )
    list_list_float16 = pa.array(
        [
            (
                None
                if i == 2
                else [
                    None if j == 0 else [np.float16(0.5 + j + i * (i - 1) // 2)]
                    for j in range(i)
                ]
            )
            for i in range(5)
        ],
        type=pa.list_(pa.list_(pa.float16())),
    )
    names += ["float16", "list_float16", "list_list_float16", "map_float16"]
    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    my_schema = table.schema.with_metadata(
        {
            "geo": json.dumps(
                {
                    "version": "0.1.0",
                    "primary_column": "geometry",
                    "columns": {
                        "geometry": {
                            "crs": wkt_epsg_4326,
                            "bbox": [0, 2, 4, 2],
                            "encoding": "WKB",
                        }
                    },
                }
            )
        }
    )

    table = table.cast(my_schema)

    feather.write_feather(table, HERE / "data/arrow/test.feather")


def generate_all_geoms_parquet():
    import json
    import pathlib

    import pyarrow as pa
    import pyarrow.parquet as pq

    from osgeo import ogr

    g1 = ogr.CreateGeometryFromWkt("POINT(1 2)")
    g2 = ogr.CreateGeometryFromWkt("LINESTRING(3 4,5 6)")
    g3 = ogr.CreateGeometryFromWkt(
        "POLYGON((10 0,11 0,11 -1,11 0,10 0),(10.2 -0.2,10.8 -0.2,10.8 -0.8,10.2 -0.8,10.2 -0.2))"
    )
    g4 = ogr.CreateGeometryFromWkt("MULTIPOINT(7 8,9 10)")
    g5 = ogr.CreateGeometryFromWkt("MULTILINESTRING((11 12,13 14),(15 16,17 18))")
    g6 = ogr.CreateGeometryFromWkt(
        "MULTIPOLYGON(((100 0,101 0,101 1,101 0,100 0),(100.2 0.2,100.8 0.2,100.8 0.8,100.2 0.8,100.2 0.2)))"
    )
    g7 = ogr.CreateGeometryFromWkt(
        "GEOMETRYCOLLECTION(POINT(19 20),LINESTRING(21 22, 23 24))"
    )
    geometry = pa.array(
        [x.ExportToWkb(byte_order=ogr.wkbXDR) for x in (g1, g2, g3, g4, g5, g6, g7)],
        type=pa.binary(),
    )
    names = ["geometry"]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    my_schema = table.schema.with_metadata(
        {
            "geo": json.dumps(
                {
                    "version": "0.1.0",
                    "primary_column": "geometry",
                    "columns": {"geometry": {"crs": wkt_epsg_4326, "encoding": "WKB"}},
                }
            )
        }
    )

    table = table.cast(my_schema)
    HERE = pathlib.Path(__file__).parent
    pq.write_table(table, HERE / "data/parquet/all_geoms.parquet", compression="NONE")


def generate_parquet_wkt_with_dict():
    import json
    import pathlib

    import pyarrow as pa
    import pyarrow.parquet as pq

    geometry = pa.array(
        ["POINT (1 2)", "POINT (3 4)", None, "POINT (7 8)", "POINT (9 10)"],
        type=pa.string(),
    )

    indices = pa.array([0, 1, 2, None, 2])
    dictionary = pa.array(["foo", "bar", "baz"])
    dict = pa.DictionaryArray.from_arrays(indices, dictionary)

    names = ["geometry", "dict"]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    my_schema = table.schema.with_metadata(
        {
            "geo": json.dumps(
                {
                    "version": "0.1.0",
                    "primary_column": "geometry",
                    "columns": {"geometry": {"encoding": "WKT"}},
                }
            )
        }
    )

    table = table.cast(my_schema)
    HERE = pathlib.Path(__file__).parent
    pq.write_table(
        table,
        HERE / "data/parquet/wkt_with_dict.parquet",
        compression="NONE",
        row_group_size=3,
    )


def generate_nested_types():
    import decimal
    import pathlib

    import numpy as np
    import pyarrow as pa
    import pyarrow.parquet as pq

    map_list_bool = pa.array(
        [
            [("x", [True]), ("y", [False, True])],
            [("z", [])],
            None,
            [("w", [True, False])],
            [("null", None)],
        ],
        type=pa.map_(pa.string(), pa.list_(pa.bool_())),
    )

    map_list_uint8 = pa.array(
        [[("x", [2]), ("y", [3, 4])], [("z", [])], None, [("w", [5, 6])], []],
        type=pa.map_(pa.string(), pa.list_(pa.uint8())),
    )

    map_list_int8 = pa.array(
        [[("x", [2]), ("y", [3, 4])], [("z", [])], None, [("w", [5, 6])], []],
        type=pa.map_(pa.string(), pa.list_(pa.int8())),
    )

    map_list_uint16 = pa.array(
        [[("x", [2]), ("y", [3, 4])], [("z", [])], None, [("w", [5, 6])], []],
        type=pa.map_(pa.string(), pa.list_(pa.uint16())),
    )

    map_list_int16 = pa.array(
        [[("x", [2]), ("y", [3, 4])], [("z", [])], None, [("w", [5, 6])], []],
        type=pa.map_(pa.string(), pa.list_(pa.int16())),
    )

    map_list_uint32 = pa.array(
        [[("x", [2]), ("y", [3, 4])], [("z", [])], None, [("w", [5, 6])], []],
        type=pa.map_(pa.string(), pa.list_(pa.uint32())),
    )

    map_list_int32 = pa.array(
        [[("x", [2]), ("y", [3, 4])], [("z", [])], None, [("w", [5, 6])], []],
        type=pa.map_(pa.string(), pa.list_(pa.int32())),
    )

    map_list_uint64 = pa.array(
        [[("x", [2]), ("y", [3, 4])], [("z", [])], None, [("w", [5, 6])], []],
        type=pa.map_(pa.string(), pa.list_(pa.uint64())),
    )

    map_list_int64 = pa.array(
        [[("x", [2]), ("y", [3, 4])], [("z", [])], None, [("w", [5, 6])], []],
        type=pa.map_(pa.string(), pa.list_(pa.int64())),
    )

    map_list_float32 = pa.array(
        [[("x", [2]), ("y", [3, 4])], [("z", [])], None, [("w", [5, 6])], []],
        type=pa.map_(pa.string(), pa.list_(pa.float32())),
    )

    map_list_float64 = pa.array(
        [[("x", [2]), ("y", [3, 4])], [("z", [])], None, [("w", [5, 6])], []],
        type=pa.map_(pa.string(), pa.list_(pa.float64())),
    )

    map_map_bool = pa.array(
        [
            [("a", [("b", True), ("c", None), ("d", None)]), ("e", None)],
            None,
            [("f", [("g", False)])],
            None,
            None,
        ],
        type=pa.map_(pa.string(), pa.map_(pa.string(), pa.bool_())),
    )

    map_map_uint8 = pa.array(
        [
            [("a", [("b", 1), ("c", None), ("d", 2)]), ("e", None)],
            None,
            [("f", [("g", 3)])],
            None,
            None,
        ],
        type=pa.map_(pa.string(), pa.map_(pa.string(), pa.uint8())),
    )

    map_map_int8 = pa.array(
        [
            [("a", [("b", 1), ("c", None), ("d", 2)]), ("e", None)],
            None,
            [("f", [("g", 3)])],
            None,
            None,
        ],
        type=pa.map_(pa.string(), pa.map_(pa.string(), pa.int8())),
    )

    map_map_uint16 = pa.array(
        [
            [("a", [("b", 1), ("c", None), ("d", 2)]), ("e", None)],
            None,
            [("f", [("g", 3)])],
            None,
            None,
        ],
        type=pa.map_(pa.string(), pa.map_(pa.string(), pa.uint16())),
    )

    map_map_int16 = pa.array(
        [
            [("a", [("b", 1), ("c", None), ("d", 2)]), ("e", None)],
            None,
            [("f", [("g", 3)])],
            None,
            None,
        ],
        type=pa.map_(pa.string(), pa.map_(pa.string(), pa.int16())),
    )

    map_map_uint32 = pa.array(
        [
            [("a", [("b", 1), ("c", None), ("d", 2)]), ("e", None)],
            None,
            [("f", [("g", 3)])],
            None,
            None,
        ],
        type=pa.map_(pa.string(), pa.map_(pa.string(), pa.uint32())),
    )

    map_map_int32 = pa.array(
        [
            [("a", [("b", 1), ("c", None), ("d", 2)]), ("e", None)],
            None,
            [("f", [("g", 3)])],
            None,
            None,
        ],
        type=pa.map_(pa.string(), pa.map_(pa.string(), pa.int32())),
    )

    map_map_uint64 = pa.array(
        [
            [("a", [("b", 1), ("c", None), ("d", 2)]), ("e", None)],
            None,
            [("f", [("g", 3)])],
            None,
            None,
        ],
        type=pa.map_(pa.string(), pa.map_(pa.string(), pa.uint64())),
    )

    map_map_int64 = pa.array(
        [
            [("a", [("b", 1), ("c", None), ("d", 2)]), ("e", None)],
            None,
            [("f", [("g", 3)])],
            None,
            None,
        ],
        type=pa.map_(pa.string(), pa.map_(pa.string(), pa.int64())),
    )

    map_map_float32 = pa.array(
        [
            [("a", [("b", 1), ("c", None), ("d", 2)]), ("e", None)],
            None,
            [("f", [("g", 3)])],
            None,
            None,
        ],
        type=pa.map_(pa.string(), pa.map_(pa.string(), pa.float32())),
    )

    map_map_float64 = pa.array(
        [
            [("a", [("b", 1), ("c", None), ("d", 2)]), ("e", None)],
            None,
            [("f", [("g", 3)])],
            None,
            None,
        ],
        type=pa.map_(pa.string(), pa.map_(pa.string(), pa.float64())),
    )

    map_map_string = pa.array(
        [
            [("a", [("b", "c"), ("d", None)]), ("e", None)],
            None,
            [("f", [("g", "h")])],
            None,
            None,
        ],
        type=pa.map_(pa.string(), pa.map_(pa.string(), pa.string())),
    )

    list_list_bool = pa.array(
        [[[True], None, [False, None, True]], None, [[False]], [], []],
        type=pa.list_(pa.list_(pa.bool_())),
    )

    list_list_uint8 = pa.array(
        [[[1], None, [2, None, 3]], None, [[4]], [], []],
        type=pa.list_(pa.list_(pa.uint8())),
    )

    list_list_int8 = pa.array(
        [[[1], None, [-2, None, 3]], None, [[-4]], [], []],
        type=pa.list_(pa.list_(pa.int8())),
    )

    list_list_uint16 = pa.array(
        [[[1], None, [2, None, 3]], None, [[4]], [], []],
        type=pa.list_(pa.list_(pa.uint16())),
    )

    list_list_int16 = pa.array(
        [[[1], None, [-2, None, 3]], None, [[-4]], [], []],
        type=pa.list_(pa.list_(pa.int16())),
    )

    list_list_uint32 = pa.array(
        [[[1], None, [2, None, 3]], None, [[4]], [], []],
        type=pa.list_(pa.list_(pa.uint32())),
    )

    list_list_int32 = pa.array(
        [[[1], None, [-2, None, 3]], None, [[-4]], [], []],
        type=pa.list_(pa.list_(pa.int32())),
    )

    list_list_uint64 = pa.array(
        [[[1], None, [2, None, 3]], None, [[4]], [], []],
        type=pa.list_(pa.list_(pa.uint64())),
    )

    list_list_int64 = pa.array(
        [[[1], None, [-2, None, 3]], None, [[-4]], [], []],
        type=pa.list_(pa.list_(pa.int64())),
    )

    list_list_float16 = pa.array(
        [
            [[np.float16(1.5)], None, [np.float16(2.5), None, np.float16(3.5)]],
            None,
            [[np.float16(4.5)]],
            [],
            [],
        ],
        type=pa.list_(pa.list_(pa.float16())),
    )

    list_list_float32 = pa.array(
        [[[1.5], None, [-2.5, None, 3.5]], None, [[-4.5]], [], []],
        type=pa.list_(pa.list_(pa.float32())),
    )

    list_list_float64 = pa.array(
        [[[1.5], None, [-2.5, None, 3.5]], None, [[-4.5]], [], []],
        type=pa.list_(pa.list_(pa.float64())),
    )

    list_list_decimal128 = pa.array(
        [
            [
                [decimal.Decimal("1234.567")],
                None,
                [decimal.Decimal("-1234.567"), None, decimal.Decimal("1234.567")],
            ],
            None,
            [[decimal.Decimal("-1234.567")]],
            [],
            [],
        ],
        type=pa.list_(pa.list_(pa.decimal128(7, 3))),
    )

    list_list_decimal256 = pa.array(
        [
            [
                [decimal.Decimal("1234.567")],
                None,
                [decimal.Decimal("-1234.567"), None, decimal.Decimal("1234.567")],
            ],
            None,
            [[decimal.Decimal("-1234.567")]],
            [],
            [],
        ],
        type=pa.list_(pa.list_(pa.decimal256(7, 3))),
    )

    list_list_string = pa.array(
        [[["a"], None, ["b", None, "cd"]], None, [["efg"]], [], []],
        type=pa.list_(pa.list_(pa.string())),
    )

    list_list_binary = pa.array(
        [[["a"], None, ["b", None, "cd"]], None, [[b"\x01\x02"]], [], []],
        type=pa.list_(pa.list_(pa.binary())),
    )

    list_list_large_string = pa.array(
        [[["a"], None, ["b", None, "cd"]], None, [["efg"]], [], []],
        type=pa.list_(pa.list_(pa.large_string())),
    )

    list_large_list_string = pa.array(
        [[["a"], None, ["b", None, "cd"]], None, [["efg"]], [], []],
        type=pa.list_(pa.large_list(pa.string())),
    )

    list_fixed_size_list_string = pa.array(
        [[["a", "b"]], None, [["e", "f"]], [["g", "h"]], [["i", "j"]]],
        type=pa.list_(pa.list_(pa.string(), 2)),
    )

    list_map_string = pa.array(
        [[[("a", "b"), ("c", "d")], [("e", "f")]], None, [None], [], []],
        type=pa.list_(pa.map_(pa.string(), pa.string())),
    )

    names = [
        "map_list_bool",
        "map_list_uint8",
        "map_list_int8",
        "map_list_uint16",
        "map_list_int16",
        "map_list_uint32",
        "map_list_int32",
        "map_list_uint64",
        "map_list_int64",
        "map_list_float32",
        "map_list_float64",
        "map_map_bool",
        "map_map_uint8",
        "map_map_int8",
        "map_map_uint16",
        "map_map_int16",
        "map_map_uint32",
        "map_map_int32",
        "map_map_uint64",
        "map_map_int64",
        "map_map_float32",
        "map_map_float64",
        "map_map_string",
        "list_list_bool",
        "list_list_uint8",
        "list_list_int8",
        "list_list_uint16",
        "list_list_int16",
        "list_list_uint32",
        "list_list_int32",
        "list_list_uint64",
        "list_list_int64",
        # "list_list_float16",
        "list_list_float32",
        "list_list_float64",
        "list_list_decimal128",
        "list_list_decimal256",
        "list_list_string",
        "list_list_large_string",
        "list_list_binary",
        "list_large_list_string",
        "list_fixed_size_list_string",
        "list_map_string",
    ]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    HERE = pathlib.Path(__file__).parent
    pq.write_table(
        table,
        HERE / "data/parquet/nested_types.parquet",
        compression="NONE",
        row_group_size=3,
    )


def generate_extension_custom():
    import pathlib

    import pyarrow as pa
    import pyarrow.feather as feather
    import pyarrow.parquet as pq

    class MyJsonType(pa.ExtensionType):
        def __init__(self):
            super().__init__(pa.string(), "my_json")

        def __arrow_ext_serialize__(self):
            return b""

        @classmethod
        def __arrow_ext_deserialize__(cls, storage_type, serialized):
            return cls()

    my_json_type = MyJsonType()
    storage_array = pa.array(['{"foo":"bar"}'], pa.string())
    extension_custom = pa.ExtensionArray.from_storage(my_json_type, storage_array)

    names = ["extension_custom"]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    HERE = pathlib.Path(__file__).parent
    feather.write_feather(table, HERE / "data/arrow/extension_custom.feather")
    pq.write_table(
        table, HERE / "data/parquet/extension_custom.parquet", compression="NONE"
    )


def generate_extension_json():
    import pathlib

    import pyarrow as pa
    import pyarrow.feather as feather
    import pyarrow.parquet as pq

    class JsonType(pa.ExtensionType):
        def __init__(self):
            super().__init__(pa.string(), "arrow.json")

        def __arrow_ext_serialize__(self):
            return b""

        @classmethod
        def __arrow_ext_deserialize__(cls, storage_type, serialized):
            return cls()

    json_type = JsonType()
    storage_array = pa.array(['{"foo":"bar"}'], pa.string())
    extension_json = pa.ExtensionArray.from_storage(json_type, storage_array)

    names = ["extension_json"]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    HERE = pathlib.Path(__file__).parent
    feather.write_feather(table, HERE / "data/arrow/extension_json.feather")
    pq.write_table(
        table, HERE / "data/parquet/extension_json.parquet", compression="NONE"
    )


def generate_arrow_stringview():
    import pathlib

    import pyarrow as pa
    import pyarrow.feather as feather

    stringview = pa.array(["foo", "bar", "looooooooooong string"], pa.string_view())
    list_stringview = pa.array(
        [None, [None], ["foo", "bar", "looooooooooong string"]],
        pa.list_(pa.string_view()),
    )
    list_of_list_stringview = pa.array(
        [None, [None], [["foo", "bar", "looooooooooong string"]]],
        pa.list_(pa.list_(pa.string_view())),
    )
    map_stringview = pa.array(
        [None, [], [("x", "x_val"), ("y", None)]],
        type=pa.map_(pa.string_view(), pa.string_view()),
    )

    names = [
        "stringview",
        "list_stringview",
        "list_of_list_stringview",
        "map_stringview",
    ]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    HERE = pathlib.Path(__file__).parent
    feather.write_feather(table, HERE / "data/arrow/stringview.feather")


def generate_arrow_binaryview():
    import pathlib

    import pyarrow as pa
    import pyarrow.feather as feather

    binaryview = pa.array([b"foo", b"bar", b"looooooooooong binary"], pa.binary_view())

    names = ["binaryview"]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    HERE = pathlib.Path(__file__).parent
    feather.write_feather(table, HERE / "data/arrow/binaryview.feather")


def generate_arrow_listview():
    import pathlib

    import pyarrow as pa
    import pyarrow.feather as feather

    listview = pa.array([[1]], pa.list_view(pa.int32()))

    names = ["listview"]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    HERE = pathlib.Path(__file__).parent
    feather.write_feather(table, HERE / "data/arrow/listview.feather")


def generate_arrow_largelistview():
    import pathlib

    import pyarrow as pa
    import pyarrow.feather as feather

    largelistview = pa.array([[1]], pa.large_list_view(pa.int32()))

    names = ["largelistview"]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    HERE = pathlib.Path(__file__).parent
    feather.write_feather(table, HERE / "data/arrow/largelistview.feather")


def generate_parquet_list_binary():
    import pathlib

    import pyarrow as pa
    import pyarrow.parquet as pq

    list_binary = pa.array(
        [None, [None], ["foo", "bar", b"\x01"]],
        pa.list_(pa.binary()),
    )
    names = ["list_binary"]

    locals_ = locals()
    table = pa.table([locals_[x] for x in names], names=names)

    HERE = pathlib.Path(__file__).parent
    pq.write_table(table, HERE / "data/parquet/list_binary.parquet")


if __name__ == "__main__":
    generate_test_parquet()
    generate_all_geoms_parquet()
    generate_parquet_wkt_with_dict()
    generate_nested_types()
    generate_extension_custom()
    generate_extension_json()
    generate_arrow_stringview()
    generate_arrow_binaryview()
    generate_arrow_listview()
    generate_arrow_largelistview()
    generate_parquet_list_binary()
