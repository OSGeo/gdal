#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test JPEG format driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil
import struct
import sys

import gdaltest
import pytest

from osgeo import gdal, gdalconst

pytestmark = pytest.mark.require_driver("JPEG")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Perform simple read test.


def test_jpeg_1(jpeg_version):

    if jpeg_version == "9b":
        tst = gdaltest.GDALTest("JPEG", "jpeg/albania.jpg", 2, 34296)
    elif jpeg_version == "8":
        tst = gdaltest.GDALTest("JPEG", "jpeg/albania.jpg", 2, 34298)
    else:
        tst = gdaltest.GDALTest("JPEG", "jpeg/albania.jpg", 2, 17016)
    tst.testOpen()


###############################################################################
# Verify EXIF metadata, color interpretation and image_structure


def test_jpeg_2():

    ds = gdal.Open("data/jpeg/albania.jpg")

    assert ds.GetMetadataDomainList() == ["IMAGE_STRUCTURE", "", "DERIVED_SUBDATASETS"]
    md = ds.GetMetadata()

    ds.GetFileList()

    try:
        assert not (
            md["EXIF_GPSLatitudeRef"] != "N"
            or md["EXIF_GPSLatitude"] != "(41) (1) (22.91)"
            or md["EXIF_PixelXDimension"] != "361"
            or md["EXIF_GPSVersionID"] != "0x02 0x00 0x00 0x00"
            or md["EXIF_ExifVersion"] != "0210"
            or md["EXIF_XResolution"] != "(96)"
        ), "Exif metadata wrong."
    except KeyError:
        print(md)
        pytest.fail("Exit metadata apparently missing.")

    assert (
        ds.GetRasterBand(3).GetRasterColorInterpretation() == gdal.GCI_BlueBand
    ), "Did not get expected color interpretation."

    md = ds.GetMetadata("IMAGE_STRUCTURE")
    assert (
        "INTERLEAVE" in md and md["INTERLEAVE"] == "PIXEL"
    ), "missing INTERLEAVE metadata"
    assert (
        "COMPRESSION" in md and md["COMPRESSION"] == "JPEG"
    ), "missing INTERLEAVE metadata"

    md = ds.GetRasterBand(3).GetMetadata("IMAGE_STRUCTURE")
    assert (
        "COMPRESSION" in md and md["COMPRESSION"] == "JPEG"
    ), "missing INTERLEAVE metadata"


###############################################################################
# Create simple copy and check (greyscale) using progressive option.


def test_jpeg_3():

    ds = gdal.Open("data/byte.tif")

    options = ["PROGRESSIVE=YES", "QUALITY=50", "WORLDFILE=YES"]
    ds = gdal.GetDriverByName("JPEG").CreateCopy("tmp/byte.jpg", ds, options=options)

    # IJG, MozJPEG
    expected_cs = [4794, 4787]

    assert (
        ds.GetRasterBand(1).Checksum() in expected_cs
    ), "Wrong checksum on copied image."

    assert (
        ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_GrayIndex
    ), "Wrong color interpretation."

    expected_gt = [440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0]
    gt = ds.GetGeoTransform()
    for i in range(6):
        assert gt[i] == pytest.approx(
            expected_gt[i], abs=1e-6
        ), "did not get expected geotransform from PAM"

    ds = None

    ds = gdal.Open("tmp/byte.jpg")
    assert ds.GetMetadata() == {"AREA_OR_POINT": "Area"}
    ds = None

    os.unlink("tmp/byte.jpg.aux.xml")

    try:
        os.stat("tmp/byte.wld")
    except OSError:
        pytest.fail("should have .wld file at that point")

    ds = gdal.Open("tmp/byte.jpg")
    expected_gt = [440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0]
    gt = ds.GetGeoTransform()
    for i in range(6):
        assert gt[i] == pytest.approx(
            expected_gt[i], abs=1e-6
        ), "did not get expected geotransform from .wld"
    ds = None

    ds = gdal.Open("tmp/byte.jpg")
    ds.GetFileList()
    ds = None

    gdal.GetDriverByName("JPEG").Delete("tmp/byte.jpg")

    assert not os.path.exists("tmp/byte.wld")


###############################################################################
# Verify masked jpeg.


def test_jpeg_4():

    ds = gdal.Open("data/jpeg/masked.jpg")

    assert ds.GetMetadataDomainList() == ["IMAGE_STRUCTURE", "DERIVED_SUBDATASETS"]

    refband = ds.GetRasterBand(1)

    assert refband.GetMaskFlags() == gdalconst.GMF_PER_DATASET, "wrong mask flags"

    cs = refband.GetMaskBand().Checksum()
    assert cs == 770, "Wrong mask checksum"


###############################################################################
# Verify CreateCopy() of masked jpeg.


def test_jpeg_5():

    ds = gdal.Open("data/jpeg/masked.jpg")

    ds2 = gdal.GetDriverByName("JPEG").CreateCopy("tmp/masked.jpg", ds)

    refband = ds2.GetRasterBand(1)

    assert refband.GetMaskFlags() == gdalconst.GMF_PER_DATASET, "wrong mask flags"

    cs = refband.GetMaskBand().Checksum()
    assert cs == 770, "Wrong checksum on copied images mask."

    refband = None
    ds2 = None
    gdal.GetDriverByName("JPEG").Delete("tmp/masked.jpg")


###############################################################################
# Verify ability to open file with corrupt metadata (#1904).  Note the file
# data/jpeg/vophead.jpg is truncated to keep the size small, but this should
# not affect opening the file which just reads the header.


def test_jpeg_6():

    ds = gdal.Open("data/jpeg/vophead.jpg")

    # Because of the optimization in r17446, we shouldn't yet get this error.
    assert not (
        gdal.GetLastErrorType() == 2
        and gdal.GetLastErrorMsg().find("Ignoring EXIF") != -1
    ), "got error too soon."

    with gdaltest.error_handler("CPLQuietErrorHandler"):
        # Get this warning:
        #   Ignoring EXIF directory with unlikely entry count (65499).
        md = ds.GetMetadata()

    # Did we get an exif related warning?
    assert not (
        gdal.GetLastErrorType() != 2
        or gdal.GetLastErrorMsg().find("Ignoring EXIF") == -1
    ), "did not get expected error."

    assert (
        len(md) == 1 and md["EXIF_Software"] == "IrfanView"
    ), "did not get expected metadata."

    ds = None


###############################################################################
# Test creating an in memory copy.


def test_jpeg_7():

    ds = gdal.Open("data/byte.tif")

    options = ["PROGRESSIVE=YES", "QUALITY=50"]
    ds = gdal.GetDriverByName("JPEG").CreateCopy(
        "/vsimem/byte.jpg", ds, options=options
    )

    # IJG, MozJPEG
    expected_cs = [4794, 4787]

    assert (
        ds.GetRasterBand(1).Checksum() in expected_cs
    ), "Wrong checksum on copied image."

    ds = None
    gdal.GetDriverByName("JPEG").Delete("/vsimem/byte.jpg")


###############################################################################
# Read a CMYK image as a RGB image


def test_jpeg_8():

    ds = gdal.Open("data/jpeg/rgb_ntf_cmyk.jpg")

    expected_cs = 20385

    assert (
        ds.GetRasterBand(1).Checksum() == expected_cs
    ), "Wrong checksum on copied image."

    assert (
        ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_RedBand
    ), "Wrong color interpretation."

    expected_cs = 20865

    assert (
        ds.GetRasterBand(2).Checksum() == expected_cs
    ), "Wrong checksum on copied image."

    assert (
        ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_GreenBand
    ), "Wrong color interpretation."

    expected_cs = 19441

    assert (
        ds.GetRasterBand(3).Checksum() == expected_cs
    ), "Wrong checksum on copied image."

    assert (
        ds.GetRasterBand(3).GetRasterColorInterpretation() == gdal.GCI_BlueBand
    ), "Wrong color interpretation."

    md = ds.GetMetadata("IMAGE_STRUCTURE")

    assert (
        "SOURCE_COLOR_SPACE" in md and md["SOURCE_COLOR_SPACE"] == "CMYK"
    ), "missing SOURCE_COLOR_SPACE metadata"


###############################################################################
# Read a CMYK image as a CMYK image


def test_jpeg_9():

    with gdaltest.config_option("GDAL_JPEG_TO_RGB", "NO"):
        ds = gdal.Open("data/jpeg/rgb_ntf_cmyk.jpg")

    expected_cs = 21187

    assert (
        ds.GetRasterBand(1).Checksum() == expected_cs
    ), "Wrong checksum on copied image."

    assert (
        ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_CyanBand
    ), "Wrong color interpretation."

    expected_cs = 21054

    assert (
        ds.GetRasterBand(2).Checksum() == expected_cs
    ), "Wrong checksum on copied image."

    assert (
        ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_MagentaBand
    ), "Wrong color interpretation."

    expected_cs = 21499

    assert (
        ds.GetRasterBand(3).Checksum() == expected_cs
    ), "Wrong checksum on copied image."

    assert (
        ds.GetRasterBand(3).GetRasterColorInterpretation() == gdal.GCI_YellowBand
    ), "Wrong color interpretation."

    expected_cs = 21069

    assert (
        ds.GetRasterBand(4).Checksum() == expected_cs
    ), "Wrong checksum on copied image."

    assert (
        ds.GetRasterBand(4).GetRasterColorInterpretation() == gdal.GCI_BlackBand
    ), "Wrong color interpretation."


