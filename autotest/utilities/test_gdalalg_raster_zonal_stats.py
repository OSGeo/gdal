#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster zonal-stats' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2025, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import math
import sys

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr


@pytest.fixture()
def zonal():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("zonal-stats")


@pytest.fixture(params=["raster", "feature"])
def strategy(request):
    if request.param == "raster" and not ogrtest.have_geos():
        pytest.skip("--strategy raster requires GEOS")

    return request.param


def have_fractional_pixels():
    return ogrtest.have_geos() and (
        ogr.GetGEOSVersionMajor(),
        ogr.GetGEOSVersionMinor(),
    ) >= (3, 14, 1)


@pytest.fixture(params=["default", "all-touched", "fractional"])
def pixels(request):
    if request.param == "fractional" and not have_fractional_pixels():
        pytest.skip("--pixels fractional requires GEOS >= 3.14.1")

    return request.param


@pytest.fixture(scope="module")
def polyrast():

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    ds = gdal.GetDriverByName("MEM").Create("", 20, 20, 1, eType=gdal.GDT_Int16)
    ds.SetSpatialRef(osr.SpatialReference(epsg=27700))
    ds.SetGeoTransform((478000, 200, 0, 4766000, 0, -200))
    ds.WriteArray(np.arange(400).reshape(20, 20))

    return ds


def field_names(f):
    return [f.GetFieldDefnRef(i).GetName() for i in range(f.GetFieldCount())]


def test_gdalalg_raster_zonal_stats_zones_dataset_empty(zonal):

    src_ds = gdal.Open("../gcore/data/gtiff/rgbsmall_NONE_tiled.tif")

    zonal["input"] = src_ds
    zonal["zones"] = gdal.GetDriverByName("MEM").Create("", 1, 1, 0)
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["stat"] = "sum"

    with pytest.raises(Exception, match="Zones dataset has no band or layer"):
        zonal.Run()


def test_gdalalg_raster_zonal_stats_no_geotransform(zonal):

    zonal["input"] = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    zonal["zones"] = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["stat"] = "sum"

    with pytest.raises(Exception, match="Dataset has no geotransform"):
        zonal.Run()


def test_gdalalg_raster_zonal_stats_non_invertible_geotransform(zonal):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.SetGeoTransform([0] * 6)
    zonal["input"] = src_ds
    zonal["zones"] = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["stat"] = "sum"

    with pytest.raises(Exception, match="Dataset geotransform cannot be inverted"):
        zonal.Run()


@pytest.mark.parametrize("band", (1, 2, [3, 1]))
def test_gdalalg_raster_zonal_stats_polygon_zones_basic(zonal, strategy, pixels, band):

    src_ds = gdal.Open("../gcore/data/gtiff/rgbsmall_NONE_tiled.tif")

    zonal["input"] = src_ds
    zonal["zones"] = gdaltest.wkt_ds(
        [
            "POLYGON((-44.8151 -22.9973, -44.7357 -22.9979,-44.7755 -22.9563,-44.8151 -22.9973))",
            "POLYGON((-44.7671 -23.0347,-44.7683 -23.0678,-44.7183 -23.0686,-44.7191 -23.0339,-44.7671 -23.0347))",
        ]
    )
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["strategy"] = strategy
    zonal["pixels"] = pixels
    zonal["stat"] = ["sum", "mean"] + (
        ["max", "max_center_x", "max_center_y"] if band == 1 else []
    )
    zonal["band"] = band
    zonal["chunk-size"] = "2k"  # force iteration over blocks
    zonal["layer-creation-option"] = {"FID": "my_fid"}

    assert zonal.Run()

    out_ds = zonal.Output()
    assert out_ds.GetLayer(0).GetFIDColumn() == "my_fid"

    results = [f for f in out_ds.GetLayer(0)]

    assert len(results) == 2

    if band == 1:
        if pixels == "fractional":
            assert results[0]["sum"] == pytest.approx(7130.21435281425)
            assert results[1]["sum"] == pytest.approx(12886.7218339441)
        elif pixels == "all-touched":
            assert results[0]["sum"] == 8251
            assert results[1]["sum"] == 15131
        else:
            assert results[0]["sum"] == 7148
            assert results[1]["sum"] == 12497

            try:
                from osgeo import gdal_array  # noqa: F401
            except Exception:
                return
            src_values = src_ds.GetRasterBand(1).ReadAsMaskedArray()

            for f in results:
                src_max_px = gdal.ApplyGeoTransform(
                    gdal.InvGeoTransform(src_ds.GetGeoTransform()),
                    f["max_center_x"],
                    f["max_center_y"],
                )
                assert src_values[int(src_max_px[1]), int(src_max_px[0])] == f["max"]
    elif band == 2:
        if pixels == "fractional":
            assert results[0]["sum"] == pytest.approx(9743.76203341247)
            assert results[1]["sum"] == pytest.approx(18396.5491179946)
        elif pixels == "all-touched":
            assert results[0]["sum"] == 11232
            assert results[1]["sum"] == 21693
        else:
            assert results[0]["sum"] == 9764
            assert results[1]["sum"] == 17917
    else:
        assert results[0].GetFieldCount() == 4
        assert field_names(results[0]) == [
            "sum_band_3",
            "mean_band_3",
            "sum_band_1",
            "mean_band_1",
        ]

        if pixels == "fractional":
            assert results[0]["sum_band_1"] == pytest.approx(7130.21435281425)
            assert results[1]["sum_band_1"] == pytest.approx(12886.7218339441)
        elif pixels == "all-touched":
            assert results[0]["sum_band_1"] == 8251
            assert results[1]["sum_band_1"] == 15131
        else:
            assert results[0]["sum_band_1"] == 7148
            assert results[1]["sum_band_1"] == 12497


