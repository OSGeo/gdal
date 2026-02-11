#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RasterizeLayer() and related calls.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import struct

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr

###############################################################################
# Simple polygon rasterization.


def test_rasterize_1():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    # Create a memory raster to rasterize into.

    target_ds = gdal.GetDriverByName("MEM").Create("", 100, 100, 3, gdal.GDT_UInt8)
    target_ds.SetGeoTransform((1000, 1, 0, 1100, 0, -1))
    target_ds.SetProjection(sr_wkt)

    # Create a memory layer to rasterize from.

    rast_ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("wrk")
    rast_mem_lyr = rast_ogr_ds.CreateLayer("poly", srs=sr)

    # Add a polygon.

    wkt_geom = "POLYGON((1020 1030,1020 1045,1050 1045,1050 1030,1020 1030))"

    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))

    rast_mem_lyr.CreateFeature(feat)

    # Add a linestring.

    wkt_geom = "LINESTRING(1000 1000, 1100 1050)"

    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))

    rast_mem_lyr.CreateFeature(feat)

    # Run the algorithm.

    err = gdal.RasterizeLayer(
        target_ds, [3, 2, 1], rast_mem_lyr, burn_values=[256, 220, -1]
    )

    assert err == 0, "got non-zero result code from RasterizeLayer"

    # Check results.

    expected = 6452
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdal.GetDriverByName("GTiff").CreateCopy("tmp/rasterize_1.tif", target_ds)
        pytest.fail("Did not get expected image checksum")

    _, maxval = target_ds.GetRasterBand(3).ComputeRasterMinMax()
    assert maxval == 255

    minval, _ = target_ds.GetRasterBand(1).ComputeRasterMinMax()
    assert minval == 0


###############################################################################
# Test rasterization with ALL_TOUCHED.


@pytest.mark.require_driver("CSV")
def test_rasterize_2():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'

    # Create a memory raster to rasterize into.

    target_ds = gdal.GetDriverByName("MEM").Create("", 12, 12, 3, gdal.GDT_UInt8)
    target_ds.SetGeoTransform((0, 1, 0, 12, 0, -1))
    target_ds.SetProjection(sr_wkt)

    # Create a memory layer to rasterize from.

    cutline_ds = ogr.Open("data/cutline.csv")

    # Run the algorithm.

    with gdal.quiet_errors():
        err = gdal.RasterizeLayer(
            target_ds,
            [3, 2, 1],
            cutline_ds.GetLayer(0),
            burn_values=[200, 220, 240],
            options=["ALL_TOUCHED=TRUE"],
        )
        assert (
            "Failed to fetch spatial reference on layer cutline to build transformer, assuming matching coordinate systems"
            in gdal.GetLastErrorMsg()
        )

    assert err == 0, "got non-zero result code from RasterizeLayer"

    # Check results.

    expected = 121
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdal.GetDriverByName("GTiff").CreateCopy("tmp/rasterize_2.tif", target_ds)
        pytest.fail("Did not get expected image checksum")


###############################################################################
# Rasterization with BURN_VALUE_FROM.


def test_rasterize_3():

    # Create a memory raster to rasterize into.

    target_ds = gdal.GetDriverByName("MEM").Create("", 100, 100, 3, gdal.GDT_UInt8)
    target_ds.SetGeoTransform((1000, 1, 0, 1100, 0, -1))

    # Create a memory layer to rasterize from.

    rast_ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("wrk")
    rast_mem_lyr = rast_ogr_ds.CreateLayer("poly")

    # Add polygons and linestrings.
    wkt_geom = [
        "POLYGON((1020 1030 40,1020 1045 30,1050 1045 20,1050 1030 35,1020 1030 40))",
        "POLYGON((1010 1046 85,1015 1055 35,1055 1060 26,1054 1048 35,1010 1046 85))",
        "POLYGON((1020 1076 190,1025 1085 35,1065 1090 26,1064 1078 35,1020 1076 190),(1023 1079 5,1061 1081 35,1062 1087 26,1028 1082 35,1023 1079 85))",
        "LINESTRING(1005 1000 10, 1100 1050 120)",
        "LINESTRING(1000 1000 150, 1095 1050 -5, 1080 1080 200)",
    ]
    for g in wkt_geom:
        feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
        feat.SetGeometryDirectly(ogr.Geometry(wkt=g))
        rast_mem_lyr.CreateFeature(feat)

    # Run the algorithm.

    gdal.ErrorReset()
    err = gdal.RasterizeLayer(
        target_ds,
        [3, 2, 1],
        rast_mem_lyr,
        burn_values=[10, 10, 55],
        options=["BURN_VALUE_FROM=Z"],
    )
    assert gdal.GetLastErrorMsg() == ""

    assert err == 0, "got non-zero result code from RasterizeLayer"

    # Check results.

    expected = 15037
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdal.GetDriverByName("GTiff").CreateCopy("tmp/rasterize_3.tif", target_ds)
        pytest.fail("Did not get expected image checksum")


