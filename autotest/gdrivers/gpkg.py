#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GeoPackage raster functionality.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import sys

import pytest
from test_py_scripts import samples_path

# Make sure we run from the directory of the script
if os.path.basename(sys.argv[0]) == os.path.basename(__file__):
    if os.path.dirname(sys.argv[0]) != "":
        os.chdir(os.path.dirname(sys.argv[0]))

import gdaltest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("GPKG")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def setup_and_cleanup():
    options = {
        # This is to speed-up the runtime of tests on EXT4 filesystems
        # Do not use this for production environment if you care about data safety
        # w.r.t system/OS crashes, unless you know what you are doing.
        "OGR_SQLITE_SYNCHRONOUS": "OFF",
        "GPKG_DEBUG": "ON",
    }

    gdaltest.gpkg_dr = gdal.GetDriverByName("GPKG")
    gdaltest.png_dr = gdal.GetDriverByName("PNG")
    gdaltest.jpeg_dr = gdal.GetDriverByName("JPEG")
    gdaltest.webp_dr = gdal.GetDriverByName("WEBP")
    gdaltest.webp_supports_rgba = False
    if (
        gdaltest.webp_dr is not None
        and gdal.GetConfigOption("GPKG_SIMUL_WEBP_3BAND") is None
    ):
        md = gdaltest.webp_dr.GetMetadata()
        if md["DMD_CREATIONOPTIONLIST"].find("LOSSLESS") >= 0:
            gdaltest.webp_supports_rgba = True

    with gdaltest.disable_exceptions(), gdal.config_options(options):
        yield

        gdal.Unlink("/vsimem/tmp.gpkg")
        gdal.Unlink("/vsimem/tmp.gpkg.aux.xml")


###############################################################################
# Validate a geopackage


def validate(filename, quiet=False):

    path = samples_path
    if path not in sys.path:
        sys.path.append(path)
    try:
        import validate_gpkg
    except ImportError:
        print("Cannot import validate_gpkg")
        return True

    my_filename = filename
    if my_filename.startswith("/vsimem/"):
        my_filename = "tmp/validate.gpkg"
        f = gdal.VSIFOpenL(filename, "rb")
        if f is None:
            print("Cannot open %s" % filename)
            return False
        content = gdal.VSIFReadL(1, 10000000, f)
        gdal.VSIFCloseL(f)
        open(my_filename, "wb").write(content)
    try:
        validate_gpkg.check(my_filename, extra_checks=True, warning_as_error=True)
    except Exception as e:
        if not quiet:
            print(e)
        return False
    finally:
        if my_filename != filename:
            os.unlink(my_filename)
    return True


###############################################################################
#


def get_expected_checksums(
    src_ds, tile_drv, working_bands, extend_src=True, clamp_output=True
):
    if extend_src:
        mem_ds = gdal.GetDriverByName("MEM").Create("", 256, 256, working_bands)
    else:
        mem_ds = gdal.GetDriverByName("MEM").Create(
            "", src_ds.RasterXSize, src_ds.RasterYSize, working_bands
        )
    for i in range(working_bands):
        if src_ds.RasterCount == 2 and working_bands == 3:
            src_band = 1
        elif src_ds.RasterCount == 2 and working_bands == 4:
            if i < 3:
                src_band = 1
            else:
                src_band = 2
        elif src_ds.RasterCount == 1:
            src_band = 1
        else:
            src_band = i + 1
        data = src_ds.GetRasterBand(src_band).ReadRaster(
            0, 0, src_ds.RasterXSize, src_ds.RasterYSize
        )
        mem_ds.GetRasterBand(i + 1).WriteRaster(
            0, 0, src_ds.RasterXSize, src_ds.RasterYSize, data
        )
    if tile_drv.ShortName == "PNG":
        options = []
    else:
        options = ["QUALITY=75"]
    tmp_ds = tile_drv.CreateCopy(
        "/vsimem/tmp." + tile_drv.ShortName, mem_ds, options=options
    )
    if clamp_output:
        mem_ds = gdal.GetDriverByName("MEM").Create(
            "", src_ds.RasterXSize, src_ds.RasterYSize, working_bands
        )
        mem_ds.WriteRaster(
            0,
            0,
            src_ds.RasterXSize,
            src_ds.RasterYSize,
            tmp_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize),
        )
        expected_cs = [
            mem_ds.GetRasterBand(i + 1).Checksum() for i in range(working_bands)
        ]
    else:
        tmp_ds.FlushCache()
        expected_cs = [
            tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(working_bands)
        ]
    mem_ds = None
    tmp_ds = None
    gdal.Unlink("/vsimem/tmp." + tile_drv.ShortName)
    return expected_cs


###############################################################################
#


def check_tile_format(
    out_ds,
    expected_format,
    expected_band_count,
    expected_ct,
    row=0,
    col=0,
    zoom_level=None,
):
    if zoom_level is None:
        zoom_level_str = "(SELECT MAX(zoom_level) FROM tmp)"
    else:
        zoom_level_str = str(zoom_level)
    sql_lyr = out_ds.ExecuteSQL(
        "SELECT GDAL_GetMimeType(tile_data), "
        + "GDAL_GetBandCount(tile_data), "
        + "GDAL_HasColorTable(tile_data) FROM tmp "
        + "WHERE zoom_level = %s AND tile_column = %d AND tile_row = %d"
        % (zoom_level_str, col, row)
    )
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        mime_type = feat.GetField(0)
        band_count = feat.GetField(1)
        has_ct = feat.GetField(2)
    else:
        mime_type = None
        band_count = None
        has_ct = None
    out_ds.ReleaseResultSet(sql_lyr)
    out_ds = None

    if expected_format is None:
        if mime_type is None:
            return
        pytest.fail()

    if expected_format == "PNG":
        expected_mime_type = "image/png"
    elif expected_format == "JPEG":
        expected_mime_type = "image/jpeg"
    elif expected_format == "WEBP":
        expected_mime_type = "image/x-webp"

    assert mime_type == expected_mime_type
    assert band_count == expected_band_count
    assert expected_ct == has_ct


###############################################################################
# Single band, PNG


@pytest.mark.require_driver("PNG")
def test_gpkg_1():

    gdal.Unlink("/vsimem/tmp.gpkg")

    # With padding
    ds = gdal.Open("data/byte.tif")
    expected_cs = ds.GetRasterBand(1).Checksum()
    clamped_expected_cs = get_expected_checksums(
        ds, gdaltest.png_dr, 1, clamp_output=False
    )[0]
    expected_gt = ds.GetGeoTransform()
    expected_wkt = ds.GetProjectionRef()
    gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["TILE_FORMAT=PNG", "METADATA_TABLES=NO"]
    )
    ds = None

    assert validate("/vsimem/tmp.gpkg"), "validation failed"

    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg")
    assert out_ds.RasterCount == 4

    got_gt = out_ds.GetGeoTransform()
    for i in range(6):
        assert expected_gt[i] == pytest.approx(got_gt[i], abs=1e-8)
    got_wkt = out_ds.GetProjectionRef()
    assert expected_wkt == got_wkt
    expected_cs = [expected_cs, expected_cs, expected_cs, 4873]
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    check_tile_format(out_ds, "PNG", 2, False)

    # Check that there's no extensions
    out_ds = gdal.Open("/vsimem/tmp.gpkg")

    sql_lyr = out_ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE type = 'table' AND name = 'gpkg_extensions'"
    )
    assert sql_lyr.GetFeatureCount() == 0
    out_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys")
    assert sql_lyr.GetLayerDefn().GetFieldIndex("definition_12_063") < 0
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds = None

    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=3"])
    expected_cs = expected_cs[0:3]
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(out_ds.RasterCount)]
    assert got_cs == expected_cs
    out_ds = None

    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4", "USE_TILE_EXTENT=YES"]
    )
    assert ds.RasterXSize == 256 and ds.RasterYSize == 256
    expected_cs = [clamped_expected_cs, clamped_expected_cs, clamped_expected_cs, 4898]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    ds = None

    # Test USE_TILE_EXTENT=YES with empty table
    ds = gdal.OpenEx("/vsimem/tmp.gpkg", gdal.OF_UPDATE)
    ds.ExecuteSQL("DELETE FROM tmp")
    ds = None
    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg",
        gdal.OF_RASTER,
        open_options=["BAND_COUNT=4", "USE_TILE_EXTENT=YES"],
    )
    assert ds is None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding
    ds = gdal.Open("data/byte.tif")
    expected_cs = ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["TILE_FORMAT=PNG", "BLOCKSIZE=20"]
    )
    out_ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    assert out_ds.RasterCount == 1
    out_ds = None

    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    expected_cs = [expected_cs, expected_cs, expected_cs, 4873]
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    check_tile_format(out_ds, "PNG", 1, False)
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Single band, JPEG


@pytest.mark.require_driver("JPEG")
def test_gpkg_2():

    gdal.Unlink("/vsimem/tmp.gpkg")

    # With padding
    ds = gdal.Open("data/byte.tif")
    expected_cs = get_expected_checksums(ds, gdaltest.jpeg_dr, 1)[0]
    clamped_expected_cs = get_expected_checksums(
        ds, gdaltest.jpeg_dr, 3, clamp_output=False
    )
    clamped_expected_cs.append(17849)

    gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["TILE_FORMAT=JPEG", "METADATA_TABLES=NO"]
    )

    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    expected_cs = [expected_cs, expected_cs, expected_cs, 4873]
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    check_tile_format(out_ds, "JPEG", 1, False)

    # Check that there's no extensions
    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    sql_lyr = out_ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE type = 'table' AND name = 'gpkg_extensions'"
    )
    assert sql_lyr.GetFeatureCount() == 0
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds = None

    ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["USE_TILE_EXTENT=YES"])
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == clamped_expected_cs
    ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding
    ds = gdal.Open("data/byte.tif")
    expected_cs = get_expected_checksums(ds, gdaltest.jpeg_dr, 1, extend_src=False)[0]
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["TILE_FORMAT=JPEG", "BLOCKSIZE=20"]
    )
    out_ds = None

    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    expected_cs = [expected_cs, expected_cs, expected_cs, 4873]
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    check_tile_format(out_ds, "JPEG", 1, False)

    # Try deregistering JPEG driver
    gdaltest.jpeg_dr.Deregister()

    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    # Should give warning at pixel reading time
    gdal.ErrorReset()
    with gdal.quiet_errors():
        out_ds.GetRasterBand(1).Checksum()
    assert gdal.GetLastErrorMsg() != ""
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    ds = gdal.Open("data/byte.tif")
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["TILE_FORMAT=JPEG"]
    )
    gdal.ErrorReset()
    with gdal.quiet_errors():
        out_ds.FlushCache()
    assert gdal.GetLastErrorMsg() != ""
    out_ds = None

    # Re-register driver
    gdaltest.jpeg_dr.Register()

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Single band, WEBP


@pytest.mark.require_driver("WEBP")
def test_gpkg_3():

    gdal.Unlink("/vsimem/tmp.gpkg")

    ds = gdal.Open("data/byte.tif")
    expected_cs = get_expected_checksums(ds, gdaltest.webp_dr, 3)
    clamped_expected_cs = get_expected_checksums(
        ds, gdaltest.webp_dr, 3, clamp_output=False
    )
    if gdaltest.webp_supports_rgba:
        clamped_expected_cs.append(4898)
    else:
        clamped_expected_cs.append(17849)

    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["TILE_FORMAT=WEBP"]
    )
    out_ds = None

    assert validate("/vsimem/tmp.gpkg"), "validation failed"

    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert got_cs in (expected_cs, [4736, 4734, 4736])

    # Check that extension is declared
    sql_lyr = out_ds.ExecuteSQL(
        "SELECT * FROM gpkg_extensions WHERE table_name = 'tmp' AND column_name = 'tile_data' AND extension_name = 'gpkg_webp'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    out_ds.ReleaseResultSet(sql_lyr)

    if gdaltest.webp_supports_rgba:
        expected_band_count = 4
    else:
        expected_band_count = 3
    check_tile_format(out_ds, "WEBP", expected_band_count, False)

    out_ds = None

    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4", "USE_TILE_EXTENT=YES"]
    )
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs in (clamped_expected_cs, [6850, 6848, 6850, 4898])
    ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding
    ds = gdal.Open("data/byte.tif")
    expected_cs = get_expected_checksums(ds, gdaltest.webp_dr, 3, extend_src=False)
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["TILE_FORMAT=WEBP", "BLOCKSIZE=20"]
    )
    out_ds = None

    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    expected_cs.append(4873)
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    check_tile_format(out_ds, "WEBP", 3, False)

    # Try deregistering WEBP driver
    gdaltest.webp_dr.Deregister()

    # Should give warning at open time since the webp extension is declared
    gdal.ErrorReset()
    with gdal.quiet_errors():
        out_ds = gdal.Open("/vsimem/tmp.gpkg")
    if gdal.GetLastErrorMsg() == "":
        gdaltest.webp_dr.Register()
        pytest.fail()

    # And at pixel reading time as well
    gdal.ErrorReset()
    with gdal.quiet_errors():
        out_ds.GetRasterBand(1).Checksum()
    if gdal.GetLastErrorMsg() == "":
        gdaltest.webp_dr.Register()
        pytest.fail()
    out_ds = None

    # Re-register driver
    gdaltest.webp_dr.Register()

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Check updating a non-WEBP dataset with TILE_FORMAT=WEBP
    out_ds = gdaltest.gpkg_dr.Create("/vsimem/tmp.gpkg", 1, 1)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    out_ds = None

    out_ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg",
        gdal.OF_RASTER | gdal.OF_UPDATE,
        open_options=["TILE_FORMAT=WEBP"],
    )
    sql_lyr = out_ds.ExecuteSQL(
        "SELECT * FROM gpkg_extensions WHERE table_name = 'tmp' AND column_name = 'tile_data' AND extension_name = 'gpkg_webp'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    out_ds.ReleaseResultSet(sql_lyr)
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Three band


@pytest.mark.parametrize("tile_drv_name", ["PNG", "JPEG", "WEBP"])
def test_gpkg_4(tile_drv_name):

    if tile_drv_name == "PNG":
        tile_drv = gdaltest.png_dr
        working_bands = 4
    elif tile_drv_name == "JPEG":
        tile_drv = gdaltest.jpeg_dr
        working_bands = 3
    elif tile_drv_name == "WEBP":
        tile_drv = gdaltest.webp_dr
        if gdaltest.webp_supports_rgba:
            working_bands = 4
        else:
            working_bands = 3
    if tile_drv is None:
        pytest.skip(f"Driver {tile_drv} not available.")

    gdal.Unlink("/vsimem/tmp.gpkg")

    ds = gdal.Open("data/rgbsmall.tif")
    expected_cs = get_expected_checksums(ds, tile_drv, 3)
    clamped_expected_cs = get_expected_checksums(ds, tile_drv, 3, clamp_output=False)
    if working_bands == 3:
        clamped_expected_cs.append(17849)
    else:
        clamped_expected_cs.append(30638)

    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["TILE_FORMAT=" + tile_drv_name]
    )
    ds = None
    out_ds = None

    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    expected_cs.append(30658)
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs in (
        expected_cs,
        [22290, 21651, 21551, 30658],
        [22286, 21645, 21764, 30658],  # libwebp 1.0.3
    )
    check_tile_format(out_ds, tile_drv_name, working_bands, False)
    out_ds = None

    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4", "USE_TILE_EXTENT=YES"]
    )
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs in (
        clamped_expected_cs,
        [56886, 43228, 56508, 30638],
        [30478, 31718, 31360, 30638],  # libwebp 1.0.3
    )
    ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding
    ds = gdal.Open("data/rgbsmall.tif")
    expected_cs = get_expected_checksums(ds, tile_drv, 3, extend_src=False)
    expected_cs.append(30658)
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["TILE_FORMAT=" + tile_drv_name, "BLOCKSIZE=50"]
    )
    out_ds = None

    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    check_tile_format(out_ds, tile_drv_name, 3, False)
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# 4 band


def get_georeferenced_rgba_ds(alpha_fully_transparent=False, alpha_fully_opaque=False):
    assert not (alpha_fully_transparent and alpha_fully_opaque)
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    tmp_ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/tmp.tif", src_ds.RasterXSize, src_ds.RasterYSize, 4
    )
    tmp_ds.SetGeoTransform([0, 10, 0, 0, 0, -10])
    tmp_ds.WriteRaster(
        0,
        0,
        src_ds.RasterXSize,
        src_ds.RasterYSize,
        src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize),
    )
    if alpha_fully_opaque:
        tmp_ds.GetRasterBand(4).Fill(255)
    elif alpha_fully_transparent:
        tmp_ds.GetRasterBand(4).Fill(0)
    return tmp_ds


