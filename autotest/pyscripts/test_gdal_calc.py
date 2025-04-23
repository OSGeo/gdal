#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id: test_gdal_calc.py 25549 2013-01-26 11:17:10Z rouault $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_calc.py testing
# Author:   Etienne Tourigny <etourigny dot dev @ gmail dot com>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault @ spatialys.com>
# Copyright (c) 2014, Etienne Tourigny <etourigny dot dev @ gmail dot com>
# Copyright (c) 2020, Idan Miara <idan@miara.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil
import sys

import gdaltest
import pytest
import test_py_scripts

from osgeo import gdal

# test that numpy is available, if not skip all tests
np = pytest.importorskip("numpy")
gdal_calc = pytest.importorskip("osgeo_utils.gdal_calc", exc_type=ImportError)
gdal_array = gdaltest.importorskip_gdal_array()
try:
    GDALTypeCodeToNumericTypeCode = gdal_array.GDALTypeCodeToNumericTypeCode
except AttributeError:
    pytestmark = pytest.mark.skip(
        "osgeo.gdal_array.GDALTypeCodeToNumericTypeCode is unavailable"
    )

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdal_calc") is None,
    reason="gdal_calc not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdal_calc")


###############################################################################
#


def test_gdal_calc_help(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_calc", "--help"
    )


###############################################################################
#


def test_gdal_calc_version(script_path):

    assert "ERROR" not in test_py_scripts.run_py_script(
        script_path, "gdal_calc", "--version"
    )


# Usage: gdal_calc.py [-A <filename>] [--A_band] [-B...-Z filename] [other_options]


def check_file(filename_or_ds, checksum, i=None, bnd_idx=1):
    if gdal_calc.is_path_like(filename_or_ds):
        ds = gdal.Open(os.fspath(filename_or_ds))
    else:
        ds = filename_or_ds
    assert ds is not None, f'ds{i if i is not None else ""} not found'
    ds_checksum = ds.GetRasterBand(bnd_idx).Checksum()
    if checksum is None:
        print(f"ds{i} bnd{bnd_idx} checksum is {ds_checksum}")
    else:
        assert (
            ds_checksum == checksum
        ), f"ds{i} bnd{bnd_idx} wrong checksum, expected {checksum}, got {ds_checksum}"
    return ds


input_checksum = (12603, 58561, 36064, 10807)


@pytest.fixture(scope="module")
def stefan_full_rgba(tmp_path_factory):

    tmpdir = tmp_path_factory.mktemp("gdal_calc")

    infile = tmpdir / "stefan_full_rgba.tif"

    if not os.path.isfile(infile):
        shutil.copy(
            test_py_scripts.get_data_path("gcore") + "stefan_full_rgba.tif", infile
        )

    return infile


def test_gdal_calc_py_1(script_path, tmp_path, stefan_full_rgba):
    """test basic copy"""

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"

    _, err = test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f"-A {infile} --calc=A --overwrite --outfile {out}",
        return_stderr=True,
    )
    assert "UseExceptions" not in err

    check_file(out, input_checksum[0])


def test_gdal_calc_py_1b(script_path, tmp_path, stefan_full_rgba):

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"

    test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f"-A {infile} --A_band=2 --calc=A --overwrite --outfile {out}",
    )

    check_file(out, input_checksum[1])


def test_gdal_calc_py_1c(script_path, tmp_path, stefan_full_rgba):

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"

    test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f"-Z {infile} --Z_band=2 --calc=Z --overwrite --format GTiff --outfile {out}",
    )

    check_file(out, input_checksum[1])

    # Test update
    with gdal.Open(out, gdal.GA_Update) as ds:
        ds.GetRasterBand(1).Fill(0)

    test_py_scripts.run_py_script(
        script_path, "gdal_calc", f"-Z {infile} --Z_band=2 --calc=Z --outfile {out}"
    )
    check_file(out, input_checksum[1])

    # Test failed overwrite (missing --overwrite)
    with gdal.Open(out, gdal.GA_Update) as ds:
        ds.GetRasterBand(1).Fill(0)
        zero_cs = ds.GetRasterBand(1).Checksum()

    test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f"-Z {infile} --Z_band=2 --calc=Z --format GTiff --outfile {out}",
    )
    check_file(out, zero_cs)


