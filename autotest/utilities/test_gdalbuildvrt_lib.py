#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test librarified gdalbuildvrt
# Author:   Even Rouault <even dot rouault @ spatialys dot com>
#
###############################################################################
# Copyright (c) 2016, Even Rouault <even dot rouault @ spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import pathlib
import struct

import gdaltest
import pytest

from osgeo import gdal, osr

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


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
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
        tmp_vsimem / "out.vrt", [src1_ds, src2_ds], separate=True, srcNodata="-3 4"
    )

    f = gdal.VSIFOpenL(tmp_vsimem / "out.vrt", "rb")
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    assert b"<NoDataValue>-3</NoDataValue>" in data
    assert b"<NODATA>-3</NODATA>" in data
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
        VRTNodata="-5 6",
    )

    f = gdal.VSIFOpenL(tmp_vsimem / "out.vrt", "rb")
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)

    assert b"<NoDataValue>-5</NoDataValue>" in data
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
    class GdalErrorHandler:
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


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
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


###############################################################################


def test_gdalbuildvrt_lib_nodataMaxMaskThreshold_rgba(tmp_vsimem):

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 4)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    # Test remapping of second valid pixel at 0 to 1
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\x00")
    ds.GetRasterBand(2).WriteRaster(0, 0, 2, 1, b"\x02\x02")
    ds.GetRasterBand(3).WriteRaster(0, 0, 2, 1, b"\x03\x03")
    ds.GetRasterBand(4).WriteRaster(0, 0, 2, 1, b"\x00\xff")
    ds.GetRasterBand(4).SetColorInterpretation(gdal.GCI_AlphaBand)

    vrt_ds = gdal.BuildVRT("", [ds], nodataMaxMaskThreshold=128, VRTNodata=0)
    assert vrt_ds.RasterCount == 3
    assert vrt_ds.GetRasterBand(1).GetNoDataValue() == 0
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x00\x01"
    assert vrt_ds.GetRasterBand(2).GetNoDataValue() == 0
    assert vrt_ds.GetRasterBand(2).ReadRaster() == b"\x00\x02"
    assert vrt_ds.GetRasterBand(3).GetNoDataValue() == 0
    assert vrt_ds.GetRasterBand(3).ReadRaster() == b"\x00\x03"

    assert struct.unpack(
        "h" * 2, vrt_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_Int16)
    ) == (0, 1)

    vrt_ds = gdal.BuildVRT("", [ds], nodataMaxMaskThreshold=128.5, VRTNodata=0)
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x00\x01"

    # VRTNodata=255, test remapping of 255 to 254
    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\xff")
    ds.GetRasterBand(2).WriteRaster(0, 0, 2, 1, b"\x00\xff")
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)

    vrt_ds = gdal.BuildVRT("", [ds], nodataMaxMaskThreshold=128, VRTNodata=255)
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\xff\xfe"


