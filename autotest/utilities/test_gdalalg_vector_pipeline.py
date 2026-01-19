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

import ogrtest
import pytest

from osgeo import gdal, ogr


def get_pipeline_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["pipeline"]


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


@pytest.mark.require_driver("OSM")
def test_gdalalg_vector_pipeline_read_osm():

    pipeline = get_pipeline_alg()
    assert pipeline.ParseCommandLineArguments(
        [
            "read",
            "../ogr/data/osm/test.pbf",
            "!",
            "write",
            "--of=stream",
            "streamed_file",
        ]
    )
    assert pipeline.Run()

    out_ds = pipeline["output"].GetDataset()
    assert out_ds.TestCapability(ogr.ODsCRandomLayerRead)
    assert out_ds.TestCapability("unknown") == 0

    expected = []
    src_ds = gdal.OpenEx("../ogr/data/osm/test.pbf")
    while True:
        f, _ = src_ds.GetNextFeature()
        if not f:
            break
        expected.append(str(f))

    got = []
    out_ds.ResetReading()
    while True:
        f, _ = out_ds.GetNextFeature()
        if not f:
            break
        got.append(str(f))

    assert expected == got


@pytest.mark.require_driver("OSM")
def test_gdalalg_vector_pipeline_read_osm_subset_of_layers():

    pipeline = get_pipeline_alg()
    assert pipeline.ParseCommandLineArguments(
        [
            "read",
            "../ogr/data/osm/test.pbf",
            "--layer=points,multipolygons",
            "!",
            "write",
            "--of=stream",
            "streamed_file",
        ]
    )
    assert pipeline.Run()

    out_ds = pipeline["output"].GetDataset()
    assert out_ds.TestCapability(ogr.ODsCRandomLayerRead)

    expected = []
    src_ds = gdal.OpenEx("../ogr/data/osm/test.pbf")
    while True:
        f, lyr = src_ds.GetNextFeature()
        if not f:
            break
        if lyr.GetName() in ["points", "multipolygons"]:
            expected.append(str(f))

    got = []
    out_ds.ResetReading()
    while True:
        f, _ = out_ds.GetNextFeature()
        if not f:
            break
        got.append(str(f))

    assert len(expected) == len(got)
    assert expected == got


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
    pipeline["pipeline"] = f"read ../ogr/data/poly.shp ! write {out_filename}"
    assert pipeline.Run()
    ds = pipeline["output"].GetDataset()
    assert ds.GetLayer(0).GetFeatureCount() == 10
    assert pipeline.Finalize()
    ds = None

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_pipeline_input_through_api(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    pipeline["input"] = gdal.OpenEx("../ogr/data/poly.shp")
    pipeline["pipeline"] = f"read ! write {out_filename}"
    assert pipeline.Run()
    assert pipeline.Finalize()

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_pipeline_input_through_api_run_twice(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    pipeline["input"] = gdal.OpenEx("../ogr/data/poly.shp")
    pipeline["pipeline"] = f"read ! write {out_filename}"
    assert pipeline.Run()
    with pytest.raises(
        Exception, match=r"pipeline: Step nr 0 \(read\) has already an output dataset"
    ):
        pipeline.Run()


def test_gdalalg_vector_pipeline_output_through_api(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    pipeline["output"] = out_filename
    pipeline["pipeline"] = "read ../ogr/data/poly.shp ! write"
    assert pipeline.Run()
    assert pipeline.Finalize()

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_pipeline_mutually_exclusive_args():

    with pytest.raises(
        Exception, match="clip: Argument 'like' is mutually exclusive with 'bbox'"
    ):
        gdal.Run(
            "vector pipeline",
            input="../ogr/data/poly.shp",
            output_format="MEM",
            output="",
            pipeline="read ! clip --bbox=1,2,3,4 --like=../ogr/data/poly.shp ! write",
        )


def test_gdalalg_vector_pipeline_usage_as_json():

    pipeline = get_pipeline_alg()
    j = json.loads(pipeline.GetUsageAsJSON())
    assert "pipeline_algorithms" in j


def test_gdalalg_vector_pipeline_help_doc():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(f"{gdal_path} vector pipeline --help-doc=main")

    assert "Usage: gdal vector pipeline [OPTIONS] <PIPELINE>" in out
    assert "<PIPELINE> is of the form:" in out

    out = gdaltest.runexternal(f"{gdal_path} vector pipeline --help-doc=edit")

    assert "* edit [OPTIONS]" in out

    out, _ = gdaltest.runexternal_out_and_err(
        f"{gdal_path} vector pipeline --help-doc=unknown"
    )

    assert "ERROR: unknown pipeline step 'unknown'" in out


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


def test_gdalalg_vector_easter_egg_failed():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} vector +gdal=pipeline +step +gdal=read +input=../ogr/data/poly.shp +step +gdal=unknown +step +write +output=/vsimem/out.shp"
    )

    assert "pipeline: unknown step name: unknown" in err


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
    with pytest.raises(
        Exception, match="pipeline: At least one step must be provided in a pipeline"
    ):
        pipeline.ParseRunAndFinalize([])


def test_gdalalg_vector_pipeline_unknown_step():

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
    with pytest.raises(
        Exception, match="pipeline: 'read' is only allowed as a first step"
    ):
        pipeline.ParseRunAndFinalize(
            ["read", "../ogr/data/poly.shp", "!", "read", "../ogr/data/poly.shp"]
        )


def test_gdalalg_vector_pipeline_read_read_several_input():

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="read: Positional values starting at '../ogr/data/poly.dbf' are not expected.",
    ):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../ogr/data/poly.shp",
                "../ogr/data/poly.dbf",
                "!",
                "write",
                "/vsimem/poly.shp",
            ]
        )

    pipeline = get_pipeline_alg()
    pipeline["input"] = ["../ogr/data/poly.shp", "../ogr/data/poly.dbf"]
    pipeline["output"] = "/vsimem/poly.shp"
    pipeline["pipeline"] = "read ! write"
    with pytest.raises(
        Exception,
        match="read: 2 values have been specified for argument 'input', whereas exactly 1 was expected.",
    ):
        pipeline.Run()


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
    with pytest.raises(Exception, match="write: Option '--invalid' is unknown"):
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
        match="already exists",
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
        match="reproject: Invalid value for 'src-crs' argument",
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
        match="reproject: Invalid value for 'dst-crs' argument",
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
    mem_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    mem_ds.CreateLayer("layer")
    pipeline["input"] = mem_ds
    pipeline[
        "pipeline"
    ] = f"read ! reproject --dst-crs=EPSG:4326 ! write {out_filename}"
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


