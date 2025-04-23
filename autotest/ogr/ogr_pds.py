#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR PDS driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import ogrtest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("OGR_PDS")

###############################################################################
# Basic test


def test_ogr_pds_1():

    ds = ogr.Open("data/pds/ap01578l.lbl")
    assert ds is not None, "cannot open dataset"

    lyr = ds.GetLayerByName("RAMAPPING")
    assert lyr is not None, "cannot find layer"

    assert lyr.GetFeatureCount() == 74786, "did not get expected feature count"

    with gdal.quiet_errors():
        feat = lyr.GetNextFeature()
    if feat.GetField("NOISE_COUNTS_1") != 96:
        feat.DumpReadable()
        pytest.fail()

    ogrtest.check_feature_geometry(
        feat, "POINT (146.1325 -55.648)", max_error=0.000000001
    )

    with gdal.quiet_errors():
        feat = lyr.GetFeature(1)
    if feat.GetField("MARS_RADIUS") != 3385310.2:
        feat.DumpReadable()
        pytest.fail()


###############################################################################
# Read IEEE_FLOAT columns (see https://github.com/OSGeo/gdal/issues/570)


def test_ogr_pds_2():

    ds = ogr.Open("data/pds/virsvd_orb_11187_050618.lbl")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f["INCIDENCE_ANGLE"] != pytest.approx(3.56775538, abs=1e-7) or f[
        "TEMP_2"
    ] != pytest.approx(28.1240005493164, abs=1e-7):
        f.DumpReadable()
        pytest.fail()
