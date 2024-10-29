#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalenhance testing
# Author:   Daniel Baston <dbaston at gmail.com>
#
###############################################################################
# Copyright (c) 2024, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdalenhance_path() is None,
    reason="gdalenhance not available",
)


@pytest.fixture()
def gdalenhance_path():
    return test_cli_utilities.get_gdalenhance_path()


###############################################################################
# Output a lookup table, then apply it to the image


def test_gdalenhance_output_histogram(gdalenhance_path, tmp_path):

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdalenhance_path} -quiet -equalize ../gcore/data/rgbsmall.tif"
    )

    assert not err

    lines = out.strip().split("\n")
    assert len(lines) == 3

    assert lines[0].startswith("1:Band ")
    assert lines[1].startswith("2:Band ")
    assert lines[2].startswith("3:Band ")

    lut_fname = tmp_path / "lut.txt"

    with open(lut_fname, "w") as outfile:
        for line in lines:
            outfile.write(line.strip())
            outfile.write("\n")

    enhanced_fname = tmp_path / "out.tif"

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdalenhance_path} -quiet -config {lut_fname} ../gcore/data/rgbsmall.tif {enhanced_fname}"
    )

    assert not err

    assert enhanced_fname.exists()


###############################################################################
# Write a new image directly


def test_gdalenhance_output_image(gdalenhance_path, tmp_path):

    infile = "../gcore/data/rgbsmall.tif"
    outfile = tmp_path / "out.tif"

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdalenhance_path} -quiet -equalize -co COMPRESS=DEFLATE {infile} {outfile}"
    )

    assert not err

    with gdal.Open(infile) as src, gdal.Open(outfile) as dst:
        assert src.RasterCount == dst.RasterCount
        assert src.RasterXSize == dst.RasterXSize
        assert src.RasterYSize == dst.RasterYSize

        # check that -co was honored
        assert dst.GetMetadata("IMAGE_STRUCTURE")["COMPRESSION"] == "DEFLATE"


###############################################################################
# Usage printed with invalid arguments


def test_gdalenhance_invalid_usage(gdalenhance_path, tmp_path):

    infile = "../gcore/data/rgbsmall.tif"
    outfile = tmp_path / "out.tif"

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdalenhance_path} -quiet {infile} {outfile}"
    )

    assert "ret code = 1" in err
    assert "Usage" in out
