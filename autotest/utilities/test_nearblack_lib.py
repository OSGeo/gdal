#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  nearblack testing
# Author:   Even Rouault <even dot rouault @ spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault @ spatialys dot com>
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
import collections
import pathlib

import pytest

from osgeo import gdal

###############################################################################
# Basic test


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_1(alg):

    src_ds = gdal.Open("../gdrivers/data/rgbsmall.tif")
    ds = gdal.Nearblack("", src_ds, format="MEM", maxNonBlack=0, nearDist=15, alg=alg)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 21106, "Bad checksum band 1"

    assert ds.GetRasterBand(2).Checksum() == 20736, "Bad checksum band 2"

    assert ds.GetRasterBand(3).Checksum() == 21309, "Bad checksum band 3"

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        assert src_gt[i] == pytest.approx(dst_gt[i], abs=1e-10), "Bad geotransform"

    dst_wkt = ds.GetProjectionRef()
    assert dst_wkt.find('AUTHORITY["EPSG","4326"]') != -1, "Bad projection"

    src_ds = None
    ds = None


###############################################################################
# Add alpha band


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_2(alg):

    ds = gdal.Nearblack(
        "",
        "../gdrivers/data/rgbsmall.tif",
        format="MEM",
        maxNonBlack=0,
        setAlpha=True,
        alg=alg,
    )
    assert ds is not None

    assert ds.GetRasterBand(4).Checksum() == 22002, "Bad checksum band 0"

    ds = None


###############################################################################
# Set existing alpha band


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_3(alg):

    src_ds = gdal.Nearblack(
        "", "../gdrivers/data/rgbsmall.tif", format="MEM", maxNonBlack=0, setAlpha=True
    )
    ds = gdal.Nearblack("", src_ds, format="MEM", maxNonBlack=0, setAlpha=True, alg=alg)
    assert ds is not None

    assert ds.GetRasterBand(4).Checksum() == 22002, "Bad checksum band 0"

    ds = None


###############################################################################
# Test -white


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_4(alg):

    src_ds = gdal.Warp(
        "",
        "../gdrivers/data/rgbsmall.tif",
        format="MEM",
        warpOptions=["INIT_DEST=255"],
        srcNodata=0,
    )
    ds = gdal.Nearblack(
        "", src_ds, format="MEM", white=True, maxNonBlack=0, setAlpha=True, alg=alg
    )
    assert ds is not None

    expected_cs = 24151 if alg == "twopasses" else 24024
    assert ds.GetRasterBand(4).Checksum() == expected_cs, "Bad checksum band 0"

    ds = None


###############################################################################
# Add mask band


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_5(tmp_vsimem, alg):

    ds = gdal.Nearblack(
        tmp_vsimem / "test_nearblack_lib_5.tif",
        pathlib.Path("../gdrivers/data/rgbsmall.tif"),
        format="GTiff",
        maxNonBlack=0,
        setMask=True,
        alg=alg,
    )
    assert ds is not None

    assert (
        ds.GetRasterBand(1).GetMaskBand().Checksum() == 22002
    ), "Bad checksum mask band"

    ds = None


###############################################################################
# Test -color


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_7(alg):

    ds = gdal.Nearblack(
        "",
        "data/whiteblackred.tif",
        format="MEM",
        colors=((0, 0, 0), (255, 255, 255)),
        alg=alg,
        maxNonBlack=0,
    )
    assert ds is not None

    assert (
        ds.GetRasterBand(1).Checksum() == 1217
        and ds.GetRasterBand(2).Checksum() == 0
        and ds.GetRasterBand(3).Checksum() == 0
    ), "Bad checksum"

    ds = None


###############################################################################
# Test in-place update


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_8(alg):

    src_ds = gdal.Open("../gdrivers/data/rgbsmall.tif")
    ds = gdal.GetDriverByName("MEM").CreateCopy("", src_ds)
    ret = gdal.Nearblack(ds, ds, maxNonBlack=0, alg=alg)
    assert ret == 1

    assert ds.GetRasterBand(1).Checksum() == 21106, "Bad checksum band 1"

    assert ds.GetRasterBand(2).Checksum() == 20736, "Bad checksum band 2"

    assert ds.GetRasterBand(3).Checksum() == 21309, "Bad checksum band 3"


