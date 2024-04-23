#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Overview Support (mostly a GeoTIFF issue).
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

import array
import os
import shutil
import stat
import struct

import gdaltest
import pytest

from osgeo import gdal, osr


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


@pytest.fixture(params=["invert", "dont-invert"])
def both_endian(request):
    """
    Runs tests with both values of GDAL_TIFF_ENDIANNESS
    """
    if request.param == "invert":
        with gdaltest.config_option("GDAL_TIFF_ENDIANNESS", "INVERTED"):
            yield
    else:
        yield


###############################################################################
# Check the overviews


def tiff_ovr_check(src_ds):
    for i in (1, 2, 3):
        assert src_ds.GetRasterBand(i).GetOverviewCount() == 2, "overviews missing"

        ovr_band = src_ds.GetRasterBand(i).GetOverview(0)
        if ovr_band.XSize != 10 or ovr_band.YSize != 10:
            msg = "overview wrong size: band %d, overview 0, size = %d * %d," % (
                i,
                ovr_band.XSize,
                ovr_band.YSize,
            )
            pytest.fail(msg)

        if ovr_band.Checksum() != 1087:
            msg = "overview wrong checksum: band %d, overview 0, checksum = %d," % (
                i,
                ovr_band.Checksum(),
            )
            pytest.fail(msg)

        ovr_band = src_ds.GetRasterBand(i).GetOverview(1)
        if ovr_band.XSize != 5 or ovr_band.YSize != 5:
            msg = "overview wrong size: band %d, overview 1, size = %d * %d," % (
                i,
                ovr_band.XSize,
                ovr_band.YSize,
            )
            pytest.fail(msg)

        if ovr_band.Checksum() != 328:
            msg = "overview wrong checksum: band %d, overview 1, checksum = %d," % (
                i,
                ovr_band.Checksum(),
            )
            pytest.fail(msg)


###############################################################################
# Create a 3 band floating point GeoTIFF file so we can build overviews on it
# later.  Build overviews on it.


@pytest.fixture()
def mfloat32_tif(tmp_path):
    dst_fname = str(tmp_path / "mfloat32.tif")

    src_ds = gdal.Open("data/mfloat32.vrt")

    assert src_ds is not None, "Failed to open test dataset."

    gdal.GetDriverByName("GTiff").CreateCopy(
        dst_fname, src_ds, options=["INTERLEAVE=PIXEL"]
    )

    yield dst_fname


def test_tiff_ovr_1(mfloat32_tif, both_endian):

    ds = gdal.Open(mfloat32_tif)

    assert ds is not None, "Failed to open test dataset."

    err = ds.BuildOverviews(overviewlist=[2, 4])

    assert err == 0, "BuildOverviews reports an error"

    tiff_ovr_check(ds)

    ds = None

    # Open file and verify some characteristics of the overviews.

    src_ds = gdal.Open(mfloat32_tif)

    assert src_ds is not None, "Failed to open test dataset."

    tiff_ovr_check(src_ds)

    src_ds = None


###############################################################################
# Open target file in update mode, and create internal overviews.


def test_tiff_ovr_3(mfloat32_tif, both_endian):

    src_ds = gdal.Open(mfloat32_tif, gdal.GA_Update)

    assert src_ds is not None, "Failed to open test dataset."

    err = src_ds.BuildOverviews(overviewlist=[2, 4])
    assert err == 0, "BuildOverviews reports an error"

    tiff_ovr_check(src_ds)

    src_ds = None

    # Re-open target file and check overviews
    src_ds = gdal.Open(mfloat32_tif)

    assert src_ds is not None, "Failed to open test dataset."

    tiff_ovr_check(src_ds)

    src_ds = None


###############################################################################
# Test generation


def test_tiff_ovr_4(tmp_path, both_endian):
    tif_fname = str(tmp_path / "ovr4.tif")

    shutil.copyfile("data/oddsize_1bit2b.tif", tif_fname)

    wrk_ds = gdal.Open(tif_fname, gdal.GA_Update)

    assert wrk_ds is not None, "Failed to open test dataset."

    wrk_ds.BuildOverviews("AVERAGE_BIT2GRAYSCALE", overviewlist=[2, 4])
    wrk_ds = None

    wrk_ds = gdal.Open(tif_fname)

    ovband = wrk_ds.GetRasterBand(1).GetOverview(1)
    md = ovband.GetMetadata()
    assert (
        "RESAMPLING" in md and md["RESAMPLING"] == "AVERAGE_BIT2GRAYSCALE"
    ), "Did not get expected RESAMPLING metadata."

    # compute average value of overview band image data.
    ovimage = ovband.ReadRaster(0, 0, ovband.XSize, ovband.YSize)

    pix_count = ovband.XSize * ovband.YSize
    total = float(sum(ovimage))

    average = total / pix_count
    exp_average = 154.0992
    assert average == pytest.approx(
        exp_average, abs=0.1
    ), "got wrong average for overview image"

    # Read base band as overview resolution and verify we aren't getting
    # the grayscale image.

    frband = wrk_ds.GetRasterBand(1)
    ovimage = frband.ReadRaster(
        0, 0, frband.XSize, frband.YSize, ovband.XSize, ovband.YSize
    )

    pix_count = ovband.XSize * ovband.YSize
    total = float(sum(ovimage))
    average = total / pix_count
    exp_average = 0.6096

    assert average == pytest.approx(
        exp_average, abs=0.01
    ), "got wrong average for downsampled image"

    wrk_ds = None


###############################################################################
# Test average overview generation with nodata.


def test_tiff_ovr_5(tmp_path, both_endian):
    tif_fname = str(tmp_path / "ovr5.tif")

    shutil.copyfile("data/nodata_byte.tif", tif_fname)

    wrk_ds = gdal.Open(tif_fname, gdal.GA_ReadOnly)

    assert wrk_ds is not None, "Failed to open test dataset."

    wrk_ds.BuildOverviews("AVERAGE", overviewlist=[2])

    cs = wrk_ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1130

    assert cs == exp_cs, "got wrong overview checksum."


###############################################################################
# Same as tiff_ovr_5 but with USE_RDD=YES to force external overview


def test_tiff_ovr_6(tmp_path, both_endian):
    tif_fname = str(tmp_path / "ovr6.tif")

    shutil.copyfile("data/nodata_byte.tif", tif_fname)

    wrk_ds = gdal.Open(tif_fname, gdal.GA_Update)

    assert wrk_ds is not None, "Failed to open test dataset."

    def cbk(pct, _, user_data):
        if user_data[0] < 0:
            assert pct == 0
        assert pct >= user_data[0]
        user_data[0] = pct
        return 1

    tab = [-1]
    wrk_ds.BuildOverviews(
        "AVERAGE",
        overviewlist=[2],
        callback=cbk,
        callback_data=tab,
        options=["USE_RRD=YES"],
    )
    assert tab[0] == 1.0

    try:
        os.stat(tif_fname.replace(".tif", ".aux"))
    except OSError:
        pytest.fail("no external overview.")

    cs = wrk_ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1130

    assert cs == exp_cs, "got wrong overview checksum."


###############################################################################
# Check nearest resampling on a dataset with a raster band that has a color table


def test_tiff_ovr_7(tmp_path, both_endian):
    tif_fname = str(tmp_path / "test_average_palette.tif")

    shutil.copyfile("data/test_average_palette.tif", tif_fname)

    # This dataset is a black&white chessboard, index 0 is black, index 1 is white.
    # In nearest resampling, we are expecting a uniform black image.
    ds = gdal.Open(tif_fname, gdal.GA_Update)

    assert ds is not None, "Failed to open test dataset."

    ds.BuildOverviews("NEAREST", overviewlist=[2])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 0

    ds = None

    assert cs == exp_cs, "got wrong overview checksum."


###############################################################################
# Check average resampling on a dataset with a raster band that has a color table


def test_tiff_ovr_8(tmp_path, both_endian):

    tif_fname = str(tmp_path / "test_average_palette.tif")

    shutil.copyfile("data/test_average_palette.tif", tif_fname)

    # This dataset is a black&white chessboard, index 0 is black, index 1 is white.
    # So the result of averaging (0,0,0) and (255,255,255) is (127,127,127), which is
    # index 2. So the result of the averaging is a uniform grey image.
    ds = gdal.Open(tif_fname, gdal.GA_Update)

    assert ds is not None, "Failed to open test dataset."

    ds.BuildOverviews("AVERAGE", overviewlist=[2])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 200

    ds = None

    assert cs == exp_cs, "got wrong overview checksum."


###############################################################################
# Check RMS resampling on a dataset with a raster band that has a color table


def test_tiff_ovr_rms_palette(tmp_path, both_endian):
    tif_fname = str(tmp_path / "test_average_palette.tif")

    shutil.copyfile("data/test_average_palette.tif", tif_fname)

    # This dataset is a black&white chessboard, index 0 is black, index 1 is white.
    # So the result of averaging (0,0,0) and (255,255,255) is (180.3,180.3,180.3),
    # and the closest color is (127,127,127) at index 2.
    # So the result of the averaging is a uniform grey image.
    ds = gdal.Open(tif_fname, gdal.GA_Update)
    ds.BuildOverviews("RMS", overviewlist=[2])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 200

    ds = None

    assert cs == exp_cs, "got wrong overview checksum."


###############################################################################
# Test --config COMPRESS_OVERVIEW JPEG --config PHOTOMETRIC_OVERVIEW YCBCR -ro
# Will also check that pixel interleaving is automatically selected (#3064)


@pytest.mark.parametrize("option_name_suffix", ["", "_OVERVIEW"])
@pytest.mark.parametrize("read_only", [True, False])
@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_tiff_ovr_9(tmp_path, both_endian, option_name_suffix, read_only):
    tif_fname = str(tmp_path / "ovr9.tif")

    shutil.copyfile("data/rgbsmall.tif", tif_fname)

    ds = gdal.Open(tif_fname, gdal.GA_ReadOnly if read_only else gdal.GA_Update)

    assert ds is not None, "Failed to open test dataset."

    ds.BuildOverviews(
        "AVERAGE",
        overviewlist=[2],
        options=[
            "COMPRESS" + option_name_suffix + "=JPEG",
            "PHOTOMETRIC" + option_name_suffix + "=YCBCR",
        ],
    )

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds = None

    if read_only:
        exp_cs_list = (
            5562,
            5635,
            5601,  # libjpeg 9e
        )
        assert cs in exp_cs_list
    else:
        assert cs != 0

    # Re-check after dataset reopening
    ds = gdal.Open(tif_fname, gdal.GA_ReadOnly)

    assert (
        ds.GetRasterBand(1)
        .GetOverview(0)
        .GetDataset()
        .GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE")
        == "YCbCr JPEG"
    )
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()

    ds = None

    if read_only:
        assert cs in exp_cs_list
    else:
        assert cs != 0


###############################################################################
# Similar to tiff_ovr_9 but with internal overviews.


@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_tiff_ovr_10(tmp_path, both_endian):
    tif_fname = str(tmp_path / "ovr10.tif")

    src_ds = gdal.Open("data/rgbsmall.tif", gdal.GA_ReadOnly)

    assert src_ds is not None, "Failed to open test dataset."

    ds = gdal.GetDriverByName("GTiff").CreateCopy(
        tif_fname, src_ds, options=["COMPRESS=JPEG", "PHOTOMETRIC=YCBCR"]
    )
    src_ds = None

    assert ds is not None, "Failed to apply JPEG compression."

    ds.BuildOverviews("AVERAGE", overviewlist=[2])

    ds = None
    ds = gdal.Open(tif_fname, gdal.GA_ReadOnly)

    assert ds is not None, "Failed to open copy of test dataset."

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()

    ds = None

    assert cs in (
        5879,
        5992,  # GISInternals build
        6050,  # libjpeg 9e
    )