@pytest.mark.parametrize("pixels", ["all-touched", "fractional"], indirect=True)
def test_gdalalg_raster_zonal_stats_polygon_zones_weighted(zonal, strategy, pixels):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    values = np.array(
        [
            [1, 2, 3, 4],
            [9, 10, 11, 12],
            [5, 6, 7, 8],
        ]
    )
    weights = np.array(
        [[0.3, 0.3, 0.3, 0.3], [0.1, 0.1, 0.1, 0.1], [0.2, 0.2, 0.2, 0.2]]
    )

    ny, nx = values.shape

    values_ds = gdal.GetDriverByName("MEM").Create("", nx, ny)
    values_ds.SetGeoTransform((0, 1, 0, ny, 0, -1))
    values_ds.WriteArray(values)

    weights_ds = gdal.GetDriverByName("MEM").Create("", nx, ny, eType=gdal.GDT_Float64)
    weights_ds.SetGeoTransform((0, 1, 0, ny, 0, -1))
    weights_ds.WriteArray(weights)

    zones_ds = gdaltest.wkt_ds(
        ["POLYGON ((0.5 0.5, 3.5 0.5, 3.5 2.5, 0.5 2.5, 0.5 0.5))"]
    )

    zonal["input"] = values_ds
    zonal["weights"] = weights_ds
    zonal["zones"] = zones_ds
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["strategy"] = strategy
    zonal["pixels"] = pixels
    zonal["stat"] = [
        "mean",
        "weighted_mean",
        "weighted_sum",
        "weighted_variance",
        "weighted_stdev",
    ]

    assert zonal.Run()

    out_ds = zonal.Output()

    results = out_ds.GetLayer(0).GetNextFeature()

    if pixels == "all-touched":
        assert results["mean"] == pytest.approx(np.average(values))
        assert results["weighted_mean"] == pytest.approx(
            np.average(values, weights=weights)
        )
        assert results["weighted_sum"] == pytest.approx(np.sum(values * weights))

        # > Weighted.Desc.Stat::w.var(c(1:4,9:12,5:8), rep(c(0.3, 0.1, 0.2), each=4))
        assert results["weighted_variance"] == pytest.approx(10.13889)

        # > Weighted.Desc.Stat::w.sd(c(1:4,9:12,5:8), rep(c(0.3, 0.1, 0.2), each=4))
        assert results["weighted_stdev"] == pytest.approx(3.184162)

    elif pixels == "fractional":
        cov = np.array(
            [[0.25, 0.5, 0.5, 0.25], [0.5, 1, 1, 0.5], [0.25, 0.5, 0.5, 0.25]]
        )

        assert results["mean"] == pytest.approx(np.average(values, weights=cov))
        assert results["weighted_mean"] == pytest.approx(
            np.average(values, weights=weights * cov)
        )
        assert results["weighted_sum"] == pytest.approx(np.sum(values * weights * cov))


@pytest.mark.parametrize("band", (1, 2, [3, 1]))
def test_gdalalg_raster_zonal_stats_raster_zones(zonal, band):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    src_ds = gdal.Open("../gcore/data/gtiff/rgbsmall_NONE_tiled.tif")

    nx, ny = src_ds.RasterXSize, src_ds.RasterYSize

    zones_ds = gdal.GetDriverByName("MEM").Create("", nx, ny)
    zones_ds.SetGeoTransform(src_ds.GetGeoTransform())
    zones = np.round(src_ds.GetRasterBand(1).ReadAsArray() / 51).astype(np.uint8)
    zones_ds.WriteArray(zones)

    weights_ds = gdal.GetDriverByName("MEM").Create("", nx, ny, eType=gdal.GDT_Float32)
    weights_ds.SetGeoTransform(src_ds.GetGeoTransform())
    weights = np.arange(ny * nx).reshape(ny, nx) / 1000
    weights_ds.WriteArray(weights)

    zonal["input"] = src_ds
    zonal["weights"] = weights_ds
    zonal["zones"] = zones_ds
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["stat"] = [
        "sum",
        "mean",
        "weighted_mean",
        "max",
        "max_center_y",
        "max_center_x",
    ]
    zonal["band"] = band
    zonal["chunk-size"] = "2k"  # force iteration over blocks

    assert zonal.Run()

    out_ds = zonal.Output()

    results = [f for f in out_ds.GetLayer(0)]

    assert len(results) == len(np.unique(zones))

    if type(band) is int:
        band = [band]

    for b in band:
        src_values = src_ds.GetRasterBand(b).ReadAsArray()

        def stat_field(x):
            return x if len(band) == 1 else f"{x}_band_{b}"

        for i, value in enumerate(np.unique(zones)):
            assert results[i]["value"] == value, f"i={i}"
            assert (
                results[i][stat_field("sum")] == src_values[zones == value].sum()
            ), f"i={i}"
            assert results[i][stat_field("weighted_mean")] == pytest.approx(
                np.average(src_values[zones == value], weights=weights[zones == value])
            ), f"i={i}"

            assert results[i][stat_field("max")] == src_values[zones == value].max()

            src_max_px = gdal.ApplyGeoTransform(
                gdal.InvGeoTransform(src_ds.GetGeoTransform()),
                results[i][stat_field("max_center_x")],
                results[i][stat_field("max_center_y")],
            )

            assert (
                src_values[int(src_max_px[1]), int(src_max_px[0])]
                == results[i][stat_field("max")]
            )


