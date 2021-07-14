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

import lzma
import os
import zarr
import numpy as np
from numcodecs import Blosc, GZip, Zlib, LZMA, Zstd, LZ4

os.chdir(os.path.dirname(__file__))

z = zarr.open('blosc.zarr', mode='w', dtype='u1',
              shape=(2,), chunks=(2,), compressor=Blosc())
z[:] = [1, 2]


z = zarr.open('zlib.zarr', mode='w', dtype='u1',
              shape=(2,), chunks=(2,), compressor=Zlib(level=1))
z[:] = [1, 2]


z = zarr.open('gzip.zarr', mode='w', dtype='u1',
              shape=(2,), chunks=(2,), compressor=GZip(level=1))
z[:] = [1, 2]


z = zarr.open('lzma.zarr', mode='w', dtype='u1',
              shape=(2,), chunks=(2,), compressor=LZMA())
z[:] = [1, 2]


z = zarr.open('lzma_with_filters.zarr', mode='w', dtype='u1',
              shape=(2,), chunks=(2,), compressor=LZMA(filters=[dict(id=lzma.FILTER_DELTA, dist=4),
                                                                dict(id=lzma.FILTER_LZMA2, preset=1)]))
z[:] = [1, 2]

z = zarr.open('lz4.zarr', mode='w', dtype='u1',
              shape=(2,), chunks=(2,), compressor=LZ4())
z[:] = [1, 2]

z = zarr.open('zstd.zarr', mode='w', dtype='u1',
              shape=(2,), chunks=(2,), compressor=Zstd())
z[:] = [1, 2]


z = zarr.open('order_f_u1.zarr', order='F', mode='w', dtype='u1',
              shape=(4, 4), chunks=(2, 3), compressor=None)
z[:] = np.reshape(np.arange(0, 16, dtype=np.uint8), (4, 4))

z = zarr.open('order_f_u2.zarr', order='F', mode='w', dtype='u2',
              shape=(4, 4), chunks=(2, 3), compressor=None)
z[:] = np.reshape(np.arange(0, 16, dtype=np.uint16), (4, 4))

z = zarr.open('order_f_u4.zarr', order='F', mode='w', dtype='u4',
              shape=(4, 4), chunks=(2, 3), compressor=None)
z[:] = np.reshape(np.arange(0, 16, dtype=np.uint32), (4, 4))

z = zarr.open('order_f_u8.zarr', order='F', mode='w', dtype='u8',
              shape=(4, 4), chunks=(2, 3), compressor=None)
z[:] = np.reshape(np.arange(0, 16, dtype=np.uint64), (4, 4))

z = zarr.open('order_f_s3.zarr', order='F', mode='w', dtype='S3',
              shape=(4, 4), chunks=(2, 3), compressor=None)
z[:] = [['000', '111', '222', '333'],
        ['444', '555', '666', '777'],
        ['888', '999', 'AAA', 'BBB'],
        ['CCC', 'DDD', 'EEE', 'FFF']]

z = zarr.open('order_f_u1_3d.zarr', order='F', mode='w', dtype='u1',
              shape=(2, 3, 4), chunks=(2, 3, 4), compressor=None)
z[:] = np.reshape(np.arange(0, 2 * 3 * 4, dtype=np.uint8), (2, 3, 4))


z = zarr.open('compound_well_aligned.zarr', mode='w', dtype=[('a', 'u2'), ('b', 'u2')],
              shape=(3,), chunks=(2,), compressor=None)
z[0] = (1000, 3000)
z[1] = (4000, 5000)


z = zarr.open('compound_not_aligned.zarr', mode='w', dtype=[('a', 'u2'), ('b', 'u1'), ('c', 'u2')],
              shape=(3,), chunks=(2,), compressor=None)
z[0] = (1000, 2, 3000)
z[1] = (4000, 4, 5000)


z = zarr.open('compound_complex.zarr', mode='w', dtype=[('a', 'u1'), ('b', [('b1', 'u1'), ('b2', 'u1'), ('b3', 'u2'), ('b5', 'u1')]), ('c', 'S3'), ('d', 'i1')],
              shape=(2,), chunks=(1,), fill_value=(2, (255, 254, 65534, 253), 'ZZ', -2), compressor=None)
z[0] = (1, (2, 3, 1000, 4), 'AAA', -1)
