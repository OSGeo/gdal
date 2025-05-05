#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_vector_alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    return reg.InstantiateAlg("vector")


def test_gdalalg_vector_run_error():
    info = get_vector_alg()
    with pytest.raises(Exception, match="method should not be called directly"):
        info.Run()


def test_gdalalg_vector_drivers():

    with gdal.Run(["vector"], drivers=True) as alg:
        j = alg.Output()
        assert "GTiff" not in [x["short_name"] for x in j]
        assert "ESRI Shapefile" in [x["short_name"] for x in j]
