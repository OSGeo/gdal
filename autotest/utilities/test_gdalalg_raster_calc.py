#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster calc' testing
# Author:   Daniel Baston
#
###############################################################################
# Copyright (c) 2025, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import re

import gdaltest
import pytest

from osgeo import gdal

gdal.UseExceptions()


@pytest.fixture(scope="module", autouse=True)
def require_muparser():
    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("muparser not available")


@pytest.fixture()
def calc():
    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    return raster.InstantiateSubAlgorithm("calc")


@pytest.mark.parametrize("output_format", ("tif", "vrt"))
def test_gdalalg_raster_calc_basic_1(calc, tmp_vsimem, output_format):

    np = pytest.importorskip("numpy")

    infile = "../gcore/data/rgbsmall.tif"
    outfile = tmp_vsimem / "out.tif"

    calc.GetArg("input").Set([infile])
    calc.GetArg("output").Set(outfile)
    calc.GetArg("calc").Set(["2 + X / (1 + sum(X[1], X[2], X[3]))"])

    assert calc.Run()

    with gdal.Open(infile) as src, gdal.Open(outfile) as dst:
        srcval = src.ReadAsArray().astype("float64")
        expected = np.apply_along_axis(lambda x: 2 + x / (1 + x.sum()), 0, srcval)

        np.testing.assert_array_equal(expected, dst.ReadAsArray())
        assert src.GetGeoTransform() == dst.GetGeoTransform()
        assert src.GetSpatialRef().IsSame(dst.GetSpatialRef())


@pytest.mark.parametrize("output_format", ("tif", "vrt"))
def test_gdalalg_raster_calc_basic_2(calc, tmp_vsimem, output_format):

    np = pytest.importorskip("numpy")

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    calc.GetArg("input").Set([infile])
    calc.GetArg("output").Set(outfile)
    calc.GetArg("calc").Set(["X > 128 ? X + 3 : nan"])

    assert calc.Run()

    with gdal.Open(infile) as src, gdal.Open(outfile) as dst:
        srcval = src.ReadAsArray().astype("float64")
        expected = np.where(srcval > 128, srcval + 3, float("nan"))

        np.testing.assert_array_equal(expected, dst.ReadAsArray())
        assert src.GetGeoTransform() == dst.GetGeoTransform()
        assert src.GetSpatialRef().IsSame(dst.GetSpatialRef())


def test_gdalalg_raster_calc_creation_options(calc, tmp_vsimem):

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    calc.GetArg("input").Set([infile])
    calc.GetArg("output").Set(outfile)
    calc.GetArg("creation-option").Set(["COMPRESS=LZW"])
    calc.GetArg("calc").Set(["X[1] + 3"])

    assert calc.Run()

    with gdal.Open(outfile) as dst:
        assert dst.GetMetadata("IMAGE_STRUCTURE")["COMPRESSION"] == "LZW"


def test_gdalalg_raster_calc_output_format(calc, tmp_vsimem):

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.unknown"

    calc.GetArg("input").Set([infile])
    calc.GetArg("output").Set(outfile)
    calc.GetArg("output-format").Set("GTiff")
    calc.GetArg("calc").Set(["X + 3"])

    assert calc.Run()

    with gdal.Open(outfile) as dst:
        assert dst.GetDriver().GetName() == "GTiff"


def test_gdalalg_raster_calc_overwrite(calc, tmp_vsimem):

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    gdal.CopyFile(infile, outfile)

    calc.GetArg("input").Set([infile])
    calc.GetArg("output").Set(outfile)
    calc.GetArg("calc").Set(["X + 3"])

    with pytest.raises(Exception, match="already exists"):
        assert not calc.Run()

    calc.GetArg("overwrite").Set(True)

    assert calc.Run()


