#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test HEIF driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import os
import shutil

import gdaltest
import pytest
import webserver

from osgeo import gdal

pytestmark = pytest.mark.require_driver("HEIF")


def get_version():
    return [
        int(x)
        for x in gdal.GetDriverByName("HEIF")
        .GetMetadataItem("LIBHEIF_VERSION")
        .split(".")
    ]


def _has_tiling_support():
    drv = gdal.GetDriverByName("HEIF")
    return drv and drv.GetMetadataItem("SUPPORTS_TILES", "HEIF")


def _has_avif_decoding_support():
    drv = gdal.GetDriverByName("HEIF")
    return drv and drv.GetMetadataItem("SUPPORTS_AVIF", "HEIF")


def _has_hevc_decoding_support():
    drv = gdal.GetDriverByName("HEIF")
    return drv and drv.GetMetadataItem("SUPPORTS_HEVC", "HEIF")


def _has_uncompressed_decoding_support():
    drv = gdal.GetDriverByName("HEIF")
    return drv and drv.GetMetadataItem("SUPPORTS_UNCOMPRESSED", "HEIF")


def _has_read_write_support_for(format):
    drv = gdal.GetDriverByName("HEIF")
    return (
        drv
        and drv.GetMetadataItem("SUPPORTS_" + format, "HEIF")
        and drv.GetMetadataItem("SUPPORTS_" + format + "_WRITE", "HEIF")
    )


def _has_geoheif_support():
    drv = gdal.GetDriverByName("HEIF")
    return drv and drv.GetMetadataItem("SUPPORTS_GEOHEIF", "HEIF")


@pytest.mark.parametrize("endianness", ["big_endian", "little_endian"])
def test_heif_exif_endian(endianness):
    if not _has_hevc_decoding_support():
        pytest.skip("no HEVC decoding support")

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
    if not _has_hevc_decoding_support():
        pytest.skip("no HEVC decoding support")

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
    if not _has_hevc_decoding_support():
        pytest.skip("no HEVC decoding support")

    ds = gdal.Open("data/heif/small_world_16.heic")
    assert ds
    assert ds.RasterXSize == 400
    assert ds.RasterYSize == 200
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "10"
    assert ds.GetRasterBand(1).ComputeRasterMinMax() == pytest.approx((0, 1023), abs=2)


def test_heif_rgba():
    if not _has_hevc_decoding_support():
        pytest.skip("no HEVC decoding support")

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
    if not _has_hevc_decoding_support():
        pytest.skip("no HEVC decoding support")

    ds = gdal.Open("data/heif/stefan_full_rgba_16.heic")
    assert ds
    assert ds.RasterCount == 4
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16


def test_heif_tiled():
    if not _has_tiling_support():
        pytest.skip("no tiling support")
    if not _has_uncompressed_decoding_support():
        pytest.skip("no HEVC decoding support")

    ds = gdal.Open("data/heif/uncompressed_comp_RGB_tiled.heif")
    assert ds
    assert ds.RasterXSize == 30
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
    assert ds.GetRasterBand(1).GetBlockSize() == [15, 5]
    assert ds.GetRasterBand(2).GetBlockSize() == [15, 5]
    assert ds.GetRasterBand(3).GetBlockSize() == [15, 5]
    gdaltest.importorskip_gdal_array()
    assert (
        ds.GetRasterBand(1).ReadAsArray(0, 0, 30, 1)
        == [
            [
                255,
                255,
                255,
                255,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                255,
                255,
                255,
                255,
                0,
                0,
                0,
                0,
                255,
                255,
                255,
                255,
                0,
                0,
                0,
                0,
                128,
                128,
            ]
        ]
    ).all()
    assert (
        ds.GetRasterBand(1).ReadAsArray(0, 19, 30, 1)
        == [
            [
                0,
                0,
                0,
                0,
                255,
                255,
                255,
                255,
                0,
                0,
                0,
                0,
                128,
                128,
                128,
                128,
                255,
                255,
                255,
                255,
                238,
                238,
                238,
                238,
                255,
                255,
                255,
                255,
                0,
                0,
            ]
        ]
    ).all()
    assert (
        ds.GetRasterBand(2).ReadAsArray(0, 0, 30, 1)
        == [
            [
                0,
                0,
                0,
                0,
                128,
                128,
                128,
                128,
                0,
                0,
                0,
                0,
                255,
                255,
                255,
                255,
                0,
                0,
                0,
                0,
                255,
                255,
                255,
                255,
                255,
                255,
                255,
                255,
                128,
                128,
            ]
        ]
    ).all()
    assert (
        ds.GetRasterBand(2).ReadAsArray(0, 19, 30, 1)
        == [
            [
                0,
                0,
                0,
                0,
                255,
                255,
                255,
                255,
                255,
                255,
                255,
                255,
                128,
                128,
                128,
                128,
                165,
                165,
                165,
                165,
                130,
                130,
                130,
                130,
                0,
                0,
                0,
                0,
                128,
                128,
            ]
        ]
    ).all()
    assert (
        ds.GetRasterBand(3).ReadAsArray(0, 0, 30, 1)
        == [
            [
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                255,
                255,
                255,
                255,
                255,
                255,
                255,
                255,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                255,
                255,
                255,
                255,
                128,
                128,
            ]
        ]
    ).all()
    assert (
        ds.GetRasterBand(3).ReadAsArray(0, 19, 30, 1)
        == [
            [
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                255,
                255,
                255,
                255,
                128,
                128,
                128,
                128,
                0,
                0,
                0,
                0,
                238,
                238,
                238,
                238,
                0,
                0,
                0,
                0,
                0,
                0,
            ]
        ]
    ).all()


