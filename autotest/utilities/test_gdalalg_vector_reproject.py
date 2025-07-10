#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector reproject' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import ogrtest
import pytest

from osgeo import gdal, ogr


def get_reproject_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["reproject"]


# Most of the testing is done in test_gdalalg_vector_pipeline.py


@pytest.mark.require_driver("OSM")
def test_gdalalg_vector_reproject_dataset_getnextfeature():

    alg = get_reproject_alg()
    src_ds = gdal.OpenEx("../ogr/data/osm/test.pbf")
    alg["input"] = src_ds
    alg["dst-crs"] = "EPSG:4326"

    assert alg.ParseCommandLineArguments(
        ["--of", "stream", "--output", "streamed_output"]
    )
    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    assert out_ds.TestCapability(ogr.ODsCRandomLayerRead)

    expected = []
    while True:
        f, _ = src_ds.GetNextFeature()
        if not f:
            break
        expected.append(str(f))

    got = []
    out_ds.ResetReading()
    while True:
        f, _ = out_ds.GetNextFeature()
        if not f:
            break
        got.append(str(f))

    assert expected == got


def test_gdalalg_vector_reproject_active_layer():

    src_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    src_lyr = src_ds.CreateLayer("the_layer")

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 0)"))
    src_lyr.CreateFeature(f)

    src_lyr = src_ds.CreateLayer("other_layer")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 0)"))
    src_lyr.CreateFeature(f)

    alg = get_reproject_alg()
    alg["input"] = src_ds
    alg["active-layer"] = "the_layer"

    assert alg.ParseCommandLineArguments(
        [
            "--src-crs=EPSG:4326",
            "--dst-crs=EPSG:32631",
            "--of",
            "MEM",
            "--output",
            "memory_ds",
        ]
    )
    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, "POINT (500000 0)")

    out_lyr = out_ds.GetLayer(1)
    out_f = out_lyr.GetNextFeature()
    ogrtest.check_feature_geometry(out_f, "POINT (3 0)")


def test_gdalalg_vector_reproject_complete_dst_crs():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")
    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal vector reproject ../ogr/data/poly.shp --dst-crs=EPSG:"
    )
    assert "4326\\ --" in out
    assert "2193\\ --" not in out  # NZGD2000
