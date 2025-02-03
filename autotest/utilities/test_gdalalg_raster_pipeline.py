#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster pipeline' testing
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
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("pipeline")


def test_gdalalg_raster_pipeline_read_and_write(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        ["read", "../gcore/data/byte.tif", "!", "write", out_filename], my_progress
    )
    assert last_pct[0] == 1.0

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672

    with pytest.raises(Exception, match="can only be called once per instance"):
        pipeline.ParseRunAndFinalize(
            ["read", "../gcore/data/byte.tif", "!", "write", out_filename], my_progress
        )


def test_gdalalg_raster_pipeline_pipeline_arg(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    # Also test extra pipes / exclamation mark
    assert pipeline.ParseRunAndFinalize(
        ["--pipeline", f"! read ../gcore/data/byte.tif | | write {out_filename} !"]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_pipeline_as_api(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    pipeline.GetArg("pipeline").Set(
        f"read ../gcore/data/byte.tif ! write {out_filename}"
    )
    assert pipeline.Run()
    ds = pipeline.GetArg("output").Get().GetDataset()
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert pipeline.Finalize()
    ds = None

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_pipeline_input_through_api(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    pipeline.GetArg("input").Get().SetDataset(gdal.OpenEx("../gcore/data/byte.tif"))
    pipeline.GetArg("pipeline").Set(f"read ! write {out_filename}")
    assert pipeline.Run()
    assert pipeline.Finalize()

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_pipeline_input_through_api_run_twice(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    pipeline.GetArg("input").Get().SetDataset(gdal.OpenEx("../gcore/data/byte.tif"))
    pipeline.GetArg("pipeline").Set(f"read ! write {out_filename}")
    assert pipeline.Run()
    with pytest.raises(
        Exception, match=r"pipeline: Step nr 0 \(read\) has already an output dataset"
    ):
        pipeline.Run()


def test_gdalalg_raster_pipeline_output_through_api(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    pipeline.GetArg("output").Get().SetName(out_filename)
    pipeline.GetArg("pipeline").Set("read ../gcore/data/byte.tif ! write")
    assert pipeline.Run()
    assert pipeline.Finalize()

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_pipeline_as_api_error():

    pipeline = get_pipeline_alg()
    pipeline.GetArg("pipeline").Set("read")
    with pytest.raises(Exception, match="pipeline: At least 2 steps must be provided"):
        pipeline.Run()


def test_gdalalg_raster_pipeline_usage_as_json():

    pipeline = get_pipeline_alg()
    j = json.loads(pipeline.GetUsageAsJSON())
    assert "pipeline_algorithms" in j


def test_gdalalg_raster_pipeline_quoted(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [f"read ../gcore/data/byte.tif ! write {out_filename}"]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_pipeline_progress(tmp_path):

    out_filename = str(tmp_path / "out.tif")
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster pipeline --progress read ../gcore/data/byte.tif ! write {out_filename}"
    )
    assert out.startswith(
        "0...10...20...30...40...50...60...70...80...90...100 - done."
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_easter_egg(tmp_path):

    out_filename = str(tmp_path / "out.tif")
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    gdaltest.runexternal(
        f"{gdal_path} raster +gdal=pipeline +step +gdal=read +input=../gcore/data/byte.tif +step +write +output={out_filename}"
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_pipeline_usage_as_json_bis():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster pipeline --json-usage"
    )
    j = json.loads(out)
    assert "pipeline_algorithms" in j


def test_gdalalg_raster_pipeline_missing_at_run():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: 'pipeline' argument not set"):
        pipeline.Run()


def test_gdalalg_raster_pipeline_empty_args():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: At least 2 steps must be provided"):
        pipeline.ParseRunAndFinalize([])


def test_gdalalg_raster_pipeline_unknow_step():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: unknown step name: unknown_step"):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../gcore/data/byte.tif",
                "!",
                "unknown_step",
                "!",
                "write",
                "/vsimem/foo.tif",
            ]
        )


def test_gdalalg_raster_pipeline_read_read():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: Last step should be 'write'"):
        pipeline.ParseRunAndFinalize(
            ["read", "../gcore/data/byte.tif", "!", "read", "../gcore/data/byte.tif"]
        )


def test_gdalalg_raster_pipeline_write_write():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: First step should be 'read'"):
        pipeline.ParseRunAndFinalize(
            ["write", "/vsimem/out.tif", "!", "write", "/vsimem/out.tif"]
        )


def test_gdalalg_raster_pipeline_read_write_write():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: Only last step can be 'write'"):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../gcore/data/byte.tif",
                "!",
                "write",
                "/vsimem/out.tif",
                "!",
                "write",
                "/vsimem/out.tif",
            ]
        )


def test_gdalalg_raster_pipeline_read_read_write():

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="pipeline: Only first step can be 'read'"):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../gcore/data/byte.tif",
                "!",
                "read",
                "../gcore/data/byte.tif",
                "!",
                "write",
                "/vsimem/out.tif",
            ]
        )


def test_gdalalg_raster_pipeline_invalid_step_during_parsing(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception, match="write: Long name option '--invalid' is unknown"
    ):
        pipeline.ParseRunAndFinalize(
            ["read", "../gcore/data/byte.tif", "!", "write", "--invalid", out_filename]
        )


