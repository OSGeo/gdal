#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster compare' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import math
import os

import gdaltest
import pytest

from osgeo import gdal, osr


def test_gdalalg_raster_compare_same_file():

    with gdal.Run(
        "raster",
        "compare",
        input="../gcore/data/byte.tif",
        reference="../gcore/data/byte.tif",
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_raster_compare_progress():

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.Run(
        "raster",
        "compare",
        input="../gcore/data/rgbsmall.tif",
        reference="../gcore/data/rgbsmall.tif",
        skip_binary=True,
        progress=my_progress,
    ) as alg:
        assert alg["output-string"] == ""

    assert tab_pct[0] == 1


def test_gdalalg_raster_compare_progress_interrupted():

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return False

    with pytest.raises(Exception, match="Interrupted by user"):
        gdal.Run(
            "raster",
            "compare",
            input="../gcore/data/byte.tif",
            reference="../gcore/data/byte.tif",
            skip_binary=True,
            progress=my_progress,
        )


def test_gdalalg_raster_compare_pipeline_same_file():

    with gdal.Run(
        "raster",
        "pipeline",
        pipeline="read ../gcore/data/byte.tif ! compare --reference=../gcore/data/byte.tif",
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_raster_compare_pipeline_progress():

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.Run(
        "raster",
        "pipeline",
        pipeline="read ../gcore/data/byte.tif ! compare --reference=../gcore/data/byte.tif --skip-binary",
        progress=my_progress,
    ) as alg:
        assert alg["output-string"] == ""

    assert tab_pct[0] == 1


def test_gdalalg_raster_compare_same_content_but_not_same_binary(tmp_vsimem):

    input_ds = gdal.Translate(tmp_vsimem / "tmp.tif", "../gcore/data/byte.tif")

    with gdal.Run(
        "raster", "compare", input=input_ds, reference="../gcore/data/byte.tif"
    ) as alg:
        ret = alg["output-string"].split("\n")[:-1]
        assert len(ret) == 1
        assert ret[0].startswith(
            "Reference file has size 736 bytes, whereas input file has size"
        )

    with gdal.Run(
        "raster",
        "pipeline",
        pipeline=f"read {tmp_vsimem}/tmp.tif ! compare --reference=../gcore/data/byte.tif",
    ) as alg:
        assert alg["output-string"].startswith(
            "Reference file has size 736 bytes, whereas input file has size"
        )

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference="../gcore/data/byte.tif",
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference="../gcore/data/byte.tif",
        skip_all_optional=True,
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_raster_compare_binary_comparison(tmp_vsimem):

    input_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    ref_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Reference dataset has no name. Skipping binary file comparison",
    ):
        with gdal.Run(
            "raster",
            "compare",
            input="../gcore/data/byte.tif",
            reference=ref_ds,
        ) as alg:
            assert alg["output-string"] == ""

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Input dataset has no name. Skipping binary file comparison",
    ):
        with gdal.Run(
            "raster",
            "compare",
            input=input_ds,
            reference="../gcore/data/byte.tif",
        ) as alg:
            assert alg["output-string"] == ""

    ref_ds = gdal.Translate("named_but_mem", "../gcore/data/byte.tif", format="MEM")

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Reference dataset is a in-memory dataset. Skipping binary file comparison",
    ):
        with gdal.Run(
            "raster",
            "compare",
            input="../gcore/data/byte.tif",
            reference=ref_ds,
        ) as alg:
            assert alg["output-string"] == ""

    input_ds = gdal.Translate("named_but_mem", "../gcore/data/byte.tif", format="MEM")

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Input dataset is a in-memory dataset. Skipping binary file comparison",
    ):
        with gdal.Run(
            "raster",
            "compare",
            input=input_ds,
            reference="../gcore/data/byte.tif",
        ) as alg:
            assert alg["output-string"] == ""

    input_ds = gdal.Translate(tmp_vsimem / "input.tif", "../gcore/data/byte.tif")
    ref_ds = gdal.Translate(tmp_vsimem / "ref.tif", "../gcore/data/byte.tif")
    gdal.Unlink(tmp_vsimem / "input.tif")

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Input dataset '/vsimem/test_gdalalg_raster_compare_binary_comparison/input.tif' is not a file. Skipping binary file comparison",
    ):
        with gdal.Run(
            "raster",
            "compare",
            input=input_ds,
            reference=ref_ds,
        ) as alg:
            assert alg["output-string"] == ""

    input_ds = gdal.Translate(tmp_vsimem / "input.tif", "../gcore/data/byte.tif")
    ref_ds = gdal.Translate(tmp_vsimem / "ref.tif", "../gcore/data/byte.tif")
    gdal.Unlink(tmp_vsimem / "ref.tif")

    with gdaltest.error_raised(
        gdal.CE_Warning,
        match="Reference dataset '/vsimem/test_gdalalg_raster_compare_binary_comparison/ref.tif' is not a file. Skipping binary file comparison",
    ):
        with gdal.Run(
            "raster",
            "compare",
            input=input_ds,
            reference=ref_ds,
        ) as alg:
            assert alg["output-string"] == ""

    gdal.Translate(tmp_vsimem / "input.tif", "../gcore/data/byte.tif")
    gdal.Translate(tmp_vsimem / "ref.tif", "../gcore/data/byte.tif")

    with gdal.VSIFile(tmp_vsimem / "input.tif", "r+b") as f:
        f.seek(0, os.SEEK_END)
        pos = f.tell()
        f.seek(pos - 1, os.SEEK_SET)
        f.write(b"\xff")

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference file and input file differ at the binary level.\n"
        )


