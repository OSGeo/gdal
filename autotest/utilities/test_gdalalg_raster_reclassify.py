#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster reclassify' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2025, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal


@pytest.fixture()
def reclassify():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("reclassify")


@pytest.mark.parametrize("output_format", ("tif", "vrt"))
@pytest.mark.parametrize("mapping_format", ("file", "text"))
def test_gdalalg_raster_reclassify_basic_1(
    reclassify, tmp_vsimem, mapping_format, output_format
):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    if output_format == "vrt":
        src = gdal.Open("../gcore/data/nodata_byte.tif")
    else:
        src = gdal.Translate("", "../gcore/data/nodata_byte.tif", format="MEM")
    outfile = tmp_vsimem / f"out.{output_format}"

    reclassify["input"] = src
    reclassify["output"] = outfile

    if mapping_format == "text":
        reclassify["mapping"] = (
            "165 = 120; (-inf, 0) = 140; (0, 100] = 140; (100,  130] = PASS_THROUGH; DEFAULT = 160; NO_DATA = NO_DATA"
        )
    else:
        gdal.FileFromMemBuffer(
            tmp_vsimem / "mapping.txt",
            """
           # A sample reclassification
           165         = 120
           (-inf, 0)   = 140 # Match everything <= 100
           (0, 100]    = 140 # manually exclude NoData value of 0
           (100, 130]  = PASS_THROUGH
           DEFAULT     = 160
           NO_DATA     = NO_DATA

        """,
        )
        reclassify["mapping"] = f"@{tmp_vsimem}/mapping.txt"

    assert reclassify.Run()
    assert reclassify.Finalize()

    with gdal.Open(outfile) as dst:
        assert src.GetGeoTransform() == dst.GetGeoTransform()
        assert src.GetSpatialRef().IsSame(dst.GetSpatialRef())
        assert (
            src.GetRasterBand(1).GetNoDataValue()
            == dst.GetRasterBand(1).GetNoDataValue()
        )
        assert src.GetRasterBand(1).DataType == dst.GetRasterBand(1).DataType

        src_val = src.ReadAsMaskedArray()
        dst_val = dst.ReadAsMaskedArray()

        # Constant mapped to constant (165 = 120)
        src_165 = np.ma.where(src_val == 165)
        assert np.all(dst_val[src_165] == 120)

        # NoData passed through (NO_DATA = NO_DATA)
        np.testing.assert_array_equal(src_val.mask, dst_val.mask)

        # Range mapped to constant ((-inf, 100] = 140)
        src_lt_140 = np.ma.where(src_val <= 100)
        assert np.all(dst_val[src_lt_140] == 140)

        # Range passed through
        src_100_to_130 = np.ma.where((src_val > 100) & (src_val <= 130))
        np.testing.assert_array_equal(src_val[src_100_to_130], dst_val[src_100_to_130])

        # Everything else mapped to constant (DEFAULT = 160)
        dst_val.mask[src_165] = True
        dst_val.mask[src_lt_140] = True
        dst_val.mask[src_100_to_130] = True

        assert np.all(dst_val == 160)


def test_gdalalg_raster_reclassify_output_type(reclassify, tmp_vsimem):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    infile = "../gcore/data/float32.tif"
    outfile = tmp_vsimem / "out.tif"

    reclassify["input"] = infile
    reclassify["output"] = outfile
    reclassify["mapping"] = "(-inf, 132)=0; [132, inf)=1"
    reclassify["output-data-type"] = "Int16"

    assert reclassify.Run()
    assert reclassify.Finalize()

    with gdal.Open(infile) as src, gdal.Open(outfile) as dst:
        assert dst.GetRasterBand(1).DataType == gdal.GDT_Int16

        src_val = src.ReadAsArray()
        dst_val = dst.ReadAsArray()

        assert np.all(dst_val[np.where(src_val < 132)] == 0)
        assert np.all(dst_val[np.where(src_val >= 132)] == 1)


def test_gdalalg_raster_reclassify_source_transfer_type(reclassify, tmp_vsimem):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    src_ds = gdal.GetDriverByName("MEM").Create("", 5, 5, eType=gdal.GDT_Int32)
    src_ds.GetRasterBand(1).Fill(500)

    reclassify["input"] = src_ds
    reclassify["output"] = ""
    reclassify["output-format"] = "MEM"
    reclassify["mapping"] = "[-inf, 500)=3; [500, inf]=7"
    reclassify["output-data-type"] = "Byte"

    assert reclassify.Run()

    assert np.all(reclassify["output"].GetDataset().ReadAsArray() == 7)

    assert reclassify.Finalize()


