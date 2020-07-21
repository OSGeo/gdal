#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for NGSGEOID driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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
# Test opening a little endian file


def test_ngsgeoid_1():

    tst = gdaltest.GDALTest('NGSGEOID', 'ngsgeoid/g2009u01_le_truncated.bin', 1, 65534)
    return tst.testOpen(check_gt=(229.99166666666667, 0.016666666666670001, 0.0, 40.00833333333334, 0.0, -0.016666666666670001), check_prj='WGS84')

###############################################################################
# Test opening a big endian file


def test_ngsgeoid_2():

    tst = gdaltest.GDALTest('NGSGEOID', 'ngsgeoid/g2009u01_be_truncated.bin', 1, 65534)
    return tst.testOpen(check_gt=(229.99166666666667, 0.016666666666670001, 0.0, 40.00833333333334, 0.0, -0.016666666666670001), check_prj='WGS84')



