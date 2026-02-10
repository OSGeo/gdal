#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GDALOverviewDataset
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2014 Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import shutil
import struct

import gdaltest
import pytest

from osgeo import gdal

###############################################################################
# Error cases


def test_overviewds_1():
    with pytest.raises(Exception):
        gdal.OpenEx("data/byte.tif", open_options=["OVERVIEW_LEVEL=0"])


###############################################################################
# Nominal cases


@pytest.mark.parametrize("externalOverviews", [True, False])
def test_overviewds_2(tmp_path, externalOverviews):

    shutil.copy("data/byte.tif", tmp_path)
    tmpfilename = str(tmp_path.joinpath("byte.tif"))
    if externalOverviews:
        ds = gdal.Open(tmpfilename)
        ds.BuildOverviews("NEAR", overviewlist=[2, 4])
        ds = None
        ds = gdal.Open(tmpfilename, gdal.GA_Update)
    else:
        ds = gdal.Open(tmpfilename, gdal.GA_Update)
        ds.BuildOverviews("NEAR", overviewlist=[2, 4])
    ds.GetRasterBand(1).WriteRaster(2, 2, 5, 5, b"\0" * 25)
    ds.GetRasterBand(1).WriteRaster(2, 2, 1, 1, b"\0")
    ds.GetRasterBand(1).GetOverview(1).WriteRaster(2, 2, 1, 1, b"\0")
    ds = None

    src_ds = gdal.Open(tmpfilename)

    ds = gdal.OpenEx(tmpfilename, open_options=["OVERVIEW_LEVEL=NONE"])
    assert ds.RasterXSize == 20 and ds.RasterYSize == 20 and ds.RasterCount == 1
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetProjectionRef() == src_ds.GetProjectionRef()
    assert ds.GetGeoTransform() == src_ds.GetGeoTransform()
    assert ds.ReadRaster() == src_ds.ReadRaster()
    # Check that subsampled request doesn't use source overviews
    assert (
        ds.ReadRaster(0, 0, 20, 20, 10, 10)
        != src_ds.GetRasterBand(1).GetOverview(0).ReadRaster()
    )
    assert (
        ds.GetRasterBand(1).ReadRaster(0, 0, 20, 20, 10, 10)
        != src_ds.GetRasterBand(1).GetOverview(0).ReadRaster()
    )
    ds = None

    ds = gdal.OpenEx(tmpfilename, open_options=["OVERVIEW_LEVEL=0only"])
    assert ds.RasterXSize == 10 and ds.RasterYSize == 10 and ds.RasterCount == 1
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    expected_data = src_ds.GetRasterBand(1).GetOverview(1).ReadRaster()
    got_data = ds.ReadRaster(0, 0, 10, 10, 5, 5)
    assert len(got_data) == len(expected_data)
    assert got_data != expected_data
    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 10, 10, 5, 5)
    assert len(got_data) == len(expected_data)
    assert got_data != expected_data
    ds = None

    ds = gdal.OpenEx(tmpfilename, open_options=["OVERVIEW_LEVEL=0"])
    assert ds is not None
    assert ds.RasterXSize == 10 and ds.RasterYSize == 10 and ds.RasterCount == 1
    assert ds.GetProjectionRef() == src_ds.GetProjectionRef()
    src_gt = src_ds.GetGeoTransform()
    expected_gt = (
        src_gt[0],
        src_gt[1] * 2,
        src_gt[2],
        src_gt[3],
        src_gt[4],
        src_gt[5] * 2,
    )
    gt = ds.GetGeoTransform()
    for i in range(6):
        assert expected_gt[i] == pytest.approx(gt[i], abs=1e-5)
    assert (
        ds.GetGCPCount() == 0
        and ds.GetGCPProjection() == src_ds.GetGCPProjection()
        and not ds.GetGCPs()
    )
    expected_data = src_ds.GetRasterBand(1).GetOverview(0).ReadRaster()
    got_data = ds.ReadRaster()
    assert expected_data == got_data
    got_data = ds.GetRasterBand(1).ReadRaster()
    assert expected_data == got_data
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    expected_data = src_ds.GetRasterBand(1).GetOverview(1).ReadRaster()
    got_data = ds.GetRasterBand(1).GetOverview(0).ReadRaster()
    assert expected_data == got_data
    got_data = ds.ReadRaster(0, 0, 10, 10, 5, 5)
    assert expected_data == got_data
    got_data = ds.GetRasterBand(1).ReadRaster(0, 0, 10, 10, 5, 5)
    assert expected_data == got_data
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID
    assert ds.GetRasterBand(1).GetMaskBand()
    assert ds.GetMetadata() == src_ds.GetMetadata()
    assert ds.GetMetadataItem("AREA_OR_POINT") == src_ds.GetMetadataItem(
        "AREA_OR_POINT"
    )
    assert not ds.GetMetadata("RPC")
    assert not ds.GetMetadata("GEOLOCATION")
    assert ds.GetMetadataItem("RPC", "FOO") is None
    ds = None