def test_gdalalg_raster_compare_crs():

    input_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    input_ds.SetSpatialRef(None)
    ref_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    ref_ds.SetSpatialRef(None)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""

    input_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    input_ds.SetSpatialRef(None)
    ref_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_crs=True,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_all_optional=True,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference dataset has a CRS, but input dataset has none.\n"
        )

    input_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    ref_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    ref_ds.SetSpatialRef(None)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference dataset has no CRS, but input dataset has one.\n"
        )

    input_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    input_ds.SetSpatialRef(osr.SpatialReference(epsg=4326))
    ref_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        ret = alg["output-string"].split("\n")[:-1]
        assert len(ret) == 1
        assert ret[0].startswith("Reference and input CRS are not equivalent.")


def test_gdalalg_raster_compare_geotransform():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""

    input_ds.SetGeoTransform([1, 2, 3, 4, 5, 6])

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_geotransform=True,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_all_optional=True,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference dataset has no geotransform, but input one has one.\n"
        )

    with gdal.Run(
        "raster",
        "compare",
        input=ref_ds,
        reference=input_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference dataset has a geotransform, but input one has none.\n"
        )

    ref_ds.SetGeoTransform([1, 2, 3, 4, 5, -6])

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Geotransform of reference and input dataset are not equivalent. Reference geotransform is (1.000000,2.000000,3.000000,4.000000,5.000000,-6.000000). Input geotransform is (1.000000,2.000000,3.000000,4.000000,5.000000,6.000000)\n"
        )


