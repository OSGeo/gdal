#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DDS driver
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2019 Even Rouault <even dot rouault at spatialys.com>
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

pytestmark = pytest.mark.require_driver('DDS')

test_list = [
    ('DXT1', [11376, 57826, 34652, 32919]),
    ('DXT3', [12272, 59240, 34811, 7774]),
    ('DXT5', [12272, 59240, 34811, 10402]),
    ('ETC1', [9560, 57939, 30566]),
]

@pytest.mark.parametrize(
    'compression,checksums',
    test_list,
    ids=[row[0] for row in test_list]
)
def test_dds(compression,checksums):
    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    ds = gdal.GetDriverByName('DDS').CreateCopy('/vsimem/out.dds', src_ds,
                                                options=['FORMAT=' + compression])
    assert ds
    assert ds.RasterCount == len(checksums)
    assert ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE') == compression
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert [ds.GetRasterBand(i+1).Checksum() for i in range(ds.RasterCount)] == checksums


def test_dds_no_compression():
    ref_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    ds = gdal.Open('data/dds/stefan_full_rgba_no_compression.dds')
    assert ds
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    ref_checksum = [ref_ds.GetRasterBand(i+1).Checksum() for i in range(4)]
    assert [ds.GetRasterBand(i+1).Checksum() for i in range(4)] == ref_checksum
