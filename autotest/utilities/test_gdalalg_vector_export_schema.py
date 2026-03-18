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

from osgeo import gdal


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
    assert geom_fields[0]["coordinateSystem"]["projjson"]["id"]["code"] == 4326