@pytest.mark.parametrize(
    "dt,stype,v1,v2",
    [
        (gdal.GDT_UInt8, "B", 255, 255),
        (gdal.GDT_UInt8, "B", 255, 0),
        (gdal.GDT_UInt8, "B", 0, 255),
        (gdal.GDT_Int8, "b", 127, 127),
        (gdal.GDT_Int8, "b", -128, -128),
        (gdal.GDT_Int8, "b", 127, -128),
        (gdal.GDT_Int8, "b", -128, 127),
        (gdal.GDT_UInt16, "H", 65535, 65535),
        (gdal.GDT_UInt16, "H", 65535, 0),
        (gdal.GDT_UInt16, "H", 0, 65535),
        (gdal.GDT_Int16, "h", 32767, 32767),
        (gdal.GDT_Int16, "h", -32768, -32768),
        (gdal.GDT_Int16, "h", 32767, -32768),
        (gdal.GDT_Int16, "h", -32768, 32767),
        (gdal.GDT_UInt32, "I", (1 << 32) - 1, (1 << 32) - 1),
        (gdal.GDT_UInt32, "I", (1 << 32) - 1, 0),
        (gdal.GDT_UInt32, "I", 0, (1 << 32) - 1),
        (gdal.GDT_Int32, "i", (1 << 31) - 1, (1 << 31) - 1),
        (gdal.GDT_Int32, "i", -(1 << 31), -(1 << 31)),
        (gdal.GDT_Int32, "i", (1 << 31) - 1, -(1 << 31)),
        (gdal.GDT_Int32, "i", -(1 << 31), (1 << 31) - 1),
        (gdal.GDT_UInt64, "Q", (1 << 64) - 1, (1 << 64) - 1),
        (gdal.GDT_UInt64, "Q", (1 << 64) - 1, 0),
        (gdal.GDT_UInt64, "Q", 0, (1 << 64) - 1),
        (gdal.GDT_Int64, "q", (1 << 63) - 1, (1 << 63) - 1),
        (gdal.GDT_Int64, "q", -(1 << 63), -(1 << 63)),
        (gdal.GDT_Int64, "q", (1 << 63) - 1, -(1 << 63)),
        (gdal.GDT_Int64, "q", -(1 << 63), (1 << 63) - 1),
        (gdal.GDT_Float16, "f", 1.5, 1.5),
        (gdal.GDT_Float16, "f", float("nan"), float("nan")),
        (gdal.GDT_Float16, "f", float("nan"), 0),
        (gdal.GDT_Float16, "f", 0, float("nan")),
        (gdal.GDT_Float16, "f", float("inf"), float("inf")),
        (gdal.GDT_Float16, "f", float("-inf"), float("-inf")),
        (gdal.GDT_Float16, "f", -1.5, 0),
        (gdal.GDT_Float16, "f", 0, -1.5),
        (gdal.GDT_Float32, "f", 1.5, 1.5),
        (gdal.GDT_Float32, "f", float("nan"), float("nan")),
        (gdal.GDT_Float32, "f", float("inf"), float("inf")),
        (gdal.GDT_Float32, "f", float("-inf"), float("-inf")),
        (gdal.GDT_Float32, "f", -1.5, 0),
        (gdal.GDT_Float32, "f", 0, -1.5),
        (gdal.GDT_Float64, "d", 1.5, 1.5),
        (gdal.GDT_Float64, "d", float("nan"), float("nan")),
        (gdal.GDT_Float64, "d", float("inf"), float("inf")),
        (gdal.GDT_Float64, "d", float("-inf"), float("-inf")),
        (gdal.GDT_Float64, "d", -1.5, 0),
        (gdal.GDT_Float64, "d", 0, -1.5),
        (
            gdal.GDT_CInt16,
            "h",
            [(1 << 15) - 1, -(1 << 15)],
            [(1 << 15) - 1, -(1 << 15)],
        ),
        (gdal.GDT_CInt16, "h", [(1 << 15) - 1, -(1 << 15)], [0, 0]),
        (
            gdal.GDT_CInt32,
            "i",
            [(1 << 31) - 1, -(1 << 31)],
            [(1 << 31) - 1, -(1 << 31)],
        ),
        (gdal.GDT_CInt32, "i", [(1 << 31) - 1, -(1 << 31)], [0, 0]),
        (
            gdal.GDT_CFloat16,
            "f",
            [float("nan"), float("nan")],
            [float("nan"), float("nan")],
        ),
        (gdal.GDT_CFloat16, "f", [1.5, -2.5], [1.5, -2.5]),
        (gdal.GDT_CFloat16, "f", [1.5, -2.5], [0, 0]),
        (gdal.GDT_CFloat32, "f", [1.5, -2.5], [1.5, -2.5]),
        (gdal.GDT_CFloat32, "f", [1.5, -2.5], [0, 0]),
        (gdal.GDT_CFloat64, "d", [1.5, -2.5], [1.5, -2.5]),
        (gdal.GDT_CFloat64, "d", [1.5, -2.5], [0, 0]),
    ],
)
@pytest.mark.parametrize("band_interleaved", [False, True])
def test_gdalalg_raster_compare_pixel(dt, stype, v1, v2, band_interleaved):

    buf_type = dt
    if dt == gdal.GDT_Float16:
        buf_type = gdal.GDT_Float32
    elif dt == gdal.GDT_CFloat16:
        buf_type = gdal.GDT_CFloat32

    nbands = 1 if band_interleaved else 16
    options = [] if band_interleaved else ["INTERLEAVE=PIXEL"]

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, nbands, dt, options=options)
    input_ds.GetRasterBand(nbands).WriteRaster(
        0,
        0,
        1,
        1,
        bytes(array.array(stype, v1 if isinstance(v1, list) else [v1])),
        buf_type=buf_type,
    )
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, nbands, dt, options=options)
    ref_ds.GetRasterBand(nbands).WriteRaster(
        0,
        0,
        1,
        1,
        bytes(array.array(stype, v2 if isinstance(v2, list) else [v2])),
        buf_type=buf_type,
    )

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        if v1 == v2 or (not isinstance(v1, list) and math.isnan(v1) and math.isnan(v2)):
            assert alg["output-string"] == ""
        elif not isinstance(v1, list) and math.isnan(v1) and not math.isnan(v2):
            assert alg["output-string"] != ""
        elif not isinstance(v1, list) and not math.isnan(v1) and math.isnan(v2):
            assert alg["output-string"] != ""
        elif isinstance(v1, list):
            if (
                math.isnan(v1[0])
                and math.isnan(v1[1])
                and math.isnan(v2[0])
                and math.isnan(v2[1])
            ):
                assert alg["output-string"] == ""
            else:
                assert (
                    str(math.hypot(v1[0] - v2[0], v1[1], v2[1]))[0:7]
                    in alg["output-string"]
                )
        else:
            assert str(abs(v1 - v2)) in alg["output-string"]