###############################################################################
# Test GCP


def test_overviewds_3(tmp_path):

    src_ds = gdal.Open("data/byte.tif")
    ds = gdal.GetDriverByName("GTiff").CreateCopy(tmp_path / "byte.tif", src_ds)
    ds.SetGeoTransform([0, 1, 0, 0, 0, 1])  # cancel geotransform
    gcp1 = gdal.GCP()
    gcp1.GCPPixel = 0
    gcp1.GCPLine = 0
    gcp1.GCPX = 440720.000
    gcp1.GCPY = 3751320.000
    gcp2 = gdal.GCP()
    gcp2.GCPPixel = 0
    gcp2.GCPLine = 20
    gcp2.GCPX = 440720.000
    gcp2.GCPY = 3750120.000
    gcp3 = gdal.GCP()
    gcp3.GCPPixel = 20
    gcp3.GCPLine = 0
    gcp3.GCPX = 441920.000
    gcp3.GCPY = 3751320.000
    src_gcps = (gcp1, gcp2, gcp3)
    ds.SetGCPs(src_gcps, src_ds.GetProjectionRef())

    tr = gdal.Transformer(ds, None, ["METHOD=GCP_POLYNOMIAL"])
    _, ref_pnt = tr.TransformPoint(0, 20, 10)

    ds.BuildOverviews("NEAR", overviewlist=[2, 4])
    ds = None

    ds = gdal.OpenEx(tmp_path / "byte.tif", open_options=["OVERVIEW_LEVEL=0"])
    gcps = ds.GetGCPs()
    for i in range(3):
        assert (
            gcps[i].GCPPixel == src_gcps[i].GCPPixel / 2
            and gcps[i].GCPLine == src_gcps[i].GCPLine / 2
            and gcps[i].GCPX == src_gcps[i].GCPX
            and gcps[i].GCPY == src_gcps[i].GCPY
        )

    # Really check that the transformer works
    tr = gdal.Transformer(ds, None, ["METHOD=GCP_POLYNOMIAL"])
    _, pnt = tr.TransformPoint(0, 20 / 2.0, 10 / 2.0)

    for i in range(3):
        assert ref_pnt[i] == pytest.approx(pnt[i], abs=1e-5)
    ds = None


###############################################################################
# Test RPC


def myfloat(s):
    p = s.rfind(" ")
    if p >= 0:
        s = s[0:p]
    return float(s)


def test_overviewds_4(tmp_path):

    shutil.copy("data/byte.tif", tmp_path)
    shutil.copy("data/test_rpc.txt", tmp_path / "byte_rpc.txt")
    ds = gdal.Open(tmp_path / "byte.tif")
    rpc_md = ds.GetMetadata("RPC")

    tr = gdal.Transformer(ds, None, ["METHOD=RPC"])
    _, ref_pnt = tr.TransformPoint(0, 20, 10)

    ds.BuildOverviews("NEAR", overviewlist=[2, 4])
    ds = None

    ds = gdal.OpenEx(tmp_path / "byte.tif", open_options=["OVERVIEW_LEVEL=0"])
    got_md = ds.GetMetadata("RPC")

    for key in rpc_md:
        assert ds.GetMetadataItem(key, "RPC") == got_md[key]
        if key == "LINE_SCALE" or key == "SAMP_SCALE":
            assert float(got_md[key]) == pytest.approx(myfloat(rpc_md[key]) / 2)
        elif key == "LINE_OFF" or key == "SAMP_OFF":
            assert float(got_md[key]) == pytest.approx(
                (myfloat(rpc_md[key]) + 0.5) / 2 - 0.5
            )
        elif got_md[key] != rpc_md[key]:
            print(got_md[key])
            print(rpc_md[key])
            pytest.fail(key)

    # Really check that the transformer works
    tr = gdal.Transformer(ds, None, ["METHOD=RPC"])
    _, pnt = tr.TransformPoint(0, 20 / 2.0, 10 / 2.0)

    for i in range(3):
        assert ref_pnt[i] == pytest.approx(pnt[i], abs=1e-5)

    ds = None