###############################################################################
# Check reading a 12-bit JPEG


def test_jpeg_10(jpeg_version):

    if jpeg_version == "9b":  # Fails for some reason
        pytest.skip()

    # Check if JPEG driver supports 12bit JPEG reading/writing
    drv = gdal.GetDriverByName("JPEG")
    md = drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find("UInt16") == -1:
        pytest.skip("12bit jpeg not available")

    try:
        os.remove("data/jpeg/12bit_rose_extract.jpg.aux.xml")
    except OSError:
        pass

    ds = gdal.Open("data/jpeg/12bit_rose_extract.jpg")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    assert stats[2] >= 3613 and stats[2] <= 3614
    ds = None

    try:
        os.remove("data/jpeg/12bit_rose_extract.jpg.aux.xml")
    except OSError:
        pass


###############################################################################
# Check creating a 12-bit JPEG


def test_jpeg_11(jpeg_version):

    if jpeg_version == "9b":  # Fails for some reason
        pytest.skip()

    # Check if JPEG driver supports 12bit JPEG reading/writing
    drv = gdal.GetDriverByName("JPEG")
    md = drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find("UInt16") == -1:
        pytest.skip("12bit jpeg not available")

    ds = gdal.Open("data/jpeg/12bit_rose_extract.jpg")
    out_ds = gdal.GetDriverByName("JPEG").CreateCopy("tmp/jpeg11.jpg", ds)
    del out_ds

    ds = gdal.Open("tmp/jpeg11.jpg")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    assert stats[2] >= 3613 and stats[2] <= 3614
    ds = None

    gdal.GetDriverByName("JPEG").Delete("tmp/jpeg11.jpg")


###############################################################################
# Test reading a stored JPEG in ZIP (#3908)


def test_jpeg_12():

    ds = gdal.Open("/vsizip/data/jpeg/byte_jpg.zip")
    assert ds is not None

    gdal.ErrorReset()
    ds.GetRasterBand(1).Checksum()
    assert gdal.GetLastErrorMsg() == ""

    gdal.ErrorReset()
    ds.GetRasterBand(1).GetMaskBand().Checksum()
    assert gdal.GetLastErrorMsg() == ""

    ds = None


###############################################################################
# Test writing to /vsistdout/


def test_jpeg_13():

    src_ds = gdal.Open("data/byte.tif")
    ds = gdal.GetDriverByName("JPEG").CreateCopy(
        "/vsistdout_redirect//vsimem/tmp.jpg", src_ds
    )
    assert ds.GetRasterBand(1).Checksum() == 0
    ds.ReadRaster(0, 0, 1, 1)
    src_ds = None
    ds = None

    ds = gdal.Open("/vsimem/tmp.jpg")
    assert ds is not None

    gdal.Unlink("/vsimem/tmp.jpg")


###############################################################################
# Test writing to /vsistdout/


def test_jpeg_14(jpeg_version):

    if jpeg_version == "9b":  # Fails for some reason
        pytest.skip()

    # Check if JPEG driver supports 12bit JPEG reading/writing
    drv = gdal.GetDriverByName("JPEG")
    md = drv.GetMetadata()
    if md[gdal.DMD_CREATIONDATATYPES].find("UInt16") == -1:
        pytest.skip("12bit jpeg not available")

    src_ds = gdal.Open("data/jpeg/12bit_rose_extract.jpg")
    ds = drv.CreateCopy("/vsistdout_redirect//vsimem/tmp.jpg", src_ds)
    assert ds.GetRasterBand(1).Checksum() == 0
    src_ds = None
    ds = None

    ds = gdal.Open("/vsimem/tmp.jpg")
    assert ds is not None

    gdal.Unlink("/vsimem/tmp.jpg")


###############################################################################
# Test CreateCopy() interruption


def test_jpeg_15():

    gdal.Translate("/vsimem/tmp.tif", "data/jpeg/albania.jpg")
    try:
        tst = gdaltest.GDALTest(
            "JPEG", "/vsimem/tmp.tif", 2, 17016, filename_absolute=1
        )
        tst.testCreateCopy(vsimem=1, interrupt_during_copy=True)
    finally:
        gdal.Unlink("/vsimem/tmp.tif")


###############################################################################
# Test overview support


def test_jpeg_16(jpeg_version, tmp_path):

    shutil.copy("data/jpeg/albania.jpg", tmp_path / "albania.jpg")

    ds = gdal.Open(tmp_path / "albania.jpg")
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(-1) is None
    assert ds.GetRasterBand(1).GetOverview(1) is None
    assert ds.GetRasterBand(1).GetOverview(0) is not None
    # "Internal" overview

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if jpeg_version in ("8", "9b"):
        expected_cs = 34218
    else:
        expected_cs = 31892
    assert cs == expected_cs

    # Build external overviews
    ds.BuildOverviews("NEAR", [2, 4])
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    # Check updated checksum
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if jpeg_version in ("8", "9b"):
        expected_cs = 33698
    else:
        expected_cs = 32460
    assert cs == expected_cs

    ds = None

    # Check we are using external overviews
    ds = gdal.Open(tmp_path / "albania.jpg")
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    if jpeg_version in ("8", "9b"):
        expected_cs = 33698
    else:
        expected_cs = 32460
    assert cs == expected_cs

    assert ds.Close() == gdal.CE_None
    os.unlink(tmp_path / "albania.jpg")
    os.unlink(tmp_path / "albania.jpg.ovr")


###############################################################################
# Test bogus files


def test_jpeg_17():
    gdal.ErrorReset()
    with gdaltest.error_handler("CPLQuietErrorHandler"):
        ds = gdal.Open("data/jpeg/bogus.jpg")
        assert not (
            ds is not None
            or gdal.GetLastErrorType() != gdal.CE_Failure
            or gdal.GetLastErrorMsg() == ""
        )

    gdal.ErrorReset()
    ds = gdal.Open("data/jpeg/byte_corrupted.jpg")
    with gdaltest.error_handler("CPLQuietErrorHandler"):
        # ERROR 1: libjpeg: Huffman table 0x00 was not defined
        cs = ds.GetRasterBand(1).Checksum()
    if gdal.GetLastErrorType() != gdal.CE_Failure or gdal.GetLastErrorMsg() == "":
        # libjpeg-turbo 1.4.0 doesn't emit errors...
        assert cs == 4925

    gdal.ErrorReset()
    ds = gdal.Open("data/jpeg/byte_corrupted2.jpg")
    with gdaltest.error_handler("CPLQuietErrorHandler"):
        # Get this error:
        #   libjpeg: Corrupt JPEG data: found marker 0x00 instead of RST63
        assert ds.GetRasterBand(1).Checksum() < 0

    assert not (
        gdal.GetLastErrorType() != gdal.CE_Failure or gdal.GetLastErrorMsg() == ""
    ), "Premature end of file should be a failure by default"

    gdal.ErrorReset()
    ds = gdal.Open("data/jpeg/byte_corrupted2.jpg")
    with gdaltest.error_handler("CPLQuietErrorHandler"):
        with gdaltest.config_option("GDAL_ERROR_ON_LIBJPEG_WARNING", "TRUE"):
            assert ds.GetRasterBand(1).Checksum() < 0

    assert not (
        gdal.GetLastErrorType() != gdal.CE_Failure or gdal.GetLastErrorMsg() == ""
    ), "Premature end of file should be a failure with GDAL_ERROR_ON_LIBJPEG_WARNING = TRUE"

    gdal.ErrorReset()
    ds = gdal.Open("data/jpeg/byte_corrupted2.jpg")
    with gdaltest.error_handler("CPLQuietErrorHandler"):
        with gdaltest.config_option("GDAL_ERROR_ON_LIBJPEG_WARNING", "FALSE"):
            assert ds.GetRasterBand(1).Checksum() != 0

    assert not (
        gdal.GetLastErrorType() != gdal.CE_Warning or gdal.GetLastErrorMsg() == ""
    ), "Premature end of file should be a warning with GDAL_ERROR_ON_LIBJPEG_WARNING = FALSE"

    gdal.ErrorReset()
    with gdaltest.error_handler("CPLQuietErrorHandler"):
        ds = gdal.Open("data/jpeg/byte_corrupted3.jpg")
        assert ds.GetRasterBand(1).Checksum() != 0

    assert not (
        gdal.GetLastErrorType() != gdal.CE_Warning or gdal.GetLastErrorMsg() == ""
    ), "Extraneous bytes before marker should be a warning by default"

    gdal.ErrorReset()
    with gdaltest.error_handler("CPLQuietErrorHandler"):
        with gdaltest.config_option("GDAL_ERROR_ON_LIBJPEG_WARNING", "TRUE"):
            ds = gdal.Open("data/jpeg/byte_corrupted3.jpg")

    assert not (
        gdal.GetLastErrorType() != gdal.CE_Failure or gdal.GetLastErrorMsg() == ""
    ), "Extraneous bytes before marker should be a failure with GDAL_ERROR_ON_LIBJPEG_WARNING = TRUE"

    gdal.ErrorReset()
    with gdaltest.error_handler("CPLQuietErrorHandler"):
        with gdaltest.config_option("GDAL_ERROR_ON_LIBJPEG_WARNING", "FALSE"):
            ds = gdal.Open("data/jpeg/byte_corrupted3.jpg")
            assert ds.GetRasterBand(1).Checksum() != 0

    assert not (
        gdal.GetLastErrorType() != gdal.CE_Warning or gdal.GetLastErrorMsg() == ""
    ), "Extraneous bytes before marker should be a warning with GDAL_ERROR_ON_LIBJPEG_WARNING = FALSE"


