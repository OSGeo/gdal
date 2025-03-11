#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector rasterize' testing
# Author:   Alessandro Pasotti, <elpaso at itopen dot it>
#
###############################################################################
# Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_rasterize_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["rasterize"]


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize(
    "options,expected",
    [
        (
            ["--all-touched", "-b", "3,2,1", "--burn", "200,220,240", "-l", "cutline"],
            121,
        ),
        (
            [
                "--all-touched",
                "--where",
                "Counter='2'",
                "-b",
                "3,2,1",
                "--burn",
                "200,220,240",
                "-l",
                "cutline",
            ],
            46,
        ),
    ],
)
def test_gdalalg_vector_rasterize(tmp_vsimem, options, expected):

    output_tif = str(tmp_vsimem / "rasterize_alg_1.tif")

    # Create a raster to rasterize into.
    target_ds = gdal.GetDriverByName("GTiff").Create(
        output_tif, 12, 12, 3, gdal.GDT_Byte
    )
    target_ds.SetGeoTransform((0, 1, 0, 12, 0, -1))

    # Close TIF file
    target_ds = None

    # Rasterize
    rasterize = get_rasterize_alg()
    assert rasterize.ParseRunAndFinalize(
        options + ["../alg/data/cutline.csv", output_tif]
    )

    # Check the result

    target_ds = gdal.Open(output_tif)
    checksum = target_ds.GetRasterBand(2).Checksum()
    assert (
        checksum == expected
    ), "Did not get expected image checksum (got %d, expected %d)" % (
        checksum,
        expected,
    )

    target_ds = None
