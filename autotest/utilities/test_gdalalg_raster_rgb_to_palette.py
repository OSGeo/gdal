#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster rgb-to-palette' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal


def get_alg():
    return ["raster", "rgb-to-palette"]


def test_gdalalg_raster_rgb_to_palette_nominal(tmp_vsimem):

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    with gdal.Run(get_alg(), input=src_ds, output=tmp_vsimem / "out.tif") as alg:
        ds = alg.Output()
        assert ds.RasterCount == 1
        ct = ds.GetRasterBand(1).GetColorTable()
        assert ct.GetCount() == 256
        assert ct.GetColorEntry(0) == (188, 168, 100, 255)
        assert ds.GetRasterBand(1).Checksum() == 14890
        assert ds.GetGeoTransform() == src_ds.GetGeoTransform()
        assert ds.GetSpatialRef().IsSame(src_ds.GetSpatialRef())
        assert ds.GetMetadata() == src_ds.GetMetadata()


def test_gdalalg_raster_rgb_to_palette_mem_output():

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    with gdal.Run(get_alg(), input=src_ds, output_format="MEM") as alg:
        ds = alg.Output()
        assert ds.RasterCount == 1
        ct = ds.GetRasterBand(1).GetColorTable()
        assert ct.GetCount() == 256
        assert ct.GetColorEntry(0) == (188, 168, 100, 255)
        assert ds.GetRasterBand(1).Checksum() == 14890
        assert ds.GetGeoTransform() == src_ds.GetGeoTransform()
        assert ds.GetSpatialRef().IsSame(src_ds.GetSpatialRef())
        assert ds.GetMetadata() == src_ds.GetMetadata()


@pytest.mark.parametrize(
    "creation_option", [{}, {"COMPRESS": "LZW"}, {"TILED": "YES", "COMPRESS": "LZW"}]
)
def test_gdalalg_raster_rgb_to_palette_nominal_with_progress(
    tmp_vsimem, creation_option
):

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        tab_pct[0] = pct
        return True

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    assert gdal.Run(
        get_alg(),
        input=src_ds,
        output=tmp_vsimem / "out.tif",
        creation_option=creation_option,
        progress=my_progress,
    )

    with gdal.Open(tmp_vsimem / "out.tif") as ds:

        assert gdal.VSIStatL(ds.GetDescription() + ".tmp.tif") is None

        assert tab_pct[0] == 1.0

        assert ds.RasterCount == 1
        ct = ds.GetRasterBand(1).GetColorTable()
        assert ct.GetCount() == 256
        assert ct.GetColorEntry(0) == (188, 168, 100, 255)
        assert ds.GetRasterBand(1).Checksum() == 14890
        assert ds.GetGeoTransform() == src_ds.GetGeoTransform()
        assert ds.GetSpatialRef().IsSame(src_ds.GetSpatialRef())
        assert ds.GetMetadata() == src_ds.GetMetadata()
        if "TILED" in creation_option:
            assert ds.GetRasterBand(1).GetBlockSize() == [256, 256]
        if creation_option.get("COMPRESS", None):
            assert (
                ds.GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE")
                == creation_option["COMPRESS"]
            )


def test_gdalalg_raster_rgb_to_palette_not_enough_bands(tmp_vsimem):

    with pytest.raises(Exception, match="Input dataset must have at least 3 bands"):
        gdal.Run(
            get_alg(), input="../gcore/data/byte.tif", output=tmp_vsimem / "out.tif"
        )


def test_gdalalg_raster_rgb_to_palette_not_byte(tmp_vsimem):

    with pytest.raises(Exception, match="Non-byte band found and not supported"):
        gdal.Run(
            get_alg(),
            input=gdal.Translate(
                "",
                "../gcore/data/rgbsmall.tif",
                format="MEM",
                outputType=gdal.GDT_UInt16,
            ),
            output=tmp_vsimem / "out.tif",
        )


