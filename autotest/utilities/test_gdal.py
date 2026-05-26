#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal' program testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import os
import subprocess
import sys

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdal_path() is None, reason="gdal binary not available"
)


@pytest.fixture()
def gdal_path():
    return test_cli_utilities.get_gdal_path()


def test_gdal_no_argument(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(f"{gdal_path}")
    assert out == ""
    assert "gdal: Missing command name" in err
    assert "Usage: " in err
    assert "ret code = 1" in err


def test_gdal_help(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(f"{gdal_path} --help")
    assert out.startswith("Usage: ")
    assert err == ""


def test_gdal_json_usage(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(f"{gdal_path} --json-usage")
    assert out.startswith("{")
    assert err == ""
    assert "description" in json.loads(out)


def test_gdal_invalid_command_line(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(f"{gdal_path} --invalid")
    assert out == ""
    assert "Option '--invalid' is unknown" in err
    assert "Usage: " in err
    assert "ret code = 1" in err


def test_gdal_format_only(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(f"{gdal_path} --format MEM")
    assert "Short Name: MEM" in out
    assert err == ""


def test_gdal_format_as_output_format(gdal_path, tmp_path):

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster convert --format GTIFF ../gcore/data/byte.tif {tmp_path}/out.xxx"
    )
    assert out.startswith("0...10...20...30...40...50...60...70...80...90...100")
    assert err == ""
    assert gdal.VSIStatL(f"{tmp_path}/out.xxx") is not None


def test_gdal_format_as_output_format_quiet(gdal_path, tmp_path):

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster convert -q --format GTIFF ../gcore/data/byte.tif {tmp_path}/out.xxx"
    )
    assert not out.startswith("0...10...20...30...40...50...60...70...80...90...100")
    assert err == ""
    assert gdal.VSIStatL(f"{tmp_path}/out.xxx") is not None


def test_gdal_failure_during_run(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster info i_do_not_exist"
    )
    assert out == ""
    assert "i_do_not_exist:" in err
    assert "Usage: " in err
    assert "ret code = 1" in err


def test_gdal_success(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(f"{gdal_path} data/utmsmall.tif")
    assert err == ""
    assert out.startswith("Driver: GTiff/GeoTIFF")


def test_gdal_failure_during_finalize(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster convert ../gcore/data/byte.tif /vsimem/out.tif||maxlength=100"
    )
    assert "ret code = 1" in err


def test_gdal_config_not_serialized_to_gdalg(tmp_path, gdal_path):

    out_filename = str(tmp_path / "out.gdalg.json")
    gdaltest.runexternal(
        f"{gdal_path} raster reproject --config FOO=BAR ../gcore/data/byte.tif {out_filename}"
    )
    # Test that configuration option is not serialized
    j = json.loads(gdal.VSIFile(out_filename, "rb").read())
    assert "gdal_version" in j
    del j["gdal_version"]
    assert j == {
        "command_line": "gdal raster reproject --input ../gcore/data/byte.tif --output-format stream --output streamed_dataset",
        "type": "gdal_streamed_alg",
    }


def test_gdal_suggestions(gdal_path):

    _, err = gdaltest.runexternal_out_and_err(f"{gdal_path} rastr")
    assert "Algorithm 'rastr' is unknown. Do you mean 'raster'?" in err

    _, err = gdaltest.runexternal_out_and_err(f"{gdal_path} raster nifo")
    assert "Algorithm 'nifo' is unknown. Do you mean 'info'?" in err

    _, err = gdaltest.runexternal_out_and_err(f"{gdal_path} raster info --frmt=json")
    assert "Option '--frmt' is unknown. Do you mean '--format'?" in err


def test_gdal_completion(gdal_path):

    out = gdaltest.run_and_parse_completion_output(f"{gdal_path} completion gdal -")
    assert "--version" in out

    out = gdaltest.run_and_parse_completion_output(f"{gdal_path} completion gdal")
    assert "convert" in out
    assert "info" in out
    assert "raster" in out
    assert "vector" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster"
    )
    assert "convert" in out
    assert "edit" in out
    assert "info" in out
    assert "reproject" in out
    assert "pipeline" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster info -"
    )
    assert "-f" not in out
    assert "--of" not in out
    assert "--format" not in out
    assert "--output-format" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster info --"
    )
    assert "-f" not in out
    assert "--of" not in out
    assert "--format" not in out
    assert "--output-format" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster info --o"
    )
    assert "--of" in out
    assert "--output-format" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster info --form"
    )
    assert out == ["--format"]

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster info --of"
    )
    assert "json" in out
    assert "text" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster info --of="
    )
    assert "json" in out
    assert "text" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster info --of=t"
    )
    assert "json" in out
    assert "text" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert --of"
    )
    if gdal.GetDriverByName("GTiff"):
        assert "GTiff" in out
    if gdal.GetDriverByName("HFA"):
        assert "HFA" in out

    # Test that config options are taken into account
    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert --config GDAL_SKIP=GTiff --of"
    )
    assert "GTiff" not in out
    if gdal.GetDriverByName("HFA"):
        assert "HFA" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert --input"
    )
    assert "data/" in out or "data\\" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert --input data/"
    )
    assert "data/whiteblackred.tif" in out or "data\\whiteblackred.tif" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert /vsizip/../gcore/data/byte.tif.zip/"
    )
    assert out == ["/vsizip/../gcore/data/byte.tif.zip/byte.tif"]

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster reproject --resolution"
    )
    assert out == [
        "**",
        "\xc2\xa0description: Target resolution (in destination CRS units)",
    ]

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert --input /vsi"
    )
    assert "/vsimem/" in out

    # Just run it. Result will depend on local configuration
    gdaltest.runexternal(f"{gdal_path} completion gdal raster convert --input /vsis3/")


