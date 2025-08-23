#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for JPEGXL driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import base64
import struct

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal

pytestmark = pytest.mark.require_driver("JPEGXL")

###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


def test_jpegxl_read():
    tst = gdaltest.GDALTest("JPEGXL", "jpegxl/byte.jxl", 1, 4672)
    tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


def test_jpegxl_byte():
    tst = gdaltest.GDALTest(
        "JPEGXL", "byte.tif", 1, 4672, options=["LOSSLESS_COPY=YES"]
    )
    tst.testCreateCopy(vsimem=1)


def test_jpegxl_uint16():
    tst = gdaltest.GDALTest("JPEGXL", "../../gcore/data/uint16.tif", 1, 4672)
    tst.testCreateCopy(vsimem=1)


def test_jpegxl_float32():
    tst = gdaltest.GDALTest("JPEGXL", "float32.tif", 1, 4672)
    tst.testCreateCopy(vsimem=1)


def test_jpegxl_grey_alpha():
    tst = gdaltest.GDALTest(
        "JPEGXL", "../../gcore/data/stefan_full_greyalpha.tif", 1, 1970
    )
    tst.testCreateCopy(vsimem=1)


def test_jpegxl_rgb():
    tst = gdaltest.GDALTest("JPEGXL", "rgbsmall.tif", 1, 21212)
    tst.testCreateCopy(vsimem=1)


def test_jpegxl_rgba():
    tst = gdaltest.GDALTest("JPEGXL", "../../gcore/data/stefan_full_rgba.tif", 1, 12603)
    tst.testCreateCopy(vsimem=1)


@pytest.mark.parametrize("lossless", ["YES", "NO", None])
def test_jpegxl_rgba_lossless_param(tmp_vsimem, lossless):
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    filename = tmp_vsimem / "jpegxl_rgba_lossless_param.jxl"
    options = []
    if lossless:
        options += ["LOSSLESS=" + lossless]
    gdal.GetDriverByName("JPEGXL").CreateCopy(filename, src_ds, options=options)
    ds = gdal.Open(filename)
    assert (
        ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE") == "LOSSY"
        if lossless == "NO"
        else "LOSSLESS (possibly)"
    )
    cs = ds.GetRasterBand(1).Checksum()
    assert cs != 0
    if lossless == "NO":
        assert cs != src_ds.GetRasterBand(1).Checksum()
    else:
        assert cs == src_ds.GetRasterBand(1).Checksum()


def test_jpegxl_rgba_lossless_no_but_lossless_copy_yes():
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    filename = "/vsimem/jpegxl_rgba_lossless_no_but_lossless_copy_yes.jxl"
    with gdal.quiet_errors():
        assert (
            gdal.GetDriverByName("JPEGXL").CreateCopy(
                filename, src_ds, options=["LOSSLESS=NO", "LOSSLESS_COPY=YES"]
            )
            is None
        )
    assert gdal.VSIStatL(filename) is None


def test_jpegxl_rgba_distance(tmp_vsimem):
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    filename = tmp_vsimem / "jpegxl_rgba_distance.jxl"
    gdal.GetDriverByName("JPEGXL").CreateCopy(filename, src_ds, options=["DISTANCE=2"])
    ds = gdal.Open(filename)
    assert ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE") == "LOSSY"
    cs = ds.GetRasterBand(1).Checksum()
    assert cs != 0 and cs != src_ds.GetRasterBand(1).Checksum()


@pytest.mark.parametrize(
    "quality,equivalent_distance", [(100, 0), (10, 15.266666666666667)]
)
def test_jpegxl_rgba_quality(tmp_vsimem, quality, equivalent_distance):
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    filename = tmp_vsimem / "jpegxl_rgba_quality.jxl"

    gdal.GetDriverByName("JPEGXL").CreateCopy(
        filename, src_ds, options=["QUALITY=" + str(quality)]
    )
    ds = gdal.Open(filename)
    assert ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE") == "LOSSY"
    cs = ds.GetRasterBand(1).Checksum()
    assert cs != 0 and cs != src_ds.GetRasterBand(1).Checksum()

    with gdal.quiet_errors():
        gdal.GetDriverByName("JPEGXL").CreateCopy(
            filename, src_ds, options=["DISTANCE=" + str(equivalent_distance)]
        )
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == cs


