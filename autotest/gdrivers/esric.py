#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for Esri Compact Cache driver.
# Author:   Lucian Plesea <lplesea at esri.com>
#
###############################################################################
# Copyright (c) 2020, Esri
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("ESRIC")

###############################################################################
# Open the dataset


@pytest.fixture()
def esric_ds():

    ds = gdal.Open("/vsitar/data/esric/Layers.tar/Layers/conf.xml")
    assert ds is not None, "open failed"

    return ds


###############################################################################
# Check that the configuration was read as expected


def test_esric_2(esric_ds):

    ds = esric_ds
    b1 = ds.GetRasterBand(1)

    assert (
        ds.RasterCount == 4 and ds.RasterXSize == 2048 and ds.RasterYSize == 2048
    ), "wrong size or band count"

    assert b1.GetOverviewCount() == 3, "Wrong number of overviews"

    wkt = ds.GetProjectionRef()
    assert 'AUTHORITY["EPSG","3857"]' in wkt, "wrong SRS"

    gt = ds.GetGeoTransform()
    assert gt[0] == pytest.approx(-20037508, abs=1), "wrong geolocation"
    assert gt[1] == pytest.approx(20037508 / 1024, abs=1), "wrong geolocation"
    assert gt[2] == 0 and gt[4] == 0, "wrong geolocation"
    assert gt[3] == pytest.approx(20037508, abs=1), "wrong geolocation"
    assert gt[5] == pytest.approx(-20037508 / 1024, abs=1), "wrong geolocation"


###############################################################################
# Check that the read a missing level generates black


def test_esric_3(esric_ds):

    ds = esric_ds
    # There are no tiles at this level, driver will return black
    b1 = ds.GetRasterBand(1)
    cs = b1.Checksum()
    assert cs == 0, "wrong checksum from missing level"


###############################################################################
# Check that the read of PNG tiles returns the right checksum


@pytest.mark.require_driver("PNG")
def test_esric_4(esric_ds):

    ds = esric_ds

    # Read from level 1, band 2, where we have data
    # Overviews are counted from zero, in reverse order from levels

    l1b2 = ds.GetRasterBand(2).GetOverview(1)
    assert l1b2.XSize == 512 and l1b2.YSize == 512

    # There are four PNG tiles at this level, one is grayscale

    cs = l1b2.Checksum()
    expectedcs = 46857
    assert cs == expectedcs, "wrong data checksum"


###############################################################################
# Open the tpkx dataset


@pytest.fixture
def tpkx_ds_extent_source_tiling_scheme():
    return gdal.OpenEx(
        "data/esric/Usa.tpkx", open_options=["EXTENT_SOURCE=TILING_SCHEME"]
    )


###############################################################################
# Check that the configuration was read as expected


def test_tpkx_2(tpkx_ds_extent_source_tiling_scheme):
    ds = tpkx_ds_extent_source_tiling_scheme
    b1 = ds.GetRasterBand(1)

    assert (
        ds.RasterCount == 4 and ds.RasterXSize == 8192 and ds.RasterYSize == 8192
    ), "wrong size or band count"

    assert b1.GetOverviewCount() == 5, "Wrong number of overviews"

    wkt = ds.GetProjectionRef()
    assert 'AUTHORITY["EPSG","3857"]' in wkt, "wrong SRS"

    gt = ds.GetGeoTransform()
    assert gt[0] == pytest.approx(-20037508, abs=1), "wrong geolocation"
    assert gt[1] == pytest.approx(20037508 / 4096, abs=1), "wrong geolocation"
    assert gt[2] == 0 and gt[4] == 0, "wrong geolocation"
    assert gt[3] == pytest.approx(20037508, abs=1), "wrong geolocation"
    assert gt[5] == pytest.approx(-20037508 / 4096, abs=1), "wrong geolocation"


###############################################################################
# Check that the raster returns right checksums


def test_tpkx_3(tpkx_ds_extent_source_tiling_scheme):
    ds = tpkx_ds_extent_source_tiling_scheme
    # There are no tiles at this level, driver will return black
    b1 = ds.GetRasterBand(1)
    b2 = ds.GetRasterBand(2)
    b3 = ds.GetRasterBand(3)
    b4 = ds.GetRasterBand(4)
    cs1 = b1.Checksum()
    cs2 = b2.Checksum()
    cs3 = b3.Checksum()
    cs4 = b4.Checksum()
    assert cs1 == 61275, "wrong checksum at band 1"
    assert cs2 == 57672, "wrong checksum at band 2"
    assert cs3 == 61542, "wrong checksum at band 3"
    assert cs4 == 19476, "wrong checksum at band 4"


