#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  Test LIBERTIFF driver
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import glob
import math
import os
import threading

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("LIBERTIFF")


def libertiff_open(filename):
    return gdal.OpenEx(filename, allowed_drivers=["LIBERTIFF"])


def test_libertiff_basic():
    ds = libertiff_open("data/byte.tif")
    assert ds.GetMetadataItem("AREA_OR_POINT") == "Area"
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert ds.GetGeoTransform() == pytest.approx(
        (440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0)
    )
    assert ds.GetGCPCount() == 0
    assert ds.GetGCPSpatialRef() is None
    assert len(ds.GetGCPs()) == 0
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(1).GetColorTable() is None
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID
    assert ds.GetRasterBand(1).GetMaskBand().ComputeRasterMinMax() == (255, 255)
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetRasterBand(1).GetOffset() is None
    assert ds.GetRasterBand(1).GetScale() is None
    assert ds.GetRasterBand(1).GetDescription() == ""
    assert ds.GetRasterBand(1).GetUnitType() == ""
    assert ds.GetRasterBand(1).GetMetadataItem("JPEGTABLES", "TIFF") is None
    assert ds.GetRasterBand(1).GetMetadataItem("IFD_OFFSET", "TIFF") == "408"
    assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF") == "8"
    assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_0_1", "TIFF") is None
    assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_1_0", "TIFF") is None
    assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_SIZE_0_0", "TIFF") == "400"
    assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_SIZE_0_1", "TIFF") is None
    assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_SIZE_1_0", "TIFF") is None
    assert (
        ds.GetRasterBand(1).InterpolateAtPoint(0, 0, gdal.GRIORA_NearestNeighbour)
        == 107.0
    )
    with pytest.raises(Exception):
        ds.GetRasterBand(1).Fill(0)


@pytest.mark.require_driver("GTiff")
def test_libertiff_basic_compare_gtiff():
    ds = libertiff_open("data/byte.tif")
    ds_ref = gdal.OpenEx("data/byte.tif", allowed_drivers=["GTiff"])
    assert ds.ReadRaster() == ds_ref.ReadRaster()
    assert ds.ReadRaster(1, 2, 1, 1) == ds_ref.ReadRaster(1, 2, 1, 1)
    assert ds.ReadRaster(band_list=[1, 1]) == ds_ref.ReadRaster(band_list=[1, 1])
    assert ds.ReadRaster(1, 2, 3, 4) == ds_ref.ReadRaster(1, 2, 3, 4)
    assert ds.ReadRaster(buf_type=gdal.GDT_Int16) == ds_ref.ReadRaster(
        buf_type=gdal.GDT_Int16
    )
    assert ds.ReadRaster(buf_xsize=3, buf_ysize=5) == ds_ref.ReadRaster(
        buf_xsize=3, buf_ysize=5
    )
    assert ds.ReadRaster(
        buf_xsize=3, buf_ysize=5, resample_alg=gdal.GRIORA_Bilinear
    ) == ds_ref.ReadRaster(buf_xsize=3, buf_ysize=5, resample_alg=gdal.GRIORA_Bilinear)


@pytest.mark.require_driver("GTiff")
@pytest.mark.parametrize("GDAL_FORCE_CACHING", ["YES", "NO"])
def test_libertiff_3band_separate(GDAL_FORCE_CACHING):
    with gdal.config_option("GDAL_FORCE_CACHING", GDAL_FORCE_CACHING):
        ds = libertiff_open("data/rgbsmall.tif")
    ds_ref = gdal.OpenEx("data/rgbsmall.tif", allowed_drivers=["GTiff"])
    assert ds.ReadRaster() == ds_ref.ReadRaster()
    assert ds.GetRasterBand(2).ReadRaster() == ds_ref.GetRasterBand(2).ReadRaster()
    assert ds.ReadRaster(1, 2, 1, 1) == ds_ref.ReadRaster(1, 2, 1, 1)
    assert ds.ReadRaster(band_list=[3, 2, 1]) == ds_ref.ReadRaster(band_list=[3, 2, 1])
    assert ds.ReadRaster(1, 2, 3, 4) == ds_ref.ReadRaster(1, 2, 3, 4)
    assert ds.ReadRaster(buf_type=gdal.GDT_Int16) == ds_ref.ReadRaster(
        buf_type=gdal.GDT_Int16
    )
    assert ds.ReadRaster(buf_xsize=3, buf_ysize=5) == ds_ref.ReadRaster(
        buf_xsize=3, buf_ysize=5
    )
    assert ds.ReadRaster(
        1.5, 2.5, 6.5, 7.5, buf_xsize=3, buf_ysize=5, resample_alg=gdal.GRIORA_Bilinear
    ) == ds_ref.ReadRaster(
        1.5, 2.5, 6.5, 7.5, buf_xsize=3, buf_ysize=5, resample_alg=gdal.GRIORA_Bilinear
    )


