#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test CALS driver
# Author:   Even Rouault, <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault, <even dot rouault at spatialys dot com>
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



from osgeo import gdal
import gdaltest

###############################################################################
# Source has no color table


def test_cals_1():

    tst = gdaltest.GDALTest('CALS', 'hfa/small1bit.img', 1, 9907)

    return tst.testCreateCopy()

###############################################################################
# Source has a color table (0,0,0),(255,255,255)


def test_cals_2():

    # Has no color table
    tst = gdaltest.GDALTest('CALS', '../../gcore/data/oddsize1bit.tif', 1, 3883)

    return tst.testCreateCopy()

###############################################################################
# Source has a color table (255,255,255),(0,0,0)


def test_cals_3():

    src_ds = gdal.Open('../gcore/data/oddsize1bit.tif')
    tmp_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_2_tmp.cal', src_ds)
    tmp_ds.SetMetadataItem('TIFFTAG_XRESOLUTION', '600')
    tmp_ds.SetMetadataItem('TIFFTAG_YRESOLUTION', '600')
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_2.cal', tmp_ds)
    assert gdal.VSIStatL('/vsimem/cals_2.cal.aux.xml') is None
    assert out_ds.GetRasterBand(1).Checksum() == 3883
    assert out_ds.GetMetadataItem('PIXEL_PATH') is None
    assert out_ds.GetMetadataItem('TIFFTAG_XRESOLUTION') == '600'
    assert out_ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_PaletteIndex
    tmp_ds = None
    out_ds = None
    gdal.Unlink('/vsimem/cals_2_tmp.cal')
    gdal.Unlink('/vsimem/cals_2_tmp.cal.aux.xml')
    gdal.Unlink('/vsimem/cals_2.cal')

###############################################################################
# Test CreateCopy() error conditions


def test_cals_4():

    # 0 band
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 0)
    gdal.PushErrorHandler()
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_4.cal', src_ds)
    gdal.PopErrorHandler()
    assert out_ds is None

    # 2 bands
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 2)
    gdal.PushErrorHandler()
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_4.cal', src_ds, strict=True)
    gdal.PopErrorHandler()
    assert out_ds is None

    # 1 band but not 1-bit
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 1)
    gdal.PushErrorHandler()
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_4.cal', src_ds, strict=True)
    gdal.PopErrorHandler()
    assert out_ds is None

    # Dimension > 999999
    src_ds = gdal.GetDriverByName('MEM').Create('', 1000000, 1, 1)
    src_ds.GetRasterBand(1).SetMetadataItem('NBITS', '1', 'IMAGE_STRUCTURE')
    gdal.PushErrorHandler()
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_4.cal', src_ds, strict=True)
    gdal.PopErrorHandler()
    assert out_ds is None

    # Invalid output filename
    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 1)
    src_ds.GetRasterBand(1).SetMetadataItem('NBITS', '1', 'IMAGE_STRUCTURE')
    gdal.PushErrorHandler()
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/not_existing_dir/cals_4.cal', src_ds)
    gdal.PopErrorHandler()
    assert out_ds is None

###############################################################################
# Test PIXEL_PATH & LINE_PROGRESSION metadata item


def test_cals_5():

    src_ds = gdal.GetDriverByName('MEM').Create('', 1, 1, 1)
    src_ds.GetRasterBand(1).SetMetadataItem('NBITS', '1', 'IMAGE_STRUCTURE')
    src_ds.SetMetadataItem('PIXEL_PATH', '90')
    src_ds.SetMetadataItem('LINE_PROGRESSION', '270')
    out_ds = gdal.GetDriverByName('CALS').CreateCopy('/vsimem/cals_5.cal', src_ds)
    assert gdal.VSIStatL('/vsimem/cals_5.cal.aux.xml') is None
    assert out_ds.GetMetadataItem('PIXEL_PATH') == '90'
    assert out_ds.GetMetadataItem('LINE_PROGRESSION') == '270'
    out_ds = None
    gdal.Unlink('/vsimem/cals_5.cal')

###############################################################################




