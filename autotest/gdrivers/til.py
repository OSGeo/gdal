#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test TIL driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("TIL")

###############################################################################
# Test a fake TIL dataset


def test_til_1():

    tst = gdaltest.GDALTest("TIL", "til/testtil.til", 1, 4672)
    tst.testOpen()


###############################################################################
# Check GetFileList() result (#4018) & IMD


def test_til_2():

    ds = gdal.Open("data/til/testtil.til")
    filelist = ds.GetFileList()

    assert len(filelist) == 3, "did not get expected file list."

    md = ds.GetMetadata("IMAGERY")
    assert "SATELLITEID" in md, "SATELLITEID not present in IMAGERY Domain"
    assert "CLOUDCOVER" in md, "CLOUDCOVER not present in IMAGERY Domain"
    assert (
        "ACQUISITIONDATETIME" in md
    ), "ACQUISITIONDATETIME not present in IMAGERY Domain"

    ds = None

    assert not os.path.exists("data/til/testtil.til.aux.xml")


###############################################################################
# Check GetFileList() & XML


def test_til_3():

    ds = gdal.Open("data/til/testtil2.til")
    filelist = ds.GetFileList()

    assert len(filelist) == 3, "did not get expected file list."

    md = ds.GetMetadata("IMAGERY")
    assert "SATELLITEID" in md, "SATELLITEID not present in IMAGERY Domain"
    assert "CLOUDCOVER" in md, "CLOUDCOVER not present in IMAGERY Domain"
    assert (
        "ACQUISITIONDATETIME" in md
    ), "ACQUISITIONDATETIME not present in IMAGERY Domain"

    ds = None

    assert not os.path.exists("data/til/testtil.til.aux.xml")
