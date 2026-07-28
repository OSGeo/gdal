#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test librarified gdal_translate
# Author:   Faza Mahamood <fazamhd at gmail dot com>
#
###############################################################################
# Copyright (c) 2015, Faza Mahamood <fazamhd at gmail dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import collections
import os
import shutil
import struct

import gdaltest
import pytest

from osgeo import gdal, osr

###############################################################################
# Simple test


def test_gdal_translate_lib_1(tmp_path):

    dst_tif = tmp_path / "test1.tif"

    ds = gdal.Open("../gcore/data/byte.tif")

    ds = gdal.Translate(dst_tif, ds)
    assert ds is not None, "got error/warning"

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test error case of argument parser


def test_gdal_translate_lib_error_case_arg_parser(tmp_vsimem):

    dst_tif = tmp_vsimem / "test_gdal_translate_lib_error_case_arg_parser.tif"

    with pytest.raises(Exception, match="Zero positional arguments expected"):
        gdal.Translate(
            dst_tif, "../gcore/data/byte.tif", options="unexpected_positional"
        )


###############################################################################
# Test format option and callback


def mycallback(pct, msg, user_data):
    # pylint: disable=unused-argument
    user_data[0] = pct
    return 1


def test_gdal_translate_lib_2(tmp_path):

    dst_tif = str(tmp_path / "test2.tif")

    src_ds = gdal.Open("../gcore/data/byte.tif")
    tab = [0]
    ds = gdal.Translate(
        dst_tif, src_ds, format="GTiff", callback=mycallback, callback_data=tab
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    assert tab[0] == 1.0, "Bad percentage"

    ds = None


###############################################################################
# Test outputType option


def test_gdal_translate_lib_3(tmp_path):

    dst_tif = str(tmp_path / "test3.tif")

    ds = gdal.Open("../gcore/data/byte.tif")
    ds = gdal.Translate(dst_tif, ds, outputType=gdal.GDT_Int16)
    assert ds is not None

    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int16, "Bad data type"

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test bandList option


def test_gdal_translate_lib_4(tmp_path):

    dst_tif = str(tmp_path / "test4.tif")

    ds = gdal.Open("../gcore/data/rgbsmall.tif")

    ds = gdal.Translate(dst_tif, ds, bandList=[3, 2, 1])
    assert ds is not None, "got error/warning"

    assert ds.GetRasterBand(1).Checksum() == 21349, "Bad checksum"

    assert ds.GetRasterBand(2).Checksum() == 21053, "Bad checksum"

    assert ds.GetRasterBand(3).Checksum() == 21212, "Bad checksum"

    ds = None


###############################################################################
# Test rgbExpand option


@pytest.mark.require_driver("GIF")
def test_gdal_translate_lib_5(tmp_path):

    dst_tif = str(tmp_path / "test5.tif")

    ds = gdal.Open("../gdrivers/data/gif/bug407.gif")
    ds = gdal.Translate(dst_tif, ds, rgbExpand="rgb")
    assert ds is not None

    assert (
        ds.GetRasterBand(1).GetRasterColorInterpretation() == gdal.GCI_RedBand
    ), "Bad color interpretation"

    assert (
        ds.GetRasterBand(2).GetRasterColorInterpretation() == gdal.GCI_GreenBand
    ), "Bad color interpretation"

    assert (
        ds.GetRasterBand(3).GetRasterColorInterpretation() == gdal.GCI_BlueBand
    ), "Bad color interpretation"

    assert ds.GetRasterBand(1).Checksum() == 20615, "Bad checksum"

    assert ds.GetRasterBand(2).Checksum() == 59147, "Bad checksum"

    assert ds.GetRasterBand(3).Checksum() == 63052, "Bad checksum"

    ds = None


###############################################################################
# Test oXSizePixel and oYSizePixel option


def test_gdal_translate_lib_6(tmp_path):

    dst_tif = str(tmp_path / "test6.tif")

    ds = gdal.Open("../gcore/data/byte.tif")
    ds = gdal.Translate(dst_tif, ds, width=40, height=40)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18784, "Bad checksum"

    ds = None


###############################################################################
# Test oXSizePct and oYSizePct option


def test_gdal_translate_lib_7(tmp_vsimem):

    dst_tif = tmp_vsimem / "test7.tif"

    ds = gdal.Open("../gcore/data/byte.tif")
    ds = gdal.Translate(dst_tif, ds, widthPct=200.0, heightPct=200.0)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18784, "Bad checksum"


def test_gdal_translate_lib_7_error(tmp_vsimem):

    with pytest.raises(Exception, match="Invalid output width"):
        gdal.Translate(
            tmp_vsimem / "out.tif",
            "../gcore/data/byte.tif",
            widthPct=1,
            heightPct=200.0,
        )

    with pytest.raises(Exception, match="Invalid output height"):
        gdal.Translate(
            tmp_vsimem / "out.tif", "../gcore/data/byte.tif", widthPct=200, heightPct=1
        )


###############################################################################
# Test outputSRS and GCPs options


def test_gdal_translate_lib_8(tmp_path):

    dst_tif = str(tmp_path / "test8.tif")

    gcpList = [
        gdal.GCP(440720.000, 3751320.000, 0, 0, 0),
        gdal.GCP(441920.000, 3751320.000, 0, 20, 0),
        gdal.GCP(441920.000, 3750120.000, 0, 20, 20),
        gdal.GCP(440720.000, 3750120.000, 0, 0, 20),
    ]
    ds = gdal.Open("../gcore/data/byte.tif")
    ds = gdal.Translate(dst_tif, ds, outputSRS="EPSG:26711", GCPs=gcpList)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gcps = ds.GetGCPs()
    assert len(gcps) == 4, "GCP count wrong."

    assert ds.GetGCPProjection().find("26711") != -1, "Bad GCP projection."

    ds = None


###############################################################################
# Test nodata option


def test_gdal_translate_lib_9(tmp_path):

    dst_tif = str(tmp_path / "test9.tif")

    ds = gdal.Open("../gcore/data/byte.tif")
    ds = gdal.Translate(dst_tif, ds, noData=1)
    assert ds is not None

    assert ds.GetRasterBand(1).GetNoDataValue() == 1, "Bad nodata value"

    ds = None


###############################################################################
# Test nodata option


def test_gdal_translate_lib_nodata_uint64():

    noData = (1 << 64) - 1
    ds = gdal.Translate(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        outputType=gdal.GDT_UInt64,
        noData=noData,
    )
    assert ds is not None

    assert ds.GetRasterBand(1).GetNoDataValue() == noData, "Bad nodata value"


@pytest.mark.parametrize("nodata", (1 << 65, 3.2))
def test_gdal_translate_lib_nodata_uint64_invalid(nodata):

    with gdaltest.error_raised(
        gdal.CE_Warning, "Nodata value was not set to output band"
    ):
        ds = gdal.Translate(
            "",
            "../gcore/data/byte.tif",
            format="MEM",
            outputType=gdal.GDT_Int64,
            noData=nodata,
        )
    assert ds is not None
    assert ds.GetRasterBand(1).GetNoDataValue() is None


###############################################################################
# Test nodata option


def test_gdal_translate_lib_nodata_int64():

    ds = gdal.Open("../gcore/data/byte.tif")
    noData = -(1 << 63)
    ds = gdal.Translate("", ds, format="MEM", outputType=gdal.GDT_Int64, noData=noData)
    assert ds is not None

    assert ds.GetRasterBand(1).GetNoDataValue() == noData, "Bad nodata value"

    ds = None


@pytest.mark.parametrize("nodata", (1 << 65, 3.2))
def test_gdal_translate_lib_nodata_int64_invalid(nodata):

    with gdaltest.error_raised(
        gdal.CE_Warning, "Nodata value was not set to output band"
    ):
        ds = gdal.Translate(
            "",
            "../gcore/data/byte.tif",
            format="MEM",
            outputType=gdal.GDT_Int64,
            noData=nodata,
        )
    assert ds is not None
    assert ds.GetRasterBand(1).GetNoDataValue() is None


###############################################################################
# Test nodata=-inf


def test_gdal_translate_lib_nodata_minus_inf():

    ds = gdal.Translate(
        "", "../gcore/data/float32.tif", format="MEM", noData=float("-inf")
    )
    assert ds.GetRasterBand(1).GetNoDataValue() == float("-inf"), "Bad nodata value"


###############################################################################
# Test -srcwin option


def test_gdal_translate_lib_10(tmp_vsimem):

    ds = gdal.Translate(
        tmp_vsimem / "out.tif", "../gcore/data/byte.tif", srcWin=(0, 0, 1, 1)
    )

    assert ds.GetRasterBand(1).Checksum() == 2, "Bad checksum"


def test_gdal_translate_lib_srcwin_invalid(tmp_vsimem):

    with pytest.raises(Exception, match="Invalid output size"):
        gdal.Translate(
            tmp_vsimem / "out.tif", "../gcore/data/byte.tif", srcWin=(0, 0, 2 << 33, 1)
        )

    with pytest.raises(Exception, match="Invalid output size"):
        gdal.Translate(
            tmp_vsimem / "out.tif", "../gcore/data/byte.tif", srcWin=(0, 0, 1, 2 << 33)
        )


###############################################################################
# Test projWin option


def test_gdal_translate_lib_11(tmp_path):

    dst_tif = str(tmp_path / "test11.tif")

    ds = gdal.Open("../gcore/data/byte.tif")
    ds = gdal.Translate(
        dst_tif, ds, projWin=[440720.000, 3751320.000, 441920.000, 3750120.000]
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test outputBounds option


def test_gdal_translate_lib_12(tmp_path):

    dst_tif = str(tmp_path / "test12.tif")

    ds = gdal.Open("../gcore/data/byte.tif")
    ds = gdal.Translate(
        dst_tif,
        ds,
        outputBounds=[440720.000, 3751320.000, 441920.000, 3750120.000],
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test outputGeotransform option


def test_gdal_translate_lib_outputGeotransform(tmp_vsimem):

    dst_tif = str(tmp_vsimem / "test_gdal_translate_lib_outputGeotransform.tif")

    with pytest.raises(
        Exception, match="outputBounds and outputGeotransform are mutually exclusive"
    ):
        gdal.Translate(
            dst_tif,
            gdal.Open("../gcore/data/byte.tif"),
            outputBounds=[440720.000, 3751320.000, 441920.000, 3750120.000],
            outputGeotransform=[1.25, 2, 3, 4, 5, 6],
        )

    ds = gdal.Translate(
        dst_tif,
        gdal.Open("../gcore/data/byte.tif"),
        outputGeotransform=[1.25, 2, 3, 4, 5, 6],
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(
        (1.25, 2, 3, 4, 5, 6),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test metadataOptions


def test_gdal_translate_lib_13(tmp_vsimem):

    src_ds = gdal.Open("../gcore/data/byte.tif")

    ds = gdal.Translate(
        tmp_vsimem / "test13.tif",
        src_ds,
        metadataOptions=["TIFFTAG_DOCUMENTNAME=test13"],
    )
    assert ds is not None

    md = ds.GetMetadata()
    assert "TIFFTAG_DOCUMENTNAME" in md, "Did not get TIFFTAG_DOCUMENTNAME"
    ds = None


def test_gdal_translate_lib_13a(tmp_vsimem):

    src_ds = gdal.Open("../gcore/data/byte.tif")

    ds = gdal.Translate(
        tmp_vsimem / "test13.tif", src_ds, metadataOptions="TIFFTAG_DOCUMENTNAME=test13"
    )
    assert ds is not None

    md = ds.GetMetadata()
    assert "TIFFTAG_DOCUMENTNAME" in md, "Did not get TIFFTAG_DOCUMENTNAME"
    ds = None


###############################################################################
# Test creationOptions


def test_gdal_translate_lib_14(tmp_path):

    dst_tif = str(tmp_path / "test14.tif")

    ds = gdal.Open("../gcore/data/byte.tif")
    ds = gdal.Translate(dst_tif, ds, creationOptions=["COMPRESS=LZW"])
    assert ds is not None

    md = ds.GetMetadata("IMAGE_STRUCTURE")
    assert "COMPRESSION" in md and md["COMPRESSION"] == "LZW", "Did not get COMPRESSION"

    ds = None


###############################################################################
# Test internal wrappers


def test_gdal_translate_lib_100():

    # No option
    with pytest.raises(Exception):
        gdal.TranslateInternal("", gdal.Open("../gcore/data/byte.tif"), None)

    # Will create an implicit options structure
    with pytest.raises(Exception):
        gdal.TranslateInternal(
            "", gdal.Open("../gcore/data/byte.tif"), None, gdal.TermProgress_nocb
        )

    # Null dest name
    try:
        gdal.TranslateInternal(None, gdal.Open("../gcore/data/byte.tif"), None)
    except Exception:
        pass


###############################################################################
# Test behaviour with SIGNEDBYTE


def test_gdal_translate_lib_101(tmp_vsimem):

    ds = gdal.Translate(
        tmp_vsimem / "test_gdal_translate_lib_101.tif",
        gdal.Open("../gcore/data/byte.tif"),
        creationOptions=["PIXELTYPE=SIGNEDBYTE"],
        noData="-128",
    )
    assert (
        ds.GetRasterBand(1).GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE")
        == "SIGNEDBYTE"
    ), "Did not get SIGNEDBYTE"
    assert ds.GetRasterBand(1).GetNoDataValue() == -128, "Did not get -128"
    ds2 = gdal.Translate(
        tmp_vsimem / "test_gdal_translate_lib_101_2.tif", ds, noData=-127
    )
    assert ds2.GetRasterBand(1).GetNoDataValue() == -127, "Did not get -127"
    ds = None
    ds2 = None


###############################################################################
# Test -scale


def test_gdal_translate_lib_102():

    ds = gdal.Translate(
        "",
        gdal.Open("../gcore/data/byte.tif"),
        format="MEM",
        scaleParams=[[0, 255, 0, 65535]],
        outputType=gdal.GDT_UInt16,
    )
    result = ds.GetRasterBand(1).ComputeRasterMinMax(False)
    assert result == (19018.0, 65535.0)

    approx_min, approx_max = ds.GetRasterBand(1).ComputeRasterMinMax(True)
    ds2 = gdal.Translate(
        "",
        ds,
        format="MEM",
        scaleParams=[[approx_min, approx_max]],
        outputType=gdal.GDT_UInt8,
    )
    expected_stats = ds2.GetRasterBand(1).ComputeStatistics(False)

    # Implicit source statistics use approximate source min/max
    ds2 = gdal.Translate(
        "", ds, format="MEM", scaleParams=[[]], outputType=gdal.GDT_UInt8
    )
    stats = ds2.GetRasterBand(1).ComputeStatistics(False)
    for i in range(4):
        assert stats[i] == pytest.approx(expected_stats[i], abs=1e-3)


###############################################################################
# Test -scale preserves [0,255] input range


def test_gdal_translate_lib_scale_0_255_input_range():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 1)
    expected_data = struct.pack("B" * 3, 0, 254, 255)
    src_ds.WriteRaster(0, 0, 3, 1, expected_data)
    ds = gdal.Translate("", src_ds, options="-of MEM -scale")
    assert ds.ReadRaster() == expected_data


###############################################################################
# Test error cases of -projwin


def test_gdal_translate_lib_projwin_rotated():

    with pytest.raises(Exception, match="not supported"):

        gdal.Translate(
            "",
            "../gcore/data/geomatrix.tif",
            format="VRT",
            projWin=[1840936, 1143965, 1840999, 1143922],
        )


def test_gdal_translate_lib_projwin_srs_no_source_srs():

    src_ds = gdal.GetDriverByName("MEM").Create("", 10, 10)
    src_ds.SetGeoTransform((0, 1, 0, 10, 0, -1))

    with gdaltest.error_raised(gdal.CE_Warning, "projwin_srs ignored"):
        gdal.Translate(
            "", src_ds, format="VRT", projWin=[2, 4, 4, 2], projWinSRS="EPSG:4326"
        )


def test_gdal_translate_lib_srcwin_negative():

    with pytest.raises(Exception, match="negative width and/or height"):

        gdal.Translate(
            "",
            "../gcore/data/byte.tif",
            format="VRT",
            projWin=[440720.000, 3751320.000, 441920.000, 3751320.000],
        )


def test_gdal_translate_lib_cannot_identify_format(tmp_vsimem):

    with pytest.raises(Exception, match="Could not identify an output driver"):

        gdal.Translate(tmp_vsimem / "out.txt", "../gcore/data/byte.tif")


def test_gdal_translate_lib_invalid_format(tmp_vsimem):

    with pytest.raises(Exception, match="Output driver .* not recognised"):

        gdal.Translate(
            tmp_vsimem / "out.txt", "../gcore/data/byte.tif", format="JPEG3000"
        )


def test_gdal_translate_lib_no_raster_capabilites(tmp_vsimem):

    with pytest.raises(Exception, match="no raster capabilities"):
        gdal.Translate(
            tmp_vsimem / "byte.shp", "../gcore/data/byte.tif", format="ESRI Shapefile"
        )


@pytest.mark.require_driver("DOQ1")
def test_gdal_translate_lib_no_creation_capabilites(tmp_vsimem):

    with pytest.raises(Exception, match="no creation capabilities"):
        gdal.Translate(tmp_vsimem / "byte.doq", "../gcore/data/byte.tif", format="DOQ1")


###############################################################################
# Test that -projwin with nearest neighbor resampling uses integer source
# pixel boundaries (#6610)


def test_gdal_translate_lib_103():

    ds = gdal.Translate(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        projWin=[440730, 3751310, 441910, 3750140],
    )
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )


###############################################################################
# Test that -projwin with nearest neighbor resampling produces an output
# extent that includes all of -projwin


def test_gdal_translate_lib_projwin_expand():

    # Pixel size is 60. We should be able to shrink the corners by 59
    # without losing any pixels
    ds = gdal.Translate(
        "",
        "../gcore/data/byte.tif",
        format="MEM",
        projWin=[440720 + 59, 3751320 - 59, 441920.000 - 59, 3750120 + 59],
    )

    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20


###############################################################################
# Test -projwin_srs option


def test_gdal_translate_lib_31():

    ds = gdal.Translate(
        "",
        "../gcore/data/byte.tif",
        projWin=(-117.6408, 33.9023, -117.6282, 33.8920),
        projWinSRS="EPSG:4267",
        format="MEM",
    )

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-6,
    )


def test_gdal_translate_lib_projwin_polar(tmp_vsimem):

    src = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "in.tif", 16620, 30000, options={"SPARSE_OK": True}
    )
    src.SetGeoTransform((-640000.0, 90.0, 0.0, -655550.0, 0.0, -90.0))
    src.SetProjection("EPSG:3413")

    dst = gdal.Translate(
        "", src, projWin=(-20, 80, -19, 79), projWinSRS="EPSG:4326", format="VRT"
    )

    dst_gt = dst.GetGeoTransform()
    assert dst_gt[0] == 458900
    assert dst_gt[3] == -975950

    assert dst.RasterXSize == 723
    assert dst.RasterYSize == 1192


###############################################################################
# Test that a warning or error is issued when -projwin is partially or wholly
# outside of the source


@pytest.mark.parametrize("error", ("partially", "completely", False, True))
@gdaltest.disable_exceptions()
def test_gdal_translate_lib_projwin_partially_outside(error):

    msg_type = gdal.CE_Failure if error in ("partially", True) else gdal.CE_Warning

    with gdaltest.error_raised(msg_type, "partially outside"):
        ds = gdal.Translate(
            "",
            "../gcore/data/byte.tif",
            format="VRT",
            projWin=(440000, 3751320, 441920, 3750120),
            errorIfWindowOutsideSource=error,
        )

        if error in ("partially", True):
            assert ds is None
        else:
            assert ds is not None
            assert ds.RasterXSize == 32
            assert ds.RasterYSize == 20


@pytest.mark.parametrize("error", ("partially", "completely", False, True))
@gdaltest.disable_exceptions()
def test_gdal_translate_lib_projwin_completely_outside(error):

    msg_type = (
        gdal.CE_Failure
        if error in {"partially", "completely", True}
        else gdal.CE_Warning
    )

    with gdaltest.error_raised(msg_type, "completely outside"):
        ds = gdal.Translate(
            "",
            "../gcore/data/byte.tif",
            format="VRT",
            projWin=(0, 120, 120, 0),
            errorIfWindowOutsideSource=error,
        )

        if error in {"partially", "completely", True}:
            assert ds is None
        else:
            assert ds is not None
            assert ds.RasterXSize == 3
            assert ds.RasterYSize == 2


def test_gdal_translate_lib_projwin_invalid_error_if_window_outside_source():

    with pytest.raises(RuntimeError, match="errorIfWindowOutsideSource must be one of"):
        gdal.Translate(
            "",
            "../gcore/data/byte.tif",
            format="VRT",
            projWin=(0, 120, 120, 0),
            errorIfWindowOutsideSource="possibly",
        )


###############################################################################
# Test translate with a MEM source to a anonymous VRT


def test_gdal_translate_lib_104():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 2)
    src_ds.GetRasterBand(1).Fill(255)
    ds = gdal.Translate("", "../gcore/data/byte.tif", format="VRT", width=1, height=1)
    assert ds.GetRasterBand(1).Checksum() == 3, "Bad checksum"


