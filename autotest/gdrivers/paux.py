#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for PAux driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("PAUX")

###############################################################################
# Read test of simple byte reference data.


def test_paux_1():

    tst = gdaltest.GDALTest("PAux", "paux/small16.raw", 2, 12816)
    tst.testOpen()