def test_gdal_completion_co(gdal_path):

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co"
    )
    assert "COMPRESS=" in out
    assert "RPCTXT=" in out
    assert "TILING_SCHEME=" not in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co="
    )
    assert "COMPRESS=" in out
    assert "RPCTXT=" in out
    assert "TILING_SCHEME=" not in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co COMPRESS="
    )
    assert "NONE" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co COMPRESS=NO"
    )
    assert "NONE" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co=COMPRESS="
    )
    assert "NONE" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co=COMPRESS=NO"
    )
    assert "NONE" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co TILED="
    )
    assert out == ["NO", "YES"]

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co ZLEVEL="
    )
    assert "1" in out
    assert "9" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert --of CO in.tif out.tif prev=of cur=CO"
    )
    assert "COG" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert --of=CO in.tif out.tif prev== cur=CO"
    )
    assert "COG" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert --of COG --co="
    )
    assert "COMPRESS=" in out
    assert "RPCTXT=" not in out
    assert "TILING_SCHEME=" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert --of=COG --creation-option ZOOM_LEVEL="
    )
    assert out == [
        "##",
        "type: int, description: Target zoom level. Only used for TILING_SCHEME != CUSTOM",
    ]

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert --of COG --co BLOCKSIZE="
    )
    assert out == ["##", "validity range: >= 128"]

    if "JPEG_QUALITY" in gdal.GetDriverByName("GTiff").GetMetadataItem(
        "DMD_CREATIONOPTIONLIST"
    ):
        out = gdaltest.run_and_parse_completion_output(
            f"{gdal_path} completion gdal raster convert in.tif out.tif --co JPEG_QUALITY="
        )
        assert out == ["##", "validity range: [1,100]"]

    if gdal.GetDriverByName("GPKG"):
        out = gdaltest.run_and_parse_completion_output(
            f"{gdal_path} completion gdal raster convert --of GPKG --co="
        )
        assert "APPEND_SUBDATASET=" in out
        assert "VERSION=" in out

        out = gdaltest.run_and_parse_completion_output(
            f"{gdal_path} completion gdal vector convert --of GPKG --co="
        )
        assert "APPEND_SUBDATASET=" not in out
        assert "VERSION=" in out

        out = gdaltest.run_and_parse_completion_output(
            f"{gdal_path} completion gdal raster convert --of GPKG --co BLOCKSIZE="
        )
        assert out == ["##", "validity range: <= 4096"]


