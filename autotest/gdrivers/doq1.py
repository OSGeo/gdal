#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DOQ1 driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("DOQ1")

###############################################################################
# Test a fake DOQ1 dataset


def test_doq1_1():

    tst = gdaltest.GDALTest("DOQ1", "doq1/fakedoq1.doq", 1, -1)
    with pytest.raises(Exception):
        tst.testOpen()
