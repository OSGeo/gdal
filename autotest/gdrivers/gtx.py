#!/usr/bin/env pytest
###############################################################################
# $Id: gsc.py 16265 2009-02-08 11:15:27Z rouault $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GSC driver
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2010, Frank Warmerdam
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("GTX")


###############################################################################
# Test reading a small gtx file.


def test_gtx_1():

    tst = gdaltest.GDALTest("GTX", "gtx/hydroc1.gtx", 1, 64183)
    gt = (276.725, 0.05, 0.0, 42.775, 0.0, -0.05)
    tst.testOpen(check_gt=gt, check_prj="WGS84")