@pytest.mark.skipif(not have_fractional_pixels(), reason="requires GEOS >= 3.14.1")
@pytest.mark.parametrize(
    "stat",
    [
        "center_x",
        "center_y",
        "count",
        "coverage",
        "max",
        "mean",
        "min",
        "minority",
        "mode",
        "stdev",
        "sum",
        ["frac", "unique"],
        "variance",
        "variety",
        "values",
        "max_center_x",
        "max_center_y",
        "min_center_x",
        "min_center_y",
        "weighted_mean",
        "weighted_sum",
        "weighted_stdev",
        "weighted_variance",
        "weights",
    ],
)
def test_gdalalg_raster_zonal_stats_polygon_zones_all_stats(zonal, strategy, stat):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    nodata = 99
    values = np.array(
        [
            [1, 1, 1, 1, 1],
            [1, 1, 2, 3, 1],
            [1, 4, 5, 6, 1],
            [1, 0, nodata, 7, 1],
            [1, 1, 1, 1, 1],
        ]
    )
    ny, nx = values.shape

    weights = np.array(
        [
            [0.3, 0.3, 0.3, 0.3, 0.3],
            [0.1, 0.1, 0.1, 0.1, 0.1],
            [0.2, 0.2, 0.2, 0.2, 0.2],
            [0.4, 0.4, 0.4, 0.4, 0.4],
            [0.5, 0.5, 0.5, 0.5, 0.5],
        ]
    )

    ds = gdal.GetDriverByName("MEM").Create("", nx, ny)
    ds.SetGeoTransform((-1, 1, 0, ny - 1, 0, -1))
    ds.GetRasterBand(1).SetNoDataValue(nodata)
    ds.WriteArray(values)

    weights_ds = gdal.GetDriverByName("MEM").Create("", nx, ny, eType=gdal.GDT_Float64)
    weights_ds.SetGeoTransform((0, 1, 0, ny, 0, -1))
    weights_ds.WriteArray(weights)

    zones = gdaltest.wkt_ds(["POLYGON ((0.5 0.5, 2.5 0.5, 2.5 2.5, 0.5 2.5, 0.5 0.5))"])

    zonal["input"] = ds
    zonal["weights"] = weights_ds
    zonal["zones"] = zones
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["pixels"] = "fractional"
    zonal["strategy"] = strategy
    zonal["stat"] = (
        stat  # process stats individually to ensure RasterStatsOptions set correctly
    )

    assert zonal.Run()

    out_ds = zonal.Output()
    f = out_ds.GetLayer(0).GetNextFeature()

    expected_values = np.array([1, 2, 3, 4, 5, 6, 0, 7])
    expected_coverage = np.array(
        [0.25, 0.5, 0.25, 0.5, 1, 0.5, 0.25, 0.25], dtype=np.float32
    )
    expected_weights = np.array(
        [0.2, 0.2, 0.2, 0.4, 0.4, 0.4, 0.5, 0.5], dtype=np.float64
    )

    if stat == "center_x":
        np.testing.assert_array_equal(
            f["center_x"], np.array([0.5, 1.5, 2.5, 0.5, 1.5, 2.5, 0.5, 2.5])
        )
    elif stat == "center_y":
        np.testing.assert_array_equal(
            f["center_y"], np.array([2.5, 2.5, 2.5, 1.5, 1.5, 1.5, 0.5, 0.5])
        )
    elif stat == "coverage":
        np.testing.assert_array_equal(f["coverage"], expected_coverage)
    elif stat == "count":
        assert f["count"] == 3.5
    elif stat == ["frac", "unique"]:
        x = {k: v for (k, v) in zip(f["unique"], f["frac"])}

        assert set(x.keys()) == {0, 1, 2, 3, 4, 5, 6, 7}

        assert x[0] == 0.25 / 3.5
        assert x[1] == 0.25 / 3.5
        assert x[2] == 0.5 / 3.5
        assert x[3] == 0.25 / 3.5
        assert x[4] == 0.5 / 3.5
        assert x[5] == 1 / 3.5
        assert x[6] == 0.5 / 3.5
        assert x[7] == 0.25 / 3.5
    elif stat == "max":
        assert f["max"] == 7
    elif stat == "max_center_x":
        assert f["max_center_x"] == 2.5
    elif stat == "max_center_y":
        assert f["max_center_y"] == 0.5
    elif stat == "mean":
        assert f["mean"] == pytest.approx(13.75 / 3.5)
    elif stat == "min":
        assert f["min"] == 0
    elif stat == "minority":
        assert f["minority"] == 0
    elif stat == "min_center_x":
        assert f["min_center_x"] == 0.5
    elif stat == "min_center_y":
        assert f["min_center_y"] == 0.5
    elif stat == "mode":
        assert f["mode"] == 5
    elif stat == "stdev":
        assert f["stdev"] == pytest.approx(
            1.9807749
        )  # > Weighted.Desc.Stat::w.sd(c(1:6, 0, 7), c(0.25, 0.5, 0.25, 0.5, 1, 0.5, 0.25, 0.25))
    elif stat == "sum":
        assert f["sum"] == 13.75
    elif stat == "variety":
        assert f["variety"] == 8
    elif stat == "values":
        np.testing.assert_array_equal(f["values"], [1, 2, 3, 4, 5, 6, 0, 7])
    elif stat == "variance":
        assert f["variance"] == pytest.approx(
            3.923469
        )  # > Weighted.Desc.Stat::w.var(c(1:6, 0, 7), c(0.25, 0.5, 0.25, 0.5, 1, 0.5, 0.25, 0.25))
    elif stat == "weighted_mean":
        assert f["weighted_mean"] == pytest.approx(
            np.average(expected_values, weights=expected_weights * expected_coverage)
        )
    elif stat == "weighted_stdev":
        assert f["weighted_stdev"] == pytest.approx(
            2.032634
        )  # > Weighted.Desc.Stat::w.sd(c(1:6, 0, 7), c(0.25, 0.5, 0.25, 0.5, 1, 0.5, 0.25, 0.25)*c(0.2, 0.2, 0.2, 0.4, 0.4, 0.4, 0.5, 0.5))
    elif stat == "weighted_variance":
        assert f["weighted_variance"] == pytest.approx(
            4.1316
        )  # > Weighted.Desc.Stat::w.var(c(1:6, 0, 7), c(0.25, 0.5, 0.25, 0.5, 1, 0.5, 0.25, 0.25)*c(0.2, 0.2, 0.2, 0.4, 0.4, 0.4, 0.5, 0.5))
    elif stat == "weighted_sum":
        assert f["weighted_sum"] == pytest.approx(
            np.sum(expected_values * expected_weights * expected_coverage)
        )
    elif stat == "weights":
        np.testing.assert_array_equal(f["weights"], expected_weights)
    else:
        pytest.fail(f"No assert for stat: {stat}")


