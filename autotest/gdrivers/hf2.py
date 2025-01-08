#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for HF2 driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest

###############################################################################
# Test CreateCopy() of byte.tif


def test_hf2_1():

    tst = gdaltest.GDALTest("HF2", "byte.tif", 1, 4672)
    tst.testCreateCopy(
        vsimem=1,
        check_gt=(-67.00041667, 0.00083333, 0.0, 50.000416667, 0.0, -0.00083333),
    )


###############################################################################
# Test CreateCopy() of byte.tif with options


def test_hf2_2():

    tst = gdaltest.GDALTest(
        "HF2", "byte.tif", 1, 4672, options=["COMPRESS=YES", "BLOCKSIZE=10"]
    )
    tst.testCreateCopy(new_filename="tmp/hf2_2.hfz")
    try:
        os.remove("tmp/hf2_2.hfz.properties")
    except OSError:
        pass


###############################################################################
# Test CreateCopy() of float.img


def test_hf2_3():

    tst = gdaltest.GDALTest("HF2", "hfa/float.img", 1, 23529)
    tst.testCreateCopy(check_minmax=0)


###############################################################################
# Test CreateCopy() of n43.dt0


def test_hf2_4():

    tst = gdaltest.GDALTest("HF2", "n43.dt0", 1, 49187)
    tst.testCreateCopy()


###############################################################################
# Cleanup


def test_hf2_cleanup():

    pass
