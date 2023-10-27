#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test librarified gdalbuildvrt
# Author:   Even Rouault <even dot rouault @ spatialys dot com>
#
###############################################################################
# Copyright (c) 2016, Even Rouault <even dot rouault @ spatialys dot com>
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

import pathlib
import struct

import gdaltest
import pytest

from osgeo import gdal

###############################################################################
# Simple test


def test_gdalbuildvrt_lib_1():

    # Source = String
    ds = gdal.BuildVRT("", "../gcore/data/byte.tif")
    assert ds is not None, "got error/warning"

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    # Source = Array of string
    ds = gdal.BuildVRT("", ["../gcore/data/byte.tif"])
    assert ds is not None, "got error/warning"

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    # Source = Dataset
    ds = gdal.BuildVRT("", gdal.Open("../gcore/data/byte.tif"))
    assert ds is not None, "got error/warning"

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    # Source = Array of dataset
    ds = gdal.BuildVRT("", [gdal.Open("../gcore/data/byte.tif")])
    assert ds is not None, "got error/warning"

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"


###############################################################################
# Test callback


def mycallback(pct, msg, user_data):
    # pylint: disable=unused-argument
    user_data[0] = pct
    return 1


def test_gdalbuildvrt_lib_2():

    tab = [0]
    ds = gdal.BuildVRT(
        "", "../gcore/data/byte.tif", callback=mycallback, callback_data=tab
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    assert tab[0] == 1.0, "Bad percentage"

    ds = None


###############################################################################
# Test creating overviews


def test_gdalbuildvrt_lib_ovr(tmp_vsimem):

    tmpfilename = tmp_vsimem / "my.vrt"
    ds = gdal.BuildVRT(tmpfilename, pathlib.Path("../gcore/data/byte.tif"))
    ds.BuildOverviews("NEAR", [2])
    ds = None
    ds = gdal.Open(tmpfilename)
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    ds = None


def test_gdalbuildvrt_lib_te_partial_overlap():

    ds = gdal.BuildVRT(
        "",
        "../gcore/data/byte.tif",
        outputBounds=[440600, 3750060, 441860, 3751260],
        xRes=30,
        yRes=60,
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 8454
    xml = ds.GetMetadata("xml:VRT")[0]
    assert '<SrcRect xOff="0" yOff="1" xSize="19" ySize="19" />' in xml
    assert '<DstRect xOff="4" yOff="0" xSize="38" ySize="19" />' in xml


###############################################################################
# Test BuildVRT() with sources that can't be opened by name


def test_gdalbuildvrt_lib_mem_sources():
    def create_sources():
        src1_ds = gdal.GetDriverByName("MEM").Create(
            "i_have_a_name_but_nobody_can_open_me_through_it", 1, 1
        )
        src1_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
        src1_ds.GetRasterBand(1).Fill(100)

        src2_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
        src2_ds.SetGeoTransform([3, 1, 0, 49, 0, -1])
        src2_ds.GetRasterBand(1).Fill(200)

        return src1_ds, src2_ds

    def scenario_1():
        src1_ds, src2_ds = create_sources()
        vrt_ds = gdal.BuildVRT("", [src1_ds, src2_ds])
        vals = struct.unpack("B" * 2, vrt_ds.ReadRaster())
        assert vals == (100, 200)

        vrt_of_vrt_ds = gdal.BuildVRT("", [vrt_ds])
        vals = struct.unpack("B" * 2, vrt_of_vrt_ds.ReadRaster())
        assert vals == (100, 200)

    # Alternate scenario where the Python objects of sources and intermediate
    # VRT are no longer alive when the VRT of VRT is accessed
    def scenario_2():
        def get_vrt_of_vrt():
            src1_ds, src2_ds = create_sources()
            return gdal.BuildVRT("", [gdal.BuildVRT("", [src1_ds, src2_ds])])

        vrt_of_vrt_ds = get_vrt_of_vrt()
        vals = struct.unpack("B" * 2, vrt_of_vrt_ds.ReadRaster())
        assert vals == (100, 200)

    scenario_1()
    scenario_2()


###############################################################################
# Test BuildVRT() with sources that can't be opened by name, in separate mode


def test_gdalbuildvrt_lib_mem_sources_separate():
    def create_sources():
        src1_ds = gdal.GetDriverByName("MEM").Create(
            "i_have_a_name_but_nobody_can_open_me_through_it", 1, 1
        )
        src1_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
        src1_ds.GetRasterBand(1).Fill(100)

        src2_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
        src2_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
        src2_ds.GetRasterBand(1).Fill(200)

        return src1_ds, src2_ds

    def scenario_1():
        src1_ds, src2_ds = create_sources()
        vrt_ds = gdal.BuildVRT("", [src1_ds, src2_ds], options="-separate")
        vals = struct.unpack("B" * 2, vrt_ds.ReadRaster())
        assert vals == (100, 200)

        vrt_of_vrt_ds = gdal.BuildVRT("", [vrt_ds])
        vals = struct.unpack("B" * 2, vrt_of_vrt_ds.ReadRaster())
        assert vals == (100, 200)

    # Alternate scenario where the Python objects of sources and intermediate
    # VRT are no longer alive when the VRT of VRT is accessed
    def scenario_2():
        def get_vrt_of_vrt():
            src1_ds, src2_ds = create_sources()
            return gdal.BuildVRT(
                "", [gdal.BuildVRT("", [src1_ds, src2_ds], options="-separate")]
            )

        vrt_of_vrt_ds = get_vrt_of_vrt()
        vals = struct.unpack("B" * 2, vrt_of_vrt_ds.ReadRaster())
        assert vals == (100, 200)

    scenario_1()
    scenario_2()


###############################################################################
# Test BuildVRT() with virtual overviews


def test_gdalbuildvrt_lib_virtual_overviews():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1000, 1000)
    src1_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src1_ds.BuildOverviews("NEAR", [2, 4, 8])

    src2_ds = gdal.GetDriverByName("MEM").Create("", 2000, 2000)
    src2_ds.SetGeoTransform([3, 0.001, 0, 49, 0, -0.001])
    src2_ds.BuildOverviews("NEAR", [2, 4, 16])

    vrt_ds = gdal.BuildVRT("", [src1_ds, src2_ds])
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 2


def test_gdalbuildvrt_lib_virtual_overviews_not_same_res():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1000, 1000)
    src1_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src1_ds.BuildOverviews("NEAR", [2, 4])

    src2_ds = gdal.GetDriverByName("MEM").Create("", 500, 500)
    src2_ds.SetGeoTransform([3, 0.002, 0, 49, 0, -0.002])
    src2_ds.BuildOverviews("NEAR", [2, 4])

    vrt_ds = gdal.BuildVRT("", [src1_ds, src2_ds])
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 0


