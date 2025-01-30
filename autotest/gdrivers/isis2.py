#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for ISIS2 driver.
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("ISIS2")

###############################################################################
# Read a truncated and modified version of arvidson_original.cub from
# ftp://ftpflag.wr.usgs.gov/dist/pigpen/venus/venustopo_download/ovda_dtm.zip


def test_isis2_1():

    tst = gdaltest.GDALTest("ISIS2", "isis2/arvidson_original_truncated.cub", 1, 382)
    expected_prj = """PROJCS["SIMPLE_CYLINDRICAL VENUS",
    GEOGCS["GCS_VENUS",
        DATUM["D_VENUS",
            SPHEROID["VENUS",6051000,0]],
        PRIMEM["Reference_Meridian",0],
        UNIT["degree",0.0174532925199433]],
    PROJECTION["Equirectangular"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",0],
    PARAMETER["standard_parallel_1",-6.5],
    PARAMETER["false_easting",0],
    PARAMETER["false_northing",0],
    UNIT["meter",1]]"""
    expected_gt = (
        10157400.403618813,
        1200.0000476837158,
        0.0,
        -585000.02324581146,
        0.0,
        -1200.0000476837158,
    )
    tst.testOpen(check_prj=expected_prj, check_gt=expected_gt)
