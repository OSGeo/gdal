#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster overview refresh' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import os

import gdaltest
import pytest

from osgeo import gdal, ogr

###############################################################################
# Test no option


def test_gdalalg_raster_overview_refresh_no_option():

    ds = gdal.Translate(
        "", "../gcore/data/byte.tif", options="-f MEM -outsize 512 512 -r cubic"
    )
    ds.BuildOverviews("BILINEAR", [2])
    ovr_data_ori = array.array("B", ds.GetRasterBand(1).GetOverview(0).ReadRaster())
    ds.GetRasterBand(1).GetOverview(0).Fill(0)

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = ds
    alg["resampling"] = "bilinear"
    assert alg.Run()

    ovr_band = ds.GetRasterBand(1).GetOverview(0)
    ovr_data_refreshed = array.array("B", ovr_band.ReadRaster())
    assert ovr_data_refreshed == ovr_data_ori


###############################################################################
# Test --bbox


def test_gdalalg_raster_overview_refresh_bbox():

    ds = gdal.Translate(
        "", "../gcore/data/byte.tif", options="-f MEM -outsize 512 512 -r cubic"
    )
    ds.BuildOverviews("BILINEAR", [2, 4])
    ovr_data_ori = array.array("B", ds.GetRasterBand(1).GetOverview(0).ReadRaster())
    ds.GetRasterBand(1).Fill(0)
    gt = ds.GetGeoTransform()

    x = 10
    y = 20
    width = 30
    height = 40
    ulx = gt[0] + gt[1] * x
    uly = gt[3] + gt[5] * y
    lrx = gt[0] + gt[1] * (x + width)
    lry = gt[3] + gt[5] * (y + height)

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = ds
    alg["bbox"] = [ulx, lry, lrx, uly]
    assert alg.Run()

    ovr_band = ds.GetRasterBand(1).GetOverview(0)
    ovr_data_refreshed = array.array("B", ovr_band.ReadRaster())
    # Test that data is zero only in the refreshed area, and unchanged
    # in other areas
    for j in range(height // 2):
        for i in range(width // 2):
            idx = (y // 2 + j) * ovr_band.XSize + (x // 2 + i)
            assert ovr_data_refreshed[idx] == 0
            ovr_data_refreshed[idx] = ovr_data_ori[idx]
    assert ovr_data_refreshed == ovr_data_ori


###############################################################################
# Test --bbox on dataset without gt


def test_gdalalg_raster_overview_refresh_bbox_no_gt():

    ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    ds.BuildOverviews("NEAR", [2])

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = ds
    alg["bbox"] = [0, 0, 1, 1]
    with pytest.raises(Exception, match="Dataset has no geotransform"):
        alg.Run()


###############################################################################
# Test --bbox on dataset with non-invertible gt


def test_gdalalg_raster_overview_refresh_bbox_no_invertible_gt():

    ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    ds.SetGeoTransform([0, 0, 0, 0, 0, 0])
    ds.BuildOverviews("NEAR", [2])

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = ds
    alg["bbox"] = [0, 0, 1, 1]
    with pytest.raises(Exception, match="Cannot invert geotransform"):
        alg.Run()


###############################################################################
# Test --like


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
@pytest.mark.parametrize("with_progress", [True, False])
def test_gdalalg_raster_overview_refresh_like(tmp_path, with_progress):

    left_tif = str(tmp_path / "left.tif")
    right_tif = str(tmp_path / "right.tif")
    tmp_vrt = str(tmp_path / "tmp.vrt")

    gdal.Translate(left_tif, "../gcore/data/byte.tif", options="-srcwin 0 0 10 20")
    gdal.Translate(right_tif, "../gcore/data/byte.tif", options="-srcwin 10 0 10 20")
    ds = gdal.BuildVRT(tmp_vrt, [left_tif, right_tif])
    ds.BuildOverviews("BILINEAR", [2])
    ovr_data_ori = array.array("B", ds.GetRasterBand(1).GetOverview(0).ReadRaster())
    ds = None

    ds = gdal.Open(left_tif, gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = gdal.Open(right_tif, gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = tmp_vrt
    alg["resampling"] = "bilinear"
    alg["like"] = right_tif

    if with_progress:
        tab_pct = [0]

        def my_progress(pct, msg, user_data):
            assert pct >= tab_pct[0]
            tab_pct[0] = pct
            return True

        assert alg.Run(my_progress)
        assert tab_pct[0] == 1.0
    else:
        assert alg.Run()
    assert alg.Finalize()

    ds = gdal.Open(tmp_vrt)
    ovr_band = ds.GetRasterBand(1).GetOverview(0)
    ovr_data_refreshed = array.array("B", ovr_band.ReadRaster())
    # Test that data is zero only in the refreshed area, and unchanged
    # in other areas
    for j in range(10):
        for i in range(5):
            idx = (j) * ovr_band.XSize + (i + 5)
            assert ovr_data_refreshed[idx] == 0
            ovr_data_refreshed[idx] = ovr_data_ori[idx]
    assert ovr_data_refreshed == ovr_data_ori
    ds = None


###############################################################################
# Test --like on dataset without gt


def test_gdalalg_raster_overview_refresh_like_no_gt():

    ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    ds.BuildOverviews("NEAR", [2])

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = ds
    alg["like"] = "/i_do/not/exist"
    with pytest.raises(Exception, match="Dataset has no geotransform"):
        alg.Run()


###############################################################################
# Test --like on dataset with non-invertible gt


def test_gdalalg_raster_overview_refresh_like_no_invertible_gt():

    ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    ds.SetGeoTransform([0, 0, 0, 0, 0, 0])
    ds.BuildOverviews("NEAR", [2])

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = ds
    alg["like"] = "/i_do/not/exist"
    with pytest.raises(Exception, match="Cannot invert geotransform"):
        alg.Run()


###############################################################################
# Test --like on dataset with non valid filename


def test_gdalalg_raster_overview_refresh_like_invalid_source():

    ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.BuildOverviews("NEAR", [2])

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = ds
    alg["like"] = "/i_do/not/exist"
    with pytest.raises(Exception, match="/i_do/not/exist"):
        alg.Run()


###############################################################################
# Test --like on source without GT


def test_gdalalg_raster_overview_refresh_like_source_has_no_gt(tmp_vsimem):

    gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "source.tif", 1, 1)

    ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.BuildOverviews("NEAR", [2])

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = ds
    alg["like"] = tmp_vsimem / "source.tif"
    with pytest.raises(Exception, match="Source dataset has no geotransform"):
        alg.Run()


###############################################################################
# Test --use-source-timestamp on a VRT


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalalg_raster_overview_refresh_source_timestamp_vrt(tmp_path):

    left_tif = str(tmp_path / "left.tif")
    right_tif = str(tmp_path / "right.tif")
    tmp_vrt = str(tmp_path / "tmp.vrt")

    gdal.Translate(left_tif, "../gcore/data/byte.tif", options="-srcwin 0 0 10 20")
    gdal.Translate(right_tif, "../gcore/data/byte.tif", options="-srcwin 10 0 10 20")
    ds = gdal.BuildVRT(tmp_vrt, [left_tif, right_tif])
    ds.BuildOverviews("BILINEAR", [2, 4])
    ovr_data_ori = array.array("B", ds.GetRasterBand(1).GetOverview(0).ReadRaster())
    ds = None

    ds = gdal.Open(left_tif, gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None

    # Make sure timestamp of left.tif is before tmp.vrt.ovr
    timestamp = int(os.stat(tmp_vrt + ".ovr").st_mtime) - 10
    os.utime(left_tif, times=(timestamp, timestamp))

    ds = gdal.Open(right_tif, gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None

    # Make sure timestamp of right.tif is after tmp.vrt.ovr
    timestamp = int(os.stat(tmp_vrt + ".ovr").st_mtime) + 10
    os.utime(right_tif, times=(timestamp, timestamp))

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = tmp_vrt
    alg["resampling"] = "bilinear"
    alg["use-source-timestamp"] = True
    assert alg.Run(my_progress)
    assert alg.Finalize()

    assert tab_pct[0] == 1

    ds = gdal.Open(tmp_vrt)
    ovr_band = ds.GetRasterBand(1).GetOverview(0)
    ovr_data_refreshed = array.array("B", ovr_band.ReadRaster())
    # Test that data is zero only in the refreshed area, and unchanged
    # in other areas
    for j in range(10):
        for i in range(5):
            idx = (j) * ovr_band.XSize + (i + 5)
            assert ovr_data_refreshed[idx] == 0
            ovr_data_refreshed[idx] = ovr_data_ori[idx]
    assert ovr_data_refreshed == ovr_data_ori


###############################################################################
# Test --use-source-timestamp on a GTI


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_driver("GTI")
def test_gdalalg_raster_overview_refresh_source_timestamp_gti(tmp_path):

    gti_drv = gdal.GetDriverByName("GTI")
    if gti_drv.GetMetadataItem("IS_PLUGIN"):
        pytest.skip("Test skipped because GTI driver as a plugin")

    left_tif = str(tmp_path / "left.tif")
    right_tif = str(tmp_path / "right.tif")

    gdal.Translate(left_tif, "../gcore/data/byte.tif", options="-srcwin 0 0 10 20")
    gdal.Translate(right_tif, "../gcore/data/byte.tif", options="-srcwin 10 0 10 20")

    source_ds = [gdal.Open(left_tif), gdal.Open(right_tif)]
    tmp_gti = str(tmp_path / "test.gti.gpkg")
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(tmp_gti)
    lyr = index_ds.CreateLayer("index", srs=source_ds[0].GetSpatialRef())
    lyr.CreateField(ogr.FieldDefn("location"))
    for i, src_ds in enumerate(source_ds):
        f = ogr.Feature(lyr.GetLayerDefn())
        src_gt = src_ds.GetGeoTransform()
        minx = src_gt[0]
        maxx = minx + src_ds.RasterXSize * src_gt[1]
        maxy = src_gt[3]
        miny = maxy + src_ds.RasterYSize * src_gt[5]
        f["location"] = src_ds.GetDescription()
        f.SetGeometry(
            ogr.CreateGeometryFromWkt(
                f"POLYGON(({minx} {miny},{minx} {maxy},{maxx} {maxy},{maxx} {miny},{minx} {miny}))"
            )
        )
        lyr.CreateFeature(f)
    index_ds.Close()
    del source_ds

    ds = gdal.Open(tmp_gti, gdal.GA_Update)
    ds.BuildOverviews("BILINEAR", [2, 4])
    ovr_data_ori = array.array("B", ds.GetRasterBand(1).GetOverview(0).ReadRaster())
    ds = None

    ds = gdal.Open(left_tif, gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None

    # Make sure timestamp of left.tif is before tmp.vrt.ovr
    timestamp = int(os.stat(tmp_gti + ".ovr").st_mtime) - 10
    os.utime(left_tif, times=(timestamp, timestamp))

    ds = gdal.Open(right_tif, gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None

    # Make sure timestamp of right.tif is after tmp.vrt.ovr
    timestamp = int(os.stat(tmp_gti + ".ovr").st_mtime) + 10
    os.utime(right_tif, times=(timestamp, timestamp))

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = tmp_gti
    alg["resampling"] = "bilinear"
    alg["use-source-timestamp"] = True
    assert alg.Run()
    assert alg.Finalize()

    ds = gdal.Open(tmp_gti)
    ovr_band = ds.GetRasterBand(1).GetOverview(0)
    ovr_data_refreshed = array.array("B", ovr_band.ReadRaster())
    # Test that data is zero only in the refreshed area, and unchanged
    # in other areas
    for j in range(10):
        for i in range(5):
            idx = (j) * ovr_band.XSize + (i + 5)
            assert ovr_data_refreshed[idx] == 0
            ovr_data_refreshed[idx] = ovr_data_ori[idx]
    assert ovr_data_refreshed == ovr_data_ori
    ds = None


###############################################################################


def test_gdalalg_raster_overview_refresh_source_timestamp_no_ovr():

    ds = gdal.Translate(
        "", "../gcore/data/byte.tif", options="-f MEM -outsize 512 512 -r cubic"
    )
    ds.BuildOverviews("BILINEAR", [2])

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = ds
    alg["use-source-timestamp"] = True
    with pytest.raises(Exception, match="Cannot find .ovr"):
        alg.Run()


###############################################################################


def test_gdalalg_raster_overview_refresh_source_timestamp_mtime_zero(tmp_vsimem):

    gdal.Translate(
        tmp_vsimem / "out.tif",
        "../gcore/data/byte.tif",
        options="-f GTiff -outsize 512 512 -r cubic",
    )
    ds = gdal.Open(tmp_vsimem / "out.tif")
    ds.BuildOverviews("BILINEAR", [2])

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = ds
    alg["use-source-timestamp"] = True
    with gdaltest.config_option("CPL_VSI_MEM_MTIME", "0"):
        with pytest.raises(Exception, match="Cannot get modification time"):
            alg.Run()


###############################################################################


def test_gdalalg_raster_overview_refresh_source_timestamp_no_vrt_or_gti(tmp_vsimem):

    gdal.Translate(
        tmp_vsimem / "out.tif",
        "../gcore/data/byte.tif",
        options="-f GTiff -outsize 512 512 -r cubic",
    )
    ds = gdal.Open(tmp_vsimem / "out.tif")
    ds.BuildOverviews("BILINEAR", [2])

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = ds
    alg["use-source-timestamp"] = True
    with pytest.raises(
        Exception, match="--use-source-timestamp only works on a VRT or GTI dataset"
    ):
        alg.Run()


###############################################################################


def test_gdalalg_raster_overview_refresh_no_band():

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = gdal.GetDriverByName("MEM").Create("", 1, 1, 0)
    with pytest.raises(Exception, match="Dataset has no raster band"):
        alg.Run()


###############################################################################


def test_gdalalg_raster_overview_refresh_no_overview():

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with pytest.raises(Exception, match="No overviews to refresh"):
        alg.Run()


###############################################################################


def test_gdalalg_raster_overview_refresh_wrong_overview_level():

    ds = gdal.GetDriverByName("MEM").Create("", 4, 4)
    ds.BuildOverviews("NEAR", [4])

    alg = gdal.Algorithm("raster", "overview", "refresh")
    alg["dataset"] = ds
    alg["levels"] = 2
    with pytest.raises(
        Exception, match="Cannot find overview level with subsampling factor of 2"
    ):
        alg.Run()