def test_gdal_completion_lco(gdal_path):

    if gdal.GetDriverByName("GPKG"):

        out = gdaltest.run_and_parse_completion_output(
            f"{gdal_path} completion gdal vector convert --of GPKG --lco"
        )
        assert "FID=" in out

        out = gdaltest.run_and_parse_completion_output(
            f"{gdal_path} completion gdal vector convert in.shp out.gpkg --layer-creation-option="
        )
        assert "FID=" in out


def test_gdal_completion_oo(gdal_path):

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster info foo.tif --oo"
    )
    assert "COLOR_TABLE_MULTIPLIER=" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster info --if GTiff --open-option"
    )
    assert "COLOR_TABLE_MULTIPLIER=" in out


def test_gdal_completion_dst_crs(gdal_path):

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster reproject --dst-crs"
    )
    assert "EPSG:" in out
    assert "ESRI:" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster reproject --dst-crs EPSG:"
    )
    assert "4326 -- WGS 84 (geographic 2D)" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster reproject --dst-crs=EPSG:"
    )
    assert "4326 -- WGS 84 (geographic 2D)" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster reproject --dst-crs EPSG:43"
    )
    assert "4326 -- WGS 84 (geographic 2D)" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster reproject --dst-crs=EPSG:43"
    )
    assert "4326 -- WGS 84 (geographic 2D)" in out

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster reproject --dst-crs EPSG:4326"
    )
    assert out == ["4326"]


def test_gdal_completion_config(gdal_path):

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} completion gdal raster convert --config"
    )
    assert "CPL_DEBUG=" in out
    assert err == ""

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} completion gdal raster convert --config="
    )
    assert "CPL_DEBUG=" in out
    assert err == ""

    out, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} completion gdal raster convert --config CPL_"
    )
    assert "CPL_DEBUG=" in out
    assert err == ""

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert --config FOO="
    )
    assert out == [""]

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal raster convert --config=FOO="
    )
    assert out == [""]


