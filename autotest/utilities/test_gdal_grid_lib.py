#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_grid testing
# Author:   Even Rouault <even dot rouault @ spatialys dot com>
#
###############################################################################
# Copyright (c) 2008-2015, Even Rouault <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import collections
import struct

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr


@pytest.fixture()
def n43_tif():
    ds = gdal.Open("../gdrivers/data/n43.tif")
    return ds


@pytest.fixture()
def n43_shp(tmp_vsimem, n43_tif):

    # Create an OGR grid from the values of n43.tif
    geotransform = n43_tif.GetGeoTransform()

    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    shape_ds = shape_drv.CreateDataSource(tmp_vsimem / "tmp")
    shape_lyr = shape_ds.CreateLayer("n43")

    data = n43_tif.ReadRaster(0, 0, 121, 121)
    array_val = struct.unpack("h" * 121 * 121, data)
    for j in range(121):
        for i in range(121):
            wkt = "POINT(%f %f %s)" % (
                geotransform[0] + (i + 0.5) * geotransform[1],
                geotransform[3] + (j + 0.5) * geotransform[5],
                array_val[j * 121 + i],
            )
            dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
            dst_feat.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
            shape_lyr.CreateFeature(dst_feat)

    shape_ds.ExecuteSQL("CREATE SPATIAL INDEX ON n43")

    return gdaltest.reopen(shape_ds, update=1)


###############################################################################
#


def test_gdal_grid_lib_1(n43_tif, n43_shp):

    ds = n43_tif

    spatFilter = None
    if ogrtest.have_geos():
        spatFilter = [-180, -90, 180, 90]

    # Create a GDAL dataset from the previous generated OGR grid
    ds2 = gdal.Grid(
        "",
        n43_shp.GetDescription(),
        format="MEM",
        outputBounds=[-80.0041667, 42.9958333, -78.9958333, 44.0041667],
        width=121,
        height=121,
        outputType=gdal.GDT_Int16,
        algorithm="nearest:radius1=0.0:radius2=0.0:angle=0.0",
        spatFilter=spatFilter,
        layers=[n43_shp.GetLayer(0).GetName()],
        where="1=1",
    )

    # We should get the same values as in n43.td0
    assert (
        ds.GetRasterBand(1).Checksum() == ds2.GetRasterBand(1).Checksum()
    ), "bad checksum : got %d, expected %d" % (
        ds.GetRasterBand(1).Checksum(),
        ds2.GetRasterBand(1).Checksum(),
    )
    assert ds2.GetRasterBand(1).GetNoDataValue() is None, "did not expect nodata value"

    ds = None
    ds2 = None


###############################################################################
# Test with a point number not multiple of 8 or 16


@pytest.mark.parametrize(
    "env",
    [
        {"GDAL_USE_AVX": "NO", "GDAL_USE_SSE": "NO"},
        {"GDAL_USE_AVX": "NO"},
        {},
    ],
)
def test_gdal_grid_lib_2(tmp_vsimem, env):

    shp_filename = tmp_vsimem / "test_gdal_grid_lib_2.shp"

    with ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(
        shp_filename
    ) as shape_ds:
        shape_lyr = shape_ds.CreateLayer("test_gdal_grid_lib_2")
        dst_feat = ogr.Feature(feature_def=shape_lyr.GetLayerDefn())
        dst_feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 0 100)"))
        shape_lyr.CreateFeature(dst_feat)

    with gdal.config_options(env):

        # Point strictly on grid
        ds1 = gdal.Grid(
            "",
            shp_filename,
            format="MEM",
            outputBounds=[-0.5, -0.5, 0.5, 0.5],
            width=1,
            height=1,
            outputType=gdal.GDT_UInt8,
            SQLStatement="select * from test_gdal_grid_lib_2",
        )

        ds2 = gdal.Grid(
            "",
            shp_filename,
            format="MEM",
            outputBounds=[-0.4, -0.4, 0.6, 0.6],
            width=10,
            height=10,
            outputType=gdal.GDT_UInt8,
        )

    cs = ds1.GetRasterBand(1).Checksum()
    assert cs == 2

    cs = ds2.GetRasterBand(1).Checksum()
    assert cs == 1064