###############################################################################
def test_gdalbuildvrt_lib_separate_nodata(tmp_vsimem):

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1000, 1000)
    src1_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src1_ds.GetRasterBand(1).SetNoDataValue(1)

    src2_ds = gdal.GetDriverByName("MEM").Create("", 1000, 1000)
    src2_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src2_ds.GetRasterBand(1).SetNoDataValue(2)

    gdal.BuildVRT(tmp_vsimem / "out.vrt", [src1_ds, src2_ds], separate=True)

    f = gdal.VSIFOpenL(tmp_vsimem / "out.vrt", "rb")
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    assert b"<NoDataValue>1</NoDataValue>" in data
    assert b"<NODATA>1</NODATA>" in data
    assert b"<NoDataValue>2</NoDataValue>" in data
    assert b"<NODATA>2</NODATA>" in data


###############################################################################
def test_gdalbuildvrt_lib_separate_nodata_2(tmp_vsimem):

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1000, 1000)
    src1_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src1_ds.GetRasterBand(1).SetNoDataValue(1)

    src2_ds = gdal.GetDriverByName("MEM").Create("", 1000, 1000)
    src2_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src2_ds.GetRasterBand(1).SetNoDataValue(2)

    gdal.BuildVRT(
        tmp_vsimem / "out.vrt", [src1_ds, src2_ds], separate=True, srcNodata="3 4"
    )

    f = gdal.VSIFOpenL(tmp_vsimem / "out.vrt", "rb")
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    assert b"<NoDataValue>3</NoDataValue>" in data
    assert b"<NODATA>3</NODATA>" in data
    assert b"<NoDataValue>4</NoDataValue>" in data
    assert b"<NODATA>4</NODATA>" in data