def test_gdalalg_raster_compare_float_a_one_b_one():

    ds_a = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_a.GetRasterBand(1).Fill(1)
    ds_b = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_b.GetRasterBand(1).Fill(1)

    with gdal.Run(
        "raster",
        "compare",
        input=ds_a,
        reference=ds_b,
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_raster_compare_float_a_one_b_two():

    ds_a = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_a.GetRasterBand(1).Fill(1)
    ds_b = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_b.GetRasterBand(1).Fill(2)

    with gdal.Run(
        "raster",
        "compare",
        input=ds_a,
        reference=ds_b,
        skip_binary=True,
    ) as alg:
        assert "pixels differing: 17" in alg["output-string"]
        assert "maximum pixel value difference: 1.0" in alg["output-string"]

    with gdal.Run(
        "raster",
        "compare",
        input=ds_b,
        reference=ds_a,
        skip_binary=True,
    ) as alg:
        assert "pixels differing: 17" in alg["output-string"]
        assert "maximum pixel value difference: 1.0" in alg["output-string"]


def test_gdalalg_raster_compare_float_a_nan_b_one():

    ds_a = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_a.GetRasterBand(1).Fill(float("nan"))
    ds_b = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_b.GetRasterBand(1).Fill(1)

    with gdal.Run(
        "raster",
        "compare",
        input=ds_a,
        reference=ds_b,
        skip_binary=True,
    ) as alg:
        assert "pixels differing: 17" in alg["output-string"]
        assert "maximum pixel value difference: 0.0" in alg["output-string"]

    with gdal.Run(
        "raster",
        "compare",
        input=ds_b,
        reference=ds_a,
        skip_binary=True,
    ) as alg:
        assert "pixels differing: 17" in alg["output-string"]
        assert "maximum pixel value difference: 0.0" in alg["output-string"]


@pytest.mark.parametrize("idx", [i for i in range(5)])
def test_gdalalg_raster_compare_float_a_nan_b_one_but_at_one_index(idx):

    ds_a = gdal.GetDriverByName("MEM").Create("", 5, 1, 1, gdal.GDT_Float32)
    ds_a.GetRasterBand(1).Fill(float("nan"))
    ds_a.GetRasterBand(1).WriteRaster(idx, 0, 1, 1, b"\x00\x00\x00\x00")
    ds_b = gdal.GetDriverByName("MEM").Create("", 5, 1, 1, gdal.GDT_Float32)
    ds_b.GetRasterBand(1).Fill(1)
    ds_b.GetRasterBand(1).WriteRaster(idx, 0, 1, 1, b"\x00\x00\x00\x00")

    with gdal.Run(
        "raster",
        "compare",
        input=ds_a,
        reference=ds_b,
        skip_binary=True,
    ) as alg:
        assert "pixels differing: 4" in alg["output-string"]
        assert "maximum pixel value difference: 0.0" in alg["output-string"]

    with gdal.Run(
        "raster",
        "compare",
        input=ds_b,
        reference=ds_a,
        skip_binary=True,
    ) as alg:
        assert "pixels differing: 4" in alg["output-string"]
        assert "maximum pixel value difference: 0.0" in alg["output-string"]


def test_gdalalg_raster_compare_float_a_nan_b_minus_nan():

    ds_a = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_a.GetRasterBand(1).Fill(float("nan"))
    ds_b = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_b.GetRasterBand(1).Fill(-float("nan"))

    with gdal.Run(
        "raster",
        "compare",
        input=ds_a,
        reference=ds_b,
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_raster_compare_float_zero_and_minus_zero():

    ds_a = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_a.GetRasterBand(1).Fill(0)
    ds_b = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_b.GetRasterBand(1).Fill(-0)

    with gdal.Run(
        "raster",
        "compare",
        input=ds_a,
        reference=ds_b,
        skip_binary=True,
    ) as alg:
        alg["output-string"] == ""


def test_gdalalg_raster_compare_float_inf():

    ds_a = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_a.GetRasterBand(1).Fill(float("inf"))
    ds_b = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_b.GetRasterBand(1).Fill(float("inf"))

    with gdal.Run(
        "raster",
        "compare",
        input=ds_a,
        reference=ds_b,
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_raster_compare_float_inf_and_minus_inf():

    ds_a = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_a.GetRasterBand(1).Fill(float("inf"))
    ds_b = gdal.GetDriverByName("MEM").Create("", 17, 1, 1, gdal.GDT_Float32)
    ds_b.GetRasterBand(1).Fill(float("-inf"))

    with gdal.Run(
        "raster",
        "compare",
        input=ds_a,
        reference=ds_b,
        skip_binary=True,
    ) as alg:
        assert "pixels differing: 17" in alg["output-string"]
        assert "maximum pixel value difference: inf" in alg["output-string"]

    with gdal.Run(
        "raster",
        "compare",
        input=ds_b,
        reference=ds_a,
        skip_binary=True,
    ) as alg:
        assert "pixels differing: 17" in alg["output-string"]
        assert "maximum pixel value difference: inf" in alg["output-string"]


def test_gdalalg_raster_compare_pixel_interleaved_progress():

    ds = gdal.GetDriverByName("MEM").Create(
        "", 1, 1, 16, gdal.GDT_UInt8, options=["INTERLEAVE=PIXEL"]
    )

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    assert gdal.Run(
        "raster",
        "compare",
        input=ds,
        reference=ds,
        skip_binary=True,
        progress=my_progress,
    )

    assert tab_pct[0] == 1


def test_gdalalg_raster_compare_pixel_interleaved_progress_interrupted():

    ds = gdal.GetDriverByName("MEM").Create(
        "", 1, 1, 16, gdal.GDT_UInt8, options=["INTERLEAVE=PIXEL"]
    )

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return False

    with pytest.raises(Exception, match="Interrupted by user"):
        gdal.Run(
            "raster",
            "compare",
            input=ds,
            reference=ds,
            skip_binary=True,
            progress=my_progress,
        )


def test_gdalalg_raster_compare_band_count():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference dataset has 2 band(s), but input dataset has 1\n"
        )


def test_gdalalg_raster_compare_width():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 1)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference dataset width is 2, but input dataset width is 1\n"
        )


