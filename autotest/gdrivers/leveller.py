#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Leveller driver support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest

###############################################################################
# Perform simple read test.


def test_leveller_1():

    tst = gdaltest.GDALTest("Leveller", "leveller/ter6test.ter", 1, 33441)
    tst.testOpen()
