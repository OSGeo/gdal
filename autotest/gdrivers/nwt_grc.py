#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Northwood GRC driver
# Author:   Chaitanya kumar CH, <chaitanya at osgeo dot in>
#
###############################################################################
# Copyright (c) 2009, Chaitanya kumar CH, <chaitanya at osgeo dot in>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("NWT_GRC")

###############################################################################
# Test a GRC dataset


def test_nwt_grc_1():

    tst = gdaltest.GDALTest("NWT_GRC", "nwt_grc/nwt_grc.grc", 1, 46760)
    tst.testOpen()
