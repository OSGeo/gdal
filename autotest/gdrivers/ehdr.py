#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test EHdr format driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import struct

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("EHDR")

###############################################################################
# 16bit image.


def test_ehdr_1():

    tst = gdaltest.GDALTest("EHDR", "png/rgba16.png", 2, 2042)

    tst.testCreate()


###############################################################################
# 8bit with geotransform and projection check.


def test_ehdr_2():

    tst = gdaltest.GDALTest("EHDR", "byte.tif", 1, 4672)

    tst.testCreateCopy(check_gt=1, check_srs=1)


###############################################################################
# 32bit floating point (read, and createcopy).


def test_ehdr_3():

    tst = gdaltest.GDALTest("EHDR", "ehdr/float32.bil", 1, 27)

    tst.testCreateCopy()


###############################################################################
# create dataset with a nodata value and a color table.


def test_ehdr_4():

    drv = gdal.GetDriverByName("EHdr")
    ds = drv.Create("tmp/test_4.bil", 200, 100, 1, gdal.GDT_UInt8)

    raw_data = b"".join(struct.pack("h", v) for v in range(200))

    for line in range(100):
        ds.WriteRaster(0, line, 200, 1, raw_data, buf_type=gdal.GDT_Int16)

    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (255, 255, 255, 255))
    ct.SetColorEntry(1, (255, 255, 0, 255))
    ct.SetColorEntry(2, (255, 0, 255, 255))
    ct.SetColorEntry(3, (0, 255, 255, 255))

    ds.GetRasterBand(1).SetRasterColorTable(ct)

    ds.GetRasterBand(1).SetRasterColorTable(None)

    ds.GetRasterBand(1).SetRasterColorTable(ct)

    ds.GetRasterBand(1).SetNoDataValue(17)

    ds = None

    ###############################################################################
    # verify dataset's colortable and nodata value.

    ds = gdal.Open("tmp/test_4.bil")
    band = ds.GetRasterBand(1)

    assert band.GetNoDataValue() == 17, "failed to preserve nodata value."

    ct = band.GetRasterColorTable()
    assert (
        ct is not None
        and ct.GetCount() == 4
        and ct.GetColorEntry(2) == (255, 0, 255, 255)
    ), "color table not persisted properly."

    assert not band.GetDefaultRAT(), "did not expect RAT"

    band = None
    ct = None
    ds = None

    gdal.GetDriverByName("EHdr").Delete("tmp/test_4.bil")


###############################################################################
# Test creating an in memory copy.


def test_ehdr_6():

    tst = gdaltest.GDALTest("EHDR", "ehdr/float32.bil", 1, 27)

    tst.testCreateCopy(vsimem=1)


###############################################################################
# 32bit integer (read, and createcopy).


def test_ehdr_7():

    tst = gdaltest.GDALTest("EHDR", "int32.tif", 1, 4672)

    tst.testCreateCopy()


###############################################################################
# Test signed 8bit integer support. (#2717)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_ehdr_8():

    drv = gdal.GetDriverByName("EHDR")
    src_ds = gdal.Open("data/ehdr/8s.vrt")
    ds = drv.CreateCopy("tmp/ehdr_8.bil", src_ds)
    src_ds = None

    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int8
    cs = ds.GetRasterBand(1).Checksum()
    expected = 4776
    assert cs == expected, "Did not get expected image checksum."

    ds = None

    drv.Delete("tmp/ehdr_8.bil")


###############################################################################
# Test opening worldclim .hdr files that have a few extensions fields in the
# .hdr file to specify minimum, maximum and projection. Also test that we
# correctly guess the signedness of the datatype from the sign of the nodata
# value.


def test_ehdr_9():

    ds = gdal.Open("data/ehdr/wc_10m_CCCMA_A2a_2020_tmin_9.bil")

    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int16, "wrong datatype"

    assert ds.GetRasterBand(1).GetMinimum() == -191, "wrong minimum value"

    wkt = ds.GetProjectionRef()
    assert wkt.startswith('GEOGCS["WGS 84'), "wrong projection"

    ds = None


###############################################################################
# Test detecting floating point file based on image file size (#3933)


