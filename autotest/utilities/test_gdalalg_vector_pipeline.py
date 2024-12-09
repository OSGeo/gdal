#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector pipeline' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json

import pytest

from osgeo import gdal


def get_pipeline_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    vector = reg.InstantiateAlg("vector")
    return vector.InstantiateSubAlgorithm("pipeline")


def test_gdalalg_vector_pipeline_read_and_write(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        ["read", "../ogr/data/poly.shp", "!", "write", out_filename], my_progress
    )
    assert last_pct[0] == 1.0

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10

    with pytest.raises(Exception, match="can only be called once per instance"):
        pipeline.ParseRunAndFinalize(
            ["read", "../ogr/data/poly.shp", "!", "write", out_filename], my_progress
        )


def test_gdalalg_vector_pipeline_pipeline_arg(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    # Also test extra pipes / exclamation mark
    assert pipeline.ParseRunAndFinalize(
        ["--pipeline", f"! read ../ogr/data/poly.shp | | write {out_filename} !"]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_pipeline_as_api(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    pipeline.GetArg("pipeline").Set(f"read ../ogr/data/poly.shp ! write {out_filename}")
    assert pipeline.Run()
    ds = pipeline.GetArg("output").Get().GetDataset()
    assert ds.GetLayer(0).GetFeatureCount() == 10
    assert pipeline.Finalize()
    ds = None

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_pipeline_input_through_api(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    pipeline.GetArg("input").Get().SetDataset(gdal.OpenEx("../ogr/data/poly.shp"))
    pipeline.GetArg("pipeline").Set(f"read ! write {out_filename}")
    assert pipeline.Run()
    assert pipeline.Finalize()

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_pipeline_input_through_api_run_twice(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    pipeline.GetArg("input").Get().SetDataset(gdal.OpenEx("../ogr/data/poly.shp"))
    pipeline.GetArg("pipeline").Set(f"read ! write {out_filename}")
    assert pipeline.Run()
    with pytest.raises(
        Exception, match=r"pipeline: Step nr 0 \(read\) has already an output dataset"
    ):
        pipeline.Run()


def test_gdalalg_vector_pipeline_output_through_api(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    pipeline.GetArg("output").Get().SetName(out_filename)
    pipeline.GetArg("pipeline").Set("read ../ogr/data/poly.shp ! write")
    assert pipeline.Run()
    assert pipeline.Finalize()

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_pipeline_as_api_error():

    pipeline = get_pipeline_alg()
    pipeline.GetArg("pipeline").Set("read")
    with pytest.raises(Exception, match="pipeline: At least 2 steps must be provided"):
        pipeline.Run()


def test_gdalalg_vector_pipeline_usage_as_json():

    pipeline = get_pipeline_alg()
    j = json.loads(pipeline.GetUsageAsJSON())
    assert "pipeline_algorithms" in j


def test_gdalalg_vector_pipeline_quoted(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [f"read ../ogr/data/poly.shp ! write {out_filename}"]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_pipeline_progress(tmp_path):

    out_filename = str(tmp_path / "out.shp")
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} vector pipeline --progress read ../ogr/data/poly.shp ! write {out_filename}"
    )
    assert out.startswith(
        "0...10...20...30...40...50...60...70...80...90...100 - done."
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_easter_egg(tmp_path):

    out_filename = str(tmp_path / "out.shp")
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    gdaltest.runexternal(
        f"{gdal_path} vector +gdal=pipeline +step +gdal=read +input=../ogr/data/poly.shp +step +write +output={out_filename}"
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_pipeline_usage_as_json_bis():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} vector pipeline --json-usage"
    )
    j = json.loads(out)
    assert "pipeline_algorithms" in j


def test_gdalalg_vector_pipeline_missing_at_run():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: 'pipeline' argument not set"):
        pipeline.Run()


def test_gdalalg_vector_pipeline_empty_args():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: At least 2 steps must be provided"):
        pipeline.ParseRunAndFinalize([])


def test_gdalalg_vector_pipeline_unknow_step():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: unknown step name: unknown_step"):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../ogr/data/poly.shp",
                "!",
                "unknown_step",
                "!",
                "write",
                "/vsimem/foo.shp",
            ]
        )


def test_gdalalg_vector_pipeline_read_read():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: Last step should be 'write'"):
        pipeline.ParseRunAndFinalize(
            ["read", "../ogr/data/poly.shp", "!", "read", "../ogr/data/poly.shp"]
        )


def test_gdalalg_vector_pipeline_write_write():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: First step should be 'read'"):
        pipeline.ParseRunAndFinalize(
            ["write", "/vsimem/poly.shp", "!", "write", "/vsimem/poly.shp"]
        )


def test_gdalalg_vector_pipeline_read_write_write():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: Only last step can be 'write'"):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../ogr/data/poly.shp",
                "!",
                "write",
                "/vsimem/poly.shp",
                "!",
                "write",
                "/vsimem/poly.shp",
            ]
        )


def test_gdalalg_vector_pipeline_read_read_write():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: Only first step can be 'read'"):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../ogr/data/poly.shp",
                "!",
                "read",
                "../ogr/data/poly.shp",
                "!",
                "write",
                "/vsimem/poly.shp",
            ]
        )


def test_gdalalg_vector_pipeline_invalid_step_during_parsing(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception, match="write: Long name option '--invalid' is unknown"
    ):
        pipeline.ParseRunAndFinalize(
            ["read", "../ogr/data/poly.shp", "!", "write", "--invalid", out_filename]
        )


