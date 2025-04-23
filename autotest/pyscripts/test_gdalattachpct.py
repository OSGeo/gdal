#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalattachpct.py testing
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest
import test_py_scripts

from osgeo import gdal

pytestmark = [
    pytest.mark.skipif(
        test_py_scripts.get_py_script("gdalattachpct") is None,
        reason="gdalattachpct.py not available",
    ),
]


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdalattachpct")


@pytest.fixture()
def palette_file(tmp_path):
    palette_filename = str(tmp_path / "pal.txt")
    with open(palette_filename, "wt") as f:
        f.write("0 1 2 3\n")
        f.write("255 254 253 252\n")
    return palette_filename


###############################################################################
# Basic test


def test_gdalattachpct_basic(script_path, tmp_path, palette_file):

    src_filename = str(tmp_path / "src.tif")
    ds = gdal.GetDriverByName("GTiff").Create(src_filename, 1, 1)
    ds.GetRasterBand(1).Fill(1)
    ds = None

    out_filename = str(tmp_path / "dst.tif")

    _, err = test_py_scripts.run_py_script(
        script_path,
        "gdalattachpct",
        f" {palette_file} {src_filename} {out_filename}",
        return_stderr=True,
    )
    assert "UseExceptions" not in err

    ds = gdal.Open(out_filename)
    assert ds.GetDriver().ShortName == "GTiff"
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct
    assert ct.GetCount() == 256
    assert ct.GetColorEntry(0) == (1, 2, 3, 255)
    assert ct.GetColorEntry(255) == (254, 253, 252, 255)
    assert ds.GetRasterBand(1).Checksum() == 1


###############################################################################
# Test outputting to VRT


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalattachpct_vrt_output(script_path, tmp_path, palette_file):

    src_filename = str(tmp_path / "src.tif")
    ds = gdal.GetDriverByName("GTiff").Create(src_filename, 1, 1)
    ds.GetRasterBand(1).Fill(1)
    ds = None

    out_filename = str(tmp_path / "dst.vrt")

    test_py_scripts.run_py_script(
        script_path,
        "gdalattachpct",
        f" {palette_file} {src_filename} {out_filename}",
    )

    ds = gdal.Open(out_filename)
    assert ds.GetDriver().ShortName == "VRT"
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct
    assert ct.GetCount() == 256
    assert ct.GetColorEntry(0) == (1, 2, 3, 255)
    assert ct.GetColorEntry(255) == (254, 253, 252, 255)
    assert ds.GetRasterBand(1).Checksum() == 1

    # Check source file is not altered
    ds = gdal.Open(src_filename)
    assert ds.GetRasterBand(1).GetColorTable() is None
