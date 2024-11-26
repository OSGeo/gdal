#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalinfo.py testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil

import gdaltest
import pytest
import test_py_scripts

pytestmark = pytest.mark.skipif(
    test_py_scripts.get_py_script("gdalinfo") is None,
    reason="gdalinfo.py not available",
)


@pytest.fixture()
def script_path():
    return test_py_scripts.get_py_script("gdalinfo")


###############################################################################
# Simple test


def test_gdalinfo_py_1(script_path):

    ret = test_py_scripts.run_py_script(
        script_path, "gdalinfo", test_py_scripts.get_data_path("gcore") + "byte.tif"
    )
    assert ret.find("Driver: GTiff/GeoTIFF") != -1


###############################################################################
# Test -checksum option


def test_gdalinfo_py_2(script_path):

    ret = test_py_scripts.run_py_script(
        script_path,
        "gdalinfo",
        "-checksum " + test_py_scripts.get_data_path("gcore") + "byte.tif",
    )
    assert ret.find("Checksum=4672") != -1


###############################################################################
# Test -nomd option


def test_gdalinfo_py_3(script_path):

    ret = test_py_scripts.run_py_script(
        script_path, "gdalinfo", test_py_scripts.get_data_path("gcore") + "byte.tif"
    )
    assert ret.find("Metadata") != -1

    ret = test_py_scripts.run_py_script(
        script_path,
        "gdalinfo",
        "-nomd " + test_py_scripts.get_data_path("gcore") + "byte.tif",
    )
    assert ret.find("Metadata") == -1


###############################################################################
# Test -noct option


@pytest.mark.require_driver("GIF")
def test_gdalinfo_py_4(script_path):

    ret = test_py_scripts.run_py_script(
        script_path,
        "gdalinfo",
        test_py_scripts.get_data_path("gdrivers") + "gif/bug407.gif",
    )
    assert ret.find("0: 255,255,255,255") != -1

    ret = test_py_scripts.run_py_script(
        script_path,
        "gdalinfo",
        "-noct " + test_py_scripts.get_data_path("gdrivers") + "gif/bug407.gif",
    )
    assert ret.find("0: 255,255,255,255") == -1


###############################################################################
# Test -stats option


def test_gdalinfo_py_5(script_path):

    tmpfilename = "tmp/test_gdalinfo_py_5.tif"
    if os.path.exists(tmpfilename + ".aux.xml"):
        os.remove(tmpfilename + ".aux.xml")
    shutil.copy(test_py_scripts.get_data_path("gcore") + "byte.tif", tmpfilename)

    ret = test_py_scripts.run_py_script(script_path, "gdalinfo", tmpfilename)
    assert ret.find("STATISTICS_MINIMUM=74") == -1, "got wrong minimum."

    ret = test_py_scripts.run_py_script(
        script_path, "gdalinfo", "-stats " + tmpfilename
    )
    assert ret.find("STATISTICS_MINIMUM=74") != -1, "got wrong minimum (2)."

    # We will blow an exception if the file does not exist now!
    os.remove(tmpfilename + ".aux.xml")
    os.remove(tmpfilename)


###############################################################################
# Test a dataset with overviews and RAT


@pytest.mark.require_driver("HFA")
def test_gdalinfo_py_6(script_path):

    ret = test_py_scripts.run_py_script(
        script_path,
        "gdalinfo",
        test_py_scripts.get_data_path("gdrivers") + "hfa/int.img",
    )
    assert ret.find("Overviews") != -1


###############################################################################
# Test a dataset with GCPs


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_gdalinfo_py_7(script_path):

    ret = test_py_scripts.run_py_script(
        script_path, "gdalinfo", test_py_scripts.get_data_path("gcore") + "gcps.vrt"
    )
    assert ret.find("GCP Projection =") != -1
    assert ret.find('PROJCS["NAD27 / UTM zone 11N"') != -1
    assert ret.find("(100,100) -> (446720,3745320,0)") != -1

    # Same but with -nogcps
    ret = test_py_scripts.run_py_script(
        script_path,
        "gdalinfo",
        "-nogcp " + test_py_scripts.get_data_path("gcore") + "gcps.vrt",
    )
    assert ret.find("GCP Projection =") == -1
    assert ret.find('PROJCS["NAD27 / UTM zone 11N"') == -1
    assert ret.find("(100,100) -> (446720,3745320,0)") == -1


###############################################################################
# Test -hist option


def test_gdalinfo_py_8(script_path):

    tmpfilename = "tmp/test_gdalinfo_py_8.tif"
    if os.path.exists(tmpfilename + ".aux.xml"):
        os.remove(tmpfilename + ".aux.xml")
    shutil.copy(test_py_scripts.get_data_path("gcore") + "byte.tif", tmpfilename)

    ret = test_py_scripts.run_py_script(script_path, "gdalinfo", tmpfilename)
    assert (
        ret.find(
            "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 6 0 0 0 0 0 0 0 0 37 0 0 0 0 0 0 0 57 0 0 0 0 0 0 0 62 0 0 0 0 0 0 0 66 0 0 0 0 0 0 0 0 72 0 0 0 0 0 0 0 31 0 0 0 0 0 0 0 24 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 0 7 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 5 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1"
        )
        == -1
    ), "did not expect histogram."

    ret = test_py_scripts.run_py_script(script_path, "gdalinfo", "-hist " + tmpfilename)
    assert (
        ret.find(
            "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 6 0 0 0 0 0 0 0 0 37 0 0 0 0 0 0 0 57 0 0 0 0 0 0 0 62 0 0 0 0 0 0 0 66 0 0 0 0 0 0 0 0 72 0 0 0 0 0 0 0 31 0 0 0 0 0 0 0 24 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 0 7 0 0 0 0 0 0 0 12 0 0 0 0 0 0 0 5 0 0 0 0 0 0 0 3 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1"
        )
        != -1
    ), "did not get expected histogram."

    # We will blow an exception if the file does not exist now!
    os.remove(tmpfilename + ".aux.xml")
    os.remove(tmpfilename)


###############################################################################
# Test -mdd option


@pytest.mark.require_driver("NITF")
def test_gdalinfo_py_9(script_path):

    ret = test_py_scripts.run_py_script(
        script_path,
        "gdalinfo",
        test_py_scripts.get_data_path("gdrivers") + "nitf/fake_nsif.ntf",
    )
    assert ret.find("BLOCKA=010000001000000000") == -1, "Got unexpected extra MD."

    ret = test_py_scripts.run_py_script(
        script_path,
        "gdalinfo",
        "-mdd TRE " + test_py_scripts.get_data_path("gdrivers") + "nitf/fake_nsif.ntf",
    )
    assert ret.find("BLOCKA=010000001000000000") != -1, "did not get extra MD."


###############################################################################
# Test -mm option


def test_gdalinfo_py_10(script_path):

    ret = test_py_scripts.run_py_script(
        script_path, "gdalinfo", test_py_scripts.get_data_path("gcore") + "byte.tif"
    )
    assert ret.find("Computed Min/Max=74.000,255.000") == -1

    ret = test_py_scripts.run_py_script(
        script_path,
        "gdalinfo",
        "-mm " + test_py_scripts.get_data_path("gcore") + "byte.tif",
    )
    assert ret.find("Computed Min/Max=74.000,255.000") != -1