@pytest.mark.parametrize("expr", ("X + 3", "X[1] + 3"))
def test_gdalalg_raster_calc_basic_named_source(calc, tmp_vsimem, expr):

    np = pytest.importorskip("numpy")

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    calc.GetArg("input").Set([f"X={infile}"])
    calc.GetArg("output").Set(outfile)
    calc.GetArg("calc").Set([expr])

    assert calc.Run()

    with gdal.Open(infile) as src, gdal.Open(outfile) as dst:
        np.testing.assert_array_equal(src.ReadAsArray() + 3.0, dst.ReadAsArray())


def test_gdalalg_raster_calc_multiple_calcs(calc, tmp_vsimem):

    np = pytest.importorskip("numpy")

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    calc.GetArg("input").Set([infile])
    calc.GetArg("output").Set(outfile)
    calc.GetArg("calc").Set(["X + 3", "sqrt(X)"])

    assert calc.Run()

    with gdal.Open(infile) as src, gdal.Open(outfile) as dst:
        src_dat = src.ReadAsArray()
        dst_dat = dst.ReadAsArray()

    np.testing.assert_array_equal(src_dat + 3.0, dst_dat[0])
    np.testing.assert_array_equal(np.sqrt(src_dat.astype(np.float64)), dst_dat[1])


@pytest.mark.parametrize(
    "expr",
    (
        "(A+B) / (A - B + 3)",
        "A[2] + B",
    ),
)
def test_gdalalg_raster_calc_multiple_inputs(calc, tmp_vsimem, expr):

    np = pytest.importorskip("numpy")

    # convert 1-based indices to 0-based indices to evaluate expression
    # with numpy
    numpy_expr = expr
    for match in re.findall(r"(?<=\[)\d+(?=])", expr):
        numpy_expr = re.sub(match, str(int(match) - 1), expr, count=1)

    nx = 3
    ny = 5
    nz = 2

    input_1 = tmp_vsimem / "in1.tif"
    input_2 = tmp_vsimem / "in2.tif"
    outfile = tmp_vsimem / "out.tif"

    A = np.arange(nx * ny * nz, dtype=np.float32).reshape(nz, ny, nx)
    B = np.sqrt(A)

    with gdal.GetDriverByName("GTiff").Create(
        input_1, nx, ny, nz, eType=gdal.GDT_Float32
    ) as ds:
        ds.WriteArray(A)

    with gdal.GetDriverByName("GTiff").Create(
        input_2, nx, ny, nz, eType=gdal.GDT_Float32
    ) as ds:
        ds.WriteArray(B)

    calc.GetArg("input").Set([f"A={input_1}", f"B={input_2}"])
    calc.GetArg("output").Set(outfile)
    calc.GetArg("calc").Set([expr])

    assert calc.Run()

    with gdal.Open(outfile) as dst:
        dat = dst.ReadAsArray()
        np.testing.assert_allclose(dat, eval(numpy_expr), rtol=1e-6)


def test_gdalalg_raster_calc_inputs_from_file(calc, tmp_vsimem, tmp_path):

    np = pytest.importorskip("numpy")

    input_1 = tmp_vsimem / "in1.tif"
    input_2 = tmp_vsimem / "in2.tif"
    input_txt = tmp_path / "inputs.txt"
    outfile = tmp_vsimem / "out.tif"

    with gdal.GetDriverByName("GTiff").Create(input_1, 2, 2) as ds:
        ds.GetRasterBand(1).Fill(1)

    with gdal.GetDriverByName("GTIff").Create(input_2, 2, 2) as ds:
        ds.GetRasterBand(1).Fill(2)

    with gdal.VSIFile(input_txt, "w") as txtfile:
        txtfile.write(f"A={input_1}\n")
        txtfile.write(f"B={input_2}\n")

    calc.GetArg("input").Set([f"@{input_txt}"])
    calc.GetArg("output").Set(outfile)
    calc.GetArg("calc").Set(["A + B"])

    assert calc.Run()

    with gdal.Open(outfile) as dst:
        assert np.all(dst.ReadAsArray() == 3)