###############################################################################
# Test GCPs propagation in "VRT path"


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdal_translate_lib_gcp_vrt_path():

    src_ds = gdal.Open("../gcore/data/gcps.vrt")
    ds = gdal.Translate("", src_ds, format="MEM", metadataOptions=["FOO=BAR"])
    assert len(ds.GetGCPs()) == len(src_ds.GetGCPs())
    for i in range(len(src_ds.GetGCPs())):
        assert ds.GetGCPs()[i].GCPX == src_ds.GetGCPs()[i].GCPX
        assert ds.GetGCPs()[i].GCPY == src_ds.GetGCPs()[i].GCPY
        assert ds.GetGCPs()[i].GCPPixel == src_ds.GetGCPs()[i].GCPPixel
        assert ds.GetGCPs()[i].GCPLine == src_ds.GetGCPs()[i].GCPLine


###############################################################################
# Test RPC propagation in "VRT path"


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdal_translate_lib_rcp_vrt_path():

    src_ds = gdal.Open("../gcore/data/rpc.vrt")
    ds = gdal.Translate("", src_ds, format="MEM", metadataOptions=["FOO=BAR"])
    assert ds.GetMetadata("RPC") == src_ds.GetMetadata("RPC")


###############################################################################
# Test GeoLocation propagation in "VRT path"


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdal_translate_lib_geolocation_vrt_path(tmp_vsimem):

    src_ds = gdal.Open("../gcore/data/sstgeo.vrt")
    ds = gdal.Translate(
        tmp_vsimem / "temp.vrt", src_ds, format="VRT", metadataOptions=["FOO=BAR"]
    )
    assert ds.GetMetadata("GEOLOCATION") == src_ds.GetMetadata("GEOLOCATION")


