#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for PCIDSK driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("PCIDSK")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Read test of floating point reference data.


def test_pcidsk_1():

    tst = gdaltest.GDALTest("PCIDSK", "pcidsk/utm.pix", 1, 39576)
    tst.testOpen()


###############################################################################
# Test lossless copying (16, multiband) via Create().


@pytest.mark.require_driver("PNG")
def test_pcidsk_2():

    tst = gdaltest.GDALTest("PCIDSK", "png/rgba16.png", 2, 2042)

    tst.testCreate()


###############################################################################
# Test copying of georeferencing and projection.


def test_pcidsk_3():

    tst = gdaltest.GDALTest("PCIDSK", "pcidsk/utm.pix", 1, 39576)

    tst.testCreateCopy(check_gt=1, check_srs=1)


###############################################################################
# Test overview reading.


def test_pcidsk_4():

    ds = gdal.Open("data/pcidsk/utm.pix")

    band = ds.GetRasterBand(1)
    assert band.GetOverviewCount() == 1, "did not get expected overview count"

    cs = band.GetOverview(0).Checksum()
    assert cs == 8368, "wrong overview checksum (%d)" % cs


###############################################################################
# Test writing metadata to a newly created file.


def test_pcidsk_5(tmp_path):

    testfile = str(tmp_path / "pcidsk_5.pix")

    # Create testing file.

    pcidsk_ds = gdal.GetDriverByName("PCIDSK").Create(
        testfile, 400, 600, 1, gdal.GDT_UInt8
    )

    # Write out some metadata to the default and non-default domain and
    # using the set and single methods.

    pcidsk_ds.SetMetadata(["ABC=DEF", "GHI=JKL"])
    pcidsk_ds.SetMetadataItem("XXX", "YYY")
    pcidsk_ds.SetMetadataItem("XYZ", "123", "AltDomain")

    # Close and reopen.
    pcidsk_ds = None
    pcidsk_ds = gdal.Open(testfile, gdal.GA_Update)

    # Check metadata.
    mddef = pcidsk_ds.GetMetadata()
    assert mddef["GHI"] == "JKL", "file default domain metadata broken. "
    assert mddef["XXX"] == "YYY", "file default domain metadata broken. "

    assert pcidsk_ds.GetMetadataItem("GHI") == "JKL"
    assert pcidsk_ds.GetMetadataItem("GHI") == "JKL"
    pcidsk_ds.SetMetadataItem("GHI", "JKL2")
    assert pcidsk_ds.GetMetadataItem("GHI") == "JKL2"
    assert pcidsk_ds.GetMetadataItem("I_DONT_EXIST") is None
    assert pcidsk_ds.GetMetadataItem("I_DONT_EXIST") is None

    mdalt = pcidsk_ds.GetMetadata("AltDomain")
    assert mdalt["XYZ"] == "123", "file alt domain metadata broken."

    ###############################################################################
    # Test writing metadata to a band.

    # Write out some metadata to the default and non-default domain and
    # using the set and single methods.
    band = pcidsk_ds.GetRasterBand(1)

    band.SetMetadata(["ABC=DEF", "GHI=JKL"])
    band.SetMetadataItem("XXX", "YYY")
    band.SetMetadataItem("XYZ", "123", "AltDomain")
    band = None

    # Close and reopen.
    pcidsk_ds = None
    pcidsk_ds = gdal.Open(testfile, gdal.GA_Update)

    # Check metadata.
    band = pcidsk_ds.GetRasterBand(1)
    mddef = band.GetMetadata()
    assert mddef["GHI"] == "JKL", "channel default domain metadata broken. "
    assert mddef["XXX"] == "YYY", "channel default domain metadata broken. "

    assert band.GetMetadataItem("GHI") == "JKL"
    assert band.GetMetadataItem("GHI") == "JKL"
    band.SetMetadataItem("GHI", "JKL2")
    assert band.GetMetadataItem("GHI") == "JKL2"
    assert band.GetMetadataItem("I_DONT_EXIST") is None
    assert band.GetMetadataItem("I_DONT_EXIST") is None

    mdalt = band.GetMetadata("AltDomain")
    assert mdalt["XYZ"] == "123", "channel alt domain metadata broken."

    ###############################################################################
    # Test creating a color table and reading it back.

    # Write out some metadata to the default and non-default domain and
    # using the set and single methods.
    band = pcidsk_ds.GetRasterBand(1)

    ct = band.GetColorTable()

    assert ct is None, "Got color table unexpectedly."

    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (0, 255, 0, 255))
    ct.SetColorEntry(1, (255, 0, 255, 255))
    ct.SetColorEntry(2, (0, 0, 255, 255))
    band.SetColorTable(ct)

    ct = band.GetColorTable()

    assert ct.GetColorEntry(1) == (
        255,
        0,
        255,
        255,
    ), "Got wrong color table entry immediately."

    ct = None
    band = None

    # Close and reopen.
    pcidsk_ds = None
    pcidsk_ds = gdal.Open(testfile, gdal.GA_Update)

    band = pcidsk_ds.GetRasterBand(1)

    ct = band.GetColorTable()

    assert ct.GetColorEntry(1) == (
        255,
        0,
        255,
        255,
    ), "Got wrong color table entry after reopen."

    assert band.GetColorInterpretation() == gdal.GCI_PaletteIndex, "Not a palette?"

    assert band.SetColorTable(None) == 0, "SetColorTable failed."

    assert band.GetColorTable() is None, "color table still exists!"

    assert band.GetColorInterpretation() == gdal.GCI_Undefined, "Paletted?"


