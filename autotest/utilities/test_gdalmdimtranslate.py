#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test command line gdalmdimtranslate
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest
import test_cli_utilities

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdalmdimtranslate_path() is None,
    reason="gdalmdimtranslate not available",
)


@pytest.fixture()
def gdalmdimtranslate_path():
    return test_cli_utilities.get_gdalmdimtranslate_path()


###############################################################################
# Simple test


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_1(gdalmdimtranslate_path, tmp_path):

    dst_vrt = str(tmp_path / "out.vrt")

    ret, err = gdaltest.runexternal_out_and_err(
        f"{gdalmdimtranslate_path} data/mdim.vrt {dst_vrt}"
    )
    assert err is None or err == "", "got error/warning"
    assert os.path.exists(dst_vrt)


###############################################################################
# Test -if option


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdimtranslate_if(gdalmdimtranslate_path, tmp_path):

    dst_vrt = str(tmp_path / "out.vrt")

    ret, err = gdaltest.runexternal_out_and_err(
        f"{gdalmdimtranslate_path} -if VRT data/mdim.vrt {dst_vrt}"
    )
    assert err is None or err == "", "got error/warning"
    assert os.path.exists(dst_vrt)


def test_gdalmdimtranslate_if_error(gdalmdimtranslate_path, tmp_path):

    dst_vrt = str(tmp_path / "out.vrt")

    ret, err = gdaltest.runexternal_out_and_err(
        f"{gdalmdimtranslate_path} -if i_do_not_exist data/mdim.vrt {dst_vrt}"
    )
    assert "i_do_not_exist is not a recognized driver" in err
    assert not os.path.exists(dst_vrt)