def test_heif_subdatasets(tmp_path):
    if not _has_hevc_decoding_support():
        pytest.skip("no HEVC decoding support")

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


@pytest.mark.require_curl()
def test_heif_network_read(tmp_vsimem):

    if not _has_read_write_support_for("UNCOMPRESSED"):
        pytest.skip("does not support UNCOMPRESSED")
    if get_version() < [1, 19, 8]:
        pytest.skip("libheif >= 1.19.8 not met")

    filename = tmp_vsimem / "test.hic"
    gdal.Translate(
        filename,
        "data/small_world.tif",
        options="-outsize 1024 0 -of HEIF -co CODEC=UNCOMPRESSED",
    )

    src_ds = gdal.Open(filename)

    gdal.VSICurlClearCache()

    webserver_process = None
    webserver_port = 0

    webserver_process, webserver_port = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    if webserver_port == 0:
        pytest.skip()

    def extract(offset, size):
        with gdal.VSIFile(filename, "rb") as f:
            f.seek(offset)
            return f.read(size)

    try:
        filesize = gdal.VSIStatL(filename).size

        handler = webserver.SequentialHandler()
        handler.add("HEAD", "/test.hic", 200, {"Content-Length": "%d" % filesize})
        handler.add(
            "GET",
            "/test.hic",
            206,
            {"Content-Length": "16384"},
            extract(0, 16384),
            expected_headers={"Range": "bytes=0-16383"},
        )
        handler.add("GET", "/", 404)
        with gdaltest.config_option(
            "GDAL_PAM_ENABLED", "NO"
        ), webserver.install_http_handler(handler):
            ds = gdal.Open("/vsicurl/http://localhost:%d/test.hic" % webserver_port)

        handler = webserver.SequentialHandler()

        def method(request):
            # sys.stderr.write('%s\n' % str(request.headers))

            if request.headers["Range"].startswith("bytes="):
                rng = request.headers["Range"][len("bytes=") :]
                assert len(rng.split("-")) == 2
                start = int(rng.split("-")[0])
                end = int(rng.split("-")[1])

                request.protocol_version = "HTTP/1.1"
                request.send_response(206)
                request.send_header("Content-type", "application/octet-stream")
                request.send_header(
                    "Content-Range", "bytes %d-%d/%d" % (start, end, filesize)
                )
                request.send_header("Content-Length", end - start + 1)
                request.send_header("Connection", "close")
                request.end_headers()

                request.wfile.write(extract(start, end - start + 1))

        handler.add("GET", "/test.hic", custom_method=method)
        handler.add("GET", "/test.hic", custom_method=method)
        handler.add("GET", "/test.hic", custom_method=method)
        with webserver.install_http_handler(handler):
            ret = ds.ReadRaster()
        assert ret == src_ds.ReadRaster()

    finally:
        webserver.server_stop(webserver_process, webserver_port)

        gdal.VSICurlClearCache()


def make_data():
    ds = gdal.GetDriverByName("MEM").Create("", 300, 200, 3, gdal.GDT_UInt8)

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
            0, line, 300, 1, rgb_bytes, buf_type=gdal.GDT_UInt8, band_list=[1, 2, 3]
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
            buf_type=gdal.GDT_UInt8,
            band_list=[1, 2, 3],
        )

    assert ds.FlushCache() == gdal.CE_None
    return ds


def make_data_with_alpha():
    ds = gdal.GetDriverByName("MEM").Create("", 300, 200, 4, gdal.GDT_UInt8)

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
            0, line, 300, 1, rgba_bytes, buf_type=gdal.GDT_UInt8, band_list=[1, 2, 3, 4]
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
            buf_type=gdal.GDT_UInt8,
            band_list=[1, 2, 3, 4],
        )

    assert ds.FlushCache() == gdal.CE_None
    return ds


