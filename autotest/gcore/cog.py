#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  COG driver testing
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import struct
import sys

import gdaltest
import pytest
import webserver
from test_py_scripts import samples_path

from osgeo import gdal, osr


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################


def _check_cog(filename):

    path = samples_path
    if path not in sys.path:
        sys.path.append(path)
    import validate_cloud_optimized_geotiff

    try:
        _, errors, _ = validate_cloud_optimized_geotiff.validate(
            filename, full_check=True
        )
        assert not errors, "validate_cloud_optimized_geotiff failed"
    except OSError:
        pytest.fail("validate_cloud_optimized_geotiff failed")


###############################################################################


def check_libtiff_internal_or_at_least(expected_maj, expected_min, expected_micro):

    md = gdal.GetDriverByName("GTiff").GetMetadata()
    if md["LIBTIFF"] == "INTERNAL":
        return True
    if md["LIBTIFF"].startswith("LIBTIFF, Version "):
        version = md["LIBTIFF"][len("LIBTIFF, Version ") :]
        version = version[0 : version.find("\n")]
        got_maj, got_min, got_micro = version.split(".")
        got_maj = int(got_maj)
        got_min = int(got_min)
        got_micro = int(got_micro)
        if got_maj > expected_maj:
            return True
        if got_maj < expected_maj:
            return False
        if got_min > expected_min:
            return True
        if got_min < expected_min:
            return False
        return got_micro >= expected_micro
    return False


###############################################################################
# Basic test


def test_cog_basic():

    tab = [0]

    def my_cbk(pct, _, arg):
        assert pct >= tab[0]
        tab[0] = pct
        return 1

    filename = "/vsimem/cog.tif"
    src_ds = gdal.Open("data/byte.tif")
    assert src_ds.GetMetadataItem("GDAL_STRUCTURAL_METADATA", "TIFF") is None

    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, callback=my_cbk, callback_data=tab
    )
    assert not ds.GetCloseReportsProgress()
    src_ds = None
    assert tab[0] == 1.0
    assert ds
    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetMetadataItem("LAYOUT", "IMAGE_STRUCTURE") == "COG"
    assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"
    assert ds.GetMetadataItem("OVERVIEW_RESAMPLING", "IMAGE_STRUCTURE") is None
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetRasterBand(1).GetBlockSize() == [512, 512]
    assert (
        ds.GetMetadataItem("GDAL_STRUCTURAL_METADATA", "TIFF")
        == """GDAL_STRUCTURAL_METADATA_SIZE=000140 bytes
LAYOUT=IFDS_BEFORE_DATA
BLOCK_ORDER=ROW_MAJOR
BLOCK_LEADER=SIZE_AS_UINT4
BLOCK_TRAILER=LAST_4_BYTES_REPEATED
KNOWN_INCOMPATIBLE_EDITION=NO
 """
    )
    ds = None
    _check_cog(filename)
    gdal.GetDriverByName("GTiff").Delete(filename)


###############################################################################
# Test creation options


def test_cog_creation_options():

    filename = "/vsimem/cog.tif"
    src_ds = gdal.Open("data/rgbsmall.tif")
    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["COMPRESS=DEFLATE", "LEVEL=1", "NUM_THREADS=2"]
    )
    assert ds
    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == 21212
    assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "DEFLATE"
    assert ds.GetMetadataItem("PREDICTOR", "IMAGE_STRUCTURE") is None
    ds = None
    filesize = gdal.VSIStatL(filename).size
    _check_cog(filename)

    gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["COMPRESS=DEFLATE", "BIGTIFF=YES", "LEVEL=1"]
    )
    assert gdal.VSIStatL(filename).size != filesize

    gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["COMPRESS=DEFLATE", "PREDICTOR=YES", "LEVEL=1"]
    )
    assert gdal.VSIStatL(filename).size != filesize
    ds = gdal.Open(filename)
    assert ds.GetMetadataItem("PREDICTOR", "IMAGE_STRUCTURE") == "2"
    ds = None

    gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["COMPRESS=DEFLATE", "LEVEL=9"]
    )
    assert gdal.VSIStatL(filename).size < filesize

    colist = gdal.GetDriverByName("COG").GetMetadataItem("DMD_CREATIONOPTIONLIST")
    if "<Value>ZSTD" in colist:

        gdal.GetDriverByName("COG").CreateCopy(
            filename, src_ds, options=["COMPRESS=ZSTD"]
        )
        ds = gdal.Open(filename)
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "ZSTD"
        ds = None

    if "<Value>LZMA" in colist:

        gdal.GetDriverByName("COG").CreateCopy(
            filename, src_ds, options=["COMPRESS=LZMA"]
        )
        ds = gdal.Open(filename)
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZMA"
        ds = None

    if "<Value>WEBP" in colist:

        assert gdal.GetDriverByName("COG").CreateCopy(
            filename, src_ds, options=["COMPRESS=WEBP"]
        )
        ds = gdal.Open(filename)
        assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "WEBP"
        ds = None

    if "<Value>LERC" in colist:

        assert gdal.GetDriverByName("COG").CreateCopy(
            filename, src_ds, options=["COMPRESS=LERC"]
        )
        filesize_no_z_error = gdal.VSIStatL(filename).size
        assert gdal.VSIStatL(filename).size != filesize

        assert gdal.GetDriverByName("COG").CreateCopy(
            filename, src_ds, options=["COMPRESS=LERC", "MAX_Z_ERROR=10"]
        )
        filesize_with_z_error = gdal.VSIStatL(filename).size
        assert filesize_with_z_error < filesize_no_z_error

        assert gdal.GetDriverByName("COG").CreateCopy(
            filename, src_ds, options=["COMPRESS=LERC_DEFLATE"]
        )
        filesize_lerc_deflate = gdal.VSIStatL(filename).size
        assert filesize_lerc_deflate < filesize_no_z_error

        assert gdal.GetDriverByName("COG").CreateCopy(
            filename, src_ds, options=["COMPRESS=LERC_DEFLATE", "LEVEL=1"]
        )
        filesize_lerc_deflate_level_1 = gdal.VSIStatL(filename).size
        assert filesize_lerc_deflate_level_1 > filesize_lerc_deflate

        if "<Value>ZSTD" in colist:
            assert gdal.GetDriverByName("COG").CreateCopy(
                filename, src_ds, options=["COMPRESS=LERC_ZSTD"]
            )
            filesize_lerc_zstd = gdal.VSIStatL(filename).size
            assert filesize_lerc_zstd < filesize_no_z_error

            assert gdal.GetDriverByName("COG").CreateCopy(
                filename, src_ds, options=["COMPRESS=LERC_ZSTD", "LEVEL=1"]
            )
            filesize_lerc_zstd_level_1 = gdal.VSIStatL(filename).size
            assert filesize_lerc_zstd_level_1 > filesize_lerc_zstd

    src_ds = None
    with gdal.quiet_errors():
        gdal.GetDriverByName("GTiff").Delete(filename)


###############################################################################
# Test creation of overviews


def test_cog_creation_of_overviews(tmp_vsimem):

    tab = [0]

    def my_cbk(pct, _, arg):
        assert pct >= tab[0]
        tab[0] = pct
        return 1

    filename = tmp_vsimem / "cog.tif"
    src_ds = gdal.Translate("", "data/byte.tif", options="-of MEM -outsize 2048 300")

    check_filename = "/vsimem/tmp.tif"
    ds = gdal.GetDriverByName("GTiff").CreateCopy(
        check_filename, src_ds, options=["TILED=YES"]
    )
    ds.BuildOverviews("AVERAGE", [2, 4])
    cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs2 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    ds = None
    gdal.Unlink(check_filename)

    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        callback=my_cbk,
        callback_data=tab,
        options=["OVERVIEW_RESAMPLING=AVERAGE"],
    )
    assert tab[0] == 1.0
    assert ds
    assert len(gdal.ReadDir(tmp_vsimem)) == 1  # check that the temp file has gone away

    ds = None
    ds = gdal.Open(filename)
    assert ds.GetMetadataItem("OVERVIEW_RESAMPLING", "IMAGE_STRUCTURE") == "AVERAGE"
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == cs1
    assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("RESAMPLING") == "AVERAGE"
    assert ds.GetRasterBand(1).GetOverview(1).Checksum() == cs2
    ds = None
    _check_cog(str(filename))

    gdal.Translate(tmp_vsimem / "cog2.tif", filename, format="COG")
    ds = gdal.Open(tmp_vsimem / "cog2.tif")
    assert ds.GetMetadataItem("OVERVIEW_RESAMPLING", "IMAGE_STRUCTURE") == "AVERAGE"


###############################################################################
# Test creation from a single band + alpha dataset


@pytest.mark.require_creation_option("COG", "JPEG")
def test_cog_single_band_plus_alpha_jpeg_compression(tmp_vsimem):

    filename = str(tmp_vsimem / "cog.tif")
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    src_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)

    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=["COMPRESS=JPEG"],
    )

    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET


###############################################################################
# Test creation of overviews with a different compression method