def test_gdalalg_raster_calc_different_band_counts(calc, tmp_vsimem):

    np = pytest.importorskip("numpy")

    input_1 = tmp_vsimem / "in1.tif"
    input_2 = tmp_vsimem / "in2.tif"
    outfile = tmp_vsimem / "out.tif"

    with gdal.GetDriverByName("GTiff").Create(input_1, 2, 2, 2) as ds:
        ds.GetRasterBand(1).Fill(1)
        ds.GetRasterBand(2).Fill(2)

    with gdal.GetDriverByName("GTIff").Create(input_2, 2, 2, 3) as ds:
        ds.GetRasterBand(1).Fill(3)
        ds.GetRasterBand(2).Fill(4)
        ds.GetRasterBand(3).Fill(5)

    calc.GetArg("input").Set([f"A={input_1}", f"B={input_2}"])
    calc.GetArg("output").Set(outfile)
    calc.GetArg("calc").Set(["A[1] + A[2] + B[1] + B[2] + B[3]"])

    assert calc.Run()

    with gdal.Open(outfile) as dst:
        assert np.all(dst.ReadAsArray() == (1 + 2 + 3 + 4 + 5))


def test_gdalalg_calc_different_resolutions(calc, tmp_vsimem):

    np = pytest.importorskip("numpy")

    xmax = 60
    ymax = 60
    resolutions = [10, 20, 60]

    inputs = [tmp_vsimem / f"in_{i}.tif" for i in range(len(resolutions))]
    outfile = tmp_vsimem / "out.tif"

    for res, fname in zip(resolutions, inputs):
        with gdal.GetDriverByName("GTiff").Create(
            fname, int(xmax / res), int(ymax / res), 1
        ) as ds:
            ds.GetRasterBand(1).Fill(res)
            ds.SetGeoTransform((0, res, 0, ymax, 0, -res))

    calc.GetArg("input").Set([f"A={inputs[0]}", f"B={inputs[1]}", f"C={inputs[2]}"])
    calc.GetArg("calc").Set(["A + B + C"])
    calc.GetArg("output").Set(outfile)

    calc.GetArg("no-check-extent").Set(True)
    with pytest.raises(Exception, match="Inputs do not have the same dimensions"):
        calc.Run()
    calc.GetArg("no-check-extent").Set(False)

    assert calc.Run()

    with gdal.Open(outfile) as ds:
        assert ds.RasterXSize == xmax / min(resolutions)
        assert ds.RasterYSize == ymax / min(resolutions)

        assert np.all(ds.ReadAsArray() == sum(resolutions))


def test_gdalalg_raster_calc_error_extent_mismatch(calc, tmp_vsimem):

    input_1 = tmp_vsimem / "in1.tif"
    input_2 = tmp_vsimem / "in2.tif"
    outfile = tmp_vsimem / "out.tif"

    with gdal.GetDriverByName("GTiff").Create(input_1, 2, 2) as ds:
        ds.SetGeoTransform((0, 1, 0, 2, 0, -1))
    with gdal.GetDriverByName("GTIff").Create(input_2, 2, 2) as ds:
        ds.SetGeoTransform((0, 2, 0, 4, 0, -2))

    calc.GetArg("input").Set([f"A={input_1}", f"B={input_2}"])
    calc.GetArg("output").Set(outfile)
    calc.GetArg("calc").Set(["A+B"])

    with pytest.raises(Exception, match="extents are inconsistent"):
        calc.Run()

    calc.GetArg("no-check-extent").Set(True)
    assert calc.Run()

    with gdal.Open(input_1) as src, gdal.Open(outfile) as dst:
        assert src.GetGeoTransform() == dst.GetGeoTransform()


def test_gdalalg_raster_calc_error_crs_mismatch(calc, tmp_vsimem):

    input_1 = tmp_vsimem / "in1.tif"
    input_2 = tmp_vsimem / "in2.tif"
    outfile = tmp_vsimem / "out.tif"

    with gdal.GetDriverByName("GTiff").Create(input_1, 2, 2) as ds:
        ds.SetProjection("EPSG:4326")
    with gdal.GetDriverByName("GTIff").Create(input_2, 2, 2) as ds:
        ds.SetProjection("EPSG:4269")

    calc.GetArg("input").Set([f"B={input_1}", f"A={input_2}"])
    calc.GetArg("output").Set(outfile)
    calc.GetArg("calc").Set(["A+B"])

    with pytest.raises(Exception, match="spatial reference systems are inconsistent"):
        calc.Run()

    calc.GetArg("no-check-srs").Set(True)
    assert calc.Run()

    with gdal.Open(input_1) as src, gdal.Open(outfile) as dst:
        assert src.GetSpatialRef().IsSame(dst.GetSpatialRef())


