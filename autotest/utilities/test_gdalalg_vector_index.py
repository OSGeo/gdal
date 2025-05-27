#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector index' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr


def test_gdalalg_vector_index_new_file():

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.Run(
        "vector",
        "index",
        input="../ogr/data/poly.shp",
        output_format="MEM",
        metadata={"FOO": "BAR"},
        progress=my_progress,
    ) as alg:
        assert tab_pct[0] == 1.0
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetName() == "tileindex"
        assert lyr.GetFeatureCount() == 1
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == "27700"
        assert lyr.GetLayerDefn().GetFieldCount() == 1
        f = lyr.GetNextFeature()
        assert f["location"] == "../ogr/data/poly.shp,0"
        assert (
            f.GetGeometryRef().ExportToWkt()
            == "POLYGON ((478315.53125 4762880.5,478315.53125 4765610.5,481645.3125 4765610.5,481645.3125 4762880.5,478315.53125 4762880.5))"
        )
        assert lyr.GetMetadata_Dict() == {"FOO": "BAR"}


def test_gdalalg_vector_index_absolute_path():

    with gdal.Run(
        "vector",
        "index",
        input="../ogr/data/poly.shp",
        output_format="MEM",
        absolute_path=True,
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetName() == "tileindex"
        assert lyr.GetFeatureCount() == 1
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == "27700"
        assert lyr.GetLayerDefn().GetFieldCount() == 1
        f = lyr.GetNextFeature()
        assert (
            f["location"].replace("\\", "/")
            == os.path.join(os.getcwd(), "../ogr/data/poly.shp")
            .replace("\\", "/")
            .replace("utilities/../", "")
            + ",0"
        )


def test_gdalalg_vector_index_new_file_source_no_crs(tmp_vsimem):

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "no_crs.shp"
    ) as ds:
        ds.CreateLayer("no_crs", geom_type=ogr.wkbPoint)

    with gdal.Run(
        "vector", "index", input=tmp_vsimem / "no_crs.shp", output_format="MEM"
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetName() == "tileindex"
        assert lyr.GetFeatureCount() == 1
        assert lyr.GetSpatialRef() is None
        assert lyr.GetLayerDefn().GetFieldCount() == 1


def test_gdalalg_vector_index_new_file_source_as_dataset(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")

    with pytest.raises(
        Exception, match="Input datasets must be provided by name, not as object"
    ):
        gdal.Run("vector", "index", input=src_ds, output_format="MEM")


def test_gdalalg_vector_index_cannot_create_output_file():

    with pytest.raises(Exception, match="Failed to create file"):
        gdal.Run(
            "vector",
            "index",
            input="../ogr/data/poly.shp",
            output="/i_can/not/be/created.shp",
        )


def test_gdalalg_vector_index_new_file_dst_crs(tmp_vsimem):

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "dst_crs.shp"
    ) as ds:
        lyr = ds.CreateLayer("dst_crs", srs=osr.SpatialReference(epsg=32631))
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(500000 0)"))
        lyr.CreateFeature(f)

    with gdal.Run(
        "vector",
        "index",
        input=tmp_vsimem / "dst_crs.shp",
        output_format="MEM",
        dst_crs="EPSG:4326",
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"
        assert lyr.GetLayerDefn().GetFieldCount() == 1
        f = lyr.GetNextFeature()
        ogrtest.check_feature_geometry(f, "POLYGON ((3 0,3 0,3 0,3 0,3 0))")


def test_gdalalg_vector_index_store_source_crs_auto_as_epsg(tmp_vsimem):

    with gdal.Run(
        "vector",
        "index",
        input="../ogr/data/poly.shp",
        output_format="MEM",
        source_crs_field_name="crs",
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldCount() == 2
        f = lyr.GetNextFeature()
        assert f["crs"] == "EPSG:27700"


def test_gdalalg_vector_index_store_source_crs_auto_as_wkt(tmp_vsimem):

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "test.shp"
    ) as ds:
        ds.CreateLayer(
            "test", srs=osr.SpatialReference("+proj=utm +zone=31 +ellps=GRS80")
        )

    with gdal.Run(
        "vector",
        "index",
        input=tmp_vsimem / "test.shp",
        output_format="MEM",
        source_crs_field_name="crs",
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldCount() == 2
        f = lyr.GetNextFeature()
        assert f["crs"].startswith("PROJCS[")


def test_gdalalg_vector_index_store_source_crs_auto_as_proj(tmp_vsimem):

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "test.shp"
    ) as ds:
        ds.CreateLayer(
            "test", srs=osr.SpatialReference("+proj=utm +zone=31 +ellps=GRS80")
        )

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "index.gpkg"
    ) as ds:
        lyr = ds.CreateLayer(
            "tileindex", srs=osr.SpatialReference("+proj=utm +zone=31 +ellps=GRS80")
        )
        lyr.CreateField(ogr.FieldDefn("location"))
        fld_defn = ogr.FieldDefn("crs")
        fld_defn.SetWidth(50)
        lyr.CreateField(fld_defn)

    with gdal.Run(
        "vector",
        "index",
        input=tmp_vsimem / "test.shp",
        output=tmp_vsimem / "index.gpkg",
        append=True,
        source_crs_field_name="crs",
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldCount() == 2
        f = lyr.GetNextFeature()
        assert f["crs"].startswith("+proj=utm ")


def test_gdalalg_vector_index_store_source_crs_epsg(tmp_vsimem):

    with gdal.Run(
        "vector",
        "index",
        input="../ogr/data/poly.shp",
        output_format="MEM",
        source_crs_field_name="crs",
        source_crs_format="EPSG",
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldCount() == 2
        f = lyr.GetNextFeature()
        assert f["crs"] == "EPSG:27700"


def test_gdalalg_vector_index_store_source_crs_wkt(tmp_vsimem):

    with gdal.Run(
        "vector",
        "index",
        input="../ogr/data/poly.shp",
        output_format="MEM",
        source_crs_field_name="crs",
        source_crs_format="WKT",
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldCount() == 2
        f = lyr.GetNextFeature()
        assert f["crs"].startswith("PROJCS")


def test_gdalalg_vector_index_location_nameand_source_crs_field_name(tmp_vsimem):

    alg = gdal.Run(
        "vector",
        "index",
        input="../ogr/data/poly.shp",
        output_format="MEM",
        location_name="my_loc",
        source_crs_field_name="my_crs",
    )
    ds = alg.Output()
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["my_loc"] == "../ogr/data/poly.shp,0"
    assert f["my_crs"] == "EPSG:27700"

    with pytest.raises(
        Exception, match="Unable to find field 'wrong_location_name' in output layer"
    ):
        gdal.Run(
            "vector",
            "index",
            input="../ogr/data/poly.shp",
            output=ds,
            location_name="wrong_location_name",
            source_crs_field_name="my_crs",
            append=True,
        )

    with pytest.raises(
        Exception,
        match="Unable to find field 'wrong_source_crs_field_name' in output layer",
    ):
        gdal.Run(
            "vector",
            "index",
            input="../ogr/data/poly.shp",
            output=ds,
            location_name="my_loc",
            source_crs_field_name="wrong_source_crs_field_name",
            append=True,
        )


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_index_store_source_crs_wkt_too_long(tmp_vsimem):

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "index.gpkg"
    ) as ds:
        lyr = ds.CreateLayer("tileindex", srs=osr.SpatialReference(epsg=27700))
        lyr.CreateField(ogr.FieldDefn("location"))
        fld_defn = ogr.FieldDefn("crs")
        fld_defn.SetWidth(10)
        lyr.CreateField(fld_defn)

    with gdaltest.error_raised(gdal.CE_Warning):
        with gdal.Run(
            "vector",
            "index",
            input="../ogr/data/poly.shp",
            output=tmp_vsimem / "index.gpkg",
            append=True,
            source_crs_field_name="crs",
            source_crs_format="WKT",
        ) as alg:
            ds = alg.Output()
            lyr = ds.GetLayer(0)
            assert lyr.GetLayerDefn().GetFieldCount() == 2
            f = lyr.GetNextFeature()
            assert f["crs"] is None


def test_gdalalg_vector_index_store_source_crs_proj(tmp_vsimem):

    with gdal.Run(
        "vector",
        "index",
        input="../ogr/data/poly.shp",
        output_format="MEM",
        source_crs_field_name="crs",
        source_crs_format="PROJ",
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldCount() == 2
        f = lyr.GetNextFeature()
        assert f["crs"].startswith("+proj=tmerc ")


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_index_append(tmp_vsimem):

    assert gdal.Run(
        "vector", "index", input="../ogr/data/poly.shp", output=tmp_vsimem / "out.shp"
    )
    with gdaltest.error_raised(gdal.CE_Warning):
        assert gdal.Run(
            "vector",
            "index",
            input="../ogr/data/poly.shp",
            output=tmp_vsimem / "out.shp",
            accept_different_schemas=True,
            append=True,
        )
    with gdal.OpenEx(tmp_vsimem / "out.shp") as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 1
    assert gdal.Run(
        "vector",
        "index",
        input="../ogr/data/gpkg/poly.gpkg.zip",
        output=tmp_vsimem / "out.shp",
        accept_different_schemas=True,
        append=True,
    )
    with gdal.OpenEx(tmp_vsimem / "out.shp") as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 2


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_index_create_location_field_failed(tmp_vsimem):

    with pytest.raises(Exception):
        gdal.Run(
            "vector",
            "index",
            input="../ogr/data/poly.shp",
            output=tmp_vsimem / "out.gpkg",
            location_name="fid",
        )


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_index_create_source_crs_field_failed(tmp_vsimem):

    with pytest.raises(Exception):
        gdal.Run(
            "vector",
            "index",
            input="../ogr/data/poly.shp",
            output=tmp_vsimem / "out.gpkg",
            source_crs_field_name="fid",
        )


def test_gdalalg_vector_index_different_crs(tmp_vsimem):

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "test.shp"
    ) as ds:
        ds.CreateLayer(
            "test", srs=osr.SpatialReference("+proj=utm +zone=31 +ellps=GRS80")
        )

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "index.gpkg"
    ) as ds:
        lyr = ds.CreateLayer(
            "tileindex", srs=osr.SpatialReference("+proj=utm +zone=30 +ellps=GRS80")
        )
        lyr.CreateField(ogr.FieldDefn("location"))

    with gdaltest.error_raised(gdal.CE_Warning):
        with gdal.Run(
            "vector",
            "index",
            input=tmp_vsimem / "test.shp",
            output=tmp_vsimem / "index.gpkg",
            append=True,
        ) as alg:
            ds = alg.Output()
            lyr = ds.GetLayer(0)
            assert lyr.GetFeatureCount() == 0

    with gdaltest.error_raised(gdal.CE_Warning):
        with gdal.Run(
            "vector",
            "index",
            input=tmp_vsimem / "test.shp",
            output=tmp_vsimem / "index.gpkg",
            append=True,
            accept_different_crs=True,
        ) as alg:
            ds = alg.Output()
            lyr = ds.GetLayer(0)
            assert lyr.GetFeatureCount() == 1


def test_gdalalg_vector_index_different_schemas(tmp_vsimem):

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "test.shp"
    ) as ds:
        ds.CreateLayer("test")

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "test2.shp"
    ) as ds:
        lyr = ds.CreateLayer("test2")
        lyr.CreateField(ogr.FieldDefn("foo"))

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "test3.shp"
    ) as ds:
        lyr = ds.CreateLayer("test2")
        lyr.CreateField(ogr.FieldDefn("bar"))

    with gdaltest.error_raised(gdal.CE_Warning):
        with gdal.Run(
            "vector",
            "index",
            input=[tmp_vsimem / "test.shp", tmp_vsimem / "test2.shp"],
            output=tmp_vsimem / "index.shp",
        ) as alg:
            ds = alg.Output()
            lyr = ds.GetLayer(0)
            assert lyr.GetFeatureCount() == 1

    with gdal.Run(
        "vector",
        "index",
        input=[
            tmp_vsimem / "test.shp",
            tmp_vsimem / "test2.shp",
            tmp_vsimem / "test3.shp",
        ],
        output=tmp_vsimem / "index.shp",
        overwrite=True,
        accept_different_schemas=True,
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 3


def test_gdalalg_vector_index_empty_source(tmp_vsimem):

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "aspatial.dbf"
    ) as ds:
        ds.CreateLayer("aspatial", geom_type=ogr.wkbNone)

    with gdaltest.error_raised(gdal.CE_Warning):
        with gdal.Run(
            "vector", "index", input=tmp_vsimem / "aspatial.dbf", output_format="MEM"
        ) as alg:
            ds = alg.Output()
            lyr = ds.GetLayer(0)
            assert lyr.GetFeatureCount() == 0