@pytest.mark.parametrize("subcommand", [None, "raster", "vector"])
def test_gdal_completion_pipeline(gdal_path, subcommand):

    pipeline_cmd = f"{gdal_path} completion gdal"
    if subcommand:
        pipeline_cmd += " "
        pipeline_cmd += subcommand
    pipeline_cmd += " pipeline"

    out = gdaltest.run_and_parse_completion_output(f"{pipeline_cmd}")
    if subcommand == "raster":
        assert "calc" in out
        assert "concat" not in out
    elif subcommand == "vector":
        assert "calc" not in out
        assert "concat" in out
    assert "read" in out
    assert "write" not in out

    out = gdaltest.run_and_parse_completion_output(f"{pipeline_cmd} re")
    assert out == ["read"]

    out = gdaltest.run_and_parse_completion_output(f"{pipeline_cmd} read")
    assert out == [""]

    out = gdaltest.run_and_parse_completion_output(f"{pipeline_cmd} read -")
    assert "--input" in out
    assert "--open-option" in out
    if subcommand == "raster":
        assert "--input-layer" not in out
    else:
        assert "--input-layer" in out

    out = gdaltest.run_and_parse_completion_output(f"{pipeline_cmd} read !")
    assert "reproject" in out
    assert "write" in out
    assert "read" not in out

    out = gdaltest.run_and_parse_completion_output(f"{pipeline_cmd} read ! re")
    assert "reproject" in out
    assert "write" in out
    assert "read" not in out

    out = gdaltest.run_and_parse_completion_output(f"{pipeline_cmd} read foo ! write -")
    assert "--output" in out
    assert "--creation-option" in out

    if subcommand != "vector":
        out = gdaltest.run_and_parse_completion_output(
            f"{pipeline_cmd} read foo ! foo ! reproject -"
        )
        assert "--resampling" in out

        out = gdaltest.run_and_parse_completion_output(
            f"{pipeline_cmd} read foo ! reproject --resampling"
        )
        assert "nearest" in out

        out = gdaltest.run_and_parse_completion_output(
            f"{pipeline_cmd} read ../gcore/data/byte.tif !"
        )
        assert "resize" in out
        assert "make-valid" not in out
        if subcommand is None:
            assert "polygonize" in out
        else:
            assert "polygonize" not in out

        out = gdaltest.run_and_parse_completion_output(
            f"{pipeline_cmd} read ../gcore/data/byte.tif ! edit -"
        )
        assert "--nodata" in out
        assert "--geometry-type" not in out

    if subcommand != "raster":

        out = gdaltest.run_and_parse_completion_output(
            f"{pipeline_cmd} read test_gdal.py -"
        )
        assert "--input-layer" in out

        out = gdaltest.run_and_parse_completion_output(
            f"{pipeline_cmd} read ../ogr/data/poly.shp ! reproject -"
        )
        assert "--active-layer" in out

        out = gdaltest.run_and_parse_completion_output(
            f"{pipeline_cmd} read foo ! reproject --dst-crs"
        )
        assert "EPSG:" in out

        out = gdaltest.run_and_parse_completion_output(
            f"{pipeline_cmd} read ../ogr/data/poly.shp !"
        )
        assert "resize" not in out
        assert "make-valid" in out
        if subcommand is None:
            assert "rasterize" in out
        else:
            assert "rasterize" not in out

        out = gdaltest.run_and_parse_completion_output(
            f"{pipeline_cmd} read ../ogr/data/poly.shp ! edit -"
        )
        assert "--nodata" not in out
        assert "--geometry-type" in out


def test_gdal_completion_gdal_vector_info_layer(gdal_path):

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal vector info ../ogr/data/poly.shp --layer"
    )
    assert out == ["poly"]

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal vector info ../ogr/data/poly.shp --layer p"
    )
    assert out == ["poly"]

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal vector info ../ogr/data/poly.shp --layer poly"
    )
    assert out == [""]

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal vector info ../ogr/data/poly.shp --layer poly XX"
    )
    assert out == [""]


def test_gdal_completion_gdal_vector_pipeline_read_layer(gdal_path):

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal vector pipeline read ../ogr/data/poly.shp --layer"
    )
    assert out == ["poly"]


def test_gdal_question_mark(gdal_path):

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} vector info ../ogr/data/poly.shp --layer=?"
    )
    assert "Single potential value for argument 'input-layer' is 'poly'" in err

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} vector pipeline read ../ogr/data/poly.shp --layer=?"
    )
    assert "Single potential value for argument 'input-layer' is 'poly'" in err

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster reproject --resampling=?"
    )
    assert (
        "Potential values for argument 'resampling' are:\n- nearest\n- bilinear"
        in err.replace("\r\n", "\n")
    )

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdal_path} raster pipeline read ../gcore/data/byte.tif ! reproject --resampling=?"
    )
    assert (
        "Potential values for argument 'resampling' are:\n- nearest\n- bilinear"
        in err.replace("\r\n", "\n")
    )


def test_gdal_algorithm_getter_setter():

    with pytest.raises(
        Exception, match="'not_existing' is not a valid sub-algorithm of 'raster'"
    ):
        gdal.GetGlobalAlgorithmRegistry()["raster"]["not_existing"]

    alg = gdal.GetGlobalAlgorithmRegistry()["raster"]["info"]

    with pytest.raises(
        Exception, match="'not_existing' is not a valid argument of 'info'"
    ):
        alg["not_existing"]

    with pytest.raises(
        Exception, match="'not_existing' is not a valid argument of 'info'"
    ):
        alg["not_existing"] = "bar"

    with pytest.raises(Exception):
        alg["no-mask"] = "bar"


