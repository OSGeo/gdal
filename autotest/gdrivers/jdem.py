#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test JDEM driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest

###############################################################################
# Test reading a - fake - JDEM dataset


def test_jdem_1():

    tst = gdaltest.GDALTest("JDEM", "jdem/fakejdem.mem", 1, 15)
    tst.testOpen()