def test_gdalalg_raster_zonal_stats_weighted_stats_nodata(zonal):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    nx, ny = 3, 4
    values_ds = gdal.GetDriverByName("MEM").Create("", nx, ny)
    values_ds.SetGeoTransform((0, 1, 0, ny, 0, -1))
    values_ds.WriteArray(np.arange(nx * ny).reshape(ny, nx))

    weights_ds = gdal.GetDriverByName("MEM").Create("", nx, ny)
    weights_ds.SetGeoTransform((0, 1, 0, ny, 0, -1))
    weights_ds.GetRasterBand(1).SetNoDataValue(6)
    weights_ds.WriteArray(np.arange(nx * ny).reshape(ny, nx))

    zones_ds = gdaltest.wkt_ds([f"POLYGON ((0 0, {nx} 0, {nx} {ny}, 0 {ny}, 0 0))"])

    zonal["input"] = values_ds
    zonal["weights"] = weights_ds
    zonal["zones"] = zones_ds
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["stat"] = [
        "weighted_mean",
        "weighted_sum",
        "weighted_stdev",
        "weighted_variance",
        "weights",
    ]

    assert zonal.Run()

    out_ds = zonal.Output()

    results = out_ds.GetLayer(0).GetNextFeature()

    assert math.isnan(results["weighted_mean"])
    assert math.isnan(results["weighted_sum"])
    assert math.isnan(results["weighted_stdev"])
    assert math.isnan(results["weighted_variance"])

    expected_weights = np.arange(nx * ny).astype(np.float64)
    expected_weights[expected_weights == 6] = float("nan")

    np.testing.assert_array_equal(results["weights"], expected_weights)


@pytest.mark.parametrize(
    "stat",
    ("weights", "weighted_mean", "weighted_sum", "weighted_stdev", "weighted_variance"),
)
def test_gdalalg_raster_zonal_stats_missing_weights(zonal, polyrast, stat):

    zonal["input"] = polyrast
    zonal["zones"] = "../ogr/data/poly.shp"
    zonal["stat"] = stat
    zonal["output"] = ""
    zonal["output-format"] = "MEM"

    with pytest.raises(Exception, match="requires weights"):
        zonal.Run()


def test_gdalalg_raster_zonal_stats_non_polygon_geometry(zonal, strategy):

    zonal["input"] = "../gcore/data/byte.tif"
    zonal["zones"] = gdaltest.wkt_ds(["LINESTRING (3 3, 8 8)"])
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["strategy"] = strategy
    zonal["stat"] = "sum"

    with pytest.raises(Exception, match="Non-polygonal geometry"):
        zonal.Run()