@pytest.mark.parametrize("tile_drv_name", ["PNG", "JPEG", "WEBP"])
def test_gpkg_7(tile_drv_name):

    if gdaltest.gpkg_dr is None:
        pytest.skip()
    if tile_drv_name == "PNG":
        tile_drv = gdaltest.png_dr
        working_bands = 4
    elif tile_drv_name == "JPEG":
        tile_drv = gdaltest.jpeg_dr
        working_bands = 3
    elif tile_drv_name == "WEBP":
        tile_drv = gdaltest.webp_dr
        if gdaltest.webp_supports_rgba:
            working_bands = 4
        else:
            working_bands = 3
    if tile_drv is None:
        pytest.skip(f"Driver {tile_drv} not available.")

    gdal.Unlink("/vsimem/tmp.gpkg")

    src_ds = get_georeferenced_rgba_ds()
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", src_ds, options=["TILE_FORMAT=" + tile_drv_name]
    )
    out_ds = None

    expected_cs = get_expected_checksums(src_ds, tile_drv, working_bands)

    src_filename = src_ds.GetDescription()
    src_ds = None
    gdal.Unlink(src_filename)

    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(working_bands)]
    assert got_cs == expected_cs
    check_tile_format(out_ds, tile_drv_name, working_bands, False)
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding with alpha fully opaque
    tmp_ds = get_georeferenced_rgba_ds(alpha_fully_opaque=True)
    expected_cs = get_expected_checksums(tmp_ds, tile_drv, 3, extend_src=False)
    tmp_filename = tmp_ds.GetDescription()
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg",
        tmp_ds,
        options=[
            "TILE_FORMAT=" + tile_drv_name,
            "BLOCKXSIZE=%d" % tmp_ds.RasterXSize,
            "BLOCKYSIZE=%d" % tmp_ds.RasterYSize,
        ],
    )
    out_ds = None
    tmp_ds = None
    gdal.Unlink(tmp_filename)

    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert got_cs == expected_cs
    check_tile_format(out_ds, tile_drv_name, 3, False)
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding with alpha fully transparent
    tmp_ds = get_georeferenced_rgba_ds(alpha_fully_transparent=True)
    tmp_filename = tmp_ds.GetDescription()
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg",
        tmp_ds,
        options=[
            "TILE_FORMAT=" + tile_drv_name,
            "BLOCKXSIZE=%d" % tmp_ds.RasterXSize,
            "BLOCKYSIZE=%d" % tmp_ds.RasterYSize,
        ],
    )
    out_ds = None
    tmp_ds = None
    gdal.Unlink(tmp_filename)

    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    expected_cs = [0, 0, 0, 0]
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    check_tile_format(out_ds, None, None, None)
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
#


def get_georeferenced_ds_with_pct32():
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba_pct32.png")
    tmp_ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/tmp.tif", src_ds.RasterXSize, src_ds.RasterYSize
    )
    tmp_ds.SetGeoTransform([0, 10, 0, 0, 0, -10])
    tmp_ds.GetRasterBand(1).SetColorTable(src_ds.GetRasterBand(1).GetColorTable())
    tmp_ds.WriteRaster(
        0,
        0,
        src_ds.RasterXSize,
        src_ds.RasterYSize,
        src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize),
    )
    return tmp_ds


###############################################################################
# Single band with 32 bit color table, PNG


@pytest.mark.require_driver("PNG")
def test_gpkg_10():

    gdal.Unlink("/vsimem/tmp.gpkg")

    tmp_ds = get_georeferenced_ds_with_pct32()
    expected_ct = tmp_ds.GetRasterBand(1).GetColorTable().Clone()
    expected_cs_single_band = tmp_ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg",
        tmp_ds,
        options=[
            "BLOCKXSIZE=%d" % tmp_ds.RasterXSize,
            "BLOCKYSIZE=%d" % tmp_ds.RasterYSize,
        ],
    )
    out_ds = None
    tmp_filename = tmp_ds.GetDescription()
    tmp_ds = None
    gdal.Unlink(tmp_filename)

    expected_cs = [10991, 57677, 34965, 10638]
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    block_size = out_ds.GetRasterBand(1).GetBlockSize()
    assert block_size == [out_ds.RasterXSize, out_ds.RasterYSize]
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    check_tile_format(out_ds, "PNG", 1, True)
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    assert got_ct is None

    # SetColorTable() on a non single-band dataset
    gdal.ErrorReset()
    with gdal.quiet_errors():
        out_ds.GetRasterBand(1).SetColorTable(None)
    assert gdal.GetLastErrorMsg() != ""

    out_ds = None

    expected_cs = [expected_cs_single_band]
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg")
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(out_ds.RasterCount)]
    assert got_cs == expected_cs
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    assert expected_ct.GetCount() == got_ct.GetCount()
    assert expected_ct.GetColorEntry(0) == got_ct.GetColorEntry(0)

    # SetColorTable() on a re-opened dataset
    gdal.ErrorReset()
    with gdal.quiet_errors():
        out_ds.GetRasterBand(1).SetColorTable(None)
    assert gdal.GetLastErrorMsg() != ""

    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Repeated SetColorTable()
    out_ds = gdaltest.gpkg_dr.Create("/vsimem/tmp.gpkg", 1, 1)
    out_ds.GetRasterBand(1).SetColorTable(None)

    gdal.ErrorReset()
    with gdal.quiet_errors():
        out_ds.GetRasterBand(1).SetColorTable(None)
    assert gdal.GetLastErrorMsg() != ""

    with gdal.quiet_errors():
        out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Partial tile
    tmp_ds = get_georeferenced_ds_with_pct32()
    expected_ct = tmp_ds.GetRasterBand(1).GetColorTable().Clone()
    expected_cs_single_band = tmp_ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy("/vsimem/tmp.gpkg", tmp_ds)
    out_ds = None
    tmp_filename = tmp_ds.GetDescription()
    tmp_ds = None
    gdal.Unlink(tmp_filename)

    expected_cs = [10991, 57677, 34965, 10638]
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    check_tile_format(out_ds, "PNG", 4, False)
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    assert got_ct is None


###############################################################################
# Single band with 32 bit color table


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
@pytest.mark.parametrize("tile_drv_name", ["JPEG", "WEBP"])
def test_gpkg_11(tile_drv_name):

    if gdaltest.gpkg_dr is None:
        pytest.skip()
    if tile_drv_name == "JPEG":
        tile_drv = gdaltest.jpeg_dr
        working_bands = 3
    elif tile_drv_name == "WEBP":
        tile_drv = gdaltest.webp_dr
        if gdaltest.webp_supports_rgba:
            working_bands = 4
        else:
            working_bands = 3
    if tile_drv is None:
        pytest.skip(f"Driver {tile_drv} not available.")

    gdal.Unlink("/vsimem/tmp.gpkg")

    rgba_xml = '<VRTDataset rasterXSize="162" rasterYSize="150">'
    for i in range(4):
        rgba_xml += """<VRTRasterBand dataType="Byte" band="%d">
    <ComplexSource>
      <SourceFilename relativeToVRT="0">../gcore/data/stefan_full_rgba_pct32.png</SourceFilename>
      <SourceBand>1</SourceBand>
      <ColorTableComponent>%d</ColorTableComponent>
    </ComplexSource>
  </VRTRasterBand>""" % (
            i + 1,
            i + 1,
        )
    rgba_xml += "</VRTDataset>"
    rgba_ds = gdal.Open(rgba_xml)

    tmp_ds = get_georeferenced_ds_with_pct32()
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", tmp_ds, options=["TILE_FORMAT=" + tile_drv_name]
    )
    out_ds = None
    tmp_filename = tmp_ds.GetDescription()
    tmp_ds = None
    gdal.Unlink(tmp_filename)

    expected_cs = get_expected_checksums(rgba_ds, tile_drv, working_bands)
    rgba_ds = None

    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(working_bands)]
    assert got_cs == expected_cs
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Single band with 24 bit color table, PNG


@pytest.mark.require_driver("PNG")
def test_gpkg_13():

    gdal.Unlink("/vsimem/tmp.gpkg")

    src_ds = gdal.Open("data/small_world_pct.tif")
    expected_ct = src_ds.GetRasterBand(1).GetColorTable().Clone()
    expected_cs_single_band = src_ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg",
        src_ds,
        options=[
            "BLOCKXSIZE=%d" % src_ds.RasterXSize,
            "BLOCKYSIZE=%d" % src_ds.RasterYSize,
        ],
    )
    out_ds = None
    src_ds = None

    expected_cs = [63025, 48175, 12204]
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert got_cs == expected_cs
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    assert got_ct is None
    out_ds = None

    expected_cs = [expected_cs_single_band]
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg")
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(out_ds.RasterCount)]
    assert got_cs == expected_cs
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    assert expected_ct.GetCount() == got_ct.GetCount()
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Partial tile
    src_ds = gdal.Open("data/small_world_pct.tif")
    expected_ct = src_ds.GetRasterBand(1).GetColorTable().Clone()
    expected_cs_single_band = src_ds.GetRasterBand(1).Checksum()
    out_ds = gdaltest.gpkg_dr.CreateCopy("/vsimem/tmp.gpkg", src_ds)
    out_ds = None
    src_ds = None

    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4", "USE_TILE_EXTENT=YES"]
    )
    assert ds.RasterXSize == 512 and ds.RasterYSize == 256
    expected_cs = [62358, 45823, 12238, 64301]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test creation and opening options


@pytest.mark.require_driver("PNG")
def test_gpkg_14():

    gdal.Unlink("/vsimem/tmp.gpkg")

    src_ds = gdal.Open("data/small_world.tif")
    ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg",
        src_ds,
        options=[
            "TILE_FORMAT=PNG",
            "RASTER_TABLE=foo",
            "RASTER_IDENTIFIER=bar",
            "RASTER_DESCRIPTION=baz",
        ],
    )
    ds = None

    with gdal.quiet_errors():
        ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["TABLE=non_existing"])
    assert ds is None

    ds = gdal.Open("/vsimem/tmp.gpkg")
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents WHERE table_name='foo'")
    feat_count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert feat_count == 1
    assert ds.GetMetadataItem("IDENTIFIER") == "bar", ds.GetMetadata()
    assert ds.GetMetadataItem("DESCRIPTION") == "baz", ds.GetMetadata()
    assert ds.GetMetadataItem("ZOOM_LEVEL") == "1", ds.GetMetadata()
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetRasterBand(1).GetOverview(0) is None
    ds = None

    # In update mode, we expose even empty overview levels
    ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)
    assert ds.GetMetadataItem("ZOOM_LEVEL") == "1", ds.GetMetadata()
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0) is not None
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 0
    ds = None

    ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["ZOOM_LEVEL=2"])
    assert ds.RasterXSize == 400
    ds = None

    ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["ZOOM_LEVEL=1"])
    assert ds.RasterXSize == 400
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    ds = None

    # In update mode, we expose even empty overview levels
    ds = gdal.OpenEx("/vsimem/tmp.gpkg", gdal.OF_UPDATE, open_options=["ZOOM_LEVEL=1"])
    assert ds.RasterXSize == 400
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    ds = None

    ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["ZOOM_LEVEL=0"])
    assert ds.RasterXSize == 200
    assert ds.GetRasterBand(1).Checksum() == 0
    ds = None

    gdal.Translate("/vsimem/tmp2.gpkg", "data/byte.tif", format="GPKG")
    ds = gdal.OpenEx("/vsimem/tmp2.gpkg", gdal.OF_UPDATE)
    ds.ExecuteSQL("UPDATE gpkg_contents SET min_x = NULL")
    ds = None
    with gdal.quiet_errors():
        ds = gdal.OpenEx("/vsimem/tmp2.gpkg", open_options=["ZOOM_LEVEL=-1"])
    assert ds is None
    gdal.Unlink("/vsimem/tmp2.gpkg")

    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4", "USE_TILE_EXTENT=YES"]
    )
    assert ds.RasterXSize == 512 and ds.RasterYSize == 256
    expected_cs = [27644, 31968, 38564, 64301]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    ds = None

    # Open with exactly one tile shift
    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg",
        gdal.OF_UPDATE,
        open_options=["BAND_COUNT=4", "TILE_FORMAT=PNG", "MINX=-410.4", "MAXY=320.4"],
    )
    assert ds.RasterXSize == 400 + 256 and ds.RasterYSize == 200 + 256
    expected_cs = [29070, 32796, 41086, 64288]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    for i in range(ds.RasterCount):
        ds.GetRasterBand(i + 1).Fill(0)
    ds.FlushCache()
    sql_lyr = ds.ExecuteSQL("SELECT * FROM foo")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 0
    ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data)
    ds = None

    # Partial tile shift (enclosing tiles)
    ds = gdal.OpenEx(
        "GPKG:/vsimem/tmp.gpkg:foo",
        gdal.OF_UPDATE,
        open_options=["BAND_COUNT=4", "MINX=-270", "MAXY=180", "MINY=-180", "MAXX=270"],
    )
    assert ds.RasterXSize == 600 and ds.RasterYSize == 400
    expected_cs = [28940, 32454, 40526, 64323]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs

    # Force full rewrite
    data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    # Do a clean just to be sure
    for i in range(ds.RasterCount):
        ds.GetRasterBand(i + 1).Fill(0)
    ds.FlushCache()
    sql_lyr = ds.ExecuteSQL("SELECT * FROM foo")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 0
    ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data)
    ds = None

    ds = gdal.OpenEx(
        "GPKG:/vsimem/tmp.gpkg:foo",
        gdal.OF_UPDATE,
        open_options=["BAND_COUNT=4", "MINX=-270", "MAXY=180", "MINY=-180", "MAXX=270"],
    )
    expected_cs = [28940, 32454, 40526, 64323]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    # Partial rewrite
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 256, 256)
    ds.GetRasterBand(1).WriteRaster(0, 0, 256, 256, data)
    ds = None

    ds = gdal.OpenEx(
        "GPKG:/vsimem/tmp.gpkg:foo",
        open_options=["BAND_COUNT=4", "MINX=-270", "MAXY=180", "MINY=-180", "MAXX=270"],
    )
    expected_cs = [28940, 32454, 40526, 64323]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    ds = None

    # Partial tile shift (included in tiles)
    ds = gdal.OpenEx(
        "GPKG:/vsimem/tmp.gpkg:foo",
        gdal.OF_UPDATE,
        open_options=["BAND_COUNT=4", "MINX=-90", "MAXY=45", "MINY=-45", "MAXX=90"],
    )
    assert ds.RasterXSize == 200 and ds.RasterYSize == 100
    expected_cs = [9586, 9360, 26758, 48827]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs

    # Force full rewrite
    data = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize)
    ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, data)
    ds = None

    ds = gdal.OpenEx(
        "GPKG:/vsimem/tmp.gpkg:foo",
        open_options=["BAND_COUNT=4", "MINX=-90", "MAXY=45", "MINY=-45", "MAXX=90"],
    )
    assert ds.RasterXSize == 200 and ds.RasterYSize == 100
    expected_cs = [9586, 9360, 26758, 48827]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    ds = None

    ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg",
        src_ds,
        options=[
            "APPEND_SUBDATASET=YES",
            "RASTER_TABLE=other",
            "BLOCKSIZE=64",
            "TILE_FORMAT=PNG",
        ],
    )
    ds = None
    another_src_ds = gdal.Open("data/byte.tif")
    ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", another_src_ds, options=["APPEND_SUBDATASET=YES"]
    )
    ds = None
    another_src_ds = None

    ds = gdal.Open("/vsimem/tmp.gpkg")
    md = ds.GetMetadata("SUBDATASETS")
    assert md["SUBDATASET_1_NAME"] == "GPKG:/vsimem/tmp.gpkg:foo"
    assert md["SUBDATASET_1_DESC"] == "foo - bar"
    assert md["SUBDATASET_2_NAME"] == "GPKG:/vsimem/tmp.gpkg:other"
    assert md["SUBDATASET_2_DESC"] == "other - other"
    assert md["SUBDATASET_3_NAME"] == "GPKG:/vsimem/tmp.gpkg:byte"
    assert md["SUBDATASET_3_DESC"] == "byte - byte"
    ds = None

    ds = gdal.Open("GPKG:/vsimem/tmp.gpkg:other")
    block_size = ds.GetRasterBand(1).GetBlockSize()
    assert block_size == [64, 64]
    ds = None

    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg",
        open_options=[
            "BAND_COUNT=4",
            "TABLE=other",
            "MINX=-90",
            "MAXY=45",
            "MINY=-45",
            "MAXX=90",
        ],
    )
    assert ds.RasterXSize == 200 and ds.RasterYSize == 100
    block_size = ds.GetRasterBand(1).GetBlockSize()
    assert block_size == [64, 64]
    expected_cs = [9586, 9360, 26758, 48827]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Open and fill with an area of interest larger/containing the natural extent
    ds = gdaltest.gpkg_dr.Create(
        "/vsimem/tmp.gpkg", 20, 20, 1, options=["BLOCKSIZE=20"]
    )
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds = None

    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg",
        gdal.OF_UPDATE,
        open_options=["MINX=-5", "MAXY=5", "MAXX=25", "MINY=-25", "BAND_COUNT=1"],
    )
    ds.GetRasterBand(1).Fill(255)
    ds = None

    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", open_options=["MINX=-10", "MAXY=10", "MINY=-30", "MAXX=30"]
    )
    expected_cs = [4934, 4934, 4934, 4934]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Open and fill with an area of interest smaller/inside the natural extent
    # (and smaller than the block size actually)
    ds = gdaltest.gpkg_dr.Create(
        "/vsimem/tmp.gpkg", 20, 20, 1, options=["BLOCKSIZE=20"]
    )
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds = None

    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg",
        gdal.OF_UPDATE,
        open_options=["MINX=5", "MAXY=-5", "MAXX=15", "MINY=-15", "BAND_COUNT=1"],
    )
    ds.GetRasterBand(1).Fill(255)
    ds = None

    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", open_options=["MINX=-10", "MAXY=10", "MINY=-30", "MAXX=30"]
    )
    expected_cs = [1223, 1223, 1223, 1223]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Other corner case : the block intersects a tile at the right of the raster
    # size (because the raster size is smaller than the block size)
    ds = gdaltest.gpkg_dr.Create("/vsimem/tmp.gpkg", 400, 200, 1)
    ds.SetGeoTransform([-180, 0.9, 0, 90, 0, -0.9])
    ds = None

    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg",
        gdal.OF_UPDATE,
        open_options=["MINX=-5", "MAXY=5", "MAXX=25", "MINY=-25", "BAND_COUNT=1"],
    )
    ds.GetRasterBand(1).Fill(255)
    ds = None

    ds = gdal.OpenEx("/vsimem/tmp.gpkg")
    expected_cs = [13365, 13365, 13365, 13365]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Test reading block from partial tile database
    ds = gdaltest.gpkg_dr.Create("/vsimem/tmp.gpkg", 512, 256, 4)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds = None

    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg",
        gdal.OF_UPDATE,
        open_options=["MINX=-5", "MAXY=5", "TILE_FORMAT=PNG"],
    )
    mem_ds = gdal.GetDriverByName("MEM").Create("", 256, 256)
    mem_ds.GetRasterBand(1).Fill(255)
    mem_ds.FlushCache()
    data = mem_ds.GetRasterBand(1).ReadRaster()
    mem_ds = None
    # Only write one of the tile
    ds.GetRasterBand(2).WriteRaster(0, 0, 256, 256, data)

    # "Flush" into partial tile database, but not in definitive database
    with gdaltest.SetCacheMax(0):
        pass

    sql_lyr = ds.ExecuteSQL("SELECT * FROM tmp")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 0

    expected_cs = [0, 56451, 0, 0]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    if got_cs != expected_cs:
        ds.GetRasterBand(4).Fill(255)
        # sys.exit(0)
        pytest.fail("Got %s, expected %s" % (str(got_cs), str(expected_cs)))
    ds = None

    # Overflow occurred in ComputeTileAndPixelShifts()
    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            "/vsimem/tmp.gpkg", open_options=["MINX=-1e12", "MAXX=-0.9999e12"]
        )
    assert ds is None

    # Overflow occurred in ComputeTileAndPixelShifts()
    with gdal.quiet_errors():
        ds = gdal.OpenEx(
            "/vsimem/tmp.gpkg", open_options=["MINY=-1e12", "MAXY=-0.9999e12"]
        )
    assert ds is None

    # Overflow occurred in ComputeTileAndPixelShifts()
    gdal.Translate("/vsimem/tmp.gpkg", "data/byte.tif", format="GPKG")
    ds = gdal.OpenEx("/vsimem/tmp.gpkg", gdal.OF_UPDATE)
    ds.ExecuteSQL(
        "UPDATE gpkg_contents SET min_x=-100000000002000, max_x=-100000000000000"
    )
    ds = None
    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/tmp.gpkg")
    assert ds is None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test error cases


