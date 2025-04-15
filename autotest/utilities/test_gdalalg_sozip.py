#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal sozip' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def test_gdalalg_sozip():
    with pytest.raises(Exception, match="should not be called directly"):
        gdal.GetGlobalAlgorithmRegistry()["sozip"].Run()


def test_gdalalg_sozip_create_no_zip_extension():

    alg = gdal.GetGlobalAlgorithmRegistry()["sozip"]["create"]
    with pytest.raises(Exception, match="Extension of zip filename should be .zip"):
        alg["output"] = "foo"


def create_source_files(tmp_vsimem):

    gdal.FileFromMemBuffer(tmp_vsimem / "a", "x" * (1024 * 1024 + 1))
    gdal.FileFromMemBuffer(tmp_vsimem / "b", "x" * 40000)
    gdal.Mkdir(tmp_vsimem / "subdir", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "subdir" / "c", "x")


@pytest.mark.parametrize("enable_sozip", ["auto", "yes", "no"])
def test_gdalalg_sozip_create(tmp_vsimem, enable_sozip):

    create_source_files(tmp_vsimem)

    alg = gdal.GetGlobalAlgorithmRegistry()["sozip"]["create"]
    alg["input"] = tmp_vsimem / "b"
    alg["output"] = tmp_vsimem / "out.zip"
    alg["quiet"] = True
    alg["no-paths"] = True
    alg["enable-sozip"] = enable_sozip
    alg["sozip-chunk-size"] = 16384
    assert alg.Run()
    assert alg["output-string"] == ""

    assert gdal.VSIStatL(f"/vsizip/{tmp_vsimem}/out.zip/b").size == 40000

    alg = gdal.GetGlobalAlgorithmRegistry()["sozip"]["validate"]
    alg["input"] = tmp_vsimem / "out.zip"
    assert alg.Run()
    if enable_sozip == "yes":
        assert (
            "b has a valid SOZip index, using chunk_size = 16384"
            in alg["output-string"]
        )
        assert (
            "is a valid .zip file, and contains 1 SOZip-enabled file(s)"
            in alg["output-string"]
        )
    else:
        assert "has a valid SOZip index" not in alg["output-string"]
        assert (
            "is a valid .zip file, but does not contain any SOZip-enabled files."
            in alg["output-string"]
        )

    alg = gdal.GetGlobalAlgorithmRegistry()["sozip"]["create"]
    alg["input"] = tmp_vsimem / "subdir" / "c"
    alg["output"] = tmp_vsimem / "out.zip"
    alg["no-paths"] = True
    alg["content-type"] = "application/octet-stream"
    assert alg.Run()

    assert gdal.VSIStatL(f"/vsizip/{tmp_vsimem}/out.zip/b").size == 40000
    assert gdal.VSIStatL(f"/vsizip/{tmp_vsimem}/out.zip/c").size == 1

    md = gdal.GetFileMetadata(f"/vsizip/{tmp_vsimem}/out.zip/b", "ZIP")
    if enable_sozip == "yes":
        assert md["SOZIP_VALID"] == "YES"
        assert md["SOZIP_CHUNK_SIZE"] == "16384"
    else:
        assert "SOZIP_VALID" not in md

    md = gdal.GetFileMetadata(f"/vsizip/{tmp_vsimem}/out.zip/c", None)
    assert md["Content-Type"] == "application/octet-stream"


def test_gdalalg_sozip_create_recursive_and_optimize_and_validate(tmp_vsimem):

    create_source_files(tmp_vsimem)

    alg = gdal.GetGlobalAlgorithmRegistry()["sozip"]["create"]
    alg["input"] = tmp_vsimem
    alg["recursive"] = True
    alg["output"] = tmp_vsimem / "out.zip"
    assert alg.Run()
    assert (
        alg["output-string"]
        == f"Adding {tmp_vsimem}/a... (1/3)\nAdding {tmp_vsimem}/b... (2/3)\nAdding {tmp_vsimem}/subdir/c... (3/3)\n"
    )

    assert (
        gdal.VSIStatL(f"/vsizip/{tmp_vsimem}/out.zip{tmp_vsimem}/a").size
        == 1024 * 1024 + 1
    )
    assert gdal.VSIStatL(f"/vsizip/{tmp_vsimem}/out.zip{tmp_vsimem}/b").size == 40000
    assert gdal.VSIStatL(f"/vsizip/{tmp_vsimem}/out.zip{tmp_vsimem}/subdir/c").size == 1

    alg = gdal.GetGlobalAlgorithmRegistry()["sozip"]["optimize"]
    alg["input"] = tmp_vsimem / "out.zip"
    alg["output"] = tmp_vsimem / "out2.zip"
    assert alg.Run()

    assert (
        gdal.VSIStatL(f"/vsizip/{tmp_vsimem}/out2.zip{tmp_vsimem}/a").size
        == 1024 * 1024 + 1
    )
    assert gdal.VSIStatL(f"/vsizip/{tmp_vsimem}/out2.zip{tmp_vsimem}/b").size == 40000
    assert (
        gdal.VSIStatL(f"/vsizip/{tmp_vsimem}/out2.zip{tmp_vsimem}/subdir/c").size == 1
    )

    alg = gdal.GetGlobalAlgorithmRegistry()["sozip"]["validate"]
    alg["input"] = tmp_vsimem / "out.zip"
    assert alg.Run()
    assert "a has a valid SOZip index, using chunk_size = 32768" in alg["output-string"]
    assert (
        "is a valid .zip file, and contains 1 SOZip-enabled file(s)"
        in alg["output-string"]
    )