def test_gdalalg_raster_compare_height():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 2, 1)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference dataset height is 2, but input dataset height is 1\n"
        )


def test_gdalalg_raster_compare_type():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_UInt8)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Int16)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference band 1 has data type Int16, but input band has data type Byte\n"
        )


def test_gdalalg_raster_compare_band_description():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    input_ds.GetRasterBand(1).SetDescription("foo")
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds.GetRasterBand(1).SetDescription("bar")

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference band 1 has description bar, but input band has description foo\n"
        )


def test_gdalalg_raster_compare_nodata():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    input_ds.GetRasterBand(1).SetNoDataValue(0)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference band 1 has no nodata value, but input band has no data value 0.000000.\nReference band 1 has mask flags = 1 , but input band has mask flags = 8\n"
        )

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds.GetRasterBand(1).SetNoDataValue(0)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference band 1 has nodata value 0.000000, but input band has none.\nReference band 1 has mask flags = 8 , but input band has mask flags = 1\n"
        )

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    input_ds.GetRasterBand(1).SetNoDataValue(0)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds.GetRasterBand(1).SetNoDataValue(1)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference band 1 has nodata value 1.000000, but input band has no data value 0.000000.\n"
        )

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    input_ds.GetRasterBand(1).SetNoDataValue(1)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds.GetRasterBand(1).SetNoDataValue(1)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float32)
    input_ds.GetRasterBand(1).SetNoDataValue(float("nan"))
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float32)
    ref_ds.GetRasterBand(1).SetNoDataValue(float("nan"))

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float32)
    input_ds.GetRasterBand(1).SetNoDataValue(float("nan"))
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float32)
    ref_ds.GetRasterBand(1).SetNoDataValue(1)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] != ""


