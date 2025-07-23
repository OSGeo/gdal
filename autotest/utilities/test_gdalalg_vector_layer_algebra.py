#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector layer-algebra' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_geos


def test_gdal_vector_layer_algebra_union():

    input_ds = gdal.GetDriverByName("MEM").CreateVector("")
    input_lyr = input_ds.CreateLayer("input")
    input_lyr.CreateField(ogr.FieldDefn("a"))
    input_lyr.CreateField(ogr.FieldDefn("ignored"))
    f = ogr.Feature(input_lyr.GetLayerDefn())
    f["a"] = "foo"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 2)"))
    input_lyr.CreateFeature(f)

    f = ogr.Feature(input_lyr.GetLayerDefn())
    f["a"] = "foo2"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(3 4)"))
    input_lyr.CreateFeature(f)

    method_ds = gdal.GetDriverByName("MEM").CreateVector("")
    method_lyr = method_ds.CreateLayer("method")
    method_lyr.CreateField(ogr.FieldDefn("b"))
    method_lyr.CreateField(ogr.FieldDefn("ignored"))
    f = ogr.Feature(method_lyr.GetLayerDefn())
    f["b"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(5 6)"))
    method_lyr.CreateFeature(f)

    f = ogr.Feature(method_lyr.GetLayerDefn())
    f["b"] = "bar2"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(3 4)"))
    method_lyr.CreateFeature(f)

    with gdal.Run(
        "vector",
        "layer-algebra",
        operation="union",
        input=input_ds,
        method=method_ds,
        output_format="MEM",
        geometry_type="MULTIPOINT",
        input_field=["a", "non_existing"],
        method_field=["b", "non_existing"],
    ) as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetName() == "output"
        assert out_lyr.GetLayerDefn().GetFieldCount() == 2
        assert out_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "input_a"
        assert out_lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "method_b"
        assert out_lyr.GetFeatureCount() == 3

        f = out_lyr.GetNextFeature()
        assert f["input_a"] == "foo"
        assert not f.IsFieldSet("method_b")
        assert f.GetGeometryRef().ExportToIsoWkt() == "MULTIPOINT ((1 2))"

        f = out_lyr.GetNextFeature()
        assert f["input_a"] == "foo2"
        assert f["method_b"] == "bar2"
        assert f.GetGeometryRef().ExportToIsoWkt() == "MULTIPOINT ((3 4))"

        f = out_lyr.GetNextFeature()
        assert f["method_b"] == "bar"
        assert not f.IsFieldSet("input_a")
        assert f.GetGeometryRef().ExportToIsoWkt() == "MULTIPOINT ((5 6))"


def test_gdal_vector_layer_algebra_input_and_method_same():

    ds = gdal.GetDriverByName("MEM").CreateVector("")

    with pytest.raises(Exception, match="Input and method datasets must be different"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input=ds,
            method=ds,
            output_format="MEM",
        )


@pytest.mark.require_driver("GPKG")
def test_gdal_vector_layer_algebra_cannot_create_output():

    input_ds = gdal.GetDriverByName("MEM").CreateVector("")
    input_ds.CreateLayer("")
    method_ds = gdal.GetDriverByName("MEM").CreateVector("")
    method_ds.CreateLayer("")
    with pytest.raises(Exception):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input=input_ds,
            method=method_ds,
            output="/i_do/not/exist.gpkg",
        )


@pytest.mark.require_driver("GPKG")
def test_gdal_vector_layer_algebra_cannot_create_layer(tmp_vsimem):

    input_ds = gdal.GetDriverByName("MEM").CreateVector("")
    input_ds.CreateLayer("")
    method_ds = gdal.GetDriverByName("MEM").CreateVector("")
    method_ds.CreateLayer("")
    with pytest.raises(
        Exception, match="name may not contain special characters or spaces"
    ):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input=input_ds,
            method=method_ds,
            output=tmp_vsimem / "out.gpkg",
            layer_creation_option={"FID": "~!@#$%^&*()"},
        )


@pytest.mark.require_driver("GPKG")
def test_gdal_vector_layer_algebra_cannot_create_field_from_input(tmp_vsimem):

    input_ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = input_ds.CreateLayer("")
    lyr.CreateField(ogr.FieldDefn("geom"))
    method_ds = gdal.GetDriverByName("MEM").CreateVector("")
    method_ds.CreateLayer("")
    with pytest.raises(Exception, match="Cannot create field geom"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input=input_ds,
            method=method_ds,
            output=tmp_vsimem / "out.gpkg",
            input_prefix="",
        )