@pytest.mark.require_creation_option("COG", "JPEG")
def test_cog_creation_of_overviews_with_compression():

    directory = "/vsimem/test_cog_creation_of_overviews_with_compression"
    filename = directory + "/cog.tif"
    src_ds = gdal.Translate("", "data/byte.tif", options="-of MEM -outsize 2048 300")

    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=["COMPRESS=LZW", "OVERVIEW_COMPRESS=JPEG", "OVERVIEW_QUALITY=50"],
    )

    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert ds.GetMetadata("IMAGE_STRUCTURE")["COMPRESSION"] == "LZW"

    with gdal.Open("GTIFF_DIR:2:" + filename) as ds_overview:
        assert ds_overview.GetMetadata("IMAGE_STRUCTURE")["COMPRESSION"] == "JPEG"
        assert ds_overview.GetMetadata("IMAGE_STRUCTURE")["JPEG_QUALITY"] == "50"

    with gdal.Open("GTIFF_DIR:3:" + filename) as ds_overview:
        assert ds_overview.GetMetadata("IMAGE_STRUCTURE")["COMPRESSION"] == "JPEG"
        assert ds_overview.GetMetadata("IMAGE_STRUCTURE")["JPEG_QUALITY"] == "50"

    ds = None

    src_ds = None
    gdal.GetDriverByName("GTiff").Delete(filename)
    gdal.Unlink(directory)


###############################################################################
# Test creation of overviews with a dataset with a mask


def test_cog_creation_of_overviews_with_mask():

    tab = [0]

    def my_cbk(pct, _, arg):
        assert pct >= tab[0]
        tab[0] = pct
        return 1

    directory = "/vsimem/test_cog_creation_of_overviews_with_mask"
    gdal.Mkdir(directory, 0o755)
    filename = directory + "/cog.tif"
    src_ds = gdal.Translate("", "data/byte.tif", options="-of MEM -outsize 2048 300")
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(
        0, 0, 1024, 300, b"\xFF", buf_xsize=1, buf_ysize=1
    )

    check_filename = "/vsimem/tmp.tif"
    ds = gdal.GetDriverByName("GTiff").CreateCopy(
        check_filename, src_ds, options=["TILED=YES"]
    )
    ds.BuildOverviews("CUBIC", [2, 4])
    cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs2 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    ds = None
    gdal.Unlink(check_filename)

    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, callback=my_cbk, callback_data=tab
    )
    assert tab[0] == 1.0
    assert ds
    assert len(gdal.ReadDir(directory)) == 1  # check that the temp file has gone away

    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert ds.GetRasterBand(1).GetOverview(0).GetBlockSize() == [512, 512]
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == cs1
    assert ds.GetRasterBand(1).GetOverview(1).Checksum() == cs2
    ds = None
    _check_cog(filename)

    src_ds = None
    gdal.GetDriverByName("GTiff").Delete(filename)
    gdal.Unlink(directory)


###############################################################################
# Test MAX_Z_ERROR_OVERVIEW creation option


@pytest.mark.require_creation_option("COG", "LERC")
def test_cog_lerc_max_z_error_overview(tmp_vsimem):

    fname = str(tmp_vsimem / "test_cog_lerc_max_z_error_overview.tif")

    gdal.Translate(
        fname,
        "../gdrivers/data/utm.tif",
        options="-of COG -co COMPRESS=LERC -outsize 256 256 -co BLOCKSIZE=128",
    )
    ds = gdal.Open(fname)
    assert float(ds.GetMetadataItem("MAX_Z_ERROR", "_DEBUG_")) == 0
    assert float(ds.GetMetadataItem("MAX_Z_ERROR_OVERVIEW", "_DEBUG_")) == 0.0
    ref_cs_main = ds.GetRasterBand(1).Checksum()
    ref_cs_ovr = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds = None

    gdal.Translate(
        fname,
        "../gdrivers/data/utm.tif",
        options="-of COG -co COMPRESS=LERC -co MAX_Z_ERROR_OVERVIEW=1.5 -outsize 256 256 -co BLOCKSIZE=128",
    )
    ds = gdal.Open(fname)
    assert float(ds.GetMetadataItem("MAX_Z_ERROR", "_DEBUG_")) == 0
    assert float(ds.GetMetadataItem("MAX_Z_ERROR_OVERVIEW", "_DEBUG_")) == 1.5
    got_cs_main = ds.GetRasterBand(1).Checksum()
    got_cs_ovr = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds = None

    assert got_cs_main == ref_cs_main
    assert got_cs_ovr != ref_cs_ovr


###############################################################################
# Test full world reprojection to WebMercator


@pytest.mark.require_creation_option("COG", "JPEG")
def test_cog_small_world_to_web_mercator():

    tab = [0]

    def my_cbk(pct, _, arg):
        assert pct >= tab[0]
        tab[0] = pct
        return 1

    directory = "/vsimem/test_cog_small_world_to_web_mercator"
    gdal.Mkdir(directory, 0o755)
    filename = directory + "/cog.tif"
    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=["TILING_SCHEME=GoogleMapsCompatible", "COMPRESS=JPEG"],
        callback=my_cbk,
        callback_data=tab,
    )
    assert tab[0] == 1.0
    assert ds
    assert len(gdal.ReadDir(directory)) == 1  # check that the temp file has gone away

    ds = None
    ds = gdal.Open(filename)
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 256
    assert ds.RasterYSize == 256
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    assert ds.GetRasterBand(1).GetBlockSize() == [256, 256]
    gt = ds.GetGeoTransform()
    assert gt[1] == -gt[5]  # yes, checking for strict equality
    expected_gt = [
        -20037508.342789248,
        156543.033928041,
        0.0,
        20037508.342789248,
        0.0,
        -156543.033928041,
    ]
    for i in range(6):
        if gt[i] != pytest.approx(expected_gt[i], abs=1e-10 * abs(expected_gt[i])):
            assert False, gt
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert got_cs in (
        [26293, 23439, 14955],
        [26228, 22085, 12992],
        [25088, 23140, 13265],  # libjpeg 9e
    )
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 17849
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    ds = None
    _check_cog(filename)

    src_ds = None
    gdal.GetDriverByName("GTiff").Delete(filename)
    gdal.Unlink(directory)


###############################################################################
# Test reprojection of small extent to WebMercator


def test_cog_byte_to_web_mercator():

    tab = [0]

    def my_cbk(pct, _, arg):
        assert pct >= tab[0]
        tab[0] = pct
        return 1

    directory = "/vsimem/test_cog_byte_to_web_mercator"
    gdal.Mkdir(directory, 0o755)
    filename = directory + "/cog.tif"
    src_ds = gdal.Open("data/byte.tif")
    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=["TILING_SCHEME=GoogleMapsCompatible", "ALIGNED_LEVELS=3"],
        callback=my_cbk,
        callback_data=tab,
    )
    assert tab[0] == 1.0
    assert ds
    assert len(gdal.ReadDir(directory)) == 1  # check that the temp file has gone away

    ds = None
    ds = gdal.Open(filename)
    assert ds.RasterCount == 2
    assert ds.RasterXSize == 1024
    assert ds.RasterYSize == 1024
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALPHA + gdal.GMF_PER_DATASET
    assert ds.GetRasterBand(1).GetBlockSize() == [256, 256]
    gt = ds.GetGeoTransform()
    assert gt[1] == -gt[5]  # yes, checking for strict equality
    expected_gt = [
        -13149614.849955443,
        76.43702828517598,
        0.0,
        4070118.8821290657,
        0.0,
        -76.43702828517598,
    ]
    for i in range(6):
        if gt[i] != pytest.approx(expected_gt[i], abs=1e-10 * abs(expected_gt[i])):
            assert False, gt
    assert ds.GetRasterBand(1).Checksum() in (
        4363,
        4264,  # got on Mac at some point
        4362,  # libjpeg 9d
        4569,  # libjpeg 9e
    )
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 4356
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    ds = None
    _check_cog(filename)

    # Use our generated COG as the input of the same COG generation: reprojection
    # should be skipped
    filename2 = directory + "/cog2.tif"
    src_ds = gdal.Open(filename)

    class my_error_handler:
        def __init__(self):
            self.debug_msg_list = []
            self.other_msg_list = []

        def handler(self, eErrClass, err_no, msg):
            if eErrClass == gdal.CE_Debug:
                self.debug_msg_list.append(msg)
            else:
                self.other_msg_list.append(msg)

    handler = my_error_handler()
    try:
        gdal.PushErrorHandler(handler.handler)
        gdal.SetCurrentErrorHandlerCatchDebug(True)
        with gdaltest.config_option("CPL_DEBUG", "COG"):
            ds = gdal.GetDriverByName("COG").CreateCopy(
                filename2,
                src_ds,
                options=["TILING_SCHEME=GoogleMapsCompatible", "ALIGNED_LEVELS=3"],
            )
    finally:
        gdal.PopErrorHandler()

    assert ds
    assert (
        "COG: Skipping reprojection step: source dataset matches reprojection specifications"
        in handler.debug_msg_list
    )
    assert handler.other_msg_list == []
    src_ds = None
    ds = None

    # Cleanup
    gdal.GetDriverByName("GTiff").Delete(filename)
    gdal.GetDriverByName("GTiff").Delete(filename2)
    gdal.Unlink(directory)


