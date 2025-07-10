#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal dataset delete' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def test_gdalalg_dataset_delete(tmp_vsimem):

    gdal.Translate(tmp_vsimem / "in.tif", "../gcore/data/byte.tif")

    assert gdal.Run(
        "dataset",
        "delete",
        filename=tmp_vsimem / "in.tif",
    )

    assert gdal.VSIStatL(tmp_vsimem / "in.tif") is None


def test_gdalalg_dataset_delete_format(tmp_vsimem):

    gdal.Translate(tmp_vsimem / "in.tif", "../gcore/data/byte.tif")

    assert gdal.Run(
        "dataset",
        "delete",
        format="GTiff",
        filename=tmp_vsimem / "in.tif",
    )

    assert gdal.VSIStatL(tmp_vsimem / "in.tif") is None


def test_gdalalg_dataset_delete_error(tmp_vsimem):

    with pytest.raises(
        Exception, match="No identifiable driver for /i_do/not/exist.tif"
    ):
        gdal.Run(
            "dataset",
            "delete",
            filename="/i_do/not/exist.tif",
        )


def test_gdalalg_dataset_copy_complete():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out = gdaltest.runexternal(f"{gdal_path} completion gdal dataset delete --format=")
    assert "GTiff " in out
    assert "ESRI\\ Shapefile " in out
