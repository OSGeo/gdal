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

import functools
import math
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

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    infile = "../gcore/data/rgbsmall.tif"
    outfile = tmp_vsimem / "out.{output_format}"

    calc["input"] = [infile]
    calc["output"] = outfile
    calc["calc"] = ["2 + X / (1 + sum(X[1], X[2], X[3]))"]

    assert calc.Run()

    with gdal.Open(infile) as src, gdal.Open(outfile) as dst:
        srcval = src.ReadAsMaskedArray().astype("float64")
        expected = np.apply_along_axis(lambda x: 2 + x / (1 + x.sum()), 0, srcval)

        np.testing.assert_array_equal(expected, dst.ReadAsArray())
        assert src.GetGeoTransform() == dst.GetGeoTransform()
        assert src.GetSpatialRef().IsSame(dst.GetSpatialRef())


@pytest.mark.parametrize("output_format", ("tif", "vrt"))
def test_gdalalg_raster_calc_basic_2(calc, tmp_vsimem, output_format):

    gdaltest.importorskip_gdal_array()
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


@pytest.mark.parametrize("dialect", ("muparser", "builtin"))
@pytest.mark.parametrize("propagateNoData", (True, False))
def test_gdalalg_raster_calc_nodata(calc, tmp_vsimem, dialect, propagateNoData):

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    if (
        dialect == "muparser"
        and gdal.GetDriverByName("VRT").GetMetadataItem(
            "MUPARSER_HAS_DEFINE_FUN_USER_DATA"
        )
        is None
    ):
        pytest.skip("muparser version does not support isnodata function")

    input_1 = tmp_vsimem / "in1.tif"
    input_2 = tmp_vsimem / "in2.tif"

    with gdal.GetDriverByName("GTiff").Create(
        input_1, 2, 2, eType=gdal.GDT_Int16
    ) as ds:
        ds.GetRasterBand(1).SetNoDataValue(-9)
        ds.WriteArray(np.array([[1, 2], [-9, 4]]))

    with gdal.GetDriverByName("GTiff").Create(
        input_2, 2, 2, eType=gdal.GDT_Int16
    ) as ds:
        ds.GetRasterBand(1).SetNoDataValue(-999)
        ds.WriteArray(np.array([[1, -999], [3, 4]]))

    calc["input"] = [f"A={input_1}", f"B={input_2}"]
    calc["calc"] = (
        "(isnodata(A) ? 0 : A) + (isnodata(B) ? 0 : B)"
        if dialect == "muparser"
        else "sum"
    )
    calc["dialect"] = dialect
    calc["nodata"] = 255
    calc["output-format"] = "stream"

    if propagateNoData:
        calc["propagate-nodata"] = True

    assert calc.Run()
    assert calc["output"].GetDataset().RasterCount == 1

    result = calc["output"].GetDataset().ReadAsArray()

    if propagateNoData:
        np.testing.assert_array_equal(result, [[2, 255], [255, 8]])
    elif dialect == "builtin":
        np.testing.assert_array_equal(result, [[2, 2], [3, 8]])


@pytest.mark.parametrize("output_type", (gdal.GDT_Int16, gdal.GDT_Float32))
def test_gdalalg_raster_calc_nan_result(calc, tmp_vsimem, output_type):

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src.tif", 1, 1, eType=gdal.GDT_Float32
    ) as ds:
        ds.GetRasterBand(1).SetNoDataValue(-999)
        ds.GetRasterBand(1).Fill(-999)

    calc["input"] = [tmp_vsimem / "src.tif"]
    calc["calc"] = "X + nan"
    calc["output-format"] = "stream"
    calc["output-data-type"] = output_type

    assert calc.Run()

    result = calc["output"].GetDataset().ReadAsMaskedArray()

    if output_type == gdal.GDT_Int16:
        # NaN output value cannot be represented as an integer, so it
        # becomes zero (for now)
        np.testing.assert_array_equal(result, np.ma.masked_array([[0]], False))
    else:
        assert np.isnan(result)


