#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GRASS ASCII Grid support.
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest

###############################################################################
# Perform simple read test.


def test_grassasciigrid_1():

    tst = gdaltest.GDALTest("GRASSASCIIGrid", "grassasciigrid/grassascii.txt", 1, 212)
    expected_gt = [-100.0, 62.5, 0.0, 250.0, 0.0, -41.666666666666664]
    tst.testOpen(check_gt=expected_gt)


###############################################################################