###############################################################################
def test_gdalbuildvrt_lib_separate_nodata_3(tmp_vsimem):

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1000, 1000)
    src1_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src1_ds.GetRasterBand(1).SetNoDataValue(1)

    src2_ds = gdal.GetDriverByName("MEM").Create("", 1000, 1000)
    src2_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src2_ds.GetRasterBand(1).SetNoDataValue(2)

    gdal.BuildVRT(
        tmp_vsimem / "out.vrt",
        [src1_ds, src2_ds],
        separate=True,
        srcNodata="3 4",
        VRTNodata="5 6",
    )

    f = gdal.VSIFOpenL(tmp_vsimem / "out.vrt", "rb")
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    assert b"<NoDataValue>5</NoDataValue>" in data
    assert b"<NODATA>3</NODATA>" in data
    assert b"<NoDataValue>6</NoDataValue>" in data
    assert b"<NODATA>4</NODATA>" in data


###############################################################################
def test_gdalbuildvrt_lib_separate_nodata_4(tmp_vsimem):

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1000, 1000)
    src1_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src1_ds.GetRasterBand(1).SetNoDataValue(1)

    src2_ds = gdal.GetDriverByName("MEM").Create("", 1000, 1000)
    src2_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src2_ds.GetRasterBand(1).SetNoDataValue(2)

    gdal.BuildVRT(
        tmp_vsimem / "out.vrt",
        [src1_ds, src2_ds],
        separate=True,
        srcNodata="None",
        VRTNodata="None",
    )

    f = gdal.VSIFOpenL(tmp_vsimem / "out.vrt", "rb")
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    assert b"<NoDataValue>" not in data
    assert b"<NODATA>" not in data


###############################################################################
def test_gdalbuildvrt_lib_separate_multiband():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    src1_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src1_ds.GetRasterBand(1).Fill(1)
    src1_ds.GetRasterBand(2).Fill(2)

    src2_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3)
    src2_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src2_ds.GetRasterBand(1).Fill(3)
    src2_ds.GetRasterBand(2).Fill(4)
    src2_ds.GetRasterBand(3).Fill(5)

    ds = gdal.BuildVRT(
        "",
        [src1_ds, src2_ds],
        separate=True,
    )
    assert ds.RasterCount == 5
    for i in range(ds.RasterCount):
        assert ds.GetRasterBand(i + 1).Checksum() == i + 1


###############################################################################
def test_gdalbuildvrt_lib_separate_multiband_regular_raster(tmp_vsimem):

    src1_filename = str(tmp_vsimem / "src1.tif")
    src1_ds = gdal.GetDriverByName("GTiff").Create(src1_filename, 1, 1, 2)
    src1_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src1_ds.GetRasterBand(1).Fill(1)
    src1_ds.GetRasterBand(2).Fill(2)
    src1_ds = None

    src2_filename = str(tmp_vsimem / "src2.tif")
    src2_ds = gdal.GetDriverByName("GTiff").Create(src2_filename, 1, 1, 3)
    src2_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src2_ds.GetRasterBand(1).Fill(3)
    src2_ds.GetRasterBand(2).Fill(4)
    src2_ds.GetRasterBand(3).Fill(5)
    src2_ds = None

    ds = gdal.BuildVRT(
        "",
        [src1_filename, src2_filename],
        separate=True,
    )
    assert ds.RasterCount == 5
    for i in range(ds.RasterCount):
        assert ds.GetRasterBand(i + 1).Checksum() == i + 1


