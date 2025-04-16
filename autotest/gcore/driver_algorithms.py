#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  Check that drivers that declare algorithms actually implement them
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import pytest

from osgeo import gdal


def check(alg, expected_name):
    assert alg.GetName() == expected_name
    if alg.HasSubAlgorithms():
        for subalg_name in alg.GetSubAlgorithmNames():
            subalg = alg[subalg_name]
            assert subalg, subalg_name
            check(subalg, subalg_name)

            assert alg.InstantiateSubAlgorithm("i_do_not_exist") is None


def test_driver_algorithms():

    try:
        alg = gdal.GetGlobalAlgorithmRegistry()["driver"]
    except Exception:
        pytest.skip("no drivers")
    check(alg, "driver")
