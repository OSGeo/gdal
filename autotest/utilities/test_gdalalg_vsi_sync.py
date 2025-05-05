#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vsi sync' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vsi"]["sync"]


def test_gdalalg_vsi_sync_empty_source():

    alg = get_alg()
    with pytest.raises(Exception, match="Source filename cannot be empty"):
        alg["source"] = ""


def test_gdalalg_vsi_sync_empty_destination():

    alg = get_alg()
    with pytest.raises(Exception, match="Destination filename cannot be empty"):
        alg["destination"] = ""


def test_gdalalg_vsi_sync_nominal(tmp_vsimem, tmp_path):

    gdal.FileFromMemBuffer(tmp_vsimem / "file.bin", "foo")

    alg = get_alg()
    alg["source"] = tmp_vsimem / "file.bin"
    alg["destination"] = tmp_path / "dest.bin"
    assert alg.Run()
    assert gdal.VSIStatL(tmp_path / "dest.bin").size == 3


def test_gdalalg_vsi_sync_source_does_not_exist(tmp_vsimem):

    alg = get_alg()
    alg["source"] = tmp_vsimem / "i_do_not_exist.bin"
    alg["destination"] = tmp_vsimem / "dest.bin"
    with pytest.raises(Exception, match="does not exist"):
        alg.Run()


def test_gdalalg_vsi_sync_error(tmp_vsimem, tmp_path):

    gdal.FileFromMemBuffer(tmp_vsimem / "file.bin", "foo")

    alg = get_alg()
    alg["source"] = tmp_vsimem / "file.bin"
    alg["destination"] = tmp_path / "i_do" / "not" / "exist" / "dest.bin"
    with pytest.raises(Exception, match="Cannot create"):
        alg.Run()
