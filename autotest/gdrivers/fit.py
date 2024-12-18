#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for FIT driver.
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("FIT")


@pytest.mark.parametrize(
    "filename", ["byte", "int16", "uint16", "int32", "uint32", "float32", "float64"]
)
def test_fit(filename):
    fitDriver = gdal.GetDriverByName("FIT")

    ds = gdal.Open("../gcore/data/" + filename + ".tif")
    fitDriver.CreateCopy("tmp/" + filename + ".fit", ds, options=["PAGESIZE=2,2"])

    ds2 = gdal.Open("tmp/" + filename + ".fit")
    assert ds2.GetRasterBand(1).Checksum() == ds.GetRasterBand(1).Checksum()

    assert ds2.GetRasterBand(1).DataType == ds.GetRasterBand(1).DataType
    ds2 = None