def test_gdal_calc_py_2a(script_path, tmp_path, stefan_full_rgba):
    """test simple formula: A+B"""

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"

    test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f"-A {infile} --A_band 1 -B {infile} --B_band 2 --calc=A+B "
        f"--overwrite --outfile {out}",
    )

    check_file(out, 12368, 1)


def test_gdal_calc_py_2b(script_path, tmp_path, stefan_full_rgba):
    """test simple formula: A*B"""

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"

    test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f"-A {infile} --A_band 1 -B {infile} --B_band 2 --calc=A*B "
        f"--overwrite --outfile {out}",
    )

    check_file(out, 62785, 1)


def test_gdal_calc_py_2c(script_path, tmp_path, stefan_full_rgba):
    """test simple formula: sqrt(A)"""

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"

    test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f'-A {infile} --A_band 1 --calc="sqrt(A)" --type=Float32 '
        f"--overwrite --outfile {out}",
    )

    check_file(out, 47132, 1)


def test_gdal_calc_py_3(script_path, tmp_path, stefan_full_rgba):
    """test --allBands option (simple copy)"""

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"

    test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f"-A {infile} --allBands A --calc=A --overwrite --outfile {out}",
    )

    bnd_count = 4
    for i, checksum in enumerate(input_checksum[0:bnd_count]):
        check_file(out, checksum, 1, bnd_idx=i + 1)


def test_gdal_calc_py_4a(script_path, tmp_path, stefan_full_rgba):
    """test --allBands option (simple calc)"""

    infile = stefan_full_rgba
    ones = tmp_path / "ones.tif"
    out = tmp_path / "out.tif"

    # some values are clipped to 255, but this doesn't matter... small values were visually checked
    test_py_scripts.run_py_script(
        script_path, "gdal_calc", f"-A {infile} --calc=1 --overwrite --outfile {ones}"
    )

    test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f"-A {infile} -B {ones} --B_band 1 --allBands A --calc=A+B --NoDataValue=999 "
        f"--overwrite --outfile {out}",
    )

    for band, checksum in enumerate((29935, 13128, 59092)):
        ds = check_file(out, checksum, 2, bnd_idx=band + 1)
        # also check NoDataValue
        ds = gdal.Open(out)
        assert ds.GetRasterBand(band + 1).GetNoDataValue() == 999


def test_gdal_calc_py_4b(script_path, tmp_path, stefan_full_rgba):
    """test --allBands option (simple calc)"""

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"

    # these values were not tested
    test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f"-A {infile} -B {infile} --B_band 1 --allBands A --calc=A*B --NoDataValue=999 "
        f"--overwrite --outfile {out}",
    )

    for band, checksum in enumerate((10025, 62785, 10621)):
        check_file(out, checksum, 3, bnd_idx=band + 1)
        # also check NoDataValue
        ds = gdal.Open(out)
        assert ds.GetRasterBand(band + 1).GetNoDataValue() == 999


def test_gdal_calc_py_allbands(tmp_vsimem):
    """test --allBands option (simple calc)"""

    input1 = tmp_vsimem / "in1.tif"
    input2 = tmp_vsimem / "in2.tif"
    out = tmp_vsimem / "out.tif"

    with gdal.GetDriverByName("GTiff").Create(input1, 3, 3, bands=3) as ds:
        ds.GetRasterBand(1).Fill(9)
        ds.GetRasterBand(2).Fill(13)
        ds.GetRasterBand(3).Fill(17)
        ds.SetGeoTransform((0, 1, 0, 3, 0, -1))

    with gdal.GetDriverByName("GTiff").Create(input2, 3, 3, bands=3) as ds:
        ds.GetRasterBand(1).Fill(3)
        ds.GetRasterBand(2).Fill(5)
        ds.GetRasterBand(3).Fill(7)
        ds.SetGeoTransform((0, 1, 0, 3, 0, -1))

    # 3 bands * 1 band
    ds_out = gdal_calc.Calc(
        A=input1, B=input2, B_band=1, allBands="A", calc="A*B", outfile=out, quiet=True
    )
    assert ds_out.RasterCount == 3
    np.testing.assert_array_equal(ds_out.ReadAsArray()[:, 0, 0], [27, 39, 51])


def test_gdal_calc_py_5a(tmp_vsimem, stefan_full_rgba):
    """test python interface, basic copy"""

    infile = stefan_full_rgba
    out = tmp_vsimem / "out.tif"

    gdal_calc.Calc("A", A=infile, overwrite=True, quiet=True, outfile=out)

    check_file(out, input_checksum[0])