def test_gpkg_15():

    gdal.Unlink("/vsimem/tmp.gpkg")

    # SetGeoTransform() and SetProjection() on a non-raster GPKG
    out_ds = gdaltest.gpkg_dr.Create("/vsimem/tmp.gpkg", 0, 0, 0)
    assert out_ds.GetGeoTransform(can_return_null=True) is None
    assert out_ds.GetProjectionRef() == ""
    with gdal.quiet_errors():
        ret = out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    assert ret != 0

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    with gdal.quiet_errors():
        ret = out_ds.SetProjection(srs.ExportToWkt())
    assert ret != 0
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Repeated SetGeoTransform()
    out_ds = gdaltest.gpkg_dr.Create("/vsimem/tmp.gpkg", 1, 1)
    ret = out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    assert ret == 0
    with gdal.quiet_errors():
        ret = out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    assert ret != 0
    out_ds = None

    # Repeated SetProjection()
    out_ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)
    assert out_ds.GetSpatialRef().IsLocal()
    assert out_ds.GetProjectionRef().find("Undefined Cartesian SRS") >= 0
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    ret = out_ds.SetProjection(srs.ExportToWkt())
    assert ret == 0
    assert out_ds.GetProjectionRef().find("4326") >= 0
    out_ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)
    assert out_ds.GetProjectionRef().find("4326") >= 0
    out_ds.SetProjection("")
    out_ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    assert out_ds.GetSpatialRef() is None
    # Test setting on read-only dataset
    with gdal.quiet_errors():
        ret = out_ds.SetProjection("")
    assert ret != 0
    with gdal.quiet_errors():
        ret = out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    assert ret != 0
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Test SetColorInterpretation()
    out_ds = gdaltest.gpkg_dr.Create("/vsimem/tmp.gpkg", 1, 1)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_Undefined)
    assert ret == 0
    ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_GrayIndex)
    assert ret == 0
    ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_PaletteIndex)
    assert ret == 0
    with gdal.quiet_errors():
        ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    assert ret != 0
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    out_ds = gdaltest.gpkg_dr.Create("/vsimem/tmp.gpkg", 1, 1, 3)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    assert ret == 0
    with gdal.quiet_errors():
        ret = out_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_RedBand)
    assert ret != 0
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    out_ds = gdaltest.gpkg_dr.Create("/vsimem/tmp.gpkg", 1, 1, 2)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_GrayIndex)
    assert ret == 0
    with gdal.quiet_errors():
        ret = out_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    assert ret != 0
    ret = out_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    assert ret == 0
    with gdal.quiet_errors():
        ret = out_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_RedBand)
    assert ret != 0
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test block/tile caching


@pytest.mark.require_driver("JPEG")
def test_gpkg_16():

    gdal.Unlink("/vsimem/tmp.gpkg")

    out_ds = gdaltest.gpkg_dr.Create(
        "/vsimem/tmp.gpkg", 1, 1, 3, options=["TILE_FORMAT=JPEG"]
    )
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    out_ds.GetRasterBand(1).Fill(255)
    out_ds.GetRasterBand(1).FlushCache()
    # Rewrite same tile after re-reading it
    # Will cause a debug message to be emitted
    out_ds.GetRasterBand(2).Fill(127)
    out_ds.GetRasterBand(3).Checksum()
    out_ds.GetRasterBand(2).FlushCache()
    out_ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    val1 = ord(out_ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1))
    val2 = ord(out_ds.GetRasterBand(2).ReadRaster(0, 0, 1, 1))
    val3 = ord(out_ds.GetRasterBand(3).ReadRaster(0, 0, 1, 1))
    out_ds = None

    assert val1 == pytest.approx(255, abs=1)
    assert val2 == pytest.approx(127, abs=1)
    assert val3 == pytest.approx(0, abs=1)
    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test overviews with single band dataset


@pytest.mark.require_driver("PNG")
def test_gpkg_17():

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding, immediately after create copy
    ds = gdal.Open("data/byte.tif")
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["BLOCKSIZE=10"]
    )
    out_ds.BuildOverviews("NEAR", [2])
    out_ds = None
    ds = None

    assert validate("/vsimem/tmp.gpkg"), "validation failed"

    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    assert out_ds.RasterCount == 1
    assert out_ds.GetRasterBand(1).GetOverview(0).GetDataset().RasterCount == 1
    got_cs = out_ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert got_cs == 1087
    check_tile_format(out_ds, "PNG", 1, False, zoom_level=0)
    assert out_ds.GetRasterBand(1).GetOverview(0).GetColorTable() is None
    out_ds = None
    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding, after reopening
    ds = gdal.Open("data/byte.tif")
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["BLOCKSIZE=10"]
    )
    out_ds = None
    out_ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg",
        gdal.OF_RASTER | gdal.OF_UPDATE,
    )
    out_ds.BuildOverviews("NEAR", [2])
    out_ds = None
    ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    got_cs = out_ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert got_cs == 1087
    check_tile_format(out_ds, "PNG", 1, False, zoom_level=0)
    out_ds = None
    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding, after reopening
    ds = gdal.Open("data/byte.tif")
    gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["BLOCKSIZE=10", "METADATA_TABLES=NO"]
    )
    out_ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg",
        gdal.OF_RASTER | gdal.OF_UPDATE,
        open_options=["TILE_FORMAT=PNG"],
    )
    out_ds.BuildOverviews("NEAR", [2])
    out_ds = None
    ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    got_cs = out_ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert got_cs == 1087
    check_tile_format(out_ds, "PNG", 3, False, zoom_level=0)

    # Check that there's no extensions
    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    sql_lyr = out_ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE type = 'table' AND name = 'gpkg_extensions'"
    )
    assert sql_lyr.GetFeatureCount() == 0
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds = None

    # Test clearing overviews
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", gdal.OF_RASTER | gdal.OF_UPDATE)
    out_ds.BuildOverviews("NONE", [])
    out_ds = None
    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    assert out_ds.GetRasterBand(1).GetOverviewCount() == 0
    out_ds = None

    # Test building on an overview dataset --> error
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", gdal.OF_RASTER | gdal.OF_UPDATE)
    with gdal.quiet_errors():
        ret = (
            out_ds.GetRasterBand(1)
            .GetOverview(0)
            .GetDataset()
            .BuildOverviews("NONE", [])
        )
    assert ret != 0
    out_ds = None

    # Test building overview factor 1 --> error
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", gdal.OF_RASTER | gdal.OF_UPDATE)
    with gdal.quiet_errors():
        ret = out_ds.BuildOverviews("NEAR", [1])
    assert ret != 0
    out_ds = None

    # Test building non-supported overview levels
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", gdal.OF_RASTER | gdal.OF_UPDATE)
    with gdal.config_option(
        "ALLOW_GPKG_ZOOM_OTHER_EXTENSION", "NO"
    ), gdaltest.error_handler():
        ret = out_ds.BuildOverviews("NEAR", [3])
    assert ret != 0
    out_ds = None

    # Test building non-supported overview levels
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", gdal.OF_RASTER | gdal.OF_UPDATE)
    with gdal.config_option(
        "ALLOW_GPKG_ZOOM_OTHER_EXTENSION", "NO"
    ), gdaltest.error_handler():
        ret = out_ds.BuildOverviews("NEAR", [2, 4])
    assert ret != 0
    out_ds = None

    # Test building overviews on read-only dataset
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", gdal.OF_RASTER)
    with gdal.quiet_errors():
        ret = out_ds.BuildOverviews("NEAR", [2])
    assert ret != 0
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test overviews with 3 band dataset


@pytest.mark.require_driver("PNG")
def test_gpkg_18():

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding, immediately after create copy
    ds = gdal.Open("data/small_world.tif")
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg",
        ds,
        options=["TILE_FORMAT=PNG", "BLOCKXSIZE=100", "BLOCKYSIZE=100"],
    )
    out_ds.BuildOverviews("CUBIC", [2, 4])
    out_ds = None

    tmp_ds = gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/tmp.tif", ds)
    tmp_ds.BuildOverviews("CUBIC", [2, 4])
    expected_cs_ov0 = [
        tmp_ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(3)
    ]
    expected_cs_ov1 = [
        tmp_ds.GetRasterBand(i + 1).GetOverview(1).Checksum() for i in range(3)
    ]
    # tmp_ds.BuildOverviews('NEAR', [3])
    # expected_cs_ov_factor3 = [tmp_ds.GetRasterBand(i+1).GetOverview(2).Checksum() for i in range(3)]
    tmp_ds = None
    gdal.GetDriverByName("GTiff").Delete("/vsimem/tmp.tif")

    ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    got_cs = [out_ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(3)]
    assert got_cs == expected_cs_ov0
    got_cs = [out_ds.GetRasterBand(i + 1).GetOverview(1).Checksum() for i in range(3)]
    assert got_cs == expected_cs_ov1
    check_tile_format(out_ds, "PNG", 3, False, zoom_level=1)
    check_tile_format(out_ds, "PNG", 4, False, zoom_level=0)
    out_ds = None

    # Test gpkg_zoom_other extension
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", gdal.OF_RASTER | gdal.OF_UPDATE)
    # We expect a warning
    with gdal.quiet_errors():
        ret = out_ds.BuildOverviews("NEAR", [3])
    assert ret == 0
    assert out_ds.GetRasterBand(1).GetOverviewCount() == 3
    got_cs = [out_ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(3)]
    assert got_cs == expected_cs_ov0
    expected_cs = [24807, 25544, 34002]
    got_cs = [out_ds.GetRasterBand(i + 1).GetOverview(1).Checksum() for i in range(3)]
    assert got_cs == expected_cs
    got_cs = [out_ds.GetRasterBand(i + 1).GetOverview(2).Checksum() for i in range(3)]
    assert got_cs == expected_cs_ov1

    # Check that extension is declared
    sql_lyr = out_ds.ExecuteSQL(
        "SELECT * FROM gpkg_extensions WHERE table_name = 'tmp' AND extension_name = 'gpkg_zoom_other'"
    )
    assert sql_lyr.GetFeatureCount() == 1
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    assert out_ds.GetRasterBand(1).GetOverviewCount() == 3
    got_cs = [out_ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(3)]
    assert got_cs == expected_cs_ov0
    expected_cs = [24807, 25544, 34002]
    got_cs = [out_ds.GetRasterBand(i + 1).GetOverview(1).Checksum() for i in range(3)]
    assert got_cs == expected_cs
    got_cs = [out_ds.GetRasterBand(i + 1).GetOverview(2).Checksum() for i in range(3)]
    assert got_cs == expected_cs_ov1
    out_ds = None

    # Add terminating overview
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", gdal.OF_RASTER | gdal.OF_UPDATE)
    ret = out_ds.BuildOverviews("NEAR", [8])
    assert ret == 0
    expected_cs = [12725, 12539, 13553]
    got_cs = [out_ds.GetRasterBand(i + 1).GetOverview(3).Checksum() for i in range(3)]
    assert got_cs == expected_cs
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding, immediately after create copy
    ds = gdal.Open("data/small_world.tif")
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg",
        ds,
        options=[
            "TILE_FORMAT=PNG",
            "BLOCKXSIZE=100",
            "BLOCKYSIZE=100",
            "METADATA_TABLES=NO",
        ],
    )
    # Should not result in gpkg_zoom_other
    ret = out_ds.BuildOverviews("NEAR", [8])
    assert ret == 0
    out_ds = None

    # Check that there's no extensions
    out_ds = gdal.Open("/vsimem/tmp.gpkg")
    sql_lyr = out_ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE type = 'table' AND name = 'gpkg_extensions'"
    )
    assert sql_lyr.GetFeatureCount() == 0
    out_ds.ReleaseResultSet(sql_lyr)
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test overviews with 24-bit color palette single band dataset


@pytest.mark.require_driver("PNG")
def test_gpkg_19():

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding, immediately after create copy
    ds = gdal.Open("data/small_world_pct.tif")
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg",
        ds,
        options=["TILE_FORMAT=PNG", "BLOCKXSIZE=100", "BLOCKYSIZE=100"],
    )
    out_ds.BuildOverviews("NEAR", [2, 4])
    out_ds = None

    tmp_ds = gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/tmp.tif", ds)
    tmp_ds.BuildOverviews("NEAR", [2, 4])
    expected_cs_ov0 = [
        tmp_ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(1)
    ]
    expected_cs_ov1 = [
        tmp_ds.GetRasterBand(i + 1).GetOverview(1).Checksum() for i in range(1)
    ]
    tmp_ds = None
    gdal.GetDriverByName("GTiff").Delete("/vsimem/tmp.tif")

    ds = None

    out_ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", gdal.OF_RASTER, open_options=["BAND_COUNT=1"]
    )
    assert out_ds.GetRasterBand(1).GetOverview(0).GetColorTable() is not None
    got_cs = [out_ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(1)]
    assert got_cs == expected_cs_ov0
    got_cs = [out_ds.GetRasterBand(i + 1).GetOverview(1).Checksum() for i in range(1)]
    assert got_cs == expected_cs_ov1
    check_tile_format(out_ds, "PNG", 1, True, zoom_level=1)
    check_tile_format(out_ds, "PNG", 4, False, zoom_level=0)
    out_ds = None
    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test PNG8


