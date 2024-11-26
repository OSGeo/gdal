#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GSC driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("GSC")

###############################################################################
# Test a fake - and certainly incorrect - GSC dataset


def test_gsc_1():

    tst = gdaltest.GDALTest("GSC", "gsc/fakegsc.gsc", 1, 0)
    tst.testOpen()