@pytest.mark.require_creation_option("JPEGXL", "COMPRESS_BOX")
def test_jpegxl_xmp(tmp_vsimem):
    src_ds = gdal.Open("data/gtiff/byte_with_xmp.tif")
    filename = tmp_vsimem / "jpegxl_xmp.jxl"
    gdal.GetDriverByName("JPEGXL").CreateCopy(filename, src_ds)
    assert gdal.VSIStatL(str(filename) + ".aux.xml") is None
    ds = gdal.Open(filename)
    assert set(ds.GetMetadataDomainList()) == set(
        ["DERIVED_SUBDATASETS", "xml:XMP", "IMAGE_STRUCTURE"]
    )
    assert ds.GetMetadata("xml:XMP")[0].startswith("<?xpacket")


@pytest.mark.require_creation_option("JPEGXL", "COMPRESS_BOX")
def test_jpegxl_exif(tmp_vsimem):
    src_ds = gdal.Open("../gcore/data/exif_and_gps.tif")
    filename = tmp_vsimem / "jpegxl_exif.jxl"
    gdal.GetDriverByName("JPEGXL").CreateCopy(filename, src_ds)
    gdal.Unlink(str(filename) + ".aux.xml")
    ds = gdal.Open(filename)
    assert set(ds.GetMetadataDomainList()) == set(
        ["DERIVED_SUBDATASETS", "IMAGE_STRUCTURE", "EXIF"]
    )
    assert src_ds.GetMetadata("EXIF") == ds.GetMetadata("EXIF")


@pytest.mark.require_creation_option("JPEGXL", "COMPRESS_BOX")
def test_jpegxl_read_huge_xmp_compressed_box():
    with gdal.quiet_errors():
        gdal.ErrorReset()
        ds = gdal.Open("data/jpegxl/huge_xmp_compressed_box.jxl")
        assert ds is not None
        assert gdal.GetLastErrorMsg() != ""


def test_jpegxl_uint8_7_bits(tmp_vsimem):
    src_ds = gdal.Open("data/byte.tif")
    rescaled_ds = gdal.Translate("", src_ds, options="-of MEM -scale 0 255 0 127")
    filename = tmp_vsimem / "jpegxl_uint8_7_bits.jxl"
    gdal.GetDriverByName("JPEGXL").CreateCopy(
        filename, rescaled_ds, options=["NBITS=7"]
    )
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == rescaled_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "7"


def test_jpegxl_uint16_12_bits(tmp_vsimem):
    src_ds = gdal.Open("../gcore/data/uint16.tif")
    filename = tmp_vsimem / "jpegxl_uint16_12_bits.jxl"
    gdal.GetDriverByName("JPEGXL").CreateCopy(filename, src_ds, options=["NBITS=12"])
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "12"


def test_jpegxl_rasterio(tmp_vsimem):
    src_ds = gdal.Open("data/rgbsmall.tif")
    filename = tmp_vsimem / "jpegxl_rasterio.jxl"
    gdal.GetDriverByName("JPEGXL").CreateCopy(filename, src_ds)
    ds = gdal.Open(filename)

    # Optimized code path: read directly in target buffer.
    # Run twice to check that internal deferred decoding works properly.
    for i in range(2):
        got_data = ds.ReadRaster(
            buf_pixel_space=3, buf_line_space=3 * src_ds.RasterXSize, buf_band_space=1
        )
        expected_data = src_ds.ReadRaster(
            buf_pixel_space=3, buf_line_space=3 * src_ds.RasterXSize, buf_band_space=1
        )
        assert got_data == expected_data

    # Optimized code path: do not use block cache
    got_data = ds.ReadRaster(
        buf_type=gdal.GDT_UInt16,
        buf_pixel_space=2 * 3,
        buf_line_space=2 * 3 * src_ds.RasterXSize,
        buf_band_space=2,
    )
    expected_data = src_ds.ReadRaster(
        buf_type=gdal.GDT_UInt16,
        buf_pixel_space=2 * 3,
        buf_line_space=2 * 3 * src_ds.RasterXSize,
        buf_band_space=2,
    )
    assert got_data == expected_data

    got_data = ds.ReadRaster(
        band_list=[1, 2],
        buf_type=gdal.GDT_UInt16,
        buf_pixel_space=2 * 2,
        buf_line_space=2 * 2 * src_ds.RasterXSize,
        buf_band_space=2,
    )
    expected_data = src_ds.ReadRaster(
        band_list=[1, 2],
        buf_type=gdal.GDT_UInt16,
        buf_pixel_space=2 * 2,
        buf_line_space=2 * 2 * src_ds.RasterXSize,
        buf_band_space=2,
    )
    assert got_data == expected_data

    # Optimized code path: band interleaved spacing
    assert ds.ReadRaster(buf_type=gdal.GDT_UInt16) == src_ds.ReadRaster(
        buf_type=gdal.GDT_UInt16
    )
    assert ds.ReadRaster(band_list=[2, 1]) == src_ds.ReadRaster(band_list=[2, 1])

    # Regular code path
    assert ds.ReadRaster(0, 0, 10, 10) == src_ds.ReadRaster(0, 0, 10, 10)