###############################################################################
# Overview on a dataset with NODATA_VALUES


def test_tiff_ovr_11(tmp_path, both_endian):
    tif_fname = str(tmp_path / "ovr11.tif")

    src_ds = gdal.Open("data/test_nodatavalues.tif", gdal.GA_ReadOnly)

    assert src_ds is not None, "Failed to open test dataset."

    ds = gdal.GetDriverByName("GTiff").CreateCopy(tif_fname, src_ds)
    src_ds = None

    ds.BuildOverviews("AVERAGE", overviewlist=[2])

    ds = None
    ds = gdal.Open(tif_fname, gdal.GA_ReadOnly)

    assert ds is not None, "Failed to open copy of test dataset."

    cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
    # If NODATA_VALUES was ignored, we would get 2766
    exp_cs = 2792

    ds = None

    assert cs == exp_cs, "got wrong overview checksum."


###############################################################################
# Same as tiff_ovr_11 but with compression to trigger the multiband overview
# code


def test_tiff_ovr_12(tmp_path, both_endian):
    tif_fname = str(tmp_path / "ovr12.tif")

    src_ds = gdal.Open("data/test_nodatavalues.tif", gdal.GA_ReadOnly)

    assert src_ds is not None, "Failed to open test dataset."

    ds = gdal.GetDriverByName("GTiff").CreateCopy(
        tif_fname, src_ds, options=["COMPRESS=DEFLATE"]
    )
    src_ds = None

    ds.BuildOverviews("AVERAGE", overviewlist=[2])

    ds = None
    ds = gdal.Open(tif_fname, gdal.GA_ReadOnly)

    assert ds is not None, "Failed to open copy of test dataset."

    cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
    # If NODATA_VALUES was ignored, we would get 2766
    exp_cs = 2792

    ds = None

    assert cs == exp_cs, "got wrong overview checksum."


###############################################################################
# Test gaussian resampling


def test_tiff_ovr_13(mfloat32_tif, both_endian):

    ds = gdal.Open(mfloat32_tif)

    assert ds is not None, "Failed to open test dataset."

    err = ds.BuildOverviews("GAUSS", overviewlist=[2, 4])

    assert err == 0, "BuildOverviews reports an error"

    # if ds.GetRasterBand(1).GetOverview(0).Checksum() != 1225:
    #    gdaltest.post_reason( 'bad checksum' )
    #    return 'fail'

    ds = None


###############################################################################
# Check gauss resampling on a dataset with a raster band that has a color table


def test_tiff_ovr_14(tmp_path, both_endian):
    tif_fname = str(tmp_path / "test_gauss_palette.tif")

    shutil.copyfile("data/test_average_palette.tif", tif_fname)

    ds = gdal.Open(tif_fname, gdal.GA_Update)

    assert ds is not None, "Failed to open test dataset."

    ds.BuildOverviews("GAUSS", overviewlist=[2])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 200

    ds = None

    assert cs == exp_cs, "got wrong overview checksum."


###############################################################################
# Same as tiff_ovr_11 but with gauss, and compression to trigger the multiband overview
# code


def test_tiff_ovr_15(tmp_path, both_endian):
    tif_fname = str(tmp_path / "ovr15.tif")

    src_ds = gdal.Open("data/test_nodatavalues.tif", gdal.GA_ReadOnly)

    assert src_ds is not None, "Failed to open test dataset."

    ds = gdal.GetDriverByName("GTiff").CreateCopy(
        tif_fname, src_ds, options=["COMPRESS=DEFLATE"]
    )
    src_ds = None

    ds.BuildOverviews("GAUSS", overviewlist=[2])

    ds = None
    ds = gdal.Open(tif_fname, gdal.GA_ReadOnly)

    assert ds is not None, "Failed to open copy of test dataset."

    cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
    # If NODATA_VALUES was ignored, we would get 2954
    exp_cs = 2987

    ds = None

    assert cs == exp_cs, "got wrong overview checksum."


###############################################################################
# Test mode resampling on non-byte dataset


