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


import json

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

    out_file = str(tmp_vsimem / "vsimem/test.gpkg")

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

    alg["output"] = str(tmp_vsimem / "vsimem/test.gpkg")
    alg["output-layer"] = "test_layer"

    with pytest.raises(RuntimeError, match=match):
        for key, value in args.items():
            alg[key] = value


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_create_overwrite_update_overwrite_layer(tmp_vsimem):

    out_file = str(tmp_vsimem / "test_overwrite.gpkg")

    # ulink any existing file to ensure a clean state
    try:
        gdal.Unlink(out_file)
    except RuntimeError:
        pass

    def _get_layer_names(ds):
        layer_names = [ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount())]
        ds = None
        return layer_names

    def _get_alg():
        alg = get_create_alg()
        alg["output"] = out_file
        alg["field"] = ["test_field:String"]
        alg["output-layer"] = "test_layer"
        alg["geometry-type"] = "POINT"
        alg["geometry-field"] = "geom"
        alg["crs"] = "EPSG:3857"
        return alg

    alg = _get_alg()
    assert alg.Run()
    assert _get_layer_names(alg["output"].GetDataset()) == [
        "test_layer"
    ], "Unexpected layer names"

    # Now try to create the same layer without overwrite - should fail
    with pytest.raises(RuntimeError, match="already exists"):
        alg = _get_alg()
        assert alg.Run()

    # Now set overwrite and try again - should succeed
    alg = _get_alg()
    alg["overwrite"] = True
    assert alg.Run()
    assert _get_layer_names(alg["output"].GetDataset()) == [
        "test_layer"
    ], "Unexpected layer names after overwrite"

    # Now try with a different layer name, should fail without --update
    alg = _get_alg()
    alg["output-layer"] = "another_layer"
    with pytest.raises(RuntimeError, match="already exists"):
        assert alg.Run()

    # Now set update and try again - should succeed
    alg = _get_alg()
    alg["output-layer"] = "another_layer"
    alg["update"] = True
    assert alg.Run()

    # Check that we actually have both layers in the file
    ds = gdal.OpenEx(out_file, gdal.OF_VECTOR)
    assert ds is not None, "Failed to open dataset"
    assert _get_layer_names(alg["output"].GetDataset()) == [
        "test_layer",
        "another_layer",
    ], "Unexpected layer names in dataset"

    # Write another_layer with --overwrite, should succeed and replace all existing layers
    alg = _get_alg()
    alg["output-layer"] = "another_layer"
    alg["overwrite"] = True
    alg["field"] = ["new_field:Integer"]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds is not None, "Failed to open dataset after overwrite"
    assert _get_layer_names(ds) == [
        "another_layer"
    ], "Unexpected layer names after overwrite"
    lyr = ds.GetLayerByName("another_layer")
    assert lyr is not None, "another_layer not found after overwrite"
    field_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert field_defn.GetName() == "new_field", "Unexpected field name after overwrite"
    ds = None

    # Add another layer with --update, should succeed and keep the existing another_layer
    alg = _get_alg()
    alg["output-layer"] = "third_layer"
    alg["update"] = True
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds is not None, "Failed to open dataset after adding third layer"
    assert _get_layer_names(ds) == [
        "another_layer",
        "third_layer",
    ], "Unexpected layer names after adding third layer"
    ds = None

    # Replace the third_layer with --overwrite-layer, should succeed and keep the existing another_layer
    alg = _get_alg()
    alg["output-layer"] = "third_layer"
    alg["overwrite-layer"] = True
    alg["field"] = ["new_field2:Integer"]
    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds is not None, "Failed to open dataset after overwriting third layer"
    assert _get_layer_names(ds) == [
        "another_layer",
        "third_layer",
    ], "Unexpected layer names after overwriting third layer"
    lyr = ds.GetLayerByName("third_layer")
    assert lyr is not None, "third_layer not found after overwriting it"
    field_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert (
        field_defn.GetName() == "new_field2"
    ), "Unexpected field name after overwriting third layer"
    ds = None

    # Specify --overwrite-layer for a layer which does not exist, it should fail
    alg = _get_alg()
    alg["output-layer"] = "fourth_layer"
    alg["overwrite-layer"] = True
    with pytest.raises(RuntimeError, match="Cannot find layer 'fourth_layer'"):
        assert alg.Run()

    # Check --overwrite with a not existing file, it should succeed and create the file
    alg = _get_alg()
    alg["output"] = "/vsimem/new_file.gpkg"
    alg["overwrite"] = True
    assert alg.Run()

    # Check CRS of the created layer is correct after overwrite
    ds = alg["output"].GetDataset()
    assert ds is not None, "Failed to open dataset after overwrite"
    lyr = ds.GetLayer(0)
    assert lyr is not None, "Layer not found after overwrite"
    geom_defn = lyr.GetLayerDefn().GetGeomFieldDefn(0)
    assert (
        geom_defn.GetSpatialRef().GetAuthorityCode(None) == "3857"
    ), "Unexpected CRS after overwrite"