###############################################################################
def test_gdalbuildvrt_lib_separate_multiband_band_selection():

    src1_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    src1_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src1_ds.GetRasterBand(1).Fill(1)
    src1_ds.GetRasterBand(2).Fill(2)

    src2_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3)
    src2_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src2_ds.GetRasterBand(1).Fill(3)
    src2_ds.GetRasterBand(2).Fill(4)
    src2_ds.GetRasterBand(3).Fill(5)

    ds = gdal.BuildVRT(
        "",
        [src1_ds, src2_ds],
        separate=True,
        bandList=[2, 1],
    )
    assert ds.RasterCount == 4
    assert ds.GetRasterBand(1).Checksum() == 2
    assert ds.GetRasterBand(2).Checksum() == 1
    assert ds.GetRasterBand(3).Checksum() == 4
    assert ds.GetRasterBand(4).Checksum() == 3


###############################################################################
def test_gdalbuildvrt_lib_separate_multiband_band_selection_error():

    src1_ds = gdal.GetDriverByName("MEM").Create("foo", 1, 1, 2)
    src1_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src1_ds.GetRasterBand(1).Fill(1)
    src1_ds.GetRasterBand(2).Fill(2)

    with gdal.quiet_errors():
        ds = gdal.BuildVRT(
            "",
            [src1_ds],
            separate=True,
            bandList=[1, 2, 3],
        )
    assert ds is None


###############################################################################
def test_gdalbuildvrt_lib_usemaskband_on_mask_band():

    src1_ds = gdal.GetDriverByName("MEM").Create("src1", 3, 1)
    src1_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src1_ds.GetRasterBand(1).Fill(255)
    src1_ds.CreateMaskBand(0)
    src1_ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 1, 1, b"\xff")

    src2_ds = gdal.GetDriverByName("MEM").Create("src2", 3, 1)
    src2_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src2_ds.GetRasterBand(1).Fill(127)
    src2_ds.CreateMaskBand(0)
    src2_ds.GetRasterBand(1).GetMaskBand().WriteRaster(1, 0, 1, 1, b"\xff")

    ds = gdal.BuildVRT("", [src1_ds, src2_ds])
    assert struct.unpack("B" * 3, ds.ReadRaster()) == (255, 127, 0)
    assert struct.unpack("B" * 3, ds.GetRasterBand(1).GetMaskBand().ReadRaster()) == (
        255,
        255,
        0,
    )


###############################################################################
def test_gdalbuildvrt_lib_usemaskband_on_alpha_band():

    src1_ds = gdal.GetDriverByName("MEM").Create("src1", 3, 1, 2)
    src1_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src1_ds.GetRasterBand(1).Fill(255)
    src1_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    src1_ds.GetRasterBand(2).WriteRaster(0, 0, 1, 1, b"\xff")

    src2_ds = gdal.GetDriverByName("MEM").Create("src2", 3, 1, 2)
    src2_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src2_ds.GetRasterBand(1).Fill(127)
    src2_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    src2_ds.GetRasterBand(2).WriteRaster(1, 0, 1, 1, b"\xff")

    ds = gdal.BuildVRT("", [src1_ds, src2_ds])
    assert struct.unpack("B" * 3, ds.GetRasterBand(1).ReadRaster()) == (255, 127, 0)
    assert struct.unpack("B" * 3, ds.GetRasterBand(2).ReadRaster()) == (255, 255, 0)


###############################################################################
# Test parsing all resampling methods