###############################################################################


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalbuildvrt_lib_nodataMaxMaskThreshold_rgb_mask(tmp_vsimem):

    # UInt16, VRTNodata=0
    src_filename = str(tmp_vsimem / "src.tif")
    ds = gdal.GetDriverByName("GTiff").Create(src_filename, 2, 1, 3, gdal.GDT_UInt16)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, struct.pack("H" * 2, 1, 0))
    ds.GetRasterBand(2).WriteRaster(0, 0, 2, 1, struct.pack("H" * 2, 2, 2))
    ds.GetRasterBand(3).WriteRaster(0, 0, 2, 1, struct.pack("H" * 2, 3, 2))
    ds.GetRasterBand(1).CreateMaskBand(gdal.GMF_PER_DATASET)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 2, 1, b"\x00\xff")
    ds.Close()

    vrt_filename = str(tmp_vsimem / "test.vrt")
    gdal.BuildVRT(vrt_filename, [src_filename], nodataMaxMaskThreshold=128, VRTNodata=0)
    vrt_ds = gdal.Open(vrt_filename)
    assert struct.unpack(
        "H" * 2, vrt_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_UInt16)
    ) == (0, 1)

    assert struct.unpack(
        "B" * 2, vrt_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_UInt8)
    ) == (0, 1)

    # UInt16, VRTNodata=65535
    src_filename = str(tmp_vsimem / "src.tif")
    ds = gdal.GetDriverByName("GTiff").Create(src_filename, 2, 1, 1, gdal.GDT_UInt16)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, struct.pack("H" * 2, 1, 65535))
    ds.GetRasterBand(1).CreateMaskBand(gdal.GMF_PER_DATASET)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 2, 1, b"\x00\xff")
    ds.Close()

    vrt_filename = str(tmp_vsimem / "test.vrt")
    gdal.BuildVRT(
        vrt_filename, [src_filename], nodataMaxMaskThreshold=128, VRTNodata=65535
    )
    vrt_ds = gdal.Open(vrt_filename)
    assert struct.unpack(
        "H" * 2, vrt_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_UInt16)
    ) == (65535, 65534)

    # Int16, VRTNodata=-32768
    src_filename = str(tmp_vsimem / "src.tif")
    ds = gdal.GetDriverByName("GTiff").Create(src_filename, 2, 1, 1, gdal.GDT_Int16)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, struct.pack("h" * 2, 1, -32768))
    ds.GetRasterBand(1).CreateMaskBand(gdal.GMF_PER_DATASET)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 2, 1, b"\x00\xff")
    ds.Close()

    vrt_filename = str(tmp_vsimem / "test.vrt")
    gdal.BuildVRT(
        vrt_filename, [src_filename], nodataMaxMaskThreshold=128, VRTNodata=-32768
    )
    vrt_ds = gdal.Open(vrt_filename)
    assert struct.unpack(
        "h" * 2, vrt_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_Int16)
    ) == (-32768, -32767)

    # Int16, VRTNodata=32767
    src_filename = str(tmp_vsimem / "src.tif")
    ds = gdal.GetDriverByName("GTiff").Create(src_filename, 2, 1, 1, gdal.GDT_Int16)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, struct.pack("h" * 2, 1, 32767))
    ds.GetRasterBand(1).CreateMaskBand(gdal.GMF_PER_DATASET)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 2, 1, b"\x00\xff")
    ds.Close()

    vrt_filename = str(tmp_vsimem / "test.vrt")
    gdal.BuildVRT(
        vrt_filename, [src_filename], nodataMaxMaskThreshold=128, VRTNodata=32767
    )
    vrt_ds = gdal.Open(vrt_filename)
    assert struct.unpack(
        "h" * 2, vrt_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_Int16)
    ) == (32767, 32766)

    # Float32, VRTNodata=0
    src_filename = str(tmp_vsimem / "src.tif")
    ds = gdal.GetDriverByName("GTiff").Create(src_filename, 2, 1, 1, gdal.GDT_Float32)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, struct.pack("f" * 2, 1, 0))
    ds.GetRasterBand(1).CreateMaskBand(gdal.GMF_PER_DATASET)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 2, 1, b"\x00\xff")
    ds.Close()

    vrt_filename = str(tmp_vsimem / "test.vrt")
    gdal.BuildVRT(vrt_filename, [src_filename], nodataMaxMaskThreshold=128, VRTNodata=0)
    vrt_ds = gdal.Open(vrt_filename)
    assert struct.unpack(
        "f" * 2, vrt_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_Float32)
    ) == pytest.approx((0.0, 0.001))

    # Float32, VRTNodata=1
    src_filename = str(tmp_vsimem / "src.tif")
    ds = gdal.GetDriverByName("GTiff").Create(src_filename, 3, 1, 1, gdal.GDT_Float32)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 3, 1, struct.pack("f" * 3, 0, 1, 2))
    ds.GetRasterBand(1).CreateMaskBand(gdal.GMF_PER_DATASET)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 3, 1, b"\x00\xff\xff")
    ds.Close()

    vrt_filename = str(tmp_vsimem / "test.vrt")
    gdal.BuildVRT(vrt_filename, [src_filename], nodataMaxMaskThreshold=128, VRTNodata=1)
    vrt_ds = gdal.Open(vrt_filename)
    assert struct.unpack(
        "f" * 3, vrt_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_Float32)
    ) == pytest.approx((1.0, 1.001, 2.0))


###############################################################################


