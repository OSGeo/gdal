#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read and write functionality for Esri Compact Cache driver.
# Author:
#   Reader: Lucian Plesea <lplesea at esri.com>
#   Writer: Husam Mohammad <husam.mohammad at safe.com
#
###############################################################################
# Copyright (c) 2020, Esri
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import re

import pytest

from osgeo import gdal, osr

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


###############################################################################
#
# Writer tests
#
###############################################################################


def _read_json_from_tpkx(tpkx_path, json_file):
    """Read and parse a JSON file from inside a .tpkx ZIP archive."""
    f = gdal.VSIFOpenL(f"/vsizip/{{{tpkx_path}}}/{json_file}", "rb")
    assert f is not None, f"failed to open {json_file} in {tpkx_path}"
    data = gdal.VSIFReadL(1, 200000, f)
    gdal.VSIFCloseL(f)
    return json.loads(data)


###############################################################################
# Basic round-trip tests


@pytest.mark.require_driver("JPEG")
def test_esric_write_jpeg_roundtrip(tmp_path):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=JPEG", "MAX_LOD=0"]
    )
    assert ds is not None, "CreateCopy returned None"
    ds = None

    ds = gdal.Open(out)
    assert ds is not None, "failed to reopen written tpkx"
    assert ds.RasterCount == 3, "Expected 3 bands"
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "4326"
    assert ds.GetRasterBand(1).GetBlockSize() == [256, 256]
    gt = ds.GetGeoTransform()
    assert gt[2] == 0 and gt[4] == 0, "should have no rotation"
    assert gt[1] > 0, "pixel width should be positive"
    assert gt[5] < 0, "pixel height should be negative (north-up)"
    assert ds.GetRasterBand(1).Checksum() == 38702
    assert ds.GetRasterBand(2).Checksum() == 43101
    assert ds.GetRasterBand(3).Checksum() == 20103


@pytest.mark.require_driver("PNG")
def test_esric_write_png_roundtrip(tmp_path):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=PNG", "MAX_LOD=0"]
    )
    assert ds is not None, "CreateCopy returned None"
    ds = None

    ds = gdal.Open(out)
    assert ds is not None, "failed to reopen written tpkx"
    assert ds.RasterCount == 4
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "4326"
    assert ds.GetRasterBand(1).Checksum() == 42255
    assert ds.GetRasterBand(2).Checksum() == 47336
    assert ds.GetRasterBand(3).Checksum() == 24965
    assert ds.GetRasterBand(4).Checksum() == 35707


@pytest.mark.require_driver("PNG")
def test_esric_write_from_existing_tpkx(tmp_path):
    src = gdal.Open("data/esric/Usa.tpkx")
    assert src is not None
    out = str(tmp_path / "copy.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src, options=["TILE_FORMAT=PNG", "MIN_LOD=0", "MAX_LOD=1"]
    )
    assert ds is not None, "CreateCopy returned None"
    ds = None

    ds = gdal.Open(out)
    assert ds is not None, "failed to reopen copied tpkx"
    assert ds.RasterCount == 4
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "3857"
    assert ds.GetRasterBand(1).Checksum() == 27310
    assert ds.GetRasterBand(2).Checksum() == 28862
    assert ds.GetRasterBand(3).Checksum() == 27977
    assert ds.GetRasterBand(4).Checksum() == 52046


###############################################################################
# Creation option tests


@pytest.mark.require_driver("JPEG")
@pytest.mark.parametrize("quality, output_quality", [(-5, 1), (55, 55), (110, 100)])
def test_esric_write_quality(tmp_path, quality, output_quality):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=JPEG", f"QUALITY={quality}", "MAX_LOD=0"]
    )
    assert ds is not None
    ds = None

    root = _read_json_from_tpkx(out, "root.json")
    assert root["tileImageInfo"]["compression"] == output_quality

    ds = gdal.Open(out)
    assert ds.GetRasterBand(1).Checksum() != 0


@pytest.mark.require_driver("PNG")
@pytest.mark.parametrize(
    "min_lod,max_lod",
    [(0, 0), (0, 1), (1, 3)],
)
def test_esric_write_min_max_lod(tmp_path, min_lod, max_lod):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out,
        src_ds,
        options=["TILE_FORMAT=PNG", f"MIN_LOD={min_lod}", f"MAX_LOD={max_lod}"],
    )
    assert ds is not None
    ds = None

    root = _read_json_from_tpkx(out, "root.json")
    assert root["minLOD"] == min_lod
    assert root["maxLOD"] == max_lod
    assert len(root["tileInfo"]["lods"]) == max_lod - min_lod + 1


