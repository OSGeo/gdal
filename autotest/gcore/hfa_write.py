#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for Erdas Imagine (.img) HFA driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil
import struct

import gdaltest
import pytest

from osgeo import gdal, osr

pytestmark = pytest.mark.require_driver("HFA")

###############################################################################
# test that we can write a small file with a custom layer name.


def test_hfa_write_desc(tmp_path):

    img_path = tmp_path / "test_desc.img"

    src_ds = gdal.Open("data/byte.tif")

    new_ds = gdal.GetDriverByName("HFA").CreateCopy(img_path, src_ds)

    bnd = new_ds.GetRasterBand(1)
    bnd.SetDescription("CustomBandName")
    bnd = None

    src_ds = None
    new_ds = None

    new_ds = gdal.Open(img_path)
    bnd = new_ds.GetRasterBand(1)
    assert bnd.GetDescription() == "CustomBandName", "Didn't get custom band name."


###############################################################################
# test writing n-bit files.


@pytest.mark.parametrize(
    "options,expected_cs",
    [
        (["NBITS=1"], 252),
        (["NBITS=1", "COMPRESSED=YES"], 252),
        (["NBITS=2"], 718),
        (["NBITS=2", "COMPRESSED=YES"], 718),
        (["NBITS=4"], 2578),
        (["NBITS=4", "COMPRESSED=YES"], 2578),
    ],
)
def test_hfa_write_nbits(tmp_path, options, expected_cs):

    img_path = tmp_path / "nbits.img"

    drv = gdal.GetDriverByName("HFA")
    src_ds = gdal.Open("data/byte.tif")
    ds = drv.CreateCopy(img_path, src_ds, options=options)
    ds = None
    src_ds = None

    ds = gdal.Open(img_path)
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == expected_cs


###############################################################################
# Test creating a file with a nodata value, and fetching otherwise unread
# blocks and verifying they are the nodata value.  (#2427)


def test_hfa_write_nd_invalid(tmp_path):

    img_path = tmp_path / "ndinvalid.img"

    drv = gdal.GetDriverByName("HFA")
    ds = drv.Create(img_path, 512, 512, 1, gdal.GDT_UInt8, [])
    ds.GetRasterBand(1).SetNoDataValue(200)
    ds = None

    ds = gdal.Open(img_path)
    cs = ds.GetRasterBand(1).Checksum()

    assert cs == 29754, "Got wrong checksum on invalid image."


###############################################################################
# Test updating .rrd overviews in place (#2524).


def test_hfa_update_overviews(tmp_path):

    img_path = tmp_path / "small.img"
    rrd_path = tmp_path / "small.rrd"

    shutil.copyfile("data/small_ov.img", img_path)
    shutil.copyfile("data/small_ov.rrd", rrd_path)

    ds = gdal.Open(img_path, gdal.GA_Update)
    result = ds.BuildOverviews(overviewlist=[2])

    assert result == 0, "BuildOverviews() failed."


###############################################################################
# Test cleaning external overviews.


def test_hfa_clean_external_overviews(tmp_path):

    img_path = tmp_path / "small.img"
    rrd_path = tmp_path / "small.rrd"

    shutil.copyfile("data/small_ov.img", img_path)
    shutil.copyfile("data/small_ov.rrd", rrd_path)

    ds = gdal.Open(img_path, gdal.GA_Update)
    result = ds.BuildOverviews(overviewlist=[])

    assert result == 0, "BuildOverviews() failed."

    assert ds.GetRasterBand(1).GetOverviewCount() == 0, "Overviews still exist."

    ds = None
    ds = gdal.Open(img_path)
    assert ds.GetRasterBand(1).GetOverviewCount() == 0, "Overviews still exist."
    ds = None

    assert not os.path.exists(rrd_path)


###############################################################################
# Test writing high frequency data (#2525).


def test_hfa_bug_2525(tmp_path):

    tmp_filename = tmp_path / "test_hfa"

    drv = gdal.GetDriverByName("HFA")
    ds = drv.Create(
        tmp_filename, 64, 64, 1, gdal.GDT_UInt16, options=["COMPRESSED=YES"]
    )
    import struct

    data = struct.pack(
        "H" * 64,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
        0,
        65535,
    )
    for i in range(64):
        ds.GetRasterBand(1).WriteRaster(0, i, 64, 1, data)
    ds.Close()


