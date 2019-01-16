#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DODS raster access.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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


import gdaltest
import pytest


###############################################################################
# Open DODS datasource.

@pytest.mark.skip()
def test_dods_1():
    gdaltest.dods_ds = None
    gdaltest.dods_dr = None

    gdaltest.dods_dr = gdal.GetDriverByName('DODS')
    if gdaltest.dods_dr is None:
        pytest.skip()

    gdaltest.dods_grid_ds = gdal.Open('http://disc1.sci.gsfc.nasa.gov/opendap/tovs/TOVSAMNF/1985/032/TOVS_MONTHLY_PM_8502_NF.HDF.Z?Data-Set-11[y][x]')

    if gdaltest.dods_grid_ds is None:
        gdaltest.dods_dr = None
        pytest.fail()


###############################################################################
# Simple read test on a single variable.


@pytest.mark.skip()
def test_dods_2():
    if gdaltest.dods_dr is None:
        pytest.skip()
    tst = gdaltest.GDALTest('dods', 'http://disc1.sci.gsfc.nasa.gov/opendap/tovs/TOVSAMNF/1985/032/TOVS_MONTHLY_PM_8502_NF.HDF.Z?Data-Set-11', 1, 3391, filename_absolute=1)
    return tst.testOpen()

###############################################################################
# Access all grids at once.


@pytest.mark.skip()
def test_dods_3():
    if gdaltest.dods_dr is None:
        pytest.skip()
    tst = gdaltest.GDALTest('dods', 'http://disc1.sci.gsfc.nasa.gov/opendap/tovs/TOVSAMNF/1985/032/TOVS_MONTHLY_PM_8502_NF.HDF.Z', 12, 43208, filename_absolute=1)
    return tst.testOpen()

###############################################################################
# Test explicit transpose.


@pytest.mark.skip()
def test_dods_4():
    if gdaltest.dods_dr is None:
        pytest.skip()
    tst = gdaltest.GDALTest('dods', 'http://disc1.sci.gsfc.nasa.gov/opendap/tovs/TOVSAMNF/1985/032/TOVS_MONTHLY_PM_8502_NF.HDF.Z?Data-Set-11[y][x]', 1, 3391, filename_absolute=1)
    return tst.testOpen()

###############################################################################
# Test explicit flipping.


@pytest.mark.skip()
def test_dods_5():
    if gdaltest.dods_dr is None:
        pytest.skip()

    tst = gdaltest.GDALTest('dods', 'http://disc1.sci.gsfc.nasa.gov/opendap/tovs/TOVSAMNF/1985/032/TOVS_MONTHLY_PM_8502_NF.HDF.Z?Data-Set-11[y][-x]', 1, 2436, filename_absolute=1)
    return tst.testOpen()

###############################################################################
# Check nodata value.


@pytest.mark.skip()
def test_dods_6():
    if gdaltest.dods_dr is None:
        pytest.skip()

    # This server seems to no longer be online, skipping test.

    pytest.skip()

    # pylint: disable=unreachable
    gdaltest.dods_grid_ds = gdal.Open('http://g0dup05u.ecs.nasa.gov/opendap/AIRS/AIRX3STD.003/2004.12.28/AIRS.2004.12.28.L3.RetStd001.v4.0.9.0.G05253115303.hdf?TotH2OVap_A[y][x]')
    nd = gdaltest.dods_grid_ds.GetRasterBand(1).GetNoDataValue()
    assert nd == -9999.0, 'nodata value wrong or missing.'

###############################################################################
# Cleanup


@pytest.mark.skip()
def test_dods_cleanup():
    if gdaltest.dods_dr is None:
        pytest.skip()

    gdaltest.dods_dr = None
    gdaltest.dods_grid_ds = None
