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
# SPDX-License-Identifier: MIT
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
