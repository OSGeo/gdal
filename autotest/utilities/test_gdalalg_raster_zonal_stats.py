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
    ) >= (3, 15)


@pytest.fixture(params=["default", "all_touched", "fractional"])
def pixels(request):
    if request.param == "fractional" and not have_fractional_pixels():
        pytest.skip("--pixels fractional requires GEOS >= 3.14")

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


@pytest.mark.parametrize("band", (1, 2, [3, 1]))
def test_gdalalg_raster_zonal_stats_polygon_zones_basic(
    zonal, strategy, pixels, band, tmp_vsimem
):

    zonal["input"] = "../gcore/data/gtiff/rgbsmall_NONE_tiled.tif"
    zonal["zones"] = gdaltest.wkt_ds(
        [
            "POLYGON((-44.8151 -22.9973, -44.7357 -22.9979,-44.7755 -22.9563,-44.8151 -22.9973))",
            "POLYGON((-44.7671 -23.0347,-44.7683 -23.0678,-44.7183 -23.0686,-44.7191 -23.0339,-44.7671 -23.0347))",
        ]
    )
    zonal["output"] = tmp_vsimem / "out.csv"
    zonal["strategy"] = strategy
    zonal["pixels"] = pixels
    zonal["stat"] = ["sum", "mean"]
    zonal["band"] = band

    assert zonal.Run()

    out_ds = zonal.Output()

    results = [f for f in out_ds.GetLayer(0)]

    assert len(results) == 2

    if band == 1:
        if pixels == "fractional":
            assert results[0]["sum"] == pytest.approx(7130.21435281425)
            assert results[1]["sum"] == pytest.approx(12886.7218339441)
        elif pixels == "all_touched":
            assert results[0]["sum"] == 8251
            assert results[1]["sum"] == 15131
        else:
            assert results[0]["sum"] == 7148
            assert results[1]["sum"] == 12497
    elif band == 2:
        if pixels == "fractional":
            assert results[0]["sum"] == pytest.approx(9743.76203341247)
            assert results[1]["sum"] == pytest.approx(18396.5491179946)
        elif pixels == "all_touched":
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
        elif pixels == "all_touched":
            assert results[0]["sum_band_1"] == 8251
            assert results[1]["sum_band_1"] == 15131
        else:
            assert results[0]["sum_band_1"] == 7148
            assert results[1]["sum_band_1"] == 12497


@pytest.mark.parametrize("pixels", ["all_touched", "fractional"], indirect=True)
def test_gdalalg_raster_zonal_stats_polygon_zones_weighted(
    zonal, strategy, pixels, tmp_vsimem
):

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
    zonal["output"] = tmp_vsimem / "out.csv"
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

    if pixels == "all_touched":
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
def test_gdalalg_raster_zonal_stats_raster_zones(zonal, tmp_vsimem, band):

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
    zonal["output"] = tmp_vsimem / "out.csv"
    zonal["stat"] = ["sum", "mean", "weighted_mean"]
    zonal["band"] = band
    zonal["memory"] = "8k"  # force iteration over blocks

    assert zonal.Run()

    out_ds = zonal.Output()

    results = [f for f in out_ds.GetLayer(0)]

    assert len(results) == len(np.unique(zones))

    if type(band) is int:
        band = [band]

    for b in band:
        src_values = src_ds.GetRasterBand(b).ReadAsArray()

        sum_field = "sum" if len(band) == 1 else f"sum_band_{b}"
        weighted_mean_field = (
            "weighted_mean" if len(band) == 1 else f"weighted_mean_band_{b}"
        )

        for i, value in enumerate(np.unique(zones)):
            assert results[i]["value"] == value, f"i={i}"
            assert results[i][sum_field] == src_values[zones == value].sum(), f"i={i}"
            assert results[i][weighted_mean_field] == pytest.approx(
                np.average(src_values[zones == value], weights=weights[zones == value])
            ), f"i={i}"


@pytest.mark.skipif(not have_fractional_pixels(), reason="requires GEOS >= 3.14")
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
        "median",
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
def test_gdalalg_raster_zonal_stats_polygon_zones_all_stats(
    zonal, strategy, stat, tmp_vsimem
):

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
    zonal["output"] = tmp_vsimem / "out.csv"
    zonal["pixels"] = "fractional"
    zonal["strategy"] = strategy
    zonal[
        "stat"
    ] = stat  # process stats individually to ensure RasterStatsOptions set correctly

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
    elif stat == "median":
        assert f["median"] == pytest.approx(4.2308, abs=1e-4)
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


def test_gdalalg_raster_zonal_stats_non_polygon_geometry(zonal, strategy, tmp_vsimem):

    zonal["input"] = "../gcore/data/byte.tif"
    zonal["zones"] = gdaltest.wkt_ds(["LINESTRING (3 3, 8 8)"])
    zonal["output"] = tmp_vsimem / "out.csv"
    zonal["strategy"] = strategy
    zonal["stat"] = "sum"

    with pytest.raises(Exception, match="Non-polygonal geometry"):
        zonal.Run()


