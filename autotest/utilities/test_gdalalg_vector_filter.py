#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector filter' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import sys

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr


def get_filter_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["filter"]


def test_gdalalg_vector_filter_no_filter(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    assert filter_alg.ParseCommandLineArguments(["../ogr/data/poly.shp", out_filename])
    assert filter_alg.Run()
    ds = filter_alg["output"].GetDataset()
    assert ds.GetLayer(0).GetFeatureCount() == 10
    assert filter_alg.Finalize()
    ds = None

    with gdal.Open(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_filter_bbox(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    assert filter_alg.ParseRunAndFinalize(
        ["--bbox=479867,4762909,479868,4762910", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.Open(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 1


def test_gdalalg_vector_filter_where_discard_all(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    assert filter_alg.ParseRunAndFinalize(
        ["--where=0=1", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.Open(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 0


def test_gdalalg_vector_filter_where_accept_all(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    assert filter_alg.ParseRunAndFinalize(
        ["--where=1=1", "../ogr/data/poly.shp", out_filename]
    )

    with gdal.Open(out_filename) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_vector_filter_where_error(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.shp")

    filter_alg = get_filter_alg()
    with pytest.raises(
        Exception, match='"invalid" not recognised as an available field.'
    ):
        filter_alg.ParseRunAndFinalize(
            ["--where=invalid", "../ogr/data/poly.shp", out_filename]
        )


def test_gdalalg_vector_filter_bbox_active_layer():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")
    src_lyr.CreateField(ogr.FieldDefn("foo"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "bar"
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "baz"
    src_lyr.CreateFeature(f)

    src_lyr = src_ds.CreateLayer("other_layer")
    src_lyr.CreateField(ogr.FieldDefn("foo"))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["foo"] = "baz"
    src_lyr.CreateFeature(f)

    filter_alg = get_filter_alg()
    filter_alg["input"] = src_ds
    filter_alg["active-layer"] = "the_layer"

    assert filter_alg.ParseCommandLineArguments(
        ["--where", "foo='bar'", "--of", "MEM", "--output", "memory_ds"]
    )
    assert filter_alg.Run()

    out_ds = filter_alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "bar"
    assert out_lyr.GetNextFeature() is None

    out_lyr = out_ds.GetLayer(1)
    out_f = out_lyr.GetNextFeature()
    assert out_f["foo"] == "baz"


def test_gdalalg_vector_filter_update_extent(tmp_vsimem):

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "in"
    ) as src_ds:
        src_lyr = src_ds.CreateLayer("the_layer", geom_type=ogr.wkbPoint25D)
        src_lyr.CreateField(ogr.FieldDefn("foo"))

        f = ogr.Feature(src_lyr.GetLayerDefn())
        f["foo"] = "bar"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2 10)"))
        src_lyr.CreateFeature(f)

        f = ogr.Feature(src_lyr.GetLayerDefn())
        f["foo"] = "baz"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(3 4 20)"))
        src_lyr.CreateFeature(f)

        src_lyr = src_ds.CreateLayer(
            "other_layer", geom_type=ogr.wkbPoint25D, options=["AUTO_REPACK=NO"]
        )
        src_lyr.CreateField(ogr.FieldDefn("foo"))

        f = ogr.Feature(src_lyr.GetLayerDefn())
        f["foo"] = "baz"
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(3 4 30)"))
        src_lyr.CreateFeature(f)

        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(5 6 40)"))
        src_lyr.CreateFeature(f)
        src_lyr.DeleteFeature(f.GetFID())

    with gdal.alg.vector.filter(
        input=tmp_vsimem / "in",
        output="",
        output_format="stream",
        where="foo='bar'",
        update_extent=True,
        active_layer="the_layer",
    ) as alg:
        out_ds = alg.Output()
        lyr = out_ds.GetLayerByName("the_layer")
        assert lyr.GetExtent(force=False) == (1, 1, 2, 2)
        assert lyr.GetExtent3D(force=False) == (1, 1, 2, 2, 10, 10)
        assert lyr.GetFeatureCount() == 1
        lyr = out_ds.GetLayerByName("other_layer")
        assert lyr.GetExtent(force=False) == (3, 5, 4, 6)
        assert lyr.GetExtent3D(force=False) == (3, 5, 4, 6, 30, 40)
        assert lyr.GetFeatureCount() == 1


@pytest.mark.require_driver("GDALG")
def test_gdalalg_vector_filter_test_ogrsf(tmp_path):

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    gdalg_filename = tmp_path / "tmp.gdalg.json"
    open(gdalg_filename, "wb").write(
        b'{"type": "gdal_streamed_alg","command_line": "gdal vector filter ../ogr/data/poly.shp --where 1=1 --output-format=stream dummy_dataset_name","relative_paths_relative_to_this_file":false}'
    )

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" -ro {gdalg_filename}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret
    assert "FAILURE" not in ret

    gdalg_filename = tmp_path / "tmp.gdalg.json"
    open(gdalg_filename, "wb").write(
        b'{"type": "gdal_streamed_alg","command_line": "gdal vector filter ../ogr/data/poly.shp --where EAS_ID=170 --output-format=stream dummy_dataset_name","relative_paths_relative_to_this_file":false}'
    )

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" -ro {gdalg_filename}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret
    assert "FAILURE" not in ret


def _make_shlex_happy(cmd):
    # We add STRIP-ME" at the end just to make shlex.split() happy
    return cmd + 'STRIP-ME"'


@pytest.mark.parametrize(
    "suffix,expected_completions",
    [
        ("", ["AREA", "EAS_ID", "PRFEDEA"]),
        ("AR", ["AREA", "EAS_ID", "PRFEDEA"]),
        (
            "PRFEDEA ",
            [
                "PRFEDEA =",
                "PRFEDEA <>",
                "PRFEDEA <",
                "PRFEDEA <=",
                "PRFEDEA >",
                "PRFEDEA >=",
                "PRFEDEA AND",
                "PRFEDEA OR",
                "PRFEDEA LIKE",
                "PRFEDEA BETWEEN",
            ],
        ),
        (
            "PRFEDEA = ",
            [
                "PRFEDEA = '35043369'",
                "PRFEDEA = '35043408'",
                "PRFEDEA = '35043409'",
                "PRFEDEA = '35043411'",
                "PRFEDEA = '35043412'",
                "PRFEDEA = '35043413'",
                "PRFEDEA = '35043414'",
                "PRFEDEA = '35043415'",
                "PRFEDEA = '35043416'",
                "PRFEDEA = '35043423'",
            ],
        ),
        (
            "PRFEDEA = '3",
            [
                "PRFEDEA = '35043369'",
                "PRFEDEA = '35043408'",
                "PRFEDEA = '35043409'",
                "PRFEDEA = '35043411'",
                "PRFEDEA = '35043412'",
                "PRFEDEA = '35043413'",
                "PRFEDEA = '35043414'",
                "PRFEDEA = '35043415'",
                "PRFEDEA = '35043416'",
                "PRFEDEA = '35043423'",
            ],
        ),
        (
            "EAS_ID = ",
            [
                "EAS_ID = 168",
                "EAS_ID = 179",
                "EAS_ID = 171",
                "EAS_ID = 173",
                "EAS_ID = 172",
                "EAS_ID = 169",
                "EAS_ID = 166",
                "EAS_ID = 158",
                "EAS_ID = 165",
                "EAS_ID = 170",
            ],
        ),
        (
            "PRFEDEA = 'foo' AND ",
            [
                "PRFEDEA = 'foo' AND AREA",
                "PRFEDEA = 'foo' AND EAS_ID",
                "PRFEDEA = 'foo' AND PRFEDEA",
            ],
        ),
    ],
)
def test_gdalalg_vector_filter_where_completion(suffix, expected_completions):

    if sys.platform == "win32":
        pytest.skip("not compatible of win32")

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary not available")

    out = gdaltest.run_and_parse_completion_output(
        _make_shlex_happy(
            f'{gdal_path} completion gdal vector filter ../ogr/data/poly.shp --where "{suffix}'
        )
    )
    assert out == expected_completions


@pytest.mark.require_driver("SQLITE")
def test_gdalalg_vector_filter_where_completion_more_ten_features(tmp_path):

    if sys.platform == "win32":
        pytest.skip("not compatible of win32")

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary not available")

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(tmp_path / "tmp.db") as ds:
        lyr = ds.CreateLayer("tmp")
        lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("int2", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("str2", ogr.OFTString))
        for i in range(100):
            f = ogr.Feature(lyr.GetLayerDefn())
            f["int"] = 100 - i
            f["str"] = (
                "z"
                if i < 40
                else (
                    "b"
                    if i < 45
                    else (
                        "c"
                        if i < 55
                        else (
                            "d"
                            if i < 60
                            else (
                                "e"
                                if i < 65
                                else (
                                    "f"
                                    if i < 70
                                    else (
                                        "g"
                                        if i < 75
                                        else (
                                            "h"
                                            if i < 80
                                            else (
                                                "i"
                                                if i < 85
                                                else (
                                                    "j"
                                                    if i < 90
                                                    else "k" if i < 95 else "l"
                                                )
                                            )
                                        )
                                    )
                                )
                            )
                        )
                    )
                )
            )
            lyr.CreateFeature(f)

    out = gdaltest.run_and_parse_completion_output(
        _make_shlex_happy(
            f'{gdal_path} completion gdal vector filter {tmp_path}/tmp.db --where "str = '
        )
    )
    assert out == [
        "str = str2",
        "str = 'z'",
        "str = 'c'",
        "str = 'b'",
        "str = 'd'",
        "str = 'e'",
        "str = 'f'",
        "str = 'g'",
        "str = 'h'",
        "str = 'i'",
        "str = 'j'",
        "str = '...other values...",
    ]

    out = gdaltest.run_and_parse_completion_output(
        _make_shlex_happy(
            f'{gdal_path} completion gdal vector filter {tmp_path}/tmp.db --active-layer tmp --where "int = '
        )
    )
    assert out == [
        "int = int2",
        "int = 1",
        "int = 2",
        "int = 3",
        "int = 4",
        "int = 5",
        "int = 96",
        "int = 97",
        "int = 98",
        "int = 99",
        "int = 100",
        "int = ...other values...",
    ]

    out = gdaltest.run_and_parse_completion_output(
        _make_shlex_happy(
            f'{gdal_path} completion gdal vector filter {tmp_path}/tmp.db --active-layer invalid --where "int = '
        )
    )
    assert out[0] == "**"