###############################################################################
# Test GEOLOCATION


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_overviewds_5(tmp_path):

    shutil.copy("data/sstgeo.tif", tmp_path)
    shutil.copy("data/sstgeo.vrt", tmp_path)

    ds = gdal.Open(tmp_path / "sstgeo.vrt")
    geoloc_md = ds.GetMetadata("GEOLOCATION")

    tr = gdal.Transformer(ds, None, ["METHOD=GEOLOC_ARRAY"])
    _, ref_pnt = tr.TransformPoint(0, 20, 10)

    ds.BuildOverviews("NEAR", overviewlist=[2, 4])
    ds = None

    ds = gdal.OpenEx(tmp_path / "sstgeo.vrt", open_options=["OVERVIEW_LEVEL=0"])
    got_md = ds.GetMetadata("GEOLOCATION")

    for key in geoloc_md:
        assert ds.GetMetadataItem(key, "GEOLOCATION") == got_md[key]
        if key == "PIXEL_OFFSET" or key == "LINE_OFFSET":
            assert float(got_md[key]) == pytest.approx(
                myfloat(geoloc_md[key]) * 2, abs=1e-1
            )
        elif key == "PIXEL_STEP" or key == "LINE_STEP":
            assert float(got_md[key]) == pytest.approx(
                myfloat(geoloc_md[key]) / 2, abs=1e-1
            )
        elif got_md[key] != geoloc_md[key]:
            print(got_md[key])
            print(geoloc_md[key])
            pytest.fail(key)

    # Really check that the transformer works
    tr = gdal.Transformer(ds, None, ["METHOD=GEOLOC_ARRAY"])
    expected_xyz = (20.0 / 2, 10.0 / 2, 0)
    _, pnt = tr.TransformPoint(1, ref_pnt[0], ref_pnt[1])

    for i in range(3):
        assert pnt[i] == pytest.approx(expected_xyz[i], abs=0.5)
    ds = None


###############################################################################
# Test VRT


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_overviewds_6(tmp_path):

    shutil.copy("data/byte.tif", tmp_path)
    ds = gdal.Open(tmp_path / "byte.tif")
    ds.BuildOverviews("NEAR", overviewlist=[2, 4])
    ds = None

    src_ds = gdal.OpenEx(tmp_path / "byte.tif", open_options=["OVERVIEW_LEVEL=0"])
    expected_cs = src_ds.GetRasterBand(1).Checksum()
    ds = gdal.GetDriverByName("VRT").CreateCopy(tmp_path / "byte.vrt", src_ds)
    ds = None
    src_ds = None

    ds = gdal.Open(tmp_path / "byte.vrt")
    assert ds.RasterXSize == 10 and ds.RasterYSize == 10 and ds.RasterCount == 1
    got_cs = ds.GetRasterBand(1).Checksum()
    assert got_cs == expected_cs
    ds = None


###############################################################################
# Dataset with a mask


def test_overviewds_mask(tmp_vsimem):

    src_ds = gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "test.tif", 4, 4)
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 2, 4, b"\xff" * 8)
    src_ds.BuildOverviews("NEAR", [2, 4])
    src_ds = None

    ovr_ds = gdal.OpenEx(tmp_vsimem / "test.tif", open_options=["OVERVIEW_LEVEL=0"])
    assert ovr_ds
    assert ovr_ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    ovrmaskband = ovr_ds.GetRasterBand(1).GetMaskBand()
    assert struct.unpack("B" * 4, ovrmaskband.ReadRaster()) == (255, 0, 255, 0)

    # Mask of mask
    assert ovrmaskband.GetMaskFlags() == gdal.GMF_ALL_VALID
    assert struct.unpack("B" * 4, ovrmaskband.GetMaskBand().ReadRaster()) == (
        255,
        255,
        255,
        255,
    )

    # Overview of overview of mask
    assert ovrmaskband.GetOverviewCount() == 1
    ovrofovrmaskband = ovrmaskband.GetOverview(0)
    assert struct.unpack("B" * 1, ovrofovrmaskband.ReadRaster()) == (255,)

    ovr_ds = None
