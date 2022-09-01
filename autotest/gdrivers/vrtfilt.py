#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VRT driver based filtering.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
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

import gdaltest
import pytest

from osgeo import gdal

###############################################################################
# Verify simple 3x3 averaging filter.


def test_vrtfilt_1():

    tst = gdaltest.GDALTest("VRT", "vrt/avfilt.vrt", 1, 21890)
    return tst.testOpen()


###############################################################################
# Verify simple 3x3 averaging filter (normalized) on a dataset with nodata


def test_vrtfilt_2():

    ds = gdal.Open("data/vrt/test_vrt_filter_nodata.tif")
    checksum = ds.GetRasterBand(1).Checksum()
    ds = None

    # This is a black&white checkboard, where black = nodata
    # Thus averaging it and taking nodata into account will not change it
    tst = gdaltest.GDALTest("VRT", "vrt/avfilt_nodata.vrt", 1, checksum)
    return tst.testOpen()


###############################################################################
# Try SetMetadataItem('source_0', xml, 'vrt_sources') (fix for #3052).
# Same result expected as for vrtfilt_1


def test_vrtfilt_3():

    ds = gdal.OpenShared("data/rgbsmall.tif")
    vrt_ds = gdal.GetDriverByName("VRT").CreateCopy("", ds)
    ds = None

    filterSourceXML = """    <KernelFilteredSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="50" ySize="50"/>
      <DstRect xOff="0" yOff="0" xSize="50" ySize="50"/>
      <Kernel>
        <Size>3</Size>
        <Coefs>0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111</Coefs>
      </Kernel>
    </KernelFilteredSource>"""

    try:
        vrt_ds.GetRasterBand(1).SetMetadataItem
    except Exception:
        pytest.skip()

    vrt_ds.GetRasterBand(1).SetMetadataItem("source_0", filterSourceXML, "vrt_sources")
    assert vrt_ds.GetRasterBand(1).Checksum() == 21890


###############################################################################
# Variant for SetMetadataItem('source_0', xml, 'vrt_sources')


def test_vrtfilt_4():

    vrt_ds = gdal.GetDriverByName("VRT").Create("", 50, 50, 1)

    filterSourceXML = """    <KernelFilteredSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="50" ySize="50"/>
      <DstRect xOff="0" yOff="0" xSize="50" ySize="50"/>
      <Kernel>
        <Size>3</Size>
        <Coefs>0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111</Coefs>
      </Kernel>
    </KernelFilteredSource>"""

    try:
        vrt_ds.GetRasterBand(1).SetMetadataItem
    except Exception:
        pytest.skip()

    vrt_ds.GetRasterBand(1).SetMetadataItem(
        "source_0", filterSourceXML, "new_vrt_sources"
    )
    assert vrt_ds.GetRasterBand(1).Checksum() == 21890


###############################################################################
# Variant for SetMetadata(md, 'vrt_sources')


def test_vrtfilt_5():

    vrt_ds = gdal.GetDriverByName("VRT").Create("", 50, 50, 1)

    filterSourceXML = """    <KernelFilteredSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="50" ySize="50"/>
      <DstRect xOff="0" yOff="0" xSize="50" ySize="50"/>
      <Kernel>
        <Size>3</Size>
        <Coefs>0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111</Coefs>
      </Kernel>
    </KernelFilteredSource>"""

    md = {}
    md["source_0"] = filterSourceXML

    vrt_ds.GetRasterBand(1).SetMetadata(md, "vrt_sources")
    assert vrt_ds.GetRasterBand(1).Checksum() == 21890


###############################################################################
# Verify separable Gaussian blur filter.


def test_vrtfilt_6():

    tst = gdaltest.GDALTest("VRT", "vrt/avfilt_1d.vrt", 1, 22377)
    return tst.testOpen()


###############################################################################
# Test block access


def test_vrtfilt_7():

    gdal.Translate(
        "/vsimem/src.tif", "data/rgbsmall.tif", options="-outsize 500 500 -r bilinear"
    )

    vrt_ds = gdal.GetDriverByName("VRT").Create("/vsimem/src.vrt", 500, 500, 1)

    filterSourceXML = """    <KernelFilteredSource>
      <SourceFilename>/vsimem/src.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="500" ySize="500"/>
      <DstRect xOff="0" yOff="0" xSize="500" ySize="500"/>
      <Kernel>
        <Size>3</Size>
        <Coefs>0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111</Coefs>
      </Kernel>
    </KernelFilteredSource>"""

    md = {}
    md["source_0"] = filterSourceXML

    vrt_ds.GetRasterBand(1).SetMetadata(md, "vrt_sources")

    ref_checksum = vrt_ds.GetRasterBand(1).Checksum()
    vrt_ds = None

    # Wrap our above VRT in a VRT that will use 128x128 blocks
    # (use of -mo FOO=BAR forces a non trivial copy to be made)
    out_ds = gdal.Translate("", "/vsimem/src.vrt", options="-of VRT -mo FOO=BAR")
    assert ref_checksum == out_ds.GetRasterBand(1).Checksum()
    out_ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/src.tif")
    gdal.GetDriverByName("VRT").Delete("/vsimem/src.vrt")


###############################################################################


def test_vrtfilt_invalid_kernel_size():

    vrt_ds = gdal.GetDriverByName("VRT").Create("", 50, 50, 1)

    filterSourceXML = """ <KernelFilteredSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="50" ySize="50"/>
      <DstRect xOff="0" yOff="0" xSize="50" ySize="50"/>
      <Kernel>
        <Size>-3</Size> <!-- negative value -->
        <Coefs>0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111</Coefs>
      </Kernel>
    </KernelFilteredSource>"""

    md = {}
    md["source_0"] = filterSourceXML

    with gdaltest.error_handler():
        assert vrt_ds.GetRasterBand(1).SetMetadata(md, "vrt_sources") != gdal.CE_None

    filterSourceXML = """ <KernelFilteredSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="50" ySize="50"/>
      <DstRect xOff="0" yOff="0" xSize="50" ySize="50"/>
      <Kernel>
        <Size>46341</Size> <!-- value larger than sqrt(INT_MAX) -->
        <Coefs>0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111 0.111111</Coefs>
      </Kernel>
    </KernelFilteredSource>"""

    md = {}
    md["source_0"] = filterSourceXML

    with gdaltest.error_handler():
        assert vrt_ds.GetRasterBand(1).SetMetadata(md, "vrt_sources") != gdal.CE_None


###############################################################################
# Cleanup.


def test_vrtfilt_cleanup():
    pass
