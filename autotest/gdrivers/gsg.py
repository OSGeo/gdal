#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Golden Software ASCII and binary grid format.
# Author:   Andrey Kiselev <dron@ak4719.spb.edu>
#
###############################################################################
# Copyright (c) 2008, Andrey Kiselev <dron@ak4719.spb.edu>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest

###############################################################################
# Perform simple read tests.


def test_gsg_1():

    tst = gdaltest.GDALTest("gsbg", "gsg/gsg_binary.grd", 1, 4672)
    tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


def test_gsg_2():

    tst = gdaltest.GDALTest("gsag", "gsg/gsg_ascii.grd", 1, 4672)
    tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


def test_gsg_3():

    tst = gdaltest.GDALTest("gs7bg", "gsg/gsg_7binary.grd", 1, 4672)
    tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


###############################################################################
# Create simple copy and check.


def test_gsg_4():

    tst = gdaltest.GDALTest("gsbg", "gsg/gsg_binary.grd", 1, 4672)

    tst.testCreateCopy(check_gt=1)


def test_gsg_5():

    tst = gdaltest.GDALTest("gsag", "gsg/gsg_ascii.grd", 1, 4672)

    tst.testCreateCopy(check_gt=1)


def test_gsg_6():

    tst = gdaltest.GDALTest("gsbg", "gsg/gsg_binary.grd", 1, 4672)

    tst.testCreate(out_bands=1)


def test_gsg_7():

    tst = gdaltest.GDALTest("gs7bg", "gsg/gsg_7binary.grd", 1, 4672)

    tst.testCreate(out_bands=1)


def test_gsg_8():

    tst = gdaltest.GDALTest("gs7bg", "gsg/gsg_7binary.grd", 1, 4672)

    tst.testCreateCopy(check_gt=1)


###############################################################################
