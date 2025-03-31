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
    assert "Long name option '--invalid' is unknown" in err
    assert "Usage: " in err
    assert "ret code = 1" in err


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
    assert "description" in json.loads(out)


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
    assert json.loads(gdal.VSIFile(out_filename, "rb").read()) == {
        "command_line": "gdal raster reproject --input ../gcore/data/byte.tif --output-format stream --output streamed_dataset",
        "type": "gdal_streamed_alg",
    }


def test_gdal_completion(gdal_path):

    out = gdaltest.runexternal(f"{gdal_path} completion gdal -").split(" ")
    assert "--version" in out

    out = gdaltest.runexternal(f"{gdal_path} completion gdal").split(" ")
    assert "convert" in out
    assert "info" in out
    assert "raster" in out
    assert "vector" in out

    out = gdaltest.runexternal(f"{gdal_path} completion gdal raster").split(" ")
    assert "convert" in out
    assert "edit" in out
    assert "info" in out
    assert "reproject" in out
    assert "pipeline" in out

    out = gdaltest.runexternal(f"{gdal_path} completion gdal raster info -").split(" ")
    assert "-f" in out
    assert "--of" in out
    assert "--output-format" in out

    out = gdaltest.runexternal(f"{gdal_path} completion gdal raster info --of").split(
        " "
    )
    assert "json" in out
    assert "text" in out

    out = gdaltest.runexternal(f"{gdal_path} completion gdal raster info --of=").split(
        " "
    )
    assert "json" in out
    assert "text" in out

    out = gdaltest.runexternal(f"{gdal_path} completion gdal raster info --of=t").split(
        " "
    )
    assert "json" in out
    assert "text" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert --of"
    ).split(" ")
    if gdal.GetDriverByName("GTiff"):
        assert "GTiff" in out
    if gdal.GetDriverByName("HFA"):
        assert "HFA" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert --input"
    ).split(" ")
    assert "data/" in out or "data\\" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert --input data/"
    ).split(" ")
    assert "data/whiteblackred.tif" in out or "data\\whiteblackred.tif" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert /vsizip/../gcore/data/byte.tif.zip/"
    ).split(" ")
    assert out == ["/vsizip/../gcore/data/byte.tif.zip/byte.tif"]

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster reproject --resolution"
    )
    assert (
        out
        == "** description:\\ Target\\ resolution\\ (in\\ destination\\ CRS\\ units)"
    )


def test_gdal_completion_co(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co"
    ).split(" ")
    assert "COMPRESS=" in out
    assert "RPCTXT=" in out
    assert "TILING_SCHEME=" not in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co="
    ).split(" ")
    assert "COMPRESS=" in out
    assert "RPCTXT=" in out
    assert "TILING_SCHEME=" not in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co COMPRESS="
    ).split(" ")
    assert "NONE" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co COMPRESS=NO"
    ).split(" ")
    assert "NONE" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co=COMPRESS="
    ).split(" ")
    assert "NONE" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co=COMPRESS=NO"
    ).split(" ")
    assert "NONE" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co TILED="
    ).split(" ")
    assert out == ["NO", "YES"]

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert in.tif out.tif --co ZLEVEL="
    ).split(" ")
    assert "1" in out
    assert "9" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert --of COG --co="
    ).split(" ")
    assert "COMPRESS=" in out
    assert "RPCTXT=" not in out
    assert "TILING_SCHEME=" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert --of=COG --creation-option ZOOM_LEVEL="
    )
    assert (
        out
        == "## type:\\ int,\\ description:\\ Target\\ zoom\\ level.\\ Only\\ used\\ for\\ TILING_SCHEME\\ !=\\ CUSTOM"
    )

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert --of COG --co BLOCKSIZE="
    )
    assert out == "## validity\\ range:\\ >=\\ 128"

    if "JPEG_QUALITY" in gdal.GetDriverByName("GTiff").GetMetadataItem(
        "DMD_CREATIONOPTIONLIST"
    ):
        out = gdaltest.runexternal(
            f"{gdal_path} completion gdal raster convert in.tif out.tif --co JPEG_QUALITY="
        )
        assert out == "## validity\\ range:\\ [1,100]"

    if gdal.GetDriverByName("GPKG"):
        out = gdaltest.runexternal(
            f"{gdal_path} completion gdal raster convert --of GPKG --co="
        ).split(" ")
        assert "APPEND_SUBDATASET=" in out
        assert "VERSION=" in out

        out = gdaltest.runexternal(
            f"{gdal_path} completion gdal vector convert --of GPKG --co="
        ).split(" ")
        assert "APPEND_SUBDATASET=" not in out
        assert "VERSION=" in out

        out = gdaltest.runexternal(
            f"{gdal_path} completion gdal raster convert --of GPKG --co BLOCKSIZE="
        )
        assert out == "## validity\\ range:\\ <=\\ 4096"