def test_gdalalg_raster_rgb_to_palette_4_bands(tmp_vsimem):

    with gdal.Run(
        get_alg(),
        input="../gcore/data/stefan_full_rgba.tif",
        output=tmp_vsimem / "out.tif",
    ) as alg:
        ds = alg.Output()
        assert ds.RasterCount == 1
        ct = ds.GetRasterBand(1).GetColorTable()
        assert ct.GetCount() == 256
        assert ct.GetColorEntry(0) == (0, 0, 0, 0)
        assert ct.GetColorEntry(1) == (80, 188, 8, 255)
        assert ct.GetColorEntry(255) == (248, 216, 56, 255)
        assert ds.GetRasterBand(1).Checksum() == 19683


def test_gdalalg_raster_rgb_to_palette_no_color_interp(tmp_vsimem):

    src_ds = gdal.Translate(
        "",
        "../gdrivers/data/small_world.tif",
        format="MEM",
        colorInterpretation=["undefined", "undefined", "undefined"],
    )
    with gdal.Run(get_alg(), input=src_ds, output=tmp_vsimem / "out.tif") as alg:
        ds = alg.Output()
        assert ds.RasterCount == 1
        ct = ds.GetRasterBand(1).GetColorTable()
        assert ct.GetCount() == 256
        assert ct.GetColorEntry(0) == (188, 168, 100, 255)
        assert ds.GetRasterBand(1).Checksum() == 14890


def test_gdalalg_raster_rgb_to_palette_bgr_ordered():

    src_ds = gdal.Translate(
        "", "../gdrivers/data/small_world.tif", format="MEM", bandList=[3, 2, 1]
    )
    with gdal.Run(get_alg(), input=src_ds, output_format="MEM") as alg:
        ds = alg.Output()
        assert ds.RasterCount == 1
        ct = ds.GetRasterBand(1).GetColorTable()
        assert ct.GetCount() == 256
        assert ct.GetColorEntry(0) == (188, 168, 100, 255)
        assert ds.GetRasterBand(1).Checksum() == 14890


def test_gdalalg_raster_rgb_to_palette_rrr():

    src_ds = gdal.Translate(
        "", "../gdrivers/data/small_world.tif", format="MEM", bandList=[1, 1, 1]
    )
    with pytest.raises(Exception, match="Several Red bands found"):
        gdal.Run(get_alg(), input=src_ds, output_format="MEM")


def test_gdalalg_raster_rgb_to_palette_r_g_undefined():

    src_ds = gdal.Translate(
        "",
        "../gdrivers/data/small_world.tif",
        format="MEM",
        colorInterpretation=["red", "green", "undefined"],
    )
    with gdaltest.error_raised(gdal.CE_Warning):
        with gdal.Run(get_alg(), input=src_ds, output_format="MEM") as alg:
            ds = alg.Output()
            assert ds.RasterCount == 1
            ct = ds.GetRasterBand(1).GetColorTable()
            assert ct.GetCount() == 256
            assert ct.GetColorEntry(0) == (188, 168, 100, 255)
            assert ds.GetRasterBand(1).Checksum() == 14890


def test_gdalalg_raster_rgb_to_palette_cannot_create_output_dataset():

    with pytest.raises(Exception, match="Attempt to create new tiff file"):
        gdal.Run(
            get_alg(),
            input="../gdrivers/data/small_world.tif",
            output="/i_do/not/exist.tif",
        )


def test_gdalalg_raster_rgb_to_palette_colortable_from_other_dataset():

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    with gdal.Run(
        get_alg(),
        input=src_ds,
        output_format="MEM",
        color_map="data/color_paletted_red_green_0-255.txt",
    ) as alg:
        ds = alg.Output()
        assert ds.RasterCount == 1
        ct = ds.GetRasterBand(1).GetColorTable()
        assert ct.GetColorEntry(0) == (255, 255, 255, 0)
        assert ct.GetColorEntry(1) == (128, 128, 128, 255)