def test_gdalalg_raster_calc_nodata_variable(calc, tmp_vsimem):
    if (
        gdal.GetDriverByName("VRT").GetMetadataItem("MUPARSER_HAS_DEFINE_FUN_USER_DATA")
        is None
    ):
        pytest.skip("muparser version does not support isnodata function")

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src.tif", 1, 1, eType=gdal.GDT_Float32
    ) as ds:
        ds.GetRasterBand(1).SetNoDataValue(-999)
        ds.GetRasterBand(1).Fill(-999)

    calc["input"] = [tmp_vsimem / "src.tif"]
    calc["calc"] = "isnodata(X) ? NODATA : 5"
    calc["output-format"] = "stream"
    calc["nodata"] = -802

    assert calc.Run()

    result = calc["output"].GetDataset().ReadAsMaskedArray()

    assert np.all(result.mask)


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


def test_gdalalg_raster_calc_output_type(calc, tmp_vsimem):

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")
    gdaltest.importorskip_gdal_array()

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src.tif", 5, 5, eType=gdal.GDT_Int32
    ) as src_ds:
        src_ds.GetRasterBand(1).Fill(500)

    calc["input"] = tmp_vsimem / "src.tif"
    calc["output"] = ""
    calc["output-format"] = "MEM"
    calc["calc"] = "X > 256 ? 100 : 50"
    calc["output-data-type"] = "Byte"

    assert calc.Run()

    dst_ds = calc["output"].GetDataset()

    assert dst_ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
    assert np.all(dst_ds.ReadAsArray() == 100)

    assert calc.Finalize()


def test_gdalalg_raster_calc_invalid_nodata_for_output_type(calc, tmp_vsimem):

    with gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "src.tif", 1, 1, eType=gdal.GDT_Int16
    ) as ds:
        ds.GetRasterBand(1).SetNoDataValue(-9)
        ds.GetRasterBand(1).Fill(-9)

    calc["input"] = tmp_vsimem / "src.tif"
    calc["output-format"] = "stream"
    calc["calc"] = "X"
    calc["output-data-type"] = "Byte"
    calc["nodata"] = -9

    with pytest.raises(Exception, match="Byte cannot represent NoData value -9"):
        calc.Run()


def test_gdalalg_raster_calc_output_nodata_taken_from_source(calc, tmp_vsimem):

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src.tif", 1, 1) as ds:
        ds.GetRasterBand(1).SetNoDataValue(255)
        ds.GetRasterBand(1).Fill(255)

    calc["input"] = tmp_vsimem / "src.tif"
    calc["output-format"] = "stream"
    calc["calc"] = "X"
    calc["propagate-nodata"] = True

    assert calc.Run()

    ds = calc["output"].GetDataset()

    assert ds.GetRasterBand(1).GetNoDataValue() == 255

    with pytest.raises(Exception, match="no valid pixels"):
        ds.GetRasterBand(1).ComputeRasterMinMax(False)


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

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    infile = "../gcore/data/byte.tif"
    outfile = tmp_vsimem / "out.tif"

    calc["input"] = [f"X={infile}"]
    calc["output"] = outfile
    calc["calc"] = [expr]

    assert calc.Run()

    with gdal.Open(infile) as src, gdal.Open(outfile) as dst:
        np.testing.assert_array_equal(src.ReadAsArray() + 3.0, dst.ReadAsArray())


def test_gdalalg_raster_calc_several_inputs_same_name(calc, tmp_vsimem):

    calc["input"] = ["A=../gcore/data/byte.tif", "A=../gcore/data/uint16.tif"]
    calc["output"] = tmp_vsimem / "out.vrt"
    calc["calc"] = "A"

    with pytest.raises(
        Exception, match="An input with name 'A' has already been provided"
    ):
        calc.Run()