###############################################################################
# Test situation where we cause a restart and need to reset scale


def test_jpeg_18():
    height = 1024
    width = 1024
    src_ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/jpeg_18.tif", width, height, 1
    )
    for i in range(height):
        data = struct.pack("B" * 1, int(i / (height / 256)))
        src_ds.WriteRaster(0, i, width, 1, data, 1, 1)

    ds = gdal.GetDriverByName("JPEG").CreateCopy(
        "/vsimem/jpeg_18.jpg", src_ds, options=["QUALITY=99"]
    )
    src_ds = None
    gdal.Unlink("/vsimem/jpeg_18.tif")

    with gdaltest.SetCacheMax(0):

        line0 = ds.GetRasterBand(1).ReadRaster(0, 0, width, 1)
        data = struct.unpack("B" * width, line0)
        assert data[0] == pytest.approx(0, abs=10)
        line1023 = ds.GetRasterBand(1).ReadRaster(0, height - 1, width, 1)
        data = struct.unpack("B" * width, line1023)
        assert data[0] == pytest.approx(255, abs=10)
        line0_ovr1 = (
            ds.GetRasterBand(1).GetOverview(1).ReadRaster(0, 0, int(width / 4), 1)
        )
        data = struct.unpack("B" * (int(width / 4)), line0_ovr1)
        assert data[0] == pytest.approx(0, abs=10)
        line1023_bis = ds.GetRasterBand(1).ReadRaster(0, height - 1, width, 1)
        assert line1023_bis != line0 and line1023 == line1023_bis
        line0_bis = ds.GetRasterBand(1).ReadRaster(0, 0, width, 1)
        assert line0 == line0_bis
        line255_ovr1 = (
            ds.GetRasterBand(1)
            .GetOverview(1)
            .ReadRaster(0, int(height / 4) - 1, int(width / 4), 1)
        )
        data = struct.unpack("B" * int(width / 4), line255_ovr1)
        assert data[0] == pytest.approx(255, abs=10)
        line0_bis = ds.GetRasterBand(1).ReadRaster(0, 0, width, 1)
        assert line0 == line0_bis
        line0_ovr1_bis = (
            ds.GetRasterBand(1).GetOverview(1).ReadRaster(0, 0, int(width / 4), 1)
        )
        assert line0_ovr1 == line0_ovr1_bis
        line255_ovr1_bis = (
            ds.GetRasterBand(1)
            .GetOverview(1)
            .ReadRaster(0, int(height / 4) - 1, int(width / 4), 1)
        )
        assert line255_ovr1 == line255_ovr1_bis

    ds = None
    gdal.Unlink("/vsimem/jpeg_18.jpg")


###############################################################################
# Test MSB ordering of bits in mask (#5102)


def test_jpeg_19():

    for width, height, iX in [(32, 32, 12), (25, 25, 8), (24, 25, 8)]:
        src_ds = gdal.GetDriverByName("GTiff").Create(
            "/vsimem/jpeg_19.tif", width, height, 1
        )
        src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(
            0, 0, iX, height, struct.pack("B" * 1, 255), 1, 1
        )
        src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(
            iX, 0, width - iX, height, struct.pack("B" * 1, 0), 1, 1
        )
        tiff_mask_data = (
            src_ds.GetRasterBand(1).GetMaskBand().ReadRaster(0, 0, width, height)
        )

        # Generate a JPEG file with a (default) LSB bit mask order
        out_ds = gdal.GetDriverByName("JPEG").CreateCopy("/vsimem/jpeg_19.jpg", src_ds)
        out_ds = None

        # Generate a JPEG file with a MSB bit mask order
        with gdal.config_option("JPEG_WRITE_MASK_BIT_ORDER", "MSB"):
            out_ds = gdal.GetDriverByName("JPEG").CreateCopy(
                "/vsimem/jpeg_19_msb.jpg", src_ds
            )
            del out_ds

        src_ds = None

        # Check that the file are indeed different
        statBuf = gdal.VSIStatL("/vsimem/jpeg_19.jpg")
        f = gdal.VSIFOpenL("/vsimem/jpeg_19.jpg", "rb")
        data1 = gdal.VSIFReadL(1, statBuf.size, f)
        gdal.VSIFCloseL(f)

        statBuf = gdal.VSIStatL("/vsimem/jpeg_19_msb.jpg")
        f = gdal.VSIFOpenL("/vsimem/jpeg_19_msb.jpg", "rb")
        data2 = gdal.VSIFReadL(1, statBuf.size, f)
        gdal.VSIFCloseL(f)

        if (width, height, iX) == (24, 25, 8):
            assert data1 == data2
        else:
            assert data1 != data2

        # Check the file with the LSB bit mask order
        ds = gdal.Open("/vsimem/jpeg_19.jpg")
        jpg_mask_data = (
            ds.GetRasterBand(1).GetMaskBand().ReadRaster(0, 0, width, height)
        )
        ds = None
        assert tiff_mask_data == jpg_mask_data

        # Check the file with the MSB bit mask order
        ds = gdal.Open("/vsimem/jpeg_19_msb.jpg")
        jpg_mask_data = (
            ds.GetRasterBand(1).GetMaskBand().ReadRaster(0, 0, width, height)
        )
        ds = None
        assert tiff_mask_data == jpg_mask_data

        gdal.GetDriverByName("GTiff").Delete("/vsimem/jpeg_19.tif")
        gdal.GetDriverByName("JPEG").Delete("/vsimem/jpeg_19.jpg")
        gdal.GetDriverByName("JPEG").Delete("/vsimem/jpeg_19_msb.jpg")


###############################################################################
# Test correct decection of LSB order in mask (#4351)


def test_jpeg_mask_lsb_order_issue_4351():

    src_ds = gdal.GetDriverByName("MEM").Create("", 15, 4, 3)
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(7, 2, 2, 1, b"\xff" * 2)
    tmpfilename = "/vsimem/test_jpeg_mask_lsb_order_issue_4351.jpg"
    assert gdal.GetDriverByName("JPEG").CreateCopy(tmpfilename, src_ds)
    ds = gdal.Open(tmpfilename)
    assert (
        ds.GetRasterBand(1).GetMaskBand().ReadRaster(0, 2, 15, 1)
        == b"\x00" * 7 + b"\xff" * 2 + b"\x00" * 6
    )
    ds = None
    gdal.GetDriverByName("JPEG").Delete(tmpfilename)


###############################################################################
# Test correct GCP reading with PAM (#5352)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_jpeg_20():

    src_ds = gdal.Open("data/rgb_gcp.vrt")
    ds = gdal.GetDriverByName("JPEG").CreateCopy("/vsimem/jpeg_20.jpg", src_ds)
    ds = None

    ds = gdal.Open("/vsimem/jpeg_20.jpg")
    assert ds.GetGCPProjection().find('GEOGCS["WGS 84"') == 0
    assert ds.GetGCPCount() == 4
    assert len(ds.GetGCPs()) == 4
    ds = None

    gdal.GetDriverByName("JPEG").Delete("/vsimem/jpeg_20.jpg")


###############################################################################
# Test implicit and EXIF overviews


def test_jpeg_21():

    ds = gdal.Open("data/jpeg/black_with_white_exif_ovr.jpg")
    assert ds.GetRasterBand(1).GetOverviewCount() == 3
    expected_dim_cs = [[512, 512, 0], [256, 256, 0], [196, 196, 12681]]
    i = 0
    for expected_w, expected_h, expected_cs in expected_dim_cs:
        ovr = ds.GetRasterBand(1).GetOverview(i)
        cs = ovr.Checksum()
        assert not (
            ovr.XSize != expected_w or ovr.YSize != expected_h or cs != expected_cs
        )
        i = i + 1
    ds = None


###############################################################################
# Test generation of EXIF overviews


