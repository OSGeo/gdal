#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test PMTiles driver vector functionality.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2023, Planet Labs
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import os

import gdaltest
import ogrtest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("PMTiles")


###############################################################################


def test_ogr_pmtiles_read_basic():

    ds = ogr.Open("data/pmtiles/poly.pmtiles")
    assert ds.GetLayerCount() == 1
    assert ds.GetMetadata() == {
        "description": "",
        "format": "pbf",
        "maxzoom": "5",
        "minzoom": "0",
        "name": "poly",
        "scheme": "tms",
        "type": "overlay",
        "version": "2",
        "ZOOM_LEVEL": "5",
    }
    assert ds.GetLayer(-1) is None
    assert ds.GetLayer(1) is None
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbMultiPolygon
    assert lyr.GetFeatureCount() == 8
    assert lyr.GetExtent() == pytest.approx(
        (
            304325.6246808182,
            308876.1762213128,
            5314763.0069798315,
            5318507.966831126,
        )
    )
    assert lyr.GetSpatialRef().GetAuthorityCode(None) == "3857"
    assert len([f for f in lyr]) == 8
    assert lyr.GetNextFeature() is None
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f["AREA"] == 215229.266
    assert f["EAS_ID"] == 168
    assert f["PRFEDEA"] == "35043411"
    assert f.GetGeometryRef().GetGeometryType() == ogr.wkbMultiPolygon


###############################################################################


def test_ogr_pmtiles_read_JSON_FIELD():

    ds = gdal.OpenEx("data/pmtiles/poly.pmtiles", open_options=["JSON_FIELD=YES"])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["json"] == """{
  "AREA":215229.266,
  "EAS_ID":168,
  "PRFEDEA":"35043411"
}"""


###############################################################################


def test_ogr_pmtiles_read_ZOOM_LEVEL():

    ds = gdal.OpenEx("data/pmtiles/poly.pmtiles", open_options=["ZOOM_LEVEL=0"])
    assert ds.GetMetadataItem("ZOOM_LEVEL") == "0"
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1
    assert lyr.GetExtent() == pytest.approx(
        (
            304325.6246808182,
            308876.1762213128,
            5314763.0069798315,
            5318507.966831126,
        )
    )


###############################################################################


@pytest.mark.parametrize("zoom_level", [-1, 6])
def test_ogr_pmtiles_read_ZOOM_LEVEL_invalid(zoom_level):

    with pytest.raises(Exception, match="Invalid zoom level"):
        gdal.OpenEx(
            "data/pmtiles/poly.pmtiles", open_options=["ZOOM_LEVEL=" + str(zoom_level)]
        )


###############################################################################


@pytest.mark.parametrize(
    "clip,envelope",
    [
        (
            "YES",
            (
                -216469.66410361847,
                0,
                6261721.357121638,
                6399002.259921815,
            ),
        ),
        (
            "NO",
            (
                -398695.5395354787,
                24459.84905125713,
                6237261.508070381,
                6399002.259921815,
            ),
        ),
    ],
)
def test_ogr_pmtiles_read_CLIP(clip, envelope):

    ds = gdal.OpenEx(
        "data/pmtiles/ne_10m_admin_0_france.pmtiles", open_options=["CLIP=" + clip]
    )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if not (clip == "YES" and not ogrtest.have_geos()):
        assert f.GetGeometryRef().GetEnvelope() == pytest.approx(
            envelope, abs=1e-5, rel=1e-5
        )


###############################################################################


@pytest.mark.parametrize(
    "zoom_level_auto,fc",
    [
        (
            "YES",
            16,
        ),
        (
            "NO",
            19,
        ),
    ],
)
@pytest.mark.parametrize("iterator_threshold", [None, "1"])
def test_ogr_pmtiles_read_ZOOM_LEVEL_AUTO(zoom_level_auto, fc, iterator_threshold):

    ds = gdal.OpenEx(
        "data/pmtiles/ne_10m_admin_0_france.pmtiles",
        open_options=["ZOOM_LEVEL_AUTO=" + zoom_level_auto],
    )
    lyr = ds.GetLayer(0)
    minx, maxx, miny, maxy = lyr.GetExtent()
    extent = max(maxx - minx, maxy - miny)
    lyr.SetSpatialFilterRect(
        minx - 10 * extent, miny - 10 * extent, maxx + 10 * extent, maxy + 10 * extent
    )
    with gdaltest.config_option("OGR_PMTILES_ITERATOR_THRESHOLD", iterator_threshold):
        assert lyr.GetFeatureCount() == fc


