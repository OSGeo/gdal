#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsicached?
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import pytest

from osgeo import gdal


def test_vsicached_no_arg():

    with gdal.quiet_errors():
        assert gdal.VSIFOpenL("/vsicached?", "rb") is None


def test_vsicached_unknown_option_and_missing_file():

    with gdal.quiet_errors():
        assert gdal.VSIFOpenL("/vsicached?foo=bar", "rb") is None


def test_vsicached_unexisting_file():

    assert gdal.VSIFOpenL("/vsicached?file=/i/do/not/exist", "rb") is None


MAX_VAL_CHUNK = 1024 * 1024 * 1024
MAX_VAL_SIZE = (1 << 64) - 1


@pytest.mark.parametrize(
    "filename",
    [
        "/vsicached?file=./data/byte.tif",
        "/vsicached?chunk_size=16384&file=./data/byte.tif",
        "/vsicached?chunk_size=16KB&file=./data/byte.tif",
        "/vsicached?chunk_size=1MB&file=./data/byte.tif",
        "/vsicached?chunk_size=%dKB&file=./data/byte.tif" % (((MAX_VAL_CHUNK) // 1024)),
        "/vsicached?chunk_size=%dMB&file=./data/byte.tif"
        % (((MAX_VAL_CHUNK) // (1024 * 1024))),
    ],
)
def test_vsicached_ok(filename):
    f = gdal.VSIFOpenL(filename, "rb")
    assert f
    try:
        gdal.VSIFSeekL(f, 0, 2)
        assert gdal.VSIFTellL(f) == os.stat("data/byte.tif").st_size
    finally:
        gdal.VSIFCloseL(f)


@pytest.mark.parametrize(
    "filename",
    [
        "/vsicached?cache_size=&file=./data/byte.tif",
        "/vsicached?cache_size=1invalidsuffix&file=./data/byte.tif",
        "/vsicached?cache_size=invalid&file=./data/byte.tif",
        "/vsicached?cache_size=%d&file=./data/byte.tif" % MAX_VAL_SIZE,
        "/vsicached?cache_size=%dKB&file=./data/byte.tif"
        % ((MAX_VAL_SIZE + 1) // 1024),
        "/vsicached?cache_size=%dMB&file=./data/byte.tif"
        % ((MAX_VAL_SIZE + 1) // (1024 * 1024)),
        "/vsicached?chunk_size=&file=./data/byte.tif",
        "/vsicached?chunk_size=1invalidsuffix&file=./data/byte.tif",
        "/vsicached?chunk_size=invalid&file=./data/byte.tif",
        "/vsicached?chunk_size=%d&file=./data/byte.tif" % MAX_VAL_CHUNK,
        "/vsicached?chunk_size=%dKB&file=./data/byte.tif"
        % (1 + ((MAX_VAL_CHUNK) // 1024)),
        "/vsicached?chunk_size=%dMB&file=./data/byte.tif"
        % (1 + ((MAX_VAL_CHUNK) // (1024 * 1024))),
    ],
)
def test_vsicached_wrong_size(filename):

    with pytest.raises(Exception, match="Invalid value for"):
        gdal.VSIFOpenL(filename, "rb")


def test_vsicached_update_not_supported():

    assert gdal.VSIFOpenL("/vsicached?file=./data/byte.tif", "rb+") is None


def test_vsicached_stat_ok():

    assert (
        gdal.VSIStatL("/vsicached?file=./data/byte.tif").size
        == os.stat("data/byte.tif").st_size
    )


def test_vsicached_stat_error():

    with gdal.quiet_errors():
        assert gdal.VSIStatL("/vsicached?") is None


def test_vsicached_readdir_ok():

    assert gdal.ReadDir("/vsicached?file=./data") == gdal.ReadDir("./data")


def test_vsicached_readdir_error():

    with gdal.quiet_errors():
        assert gdal.ReadDir("/vsicached?") is None
