#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_grid testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import struct
import sys

import pytest

sys.path.append("../gcore")

import gdaltest
import test_cli_utilities

from osgeo import gdal, ogr

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdal_grid_path() is None, reason="gdal_grid not available"
)


@pytest.fixture()
def gdal_grid_path():
    return test_cli_utilities.get_gdal_grid_path()


@pytest.fixture()
def n43_shp(tmp_path):
    n43_shp_fname = str(tmp_path / "n43.shp")

    # Create an OGR grid from the values of n43.tif
    ds = gdal.Open("../gdrivers/data/n43.tif")
    geotransform = ds.GetGeoTransform()

    shape_drv = ogr.GetDriverByName("ESRI Shapefile")
    with shape_drv.CreateDataSource(str(tmp_path)) as shape_ds:
        shape_lyr = shape_ds.CreateLayer("n43")

        data = ds.ReadRaster(0, 0, 121, 121)
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

    yield n43_shp_fname


###############################################################################
#


def test_gdal_grid_1(gdal_grid_path, n43_shp, tmp_path):

    output_tif = str(tmp_path / "n43.tif")

    # Create a GDAL dataset from the previous generated OGR grid
    _, err = gdaltest.runexternal_out_and_err(
        gdal_grid_path
        + f" -txe -80.0041667 -78.9958333 -tye 42.9958333 44.0041667 -outsize 121 121 -ot Int16 -a nearest:radius1=0.0:radius2=0.0:angle=0.0 -co TILED=YES -co BLOCKXSIZE=256 -co BLOCKYSIZE=256 {n43_shp} {output_tif}"
    )
    assert err is None or err == "", "got error/warning"

    # We should get the same values as in n43.td0
    ds = gdal.Open("../gdrivers/data/n43.tif")
    ds2 = gdal.Open(output_tif)
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
# Test Nearest Neighbour gridding algorithm


@pytest.mark.require_driver("CSV")
def test_gdal_grid_2(gdal_grid_path, tmp_path):

    # Open reference dataset
    ds_ref = gdal.Open("../gcore/data/byte.tif")
    checksum_ref = ds_ref.GetRasterBand(1).Checksum()
    ds_ref = None

    grid_near_tif = str(tmp_path / "grid_near.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    # Grid nodes are located exactly in raster nodes.
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=0.0:radius2=0.0:angle=0.0:nodata=0.0 data/grid.vrt {grid_near_tif}"
    )

    # We should get the same values as in "gcore/data/byte.tif"
    ds = gdal.Open(grid_near_tif)
    if ds.GetRasterBand(1).Checksum() != checksum_ref:
        print(
            "bad checksum : got %d, expected %d"
            % (ds.GetRasterBand(1).Checksum(), checksum_ref)
        )
        pytest.fail("bad checksum")
    assert ds.GetRasterBand(1).GetNoDataValue() == 0.0, "expected a nodata value"
    ds = None


