#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test AVIF driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import base64
import os
import shutil

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("AVIF")


def has_avif_encoder():
    drv = gdal.GetDriverByName("AVIF")
    return drv is not None and drv.GetMetadataItem("DMD_CREATIONOPTIONLIST") is not None


def _has_geoheif_support():
    drv = gdal.GetDriverByName("AVIF")
    return drv and drv.GetMetadataItem("SUPPORTS_GEOHEIF", "AVIF")


def test_avif_subdatasets(tmp_path):

    filename = str(tmp_path / "out.avif")
    shutil.copy("data/avif/colors-animated-8bpc-alpha-exif-xmp.avif", filename)

    ds = gdal.Open(filename)
    assert ds
    assert len(ds.GetSubDatasets()) == 5
    subds1_name = ds.GetSubDatasets()[0][0]
    subds2_name = ds.GetSubDatasets()[1][0]

    ds = gdal.Open(subds1_name)
    assert ds
    assert ds.RasterXSize == 150
    assert ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is None
    assert ds.GetRasterBand(1).ComputeStatistics(False)
    assert ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is not None
    ds.Close()

    ds = gdal.Open(subds1_name)
    assert ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is not None

    ds = gdal.Open(subds2_name)
    assert ds
    assert ds.RasterXSize == 150
    assert ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is None

    with pytest.raises(Exception):
        gdal.Open(f"AVIF:0:{filename}")
    with pytest.raises(Exception):
        gdal.Open(f"AVIF:6:{filename}")
    with pytest.raises(Exception):
        gdal.Open("AVIF:1:non_existing.heic")
    with pytest.raises(Exception):
        gdal.Open("AVIF:")
    with pytest.raises(Exception):
        gdal.Open("AVIF:1")
    with pytest.raises(Exception):
        gdal.Open("AVIF:1:")


@pytest.mark.skipif(not has_avif_encoder(), reason="libavif encoder missing")
def test_avif_single_band():
    tst = gdaltest.GDALTest(
        "AVIF",
        "byte.tif",
        1,
        4672,
    )
    tst.testCreateCopy(vsimem=1, check_checksum_not_null=True, check_minmax=False)


@pytest.mark.skipif(not has_avif_encoder(), reason="libavif encoder missing")
@pytest.mark.require_driver("PNG")
def test_avif_gray_alpha():
    tst = gdaltest.GDALTest(
        "AVIF",
        "wms/gray+alpha.png",
        1,
        39910,
    )
    tst.testCreateCopy(vsimem=1, check_checksum_not_null=True, check_minmax=False)


@pytest.mark.skipif(not has_avif_encoder(), reason="libavif encoder missing")
def test_avif_rgb():
    tst = gdaltest.GDALTest(
        "AVIF",
        "rgbsmall.tif",
        1,
        21212,
        options=["QUALITY=100", "NUM_THREADS=1"],
    )
    tst.testCreateCopy(vsimem=1)


@pytest.mark.skipif(not has_avif_encoder(), reason="libavif encoder missing")
def test_avif_rgba():
    tst = gdaltest.GDALTest(
        "AVIF",
        "../../gcore/data/stefan_full_rgba.tif",
        1,
        12603,
        options=["QUALITY=100"],
    )
    tst.testCreateCopy(vsimem=1)


@pytest.mark.skipif(not has_avif_encoder(), reason="libavif encoder missing")
def test_avif_uint16():
    tst = gdaltest.GDALTest(
        "AVIF", "../../gcore/data/uint16.tif", 1, 4672, options=["NBITS=10"]
    )
    tst.testCreateCopy(vsimem=1, check_checksum_not_null=True, check_minmax=False)


@pytest.mark.skipif(not has_avif_encoder(), reason="libavif encoder missing")
@pytest.mark.parametrize("yuv_subsampling", ["444", "422", "420"])
def test_avif_yuv_subsampling(tmp_vsimem, yuv_subsampling):

    src_ds = gdal.Open("data/rgbsmall.tif")
    out_filename = str(tmp_vsimem / "out.avif")
    gdal.GetDriverByName("AVIF").CreateCopy(
        out_filename, src_ds, options=["YUV_SUBSAMPLING=" + yuv_subsampling]
    )
    ds = gdal.Open(out_filename)
    assert ds.GetMetadataItem("YUV_SUBSAMPLING", "IMAGE_STRUCTURE") == yuv_subsampling


@pytest.mark.skipif(not has_avif_encoder(), reason="libavif encoder missing")
def test_avif_nbits_from_src_ds(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_UInt16)
    src_ds.GetRasterBand(1).SetMetadataItem("NBITS", "12", "IMAGE_STRUCTURE")
    out_filename = str(tmp_vsimem / "out.avif")
    gdal.GetDriverByName("AVIF").CreateCopy(out_filename, src_ds)
    ds = gdal.Open(out_filename)
    assert ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "12"


@pytest.mark.skipif(not has_avif_encoder(), reason="libavif encoder missing")
def test_avif_exif_xmp(tmp_vsimem):

    src_ds = gdal.Open("data/avif/colors-animated-8bpc-alpha-exif-xmp.avif")
    out_filename = str(tmp_vsimem / "out.avif")
    gdal.GetDriverByName("AVIF").CreateCopy(out_filename, src_ds)
    if gdal.VSIStatL(out_filename + ".aux.xml"):
        gdal.Unlink(out_filename + ".aux.xml")
    ds = gdal.Open(out_filename)
    exif_mdd = ds.GetMetadata("EXIF")
    assert exif_mdd
    assert exif_mdd["EXIF_LensMake"] == "Google"
    xmp = ds.GetMetadata("xml:XMP")
    assert xmp
    assert xmp[0].startswith("<?xpacket")


