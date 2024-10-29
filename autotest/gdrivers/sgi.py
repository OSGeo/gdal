#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  PNM (Portable Anyware Map) Testing.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest

###############################################################################
# Read existing simple 1 band SGI file.


def test_sgi_1():

    tst = gdaltest.GDALTest("SGI", "sgi/byte.sgi", 1, 4672)

    tst.testOpen()


###############################################################################
# Write Test grayscale


def test_sgi_2():

    tst = gdaltest.GDALTest("SGI", "byte.tif", 1, 4672)

    tst.testCreate()


###############################################################################
# Write Test rgb


def test_sgi_3():

    tst = gdaltest.GDALTest("SGI", "rgbsmall.tif", 2, 21053)

    tst.testCreate()