###############################################################################
# Test FILE interleaving.


@pytest.mark.require_driver("PNG")
def test_pcidsk_8():

    tst = gdaltest.GDALTest(
        "PCIDSK", "png/rgba16.png", 2, 2042, options=["INTERLEAVING=FILE"]
    )

    tst.testCreate()


###############################################################################
# Test that we cannot open a vector only pcidsk
# FIXME: test disabled because of unification


def pcidsk_9():

    ogr_drv = ogr.GetDriverByName("PCIDSK")
    if ogr_drv is None:
        pytest.skip()

    ds = ogr_drv.CreateDataSource("/vsimem/pcidsk_9.pix")
    ds.CreateLayer("foo")
    ds = None

    with gdal.quiet_errors():
        ds = gdal.Open("/vsimem/pcidsk_9.pix")
    assert ds is None
    ds = None

    gdal.Unlink("/vsimem/pcidsk_9.pix")


###############################################################################
# Test overview creation.


def test_pcidsk_10():

    src_ds = gdal.Open("data/byte.tif")
    ds = gdal.GetDriverByName("PCIDSK").CreateCopy("/vsimem/pcidsk_10.pix", src_ds)
    src_ds = None

    # ds = None
    # ds = gdal.Open( '/vsimem/pcidsk_10.pix', gdal.GA_Update )

    band = ds.GetRasterBand(1)
    ds.BuildOverviews("NEAR", [2])

    assert band.GetOverviewCount() == 1, "did not get expected overview count"

    cs = band.GetOverview(0).Checksum()
    assert cs == 1087, "wrong overview checksum (%d)" % cs

    ds = None

    gdal.GetDriverByName("PCIDSK").Delete("/vsimem/pcidsk_10.pix")


###############################################################################
# Test INTERLEAVING=TILED interleaving.


@pytest.mark.require_driver("PNG")
def test_pcidsk_11():

    tst = gdaltest.GDALTest(
        "PCIDSK",
        "png/rgba16.png",
        2,
        2042,
        options=["INTERLEAVING=TILED", "TILESIZE=32"],
    )

    tst.testCreate()


@pytest.mark.require_driver("PNG")
def test_pcidsk_11_v1():

    tst = gdaltest.GDALTest(
        "PCIDSK",
        "png/rgba16.png",
        2,
        2042,
        options=["INTERLEAVING=TILED", "TILESIZE=32", "TILEVERSION=1"],
    )

    tst.testCreate()