@pytest.mark.require_driver("GTiff")
@pytest.mark.parametrize("GDAL_FORCE_CACHING", ["YES", "NO"])
def test_libertiff_4band_pixel_interleaved(GDAL_FORCE_CACHING):
    with gdal.config_option("GDAL_FORCE_CACHING", GDAL_FORCE_CACHING):
        ds = libertiff_open("data/stefan_full_rgba.tif")
    ds_ref = gdal.OpenEx("data/stefan_full_rgba.tif", allowed_drivers=["GTiff"])
    assert ds.ReadRaster() == ds_ref.ReadRaster()
    assert ds.ReadRaster(buf_pixel_space=4, buf_band_space=1) == ds_ref.ReadRaster(
        buf_pixel_space=4, buf_band_space=1
    )
    assert ds.ReadRaster(band_list=[3, 2, 1, 4]) == ds_ref.ReadRaster(
        band_list=[3, 2, 1, 4]
    )
    assert ds.ReadRaster(band_list=[3, 2, 1]) == ds_ref.ReadRaster(band_list=[3, 2, 1])


def test_libertiff_with_ovr():
    ds = libertiff_open("data/byte_with_ovr.tif")
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert ds.GetRasterBand(1).GetOverview(-1) is None
    assert ds.GetRasterBand(1).GetOverview(2) is None
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1087
    assert ds.GetRasterBand(1).GetOverview(1).Checksum() == 328
    assert (
        ds.GetRasterBand(1).ReadRaster(buf_xsize=10, buf_ysize=10)
        == ds.GetRasterBand(1).GetOverview(0).ReadRaster()
    )


@pytest.mark.require_driver("JPEG")
def test_libertiff_with_mask():
    ds = libertiff_open("data/ycbcr_with_mask.tif")
    assert ds.GetSpatialRef() is not None
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 1023
    assert ds.GetRasterBand(1).GetMetadataItem("JPEGTABLES", "TIFF") is not None


def test_libertiff_nodata():
    ds = libertiff_open("data/nan32_nodata.tif")
    assert math.isnan(ds.GetRasterBand(1).GetNoDataValue())


def test_libertiff_sparse_nodata_one():
    ds = libertiff_open("data/gtiff/sparse_nodata_one.tif")
    assert ds.GetRasterBand(1).GetNoDataValue() == 1
    assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF") is None
    assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_SIZE_0_0", "TIFF") is None
    assert ds.GetRasterBand(1).Checksum() == 1
    assert ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1) == b"\x01"
    assert ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1) == b"\x01"


@pytest.mark.parametrize("filename", ["sparse_tiled_separate", "sparse_tiled_contig"])
def test_libertiff_sparse_tiled(filename):
    ds = libertiff_open(f"data/gtiff/{filename}.tif")
    assert ds.GetRasterBand(1).ReadRaster() == b"\x01" * (40 * 21)
    assert ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1) == b"\x01"


def test_libertiff_metadata():
    ds = libertiff_open("data/complex_float32.tif")
    assert ds.GetMetadata() == {
        "bands": "1",
        "byte_order": "0",
        "data_type": "6",
        "description": "File Imported into ENVI.",
        "file_type": "ENVI Standard",
        "header_offset": "0",
        "interleave": "bsq",
        "lines": "4148",
        "samples": "9400",
        "sensor_type": "Unknown",
        "wavelength_units": "Unknown",
    }