@pytest.mark.require_driver("PNG")
def test_esric_write_summary(tmp_path):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out,
        src_ds,
        options=["TILE_FORMAT=PNG", "MAX_LOD=0", "SUMMARY=Test summary text"],
    )
    assert ds is not None
    ds = None

    info = _read_json_from_tpkx(out, "iteminfo.json")
    assert info["summary"] == "Test summary text"


@pytest.mark.require_driver("PNG")
def test_esric_write_tags(tmp_path):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=PNG", "MAX_LOD=0", "TAGS=tag1,tag2,tag3"]
    )
    assert ds is not None
    ds = None

    info = _read_json_from_tpkx(out, "iteminfo.json")
    assert info["tags"] == ["tag1", "tag2", "tag3"]


@pytest.mark.require_driver("PNG")
def test_esric_write_tags_trimmed(tmp_path):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out,
        src_ds,
        options=["TILE_FORMAT=PNG", "MAX_LOD=0", "TAGS= tag1 , tag2 , tag3 "],
    )
    assert ds is not None
    ds = None

    info = _read_json_from_tpkx(out, "iteminfo.json")
    assert info["tags"] == ["tag1", "tag2", "tag3"]


###############################################################################
# Input band variations


@pytest.mark.require_driver("JPEG")
def test_esric_write_1band_jpeg(tmp_path):
    src_ds = gdal.GetDriverByName("MEM").Create("", 256, 256, 1, gdal.GDT_Byte)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([-20037508.34, 156543.03, 0, 20037508.34, 0, -156543.03])
    src_ds.GetRasterBand(1).Fill(128)
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=JPEG", "MAX_LOD=0"]
    )
    assert ds is not None
    ds = None

    ds = gdal.Open(out)
    assert ds is not None
    assert ds.RasterCount == 3, "output should have 3 bands"
    assert ds.GetRasterBand(1).Checksum() == 23809
    assert ds.GetRasterBand(2).Checksum() == 23809
    assert ds.GetRasterBand(3).Checksum() == 23809


@pytest.mark.require_driver("PNG")
def test_esric_write_1band_png(tmp_path):
    src_ds = gdal.GetDriverByName("MEM").Create("", 256, 256, 1, gdal.GDT_Byte)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([-20037508.34, 156543.03, 0, 20037508.34, 0, -156543.03])
    src_ds.GetRasterBand(1).Fill(128)
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=PNG", "MAX_LOD=0"]
    )
    assert ds is not None
    ds = None

    ds = gdal.Open(out)
    assert ds is not None
    assert ds.RasterCount == 4, "output should have 4 bands"
    assert ds.GetRasterBand(1).Checksum() == 23809
    assert ds.GetRasterBand(2).Checksum() == 23809
    assert ds.GetRasterBand(3).Checksum() == 23809
    assert ds.GetRasterBand(4).Checksum() == 17849


@pytest.mark.require_driver("JPEG")
def test_esric_write_4band_rgba(tmp_path):
    src_ds = gdal.GetDriverByName("MEM").Create("", 256, 256, 4, gdal.GDT_Byte)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([-20037508.34, 156543.03, 0, 20037508.34, 0, -156543.03])
    for i in range(4):
        src_ds.GetRasterBand(i + 1).Fill([100, 150, 200, 255][i])
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=JPEG", "MAX_LOD=0"]
    )
    assert ds is not None
    ds = None

    ds = gdal.Open(out)
    assert ds is not None
    assert ds.RasterCount == 3, "output should have 3 bands"
    assert ds.GetRasterBand(1).Checksum() == 47652
    assert ds.GetRasterBand(2).Checksum() == 53598
    assert ds.GetRasterBand(3).Checksum() == 23798


