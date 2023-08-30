#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test BASISU driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Even Rouault <even.rouault@spatialys.com>
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

pytestmark = pytest.mark.require_driver("BASISU")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


def test_basisu_read_etc1s():
    ds = gdal.Open("data/basisu/byte_etc1s.basis")
    assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "ETC1S"
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 3
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)] == [
        4916
    ] * 3
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetRasterBand(1).GetOverview(0) is None


def test_basisu_read_uastc():
    ds = gdal.Open("data/basisu/byte_uastc.basis")
    assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "UASTC"
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 3
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(ds.RasterCount)] == [
        4775
    ] * 3
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    assert ds.GetRasterBand(1).GetOverview(0) is None


def test_basisu_read_two_images():
    # File created with "./basisu -tex_type video -multifile_printf "file%d.png" -multifile_first 1 -multifile_num 2"
    # where file1.png is ../gcore/data/stefan_full_rgba.tif
    # and file2.png the output of ´gdal_translate file1.png file2.png -scale_1 0 255 255 0 -scale_2 0 255 255 0 -scale_3 0 255 255 0
    ds = gdal.Open("data/basisu/two_images.basis")
    assert ds.RasterXSize == 0
    assert ds.RasterYSize == 0
    assert ds.RasterCount == 0
    subds_list = ds.GetSubDatasets()
    assert len(subds_list) == 2
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    subds = gdal.Open(subds_list[0][0])
    assert subds
    assert subds.RasterXSize == src_ds.RasterXSize
    assert subds.RasterYSize == src_ds.RasterYSize
    assert subds.RasterCount == src_ds.RasterCount
    subds2 = gdal.Open(subds_list[1][0])
    assert subds2
    assert subds2.RasterXSize == src_ds.RasterXSize
    assert subds2.RasterYSize == src_ds.RasterYSize
    assert subds2.RasterCount == src_ds.RasterCount
    assert [
        subds2.GetRasterBand(i + 1).Checksum() for i in range(subds.RasterCount)
    ] != [subds.GetRasterBand(i + 1).Checksum() for i in range(subds.RasterCount)]


@pytest.mark.parametrize(
    "filename",
    [
        "BASISU:",
        "BASISU:data/basisu/two_images.basis",
        "BASISU:data/basisu/i_do_not_exist.basis:0",
        "BASISU:data/basisu/two_images.basis:2",
    ],
)
def test_basisu_read_wrong_subds(filename):
    with gdal.quiet_errors():
        assert gdal.Open(filename) is None


def test_basisu_write_rgba_output_on_filesystem():
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_filename = "tmp/out.basis"
    assert gdal.GetDriverByName("BASISU").CreateCopy(out_filename, src_ds) is not None
    out_ds = gdal.Open(out_filename)
    assert out_ds.RasterXSize == src_ds.RasterXSize
    assert out_ds.RasterYSize == src_ds.RasterYSize
    assert out_ds.RasterCount == src_ds.RasterCount
    got_cs = [out_ds.GetRasterBand(i + 1).Checksum() for i in range(out_ds.RasterCount)]
    assert got_cs in (
        [7694, 58409, 37321, 8494],  # Linux
        [7913, 58488, 37737, 8324],
    )  # Windows
    assert out_ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == "ETC1S"
    gdal.Unlink(out_filename)


@pytest.mark.parametrize("compression", ["ETC1S", "UASTC"])
def test_basisu_write_compression(compression):
    gdal.ErrorReset()
    src_ds = gdal.Open("data/byte.tif")
    out_filename = "/vsimem/out.basis"
    gdal.GetDriverByName("BASISU").CreateCopy(
        out_filename, src_ds, options=["COMPRESSION=" + compression]
    )
    gdal.Unlink(out_filename + ".aux.xml")
    ds = gdal.Open(out_filename)
    assert ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE") == compression
    gdal.Unlink(out_filename)


