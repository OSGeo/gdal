#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector convert' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr


def get_convert_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["convert"]


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_convert_base(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(["../ogr/data/poly.shp", out_filename])

    with gdal.OpenEx(out_filename, gdal.OF_UPDATE) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10
        for i in range(10):
            ds.GetLayer(0).DeleteFeature(i + 1)

    convert = get_convert_alg()
    with pytest.raises(Exception, match="already exists"):
        convert.ParseRunAndFinalize(["../ogr/data/poly.shp", out_filename])

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 0

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        ["--overwrite", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        ["--append", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 20

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        ["--update", "--nln", "layer2", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayerByName("poly").GetFeatureCount() == 20
        assert ds.GetLayerByName("layer2").GetFeatureCount() == 10

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        [
            "--of",
            "GPKG",
            "--overwrite-layer",
            "--nln",
            "poly",
            "../ogr/data/poly.shp",
            out_filename,
        ]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayerByName("poly").GetFeatureCount() == 10
        assert ds.GetLayerByName("layer2").GetFeatureCount() == 10


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_convert_dsco(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        ["../ogr/data/poly.shp", out_filename, "--co", "ADD_GPKG_OGR_CONTENTS=NO"]
    )

    with gdal.OpenEx(out_filename) as ds:
        with ds.ExecuteSQL(
            "SELECT * FROM sqlite_master WHERE name = 'gpkg_ogr_contents'"
        ) as lyr:
            assert lyr.GetFeatureCount() == 0


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_convert_lco(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.gpkg")

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        ["../ogr/data/poly.shp", out_filename, "--lco", "FID=my_fid"]
    )

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFIDColumn() == "my_fid"


def test_gdalalg_vector_convert_progress(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    convert = get_convert_alg()
    assert convert.ParseRunAndFinalize(
        ["../ogr/data/poly.shp", out_filename], my_progress
    )

    assert last_pct[0] == 1.0

    with gdal.OpenEx(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_wrong_layer_name(tmp_vsimem):

    convert = get_convert_alg()
    with pytest.raises(Exception, match="Cannot find source layer 'invalid'"):
        convert.ParseRunAndFinalize(
            [
                "../ogr/data/poly.shp",
                "--of=MEM",
                "--output=empty",
                "--layer",
                "invalid",
            ]
        )


def test_gdalalg_vector_convert_error_output_not_set():
    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"

    # Make it such that the "output" argument is set, but to a unset GDALArgDatasetValue
    convert["output"] = convert["output"]

    with pytest.raises(
        Exception,
        match="convert: Argument 'output' has no dataset object or dataset name",
    ):
        convert.Run()


@pytest.mark.require_driver("GeoJSON")
def test_gdalalg_vector_convert_vsistdout(tmp_vsimem):
    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"
    convert["output"] = f"/vsistdout_redirect/{tmp_vsimem}/tmp.json"
    convert["output-format"] = "GeoJSON"
    assert convert.Run()
    assert convert.Finalize()
    assert gdal.OpenEx(f"{tmp_vsimem}/tmp.json") is not None


@pytest.mark.require_driver("OpenFileGDB")
def test_gdalalg_vector_convert_overwrite_fgdb(tmp_vsimem):

    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"
    convert["output"] = tmp_vsimem / "out.gdb"
    convert["output-format"] = "OpenFileGDB"
    convert["layer-creation-option"] = {
        "TARGET_ARCGIS_VERSION": "ARCGIS_PRO_3_2_OR_LATER"
    }
    assert convert.Run()
    assert convert.Finalize()

    gdal.FileFromMemBuffer(tmp_vsimem / "out.gdb" / "new_file.txt", "foo")
    assert gdal.VSIStatL(tmp_vsimem / "out.gdb" / "new_file.txt") is not None

    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"
    convert["output"] = tmp_vsimem / "out.gdb"
    convert["output-format"] = "OpenFileGDB"
    convert["overwrite"] = True
    convert["layer-creation-option"] = {
        "TARGET_ARCGIS_VERSION": "ARCGIS_PRO_3_2_OR_LATER"
    }
    assert convert.Run()
    assert convert.Finalize()

    assert gdal.VSIStatL(tmp_vsimem / "out.gdb" / "new_file.txt") is None


@pytest.mark.require_driver("OpenFileGDB")
def test_gdalalg_vector_convert_overwrite_non_dataset_directory(tmp_vsimem):

    gdal.FileFromMemBuffer(tmp_vsimem / "out" / "foo", "bar")

    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"
    convert["output"] = tmp_vsimem / "out"
    convert["output-format"] = "OpenFileGDB"
    convert["overwrite"] = True
    with pytest.raises(
        Exception,
        match="already exists, but is not recognized as a valid GDAL dataset. Please manually delete it before retrying",
    ):
        convert.Run()


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_convert_overwrite_non_dataset_file(tmp_vsimem):

    gdal.FileFromMemBuffer(tmp_vsimem / "out.gpkg", "bar")

    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"
    convert["output"] = tmp_vsimem / "out.gpkg"
    convert["output-format"] = "GPKG"
    convert["overwrite"] = True
    assert convert.Run()


@pytest.mark.parametrize("skip_errors", (True, False))
def test_gdalalg_vector_convert_skip_errors(tmp_vsimem, skip_errors):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = src_ds.CreateLayer("test")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(1 2, 3 4)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(3 4)"))
    lyr.CreateFeature(f)

    convert = get_convert_alg()
    convert["input"] = src_ds
    convert["output"] = tmp_vsimem / "out.shp"
    convert["skip-errors"] = skip_errors

    if skip_errors:
        assert convert.Run()

        out_ds = convert["output"].GetDataset()
        assert out_ds.GetLayer(0).GetFeatureCount() == 2
    else:
        with pytest.raises(
            Exception, match="Failed to write layer 'test'. Use --skip-errors"
        ):
            convert.Run()


def test_gdalalg_vector_convert_to_non_available_db_driver():

    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"
    convert["output"] = "MongoDBv3:tmp/foo"
    if gdal.GetDriverByName("MongoDBv3"):
        with pytest.raises(
            Exception, match="Unable to open existing output datasource"
        ):
            convert.Run()
    else:
        with pytest.raises(
            Exception,
            match="Filename MongoDBv3:tmp/foo starts with the connection prefix of driver MongoDBv3, which is not enabled in this GDAL build. If that filename is really intended, explicitly specify its output format",
        ):
            convert.Run()


def test_gdalalg_vector_convert_output_format_not_guessed(tmp_vsimem):

    convert = get_convert_alg()
    convert["input"] = "../ogr/data/poly.shp"
    convert["output"] = tmp_vsimem / "foo"
    with pytest.raises(
        Exception,
        match="Cannot guess driver for",
    ):
        convert.Run()


@pytest.mark.parametrize(
    "driver", ("GeoJSON", "GPKG", "ESRI Shapefile", "CSV", "MapInfo File")
)
def test_gdalalg_vector_convert_output_format_multiple_layers(tmp_vsimem, driver):

    if not gdal.GetDriverByName(driver):
        pytest.skip(f"Driver {driver} not available")

    ext = {
        "GeoJSON": ".geojson",
        "GPKG": ".gpkg",
        "ESRI Shapefile": "",
        "CSV": "",
        "MapInfo File": "",
    }

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    with gdal.OpenEx("../ogr/data/poly.shp") as poly_ds:
        src_ds.CopyLayer(poly_ds.GetLayer(0), "poly_1")
        src_ds.CopyLayer(poly_ds.GetLayer(0), "poly_2")

    dst_fname = tmp_vsimem / f"out{ext[driver]}"

    convert = get_convert_alg()
    convert["input"] = src_ds
    convert["output"] = dst_fname
    convert["output-format"] = driver

    if driver == "GeoJSON":
        with pytest.raises(
            Exception, match="GeoJSON driver does not support multiple layers"
        ):
            convert.Run()
        assert gdal.VSIStatL(dst_fname) is None
    else:
        assert convert.Run()


@pytest.mark.require_driver("GeoJSON")
def test_gdalalg_vector_convert_to_stdout():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    # Check that quiet mode is automatically turned on (no progress bar)
    out = gdaltest.runexternal(
        f"{gdal_path} vector convert --of=GeoJSON data/path.shp /vsistdout/"
    )

    with ogr.Open(out) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 1


###############################################################################


def _get_sqlite_version():

    if gdal.GetDriverByName("GPKG") is None:
        return (0, 0, 0)

    ds = ogr.Open(":memory:")
    sql_lyr = ds.ExecuteSQL("SELECT sqlite_version()")
    f = sql_lyr.GetNextFeature()
    version = f.GetField(0)
    ds.ReleaseResultSet(sql_lyr)
    return tuple([int(x) for x in version.split(".")[0:3]])


@pytest.mark.skipif(
    _get_sqlite_version() < (3, 24, 0),
    reason="sqlite >= 3.24 needed",
)
@pytest.mark.parametrize("output_format", ["GPKG", "SQLite"])
def test_gdalalg_vector_convert_upsert(tmp_vsimem, output_format):

    filename = tmp_vsimem / (
        "test_ogr_gpkg_upsert_without_fid." + output_format.lower()
    )

    def create_gpkg_file():
        ds = gdal.GetDriverByName(output_format).Create(
            filename, 0, 0, 0, gdal.GDT_Unknown
        )
        lyr = ds.CreateLayer("foo")
        assert lyr.CreateField(ogr.FieldDefn("other", ogr.OFTString)) == ogr.OGRERR_NONE
        unique_field = ogr.FieldDefn("unique_field", ogr.OFTString)
        unique_field.SetUnique(True)
        assert lyr.CreateField(unique_field) == ogr.OGRERR_NONE
        for i in range(5):
            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetField("unique_field", i + 1)
            f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (%d %d)" % (i, i)))
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        ds = None

    create_gpkg_file()

    def create_src_file():
        src_filename = tmp_vsimem / "test_ogr_gpkg_upsert_src.gpkg"
        srcDS = gdal.GetDriverByName("GPKG").Create(
            src_filename, 0, 0, 0, gdal.GDT_Unknown
        )
        lyr = srcDS.CreateLayer("foo")
        assert lyr.CreateField(ogr.FieldDefn("other", ogr.OFTString)) == ogr.OGRERR_NONE
        unique_field = ogr.FieldDefn("unique_field", ogr.OFTString)
        unique_field.SetUnique(True)
        assert lyr.CreateField(unique_field) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField("unique_field", "2")
        f.SetField("other", "foo")
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (10 10)"))
        lyr.CreateFeature(f)
        return srcDS

    if output_format == "SQLite":
        with pytest.raises(Exception, match="SQLite driver doest not support upsert"):
            gdal.Run(
                "vector",
                "convert",
                input=create_src_file(),
                output=filename,
                upsert=True,
            )
    else:
        gdal.Run(
            "vector", "convert", input=create_src_file(), output=filename, upsert=True
        )

        ds = ogr.Open(filename)
        lyr = ds.GetLayer(0)
        f = lyr.GetFeature(2)
        assert f["unique_field"] == "2"
        assert f["other"] == "foo"
        assert f.GetGeometryRef().ExportToWkt() == "POINT (10 10)"
        ds = None


@pytest.mark.require_driver("GeoJSON")
def test_error_message_leak(tmp_vsimem):
    """Test issue GH #13662"""

    json_path = tmp_vsimem / "test_error_message_leak_in.json"
    out_path = tmp_vsimem / "test_error_message_leak_out.shp"

    src_ds = ogr.GetDriverByName("GeoJSON").CreateDataSource(json_path)
    lyr = src_ds.CreateLayer("test")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(1 2, 3 4)"))
    lyr.CreateFeature(f)
    src_ds = None

    alg = get_convert_alg()
    with pytest.raises(
        Exception,
        match="Failed to write layer 'test'. Use --skip-errors to ignore errors",
    ):
        alg.ParseRunAndFinalize(
            [
                json_path,
                out_path,
            ],
        )