@pytest.mark.parametrize(
    "resampleAlg,resampleAlgStr",
    [
        (gdal.GRIORA_NearestNeighbour, "near"),
        (gdal.GRIORA_Cubic, "cubic"),
        (gdal.GRIORA_CubicSpline, "cubicspline"),
        (gdal.GRIORA_Lanczos, "lanczos"),
        (gdal.GRIORA_Average, "average"),
        (gdal.GRIORA_RMS, "rms"),
        (gdal.GRIORA_Mode, "mode"),
        (gdal.GRIORA_Gauss, "gauss"),
    ],
)
def test_gdalbuildvrt_lib_resampling_methods(resampleAlg, resampleAlgStr):

    option_list = gdal.BuildVRTOptions(
        resampleAlg=resampleAlg, options="__RETURN_OPTION_LIST__"
    )
    assert option_list == ["-r", resampleAlgStr]
    assert (
        gdal.BuildVRT("", "../gcore/data/byte.tif", resampleAlg=resampleAlg) is not None
    )


###############################################################################
def test_gdalbuildvrt_lib_bandList():

    src_ds = gdal.GetDriverByName("MEM").Create("src1", 3, 1, 2)
    src_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.GetRasterBand(2).Fill(0)

    vrt_ds = gdal.BuildVRT("", [src_ds], bandList=[2, 1] * 100)
    assert vrt_ds.GetRasterBand(1).Checksum() == 0
    assert vrt_ds.GetRasterBand(2).Checksum() != 0
    assert vrt_ds.GetRasterBand(3).Checksum() == 0

    with gdal.quiet_errors():
        assert gdal.BuildVRT("", [src_ds], bandList=[3]) is None

    src2_ds = gdal.GetDriverByName("MEM").Create("src2", 3, 1, 3)
    src2_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])

    # If no explicit band list, we require all sources to have the same
    # number of bands
    with gdaltest.disable_exceptions(), gdaltest.error_handler():
        assert gdal.BuildVRT("", [src_ds, src2_ds]) is not None
        assert gdal.GetLastErrorType() != 0

    with gdaltest.disable_exceptions(), gdaltest.error_handler():
        gdal.ErrorReset()
        assert gdal.BuildVRT("", [src2_ds, src_ds]) is not None
        assert gdal.GetLastErrorType() != 0

    # If explicit band list, we tolerate different band count, provided that
    # all requested bands are available.
    gdal.ErrorReset()
    assert gdal.BuildVRT("", [src_ds, src2_ds], bandList=[1, 2]) is not None
    assert gdal.GetLastErrorType() == 0

    gdal.ErrorReset()
    assert gdal.BuildVRT("", [src2_ds, src_ds], bandList=[1, 2]) is not None
    assert gdal.GetLastErrorType() == 0


###############################################################################


