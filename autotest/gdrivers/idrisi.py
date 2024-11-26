#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for RST/Idrisi driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
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
