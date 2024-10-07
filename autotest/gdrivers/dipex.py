#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DIPEx driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("DIPEx")

###############################################################################
# Test a fake DIPex dataset


def test_dipex_1():

    tst = gdaltest.GDALTest("DIPEx", "dipex/fakedipex.dat", 1, 1)
    tst.testOpen()
