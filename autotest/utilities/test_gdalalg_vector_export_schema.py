#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector export schema' testing
# Author:   Alessandro Pasotti <elpaso at itopen dot it
#
###############################################################################
# Copyright (c) 2026, Alessandro Pasotti <elpaso at itopen dot it>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json

import gdaltest
import pytest

from osgeo import gdal, ogr, osr


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["export-schema"]


def test_gdalalg_vector_info_features_json():
    info = get_alg()
    assert info.ParseRunAndFinalize(["data/path.shp"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    gdaltest.validate_json(j, "ogr_fields_override.schema.json")

    assert j["layers"][0]["schemaType"] == "Full"

    fields = j["layers"][0]["fields"]
    assert len(fields) == 2
    assert fields[0]["name"] == "id"
    assert fields[0]["type"] == "Integer64"
    assert fields[0]["width"] == 10
    assert fields[0]["nullable"] is True
    assert fields[0]["uniqueConstraint"] is False

    assert fields[1]["name"] == "name"
    assert fields[1]["type"] == "String"
    assert fields[1]["width"] == 80
    assert fields[1]["nullable"] is True
    assert fields[1]["uniqueConstraint"] is False

    geom_fields = j["layers"][0]["geometryFields"]
    assert len(geom_fields) == 1
    assert geom_fields[0]["name"] == ""
    assert geom_fields[0]["type"] == "LineString"
    assert geom_fields[0]["nullable"] is True
    assert geom_fields[0]["coordinateSystem"]["authid"] == "EPSG:4326"


@pytest.mark.require_driver("GPKG")
def test_export_field_with_alias_comment_domain_default(tmp_vsimem):

    path = str(tmp_vsimem / "test.gpkg")
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(path)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    layer = ds.CreateLayer("test_layer", srs=srs)
    field_defn = ogr.FieldDefn("field1", ogr.OFTReal)
    field_defn.SetNullable(True)
    field_defn.SetUnique(True)
    field_defn.SetComment("This is a comment")
    field_defn.SetAlternativeName("alias1")
    field_defn.SetDefault("1234")
    field_defn.SetDomainName("domain1")
    layer.CreateField(field_defn)
    ds = None

    # Export the schema using the algorithm
    info = get_alg()
    assert info.ParseRunAndFinalize([path])
    output_string = info["output-string"]
    j = json.loads(output_string)

    # Validate the exported schema
    assert j["layers"][0]["fields"][0]["name"] == "field1"
    assert j["layers"][0]["fields"][0]["type"] == "Real"
    assert j["layers"][0]["fields"][0]["nullable"] is True
    assert j["layers"][0]["fields"][0]["uniqueConstraint"] is True
    assert j["layers"][0]["fields"][0]["comment"] == "This is a comment"
    assert j["layers"][0]["fields"][0]["alias"] == "alias1"
    assert j["layers"][0]["fields"][0]["defaultValue"] == "1234"
    assert j["layers"][0]["fields"][0]["domainName"] == "domain1"
    assert (
        j["layers"][0]["geometryFields"][0]["coordinateSystem"]["authid"]
        == "EPSG:32631"
    )


def test_gdalalg_vector_export_schema_to_file(tmp_vsimem):
    info = get_alg()
    tmp_file = str(tmp_vsimem / "out_schema.json")
    assert info.ParseRunAndFinalize(["data/path.shp", f"--output={tmp_file}"])
    with gdaltest.vsi_open(tmp_file, "r") as f:
        j = json.load(f)
        gdaltest.validate_json(j, "ogr_fields_override.schema.json")

    # Test without --overwrite and file already exists
    info = get_alg()
    with pytest.raises(RuntimeError, match="already exists"):
        assert info.ParseRunAndFinalize(["data/path.shp", f"--output={tmp_file}"])

    info = get_alg()
    assert info.ParseRunAndFinalize(
        ["data/path.shp", f"--output={tmp_file}", "--overwrite"]
    )


def test_gdalalg_vector_export_schema_to_file_pipeline(tmp_vsimem):
    pipeline = gdal.Algorithm("pipeline")
    tmp_file = str(tmp_vsimem / "out_schema_pipeline.json")
    assert pipeline.ParseRunAndFinalize(
        ["read", "data/path.shp", "!", "export-schema", f"--output={tmp_file}"]
    )
    with gdaltest.vsi_open(tmp_file, "r") as f:
        j = json.load(f)
        gdaltest.validate_json(j, "ogr_fields_override.schema.json")