@pytest.mark.require_driver("CSV")
def test_gdalalg_raster_zonal_stats_output_format_detection(
    zonal, strategy, polyrast, tmp_vsimem
):
    zonal["input"] = polyrast
    zonal["output"] = tmp_vsimem / "out.csv"
    zonal["stat"] = "mean"
    zonal["zones"] = "../ogr/data/poly.shp"

    assert zonal.Run()

    out_ds = zonal.Output()

    assert out_ds.GetDriver().GetName() == "CSV"


def test_gdalalg_raster_zonal_stats_polygon_zones_include_fields(
    zonal, strategy, polyrast
):

    zonal["input"] = polyrast
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["strategy"] = strategy
    zonal["stat"] = "sum"
    zonal["include-field"] = "does_not_exist"
    zonal["zones"] = "../ogr/data/poly.shp"

    with pytest.raises(Exception, match="Field .* not found"):
        zonal.Run()

    zonal["include-field"] = ["PRFEDEA", "EAS_ID"]

    assert zonal.Run()

    out_ds = zonal.Output()

    f = out_ds.GetLayer(0).GetNextFeature()

    assert field_names(f) == ["PRFEDEA", "EAS_ID", "sum"]

    assert f["PRFEDEA"] == "35043411"
    assert f["EAS_ID"] == 168
    assert f["sum"] == 369.0


def test_gdalalg_raster_zonal_stats_raster_zones_include_fields(zonal):

    zonal["input"] = "../gcore/data/byte.tif"
    zonal["zones"] = "../gcore/data/byte.tif"
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["stat"] = "sum"
    zonal["include-field"] = "id"

    with pytest.raises(Exception, match="Cannot include fields"):
        zonal.Run()


def test_gdalalg_raster_zonal_stats_raster_zones_invalid_band(zonal):

    zonal["input"] = "../gcore/data/byte.tif"
    zonal["zones"] = "../gcore/data/byte.tif"
    zonal["zones-band"] = 2
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["stat"] = "sum"

    with pytest.raises(Exception, match="Invalid zones band: 2"):
        zonal.Run()


def test_gdalalg_raster_zonal_stats_polygon_zones_invalid_band(zonal, polyrast):

    zonal["input"] = polyrast
    zonal["zones"] = "../ogr/data/poly.shp"
    zonal["stat"] = "sum"
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["band"] = 2

    with pytest.raises(Exception, match="Value of 'band' should be"):
        zonal.Run()


def test_gdalalg_raster_zonal_stats_polygon_zones_invalid_layer(zonal, polyrast):

    zonal["input"] = polyrast
    zonal["zones"] = "../ogr/data/poly.shp"
    zonal["zones-layer"] = "does_not_exist"
    zonal["stat"] = "sum"
    zonal["output"] = ""
    zonal["output-format"] = "MEM"

    with pytest.raises(Exception, match="Specified zones layer .* not found"):
        zonal.Run()


def test_gdalalg_raster_zonal_stats_polygon_zones_invalid_chunk_size(zonal):

    with pytest.raises(Exception, match="Failed to parse memory size"):
        zonal["chunk-size"] = "2 gigs"

    with pytest.raises(Exception, match="too large"):
        zonal["chunk-size"] = "1e100 mb"

    with pytest.raises(Exception, match="must have a unit"):
        zonal["chunk-size"] = "512"


@pytest.mark.skipif(sys.maxsize <= 1 << 32, reason="only works on 64 bit")
def test_gdalalg_raster_zonal_stats_polygon_zones_large_chunk_size(zonal):

    zonal["input"] = "../gcore/data/byte.tif"
    zonal["zones"] = "../gcore/data/byte.tif"
    zonal["stat"] = "sum"
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["chunk-size"] = "10000GB"

    assert zonal.Run()


@pytest.mark.parametrize(
    "dtype",
    (
        gdal.GDT_Int64,
        gdal.GDT_UInt64,
        gdal.GDT_CFloat16,
        gdal.GDT_CFloat32,
        gdal.GDT_CFloat64,
    ),
)
def test_gdalalg_raster_zonal_stats_unsupported_type(zonal, dtype):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3, eType=dtype)
    src_ds.SetGeoTransform((0, 1, 0, 3, 0, -1))
    src_ds.GetRasterBand(1).Fill(1)

    zones = gdaltest.wkt_ds("POLYGON ((0 0, 3 0, 3 3, 0 0))")

    zonal["input"] = src_ds
    zonal["zones"] = zones
    zonal["stat"] = "sum"
    zonal["output"] = ""
    zonal["output-format"] = "MEM"

    with pytest.raises(Exception, match="Source data type"):
        zonal.Run()