def make_data_gray():
    ds = gdal.GetDriverByName("MEM").Create("", 300, 200, 1, gdal.GDT_UInt8)

    ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_GrayIndex)

    gray = [0xFF] * 150 + [0x00] * 150
    gray_bytes = array.array("B", gray).tobytes()
    for line in range(200):
        ds.WriteRaster(
            0, line, 300, 1, gray_bytes, buf_type=gdal.GDT_UInt8, band_list=[1]
        )

    assert ds.FlushCache() == gdal.CE_None
    return ds


heif_codecs = ["AV1", "HEVC", "JPEG", "JPEG2000", "UNCOMPRESSED"]


@pytest.mark.parametrize("codec", heif_codecs)
def test_heif_create_copy(tmp_path, codec):
    if not _has_read_write_support_for(codec):
        pytest.skip(f"no support for codec {codec}")
    tempfile = str(tmp_path / ("test_heif_create_copy_" + codec + ".hif"))
    input_ds = make_data()

    drv = gdal.GetDriverByName("HEIF")
    result_ds = drv.CreateCopy(tempfile, input_ds, options=["CODEC=" + codec])

    result_ds = None

    result_ds = gdal.Open(tempfile)

    assert result_ds


@pytest.mark.parametrize("codec", heif_codecs)
def test_heif_create_copy_with_alpha(tmp_path, codec):
    if not _has_read_write_support_for(codec):
        pytest.skip(f"no support for codec {codec}")
    tempfile = str(tmp_path / ("test_heif_create_copy_" + codec + "_alpha.hif"))
    input_ds = make_data_with_alpha()

    drv = gdal.GetDriverByName("HEIF")
    result_ds = drv.CreateCopy(tempfile, input_ds, options=["CODEC=" + codec])

    result_ds = None

    result_ds = gdal.Open(tempfile)

    assert result_ds


@pytest.mark.parametrize("codec", heif_codecs)
def test_heif_create_copy_gray(tmp_path, codec):
    if not _has_read_write_support_for(codec):
        pytest.skip(f"no support for codec {codec}")
    tempfile = str(tmp_path / ("test_heif_create_copy_gray_" + codec + ".hif"))
    input_ds = make_data_gray()

    drv = gdal.GetDriverByName("HEIF")
    result_ds = drv.CreateCopy(tempfile, input_ds, options=["CODEC=" + codec])

    result_ds = None

    result_ds = gdal.Open(tempfile)

    assert result_ds
    pass


def test_heif_create_copy_defaults(tmp_path):
    if not _has_read_write_support_for("HEVC"):
        pytest.skip("no HEVC encoding support")
    tempfile = str(tmp_path / "test_heif_create_copy.hif")
    input_ds = make_data()

    drv = gdal.GetDriverByName("HEIF")

    result_ds = drv.CreateCopy(tempfile, input_ds, options=[])

    result_ds = None

    result_ds = gdal.Open(tempfile)

    assert result_ds


@pytest.mark.skipif(
    not _has_geoheif_support(),
    reason="this libheif does not support opaque properties like geoheif",
)
def test_heif_geoheif_wkt2():
    ds = gdal.Open("data/heif/geo_wkt2.heif")
    assert ds
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 256
    assert ds.RasterYSize == 64
    assert ds.GetMetadataItem("NAME", "DESCRIPTION_en-AU") == "Copyright Statement"
    assert (
        ds.GetMetadataItem("DESCRIPTION", "DESCRIPTION_en-AU")
        == 'CCBY "Jacobs Group (Australia) Pty Ltd and Australian Capital Territory"'
    )
    assert ds.GetMetadataItem("TAGS", "DESCRIPTION_en-AU") == "copyright"
    assert ds.GetGeoTransform() is not None
    assert ds.GetGeoTransform() == pytest.approx(
        [691051.2, 0.1, 0.0, 6090000.0, 0.0, -0.1]
    )
    assert ds.GetGCPCount() == 1
    gcp = ds.GetGCPs()[0]
    assert (
        gcp.GCPPixel == pytest.approx(0, abs=1e-5)
        and gcp.GCPLine == pytest.approx(0, abs=1e-5)
        and gcp.GCPX == pytest.approx(691051.2, abs=1e-5)
        and gcp.GCPY == pytest.approx(6090000.0, abs=1e-5)
        and gcp.GCPZ == pytest.approx(0, abs=1e-5)
    )


