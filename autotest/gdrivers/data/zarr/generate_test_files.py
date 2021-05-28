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