def test_gdalalg_raster_zonal_stats_invalid_weight_band(zonal):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3, eType=gdal.GDT_Float32)
    src_ds.SetGeoTransform((0, 1, 0, 3, 0, -1))
    src_ds.GetRasterBand(1).Fill(1)

    weight_ds = gdal.GetDriverByName("MEM").Create("", 3, 3, eType=gdal.GDT_Float32)
    weight_ds.SetGeoTransform((0, 1, 0, 3, 0, -1))
    weight_ds.GetRasterBand(1).Fill(1)

    zones = gdaltest.wkt_ds("POLYGON ((0 0, 3 0, 3 3, 0 0))")

    zonal["input"] = src_ds
    zonal["weights"] = weight_ds
    zonal["weights-band"] = 2
    zonal["zones"] = zones
    zonal["stat"] = "weighted_mean"
    zonal["output"] = ""
    zonal["output-format"] = "MEM"

    with pytest.raises(Exception, match="invalid weights band"):
        zonal.Run()


@pytest.mark.parametrize(
    "dtype",
    (
        gdal.GDT_Int64,
        gdal.GDT_UInt64,
        gdal.GDT_CFloat16,
        gdal.GDT_CFloat32,
        gdal.GDT_CFloat64,
    ),
)
def test_gdalalg_raster_zonal_stats_unsupported_weight_type(zonal, dtype):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3, eType=gdal.GDT_Float32)
    src_ds.SetGeoTransform((0, 1, 0, 3, 0, -1))
    src_ds.GetRasterBand(1).Fill(1)

    weight_ds = gdal.GetDriverByName("MEM").Create("", 3, 3, eType=dtype)
    weight_ds.SetGeoTransform((0, 1, 0, 3, 0, -1))
    weight_ds.GetRasterBand(1).Fill(1)

    zones = gdaltest.wkt_ds("POLYGON ((0 0, 3 0, 3 3, 0 0))")

    zonal["input"] = src_ds
    zonal["weights"] = weight_ds
    zonal["zones"] = zones
    zonal["stat"] = "weighted_mean"
    zonal["output"] = ""
    zonal["output-format"] = "MEM"

    with pytest.raises(Exception, match="Weights data type"):
        zonal.Run()


@pytest.mark.parametrize(
    "raster_srs,weights_srs,zones_srs,warn",
    [
        (4326, None, None, False),
        (4326, None, 4326, False),
        (None, None, 4326, False),
        (None, None, None, False),
        (4326, None, 4269, True),
        (4326, 4269, None, True),
        (4326, 4269, 4326, True),
    ],
)
@pytest.mark.parametrize("zones_typ", ("raster", "vector"))
def test_gdalalg_raster_zonal_stats_srs_mismatch(
    zonal, raster_srs, weights_srs, zones_srs, zones_typ, warn
):

    src_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    src_ds.SetGeoTransform((0, 1, 0, 10, 0, -1))
    if raster_srs:
        src_ds.SetSpatialRef(osr.SpatialReference(epsg=raster_srs))

    weights_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    weights_ds.SetGeoTransform((0, 1, 0, 10, 0, -1))
    if weights_srs:
        weights_ds.SetSpatialRef(osr.SpatialReference(epsg=weights_srs))

    if zones_typ == "vector":
        zones_ds = gdaltest.wkt_ds(["POLYGON ((2 2, 8 2, 8 8, 2 2))"], epsg=zones_srs)
    else:
        zones_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
        zones_ds.SetGeoTransform((0, 1, 0, 10, 0, -1))
        if zones_srs:
            zones_ds.SetSpatialRef(osr.SpatialReference(epsg=zones_srs))

    zonal["input"] = src_ds
    zonal["weights"] = weights_ds
    zonal["zones"] = zones_ds
    zonal["stat"] = "weighted_mean"
    zonal["output"] = ""
    zonal["output-format"] = "MEM"

    err_type = gdal.CE_Warning if warn else gdal.CE_None

    with gdaltest.error_raised(err_type, match="do not have the same SRS"):
        assert zonal.Run()


@pytest.mark.parametrize("xoff", (-200, 0, 200))
@pytest.mark.parametrize("yoff", (-200, 0, 200))
def test_gdalalg_raster_zonal_stats_polygon_zone_outside_raster(
    zonal, strategy, xoff, yoff
):

    src_ds = gdal.Open("../gcore/data/byte.tif")

    xmin, xmax, ymin, ymax = src_ds.GetExtent()

    if xoff == 0 and yoff == 0:
        # polygon would be inside the raster
        return

    if xoff > 0:
        x0 = xmax + xoff
    else:
        x0 = xmin + xoff

    if yoff > 0:
        y0 = ymax + yoff
    else:
        y0 = ymin + yoff

    # square polygon the size of a single pixel
    wkt = f"POLYGON (({x0} {y0}, {x0 + 60} {y0}, {x0 + 60} {y0 + 60}, {x0} {y0 + 60}, {x0} {y0}))"

    zonal["input"] = "../gcore/data/byte.tif"
    zonal["zones"] = gdaltest.wkt_ds([wkt, "POLYGON EMPTY"])
    zonal["strategy"] = strategy
    zonal["stat"] = ["sum", "mode", "count"]
    zonal["output"] = ""
    zonal["output-format"] = "MEM"

    assert zonal.Run()

    out_ds = zonal.Output()

    results = [f for f in out_ds.GetLayer(0)]

    assert len(results) == 2
    assert results[0]["count"] == 0
    assert results[0]["sum"] == 0
    assert results[0]["mode"] is None

    assert results[0]["count"] == 0
    assert results[1]["sum"] == 0
    assert results[1]["mode"] is None


