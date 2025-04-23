#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test functionality of NOAA_B driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("NOAA_B")

###############################################################################
#


def test_noaa_b():

    tst = gdaltest.GDALTest("NOAA_B", "noaa_b/test.b", 1, 3)
    tst.testOpen(check_gt=(1.75, 0.5, 0.0, 49.25, 0.0, -0.5))


###############################################################################
#


def test_noaa_b_little_endian():

    tst = gdaltest.GDALTest("NOAA_B", "noaa_b/test_little_endian.b", 1, 3)
    tst.testOpen(check_gt=(1.75, 0.5, 0.0, 49.25, 0.0, -0.5))