def test_jpeg_22():

    src_ds = gdal.GetDriverByName("Mem").Create("", 4096, 2048)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName("JPEG").CreateCopy(
        "/vsimem/jpeg_22.jpg", src_ds, options=["EXIF_THUMBNAIL=YES"]
    )
    src_ds = None
    assert ds.GetRasterBand(1).GetOverviewCount() == 4
    ovr = ds.GetRasterBand(1).GetOverview(3)
    cs = ovr.Checksum()
    assert ovr.XSize == 128 and ovr.YSize == 64 and cs == 34957
    ds = None

    # With 3 bands
    src_ds = gdal.GetDriverByName("Mem").Create("", 2048, 4096, 3)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName("JPEG").CreateCopy(
        "/vsimem/jpeg_22.jpg", src_ds, options=["EXIF_THUMBNAIL=YES"]
    )
    src_ds = None
    ovr = ds.GetRasterBand(1).GetOverview(3)
    assert ovr.XSize == 64 and ovr.YSize == 128
    ds = None

    # With comment
    src_ds = gdal.GetDriverByName("Mem").Create("", 2048, 4096)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName("JPEG").CreateCopy(
        "/vsimem/jpeg_22.jpg",
        src_ds,
        options=["COMMENT=foo", "EXIF_THUMBNAIL=YES", "THUMBNAIL_WIDTH=40"],
    )
    src_ds = None
    ovr = ds.GetRasterBand(1).GetOverview(3)
    assert ds.GetMetadataItem("COMMENT") == "foo"
    assert ovr.XSize == 40 and ovr.YSize == 80
    ds = None

    src_ds = gdal.GetDriverByName("Mem").Create("", 2048, 4096)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName("JPEG").CreateCopy(
        "/vsimem/jpeg_22.jpg",
        src_ds,
        options=["EXIF_THUMBNAIL=YES", "THUMBNAIL_HEIGHT=60"],
    )
    src_ds = None
    ovr = ds.GetRasterBand(1).GetOverview(3)
    assert ovr.XSize == 30 and ovr.YSize == 60
    ds = None

    src_ds = gdal.GetDriverByName("Mem").Create("", 2048, 4096)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName("JPEG").CreateCopy(
        "/vsimem/jpeg_22.jpg",
        src_ds,
        options=["EXIF_THUMBNAIL=YES", "THUMBNAIL_WIDTH=50", "THUMBNAIL_HEIGHT=40"],
    )
    src_ds = None
    ovr = ds.GetRasterBand(1).GetOverview(3)
    assert ovr.XSize == 50 and ovr.YSize == 40
    ds = None

    gdal.Unlink("/vsimem/jpeg_22.jpg")


###############################################################################
# Test optimized JPEG IRasterIO


def test_jpeg_23():
    ds = gdal.Open("data/jpeg/albania.jpg")
    cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]

    # Band interleaved
    data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    tmp_ds = gdal.GetDriverByName("Mem").Create("", ds.RasterXSize, ds.RasterYSize, 3)
    tmp_ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data)
    got_cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == got_cs

    # Pixel interleaved
    ds = gdal.Open("data/jpeg/albania.jpg")
    data = ds.ReadRaster(
        0, 0, ds.RasterXSize, ds.RasterYSize, buf_pixel_space=3, buf_band_space=1
    )
    y = int(ds.RasterYSize / 2)
    data_bottom = ds.ReadRaster(
        0, y, ds.RasterXSize, ds.RasterYSize - y, buf_pixel_space=3, buf_band_space=1
    )
    data_top = ds.ReadRaster(
        0, 0, ds.RasterXSize, y, buf_pixel_space=3, buf_band_space=1
    )
    assert data == data_top + data_bottom
    tmp_ds = gdal.GetDriverByName("Mem").Create("", ds.RasterXSize, ds.RasterYSize, 3)
    tmp_ds.WriteRaster(
        0, 0, ds.RasterXSize, ds.RasterYSize, data, buf_pixel_space=3, buf_band_space=1
    )
    got_cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == got_cs

    # Pixel interleaved with padding
    ds = gdal.Open("data/jpeg/albania.jpg")
    data = ds.ReadRaster(
        0, 0, ds.RasterXSize, ds.RasterYSize, buf_pixel_space=4, buf_band_space=1
    )
    tmp_ds = gdal.GetDriverByName("Mem").Create("", ds.RasterXSize, ds.RasterYSize, 3)
    tmp_ds.WriteRaster(
        0, 0, ds.RasterXSize, ds.RasterYSize, data, buf_pixel_space=4, buf_band_space=1
    )
    got_cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert cs == got_cs


###############################################################################
# Test Arithmetic coding (and if not enabled, will trigger error code handling
# in CreateCopy())


def test_jpeg_24():

    has_arithmetic = bool(
        gdal.GetDriverByName("JPEG")
        .GetMetadataItem("DMD_CREATIONOPTIONLIST")
        .find("ARITHMETIC")
        >= 0
    )

    src_ds = gdal.Open("data/byte.tif")
    if not has_arithmetic:
        gdal.PushErrorHandler()
    ds = gdal.GetDriverByName("JPEG").CreateCopy(
        "/vsimem/byte.jpg", src_ds, options=["ARITHMETIC=YES"]
    )
    if not has_arithmetic:
        gdal.PopErrorHandler()
    else:
        if (
            gdal.GetLastErrorMsg().find("Requested feature was omitted at compile time")
            >= 0
        ):
            ds = None
            gdal.Unlink("/vsimem/byte.jpg")
            pytest.skip()

        expected_cs = 4743

        assert (
            ds.GetRasterBand(1).Checksum() == expected_cs
        ), "Wrong checksum on copied image."

        ds = None
        gdal.GetDriverByName("JPEG").Delete("/vsimem/byte.jpg")


###############################################################################
# Test COMMENT


def test_jpeg_25():

    src_ds = gdal.Open("data/byte.tif")
    ds = gdal.GetDriverByName("JPEG").CreateCopy(
        "/vsimem/byte.jpg", src_ds, options=["COMMENT=my comment"]
    )
    ds = None
    ds = gdal.Open("/vsimem/byte.jpg")
    if ds.GetMetadataItem("COMMENT") != "my comment":
        print(ds.GetMetadata())
        pytest.fail("Wrong comment.")

    ds = None
    gdal.GetDriverByName("JPEG").Delete("/vsimem/byte.jpg")


###############################################################################
# Test creation error


def test_jpeg_26():

    src_ds = gdal.GetDriverByName("Mem").Create("", 70000, 1)
    with gdal.quiet_errors():
        ds = gdal.GetDriverByName("JPEG").CreateCopy("/vsimem/jpeg_26.jpg", src_ds)
    assert ds is None
    gdal.Unlink("/vsimem/jpeg_26.jpg")


###############################################################################
# Test reading a file that contains the 2 denial of service
# vulnerabilities listed in
# http://www.libjpeg-jpeg_26.org/pmwiki/uploads/About/TwoIssueswiththeJPEGStandard.pdf


@pytest.mark.skipif(sys.platform == "win32", reason="Fails for some reason on Windows")
def test_jpeg_27_max_memory():

    # Should error out with 'Reading this image would require
    # libjpeg to allocate at least...'
    gdal.ErrorReset()
    with gdal.quiet_errors():
        os.environ["JPEGMEM"] = "10M"
        with gdal.config_option("GDAL_JPEG_MAX_ALLOWED_SCAN_NUMBER", "1000"):
            ds = gdal.Open(
                "/vsisubfile/146,/vsizip/../gcore/data/eofloop_valid_huff.tif.zip"
            )
            cs = ds.GetRasterBand(1).Checksum()
            del os.environ["JPEGMEM"]
        assert cs == -1 and gdal.GetLastErrorMsg() != ""


def test_jpeg_27_max_scan_number():

    # Should error out with 'Scan number...
    gdal.ErrorReset()
    ds = gdal.Open("/vsisubfile/146,/vsizip/../gcore/data/eofloop_valid_huff.tif.zip")

    options = {
        "GDAL_ALLOW_LARGE_LIBJPEG_MEM_ALLOC": "YES",
        "GDAL_JPEG_MAX_ALLOWED_SCAN_NUMBER": "10",
    }

    with gdal.config_options(options), gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    assert cs == -1 and gdal.GetLastErrorMsg() != ""


###############################################################################
# Test writing of EXIF and GPS tags