###############################################################################
# Rasterization with ATTRIBUTE.


def test_rasterize_4():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    # Create a memory raster to rasterize into.
    target_ds = gdal.GetDriverByName("MEM").Create("", 100, 100, 3, gdal.GDT_UInt8)
    target_ds.SetGeoTransform((1000, 1, 0, 1100, 0, -1))
    target_ds.SetProjection(sr_wkt)

    # Create a memory layer to rasterize from.
    rast_ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("wrk")
    rast_mem_lyr = rast_ogr_ds.CreateLayer("poly", srs=sr)
    # Setup Schema
    ogrtest.quick_create_layer_def(rast_mem_lyr, [("CELSIUS", ogr.OFTReal)])

    # Add polygons and linestrings and a field named CELSIUS.
    wkt_geom = [
        "POLYGON((1020 1030 40,1020 1045 30,1050 1045 20,1050 1030 35,1020 1030 40))",
        "POLYGON((1010 1046 85,1015 1055 35,1055 1060 26,1054 1048 35,1010 1046 85))",
        "POLYGON((1020 1076 190,1025 1085 35,1065 1090 26,1064 1078 35,1020 1076 190),(1023 1079 5,1061 1081 35,1062 1087 26,1028 1082 35,1023 1079 85))",
        "LINESTRING(1005 1000 10, 1100 1050 120)",
        "LINESTRING(1000 1000 150, 1095 1050 -5, 1080 1080 200)",
    ]
    celsius_field_values = [50, 255, 60, 100, 180]

    i = 0
    for g in wkt_geom:
        feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
        feat.SetGeometryDirectly(ogr.Geometry(wkt=g))
        feat.SetField("CELSIUS", celsius_field_values[i])
        rast_mem_lyr.CreateFeature(feat)
        i = i + 1

    # Run the algorithm.
    err = gdal.RasterizeLayer(
        target_ds, [1, 2, 3], rast_mem_lyr, options=["ATTRIBUTE=CELSIUS"]
    )

    assert err == 0, "got non-zero result code from RasterizeLayer"

    # Check results.
    expected = 16265
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdal.GetDriverByName("GTiff").CreateCopy("tmp/rasterize_4.tif", target_ds)
        pytest.fail("Did not get expected image checksum")


###############################################################################
# Rasterization with MERGE_ALG=ADD.


