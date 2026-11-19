#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster pansharpen' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal

expected_cs = (
    [4735, 10000, 9742],
    [4731, 9991, 9734],  # s390x or graviton2
    [4726, 10004, 9727],  # ICC 2004.0.2 in -O3
)

###############################################################################


@pytest.fixture(scope="module")
def small_world_pan_tif(tmp_path_factory):

    small_world_pan_tif = str(tmp_path_factory.mktemp("tmp") / "small_world_pan.tif")

    with gdal.Open("../gdrivers/data/small_world.tif") as src_ds:
        src_data = src_ds.GetRasterBand(1).ReadRaster()
        gt = src_ds.GetGeoTransform()
        wkt = src_ds.GetProjectionRef()

    with gdal.GetDriverByName("GTiff").Create(small_world_pan_tif, 800, 400) as pan_ds:
        gt = [gt[i] for i in range(len(gt))]
        gt[1] *= 0.5
        gt[5] *= 0.5
        pan_ds.SetGeoTransform(gt)
        pan_ds.SetProjection(wkt)
        pan_ds.GetRasterBand(1).WriteRaster(0, 0, 800, 400, src_data, 400, 200)

    return small_world_pan_tif


###############################################################################
# Simple test


@pytest.mark.parametrize(
    "options",
    [
        {"weights": [1.0 / 3, 1.0 / 3, 1.0 / 3]},
        {"bit-depth": 8},
    ],
)
def test_gdalalg_raster_pansharpen_basic(small_world_pan_tif, options):

    with gdal.Run(
        "raster",
        "pansharpen",
        input=small_world_pan_tif,
        spectral="../gdrivers/data/small_world.tif",
        output="",
        format="VRT",
        **options,
    ) as alg:
        ds = alg.Output()
        cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]

        assert cs in expected_cs


###############################################################################
# As pipeline


def test_gdalalg_raster_pansharpen_pipeline(small_world_pan_tif):

    with gdal.Run(
        "raster",
        "pipeline",
        pipeline=f"read {small_world_pan_tif} ! pansharpen ../gdrivers/data/small_world.tif ! write streamed_data --of=stream",
    ) as alg:
        ds = alg.Output()
        cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]

        assert cs in expected_cs


###############################################################################
# Datasets per reference


def test_gdalalg_raster_pansharpen_ds_per_reference(small_world_pan_tif):

    with gdal.Run(
        "raster",
        "pansharpen",
        input=gdal.Open(small_world_pan_tif),
        spectral=gdal.Open("../gdrivers/data/small_world.tif"),
        output="",
        format="VRT",
    ) as alg:
        ds = alg.Output()
        cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]

        assert cs in expected_cs


###############################################################################
# Select one band of multispectral datasets


def test_gdalalg_raster_pansharpen_spectral_with_band_suffix(small_world_pan_tif):

    with gdal.Run(
        "raster",
        "pansharpen",
        input=small_world_pan_tif,
        spectral=[
            "../gdrivers/data/small_world.tif,band=1",
            "../gdrivers/data/small_world.tif,band=2",
            "../gdrivers/data/small_world.tif,band=3",
        ],
        output="",
        format="VRT",
    ) as alg:
        ds = alg.Output()
        cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]

        assert cs in expected_cs


###############################################################################
# Full options


@pytest.mark.parametrize(
    "options",
    [
        {"weights": [1.0 / 3, 1.0 / 3, 0]},
        {"nodata": 1},
    ],
)
def test_gdalalg_raster_pansharpen_full_options(small_world_pan_tif, options):
    with gdal.Run(
        "raster",
        "pansharpen",
        input=small_world_pan_tif,
        spectral=gdal.Open("../gdrivers/data/small_world.tif"),
        output="",
        format="VRT",
        **options,
    ) as alg:
        ds = alg.Output()
        cs = [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)]
        assert cs not in expected_cs


###############################################################################


def test_gdalalg_raster_pansharpen_errors():

    with pytest.raises(
        Exception, match="Input panchromatic dataset must have a single band"
    ):
        gdal.Run(
            "raster",
            "pansharpen",
            input="../gdrivers/data/small_world.tif",
            spectral="../gdrivers/data/small_world.tif",
            output="",
            format="VRT",
        )

    with pytest.raises(Exception, match="/i/do_not/exist.xxx"):
        gdal.Run(
            "raster",
            "pansharpen",
            input="../gcore/data/byte.tif",
            spectral="/i/do_not/exist.xxx",
            output="",
            format="VRT",
        )

    with pytest.raises(Exception, match="Illegal band"):
        gdal.Run(
            "raster",
            "pansharpen",
            input="../gcore/data/byte.tif",
            spectral=[
                "../gdrivers/data/small_world.tif,band=4",
            ],
            output="",
            format="VRT",
        )

    with pytest.raises(
        Exception, match="Panchromatic band has no associated geotransform"
    ):
        gdal.Run(
            "raster",
            "pansharpen",
            input=gdal.GetDriverByName("MEM").Create("", 1, 1),
            spectral="../gdrivers/data/small_world.tif",
            output="",
            format="VRT",
        )


###############################################################################


def test_gdalalg_raster_pansharpen_not_intersecting(tmp_vsimem):

    gdal.alg.raster.create(
        output=tmp_vsimem / "pan.tif", size=[1, 1], bbox=[-11, -101, -10, -100]
    )
    gdal.alg.raster.create(
        output=tmp_vsimem / "ms.tif", size=[1, 1], bbox=[10, 20, 11, 21]
    )

    gdal.alg.raster.pansharpen(
        input=tmp_vsimem / "pan.tif",
        spectral=tmp_vsimem / "ms.tif",
        output=tmp_vsimem / "out.vrt",
    )

    ds = gdal.Open(tmp_vsimem / "out.vrt")
    assert ds.RasterXSize == 22
    assert ds.RasterYSize == 122
    assert ds.GetFileList() == [
        str(tmp_vsimem / "out.vrt"),
        str(tmp_vsimem / "pan.tif"),
        str(tmp_vsimem / "ms.tif"),
    ]


###############################################################################


def test_gdalalg_raster_pansharpen_not_intersecting_int_overflow(tmp_vsimem):

    billion = 1e9
    gdal.alg.raster.create(
        output=tmp_vsimem / "pan.tif",
        size=[1, 1],
        bbox=[-10 * billion - 1, -10 * billion - 1, -10 * billion, -10 * billion],
    )
    gdal.alg.raster.create(output=tmp_vsimem / "ms.tif", size=[1, 1], bbox=[0, 0, 1, 1])

    with pytest.raises(Exception, match="Datasets are too disjoint"):
        gdal.alg.raster.pansharpen(
            input=tmp_vsimem / "pan.tif",
            spectral=tmp_vsimem / "ms.tif",
            output="",
            output_format="VRT",
        )