@pytest.mark.parametrize("src_nodata", (None, 99))
def test_gdalalg_raster_zonal_stats_raster_values_partially_outside(zonal, src_nodata):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    values = np.ones((10, 10))
    src_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    src_ds.SetGeoTransform((0, 1, 0, 10, 0, -1))
    if src_nodata:
        src_ds.GetRasterBand(1).SetNoDataValue(src_nodata)
    src_ds.WriteArray(values)

    # zones grid is compatible with values grid but offset
    zones = 1 + np.floor(np.arange(100).reshape(10, 10) / 20)
    zones_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    zones_ds.SetGeoTransform((2, 1, 0, 12, 0, -1))
    zones_ds.WriteArray(zones)

    values_adj = np.zeros((10, 10))
    values_adj[2:, 2:] = values[:-2, :-2]

    zonal["input"] = src_ds
    zonal["zones"] = zones_ds
    zonal["stat"] = ["count", "sum"]
    zonal["output"] = ""
    zonal["output-format"] = "MEM"

    with gdaltest.error_raised(
        gdal.CE_Warning, match="Source raster does not fully cover zones raster"
    ):
        assert zonal.Run()

    out_ds = zonal.Output()

    results = [f for f in out_ds.GetLayer(0)]

    assert len(results) == 5

    for i in range(1, 6):
        assert results[i - 1]["value"] == i

        if src_nodata:
            assert results[i - 1]["count"] == (16 if i > 1 else 0), f"zone={i}"
        else:
            # pixels outside src extent considered valid with value of zero
            assert results[i - 1]["count"] == 20
            assert results[i - 1]["sum"] == (16 if i > 1 else 0)


@pytest.mark.parametrize("src_nodata", (None, 99))
def test_gdalalg_raster_zonal_stats_raster_zones_entirely_outside(zonal, src_nodata):
    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    src_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    src_ds.SetGeoTransform((0, 1, 0, 10, 0, -1))
    src_ds.GetRasterBand(1).Fill(1)
    if src_nodata:
        src_ds.GetRasterBand(1).SetNoDataValue(src_nodata)

    zones = 1 + np.floor(np.arange(100).reshape(10, 10) / 20)
    zones_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    zones_ds.SetGeoTransform((1000, 1, 0, 1000, 0, -1))
    zones_ds.WriteArray(zones)

    zonal["input"] = src_ds
    zonal["zones"] = zones_ds
    zonal["stat"] = ["sum", "mode"]
    zonal["output"] = ""
    zonal["output-format"] = "MEM"

    with gdaltest.error_raised(
        gdal.CE_Warning, match="Source raster does not intersect zones raster"
    ):
        assert zonal.Run()

    out_ds = zonal.Output()
    results = [f for f in out_ds.GetLayer(0)]

    assert len(results) == 5

    for f in results:
        assert f["sum"] == 0
        if src_nodata:
            assert f["mode"] is None
        else:
            assert f["mode"] == 0


def test_gdalalg_raster_zonal_stats_raster_weights_partially_outside(zonal):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    src_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    src_ds.SetGeoTransform((0, 1, 0, 10, 0, -1))
    values = np.ones((10, 10))
    src_ds.WriteArray(values)

    # zones grid is same as values grid
    zones_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    zones_ds.SetGeoTransform((0, 1, 0, 10, 0, -1))
    zones = 1 + np.floor(np.arange(100).reshape(10, 10) / 20)
    zones_ds.WriteArray(zones)

    # weights grid is compatible with values grid but offset
    weights_ds = gdal.GetDriverByName("MEM").Create("", 10, 10, eType=gdal.GDT_Float32)
    weights_ds.SetGeoTransform((2, 1, 0, 12, 0, -1))
    weights = np.arange(100).reshape(10, 10) / 100
    weights_ds.WriteArray(weights)

    weights_adj = np.zeros((10, 10))
    weights_adj[:-2, 2:] = weights[2:, :-2]

    zonal["input"] = src_ds
    zonal["zones"] = zones_ds
    zonal["weights"] = weights_ds
    zonal["stat"] = "weighted_sum"
    zonal["output"] = ""
    zonal["output-format"] = "MEM"

    with gdaltest.error_raised(
        gdal.CE_Warning, match="Weighting raster does not fully cover zones raster"
    ):
        assert zonal.Run()

    out_ds = zonal.Output()
    results = [f for f in out_ds.GetLayer(0)]

    assert len(results) == 5

    for feature in results:
        zone = feature["value"]
        expected = np.sum(values[zones == zone] * weights_adj[zones == zone])
        assert feature["weighted_sum"] == pytest.approx(expected)