def test_libertiff_check_signed_int8():
    ds = libertiff_open("../gdrivers/data/gtiff/int8.tif")
    assert ds.GetRasterBand(1).Checksum() == 1046


@pytest.mark.parametrize(
    "filename", ["int16.tif", "gtiff/int16_big_endian.tif", "int32.tif", "int64.tif"]
)
def test_libertiff_check_signed_int(filename):
    ds = libertiff_open("data/" + filename)
    assert ds.GetRasterBand(1).Checksum() == 4672


@pytest.mark.parametrize(
    "filename",
    [
        "cint16.tif",
        "cint32.tif",
        "gtiff/cint32_big_endian.tif",
        "cfloat32.tif",
        "cfloat64.tif",
    ],
)
def test_libertiff_check_complex(filename):
    ds = libertiff_open("data/" + filename)
    assert ds.GetRasterBand(1).Checksum() == 5028


@pytest.mark.parametrize(
    "filename_middle",
    [
        "DEFLATE",
        "DEFLATE_tiled",
        "JPEG",
        "JPEG_tiled",
        "JXL",
        "JXL_tiled",
        "LERC_DEFLATE",
        "LERC_DEFLATE_tiled",
        "LERC",
        "LERC_tiled",
        "LERC_ZSTD",
        "LERC_ZSTD_tiled",
        "little_endian_golden",
        "little_endian_tiled_lzw_golden",
        "LZMA",
        "LZMA_tiled",
        "LZW_predictor_2",
        "LZW",
        "LZW_tiled",
        "NONE",
        "NONE_tiled",
        "ZSTD",
        "ZSTD_tiled",
    ],
)
def test_libertiff_check_byte(filename_middle):
    if "JPEG" in filename_middle and gdal.GetDriverByName("JPEG") is None:
        pytest.skip("JPEG driver missing")
    if "JXL" in filename_middle and gdal.GetDriverByName("JPEGXL") is None:
        pytest.skip("JPEGXL driver missing")
    if "WEBP" in filename_middle and gdal.GetDriverByName("WEBP") is None:
        pytest.skip("WEBP driver missing")
    if (
        "LZMA" in filename_middle
        and gdal.GetDriverByName("LIBERTIFF").GetMetadataItem(
            "LZMA_SUPPORT", "LIBERTIFF"
        )
        is None
    ):
        pytest.skip("LZMA support missing")
    if (
        "LERC" in filename_middle
        and gdal.GetDriverByName("LIBERTIFF").GetMetadataItem(
            "LERC_SUPPORT", "LIBERTIFF"
        )
        is None
    ):
        pytest.skip("LERC support missing")
    if (
        "ZSTD" in filename_middle
        and gdal.GetDriverByName("LIBERTIFF").GetMetadataItem(
            "ZSTD_SUPPORT", "LIBERTIFF"
        )
        is None
    ):
        pytest.skip("ZSTD support missing")

    ds = libertiff_open(f"data/gtiff/byte_{filename_middle}.tif")
    if "JPEG" in filename_middle:
        assert ds.GetRasterBand(1).Checksum() > 0
        if gdal.GetDriverByName("GTiff"):
            ds_ref = gdal.OpenEx(
                f"data/gtiff/byte_{filename_middle}.tif", allowed_drivers=["GTiff"]
            )
            assert ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
    else:
        assert ds.GetRasterBand(1).Checksum() == 4672