@pytest.mark.parametrize("bands", [(2, 3), (2, 4)])
def test_gdalalg_raster_calc_error_band_count_mismatch(calc, tmp_vsimem, bands):

    input_1 = tmp_vsimem / "in1.tif"
    input_2 = tmp_vsimem / "in2.tif"
    outfile = tmp_vsimem / "out.tif"

    gdal.GetDriverByName("GTiff").Create(input_1, 2, 2, bands[0])
    gdal.GetDriverByName("GTIff").Create(input_2, 2, 2, bands[1])

    calc.GetArg("input").Set([f"A={input_1}", f"B={input_2}"])
    calc.GetArg("output").Set(outfile)
    calc.GetArg("calc").Set(["A+B"])

    with pytest.raises(Exception, match="incompatible numbers of bands"):
        calc.Run()

    calc.GetArg("calc").Set(["A+B[1]"])
    assert calc.Run()


@pytest.mark.parametrize(
    "sources",
    [
        ("AX", "A"),
        ("A", "AX"),
        ("XA", "A"),
        ("X", "AX"),
        ("A_X", "A"),
        ("A", "A_X"),
        ("SIN", "S"),
    ],
)
@pytest.mark.parametrize(
    "expr,expected",
    [
        ("SOURCE1 + SOURCE2", ["SOURCE1[1] + SOURCE2[1]", "SOURCE1[1] + SOURCE2[2]"]),
        (
            "SOURCE1* 2 + SOURCE2",
            ["SOURCE1[1]* 2 + SOURCE2[1]", "SOURCE1[1]* 2 + SOURCE2[2]"],
        ),
        ("SOURCE1 + SOURCE2[2]", ["SOURCE1[1] + SOURCE2[2]"]),
        ("SOURCE2 + SOURCE1", ["SOURCE2[1] + SOURCE1[1]", "SOURCE2[2] + SOURCE1[1]"]),
        ("SOURCE2[2] + SOURCE1", ["SOURCE2[2] + SOURCE1[1]"]),
        (
            "SIN(SOURCE1) + SOURCE2",
            ["SIN(SOURCE1[1]) + SOURCE2[1]", "SIN(SOURCE1[1]) + SOURCE2[2]"],
        ),
        (
            "SUM(SOURCE1,SOURCE2)",
            ["SUM(SOURCE1[1],SOURCE2[1])", "SUM(SOURCE1[1],SOURCE2[2])"],
        ),
    ],
)
def test_gdalalg_raster_calc_expression_rewriting(
    calc, tmp_vsimem, sources, expr, expected
):
    import xml.etree.ElementTree as ET

    outfile = tmp_vsimem / "out.vrt"

    inputs = []
    for i, source in enumerate(sources):
        fname = tmp_vsimem / f"{i}.tif"
        with gdal.GetDriverByName("GTiff").Create(
            tmp_vsimem / f"{i}.tif", 2, 2, i + 1
        ) as ds:
            ds.GetRasterBand(1).Fill(i)
        inputs.append(f"{source}={fname}")

        expr = expr.replace(f"SOURCE{i + 1}", source)
        expected = [expr.replace(f"SOURCE{i + 1}", source) for expr in expected]

    calc.GetArg("input").Set(inputs)
    calc.GetArg("output").Set(outfile)
    calc.GetArg("calc").Set([expr])

    assert calc.Run()

    with gdal.VSIFile(outfile, "r") as f:
        root = ET.fromstring(f.read())

    expr = [
        node.attrib["expression"] for node in root.findall(".//PixelFunctionArguments")
    ]
    assert expr == expected