@pytest.mark.parametrize("dialect", ("muparser", "builtin"))
def test_gdalalg_raster_calc_several_inputs_no_name(calc, tmp_vsimem, dialect):

    calc["input"] = ["../gcore/data/byte.tif", "../gcore/data/uint16.tif"]
    calc["output"] = tmp_vsimem / "out.vrt"
    calc["calc"] = "sum"
    calc["dialect"] = dialect

    try:
        calc.Run()
    except Exception as e:
        assert "Inputs must be named" in str(e)
        assert dialect == "muparser"
    else:
        assert dialect == "builtin"


@pytest.mark.parametrize(
    "name,expected_error_msg",
    [
        ("_pi", "Name '_pi' is illegal because it starts with a '_'"),
        ("0ko", "Name '0ko' is illegal because it starts with a '0'"),
        ("ko-", "Name 'ko-' is illegal because character '-' is not allowed"),
        ("ok", None),
        ("ok_", None),
        ("o0123456789", None),
    ],
)
def test_gdalalg_raster_calc_test_name(calc, name, expected_error_msg):

    calc["input"] = f"{name}=../gcore/data/byte.tif"
    calc["output-format"] = "MEM"
    calc["calc"] = name

    if expected_error_msg:
        with pytest.raises(Exception, match=expected_error_msg):
            calc.Run()
    else:
        calc.Run()


def test_gdalalg_raster_calc_multiple_calcs(calc, tmp_vsimem):

    gdaltest.importorskip_gdal_array()
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

    gdaltest.importorskip_gdal_array()
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


@pytest.mark.parametrize("formula", ["A+B", "sum(A, B)"])
def test_gdalalg_raster_calc_inputs_from_file(calc, tmp_vsimem, tmp_path, formula):

    gdaltest.importorskip_gdal_array()
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
    calc["calc"] = formula

    assert calc.Run()

    with gdal.Open(outfile) as dst:
        assert np.all(dst.ReadAsArray() == 3)


def test_gdalalg_raster_calc_different_band_counts(calc, tmp_vsimem):

    gdaltest.importorskip_gdal_array()
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

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    xmax = 60
    ymax = 60
    resolutions = [30, 20, 60]

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
        assert ds.RasterXSize == xmax / functools.reduce(math.gcd, resolutions)
        assert ds.RasterYSize == ymax / functools.reduce(math.gcd, resolutions)

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


def test_gdalalg_raster_calc_error_extent_within_tolerance(calc, tmp_vsimem):

    input_1 = tmp_vsimem / "in1.tif"
    input_2 = tmp_vsimem / "in2.tif"
    outfile = tmp_vsimem / "out.tif"

    with gdal.GetDriverByName("GTIff").Create(input_2, 3600, 3600) as ds:
        ds.SetGeoTransform(
            (
                2.4999861111111112e01,
                2.7777777777777778e-04,
                0.0000000000000000e00,
                8.0000138888888884e01,
                0.0000000000000000e00,
                -2.7777777777777778e-04,
            )
        )
    with gdal.GetDriverByName("GTiff").Create(input_1, 3600, 3600) as ds:
        # this geotransform represents error introduced by writing a dataset with the above
        # geotransform to netCDF
        ds.SetGeoTransform(
            (
                2.4999861111111112e01,
                2.7777777777777778e-04,
                0.0000000000000000e00,
                8.0000138888888884e01,
                0.0000000000000000e00,
                -2.7777777777778173e-04,
            )
        )

    calc["input"] = [f"A={input_1}", f"B={input_2}"]
    calc["output"] = outfile
    calc["calc"] = ["A+B"]
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
    calc["no-check-expression"] = True

    assert calc.Run()

    with gdal.VSIFile(outfile, "r") as f:
        root = ET.fromstring(f.read())

    expr = [
        node.attrib["expression"] for node in root.findall(".//PixelFunctionArguments")
    ]
    assert expr == expected


