#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  Test gdal_fsspec module
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 20124, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal

fsspec = pytest.importorskip("fsspec")
pytest.importorskip("fsspec.spec")

from osgeo import gdal_fsspec  # NOQA


def test_gdal_fsspec_open_read():

    with fsspec.open("gdalvsi://data/byte.tif") as f:
        assert len(f.read()) == gdal.VSIStatL("data/byte.tif").size


def test_gdal_fsspec_info_file():

    fs = fsspec.filesystem("gdalvsi")
    info = fs.info("data/byte.tif")
    assert "mtime" in info
    del info["mtime"]
    assert (info["mode"] & 32768) != 0
    del info["mode"]
    assert info == {
        "name": "data/byte.tif",
        "size": 736,
        "type": "file",
    }


def test_gdal_fsspec_info_dir():

    fs = fsspec.filesystem("gdalvsi")
    info = fs.info("data")
    assert (info["mode"] & 16384) != 0
    del info["mode"]
    assert info == {
        "name": "data",
        "size": 0,
        "type": "directory",
    }


def test_gdal_fsspec_info_error():

    fs = fsspec.filesystem("gdalvsi")
    with pytest.raises(FileNotFoundError):
        fs.info("/i/do/not/exist")


def test_gdal_fsspec_ls():

    fs = fsspec.filesystem("gdalvsi")
    ret = fs.ls("data")
    assert len(ret) > 2
    item_of_interest = None
    for item in ret:
        if item["name"] == "data/byte.tif":
            item_of_interest = item
            break
    assert item_of_interest
    assert "mtime" in item_of_interest
    del item_of_interest["mtime"]
    assert item_of_interest == {
        "name": "data/byte.tif",
        "size": 736,
        "type": "file",
    }


def test_gdal_fsspec_ls_file():

    fs = fsspec.filesystem("gdalvsi")
    ret = fs.ls("data/byte.tif")
    assert ret == ["data/byte.tif"]


def test_gdal_fsspec_ls_error():

    fs = fsspec.filesystem("gdalvsi")
    with pytest.raises(FileNotFoundError):
        fs.ls("gdalvsi://i/do/not/exist")


def test_gdal_fsspec_modified():

    fs = fsspec.filesystem("gdalvsi")
    modified = fs.modified("data/byte.tif")
    assert modified is not None
    import datetime

    assert isinstance(modified, datetime.datetime)


def test_gdal_fsspec_modified_error():

    fs = fsspec.filesystem("gdalvsi")
    with pytest.raises(FileNotFoundError):
        fs.modified("gdalvsi://i/do/not/exist")


def test_gdal_fsspec_rm():

    with fsspec.open("gdalvsi:///vsimem/foo.bin", "wb") as f:
        f.write(b"""bar""")
    fs = fsspec.filesystem("gdalvsi")
    fs.info("/vsimem/foo.bin")
    fs.rm("/vsimem/foo.bin")
    with pytest.raises(FileNotFoundError):
        fs.info("/vsimem/foo.bin")


def test_gdal_fsspec_rm_error():

    fs = fsspec.filesystem("gdalvsi")
    with pytest.raises(FileNotFoundError):
        fs.rm("/vsimem/foo.bin")


def test_gdal_fsspec_copy():

    with fsspec.open("gdalvsi:///vsimem/foo.bin", "wb") as f:
        f.write(b"""bar""")
    fs = fsspec.filesystem("gdalvsi")
    fs.copy("/vsimem/foo.bin", "/vsimem/bar.bin")
    assert fs.info("/vsimem/bar.bin")["size"] == 3
    assert fs.info("/vsimem/foo.bin")["size"] == 3
    fs.rm("/vsimem/foo.bin")
    fs.rm("/vsimem/bar.bin")


def test_gdal_fsspec_copy_error():

    fs = fsspec.filesystem("gdalvsi")
    with pytest.raises(FileNotFoundError):
        fs.copy("/vsimem/foo.bin", "/vsimem/bar.bin")


def test_gdal_fsspec_mv():

    with fsspec.open("gdalvsi:///vsimem/foo.bin", "wb") as f:
        f.write(b"""bar""")
    fs = fsspec.filesystem("gdalvsi")
    fs.mv("/vsimem/foo.bin", "/vsimem/bar.bin")
    assert fs.info("/vsimem/bar.bin")["size"] == 3
    with pytest.raises(FileNotFoundError):
        fs.info("/vsimem/foo.bin")
    fs.rm("/vsimem/bar.bin")


def test_gdal_fsspec_mv_error():

    fs = fsspec.filesystem("gdalvsi")
    with pytest.raises(FileNotFoundError):
        fs.mv("/vsimem/foo.bin", "/bar.bin")


def test_gdal_fsspec_mkdir(tmp_path):

    fs = fsspec.filesystem("gdalvsi")

    my_path = str(tmp_path) + "/my_dir"

    fs.mkdir(my_path)
    assert fs.info(my_path)["type"] == "directory"
    with pytest.raises(FileExistsError):
        fs.mkdir(my_path)
    fs.rmdir(my_path)

    fs.mkdir(my_path + "/my_subdir")
    assert fs.info(my_path)["type"] == "directory"
    assert fs.info(my_path + "/my_subdir")["type"] == "directory"
    fs.rmdir(my_path + "/my_subdir")
    fs.rmdir(my_path)
    with pytest.raises(FileNotFoundError):
        fs.info(my_path)

    fs = fsspec.filesystem("gdalvsi")
    with pytest.raises(Exception):
        fs.mkdir(my_path + "/my_subdir", create_parents=False)
    with pytest.raises(FileNotFoundError):
        fs.info(my_path)


def test_gdal_fsspec_makedirs(tmp_path):

    fs = fsspec.filesystem("gdalvsi")

    my_path = str(tmp_path) + "/my_dir"
    fs.makedirs(my_path)
    assert fs.info(my_path)["type"] == "directory"
    with pytest.raises(FileExistsError):
        fs.makedirs(my_path)
    fs.makedirs(my_path, exist_ok=True)
    fs.rmdir(my_path)


def test_gdal_fsspec_usable_by_pyarrow_dataset(tmp_vsimem):

    ds = pytest.importorskip("pyarrow.dataset")

    tmp_vsimem_file = str(tmp_vsimem / "tmp.parquet")
    gdal.FileFromMemBuffer(
        tmp_vsimem_file, open("../ogr/data/parquet/test.parquet", "rb").read()
    )

    fs_vsimem = fsspec.filesystem("gdalvsi")

    assert ds.dataset(tmp_vsimem_file, filesystem=fs_vsimem) is not None

    assert ds.dataset(str(tmp_vsimem), filesystem=fs_vsimem) is not None