@pytest.mark.require_driver("PNG")
def test_pcidsk_11_v2():

    tst = gdaltest.GDALTest(
        "PCIDSK",
        "png/rgba16.png",
        2,
        2042,
        options=["INTERLEAVING=TILED", "TILESIZE=32", "TILEVERSION=2"],
    )

    tst.testCreate()


###############################################################################
# Test INTERLEAVING=TILED interleaving and COMPRESSION=RLE


@pytest.mark.require_driver("PNG")
def test_pcidsk_12():

    tst = gdaltest.GDALTest(
        "PCIDSK",
        "png/rgba16.png",
        2,
        2042,
        options=["INTERLEAVING=TILED", "TILESIZE=32", "COMPRESSION=RLE"],
    )

    tst.testCreate()


@pytest.mark.require_driver("PNG")
def test_pcidsk_12_v1():

    tst = gdaltest.GDALTest(
        "PCIDSK",
        "png/rgba16.png",
        2,
        2042,
        options=[
            "INTERLEAVING=TILED",
            "TILESIZE=32",
            "COMPRESSION=RLE",
            "TILEVERSION=1",
        ],
    )

    tst.testCreate()


@pytest.mark.require_driver("PNG")
def test_pcidsk_12_v2():

    tst = gdaltest.GDALTest(
        "PCIDSK",
        "png/rgba16.png",
        2,
        2042,
        options=[
            "INTERLEAVING=TILED",
            "TILESIZE=32",
            "COMPRESSION=RLE",
            "TILEVERSION=2",
        ],
    )

    tst.testCreate()


###############################################################################
# Test INTERLEAVING=TILED interleaving and COMPRESSION=JPEG


@pytest.mark.require_driver("JPEG")
def test_pcidsk_13():

    src_ds = gdal.Open("data/byte.tif")
    ds = gdal.GetDriverByName("PCIDSK").CreateCopy(
        "/vsimem/pcidsk_13.pix",
        src_ds,
        options=["INTERLEAVING=TILED", "COMPRESSION=JPEG"],
    )
    src_ds = None

    gdal.Unlink("/vsimem/pcidsk_13.pix.aux.xml")

    ds = None
    ds = gdal.Open("/vsimem/pcidsk_13.pix")
    band = ds.GetRasterBand(1)
    band.GetDescription()
    cs = band.Checksum()
    ds = None

    gdal.GetDriverByName("PCIDSK").Delete("/vsimem/pcidsk_13.pix")

    assert cs == 4645, "bad checksum"


###############################################################################
# Test SetDescription()


def test_pcidsk_14():

    ds = gdal.GetDriverByName("PCIDSK").Create("/vsimem/pcidsk_14.pix", 1, 1)
    band = ds.GetRasterBand(1).SetDescription("mydescription")
    del ds

    gdal.Unlink("/vsimem/pcidsk_14.pix.aux.xml")

    ds = None
    ds = gdal.Open("/vsimem/pcidsk_14.pix")
    band = ds.GetRasterBand(1)
    desc = band.GetDescription()
    ds = None

    gdal.GetDriverByName("PCIDSK").Delete("/vsimem/pcidsk_14.pix")

    assert desc == "mydescription", "bad description"


###############################################################################
# Test mixed raster and vector


