#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_translate testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil
import sys

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdal_translate_path() is None,
    reason="gdal_translate not available",
)


@pytest.fixture()
def gdal_translate_path():
    return test_cli_utilities.get_gdal_translate_path()


###############################################################################
# Simple test


def test_gdal_translate_1(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test1.tif")

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_translate_path} ../gcore/data/byte.tif {dst_tif}"
    )
    assert err is None or err == "", "got error/warning"

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -of option


def test_gdal_translate_2(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test2.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -of GTiff ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -ot option


def test_gdal_translate_3(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test3.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -ot Int16 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int16, "Bad data type"

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -b option


def test_gdal_translate_4(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test4.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path}  -b 3 -b 2 -b 1 ../gcore/data/rgbsmall.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 21349, "Bad checksum"

    assert ds.GetRasterBand(2).Checksum() == 21053, "Bad checksum"

    assert ds.GetRasterBand(3).Checksum() == 21212, "Bad checksum"

    ds = None


###############################################################################
# Test -expand option


@pytest.mark.require_driver("GIF")
def test_gdal_translate_5(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test5.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -expand rgb ../gdrivers/data/gif/bug407.gif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
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
# Test -outsize option in absolute mode


def test_gdal_translate_6(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test6.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -outsize 40 40 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18784, "Bad checksum"

    ds = None


###############################################################################
# Test -outsize option in percentage mode


def test_gdal_translate_7(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test7.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -outsize 200% 200% ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 18784, "Bad checksum"

    ds = None


###############################################################################
# Test -a_srs and -gcp options


def test_gdal_translate_8(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test8.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -a_srs EPSG:26711 -gcp 0 0  440720.000 3751320.000 -gcp 20 0 441920.000 3751320.000 -gcp 20 20 441920.000 3750120.000 0 -gcp 0 20 440720.000 3750120.000 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gcps = ds.GetGCPs()
    assert len(gcps) == 4, "GCP count wrong."

    assert ds.GetGCPProjection().find("26711") != -1, "Bad GCP projection."

    ds = None


###############################################################################
# Test -a_nodata option


def test_gdal_translate_9(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test9.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -a_nodata 1 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).GetNoDataValue() == 1, "Bad nodata value"

    ds = None


###############################################################################
# Test -srcwin option


def test_gdal_translate_10(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test10.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -srcwin 0 0 1 1 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 2, "Bad checksum"

    ds = None


###############################################################################
# Test -projwin option


def test_gdal_translate_11(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test11.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -projwin 440720.000 3751320.000 441920.000 3750120.000 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test -a_ullr option


def test_gdal_translate_12(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test12.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -a_ullr 440720.000 3751320.000 441920.000 3750120.000 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test -a_gt option


def test_gdal_translate_add_gt(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "testaddgt.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -a_gt 0 1 0 0 0 1 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(
        (0, 1, 0, 0, 0, 1),
        ds.GetGeoTransform(),
        1e-9,
    )

    ds = None


###############################################################################
# Test -mo option


def test_gdal_translate_13(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test13.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -mo TIFFTAG_DOCUMENTNAME=test13 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    md = ds.GetMetadata()
    assert "TIFFTAG_DOCUMENTNAME" in md, "Did not get TIFFTAG_DOCUMENTNAME"

    ds = None


###############################################################################
# Test -co option


def test_gdal_translate_14(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test14.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -co COMPRESS=LZW ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    md = ds.GetMetadata("IMAGE_STRUCTURE")
    assert "COMPRESSION" in md and md["COMPRESSION"] == "LZW", "Did not get COMPRESSION"

    ds = None


###############################################################################
# Test -sds option


@pytest.mark.require_driver("RPFTOC")
def test_gdal_translate_15(gdal_translate_path, tmp_path):

    gdaltest.runexternal(
        f"{gdal_translate_path} -sds ../gdrivers/data/nitf/A.TOC {tmp_path}/test15.tif"
    )

    ds = gdal.Open(f"{tmp_path}/test15_1.tif")
    assert ds is not None

    ds = None


###############################################################################
# Test -of VRT which is a special case


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdal_translate_16(gdal_translate_path, tmp_path):

    dst_vrt = str(tmp_path / "test16.vrt")

    gdaltest.runexternal(
        f"{gdal_translate_path} -of VRT ../gcore/data/byte.tif {dst_vrt}"
    )

    ds = gdal.Open(dst_vrt)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -expand option to VRT


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
@pytest.mark.require_driver("GIF")
def test_gdal_translate_17(gdal_translate_path, tmp_path):

    dst_vrt = str(tmp_path / "test17.vrt")

    gdaltest.runexternal(
        f"{gdal_translate_path} -of VRT -expand rgba ../gdrivers/data/gif/bug407.gif {dst_vrt}"
    )

    ds = gdal.Open(dst_vrt)
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

    assert (
        ds.GetRasterBand(4).GetRasterColorInterpretation() == gdal.GCI_AlphaBand
    ), "Bad color interpretation"

    assert ds.GetRasterBand(1).Checksum() == 20615, "Bad checksum"

    assert ds.GetRasterBand(2).Checksum() == 59147, "Bad checksum"

    assert ds.GetRasterBand(3).Checksum() == 63052, "Bad checksum"

    assert ds.GetRasterBand(4).Checksum() == 63052, "Bad checksum"

    ds = None


###############################################################################
# Test translation of a VRT made of VRT


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
@pytest.mark.require_driver("BMP")
def test_gdal_translate_18(gdal_translate_path, tmp_path):

    dst1_vrt = str(tmp_path / "test18_1.vrt")
    dst2_vrt = str(tmp_path / "test18_2.vrt")
    dst2_tif = str(tmp_path / "test18_2.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} ../gcore/data/8bit_pal.bmp -of VRT {dst1_vrt}"
    )
    gdaltest.runexternal(
        f"{gdal_translate_path} {dst1_vrt} -expand rgb -of VRT {dst2_vrt}"
    )
    _, ret_stderr = gdaltest.runexternal_out_and_err(
        f"{gdal_translate_path} {dst2_vrt} {dst2_tif}"
    )

    # Check that all datasets are closed
    assert ret_stderr.find("Open GDAL Datasets") == -1

    ds = gdal.Open(dst2_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    ds = None


###############################################################################
# Test -expand rgba on a color indexed dataset with an alpha band


def test_gdal_translate_19(gdal_translate_path, tmp_path):

    src_tif = str(tmp_path / "test_gdal_translate_19_src.tif")
    dst_tif = str(tmp_path / "test_gdal_translate_19_dst.tif")

    ds = gdal.GetDriverByName("GTiff").Create(src_tif, 1, 1, 2)
    ct = gdal.ColorTable()
    ct.SetColorEntry(127, (1, 2, 3, 255))
    ds.GetRasterBand(1).SetRasterColorTable(ct)
    ds.GetRasterBand(1).Fill(127)
    ds.GetRasterBand(2).Fill(250)
    ds = None

    gdaltest.runexternal(f"{gdal_translate_path} -expand rgba {src_tif} {dst_tif}")

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 1, "Bad checksum for band 1"
    assert ds.GetRasterBand(2).Checksum() == 2, "Bad checksum for band 2"
    assert ds.GetRasterBand(3).Checksum() == 3, "Bad checksum for band 3"
    assert ds.GetRasterBand(4).Checksum() == 250 % 7, "Bad checksum for band 4"

    ds = None


###############################################################################
# Test -a_nodata None


def test_gdal_translate_20(gdal_translate_path, tmp_path):

    src_tif = str(tmp_path / "test_gdal_translate_20_src.tif")
    dst_tif = str(tmp_path / "test_gdal_translate_20_dst.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -a_nodata 255 ../gcore/data/byte.tif {src_tif}"
    )
    gdaltest.runexternal(f"{gdal_translate_path} -a_nodata None {src_tif} {dst_tif}")

    ds = gdal.Open(dst_tif)
    assert ds is not None

    nodata = ds.GetRasterBand(1).GetNoDataValue()
    assert nodata is None

    ds = None


###############################################################################
# Test that statistics are copied only when appropriate (#3889)
# in that case, they must be copied


@pytest.mark.require_driver("HFA")
def test_gdal_translate_21(gdal_translate_path, tmp_path):

    dst_img = str(tmp_path / "test_gdal_translate_21.img")

    gdaltest.runexternal(
        f"{gdal_translate_path} -of HFA ../gcore/data/utmsmall.img {dst_img}"
    )

    ds = gdal.Open(dst_img)
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    assert md["STATISTICS_MINIMUM"] == "8", "STATISTICS_MINIMUM is wrong."

    assert (
        md["STATISTICS_HISTOBINVALUES"]
        == "0|0|0|0|0|0|0|0|8|0|0|0|0|0|0|0|23|0|0|0|0|0|0|0|0|29|0|0|0|0|0|0|0|46|0|0|0|0|0|0|0|69|0|0|0|0|0|0|0|99|0|0|0|0|0|0|0|0|120|0|0|0|0|0|0|0|178|0|0|0|0|0|0|0|193|0|0|0|0|0|0|0|212|0|0|0|0|0|0|0|281|0|0|0|0|0|0|0|0|365|0|0|0|0|0|0|0|460|0|0|0|0|0|0|0|533|0|0|0|0|0|0|0|544|0|0|0|0|0|0|0|0|626|0|0|0|0|0|0|0|653|0|0|0|0|0|0|0|673|0|0|0|0|0|0|0|629|0|0|0|0|0|0|0|0|586|0|0|0|0|0|0|0|541|0|0|0|0|0|0|0|435|0|0|0|0|0|0|0|348|0|0|0|0|0|0|0|341|0|0|0|0|0|0|0|0|284|0|0|0|0|0|0|0|225|0|0|0|0|0|0|0|237|0|0|0|0|0|0|0|172|0|0|0|0|0|0|0|0|159|0|0|0|0|0|0|0|105|0|0|0|0|0|0|0|824|"
    ), "STATISTICS_HISTOBINVALUES is wrong."


###############################################################################
# Test that statistics are copied only when appropriate (#3889)
# in this case, they must *NOT* be copied


@pytest.mark.require_driver("HFA")
def test_gdal_translate_22(gdal_translate_path, tmp_path):

    dst_img = str(tmp_path / "test_gdal_translate_22.img")

    gdaltest.runexternal(
        f"{gdal_translate_path} -of HFA -scale 0 255 0 128 ../gcore/data/utmsmall.img {dst_img}"
    )

    ds = gdal.Open(dst_img)
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    assert "STATISTICS_MINIMUM" not in md, "did not expect a STATISTICS_MINIMUM value."

    assert (
        "STATISTICS_HISTOBINVALUES" not in md
    ), "did not expect a STATISTICS_HISTOBINVALUES value."


###############################################################################
# Test -stats option (#3889)


def test_gdal_translate_23(gdal_translate_path, tmp_path):

    src_tif = str(tmp_path / "byte.tif")
    dst_tif = str(tmp_path / "test_gdal_translate_23.tif")

    shutil.copy("../gcore/data/byte.tif", src_tif)

    gdaltest.runexternal(f"{gdal_translate_path} -stats {src_tif} {dst_tif}")

    ds = gdal.Open(dst_tif)
    md = ds.GetRasterBand(1).GetMetadata()
    ds = None

    assert md["STATISTICS_MINIMUM"] == "74", "STATISTICS_MINIMUM is wrong."

    assert not os.path.exists(dst_tif + ".aux.xml")


###############################################################################
# Test -srcwin option when partially outside


def test_gdal_translate_24(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test_gdal_translate_24.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -q -srcwin -10 -10 40 40 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4620, "Bad checksum"

    ds = None


###############################################################################
# Test -norat


@pytest.mark.require_driver("HFA")
def test_gdal_translate_25(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test_gdal_translate_25.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -q ../gdrivers/data/hfa/int.img {dst_tif} -norat"
    )

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).GetDefaultRAT() is None, "RAT unexpected"

    ds = None


###############################################################################
# Test -a_nodata and -stats (#5463)


@pytest.mark.require_driver("XYZ")
def test_gdal_translate_26(gdal_translate_path, tmp_path):

    src_xyz = str(tmp_path / "test_gdal_translate_26.xyz")
    dst_tif = str(tmp_path / "test_gdal_translate_26.tif")

    f = open(src_xyz, "wb")
    f.write("""X Y Z
0 0 -999
1 0 10
0 1 15
1 1 20""".encode("ascii"))
    f.close()
    gdaltest.runexternal(
        f"{gdal_translate_path} -a_nodata -999 -stats {src_xyz} {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).GetMinimum() == 10
    assert ds.GetRasterBand(1).GetNoDataValue() == -999

    ds = None


###############################################################################
# Test that we don't preserve statistics when we ought not.


@pytest.mark.require_driver("AAIGRID")
def test_gdal_translate_27(gdal_translate_path, tmp_path):

    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip("gdalinfo missing")

    src_asc = str(tmp_path / "test_gdal_translate_27.asc")
    dst_tif = str(tmp_path / "test_gdal_translate_27.tif")

    f = open(src_asc, "wb")
    f.write("""ncols        2
nrows        2
xllcorner    440720.000000000000
yllcorner    3750120.000000000000
cellsize     60.000000000000
 0 256
 0 0""".encode("ascii"))
    f.close()

    gdaltest.runexternal(f"{test_cli_utilities.get_gdalinfo_path()} -stats {src_asc}")

    # Translate to an output type that accepts 256 as maximum
    gdaltest.runexternal(f"{gdal_translate_path} {src_asc} {dst_tif} -ot UInt16")

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is not None
    ds = None

    # Translate to an output type that accepts 256 as maximum
    gdaltest.runexternal(f"{gdal_translate_path} {src_asc} {dst_tif} -ot Float64")

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is not None
    ds = None

    # Translate to an output type that doesn't accept 256 as maximum
    gdaltest.runexternal(f"{gdal_translate_path} {src_asc} {dst_tif} -ot Byte")

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).GetMetadataItem("STATISTICS_MINIMUM") is None
    ds = None


###############################################################################
# Test -oo


@pytest.mark.require_driver("AAIGRID")
def test_gdal_translate_28(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test_gdal_translate_28.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} ../gdrivers/data/aaigrid/float64.asc {dst_tif} -oo datatype=float64"
    )

    ds = gdal.Open(dst_tif)
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float64
    ds = None


###############################################################################
# Test -r


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdal_translate_29(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test_gdal_translate_29.tif")
    dst_vrt = str(tmp_path / "test_gdal_translate_29.vrt")

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_translate_path} ../gcore/data/byte.tif {dst_tif} -outsize 50% 50% -r cubic"
    )
    assert err is None or err == "", "got error/warning"

    ds = gdal.Open(dst_tif)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 1059, "Bad checksum"

    ds = None

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_translate_path} ../gcore/data/byte.tif {dst_vrt} -outsize 50% 50% -r cubic -of VRT"
    )
    assert err is None or err == "", "got error/warning"
    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_translate_path} {dst_vrt} {dst_tif}"
    )
    assert err is None or err == "", "got error/warning"

    ds = gdal.Open(dst_tif)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 1059, "Bad checksum"

    ds = None


###############################################################################
# Test -tr option


def test_gdal_translate_30(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test_gdal_translate_30.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -tr 30 30 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 18784, "Bad checksum"

    ds = None


###############################################################################
# Test -projwin_srs option


def test_gdal_translate_31(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test_gdal_translate_30.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -projwin_srs EPSG:4267 -projwin -117.6408 33.9023 -117.6282 33.8920 ../gcore/data/byte.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gdaltest.check_geotransform(
        gdal.Open("../gcore/data/byte.tif").GetGeoTransform(),
        ds.GetGeoTransform(),
        1e-6,
    )


###############################################################################
# Test subsetting a file with a RPC


def test_gdal_translate_32(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "out.tif")

    src_ds = gdal.Open("../gcore/data/byte_rpc.tif")
    src_md = src_ds.GetMetadata("RPC")
    srcxoff = 1
    srcyoff = 2
    srcwidth = 13
    srcheight = 14
    widthratio = 200
    heightratio = 300
    gdaltest.runexternal(
        f"{gdal_translate_path} ../gcore/data/byte_rpc.tif {dst_tif} -srcwin {srcxoff} {srcyoff} {srcwidth} {srcheight} -outsize {widthratio}% {heightratio}%"
    )
    widthratio /= 100.0
    heightratio /= 100.0
    ds = gdal.Open(dst_tif)
    md = ds.GetMetadata("RPC")
    assert float(md["LINE_OFF"]) == pytest.approx(
        (float(src_md["LINE_OFF"]) - srcyoff + 0.5) * heightratio - 0.5, abs=1e-5
    )
    assert float(md["LINE_SCALE"]) == pytest.approx(
        float(src_md["LINE_SCALE"]) * heightratio, abs=1e-5
    )
    assert float(md["SAMP_OFF"]) == pytest.approx(
        (float(src_md["SAMP_OFF"]) - srcxoff + 0.5) * widthratio - 0.5, abs=1e-5
    )
    assert float(md["SAMP_SCALE"]) == pytest.approx(
        float(src_md["SAMP_SCALE"]) * widthratio, abs=1e-5
    )


def test_gdal_translate_32bis(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "out.tif")

    gdaltest.runexternal_out_and_err(
        f"{gdal_translate_path}  ../gcore/data/byte_rpc.tif {dst_tif} -srcwin -10 -5 20 20"
    )
    ds = gdal.Open(dst_tif)
    md = ds.GetMetadata("RPC")
    assert (
        float(md["LINE_OFF"]) == pytest.approx((15834 - -5), abs=1e-5)
        and float(md["LINE_SCALE"]) == pytest.approx(15834, abs=1e-5)
        and float(md["SAMP_OFF"]) == pytest.approx((13464 - -10), abs=1e-5)
        and float(md["SAMP_SCALE"]) == pytest.approx(13464, abs=1e-5)
    )


###############################################################################
# Test -outsize option in auto mode


def test_gdal_translate_33(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "out.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -outsize 100 0 ../gdrivers/data/small_world.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds.RasterYSize == 50
    ds = None


def test_gdal_translate_33bis(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "out.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -outsize 0 100 ../gdrivers/data/small_world.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds.RasterXSize == 200, ds.RasterYSize
    ds = None


def test_gdal_translate_33ter(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "out.tif")

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_translate_path} -outsize 0 0 ../gdrivers/data/small_world.tif {dst_tif}"
    )
    assert "-outsize 0 0 invalid" in err


###############################################################################
# Test NBITS is preserved


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdal_translate_34(gdal_translate_path, tmp_path):

    dst_vrt = str(tmp_path / "test_gdal_translate_34.vrt")

    gdaltest.runexternal(
        f"{gdal_translate_path} ../gcore/data/oddsize1bit.tif {dst_vrt} -of VRT -mo FOO=BAR"
    )

    ds = gdal.Open(dst_vrt)
    assert ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "1"
    ds = None


###############################################################################
# Test various errors (missing source or dest...)


def test_gdal_translate_35(gdal_translate_path, tmp_vsimem):

    _, err = gdaltest.runexternal_out_and_err(gdal_translate_path)
    assert "input_file: 1 argument(s) expected. 0 provided." in err

    _, err = gdaltest.runexternal_out_and_err(
        gdal_translate_path + " ../gcore/data/byte.tif"
    )
    assert "output_file: 1 argument(s) expected. 0 provided." in err

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_translate_path} /non_existing_path/non_existing.tif {tmp_vsimem}/out.tif"
    )
    assert (
        "does not exist in the file system" in err or "No such file or directory" in err
    )

    _, err = gdaltest.runexternal_out_and_err(
        gdal_translate_path
        + " ../gcore/data/byte.tif /non_existing_path/non_existing.tif"
    )
    assert "Attempt to create new tiff file" in err


###############################################################################
# Test RAT is copied from hfa to gtiff - continuous/athematic


@pytest.mark.require_driver("HFA")
def test_gdal_translate_36(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test_gdal_translate_36.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -of gtiff data/onepixelcontinuous.img {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    rat = ds.GetRasterBand(1).GetDefaultRAT()
    assert rat, "Did not get RAT"

    assert rat.GetRowCount() == 256, "RAT has incorrect row count"

    assert rat.GetTableType() == 1, "RAT not athematic"
    rat = None
    ds = None


###############################################################################
# Test RAT is copied from hfa to gtiff - thematic


@pytest.mark.require_driver("HFA")
def test_gdal_translate_37(gdal_translate_path, tmp_path):

    dst1_tif = str(tmp_path / "test_gdal_translate_37.tif")
    dst2_tif = str(tmp_path / "test_gdal_translate_38.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -q -of gtiff data/onepixelthematic.img {dst1_tif}"
    )

    ds = gdal.Open(dst1_tif)
    assert ds is not None

    rat = ds.GetRasterBand(1).GetDefaultRAT()
    assert rat, "Did not get RAT"

    assert rat.GetRowCount() == 256, "RAT has incorrect row count"

    assert rat.GetTableType() == 0, "RAT not thematic"
    rat = None
    ds = None

    # Test RAT is copied round trip back to hfa

    gdaltest.runexternal(f"{gdal_translate_path} -q -of hfa {dst1_tif} {dst2_tif}")

    ds = gdal.Open(dst2_tif)
    assert ds is not None

    rat = ds.GetRasterBand(1).GetDefaultRAT()
    assert rat, "Did not get RAT"

    assert rat.GetRowCount() == 256, "RAT has incorrect row count"

    assert rat.GetTableType() == 0, "RAT not thematic"
    rat = None
    ds = None


###############################################################################
# Test -nogcp options


def test_gdal_translate_39(gdal_translate_path, tmp_path):

    dst_tif = str(tmp_path / "test39.tif")

    gdaltest.runexternal(
        f"{gdal_translate_path} -nogcp ../gcore/data/byte_gcp.tif {dst_tif}"
    )

    ds = gdal.Open(dst_tif)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, "Bad checksum"

    gcps = ds.GetGCPs()
    assert len(gcps) == 0, "GCP count wrong."

    ds = None


###############################################################################
# Test -if option


def test_gdal_translate_if_option(gdal_translate_path, tmp_vsimem):

    ret, err = gdaltest.runexternal_out_and_err(
        f"{gdal_translate_path} -if GTiff ../gcore/data/byte.tif {tmp_vsimem}/out.tif"
    )
    assert err is None or err == ""

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_translate_path}  -if invalid_driver_name ../gcore/data/byte.tif {tmp_vsimem}/out.tif"
    )
    assert err is not None
    assert "invalid_driver_name" in err

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_translate_path} -if HFA ../gcore/data/byte.tif {tmp_vsimem}/out.tif"
    )
    assert err is not None


###############################################################################
# Test -scale and -a_offset + -a_scale


@pytest.mark.skipif(sys.platform == "win32", reason="not working on Windows")
def test_gdal_translate_scale_and_unscale_incompatible(gdal_translate_path, tmp_vsimem):

    _, err = gdaltest.runexternal_out_and_err(
        gdal_translate_path
        + f" -a_scale 0.0001 -a_offset 0.1 -unscale ../gcore/data/byte.tif {tmp_vsimem}/out.tif"
    )
    assert "-a_scale/-a_offset are not applied by -unscale" in err


###############################################################################
# Test that invalid values of -scale are detected


def test_gdal_translate_scale_invalid(gdal_translate_path, tmp_path):

    outfile = tmp_path / "out.tif"

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_translate_path} -scale 0 255 6 -badarg ../gcore/data/byte.tif {outfile}"
    )

    assert "must be numeric" in err
    assert not outfile.exists()