@pytest.mark.require_driver("GDALG")
def test_gdalalg_raster_calc_gdalg_json(calc, tmp_vsimem):

    outfile = tmp_vsimem / "out.gdalg.json"

    calc["input"] = "../gcore/data/rgbsmall.tif"
    calc["output"] = outfile
    calc["calc"] = "X"
    assert calc.Run()
    assert calc.Finalize()

    with gdal.Open(outfile) as ds:
        assert ds.GetRasterBand(1).Checksum() == 21212
        assert ds.GetRasterBand(2).Checksum() == 21053
        assert ds.GetRasterBand(3).Checksum() == 21349


@pytest.mark.parametrize(
    "output_format,ext",
    [
        ("VRT", None),
        ("VRT", "vrt"),
        ("stream", None),
        ("GDALG", None),
        ("GDALG", "gdalg.json"),
        ("GTiff", None),
        ("GTiff", "tif"),
    ],
)
def test_gdalalg_raster_calc_invalid_formula(calc, tmp_vsimem, output_format, ext):

    if output_format != "stream" and gdal.GetDriverByName(output_format) is None:
        pytest.skip(f"{output_format} not available")

    calc["input"] = "../gcore/data/byte.tif"
    if ext:
        calc["output"] = tmp_vsimem / f"out.{ext}"
    else:
        calc["output-format"] = output_format
        calc["output"] = tmp_vsimem / "out"
    calc["calc"] = "invalid"
    with pytest.raises(Exception, match="invalid variable name"):
        calc.Run()


def test_gdalalg_raster_calc_reference_several_bands_to_stream(calc):

    calc["input"] = "../gcore/data/rgbsmall.tif"
    calc["output-format"] = "stream"
    calc["output"] = "streamed"
    calc["calc"] = "sum(X[1], X[2], X[3])"

    assert calc.Run()

    assert calc["output"].GetDataset().RasterCount == 1
    assert calc["output"].GetDataset().GetRasterBand(1).Checksum() == 21240


@pytest.mark.parametrize("fn", ("avg", "min", "max", "sum"))
def test_gdalalg_raster_calc_muparser_flatten(calc, tmp_vsimem, fn):

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "in1.tif", 1, 1, 2) as ds:
        ds.GetRasterBand(1).Fill(10)
        ds.GetRasterBand(2).Fill(100)

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "in2.tif", 1, 1, 3) as ds:
        ds.GetRasterBand(1).Fill(2)
        ds.GetRasterBand(2).Fill(20)
        ds.GetRasterBand(3).Fill(200)

    calc["input"] = [f"A={tmp_vsimem}/in1.tif", f"B={tmp_vsimem}/in2.tif"]
    calc["output-format"] = "stream"
    calc["output"] = ""
    calc["calc"] = f"{fn}(A)-{fn}(B)"
    calc["flatten"] = True
    calc.Run()
    ds = calc["output"].GetDataset()
    assert ds.RasterCount == 1

    if fn == "sum":
        expected_val = (10 + 100) - (2 + 20 + 200)
    elif fn == "avg":
        expected_val = (10 + 100) / 2 - (2 + 20 + 200) / 3
    elif fn == "max":
        expected_val = 100 - 200
    elif fn == "min":
        expected_val = 10 - 2

    assert ds.GetRasterBand(1).ComputeRasterMinMax(False) == (
        expected_val,
        expected_val,
    )


def test_gdalalg_raster_calc_muparser_flatten_not_an_aggregate(calc, tmp_vsimem):

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "src.tif", 1, 1, 2) as ds:
        ds.GetRasterBand(1).Fill(10)
        ds.GetRasterBand(2).Fill(100)

    calc["input"] = [tmp_vsimem / "src.tif"]
    calc["output-format"] = "stream"
    calc["output"] = ""
    calc["calc"] = "sin(X)"
    calc["flatten"] = True

    assert calc.Run()
    ds = calc["output"].GetDataset()
    assert ds.RasterCount == 2