@pytest.mark.require_driver("JPEG")
def test_esric_write_palette_jpeg(tmp_path):
    src_ds = gdal.Open("data/small_world_pct.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=JPEG", "MAX_LOD=0"]
    )
    assert ds is not None
    ds = None

    ds = gdal.Open(out)
    assert ds is not None
    assert ds.RasterCount == 3, "output should have 3 bands"
    assert ds.GetRasterBand(1).Checksum() == 57466
    assert ds.GetRasterBand(2).Checksum() == 50487
    assert ds.GetRasterBand(3).Checksum() == 39098


@pytest.mark.require_driver("PNG")
def test_esric_write_palette_png(tmp_path):
    src_ds = gdal.Open("data/small_world_pct.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=PNG", "MAX_LOD=0"]
    )
    assert ds is not None
    ds = None

    ds = gdal.Open(out)
    assert ds is not None
    assert ds.RasterCount == 4, "output should have 4 bands"
    assert ds.GetRasterBand(1).Checksum() == 13859
    assert ds.GetRasterBand(2).Checksum() == 36640
    assert ds.GetRasterBand(3).Checksum() == 29677
    assert ds.GetRasterBand(4).Checksum() == 35707


###############################################################################
# Metadata JSON verification


@pytest.mark.require_driver("PNG")
def test_esric_write_root_json_structure(tmp_path):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=PNG", "MAX_LOD=0"]
    )
    assert ds is not None
    ds = None

    root = _read_json_from_tpkx(out, "root.json")
    for key in [
        "name",
        "version",
        "tileBundlesPath",
        "minLOD",
        "maxLOD",
        "spatialReference",
        "tileInfo",
        "storageInfo",
        "tileImageInfo",
        "fullExtent",
        "initialExtent",
    ]:
        assert key in root, f"root.json missing key '{key}'"

    assert root["version"] == "1.0"
    assert root["tileBundlesPath"] == "tile"
    assert root["storageInfo"]["packetSize"] == 128
    assert root["storageInfo"]["storageFormat"] == "esriMapCacheStorageModeCompactV2"


@pytest.mark.require_driver("PNG")
def test_esric_write_root_json_tile_info(tmp_path):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=PNG", "MAX_LOD=0"]
    )
    assert ds is not None
    ds = None

    root = _read_json_from_tpkx(out, "root.json")
    ti = root["tileInfo"]
    assert ti["rows"] == 256
    assert ti["cols"] == 256
    assert ti["dpi"] == 96
    assert ti["origin"]["x"] == pytest.approx(-180, abs=1)
    assert ti["origin"]["y"] == pytest.approx(90, abs=1)


@pytest.mark.require_driver("PNG")
def test_esric_write_root_json_extent(tmp_path):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=PNG", "MAX_LOD=0"]
    )
    assert ds is not None
    ds = None

    root = _read_json_from_tpkx(out, "root.json")
    for extent_key in ["fullExtent", "initialExtent"]:
        ext = root[extent_key]
        assert ext["xmin"] < ext["xmax"], f"{extent_key} xmin >= xmax"
        assert ext["ymin"] < ext["ymax"], f"{extent_key} ymin >= ymax"
        assert "spatialReference" in ext


@pytest.mark.require_driver("PNG")
def test_esric_write_iteminfo_json_structure(tmp_path):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=PNG", "MAX_LOD=0"]
    )
    assert ds is not None
    ds = None

    info = _read_json_from_tpkx(out, "iteminfo.json")
    for key in ["name", "version", "guid", "created", "title", "type", "typekeywords"]:
        assert key in info, f"iteminfo.json missing key '{key}'"

    assert info["type"] == "Compact Tile Package"
    assert "tpkx" in info["typekeywords"]
    assert "Tile Package" in info["typekeywords"]
    assert info["name"] == "out"
    assert info["title"] == "out"


###############################################################################
# Error handling


def test_esric_write_error_too_many_bands(tmp_path):
    src = gdal.GetDriverByName("MEM").Create("", 256, 256, 5, gdal.GDT_Byte)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    src.SetSpatialRef(srs)
    src.SetGeoTransform([-20037508.34, 156543.03, 0, 20037508.34, 0, -156543.03])
    out = str(tmp_path / "out.tpkx")
    with pytest.raises(RuntimeError):
        gdal.GetDriverByName("ESRIC").CreateCopy(out, src)


