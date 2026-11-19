#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalmdiminfo testing
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest
import test_cli_utilities

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdalmdiminfo_path() is None,
    reason="gdalmdiminfo not available",
)


@pytest.fixture()
def gdalmdiminfo_path():
    return test_cli_utilities.get_gdalmdiminfo_path()


###############################################################################
# Simple test


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdiminfo_1(gdalmdiminfo_path):

    ret, err = gdaltest.runexternal_out_and_err(gdalmdiminfo_path + " data/mdim.vrt")
    assert err is None or err == "", "got error/warning"
    assert '"type": "group"' in ret


###############################################################################
# Test -if option


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalmdiminfo_if_option(gdalmdiminfo_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalmdiminfo_path + " -if VRT data/mdim.vrt"
    )
    assert err is None or err == "", "got error/warning"
    assert '"type": "group"' in ret

    _, err = gdaltest.runexternal_out_and_err(
        gdalmdiminfo_path + " -if i_do_not_exist data/mdim.vrt"
    )
    assert "i_do_not_exist is not a recognized driver" in err
