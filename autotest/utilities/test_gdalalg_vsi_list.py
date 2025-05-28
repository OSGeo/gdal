#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vsi list' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import os

import pytest

from osgeo import gdal


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vsi"]["list"]


def del_last_modification_date(j):
    if isinstance(j, list):
        for subj in j:
            del_last_modification_date(subj)
    elif isinstance(j, dict):
        if "last_modification_date" in j:
            del j["last_modification_date"]
        for k in j:
            del_last_modification_date(j[k])


def test_gdalalg_vsi_list(tmp_vsimem):

    alg = get_alg()
    alg["filename"] = tmp_vsimem / "i_do_not_exist"
    with pytest.raises(Exception):
        alg.Run()

    alg = get_alg()
    alg["filename"] = tmp_vsimem
    assert alg.Run()
    assert json.loads(alg["output-string"]) == []

    gdal.FileFromMemBuffer(tmp_vsimem / "a", "a")
    gdal.Mkdir(tmp_vsimem / "subdir", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "subdir" / "b", "b")
    gdal.FileFromMemBuffer(tmp_vsimem / "subdir" / "c", "c")
    gdal.Mkdir(tmp_vsimem / "subdir" / "subsubdir", 0o755)
    gdal.FileFromMemBuffer(tmp_vsimem / "d", "d")

    alg = get_alg()
    alg["filename"] = tmp_vsimem
    assert alg.Run()
    assert json.loads(alg["output-string"]) == ["a", "d", "subdir"]

    alg = get_alg()
    alg["filename"] = tmp_vsimem / "a"
    assert alg.Run()
    assert json.loads(alg["output-string"]) == str(tmp_vsimem / "a")

    alg = get_alg()
    alg["filename"] = tmp_vsimem
    alg["recursive"] = True
    assert alg.Run()
    assert json.loads(alg["output-string"]) == [
        "a",
        "d",
        "subdir",
        "subdir/b",
        "subdir/c",
        "subdir/subsubdir",
    ]

    alg = get_alg()
    alg["filename"] = tmp_vsimem
    alg["recursive"] = True
    alg["depth"] = 1
    assert alg.Run()
    assert json.loads(alg["output-string"]) == [
        "a",
        "d",
        "subdir",
    ]

    alg = get_alg()
    alg["filename"] = tmp_vsimem
    alg["recursive"] = True
    alg["depth"] = 2
    assert alg.Run()
    assert json.loads(alg["output-string"]) == [
        "a",
        "d",
        "subdir",
        "subdir/b",
        "subdir/c",
        "subdir/subsubdir",
    ]

    alg = get_alg()
    alg["filename"] = tmp_vsimem
    alg["long-listing"] = True
    assert alg.Run()
    j = json.loads(alg["output-string"])
    del_last_modification_date(j)

    assert j == [
        {
            "name": "a",
            "permissions": "----------",
            "size": 1,
            "type": "file",
        },
        {
            "name": "d",
            "permissions": "----------",
            "size": 1,
            "type": "file",
        },
        {
            "name": "subdir",
            "permissions": "d---------",
            "size": 0,
            "type": "directory",
        },
    ]

    alg = get_alg()
    alg["filename"] = tmp_vsimem
    alg["recursive"] = True
    alg["long-listing"] = True
    assert alg.Run()
    j = json.loads(alg["output-string"])
    del_last_modification_date(j)

    assert j == [
        {
            "name": "a",
            "permissions": "----------",
            "size": 1,
            "type": "file",
        },
        {
            "name": "d",
            "permissions": "----------",
            "size": 1,
            "type": "file",
        },
        {
            "name": "subdir",
            "permissions": "d---------",
            "size": 0,
            "type": "directory",
        },
        {
            "name": "subdir/b",
            "permissions": "----------",
            "size": 1,
            "type": "file",
        },
        {
            "name": "subdir/c",
            "permissions": "----------",
            "size": 1,
            "type": "file",
        },
        {
            "name": "subdir/subsubdir",
            "permissions": "d---------",
            "size": 0,
            "type": "directory",
        },
    ]

    alg = get_alg()
    alg["filename"] = tmp_vsimem
    alg["recursive"] = True
    alg["long-listing"] = True
    alg["tree"] = True
    assert alg.Run()
    j = json.loads(alg["output-string"])
    del_last_modification_date(j)

    assert j == [
        {
            "name": "a",
            "permissions": "----------",
            "size": 1,
            "type": "file",
        },
        {
            "name": "d",
            "permissions": "----------",
            "size": 1,
            "type": "file",
        },
        {
            "name": "subdir",
            "permissions": "d---------",
            "size": 0,
            "type": "directory",
            "entries": [
                {
                    "name": "b",
                    "permissions": "----------",
                    "size": 1,
                    "type": "file",
                },
                {
                    "name": "c",
                    "permissions": "----------",
                    "size": 1,
                    "type": "file",
                },
                {
                    "entries": [],
                    "name": "subsubdir",
                    "permissions": "d---------",
                    "size": 0,
                    "type": "directory",
                },
            ],
        },
    ]

    alg = get_alg()
    alg["filename"] = tmp_vsimem
    alg["recursive"] = True
    alg["tree"] = True
    assert alg.Run()
    j = json.loads(alg["output-string"])
    del_last_modification_date(j)

    assert j == [
        "a",
        "d",
        {
            "name": "subdir",
            "entries": [
                "b",
                "c",
                {
                    "entries": [],
                    "name": "subsubdir",
                },
            ],
        },
    ]

    alg = get_alg()
    alg["filename"] = tmp_vsimem
    alg["recursive"] = True
    alg["format"] = "text"
    alg["absolute-path"] = True
    assert alg.Run()
    assert alg["output-string"][0:-1].split("\n") == [
        str(tmp_vsimem) + "/" + x
        for x in [
            "a",
            "d",
            "subdir",
            "subdir/b",
            "subdir/c",
            "subdir/subsubdir",
        ]
    ]

    alg = get_alg()
    alg["filename"] = "data"
    assert alg.Run()
    assert "utmsmall.tif" in json.loads(alg["output-string"])

    alg = get_alg()
    alg["filename"] = "."
    assert alg.Run()

    alg = get_alg()
    alg["filename"] = "data"
    alg["absolute-path"] = True
    assert alg.Run()
    assert os.path.join(os.getcwd(), "data", "utmsmall.tif").replace("\\", "/") in [
        x.replace("\\", "/") for x in json.loads(alg["output-string"])
    ]

    alg = get_alg()
    alg["filename"] = "data"
    alg["long-listing"] = True
    alg["format"] = "text"
    assert alg.Run()
    assert "unknown unknown" in alg["output-string"]
    assert "utmsmall.tif" in alg["output-string"]


@pytest.mark.require_curl()
def test_gdalalg_vsi_list_source_does_not_exist_vsi():

    with gdal.config_option("OSS_SECRET_ACCESS_KEY", ""):
        alg = get_alg()
        alg["filename"] = "/vsioss/i_do_not/exist.bin"
        with pytest.raises(Exception, match="InvalidCredentials"):
            alg.Run()
