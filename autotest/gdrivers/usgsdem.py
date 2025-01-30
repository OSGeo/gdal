#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for USGSDEM driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest

from osgeo import osr

pytestmark = pytest.mark.require_driver("USGSDEM")

###############################################################################
# Test truncated version of http://download.osgeo.org/gdal/data/usgsdem/022gdeme


def test_usgsdem_1():

    tst = gdaltest.GDALTest("USGSDEM", "usgsdem/022gdeme_truncated", 1, 1583)
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("NAD27")
    tst.testOpen(
        check_prj=srs.ExportToWkt(),
        check_gt=(-67.00041667, 0.00083333, 0.0, 50.000416667, 0.0, -0.00083333),
    )


###############################################################################
# Test truncated version of http://download.osgeo.org/gdal/data/usgsdem/114p01_0100_deme.dem


def test_usgsdem_2():

    tst = gdaltest.GDALTest(
        "USGSDEM", "usgsdem/114p01_0100_deme_truncated.dem", 1, 53864
    )
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("NAD27")
    tst.testOpen(
        check_prj=srs.ExportToWkt(),
        check_gt=(
            -136.25010416667,
            0.000208333,
            0.0,
            59.25010416667,
            0.0,
            -0.000208333,
        ),
    )


###############################################################################
# Test truncated version of file that triggered bug #2348


def test_usgsdem_3():

    tst = gdaltest.GDALTest("USGSDEM", "usgsdem/39079G6_truncated.dem", 1, 61424)
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS72")
    srs.SetUTM(17)
    tst.testOpen(
        check_prj=srs.ExportToWkt(),
        check_gt=(606855.0, 30.0, 0.0, 4414605.0, 0.0, -30.0),
    )


###############################################################################
# Test truncated version of http://download.osgeo.org/gdal/data/usgsdem/various.zip/39109h1.dem
# Undocumented format


def test_usgsdem_8():

    tst = gdaltest.GDALTest("USGSDEM", "usgsdem/39109h1_truncated.dem", 1, 39443)
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("NAD27")
    srs.SetUTM(12)
    tst.testOpen(
        check_prj=srs.ExportToWkt(),
        check_gt=(660055.0, 10.0, 0.0, 4429465.0, 0.0, -10.0),
    )


###############################################################################
# Test truncated version of http://download.osgeo.org/gdal/data/usgsdem/various.zip/4619old.dem
# Old format


def test_usgsdem_9():

    tst = gdaltest.GDALTest("USGSDEM", "usgsdem/4619old_truncated.dem", 1, 10659)
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("NAD27")
    tst.testOpen(
        check_prj=srs.ExportToWkt(),
        check_gt=(18.99958333, 0.0008333, 0.0, 47.000416667, 0.0, -0.0008333),
    )


###############################################################################
# https://github.com/OSGeo/gdal/issues/583


def test_usgsdem_with_extra_values_at_end_of_profile():

    tst = gdaltest.GDALTest(
        "USGSDEM", "usgsdem/usgsdem_with_extra_values_at_end_of_profile.dem", 1, 56679
    )
    tst.testOpen()


###############################################################################
# Like Novato.dem of https://trac.osgeo.org/gdal/ticket/4901


def test_usgsdem_with_spaces_after_byte_864():

    tst = gdaltest.GDALTest(
        "USGSDEM", "usgsdem/usgsdem_with_spaces_after_byte_864.dem", 1, 61078
    )
    tst.testOpen()


###############################################################################
# Test truncated version of https://s3.amazonaws.com/data.tnris.org/8ea19b45-7a66-4e95-9833-f9e89611d106/resources/fema06-140cm-coastal_2995441_dem.zip
# downloaded from https://data.tnris.org/collection/8ea19b45-7a66-4e95-9833-f9e89611d106


def test_usgsdem_with_header_of_918_bytes():

    tst = gdaltest.GDALTest(
        "USGSDEM", "usgsdem/fema06-140cm_2995441b_truncated.dem", 1, -1
    )
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("NAD83")
    srs.SetUTM(15)
    with pytest.raises(Exception):
        tst.testOpen(
            check_prj=srs.ExportToWkt(),
            check_gt=(248500.0, 1.4, 0.0, 3252508.7, 0.0, -1.4),
        )


###############################################################################
# Test dataset with 1025 byte records ending with linefeed (#5007


def test_usgsdem_record_1025_bytes_ending_with_linefeed():

    tst = gdaltest.GDALTest(
        "USGSDEM", "usgsdem/record_1025_ending_with_linefeed.dem", 1, 14172
    )
    tst.testOpen()


###############################################################################
# Cleanup


def test_usgsdem_cleanup():

    try:
        os.remove("tmp/n43.dem")
        os.remove("tmp/n43.dem.aux.xml")

        os.remove("tmp/file_1.dem")
        os.remove("tmp/file_1.dem.aux.xml")
        os.remove("tmp/file_2.dem")
        os.remove("tmp/file_2.dem.aux.xml")

        os.remove("tmp/000a00DEMz")
        os.remove("tmp/000a00DEMz.aux.xml")
    except OSError:
        pass