def test_gdalalg_raster_zonal_stats_polygon_zones_include_fields(
    zonal, strategy, tmp_vsimem, polyrast
):

    zonal["input"] = polyrast
    zonal["output"] = tmp_vsimem / "out.csv"
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


def test_gdalalg_raster_zonal_stats_raster_zones_include_fields(zonal, tmp_vsimem):

    zonal["input"] = "../gcore/data/byte.tif"
    zonal["zones"] = "../gcore/data/byte.tif"
    zonal["output"] = tmp_vsimem / "out.csv"
    zonal["stat"] = "sum"
    zonal["include-field"] = "id"

    with pytest.raises(Exception, match="Cannot include fields"):
        zonal.Run()


def test_gdalalg_raster_zonal_stats_raster_zones_invalid_band(zonal, tmp_vsimem):

    zonal["input"] = "../gcore/data/byte.tif"
    zonal["zones"] = "../gcore/data/byte.tif"
    zonal["zones-band"] = 2
    zonal["output"] = tmp_vsimem / "out.csv"
    zonal["stat"] = "sum"

    with pytest.raises(Exception, match="Specified zones band 2 not found"):
        zonal.Run()


def test_gdalalg_raster_zonal_stats_polygon_zones_invalid_band(
    zonal, tmp_vsimem, polyrast
):

    zonal["input"] = polyrast
    zonal["zones"] = "../ogr/data/poly.shp"
    zonal["stat"] = "sum"
    zonal["output"] = tmp_vsimem / "out.csv"
    zonal["band"] = 2

    with pytest.raises(Exception, match="Value of 'band' should be"):
        zonal.Run()


def test_gdalalg_raster_zonal_stats_polygon_zones_invalid_layer(
    zonal, tmp_vsimem, polyrast
):

    zonal["input"] = polyrast
    zonal["zones"] = "../ogr/data/poly.shp"
    zonal["zones-layer"] = "does_not_exist"
    zonal["stat"] = "sum"
    zonal["output"] = tmp_vsimem / "out.csv"

    with pytest.raises(Exception, match="Specified zones layer .* not found"):
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
    zonal, raster_srs, weights_srs, zones_srs, zones_typ, warn, tmp_vsimem
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
    zonal["output"] = tmp_vsimem / "out.csv"

    err_type = gdal.CE_Warning if warn else gdal.CE_None

    with gdaltest.error_raised(err_type, match="do not have the same SRS"):
        assert zonal.Run()


def test_gdalalg_raster_zonal_stats_polygon_zone_outside_raster(
    zonal, tmp_vsimem, strategy
):
    zonal["input"] = "../gcore/data/byte.tif"
    zonal["zones"] = gdaltest.wkt_ds(["POLYGON ((2 2, 8 2, 8 8, 2 2))"])
    zonal["strategy"] = strategy
    zonal["stat"] = ["sum", "mode"]
    zonal["output"] = tmp_vsimem / "out.csv"

    assert zonal.Run()

    out_ds = zonal.Output()

    results = [f for f in out_ds.GetLayer(0)]

    assert len(results) == 1
    assert results[0]["sum"] == 0
    assert results[0]["mode"] is None


@pytest.mark.parametrize("src_nodata", (None, 99))
def test_gdalalg_raster_zonal_stats_raster_values_partially_outside(
    zonal, src_nodata, tmp_vsimem
):

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

    # FIXME: assumes default value is zero but should it be NaN ?
    values_adj = np.zeros((10, 10))
    values_adj[2:, 2:] = values[:-2, :-2]

    zonal["input"] = src_ds
    zonal["zones"] = zones_ds
    zonal["stat"] = ["count", "sum"]
    zonal["output"] = tmp_vsimem / "out.csv"

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
def test_gdalalg_raster_zonal_stats_raster_zones_entirely_outside(
    zonal, src_nodata, tmp_vsimem
):
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
    zonal["output"] = tmp_vsimem / "out.csv"

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


def test_gdalalg_raster_zonal_stats_raster_weights_partially_outside(zonal, tmp_vsimem):

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

    # FIXME: assumes default weight is zero but should it be NaN ?
    weights_adj = np.zeros((10, 10))
    weights_adj[:-2, 2:] = weights[2:, :-2]

    zonal["input"] = src_ds
    zonal["zones"] = zones_ds
    zonal["weights"] = weights_ds
    zonal["stat"] = "weighted_sum"
    zonal["output"] = tmp_vsimem / "out.csv"

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
    zonal, strategy, tmp_vsimem, polyrast
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
    zonal["output"] = tmp_vsimem / "out.csv"
    zonal["stat"] = ["weighted_mean", "weights"]

    with gdaltest.error_raised(gdal.CE_Warning, match="Resampled weight"):
        assert zonal.Run()

    out_ds = zonal.Output()
    results = out_ds.GetLayer(0).GetNextFeature()

    weights_agg = np.array([[3.5, 5.5], [11.5, 13.5]])

    assert results["weighted_mean"] == pytest.approx(
        np.average(values.flatten(), weights=weights_agg.flatten())
    )