def test_ehdr_10():
    tst = gdaltest.GDALTest("EHDR", "ehdr/ehdr10.bil", 1, 8202)
    tst.testOpen()


###############################################################################
# Test detecting floating point file based on .flt extension (#3933)


def test_ehdr_11():
    tst = gdaltest.GDALTest("EHDR", "ehdr/ehdr11.flt", 1, 8202)
    tst.testOpen()


###############################################################################
# Test CreateCopy with 1bit data


@pytest.mark.require_driver("BMP")
def test_ehdr_12():

    src_ds = gdal.Open("../gcore/data/1bit.bmp")
    ds = gdal.GetDriverByName("EHDR").CreateCopy(
        "/vsimem/1bit.bil", src_ds, options=["NBITS=1"]
    )
    ds = None

    ds = gdal.Open("/vsimem/1bit.bil")
    assert (
        ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
    ), "did not get expected checksum"
    ds = None
    src_ds = None

    gdal.GetDriverByName("EHDR").Delete("/vsimem/1bit.bil")


###############################################################################
# Test statistics


def test_ehdr_13():

    if os.path.exists("data/byte.tif.aux.xml"):
        gdal.Unlink("data/byte.tif.aux.xml")

    src_ds = gdal.Open("data/byte.tif")
    ds = gdal.GetDriverByName("EHDR").CreateCopy("/vsimem/byte.bil", src_ds)
    ds = None
    src_ds = None

    ds = gdal.Open("/vsimem/byte.bil")
    assert ds.GetRasterBand(1).GetMinimum() is None, "did not expected minimum"
    assert ds.GetRasterBand(1).GetMaximum() is None, "did not expected maximum"
    stats = ds.GetRasterBand(1).GetStatistics(False, True)
    expected_stats = [74.0, 255.0, 126.765, 22.928470838675704]
    for i in range(4):
        assert stats[i] == pytest.approx(
            expected_stats[i], abs=0.0001
        ), "did not get expected statistics"
    ds = None

    f = gdal.VSIFOpenL("/vsimem/byte.stx", "rb")
    assert f is not None, "expected .stx file"
    gdal.VSIFCloseL(f)

    ds = gdal.Open("/vsimem/byte.bil")
    assert ds.GetRasterBand(1).GetMinimum() == pytest.approx(
        74, abs=0.0001
    ), "did not get expected minimum"
    assert ds.GetRasterBand(1).GetMaximum() == pytest.approx(
        255, abs=0.0001
    ), "did not get expected maximum"
    stats = ds.GetRasterBand(1).GetStatistics(False, True)
    expected_stats = [74.0, 255.0, 126.765, 22.928470838675704]
    for i in range(4):
        assert stats[i] == pytest.approx(
            expected_stats[i], abs=0.0001
        ), "did not get expected statistics"
    ds = None

    gdal.GetDriverByName("EHDR").Delete("/vsimem/byte.bil")


###############################################################################
# Test optimized RasterIO() (#5438)


def test_ehdr_14():

    src_ds = gdal.Open("data/byte.tif")
    ds = gdal.GetDriverByName("EHDR").CreateCopy("/vsimem/byte.bil", src_ds)
    src_ds = None

    for space in [1, 2]:
        out_ds = gdal.GetDriverByName("EHDR").Create("/vsimem/byte_reduced.bil", 10, 10)
        with gdaltest.config_option("GDAL_ONE_BIG_READ", "YES"):
            data_ori = ds.GetRasterBand(1).ReadRaster(
                0, 0, 20, 20, 20, 20, buf_pixel_space=space
            )
            data = ds.GetRasterBand(1).ReadRaster(
                0, 0, 20, 20, 10, 10, buf_pixel_space=space
            )
            out_ds.GetRasterBand(1).WriteRaster(
                0, 0, 10, 10, data, 10, 10, buf_pixel_space=space
            )
            out_ds.FlushCache()
            data2 = out_ds.ReadRaster(0, 0, 10, 10, 10, 10, buf_pixel_space=space)
            cs1 = out_ds.GetRasterBand(1).Checksum()

        out_ds.FlushCache()
        cs2 = out_ds.GetRasterBand(1).Checksum()

        assert space != 1 or data == data2

        assert not (cs1 != 1087 and cs1 != 1192) or (cs2 != 1087 and cs2 != 1192), space

        with gdaltest.config_option("GDAL_ONE_BIG_READ", "YES"):
            out_ds.GetRasterBand(1).WriteRaster(
                0, 0, 10, 10, data_ori, 20, 20, buf_pixel_space=space
            )
        out_ds.FlushCache()
        cs3 = out_ds.GetRasterBand(1).Checksum()

        assert cs3 == 1087 or cs3 == 1192, space

    ds = None

    gdal.GetDriverByName("EHDR").Delete("/vsimem/byte.bil")
    gdal.GetDriverByName("EHDR").Delete("/vsimem/byte_reduced.bil")