@pytest.mark.parametrize("epsg", [4269, 32632])
def test_esric_write_error_wrong_srs(tmp_path, epsg):
    src = gdal.GetDriverByName("MEM").Create("", 256, 256, 3, gdal.GDT_Byte)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(epsg)
    src.SetSpatialRef(srs)
    src.SetGeoTransform([0, 1, 0, 0, 0, -1])
    out = str(tmp_path / "out.tpkx")
    with pytest.raises(RuntimeError):
        gdal.GetDriverByName("ESRIC").CreateCopy(out, src)


def test_esric_write_error_no_srs(tmp_path):
    drv = gdal.GetDriverByName("MEM")
    src = drv.Create("", 256, 256, 3, gdal.GDT_Byte)
    src.SetGeoTransform([0, 1, 0, 0, 0, -1])
    for i in range(3):
        src.GetRasterBand(i + 1).Fill(128)
    out = str(tmp_path / "out.tpkx")
    with pytest.raises(RuntimeError):
        gdal.GetDriverByName("ESRIC").CreateCopy(out, src)


@pytest.mark.require_driver("JPEG")
def test_esric_write_error_min_exceeds_max_lod(tmp_path):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    with pytest.raises(RuntimeError):
        gdal.GetDriverByName("ESRIC").CreateCopy(
            out, src_ds, options=["TILE_FORMAT=JPEG", "MIN_LOD=5", "MAX_LOD=3"]
        )


###############################################################################
# Miscellaneous


@pytest.mark.require_driver("PNG")
def test_esric_write_zip_contents(tmp_path):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=PNG", "MIN_LOD=0", "MAX_LOD=1"]
    )
    assert ds is not None
    ds = None

    files = gdal.ReadDirRecursive(f"/vsizip/{{{out}}}")
    assert files is not None, "failed to list ZIP contents"
    files_lower = [f.lower() for f in files]
    assert "root.json" in files_lower, "root.json not found in ZIP"
    assert "iteminfo.json" in files_lower, "iteminfo.json not found in ZIP"
    # At least one bundle in L00 and L01
    assert any("tile/l00/" in f and f.endswith(".bundle") for f in files_lower)
    assert any("tile/l01/" in f and f.endswith(".bundle") for f in files_lower)

    bundles = [f for f in files if f.endswith(".bundle")]
    for b in bundles:
        basename = b.split("/")[-1]
        assert re.match(
            r"^R[0-9a-fA-F]{4}C[0-9a-fA-F]{4}\.bundle$", basename
        ), f"bundle name does not match expected pattern: {basename}"


@pytest.mark.require_driver("PNG")
def test_esric_write_nodata_from_source(tmp_path):
    src = gdal.GetDriverByName("MEM").Create("", 256, 256, 3, gdal.GDT_Byte)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)
    src.SetSpatialRef(srs)
    src.SetGeoTransform([-20037508.34, 156543.03, 0, 20037508.34, 0, -156543.03])
    for i in range(3):
        band = src.GetRasterBand(i + 1)
        band.Fill(128)
        band.SetNoDataValue(0)
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src, options=["TILE_FORMAT=PNG", "MAX_LOD=0"]
    )
    assert ds is not None, "CreateCopy should succeed with NoData"
    ds = None

    ds = gdal.Open(out)
    assert ds is not None


@pytest.mark.require_driver("PNG")
def test_esric_write_output_file_list(tmp_path):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out, src_ds, options=["TILE_FORMAT=PNG", "MAX_LOD=0"]
    )
    assert ds is not None
    ds = None

    ds = gdal.Open(out)
    assert ds is not None
    file_list = ds.GetFileList()
    assert file_list is not None
    assert any(f.endswith("out.tpkx") for f in file_list)


@pytest.mark.require_driver("JPEG")
def test_esric_write_progress_callback(tmp_path):
    src_ds = gdal.Open("data/small_world.tif")
    out = str(tmp_path / "out.tpkx")
    progress_values = []

    def progress_cb(pct, msg, data):
        data.append(pct)
        return 1  # continue

    ds = gdal.GetDriverByName("ESRIC").CreateCopy(
        out,
        src_ds,
        options=["TILE_FORMAT=JPEG", "MAX_LOD=0"],
        callback=progress_cb,
        callback_data=progress_values,
    )
    assert ds is not None
    ds = None

    assert len(progress_values) > 0, "progress callback was never called"
    assert progress_values[-1] == pytest.approx(1.0, abs=0.01)
