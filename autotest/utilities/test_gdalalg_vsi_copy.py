#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vsi copy' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vsi"]["copy"]


def test_gdalalg_vsi_copy_single_dir_destination(tmp_vsimem):

    alg = get_alg()
    alg["source"] = "../gcore/data/byte.tif"
    alg["destination"] = tmp_vsimem
    assert alg.Run()
    assert gdal.VSIStatL(tmp_vsimem / "byte.tif").size == 736


def test_gdalalg_vsi_copy_single_file_destination(tmp_vsimem):

    alg = get_alg()
    alg["source"] = "../gcore/data/byte.tif"
    alg["destination"] = tmp_vsimem / "out.tif"
    assert alg.Run()
    assert gdal.VSIStatL(tmp_vsimem / "out.tif").size == 736


def test_gdalalg_vsi_copy_single_progress(tmp_vsimem):

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_alg()
    alg["source"] = "../gcore/data/byte.tif"
    alg["destination"] = tmp_vsimem
    assert alg.Run(my_progress)
    assert last_pct[0] == 1.0
    assert gdal.VSIStatL(tmp_vsimem / "byte.tif").size == 736


def test_gdalalg_vsi_copy_single_source_does_not_exist():

    alg = get_alg()
    alg["source"] = "/i_do/not/exist.bin"
    alg["destination"] = "/vsimem/"
    with pytest.raises(Exception, match="cannot be accessed"):
        alg.Run()


@pytest.mark.require_curl()
def test_gdalalg_vsi_copy_single_source_does_not_exist_vsi():

    with gdal.config_option("OSS_SECRET_ACCESS_KEY", ""):
        alg = get_alg()
        alg["source"] = "/vsioss/i_do_not/exist.bin"
        alg["destination"] = "/vsimem/"
        with pytest.raises(Exception, match="InvalidCredentials"):
            alg.Run()


@pytest.mark.require_curl()
def test_gdalalg_vsi_copy_recursive_source_does_not_exist_vsi():

    with gdal.config_option("OSS_SECRET_ACCESS_KEY", ""):
        alg = get_alg()
        alg["source"] = "/vsioss/i_do_not/exist.bin"
        alg["destination"] = "/vsimem/"
        alg["recursive"] = True
        with pytest.raises(Exception, match="InvalidCredentials"):
            alg.Run()


@pytest.mark.require_curl()
def test_gdalalg_vsi_copy_recursive_slash_star_source_does_not_exist_vsi():

    with gdal.config_option("OSS_SECRET_ACCESS_KEY", ""):
        alg = get_alg()
        alg["source"] = "/vsioss/i_do_not/exist.bin/*"
        alg["destination"] = "/vsimem/"
        alg["recursive"] = True
        with pytest.raises(Exception, match="InvalidCredentials"):
            alg.Run()


def test_gdalalg_vsi_copy_single_source_is_directory():

    alg = get_alg()
    alg["source"] = "../gcore"
    alg["destination"] = "/vsimem/"
    with pytest.raises(Exception, match="is a directory"):
        alg.Run()


def test_gdalalg_vsi_copy_recursive_destination_does_not_exist(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "src", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "src" / "a", "foo")
    gdal.Mkdir(tmp_vsimem / "src" / "subdir", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "src" / "subdir" / "b", "bar")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    alg = get_alg()
    alg["source"] = tmp_vsimem / "src"
    alg["destination"] = tmp_vsimem / "dst"
    alg["recursive"] = True
    assert alg.Run(my_progress)
    assert last_pct[0] == 1.0
    res = set(gdal.ReadDirRecursive(tmp_vsimem / "dst"))
    assert set(res) == set(gdal.ReadDirRecursive(tmp_vsimem / "src"))


def test_gdalalg_vsi_copy_recursive_destination_exists(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "src", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "src" / "a", "foo")
    gdal.Mkdir(tmp_vsimem / "src" / "subdir", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "src" / "subdir" / "b", "bar")

    gdal.Mkdir(tmp_vsimem / "dst", 0o755)

    alg = get_alg()
    alg["source"] = tmp_vsimem / "src"
    alg["destination"] = tmp_vsimem / "dst"
    alg["recursive"] = True
    assert alg.Run()
    res = set(gdal.ReadDirRecursive(tmp_vsimem / "dst"))
    res.remove("src/")
    assert set([x[len("src/") :] for x in res]) == set(
        gdal.ReadDirRecursive(tmp_vsimem / "src")
    )


def test_gdalalg_vsi_copy_recursive_source_ends_slash_star(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "src", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "src" / "a", "foo")
    gdal.Mkdir(tmp_vsimem / "src" / "subdir", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "src" / "subdir" / "b", "bar")

    alg = get_alg()
    alg["source"] = tmp_vsimem / "src" / "*"
    alg["destination"] = tmp_vsimem / "dst"
    alg["recursive"] = True
    assert alg.Run()
    res = set(gdal.ReadDirRecursive(tmp_vsimem / "dst"))
    assert set(res) == set(gdal.ReadDirRecursive(tmp_vsimem / "src"))


def test_gdalalg_vsi_copy_source_ends_slash_star(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "src", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "src" / "a", "foo")
    gdal.Mkdir(tmp_vsimem / "src" / "subdir", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "src" / "subdir" / "b", "bar")

    alg = get_alg()
    alg["source"] = tmp_vsimem / "src" / "*"
    alg["destination"] = tmp_vsimem / "dst"
    assert alg.Run()
    res = set(gdal.ReadDirRecursive(tmp_vsimem / "dst"))
    assert set(res) == set(["a", "subdir/"])


def test_gdalalg_vsi_copy_recursive_destination_cannot_be_created(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "src", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "src" / "a", "foo")
    gdal.Mkdir(tmp_vsimem / "src" / "subdir", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "src" / "subdir" / "b", "bar")

    alg = get_alg()
    alg["source"] = tmp_vsimem / "src"
    alg["destination"] = "/i_do/not/exist"
    alg["recursive"] = True
    with pytest.raises(Exception, match="Cannot create directory /i_do/not/exist"):
        alg.Run()


def test_gdalalg_vsi_copy_recursive_destination_cannot_be_created_skip(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "src", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "src" / "a", "foo")
    gdal.Mkdir(tmp_vsimem / "src" / "subdir", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "src" / "subdir" / "b", "bar")

    alg = get_alg()
    alg["source"] = tmp_vsimem / "src"
    alg["destination"] = "/i_do/not/exist"
    alg["recursive"] = True
    alg["skip-errors"] = True
    with gdal.quiet_errors():
        assert alg.Run()