def test_gdalalg_raster_compare_color_interpretation():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    input_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    input_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_GreenBand)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference band 1 has color interpretation Green, but input band has color interpretation Red\n"
        )


def test_gdalalg_raster_compare_mask():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    input_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds.CreateMaskBand(gdal.GMF_PER_DATASET)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""

    ref_ds.GetRasterBand(1).GetMaskBand().Fill(1)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "mask of band 1: pixels differing: 1\nmask of band 1: maximum pixel value difference: 1\n"
        )


def test_gdalalg_raster_compare_color_overviews():

    input_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 1)
    input_ds.BuildOverviews("NEAR", [2])
    ref_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 1)
    ref_ds.BuildOverviews("NEAR", [2])

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""

    input_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 1)
    input_ds.BuildOverviews("NEAR", [2])
    ref_ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 1)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference band 1 has 0 overview band(s), but input band has 1\n"
        )

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_overview=True,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_all_optional=True,
    ) as alg:
        assert alg["output-string"] == ""

    input_ds = gdal.GetDriverByName("MEM").Create("", 4, 4, 1)
    input_ds.BuildOverviews("NEAR", [4])
    ref_ds = gdal.GetDriverByName("MEM").Create("", 4, 4, 1)
    ref_ds.BuildOverviews("NEAR", [2])

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference band width is 2, but input band width is 1\nReference band height is 2, but input band height is 1\n"
        )