def test_jpegxl_icc_profile(tmp_vsimem):
    with open("data/sRGB.icc", "rb") as f:
        data = f.read()
        icc = base64.b64encode(data).decode("ascii")

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    src_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_BlueBand)
    src_ds.SetMetadataItem("SOURCE_ICC_PROFILE", icc, "COLOR_PROFILE")
    filename = tmp_vsimem / "jpegxl_icc_profile.jxl"
    gdal.GetDriverByName("JPEGXL").CreateCopy(filename, src_ds)
    ds = gdal.Open(filename)
    assert ds.GetMetadataItem("SOURCE_ICC_PROFILE", "COLOR_PROFILE") == icc


def test_jpegxl_lossless_copy_of_jpeg(tmp_vsimem):
    jpeg_drv = gdal.GetDriverByName("JPEG")
    if jpeg_drv is None:
        pytest.skip("JPEG driver missing")

    has_box_api = "COMPRESS_BOX" in gdal.GetDriverByName("JPEGXL").GetMetadataItem(
        "DMD_CREATIONOPTIONLIST"
    )

    src_ds = gdal.Open("data/jpeg/albania.jpg")
    filename = tmp_vsimem / "jpegxl_lossless_copy_of_jpeg.jxl"
    gdal.GetDriverByName("JPEGXL").CreateCopy(filename, src_ds)
    if has_box_api:
        assert gdal.VSIStatL(str(filename) + ".aux.xml") is None
    ds = gdal.Open(filename)
    assert ds is not None
    if has_box_api:
        assert set(ds.GetMetadataDomainList()) == set(
            ["DERIVED_SUBDATASETS", "EXIF", "IMAGE_STRUCTURE"]
        )
        assert (
            ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE")
            == "LOSSY"
        )
        assert ds.GetMetadataItem("ORIGINAL_COMPRESSION", "IMAGE_STRUCTURE") == "JPEG"

    ds = None
    gdal.GetDriverByName("JPEGXL").Delete(filename)

    # Test failure in JxlEncoderAddJPEGFrame() by adding a truncated JPEG file
    data = open("data/jpeg/albania.jpg", "rb").read()
    data = data[0 : len(data) // 2]
    with gdaltest.tempfile("/vsimem/truncated.jpg", data):
        src_ds = gdal.Open("/vsimem/truncated.jpg")
        with gdal.quiet_errors():
            assert gdal.GetDriverByName("JPEGXL").CreateCopy(filename, src_ds) is None


@pytest.mark.require_creation_option("JPEGXL", "COMPRESS_BOX")
def test_jpegxl_lossless_copy_of_jpeg_disabled(tmp_vsimem):
    jpeg_drv = gdal.GetDriverByName("JPEG")
    if jpeg_drv is None:
        pytest.skip("JPEG driver missing")

    src_ds = gdal.Open("data/jpeg/albania.jpg")
    filename = tmp_vsimem / "jpegxl_lossless_copy_of_jpeg_disabled.jxl"
    gdal.GetDriverByName("JPEGXL").CreateCopy(
        filename, src_ds, options=["LOSSLESS_COPY=NO"]
    )
    ds = gdal.Open(filename)
    assert ds is not None

    assert ds.GetMetadataItem("ORIGINAL_COMPRESSION", "IMAGE_STRUCTURE") != "JPEG"


def test_jpegxl_lossless_copy_of_jpeg_with_mask_band(tmp_vsimem):
    jpeg_drv = gdal.GetDriverByName("JPEG")
    if jpeg_drv is None:
        pytest.skip("JPEG driver missing")

    drv = gdal.GetDriverByName("JPEGXL")
    if drv.GetMetadataItem("JXL_ENCODER_SUPPORT_EXTRA_CHANNELS") is None:
        pytest.skip()

    has_box_api = "COMPRESS_BOX" in drv.GetMetadataItem("DMD_CREATIONOPTIONLIST")
    src_ds = gdal.Open("data/jpeg/masked.jpg")
    filename = tmp_vsimem / "jpegxl_lossless_copy_of_jpeg_with_mask_band.jxl"
    drv.CreateCopy(filename, src_ds, options=["LOSSLESS_COPY=YES"])
    if has_box_api:
        assert gdal.VSIStatL(str(filename) + ".aux.xml") is None

    ds = gdal.Open(filename)
    assert ds is not None
    assert ds.RasterCount == 4
    assert (
        ds.GetRasterBand(4).Checksum()
        == src_ds.GetRasterBand(1).GetMaskBand().Checksum()
    )

    if has_box_api:
        assert (
            ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE")
            == "LOSSY"
        )
        assert ds.GetMetadataItem("ORIGINAL_COMPRESSION", "IMAGE_STRUCTURE") == "JPEG"

    filename_jpg = tmp_vsimem / "out.jpg"

    jpeg_drv.CreateCopy(filename_jpg, ds)
    ds = None
    ds = gdal.Open(filename_jpg)
    assert ds is not None
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(2).Checksum() == src_ds.GetRasterBand(2).Checksum()
    assert ds.GetRasterBand(3).Checksum() == src_ds.GetRasterBand(3).Checksum()
    assert (
        ds.GetRasterBand(1).GetMaskBand().Checksum()
        == src_ds.GetRasterBand(1).GetMaskBand().Checksum()
    )


@pytest.mark.require_creation_option("JPEGXL", "COMPRESS_BOX")
def test_jpegxl_lossless_copy_of_jpeg_xmp(tmp_vsimem):
    jpeg_drv = gdal.GetDriverByName("JPEG")
    if jpeg_drv is None:
        pytest.skip("JPEG driver missing")
    drv = gdal.GetDriverByName("JPEGXL")

    src_ds = gdal.Open("data/jpeg/byte_with_xmp.jpg")
    filename = tmp_vsimem / "jpegxl_lossless_copy_of_jpeg_xmp.jxl"
    drv.CreateCopy(filename, src_ds)
    assert gdal.VSIStatL(str(filename) + ".aux.xml") is None

    ds = gdal.Open(filename)
    assert ds is not None

    filename_jpg = tmp_vsimem / "jpegxl_lossless_copy_of_jpeg_xmp.jpg"
    jpeg_drv.CreateCopy(filename_jpg, ds)
    assert gdal.VSIStatL(str(filename_jpg) + ".aux.xml") is None
    ds = None
    ds = gdal.Open(filename_jpg)
    assert ds is not None
    assert ds.GetMetadata("xml:XMP") == src_ds.GetMetadata("xml:XMP")
    ds = None


def test_jpegxl_read_extra_channels():
    src_ds = gdal.Open("data/rgbsmall.tif")
    ds = gdal.Open("data/jpegxl/threeband_non_rgb.jxl")

    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(src_ds.RasterCount)] == [
        src_ds.GetRasterBand(i + 1).Checksum() for i in range(src_ds.RasterCount)
    ]
    assert ds.ReadRaster() == src_ds.ReadRaster()


