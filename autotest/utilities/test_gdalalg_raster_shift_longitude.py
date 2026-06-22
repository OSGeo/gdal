#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster as-features' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2026, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

np = pytest.importorskip("numpy")
gdaltest.importorskip_gdal_array()


@pytest.fixture()
def alg():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("shift-longitude")


def create_ds(
    *,
    xmin,
    xmax,
    ymin=-90,
    ymax=90,
    dx=1,
    dy=1,
    dt=gdal.GDT_Int32,
    bands=1,
    nodata=None,
):

    nx = round((xmax - xmin) / dx)
    ny = round((ymax - ymin) / dy)

    data = np.repeat(np.arange(xmin, xmax).reshape(1, nx), ny, axis=0)

    ds = gdal.GetDriverByName("MEM").Create("", nx, ny, bands, eType=dt)
    ds.SetGeoTransform((xmin, dx, 0, ymax, 0, -dy))
    for i in range(bands):
        ds.GetRasterBand(i + 1).WriteArray(data)
        if nodata is not None:
            ds.GetRasterBand(i + 1).SetNoDataValue(nodata)

    return ds


def test_gdalalg_raster_shift_longitude_1(alg):
    # Shift (0, 360) to (-180, 180)

    src_ds = create_ds(xmin=0, xmax=360)
    src_data = src_ds.ReadAsArray()

    alg["input"] = src_ds
    alg["min-x"] = -180
    alg["max-x"] = 180
    alg["output-format"] = "MEM"

    assert alg.Run()

    dst_ds = alg.Output()

    assert dst_ds.GetGeoTransform() == (-180, 1, 0, 90, 0, -1)
    assert dst_ds.RasterXSize == 360
    assert dst_ds.RasterYSize == 180
    assert dst_ds.GetRasterBand(1).DataType == gdal.GDT_Int32

    dst_data = dst_ds.ReadAsArray()
    np.testing.assert_array_equal(
        dst_data, np.concatenate([src_data[:, 180:], src_data[:, :180]], axis=1)
    )


def test_gdalalg_raster_shift_longitude_2(alg):
    # Shift (-180, 180) to (0, 360)

    src_ds = create_ds(xmin=-180, xmax=180)
    src_data = src_ds.ReadAsArray()

    alg["input"] = src_ds
    alg["min-x"] = 0
    alg["max-x"] = 360
    alg["output-format"] = "MEM"

    assert alg.Run()

    dst_ds = alg.Output()

    assert dst_ds.GetGeoTransform() == (0, 1, 0, 90, 0, -1)
    assert dst_ds.RasterXSize == 360
    assert dst_ds.RasterYSize == 180
    assert dst_ds.GetRasterBand(1).DataType == gdal.GDT_Int32

    dst_data = dst_ds.ReadAsArray()
    np.testing.assert_array_equal(
        dst_data, np.concatenate([src_data[:, 180:], src_data[:, :180]], axis=1)
    )


def test_gdalalg_raster_shift_longitude_3(alg):
    # Attempt to shift (-0.5, 355.5) to (-180, 180)
    # Output longitude range ends up being (-180.5, 180.5). One column is duplicated.

    src_ds = create_ds(xmin=-0.5, xmax=359.5, dt=gdal.GDT_Float32)
    src_data = src_ds.ReadAsArray()

    alg["input"] = src_ds
    alg["min-x"] = -180
    alg["max-x"] = 180
    alg["output-format"] = "MEM"

    assert alg.Run()

    dst_ds = alg.Output()

    assert dst_ds.GetGeoTransform() == (-180.5, 1, 0, 90, 0, -1)
    assert dst_ds.RasterXSize == 361
    assert dst_ds.RasterYSize == 180
    assert dst_ds.GetRasterBand(1).DataType == gdal.GDT_Float32

    dst_data = dst_ds.ReadAsArray()
    np.testing.assert_array_equal(
        dst_data,
        np.concatenate(
            [src_data[:, 180:], src_data[:, :180], src_data[:, [180]]], axis=1
        ),
    )


def test_gdalalg_raster_shift_longitude_4(alg):
    # Repeat inputs

    src_ds = create_ds(xmin=-180, xmax=180)
    src_data = src_ds.ReadAsArray()

    alg["input"] = src_ds
    alg["min-x"] = -360
    alg["max-x"] = 920
    alg["output-format"] = "MEM"

    assert alg.Run()

    dst_ds = alg.Output()

    assert dst_ds.GetGeoTransform() == (-360, 1, 0, 90, 0, -1)
    assert dst_ds.RasterXSize == 1280
    assert dst_ds.RasterYSize == 180

    dst_data = dst_ds.ReadAsArray()
    np.testing.assert_array_equal(
        dst_data,
        np.concatenate(
            [src_data[:, 180:], src_data, src_data, src_data, src_data[:, :20]], axis=1
        ),
    )


