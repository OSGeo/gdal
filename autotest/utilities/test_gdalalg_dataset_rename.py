#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal dataset rename' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def test_gdalalg_dataset_rename(tmp_vsimem):

    gdal.Translate(tmp_vsimem / "in.tif", "../gcore/data/byte.tif")

    assert gdal.Run(
        "dataset",
        "rename",
        source=tmp_vsimem / "in.tif",
        destination=tmp_vsimem / "out.tif",
    )

    with gdal.Open(tmp_vsimem / "out.tif") as ds:
        assert ds.GetDriver().GetDescription() == "GTiff"

    assert gdal.VSIStatL(tmp_vsimem / "in.tif") is None


def test_gdalalg_dataset_rename_error(tmp_vsimem):

    with pytest.raises(
        Exception, match="No identifiable driver for /i_do/not/exist.tif"
    ):
        gdal.Run(
            "dataset",
            "rename",
            source="/i_do/not/exist.tif",
            destination=tmp_vsimem / "out.tif",
        )
