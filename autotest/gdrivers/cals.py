#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test CALS driver
# Author:   Even Rouault, <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault, <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("CALS")

###############################################################################
# Source has no color table


def test_cals_1():

    tst = gdaltest.GDALTest("CALS", "hfa/small1bit.img", 1, 9907)

    tst.testCreateCopy()


###############################################################################
# Source has a color table (0,0,0),(255,255,255)


def test_cals_2():

    # Has no color table
    tst = gdaltest.GDALTest("CALS", "../../gcore/data/oddsize1bit.tif", 1, 3883)

    tst.testCreateCopy()


###############################################################################
# Source has a color table (255,255,255),(0,0,0)


def test_cals_3():

    src_ds = gdal.Open("../gcore/data/oddsize1bit.tif")
    tmp_ds = gdal.GetDriverByName("CALS").CreateCopy("/vsimem/cals_2_tmp.cal", src_ds)
    tmp_ds.SetMetadataItem("TIFFTAG_XRESOLUTION", "600")
    tmp_ds.SetMetadataItem("TIFFTAG_YRESOLUTION", "600")
    out_ds = gdal.GetDriverByName("CALS").CreateCopy("/vsimem/cals_2.cal", tmp_ds)
    assert gdal.VSIStatL("/vsimem/cals_2.cal.aux.xml") is None
    assert out_ds.GetRasterBand(1).Checksum() == 3883
    assert out_ds.GetMetadataItem("PIXEL_PATH") is None
    assert out_ds.GetMetadataItem("TIFFTAG_XRESOLUTION") == "600"
    assert out_ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    tmp_ds = None
    out_ds = None
    gdal.Unlink("/vsimem/cals_2_tmp.cal")
    gdal.Unlink("/vsimem/cals_2_tmp.cal.aux.xml")
    gdal.Unlink("/vsimem/cals_2.cal")


###############################################################################
# Test CreateCopy() error conditions


def test_cals_4():

    # 0 band
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 0)
    with pytest.raises(Exception):
        gdal.GetDriverByName("CALS").CreateCopy("/vsimem/cals_4.cal", src_ds)

    # 2 bands
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    with pytest.raises(Exception):
        gdal.GetDriverByName("CALS").CreateCopy(
            "/vsimem/cals_4.cal", src_ds, strict=True
        )

    # 1 band but not 1-bit
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    with pytest.raises(Exception):
        gdal.GetDriverByName("CALS").CreateCopy(
            "/vsimem/cals_4.cal", src_ds, strict=True
        )

    # Dimension > 999999
    src_ds = gdal.GetDriverByName("MEM").Create("", 1000000, 1, 1)
    src_ds.GetRasterBand(1).SetMetadataItem("NBITS", "1", "IMAGE_STRUCTURE")
    with pytest.raises(Exception):
        gdal.GetDriverByName("CALS").CreateCopy(
            "/vsimem/cals_4.cal", src_ds, strict=True
        )

    # Invalid output filename
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).SetMetadataItem("NBITS", "1", "IMAGE_STRUCTURE")
    with pytest.raises(Exception):
        gdal.GetDriverByName("CALS").CreateCopy("/not_existing_dir/cals_4.cal", src_ds)


###############################################################################
# Test PIXEL_PATH & LINE_PROGRESSION metadata item


def test_cals_5():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    src_ds.GetRasterBand(1).SetMetadataItem("NBITS", "1", "IMAGE_STRUCTURE")
    src_ds.SetMetadataItem("PIXEL_PATH", "90")
    src_ds.SetMetadataItem("LINE_PROGRESSION", "270")
    out_ds = gdal.GetDriverByName("CALS").CreateCopy("/vsimem/cals_5.cal", src_ds)
    assert gdal.VSIStatL("/vsimem/cals_5.cal.aux.xml") is None
    assert out_ds.GetMetadataItem("PIXEL_PATH") == "90"
    assert out_ds.GetMetadataItem("LINE_PROGRESSION") == "270"
    out_ds = None
    gdal.Unlink("/vsimem/cals_5.cal")


###############################################################################
