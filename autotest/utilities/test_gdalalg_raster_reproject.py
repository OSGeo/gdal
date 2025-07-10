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
