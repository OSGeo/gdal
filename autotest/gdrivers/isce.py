#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ISCE format driver.
# Author:   Matthieu Volat <matthieu.volat@ujf-grenoble.fr>
#
###############################################################################
# Copyright (c) 2014, Matthieu Volat <matthieu.volat@ujf-grenoble.fr>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("ISCE")

###############################################################################
# Perform simple read test.


def test_isce_1():

    tst = gdaltest.GDALTest("isce", "isce/isce.slc", 1, 350)

    prj = """GEOGCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        TOWGS84[0,0,0,0,0,0,0],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9108"]],
    AUTHORITY["EPSG","4326"]]"""

    tst.testOpen(
        check_prj=prj,
        check_gt=(
            14.259166666666667,
            0.0008333333333333334,
            0.0,
            38.22083333333333,
            0.0,
            -0.0008333333333333334,
        ),
    )


###############################################################################
# Test reading of metadata from the ISCE metadata domain


def test_isce_2():

    ds = gdal.Open("data/isce/isce.slc")
    val = ds.GetMetadataItem("IMAGE_TYPE", "ISCE")
    assert val == "slc"


###############################################################################
# Verify this can be exported losslessly.


def test_isce_3():

    tst = gdaltest.GDALTest("isce", "isce/isce.slc", 1, 350)
    tst.testCreateCopy(check_gt=0, new_filename="isce.tst.slc")


###############################################################################
# Verify VSIF*L capacity


def test_isce_4():

    tst = gdaltest.GDALTest("isce", "isce/isce.slc", 1, 350)
    tst.testCreateCopy(check_gt=0, new_filename="isce.tst.slc", vsimem=1)