def test_gdalalg_raster_calc_muparser_partial_flatten(calc, tmp_vsimem):

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "in1.tif", 1, 1, 2) as ds:
        ds.GetRasterBand(1).Fill(10)
        ds.GetRasterBand(2).Fill(100)

    calc["input"] = [f"A={tmp_vsimem}/in1.tif"]
    calc["output-format"] = "stream"
    calc["output"] = ""
    calc["calc"] = "A / sum(A)"
    calc["flatten"] = True
    calc.Run()
    ds = calc["output"].GetDataset()
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).ComputeRasterMinMax(False) == (
        10 / 110,
        10 / 110,
    )
    assert ds.GetRasterBand(2).ComputeRasterMinMax(False) == (
        100 / 110,
        100 / 110,
    )


def test_gdalalg_raster_calc_muparser_nothing_to_flatten(calc, tmp_vsimem):

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "in1.tif", 1, 1, 2) as ds:
        ds.GetRasterBand(1).Fill(10)
        ds.GetRasterBand(2).Fill(100)

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "in2.tif", 1, 1, 2) as ds:
        ds.GetRasterBand(1).Fill(20)
        ds.GetRasterBand(2).Fill(200)

    calc["input"] = [f"A={tmp_vsimem}/in1.tif", f"B={tmp_vsimem}/in2.tif"]
    calc["output-format"] = "stream"
    calc["output"] = ""
    calc["calc"] = "(A + B)"
    calc["flatten"] = True
    calc.Run()
    ds = calc["output"].GetDataset()
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).ComputeRasterMinMax(False) == (
        10 + 20,
        10 + 20,
    )
    assert ds.GetRasterBand(2).ComputeRasterMinMax(False) == (
        100 + 200,
        100 + 200,
    )


@pytest.mark.parametrize("expression", ("min", "max", "mode", "mean", "median"))
def test_gdalalg_raster_calc_dialect_builtin(calc, expression):

    calc["input"] = "../gcore/data/rgbsmall.tif"
    calc["output-format"] = "stream"
    calc["output"] = ""
    calc["calc"] = expression
    calc["dialect"] = "builtin"
    calc["flatten"] = True
    calc.Run()
    ds = calc["output"].GetDataset()
    assert ds.RasterCount == 1

    if expression in ("min", "max", "mode"):
        assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt8
    else:
        assert ds.GetRasterBand(1).DataType == gdal.GDT_Float64

    if expression == "mean":
        assert ds.GetRasterBand(1).Checksum() == 21559


def test_gdalalg_raster_calc_pixel_function_arg(calc):

    calc["input"] = "../gcore/data/byte.tif"
    calc["output-format"] = "stream"
    calc["output"] = ""
    calc["calc"] = "sum(k=1)"
    calc["dialect"] = "builtin"
    calc.Run()
    ds = calc["output"].GetDataset()
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float64
    assert ds.GetRasterBand(1).Checksum() == 4455


def test_gdalalg_raster_calc_builtin_with_multiple_inputs(calc, tmp_vsimem):

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "in1.tif", 1, 1, 2) as ds:
        ds.GetRasterBand(1).Fill(10)
        ds.GetRasterBand(2).Fill(100)

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "in2.tif", 1, 1, 2) as ds:
        ds.GetRasterBand(1).Fill(20)
        ds.GetRasterBand(2).Fill(200)

    calc["input"] = [f"A={tmp_vsimem}/in1.tif", f"B={tmp_vsimem}/in2.tif"]
    calc["output-format"] = "stream"
    calc["output"] = ""
    calc["calc"] = "mean"
    calc["dialect"] = "builtin"
    calc.Run()
    ds = calc["output"].GetDataset()
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float64
    assert ds.GetRasterBand(1).ComputeRasterMinMax(False) == (15, 15)
    assert ds.GetRasterBand(2).ComputeRasterMinMax(False) == (150, 150)