def test_gdalalg_vector_create_overwrite_shapefile(tmp_vsimem):
    out_file = str(tmp_vsimem / "test_overwrite.shp")

    # ulink any existing file to ensure a clean state
    try:
        gdal.Unlink(out_file)
    except RuntimeError:
        pass

    def _get_alg():
        alg = get_create_alg()
        alg["output"] = out_file
        alg["field"] = ["test_field:String"]
        alg["output-layer"] = "test_layer"
        alg["geometry-type"] = "POINT"
        alg["geometry-field"] = "geom"
        alg["crs"] = "EPSG:4326"
        return alg

    alg = _get_alg()
    assert alg.Run()

    # Try to overwrite the same shapefile without overwrite - should fail
    alg = _get_alg()
    with pytest.raises(RuntimeError, match="already exists"):
        assert alg.Run()

    # Now set overwrite and try again - should succeed
    alg = _get_alg()
    alg["overwrite"] = True
    alg["field"] = ["new_field:Integer"]
    assert alg.Run()
    # Check that the file was overwritten and has the new field definition
    ds = alg["output"].GetDataset()
    assert ds is not None, "Failed to open dataset after overwrite"
    lyr = ds.GetLayer(0)
    field_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert field_defn.GetName() == "new_field", "Unexpected field name after overwrite"


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_create_ogr_schema(tmp_vsimem):

    ogr_schema = r"""
{
    "layers":[
    {
        "name": "layer1",
        "schemaType": "Full",
        "fields": [
        {
            "name": "field1",
            "type": "String",
            "subType": "JSON"
        }
        ],
        "geometryFields": [{
            "name": "geom",
            "type": "Point",
            "coordinateSystem": { "wkt" : "PROJCS[\"WGS 84 / Pseudo-Mercator\",GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]],PROJECTION[\"Mercator_1SP\"],PARAMETER[\"central_meridian\",0],PARAMETER[\"scale_factor\",1],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],EXTENSION[\"PROJ4\",\"+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs\"],AUTHORITY[\"EPSG\",\"3857\"]]" }
            }]
    }]
}
"""
    alg = get_create_alg()

    out_file = str(tmp_vsimem / "test_schema.gpkg")

    alg["output"] = out_file
    alg["schema"] = ogr_schema

    assert alg.Run()
    assert alg["output"]

    out_ds = alg["output"].GetDataset()
    assert out_ds is not None, "Creation failed"

    layer = out_ds.GetLayer(0)
    assert layer is not None, "Layer not found"
    assert layer.GetName() == "layer1", "Unexpected layer name"

    # Check the fields
    layer_defn = layer.GetLayerDefn()
    field_defn = layer_defn.GetFieldDefn(0)
    assert field_defn.GetName() == "field1", "Unexpected field name"
    assert field_defn.GetTypeName() == "String", "Unexpected field type"
    assert field_defn.GetSubType() == ogr.OFSTJSON, "Unexpected field sub-type"

    # Check the geometry field name and type
    assert (
        layer.GetLayerDefn().GetGeomFieldCount() == 1
    ), "Unexpected number of geometry fields"
    geom_defn = layer.GetLayerDefn().GetGeomFieldDefn(0)
    assert geom_defn.GetName() == "geom", "Unexpected geometry field name"
    assert geom_defn.GetType() == ogr.wkbPoint, "Unexpected geometry type"
    assert geom_defn.GetSpatialRef().GetAuthorityCode(None) == "3857", "Unexpected CRS"

    # Cleanup
    out_ds = None

    # Check with --like
    alg = get_create_alg()
    alg["output"] = str(tmp_vsimem / "test_schema_like.gpkg")
    alg["like"] = out_file
    assert alg.Run()
    out_ds = alg["output"].GetDataset()
    assert out_ds is not None, "Creation failed with --like"
    layer = out_ds.GetLayer(0)
    assert layer is not None, "Layer not found with --like"
    assert layer.GetName() == "layer1", "Unexpected layer name with --like"
    # Check the fields
    layer_defn = layer.GetLayerDefn()
    field_defn = layer_defn.GetFieldDefn(0)
    assert field_defn.GetName() == "field1", "Unexpected field name with --like"
    assert field_defn.GetTypeName() == "String", "Unexpected field type with --like"
    assert (
        field_defn.GetSubType() == ogr.OFSTJSON
    ), "Unexpected field sub-type with --like"
    # Check the geometry field name and type
    assert (
        layer.GetLayerDefn().GetGeomFieldCount() == 1
    ), "Unexpected number of geometry fields with --like"
    geom_defn = layer.GetLayerDefn().GetGeomFieldDefn(0)
    assert geom_defn.GetName() == "geom", "Unexpected geometry field name with --like"
    assert geom_defn.GetType() == ogr.wkbPoint, "Unexpected geometry type with --like"
    assert (
        geom_defn.GetSpatialRef().GetAuthorityCode(None) == "3857"
    ), "Unexpected CRS with --like"


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_create_aspatial_layer(tmp_vsimem):
    alg = get_create_alg()

    out_file = str(tmp_vsimem / "test_aspatial.gpkg")

    alg["output"] = out_file
    alg["field"] = ["test_field:String"]
    alg["output-layer"] = "test_layer"

    assert alg.Run()
    assert alg["output"]

    out_ds = alg["output"].GetDataset()
    assert out_ds is not None, "Creation failed"

    layer = out_ds.GetLayer(0)
    assert layer is not None, "Layer not found"
    assert layer.GetName() == "test_layer", "Unexpected layer name"

    # Check the fields
    layer_defn = layer.GetLayerDefn()
    field_defn = layer_defn.GetFieldDefn(0)
    assert field_defn.GetName() == "test_field", "Unexpected field name"
    assert field_defn.GetTypeName() == "String", "Unexpected field type"

    # Check that there is no geometry field
    assert (
        layer.GetLayerDefn().GetGeomFieldCount() == 0
    ), "Unexpected geometry field count for aspatial layer"

    # Cleanup
    out_ds = None


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize(
    "input_names, output_name, expected_names",
    [
        (["layer_1"], "", ["layer_1"]),
        (["layer_1"], "layer_new_1", ["layer_new_1"]),
        (
            ["layer_1", "layer_3"],
            "layer_new_1",
            "Output layer name should not be specified when there are multiple layers in the schema",
        ),
        (["layer_2", "layer_3"], "", ["layer_2", "layer_3"]),
    ],
)
def test_gdalalg_vector_create_layer_names(
    tmp_vsimem, input_names, output_name, expected_names
):
    alg = get_create_alg()

    in_file = str(tmp_vsimem / "test_layer_names_in.gpkg")
    out_file = str(tmp_vsimem / "test_layer_names_out.gpkg")

    # Makes sure both file are not already existing
    for f in [in_file, out_file]:
        try:
            gdal.Unlink(f)
        except RuntimeError:
            pass

    # Create 3 layers layer_1, layer_2, layer_3
    for i in range(3):
        alg["output"] = in_file
        alg["update"] = i > 0
        alg["field"] = ["test_field:String"]
        alg["output-layer"] = f"layer_{i + 1}"
        alg["geometry-type"] = "POINT"
        alg["geometry-field"] = "geom"
        alg["crs"] = "EPSG:4326"
        assert alg.Run()

    alg = get_create_alg()
    alg["output"] = out_file
    alg["like"] = in_file
    if len(input_names) > 0:
        alg["input-layer"] = input_names
    if output_name:
        alg["output-layer"] = output_name

    if isinstance(expected_names, list):
        assert alg.Run()
        out_ds = alg["output"].GetDataset()
        assert out_ds is not None, "Creation failed"
        layer_names = [
            out_ds.GetLayer(i).GetName() for i in range(out_ds.GetLayerCount())
        ]
        assert layer_names == expected_names, f"Unexpected layer names: {layer_names}"
    else:
        with pytest.raises(RuntimeError, match=expected_names):
            alg.Run()
        return


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize(
    "options, match",
    [
        (
            {"schema": r"{}", "like": "xxx"},
            "'schema' is mutually exclusive with 'input'",
        ),
        (
            {"schema": r"{}", "field": "one:string"},
            "'field' is mutually exclusive with 'schema'",
        ),
        (
            {"schema": r"{}", "geometry-type": "Point"},
            "When --schema or --like is specified, .*options must not be specified.",
        ),
        (
            {"schema": r"{}", "geometry-field": "geomm"},
            "When --schema or --like is specified, .*options must not be specified.",
        ),
        (
            {"schema": r"{}", "crs": "EPSG:4326"},
            "When --schema or --like is specified, .*options must not be specified.",
        ),
        (
            {"schema": r"{}", "fid": "fid"},
            "When --schema or --like is specified, .*options must not be specified.",
        ),
    ],
)
def test_gdalalg_vector_create_mutex_options(tmp_vsimem, options, match):
    alg = get_create_alg()

    out_file = str(tmp_vsimem / "test_mutex.gpkg")

    alg["output"] = out_file
    alg["output-layer"] = "test_layer"

    for key, value in options.items():
        alg[key] = value

    with pytest.raises(RuntimeError, match=match):
        alg.Run()


