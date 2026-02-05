#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster blend' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import struct

import gdaltest
import pytest

from osgeo import gdal


# Check that (R,G,B) -> (H,S,V) -> (R,G,B) correctly roundtrips
def test_gdalalg_raster_blend_check_rgb_hsb_conversion():

    # N = 256 does full color space checking but is a bit slow for CI
    N = 64
    assert N % 2 == 0

    Nvalues = N**3
    rgba_ds = gdal.GetDriverByName("MEM").Create("", Nvalues // 2, 2, 4)
    rgba_ds.SetProjection("WGS84")
    rgba_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    rgba_ds.GetRasterBand(1).WriteRaster(
        0,
        0,
        Nvalues // 2,
        2,
        array.array(
            "B",
            [
                255 if r == N - 1 else r * (256 // N)
                for r in range(N)
                for g in range(N)
                for b in range(N)
            ],
        ),
    )
    rgba_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    rgba_ds.GetRasterBand(2).WriteRaster(
        0,
        0,
        Nvalues // 2,
        2,
        array.array(
            "B",
            [
                255 if g == N - 1 else g * (256 // N)
                for r in range(N)
                for g in range(N)
                for b in range(N)
            ],
        ),
    )
    rgba_ds.GetRasterBand(3).WriteRaster(
        0,
        0,
        Nvalues // 2,
        2,
        array.array(
            "B",
            [
                255 if b == N - 1 else b * (256 // N)
                for r in range(N)
                for g in range(N)
                for b in range(N)
            ],
        ),
    )
    rgba_ds.GetRasterBand(4).WriteRaster(
        0, 0, Nvalues // 2, 2, array.array("B", [i & 255 for i in range(Nvalues)])
    )
    rgba_ds.BuildOverviews("NEAR", [2])

    grayscale_ds = gdal.GetDriverByName("MEM").Create("", Nvalues // 2, 2)
    grayscale_ds.WriteRaster(
        0,
        0,
        Nvalues // 2,
        2,
        array.array(
            "B",
            [
                max(
                    255 if r == N - 1 else r * (256 // N),
                    255 if g == N - 1 else g * (256 // N),
                    255 if b == N - 1 else b * (256 // N),
                )
                for r in range(N)
                for g in range(N)
                for b in range(N)
            ],
        ),
    )
    grayscale_ds.BuildOverviews("NEAR", [2])

    with gdal.Run(
        "raster",
        "blend",
        input=rgba_ds,
        overlay=grayscale_ds,
        output_format="stream",
        operator="hsv-value",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == rgba_ds.RasterCount
        assert out_ds.ReadRaster() == rgba_ds.ReadRaster()
        assert (
            out_ds.GetRasterBand(1).GetColorInterpretation()
            == rgba_ds.GetRasterBand(1).GetColorInterpretation()
        )
        assert out_ds.GetSpatialRef().IsSame(rgba_ds.GetSpatialRef())
        assert out_ds.GetGeoTransform() == rgba_ds.GetGeoTransform()
        assert (
            out_ds.GetRasterBand(1).ReadRaster()
            == rgba_ds.GetRasterBand(1).ReadRaster()
        )
        assert (
            out_ds.GetRasterBand(2).ReadRaster()
            == rgba_ds.GetRasterBand(2).ReadRaster()
        )
        assert (
            out_ds.GetRasterBand(3).ReadRaster()
            == rgba_ds.GetRasterBand(3).ReadRaster()
        )
        assert (
            out_ds.GetRasterBand(4).ReadRaster()
            == rgba_ds.GetRasterBand(4).ReadRaster()
        )
        assert out_ds.GetRasterBand(1).GetOverviewCount() == 1
        assert out_ds.GetRasterBand(1).GetOverview(0)
        assert out_ds.GetRasterBand(1).GetOverview(-1) is None
        assert out_ds.GetRasterBand(1).GetOverview(1) is None
        subsampled = out_ds.GetRasterBand(1).ReadRaster(
            0, 0, Nvalues // 2, 2, buf_xsize=Nvalues // 4, buf_ysize=1
        )
        assert subsampled == rgba_ds.GetRasterBand(1).ReadRaster(
            0, 0, Nvalues // 2, 2, buf_xsize=Nvalues // 4, buf_ysize=1
        )
        assert subsampled == out_ds.GetRasterBand(1).GetOverview(0).ReadRaster()
        subsampled = out_ds.ReadRaster(
            0, 0, Nvalues // 2, 2, buf_xsize=Nvalues // 4, buf_ysize=1
        )
        assert subsampled == rgba_ds.ReadRaster(
            0, 0, Nvalues // 2, 2, buf_xsize=Nvalues // 4, buf_ysize=1
        )
        assert (
            subsampled
            == rgba_ds.GetRasterBand(1).GetOverview(0).GetDataset().ReadRaster()
        )

        assert out_ds.GetRasterBand(1).ReadRaster(
            1.5, 0.25, 2.5, 1, buf_xsize=1, buf_ysize=1
        ) == rgba_ds.GetRasterBand(1).ReadRaster(
            1.5, 0.25, 2.5, 1, buf_xsize=1, buf_ysize=1
        )

        assert out_ds.ReadRaster(
            1, 0, 3, 2, buf_xsize=5, buf_ysize=6, buf_type=gdal.GDT_UInt16
        ) == rgba_ds.ReadRaster(
            1, 0, 3, 2, buf_xsize=5, buf_ysize=6, buf_type=gdal.GDT_UInt16
        )
        assert out_ds.GetRasterBand(2).ReadRaster(
            1, 0, 3, 2, buf_xsize=5, buf_ysize=6, buf_type=gdal.GDT_UInt16
        ) == rgba_ds.GetRasterBand(2).ReadRaster(
            1, 0, 3, 2, buf_xsize=5, buf_ysize=6, buf_type=gdal.GDT_UInt16
        )
        assert out_ds.GetRasterBand(4).ReadBlock(0, 1) == rgba_ds.GetRasterBand(
            4
        ).ReadBlock(0, 1)

        assert (
            out_ds.ReadRaster(buf_pixel_space=2)[::2]
            == rgba_ds.ReadRaster(buf_pixel_space=2)[::2]
        )
        assert (
            out_ds.GetRasterBand(1).ReadRaster(buf_pixel_space=2)[::2]
            == rgba_ds.GetRasterBand(1).ReadRaster(buf_pixel_space=2)[::2]
        )
        assert (
            out_ds.GetRasterBand(2).ReadRaster(buf_pixel_space=2)[::2]
            == rgba_ds.GetRasterBand(2).ReadRaster(buf_pixel_space=2)[::2]
        )
        assert (
            out_ds.GetRasterBand(3).ReadRaster(buf_pixel_space=2)[::2]
            == rgba_ds.GetRasterBand(3).ReadRaster(buf_pixel_space=2)[::2]
        )

        for i in range(N):
            assert out_ds.ReadRaster(i * N, 0, 7, 1) == rgba_ds.ReadRaster(
                i * N, 0, 7, 1
            )
            assert out_ds.ReadRaster(i + N // 2 * N, 0, 7, 1) == rgba_ds.ReadRaster(
                i + N // 2 * N, 0, 7, 1
            )
            assert out_ds.ReadRaster(
                i + N // 2 * N * (N - 1) - 8, 0, 7, 1
            ) == rgba_ds.ReadRaster(i + N // 2 * N * (N - 1) - 8, 0, 7, 1)

    grayscale_ds.GetRasterBand(1).GetOverview(0).Fill(0)
    with gdal.Run(
        "raster",
        "blend",
        input=rgba_ds,
        overlay=grayscale_ds,
        output_format="stream",
        operator="hsv-value",
    ) as alg:
        out_ds = alg.Output()
        subsampled = out_ds.ReadRaster(
            0, 0, Nvalues // 2, 2, buf_xsize=Nvalues // 4, buf_ysize=1
        )
        assert subsampled != rgba_ds.ReadRaster(
            0, 0, Nvalues // 2, 2, buf_xsize=Nvalues // 4, buf_ysize=1
        )

    with gdal.Run(
        "raster",
        "blend",
        input=rgba_ds,
        overlay=grayscale_ds,
        output_format="stream",
        opacity=0,
        operator="hsv-value",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.ReadRaster() == rgba_ds.ReadRaster()

    # Give also the same result given that grayscale_ds matches the value
    # of rgba_ds
    with gdal.Run(
        "raster",
        "blend",
        input=rgba_ds,
        overlay=grayscale_ds,
        output_format="stream",
        opacity=50,
        operator="hsv-value",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.ReadRaster() == rgba_ds.ReadRaster()

    with gdal.Run(
        "raster",
        "blend",
        input=rgba_ds,
        overlay=rgba_ds,
        output_format="stream",
        operator="hsv-value",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.ReadRaster() == rgba_ds.ReadRaster()

    rgb_ds = gdal.Translate("", rgba_ds, bandList=[1, 2, 3], format="MEM")
    with gdal.Run(
        "raster",
        "blend",
        input=rgb_ds,
        overlay=rgb_ds,
        output_format="stream",
        operator="hsv-value",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.ReadRaster() == rgb_ds.ReadRaster()

    grayscale_alpha_ds = gdal.Translate(
        "", grayscale_ds, bandList=[1, "mask"], format="MEM"
    )
    with gdal.Run(
        "raster",
        "blend",
        input=rgb_ds,
        overlay=grayscale_alpha_ds,
        output_format="stream",
        operator="hsv-value",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.ReadRaster() == rgb_ds.ReadRaster()


def test_gdalalg_raster_blend_invalid_input_ds():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with pytest.raises(
        Exception,
        match=r"operator hsv-value requires between 3 and 4 bands",
    ):
        gdal.Run(
            "raster",
            "blend",
            operator="hsv-value",
            input=input_ds,
            overlay=grayscale_ds,
            output_format="stream",
        )

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3, gdal.GDT_UInt16)
    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with pytest.raises(
        Exception,
        match="Only 1-band, 2-band, 3-band or 4-band Byte dataset supported as input",
    ):
        gdal.Run(
            "raster",
            "blend",
            input=input_ds,
            overlay=grayscale_ds,
            output_format="stream",
        )

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3)
    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_UInt16)
    with pytest.raises(
        Exception,
        match="Only 1-band, 2-band, 3-band or 4-band Byte dataset supported as overlay",
    ):
        gdal.Run(
            "raster",
            "blend",
            input=input_ds,
            overlay=grayscale_ds,
            output_format="stream",
        )

    input_ds = gdal.GetDriverByName("MEM").Create("", 2, 10, 3)
    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 2, 9, 1)
    with pytest.raises(
        Exception,
        match="Input dataset and overlay dataset must have the same dimensions",
    ):
        gdal.Run(
            "raster",
            "blend",
            input=input_ds,
            overlay=grayscale_ds,
            output_format="stream",
        )

    input_ds = gdal.GetDriverByName("MEM").Create("", 9, 2, 3)
    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 10, 2, 1)
    with pytest.raises(
        Exception,
        match="Input dataset and overlay dataset must have the same dimensions",
    ):
        gdal.Run(
            "raster",
            "blend",
            input=input_ds,
            overlay=grayscale_ds,
            output_format="stream",
        )


@pytest.mark.parametrize(
    "xsize,ysize",
    [
        (None, None),
        ((1 << 31) / 2, 1),
        ((1 << 31) - 1, (1 << 31) - 1),
        (100 * 1000 * 1000, 100 * 1000 * 1000),
    ],
)
@pytest.mark.parametrize("req_object", ["dataset", "band"])
def test_gdalalg_raster_blend_out_of_memory(tmp_vsimem, xsize, ysize, req_object):

    if xsize == 100 * 1000 * 1000 and gdaltest.is_travis_branch("sanitize"):
        # otherwise it would throw AddressSanitizer: out of memory: allocator is trying to allocate 0x8e1bc9bf040000 bytes
        pytest.skip("skip on ASAN")

    gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "in.tif", 2, 2, 3, options=["COMPRESS=DEFLATE", "BLOCKYSIZE=2"]
    )
    # Truncate the file by writing all bytes but the last 3 ones, to make it
    # invalid and check correct I/O error propagation.
    with gdal.VSIFile(tmp_vsimem / "in.tif", "rb") as f:
        data = f.read()[0:-3]
    with gdal.VSIFile(tmp_vsimem / "in.tif", "wb") as f:
        f.write(data)

    gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "gray.tif", 2, 2, options=["COMPRESS=DEFLATE", "BLOCKYSIZE=2"]
    )
    # Truncate the file by writing all bytes but the last 3 ones, to make it
    # invalid and check correct I/O error propagation.
    with gdal.VSIFile(tmp_vsimem / "gray.tif", "rb") as f:
        data = f.read()[0:-3]
    with gdal.VSIFile(tmp_vsimem / "gray.tif", "wb") as f:
        f.write(data)

    with gdal.Run(
        "raster",
        "blend",
        input=tmp_vsimem / "in.tif",
        overlay=tmp_vsimem / "gray.tif",
        output_format="stream",
        operator="hsv-value",
    ) as alg:
        out_ds = alg.Output()
        req_obj = out_ds if req_object == "dataset" else out_ds.GetRasterBand(1)
        with pytest.raises(Exception):
            req_obj.ReadRaster(
                buf_xsize=xsize,
                buf_ysize=ysize,
                buf_pixel_space=1,
                buf_line_space=1,
                buf_band_space=1,
            )
        with pytest.raises(Exception):
            req_obj.ReadRaster(
                buf_xsize=xsize,
                buf_ysize=ysize,
                buf_pixel_space=1,
                buf_line_space=1,
                buf_band_space=1,
            )


def test_gdalalg_raster_blend_src_over():

    input_rgba_ds = gdal.GetDriverByName("MEM").Create("", 100, 1, 4)
    input_rgba_ds.GetRasterBand(1).Fill(100)
    input_rgba_ds.GetRasterBand(2).Fill(150)
    input_rgba_ds.GetRasterBand(3).Fill(200)
    input_rgba_ds.GetRasterBand(4).Fill(250)

    input_rgb_ds = gdal.Translate("", input_rgba_ds, bandList=[1, 2, 3], format="MEM")

    overlay_rgba_ds = gdal.GetDriverByName("MEM").Create("", 100, 1, 4)
    overlay_rgba_ds.GetRasterBand(1).Fill(205)
    overlay_rgba_ds.GetRasterBand(2).Fill(220)
    overlay_rgba_ds.GetRasterBand(3).Fill(240)
    overlay_rgba_ds.GetRasterBand(4).Fill(250)

    overlay_rgb_ds = gdal.Translate(
        "", overlay_rgba_ds, bandList=[1, 2, 3], format="MEM"
    )

    with gdal.Run(
        "raster",
        "blend",
        input=input_rgb_ds,
        overlay=overlay_rgb_ds,
        output_format="stream",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 3
        assert struct.unpack("B" * 3 * 100, out_ds.ReadRaster()) == struct.unpack(
            "B" * 3 * 100, overlay_rgb_ds.ReadRaster()
        )
        assert struct.unpack(
            "B" * 100, out_ds.GetRasterBand(1).ReadRaster()
        ) == struct.unpack("B" * 100, overlay_rgb_ds.GetRasterBand(1).ReadRaster())
        assert struct.unpack(
            "B" * 100, out_ds.GetRasterBand(2).ReadRaster()
        ) == struct.unpack("B" * 100, overlay_rgb_ds.GetRasterBand(2).ReadRaster())
        assert struct.unpack(
            "B" * 100, out_ds.GetRasterBand(3).ReadRaster()
        ) == struct.unpack("B" * 100, overlay_rgb_ds.GetRasterBand(3).ReadRaster())

    with gdal.Run(
        "raster",
        "blend",
        input=input_rgb_ds,
        overlay=overlay_rgb_ds,
        output_format="stream",
        opacity=0,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 3
        assert struct.unpack("B" * 3 * 100, out_ds.ReadRaster()) == struct.unpack(
            "B" * 3 * 100, input_rgb_ds.ReadRaster()
        )

    with gdal.Run(
        "raster",
        "blend",
        input=input_rgb_ds,
        overlay=overlay_rgb_ds,
        output_format="stream",
        opacity=75,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 3
        assert struct.unpack("B" * 3, out_ds.ReadRaster(0, 0, 1, 1)) == (
            (100 * 25 + 205 * 75) // 100,
            (150 * 25 + 220 * 75) // 100,
            (200 * 25 + 240 * 75) // 100,
        )

    with gdal.Run(
        "raster",
        "blend",
        input=input_rgb_ds,
        overlay=overlay_rgba_ds,
        output_format="stream",
        opacity=75,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 3
        assert struct.unpack("B" * 3, out_ds.ReadRaster(0, 0, 1, 1)) == (177, 201, 229)

    with gdal.Run(
        "raster",
        "blend",
        input=input_rgba_ds,
        overlay=overlay_rgba_ds,
        output_format="stream",
        opacity=75,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 4
        assert struct.unpack("B" * 4, out_ds.ReadRaster(0, 0, 1, 1)) == (
            177,
            201,
            229,
            254,
        )
        assert (
            struct.unpack("B" * 100, out_ds.GetRasterBand(1).ReadRaster())
            == (177,) * 100
        )
        assert (
            struct.unpack("B" * 100, out_ds.GetRasterBand(2).ReadRaster())
            == (201,) * 100
        )
        assert (
            struct.unpack("B" * 100, out_ds.GetRasterBand(3).ReadRaster())
            == (229,) * 100
        )
        assert (
            struct.unpack("B" * 100, out_ds.GetRasterBand(4).ReadRaster())
            == (254,) * 100
        )

    with gdal.Run(
        "raster",
        "blend",
        input=input_rgba_ds,
        overlay=overlay_rgb_ds,
        output_format="stream",
        opacity=100,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 4
        assert struct.unpack("B" * 4, out_ds.ReadRaster(0, 0, 1, 1)) == (
            205,
            220,
            240,
            255,
        )

    input_gray_alpha_ds = gdal.GetDriverByName("MEM").Create("", 100, 1, 2)
    input_gray_alpha_ds.GetRasterBand(1).Fill(125)
    input_gray_alpha_ds.GetRasterBand(2).Fill(250)

    input_gray_ds = gdal.GetDriverByName("MEM").Create("", 100, 1, 1)
    input_gray_ds.GetRasterBand(1).Fill(125)

    with gdal.Run(
        "raster",
        "blend",
        input=input_gray_ds,
        overlay=overlay_rgb_ds,
        output_format="stream",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 1
        assert struct.unpack("B" * 1, out_ds.ReadRaster(0, 0, 1, 1)) == (
            (205 * 299 + 220 * 587 + 240 * 114) // 1000,
        )

    with gdal.Run(
        "raster",
        "blend",
        input=input_gray_ds,
        overlay=overlay_rgb_ds,
        output_format="stream",
        opacity=0,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 1
        assert struct.unpack("B" * 1, out_ds.ReadRaster(0, 0, 1, 1)) == (125,)

    with gdal.Run(
        "raster",
        "blend",
        input=input_gray_ds,
        overlay=overlay_rgb_ds,
        output_format="stream",
        opacity=75,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 1
        assert struct.unpack("B" * 1, out_ds.ReadRaster(0, 0, 1, 1)) == (
            ((205 * 299 + 220 * 587 + 240 * 114) // 1000 * 75 + 125 * 25) // 100,
        )

    overlay_gray_alpha_ds = gdal.GetDriverByName("MEM").Create("", 100, 1, 2)
    overlay_gray_alpha_ds.GetRasterBand(1).Fill(213)
    overlay_gray_alpha_ds.GetRasterBand(2).Fill(250)

    overlay_gray_ds = gdal.GetDriverByName("MEM").Create("", 100, 1, 1)
    overlay_gray_ds.GetRasterBand(1).Fill(213)

    with gdal.Run(
        "raster",
        "blend",
        input=input_rgb_ds,
        overlay=overlay_gray_ds,
        output_format="stream",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 3
        assert struct.unpack("B" * 3, out_ds.ReadRaster(0, 0, 1, 1)) == (213, 213, 213)

    with gdal.Run(
        "raster",
        "blend",
        input=input_rgb_ds,
        overlay=overlay_gray_ds,
        output_format="stream",
        opacity=0,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 3
        assert struct.unpack("B" * 3 * 100, out_ds.ReadRaster()) == struct.unpack(
            "B" * 3 * 100, input_rgb_ds.ReadRaster()
        )

    with gdal.Run(
        "raster",
        "blend",
        input=input_rgb_ds,
        overlay=overlay_gray_ds,
        output_format="stream",
        opacity=75,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 3
        assert struct.unpack("B" * 3, out_ds.ReadRaster(0, 0, 1, 1)) == (
            (100 * 25 + 213 * 75) // 100,
            (150 * 25 + 213 * 75) // 100,
            (200 * 25 + 213 * 75) // 100,
        )

    with gdal.Run(
        "raster",
        "blend",
        input=input_gray_ds,
        overlay=overlay_gray_ds,
        output_format="stream",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 1
        assert struct.unpack("B" * 1 * 100, out_ds.ReadRaster()) == struct.unpack(
            "B" * 1 * 100, overlay_gray_ds.ReadRaster()
        )

    with gdal.Run(
        "raster",
        "blend",
        input=input_gray_ds,
        overlay=overlay_gray_ds,
        output_format="stream",
        opacity=0,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 1
        assert struct.unpack("B" * 1 * 100, out_ds.ReadRaster()) == struct.unpack(
            "B" * 1 * 100, input_gray_ds.ReadRaster()
        )

    with gdal.Run(
        "raster",
        "blend",
        input=input_gray_ds,
        overlay=overlay_gray_ds,
        output_format="stream",
        opacity=75,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 1
        assert struct.unpack("B" * 1, out_ds.ReadRaster(0, 0, 1, 1)) == (
            (213 * 75 + 125 * 25) // 100,
        )

    with gdal.Run(
        "raster",
        "blend",
        input=input_gray_alpha_ds,
        overlay=overlay_gray_alpha_ds,
        output_format="stream",
        opacity=75,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 2
        assert struct.unpack("B" * 2, out_ds.ReadRaster(0, 0, 1, 1)) == (190, 250)

    with gdal.Run(
        "raster",
        "blend",
        input=input_gray_alpha_ds,
        overlay=overlay_gray_ds,
        output_format="stream",
        opacity=75,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 2
        assert struct.unpack("B" * 2, out_ds.ReadRaster(0, 0, 1, 1)) == (191, 222)

    with gdal.Run(
        "raster",
        "blend",
        input=input_gray_ds,
        overlay=overlay_gray_alpha_ds,
        output_format="stream",
        opacity=75,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 1
        assert struct.unpack("B" * 1, out_ds.ReadRaster(0, 0, 1, 1)) == (189,)


def test_gdalalg_raster_blend_src_over_stefan_full_rgba():

    with gdal.Run(
        "raster",
        "blend",
        input="../gcore/data/stefan_full_rgba.tif",
        overlay="../gcore/data/stefan_full_rgba.tif",
        output_format="stream",
        opacity=75,
    ) as alg:
        out_ds = alg.Output()
        assert [out_ds.GetRasterBand(i + 1).Checksum() for i in range(4)] == [
            13392,
            59283,
            36167,
            12963,
        ]


def generic_raster_blend_2bands_over_2bands(
    operator,
    swap_inputs,
    opacity,
    val_alpha,
    overlay_val_alpha,
    expected_val_alpha,
):
    val, alpha = val_alpha
    overlay_val, overlay_alpha = overlay_val_alpha
    expected_val, expected_alpha = expected_val_alpha

    ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 2)
    ds.GetRasterBand(1).Fill(val)
    ds.GetRasterBand(2).Fill(alpha)

    overlay_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 2)
    overlay_ds.GetRasterBand(1).Fill(overlay_val)
    overlay_ds.GetRasterBand(2).Fill(overlay_alpha)

    with gdal.Run(
        "raster",
        "blend",
        input=ds if not swap_inputs else overlay_ds,
        overlay=overlay_ds if not swap_inputs else ds,
        output_format="stream",
        opacity=opacity,
        operator=operator,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 2
        for y in range(2):
            for x in range(2):
                assert struct.unpack("B" * 2, out_ds.ReadRaster(x, y, 1, 1)) == (
                    expected_val,
                    expected_alpha,
                )


def generic_raster_blend_4bands_over_4bands(
    operator,
    swap_inputs,
    opacity,
    rgba,
    overlay_rgba,
    expected_rgba,
):
    r, g, b, a = rgba
    overlay_r, overlay_g, overlay_b, overlay_a = overlay_rgba
    expected_r, expected_g, expected_b, expected_a = expected_rgba

    rgba_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 4)

    rgba_ds.GetRasterBand(1).Fill(r)
    rgba_ds.GetRasterBand(2).Fill(g)
    rgba_ds.GetRasterBand(3).Fill(b)
    rgba_ds.GetRasterBand(4).Fill(a)

    overlay_rgba_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 4)
    overlay_rgba_ds.GetRasterBand(1).Fill(overlay_r)
    overlay_rgba_ds.GetRasterBand(2).Fill(overlay_g)
    overlay_rgba_ds.GetRasterBand(3).Fill(overlay_b)
    overlay_rgba_ds.GetRasterBand(4).Fill(overlay_a)

    with gdal.Run(
        "raster",
        "blend",
        input=rgba_ds if not swap_inputs else overlay_rgba_ds,
        overlay=overlay_rgba_ds if not swap_inputs else rgba_ds,
        output_format="stream",
        opacity=opacity,
        operator=operator,
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 4
        for y in range(2):
            for x in range(2):
                assert struct.unpack("B" * 4, out_ds.ReadRaster(x, y, 1, 1)) == (
                    expected_r,
                    expected_g,
                    expected_b,
                    expected_a,
                )

        # Trigger the other code path by reading subsampled data
        subsampled_r = out_ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
        assert struct.unpack("B", subsampled_r) == (expected_r,)
        subsampled = out_ds.GetRasterBand(2).ReadRaster(0, 0, 1, 1)
        assert struct.unpack("B", subsampled) == (expected_g,)
        subsampled = out_ds.GetRasterBand(3).ReadRaster(0, 0, 1, 1)
        assert struct.unpack("B", subsampled) == (expected_b,)
        subsampled = out_ds.GetRasterBand(4).ReadRaster(0, 0, 1, 1)
        assert struct.unpack("B", subsampled) == (expected_a,)


@pytest.mark.parametrize("swap_inputs", [False, True])
@pytest.mark.parametrize(
    "opacity,rgba,overlay_rgba,expected_rgba",
    [
        (100, (128, 128, 128, 128), (128, 128, 128, 128), (106, 106, 106, 192)),
        (100, (100, 100, 100, 100), (100, 100, 100, 100), (90, 90, 90, 160)),
        # Classic Multiply: Red (255,0,0) * Green (0,255,0) = Black (0,0,0)
        # Both opaque.
        (100, (255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 0, 255)),
        # Identity Case: Pure White source (255) on any background
        # Background is preserved exactly.
        (100, (120, 150, 180, 255), (255, 255, 255, 255), (120, 150, 180, 255)),
        # Semi-Transparent Neutral: White source with 50% alpha
        # Should result in no change to the background.
        (100, (100, 150, 200, 255), (255, 255, 255, 128), (100, 150, 200, 255)),
        # Tinting: Light Gray background tinted by semi-transparent Blue source
        # (200,200,200) backdrop * (0,0,255) source at 128 alpha
        (100, (200, 200, 200, 255), (0, 0, 255, 128), (100, 100, 200, 255)),
        # Deep Darkening: Mid-gray backdrop (128) * Dark-gray source (64)
        # Both opaque; results in 128 * 64 / 255 = 32
        (100, (128, 128, 128, 255), (64, 64, 64, 255), (32, 32, 32, 255)),
        # Partial Overlap: Semi-transparent Blue on Semi-transparent Red
        # Alpha increases to ~191; color shifts toward dark purple/black
        (100, (255, 0, 0, 128), (0, 0, 255, 128), (85, 0, 85, 192)),
        # Background Clipping: Multiply over a fully transparent background
        # Since background alpha is 0, the source color is returned as-is.
        (100, (0, 0, 0, 0), (150, 75, 200, 255), (150, 75, 200, 255)),
        # Low Opacity Interaction: Opaque white backdrop with very low alpha black source
        # Simulates a faint shadow or darkening effect.
        (100, (255, 255, 255, 255), (0, 0, 0, 25), (230, 230, 230, 255)),
        # Mid-tone Overlap: Opaque 50% grays
        # Resulting color: 128 * 128 / 255 = 64
        (100, (128, 128, 128, 255), (128, 128, 128, 255), (64, 64, 64, 255)),
    ],
)
def test_gdalalg_raster_blend_multiply_4bands_over_4bands(
    swap_inputs,
    opacity,
    rgba,
    overlay_rgba,
    expected_rgba,
):

    # Skip if swap_inputs is True and the opacity is != 100, as the inputs differ and
    # the test is not symmetric anymore
    if swap_inputs and opacity != 100:
        return

    r, g, b, a = rgba
    overlay_r, overlay_g, overlay_b, overlay_a = overlay_rgba
    expected_r, expected_g, expected_b, expected_a = expected_rgba

    generic_raster_blend_4bands_over_4bands(
        "multiply",
        swap_inputs,
        opacity,
        rgba,
        overlay_rgba,
        expected_rgba,
    )


@pytest.mark.parametrize("swap_inputs", [False, True])
@pytest.mark.parametrize(
    "opacity,rgba,overlay_value_alpha,expected_rgba",
    [
        (100, (0, 0, 0, 255), (255, 255), (0, 0, 0, 255)),
        (100, (255, 255, 255, 255), (255, 255), (255, 255, 255, 255)),
        (100, (128, 128, 128, 255), (255, 255), (128, 128, 128, 255)),
        (100, (10, 20, 30, 255), (128, 255), (5, 10, 15, 255)),
    ],
)
def test_gdalalg_raster_blend_multiply_2bands_over_4bands(
    swap_inputs,
    opacity,
    rgba,
    overlay_value_alpha,
    expected_rgba,
):
    r, g, b, a = rgba
    overlay_value, overlay_alpha = overlay_value_alpha
    expected_r, expected_g, expected_b, expected_a = expected_rgba

    rgba_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 4)
    rgba_ds.GetRasterBand(1).Fill(r)
    rgba_ds.GetRasterBand(2).Fill(g)
    rgba_ds.GetRasterBand(3).Fill(b)
    rgba_ds.GetRasterBand(4).Fill(a)

    overlay_rgba_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 2)
    overlay_rgba_ds.GetRasterBand(1).Fill(overlay_value)
    overlay_rgba_ds.GetRasterBand(2).Fill(overlay_alpha)

    with gdal.Run(
        "raster",
        "blend",
        input=rgba_ds if not swap_inputs else overlay_rgba_ds,
        overlay=overlay_rgba_ds if not swap_inputs else rgba_ds,
        output_format="stream",
        opacity=opacity,
        operator="multiply",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 4
        for y in range(2):
            for x in range(2):
                assert struct.unpack("B" * 4, out_ds.ReadRaster(x, y, 1, 1)) == (
                    expected_r,
                    expected_g,
                    expected_b,
                    expected_a,
                )


@pytest.mark.parametrize("swap_inputs", [False, True])
@pytest.mark.parametrize(
    "opacity,value_alpha,overlay_value_alpha,expected_value_alpha",
    [
        (100, (0, 0), (0, 0), (0, 0)),
        (100, (255, 255), (255, 255), (255, 255)),
        (100, (128, 255), (10, 255), (5, 255)),
    ],
)
def test_gdalalg_raster_blend_multiply_2bands_over_2bands(
    swap_inputs,
    opacity,
    value_alpha,
    overlay_value_alpha,
    expected_value_alpha,
):
    generic_raster_blend_2bands_over_2bands(
        "multiply",
        swap_inputs,
        opacity,
        value_alpha,
        overlay_value_alpha,
        expected_value_alpha,
    )


@pytest.mark.parametrize("swap_inputs", [False, True])
@pytest.mark.parametrize(
    "opacity,value_alpha,overlay_value,expected_value_alpha",
    [
        (100, (0, 0), 0, (0, 255)),
        (100, (255, 255), 255, (255, 255)),
        (100, (128, 255), 10, (5, 255)),
    ],
)
def test_gdalalg_raster_blend_multiply_1band_over_2bands(
    swap_inputs, opacity, value_alpha, overlay_value, expected_value_alpha
):
    value, alpha = value_alpha
    expected_value, expected_alpha = expected_value_alpha

    rgba_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 2)
    rgba_ds.GetRasterBand(1).Fill(value)
    rgba_ds.GetRasterBand(2).Fill(alpha)

    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 1)
    grayscale_ds.GetRasterBand(1).Fill(overlay_value)

    with gdal.Run(
        "raster",
        "blend",
        input=rgba_ds if not swap_inputs else grayscale_ds,
        overlay=grayscale_ds if not swap_inputs else rgba_ds,
        output_format="stream",
        opacity=opacity,
        operator="multiply",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 2
        for y in range(2):
            for x in range(2):
                assert struct.unpack("B" * 2, out_ds.ReadRaster(x, y, 1, 1)) == (
                    expected_value,
                    expected_alpha,
                )


@pytest.mark.parametrize("swap_inputs", [False, True])
@pytest.mark.parametrize(
    "opacity,value,overlay_value,expected_value",
    [
        (100, 0, 0, 0),
        (100, 255, 255, 255),
        (100, 128, 10, 5),
    ],
)
def test_gdalalg_raster_blend_multiply_1band_over_1band(
    swap_inputs, opacity, value, overlay_value, expected_value
):
    grayscale_1_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 1)
    grayscale_1_ds.GetRasterBand(1).Fill(value)

    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 1)
    grayscale_ds.GetRasterBand(1).Fill(overlay_value)

    with gdal.Run(
        "raster",
        "blend",
        input=grayscale_1_ds if not swap_inputs else grayscale_ds,
        overlay=grayscale_ds if not swap_inputs else grayscale_1_ds,
        output_format="stream",
        opacity=opacity,
        operator="multiply",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 1
        for y in range(2):
            for x in range(2):
                assert struct.unpack("B" * 1, out_ds.ReadRaster(x, y, 1, 1)) == (
                    expected_value,
                )


def test_gdalalg_raster_blend_in_pipeline(tmp_vsimem):

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "grayscale.tif", 2, 2, 1
    ) as ds:
        ds.GetRasterBand(1).Fill(128)

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "overlay.tif", 2, 2, 1
    ) as ds:
        ds.GetRasterBand(1).Fill(10)

    with gdal.alg.pipeline(
        pipeline=f"read {tmp_vsimem}/grayscale.tif ! blend --operator multiply --overlay {tmp_vsimem}/overlay.tif"
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 1
        for y in range(2):
            for x in range(2):
                assert struct.unpack("B" * 1, out_ds.ReadRaster(x, y, 1, 1)) == (5,)

    with gdal.alg.pipeline(
        pipeline=f"read {tmp_vsimem}/overlay.tif ! blend --operator multiply --input {tmp_vsimem}/grayscale.tif --overlay _PIPE_"
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 1
        for y in range(2):
            for x in range(2):
                assert struct.unpack("B" * 1, out_ds.ReadRaster(x, y, 1, 1)) == (5,)

    with gdal.alg.pipeline(
        pipeline=f"read {tmp_vsimem}/grayscale.tif ! blend --operator multiply --input _PIPE_ --overlay _PIPE_"
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 1
        for y in range(2):
            for x in range(2):
                assert struct.unpack("B" * 1, out_ds.ReadRaster(x, y, 1, 1)) == (64,)


@pytest.mark.parametrize("swap_inputs", [False, True])
@pytest.mark.parametrize(
    "opacity,paletted_value,overlay_value,expected_rgba",
    [
        (100, 0, 0, (0, 0, 0, 255)),
        (100, 1, 255, (255, 255, 255, 255)),
        (100, 2, 128, (5, 10, 15, 255)),
    ],
)
def test_gdalalg_raster_blend_multiply_1band_paletted_over_1band(
    swap_inputs,
    opacity,
    paletted_value,
    overlay_value,
    expected_rgba,
):

    expected_r, expected_g, expected_b, expected_a = expected_rgba

    paletted_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 1)
    paletted_ds.GetRasterBand(1).Fill(paletted_value)

    # Add color table to make it paletted
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (0, 0, 0, 255))
    ct.SetColorEntry(1, (255, 255, 255, 255))
    ct.SetColorEntry(2, (10, 20, 30, 255))
    paletted_ds.GetRasterBand(1).SetColorTable(ct)

    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 1)
    grayscale_ds.GetRasterBand(1).Fill(overlay_value)

    with gdal.Run(
        "raster",
        "blend",
        input=paletted_ds if not swap_inputs else grayscale_ds,
        overlay=grayscale_ds if not swap_inputs else paletted_ds,
        output_format="stream",
        opacity=opacity,
        operator="multiply",
    ) as alg:
        out_ds = alg.Output()
        assert out_ds.RasterCount == 4
        for y in range(2):
            for x in range(2):
                assert struct.unpack("B" * 4, out_ds.ReadRaster(x, y, 1, 1)) == (
                    expected_r,
                    expected_g,
                    expected_b,
                    expected_a,
                )

        # Trigger the other code path by reading subsampled data
        subsampled_r = out_ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
        assert struct.unpack("B", subsampled_r) == (expected_r,)
        subsampled = out_ds.GetRasterBand(2).ReadRaster(0, 0, 1, 1)
        assert struct.unpack("B", subsampled) == (expected_g,)
        subsampled = out_ds.GetRasterBand(3).ReadRaster(0, 0, 1, 1)
        assert struct.unpack("B", subsampled) == (expected_b,)
        subsampled = out_ds.GetRasterBand(4).ReadRaster(0, 0, 1, 1)
        assert struct.unpack("B", subsampled) == (expected_a,)


@pytest.mark.parametrize("swap_inputs", [False, True])
@pytest.mark.parametrize(
    "opacity,rgba,overlay_rgba,expected_rgba",
    [
        # 1. Inputs (100, 100, 100, 100)
        (100, (100, 100, 100, 100), (100, 100, 100, 100), (116, 116, 116, 160)),
        # 2. Inputs (128, 128, 128, 128)
        (100, (128, 128, 128, 128), (128, 128, 128, 128), (148, 148, 148, 192)),
        # 3. Opaque Mid-grays (Screen results in lighter color)
        (100, (128, 128, 128, 255), (128, 128, 128, 255), (192, 192, 192, 255)),
        # 4. Screen with Black (Identity: no change)
        (100, (150, 150, 150, 255), (0, 0, 0, 255), (150, 150, 150, 255)),
        # 5. Screen with White (Result is always white)
        (100, (150, 150, 150, 255), (255, 255, 255, 255), (255, 255, 255, 255)),
    ],
)
def test_gdalalg_raster_blend_screen_4bands_over_4bands(
    swap_inputs, opacity, rgba, overlay_rgba, expected_rgba
):
    generic_raster_blend_4bands_over_4bands(
        "screen", swap_inputs, opacity, rgba, overlay_rgba, expected_rgba
    )


@pytest.mark.parametrize(
    "opacity,rgba,overlay_rgba,expected_rgba",
    [
        # 1. Mid-tone case (100, 100, 100, 100)
        # Calculation uses the first branch (Multiply-like) because 2*Dca < Da.
        (100, (100, 100, 100, 100), (100, 100, 100, 100), (102, 102, 102, 160)),
        # 2. Mid-tone case (128, 128, 128, 128)
        # At exactly 128, the branches meet; the result is the neutral midpoint.
        (100, (128, 128, 128, 128), (128, 128, 128, 128), (127, 127, 127, 192)),
        # 3. Dark source on light opaque background
        # 2*Dca (400) > Da (255) -> Uses second branch. Result is darker than background.
        (100, (200, 200, 200, 255), (50, 50, 50, 255), (165, 165, 165, 255)),
        # 4. Light source on dark opaque background
        # 2*Dca (100) < Da (255) -> Uses first branch. Result is lighter than background.
        (100, (50, 50, 50, 255), (200, 200, 200, 255), (80, 80, 80, 255)),
        # 5. Full Transparency: Empty Background
        # Background alpha is 0, so the source color is returned unchanged.
        (100, (0, 0, 0, 0), (150, 100, 50, 255), (150, 100, 50, 255)),
        # 6. Full Transparency: Empty Source
        # Source alpha is 0, so the background color remains unchanged.
        (100, (150, 100, 50, 255), (0, 0, 0, 0), (150, 100, 50, 255)),
        # 7. Semi-transparent Overlap: (200, 200, 200, 128) on (100, 100, 100, 128)
        # Background is dark (2*Dca < Da), branch 1 used. Resulting alpha ~191.
        (100, (100, 100, 100, 128), (200, 200, 200, 128), (152, 152, 152, 192)),
    ],
)
def test_gdalalg_raster_blend_overlay_4bands_over_4bands(
    opacity, rgba, overlay_rgba, expected_rgba
):

    r, g, b, a = rgba
    overlay_r, overlay_g, overlay_b, overlay_a = overlay_rgba
    expected_r, expected_g, expected_b, expected_a = expected_rgba

    generic_raster_blend_4bands_over_4bands(
        "overlay", False, opacity, rgba, overlay_rgba, expected_rgba
    )


@pytest.mark.parametrize(
    "opacity,rgba,overlay_rgba,expected_rgba",
    [
        (100, (10, 20, 30, 255), (128, 128, 128, 255), (11, 21, 31, 255)),
        (100, (128, 128, 128, 255), (10, 20, 30, 255), (10, 20, 30, 255)),
    ],
)
def test_gdalalg_raster_blend_hard_light_4bands_over_4bands(
    opacity, rgba, overlay_rgba, expected_rgba
):

    generic_raster_blend_4bands_over_4bands(
        "hard-light", False, opacity, rgba, overlay_rgba, expected_rgba
    )


@pytest.mark.parametrize("swap_inputs", [False, True])
@pytest.mark.parametrize(
    "opacity,rgba,overlay_rgba,expected_rgba",
    [
        # 1. Mid-tone case (100, 128) on (200, 128)
        # B: (100, 100, 100, 128), S: (200, 200, 200, 128)
        # The max function picks the brighter source contribution.
        (100, (100, 100, 100, 128), (200, 200, 200, 128), (166, 166, 166, 192)),
        # 2. Balanced Mid-tones (128, 128, 128, 128) on (128, 128, 128, 128)
        # Both are identical, so max does not change the relative intensity.
        (100, (128, 128, 128, 128), (128, 128, 128, 128), (127, 127, 127, 192)),
        # 3. Opaque Comparison: Source is brighter (S=200, B=50)
        # Result is simply the brighter opaque color.
        (100, (50, 50, 50, 255), (200, 200, 200, 255), (200, 200, 200, 255)),
        # 4. Opaque Comparison: Background is brighter (S=50, B=200)
        # Result is simply the brighter opaque color.
        (100, (200, 200, 200, 255), (50, 50, 50, 255), (200, 200, 200, 255)),
        # 5. High Contrast Translucent: Red (255,0,0,128) on Blue (0,0,255,128)
        # Max picks the available color for each channel.
        (100, (0, 0, 255, 128), (255, 0, 0, 128), (170, 0, 170, 192)),
        # 6. Background Clipping: Background alpha 0
        # Result must be the source color.
        (100, (0, 0, 0, 0), (100, 150, 200, 255), (100, 150, 200, 255)),
        # 7. Source Clipping: Source alpha 0
        # Result must be the background color.
        (100, (100, 150, 200, 255), (0, 0, 0, 0), (100, 150, 200, 255)),
    ],
)
def test_gdalalg_raster_blend_lighten_4bands_over_4bands(
    swap_inputs, opacity, rgba, overlay_rgba, expected_rgba
):

    generic_raster_blend_4bands_over_4bands(
        "lighten", swap_inputs, opacity, rgba, overlay_rgba, expected_rgba
    )


@pytest.mark.parametrize("swap_inputs", [False])
@pytest.mark.parametrize(
    "opacity,rgba,overlay_rgba,expected_rgba",
    [
        # 1. Mid-tone case (100, 100, 100, 100)
        # Both colors are identical; the min function returns the original relative intensity.
        (100, (100, 100, 100, 100), (100, 100, 100, 100), (105, 105, 105, 160)),
        # 2. Mid-tone case (128, 128, 128, 128)
        (100, (128, 128, 128, 128), (128, 128, 128, 128), (127, 127, 127, 192)),
        # 3. Darker Source (S=50, B=200, both opaque)
        # Darken selects the minimum channel value: 50.
        (100, (200, 200, 200, 255), (50, 50, 50, 255), (50, 50, 50, 255)),
        # 4. Darker Background (S=200, B=50, both opaque)
        # Darken selects the minimum channel value: 50.
        (100, (50, 50, 50, 255), (200, 200, 200, 255), (50, 50, 50, 255)),
        # 5. Semi-transparent Overlap: Red (255,0,0,128) and Blue (0,0,255,128)
        # The min function on R and B channels results in a dark purple/gray.
        (100, (255, 0, 0, 128), (0, 0, 255, 128), (85, 0, 85, 192)),
        # 6. Empty Background (Alpha 0)
        # Result must be the source color.
        (100, (0, 0, 0, 0), (100, 150, 200, 255), (100, 150, 200, 255)),
        # 7. Empty Source (Alpha 0)
        # Result must be the background color.
        (100, (100, 150, 200, 255), (0, 0, 0, 0), (100, 150, 200, 255)),
        # 8. Diverse Transparency: (150, 150, 150, 200) on (100, 100, 100, 50)
        # Mixed alphas pull the result towards the darker background.
        (100, (100, 100, 100, 50), (150, 150, 150, 200), (140, 140, 140, 210)),
    ],
)
def test_gdalalg_raster_blend_darken_4bands_over_4bands(
    swap_inputs, opacity, rgba, overlay_rgba, expected_rgba
):

    generic_raster_blend_4bands_over_4bands(
        "darken", swap_inputs, opacity, rgba, overlay_rgba, expected_rgba
    )


@pytest.mark.parametrize("blend_mode", ["darken", "lighten"])
@pytest.mark.parametrize("swap_inputs", [False, True])
@pytest.mark.parametrize(
    "input_bands,overlay_bands",
    [
        (4, 2),
        (3, 2),
        (4, 1),
        (3, 1),
    ],
)
def test_gdalalg_raster_blend_darken_lighten_invalid_input(
    blend_mode, swap_inputs, input_bands, overlay_bands
):

    rgba_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, input_bands)
    overlay_rgba_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, overlay_bands)

    with pytest.raises(
        Exception,
        match=r"the source dataset and overlay dataset must have the same number of bands",
    ):
        gdal.Run(
            "raster",
            "blend",
            operator=blend_mode,
            input=rgba_ds if not swap_inputs else overlay_rgba_ds,
            overlay=overlay_rgba_ds if not swap_inputs else rgba_ds,
            output_format="stream",
        )


@pytest.mark.parametrize("swap_inputs", [False, True])
@pytest.mark.parametrize(
    "opacity,val_alpha,overlay_val_alpha,expected_val_alpha",
    [
        (100, (100, 255), (200, 255), (200, 255)),
        (100, (100, 255), (200, 128), (150, 255)),
        (100, (100, 128), (200, 128), (166, 192)),
    ],
)
def test_gdalalg_raster_blend_lighten_2bands_over_2bands(
    swap_inputs, opacity, val_alpha, overlay_val_alpha, expected_val_alpha
):
    generic_raster_blend_2bands_over_2bands(
        "lighten",
        swap_inputs,
        opacity,
        val_alpha,
        overlay_val_alpha,
        expected_val_alpha,
    )


@pytest.mark.parametrize("swap_inputs", [False, True])
@pytest.mark.parametrize(
    "opacity,val_alpha,overlay_val_alpha,expected_val_alpha",
    [
        (100, (100, 255), (200, 255), (100, 255)),
        (100, (100, 255), (200, 128), (100, 255)),
        (100, (100, 128), (200, 128), (132, 192)),
    ],
)
def test_gdalalg_raster_blend_darken_2bands_over_2bands(
    swap_inputs, opacity, val_alpha, overlay_val_alpha, expected_val_alpha
):
    generic_raster_blend_2bands_over_2bands(
        "darken",
        swap_inputs,
        opacity,
        val_alpha,
        overlay_val_alpha,
        expected_val_alpha,
    )


@pytest.mark.parametrize("swap_inputs", [False])
@pytest.mark.parametrize(
    "opacity,rgba,overlay_rgba,expected_rgba",
    [
        # 1. Low-mid Case: (100, 100, 100, 100) on (100, 100, 100, 100)
        # Condition: 0.1206 < 0.1538 (Branch 2 is used).
        # The source is not bright enough to saturate the dodge, resulting in a moderate lighten.
        (100, (100, 100, 100, 100), (100, 100, 100, 100), (121, 121, 121, 160)),
        # 2. Mid-point Case: (128, 128, 128, 128) on (128, 128, 128, 128)
        # Condition: 0.2529 >= 0.2519 (Branch 1 is used).
        # Hits the saturation point where the result becomes the maximum alpha union color.
        (100, (128, 128, 128, 128), (128, 128, 128, 128), (170, 170, 170, 192)),
        # 3. Opaque Identity: Black source (0, 0, 0, 255) on gray background
        # In Dodge, black is the neutral color. Background remains unchanged.
        (100, (150, 150, 150, 255), (0, 0, 0, 255), (150, 150, 150, 255)),
        # 4. Opaque Saturated: White source (255, 255, 255, 255) on gray background
        # Any color dodged with white results in pure white.
        (100, (150, 150, 150, 255), (255, 255, 255, 255), (255, 255, 255, 255)),
        # 5. Tinted Highlight: Semi-transparent Blue on Opaque Gray
        # (0, 0, 255, 128) on (100, 100, 100, 255)
        # The Blue channel saturates (Branch 1), while R and G stay in Branch 2.
        (100, (100, 100, 100, 255), (0, 0, 255, 128), (100, 100, 178, 255)),
        # 6. Background Clipping: Dodge over fully transparent background
        # If the background alpha is 0, the source color is returned as-is.
        (100, (0, 0, 0, 0), (100, 200, 50, 255), (100, 200, 50, 255)),
        # 7. Source Clipping: Fully transparent source
        # If the source alpha is 0, the background remains unchanged.
        (100, (100, 150, 200, 255), (0, 0, 0, 0), (100, 150, 200, 255)),
    ],
)
def test_gdalalg_raster_blend_color_dodge_4bands_over_4bands(
    swap_inputs, opacity, rgba, overlay_rgba, expected_rgba
):

    generic_raster_blend_4bands_over_4bands(
        "color-dodge", swap_inputs, opacity, rgba, overlay_rgba, expected_rgba
    )


@pytest.mark.parametrize("swap_inputs", [False])
@pytest.mark.parametrize(
    "opacity,rgba,overlay_rgba,expected_rgba",
    [
        # 1. Mid-tone case (100, 100, 100, 100)
        # Condition: 0.1206 <= 0.1537 is True. Branch 1 is used.
        # High darkening effect due to the low alpha overlap.
        (100, (100, 100, 100, 100), (100, 100, 100, 100), (79, 79, 79, 160)),
        # 2. Mid-tone case (128, 128, 128, 128)
        # Condition: 0.2529 <= 0.2519 is False. Branch 2 is used.
        # Results in a slightly lighter value than the 100-case due to the branch switch.
        (100, (128, 128, 128, 128), (128, 128, 128, 128), (85, 85, 85, 192)),
        # 3. Opaque Identity: White source on gray background
        # White (Sca=Sa) always results in the second branch simplifying to Dca.
        (100, (150, 150, 150, 255), (255, 255, 255, 255), (150, 150, 150, 255)),
        # 4. Opaque Maximum Burn: Black source on gray background
        # Black (Sca=0) triggers Branch 1. Result is 0 (Black).
        (100, (150, 150, 150, 255), (0, 0, 0, 255), (0, 0, 0, 255)),
        # 5. Partial Transparency Overlap
        # (200, 200, 200, 128) on (100, 100, 100, 128).
        # Light source on dark background, Branch 2 used.
        (100, (100, 100, 100, 128), (200, 200, 200, 128), (114, 114, 114, 192)),
        # 6. Empty Background: Source color is preserved
        (100, (0, 0, 0, 0), (100, 150, 200, 255), (100, 150, 200, 255)),
        # 7. Empty Source: Background color is preserved
        (100, (100, 150, 200, 255), (0, 0, 0, 0), (100, 150, 200, 255)),
        # 8. Opaque Mid-tone Burn
        (100, (102, 153, 204, 255), (0, 0, 0, 255), (0, 0, 0, 255)),
    ],
)
def test_gdalalg_raster_blend_color_burn_4bands_over_4bands(
    swap_inputs, opacity, rgba, overlay_rgba, expected_rgba
):

    generic_raster_blend_4bands_over_4bands(
        "color-burn", swap_inputs, opacity, rgba, overlay_rgba, expected_rgba
    )
