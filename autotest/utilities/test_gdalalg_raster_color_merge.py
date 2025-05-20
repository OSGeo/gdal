#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster color-merge' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array

import gdaltest
import pytest

from osgeo import gdal


# Check that (R,G,B) -> (H,S,V) -> (R,G,B) correctly roundtrips
def test_gdalalg_raster_color_merge_check_rgb_hsb_conversion():

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
        "color-merge",
        input=rgba_ds,
        grayscale=grayscale_ds,
        output_format="stream",
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
        "color-merge",
        input=rgba_ds,
        grayscale=grayscale_ds,
        output_format="stream",
    ) as alg:
        out_ds = alg.Output()
        subsampled = out_ds.ReadRaster(
            0, 0, Nvalues // 2, 2, buf_xsize=Nvalues // 4, buf_ysize=1
        )
        assert subsampled != rgba_ds.ReadRaster(
            0, 0, Nvalues // 2, 2, buf_xsize=Nvalues // 4, buf_ysize=1
        )


def test_gdalalg_raster_color_merge_invalid_input_ds():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with pytest.raises(
        Exception, match="Only 3 or 4-band Byte dataset supported as input"
    ):
        gdal.Run(
            "raster",
            "color-merge",
            input=input_ds,
            grayscale=grayscale_ds,
            output_format="stream",
        )

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 5)
    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with pytest.raises(
        Exception, match="Only 3 or 4-band Byte dataset supported as input"
    ):
        gdal.Run(
            "raster",
            "color-merge",
            input=input_ds,
            grayscale=grayscale_ds,
            output_format="stream",
        )

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3, gdal.GDT_UInt16)
    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with pytest.raises(
        Exception, match="Only 3 or 4-band Byte dataset supported as input"
    ):
        gdal.Run(
            "raster",
            "color-merge",
            input=input_ds,
            grayscale=grayscale_ds,
            output_format="stream",
        )

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3)
    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    with pytest.raises(
        Exception, match="Only 1-band Byte dataset supported as grayscale dataset"
    ):
        gdal.Run(
            "raster",
            "color-merge",
            input=input_ds,
            grayscale=grayscale_ds,
            output_format="stream",
        )

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 3)
    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_UInt16)
    with pytest.raises(
        Exception, match="Only 1-band Byte dataset supported as grayscale dataset"
    ):
        gdal.Run(
            "raster",
            "color-merge",
            input=input_ds,
            grayscale=grayscale_ds,
            output_format="stream",
        )

    input_ds = gdal.GetDriverByName("MEM").Create("", 2, 10, 3)
    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 2, 9, 1)
    with pytest.raises(
        Exception,
        match="Input RGB/RGBA dataset and grayscale dataset must have the same dimensions",
    ):
        gdal.Run(
            "raster",
            "color-merge",
            input=input_ds,
            grayscale=grayscale_ds,
            output_format="stream",
        )

    input_ds = gdal.GetDriverByName("MEM").Create("", 9, 2, 3)
    grayscale_ds = gdal.GetDriverByName("MEM").Create("", 10, 2, 1)
    with pytest.raises(
        Exception,
        match="Input RGB/RGBA dataset and grayscale dataset must have the same dimensions",
    ):
        gdal.Run(
            "raster",
            "color-merge",
            input=input_ds,
            grayscale=grayscale_ds,
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
def test_gdalalg_raster_color_merge_out_of_memory(tmp_vsimem, xsize, ysize, req_object):

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
        "color-merge",
        input=tmp_vsimem / "in.tif",
        grayscale=tmp_vsimem / "gray.tif",
        output_format="stream",
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
