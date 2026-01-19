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

from osgeo import gdal, ogr, osr


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


###############################################################################
# Test from a polar projected CRS to geographic


@pytest.mark.require_geos
@pytest.mark.parametrize(
    "input_wkt,output_wkt",
    [
        (
            "POLYGON((0 100000,100000 0,0 -100000,-100000 0,0 100000),(0 50000,50000 0,0 -50000,-50000 0,0 50000))",
            "POLYGON ((90.0 89.089200825091,0.0 89.089200825091,-90 89.089200825091,-180 89.0892008251069,-180 89.5445935108883,-90 89.5445935108803,0.0 89.5445935108803,90.0 89.5445935108803,180.0 89.5445935108883,180.0 89.0892008251069,90.0 89.089200825091))",
        ),
        (
            "POLYGON((50000 -100000,100000 -100000,100000 100000,-100000 100000,-100000 50000,50000 50000,50000 -100000))",
            "MULTIPOLYGON (((135.0 88.7119614804959,45.0 88.7119614804959,26.565051177078 88.9817007095479,135.0 89.3559612202261,180.0 89.5445935108803,180.0 89.089200825091,135.0 88.7119614804959)),((-116.565051177078 88.9817007095479,-135 88.7119614804959,-180 89.089200825091,-180 89.5445935108803,-116.565051177078 88.9817007095479)))",
        ),
    ],
)
def test_gdalalg_vector_reproject_polar_projected_to_geographic(input_wkt, output_wkt):

    srs_3996 = osr.SpatialReference()
    srs_3996.ImportFromEPSG(3996)
    srs_3996.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = ds.CreateLayer("test", srs=srs_3996)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt(input_wkt))
    lyr.CreateFeature(f)

    with gdal.Run(
        "vector",
        "reproject",
        input=ds,
        output="",
        output_format="MEM",
        dst_crs="EPSG:4326",
    ) as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        out_f = out_lyr.GetNextFeature()
        out_g = out_f.GetGeometryRef()
        ogrtest.check_feature_geometry(out_g, output_wkt)
