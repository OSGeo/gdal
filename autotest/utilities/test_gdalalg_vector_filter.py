#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector filter' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_filter_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["filter"]


def test_gdalalg_vector_filter_no_filter(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    assert filter_alg.ParseCommandLineArguments(["../ogr/data/poly.shp", out_filename])
    assert filter_alg.Run()
    ds = filter_alg["output"].GetDataset()
    assert ds.GetLayer(0).GetFeatureCount() == 10
    assert filter_alg.Finalize()
    ds = None

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_filter_bbox(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    assert filter_alg.ParseRunAndFinalize(
        ["--bbox=479867,4762909,479868,4762910", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 1


def test_gdalalg_vector_filter_where_discard_all(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    assert filter_alg.ParseRunAndFinalize(
        ["--where=0=1", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 0


def test_gdalalg_vector_filter_where_accept_all(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    assert filter_alg.ParseRunAndFinalize(
        ["--where=1=1", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_filter_where_error(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    with pytest.raises(
        Exception, match='"invalid" not recognised as an available field.'
    ):
        filter_alg.ParseRunAndFinalize(
            ["--where=invalid", "../ogr/data/poly.shp", out_filename]
        )
