#!/usr/bin/env pytest
###############################################################################
# Purpose:  Tests for Racurs PHOTOMOD tiled format reader (http://www.racurs.ru)
# Author:   Andrew Sudorgin (drons [a] list dot ru)
###############################################################################
# Copyright (c) 2016, Andrew Sudorgin
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



import gdaltest
from osgeo import gdal
import pytest

###############################################################################


def test_prf_1():

    tst = gdaltest.GDALTest('prf', './PRF/ph.prf', 1, 43190)
    return tst.testOpen(check_gt=(1, 2, 3, -7, 5, 6))


def test_prf_2():

    ds = gdal.Open('./data/PRF/dem.x-dem')

    assert ds.RasterXSize == 4330, 'Invalid dataset width'

    assert ds.RasterYSize == 4663, 'Invalid dataset height'

    unittype = ds.GetRasterBand(1).GetUnitType()
    assert unittype == 'm', 'Failed to read elevation units from x-dem'

    datatype = ds.GetRasterBand(1).DataType
    assert datatype == gdal.GDT_Float32, 'Failed to read datatype'

    expectedOvCount = 1
    if ds.GetRasterBand(1).GetOverviewCount() != expectedOvCount:
        print('Overview count must be %d' % expectedOvCount)
        print('But GetOverviewCount returned %d' % ds.GetRasterBand(1).GetOverviewCount())
        pytest.fail('did not get expected number of overviews')

    overview = ds.GetRasterBand(1).GetOverview(0)
    assert overview.XSize == 1082, ('Invalid dataset width %d' % overview.XSize)
    assert overview.YSize == 1165, ('Invalid dataset height %d' % overview.YSize)

    ds = None


def test_prf_3():

    ds = gdal.Open('./data/PRF/ph.prf')

    expectedOvCount = 0
    if ds.GetRasterBand(1).GetOverviewCount() != expectedOvCount:
        print('Overview count must be %d' % expectedOvCount)
        print('But GetOverviewCount returned %d' % ds.GetRasterBand(1).GetOverviewCount())
        pytest.fail('did not get expected number of overviews')

    ds = None


def test_prf_4():

    tst = gdaltest.GDALTest('prf', './PRF/dem.x-dem', 1, 0)
    return tst.testOpen(check_gt=(1.5, 1.0, 0.0, 9329.0, 0.0, -2.0))


###############################################################################