###############################################################################
# Same as previous test case but with other input options


def test_cog_byte_to_web_mercator_manual():

    directory = "/vsimem/test_cog_byte_to_web_mercator_manual"
    gdal.Mkdir(directory, 0o755)
    filename = directory + "/cog.tif"
    src_ds = gdal.Open("data/byte.tif")
    res = 76.43702828517598
    minx = -13149614.849955443
    maxx = minx + 1024 * res
    maxy = 4070118.8821290657
    miny = maxy - 1024 * res
    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=[
            "BLOCKSIZE=256",
            "TARGET_SRS=EPSG:3857",
            "RES=%.17g" % res,
            "EXTENT=%.17g,%.17g,%.17g,%.17g" % (minx, miny, maxx, maxy),
        ],
    )
    assert ds

    ds = None
    ds = gdal.Open(filename)
    assert ds.RasterCount == 2
    assert ds.RasterXSize == 1024
    assert ds.RasterYSize == 1024
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALPHA + gdal.GMF_PER_DATASET
    assert ds.GetRasterBand(1).GetBlockSize() == [256, 256]
    expected_gt = [
        -13149614.849955443,
        76.43702828517598,
        0.0,
        4070118.8821290657,
        0.0,
        -76.43702828517598,
    ]
    assert ds.GetGeoTransform() == pytest.approx(expected_gt, rel=1e-10)
    assert ds.GetRasterBand(1).Checksum() in (
        4363,
        4264,  # got on Mac at some point
        4362,  # libjpeg 9d
        4569,  # libjpeg 9e
    )
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 4356
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    ds = None
    src_ds = None

    # Check that we correctly round to the closest tile if input bounds are
    # very close to its boundary (less than half a pixel)
    filename2 = directory + "/cog2.tif"
    eps = 0.49 * res
    gdal.Translate(
        filename2,
        filename,
        options="-of COG -co TILING_SCHEME=GoogleMapsCompatible -a_ullr %.17g %.17g %.17g %.17g"
        % (minx - eps, maxy + eps, maxx + eps, miny - eps),
    )
    ds = gdal.Open(filename2)
    assert ds.RasterXSize == 1024
    assert ds.RasterYSize == 1024
    assert ds.GetGeoTransform() == pytest.approx(expected_gt, rel=1e-10)
    ds = None

    gdal.GetDriverByName("GTiff").Delete(filename)
    gdal.GetDriverByName("GTiff").Delete(filename2)
    gdal.Unlink(directory)


###############################################################################
# Test OVERVIEWS creation option


def test_cog_overviews_co():
    def my_cbk(pct, _, arg):
        assert pct >= tab[0]
        tab[0] = pct
        return 1

    directory = "/vsimem/test_cog_overviews_co"
    filename = directory + "/cog.tif"
    src_ds = gdal.Translate("", "data/byte.tif", options="-of MEM -outsize 2048 300")

    for val in ["NONE", "FORCE_USE_EXISTING"]:

        tab = [0]
        ds = gdal.GetDriverByName("COG").CreateCopy(
            filename,
            src_ds,
            options=["OVERVIEWS=" + val],
            callback=my_cbk,
            callback_data=tab,
        )
        assert tab[0] == 1.0
        assert ds

        ds = None
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
        assert ds.GetRasterBand(1).GetOverviewCount() == 0
        ds = None
        _check_cog(filename)

    for val in ["AUTO", "IGNORE_EXISTING"]:

        tab = [0]
        ds = gdal.GetDriverByName("COG").CreateCopy(
            filename,
            src_ds,
            options=["OVERVIEWS=" + val],
            callback=my_cbk,
            callback_data=tab,
        )
        assert tab[0] == 1.0
        assert ds

        ds = None
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
        assert ds.GetRasterBand(1).GetOverviewCount() == 2
        assert ds.GetRasterBand(1).GetOverview(0).Checksum() != 0
        ds = None
        _check_cog(filename)

    # Add overviews to source
    src_ds.BuildOverviews("NONE", [2])

    tab = [0]
    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["OVERVIEWS=NONE"], callback=my_cbk, callback_data=tab
    )
    assert tab[0] == 1.0
    assert ds

    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    ds = None
    _check_cog(filename)

    tab = [0]
    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=["OVERVIEWS=FORCE_USE_EXISTING"],
        callback=my_cbk,
        callback_data=tab,
    )
    assert tab[0] == 1.0
    assert ds

    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 0
    ds = None
    _check_cog(filename)

    tab = [0]
    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=["OVERVIEWS=IGNORE_EXISTING"],
        callback=my_cbk,
        callback_data=tab,
    )
    assert tab[0] == 1.0
    assert ds

    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() != 0
    ds = None
    _check_cog(filename)

    src_ds = None
    gdal.GetDriverByName("GTiff").Delete(filename)
    gdal.Unlink(directory)


###############################################################################
# Test editing and invalidating a COG file


@gdaltest.enable_exceptions()
def test_cog_invalidation_by_data_change():

    filename = "/vsimem/cog.tif"
    src_ds = gdal.GetDriverByName("MEM").Create("", 100, 100)
    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["COMPRESS=DEFLATE"]
    )
    ds = None

    with pytest.raises(
        Exception,
        match="IGNORE_COG_LAYOUT_BREAK",
    ):
        gdal.Open(filename, gdal.GA_Update)

    ds = gdal.OpenEx(
        filename, gdal.GA_Update, open_options=["IGNORE_COG_LAYOUT_BREAK=YES"]
    )
    assert ds.GetMetadataItem("LAYOUT", "IMAGE_STRUCTURE") == "COG"
    src_ds = gdal.Open("data/byte.tif")
    data = src_ds.ReadRaster()
    ds.GetRasterBand(1).WriteRaster(0, 0, 20, 20, data)
    with gdal.quiet_errors():
        assert ds.FlushCache() == gdal.CE_None
    ds = None

    with gdal.quiet_errors():
        ds = gdal.Open(filename)
    assert ds.GetMetadataItem("LAYOUT", "IMAGE_STRUCTURE") is None
    ds = None

    with pytest.raises(
        AssertionError, match="KNOWN_INCOMPATIBLE_EDITION=YES is declared in the file"
    ):
        _check_cog(filename)

    with gdal.quiet_errors():
        gdal.GetDriverByName("GTiff").Delete(filename)


###############################################################################
# Test editing and invalidating a COG file


def test_cog_invalidation_by_metadata_change():

    filename = "/vsimem/cog.tif"
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["COMPRESS=DEFLATE"]
    )
    ds = None

    ds = gdal.OpenEx(
        filename, gdal.GA_Update, open_options=["IGNORE_COG_LAYOUT_BREAK=YES"]
    )
    ds.GetRasterBand(1).ComputeStatistics(False)
    ds = None

    with gdal.quiet_errors():
        ds = gdal.Open(filename)
    assert ds.GetMetadataItem("LAYOUT", "IMAGE_STRUCTURE") is None
    ds = None

    with gdal.quiet_errors():
        gdal.GetDriverByName("GTiff").Delete(filename)


###############################################################################
# Test a tiling scheme with a CRS with northing/easting axis order
# and non power-of-two ratios of scales.


