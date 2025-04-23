#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test EIR driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("EIR")

###############################################################################
# Test a fake EIR dataset


def test_eir_1():

    tst = gdaltest.GDALTest("EIR", "eir/fakeeir.hdr", 1, 1)
    tst.testOpen()