@pytest.mark.parametrize(
    "dtype,nodata",
    [
        (gdal.GDT_UInt8, float("nan")),
        (gdal.GDT_UInt16, -1),
    ],
)
def test_gdalbuildvrt_lib_nodata_invalid(tmp_vsimem, dtype, nodata):

    drv = gdal.GetDriverByName("GTiff")
    with drv.Create(tmp_vsimem / "in.tif", 1, 1, eType=dtype) as ds:
        ds.GetRasterBand(1).Fill(1)
        ds.SetGeoTransform((0, 1, 0, 1, 0, -1))

    with gdaltest.error_raised(
        gdal.CE_Warning, "cannot represent the specified NoData value"
    ):
        gdal.BuildVRT("", [tmp_vsimem / "in.tif"], VRTNodata=nodata)


###############################################################################


# Test --resolution=common
# Also tested by C++ test_cpl.CPLGreatestCommonDivisor
@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "resolutions,expected",
    [
        ([5 / 3600, 3 / 3600], 1 / 3600),
        ([5, 3], 1),
        ([5 / 3600, 2.5 / 3600], 2.5 / 3600),
        ([1 / 10, 1], 1 / 10),
        ([1 / 10, 1 / 3], 1 / 30),
        ([1 / 17, 1 / 13], 1 / 221),
        ([1 / 17, 1 / 3600], 1 / 61200),
        ([2.9999999, 3], "common resolution"),
    ],
)
def test_gdalbuildvrt_resolution_common(tmp_vsimem, resolutions, expected):

    inputs = []

    width = 5
    height = 5

    for i, res in enumerate(resolutions):
        fname = tmp_vsimem / f"in_{i}.tif"
        inputs.append(fname)

        nx = round(width / res)
        ny = round(height / res)

        with gdal.GetDriverByName("GTiff").Create(fname, nx, ny) as ds:
            ds.SetGeoTransform((0, res, 0, height, 0, -res))

    if type(expected) is str:
        with pytest.raises(Exception, match=expected):
            gdal.BuildVRT("", inputs, resolution="common", strict=True)
    else:
        with gdal.BuildVRT("", inputs, resolution="common", strict=True) as ds:
            gt = ds.GetGeoTransform()
            assert gt[1] == expected
            assert -gt[5] == expected


###############################################################################


@pytest.mark.parametrize("separate", [True, False])
def test_gdalbuildvrt_write_absolute_path_from_absolute_path(tmp_vsimem, separate):

    gdal.Translate(
        tmp_vsimem / "byte.tif", "../gcore/data/byte.tif", options="-b 1 -mask 1"
    )
    gdal.BuildVRT(
        tmp_vsimem / "out.vrt",
        [tmp_vsimem / "byte.tif"],
        writeAbsolutePath=True,
        separate=separate,
    )
    with gdal.Open(tmp_vsimem / "out.vrt") as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672
    with gdal.VSIFile(tmp_vsimem / "out.vrt", "rb") as f:
        content = f.read().decode("utf-8")
    assert (
        '<SourceFilename relativeToVRT="0">' + str(tmp_vsimem / "byte.tif") in content
    )
    assert '<SourceFilename relativeToVRT="1">byte.tif' not in content


###############################################################################


@pytest.mark.parametrize("separate", [True, False])
def test_gdalbuildvrt_write_absolute_path_from_relative_path(tmp_path, separate):

    old_curdir = os.getcwd()
    try:
        os.chdir(tmp_path)
        gdal.Translate("byte.tif", os.path.join(old_curdir, "../gcore/data/byte.tif"))
        gdal.BuildVRT(
            "out.vrt", ["byte.tif"], writeAbsolutePath=True, separate=separate
        )
        with gdal.Open("out.vrt") as ds:
            assert ds.GetRasterBand(1).Checksum() == 4672
        with gdal.VSIFile("out.vrt", "rb") as f:
            content = f.read().decode("utf-8").replace("\\", "/")
        assert (
            '<SourceFilename relativeToVRT="0">'
            + str(tmp_path / "byte.tif").replace("\\", "/")
            in content
        )
        assert '<SourceFilename relativeToVRT="1">byte.tif' not in content

    finally:
        os.chdir(old_curdir)