def test_cog_northing_easting_and_non_power_of_two_ratios():

    filename = "/vsimem/cog.tif"

    x0_NZTM2000 = -1000000
    y0_NZTM2000 = 10000000
    blocksize = 256
    scale_denom_zoom_level_14 = 1000
    scale_denom_zoom_level_13 = 2500
    scale_denom_zoom_level_12 = 5000

    ds = gdal.Translate(
        filename,
        "data/byte.tif",
        options="-of COG -a_srs EPSG:2193 -a_ullr 1000001 5000001 1000006.6 4999995.4 -co TILING_SCHEME=NZTM2000 -co ALIGNED_LEVELS=2",
    )
    assert ds.RasterXSize == 1280
    assert ds.RasterYSize == 1280
    b = ds.GetRasterBand(1)
    assert [
        (b.GetOverview(i).XSize, b.GetOverview(i).YSize)
        for i in range(b.GetOverviewCount())
    ] == [(512, 512), (256, 256)]

    gt = ds.GetGeoTransform()
    assert gt[1] == -gt[5]  # yes, checking for strict equality

    res_zoom_level_14 = (
        scale_denom_zoom_level_14 * 0.28e-3
    )  # According to OGC Tile Matrix Set formula
    assert gt == pytest.approx(
        (999872, res_zoom_level_14, 0, 5000320, 0, -res_zoom_level_14), abs=1e-8
    )

    # Check that gt origin matches the corner of a tile at zoom 14
    res = gt[1]
    tile_x = (gt[0] - x0_NZTM2000) / (blocksize * res)
    assert tile_x == pytest.approx(round(tile_x))
    tile_y = (y0_NZTM2000 - gt[3]) / (blocksize * res)
    assert tile_y == pytest.approx(round(tile_y))

    # Check that overview=0 corresponds to the resolution of zoom level=13 / OGC ScaleDenom = 2500
    ovr0_xsize = b.GetOverview(0).XSize
    assert (
        float(ovr0_xsize) / ds.RasterXSize
        == float(scale_denom_zoom_level_14) / scale_denom_zoom_level_13
    )
    # Check that gt origin matches the corner of a tile at zoom 13
    ovr0_res = res * scale_denom_zoom_level_13 / scale_denom_zoom_level_14
    tile_x = (gt[0] - x0_NZTM2000) / (blocksize * ovr0_res)
    assert tile_x == pytest.approx(round(tile_x))
    tile_y = (y0_NZTM2000 - gt[3]) / (blocksize * ovr0_res)
    assert tile_y == pytest.approx(round(tile_y))

    # Check that overview=1 corresponds to the resolution of zoom level=12 / OGC ScaleDenom = 5000
    ovr1_xsize = b.GetOverview(1).XSize
    assert (
        float(ovr1_xsize) / ds.RasterXSize
        == float(scale_denom_zoom_level_14) / scale_denom_zoom_level_12
    )
    # Check that gt origin matches the corner of a tile at zoom 12
    ovr1_res = res * scale_denom_zoom_level_12 / scale_denom_zoom_level_14
    tile_x = (gt[0] - x0_NZTM2000) / (blocksize * ovr1_res)
    assert tile_x == pytest.approx(round(tile_x))
    tile_y = (y0_NZTM2000 - gt[3]) / (blocksize * ovr1_res)
    assert tile_y == pytest.approx(round(tile_y))

    assert ds.GetMetadata("TILING_SCHEME") == {
        "NAME": "NZTM2000",
        "ZOOM_LEVEL": "14",
        "ALIGNED_LEVELS": "2",
    }

    ds = None
    gdal.GetDriverByName("GTiff").Delete(filename)


###############################################################################
# Test SPARSE_OK=YES


def test_cog_sparse():

    filename = "/vsimem/cog.tif"
    src_ds = gdal.GetDriverByName("MEM").Create("", 512, 512)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.WriteRaster(0, 0, 256, 256, "\x00" * 256 * 256)
    src_ds.WriteRaster(256, 256, 128, 128, "\x00" * 128 * 128)
    src_ds.BuildOverviews("NEAREST", [2])
    gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["BLOCKSIZE=128", "SPARSE_OK=YES", "COMPRESS=LZW"]
    )
    _check_cog(filename)
    with gdaltest.config_option("GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE", "YES"):
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF") is None
        assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_1_0", "TIFF") is None
        assert (
            ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_2_0", "TIFF") is not None
        )
        assert (
            ds.GetRasterBand(1)
            .GetOverview(0)
            .GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
            is None
        )
        assert (
            ds.GetRasterBand(1)
            .GetOverview(0)
            .GetMetadataItem("BLOCK_OFFSET_1_0", "TIFF")
            is not None
        )
        assert ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(
            1
        ).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetOverview(0).ReadRaster(
            0, 0, 256, 256
        ) == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256)

        if check_libtiff_internal_or_at_least(4, 0, 11):
            # This file is the same as the one generated above, except that we have,
            # with an hex editor, zeroify all entries of TileByteCounts except the
            # last tile of the main IFD, and for a tile when the next tile is sparse
            ds = gdal.Open("data/cog_sparse_strile_arrays_zeroified_when_possible.tif")
            assert ds.GetRasterBand(1).ReadRaster(
                0, 0, 512, 512
            ) == src_ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512)

    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test SPARSE_OK=YES with mask


@pytest.mark.require_creation_option("COG", "JPEG")
def test_cog_sparse_mask():

    filename = "/vsimem/cog.tif"
    src_ds = gdal.GetDriverByName("MEM").Create("", 512, 512, 4)
    for i in range(4):
        src_ds.GetRasterBand(i + 1).SetColorInterpretation(gdal.GCI_RedBand + i)
        src_ds.GetRasterBand(i + 1).Fill(255)
        src_ds.GetRasterBand(i + 1).WriteRaster(0, 0, 256, 256, "\x00" * 256 * 256)
        src_ds.GetRasterBand(i + 1).WriteRaster(256, 256, 128, 128, "\x00" * 128 * 128)
    src_ds.BuildOverviews("NEAREST", [2])
    gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=[
            "BLOCKSIZE=128",
            "SPARSE_OK=YES",
            "COMPRESS=JPEG",
            "RESAMPLING=NEAREST",
        ],
    )
    assert gdal.GetLastErrorMsg() == ""
    _check_cog(filename)
    with gdaltest.config_option("GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE", "YES"):
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF") is None
        assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_1_0", "TIFF") is None
        assert (
            ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_2_0", "TIFF") is not None
        )
        assert (
            ds.GetRasterBand(1)
            .GetMaskBand()
            .GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
            is None
        )
        assert (
            ds.GetRasterBand(1)
            .GetMaskBand()
            .GetMetadataItem("BLOCK_OFFSET_1_0", "TIFF")
            is None
        )
        assert (
            ds.GetRasterBand(1)
            .GetMaskBand()
            .GetMetadataItem("BLOCK_OFFSET_2_0", "TIFF")
            is not None
        )
        assert (
            ds.GetRasterBand(1)
            .GetOverview(0)
            .GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
            is None
        )
        assert (
            ds.GetRasterBand(1)
            .GetOverview(0)
            .GetMetadataItem("BLOCK_OFFSET_1_0", "TIFF")
            is not None
        )
        assert (
            ds.GetRasterBand(1)
            .GetOverview(0)
            .GetMaskBand()
            .GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
            is None
        )
        assert (
            ds.GetRasterBand(1)
            .GetOverview(0)
            .GetMaskBand()
            .GetMetadataItem("BLOCK_OFFSET_1_0", "TIFF")
            is not None
        )
        assert ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(
            1
        ).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetMaskBand().ReadRaster(
            0, 0, 512, 512
        ) == src_ds.GetRasterBand(4).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetOverview(0).ReadRaster(
            0, 0, 256, 256
        ) == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256)
        assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().ReadRaster(
            0, 0, 256, 256
        ) == src_ds.GetRasterBand(4).GetOverview(0).ReadRaster(0, 0, 256, 256)

    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test SPARSE_OK=YES with imagery at 0 and mask at 255


@pytest.mark.require_creation_option("COG", "JPEG")
def test_cog_sparse_imagery_0_mask_255():

    filename = "/vsimem/cog.tif"
    src_ds = gdal.GetDriverByName("MEM").Create("", 512, 512, 4)
    for i in range(4):
        src_ds.GetRasterBand(i + 1).SetColorInterpretation(gdal.GCI_RedBand + i)
        src_ds.GetRasterBand(i + 1).Fill(0 if i < 3 else 255)
    src_ds.BuildOverviews("NEAREST", [2])
    gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["BLOCKSIZE=128", "SPARSE_OK=YES", "COMPRESS=JPEG"]
    )
    _check_cog(filename)
    with gdaltest.config_option("GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE", "YES"):
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF") is None
        assert (
            ds.GetRasterBand(1)
            .GetMaskBand()
            .GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
            is not None
        )
        assert (
            ds.GetRasterBand(1)
            .GetOverview(0)
            .GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
            is None
        )
        assert (
            ds.GetRasterBand(1)
            .GetOverview(0)
            .GetMaskBand()
            .GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
            is not None
        )
        assert ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(
            1
        ).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetMaskBand().ReadRaster(
            0, 0, 512, 512
        ) == src_ds.GetRasterBand(4).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetOverview(0).ReadRaster(
            0, 0, 256, 256
        ) == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256)
        assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().ReadRaster(
            0, 0, 256, 256
        ) == src_ds.GetRasterBand(4).GetOverview(0).ReadRaster(0, 0, 256, 256)

    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test SPARSE_OK=YES with imagery at 0 or 255 and mask at 255


