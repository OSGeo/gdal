#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for FIT driver.
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
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


@pytest.mark.parametrize('filename', [
    'byte', 'int16', 'uint16', 'int32', 'uint32', 'float32', 'float64'
])
def test_fit(filename):
    fitDriver = gdal.GetDriverByName('FIT')

    ds = gdal.Open('../gcore/data/' + filename + '.tif')
    fitDriver.CreateCopy('tmp/' + filename + '.fit', ds, options=['PAGESIZE=2,2'])

    ds2 = gdal.Open('tmp/' + filename + '.fit')
    assert ds2.GetRasterBand(1).Checksum() == ds.GetRasterBand(1).Checksum()

    assert ds2.GetRasterBand(1).DataType == ds.GetRasterBand(1).DataType
    ds2 = None
