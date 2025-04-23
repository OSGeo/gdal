#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test CPG driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("CPG")

###############################################################################
# Test a fake CPG dataset


def test_cpg_1():

    tst = gdaltest.GDALTest("CPG", "cpg/fakecpgSIRC.hdr", 1, 0)
    return tst.testOpen()
