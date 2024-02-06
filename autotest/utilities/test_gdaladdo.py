#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdaladdo testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import array
import os
import shutil

import gdaltest
import pytest
import test_cli_utilities

from gcore import tiff_ovr
from osgeo import gdal, ogr

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdaladdo_path() is None, reason="gdaladdo not available"
)


@pytest.fixture()
def gdaladdo_path():
    return test_cli_utilities.get_gdaladdo_path()


###############################################################################
# Similar to tiff_ovr_1


def test_gdaladdo_1(gdaladdo_path, tmp_path):

    shutil.copy("../gcore/data/mfloat32.vrt", f"{tmp_path}/mfloat32.vrt")
    shutil.copy("../gcore/data/float32.tif", f"{tmp_path}/float32.tif")

    (_, err) = gdaltest.runexternal_out_and_err(
        f"{gdaladdo_path} {tmp_path}/mfloat32.vrt 2 4"
    )
    assert err is None or err == "", "got error/warning"

    ds = gdal.Open(f"{tmp_path}/mfloat32.vrt")
    tiff_ovr.tiff_ovr_check(ds)
    ds = None


###############################################################################
# Test -r average. Similar to tiff_ovr_5


def test_gdaladdo_2(gdaladdo_path, tmp_path):

    shutil.copyfile("../gcore/data/nodata_byte.tif", f"{tmp_path}/ovr5.tif")

    gdaltest.runexternal(f"{gdaladdo_path} -r average {tmp_path}/ovr5.tif 2")

    ds = gdal.Open(f"{tmp_path}/ovr5.tif")
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1130

    assert cs == exp_cs, "got wrong overview checksum."

    ds = None


###############################################################################
# Test -ro


def test_gdaladdo_3(gdaladdo_path, tmp_path):

    gdal.Translate(
        f"{tmp_path}/test_gdaladdo_3.tif",
        "../gcore/data/nodata_byte.tif",
        options="-outsize 1024 1024",
    )

    gdaltest.runexternal(f"{gdaladdo_path} -ro {tmp_path}/test_gdaladdo_3.tif 2")

    ds = gdal.Open(f"{tmp_path}/test_gdaladdo_3.tif")
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 20683

    assert cs == exp_cs, "got wrong overview checksum."

    ds = None

    try:
        os.stat(f"{tmp_path}/test_gdaladdo_3.tif.ovr")
    except OSError:
        pytest.fail("no external overview.")

    # Test -clean

    gdaltest.runexternal(f"{gdaladdo_path} -clean {tmp_path}/test_gdaladdo_3.tif")

    ds = gdal.Open(f"{tmp_path}/test_gdaladdo_3.tif")
    cnt = ds.GetRasterBand(1).GetOverviewCount()
    ds = None

    assert cnt == 0, "did not clean overviews."

    assert not os.path.exists(f"{tmp_path}/test_gdaladdo_3.tif.ovr")


###############################################################################
# Test implicit levels


def test_gdaladdo_5(gdaladdo_path, tmp_path):

    input_tif = str(tmp_path / "test_gdaladdo_5.tif")

    shutil.copyfile("../gcore/data/nodata_byte.tif", input_tif)

    # Will not do anything given than the file is smaller than 256x256 already
    gdaltest.runexternal(f"{gdaladdo_path} {input_tif}")

    ds = gdal.Open(input_tif)
    cnt = ds.GetRasterBand(1).GetOverviewCount()
    ds = None

    assert cnt == 0

    # Will generate overviews of size 10 5 3 2 1
    gdaltest.runexternal(f"{gdaladdo_path} -minsize 1 {input_tif}")

    ds = gdal.Open(input_tif)
    cnt = ds.GetRasterBand(1).GetOverviewCount()
    ds = None

    assert cnt == 5


def test_gdaladdo_5bis(gdaladdo_path, tmp_path):

    input_tif = str(tmp_path / "test_gdaladdo_5.tif")

    shutil.copyfile("../gcore/data/nodata_byte.tif", input_tif)

    gdal.Translate(
        input_tif,
        "../gcore/data/nodata_byte.tif",
        options="-outsize 257 257",
    )

    # Will generate overviews of size 129x129
    gdaltest.runexternal(f"{gdaladdo_path} {input_tif}")

    ds = gdal.Open(input_tif)
    cnt = ds.GetRasterBand(1).GetOverviewCount()
    ds = None

    assert cnt == 1


###############################################################################
# Test --partial-refresh-from-projwin


def test_gdaladdo_partial_refresh_from_projwin(gdaladdo_path, tmp_path):

    input_tif = str(tmp_path / "tmp.tif")

    gdal.Translate(
        input_tif, "../gcore/data/byte.tif", options="-outsize 512 512 -r cubic"
    )
    gdaltest.runexternal(f"{gdaladdo_path} -r bilinear {input_tif} 2 4")

    ds = gdal.Open(input_tif, gdal.GA_Update)
    ovr_data_ori = array.array("B", ds.GetRasterBand(1).GetOverview(0).ReadRaster())
    ds.GetRasterBand(1).Fill(0)
    gt = ds.GetGeoTransform()
    ds = None

    x = 10
    y = 20
    width = 30
    height = 40
    ulx = gt[0] + gt[1] * x
    uly = gt[3] + gt[5] * y
    lrx = gt[0] + gt[1] * (x + width)
    lry = gt[3] + gt[5] * (y + height)
    out, err = gdaltest.runexternal_out_and_err(
        gdaladdo_path
        + f" -r bilinear --partial-refresh-from-projwin {ulx} {uly} {lrx} {lry} {input_tif} 2"
    )
    assert "ERROR" not in err, (out, err)

    ds = gdal.Open(input_tif)
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
    ds = None