@pytest.mark.require_driver("CSV")
def test_gdal_grid_2bis(gdal_grid_path, tmp_path):

    # Open reference dataset
    ds_ref = gdal.Open("../gcore/data/byte.tif")
    checksum_ref = ds_ref.GetRasterBand(1).Checksum()
    ds_ref = None

    grid_near_shift_tif = str(tmp_path / "grid_near_shift.tif")

    # Now the same, but shift grid nodes a bit in both horizontal and vertical
    # directions.
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=0.0:radius2=0.0:angle=0.0:nodata=0.0 data/grid.vrt {grid_near_shift_tif}"
    )

    # We should get the same values as in "gcore/data/byte.tif"
    ds = gdal.Open(grid_near_shift_tif)
    if ds.GetRasterBand(1).Checksum() != checksum_ref:
        print(
            "bad checksum : got %d, expected %d"
            % (ds.GetRasterBand(1).Checksum(), checksum_ref)
        )
        pytest.fail("bad checksum")
    ds = None


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_3(gdal_grid_path, tmp_path, use_quadtree):

    # Open reference dataset
    ds_ref = gdal.Open("../gcore/data/byte.tif")
    checksum_ref = ds_ref.GetRasterBand(1).Checksum()
    ds_ref = None

    grid_near_search3_tif = str(tmp_path / "grid_near_search3.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    # Try the search ellipse larger than the raster cell.
    with gdaltest.config_option(
        "GDAL_GRID_POINT_COUNT_THRESHOLD", "0" if use_quadtree else "1000000000"
    ):
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=180.0:radius2=180.0:angle=0.0:nodata=0.0 data/grid.vrt {grid_near_search3_tif}"
        )

        # We should get the same values as in "gcore/data/byte.tif"
        ds = gdal.Open(grid_near_search3_tif)
        if ds.GetRasterBand(1).Checksum() != checksum_ref:
            print(
                "bad checksum : got %d, expected %d"
                % (ds.GetRasterBand(1).Checksum(), checksum_ref)
            )
            pytest.fail("bad checksum")
        ds = None

        #################
        grid_near_search1_tif = str(tmp_path / "grid_near_search1.tif")

        # Search ellipse smaller than the raster cell.
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=20.0:radius2=20.0:angle=0.0:nodata=0.0 data/grid.vrt {grid_near_search1_tif}"
        )

        # We should get the same values as in "gcore/data/byte.tif"
        ds = gdal.Open(grid_near_search1_tif)
        if ds.GetRasterBand(1).Checksum() != checksum_ref:
            print(
                "bad checksum : got %d, expected %d"
                % (ds.GetRasterBand(1).Checksum(), checksum_ref)
            )
            pytest.fail("bad checksum")
        ds = None

        #################
        grid_near_shift_search3_tif = str(tmp_path / "grid_near_shift_search3.tif")

        # Large search ellipse and the grid shift.
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=180.0:radius2=180.0:angle=0.0:nodata=0.0 data/grid.vrt {grid_near_shift_search3_tif}"
        )

        # We should get the same values as in "gcore/data/byte.tif"
        ds = gdal.Open(grid_near_shift_search3_tif)
        if ds.GetRasterBand(1).Checksum() != checksum_ref:
            print(
                "bad checksum : got %d, expected %d"
                % (ds.GetRasterBand(1).Checksum(), checksum_ref)
            )
            pytest.fail("bad checksum")
        ds = None

        #################
        grid_near_shift_search1_tif = str(tmp_path / "grid_near_shift_search1.tif")

        # Small search ellipse and the grid shift.
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=20.0:radius2=20.0:angle=0.0:nodata=0.0 data/grid.vrt {grid_near_shift_search1_tif}"
        )

        # We should get the same values as in "gcore/data/byte.tif"
        ds = gdal.Open(grid_near_shift_search1_tif)
        if ds.GetRasterBand(1).Checksum() != checksum_ref:
            print(
                "bad checksum : got %d, expected %d"
                % (ds.GetRasterBand(1).Checksum(), checksum_ref)
            )
            pytest.fail("bad checksum")
        ds = None


###############################################################################
# Test Inverse Distance to a Power gridding algorithm


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize(
    "algorithm,threads",
    [("Generic", None), ("SSE", None), ("AVX", None), ("AVX", 1), ("AVX", 2)],
)
def test_gdal_grid_4(gdal_grid_path, algorithm, threads, tmp_path):

    alg_flags = {
        "Generic": "--config GDAL_USE_AVX NO --config GDAL_USE_SSE NO",
        "SSE": "--config GDAL_USE_AVX NO",
        "AVX": "",
    }

    output_tif = str(tmp_path / "output.tif")

    flags = alg_flags[algorithm]

    if threads:
        flags += " --config GDAL_NUM_THREADS {threads}"

    # Create a GDAL dataset from the values of "grid.csv".
    _, err = gdaltest.runexternal_out_and_err(
        gdal_grid_path
        + f" --debug on {flags} -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
    )

    pos = err.find(" threads")
    if pos >= 0:
        pos_blank = err[0 : pos - 1].rfind(" ")
        if pos_blank >= 0:
            print("Step 1: %s threads used" % err[pos_blank + 1 : pos])

    if algorithm == "SSE" and "SSE" not in err:
        pytest.skip(f"{algorithm} not used")