def test_jpegxl_write_extra_channels(tmp_vsimem):
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    mem_ds = gdal.GetDriverByName("MEM").Create(
        "", src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount
    )
    mem_ds.WriteRaster(
        0, 0, src_ds.RasterXSize, src_ds.RasterYSize, src_ds.ReadRaster()
    )
    mem_ds.GetRasterBand(3).SetDescription("third channel")
    filename = tmp_vsimem / "jpegxl_write_extra_channels.jxl"

    drv = gdal.GetDriverByName("JPEGXL")
    if drv.GetMetadataItem("JXL_ENCODER_SUPPORT_EXTRA_CHANNELS") is not None:
        assert drv.CreateCopy(filename, mem_ds) is not None
        assert gdal.VSIStatL(str(filename) + ".aux.xml") is None
        ds = gdal.Open(filename)
        assert [
            ds.GetRasterBand(i + 1).Checksum() for i in range(src_ds.RasterCount)
        ] == [mem_ds.GetRasterBand(i + 1).Checksum() for i in range(src_ds.RasterCount)]
        assert ds.ReadRaster() == mem_ds.ReadRaster()
        assert ds.GetRasterBand(1).GetDescription() == ""
        assert (
            ds.GetRasterBand(2).GetDescription() == ""
        )  # 'Band 2' encoded in .jxl file, but hidden when reading back
        assert ds.GetRasterBand(3).GetDescription() == "third channel"
    else:
        with gdal.quiet_errors():
            assert drv.CreateCopy(filename, mem_ds) is None
            assert (
                gdal.GetLastErrorMsg()
                == "This version of libjxl does not support creating non-alpha extra channels."
            )


