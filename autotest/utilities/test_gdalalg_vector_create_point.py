#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector create-point' testing
# Author:   Dan Baston
#
###############################################################################
# Copyright (c) 2025, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr, osr


@pytest.fixture()
def create_point():
    return gdal.Algorithm("vector", "create-point")


@pytest.mark.parametrize("use_z", (False, True))
@pytest.mark.parametrize("use_m", (False, True))
def test_gdalalg_vector_create_point_basic(create_point, use_z, use_m):

    nfeat = 3

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbNone)

    src_lyr.CreateField(ogr.FieldDefn("name", ogr.OFTString))
    src_lyr.CreateField(ogr.FieldDefn("my_x", ogr.OFTReal))
    src_lyr.CreateField(ogr.FieldDefn("my_y", ogr.OFTReal))
    src_lyr.CreateField(ogr.FieldDefn("my_m", ogr.OFTReal))
    src_lyr.CreateField(ogr.FieldDefn("my_z", ogr.OFTReal))

    feature = ogr.Feature(src_lyr.GetLayerDefn())
    for i in range(nfeat):
        feature["name"] = f"feat_{i}"
        feature["my_x"] = i
        feature["my_y"] = 2 * i
        feature["my_z"] = 3 * i
        feature["my_m"] = 4 * i
        src_lyr.CreateFeature(feature)

    create_point["input"] = src_ds
    create_point["x"] = "my_x"
    create_point["y"] = "my_y"
    if use_z:
        create_point["z"] = "my_z"
    if use_m:
        create_point["m"] = "my_m"
    create_point["dst-crs"] = "EPSG:6589"
    create_point["output"] = ""
    create_point["output-format"] = "MEM"

    assert create_point.Run()

    out_ds = create_point.Output()
    assert out_ds.GetLayerCount() == 1

    out_lyr = out_ds.GetLayer(0)

    assert out_lyr.GetFeatureCount() == nfeat

    out_defn = out_lyr.GetLayerDefn()

    assert out_defn.GetGeomFieldCount() == 1

    out_geom_defn = out_defn.GetGeomFieldDefn(0)
    assert out_geom_defn.GetSpatialRef().IsSame(osr.SpatialReference(epsg=6589))

    type_tokens = []
    if use_z:
        type_tokens.append("3D")
    if use_m:
        type_tokens.append("Measured")
    type_tokens.append("Point")

    assert ogr.GeometryTypeToName(out_geom_defn.GetType()) == " ".join(type_tokens)

    features = [f for f in out_lyr]

    assert len(features) == nfeat

    for i in range(nfeat):
        assert features[i]["name"] == f"feat_{i}"
        assert features[i]["my_x"] == i
        assert features[i]["my_y"] == 2 * i
        assert features[i]["my_z"] == 3 * i
        assert features[i]["my_m"] == 4 * i

        geom = features[i].GetGeometryRef()

        assert geom.GetSpatialReference().IsSame(osr.SpatialReference(epsg=6589))

        if use_z and not use_m:
            assert geom.ExportToIsoWkt() == f"POINT Z ({i} {2 * i} {3 * i})"
        elif use_z and use_m:
            assert geom.ExportToIsoWkt() == f"POINT ZM ({i} {2 * i} {3 * i} {4 * i})"
        elif use_m:
            assert geom.ExportToIsoWkt() == f"POINT M ({i} {2 * i} {4 * i})"
        else:
            assert geom.ExportToIsoWkt() == f"POINT ({i} {2 * i})"


def test_gdalalg_vector_create_point_invalid_srs(create_point):

    with pytest.raises(Exception, match="Invalid value for 'dst-crs'"):
        create_point["dst-crs"] = "invalid"


@pytest.mark.parametrize("value", (" 40m", "", " "))
def test_gdalalg_vector_create_point_invalid_values(create_point, value):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbNone)

    src_lyr.CreateField(ogr.FieldDefn("name", ogr.OFTString))
    src_lyr.CreateField(ogr.FieldDefn("my_x", ogr.OFTString))
    src_lyr.CreateField(ogr.FieldDefn("my_y", ogr.OFTString))

    feature = ogr.Feature(src_lyr.GetLayerDefn())
    feature["name"] = "my_feat"
    feature["my_x"] = 3
    feature["my_y"] = value
    src_lyr.CreateFeature(feature)

    create_point["input"] = src_ds
    create_point["x"] = "my_x"
    create_point["y"] = "my_y"
    create_point["output"] = ""
    create_point["output-format"] = "MEM"

    with pytest.raises(Exception, match="Invalid value in field my_y"):
        create_point.Run()


@pytest.mark.parametrize("invalid_field", ("x", "y", "m", "z"))
def test_gdalalg_vector_create_point_invalid_field_name(create_point, invalid_field):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbNone)

    src_lyr.CreateField(ogr.FieldDefn("name", ogr.OFTString))
    src_lyr.CreateField(ogr.FieldDefn("my_x", ogr.OFTReal))
    src_lyr.CreateField(ogr.FieldDefn("my_y", ogr.OFTReal))
    src_lyr.CreateField(ogr.FieldDefn("my_z", ogr.OFTReal))
    src_lyr.CreateField(ogr.FieldDefn("my_m", ogr.OFTReal))

    feature = ogr.Feature(src_lyr.GetLayerDefn())
    feature["my_x"] = 1
    feature["my_y"] = 2
    feature["my_z"] = 3
    feature["my_m"] = 4

    src_lyr.CreateFeature(feature)

    create_point["input"] = src_ds
    create_point["x"] = "my_x"
    create_point["y"] = "my_y"
    create_point["z"] = "my_z"
    create_point["m"] = "my_m"
    create_point[invalid_field] = "does_not_exist"
    create_point["output"] = ""
    create_point["output-format"] = "MEM"

    with pytest.raises(
        Exception,
        match=f"Specified {invalid_field.upper()} field name .* does not exist",
    ):
        create_point.Run()
