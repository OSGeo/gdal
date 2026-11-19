#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalmanage testing
# Author:   Alessandro Pasotti <elpaso at itopen.it>
#
###############################################################################
# Copyright (c) 2024, Alessandro Pasotti <elpaso at itopen.it>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest
import test_cli_utilities

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdalmanage_path() is None,
    reason="gdalmanage not available",
)


@pytest.fixture()
def gdalmanage_path():
    return test_cli_utilities.get_gdalmanage_path()


###############################################################################
# Simple identify test


def test_gdalmanage_identify(gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path + " identify data/utmsmall.tif"
    )
    assert err == ""
    assert "GTiff" in ret


###############################################################################
# Test -r option


def test_gdalmanage_identify_recursive_option(gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(gdalmanage_path + " identify -r data")
    assert err == ""
    assert "ESRI Shapefile" in ret
    assert len(ret.split("\n")) == 2


###############################################################################
# Test -fr option


def test_gdalmanage_identify_force_recursive_option(gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(gdalmanage_path + " identify -fr data")
    assert err == ""
    ret = ret.replace("\\", "/")
    assert len(ret.split("\n")) > 10
    assert "whiteblackred.tif: GTiff" in ret
    assert "utmsmall.tif: GTiff" in ret
    assert "ESRI Shapefile" in ret
    assert "data/path.cpg: unrecognized" not in ret

    # Test both the -r and -fr options (shouldn't change the output)
    ret2, err2 = gdaltest.runexternal_out_and_err(
        gdalmanage_path + " identify -r -fr data"
    )
    ret2 = ret2.replace("\\", "/")
    assert ret2 == ret and err2 == err


###############################################################################
# Test -u option


def test_gdalmanage_identify_report_failures_option(gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path + " identify -fr -u data"
    )
    assert err == ""
    ret = ret.replace("\\", "/")
    assert "whiteblackred.tif: GTiff" in ret
    assert "utmsmall.tif: GTiff" in ret
    assert "ESRI Shapefile" in ret
    assert "data/path.cpg: unrecognized" in ret


###############################################################################
# Test identify multiple files


def test_gdalmanage_identify_multiple_files(gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path + " identify data/utmsmall.tif data/whiteblackred.tif"
    )
    assert err == ""
    assert len(ret.split("\n")) == 3
    assert "whiteblackred.tif: GTiff" in ret
    assert "utmsmall.tif: GTiff" in ret


###############################################################################
# Test copy file


def test_gdalmanage_copy_file(tmp_path, gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path + f" copy data/utmsmall.tif {tmp_path}/utmsmall.tif"
    )
    assert err == ""
    # Verify the file was created
    assert os.path.exists(f"{tmp_path}/utmsmall.tif")


###############################################################################
# Test copy file with -f option


def test_gdalmanage_copy_file_format(tmp_path, gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path + f" copy -f GTiff data/utmsmall.tif {tmp_path}/utmsmall2.tif"
    )
    assert err == ""
    # Verify the file was created
    assert os.path.exists(f"{tmp_path}/utmsmall2.tif")

    # Wrong format
    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path
        + f" copy -f WRONGFORMAT data/utmsmall.tif {tmp_path}/utmsmall3.tif"
    )
    assert "Failed to find driver 'WRONGFORMAT'" in err


###############################################################################
# Test rename file


def test_gdalmanage_rename_file(tmp_path, gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path + f" copy data/utmsmall.tif {tmp_path}/utmsmall_to_rename.tif"
    )
    assert err == ""
    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path
        + f" rename {tmp_path}/utmsmall_to_rename.tif {tmp_path}/utmsmall_renamed.tif"
    )
    assert err == ""
    # Verify the file was renamed
    assert os.path.exists(f"{tmp_path}/utmsmall_renamed.tif")
    assert not os.path.exists(f"{tmp_path}/utmsmall_to_rename.tif")


###############################################################################
# Test delete file


def test_gdalmanage_delete_file(tmp_path, gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path + f" copy data/utmsmall.tif {tmp_path}/utmsmall_to_delete.tif"
    )
    assert err == ""
    assert os.path.exists(f"{tmp_path}/utmsmall_to_delete.tif")
    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path + f" delete {tmp_path}/utmsmall_to_delete.tif"
    )
    assert err == ""
    # Verify the file was deleted
    assert not os.path.exists(f"{tmp_path}/utmsmall_to_delete.tif")


###############################################################################
# Test delete multiple files


def test_gdalmanage_delete_multiple_files(tmp_path, gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path + f" copy data/utmsmall.tif {tmp_path}/utmsmall_to_delete.tif"
    )
    assert err == ""
    assert os.path.exists(f"{tmp_path}/utmsmall_to_delete.tif")
    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path
        + f" copy data/whiteblackred.tif {tmp_path}/whiteblackred_to_delete.tif"
    )
    assert err == ""
    assert os.path.exists(f"{tmp_path}/whiteblackred_to_delete.tif")
    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path
        + f" delete {tmp_path}/utmsmall_to_delete.tif {tmp_path}/whiteblackred_to_delete.tif"
    )
    assert err == ""
    # Verify the files were deleted
    assert not os.path.exists(f"{tmp_path}/utmsmall_to_delete.tif")
    assert not os.path.exists(f"{tmp_path}/whiteblackred_to_delete.tif")


###############################################################################
# Test no arguments


def test_gdalmanage_no_arguments(gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(gdalmanage_path)
    assert "Usage: gdalmanage" in err


###############################################################################
# Test invalid command


def test_gdalmanage_invalid_command(gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(gdalmanage_path + " invalidcommand")
    assert "Usage: gdalmanage" in err


###############################################################################
# Test invalid argument


def test_gdalmanage_invalid_argument(gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(
        gdalmanage_path + " identify -WTF data/utmsmall.tif"
    )
    assert "Usage: gdalmanage" in err
    assert "Unknown argument: -WTF" in err


###############################################################################
# Test valid command with no required argument


def test_gdalmanage_valid_command_no_argument(gdalmanage_path):

    ret, err = gdaltest.runexternal_out_and_err(gdalmanage_path + " identify")
    assert "Usage: gdalmanage" in err
    assert (
        "Error: No dataset name provided. At least one dataset name is required" in err
    )