###############################################################################
# Test building external overviews with HFA_USE_RRD=YES


def test_hfa_use_rrd(tmp_path):

    tmp_filename = tmp_path / "small.img"

    shutil.copyfile("data/small_ov.img", tmp_filename)

    with gdal.config_option("HFA_USE_RRD", "YES"):
        ds = gdal.Open(tmp_filename, gdal.GA_Update)
        result = ds.BuildOverviews(overviewlist=[2])

    assert result == 0, "BuildOverviews() failed."
    ds = None

    assert os.path.exists(tmp_path / "small.rrd")

    ds = gdal.Open(tmp_filename)
    assert (
        ds.GetRasterBand(1).GetOverview(0).Checksum() == 26148
    ), "Unexpected checksum."


###############################################################################
# Test fix for #4831


@pytest.mark.require_driver("BMP")
def test_hfa_update_existing_aux_overviews(tmp_path):

    tmp_filename = tmp_path / "hfa_update_existing_aux_overviews.bmp"

    with gdal.config_option("USE_RRD", "YES"):

        ds = gdal.GetDriverByName("BMP").Create(tmp_filename, 100, 100, 1)
        ds.GetRasterBand(1).Fill(255)
        ds = None

        # Create overviews
        ds = gdal.Open(tmp_filename)
        with gdaltest.disable_exceptions():
            ret = ds.BuildOverviews("NEAR", overviewlist=[2, 4])
        if (
            gdal.GetLastErrorMsg()
            == "This build does not support creating .aux overviews"
        ):
            pytest.skip(gdal.GetLastErrorMsg())
        assert ret == 0
        ds = None

        # Save overviews checksum
        ds = gdal.Open(tmp_filename)
        cs_ovr0 = ds.GetRasterBand(1).GetOverview(0).Checksum()
        cs_ovr1 = ds.GetRasterBand(1).GetOverview(1).Checksum()

        # and regenerate them
        ds.BuildOverviews("NEAR", overviewlist=[2, 4])
        ds = None

        ds = gdal.Open(tmp_filename)
        # Check overviews checksum
        new_cs_ovr0 = ds.GetRasterBand(1).GetOverview(0).Checksum()
        new_cs_ovr1 = ds.GetRasterBand(1).GetOverview(1).Checksum()
        if cs_ovr0 != new_cs_ovr0:
            pytest.fail()
        if cs_ovr1 != new_cs_ovr1:
            pytest.fail()

        # and regenerate them twice in a row
        ds.BuildOverviews("NEAR", overviewlist=[2, 4])
        ds.BuildOverviews("NEAR", overviewlist=[2, 4])
        ds = None

        ds = gdal.Open(tmp_filename)
        # Check overviews checksum
        new_cs_ovr0 = ds.GetRasterBand(1).GetOverview(0).Checksum()
        new_cs_ovr1 = ds.GetRasterBand(1).GetOverview(1).Checksum()
        if cs_ovr0 != new_cs_ovr0:
            pytest.fail()
        if cs_ovr1 != new_cs_ovr1:
            pytest.fail()

        # and regenerate them with an extra overview level
        ds.BuildOverviews("NEAR", overviewlist=[8])
        ds = None

        ds = gdal.Open(tmp_filename)
        # Check overviews checksum
        new_cs_ovr0 = ds.GetRasterBand(1).GetOverview(0).Checksum()
        new_cs_ovr1 = ds.GetRasterBand(1).GetOverview(1).Checksum()
        if cs_ovr0 != new_cs_ovr0:
            pytest.fail()
        if cs_ovr1 != new_cs_ovr1:
            pytest.fail()
        ds = None


###############################################################################
# Get the driver, and verify a few things about it.