###############################################################################
# Test -colorinterp and -colorinterp_X


def test_gdal_translate_lib_colorinterp():

    src_ds = gdal.Open("../gcore/data/rgbsmall.tif")

    # Less bands specified than available
    ds = gdal.Translate(
        "",
        src_ds,
        format="MEM",
        colorInterpretation=[gdal.GCI_BlueBand, gdal.GCI_GrayIndex],
    )
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand

    # More bands specified than available and a unknown color interpretation
    with gdal.quiet_errors():
        ds = gdal.Translate(
            "",
            src_ds,
            format="MEM",
            colorInterpretation=["alpha", "red", "undefined", "foo"],
        )
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_Undefined

    # Test colorinterp_
    ds = gdal.Translate("", src_ds, options="-f MEM -colorinterp_2 alpha")
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_AlphaBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand

    # Test invalid colorinterp_
    with pytest.raises(Exception):
        with gdal.quiet_errors():
            gdal.Translate("", src_ds, options="-f MEM -colorinterp_0 alpha")

    # Test colorinterp on a source mask band
    tmp_ds = gdal.Translate("", src_ds, options="-f MEM -b 1 -b 2 -b 3 -mask mask")
    ds = gdal.Translate(
        "",
        tmp_ds,
        options="-f MEM -b 1 -b 2 -b 3 -b mask -colorinterp red,green,blue,alpha",
    )
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_RedBand
    assert ds.GetRasterBand(2).GetColorInterpretation() == gdal.GCI_GreenBand
    assert ds.GetRasterBand(3).GetColorInterpretation() == gdal.GCI_BlueBand
    assert ds.GetRasterBand(4).GetColorInterpretation() == gdal.GCI_AlphaBand