###############################################################################
# Run test_ogrsf


@pytest.mark.skipif(
    test_cli_utilities.get_test_ogrsf_path() is None, reason="test_ogrsf not available"
)
@pytest.mark.parametrize(
    "filename",
    [
        "data/pmtiles/poly.pmtiles",
        "data/pmtiles/poly_with_leaf_dir.pmtiles",
        "data/pmtiles/ne_10m_admin_0_france.pmtiles",
        "data/pmtiles/ne_10m_admin_0_france_with_leaf_dir.pmtiles",
    ],
)
def test_ogr_pmtiles_test_ogrsf(filename):

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro " + filename
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Read a crazy file with run_length = uint32_max


def test_ogr_pmtiles_run_length_max():

    ds = ogr.Open("data/pmtiles/run_length_max.pmtiles")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == (1 << 32)
    assert lyr.GetGeomType() == ogr.wkbPolygon
    minx, maxx, miny, maxy = lyr.GetExtent()
    f = lyr.GetNextFeature()
    assert f
    f = lyr.GetNextFeature()
    assert f

    assert lyr.GetFeature(0)
    assert lyr.GetFeature((1 << 32) - 1)
    assert lyr.GetFeature((1 << 32)) is None

    lyr.SetSpatialFilterRect(0, 0, 1000, 1000)
    assert lyr.GetFeatureCount() == 4

    lyr.SetSpatialFilterRect(-500, -500, 500, 500)
    assert lyr.GetFeatureCount() == 4

    for x in (minx, maxx):
        for y in (miny, maxy):

            lyr.SetSpatialFilterRect(x - 500, y - 500, x + 500, y + 500)
            assert lyr.GetFeatureCount() == 1

            lyr.SetSpatialFilterRect(x - 5000, y - 5000, x + 5000, y + 5000)
            assert lyr.GetFeatureCount() == 64