def test_gdalalg_raster_pipeline_invalid_step_during_validation(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="read: Positional arguments starting at 'INPUT' have not been specified",
    ):
        pipeline.ParseRunAndFinalize(["read", "!", "write", "--invalid", out_filename])


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_pipeline_write_options(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        ["read", "../gcore/data/byte.tif", "!", "write", "--of=GPKG", out_filename]
    )

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="already exists. Specify the --overwrite option to overwrite it",
    ):
        assert pipeline.ParseRunAndFinalize(
            ["read", "../gcore/data/byte.tif", "!", "write", out_filename]
        )

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        ["read", "../gcore/data/byte.tif", "!", "write", "--overwrite", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_pipeline_write_co(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
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


def test_gdalalg_raster_pipeline_reproject_invalid_src_crs(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="reproject: Invalid value for 'src-crs' argument",
    ):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../gcore/data/byte.tif",
                "!",
                "reproject",
                "--src-crs=invalid",
                "--dst-crs=EPSG:4326",
                "!",
                "write",
                out_filename,
            ]
        )


def test_gdalalg_raster_pipeline_reproject_invalid_dst_crs(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="reproject: Invalid value for 'dst-crs' argument",
    ):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../gcore/data/byte.tif",
                "!",
                "reproject",
                "--dst-crs=invalid",
                "!",
                "write",
                out_filename,
            ]
        )


def test_gdalalg_raster_pipeline_reproject_invalid_resolution(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="Target resolution should be strictly positive",
    ):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../gcore/data/byte.tif",
                "!",
                "reproject",
                "--resolution=1,-1",
                "!",
                "write",
                out_filename,
            ]
        )


def test_gdalalg_raster_pipeline_reproject_no_args(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
            "!",
            "reproject",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "26711"
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_pipeline_reproject_invalid_bbox(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="Value of 'bbox' should be xmin,ymin,xmax,ymax with xmin <= xmax and ymin <= ymax",
    ):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../gcore/data/byte.tif",
                "!",
                "reproject",
                "--bbox=3,4,2,1",
                "!",
                "write",
                out_filename,
            ]
        )


def test_gdalalg_raster_pipeline_reproject_bbox_arg(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
            "!",
            "reproject",
            "--src-crs=EPSG:32611",
            "--dst-crs=EPSG:4326",
            "--bbox=-117.641,33.89,-117.628,33.9005",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "4326"
        assert ds.GetGeoTransform() == pytest.approx(
            (-117.641, 0.0005909090909093286, 0.0, 33.9005, 0.0, -0.0005833333333333554)
        )
        assert ds.GetRasterBand(1).Checksum() == 4585


def test_gdalalg_raster_pipeline_reproject_almost_all_args(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
            "!",
            "reproject",
            "--src-crs=EPSG:32611",
            "--dst-crs=EPSG:4326",
            "--resampling=bilinear",
            "--resolution=0.0005,0.0004",
            "--target-aligned-pixels",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "4326"
        assert ds.GetGeoTransform() == pytest.approx(
            (-117.641, 0.0005, 0.0, 33.9008, 0.0, -0.0004), rel=1e-8
        )
        assert ds.GetRasterBand(1).Checksum() == 8515


def test_gdalalg_raster_pipeline_clip_missing_bbox_or_like(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception, match="clip: Either --bbox or --like must be specified"
    ):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../gcore/data/byte.tif",
                "!",
                "clip",
                "!",
                "write",
                "--overwrite",
                out_filename,
            ]
        )


def test_gdalalg_raster_pipeline_clip(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
            "!",
            "clip",
            "--bbox=440780,3750200,441860,3751260",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.Open(out_filename) as ds:
        assert ds.RasterXSize == 18
        assert ds.RasterYSize == 18
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "26711"
        assert ds.GetGeoTransform() == pytest.approx(
            (440780, 60, 0, 3751260, 0, -60), rel=1e-8
        )
        assert ds.GetRasterBand(1).Checksum() == 3695

    out2_filename = str(tmp_vsimem / "out2.tif")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
            "!",
            "clip",
            "--like",
            out_filename,
            "!",
            "write",
            "--overwrite",
            out2_filename,
        ]
    )

    with gdal.Open(out_filename) as ds:
        assert ds.RasterXSize == 18
        assert ds.RasterYSize == 18
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "26711"
        assert ds.GetGeoTransform() == pytest.approx(
            (440780, 60, 0, 3751260, 0, -60), rel=1e-8
        )
        assert ds.GetRasterBand(1).Checksum() == 3695


def test_gdalalg_raster_pipeline_clip_like_error(tmp_vsimem):

    ref_filename = str(tmp_vsimem / "out.tif")
    gdal.GetDriverByName("GTiff").Create(ref_filename, 1, 1)

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="has no geotransform matrix"):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../gcore/data/byte.tif",
                "!",
                "clip",
                "--like",
                ref_filename,
                "!",
                "write",
                "--overwrite",
                out_filename,
            ]
        )

    with gdal.GetDriverByName("GTiff").Create(ref_filename, 1, 1) as ds:
        ds.SetGeoTransform([2, 1, 0, 49, 0, -1])

    pipeline = get_pipeline_alg()
    with pytest.raises(Exception, match="has no SRS"):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../gcore/data/byte.tif",
                "!",
                "clip",
                "--like",
                ref_filename,
                "!",
                "write",
                "--overwrite",
                out_filename,
            ]
        )


def test_gdalalg_raster_pipeline_clip_bbox_crs(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
            "!",
            "clip",
            "--bbox=-117.631,33.89,-117.628,33.9005",
            "--bbox-crs=NAD27",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.Open(out_filename) as ds:
        assert ds.RasterXSize == 5
        assert ds.RasterYSize == 19
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "26711"
        print(ds.GetGeoTransform())
        assert ds.GetGeoTransform() == pytest.approx(
            (441620.0, 60.0, 0.0, 3751140.0, 0.0, -60.0), rel=1e-8
        )