def test_tiff_ovr_16(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr16.tif")

    tiff_drv = gdal.GetDriverByName("GTiff")

    src_ds = gdal.Open("data/mfloat32.vrt")

    assert src_ds is not None, "Failed to open test dataset."

    tiff_drv.CreateCopy(tif_fname, src_ds, options=["INTERLEAVE=PIXEL"])
    src_ds = None

    ds = gdal.Open(tif_fname)

    assert ds is not None, "Failed to open test dataset."

    err = ds.BuildOverviews("MODE", overviewlist=[2, 4])

    assert err == 0, "BuildOverviews reports an error"

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1122
    assert cs == exp_cs, "bad checksum"

    ds = None


###############################################################################
# Test mode resampling on a byte dataset


def test_tiff_ovr_17(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr17.tif")

    shutil.copyfile("data/byte.tif", tif_fname)

    ds = gdal.Open(tif_fname)

    assert ds is not None, "Failed to open test dataset."

    err = ds.BuildOverviews("MODE", overviewlist=[2, 4])

    assert err == 0, "BuildOverviews reports an error"

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1122
    assert cs == exp_cs, "bad checksum"

    ds = None


###############################################################################
# Check mode resampling on a dataset with a raster band that has a color table


def test_tiff_ovr_18(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr18.tif")

    shutil.copyfile("data/test_average_palette.tif", tif_fname)

    ds = gdal.Open(tif_fname, gdal.GA_Update)

    assert ds is not None, "Failed to open test dataset."

    ds.BuildOverviews("MODE", overviewlist=[2])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 100

    ds = None

    assert cs == exp_cs, "got wrong overview checksum."


###############################################################################
# Check mode resampling with multiband logic


@pytest.mark.parametrize("multiband", [True, False])
@pytest.mark.parametrize("read_only", [True, False])
def test_tiff_ovr_mode_multiband(multiband, read_only):

    ds = gdal.Translate(
        "/vsimem/test.tif",
        "data/stefan_full_rgba.tif",
        creationOptions=["COMPRESS=LZW"] if multiband else [],
    )
    if read_only:
        ds = None
        ds = gdal.Open("/vsimem/test.tif")
    ds.BuildOverviews("MODE", [2, 4])
    ds = None
    ds = gdal.Open("/vsimem/test.tif")
    assert [ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(4)] == [
        18926,
        14090,
        8398,
        36045,
    ]
    assert [ds.GetRasterBand(i + 1).GetOverview(1).Checksum() for i in range(4)] == [
        3501,
        2448,
        1344,
        8583,
    ]
    ds = None
    gdal.GetDriverByName("GTiff").Delete("/vsimem/test.tif")


###############################################################################
# Check that we can create overviews on a newly create file (#2621)


def test_tiff_ovr_19(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr19.tif")

    ds = gdal.GetDriverByName("GTiff").Create(tif_fname, 100, 100, 1)
    ds.GetRasterBand(1).Fill(1)

    # The flush is important to simulate the behaviour that wash it by #2621
    ds.FlushCache()
    ds.BuildOverviews("NEAR", overviewlist=[2])
    ds.FlushCache()
    ds.BuildOverviews("NEAR", overviewlist=[2, 4])

    assert (
        ds.GetRasterBand(1).GetOverviewCount() == 2
    ), "Overview could not be generated"

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 2500

    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs == 625

    ds = None


###############################################################################
# Test BIGTIFF_OVERVIEW=YES option


def test_tiff_ovr_20(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr20.tif")

    ds = gdal.GetDriverByName("GTiff").Create(tif_fname, 100, 100, 1)
    ds = None

    ds = gdal.Open(tif_fname)

    assert ds is not None, "Failed to open test dataset."

    with gdaltest.config_option("BIGTIFF_OVERVIEW", "YES"):
        ds.BuildOverviews("NEAREST", overviewlist=[2, 4])

    ds = None

    fileobj = open(f"{tif_fname}.ovr", mode="rb")
    binvalues = array.array("b")
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Check BigTIFF signature
    assert not (
        (binvalues[2] != 0x2B or binvalues[3] != 0)
        and (binvalues[3] != 0x2B or binvalues[2] != 0)
    )


###############################################################################
# Test BIGTIFF_OVERVIEW=IF_NEEDED option


def test_tiff_ovr_21(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr21.tif")

    ds = gdal.GetDriverByName("GTiff").Create(
        tif_fname, 170000, 100000, 1, options=["SPARSE_OK=YES"]
    )
    ds = None

    ds = gdal.Open(tif_fname)

    assert ds is not None, "Failed to open test dataset."

    # 170 k * 100 k = 17 GB. 17 GB / (2^2) = 4.25 GB > 4.2 GB
    # so BigTIFF is needed
    ds.BuildOverviews("NONE", overviewlist=[2])

    ds = None

    fileobj = open(f"{tif_fname}.ovr", mode="rb")
    binvalues = array.array("b")
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Check BigTIFF signature
    assert not (
        (binvalues[2] != 0x2B or binvalues[3] != 0)
        and (binvalues[3] != 0x2B or binvalues[2] != 0)
    )


###############################################################################
# Test BIGTIFF_OVERVIEW=NO option when BigTIFF is really needed


def test_tiff_ovr_22(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr22.tif")

    ds = gdal.GetDriverByName("GTiff").Create(
        tif_fname, 170000, 100000, 1, options=["SPARSE_OK=YES"]
    )
    ds = None

    ds = gdal.Open(tif_fname)

    assert ds is not None, "Failed to open test dataset."

    # 170 k * 100 k = 17 GB. 17 GB / (2^2) = 4.25 GB > 4.2 GB
    # so BigTIFF is needed
    with gdaltest.config_option("BIGTIFF_OVERVIEW", "NO"):
        with gdal.quiet_errors():
            err = ds.BuildOverviews("NONE", overviewlist=[2])

    ds = None

    if err != 0:
        return
    pytest.fail()


###############################################################################
# Same as before, but BigTIFF might be not needed as we use a compression
# method for the overviews.


def test_tiff_ovr_23(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr23.tif")

    ds = gdal.GetDriverByName("GTiff").Create(
        tif_fname, 170000, 100000, 1, options=["SPARSE_OK=YES"]
    )
    ds = None

    ds = gdal.Open(tif_fname)

    assert ds is not None, "Failed to open test dataset."

    ds.BuildOverviews(
        "NONE", overviewlist=[2], options=["BIGTIFF=NO", "COMPRESS=DEFLATE"]
    )

    ds = None

    fileobj = open(f"{tif_fname}.ovr", mode="rb")
    binvalues = array.array("b")
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Check Classical TIFF signature
    assert not (
        (binvalues[2] != 0x2A or binvalues[3] != 0)
        and (binvalues[3] != 0x2A or binvalues[2] != 0)
    )


###############################################################################
# Test BIGTIFF_OVERVIEW=IF_SAFER option


def test_tiff_ovr_24(tmp_path, both_endian):
    tif_fname = str(tmp_path / "ovr24.tif")

    ds = gdal.GetDriverByName("GTiff").Create(
        tif_fname, 85000, 100000, 1, options=["SPARSE_OK=YES"]
    )
    ds = None

    ds = gdal.Open(tif_fname)

    assert ds is not None, "Failed to open test dataset."

    # 85 k * 100 k = 8.5 GB, so BigTIFF might be needed as
    # 8.5 GB / 2 > 4.2 GB
    with gdaltest.config_option("BIGTIFF_OVERVIEW", "IF_SAFER"):
        ds.BuildOverviews("NONE", overviewlist=[16])

    ds = None

    fileobj = open(tif_fname + ".ovr", mode="rb")
    binvalues = array.array("b")
    binvalues.fromfile(fileobj, 4)
    fileobj.close()

    # Check BigTIFF signature
    assert not (
        (binvalues[2] != 0x2B or binvalues[3] != 0)
        and (binvalues[3] != 0x2B or binvalues[2] != 0)
    )


###############################################################################
# Test creating overviews after some blocks have been written in the main
# band and actually flushed


@pytest.fixture()
def tiff_with_ovr(tmp_path):
    tiff_fname = str(tmp_path / "ovr.tif")

    ds = gdal.GetDriverByName("GTiff").Create(tiff_fname, 100, 100, 1)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).FlushCache()
    ds.BuildOverviews("NEAR", overviewlist=[2])

    return tiff_fname


def test_tiff_ovr_25(tiff_with_ovr, both_endian):

    ds = gdal.Open(tiff_with_ovr)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 10000

    assert ds.GetRasterBand(1).GetOverviewCount() != 0

    assert ds.GetRasterBand(1).GetOverview(0).Checksum() == 2500


###############################################################################
# Test gdal.RegenerateOverview()


def test_tiff_ovr_26(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr26.tif")

    ds = gdal.GetDriverByName("GTiff").Create(tif_fname, 100, 100, 1)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).FlushCache()
    ds.BuildOverviews("NEAR", overviewlist=[2])
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds.GetRasterBand(1).GetOverview(0).Fill(0)
    cs_new = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs_new == 0
    gdal.RegenerateOverview(
        ds.GetRasterBand(1), ds.GetRasterBand(1).GetOverview(0), "NEAR"
    )
    cs_new = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == cs_new
    ds = None


###############################################################################
# Test gdal.RegenerateOverviews()


def test_tiff_ovr_27(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr27.tif")

    ds = gdal.GetDriverByName("GTiff").Create(tif_fname, 100, 100, 1)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).FlushCache()
    ds.BuildOverviews("NEAR", overviewlist=[2, 4])
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs2 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    ds.GetRasterBand(1).GetOverview(0).Fill(0)
    ds.GetRasterBand(1).GetOverview(1).Fill(0)
    cs_new = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs2_new = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs_new == 0 and cs2_new == 0
    gdal.RegenerateOverviews(
        ds.GetRasterBand(1),
        [ds.GetRasterBand(1).GetOverview(0), ds.GetRasterBand(1).GetOverview(1)],
        "NEAR",
    )
    cs_new = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs2_new = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs == cs_new
    assert cs2 == cs2_new
    ds = None


###############################################################################
# Test cleaning overviews.


def test_tiff_ovr_28(tiff_with_ovr, both_endian):

    ds = gdal.Open(tiff_with_ovr, gdal.GA_Update)
    assert (
        ds.BuildOverviews(overviewlist=[]) == 0
    ), "BuildOverviews() returned error code."

    assert (
        ds.GetRasterBand(1).GetOverviewCount() == 0
    ), "Overview(s) appear to still exist."

    # Close and reopen to confirm they are really gone.
    ds = None
    ds = gdal.Open(tiff_with_ovr)
    assert (
        ds.GetRasterBand(1).GetOverviewCount() == 0
    ), "Overview(s) appear to still exist after reopen."


###############################################################################
# Test cleaning external overviews (ovr) on a non-TIFF format.


@pytest.mark.require_driver("PNG")
def test_tiff_ovr_29(tmp_path, both_endian):

    png_fname = str(tmp_path / "ovr29.png")

    src_ds = gdal.Open("data/byte.tif")
    png_ds = gdal.GetDriverByName("PNG").CreateCopy(png_fname, src_ds)
    src_ds = None

    png_ds.BuildOverviews(overviewlist=[2])
    png_ds = None

    assert open(f"{png_fname}.ovr") is not None, "Did not expected .ovr file."

    png_ds = gdal.Open(png_fname)

    assert png_ds.GetRasterBand(1).GetOverviewCount() == 1, "did not find overview"

    png_ds.BuildOverviews(overviewlist=[])
    assert png_ds.GetRasterBand(1).GetOverviewCount() == 0, "delete overview failed."

    png_ds = None
    png_ds = gdal.Open(png_fname)

    assert png_ds.GetRasterBand(1).GetOverviewCount() == 0, "delete overview failed."

    png_ds = None

    assert not os.path.exists(f"{png_fname}.ovr")


###############################################################################
# Test fix for #2988.


def test_tiff_ovr_30(tmp_path, both_endian):
    tif_fname = str(tmp_path / "ovr30.tif")

    ds = gdal.GetDriverByName("GTiff").Create(tif_fname, 20, 20, 1)
    ds.BuildOverviews(overviewlist=[2])
    ds = None

    ds = gdal.Open(tif_fname, gdal.GA_Update)
    ds.SetMetadata({"TEST_KEY": "TestValue"})
    ds = None

    ds = gdal.GetDriverByName("GTiff").Create(tif_fname, 20, 20, 1)
    ds.BuildOverviews(overviewlist=[2])
    ds = None

    ds = gdal.Open(tif_fname, gdal.GA_Update)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())
    ds = None

    ds = gdal.Open(tif_fname)
    assert ds.GetProjectionRef().find("4326") != -1


###############################################################################
# Test fix for #3033


def test_tiff_ovr_31(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr31.tif")

    ds = gdal.GetDriverByName("GTiff").Create(tif_fname, 100, 100, 4)
    ds.GetRasterBand(1).Fill(255)
    ds.GetRasterBand(2).Fill(255)
    ds.GetRasterBand(3).Fill(255)
    ds.GetRasterBand(4).Fill(255)
    ds.BuildOverviews("average", overviewlist=[2, 4])
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    expected_cs = 7646
    assert cs == expected_cs, "Checksum is %d. Expected checksum is %d" % (
        cs,
        expected_cs,
    )
    ds = None


###############################################################################
# Test Cubic sampling.


def test_tiff_ovr_32(tmp_path, both_endian):
    tif_fname = str(tmp_path / "ovr32.tif")

    # 4 regular band
    shutil.copyfile("data/stefan_full_rgba_photometric_rgb.tif", tif_fname)

    ds = gdal.Open(tif_fname, gdal.GA_Update)
    ds.BuildOverviews("cubic", overviewlist=[2, 5])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 21168
    assert (
        cs == expected_cs
    ), "Checksum is %d. Expected checksum is %d for overview 0." % (cs, expected_cs)

    cs = ds.GetRasterBand(3).GetOverview(1).Checksum()
    expected_cs = 1851
    assert (
        cs == expected_cs
    ), "Checksum is %d. Expected checksum is %d for overview 1." % (cs, expected_cs)

    ds = None

    gdal.GetDriverByName("GTiff").Delete(tif_fname)

    # Same, but with non-byte data type (help testing the non-SSE2 code path)
    src_ds = gdal.Open("data/stefan_full_rgba_photometric_rgb.tif")

    tmp_ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/ovr32_float.tif",
        src_ds.RasterXSize,
        src_ds.RasterYSize,
        src_ds.RasterCount,
        gdal.GDT_Float32,
    )
    src_data = src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize)
    tmp_ds.WriteRaster(
        0, 0, src_ds.RasterXSize, src_ds.RasterYSize, src_data, buf_type=gdal.GDT_Byte
    )
    tmp_ds.BuildOverviews("cubic", overviewlist=[2])

    tmp2_ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/ovr32_byte.tif",
        tmp_ds.RasterXSize,
        tmp_ds.RasterYSize,
        tmp_ds.RasterCount,
    )
    tmp2_ds.BuildOverviews("NONE", overviewlist=[2])
    tmp2_ovr_ds = tmp2_ds.GetRasterBand(1).GetOverview(0).GetDataset()
    tmp_ovr_ds = tmp_ds.GetRasterBand(1).GetOverview(0).GetDataset()
    src_data = tmp_ovr_ds.ReadRaster(
        0, 0, tmp_ovr_ds.RasterXSize, tmp_ovr_ds.RasterYSize, buf_type=gdal.GDT_Byte
    )
    tmp2_ovr_ds.WriteRaster(
        0, 0, tmp_ovr_ds.RasterXSize, tmp_ovr_ds.RasterYSize, src_data
    )

    cs = tmp2_ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 21168
    assert (
        cs == expected_cs
    ), "Checksum is %d. Expected checksum is %d for overview 0." % (cs, expected_cs)

    src_ds = None
    tmp_ds = None
    tmp2_ds = None
    gdal.GetDriverByName("GTiff").Delete("/vsimem/ovr32_float.tif")
    gdal.GetDriverByName("GTiff").Delete("/vsimem/ovr32_byte.tif")

    # Test GDALRegenerateOverviewsMultiBand
    shutil.copyfile("data/stefan_full_rgba_photometric_rgb.tif", tif_fname)

    ds = gdal.Open(tif_fname)
    with gdaltest.config_option("COMPRESS_OVERVIEW", "DEFLATE"):
        with gdaltest.config_option("INTERLEAVE_OVERVIEW", "PIXEL"):
            ds.BuildOverviews("cubic", overviewlist=[2, 5])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 21168
    assert (
        cs == expected_cs
    ), "Checksum is %d. Expected checksum is %d for overview 0." % (cs, expected_cs)

    cs = ds.GetRasterBand(3).GetOverview(1).Checksum()
    expected_cs = 1851
    assert (
        cs == expected_cs
    ), "Checksum is %d. Expected checksum is %d for overview 1." % (cs, expected_cs)

    ds = None

    gdal.GetDriverByName("GTiff").Delete(tif_fname)

    # 3 bands + alpha
    shutil.copyfile("data/stefan_full_rgba.tif", tif_fname)

    ds = gdal.Open(tif_fname, gdal.GA_Update)
    ds.BuildOverviews("cubic", overviewlist=[2, 5])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs_band1_overview0 = 21296
    assert (
        cs == expected_cs_band1_overview0
    ), "Checksum is %d. Expected checksum is %d for overview 0." % (
        cs,
        expected_cs_band1_overview0,
    )

    cs = ds.GetRasterBand(3).GetOverview(1).Checksum()
    expected_cs_band3_overview1 = 1994
    assert (
        cs == expected_cs_band3_overview1
    ), "Checksum is %d. Expected checksum is %d for overview 1." % (
        cs,
        expected_cs_band3_overview1,
    )

    ds = None

    gdal.GetDriverByName("GTiff").Delete(tif_fname)

    # Same, but with non-byte data type (help testing the non-SSE2 code path)
    src_ds = gdal.Open("data/stefan_full_rgba.tif")

    tmp_ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/ovr32_float.tif",
        src_ds.RasterXSize,
        src_ds.RasterYSize,
        src_ds.RasterCount,
        gdal.GDT_Float32,
    )
    src_data = src_ds.ReadRaster(0, 0, src_ds.RasterXSize, src_ds.RasterYSize)
    tmp_ds.WriteRaster(
        0, 0, src_ds.RasterXSize, src_ds.RasterYSize, src_data, buf_type=gdal.GDT_Byte
    )
    tmp_ds.BuildOverviews("cubic", overviewlist=[2])

    tmp2_ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/ovr32_byte.tif",
        tmp_ds.RasterXSize,
        tmp_ds.RasterYSize,
        tmp_ds.RasterCount,
    )
    tmp2_ds.BuildOverviews("NONE", overviewlist=[2])
    tmp2_ovr_ds = tmp2_ds.GetRasterBand(1).GetOverview(0).GetDataset()
    tmp_ovr_ds = tmp_ds.GetRasterBand(1).GetOverview(0).GetDataset()
    src_data = tmp_ovr_ds.ReadRaster(
        0, 0, tmp_ovr_ds.RasterXSize, tmp_ovr_ds.RasterYSize, buf_type=gdal.GDT_Byte
    )
    tmp2_ovr_ds.WriteRaster(
        0, 0, tmp_ovr_ds.RasterXSize, tmp_ovr_ds.RasterYSize, src_data
    )

    cs = tmp2_ds.GetRasterBand(1).GetOverview(0).Checksum()
    # expected_cs = 21656
    expected_cs = 21168
    assert (
        cs == expected_cs
    ), "Checksum is %d. Expected checksum is %d for overview 0." % (cs, expected_cs)

    src_ds = None
    tmp_ds = None
    tmp2_ds = None
    gdal.GetDriverByName("GTiff").Delete("/vsimem/ovr32_float.tif")
    gdal.GetDriverByName("GTiff").Delete("/vsimem/ovr32_byte.tif")

    # Same test with a compressed dataset
    src_ds = gdal.Open("data/stefan_full_rgba.tif")
    ds = gdal.GetDriverByName("GTiff").CreateCopy(
        tif_fname, src_ds, options=["COMPRESS=DEFLATE"]
    )
    ds.BuildOverviews("cubic", overviewlist=[2, 5])

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert (
        cs == expected_cs_band1_overview0
    ), "Checksum is %d. Expected checksum is %d for overview 0." % (
        cs,
        expected_cs_band1_overview0,
    )

    cs = ds.GetRasterBand(3).GetOverview(1).Checksum()
    assert (
        cs == expected_cs_band3_overview1
    ), "Checksum is %d. Expected checksum is %d for overview 1." % (
        cs,
        expected_cs_band3_overview1,
    )

    ds = None


###############################################################################
# Test creation of overviews on a 1x1 dataset (fix for #3069)


def test_tiff_ovr_33(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr33.tif")

    ds = gdal.GetDriverByName("GTiff").Create(tif_fname, 1, 1, 1)
    ds = None
    ds = gdal.Open(tif_fname)
    ds.BuildOverviews("NEAREST", overviewlist=[2, 4])
    ds = None


###############################################################################
# Confirm that overviews are used on a Band.RasterIO().


def test_tiff_ovr_34(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr34.tif")

    ds_in = gdal.Open("data/byte.tif")
    ds = gdal.GetDriverByName("GTiff").CreateCopy(tif_fname, ds_in)
    ds.BuildOverviews("NEAREST", overviewlist=[2])
    ds.GetRasterBand(1).GetOverview(0).Fill(32.0)
    ds = None
    ds_in = None

    ds = gdal.Open(tif_fname)
    data = ds.GetRasterBand(1).ReadRaster(0, 0, 20, 20, buf_xsize=5, buf_ysize=5)
    ds = None

    if data != "                         ".encode("ascii"):
        print("[%s]" % data)
        pytest.fail("did not get expected cleared overview.")


###############################################################################
# Confirm that overviews are used on a Band.RasterIO().
# FORCE_CACHING=YES confirms that overviews are used on a Band.RasterIO() when
# using BlockBasedRasterIO() (#3124)


@pytest.mark.parametrize("force_caching", ("NO", "YES"))
def test_tiff_ovr_35(tmp_path, force_caching, both_endian):

    tif_fname = str(tmp_path / "ovr35.tif")

    with gdal.config_option("GDAL_FORCE_CACHING", force_caching):

        ds_in = gdal.Open("data/byte.tif")
        ds = gdal.GetDriverByName("GTiff").CreateCopy(tif_fname, ds_in)
        ds.BuildOverviews("NEAREST", overviewlist=[2])
        ds.GetRasterBand(1).GetOverview(0).Fill(32.0)
        ds = None
        ds_in = None

        ds = gdal.Open(tif_fname)
        data = ds.ReadRaster(0, 0, 20, 20, buf_xsize=5, buf_ysize=5, band_list=[1])
        ds = None

        if data != "                         ".encode("ascii"):
            print("[%s]" % data)
            pytest.fail("did not get expected cleared overview.")


###############################################################################
# Test PREDICTOR_OVERVIEW=2 option. (#3414)


@pytest.mark.require_driver("DTED")
def test_tiff_ovr_37(tmp_path, both_endian):

    ovr37_dt0 = str(tmp_path / "ovr37.dt0")

    shutil.copy("../gdrivers/data/n43.dt0", ovr37_dt0)

    ds = gdal.Open(ovr37_dt0)
    with gdaltest.config_option("COMPRESS_OVERVIEW", "LZW"):
        ds.BuildOverviews("NEAR", overviewlist=[2])
    ds = None
    no_predictor_size = os.stat(f"{ovr37_dt0}.ovr")[stat.ST_SIZE]
    os.unlink(f"{ovr37_dt0}.ovr")

    ds = gdal.Open(ovr37_dt0)
    with gdaltest.config_option("PREDICTOR_OVERVIEW", "2"):
        with gdaltest.config_option("COMPRESS_OVERVIEW", "LZW"):
            ds.BuildOverviews("NEAR", overviewlist=[2])

    ds = None

    ds = gdal.Open(ovr37_dt0)
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 45378, "got wrong overview checksum."
    ds = None

    predictor2_size = os.stat(f"{ovr37_dt0}.ovr")[stat.ST_SIZE]
    assert predictor2_size < no_predictor_size


###############################################################################
# Test that the predictor flag gets well propagated to internal overviews


@pytest.mark.require_driver("DTED")
def test_tiff_ovr_38(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr38.tif")

    src_ds = gdal.Open("../gdrivers/data/n43.dt0")
    ds = gdal.GetDriverByName("GTiff").CreateCopy(
        tif_fname, src_ds, options=["COMPRESS=LZW", "PREDICTOR=2"]
    )
    ds.BuildOverviews(overviewlist=[2, 4])
    ds = None

    file_size = os.stat(tif_fname)[stat.ST_SIZE]

    assert file_size <= 21000, "did not get expected file size."


###############################################################################
# Test external overviews on all datatypes


@pytest.mark.parametrize(
    "datatype",
    (
        gdal.GDT_Byte,
        gdal.GDT_Int16,
        gdal.GDT_UInt16,
        gdal.GDT_Int32,
        gdal.GDT_UInt32,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
        gdal.GDT_CInt16,
        gdal.GDT_CInt32,
        gdal.GDT_CFloat32,
        gdal.GDT_CFloat64,
    ),
)
def test_tiff_ovr_39(tmp_path, datatype, both_endian):
    tif_fname = str(tmp_path / "ovr39.tif")

    gdal.Translate(
        tif_fname,
        "data/byte.tif",
        options="-ot " + gdal.GetDataTypeName(datatype),
    )

    ds = gdal.Open(tif_fname)
    ds.BuildOverviews("NEAREST", overviewlist=[2])
    ds = None

    ds = gdal.Open(tif_fname + ".ovr")
    ovr_datatype = ds.GetRasterBand(1).DataType
    ds = None

    assert datatype == ovr_datatype, "did not get expected datatype"

    ds = gdal.Open(tif_fname)
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds = None

    if gdal.DataTypeIsComplex(datatype):
        expected_cs = 1171
    else:
        expected_cs = 1087

    assert (
        cs == expected_cs
    ), "did not get expected checksum for datatype %s" % gdal.GetDataTypeName(datatype)


###############################################################################
# Test external overviews on 1 bit datasets with AVERAGE_BIT2GRAYSCALE (similar to tiff_ovr_4)


def test_tiff_ovr_40(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr40.tif")

    shutil.copyfile("data/oddsize_1bit2b.tif", tif_fname)

    wrk_ds = gdal.Open(tif_fname)

    assert wrk_ds is not None, "Failed to open test dataset."

    wrk_ds.BuildOverviews("AVERAGE_BIT2GRAYSCALE", overviewlist=[2, 4])
    wrk_ds = None

    wrk_ds = gdal.Open(tif_fname)

    ovband = wrk_ds.GetRasterBand(1).GetOverview(1)
    md = ovband.GetMetadata()
    assert (
        "RESAMPLING" in md and md["RESAMPLING"] == "AVERAGE_BIT2GRAYSCALE"
    ), "Did not get expected RESAMPLING metadata."

    # compute average value of overview band image data.
    ovimage = ovband.ReadRaster(0, 0, ovband.XSize, ovband.YSize)

    pix_count = ovband.XSize * ovband.YSize
    total = float(sum(ovimage))
    average = total / pix_count
    exp_average = 154.0992
    assert average == pytest.approx(
        exp_average, abs=0.1
    ), "got wrong average for overview image"

    # Read base band as overview resolution and verify we aren't getting
    # the grayscale image.

    frband = wrk_ds.GetRasterBand(1)
    ovimage = frband.ReadRaster(
        0, 0, frband.XSize, frband.YSize, ovband.XSize, ovband.YSize
    )

    pix_count = ovband.XSize * ovband.YSize
    total = float(sum(ovimage))
    average = total / pix_count
    exp_average = 0.6096

    assert average == pytest.approx(
        exp_average, abs=0.01
    ), "got wrong average for downsampled image"

    wrk_ds = None


###############################################################################
# Test external overviews on 1 bit datasets with NEAREST


def test_tiff_ovr_41(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr41.tif")

    shutil.copyfile("data/oddsize_1bit2b.tif", tif_fname)

    ds = gdal.Open(tif_fname)
    # data = wrk_ds.GetRasterBand(1).ReadRaster(0,0,99,99,50,50)
    ds.BuildOverviews("NEAREST", overviewlist=[2])
    ds = None

    # ds = gdaltest.tiff_drv.Create('tmp/ovr41.tif.handmade.ovr',50,50,1,options=['NBITS=1'])
    # ds.GetRasterBand(1).WriteRaster(0,0,50,50,data)
    # ds = None

    ds = gdal.Open(tif_fname)
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    ds = None

    assert cs == 1496, "did not get expected checksum"


###############################################################################
# Test external overviews on dataset with color table


def test_tiff_ovr_42(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr42.tif")

    ct_data = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)]

    ct = gdal.ColorTable()
    for i, data in enumerate(ct_data):
        ct.SetColorEntry(i, data)

    ds = gdal.GetDriverByName("GTiff").Create(tif_fname, 1, 1)
    ds.GetRasterBand(1).SetRasterColorTable(ct)
    ds = None

    ds = gdal.Open(tif_fname)
    ds.BuildOverviews("NEAREST", overviewlist=[2])
    ds = None

    ds = gdal.Open(f"{tif_fname}.ovr")
    ct2 = ds.GetRasterBand(1).GetRasterColorTable()

    assert (
        ct2.GetCount() == 256
        and ct2.GetColorEntry(0) == (255, 0, 0, 255)
        and ct2.GetColorEntry(1) == (0, 255, 0, 255)
        and ct2.GetColorEntry(2) == (0, 0, 255, 255)
        and ct2.GetColorEntry(3) == (255, 255, 255, 255)
    ), "Wrong color table entry."

    ds = None


###############################################################################
# Make sure that 16bit overviews with JPEG compression are handled using 12-bit
# jpeg-in-tiff (#3539)


@pytest.mark.skipif(
    "SKIP_TIFF_JPEG12" in os.environ, reason="Crashes on build-windows-msys2-mingw"
)
@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_tiff_ovr_43(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr43.tif")

    with gdaltest.config_option("CPL_ACCUM_ERROR_MSG", "ON"):
        gdal.ErrorReset()
        with gdal.quiet_errors():
            try:
                ds = gdal.Open("data/mandrilmini_12bitjpeg.tif")
                ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
            except Exception:
                ds = None

    if gdal.GetLastErrorMsg().find("Unsupported JPEG data precision 12") != -1:
        pytest.skip("12bit jpeg not available")

    ds = gdal.GetDriverByName("GTiff").Create(tif_fname, 16, 16, 1, gdal.GDT_UInt16)
    ds.GetRasterBand(1).Fill(4000)
    ds = None

    ds = gdal.Open(tif_fname)
    with gdaltest.config_option("COMPRESS_OVERVIEW", "JPEG"):
        ds.BuildOverviews("NEAREST", overviewlist=[2])
        ds = None

    ds = gdal.Open(f"{tif_fname}.ovr")
    md = ds.GetRasterBand(1).GetMetadata("IMAGE_STRUCTURE")
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert "NBITS" in md and md["NBITS"] == "12", "did not get expected NBITS"

    assert cs == 642, "did not get expected checksum"


###############################################################################
# Test that we can change overview block size through GDAL_TIFF_OVR_BLOCKSIZE configuration
# option


def test_tiff_ovr_44(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr44.tif")

    shutil.copyfile("data/byte.tif", tif_fname)
    with gdaltest.config_option("GDAL_TIFF_OVR_BLOCKSIZE", "256"):
        ds = gdal.Open(tif_fname, gdal.GA_Update)
        ds.BuildOverviews(overviewlist=[2])
        ds = None

    ds = gdal.Open(tif_fname)
    ovr_band = ds.GetRasterBand(1).GetOverview(0)
    if "GetBlockSize" in dir(gdal.Band):
        (blockx, blocky) = ovr_band.GetBlockSize()
        assert blockx == 256 and blocky == 256, "did not get expected block size"
    cs = ovr_band.Checksum()
    ds = None

    assert cs == 1087, "did not get expected checksum"


###############################################################################
# Same as tiff_ovr_44, but with external overviews


def test_tiff_ovr_45(tmp_path, both_endian):

    tif_fname = str(tmp_path / "ovr45.tif")

    shutil.copyfile("data/byte.tif", tif_fname)
    with gdaltest.config_option("GDAL_TIFF_OVR_BLOCKSIZE", "256"):
        ds = gdal.Open(tif_fname, gdal.GA_ReadOnly)
        ds.BuildOverviews(overviewlist=[2])
        ds = None

    ds = gdal.Open(f"{tif_fname}.ovr")
    ovr_band = ds.GetRasterBand(1)
    if "GetBlockSize" in dir(gdal.Band):
        (blockx, blocky) = ovr_band.GetBlockSize()
        assert blockx == 256 and blocky == 256, "did not get expected block size"
    cs = ovr_band.Checksum()
    ds = None

    assert cs == 1087, "did not get expected checksum"


###############################################################################
# Test that SPARSE_OK creation option propagates on internal overviews


@pytest.mark.parametrize("apply_sparse", [False, True])
def test_tiff_ovr_propagate_sparse_ok_creation_option(apply_sparse):

    filename = "/vsimem/test_tiff_ovr_propagate_sparse_ok_creation_option.tif"
    ds = gdal.GetDriverByName("GTiff").Create(
        filename, 100, 100, options=["SPARSE_OK=YES"] if apply_sparse else []
    )
    ds.BuildOverviews("NEAREST", overviewlist=[2])
    ds = None
    ds = gdal.Open(filename)
    has_block = (
        ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
        is not None
    )
    if apply_sparse:
        assert not has_block
    else:
        assert has_block
    ds = None

    gdal.GetDriverByName("GTiff").Delete(filename)


###############################################################################
# Test that SPARSE_OK open option propagates on internal overviews


@pytest.mark.parametrize("apply_sparse", [False, True])
def test_tiff_ovr_propagate_sparse_ok_open_option_internal(apply_sparse):

    filename = "/vsimem/test_tiff_ovr_propagate_sparse_ok_open_option_internal.tif"
    gdal.GetDriverByName("GTiff").Create(filename, 100, 100)
    ds = gdal.OpenEx(
        filename,
        gdal.OF_UPDATE | gdal.OF_RASTER,
        open_options=["SPARSE_OK=YES"] if apply_sparse else [],
    )
    ds.BuildOverviews("NEAREST", overviewlist=[2])
    ds = None
    ds = gdal.Open(filename)
    has_block = (
        ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
        is not None
    )
    if apply_sparse:
        assert not has_block
    else:
        assert has_block
    ds = None

    gdal.GetDriverByName("GTiff").Delete(filename)


###############################################################################
# Test that SPARSE_OK open option propagates on internal overviews


@pytest.mark.parametrize("apply_sparse", [False, True])
def test_tiff_ovr_propagate_sparse_ok_open_option_external(apply_sparse):

    filename = "/vsimem/test_tiff_ovr_propagate_sparse_ok_open_option_external.tif"
    gdal.GetDriverByName("GTiff").Create(filename, 100, 100)
    ds = gdal.OpenEx(filename, open_options=["SPARSE_OK=YES"] if apply_sparse else [])
    ds.BuildOverviews("NEAREST", overviewlist=[2])
    ds = None
    ds = gdal.Open(filename)
    has_block = (
        ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
        is not None
    )
    if apply_sparse:
        assert not has_block
    else:
        assert has_block
    ds = None

    gdal.GetDriverByName("GTiff").Delete(filename)


###############################################################################
# Test SPARSE_OK_OVERVIEW on internal overview


@pytest.mark.parametrize("apply_sparse", [False, True])
def test_tiff_ovr_sparse_ok_internal_overview(apply_sparse):

    filename = "/vsimem/test_tiff_ovr_sparse_ok_internal_overview.tif"
    gdal.GetDriverByName("GTiff").Create(filename, 100, 100)
    ds = gdal.Open(filename, gdal.GA_Update)
    with gdaltest.config_options({"SPARSE_OK_OVERVIEW": "YES"} if apply_sparse else {}):
        ds.BuildOverviews("NEAREST", overviewlist=[2])
    ds = None
    ds = gdal.Open(filename)
    has_block = (
        ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
        is not None
    )
    if apply_sparse:
        assert not has_block
    else:
        assert has_block
    ds = None

    gdal.GetDriverByName("GTiff").Delete(filename)


###############################################################################
# Test SPARSE_OK_OVERVIEW on external overview


@pytest.mark.parametrize("apply_sparse", [False, True])
def test_tiff_ovr_sparse_ok_external_overview(apply_sparse):

    filename = "/vsimem/test_tiff_ovr_sparse_ok_external_overview.tif"
    gdal.GetDriverByName("GTiff").Create(filename, 100, 100)
    ds = gdal.Open(filename)
    with gdaltest.config_options({"SPARSE_OK_OVERVIEW": "YES"} if apply_sparse else {}):
        ds.BuildOverviews("NEAREST", overviewlist=[2])
    ds = None
    ds = gdal.Open(filename)
    has_block = (
        ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF")
        is not None
    )
    if apply_sparse:
        assert not has_block
    else:
        assert has_block
    ds = None

    gdal.GetDriverByName("GTiff").Delete(filename)


###############################################################################
# Test overview on a dataset where width * height > 2 billion


@pytest.mark.slow()
def test_tiff_ovr_46():

    # Test NEAREST
    with gdaltest.config_option("GTIFF_DONT_WRITE_BLOCKS", "YES"):
        ds = gdal.GetDriverByName("GTiff").Create(
            "/vsimem/tiff_ovr_46.tif", 50000, 50000, options=["SPARSE_OK=YES"]
        )
        ds.BuildOverviews("NEAREST", overviewlist=[2])
        ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_46.tif")

    # Test AVERAGE in optimized case (x2 reduction)
    with gdaltest.config_option("GTIFF_DONT_WRITE_BLOCKS", "YES"):
        ds = gdal.GetDriverByName("GTiff").Create(
            "/vsimem/tiff_ovr_46.tif", 50000, 50000, options=["SPARSE_OK=YES"]
        )
        ds.BuildOverviews("AVERAGE", overviewlist=[2])
        ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_46.tif")

    # Test AVERAGE in un-optimized case (x3 reduction)
    with gdaltest.config_option("GTIFF_DONT_WRITE_BLOCKS", "YES"):
        ds = gdal.GetDriverByName("GTiff").Create(
            "/vsimem/tiff_ovr_46.tif", 50000, 50000, options=["SPARSE_OK=YES"]
        )
        ds.BuildOverviews("AVERAGE", overviewlist=[3])
        ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_46.tif")

    # Test AVERAGE in un-optimized case (color table)
    with gdaltest.config_option("GTIFF_DONT_WRITE_BLOCKS", "YES"):
        ds = gdal.GetDriverByName("GTiff").Create(
            "/vsimem/tiff_ovr_46.tif", 50000, 50000, options=["SPARSE_OK=YES"]
        )

        ct = gdal.ColorTable()
        ct.SetColorEntry(0, (255, 0, 0))
        ds.GetRasterBand(1).SetRasterColorTable(ct)

        ds.BuildOverviews("AVERAGE", overviewlist=[2])
        ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_46.tif")

    # Test GAUSS
    with gdaltest.config_option("GTIFF_DONT_WRITE_BLOCKS", "YES"):
        ds = gdal.GetDriverByName("GTiff").Create(
            "/vsimem/tiff_ovr_46.tif", 50000, 50000, options=["SPARSE_OK=YES"]
        )
        ds.BuildOverviews("GAUSS", overviewlist=[2])
        ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_46.tif")

    # Test GAUSS with color table
    with gdaltest.config_option("GTIFF_DONT_WRITE_BLOCKS", "YES"):
        ds = gdal.GetDriverByName("GTiff").Create(
            "/vsimem/tiff_ovr_46.tif", 50000, 50000, options=["SPARSE_OK=YES"]
        )

        ct = gdal.ColorTable()
        ct.SetColorEntry(0, (255, 0, 0))
        ds.GetRasterBand(1).SetRasterColorTable(ct)

        ds.BuildOverviews("GAUSS", overviewlist=[2])
        ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_46.tif")

    # Test MODE
    with gdaltest.config_option("GTIFF_DONT_WRITE_BLOCKS", "YES"):
        ds = gdal.GetDriverByName("GTiff").Create(
            "/vsimem/tiff_ovr_46.tif", 50000, 50000, options=["SPARSE_OK=YES"]
        )
        ds.BuildOverviews("MODE", overviewlist=[2])
        ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_46.tif")

    # Test CUBIC
    with gdaltest.config_option("GTIFF_DONT_WRITE_BLOCKS", "YES"):
        ds = gdal.GetDriverByName("GTiff").Create(
            "/vsimem/tiff_ovr_46.tif", 50000, 50000, options=["SPARSE_OK=YES"]
        )
        ds.BuildOverviews("CUBIC", overviewlist=[2])
        ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_46.tif")


###############################################################################
# Test workaround with libtiff 3.X when creating interleaved overviews


def test_tiff_ovr_47(both_endian):
    mem_drv = gdal.GetDriverByName("MEM")
    mem_ds = mem_drv.Create("", 852, 549, 3)

    for i in range(1, 4):
        band = mem_ds.GetRasterBand(i)
        band.Fill(128)

    driver = gdal.GetDriverByName("GTIFF")
    out_ds = driver.CreateCopy("/vsimem/tiff_ovr_47.tif", mem_ds)
    mem_ds = None
    out_ds.BuildOverviews("NEAREST", [2, 4, 8, 16])
    out_ds = None

    ds = gdal.Open("/vsimem/tiff_ovr_47.tif")
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    ds = None

    gdal.Unlink("/vsimem/tiff_ovr_47.tif")

    assert cs == 35721, "did not get expected checksum"


###############################################################################
# Test that we don't average 0's in alpha band


def test_tiff_ovr_48(tmp_path, both_endian):

    tif_fname = str(tmp_path / "rgba_with_alpha_0_and_255.tif")

    shutil.copy("data/rgba_with_alpha_0_and_255.tif", tif_fname)
    ds = gdal.Open(tif_fname)
    ds.BuildOverviews("AVERAGE", [2])
    ds = None

    ds = gdal.Open(f"{tif_fname}.ovr")
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        assert cs == 3, i

    # But if we define GDAL_OVR_PROPAGATE_NODATA, a nodata value in source
    # samples will cause the target pixel to be zeroed.
    shutil.copy("data/rgba_with_alpha_0_and_255.tif", tif_fname)
    ds = gdal.Open(tif_fname)
    with gdaltest.config_option("GDAL_OVR_PROPAGATE_NODATA", "YES"):
        ds.BuildOverviews("AVERAGE", [2])
    ds = None

    ds = gdal.Open(f"{tif_fname}.ovr")
    for i in range(4):
        cs = ds.GetRasterBand(i + 1).Checksum()
        assert cs == 0, i


###############################################################################
# Test possible stride computation issue in GDALRegenerateOverviewsMultiBand (#5653)


def test_tiff_ovr_49(both_endian):

    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/tiff_ovr_49.tif", 1023, 1023, 1)
    ds.GetRasterBand(1).Fill(0)
    c = "\xFF"
    # Fails on 1.11.1 with col = 255 or col = 1019
    col = 1019
    ds.GetRasterBand(1).WriteRaster(col, 0, 1, 1023, c, 1, 1)
    ds = None
    ds = gdal.Open("/vsimem/tiff_ovr_49.tif")
    with gdaltest.config_option("COMPRESS_OVERVIEW", "DEFLATE"):
        ds.BuildOverviews("AVERAGE", overviewlist=[2])
    ds = None
    ds = gdal.Open("/vsimem/tiff_ovr_49.tif.ovr")
    assert ds.GetRasterBand(1).Checksum() != 0
    ds = None
    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_49.tif")


###############################################################################
# Test overviews when X dimension is smaller than Y (#5794)


def test_tiff_ovr_50(both_endian):

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/tiff_ovr_50.tif", 6, 8192, 3, options=["COMPRESS=DEFLATE"]
    )
    ds.GetRasterBand(1).Fill(255)
    # We just check that it doesn't crash
    ds.BuildOverviews("AVERAGE", overviewlist=[2, 4, 8, 16, 32])
    ds.BuildOverviews("AVERAGE", overviewlist=[2, 4, 8, 16, 32])
    ds = None
    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_50.tif")


###############################################################################
# Test average overview on a color palette with nodata values (#6371)


@pytest.mark.require_driver("PNG")
def test_tiff_ovr_51():

    src_ds = gdal.Open("data/stefan_full_rgba_pct32.png")
    if src_ds is None:
        pytest.skip()

    ds = gdal.GetDriverByName("PNG").CreateCopy("/vsimem/tiff_ovr_51.png", src_ds)
    ds.BuildOverviews("AVERAGE", [2])
    ds = None

    ds = gdal.Open("/vsimem/tiff_ovr_51.png.ovr")
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 24518
    ds = None

    gdal.GetDriverByName("PNG").Delete("/vsimem/tiff_ovr_51.png")


###############################################################################
# Test unsorted external overview building (#6617)


def test_tiff_ovr_52():

    src_ds = gdal.Open("data/byte.tif")
    if src_ds is None:
        pytest.skip()

    gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/tiff_ovr_52.tif", src_ds)
    with gdaltest.config_option("COMPRESS_OVERVIEW", "DEFLATE"):
        with gdaltest.config_option("INTERLEAVE_OVERVIEW", "PIXEL"):
            ds = gdal.Open("/vsimem/tiff_ovr_52.tif")
            ds.BuildOverviews("NEAR", [4])
            ds = None
            ds = gdal.Open("/vsimem/tiff_ovr_52.tif")
            ds.BuildOverviews("NEAR", [2])
            ds = None

    ds = gdal.Open("/vsimem/tiff_ovr_52.tif")
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 328
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs == 1087
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_52.tif")

    gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/tiff_ovr_52.tif", src_ds)
    with gdaltest.config_option("COMPRESS_OVERVIEW", "DEFLATE"):
        with gdaltest.config_option("INTERLEAVE_OVERVIEW", "PIXEL"):
            ds = gdal.Open("/vsimem/tiff_ovr_52.tif")
            ds.BuildOverviews("NEAR", [4, 2])
            ds = None

    ds = gdal.Open("/vsimem/tiff_ovr_52.tif")
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 328
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs == 1087
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_52.tif")


###############################################################################
# Test external overviews building in several steps


def test_tiff_ovr_53():

    src_ds = gdal.Open("data/byte.tif")
    if src_ds is None:
        pytest.skip()

    gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/tiff_ovr_53.tif", src_ds)
    ds = gdal.Open("/vsimem/tiff_ovr_53.tif")
    ds.BuildOverviews("NEAR", [2])
    ds = None
    # Note: currently this will compute it from the base raster and not
    # ov_factor=2 !
    ds = gdal.Open("/vsimem/tiff_ovr_53.tif")
    ds.BuildOverviews("NEAR", [4])
    ds = None

    ds = gdal.Open("/vsimem/tiff_ovr_53.tif")
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 1087
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs == 328
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_53.tif")

    # Compressed code path
    gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/tiff_ovr_53.tif", src_ds)
    with gdaltest.config_option("COMPRESS_OVERVIEW", "DEFLATE"):
        with gdaltest.config_option("INTERLEAVE_OVERVIEW", "PIXEL"):
            ds = gdal.Open("/vsimem/tiff_ovr_53.tif")
            ds.BuildOverviews("NEAR", [2])
            ds = None
            # Note: currently this will compute it from the base raster and not
            # ov_factor=2 !
            ds = gdal.Open("/vsimem/tiff_ovr_53.tif")
            ds.BuildOverviews("NEAR", [4])
            ds = None

    ds = gdal.Open("/vsimem/tiff_ovr_53.tif")
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 1087
    cs = ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert cs == 328
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_53.tif")


###############################################################################
# Test external overviews building in several steps with jpeg compression


@pytest.mark.require_creation_option("GTiff", "JPEG")
def test_tiff_ovr_54():

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/tiff_ovr_54.tif", src_ds)

    with gdaltest.config_options(
        {
            "COMPRESS_OVERVIEW": "JPEG",
            "PHOTOMETRIC_OVERVIEW": "YCBCR",
            "INTERLEAVE_OVERVIEW": "PIXEL",
        }
    ):
        ds = gdal.Open("/vsimem/tiff_ovr_54.tif")
        ds.BuildOverviews("AVERAGE", [2])
        ds = None
        ds = gdal.Open("/vsimem/tiff_ovr_54.tif")
        ds.BuildOverviews("AVERAGE", [4])
        ds = None

    ds = gdal.Open("/vsimem/tiff_ovr_54.tif")
    cs0 = ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs1 = ds.GetRasterBand(1).GetOverview(1).Checksum()
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_54.tif")

    assert not (cs0 == 0 or cs1 == 0)


###############################################################################
# Test average overview generation with nodata.


def test_tiff_ovr_55(both_endian):

    src_ds = gdal.Open("../gdrivers/data/int16.tif")
    gdal.GetDriverByName("GTiff").CreateCopy("/vsimem/tiff_ovr_55.tif", src_ds)

    wrk_ds = gdal.Open("/vsimem/tiff_ovr_55.tif")
    assert wrk_ds is not None, "Failed to open test dataset."

    wrk_ds.BuildOverviews("RMS", overviewlist=[2])
    wrk_ds = None

    wrk_ds = gdal.Open("/vsimem/tiff_ovr_55.tif")
    cs = wrk_ds.GetRasterBand(1).GetOverview(0).Checksum()
    exp_cs = 1172

    assert cs == exp_cs, "got wrong overview checksum."


###############################################################################


def test_tiff_ovr_too_many_levels_contig():

    src_ds = gdal.Open("data/byte.tif")
    tmpfilename = "/vsimem/tiff_ovr_too_many_levels_contig.tif"
    ds = gdal.GetDriverByName("GTiff").CreateCopy(tmpfilename, src_ds)
    ds.BuildOverviews("AVERAGE", [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096])
    assert ds.GetRasterBand(1).GetOverviewCount() == 5
    ds.BuildOverviews("AVERAGE", [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096])
    assert ds.GetRasterBand(1).GetOverviewCount() == 5
    ds = None
    gdal.GetDriverByName("GTiff").Delete(tmpfilename)


###############################################################################


def test_tiff_ovr_too_many_levels_separate():

    src_ds = gdal.Open("data/separate_tiled.tif")
    tmpfilename = "/vsimem/tiff_ovr_too_many_levels_separate.tif"
    ds = gdal.GetDriverByName("GTiff").CreateCopy(tmpfilename, src_ds)
    ds.BuildOverviews("AVERAGE", [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096])
    assert ds.GetRasterBand(1).GetOverviewCount() == 6
    assert ds.GetRasterBand(1).GetOverviewCount() == 6
    ds = None
    gdal.GetDriverByName("GTiff").Delete(tmpfilename)


###############################################################################


def test_tiff_ovr_too_many_levels_external():

    src_ds = gdal.Open("data/byte.tif")
    tmpfilename = "/vsimem/tiff_ovr_too_many_levels_contig.tif"
    gdal.GetDriverByName("GTiff").CreateCopy(tmpfilename, src_ds)
    ds = gdal.Open(tmpfilename)
    ds.BuildOverviews("AVERAGE", [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096])
    assert ds.GetRasterBand(1).GetOverviewCount() == 5
    ds.BuildOverviews("AVERAGE", [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096])
    assert ds.GetRasterBand(1).GetOverviewCount() == 5
    ds = None
    gdal.GetDriverByName("GTiff").Delete(tmpfilename)


###############################################################################


def test_tiff_ovr_average_multiband_vs_singleband():

    gdal.Translate(
        "/vsimem/tiff_ovr_average_multiband_band.tif",
        "data/reproduce_average_issue.tif",
        creationOptions=["INTERLEAVE=BAND"],
    )
    gdal.Translate(
        "/vsimem/tiff_ovr_average_multiband_pixel.tif",
        "data/reproduce_average_issue.tif",
        creationOptions=["INTERLEAVE=PIXEL"],
    )

    ds = gdal.Open("/vsimem/tiff_ovr_average_multiband_band.tif", gdal.GA_Update)
    ds.BuildOverviews("AVERAGE", [2])
    cs_band = [ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(3)]
    ds = None

    ds = gdal.Open("/vsimem/tiff_ovr_average_multiband_pixel.tif", gdal.GA_Update)
    ds.BuildOverviews("AVERAGE", [2])
    cs_pixel = [ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(3)]
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_average_multiband_band.tif")
    gdal.GetDriverByName("GTiff").Delete("/vsimem/tiff_ovr_average_multiband_pixel.tif")

    assert cs_band == cs_pixel


###############################################################################


def test_tiff_ovr_multithreading_multiband():

    # Test multithreading through GDALRegenerateOverviewsMultiBand
    ds = gdal.Translate(
        "/vsimem/test.tif",
        "data/stefan_full_rgba.tif",
        creationOptions=["COMPRESS=LZW", "TILED=YES", "BLOCKXSIZE=16", "BLOCKYSIZE=16"],
    )
    with gdaltest.config_options(
        {"GDAL_NUM_THREADS": "8", "GDAL_OVR_CHUNK_MAX_SIZE": "100"}
    ):
        ds.BuildOverviews("AVERAGE", [2])
    ds = None
    ds = gdal.Open("/vsimem/test.tif")
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == [
        12603,
        58561,
        36064,
        10807,
    ]
    ds = None
    gdal.Unlink("/vsimem/test.tif")


###############################################################################


def test_tiff_ovr_multithreading_singleband():

    # Test multithreading through GDALRegenerateOverviews
    ds = gdal.Translate(
        "/vsimem/test.tif",
        "data/stefan_full_rgba.tif",
        creationOptions=["INTERLEAVE=BAND"],
    )
    with gdaltest.config_options({"GDAL_NUM_THREADS": "8", "GDAL_OVR_CHUNKYSIZE": "1"}):
        ds.BuildOverviews("AVERAGE", [2, 4])
    ds = None
    ds = gdal.Open("/vsimem/test.tif")
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == [
        12603,
        58561,
        36064,
        10807,
    ]
    ds = None
    gdal.Unlink("/vsimem/test.tif")


###############################################################################


def test_tiff_ovr_multiband_code_path_degenerate():

    temp_path = "/vsimem/test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(temp_path, 5, 6)
    ds.GetRasterBand(1).Fill(255)
    del ds
    ds = gdal.OpenEx(temp_path, gdal.GA_ReadOnly)
    with gdaltest.config_option("COMPRESS_OVERVIEW", "LZW"):
        ds.BuildOverviews("nearest", overviewlist=[2, 4, 8])
    assert ds.GetRasterBand(1).GetOverview(0).Checksum() != 0
    assert ds.GetRasterBand(1).GetOverview(1).Checksum() != 0
    assert ds.GetRasterBand(1).GetOverview(2).Checksum() != 0
    del ds
    gdal.GetDriverByName("GTiff").Delete(temp_path)


###############################################################################


def test_tiff_ovr_color_table_bug_3336():

    temp_path = "/vsimem/test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(temp_path, 242, 10442)
    ct = gdal.ColorTable()
    ct.SetColorEntry(255, (255, 2552, 55))
    ds.GetRasterBand(1).SetRasterColorTable(ct)
    del ds
    ds = gdal.OpenEx(temp_path, gdal.GA_ReadOnly)
    assert ds.BuildOverviews("nearest", overviewlist=[32]) == 0
    del ds
    gdal.GetDriverByName("GTiff").Delete(temp_path)


###############################################################################


def test_tiff_ovr_color_table_bug_3336_bis():

    temp_path = "/vsimem/test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(temp_path, 128, 12531)
    ct = gdal.ColorTable()
    ct.SetColorEntry(255, (255, 2552, 55))
    ds.GetRasterBand(1).SetRasterColorTable(ct)
    del ds
    ds = gdal.OpenEx(temp_path, gdal.GA_ReadOnly)
    assert ds.BuildOverviews("nearest", overviewlist=[128]) == 0
    del ds
    gdal.GetDriverByName("GTiff").Delete(temp_path)


###############################################################################


def test_tiff_ovr_nodata_multiband():

    numpy = pytest.importorskip("numpy")

    temp_path = "/vsimem/test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(temp_path, 4, 4, 2, gdal.GDT_Float32)
    ds.GetRasterBand(1).SetNoDataValue(-10000)
    ds.GetRasterBand(1).WriteArray(numpy.array([[0.5, 1.0], [4.5, -10000]]))
    ds.GetRasterBand(2).SetNoDataValue(-10000)
    ds.GetRasterBand(2).WriteArray(numpy.array([[-10000, 4.0], [4.5, 0.5]]))

    ds.FlushCache()
    ds.BuildOverviews("AVERAGE", overviewlist=[2])
    ds.FlushCache()

    assert (
        ds.GetRasterBand(1).GetOverviewCount() == 1
    ), "Overview could not be generated"

    pix = ds.GetRasterBand(1).GetOverview(0).ReadAsArray(win_xsize=1, win_ysize=1)
    assert pix[0, 0] == 2.0

    pix = ds.GetRasterBand(2).GetOverview(0).ReadAsArray(win_xsize=1, win_ysize=1)
    assert pix[0, 0] == 3.0

    ds = None


###############################################################################


@pytest.mark.parametrize("external_ovr", [False, True])
def test_tiff_ovr_nodata_multiband_interleave_band_non_default_color_interp(
    external_ovr,
):

    nodatavalue = -10000
    data = struct.pack(
        "f" * 4 * 4,
        0.5,
        0.2,
        0.5,
        0.2,
        0.2,
        nodatavalue,
        0.2,
        nodatavalue,
        0.5,
        0.2,
        nodatavalue,
        nodatavalue,
        0.2,
        nodatavalue,
        nodatavalue,
        nodatavalue,
    )
    numbands = 5

    temp_path = "/vsimem/test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(
        temp_path,
        4,
        4,
        numbands,
        gdal.GDT_Float32,
        options=["INTERLEAVE=BAND", "PHOTOMETRIC=MINISBLACK", "ALPHA=YES"],
    )
    for i in range(1, numbands):
        ds.GetRasterBand(i).SetColorInterpretation(gdal.GCI_GreenBand)
        ds.GetRasterBand(i).SetNoDataValue(nodatavalue)
        ds.GetRasterBand(i).WriteRaster(0, 0, 4, 4, data)

    ds.GetRasterBand(numbands).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(numbands).SetNoDataValue(nodatavalue)
    ds.GetRasterBand(numbands).WriteRaster(
        0,
        0,
        4,
        4,
        struct.pack(
            "f" * 4 * 4,
            255,
            255,
            255,
            255,
            255,
            255,
            255,
            255,
            255,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
        ),
    )

    if external_ovr:
        ds = None
        ds = gdal.Open(temp_path)
    ds.BuildOverviews("AVERAGE", overviewlist=[2, 4])
    assert (
        ds.GetRasterBand(1).GetOverview(0).GetColorInterpretation()
        == gdal.GCI_GreenBand
    )

    ds = None
    ds = gdal.Open(temp_path)
    assert (
        ds.GetRasterBand(1).GetOverviewCount() == 2
    ), "Overview could not be generated"
    assert (
        ds.GetRasterBand(1).GetOverview(0).GetColorInterpretation()
        == gdal.GCI_GreenBand
    )

    for i in range(1, numbands):
        pix = struct.unpack(
            "f", ds.GetRasterBand(i).GetOverview(1).ReadRaster(0, 0, 1, 1)
        )[0]
        assert abs(pix - 0.3) < 0.01, "Error in band " + str(i)

    pix = struct.unpack(
        "f", ds.GetRasterBand(numbands).GetOverview(1).ReadRaster(0, 0, 1, 1)
    )[0]
    assert pix == 255, "Error in alpha band "
    ds = None

    gdal.GetDriverByName("GTiff").Delete(temp_path)


###############################################################################


@pytest.mark.parametrize("external_ovr_and_msk", [False, True])
def test_tiff_ovr_clean_with_mask(external_ovr_and_msk):
    """Test fix for https://github.com/OSGeo/gdal/issues/1047"""

    filename = "/vsimem/test_tiff_ovr_clean_with_mask.tif"
    ds = gdal.GetDriverByName("GTiff").Create(filename, 10, 10)
    with gdaltest.config_option(
        "GDAL_TIFF_INTERNAL_MASK", "NO" if external_ovr_and_msk else "YES"
    ):
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    if external_ovr_and_msk:
        ds = None
        ds = gdal.Open(filename)
    ds.BuildOverviews("NEAR", [2])
    ds = None

    # Clear overviews
    ds = gdal.Open(filename, gdal.GA_Update)
    ds.BuildOverviews(None, [])
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetRasterBand(1).GetMaskBand().GetOverviewCount() == 0
    ds = None

    assert gdal.VSIStatL(filename + ".ovr") is None
    assert gdal.VSIStatL(filename + ".msk.ovr") is None

    # Check after reopening
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetRasterBand(1).GetMaskBand().GetOverviewCount() == 0
    ds = None

    gdal.GetDriverByName("GTiff").Delete(filename)


###############################################################################


def test_tiff_ovr_external_mask_update(tmp_vsimem):

    filename = str(tmp_vsimem / "test_tiff_ovr_external_mask_update.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename, 10, 10)
    ds.GetRasterBand(1).Fill(127)
    with gdaltest.config_option("GDAL_TIFF_INTERNAL_MASK", "NO"):
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
        ds.GetRasterBand(1).GetMaskBand().Fill(255)
    ds = None

    assert gdal.VSIStatL(filename + ".msk") is not None

    ds = gdal.Open(filename, gdal.GA_Update)
    ds.BuildOverviews("NEAR", [2])
    assert ds.GetRasterBand(1).GetOverview(0).ComputeRasterMinMax(0) == (127, 127)
    assert ds.GetRasterBand(1).GetMaskBand().GetOverview(0).ComputeRasterMinMax(0) == (
        255,
        255,
    )
    assert ds.GetRasterBand(1).GetOverview(0).GetMaskBand().ComputeRasterMinMax(0) == (
        255,
        255,
    )
    ds = None

    assert gdal.VSIStatL(filename + ".ovr") is None
    assert gdal.VSIStatL(filename + ".msk.ovr") is None


###############################################################################
# Test BuildOverviews(NEAR) on a tiled interleave=band raster that is large compared to
# the allowed chunk size. This will fallbacks to the tiled based approach instead
# of the default scanlines based one


def test_tiff_ovr_fallback_to_multiband_overview_generate():

    filename = "/vsimem/test_tiff_ovr_issue_4932_src.tif"
    ds = gdal.Translate(
        filename,
        "data/byte.tif",
        options="-b 1 -b 1 -b 1 -co INTERLEAVE=BAND -co TILED=YES -outsize 1024 1024",
    )
    with gdaltest.config_option("GDAL_OVR_CHUNK_MAX_SIZE", "1000"):
        ds.BuildOverviews("NEAR", overviewlist=[2, 4, 8])
    ds = None

    ds = gdal.Open(filename)
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 37308
    ds = None

    gdal.GetDriverByName("GTiff").Delete(filename)


###############################################################################


def test_tiff_ovr_int64():

    temp_path = "/vsimem/test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(temp_path, 2, 1, 1, gdal.GDT_Int64)
    ds.GetRasterBand(1).WriteRaster(
        0, 0, 2, 1, struct.pack("q" * 2, -10000000000, -10000000000)
    )
    del ds
    ds = gdal.OpenEx(temp_path, gdal.GA_ReadOnly)
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Int64
    assert ds.BuildOverviews("nearest", overviewlist=[2]) == 0
    del ds
    ds = gdal.OpenEx(temp_path, gdal.GA_ReadOnly)
    assert struct.unpack("q", ds.GetRasterBand(1).GetOverview(0).ReadRaster()) == (
        -10000000000,
    )
    del ds
    gdal.GetDriverByName("GTiff").Delete(temp_path)


###############################################################################


def test_tiff_ovr_uint64():

    temp_path = "/vsimem/test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(temp_path, 2, 1, 1, gdal.GDT_UInt64)
    ds.GetRasterBand(1).WriteRaster(
        0, 0, 2, 1, struct.pack("Q" * 2, 10000000000, 10000000000)
    )
    del ds
    ds = gdal.OpenEx(temp_path, gdal.GA_ReadOnly)
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt64
    assert ds.BuildOverviews("nearest", overviewlist=[2]) == 0
    del ds
    ds = gdal.OpenEx(temp_path, gdal.GA_ReadOnly)
    assert struct.unpack("Q", ds.GetRasterBand(1).GetOverview(0).ReadRaster()) == (
        10000000000,
    )
    del ds
    gdal.GetDriverByName("GTiff").Delete(temp_path)


###############################################################################


def test_tiff_ovr_internal_overview_different_method():

    temp_path = "/vsimem/test.tif"
    gdal.GetDriverByName("GTiff").Create(
        temp_path, 2, 1, 1, gdal.GDT_Byte, options=["COMPRESS=LZW"]
    )
    ds = gdal.OpenEx(temp_path, gdal.GA_Update)
    with gdaltest.config_options(
        {"COMPRESS_OVERVIEW": "DEFLATE", "PREDICTOR_OVERVIEW": "2"}
    ):
        assert ds.BuildOverviews("nearest", overviewlist=[2]) == 0
    del ds
    ds = gdal.Open(temp_path)
    assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"
    ovr_ds = ds.GetRasterBand(1).GetOverview(0).GetDataset()
    assert ovr_ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "DEFLATE"
    assert ovr_ds.GetMetadataItem("PREDICTOR", "IMAGE_STRUCTURE") == "2"
    del ds
    gdal.GetDriverByName("GTiff").Delete(temp_path)


###############################################################################


def test_tiff_ovr_internal_overview_different_method_propagate_predictor():

    temp_path = "/vsimem/test.tif"
    gdal.GetDriverByName("GTiff").Create(
        temp_path, 2, 1, 1, gdal.GDT_Byte, options=["COMPRESS=LZW", "PREDICTOR=2"]
    )
    ds = gdal.OpenEx(temp_path, gdal.GA_Update)
    with gdaltest.config_options({"COMPRESS_OVERVIEW": "DEFLATE"}):
        assert ds.BuildOverviews("nearest", overviewlist=[2]) == 0
    del ds
    ds = gdal.Open(temp_path)
    assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"
    ovr_ds = ds.GetRasterBand(1).GetOverview(0).GetDataset()
    assert ovr_ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "DEFLATE"
    assert ovr_ds.GetMetadataItem("PREDICTOR", "IMAGE_STRUCTURE") == "2"
    del ds
    gdal.GetDriverByName("GTiff").Delete(temp_path)


###############################################################################


def test_tiff_ovr_internal_overview_different_method_do_not_propagate_predictor():

    temp_path = "/vsimem/test.tif"
    gdal.GetDriverByName("GTiff").Create(
        temp_path, 2, 1, 1, gdal.GDT_Byte, options=["COMPRESS=LZW", "PREDICTOR=2"]
    )
    ds = gdal.OpenEx(temp_path, gdal.GA_Update)
    with gdaltest.config_options({"COMPRESS_OVERVIEW": "PACKBITS"}):
        assert ds.BuildOverviews("nearest", overviewlist=[2]) == 0
    del ds
    ds = gdal.Open(temp_path)
    assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "LZW"
    ovr_ds = ds.GetRasterBand(1).GetOverview(0).GetDataset()
    assert ovr_ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "PACKBITS"
    assert ovr_ds.GetMetadataItem("PREDICTOR", "IMAGE_STRUCTURE") is None
    del ds
    gdal.GetDriverByName("GTiff").Delete(temp_path)


###############################################################################


def test_tiff_ovr_internal_overview_different_planar_config_to_pixel():

    temp_path = "/vsimem/test.tif"
    gdal.GetDriverByName("GTiff").Create(
        temp_path, 2, 1, 3, gdal.GDT_Byte, options=["INTERLEAVE=BAND"]
    )
    ds = gdal.OpenEx(temp_path, gdal.GA_Update)
    with gdaltest.config_options({"INTERLEAVE_OVERVIEW": "PIXEL"}):
        assert ds.BuildOverviews("nearest", overviewlist=[2]) == 0
    del ds
    ds = gdal.Open(temp_path)
    assert ds.GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE") == "BAND"
    ovr_ds = ds.GetRasterBand(1).GetOverview(0).GetDataset()
    assert ovr_ds.GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE") == "PIXEL"
    del ds
    gdal.GetDriverByName("GTiff").Delete(temp_path)


###############################################################################


def test_tiff_ovr_internal_overview_different_planar_config_to_band():

    temp_path = "/vsimem/test.tif"
    gdal.GetDriverByName("GTiff").Create(
        temp_path, 2, 1, 3, gdal.GDT_Byte, options=["INTERLEAVE=PIXEL"]
    )
    ds = gdal.OpenEx(temp_path, gdal.GA_Update)
    with gdaltest.config_options({"INTERLEAVE_OVERVIEW": "BAND"}):
        assert ds.BuildOverviews("nearest", overviewlist=[2]) == 0
    del ds
    ds = gdal.Open(temp_path)
    assert ds.GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE") == "PIXEL"
    ovr_ds = ds.GetRasterBand(1).GetOverview(0).GetDataset()
    assert ovr_ds.GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE") == "BAND"
    del ds
    gdal.GetDriverByName("GTiff").Delete(temp_path)


###############################################################################


def test_tiff_ovr_external_1_px_wide_3_px_tall():

    temp_path = "/vsimem/test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(temp_path, 1, 3)
    ds.GetRasterBand(1).Fill(1)
    ds = None
    ds = gdal.OpenEx(temp_path)
    with gdaltest.config_options({"COMPRESS_OVERVIEW": "LZW"}):
        assert ds.BuildOverviews("nearest", overviewlist=[2]) == 0
    del ds
    ds = gdal.Open(temp_path + ".ovr")
    assert ds.GetRasterBand(1).Checksum() == 2
    with gdaltest.config_options({"COMPRESS_OVERVIEW": "LZW"}):
        assert ds.BuildOverviews("nearest", overviewlist=[2]) == 0
    del ds
    ds = gdal.Open(temp_path + ".ovr.ovr")
    assert ds.GetRasterBand(1).Checksum() == 1
    del ds
    gdal.GetDriverByName("GTiff").Delete(temp_path)


###############################################################################


def test_tiff_ovr_external_3_px_wide_1_px_tall():

    temp_path = "/vsimem/test.tif"
    ds = gdal.GetDriverByName("GTiff").Create(temp_path, 3, 1)
    ds.GetRasterBand(1).Fill(1)
    ds = None
    ds = gdal.OpenEx(temp_path)
    with gdaltest.config_options({"COMPRESS_OVERVIEW": "LZW"}):
        assert ds.BuildOverviews("nearest", overviewlist=[2]) == 0
    del ds
    ds = gdal.Open(temp_path + ".ovr")
    assert ds.GetRasterBand(1).Checksum() == 2
    with gdaltest.config_options({"COMPRESS_OVERVIEW": "LZW"}):
        assert ds.BuildOverviews("nearest", overviewlist=[2]) == 0
    del ds
    ds = gdal.Open(temp_path + ".ovr.ovr")
    assert ds.GetRasterBand(1).Checksum() == 1
    del ds
    gdal.GetDriverByName("GTiff").Delete(temp_path)


###############################################################################


@pytest.mark.require_creation_option("GTiff", "JXL")
def test_tiff_ovr_JXL_LOSSLESS_OVERVIEW(tmp_vsimem):

    tmpfilename = str(tmp_vsimem / "test_tiff_ovr_JXL_LOSSLESS.tif")
    gdal.Translate(tmpfilename, "data/byte.tif")
    ds = gdal.Open(tmpfilename)
    with gdaltest.config_options(
        {"COMPRESS_OVERVIEW": "JXL", "JXL_LOSSLESS_OVERVIEW": "NO"}
    ):
        ds.BuildOverviews("nearest", [2])
    del ds
    ds = gdal.Open(tmpfilename + ".ovr")
    assert ds.GetRasterBand(1).Checksum() not in (0, 1087)


###############################################################################


@pytest.mark.require_creation_option("GTiff", "JXL")
def test_tiff_ovr_JXL_DISTANCE_OVERVIEW(tmp_vsimem):

    tmpfilename = str(tmp_vsimem / "test_tiff_ovr_JXL_DISTANCE_OVERVIEW.tif")
    gdal.Translate(tmpfilename, "data/byte.tif")
    ds = gdal.Open(tmpfilename)
    with gdaltest.config_options(
        {"COMPRESS_OVERVIEW": "JXL", "JXL_LOSSLESS_OVERVIEW": "NO"}
    ):
        ds.BuildOverviews("nearest", [2])
    del ds
    ds = gdal.Open(tmpfilename + ".ovr")
    cs1 = ds.GetRasterBand(1).Checksum()
    del ds
    gdal.Unlink(tmpfilename + ".ovr")

    ds = gdal.Open(tmpfilename)
    with gdaltest.config_options(
        {
            "COMPRESS_OVERVIEW": "JXL",
            "JXL_LOSSLESS_OVERVIEW": "NO",
            "JXL_DISTANCE_OVERVIEW": "5",
        }
    ):
        ds.BuildOverviews("nearest", [2])
    del ds
    ds = gdal.Open(tmpfilename + ".ovr")
    assert ds.GetRasterBand(1).Checksum() != cs1
    del ds
    gdal.Unlink(tmpfilename + ".ovr")


###############################################################################


@pytest.mark.require_creation_option("GTiff", "JXL")
@pytest.mark.require_creation_option("GTiff", "JXL_ALPHA_DISTANCE")
def test_tiff_ovr_JXL_ALPHA_DISTANCE_OVERVIEW(tmp_vsimem):

    tmpfilename = str(tmp_vsimem / "test_tiff_ovr_JXL_ALPHA_DISTANCE_OVERVIEW.tif")
    gdal.Translate(tmpfilename, "data/stefan_full_rgba.tif")
    ds = gdal.Open(tmpfilename)
    with gdaltest.config_options(
        {"COMPRESS_OVERVIEW": "JXL", "JXL_LOSSLESS_OVERVIEW": "NO"}
    ):
        ds.BuildOverviews("nearest", [2])
    del ds
    ds = gdal.Open(tmpfilename + ".ovr")
    cs1 = ds.GetRasterBand(1).Checksum()
    cs4 = ds.GetRasterBand(4).Checksum()
    del ds
    gdal.Unlink(tmpfilename + ".ovr")

    ds = gdal.Open(tmpfilename)
    with gdaltest.config_options(
        {
            "COMPRESS_OVERVIEW": "JXL",
            "JXL_LOSSLESS_OVERVIEW": "NO",
            "JXL_ALPHA_DISTANCE_OVERVIEW": "5",
        }
    ):
        ds.BuildOverviews("nearest", [2])
    del ds
    ds = gdal.Open(tmpfilename + ".ovr")
    assert ds.GetRasterBand(1).Checksum() == cs1
    assert ds.GetRasterBand(4).Checksum() != cs4
    del ds
    gdal.Unlink(tmpfilename + ".ovr")
