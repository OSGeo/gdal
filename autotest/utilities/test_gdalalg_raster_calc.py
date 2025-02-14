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

    calc["input"] = [infile]
    calc["output"] = outfile
    calc["calc"] = ["2 + X / (1 + sum(X[1], X[2], X[3]))"]

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

    calc["input"] = [infile]
    calc["output"] = outfile
    calc["calc"] = ["X > 128 ? X + 3 : nan"]

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

    calc["input"] = [infile]
    calc["output"] = outfile
    calc["creation-option"] = ["COMPRESS=LZW"]
    calc["calc"] = ["X[1] + 3"]

    assert calc.Run()

    with gdal.Open(outfile) as dst:
        assert dst.GetMetadata("IMAGE_STRUCTURE")["COMPRESSION"] == "LZW"


def test_gdalalg_raster_calc_output_format(calc, tmp_vsimem):

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.unknown"

    calc["input"] = [infile]
    calc["output"] = outfile
    calc["output-format"] = "GTiff"
    calc["calc"] = ["X + 3"]

    assert calc.Run()

    with gdal.Open(outfile) as dst:
        assert dst.GetDriver().GetName() == "GTiff"


def test_gdalalg_raster_calc_overwrite(calc, tmp_vsimem):

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    gdal.CopyFile(infile, outfile)

    calc["input"] = [infile]
    calc["output"] = outfile
    calc["calc"] = ["X + 3"]

    with pytest.raises(Exception, match="already exists"):
        assert not calc.Run()

    calc["overwrite"] = True

    assert calc.Run()


@pytest.mark.parametrize("expr", ("X + 3", "X[1] + 3"))
def test_gdalalg_raster_calc_basic_named_source(calc, tmp_vsimem, expr):

    np = pytest.importorskip("numpy")

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    calc["input"] = [f"X={infile}"]
    calc["output"] = outfile
    calc["calc"] = [expr]

    assert calc.Run()

    with gdal.Open(infile) as src, gdal.Open(outfile) as dst:
        np.testing.assert_array_equal(src.ReadAsArray() + 3.0, dst.ReadAsArray())


def test_gdalalg_raster_calc_multiple_calcs(calc, tmp_vsimem):

    np = pytest.importorskip("numpy")

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    calc["input"] = [infile]
    calc["output"] = outfile
    calc["calc"] = ["X + 3", "sqrt(X)"]

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

    calc["input"] = [f"A={input_1}", f"B={input_2}"]
    calc["output"] = outfile
    calc["calc"] = [expr]

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

    calc["input"] = [f"@{input_txt}"]
    calc["output"] = outfile
    calc["calc"] = ["A + B"]

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

    calc["input"] = [f"A={input_1}", f"B={input_2}"]
    calc["output"] = outfile
    calc["calc"] = ["A[1] + A[2] + B[1] + B[2] + B[3]"]

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

    calc["input"] = [f"A={inputs[0]}", f"B={inputs[1]}", f"C={inputs[2]}"]
    calc["calc"] = ["A + B + C"]
    calc["output"] = outfile

    calc["no-check-extent"] = True
    with pytest.raises(Exception, match="Inputs do not have the same dimensions"):
        calc.Run()
    calc["no-check-extent"] = False

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

    calc["input"] = [f"A={input_1}", f"B={input_2}"]
    calc["output"] = outfile
    calc["calc"] = ["A+B"]

    with pytest.raises(Exception, match="extents are inconsistent"):
        calc.Run()

    calc["no-check-extent"] = True
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

    calc["input"] = [f"B={input_1}", f"A={input_2}"]
    calc["output"] = outfile
    calc["calc"] = ["A+B"]

    with pytest.raises(Exception, match="spatial reference systems are inconsistent"):
        calc.Run()

    calc["no-check-srs"] = True
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

    calc["input"] = [f"A={input_1}", f"B={input_2}"]
    calc["output"] = outfile
    calc["calc"] = ["A+B"]

    with pytest.raises(Exception, match="incompatible numbers of bands"):
        calc.Run()

    calc["calc"] = ["A+B[1]"]
    assert calc.Run()


@pytest.mark.parametrize(
    "expr,source,bands,expected",
    [
        ("aX + 2", "aX", 1, ["aX[1] + 2"]),
        ("aX + 2", "aX", 2, ["aX[1] + 2", "aX[2] + 2"]),
        ("aX + 2", "X", 1, ["aX + 2"]),
        ("aX + 2", "a", 1, ["aX + 2"]),
        ("2 + aX", "X", 1, ["2 + aX"]),
        ("2 + aX", "aX", 1, ["2 + aX[1]"]),
        ("B1 + B10", "B1", 1, ["B1[1] + B10"]),
        ("B1[1] + B10", "B1", 2, ["B1[1] + B10"]),
        ("B1[1] + B1", "B1", 2, ["B1[1] + B1[1]", "B1[1] + B1[2]"]),
        ("SIN(N) + N", "N", 1, ["SIN(N[1]) + N[1]"]),
        ("SUM(N,N2) + N", "N", 1, ["SUM(N[1],N2) + N[1]"]),
        ("SUM(N,N2) + N", "N2", 1, ["SUM(N,N2[1]) + N"]),
        ("A_X + X", "X", 1, ["A_X + X[1]"]),
    ],
)
def test_gdalalg_raster_calc_expression_rewriting(
    calc, tmp_vsimem, expr, source, bands, expected
):
    # The expression rewriting isn't exposed to Python, so we
    # create an VRT with an expression and a single source, and
    # inspect the transformed expression in the VRT XML.
    # The transformed expression need not be valid, because we
    # don't actually read the VRT in GDAL.

    import xml.etree.ElementTree as ET

    outfile = tmp_vsimem / "out.vrt"
    infile = tmp_vsimem / "input.tif"

    gdal.GetDriverByName("GTiff").Create(infile, 2, 2, bands)

    calc["input"] = [f"{source}={infile}"]
    calc["output"] = outfile
    calc["calc"] = [expr]

    assert calc.Run()

    with gdal.VSIFile(outfile, "r") as f:
        root = ET.fromstring(f.read())

    expr = [
        node.attrib["expression"] for node in root.findall(".//PixelFunctionArguments")
    ]
    assert expr == expected
