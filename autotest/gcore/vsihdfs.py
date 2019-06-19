#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VSI file primitives
# Author:   James McClain <jmcclain@azavea.com>
#
###############################################################################
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
# Copyright (c) 2018, Azavea
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

import os
from osgeo import gdal


import gdaltest
import pytest

# Read test
def test_vsihdfs_1():
    filename = '/vsihdfs/file:' + os.getcwd() + '/data/text.txt'
    fp = gdal.VSIFOpenL(filename, 'rb')
    if fp is None:
        gdaltest.have_vsihdfs = False
        pytest.skip()

    gdaltest.have_vsihdfs = True

    data = gdal.VSIFReadL(5, 1, fp)
    assert data and data.decode('ascii') == 'Lorem'

    data = gdal.VSIFReadL(1, 6, fp)
    assert data and data.decode('ascii') == ' ipsum'

    gdal.VSIFCloseL(fp)

# Seek test
def test_vsihdfs_2():
    if gdaltest.have_vsihdfs == False:
        pytest.skip()

    filename = '/vsihdfs/file:' + os.getcwd() + '/data/text.txt'
    fp = gdal.VSIFOpenL(filename, 'rb')
    assert fp is not None

    gdal.VSIFSeekL(fp, 2, 0) # From beginning
    gdal.VSIFSeekL(fp, 5, 0)
    data = gdal.VSIFReadL(6, 1, fp)
    assert data and data.decode('ascii') == ' ipsum'

    gdal.VSIFSeekL(fp, 7, 1) # From current
    data = gdal.VSIFReadL(3, 1, fp)
    assert data and data.decode('ascii') == 'sit'

    gdal.VSIFSeekL(fp, 9, 2) # From end
    data = gdal.VSIFReadL(7, 1, fp)
    assert data and data.decode('ascii') == 'laborum'

    gdal.VSIFCloseL(fp)

# Tell test
def test_vsihdfs_3():
    if gdaltest.have_vsihdfs == False:
        pytest.skip()

    filename = '/vsihdfs/file:' + os.getcwd() + '/data/text.txt'
    fp = gdal.VSIFOpenL(filename, 'rb')
    assert fp is not None

    data = gdal.VSIFReadL(5, 1, fp)
    assert data and data.decode('ascii') == 'Lorem'

    offset = gdal.VSIFTellL(fp)
    assert offset == 5

    gdal.VSIFCloseL(fp)

# Write test
def test_vsihdfs_4():
    pytest.skip()

# EOF test
def test_vsihdfs_5():
    if gdaltest.have_vsihdfs == False:
        pytest.skip()

    filename = '/vsihdfs/file:' + os.getcwd() + '/data/text.txt'
    fp = gdal.VSIFOpenL(filename, 'rb')
    assert fp is not None

    gdal.VSIFReadL(5, 1, fp)
    eof = gdal.VSIFEofL(fp)
    assert eof == 0

    gdal.VSIFReadL(1000000, 1, fp)
    eof = gdal.VSIFEofL(fp)
    assert eof == 0

    gdal.VSIFReadL(1, 1, fp)
    eof = gdal.VSIFEofL(fp)
    assert eof == 1

    gdal.VSIFSeekL(fp, 0, 0)
    eof = gdal.VSIFEofL(fp)
    assert eof == 0

    gdal.VSIFCloseL(fp)

# Stat test
def test_vsihdfs_6():
    if gdaltest.have_vsihdfs == False:
        pytest.skip()

    filename = '/vsihdfs/file:' + os.getcwd() + '/data/text.txt'
    statBuf = gdal.VSIStatL(filename, 0)
    assert statBuf

    filename = '/vsihdfs/file:' + os.getcwd() + '/data/no-such-file.txt'
    statBuf = gdal.VSIStatL(filename, 0)
    assert not statBuf

# ReadDir test
def test_vsihdfs_7():
    if gdaltest.have_vsihdfs == False:
        pytest.skip()

    dirname = '/vsihdfs/file:' + os.getcwd() + '/data/'
    lst = gdal.ReadDir(dirname)
    assert len(lst) >= 360