def test_gdalalg_raster_shift_longitude_metadata_copied(alg):

    band_md = {"item_1": "3", "item_2": "4"}
    # band_ct = gdal.ColorTable()
    # band_ct.SetColorEntry(0, (255, 0 0))

    src_ds = create_ds(xmin=-180, xmax=180, dt=gdal.GDT_Float32)
    src_ds.GetRasterBand(1).SetMetadata(band_md)
    src_ds.GetRasterBand(1).SetNoDataValue(13)
    src_ds.GetRasterBand(1).SetScale(0.5)
    src_ds.GetRasterBand(1).SetOffset(-2)

    alg["input"] = src_ds
    alg["min-x"] = 0
    alg["max-x"] = 360
    alg["output-format"] = "MEM"

    assert alg.Run()

    dst_ds = alg.Output()

    assert dst_ds.GetRasterBand(1).GetMetadata() == band_md
    assert dst_ds.GetRasterBand(1).GetNoDataValue() == 13
    assert dst_ds.GetRasterBand(1).GetScale() == 0.5
    assert dst_ds.GetRasterBand(1).GetOffset() == -2


def test_gdalalg_raster_shift_longitude_nodata_preserved(alg):

    src_ds = gdal.GetDriverByName("MEM").Create("", 4, 1, eType=gdal.GDT_Byte)
    src_ds.SetGeoTransform((-180, 90, 0, 90, 0, -180))
    src_ds.WriteArray(np.array([[1, 2, 3, 4]]))
    src_ds.GetRasterBand(1).SetNoDataValue(3)

    alg["input"] = src_ds
    alg["min-x"] = 0
    alg["max-x"] = 360
    alg["output-nodata"] = 255
    alg["output-format"] = "MEM"

    assert alg.Run()

    dst_ds = alg.Output()
    dst_data = dst_ds.ReadAsArray()

    np.testing.assert_array_equal(dst_data, np.array([[255, 4, 1, 2]]))


@pytest.mark.parametrize("xmin,xmax", ((0, 0), (180, -180)))
def test_gdalalg_raster_shift_longitude_invalid_range(alg, xmin, xmax):

    src_ds = create_ds(xmin=-180, xmax=180)

    alg["input"] = src_ds
    alg["min-x"] = 0
    alg["max-x"] = 0
    alg["output-format"] = "MEM"

    with pytest.raises(Exception, match="--max-x must be greater than --min-x"):
        alg.Run()


@pytest.mark.parametrize(
    "src_range, dst_range, missing_range",
    [
        pytest.param([-180, 0], [0, 180], [0, 180], id="1"),
        pytest.param([-180, -10], [-20, 20], [-10, 20], id="2"),
        pytest.param([0, 180], [-180, -90], [-180, -90], id="3"),
        pytest.param([10, 180], [-20, 20], [-20, 10], id="4"),
    ],
)
def test_gdalalg_raster_shift_longitude_missing_data(
    alg, src_range, dst_range, missing_range
):

    src_ds = create_ds(xmin=src_range[0], xmax=src_range[1])
    dx = src_ds.GetGeoTransform()[1]

    alg["input"] = src_ds
    alg["min-x"] = dst_range[0]
    alg["max-x"] = dst_range[1]
    alg["output-nodata"] = -9999
    alg["output-format"] = "MEM"

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match=f"No source data available for output longitude range {missing_range[0]} to {missing_range[1]}",
    ):
        alg.Run()

    ds = alg.Output()

    assert ds.RasterXSize == (dst_range[1] - dst_range[0]) / dx
    assert ds.RasterYSize == src_ds.RasterYSize

    inv_gt = gdal.InvGeoTransform(ds.GetGeoTransform())
    col0 = int(round(gdal.ApplyGeoTransform(inv_gt, missing_range[0], 0)[0]))
    col1 = int(round(gdal.ApplyGeoTransform(inv_gt, missing_range[1], 0)[0]))

    data = ds.ReadAsMaskedArray()

    expected_mask = np.zeros(data.shape, dtype=bool)
    expected_mask[:, col0:col1] = True

    # all pixels outside available range are NoData
    assert np.all(data[expected_mask].mask)

    # all pixels inside available range are defined
    assert not np.any(data[~expected_mask].mask)


def test_gdalalg_raster_shift_longitude_rotated_geotransform(alg):

    src_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    src_ds.SetGeoTransform((1, 1, 1, 1, 1, -1))

    alg["input"] = src_ds
    alg["min-x"] = 2
    alg["max-x"] = 3
    alg["output-format"] = "MEM"

    with pytest.raises(Exception, match="geotransform cannot have a rotation"):
        alg.Run()


def test_gdalalg_raster_shift_longitude_no_geotransform(alg):

    src_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)

    alg["input"] = src_ds
    alg["min-x"] = 2
    alg["max-x"] = 3
    alg["output-format"] = "MEM"

    with pytest.raises(Exception, match="does not have a geotransform"):
        alg.Run()


@pytest.mark.parametrize("nodata", (-9, 223.4))
def test_gdalalg_raster_shift_longitude_invalid_nodata(alg, nodata):

    src_ds = create_ds(xmin=0, xmax=360, dt=gdal.GDT_Byte)

    alg["input"] = src_ds
    alg["min-x"] = 2
    alg["max-x"] = 3
    alg["output-nodata"] = nodata
    alg["output-format"] = "MEM"

    with pytest.raises(Exception, match="Invalid NoData value"):
        alg.Run()