@pytest.mark.require_driver("CSV")
def test_gdal_grid_4bis(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_invdist_90_90_8p.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    # Circular window, shifted, test min points and NODATA setting.
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdist:power=2.0:radius1=90.0:radius2=90.0:angle=0.0:max_points=0:min_points=8:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "ref_data/grid_invdist_90_90_8p.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_invdist_90_90_8p.tif")
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail("Image too different from the reference")
    ds_ref = None
    ds = None


###############################################################################
# Test Moving Average gridding algorithm


@pytest.mark.require_driver("CSV")
def test_gdal_grid_5(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_average.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    # We are using all the points from input dataset to average, so
    # the result is a raster filled with the same value in each node.
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "ref_data/grid_average.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_average.tif")
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    ds_ref = None
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail("Image too different from the reference")
    ds = None


@pytest.mark.require_driver("CSV")
def test_gdal_grid_5bis(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_average_300_100_40.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    # Elliptical window, rotated.
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average:radius1=300.0:radius2=100.0:angle=40.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "ref_data/grid_average_300_100_40.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_average_300_100_40.tif")
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    ds_ref = None
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail("Image too different from the reference")
    ds = None


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_6(gdal_grid_path, tmp_path, use_quadtree):

    output_tif = str(tmp_path / "grid_average_190_190.tif")

    with gdaltest.config_option(
        "GDAL_GRID_POINT_COUNT_THRESHOLD", "0" if use_quadtree else "1000000000"
    ):
        # Create a GDAL dataset from the values of "grid.csv".
        # This time using a circular window.
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average:radius1=190.0:radius2=190.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
        )

        # We should get the same values as in "ref_data/grid_average_190_190.tif"
        ds = gdal.Open(output_tif)
        ds_ref = gdal.Open("ref_data/grid_average_190_190.tif")
        maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
        ds_ref = None
        if maxdiff > 1:
            gdaltest.compare_ds(ds, ds_ref, verbose=1)
            pytest.fail("Image too different from the reference")
        ds = None


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_6bis(gdal_grid_path, tmp_path, use_quadtree):

    output_tif = str(tmp_path / "grid_average_90_90_8p.tif")

    with gdaltest.config_option(
        "GDAL_GRID_POINT_COUNT_THRESHOLD", "0" if use_quadtree else "1000000000"
    ):

        # Create a GDAL dataset from the values of "grid.csv".
        # Circular window, shifted, test min points and NODATA setting.
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average:radius1=90.0:radius2=90.0:angle=0.0:min_points=8:nodata=0.0 data/grid.vrt {output_tif}"
        )

        # We should get the same values as in "ref_data/grid_average_90_90_8p.tif"
        ds = gdal.Open(output_tif)
        ds_ref = gdal.Open("ref_data/grid_average_90_90_8p.tif")
        maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
        ds_ref = None
        if maxdiff > 1:
            gdaltest.compare_ds(ds, ds_ref, verbose=1)
            pytest.fail("Image too different from the reference")
        ds = None


###############################################################################
# Test Minimum data metric


@pytest.mark.require_driver("CSV")
def test_gdal_grid_7(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_minimum.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    # Search the whole dataset for minimum.
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a minimum:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "ref_data/grid_minimum.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_minimum.tif")
    assert (
        ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
    ), "bad checksum : got %d, expected %d" % (
        ds.GetRasterBand(1).Checksum(),
        ds_ref.GetRasterBand(1).Checksum(),
    )
    ds_ref = None
    ds = None