def test_gdal_completion_lco(gdal_path):

    if gdal.GetDriverByName("GPKG"):

        out = gdaltest.runexternal(
            f"{gdal_path} completion gdal vector convert --of GPKG --lco"
        ).split(" ")
        assert "FID=" in out

        out = gdaltest.runexternal(
            f"{gdal_path} completion gdal vector convert in.shp out.gpkg --layer-creation-option="
        ).split(" ")
        assert "FID=" in out


def test_gdal_completion_oo(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster info foo.tif --oo"
    ).split(" ")
    assert "COLOR_TABLE_MULTIPLIER=" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster info --if GTiff --open-option"
    ).split(" ")
    assert "COLOR_TABLE_MULTIPLIER=" in out


def test_gdal_completion_dst_crs(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster reproject --dst-crs"
    ).split(" ")
    assert "EPSG:" in out
    assert "ESRI:" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster reproject --dst-crs EPSG:"
    )
    assert "4326\\ --\\ WGS\\ 84" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster reproject --dst-crs=EPSG:"
    )
    assert "4326\\ --\\ WGS\\ 84" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster reproject --dst-crs EPSG:43"
    )
    assert "4326\\ --\\ WGS\\ 84" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster reproject --dst-crs=EPSG:43"
    )
    assert "4326\\ --\\ WGS\\ 84" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster reproject --dst-crs EPSG:4326"
    )
    assert out == "4326"


def test_gdal_completion_config(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert --config"
    ).split(" ")
    assert "CPL_DEBUG=" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert --config="
    ).split(" ")
    assert "CPL_DEBUG=" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert --config FOO="
    ).split(" ")
    assert out == [""]

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal raster convert --config=FOO="
    ).split(" ")
    assert out == [""]


@pytest.mark.parametrize("subcommand", ["raster", "vector"])
def test_gdal_completion_pipeline(gdal_path, subcommand):

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal {subcommand} pipeline"
    ).split(" ")
    assert out == ["read"]

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal {subcommand} pipeline re"
    ).split(" ")
    assert out == ["read"]

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal {subcommand} pipeline read"
    ).split(" ")
    assert out == [""]

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal {subcommand} pipeline read -"
    ).split(" ")
    assert "--input" in out
    assert "--open-option" in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal {subcommand} pipeline read !"
    ).split(" ")
    assert "reproject" in out
    assert "write" in out
    assert "read" not in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal {subcommand} pipeline read ! re"
    ).split(" ")
    assert "reproject" in out
    assert "write" in out
    assert "read" not in out

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal {subcommand} pipeline read foo ! write -"
    ).split(" ")
    assert "--output" in out
    assert "--co" in out

    if subcommand == "raster":
        out = gdaltest.runexternal(
            f"{gdal_path} completion gdal {subcommand} pipeline read foo ! foo ! reproject -"
        ).split(" ")
        assert "--resampling" in out

        out = gdaltest.runexternal(
            f"{gdal_path} completion gdal {subcommand} pipeline read foo ! reproject --resampling"
        ).split(" ")
        assert "nearest" in out
    else:
        out = gdaltest.runexternal(
            f"{gdal_path} completion gdal {subcommand} pipeline read foo ! geom"
        ).split(" ")
        assert "set-type" in out
