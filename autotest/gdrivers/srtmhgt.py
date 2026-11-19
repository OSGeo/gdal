#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SRTMHGT support.
# Author:   Even Rouault < even dot rouault @ spatialys.com >
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import struct

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("SRTMHGT")


def setup_and_cleanup():

    yield

    try:
        gdal.GetDriverByName("SRTMHGT").Delete("/vsimem/n43w080.hgt")
        gdal.Unlink("/vsimem/N43W080.SRTMGL1.hgt.zip")
    except (RuntimeError, OSError):
        pass


###############################################################################
# Test a SRTMHGT Level 1 (made from a DTED Level 0)


@pytest.fixture()
def n43w080_hgt(tmp_path):

    n43_dt1_tif = str(tmp_path / "n43.dt1.tif")
    n43w080_hgt_fname = str(tmp_path / "n43w080.hgt")

    ds = gdal.Open("data/n43.dt0")

    bandSrc = ds.GetRasterBand(1)

    driver = gdal.GetDriverByName("GTiff")
    dsDst = driver.Create(n43_dt1_tif, 1201, 1201, 1, gdal.GDT_Int16)
    dsDst.SetProjection(
        'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]'
    )
    dsDst.SetGeoTransform(
        (
            -80.0004166666666663,
            0.0008333333333333,
            0,
            44.0004166666666670,
            0,
            -0.0008333333333333,
        )
    )

    bandDst = dsDst.GetRasterBand(1)

    data = bandSrc.ReadRaster(0, 0, 121, 121, 1201, 1201, gdal.GDT_Int16)
    bandDst.WriteRaster(0, 0, 1201, 1201, data, 1201, 1201, gdal.GDT_Int16)

    bandDst.FlushCache()

    bandDst = None
    ds = None
    dsDst = None

    ds = gdal.Open(n43_dt1_tif)
    driver = gdal.GetDriverByName("SRTMHGT")
    dsDst = driver.CreateCopy(n43w080_hgt_fname, ds)
    del dsDst

    return n43w080_hgt_fname


def test_srtmhgt_1(n43w080_hgt):

    with gdal.Open(n43w080_hgt) as ds:

        band = ds.GetRasterBand(1)
        chksum = band.Checksum()

        assert chksum == 60918, "Wrong checksum. Checksum found %d" % chksum


###############################################################################
# Test creating an in memory copy.


def test_srtmhgt_2(n43w080_hgt):

    ds = gdal.Open(n43w080_hgt)
    driver = gdal.GetDriverByName("SRTMHGT")
    dsDst = driver.CreateCopy("/vsimem/n43w080.hgt", ds)

    band = dsDst.GetRasterBand(1)
    chksum = band.Checksum()

    assert chksum == 60918, "Wrong checksum. Checksum found %d" % chksum
    dsDst = None

    # Test update support
    dsDst = gdal.Open("/vsimem/n43w080.hgt", gdal.GA_Update)
    dsDst.WriteRaster(0, 0, dsDst.RasterXSize, dsDst.RasterYSize, dsDst.ReadRaster())
    dsDst.FlushCache()

    assert chksum == 60918, "Wrong checksum. Checksum found %d" % chksum
    dsDst = None


###############################################################################
# Test reading from a .hgt.zip file


def test_srtmhgt_3(n43w080_hgt):

    ds = gdal.Open(n43w080_hgt)
    driver = gdal.GetDriverByName("SRTMHGT")
    driver.CreateCopy("/vsizip//vsimem/N43W080.SRTMGL1.hgt.zip/N43W080.hgt", ds)

    dsDst = gdal.Open("/vsimem/N43W080.SRTMGL1.hgt.zip")

    band = dsDst.GetRasterBand(1)
    chksum = band.Checksum()

    assert chksum == 60918, "Wrong checksum. Checksum found %d" % chksum


###############################################################################
# Test reading from a .SRTMSWBD.raw.zip file (GRASS #3246)


def test_srtmhgt_4():

    f = gdal.VSIFOpenL("/vsizip//vsimem/N43W080.SRTMSWBD.raw.zip/N43W080.raw", "wb")
    if f is None:
        pytest.skip()
    gdal.VSIFWriteL(" " * (3601 * 3601), 1, 3601 * 3601, f)
    gdal.VSIFCloseL(f)

    ds = gdal.Open("/vsimem/N43W080.SRTMSWBD.raw.zip")
    assert ds is not None
    cs = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.Unlink("/vsimem/N43W080.SRTMSWBD.raw.zip")

    assert cs == 3636, "Wrong checksum. Checksum found %d" % cs


###############################################################################
# Test reading from a .hgts file (https://github.com/OSGeo/gdal/issues/4239


def test_srtmhgt_hgts():

    f = gdal.VSIFOpenL("/vsimem/n00e006.hgts", "wb")
    if f is None:
        pytest.skip()
    gdal.VSIFWriteL(struct.pack(">f", 1.25) * (3601 * 3601), 4, 3601 * 3601, f)
    gdal.VSIFCloseL(f)

    ds = gdal.Open("/vsimem/n00e006.hgts")
    assert ds is not None
    min_, max_ = ds.GetRasterBand(1).ComputeRasterMinMax()
    gdal.Unlink("/vsimem/n00e006.hgts")

    assert min_ == 1.25
    assert max_ == 1.25


###############################################################################
# Test reading files of all supported sizes


@pytest.mark.parametrize(
    "width,height,nb_bytes",
    [
        (1201, 1201, 2),
        (1801, 3601, 2),
        (3601, 3601, 1),
        (3601, 3601, 2),
        (3601, 3601, 4),
        (7201, 7201, 2),
    ],
)
def test_srtmhgt_all_supported_sizes(tmp_vsimem, width, height, nb_bytes):

    filename = str(tmp_vsimem / "n00e000.hgt")
    f = gdal.VSIFOpenL(filename, "wb")
    if f is None:
        pytest.skip()
    gdal.VSIFTruncateL(f, width * height * nb_bytes)
    gdal.VSIFCloseL(f)

    ds = gdal.Open(filename)
    assert ds is not None
    assert ds.GetGeoTransform()[1] == pytest.approx(1.0 / (width - 1), rel=1e-8)
    assert ds.GetRasterBand(1).DataType == (
        gdal.GDT_UInt8
        if nb_bytes == 1
        else gdal.GDT_Int16 if nb_bytes == 2 else gdal.GDT_Float32
    )

    out_filename = str(tmp_vsimem / "create" / "n00e000.hgt")
    gdal.GetDriverByName("SRTMHGT").CreateCopy(out_filename, ds)
