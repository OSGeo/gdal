#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Generic Binary format driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import osr

pytestmark = pytest.mark.require_driver("GenBin")

###############################################################################
# Perform simple read test.


def test_genbin_1():

    with gdaltest.config_option("RAW_CHECK_FILE_SIZE", "NO"):
        tst = gdaltest.GDALTest("GenBin", "genbin/tm4628_96.bil", 1, 5738, 0, 0, 500, 1)

        sr = osr.SpatialReference()
        sr.ImportFromEPSG(32049)

        gt = (
            1181700.9894981384,
            82.021003723042099,
            0.0,
            596254.01050186157,
            0.0,
            -82.021003723045894,
        )

        tst.testOpen(check_prj=sr.ExportToWkt(), check_gt=gt)