###############################################################################
# Test bugfix for #7101 (segmentation fault with linear interpolation)


# May fail on minimum builds without qhull
@gdaltest.disable_exceptions()
@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_3():

    wkt = "POLYGON ((37.3495241627097 55.6901648563184 187.680953979492,37.349543273449 55.6901565410051 187.714370727539,37.3495794832707 55.6901531392856 187.67333984375,37.3496210575104 55.6901595647556 187.6396484375,37.3496398329735 55.6901716597552 187.596603393555,37.3496726900339 55.6901780852222 187.681350708008,37.3496793955565 55.6901829988139 187.933898925781,37.3496921360493 55.6901860225623 187.934280395508,37.3497162759304 55.6902037870796 187.435394287109,37.3497484624386 55.6902094566047 187.515319824219,37.3497618734837 55.6902241973661 190.329940795898,37.3497511446476 55.690238560154 190.345748901367,37.3497404158115 55.6902567026153 190.439697265625,37.3497142642736 55.6902650179072 189.086044311523,37.349688783288 55.6902608602615 187.763305664062,37.3496626317501 55.6902468754498 187.53678894043,37.3496378213167 55.6902412059301 187.598648071289,37.3496103286743 55.6902400720261 187.806274414062,37.3495902121067 55.6902313787607 187.759521484375,37.3495734483004 55.6902177719067 187.578125,37.349532879889 55.6902035980954 187.56965637207,37.3495161160827 55.6901939599008 187.541793823242,37.3495187982917 55.6901754394418 187.610427856445,37.3495241627097 55.6901648563184 187.680953979492))"
    polygon = ogr.CreateGeometryFromWkt(wkt)

    gdal.Grid(
        "",
        polygon.ExportToJson(),
        width=115,
        height=93,
        outputBounds=[
            37.3495161160827,
            55.6901531392856,
            37.3497618734837,
            55.6902650179072,
        ],
        format="MEM",
        algorithm="linear",
    )


###############################################################################
# Test invdistnn per quadrant


def _compare_arrays(ds, expected_array):

    got_data = ds.ReadRaster(buf_type=gdal.GDT_Float32)
    got_array = []
    for j in range(ds.RasterYSize):
        ar = array.array("f")
        sizeof_float = 4
        ar.frombytes(
            got_data[
                j
                * ds.RasterXSize
                * sizeof_float : (j + 1)
                * ds.RasterXSize
                * sizeof_float
            ]
        )
        got_array.append(ar.tolist())
    assert got_array == expected_array


def _shift_by(geom, dx, dy):
    for i in range(geom.GetGeometryCount()):
        subgeom = geom.GetGeometryRef(i)
        subgeom.SetPoint(0, subgeom.GetX(0) + dx, subgeom.GetY(0) + dy, subgeom.GetZ(0))


@pytest.mark.parametrize("alg", ["invdist", "invdistnn"])
@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_invdistnn_quadrant_all_params(alg):

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10,1 0 100000000)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm=alg
        + ":power=1.5:smoothing=0.000000000000001:radius=2:max_points=10:min_points=4:min_points_per_quadrant=1:max_points_per_quadrant=2",
    )
    power = 1.5
    dist1 = (0.5**2 + 0.5**2) ** (power / 2.0)
    dist2 = (1**2 + 0**2) ** (power / 2.0)
    expected_val = (4 * 10 / dist1 + 100000000 / dist2) / (4 / dist1 + 1 / dist2)
    expected_val = struct.unpack("f", struct.pack("f", expected_val))[0]
    _compare_arrays(ds, [[expected_val]])