@pytest.mark.parametrize(
    "filename_middle",
    [
        "byte_LZW_predictor_2",
        "DEFLATE_separate",
        "DEFLATE",
        "DEFLATE_tiled_separate",
        "DEFLATE_tiled",
        "JPEG_separate",
        "JPEG",
        "JPEG_tiled_separate",
        "JPEG_tiled",
        "JPEG_ycbcr",
        "JXL_separate",
        "JXL",
        "JXL_tiled_separate",
        "JXL_tiled",
        "LERC_DEFLATE_separate",
        "LERC_DEFLATE",
        "LERC_DEFLATE_tiled_separate",
        "LERC_DEFLATE_tiled",
        "LERC_separate",
        "LERC",
        "LERC_tiled_separate",
        "LERC_tiled",
        "LERC_ZSTD_separate",
        "LERC_ZSTD",
        "LERC_ZSTD_tiled_separate",
        "LERC_ZSTD_tiled",
        "LZMA_separate",
        "LZMA",
        "LZMA_tiled_separate",
        "LZMA_tiled",
        "LZW_separate",
        "LZW",
        "LZW_tiled_separate",
        "LZW_tiled",
        "NONE_separate",
        "NONE",
        "NONE_tiled_separate",
        "NONE_tiled",
        "uint16_LZW_predictor_2",
        "uint32_LZW_predictor_2",
        "uint64_LZW_predictor_2",
        "WEBP",
        "WEBP_tiled",
        "ZSTD_separate",
        "ZSTD",
        "ZSTD_tiled_separate",
        "ZSTD_tiled",
        "int16_bigendian_lzw_predictor_2",
    ],
)
def test_libertiff_check_rgbsmall(filename_middle):
    if "JPEG" in filename_middle and gdal.GetDriverByName("JPEG") is None:
        pytest.skip("JPEG driver missing")
    if "JXL" in filename_middle and gdal.GetDriverByName("JPEGXL") is None:
        pytest.skip("JPEGXL driver missing")
    if "WEBP" in filename_middle and gdal.GetDriverByName("WEBP") is None:
        pytest.skip("WEBP driver missing")
    if (
        "LZMA" in filename_middle
        and gdal.GetDriverByName("LIBERTIFF").GetMetadataItem(
            "LZMA_SUPPORT", "LIBERTIFF"
        )
        is None
    ):
        pytest.skip("LZMA support missing")
    if (
        "LERC" in filename_middle
        and gdal.GetDriverByName("LIBERTIFF").GetMetadataItem(
            "LERC_SUPPORT", "LIBERTIFF"
        )
        is None
    ):
        pytest.skip("LERC support missing")
    if (
        "ZSTD" in filename_middle
        and gdal.GetDriverByName("LIBERTIFF").GetMetadataItem(
            "ZSTD_SUPPORT", "LIBERTIFF"
        )
        is None
    ):
        pytest.skip("ZSTD support missing")

    ds = libertiff_open(f"data/gtiff/rgbsmall_{filename_middle}.tif")
    if "JPEG" in filename_middle:
        assert ds.GetRasterBand(1).Checksum() > 0
        if gdal.GetDriverByName("GTiff"):
            ds_ref = gdal.OpenEx(
                f"data/gtiff/rgbsmall_{filename_middle}.tif",
                allowed_drivers=["GTiff"],
            )
            assert [ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == [
                ds_ref.GetRasterBand(i + 1).Checksum() for i in range(3)
            ]
    else:
        assert [ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == [
            21212,
            21053,
            21349,
        ]
    assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF") is not None
    assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_SIZE_0_0", "TIFF") is not None


@pytest.mark.parametrize(
    "filename_prefix",
    [
        "byte_LZW_predictor_2",
        "float32_LZW_predictor_2",
        "float32_LZW_predictor_3",
        "float32_lzw_predictor_3_big_endian",
        "float64_LZW_predictor_2",
        "float64_LZW_predictor_3",
        "uint16_LZW_predictor_2",
        "uint32_LZW_predictor_2",
        "uint64_LZW_predictor_2",
    ],
)
def test_libertiff_check_predictor_1_band(filename_prefix):
    ds = libertiff_open(f"data/gtiff/{filename_prefix}.tif")
    assert ds.GetRasterBand(1).Checksum() == 4672


@pytest.mark.parametrize(
    "filename_prefix",
    [
        "stefan_full_greyalpha_byte_LZW_predictor_2",
        "stefan_full_greyalpha_uint16_LZW_predictor_2",
        "stefan_full_greyalpha_uint32_LZW_predictor_2",
        "stefan_full_greyalpha_uint64_LZW_predictor_2",
    ],
)
def test_libertiff_check_predictor_2_bands(filename_prefix):
    ds = libertiff_open(f"data/gtiff/{filename_prefix}.tif")
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(2)] == [1970, 10807]


@pytest.mark.parametrize(
    "filename_prefix",
    [
        "rgbsmall_byte_LZW_predictor_2",
        "rgbsmall_uint16_LZW_predictor_2",
        "rgbsmall_uint32_LZW_predictor_2",
        "rgbsmall_uint64_LZW_predictor_2",
    ],
)
def test_libertiff_check_predictor_3_bands(filename_prefix):
    ds = libertiff_open(f"data/gtiff/{filename_prefix}.tif")
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == [
        21212,
        21053,
        21349,
    ]


def test_libertiff_read_predictor_4_bands():
    ds = libertiff_open("data/gtiff/stefan_full_rgba_LZW_predictor_2.tif")
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == [
        12603,
        58561,
        36064,
        10807,
    ]


def test_libertiff_read_predictor_5_bands():
    ds = libertiff_open("data/gtiff/byte_5_bands_LZW_predictor_2.tif")
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(5)] == [
        4672,
        4563,
        4672,
        4563,
        4672,
    ]


