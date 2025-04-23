#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test gdal.LineOfSightVisible algorithm.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def test_los_basic():

    mem_ds = gdal.GetDriverByName("MEM").Create("", 2, 1)

    res = gdal.IsLineOfSightVisible(mem_ds.GetRasterBand(1), 0, 0, 1, 1, 0, 1)
    assert res.is_visible
    assert res.col_intersection == -1
    assert res.row_intersection == -1

    assert gdal.IsLineOfSightVisible(
        mem_ds.GetRasterBand(1), 0, 0, 1, 0, 0, 1
    ).is_visible
    assert not gdal.IsLineOfSightVisible(
        mem_ds.GetRasterBand(1), 0, 0, -1, 1, 0, 1
    ).is_visible
    assert not gdal.IsLineOfSightVisible(
        mem_ds.GetRasterBand(1), 0, 0, 1, 1, 0, -1
    ).is_visible

    with pytest.raises(Exception, match="Received a NULL pointer"):
        gdal.IsLineOfSightVisible(None, 0, 0, 0, 0, 0, 0)

    with pytest.raises(Exception, match="Access window out of range in RasterIO()"):
        gdal.IsLineOfSightVisible(mem_ds.GetRasterBand(1), 0, 0, 1, 2, 0, 1)
