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
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################



import gdaltest

###############################################################################
# Perform simple read tests.


def test_gsg_1():

    tst = gdaltest.GDALTest('gsbg', 'gsg/gsg_binary.grd', 1, 4672)
    return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


def test_gsg_2():

    tst = gdaltest.GDALTest('gsag', 'gsg/gsg_ascii.grd', 1, 4672)
    return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


def test_gsg_3():

    tst = gdaltest.GDALTest('gs7bg', 'gsg/gsg_7binary.grd', 1, 4672)
    return tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))

###############################################################################
# Create simple copy and check.


def test_gsg_4():

    tst = gdaltest.GDALTest('gsbg', 'gsg/gsg_binary.grd', 1, 4672)

    return tst.testCreateCopy(check_gt=1)


def test_gsg_5():

    tst = gdaltest.GDALTest('gsag', 'gsg/gsg_ascii.grd', 1, 4672)

    return tst.testCreateCopy(check_gt=1)


def test_gsg_6():

    tst = gdaltest.GDALTest('gsbg', 'gsg/gsg_binary.grd', 1, 4672)

    return tst.testCreate(out_bands=1)


def test_gsg_7():

    tst = gdaltest.GDALTest('gs7bg', 'gsg/gsg_7binary.grd', 1, 4672)

    return tst.testCreate(out_bands=1)


def test_gsg_8():

    tst = gdaltest.GDALTest('gs7bg', 'gsg/gsg_7binary.grd', 1, 4672)

    return tst.testCreateCopy(check_gt=1)

###############################################################################




