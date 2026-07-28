#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  sozip testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_cli_utility_path("sozip") is None,
    reason="sozip_path not available",
)


@pytest.fixture()
def sozip_path():
    return test_cli_utilities.get_cli_utility_path("sozip")


###############################################################################


def test_sozip_list(sozip_path):

    out, err = gdaltest.runexternal_out_and_err(
        sozip_path + " --list ../gcore/data/zero_5GB_sozip_of_sozip.zip"
    )
    assert err is None or err == "", "got error/warning"
    assert " 5232873 " in out
    assert " 2023-01-05 15:41:14 " in out
    assert " yes " in out
    assert "32768 bytes" in out
    assert " zero_5GB.bin.zip" in out


###############################################################################


def test_sozip_create(sozip_path, tmp_path):

    output_zip = str(tmp_path / "sozip.zip")

    out, err = gdaltest.runexternal_out_and_err(
        f"{sozip_path} -j --overwrite --enable-sozip=yes --sozip-chunk-size 128 --content-type=image/tiff {output_zip} ../gcore/data/byte.tif"
    )
    assert err is None or err == "", "got error/warning"

    md = gdal.GetFileMetadata(f"/vsizip/{output_zip}/byte.tif", None)
    assert md["Content-Type"] == "image/tiff"

    md = gdal.GetFileMetadata(f"/vsizip/{output_zip}/byte.tif", "ZIP")
    assert md["SOZIP_VALID"] == "YES"
    assert md["SOZIP_CHUNK_SIZE"] == "128"


###############################################################################


def test_sozip_create_recurse(sozip_path, tmp_path):

    gdal.Mkdir(tmp_path / "subdir", 0o755)
    with gdal.VSIFile(tmp_path / "subdir" / "a", "wb") as f:
        f.write(b"x" * 10001)

    output_zip = str(tmp_path / "sozip.zip")

    out, err = gdaltest.runexternal_out_and_err(
        f"{sozip_path} -r --sozip-min-file-size 1000 --sozip-chunk-size 128 -j {output_zip} {tmp_path}"
    )
    assert err is None or err == "", "got error/warning"

    md = gdal.GetFileMetadata(f"/vsizip/{output_zip}/a", "ZIP")
    assert md["SOZIP_VALID"] == "YES"


###############################################################################


def test_sozip_append(sozip_path, tmp_path):

    output_zip = str(tmp_path / "sozip.zip")

    out, err = gdaltest.runexternal_out_and_err(
        f"{sozip_path} -j --enable-sozip=yes --sozip-chunk-size 128 {output_zip} ../gcore/data/byte.tif"
    )
    assert err is None or err == "", "got error/warning"

    out, err = gdaltest.runexternal_out_and_err(
        f"{sozip_path} -j {output_zip} ../gcore/data/uint16.tif"
    )
    assert err is None or err == "", "got error/warning"

    md = gdal.GetFileMetadata(f"/vsizip/{output_zip}/byte.tif", "ZIP")
    assert md["SOZIP_VALID"] == "YES"
    assert md["SOZIP_CHUNK_SIZE"] == "128"

    md = gdal.GetFileMetadata(f"/vsizip/{output_zip}/uint16.tif", "ZIP")
    assert md != {}
    assert "SOZIP_VALID" not in md


###############################################################################


def test_sozip_validate(sozip_path, tmp_path):

    output_zip = str(tmp_path / "sozip.zip")

    out, err = gdaltest.runexternal_out_and_err(
        f"{sozip_path} -j --enable-sozip=yes --sozip-chunk-size 128 {output_zip} ../gcore/data/byte.tif"
    )
    assert err is None or err == "", "got error/warning"

    out, err = gdaltest.runexternal_out_and_err(f"{sozip_path} --validate {output_zip}")
    assert err is None or err == "", "got error/warning"
    assert "File byte.tif has a valid SOZip index, using chunk_size = 128" in out
    assert "sozip.zip is a valid .zip file, and contains 1 SOZip-enabled file(s)" in out

    out2, err = gdaltest.runexternal_out_and_err(
        f"{sozip_path} --validate --verbose {output_zip}"
    )
    assert len(out2) > len(out)


###############################################################################


def test_sozip_optimize_from(sozip_path, tmp_path):

    output_zip = str(tmp_path / "sozip.zip")

    out, err = gdaltest.runexternal_out_and_err(
        f"{sozip_path} --optimize-from=../ogr/data/filegdb/test_spatial_index.gdb.zip {output_zip}"
    )
    assert err is None or err == "", "got error/warning"

    out, err = gdaltest.runexternal_out_and_err(f"{sozip_path} --validate {output_zip}")
    assert err is None or err == "", "got error/warning"
    assert (
        "File test_spatial_index.gdb/a00000009.spx has a valid SOZip index, using chunk_size = 32768"
        in out
    )
    assert "sozip.zip is a valid .zip file, and contains 2 SOZip-enabled file(s)" in out