###############################################################################
def test_ogr_pmtiles_vsipmtiles():

    filename = "data/pmtiles/ne_10m_admin_0_france_with_leaf_dir.pmtiles"
    assert gdal.ReadDir("/vsipmtiles/" + filename) == [
        "pmtiles_header.json",
        "metadata.json",
        "3",
        "4",
        "5",
    ]

    assert gdal.ReadDir("/vsipmtiles/" + filename + "/0") is None
    assert gdal.ReadDir("/vsipmtiles/" + filename + "/3invalid") is None
    assert gdal.ReadDir("/vsipmtiles/" + filename + "/3") == ["3", "4"]
    assert gdal.ReadDir("/vsipmtiles/" + filename + "/3/4invalid") is None
    assert gdal.ReadDir("/vsipmtiles/" + filename + "/3/4") == ["2.mvt"]
    assert gdal.ReadDir("/vsipmtiles/" + filename + "/3/4/2.mvt") is None
    assert gdal.ReadDir("/vsipmtiles/" + filename + "/3/4/2") is None
    assert gdal.ReadDir("/vsipmtiles/" + filename + "/3/40") is None
    assert gdal.ReadDir("/vsipmtiles/" + filename + "/3/5") is None

    f = gdal.VSIFOpenL("/vsipmtiles/" + filename + "/pmtiles_header.json", "rb")
    assert f
    try:
        data = gdal.VSIFReadL(1, 10000, f)
    finally:
        gdal.VSIFCloseL(f)
    assert json.loads(data) == {
        "addressed_tiles_count": 8,
        "center_lat_e7": 467000000,
        "center_lon_e7": 17500000,
        "center_zoom": 3,
        "clustered": True,
        "internal_compression": 2,
        "internal_compression_str": "gzip",
        "json_metadata_bytes": 2195,
        "json_metadata_offset": 163,
        "leaf_dirs_bytes": 132,
        "leaf_dirs_offset": 2358,
        "max_lat_e7": 512000000,
        "max_lat_e7_float": 51.2,
        "max_lon_e7": 85000000,
        "max_lon_e7_float": 8.5,
        "max_zoom": 5,
        "min_lat_e7": 422000000,
        "min_lat_e7_float": 42.2,
        "min_lon_e7": -50000000,
        "min_lon_e7_float": -5.0,
        "min_zoom": 3,
        "root_dir_offset": 127,
        "tile_compression": 0,
        "tile_compression_str": "unknown",
        "tile_contents_count": 8,
        "tile_data_bytes": 34018,
        "tile_data_offset": 2490,
        "tile_entries_count": 8,
        "tile_type": 1,
        "tile_type_str": "MVT",
    }

    assert gdal.VSIStatL(
        "/vsipmtiles/" + filename + "/pmtiles_header.json"
    ).size == len(data)

    f = gdal.VSIFOpenL("/vsipmtiles/" + filename + "/metadata.json", "rb")
    assert f
    try:
        data = gdal.VSIFReadL(1, 100000, f)
    finally:
        gdal.VSIFCloseL(f)
    j = json.loads(data)
    assert "json" in j

    assert gdal.VSIStatL("/vsipmtiles/" + filename).mode == 16384

    assert gdal.VSIStatL("/vsipmtiles/" + filename + "/metadata.json").size == len(data)
    assert gdal.VSIStatL("/vsipmtiles/" + filename + "/metadata.json").mode == 32768

    assert gdal.VSIFOpenL("/vsipmtiles/invalid", "rb") is None
    assert gdal.VSIFOpenL("/vsipmtiles/invalid.pmtiles", "rb") is None
    with gdaltest.tempfile("/vsimem/invalid.pmtiles", ""):
        assert (
            gdal.VSIFOpenL("/vsipmtiles//vsimem/invalid.pmtiles/metadata.json", "rb")
            is None
        )
    with gdaltest.tempfile("/vsimem/invalid.pmtiles", "x" * 127):
        assert (
            gdal.VSIFOpenL("/vsipmtiles//vsimem/invalid.pmtiles/metadata.json", "rb")
            is None
        )
    with gdaltest.tempfile("/vsimem/invalid.pmtiles", open(filename, "rb").read(127)):
        with pytest.raises(Exception):
            gdal.VSIFOpenL("/vsipmtiles//vsimem/invalid.pmtiles/metadata.json", "rb")
    assert gdal.VSIFOpenL("/vsipmtiles/" + filename, "rb") is None
    assert gdal.VSIFOpenL("/vsipmtiles/" + filename + "/invalid", "rb") is None
    assert gdal.VSIFOpenL("/vsipmtiles/" + filename + "/metadata.json", "rb+") is None
    assert gdal.VSIFOpenL("/vsipmtiles/" + filename + "/3", "rb") is None
    assert gdal.VSIFOpenL("/vsipmtiles/" + filename + "/3/4", "rb") is None
    assert gdal.VSIFOpenL("/vsipmtiles/" + filename + "/3/4/2.invalid", "rb") is None
    assert gdal.VSIFOpenL("/vsipmtiles/" + filename + "/3/4/20.mvt", "rb") is None
    assert gdal.VSIStatL("/vsipmtiles/" + filename + "/3/4/20.mvt") is None

    f = gdal.VSIFOpenL("/vsipmtiles/" + filename + "/3/4/2.mvt", "rb")
    assert f
    try:
        data = gdal.VSIFReadL(1, 100000, f)
    finally:
        gdal.VSIFCloseL(f)
    assert gdal.VSIStatL("/vsipmtiles/" + filename + "/3/4/2.mvt").size == len(data)
    ds = ogr.Open("/vsipmtiles/" + filename + "/3/4/2.mvt")
    assert ds.GetLayerCount() == 1
    ds = None


###############################################################################