###############################################################################
# Test nogcp options


def test_gdal_translate_lib_110(tmp_path):

    dst_tif = str(tmp_path / "test110.tif")

    ds = gdal.Open("../gcore/data/byte_gcp.tif")
    ds = gdal.Translate(dst_tif, ds, nogcp="True")
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gcps = ds.GetGCPs()
    assert len(gcps) == 0, "GCP count wrong."

    ds = None


###############################################################################
# Test noxmp options


def test_gdal_translate_lib_111(tmp_path):

    dst_tif = str(tmp_path / "test111noxmp.tif")
    dst2_tif = str(tmp_path / "test111notcopied.tif")
    dst3_tif = str(tmp_path / "test111.tif")

    ds = gdal.Open("../gdrivers/data/gtiff/byte_with_xmp.tif")
    new_ds = gdal.Translate(dst_tif, ds, options="-noxmp")
    assert new_ds is not None
    xmp = new_ds.GetMetadata("xml:XMP")
    new_ds = None
    assert xmp is None

    # codepath if some other options are set is different, creating a VRTdataset
    new_ds = gdal.Translate(dst2_tif, ds, nogcp="True")
    assert new_ds is not None
    new_ds = None
    new_ds = gdal.Open(dst2_tif)
    xmp = new_ds.GetMetadata("xml:XMP")
    new_ds = None
    assert "W5M0MpCehiHzreSzNTczkc9d" in xmp[0], "Wrong output file without XMP"

    # normal codepath calling CreateCopy directly
    new_ds = gdal.Translate(dst3_tif, ds)
    assert new_ds is not None
    new_ds = None
    new_ds = gdal.Open(dst3_tif)
    xmp = new_ds.GetMetadata("xml:XMP")
    new_ds = None
    assert "W5M0MpCehiHzreSzNTczkc9d" in xmp[0], "Wrong output file without XMP"

    ds = None