@pytest.mark.require_driver("PNG")
def test_gpkg_20():

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding, with small tiles (<=256x256)
    ds = gdal.Open("data/small_world.tif")
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["TILE_FORMAT=PNG8", "BLOCKSIZE=200"]
    )
    out_ds = None
    ds = None

    out_ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", gdal.OF_RASTER, open_options=["BAND_COUNT=4"]
    )
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [30875, 31451, 38110, 64269]
    assert got_cs == expected_cs
    check_tile_format(out_ds, "PNG", 1, True)
    out_ds = None
    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding, with big tiles (>256x256)
    ds = gdal.Open("data/small_world.tif")
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg",
        ds,
        options=["TILE_FORMAT=PNG8", "BLOCKXSIZE=400", "BLOCKYSIZE=200"],
    )
    out_ds = None
    ds = None

    out_ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", gdal.OF_RASTER, open_options=["BAND_COUNT=4"]
    )
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [27001, 30168, 34800, 64269]
    assert got_cs == expected_cs
    check_tile_format(out_ds, "PNG", 1, True)
    out_ds = None
    gdal.Unlink("/vsimem/tmp.gpkg")

    # With and without padding, with small tiles
    ds = gdal.Open("data/small_world.tif")
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["TILE_FORMAT=PNG8", "BLOCKSIZE=150"]
    )
    out_ds = None
    ds = None

    out_ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", gdal.OF_RASTER, open_options=["BAND_COUNT=4"]
    )
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [27718, 31528, 42062, 64269]
    assert got_cs == expected_cs
    check_tile_format(out_ds, "PNG", 1, True)
    check_tile_format(out_ds, "PNG", 4, False, row=0, col=2)
    out_ds = None
    gdal.Unlink("/vsimem/tmp.gpkg")

    # Without padding, with small tiles (<=256x256), but especially less
    # than 256 colors.
    ds = gdal.GetDriverByName("MEM").Create("", 50, 50, 3)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(2).Fill(2)
    ds.GetRasterBand(3).Fill(3)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg", ds, options=["TILE_FORMAT=PNG8", "BLOCKSIZE=50"]
    )
    out_ds = None
    ds = None

    out_ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", gdal.OF_RASTER, open_options=["BAND_COUNT=4"]
    )
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [2500, 5000, 7500, 30658]
    assert got_cs == expected_cs
    check_tile_format(out_ds, "PNG", 1, True)
    out_ds = None
    out_ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", gdal.OF_RASTER, open_options=["BAND_COUNT=1"]
    )
    assert out_ds.GetRasterBand(1).GetColorTable().GetCount() == 1
    out_ds = None
    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test metadata


@pytest.mark.require_driver("PNG")
def test_gpkg_21():

    gdal.Unlink("/vsimem/tmp.gpkg")

    out_ds = gdaltest.gpkg_dr.Create("/vsimem/tmp.gpkg", 1, 1)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    mddlist = out_ds.GetMetadataDomainList()
    assert len(mddlist) == 3
    out_ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)

    # No metadata for now
    sql_lyr = out_ds.ExecuteSQL(
        "SELECT 1 FROM sqlite_master WHERE name = 'gpkg_metadata'"
    )
    feat = sql_lyr.GetNextFeature()
    out_ds.ReleaseResultSet(sql_lyr)
    feat_is_none = feat is None
    assert feat_is_none

    # Set a metadata item now
    out_ds.SetMetadataItem("foo", "bar")
    out_ds = None

    assert gdal.VSIStatL("/vsimem/tmp.gpkg.aux.xml") is None

    foo_value = "bar"
    for i in range(4):

        out_ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)

        assert not out_ds.GetMetadata("GEOPACKAGE")
        if out_ds.GetMetadataItem("foo") != foo_value:
            feat.DumpReadable()
            pytest.fail(out_ds.GetMetadataItem("foo"))
        md = out_ds.GetMetadata()
        if (
            len(md) != 3
            or md["foo"] != foo_value
            or md["IDENTIFIER"] != "tmp"
            or md["ZOOM_LEVEL"] != "0"
        ):
            feat.DumpReadable()
            pytest.fail(md)

        sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_metadata")
        feat = sql_lyr.GetNextFeature()
        if (
            feat.GetField("id") != 1
            or feat.GetField("md_scope") != "dataset"
            or feat.GetField("md_standard_uri") != "http://gdal.org"
            or feat.GetField("mime_type") != "text/xml"
            or feat.GetField("metadata") != """<GDALMultiDomainMetadata>
  <Metadata>
    <MDI key="foo">%s</MDI>
  </Metadata>
</GDALMultiDomainMetadata>
""" % foo_value
        ):
            feat.DumpReadable()
            pytest.fail(i)
        out_ds.ReleaseResultSet(sql_lyr)

        sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_metadata_reference")
        feat = sql_lyr.GetNextFeature()
        if (
            feat.GetField("reference_scope") != "table"
            or feat.GetField("table_name") != "tmp"
            or not feat.IsFieldNull("column_name")
            or not feat.IsFieldNull("row_id_value")
            or not feat.IsFieldSet("timestamp")
            or feat.GetField("md_file_id") != 1
            or not feat.IsFieldNull("md_parent_id")
        ):
            feat.DumpReadable()
            pytest.fail(i)
        out_ds.ReleaseResultSet(sql_lyr)

        if i == 1:
            out_ds.SetMetadataItem("foo", "bar")
        elif i == 2:
            out_ds.SetMetadataItem("foo", "baz")
            foo_value = "baz"

        out_ds = None

    # Clear metadata
    out_ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)
    out_ds.SetMetadata(None)
    out_ds = None

    # No more metadata
    out_ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_metadata")
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    out_ds.ReleaseResultSet(sql_lyr)
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_metadata_reference")
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds.SetMetadataItem("IDENTIFIER", "my_identifier")
    out_ds.SetMetadataItem("DESCRIPTION", "my_description")
    out_ds = None

    # Still no metadata
    out_ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)
    assert out_ds.GetMetadataItem("IDENTIFIER") == "my_identifier"
    assert out_ds.GetMetadataItem("DESCRIPTION") == "my_description"
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_metadata")
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    out_ds.ReleaseResultSet(sql_lyr)
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_metadata_reference")
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    out_ds.ReleaseResultSet(sql_lyr)

    # Write metadata in global scope
    out_ds.SetMetadata({"bar": "foo"}, "GEOPACKAGE")

    out_ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)

    assert out_ds.GetMetadataItem("bar", "GEOPACKAGE") == "foo"

    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_metadata")
    feat = sql_lyr.GetNextFeature()
    if (
        feat.GetField("id") != 1
        or feat.GetField("md_scope") != "dataset"
        or feat.GetField("md_standard_uri") != "http://gdal.org"
        or feat.GetField("mime_type") != "text/xml"
        or feat.GetField("metadata") != """<GDALMultiDomainMetadata>
  <Metadata>
    <MDI key="bar">foo</MDI>
  </Metadata>
</GDALMultiDomainMetadata>
"""
    ):
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    out_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_metadata_reference")
    feat = sql_lyr.GetNextFeature()
    if (
        feat.GetField("reference_scope") != "geopackage"
        or not feat.IsFieldNull("table_name")
        or not feat.IsFieldNull("column_name")
        or not feat.IsFieldNull("row_id_value")
        or not feat.IsFieldSet("timestamp")
        or feat.GetField("md_file_id") != 1
        or not feat.IsFieldNull("md_parent_id")
    ):
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    out_ds.ReleaseResultSet(sql_lyr)

    out_ds.SetMetadataItem("bar", "baz", "GEOPACKAGE")
    out_ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)
    assert out_ds.GetMetadataItem("bar", "GEOPACKAGE") == "baz"
    out_ds.SetMetadata(None, "GEOPACKAGE")
    out_ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)
    assert not out_ds.GetMetadata("GEOPACKAGE")

    out_ds.SetMetadataItem("1", "2")
    out_ds.SetMetadataItem("3", "4", "CUSTOM_DOMAIN")
    out_ds.SetMetadataItem("6", "7", "GEOPACKAGE")
    # Non GDAL metadata
    out_ds.ExecuteSQL(
        "INSERT INTO gpkg_metadata VALUES (10, 'dataset', 'uri', 'text/plain', 'my_metadata')"
    )
    out_ds.ExecuteSQL(
        "INSERT INTO gpkg_metadata_reference VALUES ('geopackage',NULL,NULL,NULL,'2012-08-17T14:49:32.932Z',10,NULL)"
    )
    out_ds.ExecuteSQL(
        "INSERT INTO gpkg_metadata VALUES (11, 'dataset', 'uri', 'text/plain', 'other_metadata')"
    )
    out_ds.ExecuteSQL(
        "INSERT INTO gpkg_metadata_reference VALUES ('geopackage',NULL,NULL,NULL,'2012-08-17T14:49:32.932Z',11,NULL)"
    )
    out_ds.ExecuteSQL(
        "INSERT INTO gpkg_metadata VALUES (12, 'dataset', 'uri', 'text/plain', 'my_metadata_local')"
    )
    out_ds.ExecuteSQL(
        "INSERT INTO gpkg_metadata_reference VALUES ('table','tmp',NULL,NULL,'2012-08-17T14:49:32.932Z',12,NULL)"
    )
    out_ds.ExecuteSQL(
        "INSERT INTO gpkg_metadata VALUES (13, 'dataset', 'uri', 'text/plain', 'other_metadata_local')"
    )
    out_ds.ExecuteSQL(
        "INSERT INTO gpkg_metadata_reference VALUES ('table','tmp',NULL,NULL,'2012-08-17T14:49:32.932Z',13,NULL)"
    )
    out_ds = None

    for i in range(2):
        out_ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)
        assert out_ds.GetMetadataItem("1") == "2"
        assert (
            out_ds.GetMetadataItem("GPKG_METADATA_ITEM_1") == "my_metadata_local"
        ), out_ds.GetMetadata()
        assert (
            out_ds.GetMetadataItem("GPKG_METADATA_ITEM_2") == "other_metadata_local"
        ), out_ds.GetMetadata()
        assert (
            out_ds.GetMetadataItem("GPKG_METADATA_ITEM_1", "GEOPACKAGE")
            == "my_metadata"
        ), out_ds.GetMetadata("GEOPACKAGE")
        assert (
            out_ds.GetMetadataItem("GPKG_METADATA_ITEM_2", "GEOPACKAGE")
            == "other_metadata"
        ), out_ds.GetMetadata("GEOPACKAGE")
        assert out_ds.GetMetadataItem("3", "CUSTOM_DOMAIN") == "4"
        assert out_ds.GetMetadataItem("6", "GEOPACKAGE") == "7"
        out_ds.SetMetadata(out_ds.GetMetadata())
        out_ds.SetMetadata(out_ds.GetMetadata("GEOPACKAGE"), "GEOPACKAGE")
        out_ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)
    out_ds.SetMetadata(None)
    out_ds.SetMetadata(None, "CUSTOM_DOMAIN")
    out_ds.SetMetadata(None, "GEOPACKAGE")
    out_ds = None

    out_ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)
    assert (
        out_ds.GetMetadataItem("GPKG_METADATA_ITEM_1", "GEOPACKAGE") == "my_metadata"
    ), out_ds.GetMetadata()
    sql_lyr = out_ds.ExecuteSQL("SELECT * FROM gpkg_metadata WHERE id < 10")
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    out_ds.ReleaseResultSet(sql_lyr)
    sql_lyr = out_ds.ExecuteSQL(
        "SELECT * FROM gpkg_metadata_reference WHERE md_file_id < 10"
    )
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        feat.DumpReadable()
        out_ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    out_ds.ReleaseResultSet(sql_lyr)
    out_ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test metadata in PAM


@pytest.mark.require_driver("PNG")
def test_gpkg_metadata_PAM():

    gdal.Unlink("/vsimem/tmp.gpkg")

    out_ds = gdaltest.gpkg_dr.Create("/vsimem/tmp.gpkg", 1, 1)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    out_ds = None

    ds = gdal.Open("/vsimem/tmp.gpkg")
    ds.SetMetadataItem("foo", "bar")
    ds = None

    f = gdal.VSIFOpenL("/vsimem/tmp.gpkg.aux.xml", "rb")
    assert f
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    assert (
        data
        == b'<PAMDataset>\n  <Metadata>\n    <MDI key="foo">bar</MDI>\n  </Metadata>\n</PAMDataset>\n'
    )

    ds = gdal.Open("/vsimem/tmp.gpkg")
    assert ds.GetMetadata() == {"IDENTIFIER": "tmp", "ZOOM_LEVEL": "0", "foo": "bar"}
    ds = None

    gdaltest.gpkg_dr.Delete("/vsimem/tmp.gpkg")


###############################################################################
# Two band, PNG


def get_georeferenced_greyalpha_ds():
    src_ds = gdal.Open("../gcore/data/stefan_full_greyalpha.tif")
    tmp_ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/tmp.tif", src_ds.RasterXSize, src_ds.RasterYSize, 2
    )
    tmp_ds.SetGeoTransform([0, 10, 0, 0, 0, -10])
    tmp_ds.WriteRaster(
        0,
        0,
        src_ds.RasterXSize,
        src_ds.RasterYSize,
        src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize),
    )
    return tmp_ds


@pytest.mark.parametrize("tile_drv_name", ["PNG", "JPEG", "WEBP", None])
def test_gpkg_22(tile_drv_name):

    if tile_drv_name is None:
        tile_drv = gdaltest.png_dr
        if gdaltest.jpeg_dr is None:
            pytest.skip()
        expected_cs = [2466, 10807]
        clamped_expected_cs = [1989, 1989, 1989, 11580]
    if tile_drv_name == "PNG":
        tile_drv = gdaltest.png_dr
        expected_cs = [1970, 10807]
        clamped_expected_cs = [2100, 2100, 2100, 11580]
    elif tile_drv_name == "JPEG":
        tile_drv = gdaltest.jpeg_dr
        expected_cs = [6782, 32706]
        clamped_expected_cs = [6538, 6538, 6538, 32744]
    elif tile_drv_name == "WEBP":
        tile_drv = gdaltest.webp_dr
        if gdaltest.webp_supports_rgba:
            expected_cs = [13112, 10807]
            clamped_expected_cs = [13380, 13380, 13380, 11580]
        else:
            expected_cs = [13112, 32706]
            clamped_expected_cs = [13380, 13380, 13380, 32744]
    if tile_drv is None:
        pytest.skip(f"Driver {tile_drv} not available.")

    gdal.Unlink("/vsimem/tmp.gpkg")

    tmp_ds = get_georeferenced_greyalpha_ds()
    if tile_drv_name:
        options = ["TILE_FORMAT=" + tile_drv_name, "BLOCKSIZE=16"]
    else:
        options = ["BLOCKSIZE=16"]
    out_ds = gdaltest.gpkg_dr.CreateCopy("/vsimem/tmp.gpkg", tmp_ds, options=options)
    tmp_ds_filename = tmp_ds.GetDescription()
    ds = None
    gdal.Unlink(tmp_ds_filename)
    out_ds = None

    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=2"])
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(2)]
    if got_cs != expected_cs:
        assert tile_drv_name == "WEBP" and got_cs in (
            [4899, 10807],
            [6274, 10807],
            [17638, 10807],
        )
    out_ds = None

    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    expected_cs = [expected_cs[0], expected_cs[0], expected_cs[0], expected_cs[1]]
    if got_cs != expected_cs:
        assert tile_drv_name == "WEBP" and got_cs in (
            [4899, 4899, 4899, 10807],
            [4899, 4984, 4899, 10807],
            [6274, 6274, 6274, 10807],
            [17638, 17631, 17638, 10807],
        )
    out_ds = None

    ds = gdal.OpenEx(
        "/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4", "USE_TILE_EXTENT=YES"]
    )
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    if got_cs != clamped_expected_cs:
        assert tile_drv_name == "WEBP" and got_cs in (
            [5266, 5266, 5266, 11580],
            [5266, 5310, 5266, 11580],
            [6436, 6436, 6436, 11580],
            [17007, 17000, 17007, 11580],
        )
    ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test TILING_SCHEME


@pytest.mark.require_driver("PNG")
@pytest.mark.parametrize(
    "scheme,expected_cs,other_options",
    [
        ("CUSTOM", [4672, 4672, 4672, 4873], None),
        ("GoogleCRS84Quad", [3439, 3439, 3439, 3691], None),
        ("GoogleCRS84Quad", [3439, 3439, 3439, 3691], ["RESAMPLING=BILINEAR"]),
        ("GoogleCRS84Quad", [3549, 3549, 3549, 3691], ["RESAMPLING=CUBIC"]),
        ("GoogleCRS84Quad", [3439, 3439, 3439, 3691], ["ZOOM_LEVEL_STRATEGY=AUTO"]),
        (
            "GoogleCRS84Quad",
            [14445, 14445, 14445, 14448],
            ["ZOOM_LEVEL_STRATEGY=UPPER"],
        ),
        (
            "GoogleCRS84Quad",
            [14445, 14445, 14445, 14448],
            ["ZOOM_LEVEL=12"],
        ),
        (
            "GoogleCRS84Quad",
            None,
            ["ZOOM_LEVEL=31"],
        ),
        ("GoogleCRS84Quad", [3439, 3439, 3439, 3691], ["ZOOM_LEVEL_STRATEGY=LOWER"]),
        ("GoogleMapsCompatible", [4118, 4118, 4118, 4406], None),
        ("PseudoTMS_GlobalGeodetic", [3439, 3439, 3439, 3691], None),
        ("PseudoTMS_GlobalMercator", [4118, 4118, 4118, 4406], None),
    ],
)
def test_gpkg_26(tmp_vsimem, scheme, expected_cs, other_options):

    src_ds = gdal.Open("data/byte.tif")
    options = ["TILE_FORMAT=PNG", "TILING_SCHEME=" + scheme]
    if other_options:
        options = options + other_options
    if expected_cs is None:
        with gdal.quiet_errors():
            ds = gdaltest.gpkg_dr.CreateCopy(
                tmp_vsimem / "tmp.gpkg", src_ds, options=options
            )
            assert ds is None
            return

    ds = gdaltest.gpkg_dr.CreateCopy(tmp_vsimem / "tmp.gpkg", src_ds, options=options)
    ds = None

    ds = gdal.OpenEx(tmp_vsimem / "tmp.gpkg", open_options=["BAND_COUNT=4"])
    assert ds.GetMetadataItem("AREA_OR_POINT") == "Area"
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == pytest.approx(
        expected_cs, abs=10
    )
    ds = None