def test_pcidsk_15(tmp_path):

    # One raster band and vector layer
    ds = gdal.GetDriverByName("PCIDSK").Create(tmp_path / "pcidsk_15.pix", 1, 1)
    ds.CreateLayer("foo")
    ds = None

    ds = gdal.Open(tmp_path / "pcidsk_15.pix")
    assert ds.RasterCount == 1
    assert ds.GetLayerCount() == 1

    ds2 = gdal.GetDriverByName("PCIDSK").CreateCopy(tmp_path / "pcidsk_15_2.pix", ds)
    ds2 = None
    ds = None

    ds = gdal.Open(tmp_path / "pcidsk_15_2.pix")
    assert ds.RasterCount == 1
    assert ds.GetLayerCount() == 1
    ds = None

    # One vector layer only
    ds = gdal.GetDriverByName("PCIDSK").Create(tmp_path / "pcidsk_15.pix", 0, 0, 0)
    ds.CreateLayer("foo")
    ds = None

    ds = gdal.OpenEx(tmp_path / "pcidsk_15.pix")
    assert ds.RasterCount == 0
    assert ds.GetLayerCount() == 1

    ds2 = gdal.GetDriverByName("PCIDSK").CreateCopy(tmp_path / "pcidsk_15_2.pix", ds)
    ds2 = None
    ds = None

    ds = gdal.OpenEx(tmp_path / "pcidsk_15_2.pix")
    assert ds.RasterCount == 0
    assert ds.GetLayerCount() == 1
    ds = None

    # Zero raster band and vector layer
    ds = gdal.GetDriverByName("PCIDSK").Create(tmp_path / "pcidsk_15.pix", 0, 0, 0)
    ds = None

    ds = gdal.OpenEx(tmp_path / "pcidsk_15.pix")
    assert ds.RasterCount == 0
    assert ds.GetLayerCount() == 0

    ds2 = gdal.GetDriverByName("PCIDSK").CreateCopy(tmp_path / "pcidsk_15_2.pix", ds)
    del ds2
    ds = None

    ds = gdal.OpenEx(tmp_path / "pcidsk_15_2.pix")
    assert ds.RasterCount == 0
    assert ds.GetLayerCount() == 0
    ds = None


###############################################################################


def test_pcidsk_external_ovr():

    gdal.Translate("/vsimem/test.pix", "data/byte.tif", format="PCIDSK")
    ds = gdal.Open("/vsimem/test.pix")
    ds.BuildOverviews("NEAR", [2])
    ds = None
    assert gdal.VSIStatL("/vsimem/test.pix.ovr") is not None
    ds = gdal.Open("/vsimem/test.pix")
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    ds = None

    gdal.GetDriverByName("PCIDSK").Delete("/vsimem/test.pix")


###############################################################################


def test_pcidsk_external_ovr_rrd():

    gdal.Translate("/vsimem/test.pix", "data/byte.tif", format="PCIDSK")
    ds = gdal.Open("/vsimem/test.pix", gdal.GA_Update)
    with gdaltest.config_option("USE_RRD", "YES"):
        ds.BuildOverviews("NEAR", [2])
    ds = None
    if gdal.GetLastErrorMsg() == "This build does not support creating .aux overviews":
        pytest.skip(gdal.GetLastErrorMsg())
    assert gdal.VSIStatL("/vsimem/test.aux") is not None
    ds = gdal.Open("/vsimem/test.pix")
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    ds = None

    gdal.GetDriverByName("PCIDSK").Delete("/vsimem/test.pix")


###############################################################################
# Check various items from a modern irvine.pix


