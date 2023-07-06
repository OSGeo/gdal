#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ERS format driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

pytestmark = pytest.mark.require_driver("ERS")

###############################################################################
# Perform simple read test.


def test_ers_1():

    tst = gdaltest.GDALTest("ERS", "ers/srtm.ers", 1, 64074)
    tst.testOpen()
    ds = gdal.Open("data/ers/srtm.ers")
    md = ds.GetRasterBand(1).GetMetadata()
    expected_md = {
        "STATISTICS_MEAN": "-4020.25",
        "STATISTICS_MINIMUM": "-4315",
        "STATISTICS_MAXIMUM": "-3744",
        "STATISTICS_MEDIAN": "-4000",
    }
    assert md == expected_md


###############################################################################
# Create simple copy and check.


def test_ers_2():

    tst = gdaltest.GDALTest("ERS", "ehdr/float32.bil", 1, 27)
    tst.testCreateCopy(new_filename="tmp/float32.ers", check_gt=1, vsimem=1)


###############################################################################
# Test multi-band file.


def test_ers_3():

    tst = gdaltest.GDALTest("ERS", "rgbsmall.tif", 2, 21053)
    tst.testCreate(new_filename="tmp/rgbsmall.ers")


###############################################################################
# Test HeaderOffset case.


def test_ers_4():

    gt = (143.59625, 0.025, 0.0, -39.38125, 0.0, -0.025)
    srs = """GEOGCS["GEOCENTRIC DATUM of AUSTRALIA",
    DATUM["GDA94",
        SPHEROID["GRS80",6378137,298.257222101]],
    PRIMEM["Greenwich",0],
    UNIT["degree",0.0174532925199433]]"""

    tst = gdaltest.GDALTest("ERS", "ers/ers_dem.ers", 1, 56588)
    tst.testOpen(check_prj=srs, check_gt=gt)


###############################################################################
# Confirm we can recognised signed 8bit data.


def test_ers_5():

    ds = gdal.Open("data/ers/8s.ers")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int8

    ds = None


###############################################################################
# Confirm a copy preserves the signed byte info.


def test_ers_6():

    drv = gdal.GetDriverByName("ERS")

    src_ds = gdal.Open("data/ers/8s.ers")

    ds = drv.CreateCopy("tmp/8s.ers", src_ds)
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int8

    ds = None

    drv.Delete("tmp/8s.ers")


###############################################################################
# Test opening a file with everything in lower case.


def test_ers_7():

    ds = gdal.Open("data/ers/caseinsensitive.ers")

    desc = ds.GetRasterBand(1).GetDescription()

    assert desc == "RTP 1st Vertical Derivative", "did not get expected values."


###############################################################################
# Test GCP support


def test_ers_8():

    src_ds = gdal.Open("../gcore/data/gcps.vrt")
    drv = gdal.GetDriverByName("ERS")
    ds = drv.CreateCopy("/vsimem/ers_8.ers", src_ds)
    ds = None

    if gdal.VSIStatL("/vsimem/ers_8.ers.aux.xml") is not None:
        gdal.Unlink("/vsimem/ers_8.ers.aux.xml")

    ds = gdal.Open("/vsimem/ers_8.ers")
    expected_gcps = src_ds.GetGCPs()
    gcps = ds.GetGCPs()
    gcp_count = ds.GetGCPCount()
    wkt = ds.GetGCPProjection()
    ds = None

    assert (
        wkt.replace(',AUTHORITY["EPSG","9122"]', "").replace(
            ',AUTHORITY["EPSG","9108"]', ""
        )
        == """PROJCS["NUTM11",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.978698213898,AUTHORITY["EPSG","7008"]],TOWGS84[-3,142,183,0,0,0,0],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Metre",1],AXIS["Easting",EAST],AXIS["Northing",NORTH]]"""
    ), "did not get expected GCP projection"

    assert (
        len(gcps) == len(expected_gcps) and len(gcps) == gcp_count
    ), "did not get expected GCP number"

    for i, gcp in enumerate(gcps):
        if (
            gcp.GCPPixel != pytest.approx(expected_gcps[i].GCPPixel, abs=1e-6)
            or gcp.GCPLine != pytest.approx(expected_gcps[i].GCPLine, abs=1e-6)
            or gcp.GCPX != pytest.approx(expected_gcps[i].GCPX, abs=1e-6)
            or gcp.GCPY != pytest.approx(expected_gcps[i].GCPY, abs=1e-6)
        ):
            print(gcps[i])
            pytest.fail("did not get expected GCP %d" % i)

    drv.Delete("/vsimem/ers_8.ers")


###############################################################################
# Test NoData support (#4207)


