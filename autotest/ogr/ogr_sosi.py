#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR SOSI driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import shutil

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("SOSI")

###############################################################################


def test_ogr_sosi_1():

    gdaltest.download_or_skip(
        "http://trac.osgeo.org/gdal/raw-attachment/ticket/3638/20BygnAnlegg.SOS",
        "20BygnAnlegg.SOS",
    )

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro tmp/cache/20BygnAnlegg.SOS"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# test using no appendFieldsMap


def test_ogr_sosi_2():

    try:
        ds = gdal.OpenEx("data/sosi/test_duplicate_fields.sos", open_options=[])
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 17
        lyr = ds.GetLayer(1)
        assert lyr.GetFeatureCount() == 1
        f = lyr.GetNextFeature()
        assert f["REINBEITEBRUKERID"] == "YD"
        ds.Close()
    finally:
        shutil.rmtree("data/sosi/test_duplicate_fields")


###############################################################################
# test using simple open_options appendFieldsMap


def test_ogr_sosi_3():

    try:
        ds = gdal.OpenEx(
            "data/sosi/test_duplicate_fields.sos",
            open_options=["appendFieldsMap=BEITEBRUKERID&OPPHAV"],
        )
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 17
        lyr = ds.GetLayer(1)
        assert lyr.GetFeatureCount() == 1
        f = lyr.GetNextFeature()
        assert f["REINBEITEBRUKERID"] == "YD,YG"
        ds.Close()
    finally:
        shutil.rmtree("data/sosi/test_duplicate_fields")


###############################################################################
# test using simple open_options appendFieldsMap with semicolumns


def test_ogr_sosi_4():

    try:
        ds = gdal.OpenEx(
            "data/sosi/test_duplicate_fields.sos",
            open_options=["appendFieldsMap=BEITEBRUKERID:;&OPPHAV:;"],
        )
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 17
        lyr = ds.GetLayer(1)
        assert lyr.GetFeatureCount() == 1
        f = lyr.GetNextFeature()
        assert f["REINBEITEBRUKERID"] == "YD;YG"
        ds.Close()
    finally:
        shutil.rmtree("data/sosi/test_duplicate_fields")
