#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector convert' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_convert_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    vector = reg.InstantiateAlg("vector")
    return vector.InstantiateSubAlgorithm("convert")


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_convert_base(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(["../ogr/data/poly.shp", out_filename])

    with gdal.OpenEx(out_filename, gdal.OF_UPDATE) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10
        for i in range(10):
            ds.GetLayer(0).DeleteFeature(i + 1)

    convert = get_convert_alg()
    with pytest.raises(
        Exception, match="already exists. Specify the --overwrite option"
    ):
        convert.ParseRunAndFinalize(["../ogr/data/poly.shp", out_filename])

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 0

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        ["--overwrite", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        ["--append", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 20

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        ["--update", "--nln", "layer2", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayerByName("poly").GetFeatureCount() == 20
        assert ds.GetLayerByName("layer2").GetFeatureCount() == 10

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        [
            "--of",
            "GPKG",
            "--overwrite-layer",
            "--nln",
            "poly",
            "../ogr/data/poly.shp",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayerByName("poly").GetFeatureCount() == 10
        assert ds.GetLayerByName("layer2").GetFeatureCount() == 10


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_convert_dsco(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        ["../ogr/data/poly.shp", out_filename, "--co", "ADD_GPKG_OGR_CONTENTS=NO"]
    )

    with gdal.OpenEx(out_filename) as ds:
        with ds.ExecuteSQL(
            "SELECT * FROM sqlite_master WHERE name = 'gpkg_ogr_contents'"
        ) as lyr:
            assert lyr.GetFeatureCount() == 0


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_convert_lco(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        ["../ogr/data/poly.shp", out_filename, "--lco", "FID=my_fid"]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFIDColumn() == "my_fid"


def test_gdalalg_vector_convert_progress(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        ["../ogr/data/poly.shp", out_filename], my_progress
    )

    assert last_pct[0] == 1.0

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_wrong_layer_name(tmp_vsimem):

    convert = get_convert_alg()
    with pytest.raises(Exception, match="Couldn't fetch requested layer"):
        convert.ParseRunAndFinalize(
            [
                "../ogr/data/poly.shp",
                "--of=Memory",
                "--output=empty",
                "--layer",
                "invalid",
            ]
        )


def test_gdalalg_vector_convert_error_output_not_set():
    convert = get_convert_alg()
    convert.GetArg("input").Get().SetName("../ogr/data/poly.shp")
    convert.GetArg("output").Set(convert.GetArg("output").Get())
    with pytest.raises(
        Exception,
        match="convert: Argument 'output' has no dataset object or dataset name",
    ):
        convert.Run()
