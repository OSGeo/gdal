#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test HEIF driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
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
import shutil

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("HEIF")


def get_version():
    return [
        int(x)
        for x in gdal.GetDriverByName("HEIF")
        .GetMetadataItem("LIBHEIF_VERSION")
        .split(".")
    ]


@pytest.mark.parametrize("endianness", ["big_endian", "little_endian"])
def test_heif_exif_endian(endianness):
    filename = "data/heif/byte_exif_%s.heic" % endianness
    gdal.ErrorReset()
    ds = gdal.Open(filename)
    assert gdal.GetLastErrorMsg() == ""
    assert ds
    assert ds.RasterXSize == 64
    assert ds.RasterYSize == 64
    assert ds.RasterCount == 3
    stats = ds.GetRasterBand(1).ComputeStatistics(False)
    assert stats[0] == pytest.approx(89, abs=2)
    assert stats[1] == pytest.approx(243, abs=2)
    assert stats[2] == pytest.approx(126.7, abs=0.5)
    assert stats[3] == pytest.approx(18.8, abs=0.5)
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(1).GetOverviewCount() == 0

    if get_version() >= [1, 4, 0]:
        assert "EXIF" in ds.GetMetadataDomainList()
        assert "xml:XMP" in ds.GetMetadataDomainList()
        assert len(ds.GetMetadata("EXIF")) > 0
        assert "xpacket" in ds.GetMetadata("xml:XMP")[0]

    ds = None
    gdal.Unlink(filename + ".aux.xml")


def test_heif_thumbnail():
    ds = gdal.Open("data/heif/byte_thumbnail.heic")
    assert ds
    assert ds.RasterXSize == 128
    assert ds.RasterYSize == 128
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(-1) is None
    assert ds.GetRasterBand(1).GetOverview(1) is None
    ovrband = ds.GetRasterBand(1).GetOverview(0)
    assert ovrband is not None
    assert ovrband.XSize == 64
    assert ovrband.YSize == 64
    assert ovrband.Checksum() != 0


def test_heif_rgb_16bit():
    if get_version() < [1, 4, 0]:
        pytest.skip()

    ds = gdal.Open("data/heif/small_world_16.heic")
    assert ds
    assert ds.RasterXSize == 400
    assert ds.RasterYSize == 200
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "10"
    assert ds.GetRasterBand(1).ComputeRasterMinMax() == pytest.approx((0, 1023), abs=2)


def test_heif_rgba():

    ds = gdal.Open("data/heif/stefan_full_rgba.heic")
    assert ds
    assert ds.RasterCount == 4
    assert ds.RasterXSize == 162
    assert ds.RasterYSize == 150
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    ovrband = ds.GetRasterBand(1).GetOverview(0)
    assert ovrband is not None
    assert ovrband.XSize == 96
    assert ovrband.YSize == 88
    assert ovrband.Checksum() != 0


def test_heif_rgba_16bit():
    if get_version() < [1, 4, 0]:
        pytest.skip()

    ds = gdal.Open("data/heif/stefan_full_rgba_16.heic")
    assert ds
    assert ds.RasterCount == 4
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16


def test_heif_subdatasets(tmp_path):

    filename = str(tmp_path / "out.heic")
    shutil.copy("data/heif/subdatasets.heic", filename)

    ds = gdal.Open(filename)
    assert ds
    assert len(ds.GetSubDatasets()) == 2
    subds1_name = ds.GetSubDatasets()[0][0]
    subds2_name = ds.GetSubDatasets()[1][0]

    ds = gdal.Open(subds1_name)
    assert ds
    assert ds.RasterXSize == 64
    assert ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is None
    assert ds.GetRasterBand(1).ComputeStatistics(False)
    assert ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is not None
    ds.Close()

    ds = gdal.Open(subds1_name)
    assert ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is not None

    ds = gdal.Open(subds2_name)
    assert ds
    assert ds.RasterXSize == 162
    assert ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is None

    with pytest.raises(Exception):
        gdal.Open(f"HEIF:0:{filename}")
    with pytest.raises(Exception):
        gdal.Open(f"HEIF:3:{filename}")
    with pytest.raises(Exception):
        gdal.Open("HEIF:1:non_existing.heic")
    with pytest.raises(Exception):
        gdal.Open("HEIF:")
    with pytest.raises(Exception):
        gdal.Open("HEIF:1")
    with pytest.raises(Exception):
        gdal.Open("HEIF:1:")


def test_heif_identify_no_match():

    drv = gdal.IdentifyDriverEx("data/byte.tif", allowed_drivers=["HEIF"])
    assert drv is None


def test_heif_identify_heic():

    drv = gdal.IdentifyDriverEx("data/heif/subdatasets.heic", allowed_drivers=["HEIF"])
    assert drv.GetDescription() == "HEIF"


@pytest.mark.parametrize(
    "major_brand,compatible_brands,expect_success",
    [
        ("heic", [], True),
        ("heix", [], True),
        ("j2ki", [], True),
        ("j2ki", ["j2ki"], True),
        ("jpeg", [], True),
        ("jpg ", [], False),
        ("miaf", [], True),
        ("mif1", [], True),
        ("mif2", [], True),
        ("mif9", [], False),  # this doesn't exist
        ("fake", ["miaf"], True),
        ("j2kj", [], False),
        ("fake", [], False),
        ("fake", ["fake", "also"], False),
        ("fake", ["fake", "avif"], True),
        ("fake", ["fake", "bvif"], False),
        ("fake", ["fake", "mif2"], True),
        ("fake", ["fake", "mif9"], False),
    ],
)
def test_identify_various(major_brand, compatible_brands, expect_success):

    f = gdal.VSIFOpenL("/vsimem/heif_header.bin", "wb")
    gdal.VSIFSeekL(f, 4, os.SEEK_SET)
    gdal.VSIFWriteL("ftyp", 1, 4, f)  # box type
    gdal.VSIFWriteL(major_brand, 1, 4, f)
    gdal.VSIFWriteL(b"\x00\x00\x00\x00", 1, 4, f)  # minor_version
    for brand in compatible_brands:
        gdal.VSIFWriteL(brand, 1, 4, f)
    length = gdal.VSIFTellL(f)
    gdal.VSIFSeekL(f, 0, os.SEEK_SET)  # go back and fill in actual box size
    gdal.VSIFWriteL(length.to_bytes(4, "big"), 1, 4, f)
    gdal.VSIFCloseL(f)

    drv = gdal.IdentifyDriverEx("/vsimem/heif_header.bin", allowed_drivers=["HEIF"])
    if expect_success:
        assert drv.GetDescription() == "HEIF"
    else:
        assert drv is None

    gdal.Unlink("/vsimem/heif_header.bin")
