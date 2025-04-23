#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test NLAPS/NDF driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("NDF")

###############################################################################
# Simple image test of an NLAPS/NDF2 dataset.


def test_ndf_1():

    tst = gdaltest.GDALTest(
        "NDF",
        "ndf/LE7134052000500350.H3",
        1,
        6510,
        xoff=0,
        yoff=0,
        xsize=15620,
        ysize=1,
    )

    gt = (320325.75, 14.25, 0, 1383062.25, 0, -14.25)

    wkt = """PROJCS["UTM Zone 46, Northern Hemisphere",
    GEOGCS["WGS 84",
        DATUM["WGS_1984",
            SPHEROID["WGS 84",6378137,298.257223563,
                AUTHORITY["EPSG","7030"]],
            TOWGS84[0,0,0,0,0,0,0],
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
    PARAMETER["central_meridian",93],
    PARAMETER["scale_factor",0.9996],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["Meter",1]]"""

    tst.testOpen(check_gt=gt, gt_epsilon=0.0001, check_prj=wkt)
