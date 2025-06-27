#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for all datatypes from a TileDB array.
# Author:   TileDB, Inc
#
###############################################################################
# Copyright (c) 2019, TileDB, Inc
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("TileDB")


def test_tiledb_open():
    ut = gdaltest.GDALTest("TileDB", "tiledb_array", 1, 4857)
    ut.testOpen()


def test_tiledb_open_does_not_exist():
    with pytest.raises(Exception, match="Failed to open"):
        gdal.OpenEx("tiledb_does_not_exist_array", gdal.OF_READONLY, ["TileDB"])


###############################################################################


def test_tiledb_force_identify():

    drv = gdal.IdentifyDriverEx("data/tiledb_array", allowed_drivers=["TileDB"])
    assert drv is not None
