#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  RIK Testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("RIK")

###############################################################################
# Test a RIK map
# Data downloaded from : http://www.lantmateriet.se/upload/filer/kartor/programvaror/sverige500_swe99.zip


def test_rik_online_1():

    gdaltest.download_or_skip(
        "http://www.lantmateriet.se/upload/filer/kartor/programvaror/sverige500_swe99.zip",
        "sverige500_swe99.zip",
    )

    try:
        os.stat("tmp/cache/sverige500_swe99.rik")
        file_to_test = "tmp/cache/sverige500_swe99.rik"
    except OSError:
        try:
            print("Uncompressing ZIP file...")
            import zipfile

            zfobj = zipfile.ZipFile("tmp/cache/sverige500_swe99.zip")
            outfile = open("tmp/cache/sverige500_swe99.rik", "wb")
            outfile.write(zfobj.read("sverige500_swe99.rik"))
            outfile.close()
            file_to_test = "tmp/cache/sverige500_swe99.rik"
        except OSError:
            pytest.skip()

    tst = gdaltest.GDALTest("RIK", file_to_test, 1, 17162, filename_absolute=1)
    tst.testOpen()


###############################################################################
# Test a LZW compressed RIK dataset


def test_rik_online_2():

    gdaltest.download_or_skip(
        "http://trac.osgeo.org/gdal/raw-attachment/ticket/3674/ab-del.rik", "ab-del.rik"
    )

    tst = gdaltest.GDALTest(
        "RIK", "tmp/cache/ab-del.rik", 1, 44974, filename_absolute=1
    )
    tst.testOpen()
