#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GDALVirtualMem interface
# Author:   Even Rouault <even dot rouault at spatialys.com>
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

import sys

from osgeo import gdal
import pytest

# All tests will be skipped if numpy unavailable or SKIP_VIRTUALMEM is set.
numpy = pytest.importorskip('numpy')
pytestmark = pytest.mark.skipif(gdal.GetConfigOption('SKIP_VIRTUALMEM'),
                                reason='SKIP_VIRTUALMEM is set in config')


###############################################################################
# Test linear and tiled virtual mem interfaces in read-only mode
def test_virtualmem_1():

    ds = gdal.Open('../gdrivers/data/small_world.tif')
    bufxsize = 400
    bufysize = 128
    tilexsize = 128
    tileysize = 64

    ar = ds.ReadAsArray(0, 0, bufxsize, bufysize)

    try:
        ar_flat_bsq = ds.GetVirtualMemArray(gdal.GF_Read, 0, 0, bufxsize, bufysize, bufxsize, bufysize, gdal.GDT_Int16, [1, 2, 3], 1, 1024 * 1024, 0)
    except:
        if not sys.platform.startswith('linux'):
            # Also try GetTiledVirtualMemArray() robustness (#5728)
            try:
                ar_tiled_band1 = ds.GetRasterBand(1).GetTiledVirtualMemArray(gdal.GF_Read, 0, 0, bufxsize, bufysize, tilexsize, tileysize, gdal.GDT_Int16, 1024 * 1024)
            except:
                pass
            pytest.skip()

    ar_flat_band1 = ds.GetRasterBand(1).GetVirtualMemArray(gdal.GF_Read, 0, 0, bufxsize, bufysize, bufxsize, bufysize, gdal.GDT_Int16, 1024 * 1024, 0)
    ar_flat_bip = ds.GetVirtualMemArray(gdal.GF_Read, 0, 0, bufxsize, bufysize, bufxsize, bufysize, gdal.GDT_Int16, [1, 2, 3], 0, 1024 * 1024, 0)
    ar_tiled_band1 = ds.GetRasterBand(1).GetTiledVirtualMemArray(gdal.GF_Read, 0, 0, bufxsize, bufysize, tilexsize, tileysize, gdal.GDT_Int16, 1024 * 1024)
    ar_tip = ds.GetTiledVirtualMemArray(gdal.GF_Read, 0, 0, bufxsize, bufysize, tilexsize, tileysize, gdal.GDT_Int16, [1, 2, 3], gdal.GTO_TIP, 1024 * 1024)
    ar_bit = ds.GetTiledVirtualMemArray(gdal.GF_Read, 0, 0, bufxsize, bufysize, tilexsize, tileysize, gdal.GDT_Int16, [1, 2, 3], gdal.GTO_BIT, 1024 * 1024)
    ar_bsq = ds.GetTiledVirtualMemArray(gdal.GF_Read, 0, 0, bufxsize, bufysize, tilexsize, tileysize, gdal.GDT_Int16, [1, 2, 3], gdal.GTO_BSQ, 1024 * 1024)
    tilepercol = int((bufysize + tileysize - 1) / tileysize)
    tileperrow = int((bufxsize + tilexsize - 1) / tilexsize)

    for tiley in range(tilepercol):
        reqysize = tileysize
        if reqysize + tiley * tileysize > bufysize:
            reqysize = bufysize - tiley * tileysize
        for tilex in range(tileperrow):
            reqxsize = tilexsize
            if reqxsize + tilex * tilexsize > bufxsize:
                reqxsize = bufxsize - tilex * tilexsize
            for y in range(reqysize):
                for x in range(reqxsize):
                    for band in range(3):
                        assert ar_tip[tiley][tilex][y][x][band] == ar[band][tiley * tileysize + y][tilex * tilexsize + x]
                        assert ar_tip[tiley][tilex][y][x][band] == ar_flat_bsq[band][tiley * tileysize + y][tilex * tilexsize + x]
                        assert ar_tip[tiley][tilex][y][x][band] == ar_flat_bip[tiley * tileysize + y][tilex * tilexsize + x][band]
                        assert ar_tip[tiley][tilex][y][x][band] == ar_bsq[band][tiley][tilex][y][x]
                        assert ar_tip[tiley][tilex][y][x][band] == ar_bit[tiley][tilex][band][y][x]
                        if band == 0:
                            assert ar_flat_band1[tiley * tileysize + y][tilex * tilexsize + x] == ar_flat_bip[tiley * tileysize + y][tilex * tilexsize + x][0]
                            assert ar_tiled_band1[tiley][tilex][y][x] == ar_flat_bip[tiley * tileysize + y][tilex * tilexsize + x][0]

    # We need to destroy the array before dataset destruction
    ar_flat_band1 = None
    ar_flat_bip = None
    ar_tiled_band1 = None
    ar_tip = None
    ar_bit = None
    ar_bsq = None
    ds = None