def test_gdalalg_raster_rgb_to_palette_colortable_from_other_dataset_no_ct():

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    with pytest.raises(Exception, match="does not contain a color table"):
        gdal.Run(
            get_alg(),
            input=src_ds,
            output_format="MEM",
            color_map="../gcore/data/byte.tif",
        )


@pytest.mark.require_driver("PNG")
def test_gdalalg_raster_rgb_to_palette_colortable_from_text_file():

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    with gdal.Run(
        get_alg(),
        input=src_ds,
        output_format="MEM",
        color_map="../gcore/data/stefan_full_rgba_pct32.png",
    ) as alg:
        ds = alg.Output()
        assert ds.RasterCount == 1
        ct = ds.GetRasterBand(1).GetColorTable()
        assert ct.GetColorEntry(0) == (0, 0, 0, 0)
        assert ct.GetColorEntry(1) == (0, 0, 0, 1)


def test_gdalalg_raster_rgb_to_palette_colortable_from_non_existing_file():

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    with pytest.raises(Exception, match="Cannot find"):
        gdal.Run(
            get_alg(), input=src_ds, output_format="MEM", color_map="/i_do/not/exist"
        )


def test_gdalalg_raster_rgb_to_palette_cannot_create_temp_file(tmp_vsimem):

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    with gdaltest.config_options(
        {
            "GDAL_RASTER_PIPELINE_USE_GTIFF_FOR_TEMP_DATASET": "YES",
            "CPL_TMPDIR": "/i_do/not/exist",
        }
    ):
        with pytest.raises(Exception):
            gdal.Run(get_alg(), input=src_ds, output=tmp_vsimem / "out.tif")


def test_gdalalg_raster_nodata(tmp_vsimem):

    src_ds = gdal.Translate(
        tmp_vsimem / "in.tif",
        "../gcore/data/stefan_full_rgba.tif",
        options="-b 1 -b 2 -b 3 -a_nodata 0",
    )

    with gdal.Run(
        get_alg(), input=src_ds, output=tmp_vsimem / "out.tif", dst_nodata=255
    ) as alg:
        ds = alg.Output()
        assert ds.RasterCount == 1
        ct = ds.GetRasterBand(1).GetColorTable()
        assert ct.GetCount() == 256
        assert ct.GetColorEntry(0) == (224, 88, 0, 255)
        assert ct.GetColorEntry(254) == (252, 120, 96, 255)
        assert ct.GetColorEntry(255) == (0, 0, 0, 0)
        assert ds.GetRasterBand(1).Checksum() == 33355
        assert ds.GetRasterBand(1).GetNoDataValue() == 255

    with gdal.Run(
        get_alg(),
        input=src_ds,
        output=tmp_vsimem / "out.tif",
        dst_nodata=0,
        overwrite=True,
    ) as alg:
        ds = alg.Output()
        assert ds.RasterCount == 1
        ct = ds.GetRasterBand(1).GetColorTable()
        assert ct.GetCount() == 256
        assert ct.GetColorEntry(0) == (0, 0, 0, 0)
        assert ct.GetColorEntry(1) == (224, 88, 0, 255)
        assert ct.GetColorEntry(255) == (252, 120, 96, 255)
        assert ds.GetRasterBand(1).Checksum() == 27863
        assert ds.GetRasterBand(1).GetNoDataValue() == 0


def test_gdalalg_raster_rgb_to_palette_no_dither(tmp_vsimem):

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    with gdal.Run(
        get_alg(), input=src_ds, output=tmp_vsimem / "out.tif", no_dither=True
    ) as alg:
        ds = alg.Output()
        assert ds.GetRasterBand(1).Checksum() == 4419


@pytest.mark.slow()
def test_gdalalg_raster_rgb_to_palette_bit_depth_8(tmp_vsimem):

    src_ds = gdal.Open("../gdrivers/data/small_world.tif")
    with gdal.Run(
        get_alg(), input=src_ds, output=tmp_vsimem / "out.tif", bit_depth=8
    ) as alg:
        ds = alg.Output()
        assert ds.GetRasterBand(1).Checksum() == 7593