def test_ers_9():

    drv = gdal.GetDriverByName("ERS")
    ds = drv.Create("/vsimem/ers_9.ers", 1, 1)
    ds.GetRasterBand(1).SetNoDataValue(123)
    ds = None

    f = gdal.VSIFOpenL("/vsimem/ers_9.ers.aux.xml", "rb")
    if f is not None:
        gdal.VSIFCloseL(f)
        drv.Delete("/vsimem/ers_9.ers")
        pytest.fail("/vsimem/ers_9.ers.aux.xml should not exist")

    ds = gdal.Open("/vsimem/ers_9.ers")
    val = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    drv.Delete("/vsimem/ers_9.ers")

    assert val == 123, "did not get expected nodata value"


###############################################################################
# Test PROJ, DATUM, UNITS support (#4229)


def test_ers_10():

    drv = gdal.GetDriverByName("ERS")
    ds = drv.Create(
        "/vsimem/ers_10.ers",
        1,
        1,
        options=["DATUM=GDA94", "PROJ=MGA55", "UNITS=METERS"],
    )

    proj = ds.GetMetadataItem("PROJ", "ERS")
    datum = ds.GetMetadataItem("DATUM", "ERS")
    units = ds.GetMetadataItem("UNITS", "ERS")
    assert proj == "MGA55", "did not get expected PROJ"

    assert datum == "GDA94", "did not get expected DATUM"

    assert units == "METERS", "did not get expected UNITS"

    # This should be overridden by the above values
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())

    proj = ds.GetMetadataItem("PROJ", "ERS")
    datum = ds.GetMetadataItem("DATUM", "ERS")
    units = ds.GetMetadataItem("UNITS", "ERS")
    assert proj == "MGA55", "did not get expected PROJ"

    assert datum == "GDA94", "did not get expected DATUM"

    assert units == "METERS", "did not get expected UNITS"

    ds = None

    f = gdal.VSIFOpenL("/vsimem/ers_10.ers.aux.xml", "rb")
    if f is not None:
        gdal.VSIFCloseL(f)
        drv.Delete("/vsimem/ers_10.ers")
        pytest.fail("/vsimem/ers_10.ers.aux.xml should not exist")

    ds = gdal.Open("/vsimem/ers_10.ers")
    wkt = ds.GetProjectionRef()
    proj = ds.GetMetadataItem("PROJ", "ERS")
    datum = ds.GetMetadataItem("DATUM", "ERS")
    units = ds.GetMetadataItem("UNITS", "ERS")
    md_ers = ds.GetMetadata("ERS")
    ds = None

    drv.Delete("/vsimem/ers_10.ers")

    assert proj == "MGA55", "did not get expected PROJ"

    assert datum == "GDA94", "did not get expected DATUM"

    assert units == "METERS", "did not get expected UNITS"

    assert (
        md_ers["PROJ"] == proj and md_ers["DATUM"] == datum and md_ers["UNITS"] == units
    ), ("GetMetadata() not consistent with " "GetMetadataItem()")

    assert wkt.startswith("""PROJCS["MGA55"""), "did not get expected projection"

    ds = drv.Create(
        "/vsimem/ers_10.ers", 1, 1, options=["DATUM=GDA94", "PROJ=MGA55", "UNITS=FEET"]
    )
    ds = None

    # Check that we can update those values with SetProjection()
    ds = gdal.Open("/vsimem/ers_10.ers", gdal.GA_Update)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    proj = ds.GetMetadataItem("PROJ", "ERS")
    datum = ds.GetMetadataItem("DATUM", "ERS")
    units = ds.GetMetadataItem("UNITS", "ERS")
    assert proj == "GEODETIC", "did not get expected PROJ"

    assert datum == "WGS84", "did not get expected DATUM"

    assert units == "METERS", "did not get expected UNITS"
    ds = None

    ds = gdal.Open("/vsimem/ers_10.ers")
    proj = ds.GetMetadataItem("PROJ", "ERS")
    datum = ds.GetMetadataItem("DATUM", "ERS")
    units = ds.GetMetadataItem("UNITS", "ERS")
    ds = None

    drv.Delete("/vsimem/ers_10.ers")

    assert proj == "GEODETIC", "did not get expected PROJ"

    assert datum == "WGS84", "did not get expected DATUM"

    assert units == "METERS", "did not get expected UNITS"


###############################################################################
# Test fix for https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=8744


def test_ers_recursive_opening():
    ds = gdal.Open("/vsitar/data/ers/test_ers_recursive.tar/test.ers")
    with pytest.raises(Exception):
        ds.GetFileList()


###############################################################################
# Cleanup


def test_ers_cleanup():
    gdaltest.clean_tmp()