def test_pcidsk_online_1():

    gdaltest.download_or_skip(
        "http://download.osgeo.org/gdal/data/pcidsk/sdk_testsuite/irvine_gcp2.pix",
        "irvine_gcp2.pix",
    )

    ds = gdal.Open("tmp/cache/irvine_gcp2.pix")

    band = ds.GetRasterBand(6)

    names = band.GetRasterCategoryNames()

    exp_names = [
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "Residential",
        "Commercial",
        "Industrial",
        "Transportation",
        "Commercial/Industrial",
        "Mixed",
        "Other",
        "",
        "",
        "",
        "Crop/Pasture",
        "Orchards",
        "Feeding",
        "Other",
        "",
        "",
        "",
        "",
        "",
        "",
        "Herbaceous",
        "Shrub",
        "Mixed",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "Deciduous",
        "Evergreen",
        "Mixed",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "Streams/Canals",
        "Lakes",
        "Reservoirs",
        "Bays/Estuaries",
        "",
        "",
        "",
        "",
        "",
        "",
        "Forested",
        "Nonforested",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "Dry_Salt_Flats",
        "Beaches",
        "Sandy_Areas",
        "Exposed_Rock",
        "Mines/Quarries/Pits",
        "Transitional_Area",
        "Mixed",
        "",
        "",
        "",
        "Shrub/Brush",
        "Herbaceous",
        "Bare",
        "Wet",
        "Mixed",
        "",
        "",
        "",
        "",
        "",
        "Perennial_Snow",
        "Glaciers",
    ]

    assert names == exp_names, "did not get expected category names."

    band = ds.GetRasterBand(20)
    assert (
        band.GetDescription() == "Training site for type 2 crop"
    ), "did not get expected band 20 description"

    exp_checksum = 2057
    checksum = band.Checksum()
    assert exp_checksum == checksum, "did not get right bitmap checksum."

    md = band.GetMetadata("IMAGE_STRUCTURE")
    assert md["NBITS"] == "1", "did not get expected NBITS=1 metadata."


###############################################################################
# Read test of a PCIDSK TILED version 1 file.


def test_pcidsk_tile_v1():

    tst = gdaltest.GDALTest("PCIDSK", "pcidsk/tile_v1.1.pix", 1, 49526)

    tst.testCreateCopy(check_gt=1, check_srs=1)


def test_pcidsk_tile_v1_overview():

    ds = gdal.Open("data/pcidsk/tile_v1.1.pix")

    band = ds.GetRasterBand(1)
    assert band.GetOverviewCount() == 1, "did not get expected overview count"

    cs = band.GetOverview(0).Checksum()
    assert cs == 12003, "wrong overview checksum (%d)" % cs


###############################################################################
# Read test of a PCIDSK TILED version 2 file.


def test_pcidsk_tile_v2():

    tst = gdaltest.GDALTest("PCIDSK", "pcidsk/tile_v2.pix", 1, 49526)

    return tst.testCreateCopy(check_gt=1, check_srs=1)


def test_pcidsk_tile_v2_overview():

    ds = gdal.Open("data/pcidsk/tile_v2.pix")

    band = ds.GetRasterBand(1)
    assert band.GetOverviewCount() == 1, "did not get expected overview count"

    cs = band.GetOverview(0).Checksum()
    assert cs == 12003, "wrong overview checksum (%d)" % cs


###############################################################################
# Test RPC


def test_pcidsk_online_rpc():

    gdaltest.download_or_skip(
        "https://github.com/OSGeo/gdal/files/6822835/pix-test.zip", "pix-test.zip"
    )

    try:
        os.stat("tmp/cache/demo.PIX")
    except OSError:
        try:
            gdaltest.unzip("tmp/cache", "tmp/cache/pix-test.zip")
        except Exception:
            pytest.skip()

    ds = gdal.Open("tmp/cache/demo.PIX")
    assert ds.GetMetadata("RPC") is not None


###############################################################################
# Test opening invalid files


@pytest.mark.parametrize(
    "filename", ["data/pcidsk/invalid_segment_pointers_offset.pix"]
)
def test_pcidsk_invalid_files(filename):

    with gdal.quiet_errors():
        assert gdal.VSIStatL(filename) is not None
        assert gdal.Open(filename) is None


###############################################################################
# Test Web Mercator support


def test_pcidsk_web_mercator(tmp_path):

    gdal.Translate(
        f"{tmp_path}/test_pcidsk_web_mercator.pix",
        "data/byte.tif",
        options="-of PCIDSK -a_srs EPSG:3857",
    )
    gdal.Unlink(f"{tmp_path}/test_pcidsk_web_mercator.pix.aux.xml")
    ds = gdal.Open(f"{tmp_path}/test_pcidsk_web_mercator.pix")
    expected_srs = osr.SpatialReference()
    expected_srs.ImportFromEPSG(3857)
    assert ds.GetSpatialRef().IsSame(expected_srs)
