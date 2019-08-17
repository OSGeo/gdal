#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  nearblack testing
# Author:   Even Rouault <even dot rouault @ spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault @ spatialys dot com>
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
import pytest

###############################################################################
# Basic test


def test_nearblack_lib_1():

    src_ds = gdal.Open('../gdrivers/data/rgbsmall.tif')
    ds = gdal.Nearblack('', src_ds, format='MEM', maxNonBlack=0, nearDist=15)
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


def test_nearblack_lib_2():

    ds = gdal.Nearblack('', '../gdrivers/data/rgbsmall.tif', format='MEM', maxNonBlack=0, setAlpha=True)
    assert ds is not None

    assert ds.GetRasterBand(4).Checksum() == 22002, 'Bad checksum band 0'

    ds = None

###############################################################################
# Set existing alpha band


def test_nearblack_lib_3():

    src_ds = gdal.Nearblack('', '../gdrivers/data/rgbsmall.tif', format='MEM', maxNonBlack=0, setAlpha=True)
    ds = gdal.Nearblack('', src_ds, format='MEM', maxNonBlack=0, setAlpha=True)
    assert ds is not None

    assert ds.GetRasterBand(4).Checksum() == 22002, 'Bad checksum band 0'

    ds = None

###############################################################################
# Test -white


def test_nearblack_lib_4():

    src_ds = gdal.Warp('', '../gdrivers/data/rgbsmall.tif', format='MEM', warpOptions=["INIT_DEST=255"], srcNodata=0)
    ds = gdal.Nearblack('', src_ds, format='MEM', white=True, maxNonBlack=0, setAlpha=True)
    assert ds is not None

    assert ds.GetRasterBand(4).Checksum() == 24151, 'Bad checksum band 0'

    ds = None

###############################################################################
# Add mask band


def test_nearblack_lib_5():

    ds = gdal.Nearblack('/vsimem/test_nearblack_lib_5.tif', '../gdrivers/data/rgbsmall.tif', format='GTiff', maxNonBlack=0, setMask=True)
    assert ds is not None

    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 22002, \
        'Bad checksum mask band'

    ds = None

    gdal.Unlink('/vsimem/test_nearblack_lib_5.tif')
    gdal.Unlink('/vsimem/test_nearblack_lib_5.tif.msk')

###############################################################################
# Test -color


def test_nearblack_lib_7():

    ds = gdal.Nearblack('', 'data/whiteblackred.tif', format='MEM', colors=((0, 0, 0), (255, 255, 255)))
    assert ds is not None

    assert (ds.GetRasterBand(1).Checksum() == 418 and \
       ds.GetRasterBand(2).Checksum() == 0 and \
       ds.GetRasterBand(3).Checksum() == 0), 'Bad checksum'

    ds = None

###############################################################################
# Test in-place update


def test_nearblack_lib_8():

    src_ds = gdal.Open('../gdrivers/data/rgbsmall.tif')
    ds = gdal.GetDriverByName('MEM').CreateCopy('', src_ds)
    ret = gdal.Nearblack(ds, ds, maxNonBlack=0)
    assert ret == 1

    assert ds.GetRasterBand(1).Checksum() == 21106, 'Bad checksum band 1'

    assert ds.GetRasterBand(2).Checksum() == 20736, 'Bad checksum band 2'

    assert ds.GetRasterBand(3).Checksum() == 21309, 'Bad checksum band 3'