def test_gdalalg_raster_calc_builtin_with_multiple_formula(calc):

    calc["input"] = "../gcore/data/byte.tif"
    calc["output-format"] = "stream"
    calc["output"] = ""
    calc["calc"] = ["mean", "sum(k=1)"]
    calc["dialect"] = "builtin"
    calc.Run()
    ds = calc["output"].GetDataset()
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).ComputeRasterMinMax(False) == (74, 255)
    assert ds.GetRasterBand(2).ComputeRasterMinMax(False) == (75, 256)


def test_gdalalg_raster_calc_complete():
    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(f"{gdal_path} completion gdal raster calc --calc")
    assert "mean" not in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster calc --dialect=builtin --calc"
    )
    assert "mean" in out


def test_gdalalg_raster_calc_sum_builtin_one_band_two_bands(calc, tmp_vsimem):

    input_1 = tmp_vsimem / "in1.tif"
    input_2 = tmp_vsimem / "in2.tif"

    with gdal.GetDriverByName("GTiff").Create(input_1, 1, 1, 1) as ds:
        ds.GetRasterBand(1).Fill(1)
    with gdal.GetDriverByName("GTIff").Create(input_2, 1, 1, 2) as ds:
        ds.GetRasterBand(1).Fill(10)
        ds.GetRasterBand(2).Fill(100)

    calc["input"] = [f"A={input_1}", f"B={input_2}"]
    calc["output-format"] = "MEM"
    calc["calc"] = "sum"
    calc["dialect"] = "builtin"
    assert calc.Run()

    out_ds = calc["output"].GetDataset()
    assert out_ds.RasterCount == 2
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax(False) == (1 + 10, 1 + 10)
    assert out_ds.GetRasterBand(2).ComputeRasterMinMax(False) == (1 + 100, 1 + 100)


def test_gdalalg_raster_calc_sum_builtin_two_bands_three_bands_fail(calc, tmp_vsimem):

    input_1 = tmp_vsimem / "in1.tif"
    input_2 = tmp_vsimem / "in2.tif"

    gdal.GetDriverByName("GTiff").Create(input_1, 1, 1, 2)
    gdal.GetDriverByName("GTIff").Create(input_2, 1, 1, 3)

    calc["input"] = [f"A={input_1}", f"B={input_2}"]
    calc["output-format"] = "MEM"
    calc["calc"] = "sum"
    calc["dialect"] = "builtin"
    with pytest.raises(
        Exception,
        match=r"Expression cannot operate on all bands of rasters with incompatible numbers of bands \(source B has 3 bands but expected to have 1 or 2 bands\)",
    ):
        calc.Run()


def test_gdalalg_raster_calc_sum_float_input_with_nodata(calc, tmp_vsimem):

    input = tmp_vsimem / "in.tif"

    with gdal.GetDriverByName("GTiff").Create(input, 1, 1, 1, gdal.GDT_Float32) as ds:
        ds.GetRasterBand(1).SetNoDataValue(0)
        ds.GetRasterBand(1).Fill(0.1)

    calc["input"] = [f"A={input}"]
    calc["output-format"] = "MEM"
    calc["calc"] = "A * 10"
    calc["output-data-type"] = "Byte"
    calc.Run()

    out_ds = calc["output"].GetDataset()
    assert out_ds.GetRasterBand(1).Checksum() == 1


@pytest.mark.require_driver("GDALG")
def test_gdalalg_raster_calc_input_pipeline(calc):

    calc["input"] = "A=[ read ../gcore/data/byte.tif ! aspect ]"
    calc["output-format"] = "MEM"
    calc["calc"] = "A * 10"
    calc.Run()

    out_ds = calc["output"].GetDataset()
    assert out_ds.GetRasterBand(1).Checksum() == 4692