def test_rasterize_5():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    # Create a memory raster to rasterize into.

    target_ds = gdal.GetDriverByName("MEM").Create("", 100, 100, 3, gdal.GDT_UInt8)
    target_ds.SetGeoTransform((1000, 1, 0, 1100, 0, -1))
    target_ds.SetProjection(sr_wkt)

    # Create a memory layer to rasterize from.

    rast_ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("wrk")
    rast_mem_lyr = rast_ogr_ds.CreateLayer("poly", srs=sr)

    # Add polygons.

    wkt_geom = "POLYGON((1020 1030,1020 1045,1050 1045,1050 1030,1020 1030))"
    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))
    rast_mem_lyr.CreateFeature(feat)

    wkt_geom = "POLYGON((1045 1050,1055 1050,1055 1020,1045 1020,1045 1050))"
    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))
    rast_mem_lyr.CreateFeature(feat)

    # Add linestrings.

    wkt_geom = "LINESTRING(1000 1000, 1100 1050)"
    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))
    rast_mem_lyr.CreateFeature(feat)

    wkt_geom = "LINESTRING(1005 1000, 1000 1050)"
    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt_geom))
    rast_mem_lyr.CreateFeature(feat)

    # Run the algorithm.

    err = gdal.RasterizeLayer(
        target_ds,
        [1, 2, 3],
        rast_mem_lyr,
        burn_values=[256, 110, -1],
        options=["MERGE_ALG=ADD"],
    )

    assert err == 0, "got non-zero result code from RasterizeLayer"

    # Check results.

    expected = 13022
    checksum = target_ds.GetRasterBand(2).Checksum()
    if checksum != expected:
        print(checksum)
        gdal.GetDriverByName("GTiff").CreateCopy("tmp/rasterize_5.tif", target_ds)
        pytest.fail("Did not get expected image checksum")

    _, maxval = target_ds.GetRasterBand(1).ComputeRasterMinMax()
    assert maxval == 255

    minval, _ = target_ds.GetRasterBand(3).ComputeRasterMinMax()
    assert minval == 0


###############################################################################
# Test bug fix for #5580 (used to hang)


def test_rasterize_6():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    wkb = struct.pack(
        "B" * 93,
        0,
        0,
        0,
        0,
        3,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        5,
        65,
        28,
        138,
        141,
        120,
        239,
        76,
        104,
        65,
        87,
        9,
        185,
        80,
        29,
        20,
        208,
        65,
        28,
        144,
        191,
        125,
        165,
        41,
        54,
        65,
        87,
        64,
        14,
        111,
        103,
        53,
        124,
        65,
        30,
        132,
        127,
        255,
        255,
        255,
        254,
        65,
        87,
        63,
        241,
        218,
        241,
        62,
        127,
        65,
        30,
        132,
        128,
        0,
        0,
        0,
        0,
        65,
        87,
        9,
        156,
        142,
        126,
        144,
        236,
        65,
        28,
        138,
        141,
        120,
        239,
        76,
        104,
        65,
        87,
        9,
        185,
        80,
        29,
        20,
        208,
    )

    data_source = ogr.GetDriverByName("MEMORY").CreateDataSource("")
    layer = data_source.CreateLayer("", sr, geom_type=ogr.wkbPolygon)
    feature = ogr.Feature(layer.GetLayerDefn())
    feature.SetGeometryDirectly(ogr.CreateGeometryFromWkb(wkb))
    layer.CreateFeature(feature)

    mask_ds = gdal.GetDriverByName("Mem").Create("", 5000, 5000, 1, gdal.GDT_UInt8)
    mask_ds.SetGeoTransform([499000, 0.4, 0, 6095000, 0, -0.4])
    mask_ds.SetProjection(sr_wkt)

    gdal.RasterizeLayer(mask_ds, [1], layer, burn_values=[1], options=["ALL_TOUCHED"])


###############################################################################
# Test rasterization with ALL_TOUCHED.


@pytest.mark.require_driver("CSV")
def test_rasterize_7():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'

    # Create a memory raster to rasterize into.
    target_ds = gdal.GetDriverByName("MEM").Create("", 12, 12, 1, gdal.GDT_UInt8)
    target_ds.SetGeoTransform((0, 1, 0, 12, 0, -1))
    target_ds.SetProjection(sr_wkt)

    # Create a memory layer to rasterize from.

    snapped_square_ds = ogr.Open("data/square.csv")

    # Run the algorithm.

    with gdal.quiet_errors():
        err = gdal.RasterizeLayer(
            target_ds,
            [1],
            snapped_square_ds.GetLayer(0),
            burn_values=[1],
            options=["ALL_TOUCHED=TRUE"],
        )

    assert err == 0, "got non-zero result code from RasterizeLayer"

    # Check results.

    expected = 1
    checksum = target_ds.GetRasterBand(1).Checksum()
    if checksum != expected:
        print(checksum)
        gdal.GetDriverByName("GTiff").CreateCopy("tmp/rasterize_7.tif", target_ds)
        pytest.fail("Did not get expected image checksum")