init_list = [
    ("byte.tif", 4672),
    ("int16.tif", 4672),
    ("uint16.tif", 4672),
    ("int32.tif", 4672),
    ("uint32.tif", 4672),
    ("float32.tif", 4672),
    ("float64.tif", 4672),
    ("cfloat32.tif", 5028),
    ("cfloat64.tif", 5028),
    ("utmsmall.tif", 50054),
]

# full set of tests for normal mode.


@pytest.mark.parametrize(
    "filename,checksum",
    init_list,
    ids=[tup[0].split(".")[0] for tup in init_list],
)
@pytest.mark.parametrize(
    "testfunction",
    [
        "testCreateCopy",
        "testCreate",
        "testSetGeoTransform",
        "testSetMetadata",
    ],
)
@pytest.mark.require_driver("HFA")
def test_hfa_create_normal(filename, checksum, testfunction, tmp_path):
    ut = gdaltest.GDALTest("HFA", filename, 1, checksum, tmpdir=str(tmp_path))
    getattr(ut, testfunction)()


# Just a few for spill file, and compressed support.
short_list = [("byte.tif", 4672), ("uint16.tif", 4672), ("float64.tif", 4672)]


@pytest.mark.parametrize(
    "filename,checksum",
    short_list,
    ids=[tup[0].split(".")[0] for tup in short_list],
)
@pytest.mark.parametrize(
    "testfunction",
    [
        "testCreateCopy",
        "testCreate",
    ],
)
@pytest.mark.require_driver("HFA")
def test_hfa_create_spill(filename, checksum, testfunction, tmp_path):
    ut = gdaltest.GDALTest(
        "HFA", filename, 1, checksum, options=["USE_SPILL=YES"], tmpdir=str(tmp_path)
    )
    getattr(ut, testfunction)()


@pytest.mark.parametrize(
    "filename,checksum",
    short_list,
    ids=[tup[0].split(".")[0] for tup in short_list],
)
@pytest.mark.parametrize(
    "testfunction",
    [
        # 'testCreateCopy',
        "testCreate",
    ],
)
@pytest.mark.require_driver("HFA")
def test_hfa_create_compress(filename, checksum, testfunction):
    ut = gdaltest.GDALTest("HFA", filename, 1, checksum, options=["COMPRESS=YES"])
    getattr(ut, testfunction)()


def test_hfa_create_compress_big_block(tmp_vsimem):
    filename = tmp_vsimem / "test.img"
    src_ds = gdal.GetDriverByName("MEM").Create("", 128, 128, 1, gdal.GDT_UInt32)
    src_ds.GetRasterBand(1).Fill(4 * 1000 * 1000 * 1000)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, struct.pack("I", 0))
    gdal.GetDriverByName("HFA").CreateCopy(
        filename, src_ds, options=["COMPRESS=YES", "BLOCKSIZE=128"]
    )
    ds = gdal.Open(filename)
    got_data = ds.GetRasterBand(1).ReadRaster()
    assert got_data == src_ds.GetRasterBand(1).ReadRaster()


# GCPs go to PAM currently
def test_hfa_create_gcp(tmp_vsimem):
    filename = tmp_vsimem / "test.img"
    ds = gdal.GetDriverByName("HFA").Create(filename, 1, 1)
    gcp1 = gdal.GCP()
    gcp1.GCPPixel = 0
    gcp1.GCPLine = 0
    gcp1.GCPX = 440720.000
    gcp1.GCPY = 3751320.000
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    assert ds.SetGCPs((gcp1,), sr.ExportToWkt()) == gdal.CE_None
    ds = None

    with gdal.Open(filename) as ds:
        assert ds.GetGCPCount() == 1
        assert ds.GetGCPSpatialRef() is not None
        assert len(ds.GetGCPs()) == 1


@pytest.mark.require_driver("L1B")
def test_hfa_create_copy_from_ysize_0(tmp_vsimem):
    src_ds = gdal.Open("../gdrivers/data/l1b/n12gac8bit_truncated_ysize_0_1band.l1b")
    with pytest.raises(Exception, match="nXSize == 0 || nYSize == 0 not supported"):
        gdal.GetDriverByName("HFA").CreateCopy(tmp_vsimem / "out.img", src_ds)