def test_gdal_calc_py_5b(tmp_vsimem, stefan_full_rgba):
    """test python interface, basic copy"""

    infile = stefan_full_rgba
    out = tmp_vsimem / "out.tif"

    gdal_calc.Calc("A", A=infile, A_band=2, overwrite=True, quiet=True, outfile=out)

    check_file(out, input_checksum[1])


def test_gdal_calc_py_5c(tmp_vsimem, stefan_full_rgba):
    """test python interface, basic copy"""

    infile = stefan_full_rgba
    out = tmp_vsimem / "out.tif"

    gdal_calc.Calc("Z", Z=infile, Z_band=2, overwrite=True, quiet=True, outfile=out)

    check_file(out, input_checksum[1])


def test_gdal_calc_py_5d(tmp_vsimem, stefan_full_rgba):
    """test python interface, basic copy"""

    infile = stefan_full_rgba
    out = tmp_vsimem / "out.tif"

    gdal_calc.Calc(
        ["A", "Z"],
        A=infile,
        Z=infile,
        Z_band=2,
        overwrite=True,
        quiet=True,
        outfile=out,
    )

    check_file(out, input_checksum[0], bnd_idx=1)
    check_file(out, input_checksum[1], bnd_idx=2)


def test_gdal_calc_py_6(tmp_path):
    """test nodata"""

    infile = tmp_path / "byte_nd74.tif"
    out = tmp_path / "out.tif"

    gdal.Translate(
        infile,
        test_py_scripts.get_data_path("gcore") + "byte.tif",
        options="-a_nodata 74",
    )

    check_file(infile, 4672)

    gdal_calc.Calc(
        "A", A=infile, overwrite=True, quiet=True, outfile=out, NoDataValue=1
    )

    ds = check_file(out, 4673)
    result = ds.GetRasterBand(1).ComputeRasterMinMax()
    assert result == (90, 255), "Error! min/max not correct!"


@pytest.mark.parametrize("opt_prefix", ("--optfile ", "@"))
def test_gdal_calc_py_7a(script_path, tmp_path, stefan_full_rgba, opt_prefix):
    """test --optfile"""

    if opt_prefix == "@" and sys.platform == "win32":
        pytest.skip("@optfile is not read correctly on Windows")

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"
    opt_file = tmp_path / "opt"

    with open(opt_file, "w") as f:
        f.write(f"-A {infile} --calc=A --overwrite --outfile {out}")

    test_py_scripts.run_py_script(script_path, "gdal_calc", f"{opt_prefix}{opt_file}")
    check_file(out, input_checksum[0])


def test_gdal_calc_py_7b(script_path, tmp_path, stefan_full_rgba):
    """test --optfile"""

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"
    opt_file = tmp_path / "opt"

    # Lines in optfiles beginning with '#' should be ignored
    with open(opt_file, "w") as f:
        f.write(f"-A {infile} --A_band=2 --calc=A --overwrite --outfile {out}")
        f.write("\n# -A_band=1")

    test_py_scripts.run_py_script(script_path, "gdal_calc", f"--optfile {opt_file}")

    check_file(out, input_checksum[1])


def test_gdal_calc_py_7c(script_path, tmp_path, stefan_full_rgba):
    """test --optfile"""

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"
    opt_file = tmp_path / "opt"

    # options on separate lines should work, too
    opts = (
        f"-Z {infile}",
        "--Z_band=2",
        "--calc=Z",
        "--overwrite",
        f"--outfile {out}",
    )
    with open(opt_file, "w") as f:
        for i in opts:
            f.write(i + "\n")

    test_py_scripts.run_py_script(script_path, "gdal_calc", f"--optfile {opt_file}")

    check_file(out, input_checksum[1])


def test_gdal_calc_py_7d(script_path, tmp_path, stefan_full_rgba):
    """test --optfile"""

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"
    opt_file = tmp_path / "opt"

    # double-quoted options should be read as single arguments. Mixed numbers of arguments per line should work.
    opts = (
        f"-Z {infile} --Z_band=2",
        '--calc "Z + 0"',
        f"--overwrite --outfile {out}",
    )
    with open(opt_file, "w") as f:
        for i in opts:
            f.write(i + "\n")

    test_py_scripts.run_py_script(script_path, "gdal_calc", f"--optfile {opt_file}")

    check_file(out, input_checksum[1])


