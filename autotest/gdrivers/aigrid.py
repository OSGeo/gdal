#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for AIGRID driver.
# Author:   Swapnil Hajare <dreamil@gmail.com>
#
###############################################################################
# Copyright (c) 2006, Swapnil Hajare <dreamil@gmail.com>
# Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
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
import shutil
from osgeo import gdal


import gdaltest
import pytest

###############################################################################
# Read test of simple byte reference data.


def test_aigrid_1():

    tst = gdaltest.GDALTest('AIG', 'aigrid/abc3x1', 1, 3)
    return tst.testOpen()

###############################################################################
# Verify some auxiliary data.


def test_aigrid_2():

    ds = gdal.Open('data/aigrid/abc3x1/prj.adf')

    gt = ds.GetGeoTransform()

    assert gt[0] == -0.5 and gt[1] == 1.0 and gt[2] == 0.0 and gt[3] == 0.5 and gt[4] == 0.0 and gt[5] == -1.0, \
        'Aigrid geotransform wrong.'

    prj = ds.GetProjection()
    assert prj.find('PROJCS["unnamed",GEOGCS["GDA94",DATUM["Geocentric_Datum_of_Australia_1994"') != -1, \
        ('Projection does not match expected:\n%s' % prj)

    band1 = ds.GetRasterBand(1)
    assert band1.GetNoDataValue() == 255, 'Grid NODATA value wrong or missing.'

    assert band1.DataType == gdal.GDT_Byte, 'Data type is not Byte!'

###############################################################################
# Verify the colormap, and nodata setting for test file.


def test_aigrid_3():

    ds = gdal.Open('data/aigrid/abc3x1')
    cm = ds.GetRasterBand(1).GetRasterColorTable()
    assert cm.GetCount() == 256 and cm.GetColorEntry(0) == (95, 113, 150, 255) and cm.GetColorEntry(1) == (95, 57, 29, 255), \
        'Wrong colormap entries'

    cm = None

    assert ds.GetRasterBand(1).GetNoDataValue() == 255.0, 'Wrong nodata value.'
###############################################################################
# Read test of simple byte reference data with data directory name in all uppercase


def test_aigrid_4():

    tst = gdaltest.GDALTest('AIG', 'aigrid/ABC3X1UC', 1, 3)
    return tst.testOpen()

###############################################################################
# Verify the colormap, and nodata setting for test file with names of coverage directory and all files in it in all uppercase. Additionally also test for case where clr file resides in parent directory of coverage.


def test_aigrid_5():

    ds = gdal.Open('data/aigrid/ABC3X1UC')
    cm = ds.GetRasterBand(1).GetRasterColorTable()
    assert cm.GetCount() == 256 and cm.GetColorEntry(0) == (95, 113, 150, 255) and cm.GetColorEntry(1) == (95, 57, 29, 255), \
        'Wrong colormap entries'

    cm = None

    assert ds.GetRasterBand(1).GetNoDataValue() == 255.0, 'Wrong nodata value.'

###############################################################################
# Verify dataset whose sta.adf is 24 bytes


def test_aigrid_6():

    ds = gdal.Open('data/aigrid/aigrid_sta_24bytes/teststa')

    assert ds.GetRasterBand(1).GetMinimum() == 0.0, 'Wrong minimum'

    assert ds.GetRasterBand(1).GetMaximum() == 2.0, 'Wrong maximum'

###############################################################################
# Read twice a broken tile (https://github.com/OSGeo/gdal/issues/4316)


def test_aigrid_broken():

    if os.path.exists('tmp/broken_aigrid'):
        shutil.rmtree('tmp/broken_aigrid')

    shutil.copytree('data/aigrid/abc3x1', 'tmp/broken_aigrid')

    # Write a bad offset for a block
    f = gdal.VSIFOpenL('tmp/broken_aigrid/w001001x.adf', 'rb+')
    gdal.VSIFSeekL(f, 100, 0)
    gdal.VSIFWriteL(b'\xff' * 4, 1, 4, f)
    gdal.VSIFCloseL(f)

    ds = gdal.Open('tmp/broken_aigrid')
    with gdaltest.error_handler():
        assert ds.GetRasterBand(1).Checksum() == 0
    with gdaltest.error_handler():
        assert ds.GetRasterBand(1).Checksum() == 0
    ds = None

    shutil.rmtree('tmp/broken_aigrid')

