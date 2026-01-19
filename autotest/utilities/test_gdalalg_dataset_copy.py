#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal dataset copy' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def test_gdalalg_dataset_copy(tmp_vsimem):

    assert gdal.Run(
        "dataset",
        "copy",
        source="../gcore/data/byte.tif",
        destination=tmp_vsimem / "out.tif",
    )

    with gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.GetDriver().GetDescription() == "GTiff"

    with pytest.raises(
        Exception,
        match="already exists. Specify the --overwrite option to overwrite it",
    ):
        gdal.Run(
            "dataset",
            "copy",
            source="../gcore/data/byte.tif",
            destination=tmp_vsimem / "out.tif",
        )

    gdal.FileFromMemBuffer(tmp_vsimem / "out.tif", "")

    assert gdal.Run(
        "dataset",
        "copy",
        format="GTiff",
        source="../gcore/data/byte.tif",
        destination=tmp_vsimem / "out.tif",
        overwrite=True,
    )

    with gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.GetDriver().GetDescription() == "GTiff"


def test_gdalalg_dataset_overwrite_existing_directory(tmp_vsimem):

    gdal.Mkdir(tmp_vsimem / "my_dir", 0o755)

    with pytest.raises(
        Exception,
        match="already exists, but is not recognized as a valid GDAL dataset. Please manually delete it before retrying",
    ):
        gdal.Run(
            "dataset",
            "copy",
            source="../gcore/data/byte.tif",
            destination=tmp_vsimem / "my_dir",
            overwrite=True,
        )


@pytest.mark.require_driver("OpenFileGDB")
def test_gdalalg_dataset_overwrite_existing_dataset_directory(tmp_vsimem):

    gdal.GetDriverByName("OpenFileGDB").CreateVector(tmp_vsimem / "out.gdb")

    gdal.Run(
        "dataset",
        "copy",
        source="../gcore/data/byte.tif",
        destination=tmp_vsimem / "out.gdb/byte.tif",
        overwrite=True,
    )

    with gdal.Open(tmp_vsimem / "out.gdb/byte.tif") as ds:
        assert ds.GetDriver().GetDescription() == "GTiff"


def test_gdalalg_dataset_copy_error(tmp_vsimem):

    with pytest.raises(
        Exception, match="No identifiable driver for /i_do/not/exist.tif"
    ):
        gdal.Run(
            "dataset",
            "copy",
            source="/i_do/not/exist.tif",
            destination=tmp_vsimem / "out.tif",
        )


def test_gdalalg_dataset_copy_complete():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out = gdaltest.runexternal(f"{gdal_path} completion gdal dataset copy --format=")
    assert "GTiff " in out
    assert "ESRI\\ Shapefile " in out
