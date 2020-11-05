#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  National Resources Canada - Vertical Datum Transformation
# Purpose:  Test read/write functionality for BYN driver.
# Author:   Ivan Lucena, ivan.lucena@outlook.com
#
###############################################################################
# Copyright (c) 2018, Ivan Lucena
# Copyright (c) 2018, Even Rouault
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
# Read test of byte file.


def test_byn_1():

    tst = gdaltest.GDALTest('BYN', 'byn/cgg2013ai08_reduced.byn', 1, 64764)
    return tst.testOpen()

###############################################################################
#

def test_byn_2():

    tst = gdaltest.GDALTest('BYN', 'byn/cgg2013ai08_reduced.byn', 1, 64764)
    return tst.testCreateCopy(new_filename='tmp/byn_test_2.byn')

###############################################################################
#

def test_byn_invalid_header_bytes():

    tst = gdaltest.GDALTest('BYN', 'byn/test_invalid_header_bytes.byn', 1, 64764)
    return tst.testOpen()

