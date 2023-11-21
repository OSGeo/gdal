#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for RST/Idrisi driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
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

import shutil

import gdaltest

###############################################################################
# Read test of byte file.


def test_idrisi_1(tmp_path):

    shutil.copy("data/rst/byte.rst", tmp_path)
    shutil.copy("data/rst/byte.rdc", tmp_path)

    tst = gdaltest.GDALTest(
        "RST", tmp_path / "byte.rst", 1, 5044, filename_absolute=True
    )
    tst.testOpen()


###############################################################################
# Read test of byte file.


def test_idrisi_2(tmp_path):

    shutil.copy("data/rst/real.rst", tmp_path)
    shutil.copy("data/rst/real.rdc", tmp_path)

    tst = gdaltest.GDALTest(
        "RST", tmp_path / "real.rst", 1, 5275, filename_absolute=True
    )
    tst.testOpen()


###############################################################################
#


def test_idrisi_3(tmp_path):

    shutil.copy("data/ehdr/float32.bil", tmp_path)
    shutil.copy("data/ehdr/float32.hdr", tmp_path)
    shutil.copy("data/ehdr/float32.prj", tmp_path)

    tst = gdaltest.GDALTest(
        "RST", tmp_path / "float32.bil", 1, 27, filename_absolute=True
    )

    tst.testCreate(new_filename=tmp_path / "float32.rst", out_bands=1, vsimem=1)


###############################################################################
#


def test_idrisi_4(tmp_path):

    shutil.copy("data/rgbsmall.tif", tmp_path)

    tst = gdaltest.GDALTest(
        "RST", tmp_path / "rgbsmall.tif", 2, 21053, filename_absolute=True
    )

    tst.testCreateCopy(
        check_gt=1, check_srs=1, new_filename=tmp_path / "rgbsmall_cc.rst", vsimem=1
    )