@pytest.mark.require_driver("CSV")
def test_gdal_grid_7bis(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_minimum_400_100_120.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    # Elliptical window, rotated.
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a minimum:radius1=400.0:radius2=100.0:angle=120.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "ref_data/grid_minimum_400_100_120.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_minimum_400_100_120.tif")
    assert (
        ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
    ), "bad checksum : got %d, expected %d" % (
        ds.GetRasterBand(1).Checksum(),
        ds_ref.GetRasterBand(1).Checksum(),
    )
    ds_ref = None
    ds = None


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_8(gdal_grid_path, tmp_path, use_quadtree):

    output_tif = str(tmp_path / "grid_minimum_180_180.tif")

    with gdaltest.config_option(
        "GDAL_GRID_POINT_COUNT_THRESHOLD", "0" if use_quadtree else "1000000000"
    ):
        # Create a GDAL dataset from the values of "grid.csv".
        # Search ellipse larger than the raster cell.
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a minimum:radius1=180.0:radius2=180.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
        )

        # We should get the same values as in "ref_data/grid_minimum_180_180.tif"
        ds = gdal.Open(output_tif)
        ds_ref = gdal.Open("ref_data/grid_minimum_180_180.tif")
        assert (
            ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
        ), "bad checksum : got %d, expected %d" % (
            ds.GetRasterBand(1).Checksum(),
            ds_ref.GetRasterBand(1).Checksum(),
        )
        ds_ref = None
        ds = None


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_8bis(gdal_grid_path, tmp_path, use_quadtree):

    output_tif = str(tmp_path / "grid_minimum_20_20.tif")

    with gdaltest.config_option(
        "GDAL_GRID_POINT_COUNT_THRESHOLD", "0" if use_quadtree else "1000000000"
    ):

        # Create a GDAL dataset from the values of "grid.csv".
        # Search ellipse smaller than the raster cell.
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a minimum:radius1=20.0:radius2=20.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
        )

        # We should get the same values as in "ref_data/grid_minimum_20_20.tif"
        ds = gdal.Open(output_tif)
        ds_ref = gdal.Open("ref_data/grid_minimum_20_20.tif")
        assert (
            ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
        ), "bad checksum : got %d, expected %d" % (
            ds.GetRasterBand(1).Checksum(),
            ds_ref.GetRasterBand(1).Checksum(),
        )
        ds_ref = None
        ds = None


###############################################################################
# Test Maximum data metric


@pytest.mark.require_driver("CSV")
def test_gdal_grid_9(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_maximum.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    # Search the whole dataset for maximum.
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a maximum:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "ref_data/grid_maximum.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_maximum.tif")
    assert (
        ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
    ), "bad checksum : got %d, expected %d" % (
        ds.GetRasterBand(1).Checksum(),
        ds_ref.GetRasterBand(1).Checksum(),
    )
    ds_ref = None
    ds = None


@pytest.mark.require_driver("CSV")
def test_gdal_grid_9bis(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_maximum_100_100.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    # Circular window.
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a maximum:radius1=100.0:radius2=100.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "ref_data/grid_maximum_100_100.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_maximum_100_100.tif")
    assert (
        ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
    ), "bad checksum : got %d, expected %d" % (
        ds.GetRasterBand(1).Checksum(),
        ds_ref.GetRasterBand(1).Checksum(),
    )
    ds_ref = None
    ds = None


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_10bis(gdal_grid_path, tmp_path, use_quadtree):

    output_tif = str(tmp_path / "grid_maximum_180_180.tif")

    with gdaltest.config_option(
        "GDAL_GRID_POINT_COUNT_THRESHOLD", "0" if use_quadtree else "1000000000"
    ):
        # Create a GDAL dataset from the values of "grid.csv".
        # Search ellipse larger than the raster cell.
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a maximum:radius1=180.0:radius2=180.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
        )

        # We should get the same values as in "ref_data/grid_maximum_180_180.tif"
        ds = gdal.Open(output_tif)
        ds_ref = gdal.Open("ref_data/grid_maximum_180_180.tif")
        assert (
            ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
        ), "bad checksum : got %d, expected %d" % (
            ds.GetRasterBand(1).Checksum(),
            ds_ref.GetRasterBand(1).Checksum(),
        )
        ds_ref = None
        ds = None


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_10(gdal_grid_path, tmp_path, use_quadtree):

    output_tif = str(tmp_path / "grid_maximum_20_20.tif")

    with gdaltest.config_option(
        "GDAL_GRID_POINT_COUNT_THRESHOLD", "0" if use_quadtree else "1000000000"
    ):

        # Create a GDAL dataset from the values of "grid.csv".
        # Search ellipse smaller than the raster cell.
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a maximum:radius1=20.0:radius2=20.0:angle=120.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
        )

        # We should get the same values as in "ref_data/grid_maximum_20_20.tif"
        ds = gdal.Open(output_tif)
        ds_ref = gdal.Open("ref_data/grid_maximum_20_20.tif")
        assert (
            ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
        ), "bad checksum : got %d, expected %d" % (
            ds.GetRasterBand(1).Checksum(),
            ds_ref.GetRasterBand(1).Checksum(),
        )
        ds_ref = None
        ds = None