###############################################################################
# Test support for RAT (#3253)


def test_ehdr_rat():

    tmpfile = "/vsimem/rat.bil"
    gdal.Translate(tmpfile, "data/ehdr/int16_rat.bil", format="EHdr")
    ds = gdal.Open(tmpfile)
    rat = ds.GetRasterBand(1).GetDefaultRAT()
    assert rat is not None
    assert rat.GetColumnCount() == 4
    assert rat.GetRowCount() == 25
    for (idx, val) in [(0, -500), (1, 127), (2, 40), (3, 65)]:
        assert rat.GetValueAsInt(0, idx) == val
    for (idx, val) in [(0, 2000), (1, 145), (2, 97), (3, 47)]:
        assert rat.GetValueAsInt(24, idx) == val
    assert ds.GetRasterBand(1).GetColorTable() is not None
    ds = None

    ds = gdal.Open(tmpfile, gdal.GA_Update)
    ds.GetRasterBand(1).SetDefaultRAT(None)
    ds.GetRasterBand(1).SetColorTable(None)
    ds = None

    ds = gdal.Open(tmpfile, gdal.GA_Update)
    assert not (
        ds.GetRasterBand(1).GetDefaultRAT() or ds.GetRasterBand(1).GetColorTable()
    )
    with gdal.quiet_errors():
        ret = ds.GetRasterBand(1).SetDefaultRAT(gdal.RasterAttributeTable())
    assert ret != 0
    ds = None

    gdal.GetDriverByName("EHDR").Delete(tmpfile)


###############################################################################
# Test STATISTICS_APPROXIMATE


def test_ehdr_approx_stats_flag():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2000, 2000)
    src_ds.GetRasterBand(1).WriteRaster(1000, 1000, 1, 1, struct.pack("B" * 1, 20))
    tmpfile = "/vsimem/ehdr_approx_stats_flag.bil"
    gdal.Translate(tmpfile, src_ds, format="EHdr")

    ds = gdal.Open(tmpfile, gdal.GA_Update)
    approx_ok = 1
    force = 1
    stats = ds.GetRasterBand(1).GetStatistics(approx_ok, force)
    assert stats == [0.0, 0.0, 0.0, 0.0]
    md = ds.GetRasterBand(1).GetMetadata()
    assert "STATISTICS_APPROXIMATE" in md, "did not get expected metadata"

    approx_ok = 0
    force = 0
    stats = ds.GetRasterBand(1).GetStatistics(approx_ok, force)
    assert stats is None

    ds = gdal.Open(tmpfile, gdal.GA_Update)
    approx_ok = 0
    force = 0
    stats = ds.GetRasterBand(1).GetStatistics(approx_ok, force)
    assert stats is None

    approx_ok = 0
    force = 1
    stats = ds.GetRasterBand(1).GetStatistics(approx_ok, force)
    assert stats[1] == 20.0, "did not get expected stats"
    md = ds.GetRasterBand(1).GetMetadata()
    assert "STATISTICS_APPROXIMATE" not in md, "did not get expected metadata"
    ds = None

    gdal.GetDriverByName("EHDR").Delete(tmpfile)


###############################################################################


def test_ehdr_read_truncated():

    ds = gdal.Open("data/ehdr/truncated.bin")
    with pytest.raises(Exception, match="Failed to read block at offset"):
        ds.GetRasterBand(1).Checksum()
    with pytest.raises(Exception, match="Failed to read block at offset"):
        ds.GetRasterBand(1).ReadRaster()