@pytest.mark.require_driver("MBTiles")
# MBTiles vector writing mode requires SQLite and GEOS
@pytest.mark.require_driver("SQLite")
@pytest.mark.require_geos
def test_ogr_pmtiles_write_from_mbtiles():

    try:
        mbtiles_filename = "/vsimem/test.mbtiles"
        gdal.VectorTranslate(
            mbtiles_filename,
            "data/poly.shp",
            options="-s_srs EPSG:32631 -t_srs EPSG:3857",
        )
        pmtiles_filename = "/vsimem/test.pmtiles"

        # Tolerate -f/-of option
        out_ds = gdal.VectorTranslate(
            "/vsimem/test.pmtiles", mbtiles_filename, options="-f PMTiles"
        )
        assert out_ds

        src_ds = ogr.Open(mbtiles_filename)
        expected_md = src_ds.GetMetadata()
        expected_md["scheme"] = "xyz"
        assert expected_md == out_ds.GetMetadata()
        out_ds = None

        src_ds_sqlite3 = gdal.OpenEx(mbtiles_filename, allowed_drivers=["SQLite"])
        with src_ds_sqlite3.ExecuteSQL(
            "SELECT zoom_level, tile_column, tile_row, tile_data FROM tiles"
        ) as lyr:
            for f in lyr:
                z = f["zoom_level"]
                x = f["tile_column"]
                # MBTiles y=0 origin is bottom-most tile, whereas PMTiles is top-most
                y = (1 << z) - 1 - f["tile_row"]
                tile_data = f.GetFieldAsBinary("tile_data")
                assert gdal.VSIStatL(
                    f"/vsipmtiles/{pmtiles_filename}/{z}/{x}/{y}.mvt"
                ).size == len(tile_data)
        src_ds_sqlite3 = None

        f = gdal.VSIFOpenL(f"/vsipmtiles/{pmtiles_filename}/pmtiles_header.json", "rb")
        assert f
        try:
            data = gdal.VSIFReadL(1, 10000, f)
        finally:
            gdal.VSIFCloseL(f)
        got = json.loads(data)

        # Do not checks items whose value might depend on the gzip implementation
        expected = {
            "addressed_tiles_count": 5,
            "center_lat_e7": 430306306,
            "center_lon_e7": 27542428,
            "center_zoom": 0,
            "clustered": True,
            "internal_compression": 2,
            "internal_compression_str": "gzip",
            #'json_metadata_bytes': 545,
            #'json_metadata_offset': 171,
            "leaf_dirs_bytes": 0,
            #'leaf_dirs_offset': 716,
            "max_lat_e7": 430429264,
            "max_lat_e7_float": 43.0429264,
            "max_lon_e7": 27746819,
            "max_lon_e7_float": 2.7746819,
            "max_zoom": 5,
            "min_lat_e7": 430183348,
            "min_lat_e7_float": 43.0183348,
            "min_lon_e7": 27338036,
            "min_lon_e7_float": 2.7338036,
            "min_zoom": 0,
            "root_dir_offset": 127,
            "tile_compression": 2,
            "tile_compression_str": "gzip",
            "tile_contents_count": 5,
            # "tile_data_bytes": 1236,
            #'tile_data_offset': 716,
            "tile_entries_count": 5,
            "tile_type": 1,
            "tile_type_str": "MVT",
        }

        # For some reason do not pass on those older platforms. Perhaps GEOS
        # version?
        if not gdaltest.skip_on_travis():
            for key in expected:
                assert got[key] == expected[key], (key, got)

    finally:
        if gdal.VSIStatL(mbtiles_filename):
            gdal.Unlink(mbtiles_filename)
        if gdal.VSIStatL(pmtiles_filename):
            gdal.Unlink(pmtiles_filename)


###############################################################################


@pytest.mark.require_driver("MBTiles")
# MBTiles vector writing mode requires SQLite and GEOS
@pytest.mark.require_driver("SQLite")
@pytest.mark.require_geos
def test_ogr_pmtiles_write_from_mbtiles_deduplication():

    try:
        mbtiles_filename = "/vsimem/test.mbtiles"
        ds = ogr.GetDriverByName("MBTiles").CreateDataSource(
            mbtiles_filename, options=["MINZOOM=2", "MAXZOOM=2"]
        )
        lyr = ds.CreateLayer("test")
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(
            ogr.CreateGeometryFromWkt(
                "POLYGON((-20000000 -20000000,-20000000 20000000,20000000 20000000,20000000 -20000000,-20000000 -20000000))"
            )
        )
        lyr.CreateFeature(f)
        ds = None

        pmtiles_filename = "/vsimem/test.pmtiles"
        out_ds = gdal.VectorTranslate("/vsimem/test.pmtiles", mbtiles_filename)
        assert out_ds
        out_ds = None

        f = gdal.VSIFOpenL(f"/vsipmtiles/{pmtiles_filename}/pmtiles_header.json", "rb")
        assert f
        try:
            data = gdal.VSIFReadL(1, 10000, f)
        finally:
            gdal.VSIFCloseL(f)
        got = json.loads(data)

        expected = {
            "addressed_tiles_count": 16,
            "tile_contents_count": 9,
            "tile_entries_count": 13,
        }

        for key in expected:
            assert got[key] == expected[key], (key, got)

        src_ds_sqlite3 = gdal.OpenEx(mbtiles_filename, allowed_drivers=["SQLite"])
        with src_ds_sqlite3.ExecuteSQL(
            "SELECT zoom_level, tile_column, tile_row, tile_data FROM tiles"
        ) as lyr:
            for f in lyr:
                z = f["zoom_level"]
                x = f["tile_column"]
                # MBTiles y=0 origin is bottom-most tile, whereas PMTiles is top-most
                y = (1 << z) - 1 - f["tile_row"]
                tile_data = f.GetFieldAsBinary("tile_data")
                assert gdal.VSIStatL(
                    f"/vsipmtiles/{pmtiles_filename}/{z}/{x}/{y}.mvt"
                ).size == len(tile_data)
        src_ds_sqlite3 = None

    finally:
        if gdal.VSIStatL(mbtiles_filename):
            gdal.Unlink(mbtiles_filename)
        if gdal.VSIStatL(pmtiles_filename):
            gdal.Unlink(pmtiles_filename)


