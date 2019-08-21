#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  nearblack testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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
import test_cli_utilities
import pytest

###############################################################################
# Basic test


def test_nearblack_1():
    if test_cli_utilities.get_nearblack_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_nearblack_path() + ' ../gdrivers/data/rgbsmall.tif -nb 0 -of GTiff -o tmp/nearblack1.tif')
    assert (err is None or err == ''), 'got error/warning'

    src_ds = gdal.Open('../gdrivers/data/rgbsmall.tif')
    ds = gdal.Open('tmp/nearblack1.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 21106, 'Bad checksum band 1'

    assert ds.GetRasterBand(2).Checksum() == 20736, 'Bad checksum band 2'

    assert ds.GetRasterBand(3).Checksum() == 21309, 'Bad checksum band 3'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), 'Bad geotransform'

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, 'Bad projection'

    src_ds = None
    ds = None

###############################################################################
# Add alpha band


def test_nearblack_2():
    if test_cli_utilities.get_nearblack_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_nearblack_path() + ' ../gdrivers/data/rgbsmall.tif -setalpha -nb 0 -of GTiff -o tmp/nearblack2.tif -co TILED=YES')

    ds = gdal.Open('tmp/nearblack2.tif')
    assert ds is not None

    assert ds.GetRasterBand(4).Checksum() == 22002, 'Bad checksum band 0'

    ds = None

###############################################################################
# Set existing alpha band


def test_nearblack_3():
    if test_cli_utilities.get_nearblack_path() is None:
        pytest.skip()

    shutil.copy('tmp/nearblack2.tif', 'tmp/nearblack3.tif')
    gdaltest.runexternal(test_cli_utilities.get_nearblack_path() + ' -setalpha -nb 0 -of GTiff tmp/nearblack3.tif')

    ds = gdal.Open('tmp/nearblack3.tif')
    assert ds is not None

    assert ds.GetRasterBand(4).Checksum() == 22002, 'Bad checksum band 0'

    ds = None

###############################################################################
# Test -white


def test_nearblack_4():
    if test_cli_utilities.get_nearblack_path() is None:
        pytest.skip()
    if test_cli_utilities.get_gdalwarp_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' -wo "INIT_DEST=255" ../gdrivers/data/rgbsmall.tif  tmp/nearblack4_src.tif -srcnodata 0')
    gdaltest.runexternal(test_cli_utilities.get_nearblack_path() + ' -q -setalpha -white -nb 0 -of GTiff tmp/nearblack4_src.tif -o tmp/nearblack4.tif')

    ds = gdal.Open('tmp/nearblack4.tif')
    assert ds is not None

    assert ds.GetRasterBand(4).Checksum() == 24151, 'Bad checksum band 0'

    ds = None

###############################################################################
# Add mask band


def test_nearblack_5():
    if test_cli_utilities.get_nearblack_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_nearblack_path() + ' ../gdrivers/data/rgbsmall.tif --config GDAL_TIFF_INTERNAL_MASK NO -setmask -nb 0 -of GTiff -o tmp/nearblack5.tif -co TILED=YES')

    ds = gdal.Open('tmp/nearblack5.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 22002, \
        'Bad checksum mask band'

    ds = None

###############################################################################
# Set existing mask band


def test_nearblack_6():
    if test_cli_utilities.get_nearblack_path() is None:
        pytest.skip()

    shutil.copy('tmp/nearblack5.tif', 'tmp/nearblack6.tif')
    shutil.copy('tmp/nearblack5.tif.msk', 'tmp/nearblack6.tif.msk')

    gdaltest.runexternal(test_cli_utilities.get_nearblack_path() + ' -setmask -nb 0 -of GTiff tmp/nearblack6.tif')

    ds = gdal.Open('tmp/nearblack6.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 22002, \
        'Bad checksum mask band'

    ds = None

###############################################################################
# Test -color


def test_nearblack_7():
    if test_cli_utilities.get_nearblack_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_nearblack_path() + ' data/whiteblackred.tif -o tmp/nearblack7.tif -color 0,0,0 -color 255,255,255 -of GTiff')

    ds = gdal.Open('tmp/nearblack7.tif')
    assert ds is not None

    assert (ds.GetRasterBand(1).Checksum() == 418 and \
       ds.GetRasterBand(2).Checksum() == 0 and \
       ds.GetRasterBand(3).Checksum() == 0), 'Bad checksum'

    ds = None

###############################################################################
# Test in-place update


def test_nearblack_8():
    if test_cli_utilities.get_nearblack_path() is None:
        pytest.skip()

    src_ds = gdal.Open('../gdrivers/data/rgbsmall.tif')
    gdal.GetDriverByName('GTiff').CreateCopy('tmp/nearblack8.tif', src_ds)
    src_ds = None

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_nearblack_path() + ' tmp/nearblack8.tif -nb 0')
    assert (err is None or err == ''), 'got error/warning'

    ds = gdal.Open('tmp/nearblack8.tif')
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 21106, 'Bad checksum band 1'

    assert ds.GetRasterBand(2).Checksum() == 20736, 'Bad checksum band 2'

    assert ds.GetRasterBand(3).Checksum() == 21309, 'Bad checksum band 3'

###############################################################################
# Cleanup


def test_nearblack_cleanup():
    try:
        os.remove('tmp/nearblack1.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/nearblack2.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/nearblack3.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/nearblack4_src.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/nearblack4.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/nearblack5.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/nearblack5.tif.msk')
    except OSError:
        pass
    try:
        os.remove('tmp/nearblack6.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/nearblack6.tif.msk')
    except OSError:
        pass
    try:
        os.remove('tmp/nearblack7.tif')
    except OSError:
        pass
    try:
        os.remove('tmp/nearblack8.tif')
    except OSError:
        pass
    



