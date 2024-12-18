#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for tiff files using overviews in subifds
# Author:   Thomas Bonfort <thomas.bonfort@airbus.com>
#
###############################################################################
# Copyright (c) 2019,  Thomas Bonfort <thomas.bonfort@airbus.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import struct

from osgeo import gdal

###############################################################################
# Test absolute/offset && index directory access


def test_tiff_read_subifds():

    ds = gdal.Open("data/tiff_with_subifds.tif")
    assert ds.GetRasterBand(1).Checksum() == 35731

    sds = ds.GetSubDatasets()
    assert len(sds) == 2
    assert sds[0][0] == "GTIFF_DIR:1:data/tiff_with_subifds.tif"
    assert sds[1][0] == "GTIFF_DIR:2:data/tiff_with_subifds.tif"

    data = ds.GetRasterBand(1).ReadRaster(
        buf_xsize=1, buf_ysize=1, xsize=1, ysize=1, buf_type=gdal.GDT_Int16
    )
    assert struct.unpack("H", data)[0] == 220

    data = (
        ds.GetRasterBand(1)
        .GetOverview(1)
        .ReadRaster(buf_xsize=1, buf_ysize=1, xsize=1, ysize=1, buf_type=gdal.GDT_Int16)
    )
    assert struct.unpack("H", data)[0] == 12

    ds = gdal.Open("GTIFF_DIR:1:data/tiff_with_subifds.tif")
    assert ds.GetRasterBand(1).Checksum() == 35731

    data = ds.GetRasterBand(1).ReadRaster(
        buf_xsize=1, buf_ysize=1, xsize=1, ysize=1, buf_type=gdal.GDT_Int16
    )
    assert struct.unpack("H", data)[0] == 220

    data = (
        ds.GetRasterBand(1)
        .GetOverview(1)
        .ReadRaster(buf_xsize=1, buf_ysize=1, xsize=1, ysize=1, buf_type=gdal.GDT_Int16)
    )
    assert struct.unpack("H", data)[0] == 12

    ds = gdal.Open("GTIFF_DIR:2:data/tiff_with_subifds.tif")
    assert ds.GetRasterBand(1).Checksum() == 0

    data = ds.GetRasterBand(1).ReadRaster(
        buf_xsize=1, buf_ysize=1, xsize=1, ysize=1, buf_type=gdal.GDT_Int16
    )
    assert struct.unpack("H", data)[0] == 0

    data = (
        ds.GetRasterBand(1)
        .GetOverview(1)
        .ReadRaster(buf_xsize=1, buf_ysize=1, xsize=1, ysize=1, buf_type=gdal.GDT_Int16)
    )
    assert struct.unpack("H", data)[0] == 128