###############################################################################
# Test --partial-refresh-from-source-timestamp


def test_gdaladdo_partial_refresh_from_source_timestamp(gdaladdo_path, tmp_path):

    left_tif = str(tmp_path / "left.tif")
    right_tif = str(tmp_path / "right.tif")
    tmp_vrt = str(tmp_path / "tmp.vrt")

    gdal.Translate(left_tif, "../gcore/data/byte.tif", options="-srcwin 0 0 10 20")
    gdal.Translate(right_tif, "../gcore/data/byte.tif", options="-srcwin 10 0 10 20")
    gdal.BuildVRT(tmp_vrt, [left_tif, right_tif])
    gdaltest.runexternal(f"{gdaladdo_path} -r bilinear {tmp_vrt} 2")

    ds = gdal.Open(tmp_vrt, gdal.GA_Update)
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

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdaladdo_path} -r bilinear --partial-refresh-from-source-timestamp {tmp_vrt}"
    )
    assert "ERROR" not in err, (out, err)

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
# Test --partial-refresh-from-source-extent


def test_gdaladdo_partial_refresh_from_source_extent(gdaladdo_path, tmp_path):

    left_tif = str(tmp_path / "left.tif")
    right_tif = str(tmp_path / "right.tif")
    tmp_vrt = str(tmp_path / "tmp.vrt")

    gdal.Translate(left_tif, "../gcore/data/byte.tif", options="-srcwin 0 0 10 20")
    gdal.Translate(right_tif, "../gcore/data/byte.tif", options="-srcwin 10 0 10 20")
    gdal.BuildVRT(tmp_vrt, [left_tif, right_tif])
    gdaltest.runexternal(f"{gdaladdo_path} -r bilinear {tmp_vrt} 2")

    ds = gdal.Open(tmp_vrt, gdal.GA_Update)
    ovr_data_ori = array.array("B", ds.GetRasterBand(1).GetOverview(0).ReadRaster())
    ds = None

    ds = gdal.Open(left_tif, gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = gdal.Open(right_tif, gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdaladdo_path} -r bilinear --partial-refresh-from-source-extent {right_tif} {tmp_vrt}"
    )
    assert "ERROR" not in err, (out, err)

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
# Test reuse of previous resampling method and overview levels


@pytest.mark.parametrize("read_only", [True, False])
def test_gdaladdo_reuse_previous_resampling_and_levels(
    gdaladdo_path, tmp_path, read_only
):

    tmpfilename = str(tmp_path / "test.tif")

    gdal.Translate(tmpfilename, "../gcore/data/byte.tif")

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdaladdo_path} -r average {tmpfilename}"
        + (" -ro" if read_only else "")
        + " 2 4"
    )
    assert "ERROR" not in err, (out, err)

    ds = gdal.Open(tmpfilename)
    assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("RESAMPLING") == "AVERAGE"
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1152
    ds = None

    # Change resampling method to CUBIC
    out, err = gdaltest.runexternal_out_and_err(
        f"{gdaladdo_path} -r cubic {tmpfilename}" + (" -ro" if read_only else "")
    )
    assert "ERROR" not in err, (out, err)

    ds = gdal.Open(tmpfilename, gdal.GA_Update)
    assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("RESAMPLING") == "CUBIC"
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1059
    # Zeroize overview
    ds.GetRasterBand(1).GetOverview(0).Fill(0)
    ds = None

    # Invoke gdaladdo without arguments and check overviews are regenerated
    # using CUBIC
    out, err = gdaltest.runexternal_out_and_err(
        f"{gdaladdo_path} {tmpfilename}" + (" -ro" if read_only else "")
    )
    assert "ERROR" not in err, (out, err)

    ds = gdal.Open(tmpfilename)
    assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("RESAMPLING") == "CUBIC"
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 1059
    ds = None


###############################################################################
# Test --partial-refresh-from-source-timestamp with GTI dataset


@pytest.mark.require_driver("GPKG")
def test_gdaladdo_partial_refresh_from_source_timestamp_gti(gdaladdo_path, tmp_path):

    left_tif = str(tmp_path / "left.tif")
    right_tif = str(tmp_path / "right.tif")

    gdal.Translate(left_tif, "../gcore/data/byte.tif", options="-srcwin 0 0 10 20")
    gdal.Translate(right_tif, "../gcore/data/byte.tif", options="-srcwin 10 0 10 20")

    source_ds = [gdal.Open(left_tif), gdal.Open(right_tif)]
    tmp_vrt = str(tmp_path / "test.gti.gpkg")
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(tmp_vrt)
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

    gdaltest.runexternal(f"{gdaladdo_path} -r bilinear {tmp_vrt} 2")

    ds = gdal.Open(tmp_vrt, gdal.GA_Update)
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

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdaladdo_path} -r bilinear --partial-refresh-from-source-timestamp {tmp_vrt}"
    )
    assert "ERROR" not in err, (out, err)

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