###############################################################################
# Check that the read of PNG tiles returns the right checksum


@pytest.mark.require_driver("PNG")
def test_tpkx_4(tpkx_ds_extent_source_tiling_scheme):
    ds = tpkx_ds_extent_source_tiling_scheme

    # Read from level 1, band 2, where we have data
    # Overviews are counted from zero, in reverse order from levels

    l1b2 = ds.GetRasterBand(2).GetOverview(1)
    assert l1b2.XSize == 2048 and l1b2.YSize == 2048

    # There are four PNG tiles at this level, one is grayscale

    cs = l1b2.Checksum()
    expectedcs = 53503
    assert cs == expectedcs, "wrong data checksum"


###############################################################################
# Open a tpkx dataset where we need to ingest more bytes


def test_tpkx_ingest_more_bytes(tmp_vsimem):
    filename = str(tmp_vsimem / "root.json")
    f = gdal.VSIFOpenL("/vsizip/{data/esric/Usa.tpkx}/root.json", "rb")
    assert f
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    # Append spaces at the beginning of the root.json file to test we try
    # to ingest more bytes
    data = b"{" + (b" " * 900) + data[1:]
    gdal.FileFromMemBuffer(filename, data)
    gdal.Open(filename)


###############################################################################
# Open a tpkx dataset where minLOD > 0


def test_tpkx_minLOD_not_zero():
    ds = gdal.Open("data/esric/Usa_lod5.tpkx")
    gt = ds.GetGeoTransform()
    # Corresponds to lon=-100 lat=40
    X = -11131949
    Y = 4865942
    x = (X - gt[0]) / gt[1]
    y = (Y - gt[3]) / gt[5]
    assert ds.GetRasterBand(1).ReadRaster(x, y, 1, 1) != b"\0"


###############################################################################
# Test opening a tpkx file with fullExtent / initialExtent


@pytest.mark.parametrize("extent_source", [None, "INITIAL_EXTENT", "FULL_EXTENT"])
def test_tpkx_default_full_extent(extent_source):
    open_options = {}
    if extent_source:
        open_options["EXTENT_SOURCE"] = extent_source
    ds = gdal.OpenEx("data/esric/Usa.tpkx", open_options=open_options)
    assert ds.RasterXSize == 2533
    assert ds.RasterYSize == 1922
    assert ds.RasterCount == 4
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "3857"
    assert ds.GetGeoTransform() == pytest.approx(
        (
            -19841829.550377003848553,
            4891.969810249979673,
            0,
            11545048.752193037420511,
            0,
            -4891.969810249979673,
        )
    )
    assert ds.GetDriver().GetDescription() == "ESRIC"
    assert ds.GetFileList() == ["data/esric/Usa.tpkx"]
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
    assert ds.GetRasterBand(1).GetBlockSize() == [256, 256]
    assert ds.GetRasterBand(1).Checksum() == 59047
    assert ds.GetRasterBand(1).GetOverviewCount() == 3


###############################################################################
# Test that LODs that exceeds GDAL raster size limit is ignored


@pytest.mark.parametrize("ignore_oversized_lods", [None, "YES", "NO"])
def test_tpkx_ignores_oversized_lod(ignore_oversized_lods):
    open_options = {}
    if ignore_oversized_lods is not None:
        open_options["IGNORE_OVERSIZED_LODS"] = ignore_oversized_lods

    if ignore_oversized_lods in (None, "NO"):
        with pytest.raises(RuntimeError):
            ds = gdal.OpenEx(
                "data/esric/oversizedLOD/root.json", open_options=open_options
            )
        return

    ds = gdal.OpenEx("data/esric/oversizedLOD/root.json", open_options=open_options)
    assert ds is not None, "Dataset failed to open"
    assert ds.RasterXSize == 2147483647
    assert ds.RasterYSize == 2147483647


@pytest.mark.parametrize("ignore_oversized_lods", [None, "YES", "NO"])
def test_esric_ignores_oversized_lod(ignore_oversized_lods):
    open_options = {}
    if ignore_oversized_lods is not None:
        open_options["IGNORE_OVERSIZED_LODS"] = ignore_oversized_lods

    if ignore_oversized_lods in (None, "NO"):
        with pytest.raises(RuntimeError):
            gdal.OpenEx("data/esric/oversizedLOD/conf.xml", open_options=open_options)
        return

    ds = gdal.OpenEx("data/esric/oversizedLOD/conf.xml", open_options=open_options)
    assert ds is not None, "Dataset failed to open"
    assert ds.RasterXSize == 2147483647
    assert ds.RasterYSize == 2147483647