def test_gdalalg_vector_pipeline_invalid_step_during_validation(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="read: Positional arguments starting at 'INPUT' have not been specified",
    ):
        pipeline.ParseRunAndFinalize(["read", "!", "write", "--invalid", out_filename])


def test_gdalalg_vector_pipeline_read_layername(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        ["read", "--layer", "poly", "../ogr/data/poly.shp", "!", "write", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_pipeline_read_layername_error(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="read: Cannot find source layer 'invalid'"):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "--layer",
                "invalid",
                "../ogr/data/poly.shp",
                "!",
                "write",
                out_filename,
            ]
        )


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_pipeline_write_options(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        ["read", "../ogr/data/poly.shp", "!", "write", "--of=GPKG", out_filename]
    )

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="already exists. Specify the --overwrite option to overwrite it",
    ):
        assert pipeline.ParseRunAndFinalize(
            ["read", "../ogr/data/poly.shp", "!", "write", out_filename]
        )

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        ["read", "../ogr/data/poly.shp", "!", "write", "--append", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 20

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../ogr/data/poly.shp",
            "!",
            "write",
            "--update",
            "--output-layer=layer2",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer("poly").GetFeatureCount() == 20
        assert ds.GetLayer("layer2").GetFeatureCount() == 10

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../ogr/data/poly.shp",
            "!",
            "write",
            "--overwrite-layer",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer("poly").GetFeatureCount() == 10
        assert ds.GetLayer("layer2").GetFeatureCount() == 10

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        ["read", "../ogr/data/poly.shp", "!", "write", "--overwrite", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayerCount() == 1
        assert ds.GetLayer(0).GetFeatureCount() == 10


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_pipeline_write_dsco(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../ogr/data/poly.shp",
            "!",
            "write",
            out_filename,
            "--co",
            "ADD_GPKG_OGR_CONTENTS=NO",
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        with ds.ExecuteSQL(
            "SELECT * FROM sqlite_master WHERE name = 'gpkg_ogr_contents'"
        ) as lyr:
            assert lyr.GetFeatureCount() == 0


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_pipeline_write_lco(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../ogr/data/poly.shp",
            "!",
            "write",
            out_filename,
            "--lco",
            "FID=my_fid",
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFIDColumn() == "my_fid"


def test_gdalalg_vector_pipeline_filter_no_arg(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        ["read", "../ogr/data/poly.shp", "!", "filter", "!", "write", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_pipeline_filter_bbox(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="Value of 'bbox' should be xmin,ymin,xmax,ymax with xmin <= xmax and ymin <= ymax",
    ):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../ogr/data/poly.shp",
                "!",
                "filter",
                "--bbox=2,49,1,50",
                "!",
                "write",
                out_filename,
            ]
        )

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="Value of 'bbox' should be xmin,ymin,xmax,ymax with xmin <= xmax and ymin <= ymax",
    ):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../ogr/data/poly.shp",
                "!",
                "filter",
                "--bbox=2,49,3,48",
                "!",
                "write",
                out_filename,
            ]
        )

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../ogr/data/poly.shp",
            "!",
            "filter",
            "--bbox=0,0,0,0",
            "!",
            "write",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 0

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../ogr/data/poly.shp",
            "!",
            "filter",
            "--bbox=479867,4762909,479868,4762910",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 1


def test_gdalalg_vector_pipeline_reproject_no_arg(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="reproject: Required argument 'dst-crs' has not been specified",
    ):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../ogr/data/poly.shp",
                "!",
                "reproject",
                "!",
                "write",
                out_filename,
            ]
        )


def test_gdalalg_vector_pipeline_reproject_invalid_src_crs(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="reproject: Invalid value for '--src-crs'",
    ):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../ogr/data/poly.shp",
                "!",
                "reproject",
                "--src-crs=invalid",
                "--dst-crs=EPSG:4326",
                "!",
                "write",
                out_filename,
            ]
        )


def test_gdalalg_vector_pipeline_reproject_invalid_dst_crs(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="reproject: Invalid value for '--dst-crs'",
    ):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../ogr/data/poly.shp",
                "!",
                "reproject",
                "--dst-crs=invalid",
                "!",
                "write",
                out_filename,
            ]
        )


def test_gdalalg_vector_pipeline_reproject_missing_layer_crs(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    mem_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    mem_ds.CreateLayer("layer")
    pipeline.GetArg("input").Get().SetDataset(mem_ds)
    pipeline.GetArg("pipeline").Set(
        f"read ! reproject --dst-crs=EPSG:4326 ! write {out_filename}"
    )
    with pytest.raises(
        Exception, match="reproject: Layer 'layer' has no spatial reference system"
    ):
        pipeline.Run()


def test_gdalalg_vector_pipeline_reproject_nominal(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../ogr/data/poly.shp",
            "!",
            "reproject",
            "--dst-crs=EPSG:4326",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetSpatialRef().GetAuthorityCode(None) == "4326"
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_pipeline_reproject_with_src_crs(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../ogr/data/poly.shp",
            "!",
            "reproject",
            "--src-crs=EPSG:32631",
            "--dst-crs=EPSG:4326",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef().GetEnvelope() == pytest.approx(
            (2.750130423614134, 2.759262932833617, 43.0361359661472, 43.0429263707128)
        )