@pytest.mark.require_creation_option("COG", "JPEG")
def test_cog_sparse_imagery_0_or_255_mask_255():

    filename = "/vsimem/cog.tif"
    src_ds = gdal.GetDriverByName("MEM").Create("", 512, 512, 4)
    for i in range(4):
        src_ds.GetRasterBand(i + 1).SetColorInterpretation(gdal.GCI_RedBand + i)
    for i in range(3):
        src_ds.GetRasterBand(i + 1).Fill(255)
        src_ds.GetRasterBand(i + 1).WriteRaster(0, 0, 256, 256, "\x00" * 256 * 256)
        src_ds.GetRasterBand(i + 1).WriteRaster(256, 256, 128, 128, "\x00" * 128 * 128)
    src_ds.GetRasterBand(4).Fill(255)
    src_ds.BuildOverviews("NEAREST", [2])
    gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=[
            "BLOCKSIZE=128",
            "SPARSE_OK=YES",
            "COMPRESS=JPEG",
            "RESAMPLING=NEAREST",
        ],
    )
    _check_cog(filename)
    with gdaltest.config_option("GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE", "YES"):
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF") is None
        assert (
            ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_2_0", "TIFF") is not None
        )
        assert (
            ds.GetRasterBand(1)
            .GetMaskBand()
            .GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
            is not None
        )
        assert (
            ds.GetRasterBand(1)
            .GetOverview(0)
            .GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
            is None
        )
        assert (
            ds.GetRasterBand(1)
            .GetOverview(0)
            .GetMaskBand()
            .GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
            is not None
        )
        assert ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(
            1
        ).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetMaskBand().ReadRaster(
            0, 0, 512, 512
        ) == src_ds.GetRasterBand(4).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetOverview(0).ReadRaster(
            0, 0, 256, 256
        ) == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256)
        assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().ReadRaster(
            0, 0, 256, 256
        ) == src_ds.GetRasterBand(4).GetOverview(0).ReadRaster(0, 0, 256, 256)

    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test SPARSE_OK=YES with imagery and mask at 0


@pytest.mark.require_creation_option("COG", "JPEG")
def test_cog_sparse_imagery_mask_0():

    filename = "/vsimem/cog.tif"
    src_ds = gdal.GetDriverByName("MEM").Create("", 512, 512, 4)
    for i in range(4):
        src_ds.GetRasterBand(i + 1).SetColorInterpretation(gdal.GCI_RedBand + i)
        src_ds.GetRasterBand(i + 1).Fill(0)
    src_ds.BuildOverviews("NEAREST", [2])
    gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["BLOCKSIZE=128", "SPARSE_OK=YES", "COMPRESS=JPEG"]
    )
    _check_cog(filename)
    with gdaltest.config_option("GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE", "YES"):
        ds = gdal.Open(filename)
        assert ds.GetRasterBand(1).GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF") is None
        assert (
            ds.GetRasterBand(1)
            .GetMaskBand()
            .GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
            is None
        )
        assert (
            ds.GetRasterBand(1)
            .GetOverview(0)
            .GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
            is None
        )
        assert (
            ds.GetRasterBand(1)
            .GetOverview(0)
            .GetMaskBand()
            .GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
            is None
        )
        assert ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512) == src_ds.GetRasterBand(
            1
        ).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetMaskBand().ReadRaster(
            0, 0, 512, 512
        ) == src_ds.GetRasterBand(4).ReadRaster(0, 0, 512, 512)
        assert ds.GetRasterBand(1).GetOverview(0).ReadRaster(
            0, 0, 256, 256
        ) == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 256, 256)
        assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().ReadRaster(
            0, 0, 256, 256
        ) == src_ds.GetRasterBand(4).GetOverview(0).ReadRaster(0, 0, 256, 256)

    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test ZOOM_LEVEL_STRATEGY option


@pytest.mark.parametrize(
    "zoom_level_strategy,expected_gt",
    [
        (
            "AUTO",
            (
                -13110479.09147343,
                76.43702828517416,
                0.0,
                4030983.1236470547,
                0.0,
                -76.43702828517416,
            ),
        ),
        (
            "LOWER",
            (
                -13110479.09147343,
                76.43702828517416,
                0.0,
                4030983.1236470547,
                0.0,
                -76.43702828517416,
            ),
        ),
        (
            "UPPER",
            (
                -13100695.151852928,
                38.21851414258708,
                0.0,
                4021199.1840265524,
                0.0,
                -38.21851414258708,
            ),
        ),
    ],
)
def test_cog_zoom_level_strategy(zoom_level_strategy, expected_gt):

    filename = "/vsimem/test_cog_zoom_level_strategy.tif"
    src_ds = gdal.Open("data/byte.tif")
    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=[
            "TILING_SCHEME=GoogleMapsCompatible",
            "ZOOM_LEVEL_STRATEGY=" + zoom_level_strategy,
        ],
    )
    gt = ds.GetGeoTransform()
    assert gt == pytest.approx(expected_gt, rel=1e-10)

    # Test that the zoom level strategy applied on input data already on a
    # zoom level doesn't lead to selecting another zoom level
    filename2 = "/vsimem/test_cog_zoom_level_strategy_2.tif"
    src_ds = gdal.Open("data/byte.tif")
    ds2 = gdal.GetDriverByName("COG").CreateCopy(
        filename2,
        ds,
        options=[
            "TILING_SCHEME=GoogleMapsCompatible",
            "ZOOM_LEVEL_STRATEGY=" + zoom_level_strategy,
        ],
    )
    gt = ds2.GetGeoTransform()
    assert gt == pytest.approx(expected_gt, rel=1e-10)
    ds2 = None
    gdal.Unlink(filename2)

    ds = None
    gdal.Unlink(filename)


###############################################################################
# Test ZOOM_LEVEL option


def test_cog_zoom_level():

    filename = "/vsimem/test_cog_zoom_level.tif"
    src_ds = gdal.Open("data/byte.tif")

    with gdal.quiet_errors():
        assert (
            gdal.GetDriverByName("COG").CreateCopy(
                filename,
                src_ds,
                options=["TILING_SCHEME=GoogleMapsCompatible", "ZOOM_LEVEL=-1"],
            )
            is None
        )
        assert (
            gdal.GetDriverByName("COG").CreateCopy(
                filename,
                src_ds,
                options=["TILING_SCHEME=GoogleMapsCompatible", "ZOOM_LEVEL=31"],
            )
            is None
        )

    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=["TILING_SCHEME=GoogleMapsCompatible", "ZOOM_LEVEL=12"],
    )
    gt = ds.GetGeoTransform()
    expected_gt = (
        -13100695.151852928,
        38.21851414258813,
        0.0,
        4021199.1840265524,
        0.0,
        -38.21851414258813,
    )
    assert gt == pytest.approx(expected_gt, rel=1e-10)
    ds = None
    gdal.Unlink(filename)


###############################################################################


def test_cog_resampling_options():

    filename = "/vsimem/test_cog_resampling_options.tif"
    src_ds = gdal.Open("data/byte.tif")

    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=["TILING_SCHEME=GoogleMapsCompatible", "WARP_RESAMPLING=NEAREST"],
    )
    cs1 = ds.GetRasterBand(1).Checksum()

    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=["TILING_SCHEME=GoogleMapsCompatible", "WARP_RESAMPLING=CUBIC"],
    )
    cs2 = ds.GetRasterBand(1).Checksum()

    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=[
            "TILING_SCHEME=GoogleMapsCompatible",
            "RESAMPLING=NEAREST",
            "WARP_RESAMPLING=CUBIC",
        ],
    )
    cs3 = ds.GetRasterBand(1).Checksum()

    assert cs1 != cs2
    assert cs2 == cs3

    src_ds = gdal.Translate("", "data/byte.tif", options="-of MEM -outsize 129 0")
    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["BLOCKSIZE=128", "OVERVIEW_RESAMPLING=NEAREST"]
    )
    cs1 = ds.GetRasterBand(1).GetOverview(0).Checksum()

    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["BLOCKSIZE=128", "OVERVIEW_RESAMPLING=BILINEAR"]
    )
    cs2 = ds.GetRasterBand(1).GetOverview(0).Checksum()

    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=["BLOCKSIZE=128", "RESAMPLING=NEAREST", "OVERVIEW_RESAMPLING=BILINEAR"],
    )
    cs3 = ds.GetRasterBand(1).GetOverview(0).Checksum()

    assert cs1 != cs2
    assert cs2 == cs3

    ds = None
    gdal.Unlink(filename)


###############################################################################


def test_cog_invalid_warp_resampling():

    filename = "/vsimem/test_cog_invalid_warp_resampling.tif"
    src_ds = gdal.Open("data/byte.tif")

    with gdal.quiet_errors():
        assert (
            gdal.GetDriverByName("COG").CreateCopy(
                filename,
                src_ds,
                options=["TILING_SCHEME=GoogleMapsCompatible", "RESAMPLING=INVALID"],
            )
            is None
        )
    gdal.Unlink(filename)


###############################################################################


def test_cog_overview_size():

    src_ds = gdal.GetDriverByName("MEM").Create("", 20480 // 4, 40960 // 4)
    src_ds.SetGeoTransform([1723840, 7 * 4, 0, 5555840, 0, -7 * 4])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(2193)
    src_ds.SetProjection(srs.ExportToWkt())
    filename = "/vsimem/test_cog_overview_size.tif"
    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename,
        src_ds,
        options=[
            "TILING_SCHEME=NZTM2000",
            "ALIGNED_LEVELS=4",
            "OVERVIEW_RESAMPLING=NONE",
        ],
    )
    assert (ds.RasterXSize, ds.RasterYSize) == (20480 // 4, 40960 // 4)
    ovr_size = [
        (
            ds.GetRasterBand(1).GetOverview(i).XSize,
            ds.GetRasterBand(1).GetOverview(i).YSize,
        )
        for i in range(ds.GetRasterBand(1).GetOverviewCount())
    ]
    assert ovr_size == [(2048, 4096), (1024, 2048), (512, 1024), (256, 512), (128, 256)]
    gdal.Unlink(filename)


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/2946


def test_cog_float32_color_table():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1024, 1024, 1, gdal.GDT_Float32)
    src_ds.GetRasterBand(1).Fill(1.0)
    ct = gdal.ColorTable()
    src_ds.GetRasterBand(1).SetColorTable(ct)
    filename = "/vsimem/test_cog_float32_color_table.tif"
    # Silence warning about color table not being copied
    with gdal.quiet_errors():
        ds = gdal.GetDriverByName("COG").CreateCopy(filename, src_ds)  # segfault
    assert ds
    assert ds.GetRasterBand(1).GetColorTable() is None
    assert struct.unpack("f", ds.ReadRaster(0, 0, 1, 1))[0] == 1.0
    assert (
        struct.unpack("f", ds.GetRasterBand(1).GetOverview(0).ReadRaster(0, 0, 1, 1))[0]
        == 1.0
    )
    gdal.Unlink(filename)