def test_gdal_translate_lib_112(tmp_path):

    dst_tif = str(tmp_path / "test112noxmp.tif")
    dst2_tif = str(tmp_path / "test112.tif")

    ds = gdal.Open("../gdrivers/data/gtiff/byte_with_xmp.tif")
    new_ds = gdal.Translate(dst_tif, ds, options="-of COG -noxmp")
    assert new_ds is not None
    xmp = new_ds.GetMetadata("xml:XMP")
    new_ds = None
    assert xmp is None

    new_ds = gdal.Translate(dst2_tif, ds, format="COG")
    assert new_ds is not None
    new_ds = None
    new_ds = gdal.Open(dst2_tif)
    xmp = new_ds.GetMetadata("xml:XMP")
    new_ds = None
    assert "W5M0MpCehiHzreSzNTczkc9d" in xmp[0], "Wrong output file without XMP"

    ds = None


###############################################################################
# Test gdal_translate foo.tif foo.tif.ovr


def test_gdal_translate_lib_generate_ovr(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "foo.tif", open("../gcore/data/byte.tif", "rb").read()
    )
    gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "foo.tif.ovr", 10, 10)
    ds = gdal.Translate(
        tmp_vsimem / "foo.tif.ovr",
        tmp_vsimem / "foo.tif",
        resampleAlg=gdal.GRIORA_Average,
        format="GTiff",
        width=10,
        height=10,
    )
    assert ds
    assert ds.GetRasterBand(1).Checksum() == 1152, "Bad checksum"
    ds = None


###############################################################################
# Test gdal_translate -tr with non-nearest resample


def _get_src_ds_test_gdal_translate_lib_tr_non_nearest():

    src_w = 5
    src_h = 3
    src_ds = gdal.GetDriverByName("MEM").Create("", src_w, src_h)
    src_ds.SetGeoTransform([100, 10, 0, 1000, 0, -10])
    src_ds.WriteRaster(
        0,
        0,
        src_w,
        src_h,
        struct.pack(
            "B" * src_w * src_h,
            100,
            100,
            200,
            200,
            10,
            100,
            100,
            200,
            200,
            20,
            30,
            30,
            30,
            30,
            30,
        ),
    )
    return src_ds


def test_gdal_translate_lib_tr_non_nearest_case_1():

    ds = gdal.Translate(
        "",
        _get_src_ds_test_gdal_translate_lib_tr_non_nearest(),
        resampleAlg=gdal.GRIORA_Average,
        format="MEM",
        xRes=20,
        yRes=20,
    )
    assert ds.RasterXSize == 3  # case where we round up
    assert ds.RasterYSize == 2
    assert struct.unpack("B" * 6, ds.ReadRaster()) == (100, 200, 15, 30, 30, 30)


def test_gdal_translate_lib_tr_non_nearest_case_2():

    ds = gdal.Translate(
        "",
        _get_src_ds_test_gdal_translate_lib_tr_non_nearest(),
        resampleAlg=gdal.GRIORA_Average,
        format="MEM",
        xRes=40,
        yRes=20,
    )
    assert ds.RasterXSize == 1  # case where we round down
    assert ds.RasterYSize == 2
    assert struct.unpack("B" * 2, ds.ReadRaster()) == (150, 30)


def test_gdal_translate_lib_tr_non_nearest_case_3():

    ds = gdal.Translate(
        "",
        _get_src_ds_test_gdal_translate_lib_tr_non_nearest(),
        resampleAlg=gdal.GRIORA_Average,
        format="MEM",
        xRes=25,
        yRes=20,
    )
    assert ds.RasterXSize == 2  # case where src_w * src_res / dst_res is integer
    assert ds.RasterYSize == 2
    assert struct.unpack("B" * 4, ds.ReadRaster()) == (120, 126, 30, 30)


