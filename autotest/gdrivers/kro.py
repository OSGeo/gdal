#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Even Rouault, <even dot rouault at spatialys.com>
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal


import gdaltest
import pytest

###############################################################################
# Create 3-band byte


def test_kro_1():

    tst = gdaltest.GDALTest('KRO', 'rgbsmall.tif', 2, 21053)

    return tst.testCreate()

###############################################################################
# Create 1-band uint16


def test_kro_2():

    tst = gdaltest.GDALTest('KRO', '../../gcore/data/uint16.tif', 1, 4672)

    return tst.testCreate()

###############################################################################
# Create 1-band float32


def test_kro_3():

    tst = gdaltest.GDALTest('KRO', '../../gcore/data/float32.tif', 1, 4672)

    return tst.testCreate()

###############################################################################
# Create 4-band rgba uint16


def test_kro_4():

    tst = gdaltest.GDALTest('KRO', 'png/rgba16.png', 1, 1886)

    return tst.testCreate()

###############################################################################
# Test optimized IO


def test_kro_5():

    # Determine if the filesystem supports sparse files (we don't want to create a real 10 GB
    # file !
    if not gdaltest.filesystem_supports_sparse_files('tmp'):
        pytest.skip()

    ds = gdal.GetDriverByName('KRO').Create('tmp/kro_5.kro', 100000, 10000, 4)
    ds = None

    ds = gdal.Open('tmp/kro_5.kro')
    ds.ReadRaster(int(ds.RasterXSize / 2), int(ds.RasterYSize / 2), 100, 100)
    ds = None

    gdal.Unlink('tmp/kro_5.kro')




