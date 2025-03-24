#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  National Resources Canada - Vertical Datum Transformation
# Purpose:  Test read/write functionality for BYN driver.
# Author:   Ivan Lucena, ivan.lucena@outlook.com
#
###############################################################################
# Copyright (c) 2018, Ivan Lucena
# Copyright (c) 2018, Even Rouault
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("BYN")


###############################################################################
# Read test of byte file.


def test_byn_1():

    tst = gdaltest.GDALTest("BYN", "byn/cgg2013ai08_reduced.byn", 1, 64764)
    return tst.testOpen()


###############################################################################
#


def test_byn_invalid_header_bytes():

    tst = gdaltest.GDALTest("BYN", "byn/test_invalid_header_bytes.byn", 1, 64764)
    return tst.testOpen()
