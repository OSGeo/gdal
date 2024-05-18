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

import array
import os

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


def test_heif_subdatasets():

    ds = gdal.Open("data/heif/subdatasets.heic")
    assert ds
    assert len(ds.GetSubDatasets()) == 2
    subds1_name = ds.GetSubDatasets()[0][0]
    subds2_name = ds.GetSubDatasets()[1][0]

    ds = gdal.Open(subds1_name)
    assert ds
    assert ds.RasterXSize == 64

    ds = gdal.Open(subds2_name)
    assert ds
    assert ds.RasterXSize == 162

    with pytest.raises(Exception):
        gdal.Open("HEIF:0:data/heif/subdatasets.heic")
    with pytest.raises(Exception):
        gdal.Open("HEIF:3:data/heif/subdatasets.heic")
    with pytest.raises(Exception):
        gdal.Open("HEIF:1:non_existing.heic")
    with pytest.raises(Exception):
        gdal.Open("HEIF:")
    with pytest.raises(Exception):
        gdal.Open("HEIF:1")
    with pytest.raises(Exception):
        gdal.Open("HEIF:1:")


def make_data():
    ds = gdal.GetDriverByName("MEM").Create("", 300, 200, 3, gdal.GDT_Byte)

    ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_RedBand)
    ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_GreenBand)
    ds.GetRasterBand(3).SetRasterColorInterpretation(gdal.GCI_BlueBand)

    red_green_blue = (
        ([0xFF] * 100 + [0x00] * 200)
        + ([0x00] * 100 + [0xFF] * 100 + [0x00] * 100)
        + ([0x00] * 200 + [0xFF] * 100)
    )
    rgb_bytes = array.array("B", red_green_blue).tobytes()
    for line in range(100):
        ds.WriteRaster(
            0, line, 300, 1, rgb_bytes, buf_type=gdal.GDT_Byte, band_list=[1, 2, 3]
        )
    black_white = ([0xFF] * 150 + [0x00] * 150) * 3
    black_white_bytes = array.array("B", black_white).tobytes()
    for line in range(100):
        ds.WriteRaster(
            0,
            100 + line,
            300,
            1,
            black_white_bytes,
            buf_type=gdal.GDT_Byte,
            band_list=[1, 2, 3],
        )

    assert ds.FlushCache() == gdal.CE_None
    return ds


def make_data_with_alpha():
    ds = gdal.GetDriverByName("MEM").Create("", 300, 200, 4, gdal.GDT_Byte)

    ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_RedBand)
    ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_GreenBand)
    ds.GetRasterBand(3).SetRasterColorInterpretation(gdal.GCI_BlueBand)
    ds.GetRasterBand(4).SetRasterColorInterpretation(gdal.GCI_AlphaBand)

    red_green_blue_alpha = (
        ([0xFF] * 100 + [0x00] * 200)
        + ([0x00] * 100 + [0xFF] * 100 + [0x00] * 100)
        + ([0x00] * 200 + [0xFF] * 100)
        + ([0x7F] * 150 + [0xFF] * 150)
    )
    rgba_bytes = array.array("B", red_green_blue_alpha).tobytes()
    for line in range(100):
        ds.WriteRaster(
            0, line, 300, 1, rgba_bytes, buf_type=gdal.GDT_Byte, band_list=[1, 2, 3, 4]
        )
    black_white = ([0xFF] * 150 + [0x00] * 150) * 4
    black_white_bytes = array.array("B", black_white).tobytes()
    for line in range(100):
        ds.WriteRaster(
            0,
            100 + line,
            300,
            1,
            black_white_bytes,
            buf_type=gdal.GDT_Byte,
            band_list=[1, 2, 3, 4],
        )

    assert ds.FlushCache() == gdal.CE_None
    return ds


heif_codecs = ["HEIF", "JPEG", "JPEG2000"]


@pytest.mark.parametrize("codec", heif_codecs)
def test_heif_create_copy(codec):
    tempfile = "tmp/test_heif_create_copy_" + codec + ".hif"
    try:
        os.remove(tempfile)
    except OSError:
        pass

    input_ds = make_data()

    drv = gdal.GetDriverByName("HEIF")
    result_ds = drv.CreateCopy(tempfile, input_ds, options=["CODEC=" + codec])

    result_ds = None

    result_ds = gdal.Open(tempfile)

    assert result_ds


@pytest.mark.parametrize("codec", heif_codecs)
def test_heif_create_copy_with_alpha(codec):
    tempfile = "tmp/test_heif_create_copy_" + codec + "_alpha.hif"
    try:
        os.remove(tempfile)
    except OSError:
        pass

    input_ds = make_data_with_alpha()

    drv = gdal.GetDriverByName("HEIF")
    result_ds = drv.CreateCopy(tempfile, input_ds, options=["CODEC=" + codec])

    result_ds = None

    result_ds = gdal.Open(tempfile)

    assert result_ds


def test_heif_create_copy_defaults():
    tempfile = "tmp/test_heif_create_copy.hif"
    try:
        os.remove(tempfile)
    except OSError:
        pass

    input_ds = make_data()

    drv = gdal.GetDriverByName("HEIF")

    result_ds = drv.CreateCopy(tempfile, input_ds, options=[])

    result_ds = None

    result_ds = gdal.Open(tempfile)

    assert result_ds