###############################################################################


@pytest.mark.parametrize(
    "pixfn,args,expected",
    [
        ("sum", None, [[1, 1, 3, 2, 2]]),
        # next test is disabled - pixel function can't distinguish between
        # "source not present" and "source present with NoData value"
        # ("sum", "propagateNoData=True", [[1, 99, 3, 99, 2]]),
    ],
)
def test_gdalbuildvrt_pixel_function(tmp_vsimem, pixfn, args, expected):

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src1.tif", 4, 1) as src1_ds:
        src1_ds.SetGeoTransform([0, 1, 0, 1, 0, -1])
        src1_ds.GetRasterBand(1).WriteArray(np.array([[1, 1, 1, 99]]))

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src2.tif", 4, 1) as src2_ds:
        src2_ds.SetGeoTransform([1, 1, 0, 1, 0, -1])
        src2_ds.GetRasterBand(1).WriteArray(np.array([[99, 2, 2, 2]]))

    with gdal.BuildVRT(
        "",
        [tmp_vsimem / "src1.tif", tmp_vsimem / "src2.tif"],
        pixelFunction=pixfn,
        pixelFunctionArgs=args,
        VRTNodata=99,
    ) as ds:
        dst_values = ds.ReadAsArray()
        np.testing.assert_array_equal(dst_values, expected)


def test_gdalbuildvrt_pixel_function_invalid_args():

    # test -pixel-function exclusive with -separate
    with pytest.raises(RuntimeError, match="Argument .* not allowed"):
        gdal.BuildVRT(
            "",
            "../gcore/data/byte.tif",
            pixelFunction="sum",
            separate=True,
        )


def test_gdalbuildvrt_pixel_function_invalid():

    with pytest.raises(RuntimeError, match="not a registered pixel function"):
        gdal.BuildVRT("", "../gcore/data/byte.tif", pixelFunction="does_not_exist")


def test_gdalbuildvrt_pixel_function_arg_no_pixel_function():

    with pytest.raises(
        RuntimeError, match="arguments provided without a pixel function"
    ):
        gdal.BuildVRT("", "../gcore/data/byte.tif", pixelFunctionArgs={"k": 7})


def test_gdalbuildvrt_lib_source_had_ds_mask_band_and_addalpha():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.GetRasterBand(1).Fill(255)
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    src_ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 1, 1, b"\xff")

    vrt_ds = gdal.BuildVRT("", src_ds, addAlpha=True)
    assert vrt_ds.GetRasterBand(1).ReadRaster() == src_ds.GetRasterBand(1).ReadRaster()
    assert vrt_ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert (
        vrt_ds.GetRasterBand(2).ReadRaster()
        == src_ds.GetRasterBand(1).GetMaskBand().ReadRaster()
    )