###############################################################################
# Test copy XMP


def test_cog_copy_xmp():

    filename = "/vsimem/cog_xmp.tif"
    src_ds = gdal.Open("../gdrivers/data/gtiff/byte_with_xmp.tif")
    ds = gdal.GetDriverByName("COG").CreateCopy(filename, src_ds)
    assert ds
    ds = None

    ds = gdal.Open(filename)
    xmp = ds.GetMetadata("xml:XMP")
    ds = None
    assert "W5M0MpCehiHzreSzNTczkc9d" in xmp[0], "Wrong input file without XMP"
    _check_cog(filename)

    gdal.Unlink(filename)


###############################################################################
# Test creating COG from a source dataset that has overview with 'odd' sizes
# and a mask without overview


def test_cog_odd_overview_size_and_msk():

    filename = "/vsimem/test_cog_odd_overview_size_and_msk.tif"
    src_ds = gdal.GetDriverByName("MEM").Create("", 511, 511)
    src_ds.BuildOverviews("NEAR", [2])
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    ds = gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["BLOCKSIZE=256"]
    )
    assert ds
    assert ds.GetRasterBand(1).GetOverview(0).XSize == 256
    assert ds.GetRasterBand(1).GetMaskBand().GetOverview(0).XSize == 256
    ds = None

    gdal.Unlink(filename)


###############################################################################
# Test turning on lossy WEBP compression if OVERVIEW_QUALITY < 100 specified


@pytest.mark.require_creation_option("COG", "WEBP")
@pytest.mark.require_driver("WEBP")
def test_cog_webp_overview_turn_on_lossy_if_webp_level():

    tmpfilename = "/vsimem/test_cog_webp_overview_turn_on_lossy_if_webp_level.tif"

    gdal.Translate(
        tmpfilename,
        "../gdrivers/data/small_world.tif",
        options="-of COG -outsize 513 0 -co COMPRESS=WEBP -co QUALITY=100 -co OVERVIEW_QUALITY=75",
    )

    ds = gdal.Open(tmpfilename)
    assert (
        ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE") == "LOSSLESS"
    )
    assert (
        ds.GetRasterBand(1)
        .GetOverview(0)
        .GetDataset()
        .GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE")
        == "LOSSY"
    )
    ds = None

    gdal.Unlink(tmpfilename)


###############################################################################
# Test lossless WEBP compression


@pytest.mark.require_creation_option("COG", "WEBP")
@pytest.mark.require_driver("WEBP")
def test_cog_webp_lossless_webp():

    tmpfilename = "/vsimem/test_cog_webp_lossless_webp.tif"

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    gdal.ErrorReset()
    gdal.Translate(
        tmpfilename,
        src_ds,
        options="-of COG -co COMPRESS=WEBP -co QUALITY=100",
    )
    assert gdal.GetLastErrorMsg() == ""

    ds = gdal.Open(tmpfilename)
    assert (
        ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE") == "LOSSLESS"
    )
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.Unlink(tmpfilename)


###############################################################################
# Test OVERVIEW_COUNT option


@pytest.mark.parametrize(
    "options,expected_count",
    [
        ("", 1),
        ("-co OVERVIEW_COUNT=1", 1),
        ("-co OVERVIEW_COUNT=2", 2),
        ("-co OVERVIEW_COUNT=10", 8),
        ("-co TILING_SCHEME=GoogleMapsCompatible -co OVERVIEW_COUNT=1", 1),
        ("-co TILING_SCHEME=GoogleMapsCompatible -co OVERVIEW_COUNT=2", 2),
        ("-co TILING_SCHEME=GoogleMapsCompatible -co OVERVIEW_COUNT=10", 8),
    ],
)
def test_cog_overview_count(options, expected_count):

    tmpfilename = "/vsimem/test_cog_overview_count.tif"

    with gdal.quiet_errors():
        gdal.Translate(
            tmpfilename,
            "../gdrivers/data/small_world.tif",
            options="-of COG -co BLOCKSIZE=256 " + options,
        )
    ds = gdal.Open(tmpfilename)
    assert ds.GetRasterBand(1).GetOverviewCount() == expected_count
    ds = None
    gdal.Unlink(tmpfilename)


###############################################################################
# Test OVERVIEW_COUNT option with dataset with existing overviews


def test_cog_overview_count_existing():

    tmpfilename = "/vsimem/test_cog_overview_count_existing.tif"

    src_ds = gdal.GetDriverByName("MEM").Create("", 512, 512)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.BuildOverviews("NONE", [2, 4, 8, 16])
    gdal.Translate(tmpfilename, src_ds, options="-of COG -co OVERVIEW_COUNT=3")
    ds = gdal.Open(tmpfilename)
    assert ds.GetRasterBand(1).GetOverviewCount() == 3
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 0
    ds = None
    gdal.Unlink(tmpfilename)


###############################################################################
# Test JPEGXL compression with alpha


@pytest.mark.require_creation_option("COG", "JXL")
def test_cog_write_jpegxl_alpha():

    src_ds = gdal.Open("data/stefan_full_rgba.tif")
    filename = "/vsimem/test_tiff_write_jpegxl_alpha_distance_zero.tif"

    gdal.GetDriverByName("GTiff").CreateCopy(
        filename,
        src_ds,
        options=[
            "COMPRESS=JXL",
            "JXL_LOSSLESS=NO",
            "TILED=YES",
            "BLOCKXSIZE=512",
            "BLOCKYSIZE=512",
        ],
    )
    ds = gdal.Open(filename)
    ref_checksum = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    ds = None

    gdal.Unlink(filename)

    drv = gdal.GetDriverByName("COG")
    drv.CreateCopy(
        filename,
        src_ds,
        options=["COMPRESS=JXL", "JXL_LOSSLESS=NO"],
    )
    ds = gdal.Open(filename)
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == ref_checksum
    ds = None

    gdal.Unlink(filename)


###############################################################################
# Test JXL_ALPHA_DISTANCE creation option


@pytest.mark.require_creation_option(
    "COG", "JXL_ALPHA_DISTANCE"
)  # "libjxl > 0.8.1 required"
def test_cog_write_jpegxl_alpha_distance_zero():

    drv = gdal.GetDriverByName("COG")

    src_ds = gdal.Open("data/stefan_full_rgba.tif")
    filename = "/vsimem/test_tiff_write_jpegxl_alpha_distance_zero.tif"
    drv.CreateCopy(
        filename,
        src_ds,
        options=["COMPRESS=JXL", "JXL_LOSSLESS=NO", "JXL_ALPHA_DISTANCE=0"],
    )
    ds = gdal.Open(filename)
    assert float(ds.GetMetadataItem("JXL_ALPHA_DISTANCE", "IMAGE_STRUCTURE")) == 0
    assert ds.GetRasterBand(1).Checksum() != src_ds.GetRasterBand(1).Checksum()
    assert ds.GetRasterBand(4).Checksum() == src_ds.GetRasterBand(4).Checksum()
    ds = None

    gdal.Unlink(filename)


###############################################################################
# Test NBITS creation option


def test_cog_NBITS():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    drv = gdal.GetDriverByName("COG")
    filename = "/vsimem/test_cog_NBITS.tif"
    drv.CreateCopy(
        filename,
        src_ds,
        options=["NBITS=7"],
    )
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "7"
    ds = None

    gdal.Unlink(filename)