def test_gdalbuildvrt_lib_bandList_subset_of_bands_from_multiple_band_source():

    src_ds = gdal.GetDriverByName("MEM").Create("src", 1, 1, 3)
    src_ds.SetGeoTransform([2, 0.001, 0, 49, 0, -0.001])
    src_ds.GetRasterBand(1).Fill(10)
    src_ds.GetRasterBand(2).Fill(20)
    src_ds.GetRasterBand(3).Fill(30)

    vrt_ds = gdal.BuildVRT("", [src_ds], bandList=[1])
    assert struct.unpack("B", vrt_ds.GetRasterBand(1).ReadRaster())[0] == 10

    vrt_ds = gdal.BuildVRT("", [src_ds], bandList=[2])
    assert struct.unpack("B", vrt_ds.GetRasterBand(1).ReadRaster())[0] == 20

    vrt_ds = gdal.BuildVRT("", [src_ds], bandList=[3])
    assert struct.unpack("B", vrt_ds.GetRasterBand(1).ReadRaster())[0] == 30

    vrt_ds = gdal.BuildVRT("", [src_ds], bandList=[1, 2])
    assert struct.unpack("B", vrt_ds.GetRasterBand(1).ReadRaster())[0] == 10
    assert struct.unpack("B", vrt_ds.GetRasterBand(2).ReadRaster())[0] == 20

    vrt_ds = gdal.BuildVRT("", [src_ds], bandList=[1, 3])
    assert struct.unpack("B", vrt_ds.GetRasterBand(1).ReadRaster())[0] == 10
    assert struct.unpack("B", vrt_ds.GetRasterBand(2).ReadRaster())[0] == 30

    vrt_ds = gdal.BuildVRT("", [src_ds], bandList=[1, 2, 3])
    assert struct.unpack("B", vrt_ds.GetRasterBand(1).ReadRaster())[0] == 10
    assert struct.unpack("B", vrt_ds.GetRasterBand(2).ReadRaster())[0] == 20
    assert struct.unpack("B", vrt_ds.GetRasterBand(3).ReadRaster())[0] == 30

    vrt_ds = gdal.BuildVRT("", [src_ds], bandList=[1, 3, 2])
    assert struct.unpack("B", vrt_ds.GetRasterBand(1).ReadRaster())[0] == 10
    assert struct.unpack("B", vrt_ds.GetRasterBand(2).ReadRaster())[0] == 30
    assert struct.unpack("B", vrt_ds.GetRasterBand(3).ReadRaster())[0] == 20

    vrt_ds = gdal.BuildVRT("", [src_ds], bandList=[2, 1])
    assert struct.unpack("B", vrt_ds.GetRasterBand(1).ReadRaster())[0] == 20
    assert struct.unpack("B", vrt_ds.GetRasterBand(2).ReadRaster())[0] == 10

    vrt_ds = gdal.BuildVRT("", [src_ds], bandList=[2, 3])
    assert struct.unpack("B", vrt_ds.GetRasterBand(1).ReadRaster())[0] == 20
    assert struct.unpack("B", vrt_ds.GetRasterBand(2).ReadRaster())[0] == 30

    vrt_ds = gdal.BuildVRT("", [src_ds], bandList=[3, 1])
    assert struct.unpack("B", vrt_ds.GetRasterBand(1).ReadRaster())[0] == 30
    assert struct.unpack("B", vrt_ds.GetRasterBand(2).ReadRaster())[0] == 10

    vrt_ds = gdal.BuildVRT("", [src_ds], bandList=[3, 2])
    assert struct.unpack("B", vrt_ds.GetRasterBand(1).ReadRaster())[0] == 30
    assert struct.unpack("B", vrt_ds.GetRasterBand(2).ReadRaster())[0] == 20


###############################################################################
def test_gdalbuildvrt_lib_warnings_and_custom_error_handler():
    class GdalErrorHandler(object):
        def __init__(self):
            self.got_failure = False
            self.got_warning = False

        def handler(self, err_level, err_no, err_msg):
            if err_level == gdal.CE_Failure:
                self.got_failure = True
            elif err_level == gdal.CE_Warning:
                self.got_warning = True

    # Heterogeneous band numbers should result in a warning from BuildVRT()
    ds_one_band = gdal.Open("../gcore/data/byte.tif")
    ds_two_band = gdal.Translate("", ds_one_band, bandList=[1, 1], format="VRT")

    err_handler = GdalErrorHandler()
    with gdaltest.error_handler(err_handler.handler):
        with gdal.ExceptionMgr():
            vrt_ds = gdal.BuildVRT("", [ds_one_band, ds_two_band])
    assert vrt_ds
    assert not err_handler.got_failure
    assert err_handler.got_warning

    err_handler = GdalErrorHandler()
    with gdaltest.error_handler(err_handler.handler):
        with gdal.ExceptionMgr():
            vrt_ds = gdal.BuildVRT("", [ds_two_band, ds_one_band])
    assert vrt_ds
    assert not err_handler.got_failure
    assert err_handler.got_warning