###############################################################################
# Test on real dataset downloaded from http://download.osgeo.org/gdal/data/aig/nzdem


def test_aigrid_online_1():

    list_files = ['info/arc.dir',
                  'info/arc0000.dat',
                  'info/arc0000.nit',
                  'info/arc0001.dat',
                  'info/arc0001.nit',
                  'info/arc0002.dat',
                  'info/arc0002.nit',
                  'info/arc0002r.001',
                  'nzdem500/dblbnd.adf',
                  'nzdem500/hdr.adf',
                  'nzdem500/log',
                  'nzdem500/sta.adf',
                  'nzdem500/vat.adf',
                  'nzdem500/w001001.adf',
                  'nzdem500/w001001x.adf']

    try:
        os.mkdir('tmp/cache/nzdem')
        os.mkdir('tmp/cache/nzdem/info')
        os.mkdir('tmp/cache/nzdem/nzdem500')
    except OSError:
        pass

    for filename in list_files:
        if not gdaltest.download_file('http://download.osgeo.org/gdal/data/aig/nzdem/' + filename, 'nzdem/' + filename):
            pytest.skip()

    tst = gdaltest.GDALTest('AIG', 'tmp/cache/nzdem/nzdem500/hdr.adf', 1, 45334, filename_absolute=1)
    tst.testOpen()

    ds = gdal.Open('tmp/cache/nzdem/nzdem500/hdr.adf')

    try:
        rat = ds.GetRasterBand(1).GetDefaultRAT()
    except:
        print('Skipping RAT checking... OG Python bindings have no RAT API')
        return

    assert rat is not None, 'No RAT found'

    assert rat.GetRowCount() == 2642, 'Wrong row count in RAT'

    assert rat.GetColumnCount() == 2, 'Wrong column count in RAT'

    assert rat.GetNameOfCol(0) == 'VALUE', 'Wrong name of col 0'

    assert rat.GetTypeOfCol(0) == gdal.GFT_Integer, 'Wrong type of col 0'

    assert rat.GetUsageOfCol(0) == gdal.GFU_MinMax, 'Wrong usage of col 0'

    assert rat.GetNameOfCol(1) == 'COUNT', 'Wrong name of col 1'

    assert rat.GetTypeOfCol(1) == gdal.GFT_Integer, 'Wrong type of col 1'

    assert rat.GetUsageOfCol(1) == gdal.GFU_PixelCount, 'Wrong usage of col 1'

    assert rat.GetValueAsInt(2641, 0) == 3627, 'Wrong value in RAT'

    assert ds.GetRasterBand(1).GetMinimum() == 0.0, 'Wrong minimum'

    assert ds.GetRasterBand(1).GetMaximum() == 3627.0, 'Wrong maximum'

###############################################################################
# Test on real dataset downloaded from http://download.osgeo.org/gdal/data/aig/nzdem


def test_aigrid_online_2():

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/aig/ai_bug_6886.zip', 'ai_bug_6886.zip'):
        pytest.skip()

    try:
        os.stat('tmp/cache/ai_bug')
    except OSError:
        try:
            gdaltest.unzip('tmp/cache', 'tmp/cache/ai_bug_6886')
            try:
                os.stat('tmp/cache/ai_bug')
            except OSError:
                pytest.skip()
        except:
            pytest.skip()

    tst = gdaltest.GDALTest('AIG', 'tmp/cache/ai_bug/ai_bug/hdr.adf', 1, 16018, filename_absolute=1)
    return tst.testOpen()

###############################################################################



