#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Scaled Integer Gridded Elevation Model support.
# Author:   Paul Austin <paul.austin@revolsys.com>
#
###############################################################################
# Copyright (c) 2018, Paul Austin <paul.austin@revolsys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

###############################################################################
# Create simple copy and check.


def test_sigdem_copy_check_prj():

    tst = gdaltest.GDALTest("SIGDEM", "byte.tif", 1, 4672)

    prj = 'PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke_1866",6378206.4,294.9786982138982]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1]]'

    tst.testCreateCopy(check_gt=1, check_srs=prj)


###############################################################################
# Verify writing files with non-square pixels.


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_sigdem_non_square():

    with gdaltest.config_option("GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE", "ALL"):
        tst = gdaltest.GDALTest("SIGDEM", "sigdem/nonsquare_nad27_utm11.vrt", 1, 12481)

    prj = 'PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke_1866",6378206.4,294.9786982138982]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["Meter",1]]'

    with gdaltest.config_option("GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE", "ALL"):
        tst.testCreateCopy(check_gt=1, check_srs=prj)


###############################################################################
# Test creating an in memory copy.


def test_sigdem_in_memory():

    tst = gdaltest.GDALTest("SIGDEM", "byte.tif", 1, 4672)

    tst.testCreateCopy(vsimem=1)


###############################################################################