def test_gdalalg_raster_reclassify_many_ranges(reclassify, tmp_vsimem):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    nx = 100
    ny = 100
    nbins = 1000

    np.random.seed(802)
    data = np.random.rand(ny, nx) * 1000

    src_ds = gdal.GetDriverByName("MEM").Create("", nx, ny, 1, eType=gdal.GDT_Float64)
    src_ds.WriteArray(data)
    rast_min, rast_max = src_ds.GetRasterBand(1).ComputeRasterMinMax(False)

    breaks = list(np.linspace(rast_min, rast_max, nbins - 1))

    bins = {}
    i = 0
    for a, b in zip(breaks, breaks[1:] + [float("inf")]):
        bins[i] = (a, b)
        i += 1

    with gdal.VSIFile(tmp_vsimem / "bins.txt", "w") as out:
        for i, minmax in bins.items():
            a, b = minmax
            out.write(f"[{a}, {b}) = {i}\n")

    reclassify["input"] = src_ds
    reclassify["output"] = ""
    reclassify["mapping"] = f"@{tmp_vsimem}/bins.txt"
    reclassify["output-format"] = "MEM"

    assert reclassify.Run()

    results = reclassify["output"].GetDataset().ReadAsArray()

    assert reclassify.Finalize()

    # check that all values reclassified correctly
    for i in range(ny):
        for j in range(nx):
            a, b = bins[results[i, j]]
            assert data[i, j] >= a and data[i, j] < b


def test_gdalalg_raster_reclassify_multiple_bands(reclassify, tmp_vsimem):

    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    infile = "../gcore/data/rgbsmall.tif"
    outfile = tmp_vsimem / "out.tif"

    reclassify["input"] = infile
    reclassify["output"] = outfile
    reclassify["mapping"] = "(-inf, 128)=0; [128, inf)=1"

    assert reclassify.Run()
    assert reclassify.Finalize()

    with gdal.Open(infile) as src, gdal.Open(outfile) as dst:
        assert src.RasterCount == dst.RasterCount

        for i in range(src.RasterCount):
            assert (
                src.GetRasterBand(i + 1).DataType == dst.GetRasterBand(i + 1).DataType
            )

            src_val = src.GetRasterBand(i + 1).ReadAsArray()
            dst_val = dst.GetRasterBand(i + 1).ReadAsArray()

            assert np.all(dst_val[np.where(src_val < 128)] == 0)
            assert np.all(dst_val[np.where(src_val >= 128)] == 1)


def test_gdalalg_raster_reclassify_empty_mapping(reclassify, tmp_vsimem):

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    reclassify["input"] = infile
    reclassify["output"] = outfile
    reclassify["mapping"] = ""

    with pytest.raises(
        RuntimeError, match="Encountered value .* with no specified mapping"
    ):
        reclassify.Run()


def test_gdalalg_raster_reclassify_invalid_mapping_vrt_output(reclassify, tmp_vsimem):

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.vrt"

    reclassify["input"] = infile
    reclassify["output"] = outfile
    reclassify["mapping"] = "invalid"

    with pytest.raises(RuntimeError, match="Interval must start with"):
        reclassify.Run()


def test_gdalalg_raster_reclassify_mapping_not_found(reclassify, tmp_vsimem):

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    reclassify["input"] = infile
    reclassify["output"] = outfile
    reclassify["mapping"] = f"@{tmp_vsimem}/i_do_not_exist.txt"

    with pytest.raises(RuntimeError, match="Cannot open .*i_do_not_exist.txt"):
        reclassify.Run()


def test_gdalalg_raster_reclassify_mapping_not_provided(reclassify, tmp_vsimem):

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    reclassify["input"] = infile
    reclassify["output"] = outfile

    with pytest.raises(
        RuntimeError, match="Required argument 'mapping' has not been specified"
    ):
        reclassify.Run()


def test_gdalalg_raster_reclassify_bad_output_type(reclassify, tmp_vsimem):

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    reclassify["input"] = infile
    reclassify["output"] = outfile

    with pytest.raises(RuntimeError, match="Invalid value .*output-data-type"):
        reclassify["output-data-type"] = "Float128"


def test_gdalalg_raster_reclassify_too_large_line(reclassify, tmp_vsimem):

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    gdal.FileFromMemBuffer(tmp_vsimem / "mapping.txt", "x" * (1000 * 1000 + 1))

    reclassify["input"] = infile
    reclassify["output"] = outfile
    reclassify["mapping"] = f"@{tmp_vsimem}/mapping.txt"

    with pytest.raises(
        RuntimeError, match="Maximum number of characters allowed reached"
    ):
        reclassify.Run()


def test_gdalalg_raster_reclassify_too_large_mapping_file(reclassify, tmp_vsimem):

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    gdal.FileFromMemBuffer(
        tmp_vsimem / "mapping.txt",
        "\n".join(["x" * (1000 * 1000 - 1) for i in range(10)]) + "\n" + "x" * 10,
    )

    reclassify["input"] = infile
    reclassify["output"] = outfile
    reclassify["mapping"] = f"@{tmp_vsimem}/mapping.txt"

    with pytest.raises(RuntimeError, match="Too large mapping size"):
        reclassify.Run()