###############################################################################


@pytest.mark.require_driver("MBTiles")
# MBTiles vector writing mode requires SQLite and GEOS
@pytest.mark.require_driver("SQLite")
@pytest.mark.require_geos
def test_ogr_pmtiles_write():

    try:
        filename = "/vsimem/test.pmtiles"
        gdal.VectorTranslate(
            filename,
            "data/poly.shp",
            options="-s_srs EPSG:32631 -t_srs EPSG:3857",
        )

        ds = ogr.Open(filename)
        assert ds.GetLayerCount() == 1
        assert ds.GetMetadata() == {
            "description": "",
            "format": "pbf",
            "maxzoom": "5",
            "minzoom": "0",
            "name": "test",
            "scheme": "xyz",
            "type": "overlay",
            "version": "2",
            "ZOOM_LEVEL": "5",
            "bounds": "2.7338036,43.0183348,2.7746819,43.0429264",
            "center": "2.7542428,43.0306306,0",
        }
    finally:
        if gdal.VSIStatL(filename):
            gdal.Unlink(filename)


###############################################################################


def test_ogr_pmtiles_read_corrupted_min_zoom_larger_than_max_zoom():

    tmpfilename = "/vsimem/tmp.pmtiles"
    with gdaltest.tempfile(tmpfilename, open("data/pmtiles/poly.pmtiles", "rb").read()):
        f = gdal.VSIFOpenL(tmpfilename, "rb+")
        assert f
        gdal.VSIFSeekL(f, 0x64, 0)
        gdal.VSIFWriteL(b"\x01\x00", 1, 2, f)
        gdal.VSIFCloseL(f)
        with pytest.raises(Exception, match=r"min_zoom\(=1\) > max_zoom\(=0\)"):
            ogr.Open(tmpfilename)


###############################################################################


# Test started to fail on Travis s390x starting with https://github.com/OSGeo/gdal/pull/10274
# which is totally unrelated...
@pytest.mark.skipif(
    os.environ.get("BUILD_NAME", "") in ("s390x", "ubuntu_2004"),
    reason="Fails randomly on that platform",
)
def test_ogr_pmtiles_read_corrupted_min_zoom_larger_than_30():

    tmpfilename = "/vsimem/tmp.pmtiles"
    with gdaltest.tempfile(tmpfilename, open("data/pmtiles/poly.pmtiles", "rb").read()):
        f = gdal.VSIFOpenL(tmpfilename, "rb+")
        assert f
        gdal.VSIFSeekL(f, 0x64, 0)
        gdal.VSIFWriteL(b"\xfe\xff", 1, 2, f)
        gdal.VSIFCloseL(f)
        with gdal.quiet_errors():
            ds = ogr.Open(tmpfilename)
        assert gdal.GetLastErrorMsg() == "Clamping max_zoom from 255 to 30"
        assert ds.GetMetadataItem("ZOOM_LEVEL") == "30"


###############################################################################


def test_ogr_pmtiles_read_with_many_directories():

    ds = gdal.OpenEx(
        "data/pmtiles/subset7_truncated.pmtiles", open_options=["ZOOM_LEVEL=0"]
    )
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() != 0