def test_gdal_calc_py_8(script_path, tmp_path, stefan_full_rgba):
    """test multiple calcs"""

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"

    test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f"-A {infile} --A_band=1 -B {infile} --B_band=2 -Z {infile} --Z_band=2 --calc=A --calc=B --calc=Z "
        f"--overwrite --outfile {out}",
    )

    for i, checksum in enumerate(
        (input_checksum[0], input_checksum[1], input_checksum[1])
    ):
        check_file(out, checksum, 1, bnd_idx=i + 1)


def test_gdal_calc_py_numpy_max_1(tmp_vsimem, stefan_full_rgba):

    out = tmp_vsimem / "out.tif"

    kwargs = {
        "a": gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[1]),
        "b": gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[2]),
        "c": gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[3]),
    }

    check_file(
        gdal_calc.Calc(
            calc="numpy.max((a,b,c),axis=0)", outfile=out, quiet=True, **kwargs
        ),
        13256,
    )


def test_gdal_calc_py_numpy_max_2(tmp_vsimem, stefan_full_rgba):

    out = tmp_vsimem / "out.tif"

    kwargs = {
        "a": [
            gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[1]),
            gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[2]),
            gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[3]),
        ]
    }

    check_file(
        gdal_calc.Calc(calc="numpy.max(a,axis=0)", outfile=out, quiet=True, **kwargs),
        13256,
    )


def test_gdal_calc_py_sum_overflow(tmp_vsimem, stefan_full_rgba):
    # for summing 3 bytes we'll use GDT_UInt16
    out = tmp_vsimem / "out.tif"

    gdal_dt = gdal.GDT_UInt16

    # sum with overflow

    kwargs = {
        "a": gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[1]),
        "b": gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[2]),
        "c": gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[3]),
    }

    check_file(
        gdal_calc.Calc(calc="a+b+c", type=gdal_dt, outfile=out, quiet=True, **kwargs),
        12261,
    )


def test_gdal_calc_py_numpy_sum(tmp_vsimem, stefan_full_rgba):
    # sum with numpy function, no overflow
    out = tmp_vsimem / "out.tif"

    gdal_dt = gdal.GDT_UInt16
    np_dt = GDALTypeCodeToNumericTypeCode(gdal_dt)

    kwargs = {
        "a": [
            gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[1]),
            gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[2]),
            gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[3]),
        ]
    }

    check_file(
        gdal_calc.Calc(
            calc="numpy.sum(a,axis=0,dtype=np_dt)",
            type=gdal_dt,
            outfile=out,
            user_namespace={"np_dt": np_dt},
            quiet=True,
            **kwargs,
        ),
        12789,
    )


def test_gdal_calc_py_user_namespace_1(tmp_vsimem, stefan_full_rgba):

    out = tmp_vsimem / "out.tif"

    kwargs = {
        "a": [
            gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[1]),
            gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[2]),
            gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[3]),
        ]
    }

    def my_max(a):
        """max using numpy"""
        concatenate = np.stack(a)
        ret = concatenate.max(axis=0)
        return ret

    check_file(
        gdal_calc.Calc(
            calc="my_neat_max(a)",
            outfile=out,
            user_namespace={"my_neat_max": my_max},
            quiet=True,
            **kwargs,
        ),
        13256,
    )


def test_gdal_calc_py_user_namespace_2(tmp_vsimem, stefan_full_rgba):
    out = tmp_vsimem / "out.tif"

    gdal_dt = gdal.GDT_UInt16

    kwargs = {
        "a": [
            gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[1]),
            gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[2]),
            gdal.Translate("", stefan_full_rgba, format="MEM", bandList=[3]),
        ]
    }

    def my_sum(a, gdal_dt=None):
        """sum using numpy"""
        np_dt = GDALTypeCodeToNumericTypeCode(gdal_dt)
        concatenate = np.stack(a)
        ret = concatenate.sum(axis=0, dtype=np_dt)
        return ret

    check_file(
        gdal_calc.Calc(
            calc="my_neat_sum(a, out_dt)",
            type=gdal_dt,
            outfile=out,
            user_namespace={"my_neat_sum": my_sum, "out_dt": gdal_dt},
            quiet=True,
            **kwargs,
        ),
        12789,
    )


