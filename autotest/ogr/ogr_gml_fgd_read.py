#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GML Reading Driver for Japanese FGD GML v4 testing.
# Author:   Hiroshi Miura <miurahr@linux.com>
#
###############################################################################
# Copyright (c) 2017, Hiroshi Miura <miurahr@linux.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import ogrtest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("GML")

###############################################################################
# Test reading Japanese FGD GML (v4) files
###############################################################################

_fgd_dir = "data/gml_jpfgd/"


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():

    # open FGD GML file
    ds = ogr.Open(_fgd_dir + "ElevPt.xml")

    if ds is None:
        if gdal.GetLastErrorMsg().find("Xerces") != -1:
            pytest.skip()
        pytest.fail("failed to open test file.")


###############################################################################
# Test reading Japanese FGD GML (v4) ElevPt file


def test_ogr_gml_fgd_1():

    # open FGD GML file
    ds = ogr.Open(_fgd_dir + "ElevPt.xml")

    # check number of layers
    assert ds.GetLayerCount() == 1, "Wrong layer count"

    lyr = ds.GetLayer(0)

    # check the SRS
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(6668)  # JGD2011
    assert sr.IsSame(
        lyr.GetSpatialRef(), options=["IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES"]
    ), "Wrong SRS"

    # check the first feature
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT (133.123456789 34.123456789)")

    assert feat.GetField("devDate") == "2015-01-07", "Wrong attribute value"


###############################################################################
# Test reading Japanese FGD GML (v4) BldA file


def test_ogr_gml_fgd_2():

    # open FGD GML file
    ds = ogr.Open(_fgd_dir + "BldA.xml")

    # check number of layers
    assert ds.GetLayerCount() == 1, "Wrong layer count"

    lyr = ds.GetLayer(0)

    # check the SRS
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(6668)  # JGD2011
    assert sr.IsSame(
        lyr.GetSpatialRef(), options=["IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES"]
    ), "Wrong SRS"

    wkt = "POLYGON ((139.718509733734 35.6952171397133,139.718444177734 35.6953121947133,139.718496754142 35.6953498949667,139.718550483734 35.6952359447133,139.718509733734 35.6952171397133))"

    # check the first feature
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, wkt)

    assert feat.GetField("devDate") == "2017-03-07", "Wrong attribute value"


###############################################################################
# Test reading Japanese FGD GML (v4) with JGD2024


def test_ogr_gml_fgd_jgd2024():

    # open FGD GML file
    ds = ogr.Open(_fgd_dir + "ElevPt_JGD2024.xml")

    # check number of layers
    assert ds.GetLayerCount() == 1, "Wrong layer count"

    lyr = ds.GetLayer(0)

    assert lyr.GetSpatialRef().IsGeographic()
    assert lyr.GetSpatialRef().GetName() == "JGD2024"

    # check the first feature
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT (133.123456789 34.123456789)")