def test_jpegxl_read_five_bands():
    ds = gdal.Open("data/jpegxl/five_bands.jxl")
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(5)] == [
        3741,
        5281,
        6003,
        5095,
        4318,
    ]
    mem_ds = gdal.GetDriverByName("MEM").CreateCopy("", ds)
    assert [mem_ds.GetRasterBand(i + 1).Checksum() for i in range(5)] == [
        3741,
        5281,
        6003,
        5095,
        4318,
    ]
    assert ds.ReadRaster() == mem_ds.ReadRaster()
    assert ds.ReadRaster(band_list=[1]) == mem_ds.ReadRaster(band_list=[1])
    assert ds.ReadRaster(
        buf_pixel_space=ds.RasterCount,
        buf_line_space=ds.RasterCount * ds.RasterXSize,
        buf_band_space=1,
    ) == mem_ds.ReadRaster(
        buf_pixel_space=ds.RasterCount,
        buf_line_space=ds.RasterCount * ds.RasterXSize,
        buf_band_space=1,
    )


def test_jpegxl_write_five_bands(tmp_vsimem):
    drv = gdal.GetDriverByName("JPEGXL")
    if drv.GetMetadataItem("JXL_ENCODER_SUPPORT_EXTRA_CHANNELS") is None:
        pytest.skip()

    src_ds = gdal.Open("data/jpegxl/five_bands.jxl")
    filename = tmp_vsimem / "jpegxl_write_five_bands.jxl"
    assert drv.CreateCopy(filename, src_ds) is not None
    ds = gdal.Open(filename)
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(5)] == [
        3741,
        5281,
        6003,
        5095,
        4318,
    ]


def test_jpegxl_write_jxl_to_jxl_data_type_change(tmp_vsimem):

    out_filename = tmp_vsimem / "out.jxl"
    gdal.Translate(out_filename, "data/jpegxl/float16.jxl", options="-ot Byte")
    with gdal.Open(out_filename) as ds:
        assert ds.GetRasterBand(1).DataType == gdal.GDT_Byte


def test_jpegxl_write_five_bands_lossy(tmp_vsimem):
    drv = gdal.GetDriverByName("JPEGXL")
    if drv.GetMetadataItem("JXL_ENCODER_SUPPORT_EXTRA_CHANNELS") is None:
        pytest.skip()

    src_ds = gdal.Open("data/jpegxl/five_bands.jxl")
    filename = tmp_vsimem / "jpegxl_write_five_bands_lossy.jxl"
    gdal.Translate(filename, src_ds, options="-of JPEGXL -co DISTANCE=3 -ot Byte")
    ds = gdal.Open(filename)
    for i in range(5):
        assert ds.GetRasterBand(i + 1).ComputeRasterMinMax() == pytest.approx(
            (10.0 * (i + 1), 10.0 * (i + 1)), abs=1
        )


