#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for AVIF_HEIF driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
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
import subprocess
import sys

import pytest


def test_avif_heif():

    from osgeo import gdal

    drv = gdal.GetDriverByName("HEIF")
    if drv is None:
        pytest.skip("HEIF driver must be available")
    if drv.GetMetadataItem("SUPPORTS_AVIF", "HEIF") is None:
        pytest.skip("libheif has no AVIF support")

    subprocess.check_call(
        [
            sys.executable,
            "avif_heif.py",
            "subprocess",
        ]
    )


if __name__ == "__main__":

    os.environ["GDAL_SKIP"] = "AVIF"
    from osgeo import gdal

    gdal.UseExceptions()
    ds = gdal.Open("data/avif/byte.avif")
    assert ds.GetRasterBand(1).Checksum() == 4672
