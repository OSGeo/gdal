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
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["convert"]


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
                "--of=MEM",
                "--output=empty",
                "--layer",
                "invalid",
            ]
        )


def test_gdalalg_vector_convert_error_output_not_set():
    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"

    # Make it such that the "output" argument is set, but to a unset GDALArgDatasetValue
    convert["output"] = convert["output"]

    with pytest.raises(
        Exception,
        match="convert: Argument 'output' has no dataset object or dataset name",
    ):
        convert.Run()


@pytest.mark.require_driver("GeoJSON")
def test_gdalalg_vector_convert_vsistdout(tmp_vsimem):
    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"
    convert["output"] = f"/vsistdout_redirect/{tmp_vsimem}/tmp.json"
    convert["output-format"] = "GeoJSON"
    assert convert.Run()
    assert convert.Finalize()
    assert gdal.OpenEx(f"{tmp_vsimem}/tmp.json") is not None


@pytest.mark.require_driver("OpenFileGDB")
def test_gdalalg_vector_convert_overwrite_fgdb(tmp_vsimem):

    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"
    convert["output"] = tmp_vsimem / "out.gdb"
    convert["output-format"] = "OpenFileGDB"
    convert["layer-creation-option"] = {
        "TARGET_ARCGIS_VERSION": "ARCGIS_PRO_3_2_OR_LATER"
    }
    assert convert.Run()
    assert convert.Finalize()

    gdal.FileFromMemBuffer(tmp_vsimem / "out.gdb" / "new_file.txt", "foo")
    assert gdal.VSIStatL(tmp_vsimem / "out.gdb" / "new_file.txt") is not None

    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"
    convert["output"] = tmp_vsimem / "out.gdb"
    convert["output-format"] = "OpenFileGDB"
    convert["overwrite"] = True
    convert["layer-creation-option"] = {
        "TARGET_ARCGIS_VERSION": "ARCGIS_PRO_3_2_OR_LATER"
    }
    assert convert.Run()
    assert convert.Finalize()

    assert gdal.VSIStatL(tmp_vsimem / "out.gdb" / "new_file.txt") is None


@pytest.mark.require_driver("OpenFileGDB")
def test_gdalalg_vector_convert_overwrite_non_dataset_directory(tmp_vsimem):

    gdal.FileFromMemBuffer(tmp_vsimem / "out" / "foo", "bar")

    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"
    convert["output"] = tmp_vsimem / "out"
    convert["output-format"] = "OpenFileGDB"
    convert["overwrite"] = True
    with pytest.raises(
        Exception,
        match="already exists, but is not recognized as a valid GDAL dataset. Please manually delete it before retrying",
    ):
        convert.Run()


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_convert_overwrite_non_dataset_file(tmp_vsimem):

    gdal.FileFromMemBuffer(tmp_vsimem / "out.gpkg", "bar")

    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"
    convert["output"] = tmp_vsimem / "out.gpkg"
    convert["output-format"] = "GPKG"
    convert["overwrite"] = True
    assert convert.Run()