def test_gdal_translate_lib_tr_non_nearest_oversampling():

    ds = gdal.Translate(
        "",
        _get_src_ds_test_gdal_translate_lib_tr_non_nearest(),
        resampleAlg=gdal.GRIORA_Bilinear,
        format="MEM",
        xRes=4,
        yRes=10,
    )
    assert ds.RasterXSize == 13
    assert ds.RasterYSize == 3
    assert 0 not in struct.unpack("B" * 13 * 3, ds.ReadRaster())


def test_gdal_translate_lib_preserve_block_size(tmp_vsimem):

    src_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "tmp.tif",
        256,
        256,
        1,
        options=["TILED=YES", "BLOCKXSIZE=32", "BLOCKYSIZE=64"],
    )

    # VRT created by CreateCopy() of VRT driver
    ds = gdal.Translate("", src_ds, format="VRT")
    assert ds.GetRasterBand(1).GetBlockSize() == [32, 64]

    # VRT created by GDALTranslate()
    ds = gdal.Translate("", src_ds, format="VRT", metadataOptions=["FOO=BAR"])
    assert ds.GetRasterBand(1).GetBlockSize() == [32, 64]
    src_ds = None


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
def test_gdal_translate_lib_resampling_methods(resampleAlg, resampleAlgStr):

    option_list = gdal.TranslateOptions(
        resampleAlg=resampleAlg, options="__RETURN_OPTION_LIST__"
    )
    assert option_list == ["-r", resampleAlgStr]
    assert (
        gdal.Translate(
            "",
            "../gcore/data/byte.tif",
            format="MEM",
            width=2,
            height=2,
            resampleAlg=resampleAlg,
        )
        is not None
    )


###############################################################################
# Test not deleting auxiliary files shared by the source and a target being
# overwritten (https://github.com/OSGeo/gdal/issues/5633)


def test_gdal_translate_lib_not_delete_shared_auxiliary_files(tmp_path):

    img_foo_r1c1_jp2 = str(tmp_path / "IMG_foo_R1C1.JP2")
    img_foo_r1c1_tif = str(tmp_path / "IMG_foo_R1C1.tif")
    dim_foo_xml = str(tmp_path / "DIM_foo.XML")

    # Yes, we do intend to copy a .TIF as a fake .JP2
    shutil.copy("../gdrivers/data/dimap2/bundle/IMG_foo_R1C1.TIF", img_foo_r1c1_jp2)
    shutil.copy("../gdrivers/data/dimap2/bundle/DIM_foo.XML", dim_foo_xml)

    gdal.Translate(img_foo_r1c1_tif, img_foo_r1c1_jp2)

    os.unlink(f"{tmp_path}/IMG_foo_R1C1.IMD")

    gdal.Translate(img_foo_r1c1_tif, img_foo_r1c1_jp2)

    assert os.path.exists(f"{tmp_path}/DIM_foo.XML")


###############################################################################
# Test preservation of IsDynamic() and support for coordinate epoch


@pytest.mark.require_proj(7, 2)
def test_gdal_translate_lib_coord_epoch_is_dynamic():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)

    out_ds = gdal.Translate(
        "", src_ds, options="-of MEM -a_srs EPSG:9000 -a_coord_epoch 2021.3"
    )
    srs = out_ds.GetSpatialRef()
    assert srs.IsDynamic()
    assert srs.GetCoordinateEpoch() == 2021.3

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(9000)
    assert srs.IsDynamic()
    srs.SetCoordinateEpoch(2022.0)
    src_ds.SetSpatialRef(srs)

    out_ds = gdal.Translate("", src_ds, options="-of MEM")
    srs = out_ds.GetSpatialRef()
    assert srs.IsDynamic()
    assert srs.GetCoordinateEpoch() == 2022.0


###############################################################################
# Test overviewLevel option