@pytest.mark.require_driver("PNG")
@pytest.mark.parametrize(
    "scheme,expected_cs,other_options",
    [
        (
            "GoogleCRS84Quad",
            [42255, 47336, 24963, 35707],
            None,
        ),
        ("GoogleMapsCompatible", [31249, 35596, 19001, 17849], None),
    ],
)
def test_gpkg_26_bis(tmp_vsimem, scheme, expected_cs, other_options):

    src_ds = gdal.Open("data/small_world.tif")
    options = ["TILE_FORMAT=PNG", "TILING_SCHEME=" + scheme]
    if other_options:
        options = options + other_options
    ds = gdaltest.gpkg_dr.CreateCopy(tmp_vsimem / "tmp.gpkg", src_ds, options=options)
    ds = None

    ds = gdal.Open(tmp_vsimem / "tmp.gpkg")
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == pytest.approx(
        expected_cs, abs=10
    )


def test_gpkg_26_errors():
    # Test a few error cases
    with gdal.quiet_errors():
        ds = gdaltest.gpkg_dr.Create(
            "/vsimem/tmp.gpkg",
            1,
            1,
            1,
            options=["TILING_SCHEME=GoogleCRS84Quad", "BLOCKSIZE=128"],
        )
    assert ds is None
    gdal.Unlink("/vsimem/tmp.gpkg")

    ds = gdaltest.gpkg_dr.Create(
        "/vsimem/tmp.gpkg", 1, 1, 1, options=["TILING_SCHEME=GoogleCRS84Quad"]
    )
    # Test that implicit SRS registration works.
    assert ds.GetProjectionRef().find("4326") >= 0
    with gdal.quiet_errors():
        ret = ds.SetGeoTransform([0, 10, 0, 0, 0, -10])
    assert ret != 0
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32630)
    with gdal.quiet_errors():
        ret = ds.SetProjection(srs.ExportToWkt())
    assert ret != 0
    with gdal.quiet_errors():
        ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Test with a .json tile matrix set
    ds = gdal.Translate(
        "/vsimem/tmp.gpkg",
        "../gdrivers/data/small_world.tif",
        options="-of GPKG -co TILING_SCHEME=LINZAntarticaMapTileGrid -projwin -180 -50 180 -90",
    )
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "5482"
    assert ds.GetGeoTransform() == pytest.approx(
        ((314023.27126670163, 28672, 0.0, 5685976.728733298, 0.0, -28672)), abs=1e-8
    )
    ds = None
    gdal.Unlink("/vsimem/tmp.gpkg")

    # Unsupported TILING_SCHEME
    src_ds = gdal.Open("data/byte.tif")
    with gdal.quiet_errors():
        assert (
            gdaltest.gpkg_dr.CreateCopy(
                "/vsimem/tmp.gpkg", src_ds, options=["TILING_SCHEME=NZTM2000"]
            )
            is None
        )
    gdal.Unlink("/vsimem/tmp.gpkg")

    # Invalid TILING_SCHEME
    src_ds = gdal.Open("data/byte.tif")
    with gdal.quiet_errors():
        assert (
            gdaltest.gpkg_dr.CreateCopy(
                "/vsimem/tmp.gpkg", src_ds, options=["TILING_SCHEME=invalid"]
            )
            is None
        )
    gdal.Unlink("/vsimem/tmp.gpkg")

    # Invalid target filename
    src_ds = gdal.Open("data/byte.tif")
    with gdal.quiet_errors():
        ds = gdaltest.gpkg_dr.CreateCopy(
            "/foo/tmp.gpkg", src_ds, options=["TILING_SCHEME=GoogleCRS84Quad"]
        )
    assert ds is None

    # Source is not georeferenced
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    with gdal.quiet_errors():
        ds = gdaltest.gpkg_dr.CreateCopy(
            "/vsimem/tmp.gpkg", src_ds, options=["TILING_SCHEME=GoogleCRS84Quad"]
        )
    assert ds is None


###############################################################################
# Test behaviour with low block cache max


@pytest.mark.require_driver("PNG")
def test_gpkg_27():

    gdal.Unlink("/vsimem/tmp.gpkg")

    with gdaltest.SetCacheMax(0):
        src_ds = gdal.Open("data/small_world.tif")
        out_ds = gdaltest.gpkg_dr.CreateCopy(
            "/vsimem/tmp.gpkg",
            src_ds,
            options=["TILE_FORMAT=PNG", "BLOCKXSIZE=200", "BLOCKYSIZE=200"],
        )

    expected_cs = [src_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert got_cs == expected_cs


###############################################################################
# Test that reading a block in a band doesn't wipe another band of the same
# block that would have gone through the GPKG in-memory cache


@pytest.mark.require_driver("PNG")
def test_gpkg_28():

    gdal.Unlink("/vsimem/tmp.gpkg")

    src_ds = gdal.Open("data/small_world.tif")
    data = []
    for b in range(3):
        data.append(src_ds.GetRasterBand(b + 1).ReadRaster())
    expected_cs = [src_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    src_ds = None

    out_ds = gdaltest.gpkg_dr.Create(
        "/vsimem/tmp.gpkg",
        400,
        200,
        3,
        options=["TILE_FORMAT=PNG", "BLOCKXSIZE=400", "BLOCKYSIZE=200"],
    )
    out_ds.SetGeoTransform([0, 10, 0, 0, 0, -10])

    out_ds.GetRasterBand(1).WriteRaster(0, 0, 400, 200, data[0])
    # Force the block to go through IWriteBlock()
    with gdaltest.SetCacheMax(0):
        pass
    # Read (another, but could be any) band
    out_ds.GetRasterBand(2).ReadRaster(0, 0, 400, 200)
    # Write remaining bands 2 and 3
    for b in range(2):
        out_ds.GetRasterBand(b + 2).WriteRaster(0, 0, 400, 200, data[b + 1])
    out_ds = None
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=3"])
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert got_cs == expected_cs


###############################################################################
# Variation of gpkg_28 with 2 blocks
# When x=200, the read is done in another block


@pytest.mark.require_driver("PNG")
@pytest.mark.parametrize("x", [0, 200])
def test_gpkg_29(x):

    gdal.Unlink("/vsimem/tmp.gpkg")

    src_ds = gdal.Open("data/small_world.tif")
    left = []
    right = []
    for b in range(3):
        left.append(src_ds.GetRasterBand(b + 1).ReadRaster(0, 0, 200, 200))
        right.append(src_ds.GetRasterBand(b + 1).ReadRaster(200, 0, 200, 200))
    expected_cs = [src_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    src_ds = None

    out_ds = gdaltest.gpkg_dr.Create(
        "/vsimem/tmp.gpkg",
        400,
        200,
        3,
        options=["TILE_FORMAT=PNG", "BLOCKXSIZE=200", "BLOCKYSIZE=200"],
    )
    out_ds.SetGeoTransform([0, 10, 0, 0, 0, -10])

    out_ds.GetRasterBand(1).WriteRaster(0, 0, 200, 200, left[0])
    # Force the block to go through IWriteBlock()
    with gdaltest.SetCacheMax(0):
        pass
    out_ds.GetRasterBand(2).ReadRaster(x, 0, 200, 200)
    for b in range(2):
        out_ds.GetRasterBand(b + 2).WriteRaster(0, 0, 200, 200, left[b + 1])
    for b in range(3):
        out_ds.GetRasterBand(b + 1).WriteRaster(200, 0, 200, 200, right[b])
    out_ds = None
    out_ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=3"])
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(3)]
    assert got_cs == expected_cs


###############################################################################
# 1 band to RGBA


@pytest.mark.require_driver("PNG")
def test_gpkg_31():

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Force use of RGBA instead of Grey-Alpha (the natural use case is WEBP)
    # but here we can test losslessly
    with gdal.config_option("GPKG_PNG_SUPPORTS_2BANDS", "NO"):
        gdaltest.gpkg_dr.CreateCopy(
            "/vsimem/tmp.gpkg",
            gdal.Open("data/byte.tif"),
            options=["TILE_FORMAT=PNG", "BLOCKSIZE=21"],
        )

    ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    check_tile_format(ds, "PNG", 4, False)
    expected_cs = [4672, 4672, 4672, 4873]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs


###############################################################################
# grey-alpha to RGBA


@pytest.mark.require_driver("PNG")
def test_gpkg_32():

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Force use of RGBA instead of Grey-Alpha (the natural use case is WEBP)
    # but here we can test losslessly
    with gdal.config_option("GPKG_PNG_SUPPORTS_2BANDS", "NO"):
        gdaltest.gpkg_dr.CreateCopy(
            "/vsimem/tmp.gpkg",
            get_georeferenced_greyalpha_ds(),
            options=["TILE_FORMAT=PNG", "BLOCKSIZE=200"],
        )

    ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    check_tile_format(ds, "PNG", 4, False)
    expected_cs = [1970, 1970, 1970, 10807]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert got_cs == expected_cs

    ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=2"])
    expected_cs = [1970, 10807]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
    assert got_cs == expected_cs


###############################################################################
# Single band with 32 bit color table -> RGBA


@pytest.mark.require_driver("PNG")
def test_gpkg_33():

    gdal.Unlink("/vsimem/tmp.gpkg")

    # Force use of RGBA instead of color-table (the natural use case is WEBP)
    # but here we can test losslessly
    with gdal.config_option("GPKG_PNG_SUPPORTS_CT", "NO"):
        src_ds = get_georeferenced_ds_with_pct32()
        gdaltest.gpkg_dr.CreateCopy(
            "/vsimem/tmp.gpkg", src_ds, options=["TILE_FORMAT=PNG"]
        )
    gdal.Unlink(src_ds.GetDescription())

    ds = gdal.OpenEx("/vsimem/tmp.gpkg", open_options=["BAND_COUNT=4"])
    check_tile_format(ds, "PNG", 4, False)
    expected_cs = [10991, 57677, 34965, 10638]
    got_cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs


###############################################################################
# Test partial tiles with overviews (#6335)


@pytest.mark.require_driver("PNG")
def test_gpkg_34():

    gdal.Unlink("/vsimem/tmp.gpkg")

    src_ds = gdal.GetDriverByName("MEM").Create("", 512, 417)
    src_ds.SetGeoTransform(
        [
            -20037508.342789299786091,
            2 * 20037508.342789299786091 / 512,
            0,
            16213801.067584000527859,
            0,
            -2 * 16213801.067584000527859 / 417,
        ]
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    src_ds.SetProjection(srs.ExportToWkt())
    gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg",
        src_ds,
        options=["TILE_FORMAT=PNG", "TILING_SCHEME=GoogleMapsCompatible"],
    )
    ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)
    gdal.ErrorReset()
    ds.BuildOverviews("NEAR", [2])
    ds = None
    assert gdal.GetLastErrorMsg() == ""


###############################################################################
# Test dirty block flushing while reading block (#6365)


@pytest.mark.require_driver("PNG")
def test_gpkg_35():

    gdal.Unlink("/vsimem/tmp.gpkg")

    src_ds = gdal.GetDriverByName("MEM").Create("", 512, 417, 4)
    src_ds.SetGeoTransform(
        [
            -20037508.342789299786091,
            2 * 20037508.342789299786091 / 512,
            0,
            16213801.067584000527859,
            0,
            -2 * 16213801.067584000527859 / 417,
        ]
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    src_ds.SetProjection(srs.ExportToWkt())
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/tmp.gpkg",
        src_ds,
        options=["TILE_FORMAT=PNG", "TILING_SCHEME=GoogleMapsCompatible"],
    )
    out_ds.GetRasterBand(1).Fill(32)
    out_ds.GetRasterBand(2).Fill(64)
    out_ds.GetRasterBand(3).Fill(128)
    out_ds.GetRasterBand(4).Fill(255)
    height = out_ds.RasterYSize
    expected_data = out_ds.ReadRaster(0, 0, 256, height)
    out_ds = None

    src_ds = gdal.GetDriverByName("MEM").Create("", 256, height, 4)
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.GetRasterBand(2).Fill(255)
    src_ds.GetRasterBand(3).Fill(255)
    src_ds.GetRasterBand(4).Fill(255)
    white_data = src_ds.ReadRaster(0, 0, 256, height)

    ds = gdal.Open("/vsimem/tmp.gpkg", gdal.GA_Update)
    ds.WriteRaster(256, 0, 256, height, white_data)

    # + 2 * 128 > + 2 * sizeof(GDALRasterBlock). Cf gdalrasterblock.cpp:GetEffectiveBlockSize()
    with gdaltest.SetCacheMax((256 * 256 + 2 * 128) * 4):
        got_data = ds.ReadRaster(0, 0, 256, height)

    assert got_data == expected_data


###############################################################################
# Single band with 24 bit color table, PNG, GoogleMapsCompatible


@pytest.mark.require_driver("PNG")
def test_gpkg_36():

    src_ds = gdal.Open("data/small_world_pct.tif")
    out_ds = gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/gpkg_36.gpkg",
        src_ds,
        options=[
            "TILE_FORMAT=PNG",
            "TILING_SCHEME=GoogleMapsCompatible",
            "RESAMPLING=NEAREST",
        ],
    )
    out_ds = None
    src_ds = None

    expected_cs = [993, 50461, 64354, 17849]
    out_ds = gdal.Open("/vsimem/gpkg_36.gpkg")
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)]
    assert got_cs == expected_cs
    got_ct = out_ds.GetRasterBand(1).GetColorTable()
    assert got_ct is None
    out_ds = None

    gdal.Unlink("/vsimem/gpkg_36.gpkg")


###############################################################################
# Test that we don't crash when generating big overview factors on rasters with big dimensions
# due to issues in comparing the factor of overviews with the user specified
# factors


def test_gpkg_37():

    ds = gdal.GetDriverByName("GPKG").Create("/vsimem/gpkg_37.gpkg", 205000, 200000)
    ds.SetGeoTransform([100, 0.000001, 0, 100, 0, -0.000001])
    ds = None

    ds = gdal.Open("/vsimem/gpkg_37.gpkg", gdal.GA_Update)
    ret = ds.BuildOverviews("NONE", [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048])
    assert ret == 0 and gdal.GetLastErrorMsg() == ""
    ds = None

    gdal.Unlink("/vsimem/gpkg_37.gpkg")


###############################################################################
# Test generating more than 1000 tiles


def test_gpkg_38():

    # Without padding, immediately after create copy
    src_ds = gdal.Open("data/small_world.tif")
    gdaltest.gpkg_dr.CreateCopy(
        "/vsimem/gpkg_38.gpkg", src_ds, options=["TILE_FORMAT=PNG", "BLOCKSIZE=8"]
    )

    ds = gdal.Open("/vsimem/gpkg_38.gpkg")
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    ds = None
    filesize = gdal.VSIStatL("/vsimem/gpkg_38.gpkg").size
    gdal.Unlink("/vsimem/gpkg_38.gpkg")

    filename = "/vsimem/||maxlength=%d||gpkg_38.gpkg" % (filesize - 100000)
    with gdal.quiet_errors():
        ds = gdaltest.gpkg_dr.CreateCopy(
            filename, src_ds, options=["TILE_FORMAT=PNG", "BLOCKSIZE=8"]
        )
        ds_is_none = ds is None
        ds = None
    gdal.Unlink(filename)
    assert ds_is_none or gdal.GetLastErrorMsg() != ""

    filename = "/vsimem/||maxlength=%d||gpkg_38.gpkg" % (filesize - 1)
    with gdal.quiet_errors():
        ds = gdaltest.gpkg_dr.CreateCopy(
            filename, src_ds, options=["TILE_FORMAT=PNG", "BLOCKSIZE=8"]
        )
        ds_is_none = ds is None
        ds = None
    gdal.Unlink(filename)
    assert ds_is_none or gdal.GetLastErrorMsg() != ""


###############################################################################
# Test tile gridded coverage data