###############################################################################
# Test Range data metric


@pytest.mark.require_driver("CSV")
def test_gdal_grid_11(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_range.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    # Search the whole dataset.
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a range:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "ref_data/grid_range.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_range.tif")
    assert (
        ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
    ), "bad checksum : got %d, expected %d" % (
        ds.GetRasterBand(1).Checksum(),
        ds_ref.GetRasterBand(1).Checksum(),
    )
    ds_ref = None
    ds = None


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_12(gdal_grid_path, tmp_path, use_quadtree):

    output_tif = str(tmp_path / "grid_range_90_90_8p.tif")

    with gdaltest.config_option(
        "GDAL_GRID_POINT_COUNT_THRESHOLD", "0" if use_quadtree else "1000000000"
    ):
        # Create a GDAL dataset from the values of "grid.csv".
        # Circular window, fill node with NODATA value if less than required
        # points found.
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a range:radius1=90.0:radius2=90.0:angle=0.0:min_points=8:nodata=0.0 data/grid.vrt {output_tif}"
        )

        # We should get the same values as in "ref_data/grid_range_90_90_8p.tif"
        ds = gdal.Open(output_tif)
        ds_ref = gdal.Open("ref_data/grid_range_90_90_8p.tif")
        assert (
            ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
        ), "bad checksum : got %d, expected %d" % (
            ds.GetRasterBand(1).Checksum(),
            ds_ref.GetRasterBand(1).Checksum(),
        )
        ds_ref = None
        ds = None


###############################################################################
# Test Count data metric


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_13bis(gdal_grid_path, tmp_path, use_quadtree):

    output_tif = str(tmp_path / "grid_count_70_70.tif")

    with gdaltest.config_option(
        "GDAL_GRID_POINT_COUNT_THRESHOLD", "0" if use_quadtree else "1000000000"
    ):
        # Create a GDAL dataset from the values of "grid.csv".
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a count:radius1=70.0:radius2=70.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
        )

        # We should get the same values as in "ref_data/grid_count_70_70.tif"
        ds = gdal.Open(output_tif)
        ds_ref = gdal.Open("ref_data/grid_count_70_70.tif")
        assert (
            ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
        ), "bad checksum : got %d, expected %d" % (
            ds.GetRasterBand(1).Checksum(),
            ds_ref.GetRasterBand(1).Checksum(),
        )
        ds_ref = None
        ds = None


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_13(gdal_grid_path, tmp_path, use_quadtree):

    output_tif = str(tmp_path / "grid_count_300_300.tif")

    with gdaltest.config_option(
        "GDAL_GRID_POINT_COUNT_THRESHOLD", "0" if use_quadtree else "1000000000"
    ):
        # Create a GDAL dataset from the values of "grid.csv".
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a count:radius1=300.0:radius2=300.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
        )

        # We should get the same values as in "ref_data/grid_count_300_300.tif"
        ds = gdal.Open(output_tif)
        ds_ref = gdal.Open("ref_data/grid_count_300_300.tif")
        assert (
            ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
        ), "bad checksum : got %d, expected %d" % (
            ds.GetRasterBand(1).Checksum(),
            ds_ref.GetRasterBand(1).Checksum(),
        )
        ds_ref = None
        ds = None


###############################################################################
# Test Average Distance data metric


