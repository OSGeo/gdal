#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for SRP driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2012-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest

from osgeo import gdal, osr

pytestmark = pytest.mark.require_driver("SRP")


@pytest.fixture(scope="module", autouse=True)
def setup_and_cleanup():

    yield

    try:
        os.unlink("data/srp/USRP_PCB0/TRANSH01.THF.aux.xml")
    except OSError:
        pass


###############################################################################
# Read USRP dataset with PCB=0


@pytest.mark.parametrize("pcb", (0, 4, 8))
def test_srp_1(pcb):

    filename = f"srp/USRP_PCB{pcb}/FKUSRP01.IMG"

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32600 + 17)

    tst = gdaltest.GDALTest("SRP", filename, 1, 24576)
    tst.testOpen(
        check_prj=srs.ExportToWkt(), check_gt=(500000.0, 5.0, 0.0, 5000000.0, 0.0, -5.0)
    )

    ds = gdal.Open("data/" + filename)
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex

    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct.GetCount() == 4

    assert ct.GetColorEntry(0) == (0, 0, 0, 255)

    assert ct.GetColorEntry(1) == (255, 0, 0, 255)

    expected_md = [
        "SRP_CLASSIFICATION=U",
        "SRP_CREATIONDATE=20120505",
        "SRP_EDN=0",
        "SRP_NAM=FKUSRP",
        "SRP_PRODUCT=USRP",
        "SRP_REVISIONDATE=20120505",
        "SRP_SCA=50000",
        "SRP_ZNA=17",
        "SRP_PSP=100.0",
    ]

    got_md = ds.GetMetadata()
    for md in expected_md:
        key, value = md.split("=")
        assert key in got_md and got_md[key] == value, "did not find %s" % md


###############################################################################
# Read from TRANSH01.THF file.


def test_srp_4():

    tst = gdaltest.GDALTest("SRP", "srp/USRP_PCB0/TRANSH01.THF", 1, 24576)
    tst.testOpen()


###############################################################################
# Read from TRANSH01.THF file (without "optimization" for single GEN in THF)


def test_srp_5():

    with gdal.config_option("SRP_SINGLE_GEN_IN_THF_AS_DATASET", "FALSE"):
        ds = gdal.Open("data/srp/USRP_PCB0/TRANSH01.THF")
    subdatasets = ds.GetMetadata("SUBDATASETS")
    assert (
        subdatasets["SUBDATASET_1_NAME"].replace("\\", "/")
        == "SRP:data/srp/USRP_PCB0/FKUSRP01.GEN,data/srp/USRP_PCB0/FKUSRP01.IMG"
    )
    assert (
        subdatasets["SUBDATASET_1_DESC"].replace("\\", "/")
        == "SRP:data/srp/USRP_PCB0/FKUSRP01.GEN,data/srp/USRP_PCB0/FKUSRP01.IMG"
    )

    expected_md = [
        "SRP_CLASSIFICATION=U",
        "SRP_CREATIONDATE=20120505",
        "SRP_EDN=1",
        "SRP_VOO=           ",
    ]

    got_md = ds.GetMetadata()
    for md in expected_md:
        key, value = md.split("=")
        assert key in got_md and got_md[key] == value, "did not find %s" % md


###############################################################################
# Read with subdataset syntax


def test_srp_6():

    tst = gdaltest.GDALTest(
        "SRP",
        "SRP:data/srp/USRP_PCB4/FKUSRP01.GEN,data/srp/USRP_PCB4/FKUSRP01.IMG",
        1,
        24576,
        filename_absolute=1,
    )
    tst.testOpen()
