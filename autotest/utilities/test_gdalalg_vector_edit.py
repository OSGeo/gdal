#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector edit' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import ogrtest
import pytest

from osgeo import gdal, ogr, osr


def get_edit_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["edit"]


def test_gdalalg_vector_edit_crs():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = src_ds.CreateLayer("the_layer")
    lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)

    alg = get_edit_alg()
    alg["input"] = src_ds

    assert alg.ParseCommandLineArguments(
        ["--crs=EPSG:4326", "--of", "MEM", "--output", "memory_ds"]
    )
    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetSpatialRef().GetAuthorityCode(None) == "4326"
    out_f = out_lyr.GetNextFeature()
    assert out_f.GetGeometryRef().GetSpatialReference().GetAuthorityCode(None) == "4326"
    assert out_f["foo"] == "bar"
    ogrtest.check_feature_geometry(out_f, "POINT (1 2)")


def test_gdalalg_vector_edit_crs_none():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = src_ds.CreateLayer("the_layer", srs=srs)
    lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["foo"] = "bar"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)

    alg = get_edit_alg()
    alg["input"] = src_ds

    assert alg.ParseCommandLineArguments(
        ["--crs=none", "--of", "MEM", "--output", "memory_ds"]
    )
    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetSpatialRef() is None
    out_f = out_lyr.GetNextFeature()
    assert out_f.GetGeometryRef().GetSpatialReference() is None
    assert out_f["foo"] == "bar"
    ogrtest.check_feature_geometry(out_f, "POINT (1 2)")


def test_gdalalg_vector_edit_dataset_metadata():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_ds.SetMetadataItem("A", "B")
    src_ds.SetMetadataItem("B", "C")

    alg = get_edit_alg()
    alg["input"] = src_ds

    assert alg.ParseCommandLineArguments(
        [
            "--metadata=C=D",
            "--unset-metadata=B",
            "--of",
            "MEM",
            "--output",
            "memory_ds",
        ]
    )
    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    assert out_ds.GetMetadata_Dict() == {"A": "B", "C": "D"}


def test_gdalalg_vector_edit_layer_metadata():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_layer = src_ds.CreateLayer("the_layer")
    src_layer.SetMetadataItem("A", "B")
    src_layer.SetMetadataItem("B", "C")

    alg = get_edit_alg()
    alg["input"] = src_ds

    assert alg.ParseCommandLineArguments(
        [
            "--layer-metadata=C=D",
            "--unset-layer-metadata=B",
            "--of",
            "MEM",
            "--output",
            "memory_ds",
        ]
    )
    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    assert out_ds.GetLayer(0).GetMetadata_Dict() == {"A": "B", "C": "D"}


def test_gdalalg_vector_edit_geometry_type_geometry():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_ds.CreateLayer("the_layer", geom_type=ogr.wkbPoint)

    alg = get_edit_alg()
    alg["input"] = src_ds

    assert alg.ParseCommandLineArguments(
        ["--geometry-type=geometry", "--of", "MEM", "--output", "memory_ds"]
    )
    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    assert out_ds.GetLayer(0).GetGeomType() == ogr.wkbUnknown


def test_gdalalg_vector_edit_geometry_type_invalid():

    alg = get_edit_alg()
    with pytest.raises(Exception, match="edit: Invalid geometry type 'invalid'"):
        alg["geometry-type"] = "invalid"


def test_gdalalg_vector_edit_active_layer():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_ds.CreateLayer("the_layer")
    src_ds.CreateLayer("other_layer")

    alg = get_edit_alg()
    alg["input"] = src_ds
    alg["active-layer"] = "the_layer"

    assert alg.ParseCommandLineArguments(
        ["--geometry-type=point", "--of", "MEM", "--output", "memory_ds"]
    )
    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    assert out_lyr.GetGeomType() == ogr.wkbPoint

    out_lyr = out_ds.GetLayer(1)
    assert out_lyr.GetGeomType() == ogr.wkbUnknown


def test_gdalalg_vector_edit_unset_fid():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    lyr = src_ds.CreateLayer("the_layer", options=["FID=my_fid_column"])
    lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(10)
    lyr.CreateFeature(f)

    with gdal.Run("vector", "edit", input=src_ds, output_format="MEM") as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetFIDColumn() == "my_fid_column"
        f = lyr.GetNextFeature()
        assert f.GetFID() == 10

    with gdal.Run(
        "vector", "edit", input=src_ds, output_format="MEM", unset_fid=True
    ) as alg:
        ds = alg.Output()
        lyr = ds.GetLayer(0)
        assert lyr.GetFIDColumn() == ""
        f = lyr.GetNextFeature()
        assert f.GetFID() == 0


@pytest.mark.require_driver("GPKG")
def test_error_message_leak():
    """Test issue GH #13662"""

    alg = get_edit_alg()
    in_filename = "../gdrivers/data/gpkg/byte.gpkg"
    out_filename = in_filename
    with pytest.raises(
        Exception,
        match="--output-layer name must be specified combined with a single source layer name and it must be different from an existing layer.",
    ):
        alg.ParseRunAndFinalize(
            [
                in_filename,
                "--crs=EPSG:4326",
                "--overwrite-layer",
                out_filename,
            ],
        )