###############################################################################
# Test write mode
@pytest.mark.skipif(sys.platform != 'linux', reason='Incorrect platform')
def test_virtualmem_2():
    ds = gdal.GetDriverByName('MEM').Create('', 100, 100, 1)
    ar = ds.GetVirtualMemArray(gdal.GF_Write)
    ar.fill(255)
    ar = None
    # We need to have released the Virtual Memory Array with ar=None to be sure that
    # every modified page gets flushed back to the dataset
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == 57182


###############################################################################
# Test virtual mem auto with a raw driver
@pytest.mark.skipif(sys.platform != 'linux', reason='Incorrect platform')
def test_virtualmem_3():
    for tmpfile in ['tmp/virtualmem_3.img', '/vsimem/virtualmem_3.img']:
        ds = gdal.GetDriverByName('EHdr').Create(tmpfile, 400, 300, 2)
        ar1 = ds.GetRasterBand(1).GetVirtualMemAutoArray(gdal.GF_Write)
        ar2 = ds.GetRasterBand(2).GetVirtualMemAutoArray(gdal.GF_Write)
        for y in range(ds.RasterYSize):
            ar1[y].fill(127)
            ar2[y].fill(255)
        # We need to destroy the array before dataset destruction
        ar1 = None
        ar2 = None
        ds = None

        ds = gdal.Open(tmpfile)
        ar1 = ds.GetRasterBand(1).GetVirtualMemAutoArray(gdal.GF_Read)
        ar2 = ds.GetRasterBand(2).GetVirtualMemAutoArray(gdal.GF_Read)
        ar_127 = numpy.empty(ds.RasterXSize)
        ar_127.fill(127)
        ar_255 = numpy.empty(ds.RasterXSize)
        ar_255.fill(255)
        for y in range(ds.RasterYSize):
            assert numpy.array_equal(ar1[y], ar_127)
            assert numpy.array_equal(ar2[y], ar_255)
        # We need to destroy the array before dataset destruction
        ar1 = None
        ar2 = None
        ds = None

        gdal.GetDriverByName('EHdr').Delete(tmpfile)


###############################################################################
# Test virtual mem auto with GTiff
@pytest.mark.skipif(sys.platform != 'linux', reason='Incorrect platform')
def test_virtualmem_4():
    tmpfile = 'tmp/virtualmem_4.tif'
    for option in ['INTERLEAVE=PIXEL', 'INTERLEAVE=BAND']:
        gdal.Unlink(tmpfile)
        ds = gdal.GetDriverByName('GTiff').Create(tmpfile, 400, 301, 2, options=[option])
        ar1 = ds.GetRasterBand(1).GetVirtualMemAutoArray(gdal.GF_Write)
        if gdal.GetLastErrorMsg().find('mmap() failed') >= 0:
            ar1 = None
            ds = None
            pytest.skip()
        ar1 = None
        ar1 = ds.GetRasterBand(1).GetVirtualMemAutoArray(gdal.GF_Write)
        ar1_bis = ds.GetRasterBand(1).GetVirtualMemAutoArray(gdal.GF_Write)
        ar2 = ds.GetRasterBand(2).GetVirtualMemAutoArray(gdal.GF_Write)
        for y in range(ds.RasterYSize):
            ar1[y].fill(127)
            ar2[y].fill(255)

        val = ar1_bis[0][0]
        # We need to destroy the array before dataset destruction
        ar1 = None
        ar1_bis = None
        ar2 = None
        ds = None
        assert val == 127

        ds = gdal.Open(tmpfile)
        ar1 = ds.GetRasterBand(1).GetVirtualMemAutoArray(gdal.GF_Read)
        ar2 = ds.GetRasterBand(2).GetVirtualMemAutoArray(gdal.GF_Read)
        ar_127 = numpy.empty(ds.RasterXSize)
        ar_127.fill(127)
        ar_255 = numpy.empty(ds.RasterXSize)
        ar_255.fill(255)
        for y in range(ds.RasterYSize):
            if not numpy.array_equal(ar1[y], ar_127):
                ar1 = None
                ar2 = None
                ds = None
                pytest.fail()
            if not numpy.array_equal(ar2[y], ar_255):
                ar1 = None
                ar2 = None
                ds = None
                pytest.fail()
        # We need to destroy the array before dataset destruction
        ar1 = None
        ar2 = None
        ds = None

        gdal.GetDriverByName('GTiff').Delete(tmpfile)