def test_gdal_calc_py_10(script_path, tmp_path, stefan_full_rgba):
    """test --NoDataValue=none"""

    infile = stefan_full_rgba
    out = tmp_path / "out.tif"

    test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f"-A {infile} --A_band=1 --NoDataValue=none --calc=A "
        f"--overwrite --outfile {out}",
    )

    check_file(out, input_checksum[0])
    ds = gdal.Open(out)
    assert ds.GetRasterBand(1).GetNoDataValue() is None


def test_gdal_calc_py_multiple_inputs_same_alpha(script_path, tmp_path):
    """test multiple values for -A flag, including wildcards"""

    shutil.copy("../gcore/data/byte.tif", f"{tmp_path}/input_wildcard_1.tif")
    shutil.copy("../gcore/data/byte.tif", f"{tmp_path}/input_wildcard_2.tif")

    test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f'-A ../gcore/data/byte.tif ../gcore/data/byte.tif {tmp_path}/input_wildcard_*.tif --calc="sum(A.astype(numpy.float32),axis=0)" --overwrite --outfile {tmp_path}/test_gdal_calc_py_multiple_inputs_same_alpha.tif --type Float32 --overwrite',
    )

    test_py_scripts.run_py_script(
        script_path,
        "gdal_calc",
        f'-A ../gcore/data/byte.tif --calc="A.astype(numpy.float32)*4" --overwrite --outfile {tmp_path}/test_gdal_calc_py_multiple_inputs_same_alpha_ref.tif --type Float32 --overwrite',
    )

    ds = gdal.Open(f"{tmp_path}/test_gdal_calc_py_multiple_inputs_same_alpha.tif")
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    ds = gdal.Open(f"{tmp_path}/test_gdal_calc_py_multiple_inputs_same_alpha_ref.tif")
    cs_ref = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == cs_ref


def test_gdal_calc_py_extent(tmp_vsimem):

    input1 = tmp_vsimem / "in1.tif"
    input2 = tmp_vsimem / "in2.tif"
    out = tmp_vsimem / "out.tif"

    # input1: (0, 0) to (3, 3)
    with gdal.GetDriverByName("GTiff").Create(input1, 3, 3, 1) as ds:
        ds.GetRasterBand(1).Fill(5)
        ds.SetGeoTransform((0, 1, 0, 3, 0, -1))

    # input2: (1, 1) to (4, 4)
    with gdal.GetDriverByName("GTiff").Create(input2, 3, 3, 1) as ds:
        ds.GetRasterBand(1).Fill(3)
        ds.SetGeoTransform((1, 1, 0, 4, 0, -1))

    # default: ignore the geotransforms
    out_ds = gdal_calc.Calc(a=input1, b=input2, calc="a+b", outfile=out, quiet=True)
    assert out_ds.RasterXSize == 3
    assert out_ds.RasterYSize == 3
    assert out_ds.GetGeoTransform() == (1, 1, 0, 4, 0, -1)  # why?
    assert np.all(out_ds.ReadAsArray() == 8)

    # extent=fail
    with pytest.raises(Exception, match="incompatible"):
        out_ds = gdal_calc.Calc(
            a=input1, b=input2, calc="a+b", outfile=out, extent="fail", quiet=True
        )

    # extent=union
    out_ds = gdal_calc.Calc(
        a=input1, b=input2, calc="a+b", outfile=out, extent="union", quiet=True
    )
    assert out_ds.RasterXSize == 4
    assert out_ds.RasterYSize == 4
    assert out_ds.GetGeoTransform() == (0, 1, 0, 4, 0, -1)

    # default value is zero where an input is missing
    np.testing.assert_array_equal(
        out_ds.ReadAsArray(),
        np.array(
            [[0, 3, 3, 3], [5, 8, 8, 3], [5, 8, 8, 3], [5, 5, 5, 0]], dtype=np.uint8
        ),
    )

    # extent=intersect
    out_ds = gdal_calc.Calc(
        a=input1,
        b=input2,
        calc="a+b",
        outfile=out,
        extent="intersect",
        quiet=True,
    )
    assert out_ds.RasterXSize == 2
    assert out_ds.RasterYSize == 2
    assert out_ds.GetGeoTransform() == (1, 1, 0, 3, 0, -1)
    assert np.all(out_ds.ReadAsArray() == 8)

    # specifying extent and projwin causes extent to be ignored, apparently
    out_ds = gdal_calc.Calc(
        a=input1,
        b=input2,
        calc="a+b",
        outfile=out,
        extent="intersect",
        projwin=(0, 3, 2.999, 0.001),
        quiet=True,
    )
    assert out_ds.RasterXSize == 3
    assert out_ds.RasterYSize == 3
    assert out_ds.GetGeoTransform() == (0, 1, 0, 3, 0, -1)
    np.testing.assert_array_equal(
        out_ds.ReadAsArray(),
        np.array(np.array([[5, 8, 8], [5, 8, 8], [5, 5, 5]], dtype=np.uint8)),
    )


