#!/usr/bin/env python
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Helper functions for testing CLI utilities
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import sys

import gdaltest

cli_exe_path = {}

###############################################################################
#


def get_cli_utility_path_internal(cli_utility_name):

    if sys.platform == "win32":
        cli_utility_name += ".exe"

    build_dir = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))

    try:
        cli_utility_path = os.path.join(build_dir, "apps", cli_utility_name)
        if sys.platform == "win32":
            cli_utility_path = cli_utility_path.replace("\\", "/")
        if os.path.isfile(cli_utility_path):
            ret = gdaltest.runexternal(cli_utility_path + " --version")

            if "GDAL" in ret:
                return cli_utility_path
    except OSError:
        pass

    # Otherwise look up in the system path
    print(f"Could not find {cli_utility_name} in {build_dir}/apps. Trying with PATH")
    try:
        cli_utility_path = cli_utility_name
        ret = gdaltest.runexternal(cli_utility_path + " --version")

        if "GDAL" in ret:
            return cli_utility_path
    except OSError:
        pass

    return None


###############################################################################
#


def get_cli_utility_path(cli_utility_name):

    if cli_utility_name in cli_exe_path:
        return cli_exe_path[cli_utility_name]
    cli_exe_path[cli_utility_name] = get_cli_utility_path_internal(cli_utility_name)
    return cli_exe_path[cli_utility_name]


###############################################################################
#


def get_gdal_path():
    return get_cli_utility_path("gdal")


###############################################################################
#


def get_gdalinfo_path():
    return get_cli_utility_path("gdalinfo")


###############################################################################
#


def get_gdalmdiminfo_path():
    return get_cli_utility_path("gdalmdiminfo")


###############################################################################
#


def get_gdalmanage_path():
    return get_cli_utility_path("gdalmanage")


###############################################################################
#


def get_gdal_translate_path():
    return get_cli_utility_path("gdal_translate")


###############################################################################
#


def get_gdalmdimtranslate_path():
    return get_cli_utility_path("gdalmdimtranslate")


###############################################################################
#


def get_gdalwarp_path():
    return get_cli_utility_path("gdalwarp")


###############################################################################
#


def get_gdaladdo_path():
    return get_cli_utility_path("gdaladdo")


###############################################################################
#


def get_gdaltransform_path():
    return get_cli_utility_path("gdaltransform")


###############################################################################
#


def get_gdaltindex_path():
    return get_cli_utility_path("gdaltindex")


###############################################################################
#


def get_gdal_grid_path():
    return get_cli_utility_path("gdal_grid")


###############################################################################
#


def get_ogrinfo_path():
    return get_cli_utility_path("ogrinfo")


###############################################################################
#


def get_ogr2ogr_path():
    return get_cli_utility_path("ogr2ogr")


###############################################################################
#


def get_ogrtindex_path():
    return get_cli_utility_path("ogrtindex")


###############################################################################
#


def get_ogrlineref_path():
    return get_cli_utility_path("ogrlineref")


###############################################################################
#


def get_gdalbuildvrt_path():
    return get_cli_utility_path("gdalbuildvrt")


###############################################################################
#


def get_gdal_contour_path():
    return get_cli_utility_path("gdal_contour")


###############################################################################
#


def get_gdaldem_path():
    return get_cli_utility_path("gdaldem")


###############################################################################
#


def get_gdalenhance_path():
    return get_cli_utility_path("gdalenhance")


###############################################################################
#


def get_gdal_rasterize_path():
    return get_cli_utility_path("gdal_rasterize")


###############################################################################
#


def get_nearblack_path():
    return get_cli_utility_path("nearblack")


###############################################################################
#


def get_test_ogrsf_path():
    return get_cli_utility_path("test_ogrsf")


###############################################################################
#


def get_gdallocationinfo_path():
    return get_cli_utility_path("gdallocationinfo")


###############################################################################
#


def get_gdalsrsinfo_path():
    return get_cli_utility_path("gdalsrsinfo")


###############################################################################
#


def get_gnmmanage_path():
    return get_cli_utility_path("gnmmanage")


###############################################################################
#


def get_gnmanalyse_path():
    return get_cli_utility_path("gnmanalyse")


###############################################################################
#


def get_gdal_viewshed_path():
    return get_cli_utility_path("gdal_viewshed")


###############################################################################
#


def get_gdal_footprint_path():
    return get_cli_utility_path("gdal_footprint")