def test_gdal_translate_lib_overview_level(tmp_vsimem):

    src_filename = tmp_vsimem / "test_gdal_translate_lib_overview_level.tif"

    src_ds = gdal.Translate(src_filename, "../gcore/data/byte.tif")
    src_ds.BuildOverviews("AVERAGE", [2])
    src_ds.BuildOverviews("NONE", [4])

    with gdal.quiet_errors():
        with pytest.raises(Exception):
            assert gdal.Translate("", src_ds, format="MEM", overviewLevel="invalid")

    out_ds = gdal.Translate("", src_ds, format="MEM", overviewLevel="NONE")
    assert out_ds.RasterXSize == 20
    assert out_ds.RasterYSize == 20
    assert out_ds.ReadRaster() == src_ds.ReadRaster()

    out_ds = gdal.Translate("", src_ds, format="MEM", overviewLevel=0)
    assert out_ds.RasterXSize == 10
    assert out_ds.RasterYSize == 10
    assert out_ds.ReadRaster() == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster()

    out_ds = gdal.Translate("", src_ds, format="MEM", overviewLevel=1)
    assert out_ds.RasterXSize == 5
    assert out_ds.RasterYSize == 5
    assert out_ds.ReadRaster() == src_ds.GetRasterBand(1).GetOverview(1).ReadRaster()

    out_ds = gdal.Translate("", src_ds, format="MEM", overviewLevel=2)
    assert out_ds.RasterXSize == 5
    assert out_ds.RasterYSize == 5
    assert out_ds.ReadRaster() == src_ds.GetRasterBand(1).GetOverview(1).ReadRaster()

    out_ds = gdal.Translate(
        "", src_ds, format="MEM", overviewLevel="AUTO", widthPct=50, heightPct=50
    )
    assert out_ds.RasterXSize == 10
    assert out_ds.RasterYSize == 10
    assert out_ds.ReadRaster() == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster()

    out_ds = gdal.Translate("", src_ds, format="MEM", widthPct=40, heightPct=40)
    assert out_ds.RasterXSize == 8
    assert out_ds.RasterYSize == 8
    assert out_ds.ReadRaster() == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(
        0, 0, 10, 10, 8, 8
    )

    out_ds = gdal.Translate(
        "", src_ds, format="MEM", overviewLevel=0, widthPct=25, heightPct=25
    )
    assert out_ds.RasterXSize == 5
    assert out_ds.RasterYSize == 5
    assert out_ds.ReadRaster() == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(
        0, 0, 10, 10, 5, 5
    )

    out_ds = gdal.Translate(
        "", src_ds, format="MEM", overviewLevel=0, xRes=240, yRes=240
    )
    assert out_ds.RasterXSize == 5
    assert out_ds.RasterYSize == 5
    assert out_ds.ReadRaster() == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(
        0, 0, 10, 10, 5, 5
    )

    out_ds = gdal.Translate(
        "", src_ds, format="MEM", overviewLevel=0, srcWin=[2, 2, 16, 16]
    )
    assert out_ds.RasterXSize == 8
    assert out_ds.RasterYSize == 8
    assert out_ds.ReadRaster() == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(
        1, 1, 8, 8
    )

    out_ds = gdal.Translate(
        "", src_ds, format="MEM", overviewLevel="AUTO-1", widthPct=60, heightPct=60
    )
    assert out_ds.RasterXSize == 12
    assert out_ds.RasterYSize == 12
    assert out_ds.ReadRaster() == src_ds.GetRasterBand(1).ReadRaster(
        0, 0, 20, 20, 12, 12
    )

    out_ds = gdal.Translate(
        "", src_ds, format="MEM", overviewLevel="AUTO-1", widthPct=40, heightPct=40
    )
    assert out_ds.RasterXSize == 8
    assert out_ds.RasterYSize == 8
    assert out_ds.ReadRaster() == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(
        0, 0, 10, 10, 8, 8
    )

    out_ds = gdal.Translate(
        "", src_ds, format="MEM", overviewLevel="AUTO-1", widthPct=25, heightPct=25
    )
    assert out_ds.RasterXSize == 5
    assert out_ds.RasterYSize == 5
    assert out_ds.ReadRaster() == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(
        0, 0, 10, 10, 5, 5
    )

    out_ds = gdal.Translate(
        "", src_ds, format="MEM", overviewLevel="AUTO-1", widthPct=10, heightPct=10
    )
    assert out_ds.RasterXSize == 2
    assert out_ds.RasterYSize == 2
    assert out_ds.ReadRaster() == src_ds.GetRasterBand(1).GetOverview(0).ReadRaster(
        0, 0, 10, 10, 2, 2
    )

    # Test requesting an overview level that doesn't exist when there is overview
    out_ds = gdal.Translate(
        "",
        src_ds,
        format="MEM",
        overviewLevel=src_ds.GetRasterBand(1).GetOverviewCount(),
    )
    ovr_band = src_ds.GetRasterBand(1).GetOverview(
        src_ds.GetRasterBand(1).GetOverviewCount() - 1
    )
    assert out_ds.RasterXSize == ovr_band.XSize
    assert out_ds.RasterYSize == ovr_band.YSize

    # Test requesting an overview level that doesn't exist when there is no overview
    out_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM", overviewLevel=0)
    assert out_ds.RasterXSize == 20
    assert out_ds.RasterYSize == 20


###############################################################################
# Test copying a raster with no input band


@pytest.mark.require_driver("ENVI")
def test_gdal_translate_lib_no_input_band(tmp_vsimem):

    out_filename = tmp_vsimem / "out.img"
    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 0)
    with pytest.raises(Exception):
        gdal.Translate(out_filename, src_ds, format="ENVI")
    with pytest.raises(Exception):
        gdal.Translate(out_filename, src_ds, format="ENVI", outputType=gdal.GDT_Int16)


###############################################################################
# Test -scale and -unscape


@gdaltest.enable_exceptions()
def test_gdal_translate_lib_scale_and_unscale_incompatible():

    with pytest.raises(
        Exception, match=r"-scale and -unscale cannot be used as the same time"
    ):
        gdal.Translate(
            "",
            gdal.Open("../gcore/data/byte.tif"),
            format="MEM",
            scaleParams=[[0, 255, 0, 65535]],
            unscale=True,
            outputType=gdal.GDT_UInt16,
        )


###############################################################################
# Test -a_offset -inf (dummy example, but to prove -inf works as a value
# numeric value)


@gdaltest.enable_exceptions()
def test_gdal_translate_lib_assign_offset():

    out_ds = gdal.Translate(
        "", gdal.Open("../gcore/data/byte.tif"), options="-f MEM -a_offset -inf"
    )
    assert out_ds.GetRasterBand(1).GetOffset() == float("-inf")


###############################################################################
# Test option argument handling


def test_gdal_translate_lib_dict_arguments():

    opt = gdal.TranslateOptions(
        "__RETURN_OPTION_LIST__",
        creationOptions=collections.OrderedDict(
            (("COMPRESS", "DEFLATE"), ("LEVEL", 4))
        ),
        metadataOptions=collections.OrderedDict(
            (("AREA_OR_POINT", "Area"), ("TIFFTAG_XRESOLUTION", 123))
        ),
    )

    co_idx = opt.index("-co")

    assert opt[co_idx : co_idx + 4] == ["-co", "COMPRESS=DEFLATE", "-co", "LEVEL=4"]

    mo_idx = opt.index("-mo")

    assert opt[mo_idx : mo_idx + 4] == [
        "-mo",
        "AREA_OR_POINT=Area",
        "-mo",
        "TIFFTAG_XRESOLUTION=123",
    ]


###############################################################################
# Test -dmo option


