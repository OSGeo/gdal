#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test gdal.ComputeMedianCutPCT() and gdal.DitherRGB2PCT()
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import pytest

from osgeo import gdal

###############################################################################
# Test


def test_dither_1():

    drv = gdal.GetDriverByName("GTiff")

    src_ds = gdal.Open("../gdrivers/data/rgbsmall.tif")
    r_band = src_ds.GetRasterBand(1)
    g_band = src_ds.GetRasterBand(2)
    b_band = src_ds.GetRasterBand(3)

    dst_ds = drv.Create(
        "tmp/rgbsmall.tif", src_ds.RasterXSize, src_ds.RasterYSize, 1, gdal.GDT_Byte
    )
    dst_band = dst_ds.GetRasterBand(1)

    ct = gdal.ColorTable()

    nColors = 8

    gdal.ComputeMedianCutPCT(r_band, g_band, b_band, nColors, ct)

    dst_band.SetRasterColorTable(ct)

    gdal.DitherRGB2PCT(r_band, g_band, b_band, dst_band, ct)

    cs_expected = 8803
    cs = dst_band.Checksum()
    dst_band = None
    dst_ds = None

    assert ct.GetCount() == nColors, "color table size wrong"

    ref_ct = [
        (36, 48, 32, 255),
        (92, 120, 20, 255),
        (88, 96, 20, 255),
        (92, 132, 56, 255),
        (0, 0, 0, 255),
        (96, 152, 24, 255),
        (60, 112, 32, 255),
        (164, 164, 108, 255),
    ]

    for i in range(nColors):
        ct_data = ct.GetColorEntry(i)
        ref_data = ref_ct[i]

        for j in range(4):

            if ct_data[j] != ref_data[j]:
                for k in range(nColors):
                    print(ct.GetColorEntry(k))
                    print(ref_ct[k])
                pytest.fail("color table mismatch")

    if cs == cs_expected or gdal.GetConfigOption("CPL_DEBUG", "OFF") != "ON":
        drv.Delete("tmp/rgbsmall.tif")

    if cs != cs_expected:
        print("Got: ", cs)
        pytest.fail("got wrong checksum")
