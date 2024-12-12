#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal' program testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json

import gdaltest
import pytest
import test_cli_utilities

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdal_path() is None, reason="gdal binary not available"
)


@pytest.fixture()
def gdal_path():
    return test_cli_utilities.get_gdal_path()


def test_gdal_no_argument(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(f"{gdal_path}")
    assert out == ""
    assert "gdal: Missing command name" in err
    assert "Usage: " in err
    assert "ret code = 1" in err


def test_gdal_help(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(f"{gdal_path} --help")
    assert out.startswith("Usage: ")
    assert err == ""


def test_gdal_json_usage(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(f"{gdal_path} --json-usage")
    assert out.startswith("{")
    assert err == ""
    assert "description" in json.loads(out)


def test_gdal_invalid_command_line(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(f"{gdal_path} --invalid")
    assert out == ""
    assert "Long name option '--invalid' is unknown" in err
    assert "Usage: " in err
    assert "ret code = 1" in err


def test_gdal_failure_during_run(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster info i_do_not_exist"
    )
    assert out == ""
    assert "i_do_not_exist:" in err
    assert "Usage: " in err
    assert "ret code = 1" in err


def test_gdal_success(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(f"{gdal_path} data/utmsmall.tif")
    assert err == ""
    assert "description" in json.loads(out)


def test_gdal_failure_during_finalize(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster convert ../gcore/data/byte.tif /vsimem/out.tif||maxlength=100"
    )
    assert "ret code = 1" in err
