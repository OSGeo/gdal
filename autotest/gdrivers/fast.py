#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test EOSAT FAST Format support.
# Author:   Mateusz Loskot <mateusz@loskot.net>
#
###############################################################################
# Copyright (c) 2007, Mateusz Loskot <mateusz@loskot.net>
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

import gdaltest
import pytest

from osgeo import gdal, osr

pytestmark = pytest.mark.require_driver("FAST")

###############################################################################
# Perform simple read test.


def test_fast_2():

    # Actually, the band (a placeholder) is of 0 bytes size,
    # so the checksum is 0 expected.

    tst = gdaltest.GDALTest(
        "fast", "fast/L71118038_03820020111_HPN.FST", 1, 60323, 0, 0, 5000, 1
    )
    tst.testOpen()


###############################################################################
# Verify metadata.


@pytest.fixture()
def fast_ds():
    ds = gdal.Open("data/fast/L71118038_03820020111_HPN.FST")

    assert ds is not None, "Missing test dataset"

    return ds


def test_fast_3(fast_ds):

    ds = fast_ds

    md = ds.GetMetadata()
    assert md is not None, "Missing metadata in test dataset"

    assert md["ACQUISITION_DATE"] == "20020111", "ACQUISITION_DATE wrong"

    assert md["SATELLITE"] == "LANDSAT7", "SATELLITE wrong"

    assert md["SENSOR"] == "ETM+", "SENSOR wrong"

    # GAIN and BIAS expected values
    gb_expected = (-6.199999809265137, 0.775686297697179)

    gain = float(md["GAIN1"])
    if gain != pytest.approx(gb_expected[0], abs=0.0001):
        print("expected:", gb_expected[0])
        print("got:", gain)

    bias = float(md["BIAS1"])
    if bias != pytest.approx(gb_expected[1], abs=0.0001):
        print("expected:", gb_expected[1])
        print("got:", bias)


###############################################################################
# Test geotransform data.


def test_fast_4(fast_ds):

    gt = fast_ds.GetGeoTransform()

    tolerance = 0.01
    assert (
        gt[0] == pytest.approx(280342.5, abs=tolerance)
        and gt[1] == pytest.approx(15.0, abs=tolerance)
        and gt[2] == pytest.approx(0.0, abs=tolerance)
        and gt[3] == pytest.approx(3621457.5, abs=tolerance)
        and gt[4] == pytest.approx(0.0, abs=tolerance)
        and abs(gt[5] + 15.0) <= tolerance
    ), "FAST geotransform wrong"


###############################################################################
# Test 2 bands dataset with checking projections and geotransform.


def test_fast_5():

    tst = gdaltest.GDALTest(
        "fast", "fast/L71230079_07920021111_HTM.FST", 2, 19110, 0, 0, 7000, 1
    )

    # Expected parameters of the geotransform
    gt = (528417.25, 30.0, 0.0, 7071187.0, 0.0, -30.0)

    # Expected definition of the projection
    proj = """PROJCS["unnamed",
        GEOGCS["WGS 84",
            DATUM["WGS_1984",
                SPHEROID["WGS 84",6378137,298.257223563,
                    AUTHORITY["EPSG","7030"]],
                AUTHORITY["EPSG","6326"]],
            PRIMEM["Greenwich",0,
                AUTHORITY["EPSG","8901"]],
            UNIT["degree",0.0174532925199433,
                AUTHORITY["EPSG","9108"]],
            AXIS["Lat",NORTH],
            AXIS["Long",EAST],
            AUTHORITY["EPSG","4326"]],
        PROJECTION["Transverse_Mercator"],
        PARAMETER["latitude_of_origin",0],
        PARAMETER["central_meridian",-66],
        PARAMETER["scale_factor",1],
        PARAMETER["false_easting",500000],
        PARAMETER["false_northing",10002288.3],
        UNIT["Meter",1]]"""

    tst.testOpen(check_gt=gt, check_prj=proj)


###############################################################################
# Test Euromap LISS3 dataset


def test_fast_6():

    tst = gdaltest.GDALTest("fast", "fast/n0o0y867.0fl", 1, 0, 0, 0, 2741, 1)

    # Expected parameters of the geotransform
    gt = (
        14640936.89174916,
        1.008817518246492,
        24.9876841746236,
        664274.3912497687,
        24.98828832116786,
        -0.9907878581173808,
    )

    # Expected definition of the projection
    proj = """LOCAL_CS["GCTP projection number 22",
    UNIT["Meter",1]]"""

    tst.testOpen(check_gt=gt, check_prj=proj)


###############################################################################
# Test Euromap PAN dataset


def test_fast_7():

    tst = gdaltest.GDALTest("fast", "fast/h0o0y867.1ah", 1, 0, 0, 0, 5815, 1)

    # Expected parameters of the geotransform
    gt = (676565.09, 5, 0, 5348341.5, 0, -5)

    # Expected definition of the projection
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32632)
    proj = srs.ExportToWkt()

    tst.testOpen(check_gt=gt, check_prj=proj)


###############################################################################
# Test Euromap WIFS dataset


def test_fast_8():

    tst = gdaltest.GDALTest("fast", "fast/w0y13a4t.010", 1, 0, 0, 0, 4748, 1)

    # Expected parameters of the geotransform
    gt = (
        -336965.0150603952,
        176.0817495260164,
        -37.35662873563219,
        484122.7765089957,
        -37.35622603749736,
        -176.081791954023,
    )

    # Expected definition of the projection
    proj = """PROJCS["unnamed",
    GEOGCS["Unknown datum based upon the International 1924 ellipsoid",
        DATUM["Not specified (based on International 1924 spheroid)",
            SPHEROID["International 1924",6378388,297,
                AUTHORITY["EPSG","7022"]]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Lambert_Conformal_Conic_2SP"],
    PARAMETER["standard_parallel_1",44.14623833735833],
    PARAMETER["standard_parallel_2",41.36002161426806],
    PARAMETER["latitude_of_origin",42.71125349618411],
    PARAMETER["central_meridian",16.31349670734809],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["Meter",1]]"""

    tst.testOpen(check_gt=gt, check_prj=proj)


###############################################################################
# Check some metadata and opening for a RevB L7 file (#3306, #3307).


def test_fast_9():

    ds = gdal.Open("data/fast/HEADER.DAT")
    assert ds.GetMetadataItem("SENSOR") == "", "Did not get expected SENSOR value."

    assert ds.RasterCount == 7, "Did not get expected band count."


###############################################################################
#
