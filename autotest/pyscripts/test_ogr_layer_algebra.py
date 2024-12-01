#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  To test the functionality of ogr_layer_algebra script
# Author:   Radha Krishna Kavuluru <kssvrk@gmail.com>
#
###############################################################################
# Copyright (c) 2022, Radha Krishna Kavuluru <kssvrk@gmail.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import ogrtest
import pytest
import test_py_scripts

from osgeo import ogr

pytestmark = [
    pytest.mark.skipif(
        test_py_scripts.get_py_script("ogr_layer_algebra") is None,
        reason="ogr_layer_algebra.py not available",
    ),
    pytest.mark.skipif(not ogrtest.have_geos(), reason="GEOS missing"),
]


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("ogr_layer_algebra")


###############################################################################
#


def test_ogr_layer_algebra_help(script_path):

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("fails on sanitize for unknown reason")

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "ogr_layer_algebra", "--help"
    )


###############################################################################
#


def test_ogr_layer_algebra_version(script_path):

    if gdaltest.is_travis_branch("sanitize"):
        pytest.skip("fails on sanitize for unknown reason")

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "ogr_layer_algebra", "--version"
    )


###############################################################################

# Test Intersection


def test_ogr_layer_algebra_intersection(script_path, tmp_path):

    # Create input,method,output paths for intersection.
    input_path = str(tmp_path / "input_layer.shp")
    method_path = str(tmp_path / "method_layer.shp")
    output_path = str(tmp_path / "output_layer.shp")

    # definition of input,method layers
    input_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(input_path)
    method_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(method_path)

    A = input_layer.CreateLayer("poly")
    B = method_layer.CreateLayer("poly")

    a1 = "POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))"
    a2 = "POLYGON((5 2, 5 3, 7 3, 7 2, 5 2))"
    b1 = "POLYGON((2 1, 2 4, 6 4, 6 1, 2 1))"

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a1))
    A.CreateFeature(feat)

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a2))
    A.CreateFeature(feat)

    input_layer = None

    feat = ogr.Feature(B.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=b1))
    B.CreateFeature(feat)

    method_layer = None

    # executing script
    _, err = test_py_scripts.run_py_script(
        script_path,
        "ogr_layer_algebra",
        f"Intersection -input_ds {input_path} -output_ds {output_path}  -method_ds {method_path}",
        return_stderr=True,
    )
    assert "UseExceptions" not in err

    driver = ogr.GetDriverByName("ESRI Shapefile")
    dataSource = driver.Open(output_path, 0)
    layer = dataSource.GetLayer()
    featureCount = layer.GetFeatureCount()

    assert featureCount == 2


###############################################################################

# Test Union


def test_ogr_layer_algebra_union(script_path, tmp_path):

    # Create input,method,output paths for intersection.
    input_path = str(tmp_path / "input_layer.shp")
    method_path = str(tmp_path / "method_layer.shp")
    output_path = str(tmp_path / "output_layer.shp")

    # definition of input,method layers
    input_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(input_path)
    method_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(method_path)

    A = input_layer.CreateLayer("poly")
    B = method_layer.CreateLayer("poly")

    a1 = "POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))"
    a2 = "POLYGON((5 2, 5 3, 7 3, 7 2, 5 2))"
    b1 = "POLYGON((2 1, 2 4, 6 4, 6 1, 2 1))"

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a1))
    A.CreateFeature(feat)

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a2))
    A.CreateFeature(feat)

    input_layer = None

    feat = ogr.Feature(B.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=b1))
    B.CreateFeature(feat)

    method_layer = None

    # executing script
    test_py_scripts.run_py_script(
        script_path,
        "ogr_layer_algebra",
        f"Union -input_ds {input_path} -output_ds {output_path}  -method_ds {method_path}",
    )

    driver = ogr.GetDriverByName("ESRI Shapefile")
    dataSource = driver.Open(output_path, 0)
    layer = dataSource.GetLayer()
    featureCount = layer.GetFeatureCount()

    assert featureCount == 5


###############################################################################

# Test Symmetric Difference


def test_ogr_layer_algebra_symdifference(script_path, tmp_path):

    # Create input,method,output paths for intersection.
    input_path = str(tmp_path / "input_layer.shp")
    method_path = str(tmp_path / "method_layer.shp")
    output_path = str(tmp_path / "output_layer.shp")

    # definition of input,method layers
    input_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(input_path)
    method_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(method_path)

    A = input_layer.CreateLayer("poly")
    B = method_layer.CreateLayer("poly")

    a1 = "POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))"
    a2 = "POLYGON((5 2, 5 3, 7 3, 7 2, 5 2))"
    b1 = "POLYGON((2 1, 2 4, 6 4, 6 1, 2 1))"
    b2 = "POLYGON((2 4, 2 6, 6 6, 6 4, 2 4))"

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a1))
    A.CreateFeature(feat)

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a2))
    A.CreateFeature(feat)

    input_layer = None

    feat = ogr.Feature(B.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=b1))
    B.CreateFeature(feat)

    feat = ogr.Feature(B.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=b2))
    B.CreateFeature(feat)

    method_layer = None

    # executing script
    test_py_scripts.run_py_script(
        script_path,
        "ogr_layer_algebra",
        f"SymDifference -input_ds {input_path} -output_ds {output_path}  -method_ds {method_path}",
    )

    driver = ogr.GetDriverByName("ESRI Shapefile")
    dataSource = driver.Open(output_path, 0)
    layer = dataSource.GetLayer()
    featureCount = layer.GetFeatureCount()

    assert featureCount == 4


###############################################################################

# Test Identity