def _test_nearblack(in_array, expected_mask_array, maxNonBlack=0, alg=None):

    ds = gdal.GetDriverByName("MEM").Create("", len(in_array[0]), len(in_array))
    ds.WriteRaster(
        0,
        0,
        ds.RasterXSize,
        ds.RasterYSize,
        b"".join([array.array("B", x).tobytes() for x in in_array]),
    )
    ret_ds = gdal.Nearblack(
        "", ds, maxNonBlack=maxNonBlack, format="MEM", setMask=True, alg=alg
    )
    mask_data = ret_ds.GetRasterBand(1).GetMaskBand().ReadRaster()
    mask_array = []
    for j in range(ds.RasterYSize):
        ar = array.array("B")
        ar.frombytes(mask_data[j * ds.RasterXSize : (j + 1) * ds.RasterXSize])
        mask_array.append(ar.tolist())
    assert mask_array == expected_mask_array


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_all_valid(alg):

    # all valid -> no erosion
    _test_nearblack(
        [
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
        ],
        [
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
        ],
        maxNonBlack=1,
        alg=alg,
    )


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_all_invalid(alg):

    # all invalid
    _test_nearblack(
        [
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
        ],
        [
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
        ],
        maxNonBlack=1,
        alg=alg,
    )


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_single_pixel_valid(alg):

    # single pixel valid -> eroded
    _test_nearblack(
        [
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
            [0, 0, 255, 0, 0],
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
        ],
        [
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
        ],
        maxNonBlack=1,
        alg=alg,
    )


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
@pytest.mark.parametrize("maxNonBlack", [0, 1, 5])
def test_nearblack_lib_all_contour_valid(alg, maxNonBlack):

    # all contour is valid -> no erosion
    _test_nearblack(
        [
            [255, 255, 255, 255, 255],
            [255, 0, 0, 0, 255],
            [255, 0, 0, 0, 255],
            [255, 0, 0, 0, 255],
            [255, 255, 255, 255, 255],
        ],
        [
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
        ],
        maxNonBlack=maxNonBlack,
        alg=alg,
    )


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_erosion_from_left(alg):

    # erosion from the left
    _test_nearblack(
        [
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [0, 0, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
        ],
        [
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [0, 0, 0, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
        ],
        maxNonBlack=1,
        alg=alg,
    )


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_erosion_from_right(alg):
    # erosion from the right
    _test_nearblack(
        [
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 0, 0],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
        ],
        [
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 0, 0, 0],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
        ],
        maxNonBlack=1,
        alg=alg,
    )


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_erosion_from_top(alg):

    # erosion from the top
    _test_nearblack(
        [
            [255, 0, 0, 0, 255],
            [255, 255, 0, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
        ],
        [
            [255, 0, 0, 0, 255],
            [255, 0, 0, 0, 255],
            [255, 255, 0, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
        ],
        maxNonBlack=1,
        alg=alg,
    )


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_erosion_from_bottom(alg):

    # erosion from the bottom
    _test_nearblack(
        [
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 0, 255, 255],
            [255, 0, 0, 0, 255],
        ],
        [
            [255, 255, 255, 255, 255],
            [255, 255, 255, 255, 255],
            [255, 255, 0, 255, 255],
            [255, 0, 0, 0, 255],
            [255, 0, 0, 0, 255],
        ],
        maxNonBlack=1,
        alg=alg,
    )


@pytest.mark.parametrize("alg", ["twopasses", "floodfill"])
def test_nearblack_lib_erosion_from_top_and_bottom(alg):

    # Maybe erosion is a bit too greedy due to top-bottom + bottom-top passes
    _test_nearblack(
        [
            [0, 0, 0, 0, 0, 0, 0],
            [0, 0, 255, 255, 255, 0, 0],
            [0, 0, 255, 255, 255, 0, 0],
            [0, 255, 255, 255, 255, 255, 0],
            [0, 0, 255, 255, 255, 0, 0],
            [0, 0, 255, 255, 255, 0, 0],
            [0, 0, 0, 0, 0, 0, 0],
        ],
        [
            [0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 255, 0, 0, 0],
            [0, 0, 0, 255, 0, 0, 0],
            [0, 0, 0, 255, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0],
        ],
        maxNonBlack=1,
        alg=alg,
    )

    # Maybe erosion is a bit too greedy due to top-bottom + bottom-top passes
    _test_nearblack(
        [
            [0, 0, 0, 0, 255],
            [0, 255, 255, 0, 0],
            [255, 255, 255, 255, 255],
            [255, 0, 255, 255, 0],
            [0, 0, 0, 255, 0],
        ],
        [
            [0, 0, 0, 0, 255],
            [0, 0, 0, 0, 0],
            [0, 0, 255, 0, 0],
            [0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0],
        ],
        maxNonBlack=1,
        alg=alg,
    )


def test_nearblack_lib_floodfill_concave_from_left():

    XXX = 0
    input_ar = [
        [255, 255, 255, 255, 255],
        [255, XXX, XXX, XXX, 255],
        [XXX, XXX, 255, XXX, 255],
        [255, 255, XXX, XXX, 255],
        [255, 255, 255, 255, 255],
    ]
    _test_nearblack(
        input_ar,
        input_ar,
        maxNonBlack=0,
        alg="floodfill",
    )


def test_nearblack_lib_floodfill_concave_from_right():

    XXX = 0
    input_ar = [
        [255, 255, 255, 255, 255],
        [255, XXX, XXX, XXX, 255],
        [255, XXX, 255, XXX, XXX],
        [255, XXX, XXX, 255, 255],
        [255, 255, 255, 255, 255],
    ]
    _test_nearblack(
        input_ar,
        input_ar,
        maxNonBlack=0,
        alg="floodfill",
    )


def test_nearblack_lib_floodfill_concave_from_top():

    XXX = 0
    input_ar = [
        [255, XXX, 255, 255, 255],
        [255, XXX, XXX, XXX, 255],
        [255, 255, 255, XXX, 255],
        [255, 255, XXX, XXX, 255],
        [255, 255, 255, 255, 255],
    ]
    _test_nearblack(
        input_ar,
        input_ar,
        maxNonBlack=0,
        alg="floodfill",
    )


def test_nearblack_lib_floodfill_concave_from_bottom():

    XXX = 0
    input_ar = [
        [255, 255, 255, 255, 255],
        [255, 255, XXX, XXX, 255],
        [255, 255, 255, XXX, 255],
        [255, XXX, XXX, XXX, 255],
        [255, XXX, 255, 255, 255],
    ]
    _test_nearblack(
        input_ar,
        input_ar,
        maxNonBlack=0,
        alg="floodfill",
    )


def test_nearblack_lib_floodfill_concave_from_bottom_non_black():

    XXX = 0
    input_ar = [
        [255, 255, 255, 255, 255],
        [255, XXX, XXX, XXX, 255],
        [255, 255, 255, 255, 255],
        [255, XXX, 255, 255, 255],
        [255, XXX, 255, 255, 255],
    ]
    output_ar = [
        [255, 255, 255, 255, 255],
        [255, XXX, XXX, XXX, 255],
        [255, XXX, 255, 255, 255],
        [255, XXX, 255, 255, 255],
        [255, XXX, 255, 255, 255],
    ]
    _test_nearblack(
        input_ar,
        output_ar,
        maxNonBlack=1,
        alg="floodfill",
    )


def test_nearblack_lib_dict_arguments():

    opt = gdal.NearblackOptions(
        "__RETURN_OPTION_LIST__",
        creationOptions=collections.OrderedDict(
            (("COMPRESS", "DEFLATE"), ("LEVEL", 4))
        ),
    )

    ind = opt.index("-co")

    assert opt[ind : ind + 4] == ["-co", "COMPRESS=DEFLATE", "-co", "LEVEL=4"]