###############################################################################
def test_cog_copy_mdd():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetMetadataItem("FOO", "BAR")
    src_ds.SetMetadataItem("BAR", "BAZ", "OTHER_DOMAIN")
    src_ds.SetMetadataItem("should_not", "be_copied", "IMAGE_STRUCTURE")

    filename = "/vsimem/test_tiff_write_copy_mdd.tif"

    gdal.GetDriverByName("COG").CreateCopy(filename, src_ds)
    ds = gdal.Open(filename)
    assert set(ds.GetMetadataDomainList()) == set(
        ["", "IMAGE_STRUCTURE", "DERIVED_SUBDATASETS"]
    )
    assert ds.GetMetadata_Dict() == {"FOO": "BAR"}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {}
    assert ds.GetMetadata_Dict("IMAGE_STRUCTURE") == {
        "COMPRESSION": "LZW",
        "INTERLEAVE": "BAND",
        "LAYOUT": "COG",
    }
    ds = None

    gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["COPY_SRC_MDD=NO"]
    )
    ds = gdal.Open(filename)
    assert ds.GetMetadata_Dict() == {}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {}
    ds = None

    gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["COPY_SRC_MDD=YES"]
    )
    ds = gdal.Open(filename)
    assert set(ds.GetMetadataDomainList()) == set(
        ["", "IMAGE_STRUCTURE", "DERIVED_SUBDATASETS", "OTHER_DOMAIN"]
    )
    assert ds.GetMetadata_Dict() == {"FOO": "BAR"}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {"BAR": "BAZ"}
    assert ds.GetMetadata_Dict("IMAGE_STRUCTURE") == {
        "COMPRESSION": "LZW",
        "INTERLEAVE": "BAND",
        "LAYOUT": "COG",
    }
    ds = None

    gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["SRC_MDD=OTHER_DOMAIN"]
    )
    ds = gdal.Open(filename)
    assert ds.GetMetadata_Dict() == {}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {"BAR": "BAZ"}
    ds = None

    gdal.GetDriverByName("COG").CreateCopy(
        filename, src_ds, options=["SRC_MDD=", "SRC_MDD=OTHER_DOMAIN"]
    )
    ds = gdal.Open(filename)
    assert ds.GetMetadata_Dict() == {"FOO": "BAR"}
    assert ds.GetMetadata_Dict("OTHER_DOMAIN") == {"BAR": "BAZ"}
    ds = None

    gdal.Unlink(filename)


###############################################################################
@pytest.mark.parametrize(
    "co,nbands,src_has_stats,expected_val",
    [
        ([], 1, False, None),
        ([], 1, True, "10"),
        (["STATISTICS=YES"], 1, False, "10"),
        (["STATISTICS=YES"], 1, True, "10"),
        (["STATISTICS=NO"], 1, False, None),
        (["STATISTICS=NO"], 1, True, None),
        (["TARGET_SRS=EPSG:32631"], 1, False, None),
        (["TARGET_SRS=EPSG:32631"], 1, True, "10"),
        (["TARGET_SRS=EPSG:32631", "STATISTICS=YES"], 1, False, "10"),
        (["TARGET_SRS=EPSG:32631", "STATISTICS=YES"], 1, True, "10"),
        (["TARGET_SRS=EPSG:32631", "STATISTICS=NO"], 1, False, None),
        (["TARGET_SRS=EPSG:32631", "STATISTICS=NO"], 1, True, None),
        (["COMPRESS=JPEG"], 4, False, None),
        (["COMPRESS=JPEG"], 4, True, "10"),
        (["COMPRESS=JPEG", "STATISTICS=YES"], 4, False, "10"),
        (["COMPRESS=JPEG", "STATISTICS=NO"], 4, True, None),
    ],
)
def test_cog_stats(tmp_vsimem, nbands, co, src_has_stats, expected_val):

    if "COMPRESS=JPEG" in co and "JPEG" not in gdal.GetDriverByName(
        "COG"
    ).GetMetadataItem(gdal.DMD_CREATIONOPTIONLIST):
        pytest.skip("JPEG not available")
    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 1, 1, nbands)
    if nbands == 4:
        src_ds.GetRasterBand(4).SetColorInterpretation(gdal.GCI_AlphaBand)
        src_ds.GetRasterBand(1).Fill(10)
        src_ds.GetRasterBand(2).Fill(10)
        src_ds.GetRasterBand(3).Fill(10)
        src_ds.GetRasterBand(4).Fill(255)
    else:
        src_ds.GetRasterBand(1).Fill(10)
    src_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    src_ds.SetSpatialRef(srs)
    if src_has_stats:
        src_ds.GetRasterBand(1).ComputeStatistics(False)
    src_ds = None
    filename = str(tmp_vsimem / "out.tif")
    src_ds = gdal.Open(src_filename)
    if co == ["STATISTICS=YES"]:
        gdal.Translate(filename, src_ds, options="-of COG -stats")
    else:
        gdal.GetDriverByName("COG").CreateCopy(filename, src_ds, options=co)
    src_ds = None
    assert gdal.VSIStatL(src_filename + ".aux.xml") is None
    ds = gdal.Open(filename)
    if nbands == 4:
        assert ds.RasterCount == 3
        assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    assert ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") == expected_val
    if expected_val and ds.RasterCount == 2:
        assert ds.GetRasterBand(2).GetMetadataItem("STATISTICS_MINIMUM") == "255"
    ds = None


###############################################################################


def test_cog_mask_band_overviews(tmp_vsimem):

    """Test bugfix for https://github.com/OSGeo/gdal/issues/10536"""

    filename = str(tmp_vsimem / "out.tif")
    with gdal.config_option("COG_DELETE_TEMP_FILES", "NO"):
        gdal.Translate(
            filename,
            "data/stefan_full_rgba.tif",
            options="-co RESAMPLING=LANCZOS -co OVERVIEW_COUNT=3 -of COG -outsize 1024 0 -b 1 -b 2 -b 3 -mask 4",
        )

    ds = gdal.Open(filename)
    assert [ds.GetRasterBand(i + 1).GetOverview(2).Checksum() for i in range(3)] == [
        51556,
        39258,
        23928,
    ]

    ds = gdal.Open(filename + ".msk.ovr.tmp")
    assert ds.GetMetadataItem("INTERNAL_MASK_FLAGS_1") == "2"
    assert ds.GetRasterBand(1).IsMaskBand()
    assert ds.GetRasterBand(1).GetOverview(0).IsMaskBand()
    assert ds.GetRasterBand(1).GetOverview(1).IsMaskBand()


###############################################################################
# Verify that we can generate an output that is byte-identical to the expected golden file.


@pytest.mark.parametrize(
    "src_filename,creation_options",
    [
        ("data/cog/byte_little_endian_golden.tif", []),
        (
            "data/cog/byte_little_endian_blocksize_16_predictor_standard_golden.tif",
            ["BLOCKSIZE=16", "PREDICTOR=STANDARD"],
        ),
    ],
)
def test_cog_write_check_golden_file(tmp_path, src_filename, creation_options):

    out_filename = str(tmp_path / "test.tif")
    with gdal.config_option("GDAL_TIFF_ENDIANNESS", "LITTLE"):
        with gdal.Open(src_filename) as src_ds:
            gdal.GetDriverByName("COG").CreateCopy(
                out_filename, src_ds, options=creation_options
            )
    assert os.stat(src_filename).st_size == os.stat(out_filename).st_size
    assert open(src_filename, "rb").read() == open(out_filename, "rb").read()


###############################################################################


