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


@pytest.mark.parametrize("histogram_dest", ("file", "stdout"))
def test_gdalenhance_output_histogram(gdalenhance_path, histogram_dest, tmp_path):

    lut_fname = tmp_path / "lut.txt"

    cmd = f"{gdalenhance_path} -quiet -equalize ../gcore/data/rgbsmall.tif"

    if histogram_dest == "file":
        cmd += f" -config {lut_fname}"

    out, err = gdaltest.runexternal_out_and_err(cmd)

    assert not err

    if histogram_dest == "stdout":

        lines = out.strip().split("\n")
        assert len(lines) == 3

        assert lines[0].startswith("1:Band ")
        assert lines[1].startswith("2:Band ")
        assert lines[2].startswith("3:Band ")

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


###############################################################################
# Malformed LUT


def test_gdalenhance_malformed_lut(gdalenhance_path, tmp_path):

    infile = "../gcore/data/rgbsmall.tif"
    outfile = tmp_path / "out.tif"
    lut_file = tmp_path / "lut.txt"

    with open(lut_file, "w") as lut:
        # not enough counts in each line
        lut.write(
            "1:Band -0.5:ScaleMin 255.5:ScaleMax "
            + " ".join(str(x) for x in range(256))
            + "\n"
        )
        lut.write(
            "2:Band -0.5:ScaleMin 255.5:ScaleMax "
            + " ".join(str(x) for x in range(255))
            + "\n"
        )
        lut.write(
            "3:Band -0.5:ScaleMin 255.5:ScaleMax "
            + " ".join(str(x) for x in range(256))
            + "\n"
        )

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdalenhance_path} {infile} {outfile} -config {lut_file}"
    )

    assert "Line 2 seems to be corrupt" in err
    assert "ret code = 1" in err

    with open(lut_file, "w") as lut:
        # not enough lines
        lut.write(
            "1:Band -0.5:ScaleMin 255.5:ScaleMax "
            + " ".join(str(x) for x in range(256))
            + "\n"
        )
        lut.write(
            "2:Band -0.5:ScaleMin 255.5:ScaleMax "
            + " ".join(str(x) for x in range(256))
            + "\n"
        )

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdalenhance_path} {infile} {outfile} -config {lut_file}"
    )

    assert "Did not get 3 lines" in err
    assert "ret code = 1" in err


###############################################################################
# Invalid output type


def test_gdalenhance_invalid_output_type(gdalenhance_path, tmp_path):

    infile = "../gcore/data/rgbsmall.tif"
    outfile = tmp_path / "out.tif"

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdalenhance_path} -equalize -ot Int16 {infile} {outfile}"
    )

    assert "only supports Byte output" in err
    assert "ret code = 1" in err