def test_gpkg_39():

    src_ds = gdal.Open("data/int16.tif")
    gdal.Translate("/vsimem/gpkg_39.gpkg", src_ds, format="GPKG")

    assert validate("/vsimem/gpkg_39.gpkg"), "validation failed"

    ds = gdal.Open("/vsimem/gpkg_39.gpkg")

    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int16
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetMetadataItem("AREA_OR_POINT") == "Area"
    sql_lyr = ds.ExecuteSQL("SELECT scale, offset FROM gpkg_2d_gridded_tile_ancillary")
    f = sql_lyr.GetNextFeature()
    if f["scale"] != 1.0:
        f.DumpReadable()
        pytest.fail()
    if f["offset"] != 0.0:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL(
        "SELECT grid_cell_encoding FROM gpkg_2d_gridded_coverage_ancillary"
    )
    f = sql_lyr.GetNextFeature()
    if f["grid_cell_encoding"] != "grid-value-is-area":
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    # No metadata for now
    sql_lyr = ds.ExecuteSQL("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_metadata'")
    feat = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    feat_is_none = feat is None
    assert feat_is_none

    sql_lyr = ds.ExecuteSQL("PRAGMA application_id")
    f = sql_lyr.GetNextFeature()
    if f["application_id"] != 1196444487:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("PRAGMA user_version")
    f = sql_lyr.GetNextFeature()
    if f["user_version"] != 10400:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    # Statistics not available on partial tile without nodata
    md = ds.GetRasterBand(1).GetMetadata()
    assert md == {}
    ds = None

    # From a AREA_OR_POINT=Point dataset
    src_ds2 = gdal.GetDriverByName("MEM").Create("", 2, 2, 1, gdal.GDT_Int16)
    src_ds2.SetGeoTransform([2, 1, 0, 49, 0, -1])
    src_ds2.SetMetadataItem("AREA_OR_POINT", "Point")
    src_ds2.GetRasterBand(1).SetUnitType("m")
    gdal.Translate("/vsimem/gpkg_39.gpkg", src_ds2, format="GPKG")
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetMetadataItem("AREA_OR_POINT") == "Point", ds.GetMetadata()
    assert ds.GetRasterBand(1).GetUnitType() == "m"
    ds = None

    # Test GRID_CELL_ENCODING=grid-value-is-corner
    if os.path.exists("data/byte.tif.aux.xml"):
        os.unlink("data/byte.tif.aux.xml")
    gdal.Translate(
        "/vsimem/gpkg_39.gpkg",
        "data/byte.tif",
        format="GPKG",
        outputType=gdal.GDT_UInt16,
        creationOptions=["GRID_CELL_ENCODING=grid-value-is-corner"],
    )
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetMetadataItem("AREA_OR_POINT") == "Point", ds.GetMetadata()
    assert ds.GetRasterBand(1).GetMetadata() == {
        "GRID_CELL_ENCODING": "grid-value-is-corner"
    }

    # No metadata for now
    sql_lyr = ds.ExecuteSQL("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_metadata'")
    feat = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    feat_is_none = feat is None
    assert feat_is_none
    ds = None

    # Test GRID_CELL_ENCODING=grid-value-is-corner
    src_with_stats = gdal.Translate("", "data/byte.tif", format="MEM")
    src_with_stats.GetRasterBand(1).ComputeStatistics(approx_ok=False)
    gdal.Translate(
        "/vsimem/gpkg_39.gpkg",
        src_with_stats,
        format="GPKG",
        outputType=gdal.GDT_UInt16,
        creationOptions=["GRID_CELL_ENCODING=grid-value-is-corner"],
    )
    assert gdal.VSIStatL("/vsimem/gpkg_39.gpkg.aux.xml") is None
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetMetadataItem("AREA_OR_POINT") == "Point", ds.GetMetadata()
    expected_md = src_with_stats.GetRasterBand(1).GetMetadata()
    expected_md.update({"GRID_CELL_ENCODING": "grid-value-is-corner"})
    assert ds.GetRasterBand(1).GetMetadata() == expected_md
    ds = None

    # With nodata: statistics available
    gdal.Translate("/vsimem/gpkg_39.gpkg", src_ds, format="GPKG", noData=0)

    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    md = ds.GetRasterBand(1).GetMetadata()
    assert md == {"STATISTICS_MINIMUM": "74", "STATISTICS_MAXIMUM": "255"}
    ds = None

    assert gdal.VSIStatL("/vsimem/gpkg_39.gpkg.aux.xml") is None

    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    mdi = ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM")
    assert mdi == "74"
    ds = None

    # Entire tile: statistics available
    gdal.Translate("/vsimem/gpkg_39.gpkg", src_ds, format="GPKG", width=256, height=256)

    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    md = ds.GetRasterBand(1).GetMetadata()
    assert md == {"STATISTICS_MINIMUM": "74", "STATISTICS_MAXIMUM": "255"}
    ds = None

    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    mdi = ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM")
    assert mdi == "74"
    ds = None

    gdal.Translate("/vsimem/gpkg_39.gpkg", src_ds, format="GPKG", noData=1)
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetNoDataValue() == -32768.0

    gdal.Translate("/vsimem/gpkg_39.gpkg", src_ds, format="GPKG", noData=74)
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4649
    assert ds.GetRasterBand(1).GetNoDataValue() == -32768.0

    gdal.Translate(
        "/vsimem/gpkg_39.gpkg",
        src_ds,
        format="GPKG",
        noData=1,
        creationOptions=["TILING_SCHEME=GoogleMapsCompatible"],
    )
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4118 or cs == 4077

    with gdal.config_option("GPKG_ADD_DEFINITION_12_063", "YES"):
        gdal.Translate(
            "/vsimem/gpkg_39.gpkg",
            src_ds,
            format="GPKG",
            noData=1,
            creationOptions=["TILING_SCHEME=GoogleMapsCompatible"],
        )
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4118 or cs == 4077

    gdal.Translate(
        "/vsimem/gpkg_39.gpkg", src_ds, format="GPKG", width=1024, height=1024
    )
    ds = gdal.Open("/vsimem/gpkg_39.gpkg", gdal.GA_Update)
    ds.BuildOverviews("NEAR", [2, 4])
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 37308
    ds.BuildOverviews("NONE", [])
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetRasterBand(1).GetOverviewCount() == 0

    gdal.Translate(
        "/vsimem/gpkg_39.gpkg", src_ds, format="GPKG", outputType=gdal.GDT_UInt16
    )
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert ds.GetRasterBand(1).Checksum() == 4672
    sql_lyr = ds.ExecuteSQL("SELECT scale, offset FROM gpkg_2d_gridded_tile_ancillary")
    f = sql_lyr.GetNextFeature()
    assert f["scale"] == 1.0
    ds.ReleaseResultSet(sql_lyr)

    gdal.Translate(
        "/vsimem/gpkg_39.gpkg",
        src_ds,
        format="GPKG",
        outputType=gdal.GDT_UInt16,
        noData=1,
    )
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetNoDataValue() == 1.0

    gdal.Translate(
        "/vsimem/gpkg_39.gpkg",
        src_ds,
        format="GPKG",
        outputType=gdal.GDT_UInt16,
        noData=74,
    )
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4672
    assert ds.GetRasterBand(1).GetNoDataValue() == 74.0

    src_ds = gdal.Open("data/float32.tif")
    gdal.Translate("/vsimem/gpkg_39.gpkg", src_ds, format="GPKG")
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
    assert ds.GetRasterBand(1).Checksum() == 4672
    sql_lyr = ds.ExecuteSQL("SELECT scale, offset FROM gpkg_2d_gridded_tile_ancillary")
    f = sql_lyr.GetNextFeature()
    assert f.GetField("scale") == 1.0
    ds.ReleaseResultSet(sql_lyr)

    gdal.Translate("/vsimem/gpkg_39.gpkg", src_ds, format="GPKG", noData=1)
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetNoDataValue() == 1

    gdal.Translate(
        "/vsimem/gpkg_39.gpkg",
        src_ds,
        format="GPKG",
        creationOptions=["TILE_FORMAT=PNG"],
    )
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
    assert ds.GetRasterBand(1).Checksum() == 4672
    sql_lyr = ds.ExecuteSQL("SELECT scale, offset FROM gpkg_2d_gridded_tile_ancillary")
    f = sql_lyr.GetNextFeature()
    assert f["scale"] != 1.0 and f.IsFieldSetAndNotNull("scale")
    ds.ReleaseResultSet(sql_lyr)

    gdal.Translate(
        "/vsimem/gpkg_39.gpkg",
        src_ds,
        format="GPKG",
        noData=74,
        creationOptions=["TILE_FORMAT=PNG"],
    )
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
    assert ds.GetRasterBand(1).GetNoDataValue() == pytest.approx(
        -3.4028234663852885981e38, rel=1e-8
    )
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4651
    sql_lyr = ds.ExecuteSQL("SELECT scale, offset FROM gpkg_2d_gridded_tile_ancillary")
    f = sql_lyr.GetNextFeature()
    assert f["scale"] != 1.0 and f.IsFieldSetAndNotNull("scale")
    ds.ReleaseResultSet(sql_lyr)

    # Particular case with nodata = -32768 for Int16
    gdal.FileFromMemBuffer(
        "/vsimem/gpkg_39.asc",
        """ncols        6
nrows        1
xllcorner    440720
yllcorner    3750120
cellsize     60
NODATA_value -32768
 -32768 -32767 -32766 0 32766 32767""",
    )
    gdal.Translate(
        "/vsimem/gpkg_39.gpkg",
        "/vsimem/gpkg_39.asc",
        format="GPKG",
        outputType=gdal.GDT_Int16,
    )
    src_ds = gdal.Open("/vsimem/gpkg_39.asc")
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int16
    assert ds.GetRasterBand(1).GetNoDataValue() == -32768.0
    assert (
        ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    ), ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    src_ds = None
    gdal.Unlink("/vsimem/gpkg_39.asc")

    # Particular case with nodata = 65535 for UInt16
    gdal.FileFromMemBuffer(
        "/vsimem/gpkg_39.asc",
        """ncols        6
nrows        1
xllcorner    440720
yllcorner    3750120
cellsize     60
NODATA_value 65535
0 1 2 65533 65534 65535""",
    )
    gdal.Translate(
        "/vsimem/gpkg_39.gpkg",
        "/vsimem/gpkg_39.asc",
        format="GPKG",
        outputType=gdal.GDT_UInt16,
    )
    src_ds = gdal.Open("/vsimem/gpkg_39.asc")
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert ds.GetRasterBand(1).GetNoDataValue() == 65535.0
    assert (
        ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    ), ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    src_ds = None
    gdal.Unlink("/vsimem/gpkg_39.asc")

    # Particular case with nodata = 0 for UInt16
    gdal.FileFromMemBuffer(
        "/vsimem/gpkg_39.asc",
        """ncols        6
nrows        1
xllcorner    440720
yllcorner    3750120
cellsize     60
NODATA_value 0
0 1 2 65533 65534 65535""",
    )
    gdal.Translate(
        "/vsimem/gpkg_39.gpkg",
        "/vsimem/gpkg_39.asc",
        format="GPKG",
        outputType=gdal.GDT_UInt16,
    )
    src_ds = gdal.Open("/vsimem/gpkg_39.asc")
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert ds.GetRasterBand(1).GetNoDataValue() == 0
    assert (
        ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    ), ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    src_ds = None
    gdal.Unlink("/vsimem/gpkg_39.asc")

    # Test large dynamic for Float32 and TILE_FORMAT=PNG
    gdal.FileFromMemBuffer(
        "/vsimem/gpkg_39.asc",
        """ncols        2
nrows        1
xllcorner    440720
yllcorner    3750120
cellsize     60
-100000 100000""",
    )
    gdal.Translate(
        "/vsimem/gpkg_39.gpkg",
        "/vsimem/gpkg_39.asc",
        format="GPKG",
        outputType=gdal.GDT_Float32,
        creationOptions=["TILE_FORMAT=PNG"],
    )

    assert validate("/vsimem/gpkg_39.gpkg"), "validation failed"

    src_ds = gdal.Open("/vsimem/gpkg_39.asc")
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
    assert (
        ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    ), ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    src_ds = None
    gdal.Unlink("/vsimem/gpkg_39.asc")

    # Test large dynamic for Float32 and TILE_FORMAT=PNG and nodata
    gdal.FileFromMemBuffer(
        "/vsimem/gpkg_39.asc",
        """ncols        2
nrows        1
xllcorner    440720
yllcorner    3750120
cellsize     60
-100000 100000""",
    )
    gdal.Translate(
        "/vsimem/gpkg_39.gpkg",
        "/vsimem/gpkg_39.asc",
        format="GPKG",
        outputType=gdal.GDT_Float32,
        noData=0,
        creationOptions=["TILE_FORMAT=PNG"],
    )
    src_ds = gdal.Open("/vsimem/gpkg_39.asc")
    ds = gdal.Open("/vsimem/gpkg_39.gpkg")
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
    assert (
        ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    ), ds.GetRasterBand(1).GetNoDataValue()
    ds = None
    src_ds = None
    gdal.Unlink("/vsimem/gpkg_39.asc")

    # Test that we can delete an existing tile
    ds = gdaltest.gpkg_dr.Create("/vsimem/gpkg_39.gpkg", 256, 256, 1, gdal.GDT_UInt16)
    ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    ds.GetRasterBand(1).SetNoDataValue(0)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).FlushCache()
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(1).FlushCache()
    sql_lyr = ds.ExecuteSQL("SELECT scale, offset FROM gpkg_2d_gridded_tile_ancillary")
    f = sql_lyr.GetNextFeature()
    assert f is None
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink("/vsimem/gpkg_39.gpkg")

    # Test detecting tiles at zero (without nodata value)
    ds = gdaltest.gpkg_dr.Create("/vsimem/gpkg_39.gpkg", 256, 256, 1, gdal.GDT_Float32)
    ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(1).FlushCache()
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_39")
    f = sql_lyr.GetNextFeature()
    assert f is None
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink("/vsimem/gpkg_39.gpkg")

    # Test detecting tiles at nodata value
    ds = gdaltest.gpkg_dr.Create("/vsimem/gpkg_39.gpkg", 256, 256, 1, gdal.GDT_Float32)
    ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    ds.GetRasterBand(1).SetNoDataValue(2)
    ds.GetRasterBand(1).Fill(2)
    ds.GetRasterBand(1).FlushCache()
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_39")
    f = sql_lyr.GetNextFeature()
    assert f is None
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink("/vsimem/gpkg_39.gpkg")
    gdal.Unlink("/vsimem/gpkg_39.gpkg.aux.xml")


###############################################################################
# Test statistics stored in GPKG