@pytest.mark.require_driver("GPKG")
def test_gdal_vector_layer_algebra_cannot_create_field_from_method(tmp_vsimem):

    input_ds = gdal.GetDriverByName("MEM").CreateVector("")
    input_ds.CreateLayer("")
    method_ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = method_ds.CreateLayer("")
    lyr.CreateField(ogr.FieldDefn("geom"))
    with pytest.raises(Exception, match="Cannot create field geom"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input=input_ds,
            method=method_ds,
            output=tmp_vsimem / "out.gpkg",
            method_prefix="",
        )


def test_gdal_vector_layer_algebra_cannot_guess_output_format(tmp_vsimem):

    input_ds = gdal.GetDriverByName("MEM").CreateVector("")
    input_ds.CreateLayer("")
    method_ds = gdal.GetDriverByName("MEM").CreateVector("")
    method_ds.CreateLayer("")
    with pytest.raises(Exception, match="Cannot guess driver"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input=input_ds,
            method=method_ds,
            output=tmp_vsimem / "out.xxx",
        )


def test_gdal_vector_layer_algebra_cannot_get_input_layer(tmp_vsimem):

    input_ds = gdal.GetDriverByName("MEM").CreateVector("")
    input_ds.CreateLayer("")
    method_ds = gdal.GetDriverByName("MEM").CreateVector("")
    method_ds.CreateLayer("")
    with pytest.raises(Exception, match="Cannot get input layer 'foo'"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input=input_ds,
            method=method_ds,
            input_layer="foo",
            output=tmp_vsimem / "out.shp",
        )


def test_gdal_vector_layer_algebra_cannot_get_method_layer(tmp_vsimem):

    input_ds = gdal.GetDriverByName("MEM").CreateVector("")
    input_ds.CreateLayer("")
    method_ds = gdal.GetDriverByName("MEM").CreateVector("")
    method_ds.CreateLayer("")
    with pytest.raises(Exception, match="Cannot get method layer 'foo'"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input=input_ds,
            method=method_ds,
            method_layer="foo",
            output=tmp_vsimem / "out.shp",
        )


def test_gdal_vector_layer_algebra_overwrite(tmp_vsimem):

    gdal.Run(
        "vector",
        "layer-algebra",
        operation="union",
        input="../ogr/data/poly.shp",
        method="../ogr/data/poly.shp",
        output=tmp_vsimem / "out.shp",
        no_input_field=True,
        no_method_field=True,
    )

    # Test missing overwrite
    with pytest.raises(
        Exception,
        match="already exists",
    ):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input="../ogr/data/poly.shp",
            method="../ogr/data/poly.shp",
            output=tmp_vsimem / "out.shp",
            no_input_field=True,
            no_method_field=True,
        )

    # Test update
    with pytest.raises(Exception, match="--output-layer should be specified"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input="../ogr/data/poly.shp",
            method="../ogr/data/poly.shp",
            output=tmp_vsimem / "out.shp",
            update=True,
            no_input_field=True,
            no_method_field=True,
        )

    # Test append
    with gdal.Run(
        "vector",
        "layer-algebra",
        operation="union",
        input="../ogr/data/poly.shp",
        method="../ogr/data/poly.shp",
        output=tmp_vsimem / "out.shp",
        append=True,
        no_input_field=True,
        no_method_field=True,
    ) as alg:
        assert alg.Output().GetLayer(0).GetFeatureCount() == 20

    # Test overwrite
    with gdal.Run(
        "vector",
        "layer-algebra",
        operation="union",
        input="../ogr/data/poly.shp",
        method="../ogr/data/poly.shp",
        output=tmp_vsimem / "out.shp",
        overwrite=True,
        no_input_field=True,
        no_method_field=True,
    ) as alg:
        assert alg.Output().GetLayer(0).GetFeatureCount() == 10

    with gdal.Run(
        "vector",
        "layer-algebra",
        operation="union",
        input="../ogr/data/poly.shp",
        method="../ogr/data/poly.shp",
        output=tmp_vsimem / "out.shp",
        append=True,
        no_input_field=True,
        no_method_field=True,
    ) as alg:
        assert alg.Output().GetLayer(0).GetFeatureCount() == 20

    # Test overwrite_layer
    with gdal.Run(
        "vector",
        "layer-algebra",
        operation="union",
        input="../ogr/data/poly.shp",
        method="../ogr/data/poly.shp",
        output=tmp_vsimem / "out.shp",
        overwrite_layer=True,
        no_input_field=True,
        no_method_field=True,
    ) as alg:
        assert alg.Output().GetLayer(0).GetFeatureCount() == 10


