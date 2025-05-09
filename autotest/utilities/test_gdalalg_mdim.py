#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal mdim' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def test_gdalalg_mdim_run_error():
    with pytest.raises(Exception, match="method should not be called directly"):
        gdal.Run(["mdim"])


def test_gdalalg_mdim_drivers():

    with gdal.Run(["mdim"], drivers=True) as alg:
        j = alg.Output()
        assert "VRT" in [x["short_name"] for x in j]
        assert "GTiff" not in [x["short_name"] for x in j]
        assert "ESRI Shapefile" not in [x["short_name"] for x in j]