@pytest.mark.parametrize("source_filename", ["data/byte.tif", "data/int16.tif"])
def test_gpkg_statistics_stored_in_gpkg(source_filename):

    src_ds = gdal.Open(source_filename)
    out_filename = "/vsimem/test_gpkg_statistics.gpkg"
    out_ds = gdal.GetDriverByName("GPKG").CreateCopy(out_filename, src_ds)
    assert out_ds.GetFileList() == [out_filename]
    out_ds.GetRasterBand(1).ComputeStatistics(False)
    assert out_ds.GetRasterBand(1).GetMetadata() == {
        "STATISTICS_MAXIMUM": "255",
        "STATISTICS_MEAN": "126.765",
        "STATISTICS_MINIMUM": "74",
        "STATISTICS_STDDEV": "22.928470838676",
        "STATISTICS_VALID_PERCENT": "100",
    }
    out_ds = None

    assert gdal.VSIStatL(out_filename + ".aux.xml") is None

    ds = gdal.Open(out_filename)
    assert ds.GetFileList() == [out_filename]
    assert ds.GetRasterBand(1).GetMetadata() == {
        "STATISTICS_MAXIMUM": "255",
        "STATISTICS_MEAN": "126.765",
        "STATISTICS_MINIMUM": "74",
        "STATISTICS_STDDEV": "22.928470838676",
        "STATISTICS_VALID_PERCENT": "100",
    }
    assert set(ds.GetMetadataDomainList()) == set(
        ["", "IMAGE_STRUCTURE", "DERIVED_SUBDATASETS"]
    )
    ds = None

    assert gdal.VSIStatL(out_filename + ".aux.xml") is None

    # Invalidate statistics
    ds = gdal.Open(out_filename, gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None

    assert gdal.VSIStatL(out_filename + ".aux.xml") is None

    ds = gdal.Open(out_filename)
    assert ds.GetRasterBand(1).GetMetadata() == {}
    ds = None

    gdal.GetDriverByName("GPKG").Delete(out_filename)


###############################################################################
# Test statistics stored in PAM


@pytest.mark.parametrize("source_filename", ["data/byte.tif", "data/int16.tif"])
def test_gpkg_statistics_stored_in_pam(source_filename):

    src_ds = gdal.Open(source_filename)
    out_filename = "/vsimem/test_gpkg_statistics.gpkg"
    gdal.GetDriverByName("GPKG").CreateCopy(out_filename, src_ds)
    ds = gdal.Open(out_filename)
    ds.GetRasterBand(1).ComputeStatistics(False)
    ds = None

    assert gdal.VSIStatL(out_filename + ".aux.xml") is not None

    ds = gdal.Open(out_filename)
    assert ds.GetFileList() == [out_filename, out_filename + ".aux.xml"]
    assert ds.GetRasterBand(1).GetMetadata() == {
        "STATISTICS_MAXIMUM": "255",
        "STATISTICS_MEAN": "126.765",
        "STATISTICS_MINIMUM": "74",
        "STATISTICS_STDDEV": "22.928470838676",
        "STATISTICS_VALID_PERCENT": "100",
    }
    assert set(ds.GetMetadataDomainList()) == set(
        ["", "IMAGE_STRUCTURE", "DERIVED_SUBDATASETS"]
    )
    ds = None

    # Invalidate statistics
    ds = gdal.Open(out_filename, gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = gdal.Open(out_filename)
    assert ds.GetRasterBand(1).GetMetadata() == {}
    ds = None

    gdal.GetDriverByName("GPKG").Delete(out_filename)


###############################################################################
# Test VERSION


def test_gpkg_40():

    src_ds = gdal.Open("data/byte.tif")
    # Should default to 1.2
    gdal.Translate("/vsimem/gpkg_40.gpkg", src_ds, format="GPKG")
    ds = gdal.Open("/vsimem/gpkg_40.gpkg")
    sql_lyr = ds.ExecuteSQL("PRAGMA application_id")
    f = sql_lyr.GetNextFeature()
    if f["application_id"] != 1196444487:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("PRAGMA user_version")
    f = sql_lyr.GetNextFeature()
    if f["user_version"] != 10400:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    # Should default to 1.2 if we didn't override it.
    gdal.Translate(
        "/vsimem/gpkg_40.gpkg",
        src_ds,
        format="GPKG",
        outputType=gdal.GDT_Int16,
        creationOptions=["VERSION=1.0"],
    )
    ds = gdal.Open("/vsimem/gpkg_40.gpkg")
    sql_lyr = ds.ExecuteSQL("PRAGMA application_id")
    f = sql_lyr.GetNextFeature()
    if f["application_id"] != 1196437808:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("PRAGMA user_version")
    f = sql_lyr.GetNextFeature()
    if f["user_version"] != 0:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Translate(
        "/vsimem/gpkg_40.gpkg", src_ds, format="GPKG", creationOptions=["VERSION=1.1"]
    )
    ds = gdal.Open("/vsimem/gpkg_40.gpkg")
    sql_lyr = ds.ExecuteSQL("PRAGMA application_id")
    f = sql_lyr.GetNextFeature()
    if f["application_id"] != 1196437809:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("PRAGMA user_version")
    f = sql_lyr.GetNextFeature()
    if f["user_version"] != 0:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Translate(
        "/vsimem/gpkg_40.gpkg", src_ds, format="GPKG", creationOptions=["VERSION=1.2"]
    )
    ds = gdal.Open("/vsimem/gpkg_40.gpkg")
    sql_lyr = ds.ExecuteSQL("PRAGMA application_id")
    f = sql_lyr.GetNextFeature()
    if f["application_id"] != 1196444487:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("PRAGMA user_version")
    f = sql_lyr.GetNextFeature()
    if f["user_version"] != 10200:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink("/vsimem/gpkg_40.gpkg")


###############################################################################
# Robustness test


def test_gpkg_41():

    # Causes SIGKILL otherwise in Vagrant or other VM based infrastructure
    # when trying to allocate huge amount of memory
    if gdal.GetUsablePhysicalRAM() < 8 * 1000 * 1000 * 1000:
        pytest.skip(
            "Only %.1f GB RAM available. At least 8 GB needed"
            % (gdal.GetUsablePhysicalRAM() / 1e9)
        )

    with gdal.config_option(
        "GPKG_ALLOW_CRAZY_SETTINGS", "YES"
    ), gdaltest.error_handler():
        gdal.Translate(
            "/vsimem/gpkg_41.gpkg",
            "data/gpkg/huge_line.tif",
            format="GPKG",
            creationOptions=["BLOCKXSIZE=500000000", "BLOCKYSIZE=1"],
        )

    gdal.Unlink("/vsimem/gpkg_41.gpkg")


###############################################################################
# Test opening in vector mode a database without gpkg_geometry_columns


def test_gpkg_42():

    with gdal.config_option("CREATE_GEOMETRY_COLUMNS", "NO"):
        gdal.Translate("/vsimem/gpkg_42.gpkg", "data/byte.tif", format="GPKG")

    ds = gdal.OpenEx("/vsimem/gpkg_42.gpkg", gdal.OF_VECTOR | gdal.OF_UPDATE)
    sql_lyr = ds.ExecuteSQL(
        "SELECT 1 FROM sqlite_master WHERE name = 'gpkg_geometry_columns'"
    )
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 0
    lyr = ds.CreateLayer("test")
    assert lyr is not None
    ds.FlushCache()
    assert gdal.GetLastErrorMsg() == ""
    ds = None

    gdal.Unlink("/vsimem/gpkg_42.gpkg")


###############################################################################
# Test adding raster to a database without pre-existing raster support tables.


def test_gpkg_43():

    with gdal.config_option("CREATE_RASTER_TABLES", "NO"):
        ds = gdaltest.gpkg_dr.Create("/vsimem/gpkg_43.gpkg", 0, 0, 0, gdal.GDT_Unknown)
    ds.CreateLayer("foo")
    ds = None

    ds = gdal.OpenEx("/vsimem/gpkg_43.gpkg", gdal.OF_UPDATE)
    sql_lyr = ds.ExecuteSQL(
        "SELECT 1 FROM sqlite_master WHERE name = 'gpkg_tile_matrix_set'"
    )
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 0
    ds = None

    gdal.Translate(
        "/vsimem/gpkg_43.gpkg",
        "data/byte.tif",
        format="GPKG",
        creationOptions=["APPEND_SUBDATASET=YES"],
    )
    ds = gdal.OpenEx("/vsimem/gpkg_43.gpkg")
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetLayerCount() == 1
    ds = None

    assert validate("/vsimem/gpkg_43.gpkg"), "validation failed"

    gdal.Unlink("/vsimem/gpkg_43.gpkg")


###############################################################################
# Test opening a .gpkg.sql file


def test_gpkg_44():

    if gdaltest.gpkg_dr.GetMetadataItem("ENABLE_SQL_GPKG_FORMAT") != "YES":
        pytest.skip()

    ds = gdal.Open("data/gpkg/byte.gpkg.sql")
    assert ds.GetRasterBand(1).Checksum() == 4672, "validation failed"


###############################################################################
# Test opening a .gpkg file


def test_gpkg_45():

    ds = gdal.Open("data/gpkg/byte.gpkg")
    assert ds.GetRasterBand(1).Checksum() == 4672, "validation failed"


###############################################################################
# Test fix for #6932


def test_gpkg_46():

    ds = gdaltest.gpkg_dr.Create(
        "/vsimem/gpkg_46.gpkg",
        6698,
        6698,
        options=["TILING_SCHEME=GoogleMapsCompatible"],
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    ds.SetProjection(srs.ExportToWkt())
    ds.SetGeoTransform([500, 0.037322767717371, 0, 750, 0, -0.037322767717371])
    ds = None

    ds = gdal.Open("/vsimem/gpkg_46.gpkg", gdal.GA_Update)
    ds.BuildOverviews("NEAR", [2, 4, 8, 16, 32, 64, 128, 256])
    ds = None

    ds = gdal.Open("/vsimem/gpkg_46.gpkg")
    sql_lyr = ds.ExecuteSQL(
        "SELECT zoom_level, matrix_width * pixel_x_size * tile_width, matrix_height * pixel_y_size * tile_height FROM gpkg_tile_matrix ORDER BY zoom_level"
    )
    count = 0
    for f in sql_lyr:
        count += 1
        if f.GetField(1) != pytest.approx(40075016.6855785, abs=1e-7) or f.GetField(
            2
        ) != pytest.approx(40075016.6855785, abs=1e-7):
            f.DumpReadable()
            ds.ReleaseResultSet(sql_lyr)
            gdal.Unlink("/vsimem/gpkg_46.gpkg")
            pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink("/vsimem/gpkg_46.gpkg")

    assert count == 23


###############################################################################
# Test fix for #6976


def test_gpkg_47():

    tmpfile = "/vsimem/gpkg_47.gpkg"
    ds = gdaltest.gpkg_dr.CreateCopy(tmpfile, gdal.Open("data/byte.tif"))
    ds.ExecuteSQL("UPDATE gpkg_contents SET min_x = 1, max_x = 0")
    ds = None
    with gdal.quiet_errors():
        ds = gdal.Open(tmpfile)
    assert ds.RasterXSize == 256
    ds = None

    gdal.Unlink(tmpfile)


###############################################################################
# Test fix for https://issues.qgis.org/issues/16997 (opening a file with
# subdatasets on Windows)


def test_gpkg_48():

    if sys.platform == "win32":
        filename = os.path.join(os.getcwd(), "tmp", "byte.gpkg")
    else:
        # Test Windows code path in a weird way...
        filename = "C:\\byte.gpkg"

    gdal.Translate(
        filename, "data/byte.tif", format="GPKG", creationOptions=["RASTER_TABLE=foo"]
    )
    gdal.Translate(
        filename,
        "data/byte.tif",
        format="GPKG",
        creationOptions=["APPEND_SUBDATASET=YES", "RASTER_TABLE=bar"],
    )
    ds = gdal.Open("GPKG:" + filename + ":foo")
    if ds is None:
        gdal.Unlink(filename)
        pytest.fail()
    ds = None
    ds = gdal.Open("GPKG:" + filename + ":bar")
    if ds is None:
        gdal.Unlink(filename)
        pytest.fail()
    ds = None

    gdal.Unlink(filename)


###############################################################################


def test_gpkg_delete_raster_layer():

    filename = "/vsimem/byte.gpkg"
    gdal.Translate(
        filename, "data/byte.tif", format="GPKG", creationOptions=["RASTER_TABLE=foo"]
    )
    ds = ogr.Open(filename, update=1)
    ds.ExecuteSQL("DROP TABLE foo")
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_metadata")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    ds.ExecuteSQL("VACUUM")
    ds = None

    assert fc == 0

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(filename, "rb")
    content = gdal.VSIFReadL(1, 1000000, f).decode("latin1")
    gdal.VSIFCloseL(f)

    assert "foo" not in content

    gdal.Unlink(filename)


###############################################################################


def test_gpkg_delete_gridded_coverage_raster_layer():

    filename = "/vsimem/float32.gpkg"
    gdal.Translate(
        filename,
        "data/float32.tif",
        format="GPKG",
        creationOptions=["RASTER_TABLE=foo"],
    )
    ds = ogr.Open(filename, update=1)
    ds.ExecuteSQL("DROP TABLE foo")
    ds.ExecuteSQL("VACUUM")
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(filename, "rb")
    content = gdal.VSIFReadL(1, 1000000, f).decode("latin1")
    gdal.VSIFCloseL(f)

    assert "foo" not in content

    gdal.Unlink(filename)


###############################################################################
def test_gpkg_open_old_gpkg_elevation_tiles_extension():

    gdal.ErrorReset()
    ds = gdal.Open("data/gpkg/uint16-old-elevation-extension.gpkg")
    assert gdal.GetLastErrorMsg() == ""
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4672


###############################################################################


def test_gpkg_GeneralCmdLineProcessor():

    import test_cli_utilities

    if (
        test_cli_utilities.get_gdalinfo_path() is not None
        and test_cli_utilities.get_ogrinfo_path() is not None
    ):
        ret_gdalinfo = gdaltest.runexternal(
            test_cli_utilities.get_gdalinfo_path() + " --format GPKG"
        )
        ret_ogrinfo = gdaltest.runexternal(
            test_cli_utilities.get_ogrinfo_path() + " --format GPKG"
        )
        assert (
            "<CreationOptionList>" in ret_gdalinfo
            and "<CreationOptionList>" in ret_ogrinfo
            and "scope=" not in ret_gdalinfo
            and "scope=" not in ret_ogrinfo
        )


###############################################################################


def test_gpkg_match_overview_factor():

    gdal.FileFromMemBuffer(
        "/vsimem/gpkg_match_overview_factor.gpkg",
        open("data/gpkg/test_match_overview_factor.gpkg", "rb").read(),
    )

    ds = gdal.Open("/vsimem/gpkg_match_overview_factor.gpkg", gdal.GA_Update)
    ret = ds.BuildOverviews("NONE", [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048])
    assert ret == 0 and gdal.GetLastErrorMsg() == ""
    ds = None

    gdal.Unlink("/vsimem/gpkg_match_overview_factor.gpkg")


###############################################################################


@pytest.mark.parametrize("version", ["1.2", "1.4"])
def test_gpkg_wkt2(version):

    # WKT2-only compatible SRS with EPSG code
    filename = "/vsimem/test_gpkg_wkt2.gpkg"
    ds = gdaltest.gpkg_dr.Create(filename, 1, 1, options=["VERSION=" + version])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4979)  # WGS 84 3D
    sr.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    ds.SetSpatialRef(sr)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds = None

    ds = gdal.Open(filename)
    sr_got = ds.GetSpatialRef()
    assert sr_got.IsSame(sr), sr_got.ExportToWkt()

    lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys ORDER BY srs_id")
    f = lyr.GetNextFeature()
    assert f.GetField("srs_name") == "Undefined Cartesian SRS"
    assert f.GetField("srs_id") == -1
    assert f.GetField("organization") == "NONE"
    assert f.GetField("organization_coordsys_id") == -1
    assert f.GetField("definition") == "undefined"
    assert (
        f.GetField("description") == "undefined Cartesian coordinate reference system"
    )
    assert f.GetField("definition_12_063") == "undefined"

    lyr.GetNextFeature()

    f = lyr.GetNextFeature()
    assert f.GetField("definition").startswith('GEOGCS["WGS 84"')
    assert f.GetField("definition_12_063").startswith(
        'GEODCRS["WGS 84"'
    ) and 'ID["EPSG",4326]' in f.GetField("definition_12_063")

    f = lyr.GetNextFeature()
    assert f.GetField("definition") == "undefined"
    assert f.GetField("definition_12_063").startswith(
        'GEODCRS["WGS 84"'
    ) and 'ID["EPSG",4979]' in f.GetField("definition_12_063")

    ds.ReleaseResultSet(lyr)

    lyr = ds.ExecuteSQL(
        "SELECT * FROM gpkg_extensions WHERE extension_name = 'gpkg_crs_wkt'"
    )
    assert lyr.GetFeatureCount() == (1 if version == "1.2" else 0)
    ds.ReleaseResultSet(lyr)

    ds = None

    # WKT2-only compatible SRS without EPSG code
    ds = gdaltest.gpkg_dr.Create(
        filename, 1, 1, options=["APPEND_SUBDATASET=YES", "RASTER_TABLE=table2"]
    )
    sr.SetFromUserInput(
        'GEODCRS["my CRS",DATUM["my datum",ELLIPSOID["WGS 84",6378137,298.257223563,LENGTHUNIT["metre",1]]],PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433]],CS[ellipsoidal,3],AXIS["geodetic latitude (Lat)",north,ORDER[1],ANGLEUNIT["degree",0.0174532925199433]],AXIS["geodetic longitude (Lon)",east,ORDER[2],ANGLEUNIT["degree",0.0174532925199433]],AXIS["ellipsoidal height (h)",up,ORDER[3],LENGTHUNIT["metre",1]]]'
    )
    sr.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    ds.SetSpatialRef(sr)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds = None

    ds = gdal.Open("GPKG:" + filename + ":table2")
    sr_got = ds.GetSpatialRef()
    assert sr_got.IsSame(sr), sr_got.ExportToWkt()
    ds = None

    # WKT1 compatible SRS
    ds = gdaltest.gpkg_dr.Create(
        filename, 1, 1, options=["APPEND_SUBDATASET=YES", "RASTER_TABLE=table3"]
    )
    sr.ImportFromEPSG(32631)
    sr.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    ds.SetSpatialRef(sr)
    ds.SetGeoTransform([500000, 1, 0, 4500000, 0, -1])
    ds = None

    ds = gdal.Open("GPKG:" + filename + ":table3")
    sr_got = ds.GetSpatialRef()
    assert sr_got.IsSame(sr), sr_got.ExportToWkt()

    lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys WHERE srs_id = 32631")
    f = lyr.GetNextFeature()
    assert f.GetField("definition").startswith(
        'PROJCS["WGS 84 / UTM zone 31N",'
    ) and 'AUTHORITY["EPSG","32631"]' in f.GetField("definition")
    assert f.GetField("definition_12_063").startswith(
        'PROJCRS["WGS 84 / UTM zone 31N",'
    ) and 'ID["EPSG",32631]' in f.GetField("definition_12_063")
    ds.ReleaseResultSet(lyr)
    ds = None

    assert validate(filename), "validation failed"

    gdal.Unlink(filename)


###############################################################################
# Test reading a 50000x25000 block uint16


@pytest.mark.slow()
def test_gpkg_50000_25000_uint16():

    if sys.maxsize < 2**32:
        pytest.skip("Test not available on 32 bit")

    ds = gdal.Open(
        "/vsizip/data/gpkg/50000_25000_uint16.gpkg.zip/50000_25000_uint16.gpkg"
    )

    import psutil

    sizeof_uint16 = 2
    sizeof_block = 50000 * 25000 * sizeof_uint16
    # 2 * sizeof_block, because of GDAL block cache and GPKG internal cache
    if psutil.virtual_memory().available < 2 * sizeof_block:
        pytest.skip("Not enough virtual memory available")

    data = ds.ReadRaster(
        0, 0, ds.RasterXSize, ds.RasterYSize, buf_xsize=20, buf_ysize=20
    )
    assert data
    ref_ds = gdal.Open("../gcore/data/uint16.tif")
    assert data == ref_ds.ReadRaster()


###############################################################################
# Test reading a 50000x50000 block uint16


@pytest.mark.slow()
def test_gpkg_50000_50000_uint16():

    if sys.maxsize < 2**32:
        pytest.skip("Test not available on 32 bit")

    ds = gdal.Open(
        "/vsizip/data/gpkg/50000_50000_uint16.gpkg.zip/50000_50000_uint16.gpkg"
    )

    import psutil

    sizeof_uint16 = 2
    sizeof_block = 50000 * 50000 * sizeof_uint16
    # 2 * sizeof_block, because of GDAL block cache and GPKG internal cache
    if psutil.virtual_memory().available < 2 * sizeof_block:
        pytest.skip("Not enough virtual memory available")

    data = ds.ReadRaster(
        0, 0, ds.RasterXSize, ds.RasterYSize, buf_xsize=20, buf_ysize=20
    )
    assert data
    ref_ds = gdal.Open("../gcore/data/uint16.tif")
    assert data == ref_ds.ReadRaster()


###############################################################################
# Test writing PNG tiles with negative values


@pytest.mark.require_driver("PNG")
def test_gpkg_float32_png_negative_values():

    gdal.Unlink("/vsimem/tmp.gpkg")

    ds = gdaltest.gpkg_dr.Create(
        "/vsimem/tmp.gpkg", 1, 1, 1, gdal.GDT_Float32, options=["TILE_FORMAT=PNG"]
    )
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.GetRasterBand(1).SetNoDataValue(-32768)
    ds.GetRasterBand(1).Fill(-10)
    ds = None
    ds = gdal.Open("/vsimem/tmp.gpkg")
    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (-10, -10)
    ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test support for coordinate epoch


def test_gpkg_coordinate_epoch():

    gdal.Unlink("/vsimem/tmp.gpkg")

    ds = gdal.GetDriverByName("GPKG").Create("/vsimem/tmp.gpkg", 1, 1)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srs.SetCoordinateEpoch(2021.3)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.SetSpatialRef(srs)
    ds = None

    ds = gdal.Open("/vsimem/tmp.gpkg")
    srs = ds.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3
    ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test preservation of IsDynamic() and support for coordinate epoch


@pytest.mark.require_proj(7, 2)
def test_gpkg_coordinate_epoch_is_dynamic():

    gdal.Unlink("/vsimem/tmp.gpkg")

    ds = gdal.GetDriverByName("GPKG").Create("/vsimem/tmp.gpkg", 1, 1)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(9000)
    assert srs.IsDynamic()
    srs.SetCoordinateEpoch(2021.3)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.SetSpatialRef(srs)
    ds = None

    ds = gdal.Open("/vsimem/tmp.gpkg")
    srs = ds.GetSpatialRef()
    assert srs.IsDynamic()
    assert srs.GetCoordinateEpoch() == 2021.3
    ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test flushing only a subset of bands


@pytest.mark.parametrize("tile_format", ["PNG_JPEG", "PNG", "PNG8", "JPEG", "WEBP"])
def test_gpkg_flushing_not_all_bands(tmp_vsimem, tile_format):

    drv_req_dict = {
        "PNG_JPEG": ["PNG", "JPEG"],
        "PNG": "PNG",
        "PNG8": "PNG",
        "JPEG": "JPEG",
        "WEBP": "WEBP",
    }
    drv_req = drv_req_dict[tile_format]
    if not isinstance(drv_req, list):
        drv_req = [drv_req]
    for drv in drv_req:
        if gdal.GetDriverByName(drv) is None:
            pytest.skip(f"Driver {drv} is missing")

    out_filename = tmp_vsimem / "test.gpkg"
    ds = gdal.GetDriverByName("GPKG").Create(
        out_filename, 256, 256, 4, options=["TILE_FORMAT=" + tile_format]
    )
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.GetRasterBand(1).Fill(255)
    ds.GetRasterBand(2).Fill(255)
    ds.GetRasterBand(3).Fill(255)
    ds.FlushCache()
    ds.GetRasterBand(4).Fill(255)
    ds = None

    ds = gdal.Open(out_filename)
    if ds is None:
        # For some reason fails on the 2 below CI configs since
        # https://github.com/OSGeo/gdal/pull/12783
        # See https://github.com/OSGeo/gdal/actions/runs/16501023213/job/46659340642?pr=12783
        if gdaltest.is_travis_branch("ubuntu_2404") or gdaltest.is_travis_branch(
            "alpine_32bit"
        ):
            pytest.skip("fails on that CI config")

    assert (
        [ds.GetRasterBand(i + 1).ComputeRasterMinMax() for i in range(ds.RasterCount)]
    ) == [(255.0, 255.0)] * 4
    ds = None


###############################################################################
# Test bugfix for scenario of https://github.com/OSGeo/gdal/pull/6476


def test_gpkg_uint16_tiling_scheme_nodata_overview():

    src_ds = gdal.GetDriverByName("MEM").Create("", 100, 100, 1, gdal.GDT_UInt16)
    src_ds.GetRasterBand(1).SetNoDataValue(-9999)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    src_ds.SetProjection(srs.ExportToWkt())
    src_ds.SetGeoTransform(
        [-180, src_ds.RasterXSize / 360.0, 0, 90, 0, -src_ds.RasterYSize / 180.0]
    )

    out_filename = "/vsimem/test.gpkg"
    ds = gdal.GetDriverByName("GPKG").CreateCopy(
        out_filename, src_ds, options=["TILING_SCHEME=GoogleCRS84Quad"]
    )
    assert (
        ds.GetRasterBand(1).GetNoDataValue() == src_ds.GetRasterBand(1).GetNoDataValue()
    )
    assert (
        ds.GetRasterBand(1).GetOverview(0).GetNoDataValue()
        == src_ds.GetRasterBand(1).GetNoDataValue()
    )
    ds = None

    gdal.Unlink("/vsimem/tmp.gpkg")


###############################################################################
# Test GDAL storage of nodata value for Byte raster in metadata


@pytest.mark.parametrize("band_count", [1, 2])
@pytest.mark.require_driver("PNG")
def test_gpkg_byte_nodata_value(band_count):

    filename = "/vsimem/tmp.gpkg"
    gdal.Unlink(filename)

    ds = gdaltest.gpkg_dr.Create(
        filename, 1, 1, band_count, gdal.GDT_UInt8, options=["TILE_FORMAT=PNG"]
    )
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    with gdal.quiet_errors():
        assert ds.GetRasterBand(1).SetNoDataValue(-32768) == gdal.CE_Failure
    assert ds.GetRasterBand(1).SetNoDataValue(255) == gdal.CE_None
    if band_count == 2:
        with gdal.quiet_errors():
            assert ds.GetRasterBand(2).SetNoDataValue(254) == gdal.CE_Failure
    ds = None
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetNoDataValue() == 255
    if band_count == 2:
        assert ds.GetRasterBand(2).GetNoDataValue() == 255
    ds = None

    gdal.Unlink(filename)


###############################################################################
# Test gdal subdataset informational functions


@pytest.mark.parametrize(
    "filename,path_component,subdataset_component",
    (
        ("XXXXXXXXX:/test.gpkg:layer1", "", ""),
        ("GPKG:/test.gpkg", "", ""),
        ("GPKG:/test.gpkg:layer1", "/test.gpkg", "layer1"),
        ("gpkg:/test.gpkg:layer1", "/test.gpkg", "layer1"),
        (r"GPKG:c:\test.gpkg", "", ""),
        (r"GPKG:c:\test.gpkg:layer1", r"c:\test.gpkg", "layer1"),
        (r"gpkg:c:\test.gpkg:layer1", r"c:\test.gpkg", "layer1"),
    ),
)
def test_gpkg_gdal_subdataset_get_filename(
    filename, path_component, subdataset_component
):

    info = gdal.GetSubdatasetInfo(filename)
    if path_component == "":
        assert info is None
    else:
        assert info.GetPathComponent() == path_component
        assert info.GetSubdatasetComponent() == subdataset_component


@pytest.mark.parametrize(
    "subdataset_component,new_path",
    (
        ("GPKG:/test.gpkg", ""),
        ("GPKG:/test.gpkg:layer1", "GPKG:/new/test.gpkg:layer1"),
        ("gpkg:/test.gpkg:layer1", "gpkg:/new/test.gpkg:layer1"),
        (r"GPKG:c:\test.gpkg:layer1", "GPKG:/new/test.gpkg:layer1"),
        (r"gpkg:c:\test.gpkg:layer1", "gpkg:/new/test.gpkg:layer1"),
    ),
)
def test_gpkg_gdal_subdataset_modify_filename(subdataset_component, new_path):

    info = gdal.GetSubdatasetInfo(subdataset_component)
    if new_path == "":
        assert info is None
    else:
        assert info.ModifyPathComponent("/new/test.gpkg") == new_path


###############################################################################
# Test gdal_get_layer_pixel_value() function


def test_gpkg_sql_gdal_get_layer_pixel_value():

    filename = "/vsimem/test_ogr_gpkg_sql_gdal_get_layer_pixel_value.gpkg"
    src_ds = gdal.Open("data/byte.tif")
    gdal.GetDriverByName("GPKG").CreateCopy(
        filename, src_ds, options=["RASTER_TABLE=byte"]
    )
    src_ds = gdal.Open("data/float32.tif")
    gdal.GetDriverByName("GPKG").CreateCopy(
        filename, src_ds, options=["APPEND_SUBDATASET=YES"]
    )

    ds = gdal.OpenEx(filename)
    sql_lyr = ds.ExecuteSQL(
        "select gdal_get_layer_pixel_value('byte', 1, 'georef', 440780, 3751080)"
    )
    f = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    assert f[0] == 156

    sql_lyr = ds.ExecuteSQL(
        "select gdal_get_layer_pixel_value('byte', 1, 'pixel', 1, 4)"
    )
    f = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    assert f[0] == 156

    with ds.ExecuteSQL(
        "select gdal_get_layer_pixel_value('byte', 1, 'georef', 440780 + 30, 3751080 - 30)"
    ) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f[0] == 156

    with ds.ExecuteSQL(
        "select gdal_get_layer_pixel_value('byte', 1, 'georef', 440780 + 30, 3751080 - 30, 'cubicspline')"
    ) as sql_lyr:
        f = sql_lyr.GetNextFeature()
        assert f[0] == pytest.approx(150.1388888888889)

    sql_lyr = ds.ExecuteSQL(
        "select gdal_get_layer_pixel_value('float32', 1, 'pixel', 0, 1)"
    )
    f = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    assert f[0] == 115.0

    # Invalid column
    sql_lyr = ds.ExecuteSQL(
        "select gdal_get_layer_pixel_value('byte', 1, 'pixel', -1, 0)"
    )
    f = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    assert f[0] is None

    # NULL as 1st arg
    with gdal.quiet_errors():
        sql_lyr = ds.ExecuteSQL(
            "select gdal_get_layer_pixel_value(NULL, 1, 'pixel', 0, 0)"
        )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    # NULL as 2nd arg
    with gdal.quiet_errors():
        sql_lyr = ds.ExecuteSQL(
            "select gdal_get_layer_pixel_value('byte', NULL, 'pixel', 0, 0)"
        )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    # NULL as 3rd arg
    with gdal.quiet_errors():
        sql_lyr = ds.ExecuteSQL(
            "select gdal_get_layer_pixel_value('byte', 1, NULL, 0, 0)"
        )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    # NULL as 4th arg
    with gdal.quiet_errors():
        sql_lyr = ds.ExecuteSQL(
            "select gdal_get_layer_pixel_value('byte', 1, 'pixel', NULL, 0)"
        )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    # NULL as 5th arg
    with gdal.quiet_errors():
        sql_lyr = ds.ExecuteSQL(
            "select gdal_get_layer_pixel_value('byte', 1, 'pixel', 0, NULL)"
        )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    # Invalid band number
    with gdal.quiet_errors():
        sql_lyr = ds.ExecuteSQL(
            "select gdal_get_layer_pixel_value('byte', 0, 'pixel', 0, 0)"
        )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    # Invalid value for 3rd argument
    with gdal.quiet_errors():
        sql_lyr = ds.ExecuteSQL(
            "select gdal_get_layer_pixel_value('byte', 1, 'invalid', 0, 0)"
        )
        f = sql_lyr.GetNextFeature()
        ds.ReleaseResultSet(sql_lyr)
        assert f[0] is None

    gdal.Unlink(filename)


###############################################################################
# Test that we can write and open a .gti.gpkg file


def test_gpkg_gti_gpkg_ext(tmp_vsimem):

    filename = str(tmp_vsimem / "test_gpkg_gti_gpkg_ext.gti.gpkg")
    gdal.Translate(filename, "data/byte.tif", format="GPKG")
    ds = gdal.Open(filename)
    assert ds.GetDriver().ShortName == "GPKG"
    assert ds.GetRasterBand(1).Checksum() == 4672


###############################################################################
# Test rename a raster table with SQL


@pytest.mark.parametrize("data_type", [gdal.GDT_UInt8, gdal.GDT_UInt16])
def test_gpkg_rename_raster_table(data_type, tmp_vsimem):

    test_layer_path = str(tmp_vsimem / "test_gpkg_rename_raster_table.gpkg")

    if data_type == gdal.GDT_UInt16:
        src_ds = gdal.Open("data/int16.tif")
    else:
        src_ds = gdal.Open("data/small_world.tif")

    ds = gdaltest.gpkg_dr.CreateCopy(
        test_layer_path,
        src_ds,
        options=[
            "TILE_FORMAT=PNG",
            "RASTER_TABLE=weird'layer\"name",
        ],
    )
    ds = None
    src_ds = None

    ds = gdal.OpenEx(test_layer_path, gdal.OF_RASTER | gdal.OF_UPDATE)
    # Get layer name
    layer_name = ds.GetMetadataItem("IDENTIFIER")
    assert layer_name == "weird'layer\"name"

    checksum = ds.GetRasterBand(1).Checksum()

    ds.ExecuteSQL('ALTER TABLE "weird\'layer""name" RENAME TO bar')
    ds.ExecuteSQL("VACUUM")
    ds = None

    ds = gdal.Open(test_layer_path)
    layer_name = ds.GetMetadataItem("IDENTIFIER")
    assert layer_name == "bar"
    assert ds.GetRasterBand(1).Checksum() == checksum
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(test_layer_path, "rb")
    content = gdal.VSIFReadL(1, 1000000, f).decode("latin1")
    gdal.VSIFCloseL(f)

    assert "weird" not in content


###############################################################################
# Test GetDataCoverageStatus() is used on the source dataset


def test_gpkg_copy_using_get_data_coverage_status(tmp_vsimem):

    tmp_gtiff = str(tmp_vsimem / "tmp.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_gtiff,
        1024,
        768,
        1,
        options=["TILED=YES", "BLOCKXSIZE=256", "BLOCKYSIZE=256", "SPARSE_OK=YES"],
    )
    src_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src_ds.WriteRaster(512, 256, 256, 256, b"\x01" * (256 * 256))

    tmp_gpkg = str(tmp_vsimem / "tmp.gpkg")
    gdaltest.gpkg_dr.CreateCopy(tmp_gpkg, src_ds)

    ds = gdal.Open(tmp_gpkg)
    assert ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()

    with ds.ExecuteSQL("SELECT COUNT(*) FROM tmp") as sql_lyr:
        assert sql_lyr.GetFeatureCount() == 1

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(0, 0, 1024, 768)
    assert (
        flags
        == (gdal.GDAL_DATA_COVERAGE_STATUS_DATA | gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY)
        and pct == 100.0 / 12
    )

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(0, 0, 1024, 256)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY and pct == 0.0

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(0, 512, 1024, 256)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY and pct == 0.0

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(0, 0, 512, 768)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY and pct == 0.0

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(768, 0, 256, 768)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY and pct == 0.0

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(512, 256, 256, 256)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(512 + 1, 256 + 2, 3, 4)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0

    flags, pct = ds.GetRasterBand(1).GetDataCoverageStatus(512 - 1, 256 - 1, 2, 2)
    assert (
        flags
        == (gdal.GDAL_DATA_COVERAGE_STATUS_DATA | gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY)
        and pct == 25.0
    )