def test_jpeg_28():

    tmpfilename = "/vsimem/jpeg_28.jpg"

    # Nothing
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds = gdal.GetDriverByName("JPEG").CreateCopy(tmpfilename, src_ds)
    src_ds = None
    ds = gdal.Open(tmpfilename)
    assert not ds.GetMetadata()

    # EXIF tags only
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)

    src_ds.SetMetadataItem("EXIF_DateTime", "dt")  # not enough values ASCII
    src_ds.SetMetadataItem(
        "EXIF_DateTimeOriginal", "01234567890123456789"
    )  # truncated ASCII
    src_ds.SetMetadataItem(
        "EXIF_DateTimeDigitized", "0123456789012345678"
    )  # right number of items ASCII
    src_ds.SetMetadataItem("EXIF_Make", "make")  # variable ASCII

    src_ds.SetMetadataItem("EXIF_ExifVersion", "01234")  # truncated UNDEFINED
    src_ds.SetMetadataItem(
        "EXIF_ComponentsConfiguration", "0x1F"
    )  # not enough values UNDEFINED
    src_ds.SetMetadataItem(
        "EXIF_FlashpixVersion", "ABCD"
    )  # right number of items UNDEFINED
    src_ds.SetMetadataItem(
        "EXIF_SpatialFrequencyResponse", "0xab 0xCD"
    )  # variable UNDEFINED

    src_ds.SetMetadataItem("EXIF_Orientation", "10")  # right number of items SHORT
    src_ds.SetMetadataItem("EXIF_ResolutionUnit", "2 4")  # truncated SHORT
    src_ds.SetMetadataItem("EXIF_TransferFunction", "0 1")  # not enough values SHORT
    src_ds.SetMetadataItem("EXIF_ISOSpeedRatings", "1 2 3")  # variable SHORT

    src_ds.SetMetadataItem(
        "EXIF_StandardOutputSensitivity", "123456789"
    )  # right number of items LONG

    src_ds.SetMetadataItem("EXIF_XResolution", "96")  # right number of items RATIONAL
    src_ds.SetMetadataItem("EXIF_YResolution", "96 0")  # truncated RATIONAL
    src_ds.SetMetadataItem("EXIF_CompressedBitsPerPixel", "nan")  # invalid RATIONAL
    src_ds.SetMetadataItem("EXIF_ApertureValue", "-1")  # invalid RATIONAL

    with gdal.quiet_errors():
        gdal.GetDriverByName("JPEG").CreateCopy(tmpfilename, src_ds)
    src_ds = None
    assert gdal.VSIStatL(tmpfilename + ".aux.xml") is None
    ds = gdal.Open(tmpfilename)
    got_md = ds.GetMetadata()
    expected_md = {
        "EXIF_DateTimeDigitized": "0123456789012345678",
        "EXIF_DateTimeOriginal": "0123456789012345678",
        "EXIF_Orientation": "10",
        "EXIF_ApertureValue": "(0)",
        "EXIF_YResolution": "(96)",
        "EXIF_XResolution": "(96)",
        "EXIF_TransferFunction": "0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
        "EXIF_ExifVersion": "0123",
        "EXIF_DateTime": "dt",
        "EXIF_FlashpixVersion": "ABCD",
        "EXIF_ComponentsConfiguration": "0x1f 0x00 0x00 0x00",
        "EXIF_Make": "make",
        "EXIF_StandardOutputSensitivity": "123456789",
        "EXIF_ResolutionUnit": "2",
        "EXIF_CompressedBitsPerPixel": "(0)",
        "EXIF_SpatialFrequencyResponse": "0xab 0xcd",
        "EXIF_ISOSpeedRatings": "1 2 3",
    }
    assert got_md == expected_md

    # Test SRATIONAL
    for val in (-1.5, -1, -0.5, 0, 0.5, 1, 1.5):
        src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
        src_ds.SetMetadataItem("EXIF_ShutterSpeedValue", str(val))
        gdal.GetDriverByName("JPEG").CreateCopy(tmpfilename, src_ds)
        src_ds = None
        assert gdal.VSIStatL(tmpfilename + ".aux.xml") is None
        ds = gdal.Open(tmpfilename)
        got_val = ds.GetMetadataItem("EXIF_ShutterSpeedValue")
        got_val = got_val.replace("(", "").replace(")", "")
        assert float(got_val) == val, ds.GetMetadataItem("EXIF_ShutterSpeedValue")

    # Test RATIONAL
    for val in (0, 0.5, 1, 1.5):
        src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
        src_ds.SetMetadataItem("EXIF_ApertureValue", str(val))
        gdal.GetDriverByName("JPEG").CreateCopy(tmpfilename, src_ds)
        src_ds = None
        assert gdal.VSIStatL(tmpfilename + ".aux.xml") is None
        ds = gdal.Open(tmpfilename)
        got_val = ds.GetMetadataItem("EXIF_ApertureValue")
        got_val = got_val.replace("(", "").replace(")", "")
        assert float(got_val) == val, ds.GetMetadataItem("EXIF_ApertureValue")

    # GPS tags only
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetMetadataItem("EXIF_GPSLatitudeRef", "N")
    src_ds.SetMetadataItem("EXIF_GPSLatitude", "49 34 56.5")
    gdal.GetDriverByName("JPEG").CreateCopy(tmpfilename, src_ds)
    src_ds = None
    assert gdal.VSIStatL(tmpfilename + ".aux.xml") is None
    ds = gdal.Open(tmpfilename)
    got_md = ds.GetMetadata()
    assert got_md == {
        "EXIF_GPSLatitudeRef": "N",
        "EXIF_GPSLatitude": "(49) (34) (56.5)",
    }
    ds = None

    # EXIF and GPS tags
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetMetadataItem("EXIF_ExifVersion", "0231")
    src_ds.SetMetadataItem("EXIF_GPSLatitudeRef", "N")
    gdal.GetDriverByName("JPEG").CreateCopy(tmpfilename, src_ds)
    src_ds = None
    assert gdal.VSIStatL(tmpfilename + ".aux.xml") is None
    ds = gdal.Open(tmpfilename)
    got_md = ds.GetMetadata()
    assert got_md == {"EXIF_ExifVersion": "0231", "EXIF_GPSLatitudeRef": "N"}
    ds = None

    # EXIF and other metadata
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetMetadataItem("EXIF_ExifVersion", "0231")
    src_ds.SetMetadataItem("EXIF_invalid", "foo")
    src_ds.SetMetadataItem("FOO", "BAR")
    with gdal.quiet_errors():
        gdal.GetDriverByName("JPEG").CreateCopy(tmpfilename, src_ds)
    src_ds = None
    assert gdal.VSIStatL(tmpfilename + ".aux.xml") is not None
    ds = gdal.Open(tmpfilename)
    got_md = ds.GetMetadata()
    assert got_md == {"EXIF_ExifVersion": "0231", "EXIF_invalid": "foo", "FOO": "BAR"}
    ds = None

    # Too much content for EXIF
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetMetadataItem("EXIF_UserComment", "x" * 65535)
    with gdal.quiet_errors():
        gdal.GetDriverByName("JPEG").CreateCopy(tmpfilename, src_ds)
    src_ds = None
    ds = None

    # EXIF and GPS tags and EXIF overview
    src_ds = gdal.GetDriverByName("MEM").Create("", 1024, 1024)
    src_ds.SetMetadataItem("EXIF_ExifVersion", "0231")
    src_ds.SetMetadataItem("EXIF_GPSLatitudeRef", "N")
    gdal.GetDriverByName("JPEG").CreateCopy(
        tmpfilename,
        src_ds,
        options=["EXIF_THUMBNAIL=YES", "THUMBNAIL_WIDTH=32", "THUMBNAIL_HEIGHT=32"],
    )
    src_ds = None
    assert gdal.VSIStatL(tmpfilename + ".aux.xml") is None
    ds = gdal.Open(tmpfilename)
    got_md = ds.GetMetadata()
    assert got_md == {"EXIF_ExifVersion": "0231", "EXIF_GPSLatitudeRef": "N"}
    assert (
        ds.GetRasterBand(1)
        .GetOverview(ds.GetRasterBand(1).GetOverviewCount() - 1)
        .XSize
        == 32
    )
    ds = None

    gdal.Unlink(tmpfilename)


###############################################################################
# Test multiscan and overviews


def test_jpeg_multiscan_overviews():

    tmpfilename = "/vsimem/test_jpeg_multiscan_overviews.jpg"

    # Will require ~ 20 MB of libjpeg memory
    src_ds = gdal.GetDriverByName("MEM").Create("", 10000, 1000)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.GetDriverByName("JPEG").CreateCopy(
        tmpfilename, src_ds, options=["PROGRESSIVE=YES"]
    )
    src_ds = None
    ds = gdal.Open(tmpfilename)
    for y in (0, 1):
        assert struct.unpack("B", ds.GetRasterBand(1).ReadRaster(0, y, 1, 1))[0] == 255
        assert (
            struct.unpack(
                "B", ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, y, 1, 1)
            )[0]
            == 255
        )
        assert (
            struct.unpack(
                "B", ds.GetRasterBand(1).GetOverview(1).ReadRaster(0, y, 1, 1)
            )[0]
            == 255
        )
    ds = None

    gdal.Unlink(tmpfilename)


###############################################################################
# Open JPEG image with FLIR metadata and raw thermal image as PNG