def test_gdal_vector_layer_algebra_delete_layer_single_fail(tmp_vsimem):

    gdal.Run(
        "vector",
        "layer-algebra",
        operation="union",
        input="../ogr/data/poly.shp",
        method="../ogr/data/poly.shp",
        output=tmp_vsimem / "out.shz",
        no_input_field=True,
        no_method_field=True,
    )

    with pytest.raises(Exception, match=".shz does not support layer deletion"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input="../ogr/data/poly.shp",
            method="../ogr/data/poly.shp",
            output=tmp_vsimem / "out.shz",
            overwrite_layer=True,
            no_input_field=True,
            no_method_field=True,
        )

    with pytest.raises(Exception, match=".shz does not support layer deletion"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input="../ogr/data/poly.shp",
            method="../ogr/data/poly.shp",
            output=tmp_vsimem / "out.shz",
            overwrite_layer=True,
            output_layer="out",
            no_input_field=True,
            no_method_field=True,
        )


def test_gdal_vector_layer_algebra_output_driver_not_existing(tmp_vsimem):

    alg = gdal.Algorithm("vector", "layer-algebra")
    alg.ParseCommandLineArguments(
        [
            "union",
            "../ogr/data/poly.shp",
            "../ogr/data/poly.shp",
            tmp_vsimem / "out.shp",
            "--output-format",
            "ESRI Shapefile",
        ]
    )

    drv = gdal.GetDriverByName("ESRI Shapefile")
    drv.Deregister()
    try:
        with pytest.raises(Exception, match="Driver ESRI Shapefile does not exist"):
            alg.Run()
    finally:
        drv.Register()


@pytest.mark.require_driver("GPKG")
def test_gdal_vector_layer_algebra_overwrite_multilayer(tmp_vsimem):

    gdal.Run(
        "vector",
        "layer-algebra",
        operation="union",
        input="../ogr/data/poly.shp",
        method="../ogr/data/poly.shp",
        output=tmp_vsimem / "out.gpkg",
    )
    with gdal.OpenEx(tmp_vsimem / "out.gpkg") as ds:
        assert ds.GetLayer(0).GetName() == "output"

    gdal.Run(
        "vector",
        "layer-algebra",
        operation="union",
        input="../ogr/data/poly.shp",
        method="../ogr/data/poly.shp",
        output=tmp_vsimem / "out.gpkg",
        update=True,
        output_layer="output2",
    )
    with gdal.OpenEx(tmp_vsimem / "out.gpkg", gdal.OF_UPDATE) as ds:
        assert ds.GetLayer(0).GetName() == "output"
        assert ds.GetLayer(0).GetFeatureCount() == 10
        assert ds.GetLayer(1).GetName() == "output2"
        assert ds.GetLayer(1).GetFeatureCount() == 10

    with pytest.raises(Exception, match="--output-layer should be specified"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input="../ogr/data/poly.shp",
            method="../ogr/data/poly.shp",
            output=tmp_vsimem / "out.gpkg",
            update=True,
        )

    with pytest.raises(Exception, match="--output-layer should be specified"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input="../ogr/data/poly.shp",
            method="../ogr/data/poly.shp",
            output=tmp_vsimem / "out.gpkg",
            append=True,
        )

    with pytest.raises(Exception, match="--output-layer should be specified"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input="../ogr/data/poly.shp",
            method="../ogr/data/poly.shp",
            output=tmp_vsimem / "out.gpkg",
            overwrite_layer=True,
        )

    gdal.Run(
        "vector",
        "layer-algebra",
        operation="union",
        input="../ogr/data/poly.shp",
        method="../ogr/data/poly.shp",
        output=tmp_vsimem / "out.gpkg",
        append=True,
        output_layer="output2",
    )
    with gdal.OpenEx(tmp_vsimem / "out.gpkg", gdal.OF_UPDATE) as ds:
        assert ds.GetLayer(0).GetName() == "output"
        assert ds.GetLayer(0).GetFeatureCount() == 10
        assert ds.GetLayer(1).GetName() == "output2"
        assert ds.GetLayer(1).GetFeatureCount() == 20

    gdal.Run(
        "vector",
        "layer-algebra",
        operation="union",
        input="../ogr/data/poly.shp",
        method="../ogr/data/poly.shp",
        output=tmp_vsimem / "out.gpkg",
        overwrite_layer=True,
        output_layer="output2",
    )
    with gdal.OpenEx(tmp_vsimem / "out.gpkg", gdal.OF_UPDATE) as ds:
        assert ds.GetLayer(0).GetName() == "output"
        assert ds.GetLayer(0).GetFeatureCount() == 10
        assert ds.GetLayer(1).GetName() == "output2"
        assert ds.GetLayer(1).GetFeatureCount() == 10

    # --append to non existing layer
    with pytest.raises(Exception, match="Layer 'wrong' does not exist"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input="../ogr/data/poly.shp",
            method="../ogr/data/poly.shp",
            output=tmp_vsimem / "out.gpkg",
            append=True,
            output_layer="wrong",
        )

    # --overwrite-layer non existing layer
    with pytest.raises(Exception, match="Layer 'wrong' does not exist"):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input="../ogr/data/poly.shp",
            method="../ogr/data/poly.shp",
            output=tmp_vsimem / "out.gpkg",
            overwrite_layer=True,
            output_layer="wrong",
        )

    # --update to existing layer
    with pytest.raises(
        Exception,
        match=r"Output layer 'output2' already exists. Specify --overwrite, --overwrite-layer, --append or --update \+ --output-layer with a different name",
    ):
        gdal.Run(
            "vector",
            "layer-algebra",
            operation="union",
            input="../ogr/data/poly.shp",
            method="../ogr/data/poly.shp",
            output=tmp_vsimem / "out.gpkg",
            update=True,
            output_layer="output2",
        )


