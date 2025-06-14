#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vsi sozip' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import sys

import pytest

from osgeo import gdal


def test_gdalalg_vsi_sozip():
    with pytest.raises(Exception, match="should not be called directly"):
        gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"].Run()


def test_gdalalg_vsi_sozip_create_no_zip_extension():

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["create"]
    with pytest.raises(Exception, match="Extension of zip filename should be .zip"):
        alg["output"] = "foo"


def create_source_files(tmp_vsimem):

    gdal.FileFromMemBuffer(tmp_vsimem / "a", "x" * (1024 * 1024 + 1))
    gdal.FileFromMemBuffer(tmp_vsimem / "b", "x" * 40000)
    gdal.Mkdir(tmp_vsimem / "subdir", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "subdir" / "c", "x")


@pytest.mark.parametrize("enable_sozip", ["auto", "yes", "no"])
def test_gdalalg_vsi_sozip_create(tmp_vsimem, enable_sozip):

    create_source_files(tmp_vsimem)

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["create"]
    alg["input"] = tmp_vsimem / "b"
    alg["output"] = tmp_vsimem / "out.zip"
    alg["quiet"] = True
    alg["no-paths"] = True
    alg["enable-sozip"] = enable_sozip
    alg["sozip-chunk-size"] = 16384
    assert alg.Run()
    assert alg["output-string"] == ""

    assert gdal.VSIStatL(f"/vsizip/{tmp_vsimem}/out.zip/b").size == 40000

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["validate"]
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

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["create"]
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


def test_gdalalg_vsi_sozip_create_non_existing_input(tmp_vsimem):

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["create"]
    alg["input"] = tmp_vsimem / "non_existing"
    alg["output"] = tmp_vsimem / "out.zip"
    with pytest.raises(Exception, match="does not exist"):
        assert alg.Run()


def test_gdalalg_vsi_sozip_create_non_existing_input_with_progress(tmp_vsimem):
    def my_progress(pct, msg, user_data):
        return True

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["create"]
    alg["input"] = tmp_vsimem / "non_existing"
    alg["output"] = tmp_vsimem / "out.zip"
    with pytest.raises(Exception, match="does not exist"):
        assert alg.Run(my_progress)


def test_gdalalg_vsi_sozip_create_input_is_directory(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "dir", 0o755)

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["create"]
    alg["input"] = tmp_vsimem / "dir"
    alg["output"] = tmp_vsimem / "out.zip"
    with pytest.raises(Exception, match="is a directory"):
        assert alg.Run()


@pytest.mark.skipif(sys.platform != "linux", reason="not linux")
def test_gdalalg_vsi_sozip_create_failed_adding(tmp_path):

    if os.getuid() == 0:
        pytest.skip("running as root... skipping")

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["create"]
    input_filename = tmp_path / "cannot_read"
    open(input_filename, "wb").close()
    os.chmod(input_filename, 0)
    alg["input"] = input_filename
    alg["output"] = tmp_path / "out.zip"
    with pytest.raises(Exception, match=f"Failed adding {input_filename}"):
        assert alg.Run()


def test_gdalalg_vsi_sozip_create_recursive_and_optimize_and_validate(tmp_vsimem):

    create_source_files(tmp_vsimem)

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["create"]
    alg["input"] = tmp_vsimem
    alg["recursive"] = True
    alg["output"] = tmp_vsimem / "out.zip"

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    assert alg.Run(my_progress)

    assert last_pct[0] == 1.0

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

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["optimize"]
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

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["optimize"]
    alg["input"] = tmp_vsimem / "out.zip"
    alg["output"] = tmp_vsimem / "out2.zip"
    with pytest.raises(Exception, match="already exists. Use --overwrite"):
        alg.Run()

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["optimize"]
    alg["input"] = tmp_vsimem / "i_do_not_exist.zip"
    alg["output"] = tmp_vsimem / "out3.zip"
    with pytest.raises(Exception, match="is not a valid .zip file"):
        alg.Run()

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["validate"]
    alg["input"] = tmp_vsimem / "out.zip"
    assert alg.Run()
    assert "a has a valid SOZip index, using chunk_size = 32768" in alg["output-string"]
    assert (
        "is a valid .zip file, and contains 1 SOZip-enabled file(s)"
        in alg["output-string"]
    )


def test_gdalalg_vsi_sozip_list_not_a_zip():

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["list"]
    alg["input"] = "/i_do/not/exist.zip"
    with pytest.raises(Exception, match="is not a valid .zip file"):
        assert alg.Run()


def test_gdalalg_vsi_sozip_validate_not_a_zip():

    alg = gdal.GetGlobalAlgorithmRegistry()["vsi"]["sozip"]["validate"]
    alg["input"] = "/i_do/not/exist.zip"
    with pytest.raises(Exception, match="is not a valid .zip file"):
        assert alg.Run()
