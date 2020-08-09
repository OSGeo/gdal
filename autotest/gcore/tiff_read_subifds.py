#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for tiff files using overviews in subifds
# Author:   Thomas Bonfort <thomas.bonfort@airbus.com>
#
###############################################################################
# Copyright (c) 2019,  Thomas Bonfort <thomas.bonfort@airbus.com>
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

import struct

from osgeo import gdal


###############################################################################
# Test absolute/offset && index directory access


def test_tiff_read_subifds():

    ds = gdal.Open('data/tiff_with_subifds.tif')
    assert ds.GetRasterBand(1).Checksum() == 35731

    sds = ds.GetSubDatasets()
    assert len(sds) == 2
    assert sds[0][0] == "GTIFF_DIR:1:data/tiff_with_subifds.tif"
    assert sds[1][0] == "GTIFF_DIR:2:data/tiff_with_subifds.tif"

    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=1,buf_ysize=1, xsize=1, ysize=1,buf_type=gdal.GDT_Int16)
    assert struct.unpack('H', data)[0] == 220

    data = ds.GetRasterBand(1).GetOverview(1).ReadRaster(buf_xsize=1,buf_ysize=1, xsize=1, ysize=1,buf_type=gdal.GDT_Int16)
    assert struct.unpack('H', data)[0] == 12

    ds = gdal.Open('GTIFF_DIR:1:data/tiff_with_subifds.tif')
    assert ds.GetRasterBand(1).Checksum() == 35731

    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=1,buf_ysize=1, xsize=1, ysize=1,buf_type=gdal.GDT_Int16)
    assert struct.unpack('H', data)[0] == 220

    data = ds.GetRasterBand(1).GetOverview(1).ReadRaster(buf_xsize=1,buf_ysize=1, xsize=1, ysize=1,buf_type=gdal.GDT_Int16)
    assert struct.unpack('H', data)[0] == 12

    ds = gdal.Open('GTIFF_DIR:2:data/tiff_with_subifds.tif')
    assert ds.GetRasterBand(1).Checksum() == 0

    data = ds.GetRasterBand(1).ReadRaster(buf_xsize=1,buf_ysize=1, xsize=1, ysize=1,buf_type=gdal.GDT_Int16)
    assert struct.unpack('H', data)[0] == 0

    data = ds.GetRasterBand(1).GetOverview(1).ReadRaster(buf_xsize=1,buf_ysize=1, xsize=1, ysize=1,buf_type=gdal.GDT_Int16)
    assert struct.unpack('H', data)[0] == 128