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

import lzma
import os

import numpy as np
import zarr
from numcodecs import (
    LZ4,
    LZMA,
    Blosc,
    FixedScaleOffset,
    GZip,
    Quantize,
    Shuffle,
    Zlib,
    Zstd,
)

os.chdir(os.path.dirname(__file__))

z = zarr.open(
    "blosc.zarr", mode="w", dtype="u1", shape=(2,), chunks=(2,), compressor=Blosc()
)
z[:] = [1, 2]


z = zarr.open(
    "zlib.zarr", mode="w", dtype="u1", shape=(2,), chunks=(2,), compressor=Zlib(level=1)
)
z[:] = [1, 2]


z = zarr.open(
    "gzip.zarr", mode="w", dtype="u1", shape=(2,), chunks=(2,), compressor=GZip(level=1)
)
z[:] = [1, 2]


z = zarr.open(
    "lzma.zarr", mode="w", dtype="u1", shape=(2,), chunks=(2,), compressor=LZMA()
)
z[:] = [1, 2]


z = zarr.open(
    "lzma_with_filters.zarr",
    mode="w",
    dtype="u1",
    shape=(2,),
    chunks=(2,),
    compressor=LZMA(
        filters=[
            dict(id=lzma.FILTER_DELTA, dist=4),
            dict(id=lzma.FILTER_LZMA2, preset=1),
        ]
    ),
)
z[:] = [1, 2]

z = zarr.open(
    "lz4.zarr", mode="w", dtype="u1", shape=(2,), chunks=(2,), compressor=LZ4()
)
z[:] = [1, 2]

z = zarr.open(
    "zstd.zarr", mode="w", dtype="u1", shape=(2,), chunks=(2,), compressor=Zstd()
)
z[:] = [1, 2]

z = zarr.open(
    "shuffle.zarr",
    mode="w",
    dtype="u2",
    shape=(2,),
    chunks=(2,),
    compressor=None,
    filters=[Shuffle(elementsize=2)],
)
z[:] = [1, 2]


z = zarr.open(
    "order_f_u1.zarr",
    order="F",
    mode="w",
    dtype="u1",
    shape=(4, 4),
    chunks=(2, 3),
    compressor=None,
)
z[:] = np.reshape(np.arange(0, 16, dtype=np.uint8), (4, 4))

z = zarr.open(
    "order_f_u2.zarr",
    order="F",
    mode="w",
    dtype="u2",
    shape=(4, 4),
    chunks=(2, 3),
    compressor=None,
)
z[:] = np.reshape(np.arange(0, 16, dtype=np.uint16), (4, 4))

z = zarr.open(
    "order_f_u4.zarr",
    order="F",
    mode="w",
    dtype="u4",
    shape=(4, 4),
    chunks=(2, 3),
    compressor=None,
)
z[:] = np.reshape(np.arange(0, 16, dtype=np.uint32), (4, 4))

z = zarr.open(
    "order_f_u8.zarr",
    order="F",
    mode="w",
    dtype="u8",
    shape=(4, 4),
    chunks=(2, 3),
    compressor=None,
)
z[:] = np.reshape(np.arange(0, 16, dtype=np.uint64), (4, 4))

z = zarr.open(
    "order_f_s3.zarr",
    order="F",
    mode="w",
    dtype="S3",
    shape=(4, 4),
    chunks=(2, 3),
    compressor=None,
)
z[:] = [
    ["000", "111", "222", "333"],
    ["444", "555", "666", "777"],
    ["888", "999", "AAA", "BBB"],
    ["CCC", "DDD", "EEE", "FFF"],
]

z = zarr.open(
    "order_f_u1_3d.zarr",
    order="F",
    mode="w",
    dtype="u1",
    shape=(2, 3, 4),
    chunks=(2, 3, 4),
    compressor=None,
)
z[:] = np.reshape(np.arange(0, 2 * 3 * 4, dtype=np.uint8), (2, 3, 4))


z = zarr.open(
    "compound_well_aligned.zarr",
    mode="w",
    dtype=[("a", "u2"), ("b", "u2")],
    shape=(3,),
    chunks=(2,),
    compressor=None,
)
z[0] = (1000, 3000)
z[1] = (4000, 5000)


z = zarr.open(
    "compound_not_aligned.zarr",
    mode="w",
    dtype=[("a", "u2"), ("b", "u1"), ("c", "u2")],
    shape=(3,),
    chunks=(2,),
    compressor=None,
)
z[0] = (1000, 2, 3000)
z[1] = (4000, 4, 5000)


z = zarr.open(
    "compound_complex.zarr",
    mode="w",
    dtype=[
        ("a", "u1"),
        ("b", [("b1", "u1"), ("b2", "u1"), ("b3", "u2"), ("b5", "u1")]),
        ("c", "S3"),
        ("d", "i1"),
    ],
    shape=(2,),
    chunks=(1,),
    fill_value=(2, (255, 254, 65534, 253), "ZZ", -2),
    compressor=None,
)
z[0] = (1, (2, 3, 1000, 4), "AAA", -1)


data = np.arange(100, dtype="f8").reshape(10, 10) / 10
quantize = Quantize(digits=1, dtype="f8", astype="f4")
z = zarr.open(
    "quantize.zarr",
    mode="w",
    dtype="f8",
    shape=(10, 10),
    chunks=(10, 10),
    compressor=None,
    filters=[quantize],
)
z[:] = data


data = np.linspace(1000, 1001, 10, dtype="f8")

filter = FixedScaleOffset(offset=1000, scale=10, dtype="f8", astype="u1")
z = zarr.open(
    "fixedscaleoffset_dtype_f8_astype_u1.zarr",
    mode="w",
    dtype="f8",
    shape=(10),
    chunks=(10),
    compressor=None,
    filters=[filter],
)
z[:] = data

filter = FixedScaleOffset(offset=1000, scale=10, dtype="f8", astype="u2")
z = zarr.open(
    "fixedscaleoffset_dtype_f8_astype_u2.zarr",
    mode="w",
    dtype="f8",
    shape=(10),
    chunks=(10),
    compressor=None,
    filters=[filter],
)
z[:] = data

filter = FixedScaleOffset(offset=1000, scale=10, dtype="f8", astype="u4")
z = zarr.open(
    "fixedscaleoffset_dtype_f8_astype_u4.zarr",
    mode="w",
    dtype="f8",
    shape=(10),
    chunks=(10),
    compressor=None,
    filters=[filter],
)
z[:] = data

filter = FixedScaleOffset(offset=1000, scale=10, dtype="f4", astype="u1")
z = zarr.open(
    "fixedscaleoffset_dtype_f4_astype_u1.zarr",
    mode="w",
    dtype="f4",
    shape=(10),
    chunks=(10),
    compressor=None,
    filters=[filter],
)
z[:] = data