@pytest.mark.parametrize("alg", ["invdist", "invdistnn"])
@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_invdistnn_quadrant_insufficient_radius(alg):

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm=alg + ":radius=0.7:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])  # insufficient radius. should be > sqrt(2)


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_invdistnn_quadrant_min_points_not_reached():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="invdistnn:radius=1:min_points_per_quadrant=1:min_points=5",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_invdistnn_quadrant_missing_point_in_one_quadrant():

    # Missing point in 0.5 -0.5 quadrant
    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="invdistnn:radius=0.8:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_invdistnn_quadrant_ignore_extra_points():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10,1 0 100000000)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="invdistnn:radius=2:min_points_per_quadrant=1:max_points=0:max_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[10.0]])

    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="invdistnn:radius=2:min_points_per_quadrant=1:max_points=4",
    )
    _compare_arrays(ds, [[10.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_average_quadrant_all_params():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10,1 0 100)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="average:radius=2:max_points=10:min_points=4:min_points_per_quadrant=1:max_points_per_quadrant=2",
    )
    expected_val = (4 * 10 + 100.0) / 5
    expected_val = struct.unpack("f", struct.pack("f", expected_val))[0]
    _compare_arrays(ds, [[expected_val]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_average_quadrant_insufficient_radius():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="average:radius=0.7:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])  # insufficient radius. should be > sqrt(2)


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_average_quadrant_min_points_not_reached():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="average:radius=1:min_points_per_quadrant=1:min_points=5",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_average_quadrant_missing_point_in_one_quadrant():

    # Missing point in 0.5 -0.5 quadrant
    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="average:radius=0.8:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_average_quadrant_ignore_extra_points():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10,1 0 100000000)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="average:radius=2:min_points_per_quadrant=1:max_points=0:max_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[10.0]])

    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="average:radius=2:min_points_per_quadrant=1:max_points=4",
    )
    _compare_arrays(ds, [[10.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_minimum_quadrant_all_params():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 9,1 0 5)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="minimum:radius=2:min_points=4:min_points_per_quadrant=1:max_points_per_quadrant=2",
    )
    _compare_arrays(ds, [[5.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_minimum_quadrant_insufficient_radius():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="minimum:radius=0.7:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])  # insufficient radius. should be > sqrt(2)


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_minimum_quadrant_min_points_not_reached():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="minimum:radius=1:min_points_per_quadrant=1:min_points=5",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_minimum_quadrant_missing_point_in_one_quadrant():

    # Missing point in 0.5 -0.5 quadrant
    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="minimum:radius=0.8:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_minimum_quadrant_ignore_extra_points():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10,1 0 1)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="minimum:radius=2:min_points_per_quadrant=1:max_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[10.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_maximum_quadrant_all_params():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 11,1 0 50)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="maximum:radius=2:min_points=4:min_points_per_quadrant=1:max_points_per_quadrant=2",
    )
    _compare_arrays(ds, [[50.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_maximum_quadrant_insufficient_radius():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="maximum:radius=0.7:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])  # insufficient radius. should be > sqrt(2)


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_maximum_quadrant_min_points_not_reached():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="maximum:radius=1:min_points_per_quadrant=1:min_points=5",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_maximum_quadrant_missing_point_in_one_quadrant():

    # Missing point in 0.5 -0.5 quadrant
    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="maximum:radius=0.8:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_maximum_quadrant_ignore_extra_points():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 10,1 0 100)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="maximum:radius=2:min_points_per_quadrant=1:max_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[10.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_range_quadrant_all_params():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 1,1 0 50)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="range:radius=2:min_points=4:min_points_per_quadrant=1:max_points_per_quadrant=2",
    )
    _compare_arrays(ds, [[50.0 - 1.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_range_quadrant_insufficient_radius():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 0)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="range:radius=0.7:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])  # insufficient radius. should be > sqrt(2)


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_range_quadrant_min_points_not_reached():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 0)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="range:radius=1:min_points_per_quadrant=1:min_points=5",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_range_quadrant_missing_point_in_one_quadrant():

    # Missing point in 0.5 -0.5 quadrant
    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 0)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="range:radius=0.8:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_range_quadrant_ignore_extra_points():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 1,1 0 100)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="range:radius=2:min_points_per_quadrant=1:max_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[9.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_count_quadrant_all_params():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 1,1 0 50)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="count:radius=2:min_points=4:min_points_per_quadrant=1:max_points_per_quadrant=2",
    )
    _compare_arrays(ds, [[5]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_count_quadrant_insufficient_radius():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 0)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="count:radius=0.7:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])  # insufficient radius. should be > sqrt(2)


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_count_quadrant_min_points_not_reached():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 0)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="count:radius=1:min_points_per_quadrant=1:min_points=5",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_count_quadrant_missing_point_in_one_quadrant():

    # Missing point in 0.5 -0.5 quadrant
    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 0)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="count:radius=0.8:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_count_quadrant_ignore_extra_points():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 1,1 0 100)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="count:radius=2:min_points_per_quadrant=1:max_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[4.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_average_distance_quadrant_all_params():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 1,1 0 50)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="average_distance:radius=2:min_points=4:min_points_per_quadrant=1:max_points_per_quadrant=2",
    )
    expected_val = (4 * (2 * 0.5**2) ** 0.5 + 1) / 5
    expected_val = struct.unpack("f", struct.pack("f", expected_val))[0]
    _compare_arrays(ds, [[expected_val]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_average_distance_quadrant_insufficient_radius():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 0)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="average_distance:radius=0.7:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])  # insufficient radius. should be > sqrt(2)


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_average_distance_quadrant_min_points_not_reached():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 0)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="average_distance:radius=1:min_points_per_quadrant=1:min_points=5",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_average_distance_quadrant_missing_point_in_one_quadrant():

    # Missing point in 0.5 -0.5 quadrant
    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 0)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="average_distance:radius=0.8:min_points_per_quadrant=1",
    )
    _compare_arrays(ds, [[0.0]])


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_average_distance_quadrant_ignore_extra_points():

    wkt = "MULTIPOINT(0.5 0.5 10,-0.5 0.5 10,-0.5 -0.5 10,0.5 -0.5 1,1 0 100)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    SHIFT_X = 10
    SHIFT_Y = 100
    _shift_by(geom, SHIFT_X, SHIFT_Y)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5 + SHIFT_X, -0.5 + SHIFT_Y, 0.5 + SHIFT_X, 0.5 + SHIFT_Y],
        format="MEM",
        algorithm="average_distance:radius=2:min_points_per_quadrant=1:max_points_per_quadrant=1",
    )
    expected_val = (4 * (2 * 0.5**2) ** 0.5) / 4
    expected_val = struct.unpack("f", struct.pack("f", expected_val))[0]
    _compare_arrays(ds, [[expected_val]])


