#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster sieve' testing
# Author:   Alessandro Pasotti
#
###############################################################################
# Copyright (c) 2025, Alessandro Pasotti
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal

pytest.importorskip("numpy")
gdaltest.importorskip_gdal_array()

gdal.UseExceptions()


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["sieve"]


@pytest.mark.require_driver("AAIGRID")
@pytest.mark.require_driver("GTiff")
@pytest.mark.parametrize(
    "connect_diagonal_pixels,expected_checksum",
    (
        (False, 364),
        (True, 370),
    ),
)
@pytest.mark.parametrize(
    "creation_options", ({}, {"TILED": "YES"}, {"COMPRESS": "LZW"})
)
def test_gdalalg_raster_sieve(
    tmp_path, tmp_vsimem, connect_diagonal_pixels, expected_checksum, creation_options
):

    result_tif = str(tmp_path / "test_gdal_sieve.tif")
    tmp_filename = str(tmp_vsimem / "tmp.grd")

    gdal.FileFromMemBuffer(tmp_filename, open("../alg/data/sieve_src.grd", "rb").read())

    alg = get_alg()
    alg["input"] = tmp_filename
    alg["output"] = result_tif
    alg["size-threshold"] = 2
    alg["connect-diagonal-pixels"] = connect_diagonal_pixels
    alg["creation-option"] = creation_options
    assert alg.Run()
    assert alg.Finalize()

    ds = gdal.Open(result_tif)
    assert ds is not None
    assert ds.RasterCount == 1

    dst_band = ds.GetRasterBand(1)

    # Check if the output is TILED
    if "COMPRESS" in creation_options and creation_options["COMPRESS"] == "LZW":
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"
    if "TILED" in creation_options and creation_options["TILED"] == "YES":
        assert dst_band.GetBlockSize() == [256, 256]
    else:
        assert dst_band.GetBlockSize() != [256, 256]

    assert dst_band.Checksum() == expected_checksum


@pytest.mark.require_driver("AAIGRID")
@pytest.mark.require_driver("GTiff")
def test_gdalalg_raster_sieve_mask(tmp_path, tmp_vsimem):

    result_tif = str(tmp_path / "test_gdal_sieve.tif")
    tmp_filename = str(tmp_vsimem / "tmp.grd")

    gdal.FileFromMemBuffer(
        tmp_filename,
        """ncols        7
nrows        7
xllcorner    440720.000000000000
yllcorner    3750120.000000000000
cellsize     60.000000000000
NODATA_value 0
 0 0 0 0 0 0 0
 0 1 1 1 1 1 1
 0 1 0 0 1 1 1
 0 1 0 2 2 2 1
 0 1 1 2 1 2 1
 0 1 1 2 2 2 1
 0 1 1 1 1 1 1
 """,
    )

    alg = get_alg()
    alg["input"] = tmp_filename
    alg["output"] = result_tif
    alg["size-threshold"] = 2
    alg["mask"] = tmp_filename

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    assert alg.Run(my_progress)
    assert alg.Finalize()

    assert tab_pct[0] == 1.0

    ds = gdal.Open(result_tif)
    assert ds is not None
    assert ds.RasterCount == 1

    dst_band = ds.GetRasterBand(1)

    assert dst_band.Checksum() == 42

    # Non existing mask
    alg = get_alg()
    alg["input"] = tmp_filename
    alg["output"] = result_tif
    alg["size-threshold"] = 2
    alg["mask"] = "/i/do_not/exist"
    with pytest.raises(Exception):
        alg.Run()


@pytest.mark.require_driver("AAIGRID")
@pytest.mark.require_driver("GTiff")
def test_gdalalg_raster_sieve_overwrite(tmp_path, tmp_vsimem):

    result_tif = str(tmp_path / "test_gdal_sieve.tif")
    tmp_filename = str(tmp_vsimem / "tmp.grd")

    gdal.FileFromMemBuffer(tmp_filename, open("../alg/data/sieve_src.grd", "rb").read())

    alg = get_alg()
    alg["input"] = tmp_filename
    alg["output"] = result_tif
    alg["size-threshold"] = 2
    alg.Run()
    alg.Finalize()  # ensure output file is closed, to be later overwritten
    with pytest.raises(Exception, match="already exists"):
        alg.Run()

    alg["overwrite"] = True
    assert alg.Run()
    assert alg.Finalize()


@pytest.mark.require_driver("AAIGRID")
@pytest.mark.require_driver("GTiff")
def test_gdalalg_raster_sieve_cannot_create_temp_file(tmp_path, tmp_vsimem):

    result_tif = str(tmp_path / "test_gdal_sieve.tif")
    tmp_filename = str(tmp_vsimem / "tmp.grd")

    gdal.FileFromMemBuffer(tmp_filename, open("../alg/data/sieve_src.grd", "rb").read())

    alg = get_alg()
    alg["input"] = tmp_filename
    alg["output"] = result_tif
    alg["size-threshold"] = 2
    with gdaltest.config_options(
        {
            "GDAL_RASTER_PIPELINE_USE_GTIFF_FOR_TEMP_DATASET": "YES",
            "CPL_TMPDIR": "/i_do/not/exist",
        }
    ):
        with pytest.raises(Exception):
            alg.Run()