###############################################################################
# Test reading a GeoTIFF file with tiepoints in PixelIsPoint format.


def test_libertiff_read_tiepoints_pixelispoint():

    ds = libertiff_open("data/byte_gcp_pixelispoint.tif")
    assert ds.GetMetadataItem("AREA_OR_POINT") == "Point"
    assert ds.GetSpatialRef() is None
    assert ds.GetGCPSpatialRef() is not None
    assert ds.GetGCPCount() == 4
    gcp = ds.GetGCPs()[0]
    assert (
        gcp.GCPPixel == pytest.approx(0.5, abs=1e-5)
        and gcp.GCPLine == pytest.approx(0.5, abs=1e-5)
        and gcp.GCPX == pytest.approx(-180, abs=1e-5)
        and gcp.GCPY == pytest.approx(90, abs=1e-5)
        and gcp.GCPZ == pytest.approx(0, abs=1e-5)
    )


###############################################################################
# Test reading a 4-band contig jpeg compressed geotiff.


@pytest.mark.require_driver("JPEG")
def test_libertiff_rgba_jpeg_contig():

    ds = libertiff_open("data/stefan_full_rgba_jpeg_contig.tif")
    assert ds.GetRasterBand(1).Checksum() > 0
    assert ds.GetRasterBand(2).Checksum() > 0
    assert ds.GetRasterBand(3).Checksum() > 0
    assert ds.GetRasterBand(4).Checksum() > 0


###############################################################################
# Test reading a 12bit jpeg compressed geotiff.


@pytest.mark.skipif(
    "SKIP_TIFF_JPEG12" in os.environ, reason="Crashes on build-windows-msys2-mingw"
)
@pytest.mark.require_driver("JPEG")
def test_libertiff_12bitjpeg(tmp_vsimem):

    jpeg_drv = gdal.GetDriverByName("JPEG")
    if "UInt16" not in jpeg_drv.GetMetadataItem(gdal.DMD_CREATIONDATATYPES):
        pytest.skip("12-bit JPEG not supported in this build")

    filename = str(tmp_vsimem / "tmp.tif")
    gdal.FileFromMemBuffer(
        filename, open("data/mandrilmini_12bitjpeg.tif", "rb").read()
    )
    ds = libertiff_open(filename)

    stats = ds.GetRasterBand(1).ComputeStatistics(False)
    assert not (
        stats[2] < 2150 or stats[2] > 2180 or str(stats[2]) == "nan"
    ), "did not get expected mean for band1."


###############################################################################


@pytest.mark.parametrize(
    "filename", ["data/excessive-memory-TIFFFillStrip.tif", "data/huge4GB.tif"]
)
def test_libertiff_read_error(filename):

    ds = libertiff_open(filename)
    for i in range(ds.RasterCount):
        with pytest.raises(Exception):
            ds.GetRasterBand(i + 1).Checksum()


###############################################################################


@pytest.mark.parametrize(
    "filename",
    [
        "data/huge-number-strips.tiff",
    ],
)
def test_libertiff_open_error(filename):

    with pytest.raises(Exception):
        libertiff_open(filename)


###############################################################################
# Test we don't crash on files


