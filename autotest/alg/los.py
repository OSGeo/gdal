#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test gdal.LineOfSightVisible algorithm.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
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


def test_los_basic():

    mem_ds = gdal.GetDriverByName("MEM").Create("", 2, 1)

    success, x, y = gdal.IsLineOfSightVisible(mem_ds.GetRasterBand(1), 0, 0, 1, 1, 0, 1)
    assert success
    assert x == -1
    assert y == -1

    assert gdal.IsLineOfSightVisible(mem_ds.GetRasterBand(1), 0, 0, 1, 0, 0, 1)[0]
    assert not gdal.IsLineOfSightVisible(mem_ds.GetRasterBand(1), 0, 0, -1, 1, 0, 1)[0]
    assert not gdal.IsLineOfSightVisible(mem_ds.GetRasterBand(1), 0, 0, 1, 1, 0, -1)[0]

    with pytest.raises(Exception, match="Received a NULL pointer"):
        assert gdal.IsLineOfSightVisible(None, 0, 0, 0, 0, 0, 0)

    with pytest.raises(Exception, match="Access window out of range in RasterIO()"):
        assert gdal.IsLineOfSightVisible(mem_ds.GetRasterBand(1), 0, 0, 1, 2, 0, 1)