def test_gdal_algorithm():

    with pytest.raises(RuntimeError, match="'i_do_not_exist' is not a valid algorithm"):
        gdal.Algorithm("i_do_not_exist")

    with pytest.raises(RuntimeError, match="'i_do_not_exist' is not a valid algorithm"):
        gdal.Algorithm(["i_do_not_exist"])

    with pytest.raises(
        RuntimeError, match="'i_do_not_exist' is not a valid sub-algorithm"
    ):
        gdal.Algorithm(["raster", "i_do_not_exist"])

    with pytest.raises(
        RuntimeError,
        match="Wrong type for algorithm path. Expected string or list of strings",
    ):
        gdal.Algorithm(None)

    with pytest.raises(
        RuntimeError,
        match="Wrong type for algorithm path. Expected string or list of strings",
    ):
        gdal.Algorithm()

    alg = gdal.Algorithm(["raster", "info"])
    assert alg.GetName() == "info"

    alg = gdal.Algorithm("raster info")
    assert alg.GetName() == "info"

    alg = gdal.Algorithm("raster", "info")
    assert alg.GetName() == "info"

    with pytest.raises(RuntimeError, match=r"Algorithm.Run\(\) must be called before"):
        alg.Output()

    with pytest.raises(RuntimeError, match=r"Algorithm.Run\(\) must be called before"):
        alg.Output()

    with gdal.Algorithm("raster info") as alg:
        assert alg.GetName() == "info"


def test_gdal_run():

    with pytest.raises(RuntimeError, match="'i_do_not_exist' is not a valid algorithm"):
        with gdal.Run("i_do_not_exist"):
            pass

    with pytest.raises(RuntimeError, match="'i_do_not_exist' is not a valid algorithm"):
        with gdal.Run(["i_do_not_exist"]):
            pass

    with pytest.raises(
        RuntimeError, match="'i_do_not_exist' is not a valid sub-algorithm"
    ):
        with gdal.Run(["raster", "i_do_not_exist"]):
            pass

    with pytest.raises(
        RuntimeError,
        match="Wrong type for alg. Expected string, list of strings or Algorithm",
    ):
        gdal.Run(None)

    with gdal.Run("raster", "info", {"input": "../gcore/data/byte.tif"}) as alg:
        assert len(alg.Output()["bands"]) == 1

    with gdal.Run("raster info", {"input": "../gcore/data/byte.tif"}) as alg:
        assert len(alg.Output()["bands"]) == 1

    with gdal.Run("gdal raster info", input="../gcore/data/byte.tif") as alg:
        assert len(alg.Output()["bands"]) == 1

    with gdal.Run(
        gdal.Algorithm("raster info"), {"input": "../gcore/data/byte.tif"}
    ) as alg:
        assert len(alg.Output()["bands"]) == 1

    alg = gdal.Run("raster info", {"input": "../gcore/data/byte.tif"})
    assert len(alg.Outputs()) == 1
    assert alg.Outputs(parse_json=False)["output-string"].startswith("{")

    with gdal.Run(
        ["gdal", "raster", "reproject"],
        input="../gcore/data/byte.tif",
        output_format="MEM",
        dst_crs="EPSG:4326",
    ) as alg:
        assert alg.Output().GetSpatialRef().GetAuthorityCode() == "4326"

    with gdal.Run(
        ["raster", "reproject"],
        {
            "input": "../gcore/data/byte.tif",
            "output-format": "MEM",
            "dst-crs": "EPSG:4326",
        },
    ) as alg:
        assert alg.Output().GetSpatialRef().GetAuthorityCode() == "4326"


def test_gdal_drivers():

    with gdal.Run([], drivers=True) as alg:
        j = alg.Output()
        assert "GTiff" in [x["short_name"] for x in j]
        assert "ESRI Shapefile" in [x["short_name"] for x in j]


