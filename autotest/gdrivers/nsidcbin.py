#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test NSIDCbin format driver.
# Author:   Michael D. Sumner, <mdsumner at gmail dot com>
#
###############################################################################
# Copyright (c) 2022, Michael Sumner, <mdsumner at gmail dot com>
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

pytestmark = pytest.mark.require_driver("NSIDCbin")

###############################################################################
# Perform simple read test.

## test file
## mkdir autotest/gdrivers/data/nsidcbin
## cd autotest/gdrivers/data/nsidcbin
## wget ftp://sidads.colorado.edu/pub/DATASETS/nsidc0081_nrt_nasateam_seaice/south/nt_20220409_f18_nrt_s.bin
## ## /vsicurl/ftp://sidads.colorado.edu/pub/DATASETS/nsidc0081_nrt_nasateam_seaice/south/nt_20220409_f18_nrt_s.bin
def test_nsidcbin_1(filename="data/nsidcbin/nt_20220409_f18_nrt_s.bin"):
    ds = gdal.Open(filename)
    band = ds.GetRasterBand(1)
    assert band.XSize == 316
    assert band.DataType == gdal.GDT_Byte
    assert int.from_bytes(band.ReadRaster(60, 44, 1, 1), "little") == 27
