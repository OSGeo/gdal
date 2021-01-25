#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support from a Zarr array.
# Author:   David Brochart, david.brochart@gmail.com
#
###############################################################################
# Copyright (c) 2021, David Brochart <david.brochart@gmail.com>
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

import pytest

from osgeo import gdal


@pytest.mark.require_driver('Zarr')
def test_zarr_from_local_fs():
    ds = gdal.Open('data/zarr')

    assert ds is not None
    b = ds.GetRasterBand(1)
    assert b.YSize == 5
    assert b.XSize == 10
    assert b.GetBlockSize() == [5, 2]
    assert b.Checksum() == 285


@pytest.mark.require_driver('Zarr')
def test_zarr_from_vsimem():
    filenames = ['0.0', '.zattrs', '.zarray']
    gdal.Mkdir('/vsimem/zarr', 777)
    for fname in filenames:
        gdal.FileFromMemBuffer('/vsimem/zarr/' + fname, open('data/zarr/' + fname, 'rb').read())

    ds = gdal.Open('/vsimem/zarr')

    assert ds is not None
    b = ds.GetRasterBand(1)
    assert b.YSize == 5
    assert b.XSize == 10
    assert b.GetBlockSize() == [5, 2]
    assert b.Checksum() == 285

    gdal.RmdirRecursive('/vsimem/zarr')