def test_gdalalg_vector_index_reprojection_failed(tmp_vsimem):

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "test.shp"
    ) as ds:
        ds.CreateLayer("test", srs=osr.SpatialReference("+proj=longlat +datum=WGS84"))

    with gdaltest.error_raised(gdal.CE_Warning):
        with gdal.Run(
            "vector",
            "index",
            input=tmp_vsimem / "test.shp",
            output=tmp_vsimem / "index.shp",
            source_crs_field_name="crs",
            dst_crs="+proj=longlat +a=1",
        ) as alg:
            ds = alg.Output()
            lyr = ds.GetLayer(0)
            assert lyr.GetFeatureCount() == 0


def test_gdalalg_vector_index_source_layer_name():

    with gdal.Run(
        "vector",
        "index",
        input="../ogr/data/poly.shp",
        output_format="MEM",
        source_layer_name="poly",
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    with pytest.raises(Exception, match="No layer to index"):
        gdal.Run(
            "vector",
            "index",
            input="../ogr/data/poly.shp",
            output_format="MEM",
            source_layer_name="unknown",
        )


def test_gdalalg_vector_index_source_layer_index():

    with gdal.Run(
        "vector",
        "index",
        input="../ogr/data/poly.shp",
        output_format="MEM",
        source_layer_index=0,
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    with pytest.raises(Exception, match="No layer to index"):
        gdal.Run(
            "vector",
            "index",
            input="../ogr/data/poly.shp",
            output_format="MEM",
            source_layer_index=1,
        )


def test_gdalalg_vector_index_incompatible_options():

    with pytest.raises(Exception):
        gdal.Run(
            "vector",
            "index",
            input="../ogr/data/poly.shp",
            output_format="MEM",
            accept_different_crs=True,
            skip_different_crs=True,
        )

    with pytest.raises(Exception):
        gdal.Run(
            "vector",
            "index",
            input="../ogr/data/poly.shp",
            output_format="MEM",
            source_crs_format="EPSG",
        )

    with gdaltest.error_raised(gdal.CE_Warning):
        assert gdal.Run(
            "vector",
            "index",
            input="../ogr/data/poly.shp",
            output_format="MEM",
            dst_crs="EPSG:4326",
            skip_different_crs=True,
        )


def test_gdalalg_vector_index_filename_filter():

    with gdal.Run(
        "vector",
        "index",
        input="../ogr/data",
        output_format="MEM",
        recursive=True,
        source_layer_name="poly",
        filename_filter="*.shp",
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() > 0

    with pytest.raises(Exception):
        gdal.Run(
            "vector",
            "index",
            input="../ogr/data",
            output_format="MEM",
            recursive=True,
            filename_filter="*.xxx",
        )