def test_gdal_translate_dmo_option():

    dst_vrt = "/vsimem/test_dmo.vrt"

    ## string dmo input
    ds = gdal.Translate(
        dst_vrt,
        "../gcore/data/byte.tif",
        domainMetadataOptions="NEW_DOMAIN:META-TAG=value",
    )

    md = ds.GetMetadata("NEW_DOMAIN")
    assert "META-TAG" in md, "Did not get META-TAG"
    assert md["META-TAG"] == "value"
    ds = None

    ## list dmo input
    ds = gdal.Translate(
        dst_vrt,
        "../gcore/data/byte.tif",
        domainMetadataOptions=[
            "NEW_DOMAIN:META-TAG=value",
            "ANOTHER_NEW_DOMAIN:META2-TAG=value",
        ],
    )

    md = ds.GetMetadata("NEW_DOMAIN")
    assert "META-TAG" in md, "Did not get META-TAG"
    assert md["META-TAG"] == "value"
    md = ds.GetMetadata("ANOTHER_NEW_DOMAIN")
    assert "META2-TAG" in md, "Did not get META2-TAG"
    assert md["META2-TAG"] == "value"

    ds = None

    ## pythonic dict dmo input
    items = {
        "domain_name": {"key": "value", "key1": "value1"},
        "domain_2": {"key2": "value2"},
    }
    ds = gdal.Translate(
        dst_vrt,
        gdal.Open("../gcore/data/byte.tif"),
        domainMetadataOptions=items,
    )
    md = ds.GetMetadata("domain_2")
    assert "key2" in md, "Did not get key2"
    assert md["key2"] == "value2"
    ds = None

    ds = gdal.Translate(
        dst_vrt,
        gdal.Open("../gcore/data/byte.tif"),
        options=["-dmo", "FOO", "-dmo", "BOO:FAR", "-dmo", "dom=ain:SOO=VAR:1"],
    )
    mdl = ds.GetMetadataDomainList()
    assert "FOO" not in mdl, "Found key 'FOO' from invalid input"
    assert "BOO" not in mdl, "Found key 'BOO' from invalid input"
    assert "dom=ain" in mdl, "Did not get dom=ain"

    md = ds.GetMetadata("dom=ain")
    assert "SOO" in md, "Did not get SOO"
    assert md["SOO"] == "VAR:1", "Did not get value VAR:1"

    ds = None


###############################################################################
# Test -ovr and RPC (https://github.com/OSGeo/gdal/issues/8386)


def test_gdal_translate_ovr_rpc():

    src_ds = gdal.Translate("", "../gcore/data/byte_rpc.tif", format="MEM")
    src_ds.BuildOverviews("NEAR", [2])
    ds = gdal.Translate("", src_ds, format="MEM", overviewLevel=0)
    src_rpc = src_ds.GetMetadata("RPC")
    ovr_rpc = ds.GetMetadata("RPC")
    assert ovr_rpc
    assert float(ovr_rpc["LINE_OFF"]) == pytest.approx(
        0.5 * (float(src_rpc["LINE_OFF"]) + 0.5) - 0.5
    )
    assert float(ovr_rpc["LINE_SCALE"]) == pytest.approx(
        0.5 * float(src_rpc["LINE_SCALE"])
    )
    assert float(ovr_rpc["SAMP_OFF"]) == pytest.approx(
        0.5 * (float(src_rpc["SAMP_OFF"]) + 0.5) - 0.5
    )
    assert float(ovr_rpc["SAMP_SCALE"]) == pytest.approx(
        0.5 * float(src_rpc["SAMP_SCALE"])
    )


###############################################################################
# Test scenario of https://github.com/OSGeo/gdal/issues/9402


def test_gdal_translate_lib_raster_uint16_ct_0_255_range():

    for r, g, b, a in [
        (255 + 1, 255, 255, 255),
        (255, 255 + 1, 255, 255),
        (255, 255, 255 + 1, 255),
        (255, 255, 255, 255 + 1),
    ]:
        src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_UInt16)
        ct = gdal.ColorTable()
        ct.SetColorEntry(0, (r, g, b, a))
        src_ds.GetRasterBand(1).SetRasterColorTable(ct)
        out_ds = gdal.Translate("", src_ds, format="MEM", rgbExpand="rgb")
        assert out_ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
        assert out_ds.GetRasterBand(2).DataType == gdal.GDT_UInt16
        assert out_ds.GetRasterBand(3).DataType == gdal.GDT_UInt16

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_UInt16)
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (255, 255, 255, 255))
    src_ds.GetRasterBand(1).SetRasterColorTable(ct)
    out_ds = gdal.Translate("", src_ds, format="MEM", rgbExpand="rgb")
    assert out_ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
    assert out_ds.GetRasterBand(2).DataType == gdal.GDT_UInt8
    assert out_ds.GetRasterBand(3).DataType == gdal.GDT_UInt8


###############################################################################


def test_gdal_translate_lib_int_max_sized_raster(tmp_vsimem):

    content = """<VRTDataset rasterXSize="2147483647" rasterYSize="2147483647">
  <VRTRasterBand dataType="Byte" band="1" />
</VRTDataset>"""
    with pytest.raises(Exception):
        gdal.Translate(tmp_vsimem / "out.tif", content)


###############################################################################


def test_gdal_translate_lib_unset_NODATA_VALUES():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    src_ds.SetMetadataItem("NODATA_VALUES", "1 2")

    out_ds = gdal.Translate("", src_ds, format="VRT", metadataOptions={"FOO": "BAR"})
    assert out_ds.GetMetadataItem("NODATA_VALUES") == "1 2"
    assert out_ds.GetMetadataItem("FOO") == "BAR"

    out_ds = gdal.Translate(
        "", src_ds, bandList=[1], format="VRT", metadataOptions={"FOO": "BAR"}
    )
    assert "NODATA_VALUES" not in out_ds.GetMetadata_Dict()
    assert out_ds.GetMetadataItem("FOO") == "BAR"

    out_ds = gdal.Translate(
        "", src_ds, bandList=[2, 1], format="VRT", metadataOptions={"FOO": "BAR"}
    )
    assert "NODATA_VALUES" not in out_ds.GetMetadata_Dict()
    assert out_ds.GetMetadataItem("FOO") == "BAR"

    out_ds = gdal.Translate(
        "", src_ds, bandList=[1, 2, 1], format="VRT", metadataOptions={"FOO": "BAR"}
    )
    assert "NODATA_VALUES" not in out_ds.GetMetadata_Dict()
    assert out_ds.GetMetadataItem("FOO") == "BAR"