@pytest.mark.skipif(
    not _has_geoheif_support(),
    reason="this libheif does not support opaque properties like geoheif",
)
def test_heif_geoheif_uri():
    ds = gdal.Open("data/heif/geo_crsu.heif")
    assert ds
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 256
    assert ds.RasterYSize == 64
    assert ds.GetMetadataItem("NAME", "DESCRIPTION_en-AU") == "Copyright Statement"
    assert (
        ds.GetMetadataItem("DESCRIPTION", "DESCRIPTION_en-AU")
        == 'CCBY "Jacobs Group (Australia) Pty Ltd and Australian Capital Territory"'
    )
    assert ds.GetMetadataItem("TAGS", "DESCRIPTION_en-AU") == "copyright"
    assert ds.GetGeoTransform() is not None
    assert ds.GetGeoTransform() == pytest.approx(
        [691051.2, 0.1, 0.0, 6090000.0, 0.0, -0.1]
    )
    assert ds.GetGCPCount() == 1
    gcp = ds.GetGCPs()[0]
    assert (
        gcp.GCPPixel == pytest.approx(0, abs=1e-5)
        and gcp.GCPLine == pytest.approx(0, abs=1e-5)
        and gcp.GCPX == pytest.approx(691051.2, abs=1e-5)
        and gcp.GCPY == pytest.approx(6090000.0, abs=1e-5)
        and gcp.GCPZ == pytest.approx(0, abs=1e-5)
    )


@pytest.mark.skipif(
    not _has_geoheif_support(),
    reason="this libheif does not support opaque properties like geoheif",
)
def test_heif_geoheif_curie():
    ds = gdal.Open("data/heif/geo_curi.heif")
    assert ds
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 256
    assert ds.RasterYSize == 64
    assert ds.GetMetadataItem("NAME", "DESCRIPTION_en-AU") == "Copyright Statement"
    assert (
        ds.GetMetadataItem("DESCRIPTION", "DESCRIPTION_en-AU")
        == 'CCBY "Jacobs Group (Australia) Pty Ltd and Australian Capital Territory"'
    )
    assert ds.GetMetadataItem("TAGS", "DESCRIPTION_en-AU") == "copyright"
    assert ds.GetSpatialRef() is not None
    assert ds.GetSpatialRef().GetAuthorityName(None) == "EPSG"
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "32755"
    assert ds.GetGeoTransform() is not None
    assert ds.GetGeoTransform() == pytest.approx(
        [691051.2, 0.1, 0.0, 6090000.0, 0.0, -0.1]
    )
    assert ds.GetGCPCount() == 1
    gcp = ds.GetGCPs()[0]
    assert (
        gcp.GCPPixel == pytest.approx(0, abs=1e-5)
        and gcp.GCPLine == pytest.approx(0, abs=1e-5)
        and gcp.GCPX == pytest.approx(691051.2, abs=1e-5)
        and gcp.GCPY == pytest.approx(6090000.0, abs=1e-5)
        and gcp.GCPZ == pytest.approx(0, abs=1e-5)
    )


@pytest.mark.skipif(
    not _has_geoheif_support(),
    reason="this libheif does not support opaque properties like geoheif",
)
def test_heif_geoheif_curie_order():
    ds = gdal.Open("data/heif/geo_curi.heif")
    assert ds
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 256
    assert ds.RasterYSize == 64
    assert ds.GetMetadataItem("NAME", "DESCRIPTION_en-AU") == "Copyright Statement"
    assert (
        ds.GetMetadataItem("DESCRIPTION", "DESCRIPTION_en-AU")
        == 'CCBY "Jacobs Group (Australia) Pty Ltd and Australian Capital Territory"'
    )
    assert ds.GetMetadataItem("TAGS", "DESCRIPTION_en-AU") == "copyright"
    assert ds.GetGeoTransform() is not None
    assert ds.GetGeoTransform() == pytest.approx(
        [691051.2, 0.1, 0.0, 6090000.0, 0.0, -0.1]
    )
    assert ds.GetSpatialRef() is not None
    assert ds.GetSpatialRef().GetAuthorityName(None) == "EPSG"
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "32755"
    assert ds.GetGCPCount() == 1
    gcp = ds.GetGCPs()[0]
    assert (
        gcp.GCPPixel == pytest.approx(0, abs=1e-5)
        and gcp.GCPLine == pytest.approx(0, abs=1e-5)
        and gcp.GCPX == pytest.approx(691051.2, abs=1e-5)
        and gcp.GCPY == pytest.approx(6090000.0, abs=1e-5)
        and gcp.GCPZ == pytest.approx(0, abs=1e-5)
    )


###############################################################################


def test_heif_close(tmp_path):
    if not _has_read_write_support_for("HEVC"):
        pytest.skip("no HEVC encoding support")

    ds = gdal.GetDriverByName("HEIF").CreateCopy(tmp_path / "out.heif", make_data())
    ds.Close()
    os.remove(tmp_path / "out.heif")
