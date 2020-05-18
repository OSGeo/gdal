#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test E00GRID driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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

###############################################################################
# Test a fake E00GRID dataset


def test_e00grid_1():

    tst = gdaltest.GDALTest('E00GRID', 'e00grid/fake_e00grid.e00', 1, 65359)
    expected_gt = [500000.0, 1000.0, 0.0, 4000000.0, 0.0, -1000.0]
    expected_srs = """PROJCS["UTM Zone 15, Northern Hemisphere",
    GEOGCS["NAD83",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6269"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9108"]],
        AUTHORITY["EPSG","4269"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-93],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["METERS",1]]"""
    ret = tst.testOpen(check_gt=expected_gt, check_prj=expected_srs)

    if ret == 'success':
        ds = gdal.Open('data/e00grid/fake_e00grid.e00')
        assert ds.GetRasterBand(1).GetNoDataValue() == -32767, \
            'did not get expected nodata value'
        assert ds.GetRasterBand(1).GetUnitType() == 'ft', \
            'did not get expected nodata value'

    return ret

###############################################################################
# Test a fake E00GRID dataset, compressed and with statistics


def test_e00grid_2():

    tst = gdaltest.GDALTest('E00GRID', 'e00grid/fake_e00grid_compressed.e00', 1, 65347)
    expected_gt = [500000.0, 1000.0, 0.0, 4000000.0, 0.0, -1000.0]
    expected_srs = """PROJCS["UTM Zone 15, Northern Hemisphere",
    GEOGCS["NAD83",
        DATUM["North_American_Datum_1983",
            SPHEROID["GRS 1980",6378137,298.257222101,
                AUTHORITY["EPSG","7019"]],
            TOWGS84[0,0,0,0,0,0,0],
            AUTHORITY["EPSG","6269"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9108"]],
        AUTHORITY["EPSG","4269"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",-93],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["METERS",1]]"""
    ret = tst.testOpen(check_gt=expected_gt, check_prj=expected_srs)

    if ret == 'success':
        ds = gdal.Open('data/e00grid/fake_e00grid_compressed.e00')
        line0 = ds.ReadRaster(0, 0, 5, 1)
        ds.ReadRaster(0, 1, 5, 1)
        line2 = ds.ReadRaster(0, 2, 5, 1)
        assert line0 != line2, 'should not have gotten the same values'
        ds.ReadRaster(0, 0, 5, 1)
        line2_bis = ds.ReadRaster(0, 2, 5, 1)
        assert line2 == line2_bis, 'did not get the same values for the same line'

        assert ds.GetRasterBand(1).GetMinimum() == 1, \
            'did not get expected minimum value'
        assert ds.GetRasterBand(1).GetMaximum() == 50, \
            'did not get expected maximum value'
        stats = ds.GetRasterBand(1).GetStatistics(False, True)
        assert stats == [1.0, 50.0, 25.5, 24.5], 'did not get expected statistics'

    return ret