def test_gdalalg_raster_zonal_stats_vector_zones_weights_resampled(
    zonal, strategy, polyrast
):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    values = np.array([[1, 2], [3, 4]])
    weights = np.array([[1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12], [13, 14, 15, 16]])

    values_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    values_ds.SetGeoTransform((0, 1, 0, 2, 0, -1))
    values_ds.WriteArray(values)

    weights_ds = gdal.GetDriverByName("MEM").Create("", 4, 4, eType=gdal.GDT_Float32)
    weights_ds.SetGeoTransform((0, 0.5, 0, 2, 0, -0.5))
    weights_ds.WriteArray(weights)

    zonal["zones"] = gdaltest.wkt_ds(["POLYGON ((0 0, 2 0, 2 2, 0 2, 0 0))"])
    zonal["input"] = values_ds
    zonal["weights"] = weights_ds
    zonal["strategy"] = strategy
    zonal["output"] = ""
    zonal["output-format"] = "MEM"
    zonal["stat"] = ["weighted_mean", "weights"]

    with gdaltest.error_raised(gdal.CE_Warning, match="Resampled weight"):
        assert zonal.Run()

    out_ds = zonal.Output()
    results = out_ds.GetLayer(0).GetNextFeature()

    weights_agg = np.array([[3.5, 5.5], [11.5, 13.5]])

    assert results["weighted_mean"] == pytest.approx(
        np.average(values.flatten(), weights=weights_agg.flatten())
    )


def test_gdalalg_raster_zonal_stats_pipeline_usage(zonal, tmp_vsimem, polyrast):

    pipeline = gdal.Algorithm("pipeline")

    src_fname = tmp_vsimem / "polyrast.tif"
    out_fname = tmp_vsimem / "out.csv"

    gdal.GetDriverByName("GTiff").CreateCopy(src_fname, polyrast)

    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            src_fname,
            "!",
            "zonal-stats",
            "--zones",
            "../ogr/data/poly.shp",
            "--stat",
            "sum",
            "!",
            "write",
            out_fname,
        ]
    )

    with gdal.OpenEx(out_fname) as dst:
        assert dst.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_raster_zonal_stats_pipeline_piped_is_zones(
    zonal, tmp_vsimem, polyrast
):

    pipeline = gdal.Algorithm("pipeline")

    src_fname = tmp_vsimem / "polyrast.tif"
    out_fname = tmp_vsimem / "out.csv"

    gdal.GetDriverByName("GTiff").CreateCopy(src_fname, polyrast)

    assert pipeline.ParseRunAndFinalize(
        [
            "read",
            "../ogr/data/poly.shp",
            "!",
            "zonal-stats",
            "--input",
            src_fname,
            "--zones",
            "_PIPE_",
            "--stat",
            "sum",
            "!",
            "write",
            out_fname,
        ]
    )

    with gdal.OpenEx(out_fname) as dst:
        assert dst.GetLayer(0).GetFeatureCount() == 10


def test_gdalalg_raster_zonal_stats_null_geometry(zonal, strategy):

    zones = gdal.GetDriverByName("MEM").CreateVector("")
    zones_lyr = zones.CreateLayer("zones")
    zones_lyr.CreateField(ogr.FieldDefn("zone_id", ogr.OFTInteger))

    zones_feature = ogr.Feature(zones_lyr.GetLayerDefn())
    zones_lyr.CreateFeature(zones_feature)

    zonal["input"] = "../gcore/data/byte.tif"
    zonal["zones"] = zones
    zonal["strategy"] = strategy
    zonal["stat"] = ["sum", "mode"]
    zonal["output"] = ""
    zonal["output-format"] = "MEM"

    assert zonal.Run()

    out_ds = zonal.Output()

    results = [f for f in out_ds.GetLayer(0)]

    assert len(results) == 1

    assert results[0]["sum"] == 0
    assert results[0]["mode"] is None


def test_gdalalg_raster_zonal_stats_polygon_huge_extent(zonal, strategy):

    src_ds = gdal.GetDriverByName("MEM").Create("", 20, 20)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    zonal["input"] = "../gcore/data/byte.tif"
    v = 1e11
    zonal["zones"] = gdaltest.wkt_ds(
        [f"POLYGON ((-{v} -{v},-{v} {v},{v} {v},{v} -{v},-{v} -{v}))"]
    )
    zonal["strategy"] = strategy
    zonal["stat"] = ["count"]
    zonal["output"] = ""
    zonal["output-format"] = "MEM"

    assert zonal.Run()

    out_ds = zonal.Output()

    results = [f for f in out_ds.GetLayer(0)]

    assert len(results) == 1

    assert results[0]["count"] == 400


@pytest.mark.require_geos
@pytest.mark.slow()
def test_gdalalg_raster_zonal_stats_polygon_huge_extent_huge_raster(zonal):

    huge_raster = gdal.Open("""<VRTDataset rasterXSize="2147483647" rasterYSize="1">
  <GeoTransform>0,1,0,0,0,1</GeoTransform>
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">../gcore/data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="1" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    zonal["input"] = huge_raster
    v = 1e11
    zonal["zones"] = gdaltest.wkt_ds(
        [f"POLYGON ((-{v} -{v},-{v} {v},{v} {v},{v} -{v},-{v} -{v}))"]
    )
    zonal["strategy"] = "raster"
    zonal["stat"] = ["count"]
    zonal["output"] = ""
    zonal["output-format"] = "MEM"

    assert zonal.Run()

    out_ds = zonal.Output()

    results = [f for f in out_ds.GetLayer(0)]

    assert len(results) == 1

    assert results[0]["count"] == 2147483647.0