def test_jpegxl_createcopy_errors(tmp_vsimem):
    filename = tmp_vsimem / "jpegxl_createcopy_errors.jxl"

    # band count = 0
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 0)
    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert gdal.GetDriverByName("JPEGXL").CreateCopy(filename, src_ds) is None
        assert gdal.GetLastErrorMsg() != ""

    # unsupported data type
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Int16)
    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert gdal.GetDriverByName("JPEGXL").CreateCopy(filename, src_ds) is None
        assert gdal.GetLastErrorMsg() != ""

    # wrong out file name
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert (
            gdal.GetDriverByName("JPEGXL").CreateCopy("/i_do/not/exist.jxl", src_ds)
            is None
        )
        assert gdal.GetLastErrorMsg() != ""

    # mutually exclusive options
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert (
            gdal.GetDriverByName("JPEGXL").CreateCopy(
                filename, src_ds, options=["LOSSLESS=YES", "DISTANCE=1"]
            )
            is None
        )
        assert gdal.GetLastErrorMsg() != ""

    # mutually exclusive options
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert (
            gdal.GetDriverByName("JPEGXL").CreateCopy(
                filename, src_ds, options=["LOSSLESS=YES", "ALPHA_DISTANCE=1"]
            )
            is None
        )
        assert gdal.GetLastErrorMsg() != ""

    # mutually exclusive options
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert (
            gdal.GetDriverByName("JPEGXL").CreateCopy(
                filename, src_ds, options=["LOSSLESS=YES", "QUALITY=90"]
            )
            is None
        )
        assert gdal.GetLastErrorMsg() != ""

    # mutually exclusive options
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert (
            gdal.GetDriverByName("JPEGXL").CreateCopy(
                filename, src_ds, options=["DISTANCE=1", "QUALITY=90"]
            )
            is None
        )
        assert gdal.GetLastErrorMsg() != ""

    # wrong value for DISTANCE
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert (
            gdal.GetDriverByName("JPEGXL").CreateCopy(
                filename, src_ds, options=["DISTANCE=-1"]
            )
            is None
        )
        assert gdal.GetLastErrorMsg() != ""

    # wrong value for EFFORT
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert (
            gdal.GetDriverByName("JPEGXL").CreateCopy(
                filename, src_ds, options=["EFFORT=-1"]
            )
            is None
        )
        assert gdal.GetLastErrorMsg() != ""


###############################################################################
def test_jpegxl_band_combinations(tmp_vsimem):
    drv = gdal.GetDriverByName("JPEGXL")
    if drv.GetMetadataItem("JXL_ENCODER_SUPPORT_EXTRA_CHANNELS") is None:
        pytest.skip()

    filename = tmp_vsimem / "test_jpegxl_band_combinations.jxl"
    src_ds = gdal.GetDriverByName("MEM").Create("", 64, 64, 6)
    for b in range(6):
        bnd = src_ds.GetRasterBand(b + 1)
        bnd.Fill(b + 1)
        bnd.FlushCache()
        assert bnd.Checksum() != 0, "bnd.Fill failed"

    cilists = [
        [gdal.GCI_RedBand],
        [gdal.GCI_RedBand, gdal.GCI_Undefined],
        [gdal.GCI_RedBand, gdal.GCI_AlphaBand],
        [gdal.GCI_Undefined, gdal.GCI_AlphaBand],
        [gdal.GCI_RedBand, gdal.GCI_GreenBand, gdal.GCI_BlueBand],
        [gdal.GCI_RedBand, gdal.GCI_GreenBand, gdal.GCI_BlueBand, gdal.GCI_AlphaBand],
        [
            gdal.GCI_RedBand,
            gdal.GCI_GreenBand,
            gdal.GCI_BlueBand,
            gdal.GCI_AlphaBand,
            gdal.GCI_Undefined,
        ],
        [
            gdal.GCI_RedBand,
            gdal.GCI_GreenBand,
            gdal.GCI_BlueBand,
            gdal.GCI_Undefined,
            gdal.GCI_Undefined,
        ],
        [
            gdal.GCI_RedBand,
            gdal.GCI_GreenBand,
            gdal.GCI_BlueBand,
            gdal.GCI_Undefined,
            gdal.GCI_AlphaBand,
        ],
        [
            gdal.GCI_RedBand,
            gdal.GCI_GreenBand,
            gdal.GCI_AlphaBand,
            gdal.GCI_Undefined,
            gdal.GCI_BlueBand,
        ],
    ]

    types = [
        gdal.GDT_Byte,
        gdal.GDT_UInt16,
    ]

    for dtype in types:
        for cilist in cilists:
            bandlist = [idx + 1 for idx in range(len(cilist))]
            vrtds = gdal.Translate(
                "", src_ds, format="vrt", bandList=bandlist, outputType=dtype
            )
            for idx, ci in enumerate(cilist):
                vrtds.GetRasterBand(idx + 1).SetColorInterpretation(ci)

            ds = gdal.Translate(filename, vrtds)
            ds = None
            gdal.Unlink(str(filename) + ".aux.xml")
            # print(dtype, cilist)
            ds = gdal.Open(filename)
            for idx in range(len(cilist)):
                assert (
                    ds.GetRasterBand(idx + 1).Checksum()
                    == src_ds.GetRasterBand(idx + 1).Checksum()
                ), (dtype, cilist, idx)
            if (
                vrtds.RasterCount >= 3
                and vrtds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
                and vrtds.GetRasterBand(2).GetColorInterpretation()
                == gdal.GCI_GreenBand
                and vrtds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
            ):
                assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
                assert (
                    ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
                )
                assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
            # Check that alpha band is preserved
            for idx in range(len(cilist)):
                if (
                    vrtds.GetRasterBand(idx + 1).GetColorInterpretation()
                    == gdal.GCI_AlphaBand
                ):
                    assert (
                        ds.GetRasterBand(idx + 1).GetColorInterpretation()
                        == gdal.GCI_AlphaBand
                    )
            vrtds = None
            ds = None
            gdal.Unlink(filename)


