#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster fillnodata' testing
# Author:   Alessandro Pasotti <elpaso at itopen dot it>
#
###############################################################################
# Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

pytest.importorskip("numpy")
gdaltest.importorskip_gdal_array()


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["fillnodata"]


def test_gdalalg_raster_fillnodata_cannot_open_file():

    alg = get_alg()
    alg["input"] = "/i_do/not/exist/in.tif"
    alg["output"] = "/i_do/not/exist/out.tif"
    with pytest.raises(
        Exception,
        match=r"No such file or directory|does not exist",
    ):
        alg.Run()


def run_alg(alg, tmp_path, tmp_vsimem):
    result_tif = str(tmp_path / "test_gdal_fillnodata_1.tif")
    tmp_filename = str(tmp_vsimem / "tmp.tif")
    gdal.FileFromMemBuffer(
        tmp_filename, open("../gcore/data/nodata_byte.tif", "rb").read()
    )
    alg["input"] = tmp_filename
    alg["output"] = result_tif

    # Check the value of pixel 1 - 1 is nodata
    ds = gdal.Open(tmp_filename)
    assert ds is not None
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).GetNoDataValue() == 0
    assert ds.ReadAsArray(1, 1, 1, 1)[0][0] == 0
    del ds

    alg.Run()

    # Check the value of pixel 1 - 1 is not nodata
    ds = gdal.Open(result_tif)
    assert ds is not None
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).GetNoDataValue() == 0
    return ds


@pytest.mark.parametrize("creation_option", ({}, {"TILED": "YES"}, {"COMPRESS": "LZW"}))
def test_gdalalg_raster_fillnodata(tmp_path, tmp_vsimem, creation_option):

    alg = get_alg()
    alg["creation-option"] = creation_option
    ds = run_alg(alg, tmp_path, tmp_vsimem)

    # Check the value of pixel 1 - 1
    assert ds.ReadAsArray(1, 1, 1, 1)[0][0] == 125

    # Check the creation options
    dst_band = ds.GetRasterBand(1)
    if "COMPRESS" in creation_option and creation_option["COMPRESS"] == "LZW":
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"
    if "TILED" in creation_option and creation_option["TILED"] == "YES":
        assert dst_band.GetBlockSize() == [256, 256]
    else:
        assert dst_band.GetBlockSize() != [256, 256]
    del ds


def test_gdalalg_raster_fillnodata_overwrite(tmp_path, tmp_vsimem):

    alg = get_alg()
    ds = run_alg(alg, tmp_path, tmp_vsimem)
    del ds

    with pytest.raises(
        Exception,
        match="already exists",
    ):
        alg.Run()

    alg["overwrite"] = True
    ds = run_alg(alg, tmp_path, tmp_vsimem)
    assert ds.ReadAsArray(1, 1, 1, 1)[0][0] == 125
    del ds


def test_gdalalg_raster_fillnodata_smoothing(tmp_path, tmp_vsimem):

    alg = get_alg()
    alg["s"] = 2
    ds = run_alg(alg, tmp_path, tmp_vsimem)
    assert ds.ReadAsArray(1, 1, 1, 1)[0][0] == 122
    del ds


def test_gdalalg_raster_fillnodata_max_distance(tmp_path, tmp_vsimem):

    alg = get_alg()
    alg["d"] = 1
    ds = run_alg(alg, tmp_path, tmp_vsimem)
    assert ds.ReadAsArray(1, 1, 1, 1)[0][0] == 123
    del ds


def test_gdalalg_raster_fillnodata_strategy(tmp_path, tmp_vsimem):

    alg = get_alg()
    alg["strategy"] = "invdist"
    ds = run_alg(alg, tmp_path, tmp_vsimem)
    assert ds.ReadAsArray(1, 1, 1, 1)[0][0] == 125
    del ds

    alg["strategy"] = "nearest"
    alg["overwrite"] = True
    ds = run_alg(alg, tmp_path, tmp_vsimem)
    assert ds.ReadAsArray(1, 1, 1, 1)[0][0] == 123
    del ds


def test_gdalalg_raster_fillnodata_mask(tmp_path, tmp_vsimem):

    # Create a mask
    mask_tif = str(tmp_vsimem / "mask.tif")
    gdal.FileFromMemBuffer(mask_tif, open("../gcore/data/nodata_byte.tif", "rb").read())

    alg = get_alg()
    alg["mask"] = mask_tif

    # Alter pixel 0 0 to NOT be invalid
    ds = gdal.Open(mask_tif, gdal.GA_Update)
    assert ds is not None
    assert ds.RasterCount == 1
    ds.GetRasterBand(1).SetNoDataValue(128)
    ds.WriteRaster(0, 0, 1, 1, b"\x00")
    del ds

    ds = run_alg(alg, tmp_path, tmp_vsimem)
    assert ds.ReadAsArray(0, 0, 1, 1)[0][0] == 120
    del ds

    # Restore 0 0 mask valid value
    ds = gdal.Open(mask_tif, gdal.GA_Update)
    assert ds is not None
    assert ds.RasterCount == 1
    ds.WriteRaster(0, 0, 1, 1, b"\x01")
    ds.FlushCache()
    del ds

    alg["overwrite"] = True
    ds = run_alg(alg, tmp_path, tmp_vsimem)
    assert ds.ReadAsArray(1, 1, 1, 1)[0][0] == 125
    del ds
