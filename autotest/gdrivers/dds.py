#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DDS driver
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2019 Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("DDS")

test_list = [
    ("DXT1", [11376, 57826, 34652, 32919]),
    ("DXT3", [12272, 59240, 34811, 7774]),
    ("DXT5", [12272, 59240, 34811, 10402]),
    ("ETC1", [9560, 57939, 30566]),
]


@pytest.mark.parametrize(
    "compression,checksums", test_list, ids=[row[0] for row in test_list]
)
def test_dds(compression, checksums):
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    ds = gdal.GetDriverByName("DDS").CreateCopy(
        "/vsimem/out.dds", src_ds, options=["FORMAT=" + compression]
    )
    assert ds
    assert ds.RasterCount == len(checksums)
    assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == compression
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert [
        ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)
    ] == checksums


def test_dds_no_compression():
    ref_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    ds = gdal.Open("data/dds/stefan_full_rgba_no_compression.dds")
    assert ds
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    ref_checksum = [ref_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == ref_checksum