def test_jpeg_flir_png():

    ds = gdal.Open("data/jpeg/flir/FLIR.jpg")
    assert ds.GetMetadataDomainList() == [
        "IMAGE_STRUCTURE",
        "",
        "FLIR",
        "SUBDATASETS",
        "DERIVED_SUBDATASETS",
    ]
    assert ds.GetMetadata("FLIR") == {
        "AboveColor": "170 128 128",
        "AtmosphericTemperature": "20.000000 C",
        "AtmosphericTransAlpha1": "0.006569",
        "AtmosphericTransAlpha2": "0.012620",
        "AtmosphericTransBeta1": "-0.002276",
        "AtmosphericTransBeta2": "-0.006670",
        "AtmosphericTransX": "1.900000",
        "BelowColor": "50 128 128",
        "CameraModel": "FLIR_i7",
        "CameraPartNumber": "T197600",
        "CameraSerialNumber": "470023842",
        "CameraSoftware": "8.1.1",
        "CameraTemperatureMaxClip": "270.000031 C",
        "CameraTemperatureMaxSaturated": "270.000031 C",
        "CameraTemperatureMaxWarn": "250.000031 C",
        "CameraTemperatureMinClip": "-40.000000 C",
        "CameraTemperatureMinSaturated": "-60.000000 C",
        "CameraTemperatureMinWarn": "0.000000 C",
        "CameraTemperatureRangeMax": "250.000031 C",
        "CameraTemperatureRangeMin": "0.000000 C",
        "DateTimeOriginal": "2012-02-11T14:17:08.253+01:00",
        "Emissivity": "0.800000",
        "FieldOfView": "24.985918 deg",
        "FocusDistance": "2.000000 m",
        "FocusStepCount": "0",
        "FrameRate": "9",
        "IRWindowTemperature": "20.000000 C",
        "IRWindowTransmission": "1.000000",
        "Isotherm1Color": "100 128 128",
        "Isotherm2Color": "100 110 240",
        "LensModel": "FOL7",
        "ObjectDistance": "1.000000 m",
        "OverflowColor": "67 216 98",
        "Palette": "(16 101 140), (17 103 142), (18 105 145), (19 106 147), (20 108 149), (21 110 152), (22 112 154), (23 114 156), (24 116 158), (25 118 160), (26 120 162), (27 121 164), (28 123 165), (29 125 167), (30 127 169), (31 128 170), (32 130 172), (33 132 173), (34 133 174), (35 135 175), (36 136 177), (37 138 178), (38 140 179), (39 141 180), (40 143 181), (41 144 181), (42 145 182), (43 147 183), (44 148 183), (44 150 184), (45 151 185), (46 152 185), (47 154 185), (48 155 186), (49 156 186), (50 157 186), (51 159 186), (52 160 186), (53 161 186), (54 162 186), (55 163 186), (56 165 186), (57 166 186), (58 167 186), (59 168 186), (60 169 185), (61 170 185), (62 171 185), (63 172 184), (64 173 184), (65 174 183), (66 175 182), (67 176 182), (68 177 181), (69 177 180), (70 178 180), (71 179 179), (72 180 178), (73 181 177), (74 182 176), (75 182 175), (76 183 174), (77 184 173), (78 184 172), (79 185 171), (80 186 170), (81 186 169), (82 187 168), (83 188 166), (84 188 165), (85 189 164), (86 189 163), (87 190 161), (88 190 160), (89 191 159), (90 191 157), (91 192 156), (92 192 154), (93 193 153), (94 193 151), (95 194 150), (96 194 148), (97 194 147), (98 195 145), (99 195 144), (99 195 142), (100 196 140), (101 196 139), (102 196 137), (103 197 135), (104 197 134), (105 197 132), (106 197 130), (107 197 129), (108 198 127), (109 198 125), (110 198 123), (111 198 122), (112 198 120), (113 198 118), (114 198 117), (115 198 115), (116 198 113), (117 198 111), (118 198 109), (119 198 108), (120 198 106), (121 198 104), (122 198 102), (123 198 101), (124 198 99), (125 198 97), (126 198 96), (127 198 94), (128 198 92), (129 198 90), (130 198 89), (131 197 87), (132 197 85), (133 197 84), (134 197 82), (135 197 80), (136 196 79), (137 196 77), (138 196 75), (139 196 74), (140 195 72), (141 195 71), (142 195 69), (143 194 68), (144 194 66), (145 194 65), (146 193 63), (147 193 62), (148 193 60), (149 192 59), (150 192 58), (151 191 56), (152 191 55), (153 191 54), (154 190 53), (154 190 51), (155 189 50), (156 189 49), (157 188 48), (158 188 47), (159 187 46), (160 187 45), (161 186 44), (162 186 43), (163 185 42), (164 184 41), (165 184 40), (166 183 40), (167 183 39), (168 182 38), (169 181 37), (170 181 37), (171 180 36), (172 179 36), (173 179 35), (174 178 35), (175 177 34), (176 177 34), (177 176 34), (178 175 33), (179 175 33), (180 174 33), (181 173 33), (182 172 33), (183 172 33), (184 171 33), (185 170 33), (186 169 33), (187 168 33), (188 168 34), (189 167 34), (190 166 34), (191 165 35), (192 164 35), (193 163 36), (194 163 37), (195 162 37), (196 161 38), (197 160 39), (198 159 40), (199 158 41), (200 157 42), (201 156 43), (202 155 44), (203 155 45), (204 154 46), (205 153 48), (206 152 49), (207 151 51), (208 150 52), (209 149 54), (209 148 56), (210 147 58), (211 146 60), (212 145 62), (213 144 64), (214 143 66), (215 142 68), (216 141 70), (217 140 73), (218 139 75), (219 138 78), (220 137 80), (221 136 83), (222 135 86), (223 134 89), (224 133 91), (225 131 95), (226 130 98), (227 129 101), (228 128 104), (229 127 108), (230 126 111), (231 125 115), (232 124 118), (233 123 122), (234 122 126), (235 121 130)",
        "PaletteColors": "224",
        "PaletteFileName": "\\FlashFS\\system\\iron.pal",
        "PaletteMethod": "0",
        "PaletteName": "Iron",
        "PaletteStretch": "2",
        "PlanckB": "1374.5",
        "PlanckF": "1.35",
        "PlanckO": "-6646",
        "PlanckR1": "13799.269",
        "PlanckR2": "0.022241818",
        "RawThermalImageHeight": "120",
        "RawThermalImageWidth": "120",
        "RawValueMedian": "12582",
        "RawValueRange": "1980",
        "RawValueRangeMax": "61986",
        "RawValueRangeMin": "7630",
        "ReflectedApparentTemperature": "20.000000 C",
        "RelativeHumidity": "50.000000 %",
        "UnderflowColor": "41 110 240",
    }
    subds = ds.GetSubDatasets()
    assert len(subds) == 1

    ds = gdal.Open(subds[0][0])
    assert ds is not None
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).Checksum() == 761


###############################################################################
# Open JPEG image with FLIR metadata and raw thermal image as PNG 16 bit


def test_jpeg_flir_png_16_bit(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "tmp.jpg", open("data/jpeg/flir/FLIR_16bit.jpg", "rb").read()
    )

    with gdal.Open(f'JPEG:"{tmp_vsimem}/tmp.jpg":FLIR_RAW_THERMAL_IMAGE') as ds:
        assert (
            ds.GetDescription() == f'JPEG:"{tmp_vsimem}/tmp.jpg":FLIR_RAW_THERMAL_IMAGE'
        )
        assert ds.GetFileList() == [str(tmp_vsimem / "tmp.jpg")]
        assert ds.GetDriver().GetDescription() == "PNG"
        assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
        assert ds.GetRasterBand(1).GetStatistics(False, True) == [
            65280.0,
            65280.0,
            65280.0,
            0.0,
        ]

    assert gdal.VSIStatL(tmp_vsimem / "tmp.jpg.aux.xml") is not None

    gdal.FileFromMemBuffer(
        tmp_vsimem / "tmp.jpg.aux.xml",
        """<PAMDataset>
  <Subdataset name="PNG_THERMAL_IMAGE">
    <PAMDataset>
      <PAMRasterBand band="1">
        <Metadata>
          <MDI key="STATISTICS_MINIMUM">1</MDI>
          <MDI key="STATISTICS_MAXIMUM">2</MDI>
          <MDI key="STATISTICS_MEAN">3</MDI>
          <MDI key="STATISTICS_STDDEV">4</MDI>
          <MDI key="STATISTICS_VALID_PERCENT">100</MDI>
        </Metadata>
      </PAMRasterBand>
    </PAMDataset>
  </Subdataset>
</PAMDataset>""",
    )

    with gdal.Open(f'JPEG:"{tmp_vsimem}/tmp.jpg":FLIR_RAW_THERMAL_IMAGE') as ds:
        assert ds.GetFileList() == [
            str(tmp_vsimem / "tmp.jpg"),
            str(tmp_vsimem / "tmp.jpg.aux.xml"),
        ]
        assert ds.GetRasterBand(1).GetStatistics(False, True) == [1, 2, 3, 4]


###############################################################################
# Open JPEG image with FLIR metadata and raw thermal image as raw


def test_jpeg_flir_raw(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "tmp.jpg",
        open(
            "data/jpeg/flir/Image_thermique_de_l_emission_d_un_radiateur_a_travers_un_mur.jpg",
            "rb",
        ).read(),
    )

    with gdal.Open(tmp_vsimem / "tmp.jpg") as ds:
        subds = ds.GetSubDatasets()
        assert len(subds) == 1

    with gdal.Open(subds[0][0]) as ds:
        assert ds is not None
        assert ds.GetDescription() == subds[0][0]
        assert ds.GetFileList() == [str(tmp_vsimem / "tmp.jpg")]
        assert ds.RasterCount == 1
        assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
        assert ds.GetRasterBand(1).GetStatistics(False, True) == [
            13872.0,
            16040.0,
            14657.947135416667,
            542.8919524629978,
        ]

    assert gdal.VSIStatL(tmp_vsimem / "tmp.jpg.aux.xml") is not None

    gdal.FileFromMemBuffer(
        tmp_vsimem / "tmp.jpg.aux.xml",
        """<PAMDataset>
  <Subdataset name="RAW_THERMAL_IMAGE">
    <PAMDataset>
      <PAMRasterBand band="1">
        <Metadata>
          <MDI key="STATISTICS_MINIMUM">1</MDI>
          <MDI key="STATISTICS_MAXIMUM">2</MDI>
          <MDI key="STATISTICS_MEAN">3</MDI>
          <MDI key="STATISTICS_STDDEV">4</MDI>
          <MDI key="STATISTICS_VALID_PERCENT">100</MDI>
        </Metadata>
      </PAMRasterBand>
    </PAMDataset>
  </Subdataset>
</PAMDataset>""",
    )

    with gdal.Open(subds[0][0]) as ds:
        assert ds.GetFileList() == [
            str(tmp_vsimem / "tmp.jpg"),
            str(tmp_vsimem / "tmp.jpg.aux.xml"),
        ]
        assert ds.GetRasterBand(1).GetStatistics(False, True) == [1, 2, 3, 4]


