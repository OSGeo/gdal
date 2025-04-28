#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal convert' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_convert_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    return reg.InstantiateAlg("convert")


def test_gdalalg_convert_raster(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.tif")

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(["data/utmsmall.tif", out_filename])

    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 50054


@pytest.mark.require_driver("GPKG")
def test_gdalalg_convert_vector(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(["../ogr/data/poly.shp", out_filename])

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_convert_on_raster_invalid_arg():
    convert = get_convert_alg()
    with pytest.raises(Exception, match="Option '--invalid' is unknown"):
        assert convert.ParseRunAndFinalize(
            ["--of=MEM", "--invalid", "data/utmsmall.tif", "out"]
        )


def test_gdalalg_convert_on_unrecognized_input():
    convert = get_convert_alg()
    with pytest.raises(Exception, match="not recognized as being"):
        assert convert.ParseRunAndFinalize(
            ["--of=MEM", "test_gdalalg_convert.py", "out.tif"]
        )
