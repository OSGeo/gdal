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


def _has_geoheif_support():
    drv = gdal.GetDriverByName("HEIF")
    return drv and drv.GetMetadataItem("SUPPORTS_GEOHEIF", "HEIF")


if __name__ == "__main__":

    os.environ["GDAL_SKIP"] = "AVIF"
    from osgeo import gdal

    gdal.UseExceptions()
    ds = gdal.Open("data/avif/byte.avif")
    assert ds.GetRasterBand(1).Checksum() == 4672

    if _has_geoheif_support():
        ds = gdal.Open("data/heif/geo_small.avif")
        assert ds
        assert ds.RasterCount == 3
        assert ds.RasterXSize == 128
        assert ds.RasterYSize == 76
        assert ds.GetGeoTransform() is not None
        assert ds.GetGeoTransform() == pytest.approx(
            [691000.0, 0.1, 0.0, 6090000.0, 0.0, -0.1]
        )
        assert ds.GetSpatialRef() is not None
        assert ds.GetSpatialRef().GetAuthorityName(None) == "EPSG"
        assert ds.GetSpatialRef().GetAuthorityCode(None) == "28355"
        assert ds.GetGCPCount() == 1
        gcp = ds.GetGCPs()[0]
        assert (
            gcp.GCPPixel == pytest.approx(0, abs=1e-5)
            and gcp.GCPLine == pytest.approx(0, abs=1e-5)
            and gcp.GCPX == pytest.approx(691000.0, abs=1e-5)
            and gcp.GCPY == pytest.approx(6090000.0, abs=1e-5)
            and gcp.GCPZ == pytest.approx(0, abs=1e-5)
        )
