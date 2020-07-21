#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  BSB Testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2009, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import osr


import gdaltest
import pytest

###############################################################################
# Test driver availability


def test_bsb_0():
    gdaltest.bsb_dr = gdal.GetDriverByName('BSB')
    if gdaltest.bsb_dr is None:
        pytest.skip()

    
###############################################################################
# Test Read


def test_bsb_1():
    if gdaltest.bsb_dr is None:
        pytest.skip()

    tst = gdaltest.GDALTest('BSB', 'bsb/rgbsmall.kap', 1, 30321)

    return tst.testOpen()

###############################################################################
# Test CreateCopy


def test_bsb_2():
    if gdaltest.bsb_dr is None:
        pytest.skip()

    md = gdaltest.bsb_dr.GetMetadata()
    if 'DMD_CREATIONDATATYPES' not in md:
        pytest.skip()

    tst = gdaltest.GDALTest('BSB', 'bsb/rgbsmall.kap', 1, 30321)

    return tst.testCreateCopy()

###############################################################################
# Read a BSB with an index table at the end (#2782)
# The rgbsmall_index.kap has been generated from rgbsmall.kap by moving the
# data of first line from offset 2382 to offset 2384, and generating the index table
# --> This is probably not a valid BSB file, but it proves that we can read the index table


def test_bsb_3():
    if gdaltest.bsb_dr is None:
        pytest.skip()

    tst = gdaltest.GDALTest('BSB', 'bsb/rgbsmall_index.kap', 1, 30321)

    return tst.testOpen()

###############################################################################
# Read a BSB without an index table but with 0 in the middle of line data
# The rgbsmall_with_line_break.kap has been generated from rgbsmall.kap by
# adding a 0 character in the middle of line data


def test_bsb_4():
    if gdaltest.bsb_dr is None:
        pytest.skip()

    tst = gdaltest.GDALTest('BSB', 'bsb/rgbsmall_with_line_break.kap', 1, 30321)

    return tst.testOpen()

###############################################################################
# Read a truncated BSB (at the level of the written scanline number starting a new row)


def test_bsb_5():
    if gdaltest.bsb_dr is None:
        pytest.skip()

    tst = gdaltest.GDALTest('BSB', 'bsb/rgbsmall_truncated.kap', 1, 29696)

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = tst.testOpen()
    gdal.PopErrorHandler()

    return ret

###############################################################################
# Read another truncated BSB (in the middle of row data)


def test_bsb_6():
    if gdaltest.bsb_dr is None:
        pytest.skip()

    tst = gdaltest.GDALTest('BSB', 'bsb/rgbsmall_truncated2.kap', 1, 29696)

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = tst.testOpen()
    gdal.PopErrorHandler()

    return ret

###############################################################################

def test_bsb_tmerc():
    if gdaltest.bsb_dr is None:
        pytest.skip()

    ds = gdal.Open('data/bsb/transverse_mercator.kap')
    gt = ds.GetGeoTransform()
    expected_gt = [28487.6637325402, 1.2711141208521637, 0.009061669923111566,
                   6539651.728646593, 0.015209115944776083, -1.267821834560455]
    assert min([gt[i] == pytest.approx(expected_gt[i], abs=1e-8 * abs(expected_gt[i])) for i in range(6)]) == True, gt
    expected_wkt = """PROJCS["unnamed",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            AUTHORITY["EPSG","6326"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AUTHORITY["EPSG","4326"]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",18.0582833333333],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["Meter",1],
    AXIS["Easting",EAST],
    AXIS["Northing",NORTH]]"""
    expected_sr = osr.SpatialReference()
    expected_sr.SetFromUserInput(expected_wkt)
    expected_sr.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER);
    got_sr = ds.GetSpatialRef()
    assert expected_sr.IsSame(got_sr), got_sr.ExportToWkt()
    got_sr = ds.GetGCPSpatialRef()
    assert expected_sr.IsSame(got_sr), got_sr.ExportToWkt()
    assert ds.GetGCPCount() == 3
    gcps = ds.GetGCPs()
    assert len(gcps) == 3

    assert gcps[0].GCPPixel == 25 and \
       gcps[0].GCPLine == 577 and \
       gcps[0].GCPX == pytest.approx(28524.670169107143, abs=1e-5) and \
       gcps[0].GCPY == pytest.approx(6538920.57567595, abs=1e-5) and \
       gcps[0].GCPZ == 0

###############################################################################


def test_bsb_cutline():
    if gdaltest.bsb_dr is None:
        pytest.skip()

    ds = gdal.Open('data/bsb/australia4c.kap')
    assert ds.GetMetadataItem('BSB_CUTLINE') == 'POLYGON ((112.72859333333334 -8.25404666666667,156.57827333333333 -7.66159166666667,164.28394166666666 -40.89653000000000,106.53042166666667 -41.14970000000000))'
