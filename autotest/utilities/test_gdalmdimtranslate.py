#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test command line gdalmdimtranslate
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
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


def test_gdalmdimtranslate_1(gdalmdimtranslate_path, tmp_path):

    dst_vrt = str(tmp_path / "out.vrt")

    (ret, err) = gdaltest.runexternal_out_and_err(
        f"{gdalmdimtranslate_path} data/mdim.vrt {dst_vrt}"
    )
    assert err is None or err == "", "got error/warning"
    assert os.path.exists(dst_vrt)


###############################################################################
# Test -if option


def test_gdalmdimtranslate_if(gdalmdimtranslate_path, tmp_path):

    dst_vrt = str(tmp_path / "out.vrt")

    (ret, err) = gdaltest.runexternal_out_and_err(
        f"{gdalmdimtranslate_path} -if VRT data/mdim.vrt {dst_vrt}"
    )
    assert err is None or err == "", "got error/warning"
    assert os.path.exists(dst_vrt)


def test_gdalmdimtranslate_if_error(gdalmdimtranslate_path, tmp_path):

    dst_vrt = str(tmp_path / "out.vrt")

    (ret, err) = gdaltest.runexternal_out_and_err(
        f"{gdalmdimtranslate_path} -if i_do_not_exist data/mdim.vrt {dst_vrt}"
    )
    assert "i_do_not_exist is not a recognized driver" in err
    assert not os.path.exists(dst_vrt)