###############################################################################


def test_jpeg_flir_error_flir_subds():

    with gdal.quiet_errors():
        ds = gdal.Open("JPEG:foo.jpg")
        assert ds is None

    with gdal.quiet_errors():
        ds = gdal.Open("JPEG:foo.jpg:BAR")
        assert ds is None

    with gdal.quiet_errors():
        ds = gdal.Open("JPEG:data/jpeg/masked.jpg:FLIR_RAW_THERMAL_IMAGE")
        assert ds is None


###############################################################################
# Open JPEG image with DJI raw thermal image as raw


def test_jpeg_dji_thermal_metadata(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.jpg", open("data/jpeg/dji/DJI_M3T.JPG", "rb").read()
    )

    with gdal.Open(tmp_vsimem / "test.jpg") as ds:
        assert sorted(ds.GetMetadataDomainList()) == [
            "",
            "DERIVED_SUBDATASETS",
            "DJI",
            "IMAGE_STRUCTURE",
            "SUBDATASETS",
            "xml:XMP",
        ]
        assert ds.GetMetadata("DJI") == {
            "RawThermalImageHeight": "512",
            "RawThermalImageWidth": "640",
        }
        assert ds.RasterCount == 3
        subds = ds.GetSubDatasets()
        assert len(subds) == 1

    assert gdal.VSIStatL(tmp_vsimem / "test.jpg.aux.xml") is None

    ds = gdal.Open(subds[0][0])
    assert ds is not None
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert ds.GetRasterBand(1).Checksum() == 50952


def test_jpeg_dji_thermal_raw():

    ds = gdal.Open('JPEG:"data/jpeg/dji/DJI_M3T.JPG":DJI_RAW_THERMAL_IMAGE')
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert ds.GetRasterBand(1).Checksum() == 50952


###############################################################################
# Open JPEG image with FLIR embedded jpeg image


def test_jpeg_flir_embedded_image_metadata(tmp_vsimem):

    # data/jpeg/flir/flir_embedded.jpg has been generated from autotest/gdrivers/data/jpeg/flir/FLIR.jpg
    # by hacking record 2 of type 32 (FLIR_REC_CAMERA_INFO) with data at offset 0xC7E + 1352
    # and replacing it by type 14 (FLIR_REC_EMBEDDEDIMAGE)

    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.jpg", open("data/jpeg/flir/flir_embedded.jpg", "rb").read()
    )

    with gdal.Open(tmp_vsimem / "test.jpg") as ds:
        assert sorted(ds.GetMetadataDomainList()) == [
            "",
            "DERIVED_SUBDATASETS",
            "FLIR",
            "IMAGE_STRUCTURE",
            "SUBDATASETS",
        ]
        metadata = ds.GetMetadata("FLIR")
        assert "EmbeddedImageHeight" in metadata
        assert metadata.get("EmbeddedImageHeight") == "1"
        assert metadata.get("EmbeddedImageWidth") == "2"

        assert ds.RasterCount == 3
        assert ds.GetRasterBand(1).Checksum() == 761
        subds = ds.GetSubDatasets()
        assert len(subds) == 2

    assert gdal.VSIStatL(tmp_vsimem / "test.jpg.aux.xml") is None

    assert "FLIR_EMBEDDED_IMAGE" in subds[0][0]
    assert "FLIR embedded image" == subds[0][1]
    ds = gdal.Open(subds[0][0])
    assert ds is not None
    assert ds.RasterCount == 1
    assert ds.RasterXSize == 2
    assert ds.RasterYSize == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Byte
    # this image is different than the "main" image
    assert ds.GetRasterBand(1).Checksum() == 7


def test_jpeg_flir_embedded_image():

    ds = gdal.Open('JPEG:"data/jpeg/flir/flir_embedded.jpg":FLIR_EMBEDDED_IMAGE')
    assert ds is not None
    assert ds.RasterCount == 1
    assert ds.RasterXSize == 2
    assert ds.RasterYSize == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Byte
    assert ds.GetRasterBand(1).Checksum() == 7


###############################################################################
# Write a CMYK image


def test_jpeg_write_cmyk():

    with gdaltest.config_option("GDAL_JPEG_TO_RGB", "NO"):
        src_ds = gdal.Open("data/jpeg/rgb_ntf_cmyk.jpg")

    gdal.GetDriverByName("JPEG").CreateCopy("/vsimem/out.jpg", src_ds)
    assert gdal.GetLastErrorMsg() == ""
    gdal.Unlink("/vsimem/out.jpg.aux.xml")

    with gdaltest.config_option("GDAL_JPEG_TO_RGB", "NO"):
        ds = gdal.Open("/vsimem/out.jpg")

    assert ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_CyanBand

    ds = None
    gdal.Unlink("/vsimem/out.jpg")


###############################################################################
# Attempt writing a 4-band image not CMYK


def test_jpeg_write_4band_not_cmyk():

    src_ds = gdal.GetDriverByName("MEM").Create("", 8, 8, 4)
    with gdal.quiet_errors():
        gdal.GetDriverByName("JPEG").CreateCopy("/vsimem/out.jpg", src_ds)
    assert gdal.GetLastErrorMsg() != ""
    gdal.GetDriverByName("JPEG").Delete("/vsimem/out.jpg")


###############################################################################
# Test APPLY_ORIENTATION=YES open option


@pytest.mark.parametrize("orientation", [i + 1 for i in range(8)])
def test_jpeg_apply_orientation(orientation):

    ds = gdal.OpenEx(
        "data/jpeg/exif_orientation/F%d.jpg" % orientation,
        open_options=["APPLY_ORIENTATION=YES"],
    )
    assert ds.RasterXSize == 3
    assert ds.RasterYSize == 5
    vals = struct.unpack("B" * 3 * 5, ds.ReadRaster())
    vals = [1 if v else 0 for v in vals]
    assert vals == [1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0]
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    if orientation != 1:
        assert ds.GetMetadataItem("EXIF_Orientation") is None
        assert ds.GetMetadataItem("original_EXIF_Orientation") == str(orientation)


###############################################################################
# Test lossless conversion from JPEGXL


@pytest.mark.require_creation_option("JPEGXL", "COMPRESS_BOXES")
def test_jpeg_from_jpegxl():

    jpegxl_drv = gdal.GetDriverByName("JPEGXL")
    if jpegxl_drv is None:
        pytest.skip("JPEGXL driver missing")

    src_ds = gdal.Open("data/jpeg/albania.jpg")

    # Lossless JPEG -> JPEGXL conversion
    tmp_filename = "/vsimem/temp.jxl"
    tmp_ds = jpegxl_drv.CreateCopy(tmp_filename, src_ds)

    # Lossless JPEGXL -> JPEG  conversion
    out_filename = "/vsimem/out.jpg"
    out_ds = gdal.Translate(out_filename, tmp_ds, metadataOptions=["foo=bar"])
    tmp_ds = None

    # Check data is preserved
    assert out_ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert out_ds.GetRasterBand(2).Checksum() == src_ds.GetRasterBand(2).Checksum()
    assert out_ds.GetRasterBand(3).Checksum() == src_ds.GetRasterBand(3).Checksum()
    assert out_ds.GetMetadataItem("EXIF_ExifVersion") == "0210"
    out_ds = None

    # Check data is preserved after file reopening
    out_ds = gdal.Open(out_filename)
    assert out_ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert out_ds.GetRasterBand(2).Checksum() == src_ds.GetRasterBand(2).Checksum()
    assert out_ds.GetRasterBand(3).Checksum() == src_ds.GetRasterBand(3).Checksum()
    assert out_ds.GetMetadataItem("EXIF_ExifVersion") == "0210"
    assert out_ds.GetMetadataItem("foo") == "bar"
    out_ds = None

    gdal.Unlink(out_filename + ".aux.xml")
    out_ds = gdal.Open(out_filename)
    assert out_ds.GetMetadataItem("EXIF_ExifVersion") == "0210"
    out_ds = None

    jpegxl_drv.Delete(tmp_filename)
    gdal.GetDriverByName("JPEG").Delete(out_filename)


def test_jpeg_read_arcgis_geodataxform_gcp():

    ds = gdal.Open("data/jpeg/arcgis_geodataxform_gcp.jpg")
    assert ds.GetGCPProjection().find("26712") >= 0
    assert ds.GetGCPCount() == 4
    gcp = ds.GetGCPs()[0]
    assert (
        gcp.GCPPixel == pytest.approx(565, abs=1e-5)
        and gcp.GCPLine == pytest.approx(11041, abs=1e-5)
        and gcp.GCPX == pytest.approx(500000, abs=1e-5)
        and gcp.GCPY == pytest.approx(4705078.79016612, abs=1e-5)
        and gcp.GCPZ == pytest.approx(0, abs=1e-5)
    )


