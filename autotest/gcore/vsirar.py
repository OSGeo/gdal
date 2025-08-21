#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsirar/
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import pytest

from osgeo import gdal, ogr

###############################################################################
# Test /vsirar/


@pytest.mark.skipif(
    "SKIP_VSIRAR" in os.environ,
    reason="test skipped due to random crashes",
)
def test_vsirar_basic():

    content = gdal.ReadDir("/vsirar/../ogr/data/poly.shp.rar")
    if content is None:
        pytest.skip("RAR support not enabled")

    assert set(content) == set(["poly.PRJ", "poly.dbf", "poly.shp", "poly.shx"])

    assert gdal.ReadDir("/vsirar/i_do/not/exist") is None
    assert (
        gdal.VSIFOpenL("/vsirar/../ogr/data/poly.shp.rar/i_do_not_exist", "rb") is None
    )

    f = gdal.VSIFOpenL("/vsirar/../ogr/data/poly.shp.rar/poly.PRJ", "rb")
    assert f is not None
    try:
        gdal.VSIFSeekL(f, 0, os.SEEK_END)
        assert gdal.VSIFTellL(f) == 415
        assert gdal.VSIFEofL(f) == 0
        assert len(gdal.VSIFReadL(1, 1, f)) == 0
        assert gdal.VSIFEofL(f) == 1
        assert gdal.VSIFTellL(f) == 415
    finally:
        gdal.VSIFCloseL(f)


###############################################################################
# Test /vsirar/


@pytest.mark.skipif(
    "SKIP_VSIRAR" in os.environ,
    reason="test skipped due to random crashes",
)
def test_vsirar_ogr():

    content = gdal.ReadDir("/vsirar/../ogr/data/poly.shp.rar")
    if content is None:
        pytest.skip("RAR support not enabled")

    ds = ogr.Open("/vsirar/../ogr/data/poly.shp.rar")
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10


###############################################################################
# Test /vsirar/


@pytest.mark.skipif(
    "SKIP_VSIRAR" in os.environ,
    reason="test skipped due to random crashes",
)
def test_vsirar_gdal():

    content = gdal.ReadDir("/vsirar/data/byte.tif.rar")
    if content is None:
        pytest.skip("RAR support not enabled")

    ds = gdal.Open("/vsirar/data/byte.tif.rar")
    assert ds.GetRasterBand(1).Checksum() == 4672