###############################################################################
def test_gdalbuildvrt_lib_strict_mode():

    with gdal.ExceptionMgr():
        with gdal.quiet_errors():
            assert (
                gdal.BuildVRT(
                    "", ["../gcore/data/byte.tif", "i_dont_exist.tif"], strict=False
                )
                is not None
            )

    with gdal.ExceptionMgr():
        with pytest.raises(Exception):
            gdal.BuildVRT(
                "", ["../gcore/data/byte.tif", "i_dont_exist.tif"], strict=True
            )


###############################################################################
def test_gdalbuildvrt_lib_te_touching_on_edge(tmp_vsimem):

    tmp_filename = tmp_vsimem / "test_gdalbuildvrt_lib_te_touching_on_edge.vrt"
    ds = gdal.BuildVRT(
        tmp_filename,
        "../gcore/data/byte.tif",
        outputBounds=[440600, 3750000, 440720, 3750120],
        xRes=60,
        yRes=60,
    )
    assert ds is not None
    ds = None

    ds = gdal.Open(tmp_filename)
    assert ds.GetRasterBand(1).Checksum() == 0
    ds = None


###############################################################################
@pytest.mark.parametrize("num_bands_1,num_bands_2", [(3, 3), (3, 4), (4, 3), (4, 4)])
@pytest.mark.parametrize("drv_name", ["MEM", "GTiff"])
def test_gdalbuildvrt_lib_addAlpha(tmp_vsimem, num_bands_1, num_bands_2, drv_name):
    fname1 = tmp_vsimem / "test_gdalbuildvrt_lib_addAlpha_1.tif"
    fname2 = tmp_vsimem / "test_gdalbuildvrt_lib_addAlpha_2.tif"

    src_ds1 = gdal.GetDriverByName(drv_name).Create(fname1, 1, 1, num_bands_1)
    if num_bands_1 == 4:
        src_ds1.GetRasterBand(src_ds1.RasterCount).SetColorInterpretation(
            gdal.GCI_AlphaBand
        )
    for i in range(src_ds1.RasterCount):
        src_ds1.GetRasterBand(i + 1).Fill(i + 1)
    src_ds1.SetGeoTransform([2, 1, 0, 49, 0, -1])

    src_ds2 = gdal.GetDriverByName(drv_name).Create(fname2, 1, 1, num_bands_2)
    if num_bands_2 == 4:
        src_ds2.GetRasterBand(src_ds2.RasterCount).SetColorInterpretation(
            gdal.GCI_AlphaBand
        )
    for i in range(src_ds2.RasterCount):
        src_ds2.GetRasterBand(i + 1).Fill(i + 1)
    src_ds2.SetGeoTransform([3, 1, 0, 49, 0, -1])

    if drv_name == "MEM":
        ds = gdal.BuildVRT("", [src_ds1, src_ds2], addAlpha=True)
    else:
        src_ds1 = None
        src_ds2 = None
        ds = gdal.BuildVRT("", [fname1, fname2], addAlpha=True)
    assert ds.RasterCount == 4
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(1).ReadRaster() == b"\x01\x01"
    assert ds.GetRasterBand(2).ReadRaster() == b"\x02\x02"
    assert ds.GetRasterBand(3).ReadRaster() == b"\x03\x03"
    assert ds.GetRasterBand(4).ReadRaster() == (
        b"\xff" if num_bands_1 == 3 else b"\x04"
    ) + (b"\xff" if num_bands_2 == 3 else b"\x04")


###############################################################################


def test_gdalbuildvrt_lib_stable_average():
    """Tests that averaging resolution is stable. Cf https://github.com/OSGeo/gdal/issues/7502"""

    gt = (
        -5570.248248450553,
        3.0004031511048237,
        0.0,
        5570.248248450553,
        0.0,
        -3.0004031511048237,
    )
    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float32)
    ds.SetGeoTransform(gt)

    vrt_ds = gdal.BuildVRT("", [ds] * 1000, separate=False)
    vrt_gt = vrt_ds.GetGeoTransform()

    assert vrt_gt == gt
