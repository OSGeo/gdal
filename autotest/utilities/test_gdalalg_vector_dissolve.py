#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector dissolve' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2025, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, ogr


@pytest.fixture()
def alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["dissolve"]


pytestmark = pytest.mark.require_geos


@pytest.mark.parametrize(
    "wkt_in,wkt_out",
    [
        ["MULTIPOINT (3 3, 4 4, 3 3)", "MULTIPOINT (3 3, 4 4)"],
        [
            "MULTIPOLYGON (((0 0, 1 0, 1 1, 0 0)), ((0 0, 1 1, 0 1, 0 0)))",
            "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
        ],
        [
            "MULTILINESTRING ((0 0, 1 1), (1 1, 2 2), (1 0, 0 1))",
            "MULTILINESTRING ((0 0, 0.5 0.5), (0.5 0.5, 1 1, 2 2), (1 0, 0.5 0.5), (0.5 0.5, 0 1))",
        ],
        [
            "GEOMETRYCOLLECTION (MULTILINESTRING ((0 0, 1 1), (2 2, 1 1)), POINT (3 7))",
            "GEOMETRYCOLLECTION (LINESTRING (0 0, 1 1, 2 2), POINT (3 7))",
        ],
    ],
)
def test_gdalalg_vector_dissolve(alg, wkt_in, wkt_out):

    if type(wkt_in) is str:
        wkt_in = [wkt_in]
    if type(wkt_out) is str:
        wkt_out = [wkt_out]

    alg["input"] = gdaltest.wkt_ds(wkt_in)
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    dst_ds = alg["output"].GetDataset()
    dst_lyr = dst_ds.GetLayer(0)

    assert dst_lyr.GetFeatureCount() == len(wkt_out)

    for f, expected_wkt in zip(dst_lyr, wkt_out):
        actual = f.GetGeometryRef().Normalize()
        expected = ogr.CreateGeometryFromWkt(expected_wkt).Normalize()

        assert actual.Equals(expected)