@pytest.mark.skipif(not has_avif_encoder(), reason="libavif encoder missing")
def test_avif_icc_profile(tmp_vsimem):

    if "SOURCE_ICC_PROFILE" not in gdal.GetDriverByName("AVIF").GetMetadataItem(
        "DMD_CREATIONOPTIONLIST"
    ):
        pytest.skip("ICC profile setting requires libavif >= 1.0")

    f = open("data/sRGB.icc", "rb")
    data = f.read()
    icc = base64.b64encode(data).decode("ascii")
    f.close()

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3)
    src_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    src_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_BlueBand)
    src_ds.SetMetadataItem("SOURCE_ICC_PROFILE", icc, "COLOR_PROFILE")

    out_filename = str(tmp_vsimem / "out.avif")
    gdal.GetDriverByName("AVIF").CreateCopy(out_filename, src_ds)
    ds = gdal.Open(out_filename)
    assert ds.GetMetadataItem("SOURCE_ICC_PROFILE", "COLOR_PROFILE") == icc


@pytest.mark.skipif(not has_avif_encoder(), reason="libavif encoder missing")
def test_avif_creation_errors(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.avif")

    src_ds = gdal.GetDriverByName("MEM").Create("", 65537, 1)
    with pytest.raises(
        Exception,
        match="Too big source dataset. Maximum AVIF image dimension is 65,536 x 65,536 pixels",
    ):
        gdal.GetDriverByName("AVIF").CreateCopy(out_filename, src_ds)

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 65537)
    with pytest.raises(
        Exception,
        match="Too big source dataset. Maximum AVIF image dimension is 65,536 x 65,536 pixels",
    ):
        gdal.GetDriverByName("AVIF").CreateCopy(out_filename, src_ds)

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 5)
    with pytest.raises(Exception, match="Unsupported number of bands"):
        gdal.GetDriverByName("AVIF").CreateCopy(out_filename, src_ds)

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float32)
    with pytest.raises(
        Exception,
        match="Unsupported data type: only Byte or UInt16 bands are supported",
    ):
        gdal.GetDriverByName("AVIF").CreateCopy(out_filename, src_ds)

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with pytest.raises(
        Exception, match="Invalid/inconsistent bit depth w.r.t data type"
    ):
        gdal.GetDriverByName("AVIF").CreateCopy(
            out_filename, src_ds, options=["NBITS=10"]
        )

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_UInt16)
    with pytest.raises(
        Exception, match="Invalid/inconsistent bit depth w.r.t data type"
    ):
        gdal.GetDriverByName("AVIF").CreateCopy(
            out_filename, src_ds, options=["NBITS=8"]
        )

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.GetRasterBand(1).SetColorTable(gdal.ColorTable())
    with pytest.raises(
        Exception,
        match="Source dataset with color table unsupported. Use gdal_translate -expand rgb|rgba first",
    ):
        gdal.GetDriverByName("AVIF").CreateCopy(out_filename, src_ds)

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3)
    with pytest.raises(
        Exception, match="Only YUV_SUBSAMPLING=444 is supported for lossless encoding"
    ):
        gdal.GetDriverByName("AVIF").CreateCopy(
            out_filename, src_ds, options=["QUALITY=100", "YUV_SUBSAMPLING=422"]
        )

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with pytest.raises(Exception, match="Cannot create file /i_do/not/exist.avif"):
        gdal.GetDriverByName("AVIF").CreateCopy("/i_do/not/exist.avif", src_ds)


@pytest.mark.skipif(
    not _has_geoheif_support(),
    reason="this libavif does not support opaque properties like geoheif",
)
def test_avif_geoheif_wkt2():
    ds = gdal.Open("data/heif/geo_small.avif")
    assert ds
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 128
    assert ds.RasterYSize == 76
    assert ds.GetMetadataItem("NAME", "DESCRIPTION_en-AU") == "Copyright Statement"
    assert (
        ds.GetMetadataItem("DESCRIPTION", "DESCRIPTION_en-AU")
        == 'CCBY "Jacobs Group (Australia) Pty Ltd and Australian Capital Territory"'
    )
    assert ds.GetMetadataItem("TAGS", "DESCRIPTION_en-AU") == "copyright"
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


@pytest.mark.skipif(
    not _has_geoheif_support(),
    reason="this libavif does not support opaque properties like geoheif",
)
def test_avif_geoheif_uri():
    ds = gdal.Open("data/heif/geo_crsu.avif")
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


@pytest.mark.skipif(
    not _has_geoheif_support(),
    reason="this libavif does not support opaque properties like geoheif",
)
def test_avif_geoheif_curie():
    ds = gdal.Open("data/heif/geo_curi.avif")
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


###############################################################################


@pytest.mark.skipif(not has_avif_encoder(), reason="libavif encoder missing")
def test_avif_close(tmp_path):

    ds = gdal.GetDriverByName("AVIF").CreateCopy(
        tmp_path / "out.avif", gdal.Open("data/rgbsmall.tif")
    )
    ds.Close()
    os.remove(tmp_path / "out.avif")