@pytest.mark.require_driver("CSV")
def test_gdal_grid_14(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_avdist.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    # We are using all the points from input dataset to average, so
    # the result is a raster filled with the same value in each node.
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average_distance:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "ref_data/grid_avdist.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_avdist.tif")
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail("Image too different from the reference")
    ds = None
    ds_ref = None


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_15(gdal_grid_path, tmp_path, use_quadtree):

    output_tif = str(tmp_path / "grid_avdist_150_150.tif")

    with gdaltest.config_option(
        "GDAL_GRID_POINT_COUNT_THRESHOLD", "0" if use_quadtree else "1000000000"
    ):
        # Create a GDAL dataset from the values of "grid.csv".
        # We are using all the points from input dataset to average, so
        # the result is a raster filled with the same value in each node.
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average_distance:radius1=150.0:radius2=150.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
        )

        # We should get the same values as in "ref_data/grid_avdist_150_150.tif"
        ds = gdal.Open(output_tif)
        ds_ref = gdal.Open("ref_data/grid_avdist_150_150.tif")
        maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
        if maxdiff > 1:
            gdaltest.compare_ds(ds, ds_ref, verbose=1)
            pytest.fail("Image too different from the reference")
        ds = None
        ds_ref = None


###############################################################################
# Test Average Distance Between Points data metric


@pytest.mark.require_driver("CSV")
def test_gdal_grid_16(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_avdistpts_150_50_-15.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    # We are using all the points from input dataset to average, so
    # the result is a raster filled with the same value in each node.
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average_distance_pts:radius1=150.0:radius2=50.0:angle=-15.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "ref_data/grid_avdistpts_150_50_-15.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_avdistpts_150_50_-15.tif")
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 1:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail("Image too different from the reference")
    ds = None
    ds_ref = None


@pytest.mark.require_driver("CSV")
@pytest.mark.parametrize("use_quadtree", [True, False])
def test_gdal_grid_17(gdal_grid_path, tmp_path, use_quadtree):

    output_tif = str(tmp_path / "grid_avdistpts_150_150.tif")

    with gdaltest.config_option(
        "GDAL_GRID_POINT_COUNT_THRESHOLD", "0" if use_quadtree else "1000000000"
    ):
        # Create a GDAL dataset from the values of "grid.csv".
        # We are using all the points from input dataset to average, so
        # the result is a raster filled with the same value in each node.
        gdaltest.runexternal(
            gdal_grid_path
            + f" -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a average_distance_pts:radius1=150.0:radius2=150.0:angle=0.0:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
        )

        # We should get the same values as in "ref_data/grid_avdistpts_150_150.tif"
        ds = gdal.Open(output_tif)
        ds_ref = gdal.Open("ref_data/grid_avdistpts_150_150.tif")
        maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
        if maxdiff > 1:
            gdaltest.compare_ds(ds, ds_ref, verbose=1)
            pytest.fail("Image too different from the reference")
        ds = None
        ds_ref = None


###############################################################################
# Test linear


@pytest.mark.skipif(not gdal.HasTriangulation(), reason="qhull missing")
def test_gdal_grid_18(gdal_grid_path, tmp_path, n43_shp):

    output_tif = str(tmp_path / "n43_linear.tif")

    # Create a GDAL dataset from the previous generated OGR grid
    _, err = gdaltest.runexternal_out_and_err(
        gdal_grid_path
        + f" -txe -80.0041667 -78.9958333 -tye 42.9958333 44.0041667 -outsize 121 121 -ot Int16 -l n43 -a linear -co TILED=YES -co BLOCKXSIZE=256 -co BLOCKYSIZE=256 {n43_shp} {output_tif}"
    )
    assert err is None or err == "", "got error/warning"

    # We should get the same values as in n43.tif
    ds = gdal.Open("../gdrivers/data/n43.tif")
    ds2 = gdal.Open(output_tif)
    assert (
        ds.GetRasterBand(1).Checksum() == ds2.GetRasterBand(1).Checksum()
    ), "bad checksum : got %d, expected %d" % (
        ds.GetRasterBand(1).Checksum(),
        ds2.GetRasterBand(1).Checksum(),
    )

    ds = None
    ds2 = None


###############################################################################
# Test Inverse Distance to a Power with Nearest Neighbor gridding algorithm