def test_gdalalg_raster_compare_metadata():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)

    input_ds.SetMetadata(
        {"ERR_BIAS": "foo", "one": "1", "two": "2", "three": "3", "NITF_FDT": "foo"}
    )
    ref_ds.SetMetadata(
        {
            "ERR_RAND": "foo",
            "two": "2",
            "three": "three",
            "four": "4",
            "NITF_FDT": "bar",
        }
    )

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference metadata (dataset default metadata domain) contains key 'four' but input metadata does not.\nReference metadata (dataset default metadata domain) has value '3' for key 'three' but input metadata has value 'three'.\nInput metadata (dataset default metadata domain) contains key 'one' but reference metadata does not.\n"
        )

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_metadata=True,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_all_optional=True,
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_raster_compare_rpc():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)

    # RPC metadata comparison strips leading and trailing spaces
    input_ds.SetMetadata({"foo": "bar", "bar": "baz"}, "RPC")
    ref_ds.SetMetadata({"foo": " bar ", "bar": "baw"}, "RPC")

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference metadata RPC has value 'baz' for key 'bar' but input metadata has value 'baw'.\n"
        )

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_rpc=True,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_all_optional=True,
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_raster_compare_geolocation():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)
    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1)

    input_ds.SetMetadata({"foo": "bar"}, "GEOLOCATION")
    ref_ds.SetMetadata({"bar": "baz"}, "GEOLOCATION")

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference metadata GEOLOCATION contains key 'bar' but input metadata does not.\nInput metadata GEOLOCATION contains key 'foo' but reference metadata does not.\n"
        )

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_geolocation=True,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_all_optional=True,
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_raster_compare_subdataset():

    input_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 0)
    input_ds.SetMetadataItem("SUBDATASET_1_DESC", "FOO", "SUBDATASETS")
    input_ds.SetMetadataItem(
        "SUBDATASET_1_NAME", "../gcore/data/byte.tif", "SUBDATASETS"
    )

    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 0)

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference dataset has 0 subdataset(s) whereas input dataset has 1 one(s).\n"
        )

    ref_ds.SetMetadataItem("SUBDATASET_1_DESC", "FOO", "SUBDATASETS")
    ref_ds.SetMetadataItem("SUBDATASET_1_NAME", "../gcore/data/byte.tif", "SUBDATASETS")

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""

    ref_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 0)
    ref_ds.SetMetadataItem("SUBDATASET_1_DESC", "FOO", "SUBDATASETS")
    ref_ds.SetMetadataItem(
        "SUBDATASET_1_NAME", "../gcore/data/uint16.tif", "SUBDATASETS"
    )

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference band 1 has data type UInt16, but input band has data type Byte\n"
        )

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_subdataset=True,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.Run(
        "raster",
        "compare",
        input=input_ds,
        reference=ref_ds,
        skip_binary=True,
        skip_all_optional=True,
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_raster_compare_subdataset_progress():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 0)
    ds.SetMetadataItem("SUBDATASET_1_DESC", "FOO", "SUBDATASETS")
    ds.SetMetadataItem("SUBDATASET_1_NAME", "../gcore/data/byte.tif", "SUBDATASETS")

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.Run(
        "raster",
        "compare",
        input=ds,
        reference=ds,
        skip_binary=True,
        progress=my_progress,
    ) as alg:
        assert alg["output-string"] == ""

    assert tab_pct[0] == 1.0


def test_gdalalg_raster_compare_same_file_pipeline():

    with gdal.alg.raster.pipeline(
        input="../gcore/data/byte.tif",
        pipeline="read ! compare --reference ../gcore/data/byte.tif",
    ) as alg:
        assert alg["output-string"] == ""
