#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GDALNoDataMaskBand
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2018, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import struct

from osgeo import gdal


def test_nodatamaskband_1():

    for (dt, struct_type, v) in [
        (gdal.GDT_Byte, "B", 255),
        (gdal.GDT_Int16, "h", 32767),
        (gdal.GDT_UInt16, "H", 65535),
        (gdal.GDT_Int32, "i", 0x7FFFFFFF),
        (gdal.GDT_UInt32, "I", 0xFFFFFFFF),
        (gdal.GDT_Float32, "f", 1.25),
        (gdal.GDT_Float32, "f", float("nan")),
        (gdal.GDT_Float64, "d", 1.2345678),
        (gdal.GDT_Float64, "d", float("nan")),
    ]:

        ds = gdal.GetDriverByName("MEM").Create("", 6, 4, 1, dt)
        ds.GetRasterBand(1).SetNoDataValue(v)
        ds.GetRasterBand(1).WriteRaster(
            0,
            0,
            6,
            4,
            struct.pack(
                struct_type * 6 * 4,
                v,
                1,
                1,
                v,
                v,
                0,
                v,
                1,
                1,
                v,
                v,
                0,
                v,
                1,
                1,
                v,
                v,
                0,
                0,
                v,
                v,
                0,
                0,
                0,
            ),
        )

        data = ds.GetRasterBand(1).GetMaskBand().ReadRaster()
        data_ar = struct.unpack("B" * 6 * 4, data)
        expected_ar = (
            0,
            255,
            255,
            0,
            0,
            255,
            0,
            255,
            255,
            0,
            0,
            255,
            0,
            255,
            255,
            0,
            0,
            255,
            255,
            0,
            0,
            255,
            255,
            255,
        )
        assert data_ar == expected_ar, dt

        data = ds.GetRasterBand(1).GetMaskBand().ReadBlock(0, 0)
        data_ar = struct.unpack("B" * 6 * 1, data)
        expected_ar = (0, 255, 255, 0, 0, 255)
        assert data_ar == expected_ar, dt

        data = ds.GetRasterBand(1).GetMaskBand().ReadBlock(0, 3)
        data_ar = struct.unpack("B" * 6 * 1, data)
        expected_ar = (255, 0, 0, 255, 255, 255)
        assert data_ar == expected_ar, dt

        data = ds.GetRasterBand(1).GetMaskBand().ReadRaster(buf_xsize=3, buf_ysize=2)
        data_ar = struct.unpack("B" * 3 * 2, data)
        expected_ar = (255, 0, 255, 0, 255, 255)
        assert data_ar == expected_ar, dt

        data = (
            ds.GetRasterBand(1)
            .GetMaskBand()
            .ReadRaster(buf_type=gdal.GDT_UInt16, buf_xsize=3, buf_ysize=2)
        )
        data_ar = struct.unpack("H" * 3 * 2, data)
        expected_ar = (255, 0, 255, 0, 255, 255)
        assert data_ar == expected_ar, dt
