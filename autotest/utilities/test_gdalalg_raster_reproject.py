#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster reproject' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import math

import gdaltest
import pytest

from osgeo import gdal, osr


def get_reproject_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("reproject")


def test_gdalalg_raster_reproject(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= last_pct[0]
        last_pct[0] = pct
        return True

    alg = get_reproject_alg()
    alg["src-crs"] = "EPSG:32611"
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    alg["dst-crs"] = srs
    alg["input"] = "../gcore/data/byte.tif"
    alg["output"] = out_filename
    alg["creation-option"] = {"COMPRESS": "LZW"}
    assert alg.Run(my_progress) and alg.Finalize()
    assert last_pct[0] == 1.0

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4727
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"


def test_gdalalg_raster_reproject_through_pipeline(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= last_pct[0]
        last_pct[0] = pct
        return True

    assert gdal.Run(
        "raster",
        "pipeline",
        pipeline=f"read ../gcore/data/byte.tif ! reproject --src-crs=EPSG:32611 --dst-crs=EPSG:4326 ! write {out_filename} --co COMPRESS=LZW",
        progress=my_progress,
    )
    assert last_pct[0] == 1.0

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4727
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"


def test_gdalalg_raster_reproject_through_pipeline_non_optimized_path(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= last_pct[0]
        last_pct[0] = pct
        return True

    assert gdal.Run(
        "raster",
        "pipeline",
        pipeline=f"read ../gcore/data/byte.tif ! reproject --src-crs=EPSG:32611 --dst-crs=EPSG:4326 ! edit ! write {out_filename} --co COMPRESS=LZW",
        progress=my_progress,
    )
    assert last_pct[0] == 1.0

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4727
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"


def test_gdalalg_raster_reproject_failure(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    alg = get_reproject_alg()
    with pytest.raises(Exception, match="Unable to compute a transformation"):
        alg.ParseRunAndFinalize(
            [
                "--src-crs=EPSG:32611",
                "--dst-crs=EPSG:4326",
                "../gcore/data/nan32.tif",
                out_filename,
            ],
        )


def test_gdalalg_raster_reproject_size(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    alg = get_reproject_alg()
    alg.ParseRunAndFinalize(
        [
            "--size=10,0",
            "../gcore/data/byte.tif",
            out_filename,
        ],
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.RasterXSize == 10
        assert ds.RasterYSize == 10


def test_gdalalg_raster_reproject_bbox_crs(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    alg = get_reproject_alg()
    alg.ParseRunAndFinalize(
        [
            "--bbox=-117.638051657173,33.8904636339659,-117.627303823822,33.8995379597727",
            "--bbox-crs=EPSG:4267",
            "../gcore/data/byte.tif",
            out_filename,
        ],
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.RasterXSize == 17
        assert ds.RasterYSize == 17


def test_gdalalg_raster_reproject_srcnodata_dst_nodata(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.WriteRaster(0, 0, 2, 1, b"\x00\x01")

    alg = get_reproject_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["src-nodata"] = [1]
    alg["dst-nodata"] = [2]
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).GetNoDataValue() == 2
    assert out_ds.GetRasterBand(1).ReadRaster() == b"\x00\x02"


def test_gdalalg_raster_reproject_addalpha(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])

    alg = get_reproject_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["add-alpha"] = True
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.RasterCount == 2
    assert out_ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand


def test_gdalalg_raster_reproject_warp_option(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(2).SetNoDataValue(1)
    src_ds.GetRasterBand(2).WriteRaster(0, 0, 1, 1, b"\x02")

    alg = get_reproject_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["warp-option"] = ["UNIFIED_SRC_NODATA=YES"]
    alg["dst-nodata"] = [3, 4]
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds.GetRasterBand(1).ReadRaster() == b"\x00"


def test_gdalalg_raster_reproject_transform_option(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])

    alg = get_reproject_alg()
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "MEM"
    alg["transform-option"] = ["METHOD=RPC"]
    with pytest.raises(Exception, match="Unable to compute a RPC based transformation"):
        alg.Run()


def test_gdalalg_raster_reproject_error_threshold(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])

    alg = get_reproject_alg()
    with pytest.raises(
        Exception, match="Value of argument 'error-threshold' is -1, but should be >= 0"
    ):
        alg["error-threshold"] = -1


def test_gdalalg_raster_reproject_num_threads_warp_option(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    alg = get_reproject_alg()
    assert alg.ParseRunAndFinalize(
        [
            "--src-crs=EPSG:32611",
            "--dst-crs=EPSG:4326",
            "../gcore/data/byte.tif",
            "--wo=NUM_THREADS=2",
            out_filename,
        ],
    )


def test_gdalalg_raster_reproject_both_num_threads_and_warp_option(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    alg = get_reproject_alg()
    with pytest.raises(
        Exception,
        match="--num-threads argument and NUM_THREADS warp options are mutually exclusive",
    ):
        alg.ParseRunAndFinalize(
            [
                "--src-crs=EPSG:32611",
                "--dst-crs=EPSG:4326",
                "../gcore/data/byte.tif",
                "--wo=NUM_THREADS=1",
                "--num-threads=2",
                out_filename,
            ],
        )


def test_gdalalg_raster_reproject_complete_dst_crs():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster reproject ../gcore/data/byte.tif --dst-crs=EPSG:"
    )
    assert "4326\\ --" in out
    assert "2193\\ --" not in out  # NZGD2000


@pytest.mark.require_proj(8, 1)
def test_gdalalg_raster_reproject_complete_dst_crs_iau_earth():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster reproject ../gcore/data/byte.tif --dst-crs=IAU:"
    )
    assert "Earth" in out
    assert "Mars" not in out


@pytest.mark.require_proj(8, 1)
def test_gdalalg_raster_reproject_complete_dst_crs_iau_mars(tmp_path):
    import gdaltest
    import test_cli_utilities

    ds = gdal.GetDriverByName("GTiff").Create(tmp_path / "in.tif", 1, 1)
    srs = osr.SpatialReference()
    srs.SetFromUserInput("IAU:49900")  # Mars sphere
    ds.SetSpatialRef(srs)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds = None

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster reproject {tmp_path}/in.tif --dst-crs=IAU:"
    )
    assert "Earth" not in out
    assert "Mars" in out


def test_empty_bbox(tmp_vsimem):
    """Test issue GH #13498, crashes when empty bbox and source has overviews"""

    in_filename = str(tmp_vsimem / "test_empty_bbox_in.tif")
    out_filename = str(tmp_vsimem / "test_empty_bbox_out.tif")

    # To reproduce the issue, we need the code path that uses the overview
    ds = gdal.GetDriverByName("GTiff").Create(in_filename, 100, 100, 1)
    ds.SetProjection("EPSG:4269")
    ds.SetGeoTransform([0, 0.005, 0, 0, 0, -0.005])
    ds.FlushCache()
    ds.BuildOverviews("NEAR", overviewlist=[2])
    ds.FlushCache()
    ds = None

    alg = get_reproject_alg()
    # This resulted in a crash before the fix
    with pytest.raises(Exception, match="Invalid bounding box specified"):
        alg.ParseRunAndFinalize(
            [
                in_filename,
                "--bbox=-110,37,-110,37",
                "--bbox-crs=EPSG:4269",
                "--dst-crs=EPSG:4326",
                "--overwrite",
                "--of=COG",
                "--co=COMPRESS=DEFLATE",
                out_filename,
            ],
        )


def test_like(tmp_vsimem):

    driver = "GTiff"
    src_path = str(tmp_vsimem / "test_like_src.tif")
    template_path = str(tmp_vsimem / "test_like_template.tif")
    output_path = str(tmp_vsimem / "test_like_out.tif")

    # Create a memory raster with size 4x4, 1 band and resolution 15m
    src_ds = gdal.GetDriverByName(driver).Create(src_path, 4, 4)
    # Coordinates of the center: 514979.562E, 5034533.939N
    src_ds.SetGeoTransform([514979 - 30, 30, 0, 5034533 + 30, 0, -30])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32632)
    src_ds.SetSpatialRef(srs)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 4, 4, b"\x00\x01\x02\x03" * 4)

    # Create the template tif with size 4x2, 1 band, resolution 15m in EPSG:3857
    # Left: 1023163.16E, 5694828.43N
    # and bbox covering the center strip 2px wide of the source raster
    template_ds = gdal.GetDriverByName(driver).Create(template_path, 4, 2)
    template_ds.SetGeoTransform([1023163, 30, 0, 5694828, 0, -30])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    template_ds.SetSpatialRef(srs)
    template_ds.GetRasterBand(1).Fill(0)
    template_ds.FlushCache()

    alg = get_reproject_alg()
    alg["input"] = src_ds
    alg["output"] = output_path
    alg["output-format"] = driver
    alg["like"] = template_ds
    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    assert out_ds.RasterXSize == 4
    assert out_ds.RasterYSize == 2
    out_ds.GetRasterBand(1).ReadRaster(0, 0, 4, 2)
    assert (
        out_ds.GetRasterBand(1).ReadRaster(0, 0, 4, 2)
        == b"\x00\x01\x01\x02\x00\x01\x01\x02"
    )


def test_rotated(tmp_vsimem):
    """Test a rotated raster"""

    driver = "GTiff"
    src_path = str(tmp_vsimem / "test_like_rotated_src.tif")
    template_path = str(tmp_vsimem / "test_like_rotated_template.tif")
    output_path = str(tmp_vsimem / "test_like_rotated_out.tif")

    # Create a memory raster with size 4x4, 1 band and resolution 15m
    src_ds = gdal.GetDriverByName(driver).Create(src_path, 4, 4)
    # Coordinates of the center: 514979.562E, 5034533.939N
    src_ds.SetGeoTransform([514979 - 30, 30, 0, 5034533 + 30, 0, -30])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32632)
    src_ds.SetSpatialRef(srs)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 4, 4, b"\x00\x01\x02\x03" * 4)

    # Create the template with same size and resolution, but rotated 10 degrees clockwise
    template_ds = gdal.GetDriverByName(driver).Create(template_path, 4, 4)
    template_ds.SetGeoTransform(
        [
            514979 - 30,
            30 * math.cos(math.radians(10)),
            30 * math.sin(math.radians(10)),
            5034533 + 30,
            -30 * math.sin(math.radians(10)),
            -30 * math.cos(math.radians(10)),
        ]
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32632)
    template_ds.SetSpatialRef(srs)
    template_ds.GetRasterBand(1).Fill(0)
    template_ds.FlushCache()

    alg = get_reproject_alg()
    alg["input"] = src_ds
    alg["output"] = output_path
    alg["like"] = template_ds

    # Check that a warning is emitted about the rotation, but that the algorithm
    # doesn't fail.
    msgs = []

    def error_handler(type, code, msg):
        msgs.append(msg)

    with gdaltest.error_handler(error_handler):
        assert alg.Run()
        assert (
            "Dataset provided with --like has a geotransform with rotation. Ignoring it"
            in msgs[0]
        )