###############################################################################
# Test rasterization with ALL_TOUCHED, and geometry close to a vertical line,
# but not exactly on it


def test_rasterize_all_touched_issue_7523():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    # Create a memory raster to rasterize into.
    target_ds = gdal.GetDriverByName("MEM").Create("", 3, 5, 1, gdal.GDT_UInt8)
    target_ds.SetGeoTransform((475435, 5, 0, 424145, 0, -5))
    target_ds.SetProjection(sr_wkt)

    vect_ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    lyr = vect_ds.CreateLayer("test", sr)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt(
            "POLYGON ((475439.996613325 424122.228740036,475439.996613325 424142.201761073,475446.914301362 424124.133743847,475439.996613325 424122.228740036))"
        )
    )
    lyr.CreateFeature(f)

    # Run the algorithm.
    gdal.RasterizeLayer(
        target_ds,
        [1],
        lyr,
        burn_values=[1],
        options=["ALL_TOUCHED=TRUE"],
    )

    # Check results.
    assert struct.unpack("B" * (3 * 5), target_ds.GetRasterBand(1).ReadRaster()) == (
        1,
        1,
        0,
        1,
        1,
        0,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
        1,
        1,
    )


###############################################################################
# Test rasterizing linestring with multiple segments and MERGE_ALG=ADD
# Tests https://github.com/OSGeo/gdal/issues/1307


def test_rasterize_merge_alg_add_multiple_segment_linestring():

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    data_source = ogr.GetDriverByName("MEMORY").CreateDataSource("")
    layer = data_source.CreateLayer("", sr, geom_type=ogr.wkbLineString)
    feature = ogr.Feature(layer.GetLayerDefn())
    # Diagonal segments
    feature.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("LINESTRING(0.5 0.5,100.5 50.5,199.5 99.5)")
    )
    layer.CreateFeature(feature)
    feature = ogr.Feature(layer.GetLayerDefn())
    # Vertical and horizontal segments
    feature.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("LINESTRING(30.5 40.5,30.5 70.5,50.5 70.5)")
    )
    layer.CreateFeature(feature)

    ds = gdal.GetDriverByName("Mem").Create("", 10, 10, 1, gdal.GDT_UInt8)
    ds.SetGeoTransform([0, 20, 0, 100, 0, -10])
    ds.SetProjection(sr_wkt)

    ds.GetRasterBand(1).Fill(0)
    gdal.RasterizeLayer(ds, [1], layer, burn_values=[1], options=["MERGE_ALG=ADD"])

    got = struct.unpack("B" * 100, ds.ReadRaster())
    expected = (
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        1,
        1,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    )
    assert got == expected, "%s" % str(got)

    ds.GetRasterBand(1).Fill(0)
    gdal.RasterizeLayer(
        ds, [1], layer, burn_values=[1], options=["MERGE_ALG=ADD", "ALL_TOUCHED"]
    )

    got = struct.unpack("B" * 100, ds.ReadRaster())
    expected = (
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        1,
        0,
        1,
        1,
        0,
        0,
        0,
        1,
        1,
        1,
        0,
        0,
        1,
        0,
        0,
        0,
        1,
        1,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        1,
        1,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        1,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    )
    assert got == expected, "%s" % str(got)


###############################################################################
# Test rasterizing polygon with horizontal segments and MERGE_ALG=ADD
# to check that we don't redraw several times the top segment, depending on
# the winding order


