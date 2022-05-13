#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test TGA driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
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

pytestmark = pytest.mark.require_driver('TGA')


def test_tga_read_rle_grey_level():
    ds = gdal.Open('data/tga/ref_test_suite/cbw8.tga')
    assert ds.RasterCount == 1
    assert ds.RasterXSize == 128
    assert ds.RasterYSize == 128
    assert ds.GetMetadataItem('AUTHOR_NAME') == 'Ricky True'
    assert ds.GetMetadataItem('COMMENTS') == 'Sample 8 bit run length compressed black and white image'
    assert ds.GetMetadataItem('IMAGE_ID') == 'Truevision(R) Sample Image'
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(1).GetColorTable() is None
    assert ds.GetRasterBand(1).Checksum() == 43089


def test_tga_read_rle_color_table():
    ds = gdal.Open('data/tga/ref_test_suite/ccm8.tga')
    assert ds.RasterCount == 1
    assert ds.RasterXSize == 128
    assert ds.RasterYSize == 128
    assert ds.GetMetadataItem('AUTHOR_NAME') == 'Ricky True'
    assert ds.GetMetadataItem('COMMENTS') == 'Sample 8 bit run length compressed color mapped image'
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct is not None
    assert ct.GetCount() == 256
    assert ct.GetColorEntry(0) == (0, 0, 0, 255)
    assert ct.GetColorEntry(1) == (8, 8, 8, 255)
    assert ct.GetColorEntry(64) == (248, 0, 0, 255)
    assert ds.GetRasterBand(1).Checksum() == 38151


def test_tga_read_rle_24bit():
    ds = gdal.Open('data/tga/ref_test_suite/ctc24.tga')
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 128
    assert ds.RasterYSize == 128
    assert ds.GetRasterBand(1).GetColorTable() is None
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(1).Checksum() == 9797
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(2).Checksum() == 9952
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(3).Checksum() == 9848


def test_tga_read_uncompressed_grey_level():
    ds = gdal.Open('data/tga/ref_test_suite/ubw8.tga')
    assert ds.RasterCount == 1
    assert ds.RasterXSize == 128
    assert ds.RasterYSize == 128
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(1).GetColorTable() is None
    assert ds.GetRasterBand(1).Checksum() == 43089


def test_tga_read_uncompressed_color_table():
    ds = gdal.Open('data/tga/ref_test_suite/ucm8.tga')
    assert ds.RasterCount == 1
    assert ds.RasterXSize == 128
    assert ds.RasterYSize == 128
    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct is not None
    assert ct.GetCount() == 256
    assert ct.GetColorEntry(0) == (0, 0, 0, 255)
    assert ct.GetColorEntry(1) == (8, 8, 8, 255)
    assert ct.GetColorEntry(64) == (248, 0, 0, 255)
    assert ds.GetRasterBand(1).Checksum() == 38151


def test_tga_read_uncompressed_16bit():
    ds = gdal.Open('data/tga/ref_test_suite/utc16.tga')
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 128
    assert ds.RasterYSize == 128
    assert ds.GetRasterBand(1).GetColorTable() is None
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(1).Checksum() == 64747
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(2).Checksum() == 64839
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(3).Checksum() == 64796


def test_tga_read_uncompressed_24bit():
    ds = gdal.Open('data/tga/ref_test_suite/utc24.tga')
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 128
    assert ds.RasterYSize == 128
    assert ds.GetRasterBand(1).GetColorTable() is None
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(1).Checksum() == 9797
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(2).Checksum() == 9952
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(3).Checksum() == 9848


def test_tga_read_uncompressed_32bit():
    ds = gdal.Open('data/tga/ref_test_suite/utc32.tga')
    assert ds.RasterCount == 4
    assert ds.RasterXSize == 128
    assert ds.RasterYSize == 128
    assert ds.GetRasterBand(1).GetColorTable() is None
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(1).Checksum() == 9797
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(2).Checksum() == 9952
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(3).Checksum() == 9848
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_Undefined
    assert ds.GetRasterBand(4).Checksum() == 0


def test_tga_read_uncompressed_32bit_alpha():
    ds = gdal.Open('data/tga/stefan_full_rgba.tga')
    assert ds.RasterCount == 4
    assert ds.RasterXSize == 162
    assert ds.RasterYSize == 150
    assert ds.GetRasterBand(1).GetColorTable() is None
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(1).Checksum() == 12603
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(2).Checksum() == 58561
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(3).Checksum() == 36064
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(4).Checksum() == 10807


def test_tga_read_single_band_runs_crossing_scanlines():
    ds = gdal.Open('data/tga/from_ffmpeg_samples/test1g.tga')
    assert ds.GetRasterBand(1).Checksum() == 13077


def test_tga_read_three_bands_runs_crossing_scanlines():
    ds = gdal.Open('data/tga/from_ffmpeg_samples/TEST24rle.tga')
    assert ds.GetRasterBand(1).Checksum() == 39607
    assert ds.GetRasterBand(2).Checksum() == 6458
    assert ds.GetRasterBand(3).Checksum() == 44534

