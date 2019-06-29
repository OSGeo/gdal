#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test TIL driver
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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


import gdaltest
from osgeo import gdal

###############################################################################
# Test a fake TIL dataset


def test_til_1():

    tst = gdaltest.GDALTest('TIL', 'testtil.til', 1, 4672)
    return tst.testOpen()

###############################################################################
# Check GetFileList() result (#4018) & IMD


def test_til_2():

    try:
        os.remove('data/testtil.til.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/testtil.til')
    filelist = ds.GetFileList()

    assert len(filelist) == 3, 'did not get expected file list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    ds = None

    assert not os.path.exists('data/testtil.til.aux.xml')
    
###############################################################################
# Check GetFileList() & XML


def test_til_3():

    try:
        os.remove('data/testtil.til.aux.xml')
    except OSError:
        pass

    ds = gdal.Open('data/testtil2.til')
    filelist = ds.GetFileList()

    assert len(filelist) == 3, 'did not get expected file list.'

    md = ds.GetMetadata('IMAGERY')
    assert 'SATELLITEID' in md, 'SATELLITEID not present in IMAGERY Domain'
    assert 'CLOUDCOVER' in md, 'CLOUDCOVER not present in IMAGERY Domain'
    assert 'ACQUISITIONDATETIME' in md, \
        'ACQUISITIONDATETIME not present in IMAGERY Domain'

    ds = None

    assert not os.path.exists('data/testtil.til.aux.xml')