def test_basisu_write_mipmap():
    gdal.ErrorReset()
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_filename = "/vsimem/out.basis"
    out_ds = gdal.GetDriverByName("BASISU").CreateCopy(
        out_filename, src_ds, options=["MIPMAP=YES"]
    )
    assert gdal.GetLastErrorMsg() == ""
    assert out_ds.GetRasterBand(1).GetOverviewCount() == 7
    ovr_ds = out_ds.GetRasterBand(1).GetOverview(0).GetDataset()
    assert ovr_ds.RasterXSize == 81
    assert ovr_ds.RasterYSize == 75
    got_cs = [ovr_ds.GetRasterBand(i + 1).Checksum() for i in range(ovr_ds.RasterCount)]
    assert got_cs in (
        [19694, 16863, 11239, 35973],  # Linux
        [19968, 16919, 11262, 36022],
    )  # Windows
    gdal.Unlink(out_filename)


def test_basisu_write_uastc_level():
    gdal.ErrorReset()
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_filename = "/vsimem/out.basis"
    assert (
        gdal.GetDriverByName("BASISU").CreateCopy(
            out_filename, src_ds, options=["COMPRESSION=UASTC", "UASTC_LEVEL=0"]
        )
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""
    # size = gdal.VSIStatL(out_filename).size
    assert (
        gdal.GetDriverByName("BASISU").CreateCopy(
            out_filename, src_ds, options=["COMPRESSION=UASTC", "UASTC_LEVEL=2"]
        )
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""
    # new_size = gdal.VSIStatL(out_filename).size
    # assert new_size != size  #  would only be true after LZ compression
    gdal.Unlink(out_filename)


def test_basisu_write_uastc_rdo_level():
    gdal.ErrorReset()
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_filename = "/vsimem/out.basis"
    assert (
        gdal.GetDriverByName("BASISU").CreateCopy(
            out_filename, src_ds, options=["COMPRESSION=UASTC", "UASTC_RDO_LEVEL=0.3"]
        )
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""
    # size = gdal.VSIStatL(out_filename).size
    assert (
        gdal.GetDriverByName("BASISU").CreateCopy(
            out_filename, src_ds, options=["COMPRESSION=UASTC", "UASTC_RDO_LEVEL=3"]
        )
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""
    # new_size = gdal.VSIStatL(out_filename).size
    # assert new_size < size  #  would only be true after LZ compression
    gdal.Unlink(out_filename)


def test_basisu_write_etc1s_level():
    gdal.ErrorReset()
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_filename = "/vsimem/out.basis"
    assert (
        gdal.GetDriverByName("BASISU").CreateCopy(
            out_filename, src_ds, options=["ETC1S_LEVEL=0"]
        )
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""
    size = gdal.VSIStatL(out_filename).size
    assert (
        gdal.GetDriverByName("BASISU").CreateCopy(
            out_filename, src_ds, options=["ETC1S_LEVEL=3"]
        )
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""
    new_size = gdal.VSIStatL(out_filename).size
    assert new_size != size
    gdal.Unlink(out_filename)


def test_basisu_write_etc1s_quality_level():
    gdal.ErrorReset()
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_filename = "/vsimem/out.basis"
    assert (
        gdal.GetDriverByName("BASISU").CreateCopy(
            out_filename, src_ds, options=["ETC1S_QUALITY_LEVEL=1"]
        )
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""
    size = gdal.VSIStatL(out_filename).size
    assert (
        gdal.GetDriverByName("BASISU").CreateCopy(
            out_filename, src_ds, options=["ETC1S_QUALITY_LEVEL=255"]
        )
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""
    new_size = gdal.VSIStatL(out_filename).size
    assert new_size > size
    gdal.Unlink(out_filename)


def test_basisu_write_etc1s_clusters_options():
    gdal.ErrorReset()
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_filename = "/vsimem/out.basis"
    assert (
        gdal.GetDriverByName("BASISU").CreateCopy(out_filename, src_ds, options=[])
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""
    size = gdal.VSIStatL(out_filename).size
    assert (
        gdal.GetDriverByName("BASISU").CreateCopy(
            out_filename,
            src_ds,
            options=[
                "ETC1S_MAX_ENDPOINTS_CLUSTERS=16128",
                "ETC1S_MAX_SELECTOR_CLUSTERS=16128",
            ],
        )
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""
    new_size = gdal.VSIStatL(out_filename).size
    assert new_size > size
    gdal.Unlink(out_filename)

    gdal.ErrorReset()
    with gdal.quiet_errors():
        assert (
            gdal.GetDriverByName("BASISU").CreateCopy(
                out_filename,
                src_ds,
                options=[
                    "ETC1S_MAX_ENDPOINTS_CLUSTERS=16129",  # too big
                    "ETC1S_MAX_SELECTOR_CLUSTERS=16128",
                ],
            )
            is None
        )


def test_basisu_write_colorspace():
    gdal.ErrorReset()
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_filename = "/vsimem/out.basis"
    assert (
        gdal.GetDriverByName("BASISU").CreateCopy(
            out_filename, src_ds, options=["COLORSPACE=PERCEPTUAL_SRGB"]
        )
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""
    size = gdal.VSIStatL(out_filename).size
    assert (
        gdal.GetDriverByName("BASISU").CreateCopy(
            out_filename, src_ds, options=["COLORSPACE=LINEAR"]
        )
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""
    new_size = gdal.VSIStatL(out_filename).size
    assert new_size != size
    gdal.Unlink(out_filename)


def test_basisu_write_num_threads():
    gdal.ErrorReset()
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_filename = "/vsimem/out.basis"
    # Just check that it works
    assert (
        gdal.GetDriverByName("BASISU").CreateCopy(
            out_filename, src_ds, options=["NUM_THREADS=1"]
        )
        is not None
    )
    assert gdal.GetLastErrorMsg() == ""
    gdal.Unlink(out_filename)


def test_basisu_write_etc1s_incompatible_or_missing_options():
    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_filename = "/vsimem/out.basis"

    gdal.ErrorReset()
    with gdal.quiet_errors():
        assert (
            gdal.GetDriverByName("BASISU").CreateCopy(
                out_filename, src_ds, options=["ETC1S_MAX_ENDPOINTS_CLUSTERS=16128"]
            )
            is None
        )
        assert gdal.GetLastErrorMsg() != ""

    gdal.ErrorReset()
    with gdal.quiet_errors():
        assert (
            gdal.GetDriverByName("BASISU").CreateCopy(
                out_filename, src_ds, options=["ETC1S_MAX_SELECTOR_CLUSTERS=16128"]
            )
            is None
        )
        assert gdal.GetLastErrorMsg() != ""

    gdal.ErrorReset()
    with gdal.quiet_errors():
        assert (
            gdal.GetDriverByName("BASISU").CreateCopy(
                out_filename,
                src_ds,
                options=["ETC1S_QUALITY_LEVEL=1", "ETC1S_MAX_ENDPOINTS_CLUSTERS=16128"],
            )
            is not None
        )
        assert gdal.GetLastErrorType() == 2  # warning
    gdal.Unlink(out_filename)

    gdal.ErrorReset()
    with gdal.quiet_errors():
        assert (
            gdal.GetDriverByName("BASISU").CreateCopy(
                out_filename,
                src_ds,
                options=["ETC1S_QUALITY_LEVEL=1", "ETC1S_MAX_SELECTOR_CLUSTERS=16128"],
            )
            is not None
        )
        assert gdal.GetLastErrorType() == 2  # warning
    gdal.Unlink(out_filename)


def test_basisu_write_incompatible_source():

    out_filename = "/vsimem/out.basis"

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 0)
    with gdal.quiet_errors():
        assert gdal.GetDriverByName("BASISU").CreateCopy(out_filename, src_ds) is None

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 5)
    with gdal.quiet_errors():
        assert gdal.GetDriverByName("BASISU").CreateCopy(out_filename, src_ds) is None

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_UInt16)
    with gdal.quiet_errors():
        assert gdal.GetDriverByName("BASISU").CreateCopy(out_filename, src_ds) is None
