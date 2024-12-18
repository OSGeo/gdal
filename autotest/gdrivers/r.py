#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test R driver support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest

###############################################################################
# Perform simple read test on an ascii file.


def test_r_1():

    tst = gdaltest.GDALTest("R", "r/r_test.asc", 2, 202)

    tst.testOpen()


###############################################################################
# Perform a simple read test on a binary (uncompressed) file.


def test_r_2():

    tst = gdaltest.GDALTest("R", "r/r_test.rdb", 1, 202)
    tst.testOpen()


###############################################################################
# Verify a simple createcopy operation with 16bit data.


def test_r_3():

    tst = gdaltest.GDALTest("R", "byte.tif", 1, 4672, options=["ASCII=YES"])
    tst.testCreateCopy()


###############################################################################
# Test creating a compressed binary stream and reading it back.


def test_r_4():

    tst = gdaltest.GDALTest("R", "byte.tif", 1, 4672)
    return tst.testCreateCopy(new_filename="tmp/r_4.rda")


###############################################################################