def test_gdal_calc_py_projwin(tmp_vsimem):

    input = tmp_vsimem / "in2.tif"
    out = tmp_vsimem / "out.tif"

    # input1: (0, 0) to (3, 3)
    with gdal.GetDriverByName("GTiff").Create(input, 3, 3, 1) as ds:
        ds.GetRasterBand(1).Fill(5)
        ds.SetGeoTransform((0, 1, 0, 3, 0, -1))

    # projwin does not behave the same as projWin in gdal.Translate
    # shrink the window a bit to get the (0, 4, 4, 0) that we actually want
    out_ds = gdal_calc.Calc(
        a=input, calc="2*a", outfile=out, projwin=(0, 4, 3.9999, 0.0001), quiet=True
    )

    assert out_ds.RasterXSize == 4
    assert out_ds.RasterYSize == 4
    assert out_ds.GetGeoTransform() == (0, 1, 0, 4, 0, -1)


def test_gdal_calc_py_projection_check(tmp_vsimem):

    input1 = tmp_vsimem / "in1.tif"
    input2 = tmp_vsimem / "in2.tif"
    out = tmp_vsimem / "out.tif"

    with gdal.GetDriverByName("GTiff").Create(input1, 3, 3, 1) as ds:
        ds.GetRasterBand(1).Fill(5)
        ds.SetGeoTransform((0, 1, 0, 3, 0, -1))
        assert ds.SetProjection("EPSG:4326") == gdal.CE_None

    with gdal.GetDriverByName("GTiff").Create(input2, 3, 3, 1) as ds:
        ds.GetRasterBand(1).Fill(3)
        ds.SetGeoTransform((1, 1, 0, 3, 0, -1))
        assert ds.SetProjection("EPSG:32145") == gdal.CE_None

    # Default: WGS84 + VT state plane? Why not?
    out_ds = gdal_calc.Calc(a=input1, b=input2, calc="a+b", outfile=out, quiet=True)
    assert out_ds.RasterXSize == 3
    assert out_ds.RasterYSize == 3
    assert np.all(out_ds.ReadAsArray() == 8)

    # Unless...
    with pytest.raises(Exception, match="Projection"):
        gdal_calc.Calc(
            a=input1,
            b=input2,
            calc="a+b",
            outfile=out,
            quiet=True,
            projectionCheck=True,
        )


def test_gdal_calc_py_mem_output(tmp_vsimem):

    input = tmp_vsimem / "in.tif"

    with gdal.GetDriverByName("GTiff").Create(input, 3, 3, 1) as ds:
        ds.GetRasterBand(1).Fill(5)

    out_ds = gdal_calc.Calc(A=input, calc="A+1", format="MEM", quiet=True)
    assert np.all(out_ds.ReadAsArray() == 6)


def test_gdal_calc_py_hidenodata(tmp_vsimem):

    input = tmp_vsimem / "in.tif"
    data = np.arange(9).reshape(3, 3)

    with gdal.GetDriverByName("GTiff").Create(input, 3, 3, 1) as ds:
        ds.GetRasterBand(1).WriteArray(data)
        ds.GetRasterBand(1).SetNoDataValue(7)

    out_ds = gdal_calc.Calc(A=input, calc="A+1", format="MEM", quiet=True)
    out_arr = out_ds.GetRasterBand(1).ReadAsMaskedArray()
    np.testing.assert_array_equal(out_arr.mask, data == 7)

    out_ds = gdal_calc.Calc(
        A=input, calc="A+1", format="MEM", hideNoData=True, quiet=True
    )
    out_arr = out_ds.GetRasterBand(1).ReadAsMaskedArray()
    assert not np.any(out_arr.mask)
    np.testing.assert_array_equal(out_arr, data + 1)