###############################################################################
# Test APPLY_ORIENTATION=YES open option
@pytest.mark.parametrize("orientation", [i + 1 for i in range(8)])
def test_jpegxl_apply_orientation(orientation):
    open_options = gdal.GetDriverByName("JPEGXL").GetMetadataItem("DMD_OPENOPTIONLIST")
    if open_options is None or "APPLY_ORIENTATION" not in open_options:
        pytest.skip()

    ds = gdal.OpenEx(
        "data/jpegxl/exif_orientation/F%d.jxl" % orientation,
        open_options=["APPLY_ORIENTATION=YES"],
    )
    assert ds.RasterXSize == 3
    assert ds.RasterYSize == 5
    vals = struct.unpack("B" * 3 * 5, ds.ReadRaster())
    vals = [1 if v >= 128 else 0 for v in vals]
    assert vals == [1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0]
    if orientation != 1:
        assert ds.GetMetadataItem("EXIF_Orientation", "EXIF") is None
        assert ds.GetMetadataItem("original_EXIF_Orientation", "EXIF") == str(
            orientation
        )


###############################################################################
# Test ALPHA_DISTANCE option0
@pytest.mark.require_creation_option(
    "JPEGXL", "ALPHA_DISTANCE"
)  # "libjxl > 0.8.1 required"
def test_jpegxl_alpha_distance_zero(tmp_vsimem):
    drv = gdal.GetDriverByName("JPEGXL")

    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    filename = tmp_vsimem / "test_jpegxl_alpha_distance_zero.jxl"
    drv.CreateCopy(
        filename,
        src_ds,
        options=["LOSSLESS=NO", "ALPHA_DISTANCE=0"],
    )
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() != src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(4).Checksum() == src_ds.GetRasterBand(4).Checksum()


###############################################################################
# Test identifying a JPEGXL raw codestream (not within a JPEGXL container)
# that has not a .jxl extension
# Serves as a way of checking that the simplified identification method in GDAL
# core, when the driver built as a plugin, is followed by a call to the real
# driver to further refine the identification.
pytest.mark.skipif(
    test_cli_utilities.get_cli_utility_path("gdalmanage") is None,
    reason="gdalmanage not available",
)


def test_jpegxl_identify_raw_codestream():
    gdalmanage_path = test_cli_utilities.get_cli_utility_path("gdalmanage")
    out, err = gdaltest.runexternal_out_and_err(
        f"{gdalmanage_path} identify data/jpegxl/test.jxl.bin"
    )
    assert "JPEGXL" in out


###############################################################################
def test_jpegxl_read_float16():
    # Image produced with:
    # gdal_translate autotest/gcore/data/rgbsmall.tif float.exr -co PIXEL_TYPE=FLOAT -co TILED=NO -co COMPRESS=PIZ
    # cjxl -d 0 float.exr float16.jxl
    ds = gdal.Open("data/jpegxl/float16.jxl")
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == [
        21212,
        21053,
        21349,
    ]
