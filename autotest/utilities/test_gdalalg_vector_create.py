#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector create' testing
# Author:   Alessandro Pasotti <elpaso at itopen dot it>
#
###############################################################################
# Copyright (c) 2026, Alessandro Pasotti <elpaso at itopen dot it>
#
# SPDX-License-Identifier: MIT
###############################################################################


import pytest

from osgeo import gdal, ogr


def get_create_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["create"]


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize(
    "geometry_field_name, field_definitions, overwrite",
    [
        ("geom", ["test_field:String"], False),
        ("custom_geom", ["test_field:String"], True),
        ("geom", ["field1:Integer", "field2:Real"], True),
    ],
)
def test_gdalalg_vector_create(
    tmp_vsimem, geometry_field_name, field_definitions, overwrite
):

    alg = get_create_alg()

    out_file = "/vsimem/test.gpkg"

    alg["output"] = out_file
    alg["field"] = field_definitions
    alg["output-layer"] = "test_layer"
    alg["geometry-type"] = "POINT"
    alg["geometry-field"] = geometry_field_name
    alg["crs"] = "EPSG:4326"

    if overwrite:
        alg["overwrite"] = True

    assert alg.Run()
    assert alg["output"]

    out_ds = alg["output"].GetDataset()
    assert out_ds is not None, "Creation failed"

    layer = out_ds.GetLayer(0)
    assert layer is not None, "Layer not found"
    assert layer.GetName() == "test_layer", "Unexpected layer name"

    # Check the fields
    layer_defn = layer.GetLayerDefn()
    expected_fields = [fd.split(":") for fd in field_definitions]
    for i, expected_field in enumerate(expected_fields):
        field_defn = layer_defn.GetFieldDefn(i)
        assert (
            field_defn.GetName() == expected_field[0]
        ), f"Unexpected field name at index {i}"
        assert (
            field_defn.GetTypeName() == expected_field[1]
        ), f"Unexpected field type for field {expected_field}"

    # Check the geometry field name and type
    geom_defn = layer.GetLayerDefn().GetGeomFieldDefn(0)
    assert geom_defn.GetName() == geometry_field_name, "Unexpected geometry field name"
    assert geom_defn.GetType() == ogr.wkbPoint, "Unexpected geometry type"

    # Cleanup
    out_ds = None


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize(
    "args , match",
    [
        [{"geometry-type": "INVALID_TYPE"}, r"Invalid geometry type 'INVALID_TYPE'"],
        [
            {"field": ["invalid_field_definition"]},
            r"Invalid field definition format. Expected <NAME>:<TYPE>\[\(<WIDTH>\[,<PRECISION>\]\)\]",
        ],
        [{"field": ["wrong_field:Stroooong:"]}, r"Unsupported field type: Stroooong"],
        [{"crs": "INVALID_CRS"}, r"Invalid value for 'crs' argument"],
    ],
)
def test_gdal_vector_create_errors(tmp_vsimem, args, match):
    alg = get_create_alg()

    alg["output"] = "/vsimem/test.gpkg"
    alg["output-layer"] = "test_layer"

    with pytest.raises(RuntimeError, match=match):
        for key, value in args.items():
            alg[key] = value