@gdaltest.disable_exceptions()
def test_libertiff_test_all_tif():

    for filename in glob.glob("data/*.tif"):
        # print(filename)
        with gdal.quiet_errors():
            ds = libertiff_open(filename)
            if ds and ds.RasterCount:
                ds.ReadRaster(0, 0, 1, 1)


def test_libertiff_thread_safe_readraster():

    ds = libertiff_open("data/gtiff/rgbsmall_DEFLATE_tiled_separate.tif")
    assert ds.IsThreadSafe(gdal.OF_RASTER)

    res = [True]

    def check():
        for i in range(100):
            if [ds.GetRasterBand(i + 1).Checksum() for i in range(3)] != [
                21212,
                21053,
                21349,
            ]:
                res[0] = False

    threads = [threading.Thread(target=check) for i in range(2)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert res[0]


###############################################################################
# Test different datatypes for StripOffsets tag with little/big, classic/bigtiff


@pytest.mark.parametrize(
    "filename,expected_offsets",
    [
        ("data/classictiff_one_block_byte.tif", [158]),
        ("data/classictiff_one_block_long.tif", [158]),
        ("data/classictiff_one_block_be_long.tif", [158]),
        ("data/classictiff_one_strip_long.tif", [146]),
        ("data/classictiff_one_strip_be_long.tif", [146]),
        ("data/classictiff_two_strip_short.tif", [162, 163]),
        ("data/classictiff_two_strip_be_short.tif", [162, 163]),
        ("data/classictiff_four_strip_short.tif", [178, 179, 180, 181]),
        ("data/classictiff_four_strip_be_short.tif", [178, 179, 180, 181]),
        ("data/bigtiff_four_strip_short.tif", [316, 317, 318, 319]),
        ("data/bigtiff_four_strip_be_short.tif", [316, 317, 318, 319]),
        ("data/bigtiff_one_block_long8.tif", [272]),
        ("data/bigtiff_one_block_be_long8.tif", [272]),
        ("data/bigtiff_one_strip_long.tif", [252]),
        ("data/bigtiff_one_strip_be_long.tif", [252]),
        ("data/bigtiff_one_strip_long8.tif", [252]),
        ("data/bigtiff_one_strip_be_long8.tif", [252]),
        ("data/bigtiff_two_strip_long.tif", [284, 285]),
        ("data/bigtiff_two_strip_be_long.tif", [284, 285]),
        ("data/bigtiff_two_strip_long8.tif", [284, 285]),
        ("data/bigtiff_two_strip_be_long8.tif", [284, 285]),
    ],
)
def test_libertiff_read_stripoffset_types(filename, expected_offsets):

    ds = libertiff_open(filename)
    offsets = []
    for row in range(4):
        mdi = ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_0_%d" % row, "TIFF")
        if mdi is None:
            break
        offsets.append(int(mdi))
    assert offsets == expected_offsets
    assert ds.GetRasterBand(1).Checksum() > 0


def test_libertiff_packbits():
    ds = libertiff_open("data/seperate_strip.tif")
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == [
        15234,
        15234,
        15234,
    ]


def test_libertiff_coordinate_epoch():
    ds = libertiff_open("data/gtiff/byte_coord_epoch.tif")
    assert ds.GetSpatialRef().GetCoordinateEpoch() == 2020.0


@pytest.mark.require_driver("WEBP")
def test_libertiff_webp_lossy():
    ds = libertiff_open("data/tif_webp.tif")
    assert ds.GetMetadata("IMAGE_STRUCTURE") == {
        "COMPRESSION_REVERSIBILITY": "LOSSY",
        "COMPRESSION": "WEBP",
        "INTERLEAVE": "PIXEL",
    }


def test_libertiff_multi_image():
    ds = libertiff_open("data/twoimages.tif")
    assert ds.RasterCount == 0
    assert ds.GetSubDatasets() == [
        ("GTIFF_DIR:1:data/twoimages.tif", "Page 1 (20P x 20L x 1B)"),
        ("GTIFF_DIR:2:data/twoimages.tif", "Page 2 (20P x 20L x 1B)"),
    ]

    with pytest.raises(Exception):
        libertiff_open("GTIFF_DIR:")
    with pytest.raises(Exception):
        libertiff_open("GTIFF_DIR:0")
    with pytest.raises(Exception):
        libertiff_open("GTIFF_DIR:-1:data/twoimages.tif")
    with pytest.raises(Exception):
        libertiff_open("GTIFF_DIR:0:data/twoimages.tif")
    with pytest.raises(Exception):
        libertiff_open("GTIFF_DIR:3:data/twoimages.tif")
    with pytest.raises(Exception):
        libertiff_open("GTIFF_DIR:1:i_do_not_exist.tif")

    ds = libertiff_open("GTIFF_DIR:1:data/twoimages.tif")
    assert ds.GetRasterBand(1).GetMetadataItem("IFD_OFFSET", "TIFF") == "408"

    ds = libertiff_open("GTIFF_DIR:2:data/twoimages.tif")
    assert ds.GetRasterBand(1).GetMetadataItem("IFD_OFFSET", "TIFF") == "958"


def test_libertiff_miniswhite():
    ds = libertiff_open("data/gtiff/miniswhite.tif")
    assert ds.GetMetadataItem("MINISWHITE", "IMAGE_STRUCTURE") == "YES"
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct
    assert ct.GetCount() == 2
    assert ct.GetColorEntry(0) == (255, 255, 255, 255)
    assert ct.GetColorEntry(1) == (0, 0, 0, 255)


def test_libertiff_colortable():
    ds = libertiff_open("data/test_average_palette.tif")
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    ct = ds.GetRasterBand(1).GetColorTable()
    assert ct
    assert ct.GetCount() == 256
    assert ct.GetColorEntry(0) == (0, 0, 0, 255)
    assert ct.GetColorEntry(1) == (255, 255, 255, 255)
    assert ct.GetColorEntry(2) == (127, 127, 127, 255)


###############################################################################
# Test reading a TIFF with the RPC tag per
#  http://geotiff.maptools.org/rpc_prop.html


def test_libertiff_read_rpc_tif():

    ds = libertiff_open("data/byte_rpc.tif")
    rpc_md = ds.GetMetadata("RPC")

    assert rpc_md["HEIGHT_OFF"] == "300", "HEIGHT_OFF wrong:" + rpc_md["HEIGHT_OFF"]

    assert (
        rpc_md["LINE_DEN_COEFF"].find("1 -0.00520769693945429") == 0
    ), "LINE_DEN_COEFF wrong"


###############################################################################
# Corrupt all bytes of an existing file


def test_libertiff_corrupted(tmp_vsimem):

    filename = str(tmp_vsimem / "out.tif")
    data = open("data/gtiff/miniswhite.tif", "rb").read()
    gdal.FileFromMemBuffer(filename, data)
    f = gdal.VSIFOpenL(filename, "rb+")
    try:
        for i in range(len(data)):
            gdal.VSIFSeekL(f, i, 0)
            ori_val = gdal.VSIFReadL(1, 1, f)

            for new_val in (b"\x00", b"\x01", b"\x7F", b"\xFE", b"\xFF"):
                gdal.VSIFSeekL(f, i, 0)
                gdal.VSIFWriteL(new_val, 1, 1, f)
                try:
                    ds = libertiff_open(filename)
                    ds.ReadRaster()
                except Exception:
                    pass

            # Restore old value
            gdal.VSIFSeekL(f, i, 0)
            gdal.VSIFWriteL(ori_val, 1, 1, f)
    finally:
        gdal.VSIFCloseL(f)


def test_libertiff_srs_read_epsg4326_3855_geotiff1_1():
    ds = libertiff_open("data/epsg4326_3855_geotiff1_1.tif")
    sr = ds.GetSpatialRef()
    assert sr.GetName() == "WGS 84 + EGM2008 height"
    assert sr.GetAuthorityCode("COMPD_CS|GEOGCS") == "4326"
    assert sr.GetAuthorityCode("COMPD_CS|VERT_CS") == "3855"


def test_libertiff_srs_read_epsg4979_geotiff1_1():
    ds = libertiff_open("data/epsg4979_geotiff1_1.tif")
    sr = ds.GetSpatialRef()
    assert sr.GetAuthorityCode(None) == "4979"