@pytest.mark.parametrize(
    "desc1,desc2,metadata1,metadata2,expected_desc,expected_metadata",
    [
        (
            "",
            "",
            {},
            {},
            "",
            {},
        ),
        (
            "desc1",
            "desc1",
            {},
            {},
            "desc1",
            {},
        ),
        (
            "desc1",
            "desc2",
            {},
            {},
            "",
            {},
        ),
        # Metadata tests
        (
            "",
            "",
            {"KEY1": "VALUE1"},
            {"KEY1": "VALUE1"},
            "",
            {"KEY1": "VALUE1"},
        ),
        (
            "",
            "",
            {"KEY1": "VALUE1"},
            {"KEY1": "DIFFERENT_VALUE"},
            "",
            {},
        ),
        (
            "",
            "",
            {"KEY1": "VALUE1", "KEY2": "VALUE2"},
            {"KEY2": "VALUE2", "KEY3": "VALUE3"},
            "",
            {"KEY2": "VALUE2"},
        ),
        (
            "",
            "",
            {"KEY1": "VALUE1"},
            {"KEY2": "VALUE2", "KEY3": "VALUE3"},
            "",
            {},
        ),
    ],
)
def test_gdalbuildvrt_preserve_band_metadata_normal_mode(
    tmp_vsimem, desc1, desc2, metadata1, metadata2, expected_desc, expected_metadata
):

    output_vrt = str(tmp_vsimem / "test_gdalbuildvrt_17.vrt")

    # Create a 10x10 sample tif with band description
    drv = gdal.GetDriverByName("GTiff")
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS84")
    wkt = srs.ExportToWkt()
    sample_tif = str(tmp_vsimem / "test_gdalbuildvrt_17.tif")
    with drv.Create(sample_tif, 10, 10, 1) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
        ds.GetRasterBand(1).SetDescription(desc1)
        for mdkey, mdval in metadata1.items():
            ds.GetRasterBand(1).SetMetadataItem(mdkey, mdval)
        ds.GetRasterBand(1).Fill(255)

    # Create a second raster without band description
    sample_tif2 = str(tmp_vsimem / "test_gdalbuildvrt_17_2.tif")
    with drv.Create(sample_tif2, 10, 10, 1) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([3, 0.1, 0, 49, 0, -0.1])
        ds.GetRasterBand(1).SetDescription(desc2)
        for mdkey, mdval in metadata2.items():
            ds.GetRasterBand(1).SetMetadataItem(mdkey, mdval)
        ds.GetRasterBand(1).Fill(127)

    # Build VRT
    rt_ds = gdal.BuildVRT(output_vrt, [sample_tif, sample_tif2])

    desc = rt_ds.GetRasterBand(1).GetDescription()

    assert desc == expected_desc
    assert rt_ds.GetRasterBand(1).GetMetadata_Dict() == expected_metadata


@pytest.mark.parametrize(
    "desc1,desc2,metadata1,metadata2,expected_desc,expected_metadata",
    [
        (
            "",
            "",
            {},
            {},
            ["", ""],
            {},
        ),
        (
            "desc1",
            "desc1",
            {},
            {},
            ["desc1", "desc1"],
            {},
        ),
        (
            "desc1",
            "desc2",
            {},
            {},
            ["desc1", "desc2"],
            {},
        ),
        # Metadata tests
        (
            "",
            "",
            {"KEY1": "VALUE1"},
            {"KEY2": "VALUE2"},
            ["", ""],
            {"KEY1": "VALUE1", "KEY2": "VALUE2"},
        ),
    ],
)
def test_gdalbuildvrt_preserve_band_metadata_separate_mode(
    tmp_vsimem, desc1, desc2, metadata1, metadata2, expected_desc, expected_metadata
):

    output_vrt = str(tmp_vsimem / "test_gdalbuildvrt_18.vrt")

    # Create a 10x10 sample tif with band description
    drv = gdal.GetDriverByName("GTiff")
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS("WGS84")
    wkt = srs.ExportToWkt()
    sample_tif = str(tmp_vsimem / "test_gdalbuildvrt_18.tif")
    with drv.Create(sample_tif, 10, 10, 1) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
        ds.GetRasterBand(1).SetDescription(desc1)
        for mdkey, mdval in metadata1.items():
            ds.GetRasterBand(1).SetMetadataItem(mdkey, mdval)
        ds.GetRasterBand(1).Fill(255)

    # Create a second raster without band description
    sample_tif2 = str(tmp_vsimem / "test_gdalbuildvrt_18_2.tif")
    with drv.Create(sample_tif2, 10, 10, 1) as ds:
        ds.SetProjection(wkt)
        ds.SetGeoTransform([3, 0.1, 0, 49, 0, -0.1])
        ds.GetRasterBand(1).SetDescription(desc2)
        for mdkey, mdval in metadata2.items():
            ds.GetRasterBand(1).SetMetadataItem(mdkey, mdval)
        ds.GetRasterBand(1).Fill(127)

    # Build VRT
    rt_ds = gdal.BuildVRT(output_vrt, [sample_tif, sample_tif2], separate=True)

    for i in range(2):
        desc = rt_ds.GetRasterBand(i + 1).GetDescription()

        if i == 0:
            expected_desc_i = desc1
            expected_metadata_i = metadata1
        else:
            expected_desc_i = desc2
            expected_metadata_i = metadata2

        assert desc == expected_desc_i
        assert rt_ds.GetRasterBand(i + 1).GetMetadata_Dict() == expected_metadata_i