def test_ogr_layer_algebra_identity(script_path, tmp_path):

    # Create input,method,output paths for intersection.
    input_path = str(tmp_path / "input_layer.shp")
    method_path = str(tmp_path / "method_layer.shp")
    output_path = str(tmp_path / "output_layer.shp")

    # definition of input,method layers
    input_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(input_path)
    method_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(method_path)

    A = input_layer.CreateLayer("poly")
    B = method_layer.CreateLayer("poly")

    a1 = "POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))"
    a2 = "POLYGON((5 2, 5 3, 7 3, 7 2, 5 2))"
    b1 = "POLYGON((2 1, 2 4, 6 4, 6 1, 2 1))"
    b2 = "POLYGON((2 4, 2 6, 6 6, 6 4, 2 4))"

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a1))
    A.CreateFeature(feat)

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a2))
    A.CreateFeature(feat)

    input_layer = None

    feat = ogr.Feature(B.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=b1))
    B.CreateFeature(feat)

    feat = ogr.Feature(B.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=b2))
    B.CreateFeature(feat)

    method_layer = None

    # executing script
    test_py_scripts.run_py_script(
        script_path,
        "ogr_layer_algebra",
        f"Identity -input_ds {input_path} -output_ds {output_path}  -method_ds {method_path}",
    )

    driver = ogr.GetDriverByName("ESRI Shapefile")
    dataSource = driver.Open(output_path, 0)
    layer = dataSource.GetLayer()
    featureCount = layer.GetFeatureCount()

    assert featureCount == 4


###############################################################################

# Test Update


def test_ogr_layer_algebra_update(script_path, tmp_path):

    # Create input,method,output paths for intersection.
    input_path = str(tmp_path / "input_layer.shp")
    method_path = str(tmp_path / "method_layer.shp")
    output_path = str(tmp_path / "output_layer.shp")

    # definition of input,method layers
    input_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(input_path)
    method_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(method_path)

    A = input_layer.CreateLayer("poly")
    B = method_layer.CreateLayer("poly")

    a1 = "POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))"
    a2 = "POLYGON((5 2, 5 3, 7 3, 7 2, 5 2))"
    b1 = "POLYGON((2 1, 2 4, 6 4, 6 1, 2 1))"

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a1))
    A.CreateFeature(feat)

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a2))
    A.CreateFeature(feat)

    input_layer = None

    feat = ogr.Feature(B.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=b1))
    B.CreateFeature(feat)

    method_layer = None

    # executing script
    test_py_scripts.run_py_script(
        script_path,
        "ogr_layer_algebra",
        f"Update -input_ds {input_path} -output_ds {output_path}  -method_ds {method_path}",
    )

    driver = ogr.GetDriverByName("ESRI Shapefile")
    dataSource = driver.Open(output_path, 0)
    layer = dataSource.GetLayer()
    featureCount = layer.GetFeatureCount()

    assert featureCount == 3


###############################################################################

# Test Clip


def test_ogr_layer_algebra_clip(script_path, tmp_path):

    # Create input,method,output paths for intersection.
    input_path = str(tmp_path / "input_layer.shp")
    method_path = str(tmp_path / "method_layer.shp")
    output_path = str(tmp_path / "output_layer.shp")

    # definition of input,method layers
    input_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(input_path)
    method_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(method_path)

    A = input_layer.CreateLayer("poly")
    B = method_layer.CreateLayer("poly")

    a1 = "POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))"
    a2 = "POLYGON((5 2, 5 3, 7 3, 7 2, 5 2))"
    b1 = "POLYGON((2 1, 2 4, 6 4, 6 1, 2 1))"

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a1))
    A.CreateFeature(feat)

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a2))
    A.CreateFeature(feat)

    input_layer = None

    feat = ogr.Feature(B.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=b1))
    B.CreateFeature(feat)

    method_layer = None

    # executing script
    test_py_scripts.run_py_script(
        script_path,
        "ogr_layer_algebra",
        f"Clip -input_ds {input_path} -output_ds {output_path}  -method_ds {method_path}",
    )

    driver = ogr.GetDriverByName("ESRI Shapefile")
    dataSource = driver.Open(output_path, 0)
    layer = dataSource.GetLayer()
    featureCount = layer.GetFeatureCount()

    assert featureCount == 2


###############################################################################

# Test Erase


def test_ogr_layer_algebra_erase(script_path, tmp_path):

    # Create input,method,output paths for intersection.
    input_path = str(tmp_path / "input_layer.shp")
    method_path = str(tmp_path / "method_layer.shp")
    output_path = str(tmp_path / "output_layer.shp")

    # definition of input,method layers
    input_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(input_path)
    method_layer = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(method_path)

    A = input_layer.CreateLayer("poly")
    B = method_layer.CreateLayer("poly")

    a1 = "POLYGON((1 2, 1 3, 3 3, 3 2, 1 2))"
    a2 = "POLYGON((5 2, 5 3, 7 3, 7 2, 5 2))"
    b1 = "POLYGON((2 1, 2 4, 6 4, 6 1, 2 1))"
    b2 = "POLYGON((2 4, 2 6, 6 6, 6 4, 2 4))"

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a1))
    A.CreateFeature(feat)

    feat = ogr.Feature(A.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=a2))
    A.CreateFeature(feat)

    input_layer = None

    feat = ogr.Feature(B.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=b1))
    B.CreateFeature(feat)

    feat = ogr.Feature(B.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=b2))
    B.CreateFeature(feat)

    method_layer = None

    # executing script
    test_py_scripts.run_py_script(
        script_path,
        "ogr_layer_algebra",
        f"Erase -input_ds {input_path} -output_ds {output_path}  -method_ds {method_path}",
    )

    driver = ogr.GetDriverByName("ESRI Shapefile")
    dataSource = driver.Open(output_path, 0)
    layer = dataSource.GetLayer()
    featureCount = layer.GetFeatureCount()

    assert featureCount == 2