@pytest.mark.parametrize(
    "wkt",
    ["POLYGON((0 0,0 1,1 1,1 0,0 0))", "POLYGON((0 0,1 0,1 1,0 1,0 0))"],
    ids=["clockwise", "counterclockwise"],
)
def test_rasterize_merge_alg_add_polygon(wkt):

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    data_source = ogr.GetDriverByName("MEMORY").CreateDataSource("")
    layer = data_source.CreateLayer("", sr, geom_type=ogr.wkbPolygon)
    feature = ogr.Feature(layer.GetLayerDefn())
    feature.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
    layer.CreateFeature(feature)

    ds = gdal.GetDriverByName("Mem").Create("", 5, 5, 1, gdal.GDT_UInt8)
    ds.SetGeoTransform([-0.125, 0.25, 0, 1.125, 0, -0.25])
    ds.SetProjection(sr_wkt)

    ds.GetRasterBand(1).Fill(0)
    gdal.RasterizeLayer(ds, [1], layer, burn_values=[10], options=["MERGE_ALG=ADD"])

    got = struct.unpack("B" * 25, ds.ReadRaster())
    expected = (
        0,
        10,
        10,
        10,
        10,
        0,
        10,
        10,
        10,
        10,
        0,
        10,
        10,
        10,
        10,
        0,
        10,
        10,
        10,
        10,
        0,
        10,
        10,
        10,
        10,
    )
    assert got == expected, "%s" % str(got)


###############################################################################


@pytest.mark.require_driver("GeoJSON")
def test_rasterize_bugfix_gh6981():

    bad_geometry = {
        "type": "MultiPolygon",
        "coordinates": [
            [
                [
                    [680334.22538978618104, 5128367.892639194615185],
                    [680812.384837608668022, 5128326.164302133023739],
                    [681291.53140685800463, 5128418.05398784019053],
                    [681317.43234931351617, 5125828.967085248790681],
                    [680424.775291706551798, 5125830.598540099337697],
                    [680334.22538978618104, 5128367.892639194615185],
                ]
            ]
        ],
    }

    driver = gdal.GetDriverByName("MEM")
    raster_ds = driver.Create("", 98, 259, 1, gdal.GDT_Float32)
    raster_ds.SetGeoTransform(
        (
            680334.2253897862,
            10.032724076809542,
            0.0,
            5128418.05398784,
            0.0,
            -9.996474527379922,
        )
    )

    import json

    vect_ds = gdal.OpenEx(json.dumps(bad_geometry), gdal.OF_VECTOR)

    # Just check we don't crash
    assert (
        gdal.RasterizeLayer(
            raster_ds,
            bands=[1],
            layer=vect_ds.GetLayerByIndex(0),
            burn_values=[1.0],
            options=["ALL_TOUCHED=TRUE"],
        )
        == gdal.CE_None
    )


###############################################################################


@pytest.mark.parametrize(
    "wkt",
    [
        "POLYGON((12.5 2.5, 12.5 12.5, 2.5 12.5, 2.5 2.5, 12.5 2.5))",
        "POLYGON((12.5 2.5, 2.5 2.5, 2.5 12.5, 12.5 12.5, 12.5 2.5))",
        "LINESTRING(12.5 2.5, 2.5 2.5, 2.5 12.5, 12.5 12.5, 12.5 2.5)",
    ],
    ids=["clockwise", "counterclockwise", "linestring"],
)
@pytest.mark.parametrize(
    "options",
    [
        ["MERGE_ALG=ADD", "ALL_TOUCHED=YES"],
        ["ALL_TOUCHED=YES"],
        ["MERGE_ALG=ADD"],
        [],
    ],
)
@pytest.mark.parametrize("nbands", [1, 2])
def test_rasterize_bugfix_gh8437(wkt, options, nbands):

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    # Create a memory raster to rasterize into.
    target_ds = gdal.GetDriverByName("MEM").Create("", 15, 15, nbands, gdal.GDT_UInt8)
    target_ds.SetGeoTransform((15, -1, 0, 15, 0, -1))

    target_ds.SetProjection(sr_wkt)

    # Create a memory layer to rasterize from.
    rast_ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("wrk")
    rast_mem_lyr = rast_ogr_ds.CreateLayer("poly", srs=sr)

    # Add a polygon.
    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt))

    rast_mem_lyr.CreateFeature(feat)

    # Run the algorithm.
    gdal.RasterizeLayer(
        target_ds,
        [i + 1 for i in range(nbands)],
        rast_mem_lyr,
        burn_values=[80] * nbands,
        options=options,
    )

    expected_checksum = (
        519
        if wkt.startswith("LINESTRING")
        else 1727 if "ALL_TOUCHED=YES" in options else 1435
    )
    for i in range(nbands):
        _, maxval = target_ds.GetRasterBand(i + 1).ComputeRasterMinMax()
        assert maxval == 80
        assert target_ds.GetRasterBand(i + 1).Checksum() == expected_checksum


