#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal crs info' testing
# Author:   Alessandro Pasotti <elpaso at itopen dot it>
#
###############################################################################
# Copyright (c) 2026, Alessandro Pasotti <elpaso at itopen dot it>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal


def get_info_alg():
    return gdal.GetGlobalAlgorithmRegistry()["crs"]["info"]


@pytest.mark.parametrize(
    "args,expected_output",
    [
        # Single output format: no format name printed
        (
            ["EPSG:3857", "-o", "proj4"],
            "+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +nadgrids=@null +wktext",
        ),
        # Multiple output formats: format name printed
        (["EPSG:3857"], "\nPROJ.4 : +proj=merc +a=6378137"),
        # Multiple output single-line format: format name not printed
        (
            ["EPSG:3857", "-o", "wkt_all", "--single-line"],
            'PROJCS["WGS 84 / Pseudo-Mercator",GEOGCS["WGS 84"',
        ),
    ],
)
def test_gdalalg_crs_info(args, expected_output):
    info = get_info_alg()
    assert info.ParseRunAndFinalize(args)
    output_string = info["output-string"]
    assert output_string.startswith(expected_output)


@pytest.mark.parametrize(
    "args,expected_code",
    [
        (["EPSG:3857"], 0),
        (["EPSG:3857", "-o", "xml"], 1),
        (["INVALID_CRS"], 1),
    ],
)
def test_gdalalg_crs_info_return_code(args, expected_code):
    info = get_info_alg()
    with gdaltest.disable_exceptions(), gdal.quiet_errors():
        ret = info.ParseRunAndFinalize(args)
    assert ret if expected_code == 0 else not ret


@pytest.mark.parametrize(
    "args,expected_output",
    [
        (
            ["EPSG:3857", "--identify", "-o", "proj4"],
            "\nEPSG:3857\n+proj=merc +a=6378137 +b=6378137",
        ),
        (
            [
                """PROJCS["ETRS89 / UTM zone 30N not exact name",GEOGCS["ETRS89",DATUM["European_Terrestrial_Reference_System_1989",SPHEROID["GRS 1980",6378137,298.257222101,AUTHORITY["EPSG","7019"]],AUTHORITY["EPSG","6258"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4258"]],PROJECTION["Transverse_Mercator"], PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-3],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1, AUTHORITY["EPSG","9001"]]]""",
                "--identify",
                "-o",
                "proj4",
            ],
            "Confidence in this match: 70\n\nEPSG:25830",
        ),
    ],
)
def test_gdalalg_crs_info_identify(args, expected_output):
    info = get_info_alg()
    assert info.ParseRunAndFinalize(args)
    output_string = info["output-string"]
    assert output_string.startswith(expected_output)