def test_gdalalg_vector_create_single_layer_format(tmp_vsimem):

    out_file = str(tmp_vsimem / "test_single_layer.shp")

    ogr_schema = r"""
{
    "layers":[
    {
        "name": "layer1",
        "schemaType": "Full",
        "fields": [
        {
            "name": "field1",
            "type": "String"
        }]
    },
    {
        "name": "layer2",
        "schemaType": "Full",
        "fields": [
        {
            "name": "field2",
            "type": "String"
        }]
    }
    ]
}"""

    alg = get_create_alg()
    alg["output"] = out_file
    alg["schema"] = ogr_schema

    with pytest.raises(
        RuntimeError,
        match=r"The output format ESRI Shapefile doesn't support multiple layers",
    ):
        assert alg.Run()

    # This should succeed since we specify a single layer to create
    alg = get_create_alg()
    alg["input-layer"] = ["layer2"]
    alg["output"] = out_file
    alg["schema"] = ogr_schema

    assert alg.Run()


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_create_datetime_timezone_round_trip(tmp_vsimem):

    out_file = str(tmp_vsimem / "test_datetime.gpkg")

    ogr_schema = r"""
{
    "layers":[
    {
        "name": "layer1",
        "schemaType": "Full",
        "fields": [
        {
            "name": "field1",
            "type": "DateTime",
            "timezone": "+11:30"
        }]
    }]
}"""

    alg = get_create_alg()
    alg["output"] = out_file
    alg["schema"] = ogr_schema

    assert alg.Run()

    # Now open the file and check that the field has the expected type and sub-type
    ds = alg["output"].GetDataset()
    assert ds is not None, "Failed to open dataset"
    layer = ds.GetLayer(0)
    assert layer is not None, "Layer not found"
    field_defn = layer.GetLayerDefn().GetFieldDefn(0)
    assert field_defn.GetName() == "field1", "Unexpected field name"
    assert field_defn.GetTypeName() == "DateTime", "Unexpected field type"
    # UTC+11:30 == 146
    assert field_defn.GetTZFlag() == 146, "Unexpected timezone information"

    # Export schema and check that timezone information is still there
    alg = gdal.GetGlobalAlgorithmRegistry()["vector"]["export-schema"]
    alg["input"] = ds
    assert alg.Run()
    out_schema = alg["output-string"]
    j = json.loads(out_schema)
    json_field_defn = j["layers"][0]["fields"][0]
    assert (
        json_field_defn["name"] == "field1"
    ), "Unexpected field name in exported schema"
    assert (
        json_field_defn["type"] == "DateTime"
    ), "Unexpected field type in exported schema"
    assert (
        json_field_defn["timezone"] == "+11:30"
    ), "Unexpected timezone information in exported schema"


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_create_geometry_field_without_crs(tmp_vsimem):

    with gdal.alg.vector.create(
        output=tmp_vsimem / "out.gpkg", geometry_type="GEOMETRY"
    ) as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetName() == "out"
        assert out_lyr.GetFIDColumn() == "fid"
        assert out_lyr.GetGeometryColumn() == "geom"
        assert out_lyr.GetGeomType() == ogr.wkbUnknown
        assert out_lyr.GetSpatialRef() is None
        assert out_lyr.GetLayerDefn().GetFieldCount() == 0


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_create_implicit_fid_from_source(tmp_vsimem):

    src_ds = gdal.GetDriverByName("GPKG").CreateVector(tmp_vsimem / "src.gpkg")
    src_ds.CreateLayer("test", options=["FID=my_fid"])

    with gdal.alg.vector.create(input=src_ds, output=tmp_vsimem / "out.gpkg") as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetName() == "test"
        assert out_lyr.GetFIDColumn() == "my_fid"
        assert out_lyr.GetGeometryColumn() == "geom"
        assert out_lyr.GetGeomType() == ogr.wkbUnknown
        assert out_lyr.GetSpatialRef() is None
        assert out_lyr.GetLayerDefn().GetFieldCount() == 0


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_create_explicit_fid(tmp_vsimem):

    with gdal.alg.vector.create(output=tmp_vsimem / "out.gpkg", fid="my_fid") as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetName() == "out"
        assert out_lyr.GetFIDColumn() == "my_fid"
        assert out_lyr.GetLayerDefn().GetFieldCount() == 0
        assert out_lyr.GetLayerDefn().GetGeomFieldCount() == 0


@pytest.mark.require_driver("GPKG")
def test_gdal_vector_create_compact_crs(tmp_vsimem):

    ogr_schema = r"""
{
    "layers":[
    {
        "name": "layer1",
        "schemaType": "Full",
        "fields": [
        {
            "name": "field1",
            "type": "DateTime",
            "timezone": "+11:30"
        }],
        "geometryFields": [{
            "name": "geom1",
            "type": "Point",
            "coordinateSystem": { "authid" : "EPSG:32632" }
        }]
    }]
}"""

    alg = get_create_alg()
    alg["output"] = str(tmp_vsimem / "test_compact_crs.gpkg")
    alg["schema"] = ogr_schema

    assert alg.Run()
    ds = alg["output"].GetDataset()
    assert ds is not None, "Failed to open dataset"
    layer = ds.GetLayer(0)
    assert layer is not None, "Layer not found"
    geom_defn = layer.GetLayerDefn().GetGeomFieldDefn(0)
    assert geom_defn.GetName() == "geom1", "Unexpected geometry field name"
    assert geom_defn.GetType() == ogr.wkbPoint, "Unexpected geometry type"
    assert geom_defn.GetSpatialRef().GetAuthorityCode(None) == "32632", "Unexpected CRS"