def test_gdalalg_vector_pipeline_reproject_proj_string(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../ogr/data/poly.shp",
            "!",
            "reproject",
            "--src-crs=EPSG:32631",
            "--dst-crs",
            "+proj=laea +lon_0=147 +lat_0=-40 +datum=WGS84",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        lyr = ds.GetLayer(0)
        assert "Lambert Azimuthal Equal Area" in lyr.GetSpatialRef().ExportToWkt(
            ["FORMAT=WKT2"]
        ), lyr.GetSpatialRef().ExportToWkt(["FORMAT=WKT2"])


def test_gdalalg_vector_pipeline_set_type():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 0)"))
    src_lyr.CreateFeature(f)

    alg = get_pipeline_alg()

    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"

    assert alg.ParseCommandLineArguments(
        ["read", "!", "set-type", "--geometry-type=POINTZ", "!", "write"]
    )

    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetGeomType() == ogr.wkbPoint25D
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, "POINT Z (3 0 0)")


def test_gdalalg_vector_pipeline_help():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(f"{gdal_path} vector pipeline --help")
    assert out.startswith("Usage: gdal vector pipeline [OPTIONS] <PIPELINE>")
    assert "* read [OPTIONS] <INPUT>" in out
    assert "* write [OPTIONS] <OUTPUT>" in out

    out = gdaltest.runexternal(f"{gdal_path} vector pipeline --progress --help")
    assert out.startswith("Usage: gdal vector pipeline [OPTIONS] <PIPELINE>")
    assert "* read [OPTIONS] <INPUT>" in out
    assert "* write [OPTIONS] <OUTPUT>" in out

    out = gdaltest.runexternal(f"{gdal_path} vector pipeline read --help")
    assert out.startswith("Usage: read [OPTIONS] <INPUT>")

    out = gdaltest.runexternal(
        f"{gdal_path} vector pipeline read foo.shp ! select --help"
    )
    assert out.startswith("Usage: select [OPTIONS] <FIELDS>")


def test_gdalalg_vector_pipeline_skip_errors(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = src_ds.CreateLayer("test")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(1 2, 3 4)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(3 4)"))
    lyr.CreateFeature(f)

    alg = get_pipeline_alg()
    alg["input"] = src_ds
    alg["output"] = tmp_vsimem / "out.shp"

    assert alg.ParseCommandLineArguments(["read", "!", "write", "--skip-errors"])
    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    assert out_ds.GetLayer(0).GetFeatureCount() == 2


def test_gdalalg_vector_pipeline_info():

    with gdal.Run(
        "vector",
        "pipeline",
        pipeline="read ../ogr/data/poly.shp ! info",
    ) as alg:
        assert "layers" in alg.Output()


def test_gdalalg_vector_pipeline_info_executable():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(
        f"{gdal_path} vector pipeline read ../ogr/data/poly.shp ! info"
    )
    assert out.startswith("INFO: Open of `../ogr/data/poly.shp'")


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_pipeline_read_limit(tmp_vsimem):

    src_filename = tmp_vsimem / "src.gpkg"
    dst_filename = tmp_vsimem / "dst.gpkg"

    src_ds = gdal.GetDriverByName("GPKG").CreateVector(src_filename)
    layers = [src_ds.CreateLayer("layer1"), src_ds.CreateLayer("layer2")]

    for lyr in layers:
        for _ in range(5):
            f = ogr.Feature(lyr.GetLayerDefn())
            lyr.CreateFeature(f)

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        ["read", src_filename, "!", "limit", "3", "!", "write", dst_filename]
    )

    with gdal.OpenEx(dst_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 3
        assert ds.GetLayer(1).GetFeatureCount() == 3