@pytest.mark.parametrize(
    "shell,input_args,last_arg_is_complete,expected_output",
    [
        (
            "bash",
            [],
            True,
            [
                "convert",
                "dataset",
                "driver",
                "info",
                "mdim",
                "pipeline",
                "raster",
                "vector",
                "vsi",
            ],
        ),
        (
            "zsh",
            [],
            True,
            [
                "convert",
                "dataset",
                "driver",
                "info",
                "mdim",
                "pipeline",
                "raster",
                "vector",
                "vsi",
            ],
        ),
        (
            "bash",
            ["raster", "convert", "--"],
            False,
            [
                "--append",
                "--creation-option",
                "--input",
                "--input-format",
                "--open-option",
                "--output",
                "--output-format",
                "--overwrite",
                "--quiet",
            ],
        ),
        (
            "zsh",
            ["raster", "convert", "--"],
            False,
            [
                "--append",
                "--creation-option",
                "--input",
                "--input-format",
                "--open-option",
                "--output",
                "--output-format",
                "--overwrite",
                "--quiet",
            ],
        ),
        (
            "bash",
            ["vector", "filter", "../ogr/data/poly.shp", "--where", '"'],
            False,
            ['"AREA', '"EAS_ID', '"PRFEDEA'],
        ),
        (
            "zsh",
            ["vector", "filter", "../ogr/data/poly.shp", "--where", '"'],
            False,
            ["AREA", "EAS_ID", "PRFEDEA"],
        ),
        ("bash", ["vector", "pipeline", "read", "!"], False, ["! info"]),
        ("zsh", ["vector", "pipeline", "read", "!"], False, ["! info"]),
        (
            "bash",
            ["raster", "convert", "--format=GTiff", "--creation-option"],
            True,
            ["COMPRESS="],
        ),
        (
            "zsh",
            ["raster", "convert", "--format=GTiff", "--creation-option"],
            True,
            ["COMPRESS="],
        ),
        ("bash", ["raster", "reproject", "--output-crs"], True, ["EPSG:"]),
        ("zsh", ["raster", "reproject", "--output-crs"], True, ["EPSG:"]),
    ],
)
def test_gdal_completion_shell(
    shell, input_args, last_arg_is_complete, expected_output
):
    """Test running bash/zsh completion script"""

    if sys.platform == "win32":
        pytest.skip("not compatible of win32")

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary not available")

    gdal_bash_completion = os.path.join(
        os.getcwd(), "..", "..", "scripts", "gdal-bash-completion.sh"
    )
    if not os.path.exists(gdal_bash_completion):
        pytest.skip(f"cannot find {gdal_bash_completion}")

    bash_completion = "/usr/share/bash-completion/bash_completion"
    if shell == "bash" and not os.path.exists(bash_completion):
        pytest.skip(f"cannot find {bash_completion}")

    input_args = [gdal_path] + input_args

    command_line = " ".join(input_args)
    command_line_escaped = command_line.replace("\\", "\\\\").replace('"', '\\"')
    COMP_CWORD = len(input_args)
    if not last_arg_is_complete:
        COMP_CWORD -= 1
    if shell == "zsh":
        # /usr/share/zsh/functions/Completion/bashcompinit does that
        # This set start array offset=0 and split variables on space
        cmd = "emulate -L sh\n"
    else:
        cmd = f"source {bash_completion}\n"

    cmd += f"""
source {gdal_bash_completion}

# Set env variables involved in completion
COMP_LINE="{command_line_escaped}"
COMP_POINT={len(command_line)}
COMP_WORDS=({command_line_escaped})
COMP_CWORD={COMP_CWORD}

# Execute the completion function
_gdal

for line in "${{COMPREPLY[@]}}"; do
  echo $line;
done
"""

    try:
        result = subprocess.run([shell, "-c", cmd], capture_output=True, text=True)

        output = result.stdout.strip().split("\n")
        for x in expected_output:
            assert x in output
    except FileNotFoundError:
        pytest.skip(f"{shell} not available")
