#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector rename-layer' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, ogr


def test_gdalalg_vector_rename_layer_output_layer():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("test")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    src_lyr.CreateFeature(f)

    with gdal.alg.vector.rename_layer(
        input=src_ds, output="", output_format="MEM", output_layer="renamed"
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetName() == "renamed"
        assert lyr.GetFeatureCount() == 1
        assert lyr.GetNextFeature().GetGeometryRef().ExportToWkt() == "POINT (1 2)"


def test_gdalalg_vector_rename_layer_input_layer_without_output_layer():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("test")

    with pytest.raises(
        Exception,
        match="Argument output-layer must be specified when input-layer is specified",
    ):
        gdal.alg.vector.rename_layer(
            input=src_ds, output="", output_format="MEM", input_layer="test"
        )


def test_gdalalg_vector_rename_layer_non_existing_input_layer():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("test")

    with pytest.raises(Exception, match="Input layer 'non_existing' does not exist"):
        gdal.alg.vector.rename_layer(
            input=src_ds,
            output="",
            output_format="MEM",
            input_layer="non_existing",
            output_layer="renamed",
        )


def test_gdalalg_vector_rename_layer_output_layer_without_intput_layer():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("1")
    src_ds.CreateLayer("2")

    with pytest.raises(
        Exception,
        match="Argument input-layer must be specified when output-layer is specified and there is more than one layer",
    ):
        gdal.alg.vector.rename_layer(
            input=src_ds, output="", output_format="MEM", output_layer="renamed"
        )


def test_gdalalg_vector_rename_layer_ascii():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("év😊en")

    with gdal.alg.vector.rename_layer(
        input=src_ds, output="", output_format="MEM", ascii=True
    ) as alg:
        ds = alg.Output()
        assert ds.GetLayer(0).GetName() == "even"


def test_gdalalg_vector_rename_layer_ascii_replacement_character():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("év😊en")

    with gdal.alg.vector.rename_layer(
        input=src_ds,
        output="",
        output_format="MEM",
        ascii=True,
        replacement_character="?",
    ) as alg:
        ds = alg.Output()
        assert ds.GetLayer(0).GetName() == "ev?en"


def test_gdalalg_vector_rename_layer_filename_compatible():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("CON")

    with gdal.alg.vector.rename_layer(
        input=src_ds,
        output="",
        output_format="MEM",
        filename_compatible=True,
    ) as alg:
        ds = alg.Output()
        assert ds.GetLayer(0).GetName() == "CON_"


def test_gdalalg_vector_rename_layer_reserved_characters():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("with space")

    with gdal.alg.vector.rename_layer(
        input=src_ds,
        output="",
        output_format="MEM",
        reserved_characters=" ",
        replacement_character="_",
    ) as alg:
        ds = alg.Output()
        assert ds.GetLayer(0).GetName() == "with_space"


def test_gdalalg_vector_rename_layer_lower_case():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("UPPERCASE")

    with gdal.alg.vector.rename_layer(
        input=src_ds,
        output="",
        output_format="MEM",
        lower_case=True,
    ) as alg:
        ds = alg.Output()
        assert ds.GetLayer(0).GetName() == "uppercase"


def test_gdalalg_vector_rename_layer_max_length():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("éven")

    with gdal.alg.vector.rename_layer(
        input=src_ds, output="", output_format="MEM", max_length=3
    ) as alg:
        ds = alg.Output()
        assert ds.GetLayer(0).GetName() == "éve"


def test_gdalalg_vector_rename_layer_unique():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("same_name")
    src_ds.CreateLayer("same_name")

    with gdal.alg.vector.rename_layer(
        input=src_ds, output="", output_format="MEM"
    ) as alg:
        ds = alg.Output()
        assert ds.GetLayer(0).GetName() == "same_name_1"
        assert ds.GetLayer(1).GetName() == "same_name_2"


def test_gdalalg_vector_rename_layer_unique_max_length():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("sâme_name")
    src_ds.CreateLayer("sâme_name")

    with gdal.alg.vector.rename_layer(
        input=src_ds, output="", output_format="MEM", max_length=5
    ) as alg:
        ds = alg.Output()
        assert ds.GetLayer(0).GetName() == "sâm_1"
        assert ds.GetLayer(1).GetName() == "sâm_2"


def test_gdalalg_vector_rename_layer_unique_max_length_impossible():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("same_name")
    src_ds.CreateLayer("same_name")

    with gdaltest.error_raised(gdal.CE_Warning, match="Cannot create unique name"):
        with gdal.alg.vector.rename_layer(
            input=src_ds, output="", output_format="stream", max_length=2
        ) as alg:
            ds = alg.Output()
            assert ds.GetLayerCount() == 2
            assert ds.GetLayer(0).GetName() == "sa"
            assert ds.GetLayer(1).GetName() == "sa"


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_rename_layer_relationships(tmp_vsimem):

    src_ds = gdal.GetDriverByName("GPKG").CreateVector(tmp_vsimem / "test.gpkg")
    src_ds.CreateLayer("left")
    src_ds.CreateLayer("right")

    relationship = gdal.Relationship(
        "my_relationship", "left", "right", gdal.GRC_MANY_TO_MANY
    )
    relationship.SetRelatedTableType("media")
    relationship.SetLeftTableFields(["fid"])
    relationship.SetRightTableFields(["fid"])
    assert src_ds.AddRelationship(relationship)

    src_ds.CreateLayer("another_left")
    src_ds.CreateLayer("another_right")
    relationship = gdal.Relationship(
        "another_relationship", "another_left", "another_right", gdal.GRC_MANY_TO_MANY
    )
    relationship.SetRelatedTableType("media")
    relationship.SetLeftTableFields(["fid"])
    relationship.SetRightTableFields(["fid"])
    assert src_ds.AddRelationship(relationship)

    assert set(src_ds.GetRelationshipNames()) == set(
        ["left_right_media", "another_left_another_right_media"]
    )

    with gdal.alg.vector.rename_layer(
        input=src_ds,
        output="",
        output_format="stream",
        input_layer="left",
        output_layer="renamed",
    ) as alg:
        ds = alg.Output()
        assert set(ds.GetRelationshipNames()) == set(
            ["left_right_media", "another_left_another_right_media"]
        )

        relationship = ds.GetRelationship("left_right_media")
        assert relationship.GetLeftTableName() == "renamed"
        assert relationship.GetRightTableName() == "right"
        assert relationship.GetMappingTableName() == "left_right"

        # Again
        relationship = ds.GetRelationship("left_right_media")
        assert relationship.GetLeftTableName() == "renamed"
        assert relationship.GetRightTableName() == "right"
        assert relationship.GetMappingTableName() == "left_right"

        relationship = ds.GetRelationship("another_left_another_right_media")
        assert relationship.GetLeftTableName() == "another_left"
        assert relationship.GetRightTableName() == "another_right"
        assert relationship.GetMappingTableName() == "another_left_another_right"

        assert ds.GetRelationship("non_existing") is None

    with gdal.alg.vector.rename_layer(
        input=src_ds,
        output="",
        output_format="stream",
        input_layer="right",
        output_layer="renamed",
    ) as alg:
        ds = alg.Output()

        relationship = ds.GetRelationship("left_right_media")
        assert relationship.GetLeftTableName() == "left"
        assert relationship.GetRightTableName() == "renamed"
        assert relationship.GetMappingTableName() == "left_right"

    with gdal.alg.vector.rename_layer(
        input=src_ds,
        output="",
        output_format="stream",
        input_layer="left_right",
        output_layer="renamed",
    ) as alg:
        ds = alg.Output()

        relationship = ds.GetRelationship("left_right_media")
        assert relationship.GetLeftTableName() == "left"
        assert relationship.GetRightTableName() == "right"
        assert relationship.GetMappingTableName() == "renamed"


def test_gdalalg_vector_rename_layer_in_pipeline():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("test")

    with gdal.alg.vector.pipeline(
        input=src_ds,
        pipeline="read ! rename-layer --input-layer=test --output-layer=renamed",
    ) as alg:
        ds = alg.Output()
        assert ds.GetLayer(0).GetName() == "renamed"


@pytest.mark.require_driver("GDALG")
def test_gdalalg_vector_rename_layer_test_ogrsf(tmp_path):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    gdalg_filename = tmp_path / "tmp.gdalg.json"
    open(gdalg_filename, "wb").write(
        b'{"type": "gdal_streamed_alg","command_line": "gdal vector rename-layer --output-layer renamed ../ogr/data/poly.shp --output-format=stream foo","relative_paths_relative_to_this_file":false}'
    )

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + f" -ro {gdalg_filename}"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret
    assert "FAILURE" not in ret
