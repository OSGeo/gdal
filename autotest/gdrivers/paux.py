#!/usr/bin/env pytest
###############################################################################
# $Id$
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

from osgeo import gdal

pytestmark = pytest.mark.require_driver("PAUX")

###############################################################################
# Read test of simple byte reference data.


def test_paux_1():

    tst = gdaltest.GDALTest("PAux", "paux/small16.raw", 2, 12816)
    tst.testOpen()


###############################################################################
# Test copying.


def test_paux_2():

    tst = gdaltest.GDALTest("PAux", "byte.tif", 1, 4672)

    tst.testCreateCopy(check_gt=1)


###############################################################################
# Test /vsimem based.


def test_paux_3():

    tst = gdaltest.GDALTest("PAux", "byte.tif", 1, 4672)

    tst.testCreateCopy(vsimem=1)


###############################################################################
# Cleanup.


def test_paux_cleanup():
    gdaltest.clean_tmp()
    if gdal.VSIStatL("/vsimem/byte.tif.tst.aux.xml") is not None:
        gdal.Unlink("/vsimem/byte.tif.tst.aux.xml")