def test_jpeg_read_arcgis_metadata_geodataxform_gcp():

    ds = gdal.Open("data/jpeg/arcgis_metadata_geodataxform_gcp.jpg")
    assert ds.GetGCPProjection().find("26712") >= 0
    assert ds.GetGCPCount() == 4
    gcp = ds.GetGCPs()[0]
    assert (
        gcp.GCPPixel == pytest.approx(565, abs=1e-5)
        and gcp.GCPLine == pytest.approx(11041, abs=1e-5)
        and gcp.GCPX == pytest.approx(500000, abs=1e-5)
        and gcp.GCPY == pytest.approx(4705078.79016612, abs=1e-5)
        and gcp.GCPZ == pytest.approx(0, abs=1e-5)
    )


###############################################################################
# Test reading lossless JPEG with libjpeg-turbo 2.2


def test_jpeg_read_lossless():

    if (
        gdal.GetDriverByName("JPEG").GetMetadataItem("LOSSLESS_JPEG_SUPPORTED", "JPEG")
        is None
    ):
        pytest.skip("lossless jpeg not supported")
    ds = gdal.Open("data/jpeg/byte_lossless.jpg")
    assert ds
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert (
        ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE") == "LOSSLESS"
    )


###############################################################################
# Test reading 16-bit lossless JPEG with libjpeg-turbo 2.2 (unsupported by GDAL)


def test_jpeg_read_lossless_16bit():

    if (
        gdal.GetDriverByName("JPEG").GetMetadataItem("LOSSLESS_JPEG_SUPPORTED", "JPEG")
        is None
    ):
        pytest.skip("lossless jpeg not supported")
    with gdal.quiet_errors():
        ds = gdal.Open("data/jpeg/uint16_lossless.jpg")
        assert ds is None


###############################################################################
def test_jpeg_copy_mdd():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetMetadataItem("FOO", "BAR")
    src_ds.SetMetadataItem("BAR", "BAZ", "OTHER_DOMAIN")
    src_ds.SetMetadataItem("should_not", "be_copied", "IMAGE_STRUCTURE")

    filename = "/vsimem/test_jpeg_copy_mdd.jpg"

    gdal.GetDriverByName("JPEG").CreateCopy(filename, src_ds)
    ds = gdal.Open(filename)
    assert set(ds.GetMetadataDomainList()) == set(
        ["", "IMAGE_STRUCTURE", "DERIVED_SUBDATASETS"]
    )
    assert ds.GetMetadata_Dict() == {"FOO": "BAR"}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {}
    ds = None

    gdal.GetDriverByName("JPEG").CreateCopy(
        filename, src_ds, options=["COPY_SRC_MDD=NO"]
    )
    ds = gdal.Open(filename)
    assert ds.GetMetadata_Dict() == {}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {}
    ds = None

    gdal.GetDriverByName("JPEG").CreateCopy(
        filename, src_ds, options=["COPY_SRC_MDD=YES"]
    )
    ds = gdal.Open(filename)
    assert set(ds.GetMetadataDomainList()) == set(
        ["IMAGE_STRUCTURE", "", "DERIVED_SUBDATASETS", "OTHER_DOMAIN"]
    )
    assert ds.GetMetadata_Dict() == {"FOO": "BAR"}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {"BAR": "BAZ"}
    ds = None

    gdal.GetDriverByName("JPEG").CreateCopy(
        filename, src_ds, options=["SRC_MDD=OTHER_DOMAIN"]
    )
    ds = gdal.Open(filename)
    assert ds.GetMetadata_Dict() == {}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {"BAR": "BAZ"}
    ds = None

    gdal.GetDriverByName("JPEG").CreateCopy(
        filename, src_ds, options=["SRC_MDD=", "SRC_MDD=OTHER_DOMAIN"]
    )
    ds = gdal.Open(filename)
    assert ds.GetMetadata_Dict() == {"FOO": "BAR"}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {"BAR": "BAZ"}
    ds = None

    gdal.Unlink(filename)


###############################################################################


def test_jpeg_read_DNG_tags():

    # File generated with:
    # gdal_translate autotest/gcore/data/byte.tif DNG_CameraSerialNumber_and_DNG_UniqueCameraModel.jpg
    # exiftool "-CameraSerialNumber=SerialNumber" "-UniqueCameraModel=CameraModel" DNG_CameraSerialNumber_and_DNG_UniqueCameraModel.jpg
    ds = gdal.Open("data/jpeg/DNG_CameraSerialNumber_and_DNG_UniqueCameraModel.jpg")
    assert ds.GetMetadataItem("DNG_CameraSerialNumber") == "SerialNumber"
    assert ds.GetMetadataItem("DNG_UniqueCameraModel") == "CameraModel"


###############################################################################


def test_jpeg_read_DNG_tags_same_value_ax_EXIF():
    """Check that DNG tags are not emitted when they have a corresponding EXIF
    tag at the same value."""

    # File generated with:
    # gdal_translate autotest/gcore/data/byte.tif DNG_and_EXIF_same_values.jpg
    # exiftool"-exif:SerialNumber=SerialNumber" "-CameraSerialNumber=SerialNumber" "-UniqueCameraModel=CameraModel" "-Model=CameraModel" DNG_and_EXIF_same_values.jpg
    ds = gdal.Open("data/jpeg/DNG_and_EXIF_same_values.jpg")
    assert ds.GetMetadataItem("DNG_CameraSerialNumber") is None
    assert ds.GetMetadataItem("DNG_UniqueCameraModel") is None
    assert ds.GetMetadataItem("EXIF_BodySerialNumber") == "SerialNumber"
    assert ds.GetMetadataItem("EXIF_Model") == "CameraModel"


###############################################################################


def test_jpeg_read_pix4d_xmp_crs_vertcs_orthometric():

    # File generated with:
    # gdal_translate autotest/gcore/data/byte.tif pix4d_xmp_crs_vertcs_orthometric.jpg
    # exiftool "-xmp<=pix4d_xmp_crs_vertcs_orthometric.xml"  pix4d_xmp_crs_vertcs_orthometric.jpg
    # where pix4d_xmp_crs_vertcs_orthometric.xml is the XMP content
    ds = gdal.Open("data/jpeg/pix4d_xmp_crs_vertcs_orthometric.jpg")
    assert ds.GetMetadataDomainList() == [
        "xml:XMP",
        "IMAGE_STRUCTURE",
        "DERIVED_SUBDATASETS",
    ]
    srs = ds.GetSpatialRef()
    assert srs.GetAuthorityCode("GEOGCS") == "6318"
    assert srs.GetAuthorityCode("VERT_CS") == "6360"


###############################################################################


def test_jpeg_read_pix4d_xmp_crs_vertcs_ellipsoidal():

    # File generated with:
    # gdal_translate autotest/gcore/data/byte.tif pix4d_xmp_crs_vertcs_ellipsoidal.jpg
    # exiftool "-xmp<=pix4d_xmp_crs_vertcs_ellipsoidal.xml"  pix4d_xmp_crs_vertcs_ellipsoidal.jpg
    # where pix4d_xmp_crs_vertcs_ellipsoidal.xml is the XMP content
    ds = gdal.Open("data/jpeg/pix4d_xmp_crs_vertcs_ellipsoidal.jpg")
    srs = ds.GetSpatialRef()
    assert srs.GetAuthorityCode(None) == "6319"


###############################################################################


def test_jpeg_create_copy_only_visible_at_close_time(tmp_path):

    src_ds = gdal.Open("data/byte.tif")
    out_filename = tmp_path / "tmp.jpg"

    def my_callback(pct, msg, user_data):
        if pct < 1:
            assert gdal.VSIStatL(out_filename) is None
        return True

    drv = gdal.GetDriverByName("JPEG")
    assert drv.GetMetadataItem(gdal.DCAP_CREATE_ONLY_VISIBLE_AT_CLOSE_TIME) == "YES"
    drv.CreateCopy(
        out_filename,
        src_ds,
        options=["@CREATE_ONLY_VISIBLE_AT_CLOSE_TIME=YES"],
        callback=my_callback,
    )

    with gdal.Open(out_filename) as ds:
        ds.GetRasterBand(1).Checksum()


###############################################################################


def test_jpeg_optimized_rasterio():

    ds = gdal.Open("data/jpeg/rgbsmall_rgb.jpg")
    assert ds.ReadRaster() == b"".join(
        [ds.GetRasterBand(i + 1).ReadRaster() for i in range(3)]
    )

    assert ds.ReadRaster(
        buf_obj=bytearray(b"\x00" * (50 * 50 * 3 * 2)), buf_pixel_space=2
    ) == b"".join(
        [
            ds.GetRasterBand(i + 1).ReadRaster(
                buf_obj=bytearray(b"\x00" * (50 * 50 * 2)), buf_pixel_space=2
            )
            for i in range(3)
        ]
    )
