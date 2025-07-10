#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vsi delete' testing
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


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vsi"]["delete"]


def test_gdalalg_vsi_delete_file(tmp_vsimem):

    gdal.FileFromMemBuffer(tmp_vsimem / "test", "test")

    alg = get_alg()
    alg["filename"] = tmp_vsimem / "test"
    assert alg.Run()

    assert gdal.VSIStatL(tmp_vsimem / "test") is None


def test_gdalalg_vsi_delete_file_not_existing():

    alg = get_alg()
    alg["filename"] = "/i_do/not/exist"
    with pytest.raises(Exception, match="does not exist"):
        alg.Run()


def test_gdalalg_vsi_delete_dir(tmp_path):

    gdal.Mkdir(tmp_path / "subdir", 0o755)

    alg = get_alg()
    alg["filename"] = tmp_path / "subdir"
    assert alg.Run()

    assert gdal.VSIStatL(tmp_path / "subdir") is None


@pytest.mark.skipif(sys.platform == "win32", reason="incompatible platform")
def test_gdalalg_vsi_delete_file_failed():

    if os.getuid() == 0:
        pytest.skip("running as root... skipping")

    alg = get_alg()
    alg["filename"] = "/dev/null"
    with pytest.raises(Exception, match="Cannot delete /dev/null"):
        alg.Run()


def test_gdalalg_vsi_delete_dir_recursive(tmp_path):

    gdal.Mkdir(tmp_path / "subdir", 0o755)
    open(tmp_path / "subdir" / "file", "wb").close()

    alg = get_alg()
    alg["filename"] = tmp_path / "subdir"
    alg["recursive"] = True
    assert alg.Run()

    assert gdal.VSIStatL(tmp_path / "subdir") is None


@pytest.mark.require_curl()
def test_gdalalg_vsi_delete_source_does_not_exist_vsi():

    with gdal.config_option("OSS_SECRET_ACCESS_KEY", ""):
        alg = get_alg()
        alg["filename"] = "/vsioss/i_do_not/exist.bin"
        with pytest.raises(Exception, match="InvalidCredentials"):
            alg.Run()