###############################################################################
# Test that features whose zfield value is unset are ignored


def test_gdal_grid_lib_skip_null_zfield():

    mem_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    mem_lyr = mem_ds.CreateLayer("test")
    mem_lyr.CreateField(ogr.FieldDefn("val", ogr.OFTInteger))
    f = ogr.Feature(mem_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    f["val"] = 10
    mem_lyr.CreateFeature(f)
    f = ogr.Feature(mem_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 0)"))
    f["val"] = 10
    mem_lyr.CreateFeature(f)
    f = ogr.Feature(mem_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1)"))
    f["val"] = 10
    mem_lyr.CreateFeature(f)
    f = ogr.Feature(mem_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 1)"))
    f["val"] = 10
    mem_lyr.CreateFeature(f)
    f = ogr.Feature(mem_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0.5 0.5)"))
    # no value!
    mem_lyr.CreateFeature(f)
    ds = gdal.Grid(
        "",
        mem_ds,
        width=3,
        height=3,
        outputBounds=[-0.25, -0.25, 1.25, 1.25],
        format="MEM",
        algorithm="invdist",
        zfield="val",
    )
    assert struct.unpack("d" * 9, ds.ReadRaster()) == pytest.approx(
        (
            10,
            10,
            10,
            10,
            10,
            10,
            10,
            10,
            10,
        )
    )


###############################################################################
# Test that features whose zfield value is unset are ignored


def test_gdal_grid_lib_skip_nan_zvalue():

    mem_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    mem_lyr = mem_ds.CreateLayer("test")
    f = ogr.Feature(mem_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 0 10)"))
    mem_lyr.CreateFeature(f)
    f = ogr.Feature(mem_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 0 10)"))
    mem_lyr.CreateFeature(f)
    f = ogr.Feature(mem_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 1 10)"))
    mem_lyr.CreateFeature(f)
    f = ogr.Feature(mem_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 1 10)"))
    mem_lyr.CreateFeature(f)
    f = ogr.Feature(mem_lyr.GetLayerDefn())
    p = ogr.Geometry(ogr.wkbPoint25D)
    p.SetPoint(0, 0.5, 0.5, float("nan"))
    f.SetGeometry(p)
    mem_lyr.CreateFeature(f)
    ds = gdal.Grid(
        "",
        mem_ds,
        width=3,
        height=3,
        outputBounds=[-0.25, -0.25, 1.25, 1.25],
        format="MEM",
        algorithm="invdist",
    )
    assert struct.unpack("d" * 9, ds.ReadRaster()) == pytest.approx(
        (
            10,
            10,
            10,
            10,
            10,
            10,
            10,
            10,
            10,
        )
    )


###############################################################################
# Test option argument handling


def test_gdal_grid_lib_dict_arguments():

    opt = gdal.GridOptions(
        "__RETURN_OPTION_LIST__",
        creationOptions=collections.OrderedDict(
            (("COMPRESS", "DEFLATE"), ("LEVEL", 4))
        ),
    )

    ind = opt.index("-co")

    assert opt[ind : ind + 4] == ["-co", "COMPRESS=DEFLATE", "-co", "LEVEL=4"]


###############################################################################
# Test various error conditions


def test_gdal_grid_lib_errors():

    mem_ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0, gdal.GDT_Unknown)
    mem_ds.CreateLayer("test")

    with pytest.raises(Exception, match='Unable to find layer "invalid"'):
        gdal.Grid(
            "",
            mem_ds,
            width=3,
            height=3,
            outputBounds=[-0.25, -0.25, 1.25, 1.25],
            format="MEM",
            algorithm="invdist",
            layers=["invalid"],
        )

    with pytest.raises(
        Exception, match='"invalid" not recognised as an available field'
    ):
        gdal.Grid(
            "",
            mem_ds,
            width=3,
            height=3,
            outputBounds=[-0.25, -0.25, 1.25, 1.25],
            format="MEM",
            algorithm="invdist",
            where="invalid",
        )

    with pytest.raises(Exception, match="SQL Expression Parsing Error"):
        gdal.Grid(
            "",
            mem_ds,
            width=3,
            height=3,
            outputBounds=[-0.25, -0.25, 1.25, 1.25],
            format="MEM",
            algorithm="invdist",
            SQLStatement="invalid",
        )


