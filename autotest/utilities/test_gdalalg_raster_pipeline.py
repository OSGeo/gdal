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
import os

import gdaltest
import pytest

from osgeo import gdal


def get_pipeline_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["pipeline"]


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
    pipeline["pipeline"] = f"read ../gcore/data/byte.tif ! write {out_filename}"
    assert pipeline.Run()
    ds = pipeline["output"].GetDataset()
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert pipeline.Finalize()
    ds = None

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_pipeline_input_through_api(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    pipeline["input"] = "../gcore/data/byte.tif"
    pipeline["pipeline"] = f"read ! write {out_filename}"
    assert pipeline.Run()
    assert pipeline.Finalize()

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_pipeline_input_through_api_run_twice(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    pipeline["input"] = "../gcore/data/byte.tif"
    pipeline["pipeline"] = f"read ! write {out_filename}"
    assert pipeline.Run()
    with pytest.raises(
        Exception, match=r"pipeline: Step nr 0 \(read\) has already an output dataset"
    ):
        pipeline.Run()


def test_gdalalg_raster_pipeline_output_through_api(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    pipeline["output"] = out_filename
    pipeline["pipeline"] = "read ../gcore/data/byte.tif ! write"
    assert pipeline.Run()
    assert pipeline.Finalize()

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672


def test_gdalalg_raster_pipeline_as_api_error():

    pipeline = get_pipeline_alg()
    pipeline["pipeline"] = "read"
    with pytest.raises(Exception, match="pipeline: At least 2 steps must be provided"):
        pipeline.Run()


def test_gdalalg_raster_pipeline_mutually_exclusive_args():

    with pytest.raises(
        Exception, match="clip: Argument 'window' is mutually exclusive with 'bbox'"
    ):
        gdal.Run(
            "raster pipeline",
            input="../gcore/data/byte.tif",
            output_format="MEM",
            output="",
            pipeline="read ! clip --bbox=1,2,3,4 --window=1,2,3,4 ! write",
        )


def test_gdalalg_raster_pipeline_usage_as_json():

    pipeline = get_pipeline_alg()
    j = json.loads(pipeline.GetUsageAsJSON())
    assert "pipeline_algorithms" in j


def test_gdalalg_raster_pipeline_help_doc():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(f"{gdal_path} raster pipeline --help-doc=main")

    assert "Usage: gdal raster pipeline [OPTIONS] <PIPELINE>" in out
    assert "<PIPELINE> is of the form: " in out

    out = gdaltest.runexternal(f"{gdal_path} raster pipeline --help-doc=edit")

    assert "* edit [OPTIONS]" in out

    out, _ = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster pipeline --help-doc=unknown"
    )

    assert "ERROR: unknown pipeline step 'unknown'" in out


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


def test_gdalalg_raster_easter_egg_failed():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster +gdal=pipeline +step +gdal=read +input=../gcore/data/byte.tif +step +gdal=unknown +step +write +output=/vsimem/out.tif"
    )

    assert "pipeline: unknown step name: unknown" in err


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
    with pytest.raises(Exception, match="write: Option '--invalid' is unknown"):
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
        match="already exists",
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
        match="Value of argument 'resolution' is -1, but should be > 0",
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


def test_gdalalg_raster_pipeline_reproject_proj_string(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    pipeline = get_pipeline_alg()
    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../gcore/data/byte.tif",
            "!",
            "reproject",
            "--src-crs=EPSG:32611",
            "--dst-crs",
            "+proj=laea +lon_0=147 +lat_0=-40 +datum=WGS84",
            "!",
            "write",
            "--overwrite",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert "Lambert Azimuthal Equal Area" in ds.GetSpatialRef().ExportToWkt(
            ["FORMAT=WKT2"]
        ), ds.GetSpatialRef().ExportToWkt(["FORMAT=WKT2"])


def test_gdalalg_raster_pipeline_too_many_steps_for_vrt_output(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.vrt")

    pipeline = get_pipeline_alg()
    with pytest.raises(
        Exception,
        match="pipeline: VRT output is not supported when there are more than 3 steps",
    ):
        pipeline.ParseRunAndFinalize(
            [
                "read",
                "../gcore/data/byte.tif",
                "!",
                "reproject",
                "!",
                "reproject",
                "!",
                "write",
                "--overwrite",
                out_filename,
            ]
        )


@pytest.mark.parametrize(
    "config_options", [{}, {"GDAL_RASTER_PIPELINE_USE_GTIFF_FOR_TEMP_DATASET": "YES"}]
)
def test_gdalalg_raster_pipeline_to_gdalg_step_non_natively_streamable(
    tmp_vsimem, config_options
):

    src_filename = os.path.join(os.getcwd(), "../gcore/data/byte.tif")

    with gdaltest.error_raised(gdal.CE_Warning):
        gdal.Run(
            "raster",
            "pipeline",
            pipeline=f"read {src_filename} ! fill-nodata ! write {tmp_vsimem}/out.gdalg.json",
        )

    if gdal.GetDriverByName("GDALG"):
        with gdaltest.config_options(config_options):
            with gdal.Open(tmp_vsimem / "out.gdalg.json") as ds:
                assert ds.GetRasterBand(1).Checksum() == 4672

        new_options = {"CPL_TMPDIR": "/i_do/not/exist"}
        new_options.update(config_options)
        with gdaltest.config_options(new_options):
            with pytest.raises(Exception):
                gdal.Open(tmp_vsimem / "out.gdalg.json")


def test_gdalalg_raster_pipeline_help():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(f"{gdal_path} raster pipeline --help")
    assert out.startswith("Usage: gdal raster pipeline [OPTIONS] <PIPELINE>")
    assert "* read [OPTIONS] <INPUT>" in out
    assert "* write [OPTIONS] <OUTPUT>" in out

    out = gdaltest.runexternal(f"{gdal_path} raster pipeline --progress --help")
    assert out.startswith("Usage: gdal raster pipeline [OPTIONS] <PIPELINE>")
    assert "* read [OPTIONS] <INPUT>" in out
    assert "* write [OPTIONS] <OUTPUT>" in out

    out = gdaltest.runexternal(f"{gdal_path} raster pipeline read --help")
    assert out.startswith("Usage: read [OPTIONS] <INPUT>")

    out = gdaltest.runexternal(
        f"{gdal_path} raster pipeline read foo.tif ! select --help"
    )
    assert out.startswith("Usage: select [OPTIONS] <BAND>")


def test_gdalalg_raster_pipeline_calc():

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("muparser not available")

    with gdal.Run(
        "raster",
        "pipeline",
        output_format="MEM",
        output="",
        pipeline="calc ../gcore/data/byte.tif --calc 255-X ! write",
    ) as alg:
        assert alg.Output().GetRasterBand(1).Checksum() == 4563


def test_gdalalg_raster_pipeline_info():

    with gdal.Run(
        "raster",
        "pipeline",
        pipeline="read ../gcore/data/byte.tif ! info",
    ) as alg:
        assert "bands" in alg.Output()


def test_gdalalg_raster_pipeline_info_executable():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(
        f"{gdal_path} raster pipeline read ../gcore/data/byte.tif ! info"
    )
    assert out.startswith("Driver: GTiff/GeoTIFF")
