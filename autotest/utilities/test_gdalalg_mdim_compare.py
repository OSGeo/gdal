#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal mdim compare' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("netCDF")


def test_gdalalg_mdim_compare_same_file():

    with gdal.alg.mdim.compare(
        input="../gdrivers/data/netcdf/byte.nc",
        reference="../gdrivers/data/netcdf/byte.nc",
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_mdim_compare_diff_by_one_pixel_everywhere(tmp_path):

    gdal.alg.raster.scale(
        input="../gdrivers/data/netcdf/byte.nc",
        output=tmp_path / "out.nc",
        input_min=0,
        input_max=255,
        output_min=1,
        output_max=256,
    )

    with gdal.alg.mdim.compare(
        reference="../gdrivers/data/netcdf/byte.nc",
        input=tmp_path / "out.nc",
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"] == """Array /Band1: maximum pixel value difference: 1
Array /Band1: pixels differing: 399
"""
        )


def test_gdalalg_mdim_compare_diff_by_one_pixel_everywhere_no_metric(tmp_path):

    gdal.alg.raster.scale(
        input="../gdrivers/data/netcdf/byte.nc",
        output=tmp_path / "out.nc",
        input_min=0,
        input_max=255,
        output_min=1,
        output_max=256,
    )

    with gdal.alg.mdim.compare(
        reference="../gdrivers/data/netcdf/byte.nc",
        input=tmp_path / "out.nc",
        skip_binary=True,
        metric="none",
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_mdim_compare_diff_by_one_pixel_everywhere_psnr_float(tmp_path):

    tmp_filename = tmp_path / "out.nc"
    with gdal.alg.raster.pipeline(
        pipeline=f"read ../gdrivers/data/netcdf/byte.nc ! set-type --output-data-type Float32 ! scale --input-min=0 --input-max=255 --output-min=1 --output-max=256 ! write {tmp_filename}"
    ) as _:
        pass

    with gdal.alg.mdim.compare(
        input="../gdrivers/data/netcdf/byte.nc",
        reference=tmp_filename,
        skip_binary=True,
        metric="PSNR",
    ) as alg:
        assert (
            alg["output-string"]
            == """Array /Band1: data type in reference is Float32, whereas it is Byte in input
Array /Band1: PSNR (dB): 45.1536
"""
        )


def test_gdalalg_mdim_compare_diff_by_one_pixel_everywhere_all_metrics(tmp_path):

    gdal.alg.raster.scale(
        input="../gdrivers/data/netcdf/byte.nc",
        output=tmp_path / "out.nc",
        input_min=0,
        input_max=255,
        output_min=1,
        output_max=256,
    )

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.alg.mdim.compare(
        input="../gdrivers/data/netcdf/byte.nc",
        reference=tmp_path / "out.nc",
        metric="all",
        skip_binary=True,
        progress=my_progress,
    ) as alg:
        assert (
            alg["output-string"] == """Array /Band1: maximum pixel value difference: 1
Array /Band1: pixels differing: 399
Array /Band1: RMSD: 0.998749
Array /Band1: PSNR (dB): 48.1417
"""
        )

    assert tab_pct[0] == 1


def test_gdalalg_mdim_compare_errors():

    with pytest.raises(
        Exception, match="Array '/i/do/not/exist' does not exist in reference dataset"
    ):
        gdal.alg.mdim.compare(
            input="../gdrivers/data/netcdf/byte.nc",
            reference="../gdrivers/data/netcdf/byte.nc",
            array="/i/do/not/exist",
            skip_binary=True,
        )

    src_ds = gdal.GetDriverByName("MEM").CreateMultiDimensional("")

    with pytest.raises(
        Exception, match="Array '/Band1' does not exist in input dataset"
    ):
        gdal.alg.mdim.compare(
            input=src_ds,
            reference="../gdrivers/data/netcdf/byte.nc",
            array="/Band1",
            skip_binary=True,
        )

    with gdal.alg.mdim.compare(
        input=src_ds,
        reference="../gdrivers/data/netcdf/byte.nc",
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "The following arrays are found in the reference dataset, but missing in the input one: /Band1, /x, /y\n"
        )

    with gdal.alg.mdim.compare(
        reference=src_ds,
        input="../gdrivers/data/netcdf/byte.nc",
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "The following arrays are found in the input dataset, but missing in the reference one: /Band1, /x, /y\n"
        )


def test_gdalalg_mdim_compare_in_pipeline(tmp_path):

    tmp_filename = tmp_path / "out.nc"
    gdal.alg.raster.scale(
        input="../gdrivers/data/netcdf/byte.nc",
        output=tmp_filename,
        input_min=0,
        input_max=255,
        output_min=1,
        output_max=256,
    )

    with gdal.alg.mdim.pipeline(
        pipeline=f"read {tmp_filename} ! compare ../gdrivers/data/netcdf/byte.nc --skip-binary --array Band1"
    ) as alg:
        assert (
            alg["output-string"] == """Array /Band1: maximum pixel value difference: 1
Array /Band1: pixels differing: 399
"""
        )