@pytest.mark.require_driver("CSV")
def test_gdal_grid_19(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_invdistnn_generic.tif")
    #################
    # Test generic implementation (no AVX, no SSE)

    # Create a GDAL dataset from the values of "grid.csv".
    _, _ = gdaltest.runexternal_out_and_err(
        gdal_grid_path
        + f" -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdistnn:power=2.0:radius=1.0:max_points=12:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "ref_data/gdal_invdistnn.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_invdistnn.tif")
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 0.00001:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail("Image too different from the reference")
    ds_ref = None
    ds = None


@pytest.mark.require_driver("CSV")
def test_gdal_grid_19_250_8minp(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_invdistnn_250_8minp.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    # Circular window, shifted, test min points and NODATA setting.
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdistnn:power=2.0:radius=250.0:max_points=12:min_points=8:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "ref_data/grid_invdistnn_250_8minp.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_invdistnn_250_8minp.tif")
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 0.00001:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail("Image too different from the reference")
    ds_ref = None
    ds = None


@pytest.mark.require_driver("CSV")
def test_gdal_grid_19_250_10maxp_3pow(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_invdistnn_250_10maxp_3pow.tif")

    #################
    # Test generic implementation with max_points and radius specified

    # Create a GDAL dataset from the values of "grid.csv".
    gdaltest.runexternal(
        gdal_grid_path
        + f" -txe 440721.0 441920.0 -tye 3751321.0 3750120.0 -outsize 20 20 -ot Float64 -l grid -a invdistnn:power=3.0:radius=250.0:max_points=10:min_points=0:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "ref_data/gdal_invdistnn_250_10maxp_3pow.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_invdistnn_250_10maxp_3pow.tif")
    maxdiff = gdaltest.compare_ds(ds, ds_ref, verbose=0)
    if maxdiff > 0.00001:
        gdaltest.compare_ds(ds, ds_ref, verbose=1)
        pytest.fail("Image too different from the reference")
    ds_ref = None
    ds = None


###############################################################################
# Test -clipsrc


@pytest.mark.require_driver("CSV")
@pytest.mark.require_geos
def test_gdal_grid_clipsrc(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_clipsrc.tif")
    clip_csv = str(tmp_path / "clip.csv")

    open(clip_csv, "wt").write(
        'id,WKT\n1,"POLYGON((440750 3751340,440750 3750100,441900 3750100,441900 3751340,440750 3751340))"\n'
    )

    # Create a GDAL dataset from the values of "grid.csv".
    # Grid nodes are located exactly in raster nodes.
    gdaltest.runexternal_out_and_err(
        gdal_grid_path
        + f" -clipsrc {clip_csv} -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -outsize 20 20 -ot Byte -l grid -a nearest:radius1=0.0:radius2=0.0:angle=0.0:nodata=0.0 data/grid.vrt {output_tif}"
    )

    # We should get the same values as in "gcore/data/byte.tif"
    ds = gdal.Open(output_tif)
    cs = ds.GetRasterBand(1).Checksum()
    assert not (cs == 0 or cs == 4672), "bad checksum"
    ds = None


###############################################################################
# Test -tr


@pytest.mark.require_driver("CSV")
def test_gdal_grid_tr(gdal_grid_path, tmp_path):

    output_tif = str(tmp_path / "grid_count_70_70.tif")

    # Create a GDAL dataset from the values of "grid.csv".
    gdaltest.runexternal(
        gdal_grid_path
        + " -txe 440720.0 441920.0 -tye 3751320.0 3750120.0 -tr 60 60 -ot Byte -l grid -a count:radius1=70.0:radius2=70.0:angle=0.0:min_points=0:nodata=0.0 "
        + " -oo X_POSSIBLE_NAMES=field_1 -oo Y_POSSIBLE_NAMES=field_2 -oo Z_POSSIBLE_NAMES=field_3"
        + " data/grid.csv "
        + output_tif
    )

    # We should get the same values as in "ref_data/grid_count_70_70.tif"
    ds = gdal.Open(output_tif)
    ds_ref = gdal.Open("ref_data/grid_count_70_70.tif")
    assert (
        ds.GetRasterBand(1).Checksum() == ds_ref.GetRasterBand(1).Checksum()
    ), "bad checksum : got %d, expected %d" % (
        ds.GetRasterBand(1).Checksum(),
        ds_ref.GetRasterBand(1).Checksum(),
    )
    ds_ref = None
    ds = None