###############################################################################


@pytest.mark.parametrize(
    "wkt",
    [
        "POLYGON((7.5 2.5, 7.5 8.0, 2.5 8.0, 2.5 2.5, 7.5 2.5))",
        "POLYGON((8.0 2.5, 8.0 7.5, 2.5 7.5, 2.5 2.5, 8.0 2.5))",
        "POLYGON((7.5 2.0, 7.5 7.5, 2.5 7.5, 2.5 2.0, 7.5 2.0))",
        "POLYGON((7.5 2.5, 7.5 7.5, 2.0 7.5, 2.0 2.5, 7.5 2.5))",
    ],
)
def test_rasterize_bugfix_gh8918(wkt):

    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    # Create a memory raster to rasterize into.
    target_ds = gdal.GetDriverByName("MEM").Create("", 10, 10, 1, gdal.GDT_UInt8)
    target_ds.SetGeoTransform((0, 1, 0, 0, 0, 1))

    target_ds.SetProjection(sr_wkt)

    # Create a memory layer to rasterize from.
    rast_ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("wrk")
    rast_mem_lyr = rast_ogr_ds.CreateLayer("poly", srs=sr)

    # Add a polygon.
    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.Geometry(wkt=wkt))

    rast_mem_lyr.CreateFeature(feat)

    # Run the algorithm.
    gdal.RasterizeLayer(
        target_ds,
        [1],
        rast_mem_lyr,
        burn_values=[1],
        options=["ALL_TOUCHED=YES"],
    )

    assert target_ds.GetRasterBand(1).Checksum() == 36


###############################################################################


def test_rasterize_bugfix_gh12129():
    # Setup working spatial reference
    sr_wkt = 'LOCAL_CS["arbitrary"]'
    sr = osr.SpatialReference(sr_wkt)

    # Create a memory raster to rasterize into.
    target_ds = gdal.GetDriverByName("MEM").Create("", 20, 16, 1, gdal.GDT_UInt8)
    target_ds.SetGeoTransform((163600, 20, 0, 168000, 0, -20))

    target_ds.SetProjection(sr_wkt)

    # Create a memory layer to rasterize from.
    rast_ogr_ds = ogr.GetDriverByName("MEM").CreateDataSource("wrk")
    rast_mem_lyr = rast_ogr_ds.CreateLayer("poly", srs=sr)

    # Add a polygon.
    feat = ogr.Feature(rast_mem_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(
        ogr.Geometry(
            wkt="POLYGON ((163899.27734375 167988.154296875,163909.339662057 167743.621912878,163628.300781257 167745.908203129,163661.337890632 167785.683593765,163700 167880,163780.000000007 167860.000000011,163835.751953125 167942.617187511,163899.27734375 167988.154296875))"
        )
    )

    rast_mem_lyr.CreateFeature(feat)

    # Run the algorithm.
    gdal.RasterizeLayer(
        target_ds,
        [1],
        rast_mem_lyr,
        burn_values=[1],
        options=["ALL_TOUCHED=YES"],
    )

    # 121 on s390x
    assert target_ds.GetRasterBand(1).Checksum() in (120, 121)


###############################################################################


def test_rasterize_huge_geometry():

    # Create a memory raster to rasterize into.
    target_ds = gdal.GetDriverByName("MEM").Create("", 20, 20, 1, gdal.GDT_UInt8)
    target_ds.SetGeoTransform((0, 1, 0, 0, 0, 1))

    v = 1e300
    rast_ogr_ds = gdaltest.wkt_ds(
        [f"POLYGON ((-{v} -{v},-{v} {v},{v} {v},{v} -{v},-{v} -{v}))"]
    )

    # Run the algorithm.
    gdal.RasterizeLayer(
        target_ds,
        [1],
        rast_ogr_ds.GetLayer(0),
        burn_values=[1],
        options=["ALL_TOUCHED=YES"],
    )

    assert target_ds.GetRasterBand(1).Checksum() == 400