def test_cog_preserve_ALPHA_PREMULTIPLIED_on_copy(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(
        src_filename, 1, 1, 4, options=["ALPHA=PREMULTIPLIED", "PROFILE=BASELINE"]
    )
    src_ds.SetGeoTransform([500000, 1, 0, 4500000, 0, -1])
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    src_ds.SetProjection(srs.ExportToWkt())

    out_filename = str(tmp_vsimem / "out.tif")
    gdal.GetDriverByName("COG").CreateCopy(
        out_filename,
        src_ds,
        options=[
            "TILING_SCHEME=GoogleMapsCompatible",
        ],
    )
    with gdal.Open(out_filename) as ds:
        assert (
            ds.GetRasterBand(4).GetMetadataItem("ALPHA", "IMAGE_STRUCTURE")
            == "PREMULTIPLIED"
        )


###############################################################################
#


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("INTERLEAVE", ["TILE", "BAND"])
def test_cog_write_interleave_tile_or_band(tmp_vsimem, INTERLEAVE):
    out_filename = str(tmp_vsimem / "out.tif")

    with gdal.quiet_errors():
        gdal.GetDriverByName("COG").CreateCopy(
            out_filename,
            gdal.Open("data/rgbsmall.tif"),
            options=["INTERLEAVE=" + INTERLEAVE, "BLOCKSIZE=32"],
        )

    ds = gdal.Open(out_filename)
    assert ds.GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE") == INTERLEAVE
    assert ds.GetMetadataItem("LAYOUT", "IMAGE_STRUCTURE") == "COG"

    assert [ds.GetRasterBand(band + 1).Checksum() for band in range(3)] == [
        21212,
        21053,
        21349,
    ]

    _check_cog(out_filename)


###############################################################################
#


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("INTERLEAVE", ["TILE", "BAND"])
def test_cog_write_interleave_with_mask(tmp_vsimem, INTERLEAVE):
    out_filename = str(tmp_vsimem / "out.tif")

    with gdal.quiet_errors():
        gdal.GetDriverByName("COG").CreateCopy(
            out_filename,
            gdal.Translate(
                "", "data/stefan_full_rgba.tif", options="-f MEM -b 1 -b 2 -b 3 -mask 4"
            ),
            options=["INTERLEAVE=" + INTERLEAVE, "BLOCKSIZE=32"],
        )

    ds = gdal.Open(out_filename)
    assert ds.GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE") == INTERLEAVE
    assert ds.GetMetadataItem("LAYOUT", "IMAGE_STRUCTURE") == "COG"

    assert [ds.GetRasterBand(band + 1).Checksum() for band in range(3)] == [
        12603,
        58561,
        36064,
    ]
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 22499

    _check_cog(out_filename)

    # Check that the tiles are in the expected order in the file
    if INTERLEAVE == "TILE":
        last_offset = 0
        for y in range(2):
            for x in range(2):
                for band in range(3):
                    offset = int(
                        ds.GetRasterBand(band + 1).GetMetadataItem(
                            f"BLOCK_OFFSET_{x}_{y}", "TIFF"
                        )
                    )
                    assert offset > last_offset
                    last_offset = offset
                offset = int(
                    ds.GetRasterBand(1)
                    .GetMaskBand()
                    .GetMetadataItem(f"BLOCK_OFFSET_{x}_{y}", "TIFF")
                )
                assert offset > last_offset
                last_offset = offset


###############################################################################
#


@gdaltest.enable_exceptions()
def test_cog_write_interleave_tile_with_mask_and_ovr(tmp_vsimem):
    out_filename = str(tmp_vsimem / "out.tif")
    out2_filename = str(tmp_vsimem / "out2.tif")

    ds = gdal.Translate(
        out_filename,
        "data/stefan_full_rgba.tif",
        options="-b 1 -b 2 -b 3 -mask 4 -outsize 1024 0",
    )
    ds.BuildOverviews("NEAR", [2])
    ds.Close()

    ds = gdal.Open(out_filename)
    expected_md = [ds.GetRasterBand(band + 1).Checksum() for band in range(3)]
    expected_md += [ds.GetRasterBand(1).GetMaskBand().Checksum()]
    expected_ovr_md = [
        ds.GetRasterBand(band + 1).GetOverview(0).Checksum() for band in range(3)
    ]
    expected_ovr_md += [ds.GetRasterBand(1).GetMaskBand().GetOverview(0).Checksum()]

    gdal.GetDriverByName("COG").CreateCopy(
        out2_filename,
        ds,
        options=["INTERLEAVE=TILE", "OVERVIEW_RESAMPLING=NEAREST"],
    )

    ds = gdal.Open(out2_filename)
    assert ds.GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE") == "TILE"
    assert ds.GetMetadataItem("LAYOUT", "IMAGE_STRUCTURE") == "COG"

    _check_cog(out2_filename)

    got_md = [ds.GetRasterBand(band + 1).Checksum() for band in range(3)]
    got_md += [ds.GetRasterBand(1).GetMaskBand().Checksum()]
    assert got_md == expected_md
    got_ovr_md = [
        ds.GetRasterBand(band + 1).GetOverview(0).Checksum() for band in range(3)
    ]
    got_ovr_md += [ds.GetRasterBand(1).GetMaskBand().GetOverview(0).Checksum()]
    assert got_ovr_md == expected_ovr_md


###############################################################################
# Check that our reading of a COG with /vsicurl is efficient


@pytest.mark.require_curl()
@pytest.mark.skipif(
    not check_libtiff_internal_or_at_least(4, 0, 11),
    reason="libtiff >= 4.0.11 required",
)
@pytest.mark.parametrize("INTERLEAVE", ["BAND", "TILE"])
def test_cog_interleave_tile_or_band_vsicurl(tmp_vsimem, INTERLEAVE):

    gdal.VSICurlClearCache()

    webserver_process = None
    webserver_port = 0

    (webserver_process, webserver_port) = webserver.launch(
        handler=webserver.DispatcherHttpHandler
    )
    if webserver_port == 0:
        pytest.skip()

    in_filename = str(tmp_vsimem / "in.tif")
    cog_filename = str(tmp_vsimem / "cog.tif")

    ds = gdal.Translate(
        in_filename,
        "data/stefan_full_rgba.tif",
        options="-b 1 -b 2 -b 3 -mask 4 -outsize 1024 0",
    )
    ds.BuildOverviews("NEAR", [2])
    ds.Close()

    src_ds = gdal.Open(in_filename)
    gdal.GetDriverByName("COG").CreateCopy(
        cog_filename,
        src_ds,
        options=["INTERLEAVE=" + INTERLEAVE, "OVERVIEW_RESAMPLING=NEAREST"],
    )

    def extract(offset, size):
        f = gdal.VSIFOpenL(cog_filename, "rb")
        gdal.VSIFSeekL(f, offset, 0)
        data = gdal.VSIFReadL(size, 1, f)
        gdal.VSIFCloseL(f)
        return data

    try:
        filesize = gdal.VSIStatL(cog_filename).size

        handler = webserver.SequentialHandler()
        handler.add("HEAD", "/cog.tif", 200, {"Content-Length": "%d" % filesize})
        handler.add(
            "GET",
            "/cog.tif",
            206,
            {"Content-Length": "16384"},
            extract(0, 16384),
            expected_headers={"Range": "bytes=0-16383"},
        )
        with webserver.install_http_handler(handler):
            ds = gdal.Open("/vsicurl/http://localhost:%d/cog.tif" % webserver_port)

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

        handler.add("GET", "/cog.tif", custom_method=method)
        with webserver.install_http_handler(handler):
            ret = ds.ReadRaster()
        assert ret == src_ds.ReadRaster()

    finally:
        webserver.server_stop(webserver_process, webserver_port)

        gdal.VSICurlClearCache()


###############################################################################


@pytest.mark.require_creation_option("COG", "JPEG")
@gdaltest.enable_exceptions()
def test_cog_write_interleave_tile_jpeg(tmp_vsimem):
    out_filename = str(tmp_vsimem / "out.tif")

    gdal.GetDriverByName("GTiff").CreateCopy(
        out_filename,
        gdal.Open("data/rgbsmall.tif"),
        options=["INTERLEAVE=BAND", "COMPRESS=JPEG"],
    )
    with gdal.Open(out_filename) as ds:
        expected_md = [ds.GetRasterBand(band + 1).Checksum() for band in range(3)]

    gdal.GetDriverByName("COG").CreateCopy(
        out_filename,
        gdal.Open("data/rgbsmall.tif"),
        options=["INTERLEAVE=TILE", "COMPRESS=JPEG"],
    )
    with gdal.Open(out_filename) as ds:
        got_md = [ds.GetRasterBand(band + 1).Checksum() for band in range(3)]

    assert got_md == expected_md


###############################################################################


@pytest.mark.require_creation_option("COG", "WEBP")
@gdaltest.enable_exceptions()
def test_cog_write_interleave_tile_webp_error(tmp_vsimem):
    out_filename = str(tmp_vsimem / "out.tif")

    with pytest.raises(
        Exception, match="COMPRESS=WEBP only supported for INTERLEAVE=PIXEL"
    ):
        gdal.GetDriverByName("COG").CreateCopy(
            out_filename,
            gdal.Open("data/rgbsmall.tif"),
            options=["INTERLEAVE=TILE", "COMPRESS=WEBP"],
        )


###############################################################################


@gdaltest.enable_exceptions()
def test_cog_write_complex(tmp_vsimem):

    gdal.Translate(
        tmp_vsimem / "out.tif", "data/cfloat32.tif", format="COG", width=1024
    )

    with gdal.Open(tmp_vsimem / "out.tif") as src_ds:
        assert src_ds.GetRasterBand(1).GetOverviewCount() == 1
        assert src_ds.GetRasterBand(1).GetOverview(0).Checksum() != 0


###############################################################################


@gdaltest.enable_exceptions()
def test_cog_create(tmp_vsimem):

    ds = gdal.GetDriverByName("COG").Create(
        tmp_vsimem / "out.tif", 2, 1, 3, options=["COMPRESS=LZW", "PREDICTOR=YES"]
    )
    assert ds.GetDriver().ShortName == "COG"
    assert ds.RasterXSize == 2
    assert ds.RasterYSize == 1
    assert ds.RasterCount == 3
    assert ds.GetCloseReportsProgress()
    ds.GetRasterBand(1).Fill(1)

    def my_progress(pct, msg, tab_pct):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    tab_pct = [0]
    assert ds.Close(callback=my_progress, callback_data=tab_pct) == gdal.CE_None

    assert tab_pct[0] == 1.0

    with gdal.Open(tmp_vsimem / "out.tif") as src_ds:
        assert src_ds.RasterXSize == 2
        assert src_ds.RasterYSize == 1
        assert src_ds.RasterCount == 3
        assert src_ds.GetRasterBand(1).Checksum() == 2
        assert src_ds.GetMetadataItem("LAYOUT", "IMAGE_STRUCTURE") == "COG"
        assert src_ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"
        assert src_ds.GetMetadataItem("PREDICTOR", "IMAGE_STRUCTURE") == "2"

    with pytest.raises(Exception, match="Attempt to create 0x0 dataset is illegal"):
        gdal.GetDriverByName("COG").Create(tmp_vsimem / "out.tif", 0, 0)