###############################################################################
# Test that ellipse rotation (angle parameter) is correctly applied
# in the quadtree-accelerated path. Before the fix, the quadtree path
# silently ignored the angle, producing wrong results.


def _make_rotated_ellipse_ds(algorithm, angle):
    """Grid a single pixel at the origin using points placed so that
    an ellipse with radius1=3 (X-axis), radius2=1 (Y-axis) includes
    different points depending on its rotation angle.

    Point layout (relative to origin):
      A = (2, 0, 10)  -> inside unrotated ellipse, outside 90-deg rotated
      B = (0, 2, 100) -> outside unrotated ellipse, inside 90-deg rotated
      C = (0, 0, 1)   -> always inside (at center)
    """
    wkt = "MULTIPOINT(2 0 10, 0 2 100, 0 0 1)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    with gdal.config_option("GDAL_GRID_POINT_COUNT_THRESHOLD", "0"):
        ds = gdal.Grid(
            "",
            geom.ExportToJson(),
            width=1,
            height=1,
            outputBounds=[-0.5, -0.5, 0.5, 0.5],
            format="MEM",
            algorithm=algorithm,
        )
    return ds


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_moving_average_rotated_ellipse():
    """Moving average with a rotated ellipse should include different
    points than with angle=0. This was broken before the quadtree
    rotation fix."""

    # angle=0: ellipse wide along X -> includes A(10) and C(1), not B(100)
    ds0 = _make_rotated_ellipse_ds("average:radius1=3:radius2=1:angle=0", 0)
    val0 = struct.unpack("f", ds0.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    expected0 = (10 + 1) / 2.0
    expected0 = struct.unpack("f", struct.pack("f", expected0))[0]
    assert val0 == pytest.approx(
        expected0, rel=1e-5
    ), f"angle=0: expected {expected0}, got {val0}"

    # angle=90: ellipse wide along Y -> includes B(100) and C(1), not A(10)
    ds90 = _make_rotated_ellipse_ds("average:radius1=3:radius2=1:angle=90", 90)
    val90 = struct.unpack("f", ds90.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    expected90 = (100 + 1) / 2.0
    expected90 = struct.unpack("f", struct.pack("f", expected90))[0]
    assert val90 == pytest.approx(
        expected90, rel=1e-5
    ), f"angle=90: expected {expected90}, got {val90}"

    assert val0 != pytest.approx(
        val90, rel=1e-5
    ), "Rotation should produce different results"


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_data_metric_minimum_rotated_ellipse():
    """Minimum with rotated ellipse."""

    ds0 = _make_rotated_ellipse_ds("minimum:radius1=3:radius2=1:angle=0", 0)
    val0 = struct.unpack("f", ds0.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    assert val0 == pytest.approx(1.0, rel=1e-5)  # min(10, 1) = 1

    ds90 = _make_rotated_ellipse_ds("minimum:radius1=3:radius2=1:angle=90", 90)
    val90 = struct.unpack("f", ds90.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    assert val90 == pytest.approx(1.0, rel=1e-5)  # min(100, 1) = 1


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_data_metric_maximum_rotated_ellipse():
    """Maximum with rotated ellipse should pick different max values."""

    ds0 = _make_rotated_ellipse_ds("maximum:radius1=3:radius2=1:angle=0", 0)
    val0 = struct.unpack("f", ds0.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    assert val0 == pytest.approx(10.0, rel=1e-5)  # max(10, 1) = 10

    ds90 = _make_rotated_ellipse_ds("maximum:radius1=3:radius2=1:angle=90", 90)
    val90 = struct.unpack("f", ds90.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    assert val90 == pytest.approx(100.0, rel=1e-5)  # max(100, 1) = 100


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_data_metric_range_rotated_ellipse():
    """Range with rotated ellipse."""

    ds0 = _make_rotated_ellipse_ds("range:radius1=3:radius2=1:angle=0", 0)
    val0 = struct.unpack("f", ds0.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    assert val0 == pytest.approx(9.0, rel=1e-5)  # 10 - 1

    ds90 = _make_rotated_ellipse_ds("range:radius1=3:radius2=1:angle=90", 90)
    val90 = struct.unpack("f", ds90.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    assert val90 == pytest.approx(99.0, rel=1e-5)  # 100 - 1


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_data_metric_count_rotated_ellipse():
    """Count with rotated ellipse should find 2 points regardless
    of rotation (different 2 points, but still 2)."""

    ds0 = _make_rotated_ellipse_ds("count:radius1=3:radius2=1:angle=0", 0)
    val0 = struct.unpack("f", ds0.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    assert val0 == pytest.approx(2.0, rel=1e-5)

    ds90 = _make_rotated_ellipse_ds("count:radius1=3:radius2=1:angle=90", 90)
    val90 = struct.unpack("f", ds90.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    assert val90 == pytest.approx(2.0, rel=1e-5)


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_data_metric_average_distance_rotated_ellipse():
    """Average distance with rotated ellipse should differ because
    different points are included."""

    ds0 = _make_rotated_ellipse_ds("average_distance:radius1=3:radius2=1:angle=0", 0)
    val0 = struct.unpack("f", ds0.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # Points A(2,0) and C(0,0): distances are 2 and 0
    expected0 = (2.0 + 0.0) / 2.0
    expected0 = struct.unpack("f", struct.pack("f", expected0))[0]
    assert val0 == pytest.approx(expected0, rel=1e-5)

    ds90 = _make_rotated_ellipse_ds("average_distance:radius1=3:radius2=1:angle=90", 90)
    val90 = struct.unpack("f", ds90.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # Points B(0,2) and C(0,0): distances are 2 and 0
    expected90 = (2.0 + 0.0) / 2.0
    expected90 = struct.unpack("f", struct.pack("f", expected90))[0]
    assert val90 == pytest.approx(expected90, rel=1e-5)


###############################################################################
# Test ellipse rotation in the PerQuadrant (quadtree) code path.
# max_points_per_quadrant forces the PerQuadrant variant which always
# creates a quadtree. Before the fix, these functions ignored the angle
# parameter, causing wrong points to be included in the search ellipse.
#
# Point layout (relative to grid point at origin):
#   A = (2,   0, 10)  -> inside unrotated ellipse (r1=3,r2=1), outside 90° rotated
#   B = (0, 1.5, 100) -> outside unrotated, inside 90° rotated
#   D = (0, 2.5, 300) -> outside unrotated, inside 90° rotated
#   C = (0,   0, 50)  -> always inside (at center)
#
# angle=0:  includes {A, C}       (2 points)
# angle=90: includes {B, D, C}    (3 points)


def _make_rotated_ellipse_per_quadrant_ds(algorithm):
    """Grid a single pixel at the origin via the PerQuadrant code path."""
    wkt = "MULTIPOINT(2 0 10, 0 1.5 100, 0 2.5 300, 0 0 50)"
    geom = ogr.CreateGeometryFromWkt(wkt)
    ds = gdal.Grid(
        "",
        geom.ExportToJson(),
        width=1,
        height=1,
        outputBounds=[-0.5, -0.5, 0.5, 0.5],
        format="MEM",
        algorithm=algorithm,
    )
    return ds


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_moving_average_rotated_per_quadrant():
    """Moving average with rotated ellipse via PerQuadrant (quadtree) path."""

    ds0 = _make_rotated_ellipse_per_quadrant_ds(
        "average:radius1=3:radius2=1:angle=0:max_points_per_quadrant=10"
    )
    val0 = struct.unpack("f", ds0.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # angle=0: includes A(10), C(50) -> avg = 30.0
    assert val0 == pytest.approx(30.0, rel=1e-5)

    ds90 = _make_rotated_ellipse_per_quadrant_ds(
        "average:radius1=3:radius2=1:angle=90:max_points_per_quadrant=10"
    )
    val90 = struct.unpack("f", ds90.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # angle=90: includes B(100), D(300), C(50) -> avg = 150.0
    assert val90 == pytest.approx(150.0, rel=1e-5)


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_minimum_rotated_per_quadrant():
    """Minimum with rotated ellipse via PerQuadrant (quadtree) path."""

    ds0 = _make_rotated_ellipse_per_quadrant_ds(
        "minimum:radius1=3:radius2=1:angle=0:max_points_per_quadrant=10"
    )
    val0 = struct.unpack("f", ds0.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # angle=0: min(10, 50) = 10
    assert val0 == pytest.approx(10.0, rel=1e-5)

    ds90 = _make_rotated_ellipse_per_quadrant_ds(
        "minimum:radius1=3:radius2=1:angle=90:max_points_per_quadrant=10"
    )
    val90 = struct.unpack("f", ds90.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # angle=90: min(100, 300, 50) = 50
    assert val90 == pytest.approx(50.0, rel=1e-5)


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_maximum_rotated_per_quadrant():
    """Maximum with rotated ellipse via PerQuadrant (quadtree) path."""

    ds0 = _make_rotated_ellipse_per_quadrant_ds(
        "maximum:radius1=3:radius2=1:angle=0:max_points_per_quadrant=10"
    )
    val0 = struct.unpack("f", ds0.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # angle=0: max(10, 50) = 50
    assert val0 == pytest.approx(50.0, rel=1e-5)

    ds90 = _make_rotated_ellipse_per_quadrant_ds(
        "maximum:radius1=3:radius2=1:angle=90:max_points_per_quadrant=10"
    )
    val90 = struct.unpack("f", ds90.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # angle=90: max(100, 300, 50) = 300
    assert val90 == pytest.approx(300.0, rel=1e-5)


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_range_rotated_per_quadrant():
    """Range with rotated ellipse via PerQuadrant (quadtree) path."""

    ds0 = _make_rotated_ellipse_per_quadrant_ds(
        "range:radius1=3:radius2=1:angle=0:max_points_per_quadrant=10"
    )
    val0 = struct.unpack("f", ds0.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # angle=0: 50 - 10 = 40
    assert val0 == pytest.approx(40.0, rel=1e-5)

    ds90 = _make_rotated_ellipse_per_quadrant_ds(
        "range:radius1=3:radius2=1:angle=90:max_points_per_quadrant=10"
    )
    val90 = struct.unpack("f", ds90.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # angle=90: 300 - 50 = 250
    assert val90 == pytest.approx(250.0, rel=1e-5)


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_count_rotated_per_quadrant():
    """Count with rotated ellipse via PerQuadrant (quadtree) path."""

    ds0 = _make_rotated_ellipse_per_quadrant_ds(
        "count:radius1=3:radius2=1:angle=0:max_points_per_quadrant=10"
    )
    val0 = struct.unpack("f", ds0.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # angle=0: A, C -> 2
    assert val0 == pytest.approx(2.0, rel=1e-5)

    ds90 = _make_rotated_ellipse_per_quadrant_ds(
        "count:radius1=3:radius2=1:angle=90:max_points_per_quadrant=10"
    )
    val90 = struct.unpack("f", ds90.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # angle=90: B, D, C -> 3
    assert val90 == pytest.approx(3.0, rel=1e-5)


@pytest.mark.require_driver("GeoJSON")
def test_gdal_grid_lib_average_distance_rotated_per_quadrant():
    """Average distance with rotated ellipse via PerQuadrant (quadtree) path."""

    ds0 = _make_rotated_ellipse_per_quadrant_ds(
        "average_distance:radius1=3:radius2=1:angle=0:max_points_per_quadrant=10"
    )
    val0 = struct.unpack("f", ds0.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # angle=0: A(dist=2), C(dist=0) -> avg = 1.0
    assert val0 == pytest.approx(1.0, rel=1e-5)

    ds90 = _make_rotated_ellipse_per_quadrant_ds(
        "average_distance:radius1=3:radius2=1:angle=90:max_points_per_quadrant=10"
    )
    val90 = struct.unpack("f", ds90.ReadRaster(buf_type=gdal.GDT_Float32))[0]
    # angle=90: B(dist=1.5), D(dist=2.5), C(dist=0) -> avg = 4/3
    assert val90 == pytest.approx(4.0 / 3.0, rel=1e-5)
