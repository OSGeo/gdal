#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster update' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, osr

np = pytest.importorskip("numpy")
gdaltest.importorskip_gdal_array()


def test_gdalalg_raster_update_in_mem():

    out_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([1, 1, 0, -1, 0, -1])
    src_ds.GetRasterBand(1).Fill(1)

    assert gdal.Run("gdal", "raster", "update", input=src_ds, output=out_ds)

    np.testing.assert_array_equal(
        out_ds.ReadAsArray(), np.array([[0, 0, 0], [0, 1, 0], [0, 0, 0]])
    )


def test_gdalalg_raster_update_gtiff(tmp_vsimem):

    dst_filename = tmp_vsimem / "dst.tif"
    out_ds = gdal.GetDriverByName("GTIFF").Create(dst_filename, 3, 3)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    out_ds.Close()

    src_filename = tmp_vsimem / "src.tif"
    src_ds = gdal.GetDriverByName("GTIFF").Create(src_filename, 1, 1)
    src_ds.SetGeoTransform([1, 1, 0, -1, 0, -1])
    src_ds.GetRasterBand(1).Fill(1)
    src_ds.Close()

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.Run(
        "gdal",
        "raster",
        "update",
        input=src_filename,
        output=dst_filename,
        progress=my_progress,
        resampling="cubic",
        warp_option={"SKIP_NOSOURCE": "YES"},
        transform_option={"SRC_METHOD": "GEOTRANSFORM"},
        error_threshold=0,
    ) as alg:
        ret_ds = alg.Output()

        np.testing.assert_array_equal(
            ret_ds.ReadAsArray(), np.array([[0, 0, 0], [0, 1, 0], [0, 0, 0]])
        )

    assert tab_pct[0] == 1.0


def test_gdalalg_raster_update_src_is_same_as_dst_pointer():

    ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])

    with pytest.raises(
        Exception, match="Source and destination datasets must be different"
    ):
        gdal.Run("gdal", "raster", "update", input=ds, output=ds)


def test_gdalalg_raster_update_src_is_same_as_dst_filename(tmp_vsimem):

    filename = tmp_vsimem / "tmp.tif"
    ds = gdal.GetDriverByName("GTiff").Create(filename, 3, 3)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.Close()

    with pytest.raises(
        Exception, match="Source and destination datasets must be different"
    ):
        gdal.Run(
            "gdal",
            "raster",
            "update",
            input=gdal.Open(filename),
            output=gdal.Open(filename, gdal.GA_Update),
        )


def test_gdalalg_raster_update_dst_does_not_exist():

    ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])

    with pytest.raises(Exception, match="/i_do/not/exist"):
        gdal.Run("gdal", "raster", "update", input=ds, output="/i_do/not/exist")


def test_gdalalg_raster_update_geometry():

    out_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.GetRasterBand(1).Fill(1)

    assert gdal.Run(
        "gdal",
        "raster",
        "update",
        input=src_ds,
        output=out_ds,
        geometry="POLYGON ((1 -1,2 -1,2 -2,1 -2,1 -1))",
    )

    np.testing.assert_array_equal(
        out_ds.ReadAsArray(), np.array([[0, 0, 0], [0, 1, 0], [0, 0, 0]])
    )


def test_gdalalg_raster_update_geometry_invalid():

    out_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 3)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.GetRasterBand(1).Fill(1)

    with pytest.raises(
        Exception, match="Clipping geometry is neither a valid WKT or GeoJSON geometry"
    ):
        gdal.Run(
            "gdal", "raster", "update", input=src_ds, output=out_ds, geometry="invalid"
        )


@pytest.mark.parametrize("with_crs", [True, False])
def test_gdalalg_raster_update_refresh_overviews(with_crs):

    crs = osr.SpatialReference(epsg=32631)

    out_ds = gdal.GetDriverByName("MEM").Create("", 1024, 1024)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    out_ds.BuildOverviews("NEAR", [2])
    out_ds.GetRasterBand(1).GetOverview(0).Fill(127)
    if with_crs:
        out_ds.SetSpatialRef(crs)

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src_ds.SetGeoTransform([511, 1, 0, -511, 0, -1])
    src_ds.GetRasterBand(1).Fill(255)
    if with_crs:
        src_ds.SetSpatialRef(crs)

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    assert gdal.Run(
        "gdal",
        "raster",
        "update",
        input=src_ds,
        output=out_ds,
        resampling="cubic",
        progress=my_progress,
    )

    assert tab_pct[0] == 1.0

    np.testing.assert_array_equal(
        out_ds.ReadAsArray(511, 511, 2, 2), np.array([[255, 255], [255, 255]])
    )

    np.testing.assert_array_equal(
        out_ds.GetRasterBand(1).GetOverview(0).ReadAsArray(254, 254, 4, 4),
        np.array(
            [
                [127, 127, 127, 127],
                [127, 76, 76, 127],
                [127, 76, 76, 127],
                [127, 127, 127, 127],
            ]
        ),
    )


def test_gdalalg_raster_update_cannot_update_overviews():

    out_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    out_ds.SetSpatialRef(osr.SpatialReference(epsg=32631))
    out_ds.BuildOverviews("NEAR", [2])

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([1, 1, 0, -1, 0, -1])
    src_ds.SetSpatialRef(osr.SpatialReference("+proj=longlat +a=1"))

    with pytest.raises(Exception), gdaltest.error_raises(gdal.CE_Warning):
        gdal.Run("gdal", "raster", "update", input=src_ds, output=out_ds)


def test_gdalalg_raster_update_pipeline_intermediate_step(tmp_vsimem):

    out_ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "tmp.tif", 3, 3)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    out_ds.Close()

    src_ds = gdal.GetDriverByName("MEM").Create("src", 1, 1)
    src_ds.SetGeoTransform([1, 1, 0, -1, 0, -1])
    src_ds.GetRasterBand(1).Fill(1)

    with gdal.alg.raster.pipeline(
        input=src_ds, pipeline=f"read ! update {tmp_vsimem}/tmp.tif ! info --stats"
    ) as alg:
        j = alg.Output()
        assert j["bands"][0]["maximum"] == 1


def test_gdalalg_raster_update_pipeline_last_step():

    out_ds = gdal.GetDriverByName("MEM").Create("out", 3, 3)
    out_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])

    src_ds = gdal.GetDriverByName("MEM").Create("src", 1, 1)
    src_ds.SetGeoTransform([1, 1, 0, -1, 0, -1])
    src_ds.GetRasterBand(1).Fill(1)

    assert gdal.alg.raster.pipeline(
        input=src_ds, output=out_ds, pipeline="read ! update"
    )

    np.testing.assert_array_equal(
        out_ds.ReadAsArray(), np.array([[0, 0, 0], [0, 1, 0], [0, 0, 0]])
    )