def get_input_method_datasets():

    input_ds = gdal.GetDriverByName("MEM").CreateVector("")
    input_lyr = input_ds.CreateLayer("input")
    input_lyr.CreateField(ogr.FieldDefn("a"))
    f = ogr.Feature(input_lyr.GetLayerDefn())
    f["a"] = "foo"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,10 0,10 10,0 10,0 0))"))
    input_lyr.CreateFeature(f)

    f = ogr.Feature(input_lyr.GetLayerDefn())
    f["a"] = "foo2"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(-3 4)"))
    input_lyr.CreateFeature(f)

    method_ds = gdal.GetDriverByName("MEM").CreateVector("")
    method_lyr = method_ds.CreateLayer("method")
    method_lyr.CreateField(ogr.FieldDefn("b"))
    f = ogr.Feature(method_lyr.GetLayerDefn())
    f["b"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((5 0,15 0,15 10,5 10,5 0))"))
    method_lyr.CreateFeature(f)

    f = ogr.Feature(method_lyr.GetLayerDefn())
    f["b"] = "bar2"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(-3 4)"))
    method_lyr.CreateFeature(f)

    f = ogr.Feature(method_lyr.GetLayerDefn())
    f["b"] = "bar3"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(-5 6)"))
    method_lyr.CreateFeature(f)

    return input_ds, method_ds


def test_gdal_vector_layer_algebra_intersection():

    input_ds, method_ds = get_input_method_datasets()
    with gdal.Run(
        "vector",
        "layer-algebra",
        operation="intersection",
        input=input_ds,
        method=method_ds,
        output_format="MEM",
    ) as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetName() == "output"
        assert out_lyr.GetLayerDefn().GetFieldCount() == 2
        assert out_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "input_a"
        assert out_lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "method_b"
        assert out_lyr.GetFeatureCount() == 2

        f = out_lyr.GetNextFeature()
        assert f["input_a"] == "foo"
        assert f["method_b"] == "bar"
        assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((5 0,5 10,10 10,10 0,5 0))"

        f = out_lyr.GetNextFeature()
        assert f["input_a"] == "foo2"
        assert f["method_b"] == "bar2"
        assert f.GetGeometryRef().ExportToWkt() == "POINT (-3 4)"


def test_gdal_vector_layer_algebra_sym_difference():

    input_ds, method_ds = get_input_method_datasets()
    with gdal.Run(
        "vector",
        "layer-algebra",
        operation="sym-difference",
        input=input_ds,
        method=method_ds,
        output_format="MEM",
    ) as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetName() == "output"
        assert out_lyr.GetLayerDefn().GetFieldCount() == 2
        assert out_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "input_a"
        assert out_lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "method_b"
        assert out_lyr.GetFeatureCount() == 3

        f = out_lyr.GetNextFeature()
        assert f["input_a"] == "foo"
        assert f["method_b"] is None
        assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((0 0,0 10,5 10,5 0,0 0))"

        f = out_lyr.GetNextFeature()
        assert f["input_a"] is None
        assert f["method_b"] == "bar"
        assert (
            f.GetGeometryRef().ExportToWkt()
            == "POLYGON ((15 10,15 0,10 0,10 10,15 10))"
        )

        f = out_lyr.GetNextFeature()
        assert f["input_a"] is None
        assert f["method_b"] == "bar3"
        assert f.GetGeometryRef().ExportToWkt() == "POINT (-5 6)"


def test_gdal_vector_layer_algebra_identity():

    input_ds, method_ds = get_input_method_datasets()
    with gdal.Run(
        "vector",
        "layer-algebra",
        operation="identity",
        input=input_ds,
        method=method_ds,
        output_format="MEM",
    ) as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetName() == "output"
        assert out_lyr.GetLayerDefn().GetFieldCount() == 2
        assert out_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "input_a"
        assert out_lyr.GetLayerDefn().GetFieldDefn(1).GetName() == "method_b"
        assert out_lyr.GetFeatureCount() == 3

        f = out_lyr.GetNextFeature()
        assert f["input_a"] == "foo"
        assert f["method_b"] == "bar"
        assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((5 0,5 10,10 10,10 0,5 0))"

        f = out_lyr.GetNextFeature()
        assert f["input_a"] == "foo"
        assert f["method_b"] is None
        assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((0 0,0 10,5 10,5 0,0 0))"

        f = out_lyr.GetNextFeature()
        assert f["input_a"] == "foo2"
        assert f["method_b"] == "bar2"
        assert f.GetGeometryRef().ExportToWkt() == "POINT (-3 4)"


def test_gdal_vector_layer_algebra_update():

    input_ds, method_ds = get_input_method_datasets()
    with gdal.Run(
        "vector",
        "layer-algebra",
        operation="update",
        input=input_ds,
        method=method_ds,
        output_format="MEM",
    ) as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetName() == "output"
        assert out_lyr.GetLayerDefn().GetFieldCount() == 1
        assert out_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "a"
        assert out_lyr.GetFeatureCount() == 4

        f = out_lyr.GetNextFeature()
        assert f["a"] == "foo"
        assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((0 0,0 10,5 10,5 0,0 0))"

        f = out_lyr.GetNextFeature()
        assert f["a"] is None
        assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((5 0,15 0,15 10,5 10,5 0))"

        f = out_lyr.GetNextFeature()
        assert f["a"] is None
        assert f.GetGeometryRef().ExportToWkt() == "POINT (-3 4)"

        f = out_lyr.GetNextFeature()
        assert f["a"] is None
        assert f.GetGeometryRef().ExportToWkt() == "POINT (-5 6)"


def test_gdal_vector_layer_algebra_clip():

    input_ds, method_ds = get_input_method_datasets()
    with gdal.Run(
        "vector",
        "layer-algebra",
        operation="clip",
        input=input_ds,
        method=method_ds,
        output_format="MEM",
    ) as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetName() == "output"
        assert out_lyr.GetLayerDefn().GetFieldCount() == 1
        assert out_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "a"
        assert out_lyr.GetFeatureCount() == 2

        f = out_lyr.GetNextFeature()
        assert f["a"] == "foo"
        assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((5 0,5 10,10 10,10 0,5 0))"

        f = out_lyr.GetNextFeature()
        assert f["a"] == "foo2"
        assert f.GetGeometryRef().ExportToWkt() == "POINT (-3 4)"


def test_gdal_vector_layer_algebra_erase():

    input_ds, method_ds = get_input_method_datasets()
    with gdal.Run(
        "vector",
        "layer-algebra",
        operation="erase",
        input=input_ds,
        method=method_ds,
        output_format="MEM",
    ) as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetName() == "output"
        assert out_lyr.GetLayerDefn().GetFieldCount() == 1
        assert out_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "a"
        assert out_lyr.GetFeatureCount() == 1

        f = out_lyr.GetNextFeature()
        assert f["a"] == "foo"
        assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((0 0,0 10,5 10,5 0,0 0))"
