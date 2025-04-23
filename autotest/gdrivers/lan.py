#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test LAN driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("LAN")

###############################################################################
# Test reading a - fake - LAN 8 bit dataset


def test_lan_1():

    tst = gdaltest.GDALTest("LAN", "lan/fakelan.lan", 1, 10)
    tst.testOpen()


###############################################################################
# Test reading a - fake - LAN 4 bit dataset


def test_lan_2():

    tst = gdaltest.GDALTest("LAN", "lan/fakelan4bit.lan", 1, 10)
    tst.testOpen()
