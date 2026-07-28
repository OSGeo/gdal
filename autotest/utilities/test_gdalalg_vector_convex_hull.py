#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector convex-hull' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2026, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_geos


@pytest.fixture()
def alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["convex-hull"]


@pytest.mark.parametrize(
    "input_wkt,output_wkt",
    [
        pytest.param("POINT (0 0)", "POINT (0 0)", id="single point"),
        pytest.param("MULTIPOINT (0 1, 3 4)", "LINESTRING (0 1, 3 4)", id="two points"),
        pytest.param(
            "MULTIPOINT (0 1, 8 2, 3 4, 3 2)",
            "POLYGON ((0 1, 3 4, 8 2, 0 1))",
            id="four points",
        ),
    ],
)
@pytest.mark.require_geos()
def test_gdalalg_vector_convex_hull(alg, input_wkt, output_wkt):

    src_ds = gdaltest.wkt_ds([input_wkt], epsg=32631)

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_lyr.GetLayerDefn().IsSame(out_f.GetDefnRef())
    assert out_lyr.GetGeomType() == ogr.wkbUnknown

    ogrtest.check_feature_geometry(out_f, output_wkt)
    assert out_f.GetGeometryRef().GetSpatialReference().GetAuthorityCode() == "32631"
    out_f = out_lyr.GetNextFeature()
    assert out_f is None
